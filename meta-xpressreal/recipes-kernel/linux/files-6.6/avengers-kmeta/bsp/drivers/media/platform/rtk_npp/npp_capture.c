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
#include <linux/time.h>   // for using jiffies
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/fdtable.h>
#include <linux/kernel.h>
#include "drv_npp.h"
#include "npp_debug.h"
#include "npp_common.h"
#include "npp_capture.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/dma-buf.h>
#include <linux/dma-map-ops.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>

#include "npp_rpc.h"
#include <soc/realtek/rtk-krpc-agent.h>
#include "npp_inband.h"

#define to_npp_buffer(buf)	container_of(buf, struct npp_buffer, vb)

#define WB_PINID 0x20140507
#define WBINBAND_VERSION 0x72746b30	// rtk0
//#define DEFAULT_TRANSCODE_WB_BUFFERNUM 3
#define WBINBAND_ENTRYNUM 1
#define PLOCK_BUFFER_SET_SIZE 32
#define PLOCK_BUFFER_SET 2
#define PLOCK_MAX_BUFFER_INDEX (PLOCK_BUFFER_SET_SIZE * PLOCK_BUFFER_SET)
#define PLOCK_INIT	  0xFF
#define PLOCK_QPEND	 0
#define ROUNDUP(x, y)	((x + y - 1) & ~(y - 1))
#define ROUNDUP16(x)	ROUNDUP(x, 16)
#define ROUNDUP2(x)	ROUNDUP(x, 2)

#define RPC_BUFFER_SIZE 4096
#define TIMER_INTERVAL 0
#define RPC_ALIGN_SZ 128

struct npp_rpc_info {
	struct device *dev;
	unsigned int ret;
	void *vaddr;
	dma_addr_t paddr;
	struct rtk_krpc_ept_info *acpu_ept_info;
	struct rtk_krpc_ept_info *hifi_ept_info;
};

struct RPCRES_LONG {
	unsigned int result;
	unsigned int data;
};

struct VIDEO_RPC_INSTANCE {
	enum VIDEO_VF_TYPE type;
};

struct video_rpc_create_t {
	struct VIDEO_RPC_INSTANCE instance;
};

struct video_rpc_destroy_t {
	u_int instanceID;
};

struct video_rpc_npp_init_t {
	struct RPCRES_LONG retval;
	unsigned int ret;
};

struct video_rpc_npp_destroy_t {
	u_int instanceID;
	struct RPCRES_LONG retval;
	unsigned int ret;
};

struct video_rpc_npp_query_in_t {
	enum VO_VIDEO_PLANE plane;
};

struct video_rpc_npp_query_out_t {
	short result;
	enum VO_VIDEO_PLANE plane;
	unsigned short numWin;
	unsigned short zOrder;
	struct vo_rectangle configWin;
	struct vo_rectangle contentWin;
	short deintMode;
	unsigned short pitch;
	enum INBAND_CMD_GRAPHIC_FORMAT colorType;
	enum INBAND_CMD_GRAPHIC_RGB_ORDER RGBOrder;
	enum INBAND_CMD_GRAPHIC_3D_MODE format3D;
	struct VO_SIZE mix1_size;
	enum VO_STANDARD standard;
	unsigned char enProg;
	unsigned char reserved1;
	unsigned short reserved2;
	struct VO_SIZE mix2_size;
};

struct VIDEO_RPC_CONFIG_WRITEBACK_FLOW {
	u_int instanceID;
	enum ENUM_WRITEBACK_TYPE type;
	u_int reserved[10];
};

struct video_rpc_config_writeback_flow_t {
	struct VIDEO_RPC_CONFIG_WRITEBACK_FLOW flow;
};

struct RPC_RINGBUFFER {
	unsigned int instanceID;
	unsigned int pinID;
	unsigned int readPtrIndex;
	unsigned int pRINGBUFF_HEADER;
};

struct npp_ringbuffer_info {
	void *vaddr;
	dma_addr_t paddr;
	struct RINGBUFFER_HEADER *ringheader;
};

struct video_rpc_ringbuffer_t {
	unsigned int instanceID;
	unsigned int pinID;
	unsigned int readPtrIndex;
	unsigned int pRINGBUFF_HEADER;
};

struct video_rpc_run_t {
	u_int instanceID;
};

struct lock_info {
	void *vaddr;
	dma_addr_t paddr;
};

static struct npp_rpc_info *rpc_info;
unsigned int rpc_instance_id;
/* ICQ ringbuffer */
static struct npp_ringbuffer_info *rb_icq;
/* Writeback ringbuffer */
static struct npp_ringbuffer_info *rb_wb;
static struct lock_info *lock_info, *recv_info;


static struct npp_fmt npp_fmt_list[2][1] = {

