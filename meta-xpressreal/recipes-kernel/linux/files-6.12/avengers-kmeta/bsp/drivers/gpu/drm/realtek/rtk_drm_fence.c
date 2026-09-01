// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Realtek Inc.
 * Author: Simon Hsu <simon_hsu@realtek.com>
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

#include <linux/kthread.h>
#include <linux/dma-buf.h>

#include <drm/drm_file.h>
#include <drm/drm_crtc.h>
#include <drm/drm_print.h>
#include <drm/drm_plane.h>
#include <drm/drm_flip_work.h>

#include "rtk_drm_drv.h"
#include "rtk_drm_crtc.h"

struct rtk_flip_task {
	struct list_head node;
	void *data;
};

static const char *rtk_drm_fence_get_driver_name(struct dma_fence *fence)
{
	return "rtk_plane_outfence";
}

static const char *rtk_drm_fence_get_timeline_name(struct dma_fence *fence)
{
	return "rtk_plane_timeline";
}

static const struct dma_fence_ops rtk_drm_fence_ops = {
	.get_driver_name = rtk_drm_fence_get_driver_name,
	.get_timeline_name = rtk_drm_fence_get_timeline_name,
};

static void fence_signal_worker(struct drm_flip_work *work, void *val)
{
	struct rtk_dma_fence *fence = val;

	DRM_DEBUG_DRIVER("signal out-fence[%d] : (%p) (%p)\n", fence->idx, fence, fence->fence);

	dma_fence_signal(fence->fence);
	dma_fence_put(fence->fence);

	kfree(fence);
}

int rtk_drm_fence_update(struct rtk_drm_plane *rtk_plane)
{
	struct rtk_drm_fence *rtk_fence = rtk_plane->rtk_fence;
	struct drm_flip_work *work = &rtk_fence->fence_signal_work;
	struct rtk_flip_task *task, *tmp;
	struct rtk_dma_fence *fence;
	struct list_head tasks;
	struct list_head release_tasks;
	unsigned long flags;

	INIT_LIST_HEAD(&tasks);
	INIT_LIST_HEAD(&release_tasks);

	if (list_empty(&work->queued) && list_empty(&rtk_fence->pending)) {
		DRM_DEBUG_DRIVER("videoplane %d 's fence (%p) no flip work queued !\n",
			rtk_plane->layer_nr, rtk_plane->rtk_fence);
		return 0;
	}

	if (!list_empty(&rtk_fence->pending))
		list_splice_tail(&rtk_fence->pending, &tasks);
	INIT_LIST_HEAD(&rtk_fence->pending);

	spin_lock_irqsave(&work->lock, flags);
	list_splice_tail(&work->queued, &tasks);
	INIT_LIST_HEAD(&work->queued);
	spin_unlock_irqrestore(&work->lock, flags);

	DRM_DEBUG_DRIVER("release target : %d\n", rtk_plane->context);

	if (rtk_plane_check_update_done(rtk_plane)) {
		list_for_each_entry_safe(task, tmp, &tasks, node) {
			fence = task->data;

			DRM_DEBUG_DRIVER("fence->idx : %d\n", fence->idx);

			spin_lock_irqsave(&rtk_fence->idx_lock, flags);

			if (((fence->idx + 1) % CONTEXT_SIZE) == rtk_plane->context) {
				/**
				 * release fence (previous)
				 */
				DRM_DEBUG_DRIVER("release fence : %d\n", fence->idx);
				list_move_tail(&task->node, &release_tasks);
			} else {
				/**
				 * pending fence (current/not queued)
				 */
				DRM_DEBUG_DRIVER("success pending fence : %d\n", fence->idx);
				list_move_tail(&task->node, &rtk_fence->pending);
			}

			spin_unlock_irqrestore(&rtk_fence->idx_lock, flags);
		}
	} else {
		/**
		 * pending fence (current)
		 */
		list_for_each_entry_safe(task, tmp, &tasks, node) {
			fence = task->data;

			DRM_DEBUG_DRIVER("fail pending fence : %d\n", fence->idx);

			spin_lock_irqsave(&rtk_fence->idx_lock, flags);
			list_move_tail(&task->node, &rtk_fence->pending);
			spin_unlock_irqrestore(&rtk_fence->idx_lock, flags);
		}
	}

	/* TODO: overlay plane */

	spin_lock_irqsave(&work->lock, flags);
	list_splice_tail(&release_tasks, &work->commited);
	spin_unlock_irqrestore(&work->lock, flags);

	queue_work(system_unbound_wq, &work->worker);

	return 0;
}

int rtk_drm_fence_create(struct rtk_drm_fence *rtk_fence, s32 __user *out_fence_ptr)
{
	struct rtk_dma_fence *fence;
	int ret = 0;
	int retval = 0;

	DRM_DEBUG_DRIVER("%d\n", __LINE__);

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence) {
		DRM_ERROR("rtk_dma_fence alloc fail\n");
		return -ENOMEM;
	}

	fence->fence = kzalloc(sizeof(struct dma_fence), GFP_KERNEL);
	if (!fence->fence) {
		DRM_ERROR("dma_fence alloc fail\n");
		ret = -ENOMEM;
		goto create_error;
	}

	dma_fence_init(fence->fence, &rtk_drm_fence_ops, &rtk_fence->fence_lock, 0, 0);

	fence->fd = get_unused_fd_flags(O_CLOEXEC);
	if (fence->fd < 0) {
		DRM_ERROR("rtk_dma_fence get unused fd fail\n");
		ret = fence->fd;
		goto create_error;
	}

	retval = put_user(fence->fd, out_fence_ptr);
	if (retval) {
		DRM_ERROR("fence fd put to user fail %d\n", retval);
		ret = -EFAULT;
		goto create_error;
	}

	fence->sync_file = sync_file_create(fence->fence);
	if (!fence->sync_file) {
		DRM_ERROR("sync_file create fail\n");
		ret = -ENOMEM;
		goto create_error;
	}

	fence->idx = rtk_fence->idx;

	fd_install(fence->fd, fence->sync_file->file);
	drm_flip_work_queue(&rtk_fence->fence_signal_work, fence);

	DRM_DEBUG_DRIVER("create and install out-fence[%d] : (%p) (%p)\n", fence->idx, fence, fence->fence);

	return 0;

create_error:
	kfree(fence);
	return ret;
}

int rtk_drm_fence_uninit(struct rtk_drm_plane *rtk_plane)
{
	struct rtk_drm_fence *rtk_fence = rtk_plane->rtk_fence;

	DRM_INFO("Uninit videoplane %d 's fence (%p)\n",
		rtk_plane->layer_nr, rtk_plane->rtk_fence);

	kfree(rtk_fence);

	return  0;
}

int rtk_drm_fence_init(struct rtk_drm_plane *rtk_plane)
{
	struct rtk_drm_fence *rtk_fence;

	rtk_fence = kzalloc(sizeof(*rtk_fence), GFP_KERNEL);

	rtk_plane->rtk_fence = rtk_fence;

	DRM_INFO("Init videoplane %d 's fence (%p)\n",
		rtk_plane->layer_nr, rtk_plane->rtk_fence);

	spin_lock_init(&rtk_fence->fence_lock);
	spin_lock_init(&rtk_fence->idx_lock);

	drm_flip_work_init(&rtk_fence->fence_signal_work, "fence_signal",
			   fence_signal_worker);
	INIT_LIST_HEAD(&rtk_fence->pending);

	return 0;
}
