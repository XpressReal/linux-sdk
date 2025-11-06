// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_mipi_csi.h"
#include "rtk_mipi_csi_trace.h"

#define RTK_MIPICSI_VIDEO_NAME    "rtk-mipi-csi"
#define MIPICSI_MAX_WIDTH   1920
#define MIPICSI_MAX_HEIGHT  1080
#define MIPICSI_MIN_WIDTH	320
#define MIPICSI_MIN_HEIGHT	240

#define FIXED_WIDTH   1920
#define FIXED_HEIGHT  1080

#define MIPICSI_CH0_VIDEO_NR  30
#define MIPICSI_CH1_VIDEO_NR  31
#define MIPICSI_CH2_VIDEO_NR  32
#define MIPICSI_CH3_VIDEO_NR  33
#define MIPICSI_CH4_VIDEO_NR  34
#define MIPICSI_CH5_VIDEO_NR  35
#define MIPICSI_GROUP_VIDEO_NR  50

#define METADA_ID  0x52544B6D
#define METADA_MAX_CH  6
/**
 * struct rtk_meta_buf - for RTK_METADA_BUF_MODE
 * @id: Fixed value - 0x52544B6D
 * @version: version of this meta struct
 * @mode: 0 - linear; 1 - compress
 * @valid_ch: number of valid channels
 * @reserved: currently unused
 * @fd:
 * @buf_size: size of frame buf
 * @done_ts: vb2_buffer_done of 90KHz timestamp
 * @start_ts: frame start of 90KHz timestamp
 * @end_ts: frame end of 90KHz timestamp
 * @crc: crc value of hardware metadata
 * @frame_cnt: frame count value of hardware metadata
 */
struct rtk_meta_buf {
	u32 id;
	u8 version;
	u8 mode;
	u8 valid_ch;
	u8 reserved;
	int fd[METADA_MAX_CH];
	u32 buf_size;
	u64 done_ts[METADA_MAX_CH];
	u64 start_ts[METADA_MAX_CH];
	u64 end_ts[METADA_MAX_CH];
	u32 crc[METADA_MAX_CH];
	u32 frame_cnt[METADA_MAX_CH];
} __attribute__ ((packed));

/**
 * struct rtk_streamparm_fd - VIDIOC_G_PARM v4l2_streamparm raw_data[200]
 * @header: 'r', 'f', 'd'
 * @size: number of fd in @fd_list
 * @fd_list: fd
 */
struct rtk_streamparm_fd {
	u8 header[3];
	u8 size;
	int fd_list[48]; /* 192 bytes */
	u8 reserved[4];
} __attribute__ ((packed));


struct rtk_mipicsi_fmt {
	u32 fourcc;
};

static const struct rtk_mipicsi_fmt rtk_mipicsi_fmt_list[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV12,
	},
};

#define NUM_FORMATS ARRAY_SIZE(rtk_mipicsi_fmt_list)

static const struct v4l2_dv_timings_cap rtk_mipicsi_timings_cap = {
	.type = V4L2_DV_BT_656_1120,
	.bt = {
		.min_width = MIPICSI_MIN_WIDTH,
		.max_width = MIPICSI_MAX_WIDTH,
		.min_height = MIPICSI_MIN_HEIGHT,
		.max_height = MIPICSI_MAX_HEIGHT,
		.min_pixelclock = 6574080, /* 640 x 480 x 24Hz */
		.max_pixelclock = 124416000, /* 1920 x 1080 x 60Hz */
		.standards = V4L2_DV_BT_STD_CEA861,
		.capabilities = V4L2_DV_BT_CAP_PROGRESSIVE,
	},
};

static struct v4l2_queryctrl mipicsi_v4l2_ctrls[] = {
	{
		.id = RTK_V4L2_CID_MIPI_COMPENC,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "COMPENC mode",
		.minimum = false,
		.maximum = true,
		.step = 1,
		.default_value = false,
		.flags = V4L2_CTRL_FLAG_WRITE_ONLY | V4L2_CTRL_FLAG_READ_ONLY,
	},
};

static const u32 mipicsi_ctrls_num = ARRAY_SIZE(mipicsi_v4l2_ctrls);

struct mipicsi_dma_buf_attachment {
	struct sg_table sgt;
};

static int mipicsi_dma_buf_attach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attach)
{
	struct rtk_mipicsi_dma_bufs *buf = dmabuf->priv;
	struct device *dev = buf->dev;
	dma_addr_t paddr = buf->paddr;
	void *vaddr = buf->vaddr;
	size_t size = dmabuf->size;

	struct mipicsi_dma_buf_attachment *a;
	int ret;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	ret = dma_get_sgtable(dev, &a->sgt, vaddr, paddr, size);
	if (ret < 0) {
		dev_err(dev, "failed to get scatterlist from DMA API\n");
		kfree(a);
		return -EINVAL;
	}

	attach->priv = a;

	return 0;
}

static void mipicsi_dma_buf_detatch(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attach)
{
	struct mipicsi_dma_buf_attachment *a = attach->priv;

	sg_free_table(&a->sgt);
	kfree(a);
}

static struct sg_table *mipicsi_map_dma_buf(struct dma_buf_attachment *attach,
			enum dma_data_direction dir)
{
	struct mipicsi_dma_buf_attachment *a = attach->priv;
	struct sg_table *table;
	int ret;

	table = &a->sgt;

	ret = dma_map_sgtable(attach->dev, table, dir, 0);
	if (ret)
		table = ERR_PTR(ret);

	return table;
}

static void mipicsi_unmap_dma_buf(struct dma_buf_attachment *attach,
			struct sg_table *table, enum dma_data_direction dir)
{
	dma_unmap_sgtable(attach->dev, table, dir, 0);
}

static int mipicsi_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct rtk_mipicsi_dma_bufs *buf = dmabuf->priv;
	struct device *dev = buf->dev;
	dma_addr_t paddr = buf->paddr;
	void *vaddr = buf->vaddr;
	size_t size = vma->vm_end - vma->vm_start;

	if (vaddr)
		return dma_mmap_coherent(dev, vma, vaddr, paddr, size);

	return 0;
}

static void mipicsi_release(struct dma_buf *dmabuf)
{
	struct rtk_mipicsi_dma_bufs *buf = dmabuf->priv;
	struct device *dev = buf->dev;
	dma_addr_t paddr = buf->paddr;
	void *vaddr = buf->vaddr;
	size_t size = dmabuf->size;

	dev_info(dev, "%s vaddr=0x%08lx phy_addr=0x%08lx %zu\n",
		__func__, (unsigned long)vaddr, (unsigned long)paddr, size);

	if (vaddr) {
		dma_free_coherent(dev, size, vaddr, paddr);
		buf->size = 0;
	}
}

static const struct dma_buf_ops mipicsi_dma_buf_ops = {
	.attach = mipicsi_dma_buf_attach,
	.detach = mipicsi_dma_buf_detatch,
	.map_dma_buf = mipicsi_map_dma_buf,
	.unmap_dma_buf = mipicsi_unmap_dma_buf,
	.mmap = mipicsi_mmap,
	.release = mipicsi_release,
};

