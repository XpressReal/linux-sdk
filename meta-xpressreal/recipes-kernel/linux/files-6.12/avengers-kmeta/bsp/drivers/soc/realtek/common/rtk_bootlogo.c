// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek free bootlogo driver
 *
 * Copyright (c) 2019-2024 Realtek Semiconductor Corp.
 */
#include <linux/err.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/delay.h>

struct bootlogo_data {
	struct device_node *dn;
	struct resource bootlogo_resource;
	bool logo_was_freed;
};

struct task_struct *freelogo_thread = NULL;

/* work list */
struct kthread_work freelogo_work;
/* main entry */
struct kthread_worker freelogo_worker;

static struct bootlogo_data *bootlogo_data = NULL;

void bootlogo_release(void);

static void do_freelogo_work_func(struct kthread_work *work)
{
	int max_run_time = 120;

	msleep_interruptible(max_run_time  * 1000);
	if (kthread_should_stop())
		return;

	bootlogo_release();
}

static unsigned long bootlogo_free_reserved_area(
	void *start, void *end, int poison, const char *s)
{
	void *pos;
	unsigned long pages = 0;

	start = (void *)PAGE_ALIGN((unsigned long)start);
	end = (void *)((unsigned long)end & PAGE_MASK);

	for (pos = start; pos < end; pos += PAGE_SIZE, pages++) {
		struct page *page = virt_to_page(pos);
		void *direct_map_addr;

		/*
		 * 'direct_map_addr' might be different from 'pos'
		 * because some architectures' virt_to_page()
		 * work with aliases.  Getting the direct map
		 * address ensures that we get a _writeable_
		 * alias for the memset().
		 */
		direct_map_addr = page_address(page);
		if ((unsigned int)poison <= 0xFF)
			memset(direct_map_addr, poison, PAGE_SIZE);

		free_reserved_page(page);
	}

	if (pages && s)
		pr_info("Freeing %s memory: %ldK\n",
			s, pages << (PAGE_SHIFT - 10));

	return pages;
}

/* copy form arch/arm/mm/init.c */
#ifdef CONFIG_HIGHMEM
static inline void free_area_high(unsigned long pfn, unsigned long end)
{
	for (; pfn < end; pfn++)
		free_highmem_page(pfn_to_page(pfn));
}
#else
static inline void free_area_high(unsigned long pfn, unsigned long end)
{
}
#endif

static void bootlogo_free_memory(phys_addr_t paddr,	size_t size)
{
	/* remove rsv. info from memblock list */
	memblock_free(phys_to_virt(paddr), size);


#if !defined(CONFIG_ARM64) || !defined(CONFIG_64BIT)
	if (PageHighMem(pfn_to_page(PFN_DOWN(paddr)))) {
		pr_info("\033[1;32m" "free high memory addr 0x%08x, size %d"
			"\033[m\n",
			paddr, size);
		free_area_high(PFN_DOWN(paddr),
			PFN_DOWN(paddr+size));
	} else { /* low memory in linear mapping */
		pr_info("\033[1;32m" "free low memory addr 0x%08x(va 0x%08x)"
			" , size %d" "\033[m\n",
			paddr, __va(paddr), size);
		bootlogo_free_reserved_area(__va(paddr),
			__va(paddr+size), 0,
			"free logo area");
	}
#else /* low memory in linear mapping */
	pr_info("\033[1;32m" "free memory addr 0x%llx(0x%llx)"
		" , size %d" "\033[m\n",
		paddr, (long long unsigned int)__va(paddr), (int)size);
	bootlogo_free_reserved_area(__va(paddr),
		__va(paddr+size), 0,
		"free logo area");
#endif
}

