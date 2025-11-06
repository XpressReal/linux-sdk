// SPDX-License-Identifier: GPL-2.0-only
#define pr_fmt(fmt)        KBUILD_MODNAME ": " fmt

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include "hse.h"
#include "uapi/hse.h"

enum HSE_FMT_TYPE {
	HSE_FMT_TYPE_YUV,
	HSE_FMT_TYPE_RGB,
	HSE_FMT_TYPE_INDEX,
};

enum HSE_DEV_COLOR_FMT {
	HSE_DEV_FMT_INDEX = 0,
	HSE_DEV_FMT_RGB565 = 0,
	HSE_DEV_FMT_RGB888 = 1,
	HSE_DEV_FMT_ARGB888 = 2,
	HSE_DEV_FMT_YUV422 = 3,
	HSE_DEV_FMT_YUV420 = 4,
};

enum HSE_DEV_RGB_ORDER {
	HSE_RGB_ORDER_RGB_ARGB,
	HSE_RGB_ORDER_RBG_ARBG,
	HSE_RGB_ORDER_BRG_ABRG,
	HSE_RGB_ORDER_BGR_ABGR,
	HSE_RGB_ORDER_GRB_AGRB,
	HSE_RGB_ORDER_GBR_AGBR,
	HSE_RGB_ORDER_RAGB,
	HSE_RGB_ORDER_RABG,
	HSE_RGB_ORDER_BARG,
	HSE_RGB_ORDER_BAGR,
	HSE_RGB_ORDER_GARB,
	HSE_RGB_ORDER_GABR,
	HSE_RGB_ORDER_RGAB,
	HSE_RGB_ORDER_RBAG,
	HSE_RGB_ORDER_BRAG,
	HSE_RGB_ORDER_BGAR,
	HSE_RGB_ORDER_GRAB,
	HSE_RGB_ORDER_GBAR,
	HSE_RGB_ORDER_RGBA,
	HSE_RGB_ORDER_RBGA,
	HSE_RGB_ORDER_BRGA,
	HSE_RGB_ORDER_BGRA,
	HSE_RGB_ORDER_GRBA,
	HSE_RGB_ORDER_GBRA,
};

struct hse_dev_color_fmt {
	enum HSE_COLOR_FMT fmt;
	enum HSE_DEV_COLOR_FMT dev_fmt;
	enum HSE_DEV_RGB_ORDER order;
	enum HSE_FMT_TYPE type;
};

struct hse_dev_file_data {
	struct hse_device *hse_dev;
	struct hse_engine *eng;
	struct hse_command_queue *cq;
	struct rb_root buf_root;
	struct mutex buf_lock;
};

struct hse_dev_buf {
	struct rb_node rb_node;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	u32 pfn;
};

static struct hse_dev_color_fmt fmt_map_tbl[] = {
	{ HSE_FMT_YUV420SP, HSE_DEV_FMT_YUV420, 0, HSE_FMT_TYPE_YUV },
	{ HSE_FMT_YUV422SP, HSE_DEV_FMT_YUV422, 0, HSE_FMT_TYPE_YUV },
	{ HSE_FMT_ARGB, HSE_DEV_FMT_ARGB888, HSE_RGB_ORDER_RGB_ARGB,
	  HSE_FMT_TYPE_RGB },
	{ HSE_FMT_BGRA, HSE_DEV_FMT_ARGB888, HSE_RGB_ORDER_BGRA,
	  HSE_FMT_TYPE_RGB },
	{ HSE_FMT_RGB888, HSE_DEV_FMT_RGB888, HSE_RGB_ORDER_RGB_ARGB,
	  HSE_FMT_TYPE_RGB },
};

#define fmt_map_tbl_size ARRAY_SIZE(fmt_map_tbl)

static inline bool buf_is_contiguous(struct hse_dev_buf *buf)
{
	return buf->sgt->nents == 1;
}

static inline dma_addr_t buf_dma_addr(struct hse_dev_buf *buf)
{
	return sg_dma_address(buf->sgt->sgl);
}

static inline unsigned int buf_len(struct hse_dev_buf *buf)
{
	return buf->attach->dmabuf->size;
}

static int hse_dev_buf_check_perm(struct hse_dev_buf *buf, unsigned int f_flags)
{
	struct file *file = buf->attach->dmabuf->file;
	unsigned int flags;

	if (!file)
		return -EBADF;

	flags = file->f_flags & O_ACCMODE;

	if (flags == O_RDWR)
		return 0;

	return f_flags == flags ? 0 : -EACCES;
}

