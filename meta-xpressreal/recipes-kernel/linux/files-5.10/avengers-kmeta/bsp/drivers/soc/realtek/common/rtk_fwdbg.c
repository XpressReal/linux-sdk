// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Realtek FW Debug driver
 *
 * Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/uaccess.h>
//#include <linux/ion.h>
//#include <soc/realtek/uapi/ion_rtk.h>
//#include <uapi/linux/ion.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/memory.h>
#include <ion_rtk_alloc.h>



#define FWDBG_IOC_MAGIC 'k'
#define FWDBG_IOCTRGETDBGREG_A _IOWR(FWDBG_IOC_MAGIC, 0x10, struct dbg_flag)
#define FWDBG_IOCTRGETDBGREG_V _IOWR(FWDBG_IOC_MAGIC, 0x11, struct dbg_flag)
#define FWDBG_IOCTRGETDBGPRINT_V _IOWR(FWDBG_IOC_MAGIC, 0x12, struct dbg_flag)

#define FWDBG_DBGREG_GET	0
#define FWDBG_DBGREG_SET	1

struct device *fw_dbg_dev;

struct dbg_flag {
	uint32_t op;
	uint32_t flagValue;
	uint32_t flagAddr;
};

struct fw_debug_flag {
	unsigned int acpu;
	unsigned int reserve_acpu[127];
	unsigned int vcpu;
	unsigned int reserve_vcpu[127];
};

struct fw_debug_flag_memory {
	int handle;
	struct fw_debug_flag *debug_flag;
	phys_addr_t debug_phys;
	size_t debug_size;
	struct dma_buf *dmabuf;
};

struct fw_debug_print_memory {
	int handle;
	void *debug_hdr;
	phys_addr_t debug_phys;
	size_t debug_size;
	struct dma_buf *dmabuf_orig;
	struct dma_buf *dmabuf;
	int32_t fd;
};

static struct fw_debug_flag_memory *mDebugFlagMemory;
static struct fw_debug_print_memory *mDebugPrintMemory;

static struct fw_debug_flag_memory *get_debug_flag_memory(struct device *dev)
{
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct fw_debug_flag_memory *tmp;
	int ret;
	unsigned int ion_flag_mask = 0;

