// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <media/v4l2-event.h>
#include <linux/device.h>
#include <linux/sysfs.h>


#define PAGE_SEL        0x40

#define STD_720P60    0
#define STD_720P50    1
#define STD_1080P30   2
#define STD_1080P25   3
#define STD_720P30    4
#define STD_720P25    5
/* divide STD_SD (6) defined in spec. into STD_SD_480I/STD_SD_576I */
#define STD_SD_480I   6
#define STD_SD_576I   7
#define STD_UNDEFINED 0x100

/* Camera signal detection mode */
#define DETECT_MODE_DISABLED 0  /* Detection disabled (default) */
#define DETECT_MODE_POLLING  1  /* Polling mode (100ms interval) */

/* Camera channel count */
#define TP2815_CH_COUNT 4

/* Signal status */
#define VIN_SIGNAL_NONE   0
#define VIN_SIGNAL_READY  1

/* Internal status to notify the user-space to reset */
#define TP2815_RESETTING         0xFE
/* Internal status to notify the user-space channels disabled after reset */
#define TP2815_CHANNEL_DISABLED  0xFC

/* Each signal status of all channels is none */
#define VIN_ALL_SIGNALS_NONE  0x00
/* Each signal status of all channels is ready */
#define VIN_ALL_SIGNALS_READY 0xFF

#define RTK_V4L2_EVENT_SIGNAL_CHANGE (V4L2_EVENT_PRIVATE_START + 0x100)

/* Some soc-vendors defined their base in <uapi/linux/v4l2-controls.h> */
#define V4L2_CID_RTK_CAMERA_BASE (V4L2_CID_USER_BASE + 0x1200)

#define V4L2_CID_RTK_CAMERA_VIDEOSIGNAL_DETECT    (V4L2_CID_RTK_CAMERA_BASE + 0x0)
#define V4L2_CID_RTK_CAMERA_VIDEOSIGNAL_AUTORESET (V4L2_CID_RTK_CAMERA_BASE + 0x1)


/* Number of pixels per line for each standard,
 * e.g npxl for STD_1080P30 is 0x898, i.e npxl_{h, l} is (0x08, 0x98)
 */
#define N_PIXELS_LINE(h, l) ((npxl_h == h) && (npxl_l == l))

#define MAX_CH_CNT (8)
#define NUM_VALID_BYTES (5 + (MAX_CH_CNT*2))
struct tp2815_event_payload {
	/* Number of valid bytes */
	u8 nbytes;
	/* Number of polling channels */
	u8 ch_cnt;
	/* Signal bits for channels, none:0, ready:1 */
	u8 sigbits;
	u8 prev_sigbits;
	/* Alternative data, compatible for the base-n state, esp. n > 2 */
	u8 status[MAX_CH_CNT];
	u8 prev_status[MAX_CH_CNT];
	/* Bits for channels being disalbed which value: 1 << channel-index */
	u8 disabled_ch_bits;
	u8 reserved[64-NUM_VALID_BYTES]; /* align to 64 bytes */
};


/* Channel state machine states */
enum tp2815_work_state {
	ST_SIGNAL_CHECK = 0,  /* Normal signal checking */
	ST_RESETTING,         /* Resetting channel */
	ST_DISABLED,          /* Disabled after reset failures */
};

/* Per-channel state */
struct tp2815_ch_state {
	enum tp2815_work_state state;
	int unstable_cnt;  /* Unstable signal counter */
	int reset_cnt;     /* Reset attempt counter */
};


static const char * const str_cam_std[] = {
	"STD_720P60", "STD_720P50", "STD_1080P30", "STD_1080P25",
	"STD_720P30", "STD_720P25", "STD_SD_480I", "STD_SD_576I",
};

enum {
	CH_1 = 0,
	CH_2 = 1,
	CH_3 = 2,
	CH_4 = 3,
	CH_ALL = 4,
	MIPI_PAGE = 8,
};

enum {
	STD_TVI, /* TVI */
	STD_HDA, /* AHD */
};

enum {
	PAL,
	NTSC,
	/* 720p25 */
	HD25,
	/* 720p30 */
	HD30,
	/* 1080p25 */
	FHD25,
	/* 1080p30 */
	FHD30,
	/* 1080p50 */
	FHD50,
	/* 1080p60 */
	FHD60,
	/* 2560x1440p25 */
	QHD25,
	/* 2560x1440p30 */
	QHD30,
	/* 1280x960p25, must use with MIPI_4CH4LANE_445M */
	UVGA25,
	/* 1280x960p30, must use with MIPI_4CH4LANE_445M */
	UVGA30,
	/* special 720p30 with ISX019, must use with MIPI_4CH4LANE_396M */
	HD30HDR,
	/* 720p50 */
	HD50,
	/* 720p60 */
	HD60,
	/* HDA 1280x960p30, must use with MIPI_4CH4LANE_378M */
	A_UVGA30,
	/* FH 1280x960p30, must use with MIPI_4CH4LANE_432M */
	F_UVGA30,
	/* TVI 1280x960p30, must use with MIPI_4CH4LANE_378M */
	UVGA30_945,
	/* 1080p27.5 */
	FHD275,
};

enum {
	MIPI_4CH4LANE_297M, /* up to 4x720p25/30 */
	MIPI_4CH4LANE_594M, /* up to 4x1080p25/30 */
	MIPI_4CH2LANE_594M, /* up to 4x720pp25/30 */
	MIPI_4CH4LANE_445M, /* only for 4x960p25/30 */
	MIPI_2CH2LANE_594M,
	MIPI_2CH4LANE_297M, /* up to 2x1080p25/30 */
	MIPI_2CH4LANE_594M, /* up to 2xQHDp25/30 or 2x1080p50/60 */
	MIPI_2CH4LANE_648M,
	MIPI_4CH4LANE_396M, /* only for 4xHD30HDR */
	MIPI_4CH4LANE_378M, /* only for 4xA_UVGA30 & 4xUVGA30_945 */
	MIPI_4CH4LANE_432M, /* only for 4xF_UVGA30 */
	MIPI_1CH2LANE_594M,
	MIPI_1CH4LANE_297M,
	MIPI_3CH4LANE_297M,
	MIPI_3CH4LANE_594M,
	MIPI_4CH4LANE_345M, /* only for HD30864 */
	MIPI_4CH4LANE_648M, /* only for FHD60_X3C */
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

