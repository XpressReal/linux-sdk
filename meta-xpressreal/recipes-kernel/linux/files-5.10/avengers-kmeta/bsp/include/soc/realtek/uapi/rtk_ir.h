/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2017 Realtek Semiconductor Corp.
 */

#ifndef __UAPI_RTK_IR_H
#define __UAPI_RTK_IR_H

#include "rtk_pm.h"

#define MAX_WAKEUP_CODE	16
#define MAX_KEY_TBL	2

#define IRDA_WK_VER	0x01000000
#define IRDA_PKEY_ANY	0xffffffff

struct irda_wakeup_key_v1 {
	unsigned int protocol;
	unsigned int scancode_mask;
	unsigned int wakeup_keynum;
	unsigned int wakeup_scancode[MAX_WAKEUP_CODE];
	unsigned int cus_mask;
	unsigned int cus_code;
};

struct ipc_shm_irda_v1 {
	unsigned int ipc_shm_ir_magic;
	unsigned int dev_count;
	struct irda_wakeup_key_v1 key_tbl[MAX_KEY_TBL];
};

struct irda_wakeup_key_v2 {
	uint8_t valid;
	uint8_t pkey_nr; /* real key num */
	uint8_t rsvd[2];
	uint8_t pkey_start; /* Power key start bit*/
	uint8_t pkey_end; /* Power key end bit */

	uint8_t ckey_start; /* Custom key start bit*/
	uint8_t ckey_end; /* Custom key start bit*/

	uint32_t ckey;	 /* Custome key */
	uint32_t pkeys[MAX_WAKEUP_CODE]; /* Power key array */
};

struct ipc_shm_irda_v2 {
	struct pm_pcpu_param_hdr header;
	uint32_t ckey_nr;
	struct irda_wakeup_key_v2 key_tbl[6];
};

#endif /* __UAPI_RTK_IR_H */
