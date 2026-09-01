// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF RTK heap exporter
 *
 * Copyright (c) 2022 Realtek Semiconductor Corp
 * Author: <cy.huang@realtek.com> .
 */

#include <linux/cma.h>
#include <linux/device.h>
#include <linux/device/class.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-map-ops.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fdtable.h>
#include <linux/genalloc.h>
#include <linux/highmem.h>
#include <linux/kstrtox.h>
#include <linux/list_sort.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>
#include <linux/syscalls.h>

#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
#include <trace/hooks/dmabuf.h>
#endif

#include "cma.h"
#include "rtk_heap_helpers.h"
#include "rtk_media_heap.h"
#include "rtk_protect.h"

#define DEVNAME "rtk_media_heap"
#define TMP_BUF_MAX 256

#ifndef ALPHA_DEFAULT
#define ALPHA_DEFAULT 1
#endif

#ifndef BETA_DEFAULT
#define BETA_DEFAULT 100
#endif

bool protect_stub_mode = false;

static int alpha = ALPHA_DEFAULT;
static int beta  = BETA_DEFAULT;


static struct dma_buf *rtk_dyn_protect_cma_do_allocate(struct dma_heap *heap,
	size_t size, unsigned long flags, bool uncached);


static const struct soc_device_attribute rtk_soc_hank[] = {
	{ .family = "Realtek Hank", },
	{ /* sentinel */ }
};

unsigned int rheap_data_size;
unsigned long sys_flags;

struct rtk_flag_replace {
	unsigned long condition;
	unsigned long replace;
	const struct soc_device_attribute *soc_type;
};
struct rheap_desc {
	int (*rheap_miscdev_init)(void);
	int (*rheap_sysfs_init)(struct platform_device *dev);
	void (*rheap_debugfs_init)(void);
	void (*rheap_procfs_init)(void);
	int android_hook_init;
};

static struct rtk_flag_replace rtk_flag_match[] = {

	{ .condition = RTK_FLAG_PROTECTED_V2_VIDEO_POOL,
	  .replace = RTK_FLAG_PROTECTED_V2_VIDEO_POOL |
			 RTK_FLAG_PROTECTED_EXT_BITS(1),
	  .soc_type = NULL },

	{ .condition = RTK_FLAG_PROTECTED_V2_AUDIO_POOL,
	  .replace = RTK_FLAG_PROTECTED_V2_AUDIO_POOL |
			 RTK_FLAG_PROTECTED_EXT_BITS(2),
	  .soc_type = NULL },

	{ .condition = RTK_FLAG_PROTECTED_V2_VO_POOL,
	  .replace = RTK_FLAG_PROTECTED_V2_VO_POOL |
			 RTK_FLAG_PROTECTED_EXT_BITS(1),
	  .soc_type = NULL },

	{ .condition = RTK_FLAG_PROTECTED_V2_AO_POOL,
	  .replace = RTK_FLAG_PROTECTED_V2_AO_POOL |
			 RTK_FLAG_PROTECTED_EXT_BITS(1),
	  .soc_type = NULL },

	{ .condition = RTK_FLAG_PROTECTED_V2_FW_STACK,
	  .replace = RTK_FLAG_PROTECTED_V2_AUDIO_POOL |
			 RTK_FLAG_PROTECTED_EXT_BITS(1),
	  .soc_type = rtk_soc_hank },

	{ .condition = 0,
	  .replace = 0,
	  .soc_type = NULL },
};


enum tag_type {
	tag_pre = 0,
	tag_gen,
	tag_cma,
};

struct dev_data {
	char *devname;
	unsigned long ext_flag;
};

struct rtk_heap_data {
	char *name;
	bool skip_set_protect;
	unsigned long flags;
	struct dev_data *devdata;
	int num_devdata;
	unsigned long static_cma_size;
	unsigned long penalty;
	void *priv;
};

struct rtk_priv {
	struct rtk_heap *rtk_heap;
	unsigned long ext_flag;
};


/* TODO :
   1.hank video video2 dsc as static.
   2.general ao ssc as dynamic.
   3.general fwstack_ssc4 as dynamic.
 */

/********************************************************************
** Secure Type :

   VIDEO EXT_TYPE 1: video client buffer
   VIDEO EXT_TYPE 2: video userdata buffer

   AUDIO EXT_TYPE 1: AFW stack data bufffer
   AUDIO EXT_TYPE 2: Audio client bufffer
   AUDIO EXT_TYPE 3: HIFI client bufffer

   AO    EXT_TYPE 1: Ao client bufffer

   VO    EXT_TYPE 1: Vo client bufffer
   VO    EXT_TYPE 2: VO_DEINTERLACE bufffer from AFW alloc
   VO    EXT_TYPE 3: VO_CVBS bufffer from AFW alloc
   VO    EXT_TYPE 4: VO_KEEP_LASTFRAME bufffer from AFW alloc

   HIFI  EXT_TYPE 1: HIFI data bufffer

*********************************************************************
*/

#define RTK_DEVNAME_DEF(name, def...)		\
	static struct dev_data name[]	= { def }

#define RTK_DEVNAME_REG(data)	\
	.devdata	= data,				\
	.num_devdata	= ARRAY_SIZE(data)

#define COMP_ARRAY(param...)	param
#define COMP_DEV(_name, flag)	{ .devname = _name, .ext_flag = flag, }


RTK_DEVNAME_DEF(SECURE_META,
	        COMP_ARRAY(COMP_DEV("secure-meta",
					 0),
				)
		);
RTK_DEVNAME_DEF(SECURE_TP,
		COMP_ARRAY(COMP_DEV("secure-tp",
					 0),
				)
		);
RTK_DEVNAME_DEF(SECURE_OTA,
		COMP_ARRAY(COMP_DEV("secure-ota",
					 0),
				)
		);
RTK_DEVNAME_DEF(SECURE_FWSTACK,
		COMP_ARRAY(COMP_DEV("secure-fwstack",
					 0),
				)
		);

RTK_DEVNAME_DEF(HIFI,
		COMP_ARRAY(COMP_DEV("hifi",
					 0),
				)
		);

RTK_DEVNAME_DEF(HEAP_B1P5,
		COMP_ARRAY(COMP_DEV("heap-high",
					 0),
				)
		);

RTK_DEVNAME_DEF(SECURE_NPP,
		COMP_ARRAY(COMP_DEV("secure-npp",
					 0),
				)
		);
RTK_DEVNAME_DEF(SECURE_NPUINF,
		COMP_ARRAY(COMP_DEV("secure-npuinf",
					 0),
				)
		);
RTK_DEVNAME_DEF(SECURE_NPUMODEL,
		COMP_ARRAY(COMP_DEV("secure-npumodel",
					 0),
				)
		);

RTK_DEVNAME_DEF(SECURE_VO,
		COMP_ARRAY(COMP_DEV("secure-vo-client",
				 RTK_FLAG_PROTECTED_EXT_BITS(1)),
			   COMP_DEV("secure-vo-deint",
				 RTK_FLAG_PROTECTED_EXT_BITS(2)),
			   COMP_DEV("secure-vo-cvbs",
				 RTK_FLAG_PROTECTED_EXT_BITS(3)),
			   COMP_DEV("secure-vo-lastf",
				 RTK_FLAG_PROTECTED_EXT_BITS(4)),
			)
		);

RTK_DEVNAME_DEF(SECURE_VOS,
		COMP_ARRAY(COMP_DEV("secure-vos-client",
				 RTK_FLAG_PROTECTED_EXT_BITS(1)),
			   COMP_DEV("secure-vos-deint",
				 RTK_FLAG_PROTECTED_EXT_BITS(2)),
			   COMP_DEV("secure-vos-cvbs",
				 RTK_FLAG_PROTECTED_EXT_BITS(3)),
			   COMP_DEV("secure-vos-lastf",
				 RTK_FLAG_PROTECTED_EXT_BITS(4)),
			)
		);

RTK_DEVNAME_DEF(SECURE_AUDIO,
		COMP_ARRAY(COMP_DEV("secure-audio-stack",
				 RTK_FLAG_PROTECTED_EXT_BITS(1)),
			   COMP_DEV("secure-audio-client",
				 RTK_FLAG_PROTECTED_EXT_BITS(2)),
			   COMP_DEV("secure-hifi-client",
				 RTK_FLAG_PROTECTED_EXT_BITS(3)),
			)
		);

RTK_DEVNAME_DEF(SECURE_VIDEO,
		COMP_ARRAY(COMP_DEV("secure-video-client",
				 RTK_FLAG_PROTECTED_EXT_BITS(1)),
			   COMP_DEV("secure-video-usrdata",
				 RTK_FLAG_PROTECTED_EXT_BITS(2)),
			)
		);

RTK_DEVNAME_DEF(SECURE_VIDEO2,
		COMP_ARRAY(COMP_DEV("secure-video2-client",
				 RTK_FLAG_PROTECTED_EXT_BITS(1)),
			)
		);

RTK_DEVNAME_DEF(SECURE_AO,
		COMP_ARRAY(COMP_DEV("secure-ao-client",
				 RTK_FLAG_PROTECTED_EXT_BITS(1)),
			)
		);

RTK_DEVNAME_DEF(SECURE_HIFI,
		COMP_ARRAY(COMP_DEV("secure-hifi-data",
				 RTK_FLAG_PROTECTED_EXT_BITS(1)),
			)
		);



static struct rtk_heap_data rheap_data[] = {
	{
		.name = "metadata",
		.skip_set_protect = true,
		.flags = (RTK_FLAG_HWIPACC |
			RTK_FLAG_VCPU_FWACC | RTK_FLAG_ACPUACC |
			RTK_FLAG_PROTECTED_V2_METADATA_POOL),

		RTK_DEVNAME_REG(SECURE_META),
	},

