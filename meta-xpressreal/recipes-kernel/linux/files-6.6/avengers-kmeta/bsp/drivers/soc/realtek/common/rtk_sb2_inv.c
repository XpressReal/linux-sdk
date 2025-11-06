// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019,2025 Realtek Semiconductor Corp.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <soc/realtek/rtk_sb2_inv.h>

#define SB2_INV_INTEN              0x0
#define SB2_INV_INTSTAT            0x4
#define SB2_INV_ADDR               0x8
#define SB2_DEBUG_REG              0xC

#define SB2_INV_IRQEN_SCPU         (1 << 1)
#define SB2_INV_IRQEN_PCPU         (1 << 2)
#define SB2_INV_IRQEN_ACPU         (1 << 3)
#define SB2_INV_IRQEN_SCPU_SWC     (1 << 6)
#define SB2_INV_IRQEN_PCPU_2       (1 << 8)
#define SB2_INV_IRQEN_VCPU         (1 << 10)
#define SB2_INV_IRQEN_AUCPU0       (1 << 11)
#define SB2_INV_IRQEN_AUCPU1       (1 << 12)
#define DEFAULT_IRQEN_CPUS \
	(SB2_INV_IRQEN_SCPU | SB2_INV_IRQEN_PCPU | SB2_INV_IRQEN_SCPU_SWC)

#define RTD1625_SB2_INV_INTEN_REE   0x0
#define RTD1625_SB2_INV_INTSTAT_REE 0x4
#define RTD1625_SB2_INV_ADDR_REE    0x8
#define RTD1625_SB2_INV_INFO_REE    0xc

struct sb2_inv_drvdata *sb2;

struct sb2_inv_desc {
	void (*inte_set)(struct sb2_inv_drvdata *sb2, u32 en);
	u32 (*inte_get)(struct sb2_inv_drvdata *sb2);
	u32 (*ints_get_and_clear)(struct sb2_inv_drvdata *sb2);
	void (*get_info)(struct sb2_inv_drvdata *sb2, u32 ints, struct sb2_inv_event_data *evt);
};

struct sb2_inv_drvdata {
	struct device *dev;
	void *base;
	const struct sb2_inv_desc *desc;

	spinlock_t lock;
	ktime_t first;
	u32 addr;
	u32 count;

	u32 saved_inte;
};

static const char * const cpu_id_string[] = {
	[SB2_INV_CPU_ID_UNKNOWN]  = "Unknown CPU",
	[SB2_INV_CPU_ID_SCPU]     = "SCPU",
	[SB2_INV_CPU_ID_PCPU]     = "PCPU",
	[SB2_INV_CPU_ID_ACPU]     = "ACPU",
	[SB2_INV_CPU_ID_SCPU_SWC] = "SCPU security world",
	[SB2_INV_CPU_ID_PCPU_2]   = "PCPU/R-BUS2",
	[SB2_INV_CPU_ID_VCPU]     = "VCPU",
	[SB2_INV_CPU_ID_AUCPU0]   = "AUCPU0",
	[SB2_INV_CPU_ID_AUCPU1]   = "AUCPU0",
	[SB2_INV_CPU_ID_HIF]      = "HIF",
};

static const char *get_cpu_name(u32 id)
{
	if (id >= ARRAY_SIZE(cpu_id_string))
		id = 0;
	return cpu_id_string[id];
}

static void __sb2_inv_inte_set(struct sb2_inv_drvdata *sb2, u32 en)
{
	if (en) {
		writel(~1, sb2->base + SB2_INV_INTSTAT);
		writel(1 | DEFAULT_IRQEN_CPUS, sb2->base + SB2_INV_INTEN);
	} else {
		writel(0 | DEFAULT_IRQEN_CPUS, sb2->base + SB2_INV_INTEN);
	}
}

static u32 __sb2_inv_inte_get(struct sb2_inv_drvdata *sb2)
{
	u32 val = readl(sb2->base + SB2_INV_INTEN);

	return !!(val & SB2_INV_IRQEN_SCPU);
}

static u32 __sb2_inv_ints_get_and_clear(struct sb2_inv_drvdata *sb2)
{
	u32 ints;

	ints = readl(sb2->base + SB2_INV_INTSTAT);
	writel(~1, sb2->base + SB2_INV_INTSTAT);
	return ints;
}

static int cpu_map[32] = {
	[1] = SB2_INV_CPU_ID_SCPU,
	[2] = SB2_INV_CPU_ID_PCPU,
	[3] = SB2_INV_CPU_ID_ACPU,
	[4] = SB2_INV_CPU_ID_SCPU_SWC,
	[5] = SB2_INV_CPU_ID_PCPU_2,
	[6] = SB2_INV_CPU_ID_VCPU,
	[7] = SB2_INV_CPU_ID_AUCPU0,
	[8] = SB2_INV_CPU_ID_AUCPU1,
	[9] = SB2_INV_CPU_ID_HIF,
};

