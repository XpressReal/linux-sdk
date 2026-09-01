/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek DisplayPort RX Link Training PHY Operations
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * Design principles:
 * 1. Integrate with existing rtk_dprx_phy_ops without breaking functionality
 * 2. Separate into [Required] and [Optional] operations
 * 3. Optional operations can be NULL, must check before calling
 * 4. Comply with Coverity requirements
 *
 * Return Value Convention (Option B):
 * PHY operations return error codes directly for precise debugging.
 *
 * Return values:
 *   0        = Success
 *   Positive = LT_ERR_xxx (specific error code, 120-199)
 *   Negative = Standard errno (e.g., -EINVAL for basic param errors)
 *
 * Example:
 *   if (reg_read_failed)
 *       return LT_ERR_PHY_REG_ACCESS;
 *
 * Caller handles error code assignment:
 *   ret = ops->check_cdr_lock(dprx, ...);
 *   if (ret) {
 *       lt->error_code = LT_ERR_IS_LT_ERROR(ret) ? ret : LT_ERR_PHY_CDR_STATUS_READ;
 *       return (ret < 0) ? ret : -EIO;
 *   }
 *
 * Error code ranges for PHY:
 *   150-159: PHY Configuration errors (LT_ERR_PHY_xxx)
 *   160-169: PHY Status/Operation errors (LT_ERR_PHY_xxx_READ)
 */

#ifndef __RTK_DPRX_LT_PHY_OPS_H__
#define __RTK_DPRX_LT_PHY_OPS_H__

#include <linux/types.h>
#include <linux/errno.h>
#include "rtk_dprx_link_training.h"  /* For RTK_DPRX_MAX_LANES, VS/PE defines */

/* Forward declaration */
struct rtk_dprx;

/*============================================================================
 * PHY Status Flags
 *============================================================================*/

/**
 * enum rtk_dprx_phy_status - PHY status flags
 *
 * Returned by get_phy_status() as bitmask
 */
enum rtk_dprx_phy_status {
	RTK_DPRX_PHY_STATUS_PLL_LOCKED   = BIT(0),
	RTK_DPRX_PHY_STATUS_CDR_LOCKED   = BIT(1),
	RTK_DPRX_PHY_STATUS_SIGNAL_DET   = BIT(2),
	RTK_DPRX_PHY_STATUS_EQ_DONE      = BIT(3),
	RTK_DPRX_PHY_STATUS_SYMBOL_LOCK  = BIT(4),
};

/*============================================================================
 * Link Training PHY Operations
 *
 * These operations are specifically for Link Training.
 * Designed to extend the existing rtk_dprx_phy_ops structure.
 *
 * Categories:
 * - [Required]: Essential for Link Training, cannot be NULL
 * - [Optional]: Depends on hardware requirements, can be NULL
 *
 * All functions take struct rtk_dprx *dprx as first parameter
 * Return: 0 = success, negative = error code
 *============================================================================*/

/**
 * struct rtk_dprx_lt_phy_ops - Link Training PHY operations
 *
 * All operations must check for NULL pointer before calling.
 * Use rtk_dprx_lt_phy_xxx() wrapper functions for automatic checking.
 */
struct rtk_dprx_lt_phy_ops {
	/*========================================
	 * [Required] Basic Configuration
	 *========================================*/

	/**
	 * configure_link - Configure PHY Link Rate and Lane Count
	 * @dprx: DPRX instance
	 * @link_rate: DPCD Link Rate value (0x06/0x0A/0x14/0x1E)
	 * @lane_count: Number of lanes (1/2/4)
	 *
	 * Called at Link Training start to configure PHY parameters.
	 * Should include: PLL config, lane enable, clock setup, etc.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*configure_link)(struct rtk_dprx *dprx, u8 link_rate, u8 lane_count);

	/**
	 * phy_reset - Reset PHY
	 * @dprx: DPRX instance
	 *
	 * Soft reset PHY for error recovery or re-training.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*phy_reset)(struct rtk_dprx *dprx);

	/*========================================
	 * [Required] Status Check Operations
	 *========================================*/

	/**
	 * get_phy_status - Get PHY status
	 * @dprx: DPRX instance
	 * @status: Output status bitmask (rtk_dprx_phy_status)
	 *
	 * Get various PHY status flags.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*get_phy_status)(struct rtk_dprx *dprx, u32 *status);

	/*========================================
	 * [Required] Clock Recovery Operations
	 *========================================*/

	/**
	 * start_cr - Start Clock Recovery
	 * @dprx: DPRX instance
	 *
	 * Called when TP1 received, start CDR (Clock Data Recovery).
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*start_cr)(struct rtk_dprx *dprx);

	/**
	 * check_cdr_lock - Check CDR Lock status
	 * @dprx: DPRX instance
	 * @lane_mask: Lane bitmask to check (bit0=lane0, ...)
	 * @locked_mask: Output locked lane bitmask
	 *
	 * Check if specified lanes have CDR locked.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*check_cdr_lock)(struct rtk_dprx *dprx, u8 lane_mask, u8 *locked_mask);

	/*========================================
	 * [Required] Channel Equalization Operations
	 *========================================*/

