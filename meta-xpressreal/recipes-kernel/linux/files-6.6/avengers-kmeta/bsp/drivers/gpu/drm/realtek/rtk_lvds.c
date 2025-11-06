/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2024 RealTek Inc
 */

#include <drm/drm_of.h>
#include <drm/drm_print.h>

#include <linux/component.h>
#include <linux/platform_device.h>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/pwm.h>
#include <linux/module.h>

#include <drm/drm_panel.h>
#include <drm/drm_modes.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>

#include <video/of_display_timing.h>
#include <video/display_timing.h>
#include <video/videomode.h>
#include "rtk_drm_drv.h"
#include "rtk_lvds_reg.h"
#include "rtk_lvds.h"

#define LVDS1 1
#define LVDS2 2

struct rtk_lvds;

#define to_rtk_lvds(x) container_of(x, struct rtk_lvds, x)

struct rtk_lvds_data {
	unsigned int id;
	int (*init)(struct rtk_lvds *lvds);
};

struct interface_info {
	unsigned int display_interface;
	unsigned int width;
	unsigned int height;
	unsigned int frame_rate;
	unsigned int mixer;

	// unsigned int panel_id;
	// unsigned int timing_id;
	// unsigned int panel_rotated;
	struct drm_display_mode disp_mode;
	struct videomode videomode;
};

struct rtk_lvds {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_panel *panel;

	struct drm_connector connector;
	struct drm_encoder encoder;
	struct regmap *reg;
	struct clk *clk;
	struct reset_control *rstc;
	struct rtk_rpc_info *rpc_info;
	struct rtk_rpc_info *rpc_info_vo;

	unsigned int lk_initialized;

	struct interface_info if_info;
	const struct rtk_lvds_data *lvds_data;
};

static void rtk_lvds_init(struct rtk_lvds *lvds)
{
	return;
}

static int rtk_lvds1_init(struct rtk_lvds *lvds)
{
	// TODO: LVDS1 init rpc

	DRM_INFO("rtk_lvds1_init\n");

	return 0;
}

static int rtk_lvds2_init(struct rtk_lvds *lvds)
{
	// TODO: LVDS2 init rpc

	DRM_INFO("rtk_lvds2_init\n");

	return 0;
}

static void rtk_lvds_enc_mode_set(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{
	struct rtk_lvds *lvds = to_rtk_lvds(encoder);

	DRM_INFO("rtk_lvds_enc_mode_set\n");

	rtk_lvds_init(lvds);
}

static void rtk_lvds_enc_enable(struct drm_encoder *encoder)
{
	struct rtk_lvds *lvds = to_rtk_lvds(encoder);
	struct rtk_rpc_info *rpc_info = lvds->rpc_info;
	struct rtk_rpc_info *rpc_info_vo = lvds->rpc_info_vo;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	struct rpc_set_display_out_interface interface;
	struct interface_info *if_info;
	struct videomode *videomode;
	int ret;

	// ret = clk_prepare_enable(lvds->clk);
	// if (ret) {
	// 	DRM_ERROR("Failed to enable clk: %d\n", ret);
	// }

	// reset_control_deassert(lvds->rstc);

	if_info = &lvds->if_info;
	videomode = &if_info->videomode;

	ret = lvds->lvds_data->init(lvds);
	if (ret) {
		DRM_ERROR("lvds data initialization failed\n");
		return;
	}

	hw_init_rpc.display_interface = if_info->display_interface;
	hw_init_rpc.enable = 1;
	// hw_init_rpc. = timing_id // LVDS
	// hw_init_rpc.hactive      = videomode->hactive;
	// hw_init_rpc.hfront_porch = videomode->hfront_porch;
	// hw_init_rpc.hback_porch  = videomode->hback_porch;
	// hw_init_rpc.hsync_len    = videomode->hsync_len;
	// hw_init_rpc.vactive      = videomode->vactive;
	// hw_init_rpc.vfront_porch = videomode->vfront_porch;
	// hw_init_rpc.vback_porch  = videomode->vback_porch;
	// hw_init_rpc.vsync_len    = videomode->vsync_len;

	DRM_INFO("[rtk_lvds_enc_enable] enable interface %s\n",
		interface_names[hw_init_rpc.display_interface]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret) {
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");
	}

	if (rpc_info_vo != NULL) {
		interface.display_interface       = if_info->display_interface;
		interface.width                   = if_info->width;
		interface.height                  = if_info->height;
		interface.frame_rate              = if_info->frame_rate;
		interface.display_interface_mixer = if_info->mixer;

		DRM_INFO("[rtk_lvds_enc_enable] enable %s on %s (%dx%d@%d)\n",
			interface_names[interface.display_interface], mixer_names[interface.display_interface_mixer],
			interface.width, interface.height, interface.frame_rate);

		ret = rpc_set_out_interface(rpc_info_vo, &interface);
		if (ret) {
			DRM_ERROR("rpc_set_out_interface rpc fail\n");
		}
	}

	// ret = drm_of_encoder_active_endpoint_id(lvds->dev->of_node, encoder);
	// if (ret < 0) {
	// 	printk(KERN_ALERT"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	// 	return;
	// }

	// printk(KERN_ALERT"active endpoint id %d\n", ret);

	return;
}

static void rtk_lvds_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_lvds *lvds = to_rtk_lvds(encoder);
	struct rtk_rpc_info *rpc_info = lvds->rpc_info;
	struct rtk_rpc_info *rpc_info_vo = lvds->rpc_info_vo;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	struct rpc_set_display_out_interface interface;
	struct interface_info *if_info;
	int ret;

	if_info = &lvds->if_info;

	if (rpc_info_vo != NULL) {
		interface.display_interface       = DISPLAY_INTERFACE_eDP;
		interface.width                   = if_info->width;
		interface.height                  = if_info->height;
		interface.frame_rate              = if_info->frame_rate;
		interface.display_interface_mixer = DISPLAY_INTERFACE_MIXER_NONE;

		DRM_INFO("[rtk_lvds_enc_disable] disable %s on %s\n",
			interface_names[interface.display_interface], mixer_names[if_info->mixer]);

		ret = rpc_set_out_interface(rpc_info_vo, &interface);
		if (ret) {
			DRM_ERROR("rpc_set_out_interface rpc fail\n");
		}
	}

	hw_init_rpc.display_interface = if_info->display_interface;
	hw_init_rpc.enable = 0;

	DRM_INFO("[rtk_lvds_enc_disable] disable interface %s\n",
		interface_names[hw_init_rpc.display_interface]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret) {
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");
	}

	return;
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

static void rtk_lvds_conn_destroy(struct drm_connector *connector)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	drm_connector_cleanup(connector);
}