static int hse_dev_buf_check_range(struct hse_dev_buf *buf, u32 offset, u32 size)
{
	if (check_add_overflow(offset, size, &size))
		return -EOVERFLOW;
	return buf_len(buf) < size ? -ENOBUFS : 0;
}

static int hse_dev_buf_va_is_less(struct rb_node *_a, struct rb_node *_b)
{
	struct hse_dev_buf *a = rb_entry(_a, struct hse_dev_buf, rb_node);
	struct hse_dev_buf *b = rb_entry(_b, struct hse_dev_buf, rb_node);

	return a->pfn < b->pfn;
}

static void hse_dev_buf_rbtree_add(struct hse_dev_file_data *fdata, struct hse_dev_buf *buf)
{
	struct rb_root *tree = &fdata->buf_root;
	struct rb_node **link = &tree->rb_node;
	struct rb_node *node = &buf->rb_node;
	struct rb_node *parent = NULL;

	mutex_lock(&fdata->buf_lock);

	while (*link) {
		parent = *link;
		if (hse_dev_buf_va_is_less(node, parent))
			link = &parent->rb_left;
		else
			link = &parent->rb_right;
	}

	rb_link_node(node, parent, link);
	rb_insert_color(node, tree);

	mutex_unlock(&fdata->buf_lock);
}

static struct hse_dev_buf *__hse_dev_buf_rbtree_find(struct hse_dev_file_data *fdata, u32 pfn)
{
	struct rb_root *tree = &fdata->buf_root;
	struct rb_node *node = tree->rb_node;

	while (node) {
		struct hse_dev_buf *buf = rb_entry(node, struct hse_dev_buf, rb_node);

		if (buf->pfn == pfn)
			return buf;
		else if (buf->pfn < pfn)
			node = node->rb_right;
		else
			node = node->rb_left;
	}

	return NULL;
}

static struct hse_dev_buf *hse_dev_buf_rbtree_find(struct hse_dev_file_data *fdata, u32 pfn)
{
	struct hse_dev_buf *buf;

	mutex_lock(&fdata->buf_lock);
	buf = __hse_dev_buf_rbtree_find(fdata, pfn);
	mutex_unlock(&fdata->buf_lock);
	return buf;
}

static void __hse_dev_buf_rbtree_remove(struct hse_dev_file_data *fdata, struct hse_dev_buf *buf)
{
	struct rb_root *tree = &fdata->buf_root;
	struct rb_node *node = &buf->rb_node;

	rb_erase(node, tree);
}

static void hse_dev_buf_rbtree_remove(struct hse_dev_file_data *fdata, struct hse_dev_buf *buf)
{
	mutex_lock(&fdata->buf_lock);
	__hse_dev_buf_rbtree_remove(fdata, buf);
	mutex_unlock(&fdata->buf_lock);
}

static int hse_dev_buf_import_dmabuf(struct hse_device *hse_dev, struct hse_dev_buf *buf, int fd)
{
	struct device *dev = hse_dev->dev;
	struct dma_buf *dmabuf = dma_buf_get(fd);
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	int ret = 0;

	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		dev_warn(dev, "%s: cannot attach\n", __func__);
		goto put_dma_buf;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		dev_warn(dev, "%s: cannot map attachment\n", __func__);
		goto detach_dma_buf;
	}

	buf->attach = attach;
	buf->sgt    = sgt;
	buf->pfn     = buf_dma_addr(buf) >> PAGE_SHIFT;
	return 0;

detach_dma_buf:
	dma_buf_detach(dmabuf, attach);
put_dma_buf:
	dma_buf_put(dmabuf);
	return ret;
}

static void hse_dev_buf_release(struct hse_dev_buf *buf)
{
	struct dma_buf_attachment *attach = buf->attach;
	struct dma_buf *dmabuf;

	if (buf->sgt)
		dma_buf_unmap_attachment(attach, buf->sgt, DMA_BIDIRECTIONAL);

	dmabuf = attach->dmabuf;
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}

static void hse_dev_buf_release_and_free_all(struct hse_dev_file_data *fdata)
{
	struct rb_node *node;

	mutex_lock(&fdata->buf_lock);
	for (node = rb_first(&fdata->buf_root); node; node = rb_first(&fdata->buf_root)) {
		struct hse_dev_buf *buf = rb_entry(node, struct hse_dev_buf, rb_node);

		__hse_dev_buf_rbtree_remove(fdata, buf);
		mutex_unlock(&fdata->buf_lock);

		pr_warn("cq %pK: %s: %#x not released\n", fdata->cq, __func__, buf->pfn);
		hse_dev_buf_release(buf);
		kfree(buf);
		mutex_lock(&fdata->buf_lock);
	}

	mutex_unlock(&fdata->buf_lock);
}

