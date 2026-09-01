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
#include <soc/realtek/uapi/dma_rtk.h>
#include <soc/realtek/rtk_media_heap.h>
#include <linux/ion.h>

#define DEBUGFS_NAME "dma_rtk"

struct miscdevice *priv_dev_dma;

static int dma_rtk_get_phys(int handle, phys_addr_t *addr, size_t *len)
{
	struct dma_buf *dmabuf;
	struct sg_table *table;
	struct device *dev = priv_dev_dma->this_device;
	struct dma_buf_attachment *attach;
	phys_addr_t paddr;
	int ret = 0;

	dmabuf = dma_buf_get(handle);
	if (IS_ERR(dmabuf)) {
		dev_err(dev, "Failed to get dmabuf\n");
		ret = PTR_ERR(dmabuf);
		goto get_err;
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
	paddr = sg_phys(table->sgl);
	*addr = paddr;
	*len = dmabuf->size;

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(dmabuf, attach);
attach_err:
	dma_buf_put(dmabuf);
get_err:
	return ret;
}

static int rtk_dma_get_phy_info(struct rtk_dma_ioc_phy_info *phyInfo)
{
	int ret = 0;
	phys_addr_t addr;
	size_t len;

	if (phyInfo->handle < 0) {
		ret = -EINVAL;
		goto err;
	}
	ret = dma_rtk_get_phys(phyInfo->handle, &addr, &len);

	phyInfo->addr = addr;
	phyInfo->len = len;

err:
	return ret;
}

struct dma_heap_buffer {
	struct dma_heap *heap;
	struct dma_buf *dmabuf;
	size_t size;
	unsigned long flags;
};


static int rtk_dma_sync_for_device(int fd, int cmd)
{
	struct dma_buf *dmabuf;
	enum dma_data_direction dir = (cmd == RTK_DMA_IOC_INVALIDATE)
				 ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	struct sg_table *table;
	struct device *dev = priv_dev_dma->this_device;
	struct dma_buf_attachment *attach;
	int ret = 0;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		dev_err(dev, "Failed to get dmabuf\n");
		ret = PTR_ERR(dmabuf);
		goto get_err;
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

	if (dmabuf->ops->end_cpu_access)
		ret = dmabuf->ops->end_cpu_access(dmabuf, dir);
	else {
	    int nents = dma_map_sg(priv_dev_dma->this_device,
				 table->sgl,
				 table->nents, dir);
		dma_sync_sg_for_device(priv_dev_dma->this_device,
				 table->sgl,
				 table->nents, dir);
		if (nents > 0)
			dma_unmap_sg(priv_dev_dma->this_device,
				 table->sgl,
				 table->nents, dir);
	}

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(dmabuf, attach);
attach_err:
	dma_buf_put(dmabuf);
get_err:
	return ret;
}

static int rtk_dma_sync_range_for_device(int handle, int cmd,
				 struct rtk_dma_ioc_sync_range *range_data)
{
	struct dma_buf *dmabuf;
	enum dma_data_direction dir =(cmd == RTK_DMA_IOC_INVALIDATE_RANGE) ?
					 DMA_FROM_DEVICE : DMA_TO_DEVICE;

	struct sg_table *table;
	struct device *dev = priv_dev_dma->this_device;
	struct dma_buf_attachment *attach;
	int ret = 0;
	int fd;

	fd = (int)range_data->handle & -1U;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		dev_err(dev, "Failed to get dmabuf\n");
		ret = PTR_ERR(dmabuf);
		goto get_err;
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

	{
		phys_addr_t addr;
		size_t len;
		if (dma_rtk_get_phys(handle, &addr, &len) == 0) {
			size_t offset = range_data->phyAddr - addr;

			if (dmabuf->ops->end_cpu_access_partial)
				ret = dmabuf->ops->end_cpu_access_partial(dmabuf, dir, offset, len);
			else {
				dma_addr_t paddr = dma_map_page(priv_dev_dma->this_device,
							sg_page(table->sgl),
							offset,
							range_data->len, dir);
				dma_sync_single_for_device(priv_dev_dma->this_device,
							 range_data->phyAddr,
							 range_data->len, dir);
				dma_unmap_page(priv_dev_dma->this_device, paddr,
						 range_data->len, dir);
			}
		}
	}

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(dmabuf, attach);
attach_err:
	dma_buf_put(dmabuf);
get_err:
	return ret;
}

static long dma_rtk_ioctl(struct file *filp, unsigned int cmd,
			 unsigned long arg)
{
	int ret = 0;
	int fd;
	struct rtk_dma_ioc_sync_range range_data;

	switch (cmd) {
	case RTK_DMA_GET_LAST_ALLOC_ADDR:
		pr_err("%s dont support RTK_ION_GET_LAST_ALLOC_ADDR\n", __func__);
		ret = -EFAULT;
		break;

	case RTK_DMA_IOC_INVALIDATE:
	case RTK_DMA_IOC_FLUSH:

		if (copy_from_user(&fd, (void __user *)arg, sizeof(fd))) {
			pr_err("%s:%d copy_from_user failed! \n",
				 __func__, __LINE__);
			ret = -EFAULT;
		}
		if (rtk_dma_sync_for_device(fd, cmd) != 0) {
			pr_err("%s: rtk_dma_sync_for_device failed!"
				" (cmd:%x fd:%d)\n", __func__, cmd, fd);
			ret = -EFAULT;
		}
		break;

	case RTK_DMA_IOC_FLUSH_RANGE:
	case RTK_DMA_IOC_INVALIDATE_RANGE:

		if (copy_from_user(&range_data, (void __user *)arg,
				 sizeof(range_data))) {
			pr_err("%s:%d copy_from_user failed! \n",
				 __func__, __LINE__);
			ret = -EFAULT;
			break;
		}
		if (rtk_dma_sync_range_for_device(range_data.handle, cmd,
					 &range_data)) {
			pr_err("%s: rtk_dma_sync_range_for_device failed!"
				 "(cmd:%d handle:%d)\n", __func__, cmd,
				 (int)range_data.handle);
			ret = -EFAULT;
		}
		break;

	case RTK_DMA_IOC_GET_PHYINFO:{
		struct rtk_dma_ioc_phy_info phyInfo;

		if (copy_from_user(&phyInfo, (void __user *)arg,
				 sizeof(phyInfo))) {
			pr_err("%s:%d copy_from_user failed! \n",
				 __func__, __LINE__);
			ret = -EFAULT;
			break;
		}
		if(rtk_dma_get_phy_info(&phyInfo)) {
			pr_err("%s:%d rtk_dma_get_phy_info failed! \n",
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
	case RTK_DMA_IOC_GET_MEMORY_INFO:{
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


static const struct file_operations dma_rtk_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dma_rtk_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ptr_ioctl,
#endif
};

static int dma_rtk_device_create(void)
{
	struct miscdevice *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->minor = MISC_DYNAMIC_MINOR;
	dev->name = "dma_rtk";
	dev->fops = &dma_rtk_fops;
	dev->parent = NULL;

	ret = misc_register(dev);
	if (ret) {
		pr_err("dma_rtk: failed to register misc device.\n");
		kfree(dev);
		return ret;
	}
	dev->this_device->coherent_dma_mask = DMA_BIT_MASK(64);
	dev->this_device->dma_mask = (u64 *)&dev->this_device->coherent_dma_mask;

	priv_dev_dma = dev;

	return 0;
}

device_initcall(dma_rtk_device_create);
MODULE_LICENSE("GPL v2");
