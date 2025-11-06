// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek DHC gpio driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */

#include <linux/bitops.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define RTD_GPIO_DEBOUNCE_1US 0
#define RTD_GPIO_DEBOUNCE_10US 1
#define RTD_GPIO_DEBOUNCE_100US 2
#define RTD_GPIO_DEBOUNCE_1MS 3
#define RTD_GPIO_DEBOUNCE_10MS 4
#define RTD_GPIO_DEBOUNCE_20MS 5
#define RTD_GPIO_DEBOUNCE_30MS 6
#define RTD_GPIO_DEBOUNCE_50MS 7

#define GPIO_CONTROL(gpio) ((gpio) << 2)

/**
 * struct rtd1625_gpio_info - Specific GPIO register information
 * @name: GPIO device name
 * @gpio_base: GPIO base number
 * @num_gpios: The number of GPIOs
 */
struct rtd1625_gpio_info {
	const char	*name;
	unsigned int	gpio_base;
	unsigned int	num_gpios;
	unsigned int	irq_type_support;
	unsigned int	gpa_offset;
	unsigned int	gpda_offset;
	unsigned int	level_offset;
	struct irq_chip *irqchip;
	unsigned int 	write_en_all;
};

struct rtd1625_gpio {
	struct gpio_chip		gpio_chip;
	const struct rtd1625_gpio_info	*info;
	void __iomem			*base;
	void __iomem			*irq_base;
	unsigned int			irqs[3];
	raw_spinlock_t			lock;
	unsigned int			*save_regs;
};

static int rtd1625_gpio_gpa_offset(struct rtd1625_gpio *data, unsigned int offset)
{
	return data->info->gpa_offset + ((offset >> 5) << 2);
}

static int rtd1625_gpio_gpda_offset(struct rtd1625_gpio *data, unsigned int offset)
{
	return data->info->gpda_offset + ((offset >> 5) << 2);
}

static int rtd1625_gpio_level_offset(struct rtd1625_gpio *data, unsigned int offset)
{
	return data->info->level_offset;
}

static int rtd1625_gpio_set_debounce(struct gpio_chip *chip, unsigned int offset,
				   unsigned int debounce)
{
	struct rtd1625_gpio *data = gpiochip_get_data(chip);
	unsigned long flags;
	u8 deb_val;
	u32 val;

	switch (debounce) {
	case 1:
		deb_val = RTD_GPIO_DEBOUNCE_1US;
		break;
	case 10:
		deb_val = RTD_GPIO_DEBOUNCE_10US;
		break;
	case 100:
		deb_val = RTD_GPIO_DEBOUNCE_100US;
		break;
	case 1000:
		deb_val = RTD_GPIO_DEBOUNCE_1MS;
		break;
	case 10000:
		deb_val = RTD_GPIO_DEBOUNCE_10MS;
		break;
	case 20000:
		deb_val = RTD_GPIO_DEBOUNCE_20MS;
		break;
	case 30000:
		deb_val = RTD_GPIO_DEBOUNCE_30MS;
		break;
	case 50000:
		deb_val = RTD_GPIO_DEBOUNCE_50MS;
		break;
	default:
		return -ENOTSUPP;
	}

	val = deb_val << 28 | BIT(31);

	raw_spin_lock_irqsave(&data->lock, flags);
	writel_relaxed(val, data->base + GPIO_CONTROL(offset));
	raw_spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int rtd1625_gpio_set_config(struct gpio_chip *chip, unsigned int offset,
				 unsigned long config)
{
	int debounce;

	switch (pinconf_to_config_param(config)) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
		return gpiochip_generic_config(chip, offset, config);
	case PIN_CONFIG_INPUT_DEBOUNCE:
		debounce = pinconf_to_config_argument(config);
		return rtd1625_gpio_set_debounce(chip, offset, debounce);
	default:
		return -ENOTSUPP;
	}
}

static void rtd1625_gpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct rtd1625_gpio *data = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	if (value)
		val = BIT(2) | BIT(3);
	else
		val = BIT(3);

	raw_spin_lock_irqsave(&data->lock, flags);
	writel_relaxed(val, data->base + GPIO_CONTROL(offset));
	raw_spin_unlock_irqrestore(&data->lock, flags);
}

static int rtd1625_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct rtd1625_gpio *data = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&data->lock, flags);
	val = readl_relaxed(data->base + GPIO_CONTROL(offset));
	raw_spin_unlock_irqrestore(&data->lock, flags);

	if (val & BIT(0))
		return !!(val & BIT(2));
	else
		return !!(val & BIT(4));
}

static int rtd1625_gpio_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	struct rtd1625_gpio *data = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&data->lock, flags);
	val = readl_relaxed(data->base + GPIO_CONTROL(offset));
	raw_spin_unlock_irqrestore(&data->lock, flags);

	if (val & BIT(0))
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