static void __sb2_inv_get_info(struct sb2_inv_drvdata *sb2, u32 ints, struct sb2_inv_event_data *evt)
{
	evt->raw_ints   = ints;
	evt->inv_cpu    = cpu_map[ffs(ints) - 1];
	evt->addr       = readl(sb2->base + SB2_INV_ADDR);
	evt->timeout_th = readl(sb2->base + SB2_DEBUG_REG);
}

static const struct sb2_inv_desc sb2_inv_desc = {
	.inte_set = __sb2_inv_inte_set,
	.inte_get = __sb2_inv_inte_get,
	.ints_get_and_clear = __sb2_inv_ints_get_and_clear,
	.get_info = __sb2_inv_get_info,
};

static void rtd1625_sb2_inv_inte_set(struct sb2_inv_drvdata *sb2, u32 en)
{
	if (en) {
		writel(~0, sb2->base + RTD1625_SB2_INV_INTSTAT_REE);
		writel(1, sb2->base +  RTD1625_SB2_INV_INTEN_REE);
	} else {
		writel(0, sb2->base +  RTD1625_SB2_INV_INTEN_REE);
	}
}

static u32 rtd1625_sb2_inv_inte_get(struct sb2_inv_drvdata *sb2)
{
	return readl(sb2->base +  RTD1625_SB2_INV_INTEN_REE) & 1;
}

static u32 rtd1625_sb2_inv_ints_get_and_clear(struct sb2_inv_drvdata *sb2)
{
	u32 ints;

	ints = readl(sb2->base +  RTD1625_SB2_INV_INTSTAT_REE);
	writel(~0, sb2->base + RTD1625_SB2_INV_INTSTAT_REE);
	return ints;
}

static int rtd1625_cpu_map[32] = {
	[0] = SB2_INV_CPU_ID_SCPU,
};

static void rtd1625_sb2_inv_get_info(struct sb2_inv_drvdata *sb2, u32 ints,
				     struct sb2_inv_event_data *evt)
{
	u32 val;

	evt->raw_ints   = ints;
	evt->inv_cpu    = rtd1625_cpu_map[ffs(ints) - 1];
	evt->addr       = readl(sb2->base + RTD1625_SB2_INV_ADDR_REE);
	evt->version    = 1;
	val = readl(sb2->base + RTD1625_SB2_INV_INFO_REE);
	evt->rw         = (val >> 8) & 1;
	evt->inv_id     = val & 0xff;
}

static const struct sb2_inv_desc rtd1625_desc = {
	.inte_set = rtd1625_sb2_inv_inte_set,
	.inte_get = rtd1625_sb2_inv_inte_get,
	.ints_get_and_clear = rtd1625_sb2_inv_ints_get_and_clear,
	.get_info = rtd1625_sb2_inv_get_info,
};

static void sb2_inv_inte_set(struct sb2_inv_drvdata *sb2, u32 en)
{
	sb2->desc->inte_set(sb2, en);
}

static u32 sb2_inv_inte_get(struct sb2_inv_drvdata *sb2)
{
	return sb2->desc->inte_get(sb2);
}

static u32 sb2_inv_ints_get_and_clear(struct sb2_inv_drvdata *sb2)
{
	return sb2->desc->ints_get_and_clear(sb2);
}

static void sb2_inv_get_info(struct sb2_inv_drvdata *sb2, u32 ints, struct sb2_inv_event_data *evt)
{
	sb2->desc->get_info(sb2, ints, evt);
}

static void sb2_inv_print_inv_event(struct sb2_inv_event_data *evt)
{
	if (evt->version == 0)
		pr_err("Invalid access: INTSTAT=0x%08x(%s), ADDR=0x%08x, TIMEOUT_TH=0x%08x\n",
			evt->raw_ints, get_cpu_name(evt->inv_cpu),
			evt->addr, evt->timeout_th);
	else
		pr_err("Invalid access: INTSTAT=0x%08x(%s), ADDR=0x%08x, RW=%d, INV_ID=%d\n",
			evt->raw_ints, get_cpu_name(evt->inv_cpu),
			evt->addr, evt->rw, evt->inv_id);
}

static void sb2_inv_check_access_count(struct sb2_inv_drvdata *sb2, struct sb2_inv_event_data *evt)
{
	u64 t;
	unsigned long flags;

	if (sb2->addr != evt->addr) {
		sb2->addr = evt->addr;
		sb2->first = ktime_get();
		sb2->count = 0;
		return;
	}

	sb2->count += 1;
	t = ktime_to_ms(ktime_sub(ktime_get(), sb2->first));
	if (t <= 500 && sb2->count >= 50) {
		pr_warn("Invalid access: ADDR=0x%08x count=%d in %llums, disabling\n",
			sb2->addr, sb2->count, t);
		spin_lock_irqsave(&sb2->lock, flags);
		sb2_inv_inte_set(sb2, 0);
		spin_unlock_irqrestore(&sb2->lock, flags);
	} else if (t > 500) {
		sb2->first = ktime_get();
		sb2->count = 0;
	}
}

