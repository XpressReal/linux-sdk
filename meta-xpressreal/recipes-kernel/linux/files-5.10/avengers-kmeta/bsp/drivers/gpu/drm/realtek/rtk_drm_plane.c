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

#include <drm/drm_plane_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>	// DEBUG: struct drm_pending_vblank_event

#include "rtk_drm_drv.h"
#include "rtk_drm_fb.h"
#include "rtk_drm_gem.h"
#include "rtk_drm_crtc.h"

#define to_rtk_plane(s) container_of(s, struct rtk_drm_plane, plane)

#define INVERT_BITVAL_1 (~1)

static const unsigned int formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YVU420,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
};

struct rtk_drm_plane_state {
	struct drm_plane_state state;
	s32 __user *out_fence_ptr;
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

#ifdef ENABLE_TEE_DRM_FLOW
extern int ta_TEEapi_init(struct tee_context **teeapi_ctx, unsigned int *teeapi_tee_session);
extern int ta_TEEapi_deinit(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session);
extern int ta_TEEapi_memcpy(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int dstPhysAddr, unsigned int srtPhysAddr, int size);
#endif

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

static int write_cmd_to_ringbuffer(struct rtk_drm_plane *rtk_plane, void *cmd)
{
	void *base_iomap = rtk_plane->ringbase;
	struct tag_ringbuffer_header *rbHeader = rtk_plane->ringheader;
	unsigned int size = ((struct inband_cmd_pkg_header *)cmd)->size;
	unsigned int read = ipcReadULONG((u8 *)&rbHeader->readPtr[0]);
	unsigned int write = ipcReadULONG((u8 *)&(rbHeader->writePtr));
	unsigned int base = ipcReadULONG((u8 *)&(rbHeader->beginAddr));
	unsigned int b_size = ipcReadULONG((u8 *)&(rbHeader->size));
	unsigned int limit = base + b_size;

	if (read + (read > write ? 0 : limit - base) - write > size) {
		unsigned long offset = write - base;
		void *write_io = (void *)((unsigned long)base_iomap + offset);

		if (write + size <= limit) {
			ipcCopyMemory((void *)write_io, cmd, size);
		} else {
			ipcCopyMemory((void *)write_io, cmd, limit - write);
			ipcCopyMemory((void *)base_iomap, (void *)((unsigned long)cmd + limit - write), size - (limit - write));
		}
		write += size;
		write = write < limit ? write : write - (limit - base);

		rbHeader->writePtr = ipcReadULONG((u8 *)&write);
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

static int queue_ring_buffer(struct drm_plane *plane)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct drm_device *drm = rtk_plane->plane.dev;
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct rtk_drm_fence *rtk_fence = rtk_plane->rtk_fence;
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_object *gem[4];
	struct rtk_gem_object *rtk_gem[4];
	enum drm_plane_type type = plane->type;
	struct vo_rectangle *p_rect, rect;
	struct vo_color blueBorder = {0, 0, 255, 1};
	unsigned int videoplane;
	char *hdr_type_str[2];
	int i;
	int index;

	videoplane = rtk_plane->info.videoPlane;

	for (i=0; i<fb->format->num_planes; i++) {
		gem[i] = rtk_fb_get_gem_obj(fb, 0);
		rtk_gem[i] = to_rtk_gem_obj(gem[i]);
	}

	if (type == DRM_PLANE_TYPE_OVERLAY) {
		struct video_object *obj = (struct video_object *)kzalloc(sizeof(struct video_object), GFP_KERNEL);
		if(!obj) {
			DRM_ERROR("queue_ring_buffer malloc video_object fail\n");
			return -1;
		}
		init_video_object(obj);

		if (rtk_gem[0]->dmabuf_type == DMABUF_TYPE_NORMAL) {
			obj->header.type = VIDEO_VO_INBAND_CMD_TYPE_OBJ_PIC;
			obj->header.size = sizeof(struct video_object);
			obj->version = 0x72746B3F;
			obj->width = fb->width;
			obj->height = fb->height;
			obj->Y_pitch = fb->width;
			obj->mode = CONSECUTIVE_FRAME;
			obj->Y_addr = rtk_gem[0]->paddr + fb->offsets[0];
			obj->U_addr = rtk_gem[1]->paddr + fb->offsets[1];
		} else {
			struct video_object *decObj = (struct video_object *)rtk_gem[0]->vaddr;
			struct video_object_ext *decObj_ext = (struct video_object_ext *)rtk_gem[0]->vaddr;
			struct kobject *kobj = &drm->primary->kdev->kobj;

			memcpy(obj, decObj, sizeof(struct video_object));

			obj->header.type = VIDEO_VO_INBAND_CMD_TYPE_OBJ_PIC;
			obj->header.size = sizeof(struct video_object);
			obj->version = 0x72746B3F;

			index = rtk_fence->idx;
			obj->context = index;
			obj->pLock = rtk_fence->lock_paddr + index;
			obj->pReceived = rtk_fence->rcv_paddr + index;
			obj->PTSH = decObj->PTSH;
			obj->PTSL = decObj->PTSL;
			obj->RPTSH = decObj->RPTSH;
			obj->RPTSL = decObj->RPTSL;
			if (rtk_plane->hdr_type != decObj_ext->hdr_type) {
				rtk_plane->hdr_type = decObj_ext->hdr_type;
				hdr_type_str[0] = kasprintf(GFP_KERNEL, "HDR_TYPE=%d", rtk_plane->hdr_type);
				hdr_type_str[1] = NULL;
				kobject_uevent_env(kobj, KOBJ_CHANGE, hdr_type_str);
				kfree(hdr_type_str[0]);
			}

			rtk_plane->secure_flag = decObj->secure_flag;
		}
		write_cmd_to_ringbuffer(rtk_plane, obj);
		kfree(obj);
	} else {
		struct graphic_object *obj = (struct graphic_object *)kzalloc(sizeof(struct graphic_object), GFP_KERNEL);
		unsigned int flags = 0;
		if(!obj) {
			DRM_ERROR("queue_ring_buffer malloc graphic_object fail\n");
			return -1;
		}

		memset(obj, 0, sizeof(struct graphic_object));
		obj->header.type = VIDEO_GRAPHIC_INBAND_CMD_TYPE_PICTURE_OBJECT;
		obj->header.size = sizeof(struct graphic_object);
		obj->colorkey = -1;
		if (fb->format->format == DRM_FORMAT_XRGB8888) {
			flags |= eBuffer_USE_GLOBAL_ALPHA;
			obj->alpha = 0x3ff;
		}
		obj->format = INBAND_CMD_GRAPHIC_FORMAT_ARGB8888_LITTLE;
		obj->width = fb->width;
		obj->height = fb->height;
		obj->pitch = fb->pitches[0];
		obj->address = rtk_gem[0]->paddr;
		obj->picLayout = INBAND_CMD_GRAPHIC_2D_MODE;
		obj->afbc = (flags & eBuffer_AFBC_Enable)?1:0;
		obj->afbc_block_split = (flags & eBuffer_AFBC_Split)?1:0;
		obj->afbc_yuv_transform = (flags & eBuffer_AFBC_YUV_Transform)?1:0;

		write_cmd_to_ringbuffer(rtk_plane, obj);
		kfree(obj);
	}

	rect.x = plane->state->crtc_x;
	rect.y = plane->state->crtc_y;
	rect.width = plane->state->crtc_w;
	rect.height = plane->state->crtc_h;

	p_rect = &rtk_plane->disp_win.videoWin;
	if (p_rect->x != rect.x || p_rect->y != rect.y ||
		p_rect->width != rect.width || p_rect->height != rect.height) {
		rtk_plane->disp_win.videoPlane = videoplane;
		rtk_plane->disp_win.videoWin = rect;
		rtk_plane->disp_win.borderWin = rect;
		rtk_plane->disp_win.borderColor = blueBorder;
		rtk_plane->disp_win.enBorder = 0;
		if (rpc_video_config_disp_win(rpc_info, &rtk_plane->disp_win)) {
			DRM_ERROR("rpc_video_config_disp_win RPC fail\n");
			return -1;
		}
	}

	return 0;
}

#ifdef DMABUF_HEAPS_RTK
static void rtk_plane_free(struct dma_buf_attachment *attach)
{
	struct dma_buf *dmabuf;

	pr_debug("%s free dmabuf\n", __func__);
	dmabuf = attach->dmabuf;
	BUG_ON(!dmabuf);
	BUG_ON(!dmabuf->ops);
	BUG_ON(!dmabuf->ops->end_cpu_access);

	if (dmabuf->vmap_ptr) {
		dma_buf_vunmap(dmabuf, dmabuf->vmap_ptr);
	}

	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	dma_buf_unmap_attachment(attach, attach->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}

static int rtk_plane_alloc(struct device *dev, size_t size,
			 char *name, unsigned int flags,
			 struct dma_buf_attachment **attach,
			 struct sg_table **table,
			 void **ion_phy, void **ion_virt)
{
	int ret = 0;
	struct dma_buf *dmabuf;

	dmabuf = rheap_alloc(name, size, flags);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(dev, "Failed to dma_buf_get\n");
		ret = PTR_ERR(dmabuf);
		goto rheap_err;
	}
	dma_buf_set_name(dmabuf, __func__);

	*attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(*attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		ret = PTR_ERR(*attach);
		goto attach_err;
	}

	*table = dma_buf_map_attachment(*attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(*table)) {
		dev_err(dev, "Failed to map attachment \n");
		ret = PTR_ERR(*table);
		goto map_err;
	}

	ret = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (ret)
		goto access_err;

	*ion_virt = dma_buf_vmap(dmabuf);
	if (!*ion_virt) {
		dev_err(dev, "dma_buf_vmap failed\n");
		ret = -ENOMEM;
		goto kmap_err;
	}

	/*or using sg_dma_address(table->sgl)) ?*/
	*ion_phy = (void *)sg_phys((*table)->sgl);

	dev_err(dev, "%s *ion_phy = 0x%lx , *ion_virt = 0x%lx \n", __func__,
		 (unsigned long)*ion_phy, (unsigned long)*ion_virt);

	return ret;
kmap_err:
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
access_err:
	dma_buf_unmap_attachment(*attach, *table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(dmabuf, *attach);
attach_err:
	dma_buf_put(dmabuf);
rheap_err:
	return ret;
}

#endif

static int rtk_plane_rpc_init(struct rtk_drm_plane *rtk_plane,
			      enum VO_VIDEO_PLANE layer_nr)
{
	struct drm_device *drm = rtk_plane->plane.dev;
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	int ret = 0;
	void *vaddr;
	void *refclock_vaddr;
	dma_addr_t paddr;
	dma_addr_t refclock_paddr;
	struct rpc_refclock refclock;
	struct rpc_ringbuffer ringbuffer;
	unsigned int id;
	int err = 0;

	enum VO_VIDEO_PLANE videoplane;

	struct vo_rectangle rect;
	struct vo_color blueBorder = {0, 0, 255, 1};

	videoplane = layer_nr;

#ifdef DMABUF_HEAPS_RTK

	ret = rtk_plane_alloc(drm->dev, 65*1024, "rtk_audio_heap", AUDIO_RTK_FLAG,
				 &attach, &table,
				 (void **)&paddr, (void **)&vaddr);
	if (ret) {
		pr_err("[%s %d alloc fail]\n", __func__, __LINE__);
		return -1;
	}
	rtk_plane->attach = attach;
	rtk_plane->dmabuf = attach->dmabuf;
	rtk_plane->ring_paddr = paddr + 64 * 1024;

	ret = rtk_plane_alloc(drm->dev, 1024, "rtk_media_heap", VIDEO_RTK_FLAG,
				 &attach, &table,
				 (void **)&refclock_paddr, (void **)&refclock_vaddr);
	if (ret) {
		pr_err("[%s %d alloc fail]\n", __func__, __LINE__);
		return -1;
	}
	rtk_plane->refclock_attach = attach;
	rtk_plane->refclock_dmabuf = attach->dmabuf;
#else
	vaddr = dma_alloc_attrs(drm->dev, 65*1024, &paddr,
				GFP_KERNEL | __GFP_NOWARN,
				DMA_ATTR_WRITE_COMBINE);

	refclock_vaddr = dma_alloc_attrs(drm->dev, 1024, &refclock_paddr,
				GFP_KERNEL | __GFP_NOWARN,
				DMA_ATTR_WRITE_COMBINE);
#endif
	rtk_plane->ringbase = (void *)((unsigned long)(vaddr));
	rtk_plane->ringheader = (struct tag_ringbuffer_header *)
			((unsigned long)(vaddr)+(64*1024));
	rtk_plane->refclock = (struct tag_refclock *)(refclock_vaddr);
	rtk_plane->keepFrmLock_paddr = (unsigned int)(0xffffffff&refclock_paddr) + sizeof(struct tag_refclock);

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

	rtk_plane->disp_win.videoPlane = videoplane;
	rtk_plane->disp_win.videoWin = rect;
	rtk_plane->disp_win.borderWin = rect;
	rtk_plane->disp_win.borderColor = blueBorder;
	rtk_plane->disp_win.enBorder = 0;

	if (rpc_video_config_disp_win(rpc_info, &rtk_plane->disp_win)) {
		DRM_ERROR("rpc_video_config_disp_win RPC fail\n");
		return -1;
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
	refclock.pRefClock = (long)(0xffffffff&refclock_paddr);
	if (rpc_video_set_refclock(rpc_info, &refclock)) {
		DRM_ERROR("rpc_video_set_refclock RPC fail\n");
		return -1;
	}

	rtk_plane->ringheader->beginAddr = htonl((long)(0xffffffff&paddr));
	rtk_plane->ringheader->size = htonl(64*1024);
	rtk_plane->ringheader->writePtr = rtk_plane->ringheader->beginAddr;
	rtk_plane->ringheader->readPtr[0] = rtk_plane->ringheader->beginAddr;
	rtk_plane->ringheader->bufferID = htonl(1);
	memset(&ringbuffer, 0, sizeof(ringbuffer));
	ringbuffer.instance = id;
	ringbuffer.readPtrIndex = 0;
	ringbuffer.pinID = 0;
	ringbuffer.pRINGBUFF_HEADER = (long)(0xffffffff&paddr)+64*1024;

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
		DRM_ERROR("Not create %d plane for device attribute\n", videoplane);

	if (err < 0)
		DRM_ERROR("failed to create %d plane devide attribute\n", videoplane);

	return 0;
}

void rtk_plane_destroy(struct drm_plane *plane)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct drm_device *drm = plane->dev;
	enum VO_VIDEO_PLANE videoplane = rtk_plane->info.videoPlane;
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;

	if (rtk_plane->rtk_fence)
		rtk_drm_fence_uninit(rtk_plane);

	if (videoplane == VO_VIDEO_PLANE_V1)
		device_remove_file(drm->dev, &dev_attr_enable_video_display);
	else if (videoplane == VO_VIDEO_PLANE_OSD1)
		device_remove_file(drm->dev, &dev_attr_enable_osd_display);
	else if (videoplane == VO_VIDEO_PLANE_SUB1)
		device_remove_file(drm->dev, &dev_attr_enable_sub_display);

	rpc_destroy_video_agent(rpc_info, rtk_plane->info.instance);

#ifdef DMABUF_HEAPS_RTK
	rtk_plane_free(rtk_plane->attach);
	rtk_plane_free(rtk_plane->refclock_attach);
#endif

	drm_plane_cleanup(plane);

	rtk_rpc_uninit(rpc_info);
}

struct drm_plane_state *
rtk_plane_atomic_helper_plane_duplicate_state(struct drm_plane *plane)
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

void rtk_plane_atomic_helper_plane_destroy_state(struct drm_plane *plane,
					   struct drm_plane_state *state)
{
	__drm_atomic_helper_plane_destroy_state(state);
	kfree(to_rtk_plane_state(state));
}


static int rtk_plane_atomic_set_property(struct drm_plane *plane,
				   struct drm_plane_state *state,
				   struct drm_property *property,
				   uint64_t val)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_drm_plane_state *s =	to_rtk_plane_state(state);

	if (property == rtk_plane->out_fence_ptr) {
		s32 __user *fence_ptr = u64_to_user_ptr(val);

		if (!fence_ptr)
			return 0;

		if (put_user(-1, fence_ptr))
			return -EFAULT;

		s->out_fence_ptr = fence_ptr;
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

	if (property == rtk_plane->out_fence_ptr) {
		*val = 0;
		return 0;
	}

	DRM_ERROR("failed to get rtk plane atomic property\n");
	return -EINVAL;
}

static const struct drm_plane_funcs rtk_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.destroy = rtk_plane_destroy,
	.disable_plane = drm_atomic_helper_disable_plane,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = rtk_plane_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = rtk_plane_atomic_helper_plane_destroy_state,
	.atomic_set_property = rtk_plane_atomic_set_property,
	.atomic_get_property = rtk_plane_atomic_get_property,
};

static int rtk_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	DRM_DEBUG_KMS("%d\n", __LINE__);

	return 0;
}

static void rtk_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct drm_crtc *crtc = plane->state->crtc;
	struct drm_framebuffer *fb = plane->state->fb;
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_drm_plane_state *s =	to_rtk_plane_state(plane->state);

