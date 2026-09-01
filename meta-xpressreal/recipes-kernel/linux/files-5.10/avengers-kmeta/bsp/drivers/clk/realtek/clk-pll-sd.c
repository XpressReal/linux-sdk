// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/clk-provider.h>
#include <linux/bitfield.h>
#include <linux/module.h>
#include "common.h"
#include "clk-pll.h"

#define FREQ(_r, _v)   { .rate = (_r), .val = (_v), }

#define PLL_SD1	0x0
#define PLL_SD2	0x4
#define PLL_SD3	0x8
#define PLL_SD4	0xc

static inline u32 __get_phrt0(struct clk_pll_mmc *clkm)
{
	return (clk_regmap_read(&clkm->clkr + PLL_SD1, clkm->pll_ofs) >> 1) & 0x1;
}

static inline void __set_phrt0(struct clk_pll_mmc *clkm, u32 val)
{
	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD1, 0x00000002, val << 1);
}

static inline u32 __get_phsel(struct clk_pll_mmc *clkm, int id)
{
	u32 sft = id ? 8 : 3;

	return (clk_regmap_read(&clkm->clkr, clkm->pll_ofs + PLL_SD1) >> sft) & 0x1f;
}

static inline void __set_phsel(struct clk_pll_mmc *clkm, int id, u32 val)
{
	u32 sft = id ? 8 : 3;

	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD1, 0x1f << sft, val << sft);
}

static int clk_pll_sd_is_enabled(struct clk_hw *hw)
{
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hw);

	return clk_regmap_read(&clkm->clkr, clkm->pll_ofs + PLL_SD4) & 1;
}

static int clk_pll_sd_enable(struct clk_hw *hw)
{
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hw);

	__set_phrt0(clkm, 1);
	udelay(10);

	return 0;
}

static void clk_pll_sd_disable(struct clk_hw *hw)
{
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hw);

	__set_phrt0(clkm, 0);

}

static void clk_pll_sd_disable_unused(struct clk_hw *hw)
{
	pr_info("%pC: %s\n", hw->clk, __func__);
	clk_pll_sd_disable(hw);
}

/* register definition of PLL_SD2 */
#define PLL_SD_REG_TUNE11			GENMASK(2, 1)
#define PLL_SD_REG_TUNE11_1V9			0x2
#define PLL_SD_SSCPLL_CS1			GENMASK(4, 3)
#define PLL_SD_SSCPLL_CS1_INIT_VALUE		0x1
#define PLL_SD_SSCPLL_ICP			GENMASK(9, 5)
#define PLL_SD_SSCPLL_ICP_CURRENT		0x01
#define PLL_SD_SSCPLL_RS			GENMASK(12, 10)
#define PLL_SD_SSCPLL_RS_13K			0x5
#define PLL_SD_SSC_DEPTH			GENMASK(15, 13)
#define PLL_SD_SSC_DEPTH_1_N			0x3
#define PLL_SD_SSC_8X_EN			BIT(16)
#define PLL_SD_SSC_DIV_EXT_F			GENMASK(25, 18)
#define PLL_SD_SSC_DIV_EXT_F_50M		0x71
#define PLL_SD_SSC_DIV_EXT_F_100M		0xe3
#define PLL_SD_SSC_DIV_EXT_F_200M		0x0
#define PLL_SD_SSC_DIV_EXT_F_208M		0xe3
#define PLL_SD_EN_CPNEW				BIT(26)

/* register definition of PLL_SD3 */
#define PLL_SD_SSC_TBASE			GENMASK(7, 0)
#define PLL_SD_SSC_TBASE_INIT_VALUE		0x88
#define PLL_SD_SSC_STEP_IN			GENMASK(14, 8)
#define PLL_SD_SSC_STEP_IN_INIT_VALUE		0x43
#define PLL_SD_SSC_DIV_N			GENMASK(25, 16)

static const struct freq_table clk_pll_sd_freq_tbl[] = {
	FREQ(50000000, 0x2a),
	FREQ(100000000, 0x56),
	FREQ(200000000, 0xae),
	FREQ(208000000, 0xb6),
	FREQ_TABLE_END
};