	{
		.name = "vo_dsc3",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_DYNAMIC |
			RTK_FLAG_PROTECTED_V2_VO_POOL |
			RTK_FLAG_SUPPORT_NONCACHED |
			RTK_FLAG_SKIP_ZERO),
		.penalty = 3000,
		RTK_DEVNAME_REG(SECURE_VO),
	},

	{
		.name = "tp_ssc2",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA |
			RTK_FLAG_PROTECTED_V2_TP_POOL),

		RTK_DEVNAME_REG(SECURE_TP),
	},

	{
		.name = "audio_ssc1",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_V2_AUDIO_POOL),

		RTK_DEVNAME_REG(SECURE_AUDIO),

	},

	{
		.name = "video_ssc5",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_V2_VIDEO_POOL),

		RTK_DEVNAME_REG(SECURE_VIDEO),
	},

	{
		.name = "video_dsc5",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_DYNAMIC |
			RTK_FLAG_PROTECTED_V2_VIDEO_POOL |
			RTK_FLAG_SUPPORT_NONCACHED|
			RTK_FLAG_SKIP_ZERO),
		.penalty = 6000,
		RTK_DEVNAME_REG(SECURE_VIDEO),
	},

	{
		.name = "video2_ssc5",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_V2_VIDEO_POOL),

		RTK_DEVNAME_REG(SECURE_VIDEO2),
	},

	{
		.name = "video2_dsc5",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_DYNAMIC |
			RTK_FLAG_PROTECTED_V2_VIDEO_POOL |
			RTK_FLAG_SUPPORT_NONCACHED|
			RTK_FLAG_SKIP_ZERO),
		.penalty = 6000,
		RTK_DEVNAME_REG(SECURE_VIDEO2),
	},

	{
		.name = "ao_ssc6",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_V2_AO_POOL) ,

		RTK_DEVNAME_REG(SECURE_AO),
	},

	{
		.name = "ao_dsc6",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_DYNAMIC |
			RTK_FLAG_PROTECTED_V2_AO_POOL) ,
		.penalty = 12000,
		RTK_DEVNAME_REG(SECURE_AO),
	},

	{
		.name = "ota_dsc8",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_DYNAMIC |
			RTK_FLAG_PROTECTED_V2_OTA_POOL |
			RTK_FLAG_SUPPORT_NONCACHED |
			RTK_FLAG_SKIP_ZERO),
		.penalty = 1000000,
		RTK_DEVNAME_REG(SECURE_OTA),

	},

	{
		.name = "vo_s_dsc3",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA |
			RTK_FLAG_PROTECTED_DYNAMIC |
			RTK_FLAG_PROTECTED_V2_VO_POOL |
			RTK_FLAG_SUPPORT_NONCACHED |
			RTK_FLAG_SKIP_ZERO),
		.penalty = 1500,
		RTK_DEVNAME_REG(SECURE_VOS),

	},

	{
		.name = "fwstack_dsc4",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA | RTK_FLAG_VCPU_FWACC |
			RTK_FLAG_ACPUACC | RTK_FLAG_PROTECTED_DYNAMIC |
			RTK_FLAG_PROTECTED_V2_FW_STACK) ,
		.penalty = 20000,
		RTK_DEVNAME_REG(SECURE_FWSTACK),

	},

	{
		.name = "hifi_b1p5",
		.flags = (RTK_FLAG_CMA |
			 RTK_FLAG_SCPUACC | RTK_FLAG_HWIPACC |
			 RTK_FLAG_HIFIACC |
			 RTK_FLAG_SUPPORT_NONCACHED) ,
		.penalty = ULONG_MAX,
		RTK_DEVNAME_REG(HIFI),
	},

	{
		.name = "npp",
		.flags = (RTK_FLAG_CMA |
			  RTK_FLAG_PROTECTED_DYNAMIC |
			  RTK_FLAG_PROTECTED_V2_NPU_PP_POOL |
			  RTK_FLAG_SUPPORT_NONCACHED),

		RTK_DEVNAME_REG(SECURE_NPP),

	},

	{
		.name = "npu-inference",
		.flags = (RTK_FLAG_CMA |
			  RTK_FLAG_PROTECTED_DYNAMIC |
			  RTK_FLAG_PROTECTED_V2_NPU_INFERENCE_POOL |
			  RTK_FLAG_SUPPORT_NONCACHED),

		RTK_DEVNAME_REG(SECURE_NPUINF),
	},

	{
		.name = "npu-model",
		.flags = (RTK_FLAG_CMA |
			  RTK_FLAG_PROTECTED_DYNAMIC |
			  RTK_FLAG_PROTECTED_V2_NPU_MODEL_POOL |
			  RTK_FLAG_SUPPORT_NONCACHED),

		RTK_DEVNAME_REG(SECURE_NPUMODEL),
	},

	{
		.name = "heap_b1p5",
		.flags = (RTK_FLAG_SCPUACC |
			RTK_FLAG_HWIPACC | RTK_FLAG_CMA |
			RTK_FLAG_SUPPORT_NONCACHED |
			RTK_FLAG_SKIP_ZERO),
		.penalty = ULONG_MAX,
		RTK_DEVNAME_REG(HEAP_B1P5),
	},

	{
		.name = "hifi-ssc12",
		.flags = (RTK_FLAG_HWIPACC | RTK_FLAG_CMA |
			 RTK_FLAG_HIFIACC | RTK_FLAG_PROTECTED_V2_HIFI_POOL),
		RTK_DEVNAME_REG(SECURE_HIFI),
	},

};

DEFINE_MUTEX(chplist_mutex);
LIST_HEAD(cheap_list);
LIST_HEAD(cheap_low_list);
LIST_HEAD(gheap_list);

static bool is_rtk_skip_zero(int flags)
{
	if (flags & RTK_FLAG_SKIP_ZERO)
		return true;
	return false;
}

static int rtk_toc_type(int flags)
{
	return (flags & RTK_FLAG_TOC_MASK);
}


static int rtk_protected_type(int flags)
{
	return (flags & RTK_FLAG_PROTECTED_MASK);
}

static int rtk_protected_ext_type(int flags)
{
	return (flags & RTK_FLAG_PROTECTED_EXT_MASK);
}

bool is_rtk_dynamic_protect(int flags)
{
	if (flags & RTK_FLAG_PROTECTED_DYNAMIC)
		return true;
	return false;
}

static int is_rtk_exclusive_pool(int flags)
{
	if (!is_rtk_dynamic_protect(flags))
		return false;

	switch (flags & RTK_FLAG_PROTECTED_MASK) {
	case RTK_FLAG_PROTECTED_V2_AUDIO_POOL:
	case RTK_FLAG_PROTECTED_V2_AO_POOL:
	case RTK_FLAG_PROTECTED_V2_FW_STACK:
		return true;
	default:
		return false;
	}
}

bool is_rtk_static_protect(int flags)
{
	if (is_rtk_dynamic_protect(flags))
		return false;

	if (rtk_protected_type(flags))
		return true;
	else
		return false;
}

bool __maybe_unused is_rtk_gen_heap(int flags)
{
	if (flags & RTK_FLAG_CMA)
		return false;
	return true;
}

static void rtk_cma_record_max_usage(struct rtk_heap *rtk_cma_heap)
{
	unsigned long used_bit, used_pages;

	used_bit = bitmap_weight(rtk_cma_heap->use_bitmap,
				 (int)cma_bitmap_maxno(rtk_cma_heap->cma));
	used_pages = used_bit << rtk_cma_heap->cma->order_per_bit;
	if (used_pages > rtk_cma_heap->max_used_pages)
		rtk_cma_heap->max_used_pages = used_pages;
}

static void rtk_gen_record_max_usage(struct rtk_heap *rtk_gen_heap)
{
	size_t used_pages, avail, size;

	size = gen_pool_size(rtk_gen_heap->gen_pool);
	avail = gen_pool_avail(rtk_gen_heap->gen_pool);
	used_pages = (size - avail) >> PAGE_SHIFT;
	if (used_pages > rtk_gen_heap->max_used_pages)
		rtk_gen_heap->max_used_pages = used_pages;
}

#ifdef CONFIG_ARM
int rtk_dma_update_pte(pte_t *pte, unsigned long addr, void *data)
{
	struct page *page = virt_to_page(addr);
	pgprot_t prot = *(pgprot_t *)data;

	set_pte_ext(pte, mk_pte(page, prot), 0);
	return 0;
}

void set_highmem_prot(struct page *page, unsigned long vaddr, pgprot_t prot)
{
	int index;

	if (vaddr >= PKMAP_ADDR(0) && vaddr < PKMAP_ADDR(LAST_PKMAP)) {
		index = PKMAP_NR(vaddr);
		set_pte_ext(&(pkmap_page_table[index]), mk_pte(page, prot), 0);
	}

}

void rtk_dma_remap(struct page *page, size_t size, pgprot_t prot)
{
	int nr_pages = size >> PAGE_SHIFT;
	unsigned long start;
	unsigned end;
	int i;

	if (PageHighMem(page)) {
		for (i = 0; i < nr_pages; i++) {
			start = (unsigned long) page_address(page);
			if (start) {
				set_highmem_prot(page, start, prot);
				flush_tlb_kernel_range(start, start+PAGE_SIZE); // page by page
			}
			page++;
		}
	} else {
		start = (unsigned long) page_address(page);
		end = start + size;
		apply_to_page_range(&init_mm, start, size, rtk_dma_update_pte, &prot);
		flush_tlb_kernel_range(start, end);
	}

}
#else
void rtk_dma_remap(struct page *page, size_t size, pgprot_t prot)
{
	unsigned long start = (unsigned long) page_address(page);
	struct page_change_data data;
	const pgprot_t kernel_prot = PAGE_KERNEL;
	int ret;

	if (pgprot_val(prot) == pgprot_val(kernel_prot)) {
		data.set_mask = __pgprot(PTE_VALID | PTE_WRITE);
		data.clear_mask = __pgprot(PTE_RDONLY);
	} else {
		data.set_mask = __pgprot(0);
		data.clear_mask = __pgprot(PTE_VALID);
	}

	ret = _rtk_apply_to_page_range(start, size, &data);
	WARN_ON(ret);
}
#endif

static struct rtk_protect_info *find_protect_info(struct rtk_heap *rtk_heap,
					 unsigned long offset, bool gen)
{
	struct rtk_protect_info *rtk_protect_info, *tmp, *ret = NULL;
	struct list_head *protect_list;
	unsigned long base;
	size_t size;

	protect_list = &rtk_heap->list;

	list_for_each_entry_safe(rtk_protect_info, tmp, protect_list, list) {
		base = rtk_protect_info->create_info.mem.base;
		size = rtk_protect_info->create_info.mem.size;
		if (offset >= base && offset < base + size) {
			ret = rtk_protect_info;
			pr_debug("%s base=0x%lx size=0x%lx \n", __func__,
						base, size);
			break;
		}
	}
	return ret;
}


static struct rtk_protect_info *find_protect_info_in(void *rtk_heap,
					 unsigned long offset, size_t size,
					 bool gen)
{
	struct rtk_protect_info *pr_info_0, *pr_info_1;

	pr_info_0 = find_protect_info(rtk_heap, offset, gen);
	if(!pr_info_0)
		return NULL;
	pr_info_1 = find_protect_info(rtk_heap, offset + size - 1, gen);
	if(!pr_info_1)
		return NULL;
	if(pr_info_0 != pr_info_1)
		return NULL;

	return pr_info_0;
}


static struct rtk_protect_ext_info *find_protect_ext_info(
				 struct rtk_heap *rtk_heap,
				 unsigned long offset, bool gen)
{
	struct rtk_protect_ext_info *rtk_protect_ext_info, *tmp, *ret = NULL;
	struct list_head *protect_list;

	unsigned long base;
	size_t size;

	protect_list = &rtk_heap->elist;

	list_for_each_entry_safe(rtk_protect_ext_info, tmp, protect_list,
					 list) {
		base = rtk_protect_ext_info->create_info.mem.base;
		size = rtk_protect_ext_info->create_info.mem.size;
		if (offset >= base && offset < base + size) {
			ret = rtk_protect_ext_info;
			pr_debug("%s base=0x%lx size=0x%lx \n", __func__,
						base, size);
			break;
		}
	}

	return ret;
}


static inline enum e_notifier_protect_ext _flag_to_notifier_protect_ext
					(unsigned long flags)
{
	enum e_notifier_protect_ext ret =
		(enum e_notifier_protect_ext) RTK_PROTECTED_EXT_GET(flags);
	return ret;
}


static int destroy_protect_ext_info(struct rtk_protect_ext_info *info)
{
	struct rtk_protect_ext_unset ext_unset;

	ext_unset.priv_virt = info->create_info.priv_virt;
	return rtk_protect_ext_unset(&ext_unset);
}

static struct rtk_protect_ext_info *create_protect_ext_info(unsigned long flags,
			 unsigned long base, unsigned long size,
			 struct rtk_protect_info *protect_info)
{
	struct rtk_protect_ext_info *ext_info;
	enum e_notifier_protect_ext ext;
	int ret;

	ext = _flag_to_notifier_protect_ext(flags);
	ext_info = (struct rtk_protect_ext_info *)kzalloc(
			sizeof(struct rtk_protect_ext_info), GFP_KERNEL);
	if (ext_info == NULL)
		goto out;

	ext_info->create_info.mem.ext = ext;
	ext_info->create_info.mem.base = base;
	ext_info->create_info.mem.size = size;

	ext_info->create_info.mem.priv = protect_info->create_info.priv_virt;

	pr_debug("%s base=0x%lx size=0x%lx \n", __func__,
						base, size);



	ret = rtk_protect_ext_set(&ext_info->create_info);
	if (ret) {
		kfree(ext_info);
		ext_info = NULL;
	}

out:
	return ext_info;
}

static struct rtk_heap_task *rtk_get_task(struct rtk_heap *heap, const char *comm)
{
	struct rtk_heap_task *task;

	list_for_each_entry(task, &heap->task_list, list) {
		if (!strcmp(task->comm, comm))
			return task;
	}

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		return NULL;

	strscpy(task->comm, comm, sizeof(task->comm));
	task->size = 0;
	INIT_LIST_HEAD(&task->alloc_list);
	list_add(&task->list, &heap->task_list);

	return task;
}

static void rtk_task_info_d(struct rtk_heap *heap,
		  unsigned long offset, size_t size,
		  const char *comm)
{
	struct rtk_heap_task *task;
	struct rtk_alloc *alloc, *tmp;
	bool found = false;

	task = rtk_get_task(heap, comm);
	if (!task) {
		WARN_ON(1);
		return;
	}

	list_for_each_entry_safe(alloc, tmp, &task->alloc_list, list) {
		if (alloc->start == offset &&
		    alloc->end == offset + size) {
			list_del(&alloc->list);
			kfree(alloc);
			found = true;
			break;
		}
	}

	if (!found) {
		WARN_ON(1);
		return;
	}

	task->size -= size;

	if (task->size == 0) {
		list_del(&task->list);
		kfree(task);
	}
}

