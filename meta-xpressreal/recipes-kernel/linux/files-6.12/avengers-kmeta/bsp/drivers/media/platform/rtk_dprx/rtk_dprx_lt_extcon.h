/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek DisplayPort RX Link Training - Extcon/PHY Consumer Interface
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This module consumes extcon and PHY resources from the USB3/TypeC PHY
 * driver via device tree references.
 *
 * Device Tree Example:
 *   dprx: dprx@... {
 *       compatible = "realtek,rtk-dprx";
 *       extcon = <&usb3_typec>;    // Reference to TypeC extcon provider
 *       phys = <&usb0_u3phy PHY_TYPE_DP>; // Reference to TypeC PHY provider
 *       phy-names = "dprx-phy";
 *   };
 */

#ifndef __RTK_DPRX_LT_EXTCON_H__
#define __RTK_DPRX_LT_EXTCON_H__

#include <linux/types.h>

/* Forward declarations */
struct rtk_dprx;

#ifdef CONFIG_EXTCON

/*============================================================================
 * Extcon Consumer API
 *============================================================================*/

/**
 * rtk_dprx_lt_extcon_init - Initialize extcon consumer
 * @dprx: DPRX instance
 *
 * Gets extcon device from device tree ("extcon" property) and registers
 * a notifier for EXTCON_DISP_DP state changes.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_extcon_init(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_extcon_exit - Cleanup extcon consumer
 * @dprx: DPRX instance
 */
void rtk_dprx_lt_extcon_exit(struct rtk_dprx *dprx);

/*============================================================================
 * PHY Consumer API
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_consumer_init - Initialize PHY consumer
 * @dprx: DPRX instance
 *
 * Gets DP PHY from device tree ("phys" property with "dp-phy" name).
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_consumer_init(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_phy_consumer_exit - Cleanup PHY consumer
 * @dprx: DPRX instance
 */
void rtk_dprx_lt_phy_consumer_exit(struct rtk_dprx *dprx);

/*============================================================================
 * PHY Operations (via kernel PHY framework)
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_init - Initialize the DP PHY
 * @dprx: DPRX instance
 *
 * Calls phy_init() on the DP PHY obtained from device tree.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_init(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_phy_exit - De-initialize the DP PHY
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_exit(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_phy_power_on - Power on the DP PHY
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_power_on(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_phy_power_off - Power off the DP PHY
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_power_off(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_phy_configure - Configure DP PHY for link training
 * @dprx: DPRX instance
 * @link_rate: Link rate (DPCD 0x100 value: 0x06=RBR, 0x0A=HBR, 0x14=HBR2, 0x1E=HBR3)
 * @lane_count: Number of lanes (1, 2, or 4)
 *
 * Uses phy_configure() with phy_configure_opts_dp to set link parameters.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_configure(struct rtk_dprx *dprx, u8 link_rate, u8 lane_count);

/**
 * rtk_dprx_lt_phy_calibrate - Calibrate the DP PHY for EQ phase
 * @dprx: DPRX instance
 *
 * Called after CR_DONE=1 as preparation for EQ training.
 * This allows PHY driver to perform internal signal quality calibration.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_calibrate(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_phy_notify_connect - Notify PHY of DP connection (assert HPD)
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_notify_connect(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_phy_notify_disconnect - Notify PHY of DP disconnection (de-assert HPD)
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_notify_disconnect(struct rtk_dprx *dprx);

/*============================================================================
 * Query Functions
 *============================================================================*/

/**
 * rtk_dprx_lt_is_connected - Check if DP source is connected
 * @dprx: DPRX instance
 *
 * Queries extcon state for EXTCON_DISP_DP.
 *
 * Return: true if connected, false otherwise
 */
bool rtk_dprx_lt_is_connected(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_get_typec_polarity - Get Type-C cable polarity
 * @dprx: DPRX instance
 *
 * Return: true if flipped, false if normal orientation
 */
bool rtk_dprx_lt_get_typec_polarity(struct rtk_dprx *dprx);

#else /* !CONFIG_EXTCON */

/* Stubs when CONFIG_EXTCON is not enabled */
static inline int rtk_dprx_lt_extcon_init(struct rtk_dprx *dprx) { return 0; }
static inline void rtk_dprx_lt_extcon_exit(struct rtk_dprx *dprx) { }
static inline int rtk_dprx_lt_phy_consumer_init(struct rtk_dprx *dprx) { return 0; }
static inline void rtk_dprx_lt_phy_consumer_exit(struct rtk_dprx *dprx) { }
static inline int rtk_dprx_lt_phy_init(struct rtk_dprx *dprx) { return 0; }
static inline int rtk_dprx_lt_phy_exit(struct rtk_dprx *dprx) { return 0; }
static inline int rtk_dprx_lt_phy_power_on(struct rtk_dprx *dprx) { return 0; }
static inline int rtk_dprx_lt_phy_power_off(struct rtk_dprx *dprx) { return 0; }
static inline int rtk_dprx_lt_phy_configure(struct rtk_dprx *dprx, u8 link_rate,
					    u8 lane_count) { return 0; }
static inline int rtk_dprx_lt_phy_calibrate(struct rtk_dprx *dprx) { return 0; }
static inline int rtk_dprx_lt_phy_notify_connect(struct rtk_dprx *dprx) { return 0; }
static inline int rtk_dprx_lt_phy_notify_disconnect(struct rtk_dprx *dprx) { return 0; }
static inline bool rtk_dprx_lt_is_connected(struct rtk_dprx *dprx) { return false; }
static inline bool rtk_dprx_lt_get_typec_polarity(struct rtk_dprx *dprx) { return false; }

#endif /* CONFIG_EXTCON */

#endif /* __RTK_DPRX_LT_EXTCON_H__ */