	DRM_DEBUG_KMS("%s, width=%d, height=%d\n", __func__,
			fb->width, fb->height);

	if (!crtc || WARN_ON(!fb))
		return;

	queue_ring_buffer(plane);

	if (rtk_plane->rtk_fence && s->out_fence_ptr) {
		if (!rtk_drm_fence_create(rtk_plane->rtk_fence, s->out_fence_ptr)) {
			s->out_fence_ptr = NULL;
		}
	}

	rtk_crtc_finish_page_flip(crtc);
}

static void rtk_plane_atomic_disable(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct rtk_drm_plane *rtk_plane = to_rtk_plane(plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct vo_rectangle rect;

	DRM_DEBUG_KMS("%d\n", __LINE__);

	rect.x = 0;
	rect.y = 0;
	rect.width = 0;
	rect.height = 0;

	rtk_plane->disp_win.videoWin = rect;
	rtk_plane->disp_win.borderWin = rect;

	if (rpc_video_config_disp_win(rpc_info, &rtk_plane->disp_win))
		DRM_ERROR("rpc_video_config_disp_win RPC fail\n");
}

static const struct drm_plane_helper_funcs rtk_plane_helper_funcs = {
	.atomic_check = rtk_plane_atomic_check,
	.atomic_update = rtk_plane_atomic_update,
	.atomic_disable = rtk_plane_atomic_disable,
};

int rtk_plane_init(struct drm_device *drm, struct rtk_drm_plane *rtk_plane,
		   unsigned long possible_crtcs, enum drm_plane_type type,
		   enum VO_VIDEO_PLANE layer_nr)
{
	struct drm_plane *plane = &rtk_plane->plane;
	struct rtk_drm_private *priv = drm->dev_private;
	int err;

	err = drm_universal_plane_init(drm, plane, possible_crtcs,
				       &rtk_plane_funcs, formats,
				       ARRAY_SIZE(formats), NULL, type, NULL);
	if (err) {
		DRM_ERROR("failed to initialize plane\n");
		goto err_cleanup_planes;
	}

	drm_plane_helper_add(plane, &rtk_plane_helper_funcs);

	if(type == DRM_PLANE_TYPE_OVERLAY) {
		drm_plane_create_zpos_immutable_property(plane, 0);

		rtk_plane->out_fence_ptr = drm_property_create_range(plane->dev, DRM_MODE_PROP_ATOMIC,
					"OUT_FENCE_PTR", 0, U64_MAX);
		if (!rtk_plane->out_fence_ptr) {
			DRM_ERROR("create plane out fence ptr property fail\n");
			return -ENOMEM;
		}

		drm_object_attach_property(&plane->base, rtk_plane->out_fence_ptr, 0);
	}
	else if (type == DRM_PLANE_TYPE_PRIMARY) {
		drm_plane_create_zpos_immutable_property(plane, 1);
	}
	else {
		drm_plane_create_zpos_immutable_property(plane, 2);
	}

	rtk_plane->rpc_info = &priv->rpc_info;
	rtk_plane->gAlpha = 0;
	rtk_plane->flags &= ~BG_SWAP;
	rtk_plane->flags |= VSYNC_FORCE_LOCK;
	rtk_plane->hdr_type = -1;

	rtk_plane_rpc_init(rtk_plane, layer_nr);
	if (type == DRM_PLANE_TYPE_OVERLAY)
		rtk_drm_fence_init(rtk_plane);

	return 0;

err_cleanup_planes:
	drm_plane_cleanup(plane);

	return -1;
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
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_set_q_param(rpc_info, (struct rpc_set_q_param *)data);
}

int rtk_plane_config_channel_lowdelay(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_config_channel_lowdelay *param = (struct rpc_config_channel_lowdelay *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane =  container_of(plane, struct rtk_drm_plane, plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	struct rpc_config_channel_lowdelay arg;

	if(!rtk_plane || !rpc_info)
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
	struct rtk_drm_plane *rtk_plane =  container_of(plane, struct rtk_drm_plane, plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;

	if(!rtk_plane || !rpc_info)
		return -1;

	param->instanceId = rtk_plane->info.instance;

	return rpc_video_privateinfo_param(rpc_info, param);
}

int rtk_plane_set_speed(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_set_speed *param = (struct rpc_set_speed *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane =  container_of(plane, struct rtk_drm_plane, plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;

	if(!rtk_plane || !rpc_info)
		return -1;

	param->instanceId = rtk_plane->info.instance;

	return rpc_video_set_speed(rpc_info, param);
}

int rtk_plane_set_background(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_set_background(rpc_info, (struct rpc_set_background *)data);
}

int rtk_plane_keep_curpic_fw(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_keep_curpic *param = (struct rpc_keep_curpic *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane =  container_of(plane, struct rtk_drm_plane, plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;
	int ret = 0;

	if(!rtk_plane || !rpc_info){
		return -1;
	}
	param->plane = rtk_plane->disp_win.videoPlane;

#if !defined(ENABLE_TEE_DRM_FLOW)
	return rpc_video_keep_curpic_fw(rpc_info, param);
#else
	{
		struct rpc_keep_curpic_svp lastpic, getpic;
#if 1//todo, should move to better position.
		if(rpc_info->teeapi_tee_session == 0)
		ret = ta_TEEapi_init((struct tee_context **)&rpc_info->teeapi_ctx, &rpc_info->teeapi_tee_session);
		if (ret < 0)
		{
			DRM_ERROR("[-] [%d]%s.ta_TEEapi_init() fail.ret:%d\n",__LINE__,__func__,ret);
			return ret;
		}
#endif

		lastpic.plane = rtk_plane->disp_win.videoPlane;
		lastpic.type = ENUM_VIDEO_KEEP_CUR_SVP_TYPE_GET_FW_MALLOC_SVP_BUFFER;
		ret = rpc_video_keep_curpic_svp(rpc_info, &lastpic);
		if (ret < 0)
		{
			DRM_ERROR("[%d]%s.rpc_video_keep_curpic_svp() fail.ret:%d\n",__LINE__,__func__, ret);
			return ret;
		}

		getpic.plane = rtk_plane->disp_win.videoPlane;
		getpic.type = ENUM_VIDEO_KEEP_CUR_SVP_TYPE_GET_CUR;
		ret = rpc_video_keep_curpic_svp(rpc_info, &getpic);
		if (ret < 0)
		{
			DRM_ERROR("[%d]%s.rpc_video_keep_curpic_svp() fail.ret:%d\n",__LINE__,__func__, ret);
			return ret;
		}

		//copy from vo_wrap_set_VO_SVPKeepLastFrame, the following value is not setting by fw
		lastpic.Caddr = (unsigned int)lastpic.Yaddr + getpic.Ysize;

		if(getpic.Ysize == 0 || getpic.Csize == 0 ||
			lastpic.Yaddr == 0 || lastpic.Caddr == 0 ||
			getpic.Yaddr == 0 || getpic.Caddr == 0)
		{
			DRM_DEBUG_KMS("[%d]%s (%d, %d, 0x%x, 0x%x, 0x%x, 0x%x)\n",__LINE__,__func__, getpic.Ysize, getpic.Csize, getpic.Yaddr, getpic.Caddr, lastpic.Yaddr, lastpic.Caddr);
			return ret;
		}

		lastpic.offsetTable_yaddr = lastpic.Caddr + getpic.Csize;
		lastpic.offsetTable_caddr = lastpic.offsetTable_yaddr + getpic.offsetTable_ysize;
		lastpic.lock = rtk_plane->keepFrmLock_paddr;

		ret = ta_TEEapi_memcpy((struct tee_context *)rpc_info->teeapi_ctx, rpc_info->teeapi_tee_session, lastpic.Yaddr, getpic.Yaddr, getpic.Ysize);
		if (ret < 0)
		{
			DRM_ERROR("[%d]%s.ta_TEEapi_memcpy() fail.ret:%d\n",__LINE__,__func__, ret);
			return ret;
		}

		ret = ta_TEEapi_memcpy((struct tee_context *)rpc_info->teeapi_ctx, rpc_info->teeapi_tee_session, lastpic.Caddr, getpic.Caddr, getpic.Csize);
		if (ret < 0)
		{
			DRM_ERROR("[%d]%s.ta_TEEapi_memcpy() fail.ret:%d\n",__LINE__,__func__, ret);
			return ret;
		}

		if(getpic.offsetTable_yaddr != 0 && getpic.offsetTable_caddr != 0)
		{
			ret = ta_TEEapi_memcpy((struct tee_context *)rpc_info->teeapi_ctx, rpc_info->teeapi_tee_session, lastpic.offsetTable_yaddr, getpic.offsetTable_yaddr, getpic.offsetTable_ysize);
			if (ret < 0)
			{
				DRM_ERROR("[%d]%s.ta_TEEapi_memcpy() fail.ret:%d\n",__LINE__,__func__, ret);
				return ret;
			}

			ret = ta_TEEapi_memcpy((struct tee_context *)rpc_info->teeapi_ctx, rpc_info->teeapi_tee_session, lastpic.offsetTable_caddr, getpic.offsetTable_caddr, getpic.offsetTable_csize);
			if (ret < 0)
			{
				DRM_ERROR("[%d]%s.ta_TEEapi_memcpy() fail.ret:%d\n",__LINE__,__func__, ret);
				return ret;
			}
		}
		else
		{
			lastpic.offsetTable_yaddr = 0;
	        lastpic.offsetTable_caddr = 0;
		}

		lastpic.type = ENUM_VIDEO_KEEP_CUR_SVP_TYPE_SET_CUR;
		rpc_video_keep_curpic_svp(rpc_info, &lastpic);
	}
#endif

	return 0;
}

int rtk_plane_set_deintflag(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_set_deintflag(rpc_info, (struct rpc_set_deintflag *)data);
}

int rtk_plane_create_graphic_win(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_create_graphic_win(rpc_info, (struct rpc_create_graphic_win *)data);
}

int rtk_plane_draw_graphic_win(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_draw_graphic_win(rpc_info, (struct rpc_draw_graphic_win *)data);
}

int rtk_plane_modify_graphic_win(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_modify_graphic_win(rpc_info, (struct rpc_modify_graphic_win *)data);
}

int rtk_plane_delete_graphic_win(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_delete_graphic_win(rpc_info, (struct rpc_delete_graphic_win *)data);
}

int rtk_plane_conf_osd_palette(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_config_osd_palette(rpc_info, (struct rpc_config_osd_palette *)data);
}

int rtk_plane_conf_plane_mixer(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rpc_config_plane_mixer *param = (struct rpc_config_plane_mixer *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, param->plane_id);
	struct rtk_drm_plane *rtk_plane =  container_of(plane, struct rtk_drm_plane, plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;

	if(!rtk_plane || !rpc_info)
		return -1;

	param->instanceId = rtk_plane->info.instance;

	return rpc_video_config_plane_mixer(rpc_info, param);
}

int rtk_plane_set_tv_system(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_set_tv_system(rpc_info, (struct rpc_config_tv_system *)data);
}

int rtk_plane_get_tv_system(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_query_tv_system(rpc_info, (struct rpc_config_tv_system *)data);
}

int rtk_plane_set_dispout_format(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_set_display_format(rpc_info, (struct rpc_display_output_format *)data);
}

int rtk_plane_get_dispout_format(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_get_display_format(rpc_info, (struct rpc_display_output_format *)data);
}

int rtk_plane_set_hdmi_audio_mute(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_set_hdmi_audio_mute(rpc_info, (struct rpc_audio_mute_info *)data);
}

int rtk_plane_set_sdrflag(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	return rpc_video_set_sdrflag(rpc_info, (struct rpc_set_sdrflag *)data);
}

int rtk_plane_set_pause_ioctl(struct drm_device *dev,
			    void *data, struct drm_file *file)
{
	struct drm_rtk_pause *pause = (struct drm_rtk_pause *)data;
	struct drm_plane *plane = drm_plane_find(dev, file, pause->plane_id);
	struct rtk_drm_plane *rtk_plane =  container_of(plane, struct rtk_drm_plane, plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;

	if(!rtk_plane || !rpc_info)
		return -1;

	if(pause->enable) {
		if (rpc_video_pause(rpc_info, rtk_plane->info.instance)) {
			DRM_ERROR("rpc_video_pause RPC fail\n");
			return -1;
		}
	} else {
		if (rpc_video_display(rpc_info, &rtk_plane->info)) { // for keep last frame
			DRM_ERROR("rpc_video_display RPC fail\n");
			return -1;
		}
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
	struct rtk_drm_plane *rtk_plane =  container_of(plane, struct rtk_drm_plane, plane);
	struct rtk_rpc_info *rpc_info = rtk_plane->rpc_info;

	if(!rtk_plane || !rpc_info)
		return -1;

	if (rpc_video_flush(rpc_info, rtk_plane->info.instance)) {
		DRM_ERROR("rpc_video_flush RPC fail\n");
		return -1;
	}

	return 0;
}

int rtk_plane_set_video_priority(struct drm_device *dev,
			    void *data, struct drm_file *file)
{
	uint8_t *mode = (uint8_t *)data;
	struct rtk_drm_private *priv = dev->dev_private;
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;

	if(!rpc_info)
		return -1;

	if (rpc_video_set_priority_mode(rpc_info, *mode)) {
		DRM_ERROR("rpc_video_dv_video_priority RPC fail\n");
		return -1;
	}

	return 0;
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
		structConfigGraphicCanvas.srcWin.width = 1280;
		structConfigGraphicCanvas.srcWin.height = 720;
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
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;
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
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;
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
	struct rtk_rpc_info *rpc_info = &priv->rpc_info;
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

