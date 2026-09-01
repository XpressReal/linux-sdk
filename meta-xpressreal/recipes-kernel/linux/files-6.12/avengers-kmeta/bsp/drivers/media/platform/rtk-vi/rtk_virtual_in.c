// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/videodev2.h>
#include <linux/dma-map-ops.h>
#include <linux/mm_types.h>
#include <linux/fdtable.h>

#include <linux/fs.h>
#include <linux/file.h>
#include <asm/uaccess.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include <soc/realtek/rtk_refclk.h>

#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>

#define RTK_METADA_BUF_MODE  1
#define RTK_SKIP_OLD_FRAME   0
#define RTK_VIRTUAL_INPUT    1

#define RTK_VI_VIDEO_NAME    "rtk-vi"
#define MAX_WIDTH   1920
#define MAX_HEIGHT  1080
#define MIN_WIDTH	720
#define MIN_HEIGHT	480

#define FIXED_WIDTH   1920
#define FIXED_HEIGHT  1080

#define MAX_DMA_BUFS 8

/**
 * struct rtk_vi_dma_bufs
 *
 * @size: allocated size
 * @vaddr:  Virtual address
 * @paddr: Physical address
 */
struct rtk_vi_dma_bufs {
	struct device *dev;
	u32 index;
	size_t size;
	void *vaddr;
	dma_addr_t paddr;
	struct dma_buf *dmabuf;
	int fd;
};

#if RTK_METADA_BUF_MODE
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
}__attribute__ ((packed));

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
}__attribute__ ((packed));
#endif

#define to_rtk_vi(x) container_of(x, struct rtk_vi, x)

struct rtk_vi {
	struct regmap *vi_reg;

	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	struct vb2_queue queue;
	struct v4l2_pix_format pix_fmt;
	struct v4l2_bt_timings active_timings;
	struct v4l2_bt_timings detected_timings;
	unsigned int v4l2_input_status;
	struct mutex video_lock;

	struct rtk_vi_dma_bufs dma_bufs[MAX_DMA_BUFS];

	struct list_head buffers;
	struct mutex buffer_lock; /* buffer list lock */
	unsigned int sequence;

#if RTK_VIRTUAL_INPUT
	bool en_vir_in;
	struct work_struct copy_work;
	struct timer_list vir_in_timer;
#endif
};

struct rtk_vi_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head link;
	// TODO: more private info?
};

#define to_vi_buffer(buf)	container_of(buf, struct rtk_vi_buffer, vb)

struct rtk_vi_fmt {
	unsigned int fourcc;
};

static const struct rtk_vi_fmt rtk_vi_fmt_list[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV12,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV16,
	},
};

#define NUM_FORMATS ARRAY_SIZE(rtk_vi_fmt_list)

static const struct v4l2_dv_timings_cap rtk_vi_timings_cap = {
	.type = V4L2_DV_BT_656_1120,
	.bt = {
		.min_width = MIN_WIDTH,
		.max_width = MAX_WIDTH,
		.min_height = MIN_HEIGHT,
		.max_height = MAX_HEIGHT,
		.min_pixelclock = 6574080, /* 640 x 480 x 24Hz */
		.max_pixelclock = 124416000, /* 1920 x 1080 x 60Hz */
		.standards = V4L2_DV_BT_STD_CEA861,
		.capabilities = V4L2_DV_BT_CAP_INTERLACED |
				V4L2_DV_BT_CAP_PROGRESSIVE,
	},
};

static void hexdump(char *note, unsigned char *buf, unsigned int len)
{
	printk(KERN_CRIT "%s", note);
	print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET,
		16, 1,
		buf, len, false);
}