static void rtk_task_info_a(struct rtk_heap *heap,
		  unsigned long offset, size_t size,
		  const char *caller)
{
	struct rtk_heap_task *task;
	struct rtk_alloc *alloc;

	task = rtk_get_task(heap, current->comm);
	if (!task)
		return;

	alloc = kzalloc(sizeof(*alloc), GFP_KERNEL);
	if (!alloc)
		return;

	alloc->start = offset;
	alloc->end   = offset + size;
	alloc->dev_name = heap->dev_name;

	if (caller)
		strscpy(alloc->ext_task, caller, sizeof(alloc->ext_task));
	else
		snprintf(alloc->ext_task, sizeof(alloc->ext_task),
			 "tgid-%d", current->tgid);

	list_add(&alloc->list, &task->alloc_list);
	task->size += size;
}


static int rtk_cheap_cmp_core(const struct rtk_heap *ha,
			      const struct rtk_heap *hb,
			      unsigned long req_pages)
{

	unsigned long used_a, used_b;
	unsigned long free_a = 0, free_b = 0;
	unsigned long diff_a, diff_b;
	unsigned long score_a = ULONG_MAX, score_b = ULONG_MAX;

	if (!rtk_toc_type(ha->flag)) {
		used_a = bitmap_weight(ha->alloc_bitmap,
			(int)cma_bitmap_maxno(ha->cma))
			<< ha->cma->order_per_bit;
		free_a = (ha->cma->count > used_a) ?
				 (ha->cma->count - used_a) : 0;
	}

	if (!rtk_toc_type(hb->flag)) {
		used_b = bitmap_weight(hb->alloc_bitmap,
			(int)cma_bitmap_maxno(hb->cma))
			<< hb->cma->order_per_bit;
		free_b = (hb->cma->count > used_b) ?
				 (hb->cma->count - used_b) : 0;
	}


	if (!req_pages) {
        	if (free_a < free_b) return -1;
        	if (free_a > free_b) return  1;
        	return 0;
    	}

	if (free_a < req_pages)
		score_a = ULONG_MAX;
	else {
		diff_a = free_a - req_pages;
		if (diff_a == 0)
			diff_a = 1;
		score_a = alpha * (diff_a * ilog2(diff_a)) +
				 beta * ha->penalty;
	}

	if (free_b < req_pages)
		score_b = ULONG_MAX;
	else {
		diff_b = free_b - req_pages;
		if (diff_b == 0)
			diff_b = 1;
		score_b = alpha * (diff_b * ilog2(diff_b)) +
				 beta * hb->penalty;
	}

	if (score_a < score_b)
		return -1;
	else if (score_a > score_b)
		return 1;
	else
		return 0;
}

#define DEFINE_BEST_CHEAP_CMP(_name, _member)                           \
static int _name(void *priv, const struct list_head *a,                \
		 const struct list_head *b)                             \
{                                                                       \
	unsigned long req_pages = priv ? *(unsigned long *)priv : 0;       \
	const struct rtk_heap *ha = container_of(a, struct rtk_heap, _member); \
	const struct rtk_heap *hb = container_of(b, struct rtk_heap, _member); \
	return rtk_cheap_cmp_core(ha, hb, req_pages);                      \
}

DEFINE_BEST_CHEAP_CMP(best_cheap_cmp, hlist)
DEFINE_BEST_CHEAP_CMP(best_cheap_low_cmp, hlist_low)


/******************************************************************************
 ******************************************************************************
 ******************************************************************************/
void rtk_pool_free(struct rtk_heap *rtk_heap, struct page *pages,
				size_t size, const char *name)
{
	unsigned long offset;
	struct rtk_protect_ext_info *protect_ext_info;

	pr_debug("%s(%pS)...\n", __func__,  __builtin_return_address(0));

	size = PAGE_ALIGN(size);
	offset = page_to_phys(pages);
	if (rtk_protected_type(rtk_heap->flag)) {
		protect_ext_info = find_protect_ext_info(rtk_heap,
						 offset, true);
		if (!protect_ext_info)
			goto free;

		if (destroy_protect_ext_info(protect_ext_info) != 0) {
			pr_err("%s destroy ext info error !\n", __func__);
			BUG();
		}
		list_del(&protect_ext_info->list);
		kfree(protect_ext_info);
	}

free:
	gen_pool_free(rtk_heap->gen_pool, PFN_PHYS(page_to_pfn(pages)), size);

	rtk_task_info_d(rtk_heap, offset, size, name);

	return;
}

static void rtk_gen_free(struct heap_helper_buffer *buffer)
{
	struct rtk_heap *rtk_heap = dma_heap_get_drvdata(
					buffer->heap_buffer.heap);
	struct dma_buf *dmabuf = buffer->heap_buffer.dmabuf;
	struct page *pages = buffer->priv_virt;

	mutex_lock(&rtk_heap->mutex);

	rtk_pool_free(rtk_heap, pages, buffer->heap_buffer.size,
			 dmabuf->exp_name);

	kfree(dmabuf->exp_name);
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	kfree(buffer);
	mutex_unlock(&rtk_heap->mutex);

	return ;

}

void rtk_normal_free(struct rtk_heap *rtk_heap, struct page *pages,
				size_t size, const char *name)
{
	struct cma *cma = rtk_heap->cma;
	unsigned long pfn, bitmap_no, bitmap_count;
	unsigned long nr_pages;
	unsigned long phy_addr;

	pr_debug("%s(%pS)...\n", __func__,  __builtin_return_address(0));

	size = PAGE_ALIGN(size);
	phy_addr = page_to_phys(pages);

	pfn = page_to_pfn(pages);
	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;
	nr_pages = size >> PAGE_SHIFT;

	bitmap_count = ALIGN(size >> PAGE_SHIFT, 1UL << cma->order_per_bit)
			 >> cma->order_per_bit;

	bitmap_clear(rtk_heap->use_bitmap, bitmap_no, bitmap_count);
	bitmap_clear(rtk_heap->alloc_bitmap, bitmap_no,
				 bitmap_count);
	cma_release(cma, pages, nr_pages);

	rtk_task_info_d(rtk_heap, phy_addr, size, name);

	return;
}

static void rtk_cma_free(struct heap_helper_buffer *buffer)
{
	struct rtk_heap *rtk_heap;
	struct dma_buf *dmabuf = buffer->heap_buffer.dmabuf;
	struct page *pages = buffer->priv_virt;
	size_t size = buffer->heap_buffer.size;

	if (buffer->priv_data != NULL)
		rtk_heap = (struct rtk_heap *)buffer->priv_data;
	else
		rtk_heap = dma_heap_get_drvdata(
					buffer->heap_buffer.heap);

	mutex_lock(&rtk_heap->mutex);
	rtk_normal_free(rtk_heap, pages, size, dmabuf->exp_name);

	kfree(dmabuf->exp_name);
	/* release sg table */
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	kfree(buffer);
	mutex_unlock(&rtk_heap->mutex);

	return ;


}

static void rtk_cheap_free(struct heap_helper_buffer *buffer)
{
	rtk_cma_free(buffer);
	return ;
}

void rtk_static_secure_free(struct rtk_heap *rtk_heap, struct page *pages,
				size_t size, const char *name)
{
	struct cma *cma = rtk_heap->cma;
	unsigned long pfn, offset, bitmap_no, bitmap_count;
	struct rtk_protect_ext_info *protect_ext_info;
	unsigned long phy_addr;


	pr_debug("%s(%pS)...\n", __func__,  __builtin_return_address(0));

	size = PAGE_ALIGN(size);

	phy_addr = page_to_phys(pages);
	pfn = page_to_pfn(pages);
	offset = page_to_phys(pages);
	protect_ext_info = find_protect_ext_info(rtk_heap, offset,
						 false);

	if (protect_ext_info) {
		if (destroy_protect_ext_info(protect_ext_info) != 0) {
			pr_err("%s destroy ext info error !\n", __func__);
			BUG();
		}
		list_del(&protect_ext_info->list);
		kfree(protect_ext_info);
	}
	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;

	bitmap_count = ALIGN(size >> PAGE_SHIFT, 1UL << cma->order_per_bit)
			 >> cma->order_per_bit;

	bitmap_clear(rtk_heap->use_bitmap, bitmap_no, bitmap_count);

	rtk_task_info_d(rtk_heap, phy_addr, size, name);


	return;
}

static void rtk_static_protect_cma_free(struct heap_helper_buffer *buffer)
{
	struct rtk_priv *rtk_priv = dma_heap_get_drvdata(
					buffer->heap_buffer.heap);
	struct rtk_heap *rtk_heap;
	struct dma_buf *dmabuf = buffer->heap_buffer.dmabuf;
	struct page *pages = buffer->priv_virt;
	size_t size = buffer->heap_buffer.size;

	rtk_heap = rtk_priv->rtk_heap;
	mutex_lock(&rtk_heap->mutex);

	rtk_static_secure_free(rtk_heap, pages, size, dmabuf->exp_name);

	kfree(dmabuf->exp_name);
	/* release sg table */
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	kfree(buffer);
	mutex_unlock(&rtk_heap->mutex);

	return ;
}

