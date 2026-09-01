// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek Generic Power Controller
 *
 * Copyright (C) 2021 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define CREATE_TRACE_POINTS
#include "rtk_gpc_trace.h"

/* SRAM Controller */
struct rtk_sram_controller {
	struct device *dev;
	void __iomem *base;
	void __iomem *ctl5;
	void __iomem *ctl8;
	u32 l2h_delay_cycle;
	u32 h2l_delay_cycle;
	u32 std_delay_cycle;
	u32 manual_mask;
	u32 delay_us;
};

#define SRAM_PWR0 0x0
#define SRAM_PWR1 0x4
#define SRAM_PWR2 0x8
#define SRAM_PWR3 0xC
#define SRAM_PWR4 0x10
#define SRAM_PWR5 0x14
#define SRAM_PWR6 0x18
#define SRAM_PWR7 0x1c

#define SRAM_PWR_INT      0x4
#define SRAM_PWR_LAST_CH  (0xf << 8)
#define SRAM_PWR_ON       0x0
#define SRAM_PWR_OFF      0x1

static void rtk_sram_setup(struct rtk_sram_controller *sram)
{
	if (sram->l2h_delay_cycle)
		writel(sram->l2h_delay_cycle, sram->base + SRAM_PWR0);

	if (sram->h2l_delay_cycle)
		writel(sram->h2l_delay_cycle, sram->base + SRAM_PWR1);

	if (sram->manual_mask) {
		u32 val, new_val;

		writel(sram->manual_mask, sram->base + SRAM_PWR3);
		val = readl(sram->base + SRAM_PWR2);
		new_val = val & ~sram->manual_mask;
		if (val != new_val)
			writel(new_val, sram->base + SRAM_PWR2);
	}

	if (sram->std_delay_cycle && sram->ctl8)
		writel(sram->std_delay_cycle, sram->ctl8);
}

static int rtk_sram_power_ready(struct rtk_sram_controller *sram)
{
	unsigned int pollval = 0;

	return readl_poll_timeout(sram->ctl5, pollval, pollval == SRAM_PWR_INT, 0, 500);
}

static void rtk_sram_power_clear_ints(struct rtk_sram_controller *sram)
{
	writel(SRAM_PWR_INT, sram->ctl5);
}

static int rtk_sram_power_set(struct rtk_sram_controller *sram, int on_off)
{
	unsigned int target_val = on_off ? SRAM_PWR_ON : SRAM_PWR_OFF;
	unsigned int val;
	int ret;

	val = readl(sram->base + SRAM_PWR4);
	if ((val & 0xff) == target_val)
		return 1;

	val &= ~0xfff;
	val |= SRAM_PWR_LAST_CH | target_val;

	rtk_sram_power_clear_ints(sram); /* make sure ints is not set */

	writel(val, sram->base + SRAM_PWR4);

	ret = rtk_sram_power_ready(sram);

	rtk_sram_power_clear_ints(sram);

	if (!ret && sram->delay_us)
		udelay(sram->delay_us);

	return ret;
}

static int rtk_sram_power_on(struct rtk_sram_controller *sram)
{
	int ret = rtk_sram_power_set(sram, 1);

	trace_rtk_gpc_sram_power(sram->dev, 1, ret);
	return ret;
}

static int rtk_sram_power_off(struct rtk_sram_controller *sram)
{
	int ret = rtk_sram_power_set(sram, 0);

	trace_rtk_gpc_sram_power(sram->dev, 0, ret);
	return ret;
}

static int rtk_sram_get_power_state(struct rtk_sram_controller *sram)
{
	unsigned int val;

	val = readl(sram->base + SRAM_PWR4);
	return (val & 0xff) == SRAM_PWR_ON;
}

/* ISO Controller */
struct rtk_iso_controller {
	struct device *dev; /* Added dev */
	struct regmap *regmap;
	u32 offset;
	u32 mask;
};

static void rtk_iso_init(struct rtk_iso_controller *iso, struct device *dev,
		struct regmap *regmap, u32 offset, u32 mask)
{
	iso->dev = dev;
	iso->regmap = regmap;
	iso->offset = offset;
	iso->mask = mask;
}

