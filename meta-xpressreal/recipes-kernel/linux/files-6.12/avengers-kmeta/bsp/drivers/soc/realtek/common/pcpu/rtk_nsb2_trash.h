/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */
#ifndef __RTK_NSB2_TRASH_H__
#define __RTK_NSB2_TRASH_H__

#include <linux/types.h>


#define NSB2_TRASH_SMCCC_VER	0x82000c00
#define NSB2_TRASH_SMCCC_EN	0x82000c01
#define NSB2_TRASH_SMCCC_DIS	0x82000c02


struct rtk_nsb2_trash_param {
	u32			start;		/* start address */
	u32			end;		/* end address */
	u32			mode;		/* read, write */
	u32			cpus;		/* what cpus to check */
};

enum {
	NSB2_TRASH_MODE_RW	= 0,
	NSB2_TRASH_MODE_RD	= 1,
	NSB2_TRASH_MODE_WR	= 2,
};

enum {
	NSB2_TRASH_CPU_DCPU	= 0,
	NSB2_TRASH_CPU_PCPU	= 1,
	NSB2_TRASH_CPU_MCU	= 2,
};

struct rtk_nsb2_trash_state {
	int				enabled;
	struct rtk_nsb2_trash_param	param;
};

struct rtk_nsb2_trash_meta {
	int				nr_set;
	struct attribute_group		*attr_group;
};

struct platform_device;

struct rtk_nsb2_trash_data {
	struct platform_device		*pdev;

	int				version;	/* reserved for extend */
	int				nr_set;
	struct rtk_nsb2_trash_state	*cfgs;
	struct rtk_nsb2_trash_meta	*meta;
};


#endif	/* __RTK_NSB2_TRASH_H__ */
