// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Realtek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>

#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/hwspinlock.h>

#ifdef CONFIG_CHROME_PLATFORMS
#include <linux/of_reserved_mem.h>
#endif

#include "rtk_drm_drv.h"
#include "rtk_drm_crtc.h"
#include "rtk_drm_plane.h"

#define to_rtk_crtc(s) container_of(s, struct rtk_drm_crtc, crtc)
#define to_rtk_crtc_state(s) container_of(s, struct rtk_crtc_state, base)

#ifdef CONFIG_CHROME_PLATFORMS
static int rtk_crtc_set_mixer_order(struct rtk_rpc_info *rpc_info,
               struct rpc_disp_mixer_order *mixer_order)
{
   int ret = 0;

   if(!rpc_info)
       return -1;

   ret = rpc_set_mixer_order(rpc_info, mixer_order);

   return ret;
}
#endif

static const struct drm_prop_enum_list panel_usage_names[] = {
	{ IVI, "IVI" },
	{ RSE, "RSE" },
	{ COPILOT, "COPILOT" },
	{ CLUSTER, "CLUSTER" },
	{ CHROME, "CHROME" },
	{ BOX, "BOX" },
	{ NONE, "NONE" },
};

int rtk_crtc_ioctl_get_mixer_id(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_rtk_mixer_id *rtk_mixer_id = (struct drm_rtk_mixer_id *) data;
	struct rtk_drm_crtc *rtk_crtc;
	struct drm_crtc *crtc;

	DRM_INFO("rtk_crtc_ioctl_get_mixer_id\n");

	crtc = drm_crtc_find(dev, file, rtk_mixer_id->crtc_id);
	if (!crtc)
		return -ENOENT;

	rtk_crtc = to_rtk_crtc(crtc);

	DRM_INFO("crtc id [%d], mixer id : %d\n", rtk_mixer_id->crtc_id, rtk_crtc->mixer);

	rtk_mixer_id->mixer_id = rtk_crtc->mixer;

	return 0;
}

int rtk_crtc_get_display_panel_usage_by_mixer(struct rtk_drm_crtc *rtk_crtc)
{
	struct rtk_rpc_info *rpc_info = rtk_crtc->rpc_info;
	struct rpc_query_display_panel_usage panel_usage;
	int ret = 0;

	DRM_INFO("rtk_crtc_get_display_panel_usage_by_mixer\n");

	panel_usage.display_interface_mixer = rtk_crtc->mixer;

	ret = rpc_query_display_panel_usage(rpc_info, &panel_usage);
	if (ret) {
		DRM_ERROR("rpc_query_display_panel_usage RPC fail\n");
		return -1;
	}

	rtk_crtc->display_panel_usage = panel_usage.display_panel_usage;

	DRM_INFO("%s is for %s\n",
		mixer_names[rtk_crtc->mixer], panel_usage_names[rtk_crtc->display_panel_usage].name);

	return 0;
}

static int rtk_crtc_rpc_bind(struct rtk_drm_crtc *rtk_crtc, struct rtk_drm_private *priv)
{
	DRM_INFO("rtk_crtc_rpc_bind\n");

	rtk_crtc_get_display_panel_usage_by_mixer(rtk_crtc);

	if (rtk_crtc->display_panel_usage == CLUSTER ||
		rtk_crtc->display_panel_usage == CHROME) {
		rtk_crtc->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];
		DRM_INFO("%s send rpc to risc-v vo : %p\n",
			mixer_names[rtk_crtc->mixer], rtk_crtc->rpc_info);
	} else { // IVI, RSE, COPILOT, BOX, NONE
		rtk_crtc->rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
		DRM_INFO("%s send rpc to hifi vo : %p\n",
			mixer_names[rtk_crtc->mixer], rtk_crtc->rpc_info);
	}

	return 0;
}

static struct drm_crtc_state *rtk_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct rtk_crtc_state *state;

	if (WARN_ON(!crtc->state))
		return NULL;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);

	WARN_ON(state->base.crtc != crtc);
	state->base.crtc = crtc;

	return &state->base;
}

