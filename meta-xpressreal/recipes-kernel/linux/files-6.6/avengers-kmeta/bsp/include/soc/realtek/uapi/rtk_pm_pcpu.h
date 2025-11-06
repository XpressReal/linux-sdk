/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */

#ifndef __UAPI_RTK_PM_PCPU_H
#define __UAPI_RTK_PM_PCPU_H

#include "rtk_ir.h"  // struct ipc_shm_irda
#include "rtk_cec.h" // struct ipc_shm_cec

#define WKM_VER		0x01000000

#define GPIO_MAX_SIZE_V1 86
#define GPIO_MAX_SIZE_V2 192
struct pm_pcpu_param_v1 {
	unsigned int wakeup_source;
	unsigned int timerout_val;
	char wu_gpio_en[GPIO_MAX_SIZE_V1];
	char wu_gpio_act[GPIO_MAX_SIZE_V1];
	struct ipc_shm_irda_v1 irda_info;
	struct ipc_shm_cec_v1 cec_info;
	unsigned int bt;
} __packed;

struct pm_pcpu_param_v2 {
	unsigned int wakeup_source;
	unsigned int timerout_val;
	char wu_gpio_en[GPIO_MAX_SIZE_V2];
	char wu_gpio_act[GPIO_MAX_SIZE_V2];
	struct ipc_shm_irda_v2 irda_info;
	struct ipc_shm_cec_v2 cec_info;
	unsigned int bt;
} __packed;

enum rtk_wakeup_event {
	LAN_EVENT = 0,
	IR_EVENT,
	GPIO_EVENT,
	ALARM_EVENT,
	TIMER_EVENT,
	CEC_EVENT,
	USB_EVENT,
	HIFI_EVENT,
	VTC_EVENT,
	PON_EVENT,
	MAX_EVENT,
};

enum rtk_wakeup_event_bitmap {
	WAKEUP_LAN = 0x1U << LAN_EVENT,
	WAKEUP_IR = 0x1U << IR_EVENT,
	WAKEUP_GPIO = 0x1U << GPIO_EVENT,
	WAKEUP_ALARM = 0x1U << ALARM_EVENT,
	WAKEUP_TIMER = 0x1U << TIMER_EVENT,
	WAKEUP_CEC = 0x1U << CEC_EVENT,
	WAKEUP_USB = 0x1U << USB_EVENT,
	WAKEUP_HIFI = 0x1U << HIFI_EVENT,
	WAKEUP_VTC = 0x1U << VTC_EVENT,
};

#define PCPU_WAKEUP_EVENT_MASK   0xfff
#define PCPU_FLAGS_DCO_ENABLED   0x1000 /* notify dco is enabled */
#define PCPU_FLAGS_IGNORE_PD_PIN 0x2000 /* pcpu should ignore setting pd pin */

#define DCO_ENABLE        PCPU_FLAGS_DCO_ENABLED

#endif /* __UAPI_RTK_PM_PCPU_H */
