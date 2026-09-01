// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_dprx.h"
#include <trace/events/rtk_dprx_trace.h>

#define RTK_DPRX_VIDEO_NAME    "rtk_dprx"

#define DPRX_VIDEO_NR  60

#define DPRX_MAX_WIDTH   3840
#define DPRX_MAX_HEIGHT  2160
#define DPRX_MIN_WIDTH	320
#define DPRX_MIN_HEIGHT	240

static const struct rtk_dprx_fmt rtk_dprx_fmt_list[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV12,
	},
};

#define NUM_FORMATS ARRAY_SIZE(rtk_dprx_fmt_list)

static const struct v4l2_dv_timings_cap rtk_dprx_timings_cap = {
	.type = V4L2_DV_BT_656_1120,
	.bt = {
		.min_width = DPRX_MIN_WIDTH,
		.max_width = DPRX_MAX_WIDTH,
		.min_height = DPRX_MIN_HEIGHT,
		.max_height = DPRX_MAX_HEIGHT,
		.min_pixelclock = 7372800, /* 640 x 480 x 24Hz */
		.max_pixelclock = 497664000, /* 3840 x 2160 x 60Hz */
		.standards = V4L2_DV_BT_STD_CEA861,
		.capabilities = V4L2_DV_BT_CAP_PROGRESSIVE,
	},
};

static void rtk_dprx_dma_pool(struct rtk_dprx *dprx)
{
	if (dprx == NULL)
		return;

	if (dprx->dev == NULL)
		return;

	dma_coerce_mask_and_coherent(dprx->dev, DMA_BIT_MASK(32));
	set_dma_ops(dprx->dev, &rheap_dma_ops);

	rheap_setup_dma_pools(dprx->dev, "rtk_media_heap",
		RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
		RTK_FLAG_CMA | RTK_FLAG_HWIPACC, __func__);
}

static const struct rtk_dprx_fmt *rtk_dprx_find_format(struct v4l2_format *f)
{
	const struct rtk_dprx_fmt *fmt;
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++) {
		fmt = &rtk_dprx_fmt_list[i];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (i == NUM_FORMATS)
		return NULL;

	return &rtk_dprx_fmt_list[i];
}

static int dprx_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers, unsigned int *num_planes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct rtk_dprx *dprx = vb2_get_drv_priv(q);

	trace_dprx_func_event(__func__);

	*num_planes = 1;

	sizes[0] = roundup(dprx->pix_fmt.sizeimage, 4096);

	dev_dbg(dprx->dev, "%s, size=%u\n", __func__, sizes[0]);

	return 0;
}

static int dprx_buf_prepare(struct vb2_buffer *vb)
{
	struct rtk_dprx *dprx = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long size;

	trace_dprx_func_event(__func__);

	size = vb2_plane_size(vb, 0);

	if (size < dprx->pix_fmt.sizeimage) {
		dev_err(dprx->dev, "%s index=%u, vb2_plane_size(%lu) < sizeimage(%u)\n",
			__func__, vb->index, size, dprx->pix_fmt.sizeimage);
		return -EINVAL;
	}

	dev_dbg(dprx->dev, "%s index=%u, set bytesused=%u for plane\n",
		__func__, vb->index, dprx->pix_fmt.sizeimage);

	vb2_set_plane_payload(vb, 0, dprx->pix_fmt.sizeimage);

	return 0;
}

static void dprx_buf_finish(struct vb2_buffer *vb)
{
	trace_dprx_func_event(__func__);
}

