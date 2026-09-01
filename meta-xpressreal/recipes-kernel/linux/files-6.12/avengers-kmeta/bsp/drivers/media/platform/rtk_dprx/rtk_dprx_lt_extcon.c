// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek DisplayPort RX Link Training - Extcon/PHY Consumer
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This module consumes extcon and PHY resources from the USB3/TypeC PHY
 * driver. It monitors Type-C connection state via extcon notifications
 * and controls the DP PHY via the kernel PHY framework.
 *
 * Architecture:
 *   USB3/TypeC PHY Driver (provider)
 *     └── extcon: EXTCON_DISP_DP
 *     └── phy: dp_phy_ops
 *
 *   DPRX Driver (consumer) - this module
 *     └── Gets extcon/phy from device tree
 *     └── Registers notifier for connection events
 *     └── Calls phy_init/phy_power_on/phy_configure
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/extcon.h>
#include <linux/phy/phy.h>
#include <linux/ktime.h>

#include <trace/events/rtk_dprx_trace.h>

#include "rtk_dprx.h"
#include "rtk_dprx_link_training.h"
#include "rtk_dprx_lt_extcon.h"

/*============================================================================
 * Type-C / Extcon Consumer
 *============================================================================*/

/**
 * rtk_dprx_lt_handle_plug_in - Handle Type-C DP plug-in event
 * @dprx: DPRX instance
 *
 * Common handler for DP plug-in events. Gets Type-C polarity,
 * initializes PHY, and notifies Link Training state machine.
 *
 * Return: 0 on success, negative on error
 */
static int rtk_dprx_lt_handle_plug_in(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt = &dprx->lt_ctx;
	union extcon_property_value prop;
	int ret;

	/* Get polarity */
	ret = extcon_get_property(lt->edev, EXTCON_DISP_DP,
				  EXTCON_PROP_USB_TYPEC_POLARITY, &prop);
	if (ret == 0) {
		lt->typec_flipped = (prop.intval != 0);
		dev_info(dprx->dev, "Type-C polarity: %s\n",
			 lt->typec_flipped ? "flipped" : "normal");
	}

	dprx->aux_ops->set_pn_swap(dprx, lt->typec_flipped);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_LANE_MUX,
		lt->typec_flipped ? 0xB1 : 0x4E);

	/* Initialize PHY */
	ret = rtk_dprx_lt_phy_init(dprx);
	if (ret) {
		dev_warn(dprx->dev, "PHY init failed on plug-in: %d\n", ret);
		return ret;
	}

	/* Power on PHY */
	ret = rtk_dprx_lt_phy_power_on(dprx);
	if (ret) {
		dev_warn(dprx->dev, "PHY power on failed: %d\n", ret);
		goto err_phy_exit;
	}

	/* Notify Link Training of HPD high */
	rtk_dprx_lt_handle_event(dprx, LT_EVENT_HPD_HIGH);

	/* Notify PHY to assert HPD */
	ret = rtk_dprx_lt_phy_notify_connect(dprx);
	if (ret) {
		dev_warn(dprx->dev, "PHY notify connect failed: %d\n", ret);
		goto err_phy_power_off;
	}

	return 0;

err_phy_power_off:
	rtk_dprx_lt_phy_power_off(dprx);
err_phy_exit:
	rtk_dprx_lt_phy_exit(dprx);
	return ret;
}

/**
 * rtk_dprx_lt_extcon_notifier - Extcon state change callback
 * @nb: Notifier block
 * @event: Event (1 = connected, 0 = disconnected)
 * @ptr: Private data (not used)
 *
 * Called by extcon subsystem when EXTCON_DISP_DP state changes.
 */
