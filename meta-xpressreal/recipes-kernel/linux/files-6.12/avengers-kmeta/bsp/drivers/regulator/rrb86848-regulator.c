// SPDX-License-Identifier: GPL-2.0
/*
 * rrb86848-regulator.c - Renesas RRB86848 bidirectional buck-boost charger/regulator
 *
 * Support Linux power_supply framework.
 * SMBus/I2C, 8-bit register address, 16-bit register data (LO byte first).
 *
 * NOTE:
 * - Registers are 16-bit. SMBus Read/Write Word protocol (LO then HI).
 * - 7-bit I2C address = 0x09 (datasheet shows 0x12/0x13 as 8-bit w/ R/W).
 *
 */
//#define DEBUG

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/debugfs.h>

/* Register Addresses (from datasheet) */
#define RRB86848_REG_OUTPUT_CURRENT_LIMIT		0x14
#define RRB86848_REG_OUTPUT_VOLTAGE			0x15
#define RRB86848_REG_CONTROL7				0x36
#define RRB86848_REG_CONTROL6				0x37
#define RRB86848_REG_CONTROL5				0x38
#define RRB86848_REG_CONTROL0				0x39
#define RRB86848_REG_INFORMATION1			0x3A
#define RRB86848_REG_ADAPTER_CURRENT_LIMIT2		0x3B
#define RRB86848_REG_CONTROL1				0x3C
#define RRB86848_REG_CONTROL2				0x3D
#define RRB86848_REG_ADAPTER_CURRENT_LIMIT1		0x3F
#define RRB86848_REG_OTG_VOLTAGE			0x49
#define RRB86848_REG_OTG_CURRENT			0x4A
#define RRB86848_REG_VIN_VOLTAGE			0x4B
#define RRB86848_REG_CONTROL3				0x4C
#define RRB86848_REG_INFORMATION2			0x4D
#define RRB86848_REG_CONTROL4				0x4E
#define RRB86848_REG_DEVICE_ID				0xFF

/* Device IDs */
#define RRB86848_DEVICE_ID_EXPECTED			0x001A
#define RRB86848_MANUFACTURER_ID_EXPECTED		0x0049

/* Control Register Bits */
#define RRB86848_CTRL1_VOUT_DISABLE			BIT(2)
#define RRB86848_CTRL1_OTG_ENABLE			BIT(11)
#define RRB86848_CTRL1_SWITCH_FREQUENCY			BIT(8) | BIT(9)

#define RRB86848_CTRL3_DIGITAL_RESET			BIT(2)

#define RRB86848_CTRL4_ADP_DISCHARGE			BIT(13)

#define RRB86848_CTRL6_OTG_UNDER_VOLT_PROTECTION_DIS	BIT(5)

/* Voltage encoding */
#define RRB86848_VOLTAGE_LSB_16MV			16
#define RRB86848_VOLTAGE_LSB_32MV			32
#define RRB86848_VOLTAGE_THRESHOLD_24576		24576
#define RRB86848_VOLTAGE_THRESHOLD_24608		24608

/* Current encoding */
#define RRB86848_CURRENT_LSB_MA				8

/* OTG Voltage encoding */
#define RRB86848_OTG_VOLTAGE_LSB_18MV			18
#define RRB86848_OTG_VOLTAGE_LSB_36MV			36
#define RRB86848_OTG_VOLTAGE_THRESHOLD_27504 		27504
#define RRB86848_OTG_VOLTAGE_THRESHOLD_27540 		27540

/* Voltage and Current Limits */
#define RRB86848_VOLTAGE_MIN_UV				2400000   /* 2.4V */
#define RRB86848_VOLTAGE_MAX_UV				55000000  /* 55V */
#define RRB86848_OTG_VOLTAGE_MIN_UV			5000000   /* 5V for OTG */
#define RRB86848_OTG_VOLTAGE_MAX_UV			55260000  /* 55.26V */
#define RRB86848_CURRENT_MIN_UA				256000	  /* 0.256A */
#define RRB86848_CURRENT_MAX_UA				12160000  /* 12.16A */

enum rrb86848_regulator_id {
	RRB86848_VBUS_OUTPUT,	 /* Forward mode output voltage */
	RRB86848_VBUS_OTG,		 /* OTG/Reverse mode output voltage */
	RRB86848_NUM_REGULATORS,
};

struct rrb86848_chip {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct regulator_dev *rdev[RRB86848_NUM_REGULATORS];
	struct mutex lock;

	/* GPIO pins */
	struct gpio_desc *enable_gpio;

	/* Mode tracking */
	bool otg_mode;
	bool forward_mode;

	struct dentry *debugfs_root;
};

/* Helper function to encode voltage to register value */

/*
 * Register format (from datasheet Table 2):
 * Bits[14:3] contain the DAC value
 * Bits[2:0] are not used for voltage
 *
 * For Output Voltage:
 * - Values ≤ 24.576V: LSB = 16mV, shifted left by 3
 * - Values ≥ 24.608V: LSB = 32mV, shifted left by 3
 */
static int rrb86848_voltage_to_reg(int voltage_uv, bool is_otg)
{
	int voltage_mv = voltage_uv / 1000;
	int reg_val, dac_value;

	if (is_otg) {
		/* OTG voltage encoding */
		if (voltage_mv <= RRB86848_OTG_VOLTAGE_THRESHOLD_27504) {
			/* LSB = 18mV */
			dac_value = (voltage_mv / RRB86848_OTG_VOLTAGE_LSB_18MV);
		} else {
			/* LSB = 36mV */
			dac_value = (voltage_mv / RRB86848_OTG_VOLTAGE_LSB_36MV);
		}
	} else {
		/* Output voltage encoding */
		if (voltage_mv <= RRB86848_VOLTAGE_THRESHOLD_24576) {
			/* LSB = 16mV */
			dac_value = (voltage_mv / RRB86848_VOLTAGE_LSB_16MV);
		} else {
			/* LSB = 32mV */
			dac_value = (voltage_mv / RRB86848_VOLTAGE_LSB_32MV);
		}
	}

	/* Shift DAC value to bits[14:3] */
	reg_val = dac_value << 3;

	return reg_val;
}

