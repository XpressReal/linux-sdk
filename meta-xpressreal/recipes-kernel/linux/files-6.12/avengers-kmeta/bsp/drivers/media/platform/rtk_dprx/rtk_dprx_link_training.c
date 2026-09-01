// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek DisplayPort RX Link Training
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/minmax.h>

#include "rtk_dprx.h"
#include "rtk_dprx_link_training.h"
#include "rtk_dprx_lt_phy_ops.h"
#include "rtk_dprx_lt_extcon.h"
#include "rtk_dprx_lt_sysfs.h"
#include "rtk_dprx_lt_v4l2.h"

#include <trace/events/rtk_dprx_trace.h>

/*============================================================================
 * DPCD Address Definitions
 *============================================================================
 */

/* Link Configuration (0x100 - 0x108) */
#define DPCD_LINK_BW_SET              0x00100
#define DPCD_LANE_COUNT_SET           0x00101
#define DPCD_TRAINING_PATTERN_SET     0x00102
#define DPCD_TRAINING_LANE0_SET       0x00103
#define DPCD_TRAINING_LANE1_SET       0x00104
#define DPCD_TRAINING_LANE2_SET       0x00105
#define DPCD_TRAINING_LANE3_SET       0x00106

/* Link/Sink Status (0x200 - 0x208) */
#define DPCD_SINK_COUNT               0x00200
#define DPCD_DEVICE_SERVICE_IRQ       0x00201
#define DPCD_LANE0_1_STATUS           0x00202
#define DPCD_LANE2_3_STATUS           0x00203
#define DPCD_LANE_ALIGN_STATUS        0x00204
#define DPCD_SINK_STATUS              0x00205
#define DPCD_ADJUST_REQUEST_LANE0_1   0x00206
#define DPCD_ADJUST_REQUEST_LANE2_3   0x00207

/*============================================================================
 * DPCD Access Wrapper Functions
 *============================================================================
 */

/**
 * rtk_dprx_lt_read_dpcd - Read DPCD register
 * @dprx: DPRX instance
 * @addr: 20-bit DPCD address
 *
 * Return: DPCD value, or 0 on error
 */
static u8 rtk_dprx_lt_read_dpcd(struct rtk_dprx *dprx, u32 addr)
{
	u8 port_h, port_m, port_l;

	/* Coverity: NULL pointer check */
	if (!dprx || !dprx->aux_ops || !dprx->aux_ops->get_dpcd_info) {
		pr_warn_ratelimited("rtk_dprx_lt: read_dpcd invalid params\n");
		return 0;
	}

	port_h = (addr >> 16) & 0x0F;
	port_m = (addr >> 8) & 0xFF;
	port_l = addr & 0xFF;

	return dprx->aux_ops->get_dpcd_info(dprx, port_h, port_m, port_l);
}

/**
 * rtk_dprx_lt_write_dpcd - Write DPCD register
 * @dprx: DPRX instance
 * @addr: 20-bit DPCD address
 * @value: Value to write
 *
 * Return: 0 on success, negative on error
 */
static int rtk_dprx_lt_write_dpcd(struct rtk_dprx *dprx, u32 addr, u8 value)
{
	u8 port_h, port_m, port_l;

	/* Coverity: NULL pointer check */
	if (!dprx || !dprx->aux_ops || !dprx->aux_ops->set_dpcd_value) {
		pr_warn_ratelimited("rtk_dprx_lt: write_dpcd invalid params\n");
		return -EINVAL;
	}

	port_h = (addr >> 16) & 0x0F;
	port_m = (addr >> 8) & 0xFF;
	port_l = addr & 0xFF;

	dprx->aux_ops->set_dpcd_value(dprx, port_h, port_m, port_l, value);
	return 0;
}

/*============================================================================
 * State/Event Name Strings (Debug)
 *============================================================================
 */

static const char * const lt_state_names[LT_STATE_COUNT] = {
	[LT_STATE_DISCONNECTED] = "DISCONNECTED",
	[LT_STATE_IDLE]         = "IDLE",
	[LT_STATE_CR_TRAINING]  = "CR_TRAINING",
	[LT_STATE_EQ_TRAINING]  = "EQ_TRAINING",
	[LT_STATE_TRAINED]      = "TRAINED",
	[LT_STATE_FAILED]       = "FAILED",
};

static const char * const lt_event_names[LT_EVENT_COUNT] = {
	[LT_EVENT_HPD_HIGH]       = "HPD_HIGH",
	[LT_EVENT_HPD_LOW]        = "HPD_LOW",
	[LT_EVENT_LINK_CONFIG]    = "LINK_CONFIG",
	[LT_EVENT_TP1_RECEIVED]   = "TP1_RECEIVED",
	[LT_EVENT_TP2_RECEIVED]   = "TP2_RECEIVED",
	[LT_EVENT_TP3_RECEIVED]   = "TP3_RECEIVED",
	[LT_EVENT_TP4_RECEIVED]   = "TP4_RECEIVED",
	[LT_EVENT_TP_END]         = "TP_END",
	[LT_EVENT_LANE_SETTING]   = "LANE_SETTING",
	[LT_EVENT_CR_DONE]        = "CR_DONE",
	[LT_EVENT_CR_FAILED]      = "CR_FAILED",
	[LT_EVENT_EQ_DONE]        = "EQ_DONE",
	[LT_EVENT_EQ_FAILED]      = "EQ_FAILED",
	[LT_EVENT_TIMEOUT]        = "TIMEOUT",
	[LT_EVENT_TRAINING_LOST]  = "TRAINING_LOST",
	[LT_EVENT_RESET]          = "RESET",
};

const char *rtk_dprx_lt_state_name(enum rtk_dprx_lt_state state)
{
	if (state < LT_STATE_COUNT)
		return lt_state_names[state];
	return "UNKNOWN";
}

const char *rtk_dprx_lt_event_name(enum rtk_dprx_lt_event event)
{
	if (event < LT_EVENT_COUNT)
		return lt_event_names[event];
	return "UNKNOWN";
}

/*============================================================================
 * State Transition Table
 *============================================================================
 */

static const int lt_next_state[LT_STATE_COUNT][LT_EVENT_COUNT] = {
	/* DISCONNECTED */
	[LT_STATE_DISCONNECTED] = {
		[LT_EVENT_HPD_HIGH]       = LT_STATE_IDLE,
		[LT_EVENT_HPD_LOW]        = LT_TRANS_IGNORE,
		[LT_EVENT_LINK_CONFIG]    = LT_TRANS_ERROR,
		[LT_EVENT_TP1_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP2_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP3_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP4_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP_END]         = LT_TRANS_ERROR,
		[LT_EVENT_LANE_SETTING]   = LT_TRANS_ERROR,
		[LT_EVENT_CR_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_CR_FAILED]      = LT_TRANS_IGNORE,
		[LT_EVENT_EQ_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_EQ_FAILED]      = LT_TRANS_IGNORE,
		[LT_EVENT_TIMEOUT]        = LT_TRANS_IGNORE,
		[LT_EVENT_TRAINING_LOST]  = LT_TRANS_IGNORE,
		[LT_EVENT_RESET]          = LT_TRANS_IGNORE,
	},

	/* IDLE */
	[LT_STATE_IDLE] = {
		[LT_EVENT_HPD_HIGH]       = LT_TRANS_IGNORE,
		[LT_EVENT_HPD_LOW]        = LT_STATE_DISCONNECTED,
		[LT_EVENT_LINK_CONFIG]    = LT_TRANS_STAY,
		[LT_EVENT_TP1_RECEIVED]   = LT_STATE_CR_TRAINING,
		[LT_EVENT_TP2_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP3_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP4_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP_END]         = LT_TRANS_IGNORE,
		[LT_EVENT_LANE_SETTING]   = LT_TRANS_IGNORE,
		[LT_EVENT_CR_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_CR_FAILED]      = LT_TRANS_IGNORE,
		[LT_EVENT_EQ_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_EQ_FAILED]      = LT_TRANS_IGNORE,
		[LT_EVENT_TIMEOUT]        = LT_TRANS_IGNORE,
		[LT_EVENT_TRAINING_LOST]  = LT_TRANS_IGNORE,
		[LT_EVENT_RESET]          = LT_TRANS_STAY,
	},

	/* CR_TRAINING */
	[LT_STATE_CR_TRAINING] = {
		[LT_EVENT_HPD_HIGH]       = LT_TRANS_IGNORE,
		[LT_EVENT_HPD_LOW]        = LT_STATE_DISCONNECTED,
		[LT_EVENT_LINK_CONFIG]    = LT_TRANS_STAY,
		[LT_EVENT_TP1_RECEIVED]   = LT_TRANS_STAY,
		[LT_EVENT_TP2_RECEIVED]   = LT_STATE_EQ_TRAINING,
		[LT_EVENT_TP3_RECEIVED]   = LT_STATE_EQ_TRAINING,
		[LT_EVENT_TP4_RECEIVED]   = LT_STATE_EQ_TRAINING,
		[LT_EVENT_TP_END]         = LT_STATE_FAILED,
		[LT_EVENT_LANE_SETTING]   = LT_TRANS_STAY,
		[LT_EVENT_CR_DONE]        = LT_TRANS_STAY,
		[LT_EVENT_CR_FAILED]      = LT_STATE_FAILED,
		[LT_EVENT_EQ_DONE]        = LT_TRANS_ERROR,
		[LT_EVENT_EQ_FAILED]      = LT_TRANS_ERROR,
		[LT_EVENT_TIMEOUT]        = LT_STATE_FAILED,
		[LT_EVENT_TRAINING_LOST]  = LT_STATE_FAILED,
		[LT_EVENT_RESET]          = LT_STATE_IDLE,
	},

	/* EQ_TRAINING */
	[LT_STATE_EQ_TRAINING] = {
		[LT_EVENT_HPD_HIGH]       = LT_TRANS_IGNORE,
		[LT_EVENT_HPD_LOW]        = LT_STATE_DISCONNECTED,
		[LT_EVENT_LINK_CONFIG]    = LT_TRANS_ERROR,
		[LT_EVENT_TP1_RECEIVED]   = LT_STATE_CR_TRAINING,
		[LT_EVENT_TP2_RECEIVED]   = LT_TRANS_STAY,
		[LT_EVENT_TP3_RECEIVED]   = LT_TRANS_STAY,
		[LT_EVENT_TP4_RECEIVED]   = LT_TRANS_STAY,
		[LT_EVENT_TP_END]         = LT_STATE_TRAINED,
		[LT_EVENT_LANE_SETTING]   = LT_TRANS_STAY,
		[LT_EVENT_CR_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_CR_FAILED]      = LT_STATE_FAILED,
		[LT_EVENT_EQ_DONE]        = LT_TRANS_STAY,
		[LT_EVENT_EQ_FAILED]      = LT_STATE_FAILED,
		[LT_EVENT_TIMEOUT]        = LT_STATE_FAILED,
		[LT_EVENT_TRAINING_LOST]  = LT_STATE_FAILED,
		[LT_EVENT_RESET]          = LT_STATE_IDLE,
	},

	/* TRAINED */
	[LT_STATE_TRAINED] = {
		[LT_EVENT_HPD_HIGH]       = LT_TRANS_IGNORE,
		[LT_EVENT_HPD_LOW]        = LT_STATE_DISCONNECTED,
		[LT_EVENT_LINK_CONFIG]    = LT_TRANS_STAY,
		[LT_EVENT_TP1_RECEIVED]   = LT_STATE_CR_TRAINING,
		[LT_EVENT_TP2_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP3_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP4_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP_END]         = LT_TRANS_IGNORE,
		[LT_EVENT_LANE_SETTING]   = LT_TRANS_IGNORE,
		[LT_EVENT_CR_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_CR_FAILED]      = LT_TRANS_IGNORE,
		[LT_EVENT_EQ_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_EQ_FAILED]      = LT_TRANS_IGNORE,
		[LT_EVENT_TIMEOUT]        = LT_TRANS_IGNORE,
		[LT_EVENT_TRAINING_LOST]  = LT_STATE_IDLE,
		[LT_EVENT_RESET]          = LT_STATE_IDLE,
	},

	/* FAILED */
	[LT_STATE_FAILED] = {
		[LT_EVENT_HPD_HIGH]       = LT_TRANS_IGNORE,
		[LT_EVENT_HPD_LOW]        = LT_STATE_DISCONNECTED,
		[LT_EVENT_LINK_CONFIG]    = LT_TRANS_STAY,
		[LT_EVENT_TP1_RECEIVED]   = LT_STATE_CR_TRAINING,
		[LT_EVENT_TP2_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP3_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP4_RECEIVED]   = LT_TRANS_ERROR,
		[LT_EVENT_TP_END]         = LT_TRANS_IGNORE,
		[LT_EVENT_LANE_SETTING]   = LT_TRANS_IGNORE,
		[LT_EVENT_CR_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_CR_FAILED]      = LT_TRANS_IGNORE,
		[LT_EVENT_EQ_DONE]        = LT_TRANS_IGNORE,
		[LT_EVENT_EQ_FAILED]      = LT_TRANS_IGNORE,
		[LT_EVENT_TIMEOUT]        = LT_TRANS_IGNORE,
		[LT_EVENT_TRAINING_LOST]  = LT_TRANS_IGNORE,
		[LT_EVENT_RESET]          = LT_STATE_IDLE,
	},
};