static void rtk_crtc_destroy_state(struct drm_crtc *crtc,
				   struct drm_crtc_state *state)
{
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(to_rtk_crtc_state(state));
}

static int rtk_crtc_atomic_get_property(struct drm_crtc *crtc,
					const struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	struct rtk_drm_crtc *rtk_crtc = to_rtk_crtc(crtc);

	if (property == rtk_crtc->present_time_prop) {
		*val = rtk_crtc->present_time;
		return 0;
	}

	if (property == rtk_crtc->display_panel_usage_prop) {
		*val = rtk_crtc->display_panel_usage;
		return 0;
	}
	return 0;
}

static int rtk_crtc_atomic_set_property(struct drm_crtc *crtc,
					struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t val)
{
	struct rtk_drm_crtc *rtk_crtc = to_rtk_crtc(crtc);
	const struct drm_display_mode *mode = &state->mode;
	u64 tolerance;

	if (property == rtk_crtc->present_time_prop) {
		tolerance = (u64)((mode->htotal * mode->vtotal)/(mode->clock)) * 500000;
		rtk_crtc->present_time = val - tolerance;
		rtk_crtc->present_time_en = 1;
		return 0;
	}
	if (property == rtk_crtc->display_panel_usage_prop)
		return 0;
	return 0;
}

static int rtk_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct rtk_drm_crtc *rtk_crtc = to_rtk_crtc(crtc);
	struct rtk_rpc_info *rpc_info = rtk_crtc->rpc_info;
	unsigned long flags;
	unsigned int val;
	unsigned int notify;

	if (rpc_info->hwlock)
		hwspin_lock_timeout_irqsave(rpc_info->hwlock, UINT_MAX, &flags);

	notify = DC_VO_SET_NOTIFY << (rtk_crtc->mixer * 2);

	DRM_DEBUG_DRIVER("enable crtc[%d]-%s, send %s notify(0x%x)\n",
			crtc->index, mixer_names[rtk_crtc->mixer],
			krpc_names[rpc_info->krpc_vo_opt], notify);

	val = readl(rpc_info->vo_sync_flag);

	if (rpc_info->krpc_vo_opt != RPC_AUDIO)
		val |= notify;
	else
		val |= __cpu_to_be32(notify);

	writel(val, rpc_info->vo_sync_flag);

	if (rpc_info->hwlock)
		hwspin_unlock_irqrestore(rpc_info->hwlock, &flags);

	return 0;
}

static void rtk_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct rtk_drm_crtc *rtk_crtc = to_rtk_crtc(crtc);
	struct rtk_rpc_info *rpc_info = rtk_crtc->rpc_info;
	unsigned long flags;
	unsigned int val;
	unsigned int notify;

	if (rpc_info->hwlock)
		hwspin_lock_timeout_irqsave(rpc_info->hwlock, UINT_MAX, &flags);

	notify = DC_VO_SET_NOTIFY << (rtk_crtc->mixer * 2);

	DRM_DEBUG_DRIVER("disable crtc[%d]-%s, send %s notify(0x%x)\n",
			crtc->index, mixer_names[rtk_crtc->mixer],
			krpc_names[rpc_info->krpc_vo_opt], notify);

	val = readl(rpc_info->vo_sync_flag);

	if (rpc_info->krpc_vo_opt != RPC_AUDIO)
		val &= ~notify;
	else
		val &= ~(__cpu_to_be32(notify));

	writel(val, rpc_info->vo_sync_flag);

	if (rpc_info->hwlock)
		hwspin_unlock_irqrestore(rpc_info->hwlock, &flags);
}

static void rtk_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

