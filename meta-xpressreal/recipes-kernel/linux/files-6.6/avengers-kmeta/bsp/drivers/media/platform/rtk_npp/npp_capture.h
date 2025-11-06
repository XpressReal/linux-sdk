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

#ifndef __NPP_CAPTURE_H__
#define __NPP_CAPTURE_H__

#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>
#include "drv_npp.h"

struct npp_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head link;
};

struct npp_capture_info {
	/* Destination */
	int width;
	int height;
	int stride;
	int type;
	int format;
	dma_addr_t paddr;
	uint8_t *vaddr;
	int crop_x;
	int crop_y;
	int crop_width;
	int crop_height;
};

enum {
	WRITEBACK_TYPE_VSYNC_V1,
};

enum npprpc_writeback_fmt_e {
	NPP_RGB_PL,
	NPP_RGB_PACKED = 4
};

enum npp_buffer_status {
	DEFAULT,
	USING,
	IDLE,
	ABNORMAL
};

struct npp_fmt_ops_capture {
	int (*npp_enum_fmt_cap)(struct v4l2_fmtdesc *f);
	int (*npp_enum_framesize)(struct v4l2_frmsizeenum *f);
	int (*npp_g_fmt)(struct v4l2_fh *fh, struct v4l2_format *f);
	int (*npp_try_fmt_cap)(struct v4l2_fh *fh, struct v4l2_format *f);
	int (*npp_s_fmt_cap)(struct v4l2_fh *fh, struct v4l2_format *f);
	int (*npp_queue_info)(struct vb2_queue *vq, int *bufcnt, int *sizeimage);
	int (*npp_start_streaming)(struct vb2_queue *q, unsigned int count);
	int (*npp_stop_streaming)(struct vb2_queue *q);
	int (*npp_qbuf)(struct videc_dev *dev, struct v4l2_fh *fh, struct vb2_buffer *vb
		, unsigned char format, int bufferNum);
	int (*npp_g_crop)(void *fh, struct v4l2_rect *rect);
	int (*npp_s_crop)(void *fh, struct v4l2_selection *sel);
};

const struct npp_fmt_ops_capture *get_npp_fmt_ops_capture(void);
int npp_object_dma_allcate_capture(struct drv_npp_ctx *ctx, int bufferNum);
void npp_object_dma_free_capture(struct drv_npp_ctx *ctx);
void *npp_alloc_context_capture(struct v4l2_fh *fh, struct videc_dev *v_dev);
void npp_free_context_capture(void *ctx);

int npp_rpc_buffer_allocate(struct videc_dev *dev);
int npp_ringbuffer_buffer_allocate(struct videc_dev *dev);
void npp_rpc_buffer_release(struct videc_dev *dev);
void npp_ringbuffer_buffer_release(struct videc_dev *dev);
int npp_rpc_init_check(struct videc_dev *dev);
int npp_initialize(struct videc_dev *dev);
int npp_destroy(struct videc_dev *dev);
int npp_run_start(struct videc_dev *dev);
int npp_run_stop(struct videc_dev *dev);
int npp_querydisplaywin(int rpc_opt);

#endif