void rtk_dynamic_secure_free(struct rtk_heap *rtk_heap, struct page *pages,
				size_t len, const char *name)
{
	struct cma *cma = rtk_heap->cma;
	struct rtk_protect_info *rtk_protect_info;
	unsigned long pfn, offset, bitmap_no, bitmap_count, *tmp_bitmap;
	unsigned long nr_pages, bit_no, bit_count;
	struct protect_region *mem;
	struct rtk_protect_change_info c_info;
	struct rtk_protect_destroy_info d_info;
	struct rtk_protect_ext_info *protect_ext_info;
	unsigned long phy_addr;
	size_t size;
	int ret;

	size = PAGE_ALIGN(len);

	phy_addr = page_to_phys(pages);
	pfn = page_to_pfn(pages);
	offset = page_to_phys(pages);
	pr_debug("%s(%pS) offset=0x%lx size=0x%lx \n", __func__,
			 __builtin_return_address(0), offset, size);
	protect_ext_info = find_protect_ext_info(rtk_heap, offset,
						 false);

	if (protect_ext_info) {
		if (destroy_protect_ext_info(protect_ext_info) != 0) {
			pr_err("%s destroy ext info error !\n", __func__);
			BUG();
		}
		pr_debug("%s destroy protect_ext_info base=0x%lx size=0x%lx \n",
			__func__,
			protect_ext_info->create_info.mem.base,
			protect_ext_info->create_info.mem.size);
		list_del(&protect_ext_info->list);
		kfree(protect_ext_info);
	}
	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;

	bitmap_count = ALIGN(size >> PAGE_SHIFT, 1UL << cma->order_per_bit)
			 >> cma->order_per_bit;

	nr_pages = size >> PAGE_SHIFT;
	bitmap_clear(rtk_heap->use_bitmap, bitmap_no, bitmap_count);

	rtk_protect_info = find_protect_info(rtk_heap, offset,
						 false);

	if (!rtk_protect_info) {
		bitmap_clear(rtk_heap->alloc_bitmap, bitmap_no,
				 bitmap_count);
		cma_release(cma, pages, nr_pages);

		goto finished;
	}

	pfn =__phys_to_pfn(rtk_protect_info->create_info.mem.base);
	bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;

	size = rtk_protect_info->create_info.mem.size;
	bitmap_count = ALIGN(size >> PAGE_SHIFT, 1UL << cma->order_per_bit)
			 >> cma->order_per_bit;
	/* bitmap_no always align  */
	tmp_bitmap = rtk_heap->use_bitmap + bitmap_no/(BITS_PER_BYTE *
							 sizeof(unsigned long));
	/* check whole protect region */
	if (bitmap_empty(tmp_bitmap, bitmap_count)) {
		d_info.priv_virt = rtk_protect_info->create_info.priv_virt;
		ret = rtk_protect_destroy(&d_info);
		if (ret) {
			pr_err("%s:%d rtk_protect_destroy"
				"notify return ERROR! (priv_virt=%p)\n",
				 __func__, __LINE__, d_info.priv_virt);
			BUG();
		}
		pr_debug("%s clear whole region : base = 0x%lx size = 0x%lx \n",
				 __func__,
				rtk_protect_info->create_info.mem.base,
				rtk_protect_info->create_info.mem.size);

		list_del(&rtk_protect_info->list);
		kfree(rtk_protect_info);
		bitmap_clear(rtk_heap->alloc_bitmap, bitmap_no,
			 bitmap_count);

		pages = pfn_to_page(pfn);
		nr_pages = size >> PAGE_SHIFT;
		rtk_dma_remap(pages, size, PAGE_KERNEL);
		cma_release(cma, pages, nr_pages);

		goto finished;
	}

	mem = &rtk_protect_info->create_info.mem;

	bit_no = find_first_bit(tmp_bitmap, bitmap_count);
	bit_count = bit_no;
	size = ALIGN_DOWN(((bit_count) << cma->order_per_bit) << PAGE_SHIFT,
			SZ_2M);
	/* check upper part of protect region */
	if (size) {
		c_info.mem.base = mem->base + size;
		c_info.mem.size = mem->size - size;
		c_info.mem.type = (enum e_notifier_protect_type)
				RTK_PROTECTED_TYPE_GET(rtk_heap->flag);
		c_info.priv_virt = rtk_protect_info->
					create_info.priv_virt;
		ret = rtk_protect_change(&c_info);
		if (ret) {
			pr_err("%s:%d rtk_protect_change"
				"notify return ERROR! (priv_virt=%p)\n",
				 __func__, __LINE__, c_info.priv_virt);
			BUG();
		}

		pr_debug("%s clear(0x%lx)hi region: from base/size = 0x%lx/0x%lx "
			"to base/size = 0x%lx/0x%lx \n", __func__, size,
				rtk_protect_info->create_info.mem.base,
				rtk_protect_info->create_info.mem.size,
				c_info.mem.base, c_info.mem.size);

		mem->base = c_info.mem.base;
		mem->size = c_info.mem.size;

		bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;
		bitmap_count = ALIGN(size >> PAGE_SHIFT,
			1UL << cma->order_per_bit)>> cma->order_per_bit;

		bitmap_clear(rtk_heap->alloc_bitmap,
					 bitmap_no, bitmap_count);

		pages = pfn_to_page(pfn);
		nr_pages = size >> PAGE_SHIFT;
		rtk_dma_remap(pages, size, PAGE_KERNEL);
		cma_release(cma, pages, nr_pages);

		goto finished;

	}

	bit_no = find_last_bit(tmp_bitmap, bitmap_count);
	if (bit_no == bitmap_count)
		bit_count = bitmap_count;
	else
		bit_count = bitmap_count - bit_no - 1;
	size = ALIGN_DOWN((bit_count << cma->order_per_bit) << PAGE_SHIFT,
			SZ_2M);

	/* check lower part of protect region */
	if (size) {
		c_info.mem.base = mem->base;
		/* mem.size should be bigger than size */
		c_info.mem.size = mem->size - size;
		c_info.mem.type = (enum e_notifier_protect_type)
				RTK_PROTECTED_TYPE_GET(rtk_heap->flag);
		c_info.priv_virt = rtk_protect_info->
					create_info.priv_virt;
		ret = rtk_protect_change(&c_info);
		if (ret) {
			pr_err("%s:%d rtk_protect_change"
				" return ERROR! (priv_virt=%p)\n",
				 __func__, __LINE__, c_info.priv_virt);
			BUG();
		}

		pr_debug("%s clear(0x%lx)lo region: from base/size = 0x%lx/0x%lx "
			"to base/size = 0x%lx/0x%lx \n", __func__, size,
				rtk_protect_info->create_info.mem.base,
				rtk_protect_info->create_info.mem.size,
				c_info.mem.base, c_info.mem.size);

		mem->base = c_info.mem.base;
		mem->size = c_info.mem.size;

		pfn =__phys_to_pfn(mem->base + mem->size);
		bitmap_no = (pfn - cma->base_pfn) >> cma->order_per_bit;

		bitmap_count = ALIGN(size >> PAGE_SHIFT,
			1UL << cma->order_per_bit)>> cma->order_per_bit;

		bitmap_clear(rtk_heap->alloc_bitmap, bitmap_no,
				bitmap_count);

		pages = pfn_to_page(pfn);
		nr_pages = size >> PAGE_SHIFT;
		rtk_dma_remap(pages, size, PAGE_KERNEL);
		cma_release(cma, pages, nr_pages);


		goto finished;
	}

finished:
	size = PAGE_ALIGN(len);
	rtk_task_info_d(rtk_heap, phy_addr, size, name);

	return;
}

static void rtk_dynamic_protect_cma_free(struct heap_helper_buffer *buffer)
{
	struct rtk_priv *rtk_priv = dma_heap_get_drvdata(
					buffer->heap_buffer.heap);
	struct rtk_heap *rtk_heap;
	struct dma_buf *dmabuf = buffer->heap_buffer.dmabuf;
	struct page *pages = buffer->priv_virt;
	size_t size = buffer->heap_buffer.size;

	rtk_heap = rtk_priv->rtk_heap;
	mutex_lock(&rtk_heap->mutex);
	size = buffer->heap_buffer.size;

	rtk_dynamic_secure_free(rtk_heap, pages, size, dmabuf->exp_name);

	kfree(dmabuf->exp_name);
	/* release sg table */
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	kfree(buffer);
	mutex_unlock(&rtk_heap->mutex);

	return ;
}



static struct page *rtk_cma_alloc(struct rtk_heap *rtk_heap, size_t count,
			unsigned int align)
{
	unsigned long *ref_bitmap;
	struct cma *cma = rtk_heap->cma;
	unsigned long pfn, bitmap_count, bitmap_size;
	unsigned long mask, offset, start = 0;
	struct page *page = NULL;
	struct rtk_protect_info *pr_info;
	int bit_id;


	if (bitmap_empty(rtk_heap->alloc_bitmap, rtk_heap->nbits))
		return NULL;

	mask = (align <= cma->order_per_bit) ? 0 :
		 (1UL << (align - cma->order_per_bit)) - 1;
        offset = (cma->base_pfn & ((1UL << align) - 1)) >> cma->order_per_bit;
	bitmap_count = ALIGN(count, 1UL << cma->order_per_bit)
				 >> cma->order_per_bit;

	bitmap_size = rtk_heap->nbits;

	ref_bitmap = bitmap_zalloc(bitmap_size, GFP_KERNEL);

	bitmap_complement(ref_bitmap, rtk_heap->alloc_bitmap,
				bitmap_size);
	bitmap_or(ref_bitmap, ref_bitmap, rtk_heap->use_bitmap,
			bitmap_size);

	for (;;) {
		bit_id = bitmap_find_next_zero_area_off(ref_bitmap,
				bitmap_size, start, bitmap_count, mask, offset);

		if (bit_id > bitmap_size)
			goto out;

		pfn = cma->base_pfn + (bit_id << cma->order_per_bit);
		/* have to be in unique protect region */
		pr_info = find_protect_info_in(rtk_heap, PFN_PHYS(pfn),
				count << PAGE_SHIFT, 0);
		if (pr_info)
			break;
		start = bit_id + mask + 1;
		pr_debug("%s find next area bit_id = 0x%x , start =0x%lx \n",
				__func__, bit_id, start);
	}

	bitmap_set(rtk_heap->use_bitmap, bit_id, bitmap_count);
	rtk_cma_record_max_usage(rtk_heap);
	pfn = cma->base_pfn + (bit_id << cma->order_per_bit);
	page = pfn_to_page(pfn);
out:
	bitmap_free(ref_bitmap);
	return page;

}

static int rtk_adjust_protect_area(struct rtk_heap *rtk_heap,
			struct page *pages, unsigned long nr_pages)
{
	struct rtk_protect_info *rtk_protect_info, *tmp;
	struct rtk_protect_change_info info;
	unsigned long base = page_to_phys(pages);
	unsigned long limit = base + (nr_pages << PAGE_SHIFT);
	struct protect_region *mem;
	int ret = 0;

	list_for_each_entry_safe(rtk_protect_info, tmp, &rtk_heap->list,
				 list) {
		mem = &rtk_protect_info->create_info.mem;
		if (limit == mem->base) {
			info.mem.base = base;
			info.mem.size = mem->size + (nr_pages << PAGE_SHIFT);
			info.mem.type = (enum e_notifier_protect_type)
				RTK_PROTECTED_TYPE_GET(rtk_heap->flag);
			info.priv_virt =
					rtk_protect_info->create_info.priv_virt;
			ret = rtk_protect_change(&info);
			if (ret) {
				pr_err("%s:%d rtk_protect_change "
				"return ERROR! (priv_virt=%p)\n",__func__,
				 __LINE__, info.priv_virt);
				continue;
			}

			pr_debug("%s change from base/size = 0x%lx/0x%lx "
				"to base/size = 0x%lx/0x%lx \n", __func__,
				mem->base, mem->size,
				info.mem.base, info.mem.size);

			mem->base = info.mem.base;
			mem->size = info.mem.size;

			ret = 0;
			goto out;
		}
		else if (base == mem->base + mem->size) {
			info.mem.base = mem->base;
			info.mem.size = limit - mem->base;
			info.mem.type = (enum e_notifier_protect_type)
				RTK_PROTECTED_TYPE_GET(rtk_heap->flag);
			info.priv_virt = rtk_protect_info->create_info.priv_virt;
			ret = rtk_protect_change(&info);
			if (ret) {
				pr_err("%s:%d rtk_protect_change "
				" return ERROR! (priv_virt=%p)\n",__func__,
				 __LINE__, info.priv_virt);
				continue;
			}

			pr_debug("%s change from base/size = 0x%lx/0x%lx "
				"to base/size = 0x%lx/0x%lx \n", __func__,
				mem->base, mem->size,
				info.mem.base, info.mem.size);

			mem->base = info.mem.base;
			mem->size = info.mem.size;

			ret = 0;
			goto out;
		}
	} /* list_for_each_entry_safe */

	rtk_protect_info = kzalloc(sizeof(struct rtk_protect_info),
				GFP_KERNEL);
	if (!rtk_protect_info) {
		pr_err("%s:%d ERROR!\n", __func__, __LINE__);
		ret = -EINVAL;
		goto out;
	}

	rtk_protect_info->create_info.mem.base = base;
	rtk_protect_info->create_info.mem.size = nr_pages << PAGE_SHIFT;
	rtk_protect_info->create_info.mem.type = (enum e_notifier_protect_type)
				RTK_PROTECTED_TYPE_GET(rtk_heap->flag);

	ret = rtk_protect_create(&rtk_protect_info->create_info);
	if (ret) {
		kfree(rtk_protect_info);
		pr_err("%s:%d rtk_protect_create return ERROR!"
			" (priv_virt=??)\n", __func__, __LINE__);
		ret = -EINVAL;
		goto out;
	}
	pr_debug("%s rtk_protect_create_notify  base =0x%lx size =0x%lx \n",
				__func__, base, nr_pages << PAGE_SHIFT);

	pr_debug("%s create from base/size = 0x%lx/0x%lx "
		"\n", __func__,
		rtk_protect_info->create_info.mem.base,
		rtk_protect_info->create_info.mem.size);


	list_add(&rtk_protect_info->list, &rtk_heap->list);

out:
	return ret;
}


static void pages_clear(struct page *pages, unsigned long nr_pages, bool gen)
{
	size_t size = nr_pages << PAGE_SHIFT;

	if (PageHighMem(pages)) {
		unsigned long nr_clear_pages = nr_pages;
		struct page *page = pages;
		while (nr_clear_pages > 0) {
			void *vaddr = kmap_atomic(page);

			memset(vaddr, 0, PAGE_SIZE);
			kunmap_atomic(vaddr);
			page++;
			nr_clear_pages--;
		}
		pr_debug("%s of cma heap of high mem \n", __func__);
	} else {
		memset(page_address(pages), 0, size);
	}

}

