// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek DisplayPort RX Link Training PHY Operations
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This file provides Link Training PHY Ops implementation,
 * integrating with existing rtk_dprx_phy_ops functionality.
 *
 * Functions marked TODO need adjustment based on actual hardware.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/minmax.h>

#include "rtk_dprx.h"
#include "rtk_dprx_link_training.h"
#include "rtk_dprx_lt_phy_ops.h"
#include "rtk_dprx_lt_extcon.h"
#include "dprx14_mac_ip_reg.h"

#include <trace/events/rtk_dprx_trace.h>

/*============================================================================
 * Register Definitions (for direct register access)
 *
 * These should match the definitions in rtk_dprx_ip_reg.h
 *============================================================================*/

/* Channel FIFO Sync - CDR Ready bits */
#define REG_CHANNEL_FIFO_SYNC		0x0C  /* PB_0C */
#define CDR_RDY_LN0			BIT(7)
#define CDR_RDY_LN1			BIT(6)
#define CDR_RDY_LN2			BIT(5)
#define CDR_RDY_LN3			BIT(4)

/* Lane Status bits in DPCD 0x202-0x203 */
#define DPCD_LANE_CR_DONE		BIT(0)
#define DPCD_LANE_EQ_DONE		BIT(1)
#define DPCD_LANE_SYMBOL_LOCKED		BIT(2)

/*============================================================================
 * Internal Helper Functions
 *============================================================================*/

/**
 * rtk_dprx_is_valid_link_rate - Check if link rate is valid
 * @link_rate: DPCD link rate value
 *
 * Return: true if valid, false otherwise
 */
static inline bool rtk_dprx_is_valid_link_rate(u8 link_rate)
{
	switch (link_rate) {
	case 0x06:  /* RBR: 1.62 Gbps */
	case 0x0A:  /* HBR: 2.7 Gbps */
	case 0x14:  /* HBR2: 5.4 Gbps */
	case 0x1E:  /* HBR3: 8.1 Gbps */
		return true;
	default:
		return false;
	}
}

/**
 * rtk_dprx_is_valid_lane_count - Check if lane count is valid
 * @lane_count: Number of lanes
 *
 * Return: true if valid, false otherwise
 */
static inline bool rtk_dprx_is_valid_lane_count(u8 lane_count)
{
	return (lane_count == 1 || lane_count == 2 || lane_count == 4);
}

/**
 * stub_get_lane_mask - Convert lane count to bitmask
 */
static inline u8 stub_get_lane_mask(u8 lane_count)
{
	switch (lane_count) {
	case 1:
		return 0x01;
	case 2:
		return 0x03;
	case 4:
		return 0x0F;
	default:
		return 0x0F;
	}
}

/*============================================================================
 * [Required] Basic Configuration Operations
 *============================================================================*/

/**
 * stub_configure_link - Configure PHY Link Rate and Lane Count
 *
 * Integrates existing phy_ops->set_link_rate() and set_lane_count()
 *
 * Return: 0 on success, LT_ERR_xxx on failure, or negative errno
 */
static int stub_configure_link(struct rtk_dprx *dprx, u8 link_rate, u8 lane_count)
{
	struct rtk_dprx_lt_context *lt;
	int ret;

	/* Coverity: NULL check */
	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	/* Validate link rate */
	if (!rtk_dprx_is_valid_link_rate(link_rate)) {
		dev_err(dprx->dev, "PHY: Invalid link rate 0x%02x\n", link_rate);
		return LT_ERR_PHY_RATE_NOT_SUPPORTED;
	}

	/* Validate lane count */
	if (!rtk_dprx_is_valid_lane_count(lane_count)) {
		dev_err(dprx->dev, "PHY: Invalid lane count %d\n", lane_count);
		return LT_ERR_PHY_LANE_CONFIG_FAIL;
	}

	if ((lane_count == _DP_TWO_LANE) || (lane_count == _DP_ONE_LANE)) {
		dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~(_BIT4 | _BIT3),
			(lane_count == _DP_TWO_LANE) ? _BIT4 : _BIT3);
		dprx->rbus_ops->set_bit(PB_02_PHY_DIG_RESET2_CTRL, ~(_BIT7 | _BIT6),
			lt->typec_flipped ? 0 : _BIT7);
	}

	/* Configure PHY via kernel PHY framework */
	ret = rtk_dprx_lt_phy_configure(dprx, link_rate, lane_count);
	if (ret) {
		dev_err(dprx->dev, "PHY configure failed: %d\n", ret);
		return LT_ERR_PHY_CONFIG_FAIL;
	}

	dev_dbg(dprx->dev, "PHY: Configured link rate=0x%02x lanes=%d\n",
		link_rate, lane_count);

	return 0;
}