	u32 std;
	struct tp2815_debug debug;
	bool pll_locked[CH_ALL];

	/* Camera signal detection */
	struct delayed_work detect_work;
	u32 detect_mode;  /* DETECT_MODE_DISABLED/POLLING */
	u8 signal_status[TP2815_CH_COUNT]; /* Signal status */
	struct tp2815_ch_state ch_state[TP2815_CH_COUNT]; /* State machine */
	bool auto_reset_enabled; /* Auto reset on signal loss */
	struct mutex detect_lock; /* Protect detection I2C */

	struct v4l2_ctrl_handler hdl;
};

#define to_tp2815(x) container_of(x, struct tp2815, x)


/* STD_HDA FHD30 4CH4LANE, 1080P30 */
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

/* STD_HDA FHD25 4CH4LANE, 1080P25 */
static const struct regval std_hda_fhd25[] = {
	{PAGE_SEL, CH_ALL},
	{0x45, 0x01},
	{0x06, 0x12},
	{0x27, 0x2D},
	{0xF5, 0xF0},
	{0x02, 0x40},
	{0x07, 0xC0},
	{0x0B, 0xC0},
	{0x0C, 0x03},
	{0x15, 0x03},
	{0x16, 0xD2},
	{0x17, 0x80},
	{0x18, 0x29},
	{0x19, 0x38},
	{0x1A, 0x47},
	{0x1C, 0x0A},
	{0x1D, 0x50},
	{0x20, 0x30},
	{0x2D, 0x30},
	{0x30, 0x48},
	{0x31, 0xBB},
	{0x32, 0x2E},
	{0x33, 0x90},
	{0x35, 0x05},
	{0x38, 0x00},
	{0x39, 0x1C},
	{0x02, 0x44},
	{0x0D, 0x73},
	{0x15, 0x01},
	{0x16, 0xF0},
	{0x18, 0x2A},
	{0x20, 0x3C},
	{0x21, 0x46},
	{0x25, 0xFE},
	{0x26, 0x0D},
	{0x2C, 0x3A},
	{0x2D, 0x54},
	{0x2E, 0x40},
	{0x30, 0xA5},
	{0x31, 0x86},
	{0x32, 0xFB},
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

/* STD_HDA HD25 4CH4LANE, 720P25 */
static const struct regval std_hda_hd25[] = {
	{PAGE_SEL, CH_ALL},
	{0x45, 0x01},
	{0x06, 0x12},
	{0xF5, 0xFF},
	{0x02, 0x46},
	{0x07, 0xC0},
	{0x0B, 0xC0},
	{0x0C, 0x13},
	{0x0D, 0x71},
	{0x15, 0x13},
	{0x16, 0x15},
	{0x17, 0x00},
	{0x18, 0x1B},
	{0x19, 0xD0},
	{0x1A, 0x25},
	{0x1C, 0x07},
	{0x1D, 0xBC},
	{0x20, 0x40},
	{0x21, 0x46},
	{0x22, 0x36},
	{0x23, 0x3C},
	{0x25, 0xfe},
	{0x26, 0x01},
	{0x2B, 0x60},
	{0x2C, 0x3A},
	{0x2D, 0x5A},
	{0x2E, 0x40},
	{0x30, 0x9E},
	{0x31, 0x20},
	{0x32, 0x10},
	{0x33, 0x90},
	{0x35, 0x25},
	{0x38, 0x00},
	{0x39, 0x18},
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
	{0x33, 0x07},
	{0x33, 0x00},
	{0x14, 0x33},
	{0x14, 0xB3},
	{0x14, 0x33},
	{0x23, 0x02},
	{0x23, 0x00}
};

static const struct regval init_pal_960h[] = {
	{0x02, 0x47},
	{0x0C, 0x13},
	{0x0D, 0x51},
	{0x15, 0x13},
	{0x16, 0x76},
	{0x17, 0x80},
	{0x18, 0x17},
	{0x19, 0x20},
	{0x1A, 0x17},
	{0x1C, 0x09},
	{0x1D, 0x48},
	{0x20, 0x48},
	{0x21, 0x84},
	{0x22, 0x37},
	{0x23, 0x3F},
	{0x2B, 0x70},
	{0x2C, 0x2A},
	{0x2D, 0x64},
	{0x2E, 0x56},
	{0x30, 0x7A},
	{0x31, 0x4A},
	{0x32, 0x4D},
	{0x33, 0xF0},
	{0x35, 0x65},
	{0x38, 0x00},
	{0x39, 0x04}
};

static const struct regval init_pal_720h[] = {
	{0x06, 0x32},
	{0x02, 0x47},
	{0x07, 0x80},
	{0x0B, 0x80},
	{0x0C, 0x13},
	{0x0D, 0x51},
	{0x15, 0x03},
	{0x16, 0xF0},
	{0x17, 0xA0},
	{0x18, 0x17},
	{0x19, 0x20},
	{0x1A, 0x15},
	{0x1C, 0x06},
	{0x1D, 0xC0},
	{0x20, 0x48},
	{0x21, 0x84},
	{0x22, 0x37},
	{0x23, 0x3F},
	{0x2B, 0x70},
	{0x2C, 0x2A},
	{0x2D, 0x4B},
	{0x2E, 0x56},
	{0x30, 0x7A},
	{0x31, 0x4A},
	{0x32, 0x4D},
	{0x33, 0xFB},
	{0x35, 0x65},
	{0x38, 0x00},
	{0x39, 0x04}
};

static const struct regval init_ntsc_960h[] = {
	{0x02, 0x47},
	{0x0C, 0x13},
	{0x0D, 0x50},
	{0x15, 0x13},
	{0x16, 0x60},
	{0x17, 0x80},
	{0x18, 0x12},
	{0x19, 0xF0},
	{0x1A, 0x07},
	{0x1C, 0x09},
	{0x1D, 0x38},
	{0x20, 0x40},
	{0x21, 0x84},
	{0x22, 0x36},
	{0x23, 0x3C},
	{0x2B, 0x70},
	{0x2C, 0x2A},
	{0x2D, 0x68},
	{0x2E, 0x57},
	{0x30, 0x62},
	{0x31, 0xBB},
	{0x32, 0x96},
	{0x33, 0xC0},
	{0x35, 0x65},
	{0x38, 0x00},
	{0x39, 0x04}
};

static const struct regval init_ntsc_720h[] = {
	{0x02, 0x47},
	{0x07, 0x80},
	{0x0B, 0x80},
	{0x0C, 0x13},
	{0x0D, 0x50},
	{0x15, 0x03},
	{0x16, 0xD6},
	{0x17, 0xA0},
	{0x18, 0x12},
	{0x19, 0xF0},
	{0x1A, 0x05},
	{0x1C, 0x06},
	{0x1D, 0xB4},
	{0x20, 0x40},
	{0x21, 0x84},
	{0x22, 0x36},
	{0x23, 0x3C},
	{0x2B, 0x70},
	{0x2C, 0x2A},
	{0x2D, 0x4B},
	{0x2E, 0x57},
	{0x30, 0x62},
	{0x31, 0xBB},
	{0x32, 0x96},
	{0x33, 0xCB},
	{0x35, 0x65},
	{0x38, 0x00},
	{0x39, 0x04}
};

static const struct regval init_hd30[] = {
	{0x02, 0x42},
	{0x07, 0xC0},
	{0x0B, 0xC0},
	{0x0C, 0x13},
	{0x0D, 0x50},
	{0x15, 0x13},
	{0x16, 0x15},
	{0x17, 0x00},
	{0x18, 0x19},
	{0x19, 0xD0},
	{0x1A, 0x25},
	{0x1C, 0x06},
	{0x1D, 0x72},
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
	{0x35, 0x25},
	{0x38, 0x00},
	{0x39, 0x18}
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

static int tp2815_1080p25_init_cfg(struct v4l2_subdev *sd)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	unsigned long timer;
	u32 time_ms;
	int i;

	timer = jiffies;

	for (i = 0; i < ARRAY_SIZE(std_hda_fhd25); i++)
		tp2815_write_reg(sd, std_hda_fhd25[i].addr, std_hda_fhd25[i].val);

	time_ms = jiffies_to_msecs(jiffies - timer);

	dev_info(tp2815->dev, "%s done, cost %u ms", __func__, time_ms);

	return 0;
}

static int tp2815_720p25_init_cfg(struct v4l2_subdev *sd)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	unsigned long timer;
	u32 time_ms;
	int i;

	timer = jiffies;

	for (i = 0; i < ARRAY_SIZE(std_hda_hd25); i++)
		tp2815_write_reg(sd, std_hda_hd25[i].addr, std_hda_hd25[i].val);

	time_ms = jiffies_to_msecs(jiffies - timer);

	dev_info(tp2815->dev, "%s done, cost %u ms", __func__, time_ms);

	return 0;
}

