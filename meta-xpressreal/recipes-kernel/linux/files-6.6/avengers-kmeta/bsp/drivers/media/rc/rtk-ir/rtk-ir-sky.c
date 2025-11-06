// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * RTK IR Decoder setup for XMP protocol.
 *
 * Copyright (C) 2020 Simon Hsu <simon_hsu@realtek.com>
 */

#include "rtk-ir.h"
#include <linux/bitrev.h>

static int rtk_ir_sky_scancode(struct rtk_ir_scancode_req *request, unsigned int raw)
{
	static unsigned int lastRecvMs = 0;
	unsigned int time;

	request->scancode = raw & 0xffffff;
	request->protocol = RC_PROTO_OTHER;

	time = jiffies_to_msecs(jiffies) - lastRecvMs;
	if (time < request->repeat_time && request->last_scancode == request->scancode)
		request->repeat = 1;

	lastRecvMs = jiffies_to_msecs(jiffies);

	return 1;
}

static int rtk_ir_sky_wakeinfo(struct rtk_ir_wakeinfo *info, unsigned int scancode)
{
	info->addr = (scancode & 0xffff00) >> 8;
	info->addr_msk = 0xffff00;
	info->scancode = scancode & 0xff;
	info->scancode_msk = 0xff;

	return 0;
}

struct rtk_ir_hw_decoder rtk_ir_sky = {
	.type = RC_PROTO_BIT_RC6_0,
	.tolerance = 70,
	.unit = 444,	/* 562.5 us */
	.sr = 20,	/* sample rate = 40 us */
	.timings = {
		.ldr = {	/* leader symbol */
			.pulse = 6,	/* 9ms */
			.space = 2,	/* 4.5ms */
		},
		.s00 = {	/* 0 symbol */
			.pulse = 1,	/* 562.5 us */
			.space = 1,	/* 562.5 us */
		},
		.s01 = {	/* 1 symbol */
			.pulse = 1,	/* 562.5 us */
			.space = 1,	/* 1687.5 us */
		},
		.ft_min = 10000,	/* 10000 us */
	},
	.hw_set = IREDN_EN | 0x21 << 16 | IRUE | IRRES | IRIE |IRDPM,
	.repeat_time = 160,
	.scancode = rtk_ir_sky_scancode,
	.wakeinfo = rtk_ir_sky_wakeinfo,
};