/**
 * stub_phy_reset - Reset PHY
 *
 * Use existing rebuild_phy or perform soft reset
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_phy_reset(struct rtk_dprx *dprx)
{
	/* Coverity: NULL check */
	if (!dprx)
		return -EINVAL;

	/*
	 * Note: rebuild_phy() requires link_rate and lane_count parameters,
	 * here we only do basic reset. Full reset should be called after configure_link.
	 */

	/* TODO: Call actual PHY reset, return error code on failure:
	 * if (phy_reset_failed)
	 *     return LT_ERR_PHY_RESET_FAIL;
	 */

	dev_dbg(dprx->dev, "PHY: Reset\n");

	/* May need to wait for PHY to stabilize */
	usleep_range(100, 200);

	return 0;
}

/*============================================================================
 * [Required] Status Check Operations
 *============================================================================*/

/**
 * stub_get_phy_status - Get PHY status
 *
 * Read hardware registers and compose status bitmask
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_get_phy_status(struct rtk_dprx *dprx, u32 *status)
{
	u32 phy_status = 0;
	u8 fifo_sync;

	/* Coverity: NULL check */
	if (!dprx || !status)
		return LT_ERR_NULL_POINTER;

	if (!dprx->rbus_ops || !dprx->rbus_ops->get_byte) {
		*status = 0;
		return LT_ERR_PHY_STATUS_READ;
	}

	/* Read CDR ready status from CHANNEL_FIFO_SYNC register */
	fifo_sync = dprx->rbus_ops->get_byte(REG_CHANNEL_FIFO_SYNC);

	/* Check if any lane has CDR locked */
	if (fifo_sync & (CDR_RDY_LN0 | CDR_RDY_LN1 | CDR_RDY_LN2 | CDR_RDY_LN3))
		phy_status |= RTK_DPRX_PHY_STATUS_CDR_LOCKED;

	/* TODO: Add PLL lock status check */
	/* TODO: Add signal detection status */

	*status = phy_status;

	return 0;
}

/*============================================================================
 * [Required] Clock Recovery Operations
 *============================================================================*/

/**
 * stub_start_cr - Start Clock Recovery
 *
 * Called when TP1 received, start CDR
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_start_cr(struct rtk_dprx *dprx)
{
	/* Coverity: NULL check */
	if (!dprx)
		return -EINVAL;

	/* Use existing set_training_pattern */
	if (dprx->phy_ops && dprx->phy_ops->set_training_pattern)
		dprx->phy_ops->set_training_pattern(dprx, 1);  /* TP1 */

	dev_dbg(dprx->dev, "PHY: Start CR (TP1)\n");

	return 0;
}

/**
 * phy_lane_to_dp_mask - Remap physical HW lane bits to logical DP lane mask
 *
 * EQ_CRC_3/EQ_CRC_1 registers report physical hardware lane status (before
 * LANE_MUX). This function converts that to the logical DP lane numbering:
 *   typec_flipped=1 (LANE_MUX=0xB1): swap within pairs (ln0↔ln1, ln2↔ln3)
 *                                     physical ln0→logical 1, ln1→logical 0,
 *                                     physical ln2→logical 3, ln3→logical 2
 *   typec_flipped=0 (LANE_MUX=0x4E): swap between pairs ({0,1}↔{2,3})
 *                                     physical ln0→logical 2, ln1→logical 3,
 *                                     physical ln2→logical 0, ln3→logical 1
 *
 * @phy_mask: 4-bit mask where bit N represents physical lane N status
 * @typec_flipped: Type-C cable orientation flag
 * Return: 4-bit logical DP lane mask
 */
static u8 phy_lane_to_dp_mask(u8 phy_mask, bool typec_flipped)
{
	u8 dp_mask;

	if (typec_flipped)
		/* Flipped (LANE_MUX=0xB1): swap within pairs {0<->1, 2<->3} */
		dp_mask = (u8)(((phy_mask & 0x05) << 1) | ((phy_mask & 0x0A) >> 1));
	else
		/* Normal (LANE_MUX=0x4E): swap between pairs {0,1} <-> {2,3} */
		dp_mask = (u8)(((phy_mask & 0x03) << 2) | ((phy_mask >> 2) & 0x03));

	trace_dprx_lt_phy_lane_remap(typec_flipped, phy_mask, dp_mask);

	return dp_mask;
}

