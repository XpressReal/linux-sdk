/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek DisplayPort RX Link Training - V4L2 Events Interface
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * Provides V4L2 event notifications for Link Training state changes.
 * This allows userspace applications to monitor LT progress via
 * standard V4L2 event subscription mechanism.
 *
 * Event Types:
 *   V4L2_EVENT_SOURCE_CHANGE - Link training state changed
 *
 * Usage:
 *   // Subscribe to source change events
 *   struct v4l2_event_subscription sub = {
 *       .type = V4L2_EVENT_SOURCE_CHANGE,
 *       .id = 0,
 *   };
 *   ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
 *
 *   // Wait for and dequeue events
 *   struct v4l2_event ev;
 *   ioctl(fd, VIDIOC_DQEVENT, &ev);
 *
 *   // Check LT-specific data
 *   struct rtk_dprx_lt_event_data *data = (void *)ev.u.data;
 */

#ifndef __RTK_DPRX_LT_V4L2_H__
#define __RTK_DPRX_LT_V4L2_H__

#include <linux/types.h>

/* Forward declarations */
struct rtk_dprx;

/*============================================================================
 * Event Data Structures
 *============================================================================*/

/**
 * enum rtk_dprx_lt_v4l2_event_type - LT event sub-types
 *
 * These are stored in v4l2_event.u.src_change.changes for
 * V4L2_EVENT_SOURCE_CHANGE events.
 */
enum rtk_dprx_lt_v4l2_event_type {
	RTK_DPRX_LT_EVENT_NONE          = 0,
	RTK_DPRX_LT_EVENT_HPD_CHANGE    = BIT(0),  /* HPD state changed */
	RTK_DPRX_LT_EVENT_TRAINING_START = BIT(1), /* Training started */
	RTK_DPRX_LT_EVENT_TRAINING_DONE = BIT(2),  /* Training completed */
	RTK_DPRX_LT_EVENT_TRAINING_FAIL = BIT(3),  /* Training failed */
	RTK_DPRX_LT_EVENT_LINK_LOST     = BIT(4),  /* Link lost after training */
	RTK_DPRX_LT_EVENT_RATE_CHANGE   = BIT(5),  /* Link rate changed */
	RTK_DPRX_LT_EVENT_LANE_CHANGE   = BIT(6),  /* Lane count changed */
};

/**
 * struct rtk_dprx_lt_event_data - LT event payload
 *
 * This structure is embedded in v4l2_event.u.data (64 bytes max).
 * Provides detailed information about the LT state change.
 */
struct rtk_dprx_lt_event_data {
	__u8 state;          /* Current LT state (enum rtk_dprx_lt_state) */
	__u8 prev_state;     /* Previous LT state */
	__u8 link_rate;      /* Current link rate (DPCD 0x100 value) */
	__u8 lane_count;     /* Current lane count */
	__s32 error_code;    /* Error code if training failed */
	__u32 train_time_us; /* Training time in microseconds */
	__u32 reserved[10];  /* Reserved for future use */
} __packed;

/*
 * Compile-time check: ensure event data fits in v4l2_event.u.data (64 bytes)
 * Note: This check is done in rtk_dprx_lt_v4l2.c using static_assert
 */

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * rtk_dprx_lt_v4l2_init - Initialize V4L2 event support
 * @dprx: DPRX instance
 *
 * Enables V4L2 event delivery via dprx->vdev.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_v4l2_init(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_v4l2_exit - Cleanup V4L2 event support
 * @dprx: DPRX instance
 */
void rtk_dprx_lt_v4l2_exit(struct rtk_dprx *dprx);

/**
 * rtk_dprx_lt_v4l2_notify - Send V4L2 event notification
 * @dprx: DPRX instance
 * @event_type: Event type (RTK_DPRX_LT_EVENT_xxx)
 *
 * Called internally when LT state changes to notify subscribed clients.
 */
void rtk_dprx_lt_v4l2_notify(struct rtk_dprx *dprx, u32 event_type);

/**
 * rtk_dprx_lt_v4l2_notify_state_change - Notify state change
 * @dprx: DPRX instance
 * @old_state: Previous state
 * @new_state: New state
 *
 * Convenience function that determines appropriate event type
 * based on state transition.
 */
void rtk_dprx_lt_v4l2_notify_state_change(struct rtk_dprx *dprx,
					  int old_state, int new_state);

#endif /* __RTK_DPRX_LT_V4L2_H__ */
