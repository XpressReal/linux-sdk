// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek RPC driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
//#define DEBUG

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/ion.h>

#include <soc/realtek/uapi/ion_rtk.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/rtk_refclk.h>
#include "rpc.h"
#include "rpc_uapi.h"

struct rpc_debug_flag {
	unsigned int acpu;
	unsigned int reserve_acpu[127];
	unsigned int vcpu;
	unsigned int reserve_vcpu[127];
};

struct rpc_debug_flag_memory {
	int handle;
	struct rpc_debug_flag *debug_flag;
	phys_addr_t debug_phys;
	size_t debug_size;
	struct dma_buf *dmabuf;
};

struct rpc_debug_print_memory {
	int handle;
	void *debug_hdr;
	phys_addr_t debug_phys;
	size_t debug_size;
	struct dma_buf *dmabuf_orig;
	struct dma_buf *dmabuf;
	int32_t fd;
};

static struct rpc_debug_flag_memory *get_debug_flag_memory(
	    struct rpc_device *rpc_dev)
{
	struct device *dev = rpc_dev->dev;
	//struct dma_heap *heap;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct rpc_debug_flag_memory *tmp;
	int ret;

	if (rpc_dev->mDebugFlagMemory == NULL) {
		unsigned int ion_flag_mask = 0;

		tmp = (struct rpc_debug_flag_memory *) kzalloc(
			    sizeof(struct rpc_debug_flag_memory), GFP_KERNEL);
		if (tmp == NULL)
			goto alloc_err;

		ion_flag_mask = 0;

		ion_flag_mask |= RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC;
		ion_flag_mask |= RTK_FLAG_VCPU_FWACC;
		tmp->dmabuf = rheap_alloc("rtk_audio_heap",
					sizeof(struct rpc_debug_flag),
					ion_flag_mask);
		if (IS_ERR_OR_NULL(tmp->dmabuf)) {
			tmp->dmabuf = rheap_alloc("rtk_media_heap",
				    sizeof(struct rpc_debug_flag),
				    ion_flag_mask);
			if (IS_ERR_OR_NULL(tmp->dmabuf)) {
				dev_err(dev, "Failed to rheap_alloc\n");
				goto find_err;
			}
		}

		//tmp->handle = dma_buf_fd(tmp->dmabuf, O_CLOEXEC);

		attach = dma_buf_attach(tmp->dmabuf, dev);
		if (IS_ERR(attach)) {
			dev_err(dev, "Failed to attach dmabuf\n");
			goto attach_err;
		}
		table = dma_buf_map_attachment(attach,
					 DMA_BIDIRECTIONAL);
		if (IS_ERR(table)) {
			dev_err(dev, "Failed to map attachment \n");
			goto map_err;
		}
		ret = dma_buf_begin_cpu_access(
			    tmp->dmabuf, DMA_BIDIRECTIONAL);
		if (ret)
			goto access_err;
		tmp->debug_flag = dma_buf_vmap(tmp->dmabuf);
		if (!tmp->debug_flag) {
			dev_err(dev, "dma_buf_vmap failed\n");
			goto kmap_err;
		}
		/*or using sg_dma_address(table->sgl)) ?*/
		tmp->debug_phys = sg_phys(table->sgl);

		rpc_dev->mDebugFlagMemory = tmp;
	}

	return (struct rpc_debug_flag_memory *)rpc_dev->mDebugFlagMemory;

kmap_err:
	dma_buf_end_cpu_access(tmp->dmabuf, DMA_BIDIRECTIONAL);
access_err:
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(tmp->dmabuf, attach);
attach_err:
	dma_buf_put(tmp->dmabuf);
find_err:
	kfree(tmp);
alloc_err:
	return NULL;
}

static struct rpc_debug_print_memory *get_debug_print_memory(
	    struct rpc_device *rpc_dev)
{
	struct device * dev = rpc_dev->dev;
	//struct dma_heap *heap;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	struct rpc_debug_print_memory *tmp;
	int ret;

	if (rpc_dev->mDebugPrintMemory == NULL) {
		unsigned int ion_flag_mask;

		tmp = (struct rpc_debug_print_memory *) kzalloc(
			    sizeof(struct rpc_debug_print_memory), GFP_KERNEL);
		ion_flag_mask = RTK_FLAG_NONCACHED;

		if (tmp == NULL)
			goto alloc_err;
		ion_flag_mask |= RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC;
		ion_flag_mask |= RTK_FLAG_VCPU_FWACC;

		tmp->dmabuf = rheap_alloc("rtk_audio_heap",
					sizeof(struct rpc_debug_flag),
					ion_flag_mask);
		if (IS_ERR_OR_NULL(tmp->dmabuf)) {
			tmp->dmabuf = rheap_alloc("rtk_media_heap",
					sizeof(struct rpc_debug_flag),
					ion_flag_mask);
			if (IS_ERR_OR_NULL(tmp->dmabuf)) {
				dev_err(dev, "Failed to rheap_alloc\n");
				goto find_err;
			}
		}

		tmp->handle = dma_buf_fd(tmp->dmabuf, O_CLOEXEC);

		attach = dma_buf_attach(tmp->dmabuf, dev);
		if (IS_ERR(attach)) {
			dev_err(dev, "Failed to attach dmabuf\n");
			goto attach_err;
		}
		table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (IS_ERR(table)) {
			dev_err(dev, "Failed to map attachment \n");
			goto map_err;
		}
		ret = dma_buf_begin_cpu_access(
				 tmp->dmabuf,
				 DMA_BIDIRECTIONAL);
		if (ret)
			goto access_err;

		tmp->debug_hdr = dma_buf_vmap(tmp->dmabuf);
		if (!tmp->debug_hdr) {
			dev_err(dev, "dma_buf_vmap failed\n");
			goto kmap_err;
		}
		/*or using sg_dma_address(table->sgl)) ?*/
		tmp->debug_phys = sg_phys(table->sgl);
		rpc_dev->mDebugPrintMemory = tmp;
	}

