// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek uncore PMU driver for DDR memory controller
 *
 * Copyright (c) 2019 Realtek Semiconductor Corp.
 * Copyright (C) 2019 Ping-Hsiung Chiu <phelic@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>

#include "rtk_uncore_pmu.h"
#include "rtk_rbus_pmu.h"


/* Rbus event format */
RTK_PMU_FORMAT_ATTR(event,		"config:0-31");
RTK_PMU_FORMAT_ATTR(rbus_target,	"config1:0-31");
RTK_PMU_FORMAT_ATTR(start_addr,		"config1:0-31");
RTK_PMU_FORMAT_ATTR(end_addr,		"config2:0-31");

static struct attribute *rtk_rbus_format_attrs[] = {
	RTK_PMU_FORMAT_REF(event),
	RTK_PMU_FORMAT_REF(rbus_target),
	RTK_PMU_FORMAT_REF(start_addr),
	RTK_PMU_FORMAT_REF(end_addr),
	NULL
};

struct attribute_group rtk_rbus_format_attr_group = {
	.name = "format",
	.attrs = rtk_rbus_format_attrs,
};
EXPORT_SYMBOL(rtk_rbus_format_attr_group);

static inline int
__pmc_to_ctrl_bit_v2(struct rtk_pmc_set *ps, union rtk_pmc_desc pmc)
{
	/*
	 * The sequence of overflow bit vector is differenct from counters in
	 * pmcgs, need remapping to find correct event
	 */
	static const int mapping[] = {
		[RBUS_PMC__V2_REQ] = 0,
		[RBUS_PMC__V2_RTG_TOTAL] = 1,
		[RBUS_PMC__V2_RTA_MAX] = 3,
		[RBUS_PMC__V2_RTA_TOTAL] = 2,
	};
	int ret;

	ret = ps->meta->clients[pmc.idx] * RBUS_OVERFLOW_PMCG_WIDTH +
		mapping[pmc.usage] + RBUS_OVERFLOW_OFFSET;

	while (ret > 32)
		ret = ret - RBUS_OVERFLOW_STATUS_WIDTH + RBUS_OVERFLOW_OFFSET;

	return ret;
}

static inline int
__pmc_to_ctrl_bit_v3(struct rtk_pmc_set *ps, union rtk_pmc_desc pmc)
{
	int ret;

	ret = ps->meta->clients[pmc.idx] * RBUS_OVERFLOW_PMCG_WIDTH +
		pmc.usage + RBUS_OVERFLOW_OFFSET;

	while (ret > 32)
		ret = ret - RBUS_OVERFLOW_STATUS_WIDTH + RBUS_OVERFLOW_OFFSET;

	return ret;
}

static inline int
__pmc_to_ctrl_bit_v4(struct rtk_pmc_set *ps, union rtk_pmc_desc pmc)
{
	return (pmc.idx * (RBUS_ARB_OVERFLOW_PMCG_WIDTH + 1) + pmc.usage) % 32;
}

static inline int
__pmc_to_ctrl_offset(union rtk_pmc_desc pmc)
{
	return (pmc.set == PMC_SET__RBUS_PRE_ARB) ? 0 : RBUS_POST_ARB_CTRL_OFFSET;
}

static void
rtk_rbus_ps_enable(struct rtk_pmc_set *ps, struct perf_event *event)
{
	unsigned int val;
	union rtk_pmc_desc pmc = rtk_event_pmc_desc(event);
	struct rtk_pmu *pmu = ps->pmu;
	struct device *dev = &pmu->pdev->dev;

	dev_dbg(dev, "enable %s, pmc:%x, config:%llx, mask:%x\n",
	     ps->name, event->hw.idx, event->hw.config,
	     rtk_event_target_mask(event));

	val = rtk_readl(rtk_event_config_addr(event));
	val |= rtk_event_config(event);
	rtk_writel(rtk_event_config_addr(event), val);

	if (!rtk_pmu_has_no_overflow(ps->pmu)) {
		rtk_writel(__get_ov_en_reg(ps, pmc.idx),
			   BIT(0) | BIT(ps->pmc_to_ctrl_bit(ps, pmc)));
	}
}

static void
rtk_rbus_ps_enable_v2(struct rtk_pmc_set *ps, struct perf_event *event)
{
	struct rtk_pmu *pmu = ps->pmu;
	struct device *dev = &pmu->pdev->dev;
	union rtk_pmc_desc pmc = rtk_event_pmc_desc(event);
	unsigned int bit_offset = __pmc_to_ctrl_offset(pmc);
	unsigned int val = 0;

	dev_dbg(dev, "enable %s, pmc:%x, config:%llx, bit_offset: %d\n",
	     ps->name, event->hw.idx, event->hw.config,
	     bit_offset);

	val = rtk_readl(ps->pmu->ctrl) | BIT(bit_offset + pmc.idx);
	rtk_writel(ps->pmu->ctrl, val);

	if (!rtk_pmu_has_no_overflow(ps->pmu)) {
		val = rtk_readl(__get_ov_en_reg_v2(ps, pmc.idx));
		rtk_writel(__get_ov_en_reg_v2(ps, pmc.idx),
			   val | BIT(ps->pmc_to_ctrl_bit(ps, pmc)));
	}
}

