/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 RealTek Inc.
 */

#ifndef _RTK_DPTX_H
#define _RTK_DPTX_H

#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <sound/hdmi-codec.h>

#include <drm/drm_crtc_helper.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_encoder.h>
#include <video/videomode.h>

#define MAX_PHY (1)
#define RTK_DP_MAX_LANE_COUNT 4

enum audio_format {
	AUDIO_FMT_I2S = 0,
	AUDIO_FMT_SPDIF = 1,
	AUDIO_FMT_UNUSED,
};

struct audio_info {
	enum audio_format format;
	int sample_rate;
	int channels;
	int sample_width;
};

struct rtk_dptx_port {
	struct rtk_dptx *dptx;
	struct notifier_block event_nb;
	struct extcon_dev *extcon;
	struct phy *phy;
	u8 lanes;
	// bool phy_enabled;
	u8 id;
};

struct rtk_dptx {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct drm_dp_aux aux;
	struct reset_control *rstc_dptx;
	struct reset_control *rstc_misc;
	struct clk *clk_dptx;
	struct clk *clk_iso_misc;
	struct clk *clk_usb_p4;
	struct rtk_rpc_info *rpc_info;
	struct rtk_rpc_info *rpc_info_vo;
	struct regmap *iso_base;
	struct regmap *dptx14_reg_base;
	struct regmap *dptx14_mac_reg_base;
	struct regmap *dptx14_aphy_reg_base;
	struct regmap *crt_reg_base;
	struct rtk_dptx_port *port[MAX_PHY];
	struct work_struct event_work;
	struct mutex lock;
	bool connected;
	bool active;
	bool sink_has_audio;
	struct platform_device *audio_pdev;
	hdmi_codec_plugged_cb plugged_cb;
	struct device *codec_dev;
	unsigned int ports;
	const struct rtk_dptx_platform_data *dptx_data;
	bool check_clock;
	int aux_irq;
	int hpd_irq;
	struct semaphore sem;
	unsigned int mixer;
	struct edid *edid;
	struct task_struct *hpd_thread;
	struct gpio_desc *hpd_gpio;
	uint8_t rx_cap[DP_RECEIVER_CAP_SIZE];
	uint8_t link_status[DP_LINK_STATUS_SIZE];
	struct drm_dp_desc desc;
	int link_rate;
	unsigned int lane_count;
	int bpc;
	bool is_autotest;
	bool is_fallback_mode;
	struct delayed_work hpd_gpio_work;
	bool tx_lane_enabled[4]; // tx lane 0, lane 1...
	int color_format;
	bool check_connector_limit;
	uint32_t max_clock_k;
#ifdef CONFIG_CHROME_PLATFORMS
	struct work_struct retrain_link_work;
#endif

	struct audio_info audio_info;
};

struct rtk_dptx_train_signal {
	uint8_t swing;
	uint8_t emphasis;
};

void rtk_dptx14_update(struct rtk_dptx *dptx, u32 reg, u32 clear, u32 bits);
void rtk_dptx14_write(struct rtk_dptx *dptx, u32 reg, u32 val);
void rtk_dptx14_mac_update(struct rtk_dptx *dptx, u32 reg, u32 clear, u32 bits);
void rtk_dptx14_mac_write(struct rtk_dptx *dptx, u32 reg, u32 val);
void rtk_dptx14_aphy_update(struct rtk_dptx *dptx, u32 reg, u32 clear, u32 bits);
void rtk_dptx14_aphy_write(struct rtk_dptx *dptx, u32 reg, u32 val);
void rtk_dptx14_crt_update(struct rtk_dptx *dptx, u32 reg, u32 clear, u32 bits);
void rtk_dptx14_crt_write(struct rtk_dptx *dptx, u32 reg, u32 val);

#endif /* _RTK_DPTX_H */