	[NPP_FMT_TYPE_CAPTURE] = {
		{
			.v4l2_pix_fmt = V4L2_PIX_FMT_RGB24,
			.max_width = MAX_IMAGE_CAPTURE_WIDTH,
			.min_width = MIN_IMAGE_CAPTURE_WIDTH,
			.max_height = MAX_IMAGE_CAPTURE_HEIGHT,
			.min_height = MIN_IMAGE_CAPTURE_HEIGHT,
			.num_planes = 1,
		},
	}
};

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

	switch (pix_mp->pixelformat) {
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

static int npp_enum_fmt_cap(struct v4l2_fmtdesc *f)
{
	struct npp_fmt *npp_fmt;

	npp_fmt = npp_find_fmt_by_idx(f->index, NPP_FMT_TYPE_CAPTURE);

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

	npp_fmt = npp_find_fmt(f->pixel_format, NPP_FMT_TYPE_CAPTURE);

	if (!npp_fmt)
		return -EINVAL;

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
	memcpy(&f->fmt.pix_mp, &ctx->cap.fmt.pix_mp, sizeof(struct v4l2_pix_format));
	mutex_unlock(&ctx->npp_mutex);

	return 0;
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

static int npp_queue_info(struct vb2_queue *vq, int *bufcnt, int *sizeimage)
{
	struct npp_ctx *nppctx = vq_to_npp(vq);

	mutex_lock(&nppctx->npp_mutex);
	if (nppctx->cap.type == -1) {
		mutex_unlock(&nppctx->npp_mutex);
		return -EINVAL;
	}

	if (sizeimage)
		*sizeimage = nppctx->cap.fmt.pix_mp.plane_fmt[0].sizeimage;

	mutex_unlock(&nppctx->npp_mutex);

	return 0;
}

int _npp_set_plock_unlocked(int offset)
{
	uint8_t *lock_p_vaddr;

	if ((offset >= 0) && (offset < PLOCK_MAX_BUFFER_INDEX)) {
		lock_p_vaddr = (uint8_t *)lock_info->vaddr + offset;
		*lock_p_vaddr = PLOCK_QPEND;
	} else {
		dev_info(rpc_info->dev, "[%s] unexpected lock offset\n", __func__);
		return -1;
	}
	return 0;
}

static void npp_vir_in_timer_cb(struct timer_list *vir_in_timer)
{
	struct npp_ctx *nppctx = container_of(vir_in_timer, struct npp_ctx, vir_in_timer);

	schedule_work(&nppctx->frame_work);
	mod_timer(vir_in_timer, jiffies + msecs_to_jiffies(TIMER_INTERVAL));
}

static int npp_start_streaming(struct vb2_queue *q, uint32_t count)
{
	struct drv_npp_ctx *drvctx = vb2_get_drv_priv(q);
	struct npp_ctx *nppctx = drvctx->npp_ctx;

	timer_setup(&nppctx->vir_in_timer, npp_vir_in_timer_cb, 0);
	mod_timer(&nppctx->vir_in_timer, jiffies + msecs_to_jiffies(TIMER_INTERVAL));

	return 0;
}

static int npp_stop_streaming(struct vb2_queue *q)
{
	struct drv_npp_ctx *drvctx = vb2_get_drv_priv(q);
	struct npp_ctx *nppctx = drvctx->npp_ctx;
	int ret, i;
	struct npp_buffer *buf;

	ret = cancel_work_sync(&nppctx->frame_work);
	if (ret < 0) {
		dev_err(drvctx->dev->dev, "cancel_work_sync failed\n");
		return ret;
	}

	del_timer_sync(&nppctx->vir_in_timer);

	list_for_each_entry(buf, &nppctx->buffers_head, link)
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);

	INIT_LIST_HEAD(&nppctx->buffers_head);

	ret = npp_run_stop(drvctx->dev);
	if (ret < 0) {
		dev_err(drvctx->dev->dev, "npp_run_stop failed\n");
		return ret;
	}

	for (i = 0; i < CAP_BUF_COUNT_MAX; i++) {
		pr_info("before stream off nppctx->plock_offset[%d] =%d\n",
			i, nppctx->plock_offset[i]);
		pr_info("before stream off nppctx->buffer_status[%d] =%d\n",
			i, nppctx->buffer_status[i]);

		nppctx->plock_offset[i] = -1;
		nppctx->buffer_status[i] = DEFAULT;
	}

	nppctx->seq_cap = 0;
	drvctx->dev->npp_activate = false;
	return 0;
}

static void memcpy_inband_swap(uint8_t *des, uint8_t *src, unsigned int size, int opt)
{
	unsigned int *src_int32 = (unsigned int *)src;
	unsigned int *des_int32 = (unsigned int *)des;
	unsigned int i;

	for (i = 0; i < (size / sizeof(int)); i++) {
		if (opt == RPC_AUDIO)
			des_int32[i] = htonl(src_int32[i]);
		else
			des_int32[i] = src_int32[i];
	}

}

static int write_inband_cmd(struct npp_ringbuffer_info *rb_info, uint8_t *buf,
				int size, int opt)
{
	struct RINGBUFFER_HEADER *ringheader = rb_info->ringheader;
	unsigned int wp, rp, rbsize;
	uint8_t *wptr, *next, *limit;
	int size0, size1;

	if (opt == RPC_AUDIO) {
		wp = htonl(ringheader->writePtr);
		rp = htonl(ringheader->readPtr[0]);
		rbsize = htonl(ringheader->size);
	} else {
		wp = ringheader->writePtr;
		rp = ringheader->readPtr[0];
		rbsize = ringheader->size;
	}

	/* Check available space for write size */
	if ((rp > wp) && (rp - wp - 1 < size))
		return -EAGAIN;


	if ((wp > rp) && (rp + rbsize - wp - 1 < size))
		return -EAGAIN;


	/* Get address for write */
	wptr = rb_info->vaddr + (wp - rb_info->paddr);
	next = wptr + size;
	limit = rb_info->vaddr + rbsize;

	/* Write cmd buffer to ring buffer */
	if (next >= limit) {
		size0 = limit - wptr;
		size1 = size - size0;
		memcpy_inband_swap(wptr, buf, size0, opt);
		memcpy_inband_swap(rb_info->vaddr, buf + size0, size1, opt);

		next -= rbsize;
	} else {
		memcpy_inband_swap(wptr, buf, size, opt);
	}

	if (opt == RPC_AUDIO)
		ringheader->writePtr =
			htonl(rb_info->paddr + (next - (uint8_t *) rb_info->vaddr));
	else
		ringheader->writePtr =
			rb_info->paddr + (next - (uint8_t *) rb_info->vaddr);

	return 0;
}

static int _npp_add_wb_buffer(struct device *dev, struct npp_capture_info *t_info, int index,
				int opt, int buffer_num)
{
	//struct npp_addr_info addr_info;
	int ret;
	dma_addr_t lock_p_addr;
	dma_addr_t recv_p_addr;
	dma_addr_t buf_p_addr;
	int strideSizeN = 1;
	int frameSizeN = 3;
	struct VIDEO_VO_NPP_WB_PICTURE_OBJECT object = { 0 };


	if (t_info->format != NPP_RGB_PL) {
		strideSizeN = 3;
		frameSizeN = 1;
	}


	/* Use cmd type VIDEO_NPP_INBAND_CMD_TYPE_WRITEBACK_BUFFER = 101 */
	object.header.type = VIDEO_NPP_INBAND_CMD_TYPE_WRITEBACK_BUFFER;
	object.header.size = sizeof(object);

	object.bufferNum = buffer_num;
	object.bufferId = index;
	/* t_info->stride is width*3,bufferSize and stride needed 16 aligned */
	object.bufferSize = t_info->stride * strideSizeN * t_info->height;


	buf_p_addr  = t_info->paddr;
	lock_p_addr = lock_info->paddr + sizeof(char) * WBINBAND_ENTRYNUM * index;
	recv_p_addr = recv_info->paddr + sizeof(char) * WBINBAND_ENTRYNUM * index;


	object.addrR = buf_p_addr;
	object.addrG = object.addrR + object.bufferSize; //useless if packed RGB
	object.addrB = object.addrG + object.bufferSize; //useless if packed RGB

	object.pitch = t_info->stride;
	object.version = WBINBAND_VERSION;
	object.pLock = lock_p_addr;
	object.pReceived = recv_p_addr;
	object.targetFormat = t_info->format;
	object.width = t_info->width;
	object.height = t_info->height;
	object.partialSrcWin_x = t_info->crop_x;
	object.partialSrcWin_y = t_info->crop_y;
	object.partialSrcWin_w = t_info->crop_width;
	object.partialSrcWin_h = t_info->crop_height;

	ret = write_inband_cmd(rb_icq,
					   (uint8_t *) &object, sizeof(object), opt);

	if (ret < 0)
		return -1;

	return 0;
}

static int npp_qbuf(struct videc_dev *dev, struct v4l2_fh *fh, struct vb2_buffer *vb,
						unsigned char format, int bufferNum)
{
	struct npp_ctx *ctx = vq_to_npp(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct npp_buffer *buffer = to_npp_buffer(vbuf);
	dma_addr_t dst_paddr;
	int ret;
	int cap_width;
	int cap_height;
	int sizeimage;
	struct v4l2_rect crop_rect;

	mutex_lock(&ctx->npp_mutex);
	cap_width = ctx->cap.fmt.pix_mp.width;
	cap_height = ctx->cap.fmt.pix_mp.height;
	crop_rect = ctx->rect;
	sizeimage = ctx->cap.fmt.pix_mp.plane_fmt[0].sizeimage;
	mutex_unlock(&ctx->npp_mutex);

	if (vb->index >= CAP_BUF_COUNT_MAX) {
		pr_err("%s, index =%d, Out off supporting buffer num, only support %d buffers\n",
					__func__, vb->index, CAP_BUF_COUNT_MAX);
		return  -1;
	}

	if (ctx->plock_offset[vb->index] == -1 && ctx->buffer_status[vb->index] == DEFAULT) {

		struct npp_capture_info capture_info = { 0 };

		ctx->memory_cap = vb->vb2_queue->memory;

		if (ctx->memory_cap == V4L2_MEMORY_USERPTR) {
			dst_paddr = ctx->cap_q_buf_obj->paddr+(sizeimage)*(vb->index);
			// internal created dst buffer physical address
			pr_info("%s, USERPTR,  dst_paddr = 0x%llx\n", __func__, dst_paddr);
		} else {
			dst_paddr = vb2_dma_contig_plane_dma_addr(&buffer->vb.vb2_buf, 0);
			// cap physical address
			pr_info("%s, MMAP/DMA, dst_paddr = 0x%llx\n", __func__, dst_paddr);
		}

		capture_info.width = cap_width;
		capture_info.height = cap_height;
		capture_info.stride = ROUNDUP16(capture_info.width);
		capture_info.type = WRITEBACK_TYPE_VSYNC_V1;
		capture_info.format = (format == 1) ? NPP_RGB_PACKED : NPP_RGB_PL;
		capture_info.paddr = dst_paddr;
		capture_info.crop_x = ctx->rect.left;
		capture_info.crop_y = ctx->rect.top;
		capture_info.crop_width = ctx->rect.width;
		capture_info.crop_height = ctx->rect.height;

		pr_info("%s, bufferNum = %d,width = %d,height = %d,format = %d\n", __func__, bufferNum,
		capture_info.width, capture_info.height, capture_info.format);
		pr_info("%s, crop_x = %d,crop_y = %d,crop_width = %d,crop_height = %d\n", __func__,
		capture_info.crop_x, capture_info.crop_y, capture_info.crop_width, capture_info.crop_height);

		ret = _npp_add_wb_buffer(rpc_info->dev, &capture_info, vb->index, ctx->rpc_opt, bufferNum);
		if (ret < 0)
			return ret;

		ctx->buffer_status[vb->index] = IDLE;

		mutex_lock(&ctx->buffer_lock);
		list_add_tail(&buffer->link, &ctx->buffers_head);
		mutex_unlock(&ctx->buffer_lock);

	} else {

		mutex_lock(&ctx->buffer_lock);
		list_add_tail(&buffer->link, &ctx->buffers_head);
		mutex_unlock(&ctx->buffer_lock);

		ctx->plock_offset[vb->index] = -1;
		ctx->buffer_status[vb->index] = IDLE;

		ret = _npp_set_plock_unlocked(vb->index);

		if (ret < 0) {
			ctx->buffer_status[vb->index] = ABNORMAL;
			return -EINVAL;
		}
	}

	pr_info("%s, index = %d , doned\n", __func__, vb->index);

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
	ctx->rect.width = ROUNDUP2(ctx->rect.width);
	ctx->rect.left = ROUNDUP2(ctx->rect.left);
	mutex_unlock(&ctx->npp_mutex);
	return 0;
}

static const struct npp_fmt_ops_capture ops_capture = {
	.npp_enum_fmt_cap = npp_enum_fmt_cap,
	.npp_enum_framesize = npp_enum_framesize,
	.npp_g_fmt = npp_g_fmt,
	.npp_try_fmt_cap = npp_try_fmt_cap,
	.npp_s_fmt_cap = npp_s_fmt_cap,
	.npp_queue_info = npp_queue_info,
	.npp_start_streaming = npp_start_streaming,
	.npp_stop_streaming = npp_stop_streaming,
	.npp_qbuf = npp_qbuf,
	.npp_g_crop = npp_g_crop,
	.npp_s_crop = npp_s_crop,
};

const struct npp_fmt_ops_capture *get_npp_fmt_ops_capture(void)
{
	return &ops_capture;
}

static struct npp_buf_object *_npp_buffer_create(struct videc_dev *v_dev, char *name, unsigned int flags)
{
	struct npp_buf_object *buf_obj;

	buf_obj = kzalloc(sizeof(*buf_obj), GFP_KERNEL);
	if (!buf_obj)
		return NULL;

	memset(buf_obj, 0x0, sizeof(*buf_obj));

#ifndef ENABLE_NPP_FPGA_TEST
	set_dma_ops(v_dev->dev, &rheap_dma_ops);
	rheap_setup_dma_pools(v_dev->dev, name, flags, __func__);

	v_dev->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	v_dev->dev->dma_mask = (u64 *)&v_dev->dev->coherent_dma_mask;
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

static int read_inband_cmd(struct npp_ringbuffer_info *rb_info, uint8_t *buf,
			   int size, int opt)
{
	struct RINGBUFFER_HEADER *ringheader = rb_info->ringheader;
	unsigned int wp, rp, rbsize;
	uint8_t *rptr, *next, *limit;
	int size0, size1;

	if (opt == RPC_AUDIO) {
		wp = htonl(ringheader->writePtr);
		rp = htonl(ringheader->readPtr[0]);
		rbsize = htonl(ringheader->size);
	} else {
		wp = ringheader->writePtr;
		rp = ringheader->readPtr[0];
		rbsize = ringheader->size;
	}

	/* Check available space for read size */
	if ((wp >= rp) && (wp - rp < size))
		return -EAGAIN;


	if ((rp > wp) && (wp + rbsize - rp < size))
		return -EAGAIN;


	/* Get address for read */
	rptr = rb_info->vaddr + (rp - rb_info->paddr);
	next = rptr + size;
	limit = rb_info->vaddr + rbsize;

	/* Read from ring buffer into cmd buffer */
	if (next > limit) {
		size0 = limit - rptr;
		size1 = size - size0;
		memcpy_inband_swap(buf, rptr, size0, opt);
		memcpy_inband_swap(buf + size0, rb_info->vaddr, size1, opt);

		next -= rbsize;
	} else {
		memcpy_inband_swap(buf, rptr, size, opt);
	}

	if (opt == RPC_AUDIO)
		ringheader->readPtr[0] =
			htonl(rb_info->paddr + (next - (uint8_t *) rb_info->vaddr));
	else
		ringheader->readPtr[0] =
			rb_info->paddr + (next - (uint8_t *) rb_info->vaddr);

	return 0;
}


static int _npp_get_wb_frame(struct npp_ringbuffer_info *rb_info,
		struct NPP_PICTURE_OBJECT_CAPTURE *object, int opt)
{
	int timeout = 20;
	int ret;

	do {
		ret = read_inband_cmd(rb_info,
					  (uint8_t *)object, sizeof(*object), opt);
		if (ret == 0)
			break;

		/* Use usleep_range to sleep for ~usecs or small msecs */
		usleep_range(500, 1000);
	} while (--timeout);

	if (ret < 0)
		return ret;


	/* receive type VIDEO_NPP_OUT_INBAND_CMD_TYPE_OBJ_PIC = 102 (0x66) */
	if (object->header.type != VIDEO_NPP_OUT_INBAND_CMD_TYPE_OBJ_PIC)
		return -1;

	return 0;
}

int npp_get_plock_offset(dma_addr_t paddr)
{
	dma_addr_t lock_base_paddr = lock_info->paddr;
	int offset = -1;

	if (paddr < lock_base_paddr) {
		dev_info(rpc_info->dev, "[%s] unexpected lock paddr\n", __func__);
		goto finish;
	}

	if (paddr >= (lock_base_paddr + PLOCK_MAX_BUFFER_INDEX)) {
		dev_info(rpc_info->dev, "[%s] unexpected lock paddr\n", __func__);
		goto finish;
	}

	offset = paddr - lock_base_paddr;

finish:
	return offset;
}

int npp_get_frame(struct npp_ctx *nppctx)
{
	int ret;
	int lock_offset;

	struct NPP_PICTURE_OBJECT_CAPTURE object = { 0 };

	ret = _npp_get_wb_frame(rb_wb, &object, nppctx->rpc_opt);
	if (ret < 0)
		return -1;

	lock_offset = npp_get_plock_offset(object.pLock);

	if (lock_offset >= 0) {
		nppctx->plock_offset[lock_offset] = lock_offset;
		nppctx->buffer_status[lock_offset] = USING;
	}

	return lock_offset;
}

static void npp_get_frame_worker(struct work_struct *frame_work)
{
	void *addr = NULL;
	int buf_index;
	u64 time_ns;
	struct npp_ctx *nppctx = container_of(frame_work, struct npp_ctx, frame_work);
	uint32_t sizeimage;
	struct npp_buffer *buf = NULL;
	uint8_t *dst_vaddr = NULL;
	bool empty = TRUE;
	bool valid = FALSE;

	mutex_lock(&nppctx->npp_mutex);
	sizeimage = nppctx->cap.fmt.pix_mp.plane_fmt[0].sizeimage;
	mutex_unlock(&nppctx->npp_mutex);

	mutex_lock(&nppctx->buffer_lock);
	list_for_each_entry(buf, &nppctx->buffers_head, link) {
		empty = FALSE;
		break;
	}
	mutex_unlock(&nppctx->buffer_lock);

	if (empty) {
		//pr_info( "%s, list empty , skip frame\n", __func__);
		return;
	}

	buf_index = npp_get_frame(nppctx);
	if (buf_index < 0)
		return;

	mutex_lock(&nppctx->buffer_lock);
	list_for_each_entry(buf, &nppctx->buffers_head, link) {
		if (buf->vb.vb2_buf.index == buf_index) {
			valid = TRUE;
			break;
		}
	}

	if (!valid) {
		mutex_unlock(&nppctx->buffer_lock);
		pr_err("%s, invalid index %d\n", __func__, buf_index);
		return;
	}

	if (nppctx->memory_cap == V4L2_MEMORY_USERPTR) {

		addr = vb2_plane_vaddr(&buf->vb.vb2_buf, 0);

		dst_vaddr = nppctx->cap_q_buf_obj->vaddr+(sizeimage)*(buf_index);
		memcpy(addr, dst_vaddr, sizeimage);
	}

	time_ns = ktime_get_ns();

	vb2_set_plane_payload(&buf->vb.vb2_buf, 0, sizeimage);
	buf->vb.vb2_buf.timestamp = time_ns;
	buf->vb.sequence = nppctx->seq_cap++;
	buf->vb.field = V4L2_FIELD_NONE;

	pr_info("%s, VB2 done,buf_index = %d seq_cap = %u timestamp(ns) = %llu\n",
		__func__, buf_index, buf->vb.sequence, time_ns);

	vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	list_del(&buf->link);
	mutex_unlock(&nppctx->buffer_lock);
}

int npp_object_dma_allcate_capture( struct drv_npp_ctx *ctx, int bufferNum)
{
	struct videc_dev *dev = ctx->dev;
	struct npp_ctx *nppctx = ctx->npp_ctx;
	struct npp_buf_object *buf_obj = nppctx->cap_q_buf_obj;
	int buffersize, sizeimage = 0;

	mutex_lock(&nppctx->npp_mutex);
	sizeimage = nppctx->cap.fmt.pix_mp.plane_fmt[0].sizeimage;
	mutex_unlock(&nppctx->npp_mutex);

	buffersize = sizeimage * bufferNum;

	if(buf_obj->vaddr != NULL) {
		dev_err(dev->dev, "buf_obj->vaddr already allocate\n");
		return -ENOMEM;
	}

#ifndef ENABLE_NPP_FPGA_TEST
	buf_obj->vaddr = dma_alloc_coherent(dev->dev, buffersize,
											&buf_obj->paddr,
											GFP_KERNEL);
#endif

	if (!buf_obj->vaddr) {
		dev_err(dev->dev, "failed to allocate buf_obj buffer\n");
		return -ENOMEM;
	}

	buf_obj->dev = dev->dev;
	buf_obj->size = buffersize;

	return 0;
}

void npp_object_dma_free_capture(struct drv_npp_ctx *ctx)
{

	struct npp_ctx *nppctx = ctx->npp_ctx;
	struct npp_buf_object *buf_obj = nppctx->cap_q_buf_obj;

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

void *npp_alloc_context_capture(struct v4l2_fh *fh, struct videc_dev *v_dev)
{
	struct npp_ctx *ctx = NULL;
	int i;
	uint32_t flag;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ctx;

	if (v_dev->rpc_opt == RPC_AUDIO)
		flag = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC;
	else
		flag = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_HIFIACC;

	ctx->cap_q_buf_obj =
	_npp_buffer_create(v_dev, "rtk_media_heap", flag);
	

	if (!ctx->cap_q_buf_obj) {
		dev_err(v_dev->dev, "allocate npp cap_q_buf_obj failed\n");
		kfree(ctx);
		return NULL;
	}

	ctx->cap.type = -1;
	ctx->seq_cap = 0;
	ctx->rpc_opt = v_dev->rpc_opt;
	ctx->rect.left = 0;
	ctx->rect.top = 0;
	ctx->rect.width = 0;
	ctx->rect.height = 0;

	for (i = 0; i < CAP_BUF_COUNT_MAX; i++) {
		ctx->plock_offset[i] = -1;
		ctx->buffer_status[i] = DEFAULT;
	}

	mutex_init(&ctx->npp_mutex);
	mutex_init(&ctx->buffer_lock);
	INIT_LIST_HEAD(&ctx->buffers_head);//
	INIT_WORK(&ctx->frame_work, npp_get_frame_worker);

	init_cap_fmt(fh, &ctx->cap);

	return ctx;
}

void npp_free_context_capture(void *ctx)
{
	struct npp_ctx *ctx_npp = (struct npp_ctx *)ctx;

	if (ctx_npp->cap_q_buf_obj != NULL)
		_npp_buffer_free(ctx_npp->cap_q_buf_obj);

	kfree(ctx);
}

static int krpc_rcpu_cb(struct rtk_krpc_ept_info *krpc_ept_info, char *buf)
{
	uint32_t *tmp;
	struct rpc_struct *rpc = (struct rpc_struct *)buf;

	if (rpc->programID == REPLYID) {
		tmp = (uint32_t *) (buf + sizeof(struct rpc_struct));
		*(krpc_ept_info->retval) = *(tmp + 1);

		complete(&krpc_ept_info->ack);
	}

	return 0;
}

static int _npp_ept_init(struct rtk_krpc_ept_info *krpc_ept_info)
{
	return  krpc_info_init(krpc_ept_info, "rtk-npp", krpc_rcpu_cb);
}

static int _npp_ept_deinit(struct rtk_krpc_ept_info *krpc_ept_info)
{
	krpc_info_deinit(krpc_ept_info);
	return 0;
}

static char *_prepare_rpc_data(struct rtk_krpc_ept_info *krpc_ept_info,
				uint32_t command, uint32_t param1,
				uint32_t param2, int *len)
{
	struct rpc_struct *rpc;
	uint32_t *tmp;
	char *buf;

	*len = sizeof(struct rpc_struct) + 3 * sizeof(uint32_t);
	buf = kmalloc(sizeof(struct rpc_struct) + 3 * sizeof(uint32_t), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	rpc = (struct rpc_struct *)buf;
	rpc->programID = KERNELID;
	rpc->versionID = KERNELID;
	rpc->procedureID = 0;
	rpc->taskID = krpc_ept_info->id;
	rpc->sysTID = krpc_ept_info->id;
	rpc->sysPID = krpc_ept_info->id;
	rpc->parameterSize = 3 * sizeof(uint32_t);
	rpc->mycontext = 0;
	tmp = (uint32_t *) (buf + sizeof(struct rpc_struct));
	*tmp = command;
	*(tmp + 1) = param1;
	*(tmp + 2) = param2;

	return buf;
}

int _npp_send_rpc(struct device *dev, struct rtk_krpc_ept_info *krpc_ept_info,
		char *buf, int len, uint32_t *retval)
{
	int ret;

	mutex_lock(&krpc_ept_info->send_mutex);

	krpc_ept_info->retval = retval;
	ret = rtk_send_rpc(krpc_ept_info, buf, len);
	if (ret < 0) {
		dev_err(dev, "[%s] send rpc failed\n", krpc_ept_info->name);
		mutex_unlock(&krpc_ept_info->send_mutex);
		return ret;
	}
	if (!wait_for_completion_timeout(&krpc_ept_info->ack, RPC_TIMEOUT)) {
		dev_err(dev, "[%s] kernel rpc timeout\n", krpc_ept_info->name);
		rtk_krpc_dump_ringbuf_info(krpc_ept_info);
		mutex_unlock(&krpc_ept_info->send_mutex);
		return -EINVAL;
	}
	mutex_unlock(&krpc_ept_info->send_mutex);

	return 0;
}

static int _send_rpc(struct npp_rpc_info *rpc_info, int opt, uint32_t command,
	uint32_t param1, uint32_t param2, uint32_t *retval)
{
	int ret = 0;
	char *buf;
	int len;

	if (opt == RPC_AUDIO) {
		buf = _prepare_rpc_data(rpc_info->acpu_ept_info, command, param1, param2, &len);
		if (!IS_ERR(buf)) {
			ret = _npp_send_rpc(rpc_info->dev, rpc_info->acpu_ept_info,
				buf, len, retval);
			kfree(buf);
		}
	} else if (opt == RPC_HIFI) {
		buf = _prepare_rpc_data(rpc_info->hifi_ept_info, command, param1, param2, &len);
		if (!IS_ERR(buf)) {
			ret = _npp_send_rpc(rpc_info->dev, rpc_info->hifi_ept_info,
				buf, len, retval);
			kfree(buf);
		}
	}

	return ret;
}

static int _npp_rpc_init_ringbuffer(struct RPC_RINGBUFFER *ringbuffer, int opt)
{
	struct video_rpc_ringbuffer_t *rpc;
	unsigned int offset, rpc_ret;
	unsigned int command;
	struct RPCRES_LONG *retval;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc->instanceID = htonl(ringbuffer->instanceID);
		rpc->pinID = htonl(ringbuffer->pinID);
		rpc->readPtrIndex = htonl(ringbuffer->readPtrIndex);
		rpc->pRINGBUFF_HEADER = htonl(ringbuffer->pRINGBUFF_HEADER);
		offset = get_rpc_alignment_offset(sizeof(*rpc));
	} else {
		rpc->instanceID = ringbuffer->instanceID;
		rpc->pinID = ringbuffer->pinID;
		rpc->readPtrIndex = ringbuffer->readPtrIndex;
		rpc->pRINGBUFF_HEADER = ringbuffer->pRINGBUFF_HEADER;
		offset = ALIGN(sizeof(*rpc), RPC_ALIGN_SZ);
	}

	retval = (struct RPCRES_LONG *)((unsigned char *)rpc + offset);

	/* VIDEO_RPC_ToAgent_InitRingBuffer_0: ENUM_VIDEO_KERNEL_RPC_INITRINGBUFFER = 46 */
	command = ENUM_VIDEO_KERNEL_RPC_INITRINGBUFFER;
	_send_rpc(rpc_info, opt, command, rpc_info->paddr, rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if ((rpc_ret != S_OK) || (retval->result != S_OK))
		return -1;

	return 0;
}

int npp_rpc_buffer_allocate(struct videc_dev *dev)
{
	/* Allocate RPC buffer */
	rpc_info = kzalloc(sizeof(*rpc_info), GFP_KERNEL);
	if (rpc_info == NULL)
		return -1;

	rpc_info->vaddr =
	dma_alloc_coherent(dev->dev, RPC_BUFFER_SIZE, &rpc_info->paddr, GFP_KERNEL);
	if (!rpc_info->vaddr) {
		dev_err(dev->dev, "allocate RPC buffer failed\n");
		kfree(rpc_info);
		return -1;
	}

	rpc_info->dev = dev->dev;
	return 0;
}

int npp_ringbuffer_buffer_allocate(struct videc_dev *dev)
{
	/* Allocate ICQ ringbuffer */
	rb_icq = kzalloc(sizeof(*rb_icq), GFP_KERNEL);
	if (rb_icq == NULL)
		return -1;
	rb_icq->vaddr = dma_alloc_coherent(dev->dev, 65 * 1024,
					&rb_icq->paddr, GFP_KERNEL);
	if (!rb_icq->vaddr) {
		kfree(rpc_info);
		return -1;
	}

	/* Allocate writeback ringbuffer */
	rb_wb = kzalloc(sizeof(*rb_wb), GFP_KERNEL);
	if (rb_wb == NULL)
		return -1;
	rb_wb->vaddr = dma_alloc_coherent(dev->dev, 65 * 1024,
					&rb_wb->paddr, GFP_KERNEL);
	if (!rb_wb->vaddr) {
		kfree(rpc_info);
		return -1;
	}

	return 0;
}

void npp_rpc_buffer_release(struct videc_dev *dev)
{
	if (rpc_info != NULL) {
		if (rpc_info->vaddr != NULL)
			dma_free_coherent(dev->dev, RPC_BUFFER_SIZE,
		 rpc_info->vaddr, rpc_info->paddr);

		kfree(rpc_info);
		rpc_info = NULL;
	}
}

void npp_ringbuffer_buffer_release(struct videc_dev *dev)
{
	if (rb_icq != NULL) {
		if (rb_icq->vaddr != NULL)
			dma_free_coherent(dev->dev, 65 * 1024, rb_icq->vaddr, rb_icq->paddr);

		kfree(rb_icq);
		rb_icq = NULL;
	}
	if (rb_wb != NULL) {
		if (rb_wb->vaddr != NULL)
			dma_free_coherent(dev->dev, 65 * 1024, rb_wb->vaddr, rb_wb->paddr);

		kfree(rb_wb);
		rb_wb = NULL;
	}
}

static int _npp_init_ringbuffer(struct npp_ringbuffer_info *rb,
				unsigned int size, unsigned int pinid, int opt)
{
	struct RPC_RINGBUFFER ringbuffer;
	int ret;

	rb->ringheader =
		(struct RINGBUFFER_HEADER *) ((unsigned long)(rb->vaddr) + (size));

	if (opt == RPC_AUDIO) {
		rb->ringheader->beginAddr = htonl((long)(0xffffffff & rb->paddr));
		rb->ringheader->size = htonl(size);
		rb->ringheader->bufferID = htonl(1);
	} else {
		rb->ringheader->beginAddr = (long)(0xffffffff & rb->paddr);
		rb->ringheader->size = size;
		rb->ringheader->bufferID = 1;
	}
	rb->ringheader->writePtr = rb->ringheader->beginAddr;
	rb->ringheader->readPtr[0] = rb->ringheader->beginAddr;

	memset(&ringbuffer, 0, sizeof(ringbuffer));
	ringbuffer.instanceID = rpc_instance_id;
	ringbuffer.pinID = pinid;
	ringbuffer.readPtrIndex = 0;
	ringbuffer.pRINGBUFF_HEADER = (long)(0xffffffff & (rb->paddr)) + size;

	ret = _npp_rpc_init_ringbuffer(&ringbuffer, opt);
	if (ret < 0)
		return ret;

	return 0;
}

int _npp_ringbuffer_initialize(struct videc_dev *dev)
{

	int ret;

	/* Init ICQ ringbuffer */
	ret = _npp_init_ringbuffer(rb_icq, 64 * 1024, 0, dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, "_npp_init_ringbuffer failed\n");
		return ret;
	}

	/* Init writeback ringbuffer */
	ret = _npp_init_ringbuffer(rb_wb, 64 * 1024, WB_PINID, dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, " _npp_init_ringbuffer WB_PINID failed\n");
		return ret;
	}

	return 0;
}

static int _npp_rpc_npp_destroy(int opt)
{

	struct video_rpc_npp_destroy_t *rpc;
	unsigned int offset, rpc_ret;
	unsigned int command;
	struct RPCRES_LONG *retval;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc->instanceID = htonl(rpc_instance_id);
		offset = get_rpc_alignment_offset(sizeof(rpc->instanceID));
	} else {
		rpc->instanceID = rpc_instance_id;
		offset = ALIGN(sizeof(rpc->instanceID), RPC_ALIGN_SZ);
	}

	retval = (struct RPCRES_LONG *)((unsigned char *)rpc + offset);

	/* VIDEO_RPC_VOUT_NPP_Destroy_0: ENUM_VIDEO_KERNEL_RPC_NPP_Destroy = 79 */
	command = ENUM_VIDEO_KERNEL_RPC_NPP_Destroy;
	_send_rpc(rpc_info, opt, command, rpc_info->paddr, rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if ((rpc_ret != S_OK) || (retval->result != S_OK))
		return -1;

	return 0;
}

static int _npp_rpc_destroy(int opt)
{
	struct video_rpc_destroy_t *rpc;
	unsigned int offset, rpc_ret;
	unsigned int command;
	struct RPCRES_LONG *retval;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc->instanceID = htonl(rpc_instance_id);
		offset = get_rpc_alignment_offset(sizeof(rpc->instanceID));
	} else {
		rpc->instanceID = rpc_instance_id;
		offset = ALIGN(sizeof(rpc->instanceID), RPC_ALIGN_SZ);
	}

	retval = (struct RPCRES_LONG *)((unsigned char *)rpc + offset);

	/* VIDEO_RPC_ToAgent_Destroy_0: ENUM_VIDEO_KERNEL_RPC_DESTROY = 55 */
	command = ENUM_VIDEO_KERNEL_RPC_DESTROY;
	_send_rpc(rpc_info, opt, command, rpc_info->paddr, rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if ((rpc_ret != S_OK) || (retval->result != S_OK))
		return -1;

	return 0;
}

int npp_destroy(struct videc_dev *dev)
{
	int ret, retval = 0;

	ret = _npp_rpc_npp_destroy(dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, "_npp_rpc_npp_destroy failed\n");
		retval = ret;
	}

	ret = _npp_rpc_destroy(dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, "_npp_rpc_destroy failed\n");
		retval = ret;
	}

	if (dev->rpc_opt == RPC_AUDIO)
		_npp_ept_deinit(rpc_info->acpu_ept_info);
	else
		_npp_ept_deinit(rpc_info->hifi_ept_info);

	return retval;
}

static int _npp_krpc_init(int rpc_opt)
{
	struct device_node *np = rpc_info->dev->of_node;
	int ret;

	if (IS_ENABLED(CONFIG_RPMSG_RTK_RPC)) {
		if (rpc_opt == RPC_AUDIO) {
			rpc_info->acpu_ept_info = of_krpc_ept_info_get(np, 0);
			if (IS_ERR(rpc_info->acpu_ept_info)) {
				ret = PTR_ERR(rpc_info->acpu_ept_info);
				if (ret == -EPROBE_DEFER)
					pr_warn("[rtk-npp] krpc ept info not ready, retry\n");
				else
					pr_warn("[rtk-npp] cannot get hifi ept info: %d\n", ret);

				return ret;
			}
			_npp_ept_init(rpc_info->acpu_ept_info);
		} else {
			rpc_info->hifi_ept_info = of_krpc_ept_info_get(np, 0);
			if (IS_ERR(rpc_info->hifi_ept_info)) {
				ret = PTR_ERR(rpc_info->hifi_ept_info);
				if (ret == -EPROBE_DEFER)
					pr_warn("[rtk-npp] hifi ept info not ready, retry\n");
				else
					pr_warn("[rtk-npp] cannot get hifi ept info: %d\n", ret);

				return ret;
			}
			_npp_ept_init(rpc_info->hifi_ept_info);
		}
	} else {
		pr_warn("[rtk-npp] CONFIG_RPMSG_RTK_RPC not enable\n");
		return -1;
	}

	return 0;
}

static int _npp_rpc_create(int opt)
{
	struct video_rpc_create_t *rpc;
	unsigned int offset, rpc_ret;
	unsigned int command;
	struct RPCRES_LONG *retval;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc->instance.type = htonl(VF_TYPE_VIDEO_OUT);
		offset = get_rpc_alignment_offset(sizeof(rpc->instance));
	} else {
		rpc->instance.type = VF_TYPE_VIDEO_OUT;
		offset = ALIGN(sizeof(rpc->instance), RPC_ALIGN_SZ);
	}

	retval = (struct RPCRES_LONG *)((unsigned char *)rpc + offset);

	/* VIDEO_RPC_ToAgent_Create_0: ENUM_VIDEO_KERNEL_RPC_CREATE = 41 */
	command = ENUM_VIDEO_KERNEL_RPC_CREATE;
	_send_rpc(rpc_info, opt, command, rpc_info->paddr, rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO) {
		retval->result = ntohl(retval->result);
		retval->data = ntohl(retval->data);
	}

	if ((rpc_ret != S_OK) || (retval->result != S_OK))
		return -1;

	rpc_instance_id = retval->data;

	return 0;
}

