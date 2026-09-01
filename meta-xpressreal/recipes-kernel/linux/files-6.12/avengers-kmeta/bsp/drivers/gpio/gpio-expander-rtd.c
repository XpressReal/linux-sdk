// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek DHC gpio driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */

#include <linux/bitmap.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/of_address.h>
#include <linux/i2c.h>

#define EGPIO_INTEN 0x158
#define EGPIO_INTST 0x15c
#define EGPIO_GRP_CON 0x100
#define EGPIO_DEB_CTRL 0x1a0
#define EGPIO_ST_CHANGE 0x150
#define EGPIO_ST_CHANGE1 0x154
#define EGPIO_INT_SRC_LIST 0x160
#define EGPIO_IP_INT_EN 0x180
#define EGPIO_GP_CTRL 0x0

#define RTD_GPIO_DEBOUNCE_1US 0
#define RTD_GPIO_DEBOUNCE_10US 1
#define RTD_GPIO_DEBOUNCE_100US 2
#define RTD_GPIO_DEBOUNCE_1MS 3
#define RTD_GPIO_DEBOUNCE_10MS 4
#define RTD_GPIO_DEBOUNCE_20MS 5
#define RTD_GPIO_DEBOUNCE_30MS 6
#define RTD_GPIO_DEBOUNCE_50MS 7

#define GPIO_CONTROL(gpio) ((gpio) << 2)

struct gpio_ctrl {
	unsigned int dati;
	unsigned int dati_tmp;
	unsigned int irq_type;
	unsigned int irq_en;
};


/**
 * struct rtd_gpio_expand_info - Specific GPIO register information
 * @name: GPIO device name
 * @gpio_base: GPIO base number
 * @num_gpios: The number of GPIOs
 */
struct rtd_gpio_expand_info {
	const char	*name;
	unsigned int	gpio_base;
	unsigned int	num_gpios;
	unsigned int	irq_type_support;
	unsigned int	gpa_offset;
	unsigned int	gpda_offset;
	unsigned int	level_offset;
	struct irq_chip *irqchip;
};

struct rtd_gpio_expand {
	struct device 			*dev;
	struct gpio_chip		gpio_chip;
	const struct rtd_gpio_expand_info	*info;
	void __iomem			*base;
	void __iomem			*wrapper_base;
	unsigned int			irq;
	raw_spinlock_t			lock;
	unsigned int			*save_regs;
	struct gpio_ctrl *gpios;
	struct i2c_adapter *adapter;
	DECLARE_BITMAP(gpio_enable, 64);
};

static int rtd_gpio_expand_direction_input(struct gpio_chip *chip, unsigned int offset)
{
	struct rtd_gpio_expand *data = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;
	int ret = 0;

	if (!test_bit(offset, data->gpio_enable))
		return -ENODEV;

	raw_spin_lock_irqsave(&data->lock, flags);
	writel_relaxed(BIT(3), data->base + EGPIO_GP_CTRL + GPIO_CONTROL(offset));
	ret = readl_poll_timeout_atomic(data->base + EGPIO_GP_CTRL + GPIO_CONTROL(offset),
				  val, !(val & BIT(3)), 10, 10000);
	if (ret)
		dev_err(data->dev, "failed to set egpio%d intput\n", offset);
	raw_spin_unlock_irqrestore(&data->lock, flags);

	return ret;
}

static int rtd_gpio_expand_direction_output(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct rtd_gpio_expand *data = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;
	int ret = 0;

	if (!test_bit(offset, data->gpio_enable))
		return -ENODEV;

	if (value) {
		val = BIT(0) | BIT(1);
	} else {
		val = BIT(0);
	}

	raw_spin_lock_irqsave(&data->lock, flags);
	writel_relaxed(val | BIT(3), data->base + EGPIO_GP_CTRL + GPIO_CONTROL(offset));
	ret = readl_poll_timeout_atomic(data->base + EGPIO_GP_CTRL + GPIO_CONTROL(offset),
				  val, !(val & BIT(3)), 10, 10000);
	if (ret)
		dev_err(data->dev, "failed to set egpio%d output %s\n", offset, value ? "high" : "low");
	raw_spin_unlock_irqrestore(&data->lock, flags);

	return ret;
}

static void rtd_gpio_expand_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	rtd_gpio_expand_direction_output(chip, offset, value);
}