static void
rtk_rbus_ps_disable(struct rtk_pmc_set *ps, struct perf_event *event)
{
	struct rtk_pmu *pmu = ps->pmu;
	struct device *dev = &pmu->pdev->dev;
	unsigned int val;
	union rtk_pmc_desc pmc = rtk_event_pmc_desc(event);

	dev_dbg(dev, "disable %s, pmc:%x, config:%llx, mask:%x, val:%llx\n",
	     ps->name, event->hw.idx, event->hw.config,
	     rtk_event_target_mask(event),
	     event->hw.config & ~rtk_event_target_mask(event));

	/*
	 * Here play a bit-wise magic for compatibility of both Rbus PMU version
	 */
	val = rtk_readl(rtk_event_config_addr(event));
	val &= ~rtk_event_target_mask(event);
	val |= event->hw.config & ~rtk_event_target_mask(event);
	rtk_writel(rtk_event_config_addr(event), val);

	if (!rtk_pmu_has_no_overflow(ps->pmu))
		rtk_writel(__get_ov_en_reg(ps, pmc.idx),
			   BIT(ps->pmc_to_ctrl_bit(ps, pmc)));
}

static void
rtk_rbus_ps_disable_v2(struct rtk_pmc_set *ps, struct perf_event *event)
{
	struct rtk_pmu *pmu = ps->pmu;
	struct device *dev = &pmu->pdev->dev;
	union rtk_pmc_desc pmc = rtk_event_pmc_desc(event);
	unsigned int bit_offset = __pmc_to_ctrl_offset(pmc);
	unsigned int val = 0;

	dev_dbg(dev, "disable %s, pmc:%x, config:%llx, bit_offset:%d\n",
	     ps->name, event->hw.idx, event->hw.config,
	     bit_offset);

	val = rtk_readl(ps->pmu->ctrl) & ~(BIT(bit_offset + pmc.idx));
	rtk_writel(ps->pmu->ctrl, val);

	val = rtk_readl(__get_ov_en_reg_v2(ps, pmc.idx));
	rtk_writel(__get_ov_en_reg_v2(ps, pmc.idx), val & ~BIT(ps->pmc_to_ctrl_bit(ps, pmc)));
}

static int
rtk_rbus_pmc_config(u64 config)
{
	return (int)(config & HWC_MASK);
}

static int
rtk_rbus_pmc_config_v2(u64 config)
{
	return (int)(config & ARB_HWC_MASK);
}

static struct rtk_pmc_set *
rtk_rbus_get_pmc_set(struct rtk_pmu *pmu, struct perf_event *event)
{
	union rtk_rbus_event_desc desc = get_event_desc(event);

	return (desc.set < pmu->nr_pmcss) ? pmu->pmcss[desc.set] : NULL;
}

static struct rtk_pmc_set *
rtk_rbus_get_pmc_set_v2(struct rtk_pmu *pmu, struct perf_event *event)
{
	union rtk_rbus_arb_event_desc desc = get_event_desc_v2(event);

	return (desc.set < pmu->nr_pmcss) ? pmu->pmcss[desc.set] : NULL;
}

static void
rtk_rbus_refresh(struct rtk_pmu *pmu, struct perf_event *event)
{
	struct rtk_pmc_set *ps = pmu->get_pmc_set(pmu, event);
	struct device *dev = &pmu->pdev->dev;
	unsigned long flags;
	u64 start, end;

	raw_spin_lock_irqsave(&ps->ps_lock, flags);
	start = ktime_get_mono_fast_ns();
	rtk_pmu_drv_pmc_inc(pmu, RBUS_REFRESH);

	rtk_rbus_ps_disable(ps, event);

	rtk_ps_refresh_pmcg(ps, event);

	rtk_rbus_ps_enable(ps, event);

	rtk_ps_refresh_pmcg_done(ps, event);

	end = ktime_get_mono_fast_ns();
	raw_spin_unlock_irqrestore(&ps->ps_lock, flags);

	if (unlikely((end - start) > REFRESH_TH))
		dev_err(dev, "** refresh took long time: %lluns\n", end - start);
}

