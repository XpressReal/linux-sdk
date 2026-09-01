// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/of_regulator.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>

#define STI8070A_REG_VSEL0      0x00
#define STI8070A_REG_VSEL1      0x01
#define STI8070A_REG_CONTROL    0x02
#define STI8070A_REG_ID0        0x03
#define STI8070A_REG_ID1        0x04
#define STI8070A_REG_PGOOD      0x05

#define STI8070A_MASK_VSEL      0x3f
#define STI8070A_MASK_BUCK_EN   0x80
#define STI8070A_MASK_MODE      0x40

#define STI8070A_MODE_AUTO        0
#define STI8070A_MODE_FORCE_PWM   1


struct sti8070a_regulator_data {
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regulator_init_data *reg_init_data;
	struct regmap *regmap;
	int vsel;
};


static int sti8070a_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct sti8070a_regulator_data *data = rdev_get_drvdata(rdev);
	int val;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = STI8070A_MODE_FORCE_PWM;
		break;

	case REGULATOR_MODE_NORMAL:
		val = STI8070A_MODE_AUTO;
		break;

	default:
		return -EINVAL;
	}

	return regmap_update_bits(data->regmap, data->vsel, STI8070A_MASK_MODE, val << 6);
}

static unsigned int sti8070a_get_mode(struct regulator_dev *rdev)
{
	struct sti8070a_regulator_data *data = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(data->regmap, data->vsel, &val);
	if (ret < 0) {
		dev_err(data->dev, " failed to read mode: %d\n", ret);
		return ret;
	}
	return val ? REGULATOR_MODE_FAST : REGULATOR_MODE_NORMAL;
}

static unsigned int sti8070a_regulator_of_map_mode(unsigned int mode)
{
	switch (mode) {
	case 0:
		return REGULATOR_MODE_NORMAL;
	case 1:
		return REGULATOR_MODE_FAST;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static const struct regulator_ops sti8070a_regulator_ops = {
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.set_voltage_time_sel	= regulator_set_voltage_time_sel,
	.enable                 = regulator_enable_regmap,
	.disable                = regulator_disable_regmap,
	.is_enabled             = regulator_is_enabled_regmap,
	.set_mode		= sti8070a_set_mode,
	.get_mode		= sti8070a_get_mode,
};

static const struct regmap_config sti8070a_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= STI8070A_REG_PGOOD,
	.cache_type		= REGCACHE_RBTREE,
};

static int sti8070a_of_parse_config(struct sti8070a_regulator_data *data)
{
	struct device_node *np = data->dev->of_node;

	of_property_read_u32(np, "toll,sti8070a-vsel", &data->vsel);
	if (data->vsel != 0 && data->vsel != 1)
		return -EINVAL;

	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id sti8070a_of_match[] = {
	{ .compatible = "toll,sti8070a", },
	{},
};
MODULE_DEVICE_TABLE(of, sti8070a_of_match);
#endif

static int sti8070a_regulator_register(struct sti8070a_regulator_data *data,
			struct regulator_config *config)
{
	struct regulator_desc *desc = &data->desc;

	desc->name = "sti8070a-reg";
	desc->supply_name = "vin";
	desc->ops         = &sti8070a_regulator_ops;
	desc->type        = REGULATOR_VOLTAGE;
	desc->n_voltages  = 64;
	desc->uV_step     = 12500;
	desc->min_uV      = 712500;
	desc->enable_reg  = data->vsel;
	desc->enable_mask = STI8070A_MASK_BUCK_EN;
	desc->vsel_reg    = data->vsel;
	desc->vsel_mask   = STI8070A_MASK_VSEL;
	desc->of_map_mode = sti8070a_regulator_of_map_mode;
	desc->owner = THIS_MODULE;

	data->rdev = devm_regulator_register(data->dev, desc, config);
	return PTR_ERR_OR_ZERO(data->rdev);
}

static int sti8070a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	struct regulator_config config = { 0 };
	struct sti8070a_regulator_data *data;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->reg_init_data = of_get_regulator_init_data(dev, np, &data->desc);
	if (!data->reg_init_data) {
		dev_err(dev, "failed to get regulator_init_data\n");
		return -EINVAL;
	}

	data->dev = dev;
	ret = sti8070a_of_parse_config(data);
	if (ret) {
		dev_err(dev, "failed to parse OF config: %d\n", ret);
		return ret;
	}

	data->regmap = devm_regmap_init_i2c(client, &sti8070a_regmap_config);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		dev_err(dev, "failed to init i2c regmap: %d\n", ret);
		return ret;
	}
	i2c_set_clientdata(client, data);

	config.dev         = dev;
	config.init_data   = data->reg_init_data;
	config.driver_data = data;
	config.of_node     = np;
	config.regmap      = data->regmap;

	ret = sti8070a_regulator_register(data, &config);
	if (ret)
		dev_err(dev, "failed to register regulator: %d\n", ret);
	return ret;
}

static void sti8070a_shutdown(struct i2c_client *client)
{
}

static const struct i2c_device_id sti8070a_id[] = {
	{ .name = "sti8070a", },
	{}
};

MODULE_DEVICE_TABLE(i2c, sti8070a_id);

static struct i2c_driver sti8070a_i2c_driver = {
	.driver = {
		.name = "sti8070a",
		.of_match_table = of_match_ptr(sti8070a_of_match),
	},
	.probe = sti8070a_probe,
	.shutdown = sti8070a_shutdown,
	.id_table = sti8070a_id,
};
module_i2c_driver(sti8070a_i2c_driver);

MODULE_DESCRIPTION("Toll STI8970A Regulator Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