struct hse_dev_cq_cbdata {
	struct completion c;
};

static void hse_dev_complete_cb(void *p)
{
	struct hse_dev_cq_cbdata *cb_data = p;

	complete(&cb_data->c);
}

static int hse_dev_wait_timeout(struct hse_dev_cq_cbdata *cb_data, int timeout_ms)
{
	int ret;

	ret = wait_for_completion_timeout(&cb_data->c, msecs_to_jiffies(timeout_ms));
	if (ret == 0)
		ret = -ETIMEDOUT;
	return ret < 0 ? ret : 0;
}

static int hse_dev_submit(struct hse_engine *eng, struct hse_command_queue *cq)
{
	struct hse_dev_cq_cbdata cb_data = { 0 };
	int ret;

	init_completion(&cb_data.c);

	hse_cq_set_complete_callback(cq, hse_dev_complete_cb, &cb_data);

	hse_engine_add_cq(eng, cq);

	hse_engine_issue_cq(eng);

	ret = hse_dev_wait_timeout(&cb_data, 500);
	if (ret)
		hse_engine_remove_cq(eng, cq);
	else {
		if (cq->status & ~HSE_STATUS_IRQ_OK)
			ret = -EFAULT;

		else if (cq->status & HSE_STATUS_IRQ_OK)
			ret = 0;
	}

	return ret;
}

static bool hse_dev_flags_is_prep_cmd(u64 flags)
{
	return flags & HSE_FLAGS_PREP_CMD;
}

static struct hse_dev_buf *hse_dev_find_buf_and_check(struct hse_dev_file_data *fdata, u32 va, u32 f_flags, u32 offset, u32 size)
{
	struct hse_dev_buf *buf;
	int ret;

	buf = hse_dev_buf_rbtree_find(fdata, va);
	if (!buf) {
		pr_debug("cq %pK: %s: va=0x%08x: no buf\n", fdata->cq, __func__, va);
		return ERR_PTR(-ENOMEM);
	}

	ret = hse_dev_buf_check_perm(buf, f_flags);
	if (ret) {
		pr_debug("cq %pK: %s: va=0x%08x: no permission\n", fdata->cq, __func__, va);
		return ERR_PTR(ret);
	}

	ret = hse_dev_buf_check_range(buf, offset, size);
	if (ret) {
		pr_debug("cq %pK: %s: va=0x%08x: invalid size\n", fdata->cq, __func__, va);
		return ERR_PTR(ret);
	}

	if (!buf_is_contiguous(buf)) {
		pr_debug("cq %pK: %s: va=0x%08x: not _contiguous\n", fdata->cq, __func__, va);
		return ERR_PTR(-EINVAL);
	}

	return buf;
}

static int hse_dev_ioctl_cmd_copy(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_cmd_copy user_arg;
	struct hse_dev_buf *dst, *src;
	int ret;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
                return -EFAULT;

	dst = hse_dev_find_buf_and_check(fdata, user_arg.dst_va, O_WRONLY, user_arg.dst_offset,
					 user_arg.size);
	if (IS_ERR(dst))
		return PTR_ERR(dst);

	src = hse_dev_find_buf_and_check(fdata, user_arg.src_va, O_RDONLY, user_arg.src_offset,
					 user_arg.size);
	if (IS_ERR(src))
		return PTR_ERR(src);

	if (!hse_dev_flags_is_prep_cmd(user_arg.flags))
		hse_cq_reset(fdata->cq);

	ret = hse_cq_prep_copy(fdata->hse_dev, fdata->cq,
			       buf_dma_addr(dst) + user_arg.dst_offset,
			       buf_dma_addr(src) + user_arg.src_offset,
			       user_arg.size, user_arg.flags);

	if (ret) {
		pr_debug("cq %pK: %s: failed to prepare command: %d\n", fdata->cq, __func__, ret);
		hse_cq_reset(fdata->cq);
		return ret;
	}

	if (hse_dev_flags_is_prep_cmd(user_arg.flags))
		return 0;

	ret = hse_dev_submit(fdata->eng, fdata->cq);
	hse_cq_reset(fdata->cq);
	return ret;
}

