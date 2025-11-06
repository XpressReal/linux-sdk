/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */

#ifndef __UAPI_RTK_GPIO_H
#define __UAPI_RTK_GPIO_H

#include "rtk_pm_pcpu.h"

enum {
	GPIO_WK_RAISING = 0,
	GPIO_WK_FALLING = 1,
	GPIO_WK_LEVEL = 2,
};

#define GPIO_WK_VER		0x01000000
#define GPIO_WK_FLAG_RAISING	BIT(GPIO_WK_RAISING)
#define GPIO_WK_FLAG_FALLING	BIT(GPIO_WK_FALLING)
#define GPIO_WK_FLAG_LEVEL	BIT(GPIO_WK_LEVEL)

 /* Number of uint32 to present GPIO pins */
#define GPIO_MAP_NR(_x)	(((_x) + 31) >> 5)
#define GPIO_MAP_SIZE(_x) ((((_x) + 31) >> 5) * 4)

struct gpio_wakeup_param {
	struct pm_pcpu_param_hdr hdr;
	uint32_t bitmap[12]; /* Bitmap array to present enabled GPIOs */
};

#endif /* __UAPI_RTK_GPIO_H */