#if RTK_VIRTUAL_INPUT
static void rtk_vi_copy_worker(struct work_struct *copy_work)
{
	struct rtk_vi *vi;
	struct rtk_vi_buffer *buf;
	void *addr;
	unsigned char buf_index;
	u64 time_ns;
#if RTK_METADA_BUF_MODE
	struct rtk_meta_buf *meta_buf;
#endif

	vi = to_rtk_vi(copy_work);

	mutex_lock(&vi->buffer_lock);

	if (!vi->en_vir_in) {
		mutex_unlock(&vi->buffer_lock);
		return;
	}

	buf = list_first_entry_or_null(&vi->buffers, struct rtk_vi_buffer, link);
	if (!buf) {
		mutex_unlock(&vi->buffer_lock);
		dev_info(vi->dev, "skip frame\n");
		return;
	}

	buf_index = vi->sequence % 4;
	addr = vb2_plane_vaddr(&buf->vb.vb2_buf, 0);

#if RTK_METADA_BUF_MODE
	meta_buf = (struct rtk_meta_buf *)addr;
	meta_buf->id = METADA_ID;
	meta_buf->valid_ch = 1;
	meta_buf->mode = 0;
	meta_buf->fd[0] = vi->dma_bufs[buf_index].fd;
	meta_buf->buf_size = vi->dma_bufs[buf_index].size;
	meta_buf->done_ts[0] = refclk_get_val_raw();
	meta_buf->start_ts[0] = 135;
	meta_buf->end_ts[0] = 246;
	meta_buf->crc[0] = 0xABCDEFEF;
	meta_buf->frame_cnt[0] = vi->sequence;
	dev_info(vi->dev, "buf%u phy_addr=0x%08lx fd=%d\n",
		buf_index, (unsigned long)vi->dma_bufs[buf_index].paddr, vi->dma_bufs[buf_index].fd);
#else
	dev_info(vi->dev, "Copy buf%u to 0x%08lx\n", buf_index, (unsigned long)addr);
	memcpy(addr, vi->dma_bufs[buf_index].vaddr, vi->dma_bufs[buf_index].size);
#endif

	time_ns = ktime_get_ns();

	vb2_set_plane_payload(&buf->vb.vb2_buf, 0, vi->dma_bufs[buf_index].size);
	buf->vb.vb2_buf.timestamp = time_ns;
	buf->vb.sequence = vi->sequence++;
	buf->vb.field = V4L2_FIELD_NONE;

	dev_info(vi->dev, "VB2 done,sequence=%u timestamp(ns)=%llu\n",
		buf->vb.sequence, time_ns);

	vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
	list_del(&buf->link);
	mutex_unlock(&vi->buffer_lock);
}

static void rtk_vir_in_timer_cb(struct timer_list *vir_in_timer)
{
	struct rtk_vi *vi;

	vi = to_rtk_vi(vir_in_timer);

	if (!vi->en_vir_in)
		return;

	schedule_work(&vi->copy_work);

	mod_timer(vir_in_timer, jiffies + msecs_to_jiffies(33));
}

static int read_video_data_file(struct rtk_vi *vi, unsigned char buf_index, char *file_path)
{
	struct file *fp;	

	fp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		dev_err(vi->dev, "Open %s failed\n", file_path);
		return -ENODEV;
	}

	dev_info(vi->dev, "Open file %s success\n", file_path);

	dev_info(vi->dev, "Read file to dma_bufs[%u].vaddr 0x%08lx, size %zu\n",
		buf_index,
		(unsigned long)vi->dma_bufs[buf_index].vaddr, vi->dma_bufs[buf_index].size);

	kernel_read(fp, vi->dma_bufs[buf_index].vaddr,
		vi->dma_bufs[buf_index].size, &fp->f_pos);

	filp_close(fp, NULL);

	/* Dump first 512 bytes */
	hexdump("Dump dma_bufs\n", vi->dma_bufs[buf_index].vaddr, 512);
	return 0;
}
#endif

struct rtkvi_dma_buf_attachment {
	struct sg_table sgt;
};

