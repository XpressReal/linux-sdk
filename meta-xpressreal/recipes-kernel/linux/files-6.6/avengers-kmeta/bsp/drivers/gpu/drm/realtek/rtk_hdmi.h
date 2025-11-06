/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 RealTek Inc.
 */

#ifndef RTK_HDMI_H_
#define RTK_HDMI_H_

#include <drm/display/drm_scdc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>

#include <sound/hdmi-codec.h>

#include "rtk_drm_drv.h"
#include "rtk_hdcp.h"

#define to_rtk_hdmi(x) container_of(x, struct rtk_hdmi, x)

/* SCDS PB5 Flags */
#define SCDS_FAPA_END    (1 << 7)
#define SCDS_QMS         (1 << 6)
#define SCDS_M_DELTA     (1 << 5)
#define SCDS_CINEMA_VRR  (1 << 4)
#define SCDS_NEG_MVRR    (1 << 3)
#define SCDS_FVA         (1 << 2)
#define SCDS_ALLM        (1 << 1)
#define SCDS_FAPA_START  (1 << 0)

/* SCDS PB8 Flags */
#define SCDS_DSC_1P2     (1 << 7)
#define SCDS_DSC_NAT420  (1 << 6)
#define SCDS_QMS_TFR_MAX (1 << 5)
#define SCDS_QMS_TFR_MIN (1 << 4)
#define SCDS_DSC_ALL_BPP (1 << 3)
#define SCDS_DSC_16BPC   (1 << 2)
#define SCDS_DSC_12BPC   (1 << 1)
#define SCDS_DSC_10BPC   (1 << 0)

/* Delay 2s for execute HDCP work */
#define RTK_EDID_QUIRK_HDCP_DELAY_2S	(1 << 0)

/* Add modes for EDID not support format */
#define RTK_EDID_QUIRK_MODES_ADD		(1 << 8)
/* Remove modes from EDID support format */
#define RTK_EDID_QUIRK_MODES_RM			(1 << 9)

/* DTS Extension Flags, for of_property ext-flags */
#define RTK_EXT_DIS_4K			(1 << 0)
#define RTK_EXT_DIS_AVMUTE		(1 << 1)
#define RTK_EXT_CVBS_HPD		(1 << 2)
#define RTK_EXT_DIS_4KP30_UPPER	(1 << 3)
#define RTK_EXT_TMDS_PHY	(1 << 4)
#define RTK_EXT_SKIP_EDID_AUDIO	(1 << 5)

#define TMDS_PLL_NUM 29

/* HDMI_NEW_CHSWAP_CTRL_reg */
#define FLAG_CHCK_LANE3_SWAP   (0x800)
#define FLAG_CH2_LANE2_SWAP    (0x400)
#define FLAG_CH1_LANE1_SWAP    (0x200)
#define FLAG_CH0_LANE0_SWAP    (0x100)
#define FLAG_ALL_CH_LANE_SWAP  (0xF00)
#define CHSWAP_TYPE(type)      (0x1f&(type))

#define SWAP_TEMPLATE_DEMOBOARD (FLAG_ALL_CH_LANE_SWAP | CHSWAP_TYPE(20))
#define SWAP_TEMPLATE_R4 CHSWAP_TYPE(20)

struct hdmi_edid_eeodb {
	u8 blk_index;
	u8 override_count;
	u8 offset_in_blk;
	u8 size;
};

struct hdmi_edid_info {
	bool sink_is_hdmi;
	bool sink_has_audio;
	u32 max_tmds_char_rate;
	u8 scdc_capable;
	u8 dc_420;
	u8 colorimetry;
	u8 vcdb;
	u8 max_frl_rate;
	u8 scds_pb5;
	u8 scds_pb8;
	u8 vrr_min;
	u16 vrr_max;
	struct hdmi_edid_eeodb eeodb;
	u32 edid_quirks;
};

