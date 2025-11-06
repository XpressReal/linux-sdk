/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */

#ifndef __UAPI_RTK_RTC_H
#define __UAPI_RTK_RTC_H

#include <soc/realtek/rtk_pm.h>

#define RTC_WK_VER 0x01000000

struct rtc_wakeup_param {
	struct pm_pcpu_param_hdr hdr;

	uint8_t min;
	uint8_t hr;
	uint16_t date;
};

#endif /* __UAPI_RTK_RTC_H */