/**
 * stub_check_cdr_lock - Check CDR Lock status
 *
 * Read TP1 detection status from 0x98166050 (EQ_CRC_3) register.
 * TP1 detection indicates CDR has locked to the incoming signal.
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_check_cdr_lock(struct rtk_dprx *dprx, u8 lane_mask, u8 *locked_mask)
{
	struct rtk_dprx_lt_context *lt;
	u8 tp1_detect;
	u8 phy_mask = 0;
	u8 result = 0;

	/* Coverity: NULL check */
	if (!dprx || !locked_mask)
		return LT_ERR_NULL_POINTER;

	if (!dprx->rbus_ops || !dprx->rbus_ops->get_byte) {
		*locked_mask = 0;
		return LT_ERR_PHY_CDR_STATUS_READ;
	}

	lt = &dprx->lt_ctx;

	/* Read TP1 detection status from 0x98166050 */
	tp1_detect = dprx->rbus_ops->get_byte(DPRX14_MAC_IP_EQ_CRC_3);

	/*
	 * Map TP1 detect bits to physical lane mask (Bit[4]=ln0 ... Bit[7]=ln3),
	 * then remap physical lanes to logical DP lanes via phy_lane_to_dp_mask().
	 *   typec_flipped=0 (LANE_MUX=0x4E): pairs {0,1} <-> {2,3}
	 *   typec_flipped=1 (LANE_MUX=0xB1): direct 1:1 mapping
	 */
	if (tp1_detect & DPRX14_MAC_IP_EQ_CRC_3_tp1_detect_ln0_mask)
		phy_mask |= BIT(0);
	if (tp1_detect & DPRX14_MAC_IP_EQ_CRC_3_tp1_detect_ln1_mask)
		phy_mask |= BIT(1);
	if (tp1_detect & DPRX14_MAC_IP_EQ_CRC_3_tp1_detect_ln2_mask)
		phy_mask |= BIT(2);
	if (tp1_detect & DPRX14_MAC_IP_EQ_CRC_3_tp1_detect_ln3_mask)
		phy_mask |= BIT(3);

	result = phy_lane_to_dp_mask(phy_mask, lt->typec_flipped);

	*locked_mask = result & lane_mask;

	trace_dprx_lt_cdr_tp1_detect(lane_mask, tp1_detect, *locked_mask);

	dev_dbg(dprx->dev, "PHY: CDR lock check mask=0x%02x result=0x%02x (tp1_detect=0x%02x)\n",
		lane_mask, *locked_mask, tp1_detect);

	return 0;
}

/*============================================================================
 * [Required] Channel Equalization Operations
 *============================================================================*/

/**
 * stub_start_eq - Start Channel Equalization
 *
 * Called when TP2/3/4 received
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_start_eq(struct rtk_dprx *dprx, u8 tp)
{
	/* Coverity: NULL check */
	if (!dprx)
		return -EINVAL;

	if (tp < 2 || tp > 4) {
		dev_err(dprx->dev, "PHY: Invalid TP %d for EQ\n", tp);
		return LT_ERR_INVALID_TP;
	}

	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_EQ_CRC_1,
		DPRX14_MAC_IP_EQ_CRC_1_eq_crc_en(1) |
		DPRX14_MAC_IP_EQ_CRC_1_eq_crc_sel(1));

	trace_printk("PHY: Start EQ (TP%d) detect\n", tp);

	dev_dbg(dprx->dev, "PHY: Start EQ (TP%d)\n", tp);

	return 0;
}

/**
 * stub_stop_eq - Stop Channel Equalization
 *
 * Disable EQ CRC checking by clearing EQ_CRC_1 register.
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_stop_eq(struct rtk_dprx *dprx)
{
	/* Coverity: NULL check */
	if (!dprx)
		return -EINVAL;

	if (!dprx->rbus_ops || !dprx->rbus_ops->set_byte)
		return LT_ERR_PHY_EQ_STATUS_READ;

	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_EQ_CRC_1, 0);

	trace_printk("PHY: Stop EQ detect\n");

	dev_dbg(dprx->dev, "PHY: Stop EQ\n");

	return 0;
}

