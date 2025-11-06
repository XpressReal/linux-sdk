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

#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_blend.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_plane.h>
#include <drm/drm_vblank.h>	// DEBUG: struct drm_pending_vblank_event

#include <linux/iosys-map.h>
#include <linux/sys_soc.h>
#include "rtk_drm_drv.h"
#include "rtk_drm_fb.h"
#include "rtk_drm_gem.h"
#include "rtk_drm_crtc.h"
#include "rtk_drm_rpc.h"

MODULE_IMPORT_NS(DMA_BUF);

#define to_rtk_plane(s) container_of(s, struct rtk_drm_plane, plane)
#define MAX_PLANE 5

#define INVERT_BITVAL_1 (~1)

static const struct soc_device_attribute rtk_soc_kent[] = {
	{ .family = "Realtek Kent", },
	{ /* empty */ }
};

static const unsigned int osd_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_RGBA4444,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_RGBA5551,
};

static const unsigned int video_formats_kent[] = {
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,
};

static const unsigned int video_formats[] = {
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
};

static const unsigned int other_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_RGB565,
};

static const uint64_t format_modifiers_default[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID,
};

static const uint64_t format_modifiers_arm[] = {
	RTK_AFBC_MOD,
	RTK_AFRC_CU16_MOD,
	RTK_AFRC_CU24_MOD,
	RTK_AFRC_CU32_MOD,
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID,
};

static const struct drm_prop_enum_list vo_plane_list[] = {
	{ VO_VIDEO_PLANE_V1, "V1" },
	{ VO_VIDEO_PLANE_V2, "V2" },
	{ VO_VIDEO_PLANE_SUB1, "SUB1" },
	{ VO_VIDEO_PLANE_OSD1, "OSD1" },
	{ VO_VIDEO_PLANE_OSD2, "OSD2" },
	{ VO_VIDEO_PLANE_WIN1, "WIN1"},
	{ VO_VIDEO_PLANE_WIN2, "WIN2"},
	{ VO_VIDEO_PLANE_WIN3, "WIN3"},
	{ VO_VIDEO_PLANE_WIN4, "WIN4"},
	{ VO_VIDEO_PLANE_WIN5, "WIN5"},
	{ VO_VIDEO_PLANE_WIN6, "WIN6"},
	{ VO_VIDEO_PLANE_WIN7, "WIN7"},
	{ VO_VIDEO_PLANE_WIN8, "WIN8"},
	{ VO_VIDEO_PLANE_SUB2, "SUB2"},
	{ VO_VIDEO_PLANE_CSR, "CSR"},
	{ VO_VIDEO_PLANE_V3, "V3"},
	{ VO_VIDEO_PLANE_V4, "V4"},
	{ VO_VIDEO_PLANE_OSD3, "OSD3"},
	{ VO_VIDEO_PLANE_OSD4, "OSD4"},
};

struct rtk_drm_plane_state {
	struct drm_plane_state state;
	s32 __user *out_fence_ptr;
	unsigned int display_idx;
	struct drm_property_blob *rtk_meta_data_blob;
	struct drm_property_enum *vo_plane_name;
	struct video_object rtk_meta_data;
};

static struct vo_rectangle rect_plane_disabled = {0};
static struct vo_rectangle rect_osd1;
static struct vo_rectangle rect_sub1;
static struct vo_rectangle rect_video1;

static DEFINE_MUTEX(enable_display_mutex);

ssize_t rtk_plane_enable_osd_display_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t rtk_plane_enable_sub_display_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t rtk_plane_enable_video_display_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) ;

static DEVICE_ATTR(enable_osd_display, S_IWUSR, NULL, rtk_plane_enable_osd_display_store);
static DEVICE_ATTR(enable_sub_display, S_IWUSR, NULL, rtk_plane_enable_sub_display_store);
static DEVICE_ATTR(enable_video_display, S_IWUSR, NULL, rtk_plane_enable_video_display_store);

#define ulPhyAddrFilter(x) ((x) & ~0xe0000000)

static uint64_t htonll(long long val)
{
	return (((long long) htonl(val)) << 32) + htonl(val >> 32);
}

static inline struct rtk_drm_plane_state *
to_rtk_plane_state(struct drm_plane_state *s)
{
	return container_of(s, struct rtk_drm_plane_state, state);
}

#ifdef CONFIG_CHROME_PLATFORMS
struct rtkplane_dma_buf_attachment {
        struct sg_table sgt;
};

static int rtkplane_dma_buf_attach(struct dma_buf *dmabuf,
                                  struct dma_buf_attachment *attach)
{
	struct rtk_drm_plane *rtk_plane = dmabuf->priv;
	struct drm_device *drm = rtk_plane->plane.dev;
        struct device *dev = drm->dev;
	dma_addr_t daddr = rtk_plane->refclock_dma_handle;
        void *vaddr = rtk_plane->refclock;
        size_t size = dmabuf->size;

        struct rtkplane_dma_buf_attachment *a;
        int ret;

        a = kzalloc(sizeof(*a), GFP_KERNEL);
        if (!a)
                return -ENOMEM;

        ret = dma_get_sgtable(dev, &a->sgt, vaddr, daddr, size);
        if (ret < 0) {
                dev_err(dev, "failed to get scatterlist from DMA API\n");
                kfree(a);
                return -EINVAL;
        }

        attach->priv = a;

        return 0;
}

static void rtkplane_dma_buf_detatch(struct dma_buf *dmabuf,
                                struct dma_buf_attachment *attach)
{
        struct rtkplane_dma_buf_attachment *a = attach->priv;

        sg_free_table(&a->sgt);
        kfree(a);
}

static struct sg_table *rtkplane_map_dma_buf(struct dma_buf_attachment *attach,
                                        enum dma_data_direction dir)
{
        struct rtkplane_dma_buf_attachment *a = attach->priv;
        struct sg_table *table;
        int ret;

        table = &a->sgt;

        ret = dma_map_sgtable(attach->dev, table, dir, 0);
        if (ret)
                table = ERR_PTR(ret);
        return table;
}

static void rtkplane_unmap_dma_buf(struct dma_buf_attachment *attach,
                                struct sg_table *table,
                                enum dma_data_direction dir)
{
        dma_unmap_sgtable(attach->dev, table, dir, 0);
}

static int rtkplane_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
        struct rtk_drm_plane *rtk_plane = dmabuf->priv;
	struct drm_device *drm = rtk_plane->plane.dev;
        struct device *dev = drm->dev;
	dma_addr_t daddr = rtk_plane->refclock_dma_handle;
        void *vaddr = rtk_plane->refclock;
        size_t size = vma->vm_end - vma->vm_start;

        if (vaddr)
                return dma_mmap_coherent(dev, vma, vaddr, daddr, size);

        return 0;
}

static void rtkplane_release(struct dma_buf *dmabuf)
{
        struct rtk_drm_plane *rtk_plane = dmabuf->priv;
	struct drm_device *drm = rtk_plane->plane.dev;
        struct device *dev = drm->dev;
	dma_addr_t daddr = rtk_plane->refclock_dma_handle;
        void *vaddr = rtk_plane->refclock;
        size_t size = dmabuf->size;

        if (vaddr)
                dma_free_coherent(dev, size, vaddr, daddr);

}

static const struct dma_buf_ops rtkplane_dma_buf_ops = {
        .attach = rtkplane_dma_buf_attach,
        .detach = rtkplane_dma_buf_detatch,
	.map_dma_buf = rtkplane_map_dma_buf,
        .unmap_dma_buf = rtkplane_unmap_dma_buf,
        .mmap = rtkplane_mmap,
        .release = rtkplane_release,
};
#endif

static int write_cmd_to_ringbuffer(struct rtk_drm_plane *rtk_plane, void *cmd)
{
	void *base_iomap = rtk_plane->ringbase;
	struct tag_ringbuffer_header *rbHeader = rtk_plane->ringheader;
	unsigned int size = ((struct inband_cmd_pkg_header *)cmd)->size;
	unsigned int read, write, base, b_size, limit;
	unsigned int type = (rtk_plane->rpc_info->krpc_vo_opt != RPC_AUDIO) ? 1:0;

	read = ipcReadULONG((u8 *)&rbHeader->readPtr[0], type);
	write = ipcReadULONG((u8 *)&(rbHeader->writePtr), type);
	base = ipcReadULONG((u8 *)&(rbHeader->beginAddr), type);
	b_size = ipcReadULONG((u8 *)&(rbHeader->size), type);
	limit = base + b_size;

	if (read + (read > write ? 0 : limit - base) - write > size) {
		unsigned long offset = write - base;
		void *write_io = (void *)((unsigned long)base_iomap + offset);

		if (write + size <= limit) {
			ipcCopyMemory((void *)write_io, cmd, size, type);
		} else {
			ipcCopyMemory((void *)write_io, cmd, limit - write, type);
			ipcCopyMemory((void *)base_iomap, (void *)((unsigned long)cmd + limit - write), size - (limit - write), type);
		}
		write += size;
		write = write < limit ? write : write - (limit - base);

		rbHeader->writePtr = ipcReadULONG((u8 *)&write, type);

		DRM_DEBUG_DRIVER("r:0x%x w:0x%x size:%u base:0x%x limit:0x%x\n",
			  read, write, size, base, limit);

	} else {
		DRM_ERROR("errQ r:%x w:%x size:%u base:%u limit:%u\n",
			  read, write, size, base, limit);
		goto err;
	}

	return 0;
err:
	return -1;
}

static void init_video_object(struct video_object *obj)
{
	memset(obj, 0, sizeof(struct video_object));

	obj->lumaOffTblAddr = 0xffffffff;
	obj->chromaOffTblAddr = 0xffffffff;
	obj->lumaOffTblAddrR = 0xffffffff;
	obj->chromaOffTblAddrR = 0xffffffff;
	obj->bufBitDepth = 8;
	obj->matrix_coefficients = 1;
	obj->tch_hdr_metadata.specVersion = -1;

	obj->Y_addr_Right = 0xffffffff;
	obj->U_addr_Right = 0xffffffff;
	obj->pLock_Right = 0xffffffff;
}

