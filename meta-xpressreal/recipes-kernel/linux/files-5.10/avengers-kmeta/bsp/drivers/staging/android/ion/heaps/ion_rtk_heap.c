// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF RTK heap exporter
 *
 * Copyright (c) 2022 Realtek Semiconductor Corp
 * Author: <cy.huang@realtek.com> .
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/dma-buf.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/swiotlb.h>
#include <linux/debugfs.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/uapi/ion_rtk.h>
#include <soc/realtek/rtk_media_heap.h>
#include <linux/ion.h>

static u64 dma_mask;
#define DEBUGFS_NAME "ion_rtk"

struct debugfs_device {
	struct dentry *debug_root;
};
struct miscdevice *priv_dev;

static inline bool ion_flag_is_protected(unsigned long flags)
{
	if (flags & ION_FLAG_PROTECTED_MASK)
		return true;
	return false;
}

static inline bool ion_flag_is_noncached(unsigned long flags)
{
	if (flags & ION_FLAG_NONCACHED)
		return true;
	return false;
}

/* ION heap operations functions */
static int ion_rtk_heap_allocate(struct ion_heap *heap,
			  struct ion_buffer *buffer, unsigned long len,
			  unsigned long flags)
{
	int ret;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	unsigned int alloc_flags = 0;
	struct device *dev = priv_dev->this_device;

	dev->coherent_dma_mask = DMA_BIT_MASK(64);
	dev->dma_mask = (u64 *)&dev->coherent_dma_mask;

	alloc_flags |= (flags & ION_FLAG_SCPUACC) ? RTK_FLAG_SCPUACC : 0;
	alloc_flags |= (flags & ION_FLAG_ACPUACC) ? RTK_FLAG_ACPUACC : 0;
	alloc_flags |= (flags & ION_FLAG_HWIPACC) ? RTK_FLAG_HWIPACC : 0;
	alloc_flags |= (flags & ION_FLAG_VE_SPEC) ? RTK_FLAG_VE_SPEC : 0;
	alloc_flags |= (flags & ION_FLAG_VCPU_FWACC) ? RTK_FLAG_VCPU_FWACC : 0;
	alloc_flags |= (flags & ION_FLAG_CMA) ? RTK_FLAG_CMA : 0;
	alloc_flags |= (flags & ION_FLAG_HIFIACC) ? RTK_FLAG_HIFIACC : 0;

	alloc_flags |= (flags & ION_FLAG_PROTECTED_BIT0) ?
			 RTK_FLAG_PROTECTED_BIT0 : 0;
	alloc_flags |= (flags & ION_FLAG_PROTECTED_BIT1) ?
			 RTK_FLAG_PROTECTED_BIT1  : 0;
	alloc_flags |= (flags & ION_FLAG_PROTECTED_BIT2) ?
			 RTK_FLAG_PROTECTED_BIT2 : 0;
	alloc_flags |= (flags & ION_FLAG_PROTECTED_BIT3) ?
			 RTK_FLAG_PROTECTED_BIT3 : 0;

	alloc_flags |= (flags & ION_FLAG_PROTECTED_EXT_BIT0) ?
			 RTK_FLAG_PROTECTED_EXT_BIT0 : 0;
	alloc_flags |= (flags & ION_FLAG_PROTECTED_EXT_BIT1) ?
			 RTK_FLAG_PROTECTED_EXT_BIT1  : 0;
	alloc_flags |= (flags & ION_FLAG_PROTECTED_EXT_BIT2) ?
			 RTK_FLAG_PROTECTED_EXT_BIT2 : 0;

	alloc_flags |= (flags & ION_FLAG_NONCACHED) ?
			 RTK_FLAG_NONCACHED : 0;
	alloc_flags |= (flags & ION_FLAG_SKIP_ZERO) ?
			 RTK_FLAG_SKIP_ZERO : 0;

	/* we steal the buffer from dmaheap , dmabuf ops is inevitable */
	dmabuf = rheap_alloc(NULL, len, alloc_flags);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(dev, "Failed to dma_buf_get\n");
		ret = PTR_ERR(dmabuf);
		goto rheap_err;
	}
	dma_buf_set_name(dmabuf, __func__);
	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		ret = PTR_ERR(attach);
		goto attach_err;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		dev_err(dev, "Failed to map attachment \n");
		ret = PTR_ERR(table);
		goto map_err;
	}

	buffer->priv_virt = attach;
	buffer->sg_table = table;

	return 0;

map_err:
       dma_buf_detach(dmabuf, attach);
attach_err:
	dma_buf_put(dmabuf);
rheap_err:
	return -ENOMEM;
}