static int _npp_rpc_npp_init(int opt)
{

	struct video_rpc_npp_init_t *rpc;
	unsigned int offset;
	unsigned int command;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	/* Use no parameter */
	offset = 0;

	/* VIDEO_RPC_VOUT_NPP_Init_0: ENUM_VIDEO_KERNEL_RPC_NPP_Init = 78 */
	command = ENUM_VIDEO_KERNEL_RPC_NPP_Init;
	_send_rpc(rpc_info, opt, command, rpc_info->paddr, rpc_info->paddr + offset, &rpc->ret);

	if (opt == RPC_AUDIO)
		rpc->retval.result = ntohl(rpc->retval.result);

	if ((rpc->ret != S_OK) || (rpc->retval.result != S_OK))
		return -1;

	return 0;
}

int npp_rpc_init_check(struct videc_dev *dev)
{
	int ret;

	ret = _npp_krpc_init(dev->rpc_opt);
	if (ret < 0) {
		dev_warn(dev->dev, " _npp_krpc_init not finished ret = %d, rpc not ready\n", ret);
	} else {
		if (dev->rpc_opt == RPC_AUDIO)
			_npp_ept_deinit(rpc_info->acpu_ept_info);
		else
			_npp_ept_deinit(rpc_info->hifi_ept_info);
	}

	return ret;
}

