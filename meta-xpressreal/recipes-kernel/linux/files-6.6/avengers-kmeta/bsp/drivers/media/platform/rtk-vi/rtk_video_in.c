// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_video_in.h"
#include "rtk_video_in_trace.h"

#define RTK_VI_VIDEO_NAME    "rtk-vi"

#define VI_MAX_CH  1

#define VI_MAX_WIDTH   1920
#define VI_MAX_HEIGHT  1080
#define VI_MIN_WIDTH	320
#define VI_MIN_HEIGHT	240

#define VI_0_VIDEO_NR  20
#define VI_1_VIDEO_NR  24

static const struct rtk_vi_fmt rtk_vi_fmt_list[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV12,
	},
};

#define NUM_FORMATS ARRAY_SIZE(rtk_vi_fmt_list)

static const struct v4l2_dv_timings_cap rtk_vi_timings_cap = {
	.type = V4L2_DV_BT_656_1120,
	.bt = {
		.min_width = VI_MIN_WIDTH,
		.max_width = VI_MAX_WIDTH,
		.min_height = VI_MIN_HEIGHT,
		.max_height = VI_MAX_HEIGHT,
		.min_pixelclock = 6574080, /* 640 x 480 x 24Hz */
		.max_pixelclock = 124416000, /* 1920 x 1080 x 60Hz */
		.standards = V4L2_DV_BT_STD_CEA861,
		.capabilities = V4L2_DV_BT_CAP_INTERLACED |
				V4L2_DV_BT_CAP_PROGRESSIVE,
	},
};

static const struct rtk_vi_fmt *rtk_vi_find_format(struct v4l2_format *f)
{
	const struct rtk_vi_fmt *fmt;
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++) {
		fmt = &rtk_vi_fmt_list[i];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (i == NUM_FORMATS)
		return NULL;

	return &rtk_vi_fmt_list[i];
}

static int rtk_vi_get_video_nr(struct rtk_vi *vi)
{
	int video_nr;

	if (vi == NULL) {
		video_nr = -1;
		goto exit;
	}

	switch (vi->ch_index) {
	case CH_0:
		video_nr = VI_0_VIDEO_NR;
		break;
	case CH_1:
		video_nr = VI_1_VIDEO_NR;
		break;
	default:
		video_nr = -1;
		break;
	}

exit:
	return video_nr;
}

static int rtk_vi_video_set_timing(struct rtk_vi *vi,
				     struct v4l2_bt_timings *timing)
{
	trace_vi_func_event(__func__);

	// TODO: regmap read/write

	return 0;
}

static void rtk_vi_video_detect_timing(struct rtk_vi *vi)
{
	struct v4l2_bt_timings *det = &vi->detected_timings;

	trace_vi_func_event(__func__);

	vi->v4l2_input_status = V4L2_IN_ST_NO_SIGNAL;
	// TODO: regmap read/write

	det->width = vi->src_width;
	det->height = vi->src_height;

	if (det->width && det->height)
		vi->v4l2_input_status = 0;
}

static int vi_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers, unsigned int *num_planes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct rtk_vi *vi = vb2_get_drv_priv(q);

	trace_vi_func_event(__func__);

	*num_planes = 1;

	if (vi->is_interlace)
		sizes[0] = roundup(vi->pix_fmt.sizeimage + 32, 4096); /* crc buf(32 bytes) */
	else
		sizes[0] = roundup(vi->pix_fmt.sizeimage + 16, 4096); /* crc buf(16 bytes) */

	dev_dbg(vi->dev, "%s, size=%u\n", __func__, sizes[0]);

	return 0;
}

static int vi_buf_prepare(struct vb2_buffer *vb)
{
	struct rtk_vi *vi = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long size;

	trace_vi_func_event(__func__);

	size = vb2_plane_size(vb, 0);

	if (size < vi->pix_fmt.sizeimage) {
		dev_err(vi->dev, "%s index=%u, vb2_plane_size(%lu) < sizeimage(%u)\n",
			__func__, vb->index, size, vi->pix_fmt.sizeimage);
		return -EINVAL;
	}

	dev_dbg(vi->dev, "%s index=%u, set bytesused=%u for plane\n",
		__func__, vb->index, vi->pix_fmt.sizeimage);

	vb2_set_plane_payload(vb, 0, vi->pix_fmt.sizeimage);

	return 0;
}