static int dprx_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct rtk_dprx *dprx = vb2_get_drv_priv(q);
	u8 entry_index;

	trace_dprx_func_event(__func__);

	dev_info(dprx->dev, "%s, detect_done=%s\n",
		__func__, dprx->detect_done ? "Y":"N");

	if (!dprx->detect_done)
		return -EAGAIN;

	mutex_lock(&dprx->buffer_lock);
	for (entry_index = DMA_ENTRY_0; entry_index <= DMA_ENTRY_3; entry_index++) {
		struct rtk_dprx_buffer *rbuf;

		rbuf = list_first_entry_or_null(&dprx->buffers, struct rtk_dprx_buffer, link);
		if (!rbuf) {
			mutex_unlock(&dprx->buffer_lock);
			dev_info(dprx->dev, "No buffer for streaming\n");
			return -ENOMEM;
		}
		rbuf->entry_index = entry_index;
		rbuf->phy_addr = vb2_dma_contig_plane_dma_addr(&rbuf->vb.vb2_buf, 0);
		dprx->cur_buf[entry_index] = rbuf;
		dprx->wrap_ops->dma_buf_cfg(dprx, entry_index, rbuf->phy_addr);
		list_del(&rbuf->link);
	}
	mutex_unlock(&dprx->buffer_lock);

	dprx->streaming_start_jiffies = jiffies;
	dprx->mismatch_err_count = 0;
	dprx->wrap_ops->interrupt_ctrl(dprx, ENABLE);
	dprx->wrap_ops->dma_go_ctrl(dprx, ENABLE);

	return 0;
}

static void dprx_stop_streaming(struct vb2_queue *q)
{
	struct rtk_dprx *dprx = vb2_get_drv_priv(q);
	int num_buffers = vb2_get_num_buffers(q);
	int i;

	trace_dprx_func_event(__func__);

	dev_info(dprx->dev, "%s, detect_done=%s\n",
		__func__, dprx->detect_done ? "Y":"N");

	/* Stop wrap */
	dprx->wrap_ops->dma_go_ctrl(dprx, DISABLE);
	dprx->wrap_ops->interrupt_ctrl(dprx, DISABLE);

	mutex_lock(&dprx->buffer_lock);

	for (i = 0; i < num_buffers; ++i) {
		dev_dbg(dprx->dev, "q->bufs[%d]->state=%u\n", i, q->bufs[i]->state);

		if (q->bufs[i]->state == VB2_BUF_STATE_ACTIVE) {
			dev_dbg(dprx->dev, "Set q->bufs[%d] done\n", i);
			vb2_buffer_done(q->bufs[i], VB2_BUF_STATE_ERROR);
		}
	}

	INIT_LIST_HEAD(&dprx->buffers);
	dprx->sequence = 0;

	mutex_unlock(&dprx->buffer_lock);

}

static void dprx_buf_queue(struct vb2_buffer *vb)
{
	struct rtk_dprx *dprx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rtk_dprx_buffer *rbuf = to_dprx_buffer(vbuf);

	dev_dbg(dprx->dev, "%s index=%u\n", __func__, vb->index);

	trace_dprx_buf_queue(vb->index);

	mutex_lock(&dprx->buffer_lock);
	list_add_tail(&rbuf->link, &dprx->buffers);
	mutex_unlock(&dprx->buffer_lock);
}

static const struct vb2_ops rtk_dprx_vb2_ops = {
	.queue_setup = dprx_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_prepare = dprx_buf_prepare,
	.buf_finish = dprx_buf_finish,
	.start_streaming = dprx_start_streaming,
	.stop_streaming = dprx_stop_streaming,
	.buf_queue =  dprx_buf_queue,
};

static int rtk_dprx_video_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{
	trace_dprx_func_event(__func__);

	strscpy(cap->driver, RTK_DPRX_VIDEO_NAME, sizeof(cap->driver));
	strscpy(cap->card, RTK_DPRX_VIDEO_NAME, sizeof(cap->card));

	return 0;
}

static int rtk_dprx_video_enum_format(struct file *file, void *fh,
				struct v4l2_fmtdesc *f)
{
	struct rtk_dprx *dprx = video_drvdata(file);
	const struct rtk_dprx_fmt *fmt;

	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	trace_dprx_func_event(__func__);

	dev_dbg(dprx->dev, "%s index(%u)\n", __func__, f->index);