static void rtk_iso_update_reg(struct rtk_iso_controller *iso, int isolate)
{
	unsigned int val;

	if (!iso->offset)
		return;

	val = isolate ? iso->mask : 0;
	regmap_update_bits(iso->regmap, iso->offset, iso->mask, val);
}

static void rtk_iso_power_on(struct rtk_iso_controller *iso)
{
	rtk_iso_update_reg(iso, 0);
	trace_rtk_gpc_iso_control(iso->dev, iso->offset, iso->mask, 0);
}

static void rtk_iso_power_off(struct rtk_iso_controller *iso)
{
	rtk_iso_update_reg(iso, 1);
	trace_rtk_gpc_iso_control(iso->dev, iso->offset, iso->mask, 1);
}

/* GPC Driver */
struct rtk_gpc_domain_desc {
	const char *name;
	u32 iso_bit;
	u32 delay_us_sram;
	u32 delay_us_reset;
	const char * const *rstc_ids;
	u32 n_rstc_ids;

	u32 setup_delays : 1;
	u32 enable_clks_before_sram : 1;
	u32 ignore_iso_pwr : 1;
	u32 workaround_ve_top_ctl : 1;
	u32 workaround_gpu_extra_iso : 1;
	u32 workaround_gpu_wdt : 1;
};

struct rtk_gpc_soc_desc {
	u32 iso_offset;
	u32 num_descs;
	const struct rtk_gpc_domain_desc *descs;
};

#define RTK_MAX_ISO 2

struct rtk_gpc_data {
	const char *name;
	struct device *dev;
	struct rtk_sram_controller sram;
	struct rtk_iso_controller iso[RTK_MAX_ISO];
	u32 num_iso;
	struct generic_pm_domain genpd;
	struct reset_controller_dev rcdev;
	struct regmap *regmap;
	struct clk_bulk_data *clks;
	u32 num_clks;
	struct reset_control_bulk_data *rstcs;
	u32 num_rstcs;
	struct reset_control_bulk_data *init_rstcs;
	u32 num_init_rstcs;
	const struct rtk_gpc_domain_desc *desc;
	u32 clk_enabled_count;
	struct notifier_block rnb;
	struct notifier_block pnb;
};

static inline int should_enable_clks_before_sram(struct rtk_gpc_data *data)
{
	return data->desc->enable_clks_before_sram;
}

static void rtk_gpc_enable_clocks(struct rtk_gpc_data *data)
{
	int ret;

	if (data->clk_enabled_count++ == 0) {
		ret = clk_bulk_prepare_enable(data->num_clks, data->clks);
		WARN_ON_ONCE(ret);
		trace_rtk_gpc_clk_control(data->dev, 1);
	}
}

static void rtk_gpc_disable_clocks(struct rtk_gpc_data *data)
{
	if (data->clk_enabled_count > 0) {
		if (--data->clk_enabled_count == 0) {
			clk_bulk_disable_unprepare(data->num_clks, data->clks);
			trace_rtk_gpc_clk_control(data->dev, 0);
		}
	} else {
		dev_warn(data->dev, "clk disable underflow\n");
	}
}

static int rtk_gpc_is_on(struct rtk_gpc_data *data)
{
	return rtk_sram_get_power_state(&data->sram);
}

static int rtk_gpc_pre_sram_power_on(struct rtk_gpc_data *data)
{
	int ret;

	ret = reset_control_bulk_acquire(data->num_rstcs, data->rstcs);
	if (ret) {
		dev_err(data->dev, "failed to acquire rstcs: %d\n", ret);
		return ret;
	}

	if (should_enable_clks_before_sram(data))
		rtk_gpc_enable_clocks(data);

	return 0;
}