static struct perf_event *
__find_event_by_ov_v2(struct rtk_pmc_set *ps, int bit)
{
	/*
	 * overflow status bit from MSB to LSB is:
	 *	RTA_MAX, RTA_TOTAL, RTG_TOTAL, REQ
	 */
	static const int mapping[] = {
		RBUS_PMC__V2_REQ,
		RBUS_PMC__V2_RTG_TOTAL,
		RBUS_PMC__V2_RTA_TOTAL,
		RBUS_PMC__V2_RTA_MAX,
	};
	int usage = bit & GENMASK(1, 0);
	int client = bit / RBUS_OVERFLOW_PMCG_WIDTH;
	int pmcg;

	for (pmcg = 0; pmcg < ps->meta->nr_clients; pmcg++)
		if (ps->meta->clients[pmcg] == client)
			break;
	if (pmcg >= ps->meta->nr_pmcgs)
		return NULL;

	return ps->tracking[pmcg].events[mapping[usage]];
}

static struct perf_event *
__find_event_by_ov_v3(struct rtk_pmc_set *ps, int bit)
{
	int usage = bit & GENMASK(1, 0);
	int client = bit / RBUS_OVERFLOW_PMCG_WIDTH;
	int pmcg;

	for (pmcg = 0; pmcg < ps->meta->nr_clients; pmcg++)
		if (ps->meta->clients[pmcg] == client)
			break;
	if (pmcg >= ps->meta->nr_pmcgs)
		return NULL;

	return ps->tracking[pmcg].events[usage];
}

static struct perf_event *
__find_event_by_ov_v4(struct rtk_pmc_set *ps, int bit)
{
	int usage = bit & GENMASK(1, 0);
	int pmcg = bit / (RBUS_ARB_OVERFLOW_PMCG_WIDTH + 1);

	if (pmcg >= ps->meta->nr_pmcgs)
		return NULL;

	return ps->tracking[pmcg].events[usage];
}

static void
update_overflow_event(struct rtk_pmu *pmu, unsigned int part,
		      unsigned long status)
{
	struct rtk_pmc_set *ps;
	unsigned long mask;
	struct perf_event *ev;
	int i;

	ps = pmu->pmcss[PMC_SET__RBUS];
	mask = status >> RBUS_OVERFLOW_OFFSET;
	for_each_set_bit(i, &mask, RBUS_OVERFLOW_STATUS_WIDTH) {
		ev = ps->find_event_by_ov(ps, i +
					(part * RBUS_OVERFLOW_STATUS_WIDTH));
		compensate_event(ps, ev);
	}
}

static void
update_overflow_event_v2(struct rtk_pmu *pmu, unsigned int arb,
		      unsigned long status, int bit_offset)
{
	struct rtk_pmc_set *ps;
	struct perf_event *ev;
	int i;

	if (arb == PMC_SET__RBUS_PRE_ARB)
		ps = pmu->pmcss[PMC_SET__RBUS_PRE_ARB];
	else if (arb == PMC_SET__RBUS_POST_ARB)
		ps = pmu->pmcss[PMC_SET__RBUS_POST_ARB];

	for_each_set_bit(i, &status, 32) {
		ev = ps->find_event_by_ov(ps, i + bit_offset);
		if (ev)
			compensate_event(ps, ev);
	}
}

static unsigned int
read_overflow_status(struct rtk_pmu *pmu, unsigned int part)
{
	u32 status;
	u32 ov_en;
	unsigned long en_addr = (unsigned long)pmu->base;
	unsigned long status_addr = (unsigned long)pmu->base;

	if (!part) {
		en_addr += RBUS_OVERFLOW_ENABLE_0;
		status_addr += RBUS_OVERFLOW_STATUS_0;
	} else {
		en_addr += RBUS_OVERFLOW_ENABLE_1;
		status_addr += RBUS_OVERFLOW_STATUS_1;
	}

	ov_en = rtk_readl(en_addr);
	rtk_writel(en_addr, ov_en);

	status = rtk_readl(status_addr);
	rtk_writel(status_addr, status);

	rtk_writel(en_addr, ov_en | BIT(0));

	return status;
}

static unsigned int
read_overflow_status_v2(struct rtk_pmu *pmu, unsigned int arb, unsigned int part)
{
	u32 status;
	u32 ov_en;
	unsigned long en_addr = (unsigned long)pmu->base;
	unsigned long status_addr = (unsigned long)pmu->base;

	if (!part) {
		if (arb == PMC_SET__RBUS_PRE_ARB) {
			en_addr += RBUS_PRE_ARB_OVERFLOW_ENABLE_0;
			status_addr += RBUS_PRE_ARB_OVERFLOW_STATUS_0;
		} else if (arb == PMC_SET__RBUS_POST_ARB) {
			en_addr += RBUS_POST_ARB_OVERFLOW_ENABLE_0;
			status_addr += RBUS_POST_ARB_OVERFLOW_STATUS_0;
		}
	} else {
		en_addr += RBUS_PRE_ARB_OVERFLOW_ENABLE_1;
		status_addr += RBUS_PRE_ARB_OVERFLOW_STATUS_1;
	}

	ov_en = rtk_readl(en_addr);
	rtk_writel(en_addr, ov_en);

	status = rtk_readl(status_addr);
	rtk_writel(status_addr, status);

	return status;
}

