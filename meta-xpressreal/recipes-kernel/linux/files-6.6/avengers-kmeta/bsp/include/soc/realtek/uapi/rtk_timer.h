/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */

#ifndef __UAPI_RTK_TIMER_H
#define __UAPI_RTK_TIMER_H

#include "rtk_pm.h"

#define TC_WK_VER 0x01000000

struct tc_wakeup_param {
	struct pm_pcpu_param_hdr hdr;
	uint32_t timeout; /* Timer in seconds */
};

#endif /* __UAPI_RTK_TIMER_H */
