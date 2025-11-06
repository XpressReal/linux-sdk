// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek npu_pp video m2m and capture v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/kthread.h>  // for threads
#include <linux/time.h>   // for using jiffies
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/fdtable.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "drv_npp.h"
#include "npp_debug.h"
#include "npp_common.h"
#include "npp_m2m.h"
#include "npp_ctrl.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/dma-buf.h>
#include <linux/dma-map-ops.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <linux/arm-smccc.h>

#ifdef ENABLE_NPP_MEASURE_TIME
#include <linux/ktime.h>
#endif

static void npp_cap_buf_done(struct v4l2_fh *fh,
struct vb2_v4l2_buffer *buf, bool eos, enum vb2_buffer_state state);
static int npp_cap_dqbuf(void *fh, struct vb2_v4l2_buffer *v4l2_buf, uint32_t *len, uint64_t *pts);
static int npp_out_dqbuf(struct v4l2_fh *fh, struct vb2_v4l2_buffer *v4l2_buf);
static int npp_run(struct v4l2_fh *fh);

static struct npp_fmt npp_fmt_list[2][7] = {

	[NPP_FMT_TYPE_OUTPUT] = {
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_YUYV,
			.max_width = MAX_IMAGE_M2M_WIDTH,
			.min_width = MIN_IMAGE_M2M_WIDTH,
			.max_height = MAX_IMAGE_M2M_HEIGHT,
			.min_height = MIN_IMAGE_M2M_HEIGHT,
			.num_planes = 1,
		},
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_YVYU,
			.max_width = MAX_IMAGE_M2M_WIDTH,
			.min_width = MIN_IMAGE_M2M_WIDTH,
			.max_height = MAX_IMAGE_M2M_HEIGHT,
			.min_height = MIN_IMAGE_M2M_HEIGHT,
			.num_planes = 1,
		},
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_UYVY,
			.max_width = MAX_IMAGE_M2M_WIDTH,
			.min_width = MIN_IMAGE_M2M_WIDTH,
			.max_height = MAX_IMAGE_M2M_HEIGHT,
			.min_height = MIN_IMAGE_M2M_HEIGHT,
			.num_planes = 1,
		},
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_VYUY,
			.max_width = MAX_IMAGE_M2M_WIDTH,
			.min_width = MIN_IMAGE_M2M_WIDTH,
			.max_height = MAX_IMAGE_M2M_HEIGHT,
			.min_height = MIN_IMAGE_M2M_HEIGHT,
			.num_planes = 1,
		},
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_NV12,
			.max_width = MAX_IMAGE_M2M_WIDTH,
			.min_width = MIN_IMAGE_M2M_WIDTH,
			.max_height = MAX_IMAGE_M2M_HEIGHT,
			.min_height = MIN_IMAGE_M2M_HEIGHT,
			.num_planes = 1,
		},
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_NV21,
			.max_width = MAX_IMAGE_M2M_WIDTH,
			.min_width = MIN_IMAGE_M2M_WIDTH,
			.max_height = MAX_IMAGE_M2M_HEIGHT,
			.min_height = MIN_IMAGE_M2M_HEIGHT,
			.num_planes = 1,
		},
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_NV16,
			.max_width = MAX_IMAGE_M2M_WIDTH,
			.min_width = MIN_IMAGE_M2M_WIDTH,
			.max_height = MAX_IMAGE_M2M_HEIGHT,
			.min_height = MIN_IMAGE_M2M_HEIGHT,
			.num_planes = 1,
		},
	},
	[NPP_FMT_TYPE_CAPTURE] = {
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_RGB24,
			.max_width = MAX_IMAGE_M2M_WIDTH,
			.min_width = MIN_IMAGE_M2M_WIDTH,
			.max_height = MAX_IMAGE_M2M_HEIGHT,
			.min_height = MIN_IMAGE_M2M_HEIGHT,
			.num_planes = 1,
		},
	}
};

static struct npp_fmt *npp_find_fmt(unsigned int v4l2_pix_fmt,
						   enum NPP_FMT_TYPE type)
{
	unsigned int index;
	struct npp_fmt *fmt = NULL;

	for (index = 0; index < ARRAY_SIZE(npp_fmt_list[type]);
	     index++) {
		if (npp_fmt_list[type][index].v4l2_pix_fmt ==
		    v4l2_pix_fmt)
			fmt = &npp_fmt_list[type][index];
	}

	return fmt;
}

static struct npp_fmt *npp_find_fmt_by_idx(unsigned int idx, enum NPP_FMT_TYPE type)
{
	struct npp_fmt *fmt = NULL;

	if (idx >= ARRAY_SIZE(npp_fmt_list[type]))
		goto exit;

	if (!npp_fmt_list[type][idx].v4l2_pix_fmt)
		goto exit;

	fmt = &npp_fmt_list[type][idx];

exit:
	return fmt;
}

static void npp_update_pix_fmt(struct drv_npp_ctx *drvctx, struct v4l2_pix_format_mplane *pix_mp,
			  unsigned int width, unsigned int height)
{
	__u32 sizeimage = pix_mp->plane_fmt[0].sizeimage;
	__u32 bytesperline = pix_mp->plane_fmt[0].bytesperline;
	__u32 mipi_csi_block_in_row, mipi_csi_4_block_in_row,
	mipi_csi_4_block_max_size, mipi_csi_of_size_one_row;
	__u32 mipi_csi_of_header_size, mipi_csi_header_pitch,
	mipi_csi_y_header_size, mipi_csi_c_header_size;
	__u32 mipi_csi_y_body_size, mipi_csi_c_body_size;

	switch (pix_mp->pixelformat) {
	case V4L2_PIX_FMT_NV12:
		if ((drvctx->params.mipi_csi_param.mipi_csi_input == 1) ||
			(drvctx->params.mipi_csi_param.in_queue_metadata == 1)) {
			mipi_csi_block_in_row = (width + 15) / 16;
			mipi_csi_4_block_in_row = (mipi_csi_block_in_row + 3) / 4;
			mipi_csi_4_block_max_size = (4 * 64);
			mipi_csi_of_size_one_row = (mipi_csi_4_block_in_row *
			mipi_csi_4_block_max_size);
			mipi_csi_of_header_size = (mipi_csi_4_block_in_row * 8);
			mipi_csi_header_pitch = ((mipi_csi_of_header_size + 63) / 64) * 64;
			mipi_csi_y_header_size = ((height + 3) / 4) * mipi_csi_header_pitch;
			mipi_csi_y_body_size = ((height + 3) / 4) * mipi_csi_of_size_one_row;
			mipi_csi_c_header_size = (((height / 2) + 3) / 4) * mipi_csi_header_pitch;
			mipi_csi_c_body_size = (((height / 2) + 3) / 4) * mipi_csi_of_size_one_row;

			bytesperline = width;
			sizeimage = mipi_csi_y_header_size + mipi_csi_y_body_size +
			mipi_csi_c_header_size + mipi_csi_c_body_size;
			pr_info("%s, y_header_size = %d, c_header_size = %d\n",
				__func__, mipi_csi_y_header_size, mipi_csi_c_header_size);
			pr_info("%s, actual alloc sizeimage = %d, ideal sizeimage = %d\n",
				__func__, sizeimage,
				(width*height*3)/2 + mipi_csi_y_header_size+mipi_csi_c_header_size);
		} else {
			bytesperline = width;
			sizeimage = width*height*3/2;
		}
		break;
	case V4L2_PIX_FMT_NV21:
		if ((drvctx->params.mipi_csi_param.mipi_csi_input == 1) ||
			(drvctx->params.mipi_csi_param.in_queue_metadata == 1)) {
			mipi_csi_block_in_row = (width + 15) / 16;
			mipi_csi_4_block_in_row = (mipi_csi_block_in_row + 3) / 4;
			mipi_csi_4_block_max_size = (4 * 64);
			mipi_csi_of_size_one_row = (mipi_csi_4_block_in_row *
			mipi_csi_4_block_max_size);
			mipi_csi_of_header_size = (mipi_csi_4_block_in_row * 8);
			mipi_csi_header_pitch = ((mipi_csi_of_header_size + 63) / 64) * 64;
			mipi_csi_y_header_size = ((height + 3) / 4) * mipi_csi_header_pitch;
			mipi_csi_y_body_size = ((height + 3) / 4) * mipi_csi_of_size_one_row;
			mipi_csi_c_header_size = (((height / 2) + 3) / 4) * mipi_csi_header_pitch;
			mipi_csi_c_body_size = (((height / 2) + 3) / 4) * mipi_csi_of_size_one_row;

			bytesperline = width;
			sizeimage = mipi_csi_y_header_size + mipi_csi_y_body_size +
			mipi_csi_c_header_size + mipi_csi_c_body_size;
			pr_info("%s, y_header_size = %d, c_header_size = %d\n",
				__func__, mipi_csi_y_header_size, mipi_csi_c_header_size);
			pr_info("%s, actual alloc sizeimage = %d, ideal sizeimage = %d\n",
				__func__, sizeimage,
				(width*height*3)/2 + mipi_csi_y_header_size+mipi_csi_c_header_size);
		} else {
			bytesperline = width;
			sizeimage = width*height*3/2;
		}
		break;
	case V4L2_PIX_FMT_NV16:
		bytesperline = width;
		sizeimage = width*height*2;
		break;
	case V4L2_PIX_FMT_YUYV:
		bytesperline = width*2;
		sizeimage = width*height*2;
		break;
	case V4L2_PIX_FMT_YVYU:
		bytesperline = width*2;
		sizeimage = width*height*2;
		break;
	case V4L2_PIX_FMT_UYVY:
		bytesperline = width*2;
		sizeimage = width*height*2;
		break;
	case V4L2_PIX_FMT_VYUY:
		bytesperline = width*2;
		sizeimage = width*height*2;
		break;
	case V4L2_PIX_FMT_RGB24:
		bytesperline = width;
		sizeimage = width*height*3;
		if (drvctx->params.single_rgb_plane == 1)
			bytesperline = width*3;
		break;
	default:
		pr_err("%s, Not support pixelformat = %c%c%c%c\n", __func__,
			pix_mp->pixelformat&0xFF, (pix_mp->pixelformat>>8)&0xFF,
			(pix_mp->pixelformat>>16)&0xFF, (pix_mp->pixelformat>>24)&0xFF);
		break;
	}

	pix_mp->width = width;
	pix_mp->height = height;
	pix_mp->plane_fmt[0].bytesperline = bytesperline;
	pix_mp->plane_fmt[0].sizeimage = sizeimage;
	pix_mp->flags = 0;
	pix_mp->field = V4L2_FIELD_NONE;
	memset(pix_mp->reserved, 0, sizeof(pix_mp->reserved));
	pr_info("%s, pix_mp->pixelformat = %c%c%c%c\n", __func__,
		pix_mp->pixelformat&0xFF, (pix_mp->pixelformat>>8)&0xFF,
		(pix_mp->pixelformat>>16)&0xFF, (pix_mp->pixelformat>>24)&0xFF);
	pr_info("%s, pix_mp->plane_fmt[0].bytesperline = %d\n", __func__,
		 pix_mp->plane_fmt[0].bytesperline);
	pr_info("%s, pix_mp->plane_fmt[0].sizeimage = %d\n", __func__,
		 pix_mp->plane_fmt[0].sizeimage);
}