static irqreturn_t
overflow_handler(int irq, void *rpmu)
{
#define NOP_THRESHOLD	3

	struct rtk_pmu *pmu = rpmu;
	struct device *dev = &pmu->pdev->dev;
	bool handled = IRQ_NONE;
	u32 ov0 = 0, ov1 = 0;
	static unsigned int nop_count;

	/*
	 * Interrupts may be fired by memory trash function instead of counter
	 * overflow, reconfirm the necessity of overflow handling.
	 */
	if (!rtk_pmu_has_no_overflow(pmu)) {
		ov0 = read_overflow_status(pmu, 0);
		if (__has_multiple_ov(pmu))
			ov1 = read_overflow_status(pmu, 1);

		nop_count = (!ov0 && !ov1) ? nop_count + 1 : 0;

		/* There is indeed at least a PMU overflow occurs */
		if (nop_count == 0) {
			if (ov0)
				update_overflow_event(pmu, 0, ov0);
			if (ov1)
				update_overflow_event(pmu, 1, ov1);
			handled = IRQ_HANDLED;
		} else if (nop_count >= NOP_THRESHOLD) {
			dev_err(dev, "interrupted with no overflow %d times", nop_count);
			nop_count = 0;
		}
	}

	rtk_pmu_drv_pmc_inc(pmu, RBUS_REFRESH);

	return IRQ_RETVAL(handled);
}

static irqreturn_t
overflow_handler_v2(int irq, void *rpmu)
{
#define NOP_THRESHOLD	3

	struct rtk_pmu *pmu = rpmu;
	struct device *dev = &pmu->pdev->dev;
	bool handled = IRQ_NONE;
	u32 pre_arb_ov0 = 0, pre_arb_ov1 = 0, post_arb_ov0 = 0;
	static unsigned int nop_count;

	/*
	 * Interrupts may be fired by memory trash function instead of counter
	 * overflow, reconfirm the necessity of overflow handling.
	 */
	if (!rtk_pmu_has_no_overflow(pmu)) {
		pre_arb_ov0 = read_overflow_status_v2(pmu, PMC_SET__RBUS_PRE_ARB, 0);
		if (__has_multiple_ov(pmu))
			pre_arb_ov1 = read_overflow_status_v2(pmu, PMC_SET__RBUS_PRE_ARB, 1);

		post_arb_ov0 = read_overflow_status_v2(pmu, PMC_SET__RBUS_POST_ARB, 0);

		nop_count = (!pre_arb_ov0 && !pre_arb_ov1 && !post_arb_ov0) ? nop_count + 1 : 0;

		/* There is indeed at least a PMU overflow occurs */
		if (nop_count == 0) {
			if (pre_arb_ov0)
				update_overflow_event_v2(pmu, PMC_SET__RBUS_PRE_ARB, pre_arb_ov0, 0);
			/* Mapped to PRE ARB sets starting from 8 */
			if (pre_arb_ov1)
				update_overflow_event_v2(pmu, PMC_SET__RBUS_PRE_ARB, pre_arb_ov1, 32);
			if (post_arb_ov0)
				update_overflow_event_v2(pmu, PMC_SET__RBUS_POST_ARB, post_arb_ov0, 0);
			handled = IRQ_HANDLED;
		} else if (nop_count >= NOP_THRESHOLD) {
			dev_err(dev, "interrupted with no overflow %d times", nop_count);
			nop_count = 0;
		}
	}

	rtk_pmu_drv_pmc_inc(pmu, RBUS_REFRESH);

	return IRQ_RETVAL(handled);
}

static int
rtk_rbus_check_event(struct rtk_pmu *pmu, struct perf_event *event)
{
	struct device *dev = &pmu->pdev->dev;
	struct rtk_pmc_set *ps = pmu->get_pmc_set(pmu, event);
	union rtk_rbus_event_desc desc = get_event_desc(event);
	int i;

	if (ps == NULL || desc.usage >= ps->meta->group_size) {
		dev_dbg(dev, "ev:%llx out of range\n", event->attr.config);
		return false;
	}

	/* check clients for event validity */
	for (i = 0; i < ps->meta->nr_clients; i++) {
		if (desc.target == ps->meta->clients[i])
			return true;
	}

	dev_dbg(dev, "not valid client, config:%#llx, target:%#x, set:%d\n",
	     event->attr.config, desc.target, desc.set);

	return false;
}