static void rtk_gpc_post_sram_power_on(struct rtk_gpc_data *data, int already_power_on)
{
	int i;

	rtk_gpc_enable_clocks(data);

	reset_control_deassert(data->rstcs[0].rstc); /* reset */
	trace_rtk_gpc_reset_control(data->dev, 0, 1);

	if (!already_power_on) {
		trace_rtk_gpc_reset_control(data->dev, 2, 0);
		reset_control_reset(data->rstcs[2].rstc); /* auto */
		trace_rtk_gpc_reset_control(data->dev, 2, 1);
	}
	reset_control_deassert(data->rstcs[1].rstc); /* bist */
	trace_rtk_gpc_reset_control(data->dev, 1, 1);

	reset_control_bulk_release(data->num_rstcs, data->rstcs);

	if (data->desc->delay_us_reset)
		udelay(data->desc->delay_us_reset);

	for (i = 0; i < data->num_iso; i++)
		rtk_iso_power_on(&data->iso[i]);
}

static int rtk_gpc_genpd_power_on(struct generic_pm_domain *genpd)
{
	struct rtk_gpc_data *data = container_of(genpd, struct rtk_gpc_data, genpd);
	int ret;

	dev_dbg(data->dev, "%s\n", __func__);

	ret = rtk_gpc_pre_sram_power_on(data);
	if (ret)
		return ret;

	ret = rtk_sram_power_on(&data->sram);
	rtk_gpc_post_sram_power_on(data, ret > 0);

	return 0;
}

static int rtk_gpc_pre_sram_power_off(struct rtk_gpc_data *data)
{
	int ret, i;

	ret = reset_control_bulk_acquire(data->num_rstcs, data->rstcs);
	if (ret) {
		dev_warn(data->dev, "failed to acquire rstcs: %d\n", ret);
		return ret;
	}

	for (i = 0; i < data->num_iso; i++)
		rtk_iso_power_off(&data->iso[i]);

	reset_control_assert(data->rstcs[0].rstc);
	trace_rtk_gpc_reset_control(data->dev, 0, 0);
	reset_control_assert(data->rstcs[1].rstc);
	trace_rtk_gpc_reset_control(data->dev, 1, 0);

	rtk_gpc_disable_clocks(data);

	return 0;
}

static void rtk_gpc_post_sram_power_off(struct rtk_gpc_data *data)
{
	if (should_enable_clks_before_sram(data))
		rtk_gpc_disable_clocks(data);
	reset_control_bulk_release(data->num_rstcs, data->rstcs);
}

static int rtk_gpc_genpd_power_off(struct generic_pm_domain *genpd)
{
	struct rtk_gpc_data *data = container_of(genpd, struct rtk_gpc_data, genpd);
	int ret;

	dev_dbg(data->dev, "%s\n", __func__);

	ret = rtk_gpc_pre_sram_power_off(data);
	if (ret)
		return ret;

	rtk_sram_power_off(&data->sram);
	rtk_gpc_post_sram_power_off(data);

	return 0;
}

static int rtk_gpc_genpd_attach_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	struct rtk_gpc_data *data = container_of(genpd, struct rtk_gpc_data, genpd);
	int ret;

	pr_debug("%s: %s %s %s\n", genpd->name, __func__, dev_driver_string(dev), dev_name(dev));

	/* sync power on device attached */
	if (rtk_gpc_is_on(data) && data->clk_enabled_count == 0) {
		ret = rtk_gpc_pre_sram_power_on(data);
		if (ret) {
			dev_err(data->dev, "failed in pre sram power on\n");
			return ret;
		}

		rtk_gpc_post_sram_power_on(data, 0);
	}
	return 0;
}

static void rtk_gpc_genpd_detach_dev(struct generic_pm_domain *genpd, struct device *dev)
{
	pr_debug("%s: %s %s %s\n", genpd->name, __func__, dev_driver_string(dev), dev_name(dev));
}

/* Reset controller */
static int rtk_gpc_rc_reset(struct reset_controller_dev *rcdev, unsigned long idx)
{
	struct rtk_gpc_data *data = container_of(rcdev, struct rtk_gpc_data, rcdev);
	int ret;

	/* Acquire resets as we are in released state usually */
	ret = reset_control_bulk_acquire(data->num_rstcs, data->rstcs);
	if (ret)
		return ret;

	if (data->rstcs[0].rstc) {
		trace_rtk_gpc_reset_control(data->dev, 0, 0);
		ret = reset_control_reset(data->rstcs[0].rstc);
		trace_rtk_gpc_reset_control(data->dev, 0, 1);
		if (ret)
			goto out;
	}

	if (data->rstcs[2].rstc) {
		trace_rtk_gpc_reset_control(data->dev, 2, 0);
		ret = reset_control_reset(data->rstcs[2].rstc);
		trace_rtk_gpc_reset_control(data->dev, 2, 1);
	}

out:
	reset_control_bulk_release(data->num_rstcs, data->rstcs);
	return ret;
}

