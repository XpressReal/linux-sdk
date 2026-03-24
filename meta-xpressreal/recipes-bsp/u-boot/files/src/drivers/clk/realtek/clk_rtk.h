/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __CLK_RTK_H
#define __CLK_RTK_H

#include <clk.h>
#include <linux/bitops.h>

struct clk_rtk_ctrl_priv {
	void __iomem *reg_base;
	struct rtk_gate **gates;
	uint32_t gate_count;
};

struct rtk_gate {
	char* name;
	char* parent;
	uint32_t flags;
	uint32_t ofs;
	uint32_t bit_idx;
	bool write_en;
	int enable_count;
};

extern const struct clk_ops clk_rtk_ctrl_ops;

#define CLK_REGMAP_GATE(_name, _parent, _flags, _ofs, _bit_idx, _write_en) \
struct rtk_gate _name = {			\
		.name = #_name,				\
		.parent = _parent,			\
		.flags = _flags,			\
		.ofs = _ofs,				\
		.bit_idx = _bit_idx,		\
		.write_en = _write_en,		\
		.enable_count = 0,			\
	}
#define CLK_REGMAP_GATE_NO_PARENT(_name, _flags, _ofs, _bit_idx, _write_en) \
	CLK_REGMAP_GATE(_name, NULL, _flags, _ofs, _bit_idx, _write_en)

/* from linux clk-provider */
#define CLK_SET_RATE_GATE	BIT(0) /* must be gated across rate change */
#define CLK_SET_PARENT_GATE	BIT(1) /* must be gated across re-parent */
#define CLK_SET_RATE_PARENT	BIT(2) /* propagate rate change up one level */
#define CLK_IGNORE_UNUSED	BIT(3) /* do not gate even if unused */
							/* unused */
							/* unused */
#define CLK_GET_RATE_NOCACHE	BIT(6) /* do not use the cached clk rate */
#define CLK_SET_RATE_NO_REPARENT BIT(7) /* don't re-parent on rate change */
#define CLK_GET_ACCURACY_NOCACHE BIT(8) /* do not use the cached clk accuracy */
#define CLK_RECALC_NEW_RATES	BIT(9) /* recalc rates after notifications */
#define CLK_SET_RATE_UNGATE	BIT(10) /* clock needs to run to set rate */
#define CLK_IS_CRITICAL		BIT(11) /* do not gate, ever */
/* parents need enable during gate/ungate, set rate and re-parent */
#define CLK_OPS_PARENT_ENABLE	BIT(12)
/* duty cycle call may be forwarded to the parent clock */
#define CLK_DUTY_CYCLE_PARENT	BIT(13)
#define CLK_DONT_HOLD_STATE	BIT(14) /* Don't hold state */

#endif /* __CLK_RTK_H */