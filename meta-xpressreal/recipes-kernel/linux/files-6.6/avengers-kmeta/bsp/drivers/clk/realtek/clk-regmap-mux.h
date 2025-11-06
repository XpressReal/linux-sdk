/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#ifndef __CLK_REALTEK_CLK_REGMAP_MUX_H
#define __CLK_REALTEK_CLK_REGMAP_MUX_H

#include "clk-regmap.h"

struct clk_regmap_mux {
	struct clk_regmap clkr;
	int mux_ofs;
	unsigned int mask;
	unsigned int shift;
};

#define to_clk_regmap_mux(_hw) \
	container_of(to_clk_regmap(_hw), struct clk_regmap_mux, clkr)
#define __clk_regmap_mux_hw(_p) __clk_regmap_hw(&(_p)->clkr)

extern const struct clk_ops clk_regmap_mux_ops;

#define __CLK_REGMAP_MUX(_name, _parents, _ops, _flags, _ofs, _sft, _mask) \
struct clk_regmap_mux _name = { \
	.clkr.hw.init = CLK_HW_INIT_PARENTS(#_name, _parents, _ops, _flags), \
	.mux_ofs = _ofs, \
	.shift = _sft, \
	.mask = _mask, \
}

#define CLK_REGMAP_MUX(_name, _parents, _flags, _ofs, _sft, _mask) \
	__CLK_REGMAP_MUX(_name, _parents, &clk_regmap_mux_ops, _flags, _ofs, _sft, _mask)

#endif