/* Helper function to decode register value to voltage */
static int rrb86848_reg_to_voltage(int reg_val, bool is_otg)
{
	int voltage_mv;
	int dac_value;

	/* Extract DAC value from bits[14:3] */
	dac_value = (reg_val >> 3) & 0xFFF;  /* 12-bit DAC */

	if (is_otg) {
		/* Try 18mV LSB first */
		voltage_mv = dac_value * RRB86848_OTG_VOLTAGE_LSB_18MV;
		if (voltage_mv > RRB86848_OTG_VOLTAGE_THRESHOLD_27504) {
			/* Use 36mV LSB */
			voltage_mv = dac_value * RRB86848_OTG_VOLTAGE_LSB_36MV;
		}
	} else {
		/* Try 16mV LSB first */
		voltage_mv = dac_value * RRB86848_VOLTAGE_LSB_16MV;
		if (voltage_mv > RRB86848_VOLTAGE_THRESHOLD_24576) {
			/* Use 32mV LSB */
			voltage_mv = dac_value * RRB86848_VOLTAGE_LSB_32MV;
		}
	}

	return voltage_mv * 1000; /* Convert to uV */
}

/* Helper function to encode current to register value */
static int rrb86848_current_to_reg(int current_ua)
{
	int current_ma = current_ua / 1000;

	return (current_ma / RRB86848_CURRENT_LSB_MA) << 2;
}

/* Helper function to decode register value to current */
static int rrb86848_reg_to_current(int reg_val)
{
	int current_ma = (reg_val >> 2) * RRB86848_CURRENT_LSB_MA;

	return current_ma * 1000; /* Convert to uA */
}

static int rrb86848_voltage_to_selector(int voltage_uv, bool is_otg)
{
	int voltage_mv = voltage_uv / 1000;
	int selector;

	if (is_otg) {
		/* OTG voltage encoding */
		if (voltage_mv < 5004)
			voltage_mv = 5004;
		if (voltage_mv <= 27504) {
			/* Range 0: 5.004V - 27.504V, LSB = 18mV */
			selector = (voltage_mv - 5004 + 9) / 18;
			if (selector < 0)
				selector = 0;
			if (selector > 1250)
				selector = 1250;
		} else {
			/* Range 1: 27.540V - 55.26V, LSB = 36mV */
			selector = 1251 + (voltage_mv - 27540 + 18) / 36;
			if (selector > 2020)
				selector = 2020;
		}
	} else {
		/* Output voltage encoding */
		if (voltage_mv < 2400)
			voltage_mv = 2400;
		if (voltage_mv <= 24576) {
			/* Range 0: 2.4V - 24.576V, LSB = 16mV */
			selector = (voltage_mv - 2400 + 8) / 16;
			if (selector < 0)
				selector = 0;
			if (selector > 1386)
				selector = 1386;
		} else {
			/* Range 1: 24.608V - 54.912V, LSB = 32mV */
			selector = 1387 + (voltage_mv - 24608 + 16) / 32;
			if (selector > 2333)
				selector = 2333;
		}
	}

	return selector;
}

static int rrb86848_selector_to_voltage(int selector, bool is_otg)
{
	int voltage_mv;

	if (is_otg) {
		if (selector < 0)
			selector = 0;
		if (selector <= 1250) {
			/* Range 0 */
			voltage_mv = 5004 + (selector * 18);
		} else if (selector <= 2020) {
			/* Range 1 */
			voltage_mv = 27540 + ((selector - 1251) * 36);
		} else {
			voltage_mv = 55260;
		}
	} else {
		if (selector < 0)
			selector = 0;
		if (selector <= 1386) {
			/* Range 0 */
			voltage_mv = 2400 + (selector * 16);
		} else if (selector <= 2333) {
			/* Range 1 */
			voltage_mv = 24608 + ((selector - 1387) * 32);
		} else {
			voltage_mv = 54912;
		}
	}

	return voltage_mv * 1000;
}

/*
 * Forward Mode (VBUS Output) Regulator Operations
 */
static int rrb86848_vbus_enable(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int ret;

	dev_dbg(chip->dev, "Enabling VBUS output\n");

	mutex_lock(&chip->lock);

	/* Enable VOUT output (clear disable bit) */
	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL1,
				 RRB86848_CTRL1_VOUT_DISABLE, 0);
	if (ret < 0)
		goto out;

	chip->forward_mode = true;

out:
	mutex_unlock(&chip->lock);
	return ret;
}

static int rrb86848_vbus_disable(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int ret;

	dev_dbg(chip->dev, "Disabling VBUS output\n");

	mutex_lock(&chip->lock);

	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL4,
				RRB86848_CTRL4_ADP_DISCHARGE,
				RRB86848_CTRL4_ADP_DISCHARGE);
	if (ret < 0)
		goto out;

	/* Disable VOUT output */
	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL1,
				RRB86848_CTRL1_VOUT_DISABLE,
				RRB86848_CTRL1_VOUT_DISABLE);
	if (ret < 0)
		goto out;

	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL4,
				RRB86848_CTRL4_ADP_DISCHARGE, 0);
	if (ret < 0)
		goto out;

	chip->forward_mode = false;

out:
	mutex_unlock(&chip->lock);
	return ret;
}

static int rrb86848_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(chip->regmap, RRB86848_REG_CONTROL1, &val);
	if (ret < 0)
		return ret;

	dev_dbg(chip->dev, "Check VBUS is %s\n", !(val & RRB86848_CTRL1_VOUT_DISABLE)?"Enable":"Disable");

	/* VOUT is enabled when bit is 0 (active low) */
	return !(val & RRB86848_CTRL1_VOUT_DISABLE);
}

static int rrb86848_vbus_set_voltage_sel(struct regulator_dev *rdev,
					 unsigned int selector)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int voltage_uv, reg_val;
	int ret;

	voltage_uv = rrb86848_selector_to_voltage(selector, false);
	if (voltage_uv < 0)
		return voltage_uv;

	reg_val = rrb86848_voltage_to_reg(voltage_uv, false);

	dev_dbg(chip->dev, "Setting VBUS voltage to %duV (reg=0x%04x, selector=%d)\n",
		voltage_uv, reg_val, selector);

	mutex_lock(&chip->lock);
	ret = regmap_write(chip->regmap, RRB86848_REG_OUTPUT_VOLTAGE, reg_val);
	mutex_unlock(&chip->lock);

	return ret;
}

static int rrb86848_vbus_get_voltage_sel(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int voltage_uv, selector;;
	int ret;

	ret = regmap_read(chip->regmap, RRB86848_REG_OUTPUT_VOLTAGE, &reg_val);
	if (ret < 0)
		return ret;

	voltage_uv = rrb86848_reg_to_voltage(reg_val, false);

	selector = rrb86848_voltage_to_selector(voltage_uv, false);

	dev_dbg(chip->dev, "Get VBUS reg=0x%04x voltage=%duV selector=%d\n",
		reg_val, voltage_uv, selector);

	return selector;
}

