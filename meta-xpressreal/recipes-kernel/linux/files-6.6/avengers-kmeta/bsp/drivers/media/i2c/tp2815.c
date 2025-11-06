// SPDX-License-Identifier: GPL-2.0-only
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-common.h>
#include <media/v4l2-subdev.h>

#define PAGE_SEL        0x40

enum {
	CH_1 = 0,
	CH_2 = 1,
	CH_3 = 2,
	CH_4 = 3,
	CH_ALL = 4,
	MIPI_PAGE = 8,
};

struct regval {
	u8 addr;
	u8 val;
};

struct tp2815_debug {
	struct dentry *debugfs_dir;
	bool en_blue_pattern;
};

struct tp2815 {
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct v4l2_subdev sd;
	struct i2c_client *i2c_client;
	struct gpio_desc *rst_gpio;

	struct tp2815_debug debug;
	bool pll_locked[CH_ALL];
};

#define to_tp2815(x) container_of(x, struct tp2815, x)


/* STD_HDA FHD30 4CH4LANE */
static const struct regval std_hda_fhd30[] = {
	{PAGE_SEL, CH_ALL},
	{0x45, 0x01},
	{0x06, 0x12},
	{0x27, 0x2D},
	{0xF5, 0xF0},
	{0x02, 0x40},
	{0x07, 0xC0},
	{0x0B, 0xC0},
	{0x0C, 0x03},
	{0x0D, 0x50},
	{0x15, 0x03},
	{0x16, 0xD2},
	{0x17, 0x80},
	{0x18, 0x29},
	{0x19, 0x38},
	{0x1A, 0x47},
	{0x1C, 0x08},
	{0x1D, 0x98},
	{0x20, 0x30},
	{0x21, 0x84},
	{0x22, 0x36},
	{0x23, 0x3C},
	{0x2B, 0x60},
	{0x2C, 0x2A},
	{0x2D, 0x30},
	{0x2E, 0x70},
	{0x30, 0x48},
	{0x31, 0xBB},
	{0x32, 0x2E},
	{0x33, 0x90},
	{0x35, 0x05},
	{0x38, 0x00},
	{0x39, 0x1C},
	{0x02, 0x44},
	{0x0D, 0x72},
	{0x15, 0x01},
	{0x16, 0xF0},
	{0x18, 0x2A},
	{0x20, 0x38},
	{0x21, 0x46},
	{0x25, 0xFE},
	{0x26, 0x0D},
	{0x2C, 0x3A},
	{0x2D, 0x54},
	{0x2E, 0x40},
	{0x30, 0xA5},
	{0x31, 0x95},
	{0x32, 0xE0},
	{0x33, 0x60},
	{PAGE_SEL, MIPI_PAGE},
	{0x01, 0xF0},
	{0x02, 0x01},
	{0x08, 0x0F},
	{0x20, 0x44},
	{0x34, 0xE4},
	{0x15, 0x0C},
	{0x25, 0x08},
	{0x26, 0x06},
	{0x27, 0x11},
	{0x29, 0x0A},
	{0x33, 0x0F},
	{0x33, 0x00},
	{0x14, 0x33},
	{0x14, 0xB3},
	{0x14, 0x33},
	{0x23, 0x02},
	{0x23, 0x00},
	{0x40, 0x04}
};

static int tp2815_write_reg(struct v4l2_subdev *sd, u8 reg, u8 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return i2c_smbus_write_byte_data(client, reg, value);
}

static int tp2815_read_reg(struct v4l2_subdev *sd, u8 reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return i2c_smbus_read_byte_data(client, reg);
}

static bool tp2815_pll_det(struct v4l2_subdev *sd, u8 ch_sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tp2815 *tp2815 = to_tp2815(sd);
	bool pll_locked = false;
	u8 video_input_status;

	if (ch_sel > CH_4)
		goto exit;

	if (tp2815_read_reg(sd, PAGE_SEL) != ch_sel)
		i2c_smbus_write_byte_data(client, PAGE_SEL, ch_sel);

	video_input_status = tp2815_read_reg(sd, 0x1);

	if ((video_input_status & 0xF0) == 0x70)
		pll_locked = true;
	else
		dev_info(tp2815->dev, "Ch%u video_input_status=0x%02x",
			ch_sel, video_input_status);

exit:
	return pll_locked;
}