static int mipicsi_export_dma_buf(struct rtk_mipicsi *mipicsi, u32 buf_index, size_t buf_size)
{
	struct rtk_mipicsi_group_data *g_data = mipicsi->g_data;
	struct dma_buf *dmabuf;
	int fd;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &mipicsi_dma_buf_ops;
	exp_info.size = buf_size;
	exp_info.flags = O_RDWR;
	exp_info.priv = &g_data->dma_bufs[buf_index];
	dmabuf = dma_buf_export(&exp_info);

	if (IS_ERR(dmabuf)) {
		dev_err(mipicsi->dev, "%s export fail, buf_index=%u\n",
				__func__, buf_index);

		return PTR_ERR(dmabuf);
	}

	g_data->dma_bufs[buf_index].dmabuf = dmabuf;

	fd = dma_buf_fd(dmabuf, exp_info.flags);
	if (fd < 0) {
		dev_err(mipicsi->dev, "%s dmabuf to fd fail\n", __func__);
		dma_buf_put(dmabuf);
		return -ENOMEM;
	}

	dev_info(mipicsi->dev, "dma_bufs[%u].fd=%d\n", buf_index, fd);
	g_data->dma_bufs[buf_index].fd = fd;

	return 0;
}

static int mipicsi_alloc_dma_bufs(struct rtk_mipicsi *mipicsi,
		u32 num_dma_bufs, size_t buf_size)
{
	struct rtk_mipicsi_group_data *g_data = mipicsi->g_data;
	int i, j;
	int ret;

	for (i = 0; i < num_dma_bufs; i++) {
		g_data->dma_bufs[i].vaddr = dma_alloc_coherent(mipicsi->dev, buf_size,
					&g_data->dma_bufs[i].paddr, GFP_KERNEL);

		g_data->dma_bufs[i].dev = mipicsi->dev;
		g_data->dma_bufs[i].size = buf_size;

		if (g_data->dma_bufs[i].vaddr == NULL) {
			for (j = 0; j < i; j++) {
				dma_free_coherent(mipicsi->dev, buf_size,
					g_data->dma_bufs[j].vaddr, g_data->dma_bufs[j].paddr);
				g_data->dma_bufs[j].size = 0;
			}
			return -ENOMEM;
		}

		dev_info(mipicsi->dev, "alloc dma addr 0x%08lx at 0x%08lx, size %zu\n",
			(unsigned long)g_data->dma_bufs[i].paddr,
			(unsigned long)g_data->dma_bufs[i].vaddr, buf_size);
	}

	for (i = 0; i < num_dma_bufs; i++) {
		ret = mipicsi_export_dma_buf(mipicsi, i, buf_size);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct rtk_mipicsi_fmt *rtk_mipicsi_find_format(struct v4l2_format *f)
{
	const struct rtk_mipicsi_fmt *fmt;
	u32 i;

	for (i = 0; i < NUM_FORMATS; i++) {
		fmt = &rtk_mipicsi_fmt_list[i];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (i == NUM_FORMATS)
		return NULL;

	return &rtk_mipicsi_fmt_list[i];
}

static int rtk_mipicsi_get_video_nr(struct rtk_mipicsi *mipicsi)
{
	int video_nr;

	if (mipicsi == NULL) {
		video_nr = -1;
		goto exit;
	}

	if (mipicsi->conf->en_group_dev) {
		video_nr = MIPICSI_GROUP_VIDEO_NR;
		goto exit;
	}

	switch (mipicsi->ch_index) {
	case CH_0:
		video_nr = MIPICSI_CH0_VIDEO_NR;
		break;
	case CH_1:
		video_nr = MIPICSI_CH1_VIDEO_NR;
		break;
	case CH_2:
		video_nr = MIPICSI_CH2_VIDEO_NR;
		break;
	case CH_3:
		video_nr = MIPICSI_CH3_VIDEO_NR;
		break;
	case CH_4:
		video_nr = MIPICSI_CH4_VIDEO_NR;
		break;
	case CH_5:
		video_nr = MIPICSI_CH5_VIDEO_NR;
		break;
	default:
		video_nr = -1;
		break;
	}

exit:
	return video_nr;
}

static int rtk_mipicsi_video_set_timing(struct rtk_mipicsi *mipicsi,
				     struct v4l2_bt_timings *timing)
{
	dev_info(mipicsi->dev, "%s width(%u) height(%u)\n",
		__func__, timing->width, timing->height);

	// TODO: regmap read/write

	return 0;
}

static void rtk_mipicsi_video_detect_timing(struct rtk_mipicsi *mipicsi)
{
	struct v4l2_bt_timings *det = &mipicsi->detected_timings;

	trace_mipicsi_func_event(__func__);

	mipicsi->v4l2_input_status = V4L2_IN_ST_NO_SIGNAL;
	// TODO: regmap read/write

	det->width = FIXED_WIDTH;
	det->height = FIXED_HEIGHT;

	if (det->width && det->height)
		mipicsi->v4l2_input_status = 0;
}

static int mipicsi_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers, unsigned int *num_planes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(q);

	dev_info(mipicsi->dev, "%s num_buffers=%u\n", __func__, *num_buffers);

	*num_planes = 1;

	sizes[0] = roundup(mipicsi->pix_fmt.sizeimage + 48, 4096); /*  Meta data(48bytes) */

	return 0;
}

static int mipicsi_queue_setup_g(struct vb2_queue *q,
				unsigned int *num_buffers, unsigned int *num_planes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(q);
	struct rtk_mipicsi_group_data *g_data = mipicsi->g_data;
	u32 num_dma_bufs;
	int ret;
	int i;

	dev_info(mipicsi->dev, "%s num_buffers=%u\n", __func__, *num_buffers);

	num_dma_bufs = *num_buffers * (mipicsi->ch_max + 1);
	if (num_dma_bufs > MAX_DMA_BUFS)
		return -EINVAL;

	*num_planes = 1;

	/* Check if previous dma bufs have already been released  */
	for (i = 0; i < g_data->num_dma_bufs; i++) {
		if (g_data->dma_bufs[i].size) {
			dev_err(mipicsi->dev, "User space didn't release previous dma_bufs, fd=%d\n",
				g_data->dma_bufs[i].fd);
			return -EPERM;
		}
	}

	memset_io(g_data, 0, sizeof(*g_data));

	ret = mipicsi_alloc_dma_bufs(mipicsi, num_dma_bufs,
			roundup(mipicsi->pix_fmt.sizeimage + 48, 4096));
	if (ret)
		return ret;

	g_data->num_dma_bufs = num_dma_bufs;
	INIT_LIST_HEAD(&mipicsi->g_data->dma_list);
	init_waitqueue_head(&mipicsi->g_data->done_wait);

	dev_info(mipicsi->dev, "Add %u dma_bufs into dma_list",
		g_data->num_dma_bufs);

	for (i = 0; i < g_data->num_dma_bufs; i++)
		list_add_tail(&g_data->dma_bufs[i].link, &g_data->dma_list);

	for (i = 0; i <= mipicsi->ch_max; i++)
		g_data->ready |= BIT(i);

	sizes[0] = 4096;

	return 0;
}

static int mipicsi_buf_prepare(struct vb2_buffer *vb)
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long size;

	size = vb2_plane_size(vb, 0);

	if (size < mipicsi->pix_fmt.sizeimage) {
		dev_err(mipicsi->dev, "%s index=%u, vb2_plane_size(%lu) < sizeimage(%u)\n",
			__func__, vb->index, size, mipicsi->pix_fmt.sizeimage);
		return -EINVAL;
	}

	trace_mipicsi_buf_prepare(vb->index, mipicsi->pix_fmt.sizeimage);

	vb2_set_plane_payload(vb, 0, mipicsi->pix_fmt.sizeimage);

	return 0;
}

static int mipicsi_buf_prepare_g(struct vb2_buffer *vb)
{
	unsigned long size;

	size = vb2_plane_size(vb, 0);

	trace_mipicsi_buf_prepare(vb->index, size);

	vb2_set_plane_payload(vb, 0, size);

	return 0;
}

static void mipicsi_buf_finish(struct vb2_buffer *vb)
{
	trace_mipicsi_buf_finish(vb->index);
}

static int mipicsi_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(q);
	u8 entry_index;

	trace_mipicsi_func_event(__func__);

	mutex_lock(&mipicsi->buffer_lock);
	for (entry_index = ENTRY_0; entry_index <= ENTRY_3; entry_index++) {
		struct rtk_mipicsi_buffer *rbuf;

		rbuf = list_first_entry_or_null(&mipicsi->buffers, struct rtk_mipicsi_buffer, link);
		if (!rbuf) {
			mutex_unlock(&mipicsi->buffer_lock);
			dev_info(mipicsi->dev, "No buffer for streaming\n");
			return -ENOMEM;
		}
		rbuf->ch_index = mipicsi->ch_index;
		rbuf->entry_index = entry_index;
		rbuf->phy_addr = vb2_dma_contig_plane_dma_addr(&rbuf->vb.vb2_buf, 0);
		mipicsi->cur_buf[entry_index] = rbuf;
		mipicsi->hw_ops->dma_buf_cfg(mipicsi, rbuf->phy_addr,
			mipicsi->ch_index, entry_index);
		list_del(&rbuf->link);
	}
	mutex_unlock(&mipicsi->buffer_lock);

	mipicsi->hw_ops->dump_frame_state(mipicsi, mipicsi->ch_index);
	mipicsi->hw_ops->interrupt_ctrl(mipicsi, mipicsi->ch_index, ENABLE);
	mipicsi->hw_ops->app_ctrl(mipicsi, mipicsi->ch_index, ENABLE);

	return 0;
}

