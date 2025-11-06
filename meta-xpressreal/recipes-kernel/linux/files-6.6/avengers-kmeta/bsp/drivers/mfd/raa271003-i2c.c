// SPDX-License-Identifier: GPL-2.0-only

#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/raa271003.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

static int regmap_raa271003_i2c_write(void *context, const void *data, size_t count)
{
	struct device *dev = context;
	struct i2c_client *client = to_i2c_client(dev);
	u16 addr = (client->addr & ~0x7) | (((u8 *)data)[0] & 0x7);
	struct i2c_msg msg = {
		.addr = addr,
		.flags = 0,
		.len = count - 1,
		.buf = (void *)(data + 1),
	};
	int ret;

	dev_dbg(dev, "%s: data=%*ph, i2c_msg=%*ph", __func__,
		(int)count, data, (int)sizeof(msg), &msg);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret == 1)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int regmap_raa271003_i2c_read(void *context, const void *reg, size_t reg_size,
			   void *val, size_t val_size)
{
	struct device *dev = context;
	struct i2c_client *client = to_i2c_client(dev);
	u16 addr = (client->addr & ~0x7) | (((u8 *)reg)[0] & 0x7);
	struct i2c_msg msgs[2];
	int ret;

	msgs[0].addr = addr;
	msgs[0].flags = 0;
	msgs[0].len = reg_size - 1;
	msgs[0].buf = (void *)(reg + 1);

	msgs[1].addr = addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = val_size;
	msgs[1].buf = val;


	dev_dbg(dev, "%s: reg=%*ph, i2c_msgs=%*ph\n", __func__,
		(int)reg_size, reg, (int)sizeof(msgs), msgs);

	ret = i2c_transfer(client->adapter, msgs, 2);
	dev_dbg(dev, "%s: ret=%d, val=%*ph\n", __func__, ret, (int)val_size, val);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static const struct regmap_bus regmap_raa271003_i2c = {
	.write = regmap_raa271003_i2c_write,
	.read = regmap_raa271003_i2c_read,
	.reg_format_endian_default = REGMAP_ENDIAN_BIG,
	.val_format_endian_default = REGMAP_ENDIAN_BIG,
};

#define devm_regmap_init_raa271003_i2c(i2c, config)  \
	__regmap_lockdep_wrapper(__devm_regmap_init, #config, \
				 i2c, &regmap_raa271003_i2c, i2c, config);

struct raa271003_device {
	struct regmap *regmap;
	struct device *dev;
};

static bool raa271003_i2c_regmap_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RAA271003_SYSCTL_RESOURCE_CONTROL_A:
	case RAA271003_BUCK1_VSEL_A_H ... RAA271003_BUCK1_VSEL_B_L:
	case RAA271003_BUCK1_VSEL_H_ACTUAL:
	case RAA271003_BUCK1_VSEL_L_ACTUAL:
	case RAA271003_BUCK1_MODE:
	case RAA271003_BUCK1_CONF1:
	case RAA271003_BUCK1_CONF5:
	case RAA271003_BUCK1_CONF8:
	case RAA271003_BUCK1_CONF10:
	case RAA271003_BUCK1_STATUS:
	case RAA271003_BUCK2_VSEL_A_H ... RAA271003_BUCK2_VSEL_B_L:
	case RAA271003_BUCK2_VSEL_H_ACTUAL:
	case RAA271003_BUCK2_VSEL_L_ACTUAL:
	case RAA271003_BUCK2_MODE:
	case RAA271003_BUCK2_CONF1:
	case RAA271003_BUCK2_CONF5:
	case RAA271003_BUCK2_CONF8:
	case RAA271003_BUCK2_CONF10:
	case RAA271003_BUCK2_STATUS:
	case RAA271003_BUCK3_VSEL_A_H ... RAA271003_BUCK3_VSEL_B_L:
	case RAA271003_BUCK3_VSEL_H_ACTUAL:
	case RAA271003_BUCK3_VSEL_L_ACTUAL:
	case RAA271003_BUCK3_MODE:
	case RAA271003_BUCK3_CONF1:
	case RAA271003_BUCK3_CONF5:
	case RAA271003_BUCK3_CONF8:
	case RAA271003_BUCK3_CONF10:
	case RAA271003_BUCK3_STATUS:
	case RAA271003_BUCK4_VSEL_A_H ... RAA271003_BUCK4_VSEL_B_L:
	case RAA271003_BUCK4_VSEL_H_ACTUAL:
	case RAA271003_BUCK4_VSEL_L_ACTUAL:
	case RAA271003_BUCK4_MODE:
	case RAA271003_BUCK4_CONF1:
	case RAA271003_BUCK4_CONF5:
	case RAA271003_BUCK4_CONF8:
	case RAA271003_BUCK4_CONF10:
	case RAA271003_BUCK4_STATUS:
	case RAA271003_BUCK5_VSEL_A_H ... RAA271003_BUCK5_VSEL_B_L:
	case RAA271003_BUCK5_VSEL_H_ACTUAL:
	case RAA271003_BUCK5_VSEL_L_ACTUAL:
	case RAA271003_BUCK5_MODE:
	case RAA271003_BUCK5_CONF1:
	case RAA271003_BUCK5_CONF5:
	case RAA271003_BUCK5_CONF8:
	case RAA271003_BUCK5_CONF10:
	case RAA271003_BUCK5_STATUS:
	case RAA271003_SYSCTL_I2C_CONF_A:
		return true;
	}
	return false;
}