static int hse_dev_ioctl_cmd_picture_copy(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_cmd_picture_copy user_arg;
	struct hse_dev_buf *dst, *src;
	int ret;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
                return -EFAULT;

	if (user_arg.dst_pitch == 0 || user_arg.src_pitch == 0 || user_arg.width == 0)
		return -EINVAL;

	dst = hse_dev_find_buf_and_check(fdata, user_arg.dst_va, O_WRONLY, user_arg.dst_offset,
					 (u32)user_arg.dst_pitch * user_arg.height);
	if (IS_ERR(dst))
		return PTR_ERR(dst);

	src = hse_dev_find_buf_and_check(fdata, user_arg.src_va, O_RDONLY, user_arg.src_offset,
					 (u32)user_arg.src_pitch * user_arg.height);
	if (IS_ERR(src))
		return PTR_ERR(src);

	if (!hse_dev_flags_is_prep_cmd(user_arg.flags))
		hse_cq_reset(fdata->cq);

	ret = hse_cq_prep_picture_copy(fdata->hse_dev, fdata->cq,
				       buf_dma_addr(dst) + user_arg.dst_offset, user_arg.dst_pitch,
				       buf_dma_addr(src) + user_arg.src_offset, user_arg.src_pitch,
				       user_arg.width, user_arg.height, user_arg.flags);

	if (ret) {
		pr_debug("cq %pK: %s: failed to prepare command: %d\n", fdata->cq, __func__, ret);
		hse_cq_reset(fdata->cq);
		return ret;
	}

	if (hse_dev_flags_is_prep_cmd(user_arg.flags))
		return 0;

	ret = hse_dev_submit(fdata->eng, fdata->cq);
	hse_cq_reset(fdata->cq);
	return ret;
}

static int hse_dev_ioctl_cmd_xor(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_cmd_xor user_arg;
	struct hse_dev_buf *dst, *src[HSE_XOR_NUM];
	int ret;
	int i;
	dma_addr_t dst_addr, src_addr[HSE_XOR_NUM];

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
                return -EFAULT;

	if (user_arg.src_num > HSE_XOR_NUM)
		return -EINVAL;

	dst = hse_dev_find_buf_and_check(fdata, user_arg.dst_va, O_WRONLY, user_arg.dst_offset,
					 user_arg.size);
	if (IS_ERR(dst))
		return PTR_ERR(dst);
	dst_addr = buf_dma_addr(dst) + user_arg.dst_offset;

	for (i = 0; i < user_arg.src_num; i++) {
		src[i] = hse_dev_find_buf_and_check(fdata, user_arg.src_va[i], O_RDONLY,
						    user_arg.src_offset[i], user_arg.size);
		if (IS_ERR(src[i]))
			return PTR_ERR(src[i]);

		src_addr[i] = buf_dma_addr(src[i]) + user_arg.src_offset[i];
	}

	ret = hse_cq_prep_xor(fdata->hse_dev, fdata->cq, dst_addr, src_addr, user_arg.src_num,
			      user_arg.size, user_arg.flags);

	if (ret)
		pr_debug("cq %pK: %s: failed to prepare command: %d\n", fdata->cq, __func__, ret);
	else
		ret = hse_dev_submit(fdata->eng, fdata->cq);
	hse_cq_reset(fdata->cq);
	return ret;
}

static int hse_dev_ioctl_cmd_constant_fill(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_cmd_constant_fill user_arg;
	struct hse_dev_buf *dst;
	int ret;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
                return -EFAULT;

	if (user_arg.size == 0 || user_arg.size > 0x4000000)
		return -EINVAL;

	dst = hse_dev_find_buf_and_check(fdata, user_arg.dst_va, O_WRONLY, user_arg.dst_offset,
					 user_arg.size);
	if (IS_ERR(dst))
		return PTR_ERR(dst);

	ret = hse_cq_prep_constant_fill(fdata->hse_dev, fdata->cq,
					buf_dma_addr(dst) + user_arg.dst_offset,
					user_arg.val, user_arg.size, user_arg.flags);
	if (ret)
		pr_debug("cq %pK: %s: failed to prepare command: %d\n", fdata->cq, __func__, ret);
	else
		ret = hse_dev_submit(fdata->eng, fdata->cq);
	hse_cq_reset(fdata->cq);
	return ret;
}