void rtk_crtc_finish_page_flip(struct drm_crtc *crtc)
{
	struct rtk_drm_crtc *rtk_crtc = to_rtk_crtc(crtc);
	struct rtk_drm_plane *rtk_plane = &rtk_crtc->nplanes[0];
	struct drm_device *drm = crtc->dev;
	unsigned long flags;
	int i;
	s64 completed_time_ms;

	spin_lock_irqsave(&drm->event_lock, flags);
	if ((rtk_plane->pending_planes || rtk_crtc->pending_needs_vblank)) {
		if (rtk_plane_check_update_done(&rtk_crtc->nplanes[0])) {

			completed_time_ms = ktime_ms_delta(ktime_get(), rtk_crtc->begin);
			if (completed_time_ms > 33)
				DRM_DEBUG_DRIVER("event (%p) flip time (%lld) completed_time_ms (%lld)"
					"pending_planes = %d, pending_needs_vblank = %d\n",
					rtk_crtc->event, ktime_get(), completed_time_ms,
					rtk_plane->pending_planes, rtk_crtc->pending_needs_vblank);

			if (rtk_crtc->event) {
				drm_crtc_send_vblank_event(crtc, rtk_crtc->event);
				drm_crtc_vblank_put(crtc);
			}

			rtk_crtc->event = NULL;
			rtk_crtc->pending_needs_vblank = false;

			for (i = 0; i < rtk_crtc->plane_count; i++)
				if(rtk_crtc->nplanes[i].rtk_fence)
					rtk_drm_fence_update(&rtk_crtc->nplanes[i]);

			if (rtk_plane->pending_planes) {
				DRM_DEBUG_DRIVER("clear pending_planes\n");
				rtk_plane->pending_planes = false;
			}
		}
	}
	spin_unlock_irqrestore(&drm->event_lock, flags);
}

static const struct drm_crtc_funcs rtk_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = rtk_crtc_destroy,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = rtk_crtc_duplicate_state,
	.atomic_destroy_state = rtk_crtc_destroy_state,
	.atomic_set_property = rtk_crtc_atomic_set_property,
	.atomic_get_property = rtk_crtc_atomic_get_property,
	.enable_vblank = rtk_crtc_enable_vblank,
	.disable_vblank = rtk_crtc_disable_vblank,
};

static bool rtk_crtc_mode_fixup(struct drm_crtc *crtc,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	DRM_DEBUG_DRIVER("%d\n", __LINE__);
	return true;
}

static int rtk_crtc_atomic_check(struct drm_crtc *crtc,
				struct drm_atomic_state *old_crtc_state)
{
	struct rtk_drm_crtc *rtk_crtc = to_rtk_crtc(crtc);
	struct vo_color blueBorder = {0, 0, 255, 1};
	int i;

	if (crtc->state->mode_changed || crtc->state->active_changed ||
			crtc->state->connectors_changed) {
		for (i = 0; i < rtk_crtc->plane_count; i++) {
			struct rtk_drm_plane *rtk_plane = &rtk_crtc->nplanes[i];

			DRM_DEBUG_DRIVER("crtc %d mode/active/connector changed, reconfig %s\n",
				crtc->index, plane_names[rtk_plane->layer_nr]);

			rtk_plane->disp_win.videoWin.x = 0;
			rtk_plane->disp_win.videoWin.y = 0;
			rtk_plane->disp_win.videoWin.width = 0;
			rtk_plane->disp_win.videoWin.height = 0;

			if (rtk_plane->display_panel_usage == CLUSTER) {
				DRM_INFO("cluster init %s disp win on %s\n",
					plane_names[rtk_plane->layer_nr], mixer_names[rtk_plane->mixer]);

				rtk_plane->disp_win.videoPlane = rtk_plane->layer_nr | (rtk_plane->mixer << 16);
				rtk_plane->disp_win.borderColor = blueBorder;
				rtk_plane->disp_win.enBorder = 0;

				if (rpc_video_config_disp_win(rtk_crtc->rpc_info, &rtk_plane->disp_win)) {
					DRM_ERROR("rpc_video_config_disp_win RPC fail\n");
					return -1;
				}
			}
		}
	}

	return 0;
}

