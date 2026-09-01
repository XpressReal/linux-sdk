// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * GIC-v3 extension setting for Realtek DHC SoCs
 * Copyright (c) 2020-2021 Realtek Semiconductor Corp.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/sys_soc.h>
#include <linux/syscore_ops.h>

#include <asm/io.h>

#define GICD_ISENABLER0 0x100
#define GICD_ISENABLER1 0x104
#define GICD_ISENABLER2 0x108
#define GICD_ISENABLER3 0x10c
#define GICD_ISENABLER4 0x110
#define GICR_ISENABLER0 (0x10000 + 0x100)
#define CPU1_RBASE 0xff160000
#define CPU2_RBASE 0xff180000
#define CPU3_RBASE 0xff1a0000
#define DISTRIBUTOR_BASE 0xff100000
#define REDISTRIBUTOR_BASE 0xff140000
#define DISTRIBUTOR_SIZE 0x10000
#define REDISTRIBUTOR_SIZE 0x80000

static unsigned int gicr_isenabler0;
static unsigned int gicd_isenabler0;
static unsigned int gicd_isenabler1;
static unsigned int gicd_isenabler2;
static unsigned int gicd_isenabler3;
static unsigned int gicd_isenabler4;

static void __iomem *cpu1_rbase;
static void __iomem *cpu2_rbase;
static void __iomem *cpu3_rbase;
static void __iomem *dbase;
static void __iomem *rbase;

static struct soc_device_attribute gic_extension_match[] = {
	{
		.family = "Realtek Hank",
	},
	{
		.family = "Realtek Stark",
	},
	{
		.family = "Realtek Parker",
	},
	{
		.family = "Realtek Groot",
	},
	{
		.family = "Realtek RTD1325",
	},
	{ /* empty */ }
};

static void rtk_gic_extension_resume(void)
{
	writel_relaxed(gicr_isenabler0, rbase + GICR_ISENABLER0);
	writel_relaxed(gicd_isenabler0, dbase + GICD_ISENABLER0);
	writel_relaxed(gicd_isenabler1, dbase + GICD_ISENABLER1);
	writel_relaxed(gicd_isenabler2, dbase + GICD_ISENABLER2);
	writel_relaxed(gicd_isenabler3, dbase + GICD_ISENABLER3);
	writel_relaxed(gicd_isenabler4, dbase + GICD_ISENABLER4);
}

static int rtk_gic_extension_suspend(void)
{
	int ret = 0;

	gicr_isenabler0 = readl_relaxed(rbase + GICR_ISENABLER0);
	gicd_isenabler0 = readl_relaxed(dbase + GICD_ISENABLER0);
	gicd_isenabler1 = readl_relaxed(dbase + GICD_ISENABLER1);
	gicd_isenabler2 = readl_relaxed(dbase + GICD_ISENABLER2);
	gicd_isenabler3 = readl_relaxed(dbase + GICD_ISENABLER3);
	gicd_isenabler4 = readl_relaxed(dbase + GICD_ISENABLER4);

	return ret;
}

static struct syscore_ops rtk_gic_extension_syscore_ops = {
	.suspend = rtk_gic_extension_suspend,
	.resume = rtk_gic_extension_resume,
};

static int rtk_gic_on_cpu(unsigned int cpu)
{
	void __iomem *rbase = NULL;

	if (cpu == 1)
		rbase = cpu1_rbase;
	else if (cpu == 2)
		rbase = cpu2_rbase;
	else if (cpu == 3)
		rbase = cpu3_rbase;

	writel_relaxed(readl_relaxed(rbase) & 0xfdffffff, rbase);

	return 0;
}

static int rtk_gic_off_cpu(unsigned int cpu)
{
	void __iomem *rbase = NULL;

	if (cpu == 1)
		rbase = cpu1_rbase;
	else if (cpu == 2)
		rbase = cpu2_rbase;
	else if (cpu == 3)
		rbase = cpu3_rbase;

	writel_relaxed(readl_relaxed(rbase) | 0x2000000, rbase);

	return 0;
}

static int rtk_soc_is_ready(struct device *dev, const void *data)
{
	if (!dev->of_node)
		return 0;

	if (of_device_is_compatible(dev->of_node, "realtek,soc-chip")) {
		if (dev_get_drvdata(dev))
			return 1;
	}

	return 0;
}

static int rtk_gic_probe(struct platform_device *pdev)
{
	struct device *soc_dev;

	soc_dev = bus_find_device(&platform_bus_type, NULL, NULL, rtk_soc_is_ready);
	if (!soc_dev) {
		pr_info("rtk_chip not ready, deferring probe\n");
		return -EPROBE_DEFER;
	}

	put_device(soc_dev);

	/* For SoCs older than Kent, it is necessary to save and restore
	 * GIC PPI and SPI settings via this driver.
	 */
	if (soc_device_match(gic_extension_match)) {
		dbase = ioremap(DISTRIBUTOR_BASE, DISTRIBUTOR_SIZE);
		rbase = ioremap(REDISTRIBUTOR_BASE, REDISTRIBUTOR_SIZE);
		cpu1_rbase = ioremap(CPU1_RBASE, 0x4);
		cpu2_rbase = ioremap(CPU2_RBASE, 0x4);
		cpu3_rbase = ioremap(CPU3_RBASE, 0x4);

		cpuhp_setup_state_nocalls(CPUHP_AP_CPU_PM_STARTING,
					"rtk:gic:extension:starting",
					rtk_gic_on_cpu, rtk_gic_off_cpu);
		register_syscore_ops(&rtk_gic_extension_syscore_ops);
	}

	return 0;
};

static void rtk_gic_remove(struct platform_device *pdev)
{

	if (soc_device_match(gic_extension_match)) {
		cpuhp_remove_state_nocalls(CPUHP_AP_CPU_PM_STARTING);
		unregister_syscore_ops(&rtk_gic_extension_syscore_ops);

		iounmap(cpu1_rbase);
		iounmap(cpu2_rbase);
		iounmap(cpu3_rbase);
		iounmap(dbase);
		iounmap(rbase);
	}
}

static struct of_device_id rtk_gic_ids[] = {
	{ .compatible = "realtek,gic-extension" },
	{ /* Sentinel */ },
};

static struct platform_driver rtk_gic_extension = {
	.probe = rtk_gic_probe,
	.remove = rtk_gic_remove,
	.driver = {
		.name = "realtek-gic-extension",
		.owner = THIS_MODULE,
		.of_match_table = rtk_gic_ids,
	},
};
module_platform_driver(rtk_gic_extension);

MODULE_AUTHOR("James Tai <james.tai@realtek.com>");
MODULE_DESCRIPTION("GIC-v3 extension setting");
MODULE_LICENSE("GPL v2");