	/**
	 * start_eq - Start Channel Equalization
	 * @dprx: DPRX instance
	 * @tp: Training Pattern (2/3/4)
	 *
	 * Called when TP2/3/4 received, start EQ adaptation.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*start_eq)(struct rtk_dprx *dprx, u8 tp);

	/**
	 * stop_eq - Stop Channel Equalization
	 * @dprx: DPRX instance
	 *
	 * Called when EQ phase ends, disable EQ CRC checking.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*stop_eq)(struct rtk_dprx *dprx);

	/**
	 * check_eq_done - Check EQ completion status
	 * @dprx: DPRX instance
	 * @lane_mask: Lane bitmask to check
	 * @done_mask: Output EQ completed lane bitmask
	 *
	 * Check if specified lanes have completed EQ.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*check_eq_done)(struct rtk_dprx *dprx, u8 lane_mask, u8 *done_mask);

	/*========================================
	 * [Required] Adjust Request (DPCD 0x206-0x207)
	 *
	 * RX PHY recommends VS/PE adjustment based on signal quality.
	 * Software reads this and writes to DPCD 0x206-0x207.
	 *========================================*/

	/**
	 * get_adjust_request - Get PHY recommended VS/PE adjustment
	 * @dprx: DPRX instance
	 * @lane: Lane number (0-3)
	 * @vs_level: Output recommended Voltage Swing Level (0-3)
	 * @pe_level: Output recommended Pre-Emphasis Level (0-3)
	 *
	 * PHY recommends VS/PE based on current signal quality.
	 * Software writes this value to DPCD 0x206-0x207.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*get_adjust_request)(struct rtk_dprx *dprx, u8 lane,
				  u8 *vs_level, u8 *pe_level);

	/*========================================
	 * [Optional] Advanced Features
	 *========================================*/

	/**
	 * set_lane_polarity - Set lane polarity inversion
	 * @dprx: DPRX instance
	 * @lane: Lane number (0-3)
	 * @invert: true = inverted, false = normal
	 *
	 * [Optional] Used for Type-C flip orientation polarity adjustment.
	 */
	void (*set_lane_polarity)(struct rtk_dprx *dprx, u8 lane, bool invert);

	/**
	 * set_ssc - Set Spread Spectrum Clocking
	 * @dprx: DPRX instance
	 * @enable: true = enable SSC
	 *
	 * [Optional] Enable/disable spread spectrum clocking.
	 */
	void (*set_ssc)(struct rtk_dprx *dprx, bool enable);

	/**
	 * get_error_count - Get error count
	 * @dprx: DPRX instance
	 * @lane: Lane number (0-3)
	 * @error_count: Output error count
	 *
	 * [Optional] Get lane symbol error count.
	 *
	 * Return: 0 on success, negative on error
	 */
	int (*get_error_count)(struct rtk_dprx *dprx, u8 lane, u32 *error_count);
};

/*============================================================================
 * PHY Ops Wrapper Functions (Coverity Compliant)
 *
 * These wrapper functions handle NULL checks automatically,
 * simplifying caller code.
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_configure_link - Configure PHY link (with NULL check)
 */
static inline int rtk_dprx_lt_phy_configure_link(struct rtk_dprx *dprx,
						 const struct rtk_dprx_lt_phy_ops *ops,
						 u8 link_rate, u8 lane_count)
{
	if (!dprx || !ops || !ops->configure_link)
		return -ENODEV;
	return ops->configure_link(dprx, link_rate, lane_count);
}

/**
 * rtk_dprx_lt_phy_reset - Reset PHY (with NULL check)
 */
static inline int rtk_dprx_lt_phy_reset(struct rtk_dprx *dprx,
					const struct rtk_dprx_lt_phy_ops *ops)
{
	if (!dprx || !ops || !ops->phy_reset)
		return -ENODEV;
	return ops->phy_reset(dprx);
}

/**
 * rtk_dprx_lt_phy_get_status - Get PHY status (with NULL check)
 */
static inline int rtk_dprx_lt_phy_get_status(struct rtk_dprx *dprx,
					     const struct rtk_dprx_lt_phy_ops *ops,
					     u32 *status)
{
	if (!dprx || !ops || !ops->get_phy_status || !status)
		return -EINVAL;
	return ops->get_phy_status(dprx, status);
}

/**
 * rtk_dprx_lt_phy_start_cr - Start Clock Recovery (with NULL check)
 */
static inline int rtk_dprx_lt_phy_start_cr(struct rtk_dprx *dprx,
					   const struct rtk_dprx_lt_phy_ops *ops)
{
	if (!dprx || !ops || !ops->start_cr)
		return -ENODEV;
	return ops->start_cr(dprx);
}

