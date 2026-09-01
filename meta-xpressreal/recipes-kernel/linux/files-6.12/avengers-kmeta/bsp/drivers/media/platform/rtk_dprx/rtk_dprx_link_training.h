/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek DisplayPort RX Link Training
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * Usage Example:
 *
 *   // Initialization (in probe)
 *   ret = rtk_dprx_link_training_init(dprx);
 *   if (ret)
 *       return ret;
 *
 *   // Optional: Set custom PHY ops
 *   ret = rtk_dprx_lt_set_phy_ops(dprx, &my_phy_ops);
 *
 *   // In HPD IRQ handler
 *   if (hpd_high)
 *       rtk_dprx_lt_handle_event(dprx, LT_EVENT_HPD_HIGH);
 *   else
 *       rtk_dprx_lt_handle_event(dprx, LT_EVENT_HPD_LOW);
 *
 *   // In DPCD Write IRQ handler
 *   rtk_dprx_lt_dpcd_irq_handler(dprx, irq_status);
 *
 *   // Query state
 *   if (rtk_dprx_lt_is_trained(dprx))
 *       // Link is ready for video
 *
 *   // Cleanup (in remove)
 *   rtk_dprx_link_training_deinit(dprx);
 */

#ifndef __RTK_DPRX_LINK_TRAINING_H__
#define __RTK_DPRX_LINK_TRAINING_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>

/* Forward declarations */
struct rtk_dprx;
struct extcon_dev;
struct phy;

/*============================================================================
 * Constants
 *============================================================================*/

#define RTK_DPRX_MAX_LANES              4

/* Link Rate (DPCD 0x100) */
#define RTK_DPRX_LINK_RATE_RBR          0x06    /* 1.62 Gbps */
#define RTK_DPRX_LINK_RATE_HBR          0x0A    /* 2.7 Gbps */
#define RTK_DPRX_LINK_RATE_HBR2         0x14    /* 5.4 Gbps */
#define RTK_DPRX_LINK_RATE_HBR3         0x1E    /* 8.1 Gbps */

/* Lane Count */
#define RTK_DPRX_LANE_COUNT_1           1
#define RTK_DPRX_LANE_COUNT_2           2
#define RTK_DPRX_LANE_COUNT_4           4

/* Voltage Swing Level (0-3) */
#define RTK_DPRX_VS_LEVEL_MAX           3

/* Pre-Emphasis Level (0-3) */
#define RTK_DPRX_PE_LEVEL_MAX           3

/* Training Pattern (DPCD 0x00102 Bits 3-0 values) */
#define RTK_DPRX_TP_NONE                0
#define RTK_DPRX_TP_1                   1
#define RTK_DPRX_TP_2                   2
#define RTK_DPRX_TP_3                   3
#define RTK_DPRX_TP_4                   7  /* DP 1.4a: TPS4 = 0111b */

/* DPCD 0x00102 TRAINING_PATTERN_SET bit field definitions */
#define DPCD_TP_SET_PATTERN_MASK        0x0F  /* Bits 3-0: Training Pattern */
#define DPCD_TP_SET_LINK_QUAL_MASK      0x30  /* Bits 5-4: Link Qual Pattern */
#define DPCD_TP_SET_SCRAMBLING_DISABLE  BIT(5) /* Bit 5: Scrambling Disable */
#define DPCD_TP_SET_SYMBOL_ERR_SEL_MASK 0xC0  /* Bits 7-6: Symbol Error Count */

/* DPCD 0x00101 LANE_COUNT_SET bit field definitions */
#define DPCD_LANE_COUNT_MASK            0x1F  /* Bits 4-0: Lane Count */
#define DPCD_LANE_COUNT_ENHANCED_FRAME  BIT(7) /* Bit 7: Enhanced Frame Enable */

/* DPCD 0x0000E TRAINING_AUX_RD_INTERVAL */
#define DPCD_TRAINING_AUX_RD_INTERVAL   0x0000E

/*============================================================================
 * Timeout / Retry Defaults
 *
 * Reference: DP 1.4a Spec, CTS 1.2, RTK Implementation
 *============================================================================*/

#define RTK_DPRX_CR_TIMEOUT_MS          100
#define RTK_DPRX_EQ_TIMEOUT_MS          400
#define RTK_DPRX_AUX_TIMEOUT_MS         50
#define RTK_DPRX_CR_MAX_RETRY           10
#define RTK_DPRX_EQ_MAX_RETRY           5       /* DP Spec: 5 */
#define RTK_DPRX_CR_MAX_SAME_VS         5       /* DP Spec: 5 */

/* Video Scan Work Timing (post-LT) */
#define RTK_DPRX_SCAN_TIMEOUT_MS        3500    /* 3.5 seconds total timeout */
#define RTK_DPRX_SCAN_INTERVAL_MS       200     /* 200ms between retries */

/* HPD Toggle Recovery (scan timeout) */
#define RTK_DPRX_HPD_TOGGLE_INTERVAL_MS 650     /* HPD low duration: 650ms */
#define RTK_DPRX_HPD_TOGGLE_MAX_RETRIES 3       /* Max HPD toggle attempts */

