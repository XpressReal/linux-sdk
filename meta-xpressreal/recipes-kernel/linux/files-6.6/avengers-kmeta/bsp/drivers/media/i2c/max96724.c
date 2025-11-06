// SPDX-License-Identifier: GPL-2.0-only
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-common.h>
#include <media/v4l2-subdev.h>

#define REG_3            0x0003
#define REG_6            0x0006
#define REG_VIDEO_RX8_A  0x0108
#define REG_VIDEO_RX8_B  0x011A
#define REG_VIDEO_RX8_C  0x012C
#define REG_VIDEO_RX8_D  0x013E
#define REG_PHY25        0x08D0
#define REG_PHY26        0x08D1
#define REG_PHY27        0x08D2
#define REG_PHY28        0x08D3
#define REG_BACKTOP12    0x040B

#define VIDEO_RX8_get_vid_lock(data)      ((0x00000040&(data))>>6)
#define VIDEO_RX8_get_vid_pkt_det(data)   ((0x00000020&(data))>>5)
#define PHY25_get_csi2_tx1_pkt_cnt(data)  ((0x000000F0&(data))>>4)
#define PHY25_get_csi2_tx0_pkt_cnt(data)  ((0x0000000F&(data))>>0)
#define PHY26_get_csi2_tx3_pkt_cnt(data)  ((0x000000F0&(data))>>4)
#define PHY26_get_csi2_tx2_pkt_cnt(data)  ((0x0000000F&(data))>>0)
#define PHY27_get_phy1_pkt_cnt(data)      ((0x000000F0&(data))>>4)
#define PHY27_get_phy0_pkt_cnt(data)      ((0x0000000F&(data))>>0)
#define PHY28_get_phy3_pkt_cnt(data)      ((0x000000F0&(data))>>4)
#define PHY28_get_phy2_pkt_cnt(data)      ((0x0000000F&(data))>>0)


enum max96724_region {
	REGION_DES,
	REGION_GMSL_A,
	REGION_GMSL_B,
	REGION_GMSL_C,
	REGION_GMSL_D,
	REGION_MAX,
};

struct regval {
	u32 addr;
	u8 val;
};

struct max96724_debug {
	struct dentry *debugfs_dir;
};

struct max96724_priv {
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct v4l2_subdev sd;
	struct i2c_client *client;

	struct i2c_client *i2c_clients[REGION_MAX];
	struct regmap *regmap[REGION_MAX];

	struct gpio_desc *rst_gpio;
	u32 en_deskew;

	struct max96724_debug debug;
};

#define to_max96724(x) container_of(x, struct max96724_priv, x)

#define MAX96724_REGMAP_CONF(n) \
{ \
	.name = n, \
	.reg_bits = 16, \
	.val_bits = 8, \
	.max_register = 0x1271, \
	.cache_type = REGCACHE_NONE, \
}

static const struct regmap_config max96724_i2c_regmap[] = {
	MAX96724_REGMAP_CONF("des"),
	MAX96724_REGMAP_CONF("gmsl_a"),
	MAX96724_REGMAP_CONF("gmsl_b"),
	MAX96724_REGMAP_CONF("gmsl_c"),
	MAX96724_REGMAP_CONF("gmsl_d"),
};