static void tp2815_decoder_init(struct v4l2_subdev *sd,
	u8 ch, u8 fmt, u8 std)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	u8 tmp;
	const u8 sys_mode[5] = {0x01, 0x02, 0x04, 0x08, 0x0f};
	bool is_cvbs_960h = false;
	int i;

	if (ch > CH_ALL)
		return;

	dev_info(tp2815->dev, "decoder_init ch=%u fmt=%u\n std=%u\n", ch, fmt, std);

	tp2815_write_reg(sd, PAGE_SEL, ch);
	tp2815_write_reg(sd, 0x45, 0x01);
	tp2815_write_reg(sd, 0x06, 0x12); /* default value */
	tp2815_write_reg(sd, 0x27, 0x2d); /* default value */

	tmp = tp2815_read_reg(sd, 0xf5);
	tmp |= sys_mode[ch];
	tp2815_write_reg(sd, 0xf5, tmp);

	switch (fmt) {
	case PAL:
		if (is_cvbs_960h) {
			for (i = 0; i < ARRAY_SIZE(init_pal_960h); i++)
				tp2815_write_reg(sd, init_pal_960h[i].addr, init_pal_960h[i].val);
		} else {
			for (i = 0; i < ARRAY_SIZE(init_pal_720h); i++)
				tp2815_write_reg(sd, init_pal_720h[i].addr, init_pal_720h[i].val);
		}
		break;
	case NTSC:
		if (is_cvbs_960h) {
			for (i = 0; i < ARRAY_SIZE(init_ntsc_960h); i++)
				tp2815_write_reg(sd, init_ntsc_960h[i].addr, init_ntsc_960h[i].val);
		} else {
			for (i = 0; i < ARRAY_SIZE(init_ntsc_720h); i++)
				tp2815_write_reg(sd, init_ntsc_720h[i].addr, init_ntsc_720h[i].val);
		}
		break;
	case HD30:
		for (i = 0; i < ARRAY_SIZE(init_hd30); i++)
			tp2815_write_reg(sd, init_hd30[i].addr, init_hd30[i].val);

		if (std == STD_HDA) {
			tp2815_write_reg(sd, 0x02, 0x46);
			tp2815_write_reg(sd, 0x0D, 0x70);
			tp2815_write_reg(sd, 0x18, 0x1B);
			tp2815_write_reg(sd, 0x20, 0x40);
			tp2815_write_reg(sd, 0x21, 0x46);
			tp2815_write_reg(sd, 0x25, 0xFE);
			tp2815_write_reg(sd, 0x26, 0x01);
			tp2815_write_reg(sd, 0x2C, 0x3A);
			tp2815_write_reg(sd, 0x2D, 0x5A);
			tp2815_write_reg(sd, 0x2E, 0x40);
			tp2815_write_reg(sd, 0x30, 0x9D);
			tp2815_write_reg(sd, 0x31, 0xCA);
			tp2815_write_reg(sd, 0x32, 0x01);
			tp2815_write_reg(sd, 0x33, 0xD0);
		}
		break;
	default:
		break;
	}
}

