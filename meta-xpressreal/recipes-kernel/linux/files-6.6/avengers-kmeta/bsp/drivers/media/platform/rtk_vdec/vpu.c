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
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "drv_if.h"
#include "debug.h"
#include "vpu.h"

struct vpu_misc {
    uint32_t 	VideoEngine;
	uint32_t	maxW;
	uint32_t	maxH;
	uint32_t	minW;
	uint32_t	minH;
	uint32_t 	bufcnt;
};

struct veng_ops *vpu_ve1_ops = NULL;
struct veng_ops *vpu_ve2_ops = NULL;

struct vpu_fmt {
	struct v4l2_format spec;
	struct vpu_misc misc;
};

void vpu_cap_buf_done(struct v4l2_fh *fh, struct vb2_v4l2_buffer *buf,
		       bool eos, enum vb2_buffer_state state);

const static struct vpu_fmt out_fmt[] = {
	/* video engine VE1 output format */
	{
		/* struct v4l2_pix_format */
		.spec.fmt.pix.width = 1920,
		.spec.fmt.pix.height = 1080,
		.spec.fmt.pix.pixelformat	= V4L2_PIX_FMT_H264,
		.spec.fmt.pix.field = V4L2_FIELD_NONE,
		.spec.fmt.pix.bytesperline = 1920,
		.spec.fmt.pix.sizeimage	= 3*1024*1024, // FIXME: 1024*1024
		.spec.fmt.pix.colorspace = V4L2_COLORSPACE_REC709,
		.spec.fmt.pix.priv = 0,
		.spec.fmt.pix.flags = 0,
		.spec.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT,
		.spec.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
		.spec.fmt.pix.xfer_func = V4L2_XFER_FUNC_DEFAULT,
		/* enum v4l2_buf_type */
		.spec.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT,
		/* struct vpu_misc */
		.misc.maxW = 3840,
		.misc.maxH = 2160,
		.misc.minW = 640,
		.misc.minW = 480,
		.misc.bufcnt = 4,
		.misc.VideoEngine = VIDEO_ENGINE_1,
	},
	{
		.spec.fmt.pix.width = 1920,
		.spec.fmt.pix.height = 1080,
		.spec.fmt.pix.pixelformat	= V4L2_PIX_FMT_MPEG2,
		.spec.fmt.pix.field = V4L2_FIELD_NONE,
		.spec.fmt.pix.bytesperline = 1920,
		.spec.fmt.pix.sizeimage	= 3*1024*1024, // FIXME: 1024*1024
		.spec.fmt.pix.colorspace = V4L2_COLORSPACE_REC709,
		.spec.fmt.pix.priv = 0,
		.spec.fmt.pix.flags = 0,
		.spec.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT,
		.spec.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
		.spec.fmt.pix.xfer_func = V4L2_XFER_FUNC_DEFAULT,
		.spec.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT,
		.misc.maxW = 3840,
		.misc.maxH = 2160,
		.misc.minW = 640,
		.misc.minW = 480,
		.misc.bufcnt = 4,
		.misc.VideoEngine = VIDEO_ENGINE_1,
	},
	{
		.spec.fmt.pix.width = 1920,
		.spec.fmt.pix.height = 1080,
		.spec.fmt.pix.pixelformat	= V4L2_PIX_FMT_MPEG4,
		.spec.fmt.pix.field = V4L2_FIELD_NONE,
		.spec.fmt.pix.bytesperline = 1920,
		.spec.fmt.pix.sizeimage	= 3*1024*1024, // FIXME: 1024*1024
		.spec.fmt.pix.colorspace = V4L2_COLORSPACE_REC709,
		.spec.fmt.pix.priv = 0,
		.spec.fmt.pix.flags = 0,
		.spec.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT,
		.spec.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
		.spec.fmt.pix.xfer_func = V4L2_XFER_FUNC_DEFAULT,
		.spec.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT,
		.misc.maxW = 3840,
		.misc.maxH = 2160,
		.misc.minW = 640,
		.misc.minW = 480,
		.misc.bufcnt = 4,
		.misc.VideoEngine = VIDEO_ENGINE_1,
	},
	/* video engine VE2 output format */
	{
		/* struct v4l2_pix_format */
		.spec.fmt.pix.width = 1920,
		.spec.fmt.pix.height = 1080,
		.spec.fmt.pix.pixelformat	= V4L2_PIX_FMT_HEVC,
		.spec.fmt.pix.field = V4L2_FIELD_NONE,
		.spec.fmt.pix.bytesperline = 1920,
		.spec.fmt.pix.sizeimage	= 3*1024*1024,
		.spec.fmt.pix.colorspace = V4L2_COLORSPACE_REC709,
		.spec.fmt.pix.priv = 0,
		.spec.fmt.pix.flags = 0,
		.spec.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT,
		.spec.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
		.spec.fmt.pix.xfer_func = V4L2_XFER_FUNC_DEFAULT,
		/* enum v4l2_buf_type */
		.spec.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT,
		/* struct vpu_misc */
		.misc.maxW = 3840,
		.misc.maxH = 2160,
		.misc.minW = 640,
		.misc.minW = 480,
		.misc.bufcnt = 8,
        .misc.VideoEngine = VIDEO_ENGINE_2,
	},
	{
		.spec.fmt.pix.width = 1920,
		.spec.fmt.pix.height = 1080,
		.spec.fmt.pix.pixelformat	= V4L2_PIX_FMT_VP9,
		.spec.fmt.pix.field = V4L2_FIELD_NONE,
		.spec.fmt.pix.bytesperline = 1920,
		.spec.fmt.pix.sizeimage	= 3*1024*1024,
		.spec.fmt.pix.colorspace = V4L2_COLORSPACE_REC709,
		.spec.fmt.pix.priv = 0,
		.spec.fmt.pix.flags = 0,
		.spec.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT,
		.spec.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
		.spec.fmt.pix.xfer_func = V4L2_XFER_FUNC_DEFAULT,
		.spec.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT,
		.misc.maxW = 3840,
		.misc.maxH = 2160,
		.misc.minW = 640,
		.misc.minW = 480,
		.misc.bufcnt = 8,
        .misc.VideoEngine = VIDEO_ENGINE_2,
	},
	{
		.spec.fmt.pix.width = 1920,
		.spec.fmt.pix.height = 1080,
		.spec.fmt.pix.pixelformat	= V4L2_PIX_FMT_AV1,
		.spec.fmt.pix.field = V4L2_FIELD_NONE,
		.spec.fmt.pix.bytesperline = 1920,
		.spec.fmt.pix.sizeimage	= 3*1024*1024,
		.spec.fmt.pix.colorspace = V4L2_COLORSPACE_REC709,
		.spec.fmt.pix.priv = 0,
		.spec.fmt.pix.flags = 0,
		.spec.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT,
		.spec.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
		.spec.fmt.pix.xfer_func = V4L2_XFER_FUNC_DEFAULT,
		.spec.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT,
		.misc.maxW = 3840,
		.misc.maxH = 2160,
		.misc.minW = 640,
		.misc.minW = 480,
		.misc.bufcnt = 8,
        .misc.VideoEngine = VIDEO_ENGINE_2,
	},
};