int npp_initialize(struct videc_dev *dev)
{

	int ret, retval = 0;

	ret = _npp_krpc_init(dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, " _npp_krpc_init failed\n");
		return ret;
	}

	ret = _npp_rpc_create(dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, " _npp_rpc_create failed\n");
		goto rpc_deinit;
	}

	ret = _npp_rpc_npp_init(dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, " _npp_rpc_npp_init failed\n");
		goto rpc_destroy;
	}

	ret = _npp_ringbuffer_initialize(dev);
	if (ret < 0) {
		dev_err(dev->dev, "_npp_ringbuffer_initialize failed\n");
		goto npp_destroy;
	}

	return ret;

npp_destroy:
		retval = _npp_rpc_npp_destroy(dev->rpc_opt);
		if (retval < 0) {
			dev_err(dev->dev, "_npp_rpc_npp_destroy failed\n");
			return retval;
		}
rpc_destroy:
		retval = _npp_rpc_destroy(dev->rpc_opt);
		if (retval < 0) {
			dev_err(dev->dev, "_npp_rpc_destroy failed\n");
			return retval;
		}
rpc_deinit:
		if (dev->rpc_opt == RPC_AUDIO)
			_npp_ept_deinit(rpc_info->acpu_ept_info);
		else
			_npp_ept_deinit(rpc_info->hifi_ept_info);


	return ret;
}

