/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __RTK_DRM_RINGBUFFER_H__
#define __RTK_DRM_RINGBUFFER_H__

#include "rtk_drm_drv.h"

struct rtk_drm_ringbuffer {
	struct tag_ringbuffer_header *shm_ringheader;
	dma_addr_t addr;
	void *virt;
	u32 size;
	u32 header_offset;
	struct rpmsg_device *rpdev;
	struct drm_device *drm;
};

#define RTK_DRM_RINGBUFFER_HEADER_SIZE     (1024)

int rtk_drm_ringbuffer_alloc(struct drm_device *drm, struct rpmsg_device *rpdev,
			     struct rtk_drm_ringbuffer *rb, u32 buffer_size);
void rtk_drm_ringbuffer_free(struct rtk_drm_ringbuffer *rb);
int rtk_drm_ringbuffer_write(struct rtk_drm_ringbuffer *rb, void *cmd, u32 size);
int rtk_drm_ringbuffer_read(struct rtk_drm_ringbuffer *rb, void *data, u32 data_size,
			    bool update_read_ptr);
void rtk_drm_ringbuffer_reset(struct rtk_drm_ringbuffer *rb);

#endif