static int mipicsi_start_streaming_g(struct vb2_queue *q, unsigned int count)
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(q);
	struct rtk_mipicsi_group_data *g_data = mipicsi->g_data;
	u8 ch_index;
	u8 entry_index;

	trace_mipicsi_func_event(__func__);

	mipicsi->g_data->do_stop = false;

	mutex_lock(&mipicsi->buffer_lock);
	for (ch_index = 0; ch_index <= mipicsi->ch_max; ch_index++) {
		for (entry_index = ENTRY_0; entry_index <= ENTRY_3; entry_index++) {
			struct rtk_mipicsi_dma_bufs *dma_buf;

			dma_buf = list_first_entry_or_null(&g_data->dma_list,
						struct rtk_mipicsi_dma_bufs, link);
			if (!dma_buf) {
				mutex_unlock(&mipicsi->buffer_lock);
				dev_info(mipicsi->dev, "No buffer for streaming\n");
				return -ENOMEM;
			}

			mipicsi->cur_dma_buf[ch_index][entry_index] = dma_buf;

			mipicsi->hw_ops->dma_buf_cfg(mipicsi, dma_buf->paddr,
				ch_index, entry_index);
			list_del(&dma_buf->link);
		}
	}
	mutex_unlock(&mipicsi->buffer_lock);

	mipicsi->hw_ops->interrupt_ctrl(mipicsi, mipicsi->ch_max, ENABLE);
	mipicsi->hw_ops->app_ctrl(mipicsi, mipicsi->ch_max, ENABLE);

	return 0;
}

static void mipicsi_stop_streaming(struct vb2_queue *q)
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(q);
	int i;

	trace_mipicsi_func_event(__func__);

	mipicsi->hw_ops->app_ctrl(mipicsi, mipicsi->ch_index, DISABLE);
	mipicsi->hw_ops->interrupt_ctrl(mipicsi, mipicsi->ch_index, DISABLE);

	mutex_lock(&mipicsi->buffer_lock);

	for (i = 0; i < q->num_buffers; ++i) {

		dev_info(mipicsi->dev, "q->bufs[%d]->state=%u\n", i, q->bufs[i]->state);

		if (q->bufs[i]->state == VB2_BUF_STATE_ACTIVE) {
			dev_info(mipicsi->dev, "Set q->bufs[%d] done\n", i);
			vb2_buffer_done(q->bufs[i], VB2_BUF_STATE_ERROR);
		}
	}

	INIT_LIST_HEAD(&mipicsi->buffers);
	mipicsi->sequence = 0;

	mutex_unlock(&mipicsi->buffer_lock);
}

static void mipicsi_stop_streaming_g(struct vb2_queue *q)
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(q);
	int i;

	trace_mipicsi_func_event(__func__);

	mipicsi->hw_ops->app_ctrl(mipicsi, mipicsi->ch_max, DISABLE);
	mipicsi->hw_ops->interrupt_ctrl(mipicsi, mipicsi->ch_max, DISABLE);

	mipicsi->g_data->do_stop = true;
	wake_up_interruptible(&mipicsi->g_data->done_wait);

	mutex_lock(&mipicsi->buffer_lock);

	for (i = 0; i < q->num_buffers; ++i) {

		dev_info(mipicsi->dev, "q->bufs[%d]->state=%u\n", i, q->bufs[i]->state);

		if (q->bufs[i]->state == VB2_BUF_STATE_ACTIVE) {
			dev_info(mipicsi->dev, "Set q->bufs[%d] done\n", i);
			vb2_buffer_done(q->bufs[i], VB2_BUF_STATE_ERROR);
		}
	}

	INIT_LIST_HEAD(&mipicsi->buffers);

	mutex_unlock(&mipicsi->buffer_lock);
}

static void mipicsi_buf_queue(struct vb2_buffer *vb)
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rtk_mipicsi_buffer *rbuf = to_mipicsi_buffer(vbuf);

	trace_mipicsi_buf_queue(vb->index);

	mutex_lock(&mipicsi->buffer_lock);
	list_add_tail(&rbuf->link, &mipicsi->buffers);
	mutex_unlock(&mipicsi->buffer_lock);
}

static void mipicsi_buf_queue_g(struct vb2_buffer *vb)
{
	struct rtk_mipicsi *mipicsi = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rtk_mipicsi_buffer *rbuf = to_mipicsi_buffer(vbuf);
	struct rtk_mipicsi_group_data *g_data = mipicsi->g_data;
	int i;

	trace_mipicsi_buf_queue(vb->index);

	if (vb->index >= NUM_MAX_VB) {
		dev_err(mipicsi->dev, "Unexpected vb->index=%u > NUM_MAX_VB(%u)\n",
			vb->index, NUM_MAX_VB);
		return;
	}

	mutex_lock(&mipicsi->buffer_lock);
	list_add_tail(&rbuf->link, &mipicsi->buffers);
	mutex_unlock(&mipicsi->buffer_lock);

	if (!g_data->dq_dma_buf[vb->index][0])
		return;

	for (i = 0; i <= mipicsi->ch_max; i++) {
		struct rtk_mipicsi_dma_bufs *buf;

		buf = g_data->dq_dma_buf[vb->index][i];
		list_add_tail(&buf->link, &g_data->dma_list);
	}
}

static const struct vb2_ops rtk_mipicsi_vb2_ops = {
	.queue_setup = mipicsi_queue_setup,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_prepare = mipicsi_buf_prepare,
	.buf_finish = mipicsi_buf_finish,
	.start_streaming = mipicsi_start_streaming,
	.stop_streaming = mipicsi_stop_streaming,
	.buf_queue =  mipicsi_buf_queue,
};

