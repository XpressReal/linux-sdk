// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/mfd/raa271003.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <dt-bindings/regulator/raa271003-regulator.h>

struct raa271003_regulator_device {
	struct regmap *regmap;
	struct device *dev;
};

struct raa271003_regulator_desc {
	struct regulator_desc desc;
	unsigned int mode_reg;
	unsigned int mode_mask;
};

struct raa271003_regulator_data {
	struct regulator_dev *rdev;
	struct raa271003_regulator_desc *desc;
	struct regmap_field *mode;
};

enum {
	RAA271003_ID_BUCK1,
	RAA271003_ID_BUCK2,
	RAA271003_ID_BUCK3,
	RAA271003_ID_BUCK4,
	RAA271003_ID_BUCK5,
};

static const struct linear_range raa271003_buck_voltage_ranges_0[] = {
	REGULATOR_LINEAR_RANGE(275000, 0, 511, 3125),
};

static const struct linear_range raa271003_buck_voltage_ranges_1[] = {
	REGULATOR_LINEAR_RANGE(687500, 0, 511, 7812),
	//REGULATOR_LINEAR_RANGE(1390625, 90, 511, 7812),
};

static int raa271003_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	u8 val[2] = { sel >> 8, sel & 0xff };

	return regmap_bulk_write(rdev->regmap, rdev->desc->vsel_reg, val, 2);
}

static int raa271003_regulator_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	u8 val[2] = {};
	int ret;

	ret = regmap_bulk_read(rdev->regmap, rdev->desc->vsel_reg, &val, 2);
	return ret ? ret : ((val[0] << 8) | val[1]);
}

#define RAA271003_BUCKx_MODE_AUTO       2
#define RAA271003_BUCKx_MODE_FORCE_PWM  8

static int raa271003_regulator_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct raa271003_regulator_data *data = rdev_get_drvdata(rdev);
	unsigned int val = 0;

	if (!data->mode)
		return -EINVAL;

	val = (mode & REGULATOR_MODE_FAST) ? RAA271003_BUCKx_MODE_FORCE_PWM : RAA271003_BUCKx_MODE_AUTO;
	return regmap_field_write(data->mode, val);
}

static unsigned int raa271003_regulator_get_mode(struct regulator_dev *rdev)
{
	struct raa271003_regulator_data *data = rdev_get_drvdata(rdev);
	unsigned int val = 0;

	if (data->mode) {
		regmap_field_read(data->mode, &val);
		return val == RAA271003_BUCKx_MODE_FORCE_PWM ? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
	}
	return REGULATOR_MODE_NORMAL;
}

static const struct regulator_ops raa271003_buck_regulator_ops = {
	.list_voltage	 = regulator_list_voltage_linear_range,
	.map_voltage	 = regulator_map_voltage_linear_range,
	.set_voltage_sel = raa271003_regulator_set_voltage_sel_regmap,
	.get_voltage_sel = raa271003_regulator_get_voltage_sel_regmap,
	.get_mode	 = raa271003_regulator_get_mode,
	.set_mode	 = raa271003_regulator_set_mode,
	.enable	         = regulator_enable_regmap,
	.disable	 = regulator_disable_regmap,
	.is_enabled	 = regulator_is_enabled_regmap,
};

#define RAA271003_OFFSET_BUCKn_VSEL_A   0x00
#define RAA271003_OFFSET_BUCKn_VSEL_B   0x02
#define RAA271003_OFFSET_BUCKn_MODE     0x0d
#define RAA271003_OFFSET_BUCKn_CONF5    0x13
#define RAA271003_OFFSET_BUCKn_STATUS   0x33

#define BUCK_INFO(n) { \
	.name = "buck" #n, \
	.id = RAA271003_ID_BUCK ## n, \
	.base_reg = RAA271003_BUCK ## n ## _VSEL_A_H, \
	.enable_reg = RAA271003_SYSCTL_RESOURCE_CONTROL_A, BIT(n-1), \
	.enable_mask = BIT(n-1), \
}

static const struct raa271003_buck_info {
	const char *name;
	int id;
	u32 base_reg;
	u32 enable_reg;
	u32 enable_mask;
} raa271003_buck_info[] = {
	BUCK_INFO(1),
	BUCK_INFO(2),
	BUCK_INFO(3),
	BUCK_INFO(4),
	BUCK_INFO(5),
};