static void tp2815_mipi_out(struct v4l2_subdev *sd, u8 output)
{
	struct tp2815 *tp2815 = to_tp2815(sd);

	tp2815_write_reg(sd, 0x40, MIPI_PAGE);
	tp2815_write_reg(sd, 0x01, 0xF0);
	tp2815_write_reg(sd, 0x02, 0x01);
	tp2815_write_reg(sd, 0x08, 0x0F);

	switch (output) {
	case MIPI_2CH4LANE_594M:
	case MIPI_4CH4LANE_594M:
		tp2815_write_reg(sd, 0x20, 0x44);

		if (output == MIPI_2CH4LANE_594M)
			tp2815_write_reg(sd, 0x20, 0x24);

		tp2815_write_reg(sd, 0x34, 0xE4);
		tp2815_write_reg(sd, 0x15, 0x0C);
		tp2815_write_reg(sd, 0x25, 0x08);
		tp2815_write_reg(sd, 0x26, 0x06);
		tp2815_write_reg(sd, 0x27, 0x11);
		tp2815_write_reg(sd, 0x29, 0x0A);
		tp2815_write_reg(sd, 0x33, 0x07);
		tp2815_write_reg(sd, 0x33, 0x00);
		tp2815_write_reg(sd, 0x14, 0x33);
		tp2815_write_reg(sd, 0x14, 0xB3);
		tp2815_write_reg(sd, 0x14, 0x33);
		break;
	case MIPI_4CH4LANE_297M:
	case MIPI_2CH4LANE_297M:
		tp2815_write_reg(sd, 0x20, 0x44);
		tp2815_write_reg(sd, 0x20, 0x44);

		if (output == MIPI_2CH4LANE_297M)
			tp2815_write_reg(sd, 0x20, 0x24);

		tp2815_write_reg(sd, 0x34, 0xE4);
		tp2815_write_reg(sd, 0x14, 0x44);
		tp2815_write_reg(sd, 0x15, 0x0D);
		tp2815_write_reg(sd, 0x25, 0x04);
		tp2815_write_reg(sd, 0x26, 0x03);
		tp2815_write_reg(sd, 0x27, 0x09);
		tp2815_write_reg(sd, 0x29, 0x02);
		tp2815_write_reg(sd, 0x33, 0x0F);
		tp2815_write_reg(sd, 0x33, 0x00);
		tp2815_write_reg(sd, 0x14, 0xC4);
		tp2815_write_reg(sd, 0x14, 0x44);
		break;
	case MIPI_1CH4LANE_297M:
		tp2815_write_reg(sd, 0x20, 0x14);
		tp2815_write_reg(sd, 0x34, 0x01);
		tp2815_write_reg(sd, 0x14, 0x44);
		tp2815_write_reg(sd, 0x15, 0x0D);
		tp2815_write_reg(sd, 0x25, 0x04);
		tp2815_write_reg(sd, 0x26, 0x03);
		tp2815_write_reg(sd, 0x27, 0x09);
		tp2815_write_reg(sd, 0x29, 0x02);
		tp2815_write_reg(sd, 0x33, 0x0F);
		tp2815_write_reg(sd, 0x33, 0x00);
		tp2815_write_reg(sd, 0x14, 0xC4);
		tp2815_write_reg(sd, 0x14, 0x44);
		break;
	default:
		break;
	}

	/* set skip_frame before setting output enabled */
	if (tp2815->std == STD_SD_480I || tp2815->std == STD_SD_576I) {
		u8 tmp;

		tmp = tp2815_read_reg(sd, 0x21);
		tmp |= (0x04 | 0x10 | 0x20); /* CH1 | CH2 | CH3 */
		tp2815_write_reg(sd, 0x21, tmp);

		tmp = tp2815_read_reg(sd, 0x04);
		tmp |= 0x08; /* CH4 */
		tp2815_write_reg(sd, 0x04, tmp);
	}

	/* Enable MIPI CSI2 output */
	tp2815_write_reg(sd, 0x23, 0x02);
	tp2815_write_reg(sd, 0x23, 0x00);
	tp2815_write_reg(sd, PAGE_SEL, CH_ALL);
}

static int tp2815_sensor_init_cfg(struct v4l2_subdev *sd,
		u8 ch, u8 fmt, u8 std, u8 output)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	unsigned long timer;
	u32 time_ms;

	timer = jiffies;

	/* Disable MIPI CSI2 output */
	tp2815_write_reg(sd, PAGE_SEL, MIPI_PAGE);
	tp2815_write_reg(sd, 0x23, 0x02);

	tp2815_decoder_init(sd, ch, fmt, std);
	tp2815_mipi_out(sd, output);
	tp2815_write_reg(sd, PAGE_SEL, CH_1);

	/* Enable MIPI CSI2 output */
	tp2815_write_reg(sd, PAGE_SEL, MIPI_PAGE);
	tp2815_write_reg(sd, 0x23, 0x00);

	time_ms = jiffies_to_msecs(jiffies - timer);

	dev_info(tp2815->dev, "%s done, cost %u ms", __func__, time_ms);

	return 0;
}

static int tp2815_format_cfg(struct v4l2_subdev *sd)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	int ret;
	int i;
	bool detected;

	switch (tp2815->std) {
	case STD_1080P30:
		ret = tp2815_1080p30_init_cfg(sd);
		break;
	case STD_1080P25:
		ret = tp2815_1080p25_init_cfg(sd);
		break;
	case STD_720P30:
		ret = tp2815_sensor_init_cfg(sd, CH_ALL, HD30, STD_HDA, MIPI_4CH4LANE_594M);
		break;
	case STD_720P25:
		ret = tp2815_720p25_init_cfg(sd);
		break;
	case STD_SD_480I:
		ret = tp2815_sensor_init_cfg(sd, CH_ALL, NTSC, STD_TVI, MIPI_4CH4LANE_594M);
		break;
	case STD_SD_576I:
		ret = tp2815_sensor_init_cfg(sd, CH_ALL, PAL, STD_TVI, MIPI_4CH4LANE_594M);
		break;
	default:
		ret = -ENXIO;
		break;
	}

	if (ret)
		goto exit;

	msleep(500);

	for (i = 0; i < 4; i++) {
		detected = tp2815_signal_detect(sd);

		if (detected)
			break;

		msleep(50);
	}