struct hdmi_tmds_phy {
	u8 pll_mode;
	u32 sd1;
	u32 ldo1;
	u32 ldo2;
	u32 ldo3;
	u32 ldo4;
	u32 ldo5;
	u32 ldo6;
	u32 ldo7;
	u32 ldo8;
	u32 ldo9;
	u32 ldo10;
	u32 ldo11;
	u32 pll_hdmi2;
	u32 pll_hdmi3;
	u32 pll_hdmi4;
	u32 kvco_res;
	u32 ps2_ckin_sel;
};

struct rtk_hdmi_pll_table {
	int mode;
	char *name;
};

struct pll_ldo_element {
	unsigned int em_en;
	unsigned int iem_level;
	unsigned int idrv;
	unsigned int empre_en;
	unsigned int iempre_level;
	unsigned int rsel;
	unsigned int ipdrv;
};

enum PLL_MODE {
	PLL_27MHZ = 0,
	PLL_27x1p25,
	PLL_27x1p5,
	PLL_54MHZ,
	PLL_54x1p25,
	PLL_54x1p5,
	PLL_59p4MHZ,
	PLL_59p4x1p25,
	PLL_59p4x1p5,
	PLL_74p25MHZ,
	PLL_74p25x1p25,
	PLL_74p25x1p5,
	PLL_83p5MHZ,
	PLL_106p5MHZ,
	PLL_108MHZ,
	PLL_146p25MHZ,
	PLL_148p5MHZ,
	PLL_148p5x1p25,
	PLL_148p5x1p5,
	PLL_297MHZ,
	PLL_297x1p25,
	PLL_297x1p5,
	PLL_348p5MHZ,
	PLL_594MHZ_420,
	PLL_594MHZ_420x1p25,
	PLL_594MHZ_420x1p5,
	PLL_453p456MHZ,
	PLL_486p048MHZ,
	PLL_594MHZ,
	PLL_INVALIDMHZ = 0xff
};

enum hdmi_ch_swap_type {
	SWAP_0_LANE_0_1_2_3,
	SWAP_1_LANE_3_1_2_0,
	SWAP_2_LANE_1_0_2_3,
	SWAP_3_LANE_3_0_2_1,
	SWAP_4_LANE_0_3_2_1,
	SWAP_5_LANE_1_3_2_0,
	SWAP_6_LANE_0_2_1_3,
	SWAP_7_LANE_3_2_1_0,
	SWAP_8_LANE_2_0_1_3,
	SWAP_9_LANE_3_0_1_2,
	SWAP_10_LANE_0_3_1_2,
	SWAP_11_LANE_2_3_1_0,
	SWAP_12_LANE_2_1_0_3,
	SWAP_13_LANE_3_1_0_2,
	SWAP_14_LANE_1_2_0_3,
	SWAP_15_LANE_3_2_0_1,
	SWAP_16_LANE_2_3_0_1,
	SWAP_17_LANE_1_3_0_2,
	SWAP_18_LANE_0_1_3_2,
	SWAP_19_LANE_2_1_3_0,
	SWAP_20_LANE_1_0_3_2,
	SWAP_21_LANE_2_0_3_1,
	SWAP_22_LANE_0_2_3_1,
	SWAP_23_LANE_1_2_3_0,
};

enum HDMI_5V_STATUS {
	HDMI_5V_DISABLE,
	HDMI_5V_ENABLE,
};

enum HDMI_AO_STATUS {
	HDMI_AO_DISABLE,
	HDMI_AO_ENABLE,
	HDMI_AO_AUTO,
};

enum ALLM_STATUS {
	ALLM_DISABLE = HDMI_ALLM_DISABLE,
	ALLM_ENABLE,
	ALLM_UNSUPPORTED,
};

enum QMS_VRR_STATUS {
	QMS_VRR_DISABLE = HDMI_VRR_DISABLE,
	QMS_VRR_EN_VRR,
	QMS_VRR_EN_QMS,
	QMS_VRR_UNSUPPORTED,
};

enum QMS_VRR_RATE {
	RATE_60HZ = HDMI_VRR_60HZ,
	RATE_50HZ,
	RATE_48HZ,
	RATE_24HZ,
	RATE_59HZ,
	RATE_47HZ,
	RATE_30HZ,
	RATE_29HZ,
	RATE_25HZ,
	RATE_23HZ,
	RATE_BASE,
	RATE_UNSPECIFIED,
};