static int
__check_custom_event(struct rtk_pmu *pmu, struct perf_event *event)
{
	struct device *dev = &pmu->pdev->dev;
	union rtk_rbus_arb_event_desc desc = get_event_desc_v2(event);
	u64 config1 = event->attr.config1;
	u64 config2 = event->attr.config2;
	int target_rbus_mask = GENMASK(ARB_CPU_ID_OFFSET - 1, 0);
	int target_rbus = desc.target & target_rbus_mask;

	if (target_rbus != RBUS_CUSTOM_RANGE_ID)
		return true;

	if (desc.set == PMC_SET__RBUS_PRE_ARB) {
		if (config2 != 0) {
			dev_err(dev, "pre-arb custom event should not set end_addr\n");
			return false;
		}

		if ((config1 == 0) || (config1 & ~ARB_RBUS_TARGET_MASK)) {
			dev_err(dev, "invalid rbus_target=0x%llx\n", config1);
			return false;
		}
	} else if (desc.set == PMC_SET__RBUS_POST_ARB) {
		if (config1 == 0 || config2 == 0) {
			dev_err(dev, "start_addr or end_addr is zero\n");
			return false;
		}

		if (config1 >= config2) {
			dev_err(dev, "start_addr should be less than end_addr\n");
			return false;
		}
	}

	return true;
}

static int
rtk_rbus_check_event_v2(struct rtk_pmu *pmu, struct perf_event *event)
{
	struct device *dev = &pmu->pdev->dev;
	struct rtk_pmc_set *ps = pmu->get_pmc_set(pmu, event);
	union rtk_rbus_arb_event_desc desc = get_event_desc_v2(event);
	int i;

	if (ps == NULL || desc.usage >= ps->meta->group_size) {
		dev_dbg(dev, "ev:%llx out of range\n", event->attr.config);
		return false;
	}

	if (!__check_custom_event(pmu, event))
		return false;

	/* check clients for event validity */
	for (i = 0; i < ps->meta->nr_clients; i++) {
		if (desc.target == ps->meta->clients[i])
			return true;
	}

	dev_dbg(dev, "not valid client, config:%#llx, target:%#x, set:%d\n",
	     event->attr.config, desc.target, desc.set);

	return false;
}

static void
__set_hwc(struct rtk_pmc_set *ps, union rtk_pmc_desc pmc,
		    struct perf_event *event)
{
	struct rtk_pmu *pmu = ps->pmu;
	struct device *dev = &pmu->pdev->dev;
	struct hw_perf_event *hwc = &event->hw;
	int target = rtk_rbus_pmc_target(event->attr.config);

	rtk_event_set_pmc_desc(hwc, pmc.val);

	/* control register address */
	rtk_event_set_config_addr(hwc,
				  (unsigned long)ps->base +
				  ps->meta->configs[0]);

	/* mask of control register */
	rtk_event_set_target_mask(hwc, GENMASK(target, target));

	/* monitored target */
	rtk_event_set_config(hwc, GENMASK(target, target));

	rtk_event_set_pmc_addr(hwc,
			       (unsigned long)ps->base +
			       ps->meta->pmcgs[pmc.idx] + (pmc.usage << 2));

	rtk_event_set_pmc_mask(hwc, ps->meta->val_mask[pmc.usage]);
	rtk_event_set_pmc_threshold(hwc, ps->meta->ov_th[pmc.usage]);

	if (IS_ENABLED(CONFIG_RTK_PMU_DEV))
		rtk_event_set_pmc_ts(&event->hw, ktime_get_mono_fast_ns());

	dev_dbg(dev, "ev:%#llx, cconf:%#llx, cbase:%#lx, ebase:%#lx, emask:%#x",
	     event->attr.config, hwc->config,
	     hwc->config_base, hwc->event_base, hwc->event_base_rdpmc);
}

static void
__set_post_arb_rbus_address_range(struct rtk_pmc_set *ps, struct perf_event *event,
		    int target_rbus)
{
	unsigned long start_addr, end_addr;

	start_addr = rtk_event_config_addr(event) + RBUS_POST_ARB_START_ADDR_CONFIG_OFFSET;
	end_addr = rtk_event_config_addr(event) + RBUS_POST_ARB_END_ADDR_CONFIG_OFFSET;

	if (target_rbus == RBUS_CUSTOM_RANGE_ID) {
		rtk_writel(start_addr, event->attr.config1);
		rtk_writel(end_addr, event->attr.config2);
	} else {
		rtk_writel(start_addr, ps->meta->regions[target_rbus].start);
		rtk_writel(end_addr, ps->meta->regions[target_rbus].end);
	}
}

