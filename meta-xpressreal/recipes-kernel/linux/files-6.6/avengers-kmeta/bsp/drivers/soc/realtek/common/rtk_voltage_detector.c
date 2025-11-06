// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Realtek DHC Voltage Detector driver
 *
 * Copyright (c) 2019 Realtek Semiconductor Corp.
 */

#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

struct rtd_volt_detect_desc_info {
	const char *name;
	unsigned int reg_offset;
	unsigned int en_offset;
	unsigned int status_offset;
	char **pins;
};

struct rtd_volt_detect_descs {
	const struct rtd_volt_detect_desc_info *info;
	int num_descs;
};

struct rtd_volt_detect_data {
	const struct rtd_volt_detect_descs *descs;
	struct pinctrl *pinctrl;
	struct regmap *base;
	struct device *dev;
};


#define RTD_VOL_DETECT_DESC(_name, _reg_off, _en_off, _st_off) \
	{ \
		.name = # _name, \
		.reg_offset = _reg_off, \
		.en_offset = _en_off, \
		.status_offset = _st_off, \
	}

static const struct rtd_volt_detect_desc_info rtd1625_volt_detect_desc[] = {
	RTD_VOL_DETECT_DESC(rgmii, 0x1a0, 8, 1),
	RTD_VOL_DETECT_DESC(sd, 0x1a0, 9, 2),
	RTD_VOL_DETECT_DESC(csi, 0x1a0, 10, 3),
	RTD_VOL_DETECT_DESC(sdio, 0x1a0, 11, 4),
	RTD_VOL_DETECT_DESC(uart1, 0x1a0, 12, 5),
	RTD_VOL_DETECT_DESC(aio, 0x1a0, 13, 6),
	RTD_VOL_DETECT_DESC(emmc, 0x1a0, 14, 7),
};

static const struct rtd_volt_detect_descs rtd1625_volt_detect_descs = {
	.info = rtd1625_volt_detect_desc,
	.num_descs = ARRAY_SIZE(rtd1625_volt_detect_desc),
};

static void detect_volt_set(const struct rtd_volt_detect_desc_info *desc, struct rtd_volt_detect_data *data)
{
	struct pinctrl_state *state_1V8;
	struct pinctrl_state *state_3V3;
	struct pinctrl_state *select_state;
	char pinctr_name[20];
	int val;
	int ret;

	snprintf(pinctr_name, sizeof(pinctr_name), "%s_1v8", desc->name);

	state_1V8 = pinctrl_lookup_state(data->pinctrl, pinctr_name);
	if (IS_ERR(state_1V8)) {
		dev_err(data->dev, "Failed to lookup 1.8V state for %s\n", pinctr_name);
		return;
	}

	snprintf(pinctr_name, sizeof(pinctr_name), "%s_3v3", desc->name);

	state_3V3 = pinctrl_lookup_state(data->pinctrl, pinctr_name);
	if (IS_ERR(state_3V3)) {
		dev_err(data->dev, "Failed to lookup 3.3V state for %s\n", pinctr_name);
		return;
	}

	regmap_read(data->base, desc->reg_offset, &val);
	regmap_write(data->base, desc->reg_offset, val | BIT(desc->en_offset));
	regmap_read(data->base, desc->reg_offset, &val);

	if (val & BIT(desc->status_offset))
		select_state = state_3V3;
	else
		select_state = state_1V8;

	ret = pinctrl_select_state(data->pinctrl, select_state);
	if (ret)
		dev_err(data->dev, "Failed to select pinctrl state\n");
}

static int rtd_volt_detector_probe(struct platform_device *pdev)
{
	struct rtd_volt_detect_data *data;
	struct device *dev = &pdev->dev;
	int i;

	data = devm_kzalloc(dev, sizeof(struct rtd_volt_detect_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->base = syscon_regmap_lookup_by_phandle(dev->of_node, "syscon");
	if (IS_ERR_OR_NULL(data->base))
		return -EINVAL;

	data->descs = of_device_get_match_data(dev);
	if (!data->descs)
		return -EINVAL;

	data->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(data->pinctrl)) {
		dev_err(data->dev, "Failed to get pinctrl\n");
		return -EINVAL;
	}

	data->dev = dev;

	for (i = 0; i < data->descs->num_descs; i++) {
		detect_volt_set(&data->descs->info[i], data);
	}

	devm_pinctrl_put(data->pinctrl);

	return 0;
}



static const struct of_device_id rtd_volt_detector_of_matches[] = {
	{ .compatible = "realtek,rtd1625-volt-detector", .data = &rtd1625_volt_detect_descs },
	{ }
};
MODULE_DEVICE_TABLE(of, rtd_volt_detector_of_matches);

static struct platform_driver rtd_volt_detector_driver = {
	.driver = {
		.name = "rtd_volt_detector",
		.of_match_table = rtd_volt_detector_of_matches,
	},
	.probe = rtd_volt_detector_probe,
};
module_platform_driver(rtd_volt_detector_driver);

MODULE_DESCRIPTION("Realtek DHC SoC Voltage Detector driver");
MODULE_LICENSE("GPL v2");