enum Resolution_ID_CODE {
	RID_1280x720_16_9 = 1,
	RID_1280x720_64_27,
	RID_1680x720_64_27,
	RID_1920x1080_16_9,
	RID_1920x1080_64_27,
	RID_2560x1080_64_27,
	RID_3440x1080_32_9,
	RID_2560x1440_16_9,
	RID_3440x1440_64_27,
	RID_5120x1440_32_9,
	RID_3840x2160_16_9,
	RID_3840x2160_64_27,
	RID_5120x2160_64_27,
	RID_7680x2160_32_9,
	RID_5120x2880_16_9,
	RID_5120x2880_64_27,
	RID_6880x2880_64_27,
	RID_10240x2880_32_9,
	RID_7680x4320_16_9,
	RID_7680x4320_64_27,
	RID_10240x4320_64_27,
	RID_15360x4320_32_9,
	RID_11520x6480_16_9,
	RID_11520x6480_64_27,
	RID_15360x6480_64_27,
	RID_15360x8640_16_9,
	RID_15360x8640_64_27,
	RID_20480x8640_64_27,
};

enum OVT_RATE {
	OVT_FR_NO_DATA = 0,
	OVT_FR_23HZ,
	OVT_FR_24HZ,
	OVT_FR_25HZ,
	OVT_FR_29HZ,
	OVT_FR_30HZ,
	OVT_FR_47HZ,
	OVT_FR_48HZ,
	OVT_FR_50HZ,
	OVT_FR_59HZ,
	OVT_FR_60HZ,
	OVT_FR_100HZ,
	OVT_FR_119HZ,
	OVT_FR_120HZ,
	OVT_FR_143HZ,
	OVT_FR_144HZ,
	OVT_FR_200HZ,
	OVT_FR_239HZ,
	OVT_FR_240HZ,
	OVT_FR_300HZ,
	OVT_FR_359HZ,
	OVT_FR_360HZ,
	OVT_FR_400HZ,
	OVT_FR_479HZ,
	OVT_FR_480HZ,
	OVT_FR_RESERVED,
};

enum HDMI_RXSENSE_MODE {
	RXSENSE_PASSIVE_MODE = 0,
	RXSENSE_TIMER_MODE,
	RXSENSE_INTERRUPT_MODE,
};

enum HDMI_RXSENSE_STATUS {
	HDMI_RXSENSE_OFF = 0,
	HDMI_RXSENSE_ON,
	HDMI_RXSENSE_UNKNOWN,
};

enum HDMI_AUTO_MODE {
	HDMI_AUTO_DISABLE,
	HDMI_AUTO_SDR_8B,
	HDMI_AUTO_SDR_DC,
	HDMI_AUTO_INPUT,
};

enum SKIP_AVMUTE_STATUS {
	SKIP_DISABLE = 0,
	SKIP_ENABLE,
};

struct rtk_hdmi_ops {
	/* Update hotplug and RxSense state */
	bool (*update_hpd_state)(struct rtk_hdmi *hdmi);
	/* enable or disable audio output */
	int (*audio_output_ctrl)(struct rtk_hdmi *hdmi, u8 enable);
	/* Read one specific block(128bytes) of EDID via ddc */
	int (*read_edid_block)(struct rtk_hdmi *hdmi, u8 *buf, u8 block_index);
	/* set tmds phy parameters */
	int (*set_tmds_phy)(struct rtk_hdmi *hdmi, struct drm_display_mode *mode,
						struct rpc_display_output_format format);
};

struct rtk_hdmi {
	struct device *codec_dev;
	hdmi_codec_plugged_cb plugged_cb;
	struct device *dev;
	struct drm_device *drm_dev;
	struct extcon_dev *edev;
	struct extcon_dev *cvbs_edev;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct drm_display_mode previous_mode;

	struct reset_control *reset_hdmi;
	struct reset_control *reset_hdmitop;
	struct clk *clk_hdmi;
	struct clk *clk_hdmitop;
	struct regmap *crtreg;
	struct regmap *hdmireg;
	struct regmap *topreg;
	int hpd_irq;
	int rxsense_irq;
	u32 is_new_mac;
	u32 lr_scramble;
	u32 ao_in_hifi;
	u32 ext_flags;
	u32 swap_config;
	u32 max_clock_k;
	struct work_struct hpd_work;
	enum HDMI_RXSENSE_MODE rxsense_mode;
	struct timer_list rxsense_timer;