static void
__set_hwc_v2(struct rtk_pmc_set *ps, union rtk_pmc_desc pmc,
		    struct perf_event *event)
{
	struct rtk_pmu *pmu = ps->pmu;
	struct device *dev = &pmu->pdev->dev;
	struct hw_perf_event *hwc = &event->hw;
	unsigned int latch_bit_offset = __pmc_to_ctrl_offset(pmc);
	int target_rbus_mask = GENMASK(ARB_CPU_ID_OFFSET - 1, 0);
	int target_cpu_mask = GENMASK(ARB_SET_OFFSET - 1, ARB_CPU_ID_OFFSET);
	int target = rtk_rbus_pmc_target_v2(event->attr.config);
	int target_cpu_shift;
	int target_cpu = (target & target_cpu_mask) >> ARB_CPU_ID_OFFSET;
	int target_rbus = target & target_rbus_mask;
	u32 config = (pmc.set == PMC_SET__RBUS_PRE_ARB) ? target_rbus : 0;

	rtk_event_set_pmc_desc(hwc, pmc.val);

	rtk_event_set_config_addr(hwc,
				  (unsigned long)ps->base +
				  ps->meta->configs[pmc.idx]);

	if (config == RBUS_CUSTOM_RANGE_ID)
		config = event->attr.config1;

	target_cpu_shift = (pmc.set == PMC_SET__RBUS_PRE_ARB) ? RBUS_PRE_ARB_CPU_SEL_OFFSET : 0;
	config |= (target_cpu << target_cpu_shift);

	/* mask of config register */
	rtk_event_set_target_mask(hwc, U32_MAX);

	/* monitored target */
	rtk_event_set_config(hwc, config);

	rtk_event_set_pmc_addr(hwc,
			       (unsigned long)ps->base +
			       ps->meta->pmcgs[pmc.idx] + (pmc.usage * RBUS_ARB_USAGE_OFFSET));

	if (pmc.set == PMC_SET__RBUS_POST_ARB)
		__set_post_arb_rbus_address_range(ps, event, target_rbus);

	rtk_event_set_pmc_mask(hwc, ps->meta->val_mask[pmc.usage]);
	rtk_event_set_pmc_threshold(hwc, ps->meta->ov_th[pmc.usage]);

	if (rtk_pmu_need_update_ctrl(ps->pmu)) {
		rtk_event_set_pmc_update_ctrl(hwc, (unsigned long)ps->base + RBUS_ARB_UPDATE_CTRL);
		rtk_event_set_pmc_update(hwc, BIT(latch_bit_offset) | BIT(latch_bit_offset + pmc.idx));

		dev_dbg(dev, "ev:%#llx update ctrl:%lx, update:%x, latch_bit_offset: %d, pmc.idx: %d\n",
		     event->attr.config, rtk_event_pmc_update_ctrl(event),
		     rtk_event_pmc_update(event), latch_bit_offset, pmc.idx);
	}

	if (IS_ENABLED(CONFIG_RTK_PMU_DEV))
		rtk_event_set_pmc_ts(&event->hw, ktime_get_mono_fast_ns());

	dev_dbg(dev, "ev:%#llx, cconf:%#llx, cbase:%#lx, ebase:%#lx, emask:%#x, pmc.idx: %d",
	     event->attr.config, hwc->config,
	     hwc->config_base, hwc->event_base, hwc->event_base_rdpmc, pmc.idx);
}

static void
rtk_rbus_set_hwc_v2(struct rtk_pmc_set *ps, union rtk_pmc_desc pmc,
		    struct perf_event *event)
{
	__set_hwc(ps, pmc, event);
	if (pmc.usage == RBUS_PMC__V2_RTA_MAX)
		rtk_event_set_pmc_type(&event->hw, RTK_PMC_STATE);
}

static void
rtk_rbus_set_hwc_v3(struct rtk_pmc_set *ps, union rtk_pmc_desc pmc,
		    struct perf_event *event)
{
	__set_hwc(ps, pmc, event);
	if (pmc.usage == RBUS_PMC__V3_RTA_MAX)
		rtk_event_set_pmc_type(&event->hw, RTK_PMC_STATE);
}

static void
rtk_rbus_set_hwc_v4(struct rtk_pmc_set *ps, union rtk_pmc_desc pmc,
		    struct perf_event *event)
{
	__set_hwc_v2(ps, pmc, event);
}

static int
rtk_rbus_find_pmc(struct rtk_pmc_set *ps, int hwc, int target)
{
	int i;

	for (i = 0; i < ps->meta->nr_clients; i++) {
		if (ps->meta->clients[i] == target) {
			test_and_set_bit(i, &ps->used_mask);
			ps->tracking[i].target = target;
			ps->tracking[i].config = hwc;

			return i;
		}
	}

	return RTK_INV_PMC_TARGET;
}