/* FIFO Check Work Timing (post-scan) */
#define RTK_DPRX_FIFO_CHECK_INTERVAL_MS 500     /* 500ms between FIFO checks */

/*============================================================================
 * State Machine
 *============================================================================*/

/**
 * enum rtk_dprx_lt_state - Link Training States
 */
enum rtk_dprx_lt_state {
	LT_STATE_DISCONNECTED = 0,
	LT_STATE_IDLE,
	LT_STATE_CR_TRAINING,
	LT_STATE_EQ_TRAINING,
	LT_STATE_TRAINED,
	LT_STATE_FAILED,
	LT_STATE_COUNT
};

/**
 * enum rtk_dprx_lt_event - Link Training Events
 */
enum rtk_dprx_lt_event {
	LT_EVENT_HPD_HIGH = 0,
	LT_EVENT_HPD_LOW,
	LT_EVENT_LINK_CONFIG,
	LT_EVENT_TP1_RECEIVED,
	LT_EVENT_TP2_RECEIVED,
	LT_EVENT_TP3_RECEIVED,
	LT_EVENT_TP4_RECEIVED,
	LT_EVENT_TP_END,
	LT_EVENT_LANE_SETTING,
	LT_EVENT_CR_DONE,
	LT_EVENT_CR_FAILED,
	LT_EVENT_EQ_DONE,
	LT_EVENT_EQ_FAILED,
	LT_EVENT_TIMEOUT,
	LT_EVENT_TRAINING_LOST,
	LT_EVENT_RESET,
	LT_EVENT_COUNT
};

/* State Transition Results */
enum rtk_dprx_lt_transition {
	LT_TRANS_STAY   = -1,
	LT_TRANS_IGNORE = -2,
	LT_TRANS_ERROR  = -3,
};

#define LT_TRANS_IS_CHANGE(trans)  ((trans) >= 0)
#define LT_TRANS_IS_STAY(trans)    ((trans) == LT_TRANS_STAY)
#define LT_TRANS_IS_IGNORE(trans)  ((trans) == LT_TRANS_IGNORE)
#define LT_TRANS_IS_ERROR(trans)   ((trans) == LT_TRANS_ERROR)

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * struct rtk_dprx_lane_status - Per-lane status
 */
struct rtk_dprx_lane_status {
	u8 cr_done:1;
	u8 eq_done:1;
	u8 symbol_locked:1;
	u8 voltage_swing:2;
	u8 pre_emphasis:2;
	u8 max_vs_reached:1;

	u8 max_pe_reached:1;
	u8 cdr_locked:1;	/* Actual CDR lock status from PHY */
	u8 reserved:6;
};

/**
 * struct rtk_dprx_lt_config - Configuration parameters
 */
struct rtk_dprx_lt_config {
	u8 max_link_rate;
	u8 max_lane_count;
	u16 cr_timeout_ms;
	u16 eq_timeout_ms;
	u16 aux_timeout_ms;
	u8 max_cr_retry;
	u8 max_eq_retry;
	u8 max_same_vs_retry;
	u8 debug_mode:1;
	u8 reserved:7;
};

/**
 * struct rtk_dprx_lt_stats - Statistics
 */
struct rtk_dprx_lt_stats {
	u32 total_attempts;
	u32 success_count;
	u32 fail_count;
	u32 cr_fail_count;
	u32 eq_fail_count;
	u32 timeout_count;
	u32 error_event_count;
	int last_error_code;
	u32 last_train_time_us;
	u32 min_train_time_us;
	u32 max_train_time_us;
};

/* Forward declarations */
struct rtk_dprx_lt_phy_ops;

/**
 * struct rtk_dprx_lt_context - Main Link Training context
 */
struct rtk_dprx_lt_context {
	/* State Machine */
	enum rtk_dprx_lt_state state;
	spinlock_t state_lock;
	enum rtk_dprx_lt_state prev_state;

	/* Link Parameters */
	u8 requested_link_rate;
	u8 requested_lane_count;
	u8 current_tp;
	bool enhanced_frame_en;		/* DPCD 0x101 Bit 7 */
	bool scrambling_disabled;	/* DPCD 0x102 Bit 5 */

	/* Lane Status */
	struct rtk_dprx_lane_status lane[RTK_DPRX_MAX_LANES];

	/* Retry Counters */
	u8 cr_retry_count;
	u8 eq_retry_count;
	bool eq_started;	/* Prevent duplicate EQ start (race condition guard) */
	u8 same_vs_count;
	u8 last_vs[RTK_DPRX_MAX_LANES];

	/* Config & Stats */
	struct rtk_dprx_lt_config config;
	struct rtk_dprx_lt_stats stats;

	/* Timeout and timing */
	struct delayed_work timeout_work;
	ktime_t start_time;

	/* Video Scan Work (post-LT) */
	struct delayed_work scan_work;		/* Periodic scan work */
	ktime_t scan_start_time;		/* Scan timeout tracking */
	bool scan_work_active;			/* Scan work currently scheduled */

