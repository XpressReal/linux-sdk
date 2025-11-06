/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 RealTek Inc
 */
#ifndef _RTK_EDP_H
#define _RTK_EDP_H

#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <drm/display/drm_dp_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>

struct rtk_edp {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct drm_dp_aux aux;
	struct reset_control *rstc_edp;
	struct reset_control *rstc_edptx;
	struct reset_control *rstc_disp;
	struct reset_control *rstc_vo;
	struct clk *clk_edp;
	struct clk *clk_edptx;
	struct gpio_desc *hpd_gpio;
	struct gpio_desc *vcc_gpio;
	struct rtk_rpc_info *rpc_info;
	struct rtk_rpc_info *rpc_info_vo;
	struct regmap *iso_base;
	struct regmap *reg_base;
	struct regmap *crt_reg_base;
	struct regmap *edp_wrapper_reg_base;
	int aux_irq;
	int hpd_irq;
	unsigned int assr_en;
	struct completion comp;
	const struct rtk_edp_data *edp_data;
	unsigned int mixer;
	uint8_t rx_cap[DP_RECEIVER_CAP_SIZE];
	uint8_t link_status[DP_LINK_STATUS_SIZE];
	struct task_struct *hpd_thread;
	bool connected;
	bool finished_training;
	struct mutex lock;
	struct drm_dp_desc desc;
	struct edid *edid;
	int link_rate;
	unsigned int lane_count;
	int bpc;
	bool is_autotest;
	bool is_fallback_mode;
	struct delayed_work hpd_gpio_work;
	int color_format;
	bool check_connector_limit;
	uint32_t max_clock_k;
#ifdef CONFIG_CHROME_PLATFORMS
	struct work_struct retrain_link_work;
#endif
};

struct rtk_edp_train_signal {
	uint8_t swing;
	uint8_t emphasis;
};

void rtk_edp_write(struct rtk_edp *edp, u32 reg, u32 val);
void rtk_edp_update(struct rtk_edp *edp, u32 reg, u32 clear, u32 bits);
void rtk_edp_crt_write(struct rtk_edp *edp, u32 reg, u32 val);
void rtk_edp_crt_update(struct rtk_edp *edp, u32 reg, u32 clear, u32 bits);
void rtk_edp_wrap_update(struct rtk_edp *edp, u32 reg, u32 clear, u32 bits);


#endif /* _RTK_EDP_H */