static int _npp_querydisplaywin(int opt)
{
	struct video_rpc_npp_query_in_t *rpc_in;
	struct video_rpc_npp_query_out_t *rpc_out;
	unsigned int offset, rpc_ret, result;
	unsigned int command;

	rpc_in = rpc_info->vaddr;
	memset(rpc_in, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc_in->plane = htonl(VO_VIDEO_PLANE_V1);
		offset = get_rpc_alignment_offset(sizeof(rpc_in->plane));
	} else {
		rpc_in->plane = VO_VIDEO_PLANE_V1;
		offset = ALIGN(sizeof(rpc_in->plane), RPC_ALIGN_SZ);
	}

	rpc_out = (struct video_rpc_npp_query_out_t *)((unsigned char *)rpc_in + offset);

	/* ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_WIN = 29 */
	command = ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_WIN;
	_send_rpc(rpc_info, opt, command, rpc_info->paddr, rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO)
		result = ntohl(rpc_out->result);
	else
		result = rpc_out->result;

	if ((rpc_ret != S_OK) || (result == 0))
		return -1;

	return 0;
}

int npp_querydisplaywin(int rpc_opt)
{
	int ret;

	ret = _npp_querydisplaywin(rpc_opt);
	if (ret < 0)
		return ret;

	return 0;
}