/* INSTRUCTIONS FOR DESERIALIZER MAX96724 */
static const struct regval des_cfg_table[] = {
	/* Video Pipes And Routing Configuration */
	{0x00F0, 0x40},
	{0x00F1, 0xC8},
	{0x00F4, 0x0F},
	/* Pipe to Controller Mapping Configuration */
	{0x090B, 0x0F},
	{0x090C, 0x00},
	{0x090D, 0x1E},
	{0x090E, 0x1E},
	{0x090F, 0x00},
	{0x0910, 0x00},
	{0x0911, 0x01},
	{0x0912, 0x01},
	{0x0913, 0x12},
	{0x0914, 0x12},
	{0x092D, 0x55},
	{0x094B, 0x0F},
	{0x094C, 0x00},
	{0x094D, 0x12},
	{0x094E, 0x52},
	{0x094F, 0x00},
	{0x0950, 0x40},
	{0x0951, 0x01},
	{0x0952, 0x41},
	{0x0953, 0x1E},
	{0x0954, 0x5E},
	{0x096D, 0x55},
	{0x098B, 0x0F},
	{0x098C, 0x00},
	{0x098D, 0x12},
	{0x098E, 0x92},
	{0x098F, 0x00},
	{0x0990, 0x80},
	{0x0991, 0x01},
	{0x0992, 0x81},
	{0x0993, 0x1E},
	{0x0994, 0x9E},
	{0x09AD, 0x55},
	{0x09CB, 0x0F},
	{0x09CC, 0x00},
	{0x09CD, 0x12},
	{0x09CE, 0xD2},
	{0x09CF, 0x00},
	{0x09D0, 0xC0},
	{0x09D1, 0x01},
	{0x09D2, 0xC1},
	{0x09D3, 0x1E},
	{0x09D4, 0xDE},
	{0x09ED, 0x55},
	/* Double Mode Configuration */
	{0x0973, 0x10},
	/* MIPI D-PHY Configuration */
	{0x08A0, 0x04},
	{0x094A, 0xD0},
	{0x08A3, 0xE4},
	{0x08A5, 0x00},
	{0x0943, 0x07},
	{0x0944, 0x01},
	{0x1D00, 0xF4},
	/*
	 * This is to set predefined (coarse) CSI output frequency
	 * CSI Phy 1 is 1500 Mbps/lane.
	 */
	{0x1D00, 0xF4},
	{0x0418, 0x2F},
	{0x1D00, 0xF5},
	{0x08A2, 0x34},
	{0x040B, 0x02},
};

static const struct regval des_cfg_table_deskew[] = {
	/* Video Pipes And Routing Configuration */
	{0x00F0, 0x40},
	{0x00F1, 0xC8},
	{0x00F4, 0x0F},
	/* Pipe to Controller Mapping Configuration */
	{0x090B, 0x0F},
	{0x090C, 0x00},
	{0x090D, 0x12}, /* enable deskew */
	{0x090E, 0x12}, /* enable deskew */
	{0x090F, 0x00},
	{0x0910, 0x00},
	{0x0911, 0x01},
	{0x0912, 0x01},
	{0x0913, 0x1E}, /* enable deskew */
	{0x0914, 0x1E}, /* enable deskew */
	{0x092D, 0x55},
	{0x094B, 0x0F},
	{0x094C, 0x00},
	{0x094D, 0x12},
	{0x094E, 0x52},
	{0x094F, 0x00},
	{0x0950, 0x40},
	{0x0951, 0x01},
	{0x0952, 0x41},
	{0x0953, 0x1E},
	{0x0954, 0x5E},
	{0x096D, 0x55},
	{0x098B, 0x0F},
	{0x098C, 0x00},
	{0x098D, 0x12},
	{0x098E, 0x92},
	{0x098F, 0x00},
	{0x0990, 0x80},
	{0x0991, 0x01},
	{0x0992, 0x81},
	{0x0993, 0x1E},
	{0x0994, 0x9E},
	{0x09AD, 0x55},
	{0x09CB, 0x0F},
	{0x09CC, 0x00},
	{0x09CD, 0x12},
	{0x09CE, 0xD2},
	{0x09CF, 0x00},
	{0x09D0, 0xC0},
	{0x09D1, 0x01},
	{0x09D2, 0xC1},
	{0x09D3, 0x1E},
	{0x09D4, 0xDE},
	{0x09ED, 0x55},
	/* Double Mode Configuration */
	{0x0973, 0x10},
	/* MIPI D-PHY Configuration */
	{0x08A0, 0x04},
	{0x094A, 0xD0},
	{0x08A3, 0xE4},
	{0x08A5, 0x00},
	{0x0944, 0x81}, /* Enable Periodic Deskew */
	{0x0943, 0x07}, /* Disabled Auto Initial Deskew */
	{0x1D00, 0xF4},
	/*
	 * This is to set predefined (coarse) CSI output frequency
	 * CSI Phy 1 is 1500 Mbps/lane.
	 */
	{0x1D00, 0xF4},
	{0x0418, 0x2F},
	{0x1D00, 0xF5},
	{0x08A2, 0x34},
	{0x040B, 0x02},
};