static int rtd_gpio_expand_get(struct gpio_chip *chip, unsigned int offset)
{
	struct rtd_gpio_expand *data = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	if (!test_bit(offset, data->gpio_enable))
		return -ENODEV;

	raw_spin_lock_irqsave(&data->lock, flags);
	val = readl_relaxed(data->base + EGPIO_GP_CTRL + GPIO_CONTROL(offset));
	raw_spin_unlock_irqrestore(&data->lock, flags);

	if (val & BIT(0))
		return !!(val & BIT(1));
	else
		return !!(val & BIT(2));
}

static int rtd_gpio_expand_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	struct rtd_gpio_expand *data = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	if (!test_bit(offset, data->gpio_enable))
		return -ENODEV;

	raw_spin_lock_irqsave(&data->lock, flags);
	val = readl_relaxed(data->base + EGPIO_GP_CTRL + GPIO_CONTROL(offset));
	raw_spin_unlock_irqrestore(&data->lock, flags);

	if (val & BIT(0))
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}
#if 1

static inline bool gpio_irq_should_handle(const struct gpio_ctrl *gpio)
{
	if (!gpio->irq_en || (gpio->dati == gpio->dati_tmp))
		return false;

	switch (gpio->irq_type) {
	case IRQ_TYPE_EDGE_RISING:
		return (gpio->dati == 0);
	case IRQ_TYPE_EDGE_FALLING:
		return (gpio->dati == 1);
	case IRQ_TYPE_EDGE_BOTH:
		return true;
	default:
		return false;
	}
}

static irqreturn_t rtd_gpio_expand_irq_handle(int irq, void *devid)
{
	struct rtd_gpio_expand *data   = devid;
	struct irq_domain     *domain  = data->gpio_chip.irq.domain;
	unsigned long          status1, status2;
	unsigned long          int_st;
	int                    idx, pin;
	int i;

	status1 = readl_relaxed(data->base + EGPIO_ST_CHANGE);
	status2 = readl_relaxed(data->base + EGPIO_ST_CHANGE1);

	writel_relaxed(status1, data->base + EGPIO_ST_CHANGE);
	writel_relaxed(status2, data->base + EGPIO_ST_CHANGE1);

	for (i = 0; i < data->info->num_gpios; i++) {
		data->gpios[i].dati_tmp = !!(readl_relaxed(data->base + EGPIO_GP_CTRL + GPIO_CONTROL(i)) &
					      BIT(2));
	}

	int_st = readl_relaxed(data->base + EGPIO_INTST);
	writel_relaxed(int_st, data->base + EGPIO_INTST);

	writel_relaxed(BIT(1), data->base + EGPIO_INT_SRC_LIST);

	for_each_set_bit(idx, &status1, 32) {
		pin = idx;
		if (test_bit(pin, data->gpio_enable) && gpio_irq_should_handle(&data->gpios[pin]))
			generic_handle_domain_irq(domain, pin);
	}

	for_each_set_bit(idx, &status2, 32) {
		pin = 32 + idx;
		if (test_bit(pin, data->gpio_enable) && gpio_irq_should_handle(&data->gpios[pin]))
			generic_handle_domain_irq(domain, pin);
	}
	for (i = 0; i < data->info->num_gpios; i++)
		data->gpios[i].dati = data->gpios[i].dati_tmp;

	return IRQ_HANDLED;
}

static void rtd_gpio_expand_enable_irq(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rtd_gpio_expand *data = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);

	data->gpios[hwirq].irq_en = 1;
}

static void rtd_gpio_expand_disable_irq(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rtd_gpio_expand *data = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);

	data->gpios[hwirq].irq_en = 0;
}

static int rtd_gpio_expand_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rtd_gpio_expand *data = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	int ret = 0;

	if (!test_bit(hwirq, data->gpio_enable))
		return 0;

	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_EDGE_RISING:
		data->gpios[hwirq].irq_type = IRQ_TYPE_EDGE_RISING;
		break;

	case IRQ_TYPE_EDGE_FALLING:
		data->gpios[hwirq].irq_type = IRQ_TYPE_EDGE_FALLING;
		break;

	case IRQ_TYPE_EDGE_BOTH:
		data->gpios[hwirq].irq_type = IRQ_TYPE_EDGE_BOTH;
		break;

	default:
		ret = -EINVAL;
	}

	irq_set_handler_locked(d, handle_simple_irq);

	return ret;
}

