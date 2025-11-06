// SPDX-License-Identifier: GPL-2.0-only

#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/rr320010.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

struct rr320010_device {
	struct regmap *regmap;
	struct device *dev;
};

static bool rr320010_i2c_regmap_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RR320010_SYSCTL_I2C_SLAVE_ADDR:
	case RR320010_SYSCTL_PWR_BUTTON_DEB:
	case RR320010_SYSCTL_PAD_OP_DRV:
	case RR320010_SEQUENCER_CONV_CONF_1 ... RR320010_SEQUENCER_SM_STATUS_1:
	case RR320010_SEQUENCER_WDT_CONF ... RR320010_SEQUENCER_IRQ_MASK_1:
	case RR320010_RTC_BBAT_VSEL ... RR320010_RTC_CLK_OUT_32K:
	case RR320010_RTC_CNT_SEC_A ... RR320010_RTC_SCRATCH:
	case RR320010_I2C_MODE_CONFIG:
	case RR320010_BUCKS_BUCK_DISCRG ... RR320010_BUCKS_BUCK_MODE_01:
	case RR320010_BUCKS_BUCK1_VOUT:
	case RR320010_BUCKS_BUCK2_VOUT:
	case RR320010_BUCKS_BUCK3_VOUT:
	case RR320010_BUCKS_BUCK4_VOUT:
	case RR320010_BUCKS_BUCK5_VOUT:
	case RR320010_BUCKS_BUCK6_VOUT:
	case RR320010_BUCKS_BUCK7_VOUT:
	case RR320010_BUCKS_BUCK8_VOUT:
	case RR320010_BUCKS_BUCK8_CFG_0A:
	case RR320010_BUCKS_BUCK8_CFG_0B:
	case RR320010_BUCKS_EVENT ... RR320010_BUCKS_IRQ_MASK:
	case RR320010_GPADC_DATA_SLOT_0_B0 ... RR320010_GPADC_IRQ_MASK:
		return true;
	}
	return false;
}

static bool rr320010_i2c_regmap_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RR320010_SYSCTL_I2C_SLAVE_ADDR:
	case RR320010_SYSCTL_PWR_BUTTON_DEB:
	case RR320010_SYSCTL_PAD_OP_DRV:
	case RR320010_SEQUENCER_CONV_CONF_1 ... RR320010_SEQUENCER_SW_RESET:
	case RR320010_SEQUENCER_WDT_CONF ... RR320010_SEQUENCER_EVENT:
	case RR320010_SEQUENCER_IRQ_MASK:
	case RR320010_SEQUENCER_EVENT_1:
	case RR320010_SEQUENCER_IRQ_MASK_1:
	case RR320010_RTC_BBAT_VSEL ... RR320010_RTC_CLK_OUT_32K:
	case RR320010_RTC_CNT_SEC_A ... RR320010_RTC_EVENT:
	case RR320010_RTC_IRQ_MASK ... RR320010_RTC_SCRATCH:
	case RR320010_I2C_MODE_CONFIG:
	case RR320010_BUCKS_BUCK_DISCRG ... RR320010_BUCKS_BUCK_MODE_01:
	case RR320010_BUCKS_BUCK1_VOUT:
	case RR320010_BUCKS_BUCK2_VOUT:
	case RR320010_BUCKS_BUCK3_VOUT:
	case RR320010_BUCKS_BUCK4_VOUT:
	case RR320010_BUCKS_BUCK5_VOUT:
	case RR320010_BUCKS_BUCK6_VOUT:
	case RR320010_BUCKS_BUCK7_VOUT:
	case RR320010_BUCKS_BUCK8_VOUT:
	case RR320010_BUCKS_BUCK8_CFG_0A:
	case RR320010_BUCKS_BUCK8_CFG_0B:
	case RR320010_BUCKS_EVENT:
	case RR320010_BUCKS_IRQ_MASK:
	case RR320010_GPADC_SLOT_0_CHSEL ... RR320010_GPADC_EVENT:
	case RR320010_GPADC_IRQ_MASK:
	case RR320010_HDMI_CFG:
	case RR320010_HDMI_EVENT:
		return true;
	}
	return false;
}

static bool rr320010_i2c_regmap_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RR320010_SEQUENCER_CONV_STATUS_1 ... RR320010_SEQUENCER_SM_STATUS_1:
	case RR320010_SEQUENCER_EVENT ... RR320010_SEQUENCER_IRQ_MASK_1:
	case RR320010_RTC_EVENT ... RR320010_RTC_IRQ_MASK:
	case RR320010_BUCKS_EVENT ... RR320010_BUCKS_IRQ_MASK:
	case RR320010_GPADC_DATA_SLOT_0_B0 ... RR320010_GPADC_DATA_SLOT_7_B1:
	case RR320010_GPADC_TRIG_MEASUREMENT:
	case RR320010_GPADC_EVENT ... RR320010_GPADC_IRQ_MASK:
		return true;
	}
	return false;
}

static bool rr320010_i2c_regmap_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RR320010_SEQUENCER_EVENT:
	case RR320010_SEQUENCER_EVENT_1:
	case RR320010_RTC_EVENT:
	case RR320010_BUCKS_EVENT:
	case RR320010_GPADC_EVENT:
		return true;
	}
	return false;
}

static const struct regmap_config rr320010_i2c_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = RR320010_HDMI_IRQ_MASK,
	.readable_reg = rr320010_i2c_regmap_readable_reg,
	.writeable_reg = rr320010_i2c_regmap_writeable_reg,
	.volatile_reg = rr320010_i2c_regmap_volatile_reg,
	.precious_reg = rr320010_i2c_regmap_precious_reg,
	.cache_type = REGCACHE_RBTREE,
};

static struct mfd_cell rr320010_devs[] = {
	{
		.name = "regulator",
		.of_compatible = "renesas,rr320010-regulator",
	},
};

static int rr320010_i2c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct rr320010_device *rrdev;
	int ret;

	rrdev = devm_kzalloc(dev, sizeof(*rrdev), GFP_KERNEL);
	if (!rrdev)
		return -ENOMEM;

	rrdev->regmap = devm_regmap_init_i2c(client, &rr320010_i2c_regmap_config);
	if (IS_ERR(rrdev->regmap))
		return PTR_ERR(rrdev->regmap);

	rrdev->dev = dev;
	i2c_set_clientdata(client, rrdev);

	ret = devm_mfd_add_devices(rrdev->dev, PLATFORM_DEVID_NONE, rr320010_devs,
				   ARRAY_SIZE(rr320010_devs), 0, 0, 0);
	if (ret) {
		dev_err(dev, "failed to add sub-devices: %d\n", ret);
		return ret;
	}
	return 0;
}

static void rr320010_i2c_remove(struct i2c_client *client)
{
}
static const struct of_device_id rr320010_of_match[] = {
	{ .compatible = "renesas,rr320010", },
	{}
};
MODULE_DEVICE_TABLE(of, rr320010_of_match);

static const struct i2c_device_id rr320010_i2c_ids[] = {
	{ "rr320010" },
	{}
};
MODULE_DEVICE_TABLE(i2c, rr320010_i2c_ids);

static struct i2c_driver rr320010_i2c_driver = {
	.driver = {
		.name = "rr320010",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rr320010_of_match),
	},
	.id_table = rr320010_i2c_ids,
	.probe    = rr320010_i2c_probe,
	.remove   = rr320010_i2c_remove,
};
module_i2c_driver(rr320010_i2c_driver);

MODULE_DESCRIPTION("Renesas RR320010 MFD Driver");
MODULE_LICENSE("GPL v2");