static u8 tp2815_std_det(struct v4l2_subdev *sd, u8 ch_sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 cvstd = 0;

	if (ch_sel > CH_4)
		goto exit;

	if (tp2815_read_reg(sd, PAGE_SEL) != ch_sel)
		i2c_smbus_write_byte_data(client, PAGE_SEL, ch_sel);

	cvstd = tp2815_read_reg(sd, 0x3) & 0x7;

exit:
	return cvstd;
}

static int tp2815_s_power(struct v4l2_subdev *sd, int on)
{
	struct tp2815 *tp2815 = to_tp2815(sd);

	dev_info(tp2815->dev, "s_power %s", on ? "on":"off");

	if (tp2815->rst_gpio == NULL) {
		dev_info(tp2815->dev, "skip rest control");
		goto exit;
	}

	/* power on -> don't rest */
	gpiod_set_value_cansleep(tp2815->rst_gpio, on ? 0:1);

exit:
	return 0;
}

static bool tp2815_signal_detect(struct v4l2_subdev *sd)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	int i;
	bool detected;

	for (i = CH_1; i < CH_ALL; i++)
		tp2815->pll_locked[i] = tp2815_pll_det(sd, i);

	dev_info(tp2815->dev, "pll_locked %u %u %u %u",
		tp2815->pll_locked[CH_1],
		tp2815->pll_locked[CH_2],
		tp2815->pll_locked[CH_3],
		tp2815->pll_locked[CH_4]);

	for (i = CH_1; i < CH_ALL; i++)
		if (tp2815->pll_locked[i])
			dev_info(tp2815->dev, "Ch%u CVSTD=%u", i, tp2815_std_det(sd, i));

	detected = tp2815->pll_locked[CH_1] | tp2815->pll_locked[CH_2] |
		tp2815->pll_locked[CH_3] | tp2815->pll_locked[CH_4];

	return detected;
}

static int tp2815_1080p30_init_cfg(struct v4l2_subdev *sd)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	unsigned long timer;
	u32 time_ms;
	int i;

	timer = jiffies;

	for (i = 0; i < ARRAY_SIZE(std_hda_fhd30); i++)
		tp2815_write_reg(sd, std_hda_fhd30[i].addr, std_hda_fhd30[i].val);

	time_ms = jiffies_to_msecs(jiffies - timer);

	dev_info(tp2815->dev, "%s done, cost %u ms", __func__, time_ms);

	return 0;
}

static int tp2815_format_cfg(struct v4l2_subdev *sd)
{
	int ret;
	int i;
	bool detected;

	ret = tp2815_1080p30_init_cfg(sd);

	detected = tp2815_signal_detect(sd);

	for (i = 0; i < 3; i++) {
		detected = tp2815_signal_detect(sd);

		if (detected)
			break;

		msleep(500);
	}

	return ret;
}

static int en_blue_pattern_set(void *data, u64 val)
{
	struct tp2815 *tp2815 = data;
	struct v4l2_subdev *sd = &tp2815->sd;

	if (val) {
		tp2815_write_reg(sd, PAGE_SEL, CH_ALL);
		tp2815_write_reg(sd, 0x2A, 0x3C);
	} else {
		tp2815_write_reg(sd, PAGE_SEL, CH_ALL);
		tp2815_write_reg(sd, 0x2A, 0x30);
	}

	tp2815->debug.en_blue_pattern = val ? true:false;

	return 0;
}