static struct dma_buf *dma_buf_allocate(struct dma_heap *heap, unsigned long len,
		struct page *pages, void *free_func,
		unsigned long flags, bool uncached)
{
	struct heap_helper_buffer *helper_buffer;
	struct sg_table *table;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct dma_buf *dmabuf;
	int ret = 0;
	size_t size;

	size = len;

	helper_buffer = kzalloc(sizeof(*helper_buffer), GFP_KERNEL);
	if (!helper_buffer) {
		dmabuf = ERR_PTR(-ENOMEM);
		goto out;
	}

	INIT_HEAP_HELPER_BUFFER(helper_buffer, free_func);
	helper_buffer->heap_buffer.flags = flags;
	helper_buffer->heap_buffer.heap = heap;
	helper_buffer->heap_buffer.size = len;
	helper_buffer->uncached = uncached;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto free_buf;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_table;
	sg_set_page(table->sgl, pages, size, 0);

	/* create the dmabuf */
	exp_info.ops = &heap_helper_ops;
	exp_info.size = len;
	exp_info.flags = O_RDWR;
	exp_info.priv = &helper_buffer->heap_buffer;
	exp_info.exp_name = (char *)kzalloc(TASK_COMM_LEN, GFP_KERNEL);
	strncpy((char *)exp_info.exp_name, current->comm, TASK_COMM_LEN);
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR_OR_NULL(dmabuf)) {
		goto free_table;
	}

	helper_buffer->heap_buffer.dmabuf = dmabuf;
	helper_buffer->priv_virt = pages;
	helper_buffer->sg_table = table;

out:
	return dmabuf;
free_table:
	kfree(table);
free_buf:
	kfree(helper_buffer);
	pr_err("%s error \n", __func__);
	return ERR_PTR(-ENOMEM);
}

static void rtk_heap_set_dmabuf_name(struct rtk_heap *rtk_heap,
					 struct dma_buf *dmabuf)
{
	char combined[TASK_COMM_LEN * 2];
	char *hname;

	if (rtk_heap->cma && rtk_heap->cma->name[0])
		hname = rtk_heap->cma->name;
	else if (rtk_heap->name)
		hname = rtk_heap->name;
	else
		hname = "rtk_heap";

	snprintf(combined, sizeof(combined), "%s@%s",
			 hname, current->comm);
	dma_buf_set_name(dmabuf, combined);
}

struct page *rtk_dynamic_secure_allocate(struct rtk_heap *rtk_heap,
					size_t len, unsigned long flags, char *caller)
{
	struct cma *cma = rtk_heap->cma;
	struct rtk_protect_info *rtk_protect_info;
	struct rtk_protect_ext_info *rtk_protect_ext_info;
	unsigned long offset;
	struct page *pages = NULL;
	unsigned long pfn, nr_pages, bitmap_count, align;
	size_t size, p_size;
	int bit_id;

	pr_debug("%s(%pS)...\n", __func__,  __builtin_return_address(0));

	size = PAGE_ALIGN(len);
	nr_pages = size >> PAGE_SHIFT;
	align = 0;

	if (rtk_protected_type(flags)) {
		pages = rtk_cma_alloc(rtk_heap, nr_pages, align);
		if (!pages) {
			align = get_order(SZ_2M);
			p_size = ALIGN(len, SZ_2M);
			nr_pages = p_size >> PAGE_SHIFT;
			pages = cma_alloc(cma, nr_pages, align, GFP_KERNEL);
			if (!pages)
				goto out;

			bitmap_count = ALIGN(nr_pages,
					 1UL << cma->order_per_bit)
					 >> cma->order_per_bit;
			pfn = __page_to_pfn(pages);
			bit_id = (pfn - cma->base_pfn) >> cma->order_per_bit;
			bitmap_set(rtk_heap->alloc_bitmap, bit_id,
					 bitmap_count);

			dma_sync_single_for_device(
					dma_heap_get_dev(rtk_heap->heap),
					page_to_phys(pages), p_size
					, DMA_BIDIRECTIONAL);

			rtk_dma_remap(pages, p_size, pgprot_dmacoherent(PAGE_KERNEL));

			if (rtk_adjust_protect_area(rtk_heap,
						 pages, nr_pages)) {
				BUG();
				goto out;
			}

			size = PAGE_ALIGN(len);
			nr_pages = size >> PAGE_SHIFT;
			align = 0;
			pages = rtk_cma_alloc(rtk_heap, nr_pages, align);
			if (!pages)
				goto out;
		}

		offset = page_to_phys(pages);
		pr_debug("%s(%pS) secure offset=0x%lx size=0x%lx\n", __func__,
			 __builtin_return_address(0), offset, size);

		if (!rtk_protected_ext_type(rtk_heap->flag) &&
			rtk_protected_ext_type(flags)) {

			rtk_protect_info = find_protect_info(
					(void *)rtk_heap, offset, false);
			if (!rtk_protect_info) {
				pr_err("%s %d rtk_protect_info = 0x%p\n",
					 __func__, __LINE__, rtk_protect_info);
				BUG();
			}

			rtk_protect_ext_info = create_protect_ext_info(flags,
					 offset, size, rtk_protect_info);

			if (rtk_protect_ext_info == NULL) {
				/* TODO : free cma ; free rtk cma*/
				BUG();
				goto out;
			}
			pr_debug("%s create protect_ext_info base=0x%lx"
				" size=0x%lx \n",
				__func__,
				rtk_protect_ext_info->create_info.mem.base,
				rtk_protect_ext_info->create_info.mem.size);

			list_add(&rtk_protect_ext_info->list,
					 &rtk_heap->elist);

		}

	} else {
		pages = cma_alloc(cma, nr_pages, align, GFP_KERNEL);
		if (!pages)
			goto out;

		offset = page_to_phys(pages);
		bitmap_count = ALIGN(nr_pages, 1UL << cma->order_per_bit)
				 >> cma->order_per_bit;
		pfn = __page_to_pfn(pages);
		bit_id = (pfn - cma->base_pfn) >> cma->order_per_bit;
		bitmap_set(rtk_heap->alloc_bitmap, bit_id, bitmap_count);

		bitmap_set(rtk_heap->use_bitmap, bit_id, bitmap_count);
		rtk_cma_record_max_usage(rtk_heap);
		pr_debug("%s(%pS) no secure offset=0x%llx size=0x%lx\n",
			 __func__, __builtin_return_address(0),
			 page_to_phys(pages), size);
		if (!is_rtk_skip_zero(flags))
			pages_clear(pages, nr_pages, 0);
		dma_sync_single_for_device(
				dma_heap_get_dev(rtk_heap->heap),
				page_to_phys(pages), size
				, DMA_BIDIRECTIONAL);
	}

	rtk_task_info_a(rtk_heap, offset, size, caller);

out:
	return pages;
}

static struct dma_buf *rtk_dyn_protect_cma_do_allocate(struct dma_heap *heap,
	size_t size, unsigned long flags, bool uncached)
{
	struct rtk_priv *rtk_priv = dma_heap_get_drvdata(heap);
	struct rtk_heap *rtk_heap;
	struct page *pages;
	struct dma_buf *ret = ERR_PTR(-ENOMEM);

	rtk_heap = rtk_priv->rtk_heap;

	mutex_lock(&rtk_heap->mutex);
	rtk_heap->dev_name = dma_heap_get_name(heap);
	size = PAGE_ALIGN(size);
	pages = rtk_dynamic_secure_allocate(rtk_heap, size, flags, NULL);
	if (!pages)
		goto out;

	ret = dma_buf_allocate(heap, size, pages,
		 	rtk_dynamic_protect_cma_free, flags, uncached);
	if (IS_ERR_OR_NULL(ret)) {
		BUG();
		goto out;
	}
	rtk_heap_set_dmabuf_name(rtk_heap, ret);

out:
	mutex_unlock(&rtk_heap->mutex);

	return ret;

}


struct page *rtk_normal_allocate(struct rtk_heap *rtk_heap,
					size_t size, unsigned long flags, char *caller)
{
	struct page *pages = NULL;
	struct cma *cma = rtk_heap->cma;
	unsigned long pfn, nr_pages, bitmap_count, align;
	int bit_id;
	unsigned long offset;

	pr_debug("%s(%pS)...\n", __func__,  __builtin_return_address(0));

	size = PAGE_ALIGN(size);
	align = 0;
	nr_pages = size >> PAGE_SHIFT;

	pages = cma_alloc(cma, nr_pages, align, GFP_KERNEL);
	if (!pages)
		goto out;
	if (!is_rtk_skip_zero(flags))
		pages_clear(pages, nr_pages, 0);
	dma_sync_single_for_device(
			dma_heap_get_dev(rtk_heap->heap),
			page_to_phys(pages), size
			, DMA_BIDIRECTIONAL);

	bitmap_count = ALIGN(nr_pages, 1UL << cma->order_per_bit)
				 >> cma->order_per_bit;
	pfn = __page_to_pfn(pages);
	offset = page_to_phys(pages);

	bit_id = (pfn - cma->base_pfn) >> cma->order_per_bit;

	bitmap_set(rtk_heap->alloc_bitmap, bit_id, bitmap_count);
	bitmap_set(rtk_heap->use_bitmap, bit_id, bitmap_count);
	rtk_cma_record_max_usage(rtk_heap);
	rtk_task_info_a(rtk_heap, offset, size, caller);

out:
	return pages;

}

static struct dma_buf *rtk_cma_do_allocate(struct dma_heap *heap,
	unsigned long len, unsigned long flags, bool uncached)
{
	struct rtk_heap *rtk_heap = dma_heap_get_drvdata(heap);
	struct page *pages;
	struct dma_buf *ret = ERR_PTR(-ENOMEM);
	size_t size;

	mutex_lock(&rtk_heap->mutex);
	rtk_heap->dev_name = dma_heap_get_name(heap);
	size = PAGE_ALIGN(len);
	pages = rtk_normal_allocate(rtk_heap, size, flags, NULL);

	if (!pages)
		goto out;

	ret = dma_buf_allocate(heap, size, pages, rtk_cma_free,
				 flags, uncached);
	if (IS_ERR_OR_NULL(ret)) {
		BUG();
		goto out;
	}
	rtk_heap_set_dmabuf_name(rtk_heap, ret);
out:
	mutex_unlock(&rtk_heap->mutex);
	return ret;

}

struct page *rtk_static_secure_allocate(struct rtk_heap *rtk_heap,
					size_t size, unsigned long flags, char *caller)
{
	struct rtk_protect_info *rtk_protect_info;
	struct rtk_protect_ext_info *rtk_protect_ext_info;
	struct page *pages = NULL;
	unsigned long nr_pages, align;
	unsigned long offset;

	pr_debug("%s(%pS)...\n", __func__,  __builtin_return_address(0));

	size = PAGE_ALIGN(size);
	align = 0;
	nr_pages = size >> PAGE_SHIFT;

	pages = rtk_cma_alloc(rtk_heap, nr_pages, align);

	if (!pages)
		goto out;

	offset = page_to_phys(pages);
	/* this function is only static protect */
	if (rtk_protected_type(rtk_heap->flag) &&
			!rtk_protected_ext_type(rtk_heap->flag) &&
			rtk_protected_ext_type(flags)) {

		rtk_protect_info = find_protect_info((void *)rtk_heap,
						 offset, false);
		rtk_protect_ext_info = create_protect_ext_info(flags, offset,
						 size, rtk_protect_info);

		if (rtk_protect_ext_info == NULL) {
			/* TODO : free cma ; free rtk cma*/
			pages = NULL;
			BUG();
			goto out;
		}
		list_add(&rtk_protect_ext_info->list, &rtk_heap->elist);
	}

	rtk_task_info_a(rtk_heap, offset, size, caller);

out:
	return pages;

}

static struct dma_buf *rtk_stc_cma_do_allocate(struct dma_heap *heap,
	unsigned long len, unsigned long flags, bool uncached)
{
	struct rtk_priv *rtk_priv = dma_heap_get_drvdata(heap);
	struct rtk_heap *rtk_heap;
	struct page *pages;
	struct dma_buf *ret = ERR_PTR(-ENOMEM);
	size_t size;

	rtk_heap = rtk_priv->rtk_heap;

	mutex_lock(&rtk_heap->mutex);

	rtk_heap->dev_name = dma_heap_get_name(heap);
	size = PAGE_ALIGN(len);
	pages = rtk_static_secure_allocate(rtk_heap, size, flags, NULL);
	if (!pages)
		goto out;

	ret = dma_buf_allocate(heap, size, pages, rtk_static_protect_cma_free,
				 flags, uncached);
	if (IS_ERR_OR_NULL(ret)) {
		BUG();
		goto out;
	}
	rtk_heap_set_dmabuf_name(rtk_heap, ret);
out:
	mutex_unlock(&rtk_heap->mutex);
	return ret;

}