	fmt = &rtk_dprx_fmt_list[f->index];

	f->pixelformat = fmt->fourcc;

	return 0;
}

static int rtk_dprx_video_get_format(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct rtk_dprx *dprx = video_drvdata(file);
	u32 size;

	trace_dprx_func_event(__func__);

	dprx->pix_fmt.field = V4L2_FIELD_NONE;
	dprx->pix_fmt.colorspace = V4L2_COLORSPACE_REC709;

	dprx->pix_fmt.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	// TODO: pix_fmt.width/height

	size = roundup(dprx->pix_fmt.width, 16) * dprx->pix_fmt.height;
	dprx->pix_fmt.sizeimage = size + (size >> 1);

	f->fmt.pix = dprx->pix_fmt;

	dev_dbg(dprx->dev, "%s %ux%u sizeimage=%u\n", __func__,
		f->fmt.pix.width, f->fmt.pix.height, f->fmt.pix.sizeimage);

	return 0;
}

static int rtk_dprx_video_try_format(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct rtk_dprx *dprx = video_drvdata(file);
	const struct rtk_dprx_fmt *fmt;

	dev_dbg(dprx->dev, "%s width=%u height=%u pixelformat=%c%c%c%c\n",
		__func__, f->fmt.pix.width, f->fmt.pix.height,
		f->fmt.pix.pixelformat & 0xFF,
		(f->fmt.pix.pixelformat >> 8) & 0xFF,
		(f->fmt.pix.pixelformat >> 16) & 0xFF,
		(f->fmt.pix.pixelformat >> 24) & 0xFF);

	fmt = rtk_dprx_find_format(f);
	if (!fmt)
		f->fmt.pix.pixelformat = rtk_dprx_fmt_list[0].fourcc;

	f->fmt.pix.field = V4L2_FIELD_NONE;
	f->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;

	f->fmt.pix.quantization = V4L2_QUANTIZATION_LIM_RANGE;

	return 0;
}

static int rtk_dprx_video_set_format(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct rtk_dprx *dprx = video_drvdata(file);
	int ret;

	trace_dprx_func_event(__func__);

	ret = rtk_dprx_video_try_format(file, fh, f);
	if (ret)
		return ret;

	if (vb2_is_busy(&dprx->queue)) {
		dev_err(dprx->dev, "%s device busy\n", __func__);
		return -EBUSY;
	}

	/* Check signal and set wrap fmt */
	if (!dprx->detect_done)
		return -EAGAIN;

	dprx->pix_fmt.width = f->fmt.pix.width;
	dprx->pix_fmt.height = f->fmt.pix.height;
	dprx->pix_fmt.pixelformat = f->fmt.pix.pixelformat;

	dprx->dst_width = f->fmt.pix.width;
	dprx->dst_height = f->fmt.pix.height;

	dprx->pix_fmt.sizeimage = dprx->wrap_ops->calculate_video_size(dprx->dst_width,
			dprx->dst_height, dprx->compenc_mode);

	dprx->wrap_ops->video_size_cfg(dprx);
	dprx->wrap_ops->meta_swap(dprx, ENABLE);
	dprx->wrap_ops->crc_ctrl(dprx, ENABLE);

	return 0;
}

static int rtk_dprx_video_enum_framesizes(struct file *file, void *fh,
					struct v4l2_frmsizeenum *fsize)
{
	struct rtk_dprx *dprx = video_drvdata(file);

	if (fsize->index != 0)
		return -EINVAL;

	if (fsize->pixel_format != V4L2_PIX_FMT_NV12)
		return -EINVAL;

	if (!dprx->detect_done)
		return -EAGAIN;

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.min_width = DPRX_MIN_WIDTH;
	fsize->stepwise.max_width = dprx->src_width;
	fsize->stepwise.step_width = 8;
	fsize->stepwise.min_height = DPRX_MIN_HEIGHT;
	fsize->stepwise.max_height = dprx->src_height;
	fsize->stepwise.step_height = 8;