static int max96724_configure_regmap(struct max96724_priv *priv, int region)
{
	int err;

	if (!priv->i2c_clients[region])
		return -ENODEV;

	priv->regmap[region] =
		devm_regmap_init_i2c(priv->i2c_clients[region],
				     &max96724_i2c_regmap[region]);

	if (IS_ERR(priv->regmap[region])) {
		err = PTR_ERR(priv->regmap[region]);
		dev_err(priv->dev,
			"Error initializing regmap %d with error %d\n",
			region, err);
		return -EINVAL;
	}

	return 0;
}

struct max96724_register_map {
	const char *name;
	u8 default_addr;
};

static const struct max96724_register_map max96724_default_addr[] = {
	[REGION_DES] = { "des", 0x27 },
	[REGION_GMSL_A] = { "gmsl_a", 0x62 },
	[REGION_GMSL_B] = { "gmsl_b", 0x63 },
	[REGION_GMSL_C] = { "gmsl_c", 0x64 },
	[REGION_GMSL_D] = { "gmsl_d", 0x65 },
};

static int max96724_read_check(struct max96724_priv *priv,
			      unsigned int region, unsigned int reg)
{
	struct i2c_client *client = priv->i2c_clients[region];
	int err;
	unsigned int val;

	err = regmap_read(priv->regmap[region], reg, &val);

	if (err) {
		dev_err(priv->dev, "error reading %02x, %02x\n",
				client->addr, reg);
		return err;
	}

	return val;
}

static int __maybe_unused max96724_read(struct max96724_priv *priv, unsigned int region, unsigned int reg)
{
	return max96724_read_check(priv, region, reg);
}

static int max96724_write(struct max96724_priv *priv, unsigned int region,
		unsigned int reg, unsigned int value)
{
	return regmap_write(priv->regmap[region], reg, value);
}

static void max96724_unregister_clients(struct max96724_priv *priv)
{
	unsigned int i;

	for (i = 1; i < ARRAY_SIZE(priv->i2c_clients); ++i)
		i2c_unregister_device(priv->i2c_clients[i]);
}

static int max96724_init_clients(struct max96724_priv *priv)
{
	unsigned int i;
	int ret;

	for (i = REGION_GMSL_A; i < REGION_MAX; ++i) {
		priv->i2c_clients[i] = i2c_new_ancillary_device(
				priv->client,
				max96724_default_addr[i].name,
				max96724_default_addr[i].default_addr);

		if (IS_ERR(priv->i2c_clients[i])) {
			dev_err(priv->dev, "failed to create i2c client %u\n", i);
			return PTR_ERR(priv->i2c_clients[i]);
		}

		ret = max96724_configure_regmap(priv, i);
		if (ret)
			return ret;
	}

	return 0;
}

static void max96724_csi_output(struct v4l2_subdev *sd, u8 enable)
{
	struct max96724_priv *priv = to_max96724(sd);
	u8 val;

	if (enable)
		val = 0x02;
	else
		val = 0x00;

	max96724_write(priv, REGION_DES, REG_BACKTOP12, val);
}

static int max96724_s_power(struct v4l2_subdev *sd, int on)
{
	struct max96724_priv *priv = to_max96724(sd);

	dev_info(priv->dev, "s_power %s", on ? "on":"off");

	if (priv->rst_gpio == NULL) {
		dev_info(priv->dev, "skip rest control");
		goto exit;
	}

	/* power on -> don't rest */
	gpiod_set_value_cansleep(priv->rst_gpio, on ? 0:1);

exit:
	return 0;
}

static int max96724_cfg_gmsl_addr(struct v4l2_subdev *sd)
{
	struct max96724_priv *priv = to_max96724(sd);

	/* Single Link Initialization Before Serializer Device Address Change */
	max96724_write(priv, REGION_DES, REG_3, 0xFB);
	usleep_range(5, 6);
	/* GMSL-B Serializer Address Change from 0xC4 to 0xC6 */
	max96724_write(priv, REGION_GMSL_A, 0x0000, 0xC6); /* (DEV_ADDR): 0x63 */

	max96724_write(priv, REGION_DES, REG_3, 0xEF);
	usleep_range(5, 6);
	/* GMSL-C Serializer Address Change from 0xC4 to 0xC8 */
	max96724_write(priv, REGION_GMSL_A, 0x0000, 0xC8); /* (DEV_ADDR): 0x64 */

	max96724_write(priv, REGION_DES, REG_3, 0xBF);
	usleep_range(5, 6);
	/* GMSL-D Serializer Address Change from 0xC4 to 0xCA */
	max96724_write(priv, REGION_GMSL_A, 0x0000, 0xCA); /* (DEV_ADDR): 0x65 */

	return 0;
}