static int _npp_rpc_config(int type, int opt)
{
	struct video_rpc_config_writeback_flow_t *rpc;
	unsigned int offset, rpc_ret;
	unsigned int command;
	struct RPCRES_LONG *retval;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc->flow.instanceID = htonl(rpc_instance_id);
		rpc->flow.type = htonl(type);
		offset = get_rpc_alignment_offset(sizeof(rpc->flow));
	} else {
		rpc->flow.instanceID = rpc_instance_id;
		rpc->flow.type = type;
		offset = ALIGN(sizeof(rpc->flow), RPC_ALIGN_SZ);
	}

	retval = (struct RPCRES_LONG *)((unsigned char *)rpc + offset);

	/* VIDEO_RPC_ToAgent_ConfigWriteBackFlow_0:ENUM_VIDEO_KERNEL_RPC_ConfigWriteBackFlow = 77 */
	command = ENUM_VIDEO_KERNEL_RPC_ConfigWriteBackFlow;
	_send_rpc(rpc_info, opt, command, rpc_info->paddr,
		rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if ((rpc_ret != S_OK) || (retval->result != S_OK))
		return -1;

	return 0;
}


static int _npp_rpc_run(int opt)
{
	struct video_rpc_run_t *rpc;
	unsigned int offset, rpc_ret;
	unsigned int command;
	struct RPCRES_LONG *retval;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc->instanceID = htonl(rpc_instance_id);
		offset = get_rpc_alignment_offset(sizeof(rpc->instanceID));
	} else {
		rpc->instanceID = rpc_instance_id;
		offset = ALIGN(sizeof(rpc->instanceID), RPC_ALIGN_SZ);
	}

	retval = (struct RPCRES_LONG *)((unsigned char *)rpc + offset);

	/* VIDEO_RPC_ToAgent_Run_0: ENUM_VIDEO_KERNEL_RPC_RUN = 45 */
	command = ENUM_VIDEO_KERNEL_RPC_RUN;
	_send_rpc(rpc_info, opt, command,
		 rpc_info->paddr, rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if ((rpc_ret != S_OK) || (retval->result != S_OK))
		return -1;

	return 0;
}