static void init_out_fmt(struct v4l2_fh *fh, struct v4l2_format *out_fmt)
{
	struct npp_fmt *npp_fmt;
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);

	npp_fmt = npp_find_fmt_by_idx(0, NPP_FMT_TYPE_OUTPUT);

	out_fmt->fmt.pix_mp.pixelformat  = npp_fmt->v4l2_pix_fmt;
	out_fmt->type                    = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	out_fmt->fmt.pix_mp.colorspace   = V4L2_COLORSPACE_REC709;
	out_fmt->fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	out_fmt->fmt.pix_mp.ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	out_fmt->fmt.pix_mp.xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	out_fmt->fmt.pix_mp.num_planes   = npp_fmt->num_planes;

	npp_update_pix_fmt(drvctx, &out_fmt->fmt.pix_mp, 1920, 1080);
}

static void init_cap_fmt(struct v4l2_fh *fh, struct v4l2_format *cap_fmt)
{
	struct npp_fmt *npp_fmt;
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);

	npp_fmt = npp_find_fmt_by_idx(0, NPP_FMT_TYPE_CAPTURE);

	cap_fmt->fmt.pix_mp.pixelformat  = npp_fmt->v4l2_pix_fmt;
	cap_fmt->type                    = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	cap_fmt->fmt.pix_mp.colorspace   = V4L2_COLORSPACE_SRGB;
	cap_fmt->fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	cap_fmt->fmt.pix_mp.ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	cap_fmt->fmt.pix_mp.xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	cap_fmt->fmt.pix_mp.num_planes   = npp_fmt->num_planes;

	npp_update_pix_fmt(drvctx, &cap_fmt->fmt.pix_mp, 1920, 1080);
}

static struct npp_buf_object *_npp_buffer_create(struct videc_dev *dev,
char *name, unsigned int flags)
{
	struct npp_buf_object *buf_obj;

	buf_obj = kzalloc(sizeof(*buf_obj), GFP_KERNEL);
	if (!buf_obj)
		return NULL;

	memset(buf_obj, 0x0, sizeof(*buf_obj));

#ifndef ENABLE_NPP_FPGA_TEST
	set_dma_ops(dev->dev, &rheap_dma_ops);
	rheap_setup_dma_pools(dev->dev, name, flags, __func__);

	dev->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	dev->dev->dma_mask = (u64 *)&dev->dev->coherent_dma_mask;
#endif

	return buf_obj;
}

static void _npp_buffer_free(struct npp_buf_object *buf_obj)
{
	if (!buf_obj) {
		pr_err("%s, buf_obj is NULL\n", __func__);
		return;
	}

#ifndef ENABLE_NPP_FPGA_TEST
	if(buf_obj->vaddr) {
		dma_free_coherent(buf_obj->dev, buf_obj->size, buf_obj->vaddr, buf_obj->paddr);
		buf_obj->vaddr = NULL;
	}
#endif

	kfree(buf_obj);
	buf_obj = NULL;
}

/*
 * Return npp_ctx structure for a given struct v4l2_fh
 */
static struct npp_ctx *fh_to_npp(struct v4l2_fh *fh)
{
	struct drv_npp_ctx *ctx = container_of(fh, struct drv_npp_ctx, fh);

	return ctx->npp_ctx;
}

/*
 * Return npp_ctx structure for a given struct vb2_queue
 */
static struct npp_ctx *vq_to_npp(struct vb2_queue *q)
{
	struct drv_npp_ctx *ctx = vb2_get_drv_priv(q);

	return ctx->npp_ctx;
}

static int npp_buf_done(struct v4l2_fh *fh, struct vb2_v4l2_buffer *v4l2_buf, uint32_t len,
uint64_t pts, uint32_t sizeimage, uint32_t sequence,
bool eos, bool no_frame)
{
	struct npp_ctx *ctx = fh_to_npp(fh);
	unsigned long flags;

	v4l2_buf = v4l2_m2m_dst_buf_remove(fh->m2m_ctx);
	if (!v4l2_buf) {
		pr_err("%s, No dst buffer to remove\n", __func__);
		return -ENOBUFS;
	}
	v4l2_buf->field = V4L2_FIELD_NONE;
	v4l2_buf->flags = V4L2_BUF_FLAG_KEYFRAME;
	v4l2_buf->vb2_buf.timestamp = pts;
	v4l2_buf->vb2_buf.planes[0].bytesused = len;
	v4l2_buf->sequence = sequence++;
	mutex_lock(&ctx->npp_mutex);
	ctx->seq_cap = sequence;
	mutex_unlock(&ctx->npp_mutex);

	if (!no_frame)
		vb2_set_plane_payload(&v4l2_buf->vb2_buf, 0, sizeimage);
	else
		vb2_set_plane_payload(&v4l2_buf->vb2_buf, 0, 0);

	spin_lock_irqsave(&ctx->npp_spin_lock, flags);

	npp_cap_buf_done(fh, v4l2_buf, eos, VB2_BUF_STATE_DONE);

	spin_unlock_irqrestore(&ctx->npp_spin_lock, flags);
	return 0;
}