static const struct reset_control_ops rtk_gpc_reset_ops = {
	.reset = rtk_gpc_rc_reset,
};

static int rtk_gpc_reset_of_xlate(struct reset_controller_dev *rcdev,
				    const struct of_phandle_args *reset_spec)
{
	if (WARN_ON(reset_spec->args_count != 0))
		return -EINVAL;

	return 0;
}

static int fix_hwwdt_issue(struct rtk_gpc_data *data)
{
	if (rtk_gpc_is_on(data)) {
		dev_info(data->dev, "%s: sram aleady on\n", __func__);
		return 0;
	}

	dev_info(data->dev, "%s: power on sram\n", __func__);
	rtk_sram_power_on(&data->sram);
	return 1;
}

static int reboot_cb(struct notifier_block *nb, unsigned long unused, void *unused1)
{
	struct rtk_gpc_data *data = container_of(nb, struct rtk_gpc_data, rnb);

	return fix_hwwdt_issue(data) ? NOTIFY_OK : NOTIFY_DONE;
}

static int panic_cb(struct notifier_block *nb, unsigned long unused, void *unused1)
{
	struct rtk_gpc_data *data = container_of(nb, struct rtk_gpc_data, pnb);

	return fix_hwwdt_issue(data) ? NOTIFY_OK : NOTIFY_DONE;
}

static ssize_t registers_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_gpc_data *data = dev_get_drvdata(dev);
	struct rtk_sram_controller *sram = &data->sram;
	int len = 0;
	int i;
	unsigned int val;

	len += scnprintf(buf + len, PAGE_SIZE - len, "SRAM Registers:\n");
	if (sram->base) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR0: %#x\n", readl(sram->base + SRAM_PWR0));
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR1: %#x\n", readl(sram->base + SRAM_PWR1));
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR2: %#x\n", readl(sram->base + SRAM_PWR2));
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR3: %#x\n", readl(sram->base + SRAM_PWR3));
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR4: %#x\n", readl(sram->base + SRAM_PWR4));
	}

	if (sram->ctl5 != sram->base + SRAM_PWR5)
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR5: %#x\n", readl(sram->ctl5));
	else {
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR5: %#x\n", readl(sram->base + SRAM_PWR5));
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR6: %#x\n", readl(sram->base + SRAM_PWR6));
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR7: %#x\n", readl(sram->base + SRAM_PWR7));
	}

	if (sram->ctl8)
		len += scnprintf(buf + len, PAGE_SIZE - len, "PWR8: %#x\n", readl(sram->ctl8));

	len += scnprintf(buf + len, PAGE_SIZE - len, "\nISO Registers:\n");
	for (i = 0; i < data->num_iso; i++) {
		if (regmap_read(data->iso[i].regmap, data->iso[i].offset, &val))
			val = 0xffffffff;
		len += scnprintf(buf + len, PAGE_SIZE - len, "ISO[%d]: offset=%#x, val=%#x, mask=%#x\n",
				 i, data->iso[i].offset, val, data->iso[i].mask);
	}

	return len;
}
static DEVICE_ATTR_RO(registers);