static int hse_dev_ioctl_cmd_yuy2_to_nv16(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_cmd_yuy2_to_nv16 user_arg;
	struct hse_dev_buf *luma, *chroma, *src;
	int ret;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
                return -EFAULT;

	if (user_arg.src_pitch == 0 || user_arg.dst_pitch == 0 || user_arg.width == 0)
		return -EINVAL;

	luma = hse_dev_find_buf_and_check(fdata, user_arg.luma_va, O_WRONLY, user_arg.luma_offset,
					  (u32)user_arg.dst_pitch * user_arg.height);
	if (IS_ERR(luma))
		return PTR_ERR(luma);

	chroma = hse_dev_find_buf_and_check(fdata, user_arg.chroma_va, O_WRONLY, user_arg.chroma_offset,
					    (u32)user_arg.dst_pitch * user_arg.height);
	if (IS_ERR(chroma))
		return PTR_ERR(chroma);

	src = hse_dev_find_buf_and_check(fdata, user_arg.src_va, O_RDONLY, user_arg.src_offset,
					 (u32)user_arg.src_pitch * user_arg.height);
	if (IS_ERR(src))
		return PTR_ERR(src);

	ret = hse_cq_prep_yuy2_to_nv16(fdata->hse_dev, fdata->cq,
				       buf_dma_addr(luma) + user_arg.luma_offset,
				       buf_dma_addr(chroma) + user_arg.chroma_offset,
				       user_arg.dst_pitch, buf_dma_addr(src) + user_arg.src_offset,
				       user_arg.src_pitch, user_arg.width, user_arg.height, user_arg.flags);
	if (ret)
		pr_debug("cq %pK: %s: failed to prepare command: %d\n", fdata->cq, __func__, ret);
	else
		ret = hse_dev_submit(fdata->eng, fdata->cq);
	hse_cq_reset(fdata->cq);
	return ret;
}

static int hse_dev_get_color_fmt(struct hse_dev_color_fmt *fmt)
{
	int i = 0;
	int ret = -EINVAL;

	for (i = 0; i < fmt_map_tbl_size; i++) {
		if (fmt_map_tbl[i].fmt == fmt->fmt) {
			fmt->dev_fmt = fmt_map_tbl[i].dev_fmt;
			fmt->order = fmt_map_tbl[i].order;
			fmt->type = fmt_map_tbl[i].type;
			ret = 0;
			break;
		}
	}
	return ret;
}

static int hse_dev_get_color_fmt_para(struct hse_dev_color_fmt *dst,
				      struct hse_dev_color_fmt *src)
{
	int ret = 0;

	ret = hse_dev_get_color_fmt(dst);
	if (ret) {
		pr_err("%s get dst color fmt fail", __func__);
		goto exit;
	}

	ret = hse_dev_get_color_fmt(src);
	if (ret) {
		pr_err("%s get src color fmt fail", __func__);
		goto exit;
	}
exit:
	return ret;
}

