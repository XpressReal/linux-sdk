/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 RealTek Inc
 */
#ifndef _RTK_EDP_PHY_H
#define _RTK_EDP_PHY_H

#include "rtk_edp.h"

void rtk_edp_phy_dppll_setting(struct rtk_edp *edp,
						 struct drm_display_mode *mode);
int rtk_edp_phy_pixelpll_setting(struct rtk_edp *edp,
						 struct drm_display_mode *mode);
void rtk_edp_phy_config_video_timing(struct rtk_edp *edp,
						 struct drm_display_mode *mode);
int rtk_edp_mac_signal_setting(struct rtk_edp *edp,
						 struct rtk_edp_train_signal signals[4]);
int rtk_edp_aphy_signal_setting(struct rtk_edp *edp,
						 struct rtk_edp_train_signal signals[4]);
void rtk_edp_phy_set_pattern(struct rtk_edp *edp, int pattern);
void rtk_edp_phy_set_scramble(struct rtk_edp *edp, bool scramble);
void rtk_edp_phy_config_csc(struct rtk_edp *edp);
void rtk_edp_phy_start_video(struct rtk_edp *edp,
						 struct drm_display_mode *mode);
void rtk_edp_phy_disable_timing_gen(struct rtk_edp *edp);

#endif /* _RTK_EDP_PHY_H */