static int rrb86848_vbus_set_current_limit(struct regulator_dev *rdev,
					   int min_ua, int max_ua)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int reg_val;
	int ret;

	if (max_ua < RRB86848_CURRENT_MIN_UA ||
		min_ua > RRB86848_CURRENT_MAX_UA)
		return -EINVAL;

	reg_val = rrb86848_current_to_reg(max_ua);

	dev_dbg(chip->dev, "Setting VBUS current limit to %duA (reg=0x%04x)\n",
		max_ua, reg_val);

	mutex_lock(&chip->lock);
	ret = regmap_write(chip->regmap, RRB86848_REG_OUTPUT_CURRENT_LIMIT, reg_val);
	mutex_unlock(&chip->lock);

	return ret;
}

static int rrb86848_vbus_get_current_limit(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int current_ua;
	int ret;

	ret = regmap_read(chip->regmap, RRB86848_REG_OUTPUT_CURRENT_LIMIT, &reg_val);
	if (ret < 0)
		return ret;

	current_ua = rrb86848_reg_to_current(reg_val);

	dev_dbg(chip->dev, "Get VBUS current limit %duA (reg=0x%04x)\n",
		current_ua, reg_val);

	return current_ua;
}

static const struct regulator_ops rrb86848_vbus_ops = {
	.enable = rrb86848_vbus_enable,
	.disable = rrb86848_vbus_disable,
	.is_enabled = rrb86848_vbus_is_enabled,
	.set_voltage_sel = rrb86848_vbus_set_voltage_sel,
	.get_voltage_sel = rrb86848_vbus_get_voltage_sel,
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_current_limit = rrb86848_vbus_set_current_limit,
	.get_current_limit = rrb86848_vbus_get_current_limit,
};

/*
 * OTG Mode Regulator Operations
 */
static int rrb86848_otg_enable(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int ret;

	dev_dbg(chip->dev, "Enabling OTG mode\n");

	mutex_lock(&chip->lock);

	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL6,
				RRB86848_CTRL6_OTG_UNDER_VOLT_PROTECTION_DIS, 0);
	if (ret < 0)
		goto out;

	/* Enable OTG function */
	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL1,
				 RRB86848_CTRL1_OTG_ENABLE,
				 RRB86848_CTRL1_OTG_ENABLE);
	if (ret < 0)
		goto out;

	chip->otg_mode = true;
out:
	mutex_unlock(&chip->lock);
	return ret;
}

static int rrb86848_otg_disable(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int ret;

	dev_dbg(chip->dev, "Disabling OTG mode\n");

	mutex_lock(&chip->lock);

	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL6,
				RRB86848_CTRL6_OTG_UNDER_VOLT_PROTECTION_DIS,
				RRB86848_CTRL6_OTG_UNDER_VOLT_PROTECTION_DIS);
	if (ret < 0)
		goto out;

	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL4,
				RRB86848_CTRL4_ADP_DISCHARGE,
				RRB86848_CTRL4_ADP_DISCHARGE);
	if (ret < 0)
		goto out;

	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL1,
				 RRB86848_CTRL1_OTG_ENABLE, 0);
	if (ret < 0)
		goto out;

	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL4,
				RRB86848_CTRL4_ADP_DISCHARGE, 0);
	if (ret < 0)
		goto out;

	chip->otg_mode = false;

out:
	mutex_unlock(&chip->lock);
	return ret;
}

static int rrb86848_otg_is_enabled(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(chip->regmap, RRB86848_REG_CONTROL1, &val);
	if (ret < 0)
		return ret;

	dev_dbg(chip->dev, "Check OTG is %s\n", !!(val & RRB86848_CTRL1_OTG_ENABLE)?"Enable":"Disable");

	return !!(val & RRB86848_CTRL1_OTG_ENABLE);
}

static int rrb86848_otg_set_voltage_sel(struct regulator_dev *rdev,
					unsigned int selector)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int voltage_uv, reg_val;
	int ret;

	voltage_uv = rrb86848_selector_to_voltage(selector, true);

	if (voltage_uv < 0)
		return voltage_uv;

	reg_val = rrb86848_voltage_to_reg(voltage_uv, true);

	dev_dbg(chip->dev, "Setting OTG voltage to %duV (reg=0x%04x, selector=%d)\n",
			voltage_uv, reg_val, selector);

	mutex_lock(&chip->lock);
	ret = regmap_write(chip->regmap, RRB86848_REG_OTG_VOLTAGE, reg_val);
	mutex_unlock(&chip->lock);

	return ret;
}

static int rrb86848_otg_get_voltage_sel(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int voltage_uv, selector;
	int ret;

	ret = regmap_read(chip->regmap, RRB86848_REG_OTG_VOLTAGE, &reg_val);
	if (ret < 0)
		return ret;

	voltage_uv = rrb86848_reg_to_voltage(reg_val, true);
	selector = rrb86848_voltage_to_selector(voltage_uv, true);

	dev_dbg(chip->dev, "Get OTG: reg=0x%04x voltage=%duV selector=%d\n",
		reg_val, voltage_uv, selector);

	return selector;
}

static int rrb86848_otg_set_current_limit(struct regulator_dev *rdev,
					  int min_ua, int max_ua)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int reg_val;
	int ret;

	if (max_ua < RRB86848_CURRENT_MIN_UA ||
		min_ua > RRB86848_CURRENT_MAX_UA)
		return -EINVAL;

	reg_val = rrb86848_current_to_reg(max_ua);

	dev_dbg(chip->dev, "Setting OTG current limit to %duA (reg=0x%04x)\n",
			max_ua, reg_val);

	mutex_lock(&chip->lock);
	ret = regmap_write(chip->regmap, RRB86848_REG_OTG_CURRENT, reg_val);
	mutex_unlock(&chip->lock);

	return ret;
}

static int rrb86848_otg_get_current_limit(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int current_ua;
	int ret;

	ret = regmap_read(chip->regmap, RRB86848_REG_OTG_CURRENT, &reg_val);
	if (ret < 0)
		return ret;

	current_ua = rrb86848_reg_to_current(reg_val);

	dev_dbg(chip->dev, "Getting OTG current limit %duA (reg=0x%04x)\n",
		current_ua, reg_val);

	return current_ua;
}

static const struct regulator_ops rrb86848_otg_ops = {
	.enable = rrb86848_otg_enable,
	.disable = rrb86848_otg_disable,
	.is_enabled = rrb86848_otg_is_enabled,
	.set_voltage_sel = rrb86848_otg_set_voltage_sel,
	.get_voltage_sel = rrb86848_otg_get_voltage_sel,
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_current_limit = rrb86848_otg_set_current_limit,
	.get_current_limit = rrb86848_otg_get_current_limit,
};

