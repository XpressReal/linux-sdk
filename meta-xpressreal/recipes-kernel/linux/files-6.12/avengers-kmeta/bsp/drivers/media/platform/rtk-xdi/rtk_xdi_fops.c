// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "rtk-xdi-fops: " fmt
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <soc/realtek/uapi/rtk_xdi.h>
#include "rtk_xdi_internal.h"
#include "rtk_xdi_trace.h"

struct rtk_xdi_dmabuf {
	struct rb_node node;
	int handle;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
};

struct rtk_xdi_file_private {
	struct rtk_xdi_context ctx;
	struct rtk_xdi_dev *xdi;
	struct rb_root dmabuf_root;
	int next_handle;
	int result;
	struct completion done;
};

static struct rtk_xdi_dmabuf *rtk_xdi_search_dmabuf(struct rb_root *root, int handle)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct rtk_xdi_dmabuf *xdi_buf = rb_entry(node, struct rtk_xdi_dmabuf, node);

		if (handle < xdi_buf->handle)
			node = node->rb_left;
		else if (handle > xdi_buf->handle)
			node = node->rb_right;
		else
			return xdi_buf;
	}
	return NULL;
}

static int rtk_xdi_insert_dmabuf(struct rb_root *root, struct rtk_xdi_dmabuf *new_buf)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	int handle = new_buf->handle;

	/* Figure out where to put new node */
	while (*new) {
		struct rtk_xdi_dmabuf *this = rb_entry(*new, struct rtk_xdi_dmabuf, node);

		parent = *new;
		if (handle < this->handle)
			new = &((*new)->rb_left);
		else if (handle > this->handle)
			new = &((*new)->rb_right);
		else
			/* handle should be unique */
			return -EEXIST;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&new_buf->node, parent, new);
	rb_insert_color(&new_buf->node, root);

	return 0;
}

static int rtk_xdi_open(struct inode *inode, struct file *file)
{
	struct rtk_xdi_dev *xdi = container_of(file->private_data,
					       struct rtk_xdi_dev, miscdev);
	struct rtk_xdi_file_private *priv;
	int ret;

	ret = pm_runtime_resume_and_get(xdi->dev);
	if (ret < 0)
		return ret;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		pm_runtime_mark_last_busy(xdi->dev);
		pm_runtime_put_autosuspend(xdi->dev);
		return -ENOMEM;
	}

	priv->xdi = xdi;
	priv->dmabuf_root = RB_ROOT;
	priv->next_handle = 1;
	priv->ctx.still_idx = 0;
	priv->ctx.cmd_id_counter = 0;
	init_completion(&priv->done);

	file->private_data = priv;

	return 0;
}

static int rtk_xdi_release(struct inode *inode, struct file *file)
{
	struct rtk_xdi_file_private *priv = file->private_data;
	struct rtk_xdi_dev *xdi = priv->xdi;
	struct rtk_xdi_dmabuf *xdi_buf;
	struct rb_node *node;

	rtk_xdi_purge_context_cmds(xdi, &priv->ctx);
	rtk_xdi_sync_cmd_context(xdi, &priv->ctx);

	node = rb_first(&priv->dmabuf_root);
	while (node) {
		xdi_buf = rb_entry(node, struct rtk_xdi_dmabuf, node);

		dma_buf_unmap_attachment(xdi_buf->attachment, xdi_buf->sgt,
					 DMA_BIDIRECTIONAL);
		dma_buf_detach(xdi_buf->dmabuf, xdi_buf->attachment);
		dma_buf_put(xdi_buf->dmabuf);

		rb_erase(node, &priv->dmabuf_root);
		kfree(xdi_buf);
		node = rb_first(&priv->dmabuf_root);
	}

	pm_runtime_mark_last_busy(xdi->dev);
	pm_runtime_put_autosuspend(xdi->dev);
	kfree(priv);
	return 0;
}

