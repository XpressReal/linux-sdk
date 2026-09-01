
#ifndef _RTK_DRM_CRTC_H
#define _RTK_DRM_CRTC_H

#include <drm/drm_crtc.h>
#include <drm/drm_print.h>

#include "rtk_drm_rpc.h"
#include "rtk_drm_fence.h"

#define VO_LAYER_NR		4
enum {
	RPC_READY = (1U << 0),
	ISR_INIT = (1U << 2),
	WAIT_VSYNC = (1U << 3),
	CHANGE_RES = (1U << 4),
	BG_SWAP = (1U << 5),
	SUSPEND = (1U << 6),
	VSYNC_FORCE_LOCK = (1U << 7),
};

struct rtk_drm_plane {
	struct drm_plane plane;

#ifdef DMABUF_HEAPS_RTK
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct dma_buf *refclock_dmabuf;
	struct dma_buf_attachment *refclock_attach;
#endif
	struct rtk_rpc_info *rpc_info;
	struct rpc_vo_filter_display info;
	struct rpc_config_disp_win disp_win;

	void *ringbase;
	struct tag_refclock *refclock;
	struct tag_ringbuffer_header *ringheader;
	dma_addr_t ring_paddr;
	unsigned int keepFrmLock_paddr;

	unsigned int flags;
	unsigned int gAlpha; /* [0]:Pixel Alpha	[0x01 ~ 0xFF]:Global Alpha */
	int hdr_type;

	struct rtk_drm_fence *rtk_fence;
	struct drm_property *out_fence_ptr;

	unsigned int secure_flag;
};

struct rtk_drm_crtc {
	struct drm_crtc crtc;
	struct device *dev;

	struct drm_pending_vblank_event *event;

	struct rtk_rpc_info *rpc_info;

	void *vo_vsync_flag; /* VSync enable and notify. */
	unsigned int irq;

	enum VO_VIDEO_PLANE layer_nr[VO_LAYER_NR];
	struct rtk_drm_plane planes[VO_LAYER_NR];
};

struct rtk_crtc_state {
	struct drm_crtc_state base;
};

int rtk_plane_init(struct drm_device *drm, struct rtk_drm_plane *rtk_plane,
		   unsigned long possible_crtcs, enum drm_plane_type type,
		   enum VO_VIDEO_PLANE layer_nr);
extern void rtk_plane_destroy(struct drm_plane *plane);
extern int rtk_drm_fence_init(struct rtk_drm_plane *rtk_plane);
extern int rtk_drm_fence_uninit(struct rtk_drm_plane *rtk_plane);
extern int rtk_drm_fence_create(struct rtk_drm_fence *rtk_fence, s32 __user *out_fence_ptr);
extern int rtk_drm_fence_update(struct rtk_drm_plane *rtk_plane);

extern void rtk_crtc_finish_page_flip(struct drm_crtc *crtc);

#endif  /* _RTK_DRM_CRTC_H_ */
