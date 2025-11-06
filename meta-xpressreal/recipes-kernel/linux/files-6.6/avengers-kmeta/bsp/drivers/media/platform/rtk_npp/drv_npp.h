/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek npu_pp video m2m and capture v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#ifndef __DRV_NPP_IF_H__
#define __DRV_NPP_IF_H__

#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include "npp_common.h"

#define xstr(s) str(s)
#define str(s) #s

#define RTK_V4L2_SINGLE_RGB_PLANE		(V4L2_CID_USER_REALTEK_BASE + 1)
#define RTK_V4L2_NPP_SVP				(V4L2_CID_USER_REALTEK_BASE + 2)
#define RTK_V4L2_MIPI_CSI_PARMS_CONFIG	(V4L2_CID_USER_REALTEK_BASE + 3)
#define RTK_V4L2_SIGNED_RGB_OUTPUT	    (V4L2_CID_USER_REALTEK_BASE + 4)

//Attentation!This value should not be included in v4l2_ctrl_type
#define V4L2_CTRL_TYPE_RTK_NPP_MIPI_CSI_PARAM 0x9000

// For stark
#define RTK_NN_CONTROL						 0x8400ff40
// For kent
#define SIP_NN_OP						0x82000600

#define NPU_PP_OP_DISABLE_PROT_MODE		0x0
#define NPU_PP_OP_ENABLE_PROT_MODE		 0x1
#define NPU_PP_OP_HOLD_RESET			0xf2
#define NPU_PP_OP_RELEASE_RESET			0xf3


enum DEVICE_STATE {
	AVAILABLE,
	M2MSTATE,
	CAPTURESTATE
};

struct videc_dev {
	struct video_device video_dev_capture;
	struct video_device video_dev_m2m;
	struct mutex dev_mutex_capture;
	struct mutex dev_mutex_m2m;
	unsigned int dev_open_cnt_capture;
	unsigned int dev_open_cnt_m2m;

	struct v4l2_device	v4l2_dev;
	struct device		*dev;

	spinlock_t irqlock;
	struct v4l2_m2m_dev	*m2m_dev;
	struct npp_ctx *nppctx_activate;
	int irq;
	struct mutex hw_mutex;
	struct semaphore hw_sem;

	spinlock_t device_spin_lock;
	enum DEVICE_STATE state;
	enum TARGET_SOC target_soc;
	int rpc_opt;
	bool npp_activate;
};

/* must be modified by npp */
struct rtk_mipi_csi_params {
	uint8_t mipi_csi_input;
	uint8_t in_queue_metadata;
	unsigned int data_pitch;
	unsigned int header_pitch;
	unsigned int y_header_size;
	unsigned int y_data_size;
	unsigned int c_header_size;
	unsigned int c_data_size;
};

struct rtk_mipi_csi_metadata {
	uint64_t y_header_phyaddr;
	uint64_t y_data_phyaddr;
	uint64_t c_header_phyaddr;
	uint64_t c_data_phyaddr;
};

struct vid_params {
	uint8_t npp_svp;
	uint8_t single_rgb_plane;
	struct rtk_mipi_csi_params mipi_csi_param;
	uint8_t signed_rgb_output;
};

struct drv_npp_ctx {
	struct v4l2_fh fh;
	struct videc_dev *dev;
	void *file;		/* struct file */
	void *npp_ctx;		/* context of npp */
	struct v4l2_ctrl_handler	ctrls;
	struct vid_params		params;

	struct vb2_queue	   queue;

	enum DEVICE_STATE state;
	int bufferNum;
};


extern char *type_str[];
extern char *mem_str[];
#endif