static int threadcap(void *data)
{
	struct v4l2_fh *fh = (struct v4l2_fh *) data;
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	uint32_t sizeimage, sequence;
	unsigned long flags;
	struct vb2_v4l2_buffer *v4l2_buf;
	int ret;

	while (1) {
		ret = wait_event_interruptible_timeout(nppctx->npp_cap_waitq,
		 v4l2_m2m_num_dst_bufs_ready(fh->m2m_ctx) || kthread_should_stop(),
		 msecs_to_jiffies(nppctx->thread_cap_interval));

		if (kthread_should_stop() || (ret == -ERESTARTSYS)) {
			for (;;) {
				v4l2_buf = v4l2_m2m_dst_buf_remove(fh->m2m_ctx);
				if (v4l2_buf == NULL) {
					pr_err("%s(), v4l2_buf = NULL, exit\n", __func__);
					return 1;
				}

				spin_lock_irqsave(&nppctx->npp_spin_lock, flags);
				v4l2_m2m_buf_done(v4l2_buf, VB2_BUF_STATE_ERROR);
				spin_unlock_irqrestore(&nppctx->npp_spin_lock, flags);
			}
		}

		if (!v4l2_m2m_num_dst_bufs_ready(fh->m2m_ctx))
			continue;

		ret = 0;
		for (;  !ret;) {
			uint32_t len = 0;
			uint64_t pts = 0;
			bool eos = 0;
			bool no_frame = 0;

			v4l2_buf = v4l2_m2m_next_dst_buf(fh->m2m_ctx);
			if (!v4l2_buf)
				break;

			mutex_lock(&nppctx->npp_mutex);
			sizeimage = nppctx->cap.fmt.pix_mp.plane_fmt[0].sizeimage;
			sequence = nppctx->seq_cap;
			mutex_unlock(&nppctx->npp_mutex);

			pr_info("%s, seq_cap=%d\n", __func__, nppctx->seq_cap);

			ret = npp_cap_dqbuf(fh, v4l2_buf, &len, &pts);	// add NPP hw function here

			if (!ret) {
				if (npp_buf_done(fh, v4l2_buf, len, pts,
				 sizeimage, sequence, eos, no_frame))
					break;
				nppctx->cap_retry_cnt = 0;
				continue;
			}
			if (ret == -EAGAIN) {
				nppctx->cap_retry_cnt++;

				if (nppctx->cap_retry_cnt > 1000) {
					if (nppctx->stop_cmd && npp_buf_done(fh, v4l2_buf,
						0, 0, 0, 0, 1, 1))
						break;
					nppctx->cap_retry_cnt = 0;
					pr_err("%s, Capture buf full !\n", __func__);
				}
			}
			usleep_range(1000, 1000);
			break;
		}
	}

	return 0;
}

static int threadout(void *data)
{
	struct v4l2_fh *fh = (struct v4l2_fh *) data;
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	uint32_t sizeimage, sequence;
	struct vb2_v4l2_buffer *v4l2_buf;
	unsigned long flags;

	while (1) {
		int ret;
		 ret = wait_event_interruptible_timeout(nppctx->npp_out_waitq,
		  v4l2_m2m_num_src_bufs_ready(fh->m2m_ctx) ||
		  kthread_should_stop(), msecs_to_jiffies(nppctx->thread_out_interval));

		if (kthread_should_stop() || (ret == -ERESTARTSYS)) {
			for (;;) {
				v4l2_buf = v4l2_m2m_src_buf_remove(fh->m2m_ctx);
				if (v4l2_buf == NULL) {
					pr_err("%s(), v4l2_buf = NULL, exit\n", __func__);
					return 1;
				}

				spin_lock_irqsave(&nppctx->npp_spin_lock, flags);
				v4l2_m2m_buf_done(v4l2_buf, VB2_BUF_STATE_ERROR);
				spin_unlock_irqrestore(&nppctx->npp_spin_lock, flags);
			}
		}

		if (v4l2_m2m_num_src_bufs_ready(fh->m2m_ctx)) {
			for (;;) {
				v4l2_buf = v4l2_m2m_next_src_buf(fh->m2m_ctx);
				if (!v4l2_buf)
					break;

				mutex_lock(&nppctx->npp_mutex);
				sizeimage = nppctx->out.fmt.pix_mp.plane_fmt[0].sizeimage;
				sequence = nppctx->seq_out;
				mutex_unlock(&nppctx->npp_mutex);

				ret = npp_out_dqbuf(fh, v4l2_buf);	// add NPP hw function here

				if (!ret) {
					v4l2_buf = v4l2_m2m_src_buf_remove(fh->m2m_ctx);
					if (!v4l2_buf) {
						pr_err("%s, No src buffer to remove\n", __func__);
						break;
					}

					v4l2_buf->sequence = sequence++;

					mutex_lock(&nppctx->npp_mutex);
					nppctx->seq_out = sequence;
					mutex_unlock(&nppctx->npp_mutex);

					vb2_set_plane_payload(&v4l2_buf->vb2_buf, 0, sizeimage);

					spin_lock_irqsave(&nppctx->npp_spin_lock, flags);
					v4l2_m2m_buf_done(v4l2_buf, VB2_BUF_STATE_DONE);
					spin_unlock_irqrestore(&nppctx->npp_spin_lock, flags);
					continue;
				}

				break;
			}
		}
	}

	return  0;
}

static int threadrun(void *data)
{
	struct v4l2_fh *fh = (struct v4l2_fh *) data;
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	int ret;

	while (1) {
		ret = wait_event_interruptible_timeout(nppctx->npp_run_waitq,
		 kthread_should_stop(), msecs_to_jiffies(nppctx->thread_run_interval));

		if (kthread_should_stop() || (ret == -ERESTARTSYS)) {
			if (ret == -ERESTARTSYS)
				pr_err("%s(), ret = -ERESTARTSYS, exit\n", __func__);
			return 1;
		}

		while (1) {
			ret = npp_run(fh);

			if (ret < 0)
				break;
		}
	}

	return  0;
}

static int npp_enum_fmt_cap(struct v4l2_fmtdesc *f)
{
	struct npp_fmt *npp_fmt;

	npp_fmt = npp_find_fmt_by_idx(f->index, NPP_FMT_TYPE_CAPTURE);

	if (!npp_fmt)
		return -EINVAL;

	f->pixelformat = npp_fmt->v4l2_pix_fmt;

	return 0;
}

static int npp_enum_fmt_out(struct v4l2_fmtdesc *f)
{
	struct npp_fmt *npp_fmt;

	npp_fmt = npp_find_fmt_by_idx(f->index, NPP_FMT_TYPE_OUTPUT);

	if (!npp_fmt)
		return -EINVAL;

	f->pixelformat = npp_fmt->v4l2_pix_fmt;

	return 0;
}

static int npp_enum_framesize(struct v4l2_frmsizeenum *f)
{
	struct npp_fmt *npp_fmt;

	if (f->index)
		return -EINVAL;

	npp_fmt = npp_find_fmt(f->pixel_format, NPP_FMT_TYPE_OUTPUT);

	if (!npp_fmt) {
		npp_fmt = npp_find_fmt(f->pixel_format, NPP_FMT_TYPE_CAPTURE);
		if (!npp_fmt)
			return -EINVAL;
	}

	f->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	f->stepwise.min_width = npp_fmt->min_width;
	f->stepwise.max_width = npp_fmt->max_width;
	f->stepwise.step_width = PIC_SIZE_STEP_WIDTH;
	f->stepwise.min_height = npp_fmt->min_height;
	f->stepwise.max_height = npp_fmt->max_height;
	f->stepwise.step_height = PIC_SIZE_STEP_HEIGHT;

	return 0;
}

static int npp_g_fmt(struct v4l2_fh *fh, struct v4l2_format *f)
{
	struct npp_ctx *ctx = fh_to_npp(fh);

	mutex_lock(&ctx->npp_mutex);
	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		if (ctx->out.type != -1) {
			pr_info("%s, width=%d\n", __func__, ctx->out.fmt.pix_mp.width);
			memcpy(&f->fmt.pix_mp, &ctx->out.fmt.pix_mp, sizeof(struct v4l2_pix_format_mplane));
		} else {
			mutex_unlock(&ctx->npp_mutex);
			return -EINVAL;
		}
	} else {
		memcpy(&f->fmt.pix_mp, &ctx->cap.fmt.pix_mp, sizeof(struct v4l2_pix_format_mplane));
	}
	mutex_unlock(&ctx->npp_mutex);
	return 0;
}