	dev_dbg(dprx->dev, "frmsizeenum min %ux%u max %ux%u, step w=%u h=%u\n",
		fsize->stepwise.min_width, fsize->stepwise.min_height,
		fsize->stepwise.max_width, fsize->stepwise.max_height,
		fsize->stepwise.step_width, fsize->stepwise.step_height);

	return 0;
}


static int rtk_dprx_video_enum_frameintervals(struct file *file, void *fh,
					struct v4l2_frmivalenum *fival)
{
	struct rtk_dprx *dprx = video_drvdata(file);

	if (fival->index > 0)
		return -EINVAL;

	if (fival->width < DPRX_MIN_WIDTH || fival->width > DPRX_MAX_WIDTH ||
	    fival->height < DPRX_MIN_HEIGHT || fival->height > DPRX_MAX_HEIGHT)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;

	if (!dprx->detect_done)
		return -EAGAIN;

	/* fps = denominator / numerator, src_vfreq is in 0.1Hz */
	fival->discrete.denominator = dprx->src_vfreq;
	fival->discrete.numerator = 10;

	dev_dbg(dprx->dev, "frmivalenum %ufps\n",
		fival->discrete.denominator / fival->discrete.numerator);

	return 0;
}

static int rtk_dprx_video_set_dv_timings(struct file *file, void *fh,
				     struct v4l2_dv_timings *timings)
{
	/* DPRX timings are determined by the source, setting is not supported */
	return -ENOTTY;
}

static int rtk_dprx_video_get_dv_timings(struct file *file, void *fh,
				     struct v4l2_dv_timings *timings)
{
	struct rtk_dprx *dprx = video_drvdata(file);

	trace_dprx_func_event(__func__);

	if (!dprx->detect_done)
		return -ENODATA;

	timings->type = V4L2_DV_BT_656_1120;
	timings->bt = dprx->detected_timings;

	return 0;
}

static int rtk_dprx_video_query_dv_timings(struct file *file, void *fh,
				       struct v4l2_dv_timings *timings)
{
	struct rtk_dprx *dprx = video_drvdata(file);

	trace_dprx_func_event(__func__);

	if (!dprx->detect_done)
		return -ENOLINK;

	timings->type = V4L2_DV_BT_656_1120;
	timings->bt = dprx->detected_timings;

	return dprx->v4l2_input_status ? -ENOLINK : 0;
}

static int rtk_dprx_video_enum_dv_timings(struct file *file, void *fh,
				      struct v4l2_enum_dv_timings *timings)
{
	trace_dprx_func_event(__func__);

	return v4l2_enum_dv_timings_cap(timings, &rtk_dprx_timings_cap,
					NULL, NULL);
}

static int rtk_dprx_video_dv_timings_cap(struct file *file, void *fh,
				     struct v4l2_dv_timings_cap *cap)
{
	trace_dprx_func_event(__func__);

	*cap = rtk_dprx_timings_cap;

	return 0;
}

static int rtk_dprx_vb2_ioctl_dqbuf(struct file *file, void *priv,
		struct v4l2_buffer *p)
{
	int ret;
	unsigned long time_start;

	time_start = jiffies;

	ret = vb2_ioctl_dqbuf(file, priv, p);

	trace_dprx_buf_dqueue(ret, jiffies_to_msecs(jiffies - time_start));

	return ret;
}