	/* HPD Toggle Recovery (scan timeout) */
	struct delayed_work hpd_reconnect_work;	/* HPD reconnect after toggle */
	u8 hpd_toggle_count;			/* HPD toggle retry counter */
	bool hpd_toggle_in_progress;		/* HPD toggle sequence active */

	/* FIFO Check Work (post-scan) */
	struct delayed_work fifo_check_work;	/* Periodic FIFO check work */
	bool fifo_check_work_active;		/* FIFO check work currently scheduled */

	/* Synchronization */
	struct completion lt_done;
	int error_code;

	/* PHY Ops (legacy - for internal PHY control) */
	const struct rtk_dprx_lt_phy_ops *lt_phy_ops;

	/* Extcon Consumer - Type-C Integration */
	struct extcon_dev *edev;		/* Extcon from DT (consumer) */
	struct notifier_block extcon_nb;	/* Notifier for EXTCON_DISP_DP */
	bool typec_flipped;			/* Type-C orientation */

	/* PHY Consumer - DP PHY from USB3/TypeC driver */
	struct phy *dp_phy;			/* PHY from DT (consumer) */

	/* V4L2 Events (Phase 6) */
	bool v4l2_events_enabled;		/* V4L2 events enabled */

	/* CR_DONE deferral: delay CR_DONE once to allow drive setting optimization */
	bool cr_first_lock;

	/* PHY link configured: prevent duplicate phy_cfg in same session */
	bool phy_link_configured;

	/* MAC Layer State Extension (replaces lt_status) */
	bool link_integrity_fail;	/* Link Status check failed but recoverable */
	bool fake_training_mode;	/* Fake Training mode (testing) */
	bool vbios_mode;		/* VBIOS auto-detection mode */
};

/*============================================================================
 * Public API
 *============================================================================*/

/* Initialization */
int rtk_dprx_link_training_init(struct rtk_dprx *dprx);
void rtk_dprx_link_training_deinit(struct rtk_dprx *dprx);

/* PHY Operations */
struct rtk_dprx_lt_phy_ops;  /* Forward declaration */
int rtk_dprx_lt_set_phy_ops(struct rtk_dprx *dprx,
			    const struct rtk_dprx_lt_phy_ops *ops);

/* Event Handling */
int rtk_dprx_lt_handle_event(struct rtk_dprx *dprx, enum rtk_dprx_lt_event event);
void rtk_dprx_lt_dpcd_irq_handler(struct rtk_dprx *dprx, u8 irq_status);

/* Scan Work */
void rtk_dprx_lt_scan_work_start(struct rtk_dprx *dprx);

/* State Query */
enum rtk_dprx_lt_state rtk_dprx_lt_get_state(struct rtk_dprx *dprx);
bool rtk_dprx_lt_is_trained(struct rtk_dprx *dprx);

/* MAC Layer State (replaces lt_status) */

/**
 * rtk_dprx_lt_get_link_integrity_fail - Get link integrity fail status
 * @dprx: DPRX device
 *
 * Return: true if Link Status check failed but recoverable, false otherwise
 */
bool rtk_dprx_lt_get_link_integrity_fail(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_set_link_integrity_fail - Set link integrity fail status
 * @dprx: DPRX device
 * @fail: true to mark Link Status failed, false to clear
 */
void rtk_dprx_lt_set_link_integrity_fail(struct rtk_dprx *dprx, bool fail);

/**
 * rtk_dprx_lt_get_fake_training_mode - Get fake training mode status
 * @dprx: DPRX device
 *
 * Return: true if in Fake Training mode (testing), false otherwise
 */
bool rtk_dprx_lt_get_fake_training_mode(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_set_fake_training_mode - Set fake training mode
 * @dprx: DPRX device
 * @enable: true to enable Fake Training mode, false to disable
 */
void rtk_dprx_lt_set_fake_training_mode(struct rtk_dprx *dprx, bool enable);

/**
 * rtk_dprx_lt_get_vbios_mode - Get VBIOS mode status
 * @dprx: DPRX device
 *
 * Return: true if in VBIOS auto-detection mode, false otherwise
 */
bool rtk_dprx_lt_get_vbios_mode(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_set_vbios_mode - Set VBIOS mode
 * @dprx: DPRX device
 * @enable: true to enable VBIOS mode, false to disable
 */
void rtk_dprx_lt_set_vbios_mode(struct rtk_dprx *dprx, bool enable);

/**
 * rtk_dprx_lt_is_normal_pass - Check if Link Training passed normally
 * @dprx: DPRX device
 *
 * Return: true if TRAINED state with no flags set (normal LT pass)
 *
 * Equivalent to old lt_status == _DP_NORMAL_LINK_TRAINING_PASS
 */
bool rtk_dprx_lt_is_normal_pass(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_can_scan_video - Check if video scanning is allowed
 * @dprx: DPRX device
 *
 * Return: true if video scanning can proceed (normal pass or fake training)
 */
bool rtk_dprx_lt_can_scan_video(struct rtk_dprx *dprx);

/* Debug */
const char *rtk_dprx_lt_state_name(enum rtk_dprx_lt_state state);
const char *rtk_dprx_lt_event_name(enum rtk_dprx_lt_event event);

#endif /* __RTK_DPRX_LINK_TRAINING_H__ */
