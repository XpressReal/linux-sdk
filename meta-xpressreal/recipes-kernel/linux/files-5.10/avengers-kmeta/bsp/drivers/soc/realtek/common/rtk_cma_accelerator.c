// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek CMA accelerator driver for Linux.
 *
 * Copyright 2022 Realtek, Inc.
 * Author: Edward Wu <edwardwu@realtek.com>
 *
 * This driver accelerates the allocation of cma and increasing the
 * success rate. Also, it increases the CMA utilization.
 *
 * Since file-backed memory on CMA area could take long-term pinning.
 *
 * Sync action after EBUSY that can unpin most file-system pages to
 * raise the success rate at next time try.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/syscalls.h>
#include <linux/suspend.h>
#include <linux/mm.h>

#include <trace/hooks/mm.h>

#define ALLOC_CMA		0x80 /* allow allocations from CMA areas */
#define LOW_RAM_SIZE	(1 * 1024 * 1024 * 1024) /* 1GB */

bool cma_sync_valve = false;
struct work_struct cma_sync_work_q;

void cma_sync_work(struct work_struct *work)
{
	ksys_sync_helper();
}

static void cma_ebusy_sync_pinned_pages(struct inode *inode)
{
	struct super_block *sb;

	if (!IS_ERR_OR_NULL(inode) && inode->i_sb) {
		sb = inode->i_sb;
		switch (sb->s_magic) {
		case BDEVFS_MAGIC:
		case EXT4_SUPER_MAGIC:
		case F2FS_SUPER_MAGIC:
			pr_debug("%s(): CMA page pinned by sb->s_magic=0x%lx\n",
					__func__, sb->s_magic);
			schedule_work(&cma_sync_work_q);
			cma_sync_valve = false;
			break;
		default:
			pr_warn("%s(): CMA page pinned by unhandled sb->s_magic=0x%lx\n",
					__func__, sb->s_magic);
			break;
		}
	}
}

static void rtk_cma_accelerator(void *data, struct acr_info *info)
{
	struct page *page = NULL;
	bool is_lru;
	struct address_space *mapping;
	struct inode *host;
	const struct address_space_operations *a_ops;
	struct hlist_node *dentry_first;
	unsigned long ino;

	if (info->failed_pfn && cma_sync_valve) {
		pr_debug("%s(): info->failed_pfn:%lx\n", __func__, info->failed_pfn);
		page = pfn_to_page(info->failed_pfn);

		/* 0249af9c0e0b ANDROID: mm: page_alloc: skip dump pages for freeable page
		 * Said the page will be freed by putback_movable_pages soon
		 * But code only exists in ACK kernel 5.10 but not 5.15
		 * Maybe we could remove this condition when alloc_contig_dump_pages()
		 * removes this too.
		 * */
		if (page_count(page) == 1) {
			pr_debug("%s(): The page will be freed by putback_movable_pages soon\n",
					__func__);
			return;
		}

		is_lru = !__PageMovable(page);
		if (is_lru) {
			mapping = page_mapping(page);
			if (mapping) {
				/*
				 * If mapping is an invalid pointer, we don't want to crash
				 * accessing it, so probe everything depending on it carefully.
				 */
				if (get_kernel_nofault(host, &mapping->host) ||
				    get_kernel_nofault(a_ops, &mapping->a_ops)) {
					pr_debug("invalid mapping:%px\n", mapping);
					return;
				}

				if (!host) {
					pr_debug("aops:%ps\n", a_ops);
					return;
				}

				if (get_kernel_nofault(dentry_first, &host->i_dentry.first) ||
				    get_kernel_nofault(ino, &host->i_ino)) {
					pr_debug("aops:%ps invalid inode:%px\n", a_ops, host);
					return;
				}

				cma_ebusy_sync_pinned_pages(host);
			}
		}
	}
}

/* Convert GFP flags to their corresponding migrate type */
#define GFP_MOVABLE_MASK (__GFP_RECLAIMABLE|__GFP_MOVABLE)
#define GFP_MOVABLE_SHIFT 3

static inline int rtk_gfp_migratetype(const gfp_t gfp_flags)
{
	return (gfp_flags & GFP_MOVABLE_MASK) >> GFP_MOVABLE_SHIFT;
}
#undef GFP_MOVABLE_MASK
#undef GFP_MOVABLE_SHIFT

static void rtk_cma_selector(void *data, unsigned int pflags, gfp_t gfp_mask,
		unsigned int *alloc_flags, bool *bypass)
{
	if (!(pflags & PF_MEMALLOC_NOCMA) &&
			rtk_gfp_migratetype(gfp_mask) == MIGRATE_MOVABLE) {
		*bypass = true;
		*alloc_flags |= ALLOC_CMA;
	}
}

static void rtk_cma_adjust(void *data, struct zone *zone, bool *is_cma_alloc)
{
	if (zone_page_state(zone, NR_FREE_CMA_PAGES) <=
	    zone_page_state(zone, NR_FREE_PAGES) / 2) {
		*is_cma_alloc = false;
	}
}

static void rtk_cma_acc_start(void *data, s64 *ts)
{
	cma_sync_valve = true;
}

static int __init rtk_cma_accelerator_init(void)
{
	int err;
	size_t total_size = totalram_pages() << PAGE_SHIFT;

	INIT_WORK(&cma_sync_work_q, cma_sync_work);

	err = register_trace_android_vh_cma_alloc_busy_info(rtk_cma_accelerator, NULL);
	if (err != 0)
		pr_err("%s(): Failed to register tracepoint\n", __func__);

	err = register_trace_android_vh_cma_alloc_start(rtk_cma_acc_start, NULL);
	if (err != 0)
		pr_err("%s(): Failed to register tracepoint\n", __func__);

	err = register_trace_android_vh_calc_alloc_flags(rtk_cma_selector, NULL);
	if (err != 0)
		pr_err("%s(): Failed to register tracepoint\n", __func__);

	/* Aggressive for low-ram devices */
	if (total_size > LOW_RAM_SIZE) {
		err = register_trace_android_vh_cma_alloc_adjust(rtk_cma_adjust, NULL);
		if (err != 0)
			pr_err("%s(): Failed to register tracepoint\n", __func__);
	}

	return err;
}

static void __exit rtk_cma_accelerator_cleanup(void)
{
	/*
	 * Description from include/trace/hooks/vendor_hooks.h
	 * vendor hooks cannot be unregistered.
	 */
	WARN_ON(1);
	pr_err("%s(): android vendor hooks cannot be unregistered\n", __func__);
}

module_init(rtk_cma_accelerator_init);
module_exit(rtk_cma_accelerator_cleanup);
MODULE_LICENSE("GPL v2");