static struct irq_chip rtd_gpio_expand_irq_chip = {
	.name = "rtd-gpio-expand",
	.irq_enable = rtd_gpio_expand_enable_irq,
	.irq_disable = rtd_gpio_expand_disable_irq,
	.irq_set_type = rtd_gpio_expand_irq_set_type,
	.flags = IRQCHIP_IMMUTABLE | IRQCHIP_SKIP_SET_WAKE,
};
#endif

static int set_expander_output(struct rtd_gpio_expand *data, u16 addr, u8 *buf)
{
	struct i2c_msg msgs[1] = {
		{
			.addr	= addr,
			.len	= 2,
			.buf	= buf,
		},
	};

	int ret;

	ret = i2c_transfer(data->adapter, msgs, ARRAY_SIZE(msgs));

	return ret;
}

static int gpio_expand_init(struct device_node *node, struct rtd_gpio_expand *data)
{
	int ret;
	u32 group;
	u32 addr;
	u32 bits;
	u32 dir;
	int val;

	ret = of_property_read_u32(node, "group", &group);
	if (ret) {
		dev_err(data->dev, "cannot read group of %s, ret=%d\n", node->name, ret);
		goto out;
	}

	ret = of_property_read_u32(node, "addr", &addr);
	if (ret) {
		dev_err(data->dev, "cannot read addr of %s, ret=%d\n", node->name, ret);
		goto out;
	}

	ret = of_property_read_u32(node, "bits", &bits);
	if (ret) {
		dev_err(data->dev, "cannot read bits of %s, ret=%d\n", node->name, ret);
		goto out;
	}

	ret = of_property_read_u32(node, "dir", &dir);
	if (ret) {
		dev_err(data->dev, "cannot read dir of %s, ret=%d\n", node->name, ret);
		goto out;
	}

	if (bits) {
		if (dir) {
			writel_relaxed(BIT(20) | addr, data->base + EGPIO_GRP_CON + group * 0x4);
			writel_relaxed(BIT(20) | BIT(19) | addr, data->base + EGPIO_GRP_CON + (group + 1) * 0x4);
		} else {
			writel_relaxed(BIT(20) | BIT(12) | addr, data->base + EGPIO_GRP_CON + group * 0x4);
			writel_relaxed(BIT(20) | BIT(19) | BIT(12) | addr, data->base + EGPIO_GRP_CON + (group + 1) * 0x4);
		}
		bitmap_set(data->gpio_enable, group * 8, 16);
	} else {
		if (dir)
			writel_relaxed(BIT(20) | addr, data->base + EGPIO_GRP_CON + group * 0x4);
		else
			writel_relaxed(BIT(20) | BIT(12) | addr, data->base + EGPIO_GRP_CON + group * 0x4);
		bitmap_set(data->gpio_enable, group * 8, 8);
	}

	if (dir) {
		u8 expander_bank0[2] = { 0x6, 0x0 };
		u8 expander_bank1[2] = { 0x7, 0x0 };
		ret = set_expander_output(data, addr, expander_bank0);
		if (ret < 0) {
			dev_err(data->dev, "failed to configuration bank0 (%d)\n", ret);
			goto out;
		}
		ret = set_expander_output(data, addr, expander_bank1);
		if (ret < 0) {
			dev_err(data->dev, "failed to configuration bank1 (%d)\n", ret);
			goto out;
		}
	} else {
		val = readl_relaxed(data->base + EGPIO_INTEN);
		writel_relaxed(val | BIT(group), data->base + EGPIO_INTEN);
	}

out:
	return ret;
}