static const struct vb2_ops rtk_mipicsi_vb2_ops_g = {
	.queue_setup = mipicsi_queue_setup_g,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.buf_prepare = mipicsi_buf_prepare_g,
	.buf_finish = mipicsi_buf_finish,
	.start_streaming = mipicsi_start_streaming_g,
	.stop_streaming = mipicsi_stop_streaming_g,
	.buf_queue =  mipicsi_buf_queue_g,
};

static int rtk_mipicsi_video_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{
	strscpy(cap->driver, RTK_MIPICSI_VIDEO_NAME, sizeof(cap->driver));
	strscpy(cap->card, RTK_MIPICSI_VIDEO_NAME, sizeof(cap->card));

	return 0;
}

static int rtk_mipicsi_video_enum_format(struct file *file, void *fh,
				struct v4l2_fmtdesc *f)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);
	const struct rtk_mipicsi_fmt *fmt;

	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	dev_info(mipicsi->dev, "%s index(%u)\n", __func__, f->index);

	fmt = &rtk_mipicsi_fmt_list[f->index];

	f->pixelformat = fmt->fourcc;

	return 0;
}

static int rtk_mipicsi_video_get_format(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);

	mipicsi->pix_fmt.field = V4L2_FIELD_NONE;
	mipicsi->pix_fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
	mipicsi->pix_fmt.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	mipicsi->pix_fmt.width = mipicsi->dst_width;
	mipicsi->pix_fmt.height = mipicsi->dst_height;

	mipicsi->pix_fmt.sizeimage = mipicsi->hw_ops->calculate_video_size(mipicsi->dst_width,
			mipicsi->dst_height, mipicsi->mode);

	f->fmt.pix = mipicsi->pix_fmt;

	dev_info(mipicsi->dev, "%s %ux%u sizeimage=%u\n", __func__,
		f->fmt.pix.width, f->fmt.pix.height, f->fmt.pix.sizeimage);

	return 0;
}

static int rtk_mipicsi_video_try_format(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);
	const struct rtk_mipicsi_fmt *fmt;

	dev_info(mipicsi->dev, "%s width=%u height=%u pixelformat=%c%c%c%c\n",
		__func__, f->fmt.pix.width, f->fmt.pix.height,
		f->fmt.pix.pixelformat & 0xFF,
		(f->fmt.pix.pixelformat >> 8) & 0xFF,
		(f->fmt.pix.pixelformat >> 16) & 0xFF,
		(f->fmt.pix.pixelformat >> 24) & 0xFF);

	fmt = rtk_mipicsi_find_format(f);
	if (!fmt)
		return -EINVAL;

	f->fmt.pix.field = V4L2_FIELD_NONE;
	f->fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
	f->fmt.pix.quantization = V4L2_QUANTIZATION_LIM_RANGE;

	return 0;
}

static int rtk_mipicsi_video_set_format(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);
	int ret;

	dev_info(mipicsi->dev, "%s %ux%u\n", __func__,
		f->fmt.pix.width, f->fmt.pix.height);

	ret = rtk_mipicsi_video_try_format(file, fh, f);
	if (ret)
		return ret;

	if (vb2_is_busy(&mipicsi->queue)) {
		dev_err(mipicsi->dev, "%s device busy\n", __func__);
		return -EBUSY;
	}

	if (!mipicsi->hw_init_done) {
		ret = rtk_mipi_csi_hw_init(mipicsi);
		if (ret) {
			dev_err(mipicsi->dev, "hw_init failed, ret=%d\n", ret);
			return ret;
		}
	}

	mipicsi->pix_fmt.width = f->fmt.pix.width;
	mipicsi->pix_fmt.height = f->fmt.pix.height;
	mipicsi->pix_fmt.pixelformat = f->fmt.pix.pixelformat;

	mipicsi->dst_width = f->fmt.pix.width;
	mipicsi->dst_height = f->fmt.pix.height;

	mipicsi->pix_fmt.sizeimage = mipicsi->hw_ops->calculate_video_size(mipicsi->dst_width,
			mipicsi->dst_height, mipicsi->mode);

	mipicsi->hw_ops->app_size_cfg(mipicsi, mipicsi->ch_index);

	mipicsi->hw_ops->dump_frame_state(mipicsi, mipicsi->ch_index);

	if ((mipicsi->ch_index == CH_0) || (mipicsi->conf->en_group_dev)) {
		if (mipicsi->debug.en_colorbar)
			mipicsi->hw_ops->color_bar_test(mipicsi, ENABLE);
		else
			mipicsi->hw_ops->color_bar_test(mipicsi, DISABLE);

		mipicsi->hw_ops->crc_ctrl(mipicsi, ENABLE);
		mipicsi->hw_ops->meta_swap(mipicsi, ENABLE);
	}

	return 0;
}

static int rtk_vb2_ioctl_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct video_device *vdev = video_devdata(file);
	struct rtk_mipicsi *mipicsi = video_drvdata(file);
	int ret;
	unsigned long flags;
	struct rtk_mipicsi_buffer *rbuf;
	struct rtk_mipicsi_group_data *g_data;
	struct vb2_v4l2_buffer *vb;
	struct rtk_meta_buf *meta_buf;
	int i;

	trace_mipicsi_func_event(__func__);

	/* This function is only for group device featue */
	if (!mipicsi->conf->en_group_dev)
		goto std_dqbuf;

	g_data = mipicsi->g_data;

	/* Get metadata buffer */
	mutex_lock(&mipicsi->buffer_lock);
	rbuf = list_first_entry_or_null(&mipicsi->buffers, struct rtk_mipicsi_buffer, link);
	if (!rbuf) {
		dev_err(mipicsi->dev, "%s no rbuf\n", __func__);
		mutex_unlock(&mipicsi->buffer_lock);
		ret = -ENOMEM;
		goto exit;
	}
	list_del(&rbuf->link);
	mutex_unlock(&mipicsi->buffer_lock);

	dev_info(mipicsi->dev, "Wait all channels frame ready or timeout\n");
	wait_event_interruptible_timeout(g_data->done_wait,
		(g_data->done_flags == g_data->ready) || g_data->do_stop,
		msecs_to_jiffies(2000));

	if (g_data->do_stop) {
		ret = -EPIPE;
		goto exit;
	}

	vb = &rbuf->vb;

	if (vb->vb2_buf.index >= NUM_MAX_VB) {
		dev_err(mipicsi->dev, "Unexpected dq_dma_buf vb2_buf.index=%u > NUM_MAX_VB(%u)\n",
				vb->vb2_buf.index, NUM_MAX_VB);
		ret = -ERANGE;
		goto exit;
	}

	spin_lock_irqsave(&mipicsi->slock, flags);

	meta_buf = (struct rtk_meta_buf *)vb2_plane_vaddr(&vb->vb2_buf, 0);
	meta_buf->id = METADA_ID;
	meta_buf->valid_ch = mipicsi->ch_max + 1;
	meta_buf->mode = mipicsi->mode;
	for (i = 0; i <= mipicsi->ch_max; i++) {
		void *vaddr;
		struct rtk_mipi_meta_data *meta_data;
		uint64_t f_start_ts;
		uint64_t f_end_ts;

		meta_buf->fd[i] = g_data->done_dma_buf[i]->fd;
		meta_buf->done_ts[i] = g_data->done_ts[i];

		vaddr = g_data->done_dma_buf[i]->vaddr;
		meta_data = (struct rtk_mipi_meta_data *)(vaddr + mipicsi->video_size);

		mipicsi->hw_ops->dump_meta_data(mipicsi, meta_data);

		f_start_ts = meta_data->f_start_ts_m;
		f_start_ts = (f_start_ts << 32) | meta_data->f_start_ts_l;
		f_end_ts = meta_data->f_end_ts_m;
		f_end_ts = (f_end_ts << 32) | meta_data->f_end_ts_l;
		meta_buf->start_ts[i] = f_start_ts;
		meta_buf->end_ts[i] = f_end_ts;
		meta_buf->crc[i] = get_meta_misc0_crc(meta_data->misc0);
		meta_buf->frame_cnt[i] = get_meta_misc0_frame_cnt(meta_data->misc0);

		g_data->dq_dma_buf[vb->vb2_buf.index][i] = g_data->done_dma_buf[i];
	}
	meta_buf->buf_size = g_data->done_dma_buf[0]->size;

	vb->vb2_buf.timestamp = ktime_get_ns();
	vb->sequence = mipicsi->sequence++;
	vb->field = V4L2_FIELD_NONE;

	vb2_buffer_done(&vb->vb2_buf, VB2_BUF_STATE_DONE);

	trace_mipicsi_buffer_done(vb->vb2_buf.index, mipicsi->ch_max,
				0, vb->vb2_buf.timestamp, vb->sequence);

	g_data->done_flags = 0;

	spin_unlock_irqrestore(&mipicsi->slock, flags);