static void rtk_crtc_atomic_flush(struct drm_crtc *crtc,
				struct drm_atomic_state *old_crtc_state)
{
	struct rtk_drm_crtc *rtk_crtc = to_rtk_crtc(crtc);
	// struct rtk_rpc_info *rpc_info = rtk_crtc->rpc_info;
	struct drm_device *drm = crtc->dev;
	unsigned long flags;

	DRM_DEBUG_DRIVER("%d\n", __LINE__);

	spin_lock_irqsave(&drm->event_lock, flags);
	if (rtk_crtc->event) {
		rtk_crtc->pending_needs_vblank = true;

		DRM_DEBUG_DRIVER("pending crtc event\n");
		// rpc_send_interrupt(rpc_info);
	}
	spin_unlock_irqrestore(&drm->event_lock, flags);
}

static void rtk_crtc_atomic_begin(struct drm_crtc *crtc,
				struct drm_atomic_state *old_crtc_state)
{
	struct rtk_drm_crtc *rtk_crtc = to_rtk_crtc(crtc);
	struct rtk_crtc_state *state = to_rtk_crtc_state(crtc->state);

	if (rtk_crtc->event && state->base.event)
		DRM_ERROR("new event while there is still a pending event\n");

	if (state->base.event) {
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		rtk_crtc->event = state->base.event;
		state->base.event = NULL;
		rtk_crtc->begin = ktime_get();
		DRM_DEBUG_DRIVER("crtc[%d] event (%p) begin (%lld)\n",
			crtc->index, rtk_crtc->event, rtk_crtc->begin);
	}
}

static void rtk_crtc_atomic_enable(struct drm_crtc *crtc,
				struct drm_atomic_state *old_crtc_state)
{
	DRM_DEBUG_DRIVER("%d\n", __LINE__);
	drm_crtc_vblank_on(crtc);
}

static void rtk_crtc_atomic_disable(struct drm_crtc *crtc,
				struct drm_atomic_state *old_crtc_state)
{
	DRM_DEBUG_DRIVER("%d\n", __LINE__);

	if (crtc->state->event) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
		spin_unlock_irq(&crtc->dev->event_lock);
	}

	drm_crtc_vblank_off(crtc);
}

static const struct drm_crtc_helper_funcs rtk_crtc_helper_funcs = {
	.mode_fixup = rtk_crtc_mode_fixup,
	.atomic_check = rtk_crtc_atomic_check,
	.atomic_flush = rtk_crtc_atomic_flush,
	.atomic_begin = rtk_crtc_atomic_begin,
	.atomic_enable = rtk_crtc_atomic_enable,
	.atomic_disable = rtk_crtc_atomic_disable,
};

irqreturn_t rtk_crtc_isr(int irq, void *dev_id)
{
	struct rtk_drm_crtc *rtk_crtc = (struct rtk_drm_crtc *)dev_id;
	struct rtk_rpc_info *rpc_info = rtk_crtc->rpc_info;
	struct drm_crtc *crtc = &rtk_crtc->crtc;
	unsigned long flags;
	unsigned int feedback_notify;

	if (rpc_info->krpc_vo_opt != RPC_AUDIO) {
		feedback_notify = 1U << (rtk_crtc->mixer * 2 + 1);
	} else {
		feedback_notify = __cpu_to_be32(1U << (rtk_crtc->mixer * 2 + 1));
	}

	if (!DC_HAS_BIT(rpc_info->vo_sync_flag, feedback_notify)) {
		return IRQ_HANDLED;
	}

	if (rpc_info->hwlock)
		hwspin_lock_timeout_irqsave(rpc_info->hwlock, UINT_MAX, &flags);

	DC_RESET_BIT(rpc_info->vo_sync_flag, feedback_notify);

	if (rpc_info->hwlock)
		hwspin_unlock_irqrestore(rpc_info->hwlock, &flags);

	drm_crtc_handle_vblank(crtc);

	if (!rtk_crtc->present_time_en) {
		rtk_crtc_finish_page_flip(crtc);
	} else if (ktime_get() >= rtk_crtc->present_time) {
		rtk_crtc_finish_page_flip(crtc);
		rtk_crtc->present_time_en = 0;
	}

	rtk_drm_vowb_isr(rtk_crtc->crtc.dev);

	return IRQ_HANDLED;
}