static void vi_buf_finish(struct vb2_buffer *vb)
{
	trace_vi_func_event(__func__);
}

static int vi_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct rtk_vi *vi = vb2_get_drv_priv(q);
	u8 entry_index;

	trace_vi_func_event(__func__);

	dev_info(vi->dev, "%s\n", __func__);

	mutex_lock(&vi->buffer_lock);
	for (entry_index = ENTRY_0; entry_index <= ENTRY_3; entry_index++) {
		struct rtk_vi_buffer *rbuf;

		rbuf = list_first_entry_or_null(&vi->buffers, struct rtk_vi_buffer, link);
		if (!rbuf) {
			mutex_unlock(&vi->buffer_lock);
			dev_info(vi->dev, "No buffer for streaming\n");
			return -ENOMEM;
		}
		rbuf->ch_index = vi->ch_index;
		rbuf->entry_index = entry_index;
		rbuf->phy_addr = vb2_dma_contig_plane_dma_addr(&rbuf->vb.vb2_buf, 0);
		vi->cur_buf[entry_index] = rbuf;
		vi->hw_ops->dma_buf_cfg(vi, entry_index, rbuf->phy_addr);
		list_del(&rbuf->link);
	}
	mutex_unlock(&vi->buffer_lock);

	vi->hw_ops->mac_ctrl(vi, vi->ch_index, ENABLE);

	return 0;
}

static void vi_stop_streaming(struct vb2_queue *q)
{
	struct rtk_vi *vi = vb2_get_drv_priv(q);
	int i;

	trace_vi_func_event(__func__);

	dev_info(vi->dev, "%s\n", __func__);

	vi->hw_ops->mac_ctrl(vi, vi->ch_index, DISABLE);
	vi->hw_ops->interrupt_ctrl(vi, vi->ch_index, DISABLE);

	mutex_lock(&vi->buffer_lock);

	for (i = 0; i < q->num_buffers; ++i) {
		dev_dbg(vi->dev, "q->bufs[%d]->state=%u\n", i, q->bufs[i]->state);

		if (q->bufs[i]->state == VB2_BUF_STATE_ACTIVE) {
			dev_dbg(vi->dev, "Set q->bufs[%d] done\n", i);
			vb2_buffer_done(q->bufs[i], VB2_BUF_STATE_ERROR);
		}
	}

	INIT_LIST_HEAD(&vi->buffers);
	vi->sequence = 0;
	vi->drop_cnt = 0;
	vi->ovf_cnt = 0;

	mutex_unlock(&vi->buffer_lock);

}

static void vi_buf_queue(struct vb2_buffer *vb)
{
	struct rtk_vi *vi = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rtk_vi_buffer *rbuf = to_vi_buffer(vbuf);

	dev_dbg(vi->dev, "%s index=%u\n", __func__, vb->index);

	trace_vi_buf_queue(vb->index);

	mutex_lock(&vi->buffer_lock);
	list_add_tail(&rbuf->link, &vi->buffers);
	mutex_unlock(&vi->buffer_lock);
}

static const struct vb2_ops rtk_vi_vb2_ops = {
	.queue_setup = vi_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_prepare = vi_buf_prepare,
	.buf_finish = vi_buf_finish,
	.start_streaming = vi_start_streaming,
	.stop_streaming = vi_stop_streaming,
	.buf_queue =  vi_buf_queue,
};

static int rtk_vi_video_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{
	trace_vi_func_event(__func__);

	strscpy(cap->driver, RTK_VI_VIDEO_NAME, sizeof(cap->driver));
	strscpy(cap->card, RTK_VI_VIDEO_NAME, sizeof(cap->card));

	return 0;
}

static int rtk_vi_video_enum_format(struct file *file, void *fh,
				struct v4l2_fmtdesc *f)
{
	struct rtk_vi *vi = video_drvdata(file);
	const struct rtk_vi_fmt *fmt;

	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	trace_vi_func_event(__func__);