static const struct vpu_fmt cap_fmt[] = {
	{
		.spec.fmt.pix.width = 1920,
		.spec.fmt.pix.height = 1080,
		.spec.fmt.pix.pixelformat	= V4L2_PIX_FMT_NV12,
		.spec.fmt.pix.field = V4L2_FIELD_NONE,
		.spec.fmt.pix.bytesperline = 3840/16,
		.spec.fmt.pix.sizeimage	= 4096,
		.spec.fmt.pix.colorspace = V4L2_COLORSPACE_REC709,
		.spec.fmt.pix.priv = 0,
		.spec.fmt.pix.flags = 0,
		.spec.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT,
		.spec.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
		.spec.fmt.pix.xfer_func = V4L2_XFER_FUNC_DEFAULT,
		.spec.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.misc.maxW = 3840,
		.misc.maxH = 2160,
		.misc.minW = 640,
		.misc.minW = 480,
		.misc.bufcnt = 8,
        .misc.VideoEngine = VIDEO_OUTPUT_1,
	},
};

/*
 * Return vpu_ctx structure for a given struct v4l2_fh
 */
static struct vpu_ctx *fh_to_vpu(struct v4l2_fh *fh)
{
	struct videc_ctx *vid_ctx = container_of(fh, struct videc_ctx, fh);
	return vid_ctx->vpu_ctx;
}

/*
 * Return vpu_ctx structure for a given struct vb2_queue
 */
static struct vpu_ctx *vq_to_vpu(struct vb2_queue *q)
{
    struct videc_ctx *vid_ctx = vb2_get_drv_priv(q);
    return vid_ctx->vpu_ctx;
}

