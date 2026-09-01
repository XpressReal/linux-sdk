/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek npu_pp video m2m and capture v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <linux/videodev2.h>
#include <linux/printk.h>

/*
 * Enabling verbose debug messages is done through the rtk_npp.debug parameter,
 * each category being enabled by a bit.
 *
 * rtk_npp.debug=0x1 will enable DEBUG messages
 * ...
 *
 * An interesting feature is that it's possible to enable verbose logging at
 * run-time by echoing the debug value in its sysfs node:
 *   # echo 0x1 > /sys/module/rtk_npp/parameters/debug
 */
#define NPP_DBG_NONE		0x00
#define NPP_DBG_ON       (1 << 0)    // enable DEBUG messages


extern __printf(3, 4)
void npp_printk(const char *level, unsigned int category,
		const char *format, ...);

#define npp_err(fmt, ...) \
	npp_printk(KERN_ERR, NPP_DBG_NONE, fmt,	##__VA_ARGS__)

#define npp_warn(fmt, ...) \
	npp_printk(KERN_WARNING, NPP_DBG_NONE, fmt,	##__VA_ARGS__)

#define npp_info(fmt, ...) \
	npp_printk(KERN_INFO, NPP_DBG_NONE, fmt,	##__VA_ARGS__)

#define npp_dbg(fmt, ...) \
	npp_printk(KERN_DEBUG, NPP_DBG_ON, fmt,	##__VA_ARGS__)

#define V4L2_TYPE_TO_STR(type) (V4L2_TYPE_IS_OUTPUT(type) ? "out":"cap")

#endif