static const struct v4l2_ioctl_ops rtk_dprx_video_ioctls = {
	/* VIDIOC_QUERYCAP handler */
	.vidioc_querycap = rtk_dprx_video_querycap,
	/* VIDIOC_ENUM_FMT handlers */
	.vidioc_enum_fmt_vid_cap = rtk_dprx_video_enum_format,
	/* VIDIOC_G_FMT handlers */
	.vidioc_g_fmt_vid_cap = rtk_dprx_video_get_format,
	/* VIDIOC_S_FMT handlers */
	.vidioc_s_fmt_vid_cap = rtk_dprx_video_set_format,
	/* VIDIOC_TRY_FMT handlers */
	.vidioc_try_fmt_vid_cap = rtk_dprx_video_try_format,

	/* Buffer handlers */
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = rtk_dprx_vb2_ioctl_dqbuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,

	/* Stream on/off */
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,

	.vidioc_enum_framesizes = rtk_dprx_video_enum_framesizes,
	.vidioc_enum_frameintervals = rtk_dprx_video_enum_frameintervals,

	/* DV Timings IOCTLs */
	.vidioc_s_dv_timings = rtk_dprx_video_set_dv_timings,
	.vidioc_g_dv_timings = rtk_dprx_video_get_dv_timings,
	.vidioc_query_dv_timings = rtk_dprx_video_query_dv_timings,
	.vidioc_enum_dv_timings = rtk_dprx_video_enum_dv_timings,
	.vidioc_dv_timings_cap = rtk_dprx_video_dv_timings_cap,
};

static const struct v4l2_file_operations rtk_dprx_v4l2_fops = {
	.owner = THIS_MODULE,
	.read = vb2_fop_read,
	.poll = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
};

static int rtk_dprx_setup_video(struct rtk_dprx *dprx)
{
	struct v4l2_device *v4l2_dev = &dprx->v4l2_dev;
	struct video_device *vdev = &dprx->vdev;
	struct vb2_queue *vbq = &dprx->queue;
	int ret;

	dprx->pix_fmt.pixelformat = V4L2_PIX_FMT_NV12;
	dprx->pix_fmt.field = V4L2_FIELD_NONE;
	dprx->pix_fmt.colorspace = V4L2_COLORSPACE_REC709;
	dprx->pix_fmt.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	dprx->v4l2_input_status = V4L2_IN_ST_NO_SIGNAL;

	ret = v4l2_device_register(dprx->dev, v4l2_dev);
	if (ret) {
		dev_err(dprx->dev, "Failed to register v4l2 device\n");
		return ret;
	}

	vbq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vbq->io_modes = VB2_MMAP | VB2_DMABUF;
	vbq->dev = v4l2_dev->dev;
	vbq->lock = &dprx->video_lock;
	vbq->ops = &rtk_dprx_vb2_ops;
	vbq->mem_ops = &vb2_dma_contig_memops;
	vbq->drv_priv = dprx;
	vbq->buf_struct_size = sizeof(struct rtk_dprx_buffer);
	vbq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	vbq->min_queued_buffers = 6;

	ret = vb2_queue_init(vbq);
	if (ret) {
		dev_err(dprx->dev, "Failed to init vb2 queue\n");
		return ret;
	}
	vdev->queue = vbq;
	vdev->fops = &rtk_dprx_v4l2_fops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	vdev->v4l2_dev = v4l2_dev;
	strscpy(vdev->name, RTK_DPRX_VIDEO_NAME, sizeof(vdev->name));
	vdev->vfl_type = VFL_TYPE_VIDEO;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->release = video_device_release_empty;
	vdev->ioctl_ops = &rtk_dprx_video_ioctls;
	vdev->lock = &dprx->video_lock;

	video_set_drvdata(vdev, dprx);
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, DPRX_VIDEO_NR);
	if (ret) {
		dev_err(dprx->dev, "Failed to register video device\n");
		return ret;
	}

	dev_info(dprx->dev, "Registered %s as /dev/video%d\n",
		vdev->name, vdev->num);

	return 0;
}

/**
 * rtk_dprx_irq_thread - Threaded IRQ handler for DPRX
 * @irq: IRQ number
 * @dev_id: Device context (rtk_dprx pointer)
 *
 * Runs in process context, can perform blocking operations.
 * Called when hardirq handler returns IRQ_WAKE_THREAD.
 * This handler processes Link Training DPCD IRQs which may require
 * calling phy_configure() that uses mutex.
 *
 * Return: IRQ_HANDLED
 */
