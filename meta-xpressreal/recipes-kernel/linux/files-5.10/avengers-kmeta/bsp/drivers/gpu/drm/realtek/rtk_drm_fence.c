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

#include "rtk_drm_crtc.h"

#define PLOCK_ADDR_SHIFT	512

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

	dma_fence_signal(fence->fence);
	dma_fence_put(fence->fence);

	kfree(fence);
}

int rtk_drm_fence_update(struct rtk_drm_plane *rtk_plane)
{
	struct rtk_drm_fence *rtk_fence = rtk_plane->rtk_fence;
	struct drm_flip_work *work = &rtk_fence->fence_signal_work;
	struct drm_flip_task *task, *tmp;
	struct rtk_dma_fence *fence;
	struct list_head tasks;
	unsigned long flags;
	volatile unsigned char pRcv;
	volatile unsigned char pLock;

	INIT_LIST_HEAD(&tasks);

	if (list_empty(&work->queued) && list_empty(&rtk_fence->pending))
		return 0;

	if (!list_empty(&rtk_fence->pending))
		list_splice_tail(&rtk_fence->pending, &tasks);
	INIT_LIST_HEAD(&rtk_fence->pending);

	spin_lock_irqsave(&work->lock, flags);
	list_splice_tail(&work->queued, &tasks);
	INIT_LIST_HEAD(&work->queued);
	spin_unlock_irqrestore(&work->lock, flags);

	list_for_each_entry_safe(task, tmp, &tasks, node) {
		fence = task->data;
		pRcv = *(volatile unsigned char*)(rtk_fence->rcv_vaddr + fence->idx);
		pLock = *(volatile unsigned char*)(rtk_fence->lock_vaddr + fence->idx);
		if (!(pLock == 0 && pRcv == PLOCK_RECEIVED)) {
			list_move_tail(&task->node, &rtk_fence->pending);
		} else {
			*(volatile unsigned char*)(rtk_fence->rcv_vaddr + fence->idx) = PLOCK_INIT;
			dsb(sy);
		}
	}

	spin_lock_irqsave(&work->lock, flags);
	list_splice_tail(&tasks, &work->commited);
	spin_unlock_irqrestore(&work->lock, flags);

	queue_work(system_unbound_wq, &work->worker);

	return 0;
}

int rtk_drm_fence_create(struct rtk_drm_fence *rtk_fence, s32 __user *out_fence_ptr)
{
	struct rtk_dma_fence *fence;
	int ret = 0;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return -ENOMEM;

	fence->fence = kzalloc(sizeof(struct dma_fence), GFP_KERNEL);
	if (!fence->fence) {
		ret = -ENOMEM;
		goto create_error;
	}

	dma_fence_init(fence->fence, &rtk_drm_fence_ops, &rtk_fence->fence_lock, 0, 0);

	fence->fd = get_unused_fd_flags(O_CLOEXEC);
	if (fence->fd < 0) {
		ret = fence->fd;
		goto create_error;
	}

	if (put_user(fence->fd, out_fence_ptr)) {
		ret = -EFAULT;
		goto create_error;
	}

	fence->sync_file = sync_file_create(fence->fence);
	if (!fence->sync_file) {
		ret = -ENOMEM;
		goto create_error;
	}
	fence->idx = rtk_fence->idx++;
	fd_install(fence->fd, fence->sync_file->file);

	if (rtk_fence->idx >= PLOCK_BUFFER_SET_SIZE)
		rtk_fence->idx -= PLOCK_BUFFER_SET_SIZE;

	drm_flip_work_queue(&rtk_fence->fence_signal_work, fence);

	return 0;

create_error:
	kfree(fence);
	return ret;
}

int rtk_drm_fence_uninit(struct rtk_drm_plane *rtk_plane)
{
	struct rtk_drm_fence *rtk_fence = rtk_plane->rtk_fence;
	struct drm_flip_work *work = &rtk_fence->fence_signal_work;
	struct list_head tasks;
	unsigned long flags;

	INIT_LIST_HEAD(&tasks);

	if (!list_empty(&rtk_fence->pending))
		list_splice_tail(&rtk_fence->pending, &tasks);

	spin_lock_irqsave(&work->lock, flags);
	if (!list_empty(&work->queued)) {
		list_splice_tail(&work->queued, &tasks);
		INIT_LIST_HEAD(&work->queued);
	}
	list_splice_tail(&tasks, &work->commited);
	spin_unlock_irqrestore(&work->lock, flags);

	drm_flip_work_commit(work, system_unbound_wq);

	flush_work(&work->worker);

	memset(rtk_fence->lock_vaddr, 0, PLOCK_MAX_BUFFER_INDEX);
	memset(rtk_fence->rcv_vaddr, PLOCK_INIT, PLOCK_MAX_BUFFER_INDEX);

	kfree(rtk_fence);

	return 0;
}

int rtk_drm_fence_init(struct rtk_drm_plane *rtk_plane)
{
	struct rtk_drm_fence *rtk_fence;

	rtk_fence = kzalloc(sizeof(*rtk_fence), GFP_KERNEL);
	if (!rtk_fence)
		return -ENOMEM;

	rtk_fence->lock_vaddr = (u8 *)rtk_plane->ringheader + PLOCK_ADDR_SHIFT;
	rtk_fence->rcv_vaddr = rtk_fence->lock_vaddr + PLOCK_MAX_BUFFER_INDEX;
	rtk_fence->lock_paddr = rtk_plane->ring_paddr + PLOCK_ADDR_SHIFT;
	rtk_fence->rcv_paddr = rtk_fence->lock_paddr + PLOCK_MAX_BUFFER_INDEX;

	memset(rtk_fence->lock_vaddr, 0, PLOCK_MAX_BUFFER_INDEX);
	memset(rtk_fence->rcv_vaddr, PLOCK_INIT, PLOCK_MAX_BUFFER_INDEX);

	spin_lock_init(&rtk_fence->fence_lock);

	drm_flip_work_init(&rtk_fence->fence_signal_work, "fence_signal",
			   fence_signal_worker);
	INIT_LIST_HEAD(&rtk_fence->pending);

	rtk_plane->rtk_fence = rtk_fence;

	return 0;
}