static int rtd1625_gpio_set_direction(struct gpio_chip *chip, unsigned int offset, bool out)
{
	struct rtd1625_gpio *data = gpiochip_get_data(chip);
	unsigned long flags;
	u32 val;

	if (out)
		val = BIT(0) | BIT(1);
	else
		val = BIT(1);

	raw_spin_lock_irqsave(&data->lock, flags);
	writel_relaxed(val, data->base + GPIO_CONTROL(offset));
	raw_spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int rtd1625_gpio_direction_input(struct gpio_chip *chip, unsigned int offset)
{
	return rtd1625_gpio_set_direction(chip, offset, false);
}

static int rtd1625_gpio_direction_output(struct gpio_chip *chip, unsigned int offset, int value)
{
	rtd1625_gpio_set(chip, offset, value);

	return rtd1625_gpio_set_direction(chip, offset, true);
}

static void rtd1625_gpio_irq_handle(struct irq_desc *desc)
{
	int (*get_reg_offset)(struct rtd1625_gpio *gpio, unsigned int offset);
	struct rtd1625_gpio *data = irq_desc_get_handler_data(desc);
	struct irq_domain *domain = data->gpio_chip.irq.domain;
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned int irq = irq_desc_get_irq(desc);
	unsigned long status;
	int reg_offset, i, j;
	unsigned int hwirq;
	int girq;
	u32 irq_type;

	if (irq == data->irqs[0])
		get_reg_offset = &rtd1625_gpio_gpa_offset;
	else if (irq == data->irqs[1])
		get_reg_offset = &rtd1625_gpio_gpda_offset;
	else if (irq == data->irqs[2])
		get_reg_offset = &rtd1625_gpio_level_offset;

	chained_irq_enter(chip, desc);

	/* Each GPIO interrupt status register contains 32 GPIOs. */
	for (i = 0; i < data->info->num_gpios; i += 32) {
		reg_offset = get_reg_offset(data, i);
		status = readl_relaxed(data->irq_base + reg_offset);
		writel_relaxed(status, data->irq_base + reg_offset);

		for_each_set_bit(j, &status, 32) {
			hwirq = i + j;
			girq = irq_find_mapping(domain, hwirq);
			irq_type = irq_get_trigger_type(girq);

			if ((irq == data->irqs[1]) && (irq_type != IRQ_TYPE_EDGE_BOTH))
				continue;

			generic_handle_domain_irq(domain, hwirq);
		}
	}

	chained_irq_exit(chip, desc);
}

static void rtd1625_gpio_ack_irq(struct irq_data *d)
{
}

static void rtd1625_gpio_enable_edge_irq(struct rtd1625_gpio *data, irq_hw_number_t hwirq)
{
	u32 mask = BIT(hwirq & GENMASK(4, 0));
	unsigned long flags;
	int gpda_reg_offset;
	int gpa_reg_offset;
	u32 val;

	gpa_reg_offset = rtd1625_gpio_gpa_offset(data, hwirq);
	gpda_reg_offset = rtd1625_gpio_gpda_offset(data, hwirq);

	raw_spin_lock_irqsave(&data->lock, flags);

	writel_relaxed(mask, data->irq_base + gpa_reg_offset);
	writel_relaxed(mask, data->irq_base + gpda_reg_offset);
	val = BIT(8) | BIT(9);
	writel_relaxed(val, data->base + GPIO_CONTROL(hwirq));

	raw_spin_unlock_irqrestore(&data->lock, flags);
}

static void rtd1625_gpio_disable_edge_irq(struct rtd1625_gpio *data, irq_hw_number_t hwirq)
{
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&data->lock, flags);
	val = BIT(9);
	writel_relaxed(val, data->base + GPIO_CONTROL(hwirq));
	raw_spin_unlock_irqrestore(&data->lock, flags);
}

static void rtd1625_gpio_enable_level_irq(struct rtd1625_gpio *data, irq_hw_number_t hwirq)
{
	u32 mask = BIT(hwirq & GENMASK(4, 0));
	unsigned long flags;
	int level_reg_offset;
	u32 val;

	level_reg_offset = rtd1625_gpio_level_offset(data, hwirq);

	raw_spin_lock_irqsave(&data->lock, flags);

	writel_relaxed(mask, data->irq_base + level_reg_offset);

	val = BIT(16) | BIT(17);
	writel_relaxed(val, data->base + GPIO_CONTROL(hwirq));

	raw_spin_unlock_irqrestore(&data->lock, flags);
}

static void rtd1625_gpio_disable_level_irq(struct rtd1625_gpio *data, irq_hw_number_t hwirq)
{
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&data->lock, flags);
	val = BIT(17);
	writel_relaxed(val, data->base + GPIO_CONTROL(hwirq));
	raw_spin_unlock_irqrestore(&data->lock, flags);
}

