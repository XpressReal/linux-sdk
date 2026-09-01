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

#define BT1120_HEADER_8BIT  0x00 /* reg0x02 bit3 0=BT1120 */
#define BT656_HEADER_8BIT   0x08 /* reg0x02 bit3 1=656 */
#define SAV_HEADER_1MUX     BT656_HEADER_8BIT

enum {
	CH_1 = 0,
	CH_2 = 1,
	CH_3 = 2,
	CH_4 = 3,
	CH_ALL = 4,
	MIPI_PAGE = 8,
};

enum {
	SDR_1CH, /* 148.5M mode */
	SDR_2CH, /* 148.5M mode */
	DDR_2CH, /* 297M mode, support from TP2822/23 */
	DDR_4CH, /* 297M mode, support from TP2824/33 */
	DDR_1CH, /* 297M mode, support from TP2827 */
	SDR_4CH_16BIT, /* from TP2836/TP2837 */
	DDR_4CH_16BIT, /* from TP2836/TP2837 */
};

enum {
	TP9937_720P25V2 = 0x0D,
	TP9937_720P30V2 = 0x0C,
	TP9937_PAL      = 0x08,
	TP9937_NTSC     = 0x09,
};

enum {
	STD_TVI,
	STD_HDA,
	STD_HDC,
	STD_HDA_DEFAULT,
	STD_HDC_DEFAULT
};

enum {
	PTZ_RX_TVI_CMD,
	PTZ_RX_TVI_BURST,
	PTZ_RX_ACP1,
	PTZ_RX_ACP2,
	PTZ_RX_ACP3,
	PTZ_RX_TEST,
	PTZ_RX_HDC1,
	PTZ_RX_HDC2
};

struct regval {
	u8 addr;
	u8 val;
};

struct tp9937_debug {
	struct dentry *debugfs_dir;
	bool en_blue_pattern;
	bool en_dump_i2c;
};

struct tp9937 {
	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct v4l2_subdev sd;
	struct i2c_client *i2c_client;
	struct gpio_desc *rst_gpio;

	bool initized;
	u8 fmt_mode;
	u8 video_mode;
	u8 std;

	struct tp9937_debug debug;
	bool pll_locked[CH_ALL];
};

#define to_tp9937(x) container_of(x, struct tp9937, x)

/* For reg 0xF5 */
static const u8 sys_mode[5] = {
	/* CH_1, CH_2, CH_3, CH_4, CH_ALL */
	0x01, 0x02, 0x04, 0x08, 0x0F
};

static const u8 e8_mode[5] = {
	0x01, 0x04, 0x10, 0x40, 0x55
};

static const u8 clk_addr[4] = {
	0xFA, 0xFA, 0xFB, 0xFB
};

static const u8 clk_and[4] = {
	0xF8, 0x8F, 0xF8, 0x8F
};

static const u8 dat_addr[4] = {
	0xF6, 0xF7, 0xF8, 0xF9
};

static const u8 sdr1_sel[4] = {
	0x00, 0x11, 0x22, 0x33
};

static const u8 tp2836_ddr4ch_mux[5] = {
	0x11, 0x22, 0x44, 0x88, 0xFF
};

static const u8 tp2837_clk_v2[4] = {
	0x04, 0x40, 0x04, 0x40
};

/* For reg 0x47 and reg 0x49 */
static const u8 ddr_2ch_mux[5] = {
	0x01, 0x02, 0x40, 0x80, 0xC3
};

static const u8 tbl_tp2802_PAL_raster[] = {
	/* Start address 0x15 */
	0x13, 0x5F, 0xBC, 0x17, 0x20, 0x17, 0x00, 0x09, 0x48
};

static const u8 tbl_tp2802_NTSC_raster[] = {
	/* Start address 0x15 */
	0x13, 0x4E, 0xBC, 0x15, 0xf0, 0x07, 0x00, 0x09, 0x38
};

static const u8 tbl_tp2802_720p50_raster[] = {
	/* Start address 0x15 */
	0x13, 0x16, 0x00, 0x19, 0xD0, 0x25, 0x00, 0x07, 0xBC
};

static const u8 tbl_tp2802_720p60_raster[] = {
	/* Start address 0x15 */
	0x13, 0x16, 0x00, 0x19, 0xD0, 0x25, 0x00, 0x06, 0x72
};

/* Reg C9~D7 */
static const unsigned char ptz_rx_dat[][15] = {
	/* TVI command */
	{0x00, 0x00, 0x07, 0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x60, 0x10, 0x06, 0xBE, 0x39, 0x27},
	/* TVI burst */
	{0x00, 0x00, 0x07, 0x08, 0x09, 0x0a, 0x04, 0x00, 0x00, 0x60, 0x10, 0x06, 0xBE, 0x39, 0x27},
	/* ACP1 0.525, 720p/1080p */
	{0x00, 0x00, 0x06, 0x07, 0x08, 0x09, 0x05, 0xbf, 0x11, 0x60, 0x50, 0x04, 0xB0, 0xD8, 0x07},
	/* ACP2 0.6, QHD15_5M12.5 */
	{0x00, 0x00, 0x07, 0x08, 0x09, 0x0a, 0x03, 0x48, 0x9b, 0x60, 0x50, 0x04, 0xB0, 0xD8, 0x07},
	/* ACP3 0.3, QHD25/30_5M20_8M15 */
	{0x00, 0x00, 0x07, 0x08, 0x09, 0x0a, 0x04, 0xc9, 0xe3, 0x60, 0x28, 0x04, 0xB0, 0xD8, 0x07},
	/* ACP1 0.525 */
	{0x00, 0x00, 0x06, 0x07, 0x08, 0x09, 0x03, 0x52, 0x53, 0x60, 0x10, 0x04, 0xF0, 0xD8, 0x17}
};

static int tp9937_write_reg(struct v4l2_subdev *sd, u8 reg, u8 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "Write 0x%02x = 0x%02x\n", reg, value);

	return i2c_smbus_write_byte_data(client, reg, value);
}

static int tp9937_read_reg(struct v4l2_subdev *sd, u8 reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	int reg_val;

	reg_val = i2c_smbus_read_byte_data(client, reg);

	if (debug->en_dump_i2c && (reg_val >= 0))
		dev_info(tp9937->dev, "Read 0x%02x = 0x%02x\n", reg, (u8)reg_val);

	return reg_val;
}

