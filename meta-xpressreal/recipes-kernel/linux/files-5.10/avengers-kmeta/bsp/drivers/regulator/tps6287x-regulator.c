// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Axis Communications AB
 *
 * Driver for Texas Instruments TPS6287x PMIC.
 * Datasheet: https://www.ti.com/lit/ds/symlink/tps62873.pdf
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/bitfield.h>
#include <linux/linear_range.h>

#define TPS6287X_VSET		0x00
#define TPS6287X_CTRL1		0x01
#define TPS6287X_CTRL1_VRAMP	GENMASK(1, 0)
#define TPS6287X_CTRL1_FPWMEN	BIT(4)
#define TPS6287X_CTRL1_SWEN	BIT(5)
#define TPS6287X_CTRL2		0x02
#define TPS6287X_CTRL2_VRANGE	GENMASK(3, 2)
#define TPS6287X_CTRL3		0x03
#define TPS6287X_STATUS		0x04

/*
 * WORKAROUND: force vrange=2 (400mV~1675mV, 5mV/step).
 * Hardware on this platform always boots with vrange=2; using pickable
 * ranges caused incorrect voltage reads when the boot selection differed
 * from what the driver expected.  Fix to a single range avoids the mismatch.
 */
#define TPS6287X_VRANGE_2_MIN_UV         400000
#define TPS6287X_VRANGE_2_MAX_UV         1675000
#define TPS6287X_VRANGE_2_STEP_UV        5000

static const struct regmap_config tps6287x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = TPS6287X_STATUS,
	/* enable cache to avoid redundant I2C reads during voltage queries */
	.cache_type = REGCACHE_RBTREE,
};

static const unsigned int tps6287x_ramp_table[] = {
	10000, 5000, 1250, 500
};

static int tps6287x_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	unsigned int val;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		val = 0;
		break;
	case REGULATOR_MODE_FAST:
		val = TPS6287X_CTRL1_FPWMEN;
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(rdev->regmap, TPS6287X_CTRL1,
				  TPS6287X_CTRL1_FPWMEN, val);
}

static unsigned int tps6287x_get_mode(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret;

	ret = regmap_read(rdev->regmap, TPS6287X_CTRL1, &val);
	if (ret < 0)
		return 0;

	return (val & TPS6287X_CTRL1_FPWMEN) ? REGULATOR_MODE_FAST :
	    REGULATOR_MODE_NORMAL;
}

static unsigned int tps6287x_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case REGULATOR_MODE_NORMAL:
	case REGULATOR_MODE_FAST:
		return mode;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

/*
 * WORKAROUND: force FPWM mode during voltage transition.
 * In AUTO mode the converter may enter PFM mid-transition, causing voltage
 * overshoot on some boards.  Switch to FPWM before writing VSEL, then
 * restore the original mode after the settling delay.
 */
static int tps6287x_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	int ret;
	u32 mode = tps6287x_get_mode(rdev);
	u32 cur = regulator_get_voltage_sel_regmap(rdev);

	ret = tps6287x_set_mode(rdev, REGULATOR_MODE_FAST);
	if (ret) {
		dev_err(&rdev->dev, "failed to set fast mode: %d\n", ret);
		return ret;
	}

	ret = regulator_set_voltage_sel_regmap(rdev, sel);

	if (!ret && rdev->constraints->ramp_delay) {
		u32 delay = DIV_ROUND_UP(abs(sel - cur) * TPS6287X_VRANGE_2_STEP_UV,
					 rdev->constraints->ramp_delay);

		if (delay)
			udelay(delay);
	}

	tps6287x_set_mode(rdev, mode);
	return ret;
}

/*
 * Return a non-zero settling time so the regulator core does not attempt
 * its own delay calculation (which would be incorrect for the fixed range).
 */
static int tps6287x_set_voltage_time(struct regulator_dev *rdev,
				     int old_uV, int new_uV)
{
	return 1;
}

static const struct regulator_ops tps6287x_regulator_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.set_mode = tps6287x_set_mode,
	.get_mode = tps6287x_get_mode,
	.is_enabled = regulator_is_enabled_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = tps6287x_set_voltage_sel_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.set_ramp_delay = regulator_set_ramp_delay_regmap,
	.set_voltage_time = tps6287x_set_voltage_time,
};