static int hse_dev_ioctl_cmd_fmt_convert(struct hse_dev_file_data *fdata,
					 unsigned long arg)
{
	struct hse_cmd_fmt_convert user_arg;
	struct hse_dev_buf *dst_luma, *dst_chroma;
	struct hse_dev_buf *src_luma, *src_chroma;
	struct hse_dev_color_fmt dst = { 0 };
	struct hse_dev_color_fmt src = { 0 };
	u32 dst_luma_addr = 0, dst_chroma_addr = 0;
	u32 src_luma_addr = 0, src_chroma_addr = 0;
	int ret = 0;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
		return -EFAULT;

	if (user_arg.src_pitch == 0 || user_arg.dst_pitch == 0 ||
	    user_arg.width == 0)
		return -EINVAL;

	dst.fmt = user_arg.dst_fmt;
	src.fmt = user_arg.src_fmt;

	ret = hse_dev_get_color_fmt_para(&dst, &src);
	if (ret)
		return -EINVAL;

	dst_luma = hse_dev_find_buf_and_check(
		fdata, user_arg.dst_luma_va, O_WRONLY, user_arg.dst_luma_offset,
		(u32)user_arg.dst_pitch * user_arg.height);
	if (IS_ERR(dst_luma))
		return PTR_ERR(dst_luma);

	dst_luma_addr = buf_dma_addr(dst_luma) + user_arg.dst_luma_offset;

	if (dst.type == HSE_FMT_TYPE_YUV) {
		u32 size = 0;
		if (dst.fmt == HSE_FMT_YUV420SP)
			size = user_arg.dst_pitch * user_arg.height / 2;
		else
			size = user_arg.dst_pitch * user_arg.height;
		dst_chroma = hse_dev_find_buf_and_check(
			fdata, user_arg.dst_chroma_va, O_WRONLY,
			user_arg.dst_chroma_offset, size);
		if (IS_ERR(dst_chroma))
			return PTR_ERR(dst_chroma);

		dst_chroma_addr =
			buf_dma_addr(dst_chroma) + user_arg.dst_chroma_offset;
	} else {
		dst_chroma_addr = 0;
	}

	src_luma = hse_dev_find_buf_and_check(
		fdata, user_arg.src_luma_va, O_RDONLY, user_arg.src_luma_offset,
		(u32)user_arg.src_pitch * user_arg.height);
	if (IS_ERR(src_luma))
		return PTR_ERR(src_luma);

	src_luma_addr = buf_dma_addr(src_luma) + user_arg.src_luma_offset;

	if (src.type == HSE_FMT_TYPE_YUV) {
		u32 size = 0;
		if (src.fmt == HSE_FMT_YUV420SP)
			size = user_arg.src_pitch * user_arg.height / 2;
		else
			size = user_arg.src_pitch * user_arg.height;

		src_chroma = hse_dev_find_buf_and_check(
			fdata, user_arg.src_chroma_va, O_RDONLY,
			user_arg.src_chroma_offset, size);

		if (IS_ERR(src_chroma))
			return PTR_ERR(src_chroma);

		src_chroma_addr =
			buf_dma_addr(src_chroma) + user_arg.src_chroma_offset;
	} else {
		src_chroma_addr = 0;
	}

	if (src.type == HSE_FMT_TYPE_YUV && dst.type == HSE_FMT_TYPE_RGB)
		hse_cq_prep_yuv2rgb_coeff(fdata->hse_dev, fdata->cq);
	else if (src.type == HSE_FMT_TYPE_RGB && dst.type == HSE_FMT_TYPE_YUV)
		hse_cq_prep_rgb2yuv_coeff(fdata->hse_dev, fdata->cq);

	ret = hse_cq_prep_fmt_convert(
		fdata->hse_dev, fdata->cq, dst.dev_fmt, user_arg.dst_pitch,
		dst.order, dst_luma_addr, dst_chroma_addr, user_arg.alpha_out,
		0, 0, src.dev_fmt, user_arg.src_pitch, src.order, src_luma_addr,
		src_chroma_addr, user_arg.width, user_arg.height);
	if (ret) {
		pr_debug("cq %pK: %s: failed to prepare command: %d\n",
			 fdata->cq, __func__, ret);
	} else {
		ret = hse_dev_submit(fdata->eng, fdata->cq);
		hse_cq_reset(fdata->cq);
	}

	return ret;
}

static int hse_dev_ioctl_cmd_rotate(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_cmd_rotate user_arg;
	struct hse_dev_buf *dst, *src;
	int ret;
	u32 height;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
                return -EFAULT;

	if (user_arg.src_pitch == 0 || user_arg.dst_pitch == 0 || user_arg.width == 0 ||
	    (user_arg.flags & ~HSE_FLAGS_ROTATE_VALID_MASK) != 0)
		return -EINVAL;

	height = user_arg.mode == 1 ? user_arg.height : user_arg.width;
	dst = hse_dev_find_buf_and_check(fdata, user_arg.dst_va, O_WRONLY, user_arg.dst_offset,
					 (u32)user_arg.dst_pitch * height);
	if (IS_ERR(dst))
		return PTR_ERR(dst);

	src = hse_dev_find_buf_and_check(fdata, user_arg.src_va, O_RDONLY, user_arg.src_offset,
					 (u32)user_arg.src_pitch * user_arg.height);
	if (IS_ERR(src))
		return PTR_ERR(src);

	if (dst->sgt->nents != 1 || src->sgt->nents != 1)
		return -EINVAL;

	ret = hse_cq_prep_rotate(fdata->hse_dev, fdata->cq,
				 buf_dma_addr(dst) + user_arg.dst_offset, user_arg.dst_pitch,
				 buf_dma_addr(src) + user_arg.src_offset, user_arg.src_pitch,
				 user_arg.width, user_arg.height, user_arg.mode,
				 user_arg.color_format, user_arg.flags & HSE_FLAGS_ROTATE_10BIT);
	if (ret)
		pr_debug("cq %pK: %s: failed to prepare command: %d\n", fdata->cq, __func__, ret);
	else {
		if (hse_dev_flags_is_prep_cmd(user_arg.flags))
			return 0;
		ret = hse_dev_submit(fdata->eng, fdata->cq);
		hse_cq_reset(fdata->cq);
	}
	return ret;
}