exit:
	return ret;
}

static int tp2815_reset_channel(struct v4l2_subdev *sd, int ch)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	int ret;

	dev_dbg(tp2815->dev, "Resetting CH%d\n", ch);

	/* Power off */
	ret = tp2815_s_power(sd, 0);
	if (ret < 0) {
		dev_err(tp2815->dev, "CH%d: power off failed\n", ch);
		return ret;
	}

	usleep_range(15000, 16000);

	/* Power on */
	ret = tp2815_s_power(sd, 1);
	if (ret < 0) {
		dev_err(tp2815->dev, "CH%d: power on failed\n", ch);
		return ret;
	}

	msleep(50);

	/* Re-initialize */
	ret = tp2815_format_cfg(sd);
	if (ret < 0) {
		dev_err(tp2815->dev, "CH%d: re-init failed\n", ch);
		return ret;
	}

	return 0;
}

static void tp2815_notify_status_event(struct v4l2_subdev *sd,
	u8 ch, u8 st, u8 pre_st, u8 dis_ch)
{
	struct v4l2_event ev;
	struct tp2815_event_payload payload;
	struct tp2815 *tp2815 = to_tp2815(sd);
	u8 cur_status, prev_status;
	int ch_index;

	memset(&ev, 0, sizeof(ev));
	memset(&payload, 0, sizeof(payload));

	if (st == TP2815_RESETTING) {
		cur_status = VIN_ALL_SIGNALS_NONE;
		prev_status = pre_st;
	} else if (st == TP2815_CHANNEL_DISABLED) {
		int i;

		cur_status = prev_status = VIN_ALL_SIGNALS_NONE;
		payload.disabled_ch_bits = dis_ch;
		for (i = 0; i < CH_ALL; i++) {
			if (dis_ch & (1 << i))
				payload.status[i] = st;
		}
	} else {
		cur_status = VIN_ALL_SIGNALS_READY;

		for (ch_index = 0; ch_index < CH_ALL; ch_index++) {
			if (tp2815->signal_status[ch_index] == VIN_SIGNAL_NONE)
				cur_status &= ~(VIN_SIGNAL_READY << ch_index);
		}

		prev_status = cur_status;
		if (tp2815->signal_status[ch] == VIN_SIGNAL_NONE)
			prev_status |= (VIN_SIGNAL_READY << ch);
		else
			prev_status &= ~(VIN_SIGNAL_READY << ch);
	}

	ev.type = RTK_V4L2_EVENT_SIGNAL_CHANGE;
	payload.nbytes = NUM_VALID_BYTES;
	payload.ch_cnt = TP2815_CH_COUNT;
	payload.sigbits = cur_status;
	payload.prev_sigbits = prev_status;
	/* Provide an exrta hint on ch0 for the user-space */
	if (st == TP2815_RESETTING)
		payload.status[0] = st;

	BUILD_BUG_ON(sizeof(payload) > sizeof(ev.u.data));
	memcpy(ev.u.data, &payload, sizeof(payload));

	dev_info(tp2815->dev, "Notify signal bits=%x\n", cur_status);

	v4l2_subdev_notify_event(sd, &ev);
}

/* Return the value that indexes the channels to reset */
static int tp2815_process_channel(struct tp2815 *tp2815,
	struct v4l2_subdev *sd, u8 ch)
{
	struct tp2815_ch_state *state = &tp2815->ch_state[ch];
	u8 pre_signal_status;
	u8 reg_01;
	bool signal_ok;
	int ret = 0;

	pre_signal_status = tp2815->signal_status[ch];

	tp2815_write_reg(sd, PAGE_SEL, ch);
	reg_01 = tp2815_read_reg(sd, 0x01);
	signal_ok = ((reg_01 & 0x60) == 0x60);

	switch (state->state) {
	case ST_SIGNAL_CHECK:
		if (signal_ok) {
			if (tp2815->signal_status[ch] != VIN_SIGNAL_READY)
				tp2815->signal_status[ch] = VIN_SIGNAL_READY;

			state->reset_cnt = 0;
			state->unstable_cnt = 0;
		} else {
			state->unstable_cnt++;
			if (state->unstable_cnt > 10) {
				tp2815->signal_status[ch] = VIN_SIGNAL_NONE;
				state->unstable_cnt = 0;
				if (tp2815->auto_reset_enabled) {
					state->state = ST_RESETTING;
					dev_info(tp2815->dev, "CH%d: entering reset state\n", ch);
				}
			}
		}
		break;

	case ST_RESETTING:
		/* Reset in custom_nosig_reset_store instead */
		break;

	case ST_DISABLED:
		if (signal_ok) {
			dev_info(tp2815->dev, "CH%d: signal recovered\n", ch);
			tp2815->signal_status[ch] = VIN_SIGNAL_READY;
			state->state = ST_SIGNAL_CHECK;
			state->unstable_cnt = 0;
			state->reset_cnt = 0;
		}
		break;

	default:
		state->state = ST_SIGNAL_CHECK;
		state->reset_cnt = 0;
		break;
	}

	if (state->state == ST_RESETTING)
		ret = 1 << ch; /* E.g 1 for ch1(index 0), 8 for ch4(index 3), etc. */
	else if (pre_signal_status != tp2815->signal_status[ch])
		tp2815_notify_status_event(sd, ch, tp2815->signal_status[ch],
			pre_signal_status, 0 /* unused */);

	return ret;
}

static void tp2815_detect_work(struct work_struct *work)
{
	struct tp2815 *tp2815 = container_of(work, struct tp2815,
					      detect_work.work);
	struct v4l2_subdev *sd = &tp2815->sd;
	u8 ch_index;
	u8 pre_all_status = VIN_ALL_SIGNALS_NONE;
	u8 notify_reset_channels = 0; /* No channel needs resetting by default */

	mutex_lock(&tp2815->detect_lock);
	/* Process each channel with state machine */
	for (ch_index = 0; ch_index < TP2815_CH_COUNT; ch_index++) {
		u8 pre_signal_status = tp2815->signal_status[ch_index];

		pre_all_status |= ((pre_signal_status ? VIN_SIGNAL_READY :
			VIN_SIGNAL_NONE) << ch_index);
		notify_reset_channels |= tp2815_process_channel(tp2815, sd, ch_index);
	}

	if (notify_reset_channels)
		tp2815_notify_status_event(sd, CH_ALL, TP2815_RESETTING,
			pre_all_status, 0 /* unused */);
	mutex_unlock(&tp2815->detect_lock);

	/* Schedule next detection based on mode */
	if (tp2815->detect_mode == DETECT_MODE_POLLING)
		schedule_delayed_work(&tp2815->detect_work, msecs_to_jiffies(100));
}