static void __maybe_unused tp2831_rx_init(struct v4l2_subdev *sd, u8 mod)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	int i, index = 0;
	u8 val_a7 = 0x00;
	u8 val_a8 = 0x00;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin, mod=%u\n", __func__, mod);

	switch (mod) {
	case PTZ_RX_TVI_CMD:
		index = 0;
		val_a7 = 0x03;
		val_a8 = 0x00;
		break;
	case PTZ_RX_TVI_BURST:
		index = 1;
		val_a7 = 0x03;
		val_a8 = 0x00;
		break;
	case PTZ_RX_ACP1:
		index = 2;
		val_a7 = 0x27;
		val_a8 = 0x00;
		break;
	case PTZ_RX_ACP2:
		index = 3;
		val_a7 = 027;
		val_a8 = 0x00;
		break;
	case PTZ_RX_ACP3:
		index = 4;
		val_a7 = 0x27;
		val_a8 = 0x00;
		break;
	case PTZ_RX_TEST:
		index = 5;
		val_a7 = 0x03;
		val_a8 = 0x00;
		break;
	}

	for (i = 0; i < 15; i++) {
		tp9937_write_reg(sd, 0xC9+i, ptz_rx_dat[index][i]);
		tp9937_write_reg(sd, 0xA8, val_a8);
		tp9937_write_reg(sd, 0xA7, val_a7);
	}

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2831_ntsc_dataset(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 tmp;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tp9937_write_reg(sd, 0x0B, 0xC0);
	tp9937_write_reg(sd, 0x0C, 0x13);
	tp9937_write_reg(sd, 0x0D, 0x50);

	tp9937_write_reg(sd, 0x20, 0x40);
	tp9937_write_reg(sd, 0x21, 0x84);
	tp9937_write_reg(sd, 0x22, 0x36);
	tp9937_write_reg(sd, 0x23, 0x3C);

	tp9937_write_reg(sd, 0x25, 0xFF);
	tp9937_write_reg(sd, 0x26, 0x05);
	tp9937_write_reg(sd, 0x27, 0x2D);
	tp9937_write_reg(sd, 0x28, 0x00);

	tp9937_write_reg(sd, 0x2B, 0x70);
	tp9937_write_reg(sd, 0x2C, 0x2A);
	tp9937_write_reg(sd, 0x2D, 0x68);
	tp9937_write_reg(sd, 0x2E, 0x57);

	tp9937_write_reg(sd, 0x30, 0x62);
	tp9937_write_reg(sd, 0x31, 0xBB);
	tp9937_write_reg(sd, 0x32, 0x96);
	tp9937_write_reg(sd, 0x33, 0xC0);
	tp9937_write_reg(sd, 0x38, 0x00);
	tp9937_write_reg(sd, 0x39, 0x04);
	tp9937_write_reg(sd, 0x3A, 0x32);
	tp9937_write_reg(sd, 0x3B, 0x25);

	tp9937_write_reg(sd, 0x18, 0x12);

	tp9937_write_reg(sd, 0x13, 0x00);
	tmp = tp9937_read_reg(sd, 0x14);
	tmp &= 0x9F;
	tp9937_write_reg(sd, 0x14, tmp);

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2831_pal_dataset(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 tmp;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tp9937_write_reg(sd, 0x0B, 0xC0);
	tp9937_write_reg(sd, 0x0C, 0x13);
	tp9937_write_reg(sd, 0x0D, 0x51);

	tp9937_write_reg(sd, 0x20, 0x48);
	tp9937_write_reg(sd, 0x21, 0x84);
	tp9937_write_reg(sd, 0x22, 0x37);
	tp9937_write_reg(sd, 0x23, 0x3F);

	tp9937_write_reg(sd, 0x25, 0xFF);
	tp9937_write_reg(sd, 0x26, 0x05);
	tp9937_write_reg(sd, 0x27, 0x2D);
	tp9937_write_reg(sd, 0x28, 0x00);

	tp9937_write_reg(sd, 0x2B, 0x70);
	tp9937_write_reg(sd, 0x2C, 0x2A);
	tp9937_write_reg(sd, 0x2D, 0x64);
	tp9937_write_reg(sd, 0x2E, 0x56);

	tp9937_write_reg(sd, 0x30, 0x7A);
	tp9937_write_reg(sd, 0x31, 0x4A);
	tp9937_write_reg(sd, 0x32, 0x4D);
	tp9937_write_reg(sd, 0x33, 0xF0);
	tp9937_write_reg(sd, 0x38, 0x00);
	tp9937_write_reg(sd, 0x39, 0x04);
	tp9937_write_reg(sd, 0x3A, 0x32);
	tp9937_write_reg(sd, 0x3B, 0x25);

	tp9937_write_reg(sd, 0x18, 0x17);

	tp9937_write_reg(sd, 0x13, 0x00);
	tmp = tp9937_read_reg(sd, 0x14);
	tmp &= 0x9f;
	tp9937_write_reg(sd, 0x14, tmp);

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2831_v2_dataset(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 tmp;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tp9937_write_reg(sd, 0x13, 0x00);
	tmp = tp9937_read_reg(sd, 0x14);
	tmp &= 0x9F;
	tp9937_write_reg(sd, 0x14, tmp);

	tp9937_write_reg(sd, 0x0B, 0xC0);
	tp9937_write_reg(sd, 0x0C, 0x13);
	tp9937_write_reg(sd, 0x0D, 0x50);

	tp9937_write_reg(sd, 0x20, 0x30);
	tp9937_write_reg(sd, 0x21, 0x84);
	tp9937_write_reg(sd, 0x22, 0x36);
	tp9937_write_reg(sd, 0x23, 0x3C);

	tp9937_write_reg(sd, 0x25, 0xFF);
	tp9937_write_reg(sd, 0x26, 0x05);
	tp9937_write_reg(sd, 0x27, 0x2D);
	tp9937_write_reg(sd, 0x28, 0x00);

	tp9937_write_reg(sd, 0x2B, 0x60);
	tp9937_write_reg(sd, 0x2C, 0x0A);
	tp9937_write_reg(sd, 0x2D, 0x30);
	tp9937_write_reg(sd, 0x2E, 0x70);

	tp9937_write_reg(sd, 0x30, 0x48);
	tp9937_write_reg(sd, 0x31, 0xBB);
	tp9937_write_reg(sd, 0x32, 0x2E);
	tp9937_write_reg(sd, 0x33, 0x90);
	tp9937_write_reg(sd, 0x38, 0x00);
	tp9937_write_reg(sd, 0x39, 0x18);
	tp9937_write_reg(sd, 0x3A, 0x32);
	tp9937_write_reg(sd, 0x3B, 0x25);

	tp9937_write_reg(sd, 0x80, 0x52);
	tp9937_write_reg(sd, 0x81, 0x10);
	tp9937_write_reg(sd, 0x82, 0x16);
	tp9937_write_reg(sd, 0x83, 0x6A);
	tp9937_write_reg(sd, 0x84, 0x14);
	tp9937_write_reg(sd, 0x88, 0x58);

	/* tp2831_rx_init(sd, PTZ_RX_TVI_CMD); */

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2831_a720p25_dataset(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 tmp;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tmp = tp9937_read_reg(sd, 0x02);
	tmp |= 0x04;
	tp9937_write_reg(sd, 0x02, tmp);

	tp9937_write_reg(sd, 0x0d, 0x71);

	tp9937_write_reg(sd, 0x18, 0x1B);

	tp9937_write_reg(sd, 0x20, 0x40);
	tp9937_write_reg(sd, 0x21, 0x46);
	tp9937_write_reg(sd, 0x22, 0x36);
	tp9937_write_reg(sd, 0x23, 0x3C);

	tp9937_write_reg(sd, 0x25, 0xFE);
	tp9937_write_reg(sd, 0x26, 0x01);
	tp9937_write_reg(sd, 0x27, 0x2D);
	tp9937_write_reg(sd, 0x28, 0x00);

	tp9937_write_reg(sd, 0x2b, 0x60);
	tp9937_write_reg(sd, 0x2c, 0x3A);
	tp9937_write_reg(sd, 0x2d, 0x5A);
	tp9937_write_reg(sd, 0x2e, 0x40);

	tp9937_write_reg(sd, 0x30, 0x9E);
	tp9937_write_reg(sd, 0x31, 0x20);
	tp9937_write_reg(sd, 0x32, 0x10);
	tp9937_write_reg(sd, 0x33, 0x90);

	tp9937_write_reg(sd, 0x38, 0x00);
	tp9937_write_reg(sd, 0x39, 0x18);
	tp9937_write_reg(sd, 0x3a, 0x32);
	tp9937_write_reg(sd, 0x3b, 0x25);

	/* tp2831_rx_init(sd, PTZ_RX_ACP1); */

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2826_c720p25_dataset(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tp9937_write_reg(sd, 0x13, 0x40);

	tp9937_write_reg(sd, 0x20, 0x3A);

	tp9937_write_reg(sd, 0x26, 0x01);
	tp9937_write_reg(sd, 0x27, 0x5A);
	tp9937_write_reg(sd, 0x28, 0x04);

	tp9937_write_reg(sd, 0x2B, 0x60);
	tp9937_write_reg(sd, 0x2D, 0x36);
	tp9937_write_reg(sd, 0x2E, 0x40);

	tp9937_write_reg(sd, 0x30, 0x48);
	tp9937_write_reg(sd, 0x31, 0x67);
	tp9937_write_reg(sd, 0x32, 0x6F);
	tp9937_write_reg(sd, 0x33, 0x33);

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2831_a720p30_dataset(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 tmp;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tmp = tp9937_read_reg(sd, 0x02);
	tmp |= 0x04;
	tp9937_write_reg(sd, 0x02, tmp);

	tp9937_write_reg(sd, 0x0D, 0x70);

	tp9937_write_reg(sd, 0x18, 0x1B);

	tp9937_write_reg(sd, 0x20, 0x40);
	tp9937_write_reg(sd, 0x21, 0x46);
	tp9937_write_reg(sd, 0x22, 0x36);
	tp9937_write_reg(sd, 0x23, 0x3C);

	tp9937_write_reg(sd, 0x25, 0xFE);
	tp9937_write_reg(sd, 0x26, 0x01);
	tp9937_write_reg(sd, 0x27, 0x2D);
	tp9937_write_reg(sd, 0x28, 0x00);

	tp9937_write_reg(sd, 0x2B, 0x60);
	tp9937_write_reg(sd, 0x2C, 0x3A);
	tp9937_write_reg(sd, 0x2D, 0x5A);
	tp9937_write_reg(sd, 0x2E, 0x40);

	tp9937_write_reg(sd, 0x30, 0x9D);
	tp9937_write_reg(sd, 0x31, 0xCA);
	tp9937_write_reg(sd, 0x32, 0x01);
	tp9937_write_reg(sd, 0x33, 0xD0);

	tp9937_write_reg(sd, 0x38, 0x00);
	tp9937_write_reg(sd, 0x39, 0x18);
	tp9937_write_reg(sd, 0x3A, 0x32);
	tp9937_write_reg(sd, 0x3B, 0x25);

	/* tp2831_rx_init(sd, PTZ_RX_ACP1); */

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2826_c720p30_dataset(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tp9937_write_reg(sd, 0x13, 0x40);

	tp9937_write_reg(sd, 0x20, 0x30);

	tp9937_write_reg(sd, 0x26, 0x01);
	tp9937_write_reg(sd, 0x27, 0x5A);
	tp9937_write_reg(sd, 0x28, 0x04);

	tp9937_write_reg(sd, 0x2B, 0x60);

	tp9937_write_reg(sd, 0x2D, 0x37);
	tp9937_write_reg(sd, 0x2E, 0x40);

	tp9937_write_reg(sd, 0x30, 0x48);
	tp9937_write_reg(sd, 0x31, 0x67);
	tp9937_write_reg(sd, 0x32, 0x6F);
	tp9937_write_reg(sd, 0x33, 0x30);

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2802_set_work_mode_720p50(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 i;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	for (i = 0; i < ARRAY_SIZE(tbl_tp2802_720p50_raster); i++) {
		u8 reg;

		reg = 0x15 + i;
		tp9937_write_reg(sd, reg, tbl_tp2802_720p50_raster[i]);
	}

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2802_set_work_mode_720p60(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 i;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	for (i = 0; i < ARRAY_SIZE(tbl_tp2802_720p60_raster); i++) {
		u8 reg;

		reg = 0x15 + i;
		tp9937_write_reg(sd, reg, tbl_tp2802_720p60_raster[i]);
	}

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2802_set_work_mode_PAL(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 i;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	for (i = 0; i < ARRAY_SIZE(tbl_tp2802_PAL_raster); i++) {
		u8 reg;

		reg = 0x15 + i;
		tp9937_write_reg(sd, reg, tbl_tp2802_PAL_raster[i]);
	}

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp2802_set_work_mode_NTSC(struct v4l2_subdev *sd)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 i;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	for (i = 0; i < ARRAY_SIZE(tbl_tp2802_NTSC_raster); i++) {
		u8 reg;

		reg = 0x15 + i;
		tp9937_write_reg(sd, reg, tbl_tp2802_NTSC_raster[i]);
	}

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp282x_sysclk_v2(struct v4l2_subdev *sd, u8 fmt_mode, u8 ch)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 tmp;

	if (ch > CH_ALL)
		return;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	switch (fmt_mode) {
	case SDR_2CH:
		/* Do nothing */
		break;
	case DDR_4CH:
		/* Do nothing */
		break;
	case DDR_2CH:
		tmp = tp9937_read_reg(sd, 0x46);
		tmp |= sys_mode[ch];
		tp9937_write_reg(sd, 0x46, tmp);

		tmp = tp9937_read_reg(sd, 0x47);
		tmp |= ddr_2ch_mux[ch];
		tp9937_write_reg(sd, 0x47, tmp);
		tmp = tp9937_read_reg(sd, 0x49);
		tmp |= ddr_2ch_mux[ch];
		tp9937_write_reg(sd, 0x49, tmp);

#if 0
		tmp = tp9937_read_reg(sd, 0xE8);
		tmp |= e8_mode[ch];
		tp9937_write_reg(sd, 0xE8, tmp);
#endif
		break;
	case DDR_1CH:
		fallthrough;
	case SDR_1CH:
		if (ch >= CH_ALL) {
			u8 i;

			for (i = 0; i < 4; i++) {
				tmp = tp9937_read_reg(sd, clk_addr[i]);
				tmp &= clk_and[i];
				tmp |= tp2837_clk_v2[i];
				tp9937_write_reg(sd, clk_addr[i], tmp);
				tp9937_write_reg(sd, dat_addr[i], sdr1_sel[i]);
			}
		} else {
			tmp = tp9937_read_reg(sd, clk_addr[ch]);
			tmp &= clk_and[ch];
			tmp |= tp2837_clk_v2[ch];
			tp9937_write_reg(sd, clk_addr[ch], tmp);
			tp9937_write_reg(sd, dat_addr[ch], sdr1_sel[ch]);
		}
		break;
	case DDR_4CH_16BIT:
		tmp = tp9937_read_reg(sd, 0x46);
		tmp |= tp2836_ddr4ch_mux[ch];
		tp9937_write_reg(sd, 0x46, tmp);
		tmp = tp9937_read_reg(sd, 0x47);
		tmp |= tp2836_ddr4ch_mux[ch];
		tp9937_write_reg(sd, 0x47, tmp);
		tmp = tp9937_read_reg(sd, 0x49);
		tmp |= tp2836_ddr4ch_mux[ch];
		tp9937_write_reg(sd, 0x49, tmp);

		break;
	default:
		break;
	}

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static bool tp9937_pll_det(struct v4l2_subdev *sd, u8 ch_sel)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	bool pll_locked = false;
	u8 video_input_status;

	if (ch_sel > CH_4)
		goto exit;

	if (tp9937_read_reg(sd, PAGE_SEL) != ch_sel)
		tp9937_write_reg(sd, PAGE_SEL, ch_sel);

	video_input_status = tp9937_read_reg(sd, 0x1);

	if ((video_input_status & 0xF0) == 0x70)
		pll_locked = true;
	else
		dev_info(tp9937->dev, "Ch%u video_input_status=0x%02x",
			ch_sel, video_input_status);

exit:
	return pll_locked;
}

static u8 tp9937_std_det(struct v4l2_subdev *sd, u8 ch_sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 cvstd = 0;

	if (ch_sel > CH_4)
		goto exit;

	if (tp9937_read_reg(sd, PAGE_SEL) != ch_sel)
		i2c_smbus_write_byte_data(client, PAGE_SEL, ch_sel);

	cvstd = tp9937_read_reg(sd, 0x3) & 0x7;

exit:
	return cvstd;
}

static int tp9937_s_power(struct v4l2_subdev *sd, int on)
{
	struct tp9937 *tp9937 = to_tp9937(sd);

	dev_info(tp9937->dev, "s_power %s", on ? "on":"off");

	if (tp9937->rst_gpio == NULL) {
		dev_info(tp9937->dev, "skip rest control");
		goto exit;
	}

	/* power on -> don't rest */
	gpiod_set_value_cansleep(tp9937->rst_gpio, on ? 0:1);

	if (!on)
		tp9937->initized = false;

exit:
	return 0;
}

static bool tp9937_signal_detect(struct v4l2_subdev *sd, u8 ch)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	int i;
	bool detected;

	for (i = CH_1; i < CH_ALL; i++)
		tp9937->pll_locked[i] = tp9937_pll_det(sd, i);

	dev_info(tp9937->dev, "pll_locked %u %u %u %u",
		tp9937->pll_locked[CH_1],
		tp9937->pll_locked[CH_2],
		tp9937->pll_locked[CH_3],
		tp9937->pll_locked[CH_4]);

	for (i = CH_1; i < CH_ALL; i++)
		if (tp9937->pll_locked[i])
			dev_info(tp9937->dev, "Ch%u CVSTD=%u", i, tp9937_std_det(sd, i));

	if (ch == CH_ALL)
		detected = tp9937->pll_locked[CH_1] && tp9937->pll_locked[CH_2] &&
			tp9937->pll_locked[CH_3] && tp9937->pll_locked[CH_4];
	else
		detected = tp9937->pll_locked[CH_1] | tp9937->pll_locked[CH_2] |
			tp9937->pll_locked[CH_3] | tp9937->pll_locked[CH_4];

	return detected;
}

static void tp9937_reset_default(struct v4l2_subdev *sd, u8 ch)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	u8 val;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tp9937_write_reg(sd, 0x07, 0xc0);
	tp9937_write_reg(sd, 0x0b, 0xc0);
	tp9937_write_reg(sd, 0x0c, 0x03);
	tp9937_write_reg(sd, 0x0e, 0x00);
	tp9937_write_reg(sd, 0x21, 0x84);
	tp9937_write_reg(sd, 0x38, 0x40);
	tp9937_write_reg(sd, 0x39, 0x1C);
	tp9937_write_reg(sd, 0x3a, 0x32);
	tp9937_write_reg(sd, 0x3B, 0x25);
	tp9937_write_reg(sd, 0x37, 0x04);
	tp9937_write_reg(sd, 0x3d, 0x60);

	switch (ch) {
	case CH_1:
		val = tp9937_read_reg(sd, 0xeb);
		val &= 0xf0;
		tp9937_write_reg(sd, 0xeb, val);
		break;
	case CH_2:
		val = tp9937_read_reg(sd, 0xeb);
		val &= 0x0f;
		tp9937_write_reg(sd, 0xeb, val);
		break;
	case CH_3:
		val = tp9937_read_reg(sd, 0xec);
		val &= 0xf0;
		tp9937_write_reg(sd, 0xec, val);
		break;
	case CH_4:
		val = tp9937_read_reg(sd, 0xec);
		val &= 0x0f;
		tp9937_write_reg(sd, 0xec, val);
		break;
	case CH_ALL:
		tp9937_write_reg(sd, 0xeb, 0x0);
		tp9937_write_reg(sd, 0xec, 0x0);
		break;
	}

	val = tp9937_read_reg(sd, 0x26);
	val &= 0xfe;
	tp9937_write_reg(sd, 0x26, val);

	val = tp9937_read_reg(sd, 0x06);
	val &= 0xfb;
	tp9937_write_reg(sd, 0x06, val);

	tp9937_write_reg(sd, 0x80, 0x50);

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static void tp9937_set_output_formatter(struct v4l2_subdev *sd, u8 fmt_mode)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	tp9937_write_reg(sd, 0x43, 0x10);
	tp9937_write_reg(sd, 0x44, 0x10);
	tp9937_write_reg(sd, 0xF5, 0xF0);
	tp9937_write_reg(sd, 0xF1, 0x00);
	tp9937_write_reg(sd, 0xEF, 0x55);
	tp9937_write_reg(sd, 0x4F, 0x07);

	switch (fmt_mode) {
	case SDR_1CH:
		tp9937_write_reg(sd, 0xFA, 0x88);
		tp9937_write_reg(sd, 0xFB, 0x88);
		tp9937_write_reg(sd, 0xF6, 0x00);
		tp9937_write_reg(sd, 0xF7, 0x11);
		tp9937_write_reg(sd, 0xF8, 0x22);
		tp9937_write_reg(sd, 0xF9, 0x33);
		tp9937_write_reg(sd, 0x50, 0x00);
		tp9937_write_reg(sd, 0x51, 0x00);
		tp9937_write_reg(sd, 0x52, 0x00);
		tp9937_write_reg(sd, 0x53, 0x00);
		tp9937_write_reg(sd, 0xF3, 0x08);
		tp9937_write_reg(sd, 0xF2, 0x00);
		break;
	case SDR_2CH:
		tp9937_write_reg(sd, 0xE8, 0x55);
		tp9937_write_reg(sd, 0xFA, 0x88);
		tp9937_write_reg(sd, 0xFB, 0x88);
		tp9937_write_reg(sd, 0xF6, 0x10);
		tp9937_write_reg(sd, 0xF7, 0x23);
		tp9937_write_reg(sd, 0xF8, 0x10);
		tp9937_write_reg(sd, 0xF9, 0x23);
		tp9937_write_reg(sd, 0x50, 0x81);
		tp9937_write_reg(sd, 0x51, 0xB2);
		tp9937_write_reg(sd, 0x52, 0x81);
		tp9937_write_reg(sd, 0x53, 0xB2);
		tp9937_write_reg(sd, 0xF3, 0x08);
		tp9937_write_reg(sd, 0xF2, 0x00);
		break;
	case DDR_2CH:
		tp9937_write_reg(sd, 0xFA, 0x88);
		tp9937_write_reg(sd, 0xFB, 0x88);
		tp9937_write_reg(sd, 0xF6, 0x10);
		tp9937_write_reg(sd, 0xF7, 0x23);
		tp9937_write_reg(sd, 0xF8, 0x10);
		tp9937_write_reg(sd, 0xF9, 0x23);
		tp9937_write_reg(sd, 0x50, 0x00);
		tp9937_write_reg(sd, 0x51, 0x00);
		tp9937_write_reg(sd, 0x52, 0x00);
		tp9937_write_reg(sd, 0x53, 0x00);
		tp9937_write_reg(sd, 0xF3, 0x08);
		tp9937_write_reg(sd, 0xF2, 0x00);
		break;
	case DDR_4CH:
		tp9937_write_reg(sd, 0xE8, 0x55);
		tp9937_write_reg(sd, 0xFA, 0x88);
		tp9937_write_reg(sd, 0xFB, 0x88);
		tp9937_write_reg(sd, 0xF6, 0x10);
		tp9937_write_reg(sd, 0xF7, 0x10);
		tp9937_write_reg(sd, 0xF8, 0x10);
		tp9937_write_reg(sd, 0xF9, 0x10);
		tp9937_write_reg(sd, 0x50, 0xB2);
		tp9937_write_reg(sd, 0x51, 0xB2);
		tp9937_write_reg(sd, 0x52, 0xB2);
		tp9937_write_reg(sd, 0x53, 0xB2);
		tp9937_write_reg(sd, 0xF3, 0x08);
		tp9937_write_reg(sd, 0xF2, 0x00);
		break;
	case DDR_1CH:
		tp9937_write_reg(sd, 0xFA, 0x88);
		tp9937_write_reg(sd, 0xFB, 0x88);
		tp9937_write_reg(sd, 0xF6, 0x04);
		tp9937_write_reg(sd, 0xF7, 0x15);
		tp9937_write_reg(sd, 0xF8, 0x26);
		tp9937_write_reg(sd, 0xF9, 0x37);
		tp9937_write_reg(sd, 0x50, 0x00);
		tp9937_write_reg(sd, 0x51, 0x00);
		tp9937_write_reg(sd, 0x52, 0x00);
		tp9937_write_reg(sd, 0x53, 0x00);
		tp9937_write_reg(sd, 0xF3, 0x08);
		tp9937_write_reg(sd, 0xF2, 0x00);
		break;
	case SDR_4CH_16BIT:
		tp9937_write_reg(sd, 0xFA, 0x88);
		tp9937_write_reg(sd, 0xFB, 0x88);
		tp9937_write_reg(sd, 0xF6, 0x01);
		tp9937_write_reg(sd, 0xF7, 0x45);
		tp9937_write_reg(sd, 0xF8, 0x01);
		tp9937_write_reg(sd, 0xF9, 0x45);
		tp9937_write_reg(sd, 0x50, 0xBA);
		tp9937_write_reg(sd, 0x51, 0xFE);
		tp9937_write_reg(sd, 0x52, 0xBA);
		tp9937_write_reg(sd, 0x53, 0xFE);
		tp9937_write_reg(sd, 0xF3, 0x08);
		tp9937_write_reg(sd, 0xF2, 0x00);
		break;
	case DDR_4CH_16BIT:
		tp9937_write_reg(sd, 0xEF, 0xAA);
		tp9937_write_reg(sd, 0xFA, 0x88);
		tp9937_write_reg(sd, 0xFB, 0x88);
		tp9937_write_reg(sd, 0xF6, 0x01);
		tp9937_write_reg(sd, 0xF7, 0x45);
		tp9937_write_reg(sd, 0xF8, 0x01);
		tp9937_write_reg(sd, 0xF9, 0x45);
		tp9937_write_reg(sd, 0x50, 0xA3);
		tp9937_write_reg(sd, 0x51, 0xE7);
		tp9937_write_reg(sd, 0x52, 0xA3);
		tp9937_write_reg(sd, 0x53, 0xE7);
		tp9937_write_reg(sd, 0xF3, 0x08);
		tp9937_write_reg(sd, 0xF2, 0x00);
	}

	tp9937_write_reg(sd, 0xF4, 0x80); /* reset digital module */
	tp9937_write_reg(sd, 0x4D, 0x0F);
	tp9937_write_reg(sd, 0x4E, 0x0F);

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s done\n", __func__);
}

static int tp9937_set_video_mode(struct v4l2_subdev *sd,
		u8 fmt_mode, u8 video_mode, u8 ch, u8 std)
{
	int ret = 0;
	struct tp9937 *tp9937 = to_tp9937(sd);
	u8 tmp;

	if (ch > CH_ALL) {
		ret = -EINVAL;
		dev_err(tp9937->dev, "%s failed, invalid ch=%u\n", __func__, ch);
		goto exit;
	}

	/* Set Page Register to the appropriate Channel */
	ret = tp9937_write_reg(sd, PAGE_SEL, ch);
	if (ret < 0) {
		dev_err(tp9937->dev, "%s failed, i2c_smbus ret=%d\n", __func__, ret);
		goto exit;
	}

	dev_info(tp9937->dev, "Set video mode fmt_mode=%u video_mode=%u ch=%u std=%u\n",
		fmt_mode, video_mode, ch, std);

	switch (video_mode) {
	case TP9937_720P30V2:
		tp9937_write_reg(sd, 0x35, 0x25);
		tp2802_set_work_mode_720p60(sd);
		tmp = tp9937_read_reg(sd, 0x02);
		tmp &= 0xE8;
		tmp |= 0x02;
		tp9937_write_reg(sd, 0x02, tmp);
		tmp = tp9937_read_reg(sd, 0xf5);
		tmp |= sys_mode[ch];
		tp9937_write_reg(sd, 0xf5, tmp);

		tp2831_v2_dataset(sd);

		if (std == STD_HDA) {
			tp2831_a720p30_dataset(sd);
		} else if (std == STD_HDC || std == STD_HDC_DEFAULT) {
			tp2826_c720p30_dataset(sd);

			/* HDC 720p30 position adjust */
			if (std == STD_HDC) {
				tp9937_write_reg(sd, 0x15, 0x13);
				tp9937_write_reg(sd, 0x16, 0x08);
				tp9937_write_reg(sd, 0x17, 0x00);
				tp9937_write_reg(sd, 0x18, 0x19);
				tp9937_write_reg(sd, 0x19, 0xD0);
				tp9937_write_reg(sd, 0x1A, 0x25);
				tp9937_write_reg(sd, 0x1C, 0x06);
				tp9937_write_reg(sd, 0x1D, 0x72);
			}
		}

		tp282x_sysclk_v2(sd, fmt_mode, ch);
		break;

	case TP9937_720P25V2:
		tp9937_write_reg(sd, 0x35, 0x25);
		tp2802_set_work_mode_720p50(sd);
		tmp = tp9937_read_reg(sd, 0x02);
		tmp &= 0xE8;
		tmp |= 0x02;
		tp9937_write_reg(sd, 0x02, tmp);
		tmp = tp9937_read_reg(sd, 0xf5);
		tmp |= sys_mode[ch];
		tp9937_write_reg(sd, 0xf5, tmp);

		tp2831_v2_dataset(sd);

		if (std == STD_HDA) {
			tp2831_a720p25_dataset(sd);

		} else if (std == STD_HDC || std == STD_HDC_DEFAULT) {

			tp2826_c720p25_dataset(sd);

			/* HDC 720p25 position adjust */
			if (std == STD_HDC) {
				tp9937_write_reg(sd, 0x15, 0x13);
				tp9937_write_reg(sd, 0x16, 0x0a);
				tp9937_write_reg(sd, 0x17, 0x00);
				tp9937_write_reg(sd, 0x18, 0x19);
				tp9937_write_reg(sd, 0x19, 0xd0);
				tp9937_write_reg(sd, 0x1A, 0x25);
				tp9937_write_reg(sd, 0x1C, 0x06);
				tp9937_write_reg(sd, 0x1D, 0x7a);
			}
		}

		tp282x_sysclk_v2(sd, fmt_mode, ch);
		break;

	case TP9937_PAL:
		tp9937_write_reg(sd, 0x35, 0x25);
		tp2802_set_work_mode_PAL(sd);
		tmp = tp9937_read_reg(sd, 0x02);
		tmp &= 0xE8;
		tmp |= 0x07;
		tp9937_write_reg(sd, 0x02, tmp);
		tmp = tp9937_read_reg(sd, 0xf5);
		tmp |= sys_mode[ch];
		tp9937_write_reg(sd, 0xf5, tmp);

		tp2831_pal_dataset(sd);

		tp282x_sysclk_v2(sd, fmt_mode, ch);
		break;

	case TP9937_NTSC:
		tp9937_write_reg(sd, 0x35, 0x25);
		tp2802_set_work_mode_NTSC(sd);
		tmp = tp9937_read_reg(sd, 0x02);
		tmp &= 0xE8;
		tmp |= 0x07;
		tp9937_write_reg(sd, 0x02, tmp);
		tmp = tp9937_read_reg(sd, 0xf5);
		tmp |= sys_mode[ch];
		tp9937_write_reg(sd, 0xf5, tmp);

		tp2831_ntsc_dataset(sd);

		tp282x_sysclk_v2(sd, fmt_mode, ch);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (fmt_mode == SDR_4CH_16BIT || fmt_mode == DDR_4CH_16BIT) {
		tmp = tp9937_read_reg(sd, 0x02);
		tmp &= 0x7f;
		tp9937_write_reg(sd, 0x02, tmp);
	} else {
		tmp = tp9937_read_reg(sd, 0x02);
		tmp |= 0xC0;
		tp9937_write_reg(sd, 0x02, tmp);
	}

	/* Clamp current */
	tmp = tp9937_read_reg(sd, 0x26);
	tmp |= 0x40;
	tp9937_write_reg(sd, 0x26, tmp);

	/* Clamp current control */
	switch (video_mode) {
	case CH_1:
		tmp = tp9937_read_reg(sd, 0xeb);
		tmp |= 0x04;
		tp9937_write_reg(sd, 0xeb, tmp);
		break;
	case CH_2:
		tmp = tp9937_read_reg(sd, 0xeb);
		tmp |= 0x40;
		tp9937_write_reg(sd, 0xeb, tmp);
		break;
	case CH_3:
		tmp = tp9937_read_reg(sd, 0xec);
		tmp |= 0x04;
		tp9937_write_reg(sd, 0xec, tmp);
		break;
	case CH_4:
		tmp = tp9937_read_reg(sd, 0xec);
		tmp |= 0x40;
		tp9937_write_reg(sd, 0xec, tmp);
		break;
	case CH_ALL:
		tmp = tp9937_read_reg(sd, 0xeb);
		tmp |= 0x44;
		tp9937_write_reg(sd, 0xeb, tmp);
		tmp = tp9937_read_reg(sd, 0xec);
		tmp |= 0x44;
		tp9937_write_reg(sd, 0xec, tmp);
		break;
	default:
		break;
	}

	tmp = tp9937_read_reg(sd, 0x06);
	tmp |= 0x80;
	tp9937_write_reg(sd, 0x06, tmp);

exit:
	return ret;
}

static void tp9937_cfg_channel_id(struct v4l2_subdev *sd, u8 fmt_mode)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	switch (fmt_mode) {
	case DDR_4CH:
		fallthrough;
	case DDR_4CH_16BIT:
		fallthrough;
	case SDR_4CH_16BIT:
		tp9937_write_reg(sd, PAGE_SEL, CH_1);
		tp9937_write_reg(sd, 0x34, 0x10);
		tp9937_write_reg(sd, PAGE_SEL, CH_2);
		tp9937_write_reg(sd, 0x34, 0x11);
		tp9937_write_reg(sd, PAGE_SEL, CH_3);
		tp9937_write_reg(sd, 0x34, 0x12);
		tp9937_write_reg(sd, PAGE_SEL, CH_4);
		tp9937_write_reg(sd, 0x34, 0x13);
		break;
	case SDR_2CH:
		fallthrough;
	case DDR_2CH:
		tp9937_write_reg(sd, PAGE_SEL, CH_1);
		tp9937_write_reg(sd, 0x34, 0x10);
		tp9937_write_reg(sd, PAGE_SEL, CH_2);
		tp9937_write_reg(sd, 0x34, 0x12);
		break;
	case SDR_1CH:
		fallthrough;
	case DDR_1CH:
		fallthrough;
	default:
		tp9937_write_reg(sd, PAGE_SEL, CH_1);
		tp9937_write_reg(sd, 0x34, 0x00);
		tp9937_write_reg(sd, PAGE_SEL, CH_2);
		tp9937_write_reg(sd, 0x34, 0x00);
		tp9937_write_reg(sd, PAGE_SEL, CH_3);
		tp9937_write_reg(sd, 0x34, 0x00);
		tp9937_write_reg(sd, PAGE_SEL, CH_4);
		tp9937_write_reg(sd, 0x34, 0x00);
		break;
	}

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s end\n", __func__);
}

static int tp9937_comm_init(struct v4l2_subdev *sd, u8 fmt_mode, u8 video_mode, u8 std)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	struct tp9937_debug *debug = &tp9937->debug;
	int ret;
	u32 ch_index;
	u32 max_ch;

	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s begin\n", __func__);

	ret = tp9937_write_reg(sd, PAGE_SEL, CH_ALL);
	if (ret < 0) {
		dev_err(tp9937->dev, "%s failed, i2c_smbus ret=%d\n", __func__, ret);
		goto exit;
	}

	tp9937_reset_default(sd, CH_ALL);

	tp9937_set_output_formatter(sd, fmt_mode);

	if (fmt_mode == SDR_1CH)
		tp9937_write_reg(sd, 0x02, 0xC0|SAV_HEADER_1MUX); /* BT1120/BT656 header */
	else
		tp9937_write_reg(sd, 0x02, 0xC8); /* BT656 header */

	if ((fmt_mode == DDR_4CH) || (fmt_mode == SDR_4CH_16BIT) || (fmt_mode == DDR_4CH_16BIT))
		max_ch = CH_4;
	else if ((fmt_mode == SDR_2CH) || (fmt_mode == DDR_2CH))
		max_ch = CH_2;
	else
		max_ch = CH_1;

	for (ch_index = CH_1; ch_index <= max_ch; ch_index++) {
		ret = tp9937_set_video_mode(sd, fmt_mode, video_mode, ch_index, std);
		if (ret) {
			dev_err(tp9937->dev, "set video_mode failed, ch=%u ret=%d\n", ch_index, ret);
			goto exit;
		}
	}

	tp9937_write_reg(sd, PAGE_SEL, CH_ALL);

	tp9937_write_reg(sd, 0x71, 0x20);
	tp9937_write_reg(sd, 0x73, 0x16);

	tp9937_cfg_channel_id(sd, fmt_mode);

exit:
	if (debug->en_dump_i2c)
		dev_info(tp9937->dev, "%s end\n", __func__);

	return ret;
}

static int tp9937_initize(struct v4l2_subdev *sd, u8 fmt_mode, u8 video_mode, u8 std)
{
	int ret = 0;
	struct tp9937 *tp9937 = to_tp9937(sd);
	u8 val;
	u32 device_id;

	dev_info(tp9937->dev, "init fmt_mode=%u video_mode=%u std=%u\n",
		fmt_mode, video_mode, std);

	if (tp9937->rst_gpio != NULL) {
		tp9937_s_power(sd, false);
		msleep(500);
		tp9937_s_power(sd, true);
	}

	/* page reset */
	ret = tp9937_write_reg(sd, PAGE_SEL, 0x00);
	if (ret < 0) {
		dev_err(tp9937->dev, "%s failed, i2c_smbus ret=%d\n", __func__, ret);
		goto exit;
	}

	/* output disable */
	tp9937_write_reg(sd, 0x4D, 0x00);
	tp9937_write_reg(sd, 0x4E, 0x00);

	/* PLL reset */
	val = tp9937_read_reg(sd, 0x44);
	tp9937_write_reg(sd, 0x44, val|0x40);
	msleep(20);
	tp9937_write_reg(sd, 0x44, val);

	val = tp9937_read_reg(sd, 0xFE);
	device_id = tp9937_read_reg(sd, 0xFF);
	device_id = (val << 8) | device_id;
	dev_info(tp9937->dev, "device_id=%x\n", device_id);

	/* common init */
	ret = tp9937_comm_init(sd, fmt_mode, video_mode, std);
	if (ret < 0) {
		dev_err(tp9937->dev, "tp9937_comm_init failed, ret=%d\n", ret);
		goto exit;
	}

	tp9937->initized = true;

exit:
	return ret;
}

static int en_blue_pattern_set(void *data, u64 val)
{
	struct tp9937 *tp9937 = data;
	struct v4l2_subdev *sd = &tp9937->sd;

	if (val) {
		tp9937_write_reg(sd, PAGE_SEL, CH_ALL);
		tp9937_write_reg(sd, 0x2A, 0x3C);
	} else {
		tp9937_write_reg(sd, PAGE_SEL, CH_ALL);
		tp9937_write_reg(sd, 0x2A, 0x30);
	}

	tp9937->debug.en_blue_pattern = val ? true:false;

	return 0;
}

static int en_blue_pattern_get(void *data, u64 *val)
{
	struct tp9937 *tp9937 = data;

	*val = (u64)tp9937->debug.en_blue_pattern;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(tp9937_dbg_pattern_fops, en_blue_pattern_get, en_blue_pattern_set,
			 "%llu\n");

static int tp9937_format_set(void *data, u64 val)
{
	struct tp9937 *tp9937 = data;
	struct v4l2_subdev *sd = &tp9937->sd;
	int ret;
	u8 fmt_mode;
	u8 video_mode;
	u8 std;
	u8 i;

	fmt_mode = tp9937->fmt_mode;
	video_mode = tp9937->video_mode;
	std = tp9937->std;

	if (!tp9937->initized)
		ret = tp9937_initize(sd, fmt_mode, video_mode, std);
	else
		ret = tp9937_set_video_mode(sd, fmt_mode, video_mode, CH_ALL, std);

	if (ret < 0) {
		dev_err(tp9937->dev, "%s failed, ret=%d\n", __func__, ret);
		goto exit;
	}

	for (i = 0; i < 3; i++) {
		bool detected;

		detected = tp9937_signal_detect(sd, CH_ALL);

		if (detected)
			break;

		msleep(500);
	}

exit:
	return ret;
}

static int tp9937_format_get(void *data, u64 *val)
{

	*val = 1;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(tp9937_dbg_format_fops, tp9937_format_get, tp9937_format_set,
			 "%llu\n");

static void tp9937_setup_dbgfs(struct tp9937 *tp9937)
{
	struct tp9937_debug *debug = &tp9937->debug;
	struct v4l2_subdev *sd = &tp9937->sd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	char name[16];

	snprintf(name, 16, "tp9937_%02x", client->addr);
	debug->debugfs_dir = debugfs_create_dir(name, NULL);

	debug->en_blue_pattern = false;
	debug->en_dump_i2c = false;

	if (IS_ERR_OR_NULL(debug->debugfs_dir)) {
		dev_info(tp9937->dev, "DebugFS unsupported\n");
		return;
	}

	debugfs_create_file("en_blue_pattern", 0644, debug->debugfs_dir, tp9937,
			&tp9937_dbg_pattern_fops);
	debugfs_create_file("format", 0644, debug->debugfs_dir, tp9937,
			&tp9937_dbg_format_fops);
	debugfs_create_bool("en_dump_i2c", 0644, debug->debugfs_dir,
		&debug->en_dump_i2c);
}

static int tp9937_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state, struct v4l2_subdev_format *format)
{
	struct tp9937 *tp9937 = to_tp9937(sd);

	format->format.width = 1280;
	format->format.height = 720;

	dev_dbg(tp9937->dev, "%s %ux%u\n", __func__,
		format->format.width, format->format.height);

	return 0;
}

static int tp9937_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state, struct v4l2_subdev_format *format)
{
	struct tp9937 *tp9937 = to_tp9937(sd);
	int ret;
	u8 fmt_mode;
	u8 video_mode;
	u8 std;

	fmt_mode = tp9937->fmt_mode;
	video_mode = tp9937->video_mode;
	std = tp9937->std;

	if (!tp9937->initized)
		ret = tp9937_initize(sd, fmt_mode, video_mode, std);
	else
		ret = tp9937_set_video_mode(sd, fmt_mode, video_mode, CH_ALL, std);

	dev_dbg(tp9937->dev, "%s, ret=%d\n", __func__, ret);

	return ret;
}

static const struct v4l2_subdev_core_ops tp9937_core_ops = {
	.s_power = tp9937_s_power,
};

static const struct v4l2_subdev_pad_ops tp9937_pad_ops = {
	/* VIDIOC_SUBDEV_G_FMT handler */
	.get_fmt = tp9937_get_fmt,
	/* VIDIOC_SUBDEV_S_FMT handler */
	.set_fmt = tp9937_set_fmt,
};

static const struct v4l2_subdev_ops tp9937_subdev_ops = {
	.core = &tp9937_core_ops,
	.pad = &tp9937_pad_ops,
};

static int tp9937_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct tp9937 *tp9937;
	int ret;

	tp9937 = devm_kzalloc(dev, sizeof(*tp9937), GFP_KERNEL);
	if (!tp9937)
		return -ENOMEM;

	tp9937->i2c_client = client;
	tp9937->dev = dev;

	tp9937->initized = false;
	tp9937->fmt_mode = DDR_4CH;
	tp9937->video_mode = TP9937_720P30V2;
	tp9937->std = STD_HDA;

	dev_info(dev, "fmt_mode=%u video_mode=0x%02x std=%u\n",
		tp9937->fmt_mode, tp9937->video_mode, tp9937->std);

	tp9937->rst_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(tp9937->rst_gpio)) {
		dev_err(dev, "reset gpio doesn't exist\n");
		tp9937->rst_gpio = NULL;
	} else {
		dev_info(dev, "Reset gpio=%d\n", desc_to_gpio(tp9937->rst_gpio));
	}

	ret = v4l2_device_register(tp9937->dev, &tp9937->v4l2_dev);
	if (ret) {
		dev_err(tp9937->dev, "Failed to register v4l2 device, ret=%d\n", ret);
		goto free_gpio;
	}

	v4l2_i2c_subdev_init(&tp9937->sd, client, &tp9937_subdev_ops);
	tp9937->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	ret = v4l2_device_register_subdev(&tp9937->v4l2_dev, &tp9937->sd);
	if (ret) {
		dev_err(tp9937->dev, "Failed to register v4l2 subdev, ret=%d\n", ret);
		goto free_gpio;
	}

	ret = v4l2_device_register_subdev_nodes(&tp9937->v4l2_dev);
	if (ret) {
		dev_err(tp9937->dev, "Failed to register v4l2 subdev node, ret=%d\n", ret);
		goto free_gpio;
	}

	ret = tp9937_s_power(&tp9937->sd, true);
	if (ret < 0) {
		dev_err(dev, "power up failed\n");
		goto free_gpio;
	}

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		tp9937_setup_dbgfs(tp9937);

	return 0;

free_gpio:
	if (tp9937->rst_gpio)
		devm_gpiod_put(dev, tp9937->rst_gpio);

	return ret;
}

static void tp9937_remove(struct i2c_client *client)
{

}

static const struct of_device_id tp9937_of_match[] = {
	{ .compatible = "techpoint,tp9937" },
	{ }
};

MODULE_DEVICE_TABLE(of, tp9937_of_match);

static struct i2c_driver tp9937_driver = {
	.probe = tp9937_probe,
	.remove = tp9937_remove,
	.driver = {
		.name = "tp9937",
		.of_match_table = tp9937_of_match,
	},
};

module_i2c_driver(tp9937_driver);

MODULE_DESCRIPTION("Techpoint tp9937 Driver");
MODULE_AUTHOR("Chase Yen <chase.yen@realtek.com>");
MODULE_LICENSE("GPL v2");