static int max96724_set_gmsl_dphy(struct v4l2_subdev *sd, u32 region)
{
	struct max96724_priv *priv = to_max96724(sd);

	if ((region != REGION_GMSL_A) && (region != REGION_GMSL_B) &&
		(region != REGION_GMSL_C) && (region != REGION_GMSL_D))
		return -ENODEV;

	/* MIPI D-PHY Configuration */
	max96724_write(priv, region, 0x0330, 0x00);
	max96724_write(priv, region, 0x0331, 0x33);
	max96724_write(priv, region, 0x0332, 0xE0);
	max96724_write(priv, region, 0x0333, 0x04);
	max96724_write(priv, region, 0x0334, 0x00);
	max96724_write(priv, region, 0x0335, 0x00);

	/* Controller to Pipe Mapping Configuration */
	max96724_write(priv, region, 0x0308, 0x7D);
	max96724_write(priv, region, 0x0311, 0x15);

	/* Double Mode Configuration */
	max96724_write(priv, region, 0x0312, 0x01);
	max96724_write(priv, region, 0x031C, 0x30);

	/* Pipe Configuration */
	max96724_write(priv, region, 0x0053, 0x10);

	return 0;
}

static int max96724_set_gmsl_io(struct v4l2_subdev *sd, u32 region)
{
	struct max96724_priv *priv = to_max96724(sd);

	if ((region != REGION_GMSL_A) && (region != REGION_GMSL_B) &&
		(region != REGION_GMSL_C) && (region != REGION_GMSL_D))
		return -ENODEV;

	usleep_range(600, 610);

	max96724_write(priv, region, 0x02BF, 0x60);
	max96724_write(priv, region, 0x02BE, 0x80);
	max96724_write(priv, region, 0x02D7, 0x60);
	max96724_write(priv, region, 0x02D6, 0x80);

	usleep_range(1200, 1300);

	max96724_write(priv, region, 0x02BE, 0x90);

	return 0;
}

static void max96724_dump_lock_state(struct v4l2_subdev *sd)
{
	struct max96724_priv *priv = to_max96724(sd);
	int val_1, val_2, val_3, val_4;

	val_1 = max96724_read(priv, REGION_GMSL_A, 0x0102);
	val_2 = max96724_read(priv, REGION_GMSL_B, 0x0102);
	val_3 = max96724_read(priv, REGION_GMSL_C, 0x0102);
	val_4 = max96724_read(priv, REGION_GMSL_D, 0x0102);

	if ((val_1 >= 0) && (val_2 >= 0) && (val_3 >= 0) && (val_4 >= 0))
		dev_info(priv->dev, "VIDEO_TX2 0x%02x 0x%02x 0x%02x 0x%02x",
			val_1, val_2, val_3, val_4);
	else
		dev_err(priv->dev, "Read VIDEO_TX2 failed, %d %d %d %d",
			val_1, val_2, val_3, val_4);

	val_1 = max96724_read(priv, REGION_DES, REG_VIDEO_RX8_A);
	val_2 = max96724_read(priv, REGION_DES, REG_VIDEO_RX8_B);
	val_3 = max96724_read(priv, REGION_DES, REG_VIDEO_RX8_C);
	val_4 = max96724_read(priv, REGION_DES, REG_VIDEO_RX8_D);

	if ((val_1 >= 0) && (val_2 >= 0) && (val_3 >= 0) && (val_4 >= 0))
		dev_info(priv->dev, "VIDEO_RX8 0x%02x 0x%02x 0x%02x 0x%02x",
			val_1, val_2, val_3, val_4);
	else
		dev_err(priv->dev, "Read VIDEO_RX8 failed, %d %d %d %d",
			val_1, val_2, val_3, val_4);

	val_1 = max96724_read(priv, REGION_DES, REG_PHY25);
	val_2 = max96724_read(priv, REGION_DES, REG_PHY26);
	val_3 = max96724_read(priv, REGION_DES, REG_PHY27);
	val_4 = max96724_read(priv, REGION_DES, REG_PHY28);

	if ((val_1 >= 0) && (val_2 >= 0) && (val_3 >= 0) && (val_4 >= 0)) {
		dev_info(priv->dev, "tx_pkt_cnt %u %u %u %u",
			PHY25_get_csi2_tx0_pkt_cnt(val_1),
			PHY25_get_csi2_tx1_pkt_cnt(val_1),
			PHY26_get_csi2_tx2_pkt_cnt(val_2),
			PHY26_get_csi2_tx3_pkt_cnt(val_2));

		dev_info(priv->dev, "phy_pkt_cnt %u %u %u %u",
			PHY27_get_phy0_pkt_cnt(val_3),
			PHY27_get_phy1_pkt_cnt(val_3),
			PHY28_get_phy2_pkt_cnt(val_4),
			PHY28_get_phy3_pkt_cnt(val_4));
	} else {
		dev_err(priv->dev, "Read pkt_cnt failed, %d %d %d %d",
			val_1, val_2, val_3, val_4);
	}

}