static int tp2815_subscribe_event(struct v4l2_subdev *sd,
		struct v4l2_fh *fh, struct v4l2_event_subscription *sub)
{
	struct tp2815 *tp2815 = to_tp2815(sd);
	int ret = -EINVAL;

	switch (sub->type) {
	case RTK_V4L2_EVENT_SIGNAL_CHANGE:
		dev_info(tp2815->dev, "Subscribe EVENT_SIGNAL_CHANGE\n");
		ret = v4l2_event_subscribe(fh, sub, 0, NULL);
		break;
	default:
		dev_err(tp2815->dev, "Unsupported event, type=%u", sub->type);
		break;
	}

	return ret;
}


static ssize_t channels_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret_count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tp2815 *tp2815 = to_tp2815(sd);
	unsigned char tp2815_status;
	int ch_index;

	tp2815_status = VIN_ALL_SIGNALS_READY;

	mutex_lock(&tp2815->detect_lock);
	/* Process each channel with state machine */
	for (ch_index = 0; ch_index < TP2815_CH_COUNT; ch_index++) {
		if (tp2815->signal_status[ch_index] == VIN_SIGNAL_NONE)
			tp2815_status = tp2815_status & ~(1<<ch_index);
	}
	mutex_unlock(&tp2815->detect_lock);

	ret_count += sprintf(buf + ret_count, "CHANNELSTATE=0x%x\n", tp2815_status);
	return ret_count;
}
static DEVICE_ATTR_RO(channels_state);

static ssize_t custom_nosig_reset_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct tp2815 *tp2815 = to_tp2815(sd);
	char *p;
	int len, ret, i;
	bool do_reset = false;
	/* Each status of all channels is not disabled or not after reset */
	u8 disabled_ch = 0;

	p = memchr(buf, '\n', count);
	len = p ? p - buf : count;

	if (strncmp(buf, "custom-reset", len))
		return -EINVAL;

	mutex_lock(&tp2815->detect_lock);
	for (i = 0; i < CH_ALL; i++) {
		struct tp2815_ch_state *state = &tp2815->ch_state[i];

		if (state->state == ST_RESETTING) {
			do_reset = true;
			break;
		}
	}

	if (!do_reset) {
		mutex_unlock(&tp2815->detect_lock);
		return -EBUSY;
	}

	ret = tp2815_reset_channel(sd, CH_ALL);

	for (i = 0; i < CH_ALL; i++) {
		struct tp2815_ch_state *state = &tp2815->ch_state[i];

		tp2815->signal_status[i] = VIN_SIGNAL_NONE;
		if (state->state == ST_RESETTING) {
			state->reset_cnt++;

			if (state->reset_cnt >= 3) {
				dev_warn(tp2815->dev,
				  "[tp2815] CH%d: disabled after 3 time\n", i);
				state->state = ST_DISABLED;
				state->reset_cnt = 0;
				disabled_ch |= 1 << i; /* ch-i: disabld */
			} else {
				state->state = ST_SIGNAL_CHECK;
			}
		} else if (state->state == ST_DISABLED) {
			disabled_ch |= 1 << i; /* ch-i: disabld */
		}
	}

	if (disabled_ch) {
		tp2815_notify_status_event(sd, CH_ALL, TP2815_CHANNEL_DISABLED,
			0 /* unused */, disabled_ch);
	}

	mod_delayed_work(system_wq, &tp2815->detect_work, msecs_to_jiffies(100));
	mutex_unlock(&tp2815->detect_lock);

	if (ret < 0) {
		dev_err(tp2815->dev, "custom reset FAIL!!\n");
		return ret;
	}

	return count;
}

static ssize_t custom_nosig_reset_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct tp2815 *tp2815 = to_tp2815(sd);
	int i;

	buf[0] = '\0';
	mutex_lock(&tp2815->detect_lock);
	for (i = 0; i < CH_ALL; i++) {
		struct tp2815_ch_state *state = &tp2815->ch_state[i];

		snprintf(buf+i*6, 32, "|ch%d:%d", i&0x3, state->state&0x3);
	}
	mutex_unlock(&tp2815->detect_lock);

	snprintf(buf+CH_ALL*6, 32, "|\n");

	return strnlen(buf, PAGE_SIZE);
}
/* Do not change the permission 0664 and ignore any warnings */
static DEVICE_ATTR(custom_nosig_reset, 0664, custom_nosig_reset_show, custom_nosig_reset_store);

static int tp2815_sysfs_create(struct v4l2_subdev *sd)
{
	int ret;
	struct tp2815 *tp2815 = to_tp2815(sd);

	ret = device_create_file(tp2815->dev, &dev_attr_channels_state);
	if (ret)
		dev_err(tp2815->dev, "create sysfs channels_state failed: %d\n", ret);

	ret = device_create_file(tp2815->dev, &dev_attr_custom_nosig_reset);
	if (ret)
		dev_err(tp2815->dev, "create sysfs custom_nosig_reset failed: %d\n", ret);

	return ret;
}

static void tp2815_sysfs_remove(struct v4l2_subdev *sd)
{
	struct tp2815 *tp2815 = to_tp2815(sd);

	device_remove_file(tp2815->dev, &dev_attr_channels_state);
	device_remove_file(tp2815->dev, &dev_attr_custom_nosig_reset);
}