static int hse_dev_ioctl_get_features(struct hse_dev_file_data *fdata, unsigned long arg)
{
	u64 features = 0;

	if (hse_support_rotate_10bit(fdata->hse_dev))
		features |= HSE_FEATURE_ROTATE_10BIT_SUPPORT;

	if (copy_to_user((void *)arg, &features, sizeof(features))){
		return -EFAULT;
        }

	return 0;
}


static int hse_dev_ioctl_cmd_stretch(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_cmd_stretch user_arg;
	struct hse_dev_buf *dst, *src;
	int ret;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
	        return -EFAULT;

	if (user_arg.src_pitch == 0 || user_arg.dst_pitch == 0 ||
		user_arg.src_width == 0 || user_arg.src_height == 0 ||
		user_arg.dst_width == 0 || user_arg.dst_height == 0)
		return -EINVAL;

	src = hse_dev_find_buf_and_check(fdata, user_arg.src_va, O_RDONLY, user_arg.src_offset,
					 (u32)user_arg.src_pitch * user_arg.src_height);
	if (IS_ERR(src))
		return PTR_ERR(src);

	dst = hse_dev_find_buf_and_check(fdata, user_arg.dst_va, O_WRONLY, user_arg.dst_offset,
					  (u32)user_arg.dst_pitch * user_arg.dst_height);
	if (IS_ERR(dst))
		return PTR_ERR(dst);

	if (dst->sgt->nents != 1 || src->sgt->nents != 1)
		return -EINVAL;

	ret = hse_cq_prep_stretch(fdata->hse_dev, fdata->cq,
		buf_dma_addr(dst) + user_arg.dst_offset, user_arg.dst_pitch,
		buf_dma_addr(src) + user_arg.src_offset, user_arg.src_pitch,
		user_arg.dst_width, user_arg.dst_height, user_arg.src_width, user_arg.src_height, user_arg.colorSel);

	if (ret)
		pr_debug("cq %pK: %s: failed to prepare command: %d\n", fdata->cq, __func__, ret);
	else {
		ret = hse_dev_submit(fdata->eng, fdata->cq);
		hse_cq_reset(fdata->cq);
	}

	return ret;
}

static int hse_dev_ioctl_import_dmabuf(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_import_dmabuf user_arg;
	struct hse_dev_buf *buf;
	int ret;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
                return -EFAULT;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = hse_dev_buf_import_dmabuf(fdata->hse_dev, buf, user_arg.fd);
	if (ret)
		goto free_buf;

        hse_dev_buf_rbtree_add(fdata, buf);
	user_arg.hse_va = buf->pfn;

	if (copy_to_user((void *)arg, &user_arg, sizeof(user_arg))){
                ret = -EFAULT;
                goto remove_buf;
        }

	return 0;

remove_buf:
	hse_dev_buf_rbtree_remove(fdata, buf);
	hse_dev_buf_release(buf);
free_buf:
	kfree(buf);
	return ret;
}

static int hse_dev_ioctl_release_mem(struct hse_dev_file_data *fdata, unsigned long arg)
{
	struct hse_release_mem user_arg;
	struct hse_dev_buf *buf;

	if (copy_from_user(&user_arg, (void *)arg, sizeof(user_arg)))
		return -EFAULT;

	mutex_lock(&fdata->buf_lock);
	buf = __hse_dev_buf_rbtree_find(fdata, user_arg.hse_va);
	if (!buf) {
		mutex_unlock(&fdata->buf_lock);
		return -EINVAL;
	}
	__hse_dev_buf_rbtree_remove(fdata, buf);
	mutex_unlock(&fdata->buf_lock);

	hse_dev_buf_release(buf);
	kfree(buf);
	return 0;
}

static int hse_dev_open(struct inode *inode, struct file *filp)
{
	struct hse_device *hse_dev = container_of(filp->private_data,
		struct hse_device, mdev);
	struct hse_dev_file_data *fdata;
	int ret;

	fdata = kzalloc(sizeof(*fdata), GFP_KERNEL);
	if (!fdata)
		return -ENOMEM;

	fdata->hse_dev = hse_dev;
	fdata->cq = hse_cq_alloc(hse_dev);
	if (!fdata->cq) {
		ret = -ENOMEM;
		goto free_data;
	}

	fdata->eng = hse_device_get_engine(hse_dev, 0);
	fdata->buf_root = RB_ROOT;
        mutex_init(&fdata->buf_lock);

	filp->private_data = fdata;
	return 0;
free_data:
	kfree(fdata);
	return ret;
}

