/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2021 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-dma-contig.h>
#include "drv_if.h"
#include "vpu.h"
#include "debug.h"
#include <linux/dma-map-ops.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>

unsigned int vpu_debug = 0;
EXPORT_SYMBOL(vpu_debug);
MODULE_PARM_DESC(debug, "activates debug info, where each bit enables a debug category.\n"
"\t\tBit 0 (0x01) will enable input messages (output type)\n"
"\t\tBit 1 (0x02) will enable output messages (capture type)\n");
module_param_named(debug, vpu_debug, int, 0600);

#define VPU_NAME		"realtek-vpu"

#define INVERT_BITVAL_1 (~1)

ssize_t get_video_status(struct device *dev,
	struct device_attribute *attr, char *buf);

static DEVICE_ATTR(video_status, S_IRUSR, get_video_status, NULL);

static char hasVideo = 0;

/*
struct videc_dev {
	struct v4l2_device	v4l2_dev;
	struct video_device	video_dev;

	atomic_t		num_inst;
	struct mutex		dev_mutex;
	spinlock_t		irqlock;

	struct v4l2_m2m_dev	*m2m_dev;
	struct device		*dev;
};
*/
static void vpu_dev_release(struct device *dev)
{}

static struct platform_device videc_pdev = {
	.name		= VPU_NAME,
	.dev.release	= vpu_dev_release,
};

enum {
	V4L2_M2M_SRC = 0,
	V4L2_M2M_DST = 1,
};

static inline struct videc_ctx *file2ctx(struct file *file)
{
	return container_of(file->private_data, struct videc_ctx, fh);
}

void vpu_printk(const char *level, unsigned int category,
		const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	if (category != VPU_DBG_NONE && !(vpu_debug & category))
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	printk("%s" "[VDEC] %s %pV",
	       level, strcmp(level, KERN_ERR) == 0 ? " *ERROR*" : "", &vaf);

	va_end(args);
}
EXPORT_SYMBOL(vpu_printk);


/*
 * mem2mem callbacks
 */

/**
 * job_ready() - check whether an instance is ready to be scheduled to run
 */
static int job_ready(void *priv)
{
	//struct videc_ctx *ctx = priv;
	vpu_input_dbg("%s\n", __func__);
#if 0 // FIXME: shall enable?
	if (v4l2_m2m_num_src_bufs_ready(ctx->fh.m2m_ctx) < ctx->translen
	    || v4l2_m2m_num_dst_bufs_ready(ctx->fh.m2m_ctx) < ctx->translen) {
		dprintk(ctx->dev, "Not enough buffers available\n");
		return 0;
	}
#endif
	return 1;
}

static void job_abort(void *priv)
{
	struct videc_ctx *ctx = priv;
	struct videc_dev *dev = ctx->dev;
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	vpu_input_dbg("%s\n", __func__);

	if (!op)
		return;
	/* Will cancel the transaction in the next interrupt handler */
	op->vpu_abort(priv, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	op->vpu_abort(priv, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	v4l2_m2m_job_finish(dev->m2m_dev, ctx->fh.m2m_ctx);
	vpu_input_dbg("%s done\n", __func__);
}

/* device_run() - prepares and starts the device
 *
 * This simulates all the immediate preparations required before starting
 * a device. This will be called by the framework when it decides to schedule
 * a particular instance.
 */
static void device_run(void *priv)
{
	vpu_input_dbg("%s\n", __func__);
}

/*
 * video ioctls
 */
static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	strncpy(cap->driver, VPU_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, VPU_NAME, sizeof(cap->card) - 1);
	snprintf(cap->bus_info, sizeof(cap->bus_info),
			"platform:%s", VPU_NAME);
	cap->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int videc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}


	return op->vpu_enum_fmt_cap(f);
}

static int videc_enum_fmt_vid_out(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	return op->vpu_enum_fmt_out(f);
}

static int videc_g_fmt(struct file *file, void *priv,
				struct v4l2_format *f)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	return op->vpu_g_fmt(priv, f);
}

static int videc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	return op->vpu_try_fmt_cap(f);
}

static int videc_try_fmt_vid_out(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	return op->vpu_try_fmt_out(f);
}

static int videc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	return op->vpu_s_fmt_cap(priv, f);
}

static int videc_s_fmt_vid_out(struct file *file, void *priv,
				struct v4l2_format *f)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	return op->vpu_s_fmt_out(priv, f);
}

