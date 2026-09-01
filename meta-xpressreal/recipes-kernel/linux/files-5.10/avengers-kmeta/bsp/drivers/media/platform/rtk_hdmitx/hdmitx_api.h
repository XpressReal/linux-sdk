/*
 * hdmitx_api.h - RTK hdmitx driver header file
 *
 * Copyright (C) 2017 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HDMITX_API_H__
#define __HDMITX_API_H__

#include "hdmitx_dev.h"
#include "hdmitx.h"
#include "hdmitx_rpc.h"

extern unsigned int hdmi_clk_always_on;
extern unsigned int dp_or_dsi_exist;

int init_hdmitx_rpc_info(struct device *dev,
		struct hdmitx_rpc_info *rpc_info);
int rpc_toagent_set_display_format(struct device *dev,
	struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT *output_fmt);
int rpc_toagent_get_display_format(struct device *dev,
	struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT *output_fmt);
int RPC_TOAGENT_HDMI_Config_TV_System(
		hdmitx_device_t *tx_dev,
		struct edid_information *hdmitx_edid_info,
		struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *arg);
int RPC_TOAGENT_HDMI_Config_AVI_Info(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_CONFIG_HDMI_INFO_FRAME *arg);
int RPC_TOAGENT_HDMI_Set(hdmitx_device_t *tx_dev,
		int opt, struct AUDIO_HDMI_SET *arg);
int RPC_TOAGENT_HDMI_Mute(hdmitx_device_t *tx_dev,
		int opt, struct AUDIO_HDMI_MUTE_INFO *arg);
int RPC_TOAGENT_HDMI_OUT_VSDB(hdmitx_device_t *tx_dev,
		int opt, struct AUDIO_HDMI_OUT_VSDB_DATA *arg);
int RPC_ToAgent_HDMI_OUT_EDID_0(hdmitx_device_t *tx_dev,
		int opt, struct AUDIO_HDMI_OUT_EDID_DATA2 *arg);
int RPC_ToAgent_QueryDisplayWin_0(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT *arg);
int RPC_ToAgent_Vout_EDIDdata(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_EDID_DATA *arg);
int RPC_ToAgent_QueryConfigTvSystem(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *arg);
int RPC_ToAgent_SET_HDMI_VRR(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_HDMI_VRR *arg);

int hdmitx_check_same_edid(hdmitx_device_t *tx_dev);
int hdmitx_reset_sink_capability(asoc_hdmi_t *p_this);
int hdmitx_get_sink_capability(hdmitx_device_t *tx_dev);
void hdmitx_print_sink_capability(asoc_hdmi_t *p_this);
void hdmitx_print_sink_info(asoc_hdmi_t *p_this);
void hdmitx_enable_rxsense_int(hdmitx_device_t *tx_dev);
int hdmitx_get_rxsense(hdmitx_device_t *tx_dev);
int hdmitx_check_rx_sense(hdmitx_device_t *tx_dev);

int get_full_edid(hdmitx_device_t *tx_dev, struct full_edid *f_edid);
int set_fake_edid(hdmitx_device_t *tx_dev, struct fake_edid *fake);
int get_edid_block(hdmitx_device_t *tx_dev, struct block_edid *b_edid);

int ops_get_sink_cap(void __user *arg, hdmitx_device_t *tx_dev);
int ops_get_raw_edid(void __user *arg, hdmitx_device_t *tx_dev);
int ops_get_extension_blk_count(void __user *arg, hdmitx_device_t *tx_dev);
int ops_get_full_edid(void __user *arg, hdmitx_device_t *tx_dev);
int ops_get_link_status(void __user *arg, hdmitx_device_t *tx_dev);
int ops_get_video_config(void __user *arg, asoc_hdmi_t *data);
int ops_config_tv_system(void __user *arg, hdmitx_device_t *tx_dev);
int ops_config_avi_info(void __user *arg, hdmitx_device_t *tx_dev);
int ops_set_frequency(void __user *arg, hdmitx_device_t *tx_dev);
int ops_set_audio_mute(void __user *arg, hdmitx_device_t *tx_dev);
int ops_send_audio_vsdb_data(void __user *arg, hdmitx_device_t *tx_dev);
int ops_send_audio_edid2(void __user *arg, hdmitx_device_t *tx_dev);

int ops_send_AVmute(void __user *arg, hdmitx_device_t *tx_dev);
int ops_check_rx_sense(void __user *arg, void __iomem *base);
int ops_send_vout_edid_data(void __user *arg, hdmitx_device_t *tx_dev);
int ops_get_edid_support_list(void __user *arg, hdmitx_device_t *tx_dev);
int ops_set_output_format(void __user *arg, hdmitx_device_t *tx_dev);
int ops_get_output_format(void __user *arg, hdmitx_device_t *tx_dev);
int ops_set_interface_type(void __user *arg);
int ops_get_config_tv_system(void __user *arg, hdmitx_device_t *tx_dev);
int ops_set_hotplug_detection(void __user *arg, hdmitx_device_t *tx_dev);
int ops_wait_hotplug(void __user *arg, hdmitx_device_t *tx_dev);
int ops_get_edid_block(void __user *arg, hdmitx_device_t *tx_dev);
int ops_set_vrr(void __user *arg, hdmitx_device_t *tx_dev);
int ops_ctrl_5v(void __user *arg, hdmitx_device_t *tx_dev);
int ops_set_fake_edid(void __user *arg, hdmitx_device_t *tx_dev);
int ops_skip_edid_check(void __user *arg, hdmitx_device_t *tx_dev);

#endif  //__HDMITX_API_H__