static int max96724_format_cfg(struct v4l2_subdev *sd)
{
	struct max96724_priv *priv = to_max96724(sd);
	unsigned long timer;
	u32 time_ms;
	u32 region;
	u32 i;

	timer = jiffies;

	max96724_csi_output(sd, 0);
	max96724_cfg_gmsl_addr(sd);

	/* Link Initialization for Deserializer */
	max96724_write(priv, REGION_DES, REG_6, 0xFF);
	max96724_write(priv, REGION_DES, REG_3, 0xAA);

	/* Video Transmit Configuration for Serializer(s), (VID_TX_EN_X): Disabled */
	for (region = REGION_GMSL_A; region <= REGION_GMSL_D; region++)
		max96724_write(priv, region, 0x0002, 0x43);

	for (region = REGION_GMSL_A; region <= REGION_GMSL_D; region++)
		max96724_set_gmsl_dphy(sd, region);

	if (priv->en_deskew)
		for (i = 0; i < ARRAY_SIZE(des_cfg_table_deskew); i++)
			max96724_write(priv, REGION_DES,
				des_cfg_table_deskew[i].addr, des_cfg_table_deskew[i].val);
	else
		for (i = 0; i < ARRAY_SIZE(des_cfg_table); i++)
			max96724_write(priv, REGION_DES,
				des_cfg_table[i].addr, des_cfg_table[i].val);

	for (region = REGION_GMSL_A; region <= REGION_GMSL_D; region++)
		max96724_set_gmsl_io(sd, region);

	/* Video Transmit Configuration for Serializer(s), (VID_TX_EN_X): Enabled */
	for (region = REGION_GMSL_A; region <= REGION_GMSL_D; region++)
		max96724_write(priv, region, 0x0002, 0x53);

	time_ms = jiffies_to_msecs(jiffies - timer);

	dev_info(priv->dev, "%s done, cost %u ms", __func__, time_ms);

	msleep(1000);
	for (i = 0; i < 5; i++) {
		max96724_dump_lock_state(sd);
		msleep(100);
	}

	return 0;
}

static int max96724_format_set(void *data, u64 val)
{
	struct max96724_priv *priv = data;
	struct v4l2_subdev *sd = &priv->sd;
	int ret;

	ret = max96724_format_cfg(sd);

	return ret;
}

static int max96724_format_get(void *data, u64 *val)
{

	*val = 1;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(max96724_dbg_format_fops,
		max96724_format_get, max96724_format_set, "%llu\n");

static void max96724_setup_dbgfs(struct max96724_priv *priv)
{
	struct max96724_debug *debug = &priv->debug;
	struct v4l2_subdev *sd = &priv->sd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	char name[16];

	snprintf(name, 16, "max96724_%02x", client->addr);
	debug->debugfs_dir = debugfs_create_dir(name, NULL);

	if (IS_ERR_OR_NULL(debug->debugfs_dir)) {
		dev_info(priv->dev, "DebugFS unsupported\n");
		return;
	}

	debugfs_create_file("format", 0644, debug->debugfs_dir, priv,
			&max96724_dbg_format_fops);
}

static int max96724_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state, struct v4l2_subdev_format *format)
{
	struct max96724_priv *priv = to_max96724(sd);

	format->format.width = 1920;
	format->format.height = 1080;

	dev_dbg(priv->dev, "%s %ux%u\n", __func__,
		format->format.width, format->format.height);

	return 0;
}