static int vpu_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct videc_ctx *ctx =
			container_of(ctrl->handler, struct videc_ctx, ctrls);

	switch (ctrl->id) {
	case RTK_V4L2_SET_SECURE:
		ctx->params.is_secure = ctrl->val;
#ifndef ENABLE_TEE_DRM_FLOW
		if(ctx->params.is_secure) {
			ctx->params.is_secure = 0;
			vpu_err("Driver doesn't support secure, force normal buffer\n");
		}
#endif
		break;
	default:
		vpu_err("Invalid control, id=%d, val=%d\n", ctrl->id, ctrl->val);
		return -EINVAL;
	}
	return 0;
}

static const struct v4l2_ctrl_ops vpu_ctrl_ops = {
	.s_ctrl = vpu_s_ctrl,
};

static const struct v4l2_ctrl_config rtk_ctrl_set_secure = {
        .ops = &vpu_ctrl_ops,
        .id = RTK_V4L2_SET_SECURE,
        .name = "Enable Secure Buffer(SVP)",
        .type = V4L2_CTRL_TYPE_INTEGER,
        .def = 0,
        .min = 0,
        .max = 1,
        .step = 1,
};

int vpu_ctrls_setup(struct videc_ctx *ctx)
{
	v4l2_ctrl_handler_init(&ctx->ctrls, 1);

	v4l2_ctrl_new_custom(&ctx->ctrls, &rtk_ctrl_set_secure, NULL);

	if (ctx->ctrls.error) {
		vpu_err("control initialization error (%d)",
			ctx->ctrls.error);
		return -EINVAL;
	}

	return v4l2_ctrl_handler_setup(&ctx->ctrls);
}

static int videc_reqbufs(struct file *file, void *priv,
				struct v4l2_requestbuffers *rb)
{
	int ret;

	if (V4L2_TYPE_IS_OUTPUT(rb->type) && rb->memory == V4L2_MEMORY_DMABUF)
	{
		struct vb2_queue *vq;
		struct v4l2_fh *fh = file->private_data;

		vq = v4l2_m2m_get_vq(fh->m2m_ctx, rb->type);
		vq->mem_ops = &vb2_dma_contig_memops;
	}

	ret = v4l2_m2m_ioctl_reqbufs(file, priv, rb);
	return ret;
}

static int videc_querybuf(struct file *file, void *priv,
				struct v4l2_buffer *buf)
{
	int ret;

	ret = v4l2_m2m_ioctl_querybuf(file, priv, buf);
	return ret;
}

static int videc_qbuf(struct file *file, void *priv,
				struct v4l2_buffer *buf)
{
	struct v4l2_fh *fh = file->private_data;

	if (V4L2_TYPE_IS_OUTPUT(buf->type) && buf->memory == V4L2_MEMORY_DMABUF)
	{
		struct vb2_queue *vq;
		struct v4l2_fh *fh = file->private_data;
		struct vb2_buffer *vb;

		vq = v4l2_m2m_get_vq(fh->m2m_ctx, buf->type);
		vb = vq->bufs[buf->index];
		vb->planes[0].length = PAGE_ALIGN(buf->length);
		buf->length = 0;
	}

	return v4l2_m2m_qbuf(file, fh->m2m_ctx, buf);
}

static int videc_create_bufs(struct file *file, void *priv,
				struct v4l2_create_buffers *create)
{
	int ret;

	ret = v4l2_m2m_ioctl_create_bufs(file, priv, create);
	return ret;
}
int videc_prepare_buf(struct file *file, void *priv,
			       struct v4l2_buffer *buf)
{
	int ret;

	ret = v4l2_m2m_ioctl_prepare_buf(file, priv, buf);
	return ret;
}

int videc_expbuf(struct file *file, void *priv,
				struct v4l2_exportbuffer *eb)
{
	int ret;

	ret = v4l2_m2m_ioctl_expbuf(file, priv, eb);

	return ret;
}

static int
videc_g_selection(struct file *file, void *fh, struct v4l2_selection *sel)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	struct v4l2_rect rsel;
	int ret;
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}
	ret = op->vpu_g_crop(fh, &rsel);

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		//todo
		/* fallthrough */
	case V4L2_SEL_TGT_CROP:
		if (sel->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
			return -EINVAL;
		//todo
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		//todo
		/* fallthrough */
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		if (sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		//todo
		break;
	default:
		return -EINVAL;
	}

	memcpy(&sel->r, &rsel, sizeof(struct v4l2_rect));

	return ret;
}