static int rtk_plane_inband_config_disp_win(struct drm_plane *plane, struct rpc_config_disp_win *disp_win)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct inband_config_disp_win *inband_cmd;

	inband_cmd = (struct inband_config_disp_win *)kzalloc(sizeof(struct inband_config_disp_win), GFP_KERNEL);
	if(!inband_cmd) {
		DRM_ERROR("rtk_plane_inband_config_disp_win malloc inband_cmd fail\n");
		return -1;
	}

	memset(inband_cmd, 0, sizeof(struct inband_config_disp_win));

	inband_cmd->header.type = VIDEO_VO_INBAND_CMD_TYPE_CONFIGUREDISPLAYWINDOW;
	inband_cmd->header.size = sizeof(struct inband_config_disp_win);

	inband_cmd->videoPlane        = disp_win->videoPlane;
	inband_cmd->videoWin.x        = disp_win->videoWin.x;
	inband_cmd->videoWin.y        = disp_win->videoWin.y;
	inband_cmd->videoWin.width    = disp_win->videoWin.width;
	inband_cmd->videoWin.height   = disp_win->videoWin.height;
	inband_cmd->borderWin.x       = disp_win->borderWin.x;
	inband_cmd->borderWin.y       = disp_win->borderWin.y;
	inband_cmd->borderWin.width   = disp_win->borderWin.width;
	inband_cmd->borderWin.height  = disp_win->borderWin.height;
	inband_cmd->borderColor.c1    = disp_win->borderColor.c1;
	inband_cmd->borderColor.c2    = disp_win->borderColor.c2;
	inband_cmd->borderColor.c3    = disp_win->borderColor.c3;
	inband_cmd->borderColor.isRGB = disp_win->borderColor.isRGB;
	inband_cmd->enBorder          = disp_win->enBorder;

	write_cmd_to_ringbuffer(rtk_plane, inband_cmd);
	kfree(inband_cmd);

	return 0;
}

static int rtk_plane_update_cluster_scaling(struct drm_plane *plane)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct rpc_query_panel_usage_pos usage_pos;
	struct vo_rectangle *old_disp_win;
	struct vo_color blueBorder = {0, 0, 255, 1};

	DRM_DEBUG_DRIVER("videoPlane : %s\n", plane_names[rtk_plane->info.videoPlane]);
	DRM_DEBUG_DRIVER("[type] : %d] [new] crtc_x : %d, crtc_y : %d\n",
		plane->type, plane->state->crtc_x, plane->state->crtc_y);
	DRM_DEBUG_DRIVER("[type] : %d] [new] crtc_w : %d, crtc_h : %d\n",
		plane->type, plane->state->crtc_w, plane->state->crtc_h);

	old_disp_win = &rtk_plane->disp_win.videoWin;

	usage_pos.display_panel_usage = rtk_plane->display_panel_usage;
	rpc_query_panel_usage_pos(rpc_info, &usage_pos);

	DRM_DEBUG_DRIVER("[type] : %d] [new] usage_pos.x : %d, usage_pos.y : %d\n",
		plane->type, usage_pos.x, usage_pos.y);

	if (old_disp_win->x != usage_pos.x ||
		old_disp_win->y != usage_pos.y ||
		old_disp_win->width != plane->state->crtc_w ||
		old_disp_win->height != plane->state->crtc_h) {

		DRM_DEBUG_DRIVER("plane type \x1b[31m%d\033[0m coordinate or size has changed\n", plane->type);
		DRM_DEBUG_DRIVER("[type] : %d] [old] disp_win->x     : %d, disp_win->y      : %d\n",
			plane->type, old_disp_win->x, old_disp_win->y);
		DRM_DEBUG_DRIVER("[type] : %d] [old] disp_win->width : %d, disp_win->height : %d\n",
			plane->type, old_disp_win->width, old_disp_win->height);

		DRM_DEBUG_DRIVER("send %s rpc to config %s on %s\n",
						krpc_names[rpc_info->krpc_vo_opt],
						plane_names[rtk_plane->info.videoPlane],
						mixer_names[rtk_plane->mixer]);

		rtk_plane->disp_win.videoPlane = rtk_plane->info.videoPlane | (rtk_plane->mixer << 16);

		rtk_plane->disp_win.videoWin.x       = usage_pos.x;
		rtk_plane->disp_win.videoWin.y       = usage_pos.y;
		rtk_plane->disp_win.videoWin.width   = plane->state->crtc_w;
		rtk_plane->disp_win.videoWin.height  = plane->state->crtc_h;
		rtk_plane->disp_win.borderWin.x      = usage_pos.x;
		rtk_plane->disp_win.borderWin.y      = usage_pos.y;
		rtk_plane->disp_win.borderWin.width  = plane->state->crtc_w;
		rtk_plane->disp_win.borderWin.height = plane->state->crtc_h;
		rtk_plane->disp_win.borderColor      = blueBorder;
		rtk_plane->disp_win.enBorder         = 0;

		if (rpc_video_config_disp_win(rpc_info, &rtk_plane->disp_win)) {
			DRM_ERROR("rpc_video_config_disp_win RPC fail\n");
			return -1;
		}
	}

	return 0;
}

static int rtk_plane_update_scaling(struct drm_plane *plane)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct vo_rectangle *old_disp_win;
	struct vo_color blueBorder = {0, 0, 255, 1};

	DRM_DEBUG_DRIVER("videoPlane : %s\n", plane_names[rtk_plane->info.videoPlane]);
	DRM_DEBUG_DRIVER("[type] : %d] [new] crtc_x : %d, crtc_y : %d\n",
		plane->type, plane->state->crtc_x, plane->state->crtc_y);
	DRM_DEBUG_DRIVER("[type] : %d] [new] crtc_w : %d, crtc_h : %d\n",
		plane->type, plane->state->crtc_w, plane->state->crtc_h);

	old_disp_win = &rtk_plane->disp_win.videoWin;

	if (old_disp_win->x != plane->state->crtc_x ||
		old_disp_win->y != plane->state->crtc_y ||
		old_disp_win->width != plane->state->crtc_w ||
		old_disp_win->height != plane->state->crtc_h) {

		DRM_DEBUG_DRIVER("plane type \x1b[31m%d\033[0m coordinate or size has changed\n", plane->type);
		DRM_DEBUG_DRIVER("[type] : %d] [old] disp_win->x     : %d, disp_win->y      : %d\n",
			plane->type, old_disp_win->x, old_disp_win->y);
		DRM_DEBUG_DRIVER("[type] : %d] [old] disp_win->width : %d, disp_win->height : %d\n",
			plane->type, old_disp_win->width, old_disp_win->height);

		DRM_DEBUG_DRIVER("send %s rpc to config %s on %s\n",
						krpc_names[rpc_info->krpc_vo_opt],
						plane_names[rtk_plane->info.videoPlane],
						mixer_names[rtk_plane->mixer]);

		rtk_plane->disp_win.videoPlane = rtk_plane->info.videoPlane | (rtk_plane->mixer << 16);

		rtk_plane->disp_win.videoWin.x       = plane->state->crtc_x;
		rtk_plane->disp_win.videoWin.y       = plane->state->crtc_y;
		rtk_plane->disp_win.videoWin.width   = plane->state->crtc_w;
		rtk_plane->disp_win.videoWin.height  = plane->state->crtc_h;
		rtk_plane->disp_win.borderWin.x      = plane->state->crtc_x;
		rtk_plane->disp_win.borderWin.y      = plane->state->crtc_y;
		rtk_plane->disp_win.borderWin.width  = plane->state->crtc_w;
		rtk_plane->disp_win.borderWin.height = plane->state->crtc_h;
		rtk_plane->disp_win.borderColor      = blueBorder;
		rtk_plane->disp_win.enBorder         = 0;

		if (plane->type == DRM_PLANE_TYPE_CURSOR) {
			DRM_DEBUG_DRIVER("[rtk_plane_inband_config_disp_win]\n");
			if (rtk_plane_inband_config_disp_win(plane, &rtk_plane->disp_win)) {
				DRM_ERROR("rtk_plane_inband_config_disp_win fail\n");
				return -1;
			}
		} else {
			DRM_DEBUG_DRIVER("[rpc_video_config_disp_win]\n");
			if (rpc_video_config_disp_win(rpc_info, &rtk_plane->disp_win)) {
				DRM_ERROR("rpc_video_config_disp_win RPC fail\n");
				return -1;
			}
		}
	}

	return 0;
}

static int rtk_plane_update_cursor(struct drm_plane *plane)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *gem[4];
	struct rtk_gem_object *rtk_gem[4];
	const struct drm_format_info *info;
	unsigned int flags = 0;
	int i;

	struct graphic_object *obj = (struct graphic_object *)kzalloc(sizeof(struct graphic_object), GFP_KERNEL);

	if(!obj) {
		DRM_ERROR("queue_ring_buffer malloc graphic_object fail\n");
		return -1;
	}

	info = drm_format_info(fb->format->format);
	for (i = 0; i < info->num_planes; i++) {
		gem[i] = rtk_fb_get_gem_obj(fb, i);
		if (!gem[i])
			gem[i] = gem[0];
		rtk_gem[i] = to_rtk_gem_obj(gem[i]);
	}

	memset(obj, 0, sizeof(struct graphic_object));
	obj->header.type = VIDEO_GRAPHIC_INBAND_CMD_TYPE_PICTURE_OBJECT;
	obj->header.size = sizeof(struct graphic_object);
	obj->colorkey = -1;
	if (fb->format->format == DRM_FORMAT_XRGB8888) {
		flags |= eBuffer_USE_GLOBAL_ALPHA;
		obj->alpha = 0x3ff;
		obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB8888_LITTLE;
	} else if (fb->format->format == DRM_FORMAT_ABGR8888) {
		obj->format = INBAND_CMD_GRAPHIC_FORMAT_RGBA8888;
	} else if (fb->format->format == DRM_FORMAT_ARGB8888) {
		obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB8888_LITTLE;
	} else if (fb->format->format == DRM_FORMAT_BGRA8888) {
		obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB8888_LITTLE;
	}

	DRM_DEBUG_DRIVER("fb info:\n");
	DRM_DEBUG_DRIVER("widthxheight (%dx%d)\n", fb->width, fb->height);
	DRM_DEBUG_DRIVER("pitches[0] (%d)\n", fb->pitches[0]);

	obj->width = fb->width;
	obj->height = fb->height;
	obj->pitch = fb->pitches[0];
	obj->address = rtk_gem[0]->paddr;
	obj->picLayout = INBAND_CMD_GRAPHIC_2D_MODE;

	obj->mode = MODE_LINEAR;
	obj->ext1 = 0;
	obj->ext2 = 0;

	write_cmd_to_ringbuffer(rtk_plane, obj);

	kfree(obj);
	return 0;
}