struct page *rtk_pool_allocate(struct rtk_heap *rtk_heap,
					size_t size, unsigned long flags, char *caller)
{
	struct rtk_protect_info *rtk_protect_info;
	struct rtk_protect_ext_info *rtk_protect_ext_info;
	struct page *pages = NULL;
	unsigned long offset;

	pr_debug("%s(%pS)...\n", __func__,  __builtin_return_address(0));

	size = PAGE_ALIGN(size);

	offset = gen_pool_alloc(rtk_heap->gen_pool, (size_t)size);

	if (!offset)
		goto out;

	pages = pfn_to_page(__phys_to_pfn(offset));
	if (!is_rtk_static_protect(rtk_heap->flag) && (
		rtk_heap->flag & RTK_FLAG_SCPUACC)) {
		unsigned long nr_pages = size >> PAGE_SHIFT;
		if (!is_rtk_skip_zero(flags))
			pages_clear(pages, nr_pages, 1);
		dma_sync_single_for_device(
				dma_heap_get_dev(rtk_heap->heap),
				page_to_phys(pages), size
				, DMA_BIDIRECTIONAL);
	}

	/* gen heap is only static protect or free protect */
	if (rtk_protected_type(rtk_heap->flag) &&
		!rtk_protected_ext_type(rtk_heap->flag) &&
		rtk_protected_ext_type(flags)) {

		rtk_protect_info = find_protect_info((void *)
					rtk_heap, offset, true);
		rtk_protect_ext_info = create_protect_ext_info(
				 flags, offset, size, rtk_protect_info);

		if (rtk_protect_ext_info == NULL) {
			gen_pool_free(rtk_heap->gen_pool, offset,
					 size);
			pages = NULL;
			goto out;
		}
		list_add(&rtk_protect_ext_info->list, &rtk_heap->elist);
	}

	rtk_task_info_a(rtk_heap, offset, size, caller);

out:
	return pages;

}

static struct dma_buf *rtk_gen_do_allocate(struct dma_heap *heap,
			 unsigned long len, unsigned long flags, bool uncached)
{
	struct rtk_heap *rtk_heap = dma_heap_get_drvdata(heap);
	struct page *pages;
	unsigned long offset;
	size_t size;
	struct dma_buf *ret = ERR_PTR(-ENOMEM);

	mutex_lock(&rtk_heap->mutex);

	size = PAGE_ALIGN(len);
	rtk_heap->dev_name =  dma_heap_get_name(heap);

	pages = rtk_pool_allocate(rtk_heap, size, flags, NULL);
	if (!pages)
		goto out;

	rtk_gen_record_max_usage(rtk_heap);
	ret = dma_buf_allocate(heap, size, pages, rtk_gen_free, flags,
				uncached);
	if (IS_ERR_OR_NULL(ret)) {
		BUG();
		gen_pool_free(rtk_heap->gen_pool, offset, size);
		goto out;
	}
	rtk_heap_set_dmabuf_name(rtk_heap, ret);
out:
	mutex_unlock(&rtk_heap->mutex);

	return ret;
}

static struct dma_buf *rtk_cheap_do_allocate(struct dma_heap *heap,
	 unsigned long len, u64 flags, bool uncached)
{
	struct dma_buf *dmabuf = ERR_PTR(-EBUSY);
	struct rtk_heap *cheap;
	struct page *pages = NULL;
	struct dma_heap_buffer *heap_buffer;
	struct heap_helper_buffer *buffer;
	unsigned long req_pages = PAGE_ALIGN(len) >> PAGE_SHIFT;
	size_t size;
	struct list_head *heap_list;
	bool is_low = false;

	heap_list = dma_heap_get_drvdata(heap);
	if (!heap_list)
		return ERR_PTR(-EINVAL);

	if (heap_list == &cheap_low_list)
		is_low = true;

	mutex_lock(&chplist_mutex);

	pr_debug("-------------------------------------\n");

	if (is_low) {

		list_sort(&req_pages, heap_list, best_cheap_low_cmp);
		list_for_each_entry(cheap, heap_list, hlist_low)
			pr_debug("cheap(low): %s\n", cheap->cma->name);
	} else {
		list_sort(&req_pages, heap_list, best_cheap_cmp);
		list_for_each_entry(cheap, heap_list, hlist)
			pr_debug("cheap: %s\n", cheap->cma->name);
	}

	/* iterate selected list */
	if (is_low) {

		list_for_each_entry(cheap, heap_list, hlist_low) {

			if (!(cheap->flag & RTK_FLAG_CMA))
				continue;

			if (is_rtk_static_protect(cheap->flag))
				continue;

			if (cheap->penalty == ULONG_MAX)
				continue;

			mutex_lock(&cheap->mutex);

			size  = PAGE_ALIGN(len);
			cheap->dev_name = dma_heap_get_name(heap);
			pages = rtk_normal_allocate(cheap, size, flags, NULL);

			mutex_unlock(&cheap->mutex);

			if (pages)
				goto success;
		}

	} else {

		list_for_each_entry(cheap, heap_list, hlist) {

			if (!(cheap->flag & RTK_FLAG_CMA))
				continue;

			if (is_rtk_static_protect(cheap->flag))
				continue;

			if (cheap->penalty == ULONG_MAX)
				continue;

			mutex_lock(&cheap->mutex);

			size  = PAGE_ALIGN(len);
			pages = rtk_normal_allocate(cheap, size, flags, NULL);

			mutex_unlock(&cheap->mutex);

			if (pages)
				goto success;
		}
	}

	mutex_unlock(&chplist_mutex);
	return dmabuf;

success:

	dmabuf = dma_buf_allocate(heap, size, pages,
				  rtk_cheap_free,
				  flags,
				  uncached);

	BUG_ON(IS_ERR_OR_NULL(dmabuf));

	rtk_heap_set_dmabuf_name(cheap, dmabuf);

	heap_buffer = dmabuf->priv;
	buffer = to_helper_buffer(heap_buffer);
	buffer->priv_data = (void *)cheap;

	mutex_unlock(&chplist_mutex);
	return dmabuf;
}

/*****************************************************************************
 ****************************************************************************/

static struct dma_buf *rtk_dynamic_protect_cma_allocate(struct dma_heap *heap,
	unsigned long len, u32 flags, u64 heap_flags)
{
	unsigned long _flag;
	struct rtk_priv *rtk_priv = dma_heap_get_drvdata(heap);

	_flag = rtk_priv->rtk_heap->flag | rtk_priv->ext_flag;

	return rtk_dyn_protect_cma_do_allocate(heap, len, _flag, false);
}

static struct dma_buf *rtk_static_protect_cma_allocate(struct dma_heap *heap,
	unsigned long len, u32 flags, u64 heap_flags)
{
	unsigned long _flag;
	struct rtk_priv *rtk_priv = dma_heap_get_drvdata(heap);

	_flag = rtk_priv->rtk_heap->flag | rtk_priv->ext_flag;

	return rtk_stc_cma_do_allocate(heap, len, _flag, false);
}

static struct dma_buf *rtk_cma_allocate(struct dma_heap *heap,
	 unsigned long len, u32 flags, u64 heap_flags)
{
	return rtk_cma_do_allocate(heap, len, heap_flags, false);
}

static struct dma_buf *rtk_uncached_cma_allocate(struct dma_heap *heap,
	 unsigned long len, u32 flags, u64 heap_flags)
{
	return rtk_cma_do_allocate(heap, len, heap_flags, true);
}

static struct dma_buf *rtk_gen_allocate(struct dma_heap *heap,
	 unsigned long len, u32 flags, u64 heap_flags)
{
	return rtk_gen_do_allocate(heap, len, heap_flags, false);
}

static struct dma_buf *rtk_uncached_gen_allocate(struct dma_heap *heap,
	 unsigned long len, u32 flags, u64 heap_flags)
{
	return rtk_gen_do_allocate(heap, len, heap_flags, true);
}

static struct dma_buf *rtk_static_protect_gen_allocate(struct dma_heap *heap,
	unsigned long len, u32 flags, u64 heap_flags)
{
	return rtk_gen_do_allocate(heap, len, heap_flags, false);
}

static struct dma_buf *rtk_cheap_allocate(struct dma_heap *heap,
	 unsigned long len, u32 flags, u64 heap_flags)
{
	return rtk_cheap_do_allocate(heap, len, heap_flags, false);

}

static struct dma_buf *rtk_uncached_cheap_allocate(struct dma_heap *heap,
	 unsigned long len, u32 flags, u64 heap_flags)
{
	return rtk_cheap_do_allocate(heap, len, heap_flags, true);

}

static struct dma_buf *rtk_not_initialized(struct dma_heap *heap,
				unsigned long len, u32 flags, u64 heap_flags)
{
	return ERR_PTR(-EBUSY);
}

/******************************************************************************
 * dma_heap_ops
 ******************************************************************************/

static struct dma_heap_ops dynamic_protect_cma_ops = {
	.allocate = rtk_dynamic_protect_cma_allocate,
};

static struct dma_heap_ops static_protect_cma_ops = {
	.allocate = rtk_static_protect_cma_allocate,
};


static struct dma_heap_ops cma_ops = {
	.allocate = rtk_cma_allocate,
};

static struct dma_heap_ops uncached_cma_ops = {
	.allocate = rtk_not_initialized,
};

static struct dma_heap_ops static_protect_gen_ops = {
	.allocate = rtk_static_protect_gen_allocate,
};

static struct dma_heap_ops gen_ops = {
	.allocate = rtk_gen_allocate,
};

static struct dma_heap_ops uncached_gen_ops = {
	.allocate = rtk_not_initialized,
};

static struct dma_heap_ops cheap_ops = {
	.allocate = rtk_cheap_allocate,
};

static struct dma_heap_ops uncached_cheap_ops = {
	.allocate = rtk_not_initialized,
};


static struct rtk_heap *alloc_rtk_heap(struct cma *cma, u32 cma_flags)
{
	struct rtk_heap *rtk_heap;
	int bitmap_size;

	rtk_heap = kzalloc(sizeof(struct rtk_heap), GFP_KERNEL);
	if (!rtk_heap) {
		pr_err("%s kzalloc err \n", __func__);
		return NULL;
	}

	rtk_heap->cma = cma;
	bitmap_size = cma->count >> cma->order_per_bit;
	rtk_heap->alloc_bitmap = bitmap_zalloc(bitmap_size, GFP_KERNEL);
	rtk_heap->use_bitmap = bitmap_zalloc(bitmap_size, GFP_KERNEL);
	rtk_heap->nbits = cma->count >> cma->order_per_bit;
	rtk_heap->flag = cma_flags;

	return rtk_heap;

};

static inline bool rtk_heap_is_low_1g(const struct rtk_heap *rtk_heap)
{
	struct cma *cma;

	if (!rtk_heap)
		return false;

	cma = rtk_heap->cma;
	if (!cma)
		return false;

	return PFN_PHYS(cma->base_pfn + cma->count) <= SZ_1G;
}

static void *rtk_dynamic_proctect_cma_create(struct cma *cma,
				struct rtk_heap_data *r_data, u32 cma_flags)
{
	struct rtk_heap *rtk_heap;
	struct dma_heap_export_info exp_info;
	struct dma_heap *dma_heap;
	struct rtk_priv *rtk_priv;
	int i;

	rtk_heap = alloc_rtk_heap(cma, cma_flags);
	if (rtk_heap == NULL) {
		return NULL;
	}
	INIT_LIST_HEAD(&rtk_heap->task_list);

