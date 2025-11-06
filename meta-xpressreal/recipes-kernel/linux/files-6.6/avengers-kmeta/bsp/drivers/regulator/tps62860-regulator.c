// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <dt-bindings/regulator/ti,tps62860.h>

#define TPS62860_VOUT1          0x01
#define TPS62860_VOUT1_VO1_SET  GENMASK(6, 0)

#define TPS62860_VOUT2          0x02

#define TPS62860_CONTROL        0x03
#define TPS62860_CONTROL_FPWM   BIT(4)
#define TPS62860_CONTROL_SWEN   BIT(5)

#define TPS62860_STATUS         0x05

#define TPS62860_MIN_UV         400000
#define TPS62860_MAX_UV         1987500
#define TPS62860_STEP_UV        12500

static bool tps62860_regmap_readable_reg(struct device *dev, unsigned int reg)
{
        switch (reg) {
        case TPS62860_VOUT1 ... TPS62860_CONTROL:
	case TPS62860_STATUS:
		return true;
	default:
		return false;
        }
}

static const struct regmap_config tps62860_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = TPS62860_STATUS,
	.readable_reg = tps62860_regmap_readable_reg,
	.cache_type = REGCACHE_RBTREE,
};

static int tps62860_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	unsigned int val;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		val = 0;
		break;
	case REGULATOR_MODE_FAST:
		val = TPS62860_CONTROL_FPWM;
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(rdev->regmap, TPS62860_CONTROL,
				  TPS62860_CONTROL_FPWM, val);
}

static unsigned int tps62860_get_mode(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret;

	ret = regmap_read(rdev->regmap, TPS62860_CONTROL, &val);
	if (ret < 0)
		return 0;

	return (val & TPS62860_CONTROL_FPWM) ? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

static const struct regulator_ops tps62860_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.set_mode = tps62860_set_mode,
	.get_mode = tps62860_get_mode,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
};

static unsigned int tps62860_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case TPS62860_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case TPS62860_MODE_FPWM:
		return REGULATOR_MODE_FAST;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static const struct regulator_desc tps62860_reg = {
	.name = "tps62860",
	.owner = THIS_MODULE,
	.ops = &tps62860_regulator_ops,
	.of_map_mode = tps62860_of_map_mode,
	.type = REGULATOR_VOLTAGE,
	.n_voltages = ((TPS62860_MAX_UV - TPS62860_MIN_UV) / TPS62860_STEP_UV) + 1,
	.min_uV = TPS62860_MIN_UV,
	.uV_step = TPS62860_STEP_UV,
	.vsel_reg = TPS62860_VOUT1,
	.vsel_mask = TPS62860_VOUT1_VO1_SET,
	.enable_reg = TPS62860_CONTROL,
	.enable_mask = TPS62860_CONTROL_SWEN,
	.ramp_delay = 1000,
	/* tDelay + tRamp, rounded up */
	.enable_time = 3000,
};

static const struct of_device_id tps62860_dt_ids[] = {
	{ .compatible = "ti,tps6286x", },
	{ .compatible = "ti,tps628610", },
	{ .compatible = "ti,tps628601", },
	{ .compatible = "ti,tps628603", },
	{ .compatible = "ti,tps628604", },
	{ }
};
MODULE_DEVICE_TABLE(of, tps62860_dt_ids);

struct tps62860_data {
	struct regulator_desc desc;
};

static int tps62860_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct regmap *regmap;
	struct tps62860_data *data;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->desc = tps62860_reg;
	if (of_property_read_bool(dev->of_node, "tps62860,vsel-high"))
		data->desc.vsel_reg = TPS62860_VOUT2;

	regmap = devm_regmap_init_i2c(i2c, &tps62860_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	config.dev = &i2c->dev;
	config.of_node = dev->of_node;
	config.regmap = regmap;
	config.driver_data = data;
	config.init_data = of_get_regulator_init_data(dev, dev->of_node, &data->desc);

	rdev = devm_regulator_register(&i2c->dev, &data->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(&i2c->dev, "Failed to register tps62860 regulator\n");
		return PTR_ERR(rdev);
	}

	return 0;
}

static const struct i2c_device_id tps62860_i2c_id[] = {
	{ "tps6286x", 0 },
	{ "tps628610", 0 },
	{ "tps628601", 0 },
	{ "tps628603", 0 },
	{ "tps628604", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, tps62860_i2c_id);

static struct i2c_driver tps62860_regulator_driver = {
	.driver = {
		.name = "tps62860",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = tps62860_dt_ids,
	},
	.probe = tps62860_i2c_probe,
	.id_table = tps62860_i2c_id,
};

module_i2c_driver(tps62860_regulator_driver);

MODULE_LICENSE("GPL v2");
