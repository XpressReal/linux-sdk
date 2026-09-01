/*
 * Realtek RPC driver
 *
 * Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/ioctl.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/fdtable.h>
#include <linux/ratelimit.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/kmemleak.h>
#include <linux/dma-buf.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <uapi/linux/ion.h>

#include <soc/realtek/rtk_refclk.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/kernel-rpc.h>
#include <soc/realtek/uapi/ion_rtk.h>

#include "rpc.h"

enum ENUM_AUDIO_KERNEL_RPC_CMD {
	ENUM_KERNEL_RPC_CREATE_AGENT,   // 0
	ENUM_KERNEL_RPC_INIT_RINGBUF,
	ENUM_KERNEL_RPC_PRIVATEINFO,
	ENUM_KERNEL_RPC_RUN,
	ENUM_KERNEL_RPC_PAUSE,
	ENUM_KERNEL_RPC_SWITCH_FOCUS,   // 5
	ENUM_KERNEL_RPC_MALLOC_ADDR,
	ENUM_KERNEL_RPC_VOLUME_CONTROL,      // AUDIO_CONFIG_COMMAND
	ENUM_KERNEL_RPC_FLUSH,               // AUDIO_RPC_SENDIO
	ENUM_KERNEL_RPC_CONNECT,             // AUDIO_RPC_CONNECTION
	ENUM_KERNEL_RPC_SETREFCLOCK,    // 10     // AUDIO_RPC_REFCLOCK
	ENUM_KERNEL_RPC_DAC_I2S_CONFIG,      // AUDIO_CONFIG_DAC_I2S
	ENUM_KERNEL_RPC_DAC_SPDIF_CONFIG,    // AUDIO_CONFIG_DAC_SPDIF
	ENUM_KERNEL_RPC_HDMI_OUT_EDID,       // AUDIO_HDMI_OUT_EDID_DATA
	ENUM_KERNEL_RPC_HDMI_OUT_EDID2,      // AUDIO_HDMI_OUT_EDID_DATA2
	ENUM_KERNEL_RPC_HDMI_SET,       // 15     // AUDIO_HDMI_SET
	ENUM_KERNEL_RPC_HDMI_MUTE,           //AUDIO_HDMI_MUTE_INFO
	ENUM_KERNEL_RPC_ASK_DBG_MEM_ADDR,
	ENUM_KERNEL_RPC_DESTROY,
	ENUM_KERNEL_RPC_STOP,
	ENUM_KERNEL_RPC_CHECK_READY,     // 20    // check if Audio get memory pool from AP
	ENUM_KERNEL_RPC_GET_MUTE_N_VOLUME,   // get mute and volume level
	ENUM_KERNEL_RPC_EOS,
	ENUM_KERNEL_RPC_ADC0_CONFIG,
	ENUM_KERNEL_RPC_ADC1_CONFIG,
	ENUM_KERNEL_RPC_ADC2_CONFIG,    // 25
#if defined(AUDIO_TV_PLATFORM)
	ENUM_KERNEL_RPC_BBADC_CONFIG,
	ENUM_KERNEL_RPC_I2SI_CONFIG,
	ENUM_KERNEL_RPC_SPDIFI_CONFIG,
#endif // AUDIO_TV_PLATFORM
	ENUM_KERNEL_RPC_HDMI_OUT_VSDB,
	ENUM_VIDEO_KERNEL_RPC_CONFIG_TV_SYSTEM,
	ENUM_VIDEO_KERNEL_RPC_CONFIG_HDMI_INFO_FRAME,
	ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_WIN,
	ENUM_VIDEO_KERNEL_RPC_PP_INIT_PIN,
	ENUM_KERNEL_RPC_INIT_RINGBUF_AO, //need check this enum
	ENUM_VIDEO_KERNEL_RPC_VOUT_EDID_DATA,
	ENUM_KERNEL_RPC_AUDIO_POWER_SET,
	ENUM_VIDEO_KERNEL_RPC_VOUT_VDAC_SET,
	ENUM_VIDEO_KERNEL_RPC_QUERY_CONFIG_TV_SYSTEM,
	ENUM_KERNEL_RPC_AUDIO_CONFIG,
	ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
	ENUM_KERNEL_RPC_QUERY_FW_DEBUG_INFO,
	ENUM_KERNEL_RPC_HDMI_RX_LATENCY_MEM,
	ENUM_KERNEL_RPC_EQ_CONFIG,
};


enum AUDIO_ENUM_PRIVAETINFO {
	ENUM_PRIVATEINFO_AUDIO_FORMAT_PARSER_CAPABILITY = 0,
	ENUM_PRIVATEINFO_AUDIO_DECODER_CAPABILITY = 1,
	ENUM_PRIVATEINFO_AUDIO_CONFIG_CMD_BS_INFO = 2,
	ENUM_PRIVATEINFO_AUDIO_CHECK_LPCM_ENDIANESS = 3,
	ENUM_PRIVATEINFO_AUDIO_CONFIG_CMD_AO_DELAY_INFO = 4,
	ENUM_PRIVATEINFO_AUDIO_AO_CHANNEL_VOLUME_LEVEL = 5,
	ENUM_PRIVATEINFO_AUDIO_GET_FLASH_PIN = 6,
	ENUM_PRIVATEINFO_AUDIO_RELEASE_FLASH_PIN = 7,
	ENUM_PRIVATEINFO_AUDIO_GET_MUTE_N_VOLUME = 8,
	ENUM_PRIVATEINFO_AUDIO_AO_MONITOR_FULLNESS = 9,
	ENUM_PRIVATEINFO_AUDIO_CONTROL_FLASH_VOLUME = 10,
	ENUM_PRIVATEINFO_AUDIO_CONTROL_DAC_SWITCH = 11,
	ENUM_PRIVATEINFO_AUDIO_PREPROCESS_CONFIG = 12,
	ENUM_PRIVATEINFO_AUDIO_CHECK_SECURITY_ID = 13,
	ENUM_PRIVATEINFO_AUDIO_LOW_DELAY_PARAMETERS = 14,
	ENUM_PRIVATEINFO_AUDIO_SET_NETWORK_JITTER = 15,
	ENUM_PRIVATEINFO_AUDIO_GET_QUEUE_DATA_SIZE = 16,
	ENUM_PRIVATEINFO_AUDIO_GET_SHARE_MEMORY_FROM_ALSA = 17,
	ENUM_PRIVATEINFO_AUDIO_AI_CONNECT_ALSA = 18,
	ENUM_PRIVATEINFO_AUDIO_SET_PCM_FORMAT = 19,
	ENUM_PRIVATEINFO_AUDIO_DO_SELF_DESTROY_FLOW = 20,
	ENUM_PRIVATEINFO_AUDIO_GET_SAMPLING_RATE = 21,
	ENUM_PRIVATEINFO_AUDIO_SLAVE_TIMEOUT_THRESHOLD = 22,
	ENUM_PRIVATEINFO_AUDIO_GET_GLOBAL_AO_INSTANCEID = 23,
	ENUM_PRIVATEINFO_AUDIO_SET_CEC_PARAMETERS = 24,
	ENUM_PRIVATEINFO_AUDIO_INIT_DBG_DUMP_MEM = 25,
	ENUM_PRIVATEINFO_AUDIO_AI_GET_AO_FLASH_PIN = 26,
	ENUM_PRIVATEINFO_AUDIO_AI_SET_AO_FLASH_PIN = 27,
	ENUM_PRIVATEINFO_AUDIO_GET_PP_FREE_PINID = 28,
	ENUM_PRIVATEINFO_AUDIO_HDMI_RX_CONNECT_TO_BT = 29,
	ENUM_PRIVATEINFO_AUDIO_GET_BS_ERR_RATE = 30,
	ENUM_PRIVATEINFO_AUDIO_SET_RESUME_IR_KEYS = 31,
	ENUM_PRIVATEINFO_SET_GSTREAMER_PTS_ACC_MODE = 32,
	ENUM_PRIVATEINFO_AUDIO_GET_BONDING_TYPE = 33,
	ENUM_PRIVATEINFO_AUDIO_SHARE_MEMORY_FOR_PORTING_FIRMWARE = 34,
	ENUM_PRIVATEINFO_AUDIO_SET_DVDPLAYER_AO_VERSION = 35,
	ENUM_PRIVATEINFO_AUDIO_MS_PP_CERT = 36,
	ENUM_PRIVATEINFO_AUDIO_TRIGGER_EVENT = 37,
	ENUM_PRIVATEINFO_AUDIO_AI_NON_PCM_IN = 38,
	ENUM_PRIVATEINFO_OMX_AUDIO_VERSION = 39,
	ENUM_PRIVATEINFO_AUDIO_AI_PAD_IN = 40,
	ENUM_PRIVATEINFO_AUDIO_MS_MAJOR_DECODER_PIN = 41,
	ENUM_PRIVATEINFO_AUDIO_PROVIDE_RAWOUT_LATENCY = 42,
	ENUM_PRIVATEINFO_AUDIO_MS_MIXER_IGNORE_PIN = 43,
	ENUM_PRIVATEINFO_AUDIO_MS_CERTIFICATION_PLATFORM = 44,
	ENUM_PRIVATEINFO_AUDIO_MS_MIXER_PIN_NEW_SEG = 45,
	ENUM_PRIVATEINFO_AUDIO_MS_DEC_DROP_BY_PTS = 46,
	ENUM_PRIVATEINFO_AUDIO_MS_DEC_INIT_PTS_OFFSET = 47,
	ENUM_PRIVATEINFO_AUDIO_MS_PP_OUTPUT_TYPE = 48,
	ENUM_PRIVATEINFO_AUDIO_DTS_ENCODER_CONFIG = 49,
	ENUM_PRIVATEINFO_AUDIO_GET_FW_VERSION = 50,
	ENUM_PRIVATEINFO_AUDIO_DTS_M8_IN_CONFIG = 51,
	ENUM_PRIVATEINFO_AUDIO_DTS_M8_LA_NUM = 52,
	ENUM_PRIVATEINFO_AUDIO_DTS_M8_SET_OUTPUT_FORMAT = 53,
	ENUM_PRIVATEINFO_AUDIO_SET_DRC_CFG = 54,
	ENUM_PRIVATEINFO_AUDIO_DTS_M8_LA_ERROR_MSG = 55,
	ENUM_PRIVATEINFO_GET_B_VALUE = 56,
	ENUM_PRIVATEINFO_AUDIO_ENTER_SUSPEND = 57,
	ENUM_PRIVATEINFO_AUDIO_MPEGH_IN_CONFIG = 58,
	ENUM_PRIVATEINFO_AUDIO_SET_LOW_WATERLEVEL = 59,
};

struct AUDIO_RPC_PRIVATEINFO_PARAMETERS {
	int instanceID;
	enum AUDIO_ENUM_PRIVAETINFO type;
	volatile int privateInfo[16];
};

struct AUDIO_RPC_PRIVATEINFO_RETURNVAL {
	int instanceID;
	volatile int privateInfo[16];
};

#define S_OK        0x10000000

static int rpc_notify_acpu_fw_destroy_process(int pid, phys_addr_t paddr, void *vaddr)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned int offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset_io(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	cmd->instanceID = htonl(-1);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_DO_SELF_DESTROY_FLOW);
	cmd->privateInfo[0] = htonl(pid);

	if (send_rpc_command(RPC_AUDIO,
				ENUM_KERNEL_RPC_PRIVATEINFO,
				dat, dat + offset,
				&RPC_ret)) {
		pr_err("[RPC] %s send RPC failed!\n", __func__);
	}

	if (RPC_ret != S_OK) {
		pr_err("[RPC] %s received RPC failed!\n", __func__);
		goto exit;
	}

exit:

	return ret;
}

static int rpc_notify_vcpu_fw_destroy_process(int pid, phys_addr_t paddr, void *vaddr)
{
	uint32_t *pid_info;
	uint32_t *res;
	uint32_t RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned int offset;

	pid_info = (uint32_t *)vaddr;
	dat = paddr;
	offset = get_rpc_alignment_offset(sizeof(uint32_t));
	res = (uint32_t *)(pid_info + offset);
	memset_io(pid_info, 0, sizeof(uint32_t));
	*pid_info =  htonl(pid);

	if (send_rpc_command(RPC_VIDEO,
				170,
				dat, dat + offset,
				&RPC_ret)) {
		pr_err("[RPC] %s send RPC failed!\n", __func__);
	}

	if (RPC_ret != S_OK) {
		pr_err("[RPC] %s received RPC failed!\n", __func__);
		goto exit;
	}
exit:

	return ret;
}

static int rpc_notify_hifi_fw_destroy_process(int pid, phys_addr_t paddr, void *vaddr)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned int offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128);
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset_io(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	cmd->instanceID = -1;
	cmd->type = ENUM_PRIVATEINFO_AUDIO_DO_SELF_DESTROY_FLOW;
	cmd->privateInfo[0] = pid;

	if (send_rpc_command(RPC_HIFI,
				ENUM_KERNEL_RPC_PRIVATEINFO,
				dat, dat + offset,
				&RPC_ret)) {
		pr_err("[RPC] %s send RPC failed!\n", __func__);
	}

	if (RPC_ret != S_OK) {
		pr_err("[RPC] %s received RPC failed!\n", __func__);
		goto exit;
	}

exit:

	return ret;
}

int rpc_notify_fw_destroy_process(struct rpc_client *client,
	    int pid, phys_addr_t paddr, void *vaddr)
{
	if (client->id == RPC_AUDIO)
		return rpc_notify_acpu_fw_destroy_process(pid, paddr, vaddr);
	else if (client->id == RPC_VIDEO)
		return rpc_notify_vcpu_fw_destroy_process(pid, paddr, vaddr);
	else if (client->id == RPC_HIFI)
		return rpc_notify_hifi_fw_destroy_process(pid, paddr, vaddr);
	else
		pr_err("[RPC] %s failed! for %s\n", __func__, client->name);
	return -ENODEV;
}