static int clk_pll_sd_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hw);
	const struct freq_table *fv = ftbl_find_by_rate(clk_pll_sd_freq_tbl, rate);
	u32 mask;
	u32 val;

	if (!fv)
		return -EINVAL;

	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD4, 0x7, 0x6);

	mask = PLL_SD_REG_TUNE11 | PLL_SD_SSCPLL_CS1 | PLL_SD_SSCPLL_ICP | PLL_SD_SSCPLL_RS |
	       PLL_SD_SSC_DEPTH | PLL_SD_SSC_8X_EN | PLL_SD_SSC_DIV_EXT_F | PLL_SD_EN_CPNEW;

	val = FIELD_PREP(PLL_SD_REG_TUNE11, PLL_SD_REG_TUNE11_1V9) |
	      FIELD_PREP(PLL_SD_SSCPLL_CS1, PLL_SD_SSCPLL_CS1_INIT_VALUE) |
	      FIELD_PREP(PLL_SD_SSCPLL_ICP, PLL_SD_SSCPLL_ICP_CURRENT) |
	      FIELD_PREP(PLL_SD_SSCPLL_RS, PLL_SD_SSCPLL_RS_13K) |
	      FIELD_PREP(PLL_SD_SSC_DEPTH, 0) |
	      PLL_SD_SSC_8X_EN |
	      PLL_SD_EN_CPNEW;

	switch (rate) {
	case 50000000:
		val |= FIELD_PREP(PLL_SD_SSC_DIV_EXT_F, PLL_SD_SSC_DIV_EXT_F_50M);
		break;
	case 100000000:
		val |= FIELD_PREP(PLL_SD_SSC_DIV_EXT_F, PLL_SD_SSC_DIV_EXT_F_100M);
		break;
	case 200000000:
		val |= FIELD_PREP(PLL_SD_SSC_DIV_EXT_F, PLL_SD_SSC_DIV_EXT_F_200M);
		break;
	case 208000000:
		val |= FIELD_PREP(PLL_SD_SSC_DIV_EXT_F, PLL_SD_SSC_DIV_EXT_F_208M);
		break;
	}

	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD2, mask, val);

	/* set freq with initial value */
	val = FIELD_PREP(PLL_SD_SSC_TBASE, PLL_SD_SSC_TBASE_INIT_VALUE) |
	      FIELD_PREP(PLL_SD_SSC_STEP_IN, PLL_SD_SSC_STEP_IN_INIT_VALUE) |
	      FIELD_PREP(PLL_SD_SSC_DIV_N, fv->val);
	clk_regmap_write(&clkm->clkr, clkm->pll_ofs + PLL_SD3, val);
	mdelay(2);

	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD4, 0x7, 0x7);
	udelay(200);

	return 0;
}

static unsigned long clk_pll_sd_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hw);
	const struct freq_table *fv;
	unsigned long rate;
	u32 val;

	val = clk_regmap_read(&clkm->clkr, clkm->pll_ofs + PLL_SD3);
	val = FIELD_GET(PLL_SD_SSC_DIV_N, val);

	switch (val) {
	case 0x2a:
		rate = 50000000;
		break;
	case 0x56:
		rate = 100000000;
		break;
	case 0xae:
		rate = 200000000;
		break;
	case 0xb6:
		rate = 208000000;
		break;
	}

	fv = ftbl_find_by_rate(clk_pll_sd_freq_tbl, rate);

	return fv ? fv->rate : 0;
}

static long clk_pll_sd_round_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long *parent_rate)
{
	const struct freq_table *fv = ftbl_find_by_rate(clk_pll_sd_freq_tbl, rate);

	return fv ? fv->rate : -EINVAL;
}

const struct clk_ops clk_pll_sd_ops = {
	.set_rate	= clk_pll_sd_set_rate,
	.recalc_rate	= clk_pll_sd_recalc_rate,
	.round_rate	= clk_pll_sd_round_rate,
	.is_enabled	= clk_pll_sd_is_enabled,
	.enable		= clk_pll_sd_enable,
	.disable	= clk_pll_sd_disable,
	.disable_unused	= clk_pll_sd_disable_unused,
};
EXPORT_SYMBOL_GPL(clk_pll_sd_ops);