	const struct rtk_hdmi_ops *hdmi_ops;
	struct mutex hpd_lock;

	struct regulator *supply;
	struct gpio_desc *hdmi5v_gpio;
	struct gpio_desc *hpd_gpio;
	struct i2c_adapter *ddc;
	struct hdmi_edid_info edid_info;
	struct hdmi_tmds_phy tmds_phy[TMDS_PLL_NUM];
	int hpd_state;
	int rxsense_state;
	bool is_hdmi_on;
	bool is_force_on;
	bool in_suspend;
	bool format_changed;
	bool check_connector_limit;

	enum hdmi_colorspace rgb_or_yuv;
	struct drm_property *rgb_or_yuv_property;

	enum rtk_hdr_mode hdr_mode;
	struct drm_property *hdr_mode_property;

	enum HDMI_5V_STATUS en_hdmi_5v;
	struct drm_property *hdmi_5v_property;

	enum HDMI_AO_STATUS en_audio;
	struct drm_property *en_ao_property;

	enum ALLM_STATUS en_allm;
	struct drm_property *allm_property;

	enum QMS_VRR_STATUS en_qms_vrr;
	struct drm_property *qms_property;

	enum QMS_VRR_RATE vrr_rate;
	struct drm_property *vrr_rate_property;

	enum HDMI_AUTO_MODE auto_mode;
	enum HDMI_AUTO_MODE request_auto_mode;
	struct drm_property *auto_mode_property;

	enum SKIP_AVMUTE_STATUS skip_avmute;
	struct drm_property *skp_avmute_property;

	enum hdmi_scan_mode scan_mode;
	struct drm_property *scan_mode_property;

	struct rtk_rpc_info *rpc_info;
	struct rtk_rpc_info *rpc_info_ao;

	struct rtk_hdcp hdcp;
	u32 hdcp_support;

	struct cec_notifier *cec;
	struct edid *edid_cache;
};

/**
 *	frame_rate
 *	bit	7	6	5	4	3	2	1	0
 *					144	60	50	48	24
 *	frame_rate_factor
 *	bit	7	6	5	4	3	2	1	0
 *				8X	6X	4X	2X	1X	0.5X
 */
static const struct rtk_ovt_info {
	int rid;
	char frame_rate;
	char frame_rate_factor;
	struct drm_display_mode rtk_ovt_mode;
} rtk_ovt_info_list[] = {
	{ RID_2560x1080_64_27, 0x10, 0x02,
		{ DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 453456, 2560, 2616,
		   2648, 2680, 0, 1080, 1107, 1115, 1175, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27, } },
	{ RID_2560x1440_16_9, 0x08, 0x04,
		{ DRM_MODE("2560x1440", DRM_MODE_TYPE_DRIVER, 486048, 2560, 2592,
		   2624, 2656, 0, 1440, 1451, 1459, 1525, 0,
		   DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9, } },
};

extern u32 rtk_edid_get_quirks(struct rtk_hdmi *hdmi, const struct edid *edid);
extern u32 get_frame_per_10s(struct rtk_hdmi *hdmi, struct drm_display_mode *mode);
extern void rtk_parse_cea_ext(struct rtk_hdmi *hdmi, struct edid *edid);
extern int rtk_add_fractional_modes(struct drm_connector *connector, const struct drm_display_mode *mode);
extern int rtk_add_force_modes(struct drm_connector *connector);
extern int rtk_add_more_ext_modes(struct drm_connector *connector, struct edid *edid);
extern int rtk_add_ovt_modes(struct drm_connector *connector, struct edid *edid);
extern int rtk_add_quirk_modes(struct drm_connector *connector, struct edid *edid);

void rtk_content_type_commit_state(struct drm_connector *connector,
		unsigned int old_ct, unsigned int new_ct);
#endif /* RTK_HDMI_H_ */