static int rtk_xdi_ioctl_import_dmabuf(struct rtk_xdi_file_private *priv,
					 void __user *argp)
{
	struct rtk_xdi_dev *xdi = priv->xdi;
	struct xdi_dmabuf data;
	struct rtk_xdi_dmabuf *xdi_buf;
	int ret;

	if (copy_from_user(&data, argp, sizeof(data)))
		return -EFAULT;

	xdi_buf = kzalloc(sizeof(*xdi_buf), GFP_KERNEL);
	if (!xdi_buf)
		return -ENOMEM;

	xdi_buf->dmabuf = dma_buf_get(data.fd);
	if (IS_ERR(xdi_buf->dmabuf)) {
		ret = PTR_ERR(xdi_buf->dmabuf);
		pr_debug("%p: dma_buf_get failed ret=%d\n", &priv->ctx, ret);
		goto err_free_xdi_buf;
	}

	xdi_buf->attachment = dma_buf_attach(xdi_buf->dmabuf, xdi->dev);
	if (IS_ERR(xdi_buf->attachment)) {
		ret = PTR_ERR(xdi_buf->attachment);
		pr_debug("%p: dma_buf_attach failed ret=%d\n", &priv->ctx, ret);
		goto err_put_dmabuf;
	}

	xdi_buf->sgt = dma_buf_map_attachment(xdi_buf->attachment,
					      DMA_BIDIRECTIONAL);
	if (IS_ERR(xdi_buf->sgt)) {
		ret = PTR_ERR(xdi_buf->sgt);
		pr_debug("%p: dma_buf_map_attachment failed ret=%d\n", &priv->ctx, ret);
		goto err_detach_dmabuf;
	}
	xdi_buf->dma_addr = xdi_buf->sgt->sgl->dma_address;

	xdi_buf->handle = priv->next_handle++;
	data.handle = xdi_buf->handle;

	ret = rtk_xdi_insert_dmabuf(&priv->dmabuf_root, xdi_buf);
	if (ret) {
		pr_debug("%p: rtk_xdi_insert_dmabuf failed ret=%d\n", &priv->ctx, ret);
		goto err_unmap_dmabuf_no_erase;
	}

	if (copy_to_user(argp, &data, sizeof(data))) {
		ret = -EFAULT;
		goto err_unmap_dmabuf;
	}

	return 0;

err_unmap_dmabuf:
	rb_erase(&xdi_buf->node, &priv->dmabuf_root);
err_unmap_dmabuf_no_erase:
	dma_buf_unmap_attachment(xdi_buf->attachment, xdi_buf->sgt,
					 DMA_BIDIRECTIONAL);
err_detach_dmabuf:
	dma_buf_detach(xdi_buf->dmabuf, xdi_buf->attachment);
err_put_dmabuf:
	dma_buf_put(xdi_buf->dmabuf);
err_free_xdi_buf:
	kfree(xdi_buf);
	return ret;
}

static int rtk_xdi_ioctl_release_dmabuf(struct rtk_xdi_file_private *priv,
					  void __user *argp)
{
	struct xdi_dmabuf data;
	struct rtk_xdi_dmabuf *xdi_buf;

	if (copy_from_user(&data, argp, sizeof(data)))
		return -EFAULT;

	xdi_buf = rtk_xdi_search_dmabuf(&priv->dmabuf_root, data.handle);
	if (xdi_buf) {
		dma_buf_unmap_attachment(xdi_buf->attachment,
					 xdi_buf->sgt,
					 DMA_BIDIRECTIONAL);
		dma_buf_detach(xdi_buf->dmabuf, xdi_buf->attachment);
		dma_buf_put(xdi_buf->dmabuf);
		rb_erase(&xdi_buf->node, &priv->dmabuf_root);
		kfree(xdi_buf);
	} else {
		return -EINVAL;
	}

	return 0;
}

static int rtk_xdi_ioctl_set_still_region(struct rtk_xdi_file_private *priv,
					  void __user *argp)
{
	struct xdi_still_region region;
	struct rtk_xdi_dmabuf *xdi_buf;
	int i;

	if (copy_from_user(&region, argp, sizeof(region)))
		return -EFAULT;

	for (i = 0; i < 3; i++) {
		xdi_buf = rtk_xdi_search_dmabuf(&priv->dmabuf_root, region.handles[i]);
		if (!xdi_buf) {
			memset(priv->ctx.still_region_dma, 0, sizeof(priv->ctx.still_region_dma));
			return -EINVAL;
		}

		priv->ctx.still_region_dma[i] = xdi_buf->dma_addr;
	}

	return 0;
}

static int rtk_xdi_ioctl_clear_still_region(struct rtk_xdi_file_private *priv,
					    void __user *argp)
{
	memset(priv->ctx.still_region_dma, 0, sizeof(priv->ctx.still_region_dma));
	return 0;
}

static int rtk_xdi_ioctl_get_version(void __user *argp)
{
	const __u32 version =
		(XDI_IOCTL_VERSION_MAJOR << 16) | XDI_IOCTL_VERSION_MINOR;

	if (put_user(version, (__u32 __user *)argp))
		return -EFAULT;

	return 0;
}
static void rtk_xdi_fops_cmd_complete_cb(struct rtk_xdi_cmd_queue_entry *entry, void *data)
{
	struct rtk_xdi_file_private *priv = data;

	priv->result = entry->result;
	complete(&priv->done);
	kfree(entry);
}

