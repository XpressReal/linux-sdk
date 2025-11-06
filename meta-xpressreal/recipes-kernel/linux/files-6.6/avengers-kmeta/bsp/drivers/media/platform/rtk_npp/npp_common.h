/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek npu_pp video m2m and capture v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#ifndef __NPP_COMMON_H__
#define __NPP_COMMON_H__

#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>

#define OUT_BUF_COUNT   (4)
#define CAP_BUF_COUNT   (4)
#define CAP_BUF_COUNT_MAX  (32)

#define NPP_IRQ 103
#define MAX_IMAGE_M2M_WIDTH 3840
#define MAX_IMAGE_M2M_HEIGHT 2176
#define MIN_IMAGE_M2M_WIDTH 16
#define MIN_IMAGE_M2M_HEIGHT 2

#define MAX_IMAGE_CAPTURE_WIDTH 3840
#define MAX_IMAGE_CAPTURE_HEIGHT 2176
#define MIN_IMAGE_CAPTURE_WIDTH 16
#define MIN_IMAGE_CAPTURE_HEIGHT 2

#define MAX_IMAGE_SIZE_M2M (MAX_IMAGE_M2M_WIDTH * MAX_IMAGE_M2M_HEIGHT)
#define MAX_IMAGE_SIZE_CAPTURE (MAX_IMAGE_CAPTURE_WIDTH * MAX_IMAGE_CAPTURE_HEIGHT)
#define MAX_HEADER_SIZE (4096)

#define PIC_SIZE_STEP_WIDTH 16
#define PIC_SIZE_STEP_HEIGHT 2

struct npp_buf_object {
	struct device *dev;
	dma_addr_t paddr;
	void *vaddr;
	size_t size;

	unsigned char done;
	unsigned char ready;
};

struct npp_ctx {
	struct mutex npp_mutex;
	struct npp_buf_object *cap_q_buf_obj;
	struct list_head buffers_head;
	struct mutex buffer_lock; /* buffer list lock */
	uint32_t seq_cap;
	struct v4l2_format  cap;
	struct work_struct frame_work;
	struct timer_list vir_in_timer;
	uint32_t  memory_cap;
	int plock_offset[CAP_BUF_COUNT_MAX];
	int buffer_status[CAP_BUF_COUNT_MAX];
	int rpc_opt;

	struct v4l2_rect rect;

	struct v4l2_format out;
	struct task_struct *thread_out, *thread_cap, *thread_run;
	int	thread_out_interval, thread_cap_interval, thread_run_interval;
	uint32_t seq_out;
	spinlock_t npp_spin_lock;
	wait_queue_head_t npp_out_waitq;
	wait_queue_head_t npp_cap_waitq;
	wait_queue_head_t npp_run_waitq;
	int is_cap_started, is_out_started;
	uint32_t bufcnt_out, bufcnt_cap;
	uint32_t memory_out;
	bool stop_cmd;
	int cap_retry_cnt;
	uint64_t out_q_cnt;
	uint64_t cap_q_cnt;

#ifdef ENABLE_NPP_MEASURE_TIME
	ktime_t start_time, end_time;
	uint64_t delta_ns, sum_ns;
	int excution_count;
#endif
	struct npp_buf_object *out_q_buf_obj;

	bool npp_hw_finish;

	struct vb2_v4l2_buffer *out_v4l2_buf;
	struct vb2_v4l2_buffer *cap_v4l2_buf;
};

struct npp_fmt {
	unsigned int v4l2_pix_fmt;
	unsigned int max_width;
	unsigned int min_width;
	unsigned int max_height;
	unsigned int min_height;
	unsigned int num_planes;
};

enum TARGET_SOC {
	STARK = 0,
	KENT,
	PRINCE,
};

enum NPP_FMT_TYPE {
	NPP_FMT_TYPE_OUTPUT = 0,
	NPP_FMT_TYPE_CAPTURE,
};

#endif