static int rtd_gpio_expand_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *phandle;
	struct gpio_irq_chip *irq_chip;
	struct device_node *child_node;
	struct rtd_gpio_expand *data;
        unsigned int val;
	int ret;
	int i;


	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	phandle = of_parse_phandle(pdev->dev.of_node, "adapter", 0);
	if (phandle) {
		data->adapter = of_get_i2c_adapter_by_node(phandle);
		of_node_put(phandle);
		if (!data->adapter) {
			return -EPROBE_DEFER;
		}
	} else {
		dev_err(&pdev->dev, "cannot get adapter phandle\n");
	}


	data->info = device_get_match_data(dev);
	if (!data->info)
		return -EINVAL;


	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0)
		return data->irq;

	raw_spin_lock_init(&data->lock);

	data->base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	data->wrapper_base = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(data->wrapper_base))
		return PTR_ERR(data->wrapper_base);

	data->dev = &pdev->dev;

	writel_relaxed(BIT(0), data->base + EGPIO_IP_INT_EN);
	for_each_available_child_of_node(data->dev->of_node, child_node) {
		ret = gpio_expand_init(child_node, data);
	}

	writel_relaxed(BIT(0) | BIT(1), data->wrapper_base);
	writel_relaxed(BIT(3) | 0x2, data->base + EGPIO_DEB_CTRL);

	data->gpios = devm_kmalloc_array(data->dev, data->info->num_gpios,
					 sizeof(struct gpio_ctrl), GFP_KERNEL);
	if (!data->gpios)
		return -ENOMEM;

	data->gpio_chip.label = dev_name(dev);
	data->gpio_chip.base = -1;
	data->gpio_chip.ngpio = data->info->num_gpios;
	data->gpio_chip.request = gpiochip_generic_request;
	data->gpio_chip.free = gpiochip_generic_free;
	data->gpio_chip.get_direction = rtd_gpio_expand_get_direction;
	data->gpio_chip.direction_input = rtd_gpio_expand_direction_input;
	data->gpio_chip.direction_output = rtd_gpio_expand_direction_output;
	data->gpio_chip.set = rtd_gpio_expand_set;
	data->gpio_chip.get = rtd_gpio_expand_get;
	data->gpio_chip.parent = dev;

	irq_chip = &data->gpio_chip.irq;
	irq_chip->chip = data->info->irqchip;
	irq_chip->handler = handle_bad_irq;
	irq_chip->default_type = IRQ_TYPE_NONE;
	irq_chip->parent_handler = NULL;
	irq_chip->num_parents = 0;
	irq_chip->parents = NULL;

	ret = devm_request_irq(data->dev, data->irq, rtd_gpio_expand_irq_handle, IRQF_SHARED,
					dev_name(data->dev), data);

	for (i = 0; i < 64; i = i + 8) {
		if (!test_bit(i, data->gpio_enable))
			continue;
		writel_relaxed(BIT(3), data->base + EGPIO_GP_CTRL + i * 0x4);
		ret = readl_poll_timeout(data->base + EGPIO_GP_CTRL + i * 0x4,
					  val, !(val & BIT(3)), 10, 10000);
		if (ret)
			dev_err(data->dev, "get group%d input shadow timeout\n", i >>  3);
	}

	writel_relaxed(0xffffffff, data->base + EGPIO_ST_CHANGE);
	writel_relaxed(0xffffffff, data->base + EGPIO_ST_CHANGE1);

	for (i = 0; i < data->info->num_gpios; i++) {
		data->gpios[i].dati = !!(readl_relaxed(data->base + EGPIO_GP_CTRL + GPIO_CONTROL(i)) &
					      BIT(2));
	}

	platform_set_drvdata(pdev, data);
	dev_info(data->dev, "probe\n");

	return devm_gpiochip_add_data(dev, &data->gpio_chip, data);
}


static const struct rtd_gpio_expand_info rtd_egpio_info = {
	.name			= "rtd_gpio_expand",
	.gpio_base		= 139,
	.num_gpios		= 64,
	.irq_type_support	= IRQ_TYPE_EDGE_BOTH,
	.gpa_offset		= 0x0,
	.gpda_offset		= 0x20,
	.irqchip		= &rtd_gpio_expand_irq_chip,
};

static const struct of_device_id rtd_gpio_expand_of_matches[] = {
	{ .compatible = "realtek,rtd-gpio-expander", .data = &rtd_egpio_info },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd_gpio_expand_of_matches);

static struct platform_driver rtd_gpio_expand_platform_driver = {
	.driver = {
		.name = "gpio-expand-rtd",
		.of_match_table = rtd_gpio_expand_of_matches,
	},
	.probe = rtd_gpio_expand_probe,
};
module_platform_driver(rtd_gpio_expand_platform_driver);

MODULE_DESCRIPTION("Realtek DHC SoC rtd gpio driver");
MODULE_LICENSE("GPL v2");
