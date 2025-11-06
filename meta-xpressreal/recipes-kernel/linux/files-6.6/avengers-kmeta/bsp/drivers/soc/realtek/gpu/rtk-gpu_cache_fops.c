// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022-2025 Realtek Semiconductor Corp.
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#define pr_fmt(fmt) "gpu_cache: " fmt

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/nospec.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <soc/realtek/uapi/rtk_gpu_cache.h>
#include "rtk-gpu_cache_internal.h"

struct rtk_gpu_cache_buf_data {
	struct rb_node rb_node;
	struct dma_buf_attachment *attachment;
	dma_addr_t dma_addr;
	struct sg_table *sgt;
};

struct rtk_gpu_cache_file {
	struct gpu_cache_context context;
	struct rtk_gpu_cache_data *data;
	struct rb_root buf_root;
};

typedef int rtk_gpu_cache_ioctl_t(struct rtk_gpu_cache_data *data,
				  struct rtk_gpu_cache_file *file_priv,
				  void *arg);

struct rtk_gpu_cache_ioctl_desc {
	unsigned int cmd;
	rtk_gpu_cache_ioctl_t *func;
};

#define DEFINE_GPU_CACHE_IOCTL(_ioctl, _func) \
	[_IOC_NR(GPU_CACHE_IOCTL_ ## _ioctl)] = { .cmd = GPU_CACHE_IOCTL_ ## _ioctl, \
		.func = (rtk_gpu_cache_ioctl_t *)rtk_gpu_cache_ioctl_ ##_func, }

static int rtk_gpu_cache_import_dmabuf(struct rtk_gpu_cache_data *data, int fd,
				       struct rtk_gpu_cache_buf_data *buf_data)
{
	struct device *dev = rtk_gpu_cache_dev(data);
	struct dma_buf *buf = dma_buf_get(fd);
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	int ret = 0;

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	attach = dma_buf_attach(buf, dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		pr_debug("%s: cannot attach buf to dev\n", __func__);
		goto put_dma_buf;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		pr_debug("%s: cannot map attachment\n", __func__);
		goto detach_dma_buf;
	}

	if (sgt->nents != 1) {
		ret = -EINVAL;
		pr_debug("%s: scatter list not supportted\n", __func__);
		goto detach_dma_buf;
	}

	dma_addr = sg_dma_address(sgt->sgl);
	if (!dma_addr) {
		ret = -EINVAL;
		pr_debug("%s: invalid dma address\n", __func__);
		goto detach_dma_buf;
	}

	buf_data->attachment = attach;
	buf_data->sgt        = sgt;
	buf_data->dma_addr   = dma_addr;
	return 0;

detach_dma_buf:
	dma_buf_detach(buf, attach);
put_dma_buf:
	dma_buf_put(buf);
	return ret;
}

static void rtk_gpu_cache_release_dmabuf(struct rtk_gpu_cache_data *data,
					 struct rtk_gpu_cache_buf_data *buf_data)
{
	struct dma_buf_attachment *attach = buf_data->attachment;
	struct dma_buf *buf;

	if (buf_data->sgt)
		dma_buf_unmap_attachment(attach, buf_data->sgt, DMA_BIDIRECTIONAL);
	buf = attach->dmabuf;
	dma_buf_detach(buf, attach);
	dma_buf_put(buf);
}

static int rtk_gpu_cache_buf_data_addr_less(struct rb_node *_a, struct rb_node *_b)
{
	struct rtk_gpu_cache_buf_data *a = rb_entry(_a, struct rtk_gpu_cache_buf_data, rb_node);
	struct rtk_gpu_cache_buf_data *b = rb_entry(_b, struct rtk_gpu_cache_buf_data, rb_node);

	return a->dma_addr < b->dma_addr;
}

static int rtk_gpu_cache_open(struct inode *inode, struct file *filp)
{
	struct rtk_gpu_cache_file *file_priv;
	struct rtk_gpu_cache_data *data = rtk_gpu_cache_mdev_to_data(filp->private_data);

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -EINVAL;
	file_priv->data = data;
	file_priv->buf_root = RB_ROOT;
	filp->private_data = file_priv;

	pr_debug("context %p: create\n", &file_priv->context);

	rtk_gpu_cache_dev_get(data);
	return 0;
}

static struct rtk_gpu_cache_buf_data *
rtk_gpu_cache_file_lookup_buf(struct rtk_gpu_cache_file *file_priv, u64 va)
{
	struct rb_root *tree = &file_priv->buf_root;
	struct rb_node *node = tree->rb_node;

	while (node) {
		struct rtk_gpu_cache_buf_data *buf = rb_entry(node, struct rtk_gpu_cache_buf_data, rb_node);

		if (buf->dma_addr == (dma_addr_t)va)
			return buf;
		else if (buf->dma_addr < (dma_addr_t)va)
			node = node->rb_right;
		else
			node = node->rb_left;
	}
	return NULL;
}

static void rtk_gpu_cache_file_add_buf(struct rtk_gpu_cache_file *file_priv,
				       struct rtk_gpu_cache_buf_data *buf_data)
{
	struct rb_root *tree = &file_priv->buf_root;
	struct rb_node **link = &tree->rb_node;
	struct rb_node *node = &buf_data->rb_node;
	struct rb_node *parent = NULL;

	while (*link) {
		parent = *link;
		if (rtk_gpu_cache_buf_data_addr_less(node, parent))
			link = &parent->rb_left;
		else
			link = &parent->rb_right;
	}

	rb_link_node(node, parent, link);
	rb_insert_color(node, tree);
}

static void rtk_gpu_cache_file_remove_buf(struct rtk_gpu_cache_file *file_priv,
					  struct rtk_gpu_cache_buf_data *buf_data)
{
	struct rb_root *tree = &file_priv->buf_root;
	struct rb_node *node = &buf_data->rb_node;

	rb_erase(node, tree);
}

static void rtk_gpu_cache_file_remove_all_bufs(struct rtk_gpu_cache_data *data,
					       struct rtk_gpu_cache_file *file_priv)
{
	struct rb_node *node;

	for (node = rb_first(&file_priv->buf_root); node; node = rb_first(&file_priv->buf_root)) {
		struct rtk_gpu_cache_buf_data *buf_data =
			rb_entry(node, struct rtk_gpu_cache_buf_data, rb_node);

		pr_notice("context %p: release dma address %p\n",
			  &file_priv->context, &buf_data->dma_addr);
		rtk_gpu_cache_release_dmabuf(data, buf_data);
		rtk_gpu_cache_file_remove_buf(file_priv, buf_data);
		kfree(buf_data);
	}
}

static int rtk_gpu_cache_release(struct inode *inode, struct file *filp)
{
	struct rtk_gpu_cache_file *file_priv = filp->private_data;
	struct rtk_gpu_cache_data *data = file_priv->data;

	rtk_gpu_cache_lock(data);
	rtk_gpu_cache_release_all_frames_by_context(data, &file_priv->context);
	rtk_gpu_cache_file_remove_all_bufs(data, file_priv);
	rtk_gpu_cache_unlock(data);

	rtk_gpu_cache_dev_put(data);
	pr_debug("context %p: destroy\n", &file_priv->context);
	kfree(file_priv);
	return 0;
}

static int rtk_gpu_cache_ioctl_get_version(struct rtk_gpu_cache_data *data,
					   struct rtk_gpu_cache_file *file_priv,
					   void *_arg)
{
	u32 *arg = _arg;

	*arg = (GPU_CACHE_IOCTL_VERSION_MAJOR << 16) | GPU_CACHE_IOCTL_VERSION_MINOR;
	return 0;
}

static int rtk_gpu_cache_ioctl_import_dmabuf(struct rtk_gpu_cache_data *data,
					     struct rtk_gpu_cache_file *file_priv,
					     void *_arg)
{
	struct gpu_cache_ioctl_import_dmabuf_data *arg = _arg;
	struct rtk_gpu_cache_buf_data *buf_data;
	int ret;

	buf_data = kzalloc(sizeof(*buf_data), GFP_KERNEL);
	if (!buf_data)
		return -ENOMEM;

	ret = rtk_gpu_cache_import_dmabuf(data, arg->fd, buf_data);
	if (ret) {
		pr_debug("context %p: import dmabuf failed\n", &file_priv->context);
		kfree(buf_data);
		return ret;
	}

	arg->va = sg_dma_address(buf_data->sgt->sgl);
	rtk_gpu_cache_file_add_buf(file_priv, buf_data);
	return 0;
}

static int rtk_gpu_cache_ioctl_release_dmabuf(struct rtk_gpu_cache_data *data,
					      struct rtk_gpu_cache_file *file_priv,
					      void *_arg)
{
	u64 *va = _arg;
	struct rtk_gpu_cache_buf_data *buf_data = rtk_gpu_cache_file_lookup_buf(file_priv, *va);

	if (!buf_data)
		return -ENOMEM;
	rtk_gpu_cache_file_remove_buf(file_priv, buf_data);
	rtk_gpu_cache_release_dmabuf(data, buf_data);
	return 0;
}

static int rtk_gpu_cache_get_dev_addr(struct rtk_gpu_cache_file *file_priv, u64 va, u32 offset,
				      u64 *out_addr)
{
	struct rtk_gpu_cache_buf_data *buf_data = rtk_gpu_cache_file_lookup_buf(file_priv, va);
	u32 size;

	if (!buf_data) {
		pr_debug("context %p: invalid VA 0x%016llx\n", &file_priv->context, va);
		return -ENOMEM;
	}

	size = buf_data->attachment->dmabuf->size;
	if (offset >= size) {
		pr_debug("context %p: offset(%d) exceeds buf size(%d)\n", &file_priv->context,
			 offset, size);
		return -ENOBUFS;
	}

	*out_addr = va + offset;
	return 0;
}

static int rtk_gpu_cache_ioctl_set_frame(struct rtk_gpu_cache_data *data,
					 struct rtk_gpu_cache_file *file_priv,
					 void *_arg)
{

	struct gpu_cache_ioctl_set_frame *arg = _arg;
	struct gpu_cache_set_frame_args x = {};

	x.index = arg->index;
	x.type  = arg->type;
	if (rtk_gpu_cache_get_dev_addr(file_priv, arg->adr_va, arg->adr_begin_offset, &x.adr_begin))
		return -ENOMEM;
	if (rtk_gpu_cache_get_dev_addr(file_priv, arg->adr_va, arg->adr_end_offset, &x.adr_end))
		return -ENOMEM;
	if (rtk_gpu_cache_get_dev_addr(file_priv, arg->header_va, arg->header_offset, &x.header))
		return -ENOMEM;
	if (rtk_gpu_cache_get_dev_addr(file_priv, arg->payload_va, arg->payload_offset, &x.payload))
		return -ENOMEM;
	return rtk_gpu_cache_set_frame(data, &file_priv->context, &x);
}

static int rtk_gpu_cache_ioctl_clear_frame(struct rtk_gpu_cache_data *data,
					   struct rtk_gpu_cache_file *file_priv,
					   void *_arg)
{
	struct gpu_cache_ioctl_clear_frame *arg = _arg;

	return rtk_gpu_cache_clear_frame(data, &file_priv->context, arg->index, arg->type);
}

static int rtk_gpu_cache_ioctl_set_frame_info(struct rtk_gpu_cache_data *data,
					      struct rtk_gpu_cache_file *file_priv,
					      void *_arg)
{
	struct gpu_cache_ioctl_set_frame_info *arg = _arg;
	struct gpu_cache_set_frame_info_args x = {};

	x.decomp_payload_pitch = arg->decomp_payload_pitch;
	x.decomp_header_pitch = arg->decomp_header_pitch;
	x.gpu_ip_pitch = arg->gpu_ip_pitch;
	x.index = arg->index;
	x.pic_height = arg->pic_height;
	return rtk_gpu_cache_set_frame_info(data, &file_priv->context, &x);
}

static int rtk_gpu_cache_ioctl_request_frame(struct rtk_gpu_cache_data *data,
					     struct rtk_gpu_cache_file *file_priv,
					     void *_arg)
{
	struct gpu_cache_ioctl_request_frame *arg = _arg;
	struct gpu_cache_request_frame_args x = {};
	int ret;

	if (arg->hint_prev_adr) {
		pr_debug("context %p: hint_prev_adr=1\n", &file_priv->context);
		if (rtk_gpu_cache_get_dev_addr(file_priv, arg->y_adr_va, arg->y_adr_begin_offset,
					       &x.y_adr_begin))
			return -ENOMEM;
		if (rtk_gpu_cache_get_dev_addr(file_priv, arg->y_adr_va, arg->y_adr_end_offset,
					       &x.y_adr_end))
			return -ENOMEM;
		if (rtk_gpu_cache_get_dev_addr(file_priv, arg->c_adr_va, arg->c_adr_begin_offset,
					       &x.c_adr_begin))
			return -ENOMEM;
		if (rtk_gpu_cache_get_dev_addr(file_priv, arg->c_adr_va, arg->c_adr_end_offset,
					       &x.c_adr_end))
			return -ENOMEM;

		x.check_prev_adr = 1;
	}

	ret = rtk_gpu_cache_request_frame(data, &file_priv->context, &x);
	if (ret < 0)
		return ret;

	arg->frame_index = ret;
	return 0;
}

static int rtk_gpu_cache_ioctl_release_frame(struct rtk_gpu_cache_data *data,
					     struct rtk_gpu_cache_file *file_priv,
					     void *_arg)
{
	u32 *arg = _arg;

	return rtk_gpu_cache_release_frame(data, &file_priv->context, *arg);
}

static int rtk_gpu_cache_ioctl_flush(struct rtk_gpu_cache_data *data,
				     struct rtk_gpu_cache_file *file_priv,
				     void *_arg)
{
	return rtk_gpu_cache_flush(data);
}

static int rtk_gpu_cache_ioctl_set_param(struct rtk_gpu_cache_data *data,
					 struct rtk_gpu_cache_file *file_priv,
					 void *_arg)
{
	struct gpu_cache_ioctl_param_data *arg = _arg;

	return rtk_gpu_cache_set_param(data, arg->param_id, arg->value);
}

static int rtk_gpu_cache_ioctl_get_param(struct rtk_gpu_cache_data *data,
					 struct rtk_gpu_cache_file *file_priv,
					 void *_arg)
{
	struct gpu_cache_ioctl_param_data *arg = _arg;

	return rtk_gpu_cache_get_param(data, arg->param_id, &arg->value);
}

static int do_ioctl_locked(struct file *filp, rtk_gpu_cache_ioctl_t *func, void *kdata)
{
	struct rtk_gpu_cache_file *file_priv = filp->private_data;
	struct rtk_gpu_cache_data *data = file_priv->data;
	int ret;

	rtk_gpu_cache_lock(data);
	ret = func(data, file_priv, kdata);
	rtk_gpu_cache_unlock(data);
	return ret;
}

static const struct rtk_gpu_cache_ioctl_desc rtk_gpu_cache_ioctls[] = {
	DEFINE_GPU_CACHE_IOCTL(GET_VERSION,    get_version),
	DEFINE_GPU_CACHE_IOCTL(IMPORT_DMABUF,  import_dmabuf),
	DEFINE_GPU_CACHE_IOCTL(RELEASE_DMABUF, release_dmabuf),
	DEFINE_GPU_CACHE_IOCTL(SET_FRAME,      set_frame),
	DEFINE_GPU_CACHE_IOCTL(CLEAR_FRAME,    clear_frame),
	DEFINE_GPU_CACHE_IOCTL(SET_FRAME_INFO, set_frame_info),
	DEFINE_GPU_CACHE_IOCTL(SET_PARAM,      set_param),
	DEFINE_GPU_CACHE_IOCTL(GET_PARAM,      get_param),
	DEFINE_GPU_CACHE_IOCTL(FLUSH,          flush),
	DEFINE_GPU_CACHE_IOCTL(REQUEST_FRAME,  request_frame),
	DEFINE_GPU_CACHE_IOCTL(RELEASE_FRAME,  release_frame),
};

static long rtk_gpu_cache_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	char stack_kdata[128];
	char *kdata = stack_kdata;
	unsigned int in_size, out_size, drv_size, ksize;
	int nr = _IOC_NR(cmd);
	int ret = 0;
	const struct rtk_gpu_cache_ioctl_desc *ioctl = NULL;
	rtk_gpu_cache_ioctl_t *func;

	if (nr >= ARRAY_SIZE(rtk_gpu_cache_ioctls))
		return -EINVAL;
	nr = array_index_nospec(nr, ARRAY_SIZE(rtk_gpu_cache_ioctls));

	ioctl = &rtk_gpu_cache_ioctls[nr];
	if (!ioctl || !ioctl->func)
		return -ENOIOCTLCMD;
	func = ioctl->func;

	drv_size = _IOC_SIZE(ioctl->cmd);
	out_size = in_size = _IOC_SIZE(cmd);
	if ((cmd & ioctl->cmd & IOC_IN) == 0)
		in_size = 0;
	if ((cmd & ioctl->cmd & IOC_OUT) == 0)
		out_size = 0;
	ksize = max(max(in_size, out_size), drv_size);

	if (ksize > sizeof(stack_kdata)) {
		kdata = kmalloc(ksize, GFP_KERNEL);
		if (!kdata)
			return -ENOMEM;
	}

	if (copy_from_user(kdata, (void __user *)arg, in_size) != 0) {
		ret = -EFAULT;
		goto err;
	}
	if (ksize > in_size)
		memset(kdata + in_size, 0, ksize - in_size);

	ret = do_ioctl_locked(file, func, kdata);

	if (copy_to_user((void __user *)arg, kdata, out_size) != 0)
		ret = -EFAULT;
err:
	if (ret)
		pr_debug("%s: comm=\"%s\", pid=%d, ret=%d\n", __func__, current->comm,
			 task_pid_nr(current), ret);

	if (kdata != stack_kdata)
		kfree(kdata);
	return ret;
}

const struct file_operations rtk_gpu_cache_fops = {
	.owner          = THIS_MODULE,
	.open           = rtk_gpu_cache_open,
	.release        = rtk_gpu_cache_release,
	.unlocked_ioctl = rtk_gpu_cache_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = compat_ptr_ioctl,
#endif
};