/**
 * stub_check_eq_done - Check EQ completion status
 *
 * Read EQ CRC correct status from EQ_CRC_1 (0x98166048) Bit[3:0].
 * Bit[3:0] report physical hardware lane order; remap to logical DP lanes
 * via phy_lane_to_dp_mask() to match typec_flipped orientation.
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_check_eq_done(struct rtk_dprx *dprx, u8 lane_mask, u8 *done_mask)
{
	struct rtk_dprx_lt_context *lt;
	u8 eq_crc_raw, eq_crc;

	if (!dprx || !done_mask)
		return LT_ERR_NULL_POINTER;

	if (!dprx->rbus_ops || !dprx->rbus_ops->get_byte) {
		*done_mask = 0;
		return LT_ERR_PHY_EQ_STATUS_READ;
	}

	lt = &dprx->lt_ctx;

	/* EQ Done from EQ_CRC_1 (0x98166048) Bit[3:0] (physical lane order) */
	eq_crc_raw = dprx->rbus_ops->get_byte(DPRX14_MAC_IP_EQ_CRC_1);
	eq_crc = phy_lane_to_dp_mask(eq_crc_raw & 0x0F, lt->typec_flipped);
	*done_mask = eq_crc & lane_mask;

	/* Diagnostic trace: show raw register value for debugging */
	trace_printk("EQ_CRC_1=0x%02x mask=0x%02x eq_crc=0x%02x done=0x%02x\n",
		     eq_crc_raw, lane_mask, eq_crc, *done_mask);

	dev_dbg(dprx->dev, "PHY: EQ done check mask=0x%02x result=0x%02x\n",
		lane_mask, *done_mask);

	return 0;
}

/*============================================================================
 * [Required] Adjust Request
 *============================================================================*/

/**
 * stub_get_adjust_request - Get PHY recommended VS/PE adjustment
 *
 * Read current VS/PE directly from DPCD 0x103-0x106 to get the latest
 * values set by TX. Then recommend adjustment based on CR status:
 * - If CR not done: increase VS first, then PE if VS is maxed
 * - If CR done: maintain current settings
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_get_adjust_request(struct rtk_dprx *dprx, u8 lane,
				   u8 *vs_level, u8 *pe_level)
{
	struct rtk_dprx_lt_context *lt;
	u8 current_vs, current_pe;

	if (!dprx || !vs_level || !pe_level)
		return LT_ERR_NULL_POINTER;

	if (lane >= RTK_DPRX_MAX_LANES)
		return LT_ERR_INVALID_LANE_COUNT;

	lt = &dprx->lt_ctx;

	/*
	 * Use software cached values from lane_setting_handler.
	 * The handler parses DPCD 0x103-0x106 on each lane_setting IRQ and
	 * updates lt->lane[i].voltage_swing/pre_emphasis with the latest
	 * TX settings.
	 *
	 * Do NOT use get_dpcd_info() here - it reads hardware DPCD cache
	 * which may contain stale values, causing PE to stop incrementing
	 * during EQ phase. The lane_setting_handler is guaranteed to run
	 * before this function is called from the workqueue.
	 */
	current_vs = lt->lane[lane].voltage_swing;
	current_pe = lt->lane[lane].pre_emphasis;

	/* Diagnostic trace: show input values for debugging */
	trace_printk("ADJ_REQ[%d]: state=%d eq_done=%d cur_vs=%d cur_pe=%d\n",
		     lane, lt->state, lt->lane[lane].eq_done,
		     current_vs, current_pe);

	/*
	 * Recommend adjustment based on training state:
	 * Use lt->state instead of cr_done/eq_done to handle the case where
	 * cr_done is set but state hasn't transitioned to EQ yet.
	 *
	 * 1. CR state: increase VS (primary), then PE if VS maxed
	 * 2. EQ state with EQ not done: increase PE
	 * 3. Otherwise: maintain current settings
	 */
	if (lt->state == LT_STATE_CR_TRAINING || lt->state == LT_STATE_IDLE) {
		/* CR phase: increase VS first, then PE if VS maxed */
		if (current_vs < RTK_DPRX_VS_LEVEL_MAX) {
			*vs_level = current_vs + 1;
			*pe_level = current_pe;
		} else if (current_pe < RTK_DPRX_PE_LEVEL_MAX) {
			/* VS maxed out, try increasing PE */
			*vs_level = current_vs;
			*pe_level = current_pe + 1;
		} else {
			/* Both VS and PE maxed, maintain current */
			*vs_level = current_vs;
			*pe_level = current_pe;
		}
	} else if (lt->state == LT_STATE_EQ_TRAINING) {
		/*
		 * EQ phase: try increasing PE for all lanes.
		 *
		 * Do NOT check per-lane eq_done here. The EQ phase succeeds only
		 * when ALL conditions are met: CR_DONE + EQ_DONE + SYMBOL_LOCKED
		 * for all lanes, AND INTERLANE_ALIGN_DONE=1.
		 *
		 * Individual lanes may pass CRC check (eq_done=1) before overall
		 * alignment is achieved. We must continue adjusting PE for all
		 * lanes until the entire EQ phase succeeds.
		 *
		 * Pre-emphasis helps compensate for ISI (Inter-Symbol
		 * Interference) which is the key issue during channel
		 * equalization.
		 */
		if (current_pe < RTK_DPRX_PE_LEVEL_MAX) {
			*vs_level = current_vs;
			*pe_level = current_pe + 1;
		} else {
			/* PE maxed, maintain current */
			*vs_level = current_vs;
			*pe_level = current_pe;
		}
	} else {
		/* Other states: maintain current */
		*vs_level = current_vs;
		*pe_level = current_pe;
	}

	/* Clamp to valid range */
	*vs_level = min_t(u8, *vs_level, RTK_DPRX_VS_LEVEL_MAX);
	*pe_level = min_t(u8, *pe_level, RTK_DPRX_PE_LEVEL_MAX);

	/* Ensure VS + PE doesn't exceed maximum per DP spec */
	if (*vs_level + *pe_level > 3)
		*pe_level = 3 - *vs_level;

	/* Diagnostic trace: show output values for debugging */
	trace_printk("ADJ_REQ[%d]: output vs=%d pe=%d\n",
		     lane, *vs_level, *pe_level);

	dev_dbg(dprx->dev, "PHY: Lane%d adjust request VS=%d PE=%d (current VS=%d PE=%d)\n",
		lane, *vs_level, *pe_level, current_vs, current_pe);

	return 0;
}