static int vpu_buf_done(struct v4l2_fh *fh, struct vb2_v4l2_buffer *v4l2_buf
    , uint32_t len, uint64_t pts, uint32_t sizeimage, uint32_t sequence, bool eos, bool no_frame)
{
	struct vpu_ctx *ctx = fh_to_vpu(fh);
	unsigned long flags;

	v4l2_buf = v4l2_m2m_dst_buf_remove(fh->m2m_ctx);
	if(!v4l2_buf) {
		vpu_warn("No dst buffer to remove\n");
		return -ENOBUFS;
	}
	v4l2_buf->field = V4L2_FIELD_NONE;
	v4l2_buf->flags = V4L2_BUF_FLAG_KEYFRAME;
	v4l2_buf->vb2_buf.timestamp = pts;
	v4l2_buf->vb2_buf.planes[0].bytesused = len;
	v4l2_buf->sequence = sequence++;
	mutex_lock(&ctx->vpu_mutex);
	ctx->seq_cap = sequence;
	mutex_unlock(&ctx->vpu_mutex);

	if(!no_frame)
		vb2_set_plane_payload(&v4l2_buf->vb2_buf, 0, sizeimage);
	else
		vb2_set_plane_payload(&v4l2_buf->vb2_buf, 0, 0);

	spin_lock_irqsave(&ctx->vpu_spin_lock, flags);

	vpu_cap_buf_done(fh, v4l2_buf, eos, VB2_BUF_STATE_DONE);

	spin_unlock_irqrestore(&ctx->vpu_spin_lock, flags);
	return 0;
}

static int threadcap(void *data)
{
	struct v4l2_fh *fh = (struct v4l2_fh *) data;
	struct vpu_ctx *ctx = fh_to_vpu(fh);
	uint32_t sizeimage, sequence;
	unsigned long flags;
	struct vb2_v4l2_buffer *v4l2_buf;

	while(1) {
		int ret;
		ret = wait_event_interruptible_timeout(ctx->vpu_cap_waitq,
		kthread_should_stop()||v4l2_m2m_num_dst_bufs_ready(fh->m2m_ctx), msecs_to_jiffies(ctx->thread_cap_interval));

		if (kthread_should_stop() || (ret == -ERESTART))
		{
			for (;;) {
				v4l2_buf = v4l2_m2m_dst_buf_remove(fh->m2m_ctx);
				if (v4l2_buf == NULL)
					return 1;

				spin_lock_irqsave(&ctx->vpu_spin_lock, flags);
				v4l2_m2m_buf_done(v4l2_buf, VB2_BUF_STATE_ERROR);
				spin_unlock_irqrestore(&ctx->vpu_spin_lock, flags);
			}
		}

		if (!v4l2_m2m_num_dst_bufs_ready(fh->m2m_ctx))
			continue;

		ret = 0;
		for(;!ret;)
		{
			uint8_t *p;
			uint32_t len=0;
			uint64_t pts=0;
			bool eos=0;
			bool no_frame=0;

			v4l2_buf = v4l2_m2m_next_dst_buf(fh->m2m_ctx);
			if (!v4l2_buf)
				break;

			mutex_lock(&ctx->vpu_mutex);
			sizeimage = ctx->cap.fmt.pix.sizeimage;
			sequence = ctx->seq_cap;
			mutex_unlock(&ctx->vpu_mutex);

			p = vb2_plane_vaddr(&v4l2_buf->vb2_buf, 0);
			ret = ctx->veng_ops->ve_cap_dqbuf(fh, p, &len, &pts, v4l2_buf->vb2_buf.index);
			if (!ret)
			{
				if (ctx->veng_ops->ve_get_info)
					ctx->veng_ops->ve_get_info(fh, &eos, &no_frame);

				if(vpu_buf_done(fh, v4l2_buf, len, pts, sizeimage, sequence, eos, no_frame))
					break;
				ctx->cap_retry_cnt = 0;
			}
			else
			{
				if (ret == -EAGAIN) {
					ctx->cap_retry_cnt++;

					if(ctx->cap_retry_cnt>1000)
					{
						if(ctx->stop_cmd)
							if(vpu_buf_done(fh, v4l2_buf, 0, 0, 0, 0, 1, 1))
								break;
						ctx->cap_retry_cnt = 0;
						vpu_warn("Capture buf full !\n");
					}
				}
				usleep_range(1000,1000);
				break;
			}
		}
	}

	return 0;
}