static int rtk_crtc_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm = data;
	struct rtk_drm_private *priv = drm->dev_private;
	struct device_node *port;
	struct rtk_drm_crtc *rtk_crtc;
	const struct crtc_data *crtc_data;
	struct drm_plane *primary = NULL, *cursor = NULL, *plane, *tmp;
	unsigned int display_type = 0;
	int i, ret;

	dev_info(dev, "rtk_crtc_bind\n");

	crtc_data = of_device_get_match_data(dev);
	if (!crtc_data)
		return -ENODEV;

#ifdef CONFIG_CHROME_PLATFORMS
	ret = of_reserved_mem_device_init(dev);
	if (ret)
		dev_warn(dev, "init reserved memory failed");
#endif

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port)
		return -ENOENT;

	rtk_crtc = devm_kzalloc(dev, sizeof(*rtk_crtc), GFP_KERNEL);
	if (!rtk_crtc)
		return -ENOMEM;

	rtk_crtc->nplanes = devm_kcalloc(dev, crtc_data->plane_size,
					sizeof(struct rtk_drm_plane), GFP_KERNEL);
	if (!rtk_crtc->nplanes)
		return -ENOMEM;

	rtk_crtc->plane_count = crtc_data->plane_size;

	rtk_crtc->dev = dev;
	dev_set_drvdata(dev, rtk_crtc);

	rtk_crtc->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];
	memset(&rtk_crtc->mixer_order, 0, sizeof(struct rpc_disp_mixer_order));

	display_type = priv->display_type;

	dev_info(dev, "rtk_crtc->rpc_info : %p\n", rtk_crtc->rpc_info);

	for (i = 0; i < crtc_data->plane_size; i++) {
		const struct crtc_plane_data *plane = &crtc_data->plane[i];
		struct rtk_drm_plane *rtk_plane = &rtk_crtc->nplanes[i];

		if (plane->type != DRM_PLANE_TYPE_PRIMARY &&
		    plane->type != DRM_PLANE_TYPE_CURSOR)
			continue;

		if (plane->type == DRM_PLANE_TYPE_PRIMARY) {
			rtk_plane_get_mixer_by_videoplane(rtk_crtc, plane->layer_nr);

			if (crtc_data->rpc_bind && display_type == 1) {
				rtk_plane_get_mixer_from_cluster(rtk_crtc, plane->layer_nr);
			}
		}

		if (rtk_crtc->mixer == DISPLAY_INTERFACE_MIXER_NONE) {
			rtk_crtc->mixer = crtc_data->mixer;
			dev_info(dev, "%s use %s for default\n",
				plane_names[plane->layer_nr], mixer_names[crtc_data->mixer]);
		}

		rtk_crtc->nplanes[i].mixer = rtk_crtc->mixer;
		if (crtc_data->rpc_bind) {
			crtc_data->rpc_bind(rtk_crtc, priv);
			rtk_plane->display_panel_usage = rtk_crtc->display_panel_usage;
		} else {
			rtk_crtc->display_panel_usage  = NONE;
			rtk_plane->display_panel_usage = NONE;
		}

		dev_info(dev, "plane usage(%d), crtc_usage (%d)\n",
			rtk_plane->display_panel_usage, rtk_crtc->display_panel_usage);

		ret = rtk_plane_init(drm, &rtk_crtc->nplanes[i],
					0, plane->type, plane->layer_nr, rtk_crtc->rpc_info);
		if (ret) {
			DRM_DEV_ERROR(dev, "rtk plane init fail\n");
			goto err_cleanup_plane;
		}

		if (plane->type == DRM_PLANE_TYPE_PRIMARY)
			primary = &rtk_crtc->nplanes[i].plane;
		else
			cursor = &rtk_crtc->nplanes[i].plane;

#ifdef CONFIG_CHROME_PLATFORMS
		if (plane->layer_nr == VO_VIDEO_PLANE_OSD1) {
			rtk_crtc->mixer_order.osd1 = plane->plane_order;
		} else if (plane->layer_nr == VO_VIDEO_PLANE_SUB1) {
			rtk_crtc->mixer_order.sub1 = plane->plane_order;
		} else if (plane->layer_nr == VO_VIDEO_PLANE_OSD3) {
			rtk_crtc->mixer_order.osd3 = plane->plane_order;
		}  else if (plane->layer_nr == VO_VIDEO_PLANE_OSD4) {
			rtk_crtc->mixer_order.osd4 = plane->plane_order;
		}
#endif
	}

	ret = drm_crtc_init_with_planes(drm, &rtk_crtc->crtc, primary, cursor,
					&rtk_crtc_funcs, NULL);
	if (ret)
		goto err_cleanup_crtc;

	drm_crtc_helper_add(&rtk_crtc->crtc, &rtk_crtc_helper_funcs);

	for (i = 0; i < crtc_data->plane_size; i++) {
		const struct crtc_plane_data *plane = &crtc_data->plane[i];

		if (plane->type != DRM_PLANE_TYPE_OVERLAY)
			continue;

		if (rtk_crtc->mixer == DISPLAY_INTERFACE_MIXER_NONE) {
			rtk_crtc->mixer = crtc_data->mixer;
			dev_info(dev, "%s use %s for default\n",
				plane_names[plane->layer_nr], mixer_names[crtc_data->mixer]);
		}

		rtk_crtc->nplanes[i].mixer = rtk_crtc->mixer;

		ret = rtk_plane_init(drm, &rtk_crtc->nplanes[i],
				     1 << drm_crtc_index(&rtk_crtc->crtc),
				     plane->type, plane->layer_nr, rtk_crtc->rpc_info);
		if (ret) {
			DRM_DEV_ERROR(dev, "rtk plane init fail\n");
			goto err_cleanup_crtc;
		}


#ifdef CONFIG_CHROME_PLATFORMS
		if (plane->layer_nr == VO_VIDEO_PLANE_V1) {
			rtk_crtc->mixer_order.v1 = plane->plane_order;
		} else if (plane->layer_nr == VO_VIDEO_PLANE_V2) {
			rtk_crtc->mixer_order.v2 = plane->plane_order;
		}
#endif
	}