static irqreturn_t sb2_inv_int_handler(int irq, void *id)
{
	struct platform_device *pdev = id;
	struct sb2_inv_drvdata *sb2 = platform_get_drvdata(pdev);
	struct sb2_inv_event_data evt = {0};
	u32 ints;

	ints = sb2_inv_ints_get_and_clear(sb2);
	if (!ints)
		return IRQ_NONE;
	sb2_inv_get_info(sb2, ints, &evt);

	sb2_inv_print_inv_event(&evt);
	sb2_inv_check_access_count(sb2, &evt);
	return IRQ_HANDLED;
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sb2_inv_drvdata *sb2 = dev_get_drvdata(dev);
	u32 enabled;
	unsigned long flags;

	spin_lock_irqsave(&sb2->lock, flags);
	enabled = sb2_inv_inte_get(sb2);
	spin_unlock_irqrestore(&sb2->lock, flags);

	return snprintf(buf, PAGE_SIZE, "%s\n", enabled ? "enabled" : "disabled");
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr, const char *buf,
			     size_t size)
{
	struct sb2_inv_drvdata *sb2 = dev_get_drvdata(dev);
	bool enable;
	int ret;
	unsigned long flags;

	ret = strtobool(buf, &enable);
	if (ret)
		return ret;

	spin_lock_irqsave(&sb2->lock, flags);
	sb2_inv_inte_set(sb2, enable);
	spin_unlock_irqrestore(&sb2->lock, flags);
	return size;
}

static DEVICE_ATTR_RW(enabled);

static int sb2_inv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sb2_inv_drvdata *sb2;
	struct device_node *np = dev->of_node;
	struct resource res;
	int ret;
	int irq;

	sb2 = devm_kzalloc(dev, sizeof(*sb2), GFP_KERNEL);
	if (!sb2)
		return -ENOMEM;

	spin_lock_init(&sb2->lock);
	sb2->dev = dev;
	sb2->desc = of_device_get_match_data(&pdev->dev);
	if (!sb2->desc)
		return -EINVAL;

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		return ret;

	sb2->base = devm_ioremap(dev, res.start, resource_size(&res));
	if (!sb2->base)
		return -ENOMEM;

	platform_set_drvdata(pdev, sb2);

	irq = irq_of_parse_and_map(np, 0);
	if (irq < 0)
		return dev_err_probe(dev, irq, "failed to parse irq\n");

	ret = devm_request_irq(dev, irq, sb2_inv_int_handler, IRQF_SHARED,
			       dev_name(dev), pdev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to request irq\n");

	ret = device_create_file(dev, &dev_attr_enabled);
	if (ret)
		return dev_err_probe(dev, ret, "failed to create file\n");

	sb2_inv_inte_set(sb2, 1);
	return 0;
}

static int sb2_inv_suspend(struct device *dev)
{
	struct sb2_inv_drvdata *sb2 = dev_get_drvdata(dev);

	sb2->saved_inte = sb2_inv_inte_get(sb2);
	return 0;
}

static int sb2_inv_resume(struct device *dev)
{
	struct sb2_inv_drvdata *sb2 = dev_get_drvdata(dev);

	if (sb2->saved_inte)
		sb2_inv_inte_set(sb2, 1);
	return 0;
}

static const struct dev_pm_ops sb2_inv_pm_ops = {
	.suspend_noirq = sb2_inv_suspend,
	.resume_noirq  = sb2_inv_resume,
};

static const struct of_device_id sb2_inv_match[] = {
	{.compatible = "realtek,sysbrg2-inv", .data = &sb2_inv_desc, },
	{.compatible = "realtek,rtd1625-sysbrg2-inv", .data = &rtd1625_desc, },
	{},
};
MODULE_DEVICE_TABLE(of, sb2_inv_match);

static struct platform_driver sb2_inv_driver = {
	.probe  = sb2_inv_probe,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "rtk-sb2-inv",
		.pm             = &sb2_inv_pm_ops,
		.of_match_table = of_match_ptr(sb2_inv_match),
		.suppress_bind_attrs = true,
	},
};

static int __init rtk_sb2_init(void)
{
	return platform_driver_register(&sb2_inv_driver);
}
subsys_initcall_sync(rtk_sb2_init);

MODULE_DESCRIPTION("Realtek SB2 Invaild Access driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-sb2-inv");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