/**
 * rtk_dprx_lt_phy_check_cdr_lock - Check CDR lock (with NULL check)
 */
static inline int rtk_dprx_lt_phy_check_cdr_lock(struct rtk_dprx *dprx,
						 const struct rtk_dprx_lt_phy_ops *ops,
						 u8 lane_mask, u8 *locked_mask)
{
	if (!dprx || !ops || !ops->check_cdr_lock || !locked_mask)
		return -EINVAL;
	return ops->check_cdr_lock(dprx, lane_mask, locked_mask);
}

/**
 * rtk_dprx_lt_phy_start_eq - Start Channel EQ (with NULL check)
 */
static inline int rtk_dprx_lt_phy_start_eq(struct rtk_dprx *dprx,
					   const struct rtk_dprx_lt_phy_ops *ops,
					   u8 tp)
{
	if (!dprx || !ops || !ops->start_eq)
		return -ENODEV;
	return ops->start_eq(dprx, tp);
}

/**
 * rtk_dprx_lt_phy_stop_eq - Stop Channel EQ (with NULL check)
 */
static inline int rtk_dprx_lt_phy_stop_eq(struct rtk_dprx *dprx,
					  const struct rtk_dprx_lt_phy_ops *ops)
{
	if (!dprx || !ops || !ops->stop_eq)
		return -ENODEV;
	return ops->stop_eq(dprx);
}

/**
 * rtk_dprx_lt_phy_check_eq_done - Check EQ done (with NULL check)
 */
static inline int rtk_dprx_lt_phy_check_eq_done(struct rtk_dprx *dprx,
						const struct rtk_dprx_lt_phy_ops *ops,
						u8 lane_mask, u8 *done_mask)
{
	if (!dprx || !ops || !ops->check_eq_done || !done_mask)
		return -EINVAL;
	return ops->check_eq_done(dprx, lane_mask, done_mask);
}

/**
 * rtk_dprx_lt_phy_get_adjust_request - Get adjust request (with NULL check)
 *
 * PHY recommended VS/PE for TX, to be written to DPCD 0x206-0x207.
 */
static inline int rtk_dprx_lt_phy_get_adjust_request(struct rtk_dprx *dprx,
						     const struct rtk_dprx_lt_phy_ops *ops,
						     u8 lane, u8 *vs_level, u8 *pe_level)
{
	if (!dprx || !ops || !ops->get_adjust_request)
		return -ENODEV;
	if (!vs_level || !pe_level)
		return -EINVAL;
	if (lane >= RTK_DPRX_MAX_LANES)
		return -EINVAL;
	return ops->get_adjust_request(dprx, lane, vs_level, pe_level);
}

/*============================================================================
 * PHY Ops Validation
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_ops_validate - Validate required ops are implemented
 * @ops: PHY operations structure
 *
 * Return: 0 if valid, negative if missing required ops
 */
static inline int rtk_dprx_lt_phy_ops_validate(const struct rtk_dprx_lt_phy_ops *ops)
{
	if (!ops)
		return -EINVAL;

	/* Check required operations */
	if (!ops->configure_link) {
		pr_err("rtk_dprx_lt: missing required op: configure_link\n");
		return -EINVAL;
	}
	if (!ops->phy_reset) {
		pr_err("rtk_dprx_lt: missing required op: phy_reset\n");
		return -EINVAL;
	}
	if (!ops->get_phy_status) {
		pr_err("rtk_dprx_lt: missing required op: get_phy_status\n");
		return -EINVAL;
	}
	if (!ops->start_cr) {
		pr_err("rtk_dprx_lt: missing required op: start_cr\n");
		return -EINVAL;
	}
	if (!ops->check_cdr_lock) {
		pr_err("rtk_dprx_lt: missing required op: check_cdr_lock\n");
		return -EINVAL;
	}
	if (!ops->start_eq) {
		pr_err("rtk_dprx_lt: missing required op: start_eq\n");
		return -EINVAL;
	}
	if (!ops->check_eq_done) {
		pr_err("rtk_dprx_lt: missing required op: check_eq_done\n");
		return -EINVAL;
	}
	if (!ops->get_adjust_request) {
		pr_err("rtk_dprx_lt: missing required op: get_adjust_request\n");
		return -EINVAL;
	}

	/* Optional operations not checked */

	return 0;
}

/*============================================================================
 * Default Implementation
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_ops_default - Default implementation of LT PHY ops
 *
 * Integrates with existing rtk_dprx_phy_ops, provides basic PHY operations.
 * Functions marked TODO need adjustment based on actual hardware.
 */
extern const struct rtk_dprx_lt_phy_ops rtk_dprx_lt_phy_ops_default;

/**
 * rtk_dprx_lt_phy_ops_register - Register default PHY ops
 * @dprx: DPRX instance
 *
 * Return: 0 on success
 */
int rtk_dprx_lt_phy_ops_register(struct rtk_dprx *dprx);

#endif /* __RTK_DPRX_LT_PHY_OPS_H__ */