static union rtk_pmc_desc
rtk_rbus_arrange_pmc(struct rtk_pmu *pmu, u64 config)
{
	union rtk_pmc_desc pmc;
	union rtk_rbus_event_desc desc = __get_event_desc(config);
	struct rtk_pmc_set *ps = pmu->pmcss[desc.set];
	struct device *dev = &pmu->pdev->dev;
	int idx = ps->arrange_pmc(ps,
				  rtk_rbus_pmc_config(config),
				  rtk_rbus_pmc_target(config));

	if (idx < 0) {
		pmc.val = -EAGAIN;
	} else {
		pmc.set = desc.set;
		pmc.idx = idx;
		pmc.usage = desc.usage;
	}

	dev_dbg(dev, "%s- ev:%#llx, desc %#x:%#x:%#x\n", ps->name,
	     config, desc.set, desc.target, desc.usage);
	dev_dbg(dev, "%s- event pmc set:%#x, idx:%#x, usage:%#x\n",
	     ps->name, pmc.set, pmc.idx, pmc.usage);

	return pmc;
}

static union rtk_pmc_desc
rtk_rbus_arrange_pmc_v2(struct rtk_pmu *pmu, u64 config)
{
	union rtk_pmc_desc pmc;
	union rtk_rbus_arb_event_desc desc = __get_event_desc_v2(config);
	struct rtk_pmc_set *ps = pmu->pmcss[desc.set];
	struct device *dev = &pmu->pdev->dev;
	int idx = ps->arrange_pmc(ps,
				  rtk_rbus_pmc_config_v2(config),
				  rtk_rbus_pmc_target_v2(config));

	if (idx < 0) {
		pmc.val = -EAGAIN;
	} else {
		pmc.set = desc.set;
		pmc.idx = idx;
		pmc.usage = desc.usage;
	}

	dev_dbg(dev, "%s- ev:%#llx, desc %#x:%#x:%#x\n", ps->name,
	     config, desc.set, desc.target, desc.usage);
	dev_dbg(dev, "%s- event pmc set:%#x, idx:%#x, usage:%#x\n",
	     ps->name, pmc.set, pmc.idx, pmc.usage);

	return pmc;
}

static u32
rtk_rbus_read_counter_with_update(struct rtk_pmc_set *ps,
					struct perf_event *event)
{
	u32 val = rtk_readl(rtk_event_pmc_update_ctrl(event));
	u32 update_val = rtk_event_pmc_update(event);

	val ^= update_val;
	rtk_writel(rtk_event_pmc_update_ctrl(event), val);

	return rtk_readl(rtk_event_pmc_addr(event));
}