std_dqbuf:
	ret = vb2_dqbuf(vdev->queue, p, file->f_flags & O_NONBLOCK);
exit:
	if (ret == 0)
		trace_mipicsi_buf_dqueue(p->sequence, ktime_get_ns()/1000);
	 else
		trace_mipicsi_buf_dqueue_err(ret);

	return ret;
}

static int rtk_mipicsi_vidioc_enum_framesizes(struct file *file, void *priv,
					struct v4l2_frmsizeenum *fsize)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);

	trace_mipicsi_func_event(__func__);

	if (fsize->index != 0)
		return -EINVAL;

	if (fsize->pixel_format != V4L2_PIX_FMT_NV12)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.min_width = MIPICSI_MIN_WIDTH;
	fsize->stepwise.max_width = mipicsi->src_width;
	fsize->stepwise.step_width = 8;
	fsize->stepwise.min_height = MIPICSI_MIN_HEIGHT;
	fsize->stepwise.max_height = mipicsi->src_height;
	fsize->stepwise.step_height = 8;

	dev_info(mipicsi->dev, "frmsizeenum min %ux%u max %ux%u, step w=%u h=%u\n",
		fsize->stepwise.min_width, fsize->stepwise.min_height,
		fsize->stepwise.max_width, fsize->stepwise.max_height,
		fsize->stepwise.step_width, fsize->stepwise.step_height);

	return 0;
}

static int rtk_mipicsi_vidioc_enum_frameintervals(struct file *file,
				void *fh, struct v4l2_frmivalenum *fival)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);

	if (fival->index > 0)
		return -EINVAL;

	if (fival->width < MIPICSI_MIN_WIDTH || fival->width > mipicsi->src_width ||
	    fival->height < MIPICSI_MIN_HEIGHT || fival->height > mipicsi->src_height)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;

	/* fps = denominator / numerator */
	fival->discrete.denominator = 30;
	fival->discrete.numerator = 1;

	dev_info(mipicsi->dev, "frmivalenum %ufps\n",
		fival->discrete.denominator / fival->discrete.numerator);

	return 0;
}

static int rtk_mipicsi_video_set_dv_timings(struct file *file, void *fh,
				     struct v4l2_dv_timings *timings)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);
	int ret;

	if (timings->bt.width == mipicsi->active_timings.width &&
	    timings->bt.height == mipicsi->active_timings.height)
		return 0;

	if (vb2_is_busy(&mipicsi->queue)) {
		dev_err(mipicsi->dev, "%s device busy\n", __func__);
		return -EBUSY;
	}

	ret = rtk_mipicsi_video_set_timing(mipicsi, &timings->bt);
	if (ret)
		return ret;

	timings->type = V4L2_DV_BT_656_1120;

	return 0;
}

static int rtk_mipicsi_video_get_dv_timings(struct file *file, void *fh,
				     struct v4l2_dv_timings *timings)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);
	struct v4l2_dv_timings t;
	bool exist;
	u8 vic;

	if (mipicsi->src_width >= 3840)
		vic = 95;
	if (mipicsi->src_width >= 1920)
		vic = 16;
	else if (mipicsi->src_width >= 1280)
		vic = 19;
	else
		vic = 16;

	exist = v4l2_find_dv_timings_cea861_vic(&t, vic);
	if (exist) {
		memcpy(&mipicsi->active_timings, &t.bt, sizeof(t.bt));
	} else {
		mipicsi->active_timings.width = mipicsi->src_width;
		mipicsi->active_timings.height = mipicsi->src_height;
		mipicsi->active_timings.interlaced = 0;
	}

	dev_dbg(mipicsi->dev, "%s width=%u height=%u\n", __func__,
		mipicsi->active_timings.width, mipicsi->active_timings.height);

	timings->type = V4L2_DV_BT_656_1120;
	timings->bt = mipicsi->active_timings;

	return 0;
}

static int rtk_mipicsi_video_query_dv_timings(struct file *file, void *fh,
				       struct v4l2_dv_timings *timings)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);

	dev_info(mipicsi->dev, "%s\n", __func__);

	rtk_mipicsi_video_detect_timing(mipicsi);
	timings->type = V4L2_DV_BT_656_1120;
	timings->bt = mipicsi->detected_timings;

	return mipicsi->v4l2_input_status ? -ENOLINK : 0;
}

static int rtk_mipicsi_video_enum_dv_timings(struct file *file, void *fh,
				      struct v4l2_enum_dv_timings *timings)
{
	return v4l2_enum_dv_timings_cap(timings, &rtk_mipicsi_timings_cap,
					NULL, NULL);
}

static int rtk_mipicsi_video_dv_timings_cap(struct file *file, void *fh,
				     struct v4l2_dv_timings_cap *cap)
{
	*cap = rtk_mipicsi_timings_cap;

	return 0;
}

static int rtk_mipicsi_vidioc_queryctrl(struct file *file, void *fh,
				struct v4l2_queryctrl *queryctrl)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);
	int i;
	int ret = -EINVAL;

	dev_info(mipicsi->dev, "%s id=0x%x\n", __func__, queryctrl->id);

	if (V4L2_CTRL_ID2WHICH(queryctrl->id) != V4L2_CID_PRIVATE_BASE)
		return ret;

	for (i = 0; i < mipicsi_ctrls_num; i++) {
		if (mipicsi_v4l2_ctrls[i].id == queryctrl->id) {
			memcpy(queryctrl, &mipicsi_v4l2_ctrls[i],
			       sizeof(struct v4l2_queryctrl));
			ret = 0;
			break;
		}
	}

	return ret;
}

static int rtk_mipicsi_vidioc_g_ctrl(struct file *file, void *fh,
				struct v4l2_control *ctrl)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);

	if (ctrl->id != RTK_V4L2_CID_MIPI_COMPENC)
		return -EINVAL;

	ctrl->value = (mipicsi->mode == DATA_MODE_COMPENC);

	return 0;
}