#ifdef CONFIG_CHROME_PLATFORMS
	if (rtk_crtc_set_mixer_order(rtk_crtc->rpc_info, &rtk_crtc->mixer_order)) {
		DRM_ERROR("rtk crtc set mixer order fail\n");
		return -1;
	}
#endif

	rtk_crtc->crtc.port = port;
	dev_info(dev, "rtk crtc port (%p)\n", port);

	if (rtk_crtc->rpc_info->krpc_vo_opt != RPC_HIFI)
		rtk_crtc->irq = platform_get_irq(pdev, 0);
	else
		rtk_crtc->irq = platform_get_irq(pdev, 1);
	if (rtk_crtc->irq < 0) {
		DRM_DEV_ERROR(dev, "can't find irq for rtk crtc\n");
		return rtk_crtc->irq;
	}

	ret = devm_request_irq(dev, rtk_crtc->irq, rtk_crtc_isr,
				IRQF_SHARED | IRQF_NO_SUSPEND,
				"crtc_irq", rtk_crtc);
	if (ret) {
		DRM_DEV_ERROR(dev, "can't request crtc irq\n");
		goto err_cleanup_all;
	}

	rtk_crtc->present_time_prop = drm_property_create_range(drm, DRM_MODE_PROP_ATOMIC,
					"expectedPresentTime", 0, 0xffffffffffffffff);
	drm_object_attach_property(&rtk_crtc->crtc.base, rtk_crtc->present_time_prop, 0);

	rtk_crtc->display_panel_usage_prop = drm_property_create_enum(drm, 0, "display panel name",
				panel_usage_names, ARRAY_SIZE(panel_usage_names));
	drm_object_attach_property(&rtk_crtc->crtc.base, rtk_crtc->display_panel_usage_prop, 0);

	return 0;