static int rtk_dprx_lt_extcon_notifier(struct notifier_block *nb,
				       unsigned long event, void *ptr)
{
	struct rtk_dprx_lt_context *lt;
	struct rtk_dprx *dprx;
	int ret;

	lt = container_of(nb, struct rtk_dprx_lt_context, extcon_nb);
	dprx = container_of(lt, struct rtk_dprx, lt_ctx);

	dev_info(dprx->dev, "Extcon: DISP_DP event=%lu\n", event);

	if (event) {
		/* Connected */
		ret = rtk_dprx_lt_handle_plug_in(dprx);
		if (ret)
			dev_err(dprx->dev, "Plug-in handling failed: %d\n", ret);
	} else {
		/* Disconnected */
		rtk_dprx_lt_handle_event(dprx, LT_EVENT_HPD_LOW);

		/* Notify PHY to de-assert HPD */
		rtk_dprx_lt_phy_notify_disconnect(dprx);

		/* Power off and exit PHY */
		rtk_dprx_lt_phy_power_off(dprx);
		rtk_dprx_lt_phy_exit(dprx);
	}

	return NOTIFY_OK;
}

/**
 * rtk_dprx_lt_extcon_init - Initialize extcon consumer
 * @dprx: DPRX instance
 *
 * Gets extcon device from device tree and registers notifier.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_extcon_init(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	struct device *dev;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;
	dev = dprx->dev;

	/* Get extcon from device tree */
	lt->edev = extcon_get_edev_by_phandle(dev, 0);
	if (IS_ERR(lt->edev)) {
		ret = PTR_ERR(lt->edev);
		if (ret == -ENODEV) {
			dev_info(dev, "No extcon in DT, Type-C monitoring disabled\n");
			lt->edev = NULL;
			return 0;  /* Optional, not an error */
		}
		dev_err(dev, "Failed to get extcon: %d\n", ret);
		lt->edev = NULL;
		return ret;
	}

	/* Register notifier for EXTCON_DISP_DP */
	lt->extcon_nb.notifier_call = rtk_dprx_lt_extcon_notifier;
	ret = extcon_register_notifier(lt->edev, EXTCON_DISP_DP, &lt->extcon_nb);
	if (ret) {
		dev_err(dev, "Failed to register extcon notifier: %d\n", ret);
		lt->edev = NULL;
		return ret;
	}

	/* Check current state */
	if (extcon_get_state(lt->edev, EXTCON_DISP_DP)) {
		dev_info(dev, "Extcon: DP already connected\n");
		ret = rtk_dprx_lt_handle_plug_in(dprx);
		if (ret)
			dev_err(dev, "Plug-in handling failed: %d\n", ret);
	}

	dev_info(dev, "Extcon consumer initialized\n");

	return 0;
}

/**
 * rtk_dprx_lt_extcon_exit - Cleanup extcon consumer
 * @dprx: DPRX instance
 */
void rtk_dprx_lt_extcon_exit(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	if (lt->edev) {
		extcon_unregister_notifier(lt->edev, EXTCON_DISP_DP,
					   &lt->extcon_nb);
		lt->edev = NULL;
		dev_info(dprx->dev, "Extcon consumer cleaned up\n");
	}
}

/*============================================================================
 * PHY Consumer
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_consumer_init - Initialize PHY consumer
 * @dprx: DPRX instance
 *
 * Gets DP PHY from device tree.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_consumer_init(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	struct device *dev;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;
	dev = dprx->dev;

	/* Get PHY from device tree */
	lt->dp_phy = devm_phy_get(dev, "dprx-phy");
	if (IS_ERR(lt->dp_phy)) {
		int ret = PTR_ERR(lt->dp_phy);

		if (ret == -ENODEV || ret == -ENOENT) {
			dev_info(dev, "No dprx-phy in DT, using internal PHY ops\n");
			lt->dp_phy = NULL;
			return 0;  /* Optional, not an error */
		}
		dev_err(dev, "Failed to get dprx-phy: %d\n", ret);
		lt->dp_phy = NULL;
		return ret;
	}

	dev_info(dev, "DP PHY consumer initialized\n");

	return 0;
}

/**
 * rtk_dprx_lt_phy_consumer_exit - Cleanup PHY consumer
 * @dprx: DPRX instance
 */