	dev_dbg(vi->dev, "%s index(%u)\n", __func__, f->index);

	fmt = &rtk_vi_fmt_list[f->index];

	f->pixelformat = fmt->fourcc;

	return 0;
}

static int rtk_vi_video_get_format(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct rtk_vi *vi = video_drvdata(file);
	u32 size;

	trace_vi_func_event(__func__);

	if (vi->is_interlace) {
		vi->pix_fmt.field = V4L2_FIELD_SEQ_TB;
		vi->pix_fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
	} else {
		vi->pix_fmt.field = V4L2_FIELD_NONE;
		vi->pix_fmt.colorspace = V4L2_COLORSPACE_REC709;
	}
	vi->pix_fmt.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	vi->pix_fmt.width = vi->dst_width;
	vi->pix_fmt.height = vi->dst_height;

	size = roundup(vi->pix_fmt.width, 16) * vi->pix_fmt.height;
	vi->pix_fmt.sizeimage = size + (size >> 1);

	f->fmt.pix = vi->pix_fmt;

	dev_dbg(vi->dev, "%s %ux%u sizeimage=%u\n", __func__,
		f->fmt.pix.width, f->fmt.pix.height, f->fmt.pix.sizeimage);

	return 0;
}

static int rtk_vi_video_try_format(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct rtk_vi *vi = video_drvdata(file);
	const struct rtk_vi_fmt *fmt;

	dev_dbg(vi->dev, "%s width=%u height=%u pixelformat=%c%c%c%c\n",
		__func__, f->fmt.pix.width, f->fmt.pix.height,
		f->fmt.pix.pixelformat & 0xFF,
		(f->fmt.pix.pixelformat >> 8) & 0xFF,
		(f->fmt.pix.pixelformat >> 16) & 0xFF,
		(f->fmt.pix.pixelformat >> 24) & 0xFF);

	fmt = rtk_vi_find_format(f);
	if (!fmt)
		f->fmt.pix.pixelformat = rtk_vi_fmt_list[0].fourcc;

	if (vi->is_interlace) {
		f->fmt.pix.field = V4L2_FIELD_SEQ_TB;
		f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	} else {
		f->fmt.pix.field = V4L2_FIELD_NONE;
		f->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
	}
	f->fmt.pix.quantization = V4L2_QUANTIZATION_LIM_RANGE;

	return 0;
}

static int rtk_vi_video_set_format(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct rtk_vi *vi = video_drvdata(file);
	int ret;
	u32 size;
	unsigned long time_start;

	trace_vi_func_event(__func__);

	ret = rtk_vi_video_try_format(file, fh, f);
	if (ret)
		return ret;

	if (vb2_is_busy(&vi->queue)) {
		dev_err(vi->dev, "%s device busy\n", __func__);
		return -EBUSY;
	}

	if (!vi->hw_init_done) {
		ret = rtk_vi_hw_init(vi);
		if (ret)
			return ret;
	}

	vi->detect_done = false;

	vi->pix_fmt.width = f->fmt.pix.width;
	vi->pix_fmt.height = f->fmt.pix.height;
	vi->pix_fmt.pixelformat = f->fmt.pix.pixelformat;

	size = roundup(vi->pix_fmt.width, 16) * vi->pix_fmt.height;
	vi->pix_fmt.sizeimage = size + (size >> 1);

	vi->dst_width = f->fmt.pix.width;
	vi->dst_height = f->fmt.pix.height;

	vi->hw_ops->decoder_cfg(vi);
	vi->hw_ops->isp_cfg(vi);
	vi->hw_ops->packet_det_ctrl(vi, ENABLE);
	vi->hw_ops->interrupt_ctrl(vi, vi->ch_index, ENABLE);
	vi->hw_ops->mac_ctrl(vi, vi->ch_index, ENABLE);

	dev_info(vi->dev, "Wait detect done or timeout\n");
	time_start = jiffies;
	wait_event_interruptible_timeout(vi->detect_wait,
		vi->detect_done, msecs_to_jiffies(10000));

	dev_info(vi->dev, "detect %s, spent %ums\n",
		vi->detect_done ? "succeed" : "failed",
		jiffies_to_msecs(jiffies - time_start));

	if (!vi->detect_done)
		vi->hw_ops->dump_registers(vi, VI_FC, VI_CRC_CTRL);

	return 0;
}


