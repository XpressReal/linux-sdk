/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 RealTek Inc.
 */

#ifndef __RTK_PRINCE_DPTX_H__
#define __RTK_PRINCE_DPTX_H__

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <sound/hdmi-codec.h>

#include <drm/display/drm_dp_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>
#include <video/videomode.h>

#include "rtk_hdcp.h"

#define MAX_PHY (1)
#define RTK_DP_MAX_LANE_COUNT 4
#define MAX_VOLTAGE_SWING_LEVEL 3
#define MAX_EMPHASIS_LEVEL 3
#define MAX_CLOCK_RECOVERY_LOOP 5
#define MAX_EQUALIZER_LOOP 5

#define DPCD_ENHANCED_FRAME_CAP(x)		(((x) >> 7) & 0x1)
#define DPCD_LANE_COUNT_SET(x)			((x) & 0x1f)
#define DPCD_PRE_EMPHASIS_SET(x)		(((x) & 0x3) << 3)
#define DPCD_PRE_EMPHASIS_GET(x)		(((x) >> 3) & 0x3)
#define DPCD_VOLTAGE_SWING_SET(x)		(((x) & 0x3) << 0)

#define to_rtk_prince_dptx(x) container_of(x, struct rtk_prince_dptx, x)

#define DPCD_MAX_LANE_COUNT(x) ((x) & 0x1f)

/* 1920x1080@60 */
#define RTK_EDP_MAX_CLOCK_K 148500

enum dp_audio_format {
	DP_AUDIO_FMT_I2S = 0,
	DP_AUDIO_FMT_SPDIF = 1,
	DP_AUDIO_FMT_UNUSED,
};

struct dp_audio_info {
	enum dp_audio_format format;

	/**
	 * @sample_rate:
	 *
	 * 0 - 48k
	 * 1 - 44.1k
	 * 2 - 32k
	 * 3 - 96k
	 * 4 - 24k
	 * 5 - 22.05k
	 * 6 - 16k
	 * 7 - 64k
	 * 8 - 88.2k
	 * 9 - 192k
	 * 10 - 176.4k
	 * 11 - 8k
	 * 12 - 11.025k
	 * 13 - 12k
	 * 14 - 128k
	 * 15 - 384k
	 */
	int sample_rate;
	int channels;

	/**
	 * @sample_width:
	 *
	 * 0 - 16bit
	 * 1 - 18bit
	 * 2 - 20bit
	 * 3 - 24bit
	 * 4 - 32bit
	 */
	int sample_width;
};

enum link_lane_count {
	LINK_LANE_COUNT_1 = 1,
	LINK_LANE_COUNT_2 = 2,
	LINK_LANE_COUNT_4 = 4
};

enum link_training_state {
	START,
	CLOCK_RECOVERY,
	EQUALIZER_TRAINING,
	FINISHED,
	FAILED
};

struct video_info {
	// char *name;

	// bool h_sync_polarity;
	// bool v_sync_polarity;
	// bool interlaced;

	// enum color_space color_space;
	// enum dynamic_range dynamic_range;
	// enum color_coefficient ycbcr_coeff;
	// enum color_depth color_depth;

	int max_link_bw;
	enum link_lane_count max_lane_count;
};

struct link_train {
	int eq_loop;
	int cr_loop[4];
	int link_rate;

	u8 rx_link_bw;
	u8 rx_lane_count;

	u8 link_bw;
	u8 lane_count;
	u8 training_lane[4];

	enum link_training_state lt_state;
};

struct rtk_prince_dptx {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct drm_dp_aux aux;
	struct reset_control *rstc_dptx;
	struct reset_control *rstc_edptx;
	struct clk *clk_dptx;
	struct clk *clk_edptx;
	struct rtk_rpc_info *rpc_info;
	struct regmap *iso_sys_base;
	struct regmap *dptx14_reg_base;
	struct regmap *dptx14_mac_reg_base;
	struct regmap *dptx14_edp_reg_base;
	struct regmap *edp_wrapper_reg_base;
	struct regmap *crt_reg_base;
	struct mutex lock;
	bool connected;
	bool sink_has_audio;
	struct platform_device *audio_pdev;
	hdmi_codec_plugged_cb plugged_cb;
	struct device *codec_dev;
	unsigned int ports;
	const struct rtk_prince_dptx_platform_data *dptx_data;
	bool check_clock;
	int aux_irq;
	int hpd_irq;
	struct semaphore sem;
	unsigned int mixer;
	struct edid *edid;
	struct task_struct *hpd_thread;
	struct gpio_desc *dp5v_gpio;
	struct gpio_desc *hpd_gpio;

	struct video_info video_info;
	struct link_train link_train;
	bool fast_train_enable;

	uint8_t rx_cap[DP_RECEIVER_CAP_SIZE];
	uint8_t link_status[DP_LINK_STATUS_SIZE];
	struct drm_dp_desc desc;
	int test_train_pattern; /* debug mode */
	int bpc;
	bool is_autotest; /* CTS autotest or debug mode */
	struct delayed_work hpd_gpio_work;
	bool tx_lane_enabled[4]; // tx lane 0, lane 1...
	int color_format;
	bool check_connector_limit;
	uint32_t max_clock_k;

	struct drm_property *link_rate_property;
	struct drm_property *lane_count_property;
	struct drm_property *train_pattern_property;
	struct drm_property *dpcd_property;
	struct drm_property_blob *dpcd_blob;
	struct drm_property *link_status_property;
	struct drm_property_blob *link_status_blob;
	int prop_link_rate;
	unsigned int prop_lane_count;

	struct dp_audio_info dp_audio_info;
	struct rtk_hdcp hdcp;

	int connector_type;
	struct drm_info_list *debugfs_files;
};

struct rtk_prince_dptx_train_signal {
	uint8_t swing;
	uint8_t emphasis;
};

extern int rtk_dptx_add_force_modes(struct drm_connector *connector);
// int rtk_prince_dptx_hdcp_init(struct rtk_prince_dptx *dptx, u32 hdcp_support);

#endif /* __RTK_PRINCE_DPTX_H__ */