static int rtk_mipicsi_vidioc_s_ctrl(struct file *file, void *fh,
				struct v4l2_control *ctrl)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);

	if (ctrl->id != RTK_V4L2_CID_MIPI_COMPENC)
		return -EINVAL;

	dev_info(mipicsi->dev, "%s %s\n", __func__,
			ctrl->value ? "MODE=COMPENC" : "MODE=LINE");

	if (ctrl->value)
		mipicsi->mode = DATA_MODE_COMPENC;
	else
		mipicsi->mode = DATA_MODE_LINE;

	return 0;
}

static int rtk_mipicsi_video_g_parm(struct file *file, void *fh,
				struct v4l2_streamparm *a)
{
	struct rtk_mipicsi *mipicsi = video_drvdata(file);
	struct rtk_mipicsi_group_data *g_data = mipicsi->g_data;
	struct rtk_streamparm_fd *param_fd;
	u32 i;

	if (!mipicsi->conf->en_group_dev)
		return -ESRCH;

	dev_info(mipicsi->dev, "%s type=%u\n", __func__, a->type);

	param_fd = (struct rtk_streamparm_fd *)&a->parm.raw_data[0];
	memset_io(param_fd, 0, sizeof(struct rtk_streamparm_fd));

	param_fd->header[0] = 'r';
	param_fd->header[1] = 'f';
	param_fd->header[2] = 'd';
	param_fd->size = g_data->num_dma_bufs;

	for (i = 0; i < g_data->num_dma_bufs; i++)
		if (g_data->dma_bufs[i].size != 0)
			param_fd->fd_list[i] = g_data->dma_bufs[i].fd;

	return 0;
}

static const struct v4l2_ioctl_ops rtk_mipicsi_video_ioctls = {
	/* VIDIOC_QUERYCAP handler */
	.vidioc_querycap = rtk_mipicsi_video_querycap,
	/* VIDIOC_ENUM_FMT handlers */
	.vidioc_enum_fmt_vid_cap = rtk_mipicsi_video_enum_format,
	/* VIDIOC_G_FMT handlers */
	.vidioc_g_fmt_vid_cap = rtk_mipicsi_video_get_format,
	/* VIDIOC_S_FMT handlers */
	.vidioc_s_fmt_vid_cap = rtk_mipicsi_video_set_format,
	/* VIDIOC_TRY_FMT handlers */
	.vidioc_try_fmt_vid_cap = rtk_mipicsi_video_try_format,

	/* Buffer handlers */
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = rtk_vb2_ioctl_dqbuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,

	/* Stream on/off */
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,

	.vidioc_enum_framesizes = rtk_mipicsi_vidioc_enum_framesizes,
	.vidioc_enum_frameintervals = rtk_mipicsi_vidioc_enum_frameintervals,

	/* DV Timings IOCTLs */
	.vidioc_s_dv_timings = rtk_mipicsi_video_set_dv_timings,
	.vidioc_g_dv_timings = rtk_mipicsi_video_get_dv_timings,
	.vidioc_query_dv_timings = rtk_mipicsi_video_query_dv_timings,
	.vidioc_enum_dv_timings = rtk_mipicsi_video_enum_dv_timings,
	.vidioc_dv_timings_cap = rtk_mipicsi_video_dv_timings_cap,

	/* Control handling */
	.vidioc_queryctrl = rtk_mipicsi_vidioc_queryctrl,
	.vidioc_g_ctrl = rtk_mipicsi_vidioc_g_ctrl,
	.vidioc_s_ctrl = rtk_mipicsi_vidioc_s_ctrl,

	/* Stream type-dependent parameter ioctls */
	.vidioc_g_parm = rtk_mipicsi_video_g_parm,
};

static const struct v4l2_file_operations rtk_mipicsi_v4l2_fops = {
	.owner = THIS_MODULE,
	.read = vb2_fop_read,
	.poll = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
};

static int rtk_video_setup_video(struct rtk_mipicsi *mipicsi)
{
	struct v4l2_device *v4l2_dev = &mipicsi->v4l2_dev;
	struct video_device *vdev = &mipicsi->vdev;
	struct vb2_queue *vbq = &mipicsi->queue;
	int video_nr;
	int ret;

	mipicsi->pix_fmt.pixelformat = V4L2_PIX_FMT_NV12;
	mipicsi->pix_fmt.field = V4L2_FIELD_NONE;
	mipicsi->pix_fmt.colorspace = V4L2_COLORSPACE_REC709;
	mipicsi->pix_fmt.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	mipicsi->v4l2_input_status = V4L2_IN_ST_NO_SIGNAL;

	ret = v4l2_device_register(mipicsi->dev, v4l2_dev);
	if (ret) {
		dev_err(mipicsi->dev, "Failed to register v4l2 device\n");
		return ret;
	}

	vbq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vbq->io_modes = VB2_MMAP | VB2_DMABUF;
	vbq->dev = v4l2_dev->dev;
	vbq->lock = &mipicsi->video_lock;

	if (mipicsi->conf->en_group_dev)
		vbq->ops = &rtk_mipicsi_vb2_ops_g;
	else
		vbq->ops = &rtk_mipicsi_vb2_ops;

	vbq->mem_ops = &vb2_dma_contig_memops;
	vbq->drv_priv = mipicsi;
	vbq->buf_struct_size = sizeof(struct rtk_mipicsi_buffer);
	vbq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	vbq->min_buffers_needed = 6;

	ret = vb2_queue_init(vbq);
	if (ret) {
		dev_err(mipicsi->dev, "Failed to init vb2 queue\n");
		return ret;
	}
	vdev->queue = vbq;
	vdev->fops = &rtk_mipicsi_v4l2_fops;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	vdev->v4l2_dev = v4l2_dev;
	strscpy(vdev->name, RTK_MIPICSI_VIDEO_NAME, sizeof(vdev->name));
	vdev->vfl_type = VFL_TYPE_VIDEO;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->release = video_device_release_empty;
	vdev->ioctl_ops = &rtk_mipicsi_video_ioctls;
	vdev->lock = &mipicsi->video_lock;

	video_nr = rtk_mipicsi_get_video_nr(mipicsi);
	video_set_drvdata(vdev, mipicsi);
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, video_nr);
	if (ret) {
		dev_err(mipicsi->dev, "Failed to register video device\n");
		return ret;
	}

	dev_info(mipicsi->dev, "Registered %s as /dev/video%d\n",
		vdev->name, vdev->num);

	return 0;
}

static void rtk_mipicsi_recovery_reset(struct rtk_mipicsi *mipicsi)
{
	u8 entry;
	unsigned long flags;

	spin_lock_irqsave(&mipicsi->v4l2_dev.lock, flags);

	mipicsi->hw_ops->app_ctrl(mipicsi, mipicsi->ch_index, DISABLE);
	mipicsi->hw_ops->interrupt_ctrl(mipicsi, mipicsi->ch_index, DISABLE);

	mipicsi->hw_ops->app_size_cfg(mipicsi, mipicsi->ch_index);

	if (mipicsi->debug.en_colorbar)
		mipicsi->hw_ops->color_bar_test(mipicsi, ENABLE);

	mipicsi->hw_ops->crc_ctrl(mipicsi, ENABLE);
	mipicsi->hw_ops->meta_swap(mipicsi, ENABLE);

	if (mipicsi->conf->en_group_dev) {
		u8 ch;

		for (ch = 0; ch <= mipicsi->ch_max; ch++)
			for (entry = ENTRY_0; entry <= ENTRY_3; entry++)
				mipicsi->hw_ops->dma_buf_cfg(mipicsi,
					mipicsi->cur_dma_buf[ch][entry]->paddr,
					ch, entry);
	} else {
		for (entry = ENTRY_0; entry <= ENTRY_3; entry++)
			mipicsi->hw_ops->dma_buf_cfg(mipicsi,
				mipicsi->cur_buf[entry]->phy_addr,
				mipicsi->ch_index, entry);
	}

	mipicsi->hw_ops->interrupt_ctrl(mipicsi, mipicsi->ch_index, ENABLE);
	mipicsi->hw_ops->app_ctrl(mipicsi, mipicsi->ch_index, ENABLE);

	spin_unlock_irqrestore(&mipicsi->v4l2_dev.lock, flags);
}