static int en_blue_pattern_get(void *data, u64 *val)
{
	struct tp2815 *tp2815 = data;

	*val = (u64)tp2815->debug.en_blue_pattern;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(tp2815_dbg_pattern_fops, en_blue_pattern_get, en_blue_pattern_set,
			 "%llu\n");

static int tp2815_format_set(void *data, u64 val)
{
	struct tp2815 *tp2815 = data;
	struct v4l2_subdev *sd = &tp2815->sd;
	int ret;

	ret = tp2815_format_cfg(sd);

	return ret;
}

static int tp2815_format_get(void *data, u64 *val)
{

	*val = 1;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(tp2815_dbg_format_fops, tp2815_format_get, tp2815_format_set,
			 "%llu\n");

static void tp2815_setup_dbgfs(struct tp2815 *tp2815)
{
	struct tp2815_debug *debug = &tp2815->debug;
	struct v4l2_subdev *sd = &tp2815->sd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	char name[16];

	snprintf(name, 16, "tp2815_%02x", client->addr);
	debug->debugfs_dir = debugfs_create_dir(name, NULL);

	if (IS_ERR_OR_NULL(debug->debugfs_dir)) {
		dev_info(tp2815->dev, "DebugFS unsupported\n");
		return;
	}

	debugfs_create_file("en_blue_pattern", 0644, debug->debugfs_dir, tp2815,
			&tp2815_dbg_pattern_fops);
	debugfs_create_file("format", 0644, debug->debugfs_dir, tp2815,
			&tp2815_dbg_format_fops);
}

static int tp2815_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state, struct v4l2_subdev_format *format)
{
	struct tp2815 *tp2815 = to_tp2815(sd);

	format->format.width = 1920;
	format->format.height = 1080;

	dev_dbg(tp2815->dev, "%s %ux%u\n", __func__,
		format->format.width, format->format.height);

	return 0;
}

static int tp2815_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state, struct v4l2_subdev_format *format)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	int ret;

	ret = tp2815_format_cfg(sd);

	dev_dbg(tp2815->dev, "%s, ret=%d\n", __func__, ret);

	return ret;
}

static const struct v4l2_subdev_core_ops tp2815_core_ops = {
	.s_power = tp2815_s_power,
};

static const struct v4l2_subdev_pad_ops tp2815_pad_ops = {
	/* VIDIOC_SUBDEV_G_FMT handler */
	.get_fmt = tp2815_get_fmt,
	/* VIDIOC_SUBDEV_S_FMT handler */
	.set_fmt = tp2815_set_fmt,
};

static const struct v4l2_subdev_ops tp2815_subdev_ops = {
	.core = &tp2815_core_ops,
	.pad = &tp2815_pad_ops,
};

static int tp2815_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct tp2815 *tp2815;
	int ret;

	tp2815 = devm_kzalloc(dev, sizeof(*tp2815), GFP_KERNEL);
	if (!tp2815)
		return -ENOMEM;

	tp2815->i2c_client = client;
	tp2815->dev = dev;

	tp2815->rst_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(tp2815->rst_gpio)) {
		dev_err(dev, "reset gpio doesn't exist\n");
		tp2815->rst_gpio = NULL;
	} else {
		dev_info(dev, "Reset gpio=%d\n", desc_to_gpio(tp2815->rst_gpio));
	}

	ret = v4l2_device_register(tp2815->dev, &tp2815->v4l2_dev);
	if (ret) {
		dev_err(tp2815->dev, "Failed to register v4l2 device, ret=%d\n", ret);
		goto free_gpio;
	}

	v4l2_i2c_subdev_init(&tp2815->sd, client, &tp2815_subdev_ops);
	tp2815->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	ret = v4l2_device_register_subdev(&tp2815->v4l2_dev, &tp2815->sd);
	if (ret) {
		dev_err(tp2815->dev, "Failed to register v4l2 subdev, ret=%d\n", ret);
		goto free_gpio;
	}

	ret = v4l2_device_register_subdev_nodes(&tp2815->v4l2_dev);
	if (ret) {
		dev_err(tp2815->dev, "Failed to register v4l2 subdev node, ret=%d\n", ret);
		goto free_gpio;
	}

	ret = tp2815_s_power(&tp2815->sd, true);
	if (ret < 0) {
		dev_err(dev, "power up failed\n");
		goto free_gpio;
	}

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		tp2815_setup_dbgfs(tp2815);

	return 0;

free_gpio:
	if (tp2815->rst_gpio)
		devm_gpiod_put(dev, tp2815->rst_gpio);

	return ret;
}

static int tp2815_remove(struct i2c_client *client)
{

	return 0;
}

static const struct of_device_id tp2815_of_match[] = {
	{ .compatible = "techpoint,tp2815" },
	{ }
};

MODULE_DEVICE_TABLE(of, tp2815_of_match);

static struct i2c_driver tp2815_driver = {
	.probe_new = tp2815_probe,
	.remove = tp2815_remove,
	.driver = {
		.name = "tp2815",
		.of_match_table = tp2815_of_match,
	},
};

module_i2c_driver(tp2815_driver);

MODULE_DESCRIPTION("Techpoint TP2815 Driver");
MODULE_AUTHOR("Chase Yen <chase.yen@realtek.com>");
MODULE_LICENSE("GPL v2");