static bool raa271003_i2c_regmap_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RAA271003_BUCK1_VSEL_A_H ... RAA271003_BUCK1_VSEL_B_L:
	case RAA271003_SYSCTL_RESOURCE_CONTROL_A:
	case RAA271003_BUCK1_MODE:
	case RAA271003_BUCK1_CONF1:
	case RAA271003_BUCK1_CONF5:
	case RAA271003_BUCK1_CONF8:
	case RAA271003_BUCK1_CONF10:
	case RAA271003_BUCK2_VSEL_A_H ... RAA271003_BUCK2_VSEL_B_L:
	case RAA271003_BUCK2_MODE:
	case RAA271003_BUCK2_CONF1:
	case RAA271003_BUCK2_CONF5:
	case RAA271003_BUCK2_CONF8:
	case RAA271003_BUCK2_CONF10:
	case RAA271003_BUCK3_VSEL_A_H ... RAA271003_BUCK3_VSEL_B_L:
	case RAA271003_BUCK3_MODE:
	case RAA271003_BUCK3_CONF1:
	case RAA271003_BUCK3_CONF5:
	case RAA271003_BUCK3_CONF8:
	case RAA271003_BUCK3_CONF10:
	case RAA271003_BUCK4_VSEL_A_H ... RAA271003_BUCK4_VSEL_B_L:
	case RAA271003_BUCK4_MODE:
	case RAA271003_BUCK4_CONF1:
	case RAA271003_BUCK4_CONF5:
	case RAA271003_BUCK4_CONF8:
	case RAA271003_BUCK4_CONF10:
	case RAA271003_BUCK5_VSEL_A_H ... RAA271003_BUCK5_VSEL_B_L:
	case RAA271003_BUCK5_MODE:
	case RAA271003_BUCK5_CONF1:
	case RAA271003_BUCK5_CONF5:
	case RAA271003_BUCK5_CONF8:
	case RAA271003_BUCK5_CONF10:
		return true;
	}
	return false;
}

static bool raa271003_regmap_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RAA271003_BUCK1_STATUS:
	case RAA271003_BUCK2_STATUS:
	case RAA271003_BUCK3_STATUS:
	case RAA271003_BUCK4_STATUS:
	case RAA271003_BUCK5_STATUS:
		return true;
	}
	return false;
}

static const struct regmap_config raa271003_i2c_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.readable_reg = raa271003_i2c_regmap_readable_reg,
	.writeable_reg = raa271003_i2c_regmap_writeable_reg,
	.volatile_reg = raa271003_regmap_volatile_reg,
	.cache_type = REGCACHE_RBTREE,
};

static struct mfd_cell raa271003_devs[] = {
	{
		.name = "regulator",
		.of_compatible = "renesas,raa271003-regulator",
	},
};

static int raa271003_i2c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct raa271003_device *rrdev;
	int ret;
	u32 val;

	rrdev = devm_kzalloc(dev, sizeof(*rrdev), GFP_KERNEL);
	if (!rrdev)
		return -ENOMEM;

	rrdev->dev = dev;
	rrdev->regmap = devm_regmap_init_raa271003_i2c(dev, &raa271003_i2c_regmap_config);
	if (IS_ERR(rrdev->regmap))
		return PTR_ERR(rrdev->regmap);

	regmap_read(rrdev->regmap, RAA271003_SYSCTL_I2C_CONF_A, &val);
	if (val != (client->addr >> 3)) {
		dev_err(dev, "configure is not matched\n");
		return -EINVAL;
	}



	ret = devm_mfd_add_devices(rrdev->dev, PLATFORM_DEVID_NONE, raa271003_devs,
				   ARRAY_SIZE(raa271003_devs), 0, 0, 0);
	if (ret) {
		dev_err(dev, "failed to add sub-devices: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(client, rrdev);
	return 0;
}

static void raa271003_i2c_remove(struct i2c_client *client)
{
}
static const struct of_device_id raa271003_of_match[] = {
	{ .compatible = "renesas,raa271003", },
	{}
};
MODULE_DEVICE_TABLE(of, raa271003_of_match);

static const struct i2c_device_id raa271003_i2c_ids[] = {
	{ "raa271003" },
	{}
};
MODULE_DEVICE_TABLE(i2c, raa271003_i2c_ids);

static struct i2c_driver raa271003_i2c_driver = {
	.driver = {
		.name = "raa271003",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(raa271003_of_match),
	},
	.id_table = raa271003_i2c_ids,
	.probe    = raa271003_i2c_probe,
	.remove   = raa271003_i2c_remove,
};
module_i2c_driver(raa271003_i2c_driver);

MODULE_DESCRIPTION("Renesas RR320010 MFD Driver");
MODULE_LICENSE("GPL v2");