static int tp2815_setup_polling_mode(struct tp2815 *tp2815)
{
	struct v4l2_subdev *sd = &tp2815->sd;

	/* The standard might be set from video-input-config and
	 * should be determined before detetion-polling is started
	 */
	if (tp2815->std > STD_SD_576I) {
		u8 sd_fmt_ctl, npxl_h, npxl_l;
		/* Check CH_2 only because there is no dt-prop for each CH */
		tp2815_write_reg(sd, PAGE_SEL, CH_2);

		/* comb-filter and sd-format control, reset value:0x50 */
		sd_fmt_ctl = tp2815_read_reg(sd, 0x0D);
		/* npxl, to compare values with N_PIXELS_LINE(h, l) */
		npxl_h = tp2815_read_reg(sd, 0x1C); /* reset: 0x06 */
		npxl_l = tp2815_read_reg(sd, 0x1D); /* reset: 0x72 */

		dev_info(tp2815->dev, "Comb-filter/sd-fmt=0x%02x npxl=0x%02x%02x\n",
				sd_fmt_ctl, npxl_h, npxl_l);

		/* sd_fmt_ctl & 0x20: true for HD, otherwise for SD */
		if (sd_fmt_ctl == 0x72 && N_PIXELS_LINE(0x08, 0x98))
			tp2815->std = STD_1080P30;
		else if (sd_fmt_ctl == 0x73 && N_PIXELS_LINE(0x0A, 0x50))
			tp2815->std = STD_1080P25;
		else if (sd_fmt_ctl == 0x70 && N_PIXELS_LINE(0x06, 0x72))
			tp2815->std = STD_720P30;
		else if (sd_fmt_ctl == 0x71 && N_PIXELS_LINE(0x07, 0xBC))
			tp2815->std = STD_720P25;
		else if (sd_fmt_ctl == 0x50 && N_PIXELS_LINE(0x06, 0xB4))
			tp2815->std = STD_SD_480I;
		else if (sd_fmt_ctl == 0x51 && N_PIXELS_LINE(0x06, 0xC0))
			tp2815->std = STD_SD_576I;
		else
			dev_dbg(tp2815->dev, "Polling setup: std is not set\n");

		if (tp2815->std < STD_UNDEFINED)
			dev_info(tp2815->dev, "Configured standard=%s is found\n",
				str_cam_std[tp2815->std & 0x7]);

	} else {
		dev_info(tp2815->dev, "Polling setup standard=%s\n",
				str_cam_std[tp2815->std & 0x7]);
	}

	tp2815_signal_detect(sd);

	return 0;
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
	int ret = 0;

	if ((val != STD_720P60) && (val != STD_720P50) &&
		(val != STD_1080P30) && (val != STD_1080P25) &&
		(val != STD_720P30) && (val != STD_720P25) &&
		(val != STD_SD_480I) && (val != STD_SD_576I)) {
		ret = -EINVAL;
		goto exit;
	}

	if (tp2815 == NULL) {
		ret = -EFAULT;
		goto exit;
	}

	tp2815->std = (u32)val;

	if (tp2815->rst_gpio != NULL) {
		tp2815_s_power(&tp2815->sd, false);
		msleep(500);
		tp2815_s_power(&tp2815->sd, true);
	}

	ret = tp2815_format_cfg(sd);

exit:
	return ret;
}