static int threadout(void *data)
{
	struct v4l2_fh *fh = (struct v4l2_fh *) data;
	struct vpu_ctx *ctx = fh_to_vpu(fh);
	uint32_t sizeimage, sequence;
	struct vb2_v4l2_buffer *v4l2_buf;
	unsigned long flags;

	while(1) {
		int ret;
		ret = wait_event_interruptible_timeout(ctx->vpu_out_waitq, kthread_should_stop(), msecs_to_jiffies(ctx->thread_out_interval));

		if (kthread_should_stop() || (ret == -ERESTART))
		{
			for (;;) {
				v4l2_buf = v4l2_m2m_src_buf_remove(fh->m2m_ctx);
				if (v4l2_buf == NULL)
					return 1;

				spin_lock_irqsave(&ctx->vpu_spin_lock, flags);
				v4l2_m2m_buf_done(v4l2_buf, VB2_BUF_STATE_ERROR);
				spin_unlock_irqrestore(&ctx->vpu_spin_lock, flags);
			}
		}
		mutex_lock(&ctx->vpu_mutex);
		sizeimage = ctx->out.fmt.pix.sizeimage;
		sequence = ctx->seq_out;
		mutex_unlock(&ctx->vpu_mutex);

		if (v4l2_m2m_num_src_bufs_ready(fh->m2m_ctx))
		{
			uint8_t *p;
			uint32_t offset, len;
			uint64_t pts;
			for (;;)
			{
				v4l2_buf = v4l2_m2m_next_src_buf(fh->m2m_ctx);
				if (!v4l2_buf)
					break;

				p = (ctx->memory_out == V4L2_MEMORY_DMABUF)?vb2_dma_contig_plane_dma_addr(&v4l2_buf->vb2_buf, 0):vb2_plane_vaddr(&v4l2_buf->vb2_buf, 0);
				pts = div_u64(v4l2_buf->vb2_buf.timestamp, 1000);
				len = v4l2_buf->vb2_buf.planes[0].bytesused;
				offset = v4l2_buf->vb2_buf.planes[0].data_offset;

				if (!ctx->veng_ops->ve_out_qbuf(fh, p + offset, len, pts, sequence))
				{
					v4l2_buf = v4l2_m2m_src_buf_remove(fh->m2m_ctx);
					if (!v4l2_buf) {
						vpu_warn("No src buffer to remove\n");
						break;
					}

					v4l2_buf->sequence = sequence++;

					mutex_lock(&ctx->vpu_mutex);
					ctx->seq_out = sequence;
					mutex_unlock(&ctx->vpu_mutex);

					vb2_set_plane_payload(&v4l2_buf->vb2_buf, 0, sizeimage);

					spin_lock_irqsave(&ctx->vpu_spin_lock, flags);
					v4l2_m2m_buf_done(v4l2_buf, VB2_BUF_STATE_DONE);
					spin_unlock_irqrestore(&ctx->vpu_spin_lock, flags);
					continue;
				}

				break;
			}
		}
	}

	return  0;
}

static int vpu_enum_fmt_cap(struct v4l2_fmtdesc *f)
{
	if (f->index < ARRAY_SIZE(cap_fmt)) {
		/* Format found */
		f->pixelformat = cap_fmt[f->index].spec.fmt.pix.pixelformat;
		return 0;
	}
	return -EINVAL;
}

static int vpu_enum_fmt_out(struct v4l2_fmtdesc *f)
{
	if (f->index < ARRAY_SIZE(out_fmt)) {
		/* Format found */
		f->pixelformat = out_fmt[f->index].spec.fmt.pix.pixelformat;
		return 0;
	}
	return -EINVAL;
}

static int vpu_g_fmt(struct v4l2_fh *fh, struct v4l2_format *f)
{
	struct vpu_ctx *ctx = fh_to_vpu(fh);

	mutex_lock(&ctx->vpu_mutex);
	if (V4L2_TYPE_IS_OUTPUT(f->type))
	{
		if (ctx->out.type == -1)
		{
			mutex_unlock(&ctx->vpu_mutex);
			return -EINVAL;
		}
		else
		{
			memcpy(&f->fmt.pix, &ctx->out.fmt.pix, sizeof(struct v4l2_pix_format));
		}
	}
	else
	{
        //if (ctx->cap.type == -1)
		//{
			//move to vpu_alloc_context()
			//memcpy(&ctx->cap, &cap_fmt[0].spec, sizeof(struct v4l2_format));
		//}
		memcpy(&f->fmt.pix, &ctx->cap.fmt.pix, sizeof(struct v4l2_pix_format));
	}
	mutex_unlock(&ctx->vpu_mutex);
	return 0;
}

