// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#include <linux/arm-smccc.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-direct.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/mount.h>
#include <soc/realtek/rtk_ipc_shm.h>

#include "autctl.h"

#define SIP_IL_LOAD_CLUSTER_FW		0x82000005

static int cluster_part_read_pages(struct device *dev, struct block_device *bdev, pgoff_t pg_off, struct page *pg, size_t len)
{
	int error;
	struct bio *bio;

	bio = bio_alloc(bdev, 1, REQ_OP_READ|REQ_SYNC, GFP_NOIO | __GFP_HIGH);
	bio->bi_iter.bi_sector = pg_off * (PAGE_SIZE >> 9);
	bio_set_dev(bio, bdev);

	if (bio_add_page(bio, pg, len, 0) < len) {
		dev_err(dev, "Adding page to bio failed at %llu\n",
			(unsigned long long)bio->bi_iter.bi_sector);
		bio_put(bio);
		return -EFAULT;
	}

	error = submit_bio_wait(bio);
	bio_put(bio);
	return error;
}

/* using gpt name label */
static int match_dev_by_volname(struct device *dev, const void *data)
{
	struct block_device *bdev = dev_to_bdev(dev);
	const char *volname = data;

	if (!bdev->bd_meta_info || strcmp(volname, bdev->bd_meta_info->volname))
		return 0;
	return 1;
}

static dev_t get_cluster_dev(const char *volname)
{
	struct device *dev;

	dev = class_find_device(&block_class, NULL, volname, &match_dev_by_volname);
	if (!dev)
		return 0;
	put_device(dev);
	return dev->devt;
}

static int cluster_load_fw(unsigned long cmd, unsigned long paddr, unsigned long size)
{
	struct arm_smccc_res res = {0};

	arm_smccc_smc(cmd, paddr, size, 0, 0, 0, 0, 0, &res);

	if (res.a0 != 0)
		return -EPERM;

	return 0;
}

static int cluster_read_partition(struct device *dev, dev_t devt, const char *p_name, uint32_t cmd)
{
	struct block_device *bdev;
	unsigned int blk_sz;
	size_t total_len;
	struct page *pg;
	void *vaddr;
	dma_addr_t dma_addr;
	sector_t start, number;
	int ret = 0;

	if (devt == 0 || dev == NULL)
		return -EINVAL;

	bdev = blkdev_get_by_dev(devt, FMODE_READ, NULL, NULL);
	if (IS_ERR(bdev))
		return PTR_ERR(bdev);

	blk_sz = bdev_logical_block_size(bdev);

	start = get_start_sect(bdev);
	number = bdev_nr_sectors(bdev);
	total_len = number * blk_sz;

	dev_dbg(dev, "0x%08x(%s) start:%llu number:%llu block_size:%u\n",
		devt, p_name, start, number, blk_sz);

	vaddr = dma_alloc_coherent(dev, total_len, &dma_addr, GFP_KERNEL);
	if (vaddr == NULL) {
		dev_err(dev, "part %s allocate fail with size %zu\n", p_name, total_len);
		ret = -ENOMEM;
		goto out;
	}
	dev_dbg(dev, "vaddr:0x%08lx dma_addr:0x%08lx\n", (unsigned long)vaddr, (unsigned long)dma_addr);

	ret = set_blocksize(bdev, PAGE_SIZE);
	if (ret) {
		dev_err(dev, "set_blocksize fail\n");
		goto out;
	}

	pg = phys_to_page(dma_to_phys(dev, dma_addr));

	ret = cluster_part_read_pages(dev, bdev, 0, pg, total_len);
	if (ret) {
		dev_err(dev, "cluster bio fail!\n");
		goto out;
	}

	wmb();
	ret = cluster_load_fw((unsigned long)cmd, (unsigned long)dma_to_phys(dev, dma_addr),
				(unsigned long)total_len);
	if (ret)
		dev_err(dev, "%s load & decrypt fail!\n", p_name);

out:
	if (vaddr)
		dma_free_coherent(dev, total_len, vaddr, dma_addr);
	if (bdev)
		blkdev_put(bdev, NULL);

	return ret;
}

int rtk_aut_load_cluster(struct autctl_device *autctl)
{
	dev_t clus_devt;
	int ret = 0;

	clus_devt = get_cluster_dev(autctl->clus_part);

	if (!clus_devt) {
		dev_err(autctl->dev, "partition not found\n");
		return -ENOENT;
	}

	ret = cluster_read_partition(autctl->dev, clus_devt, autctl->clus_part, SIP_IL_LOAD_CLUSTER_FW);
	if (ret) {
		dev_err(autctl->dev, "%s read with fail %d\n", autctl->clus_part, ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(rtk_aut_load_cluster);

MODULE_DESCRIPTION("Realtek Automotive Control driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-autctl");
