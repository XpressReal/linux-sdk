// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek DisplayPort RX Link Training - Sysfs Interface
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * Provides sysfs nodes for debugging and monitoring Link Training.
 *
 * Sysfs nodes (under /sys/devices/.../dprx_lt/):
 *   state      (RO) - Current LT state
 *   trained    (RO) - Link trained status (0/1)
 *   link_rate  (RO) - Current link rate
 *   lane_count (RO) - Current lane count
 *   error_code (RO) - Last error code
 *   stats      (RO) - Training statistics
 *   reset      (WO) - Reset state machine (write 1)
 *   debug      (RW) - Debug mode (0/1)
 */

#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/string.h>

#include "rtk_dprx.h"
#include "rtk_dprx_link_training.h"
#include "rtk_dprx_lt_sysfs.h"

/*============================================================================
 * Sysfs Show Functions (Read-Only Nodes)
 *============================================================================*/

/**
 * state_show - Show current Link Training state
 */
static ssize_t state_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	enum rtk_dprx_lt_state state;

	if (!dprx)
		return -ENODEV;

	state = rtk_dprx_lt_get_state(dprx);

	return sysfs_emit(buf, "%s\n", rtk_dprx_lt_state_name(state));
}
static DEVICE_ATTR_RO(state);

/**
 * trained_show - Show if link is trained
 */
static ssize_t trained_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);

	if (!dprx)
		return -ENODEV;

	return sysfs_emit(buf, "%d\n", rtk_dprx_lt_is_trained(dprx) ? 1 : 0);
}
static DEVICE_ATTR_RO(trained);

/**
 * link_rate_show - Show current link rate
 */
static ssize_t link_rate_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	struct rtk_dprx_lt_context *lt;
	const char *rate_name;
	u8 rate;

	if (!dprx)
		return -ENODEV;

	lt = &dprx->lt_ctx;
	rate = lt->requested_link_rate;

	switch (rate) {
	case RTK_DPRX_LINK_RATE_RBR:
		rate_name = "RBR (1.62 Gbps)";
		break;
	case RTK_DPRX_LINK_RATE_HBR:
		rate_name = "HBR (2.7 Gbps)";
		break;
	case RTK_DPRX_LINK_RATE_HBR2:
		rate_name = "HBR2 (5.4 Gbps)";
		break;
	case RTK_DPRX_LINK_RATE_HBR3:
		rate_name = "HBR3 (8.1 Gbps)";
		break;
	default:
		rate_name = "Unknown";
		break;
	}

	return sysfs_emit(buf, "0x%02x %s\n", rate, rate_name);
}
static DEVICE_ATTR_RO(link_rate);

/**
 * lane_count_show - Show current lane count
 */
static ssize_t lane_count_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return -ENODEV;

	lt = &dprx->lt_ctx;

	return sysfs_emit(buf, "%d\n", lt->requested_lane_count);
}
static DEVICE_ATTR_RO(lane_count);

/**
 * error_code_show - Show last error code
 */
static ssize_t error_code_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	struct rtk_dprx_lt_context *lt;
	int err;
	const char *category;

	if (!dprx)
		return -ENODEV;

	lt = &dprx->lt_ctx;
	err = lt->error_code;

	/* Determine error category */
	if (err == 0)
		category = "None";
	else if (LT_ERR_IS_PARAM(err))
		category = "Parameter";
	else if (LT_ERR_IS_CR(err))
		category = "CR";
	else if (LT_ERR_IS_EQ(err))
		category = "EQ";
	else if (LT_ERR_IS_PHY_CONFIG(err))
		category = "PHY Config";
	else if (LT_ERR_IS_PHY_STATUS(err))
		category = "PHY Status";
	else if (LT_ERR_IS_AUX(err))
		category = "AUX";
	else if (LT_ERR_IS_EXTCON(err))
		category = "Extcon";
	else if (LT_ERR_IS_TIMEOUT(err))
		category = "Timeout";
	else
		category = "Unknown";

	return sysfs_emit(buf, "%d (%s)\n", err, category);
}
static DEVICE_ATTR_RO(error_code);

/**
 * stats_show - Show training statistics
 */
static ssize_t stats_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	struct rtk_dprx_lt_context *lt;
	struct rtk_dprx_lt_stats *stats;

	if (!dprx)
		return -ENODEV;

	lt = &dprx->lt_ctx;
	stats = &lt->stats;

	return sysfs_emit(buf,
		"total_attempts:    %u\n"
		"success_count:     %u\n"
		"fail_count:        %u\n"
		"cr_fail_count:     %u\n"
		"eq_fail_count:     %u\n"
		"timeout_count:     %u\n"
		"error_event_count: %u\n"
		"last_error_code:   %d\n"
		"last_train_time:   %u us\n"
		"min_train_time:    %u us\n"
		"max_train_time:    %u us\n",
		stats->total_attempts,
		stats->success_count,
		stats->fail_count,
		stats->cr_fail_count,
		stats->eq_fail_count,
		stats->timeout_count,
		stats->error_event_count,
		stats->last_error_code,
		stats->last_train_time_us,
		stats->min_train_time_us,
		stats->max_train_time_us);
}
static DEVICE_ATTR_RO(stats);

