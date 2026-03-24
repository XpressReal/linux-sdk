// SPDX-License-Identifier: GPL-2.0+
/*
 * Realtek RTD GPIO driver for U-Boot
 *
 * Copyright (C) 2024
 *
 * Simplified from Linux kernel driver
 * Original: Copyright (c) 2023 Realtek Semiconductor Corp.
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/bitops.h>

#define GPIO_CONTROL(gpio) ((gpio) << 2)

/**
 * struct rtd_gpio_info - GPIO register configuration
 * @name: GPIO device name
 * @num_gpios: The number of GPIOs
 */
struct rtd_gpio_info {
	const char	*name;
	unsigned int	num_gpios;
};

struct rtd_gpio_priv {
	void __iomem			*base;
	const struct rtd_gpio_info	*info;
};

/* RTD1625 ISO GPIO configuration - 166 GPIOs */
static const struct rtd_gpio_info rtd1625_iso_gpio_info = {
	.name			= "rtd1625_iso_gpio",
	.num_gpios		= 166,
};

static int rtd_gpio_direction_input(struct udevice *dev, unsigned int offset)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	writel(BIT(1), priv->base + GPIO_CONTROL(offset));

	debug("GPIO%d: set direction input\n", offset);

	return 0;
}

static int rtd_gpio_direction_output(struct udevice *dev, unsigned int offset,
				     int value)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	/* Set output value first */
	if (value)
		val = BIT(2) | BIT(3);
	else
		val = BIT(3);
	writel(val, priv->base + GPIO_CONTROL(offset));

	/* Then set direction to output */
	writel(BIT(0) | BIT(1), priv->base + GPIO_CONTROL(offset));

	debug("GPIO%d: set direction output, value %d\n", offset, value);

	return 0;
}

static int rtd_gpio_get_value(struct udevice *dev, unsigned int offset)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	val = readl(priv->base + GPIO_CONTROL(offset));

	/* check if direction is output */
	if (val & BIT(0))
		return !!(val & BIT(2));
	else
		return !!(val & BIT(4));
}

static int rtd_gpio_set_value(struct udevice *dev, unsigned int offset,
			      int value)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	if (value)
		val = BIT(2) | BIT(3);
	else
		val = BIT(3);

	writel(val, priv->base + GPIO_CONTROL(offset));

	debug("GPIO%d: set value %d\n", offset, value);

	return 0;
}

static int rtd_gpio_get_function(struct udevice *dev, unsigned int offset)
{
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	u32 val;

	if (offset >= priv->info->num_gpios)
		return -EINVAL;

	val = readl(priv->base + GPIO_CONTROL(offset));

	if (val & BIT(0))
		return GPIOF_OUTPUT;
	else
		return GPIOF_INPUT;
}

static int rtd_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct rtd_gpio_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;

	priv->info = (const struct rtd_gpio_info *)dev_get_driver_data(dev);
	if (!priv->info) {
		dev_err(dev, "Failed to get GPIO info\n");
		return -EINVAL;
	}

	/* Get base address from device tree */
	addr = dev_read_addr_index(dev, 0);
	if (addr == FDT_ADDR_T_NONE) {
		dev_err(dev, "Failed to get GPIO base address\n");
		return -EINVAL;
	}
	priv->base = (void __iomem *)addr;

	/* Setup GPIO chip */
	uc_priv->bank_name = priv->info->name;
	uc_priv->gpio_count = priv->info->num_gpios;

	dev_info(dev, "Realtek GPIO: %s, %d GPIOs at 0x%p\n",
		 priv->info->name, priv->info->num_gpios, priv->base);

	return 0;
}

static const struct dm_gpio_ops rtd_gpio_ops = {
	.direction_input	= rtd_gpio_direction_input,
	.direction_output	= rtd_gpio_direction_output,
	.get_value		= rtd_gpio_get_value,
	.set_value		= rtd_gpio_set_value,
	.get_function		= rtd_gpio_get_function,
};

static const struct udevice_id rtd_gpio_ids[] = {
	{ .compatible = "realtek,rtd1625-iso-gpio", .data = (ulong)&rtd1625_iso_gpio_info },
	{ }
};

U_BOOT_DRIVER(gpio_realtek_rtd1625) = {
	.name	= "gpio_realtek_rtd1625",
	.id	= UCLASS_GPIO,
	.of_match = rtd_gpio_ids,
	.probe	= rtd_gpio_probe,
	.ops	= &rtd_gpio_ops,
	.priv_auto = sizeof(struct rtd_gpio_priv),
};