/* VBUS Output Voltage Ranges */
static const struct linear_range rrb86848_vbus_ranges[] = {
	/*
	 * Range 0: 2.4V - 24.576V
	 * Selector 0-1386, 16mV steps
	 * selector 0 = 2.4V
	 * selector 1386 = 2.4V + 1386*16mV = 24.576V
	 */
	REGULATOR_LINEAR_RANGE(2400000, 0, 1386, 16000),
	/*
	 * Range 1: 24.608V - 54.912V
	 * Selector 1387-2333, 32mV steps
	 * selector 1387 = 24.608V
	 * selector 2333 = 24.608V + (2333-1387)*32mV = 54.912V
	 */
	REGULATOR_LINEAR_RANGE(24608000, 1387, 2333, 32000),
};

/* OTG Voltage Ranges */
static const struct linear_range rrb86848_otg_ranges[] = {
	/*
	 * Range 0: 5.004V - 27.504V
	 * Selector 0-1250, 18mV steps
	 * selector 0 = 5.004V
	 * selector 1250 = 5.004V + 1250*18mV = 27.504V
	 */
	REGULATOR_LINEAR_RANGE(5004000, 0, 1250, 18000),
	/*
	 * Range 1: 27.540V - 55.26V
	 * Selector 1251-2020, 36mV steps
	 * selector 1251 = 27.540V
	 * selector 2020 = 27.540V + (2020-1251)*36mV = 55.26V
	 */
	REGULATOR_LINEAR_RANGE(27540000, 1251, 2020, 36000),
};

/* Regulator descriptors */
static const struct regulator_desc rrb86848_regulators[] = {
	[RRB86848_VBUS_OUTPUT] = {
		.name = "rrb86848-vbus",
		.id = RRB86848_VBUS_OUTPUT,
		.ops = &rrb86848_vbus_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.linear_ranges = rrb86848_vbus_ranges,
		.n_linear_ranges = ARRAY_SIZE(rrb86848_vbus_ranges),
		.n_voltages = 2334,  /* 0-2333 */
		.min_uV = RRB86848_VOLTAGE_MIN_UV,  /* 2.4V */
		.uV_step = 0,  /* Non-linear, use ranges */
	},
	[RRB86848_VBUS_OTG] = {
		.name = "rrb86848-otg",
		.id = RRB86848_VBUS_OTG,
		.ops = &rrb86848_otg_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.linear_ranges = rrb86848_otg_ranges,
		.n_linear_ranges = ARRAY_SIZE(rrb86848_otg_ranges),
		.n_voltages = 2021,  /* 0-2020 */
		.min_uV = RRB86848_OTG_VOLTAGE_MIN_UV,  /* 5.004V */
		.uV_step = 0,  /* Non-linear, use ranges */
	},
};

/* Regmap configuration */
static bool rrb86848_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RRB86848_REG_OUTPUT_CURRENT_LIMIT:
	case RRB86848_REG_OUTPUT_VOLTAGE:
	case RRB86848_REG_CONTROL7:
	case RRB86848_REG_CONTROL6:
	case RRB86848_REG_CONTROL5:
	case RRB86848_REG_CONTROL0:
	case RRB86848_REG_INFORMATION1:
	case RRB86848_REG_ADAPTER_CURRENT_LIMIT2:
	case RRB86848_REG_CONTROL1:
	case RRB86848_REG_CONTROL2:
	case RRB86848_REG_ADAPTER_CURRENT_LIMIT1:
	case RRB86848_REG_OTG_VOLTAGE:
	case RRB86848_REG_OTG_CURRENT:
	case RRB86848_REG_VIN_VOLTAGE:
	case RRB86848_REG_CONTROL3:
	case RRB86848_REG_INFORMATION2:
	case RRB86848_REG_CONTROL4:
	case RRB86848_REG_DEVICE_ID:
		return true;
	default:
		return false;
	}
}

static bool rrb86848_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RRB86848_REG_OUTPUT_CURRENT_LIMIT:
	case RRB86848_REG_OUTPUT_VOLTAGE:
	case RRB86848_REG_CONTROL7:
	case RRB86848_REG_CONTROL6:
	case RRB86848_REG_CONTROL5:
	case RRB86848_REG_CONTROL0:
	case RRB86848_REG_ADAPTER_CURRENT_LIMIT2:
	case RRB86848_REG_CONTROL1:
	case RRB86848_REG_CONTROL2:
	case RRB86848_REG_ADAPTER_CURRENT_LIMIT1:
	case RRB86848_REG_OTG_VOLTAGE:
	case RRB86848_REG_OTG_CURRENT:
	case RRB86848_REG_VIN_VOLTAGE:
	case RRB86848_REG_CONTROL3:
	case RRB86848_REG_CONTROL4:
		return true;
	default:
		return false;
	}
}

static bool rrb86848_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RRB86848_REG_INFORMATION1:
	case RRB86848_REG_INFORMATION2:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config rrb86848_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = 0xFF,
	.cache_type = REGCACHE_RBTREE,
	.val_format_endian = REGMAP_ENDIAN_LITTLE, /* LO byte first on SMBus Read/Write Word */
	.readable_reg = rrb86848_readable_reg,
	.writeable_reg = rrb86848_writeable_reg,
	.volatile_reg = rrb86848_volatile_reg,
};

/* -----------------------*/
static int rrb86848_adapter_set_current_limit(struct regulator_dev *rdev,
					      int min_ua, int max_ua)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	int reg_val;
	int ret;

	if (max_ua < RRB86848_CURRENT_MIN_UA ||
		min_ua > RRB86848_CURRENT_MAX_UA)
		return -EINVAL;

	reg_val = rrb86848_current_to_reg(max_ua);

	dev_dbg(chip->dev, "Setting adapter current limit to %duA (reg=0x%04x)\n",
			max_ua, reg_val);

	mutex_lock(&chip->lock);
	ret = regmap_write(chip->regmap, RRB86848_REG_ADAPTER_CURRENT_LIMIT1, reg_val);
	mutex_unlock(&chip->lock);

	return ret;
}

__maybe_unused
static int rrb86848_adapter_get_current_limit(struct regulator_dev *rdev)
{
	struct rrb86848_chip *chip = rdev_get_drvdata(rdev);
	unsigned int reg_val;
	int ret;

	ret = regmap_read(chip->regmap, RRB86848_REG_ADAPTER_CURRENT_LIMIT1, &reg_val);
	if (ret < 0)
		return ret;

	return rrb86848_reg_to_current(reg_val);
}

static int rrb86848_wait_info2_ready(struct rrb86848_chip *chip,
				     unsigned int timeout_ms)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
	unsigned int val;
	int ret;

	do {
		ret = regmap_read(chip->regmap, RRB86848_REG_INFORMATION2, &val);
		if (!ret && (val & 0xF00) != 0)
			return 0;
		usleep_range(1000, 2000);
	} while (time_before(jiffies, timeout));

	return -ETIMEDOUT;
}