static ssize_t info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_gpc_data *data = dev_get_drvdata(dev);
	const struct rtk_gpc_domain_desc *desc = data->desc;
	int len = 0;

	len += scnprintf(buf + len, PAGE_SIZE - len, "Domain: %s\n", desc->name);
	len += scnprintf(buf + len, PAGE_SIZE - len, "iso_bit: %d\n", desc->iso_bit);
	len += scnprintf(buf + len, PAGE_SIZE - len, "delay_us_sram: %d\n", desc->delay_us_sram);
	len += scnprintf(buf + len, PAGE_SIZE - len, "delay_us_reset: %d\n", desc->delay_us_reset);
	len += scnprintf(buf + len, PAGE_SIZE - len, "setup_delays: %d\n", desc->setup_delays);
	len += scnprintf(buf + len, PAGE_SIZE - len, "enable_clks_before_sram: %d\n", desc->enable_clks_before_sram);
	len += scnprintf(buf + len, PAGE_SIZE - len, "ignore_iso_pwr: %d\n", desc->ignore_iso_pwr);
	len += scnprintf(buf + len, PAGE_SIZE - len, "workaround_ve_top_ctl: %d\n", desc->workaround_ve_top_ctl);
	len += scnprintf(buf + len, PAGE_SIZE - len, "workaround_gpu_extra_iso: %d\n", desc->workaround_gpu_extra_iso);
	len += scnprintf(buf + len, PAGE_SIZE - len, "workaround_gpu_wdt: %d\n", desc->workaround_gpu_wdt);

	return len;
}
static DEVICE_ATTR_RO(info);

static struct attribute *rtk_gpc_attrs[] = {
	&dev_attr_registers.attr,
	&dev_attr_info.attr,
	NULL,
};
ATTRIBUTE_GROUPS(rtk_gpc);

static const struct rtk_gpc_domain_desc *rtk_gpc_find_desc_by_name(const struct rtk_gpc_soc_desc *soc_desc,
	const char *name)
{
	int i;

	for (i = 0; i < soc_desc->num_descs; i++)
		if (!strcmp(name, soc_desc->descs[i].name))
			return &soc_desc->descs[i];

	return NULL;
}

static void rtk_gpc_setup(struct rtk_gpc_data *data)
{
	struct device *dev = data->dev;
	int ret;

	rtk_sram_setup(&data->sram);

	if (data->init_rstcs) {
		ret = reset_control_bulk_acquire(data->num_init_rstcs, data->init_rstcs);
		if (ret) {
			dev_warn(dev, "failed in reset_control_bulk_acquire(), skip deassert\n");
		} else {
			ret = reset_control_bulk_deassert(data->num_init_rstcs, data->init_rstcs);
			if (ret)
				dev_warn(dev, "failed in reset_control_bulk_deassert()\n");
			reset_control_bulk_release(data->num_init_rstcs, data->init_rstcs);
		}
	}
}