/*============================================================================
 * Link Rate Helpers
 *============================================================================
 */

static inline u32 rtk_dprx_link_rate_to_mbps(u8 link_rate)
{
	switch (link_rate) {
	case RTK_DPRX_LINK_RATE_RBR:
		return 1620;
	case RTK_DPRX_LINK_RATE_HBR:
		return 2700;
	case RTK_DPRX_LINK_RATE_HBR2:
		return 5400;
	case RTK_DPRX_LINK_RATE_HBR3:
		return 8100;
	default:
		return 0;
	}
}

static inline const char *rtk_dprx_link_rate_name(u8 link_rate)
{
	switch (link_rate) {
	case RTK_DPRX_LINK_RATE_RBR:
		return "RBR";
	case RTK_DPRX_LINK_RATE_HBR:
		return "HBR";
	case RTK_DPRX_LINK_RATE_HBR2:
		return "HBR2";
	case RTK_DPRX_LINK_RATE_HBR3:
		return "HBR3";
	default:
		return "Unknown";
	}
}

static inline bool rtk_dprx_is_valid_link_rate(u8 link_rate)
{
	switch (link_rate) {
	case RTK_DPRX_LINK_RATE_RBR:
	case RTK_DPRX_LINK_RATE_HBR:
	case RTK_DPRX_LINK_RATE_HBR2:
	case RTK_DPRX_LINK_RATE_HBR3:
		return true;
	default:
		return false;
	}
}

static inline bool rtk_dprx_is_valid_lane_count(u8 lane_count)
{
	return (lane_count == 1 || lane_count == 2 || lane_count == 4);
}

/*============================================================================
 * Lane Status Helpers
 *============================================================================
 */

/**
 * rtk_dprx_lt_get_valid_lane_count - Get validated lane count
 * @lt: Link Training context
 *
 * Return: Lane count clamped to valid range
 */
static inline u8 rtk_dprx_lt_get_valid_lane_count(
	const struct rtk_dprx_lt_context *lt)
{
	/* Coverity: Ensure lane count within array bounds */
	return min_t(u8, lt->requested_lane_count, RTK_DPRX_MAX_LANES);
}

static bool __maybe_unused
rtk_dprx_lt_all_lanes_cr_done(const struct rtk_dprx_lt_context *lt)
{
	u8 lane_count;
	int i;

	if (!lt)
		return false;

	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);
	for (i = 0; i < lane_count; i++) {
		if (!lt->lane[i].cr_done)
			return false;
	}
	return (lane_count > 0);
}

static bool __maybe_unused
rtk_dprx_lt_all_lanes_eq_done(const struct rtk_dprx_lt_context *lt)
{
	u8 lane_count;
	int i;

	if (!lt)
		return false;

	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);
	for (i = 0; i < lane_count; i++) {
		if (!lt->lane[i].eq_done)
			return false;
	}
	return (lane_count > 0);
}

/*============================================================================
 * Forward Declarations
 *============================================================================
 */

static void rtk_dprx_lt_timeout_func(struct work_struct *work);
static void rtk_dprx_lt_scan_work_func(struct work_struct *work);
static void rtk_dprx_lt_scan_work_cancel(struct rtk_dprx *dprx);
static void rtk_dprx_lt_fifo_check_work_start(struct rtk_dprx *dprx);
static void rtk_dprx_lt_fifo_check_work_cancel(struct rtk_dprx *dprx);
static void rtk_dprx_lt_hpd_toggle_reset(struct rtk_dprx *dprx);
static int rtk_dprx_lt_trigger_hpd_toggle(struct rtk_dprx *dprx);
static int rtk_dprx_lt_do_state_transition(struct rtk_dprx *dprx,
					   enum rtk_dprx_lt_event event);

/*============================================================================
 * DPCD Write IRQ Handlers (Phase 3.0)
 *============================================================================
 */

/**
 * rtk_dprx_lt_link_config_handler - Handle Link Config write
 * @dprx: DPRX instance
 *
 * Called when Source writes DPCD 0x100-0x101
 *
 * DPCD 0x00100 LINK_BW_SET: Link rate (0x06=RBR, 0x0A=HBR, 0x14=HBR2, 0x1E=HBR3)
 * DPCD 0x00101 LANE_COUNT_SET:
 *   Bits 4-0: Lane count (1, 2, or 4)
 *   Bit 7:    ENHANCED_FRAME_EN
 */
static void rtk_dprx_lt_link_config_handler(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 link_rate, lane_count_raw, lane_count;
	bool enhanced_frame_en;
	u8 old_rate, old_lanes;
	u32 v4l2_changes = 0;
	int i;

	/* Coverity: NULL pointer check */
	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	/* Save old values for change detection */
	old_rate = lt->requested_link_rate;
	old_lanes = lt->requested_lane_count;

	link_rate = rtk_dprx_lt_read_dpcd(dprx, DPCD_LINK_BW_SET);
	lane_count_raw = rtk_dprx_lt_read_dpcd(dprx, DPCD_LANE_COUNT_SET);

	/* Extract lane count (Bits 4-0) and Enhanced Frame (Bit 7) */
	lane_count = lane_count_raw & DPCD_LANE_COUNT_MASK;
	enhanced_frame_en = !!(lane_count_raw & DPCD_LANE_COUNT_ENHANCED_FRAME);
	lt->enhanced_frame_en = enhanced_frame_en;

	/* Coverity: Validate before storing */
	if (!rtk_dprx_is_valid_link_rate(link_rate)) {
		dev_warn(dprx->dev, "LT: Invalid link rate 0x%02x\n", link_rate);
		/* Store anyway for debug, but it will be rejected later */
	}

	if (!rtk_dprx_is_valid_lane_count(lane_count)) {
		dev_dbg(dprx->dev, "LT: Invalid lane count %d, keeping previous value %d\n",
			lane_count, lt->requested_lane_count);
		/* Keep previous valid lane_count, wait for next IRQ with valid value */
		lane_count = lt->requested_lane_count;
		/* If still invalid (first time), use default */
		if (!rtk_dprx_is_valid_lane_count(lane_count))
			lane_count = 4;
	}

	/*
	 * PHY configuration is deferred to training_pattern_handler when TPS1
	 * is received. This ensures PHY is configured only after the Source
	 * starts sending TPS1 signal, which is required for PHY to lock.
	 */

	/*
	 * Reset lane status for new Link Training.
	 * This must happen BEFORE TPS1 is written, so do it here when
	 * link_config is received. This ensures CR training sees clean
	 * initial state when TPS1 is received.
	 */
	for (i = 0; i < RTK_DPRX_MAX_LANES; i++) {
		lt->lane[i].voltage_swing = 0;
		lt->lane[i].pre_emphasis = 0;
		lt->lane[i].cdr_locked = 0;
	}
	lt->cr_first_lock = false;

	lt->requested_link_rate = link_rate;
	lt->requested_lane_count = lane_count;

	/* Detect changes for V4L2 notification (Phase 6.2) */
	if (old_rate != link_rate)
		v4l2_changes |= RTK_DPRX_LT_EVENT_RATE_CHANGE;
	if (old_lanes != lane_count)
		v4l2_changes |= RTK_DPRX_LT_EVENT_LANE_CHANGE;

	if (v4l2_changes)
		rtk_dprx_lt_v4l2_notify(dprx, v4l2_changes);

	/* Trace: Link Config received */
	trace_dprx_lt_link_config(lt->requested_link_rate,
				 lt->requested_lane_count,
				 lt->enhanced_frame_en);

	dev_dbg(dprx->dev, "LT: Link Config rate=0x%02x(%s) lanes=%d enhanced=%d\n",
		lt->requested_link_rate,
		rtk_dprx_link_rate_name(lt->requested_link_rate),
		lt->requested_lane_count,
		lt->enhanced_frame_en);
}

/**
 * rtk_dprx_lt_training_pattern_handler - Handle Training Pattern write
 * @dprx: DPRX instance
 *
 * Called when Source writes DPCD 0x102
 *
 * DPCD 0x00102 TRAINING_PATTERN_SET format:
 *   Bits 3-0: TRAINING_PATTERN_SET (0=None, 1=TPS1, 2=TPS2, 3=TPS3, 7=TPS4)
 *   Bit 5:    SCRAMBLING_DISABLE (0=enabled, 1=disabled)
 *   Bits 7-6: SYMBOL_ERROR_COUNT_SEL
 *
 * Return: Event to send to state machine
 */