static irqreturn_t rtk_dprx_irq_thread(int irq, void *dev_id)
{
	struct rtk_dprx *dprx = dev_id;
	int pending;

	if (!dprx)
		return IRQ_NONE;

	/* Get and clear pending IRQ status atomically */
	pending = atomic_xchg(&dprx->lt_dpcd_irq_pending, 0);

	if (pending) {
		/* Trace: threaded handler entry - measures scheduling latency */
		trace_dprx_lt_thread_enter((u8)pending);

		/* Process Link Training DPCD IRQ in threaded context */
		/* This can call phy_configure() which uses mutex */
		rtk_dprx_lt_dpcd_irq_handler(dprx, (u8)pending);
	}

	return IRQ_HANDLED;
}

static irqreturn_t rtk_dprx_irq_handler(int irq, void *dev_id)
{
	struct rtk_dprx *dprx = dev_id;
	int ret;
	u32 done_st = 0;
	u8 is_done;
	u8 entry_index;
	u8 aux_dpcd_irq;
	bool need_thread = false;

	if (dprx == NULL)
		return IRQ_NONE;

	/* Handle Link Training DPCD IRQ */
	if (dprx->rbus_ops) {
		aux_dpcd_irq = dprx->rbus_ops->get_byte_extint(PB7_DD_AUX_DPCD_IRQ);

		if (aux_dpcd_irq & (_BIT7 | _BIT6 | _BIT5)) {
			/* Trace: DPCD IRQ in hardirq context */
			trace_dprx_lt_dpcd_irq(aux_dpcd_irq, true);

			/* Accumulate IRQ status bits atomically */
			atomic_or(aux_dpcd_irq, &dprx->lt_dpcd_irq_pending);

			/* Clear IRQ flags immediately in hardirq */
			dprx->rbus_ops->set_byte_extint(PB7_DD_AUX_DPCD_IRQ,
				aux_dpcd_irq & (_BIT7 | _BIT6 | _BIT5));

			/* Wake threaded handler for blocking operations */
			need_thread = true;
		}
	}

	if (dprx->wrap_ops == NULL)
		return need_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;

	ret = dprx->wrap_ops->get_intr_state(dprx, &done_st);
	if (ret) {
		if (++dprx->mismatch_err_count > 5) {
			dev_err(dprx->dev, "Recovery: mismatch count=%u, stopping streaming\n",
				dprx->mismatch_err_count);
			dprx->detect_done = false;
			dprx->wrap_ops->dma_go_ctrl(dprx, DISABLE);
			dprx->wrap_ops->interrupt_ctrl(dprx, DISABLE);
			rtk_dprx_lt_scan_work_start(dprx);
			return need_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
		}
	}

	if (!done_st)
		return need_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;

	for (entry_index = DMA_ENTRY_0; entry_index <= DMA_ENTRY_3; entry_index++) {
		struct rtk_dprx_buffer *rbuf;

		is_done = dprx->wrap_ops->is_frame_done(done_st, entry_index);

		if (!is_done)
			continue;

		dprx->wrap_ops->clear_done_flag(dprx, entry_index);
		rbuf = list_first_entry_or_null(&dprx->buffers, struct rtk_dprx_buffer, link);
		if (rbuf) {
			struct vb2_v4l2_buffer *vb;

			vb = &dprx->cur_buf[entry_index]->vb;
			vb->vb2_buf.timestamp = ktime_get_ns();
			vb->sequence = dprx->sequence++;
			vb->field = V4L2_FIELD_NONE;

			vb2_buffer_done(&vb->vb2_buf, VB2_BUF_STATE_DONE);

			rbuf->entry_index = entry_index;
			rbuf->phy_addr = vb2_dma_contig_plane_dma_addr(&rbuf->vb.vb2_buf, 0);
			dprx->cur_buf[entry_index] = rbuf;
			list_del(&rbuf->link);
		} else {
			// TODO: trace skip frame
		}
		dprx->wrap_ops->dma_buf_cfg(dprx,
			entry_index, dprx->cur_buf[entry_index]->phy_addr);
	}

	/* Return IRQ_WAKE_THREAD if threaded handler is needed */
	return need_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static int rtk_dprx_ops_init(struct rtk_dprx *dprx)
{
	int ret;

	ret = rtk_dprx_rbus_init(dprx);
	if (ret)
		goto exit;

	ret = rtk_dprx_phy_init(dprx);
	if (ret)
		goto exit;

	ret = rtk_dprx_aux_init(dprx);
	if (ret)
		goto exit;

	ret = rtk_dprx_mac_init(dprx);
	if (ret)
		goto exit;

	ret = rtk_dprx_wrap_init(dprx);
	if (ret)
		goto exit;

	ret = rtk_dprx_edid_init(dprx);
	if (ret)
		goto exit;

	ret = rtk_dprx_audio_init(dprx);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int rtk_dprx_feature_init(struct rtk_dprx *dprx)
{
	dprx->hdcp_support = 0;
	dprx->phy_dat.lane_count = _DP_FOUR_LANE;
	dprx->aux_diff_mode = true;
	dprx->free_sync_support = false;
	dprx->audio_support = true;
	dprx->dpcd_ver = _DP_VERSION_1_2;
	dprx->max_link_rate = _DP_LINK_HBR2;

	return 0;
}

static int rtk_dprx_parse_rbus_dt(struct rtk_dprx *dprx)
{
	int ret = 0;
	struct device *dev = dprx->dev;
	struct device_node *syscon_np;

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 0 fail");
		ret = -ENODEV;
		goto exit;
	}

	dprx->dprx_reg = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(dprx->dprx_reg)) {
		dev_err(dev, "Remap syscon 0 to dprx_reg fail");
		of_node_put(syscon_np);
		ret = PTR_ERR(dprx->dprx_reg);
		goto exit;
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 1);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 1 fail");
		ret = -ENODEV;
		goto exit;
	}

	dprx->ip_reg = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(dprx->ip_reg)) {
		dev_err(dev, "Remap syscon 1 to ip_reg fail");
		of_node_put(syscon_np);
		ret = PTR_ERR(dprx->ip_reg);
		goto exit;
	}