static irqreturn_t rtk_mipicsi_irq_handler(int irq, void *dev_id)
{
	struct rtk_mipicsi *mipicsi = dev_id;
	int ret;
	u32 done_st = 0;
	u8 is_done;
	u8 entry_index;

	if (mipicsi->hw_ops == NULL)
		return IRQ_HANDLED;

	ret = mipicsi->hw_ops->get_intr_state(mipicsi, &done_st, mipicsi->ch_index);
	if (ret) {
		rtk_mipicsi_recovery_reset(mipicsi);
		return IRQ_HANDLED;
	}

	if (!done_st)
		return IRQ_HANDLED;

	for (entry_index = ENTRY_0; entry_index <= ENTRY_3; entry_index++) {
		struct rtk_mipicsi_buffer *rbuf;

		is_done = mipicsi->hw_ops->is_frame_done(done_st, mipicsi->ch_index, entry_index);

		if (!is_done)
			continue;

		mipicsi->hw_ops->dump_entry_state(mipicsi, mipicsi->ch_index, entry_index);
		mipicsi->hw_ops->clear_done_flag(mipicsi, mipicsi->ch_index, entry_index);
		rbuf = list_first_entry_or_null(&mipicsi->buffers, struct rtk_mipicsi_buffer, link);
		if (rbuf) {
			struct vb2_v4l2_buffer *vb;

			vb = &mipicsi->cur_buf[entry_index]->vb;
			vb->vb2_buf.timestamp = ktime_get_ns();
			vb->sequence = mipicsi->sequence++;
			vb->field = V4L2_FIELD_NONE;

			trace_mipicsi_buffer_done(vb->vb2_buf.index, mipicsi->ch_index,
				entry_index, vb->vb2_buf.timestamp/1000, vb->sequence);

			vb2_buffer_done(&vb->vb2_buf, VB2_BUF_STATE_DONE);

			rbuf->ch_index = mipicsi->ch_index;
			rbuf->entry_index = entry_index;
			rbuf->phy_addr = vb2_dma_contig_plane_dma_addr(&rbuf->vb.vb2_buf, 0);
			mipicsi->cur_buf[entry_index] = rbuf;
			list_del(&rbuf->link);
		} else {
			trace_mipicsi_skip_frame(mipicsi->ch_index, entry_index);
		}
		mipicsi->hw_ops->dma_buf_cfg(mipicsi,
			mipicsi->cur_buf[entry_index]->phy_addr, mipicsi->ch_index, entry_index);
	}

	return IRQ_HANDLED;
}

static irqreturn_t rtk_mipicsi_irq_handler_g(int irq, void *dev_id)
{
	struct rtk_mipicsi *mipicsi = dev_id;
	struct rtk_mipicsi_group_data *g_data = mipicsi->g_data;
	int ret;
	u32 done_st = 0;
	u8 ch_index;
	u8 is_done;
	u8 entry_index;

	if (mipicsi->hw_ops == NULL)
		return IRQ_HANDLED;

	ret = mipicsi->hw_ops->get_intr_state(mipicsi, &done_st, mipicsi->ch_max);
	ret = 0; // TODO: Enable reset mechanism
	if (ret) {
		rtk_mipicsi_recovery_reset(mipicsi);
		return IRQ_HANDLED;
	}

	if (!done_st)
		return IRQ_HANDLED;

	if (done_st)
		dev_dbg(mipicsi->dev, "TOP_0_INT_STS_SCPU_1=0x%08x\n", done_st);

	for (ch_index = 0; ch_index <= mipicsi->ch_max; ch_index++) {
		for (entry_index = ENTRY_0; entry_index <= ENTRY_3; entry_index++) {
			struct rtk_mipicsi_dma_bufs *dma_buf;

			is_done = mipicsi->hw_ops->is_frame_done(done_st, ch_index, entry_index);

			if (!is_done)
				continue;

			mipicsi->hw_ops->clear_done_flag(mipicsi, ch_index, entry_index);

			dma_buf = list_first_entry_or_null(&g_data->dma_list,
						struct rtk_mipicsi_dma_bufs, link);
			if (dma_buf) {
				dev_info(mipicsi->dev, "cur_dma_buf[%u][%u] phy_addr=0x%08lx sizeimage=%u\n",
					ch_index, entry_index,
					(unsigned long)mipicsi->cur_dma_buf[ch_index][entry_index]->paddr,
					mipicsi->pix_fmt.sizeimage);

				/* Already done, retrieve dma buf */
				if (g_data->done_flags & BIT(ch_index))
					list_add_tail(&g_data->done_dma_buf[ch_index]->link, &g_data->dma_list);

				g_data->done_dma_buf[ch_index] = mipicsi->cur_dma_buf[ch_index][entry_index];
				g_data->done_ts[ch_index] = refclk_get_val_raw();
				g_data->done_flags |= BIT(ch_index);

				mipicsi->cur_dma_buf[ch_index][entry_index] = dma_buf;
				list_del(&dma_buf->link);
			} else {
				trace_mipicsi_skip_frame(ch_index, entry_index);
			}

			mipicsi->hw_ops->dma_buf_cfg(mipicsi,
				mipicsi->cur_dma_buf[ch_index][entry_index]->paddr,
				ch_index, entry_index);

		}
	}

	if (g_data->done_flags == g_data->ready)
		wake_up_interruptible(&g_data->done_wait);

	return IRQ_HANDLED;
}

static int rtk_mipicsi_parse_clk_dt(struct rtk_mipicsi *mipicsi)
{
	int ret;
	struct device *dev = mipicsi->dev;
	struct rtk_mipicsi_crt crt;

	if ((mipicsi->ch_index != CH_0) && (!mipicsi->conf->en_group_dev))
		return 0;

	memset_io(&crt, 0, sizeof(struct rtk_mipicsi_crt));

	crt.reset_mipi = devm_reset_control_get_optional_exclusive(dev, "rstn_mipi_csi");
	if (IS_ERR(crt.reset_mipi))
		return dev_err_probe(dev, PTR_ERR(crt.reset_mipi),
					"Can't get reset_control rstn_mipi_csi\n");

	crt.clk_mipi = devm_clk_get(dev, "clk_en_mipi_csi");
	if (IS_ERR(crt.clk_mipi))
		return dev_err_probe(dev, PTR_ERR(crt.clk_mipi),
					"Can't get clk clk_en_mipi_csi\n");

	crt.clk_npu_mipi = devm_clk_get(dev, "clk_en_npu_mipi_csi");
	if (IS_ERR(crt.clk_npu_mipi))
		return dev_err_probe(dev, PTR_ERR(crt.clk_npu_mipi),
					"Can't get clk clk_en_npu_mipi_csi\n");

	crt.clk_npu_pll = devm_clk_get(dev, "clk_npu_mipi_csi");
	if (IS_ERR(crt.clk_npu_pll))
		return dev_err_probe(dev, PTR_ERR(crt.clk_npu_pll),
					"Can't get clk clk_npu_mipi_csi\n");

	ret = rtk_mipi_csi_save_crt(mipicsi, &crt);

	return 0;
}