static void ion_rtk_heap_free(struct ion_buffer *buffer)
{
	struct dma_buf_attachment *attach = buffer->priv_virt;
	struct dma_buf *dmabuf;
       /* cache_sgt_mapping = true */
	struct sg_table *table = attach->sgt;

	dmabuf = attach->dmabuf;
	BUG_ON(!dmabuf);

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}

static int ion_rtk_get_phys(int handle, phys_addr_t *addr, size_t *len)
{
	struct ion_buffer *buffer;
	struct device *dev = priv_dev->this_device;
	struct dma_buf_attachment *attach;
	struct dma_buf *dmabuf;
	struct sg_table *table;
	int ret = 0;

	dmabuf = dma_buf_get(handle);
	if (IS_ERR(dmabuf)) {
		WARN(1, "invalid fd:%d to get dmabuf (err=%d)",
				handle, (int)PTR_ERR(dmabuf));
		return PTR_ERR(dmabuf);
	}

	if (!strcmp(dmabuf->exp_name, "ion_dma_buf")) {
		buffer = dmabuf->priv;
		table = buffer->sg_table;
		*addr = sg_phys(table->sgl);
		*len = buffer->size;
		goto out;
	}

	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		ret = PTR_ERR(attach);
		goto attach_err;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		dev_err(dev, "Failed to map attachment \n");
		ret = PTR_ERR(table);
		goto map_err;
	}

	*addr = sg_phys(table->sgl);
	*len = dmabuf->size;

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(dmabuf, attach);
attach_err:
out:
	dma_buf_put(dmabuf);

	return ret;
}

static int rtk_ion_get_phy_info(struct rtk_ion_ioc_phy_info *phyInfo)
{
	int ret = 0;
	phys_addr_t addr;
	size_t len;

	if (phyInfo->handle < 0) {
		ret = -EINVAL;
		goto err;
	}
	ret = ion_rtk_get_phys(phyInfo->handle, &addr, &len);

	phyInfo->addr = addr;
	phyInfo->len = len;

err:
	return ret;
}




static int rtk_ion_sync_for_device(int fd, int cmd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;
	enum dma_data_direction dir = (cmd == RTK_ION_IOC_INVALIDATE)
				 ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);
	buffer = dmabuf->priv;
	dma_mask = 0xffffffff;
	priv_dev->this_device->dma_mask = &dma_mask;

	if (!ion_flag_is_protected(buffer->flags) &&
		!ion_flag_is_noncached(buffer->flags)) {
	    	int nents = dma_map_sg(priv_dev->this_device,
				 buffer->sg_table->sgl,
				 buffer->sg_table->nents, dir);
		dma_sync_sg_for_device(priv_dev->this_device,
				 buffer->sg_table->sgl,
				 buffer->sg_table->nents, dir);
		if (nents > 0)
			dma_unmap_sg(priv_dev->this_device,
				 buffer->sg_table->sgl,
				 buffer->sg_table->nents, dir);
	}
	dma_buf_put(dmabuf);
	return 0;
}

static int rtk_ion_sync_range_for_device(int handle, int cmd,
				 struct rtk_ion_ioc_sync_rane *range_data)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;
	enum dma_data_direction dir =(cmd == RTK_ION_IOC_INVALIDATE_RANGE) ?
					 DMA_FROM_DEVICE : DMA_TO_DEVICE;
	int fd;

	fd = (int)range_data->handle & -1U;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	buffer = dmabuf->priv;
	dma_mask = 0xffffffff;
	priv_dev->this_device->dma_mask = &dma_mask;

	if (!ion_flag_is_protected(buffer->flags) &&
		!ion_flag_is_noncached(buffer->flags)) {
		phys_addr_t addr;
		size_t len;
		if (ion_rtk_get_phys(handle, &addr, &len) == 0) {
			size_t offset = range_data->phyAddr - addr;
			dma_addr_t paddr = dma_map_page(priv_dev->this_device,
						sg_page(buffer->sg_table->sgl),
						offset,
						range_data->len, dir);
			dma_sync_single_for_device(priv_dev->this_device,
						 range_data->phyAddr,
						 range_data->len, dir);
			dma_unmap_page(priv_dev->this_device, paddr,
					 range_data->len, dir);
		}
	}

	dma_buf_put(dmabuf);
	return 0;
}