static enum rtk_dprx_lt_event rtk_dprx_lt_training_pattern_handler(
	struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 tp_set;
	u8 tp_select;
	bool scrambling_disabled;
	enum rtk_dprx_lt_event event;

	/* Coverity: NULL pointer check */
	if (!dprx)
		return LT_EVENT_TP_END;

	lt = &dprx->lt_ctx;

	tp_set = rtk_dprx_lt_read_dpcd(dprx, DPCD_TRAINING_PATTERN_SET);

	/* Extract Training Pattern (Bits 3-0) - use 0x0F mask */
	tp_select = tp_set & DPCD_TP_SET_PATTERN_MASK;

	/* Extract Scrambling Disable (Bit 5) */
	scrambling_disabled = !!(tp_set & DPCD_TP_SET_SCRAMBLING_DISABLE);
	lt->scrambling_disabled = scrambling_disabled;

	/* Correct TPS detection using Bits 3-0 values */
	switch (tp_select) {
	case 0x00:
		lt->current_tp = RTK_DPRX_TP_NONE;
		event = LT_EVENT_TP_END;
		break;
	case 0x01:
		lt->current_tp = RTK_DPRX_TP_1;

		/*
		 * Configure PHY when TPS1 received. This ensures PHY is
		 * configured only after the Source starts sending TPS1 signal,
		 * which is required for PHY CDR to lock properly.
		 */
		if (!lt->phy_link_configured) {
			const struct rtk_dprx_lt_phy_ops *ops = lt->lt_phy_ops;

			if (ops && ops->configure_link) {
				ktime_t start_time = ktime_get();
				int ret;
				u32 elapsed_us;

				/*
				 * Enable AUX DEFER mode before PHY configuration.
				 * This makes Source wait when reading DPCD 0x202/0x203
				 * while PHY is being configured.
				 */
				if (dprx->aux_ops && dprx->aux_ops->set_manual_mode)
					dprx->aux_ops->set_manual_mode(dprx);

				/* Trace: PHY configure start */
				trace_dprx_lt_phy_cfg(true, lt->requested_link_rate,
						      lt->requested_lane_count, 0, 0);

				ret = ops->configure_link(dprx, lt->requested_link_rate,
							  lt->requested_lane_count);

				/* Trace: PHY configure end with timing */
				elapsed_us = (u32)ktime_us_delta(ktime_get(), start_time);
				trace_dprx_lt_phy_cfg(false, lt->requested_link_rate,
						      lt->requested_lane_count, elapsed_us, ret);

				/*
				 * Restore AUX to AUTO mode after PHY configuration.
				 * Now PHY is ready to respond to Source's DPCD reads.
				 */
				if (dprx->aux_ops && dprx->aux_ops->set_auto_mode)
					dprx->aux_ops->set_auto_mode(dprx);

				if (ret) {
					dev_err(dprx->dev, "LT: PHY configure failed: %d\n", ret);
					/* Continue to CR_TRAINING; CDR lock will naturally fail */
				} else {
					lt->phy_link_configured = true;
				}
			}
		}

		event = LT_EVENT_TP1_RECEIVED;
		break;
	case 0x02:
		lt->current_tp = RTK_DPRX_TP_2;
		event = LT_EVENT_TP2_RECEIVED;
		break;
	case 0x03:
		lt->current_tp = RTK_DPRX_TP_3;
		event = LT_EVENT_TP3_RECEIVED;
		break;
	case 0x07:  /* TPS4 = 0111b (DP 1.4a) */
		lt->current_tp = RTK_DPRX_TP_4;
		event = LT_EVENT_TP4_RECEIVED;
		break;
	default:
		/* Unknown pattern - treat as end */
		dev_warn(dprx->dev, "LT: Unknown Training Pattern 0x%x\n",
			 tp_select);
		lt->current_tp = RTK_DPRX_TP_NONE;
		event = LT_EVENT_TP_END;
		break;
	}

	/* Trace: Training Pattern change */
	trace_dprx_lt_tp_set(tp_set, tp_select, scrambling_disabled);

	dev_dbg(dprx->dev, "LT: TP%d (raw=0x%02x, scramble=%s)\n",
		lt->current_tp, tp_set,
		scrambling_disabled ? "OFF" : "ON");

	return event;
}

/**
 * rtk_dprx_lt_lane_setting_handler - Handle Lane Setting write
 * @dprx: DPRX instance
 *
 * Called when Source writes DPCD 0x103-0x108.
 * Parse TX's VS/PE settings and store for reference.
 */
static void rtk_dprx_lt_lane_setting_handler(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 lane_count;
	int i;
	u8 lane_set;

	/* Coverity: NULL pointer check */
	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	/* Coverity: Ensure lane count within array bounds */
	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);

	for (i = 0; i < lane_count; i++) {
		lane_set = rtk_dprx_lt_read_dpcd(dprx,
						 DPCD_TRAINING_LANE0_SET + i);

		/* Parse TX's current VS/PE (for reference) */
		lt->lane[i].voltage_swing = lane_set & 0x03;
		lt->lane[i].pre_emphasis = (lane_set >> 3) & 0x03;
		lt->lane[i].max_vs_reached = (lane_set >> 2) & 0x01;
		lt->lane[i].max_pe_reached = (lane_set >> 5) & 0x01;

		dev_dbg(dprx->dev, "LT: Lane%d TX VS=%d PE=%d\n",
			i, lt->lane[i].voltage_swing,
			lt->lane[i].pre_emphasis);
	}

	/*
	 * Note: RX doesn't "set" VS/PE - that's TX's job.
	 * RX's job is to get PHY's recommendation via get_adjust_request(),
	 * then write to DPCD 0x206-0x207 for TX to read.
	 * This is implemented in CR/EQ state handlers.
	 */
}

/**
 * rtk_dprx_lt_dpcd_irq_handler - Main DPCD IRQ handler
 * @dprx: DPRX instance
 * @irq_status: PB7_DD_AUX_DPCD_IRQ status
 *
 * Called from main ISR when DPCD write interrupt occurs
 */
void rtk_dprx_lt_dpcd_irq_handler(struct rtk_dprx *dprx, u8 irq_status)
{
	struct rtk_dprx_lt_context *lt;
	enum rtk_dprx_lt_event event;

	/* Coverity: NULL pointer check */
	if (!dprx)
		return;

	/* Trace: DPCD IRQ in threaded context */
	trace_dprx_lt_dpcd_irq(irq_status, false);

	lt = &dprx->lt_ctx;

	/*
	 * Auto-recovery: If DPCD events arrive while in DISCONNECTED state,
	 * it indicates HPD_HIGH was missed. Inject HPD_HIGH first to
	 * transition to IDLE, then process the DPCD events normally.
	 *
	 * This can happen when:
	 * 1. Extcon notification is delayed or lost
	 * 2. Source sends DPCD writes faster than HPD propagates
	 * 3. Driver initialized after source already connected
	 */
	if (lt->state == LT_STATE_DISCONNECTED) {
		dev_info(dprx->dev,
			 "LT: Auto-recovery - DPCD event in DISCONNECTED, injecting HPD_HIGH\n");
		rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_HPD_HIGH);
	}

	/* Bit[6]: wr100_101_int - Link Config */
	if (irq_status & BIT(6)) {
		rtk_dprx_lt_link_config_handler(dprx);
		rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_LINK_CONFIG);
	}

	/* Bit[7]: wr102_int - Training Pattern */
	if (irq_status & BIT(7)) {
		event = rtk_dprx_lt_training_pattern_handler(dprx);
		rtk_dprx_lt_do_state_transition(dprx, event);
	}

	/* Bit[5]: wr103_108_int - Lane Setting */
	if (irq_status & BIT(5)) {
		rtk_dprx_lt_lane_setting_handler(dprx);
		rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_LANE_SETTING);
	}
}

/*============================================================================
 * Phase 3: Clock Recovery (CR) Training Implementation
 *============================================================================
 */

/**
 * rtk_dprx_lt_update_adjust_request - Update DPCD Adjust Request
 * @dprx: DPRX instance
 *
 * Read PHY's recommended VS/PE, write to DPCD 0x206-0x207 for TX to read.
 *
 * Return: 0 on success, negative on error
 */