static int hse_dev_release(struct inode *inode, struct file *filp)
{
	struct hse_dev_file_data *fdata = filp->private_data;

	hse_dev_buf_release_and_free_all(fdata);

	hse_cq_free(fdata->cq);
	kfree(fdata);
	return 0;
}

static long hse_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct hse_dev_file_data *data = filp->private_data;
	struct hse_command_queue *cq = data->cq;
	int ret;

	/* reset cq for the commands */
	switch (cmd) {
	case HSE_IOCTL_CMD_CONSTANT_FILL:
	case HSE_IOCTL_CMD_XOR:
	case HSE_IOCTL_CMD_YUY2_TO_NV16:
		hse_cq_reset(data->cq);
		break;
	default:
		break;
	}

	switch (cmd) {
	case HSE_IOCTL_VERSION:
	{
		u32 v = HSE_VERSION_MAJOR << 16 | HSE_VERSION_MINOR;

		if (copy_to_user((unsigned int __user *)arg, &v, sizeof(v)))
			return -EFAULT;
		return 0;
	}

	case HSE_IOCTL_CMD_PREP_RAW:
	{
		struct hse_cmd cmd;

		if (copy_from_user(&cmd, (unsigned int __user *)arg, sizeof(cmd)))
			return -EFAULT;

		if (cmd.size == 0 || cmd.size > ARRAY_SIZE(cmd.cmds))
			return -EINVAL;

		return hse_cq_add_data(cq, cmd.cmds, cmd.size);
	}

	case HSE_IOCTL_CMD_START:
		ret = hse_dev_submit(data->eng, cq);
		hse_cq_reset(data->cq);
		return ret;

	case HSE_IOCTL_SET_ENGINE:
	{
		__u32 eng_id;

		if (copy_from_user(&eng_id, (unsigned int __user *)arg, sizeof(__u32)))
			return -EFAULT;

		data->eng = hse_device_get_engine(data->hse_dev, eng_id);
		return 0;
	}

	case HSE_IOCTL_GET_FEATURES:
		return hse_dev_ioctl_get_features(data, arg);

	case HSE_IOCTL_CMD_COPY:
		return hse_dev_ioctl_cmd_copy(data, arg);

	case HSE_IOCTL_CMD_CONSTANT_FILL:
		return hse_dev_ioctl_cmd_constant_fill(data, arg);

	case HSE_IOCTL_CMD_XOR:
		return hse_dev_ioctl_cmd_xor(data, arg);

	case HSE_IOCTL_CMD_PICTURE_COPY:
		return hse_dev_ioctl_cmd_picture_copy(data, arg);

	case HSE_IOCTL_CMD_YUY2_TO_NV16:
		return hse_dev_ioctl_cmd_yuy2_to_nv16(data, arg);

	case HSE_IOCTL_CMD_FMT_CONVERT:
		return hse_dev_ioctl_cmd_fmt_convert(data, arg);

	case HSE_IOCTL_CMD_ROTATE:
		return hse_dev_ioctl_cmd_rotate(data, arg);

	case HSE_IOCTL_CMD_STRETCH:
		return hse_dev_ioctl_cmd_stretch(data, arg);

	case HSE_IOCTL_IMPORT_DMABUF:
		return hse_dev_ioctl_import_dmabuf(data, arg);

	case HSE_IOCTL_RELEASE_MEM:
		return hse_dev_ioctl_release_mem(data, arg);

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static const struct file_operations hse_dev_fops = {
	.owner          = THIS_MODULE,
	.open           = hse_dev_open,
	.unlocked_ioctl = hse_dev_ioctl,
	.compat_ioctl   = compat_ptr_ioctl,
	.release        = hse_dev_release,
};

int hse_setup_miscdevice(struct hse_device *hse_dev)
{
	hse_dev->mdev.minor  = MISC_DYNAMIC_MINOR;
	hse_dev->mdev.name   = HSE_MISC_NAME;
	hse_dev->mdev.fops   = &hse_dev_fops;
	hse_dev->mdev.parent = NULL;
	return misc_register(&hse_dev->mdev);
}

void hse_teardown_miscdevice(struct hse_device *hse_dev)
{
	misc_deregister(&hse_dev->mdev);
}