static int rtk_mipicsi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *syscon_np;
	struct rtk_mipicsi *mipicsi;
	int ret;

	mipicsi = devm_kzalloc(dev, sizeof(*mipicsi), GFP_KERNEL);
	if (IS_ERR(mipicsi))
		return PTR_ERR(mipicsi);

	mipicsi->dev = dev;
	dev_set_drvdata(dev, mipicsi);

	dev_info(mipicsi->dev, "init begin\n");

	mipicsi->conf = of_device_get_match_data(dev);
	if (!mipicsi->conf) {
		ret = -EINVAL;
		goto err_exit;
	}

	dev_info(mipicsi->dev, "en_group_dev=%s\n",
		mipicsi->conf->en_group_dev ? "Y":"N");

	mutex_init(&mipicsi->video_lock);
	mutex_init(&mipicsi->buffer_lock);
	INIT_LIST_HEAD(&mipicsi->buffers);

	ret = of_property_read_u32(dev->of_node, "ch-index",
			&mipicsi->ch_index);
	if (ret < 0 || mipicsi->ch_index > 5) {
		ret = -ENODEV;
		goto err_exit;
	}

	if (mipicsi->conf->en_group_dev) {
		dev_info(mipicsi->dev, "ch_max=%u\n", mipicsi->ch_max);
		mipicsi->g_data = devm_kzalloc(dev, sizeof(*mipicsi->g_data), GFP_KERNEL);
		if (IS_ERR(mipicsi->g_data))
			return PTR_ERR(mipicsi->g_data);
	} else {
		dev_info(mipicsi->dev, "ch_index=%u\n", mipicsi->ch_index);
	}

	ret = of_property_read_u32(dev->of_node, "src-width",
				&mipicsi->src_width);
	if (ret < 0 || mipicsi->src_width < MIPICSI_MIN_WIDTH)
		mipicsi->src_width = MIPICSI_MAX_WIDTH;

	ret = of_property_read_u32(dev->of_node, "src-height",
			&mipicsi->src_height);
	if (ret < 0 || mipicsi->src_height < MIPICSI_MIN_HEIGHT)
		mipicsi->src_height = MIPICSI_MAX_HEIGHT;

	ret = of_property_read_u32(dev->of_node, "skew-mode",
				&mipicsi->skew_mode);
	if (ret < 0 || mipicsi->skew_mode > SKEW_AUTO)
		mipicsi->skew_mode = SKEW_DIS;

	ret = of_property_read_u32(dev->of_node, "mirror-mode",
				&mipicsi->mirror_mode);
	if (ret < 0 || mipicsi->mirror_mode > MIRROR_EN)
		mipicsi->mirror_mode = MIRROR_DIS;

	dev_info(dev, "src_width=%u src_height=%u\n",
		mipicsi->src_width, mipicsi->src_height);
	dev_info(dev, "skew_mode=%u mirror_mode=%u\n",
		mipicsi->skew_mode, mipicsi->mirror_mode);

	ret = rtk_mipicsi_parse_clk_dt(mipicsi);
	/* return dev_err_probe */
	if (ret)
		return ret;

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 0 fail");
		ret = -ENODEV;
		goto err_exit;
	}

	mipicsi->topreg = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(mipicsi->topreg)) {
		dev_err(dev, "Remap syscon 0 to topreg fail");
		of_node_put(syscon_np);
		ret = PTR_ERR(mipicsi->topreg);
		goto err_exit;
	}

	mipicsi->appreg = mipicsi->topreg;

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 1);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 1 fail");
		ret = -ENODEV;
		goto err_exit;
	}

	mipicsi->phyreg = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(mipicsi->phyreg)) {
		dev_err(dev, "Remap syscon 1 to phyreg fail");
		of_node_put(syscon_np);
		ret = PTR_ERR(mipicsi->phyreg);
		goto err_exit;
	}

	mipicsi->irq = irq_of_parse_and_map(dev->of_node, 0);
	if (!mipicsi->irq) {
		dev_err(dev, "Fail to get irq");
		ret = -ENODEV;
		goto err_exit;
	}

	dev_info(mipicsi->dev, "irq=%d\n", mipicsi->irq);

	ret = rtk_video_setup_video(mipicsi);
	if (ret)
		goto err_exit;

	if (mipicsi->conf->en_group_dev)
		ret = devm_request_irq(dev, mipicsi->irq, rtk_mipicsi_irq_handler_g,
				IRQF_SHARED, dev_name(dev), mipicsi);
	else
		ret = devm_request_irq(dev, mipicsi->irq, rtk_mipicsi_irq_handler,
				IRQF_SHARED, dev_name(dev), mipicsi);
	if (ret) {
		dev_err(dev, "can't request mipicsi irq %d\n", mipicsi->irq);
		goto err_exit;
	}

	if (IS_ENABLED(CONFIG_DEBUG_FS))
		rtk_mipicsi_setup_dbgfs(mipicsi);

	dev_info(mipicsi->dev, "init done\n");

	return 0;

err_exit:
	return ret;
}

static int rtk_mipicsi_remove(struct platform_device *pdev)
{
	// TODO:
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int rtk_mipicsi_suspend(struct device *dev)
{
	int ret;
	struct rtk_mipicsi *mipicsi = dev_get_drvdata(dev);

	ret = rtk_mipi_csi_hw_deinit(mipicsi);

	return ret;
}

static int rtk_mipicsi_resume(struct device *dev)
{
	int ret;
	struct rtk_mipicsi *mipicsi = dev_get_drvdata(dev);

	ret = rtk_mipi_csi_hw_init(mipicsi);

	return ret;
}

static const struct dev_pm_ops rtk_mipicsi_pm_ops = {
	.suspend = rtk_mipicsi_suspend,
	.resume = rtk_mipicsi_resume,
	.freeze = rtk_mipicsi_suspend,
	.thaw = rtk_mipicsi_resume,
	.restore = rtk_mipicsi_resume,
};
#endif

static void rtk_mipicsi_shutdown(struct platform_device *pdev)
{
	struct rtk_mipicsi *mipicsi = dev_get_drvdata(&pdev->dev);

	rtk_mipi_csi_hw_deinit(mipicsi);
}

static const struct rtk_mipicsi_conf conf_default = {
	.en_group_dev = false,
};

static const struct rtk_mipicsi_conf conf_car = {
	.en_group_dev = true,
};

static const struct of_device_id rtk_mipicsi_match[] = {
	{ .compatible = "realtek,rtk-mipi-csi",
	  .data = &conf_default, },
	{ .compatible = "realtek,rtk-mipi-csi-group",
	  .data = &conf_car, },
	{},
};

MODULE_DEVICE_TABLE(of, rtk_mipicsi_match);

static struct platform_driver rtk_mipicsi_driver = {
	.driver = {
		.name = "rtk-mipi-csi",
		.owner = THIS_MODULE,
		.of_match_table = rtk_mipicsi_match,
#if IS_ENABLED(CONFIG_PM)
		.pm = &rtk_mipicsi_pm_ops,
#endif
	},
	.probe = rtk_mipicsi_probe,
	.remove = rtk_mipicsi_remove,
	.shutdown = rtk_mipicsi_shutdown,
};

module_platform_driver(rtk_mipicsi_driver);


MODULE_AUTHOR("Chase Yen <chase.yen@realtek.com>");
MODULE_DESCRIPTION("REALTEK MIPI CSI Driver");
MODULE_LICENSE("GPL v2");