static int rtk_vi_video_enum_framesizes(struct file *file, void *fh,
					struct v4l2_frmsizeenum *fsize)
{
	struct rtk_vi *vi = video_drvdata(file);

	if (fsize->index != 0)
		return -EINVAL;

	if (fsize->pixel_format != V4L2_PIX_FMT_NV12)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.min_width = VI_MIN_WIDTH;
	fsize->stepwise.max_width = vi->src_width;
	fsize->stepwise.step_width = 8;
	fsize->stepwise.min_height = VI_MIN_HEIGHT;
	fsize->stepwise.max_height = vi->src_height;
	fsize->stepwise.step_height = 8;

	dev_dbg(vi->dev, "frmsizeenum min %ux%u max %ux%u, step w=%u h=%u\n",
		fsize->stepwise.min_width, fsize->stepwise.min_height,
		fsize->stepwise.max_width, fsize->stepwise.max_height,
		fsize->stepwise.step_width, fsize->stepwise.step_height);

	return 0;
}

static int rtk_vi_video_enum_frameintervals(struct file *file, void *fh,
					struct v4l2_frmivalenum *fival)
{
	struct rtk_vi *vi = video_drvdata(file);

	if (fival->index > 0)
		return -EINVAL;

	if (fival->width < VI_MIN_WIDTH || fival->width > VI_MAX_WIDTH ||
	    fival->height < VI_MIN_HEIGHT || fival->height > VI_MAX_HEIGHT)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;

	/* fps = denominator / numerator */
	fival->discrete.denominator = 30;
	fival->discrete.numerator = 1;

	dev_dbg(vi->dev, "frmivalenum %ufps\n",
		fival->discrete.denominator / fival->discrete.numerator);

	return 0;
}

static int rtk_vi_video_set_dv_timings(struct file *file, void *fh,
				     struct v4l2_dv_timings *timings)
{
	struct rtk_vi *vi = video_drvdata(file);
	int ret;

	trace_vi_func_event(__func__);

	if (timings->bt.width == vi->active_timings.width &&
	    timings->bt.height == vi->active_timings.height)
		return 0;

	if (vb2_is_busy(&vi->queue)) {
		dev_err(vi->dev, "%s device busy\n", __func__);
		return -EBUSY;
	}

	ret = rtk_vi_video_set_timing(vi, &timings->bt);
	if (ret)
		return ret;

	timings->type = V4L2_DV_BT_656_1120;

	return 0;
}

static int rtk_vi_video_get_dv_timings(struct file *file, void *fh,
				     struct v4l2_dv_timings *timings)
{
	struct rtk_vi *vi = video_drvdata(file);
	struct v4l2_dv_timings t;
	bool exist;
	u8 vic;

	trace_vi_func_event(__func__);

	if (vi->src_width >= 1920)
		vic = 16;
	else if (vi->src_width >= 1280)
		vic = 19;
	else if (vi->src_height >= 576)
		vic = 21;
	else
		vic = 6;

	exist = v4l2_find_dv_timings_cea861_vic(&t, vic);
	if (exist)
		memcpy(&vi->active_timings, &t.bt, sizeof(t.bt));

	dev_dbg(vi->dev, "%s width=%u height=%u\n", __func__,
		vi->active_timings.width, vi->active_timings.height);

	timings->type = V4L2_DV_BT_656_1120;
	timings->bt = vi->active_timings;

	return 0;
}

static int rtk_vi_video_query_dv_timings(struct file *file, void *fh,
				       struct v4l2_dv_timings *timings)
{
	struct rtk_vi *vi = video_drvdata(file);

	trace_vi_func_event(__func__);

	dev_dbg(vi->dev, "%s\n", __func__);

	rtk_vi_video_detect_timing(vi);
	timings->type = V4L2_DV_BT_656_1120;
	timings->bt = vi->detected_timings;

