// SPDX-License-Identifier: GPL-2.0-only
/**
 * Copyright (c) 2024 RealTek Inc
 */

#include <drm/drm_of.h>
#include <drm/drm_print.h>
#include <linux/of_address.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/pwm.h>
#include <linux/module.h>
#include <linux/kthread.h>

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>

#include <video/of_display_timing.h>
#include <video/display_timing.h>
#include "rtk_drm_drv.h"
#include "rtk_lvds_reg.h"
#include "rtk_lvds.h"

struct rtk_lvds;

#define to_rtk_lvds(x) container_of(x, struct rtk_lvds, x)

struct rtk_lvds_platform_data {
	unsigned int reg;
	unsigned int id;
	unsigned int type;
	enum rtk_display_interface display_interface;
};

struct rtk_lvds {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_panel *panel;

	struct drm_display_mode disp_mode;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct regmap *reg;
	struct clk *clk;
	struct reset_control *rstc;
	struct rtk_rpc_info *rpc_info;

	unsigned int lk_initialized;
	unsigned int mixer;

	const struct rtk_lvds_platform_data *lvds_data;

	struct task_struct *hpd_thread;
	bool force_hpd;
	bool connected;
	struct mutex lock;
};

static void rtk_lvds_enc_mode_set(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{

}

static void rtk_lvds_enc_enable(struct drm_encoder *encoder)
{
	struct rtk_lvds *lvds = to_rtk_lvds(encoder);
	struct rtk_rpc_info *rpc_info = lvds->rpc_info;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	int ret;

	hw_init_rpc.display_interface = lvds->lvds_data->display_interface;
	hw_init_rpc.enable = 1;

	DRM_INFO("[rtk_lvds: enc_enable] enable interface %s\n",
		interface_names[hw_init_rpc.display_interface]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret)
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");
}

static void rtk_lvds_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_lvds *lvds = to_rtk_lvds(encoder);
	struct rtk_rpc_info *rpc_info = lvds->rpc_info;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	int ret;

	hw_init_rpc.display_interface = lvds->lvds_data->display_interface;
	hw_init_rpc.enable = 0;

	DRM_INFO("[rtk_lvds: enc_disable] disable interface %s\n",
		interface_names[hw_init_rpc.display_interface]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret)
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");
}

static int rtk_lvds_enc_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	return 0;
}

static const struct drm_encoder_funcs rtk_lvds_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs rtk_lvds_encoder_helper_funcs = {
	.mode_set   = rtk_lvds_enc_mode_set,
	.enable     = rtk_lvds_enc_enable,
	.disable    = rtk_lvds_enc_disable,
	.atomic_check = rtk_lvds_enc_atomic_check,
};

static enum drm_connector_status rtk_lvds_conn_detect
(struct drm_connector *connector, bool force)
{
	struct rtk_lvds *lvds = to_rtk_lvds(connector);
	enum drm_connector_status status = connector_status_disconnected;

	mutex_lock(&lvds->lock);
	if (lvds->connected) {
		dev_dbg(lvds->dev, "rtk lvds connected\n");
		status = connector_status_connected;
	}
	mutex_unlock(&lvds->lock);

	return status;
}

static void rtk_lvds_conn_destroy(struct drm_connector *connector)
{
	drm_connector_cleanup(connector);
}

