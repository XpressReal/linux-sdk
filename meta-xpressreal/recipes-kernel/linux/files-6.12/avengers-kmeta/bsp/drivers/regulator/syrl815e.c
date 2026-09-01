// SPDX-License-Identifier: GPL-2.0+
//
// syrl815e.c - Regulator device driver for SYRL815E
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
#define SYRL815E_REG_VSEL               0x00
#define SYRL815E_REG_VSEL_MASK          0x7f
#define SYRL815E_REG_CONTROL            0x01
#define SYRL815E_BUCK_EN                BIT(7)
#define SYRL815E_GO_BIT                 BIT(6)

static const struct regmap_config syrl815e_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int syrl815e_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	int ret = 0;

	sel <<= ffs(rdev->desc->vsel_mask) - 1;

	ret = regmap_update_bits(rdev->regmap, rdev->desc->apply_reg,
				 rdev->desc->apply_bit,
				 rdev->desc->apply_bit);
	if (ret)
		return ret;

	return regmap_update_bits(rdev->regmap, rdev->desc->vsel_reg,
				 rdev->desc->vsel_mask, sel);
}

static const struct regulator_ops syrl815e_ops = {
	.set_voltage_sel = syrl815e_regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.list_voltage = regulator_list_voltage_linear,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

/* Voltage limits measured in microvolts */
#define SYRL815E_MIN_UV		435000
#define SYRL815E_STEP_UV	5000
#define SYRL815E_N_VOLTAGES	0x7F

static const struct regulator_desc syrl815e_reg = {
	.name = "SYRL815E",
	.id = 0,
	.ops = &syrl815e_ops,
	.type = REGULATOR_VOLTAGE,
	.n_voltages = SYRL815E_N_VOLTAGES,
	.min_uV = SYRL815E_MIN_UV,
	.uV_step = SYRL815E_STEP_UV,
	.vsel_reg = SYRL815E_REG_VSEL,
	.vsel_mask = SYRL815E_REG_VSEL_MASK,
	.enable_reg = SYRL815E_REG_CONTROL,
	.enable_mask = SYRL815E_BUCK_EN,
	.apply_reg = SYRL815E_REG_CONTROL,
	.apply_bit = SYRL815E_GO_BIT,
	/*
	 * This ramp_delay is a conservative default value.
	 */
	.ramp_delay = 200,
	.owner = THIS_MODULE,
};

/*
 * I2C driver interface functions
 */
static int syrl815e_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct regulator_dev *rdev;
	struct regulator_config config = { };
	struct regmap *regmap;
	unsigned int reg;
	int error;

	regmap = devm_regmap_init_i2c(i2c, &syrl815e_regmap_config);
	if (IS_ERR(regmap)) {
		error = PTR_ERR(regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", error);
		return error;
	}

	config.dev = &i2c->dev;
	config.regmap = regmap;

	config.of_node = dev->of_node;
	config.init_data = of_get_regulator_init_data(dev, dev->of_node,
						      &syrl815e_reg);

	if (!config.init_data)
		return -ENOMEM;

	/* Ensure GO_BIT is enabled when probing if necessary */
	error = regmap_read(regmap, SYRL815E_REG_CONTROL, &reg);
	if (error)
		return error;

	/* Probe regulator */
	rdev = devm_regulator_register(&i2c->dev, &syrl815e_reg, &config);
	if (IS_ERR(rdev)) {
		error = PTR_ERR(rdev);
		dev_err(&i2c->dev, "Failed to register SYRL815E regulator: %d\n", error);
		return error;
	}

	return 0;
}

static const struct of_device_id __maybe_unused syrl815e_i2c_of_match[] = {
	{ .compatible = "silergy,syrl815e" },
	{ },
};
MODULE_DEVICE_TABLE(of, syrl815e_i2c_of_match);

static const struct i2c_device_id syrl815e_i2c_id[] = {
	{ "syrl815e", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, syrl815e_i2c_id);

static struct i2c_driver syrl815e_regulator_driver = {
	.driver = {
		.name = "syrl815e",
		.of_match_table	= of_match_ptr(syrl815e_i2c_of_match),
	},
	.probe = syrl815e_i2c_probe,
	.id_table = syrl815e_i2c_id,
};

module_i2c_driver(syrl815e_regulator_driver);

MODULE_AUTHOR("Silergy Corp.");
MODULE_DESCRIPTION("Regulator device driver for Silergy SYRL815E");
MODULE_LICENSE("GPL");