static int rtk_xdi_ioctl_start_cmd(struct rtk_xdi_file_private *priv,
				   void __user *argp)
{
	struct rtk_xdi_dev *xdi = priv->xdi;
	struct xdi_cmd cmd;
	struct rtk_xdi_dmabuf *y_buf[4], *c_buf[4];
	struct rtk_xdi_cmd_queue_entry *entry;
	long ret;
	int i;
	u32 num_addr;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	if (copy_from_user(&cmd, argp, sizeof(cmd))) {
		kfree(entry);
		return -EFAULT;
	}

	if (cmd.flags.use_wb_diff) {
		for (i = 0; i < 3; i++) {
			if (!priv->ctx.still_region_dma[i]) {
				kfree(entry);
				pr_debug("%p: use_wb_diff but still_region_dma is not set\n",
					 &priv->ctx);
				return -EINVAL;
			}
		}
	}

	num_addr = cmd.flags.mode == 3 ? 2 : 4;
	for (i = 0; i < num_addr; i++) {
		y_buf[i] = rtk_xdi_search_dmabuf(&priv->dmabuf_root, cmd.y_handles[i]);
		c_buf[i] = rtk_xdi_search_dmabuf(&priv->dmabuf_root, cmd.c_handles[i]);

		if (!y_buf[i] || !c_buf[i]) {
			kfree(entry);
			pr_debug("%p: y_handles[%d] or c_handles[%d] is not set\n",
				 &priv->ctx, i, i);
			return -EINVAL;
		}

		entry->hw_cmd.y_addr[i] = y_buf[i]->dma_addr + cmd.y_offsets[i];
		entry->hw_cmd.c_addr[i] = c_buf[i]->dma_addr + cmd.c_offsets[i];
		entry->hw_cmd.y_pitches[i] = cmd.y_pitches[i];
		entry->hw_cmd.c_pitches[i] = cmd.c_pitches[i];

		pr_debug("%p: y_addr[%d] = 0x%llx, c_addr[%d] = 0x%llx\n",
			 &priv->ctx, i, entry->hw_cmd.y_addr[i], i, entry->hw_cmd.c_addr[i]);
		pr_debug("%p: y_pitches[%d] = %d, c_pitches[%d] = %d\n",
			 &priv->ctx, i, entry->hw_cmd.y_pitches[i], i, entry->hw_cmd.c_pitches[i]);
	}

	entry->hw_cmd.w = cmd.w;
	entry->hw_cmd.h = cmd.h;
	entry->hw_cmd.flags = cmd.flags;

	pr_debug("%p: w = %d, h = %d\n", &priv->ctx, entry->hw_cmd.w, entry->hw_cmd.h);

	if (cmd.num_params > XDI_MAX_PARAMS ||
	    !rtk_xdi_params_is_valid(xdi, cmd.params, cmd.num_params)) {
		kfree(entry);
		pr_debug("%p: params is not valid\n", &priv->ctx);
		return -EINVAL;
	}
	memcpy(entry->hw_cmd.params, cmd.params, sizeof(entry->hw_cmd.params));
	entry->hw_cmd.num_params = cmd.num_params;

	entry->ctx = &priv->ctx;
	entry->id = priv->ctx.cmd_id_counter++;

	reinit_completion(&priv->done);
	entry->cb_data = priv;
	entry->cb = rtk_xdi_fops_cmd_complete_cb;
	rtk_xdi_queue_cmd(xdi, entry);
	trace_xdi_cmd_queue(entry->ctx, entry->id);
	rtk_xdi_start_cmd_work(xdi);

	ret = wait_for_completion_timeout(&priv->done, msecs_to_jiffies(1000));
	ret = ret == 0 ? -ETIMEDOUT : 0;

	if (ret == 0 && priv->result < 0)
		ret = priv->result;

	return ret;
}

static long rtk_xdi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rtk_xdi_file_private *priv = file->private_data;
	void __user *argp = (void __user *)arg;
	int ret;

	pr_debug("%p: ioctl cmd=0x%x\n", &priv->ctx, cmd);

	switch (cmd) {
	case XDI_IOCTL_GET_VERSION:
		ret = rtk_xdi_ioctl_get_version(argp);
		break;
	case XDI_IOCTL_IMPORT_DMABUF:
		ret = rtk_xdi_ioctl_import_dmabuf(priv, argp);
		break;
	case XDI_IOCTL_RELEASE_DMABUF:
		ret = rtk_xdi_ioctl_release_dmabuf(priv, argp);
		break;
	case XDI_IOCTL_START_CMD:
		ret = rtk_xdi_ioctl_start_cmd(priv, argp);
		break;
	case XDI_IOCTL_SET_STILL_REGION:
		ret = rtk_xdi_ioctl_set_still_region(priv, argp);
		break;
	case XDI_IOCTL_CLEAR_STILL_REGION:
		ret = rtk_xdi_ioctl_clear_still_region(priv, argp);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

const struct file_operations rtk_xdi_fops = {
	.owner		= THIS_MODULE,
	.open		= rtk_xdi_open,
	.release	= rtk_xdi_release,
	.unlocked_ioctl	= rtk_xdi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = compat_ptr_ioctl,
#endif
};

MODULE_IMPORT_NS(DMA_BUF);