static int rtk_plane_update_video_obj(struct drm_plane *plane)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_drm_fence *rtk_fence = rtk_plane->rtk_fence;
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *gem[4];
	struct rtk_gem_object *rtk_gem[4];
	const struct drm_format_info *info;

	struct rtk_drm_plane_state *s = to_rtk_plane_state(plane->state);
	int i;
	int index;
	struct video_object *obj = (struct video_object *)kzalloc(sizeof(struct video_object), GFP_KERNEL);
	if(!obj) {
		DRM_ERROR("rtk_plane_update_video_obj malloc video_object fail\n");
		return -1;
	}

	info = drm_format_info(fb->format->format);
	for (i = 0; i < info->num_planes; i++) {
		gem[i] = rtk_fb_get_gem_obj(fb, i);
		if (!gem[i])
			gem[i] = gem[0];
		rtk_gem[i] = to_rtk_gem_obj(gem[i]);
	}

	init_video_object(obj);

	DRM_DEBUG_DRIVER("fb info:\n");
	DRM_DEBUG_DRIVER("widthxheight (%dx%d)\n", fb->width, fb->height);
	DRM_DEBUG_DRIVER("format (0x%x) num_planes (%d)\n", fb->format->format, info->num_planes);

	switch (fb->format->format) {
		case DRM_FORMAT_NV12:
			obj->Y_pitch = fb->width;
			break;
		case DRM_FORMAT_NV21:
			obj->mode |= INBAND_CMD_VIDEO_FORMAT_NV21;
			obj->Y_pitch = fb->width;
			break;
		case DRM_FORMAT_YUYV:
			obj->mode |= INBAND_CMD_VIDEO_FORMAT_PACKED_EN;
			obj->Y_pitch = fb->width * 2;
			break;
		case DRM_FORMAT_YVYU:
			obj->mode |= (INBAND_CMD_VIDEO_FORMAT_PACKED_EN | INBAND_CMD_VIDEO_FORMAT_YVYU);
			obj->Y_pitch = fb->width * 2;
			break;
		case DRM_FORMAT_UYVY:
			obj->mode |= (INBAND_CMD_VIDEO_FORMAT_PACKED_EN | INBAND_CMD_VIDEO_FORMAT_UYVY);
			obj->Y_pitch = fb->width * 2;
			break;
		case DRM_FORMAT_VYUY:
			obj->mode |= (INBAND_CMD_VIDEO_FORMAT_PACKED_EN | INBAND_CMD_VIDEO_FORMAT_VYUY);
			obj->Y_pitch = fb->width * 2;
			break;
		default:
			DRM_ERROR("Unsupported video format\n");
			break;
	}

#ifdef CONFIG_RTK_METADATA_AUTOJUDGE
	if (rtk_gem[0]->dmabuf_type == DMABUF_TYPE_NORMAL) {
#else
	if (s->rtk_meta_data.header.type != METADATA_HEADER) {
#endif

		obj->header.type = VIDEO_VO_INBAND_CMD_TYPE_OBJ_PIC;
		obj->header.size = sizeof(struct video_object);
		obj->version = 0x72746B3F;
		obj->width = fb->width;
		obj->height = fb->height;
		obj->mode |= CONSECUTIVE_FRAME;

		obj->Y_addr = rtk_gem[0]->paddr + fb->offsets[0];
		obj->U_addr = rtk_gem[0]->paddr + fb->offsets[0];

		if (info->num_planes > 1)
			obj->U_addr = rtk_gem[1]->paddr + fb->offsets[1];

	} else {
#ifdef CONFIG_RTK_METADATA_AUTOJUDGE
		struct video_object *decObj = (struct video_object *)rtk_gem[0]->vaddr;
#else
		struct video_object *decObj = (struct video_object *)(&s->rtk_meta_data);
#endif
		memcpy(obj, decObj, sizeof(struct video_object));

		obj->header.type = VIDEO_VO_INBAND_CMD_TYPE_OBJ_PIC;
		obj->header.size = sizeof(struct video_object);
		obj->version = 0x72746B3F;

		index = s->display_idx;
		obj->context = index;
		obj->pLock = rtk_fence->pLock_paddr+index;
		obj->pReceived = rtk_fence->pReceived_paddr+index;
		obj->PTSH = decObj->PTSH;
		obj->PTSL = decObj->PTSL;
		obj->RPTSH = decObj->RPTSH;
		obj->RPTSL = decObj->RPTSL;
	}

	DRM_DEBUG_DRIVER("obj->mode = 0x%x\n", obj->mode);

	write_cmd_to_ringbuffer(rtk_plane, obj);
	kfree(obj);
	return 0;
}

static int rtk_plane_update_graphic_obj(struct drm_plane *plane)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct rtk_drm_fence *rtk_fence = rtk_plane->rtk_fence;
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *gem[4];
	struct rtk_gem_object *rtk_gem[4];
	const struct drm_format_info *info;
	unsigned int flags = 0;
	int i;
	unsigned int cu_size = 0;
	unsigned int coding_unit = 0;

	struct graphic_object *obj = (struct graphic_object *)kzalloc(sizeof(struct graphic_object), GFP_KERNEL);
	ktime_t start;
	s64 update_time_ms;

	start = ktime_get();

	if(!obj) {
		DRM_ERROR("queue_ring_buffer malloc graphic_object fail\n");
		return -1;
	}

	info = drm_format_info(fb->format->format);
	for (i = 0; i < info->num_planes; i++) {
		gem[i] = rtk_fb_get_gem_obj(fb, i);
		if (!gem[i])
			gem[i] = gem[0];
		rtk_gem[i] = to_rtk_gem_obj(gem[i]);
	}

	memset(obj, 0, sizeof(struct graphic_object));
	obj->header.type = VIDEO_GRAPHIC_INBAND_CMD_TYPE_PICTURE_OBJECT;
	obj->header.size = sizeof(struct graphic_object);
	obj->colorkey = -1;

	switch (fb->format->format) {
		case DRM_FORMAT_XRGB8888:
			flags |= eBuffer_USE_GLOBAL_ALPHA;
			obj->alpha = 0x3ff;
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB8888_LITTLE;
			break;
		case DRM_FORMAT_ARGB8888:
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB8888_LITTLE;
			break;
		case DRM_FORMAT_RGBA8888:
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_RGBA8888_LITTLE;
			break;
		case DRM_FORMAT_ABGR8888:
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_RGBA8888;
			break;
		case DRM_FORMAT_BGRA8888:
			if (rtk_drm_recovery)
				obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB8888_LITTLE;
			else
				obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB8888;
			break;
		case DRM_FORMAT_RGB565:
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_RGB565_LITTLE;
			break;
		case DRM_FORMAT_ARGB4444:
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB4444_LITTLE;
			break;
		case DRM_FORMAT_RGBA4444:
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_RGBA4444_LITTLE;
			break;
		case DRM_FORMAT_ARGB1555:
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB1555_LITTLE;
			break;
		case DRM_FORMAT_RGBA5551:
			obj->format = INBAND_CMD_GRAPHIC_FORMAT_RGBA5551_LITTLE;
			break;
		default:
			DRM_ERROR("Unsupported graphic format\n");
			break;
	}

	DRM_DEBUG_DRIVER("fb info:\n");
	DRM_DEBUG_DRIVER("widthxheight (%dx%d)\n", fb->width, fb->height);
	DRM_DEBUG_DRIVER("format (0x%x) modifier (0x%llx) pitches[0] (%d)\n",
		fb->format->format, fb->modifier, fb->pitches[0]);

	obj->context = rtk_fence->idx++;
	rtk_plane->context = obj->context;

	DRM_DEBUG_DRIVER("%s queue context %d\n",
		plane_names[rtk_plane->layer_nr], rtk_plane->context);

	if (rtk_fence->idx >= CONTEXT_SIZE)
		rtk_fence->idx -= CONTEXT_SIZE;

	obj->width = fb->width;
	obj->height = fb->height;
	obj->pitch = fb->pitches[0];
	obj->address = rtk_gem[0]->paddr;
	obj->picLayout = INBAND_CMD_GRAPHIC_2D_MODE;

	if (fb->modifier & AFBC_FORMAT_MOD_YTR) {
		DRM_DEBUG_DRIVER("AFBBBBBBBC\n");

		flags |= eBuffer_AFBC_Enable | eBuffer_AFBC_YUV_Transform;

		obj->mode = MODE_AFBC;
		obj->ext1 = (flags & eBuffer_AFBC_Split)?1:0;
		obj->ext2 = (flags & eBuffer_AFBC_YUV_Transform)?1:0;
	} else if (fb->modifier & AFRC_FORMAT_MOD_LAYOUT_SCAN) {
		DRM_DEBUG_DRIVER("AFRRRRRRRC\n");

		obj->mode = MODE_AFRC;

		switch (fb->modifier & AFRC_FORMAT_MOD_CU_SIZE_MASK) {
			case AFRC_FORMAT_MOD_CU_SIZE_16:
				coding_unit = AFRC_FORMAT_MOD_CU_SIZE_16 - 1;
				cu_size = 16;
				DRM_DEBUG_DRIVER("Coding Unit 16 (%d)\n", coding_unit);
				break;
			case AFRC_FORMAT_MOD_CU_SIZE_24:
				coding_unit = AFRC_FORMAT_MOD_CU_SIZE_24 - 1;
				cu_size = 24;
				DRM_DEBUG_DRIVER("Coding Unit 24 (%d)\n", coding_unit);
				break;
			case AFRC_FORMAT_MOD_CU_SIZE_32:
				coding_unit = AFRC_FORMAT_MOD_CU_SIZE_32 - 1;
				cu_size = 32;
				DRM_DEBUG_DRIVER("Coding Unit 32 (%d)\n", coding_unit);
				break;
			default:
				cu_size = 0;
				break;
		}

		obj->ext1 = coding_unit;
		obj->ext2 = STRIDE_HW_PITCH; //stride_en
		// obj->ext2 = STRIDE_USER_PITCH;

		if (obj->ext2 == STRIDE_USER_PITCH) {
			obj->pitch = ALIGN(obj->width, 64) * cu_size;
			DRM_DEBUG_DRIVER("User pitch %d\n", obj->pitch);
		}

	} else {
		obj->mode = MODE_LINEAR;
		obj->ext1 = 0;
		obj->ext2 = 0;
	}

	write_cmd_to_ringbuffer(rtk_plane, obj);

	DRM_DEBUG_DRIVER("rpc_send_interrupt first\n");
	rpc_send_interrupt(rpc_info);

	rtk_plane->pending_planes = true;
	rtk_plane->pending = ktime_get();

	update_time_ms = ktime_ms_delta(ktime_get(), start);
	if (update_time_ms > 2)
		DRM_DEBUG_DRIVER("queue context %d update_time_ms (%lld)\n",
			rtk_plane->context, update_time_ms);

	kfree(obj);

	return 0;
}