	for ( i = 0; i < r_data->num_devdata; i++) {
		rtk_priv = kzalloc(sizeof(struct rtk_priv), GFP_KERNEL);
		if (!rtk_priv) {
			pr_err("%s kzalloc err \n", __func__);
			return NULL;
		}
		rtk_priv->rtk_heap = rtk_heap;
		rtk_priv->ext_flag = r_data->devdata[i].ext_flag;

		exp_info.name = r_data->devdata[i].devname;
		exp_info.ops =  &dynamic_protect_cma_ops;
		exp_info.priv = rtk_priv;

		dma_heap = dma_heap_add(&exp_info);
		if (IS_ERR(dma_heap)) {
			kfree(rtk_heap);
			kfree(rtk_priv);
			rtk_heap = NULL;
			goto out;
		}
		dma_coerce_mask_and_coherent(dma_heap_get_dev
			(dma_heap), DMA_BIT_MASK(32));
		mb();
	}

	/* set the last one dma_heap */
	rtk_heap->heap = dma_heap;
	rtk_heap->penalty = r_data->penalty;

	INIT_LIST_HEAD(&rtk_heap->list);
	INIT_LIST_HEAD(&rtk_heap->elist);

	list_add(&rtk_heap->hlist, &cheap_list);
	if (rtk_heap_is_low_1g(rtk_heap))
	{
		pr_err("add cheap_low\n"); //cy test
		list_add(&rtk_heap->hlist_low, &cheap_low_list);
	}	
	mutex_init(&rtk_heap->mutex);

out:
	return (void *)rtk_heap;

}

static void *rtk_cma_create(struct cma *cma, struct rtk_heap_data *r_data,
				 u32 cma_flags)
{
	struct rtk_heap *rtk_heap;
	struct dma_heap_export_info exp_info;

	rtk_heap = alloc_rtk_heap(cma, cma_flags);
	if (rtk_heap == NULL) {
		return NULL;
	}
	INIT_LIST_HEAD(&rtk_heap->task_list);

	exp_info.name = r_data->devdata[0].devname;
	exp_info.ops = &cma_ops;
	exp_info.priv = rtk_heap;

	rtk_heap->ops = &cma_ops;
	rtk_heap->heap = dma_heap_add(&exp_info);
	if (IS_ERR(rtk_heap->heap)) {
		kfree(rtk_heap);
		rtk_heap = NULL;
		goto out;
	}

	rtk_heap->penalty = r_data->penalty;
	INIT_LIST_HEAD(&rtk_heap->list);
	INIT_LIST_HEAD(&rtk_heap->elist);

	list_add(&rtk_heap->hlist, &cheap_list);
	if (rtk_heap_is_low_1g(rtk_heap))
		list_add(&rtk_heap->hlist_low, &cheap_low_list);

	mutex_init(&rtk_heap->mutex);

	dma_coerce_mask_and_coherent(dma_heap_get_dev
			(rtk_heap->heap), DMA_BIT_MASK(32));
	mb();

	if (!(cma_flags & RTK_FLAG_SUPPORT_NONCACHED))
		goto out;

	exp_info.name =  kasprintf(GFP_KERNEL,
				"%s-uncached", r_data->devdata[0].devname);

	exp_info.ops =  &uncached_cma_ops;
	exp_info.priv = rtk_heap;

	rtk_heap->uncached_ops = &uncached_cma_ops;
	rtk_heap->uncached_heap = dma_heap_add(&exp_info);
	if (IS_ERR_OR_NULL(rtk_heap->uncached_heap)) {
		kfree(rtk_heap);
		rtk_heap = NULL;
		goto out;
	}

	dma_coerce_mask_and_coherent(dma_heap_get_dev
			(rtk_heap->uncached_heap), DMA_BIT_MASK(32));
	mb();

	uncached_cma_ops.allocate = rtk_uncached_cma_allocate;

out:
	return (void *)rtk_heap;

}


static void *rtk_static_proctect_cma_create(struct cma *cma,
					struct rtk_heap_data *r_data, u32 cma_flags)
{
	struct rtk_protect_info *rtk_protect_info;
	struct rtk_heap *rtk_heap;
	unsigned long bitmap_count = cma->count >> cma->order_per_bit;
	unsigned long pfn, base;
	struct page *pages;
	int bit_id;
	struct dma_heap_export_info exp_info;
	struct dma_heap *dma_heap;
	struct rtk_priv *rtk_priv;
	int ret, i;

	rtk_heap = alloc_rtk_heap(cma, cma_flags);
	if (rtk_heap == NULL) {
		return NULL;
	}
	INIT_LIST_HEAD(&rtk_heap->task_list);
	pages = cma_alloc(cma, cma->count, 0, GFP_KERNEL);
	pfn = __page_to_pfn(pages);
	base = page_to_phys(pages);

	bit_id = (pfn - cma->base_pfn) >> cma->order_per_bit;
	bitmap_set(rtk_heap->alloc_bitmap, bit_id, bitmap_count);

	for ( i = 0; i < r_data->num_devdata; i++) {
		rtk_priv = kzalloc(sizeof(struct rtk_priv), GFP_KERNEL);
		if (!rtk_priv) {
			pr_err("%s kzalloc err \n", __func__);
			return NULL;
		}
		rtk_priv->rtk_heap = rtk_heap;
		rtk_priv->ext_flag = r_data->devdata[i].ext_flag;

		exp_info.name = r_data->devdata[i].devname;
		exp_info.ops =  &static_protect_cma_ops;
		exp_info.priv = rtk_priv;

		dma_heap = dma_heap_add(&exp_info);
		if (IS_ERR(dma_heap)) {
			kfree(rtk_heap);
			kfree(rtk_priv);
			rtk_heap = NULL;
			goto out;
		}

		dma_coerce_mask_and_coherent(dma_heap_get_dev
			(dma_heap), DMA_BIT_MASK(32));
		mb();

	}
	/* set the last one dma_heap */
	rtk_heap->heap = dma_heap;

	list_add(&rtk_heap->hlist, &cheap_list);
	if (rtk_heap_is_low_1g(rtk_heap))
		list_add(&rtk_heap->hlist_low, &cheap_low_list);

	mutex_init(&rtk_heap->mutex);

	dma_sync_single_for_device(dma_heap_get_dev(rtk_heap->heap), base,
			 cma->count << PAGE_SHIFT, DMA_BIDIRECTIONAL);

	rtk_dma_remap(pages, cma->count << PAGE_SHIFT, pgprot_dmacoherent(PAGE_KERNEL));

	rtk_protect_info = kzalloc(sizeof(struct rtk_protect_info),
				GFP_KERNEL);
	if (!rtk_protect_info) {
		pr_err("%s:%d ERROR!\n", __func__, __LINE__);
		kfree(rtk_heap);
		cma_release(cma, pages, cma->count);
		rtk_heap = NULL;
		goto out;
	}

	rtk_protect_info->create_info.mem.base = base;
	rtk_protect_info->create_info.mem.size = cma->count << PAGE_SHIFT;
	rtk_protect_info->create_info.mem.type = (enum e_notifier_protect_type)
					RTK_PROTECTED_TYPE_GET(cma_flags);

	ret = rtk_protect_create(&rtk_protect_info->create_info);
	if (ret) {
		pr_err("%s:%d rtk_protect_create_notify return ERROR!"
			" (priv_virt=0x%lx)\n", __func__, __LINE__, 
			(long unsigned int)
			rtk_protect_info->create_info.priv_virt);
		kfree(rtk_protect_info);
		kfree(rtk_heap);
		cma_release(cma, pages, cma->count);
		rtk_heap = NULL;
		goto out;
	}


	INIT_LIST_HEAD(&rtk_heap->list);
	INIT_LIST_HEAD(&rtk_heap->elist);

	list_add(&rtk_protect_info->list, &rtk_heap->list);


out:
	return (void *)rtk_heap;

}


static void *rtk_static_proctect_gen_create(unsigned long base,
				 unsigned long size, struct rtk_heap_data *r_data
				, u32 flags)
{
	struct rtk_protect_info *rtk_protect_info;
	struct rtk_heap *rtk_heap;
	struct dma_heap_export_info exp_info;
	struct rtk_priv *rtk_priv;
	struct dma_heap *dma_heap;
	int ret, i;

	rtk_heap = kzalloc(sizeof(*rtk_heap), GFP_KERNEL);
	if (!rtk_heap)
		return NULL;

	INIT_LIST_HEAD(&rtk_heap->task_list);

	rtk_heap->gen_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!rtk_heap->gen_pool) {
		kfree(rtk_heap);
		pr_err("%s:%d gen_pool_create \n", __func__, __LINE__);
		rtk_heap = NULL;
		goto out;
	}

	rtk_heap->name = r_data->name;
	gen_pool_set_algo(rtk_heap->gen_pool, gen_pool_best_fit, NULL);
	gen_pool_add(rtk_heap->gen_pool, base, size, -1);

	for ( i = 0; i < r_data->num_devdata; i++) {
		rtk_priv = kzalloc(sizeof(struct rtk_priv), GFP_KERNEL);
		if (!rtk_priv) {
			pr_err("%s kzalloc err \n", __func__);
			return NULL;
		}
		rtk_priv->rtk_heap = rtk_heap;
		rtk_priv->ext_flag = r_data->devdata[i].ext_flag;

		exp_info.name = r_data->devdata[i].devname;
		exp_info.ops =  &static_protect_gen_ops;
		exp_info.priv = rtk_priv;

		dma_heap = dma_heap_add(&exp_info);
		if (IS_ERR(dma_heap)) {
			kfree(rtk_heap);
			kfree(rtk_priv);
			rtk_heap = NULL;
			goto out;
		}
		dma_coerce_mask_and_coherent(dma_heap_get_dev
			(dma_heap), DMA_BIT_MASK(32));
		mb();
	}

	rtk_heap->heap = dma_heap;
	list_add(&rtk_heap->hlist, &gheap_list);
	mutex_init(&rtk_heap->mutex);

	dma_sync_single_for_device(dma_heap_get_dev(rtk_heap->heap),
			 base, size, DMA_BIDIRECTIONAL);

	rtk_protect_info = kzalloc(sizeof(struct rtk_protect_info),
				GFP_KERNEL);
	if (!rtk_protect_info) {
		pr_err("%s:%d ERROR!\n", __func__, __LINE__);
		kfree(rtk_heap);
		rtk_heap = NULL;
		goto out;
	}


	rtk_protect_info->create_info.mem.base = base ;

	rtk_protect_info->create_info.mem.size = size;

	rtk_protect_info->create_info.mem.type = (enum e_notifier_protect_type)
					RTK_PROTECTED_TYPE_GET(flags);

	ret = rtk_protect_create(&rtk_protect_info->create_info);
	if (ret) {
		kfree(rtk_protect_info);
		kfree(rtk_heap);
		pr_err("%s:%d rtk_protect_create return ERROR!"
			" (priv_virt=??)\n", __func__, __LINE__);
		rtk_heap = NULL;
		goto out;
	}


	INIT_LIST_HEAD(&rtk_heap->list);
	INIT_LIST_HEAD(&rtk_heap->elist);

	list_add(&rtk_protect_info->list, &rtk_heap->list);

	dma_coerce_mask_and_coherent(dma_heap_get_dev
			(rtk_heap->heap), DMA_BIT_MASK(32));
	mb();

out:
	return (void *)rtk_heap;

}

