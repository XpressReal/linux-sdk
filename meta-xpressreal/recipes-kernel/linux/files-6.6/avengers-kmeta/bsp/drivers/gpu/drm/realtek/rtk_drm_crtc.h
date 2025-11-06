/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019 RealTek Inc.
 */

#ifndef _RTK_DRM_CRTC_H
#define _RTK_DRM_CRTC_H

#include <linux/iosys-map.h>
#include "rtk_drm_rpc.h"
#include "rtk_drm_fence.h"

#define VO_LAYER_NR		4
#define OVERLAY_PLANE_MAX	2
#define BUFLOCK_MAX		10

struct rtk_drm_crtc;

enum {
	RPC_READY = (1U << 0),
	ISR_INIT = (1U << 2),
	WAIT_VSYNC = (1U << 3),
	CHANGE_RES = (1U << 4),
	BG_SWAP = (1U << 5),
	SUSPEND = (1U << 6),
	VSYNC_FORCE_LOCK = (1U << 7),
};

struct crtc_plane_data {
	enum VO_VIDEO_PLANE layer_nr;
	enum drm_plane_type type;
	int plane_order;
};

struct crtc_data {
	unsigned int version;
	const struct crtc_plane_data *plane;
	unsigned int plane_size;
	unsigned int mixer;
	int (*rpc_bind)(struct rtk_drm_crtc *rtk_crtc, struct rtk_drm_private *priv);
};

struct rtk_drm_plane {
	struct drm_plane plane;

	/* TODO remove DMABUF_HEAPS_RTK */
	dma_addr_t dma_handle, refclock_dma_handle;
#ifdef DMABUF_HEAPS_RTK
	int handle;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
#ifdef CONFIG_CHROME_PLATFORMS
#else
	struct dma_buf *refclock_dmabuf;
#endif
	struct dma_buf_attachment *refclock_attach;
	struct iosys_map map;
#endif

#ifdef CONFIG_CHROME_PLATFORMS
	struct dma_buf *refclock_dmabuf;
#endif
	struct rtk_rpc_info *rpc_info;
	struct rpc_vo_filter_display info;
	struct rpc_config_disp_win disp_win;

	void *ringbase;
	struct tag_refclock *refclock;
	struct tag_ringbuffer_header *ringheader;

	unsigned int flags;
	unsigned int gAlpha; /* [0]:Pixel Alpha	[0x01 ~ 0xFF]:Global Alpha */

	struct rtk_drm_fence *rtk_fence;
	struct drm_property *out_fence_ptr;
	struct drm_property *display_idx_prop;
	struct drm_property *rtk_meta_data_prop;
	struct drm_property *vo_plane_name_prop;

	enum VO_VIDEO_PLANE layer_nr;
	unsigned int mixer;

	ktime_t update_time;
	unsigned int context;
	enum display_panel_usage display_panel_usage;
	bool wait_a_vsync;
	wait_queue_head_t wait_vsync;

	bool pending_planes;
	ktime_t pending;
};

struct rtk_drm_crtc {
	struct drm_crtc crtc;
	struct device *dev;
	struct drm_pending_vblank_event *event;

	int plane_count;
	struct rtk_drm_plane *nplanes;
	struct rtk_rpc_info *rpc_info;
	struct rpc_disp_mixer_order mixer_order;

	unsigned int irq;

	enum VO_VIDEO_PLANE layer_nr[VO_VIDEO_PLANE_NONE];
	struct drm_property *present_time_prop;
	struct drm_property *display_panel_usage_prop;
	int present_time_en;
	long long present_time;
	unsigned int mixer;

	bool pending_needs_vblank;

	enum display_panel_usage display_panel_usage;
	ktime_t begin;
	ktime_t completed;
};

struct rtk_crtc_state {
	struct drm_crtc_state base;
};

bool rtk_plane_check_update_done(struct rtk_drm_plane *rtk_plane);

extern void rtk_plane_destroy(struct drm_plane *plane);
extern void rtk_crtc_finish_page_flip(struct drm_crtc *crtc);
extern int rtk_drm_fence_init(struct rtk_drm_plane *rtk_plane);
extern int rtk_drm_fence_uninit(struct rtk_drm_plane *rtk_plane);
extern int rtk_drm_fence_create(struct rtk_drm_fence *rtk_fence, s32 __user *out_fence_ptr);
extern int rtk_drm_fence_update(struct rtk_drm_plane *rtk_plane);

int rtk_crtc_ioctl_get_mixer_id(struct drm_device *dev, void *data, struct drm_file *file);
int rtk_crtc_get_display_panel_usage_by_mixer(struct rtk_drm_crtc *rtk_crtc);

#endif  /* _RTK_DRM_CRTC_H_ */