static int rtk_dprx_lt_update_adjust_request(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	const struct rtk_dprx_lt_phy_ops *ops;
	u8 adj_lane01 = 0;
	u8 adj_lane23 = 0;
	u8 vs, pe;
	u8 lane_count;
	int i, ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;
	ops = lt->lt_phy_ops;

	if (!ops || !ops->get_adjust_request)
		return -ENODEV;

	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);

	for (i = 0; i < lane_count; i++) {
		ret = ops->get_adjust_request(dprx, i, &vs, &pe);
		if (ret) {
			dev_err(dprx->dev, "LT: Failed to get adjust request for lane %d\n", i);
			return ret;
		}

		/*
		 * DPCD 0x206 (Lane 0/1) format:
		 * Bits [1:0]: Lane 0 VS
		 * Bits [3:2]: Lane 0 PE
		 * Bits [5:4]: Lane 1 VS
		 * Bits [7:6]: Lane 1 PE
		 *
		 * DPCD 0x207 (Lane 2/3) same format
		 */
		if (i < 2)
			adj_lane01 |= ((vs & 0x03) | ((pe & 0x03) << 2)) << (i * 4);
		else
			adj_lane23 |= ((vs & 0x03) | ((pe & 0x03) << 2)) << ((i - 2) * 4);

		dev_dbg(dprx->dev, "LT: Lane%d adjust request VS=%d PE=%d\n",
			i, vs, pe);
	}

	/* Diagnostic trace: show DPCD values before write */
	trace_dprx_lt_adjust_req(adj_lane01, adj_lane23, lane_count);

	/* Write DPCD 0x206 */
	ret = rtk_dprx_lt_write_dpcd(dprx, DPCD_ADJUST_REQUEST_LANE0_1, adj_lane01);
	if (ret)
		return ret;

	/* Write DPCD 0x207 if 4 lanes */
	if (lane_count > 2) {
		ret = rtk_dprx_lt_write_dpcd(dprx, DPCD_ADJUST_REQUEST_LANE2_3, adj_lane23);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * rtk_dprx_lt_update_lane_status - Update DPCD Lane Status
 * @dprx: DPRX instance
 *
 * Read PHY lane status, write to DPCD 0x202-0x204 for TX to read.
 *
 * CR_DONE determination:
 *   - CR phase: from PHY check_cdr_lock (TP1 detect)
 *   - EQ phase: maintain as true (CR already achieved)
 *
 * DPCD 0x00204 LANE_ALIGN_STATUS_UPDATED format:
 *   Bit 0: INTERLANE_ALIGN_DONE
 *   Bit 7: LINK_STATUS_UPDATED (must be set after any status update)
 *
 * Return: 0 on success, negative on error
 */
static int rtk_dprx_lt_update_lane_status(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	const struct rtk_dprx_lt_phy_ops *ops;
	u8 lane_count;
	u8 lane_mask;
	u8 cr_mask = 0;
	u8 eq_mask = 0;
	u8 lane01_status = 0;
	u8 lane23_status = 0;
	u8 lane_align;
	int i, ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;
	ops = lt->lt_phy_ops;

	if (!ops)
		return -ENODEV;

	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);
	lane_mask = (1 << lane_count) - 1;

	/*
	 * CR_DONE: Determine source based on training phase
	 * - EQ phase: Keep CR as true (already achieved in CR phase)
	 * - CR phase: Get from PHY TP1 detect, with deferral logic
	 */
	if (lt->state == LT_STATE_EQ_TRAINING) {
		cr_mask = lane_mask;
	} else {
		/* CR phase: apply deferral logic */
		u8 cdr_locked_mask = 0;

		if (ops->check_cdr_lock)
			ops->check_cdr_lock(dprx, lane_mask, &cdr_locked_mask);

		/*
		 * CR_DONE deferral: On first CDR lock, don't report CR_DONE.
		 * This allows Source to optimize drive settings.
		 * Exception: Skip deferral if VS is already at max.
		 */
		for (i = 0; i < lane_count; i++) {
			if (cdr_locked_mask & BIT(i)) {
				if (!lt->lane[i].cdr_locked) {
					/* First lock */
					lt->lane[i].cdr_locked = 1;

					/* Skip deferral if VS is already at max */
					if (lt->lane[i].voltage_swing >= RTK_DPRX_VS_LEVEL_MAX)
						cr_mask |= BIT(i);
					else
						lt->cr_first_lock = true;
				} else {
					/* Not first lock: set CR_DONE */
					cr_mask |= BIT(i);
				}
			}
		}
	}

	/* EQ_DONE: Get from PHY */
	if (ops->check_eq_done)
		ops->check_eq_done(dprx, lane_mask, &eq_mask);

	/*
	 * Build DPCD 0x202-0x203
	 * DPCD 0x202 (Lane 0/1 Status) format:
	 *   Bit 0: Lane 0 CR Done
	 *   Bit 1: Lane 0 Channel EQ Done
	 *   Bit 2: Lane 0 Symbol Locked
	 *   Bit 4: Lane 1 CR Done
	 *   Bit 5: Lane 1 Channel EQ Done
	 *   Bit 6: Lane 1 Symbol Locked
	 */
	for (i = 0; i < lane_count; i++) {
		u8 status = 0;

		if (cr_mask & BIT(i))
			status |= BIT(0);
		if (eq_mask & BIT(i))
			status |= BIT(1);
		/* Symbol locked follows eq_done */
		if (eq_mask & BIT(i))
			status |= BIT(2);

		if (i < 2)
			lane01_status |= (status << (i * 4));
		else
			lane23_status |= (status << ((i - 2) * 4));

		/* Update context */
		lt->lane[i].cr_done = !!(cr_mask & BIT(i));
		lt->lane[i].eq_done = !!(eq_mask & BIT(i));
		lt->lane[i].symbol_locked = lt->lane[i].eq_done;
	}

	/*
	 * DPCD 0x204 Lane Align Status Updated
	 * Bit 7: LINK_STATUS_UPDATED - always set after status update
	 * Bit 0: INTERLANE_ALIGN_DONE - set when all lanes aligned
	 */
	lane_align = BIT(7);
	if (cr_mask == lane_mask && eq_mask == lane_mask)
		lane_align |= BIT(0);

	dev_dbg(dprx->dev, "LT: status cr=0x%02x eq=0x%02x\n",
		cr_mask, eq_mask);

	/* Write DPCD 0x202 */
	ret = rtk_dprx_lt_write_dpcd(dprx, DPCD_LANE0_1_STATUS, lane01_status);
	if (ret)
		return ret;

	/* Write DPCD 0x203 if 4 lanes */
	if (lane_count > 2) {
		ret = rtk_dprx_lt_write_dpcd(dprx, DPCD_LANE2_3_STATUS, lane23_status);
		if (ret)
			return ret;
	}

	/* Write DPCD 0x204 */
	ret = rtk_dprx_lt_write_dpcd(dprx, DPCD_LANE_ALIGN_STATUS, lane_align);
	if (ret)
		return ret;

	return 0;
}

/**
 * rtk_dprx_lt_check_cr_done - Check if all lanes have CR done
 * @dprx: DPRX instance
 *
 * Return: true if all lanes CR done, false otherwise
 */
static bool rtk_dprx_lt_check_cr_done(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 lane_count;
	int i;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);

	for (i = 0; i < lane_count; i++) {
		if (!lt->lane[i].cr_done)
			return false;
	}

	return true;
}

/**
 * rtk_dprx_lt_check_same_vs - Check if VS is same as last iteration
 * @dprx: DPRX instance
 *
 * Return: true if all lanes have same VS as last time
 */
static bool rtk_dprx_lt_check_same_vs(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 lane_count;
	int i;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);

	for (i = 0; i < lane_count; i++) {
		if (lt->lane[i].voltage_swing != lt->last_vs[i])
			return false;
	}

	return true;
}

/**
 * rtk_dprx_lt_save_vs - Save current VS for next iteration comparison
 * @dprx: DPRX instance
 */
static void rtk_dprx_lt_save_vs(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 lane_count;
	int i;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;
	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);

	for (i = 0; i < lane_count; i++)
		lt->last_vs[i] = lt->lane[i].voltage_swing;
}

/**
 * rtk_dprx_lt_do_cr_training - Execute CR training state
 * @dprx: DPRX instance
 *
 * Called from workqueue when in LT_STATE_CR_TRAINING.
 * Implements the CR training loop as per DP spec.
 */
static void rtk_dprx_lt_do_cr_training(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	const struct rtk_dprx_lt_phy_ops *ops;
	u8 lane_mask;
	u8 locked_mask = 0;
	int ret;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;
	ops = lt->lt_phy_ops;

	dev_dbg(dprx->dev, "LT CR: retry=%d/%d same_vs=%d/%d\n",
		lt->cr_retry_count, lt->config.max_cr_retry,
		lt->same_vs_count, lt->config.max_same_vs_retry);

	/* First entry: configure PHY and start CR */
	if (lt->cr_retry_count == 0) {
#if 0 // TODO: This block maybe can be removed
		/* Configure PHY link parameters */
		if (ops && ops->configure_link) {
			ret = ops->configure_link(dprx, lt->requested_link_rate, lt->requested_lane_count);
			if (ret) {
				dev_err(dprx->dev, "LT CR: PHY configure failed: %d\n", ret);
				lt->error_code = LT_ERR_PHY_CONFIG_FAIL;
				lt->stats.cr_fail_count++;
				rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_CR_FAILED);
				return;
			}
		}
#endif

		/* Start CR (PHY receives TP1) */
		if (ops && ops->start_cr) {
			ret = ops->start_cr(dprx);
			if (ret) {
				dev_err(dprx->dev, "LT CR: PHY start_cr failed: %d\n", ret);
				lt->error_code = LT_ERR_PHY_CDR_LOCK_FAIL;
				lt->stats.cr_fail_count++;
				rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_CR_FAILED);
				return;
			}
		}

		/* Start timeout timer */
		schedule_delayed_work(&lt->timeout_work,
				      msecs_to_jiffies(lt->config.cr_timeout_ms));

		/* Record start time */
		lt->start_time = ktime_get();
	}

	/* Check CDR lock status */
	lane_mask = (1 << lt->requested_lane_count) - 1;

	if (ops && ops->check_cdr_lock) {
		ret = ops->check_cdr_lock(dprx, lane_mask, &locked_mask);
		if (ret) {
			dev_err(dprx->dev, "LT CR: check_cdr_lock failed: %d\n", ret);
			/* Continue anyway - might succeed on retry */
		}
	}

	/* Update lane status and write to DPCD */
	ret = rtk_dprx_lt_update_lane_status(dprx);
	if (ret)
		dev_err(dprx->dev, "LT CR: update_lane_status failed: %d\n", ret);

	/* Update adjust request and write to DPCD */
	ret = rtk_dprx_lt_update_adjust_request(dprx);
	if (ret)
		dev_err(dprx->dev, "LT CR: update_adjust_request failed: %d\n", ret);

	/* Check if CR done */
	if (rtk_dprx_lt_check_cr_done(dprx)) {
		dev_info(dprx->dev, "LT CR: All lanes CR done\n");
		cancel_delayed_work(&lt->timeout_work);
		rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_CR_DONE);
		return;
	}

	/* Check same VS retry limit (DP spec: 5 times same VS = fail) */
	if (rtk_dprx_lt_check_same_vs(dprx)) {
		lt->same_vs_count++;
		if (lt->same_vs_count >= lt->config.max_same_vs_retry) {
			dev_err(dprx->dev, "LT CR: Same VS retry limit reached\n");
			lt->error_code = LT_ERR_CR_SAME_VS_RETRY;
			lt->stats.cr_fail_count++;
			cancel_delayed_work(&lt->timeout_work);
			rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_CR_FAILED);
			return;
		}
	} else {
		lt->same_vs_count = 0;
	}

	/* Save VS for next iteration comparison */
	rtk_dprx_lt_save_vs(dprx);

	/* Check retry limit */
	lt->cr_retry_count++;

	/* Trace: CR retry status */
	trace_dprx_lt_cr_retry(lt->cr_retry_count, lt->same_vs_count,
			      lt->config.max_cr_retry);

	if (lt->cr_retry_count >= lt->config.max_cr_retry) {
		dev_err(dprx->dev, "LT CR: Retry limit reached\n");
		lt->error_code = LT_ERR_CR_MAX_RETRY;
		lt->stats.cr_fail_count++;
		cancel_delayed_work(&lt->timeout_work);
		rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_CR_FAILED);
		return;
	}

	/*
	 * CR not done yet - wait for next lane_setting write from TX.
	 * TX will read our DPCD 0x202-0x204 (lane status) and 0x206-0x207 (adjust request),
	 * then write new VS/PE to DPCD 0x103-0x106.
	 * The lane_setting_handler will trigger another round.
	 */
	dev_dbg(dprx->dev, "LT CR: Waiting for TX adjust (retry %d)\n",
		lt->cr_retry_count);
}

/*============================================================================
 * Phase 4: Channel Equalization (EQ) Training Implementation
 *============================================================================
 */

/**
 * rtk_dprx_lt_check_eq_done - Check if all lanes have EQ done
 * @dprx: DPRX instance
 *
 * Return: true if all lanes EQ done, false otherwise
 */
static bool rtk_dprx_lt_check_eq_done(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 lane_count;
	int i;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);

	for (i = 0; i < lane_count; i++) {
		if (!lt->lane[i].eq_done)
			return false;
	}

	return (lane_count > 0);
}

/**
 * rtk_dprx_lt_check_symbol_locked - Check if all lanes have symbol lock
 * @dprx: DPRX instance
 *
 * Return: true if all lanes symbol locked, false otherwise
 */
static bool rtk_dprx_lt_check_symbol_locked(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 lane_count;
	int i;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	lane_count = rtk_dprx_lt_get_valid_lane_count(lt);

	for (i = 0; i < lane_count; i++) {
		if (!lt->lane[i].symbol_locked)
			return false;
	}

	return (lane_count > 0);
}

/**
 * rtk_dprx_lt_check_lane_aligned - Check interlane alignment
 * @dprx: DPRX instance
 *
 * Checks DPCD 0x204 bit 0 for interlane alignment.
 * For single-lane configurations, alignment is always considered done.
 *
 * Return: true if lanes are aligned, false otherwise
 */