static int videc_frmsizeenum(struct file *file, void *priv,
				  struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index > 0)
		return -EINVAL;
	if (fsize->pixel_format == V4L2_PIX_FMT_HEVC)
	{
		fsize->discrete.width = 3840;
		fsize->discrete.height = 2160;
	}
	else 	if (fsize->pixel_format == V4L2_PIX_FMT_VP9)
	{
		fsize->discrete.width = 3840;
		fsize->discrete.height = 2160;
	}
	else if (fsize->pixel_format == V4L2_PIX_FMT_AV1)
	{
		fsize->discrete.width = 3840;
		fsize->discrete.height = 2160;
	}
	else if (fsize->pixel_format == V4L2_PIX_FMT_H264)
	{
		fsize->discrete.width = 3840;
		fsize->discrete.height = 2160;
	}
	else
	{
		fsize->discrete.width = 1920;
		fsize->discrete.height = 1080;
	}
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	return 0;
}

int videc_subscribe_event(struct v4l2_fh *fh,
				const struct v4l2_event_subscription *sub)
{
	switch (sub->type)
	{
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

static int videc_try_decoder_cmd(struct file *file, void *fh,
                                struct v4l2_decoder_cmd *dc)
{
	if (dc->cmd != V4L2_DEC_CMD_STOP)
		return -EINVAL;
#if 0
	if (dc->flags & V4L2_DEC_CMD_STOP_TO_BLACK)
		return -EINVAL;

	if (!(dc->flags & V4L2_DEC_CMD_STOP_IMMEDIATELY) && (dc->stop.pts != 0))
		return -EINVAL;
#endif
        return 0;
}

static int videc_decoder_cmd(struct file *file, void *fh,
                            struct v4l2_decoder_cmd *dc)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	int ret;

	ret = videc_try_decoder_cmd(file, fh, dc);
	if (ret < 0)
		return ret;

	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	ret = op->vpu_stop_cmd(fh);


	return 0;
}

static const struct v4l2_ioctl_ops vpu_ioctl_ops = {
	.vidioc_querycap	= vidioc_querycap,

	.vidioc_enum_fmt_vid_cap = videc_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	= videc_g_fmt,
	.vidioc_try_fmt_vid_cap	= videc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap	= videc_s_fmt_vid_cap,

	.vidioc_enum_fmt_vid_out = videc_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out	= videc_g_fmt,
	.vidioc_try_fmt_vid_out	= videc_try_fmt_vid_out,
	.vidioc_s_fmt_vid_out	= videc_s_fmt_vid_out,

	.vidioc_reqbufs		= videc_reqbufs,
	.vidioc_querybuf	= videc_querybuf,
	.vidioc_qbuf		= videc_qbuf,
	.vidioc_dqbuf		= v4l2_m2m_ioctl_dqbuf,
	.vidioc_prepare_buf	= videc_prepare_buf,
	.vidioc_create_bufs	= videc_create_bufs,
	.vidioc_expbuf		= v4l2_m2m_ioctl_expbuf,
	.vidioc_g_selection = videc_g_selection,
	.vidioc_streamon	= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,

	.vidioc_enum_framesizes   = videc_frmsizeenum,
	.vidioc_subscribe_event = videc_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,

	.vidioc_try_decoder_cmd = videc_try_decoder_cmd,
	.vidioc_decoder_cmd     = videc_decoder_cmd,
};


/*
 * Queue operations
 */

static int videc_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers, unsigned int *nplanes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	struct videc_ctx *ctx = vb2_get_drv_priv(vq);
	vpu_input_dbg("%s\n", __func__);
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	if (nplanes)
		*nplanes = 1;

	alloc_devs[0] = ctx->dev->v4l2_dev.dev;
	return op->vpu_queue_info(vq, nbuffers, &sizes[0]);
}

static int videc_buf_prepare(struct vb2_buffer *vb)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	int sizeimages, ret;

	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}

	ret = op->vpu_queue_info(vb->vb2_queue, NULL, &sizeimages);
	if (ret)
		return ret;

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		if (vbuf->field == V4L2_FIELD_ANY)
			vbuf->field = V4L2_FIELD_NONE;
		if (vbuf->field != V4L2_FIELD_NONE) {
			vpu_err("%s field isn't supported\n",
					__func__);
			return -EINVAL;
		}
	}

	if (vb2_plane_size(vb, 0) < sizeimages) {
		vpu_err("%s data will not fit into plane (%lu < %lu)\n",
				__func__, vb2_plane_size(vb, 0), (long)sizeimages);
		return -EINVAL;
	}
	return 0;
}
#if 0 //Keep for libmali verify, remove me while libmali finish metadata conversion
static void videc_buf_finish(struct vb2_buffer *vb)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();

	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return;
	}
	op->vpu_buf_finish(vb);
}
#endif
static void videc_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct videc_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return;
	}
	op->vpu_qbuf(&ctx->fh, vb);
}