/* register definition v2 of PLL_SD2 */
#define PLL_SD_V2_PI_IBSELH			GENMASK(2, 1)
#define PLL_SD_V2_PI_IBSELH_50_80M		0x0
#define PLL_SD_V2_PI_IBSELH_80_150M		0x1
#define PLL_SD_V2_PI_IBSELH_150_255M		0x2
#define PLL_SD_V2_SSC_PLL_ICP			GENMASK(9, 5)
#define PLL_SD_V2_SSC_PLL_ICP_50M		0x00
#define PLL_SD_V2_SSC_PLL_ICP_INIT_VALUE	0x01
#define PLL_SD_V2_SSC_PLL_RS			GENMASK(12, 10)
#define PLL_SD_V2_SSC_PLL_RS_4K			0x0
#define PLL_SD_V2_SSC_PLL_RS_6K			0x2
#define PLL_SD_V2_SSC_PLL_RS_8K			0x3
#define PLL_SD_V2_SSC_FLAG_INIT			BIT(13)
#define PLL_SD_V2_SSC_OC_EN			BIT(14)
#define PLL_SD_V2_SSC_DIV_EXT_F			GENMASK(28, 16)
#define PLL_SD_V2_SSC_DIV_EXT_F_50M		0x0d09
#define PLL_SD_V2_SSC_DIV_EXT_F_100M		0x1a12
#define PLL_SD_V2_SSC_DIV_EXT_F_200M		0x0aaa
#define PLL_SD_V2_SSC_DIV_EXT_F_208M		0x1a12
/* register definition v2 of PLL_SD3 */
#define PLL_SD_V2_SSC_DIV_N			GENMASK(7, 0)
/* register definition v2 of PLL_SD4 */
#define PLL_SD_V2_SSC_RSTB			BIT(0)
#define PLL_SD_V2_SSC_PLL_RSTB			BIT(1)
#define PLL_SD_V2_SSC_PLL_POW			BIT(2)
#define PLL_SD_V2_SSC_PLL_EN			BIT(8)

static const struct freq_table clk_pll_v2_sd_freq_tbl[] = {
	FREQ(50000000, 0x5),
	FREQ(100000000, 0xc),
	FREQ(200000000, 0x1b),
	FREQ(208000000, 0x1c),
	FREQ_TABLE_END
};

static int clk_pll_sd_v2_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hw);
	const struct freq_table *fv = ftbl_find_by_rate(clk_pll_v2_sd_freq_tbl, rate);
	u32 mask, val;

	if (!fv)
		return -EINVAL;

	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD2, PLL_SD_V2_SSC_FLAG_INIT |
			       PLL_SD_V2_SSC_OC_EN , 0);

	__set_phrt0(clkm, 0);

	/* set freq value */
	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD3, PLL_SD_V2_SSC_DIV_N, fv->val);

	mask = PLL_SD_V2_PI_IBSELH | PLL_SD_V2_SSC_PLL_ICP | PLL_SD_V2_SSC_PLL_RS | PLL_SD_V2_SSC_DIV_EXT_F;

	switch (rate) {
	case 50000000:
		val = FIELD_PREP(PLL_SD_V2_PI_IBSELH, PLL_SD_V2_PI_IBSELH_50_80M) |
		      FIELD_PREP(PLL_SD_V2_SSC_PLL_ICP, PLL_SD_V2_SSC_PLL_ICP_50M) |
		      FIELD_PREP(PLL_SD_V2_SSC_PLL_RS, PLL_SD_V2_SSC_PLL_RS_6K) |
		      FIELD_PREP(PLL_SD_V2_SSC_DIV_EXT_F, PLL_SD_V2_SSC_DIV_EXT_F_50M);
		break;
	case 100000000:
		val = FIELD_PREP(PLL_SD_V2_PI_IBSELH, PLL_SD_V2_PI_IBSELH_80_150M) |
		      FIELD_PREP(PLL_SD_V2_SSC_PLL_ICP, PLL_SD_V2_SSC_PLL_ICP_INIT_VALUE) |
		      FIELD_PREP(PLL_SD_V2_SSC_PLL_RS, PLL_SD_V2_SSC_PLL_RS_6K) |
		      FIELD_PREP(PLL_SD_V2_SSC_DIV_EXT_F, PLL_SD_V2_SSC_DIV_EXT_F_100M);
		break;
	case 200000000:
		val = FIELD_PREP(PLL_SD_V2_PI_IBSELH, PLL_SD_V2_PI_IBSELH_150_255M) |
		      FIELD_PREP(PLL_SD_V2_SSC_PLL_ICP, PLL_SD_V2_SSC_PLL_ICP_INIT_VALUE) |
		      FIELD_PREP(PLL_SD_V2_SSC_PLL_RS, PLL_SD_V2_SSC_PLL_RS_8K) |
		      FIELD_PREP(PLL_SD_V2_SSC_DIV_EXT_F, PLL_SD_V2_SSC_DIV_EXT_F_200M);
		break;
	case 208000000:
		val = FIELD_PREP(PLL_SD_V2_PI_IBSELH, PLL_SD_V2_PI_IBSELH_150_255M) |
		      FIELD_PREP(PLL_SD_V2_SSC_PLL_ICP, PLL_SD_V2_SSC_PLL_ICP_INIT_VALUE) |
		      FIELD_PREP(PLL_SD_V2_SSC_PLL_RS, PLL_SD_V2_SSC_PLL_RS_8K) |
		      FIELD_PREP(PLL_SD_V2_SSC_DIV_EXT_F, PLL_SD_V2_SSC_DIV_EXT_F_208M);
		break;
	}

	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD2, mask, val);
	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD4, PLL_SD_V2_SSC_RSTB,
			       PLL_SD_V2_SSC_RSTB);
	udelay(100);

	mask = PLL_SD_V2_SSC_PLL_RSTB | PLL_SD_V2_SSC_PLL_POW | PLL_SD_V2_SSC_PLL_EN;

	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD4, mask, mask);
	udelay(100);

	clk_regmap_update_bits(&clkm->clkr, clkm->pll_ofs + PLL_SD2, PLL_SD_V2_SSC_OC_EN, PLL_SD_V2_SSC_OC_EN);
	udelay(100);

	__set_phrt0(clkm, 1);

	return 0;
}