static void *rtk_gen_create(unsigned long base, unsigned long size,
				struct rtk_heap_data *r_data,
				 u32 flags)
{
	struct rtk_heap *rtk_heap;
	struct dma_heap_export_info exp_info;

	rtk_heap = kzalloc(sizeof(*rtk_heap), GFP_KERNEL);
	if (!rtk_heap)
		return NULL;

	INIT_LIST_HEAD(&rtk_heap->task_list);
	/* maybe we dont need these 2 , but helpful for debugfs */
	INIT_LIST_HEAD(&rtk_heap->list);
	INIT_LIST_HEAD(&rtk_heap->elist);

	rtk_heap->gen_pool = gen_pool_create(PAGE_SHIFT, -1);

	gen_pool_set_algo(rtk_heap->gen_pool, gen_pool_best_fit, NULL);
	gen_pool_add(rtk_heap->gen_pool, base, size, -1);

	rtk_heap->flag = flags;

	exp_info.name = r_data->devdata[0].devname;
	exp_info.ops = &gen_ops;
	exp_info.priv = rtk_heap;

	rtk_heap->ops = &gen_ops;
	rtk_heap->heap = dma_heap_add(&exp_info);
	if (IS_ERR_OR_NULL(rtk_heap->heap)) {
		kfree(rtk_heap);
		return NULL;
	}

	INIT_LIST_HEAD(&rtk_heap->elist);
	list_add(&rtk_heap->hlist, &gheap_list);
	mutex_init(&rtk_heap->mutex);

	dma_coerce_mask_and_coherent(dma_heap_get_dev
			(rtk_heap->heap), DMA_BIT_MASK(32));
	mb();

	if (!(flags & RTK_FLAG_SUPPORT_NONCACHED))
		goto out;

	exp_info.name =  kasprintf(GFP_KERNEL,
				"%s-uncached", r_data->devdata[0].devname);

	exp_info.ops = &uncached_gen_ops;
	exp_info.priv = rtk_heap;

	rtk_heap->uncached_ops = &uncached_gen_ops;
	rtk_heap->uncached_heap = dma_heap_add(&exp_info);
	if (IS_ERR_OR_NULL(rtk_heap->uncached_heap)) {
		kfree(rtk_heap);
		return NULL;
	}

	dma_coerce_mask_and_coherent(dma_heap_get_dev
			(rtk_heap->uncached_heap), DMA_BIT_MASK(32));
	mb();

	uncached_gen_ops.allocate = rtk_uncached_gen_allocate;

out:
	return (void *)rtk_heap;

}

/******************************************************************************
 ******************************************************************************/
static int best_fit_cmp(void *priv, const struct list_head *a,
			const struct list_head *b)
{
	struct rtk_best_fit *bfa = container_of(a, struct rtk_best_fit, hlist);
	struct rtk_best_fit *bfb = container_of(b, struct rtk_best_fit, hlist);

	if (bfb->score == bfa->score)
		return bfb->freed_pages > bfa->freed_pages;

	return bfb->score > bfa->score;
}

void fill_best_fit_list(char *name, unsigned long flags,
				 struct list_head *best_list)
{
	struct rtk_heap *gh;
	struct rtk_heap *ch;
	struct rtk_best_fit *best_fit;
	int score = 0;
	const int unit = 1;
	unsigned long used_bit, used_pages;

	/* general heaps */
	list_for_each_entry(gh, &gheap_list, hlist) {

		if (((flags & ~RTK_FLAG_PROTECTED_EXT_MASK) & gh->flag)
			 != (flags & ~RTK_FLAG_PROTECTED_EXT_MASK)) {
			continue;
		}

		score = 128;

		if (rtk_toc_type(flags) != rtk_toc_type(gh->flag)) {
			continue;
		}

		/* check protect type equals heap's protect type */
		if (rtk_protected_type(flags)) {
			if (rtk_protected_type(flags) !=
				rtk_protected_type(gh->flag)) {
				continue;
			}
		} else if (is_rtk_static_protect(gh->flag)) {
			continue;
		} else if (is_rtk_dynamic_protect(gh->flag)) {
			score -= unit*2;
		}

		score -= hweight_long((gh->flag &
				 ~(RTK_FLAG_PROTECTED_MASK |
				 RTK_FLAG_PROTECTED_EXT_MASK))) * unit * 4;

		best_fit = kmalloc(sizeof(*best_fit), GFP_KERNEL);
		best_fit->data = (void *)gh;
		best_fit->tag = tag_gen;
		best_fit->name = dma_heap_get_name(gh->heap);
		best_fit->score = score;
		best_fit->freed_pages = gen_pool_avail(gh->gen_pool) >> PAGE_SHIFT;

		list_add(&best_fit->hlist, best_list);

	}

	mutex_lock(&chplist_mutex);
	/* cma heaps */
	list_for_each_entry(ch, &cheap_list, hlist) {
		/* Exclusive pool doesn't support non-secure memory */
		if (!rtk_protected_type(flags) && is_rtk_exclusive_pool(ch->flag))
			continue;

		if (((flags & ~RTK_FLAG_PROTECTED_EXT_MASK) & ch->flag)
			 != (flags & ~RTK_FLAG_PROTECTED_EXT_MASK)) {
			continue;
		}

		score = 128;
		score -= unit*2;

		if (rtk_toc_type(flags) != rtk_toc_type(ch->flag)) {
			continue;
		}

		if (rtk_protected_type(flags)) {
			if (rtk_protected_type(flags) !=
				rtk_protected_type(ch->flag)) {
				continue;
			}
		} else if (is_rtk_static_protect(ch->flag)) {
			continue;
		} else if (is_rtk_dynamic_protect(ch->flag)) {
			score -= unit*4;
		}

		score -= hweight_long((ch->flag &
				 ~(RTK_FLAG_PROTECTED_MASK |
				 RTK_FLAG_PROTECTED_EXT_MASK))) * unit * 4;

		best_fit = kmalloc(sizeof(*best_fit), GFP_KERNEL);
		best_fit->data = (void *)ch;
		best_fit->tag = tag_cma;
		best_fit->name = dma_heap_get_name(ch->heap);
		if(rtk_protected_type(flags)){
			if (strcmp(best_fit->name, "vo_non-vc-ac-ve") == 0)
				score = 128;
		}
		else if (strcmp(best_fit->name, "vo_non-vc-ac-ve") == 0)
				score = 0;
		best_fit->score = score;
		pr_debug("%s: the heap name %s, flags = 0x%lx, score = %d\n", __func__, best_fit->name, flags, score);
		used_bit = bitmap_weight(ch->use_bitmap,
					 (int)cma_bitmap_maxno(ch->cma));
		used_pages = used_bit << ch->cma->order_per_bit;
		best_fit->freed_pages = (ch->cma->count) - used_pages;

		list_add(&best_fit->hlist, best_list);

	}

	mutex_unlock(&chplist_mutex);

	list_sort(NULL, best_list, best_fit_cmp);
}

void rtk_check_flag(unsigned long *flags)
{
	struct rtk_flag_replace *match;
	unsigned long condition;

	for (match = rtk_flag_match; match->condition; match++) {
		if (soc_device_match(rtk_soc_hank) || match->soc_type == NULL) {
			condition = match->condition | RTK_FLAG_PROTECTED_MASK |
				 RTK_FLAG_PROTECTED_EXT_MASK;

			if ((*flags & condition) == match->condition) {
 				*flags &= ~match->condition;
				*flags |= match->replace;
				break;
			}
		}
	}

}
/*****************************************************************************
 ****************************************************************************/
static ssize_t best_fit_heap_store(const struct class *class,
				    const struct class_attribute *attr,
				    const char *buf,
				    size_t count)
{
	sys_flags = simple_strtoul(buf, NULL, 0);
	return count;
}

static ssize_t best_fit_heap_show(const struct class *class,
				 const struct class_attribute *attr,
				 char *buf)
{
	LIST_HEAD(best_list);
	struct rtk_best_fit *best_fit;
	bool uncached = (sys_flags & RTK_FLAG_NONCACHED) ? true : false;
	int n = 0;

	if (sys_flags != 0) {
		fill_best_fit_list(NULL,  sys_flags, &best_list);
		list_for_each_entry(best_fit, &best_list, hlist) {
			if (uncached)
				n += scnprintf(buf+n, TMP_BUF_MAX-n,
					 "%s_uncached : %u \n",
					 best_fit->name, best_fit->score);
			else
				n += scnprintf(buf+n, TMP_BUF_MAX-n,
					 "%s : %u \n",
					 best_fit->name, best_fit->score);
		}
		for (;;) {
			if (list_empty(&best_list))
				break;
			best_fit = list_last_entry(&best_list,
				     struct rtk_best_fit,
				     hlist);
			list_del(&best_fit->hlist);
			kfree(best_fit);
		}
		sys_flags = 0;
		return n;
	}
	return -EINVAL;
}

static CLASS_ATTR_RW(best_fit_heap);


struct class *rtk_heap_class = NULL;
/******************************************************************************
 *
 *
 *****************************************************************************/
static int rheap_cma_check(struct cma *cma, void *data)
{
	unsigned long flag;
	int i;

	for (i = 0; i < rheap_data_size; i++) {

		if (strcmp(cma->name, rheap_data[i].name))
			continue;

		pr_info("rheap %s\n", cma->name);

		flag = rheap_data[i].flags;
		if (flag & RTK_FLAG_PROTECTED_DYNAMIC) {
			rtk_dynamic_proctect_cma_create(
				cma, &rheap_data[i], flag);
			goto out;
		}

		if (flag & RTK_FLAG_PROTECTED_MASK) {
			rtk_static_proctect_cma_create(
				cma, &rheap_data[i], flag);
			goto out;
		}
		rtk_cma_create(cma, &rheap_data[i], flag);
	}
out:
	return 0;
}

static void rheap_cheap_create(void)
{
	struct dma_heap_export_info exp_info;

	exp_info.name = "cma-rheap";
	exp_info.ops = &cheap_ops;
	exp_info.priv = &cheap_list;
	dma_heap_add(&exp_info);

	exp_info.name = "cma-rheap-uncached";
	exp_info.ops = &uncached_cheap_ops;
	exp_info.priv = &cheap_list;
	dma_heap_add(&exp_info);

	/* low 1G only */
	if (!list_empty(&cheap_low_list)) {

		memset(&exp_info, 0, sizeof(exp_info));
		exp_info.name = "cma-rheap-low";
		exp_info.ops  = &cheap_ops;
		exp_info.priv = &cheap_low_list;
		dma_heap_add(&exp_info);

		memset(&exp_info, 0, sizeof(exp_info));
		exp_info.name = "cma-rheap-low-uncached";
		exp_info.ops  = &uncached_cheap_ops;
		exp_info.priv = &cheap_low_list;
		dma_heap_add(&exp_info);
	}

	uncached_cheap_ops.allocate = rtk_uncached_cheap_allocate;

	list_sort(NULL, &cheap_list, best_cheap_cmp);
	list_sort(NULL, &cheap_low_list, best_cheap_low_cmp);
}


static int rheap_rmem_check(void)
{
	unsigned long flag;
	bool skip;
	struct device_node *node;
	struct resource res;
	int i, ret;

	node = of_find_compatible_node(NULL, NULL, "metadata");
	if (!node) {
		pr_err("%s: Unable to get metadata node", __func__);
		goto out;
	}

	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		pr_err("%s: Unable to get resource", __func__);
		goto out;
	}

	for (i = 0; i < rheap_data_size; i++) {

		if (strcmp("metadata", rheap_data[i].name))
			continue;

		flag = rheap_data[i].flags;
		skip = rheap_data[i].skip_set_protect;

		pr_info("rheap %s\n", rheap_data[i].name);

		if (!skip && flag & RTK_FLAG_PROTECTED_MASK) {

			rtk_static_proctect_gen_create(res.start,
					(unsigned long) resource_size(&res),
					&rheap_data[i], flag);
		} else {

			rtk_gen_create(res.start,
					(unsigned long) resource_size(&res),
					&rheap_data[i], flag);
		}
		break;
	}

out:

	return 0;
}

static int __init rheap_init(void)
{
	int ret;

	rheap_data_size = ARRAY_SIZE(rheap_data);

	if (!rtk_protect_handler_ready()) {
		pr_err("no protect region handler\n");
		return -EINVAL;
	}

	if (cma_for_each_area(rheap_cma_check, NULL) == 0)
		rheap_cheap_create();
	rheap_rmem_check();

	rtk_heap_class = class_create(DEVNAME);
	if (IS_ERR(rtk_heap_class)) {
		ret = PTR_ERR(rtk_heap_class);
		rtk_heap_class = NULL;
		return ret;
	}

	ret = class_create_file(rtk_heap_class, &class_attr_best_fit_heap);
	if (ret) {
		pr_err("create class file failed\n");
		return -EINVAL;
	}

	rheap_procfs_init();
	rheap_debugfs_init();
	rheap_miscdev_register();

	return 0;

}

fs_initcall(rheap_init);

module_param(alpha, int, 0644);
module_param(beta,  int, 0644);
module_param(protect_stub_mode,  bool, 0644);
MODULE_PARM_DESC(protect_stub_mode,
	"Enable protect stub mode when TEE/firmware is absent or for debug");

MODULE_DESCRIPTION("DMA-BUF RTK Heap");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(DMA_BUF);