bool rtk_plane_check_update_done(struct rtk_drm_plane *rtk_plane)
{
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	int refContext = -1;
	s64 received_time_ms;

	rtk_plane->wait_a_vsync = true;
	wake_up(&rtk_plane->wait_vsync);

	if (rtk_plane->refclock) {

		if (rpc_info->krpc_vo_opt != RPC_AUDIO)
			refContext = rtk_plane->refclock->videoContext;
		else
			refContext = htonl(rtk_plane->refclock->videoContext);

		if (refContext == rtk_plane->context) {

			received_time_ms = ktime_ms_delta(ktime_get(), rtk_plane->pending);
			if (received_time_ms > 16) {
				DRM_DEBUG_DRIVER("%s's context %d update successful, received_time_ms (%lld)\n",
					plane_names[rtk_plane->layer_nr], rtk_plane->context, received_time_ms);
			}

			return true;
		}
	}

	DRM_DEBUG_DRIVER("%s's context %d update fail, continue to show %d\n",
		plane_names[rtk_plane->layer_nr], rtk_plane->context, refContext);

	return false;
}

static int rtk_plane_rpc_init(struct rtk_drm_plane *rtk_plane,
			      enum VO_VIDEO_PLANE layer_nr)
{
	struct drm_device *drm = rtk_plane->plane.dev;
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	void *vaddr;
	struct rpc_refclock refclock;
	struct rpc_ringbuffer ringbuffer;

	unsigned int id;

#ifdef CONFIG_CHROME_PLATFORMS
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
#else
	struct sg_table *table;
#endif
	int err = 0;
	unsigned long flags;

	enum VO_VIDEO_PLANE videoplane;

	struct vo_rectangle rect;
	struct vo_color blueBorder = {0, 0, 255, 1};

	videoplane = layer_nr;
	if (rpc_info->krpc_vo_opt != RPC_AUDIO)
		flags = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_HIFIACC;
	else
		flags = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC;

#ifndef CONFIG_CHROME_PLATFORMS
	rheap_setup_dma_pools(drm->dev, "rtk_audio_heap", flags, __func__);

	vaddr = dma_alloc_coherent(drm->dev, 65*1024, &rtk_plane->dma_handle,
				GFP_KERNEL | __GFP_NOWARN);

	rtk_plane->ringbase = (void *)((unsigned long)(vaddr));
	rtk_plane->ringheader = (struct tag_ringbuffer_header *)
			((unsigned long)(vaddr)+(64*1024));

	rtk_plane->refclock_dmabuf = rheap_alloc("rtk_audio_heap", 2*1024, flags);
	if (IS_ERR_OR_NULL(rtk_plane->refclock_dmabuf)) {
		DRM_ERROR("Failed to alloc refclock dmabuf\n");
		return PTR_ERR(rtk_plane->refclock_dmabuf);
	}
	dma_buf_set_name(rtk_plane->refclock_dmabuf, __func__);
	rtk_plane->refclock_attach = dma_buf_attach(rtk_plane->refclock_dmabuf, drm->dev);
	table = dma_buf_map_attachment(rtk_plane->refclock_attach, DMA_BIDIRECTIONAL);
	dma_buf_begin_cpu_access(rtk_plane->refclock_dmabuf, DMA_BIDIRECTIONAL);

	dma_buf_vmap(rtk_plane->refclock_dmabuf, &rtk_plane->map);
	vaddr = rtk_plane->refclock_dmabuf->vmap_ptr.vaddr;

	rtk_plane->refclock_dma_handle = sg_phys(table->sgl);
//	vaddr = dma_alloc_coherent(drm->dev, 2*1024, &rtk_plane->refclock_dma_handle,
//				GFP_KERNEL | __GFP_NOWARN);

	rtk_plane->refclock = (struct tag_refclock *)((unsigned long)(vaddr));
#else
	vaddr = dma_alloc_coherent(drm->dev, 65*SZ_1K, &rtk_plane->dma_handle,
				GFP_KERNEL | __GFP_NOWARN);
	if (!vaddr) {
		dev_err(drm->dev, "%s dma_alloc fail \n", __func__);
		return -ENOMEM;
	}
	rtk_plane->ringbase = vaddr;
	rtk_plane->ringheader = (struct tag_ringbuffer_header *)
			((unsigned long)(vaddr)+(64*1024));

	vaddr = dma_alloc_coherent(drm->dev, SZ_2K,
				&rtk_plane->refclock_dma_handle,
				GFP_KERNEL | __GFP_NOWARN);

        if (!vaddr) {
		dev_err(drm->dev, "%s dma_alloc fail \n", __func__);
		dma_free_coherent(drm->dev, 65*SZ_1K, rtk_plane->ringbase,
				 rtk_plane->dma_handle);
		return -ENOMEM;
	}

	rtk_plane->refclock = (struct tag_refclock *)(vaddr);

	exp_info.ops = &rtkplane_dma_buf_ops;
	exp_info.size = SZ_2K;
	exp_info.flags = O_RDWR;
	exp_info.priv = rtk_plane;
	rtk_plane->refclock_dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(rtk_plane->refclock_dmabuf)) {
		dev_err(drm->dev, "%s dmabuf export fail \n", __func__);
		dma_free_coherent(drm->dev, SZ_2K, vaddr,
				 rtk_plane->refclock_dma_handle);
		dma_free_coherent(drm->dev, 65*SZ_1K, rtk_plane->ringbase,
				 rtk_plane->dma_handle);
		return PTR_ERR(rtk_plane->refclock_dmabuf);
	}
#endif

	if (rpc_create_video_agent(rpc_info, &id, VF_TYPE_VIDEO_OUT)) {
		DRM_ERROR("rpc_create_video_agent RPC fail\n");
		return -1;
	}

	rtk_plane->info.instance = id;
	rtk_plane->info.videoPlane = videoplane;
	rtk_plane->info.zeroBuffer = 0;
	rtk_plane->info.realTimeSrc = 0;

	if (rpc_video_display(rpc_info, &rtk_plane->info)) {
		DRM_ERROR("rpc_video_display RPC fail\n");
		return -1;
	}

	rect.x = 0;
	rect.y = 0;
	rect.width = 0;
	rect.height = 0;

	rtk_plane->disp_win.videoPlane = videoplane | (rtk_plane->mixer << 16);
	rtk_plane->disp_win.videoWin = rect;
	rtk_plane->disp_win.borderWin = rect;
	rtk_plane->disp_win.borderColor = blueBorder;
	rtk_plane->disp_win.enBorder = 0;

	if (rtk_plane->display_panel_usage != CLUSTER) {
		if (rpc_video_config_disp_win(rpc_info, &rtk_plane->disp_win)) {
			DRM_ERROR("rpc_video_config_disp_win RPC fail\n");
			return -1;
		}
	} else {
		dev_info(drm->dev, "%s is on cluster, do not init config disp win\n",
			plane_names[rtk_plane->layer_nr]);
	}


	rtk_plane->refclock->RCD = htonll(-1LL);
	rtk_plane->refclock->RCD_ext = htonl(-1L);
	rtk_plane->refclock->masterGPTS = htonll(-1LL);
	rtk_plane->refclock->GPTSTimeout = htonll(0LL);
	rtk_plane->refclock->videoSystemPTS = htonll(-1LL);
	rtk_plane->refclock->audioSystemPTS = htonll(-1LL);
	rtk_plane->refclock->videoRPTS = htonll(-1LL);
	rtk_plane->refclock->audioRPTS = htonll(-1LL);
	rtk_plane->refclock->videoContext = htonl(-1);
	rtk_plane->refclock->audioContext = htonl(-1);
	rtk_plane->refclock->videoEndOfSegment = htonl(-1);
	rtk_plane->refclock->videoFreeRunThreshold = htonl(0x7FFFFFFF);
	rtk_plane->refclock->audioFreeRunThreshold = htonl(0x7FFFFFFF);
	rtk_plane->refclock->VO_Underflow = htonl(0);
	rtk_plane->refclock->AO_Underflow = htonl(0);
	rtk_plane->refclock->mastership.systemMode = (unsigned char)AVSYNC_FORCED_SLAVE;
	rtk_plane->refclock->mastership.videoMode = (unsigned char)AVSYNC_FORCED_MASTER;
	rtk_plane->refclock->mastership.audioMode = (unsigned char)AVSYNC_FORCED_MASTER;
	rtk_plane->refclock->mastership.masterState = (unsigned char)AUTOMASTER_NOT_MASTER;
	refclock.instance = id;
	refclock.pRefClock = (long)(0xffffffff&(rtk_plane->refclock_dma_handle));
	if (rpc_video_set_refclock(rpc_info, &refclock)) {
		DRM_ERROR("rpc_video_set_refclock RPC fail\n");
		return -1;
	}
	if (rpc_info->krpc_vo_opt != RPC_AUDIO) {
		rtk_plane->ringheader->beginAddr = (long)(0xffffffff&(rtk_plane->dma_handle));
		rtk_plane->ringheader->size = 64*1024;
		rtk_plane->ringheader->bufferID = 1;
	} else {
		rtk_plane->ringheader->beginAddr = htonl((long)(0xffffffff&(rtk_plane->dma_handle)));
		rtk_plane->ringheader->size = htonl(64*1024);
		rtk_plane->ringheader->bufferID = htonl(1);
	}
	rtk_plane->ringheader->writePtr = rtk_plane->ringheader->beginAddr;
	rtk_plane->ringheader->readPtr[0] = rtk_plane->ringheader->beginAddr;
	memset(&ringbuffer, 0, sizeof(ringbuffer));
	ringbuffer.instance = id;
	ringbuffer.readPtrIndex = 0;
	ringbuffer.pinID = 0;
	ringbuffer.pRINGBUFF_HEADER = (long)(0xffffffff&(rtk_plane->dma_handle))+64*1024;

	if (rpc_video_init_ringbuffer(rpc_info, &ringbuffer)) {
		DRM_ERROR("rpc_video_int_ringbuffer RPC fail\n");
		return -1;
	}

	if (rpc_video_run(rpc_info, id)) {
		DRM_ERROR("rpc_video_run RPC fail\n");
		return -1;
	}

	rtk_plane->flags |= RPC_READY;

	rtk_plane->info.instance = id;
	rtk_plane->info.videoPlane = videoplane;
	rtk_plane->info.zeroBuffer = 1;
	rtk_plane->info.realTimeSrc = 0;

	if (rpc_video_display(rpc_info, &rtk_plane->info)) {
		DRM_ERROR("rpc_video_display RPC fail\n");
		return -1;
	}

	if(videoplane == VO_VIDEO_PLANE_V1)
		err = device_create_file(drm->dev, &dev_attr_enable_video_display);
	else if(videoplane == VO_VIDEO_PLANE_OSD1)
		err = device_create_file(drm->dev, &dev_attr_enable_osd_display);
	else if(videoplane == VO_VIDEO_PLANE_SUB1)
		err = device_create_file(drm->dev, &dev_attr_enable_sub_display);
	else
		DRM_DEBUG("Not create %d plane for device attribute\n", videoplane);

	if (err < 0)
		DRM_ERROR("failed to create %d plane devide attribute\n", videoplane);

	return 0;
}