	return vi->v4l2_input_status ? -ENOLINK : 0;
}

static int rtk_vi_video_enum_dv_timings(struct file *file, void *fh,
				      struct v4l2_enum_dv_timings *timings)
{
	trace_vi_func_event(__func__);

	return v4l2_enum_dv_timings_cap(timings, &rtk_vi_timings_cap,
					NULL, NULL);
}

static int rtk_vi_video_dv_timings_cap(struct file *file, void *fh,
				     struct v4l2_dv_timings_cap *cap)
{
	trace_vi_func_event(__func__);

	*cap = rtk_vi_timings_cap;

	return 0;
}

int rtk_vi_vb2_ioctl_dqbuf(struct file *file, void *priv,
		struct v4l2_buffer *p)
{
	int ret;
	unsigned long time_start;

	time_start = jiffies;

	ret = vb2_ioctl_dqbuf(file, priv, p);

	trace_vi_buf_dqueue(ret, jiffies_to_msecs(jiffies - time_start));

	return ret;
}

static const struct v4l2_ioctl_ops rtk_vi_video_ioctls = {
	/* VIDIOC_QUERYCAP handler */
	.vidioc_querycap = rtk_vi_video_querycap,
	/* VIDIOC_ENUM_FMT handlers */
	.vidioc_enum_fmt_vid_cap = rtk_vi_video_enum_format,
	/* VIDIOC_G_FMT handlers */
	.vidioc_g_fmt_vid_cap = rtk_vi_video_get_format,
	/* VIDIOC_S_FMT handlers */
	.vidioc_s_fmt_vid_cap = rtk_vi_video_set_format,
	/* VIDIOC_TRY_FMT handlers */
	.vidioc_try_fmt_vid_cap = rtk_vi_video_try_format,

	/* Buffer handlers */
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = rtk_vi_vb2_ioctl_dqbuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,

	/* Stream on/off */
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,

	.vidioc_enum_framesizes = rtk_vi_video_enum_framesizes,
	.vidioc_enum_frameintervals = rtk_vi_video_enum_frameintervals,

	/* DV Timings IOCTLs */
	.vidioc_s_dv_timings = rtk_vi_video_set_dv_timings,
	.vidioc_g_dv_timings = rtk_vi_video_get_dv_timings,
	.vidioc_query_dv_timings = rtk_vi_video_query_dv_timings,
	.vidioc_enum_dv_timings = rtk_vi_video_enum_dv_timings,
	.vidioc_dv_timings_cap = rtk_vi_video_dv_timings_cap,
};

static const struct v4l2_file_operations rtk_vi_v4l2_fops = {
	.owner = THIS_MODULE,
	.read = vb2_fop_read,
	.poll = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
};

static void rtk_vi_recovery_reset(struct rtk_vi *vi, u8 ch_index)
{
	unsigned long flags;
	u8 entry_index;

	spin_lock_irqsave(&vi->v4l2_dev.lock, flags);
	vi->hw_ops->mac_ctrl(vi, vi->ch_index, DISABLE);
	vi->hw_ops->interrupt_ctrl(vi, vi->ch_index, DISABLE);
	vi->hw_ops->mac_rst(vi, ch_index, ENABLE);
	vi->hw_ops->mac_rst(vi, ch_index, DISABLE);
	vi->hw_ops->decoder_cfg(vi);
	vi->hw_ops->isp_cfg(vi);

	for (entry_index = ENTRY_0; entry_index <= ENTRY_3; entry_index++)
		vi->hw_ops->dma_buf_cfg(vi, entry_index, vi->cur_buf[entry_index]->phy_addr);

	vi->hw_ops->interrupt_ctrl(vi, vi->ch_index, ENABLE);
	vi->hw_ops->mac_ctrl(vi, vi->ch_index, ENABLE);
	spin_unlock_irqrestore(&vi->v4l2_dev.lock, flags);
}

