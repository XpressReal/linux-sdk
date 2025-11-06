/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2021 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */
#ifndef __DRV_IF_H__
#define __DRV_IF_H__

#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#define RTK_VDEC_VERSION "0.0.4"

#define RTK_V4L2_SET_SECURE 				(V4L2_CID_USER_REALTEK_BASE + 0)

// SW-6045, ve1_v4l2.c will use videc_dev, so move definition from drv_if.c to .h
struct videc_dev {
	struct v4l2_device	v4l2_dev;
	struct video_device	video_dev;

	atomic_t		num_inst;
	struct mutex		dev_mutex;
	spinlock_t		irqlock;

	struct v4l2_m2m_dev	*m2m_dev;
	struct device		*dev;
};

struct videc_params {
	uint8_t		is_secure;
	uint8_t		is_adaptive_playback;
};

struct videc_ctx {
	struct v4l2_fh fh;
	struct videc_dev *dev;

	void *file;		/* struct file */
	void *vpu_ctx;		/* context of vpu */
	void *ve_ctx;		/* context of video engine */

	struct v4l2_ctrl_handler	ctrls;
	struct videc_params		params;
};

#endif