void rtk_plane_destroy(struct drm_plane *plane)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct drm_device *drm = rtk_plane->plane.dev;
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	enum drm_plane_type type = plane->type;
	enum VO_VIDEO_PLANE videoplane;

#ifndef CONFIG_CHROME_PLATFORMS
	struct dma_buf_attachment *attach;
	struct dma_buf *dmabuf;
#endif

	if (rtk_plane->rtk_fence)
		rtk_drm_fence_uninit(rtk_plane);

	rpc_destroy_video_agent(rpc_info, rtk_plane->info.instance);

	dma_free_coherent(drm->dev, 65*SZ_1K, rtk_plane->ringbase,
				 rtk_plane->dma_handle);

#ifndef CONFIG_CHROME_PLATFORMS
	attach = rtk_plane->refclock_attach;
	dmabuf = attach->dmabuf;
	BUG_ON(!dmabuf);
	BUG_ON(!dmabuf->ops);
	BUG_ON(!dmabuf->ops->end_cpu_access);

	if (dmabuf->vmap_ptr.vaddr)
		dma_buf_vunmap(dmabuf, &rtk_plane->map);

	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	dma_buf_unmap_attachment(attach, attach->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
#else
	dma_buf_put(rtk_plane->refclock_dmabuf);
#endif

	if (type == DRM_PLANE_TYPE_PRIMARY)
		videoplane = VO_VIDEO_PLANE_OSD1;
	else if (type == DRM_PLANE_TYPE_CURSOR)
		videoplane = VO_VIDEO_PLANE_SUB1;
	else
		videoplane = VO_VIDEO_PLANE_V1;

	if (videoplane == VO_VIDEO_PLANE_V1)
		device_remove_file(drm->dev, &dev_attr_enable_video_display);
	else if (videoplane == VO_VIDEO_PLANE_OSD1)
		device_remove_file(drm->dev, &dev_attr_enable_osd_display);
	else if (videoplane == VO_VIDEO_PLANE_SUB1)
		device_remove_file(drm->dev, &dev_attr_enable_sub_display);

	drm_plane_cleanup(plane);
}

struct drm_plane_state *
rtk_plane_atomic_plane_duplicate_state(struct drm_plane *plane)
{
	struct rtk_drm_plane_state *state;
	struct rtk_drm_plane_state *copy;

	if (WARN_ON(!plane->state))
		return NULL;

	state = to_rtk_plane_state(plane->state);
	copy = kmemdup(state, sizeof(*state), GFP_KERNEL);
	if (copy == NULL)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &copy->state);

	return &copy->state;
}

void rtk_plane_atomic_plane_destroy_state(struct drm_plane *plane,
					   struct drm_plane_state *state)
{
	__drm_atomic_helper_plane_destroy_state(state);
	kfree(to_rtk_plane_state(state));
}

static void rtk_plane_atomic_plane_reset(struct drm_plane *plane)
{
	struct rtk_drm_plane_state *state;

	if (plane->state) {
		rtk_plane_atomic_plane_destroy_state(plane, plane->state);
		plane->state = NULL;
	}

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return;

	__drm_atomic_helper_plane_reset(plane, &state->state);
}

static int rtk_plane_atomic_set_property(struct drm_plane *plane,
				   struct drm_plane_state *state,
				   struct drm_property *property,
				   uint64_t val)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_drm_plane_state *s =	to_rtk_plane_state(state);

	if (property == rtk_plane->display_idx_prop) {
		s->display_idx = (unsigned int)val;
		return 0;
	}

	if (property == rtk_plane->rtk_meta_data_prop) {
		struct drm_property_blob *meta_data =
			drm_property_lookup_blob(rtk_plane->plane.dev, val);

		if (meta_data->length > sizeof(struct video_object)) {
			DRM_ERROR("Meta data structure size not match\n");
			meta_data->length = sizeof(struct video_object);
		}
		memcpy(&s->rtk_meta_data, meta_data->data, meta_data->length);

		drm_property_blob_put(meta_data);

		return 0;
	}

	if (property == rtk_plane->vo_plane_name_prop)
		return 0;

	if (property == rtk_plane->out_fence_ptr) {
		s32 __user *fence_ptr = u64_to_user_ptr(val);

		if (!fence_ptr) {
			DRM_ERROR("No plane out-fence-ptr\n");
			return 0;
		}

		s->out_fence_ptr = fence_ptr;

		DRM_DEBUG_DRIVER("%s's fence (%p) has out-fence-ptr (%p)\n",
			plane_names[rtk_plane->layer_nr], rtk_plane->rtk_fence, s->out_fence_ptr);

		if (rtk_plane->rtk_fence && s->out_fence_ptr) {
			if (!rtk_drm_fence_create(rtk_plane->rtk_fence, s->out_fence_ptr)) {
				s->out_fence_ptr = NULL;
			}
		}

		return 0;
	}

	DRM_ERROR("failed to set rtk plane atomic property\n");
	return -EINVAL;
}


static int rtk_plane_atomic_get_property(struct drm_plane *plane,
				   const struct drm_plane_state *state,
				   struct drm_property *property,
				   uint64_t *val)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	const struct rtk_drm_plane_state *s =
		container_of(state, const struct rtk_drm_plane_state, state);

	if (property == rtk_plane->display_idx_prop) {
		*val = s->display_idx;
		return 0;
	}

	if (property == rtk_plane->rtk_meta_data_prop) {
		*val = (s->rtk_meta_data_blob) ? s->rtk_meta_data_blob->base.id : 0;
		return 0;
	}

	if (property == rtk_plane->vo_plane_name_prop) {
		*val = rtk_plane->layer_nr;
		return 0;
	}

	if (property == rtk_plane->out_fence_ptr) {
		*val = 0;
		return 0;
	}

	DRM_ERROR("failed to get rtk plane atomic property\n");
	return -EINVAL;
}

static bool rtk_plane_mod_supported(struct drm_plane *plane,
				   u32 format, u64 modifier)
{
	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return true;

	if (modifier == RTK_AFBC_MOD)
		return true;
	else if (modifier == RTK_AFRC_CU16_MOD)
		return true;
	else if (modifier == RTK_AFRC_CU24_MOD)
		return true;
	else if (modifier == RTK_AFRC_CU32_MOD)
		return true;
	else
		return false;
}

static const struct drm_plane_funcs rtk_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.destroy = rtk_plane_destroy,
	.disable_plane = drm_atomic_helper_disable_plane,
	.reset = rtk_plane_atomic_plane_reset,
	.atomic_duplicate_state = rtk_plane_atomic_plane_duplicate_state,
	.atomic_destroy_state = rtk_plane_atomic_plane_destroy_state,
	.atomic_set_property = rtk_plane_atomic_set_property,
	.atomic_get_property = rtk_plane_atomic_get_property,
	.format_mod_supported = rtk_plane_mod_supported,
};

static int rtk_plane_atomic_check(struct drm_plane *plane,
				  struct drm_atomic_state *state)
{
	DRM_DEBUG_DRIVER("%d\n", __LINE__);

	return 0;
}

#define MIN_UPDATE_INTERVAL 20

static void rtk_plane_atomic_update(struct drm_plane *plane,
				    struct drm_atomic_state *old_state)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct drm_crtc *crtc = plane->state->crtc;
	struct drm_framebuffer *fb = plane->state->fb;

	if (!crtc || WARN_ON(!fb)) {
		DRM_ERROR("NULL crtc or fb\n");
		return;
	}

	DRM_DEBUG_DRIVER("crtc[%d]-%s\n",
		crtc->index, mixer_names[rtk_plane->mixer]);

	if (plane->type == DRM_PLANE_TYPE_OVERLAY)
		rtk_plane_update_video_obj(plane);
	else if (plane->type == DRM_PLANE_TYPE_PRIMARY)
		rtk_plane_update_graphic_obj(plane);
	else {
		DRM_DEBUG_DRIVER("Please consider putting %s on OVERLAY/PRIMARY\n",
			plane_names[rtk_plane->layer_nr]);
		return;
	}

	if (rtk_plane->display_panel_usage == CLUSTER) {
		if (rtk_plane_update_cluster_scaling(plane)) {
			DRM_ERROR("rtk_plane_update_cluster_scaling fail\n");
			return;
		}
	} else {
		if (rtk_plane_update_scaling(plane)) {
			DRM_ERROR("rtk_plane_update_scaling fail\n");
			return;
		}
	}
}

static void rtk_plane_atomic_disable(struct drm_plane *plane,
				struct drm_atomic_state *old_state)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct vo_rectangle rect;

	DRM_DEBUG_DRIVER("(%s)\n", plane_names[rtk_plane->layer_nr]);

	rect.x = 0;
	rect.y = 0;
	rect.width = 0;
	rect.height = 0;

	rtk_plane->disp_win.videoWin = rect;
	rtk_plane->disp_win.borderWin = rect;

	if (rpc_video_config_disp_win(rpc_info, &rtk_plane->disp_win))
		DRM_ERROR("rpc_video_config_disp_win RPC fail\n");

	if (plane->type == DRM_PLANE_TYPE_PRIMARY) {
		rtk_plane->wait_a_vsync = false;
		wait_event_interruptible_timeout(rtk_plane->wait_vsync,
			rtk_plane->wait_a_vsync == true, msecs_to_jiffies(16));
	}

	if (plane->type == DRM_PLANE_TYPE_CURSOR) {
		if (rtk_plane_inband_config_disp_win(plane, &rtk_plane->disp_win)) {
			DRM_ERROR("rtk_plane_inband_config_disp_win fail\n");
		}
	}
}

static int rtk_plane_atomic_async_check(struct drm_plane *plane,
           struct drm_atomic_state *state)
{
	if (plane->state->fb->width < 257 && plane->state->fb->height < 257)
		return 0;
	return -EINVAL;
}

static void rtk_plane_atomic_async_update(struct drm_plane *plane,
            struct drm_atomic_state *state)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct drm_plane_state *new_plane_state;

	new_plane_state = drm_atomic_get_new_plane_state(state, plane);

	plane->state->crtc_x = new_plane_state->crtc_x;
	plane->state->crtc_y = new_plane_state->crtc_y;
	plane->state->crtc_h = new_plane_state->crtc_h;
	plane->state->crtc_w = new_plane_state->crtc_w;
	plane->state->src_x  = new_plane_state->src_x;
	plane->state->src_y  = new_plane_state->src_y;
	plane->state->src_h  = new_plane_state->src_h;
	plane->state->src_w  = new_plane_state->src_w;

	swap(plane->state->fb, new_plane_state->fb);

	if (ktime_before(ktime_get(),
		ktime_add_ms(rtk_plane->update_time, MIN_UPDATE_INTERVAL)))
		return;

	rtk_plane->update_time = ktime_get();

	if (plane->type == DRM_PLANE_TYPE_CURSOR)
		rtk_plane_update_cursor(plane);

	if (rtk_plane_update_scaling(plane)) {
		DRM_ERROR("rtk_plane_update_scaling fail\n");
		return;
	}
}