static int rtk_lvds_conn_get_modes(struct drm_connector *connector)
{
	struct rtk_lvds *lvds = to_rtk_lvds(connector);
	struct drm_display_mode *mode;
	struct rtk_rpc_info *rpc_info = lvds->rpc_info;
	struct rpc_query_display_out_interface_timing interface_timing;
	struct drm_display_mode *disp_mode;

	disp_mode = &lvds->disp_mode;

	interface_timing.display_interface = lvds->lvds_data->display_interface;

	dev_info(lvds->dev, "(%s) get_modes\n",
		interface_names[lvds->lvds_data->display_interface]);

	rpc_query_out_interface_timing(rpc_info, &interface_timing);

	disp_mode->clock       = interface_timing.clock;
	disp_mode->hdisplay    = interface_timing.hdisplay;
	disp_mode->hsync_start = interface_timing.hsync_start;
	disp_mode->hsync_end   = interface_timing.hsync_end;
	disp_mode->htotal      = interface_timing.htotal;
	disp_mode->vdisplay    = interface_timing.vdisplay;
	disp_mode->vsync_start = interface_timing.vsync_start;
	disp_mode->vsync_end   = interface_timing.vsync_end;
	disp_mode->vtotal      = interface_timing.vtotal;
	disp_mode->flags       = 0;
	drm_mode_set_name(disp_mode);

	lvds->mixer = interface_timing.mixer;

	dev_info(lvds->dev, "[lvds][rpc_query_out_interface_timing] (%dx%d)@%d on %s\n",
		disp_mode->hdisplay, disp_mode->vdisplay,
		drm_mode_vrefresh(disp_mode), mixer_names[lvds->mixer]);

	mode = drm_mode_duplicate(connector->dev, disp_mode);

	if (!mode) {
		DRM_ERROR("bad mode or failed to add mode\n");
		return -EINVAL;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	connector->display_info.width_mm = lvds->disp_mode.width_mm;
	connector->display_info.height_mm = lvds->disp_mode.height_mm;

	drm_mode_probed_add(connector, mode);

	return 1;
}

static enum drm_mode_status rtk_lvds_conn_mode_valid(
	struct drm_connector *connector, struct drm_display_mode *mode)
{
	DRM_INFO("\n");

	return MODE_OK;
}

static const struct drm_connector_funcs rtk_lvds_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = rtk_lvds_conn_detect,
	.destroy = rtk_lvds_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs rtk_lvds_connector_helper_funcs = {
	.get_modes = rtk_lvds_conn_get_modes,
	.mode_valid = rtk_lvds_conn_mode_valid,
};

static int rtk_lvds_get_hpd_status(struct rtk_lvds *lvds)
{
	unsigned int val, offset;
	bool connected = false;

	val = readl(ioremap(DUMMY_SERDES_HPD, 0x1));
	offset = lvds->lvds_data->id + SERDES_HPD_LVDS_OFFSET;

	DRM_DEBUG_DRIVER("lvds%d hpd val = 0x%x\n", lvds->lvds_data->id, val);

	if (val & (1 << offset))
		connected = true;

	return connected;
}

/* LVDS detect hpd by dummy register for serdes IC */
static void rtk_lvds_poll_hpd(struct rtk_lvds *lvds)
{
	struct drm_connector *connector = &lvds->connector;
	enum drm_connector_status old_status;
	int val = 0;

	val = rtk_lvds_get_hpd_status(lvds);

	mutex_lock(&lvds->lock);
	lvds->connected = val;
	old_status = connector->status;
	mutex_unlock(&lvds->lock);

	connector->status = connector->funcs->detect(connector, false);

	DRM_DEBUG_DRIVER("old_status = %d, new_status = %d\n",
						old_status, connector->status);

	if (old_status != connector->status) {
		dev_info(lvds->dev, "lvds status changed, send hotplug event\n");
		drm_kms_helper_hotplug_event(lvds->drm_dev);
	}
}

static int rtk_lvds_hpd_thread(void *data)
{
	struct rtk_lvds *lvds = (struct rtk_lvds *) data;

	while (!kthread_should_stop()) {
		rtk_lvds_poll_hpd(lvds);
		msleep_interruptible(LVDS_POLL_HPD_MS);
	}

	return 0;
}

static int rtk_lvds_start_hpd_thread(struct rtk_lvds *lvds)
{
	dev_info(lvds->dev, "lvds: start hpd thread\n");

	lvds->hpd_thread = kthread_run(rtk_lvds_hpd_thread, lvds, "lvds_hpd_thread");
	if (IS_ERR(lvds->hpd_thread)) {
		dev_err(lvds->dev, "Failed to create lvds hpd thread\n");
		lvds->hpd_thread = NULL;
		return PTR_ERR(lvds->hpd_thread);
	}

	return 0;
}

static int rtk_lvds_stop_hpd_thread(struct rtk_lvds *lvds)
{
	dev_info(lvds->dev, "lvds: stop hpd thread\n");

	if (lvds->hpd_thread) {
		kthread_stop(lvds->hpd_thread);
		lvds->hpd_thread = NULL;
	}

	lvds->connected = false;

	return 0;
}

static int rtk_lvds_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_lvds *lvds = dev_get_drvdata(dev);
	char encoder_name[20];

	lvds->drm_dev = drm;

	dev_info(dev, "[rtk_lvds: bind] lvds%d\n", lvds->lvds_data->id);

	snprintf(encoder_name, sizeof(encoder_name), "rtk_lvds%d", lvds->lvds_data->id);

	lvds->reg = syscon_regmap_lookup_by_phandle(dev->of_node, "syscon");
	if (IS_ERR(lvds->reg)) {
		return PTR_ERR(lvds->reg);
	}

	of_property_read_u32(dev->of_node, "lk-init", &lvds->lk_initialized);

	lvds->force_hpd = of_property_read_bool(dev->of_node, "force-hpd");

	encoder = &lvds->encoder;
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	dev_info(dev, "lvds possible_crtcs (0x%x)\n", encoder->possible_crtcs);

	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	drm_encoder_init(drm, encoder, &rtk_lvds_encoder_funcs,
			 DRM_MODE_ENCODER_LVDS, encoder_name);

	drm_encoder_helper_add(encoder, &rtk_lvds_encoder_helper_funcs);

	connector = &lvds->connector;
	drm_connector_init(drm, connector, &rtk_lvds_connector_funcs,
			   DRM_MODE_CONNECTOR_LVDS);
	drm_connector_helper_add(connector, &rtk_lvds_connector_helper_funcs);

	drm_connector_attach_encoder(connector, encoder);

	lvds->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	dev_info(dev, "lvds->rpc_info (%p),\n", lvds->rpc_info);

	if (lvds->force_hpd) {
		rtk_lvds_start_hpd_thread(lvds);
	} else
		lvds->connected = true;

	return 0;
}