void _npp_allocate_lock_buffer(struct device *dev)
{
	lock_info = kzalloc(sizeof(*lock_info), GFP_KERNEL);
	lock_info->vaddr = dma_alloc_coherent(dev, PLOCK_MAX_BUFFER_INDEX,
						&lock_info->paddr, GFP_KERNEL);


	recv_info = kzalloc(sizeof(*recv_info), GFP_KERNEL);
	recv_info->vaddr = dma_alloc_coherent(dev, PLOCK_MAX_BUFFER_INDEX,
						&recv_info->paddr, GFP_KERNEL);

	memset(lock_info->vaddr, PLOCK_QPEND, PLOCK_MAX_BUFFER_INDEX);
	memset(recv_info->vaddr, PLOCK_INIT, PLOCK_MAX_BUFFER_INDEX);

}

int npp_run_start(struct videc_dev *dev)
{
	int ret;

	ret = _npp_rpc_config(VSYNC_V1_NPU_PP, dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, " _npp_rpc_config failed\n");
		return ret;
	}

	/* Run */
	ret = _npp_rpc_run(dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, " _npp_rpc_run failed\n");
		return ret;
	}

	_npp_allocate_lock_buffer(rpc_info->dev);

	return 0;
}

void _npp_free_lock_buffer(struct device *dev)
{
	if (lock_info != NULL) {
		if (lock_info->vaddr != NULL)
			dma_free_coherent(dev, PLOCK_MAX_BUFFER_INDEX,
					lock_info->vaddr, lock_info->paddr);
		kfree(lock_info);
		lock_info = NULL;
	}

	if (recv_info != NULL) {
		if (recv_info->vaddr != NULL)
			dma_free_coherent(dev, PLOCK_MAX_BUFFER_INDEX,
					recv_info->vaddr, recv_info->paddr);
		kfree(recv_info);
		recv_info = NULL;
	}
}