	return (struct rpc_debug_print_memory *)rpc_dev->mDebugPrintMemory;

kmap_err:
	dma_buf_end_cpu_access(tmp->dmabuf, DMA_BIDIRECTIONAL);
access_err:
	dma_buf_unmap_attachment(attach, table,  DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(tmp->dmabuf, attach);
attach_err:
	dma_buf_put(tmp->dmabuf);
find_err:
	kfree(tmp);
alloc_err:
	return NULL;
}

static struct rpc_debug_flag *get_debug_flag(struct rpc_device *dev)
{
	struct rpc_debug_flag_memory *debug_memory = get_debug_flag_memory(dev);

	return (debug_memory) ? debug_memory->debug_flag : NULL;
}

static phys_addr_t get_debug_flag_phyAddr(struct rpc_device *dev)
{
	struct rpc_debug_flag_memory *debug_memory = get_debug_flag_memory(dev);

	return (debug_memory) ? debug_memory->debug_phys : -1UL;
}

static struct rpc_debug_print_memory *get_debug_print(struct rpc_device *dev)
{
	struct rpc_debug_print_memory *debug_print = get_debug_print_memory(dev);

	return (debug_print) ? debug_print : NULL;
}

static phys_addr_t get_debug_print_phyAddr(struct rpc_device *dev)
{
	struct rpc_debug_print_memory *debug_print = get_debug_print_memory(dev);

	return (debug_print) ? debug_print->debug_phys : -1UL;
}

static long rpc_ctrl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct rpc_device *dev = filp->private_data;
	struct RPC_DBG_FLAG dFlag;

	switch (cmd) {
	case RPC_IOCTRESET: {
#if 0
/* TODO */
		pr_info("[RPC]start reset...\n");
		rpc_poll_init();
		rpc_intr_init();
		rpc_kern_init();

		/* clear the inter-processor interrupts */

		writel_relaxed(1 << 1, rpc_int_base+RPC_SB2_INT);

		writel(RPC_INT_SA, rpc_int_base+RPC_SB2_INT);

		rpc_set_flag(RPC_AUDIO, 0xffffffff);
#endif
		pr_info("[RPC]done...\n");
		break;}
	case RPC_IOCTRGETDBGREG_A:
	case RPC_IOCTRGETDBGREG_V: {
		unsigned int *puDebugFlag = NULL;
		struct rpc_debug_flag *debug_flag = get_debug_flag(dev);
		phys_addr_t debug_flag_phyAddr = get_debug_flag_phyAddr(dev);

		if (copy_from_user(&dFlag, (void __user *)arg, sizeof(dFlag)))
			return -EFAULT;

		if (debug_flag == NULL || -1 == debug_flag_phyAddr)
			return -EFAULT;

		if (cmd == RPC_IOCTRGETDBGREG_V) {
			puDebugFlag = &debug_flag->vcpu;
			debug_flag_phyAddr = debug_flag_phyAddr + offsetof(struct rpc_debug_flag, vcpu);
		} else {
			puDebugFlag = &debug_flag->acpu;
			debug_flag_phyAddr = debug_flag_phyAddr + offsetof(struct rpc_debug_flag, acpu);
		}

		if (dFlag.op == RPC_DBGREG_SET) {
			*puDebugFlag = dFlag.flagValue;
		} else {
			dFlag.flagValue = (unsigned int)*puDebugFlag;
			dFlag.flagAddr = (uint32_t) debug_flag_phyAddr & -1U;
			if (copy_to_user((void __user *)arg, &dFlag, sizeof(dFlag)))
				return -EFAULT;
		}

		pr_debug("RPC_DEBUG cmd=%s op=%s phyAddr=0x%08x flag=0x%08x",
			(cmd == RPC_IOCTRGETDBGREG_V) ? "RPC_IOCTRGETDBGREG_V" : "RPC_IOCTRGETDBGREG_A",
			(dFlag.op == RPC_DBGREG_SET) ? "SET" : "GET",
			(unsigned int)debug_flag_phyAddr, (unsigned int)*puDebugFlag);

		break;}
	case RPC_IOCTRGETDBGPRINT_V: {
		struct rpc_debug_print_memory * debug_print = get_debug_print(dev);
		phys_addr_t debug_print_phyAddr = get_debug_print_phyAddr(dev);
		//struct dma_buf * dmabuf;

		if (copy_from_user(&dFlag, (void __user *)arg, sizeof(dFlag)))
			return -EFAULT;
		if (debug_print == NULL || -1 == debug_print_phyAddr)
			return -EFAULT;

		dFlag.flagAddr = (uint32_t) debug_print_phyAddr & -1U;

		debug_print->fd = debug_print->handle;
		dFlag.flagValue = debug_print->fd;

		if (copy_to_user((void __user *)arg, &dFlag, sizeof(dFlag)))
		return -EFAULT;

		break;}
	default: {
		pr_warn("[RPC]: error ioctl command...\n");
		break;}
	}
	return ret;
}

static int rpc_ctrl_open(struct inode *inode, struct file *filp)
{
	int minor = MINOR(inode->i_rdev);

	pr_info("[RPC] open for RPC ioctl...(minor=%d)\n", minor);

	filp->private_data = inode->i_private;

	return 0;
}

struct file_operations rpc_ctrl_fops = {
	.unlocked_ioctl = rpc_ctrl_ioctl,
	.compat_ioctl = rpc_ctrl_ioctl,
	.open = rpc_ctrl_open,
};

MODULE_LICENSE("GPL v2");