static void rtd1625_gpio_enable_irq(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rtd1625_gpio *data = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	u32 irq_type = irqd_get_trigger_type(d);

	//gpiochip_enable_irq(gc, hwirq);

	if (irq_type & IRQ_TYPE_EDGE_BOTH)
		rtd1625_gpio_enable_edge_irq(data, hwirq);
	else if (irq_type & IRQ_TYPE_LEVEL_MASK)
		rtd1625_gpio_enable_level_irq(data, hwirq);
}

static void rtd1625_gpio_disable_irq(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rtd1625_gpio *data = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	u32 irq_type = irqd_get_trigger_type(d);

	if (irq_type & IRQ_TYPE_EDGE_BOTH)
		rtd1625_gpio_disable_edge_irq(data, hwirq);
	else if (irq_type & IRQ_TYPE_LEVEL_MASK)
		rtd1625_gpio_disable_level_irq(data, hwirq);

	//gpiochip_disable_irq(gc, hwirq);
}

static int rtd1625_gpio_irq_set_level_type(struct irq_data *d, bool level)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rtd1625_gpio *data = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	unsigned long flags;
	u32 val = 0;

	if (!(data->info->irq_type_support & IRQ_TYPE_LEVEL_MASK))
		return -EINVAL;

	raw_spin_lock_irqsave(&data->lock, flags);
	if (level)
		val |= BIT(18) | BIT(19);
	else
		val |= BIT(18);
	writel_relaxed(val, data->base + GPIO_CONTROL(hwirq));
	raw_spin_unlock_irqrestore(&data->lock, flags);

	irq_set_handler_locked(d, handle_level_irq);

	return 0;
}

static int rtd1625_gpio_irq_set_edge_type(struct irq_data *d, bool polarity)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct rtd1625_gpio *data = gpiochip_get_data(gc);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	unsigned long flags;
	u32 val = 0;

	if (!(data->info->irq_type_support & IRQ_TYPE_EDGE_BOTH))
		return -EINVAL;

	raw_spin_lock_irqsave(&data->lock, flags);
	if (polarity)
		val |= BIT(6) | BIT(7);
	else
		val |= BIT(7);
	writel_relaxed(val, data->base + GPIO_CONTROL(hwirq));
	raw_spin_unlock_irqrestore(&data->lock, flags);

	irq_set_handler_locked(d, handle_edge_irq);

	return 0;
}

static int rtd1625_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	int ret;

	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_EDGE_RISING:
		ret = rtd1625_gpio_irq_set_edge_type(d, 1);
		break;

	case IRQ_TYPE_EDGE_FALLING:
		ret = rtd1625_gpio_irq_set_edge_type(d, 0);
		break;

	case IRQ_TYPE_EDGE_BOTH:
		ret = rtd1625_gpio_irq_set_edge_type(d, 1);
		break;

	case IRQ_TYPE_LEVEL_HIGH:
		ret = rtd1625_gpio_irq_set_level_type(d, 0);
		break;

	case IRQ_TYPE_LEVEL_LOW:
		ret = rtd1625_gpio_irq_set_level_type(d, 1);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static struct irq_chip rtd1625_iso_gpio_irq_chip = {
	.name = "rtd1625-iso-gpio",
	.irq_ack = rtd1625_gpio_ack_irq,
	.irq_mask = rtd1625_gpio_disable_irq,
	.irq_unmask = rtd1625_gpio_enable_irq,
	.irq_enable = rtd1625_gpio_enable_irq,
	.irq_disable = rtd1625_gpio_disable_irq,
	.irq_set_type = rtd1625_gpio_irq_set_type,
	.flags = IRQCHIP_SKIP_SET_WAKE,
};

static struct irq_chip rtd1625_isom_gpio_irq_chip = {
	.name = "rtd1625-isom-gpio",
	.irq_ack = rtd1625_gpio_ack_irq,
	.irq_mask = rtd1625_gpio_disable_irq,
	.irq_unmask = rtd1625_gpio_enable_irq,
	.irq_enable = rtd1625_gpio_enable_irq,
	.irq_disable = rtd1625_gpio_disable_irq,
	.irq_set_type = rtd1625_gpio_irq_set_type,
	.flags = IRQCHIP_SKIP_SET_WAKE,
};

