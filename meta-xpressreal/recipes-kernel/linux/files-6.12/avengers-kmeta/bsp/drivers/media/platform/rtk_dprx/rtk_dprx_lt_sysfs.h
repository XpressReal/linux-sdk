/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek DisplayPort RX Link Training - Sysfs Interface
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * Sysfs nodes (under /sys/devices/.../link_training/):
 *
 *   Read-only:
 *     state       - Current LT state name
 *     trained     - Link trained status (0/1)
 *     link_rate   - Current link rate (hex and name)
 *     lane_count  - Current lane count
 *     error_code  - Last error code with category
 *     stats       - Full training statistics
 *     lane_status - Per-lane CR/EQ/Symbol status
 *
 *   Write-only:
 *     reset       - Reset state machine (write 1)
 *     stats_reset - Reset statistics (write 1)
 *
 *   Read-write:
 *     debug       - Debug mode enable (0/1)
 */

#ifndef __RTK_DPRX_LT_SYSFS_H__
#define __RTK_DPRX_LT_SYSFS_H__

struct rtk_dprx;

/**
 * rtk_dprx_lt_sysfs_init - Create sysfs nodes
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_sysfs_init(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_sysfs_exit - Remove sysfs nodes
 * @dprx: DPRX instance
 */
void rtk_dprx_lt_sysfs_exit(struct rtk_dprx *dprx);

#endif /* __RTK_DPRX_LT_SYSFS_H__ */