/*============================================================================
 * [Optional] Advanced Features
 *
 * Optional operations return void or LT_ERR_xxx on failure.
 *============================================================================*/

/**
 * stub_set_lane_polarity - Set lane polarity inversion
 */
static void stub_set_lane_polarity(struct rtk_dprx *dprx, u8 lane, bool invert)
{
	if (!dprx)
		return;

	if (lane >= RTK_DPRX_MAX_LANES)
		return;

	/* TODO: Implement lane polarity setting */
	dev_dbg(dprx->dev, "PHY: Lane%d polarity %s\n",
		lane, invert ? "inverted" : "normal");
}

/**
 * stub_set_ssc - Set Spread Spectrum Clocking
 */
static void stub_set_ssc(struct rtk_dprx *dprx, bool enable)
{
	if (!dprx)
		return;

	/* TODO: Implement SSC control */
	dev_dbg(dprx->dev, "PHY: SSC %s\n", enable ? "enabled" : "disabled");
}

/**
 * stub_get_error_count - Get error count
 *
 * Return: 0 on success, LT_ERR_xxx on failure
 */
static int stub_get_error_count(struct rtk_dprx *dprx, u8 lane, u32 *error_count)
{
	if (!dprx || !error_count)
		return LT_ERR_NULL_POINTER;

	if (lane >= RTK_DPRX_MAX_LANES)
		return LT_ERR_INVALID_LANE_COUNT;

	/* TODO: Implement error count reading */
	*error_count = 0;

	return 0;
}

/*============================================================================
 * PHY Ops Structure
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_ops_default - Default implementation of LT PHY ops
 */
const struct rtk_dprx_lt_phy_ops rtk_dprx_lt_phy_ops_default = {
	/* Required operations */
	.configure_link		= stub_configure_link,
	.phy_reset		= stub_phy_reset,
	.get_phy_status		= stub_get_phy_status,
	.start_cr		= stub_start_cr,
	.check_cdr_lock		= stub_check_cdr_lock,
	.start_eq		= stub_start_eq,
	.stop_eq		= stub_stop_eq,
	.check_eq_done		= stub_check_eq_done,
	.get_adjust_request	= stub_get_adjust_request,

	/* Optional operations */
	.set_lane_polarity	= stub_set_lane_polarity,
	.set_ssc		= stub_set_ssc,
	.get_error_count	= stub_get_error_count,
};

/*============================================================================
 * Registration Helper
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_ops_register - Register default PHY ops
 * @dprx: DPRX instance
 *
 * Register PHY ops to Link Training context.
 * Call after rtk_dprx_link_training_init().
 *
 * Return: 0 on success
 */
int rtk_dprx_lt_phy_ops_register(struct rtk_dprx *dprx)
{
	int ret;

	if (!dprx)
		return -EINVAL;

	/* Validate ops */
	ret = rtk_dprx_lt_phy_ops_validate(&rtk_dprx_lt_phy_ops_default);
	if (ret) {
		dev_err(dprx->dev, "LT PHY ops validation failed\n");
		return ret;
	}

	/* Register */
	dprx->lt_ctx.lt_phy_ops = &rtk_dprx_lt_phy_ops_default;

	dev_info(dprx->dev, "LT PHY ops registered\n");

	return 0;
}