static int _npp_rpc_pause(int opt)
{
	struct video_rpc_run_t *rpc;
	unsigned int offset, rpc_ret;
	unsigned int command;
	struct RPCRES_LONG *retval;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc->instanceID = htonl(rpc_instance_id);
		offset = get_rpc_alignment_offset(sizeof(rpc->instanceID));
	} else {
		rpc->instanceID = rpc_instance_id;
		offset = ALIGN(sizeof(rpc->instanceID), RPC_ALIGN_SZ);
	}

	retval = (struct RPCRES_LONG *)((unsigned char *)rpc + offset);

	/* VIDEO_RPC_ToAgent_Pause_0: ENUM_VIDEO_KERNEL_RPC_PAUSE = 53 */
	command = ENUM_VIDEO_KERNEL_RPC_PAUSE;
	_send_rpc(rpc_info, opt, command,
		 rpc_info->paddr, rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if ((rpc_ret != S_OK) || (retval->result != S_OK))
		return -1;

	return 0;
}

static int _npp_rpc_stop(int opt)
{
	struct video_rpc_run_t *rpc;
	unsigned int offset, rpc_ret;
	unsigned int command;
	struct RPCRES_LONG *retval;

	rpc = rpc_info->vaddr;
	memset(rpc, 0, RPC_BUFFER_SIZE);

	if (opt == RPC_AUDIO) {
		rpc->instanceID = htonl(rpc_instance_id);
		offset = get_rpc_alignment_offset(sizeof(rpc->instanceID));
	} else {
		rpc->instanceID = rpc_instance_id;
		offset = ALIGN(sizeof(rpc->instanceID), RPC_ALIGN_SZ);
	}

	retval = (struct RPCRES_LONG *)((unsigned char *)rpc + offset);

	/* VIDEO_RPC_ToAgent_Stop_0: ENUM_VIDEO_KERNEL_RPC_STOP = 54 */
	command = ENUM_VIDEO_KERNEL_RPC_STOP;
	_send_rpc(rpc_info, opt, command,
		 rpc_info->paddr, rpc_info->paddr + offset, &rpc_ret);

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if ((rpc_ret != S_OK) || (retval->result != S_OK))
		return -1;

	return 0;
}

int npp_run_stop(struct videc_dev *dev)
{

	int ret;

	_npp_free_lock_buffer(rpc_info->dev);

	/* Pause */
	ret = _npp_rpc_pause(dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, "_npp_rpc_pause failed\n");
		return ret;
	}

	/* Stop */
	ret = _npp_rpc_stop(dev->rpc_opt);
	if (ret < 0) {
		dev_err(dev->dev, "_npp_rpc_stop failed\n");
		return ret;
	}

	return 0;
}