static bool rtk_dprx_lt_check_lane_aligned(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	u8 lane_align_status;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;

	/* Single lane doesn't need alignment */
	if (lt->requested_lane_count == 1)
		return true;

	/* Read DPCD 0x204 */
	lane_align_status = rtk_dprx_lt_read_dpcd(dprx, DPCD_LANE_ALIGN_STATUS);

	return !!(lane_align_status & BIT(0));
}

/**
 * rtk_dprx_lt_get_tp_name - Get training pattern name
 * @tp: Training pattern number
 *
 * Return: Human-readable pattern name
 */
static const char *rtk_dprx_lt_get_tp_name(u8 tp)
{
	static const char * const names[] = {
		[0] = "TP_NONE",
		[1] = "TP1",
		[2] = "TP2",
		[3] = "TP3",
		[4] = "TP4",
	};

	if (tp <= 4)
		return names[tp];
	return "TP_UNKNOWN";
}

/**
 * rtk_dprx_lt_do_eq_training - Execute EQ training state
 * @dprx: DPRX instance
 *
 * Called from workqueue when in LT_STATE_EQ_TRAINING.
 * Implements the Channel Equalization training loop as per DP spec.
 *
 * EQ Training flow (DP 1.4a spec):
 * 1. TX sends TP2/TP3/TP4
 * 2. RX attempts to achieve symbol lock and lane alignment
 * 3. RX updates DPCD 0x202-0x207 with status
 * 4. TX reads status and adjusts VS/PE
 * 5. Repeat until all lanes EQ done or max retry reached
 */
static void rtk_dprx_lt_do_eq_training(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	const struct rtk_dprx_lt_phy_ops *ops;
	u8 lane_mask;
	u8 done_mask = 0;
	int ret;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;
	ops = lt->lt_phy_ops;

	dev_dbg(dprx->dev, "LT EQ: retry=%d/%d tp=%s\n",
		lt->eq_retry_count, lt->config.max_eq_retry,
		rtk_dprx_lt_get_tp_name(lt->current_tp));

	/*
	 * First entry: start EQ training.
	 *
	 * Use eq_started flag instead of eq_retry_count to prevent race condition.
	 * If rtk_dprx_lt_do_eq_training() is called twice rapidly (e.g., state
	 * transition followed by LANE_SETTING event), both calls may see
	 * eq_retry_count == 0 and call start_eq() twice, which resets PHY EQ
	 * detection logic and breaks already-achieved ALIGNED status.
	 */
	if (!lt->eq_started) {
		/* PHY calibrate as EQ preparation (after CR_DONE) */
		ret = rtk_dprx_lt_phy_calibrate(dprx);
		if (ret)
			dev_warn(dprx->dev, "LT EQ: PHY calibrate failed: %d (non-fatal)\n", ret);

		/* Start EQ (PHY receives TP2/3/4) */
		if (ops && ops->start_eq) {
			ret = ops->start_eq(dprx, lt->current_tp);
			if (ret) {
				dev_err(dprx->dev, "LT EQ: PHY start_eq failed: %d\n", ret);
				lt->error_code = LT_ERR_IS_LT_ERROR(ret) ?
						 ret : LT_ERR_EQ_START_FAIL;
				lt->stats.eq_fail_count++;
				rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_EQ_FAILED);
				return;
			}
		}

		/* Mark EQ as started to prevent duplicate start_eq calls */
		lt->eq_started = true;

		/* Start timeout timer */
		schedule_delayed_work(&lt->timeout_work,
				      msecs_to_jiffies(lt->config.eq_timeout_ms));

		/* Record start time (if not already set by CR) */
		if (lt->start_time == 0)
			lt->start_time = ktime_get();
	}

	/* Calculate lane mask */
	lane_mask = (1 << lt->requested_lane_count) - 1;

	/* Check EQ done status via PHY ops */
	if (ops && ops->check_eq_done) {
		ret = ops->check_eq_done(dprx, lane_mask, &done_mask);
		if (ret) {
			dev_dbg(dprx->dev, "LT EQ: check_eq_done returned %d\n", ret);
			/* Continue anyway - might succeed on retry */
		}
	}

	/* Update lane status and write to DPCD */
	ret = rtk_dprx_lt_update_lane_status(dprx);
	if (ret)
		dev_err(dprx->dev, "LT EQ: update_lane_status failed: %d\n", ret);

	/* Trace EQ status (after DPCD update to get correct lane_aligned) */
	trace_dprx_lt_eq_status(lt->requested_lane_count, done_mask, done_mask,
				rtk_dprx_lt_check_lane_aligned(dprx));

	/*
	 * Update adjust request ONLY if EQ not complete yet.
	 *
	 * Per DP spec 3.5.1.2.2, Sink should maintain stable adjust request
	 * when EQ is achieved. Continuing to request PE increase after all
	 * lanes are EQ_DONE + SYMBOL_LOCKED + INTERLANE_ALIGNED can cause
	 * over-emphasis and degrade signal quality.
	 *
	 * Check all EQ completion conditions before updating adjust request:
	 * - All lanes CR_DONE (checked earlier, CR loss causes early return)
	 * - All lanes EQ_DONE
	 * - All lanes SYMBOL_LOCKED
	 * - INTERLANE_ALIGN_DONE
	 */
	if (!(rtk_dprx_lt_check_eq_done(dprx) &&
	      rtk_dprx_lt_check_symbol_locked(dprx) &&
	      rtk_dprx_lt_check_lane_aligned(dprx))) {
		/* EQ not complete - update adjust request to guide TX */
		ret = rtk_dprx_lt_update_adjust_request(dprx);
		if (ret)
			dev_err(dprx->dev, "LT EQ: update_adjust_request failed: %d\n", ret);
	} else {
		/*
		 * EQ complete - do NOT update adjust request.
		 * Maintain current VS/PE settings to avoid over-emphasis.
		 */
		dev_dbg(dprx->dev, "LT EQ: Complete, skip adjust request update\n");
	}

	/* Check CR still maintained (CR can be lost during EQ) */
	if (!rtk_dprx_lt_check_cr_done(dprx)) {
		dev_warn(dprx->dev, "LT EQ: CR lost during EQ training\n");
		lt->error_code = LT_ERR_CR_LOST;
		lt->stats.eq_fail_count++;
		cancel_delayed_work(&lt->timeout_work);
		rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_EQ_FAILED);
		return;
	}

	/* Check if EQ done for all lanes */
	if (rtk_dprx_lt_check_eq_done(dprx) &&
	    rtk_dprx_lt_check_symbol_locked(dprx) &&
	    rtk_dprx_lt_check_lane_aligned(dprx)) {
		dev_info(dprx->dev, "LT EQ: All lanes EQ done, symbol locked, aligned\n");
		cancel_delayed_work(&lt->timeout_work);
		rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_EQ_DONE);
		return;
	}

	/* Check retry limit (DP spec: 5 retries for EQ) */
	lt->eq_retry_count++;

	/* Trace EQ retry */
	trace_dprx_lt_eq_retry(lt->eq_retry_count, lt->config.max_eq_retry,
			       rtk_dprx_lt_check_eq_done(dprx),
			       rtk_dprx_lt_check_symbol_locked(dprx));

	if (lt->eq_retry_count >= lt->config.max_eq_retry) {
		dev_err(dprx->dev, "LT EQ: Retry limit reached\n");

		/* Set specific error code based on what failed */
		if (!rtk_dprx_lt_check_eq_done(dprx))
			lt->error_code = LT_ERR_EQ_MAX_RETRY;
		else if (!rtk_dprx_lt_check_symbol_locked(dprx))
			lt->error_code = LT_ERR_SYMBOL_LOCK_TIMEOUT;
		else
			lt->error_code = LT_ERR_LANE_ALIGN_TIMEOUT;

		lt->stats.eq_fail_count++;
		cancel_delayed_work(&lt->timeout_work);
		rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_EQ_FAILED);
		return;
	}

	/*
	 * EQ not done yet - wait for next lane_setting write from TX.
	 * TX will read our DPCD 0x202-0x204 and 0x206-0x207,
	 * then write new VS/PE to DPCD 0x103-0x106.
	 * The lane_setting_handler will trigger another round.
	 */
	dev_dbg(dprx->dev, "LT EQ: Waiting for TX adjust (retry %d) eq=0x%02x\n",
		lt->eq_retry_count, done_mask);
}

/**
 * rtk_dprx_lt_enter_eq_training - Prepare for EQ training
 * @dprx: DPRX instance
 * @tp: Training pattern (2, 3, or 4)
 *
 * Called when transitioning from CR_TRAINING to EQ_TRAINING.
 * Resets EQ counters and sets up for EQ phase.
 */
static void rtk_dprx_lt_enter_eq_training(struct rtk_dprx *dprx, u8 tp)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	/* Reset EQ counters */
	lt->eq_retry_count = 0;
	lt->eq_started = false;

	/* Store current training pattern */
	lt->current_tp = tp;

	dev_info(dprx->dev, "LT: Entering EQ training with %s\n",
		 rtk_dprx_lt_get_tp_name(tp));
}

/*============================================================================
 * State Machine
 *============================================================================
 */

/**
 * rtk_dprx_lt_do_state_transition - Process state transition
 * @dprx: DPRX instance
 * @event: Event to process
 *
 * Return: 0 on success, negative on error
 */
static int rtk_dprx_lt_do_state_transition(struct rtk_dprx *dprx,
					   enum rtk_dprx_lt_event event)
{
	struct rtk_dprx_lt_context *lt;
	enum rtk_dprx_lt_state old_state;
	int result;
	unsigned long flags;

	/* Coverity: NULL pointer check */
	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	/* Coverity: Validate event range before array access */
	if (event >= LT_EVENT_COUNT) {
		dev_warn(dprx->dev, "LT: Invalid event %d\n", event);
		return -EINVAL;
	}

	spin_lock_irqsave(&lt->state_lock, flags);

	old_state = lt->state;

	/* Coverity: Validate state range before array access */
	if (old_state >= LT_STATE_COUNT) {
		spin_unlock_irqrestore(&lt->state_lock, flags);
		dev_err(dprx->dev, "LT: Invalid state %d\n", old_state);
		return -EINVAL;
	}

	result = lt_next_state[old_state][event];

	if (LT_TRANS_IS_CHANGE(result)) {
		/* State change */
		lt->prev_state = old_state;
		lt->state = result;

		/* Trace: state transition */
		trace_dprx_lt_state(old_state, lt->state, event);

		dev_dbg(dprx->dev, "LT: %s + %s -> %s\n",
			rtk_dprx_lt_state_name(old_state),
			rtk_dprx_lt_event_name(event),
			rtk_dprx_lt_state_name(lt->state));

	} else if (LT_TRANS_IS_STAY(result)) {
		/* Stay in current state */
		dev_dbg(dprx->dev, "LT: %s + %s -> STAY\n",
			rtk_dprx_lt_state_name(old_state),
			rtk_dprx_lt_event_name(event));

	} else if (LT_TRANS_IS_ERROR(result)) {
		/* Unexpected event */
		lt->stats.error_event_count++;
		dev_warn(dprx->dev, "LT: Unexpected %s in state %s\n",
			 rtk_dprx_lt_event_name(event),
			 rtk_dprx_lt_state_name(old_state));
	}
	/* LT_TRANS_IGNORE: do nothing */