static
struct regmap_field *raa271003_regulator_regmap_field_alloc(struct raa271003_regulator_device *regdev,
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

static unsigned int raa271003_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case RAA271003_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	}
	return REGULATOR_MODE_NORMAL;
}

static struct raa271003_regulator_desc *
raa271003_regulator_desc_create(struct raa271003_regulator_device *regdev,
				const struct raa271003_buck_info *info)
{
	struct device *dev = regdev->dev;
	struct raa271003_regulator_desc *desc;
	u32 val;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return NULL;

	desc->desc.owner = THIS_MODULE;
	desc->desc.name = info->name;
	desc->desc.of_match = of_match_ptr(info->name);
	desc->desc.id = info->id;
	desc->desc.ops = &raa271003_buck_regulator_ops;

	regmap_read(regdev->regmap, info->base_reg + RAA271003_OFFSET_BUCKn_STATUS, &val);
	desc->desc.vsel_reg = info->base_reg + ((val & 2) ? RAA271003_OFFSET_BUCKn_VSEL_B :
			      RAA271003_OFFSET_BUCKn_VSEL_A);
	desc->desc.vsel_mask = 0x1ff;
	desc->desc.n_voltages = 512;
	regmap_read(regdev->regmap, info->base_reg + RAA271003_OFFSET_BUCKn_CONF5, &val);
	desc->desc.n_linear_ranges = 1;
	desc->desc.linear_ranges = (val & 2) ? raa271003_buck_voltage_ranges_1 :
				   raa271003_buck_voltage_ranges_0;
	desc->desc.enable_reg = info->enable_reg;
	desc->desc.enable_mask = info->enable_mask;
	desc->desc.enable_val = info->enable_mask;
	desc->desc.of_map_mode = raa271003_of_map_mode;
	desc->mode_reg = info->base_reg + RAA271003_OFFSET_BUCKn_MODE;
	desc->mode_mask = 0xf;
	return desc;
};

static int raa271003_regulator_register(struct raa271003_regulator_device *regdev,
					struct raa271003_regulator_desc *desc)
{
	struct device *dev = regdev->dev;
	struct raa271003_regulator_data *data;
	struct regulator_config config = {};

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->desc = desc;
	data->mode = raa271003_regulator_regmap_field_alloc(regdev, desc->mode_reg,
							    desc->mode_mask);
	if (IS_ERR(data->mode))
		return PTR_ERR(data->mode);

	config.dev	 = regdev->dev;
	config.regmap      = regdev->regmap;
	config.driver_data = data;

	data->rdev = devm_regulator_register(dev, &desc->desc, &config);
	if (IS_ERR(data->rdev))
		return PTR_ERR(data->rdev);
	return 0;
}

static int raa271003_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct raa271003_regulator_device *regdev;
	int i, ret;

	regdev = devm_kzalloc(dev, sizeof(*regdev), GFP_KERNEL);
	if (!regdev)
		return -ENOMEM;

	regdev->dev = dev;
	regdev->regmap = dev_get_regmap(dev->parent, NULL);
	if (!regdev->regmap)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(raa271003_buck_info); i++) {
		struct raa271003_regulator_desc *desc;


		desc = raa271003_regulator_desc_create(regdev, &raa271003_buck_info[i]);
		if (!desc) {
			dev_err(dev, "failed to create regulator_desc\n");
			return -ENOMEM;
		}

		ret = raa271003_regulator_register(regdev, desc);
		if (ret) {
			dev_err(dev, "failed to register %s: %d\n", desc->desc.name, ret);
			return ret;
		}
	}

	platform_set_drvdata(pdev, regdev);
	return 0;
}

static const struct of_device_id raa271003_regulator_ids[] = {
	{ .compatible = "renesas,raa271003-regulator", },
	{}
};
MODULE_DEVICE_TABLE(of, raa271003_regulator_ids);

static struct platform_driver raa271003_regulator_driver = {
	.driver = {
		.name = "raa271003-regulator",
		.owner = THIS_MODULE,
		.of_match_table = raa271003_regulator_ids,
	},
	.probe    = raa271003_regulator_probe,
};
module_platform_driver(raa271003_regulator_driver);

MODULE_DESCRIPTION("Renesas RAA271003 Regulator Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