static int tp2815_format_get(void *data, u64 *val)
{
	struct tp2815 *tp2815 = data;
	int ret;

	if (tp2815 == NULL || val == NULL) {
		ret = -EFAULT;
		goto exit;
	}

	*val = tp2815->std;

	ret = 0;
exit:
	return ret;
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
	int ret = 0;

	if ((tp2815 == NULL) || (format == NULL)) {
		ret = -EFAULT;
		goto exit;
	}

	switch (tp2815->std) {
	case STD_1080P30:
	case STD_1080P25:
		format->format.width = 1920;
		format->format.height = 1080;
		break;
	case STD_720P60:
	case STD_720P50:
	case STD_720P30:
	case STD_720P25:
		format->format.width = 1280;
		format->format.height = 720;
		break;
	case STD_SD_480I:
		format->format.width = 720;
		format->format.height = 480;
		format->format.field = V4L2_FIELD_SEQ_TB;
		break;
	case STD_SD_576I:
		format->format.width = 720;
		format->format.height = 576;
		format->format.field = V4L2_FIELD_SEQ_TB;
		break;
	default:
		format->format.width = 0;
		format->format.height = 0;
		ret = -ENXIO;
		break;
	}

	dev_dbg(tp2815->dev, "%s %ux%u\n", __func__,
		format->format.width, format->format.height);

exit:
	return ret;
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
	.subscribe_event = tp2815_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
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

static void tp2815_s_detection(struct tp2815 *tp2815)
{
	if (tp2815->detect_mode == DETECT_MODE_POLLING) {
		tp2815_setup_polling_mode(tp2815);
		schedule_delayed_work(&tp2815->detect_work, msecs_to_jiffies(100));
	} else {
		cancel_delayed_work_sync(&tp2815->detect_work);
	}
}

static int tp2815_s_ctrl(struct v4l2_ctrl *c)
{
	struct tp2815 *tp2815 = container_of(c->handler, struct tp2815, hdl);

	switch (c->id) {
	case V4L2_CID_RTK_CAMERA_VIDEOSIGNAL_DETECT:
		tp2815->detect_mode = c->val;
		dev_info(tp2815->dev, "S_CTRL detect_mode=%d\n", c->val);
		tp2815_s_detection(tp2815);
		break;
	case V4L2_CID_RTK_CAMERA_VIDEOSIGNAL_AUTORESET:
		tp2815->auto_reset_enabled = c->val;
		dev_info(tp2815->dev, "S_CTRL auto_reset_enabled=%d\n", c->val);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops tp2815_ctrl_ops = {
	.s_ctrl = tp2815_s_ctrl,
};

static struct v4l2_ctrl_config tp2815_detect_mode = {
	.ops = &tp2815_ctrl_ops,
	.id = V4L2_CID_RTK_CAMERA_VIDEOSIGNAL_DETECT,
	.name = "signal_detection_mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
};

static struct v4l2_ctrl_config tp2815_auto_reset = {
	.ops = &tp2815_ctrl_ops,
	.id = V4L2_CID_RTK_CAMERA_VIDEOSIGNAL_AUTORESET,
	.name = "auto_reset",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
};

static int tp2815_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct tp2815 *tp2815;
	int ret;
	int i;

	tp2815 = devm_kzalloc(dev, sizeof(*tp2815), GFP_KERNEL);
	if (!tp2815)
		return -ENOMEM;

	tp2815->i2c_client = client;
	tp2815->dev = dev;

	/* Initialize camera channel status and state machine */
	for (i = 0; i < TP2815_CH_COUNT; i++) {
		tp2815->signal_status[i] = VIN_SIGNAL_NONE;
		tp2815->ch_state[i].state = ST_SIGNAL_CHECK;
		tp2815->ch_state[i].unstable_cnt = 0;
		tp2815->ch_state[i].reset_cnt = 0;
	}
	mutex_init(&tp2815->detect_lock);

	tp2815->rst_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(tp2815->rst_gpio)) {
		dev_err(dev, "reset gpio doesn't exist\n");
		tp2815->rst_gpio = NULL;
	} else {
		dev_info(dev, "Reset gpio=%d\n", desc_to_gpio(tp2815->rst_gpio));
	}

	ret = of_property_read_u32(dev->of_node, "camera-std",
				&tp2815->std);
	if (ret < 0 || tp2815->std > STD_SD_576I) {
		dev_info(dev, "Camera standard is undefined, ret=%d\n", ret);
		tp2815->std = STD_UNDEFINED;
	} else {
		dev_info(dev, "Camera standard=%s\n", str_cam_std[tp2815->std & 0x7]);
	}

	/* Read detection mode from device tree */
	ret = of_property_read_u32(dev->of_node, "detect-mode", &tp2815->detect_mode);
	if (ret < 0) {
		/* Default to disabled mode if not specified */
		tp2815->detect_mode = DETECT_MODE_DISABLED;
		dev_info(dev, "Detect-mode not specified, detection disabled\n");
	} else {
		/* Validate detection mode */
		if (tp2815->detect_mode != DETECT_MODE_DISABLED &&
		    tp2815->detect_mode != DETECT_MODE_POLLING) {
			dev_warn(dev, "Invalid detect-mode=%d, using disabled mode\n",
				 tp2815->detect_mode);
			tp2815->detect_mode = DETECT_MODE_DISABLED;
		}
	}

	ret = v4l2_device_register(tp2815->dev, &tp2815->v4l2_dev);
	if (ret) {
		dev_err(tp2815->dev, "Failed to register v4l2 device, ret=%d\n", ret);
		goto free_gpio;
	}

	v4l2_i2c_subdev_init(&tp2815->sd, client, &tp2815_subdev_ops);
	tp2815->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE|V4L2_SUBDEV_FL_HAS_EVENTS;

	v4l2_ctrl_handler_init(&tp2815->hdl, 2);
	v4l2_ctrl_new_custom(&tp2815->hdl, &tp2815_detect_mode, NULL);
	v4l2_ctrl_new_custom(&tp2815->hdl, &tp2815_auto_reset, NULL);

	if (tp2815->hdl.error) {
		ret = tp2815->hdl.error;
		goto free_gpio;
	}

	tp2815->sd.ctrl_handler = &tp2815->hdl;

	ret = v4l2_device_register_subdev(&tp2815->v4l2_dev, &tp2815->sd);
	if (ret) {
		dev_err(tp2815->dev, "Failed to register v4l2 subdev, ret=%d\n", ret);
		goto free_hdl;
	}

	ret = v4l2_device_register_subdev_nodes(&tp2815->v4l2_dev);
	if (ret) {
		dev_err(tp2815->dev, "Failed to register v4l2 subdev node, ret=%d\n", ret);
		goto free_hdl;
	}

	if (tp2815->std < STD_UNDEFINED && tp2815->rst_gpio != NULL) {
		/* s_power always returns 0 */
		tp2815_s_power(&tp2815->sd, false);
		/* techpoint-cn suggests that 10ms should be enough and
		 * we raise up to 15ms for the tolerance.
		 */
		usleep_range(15000, 16000);
		tp2815_s_power(&tp2815->sd, true);

		ret = tp2815_format_cfg(&tp2815->sd);
		if (ret < 0) {
			dev_err(tp2815->dev, "Init with std=%d failed\n", tp2815->std);
			goto free_hdl;
		}
	}

	/* Initialize delayed work for camera signal detection */
	INIT_DELAYED_WORK(&tp2815->detect_work, tp2815_detect_work);

	/* Setup detection mode */
	if (tp2815->detect_mode == DETECT_MODE_POLLING) {
		ret = tp2815_setup_polling_mode(tp2815);
		if (ret) {
			dev_err(dev, "Failed to setup polling mode\n");
			goto free_hdl;
		}
		/* Start detection work for polling mode */
		schedule_delayed_work(&tp2815->detect_work, msecs_to_jiffies(500));
	} else {
		dev_info(dev, "Camera signal detection is disabled\n");
	}

	tp2815_sysfs_create(&tp2815->sd);

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		tp2815_setup_dbgfs(tp2815);

	dev_info(dev, "TP2815 probe completed successfully\n");

	return 0;

free_hdl:
	v4l2_ctrl_handler_free(&tp2815->hdl);

free_gpio:
	if (tp2815->rst_gpio)
		devm_gpiod_put(dev, tp2815->rst_gpio);

	return ret;
}

static void tp2815_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tp2815 *tp2815 = to_tp2815(sd);

	tp2815_sysfs_remove(sd);
	/* Cancel delayed work if detection is enabled */
	if (tp2815->detect_mode != DETECT_MODE_DISABLED)
		cancel_delayed_work_sync(&tp2815->detect_work);

	v4l2_ctrl_handler_free(&tp2815->hdl);

	/* Destroy mutex */
	mutex_destroy(&tp2815->detect_lock);
}

static const struct of_device_id tp2815_of_match[] = {
	{ .compatible = "techpoint,tp2815" },
	{ }
};

MODULE_DEVICE_TABLE(of, tp2815_of_match);

static struct i2c_driver tp2815_driver = {
	.probe = tp2815_probe,
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
