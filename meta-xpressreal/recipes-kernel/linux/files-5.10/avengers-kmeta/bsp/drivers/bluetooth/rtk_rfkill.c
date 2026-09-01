/*
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sys_soc.h>
#include <linux/rfkill.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#define LSADC0_PAD0	0x900
#define ADC_VAL0_MASK	GENMASK(7, 0)
#define ADC_VAL0_INIT	0x0
#define ADC_VAL0	0x94
#define VAL_CHK_LMT	6

enum voltage_config {
	VOLT_DETECT = 0,
	VOLT_FIX_1V8,
	VOLT_FIX_3V3,
};

struct rfkill *bt_rfk;
struct gpio_desc *bt_reset;
struct pinctrl *pinctrl;
struct pinctrl_state *pins_default;
struct pinctrl_state *pins_1v8;
struct pinctrl_state *pins_3v3;

static const struct soc_device_attribute soc_voltage_detect[] = {
	{ .family = "Realtek Stark", },
	{ /* sentinel */ }
};

static int bluetooth_set_power(void *data, bool blocked)
{
	int err = 0;

	pr_info("%s: %s\n", __func__, blocked ? "off" : "on");

	if (!blocked)
		err = gpiod_direction_output(bt_reset, 1);
	else
		err = gpiod_direction_output(bt_reset, 0);

	if (err)
		pr_err("%s set bt power fail\n", __func__);

	return 0;
}

static struct rfkill_ops rfkill_bluetooth_ops = {
	.set_block = bluetooth_set_power,
};

static int rfkill_gpio_init(struct device *dev)
{
	bt_reset = devm_gpiod_get(dev, "rfkill", GPIOD_OUT_LOW);
	if (IS_ERR(bt_reset)) {
		pr_err("%s could not request gpio\n", __func__);
		return PTR_ERR(bt_reset);
	}

	return 0;
}

static void rfkill_gpio_deinit(void)
{
	gpiod_put(bt_reset);
}

static void rfkill_uart_voltage_detect(struct regmap *iso_base, int mode)
{
	u32 reg, mask_val = ADC_VAL0_INIT;
	int val_chk = 0;

	if (mode == VOLT_DETECT) {
		do {
			val_chk += 1;

			regmap_read(iso_base, LSADC0_PAD0, &reg);
			mask_val = reg & ADC_VAL0_MASK;

			if (mask_val != ADC_VAL0_INIT)
				break;

			mdelay(50);

		} while((!mask_val) && (val_chk < VAL_CHK_LMT));

		if (mask_val >= ADC_VAL0)
			mode = VOLT_FIX_3V3;
		else
			mode = VOLT_FIX_1V8;
	}

	switch(mode) {
	case VOLT_FIX_3V3:
		pinctrl_select_state(pinctrl, pins_3v3);
		break;
	case VOLT_FIX_1V8:
		pinctrl_select_state(pinctrl, pins_1v8);
		break;
	}
}

static int rfkill_bluetooth_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct regmap *iso_base;
	const u32 *value;
	int mode = 0, ret = 0;

	pr_info("-->%s\n", __func__);

	ret = rfkill_gpio_init(dev);
	if (ret)
		goto deferred;

	if (soc_device_match(soc_voltage_detect)) {
		iso_base = syscon_regmap_lookup_by_phandle(node, "realtek,iso");
		if (IS_ERR(iso_base)) {
			dev_warn(dev, "could not get iso register base address\n");
			return PTR_ERR(iso_base);
		}

		pinctrl = devm_pinctrl_get(dev);
		if (IS_ERR(pinctrl))
			dev_warn(dev, "no pinctrl\n");

		pins_default = pinctrl_lookup_state(pinctrl, PINCTRL_STATE_DEFAULT);
		if (IS_ERR(pins_default)) {
			dev_warn(dev, "could not get default state\n");
			pins_default = NULL;
		}

		pins_3v3 = pinctrl_lookup_state(pinctrl, "voltage-3v3");
		if (IS_ERR(pins_3v3)) {
			dev_warn(dev, "could not get state voltage-3v3\n");
			pins_3v3 = NULL;
		}

		pins_1v8 = pinctrl_lookup_state(pinctrl, "voltage-1v8");
		if (IS_ERR(pins_1v8)) {
			dev_warn(dev, "could not get state voltage-1v8\n");
			pins_1v8 = NULL;
		}

		value = of_get_property(dev->of_node, "voltage-config", NULL);
		if (value)
			mode = of_read_number(value, 1);

		rfkill_uart_voltage_detect(iso_base, mode);
	}

	bt_rfk = rfkill_alloc("rtk-bt", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			      &rfkill_bluetooth_ops, NULL);
	if (!bt_rfk) {
		ret = -ENOMEM;
		goto err_rfkill_alloc;
	}

	/* userspace cannot take exclusive control */
	rfkill_init_sw_state(bt_rfk, false);

	ret = rfkill_register(bt_rfk);
	if (ret)
		goto err_rfkill_reg;

	rfkill_set_sw_state(bt_rfk, true);

	pr_info("<--%s\n", __func__);

	return 0;

err_rfkill_reg:
	rfkill_destroy(bt_rfk);
err_rfkill_alloc:
	return ret;
deferred:
	return -EPROBE_DEFER;
}

static int rfkill_bluetooth_remove(struct platform_device *dev)
{
	pr_info("-->%s\n", __func__);

	rfkill_gpio_deinit();
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);

	pr_info("<--%s\n", __func__);

	return 0;
}

static const struct of_device_id rtk_bt_ids[] = {
	{ .compatible = "realtek,rfkill" },
	{ /* Sentinel */ },
};

static struct platform_driver rfkill_bluetooth_driver = {
	.probe  = rfkill_bluetooth_probe,
	.remove = rfkill_bluetooth_remove,
	.driver = {
		.name = "rfkill",
		.owner = THIS_MODULE,
		.of_match_table = rtk_bt_ids,
	},
};

static int __init rfkill_bluetooth_init(void)
{
	pr_info("-->%s\n", __func__);
	return platform_driver_register(&rfkill_bluetooth_driver);
}

static void __exit rfkill_bluetooth_exit(void)
{
	pr_info("-->%s\n", __func__);
	platform_driver_unregister(&rfkill_bluetooth_driver);
}

late_initcall(rfkill_bluetooth_init);
module_exit(rfkill_bluetooth_exit);

MODULE_DESCRIPTION("bluetooth rfkill");
MODULE_AUTHOR("rs <wn@realsil.com.cn>");
MODULE_LICENSE("GPL");