static int npp_try_fmt_out(struct v4l2_fh *fh, struct v4l2_format *f)
{
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);
	struct npp_ctx *ctx = fh_to_npp(fh);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct npp_fmt *npp_fmt;
	__u32 width, height;
	int ret = 0;

	if (!V4L2_TYPE_IS_OUTPUT(f->type)) {
		ret = -EINVAL;
		goto exit;
	}
	npp_fmt = npp_find_fmt(pix_mp->pixelformat, NPP_FMT_TYPE_OUTPUT);

	if (!npp_fmt) {
		pr_err("%s, unknown output format %c%c%c%c width=%d height=%d quantization=%d\n",
		__func__,
		pix_mp->pixelformat&0xff, (pix_mp->pixelformat>>8)&0xff,
		(pix_mp->pixelformat>>16)&0xff, (pix_mp->pixelformat>>24)&0xff,
		pix_mp->width, pix_mp->height, pix_mp->quantization);

		mutex_lock(&ctx->npp_mutex);
		width = ctx->out.fmt.pix_mp.width;
		height = ctx->out.fmt.pix_mp.height;
		pix_mp->pixelformat = ctx->out.fmt.pix_mp.pixelformat;
		pix_mp->num_planes = ctx->out.fmt.pix_mp.num_planes;
		mutex_unlock(&ctx->npp_mutex);

		ret = -EINVAL;
	} else {
		if ((pix_mp->width < npp_fmt->min_width) || (pix_mp->width > npp_fmt->max_width)
		|| (pix_mp->height < npp_fmt->min_height) || (pix_mp->height > npp_fmt->max_height)) {
			ret = -EINVAL;
			goto exit;
		}

		width  = pix_mp->width;
		height = pix_mp->height;
		pix_mp->pixelformat = npp_fmt->v4l2_pix_fmt;
		pix_mp->num_planes = npp_fmt->num_planes;
	}

	npp_update_pix_fmt(drvctx, pix_mp, width, height);

	pr_info("%s, pixelformat=%c%c%c%c width=%d height=%d\n", __func__,
			pix_mp->pixelformat&0xFF, (pix_mp->pixelformat>>8)&0xFF,
			(pix_mp->pixelformat>>16)&0xFF, (pix_mp->pixelformat>>24)&0xFF,
			pix_mp->width, pix_mp->height);

exit:
	return ret;
}

static int npp_try_fmt_cap(struct v4l2_fh *fh, struct v4l2_format *f)
{
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);
	struct npp_ctx *ctx = fh_to_npp(fh);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct npp_fmt *npp_fmt;
	__u32 width, height;
	int ret = 0;

	if (!V4L2_TYPE_IS_CAPTURE(f->type)) {
		ret = -EINVAL;
		goto exit;
	}

	npp_fmt = npp_find_fmt(pix_mp->pixelformat, NPP_FMT_TYPE_CAPTURE);

	if (!npp_fmt) {
		pr_err("%s, unknown capture format %c%c%c%c width=%d height=%d quantization=%d\n",
		__func__,
		pix_mp->pixelformat&0xff, (pix_mp->pixelformat>>8)&0xff,
		(pix_mp->pixelformat>>16)&0xff, (pix_mp->pixelformat>>24)&0xff,
		pix_mp->width, pix_mp->height, pix_mp->quantization);

		mutex_lock(&ctx->npp_mutex);
		width = ctx->cap.fmt.pix_mp.width;
		height = ctx->cap.fmt.pix_mp.height;
		pix_mp->pixelformat = ctx->cap.fmt.pix_mp.pixelformat;
		pix_mp->num_planes = ctx->cap.fmt.pix_mp.num_planes;
		mutex_unlock(&ctx->npp_mutex);

		ret = -EINVAL;
	} else {

		if ((pix_mp->width < npp_fmt->min_width) || (pix_mp->width > npp_fmt->max_width)
		|| (pix_mp->height < npp_fmt->min_height) || (pix_mp->height > npp_fmt->max_height)) {
			ret = -EINVAL;
			goto exit;
		}

		width  = pix_mp->width;
		height = pix_mp->height;
		pix_mp->pixelformat = npp_fmt->v4l2_pix_fmt;
		pix_mp->num_planes = npp_fmt->num_planes;
	}

	npp_update_pix_fmt(drvctx, pix_mp, width, height);

	pr_info("%s, pixelformat=%c%c%c%c width=%d height=%d\n", __func__,
			pix_mp->pixelformat&0xFF, (pix_mp->pixelformat>>8)&0xFF,
			(pix_mp->pixelformat>>16)&0xFF, (pix_mp->pixelformat>>24)&0xFF,
			pix_mp->width, pix_mp->height);

exit:
	return ret;
}

static int npp_s_fmt_cap(struct v4l2_fh *fh, struct v4l2_format *f)
{
	struct npp_ctx *ctx = fh_to_npp(fh);
	int ret = 0;

	ret = npp_try_fmt_cap(fh, f);
	if (ret) {
		pr_err("%s, npp_try_fmt_cap fail", __func__);
		return ret;
	}

	mutex_lock(&ctx->npp_mutex);
	memcpy(&ctx->cap.fmt.pix_mp, &f->fmt.pix_mp, sizeof(struct v4l2_pix_format_mplane));
	mutex_unlock(&ctx->npp_mutex);

	return ret;
}


static int npp_s_fmt_out(struct v4l2_fh *fh, struct v4l2_format *f)
{
	struct npp_ctx *ctx = fh_to_npp(fh);
	int ret = 0;

	ret = npp_try_fmt_out(fh, f);
	if (ret) {
		pr_err("%s, npp_try_fmt_out fail", __func__);
		return ret;
	}

	mutex_lock(&ctx->npp_mutex);
	memcpy(&ctx->out.fmt.pix_mp, &f->fmt.pix_mp, sizeof(struct v4l2_pix_format_mplane));
	mutex_unlock(&ctx->npp_mutex);

	return ret;
}

static int npp_queue_info(struct vb2_queue *vq, int *bufcnt, int *sizeimage)
{
	struct npp_ctx *nppctx = vq_to_npp(vq);
	int type = vq->type;

	mutex_lock(&nppctx->npp_mutex);
	if (V4L2_TYPE_IS_OUTPUT(type)) {
		if (nppctx->out.type == -1) {
			// move to npp_alloc_context()
			mutex_unlock(&nppctx->npp_mutex);
			return -EINVAL;
		}

		if (sizeimage)
			*sizeimage = nppctx->out.fmt.pix_mp.plane_fmt[0].sizeimage;

		if (bufcnt)
			*bufcnt = OUT_BUF_COUNT;

	} else {
		if (nppctx->cap.type == -1) {
			mutex_unlock(&nppctx->npp_mutex);
			return -EINVAL;
		}

		if (sizeimage)
			*sizeimage = nppctx->cap.fmt.pix_mp.plane_fmt[0].sizeimage;

		if (bufcnt)
			*bufcnt = CAP_BUF_COUNT;
	}

	mutex_unlock(&nppctx->npp_mutex);
	return 0;
}

static int npp_start_streaming(struct vb2_queue *q, uint32_t count)
{
	struct drv_npp_ctx *drvctx = vb2_get_drv_priv(q);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	struct v4l2_fh *fh = &drvctx->fh;
	int ret = 0;
	int out_pixelformat = nppctx->out.fmt.pix_mp.pixelformat;
	int cap_pixelformat = nppctx->cap.fmt.pix_mp.pixelformat;

	mutex_lock(&nppctx->npp_mutex);

	if (!nppctx->thread_run)
		nppctx->thread_run = kthread_run(threadrun, fh, "runthread");

	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		nppctx->seq_out = 0;
		nppctx->memory_out = q->memory;
		nppctx->thread_out = kthread_run(threadout, fh, "outthread %c%c%c%c",
		((0xff&out_pixelformat)>>0), ((0xff&out_pixelformat)>>8),
		((0xff&out_pixelformat)>>16), ((0xff&out_pixelformat)>>24));
		nppctx->is_out_started = 1;
	} else {
		nppctx->seq_cap = 0;
		nppctx->memory_cap = q->memory;
		nppctx->thread_cap = kthread_run(threadcap, fh, "capthread %c%c%c%c",
		((0xff&cap_pixelformat)>>0), ((0xff&cap_pixelformat)>>8),
		((0xff&cap_pixelformat)>>16), ((0xff&cap_pixelformat)>>24));
		nppctx->is_cap_started = 1;
	}
	mutex_unlock(&nppctx->npp_mutex);

	return ret;
}