static int rrb86848_get_info(struct rrb86848_chip *chip)
{
	unsigned int val;
	int ret;
	unsigned int info2, state, mode;
	const char *state_names[] = {
					"OFF", "REV_SLP", "ADAPTER", "ACOK",
					"VOUT", "IOUT_LIM", "ENOTG", "OTG",
					"ENLDO5", "N/A", "TRIM/ENCHREF", "N/A",
					"CAL", "AGON/AGONTG", "WAIT", "N/A"
				};
	const char *mode_names[] = {
					"OFF", "Boost", "Buck", "Buck-Boost",
					"N/A", "OTG Boost", "OTG Buck", "OTG Buck-Boost"
				};

	ret = regmap_read(chip->regmap, RRB86848_REG_INFORMATION1, &val);
	if (ret) {
		pr_err("Failed to get information1: %d\n", ret);
		return ret;
	}
	dev_info(chip->dev, "%s information1 = %x\n", __func__, val);

	ret = regmap_read(chip->regmap, RRB86848_REG_INFORMATION2, &val);
	if (ret) {
		pr_err("Failed to get information2: %d\n", ret);
		return ret;
	}
	dev_info(chip->dev, "%s information2 = %x\n", __func__, val);

	info2 = val;

	if (info2 & BIT(14))
		dev_info(chip->dev, "  ACOK: Adapter present\n");
	else
		dev_warn(chip->dev, "  ACOK: No adapter detected\n");

	// check State Machine
	state = (info2 >> 8) & 0xF;
	dev_info(chip->dev, "  State: %s (0x%x)\n", state_names[state], state);

	// check Operation Mode
	mode = (info2 >> 5) & 0x7;
	dev_info(chip->dev, "  Mode: %s (0x%x)\n", mode_names[mode], mode);

	return 0;
}

/**
 * rrb86848_enable_slew_rate - Enable/disable slew rate control
 */
__maybe_unused
static int rrb86848_enable_slew_rate(struct rrb86848_chip *chip, bool enable)
{
	unsigned int val = enable ? BIT(6) : 0;
	return regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL6, BIT(6), val);
}

/**
 * rrb86848_digital_reset - Perform digital reset
 */
__maybe_unused
static int rrb86848_digital_reset(struct rrb86848_chip *chip)
{
	int ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL3,
				       BIT(2), BIT(2));
	if (ret)
		return ret;
	msleep(10);
	return regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL3, BIT(2), 0);
}

/* ===================== Charge On/Off/Change APIs (output mode) ============== */
static int rrb86848_wait_output_voltage(struct rrb86848_chip *chip,
						unsigned int target_uv,
						unsigned int tolerance_uv,
						unsigned int timeout_ms)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms ? timeout_ms : 800);
	unsigned int v;
	int selector;

	do {
		selector = rrb86848_vbus_get_voltage_sel(chip->rdev[RRB86848_VBUS_OUTPUT]);
		v = rrb86848_selector_to_voltage(selector, false);
		if (v >= 0) {
			unsigned int now_uv = v;

			if (now_uv + tolerance_uv >= target_uv && now_uv <= target_uv + tolerance_uv)
				return 0;
		}
		usleep_range(2000, 4000);
	} while (time_before(jiffies, timeout));

	return -ETIMEDOUT;
}

static int rrb86848_output_charge_on(struct rrb86848_chip *chip,
				     unsigned int batt_uv, unsigned int batt_ua,
				     unsigned int vbus_ua, unsigned int bias_ua,
				     unsigned int unit_ua)
{
	int ret;
	unsigned int acli_ua;
	int selector;

	acli_ua = (vbus_ua > (bias_ua + unit_ua)) ? (vbus_ua - (bias_ua + unit_ua)) : 0;
	ret = rrb86848_adapter_set_current_limit(chip->rdev[RRB86848_VBUS_OUTPUT],
						RRB86848_CURRENT_MIN_UA, acli_ua);
	if (ret) {
		pr_err("%s Failed to set adapter current: %duA %d\n", __func__, acli_ua, ret);
		return ret;
	}

	selector = rrb86848_voltage_to_selector(batt_uv, false);
	ret = rrb86848_vbus_set_voltage_sel(chip->rdev[RRB86848_VBUS_OUTPUT], selector);
	if (ret) {
		pr_err("%s Failed to set output voltage: %d %d\n", __func__, batt_uv, ret);
		return ret;
	}

	ret = rrb86848_vbus_set_current_limit(chip->rdev[RRB86848_VBUS_OUTPUT],
						RRB86848_CURRENT_MIN_UA, batt_ua);
	if (ret) {
		pr_err("%s Failed to set output current: %duA %d\n", __func__, batt_ua, ret);
		return ret;
	}

	ret = rrb86848_vbus_enable(chip->rdev[RRB86848_VBUS_OUTPUT]);
	if (ret) {
		pr_err("Failed to enable output: %d\n", ret);
		return ret;
	}

	return 0;
}

