/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (C) 2021 Realtek Semiconductor Corporation
 *  Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#ifndef __CLK_REALTEK_CLK_REGMAP_CLKDET_H
#define __CLK_REALTEK_CLK_REGMAP_CLKDET_H

#include <linux/clk-provider.h>
#include <soc/realtek/rtk_clk_det.h> /* for CLK_DET_TYPE_* */
#include "clk-regmap.h"

struct clk_regmap_clkdet {
	struct clk_regmap clkr;
	int ofs;
	int type;
	const char *const *output_sel;
	int n_output_sel;
	u32 reg_output_sel;
	u32 mask_output_sel;
};

#define to_clk_regmap_clkdet(_hw) container_of(_hw, struct clk_regmap_clkdet, clkr.hw)
#define __clk_regmap_clkdet_hw(_ptr)  ((_ptr)->clkr.hw)

extern const struct clk_ops clk_regmap_clkdet_ops;

#endif