static int videc_start_streaming(struct vb2_queue *q, unsigned count)
{
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	int ret;
	vpu_input_dbg("%s %s\n", __func__, V4L2_TYPE_TO_STR(q->type));
	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return 0;
	}
	ret = op->vpu_start_streaming(q, count);
	if (ret)
	{
		vpu_err("vpu_start_streaming fail ret %d\n", ret);
		return 0;
	}

	return 0;
}

static void videc_stop_streaming(struct vb2_queue *q)
{
	struct videc_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vbuf;
	unsigned long flags;
	const struct vpu_fmt_ops * op = get_vpu_fmt_ops();
	int ret;
	vpu_input_dbg("%s %s\n", __func__, V4L2_TYPE_TO_STR(q->type));
	for (;;) {
		if (V4L2_TYPE_IS_OUTPUT(q->type))
			vbuf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
		else
			vbuf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
		if (vbuf == NULL)
			break;
		spin_lock_irqsave(&ctx->dev->irqlock, flags);
		v4l2_m2m_buf_done(vbuf, VB2_BUF_STATE_ERROR);
		spin_unlock_irqrestore(&ctx->dev->irqlock, flags);
	}

	if (!op)
	{
		vpu_err("vpu ops is NULL\n");
		return;
	}

	ret = op->vpu_stop_streaming(q);
	if (ret)
	{
		vpu_err("vpu_stop_streaming fail ret %d\n", ret);
	}
	return;
}

static const struct vb2_ops vpu_qops = {
	.queue_setup	 = videc_queue_setup,
	.buf_prepare	 = videc_buf_prepare,
#if 0 //Keep for libmali verify, remove me while libmali finish metadata conversion
	.buf_finish      = videc_buf_finish,
#endif
	.buf_queue	 	 = videc_buf_queue,
	.start_streaming = videc_start_streaming,
	.stop_streaming  = videc_stop_streaming,
	.wait_prepare	 = vb2_ops_wait_prepare,
	.wait_finish	 = vb2_ops_wait_finish,
};

static int queue_init(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq)
{
	struct videc_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	//src_vq->io_modes = VB2_MMAP;
	src_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->ops = &vpu_qops;
	src_vq->mem_ops = &vb2_vmalloc_memops;
	//src_vq->mem_ops = &vb2_dma_contig_memops; //this could be chagned dynamic due to memory mode
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->dev->dev_mutex;
	src_vq->dev = ctx->dev->dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes = VB2_MMAP;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &vpu_qops;
	dst_vq->mem_ops = &vb2_vmalloc_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->dev->dev_mutex;
	dst_vq->dev = ctx->dev->dev;

	return vb2_queue_init(dst_vq);
}


/*
 * File operations
 */
static int vpu_open(struct file *file)
{
	struct videc_dev *dev = video_drvdata(file);
	struct videc_ctx *ctx = NULL;
	struct v4l2_ctrl_handler *hdl;
	int rc = 0;

	if (mutex_lock_interruptible(&dev->dev_mutex))
		return -ERESTARTSYS;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		rc = -ENOMEM;
		goto open_unlock;
	}

	ctx->vpu_ctx = vpu_alloc_context();
	if (!ctx->vpu_ctx) {
		rc = -ENOMEM;
		kfree(ctx);
		goto open_unlock;
	}

	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	ctx->dev = dev;

	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev, ctx, &queue_init);

	if (IS_ERR(ctx->fh.m2m_ctx)) {
		rc = PTR_ERR(ctx->fh.m2m_ctx);

		v4l2_ctrl_handler_free(hdl);
		vpu_free_context(ctx->vpu_ctx);
		kfree(ctx);
		goto open_unlock;
	}

	v4l2_fh_add(&ctx->fh);

	ctx->file = file;

	if ( vpu_ctrls_setup(ctx) ) {
		v4l2_err(&dev->v4l2_dev, "failed to setup realtek vpu controls\n");
		goto open_unlock;
	}

	ctx->fh.ctrl_handler = &ctx->ctrls;

	memset(&ctx->params, 0, sizeof(struct videc_params));
	ctx->params.is_adaptive_playback = 1;

	atomic_inc(&dev->num_inst);
	hasVideo = 1;

	vpu_info("Created instance: %p, m2m_ctx: %p\n",ctx, ctx->fh.m2m_ctx);