int
rbus_pmu_ps_init_v2(const struct rtk_pmc_set_meta *meta, struct rtk_pmc_set *ps)
{
	struct rtk_pmu *pmu = ps->pmu;
	struct device *dev = &pmu->pdev->dev;
	int ret = -EINVAL;

	ps->pmc_config = rtk_rbus_pmc_config;
	ps->release_pmc = rtk_ps_release_pmc;
	ps->arrange_pmc = rtk_rbus_find_pmc;
	ps->start_pmc = rtk_ps_start_pmc;
	ps->stop_pmc = rtk_ps_stop_pmc;
	ps->read_pmc = rtk_ps_read_pmc;
	ps->enable = rtk_rbus_ps_enable;
	ps->disable = rtk_rbus_ps_disable;
	ps->read_counter = rtk_read_counter;

	switch (ps->meta->hw_ver) {
	case RTK_RBUS_EV_ORDER_V2:
		ps->set_perf_hwc = rtk_rbus_set_hwc_v2;
		ps->find_event_by_ov = __find_event_by_ov_v2;
		ps->pmc_to_ctrl_bit = __pmc_to_ctrl_bit_v2;
		ret = 0;
		break;
	case RTK_RBUS_EV_ORDER_V3:
		ps->set_perf_hwc = rtk_rbus_set_hwc_v3;
		ps->find_event_by_ov = __find_event_by_ov_v3;
		ps->pmc_to_ctrl_bit = __pmc_to_ctrl_bit_v3;
		ret = 0;
		break;
	case RTK_RBUS_EV_ORDER_V4:
		ps->pmc_config = rtk_rbus_pmc_config_v2;
		ps->arrange_pmc = rtk_ps_find_pmc;
		ps->set_perf_hwc = rtk_rbus_set_hwc_v4;
		ps->pmc_to_ctrl_bit = __pmc_to_ctrl_bit_v4;
		ps->enable = rtk_rbus_ps_enable_v2;
		ps->read_counter = rtk_rbus_read_counter_with_update;
		ps->find_event_by_ov = __find_event_by_ov_v4;
		ps->disable = rtk_rbus_ps_disable_v2;
		ret = 0;
		break;
	default:
		dev_err(dev, "Invalide Rbus version(%u)\n", ps->meta->hw_ver);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(rbus_pmu_ps_init_v2);

int
rbus_pmu_ps_drv_init(const struct rtk_pmc_set_meta *meta,
		     struct rtk_pmc_set *ps)
{
	ps->pmc_config = rtk_rbus_pmc_config;
	ps->release_pmc = rtk_ps_release_pmc;
	ps->arrange_pmc = rtk_find_drv_pmc;
	ps->set_perf_hwc = rtk_set_drv_hwc;
	ps->start_pmc = rtk_start_drv_pmc;
	ps->stop_pmc = rtk_stop_drv_pmc;
	ps->read_pmc = rtk_read_drv_pmc;

	return 0;
}
EXPORT_SYMBOL(rbus_pmu_ps_drv_init);

int
rtk_rbus_pmu_init(struct rtk_pmu *pmu, struct device_node *dt,
		  const char *name,
		  const struct attribute_group **attr_groups,
		  const struct rtk_pmc_set_meta *meta, int nr_ps)
{
	struct device *dev = &pmu->pdev->dev;
	int ret;

	ret = rtk_uncore_pmu_init(pmu, dt, name, RTK_RBUS_PMU_CPUHP_NAME,
				  0, attr_groups,
				  meta, nr_ps, RBUS_EV_NUM);
	if (ret)
		return ret;

	pmu->get_pmc_set = rtk_rbus_get_pmc_set;
	pmu->is_valid_event = rtk_rbus_check_event;
	pmu->arrange_pmc = rtk_rbus_arrange_pmc;

	if (rtk_pmu_has_no_overflow(pmu)) {
		pmu->refresh = rtk_rbus_refresh;
	} else {
		ret = request_irq(pmu->irq, overflow_handler, IRQF_SHARED, name,
				  pmu);
		if (ret < 0)
			dev_err(dev, "request IRQ:%d failed(%d)\n", pmu->irq, ret);
	}

	return ret;
}
EXPORT_SYMBOL(rtk_rbus_pmu_init);

int
rtk_rbus_pmu_init_v2(struct rtk_pmu *pmu, struct device_node *dt,
		  const char *name,
		  const struct attribute_group **attr_groups,
		  const struct rtk_pmc_set_meta *meta, int nr_ps)
{
	struct device *dev = &pmu->pdev->dev;
	int ret;

	ret = rtk_uncore_pmu_init(pmu, dt, name, RTK_RBUS_PMU_CPUHP_NAME,
				  0, attr_groups,
				  meta, nr_ps, RBUS_EV_NUM);
	if (ret)
		return ret;

	pmu->get_pmc_set = rtk_rbus_get_pmc_set_v2;
	pmu->is_valid_event = rtk_rbus_check_event_v2;
	pmu->arrange_pmc = rtk_rbus_arrange_pmc_v2;

	if (rtk_pmu_has_no_overflow(pmu)) {
		dev_err(dev, "init failed: missing overflow configuration in DTS\n");
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Rbus need to raise update signal before reading counter
	 */
	pmu->flags |= RTK_PMU_NEED_UPDATE_CTRL;

	ret = request_irq(pmu->irq, overflow_handler_v2, IRQF_SHARED, name, pmu);
	if (ret < 0) {
		dev_err(dev, "request IRQ:%d failed(%d)\n", pmu->irq, ret);
		goto out;
	}

out:
	return ret;
}
EXPORT_SYMBOL(rtk_rbus_pmu_init_v2);

static const struct of_device_id rtk_pmu_of_device_ids[] = {
	{
		.compatible = "realtek,rtk-16xxb-rbus-pmu",
		.data = rtk_16xxb_rbus_init
	},
	{
		.compatible = "realtek,rtk-13xxd-rbus-pmu",
		.data = rtk_13xxd_rbus_init
	},
	{
		.compatible = "realtek,rtk-1625-rbus-pmu",
		.data = rtk_1625_rbus_init
	},
	{},
};

static int
rtk_rbus_pmu_probe(struct platform_device *pdev)
{
	return rtk_pmu_device_probe(pdev, rtk_pmu_of_device_ids);
}

static void rtk_rbus_pmu_remove(struct platform_device *pdev)
{
	rtk_pmu_device_remove(pdev);
}

static struct platform_driver rtk_rbus_pmu_driver = {
	.driver         = {
		.name   = RTK_RBUS_PMU_PDEV_NAME,
		.of_match_table = rtk_pmu_of_device_ids,
	},
	.probe          = rtk_rbus_pmu_probe,
	.remove		= rtk_rbus_pmu_remove
};

module_platform_driver(rtk_rbus_pmu_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTK RBUS PMU support");