static int rtkvi_dma_buf_attach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attach)
{
	struct rtk_vi_dma_bufs *buf = dmabuf->priv;
	struct device *dev = buf->dev;
	dma_addr_t paddr = buf->paddr;
	void *vaddr = buf->vaddr;
	size_t size = dmabuf->size;

	struct rtkvi_dma_buf_attachment *a;
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

static void rtkvi_dma_buf_detatch(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attach)
{
	struct rtkvi_dma_buf_attachment *a = attach->priv;

	sg_free_table(&a->sgt);
	kfree(a);
}

static struct sg_table *rtkvi_map_dma_buf(struct dma_buf_attachment *attach,
                                        enum dma_data_direction dir)
{
	struct rtkvi_dma_buf_attachment *a = attach->priv;
	struct sg_table *table;
	int ret;

	table = &a->sgt;

	ret = dma_map_sgtable(attach->dev, table, dir, 0);
	if (ret)
		table = ERR_PTR(ret);

	return table;
}

static void rtkvi_unmap_dma_buf(struct dma_buf_attachment *attach,
                                struct sg_table *table,
                                enum dma_data_direction dir)
{
	dma_unmap_sgtable(attach->dev, table, dir, 0);
}

static int rtkvi_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct rtk_vi_dma_bufs *buf = dmabuf->priv;
	struct device *dev = buf->dev;
	dma_addr_t paddr = buf->paddr;
	void *vaddr = buf->vaddr;
	size_t size = vma->vm_end - vma->vm_start;

	if (vaddr)
		return dma_mmap_coherent(dev, vma, vaddr, paddr, size);

	return 0;
}

static void rtkvi_release(struct dma_buf *dmabuf)
{
	struct rtk_vi_dma_bufs *buf = dmabuf->priv;
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

static const struct dma_buf_ops rtkvi_dma_buf_ops = {
	.attach = rtkvi_dma_buf_attach,
	.detach = rtkvi_dma_buf_detatch,
	.map_dma_buf = rtkvi_map_dma_buf,
	.unmap_dma_buf = rtkvi_unmap_dma_buf,
	.mmap = rtkvi_mmap,
	.release = rtkvi_release,
};

static int rtk_vi_export_dma_buf(struct rtk_vi *vi, u32 buf_index, size_t buf_size)
{
	struct dma_buf *dmabuf;
	int fd;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &rtkvi_dma_buf_ops;
	exp_info.size = buf_size;
	exp_info.flags = O_RDWR;
	exp_info.priv = &vi->dma_bufs[buf_index];
	dmabuf = dma_buf_export(&exp_info);

	if (IS_ERR(dmabuf)) {
		dev_err(vi->dev, "%s export fail, buf_index=%u\n",
				__func__, buf_index);

		return PTR_ERR(dmabuf);
	}

	vi->dma_bufs[buf_index].dmabuf = dmabuf;

	fd = dma_buf_fd(dmabuf, exp_info.flags);
	if (fd < 0) {
		dev_err(vi->dev, "%s dmabuf to fd fail\n", __func__);
		dma_buf_put(dmabuf);
		return -ENOMEM;
	}

	dev_info(vi->dev, "dma_bufs[%u].fd=%d\n", buf_index, fd);
	vi->dma_bufs[buf_index].fd = fd;

	return 0;
}

static int rtk_vi_alloc_dma_bufs(struct rtk_vi *vi, size_t buf_size)
{
	int i, j;
	int ret;

	if (vi->dma_bufs[0].size) {
		dev_err(vi->dev, "%s fail, user space didn't release previous dma_bufs\n",
			 __func__);
		return -EBUSY;
	}

	for (i = 0; i < MAX_DMA_BUFS; i++) {
		vi->dma_bufs[i].vaddr = dma_alloc_coherent(vi->dev, buf_size,
					&vi->dma_bufs[i].paddr, GFP_KERNEL);

		vi->dma_bufs[i].dev = vi->dev;
		vi->dma_bufs[i].index = i;
		vi->dma_bufs[i].size = buf_size;

		if (vi->dma_bufs[i].vaddr == NULL) {
			for (j = 0; j < i; j++) {
				dma_free_coherent(vi->dev, buf_size,
					vi->dma_bufs[j].vaddr, vi->dma_bufs[j].paddr);
				vi->dma_bufs[j].size = 0;
			}
			return -ENOMEM;
		}

		dev_info(vi->dev, "alloc dma addr 0x%08lx at 0x%08lx, size %zu\n",
			(unsigned long)vi->dma_bufs[i].paddr,
			(unsigned long)vi->dma_bufs[i].vaddr, buf_size);
	}

	for (i = 0; i < MAX_DMA_BUFS; i++) {
		ret = rtk_vi_export_dma_buf(vi, i, buf_size);
		if (ret)
			return ret;
	}

	return 0;
}

static int rtk_vi_dma_init(struct rtk_vi *vi)
{
	rheap_setup_dma_pools(vi->dev, NULL,
			RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_HWIPACC, __func__);

	return 0;
}

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

static int rtk_vi_video_set_timing(struct rtk_vi *vi,
				     struct v4l2_bt_timings *timing)
{
	dev_info(vi->dev, "%s width(%u) height(%u)\n",
		__func__, timing->width, timing->height);

	// TODO: regmap read/write

	return 0;
}

static void rtk_vi_video_detect_timing(struct rtk_vi *vi)
{
	struct v4l2_bt_timings *det = &vi->detected_timings;

	dev_info(vi->dev, "%s\n", __func__);

	vi->v4l2_input_status = V4L2_IN_ST_NO_SIGNAL;
	// TODO: regmap read/write

	det->width = FIXED_WIDTH;
	det->height = FIXED_HEIGHT;

	if (det->width && det->height)
		vi->v4l2_input_status = 0;
}

static int vi_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers, unsigned int *num_planes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct rtk_vi *vi = vb2_get_drv_priv(q);
	int ret;

	dev_info(vi->dev, "%s\n", __func__);

	ret = rtk_vi_alloc_dma_bufs(vi, vi->pix_fmt.sizeimage);
	if (ret)
		return ret;

#if RTK_VIRTUAL_INPUT
	read_video_data_file(vi, 0, "/mnt/usb/vi_test_pattern/ColorBar_1920x1080.yuv");
	read_video_data_file(vi, 1, "/mnt/usb/vi_test_pattern/ColorBar2_1920x1080.yuv");
	read_video_data_file(vi, 2, "/mnt/usb/vi_test_pattern/ColorBar_1920x1080.yuv");
	read_video_data_file(vi, 3, "/mnt/usb/vi_test_pattern/ColorBar2_1920x1080.yuv");
#endif

	*num_planes = 1;

#if RTK_METADA_BUF_MODE
	sizes[0] = 4096;
#else
	sizes[0] = vi->pix_fmt.sizeimage;
#endif
	// TODO:
	return 0;
}