open_unlock:
	mutex_unlock(&dev->dev_mutex);
	return rc;
}

static int vpu_release(struct file *file)
{
	struct videc_dev *dev = video_drvdata(file);
	struct videc_ctx *ctx = file2ctx(file);

	vpu_info("Releasing instance %p\n", ctx);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mutex_lock(&dev->dev_mutex);
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	mutex_unlock(&dev->dev_mutex);

	vpu_free_context(ctx->vpu_ctx);

	v4l2_ctrl_handler_free(&ctx->ctrls);

	kfree(ctx);

	atomic_dec(&dev->num_inst);
	hasVideo = 0;

	vpu_info("Releasing done\n");
	return 0;
}

static const struct v4l2_file_operations vpu_fops = {
	.owner		= THIS_MODULE,
	.open		= vpu_open,
	.release	= vpu_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

static struct video_device vpu_videodev = {
	.name		= VPU_NAME,
	.vfl_dir	= VFL_DIR_M2M,
	.fops		= &vpu_fops,
	.ioctl_ops	= &vpu_ioctl_ops,
	.minor		= -1,
	.release	= video_device_release_empty,
};

static struct v4l2_m2m_ops m2m_ops = {
	.device_run	= device_run,
	.job_ready	= job_ready,
	.job_abort	= job_abort,
};

ssize_t get_video_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%d\n", hasVideo);
}

static int vpu_probe(struct platform_device *pdev)
{
	struct videc_dev *dev;
	struct video_device *video_dev;
	int ret;

	/* Allocate a new instance */
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->irqlock);

	/* Initialize the top-level structure */
	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		return ret;

	dev->dev = &pdev->dev;
	set_dma_ops(dev->dev, &rheap_dma_ops);
	atomic_set(&dev->num_inst, 0);
	mutex_init(&dev->dev_mutex);

	/* Initialize the video_device structure */
	dev->video_dev = vpu_videodev;
	video_dev = &dev->video_dev;
	video_dev->lock = &dev->dev_mutex;
	video_dev->v4l2_dev = &dev->v4l2_dev;
	video_dev->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;

	/* Register video4linux device */
	ret = video_register_device(video_dev, VFL_TYPE_VIDEO, 0);
	if (ret) {
		vpu_err("Failed to register video device\n");
		goto unreg_dev;
	}

	/* Set private data */
	video_set_drvdata(video_dev, dev);
	snprintf(video_dev->name, sizeof(video_dev->name), "%s", vpu_videodev.name);
	vpu_info("Device registered as /dev/video%d\n", video_dev->num);

	platform_set_drvdata(pdev, dev);

	/* Initialize per-driver m2m data */
	dev->m2m_dev = v4l2_m2m_init(&m2m_ops);
	if (IS_ERR(dev->m2m_dev)) {
		vpu_err("Failed to init mem2mem device\n");
		ret = PTR_ERR(dev->m2m_dev);
		goto err_m2m;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_video_status);
	if (ret < 0)
		vpu_err("failed to create v4l2 video attribute\n");
	return 0;

err_m2m:
	v4l2_m2m_release(dev->m2m_dev);
	video_unregister_device(&dev->video_dev);
unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);

	return ret;
}

static int vpu_remove(struct platform_device *pdev)
{
	struct videc_dev *dev = platform_get_drvdata(pdev);

	vpu_info("Removing %s\n", VPU_NAME);
	device_remove_file(&pdev->dev, &dev_attr_video_status);
	v4l2_m2m_release(dev->m2m_dev);
	video_unregister_device(&dev->video_dev);
	v4l2_device_unregister(&dev->v4l2_dev);

	return 0;
}

static struct platform_driver vpu_pdrv = {
	.probe		= vpu_probe,
	.remove		= vpu_remove,
	.driver		= {
		.name	= VPU_NAME,
	},
};

static void __exit vpu_exit(void)
{
	platform_driver_unregister(&vpu_pdrv);
	platform_device_unregister(&videc_pdev);
}

static int __init vpu_init(void)
{
	int ret;

	ret = platform_device_register(&videc_pdev);
	if (ret)
		return ret;

	ret = platform_driver_register(&vpu_pdrv);
	if (ret)
		platform_device_unregister(&videc_pdev);

	return ret;
}

module_init(vpu_init);
module_exit(vpu_exit);

MODULE_VERSION(RTK_VDEC_VERSION);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("William Lee <william.lee@realtek.com>");
MODULE_DESCRIPTION("Realtek V4L2 VE2 Codec Driver");