static void rtk_lvds_unbind(struct device *dev, struct device *master,
			     void *data)
{

}

static int rtk_lvds_suspend(struct device *dev)
{
	struct rtk_lvds *lvds = dev_get_drvdata(dev);

	if (!lvds)
		return 0;

	dev_info(lvds->dev, "lvds: suspend\n");

	if (lvds->force_hpd)
		rtk_lvds_stop_hpd_thread(lvds);

	return 0;
}

static int rtk_lvds_resume(struct device *dev)
{
	struct rtk_lvds *lvds = dev_get_drvdata(dev);

	if (!lvds)
		return 0;

	dev_info(lvds->dev, "lvds: resume\n");

	if (lvds->force_hpd)
		rtk_lvds_start_hpd_thread(lvds);

	return 0;
}

static const struct component_ops rtk_lvds_ops = {
	.bind	= rtk_lvds_bind,
	.unbind	= rtk_lvds_unbind,
};

static const struct rtk_lvds_platform_data rtk_lvds_data[] = {
	{
		.reg = 0x9812f000,
		.id = LVDS1,
		.type = RTK_AUTOMOTIVE_TYPE,
		.display_interface = DISPLAY_INTERFACE_LVDS1,
	},
	{
		.reg = 0x9812f800,
		.id = LVDS2,
		.type = RTK_AUTOMOTIVE_TYPE,
		.display_interface = DISPLAY_INTERFACE_LVDS2,
	}
};

static const struct dev_pm_ops rtk_lvds_pm_ops = {
	.suspend = rtk_lvds_suspend,
	.resume  = rtk_lvds_resume,
};

static const struct of_device_id rtk_lvds_dt_ids[] = {
	{
		.compatible = "realtek,rtk-lvds",
		.data = &rtk_lvds_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_lvds_dt_ids);

static int rtk_lvds_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct resource res;
	struct rtk_lvds *lvds;
	const struct rtk_lvds_platform_data *lvds_data = of_device_get_match_data(dev);
	int ret, i;

	dev_info(dev, "rtk_lvds: probe\n");

	lvds = devm_kzalloc(dev, sizeof(*lvds), GFP_KERNEL);
	if (!lvds)
		return -ENOMEM;

	dev_set_drvdata(dev, lvds);
	lvds->dev = dev;

	np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (!np) {
		dev_err(dev, "Failed to parse syscon phandle\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		dev_err(dev, "Failed to get resource from syscon node\n");
		of_node_put(np);
		return ret;
	}

	i = 0;
	while (lvds_data[i].reg) {
		if (lvds_data[i].reg == res.start) {
			lvds->lvds_data = &lvds_data[i];
			dev_info(dev, "Find LVDS%d\n", i);
			break;
		}

		i++;
	}

	if (!lvds->lvds_data) {
		dev_err(dev, "no lvds config for %s node\n", np->name);
		return -EINVAL;
	}

	mutex_init(&lvds->lock);

	of_node_put(np);

	return component_add(&pdev->dev, &rtk_lvds_ops);
}

static void rtk_lvds_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_lvds_ops);
}

struct platform_driver rtk_lvds_driver = {
	.probe  = rtk_lvds_probe,
	.remove = rtk_lvds_remove,
	.driver = {
		.name = "rtk-lvds",
		.of_match_table = rtk_lvds_dt_ids,
#if IS_ENABLED(CONFIG_PM)
		.pm = &rtk_lvds_pm_ops,
#endif
	},
};