static int max96724_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state, struct v4l2_subdev_format *format)
{
	struct max96724_priv *priv = to_max96724(sd);
	int ret;

	ret = max96724_format_cfg(sd);

	dev_dbg(priv->dev, "%s, ret=%d\n", __func__, ret);

	return ret;
}

static const struct v4l2_subdev_core_ops max96724_core_ops = {
	.s_power = max96724_s_power,
};

static const struct v4l2_subdev_pad_ops max96724_pad_ops = {
	/* VIDIOC_SUBDEV_G_FMT handler */
	.get_fmt = max96724_get_fmt,
	/* VIDIOC_SUBDEV_S_FMT handler */
	.set_fmt = max96724_set_fmt,
};

static const struct v4l2_subdev_ops max96724_subdev_ops = {
	.core = &max96724_core_ops,
	.pad = &max96724_pad_ops,
};

static int max96724_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct max96724_priv *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_ret;
	}

	priv->dev = dev;
	priv->client = client;
	priv->i2c_clients[REGION_DES] = client;

	priv->rst_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->rst_gpio)) {
		dev_info(dev, "reset gpio doesn't exist\n");
		priv->rst_gpio = NULL;
	} else {
		dev_info(dev, "Reset gpio=%d\n", desc_to_gpio(priv->rst_gpio));
	}

	ret = max96724_configure_regmap(priv, REGION_DES);
	if (ret) {
		dev_err(dev, "Failed to setup main client regmap, ret=%d", ret);
		goto err_cleanup_clients;
	}

	ret = max96724_init_clients(priv);
	if (ret) {
		dev_err(dev, "Failed to setup clients regmap, ret=%d", ret);
		goto err_cleanup_clients;
	}

	ret = of_property_read_u32(dev->of_node, "en-deskew",
				&priv->en_deskew);
	if (ret < 0 || priv->en_deskew > 1)
		priv->en_deskew = 0;

	dev_info(dev, "en_deskew=%u\n", priv->en_deskew);

	ret = v4l2_device_register(priv->dev, &priv->v4l2_dev);
	if (ret) {
		dev_err(priv->dev, "Failed to register v4l2 device, ret=%d\n", ret);
		goto err_cleanup_clients;
	}

	v4l2_i2c_subdev_init(&priv->sd, client, &max96724_subdev_ops);
	priv->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	ret = v4l2_device_register_subdev(&priv->v4l2_dev, &priv->sd);
	if (ret) {
		dev_err(priv->dev, "Failed to register v4l2 subdev, ret=%d\n", ret);
		goto err_cleanup_clients;
	}

	ret = v4l2_device_register_subdev_nodes(&priv->v4l2_dev);
	if (ret) {
		dev_err(priv->dev, "Failed to register v4l2 subdev node, ret=%d\n", ret);
		goto err_cleanup_clients;
	}

	ret = max96724_s_power(&priv->sd, true);
	if (ret < 0) {
		dev_err(dev, "power up failed\n");
		goto err_cleanup_clients;
	}

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		max96724_setup_dbgfs(priv);

	return 0;

err_cleanup_clients:
	max96724_unregister_clients(priv);
	if (priv->rst_gpio)
		devm_gpiod_put(dev, priv->rst_gpio);
err_ret:
	return ret;
}

static int max96724_remove(struct i2c_client *client)
{

	return 0;
}

static const struct of_device_id max96724_of_match[] = {
	{ .compatible = "adi,max96724" },
	{ }
};

MODULE_DEVICE_TABLE(of, max96724_of_match);

static struct i2c_driver max96724_driver = {
	.probe_new = max96724_probe,
	.remove = max96724_remove,
	.driver = {
		.name = "max96724",
		.of_match_table = max96724_of_match,
	},
};

module_i2c_driver(max96724_driver);

MODULE_DESCRIPTION("Maxim MAX96724 Quad GMSL2/1 Deserializer Driver");
MODULE_AUTHOR("Chase Yen <chase.yen@realtek.com>");
MODULE_LICENSE("GPL v2");