static int vi_buf_prepare(struct vb2_buffer *vb)
{
	struct rtk_vi *vi = vb2_get_drv_priv(vb->vb2_queue);

	dev_info(vi->dev, "%s index=%u\n", __func__, vb->index);
	// TODO:
	return 0;
}

static void vi_buf_finish(struct vb2_buffer *vb)
{
	struct rtk_vi *vi = vb2_get_drv_priv(vb->vb2_queue);

	dev_info(vi->dev, "%s\n", __func__);
	// TODO:
}

static int vi_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct rtk_vi *vi = vb2_get_drv_priv(q);

	dev_info(vi->dev, "%s\n", __func__);

	vi->sequence = 0;
	// TODO:

#if RTK_VIRTUAL_INPUT
	dev_info(vi->dev, "Enable virtual input\n");
	vi->en_vir_in = true;
	mod_timer(&vi->vir_in_timer, jiffies + msecs_to_jiffies(33));
#endif

	return 0;
}

static void vi_stop_streaming(struct vb2_queue *q)
{
	struct rtk_vi *vi = vb2_get_drv_priv(q);
	struct rtk_vi_buffer *buf;

	dev_info(vi->dev, "%s\n", __func__);

#if RTK_VIRTUAL_INPUT
	dev_info(vi->dev, "Disable virtual input\n");
	vi->en_vir_in = false;
	cancel_work_sync(&vi->copy_work);
#endif

	// TODO:
	mutex_lock(&vi->buffer_lock);

	list_for_each_entry(buf, &vi->buffers, link)
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);

	INIT_LIST_HEAD(&vi->buffers);

	mutex_unlock(&vi->buffer_lock);
}

static void vi_buf_queue(struct vb2_buffer *vb)
{
	struct rtk_vi *vi = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rtk_vi_buffer *rvb = to_vi_buffer(vbuf);

	dev_info(vi->dev, "%s index=%u\n", __func__, vb->index);

	mutex_lock(&vi->buffer_lock);
	list_add_tail(&rvb->link, &vi->buffers);
	mutex_unlock(&vi->buffer_lock);

	// TODO: 
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

	dev_info(vi->dev, "%s index(%u)\n", __func__, f->index);

	fmt = &rtk_vi_fmt_list[f->index];

	f->pixelformat = fmt->fourcc;

	return 0;
}