static long ion_rtk_ioctl(struct file *filp, unsigned int cmd,
			 unsigned long arg)
{
	int ret = 0;
	int fd;
	struct rtk_ion_ioc_sync_rane range_data;


	switch (cmd) {
	case RTK_ION_GET_LAST_ALLOC_ADDR:
		pr_err("%s dont support RTK_ION_GET_LAST_ALLOC_ADDR\n", __func__);
		ret = -EFAULT;
		break;

	case RTK_ION_IOC_INVALIDATE:
	case RTK_ION_IOC_FLUSH:

		if (copy_from_user(&fd, (void __user *)arg, sizeof(fd))) {
			pr_err("%s:%d copy_from_user failed! \n",
				 __func__, __LINE__);
			ret = -EFAULT;
		}
		if (rtk_ion_sync_for_device(fd, cmd) != 0) {
			pr_err("%s: rtk_ion_sync_for_device failed!"
				" (cmd:%x fd:%d)\n", __func__, cmd, fd);
			ret = -EFAULT;
		}
		break;

	case RTK_ION_IOC_FLUSH_RANGE:
	case RTK_ION_IOC_INVALIDATE_RANGE:

		if (copy_from_user(&range_data, (void __user *)arg,
				 sizeof(range_data))) {
			pr_err("%s:%d copy_from_user failed! \n",
				 __func__, __LINE__);
			ret = -EFAULT;
			break;
		}
		if (rtk_ion_sync_range_for_device(range_data.handle, cmd,
					 &range_data)) {
			pr_err("%s: rtk_ion_sync_range_for_device failed!"
				 "(cmd:%d handle:%d)\n", __func__, cmd,
				 (int)range_data.handle);
			ret = -EFAULT;
		}
		break;

	case RTK_ION_IOC_GET_PHYINFO:{
		struct rtk_ion_ioc_phy_info phyInfo;

		if (copy_from_user(&phyInfo, (void __user *)arg,
				 sizeof(phyInfo))) {
			pr_err("%s:%d copy_from_user failed! \n",
				 __func__, __LINE__);
			ret = -EFAULT;
			break;
		}
		if(rtk_ion_get_phy_info(&phyInfo)) {
			pr_err("%s:%d rtk_ion_get_phy_info failed! \n",
				 __func__, __LINE__);
			ret = -EFAULT;
			break;
		}

		ret = copy_to_user((void __user *)arg, &phyInfo,
				 sizeof(phyInfo));
		if (ret) {
			pr_err("%s:%d copy_to_user failed! (ret = %d)\n",
				 __func__, __LINE__, ret);
			return -EFAULT;
		}

		break;
	}
	case RTK_ION_IOC_GET_MEMORY_INFO:{
		pr_err("%s:%d not yet support RTK_ION_IOC_GET_MEMORY_INFO\n",
				 __func__, __LINE__);
		ret = -EFAULT;
		break;
	}

	default:
		pr_err("%s: Unknown custom ioctl\n", __func__);
		ret = -ENOTTY;
		break;
	}
return ret;
}


static const struct file_operations ion_rtk_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ion_rtk_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ptr_ioctl,
#endif
};

static struct ion_heap_ops ion_rtk_heap_ops = {
	.allocate = ion_rtk_heap_allocate,
	.free = ion_rtk_heap_free,
};

static int ion_rtk_device_create(void)
{
	struct miscdevice *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->minor = MISC_DYNAMIC_MINOR;
	dev->name = "ion_rtk";
	dev->fops = &ion_rtk_fops;
	dev->parent = NULL;

	ret = misc_register(dev);
	if (ret) {
		pr_err("ion_rtk: failed to register misc device.\n");
		kfree(dev);
		return ret;
	}
	priv_dev = dev;
	return 0;
}


static void debugfs_add_heap(struct ion_heap *heap)
{
	struct debugfs_device *dev;
	struct dentry *debug_file;

	dev = kzalloc(sizeof(struct debugfs_device), GFP_KERNEL);
	if (!dev)
		return;

	dev->debug_root = debugfs_create_dir(DEBUGFS_NAME, NULL);
	if (!dev->debug_root)
		kfree(dev);

	debug_file = debugfs_create_file(heap->name, 0644, dev->debug_root,
					 heap, NULL /*&debug_heap_fops*/);
}

static int __ion_add_rtk_heaps(char *name, int type)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);;
	if (!heap)
		return -ENOMEM;

	heap->name = kasprintf(GFP_KERNEL, "%s", name);;
	heap->type = type;
	heap->ops = &ion_rtk_heap_ops;

	ion_device_add_heap(heap);
	debugfs_add_heap(heap);
	return 0;
}

static int ion_add_rtk_heaps(void)
{
	__ion_add_rtk_heaps("Audio", RTK_ION_HEAP_TYPE_AUDIO);
	__ion_add_rtk_heaps("Media", RTK_ION_HEAP_TYPE_MEDIA);

	ion_rtk_device_create();

	return 0;
}
device_initcall(ion_add_rtk_heaps);
MODULE_LICENSE("GPL v2");