void rtk_dprx_lt_phy_consumer_exit(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return;

	lt = &dprx->lt_ctx;

	/* PHY is managed by devm, no explicit cleanup needed */
	lt->dp_phy = NULL;
}

/*============================================================================
 * PHY Operations via Kernel PHY Framework
 *============================================================================*/

/**
 * rtk_dprx_lt_phy_init - Initialize the DP PHY
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_init(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	ktime_t start;
	u32 elapsed_us;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	if (!lt->dp_phy) {
		dev_dbg(dprx->dev, "No DP PHY, skipping phy_init\n");
		return 0;
	}

	start = ktime_get();
	ret = phy_init(lt->dp_phy);
	elapsed_us = ktime_us_delta(ktime_get(), start);
	trace_dprx_phy_ops("init", elapsed_us, ret);

	if (ret) {
		dev_err(dprx->dev, "phy_init failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dprx->dev, "DP PHY initialized\n");

	return 0;
}

/**
 * rtk_dprx_lt_phy_exit - De-initialize the DP PHY
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_exit(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	if (!lt->dp_phy)
		return 0;

	ret = phy_exit(lt->dp_phy);
	if (ret)
		dev_err(dprx->dev, "phy_exit failed: %d\n", ret);

	return ret;
}

/**
 * rtk_dprx_lt_phy_power_on - Power on the DP PHY
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_power_on(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	if (!lt->dp_phy) {
		dev_dbg(dprx->dev, "No DP PHY, skipping phy_power_on\n");
		return 0;
	}

	ret = phy_power_on(lt->dp_phy);
	if (ret) {
		dev_err(dprx->dev, "phy_power_on failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dprx->dev, "DP PHY powered on\n");

	return 0;
}

/**
 * rtk_dprx_lt_phy_power_off - Power off the DP PHY
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_power_off(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	if (!lt->dp_phy)
		return 0;

	ret = phy_power_off(lt->dp_phy);
	if (ret)
		dev_err(dprx->dev, "phy_power_off failed: %d\n", ret);

	return ret;
}

/**
 * rtk_dprx_lt_phy_notify_connect - Notify PHY of DP connection (assert HPD)
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_notify_connect(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	if (!lt->dp_phy) {
		dev_dbg(dprx->dev, "No DP PHY, skipping phy_notify_connect\n");
		return 0;
	}

	ret = phy_notify_connect(lt->dp_phy, 0);
	if (ret) {
		dev_err(dprx->dev, "phy_notify_connect failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dprx->dev, "DP PHY notified of connection (HPD asserted)\n");

	return 0;
}

/**
 * rtk_dprx_lt_phy_notify_disconnect - Notify PHY of DP disconnection (de-assert HPD)
 * @dprx: DPRX instance
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_notify_disconnect(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	if (!lt->dp_phy) {
		dev_dbg(dprx->dev, "No DP PHY, skipping phy_notify_disconnect\n");
		return 0;
	}

	ret = phy_notify_disconnect(lt->dp_phy, 0);
	if (ret) {
		dev_err(dprx->dev, "phy_notify_disconnect failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dprx->dev, "DP PHY notified of disconnection (HPD de-asserted)\n");

	return 0;
}

/**
 * rtk_dprx_lt_phy_calibrate - Calibrate the DP PHY for EQ phase
 * @dprx: DPRX instance
 *
 * Called after CR_DONE=1 as preparation for EQ training.
 * This allows PHY driver to perform internal signal quality calibration.
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_calibrate(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;
	ktime_t start;
	u32 elapsed_us;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	if (!lt->dp_phy) {
		dev_dbg(dprx->dev, "No DP PHY, skipping phy_calibrate\n");
		return 0;
	}

	start = ktime_get();

	/*
	 * Enable AUX DEFER mode before PHY calibration.
	 * This makes Source wait when reading DPCD 0x202/0x203
	 * while PHY is being calibrated.
	 */
	if (dprx->aux_ops && dprx->aux_ops->set_manual_mode)
		dprx->aux_ops->set_manual_mode(dprx);

	ret = phy_calibrate(lt->dp_phy);

	/*
	 * Restore AUX to AUTO mode after PHY calibration.
	 * Now PHY is ready to respond to Source's DPCD reads.
	 */
	if (dprx->aux_ops && dprx->aux_ops->set_auto_mode)
		dprx->aux_ops->set_auto_mode(dprx);

	elapsed_us = ktime_us_delta(ktime_get(), start);
	trace_dprx_phy_ops("calibrate", elapsed_us, ret);

	if (ret) {
		dev_err(dprx->dev, "phy_calibrate failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dprx->dev, "DP PHY calibrated (EQ preparation)\n");

	return 0;
}

/**
 * rtk_dprx_lt_phy_configure - Configure DP PHY for link training
 * @dprx: DPRX instance
 * @link_rate: Link rate (DPCD 0x100 value)
 * @lane_count: Number of lanes
 *
 * Return: 0 on success, negative on error
 */
int rtk_dprx_lt_phy_configure(struct rtk_dprx *dprx, u8 link_rate, u8 lane_count)
{
	struct rtk_dprx_lt_context *lt;
	union phy_configure_opts opts;
	struct phy_configure_opts_dp *dp_opts;
	ktime_t start;
	u32 elapsed_us;
	int ret;

	if (!dprx)
		return -EINVAL;

	lt = &dprx->lt_ctx;

	if (!lt->dp_phy) {
		dev_dbg(dprx->dev, "No DP PHY, skipping phy_configure\n");
		return 0;
	}

	memset(&opts, 0, sizeof(opts));
	dp_opts = &opts.dp;

	/* Convert DPCD link rate to link_rate in 10 kbit/s units */
	switch (link_rate) {
	case 0x06:  /* RBR: 1.62 Gbps */
		dp_opts->link_rate = 162000;  /* 162000 * 10 kbit/s = 1.62 Gbps */
		break;
	case 0x0A:  /* HBR: 2.7 Gbps */
		dp_opts->link_rate = 270000;
		break;
	case 0x14:  /* HBR2: 5.4 Gbps */
		dp_opts->link_rate = 540000;
		break;
	case 0x1E:  /* HBR3: 8.1 Gbps */
		dp_opts->link_rate = 810000;
		break;
	default:
		dev_warn(dprx->dev, "Unknown link rate 0x%02x, using HBR2\n",
			 link_rate);
		dp_opts->link_rate = 540000;
		break;
	}

	/* APHY only supports 4-lane and 2-lane; 1-lane uses same config as 2-lane */
	dp_opts->lanes = (lane_count == 1) ? 2 : lane_count;
	dp_opts->set_rate = 1;
	dp_opts->set_lanes = 1;

	start = ktime_get();
	ret = phy_configure(lt->dp_phy, &opts);
	elapsed_us = ktime_us_delta(ktime_get(), start);
	trace_dprx_phy_ops("configure", elapsed_us, ret);

	if (ret) {
		dev_err(dprx->dev, "phy_configure failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dprx->dev, "DP PHY configured: rate=%u lanes=%u\n",
		dp_opts->link_rate, dp_opts->lanes);

	return 0;
}

/*============================================================================
 * Query Functions
 *============================================================================*/

/**
 * rtk_dprx_lt_is_connected - Check if DP source is connected
 * @dprx: DPRX instance
 *
 * Return: true if connected, false otherwise
 */
bool rtk_dprx_lt_is_connected(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;

	if (!lt->edev)
		return false;

	return extcon_get_state(lt->edev, EXTCON_DISP_DP) != 0;
}

/**
 * rtk_dprx_lt_get_typec_polarity - Get Type-C cable polarity
 * @dprx: DPRX instance
 *
 * Return: true if flipped, false if normal
 */
bool rtk_dprx_lt_get_typec_polarity(struct rtk_dprx *dprx)
{
	struct rtk_dprx_lt_context *lt;

	if (!dprx)
		return false;

	lt = &dprx->lt_ctx;

	return lt->typec_flipped;
}