static struct regulator_desc tps6287x_reg = {
	.name = "tps6287x",
	.owner = THIS_MODULE,
	.ops = &tps6287x_regulator_ops,
	.of_map_mode = tps6287x_of_map_mode,
	.type = REGULATOR_VOLTAGE,
	.enable_reg = TPS6287X_CTRL1,
	.enable_mask = TPS6287X_CTRL1_SWEN,
	.vsel_reg = TPS6287X_VSET,
	.vsel_mask = 0xFF,
	.ramp_reg = TPS6287X_CTRL1,
	.ramp_mask = TPS6287X_CTRL1_VRAMP,
	.ramp_delay_table = tps6287x_ramp_table,
	.n_ramp_values = ARRAY_SIZE(tps6287x_ramp_table),
	.n_voltages = 256,
	.min_uV = TPS6287X_VRANGE_2_MIN_UV,
	.uV_step = TPS6287X_VRANGE_2_STEP_UV,
};

struct tps6287x_data {
	struct regmap *regmap;
	/* boot VSET value saved for restore on shutdown */
	unsigned int saved_vset;
};

static int tps6287x_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct regulator_config config = {};
	struct tps6287x_data *data;
	struct regulator_dev *rdev;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	config.regmap = devm_regmap_init_i2c(i2c, &tps6287x_regmap_config);
	if (IS_ERR(config.regmap)) {
		dev_err(dev, "Failed to init i2c\n");
		return PTR_ERR(config.regmap);
	}

	data->regmap = config.regmap;

	ret = regmap_read(config.regmap, TPS6287X_VSET, &data->saved_vset);
	if (ret)
		dev_warn(dev, "Failed to read boot VSET: %d\n", ret);

	i2c_set_clientdata(i2c, data);

	config.dev = dev;
	config.of_node = dev->of_node;
	config.init_data = of_get_regulator_init_data(dev, dev->of_node,
						      &tps6287x_reg);

	rdev = devm_regulator_register(dev, &tps6287x_reg, &config);
	if (IS_ERR(rdev)) {
		dev_err(dev, "Failed to register regulator\n");
		return PTR_ERR(rdev);
	}

	/*
	 * WORKAROUND: set PGBLNKDVS=1 (CTRL3[0]) to blank the power-good
	 * signal during DVS transitions, preventing spurious resets on boards
	 * that monitor PGOOD.
	 */
	regmap_write(config.regmap, TPS6287X_CTRL3, 1);

	dev_dbg(dev, "Probed regulator (boot VSET=0x%02x)\n", data->saved_vset);

	return 0;
}

/*
 * Restore the boot voltage on shutdown so the next boot sees a known-good
 * VSET value.  Without this, a soft reboot after a voltage change may start
 * the CPU at the wrong voltage.
 */
static void tps6287x_shutdown(struct i2c_client *i2c)
{
	struct tps6287x_data *data = i2c_get_clientdata(i2c);

	if (!data->saved_vset)
		return;

	regmap_write(data->regmap, TPS6287X_VSET, data->saved_vset);
}

static const struct of_device_id tps6287x_dt_ids[] = {
	{ .compatible = "ti,tps62870", },
	{ .compatible = "ti,tps62871", },
	{ .compatible = "ti,tps62872", },
	{ .compatible = "ti,tps62873", },
	{ }
};

MODULE_DEVICE_TABLE(of, tps6287x_dt_ids);

static const struct i2c_device_id tps6287x_i2c_id[] = {
	{ "tps62870" },
	{ "tps62871" },
	{ "tps62872" },
	{ "tps62873" },
	{}
};

MODULE_DEVICE_TABLE(i2c, tps6287x_i2c_id);

static struct i2c_driver tps6287x_regulator_driver = {
	.driver = {
		.name = "tps6287x",
		.of_match_table = tps6287x_dt_ids,
	},
	.probe_new = tps6287x_i2c_probe,
	.shutdown = tps6287x_shutdown,
	.id_table = tps6287x_i2c_id,
};

module_i2c_driver(tps6287x_regulator_driver);

MODULE_AUTHOR("Mårten Lindahl <marten.lindahl@axis.com>");
MODULE_DESCRIPTION("Regulator driver for TI TPS6287X PMIC");
MODULE_LICENSE("GPL");