static unsigned long clk_pll_sd_v2_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hw);
	const struct freq_table *fv;
	unsigned long rate;
	u32 val;

	val = clk_regmap_read(&clkm->clkr, clkm->pll_ofs + PLL_SD3);
	val = FIELD_GET(PLL_SD_V2_SSC_DIV_N, val);

	switch (val) {
	case 0x5:
		rate = 50000000;
		break;
	case 0xc:
		rate = 100000000;
		break;
	case 0x1b:
		rate = 200000000;
		break;
	case 0x1c:
		rate = 208000000;
		break;
	}

	fv = ftbl_find_by_rate(clk_pll_v2_sd_freq_tbl, rate);

	return fv ? fv->rate : 0;
}

static long clk_pll_sd_v2_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	const struct freq_table *fv = ftbl_find_by_rate(clk_pll_v2_sd_freq_tbl, rate);

	return fv ? fv->rate : -EINVAL;
}

const struct clk_ops clk_pll_sd_v2_ops = {
	.set_rate       = clk_pll_sd_v2_set_rate,
	.recalc_rate    = clk_pll_sd_v2_recalc_rate,
	.round_rate     = clk_pll_sd_v2_round_rate,
	.is_enabled     = clk_pll_sd_is_enabled,
	.enable         = clk_pll_sd_enable,
	.disable        = clk_pll_sd_disable,
	.disable_unused = clk_pll_sd_disable_unused,
};
EXPORT_SYMBOL_GPL(clk_pll_sd_v2_ops);

static int clk_pll_sd_phase_set_phase(struct clk_hw *hw, int degrees)
{
	struct clk_hw *hwp = clk_hw_get_parent(hw);
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hwp);
	int phase_id = (hw - &clkm->phase0_hw) ? 1 : 0;
	u32 val;

	val = DIV_ROUND_CLOSEST(degrees * 100, 1125);
	pr_debug("%pC: %s: id=%d, degrees=%d(%x)\n", hw->clk, __func__, phase_id, degrees, val);

	__set_phsel(clkm, phase_id, val);
	udelay(100);

	return 0;
}

static int clk_pll_sd_phase_get_phase(struct clk_hw *hw)
{
	struct clk_hw *hwp = clk_hw_get_parent(hw);
	struct clk_pll_mmc *clkm = to_clk_pll_mmc(hwp);
	int phase_id = (hw - &clkm->phase0_hw) ? 1 : 0;
	u32 val;

	val = __get_phsel(clkm, phase_id);
	val = DIV_ROUND_CLOSEST(val * 360, 32);

	pr_debug("%pC: %s: id=%d, degrees=%d\n", hw->clk, __func__, phase_id, val);

	return val;
}

const struct clk_ops clk_pll_sd_phase_ops = {
	.set_phase = clk_pll_sd_phase_set_phase,
	.get_phase = clk_pll_sd_phase_get_phase,
};
EXPORT_SYMBOL_GPL(clk_pll_sd_phase_ops);