static int rtk_gpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct rtk_gpc_data *data;
	struct resource *res;
	int ret;
	int i;
	int power_off;
	const struct rtk_gpc_soc_desc *soc_desc;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = of_property_read_string(np, "label", &data->name);
	if (ret) {
		dev_err(dev, "failed to get label: %d\n", ret);
		return ret;
	}

	soc_desc = of_device_get_match_data(dev);
	if (!soc_desc) {
		dev_err(dev, "no match data\n");
		return -EINVAL;
	}

	data->dev = dev;

	data->desc = rtk_gpc_find_desc_by_name(soc_desc, data->name);
	if (!data->desc) {
		dev_err(dev, "invalid config for %s\n", data->name);
		return -EINVAL;
	}

	data->regmap = syscon_node_to_regmap(np->parent);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		dev_err(dev, "failed to get syscon regmap from parent: %d\n", ret);
		return ret;
	}

	/* Mapping SRAM */
	data->sram.base = devm_platform_ioremap_resource_byname(pdev, "ctl");
	if (IS_ERR(data->sram.base))
		return dev_err_probe(dev, PTR_ERR(data->sram.base), "failed to get iomem ctl\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ctl5");
	if (!res)
		data->sram.ctl5 = data->sram.base + SRAM_PWR5;
	else
		data->sram.ctl5 = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->sram.ctl5))
		return dev_err_probe(dev, PTR_ERR(data->sram.ctl5), "failed to get iomem ctl5\n");

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ctl8");
	if (res)
		data->sram.ctl8 = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->sram.ctl8))
		return dev_err_probe(dev, PTR_ERR(data->sram.ctl8), "failed to get iomem ctl8\n");
	data->sram.dev = dev;

	if (!data->desc->ignore_iso_pwr) {
		rtk_iso_init(&data->iso[0], dev, data->regmap, soc_desc->iso_offset,
			     BIT(data->desc->iso_bit));
		data->num_iso = 1;

		if (data->desc->workaround_gpu_extra_iso) {
			rtk_iso_init(&data->iso[1], dev, data->regmap, 0x078, BIT(1));
			data->num_iso = 2;
		}
	}

	ret = devm_clk_bulk_get_all(dev, &data->clks);
	if (ret < 0) {
		dev_err(dev, "failed to get clk: %d\n", ret);
		return ret;
	}
	data->num_clks = ret;

	data->num_rstcs = 3;
	data->rstcs = devm_kcalloc(dev, data->num_rstcs, sizeof(*data->rstcs), GFP_KERNEL);
	if (!data->rstcs)
		return -ENOMEM;

	data->rstcs[0].id = "reset";
	data->rstcs[1].id = "bist";
	data->rstcs[2].id = "auto";

	ret = devm_reset_control_bulk_get_optional_exclusive_released(dev, data->num_rstcs, data->rstcs);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get rstcs\n");

	if (data->rstcs[0].rstc || data->rstcs[2].rstc) {
		data->rcdev.owner            = THIS_MODULE;
		data->rcdev.ops              = &rtk_gpc_reset_ops;
		data->rcdev.nr_resets        = 1;
		data->rcdev.of_node          = dev->of_node;
		data->rcdev.of_reset_n_cells = 0;
		data->rcdev.of_xlate         = rtk_gpc_reset_of_xlate;

		ret = devm_reset_controller_register(dev, &data->rcdev);
		if (ret) {
			dev_err(dev, "failed to register reset_controller: %d\n", ret);
			return ret;
		}
	}

	if (data->desc->n_rstc_ids) {
		data->num_init_rstcs = data->desc->n_rstc_ids;
		data->init_rstcs = devm_kcalloc(dev, data->num_init_rstcs, sizeof(*data->init_rstcs), GFP_KERNEL);
		if (!data->init_rstcs)
			return -ENOMEM;

		for (i = 0; i < data->num_init_rstcs; i++)
			data->init_rstcs[i].id = data->desc->rstc_ids[i];

		ret = devm_reset_control_bulk_get_optional_exclusive_released(
			dev, data->num_init_rstcs, data->init_rstcs);
		if (ret)
			return dev_err_probe(dev, ret, "failed to get init rstcs\n");
	}

	power_off = !rtk_gpc_is_on(data);

	dev_set_drvdata(dev, data);

	data->genpd.name       = data->name;
	data->genpd.power_on   = rtk_gpc_genpd_power_on;
	data->genpd.power_off  = rtk_gpc_genpd_power_off;
	data->genpd.attach_dev = rtk_gpc_genpd_attach_dev;
	data->genpd.detach_dev = rtk_gpc_genpd_detach_dev;
	ret = pm_genpd_init(&data->genpd, NULL, power_off);
	if (ret) {
		dev_err(dev, "failed to init genpd: %d\n", ret);
		return ret;
	}

	ret = of_genpd_add_provider_simple(np, &data->genpd);
	if (ret) {
		dev_err(dev, "failed to add genpd of provider: %d\n", ret);
		pm_genpd_remove(&data->genpd);
		return ret;
	}

	if (data->desc->workaround_gpu_wdt) {
		dev_info(dev, "add nbs to fix hwwdt issue\n");

		data->rnb.notifier_call = reboot_cb;
		register_reboot_notifier(&data->rnb);
		data->pnb.notifier_call = panic_cb;
		atomic_notifier_chain_register(&panic_notifier_list, &data->pnb);
	}

	if (data->desc->workaround_ve_top_ctl)
		data->sram.manual_mask = 0x00008000;

	if (data->desc->setup_delays) {
		data->sram.l2h_delay_cycle = 0xf;
		data->sram.h2l_delay_cycle = 0xf;
		data->sram.std_delay_cycle = 0x32;
	}

	of_property_read_u32(np, "realtek,l2h_delay_cycle", &data->sram.l2h_delay_cycle);
	of_property_read_u32(np, "realtek,h2l_delay_cycle", &data->sram.h2l_delay_cycle);
	of_property_read_u32(np, "realtek,std_delay_cycle", &data->sram.std_delay_cycle);
	of_property_read_u32(np, "realtek,manual_mask", &data->sram.manual_mask);
	dev_info(dev, "delay: l2h=%#x, h2l=%#x, mm=%#x, std=%#x\n",
		 data->sram.l2h_delay_cycle, data->sram.h2l_delay_cycle, data->sram.manual_mask,
		 data->sram.std_delay_cycle);

	rtk_gpc_setup(data);
	return 0;
}