static int rrb86848_output_charge_off(struct rrb86848_chip *chip)
{
	int ret;

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL4, 0x2800);
	if (ret) {
		pr_err("%s Failed to set control4 ret=%d\n", __func__, ret);
		return ret;
	}

	ret = rrb86848_vbus_set_voltage_sel(chip->rdev[RRB86848_VBUS_OUTPUT], 0);
	if (ret) {
		pr_err("%s Failed to set output voltage: 0uV %d\n", __func__, ret);
		return ret;
	}

	ret = rrb86848_adapter_set_current_limit(chip->rdev[RRB86848_VBUS_OUTPUT],
			RRB86848_CURRENT_MIN_UA, RRB86848_CURRENT_MIN_UA + 1);
	if (ret) {
		pr_err("%s Failed to set adapter current: 0uA %d\n", __func__, ret);
		return ret;
	}

	ret = rrb86848_vbus_set_current_limit(chip->rdev[RRB86848_VBUS_OUTPUT],
		       RRB86848_CURRENT_MIN_UA, RRB86848_CURRENT_MIN_UA + 1);
	if (ret) {
		pr_err("%s Failed to set output current: 0uA %d\n", __func__, ret);
		return ret;
	}

	ret = rrb86848_vbus_disable(chip->rdev[RRB86848_VBUS_OUTPUT]);
	if (ret) {
		pr_err("Failed to disable output: %d\n", ret);
		return ret;
	}

	(void)rrb86848_wait_output_voltage(chip, 0, 500000, 800);

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL4, 0x0800);
	if (ret) {
		pr_err("%s Failed to set control4 ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int rrb86848_output_charge_change(struct rrb86848_chip *chip,
					  unsigned int vbus_ua, unsigned int bias_ua,
					  unsigned int unit_ua)
{
	int ret;
	unsigned int acli_ua = (vbus_ua > (bias_ua + unit_ua)) ? (vbus_ua - (bias_ua + unit_ua)) : 0;

	ret = rrb86848_adapter_set_current_limit(chip->rdev[RRB86848_VBUS_OUTPUT],
				RRB86848_CURRENT_MIN_UA, acli_ua);
	if (ret) {
		pr_err("%s Failed to set adapter current: %duA %d\n", __func__, acli_ua, ret);
		return ret;
	}

	return 0;
}


/* sysfs: echo "on <batt_uv> <batt_ua> <vbus_ua> [bias_ua] [unit_ua]" > charge_ctl
 *        echo "off" > charge_ctl
 *        echo "change <vbus_ua> [bias_ua] [unit_ua]" > charge_ctl
 */
static ssize_t output_charge_ctl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct rrb86848_chip *chip = dev_get_drvdata(dev);
	char cmd[8] = {0};
	unsigned int batt_uv=0, batt_ua=0, vbus_ua=0, bias_ua=0, unit_ua=0;
	int n=0;

	if (!chip)
		return -ENODEV;

	if (sscanf(buf, "%7s%n", cmd, &n) < 1)
		return -EINVAL;

	if (!strcmp(cmd, "on")) {
		int got = sscanf(buf + n, "%u %u %u %u %u", &batt_uv, &batt_ua, &vbus_ua, &bias_ua, &unit_ua);

		if (got < 3)
			return -EINVAL;
		if (rrb86848_output_charge_on(chip, batt_uv, batt_ua, vbus_ua, bias_ua, unit_ua))
			return -EIO;
	} else if (!strcmp(cmd, "off")) {
		if (rrb86848_output_charge_off(chip))
			return -EIO;
	} else if (!strcmp(cmd, "change")) {
		int got = sscanf(buf + n, "%u %u %u", &vbus_ua, &bias_ua, &unit_ua);

		if (got < 1)
			return -EINVAL;
		if (rrb86848_output_charge_change(chip, vbus_ua, bias_ua, unit_ua))
			return -EIO;
	} else {
		return -EINVAL;
	}
	return count;
}
static DEVICE_ATTR_WO(output_charge_ctl);

/* Source On/Off/Change sequences (otg mode) */
static int rrb86848_wait_otg_voltage(struct rrb86848_chip *chip,
					     unsigned int target_uv,
					     unsigned int tolerance_uv,
					     unsigned int timeout_ms)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms ? timeout_ms : 500);
	unsigned int v;
	int selector;

	do {
		selector = rrb86848_otg_get_voltage_sel(chip->rdev[RRB86848_VBUS_OTG]);
		v = rrb86848_selector_to_voltage(selector, true);
		if (v >= 0) {
			unsigned int now_uv = v;

			if (now_uv + tolerance_uv >= target_uv && now_uv <= target_uv + tolerance_uv)
				return 0;
		}
		usleep_range(2000, 4000);
	} while (time_before(jiffies, timeout));

	return -ETIMEDOUT;
}