static int vpu_try_fmt(struct v4l2_format *f, const struct vpu_fmt *fmt)
{
	/* V4L2 specification suggests the driver corrects the format struct
	 * if any of the dimensions is unsupported */
	if (f->fmt.pix.height < fmt->misc.minH)
		f->fmt.pix.height = fmt->misc.minH;
	else if (f->fmt.pix.height > fmt->misc.maxH)
		f->fmt.pix.height = fmt->misc.maxH;

	if (f->fmt.pix.width < fmt->misc.minW)
		f->fmt.pix.width = fmt->misc.minW;
	else if (f->fmt.pix.width > fmt->misc.maxW)
		f->fmt.pix.width = fmt->misc.maxW;

	return 0;
}

static int vpu_try_fmt_cap(struct v4l2_format *f)
{
	int ret;
	const struct vpu_fmt *fmt;

	fmt = &cap_fmt[0];

	f->fmt.pix.colorspace = fmt->spec.fmt.pix.colorspace;
	f->fmt.pix.xfer_func = fmt->spec.fmt.pix.xfer_func;
	f->fmt.pix.ycbcr_enc = fmt->spec.fmt.pix.ycbcr_enc;
	f->fmt.pix.quantization = fmt->spec.fmt.pix.quantization;
	f->fmt.pix.bytesperline = fmt->spec.fmt.pix.bytesperline;
	f->fmt.pix.sizeimage = fmt->spec.fmt.pix.sizeimage;
	f->fmt.pix.field = fmt->spec.fmt.pix.field;
	ret = vpu_try_fmt(f, fmt);

	return 0;
}

static const struct vpu_fmt *find_src_format(struct v4l2_format *f)
{
	const struct vpu_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(out_fmt); k++) {
		fmt = &out_fmt[k];
		if (fmt->spec.fmt.pix.pixelformat == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(out_fmt))
	{
		vpu_err("unknow source format %c%c%c%c\n",
		(f->fmt.pix.pixelformat>>0)&0xff, (f->fmt.pix.pixelformat>>8)&0xff,
		(f->fmt.pix.pixelformat>>16)&0xff, (f->fmt.pix.pixelformat>>24)&0xff);
		return NULL;
	}

	return &out_fmt[k];
}

static int vpu_try_fmt_out(struct v4l2_format *f)
{
	const struct vpu_fmt *fmt;
	int ret;

	fmt = find_src_format(f);
	if (!fmt) {
		// FIXME: set f->fmt.pix.pixelformat?
		fmt = &out_fmt[0];
	}

	ret = vpu_try_fmt(f, fmt);

	return ret;
}

static int vpu_s_fmt_cap(struct v4l2_fh *fh, struct v4l2_format *f)
{
	struct vpu_ctx *ctx = fh_to_vpu(fh);
#if 0
	int ret;

	ret = vpu_try_fmt_cap(f);
	if (ret)
		return ret;
#endif
	mutex_lock(&ctx->vpu_mutex);
	memcpy(&ctx->cap, f, sizeof(struct v4l2_format));
	mutex_unlock(&ctx->vpu_mutex);
	return 0;
}

static int vpu_s_fmt_out(struct v4l2_fh *fh, struct v4l2_format *f)
{
	struct vpu_ctx *ctx = fh_to_vpu(fh);
#if 0
	int ret;

	ret = vpu_try_fmt_out(f);
	if (ret)
		return ret;
#endif
	mutex_lock(&ctx->vpu_mutex);
	memcpy(&ctx->out, f, sizeof(struct v4l2_format));
	if (ctx->out.fmt.pix.width > ctx->rect.width)
		ctx->rect.width = ctx->out.fmt.pix.width;

	if (ctx->out.fmt.pix.height > ctx->rect.height)
		ctx->rect.height = ctx->out.fmt.pix.height;
	mutex_unlock(&ctx->vpu_mutex);

	return 0;
}

