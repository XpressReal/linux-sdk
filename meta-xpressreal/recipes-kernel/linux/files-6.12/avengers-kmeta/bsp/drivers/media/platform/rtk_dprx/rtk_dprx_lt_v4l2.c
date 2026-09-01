// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek DisplayPort RX Link Training - V4L2 Events Interface
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * Provides V4L2 event notifications for Link Training state changes
 * using the video_device event queue mechanism.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/build_bug.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-v4l2.h>

#include "rtk_dprx.h"
#include "rtk_dprx_link_training.h"
#include "rtk_dprx_lt_v4l2.h"

/* Compile-time check: ensure event data fits in v4l2_event.u.data (64 bytes) */
static_assert(sizeof(struct rtk_dprx_lt_event_data) <= 64,
	      "rtk_dprx_lt_event_data exceeds v4l2_event.u.data size");

/*============================================================================
 * Internal Helper Functions
 *============================================================================*/

/**
 * rtk_dprx_lt_fill_event_data - Fill event data structure
 * @dprx: DPRX instance
 * @data: Event data to fill
 */
static void rtk_dprx_lt_fill_event_data(struct rtk_dprx *dprx,
					struct rtk_dprx_lt_event_data *data)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx || !data)
		return;

	lt = &dprx->lt_ctx;

	memset(data, 0, sizeof(*data));

	data->state = lt->state;
	data->prev_state = lt->prev_state;
	data->link_rate = lt->requested_link_rate;
	data->lane_count = lt->requested_lane_count;
	data->error_code = lt->error_code;
	data->train_time_us = lt->stats.last_train_time_us;
}

/**
 * rtk_dprx_lt_state_to_event - Map state transition to event type
 * @old_state: Previous state
 * @new_state: New state
 *
 * Return: Appropriate RTK_DPRX_LT_EVENT_xxx flag
 */
static u32 rtk_dprx_lt_state_to_event(int old_state, int new_state)
{
	u32 event_type = 0;

	/* Disconnected <-> Connected transitions */
	if (old_state == LT_STATE_DISCONNECTED && new_state != LT_STATE_DISCONNECTED)
		event_type |= RTK_DPRX_LT_EVENT_HPD_CHANGE;
	else if (old_state != LT_STATE_DISCONNECTED && new_state == LT_STATE_DISCONNECTED)
		event_type |= RTK_DPRX_LT_EVENT_HPD_CHANGE;

	/* Training state transitions */
	switch (new_state) {
	case LT_STATE_CR_TRAINING:
		if (old_state == LT_STATE_IDLE)
			event_type |= RTK_DPRX_LT_EVENT_TRAINING_START;
		break;

	case LT_STATE_TRAINED:
		event_type |= RTK_DPRX_LT_EVENT_TRAINING_DONE;
		break;

	case LT_STATE_FAILED:
		event_type |= RTK_DPRX_LT_EVENT_TRAINING_FAIL;
		break;

	case LT_STATE_IDLE:
		if (old_state == LT_STATE_TRAINED)
			event_type |= RTK_DPRX_LT_EVENT_LINK_LOST;
		break;

	default:
		break;
	}

	return event_type;
}

/*============================================================================
 * V4L2 Event Delivery
 *============================================================================*/

/**
 * rtk_dprx_lt_v4l2_send_event - Send V4L2 source change event
 * @dprx: DPRX instance
 * @changes: Change flags (RTK_DPRX_LT_EVENT_xxx)
 */
static void rtk_dprx_lt_v4l2_send_event(struct rtk_dprx *dprx, u32 changes)
{
	struct rtk_dprx_lt_context *lt;
	struct v4l2_event event;
	struct rtk_dprx_lt_event_data *data;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	/* Skip if V4L2 events not enabled */
	if (!lt->v4l2_events_enabled)
		return;

	/* Build event */
	memset(&event, 0, sizeof(event));
	event.type = V4L2_EVENT_SOURCE_CHANGE;
	event.u.src_change.changes = changes;

	/* Fill custom event data */
	data = (struct rtk_dprx_lt_event_data *)event.u.data;
	rtk_dprx_lt_fill_event_data(dprx, data);

	/* Queue event to video device */
	v4l2_event_queue(&dprx->vdev, &event);

	dev_dbg(dprx->dev, "LT V4L2: Event sent changes=0x%08x state=%d\n",
		changes, lt->state);
}

/*============================================================================
 * Public API Implementation
 *============================================================================*/

/**
 * rtk_dprx_lt_v4l2_init - Initialize V4L2 event support
 */
int rtk_dprx_lt_v4l2_init(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	/* Enable V4L2 events */
	lt->v4l2_events_enabled = true;

	dev_info(dprx->dev, "LT V4L2: Event support initialized\n");

	return 0;
}

/**
 * rtk_dprx_lt_v4l2_exit - Cleanup V4L2 event support
 */
void rtk_dprx_lt_v4l2_exit(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	/* Disable V4L2 events */
	lt->v4l2_events_enabled = false;

	dev_info(dprx->dev, "LT V4L2: Event support cleaned up\n");
}

/**
 * rtk_dprx_lt_v4l2_notify - Send V4L2 event notification
 */
void rtk_dprx_lt_v4l2_notify(struct rtk_dprx *dprx, u32 event_type)
{
	if (!dprx || event_type == RTK_DPRX_LT_EVENT_NONE)
		return;

	rtk_dprx_lt_v4l2_send_event(dprx, event_type);
}

/**
 * rtk_dprx_lt_v4l2_notify_state_change - Notify state change
 */
void rtk_dprx_lt_v4l2_notify_state_change(struct rtk_dprx *dprx,
					  int old_state, int new_state)
{
	u32 event_type;

	if (!dprx)
		return;

	/* Don't notify if state unchanged */
	if (old_state == new_state)
		return;

	/* Determine event type from state transition */
	event_type = rtk_dprx_lt_state_to_event(old_state, new_state);

	if (event_type != RTK_DPRX_LT_EVENT_NONE)
		rtk_dprx_lt_v4l2_send_event(dprx, event_type);
}