void bootlogo_release(void)
{
	phys_addr_t paddr;
	size_t size;

	if (!bootlogo_data) {
		pr_info("\033[1;33m"
			"device bootlogo data empty" "\033[m\n");
		return;
	}

	if (bootlogo_data->logo_was_freed) {
		pr_info("\033[1;33m" "logo was freed ever" "\033[m\n");
		return;
	}

	paddr = bootlogo_data->bootlogo_resource.start;
	size = resource_size(&bootlogo_data->bootlogo_resource);
	pr_info("free base=%pa size=0x%zx(%zu) high_memory=0x%08lx\n",
		&paddr, size, size,(long)high_memory);

	bootlogo_free_memory(paddr, size);
	bootlogo_data->logo_was_freed = true;
}

/*
 * fdt example
 *
 * reserved-memory {
 *
 *  Realtek> fdt print /reserved-memory/buffer
 *  buffer@2F700000 {
 *          reg = <0x2f700000 0x00900000>;
 *          phandle = <0x00000002>;
 *  };
 *
 */

static int __init bootlogo_init(void)
{
	int ret;
	freelogo_thread = NULL;
	struct device_node *reserved_mem, *mem_dn;
	phys_addr_t paddr;
	size_t size;

	pr_info("\033[1;33m" "bootlogo init ver.0705.1000" "\033[m\n");

	reserved_mem = of_find_node_by_path("/reserved-memory");
	if (!reserved_mem) {
		pr_err("\033[1;33mFailed to find reserved-memory node\033[m\n");
		ret = -ENODEV;
		goto fail_find_reserved;
	}

	mem_dn = of_find_node_by_name(reserved_mem, "buffer");
	if (!mem_dn) {
		pr_err("Failed to find buffer node under /reserved-memory\n");
		ret = -ENODEV;
		goto fail_find_buffer;
	}

	bootlogo_data = kzalloc(sizeof(*bootlogo_data), GFP_KERNEL);
	if (!bootlogo_data) {
		pr_err("\033[1;33m" "alloc bootlogo data failed" "\033[m\n");
		ret = -ENOMEM;
		goto fail_alloc;
	}

	bootlogo_data->logo_was_freed = false;
	ret = of_address_to_resource(mem_dn, 0, &bootlogo_data->bootlogo_resource);
	if (ret) {
		pr_err("Failed to get resource from reserved-memory node\n");
		goto fail_resource;
	}

	bootlogo_data->dn = mem_dn;

	paddr = bootlogo_data->bootlogo_resource.start;
	size = resource_size(&bootlogo_data->bootlogo_resource);

	pr_info("\033[1;33m" "logo base=%pa size=0x%lx(%lu)" "\033[m\n",
		&paddr, (long)size, (long)size);

	kthread_init_worker(&freelogo_worker);
	kthread_init_work(&freelogo_work, do_freelogo_work_func);
	kthread_queue_work(&freelogo_worker, &freelogo_work);
	freelogo_thread = kthread_run(kthread_worker_fn, &freelogo_worker,
		"freelogo_thread");

	/* sanity check */
	if (IS_ERR(freelogo_thread)) {
		pr_err("\033[1;33m"
			"failed to start freelogo_thread" "\033[m\n");
		ret = PTR_ERR(freelogo_thread);
		goto fail_thread;
	}

	of_node_put(reserved_mem);
	of_node_put(mem_dn);
	return 0;

fail_thread:
	freelogo_thread = NULL;
fail_resource:
	kfree(bootlogo_data);
	bootlogo_data = NULL;
fail_alloc:
	of_node_put(mem_dn);
fail_find_buffer:
	of_node_put(reserved_mem);
fail_find_reserved:
	pr_err("\033[1;33m"
		"bootlogo register failed %d" "\033[m\n", ret);
	return ret;
}

static void __exit bootlogo_exit(void)
{
	// TODO: might add lock to prevent from race condition
	// between kthread and driver exit.

	if (freelogo_thread) {
		pr_info("\033[1;33m" "stop freelogo kthread" "\033[m\n");
		kthread_stop(freelogo_thread);
	}

	pr_info("\033[1;33m" "bootlogo free memory module exit" "\033[m\n");
}

module_init(bootlogo_init);
module_exit(bootlogo_exit);
MODULE_LICENSE("GPL v2");