static int rtk_lvds_conn_get_modes(struct drm_connector *connector)
{
	struct rtk_lvds *lvds = to_rtk_lvds(connector);
	struct drm_display_mode *mode;
	struct rtk_rpc_info *rpc_info = lvds->rpc_info;
	struct rpc_query_display_out_interface_timing interface_timing;
	struct interface_info *if_info;
	struct drm_display_mode *disp_mode;
	struct videomode *videomode;
	// int ret;

	if_info = &lvds->if_info;
	disp_mode = &if_info->disp_mode;
	videomode = &if_info->videomode;

	if (lvds->lvds_data->id == LVDS1) {
		interface_timing.display_interface = DISPLAY_INTERFACE_LVDS1;
		if_info->display_interface  = DISPLAY_INTERFACE_LVDS1;
	} else if (lvds->lvds_data->id == LVDS2) {
		interface_timing.display_interface = DISPLAY_INTERFACE_LVDS2;
		if_info->display_interface  = DISPLAY_INTERFACE_LVDS2;
	}

	DRM_INFO("rtk_lvds_conn_get_modes (%s)\n",
		interface_names[if_info->display_interface]);
/*
	disp_mode->clock       = 2000 * 1111 * 60 / 1000;
	disp_mode->hdisplay    = 1920;
	disp_mode->hsync_start = 1928;
	disp_mode->hsync_end   = 1960;
	disp_mode->htotal      = 2000;
	disp_mode->vdisplay    = 1080;
	disp_mode->vsync_start = 1097;
	disp_mode->vsync_end   = 1105;
	disp_mode->vtotal      = 1111;
*/

	// TODO: query display timing
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

	if_info->frame_rate = interface_timing.framerate;
	if_info->mixer      = interface_timing.mixer;

	DRM_INFO("rtk_lvds_conn_get_modes (%dx%d)@%d on %s\n",
		disp_mode->hdisplay, disp_mode->vdisplay,
		if_info->frame_rate, mixer_names[if_info->mixer]);

	if_info->width  = disp_mode->hdisplay;
	if_info->height = disp_mode->vdisplay;

	drm_display_mode_to_videomode(disp_mode, videomode);

	mode = drm_mode_duplicate(connector->dev, disp_mode);

	if (!mode) {
		DRM_ERROR("bad mode or failed to add mode\n");
		return -EINVAL;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	connector->display_info.width_mm = if_info->disp_mode.width_mm;
	connector->display_info.height_mm = if_info->disp_mode.height_mm;

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
	.destroy = rtk_lvds_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs rtk_lvds_connector_helper_funcs = {
	.get_modes = rtk_lvds_conn_get_modes,
	.mode_valid = rtk_lvds_conn_mode_valid,
};

// int rtk_lvds_parse_dts_timing(struct device_node *np, struct rtk_lvds *lvds)
// {
// 	struct interface_info *if_info;
// 	struct drm_display_mode *disp_mode;
// 	// struct videomode *videomode;
// 	unsigned int panel_id, timing_id, panel_rotated;
// 	int ret;

// 	if_info   = &lvds->if_info;
// 	disp_mode = &if_info->disp_mode;
// 	// videomode = &if_info->videomode;

// 	of_property_read_u32(np, "panel-id", &panel_id);
// 	of_property_read_u32(np, "timing-id", &timing_id);
// 	of_property_read_u32(np, "panel-rotated", &panel_rotated);

// 	DRM_DEBUG_DRIVER("panel_id = %d, timing_id = %d, panel_rotated = %d\n",
// 		panel_id, timing_id, panel_rotated);

// 	ret = of_get_drm_display_mode(np, disp_mode, NULL, OF_USE_NATIVE_MODE);
// 	if (ret) {
// 		DRM_ERROR("of_get_drm_display_mode fail %d\n", ret);
// 		return -EINVAL;
// 	}

// 	disp_mode->width_mm  = disp_mode->hdisplay / 10;
// 	disp_mode->height_mm = disp_mode->vdisplay / 10;

// 	DRM_DEBUG_DRIVER("lvds drm display mode:\n");
// 	DRM_DEBUG_DRIVER("clock       = %d\n", disp_mode->clock);
// 	DRM_DEBUG_DRIVER("hdisplay    = %d\n", disp_mode->hdisplay);
// 	DRM_DEBUG_DRIVER("hsync_start = %d\n", disp_mode->hsync_start);
// 	DRM_DEBUG_DRIVER("hsync_end   = %d\n", disp_mode->hsync_end);
// 	DRM_DEBUG_DRIVER("htotal      = %d\n", disp_mode->htotal);
// 	DRM_DEBUG_DRIVER("vdisplay    = %d\n", disp_mode->vdisplay);
// 	DRM_DEBUG_DRIVER("vsync_start = %d\n", disp_mode->vsync_start);
// 	DRM_DEBUG_DRIVER("vsync_end   = %d\n", disp_mode->vsync_end);
// 	DRM_DEBUG_DRIVER("vtotal      = %d\n", disp_mode->vtotal);
// 	DRM_DEBUG_DRIVER("width_mm    = %d\n", disp_mode->width_mm);
// 	DRM_DEBUG_DRIVER("height_mm   = %d\n", disp_mode->height_mm);
// 	DRM_DEBUG_DRIVER("frame rate = %d\n", disp_mode->clock * 1000 / (disp_mode->htotal * disp_mode->vtotal));

// 	// if_info->panel_id  = panel_id;
// 	// if_info->timing_id = timing_id;
// 	// if_info->panel_rotated = panel_rotated;

// 	// drm_display_mode_to_videomode(disp_mode, videomode);

// 	// DRM_DEBUG_DRIVER("lvds video mode:\n");
// 	// DRM_DEBUG_DRIVER("pixelclock   = %lu\n", videomode->pixelclock);
// 	// DRM_DEBUG_DRIVER("hactive      = %d\n", videomode->hactive);
// 	// DRM_DEBUG_DRIVER("hfront_porch = %d\n", videomode->hfront_porch);
// 	// DRM_DEBUG_DRIVER("hback_porch  = %d\n", videomode->hback_porch);
// 	// DRM_DEBUG_DRIVER("hsync_len    = %d\n", videomode->hsync_len);
// 	// DRM_DEBUG_DRIVER("vactive      = %d\n", videomode->vactive);
// 	// DRM_DEBUG_DRIVER("vfront_porch = %d\n", videomode->vfront_porch);
// 	// DRM_DEBUG_DRIVER("vback_porch  = %d\n", videomode->vback_porch);
// 	// DRM_DEBUG_DRIVER("vsync_len    = %d\n", videomode->vsync_len);

// 	// if (panel_id == DSI_FMT_1920_720P_30 ||
// 	// 	panel_id == DSI_FMT_600_1024P_30 ) {
// 	// 	if_info->frame_rate = 30;
// 	// } else {
// 	// 	if_info->frame_rate = 60;
// 	// }

// 	// DRM_DEBUG_DRIVER("panel_id   = %d\n", if_info->panel_id);
// 	// DRM_DEBUG_DRIVER("timing_id  = %d\n", if_info->timing_id);
// 	// DRM_DEBUG_DRIVER("frame_rate = %d\n", if_info->frame_rate);

// 	return 0;
// }

static int rtk_lvds_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_lvds *lvds = dev_get_drvdata(dev);
	char encoder_name[20];
	char clk_name[20];
	char rstn_name[20];
	unsigned int lvds_id;

	lvds_id = lvds->lvds_data->id;

	snprintf(encoder_name, sizeof(encoder_name), "rtk_lvds%d", lvds_id);
	snprintf(clk_name, sizeof(clk_name), "clk_en_lvds%d", lvds_id);
	snprintf(rstn_name, sizeof(rstn_name), "rstn_lvds%d", lvds_id);

	dev_info(dev, "[rtk_lvds_bind] lvds%d\n", lvds_id);

	// lvds->clk = devm_clk_get(dev, clk_name);

	// printk(KERN_ALERT"lvds->clk : 0x%x\n", lvds->clk);

	// if (IS_ERR(lvds->clk)) {
	// 	dev_err(dev, "failed to get clock\n");
	// 	return PTR_ERR(lvds->clk);
	// }

	// ret = clk_prepare_enable(lvds->clk);
	// if (ret) {
	// 	DRM_ERROR("Failed to enable clk: %d\n", ret);
	// 	return ret;
	// }

	// lvds->rstc = devm_reset_control_get(dev, rstn_name);

	// printk(KERN_ALERT"lvds->rstc : 0x%x\n", lvds->rstc);

	// if (IS_ERR(lvds->rstc)) {
	// 	dev_err(dev, "failed to get reset controller\n");
	// 	return PTR_ERR(lvds->rstc);
	// }
	// reset_control_deassert(lvds->rstc);

	// lvds->reg = syscon_regmap_lookup_by_phandle(dev->of_node, "syscon");
	// if (IS_ERR(lvds->reg)) {
	// 	return PTR_ERR(lvds->reg);
	// }

	of_property_read_u32(dev->of_node, "lk-init", &lvds->lk_initialized);

	// if (of_get_child_by_name(dev->of_node, "display-timings")) {

	// 	ret = rtk_lvds_parse_dts_timing(dev->of_node, lvds);
	// 	if (ret) {
	// 		DRM_ERROR("Failed to parse dts timing : %d\n", ret);
	// 		return ret;
	// 	}
	// } else {
	// 	// TODO: query lvds display-timings
	// 	printk(KERN_ALERT"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	// }

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

	if (priv->krpc_second == 1)
		lvds->rpc_info_vo = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		lvds->rpc_info_vo = NULL;

	dev_info(dev, "lvds->rpc_info (%p), lvds->rpc_info_vo (%p)\n",
		lvds->rpc_info, lvds->rpc_info_vo);

	return 0;
}

static void rtk_lvds_unbind(struct device *dev, struct device *master,
			     void *data)
{

}

static const struct component_ops rtk_lvds_ops = {
	.bind	= rtk_lvds_bind,
	.unbind	= rtk_lvds_unbind,
};

static const struct rtk_lvds_data rtk_lvds1_data = {
	.id = LVDS1,
	.init = rtk_lvds1_init,
};

static const struct rtk_lvds_data rtk_lvds2_data = {
	.id = LVDS2,
	.init = rtk_lvds2_init,
};

static const struct of_device_id rtk_lvds_dt_ids[] = {
	{
		.compatible = "realtek,rtk-lvds1",
		.data = &rtk_lvds1_data
	},
	{
		.compatible = "realtek,rtk-lvds2",
		.data = &rtk_lvds2_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_lvds_dt_ids);

static int rtk_lvds_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_lvds *lvds;
	const struct of_device_id *match;
	// int ret;

	dev_info(dev, "rtk_lvds_probe\n");

	lvds = devm_kzalloc(dev, sizeof(*lvds), GFP_KERNEL);
	if (!lvds)
		return -ENOMEM;

	lvds->dev = dev;
	dev_set_drvdata(dev, lvds);

	match = of_match_node(rtk_lvds_dt_ids, dev->of_node);
	if (!match)
		return -ENODEV;

	lvds->lvds_data = match->data;

	return component_add(&pdev->dev, &rtk_lvds_ops);
}

static int rtk_lvds_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_lvds_ops);
	return 0;
}

struct platform_driver rtk_lvds_driver = {
	.probe  = rtk_lvds_probe,
	.remove = rtk_lvds_remove,
	.driver = {
		.name = "rtk-lvds",
		.of_match_table = rtk_lvds_dt_ids,
	},
};