static int npp_stop_streaming(struct vb2_queue *q)
{
	struct drv_npp_ctx *drvctx = vb2_get_drv_priv(q);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	int ret = 0;


	if (nppctx->thread_run) {
		wake_up_interruptible(&nppctx->npp_run_waitq);
		kthread_stop(nppctx->thread_run);
		nppctx->thread_run = NULL;
	}

	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		wake_up_interruptible(&nppctx->npp_out_waitq);
		if (nppctx->thread_out)
			kthread_stop(nppctx->thread_out);
	} else {
		wake_up_interruptible(&nppctx->npp_cap_waitq);
		if (nppctx->thread_cap)
			kthread_stop(nppctx->thread_cap);
	}

	mutex_lock(&nppctx->npp_mutex);


	if (V4L2_TYPE_IS_OUTPUT(q->type))
		nppctx->is_out_started = 0;
	else
		nppctx->is_cap_started = 0;

	mutex_unlock(&nppctx->npp_mutex);
	return ret;
}

static int npp_qbuf(struct v4l2_fh *fh, struct vb2_buffer *vb)
{
	struct npp_ctx *ctx = vq_to_npp(vb->vb2_queue);

	if (V4L2_TYPE_IS_OUTPUT(vb->type)) {
		ctx->out_q_cnt++;
		wake_up_interruptible(&ctx->npp_out_waitq);
		return 0;
	}
	ctx->cap_q_cnt++;
	wake_up_interruptible(&ctx->npp_cap_waitq);
	return 0;
}

static int npp_abort(void *priv, int type)
{
	return 0;
}

static int npp_g_crop(void *fh, struct v4l2_rect *rect)
{
	/* G_SELECTION */
	struct npp_ctx *ctx = fh_to_npp(fh);

	mutex_lock(&ctx->npp_mutex);
	memcpy(rect, &ctx->rect, sizeof(ctx->rect));
	mutex_unlock(&ctx->npp_mutex);
	return 0;
}

static int npp_s_crop(void *fh, struct v4l2_selection *sel)
{
	/* S_SELECTION */
	struct npp_ctx *ctx = fh_to_npp(fh);

	mutex_lock(&ctx->npp_mutex);
	memcpy(&ctx->rect, &sel->r, sizeof(ctx->rect));
	mutex_unlock(&ctx->npp_mutex);
	return 0;
}

void npp_cap_buf_done(struct v4l2_fh *fh, struct vb2_v4l2_buffer *buf,
			   bool eos, enum vb2_buffer_state state)
{
	const struct v4l2_event eos_event = {

		.type = V4L2_EVENT_EOS
	};

	if (eos) {
		buf->flags |= V4L2_BUF_FLAG_LAST;
		v4l2_event_queue_fh(fh, &eos_event);
	}

	v4l2_m2m_buf_done(buf, state);
}

int npp_cap_dqbuf(void *fh, struct vb2_v4l2_buffer *v4l2_buf, uint32_t *len, uint64_t *pts)
{
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	unsigned int cap_buf_data_size = (nppctx->cap.fmt.pix_mp.width * nppctx->cap.fmt.pix_mp.height);
	int ret;

	nppctx->cap_v4l2_buf = v4l2_buf;
	nppctx->cap_q_buf_obj->done = 0;
	nppctx->cap_q_buf_obj->ready = 1;
	wake_up_interruptible(&nppctx->npp_run_waitq);

	ret = wait_event_interruptible(nppctx->npp_cap_waitq, kthread_should_stop()
	|| nppctx->cap_q_buf_obj->done);
	if (kthread_should_stop() || (ret == -ERESTART)) {
		if (ret == -ERESTART)
			pr_err("%s(%d) wait event error !!!\n", __func__, __LINE__);
		return -ERESTARTSYS;
	}

	*len = (cap_buf_data_size * 3);
	pr_info("%s, len = %d, %d\n", __func__, *len, v4l2_buf->vb2_buf.planes[0].bytesused);
	return 0;
}

static int npp_out_dqbuf(struct v4l2_fh *fh, struct vb2_v4l2_buffer *v4l2_buf)
{
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	int ret;

	if (nppctx->memory_out == V4L2_MEMORY_USERPTR) {
		uint8_t *out_vaddr = vb2_plane_vaddr(&v4l2_buf->vb2_buf, 0);
		uint8_t *dst = nppctx->out_q_buf_obj->vaddr;
		uint32_t len = v4l2_buf->vb2_buf.planes[0].bytesused;

		if ((drvctx->params.mipi_csi_param.mipi_csi_input == 0) ||
			(drvctx->params.mipi_csi_param.mipi_csi_input == 1 &&
			 drvctx->params.mipi_csi_param.in_queue_metadata == 0))
			memcpy(dst, out_vaddr, len);
		if (drvctx->params.mipi_csi_param.mipi_csi_input == 1 &&
		 drvctx->params.mipi_csi_param.in_queue_metadata == 1)
			memcpy(dst, out_vaddr,
			drvctx->params.mipi_csi_param.y_header_size +
			drvctx->params.mipi_csi_param.y_data_size +
			drvctx->params.mipi_csi_param.c_header_size +
			drvctx->params.mipi_csi_param.c_data_size);
	}

	nppctx->out_v4l2_buf = v4l2_buf;
	nppctx->out_q_buf_obj->done = 0;
	nppctx->out_q_buf_obj->ready = 1;
	pr_info("%s, done=%d, ready=%d\n", __func__, nppctx->out_q_buf_obj->done,
	 nppctx->out_q_buf_obj->ready);
	wake_up_interruptible(&nppctx->npp_run_waitq);

	ret = wait_event_interruptible(nppctx->npp_out_waitq,
	(kthread_should_stop() || nppctx->out_q_buf_obj->done));
	if (kthread_should_stop() || (ret == -ERESTARTSYS)) {
		if (ret == -ERESTARTSYS)
			pr_err("%s(%d) wait event error !!!\n", __func__, __LINE__);
		return -ERESTARTSYS;
	}

	return 0;
}

