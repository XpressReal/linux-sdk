// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/mfd/rr320010.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/rr320010.h>
#include <dt-bindings/regulator/rr320010-regulator.h>

struct rr320010_regulator_device {
	struct regmap *regmap;
	struct device *dev;
};

struct rr320010_regulator_desc {
	struct regulator_desc desc;
	unsigned int mode_reg;
	unsigned int mode_mask;
};

struct rr320010_regulator_data {
	struct regulator_dev *rdev;
	struct rr320010_regulator_desc *desc;
	struct regmap_field *mode;
};

enum {
	RR320010_ID_BUCK1,
	RR320010_ID_BUCK2,
	RR320010_ID_BUCK3,
	RR320010_ID_BUCK4,
	RR320010_ID_BUCK5,
	RR320010_ID_BUCK6,
	RR320010_ID_BUCK7,
	RR320010_ID_BUCK8,
	RR320010_ID_HDMI,
};

static int rr320010_regulator_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct rr320010_regulator_data *data = rdev_get_drvdata(rdev);
	struct rr320010_regulator_desc *desc = data->desc;
	unsigned int val = 0;

	if (!data->mode)
		return -EINVAL;

	if (desc->desc.id == RR320010_ID_BUCK8)
		val = (mode & REGULATOR_MODE_FAST) ? 0x1 : 0x3;
	else
		val = (mode & REGULATOR_MODE_FAST) ? 0x1 : 0x0;

	return regmap_field_write(data->mode, val);
}

static unsigned int rr320010_regulator_get_mode(struct regulator_dev *rdev)
{
	struct rr320010_regulator_data *data = rdev_get_drvdata(rdev);
	unsigned int val = 0;

	if (data->mode) {
		regmap_field_read(data->mode, &val);
		if (val == 1 || val == 2)
			return REGULATOR_MODE_FAST;
	}
	return REGULATOR_MODE_NORMAL;
}

static const struct regulator_ops rr320010_regulator_ops = {
	.list_voltage         = regulator_list_voltage_linear_range,
	.map_voltage          = regulator_map_voltage_linear_range,
	.set_voltage_sel      = regulator_set_voltage_sel_regmap,
	.get_voltage_sel      = regulator_get_voltage_sel_regmap,
	.enable               = regulator_enable_regmap,
	.disable              = regulator_disable_regmap,
	.is_enabled           = regulator_is_enabled_regmap,
	.set_mode             = rr320010_regulator_set_mode,
	.get_mode             = rr320010_regulator_get_mode,
};

static const struct regulator_ops rr320010_fixed_regulator_ops = {
	.enable               = regulator_enable_regmap,
	.disable              = regulator_disable_regmap,
	.is_enabled           = regulator_is_enabled_regmap,
};

static unsigned int rr320010_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case RR320010_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	}
	return REGULATOR_MODE_NORMAL;
}

static const struct linear_range rr320010_buck_1_2_voltage_ranges[] = {
	REGULATOR_LINEAR_RANGE(2800000, 0x8c, 0xf0, 10000),
	REGULATOR_LINEAR_RANGE(3800000, 0xf1, 0xff, 0),
	REGULATOR_LINEAR_RANGE(2800000, 0x00, 0x8b, 0),
};

static const struct linear_range rr320010_buck_3_voltage_ranges[] = {
	REGULATOR_LINEAR_RANGE(1400000, 0x00, 0x3c, 10000),
	REGULATOR_LINEAR_RANGE(2000000, 0x3d, 0xff, 0),
};

static const struct linear_range rr320010_buck_4_5_6_7_voltage_ranges[] = {
	REGULATOR_LINEAR_RANGE(300000, 0x00, 0xf0, 5000),
	REGULATOR_LINEAR_RANGE(1500000, 0xf1, 0xff, 0),
};

static const struct linear_range rr320010_buck_8_voltage_ranges[] = {
	REGULATOR_LINEAR_RANGE(500000, 0x28, 0xa0, 5000),
	REGULATOR_LINEAR_RANGE(1100000, 0xa1, 0xff, 0),
};