static const struct drm_plane_helper_funcs rtk_plane_helper_funcs = {
	.atomic_check = rtk_plane_atomic_check,
	.atomic_update = rtk_plane_atomic_update,
	.atomic_disable = rtk_plane_atomic_disable,
	.atomic_async_check = rtk_plane_atomic_async_check,
	.atomic_async_update = rtk_plane_atomic_async_update,
};

int rtk_plane_init(struct drm_device *drm, struct rtk_drm_plane *rtk_plane,
		   unsigned long possible_crtcs, enum drm_plane_type type,
		   enum VO_VIDEO_PLANE layer_nr, struct rtk_rpc_info *rpc_info)
{
	struct drm_plane *plane = &rtk_plane->plane;
	// struct rtk_drm_private *priv = drm->dev_private;
	int err;
	const uint32_t *plane_formats;
	unsigned int format_count;
	const uint64_t *format_modifiers;

	if (type == DRM_PLANE_TYPE_OVERLAY) {

		if (soc_device_match(rtk_soc_kent)) {
			plane_formats = video_formats_kent;
			format_count = ARRAY_SIZE(video_formats_kent);
		} else {
			plane_formats = video_formats;
			format_count = ARRAY_SIZE(video_formats);
		}

		format_modifiers = format_modifiers_default;
	} else if (type == DRM_PLANE_TYPE_PRIMARY) {
		plane_formats = osd_formats;
		format_count = ARRAY_SIZE(osd_formats);
		format_modifiers = format_modifiers_arm;
	} else {
		plane_formats = other_formats;
		format_count = ARRAY_SIZE(other_formats);
		format_modifiers = format_modifiers_default;
	}

	err = drm_universal_plane_init(drm, plane, possible_crtcs,
				       &rtk_plane_funcs, plane_formats,
				       format_count, format_modifiers,
				       type, NULL);
	if (err) {
		DRM_ERROR("failed to initialize plane\n");
		return err;
	}

	drm_plane_helper_add(plane, &rtk_plane_helper_funcs);

	if(type == DRM_PLANE_TYPE_OVERLAY) {
		drm_plane_create_zpos_immutable_property(plane, 0);

		rtk_plane->display_idx_prop = drm_property_create_range(plane->dev, DRM_MODE_PROP_ATOMIC,
					"display_idx", 0, PLOCK_BUFFER_SET_SIZE-1);
		if (!rtk_plane->display_idx_prop) {
			pr_err("III %s create display_idx property fail\n", __func__);
			return -ENOMEM;
		}

		drm_object_attach_property(&plane->base, rtk_plane->display_idx_prop, 0);
	}
	else if (type == DRM_PLANE_TYPE_PRIMARY) {
		drm_plane_create_zpos_immutable_property(plane, 1);

		rtk_plane->out_fence_ptr = drm_property_create_range(plane->dev, DRM_MODE_PROP_ATOMIC,
					"OUT_FENCE_PTR", 0, U64_MAX);
		if (!rtk_plane->out_fence_ptr) {
			DRM_ERROR("create plane out fence ptr property fail\n");
			return -ENOMEM;
		}

		drm_object_attach_property(&plane->base, rtk_plane->out_fence_ptr, 0);
	}
	else {
		drm_plane_create_zpos_immutable_property(plane, 2);
	}

	rtk_plane->vo_plane_name_prop = drm_property_create_enum(plane->dev, 0, "plane name",
				vo_plane_list, ARRAY_SIZE(vo_plane_list));

	drm_object_attach_property(&plane->base, rtk_plane->vo_plane_name_prop, 0);
	rtk_plane->layer_nr = layer_nr;

	rtk_plane->rtk_meta_data_prop = drm_property_create(plane->dev,
							DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
							"RTK_META_DATA", 0);
	if (!rtk_plane->rtk_meta_data_prop) {
		DRM_ERROR("%s create rtk_meta_data_prop property fail\n", __func__);
		return -ENOMEM;
	}
	drm_object_attach_property(&plane->base, rtk_plane->rtk_meta_data_prop, 0);

	rtk_plane->rpc_info = rpc_info;

	DRM_INFO("%s's rpc_info (%p)\n",
		plane_names[rtk_plane->layer_nr], rtk_plane->rpc_info);

	rtk_plane->gAlpha = 0;
	rtk_plane->flags &= ~BG_SWAP;
	rtk_plane->flags |= VSYNC_FORCE_LOCK;

	rtk_plane_rpc_init(rtk_plane, layer_nr);

	if (type == DRM_PLANE_TYPE_PRIMARY)
		rtk_drm_fence_init(rtk_plane);

	rtk_plane->wait_a_vsync = true;
	init_waitqueue_head(&rtk_plane->wait_vsync);

	return 0;
}

int rtk_plane_export_refclock_fd_ioctl(struct drm_device *dev,
			    void *data, struct drm_file *file)
{
	struct drm_rtk_refclk *refclk = (struct drm_rtk_refclk *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, refclk->plane_id);
	struct rtk_drm_plane *rtk_plane =  container_of(plane, struct rtk_drm_plane, plane);
	int ret = 0;

	get_dma_buf(rtk_plane->refclock_dmabuf);
	refclk->fd = dma_buf_fd(rtk_plane->refclock_dmabuf, O_CLOEXEC);
	if (refclk->fd < 0) {
		dma_buf_put(rtk_plane->refclock_dmabuf);
		return refclk->fd;
	}

	return ret;
}

int rtk_plane_get_plane_id(struct drm_device *dev,
			    void *data, struct drm_file *file)
{
	struct drm_rtk_vo_plane *rtk_vo_plane = (struct drm_rtk_vo_plane *)data;
	struct drm_plane *plane;
	struct rtk_drm_plane *rtk_plane;
	int ret = -1;

	drm_for_each_plane(plane, dev) {
		rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
		if(rtk_plane->info.videoPlane == rtk_vo_plane->vo_plane) {
			rtk_vo_plane->plane_id = rtk_plane->plane.base.id;
			ret = 0;
			break;
		}
	}

	return ret;
}

int rtk_plane_set_q_param(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_set_q_param(rpc_info, (struct rpc_set_q_param *)data);
}