static irqreturn_t rtk_vi_irq_handler(int irq, void *dev_id)
{
	struct rtk_vi *vi = dev_id;
	int ret;
	u32 inst_a = 0;
	u32 inst_b = 0;
	u8 is_done;
	u8 entry_index;

	if (vi->hw_ops == NULL)
		return IRQ_HANDLED;

	ret = vi->hw_ops->state_isr(vi, vi->ch_index, &inst_a, &inst_b);
	if (ret) {
		rtk_vi_recovery_reset(vi, vi->ch_index);
		return IRQ_HANDLED;
	}

	if (!inst_a && !inst_b)
		return IRQ_HANDLED;

	if ((inst_a & VI_INTST_mac_ovf_mask) ||
		(inst_b & VI_INTST_mac_ovf_mask)) {
		dev_dbg(vi->dev, "Warn: mac entry overflow, inst_a=0x%08x inst_b=0x%08x\n",
			inst_a, inst_b);
		vi->hw_ops->clear_mac_ovf_flags(vi, vi->ch_index, inst_a, inst_b);
		vi->ovf_cnt++;
	}

	if ((inst_a & VI_INTST_mac_ovr_mask) ||
		(inst_b & VI_INTST_mac_ovr_mask)) {
		dev_err(vi->dev, "Error: DMA out of range, inst_a=0x%08x inst_b=0x%08x\n",
			inst_a, inst_b);
		vi->hw_ops->clear_mac_ovr_flags(vi, vi->ch_index, inst_a, inst_b);
	}

	for (entry_index = ENTRY_0; entry_index <= ENTRY_3; entry_index++) {
		struct rtk_vi_buffer *rbuf;

		is_done = vi->hw_ops->is_frame_done(entry_index, inst_a, inst_b);
		if (!is_done)
			continue;

		vi->hw_ops->clear_done_flag(vi, vi->ch_index, entry_index);
		rbuf = list_first_entry_or_null(&vi->buffers, struct rtk_vi_buffer, link);
		if (rbuf) {
			struct vb2_v4l2_buffer *vb;

			vb = &vi->cur_buf[entry_index]->vb;
			vb->vb2_buf.timestamp = ktime_get_ns();
			vb->sequence = vi->sequence++;
			vb->field = V4L2_FIELD_NONE;
			vb2_buffer_done(&vb->vb2_buf, VB2_BUF_STATE_DONE);

			trace_vi_buffer_done(vb->vb2_buf.index, entry_index,
				vb->vb2_buf.timestamp, vb->sequence);

			rbuf->ch_index = vi->ch_index;
			rbuf->entry_index = entry_index;
			rbuf->phy_addr = vb2_dma_contig_plane_dma_addr(&rbuf->vb.vb2_buf, 0);
			vi->cur_buf[entry_index] = rbuf;
			list_del(&rbuf->link);
		} else {
			vi->drop_cnt++;
			trace_vi_skip_frame(entry_index);
		}

		vi->hw_ops->dma_buf_cfg(vi, entry_index, (u64)vi->cur_buf[entry_index]->phy_addr);
	}

	return IRQ_HANDLED;
}

static int rtk_video_setup_video(struct rtk_vi *vi)
{
	struct v4l2_device *v4l2_dev = &vi->v4l2_dev;
	struct video_device *vdev = &vi->vdev;
	struct vb2_queue *vbq = &vi->queue;
	int video_nr;
	int ret;

	vi->pix_fmt.pixelformat = V4L2_PIX_FMT_NV12;
	vi->pix_fmt.field = V4L2_FIELD_SEQ_TB;
	vi->pix_fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
	vi->pix_fmt.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	vi->v4l2_input_status = V4L2_IN_ST_NO_SIGNAL;

	ret = v4l2_device_register(vi->dev, v4l2_dev);
	if (ret) {
		dev_err(vi->dev, "Failed to register v4l2 device\n");
		return ret;
	}

	vbq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vbq->io_modes = VB2_MMAP | VB2_DMABUF;
	vbq->dev = v4l2_dev->dev;
	vbq->lock = &vi->video_lock;
	vbq->ops = &rtk_vi_vb2_ops;
	vbq->mem_ops = &vb2_dma_contig_memops;
	vbq->drv_priv = vi;
	vbq->buf_struct_size = sizeof(struct rtk_vi_buffer);
	vbq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	vbq->min_buffers_needed = 6;

	ret = vb2_queue_init(vbq);
	if (ret) {
		dev_err(vi->dev, "Failed to init vb2 queue\n");
		return ret;
	}
	vdev->queue = vbq;
	vdev->fops = &rtk_vi_v4l2_fops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	vdev->v4l2_dev = v4l2_dev;
	strscpy(vdev->name, RTK_VI_VIDEO_NAME, sizeof(vdev->name));
	vdev->vfl_type = VFL_TYPE_VIDEO;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->release = video_device_release_empty;
	vdev->ioctl_ops = &rtk_vi_video_ioctls;
	vdev->lock = &vi->video_lock;

	video_nr = rtk_vi_get_video_nr(vi);
	video_set_drvdata(vdev, vi);
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, video_nr);
	if (ret) {
		dev_err(vi->dev, "Failed to register video device\n");
		return ret;
	}

	dev_info(vi->dev, "Registered %s as /dev/video%d\n",
		vdev->name, vdev->num);

	return 0;
}

