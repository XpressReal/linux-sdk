/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __RTK_DRM_VOWB_JOB_H__
#define __RTK_DRM_VOWB_JOB_H__

#include <linux/ktime.h>
struct rtk_drm_vowb;

#define RTK_DRM_VOWB_JOB_STATUS_UNDEFINED 0
#define RTK_DRM_VOWB_JOB_STATUS_START 1
#define RTK_DRM_VOWB_JOB_STATUS_DONE 2
#define RTK_DRM_VOWB_JOB_STATUS_TIMEOUT 3

struct rtk_drm_vowb_job {
	u64 job_id;
	ktime_t time;
	void (*job_done_cb)(struct rtk_drm_vowb *vowb, struct rtk_drm_vowb_job *job);
	u32 status;
};

#endif