static int rtk_gpc_resume(struct device *dev)
{
	struct rtk_gpc_data *data = dev_get_drvdata(dev);

	dev_info(dev, "%s enter\n", __func__);
	rtk_gpc_setup(data);
	dev_info(dev, "%s exit\n", __func__);

	return 0;
}

static const struct dev_pm_ops rtk_gpc_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(NULL, rtk_gpc_resume)
};

static const struct rtk_gpc_domain_desc rtd1295_gpc_descs[] = {
	{ .name = "ve1", .iso_bit = 0 },
	{ .name = "gpu", .iso_bit = 1 },
	{ .name = "ve2", .iso_bit = 4 },
	{ .name = "ve3", .iso_bit = 6 },
	{ .name = "nat", .iso_bit = 18 },
};

static const struct rtk_gpc_soc_desc rtd1295_soc_desc = {
	.iso_offset = 0x400,
	.descs = rtd1295_gpc_descs,
	.num_descs = ARRAY_SIZE(rtd1295_gpc_descs),
};

static const struct rtk_gpc_domain_desc rtd1395_gpc_descs[] = {
	{ .name = "ve1", .iso_bit = 0, .workaround_ve_top_ctl = 1 },
	{ .name = "ve2", .iso_bit = 1 },
	{ .name = "gpu", .iso_bit = 3 },
	{ .name = "ve3", .iso_bit = 10 },
};

static const struct rtk_gpc_soc_desc rtd1395_soc_desc = {
	.iso_offset = 0xfd0,
	.descs = rtd1395_gpc_descs,
	.num_descs = ARRAY_SIZE(rtd1395_gpc_descs),
};

static const struct rtk_gpc_domain_desc rtd1619_gpc_descs[] = {
	{ .name = "ve1", .iso_bit = 0, .workaround_ve_top_ctl = 1 },
	{ .name = "ve2", .iso_bit = 1 },
	{ .name = "gpu", .iso_bit = 3, .workaround_gpu_extra_iso = 1 },
	{ .name = "hdmirx", .iso_bit = 9 },
	{ .name = "ve3", .iso_bit = 10 },
};

static const struct rtk_gpc_soc_desc rtd1619_soc_desc = {
	.iso_offset = 0xfd0,
	.descs = rtd1619_gpc_descs,
	.num_descs = ARRAY_SIZE(rtd1619_gpc_descs),
};

static const struct rtk_gpc_domain_desc rtd1319_gpc_descs[] = {
	{ .name = "ve1", .iso_bit = 0 },
	{ .name = "ve2", .iso_bit = 1 },
	{ .name = "gpu", .iso_bit = 3 },
	{ .name = "ve3", .iso_bit = 10 },
};

static const struct rtk_gpc_soc_desc rtd1319_soc_desc = {
	.iso_offset = 0xfd0,
	.descs = rtd1319_gpc_descs,
	.num_descs = ARRAY_SIZE(rtd1319_gpc_descs),
};

static const struct rtk_gpc_domain_desc rtd1619b_gpc_descs[] = {
	{ .name = "ve1", .iso_bit = 0 },
	{ .name = "ve2", .iso_bit = 1 },
	{ .name = "gpu", .iso_bit = 3, .workaround_gpu_wdt = 1 },

	{ .name = "ve3", .iso_bit = 10 },
	{ .name = "npu", .iso_bit = 12, .enable_clks_before_sram = 1, .setup_delays = 1 },
};

static const struct rtk_gpc_soc_desc rtd1619b_soc_desc = {
	.iso_offset = 0xfd0,
	.descs = rtd1619b_gpc_descs,
	.num_descs = ARRAY_SIZE(rtd1619b_gpc_descs),
};

