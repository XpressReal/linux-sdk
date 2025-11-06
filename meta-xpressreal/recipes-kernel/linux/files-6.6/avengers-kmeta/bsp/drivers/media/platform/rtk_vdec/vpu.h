/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2021 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#ifndef __VPU_H__
#define __VPU_H__
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>
#include "video_engine.h"

struct vpu_ctx {
	struct v4l2_format out, cap;
	struct v4l2_rect rect;
	struct task_struct *thread_out, *thread_cap;
	int	thread_out_interval, thread_cap_interval;
	uint32_t seq_out, seq_cap;
	struct mutex vpu_mutex;
	spinlock_t vpu_spin_lock;
	wait_queue_head_t vpu_out_waitq;
	wait_queue_head_t vpu_cap_waitq;

	/* video engine operations */
	struct veng_ops *veng_ops;
	struct veng_ops *ve1_ops;
	struct veng_ops *ve2_ops;

	int is_cap_started, is_out_started;
	uint32_t bufcnt_out, bufcnt_cap;
	uint32_t memory_out, memory_cap;

	bool stop_cmd;
	int cap_retry_cnt;
};

struct vpu_fmt_ops {
    int (*vpu_enum_fmt_cap)(struct v4l2_fmtdesc *f);
    int (*vpu_enum_fmt_out)(struct v4l2_fmtdesc *f);
    int (*vpu_g_fmt)(struct v4l2_fh *fh, struct v4l2_format *f);
    int (*vpu_try_fmt_cap)(struct v4l2_format *f);
    int (*vpu_try_fmt_out)(struct v4l2_format *f);
    int (*vpu_s_fmt_cap)(struct v4l2_fh *fh, struct v4l2_format *f);
    int (*vpu_s_fmt_out)(struct v4l2_fh *fh, struct v4l2_format *f);
    int (*vpu_queue_info)(struct vb2_queue *vq, int *bufcnt, int *sizeimage);
    int (*vpu_start_streaming)(struct vb2_queue *q, unsigned count);
    int (*vpu_stop_streaming)(struct vb2_queue *q);
    int (*vpu_qbuf)(struct v4l2_fh *fh, struct vb2_buffer *vb);
    void (*vpu_buf_finish)(struct vb2_buffer *vb);
    int (*vpu_abort)(void *priv, int type);
    int (*vpu_g_crop)(void *fh, struct v4l2_rect *rect);
    int (*vpu_stop_cmd)(void *fh);
};

const struct vpu_fmt_ops *get_vpu_fmt_ops(void);

/*
 * struct veng_ops - video engine operations
 */
struct veng_ops {
    int (*ve_start_streaming)(struct vb2_queue *q, uint32_t count, int pixelformat);
    int (*ve_stop_streaming)(struct vb2_queue *q);
    int (*ve_out_qbuf)(void *fh, uint8_t *buf, uint32_t len, uint64_t pts, uint32_t sequence);
    int (*ve_cap_qbuf)(void *fh, struct vb2_buffer *vb);
    int (*ve_cap_dqbuf)(void *fh, uint8_t *buf, uint32_t *len, uint64_t *pts, uint32_t work_idx);
#if 0 //Keep for libmali verify, remove me while libmali finish metadata conversion
    void (*ve_buf_finish)(struct vb2_buffer *vb);
#endif
    int (*ve_abort)(void *ctx, int type);
    void *(*ve_alloc_context)(struct file *file);
    void (*ve_free_context)(void *ctx);
    void (*ve_stop_cmd)(void *fh, int pixelformat);
    void (*ve_get_info)(void *fh, bool *eos, bool *no_frame);
};

/**
* @brief Get the original v4l2_pix_format of cap in vpu_ctx
* @param fh [input] struct v4l2_fh
* @param cap_fmt [output] struct v4l2_pix_format, copied from the original v4l2_pix_format of cap in vpu_ctx
*/
int vpu_get_cap_fmt(void *fh, void *cap_fmt);
/**
* @brief Update the v4l2_pix_format of cap in vpu_ctx
* @param fh [input] struct v4l2_fh
* @param cap_fmt [input] struct v4l2_pix_format, it will be copied to the v4l2_pix_format of cap in vpu_ctx
*/
int vpu_update_cap_fmt(void *fh, void *cap_fmt);
/**
 * @brief Notify source resolution change event
 * @param fh [input] struct v4l2_fh
 */
void vpu_notify_event_resolution_change(void *fh);
void *vpu_alloc_context(void);
void vpu_free_context(void *ctx);
int vpu_ve_register(int index, struct veng_ops *ops);
void vpu_ve_unregister(int index);

#endif