int rtk_plane_config_channel_lowdelay(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_config_channel_lowdelay *param = (struct rpc_config_channel_lowdelay *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;
	struct rpc_config_channel_lowdelay arg;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	arg.mode = param->mode;
	arg.instanceId = rtk_plane->info.instance;

	return rpc_video_config_channel_lowdelay(rpc_info, &arg);
}

int rtk_plane_query_dispwin_new(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_query_disp_win_out_new *param = (struct rpc_query_disp_win_out_new *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct rpc_query_disp_win_in argp_in;

	argp_in.plane = rtk_plane->disp_win.videoPlane;

	return rpc_video_query_disp_win_new(rpc_info, &argp_in, param);
}

int rtk_plane_get_privateinfo(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_privateinfo_param *param = (struct rpc_privateinfo_param *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	param->instanceId = rtk_plane->info.instance;

	return rpc_video_privateinfo_param(rpc_info, param);
}

int rtk_plane_set_speed(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_set_speed *param = (struct rpc_set_speed *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	param->instanceId = rtk_plane->info.instance;

	return rpc_video_set_speed(rpc_info, param);
}

int rtk_plane_set_background(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_set_background(rpc_info, (struct rpc_set_background *)data);
}

int rtk_plane_keep_curpic(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_keep_curpic *param = (struct rpc_keep_curpic *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	param->plane = rtk_plane->disp_win.videoPlane;

	return rpc_video_keep_curpic(rpc_info, param);
}

int rtk_plane_keep_curpic_fw(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_keep_curpic *param = (struct rpc_keep_curpic *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	param->plane = rtk_plane->disp_win.videoPlane;

	return rpc_video_keep_curpic_fw(rpc_info, param);
}

int rtk_plane_keep_curpic_svp(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_keep_curpic_svp *param = (struct rpc_keep_curpic_svp *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	param->plane = rtk_plane->disp_win.videoPlane;

	return rpc_video_keep_curpic_svp(rpc_info, param);
}

int rtk_plane_set_deintflag(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_set_deintflag(rpc_info, (struct rpc_set_deintflag *)data);
}

int rtk_plane_create_graphic_win(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_create_graphic_win(rpc_info, (struct rpc_create_graphic_win *)data);
}

int rtk_plane_draw_graphic_win(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_draw_graphic_win(rpc_info, (struct rpc_draw_graphic_win *)data);
}

int rtk_plane_modify_graphic_win(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_modify_graphic_win(rpc_info, (struct rpc_modify_graphic_win *)data);
}

int rtk_plane_delete_graphic_win(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_delete_graphic_win(rpc_info, (struct rpc_delete_graphic_win *)data);
}

int rtk_plane_conf_osd_palette(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_config_osd_palette(rpc_info, (struct rpc_config_osd_palette *)data);
}

int rtk_plane_conf_plane_mixer(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_config_plane_mixer *param = (struct rpc_config_plane_mixer *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	param->instanceId = rtk_plane->info.instance;

	return rpc_video_config_plane_mixer(rpc_info, param);
}

int rtk_plane_set_tv_system(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_set_tv_system(rpc_info, (struct rpc_config_tv_system *)data);
}

int rtk_plane_get_tv_system(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_query_tv_system(rpc_info, (struct rpc_config_tv_system *)data);
}

int rtk_plane_set_dispout_format(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_set_display_format(rpc_info, (struct rpc_display_output_format *)data);
}

int rtk_plane_get_dispout_format(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_get_display_format(rpc_info, (struct rpc_display_output_format *)data);
}

int rtk_plane_set_hdmi_audio_mute(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_set_hdmi_audio_mute(rpc_info, (struct rpc_audio_mute_info *)data);
}

int rtk_plane_get_quick_dv_switch(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rpc_config_tv_system tvsystem;
	struct rpc_display_output_format format;
	struct rtk_rpc_info *rpc_info;
	uint32_t *dv_switch_fmt = (uint32_t *)data;
	int is_dv_switch = 0;
	int hdmiMode = 0;
	int hdr = 0;
	int color = 0;
	int bpc = 0;
	int ret = 0;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if (!rpc_info)
		return -1;

	if (rpc_info->hdmi_new_mac == NULL)
		return -ENXIO;

	if (*rpc_info->hdmi_new_mac) {
		memset_io(&format, 0, sizeof(format));
		ret = rpc_get_display_format(rpc_info, &format);
		hdmiMode = format.display_mode;
		color = format.colorspace;
		bpc = format.color_depth;
		hdr = format.hdr_mode;

		if (hdmiMode != DISPLAY_MODE_HDMI)
			return -ENOEXEC;

		if (bpc == 8 && color == COLORSPACE_RGB &&
			hdr == HDR_CTRL_DV_ON) {
			*dv_switch_fmt = QDVSTD_ON;
		} else if (bpc == 12 && color == COLORSPACE_YUV422 &&
			hdr == HDR_CTRL_INPUT) {
			*dv_switch_fmt = QDV_INPUT;
		} else if (bpc == 12 && color == COLORSPACE_YUV422 &&
			hdr == HDR_CTRL_DV_LOW_LATENCY_12b_YUV422) {
			*dv_switch_fmt = QDVLL_ON;
		} else {
			DRM_ERROR("Current format doesn't support quick dv switch");
			DRM_ERROR("color: %u, bpc :%u, hdr: %u", color, bpc, hdr);
			ret = -ENXIO;
		}
	} else {
		memset_io(&tvsystem, 0, sizeof(tvsystem));
		rpc_query_tv_system(rpc_info, &tvsystem);
		hdmiMode = tvsystem.info_frame.hdmiMode;
		color = (tvsystem.info_frame.dataByte1 >> 5) & 0x3;
		bpc = (tvsystem.info_frame.dataInt0 >> 2) & 0xF;
		hdr = tvsystem.info_frame.hdr_ctrl_mode;
		is_dv_switch = tvsystem.info_frame.hdmi2px_feature & HDMI2PX_QUICK_DV;

		if (hdmiMode != VO_HDMI_ON)
			return -ENOEXEC;

		if (bpc == 4 && color == COLORSPACE_RGB &&
			hdr == HDR_CTRL_DV_ON) {
			if (is_dv_switch)
				*dv_switch_fmt = QDVSTD_ON;
		} else if (bpc == 6 && color == COLORSPACE_YUV422 &&
			hdr == HDR_CTRL_INPUT) {
			if (is_dv_switch)
				*dv_switch_fmt = QDV_INPUT;
		} else if (bpc == 6 && color == COLORSPACE_YUV422 &&
			hdr == HDR_CTRL_DV_LOW_LATENCY_12b_YUV422) {
			if (is_dv_switch)
				*dv_switch_fmt = QDVLL_ON;
		} else {
			DRM_ERROR("Current format doesn't support quick dv switch");
			DRM_ERROR("color: %u, depth: %u, hdr: %u", color, bpc, hdr);
			ret = -ENXIO;
		}
	}

	return ret;
}

int rtk_plane_set_quick_dv_switch(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rpc_display_output_format format;
	struct rpc_config_tv_system tvsystem;
	struct rtk_rpc_info *rpc_info;
	uint32_t *dv_switch_fmt = (uint32_t *)data;
	int hdmiMode = 0;
	int color = 0;
	int bpc = 0;
	int hdr = 0;
	int ret = 0;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if (!rpc_info)
		return -1;

	if (rpc_info->hdmi_new_mac == NULL)
		return -ENXIO;

	DRM_INFO("Set quick dv switch format= %u\n", *dv_switch_fmt);

	if (*rpc_info->hdmi_new_mac) {
		memset_io(&format, 0, sizeof(format));
		ret = rpc_get_display_format(rpc_info, &format);
		hdmiMode = format.display_mode;
		color = format.colorspace;
		bpc = format.color_depth;
		hdr = format.hdr_mode;

		if (hdmiMode != DISPLAY_MODE_HDMI)
			return -ENOEXEC;

		switch (*dv_switch_fmt) {
		case QDVLL_ON:
			format.quick_dv_switch = 1;
			format.colorspace = COLORSPACE_YUV422;
			format.color_depth = 12;
			format.hdr_mode = HDR_CTRL_DV_LOW_LATENCY_12b_YUV422;
			break;
		case QDVSTD_ON:
			format.quick_dv_switch = 1;
			format.colorspace = COLORSPACE_RGB;
			format.color_depth = 8;
			format.hdr_mode = HDR_CTRL_DV_ON;
			break;
		case QDV_INPUT:
			format.quick_dv_switch = 1;
			format.colorspace = COLORSPACE_YUV422;
			format.color_depth = 12;
			format.hdr_mode = HDR_CTRL_INPUT;
			break;
		default:
			DRM_ERROR("Unsupport set quick_dv_switch format: %u\n", *dv_switch_fmt);
			return -ENOEXEC;
		}

		if (bpc == 8 && hdr == HDR_CTRL_DV_ON &&
			color == COLORSPACE_RGB &&
			*dv_switch_fmt != QDVSTD_ON) {
			ret = rpc_set_display_format(rpc_info, &format);
		} else if (bpc == 12 && hdr == HDR_CTRL_INPUT &&
			color == COLORSPACE_YUV422 &&
			*dv_switch_fmt != QDV_INPUT) {
			ret = rpc_set_display_format(rpc_info, &format);
		} else if (bpc == 12 && hdr == HDR_CTRL_DV_LOW_LATENCY_12b_YUV422 &&
			color == COLORSPACE_YUV422 &&
			*dv_switch_fmt != QDVLL_ON) {
			ret = rpc_set_display_format(rpc_info, &format);
		} else {
			DRM_ERROR("Current format doesn't support quick dv switch");
			DRM_ERROR("color: %u, bpc: %u, hdr: %u", color, bpc, hdr);
			return -ENOEXEC;
		}
	} else {
		memset_io(&tvsystem, 0, sizeof(tvsystem));
		rpc_query_tv_system(rpc_info, &tvsystem);
		hdmiMode = tvsystem.info_frame.hdmiMode;
		color = (tvsystem.info_frame.dataByte1 >> 5) & 0x3;
		bpc = (tvsystem.info_frame.dataInt0 >> 2) & 0xF;
		hdr = tvsystem.info_frame.hdr_ctrl_mode;

		if (hdmiMode != VO_HDMI_ON)
			return -ENOEXEC;

		switch (*dv_switch_fmt) {
		case QDVLL_ON:
			tvsystem.info_frame.hdmi2px_feature |= HDMI2PX_QUICK_DV;
			tvsystem.info_frame.dataByte1 = 0x32;
			tvsystem.info_frame.dataByte2 &= 0x30;
			tvsystem.info_frame.dataByte2 |= 0x08;
			tvsystem.info_frame.dataByte3 = 0x0;
			tvsystem.info_frame.dataInt0 = RPC_BPC_12;
			tvsystem.videoInfo.dataInt0 &= 0x06;
			tvsystem.videoInfo.dataInt0 |= RPC_BPC_12;
			tvsystem.videoInfo.enDIF = 0x1;
			tvsystem.info_frame.hdr_ctrl_mode = HDR_CTRL_DV_LOW_LATENCY_12b_YUV422;
			break;
		case QDVSTD_ON:
			tvsystem.info_frame.hdmi2px_feature |= HDMI2PX_QUICK_DV;
			tvsystem.info_frame.dataByte1 = 0x12;
			tvsystem.info_frame.dataByte2 &= 0x30;
			tvsystem.info_frame.dataByte2 |= 0x08;
			tvsystem.info_frame.dataByte3 = 0x0;
			tvsystem.info_frame.dataInt0 = RPC_BPC_8;
			tvsystem.videoInfo.dataInt0 &= 0x06;
			tvsystem.videoInfo.dataInt0 |= RPC_BPC_8;
			tvsystem.videoInfo.enDIF = 0x1;
			tvsystem.info_frame.hdr_ctrl_mode = HDR_CTRL_DV_ON;
			break;
		case QDV_INPUT:
			tvsystem.info_frame.hdmi2px_feature |= HDMI2PX_QUICK_DV;
			tvsystem.info_frame.dataByte1 = 0x32;
			tvsystem.info_frame.dataByte2 &= 0x30;
			tvsystem.info_frame.dataByte2 |= 0x08;
			tvsystem.info_frame.dataByte3 = 0x0;
			tvsystem.info_frame.dataInt0 = RPC_BPC_12;
			tvsystem.videoInfo.dataInt0 &= 0x06;
			tvsystem.videoInfo.dataInt0 |= RPC_BPC_12;
			tvsystem.videoInfo.enDIF = 0x1;
			tvsystem.info_frame.hdr_ctrl_mode = HDR_CTRL_INPUT;
			break;
		default:
			DRM_ERROR("Unsupport set quick_dv_switch format: %u\n", *dv_switch_fmt);
			return -ENOEXEC;
		}

		if (bpc == 4 && hdr == HDR_CTRL_DV_ON &&
			color == COLORSPACE_RGB &&
			*dv_switch_fmt != QDVSTD_ON) {
			ret = rpc_set_tv_system(rpc_info, &tvsystem);
		} else if (bpc == 6 && hdr == HDR_CTRL_INPUT &&
			color == COLORSPACE_YUV422 &&
			*dv_switch_fmt != QDV_INPUT) {
			ret = rpc_set_tv_system(rpc_info, &tvsystem);
		} else if (bpc == 6 && hdr == HDR_CTRL_DV_LOW_LATENCY_12b_YUV422 &&
			color == COLORSPACE_YUV422 &&
			*dv_switch_fmt != QDVLL_ON) {
			ret = rpc_set_tv_system(rpc_info, &tvsystem);
		} else {
			DRM_ERROR("Current format doesn't support quick dv switch");
			DRM_ERROR("color: %u, depth: %u, hdr: %u", color, bpc, hdr);
			return -ENOEXEC;
		}
	}

	return ret;
}

int rtk_plane_set_sdrflag(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if(!rpc_info)
		return -1;

	return rpc_video_set_sdrflag(rpc_info, (struct rpc_set_sdrflag *)data);
}

int rtk_plane_set_pause_ioctl(struct drm_device *dev,
			    void *data, struct drm_file *file)
{
	struct drm_rtk_pause *pause = (struct drm_rtk_pause *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, pause->plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	if(pause->enable) {
		if (rpc_video_pause(rpc_info, rtk_plane->info.instance)) {
			DRM_ERROR("rpc_video_pause RPC fail\n");
			return -1;
		}
	} else {
		if (rpc_video_run(rpc_info, rtk_plane->info.instance)) {
			DRM_ERROR("rpc_video_run RPC fail\n");
			return -1;
		}
	}

	return 0;
}

int rtk_plane_set_flush_ioctl(struct drm_device *dev,
			    void *data, struct drm_file *file)
{
	uint32_t *plane_id = (uint32_t *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, *plane_id);
	struct rtk_drm_plane *rtk_plane;
	struct rtk_rpc_info *rpc_info;

	if (IS_ERR_OR_NULL(plane))
		return -1;

	rtk_plane = container_of(plane, struct rtk_drm_plane, plane);
	rpc_info = rtk_plane->rpc_info;

	if(!rpc_info)
		return -1;

	if (rpc_video_flush(rpc_info, rtk_plane->info.instance)) {
		DRM_ERROR("rpc_video_flush RPC fail\n");
		return -1;
	}

	return 0;
}

int rtk_plane_get_mixer_by_videoplane(struct rtk_drm_crtc *rtk_crtc, enum VO_VIDEO_PLANE layer_nr)
{
	struct rtk_rpc_info *rpc_info = rtk_crtc->rpc_info;
	struct rpc_query_disp_win_in disp_win_in;
	struct rpc_query_disp_win_out_new disp_win_out;
	int ret = 0;

	DRM_INFO("rtk_plane_get_mixer_by_videoplane\n");

	disp_win_in.plane = layer_nr;

	ret = rpc_video_query_disp_win_new(rpc_info, &disp_win_in, &disp_win_out);
	if (ret) {
		DRM_ERROR("rpc_video_query_disp_win_new RPC fail\n");
		return -1;
	}

	rtk_crtc->mixer = disp_win_out.is_on_mixer;

	DRM_INFO("%s is on %s (from vo)\n", plane_names[disp_win_in.plane],
							mixer_names[rtk_crtc->mixer]);

	return 0;
}

int rtk_plane_get_mixer_from_cluster(struct rtk_drm_crtc *rtk_crtc, enum VO_VIDEO_PLANE layer_nr)
{
	struct rtk_rpc_info *rpc_info = rtk_crtc->rpc_info;
	struct rpc_query_mixer_by_plane_in mixer_by_plane_in;
	struct rpc_query_mixer_by_plane_out mixer_by_plane_out;
	int ret = 0;

	DRM_INFO("rtk_plane_get_mixer_from_cluster\n");

	mixer_by_plane_in.video_plane = layer_nr;
	mixer_by_plane_in.config_no   = 1;

	ret = rpc_query_mixer_by_plane(rpc_info, &mixer_by_plane_in, &mixer_by_plane_out);
	if (ret) {
		DRM_ERROR("rpc_query_mixer_by_plane RPC fail\n");
		return -1;
	}

	rtk_crtc->mixer = mixer_by_plane_out.mixer;

	DRM_INFO("%s is on %s (from cluster)\n", plane_names[mixer_by_plane_in.video_plane],
							mixer_names[rtk_crtc->mixer]);

	return 0;
}

int rtk_plane_set_cvbs_format(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info;
	uint32_t *p_cvbs_fmt = (uint32_t *)data;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if (!rpc_info)
		return -ENODEV;

	DRM_INFO("Set cvbs fmt=%u\n", *p_cvbs_fmt);

	return rpc_set_cvbs_format(rpc_info, *p_cvbs_fmt);
}

int rtk_plane_get_cvbs_format(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info;

	if (priv->krpc_second == 1)
		rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if (!rpc_info)
		return -ENODEV;

	return rpc_get_cvbs_format(rpc_info, (u32 *)data);
}

static int plane_display_get(struct rtk_rpc_info *rpc_info,
	struct vo_rectangle* rect, enum VO_VIDEO_PLANE plane_type)
{
	struct rpc_query_disp_win_in structQueryDispWin_in;
	struct rpc_query_disp_win_out structQueryDispWin_out;

	mutex_lock(&enable_display_mutex);

	memset(&structQueryDispWin_in, 0, sizeof(structQueryDispWin_in));
	memset(&structQueryDispWin_out, 0, sizeof(structQueryDispWin_out));

	structQueryDispWin_in.plane = plane_type;

	if (rpc_video_query_dis_win(rpc_info, &structQueryDispWin_in, &structQueryDispWin_out))
	{
		DRM_ERROR("[%s %d]\n", __FUNCTION__, __LINE__);
		return -1;
	}

	rect->x = structQueryDispWin_out.configWin.x;
	rect->y = structQueryDispWin_out.configWin.y;
	rect->width = structQueryDispWin_out.configWin.width;
	rect->height = structQueryDispWin_out.configWin.height;
	mutex_unlock(&enable_display_mutex);

	return 0;
}

static int plane_display_set(struct rtk_rpc_info *rpc_info,
	struct vo_rectangle* rect, enum VO_VIDEO_PLANE plane_type)
{
	mutex_lock(&enable_display_mutex);

	if (plane_type == VO_VIDEO_PLANE_V1 || plane_type == VO_VIDEO_PLANE_OSD1)
	{
		struct rpc_config_disp_win structConfigDispWin;
		struct vo_color blueBorder = {0,0,255,1};

		memset(&structConfigDispWin, 0, sizeof(structConfigDispWin));
		structConfigDispWin.videoPlane = plane_type;
		structConfigDispWin.videoWin = *rect;
		structConfigDispWin.borderWin = *rect;
		structConfigDispWin.borderColor = blueBorder;
		structConfigDispWin.enBorder = 0;
		if (rpc_video_config_disp_win(rpc_info, &structConfigDispWin))
		{
			DRM_ERROR("[%s %d]\n", __FUNCTION__, __LINE__);
			return -1;
		}
	}else if (plane_type == VO_VIDEO_PLANE_SUB1)
	{
		struct rpc_config_graphic_canvas  structConfigGraphicCanvas;

		memset(&structConfigGraphicCanvas, 0, sizeof(structConfigGraphicCanvas));
		structConfigGraphicCanvas.plane = VO_GRAPHIC_SUB1;
		structConfigGraphicCanvas.srcWin.width = 256;
		structConfigGraphicCanvas.srcWin.height = 256;
		structConfigGraphicCanvas.srcWin.x = 0;
		structConfigGraphicCanvas.srcWin.y = 0;
		structConfigGraphicCanvas.dispWin = *rect;
		structConfigGraphicCanvas.go = 1;
		if (rpc_video_config_graphic(rpc_info, &structConfigGraphicCanvas))
		{
			DRM_ERROR("[%s %d]\n", __FUNCTION__, __LINE__);
			return -1;
		}
	}else
	{
		DRM_ERROR("[%s %d]\n", __FUNCTION__, __LINE__);
		return -1;
	}

	mutex_unlock(&enable_display_mutex);
	return 0;
}

ssize_t rtk_plane_enable_osd_display_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];
	unsigned long state;
	int ret;
	struct vo_rectangle rect = {0};

	ret = kstrtol(buf, 0, &state);
	/* valid input value: 0 or 1 */
	if (ret != 0 || state & INVERT_BITVAL_1)
	    return -EINVAL;
	if (state == 0){
	    if(plane_display_get(rpc_info, &rect, VO_VIDEO_PLANE_OSD1) != 0)
	        return -EINVAL;
	    if (rect.x == 0 && rect.y == 0 && rect.width == 0 && rect.height == 0){
	        DRM_ERROR("osd1 plane is already disabled \n");
	        return count;
	    }
	    rect_osd1 = rect;
	    if(plane_display_set(rpc_info, &rect_plane_disabled, VO_VIDEO_PLANE_OSD1) != 0)
	        return -EINVAL;
	}else if(state == 1){
	    if(plane_display_get(rpc_info, &rect, VO_VIDEO_PLANE_OSD1) != 0)
	        return -EINVAL;
	    if (rect.x != 0 || rect.y != 0 || rect.width != 0 || rect.height != 0){
	        DRM_ERROR("osd1 plane is already enabled \n");
	        return count;
	    }
	    if(plane_display_set(rpc_info, &rect_osd1, VO_VIDEO_PLANE_OSD1) != 0)
	        return -EINVAL;
	}else{
	    DRM_ERROR("enable_osd_display_store fail \n");
	}
	return count;
}

ssize_t rtk_plane_enable_sub_display_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];
	unsigned long state;
	int ret;
	struct vo_rectangle rect = {0};

	ret = kstrtol(buf, 0, &state);
	/* valid input value: 0 or 1 */
	if (ret != 0 || state & INVERT_BITVAL_1)
	    return -EINVAL;
	if (state == 0){
	    if(plane_display_get(rpc_info, &rect, VO_VIDEO_PLANE_SUB1) != 0)
	        return -EINVAL;
	    if (rect.x == 0 && rect.y == 0 && rect.width == 0 && rect.height == 0){
	        DRM_ERROR("sub1 plane is already disabled \n");
	        return count;
	    }
	    rect_sub1 = rect;
	    if(plane_display_set(rpc_info, &rect_plane_disabled, VO_VIDEO_PLANE_SUB1) != 0)
	        return -EINVAL;
	}else if(state == 1){
	    if(plane_display_get(rpc_info, &rect, VO_VIDEO_PLANE_SUB1) != 0)
	        return -EINVAL;
	    if (rect.x != 0 || rect.y != 0 || rect.width != 0 || rect.height != 0){
	        DRM_ERROR("sub1 plane is already enabled \n");
	        return count;
	    }
	    if(plane_display_set(rpc_info, &rect_sub1, VO_VIDEO_PLANE_SUB1) != 0)
	        return -EINVAL;
	}else{
	    DRM_ERROR("enable_sub_display_store fail \n");
	}
	return count;
}

ssize_t rtk_plane_enable_video_display_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info[RTK_RPC_MAIN];
	unsigned long state;
	int ret;
	struct vo_rectangle rect = {0};
	ret = kstrtol(buf, 0, &state);
	/* valid input value: 0 or 1 */
	if (ret != 0 || state & INVERT_BITVAL_1)
	    return -EINVAL;
	if (state == 0){
	    if(plane_display_get(rpc_info, &rect, VO_VIDEO_PLANE_V1) != 0)
	        return -EINVAL;
	    if (rect.x == 0 && rect.y == 0 && rect.width == 0 && rect.height == 0){
	        DRM_ERROR("video1 plane is already disabled \n");
	        return count;
	    }
	    rect_video1 = rect;
	    if(plane_display_set(rpc_info, &rect_plane_disabled, VO_VIDEO_PLANE_V1) != 0)
	        return -EINVAL;
	}else if(state == 1){
	    if(plane_display_get(rpc_info, &rect, VO_VIDEO_PLANE_V1) != 0)
	        return -EINVAL;
	    if (rect.x != 0 || rect.y != 0 || rect.width != 0 || rect.height != 0){
	        DRM_ERROR("video1 plane is already enabled \n");
	        return count;
	    }
	    if(plane_display_set(rpc_info, &rect_video1, VO_VIDEO_PLANE_V1) != 0)
	        return -EINVAL;
	}else{
	    DRM_ERROR("enable_video_display_store fail \n");
	}
	return count;
}

