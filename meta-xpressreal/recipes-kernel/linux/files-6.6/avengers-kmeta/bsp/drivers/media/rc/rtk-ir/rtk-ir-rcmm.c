// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * RTK IR Decoder setup for RCMM protocol.
 *
 * Copyright (C) 2020 Simon Hsu <simon_hsu@realtek.com>
 */

#include "rtk-ir.h"
#include <linux/bitrev.h>

static int rtk_ir_rcmm_scancode(struct rtk_ir_scancode_req *request, unsigned int raw)
{
	unsigned int addr, data;

	addr     = (raw >> 20) & 0xff;
	data     = (raw >> 0) & 0xff;

	request->scancode = addr << 8 | data;
	request->protocol = RC_PROTO_RCMM12;

	return 1;
}

static int rtk_ir_rcmm_wakeinfo(struct rtk_ir_wakeinfo *info, unsigned int scancode)
{
	info->addr = (scancode >> 8) & 0xff;
	info->addr_msk = 0xff << 20;
	info->scancode = scancode & 0xff;
	info->scancode_msk = 0xff;

	return 0;
}

struct rtk_ir_hw_decoder rtk_ir_rcmm = {
	.type = RC_PROTO_BIT_RCMM12,
	.tolerance = 70,
	.unit = 150,
	.sr = 5,
	.timings = {
		.ldr = {	/* leader symbol */
			.pulse = 0,
			.space = 2,
		},
		.ft_min = 1100,
	},
	.hw_set = RCMM_EN | IRBME | IRCM | IRRES | IRUE | IRIE | 0x1f,
	.scancode = rtk_ir_rcmm_scancode,
	.wakeinfo = rtk_ir_rcmm_wakeinfo,
};