	do {
		if (mDebugFlagMemory == NULL) {
			tmp = kzalloc(sizeof(struct fw_debug_flag_memory), GFP_KERNEL);
			if (tmp == NULL)
				goto alloc_err;

			ion_flag_mask |= RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC;
			ion_flag_mask |= RTK_FLAG_VCPU_FWACC;

			tmp->dmabuf = rheap_alloc("rtk_audio_heap",
						sizeof(struct fw_debug_flag),
						ion_flag_mask);
			if (IS_ERR_OR_NULL(tmp->dmabuf)) {
				tmp->dmabuf = rheap_alloc("rtk_media_heap",
						sizeof(struct fw_debug_flag),
						ion_flag_mask);

				if (IS_ERR_OR_NULL(tmp->dmabuf)) {
					dev_err(dev, "Failed to rheap_alloc\n");
					goto find_err;
				}
			}

			attach = dma_buf_attach(tmp->dmabuf, dev);
			if (IS_ERR(attach)) {
				dev_err(dev, "Failed to attach dmabuf\n");
				goto attach_err;
			}
			table = dma_buf_map_attachment(attach,
						 DMA_BIDIRECTIONAL);
			if (IS_ERR(table)) {
				dev_err(dev, "Failed to map attachment\n");
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

			mDebugFlagMemory = tmp;
		}
	} while (0);

	return mDebugFlagMemory;

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

static struct fw_debug_print_memory *get_debug_print_memory(struct device *dev)
{
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	struct fw_debug_print_memory *tmp;
	int ret;
	unsigned int ion_flag_mask = RTK_FLAG_NONCACHED;

	do {
		if (mDebugPrintMemory == NULL) {
			tmp = kzalloc(sizeof(struct fw_debug_print_memory), GFP_KERNEL);

			if (tmp == NULL)
				goto alloc_err;

			ion_flag_mask |= RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC;
			ion_flag_mask |= RTK_FLAG_VCPU_FWACC;

			tmp->dmabuf = rheap_alloc("rtk_audio_heap",
						sizeof(struct fw_debug_flag),
						ion_flag_mask);
			if (IS_ERR_OR_NULL(tmp->dmabuf)) {
				tmp->dmabuf = rheap_alloc("rtk_media_heap",
						sizeof(struct fw_debug_flag),
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
				dev_err(dev, "Failed to map attachment\n");
				goto map_err;
			}
			ret = dma_buf_begin_cpu_access(
					 tmp->dmabuf,
					 DMA_BIDIRECTIONAL);
			if (ret)
				goto access_err;

			/*or using sg_dma_address(table->sgl)) ?*/
			tmp->debug_phys = sg_phys(table->sgl);
			mDebugPrintMemory = tmp;
		}
	} while (0);

	return mDebugPrintMemory;


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


static struct fw_debug_flag *get_debug_flag(struct device *dev)
{
	struct fw_debug_flag_memory *debug_memory = get_debug_flag_memory(dev);

	return (debug_memory) ? debug_memory->debug_flag : NULL;
}

static phys_addr_t get_debug_flag_phyAddr(struct device *dev)
{
	struct fw_debug_flag_memory *debug_memory = get_debug_flag_memory(dev);

	return (debug_memory) ? debug_memory->debug_phys : -1UL;
}

static struct fw_debug_print_memory *get_debug_print(struct device *dev)
{
	struct fw_debug_print_memory *debug_print = get_debug_print_memory(dev);

	return (debug_print) ? debug_print : NULL;
}

static phys_addr_t get_debug_print_phyAddr(struct device *dev)
{
	struct fw_debug_print_memory *debug_print = get_debug_print_memory(dev);

	return (debug_print) ? debug_print->debug_phys : -1UL;
}


long rtk_fwdbg_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct dbg_flag dFlag;
	unsigned int *puDebugFlag = NULL;
	struct fw_debug_flag *debug_flag;
	phys_addr_t debug_flag_phyAddr;
	struct fw_debug_print_memory *debug_print;
	phys_addr_t debug_print_phyAddr;
	struct device *dev = filp->private_data;

	switch (cmd) {
	case FWDBG_IOCTRGETDBGREG_A:
	case FWDBG_IOCTRGETDBGREG_V:
		if (copy_from_user(&dFlag, (void __user *)arg, sizeof(dFlag)))
			return -EFAULT;
		debug_flag = get_debug_flag(dev);
		debug_flag_phyAddr = get_debug_flag_phyAddr(dev);

		if (debug_flag == NULL || -1 == debug_flag_phyAddr)
			return -EFAULT;

		if (cmd == FWDBG_IOCTRGETDBGREG_V) {
			puDebugFlag = &debug_flag->vcpu;
			debug_flag_phyAddr = debug_flag_phyAddr + offsetof(struct fw_debug_flag, vcpu);
		} else {
			puDebugFlag = &debug_flag->acpu;
			debug_flag_phyAddr = debug_flag_phyAddr + offsetof(struct fw_debug_flag, acpu);
		}

		if (dFlag.op == FWDBG_DBGREG_SET) {
			*puDebugFlag = dFlag.flagValue;
		} else {
			dFlag.flagValue = (unsigned int)*puDebugFlag;
			dFlag.flagAddr = (uint32_t) debug_flag_phyAddr & -1U;
			if (copy_to_user((void __user *)arg, &dFlag, sizeof(dFlag)))
				return -EFAULT;
		}

		pr_debug("FWDBG cmd=%s op=%s phyAddr=0x%08llx flag=0x%08x",
			(cmd == FWDBG_IOCTRGETDBGREG_V) ? "FWDBG_IOCTRGETDBGREG_V" : "FWDBG_IOCTRGETDBGREG_A",
			(dFlag.op == FWDBG_DBGREG_SET) ? "SET" : "GET",
			debug_flag_phyAddr, *puDebugFlag);
		break;
	case FWDBG_IOCTRGETDBGPRINT_V:
		if (copy_from_user(&dFlag, (void __user *)arg, sizeof(dFlag)))
			return -EFAULT;

		debug_print = get_debug_print(dev);
		debug_print_phyAddr = get_debug_print_phyAddr(dev);

		if (debug_print == NULL || -1 == debug_print_phyAddr)
			return -EFAULT;

		dFlag.flagAddr = (uint32_t) debug_print_phyAddr & -1U;

		debug_print->fd = debug_print->handle;
		dFlag.flagValue = debug_print->fd;

		if (copy_to_user((void __user *)arg, &dFlag, sizeof(dFlag)))
			return -EFAULT;
		break;
	default:
		pr_warn("[FWDBG]: error ioctl command...\n");
		break;
	}

	return 0;
}

int rtk_fwdbg_open(struct inode *inode, struct file *filp)
{

	filp->private_data = fw_dbg_dev;

	return 0;
}


static const struct file_operations rtk_fwdbg_fops = {
	.owner      = THIS_MODULE,
	.open       = rtk_fwdbg_open,
	.unlocked_ioctl = rtk_fwdbg_ioctl,
	.compat_ioctl = rtk_fwdbg_ioctl,
};


static char *fwdbg_devnode(struct device *dev, umode_t *mode)
{
	*mode = 0660;
	return NULL;
}


static int rtk_fwdbg_probe(struct platform_device *pdev)
{
	int major = 0;
	struct class *fwdbg_class = NULL;
	int ret = 0;

	major = 240;

	ret = register_chrdev(major, "rtk_fwdbg", &rtk_fwdbg_fops);

	fwdbg_class = class_create(THIS_MODULE, "rtk_fwdbg");
	fwdbg_class->devnode = fwdbg_devnode;
	device_create(fwdbg_class, NULL, MKDEV(major, 1), NULL, "rtk_fwdbg");

	mDebugFlagMemory = NULL;
	mDebugPrintMemory = NULL;

	fw_dbg_dev = &pdev->dev;

	fw_dbg_dev->coherent_dma_mask = DMA_BIT_MASK(32);
	fw_dbg_dev->dma_mask = (u64 *)&fw_dbg_dev->coherent_dma_mask;

	dev_info(&pdev->dev, "probe\n");

	return 0;
}


static const struct of_device_id rtk_fwdbg_of_match[] = {
	{ .compatible = "realtek, rtk-fwdbg"},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_fwdbg_of_match);

static struct platform_driver rtk_fwdbg_driver = {
	.probe = rtk_fwdbg_probe,
	.driver = {
		.name = "rtk-fwdbg",
		.of_match_table = rtk_fwdbg_of_match,
	},
};

static int __init rtk_fwdbg_init(void)
{
	return platform_driver_register(&rtk_fwdbg_driver);
}
module_init(rtk_fwdbg_init);

static void __exit rtk_fwdbg_exit(void)
{
	platform_driver_register(&rtk_fwdbg_driver);
}
module_exit(rtk_fwdbg_exit);


