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

#ifndef __NPP_M2M_H__
#define __NPP_M2M_H__

#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>
#include "drv_npp.h"

struct npp_fmt_ops_m2m {
	int (*npp_enum_fmt_cap)(struct v4l2_fmtdesc *f);
	int (*npp_enum_fmt_out)(struct v4l2_fmtdesc *f);
	int (*npp_enum_framesize)(struct v4l2_frmsizeenum *f);
	int (*npp_g_fmt)(struct v4l2_fh *fh, struct v4l2_format *f);
	int (*npp_try_fmt_cap)(struct v4l2_fh *fh, struct v4l2_format *f);
	int (*npp_try_fmt_out)(struct v4l2_fh *fh, struct v4l2_format *f);
	int (*npp_s_fmt_cap)(struct v4l2_fh *fh, struct v4l2_format *f);
	int (*npp_s_fmt_out)(struct v4l2_fh *fh, struct v4l2_format *f);
	int (*npp_queue_info)(struct vb2_queue *vq, int *bufcnt, int *sizeimage);
	int (*npp_start_streaming)(struct vb2_queue *q, unsigned int count);
	int (*npp_stop_streaming)(struct vb2_queue *q);
	int (*npp_qbuf)(struct v4l2_fh *fh, struct vb2_buffer *vb);
	int (*npp_abort)(void *priv, int type);
	int (*npp_g_crop)(void *fh, struct v4l2_rect *rect);
	int (*npp_s_crop)(void *fh, struct v4l2_selection *sel);
};

const struct npp_fmt_ops_m2m *get_npp_fmt_ops_m2m(void);
int npp_object_dma_allcate_m2m(struct drv_npp_ctx *ctx, int type);
void npp_object_dma_free_m2m(struct drv_npp_ctx *ctx, int type);
void *npp_alloc_context_m2m(struct v4l2_fh *fh, struct videc_dev *dev);
void npp_free_context_m2m(struct videc_dev *dev, void *ctx);
#ifdef ENABLE_NPP_MEASURE_TIME
void npp_time_measure(void *ctx);
#endif

#endif