exit:
	return ret;
}

static int rtk_dprx_parse_clk_dt(struct rtk_dprx *dprx)
{
	struct device *dev = dprx->dev;

	dprx->reset_dprx = devm_reset_control_get_optional_exclusive(dev, "rstn_dprx");
	if (IS_ERR(dprx->reset_dprx))
		return dev_err_probe(dev, PTR_ERR(dprx->reset_dprx),
					"Can't get reset_control rstn_dprx\n");

	dprx->clk_dprx = devm_clk_get(dev, "clk_en_dprx");
	if (IS_ERR(dprx->clk_dprx))
		return dev_err_probe(dev, PTR_ERR(dprx->clk_dprx),
					"Can't get clk clk_en_dprx\n");

	dprx->crt_clk_inited = false;

	return 0;
}

static int rtk_dprx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *syscon_np;
	struct rtk_dprx *dprx;
	int ret;
	int lt_ret;

	dprx = devm_kzalloc(dev, sizeof(*dprx), GFP_KERNEL);
	if (IS_ERR(dprx))
		return PTR_ERR(dprx);

	dprx->dev = dev;
	dev_set_drvdata(dev, dprx);

	dev_info(dprx->dev, "init begin\n");

	mutex_init(&dprx->video_lock);
	mutex_init(&dprx->buffer_lock);
	INIT_LIST_HEAD(&dprx->buffers);
	init_waitqueue_head(&dprx->detect_wait);

	/* Initialize Link Training DPCD IRQ pending status */
	atomic_set(&dprx->lt_dpcd_irq_pending, 0);

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 0 fail");
		ret = -ENODEV;
		goto err_exit;
	}

	ret = rtk_dprx_parse_clk_dt(dprx);
	if (ret)
		goto err_exit;

	ret = rtk_dprx_parse_rbus_dt(dprx);
	if (ret)
		goto err_exit;

	dprx->irq = irq_of_parse_and_map(dev->of_node, 0);
	if (!dprx->irq) {
		dev_err(dev, "Fail to get irq");
		ret = -ENODEV;
		goto err_exit;
	}
	dev_info(dprx->dev, "irq=%d\n", dprx->irq);

	ret = rtk_dprx_feature_init(dprx);
	if (ret)
		goto err_exit;

	ret = rtk_dprx_ops_init(dprx);
	if (ret)
		goto err_exit;

	ret = rtk_dprx_setup_video(dprx);
	if (ret)
		goto err_exit;

	ret = devm_request_threaded_irq(dev, dprx->irq,
				rtk_dprx_irq_handler,	/* hardirq handler */
				rtk_dprx_irq_thread,	/* threaded handler */
				IRQF_SHARED | IRQF_ONESHOT,
				dev_name(dev), dprx);
	if (ret) {
		dev_err(dev, "can't request dprx irq %d\n", dprx->irq);
		goto err_exit;
	}

	dprx->mac_ops->crt_clk_init(dprx);