static int rtk_vi_video_get_format(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct rtk_vi *vi = video_drvdata(file);
	u32 size;

	dev_info(vi->dev, "%s\n", __func__);

	// TODO:
	vi->pix_fmt.field = V4L2_FIELD_SEQ_TB;
	vi->pix_fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
	vi->pix_fmt.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	vi->pix_fmt.width = FIXED_WIDTH;
	vi->pix_fmt.height = FIXED_HEIGHT;

	// TODO: NV16 size
	size = roundup(vi->pix_fmt.width, 16) * vi->pix_fmt.height;
	vi->pix_fmt.sizeimage = roundup(size + (size >> 1), 4096); /* dma 4K align */

	f->fmt.pix = vi->pix_fmt;

	return 0;
}

static int rtk_vi_video_try_format(struct file *file, void *fh,
				 struct v4l2_format *f)
{
	struct rtk_vi *vi = video_drvdata(file);
	const struct rtk_vi_fmt *fmt;

	dev_info(vi->dev, "%s\n", __func__);

	fmt = rtk_vi_find_format(f);
	if (!fmt)
		f->fmt.pix.pixelformat = rtk_vi_fmt_list[0].fourcc;

	// TODO: Progressive case for pix.field
	f->fmt.pix.field = V4L2_FIELD_SEQ_TB;
	f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	f->fmt.pix.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	f->fmt.pix.width = vi->pix_fmt.width;
	f->fmt.pix.height = vi->pix_fmt.height;

	return 0;
}

static int rtk_vi_video_set_format(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct rtk_vi *vi = video_drvdata(file);
	int ret;

	dev_info(vi->dev, "%s\n", __func__);

	ret = rtk_vi_video_try_format(file, fh, f);
	if (ret)
		return ret;

	if (vb2_is_busy(&vi->queue)) {
		dev_err(vi->dev, "%s device busy\n", __func__);
		return -EBUSY;
	}

	vi->pix_fmt.pixelformat = f->fmt.pix.pixelformat;

	return 0;
}

#if RTK_SKIP_OLD_FRAME
int rtk_vb2_ioctl_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct video_device *vdev = video_devdata(file);
	struct rtk_vi *vi = video_drvdata(file);

	dev_info(vi->dev, "%s\n", __func__);

	return vb2_dqbuf(vdev->queue, p, file->f_flags & O_NONBLOCK);
}
#endif

static int rtk_vi_video_set_dv_timings(struct file *file, void *fh,
				     struct v4l2_dv_timings *timings)
{
	struct rtk_vi *vi = video_drvdata(file);
	int ret;

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

	// TODO:
	exist = v4l2_find_dv_timings_cea861_vic(&t, 21);
	if (exist)
		memcpy(&vi->active_timings, &t.bt, sizeof(t.bt));

	dev_info(vi->dev, "%s width=%u height=%u\n", __func__,
		vi->active_timings.width, vi->active_timings.height);

	timings->type = V4L2_DV_BT_656_1120;
	timings->bt = vi->active_timings;

	return 0;
}

static int rtk_vi_video_query_dv_timings(struct file *file, void *fh,
				       struct v4l2_dv_timings *timings)
{
	struct rtk_vi *vi = video_drvdata(file);

	dev_info(vi->dev, "%s\n", __func__);

	rtk_vi_video_detect_timing(vi);
	timings->type = V4L2_DV_BT_656_1120;
	timings->bt = vi->detected_timings;

	return vi->v4l2_input_status ? -ENOLINK : 0;
}

static int rtk_vi_video_enum_dv_timings(struct file *file, void *fh,
				      struct v4l2_enum_dv_timings *timings)
{
	return v4l2_enum_dv_timings_cap(timings, &rtk_vi_timings_cap,
					NULL, NULL);
}

static int rtk_vi_video_dv_timings_cap(struct file *file, void *fh,
				     struct v4l2_dv_timings_cap *cap)
{
	*cap = rtk_vi_timings_cap;