static int vpu_queue_info(struct vb2_queue *vq, int *bufcnt, int *sizeimage)
{
	struct vpu_ctx *ctx = vq_to_vpu(vq);
	const struct vpu_fmt * fmt;
	int type = vq->type;

	mutex_lock(&ctx->vpu_mutex);
	if (V4L2_TYPE_IS_OUTPUT(type))
	{
		if (ctx->out.type == -1)
		{
			// move to vpu_alloc_context()
			//memcpy(&ctx->out, &out_fmt[0], sizeof(struct vpu_fmt));
			mutex_unlock(&ctx->vpu_mutex);
			return -EPERM;
		}

		fmt = find_src_format(&ctx->out);
	}
	else
	{
		if (ctx->cap.type == -1)
		{
			mutex_unlock(&ctx->vpu_mutex);
			return -EPERM;
		}

		fmt = &cap_fmt[0];
	}
	if (bufcnt)
		*bufcnt = fmt->misc.bufcnt;

	if (sizeimage)
	{
		if(vq->memory == V4L2_MEMORY_DMABUF)
			*sizeimage = 4096;
		else
			*sizeimage = fmt->spec.fmt.pix.sizeimage;
	}

	mutex_unlock(&ctx->vpu_mutex);
	return 0;
}

int vpu_start_streaming(struct vb2_queue *q, uint32_t count)
{
	struct videc_ctx *vid_ctx = vb2_get_drv_priv(q);
	struct vpu_ctx *ctx = vid_ctx->vpu_ctx;
	struct v4l2_fh *fh = &vid_ctx->fh;
	int ret;
	int pixelformat = ctx->out.fmt.pix.pixelformat;
	const struct vpu_fmt * fmt;

	fmt = find_src_format(&ctx->out);
	ctx->bufcnt_out = fmt->misc.bufcnt;
	fmt = &cap_fmt[0];
	ctx->bufcnt_cap = fmt->misc.bufcnt;


	mutex_lock(&ctx->vpu_mutex);
	switch (pixelformat)
	{
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_MPEG2:
	case V4L2_PIX_FMT_MPEG4:
		/* Set VE1 ops */
		if (!ctx->ve1_ops) {
			mutex_unlock(&ctx->vpu_mutex);
			return -EINVAL;
		}
		ctx->veng_ops = ctx->ve1_ops;
		break;
	case V4L2_PIX_FMT_HEVC:
	case V4L2_PIX_FMT_VP9:
	case V4L2_PIX_FMT_AV1:
		/* Set VE2 ops */
		if (!ctx->ve2_ops) {
			mutex_unlock(&ctx->vpu_mutex);
			return -EINVAL;
		}
		ctx->veng_ops = ctx->ve2_ops;
		break;
	default:
		vpu_err("Unsupported format=%c%c%c%c\n",
			(pixelformat & 0xff), (pixelformat >> 8) & 0xff,
			(pixelformat >> 16) & 0xff, (pixelformat >> 24) & 0xff);
		mutex_unlock(&ctx->vpu_mutex);
		return -EINVAL;
	}

	/* Allocate video engine context */
	if (!vid_ctx->ve_ctx) {
		vid_ctx->ve_ctx = ctx->veng_ops->ve_alloc_context(vid_ctx->file);
		if (!vid_ctx->ve_ctx) {
			mutex_unlock(&ctx->vpu_mutex);
			return -EINVAL;
		}
	}

	ret = ctx->veng_ops->ve_start_streaming(q, count, pixelformat);
	if (ret)
	{
		vpu_err("Failed to start streaming %d\n", ret);
		mutex_unlock(&ctx->vpu_mutex);
		return ret;
	}

	if (V4L2_TYPE_IS_OUTPUT(q->type))
	{
		ctx->seq_out = 0;
		ctx->memory_out = q->memory;
		ctx->thread_out = kthread_run(threadout, fh, "outhread %c%c%c%c",
		(0xff&pixelformat>>0), (0xff&pixelformat>>8), (0xff&pixelformat>>16), (0xff&pixelformat>>24));
		ctx->is_out_started = 1;
	}
	else
	{
		ctx->seq_cap = 0;
		ctx->memory_cap = q->memory;
		ctx->thread_cap = kthread_run(threadcap, fh, "caphread %c%c%c%c",
		(0xff&pixelformat>>0), (0xff&pixelformat>>8), (0xff&pixelformat>>16), (0xff&pixelformat>>24));
		ctx->is_cap_started = 1;
	}
	mutex_unlock(&ctx->vpu_mutex);

	return ret;
}