static struct rr320010_regulator_desc rr320010_regulator_descs[] = {
	{
		.desc = {
			.name = "buck1",
			.of_match = of_match_ptr("buck1"),
			.id = RR320010_ID_BUCK1,
			.owner = THIS_MODULE,
			.ops = &rr320010_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_1,
			.enable_mask = BIT(0),
			.enable_val = BIT(0),
			.vsel_reg = RR320010_BUCKS_BUCK1_VOUT,
			.vsel_mask = ~0,
			.n_voltages = 255,
			.linear_ranges = rr320010_buck_1_2_voltage_ranges,
			.n_linear_ranges = ARRAY_SIZE(rr320010_buck_1_2_voltage_ranges),
			.of_map_mode = rr320010_of_map_mode,
		},
		.mode_reg = RR320010_BUCKS_BUCK_MODE_00,
		.mode_mask = BIT(0),
	},
	{
		.desc = {
			.name = "buck2",
			.of_match = of_match_ptr("buck2"),
			.id = RR320010_ID_BUCK2,
			.owner = THIS_MODULE,
			.ops = &rr320010_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_1,
			.enable_mask = BIT(1),
			.enable_val = BIT(1),
			.vsel_reg = RR320010_BUCKS_BUCK2_VOUT,
			.vsel_mask = ~0,
			.n_voltages = 255,
			.linear_ranges = rr320010_buck_1_2_voltage_ranges,
			.n_linear_ranges = ARRAY_SIZE(rr320010_buck_1_2_voltage_ranges),
			.of_map_mode = rr320010_of_map_mode,
		},
		.mode_reg = RR320010_BUCKS_BUCK_MODE_00,
		.mode_mask = BIT(1),
	},
	{
		.desc = {
			.name = "buck3",
			.of_match = of_match_ptr("buck3"),
			.id = RR320010_ID_BUCK3,
			.owner = THIS_MODULE,
			.ops = &rr320010_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_1,
			.enable_mask = BIT(2),
			.enable_val = BIT(2),
			.vsel_reg = RR320010_BUCKS_BUCK3_VOUT,
			.vsel_mask = ~0,
			.n_voltages = 255,
			.linear_ranges = rr320010_buck_3_voltage_ranges,
			.n_linear_ranges = ARRAY_SIZE(rr320010_buck_3_voltage_ranges),
			.of_map_mode = rr320010_of_map_mode,
		},
		.mode_reg = RR320010_BUCKS_BUCK_MODE_00,
		.mode_mask = BIT(2),
	},
	{
		.desc = {
			.name = "buck4",
			.of_match = of_match_ptr("buck4"),
			.id = RR320010_ID_BUCK4,
			.owner = THIS_MODULE,
			.ops = &rr320010_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_1,
			.enable_mask = BIT(3),
			.enable_val = BIT(3),
			.vsel_reg = RR320010_BUCKS_BUCK4_VOUT,
			.vsel_mask = ~0,
			.n_voltages = 255,
			.linear_ranges = rr320010_buck_4_5_6_7_voltage_ranges,
			.n_linear_ranges = ARRAY_SIZE(rr320010_buck_4_5_6_7_voltage_ranges),
			.of_map_mode = rr320010_of_map_mode,
		},
		.mode_reg = RR320010_BUCKS_BUCK_MODE_00,
		.mode_mask = BIT(3),
	},
	{
		.desc = {
			.name = "buck5",
			.of_match = of_match_ptr("buck5"),
			.id = RR320010_ID_BUCK5,
			.owner = THIS_MODULE,
			.ops = &rr320010_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_1,
			.enable_mask = BIT(4),
			.enable_val = BIT(4),
			.vsel_reg = RR320010_BUCKS_BUCK5_VOUT,
			.vsel_mask = ~0,
			.n_voltages = 255,
			.linear_ranges = rr320010_buck_4_5_6_7_voltage_ranges,
			.n_linear_ranges = ARRAY_SIZE(rr320010_buck_4_5_6_7_voltage_ranges),
			.of_map_mode = rr320010_of_map_mode,
		},
		.mode_reg = RR320010_BUCKS_BUCK_MODE_00,
		.mode_mask = BIT(4),
	},
	{
		.desc = {
			.name = "buck6",
			.of_match = of_match_ptr("buck6"),
			.id = RR320010_ID_BUCK6,
			.owner = THIS_MODULE,
			.ops = &rr320010_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_1,
			.enable_mask = BIT(5),
			.enable_val = BIT(5),
			.vsel_reg = RR320010_BUCKS_BUCK6_VOUT,
			.vsel_mask = ~0,
			.n_voltages = 255,
			.linear_ranges = rr320010_buck_4_5_6_7_voltage_ranges,
			.n_linear_ranges = ARRAY_SIZE(rr320010_buck_4_5_6_7_voltage_ranges),
			.of_map_mode = rr320010_of_map_mode,
		},
		.mode_reg = RR320010_BUCKS_BUCK_MODE_00,
		.mode_mask = BIT(5),
	},
	{
		.desc = {
			.name = "buck7",
			.of_match = of_match_ptr("buck7"),
			.id = RR320010_ID_BUCK7,
			.owner = THIS_MODULE,
			.ops = &rr320010_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_1,
			.enable_mask = BIT(6),
			.enable_val = BIT(6),
			.vsel_reg = RR320010_BUCKS_BUCK7_VOUT,
			.vsel_mask = ~0,
			.n_voltages = 255,
			.linear_ranges = rr320010_buck_4_5_6_7_voltage_ranges,
			.n_linear_ranges = ARRAY_SIZE(rr320010_buck_4_5_6_7_voltage_ranges),
			.of_map_mode = rr320010_of_map_mode,
		},
		.mode_reg = RR320010_BUCKS_BUCK_MODE_00,
		.mode_mask = BIT(6),
	},
	{
		.desc = {
			.name = "buck8",
			.of_match = of_match_ptr("buck8"),
			.id = RR320010_ID_BUCK8,
			.owner = THIS_MODULE,
			.ops = &rr320010_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_1,
			.enable_mask = BIT(7),
			.enable_val = BIT(7),
			.vsel_reg = RR320010_BUCKS_BUCK8_VOUT,
			.vsel_mask = ~0,
			.n_voltages = 255,
			.linear_ranges = rr320010_buck_8_voltage_ranges,
			.n_linear_ranges = ARRAY_SIZE(rr320010_buck_8_voltage_ranges),
			.of_map_mode = rr320010_of_map_mode,
		},
		.mode_reg = RR320010_BUCKS_BUCK_MODE_01,
		.mode_mask = GENMASK(1, 0),
	},
	{
		.desc = {
			.name = "hdmi",
			.of_match = of_match_ptr("hdmi"),
			.id = RR320010_ID_HDMI,
			.owner = THIS_MODULE,
			.ops = &rr320010_fixed_regulator_ops,
			.enable_reg = RR320010_SEQUENCER_CONV_CONF_3,
			.enable_mask = BIT(3),
			.enable_val = BIT(3),
			.n_voltages = 1,
			.fixed_uV = 5000000,
		},
	},
};