err_cleanup_all:
	of_node_put(port);
err_cleanup_crtc:
	drm_crtc_cleanup(&rtk_crtc->crtc);
err_cleanup_plane:
	list_for_each_entry_safe(plane, tmp, &drm->mode_config.plane_list, head)
		rtk_plane_destroy(plane);

	return ret;
}

static void
rtk_crtc_unbind(struct device *dev, struct device *master, void *data)
{
	struct rtk_drm_crtc *rtk_crtc = dev_get_drvdata(dev);
	struct drm_crtc *crtc = &rtk_crtc->crtc;
	struct drm_device *drm = rtk_crtc->crtc.dev;
	struct drm_plane *plane, *tmp;

	list_for_each_entry_safe(plane, tmp, &drm->mode_config.plane_list, head)
		rtk_plane_destroy(plane);

	of_node_put(crtc->port);
	drm_crtc_cleanup(crtc);
}

const struct component_ops rtk_crtc_component_ops = {
	.bind = rtk_crtc_bind,
	.unbind = rtk_crtc_unbind,
};

static int rtk_crtc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	return component_add(dev, &rtk_crtc_component_ops);
}

static int rtk_crtc_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_crtc_component_ops);
	return 0;
}

static const struct crtc_plane_data rtd_crtc_plane_all[] = {
	{ .layer_nr = VO_VIDEO_PLANE_OSD1,
	  .type = DRM_PLANE_TYPE_PRIMARY },
	{ .layer_nr = VO_VIDEO_PLANE_SUB1,
	  .type = DRM_PLANE_TYPE_CURSOR },
	{ .layer_nr = VO_VIDEO_PLANE_V1,
	  .type = DRM_PLANE_TYPE_OVERLAY },
	{ .layer_nr = VO_VIDEO_PLANE_V2,
	  .type = DRM_PLANE_TYPE_OVERLAY },
};

#ifdef CONFIG_CHROME_PLATFORMS
static const struct crtc_plane_data rtd_crtc_plane_data_main[] = {
	{ .layer_nr = VO_VIDEO_PLANE_OSD1,
	  .type = DRM_PLANE_TYPE_PRIMARY,
	  .plane_order = 2 },
	{ .layer_nr = VO_VIDEO_PLANE_OSD3,
	  .type = DRM_PLANE_TYPE_CURSOR,
	  .plane_order = 3 },
};
#else
static const struct crtc_plane_data rtd_crtc_plane_data_main[] = {
	{ .layer_nr = VO_VIDEO_PLANE_OSD1,
	  .type = DRM_PLANE_TYPE_PRIMARY,
	  .plane_order = 2 },
};
#endif

static const struct crtc_plane_data rtd_crtc_plane_main[] = {
	{ .layer_nr = VO_VIDEO_PLANE_OSD1,
	  .type = DRM_PLANE_TYPE_PRIMARY,
	  .plane_order = 2 },
};

#ifdef CONFIG_CHROME_PLATFORMS
static const struct crtc_plane_data rtd_crtc_plane_data_ext[] = {
	{ .layer_nr = VO_VIDEO_PLANE_SUB1,
	  .type = DRM_PLANE_TYPE_PRIMARY,
	  .plane_order = 2 },
	{ .layer_nr = VO_VIDEO_PLANE_OSD4,
	  .type = DRM_PLANE_TYPE_CURSOR,
	  .plane_order = 3 },
};
#else
static const struct crtc_plane_data rtd_crtc_plane_data_ext[] = {
	{ .layer_nr = VO_VIDEO_PLANE_SUB1,
	  .type = DRM_PLANE_TYPE_PRIMARY,
	  .plane_order = 2 },
};
#endif