/**
 * lane_status_show - Show per-lane status
 */
static ssize_t lane_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	struct rtk_dprx_lt_context *lt;
	int i, len = 0;
	u8 lane_count;

	if (!dprx)
		return -ENODEV;

	lt = &dprx->lt_ctx;
	lane_count = lt->requested_lane_count;
	if (lane_count > RTK_DPRX_MAX_LANES)
		lane_count = RTK_DPRX_MAX_LANES;

	len += sysfs_emit_at(buf, len, "Lane  CR  EQ  SYM  VS  PE\n");
	len += sysfs_emit_at(buf, len, "----  --  --  ---  --  --\n");

	for (i = 0; i < lane_count; i++) {
		len += sysfs_emit_at(buf, len, "  %d    %d   %d   %d    %d   %d\n",
			i,
			lt->lane[i].cr_done ? 1 : 0,
			lt->lane[i].eq_done ? 1 : 0,
			lt->lane[i].symbol_locked ? 1 : 0,
			lt->lane[i].voltage_swing,
			lt->lane[i].pre_emphasis);
	}

	return len;
}
static DEVICE_ATTR_RO(lane_status);

/*============================================================================
 * Sysfs Store Functions (Write-Only / Read-Write Nodes)
 *============================================================================*/

/**
 * reset_store - Reset Link Training state machine
 */
static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!dprx)
		return -ENODEV;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	if (val == 1) {
		ret = rtk_dprx_lt_handle_event(dprx, LT_EVENT_RESET);
		if (ret)
			return ret;
		dev_info(dev, "LT: State machine reset via sysfs\n");
	}

	return count;
}
static DEVICE_ATTR_WO(reset);

/**
 * debug_show - Show debug mode status
 */
static ssize_t debug_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return -ENODEV;

	lt = &dprx->lt_ctx;

	return sysfs_emit(buf, "%d\n", lt->config.debug_mode);
}

/**
 * debug_store - Set debug mode
 */
static ssize_t debug_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	struct rtk_dprx_lt_context *lt;
	unsigned long val;
	int ret;

	if (!dprx)
		return -ENODEV;

	lt = &dprx->lt_ctx;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	lt->config.debug_mode = val ? 1 : 0;
	dev_info(dev, "LT: Debug mode %s\n",
		 lt->config.debug_mode ? "enabled" : "disabled");

	return count;
}
static DEVICE_ATTR_RW(debug);

/**
 * stats_reset_store - Reset statistics
 */
static ssize_t stats_reset_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct rtk_dprx *dprx = dev_get_drvdata(dev);
	struct rtk_dprx_lt_context *lt;
	unsigned long val;
	int ret;

	if (!dprx)
		return -ENODEV;

	lt = &dprx->lt_ctx;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	if (val == 1) {
		memset(&lt->stats, 0, sizeof(lt->stats));
		dev_info(dev, "LT: Statistics reset via sysfs\n");
	}

	return count;
}
static DEVICE_ATTR_WO(stats_reset);

/*============================================================================
 * Sysfs Attribute Group
 *============================================================================*/

static struct attribute *rtk_dprx_lt_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_trained.attr,
	&dev_attr_link_rate.attr,
	&dev_attr_lane_count.attr,
	&dev_attr_error_code.attr,
	&dev_attr_stats.attr,
	&dev_attr_lane_status.attr,
	&dev_attr_reset.attr,
	&dev_attr_debug.attr,
	&dev_attr_stats_reset.attr,
	NULL,
};

static const struct attribute_group rtk_dprx_lt_attr_group = {
	.name = "link_training",
	.attrs = rtk_dprx_lt_attrs,
};

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * rtk_dprx_lt_sysfs_init - Create sysfs nodes
 * @dprx: DPRX instance
 *
 * Creates sysfs attribute group under the device.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_sysfs_init(struct rtk_dprx *dprx)
{
	int ret;

	if (!dprx || !dprx->dev)
		return -EINVAL;

	ret = sysfs_create_group(&dprx->dev->kobj, &rtk_dprx_lt_attr_group);
	if (ret) {
		dev_err(dprx->dev, "LT: Failed to create sysfs group: %d\n", ret);
		return ret;
	}

	dev_dbg(dprx->dev, "LT: Sysfs interface created\n");

	return 0;
}

/**
 * rtk_dprx_lt_sysfs_exit - Remove sysfs nodes
 * @dprx: DPRX instance
 */
void rtk_dprx_lt_sysfs_exit(struct rtk_dprx *dprx)
{
	if (!dprx || !dprx->dev)
		return;

	sysfs_remove_group(&dprx->dev->kobj, &rtk_dprx_lt_attr_group);

	dev_dbg(dprx->dev, "LT: Sysfs interface removed\n");
}