int vpu_stop_streaming(struct vb2_queue *q)
{
	struct videc_ctx *vid_ctx = vb2_get_drv_priv(q);
	struct vpu_ctx *ctx = vid_ctx->vpu_ctx;
	int ret = 0;;

	if (!ctx->veng_ops)
		return -EINVAL;

	if (V4L2_TYPE_IS_OUTPUT(q->type))
	{
		wake_up_interruptible(&ctx->vpu_out_waitq);
		if (ctx->thread_out)
			kthread_stop(ctx->thread_out);
	}
	else
	{
		wake_up_interruptible(&ctx->vpu_cap_waitq);
		if (ctx->thread_cap)
			kthread_stop(ctx->thread_cap);
	}

	mutex_lock(&ctx->vpu_mutex);
	ret = ctx->veng_ops->ve_stop_streaming(q);
	if (ret)
	{
		vpu_err("fail to stop streaming(ve2_stop_streaming %d)\n", ret);
	}

	if (V4L2_TYPE_IS_OUTPUT(q->type))
		ctx->is_out_started = 0;
	else
		ctx->is_cap_started = 0;
	if (!ctx->is_cap_started && !ctx->is_out_started) {
		ctx->veng_ops->ve_free_context(vid_ctx->ve_ctx);
		ctx->veng_ops = NULL;
	}

	mutex_unlock(&ctx->vpu_mutex);
	return ret;
}

static int vpu_qbuf(struct v4l2_fh *fh, struct vb2_buffer *vb)
{
	struct vpu_ctx *ctx = vq_to_vpu(vb->vb2_queue);

	if (V4L2_TYPE_IS_OUTPUT(vb->type))
	{
		wake_up_interruptible(&ctx->vpu_out_waitq);
		return 0;
	}
	else
	{
		int ret;

		if (!ctx->veng_ops)
			return -EINVAL;

		ret = ctx->veng_ops->ve_cap_qbuf(fh, vb);
		if (!ret)
		{
			wake_up_interruptible(&ctx->vpu_cap_waitq);
		}
		return ret;
	}
	return -EINVAL;
}

#if 0 //Keep for libmali verify, remove me while libmali finish metadata conversion
static void vpu_buf_finish(struct vb2_buffer *vb)
{
	struct vpu_ctx *ctx = vq_to_vpu(vb->vb2_queue);

	if (!ctx->veng_ops)
		return;

	if (ctx->veng_ops->ve_buf_finish)
	{
		ctx->veng_ops->ve_buf_finish(vb);
	}
}
#endif
static int vpu_abort(void *priv, int type)
{
	struct videc_ctx *vid_ctx = priv;
	struct vpu_ctx *ctx = vid_ctx->vpu_ctx;
	int ret;

	if (!ctx->veng_ops)
		return -EINVAL;

	ret = ctx->veng_ops->ve_abort(vid_ctx->ve_ctx, type);
	if (ret)
	{
		vpu_err("fail to abort streaming(ve2_abort %d)\n", ret);
	}

	return 0;
}

static int vpu_g_crop(void *fh, struct v4l2_rect *rect)
{
	/* G_SELECTION */
	struct vpu_ctx *ctx = fh_to_vpu(fh);

	mutex_lock(&ctx->vpu_mutex);
	memcpy(rect, &ctx->rect, sizeof(ctx->rect));
	mutex_unlock(&ctx->vpu_mutex);
	return 0;
}

static int vpu_stop_cmd(void *fh)
{
	/* G_SELECTION */
	struct vpu_ctx *ctx = fh_to_vpu(fh);

	if (!ctx->veng_ops)
		return -EINVAL;

	if (ctx->veng_ops->ve_stop_cmd)
	{
		ctx->stop_cmd = true;
		ctx->veng_ops->ve_stop_cmd(fh, ctx->out.fmt.pix.pixelformat);
	}
	return 0;
}

int vpu_get_cap_fmt(void *fh, void *cap_fmt)
{
	struct vpu_ctx *ctx = fh_to_vpu(fh);

	if (ctx && cap_fmt)
	{
		mutex_lock(&ctx->vpu_mutex);
		memcpy(cap_fmt, &ctx->cap.fmt.pix, sizeof(struct v4l2_pix_format));
		mutex_unlock(&ctx->vpu_mutex);
	}
	return 0;
}
EXPORT_SYMBOL(vpu_get_cap_fmt);

int vpu_update_cap_fmt(void *fh, void *cap_fmt)
{
	struct vpu_ctx *ctx = fh_to_vpu(fh);

	if (ctx && cap_fmt)
	{
		mutex_lock(&ctx->vpu_mutex);
		memcpy(&ctx->cap.fmt.pix, cap_fmt, sizeof(struct v4l2_pix_format));
		ctx->rect.width = ctx->cap.fmt.pix.width;
		ctx->rect.height = ctx->cap.fmt.pix.height;
		mutex_unlock(&ctx->vpu_mutex);
	}
	return 0;
}
EXPORT_SYMBOL(vpu_update_cap_fmt);