	spin_unlock_irqrestore(&lt->state_lock, flags);

	/* Handle state entry actions (outside spinlock for I/O operations) */
	if (LT_TRANS_IS_CHANGE(result)) {
		/* V4L2 notification (Phase 6.2) */
		rtk_dprx_lt_v4l2_notify_state_change(dprx, old_state, lt->state);

		/*
		 * Entry to DISCONNECTED: reset HPD toggle counter on genuine
		 * cable disconnect. Skip reset if HPD toggle is in progress
		 * (the disconnect was intentionally triggered by us).
		 */
		if (lt->state == LT_STATE_DISCONNECTED &&
		    !lt->hpd_toggle_in_progress) {
			lt->hpd_toggle_count = 0;
		}

		/* Exit from EQ_TRAINING: stop EQ */
		if (old_state == LT_STATE_EQ_TRAINING &&
		    lt->state != LT_STATE_EQ_TRAINING) {
			const struct rtk_dprx_lt_phy_ops *ops = lt->lt_phy_ops;

			if (ops && ops->stop_eq)
				ops->stop_eq(dprx);
		}

		/* Exit from TRAINED: cancel FIFO check, scan work and HPD toggle */
		if (old_state == LT_STATE_TRAINED &&
		    lt->state != LT_STATE_TRAINED) {
			rtk_dprx_lt_fifo_check_work_cancel(dprx);
			rtk_dprx_lt_scan_work_cancel(dprx);
			rtk_dprx_lt_hpd_toggle_reset(dprx);
		}

		/* Entry to CR_TRAINING: reset counters and flags */
		if (lt->state == LT_STATE_CR_TRAINING &&
		    old_state != LT_STATE_CR_TRAINING) {
			int j;

			/* Cancel any pending FIFO check, scan work and HPD toggle */
			rtk_dprx_lt_fifo_check_work_cancel(dprx);
			rtk_dprx_lt_scan_work_cancel(dprx);
			rtk_dprx_lt_hpd_toggle_reset(dprx);

			lt->cr_retry_count = 0;
			lt->same_vs_count = 0;

			/*
			 * Clear phy_link_configured for new training session.
			 * This ensures PHY will be configured even if TX uses
			 * same link rate/lane count as previous session.
			 */
			lt->phy_link_configured = false;

			/*
			 * Note: cdr_locked, cr_first_lock, voltage_swing, and
			 * pre_emphasis are cleared in link_config_handler (before
			 * TPS1 is written).
			 */
			for (j = 0; j < RTK_DPRX_MAX_LANES; j++)
				lt->last_vs[j] = 0;
		}

		/* Entry to EQ_TRAINING: reset counters */
		if (lt->state == LT_STATE_EQ_TRAINING &&
		    old_state != LT_STATE_EQ_TRAINING) {
			rtk_dprx_lt_enter_eq_training(dprx, lt->current_tp);
		}

		/* Entry to TRAINED: record success */
		if (lt->state == LT_STATE_TRAINED) {
			ktime_t end_time = ktime_get();
			u32 train_time_us;

			/* Cancel any pending timeout */
			cancel_delayed_work(&lt->timeout_work);

			/* Calculate training time */
			if (lt->start_time != 0) {
				train_time_us = ktime_us_delta(end_time, lt->start_time);
				lt->stats.last_train_time_us = train_time_us;

				/* Update min/max */
				if (lt->stats.min_train_time_us == 0 ||
				    train_time_us < lt->stats.min_train_time_us)
					lt->stats.min_train_time_us = train_time_us;
				if (train_time_us > lt->stats.max_train_time_us)
					lt->stats.max_train_time_us = train_time_us;
			}

			/* Update statistics */
			lt->stats.success_count++;

			/* Signal completion */
			complete(&lt->lt_done);

			dev_info(dprx->dev,
				 "LT: Training completed - rate=0x%02x(%s) lanes=%d time=%uus\n",
				 lt->requested_link_rate,
				 rtk_dprx_link_rate_name(lt->requested_link_rate),
				 lt->requested_lane_count,
				 lt->stats.last_train_time_us);

			/* Reset HPD toggle state on successful training */
			rtk_dprx_lt_hpd_toggle_reset(dprx);

			/* Start video scan work */
			rtk_dprx_lt_scan_work_start(dprx);
		}

		/* Entry to FAILED: record failure */
		if (lt->state == LT_STATE_FAILED) {
			/* Cancel any pending timeout */
			cancel_delayed_work(&lt->timeout_work);

			/* Set error_code if not already set */
			if (lt->error_code == 0) {
				switch (event) {
				case LT_EVENT_TP_END:
					lt->error_code = LT_ERR_SOURCE_ABORT;
					break;
				case LT_EVENT_TIMEOUT:
					lt->error_code = (old_state == LT_STATE_CR_TRAINING) ?
						LT_ERR_CR_TIMEOUT : LT_ERR_EQ_TIMEOUT;
					break;
				case LT_EVENT_TRAINING_LOST:
					lt->error_code = LT_ERR_TRAINING_LOST;
					break;
				default:
					lt->error_code = LT_ERR_UNKNOWN;
					break;
				}
			}

			lt->stats.fail_count++;
			lt->stats.last_error_code = lt->error_code;

			/* Signal completion (with error) */
			complete(&lt->lt_done);

			dev_err(dprx->dev, "LT: Training failed - error=%d\n",
				lt->error_code);
		}

		/* Execute training directly in threaded handler context */
		if (lt->state == LT_STATE_CR_TRAINING)
			rtk_dprx_lt_do_cr_training(dprx);
		else if (lt->state == LT_STATE_EQ_TRAINING)
			rtk_dprx_lt_do_eq_training(dprx);
	} else if (LT_TRANS_IS_STAY(result)) {
		/*
		 * For STAY transitions during training, run training again to
		 * process updated lane settings. When TX writes new VS/PE to
		 * DPCD 0x103-0x106, lane_setting_handler updates the cached
		 * values, and we need to update ADJUST_REQUEST (DPCD 0x206-0x207).
		 *
		 * CR phase: Source increases VS/PE seeking CR_DONE
		 * EQ phase: Source increases PE seeking EQ_DONE
		 */
		if (event == LT_EVENT_LANE_SETTING) {
			if (lt->state == LT_STATE_CR_TRAINING)
				rtk_dprx_lt_do_cr_training(dprx);
			else if (lt->state == LT_STATE_EQ_TRAINING)
				rtk_dprx_lt_do_eq_training(dprx);
		}
	}

	return 0;
}

/**
 * rtk_dprx_lt_handle_event - Public event handler
 * @dprx: DPRX instance
 * @event: Event to handle
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_handle_event(struct rtk_dprx *dprx, enum rtk_dprx_lt_event event)
{
	if (!dprx)
		return -EINVAL;

	return rtk_dprx_lt_do_state_transition(dprx, event);
}

/*============================================================================
 * State Query
 *============================================================================
 */

/**
 * rtk_dprx_lt_get_state - Get current Link Training state
 * @dprx: DPRX instance
 *
 * Return: Current state, or LT_STATE_DISCONNECTED on error
 */
enum rtk_dprx_lt_state rtk_dprx_lt_get_state(struct rtk_dprx *dprx)
{
	if (!dprx)
		return LT_STATE_DISCONNECTED;

	return dprx->lt_ctx.state;
}

/**
 * rtk_dprx_lt_is_trained - Check if link is trained
 * @dprx: DPRX instance
 *
 * Return: true if trained, false otherwise
 */
bool rtk_dprx_lt_is_trained(struct rtk_dprx *dprx)
{
	if (!dprx)
		return false;

	return dprx->lt_ctx.state == LT_STATE_TRAINED;
}

/*============================================================================
 * MAC Layer State API (replaces lt_status)
 *============================================================================
 */

bool rtk_dprx_lt_get_link_integrity_fail(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	unsigned long flags;
	bool result;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	spin_lock_irqsave(&lt->state_lock, flags);
	result = lt->link_integrity_fail;
	spin_unlock_irqrestore(&lt->state_lock, flags);

	return result;
}

void rtk_dprx_lt_set_link_integrity_fail(struct rtk_dprx *dprx, bool fail)
{
	struct rtk_dprx_lt_context *lt;
	unsigned long flags;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;
	spin_lock_irqsave(&lt->state_lock, flags);
	lt->link_integrity_fail = fail;
	spin_unlock_irqrestore(&lt->state_lock, flags);

	dev_info(dprx->dev, "LT MAC: Link integrity %s\n",
		 fail ? "FAIL" : "OK");
}

bool rtk_dprx_lt_get_fake_training_mode(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	unsigned long flags;
	bool result;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	spin_lock_irqsave(&lt->state_lock, flags);
	result = lt->fake_training_mode;
	spin_unlock_irqrestore(&lt->state_lock, flags);

	return result;
}

void rtk_dprx_lt_set_fake_training_mode(struct rtk_dprx *dprx, bool enable)
{
	struct rtk_dprx_lt_context *lt;
	unsigned long flags;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;
	spin_lock_irqsave(&lt->state_lock, flags);
	lt->fake_training_mode = enable;
	spin_unlock_irqrestore(&lt->state_lock, flags);

	dev_info(dprx->dev, "LT MAC: Fake training mode %s\n",
		 enable ? "ENABLED" : "DISABLED");
}

bool rtk_dprx_lt_get_vbios_mode(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	unsigned long flags;
	bool result;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	spin_lock_irqsave(&lt->state_lock, flags);
	result = lt->vbios_mode;
	spin_unlock_irqrestore(&lt->state_lock, flags);

	return result;
}

void rtk_dprx_lt_set_vbios_mode(struct rtk_dprx *dprx, bool enable)
{
	struct rtk_dprx_lt_context *lt;
	unsigned long flags;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;
	spin_lock_irqsave(&lt->state_lock, flags);
	lt->vbios_mode = enable;
	spin_unlock_irqrestore(&lt->state_lock, flags);

	dev_info(dprx->dev, "LT MAC: VBIOS mode %s\n",
		 enable ? "ENABLED" : "DISABLED");
}

bool rtk_dprx_lt_is_normal_pass(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	unsigned long flags;
	bool result;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	spin_lock_irqsave(&lt->state_lock, flags);
	result = (lt->state == LT_STATE_TRAINED &&
		  !lt->link_integrity_fail &&
		  !lt->fake_training_mode &&
		  !lt->vbios_mode);
	spin_unlock_irqrestore(&lt->state_lock, flags);

	return result;
}