static const struct crtc_plane_data rtd_crtc_plane_ext[] = {
	{ .layer_nr = VO_VIDEO_PLANE_SUB1,
	  .type = DRM_PLANE_TYPE_PRIMARY,
	  .plane_order = 2 },
};

#ifdef CONFIG_CHROME_PLATFORMS
static const struct crtc_plane_data rtd_crtc_plane_data_third[] = {
	{ .layer_nr = VO_VIDEO_PLANE_OSD1,
	  .type = DRM_PLANE_TYPE_PRIMARY,
	  .plane_order = 2 },
	{ .layer_nr = VO_VIDEO_PLANE_OSD3,
	  .type = DRM_PLANE_TYPE_CURSOR,
	  .plane_order = 3 },
};
#else
static const struct crtc_plane_data rtd_crtc_plane_data_third[] = {
	{ .layer_nr = VO_VIDEO_PLANE_OSD3,
	  .type = DRM_PLANE_TYPE_PRIMARY,
	  .plane_order = 2 },
};
#endif

static const struct crtc_data rtd_crtc_all = {
	.version = 0,
	.plane = rtd_crtc_plane_all,
	.plane_size = ARRAY_SIZE(rtd_crtc_plane_all),
	.rpc_bind = NULL,
};

static const struct crtc_data rtd_crtc_main = {
	.version = 0,
	.mixer = 0,
	.plane = rtd_crtc_plane_main,
	.plane_size = ARRAY_SIZE(rtd_crtc_plane_main),
	.rpc_bind = NULL,
};

static const struct crtc_data rtd_crtc_ext = {
	.version = 0,
	.mixer = 1,
	.plane = rtd_crtc_plane_ext,
	.plane_size = ARRAY_SIZE(rtd_crtc_plane_ext),
	.rpc_bind = NULL,
};

static const struct crtc_data rtd_crtc_pipe_main = {
	.version = 0,
	.mixer = 0,
	.plane = rtd_crtc_plane_data_main,
	.plane_size = ARRAY_SIZE(rtd_crtc_plane_data_main),
	.rpc_bind = rtk_crtc_rpc_bind,
};

static const struct crtc_data rtd_crtc_pipe_second = {
	.version = 0,
	.mixer = 1,
	.plane = rtd_crtc_plane_data_ext,
	.plane_size = ARRAY_SIZE(rtd_crtc_plane_data_ext),
	.rpc_bind = rtk_crtc_rpc_bind,
};

static const struct crtc_data rtd_crtc_pipe_third = {
	.version = 0,
	.mixer = 2,
	.plane = rtd_crtc_plane_data_third,
	.plane_size = ARRAY_SIZE(rtd_crtc_plane_data_third),
	.rpc_bind = rtk_crtc_rpc_bind,
};


static const struct of_device_id rtk_crtc_of_ids[] = {
	{ .compatible = "realtek,rtd-crtc-main",
	  .data = &rtd_crtc_main },
	{ .compatible = "realtek,rtd-crtc-ext",
	  .data = &rtd_crtc_ext },
	{ .compatible = "realtek,rtd-crtc-pipe-main",
	  .data = &rtd_crtc_pipe_main },
	{ .compatible = "realtek,rtd-crtc-pipe-second",
	  .data = &rtd_crtc_pipe_second },
	{ .compatible = "realtek,rtd-crtc-pipe-third",
	  .data = &rtd_crtc_pipe_third },
	{ .compatible = "realtek,rtd-crtc-all",
	  .data = &rtd_crtc_all },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_crtc_of_ids);

struct platform_driver rtk_crtc_platform_driver = {
	.probe = rtk_crtc_probe,
	.remove = rtk_crtc_remove,
	.driver = {
		.name = "realtek-crtc",
		.of_match_table = rtk_crtc_of_ids,
	},
};