static int npp_run(struct v4l2_fh *fh)
{
	struct drv_npp_ctx *drvctx = container_of(fh, struct drv_npp_ctx, fh);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	struct videc_dev *v_dev = drvctx->dev;
	int ret;
	unsigned long flags;

	__u32 pixelformat;
	struct Npp_InputBufInfo input_buffer;
	struct Npp_OutputBufInfo output_buffer;
	struct Npp_ImageScalingInfo image_scaling;
	struct Npp_InputImageFormat src_input;
	struct Npp_OutputImageFormat tgt_output;
	struct v4l2_rect crop_rect;

	struct rtk_mipi_csi_metadata *mipi_csi_metadata;
	struct Npp_MIPICSI_ImageFormat mipi_csi_image_param;
	struct Npp_MIPI_CSI_InputBufInfo mipi_csi_input_buf;
	dma_addr_t src_paddr, dst_paddr;
	uint8_t *src_vaddr;

#ifndef ENABLE_NPP_ISR
	unsigned int int_status = 0;
#endif

	if (nppctx->memory_cap == V4L2_MEMORY_USERPTR)
		ret = wait_event_interruptible(nppctx->npp_run_waitq, kthread_should_stop()
	|| nppctx->out_q_buf_obj->ready);
	else
		ret = wait_event_interruptible(nppctx->npp_run_waitq, kthread_should_stop()
	|| (nppctx->out_q_buf_obj->ready && nppctx->cap_q_buf_obj->ready));

	if (kthread_should_stop() || (ret == -ERESTARTSYS)) {
		if (ret == -ERESTARTSYS)
			pr_err("%s, [%p] wait event error !!!\n", __func__, nppctx);
		return -ERESTARTSYS;
	}

	crop_rect = nppctx->rect;

	if (nppctx->out.fmt.pix_mp.width < crop_rect.left+crop_rect.width)
		crop_rect.width = nppctx->out.fmt.pix_mp.width-crop_rect.left;

	if (nppctx->out.fmt.pix_mp.height < crop_rect.top+crop_rect.height)
		crop_rect.height = nppctx->out.fmt.pix_mp.height-crop_rect.top;

	if (nppctx->memory_out == V4L2_MEMORY_USERPTR) {
		src_vaddr = nppctx->out_q_buf_obj->vaddr +
			nppctx->out_v4l2_buf->planes[0].data_offset;
		// internal created src buffer virtual address
		src_paddr = nppctx->out_q_buf_obj->paddr +
			nppctx->out_v4l2_buf->planes[0].data_offset;
		// internal created src buffer physical address
		dst_paddr = nppctx->cap_q_buf_obj->paddr;
		// internal created dst buffer physical address
		pr_info("%s, USERPTR, src_paddr = 0x%llx, dst_paddr = 0x%llx\n",
		 __func__, src_paddr, dst_paddr);
	} else if (nppctx->memory_out == V4L2_MEMORY_MMAP) {
		src_vaddr =
		vb2_plane_vaddr(&nppctx->out_v4l2_buf->vb2_buf, 0);
		// out virtual address
		src_paddr =
		vb2_dma_contig_plane_dma_addr(&nppctx->out_v4l2_buf->vb2_buf, 0);
		// out physical address
		dst_paddr =
		vb2_dma_contig_plane_dma_addr(&nppctx->cap_v4l2_buf->vb2_buf, 0);
		// cap physical address
		pr_info("%s, MMAP, src_paddr = 0x%llx, dst_paddr = 0x%llx\n",
		 __func__, src_paddr, dst_paddr);
	} else {
		src_vaddr =
		vb2_plane_vaddr(&nppctx->out_v4l2_buf->vb2_buf, 0) +
			nppctx->out_v4l2_buf->planes[0].data_offset;
		// out virtual address
		src_paddr =
		vb2_dma_contig_plane_dma_addr(&nppctx->out_v4l2_buf->vb2_buf, 0) +
			nppctx->out_v4l2_buf->planes[0].data_offset;
		// out physical address
		dst_paddr =
		vb2_dma_contig_plane_dma_addr(&nppctx->cap_v4l2_buf->vb2_buf, 0);
		// cap physical address
		pr_info("%s, DMA, src_paddr = 0x%llx, dst_paddr = 0x%llx\n",
		 __func__, src_paddr, dst_paddr);
	}

	if (v_dev->target_soc == STARK) {
		switch (nppctx->out.fmt.pix_mp.pixelformat) {
		case V4L2_PIX_FMT_NV12:
			src_input.yuv_format = NV12_UV;
			break;
		case V4L2_PIX_FMT_NV21:
			src_input.yuv_format = NV21_VU;
			break;
		case V4L2_PIX_FMT_NV16:
			src_input.yuv_format = NV16;
			break;
		default:
			pr_err("%s, Unsupported format=%c%c%c%c\n", __func__,
				(nppctx->out.fmt.pix_mp.pixelformat & 0xff),
				(nppctx->out.fmt.pix_mp.pixelformat >> 8) & 0xff,
				(nppctx->out.fmt.pix_mp.pixelformat >> 16) & 0xff,
				(nppctx->out.fmt.pix_mp.pixelformat >> 24) & 0xff);
			return -EINVAL;
		}
	} else {
		switch (nppctx->out.fmt.pix_mp.pixelformat) {
		case V4L2_PIX_FMT_YUYV:
			src_input.yuv_format = PACKED_YUYV;
			break;
		case V4L2_PIX_FMT_YVYU:
			src_input.yuv_format = PACKED_YVYU;
			break;
		case V4L2_PIX_FMT_UYVY:
			src_input.yuv_format = PACKED_UYVY;
			break;
		case V4L2_PIX_FMT_VYUY:
			src_input.yuv_format = PACKED_VYUY;
			break;
		case V4L2_PIX_FMT_NV12:
			src_input.yuv_format = NV12_UV;
			break;
		case V4L2_PIX_FMT_NV21:
			src_input.yuv_format = NV21_VU;
			break;
		case V4L2_PIX_FMT_NV16:
			src_input.yuv_format = NV16;
			break;
		default:
			pr_err("%s, Unsupported format=%c%c%c%c\n", __func__,
				(nppctx->out.fmt.pix_mp.pixelformat & 0xff),
				(nppctx->out.fmt.pix_mp.pixelformat >> 8) & 0xff,
				(nppctx->out.fmt.pix_mp.pixelformat >> 16) & 0xff,
				(nppctx->out.fmt.pix_mp.pixelformat >> 24) & 0xff);
			return -EINVAL;
		}
	}

	mutex_lock(&drvctx->dev->hw_mutex);
	down(&drvctx->dev->hw_sem);
	drvctx->dev->nppctx_activate = nppctx;
	pr_info("%s, mutex_lock npp_ctx = %p\n", __func__, nppctx);

	src_input.src_width = nppctx->out.fmt.pix_mp.width;
	src_input.src_height = nppctx->out.fmt.pix_mp.height;

	if (drvctx->params.mipi_csi_param.mipi_csi_input == 0)
		src_input.source = SOURCE_LINEAR_MODE;
	else
		src_input.source = SOURCE_VE_MODE;

	src_input.yuv_sample = (src_input.yuv_format == NV12_UV
	|| src_input.yuv_format == NV21_VU)
	? YUV_420 : YUV_422;
	src_input.bit_depth = 8;
	src_input.pitch = (src_input.yuv_format == NV12_UV
	|| src_input.yuv_format == NV21_VU || src_input.yuv_format == NV16)
	 ? (nppctx->out.fmt.pix_mp.width) : (nppctx->out.fmt.pix_mp.width * 2);
	pr_info("%s, width(%d), height(%d), yuv_sample(%d), bit_depth(%d)\n",
	 __func__, src_input.src_width, src_input.src_height,
	 src_input.yuv_sample, src_input.bit_depth);
	NPP_Set_InputImageFormat(&src_input, v_dev->target_soc);

	if (drvctx->params.mipi_csi_param.mipi_csi_input == 1) {
		mipi_csi_image_param.data_pitch = drvctx->params.mipi_csi_param.data_pitch;
		mipi_csi_image_param.header_pitch = drvctx->params.mipi_csi_param.header_pitch;
		mipi_csi_image_param.qlevel_queue_sel_y = 1;
		mipi_csi_image_param.qlevel_queue_sel_c = 1;
		mipi_csi_image_param.width = nppctx->out.fmt.pix_mp.width;
		mipi_csi_image_param.height = nppctx->out.fmt.pix_mp.height;
		NPP_Set_MIPICSI_ImageFormat(&mipi_csi_image_param, v_dev->target_soc);
	}

	pixelformat = nppctx->out.fmt.pix_mp.pixelformat;
	input_buffer.scan_mode = (pixelformat == V4L2_PIX_FMT_NV12 ||
	pixelformat == V4L2_PIX_FMT_NV21) ? CONSECUTIVE_FRAME : CONSECUTIVE_FRAME_422;
	input_buffer.top_filed_y_addr =
	 (unsigned long)src_paddr;
	input_buffer.top_filed_c_addr =
	 (unsigned long)src_paddr +
	(nppctx->out.fmt.pix_mp.width * nppctx->out.fmt.pix_mp.height);

	if (drvctx->params.mipi_csi_param.mipi_csi_input == 0) {
		NPP_Set_InputDataBuffer(&input_buffer, v_dev->target_soc);
	} else {
		if (drvctx->params.mipi_csi_param.in_queue_metadata == 0) {
			mipi_csi_input_buf.y_header_addr = (uint64_t)src_paddr;
			mipi_csi_input_buf.c_header_addr =
			(uint64_t)(mipi_csi_input_buf.y_header_addr
			+ drvctx->params.mipi_csi_param.y_header_size);
			mipi_csi_input_buf.y_data_addr =
			(uint64_t)(mipi_csi_input_buf.c_header_addr
			+ drvctx->params.mipi_csi_param.c_header_size);
			mipi_csi_input_buf.c_data_addr =
			(uint64_t)(mipi_csi_input_buf.y_data_addr
			+ drvctx->params.mipi_csi_param.y_data_size);
		} else {
			mipi_csi_metadata = (struct rtk_mipi_csi_metadata *)(src_vaddr);
			mipi_csi_input_buf.y_header_addr =
			(uint64_t)mipi_csi_metadata->y_header_phyaddr;
			mipi_csi_input_buf.y_data_addr =
			(uint64_t)mipi_csi_metadata->y_data_phyaddr;
			mipi_csi_input_buf.c_header_addr =
			(uint64_t)mipi_csi_metadata->c_header_phyaddr;
			mipi_csi_input_buf.c_data_addr =
			(uint64_t)mipi_csi_metadata->c_data_phyaddr;
		}
		NPP_Set_MIPICSI_InputDataBuffer(&mipi_csi_input_buf, v_dev->target_soc);
	}

	NPP_Set_OutputRGBFormat((drvctx->params.single_rgb_plane == 1)
	 ? SINGLE_PLANE_RGB : THREE_PLANE_RGB, v_dev->target_soc);

	tgt_output.tgt_width = nppctx->cap.fmt.pix_mp.width;
	tgt_output.tgt_height = nppctx->cap.fmt.pix_mp.height;
	tgt_output.wb_format = RGB_MODE;
	tgt_output.rgb_level = (drvctx->params.signed_rgb_output == 1)
	? NEGATIVE_LEVEL : POSITIVE_LEVEL;
	tgt_output.rgb_format = THREE_PLANE_RGB;
	tgt_output.rgb_bit_mode = MODE_8bit;
	NPP_Set_OutputImageFormat(&tgt_output, v_dev->target_soc);

	output_buffer.r_addr = (unsigned long)dst_paddr;
	output_buffer.g_addr =
	(unsigned long)dst_paddr + (nppctx->cap.fmt.pix_mp.width * nppctx->cap.fmt.pix_mp.height);
	output_buffer.b_addr =
	(unsigned long)dst_paddr + ((nppctx->cap.fmt.pix_mp.width * nppctx->cap.fmt.pix_mp.height) * 2);
	NPP_Set_OutputDataBuffer(&output_buffer, v_dev->target_soc);

	NPP_Set_ColorSpaceConversion(v_dev->target_soc);

	if (crop_rect.left != 0  ||
		crop_rect.top != 0   ||
		crop_rect.width != 0 ||
		crop_rect.height != 0) {
		pr_info("%s, crop win: x(%d), y(%d), w(%d), h(%d)\n",
		 __func__, crop_rect.left, crop_rect.top, crop_rect.width, crop_rect.height);

		src_input.src_width = crop_rect.width;
		src_input.src_height = crop_rect.height;

		NPP_Set_InputImageFormat(&src_input, v_dev->target_soc);

		if (drvctx->params.mipi_csi_param.mipi_csi_input == 1)
			NPP_Set_MIPICSI_CropWindow(crop_rect.left, crop_rect.top,
		 crop_rect.width, crop_rect.height, v_dev->target_soc);
	}

	image_scaling.crop_pos.win_x = crop_rect.left;
	image_scaling.crop_pos.win_y = crop_rect.top;
	image_scaling.crop_pos.win_w = crop_rect.width;
	image_scaling.crop_pos.win_h = crop_rect.height;
	image_scaling.input_width = nppctx->out.fmt.pix_mp.width;
	image_scaling.input_height = nppctx->out.fmt.pix_mp.height;
	image_scaling.output_width = nppctx->cap.fmt.pix_mp.width;
	image_scaling.output_height = nppctx->cap.fmt.pix_mp.height;
	pr_info("%s, input(%d x %d), output(%d x %d)\n",
	 __func__, image_scaling.input_width, image_scaling.input_height,
	 image_scaling.output_width, image_scaling.output_height);

	NPP_Set_ScalerCoefficient(&image_scaling, v_dev->target_soc);

	NPP_Set_SecureVideoPath((unsigned char)drvctx->params.npp_svp, v_dev->target_soc);

	spin_lock_irqsave(&nppctx->npp_spin_lock, flags);
	nppctx->out_q_buf_obj->done = 0;
	pr_info("%s, out_q_buf_obj->done = 0\n", __func__);
	spin_unlock_irqrestore(&nppctx->npp_spin_lock, flags);

	NPP_Set_EngineGo(v_dev->target_soc);
#ifdef ENABLE_NPP_MEASURE_TIME
	nppctx->start_time = ktime_get();
#endif

#ifndef ENABLE_NPP_ISR
	while (!kthread_should_stop()) {
		int_status = 0;
		NPP_Get_INTStatus(&int_status, v_dev->target_soc);

		if (int_status) {
#ifdef ENABLE_NPP_MEASURE_TIME
			end_time = ktime_get();
#endif
			NPP_Close_INTEN(v_dev->target_soc);
			if ((int_status & NPP_INTST_fin_mask) != 0) {
				NPP_Clear_INTStatus(NPP_INTST_fin_mask, v_dev->target_soc);
				pr_info("polling INTST_fin\n");
			}
			if ((int_status & NPP_INTST_vi_tvve_core_mask) != 0) {
				NPP_Clear_INTStatus(NPP_INTST_vi_tvve_core_mask, v_dev->target_soc);
				pr_info("polling INTST_vi_tvve_core\n");
			}

			spin_lock_irqsave(&nppctx->npp_spin_lock, flags);
			nppctx->npp_hw_finish = true;
			spin_unlock_irqrestore(&nppctx->npp_spin_lock, flags);

			break;
		}

		usleep_range(500, 500);
	}
#endif

	ret = wait_event_interruptible(nppctx->npp_run_waitq,
	 kthread_should_stop() || nppctx->npp_hw_finish);

	up(&drvctx->dev->hw_sem);

	if (kthread_should_stop() || (ret == -ERESTARTSYS)) {
		if (ret == -ERESTARTSYS)
			pr_err("%s(%d) [%p] wait event error !!!\n", __func__, __LINE__, nppctx);
		mutex_unlock(&drvctx->dev->hw_mutex);
		return -ERESTARTSYS;
	}
#ifdef ENABLE_NPP_MEASURE_TIME
	nppctx->delta_ns = ktime_to_ns(ktime_sub(nppctx->end_time, nppctx->start_time));
	nppctx->sum_ns += nppctx->delta_ns;
	nppctx->excution_count++;
	pr_info("NPP Execution time: %lld us\n", nppctx->delta_ns / 1000);
#endif
	nppctx->npp_hw_finish = false;
	nppctx->out_q_buf_obj->done = 1;
	nppctx->out_q_buf_obj->ready = 0;
	wake_up_interruptible(&nppctx->npp_out_waitq);

	if (nppctx->memory_cap == V4L2_MEMORY_USERPTR) {
		unsigned int cap_buf_data_size =
		 (nppctx->cap.fmt.pix_mp.width * nppctx->cap.fmt.pix_mp.height);
		unsigned char rgb_single_plane =
		 (drvctx->params.single_rgb_plane == 1) ? 1 : 0;
		uint8_t *cap_vaddr;

		ret = wait_event_interruptible(nppctx->npp_run_waitq,
		 kthread_should_stop() || nppctx->cap_q_buf_obj->ready);
		if (kthread_should_stop() || (ret == -ERESTARTSYS)) {
			if (ret == -ERESTARTSYS)
				pr_err("%s(%d) [%p] wait event error !!!\n",
			 __func__, __LINE__, nppctx);
			mutex_unlock(&drvctx->dev->hw_mutex);
			return -ERESTARTSYS;
		}

		cap_vaddr = vb2_plane_vaddr(&nppctx->cap_v4l2_buf->vb2_buf, 0);

		if (rgb_single_plane == 0) {
			memcpy(cap_vaddr,
			 nppctx->cap_q_buf_obj->vaddr, cap_buf_data_size);
			memcpy(cap_vaddr + cap_buf_data_size,
			 nppctx->cap_q_buf_obj->vaddr + cap_buf_data_size, cap_buf_data_size);
			memcpy(cap_vaddr + (cap_buf_data_size*2),
			 nppctx->cap_q_buf_obj->vaddr + (cap_buf_data_size*2), cap_buf_data_size);
		} else {
			memcpy(cap_vaddr, nppctx->cap_q_buf_obj->vaddr, (cap_buf_data_size * 3));
		}
	}

	mutex_unlock(&drvctx->dev->hw_mutex);

	nppctx->cap_q_buf_obj->done = 1;
	nppctx->cap_q_buf_obj->ready = 0;
	wake_up_interruptible(&nppctx->npp_cap_waitq);

	return 0;
}