static int rtd1625_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_irq_chip *irq_chip;
	struct rtd1625_gpio *data;
	int num_irqs;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->info = device_get_match_data(dev);
	if (!data->info)
		return -EINVAL;

	data->irqs[0] = platform_get_irq(pdev, 0);
	if (data->irqs[0] < 0)
		return data->irqs[0];

	data->irqs[1] = platform_get_irq(pdev, 1);
	if (data->irqs[1] < 0)
		return data->irqs[1];

	num_irqs = 2;
	if (data->info->irq_type_support & IRQ_TYPE_LEVEL_MASK) {
		data->irqs[2] = platform_get_irq(pdev, 2);
		if (data->irqs[2] < 0)
			return data->irqs[2];
		num_irqs = 3;
	}

	raw_spin_lock_init(&data->lock);

	data->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	data->irq_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(data->irq_base))
		return PTR_ERR(data->irq_base);

	data->gpio_chip.label = dev_name(dev);
	data->gpio_chip.base = data->info->gpio_base;
	data->gpio_chip.ngpio = data->info->num_gpios;
	data->gpio_chip.request = gpiochip_generic_request;
	data->gpio_chip.free = gpiochip_generic_free;
	data->gpio_chip.get_direction = rtd1625_gpio_get_direction;
	data->gpio_chip.direction_input = rtd1625_gpio_direction_input;
	data->gpio_chip.direction_output = rtd1625_gpio_direction_output;
	data->gpio_chip.set = rtd1625_gpio_set;
	data->gpio_chip.get = rtd1625_gpio_get;
	data->gpio_chip.set_config = rtd1625_gpio_set_config;
	data->gpio_chip.parent = dev;

	irq_chip = &data->gpio_chip.irq;
	irq_chip->chip = data->info->irqchip;
	irq_chip->handler = handle_bad_irq;
	irq_chip->default_type = IRQ_TYPE_NONE;
	irq_chip->parent_handler = rtd1625_gpio_irq_handle;
	irq_chip->parent_handler_data = data;
	irq_chip->num_parents = num_irqs;
	irq_chip->parents = data->irqs;

	platform_set_drvdata(pdev, data);

	return devm_gpiochip_add_data(dev, &data->gpio_chip, data);
}


static const struct rtd1625_gpio_info rtd1625_iso_gpio_info = {
	.name			= "rtd1625_iso_gpio",
	.gpio_base		= 0,
	.num_gpios		= 166,
	.irq_type_support	= IRQ_TYPE_EDGE_BOTH,
	.gpa_offset		= 0x0,
	.gpda_offset		= 0x20,
	.irqchip		= &rtd1625_iso_gpio_irq_chip,
	.write_en_all		= 0x8000aa8a,
};

static const struct rtd1625_gpio_info rtd1625_isom_gpio_info = {
	.name			= "rtd1625_isom_gpio",
	.gpio_base		= 166,
	.num_gpios		= 4,
	.irq_type_support	= IRQ_TYPE_EDGE_BOTH | IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH,
	.gpa_offset		= 0x0,
	.gpda_offset		= 0x4,
	.level_offset		= 0x18,
	.irqchip		= &rtd1625_isom_gpio_irq_chip,
	.write_en_all		= 0x800aaa8a,
};

static const struct of_device_id rtd1625_gpio_of_matches[] = {
	{ .compatible = "realtek,rtd1625-iso-gpio", .data = &rtd1625_iso_gpio_info },
	{ .compatible = "realtek,rtd1625-isom-gpio", .data = &rtd1625_isom_gpio_info },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd1625_gpio_of_matches);

#ifdef CONFIG_PM
static int realtek_gpio_suspend(struct device *dev)
{
	struct rtd1625_gpio *data = dev_get_drvdata(dev);
	const struct rtd1625_gpio_info *info = data->info;
	int i;

	data->save_regs = kmalloc(data->info->num_gpios * sizeof(*data->save_regs), GFP_KERNEL);
	for (i = 0; i < info->num_gpios; i++)
		data->save_regs[i] = readl_relaxed(data->base + GPIO_CONTROL(i));

	return 0;
}

static int realtek_gpio_resume(struct device *dev)
{
	struct rtd1625_gpio *data = dev_get_drvdata(dev);
	const struct rtd1625_gpio_info *info = data->info;
	int i;

	for (i = 0; i < info->num_gpios; i++)
		writel_relaxed(data->save_regs[i] | info->write_en_all,
			       data->base + GPIO_CONTROL(i));

	kfree(data->save_regs);

	return 0;
}
#endif

static const struct dev_pm_ops realtek_gpio_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(realtek_gpio_suspend, realtek_gpio_resume)
};

static struct platform_driver rtd1625_gpio_platform_driver = {
	.driver = {
		.name = "gpio-rtd1625",
		.of_match_table = rtd1625_gpio_of_matches,
		.pm = &realtek_gpio_pm_ops,
	},
	.probe = rtd1625_gpio_probe,
};
module_platform_driver(rtd1625_gpio_platform_driver);

MODULE_DESCRIPTION("Realtek DHC SoC RTD1625 gpio driver");
MODULE_LICENSE("GPL v2");