bool rtk_dprx_lt_can_scan_video(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	unsigned long flags;
	bool result;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;
	spin_lock_irqsave(&lt->state_lock, flags);
	result = (lt->state == LT_STATE_TRAINED &&
		  (!lt->link_integrity_fail || lt->fake_training_mode || lt->vbios_mode));
	spin_unlock_irqrestore(&lt->state_lock, flags);

	return result;
}

/*============================================================================
 * Video Scan Work (Post-LT)
 *============================================================================
 */

/**
 * rtk_dprx_lt_scan_work_func - Periodic video scan after LT success
 * @work: Delayed work structure
 *
 * Called periodically after Link Training completes successfully.
 * Attempts to scan input port until success or timeout (RTK_DPRX_SCAN_TIMEOUT_MS).
 */
static void rtk_dprx_lt_scan_work_func(struct work_struct *work)
{
	struct rtk_dprx_lt_context *lt = container_of(work,
						       struct rtk_dprx_lt_context,
						       scan_work.work);
	struct rtk_dprx *dprx = container_of(lt, struct rtk_dprx, lt_ctx);
	ktime_t now;
	s64 elapsed_ms;
	int ret;

	/* Check if still in TRAINED state */
	if (lt->state != LT_STATE_TRAINED) {
		dev_dbg(dprx->dev, "Scan: Aborted, state changed to %s\n",
			rtk_dprx_lt_state_name(lt->state));
		lt->scan_work_active = false;
		return;
	}

	/* Check timeout */
	now = ktime_get();
	elapsed_ms = ktime_ms_delta(now, lt->scan_start_time);
	if (elapsed_ms >= RTK_DPRX_SCAN_TIMEOUT_MS) {
		dev_warn(dprx->dev,
			 "Scan: Timeout after %lldms, scan_input_port not successful\n",
			 elapsed_ms);
		lt->scan_work_active = false;

		/* Trigger HPD toggle to force Source re-training */
		rtk_dprx_lt_trigger_hpd_toggle(dprx);
		return;
	}

	if (dprx->mac_ops && dprx->mac_ops->pre_detect) {
		ret = dprx->mac_ops->pre_detect(dprx);
		if (ret == DPRX_NO_ERR) {
			dev_info(dprx->dev, "Scan: Pre detect success after %lldms\n",
				 elapsed_ms);
		} else {
			dev_info(dprx->dev, "Scan: pre detect failed - error=%d, elapsed=%lldms\n",
				ret, elapsed_ms);
			goto re_schedule;
		}
	}

	/* Attempt scan */
	if (dprx->mac_ops && dprx->mac_ops->scan_input_port) {
		ret = dprx->mac_ops->scan_input_port(dprx);
		if (ret == DPRX_NO_ERR) {
			dev_info(dprx->dev, "Scan: detect success after %lldms\n",
				 elapsed_ms);
			lt->scan_work_active = false;

			dprx->detect_done = true;

			/* Reset HPD toggle counter on successful scan */
			lt->hpd_toggle_count = 0;

			/* Start FIFO check monitoring */
			rtk_dprx_lt_fifo_check_work_start(dprx);
			return;
		}

		dev_info(dprx->dev, "Scan: scan input failed - error=%d, elapsed=%lldms\n",
			ret, elapsed_ms);
	} else {
		dev_warn(dprx->dev,
			 "Scan: mac_ops->scan_input_port not available\n");
		lt->scan_work_active = false;
		return;
	}

re_schedule:
	/* Reschedule */
	schedule_delayed_work(&lt->scan_work,
			      msecs_to_jiffies(RTK_DPRX_SCAN_INTERVAL_MS));
}

/**
 * rtk_dprx_lt_scan_work_start - Start video scan work
 * @dprx: DPRX instance
 *
 * Called when entering TRAINED state to begin periodic video scanning.
 */
void rtk_dprx_lt_scan_work_start(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt = &dprx->lt_ctx;

	if (lt->scan_work_active) {
		dev_dbg(dprx->dev, "Scan: Already active, skipping start\n");
		return;
	}

	lt->scan_start_time = ktime_get();
	lt->scan_work_active = true;

	dev_info(dprx->dev, "Scan: Starting video scan work (timeout=%dms)\n",
		 RTK_DPRX_SCAN_TIMEOUT_MS);

	/* Schedule immediately for first attempt */
	schedule_delayed_work(&lt->scan_work, 0);
}

/**
 * rtk_dprx_lt_scan_work_cancel - Cancel video scan work
 * @dprx: DPRX instance
 *
 * Called when leaving TRAINED state or during cleanup.
 */
static void rtk_dprx_lt_scan_work_cancel(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt = &dprx->lt_ctx;

	if (lt->scan_work_active) {
		cancel_delayed_work(&lt->scan_work);
		lt->scan_work_active = false;
		dev_dbg(dprx->dev, "Scan: Work cancelled\n");
	}
}

/*============================================================================
 * FIFO Check Work (Post-Scan)
 *============================================================================
 */

/**
 * rtk_dprx_lt_fifo_check_work_func - Periodic FIFO check after scan success
 * @work: Delayed work structure
 *
 * Called periodically after detect_done to monitor FIFO health.
 * On failure, stops streaming if active and triggers re-scan.
 */
static void rtk_dprx_lt_fifo_check_work_func(struct work_struct *work)
{
	struct rtk_dprx_lt_context *lt = container_of(work,
						       struct rtk_dprx_lt_context,
						       fifo_check_work.work);
	struct rtk_dprx *dprx = container_of(lt, struct rtk_dprx, lt_ctx);
	int ret;

	/* Abort if state changed */
	if (lt->state != LT_STATE_TRAINED) {
		dev_dbg(dprx->dev, "FIFO check: Aborted, state=%s\n",
			rtk_dprx_lt_state_name(lt->state));
		lt->fifo_check_work_active = false;
		return;
	}

	if (!dprx->detect_done) {
		dev_dbg(dprx->dev, "FIFO check: Aborted, detect_done=false\n");
		lt->fifo_check_work_active = false;
		return;
	}

	/* Perform FIFO check */
	if (dprx->mac_ops && dprx->mac_ops->fifo_check) {
		ret = dprx->mac_ops->fifo_check(dprx, _DP_FIFO_DELAY_CHECK);
		if (ret != DPRX_NO_ERR) {
			dev_warn(dprx->dev,
				 "FIFO check: Failed (err=%d), triggering recovery\n", ret);

			/* Mark detection as invalid */
			dprx->detect_done = false;

			/* Stop streaming if active (sequence > 0 means frames captured) */
			if (dprx->sequence > 0) {
				dev_info(dprx->dev,
					 "FIFO check: Stopping DMA/IRQ (seq=%u)\n",
					 dprx->sequence);
				if (dprx->wrap_ops) {
					dprx->wrap_ops->dma_go_ctrl(dprx, DISABLE);
					dprx->wrap_ops->interrupt_ctrl(dprx, DISABLE);
					dprx->mac_ops->mac_reset(dprx);
					dprx->mac_ops->mac_initial(dprx);
				}
			}

			/* Trigger re-scan */
			lt->fifo_check_work_active = false;
			rtk_dprx_lt_scan_work_start(dprx);
			return;
		}
		dev_dbg(dprx->dev, "FIFO check: OK\n");
	} else {
		dev_warn(dprx->dev, "FIFO check: fifo_check not available\n");
		lt->fifo_check_work_active = false;
		return;
	}

	/* Reschedule next check */
	schedule_delayed_work(&lt->fifo_check_work,
			      msecs_to_jiffies(RTK_DPRX_FIFO_CHECK_INTERVAL_MS));
}

/**
 * rtk_dprx_lt_fifo_check_work_start - Start FIFO check work
 * @dprx: DPRX instance
 *
 * Called when detect_done becomes true to begin periodic FIFO monitoring.
 */
static void rtk_dprx_lt_fifo_check_work_start(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt = &dprx->lt_ctx;

	if (lt->fifo_check_work_active) {
		dev_dbg(dprx->dev, "FIFO check: Already active\n");
		return;
	}

	lt->fifo_check_work_active = true;
	dev_info(dprx->dev, "FIFO check: Starting (interval=%dms)\n",
		 RTK_DPRX_FIFO_CHECK_INTERVAL_MS);

	schedule_delayed_work(&lt->fifo_check_work,
			      msecs_to_jiffies(RTK_DPRX_FIFO_CHECK_INTERVAL_MS));
}

/**
 * rtk_dprx_lt_fifo_check_work_cancel - Cancel FIFO check work
 * @dprx: DPRX instance
 *
 * Called when leaving TRAINED state or during cleanup.
 */
static void rtk_dprx_lt_fifo_check_work_cancel(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt = &dprx->lt_ctx;

	if (lt->fifo_check_work_active) {
		cancel_delayed_work(&lt->fifo_check_work);
		lt->fifo_check_work_active = false;
		dev_dbg(dprx->dev, "FIFO check: Cancelled\n");
	}
}

/*============================================================================
 * HPD Toggle Recovery (Scan Timeout)
 *============================================================================
 */

/**
 * rtk_dprx_lt_hpd_toggle_reset - Cancel pending HPD toggle work
 * @dprx: DPRX instance
 *
 * Cancels any pending reconnect work and clears in_progress flag.
 * Does NOT reset the toggle counter - the counter persists across
 * HPD toggle recovery cycles and is only reset on scan success or
 * genuine cable disconnect.
 */
static void rtk_dprx_lt_hpd_toggle_reset(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	if (lt->hpd_toggle_in_progress) {
		cancel_delayed_work(&lt->hpd_reconnect_work);
		lt->hpd_toggle_in_progress = false;
	}
}

/**
 * rtk_dprx_lt_hpd_reconnect_work_func - Reconnect HPD after toggle delay
 * @work: Delayed work structure
 *
 * Second phase of HPD toggle sequence. Called after HPD_TOGGLE_INTERVAL_MS
 * to re-assert HPD and allow Source to restart Link Training.
 */
static void rtk_dprx_lt_hpd_reconnect_work_func(struct work_struct *work)
{
	struct rtk_dprx_lt_context *lt = container_of(work,
						       struct rtk_dprx_lt_context,
						       hpd_reconnect_work.work);
	struct rtk_dprx *dprx = container_of(lt, struct rtk_dprx, lt_ctx);
	int ret;

	/* Race condition check: verify cable still connected */
	if (!rtk_dprx_lt_is_connected(dprx)) {
		dev_info(dprx->dev,
			 "HPD Toggle: Cable disconnected during toggle, aborting reconnect\n");
		lt->hpd_toggle_in_progress = false;
		return;
	}

	/* Re-assert HPD */
	ret = rtk_dprx_lt_phy_notify_connect(dprx);
	if (ret)
		dev_err(dprx->dev,
			"HPD Toggle: phy_notify_connect failed: %d\n", ret);

	dev_info(dprx->dev,
		 "HPD Toggle: HPD re-asserted (attempt %u/%u)\n",
		 lt->hpd_toggle_count, RTK_DPRX_HPD_TOGGLE_MAX_RETRIES);

	lt->hpd_toggle_in_progress = false;
}