#if defined(ENABLE_NPP_ISR)
static irqreturn_t npp_isr(int this_irq, void *dev_id)
{
	unsigned long flags;
	unsigned int int_status = 0;
	struct videc_dev *p_this = (struct videc_dev *)dev_id;
	struct npp_ctx *nppctx = p_this->nppctx_activate;

	if (nppctx == NULL)
		return IRQ_NONE;

	NPP_Get_INTStatus(&int_status, p_this->target_soc);

	if (int_status) {
#ifdef ENABLE_NPP_MEASURE_TIME
		nppctx->end_time = ktime_get();
#endif
		if ((int_status & NPP_INTST_fin_mask) != 0) {
			NPP_Clear_INTStatus(NPP_INTST_fin_mask, p_this->target_soc);
			pr_info("isr INTST_fin\n");
		}
		if ((int_status & NPP_INTST_vi_tvve_core_mask) != 0) {
			NPP_Clear_INTStatus(NPP_INTST_vi_tvve_core_mask, p_this->target_soc);
			pr_info("isr INTST_vi_tvve_core\n");
		}

		spin_lock_irqsave(&nppctx->npp_spin_lock, flags);
		nppctx->npp_hw_finish = true;
		spin_unlock_irqrestore(&nppctx->npp_spin_lock, flags);

		wake_up_interruptible(&nppctx->npp_run_waitq);
	}

	return (int_status) ? IRQ_HANDLED : IRQ_NONE;
}
#endif

