// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include "common.h"
#include "clk-pll.h"

static int wait_freq_ready(struct clk_pll2 *clkp)
{
	u32 pollval;

	if (!clkp->freq_ready_valid)
		return 0;

	return regmap_read_poll_timeout(clkp->clkr.regmap, clkp->freq_ready_reg, pollval,
	       (pollval & clkp->freq_ready_mask) == clkp->freq_ready_val, 0, 2000);
}

static int is_power_on(struct clk_pll2 *clkp)
{
	u32 val;

	if (!clkp->power_reg)
		return 1;

	regmap_read(clkp->clkr.regmap, clkp->power_reg, &val);
	return (val & clkp->power_mask) == clkp->power_val_on;
}

static u32 get_freq_val_raw(struct clk_pll2 *clkp)
{
	u32 val;

	regmap_read(clkp->clkr.regmap, clkp->freq_reg, &val);
	return val;
}

static u32 get_freq_val(struct clk_pll2 *clkp)
{
	return get_freq_val_raw(clkp) & clkp->freq_mask;
}

static void update_reqseq_reg_val(struct reg_sequence *seq, u32 num_seq, u32 reg, u32 val)
{
	u32 i;

	for (i = 0; i < num_seq; i++) {
		if (seq[i].reg == reg)
			seq[i].def = val;
	}
}

static int clk_pll2_enable(struct clk_hw *hw)
{
	struct clk_pll2 *clkp = to_clk_pll2(hw);
	u32 freq_val = get_freq_val_raw(clkp);

	if (!clkp->seq_power_on)
		return 0;

	if (is_power_on(clkp))
		return 0;

	update_reqseq_reg_val(clkp->seq_power_on, clkp->num_seq_power_on, clkp->freq_reg, freq_val);
	regmap_multi_reg_write(clkp->clkr.regmap, clkp->seq_power_on, clkp->num_seq_power_on);
	return wait_freq_ready(clkp);
}

static void clk_pll2_disable(struct clk_hw *hw)
{
	struct clk_pll2 *clkp = to_clk_pll2(hw);

	if (!clkp->seq_power_off)
		return;

	regmap_multi_reg_write(clkp->clkr.regmap, clkp->seq_power_off, clkp->num_seq_power_off);
}

static void clk_pll2_disable_unused(struct clk_hw *hw)
{
	pr_info("%pC: %s\n", hw->clk, __func__);
	clk_pll2_disable(hw);
}

static int clk_pll2_is_enabled(struct clk_hw *hw)
{
	struct clk_pll2 *clkp = to_clk_pll2(hw);

	return is_power_on(clkp);
}

static long clk_pll2_round_rate(struct clk_hw *hw, unsigned long rate, unsigned long *parent_rate)
{
	struct clk_pll2 *clkp = to_clk_pll2(hw);
	const struct freq_table *ftblv = NULL;

	ftblv = ftbl_find_by_rate(clkp->freq_tbl, rate);
	return ftblv ? ftblv->rate : 0;
}

static unsigned long clk_pll2_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_pll2 *clkp = to_clk_pll2(hw);
	const struct freq_table *fv;
	u32 freq_val = get_freq_val(clkp);

	fv = ftbl_find_by_val_with_mask(clkp->freq_tbl, clkp->freq_mask, freq_val);
	return fv ? fv->rate : 0;
}

static int clk_pll2_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct clk_pll2 *clkp = to_clk_pll2(hw);
	const struct freq_table *fv;
	int ret = 0;
	u32 freq_val = get_freq_val_raw(clkp);

	fv = ftbl_find_by_rate(clkp->freq_tbl, rate);
	if (!fv || fv->rate != rate)
		return -EINVAL;

	freq_val = (freq_val & ~clkp->freq_mask) | (fv->val & clkp->freq_mask);
	pr_debug("%pC: %s: target_rate=%ld, tbl=(%ld, 0x%08x), reg_val=%08x\n", hw->clk, __func__,
		 rate, fv->rate, fv->val, freq_val);

	update_reqseq_reg_val(clkp->seq_set_freq, clkp->num_seq_set_freq, clkp->freq_reg, freq_val);
	regmap_multi_reg_write(clkp->clkr.regmap, clkp->seq_set_freq, clkp->num_seq_set_freq);

	if (is_power_on(clkp))
		ret =  wait_freq_ready(clkp);
	if (ret)
		pr_warn("%pC %s: failed to set freq: %d\n", hw->clk, __func__, ret);
	return 0;
}

const struct clk_ops clk_pll2_ops = {
	.round_rate       = clk_pll2_round_rate,
	.recalc_rate      = clk_pll2_recalc_rate,
	.set_rate         = clk_pll2_set_rate,
	.enable           = clk_pll2_enable,
	.disable          = clk_pll2_disable,
	.disable_unused   = clk_pll2_disable_unused,
	.is_enabled       = clk_pll2_is_enabled,
};
EXPORT_SYMBOL_GPL(clk_pll2_ops);