void vpu_notify_event_resolution_change(void *fh)
{
    static const struct v4l2_event event_source_change = {
        .type = V4L2_EVENT_SOURCE_CHANGE,
        .u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION
    };

    v4l2_event_queue_fh(fh, &event_source_change);
}
EXPORT_SYMBOL(vpu_notify_event_resolution_change);

void vpu_cap_buf_done(struct v4l2_fh *fh, struct vb2_v4l2_buffer *buf,
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

const static struct vpu_fmt_ops ops = {
	.vpu_enum_fmt_cap = vpu_enum_fmt_cap,
	.vpu_enum_fmt_out = vpu_enum_fmt_out,
	.vpu_g_fmt = vpu_g_fmt,
	.vpu_try_fmt_cap = vpu_try_fmt_cap,
	.vpu_try_fmt_out = vpu_try_fmt_out,
	.vpu_s_fmt_cap = vpu_s_fmt_cap,
	.vpu_s_fmt_out = vpu_s_fmt_out,
	.vpu_queue_info = vpu_queue_info,
	.vpu_start_streaming = vpu_start_streaming,
	.vpu_stop_streaming = vpu_stop_streaming,
	.vpu_qbuf = vpu_qbuf,
#if 0 //Keep for libmali verify, remove me while libmali finish metadata conversion
	.vpu_buf_finish = vpu_buf_finish,
#endif
	.vpu_abort = vpu_abort,
	.vpu_g_crop = vpu_g_crop,
	.vpu_stop_cmd = vpu_stop_cmd,
};

const struct vpu_fmt_ops *get_vpu_fmt_ops(void)
{
	return &ops;
}

void *vpu_alloc_context(void)
{
	struct vpu_ctx *ctx = NULL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		vpu_err("Failed to allocate vpu\n");
		return ctx;
	}

	ctx->out.type = -1;
	ctx->cap.type = -1;
	ctx->thread_out = NULL;
	ctx->thread_cap = NULL;
	ctx->seq_out = 1;
	ctx->seq_cap = 1;
	ctx->thread_out_interval = 20;
	ctx->thread_cap_interval = 10;
	ctx->rect.left = 0;
	ctx->rect.top = 0;
	ctx->rect.width = 1920;
	ctx->rect.height = 1080;
	ctx->veng_ops = NULL;
	ctx->ve1_ops = NULL;
	ctx->ve2_ops = NULL;
	ctx->is_cap_started = 0;
	ctx->is_out_started = 0;
	ctx->ve1_ops = vpu_ve1_ops;
	ctx->ve2_ops = vpu_ve2_ops;
	ctx->cap_retry_cnt = 0;
	ctx->stop_cmd = false;

	mutex_init(&ctx->vpu_mutex);
	spin_lock_init(&ctx->vpu_spin_lock);
	init_waitqueue_head(&ctx->vpu_out_waitq);
	init_waitqueue_head(&ctx->vpu_cap_waitq);

	// init ctx->out with default value
	memcpy(&ctx->out, &out_fmt[0], sizeof(struct vpu_fmt));
	// init ctx->cap with default value
	memcpy(&ctx->cap, &cap_fmt[0].spec, sizeof(struct v4l2_format));

	return ctx;
}

void vpu_free_context(void *ctx)
{
	kfree(ctx);
}
/*
 * Register video engine operations.
 */
int vpu_ve_register(int id, struct veng_ops *ops)
{
	if (id == 1)
		vpu_ve1_ops = ops;
	else if (id == 2)
		vpu_ve2_ops = ops;
	else {
		vpu_err("Register invalid video engine VE%d ops\n", id);
		return -EINVAL;;
	}

	vpu_info("Registered video engine VE%d ops\n", id);

	return 0;
}
EXPORT_SYMBOL(vpu_ve_register);

/*
 * Unregister video engine operations.
 */
void vpu_ve_unregister(int id)
{
	if (id == 1)
		vpu_ve1_ops = NULL;
	else if (id == 2)
		vpu_ve2_ops = NULL;

	vpu_info("Unregistered video engine VE%d ops\n", id);
}
EXPORT_SYMBOL(vpu_ve_unregister);