static const struct npp_fmt_ops_m2m ops_m2m = {
	.npp_enum_fmt_cap = npp_enum_fmt_cap,
	.npp_enum_fmt_out = npp_enum_fmt_out,
	.npp_enum_framesize = npp_enum_framesize,
	.npp_g_fmt = npp_g_fmt,
	.npp_try_fmt_cap = npp_try_fmt_cap,
	.npp_try_fmt_out = npp_try_fmt_out,
	.npp_s_fmt_cap = npp_s_fmt_cap,
	.npp_s_fmt_out = npp_s_fmt_out,
	.npp_queue_info = npp_queue_info,
	.npp_start_streaming = npp_start_streaming,
	.npp_stop_streaming = npp_stop_streaming,
	.npp_qbuf = npp_qbuf,
	.npp_abort = npp_abort,
	.npp_g_crop = npp_g_crop,
	.npp_s_crop = npp_s_crop,
};

const struct npp_fmt_ops_m2m *get_npp_fmt_ops_m2m(void)
{
	return &ops_m2m;
}

int npp_object_dma_allcate_m2m(struct drv_npp_ctx *ctx, int type)
{
	struct videc_dev *dev = ctx->dev;
	struct npp_ctx *nppctx = ctx->npp_ctx;
	struct npp_buf_object *buf_obj;
	int size = 0;

	if V4L2_TYPE_IS_OUTPUT(type) {
 		buf_obj = nppctx->out_q_buf_obj;
		size = MAX_IMAGE_SIZE_M2M*2 + MAX_HEADER_SIZE;
	} else {
		buf_obj = nppctx->cap_q_buf_obj;
		size = MAX_IMAGE_SIZE_M2M*3;
	}

	if(buf_obj->vaddr != NULL) {
		dev_info(dev->dev, "buf_obj->vaddr already allocate\n");
		return 0;
	}

#ifndef ENABLE_NPP_FPGA_TEST
	buf_obj->vaddr = dma_alloc_coherent(dev->dev, size,
											&buf_obj->paddr,
											GFP_KERNEL);
#endif

	if (!buf_obj->vaddr) {
		dev_err(dev->dev, "failed to allocate buf_obj buffer\n");
		return -ENOMEM;
	}

	buf_obj->dev = dev->dev;
	buf_obj->size = size;

	return 0;
}

void npp_object_dma_free_m2m(struct drv_npp_ctx *ctx, int type)
{
	struct npp_ctx *nppctx = ctx->npp_ctx;
	struct npp_buf_object *buf_obj = NULL;

	if V4L2_TYPE_IS_OUTPUT(type)
 		buf_obj = nppctx->out_q_buf_obj;
	else
		buf_obj = nppctx->cap_q_buf_obj;

	if (!buf_obj) {
		pr_err("%s, buf_obj is NULL\n", __func__);
		return;
	}

#ifndef ENABLE_NPP_FPGA_TEST
	if(buf_obj->vaddr) {
		dma_free_coherent(buf_obj->dev, buf_obj->size, buf_obj->vaddr, buf_obj->paddr);
		buf_obj->vaddr = NULL;
	}
#endif
}

void *npp_alloc_context_m2m(struct v4l2_fh *fh, struct videc_dev *dev)
{
	struct npp_ctx *ctx = NULL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ctx;

	ctx->out_q_buf_obj =
	_npp_buffer_create(dev, "rtk_media_heap", RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC);
	if (!ctx->out_q_buf_obj) {
		dev_err(dev->dev, "allocate npp out_q_buf_obj failed\n");
		kfree(ctx);
		return NULL;
	}

	ctx->cap_q_buf_obj =
	_npp_buffer_create(dev, "rtk_media_heap", RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC);
	if (!ctx->cap_q_buf_obj) {
		dev_err(dev->dev, "allocate npp cap_q_buf_obj failed\n");
		_npp_buffer_free(ctx->out_q_buf_obj);
		kfree(ctx);
		return NULL;
	}

	ctx->out.type = -1;
	ctx->cap.type = -1;
	ctx->thread_out = NULL;
	ctx->thread_cap = NULL;
	ctx->seq_out = 1;
	ctx->seq_cap = 1;
	ctx->thread_out_interval = 4;
	ctx->thread_cap_interval = 4;
	ctx->thread_run_interval = 4;
	ctx->rect.left = 0;
	ctx->rect.top = 0;
	ctx->rect.width = 0;
	ctx->rect.height = 0;
	ctx->is_cap_started = 0;
	ctx->is_out_started = 0;
	ctx->cap_retry_cnt = 0;
	ctx->stop_cmd = false;
	ctx->out_q_cnt = 0;
	ctx->cap_q_cnt = 0;
	ctx->out_q_buf_obj->done = 1;
	ctx->cap_q_buf_obj->done = 0;
	ctx->npp_hw_finish = false;
#ifdef ENABLE_NPP_MEASURE_TIME
	ctx->sum_ns = 0;
	ctx->excution_count = 0;
#endif
	mutex_init(&ctx->npp_mutex);
	spin_lock_init(&ctx->npp_spin_lock);
	init_waitqueue_head(&ctx->npp_out_waitq);
	init_waitqueue_head(&ctx->npp_cap_waitq);
	init_waitqueue_head(&ctx->npp_run_waitq);

	// init ctx->out with default value
	init_out_fmt(fh, &ctx->out);
	// init ctx->cap with default value
	init_cap_fmt(fh, &ctx->cap);

#if defined(ENABLE_NPP_ISR)
	if (dev->dev_open_cnt_m2m == 0) {
		if (request_irq(dev->irq, npp_isr, IRQF_SHARED, "NPU_PP", (void *)dev) != 0) {
			dev_err(dev->dev, "request_irq fail\n");
			_npp_buffer_free(ctx->out_q_buf_obj);
			_npp_buffer_free(ctx->cap_q_buf_obj);
			kfree(ctx);
			return NULL;
		}
		pr_info("%s, request_irq done\n", __func__);
	}
#endif

	pr_info("%s, npp_ctx = %p\n", __func__, ctx);
	return ctx;
}

void npp_free_context_m2m(struct videc_dev *dev, void *ctx)
{
	struct npp_ctx *ctx_npp = (struct npp_ctx *)ctx;
#ifndef ENABLE_NPP_FPGA_TEST
	struct arm_smccc_res res;
#endif
	if (ctx_npp->out_q_buf_obj != NULL) {
		_npp_buffer_free(ctx_npp->out_q_buf_obj);
		dev_info(dev->dev, "free out_q_buf_obj\n");
	}

	if (ctx_npp->cap_q_buf_obj != NULL) {
		_npp_buffer_free(ctx_npp->cap_q_buf_obj);
		dev_info(dev->dev, "free cap_q_buf_obj\n");
	}

	if (dev->dev_open_cnt_m2m == 0) {
#if defined(ENABLE_NPP_ISR)
		free_irq(dev->irq, (void *)dev);
#endif
		NPP_UnInit(dev->target_soc);
		if (dev->target_soc == STARK) {
			NPP_NPP_HwClock(0, dev->target_soc);
#ifndef ENABLE_NPP_FPGA_TEST
			arm_smccc_smc(RTK_NN_CONTROL, NPU_PP_OP_HOLD_RESET, 0, 0, 0, 0, 0, 0, &res);
#endif
		} else if (dev->target_soc == KENT) {
			NPP_NPP_HwClock(0, dev->target_soc);
#ifndef ENABLE_NPP_FPGA_TEST
			arm_smccc_smc(SIP_NN_OP, NPU_PP_OP_HOLD_RESET, 0, 0, 0, 0, 0, 0, &res);
#endif
		} else {
			dev_info(dev->dev, "not need to do hw reset and close clock\n");
		}
	}

	kfree(ctx);
	ctx = NULL;
}

#ifdef ENABLE_NPP_MEASURE_TIME
void npp_time_measure(void *ctx)
{
	struct npp_ctx *ctx_npp = (struct npp_ctx *)ctx;
	uint64_t avg_ns, avg_ms_whole, avg_ms_decimal;

	avg_ns = ctx_npp->sum_ns/ctx_npp->excution_count;
	avg_ms_whole = avg_ns / 1000000;
	avg_ms_decimal = (avg_ns % 1000000) / 1000;

	pr_info("NPP Average Execution Time: %llu.%03llu ms, %llu us\n",
		avg_ms_whole, avg_ms_decimal, avg_ns);
}
#endif
