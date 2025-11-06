// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek free bootlogo driver
 *
 * Copyright (c) 2019-2024 Realtek Semiconductor Corp.
 */
#include <linux/err.h>
#include <linux/platform_device.h>
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

static struct platform_device *bootlogo_pdev = NULL;

struct bootlogo_drvdata {
	struct device_node *dn;
	struct resource bootlogo_resource;
	void __iomem *vaddr;
	bool dt_bootlogo_rsvmem_exist;
	bool logo_was_freed;
};

struct task_struct *freelogo_thread = NULL;
struct kthread_work freelogo_work; // work list
struct kthread_worker freelogo_worker; // main entry
bool freelogo_thread_stop = false;

void bootlogo_release(struct platform_device *pdev);

static void do_freelogo_work_func(struct kthread_work *work)
{
	int max_run_time = 120;
	const int sleep_interval = 2;

	while (max_run_time>0) {
		if (freelogo_thread_stop)
			return;
		//pr_err("\033[1;33m"
		//	"freelogo work. wait %d sec..."
		//	"\033[m\n", max_run_time);
		msleep_interruptible(sleep_interval*1000);
		max_run_time -= sleep_interval;
	}

	// free logo
	bootlogo_release(NULL);
}

unsigned long bootlogo_free_reserved_area(
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

void bootlogo_free_memory(phys_addr_t paddr,
	void __iomem * vaddr,
	size_t size)
{
	if (vaddr) {
		pr_info("\033[1;32m" "memunmap 0x%08lx"
			"\033[m\n", (long)vaddr);
		memunmap(vaddr);
	}

	/* remove rsv. info from memblock list */
	memblock_free(paddr, size);

#if !defined(CONFIG_ARM64) || !defined(CONFIG_64BIT)
	if (PageHighMem(pfn_to_page(paddr >> PAGE_SHIFT))) {
		pr_info("\033[1;32m" "free high memory addr 0x%08x, size %d"
			"\033[m\n",
			paddr, size);
		free_area_high((paddr >> PAGE_SHIFT),
			(paddr+size) >> PAGE_SHIFT);
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

void bootlogo_release(struct platform_device *pdev)
{
	int ret;
	phys_addr_t paddr;
	size_t size;
	void __iomem *vaddr;
	struct platform_device *curr_pdev;
	struct bootlogo_drvdata *pdata;
	struct device *dev;
	struct property *dt_prop;

	curr_pdev = pdev ? pdev : bootlogo_pdev;

	if (!curr_pdev) {
		pr_info("\033[1;33m" "platform_device NULL" "\033[m\n");
		return;
	}

	dev = &curr_pdev->dev;

	pdata = (struct bootlogo_drvdata *)platform_get_drvdata(curr_pdev);
	if (!pdata) {
		pr_info("\033[1;33m"
			"platform_device bootlogo data empty" "\033[m\n");
		return;
	}

	if (!pdata->dt_bootlogo_rsvmem_exist) {
		pr_info("\033[1;33m"
			"no reserved memory for bootlogo" "\033[m\n");
		return;
	}

	if (pdata->logo_was_freed) {
		pr_info("\033[1;33m" "logo was freed ever" "\033[m\n");
		return;
	}

	paddr = pdata->bootlogo_resource.start;
	size = resource_size(&pdata->bootlogo_resource);
	vaddr = pdata->vaddr;

	pr_info("free base=%pa size=0x%zx(%zu) v=0x%08lx high_memory=0x%08lx\n",
		&paddr, size, size, (long)vaddr,
		(long)high_memory);

	bootlogo_free_memory(paddr, vaddr, size);

	pdata->logo_was_freed = true;
	pdata->dt_bootlogo_rsvmem_exist = false;

	// find property and remove it
	if (dev && dev->of_node) {
		dt_prop = of_find_property(dev->of_node, "memory-region", NULL);
		if (dt_prop) {
			ret = of_remove_property(dev->of_node, dt_prop);
			if (ret) {
				pr_err("\033[1;33m"
					"failed to remove property. %d"
					"\033[m\n", ret);
			}
		}
		else {
			pr_err("\033[1;33m"
				"Can not find property \"memory-region\""
				"\033[m\n");
		}
	}
}

static int bootlogo_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct device *dev = &pdev->dev;
	struct device_node *dn;
	struct bootlogo_drvdata *pdata = NULL;
	phys_addr_t paddr;
	size_t size;

	pdata = (struct bootlogo_drvdata *)
		kzalloc(sizeof(struct bootlogo_drvdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("\033[1;33m" "alloc bootlogo drvdata failed" "\033[m\n");
		goto fail;
	}
	pdata->logo_was_freed = false;
	pdata->dt_bootlogo_rsvmem_exist = false;

	dn = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (of_device_is_available(dn)) {
		if (of_address_to_resource(dn, 0, &pdata->bootlogo_resource)) {
			pr_err("No memory address assigned to the region\n");
			goto fail;
		}
		pdata->dt_bootlogo_rsvmem_exist = true;
	}
	else {
		pr_err("\033[1;33m"
			"Can not find handle of \"memory-region\"" "\033[m\n");
		ret = -ENODEV;
		goto fail;
	}
	pdata->dn = dn;

	paddr = pdata->bootlogo_resource.start;
	size = resource_size(&pdata->bootlogo_resource);
	pr_info("\033[1;33m" "logo base=%pa size=0x%lx(%lu)" "\033[m\n",
		&paddr, (long)size, (long)size);

	// save driver data
	platform_set_drvdata(pdev, (void *)pdata);
	bootlogo_pdev = pdev;

	// init kthread worker
	kthread_init_worker(&freelogo_worker);

	// init kthread work
	kthread_init_work(&freelogo_work, do_freelogo_work_func);

	// atach work to worker
	kthread_queue_work(&freelogo_worker, &freelogo_work);

	freelogo_thread = kthread_run(kthread_worker_fn, &freelogo_worker,
		"freelogo_thread");

	// sanity check
	if (IS_ERR(freelogo_thread)) {
		pr_err("\033[1;33m"
			"failed to start freelogo_thread" "\033[m\n");
		freelogo_thread = NULL;
	}

	return 0;
fail:
	if (pdata)
		kfree(pdata);

	return ret;
}

static int bootlogo_remove(struct platform_device *pdev)
{
	struct bootlogo_drvdata *pdata =
	    (struct bootlogo_drvdata *)platform_get_drvdata(pdev);

	bootlogo_release(pdev);

	if (pdata)
		kfree(pdata);

	// bootlogo_pdev = NULL; // no necessary

	return 0;
}

/*
 * fdt example
 *
 * reserved-memory {
 *
 *  Realtek> fdt print /bootlogo
 *  bootlogo {
 *          compatible = "realtek,bootlogo";
 *          memory-region = <0x00000002>;
 *  };
 *  Realtek> fdt print /reserved-memory/buffer
 *  buffer@2F700000 {
 *          reg = <0x2f700000 0x00900000>;
 *          phandle = <0x00000002>;
 *  };
 *
 */
static struct of_device_id bootlogo_ids[] = {
	{.compatible = "realtek,bootlogo"},
	{ /* Sentinel */ },
};

static struct platform_driver bootlogo_driver = {
	.probe = bootlogo_probe,
	.remove = bootlogo_remove,
	.driver = {
		.name = "bootlogo",
		.owner = THIS_MODULE,
		.of_match_table = bootlogo_ids,
	},
};

static int __init bootlogo_init(void)
{
	int ret;
	freelogo_thread = NULL;
	freelogo_thread_stop = false;

	pr_info("\033[1;33m" "bootlogo init ver.0705.1000" "\033[m\n");
	ret = platform_driver_register(&bootlogo_driver);
	if (ret) {
		pr_err("\033[1;33m"
			"bootlogo register failed %d" "\033[m\n", ret);
		return ret;
	}

	return 0;
}

static void __exit bootlogo_exit(void)
{
	// TODO: might add lock to prevent from race condition
	//       between kthread and driver exit.

	if (freelogo_thread) {
		freelogo_thread_stop = true;
		pr_info("\033[1;33m" "stop freelogo kthread" "\033[m\n");
		kthread_stop(freelogo_thread);
	}

	pr_info("\033[1;33m" "bootlogo driver exit" "\033[m\n");
	platform_driver_unregister(&bootlogo_driver);
}

module_init(bootlogo_init);
module_exit(bootlogo_exit);

MODULE_LICENSE("GPL v2");
