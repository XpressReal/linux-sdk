/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 RealTek Inc
 */
#ifndef __RTK_PRINCE_DPTX_PHY_H__
#define __RTK_PRINCE_DPTX_PHY_H__

#include "rtk_dptx.h"

/**
 * DisplayPort
 */
void rtk_dptx_update(struct regmap *reg_base, u32 reg, u32 clear, u32 bits);
void rtk_dptx_write(struct regmap *reg_base, u32 reg, u32 val);
unsigned int rtk_dptx_read(struct regmap *reg_base, u32 reg);

// void rtk_prince_dptx_phy_disable_dppll_setting(struct rtk_prince_dptx *dptx);
int rtk_prince_dptx_phy_dppll_setting(struct rtk_prince_dptx *dptx,
	 struct drm_display_mode *mode);
void rtk_prince_dptx_csc_setting(struct rtk_prince_dptx *dptx);
void rtk_prince_dptx_phy_config_lane(struct rtk_prince_dptx *dptx);
void rtk_prince_dptx_phy_config_video_timing(struct rtk_prince_dptx *dptx,
	struct drm_display_mode *mode);
void rtk_prince_dptx_phy_set_scramble(struct rtk_prince_dptx *dptx, bool scramble);
void rtk_prince_dptx_phy_set_pattern(struct rtk_prince_dptx *dptx, int pattern);
void rtk_prince_dptx_phy_start_video(struct rtk_prince_dptx *dptx,
	struct drm_display_mode *mode);
void rtk_prince_dptx_phy_disable_timing_gen(struct rtk_prince_dptx *dptx);

/**
 * e DisplayPort
 */
int rtk_prince_edptx_phy_dppll_setting(struct rtk_prince_dptx *dptx,
	struct drm_display_mode *mode);
void rtk_prince_edp_phy_config_video_timing(struct rtk_prince_dptx *dptx,
	struct drm_display_mode *mode);
void rtk_prince_edp_phy_set_scramble(struct rtk_prince_dptx *dptx, bool scramble);
void rtk_prince_edp_phy_set_pattern(struct rtk_prince_dptx *dptx, int pattern);
void rtk_prince_edp_phy_config_csc(struct rtk_prince_dptx *dptx);
void rtk_prince_edp_phy_start_video(struct rtk_prince_dptx *dptx,
	struct drm_display_mode *mode);
void rtk_prince_edp_phy_disable_timing_gen(struct rtk_prince_dptx *dptx);
int rtk_prince_dptx_combo_phy_setting(struct rtk_prince_dptx *dptx, struct drm_display_mode *mode);

#endif /* __RTK_PRINCE_DPTX_PHY_H__ */