ssize_t vi_dbg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t ret_count = 0;
	struct rtk_vi *vi = dev_get_drvdata(dev);

	ret_count += sprintf(buf + ret_count, "width/height: %ux%u -> %ux%u\n",
			vi->src_width, vi->src_height,
			vi->dst_width, vi->dst_height);
	ret_count += sprintf(buf + ret_count, "current sequence: %u\n",
			vi->sequence);
	ret_count += sprintf(buf + ret_count, "drop count: %u\n",
			vi->drop_cnt);
	ret_count += sprintf(buf + ret_count, "entry_ovf: %u\n",
			vi->ovf_cnt);

	return ret_count;
}

static DEVICE_ATTR_RO(vi_dbg);

static int rtk_vi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *syscon_np;
	struct rtk_vi *vi;
	u32 is_interlace;
	u32 ch_index;
	int ret;

	vi = devm_kzalloc(dev, sizeof(*vi), GFP_KERNEL);
	if (IS_ERR(vi))
		return PTR_ERR(vi);

	vi->dev = dev;
	dev_set_drvdata(dev, vi);

	dev_info(vi->dev, "init begin\n");

	mutex_init(&vi->video_lock);
	mutex_init(&vi->buffer_lock);
	INIT_LIST_HEAD(&vi->buffers);
	init_waitqueue_head(&vi->detect_wait);

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 0 fail");
		ret = -ENODEV;
		goto err_exit;
	}

	vi->vi_reg = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(vi->vi_reg)) {
		dev_err(dev, "Remap syscon 0 to vi_reg fail");
		of_node_put(syscon_np);
		ret = PTR_ERR(vi->vi_reg);
		goto err_exit;
	}

	vi->irq = irq_of_parse_and_map(dev->of_node, 0);
	if (!vi->irq) {
		dev_err(dev, "Fail to get irq");
		ret = -ENODEV;
		goto err_exit;
	}
	dev_info(vi->dev, "irq=%d\n", vi->irq);

	ret = of_property_read_u32(dev->of_node, "src-width",
				&vi->src_width);
	if (ret < 0 || vi->src_width < VI_MIN_WIDTH)
		vi->src_width = VI_MAX_WIDTH;

	ret = of_property_read_u32(dev->of_node, "src-height",
				&vi->src_height);
	if (ret < 0 || vi->src_height < VI_MIN_HEIGHT)
		vi->src_height = VI_MAX_HEIGHT;

	ret = of_property_read_u32(dev->of_node, "is-interlace",
				&is_interlace);
	if (ret < 0 || is_interlace == 0)
		vi->is_interlace = false;
	else
		vi->is_interlace = true;

	ret = of_property_read_u32(dev->of_node, "ch-index",
				&ch_index);
	if (ret < 0 || ch_index > VI_MAX_CH) {
		dev_err(dev, "Failed to get ch_index");
		ret = -EFAULT;
		goto err_exit;
	}
	vi->ch_index = ch_index;

	ret = of_property_read_u32(dev->of_node, "bt-mode",
				&vi->bt_mode);
	if (ret < 0 || vi->bt_mode > MODE_BT1120)
		vi->bt_mode = MODE_BT1120;

	ret = of_property_read_u32(dev->of_node, "separate-mode",
				&vi->separate_mode);
	if (ret < 0 || vi->separate_mode > SEP_YC_ASCEND)
		vi->separate_mode = SEP_NONE;

	ret = of_property_read_u32(dev->of_node, "cascade-mode",
				&vi->cascade_mode);
	if (ret < 0 || vi->cascade_mode > CASCADE_SLAVE || vi->separate_mode != SEP_NONE)
		vi->cascade_mode = CASCADE_OFF;

	dev_info(vi->dev, "ch%u src_width=%u src_height=%u is_interlace=%s\n",
			vi->ch_index, vi->src_width, vi->src_height,
			vi->is_interlace ? "Yes":"No");

	dev_info(vi->dev, "bt_mode=%u separate_mode=%u cascade_mode=%u\n",
		vi->bt_mode, vi->separate_mode, vi->cascade_mode);

	if (vi->ch_index != CH_0)
		goto skip_clk;

	vi->reset_vi = devm_reset_control_get_optional_exclusive(dev, "rstn_vi");
	if (IS_ERR(vi->reset_vi))
		return dev_err_probe(dev, PTR_ERR(vi->reset_vi),
					"Can't get reset_control rstn_vi\n");

	vi->clk_vi = devm_clk_get(dev, "clk_en_vi");
	if (IS_ERR(vi->clk_vi))
		return dev_err_probe(dev, PTR_ERR(vi->clk_vi),
					"Can't get clk clk_en_vi\n");