#if 1 // TODO: Check init flow
	dprx->aux_ops->initial(dprx);
	dprx->aux_ops->set_pn_swap(dprx, false);
	dprx->edid_ops->set_dft(dprx);
	dprx->mac_dat.pre_color_depth = BIT_DEPTH_8BPC;
	dprx->mac_ops->mac_reset(dprx);
	dprx->mac_ops->mac_initial(dprx);
	dprx->mac_ops->sdp_initial(dprx);
	dprx->mac_ops->lane_count_set(dprx, dprx->phy_dat.lane_count);

	dprx->rbus_ops->mask_write(DPRX14_INTEN,
		DPRX14_INTEN_aux_ip_inten_mask, DPRX14_INTEN_aux_ip_inten_mask);
#endif

	/*
	 * Initialize Link Training module
	 * This should be done after video setup so V4L2 events work properly.
	 */
	lt_ret = rtk_dprx_link_training_init(dprx);
	if (lt_ret == -EPROBE_DEFER) {
		dev_info(dprx->dev, "Deferring probe: Link Training dependencies not ready\n");
		return -EPROBE_DEFER;
	} else if (lt_ret) {
		dev_warn(dprx->dev, "Link Training init failed: %d (non-fatal)\n", lt_ret);
		dev_warn(dprx->dev, "Continuing without Link Training support\n");
	}

	rtk_dprx_dma_pool(dprx);

	dev_info(dprx->dev, "init done\n");

	return 0;

err_exit:
	dev_err(dprx->dev, "init failed, ret=%d\n", ret);
	return ret;
}

static void rtk_dprx_remove(struct platform_device *pdev)
{
	struct rtk_dprx *dprx = platform_get_drvdata(pdev);

	/* Cleanup Link Training module */
	rtk_dprx_link_training_deinit(dprx);
	dev_info(dprx->dev, "Link Training module removed\n");

	video_unregister_device(&dprx->vdev);
	v4l2_device_unregister(&dprx->v4l2_dev);
}

static const struct of_device_id rtk_dprx_match[] = {
	{ .compatible = "realtek,rtk-dprx", },
	{},
};

MODULE_DEVICE_TABLE(of, rtk_dprx_match);

static struct platform_driver rtk_dprx_driver = {
	.driver = {
		.name = "rtk_dprx",
		.owner = THIS_MODULE,
		.of_match_table = rtk_dprx_match,
	},
	.probe = rtk_dprx_probe,
	.remove = rtk_dprx_remove,
};

module_platform_driver(rtk_dprx_driver);

MODULE_AUTHOR("Chase Yen <chase.yen@realtek.com>");
MODULE_DESCRIPTION("REALTEK DisplayPort receiver driver");
MODULE_LICENSE("GPL v2");