static const struct rtk_gpc_domain_desc rtd1319d_gpc_descs[] = {
	{ .name = "ve1", .iso_bit = 0, .enable_clks_before_sram = 1, .setup_delays = 1 },
	{ .name = "ve2", .iso_bit = 1 },
	{ .name = "gpu", .iso_bit = 3 },
	{ .name = "ve3", .iso_bit = 10 },
};

static const struct rtk_gpc_soc_desc rtd1319d_soc_desc = {
	.iso_offset = 0xfd0,
	.descs = rtd1319d_gpc_descs,
	.num_descs = ARRAY_SIZE(rtd1319d_gpc_descs),
};

static const struct rtk_gpc_domain_desc rtd1315e_gpc_descs[] = {
	{ .name = "ve1", .iso_bit = 0, .enable_clks_before_sram = 1, .setup_delays = 1 },
	{ .name = "ve2", .iso_bit = 1 },
	{ .name = "gpu", .iso_bit = 3 },
	{ .name = "ve3", .iso_bit = 10 },
};

static const struct rtk_gpc_soc_desc rtd1315e_soc_desc = {
	.iso_offset = 0x300,
	.descs = rtd1315e_gpc_descs,
	.num_descs = ARRAY_SIZE(rtd1315e_gpc_descs),
};

static const char * const rtd1625_ve1_rstc_ids[] = { "mmu", "mmu_func", "bist_common" };
static const char * const rtd1625_ve4_rstc_ids[] = { "bist_common", "bist_on" };

static const struct rtk_gpc_domain_desc rtd1625_gpc_descs[] = {
	{
		.name = "ve1",
		.iso_bit = 0,
		.enable_clks_before_sram = 1,
		.setup_delays = 1,
		.rstc_ids = rtd1625_ve1_rstc_ids,
		.n_rstc_ids = ARRAY_SIZE(rtd1625_ve1_rstc_ids),
	},
	{
		.name = "ve2",
		.iso_bit = 1,
	},
	{
		.name = "gpu",
		.iso_bit = 3,
	},
	{
		.name = "ve4",
		.iso_bit = 10,
		.ignore_iso_pwr = 1,
		.rstc_ids = rtd1625_ve4_rstc_ids,
		.n_rstc_ids = ARRAY_SIZE(rtd1625_ve4_rstc_ids),
	},
	{
		.name = "npu",
		.iso_bit = 12,
		.enable_clks_before_sram = 1,
		.delay_us_sram = 10,
		.delay_us_reset = 10,
	},
};
static const struct rtk_gpc_soc_desc rtd1625_soc_desc = {
	.iso_offset = 0x300,
	.descs = rtd1625_gpc_descs,
	.num_descs = ARRAY_SIZE(rtd1625_gpc_descs),
};

static const struct of_device_id rtk_gpc_match[] = {
	{ .compatible = "realtek,rtd1295-gpc", .data = &rtd1295_soc_desc, },
	{ .compatible = "realtek,rtd1395-gpc", .data = &rtd1395_soc_desc, },
	{ .compatible = "realtek,rtd1619-gpc", .data = &rtd1619_soc_desc, },
	{ .compatible = "realtek,rtd1319-gpc", .data = &rtd1319_soc_desc, },
	{ .compatible = "realtek,rtd1619b-gpc", .data = &rtd1619b_soc_desc, },
	{ .compatible = "realtek,rtd1319d-gpc", .data = &rtd1319d_soc_desc, },
	{ .compatible = "realtek,rtd1315e-gpc", .data = &rtd1315e_soc_desc, },
	{ .compatible = "realtek,rtd1625-gpc", .data = &rtd1625_soc_desc, },
	{}
};
MODULE_DEVICE_TABLE(of, rtk_gpc_match);

static struct platform_driver rtk_gpc_driver = {
	.probe = rtk_gpc_probe,
	.driver = {
		.name = "rtk-gpc",
		.of_match_table = of_match_ptr(rtk_gpc_match),
		.pm = &rtk_gpc_pm_ops,
		.suppress_bind_attrs = true,
		.dev_groups = rtk_gpc_groups,
	},
};

static int __init rtk_gpc_init(void)
{
	return platform_driver_register(&rtk_gpc_driver);
}
fs_initcall(rtk_gpc_init);

MODULE_DESCRIPTION("Realtek Generic Power Controller");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