static
struct regmap_field *rr320010_regulator_regmap_field_alloc(struct rr320010_regulator_device *regdev,
							   u32 reg, u32 mask)
{
	struct reg_field map = REG_FIELD(reg, ffs(mask) - 1, fls(mask) - 1);
	struct regmap_field *rmap;

	if (reg == 0 && mask == 0)
		return NULL;

	dev_dbg(regdev->dev, "reg=%02x, mask=%02x, lsb=%d, msb=%d\n", reg, mask, map.lsb, map.msb);
	rmap = devm_regmap_field_alloc(regdev->dev, regdev->regmap, map);
	if (IS_ERR(rmap))
		dev_err(regdev->dev, "failed to alloc regmap field for: %ld\n", PTR_ERR(rmap));
	return rmap;
}


static int rr320010_regulator_register(struct rr320010_regulator_device *regdev,
				       struct rr320010_regulator_desc *desc)
{
	struct device *dev = regdev->dev;
	struct rr320010_regulator_data *data;
	struct regulator_config config = {};

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->desc = desc;
	data->mode = rr320010_regulator_regmap_field_alloc(regdev, desc->mode_reg,
								   desc->mode_mask);
	if (IS_ERR(data->mode))
		return PTR_ERR(data->mode);

	config.dev         = regdev->dev;
	config.regmap      = regdev->regmap;
	config.driver_data = data;

	data->rdev = devm_regulator_register(dev, &desc->desc, &config);
	if (IS_ERR(data->rdev))
		return PTR_ERR(data->rdev);
	return 0;
}

static int rr320010_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rr320010_regulator_device *regdev;
	int i, ret;

	regdev = devm_kzalloc(dev, sizeof(*regdev), GFP_KERNEL);
	if (!regdev)
		return -ENOMEM;

	regdev->dev = dev;
	regdev->regmap = dev_get_regmap(dev->parent, NULL);
	if (!regdev->regmap)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(rr320010_regulator_descs); i++) {
		ret = rr320010_regulator_register(regdev, &rr320010_regulator_descs[i]);
		if (ret) {
			dev_err(dev, "failed to register %s: %d\n",
				rr320010_regulator_descs[i].desc.name, ret);
			return ret;
		}
	}

	platform_set_drvdata(pdev, regdev);
	return 0;
}

static const struct of_device_id rr320010_regulator_ids[] = {
	{ .compatible = "renesas,rr320010-regulator", },
	{}
};
MODULE_DEVICE_TABLE(of, rr320010_regulator_ids);

static struct platform_driver rr320010_regulator_driver = {
	.driver = {
		.name = "rr320010-regulator",
		.owner = THIS_MODULE,
		.of_match_table = rr320010_regulator_ids,
	},
	.probe    = rr320010_regulator_probe,
};
module_platform_driver(rr320010_regulator_driver);

MODULE_DESCRIPTION("Renesas RR320010 Regulator Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
