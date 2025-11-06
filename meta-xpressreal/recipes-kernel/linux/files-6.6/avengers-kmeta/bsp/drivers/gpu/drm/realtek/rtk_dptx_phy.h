/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 RealTek Inc
 */
#ifndef _RTK_DPTX_PHY_H
#define _RTK_DPTX_PHY_H

#include "rtk_dptx.h"

void rtk_dptx_phy_config_aphy(struct rtk_dptx *dptx);
int rtk_dptx_phy_dppll_setting(struct rtk_dptx *dptx,
			 struct drm_display_mode *mode, bool polarity);
void rtk_dptx_phy_config_lane(struct rtk_dptx *dptx);
void rtk_dptx_phy_config_video_timing(struct rtk_dptx *dptx,
			 struct drm_display_mode *mode, bool polarity);
void rtk_dptx_phy_set_scramble(struct rtk_dptx *dptx, bool scramble);
void rtk_dptx_phy_set_pattern(struct rtk_dptx *dptx, uint8_t pattern);
int rtk_dptx_mac_signal_setting(struct rtk_dptx *dptx,
			 struct rtk_dptx_train_signal signals[4]);
int rtk_dptx_aphy_signal_setting(struct rtk_dptx *dptx,
			 struct rtk_dptx_train_signal signals[4]);
void rtk_dptx_phy_start_video(struct rtk_dptx *dptx,
			 struct drm_display_mode *mode);
void rtk_dptx_phy_disable_timing_gen(struct rtk_dptx *dptx);
void rtk_dptx_phy_config_audio(struct rtk_dptx *dptx, struct audio_info *ainfo);

#endif /* _RTK_EDP_PHY_H */