	return 0;
}

static int rtk_vi_video_g_parm(struct file *file, void *fh,
				struct v4l2_streamparm *a)
{
	struct rtk_vi *vi = video_drvdata(file);
	struct rtk_streamparm_fd *param_fd;
	u32 i;

	dev_info(vi->dev, "%s type=%u\n", __func__, a->type);

	param_fd = (struct rtk_streamparm_fd *)&a->parm.raw_data[0];
	memset_io(param_fd, 0, sizeof(struct rtk_streamparm_fd));

	param_fd->header[0] = 'r';
	param_fd->header[1] = 'f';
	param_fd->header[2] = 'd';
	param_fd->size = MAX_DMA_BUFS;

	for (i = 0; i < MAX_DMA_BUFS; i++)
		if (vi->dma_bufs[i].size != 0)
			param_fd->fd_list[i] = vi->dma_bufs[i].fd;

	return 0;
}

static const struct v4l2_ioctl_ops rtk_vi_video_ioctls = {
	.vidioc_querycap = rtk_vi_video_querycap,

	.vidioc_enum_fmt_vid_cap = rtk_vi_video_enum_format,
	.vidioc_g_fmt_vid_cap = rtk_vi_video_get_format,
	.vidioc_s_fmt_vid_cap = rtk_vi_video_set_format,
	.vidioc_try_fmt_vid_cap = rtk_vi_video_try_format,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
#if RTK_SKIP_OLD_FRAME
	.vidioc_dqbuf = rtk_vb2_ioctl_dqbuf,
#else
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
#endif
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,

	.vidioc_s_dv_timings = rtk_vi_video_set_dv_timings,
	.vidioc_g_dv_timings = rtk_vi_video_get_dv_timings,
	.vidioc_query_dv_timings = rtk_vi_video_query_dv_timings,
	.vidioc_enum_dv_timings = rtk_vi_video_enum_dv_timings,
	.vidioc_dv_timings_cap = rtk_vi_video_dv_timings_cap,

	.vidioc_g_parm = rtk_vi_video_g_parm,
	// TODO: 
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

static int rtk_video_setup_video(struct rtk_vi *vi)
{
	struct v4l2_device *v4l2_dev = &vi->v4l2_dev;
	struct video_device *vdev = &vi->vdev;
	struct vb2_queue *vbq = &vi->queue;
	int ret;

	vi->pix_fmt.pixelformat = V4L2_PIX_FMT_NV12;
	vi->pix_fmt.field = V4L2_FIELD_SEQ_TB;
	vi->pix_fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
	vi->pix_fmt.quantization = V4L2_QUANTIZATION_LIM_RANGE;
	vi->v4l2_input_status = V4L2_IN_ST_NO_SIGNAL;

	ret= v4l2_device_register(vi->dev, v4l2_dev);
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
	vbq->min_buffers_needed = 4;

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

	video_set_drvdata(vdev, vi);
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, -1);
	if (ret) {
		dev_err(vi->dev, "Failed to register video device\n");
		return ret;
	}

	dev_info(vi->dev, "Registered %s as /dev/video%d\n",
		vdev->name, vdev->num);

	return 0;
}

static int rtk_vi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *syscon_np;
	struct rtk_vi *vi;
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

	ret = rtk_vi_dma_init(vi);
	if (ret)
		goto err_exit;

	ret = rtk_video_setup_video(vi);
	if (ret)
		goto err_exit;

#if RTK_VIRTUAL_INPUT
	INIT_WORK(&vi->copy_work, rtk_vi_copy_worker);
	timer_setup(&vi->vir_in_timer, rtk_vir_in_timer_cb, 0);
#endif

	dev_info(vi->dev, "init done\n");

	return 0;

err_exit:
	return ret;
}

static int rtk_vi_remove(struct platform_device *pdev)
{
	// TODO:
	return 0;
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
	},
	.probe = rtk_vi_probe,
	.remove = rtk_vi_remove,
};

module_platform_driver(rtk_vi_driver);


MODULE_AUTHOR("Chase Yen <chase.yen@realtek.com>");
MODULE_DESCRIPTION("REALTEK Video Input Driver");
MODULE_LICENSE("GPL v2");