static int rrb86848_otg_source_on(struct rrb86848_chip *chip,
				      unsigned int vbus_uv, unsigned int vbus_ua,
				      unsigned int batt_ichg_ua)
{
	int ret;
	unsigned int val;
	int selector;

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL6, 0x0040);
	if (ret) {
		pr_err("%s Failed to set CONTROL6 %d\n", __func__, ret);
		return ret;
	}

	val = (batt_ichg_ua == 0) ? 0 : (batt_ichg_ua * 2);
	ret = rrb86848_vbus_set_current_limit(chip->rdev[RRB86848_VBUS_OTG],
		       RRB86848_CURRENT_MIN_UA, val);
	if (ret) {
		pr_err("%s Failed to set output current limit %duA ret=%d\n", __func__, val, ret);
		return ret;
	}

	val = vbus_uv + 200000;
	selector = rrb86848_voltage_to_selector(val, true);
	ret = rrb86848_otg_set_voltage_sel(chip->rdev[RRB86848_VBUS_OTG], selector);
	if (ret) {
		pr_err("%s Failed to set otg voltage %duV ret=%d\n", __func__, val, ret);
		return ret;
	}

	val = (vbus_ua + 400000);
	ret = rrb86848_otg_set_current_limit(chip->rdev[RRB86848_VBUS_OTG],
			RRB86848_CURRENT_MIN_UA, val);
	if (ret) {
		pr_err("%s Failed to set otg current: %d ret=%d\n", __func__, val, ret);
		return ret;
	}

	ret = rrb86848_otg_enable(chip->rdev[RRB86848_VBUS_OTG]);
	if (ret) {
		pr_err("%s Failed to enable otg mode ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int rrb86848_otg_source_off(struct rrb86848_chip *chip)
{
	int ret;

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL6, 0x0060);
	if (ret) {
		pr_err("%s Failed to set CONTROL6 %d\n", __func__, ret);
		return ret;
	}

	ret = rrb86848_vbus_set_current_limit(chip->rdev[RRB86848_VBUS_OTG],
			RRB86848_CURRENT_MIN_UA, RRB86848_CURRENT_MIN_UA + 1);
	if (ret) {
		pr_err("%s Failed to set output current limit 0uA ret=%d\n", __func__, ret);
		return ret;
	}

	ret = rrb86848_otg_set_voltage_sel(chip->rdev[RRB86848_VBUS_OTG], 0);
	if (ret) {
		pr_err("%s Failed to set otg voltage 0uV ret=%d\n", __func__, ret);
		return ret;
	}

	ret = rrb86848_otg_set_current_limit(chip->rdev[RRB86848_VBUS_OTG],
			RRB86848_CURRENT_MIN_UA, RRB86848_CURRENT_MIN_UA + 1);
	if (ret) {
		pr_err("%s Failed to set otg current: 0 ret=%d\n", __func__, ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL4, 0x2800);
	if (ret) {
		pr_err("%s Failed to set control4 freq ret=%d\n", __func__, ret);
		return ret;
	}

	(void)rrb86848_wait_otg_voltage(chip, 0, 500000, 500);

	ret = rrb86848_otg_disable(chip->rdev[RRB86848_VBUS_OTG]);
	if (ret) {
		pr_err("%s Failed to disable otg mode ret=%d\n", __func__, ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL4, 0x0800);
	if (ret) {
		pr_err("%s Failed to set control4 freq ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int rrb86848_otg_source_change(struct rrb86848_chip *chip,
				      unsigned int vbus_uv, unsigned int vbus_ua)
{
	int ret;
	unsigned int val;
	int selector;

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL6, 0x0060);
	if (ret) {
		pr_err("%s Failed to set CONTROL6 %d\n", __func__, ret);
		return ret;
	}

	val = (vbus_uv + 200000);
	selector = rrb86848_voltage_to_selector(val, true);
	ret = rrb86848_otg_set_voltage_sel(chip->rdev[RRB86848_VBUS_OTG], selector);
	if (ret) {
		pr_err("%s Failed to set otg voltage %duV ret=%d\n", __func__, val, ret);
		return ret;
	}

	val = (vbus_ua + 400000);
	ret = rrb86848_otg_set_current_limit(chip->rdev[RRB86848_VBUS_OTG],
			RRB86848_CURRENT_MIN_UA, val);
	if (ret) {
		pr_err("%s Failed to set otg current: %d ret=%d\n", __func__, val, ret);
		return ret;
	}

	(void)rrb86848_wait_otg_voltage(chip, vbus_uv + 200000, 100000, 800);

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL6, 0x0040);
	if (ret) {
		pr_err("%s Failed to set CONTROL6 %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

/* sysfs knob: echo "on <uv> <ua> <ichg>" | "off" | "change <uv> <ua>" > source_ctl */
static ssize_t otg_source_ctl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct rrb86848_chip *chip = dev_get_drvdata(dev);
	char cmd[8] = {0};
	unsigned int uv=0, ua=0, ichg=0;
	int n=0;

	if (!chip)
		return -ENODEV;

	if (sscanf(buf, "%7s%n", cmd, &n) < 1)
		return -EINVAL;

	if (!strcmp(cmd, "on")) {
		if (sscanf(buf + n, "%u %u %u", &uv, &ua, &ichg) < 2)
			return -EINVAL;
		if (rrb86848_otg_source_on(chip, uv, ua, ichg))
			return -EIO;
	} else if (!strcmp(cmd, "off")) {
		if (rrb86848_otg_source_off(chip))
			return -EIO;
	} else if (!strcmp(cmd, "change")) {
		if (sscanf(buf + n, "%u %u", &uv, &ua) < 2)
			return -EINVAL;
		if (rrb86848_otg_source_change(chip, uv, ua))
			return -EIO;
	} else {
		return -EINVAL;
	}
	return count;
}
static DEVICE_ATTR_WO(otg_source_ctl);

/* Debugfs for i2c reg read/write */
static ssize_t rrb86848_reg_write(struct file *file,
				  const char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct rrb86848_chip *chip = file->private_data;
	char buf[32];
	unsigned int reg, val;
	int ret;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';

	/* get reg address and val */
	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret != 2) {
		dev_err(chip->dev, "Usage: echo 'reg val' > write_reg\n");
		return -EINVAL;
	}

	if (reg > 0xFF || val > 0xFFFF) {
		dev_err(chip->dev, "Invalid reg (0x%x) or val (0x%x)\n", reg, val);
		return -EINVAL;
	}

	dev_info(chip->dev, "Writing reg 0x%02x = 0x%04x\n", reg, val);

	ret = regmap_write(chip->regmap, reg, val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to write register: %d\n", ret);
		return ret;
	}

	return count;
}

/* Debugfs i2c reg read */
static ssize_t rrb86848_reg_read(struct file *file,
				 char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct rrb86848_chip *chip = file->private_data;
	char buf[128];
	unsigned int val;
	int len, ret;

	if (*ppos > 0)
		return 0;

	/* read some reg value */
	len = snprintf(buf, sizeof(buf), "RRB86848 Registers:\n");

	ret = regmap_read(chip->regmap, RRB86848_REG_CONTROL1, &val);
	if (ret == 0)
		len += snprintf(buf + len, sizeof(buf) - len,
				"Control1 (0x3c): 0x%04x\n", val);

	ret = regmap_read(chip->regmap, RRB86848_REG_OUTPUT_VOLTAGE, &val);
	if (ret == 0)
		len += snprintf(buf + len, sizeof(buf) - len,
				"OutputVoltage (0x15): 0x%04x\n", val);

	ret = regmap_read(chip->regmap, RRB86848_REG_INFORMATION2, &val);
	if (ret == 0)
		len += snprintf(buf + len, sizeof(buf) - len,
				"Information2 (0x4d): 0x%04x\n", val);

	if (copy_to_user(user_buf, buf, len))
		return -EFAULT;

	*ppos = len;
	return len;
}

static const struct file_operations rrb86848_reg_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.write = rrb86848_reg_write,
	.read = rrb86848_reg_read,
};

/* Device initialization */
static int rrb86848_init_device(struct rrb86848_chip *chip)
{
	unsigned int device_id;
	int ret;

	/* Read and verify device ID */
	ret = regmap_read(chip->regmap, RRB86848_REG_DEVICE_ID, &device_id);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read device ID: %d\n", ret);
		return ret;
	}

	if (device_id != RRB86848_DEVICE_ID_EXPECTED) {
		dev_err(chip->dev, "Invalid device ID: 0x%04x\n", device_id);
		return -ENODEV;
	}

	dev_info(chip->dev, "RRB86848 detected, ID=0x%04x\n", device_id);

	/* Perform digital reset */
	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL3,
				 RRB86848_CTRL3_DIGITAL_RESET,
				 RRB86848_CTRL3_DIGITAL_RESET);
	if (ret < 0)
		dev_info(chip->dev, "RRB86848 reset (ret=%d)\n", ret);

	msleep(10); /* Wait for reset to complete */

	ret = rrb86848_wait_info2_ready(chip, 300);
	if (ret)
		dev_warn(chip->dev, "INFO2 not ready, continuing\n");

	/* Configure switching frequency to 377kHz (recommended) */
	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL1,
				 0x0300, 0x0300); /* Bits[9:8] = 11 */
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL4,
				 0x0800, 0x0800); /* Bit[11] = 1 */
	if (ret < 0)
		return ret;

	/*
	 * Set default Output Voltage = 5V
	 * 5000mV / 16mV = 312.5 ≈ 313 (0x139)
	 * Register value = 313 << 3 = 2504 = 0x09C8
	 *
	 * But let's use exact calculation:
	 * 5000mV / 16mV = 312.5, round up to 313
	 * 313 * 16mV = 5008mV (5.008V - close enough)
	 * Register = 313 << 3 = 0x09C8
	 */
	/* set default Output Voltage = 5V (0x09C8 = 5V with 16mV LSB) */
	ret = regmap_write(chip->regmap, RRB86848_REG_OUTPUT_VOLTAGE, 0x09C8);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set default output voltage: %d\n", ret);
		return ret;
	}

	/*
	 * Set default OTG Voltage = 5.004V
	 * From datasheet default: 5.004V
	 * 5004mV / 18mV = 278 (0x116)
	 * Register value = 278 << 3 = 2224 = 0x08B0
	 */
	/* set default OTG Voltage = 5V (0x08B0 = 5.004V with 18mV LSB) */
	ret = regmap_write(chip->regmap, RRB86848_REG_OTG_VOLTAGE, 0x08B0);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set default OTG voltage: %d\n", ret);
		return ret;
	}

	/* set default Output Current Limit = 3A */
	ret = regmap_write(chip->regmap, RRB86848_REG_OUTPUT_CURRENT_LIMIT, 0x05DC);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set default current limit: %d\n", ret);
		return ret;
	}

	/* set default OTG Current Limit = 3A */
	ret = regmap_write(chip->regmap, RRB86848_REG_OTG_CURRENT, 0x05DC);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set default OTG current: %d\n", ret);
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL6, 0x0060);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set CONTROL6: 0x0060\n");
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL1, 0x0304);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set CONTROL1: 0x0304\n");
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL3, 0x8081);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set CONTROL3: 0x8081\n");
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL2, 0x2810);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set CONTROL2: 0x2810\n");
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL0, 0x0080);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set CONTROL0: 0x0080\n");
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL4, 0x0800);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set CONTROL4: 0x0800\n");
		return ret;
	}

	ret = regmap_write(chip->regmap, RRB86848_REG_CONTROL5, 0x0800);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to set CONTROL5: 0x0800\n");
		return ret;
	}

	/* Default: disable OTG, disable VOUT */
	ret = regmap_update_bits(chip->regmap, RRB86848_REG_CONTROL1,
				 RRB86848_CTRL1_OTG_ENABLE |
				 RRB86848_CTRL1_VOUT_DISABLE ,
				 RRB86848_CTRL1_VOUT_DISABLE);
	if (ret < 0)
		return ret;

	chip->forward_mode = false;
	chip->otg_mode = false;

	rrb86848_get_info(chip);

	return 0;
}