/**
 * rtk_dprx_lt_trigger_hpd_toggle - Initiate HPD toggle sequence
 * @dprx: DPRX instance
 *
 * First phase of HPD toggle. De-asserts HPD and schedules reconnect work.
 * Called when scan work times out to force Source to re-initiate Link Training.
 *
 * Return: 0 on success (toggle initiated), negative on error or skip
 */
static int rtk_dprx_lt_trigger_hpd_toggle(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	/* Check if toggle already in progress */
	if (lt->hpd_toggle_in_progress) {
		dev_dbg(dprx->dev, "HPD Toggle: Already in progress, skipping\n");
		return -EBUSY;
	}

	/* Check connection state before toggle */
	if (!rtk_dprx_lt_is_connected(dprx)) {
		dev_info(dprx->dev,
			 "HPD Toggle: Cable not connected, skipping toggle\n");
		return -ENODEV;
	}

	/* Check retry limit */
	if (lt->hpd_toggle_count >= RTK_DPRX_HPD_TOGGLE_MAX_RETRIES) {
		dev_warn(dprx->dev,
			 "HPD Toggle: Max retries (%u) reached, giving up\n",
			 RTK_DPRX_HPD_TOGGLE_MAX_RETRIES);
		return -EAGAIN;
	}

	/* Increment retry counter */
	lt->hpd_toggle_count++;
	lt->hpd_toggle_in_progress = true;

	dev_info(dprx->dev,
		 "HPD Toggle: Initiating toggle (attempt %u/%u)\n",
		 lt->hpd_toggle_count, RTK_DPRX_HPD_TOGGLE_MAX_RETRIES);

	/* Phase 1: De-assert HPD */
	ret = rtk_dprx_lt_phy_notify_disconnect(dprx);
	if (ret) {
		dev_err(dprx->dev,
			"HPD Toggle: phy_notify_disconnect failed: %d\n", ret);
		lt->hpd_toggle_in_progress = false;
		return ret;
	}

	/* Phase 2: Schedule HPD re-assert after delay */
	schedule_delayed_work(&lt->hpd_reconnect_work,
			      msecs_to_jiffies(RTK_DPRX_HPD_TOGGLE_INTERVAL_MS));

	return 0;
}

/*============================================================================
 * Timeout Handler
 *============================================================================
 */

static void rtk_dprx_lt_timeout_func(struct work_struct *work)
{
	struct rtk_dprx_lt_context *lt;
	struct rtk_dprx *dprx;
	struct delayed_work *dwork;

	/* Coverity: NULL pointer check */
	if (!work)
		return;

	dwork = to_delayed_work(work);
	lt = container_of(dwork, struct rtk_dprx_lt_context, timeout_work);
	if (!lt)
		return;

	dprx = container_of(lt, struct rtk_dprx, lt_ctx);
	if (!dprx)
		return;

	dev_warn(dprx->dev, "LT Timeout in state %s\n",
		 rtk_dprx_lt_state_name(lt->state));

	lt->stats.timeout_count++;
	rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_TIMEOUT);
}

/*============================================================================
 * Context Reset
 *============================================================================
 */

/**
 * rtk_dprx_lt_context_reset - Reset Link Training context
 * @lt: Link Training context
 *
 * Resets runtime state while preserving config and stats.
 * Call before starting a new training sequence.
 */
static void __maybe_unused
rtk_dprx_lt_context_reset(struct rtk_dprx_lt_context *lt)
{
	int i;

	/* Coverity: NULL pointer check */
	if (!lt)
		return;

	lt->state = LT_STATE_IDLE;
	lt->prev_state = LT_STATE_IDLE;

	lt->requested_link_rate = 0;
	lt->requested_lane_count = 0;
	lt->current_tp = RTK_DPRX_TP_NONE;

	/* Coverity: Use ARRAY_SIZE for loop bounds */
	for (i = 0; i < ARRAY_SIZE(lt->lane); i++) {
		memset(&lt->lane[i], 0, sizeof(lt->lane[i]));
		lt->last_vs[i] = 0;
	}

	lt->cr_retry_count = 0;
	lt->eq_retry_count = 0;
	lt->eq_started = false;
	lt->same_vs_count = 0;

	lt->error_code = 0;

	reinit_completion(&lt->lt_done);
}

/**
 * rtk_dprx_lt_stats_reset - Reset Link Training statistics
 * @stats: Statistics structure
 */
static void rtk_dprx_lt_stats_reset(struct rtk_dprx_lt_stats *stats)
{
	/* Coverity: NULL pointer check */
	if (!stats)
		return;

	memset(stats, 0, sizeof(*stats));
	stats->min_train_time_us = U32_MAX;
}

/*============================================================================
 * Init / Deinit
 *============================================================================
 */

/**
 * rtk_dprx_link_training_init - Initialize Link Training module
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_link_training_init(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	int ret;

	/* Coverity: NULL pointer check */
	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	memset(lt, 0, sizeof(*lt));

	/* State machine init */
	lt->state = LT_STATE_DISCONNECTED;
	spin_lock_init(&lt->state_lock);

	/* Default config */
	lt->config.max_link_rate = RTK_DPRX_LINK_RATE_HBR2;
	lt->config.max_lane_count = RTK_DPRX_LANE_COUNT_4;
	lt->config.cr_timeout_ms = RTK_DPRX_CR_TIMEOUT_MS;
	lt->config.eq_timeout_ms = RTK_DPRX_EQ_TIMEOUT_MS;
	lt->config.aux_timeout_ms = RTK_DPRX_AUX_TIMEOUT_MS;
	lt->config.max_cr_retry = RTK_DPRX_CR_MAX_RETRY;
	lt->config.max_eq_retry = RTK_DPRX_EQ_MAX_RETRY;
	lt->config.max_same_vs_retry = RTK_DPRX_CR_MAX_SAME_VS;

	/* Stats init */
	rtk_dprx_lt_stats_reset(&lt->stats);

	/* Delayed work and completion init */
	INIT_DELAYED_WORK(&lt->timeout_work, rtk_dprx_lt_timeout_func);
	INIT_DELAYED_WORK(&lt->scan_work, rtk_dprx_lt_scan_work_func);
	INIT_DELAYED_WORK(&lt->hpd_reconnect_work, rtk_dprx_lt_hpd_reconnect_work_func);
	INIT_DELAYED_WORK(&lt->fifo_check_work, rtk_dprx_lt_fifo_check_work_func);
	lt->scan_work_active = false;
	lt->fifo_check_work_active = false;
	lt->hpd_toggle_count = 0;
	lt->hpd_toggle_in_progress = false;
	init_completion(&lt->lt_done);

	/* PHY ops init - use default implementation */
	lt->lt_phy_ops = &rtk_dprx_lt_phy_ops_default;

	/* Extcon consumer init - get extcon from DT */
	lt->edev = NULL;
	lt->dp_phy = NULL;
	ret = rtk_dprx_lt_extcon_init(dprx);
	if (ret == -EPROBE_DEFER) {
		dev_info(dprx->dev, "LT: Extcon not ready, requesting probe defer\n");
		return -EPROBE_DEFER;
	} else if (ret) {
		dev_warn(dprx->dev, "LT: Extcon init failed: %d (non-fatal)\n", ret);
	}

	/* PHY consumer init - get PHY from DT */
	ret = rtk_dprx_lt_phy_consumer_init(dprx);
	if (ret == -EPROBE_DEFER) {
		dev_info(dprx->dev, "LT: PHY not ready, requesting probe defer\n");
		return -EPROBE_DEFER;
	} else if (ret) {
		dev_warn(dprx->dev, "LT: PHY consumer init failed: %d (non-fatal)\n", ret);
	}

	/*
	 * Note: PHY init/power_on are now called in extcon_notifier when
	 * USB-C Alt Mode is plugged in. See rtk_dprx_lt_extcon_notifier().
	 */

	/* Sysfs init (Phase 5) */
	ret = rtk_dprx_lt_sysfs_init(dprx);
	if (ret)
		dev_warn(dprx->dev, "LT: Sysfs init failed: %d (non-fatal)\n", ret);

	/* V4L2 Events init (Phase 6) - subdev set later via rtk_dprx_lt_v4l2_init() */
	lt->v4l2_events_enabled = false;

	/* MAC Layer State init (replaces lt_status) */
	lt->link_integrity_fail = false;
	lt->fake_training_mode = false;
	lt->vbios_mode = false;

	dev_info(dprx->dev, "Link Training module initialized\n");

#if 0 // TODO: Only for test, should removed after finish driver implementation
	rtk_dprx_lt_phy_init(dprx);
	rtk_dprx_lt_do_state_transition(dprx, LT_EVENT_HPD_HIGH);
#endif
#if RTK_ONLY_FOR_TEST // TODO: Only for test, should removed after finish driver implementation
	rtk_dprx_lt_phy_configure(dprx, _DP_HIGH_SPEED_270MHZ, _DP_FOUR_LANE);
	msleep(100);
	dprx->detect_done = true;
#endif

	return 0;
}

/**
 * rtk_dprx_lt_set_phy_ops - Set custom PHY operations
 * @dprx: DPRX instance
 * @ops: PHY operations structure (NULL to use default)
 *
 * Allows platform-specific PHY operations to be registered.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_set_phy_ops(struct rtk_dprx *dprx,
			    const struct rtk_dprx_lt_phy_ops *ops)
{
	if (!dprx)
		return -EINVAL;

	if (ops) {
		/* Validate required ops */
		if (!rtk_dprx_lt_phy_ops_validate(ops)) {
			dev_err(dprx->dev, "LT: Invalid PHY ops\n");
			return -EINVAL;
		}
		dprx->lt_ctx.lt_phy_ops = ops;
	} else {
		/* Use default */
		dprx->lt_ctx.lt_phy_ops = &rtk_dprx_lt_phy_ops_default;
	}

	dev_dbg(dprx->dev, "LT: PHY ops set\n");
	return 0;
}

/**
 * rtk_dprx_link_training_deinit - Deinitialize Link Training module
 * @dprx: DPRX instance
 */
void rtk_dprx_link_training_deinit(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;

	/* Coverity: NULL pointer check */
	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	/* Remove sysfs */
	rtk_dprx_lt_sysfs_exit(dprx);

	/* Cleanup V4L2 events */
	rtk_dprx_lt_v4l2_exit(dprx);

	/* Cancel pending delayed work - must be done before cleanup */
	cancel_delayed_work_sync(&lt->timeout_work);
	cancel_delayed_work_sync(&lt->scan_work);
	cancel_delayed_work_sync(&lt->fifo_check_work);
	cancel_delayed_work_sync(&lt->hpd_reconnect_work);

	/* Cleanup extcon consumer */
	rtk_dprx_lt_extcon_exit(dprx);

	/* PHY hardware exit - must be before consumer exit */
	rtk_dprx_lt_phy_exit(dprx);

	/* Cleanup PHY consumer */
	rtk_dprx_lt_phy_consumer_exit(dprx);

	dev_info(dprx->dev, "Link Training module deinitialized\n");
}

MODULE_DESCRIPTION("Realtek DisplayPort RX Link Training");
MODULE_LICENSE("GPL");
