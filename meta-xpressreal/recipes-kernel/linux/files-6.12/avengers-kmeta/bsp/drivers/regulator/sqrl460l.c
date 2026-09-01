// SPDX-License-Identifier: GPL-2.0+
//
// sqrl460l.c - Regulator device driver for SQRL460L
//
// Copyright (C) 2026 Silergy Corp.
//

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

/*
 * Register definitions.
 */
#define SQRL460L_REG_B1_CTRL		0x00
#define SQRL460L_REG_B2_CTRL		0x01
#define SQRL460L_REG_B3_VSET0		0x02
#define SQRL460L_REG_B3_VSET1		0x03
#define SQRL460L_REG_B4_VSET		0x04
#define SQRL460L_REG_ENABLE		0x07

/* Voltage Config Masks */
#define SQRL460L_BUCK1_VSEL_MASK	0x3F
#define SQRL460L_BUCK2_VSEL_MASK	0x7F
#define SQRL460L_BUCK3_VSEL_MASK	0x7F
#define SQRL460L_BUCK4_VSEL_MASK	0x1F

/* Enable Bit Definitions */
#define SQRL460L_ENABLE_B1		BIT(0)
#define SQRL460L_ENABLE_B2		BIT(1)
#define SQRL460L_ENABLE_B3		BIT(2)
#define SQRL460L_ENABLE_B4		BIT(3)

static const struct regmap_config sqrl460l_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static const struct regulator_ops sqrl460l_buck1_ops = {
	.list_voltage = regulator_list_voltage_table,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

static const struct regulator_ops sqrl460l_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

enum {
	SQRL460L_ID_1,
	SQRL460L_ID_2,
	SQRL460L_ID_3,
	SQRL460L_ID_4,
	SQRL460L_MAX_REGULATORS,
};

static const unsigned int sqrl460l_buck1_voltages[] = {
	2133000, 2167000, 2200000, 2233000, 2267000, 2300000, 2333000, 2367000,
	2400000, 2433000, 2467000, 2500000, 2533000, 2567000, 2600000, 2633000,
	2667000, 2700000, 2733000, 2767000, 2800000, 2833000, 2867000, 2900000,
	2933000, 2967000, 3000000, 3033000, 3067000, 3100000, 3133000, 3167000,
	3200000, 3233000, 3267000, 3300000, 3333000, 3367000, 3400000, 3433000,
	3467000, 3500000, 3533000, 3567000, 3600000, 3633000, 3667000, 3700000,
	3733000, 3767000, 3800000, 3833000, 3867000, 3900000, 3933000, 3967000,
	4000000,
};

static const struct regulator_desc sqrl460l_regulators[] = {
	{
		.name = "buck1",
		.of_match = of_match_ptr("buck1"),
		.id = SQRL460L_ID_1,
		.ops = &sqrl460l_buck1_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.volt_table = sqrl460l_buck1_voltages,
		.n_voltages = 57,
		.vsel_reg = SQRL460L_REG_B1_CTRL,
		.vsel_mask = SQRL460L_BUCK1_VSEL_MASK,
		.enable_reg = SQRL460L_REG_ENABLE,
		.enable_mask = SQRL460L_ENABLE_B1,
		.enable_time = 500,
	},
	{
		.name = "buck2",
		.of_match = of_match_ptr("buck2"),
		.id = SQRL460L_ID_2,
		.ops = &sqrl460l_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.n_voltages = 111,
		.min_uV = 900000,
		.uV_step = 10000,
		.vsel_reg = SQRL460L_REG_B2_CTRL,
		.vsel_mask = SQRL460L_BUCK2_VSEL_MASK,
		.enable_reg = SQRL460L_REG_ENABLE,
		.enable_mask = SQRL460L_ENABLE_B2,
		.ramp_delay = 12500,
	},
	{
		.name = "buck3",
		.of_match = of_match_ptr("buck3"),
		.id = SQRL460L_ID_3,
		.ops = &sqrl460l_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.n_voltages = 71,
		.min_uV = 500000,
		.uV_step = 10000,
		.vsel_reg = SQRL460L_REG_B3_VSET0,
		.vsel_mask = SQRL460L_BUCK3_VSEL_MASK,
		.enable_reg = SQRL460L_REG_ENABLE,
		.enable_mask = SQRL460L_ENABLE_B3,
	},
	{
		.name = "buck4",
		.of_match = of_match_ptr("buck4"),
		.id = SQRL460L_ID_4,
		.ops = &sqrl460l_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.n_voltages = 23,
		.min_uV = 900000,
		.uV_step = 50000,
		.vsel_reg = SQRL460L_REG_B4_VSET,
		.vsel_mask = SQRL460L_BUCK4_VSEL_MASK,
		.enable_reg = SQRL460L_REG_ENABLE,
		.enable_mask = SQRL460L_ENABLE_B4,
	},
};

static int sqrl460l_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct regmap *regmap;
	int i, error;
	struct device_node *regulator_np;

	regmap = devm_regmap_init_i2c(i2c, &sqrl460l_regmap_config);
	if (IS_ERR(regmap)) {
		error = PTR_ERR(regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", error);
		return error;
	}

	regulator_np = of_get_child_by_name(dev->of_node, "regulators");
	if (!regulator_np) {
		dev_err(dev, "Failed to find regulator node\n");
		return -EINVAL;
	}

	for (i = 0; i < SQRL460L_MAX_REGULATORS; i++) {
		struct regulator_dev *rdev;
		struct device_node *child;
		struct regulator_config config = { };

		for_each_child_of_node(regulator_np, child) {
			if (of_node_cmp(child->name, sqrl460l_regulators[i].name))
				continue;

			config.init_data = of_get_regulator_init_data(dev, child, &sqrl460l_regulators[i]);
			break;
		}

		if (!config.init_data)
			return -EINVAL;

		config.dev = dev;
		config.regmap = regmap;

		rdev = devm_regulator_register(dev, &sqrl460l_regulators[i], &config);
		if (IS_ERR(rdev)) {
			error = PTR_ERR(rdev);
			dev_err(dev, "Failed to register %s regulator: %d\n",
				sqrl460l_regulators[i].name, error);
			return error;
		}
	}

	return 0;
}

static const struct of_device_id __maybe_unused sqrl460l_i2c_of_match[] = {
	{ .compatible = "silergy,sqrl460l" },
	{ },
};
MODULE_DEVICE_TABLE(of, sqrl460l_i2c_of_match);

static const struct i2c_device_id sqrl460l_i2c_id[] = {
	{ "sqrl460l", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, sqrl460l_i2c_id);

static struct i2c_driver sqrl460l_regulator_driver = {
	.driver = {
		.name = "sqrl460l",
		.of_match_table	= of_match_ptr(sqrl460l_i2c_of_match),
	},
	.probe = sqrl460l_i2c_probe,
	.id_table = sqrl460l_i2c_id,
};

module_i2c_driver(sqrl460l_regulator_driver);

MODULE_AUTHOR("Silergy Corp.");
MODULE_DESCRIPTION("Regulator device driver for Silergy SQRL460L");
MODULE_LICENSE("GPL");