/* Probe function */
static int rrb86848_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct rrb86848_chip *chip;
	struct device_node *np = dev->of_node;
	struct device_node *regulators_np;
	struct regulator_config config = { };
	int i, ret;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = dev;
	chip->client = client;
	i2c_set_clientdata(client, chip);
	mutex_init(&chip->lock);

	/* Initialize regmap */
	chip->regmap = devm_regmap_init_i2c(client, &rrb86848_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(dev, "Failed to initialize regmap: %d\n", ret);
		return ret;
	}

	regcache_cache_bypass(chip->regmap, true);

	/* Get optional GPIO */
	chip->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(chip->enable_gpio))
		return PTR_ERR(chip->enable_gpio);

	/* Initialize device */
	ret = rrb86848_init_device(chip);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize device: %d\n", ret);
		return ret;
	}

	/* Find regulators node */
	regulators_np = of_get_child_by_name(np, "regulators");
	if (!regulators_np) {
		dev_err(dev, "Missing 'regulators' node\n");
		return -EINVAL;
	}

	/* Register regulators */
	config.dev = dev;
	config.driver_data = chip;
	config.regmap = chip->regmap;

	for (i = 0; i < RRB86848_NUM_REGULATORS; i++) {
		struct device_node *reg_np;
		char reg_name[32];

		snprintf(reg_name, sizeof(reg_name), "%s@%d",
				rrb86848_regulators[i].name, i);

		reg_np = of_get_child_by_name(regulators_np,
				rrb86848_regulators[i].name);
		if (!reg_np) {
			reg_np = of_get_child_by_name(regulators_np, reg_name);
		}

		config.of_node = reg_np;
		config.init_data = of_get_regulator_init_data(dev, reg_np,
					&rrb86848_regulators[i]);

		chip->rdev[i] = devm_regulator_register(dev,
					&rrb86848_regulators[i],
					&config);

		if (IS_ERR(chip->rdev[i])) {
			ret = PTR_ERR(chip->rdev[i]);
			dev_err(dev, "Failed to register %s: %d\n",
					rrb86848_regulators[i].name, ret);
			of_node_put(reg_np);
			of_node_put(regulators_np);
			return ret;
		}

		dev_info(dev, "Registered regulator: %s\n",
				rrb86848_regulators[i].name);

		of_node_put(reg_np);
	}

	of_node_put(regulators_np);

	if (device_create_file(chip->dev, &dev_attr_otg_source_ctl))
		dev_warn(dev, "failed to create otg_source_ctl sysfs\n");

	if (device_create_file(chip->dev, &dev_attr_output_charge_ctl))
		dev_warn(dev, "failed to create output_charge_ctl sysfs\n");

	/* create debugfs folder */
	chip->debugfs_root = debugfs_create_dir("rrb86848", NULL);
	if (IS_ERR(chip->debugfs_root)) {
		dev_warn(dev, "Failed to create debugfs directory\n");
		chip->debugfs_root = NULL;
	} else {
		/* create interface */
		debugfs_create_file("write_reg", 0600, chip->debugfs_root,
				    chip, &rrb86848_reg_fops);
	}

	dev_info(dev, "RRB86848 regulator driver probed successfully\n");

	return 0;
}

static void rrb86848_remove(struct i2c_client *client)
{
	struct rrb86848_chip *chip = i2c_get_clientdata(client);
	int ret;

	debugfs_remove_recursive(chip->debugfs_root);

	/* Disable outputs */
	ret = rrb86848_vbus_disable(chip->rdev[RRB86848_VBUS_OUTPUT]);
	if (ret < 0)
		dev_err(chip->dev, "Failed to disable vbus: %d\n", ret);

	device_remove_file(chip->dev, &dev_attr_otg_source_ctl);
	device_remove_file(chip->dev, &dev_attr_output_charge_ctl);
}

static const struct of_device_id rrb86848_of_match[] = {
	{ .compatible = "renesas,rrb86848" },
	{ }
};
MODULE_DEVICE_TABLE(of, rrb86848_of_match);

static const struct i2c_device_id rrb86848_id[] = {
	{ "rrb86848", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rrb86848_id);

static struct i2c_driver rrb86848_driver = {
	.driver = {
		.name = "rrb86848-regulator",
		.of_match_table = rrb86848_of_match,
	},
	.probe = rrb86848_probe,
	.remove = rrb86848_remove,
	.id_table = rrb86848_id,
};

module_i2c_driver(rrb86848_driver);

MODULE_AUTHOR("Stanley Chang <stanley_chang@realtek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Renesas RRB86848 Bidirectional Buck-Boost Regulator Driver");
