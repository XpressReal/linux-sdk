/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __RTK_LSADC_KNOB_H
#define __RTK_LSADC_KNOB_H

enum {
	RTK_LSADC_KNOB_VOLTAGE_ZERO = 0,
	RTK_LSADC_KNOB_VOLTAGE_LOW,
	RTK_LSADC_KNOB_VOLTAGE_HIGH
};

struct rtk_lsadc_knob_vstate {
	u32 state;
	u32 duration;
};

enum {
	RTK_LSADC_KNOB_ROTATE_NONE,
	RTK_LSADC_KNOB_ROTATE_CLOCKWISE,
	RTK_LSADC_KNOB_ROTATE_ANTICLOCKWISE
};

#endif /* __RTK_LSADC_KNOB_H */