skip_clk:

	ret = dma_coerce_mask_and_coherent(vi->dev, DMA_BIT_MASK(64));
	if (ret)
		goto err_exit;

	ret = rtk_video_setup_video(vi);
	if (ret)
		goto err_exit;

	ret = devm_request_irq(dev, vi->irq, rtk_vi_irq_handler,
				IRQF_SHARED, dev_name(dev), vi);
	if (ret) {
		dev_err(dev, "can't request vi irq %d\n", vi->irq);
		goto err_exit;
	}

	ret = device_create_file(vi->dev, &dev_attr_vi_dbg);
	if (ret) {
		dev_err(vi->dev, "create vi_dbg sysfs failed");
		return -ENOMEM;
	}

	dev_info(vi->dev, "init done\n");

	return 0;

err_exit:
	dev_err(vi->dev, "init failed, ret=%d\n", ret);
	return ret;
}

static int rtk_vi_remove(struct platform_device *pdev)
{
	// TODO:
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int rtk_vi_suspend(struct device *dev)
{
	int ret;
	struct rtk_vi *vi = dev_get_drvdata(dev);

	ret = rtk_vi_hw_deinit(vi);

	return ret;
}

static int rtk_vi_resume(struct device *dev)
{

	return 0;
}

static const struct dev_pm_ops rtk_vi_pm_ops = {
	.suspend = rtk_vi_suspend,
	.resume = rtk_vi_resume,
	.freeze = rtk_vi_suspend,
	.thaw = rtk_vi_resume,
	.restore = rtk_vi_resume,
};
#endif

static void rtk_vi_shutdown(struct platform_device *pdev)
{
	struct rtk_vi *vi = dev_get_drvdata(&pdev->dev);

	rtk_vi_hw_deinit(vi);
}

static const struct of_device_id rtk_vi_match[] = {
	{ .compatible = "realtek,rtk-vi", },
	{},
};

MODULE_DEVICE_TABLE(of, rtk_vi_match);

static struct platform_driver rtk_vi_driver = {
	.driver = {
		.name = "rtk-vi",
		.owner = THIS_MODULE,
		.of_match_table = rtk_vi_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &rtk_vi_pm_ops,
#endif
	},
	.probe = rtk_vi_probe,
	.remove = rtk_vi_remove,
	.shutdown = rtk_vi_shutdown,
};

module_platform_driver(rtk_vi_driver);


MODULE_AUTHOR("Chase Yen <chase.yen@realtek.com>");
MODULE_DESCRIPTION("REALTEK Video Input Driver");
MODULE_LICENSE("GPL v2");
