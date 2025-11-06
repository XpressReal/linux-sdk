// SPDX-License-Identifier: GPL-2.0-only
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/dma-fence.h>
#include <linux/platform_device.h>
#include <linux/sync_file.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_print.h>
#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>
#include "rtk_drm_drv.h"
#include "rtk_drm_gem.h"
#include "rtk_drm_rpc.h"
#include "rtk_drm_vowb.h"
#include "rtk_drm_vowb_job.h"
#include "rtk_drm_vowb_priv.h"
#include "rtk_drm_ringbuffer.h"
#include "uapi/rtk_drm_vowb.h"

#define CREATE_TRACE_POINTS
#include "rtk_drm_vowb_trace.h"

#define RTK_DRM_VOWB_BRINGBUFFER_SIZE      (16*1024)
#define RTK_DRM_VOWB_REFCLOCK_SIZE         (2048)

struct rtk_drm_vowb_fence {
	struct dma_fence base;
};

struct rtk_drm_vowb_func1_data {
	struct rtk_drm_vowb_job job;
	struct drm_file *file_priv;
	struct rtk_drm_vowb_src_pic srcs[RTK_DRM_VOWB_MAX_SRC_PIC];
	u32 w;
	u32 h;
	u32 y_offset;
	u32 c_offset;
	u32 format;
	u32 handles[4];
	u32 num_handles;
	dma_addr_t addrs[4];
	u32 num_srcs;
	u32 dst_id;
	u32 flags;
	u64 cnt_display;
	u32 cnt_vowb;
	u32 enabled : 1;
	u32 should_stop : 1;
	u32 should_teardown : 1;
	u32 create_fence : 1;
	spinlock_t data_lock;
};

struct rtk_drm_vowb_func2_data {
	struct rtk_drm_vowb_job job;
	struct drm_file *file_priv;
};

struct refclock_data {
	dma_addr_t addr;
	void *virt;
	u32 size;
};

struct rtk_drm_vowb {
	struct drm_device *dev;
	struct rtk_rpc_info *rpc_info;
	u32 instance;
	struct tag_refclock *shm_refclock;
	struct refclock_data refclock_data;

	struct rtk_drm_ringbuffer tx;
	struct rtk_drm_ringbuffer rx;

	spinlock_t fence_lock;
	spinlock_t lock;
	spinlock_t tx_lock;

	u64 emit_job_id;
	atomic64_t resp_job_id;
	wait_queue_head_t wq;
	struct delayed_work work;
	struct rtk_drm_vowb_job *cur_job;
	struct dma_fence *fence;
	u64 fence_context;
	u64 emit_seqno;

	struct rtk_drm_vowb_func1_data func1_data;
	struct rtk_drm_vowb_func2_data func2_data;
};

static void rtk_drm_vowb_check_resp(struct work_struct *work);

static int rtk_drm_alloc_refclock(struct rtk_drm_vowb *vowb)
{
	struct drm_device *drm = vowb->dev;
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;
	struct rpmsg_device *rpdev = get_rpdev(rpc_info);
	struct refclock_data *r = &vowb->refclock_data;

	r->size = RTK_DRM_VOWB_REFCLOCK_SIZE;
	r->virt = dma_alloc_coherent(drm->dev, r->size, &r->addr, GFP_KERNEL | __GFP_NOWARN);
	if (!r->virt)
		return -ENOMEM;

	vowb->shm_refclock = r->virt;
	vowb->shm_refclock->RCD = cpu_to_rpmsg64(rpdev, -1LL);
	vowb->shm_refclock->RCD_ext = cpu_to_rpmsg32(rpdev, -1L);
	vowb->shm_refclock->masterGPTS = cpu_to_rpmsg64(rpdev, -1LL);
	vowb->shm_refclock->GPTSTimeout = cpu_to_rpmsg64(rpdev, 0LL);
	vowb->shm_refclock->videoSystemPTS = cpu_to_rpmsg64(rpdev, -1LL);
	vowb->shm_refclock->audioSystemPTS = cpu_to_rpmsg64(rpdev, -1LL);
	vowb->shm_refclock->videoRPTS = cpu_to_rpmsg64(rpdev, -1LL);
	vowb->shm_refclock->audioRPTS = cpu_to_rpmsg64(rpdev, -1LL);
	vowb->shm_refclock->videoContext = cpu_to_rpmsg32(rpdev, -1);
	vowb->shm_refclock->audioContext = cpu_to_rpmsg32(rpdev, -1);
	vowb->shm_refclock->videoEndOfSegment = cpu_to_rpmsg32(rpdev, -1);
	vowb->shm_refclock->videoFreeRunThreshold = cpu_to_rpmsg32(rpdev, 0x7FFFFFFF);
	vowb->shm_refclock->audioFreeRunThreshold = cpu_to_rpmsg32(rpdev, 0x7FFFFFFF);
	vowb->shm_refclock->VO_Underflow = cpu_to_rpmsg32(rpdev, 0);
	vowb->shm_refclock->AO_Underflow = cpu_to_rpmsg32(rpdev, 0);
	vowb->shm_refclock->mastership.systemMode = (unsigned char)AVSYNC_FORCED_SLAVE;
	vowb->shm_refclock->mastership.videoMode = (unsigned char)AVSYNC_FORCED_MASTER;
	vowb->shm_refclock->mastership.audioMode = (unsigned char)AVSYNC_FORCED_MASTER;
	vowb->shm_refclock->mastership.masterState = (unsigned char)AUTOMASTER_NOT_MASTER;
	return 0;
}

static void rtk_drm_vowb_free_refclock(struct rtk_drm_vowb *vowb)
{
	struct refclock_data *r = &vowb->refclock_data;

	dma_free_coherent(vowb->dev->dev, r->size, r->virt, r->addr);
}

static int rtk_drm_vowb_setup_agent(struct rtk_drm_vowb *vowb)
{
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;

	if (rpc_create_video_agent(rpc_info, &vowb->instance, VF_TYPE_VIDEO_OUT)) {
		DRM_ERROR("failed in rpc_create_video_agent()\n");
		return -1;
	}
	return 0;
}

static int rtk_drm_vowb_display(struct rtk_drm_vowb *vowb, bool zero_buffer)
{
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;
	struct rpc_vo_filter_display info = {};

	info.instance = vowb->instance;
	info.videoPlane = VO_VIDEO_PLANE_V1;
	info.zeroBuffer = zero_buffer ? 1 : 0;
	info.realTimeSrc = 0;
	if (rpc_video_display(rpc_info, &info)) {
		DRM_ERROR("failed in rpc_video_display()\n");
		return -EINVAL;
	}
	return 0;
}

static int rtk_drm_vowb_setup_ringbuffer(struct rtk_drm_vowb *vowb, struct rtk_drm_ringbuffer *rb,
					 u32 pin_id)
{
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;
	struct rpc_ringbuffer ringbuffer = {};

	ringbuffer.instance = vowb->instance;
	ringbuffer.readPtrIndex = 0;
	ringbuffer.pinID = pin_id;
	ringbuffer.pRINGBUFF_HEADER = (u32)rb->addr + rb->header_offset;
	if (rpc_video_init_ringbuffer(rpc_info, &ringbuffer)) {
		DRM_ERROR("failed in rpc_video_init_ringbuffer()\n");
		return -EINVAL;
	}
	return 0;
}

static int rtk_drm_config_display_window(struct rtk_drm_vowb *vowb,
					 struct vo_rectangle *video_win,
					 struct vo_rectangle *border_win)
{
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;
	struct vo_rectangle rect = {};
	struct vo_color blueBorder = {0, 0, 255, 1};
	struct rpc_config_disp_win disp_win = {};

	disp_win.videoPlane = VO_VIDEO_PLANE_V1 | (0 << 16);
	disp_win.videoWin = video_win ? *video_win : rect;
	disp_win.borderWin = border_win ? *border_win : rect;
	disp_win.borderColor = blueBorder;
	disp_win.enBorder = 0;
	if (rpc_video_config_disp_win(rpc_info, &disp_win)) {
		DRM_ERROR("failed rpc_video_config_disp_win()\n");
		return -EINVAL;
	}
	return 0;
}

static int rtk_drm_vowb_setup_refclock(struct rtk_drm_vowb *vowb)
{
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;
	struct refclock_data *r = &vowb->refclock_data;
	struct rpc_refclock refclock = {};

	refclock.instance = vowb->instance;
	refclock.pRefClock = (u32)r->addr;
	if (rpc_video_set_refclock(rpc_info, &refclock)) {
		DRM_ERROR("failed in rpc_video_set_refclock()\n");
		return -EINVAL;
	}
	return 0;
}

static int rtk_drm_vowb_run(struct rtk_drm_vowb *vowb)
{
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;

	if (rpc_video_run(rpc_info, vowb->instance)) {
		DRM_ERROR("failed in rpc_video_run()\n");
		return -EINVAL;
	}
	return 0;
}

static int rtk_drm_vowb_stop(struct rtk_drm_vowb *vowb)
{
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;

	if (rpc_video_stop(rpc_info, vowb->instance)) {
		DRM_ERROR("failed in rpc_video_stop()\n");
		return -EINVAL;
	}
	return 0;
}

static void rtk_drm_destroy_agent(struct rtk_drm_vowb *vowb)
{
	struct rtk_rpc_info *rpc_info = vowb->rpc_info;

	WARN_ON_ONCE(rpc_destroy_video_agent(rpc_info, vowb->instance));
	vowb->instance = U32_MAX;
}

static int rtk_drm_vowb_handle_to_addr(struct drm_file *file_priv, u32 handle, dma_addr_t *addr)
{
	struct drm_gem_object *obj;
	struct rtk_gem_object *robj;

	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DRM_ERROR("Failed to lookup GEM object of handle %u\n", handle);
		return -ENXIO;
	}

	robj = to_rtk_gem_obj(obj);
	*addr = robj->paddr;
	drm_gem_object_put(obj);

	return 0;
}

static int rtk_drm_vowb_handle_to_vaddr(struct drm_file *file_priv, u32 handle, void **addr)
{
	struct drm_gem_object *obj;
	struct rtk_gem_object *robj;

	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DRM_ERROR("Failed to lookup GEM object of handle %u\n", handle);
		return -ENXIO;
	}

	robj = to_rtk_gem_obj(obj);
	*addr = robj->vaddr;
	drm_gem_object_put(obj);

	return 0;
}

static int rtk_drm_vowb_handle_to_addr_offset(struct drm_file *file_priv, u32 handle, u32 offset,
					      dma_addr_t *addr)
{
	int ret;

	ret = rtk_drm_vowb_handle_to_addr(file_priv, handle, addr);
	if (!ret)
		*addr += offset;
	return 0;
}

static inline struct rtk_drm_vowb_fence *
to_rtk_drm_vowb_fence(struct dma_fence *fence)
{
	return (struct rtk_drm_vowb_fence *)fence;
}

static const char *rtk_drm_vowb_fence_get_driver_name(struct dma_fence *fence)
{
	return "rtk_vowb";
}

static const char *rtk_drm_vowb_fence_get_timeline_name(struct dma_fence *fence)
{
	return "rtk_vowb";
}

static const struct dma_fence_ops rtk_drm_vowb_fence_ops = {
	.get_driver_name = rtk_drm_vowb_fence_get_driver_name,
	.get_timeline_name = rtk_drm_vowb_fence_get_timeline_name,
};

static struct dma_fence *rtk_drm_vowb_fence_create(struct rtk_drm_vowb *vowb)
{
	struct rtk_drm_vowb_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return ERR_PTR(-ENOMEM);

	dma_fence_init(&fence->base, &rtk_drm_vowb_fence_ops, &vowb->fence_lock,
		       vowb->fence_context, ++vowb->emit_seqno);
	return &fence->base;
}

static int rtk_drm_vowb_sync_file_create(struct dma_fence *fence)
{
	int fd;
	struct sync_file *sync_file;

	if (!fence)
		return -EINVAL;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		return fd;
	}

	sync_file = sync_file_create(fence);
	if (!sync_file) {
		put_unused_fd(fd);
		return -ENOMEM;
	}

	fd_install(fd, sync_file->file);
	return fd;
}

static void rtk_drm_vowb_signal_completion(struct rtk_drm_vowb *vowb, int error)
{
	unsigned long flags;
	struct dma_fence *fence;
	struct dma_fence *new_fence;

	spin_lock_irqsave(&vowb->lock, flags);
	if (vowb->fence) {
		fence = vowb->fence;
		vowb->fence = NULL;

		if (vowb->func1_data.enabled) {
			new_fence = rtk_drm_vowb_fence_create(vowb);
			if (IS_ERR(fence))
				DRM_WARN("failed to create fence\n");
	                else
		                vowb->fence = new_fence;
		}

		if (error)
			dma_fence_set_error(fence, error);
		dma_fence_signal(fence);
		dma_fence_put(fence);
	}
	spin_unlock_irqrestore(&vowb->lock, flags);

	wake_up_all(&vowb->wq);
}

static int rtk_drm_vowb_set_job(struct rtk_drm_vowb *vowb, struct rtk_drm_vowb_job *job)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&vowb->lock, flags);
	if (vowb->cur_job)
		ret = -EBUSY;
	else
		vowb->cur_job = job;
	spin_unlock_irqrestore(&vowb->lock, flags);

	return ret;
}

static void rtk_drm_vowb_clear_job(struct rtk_drm_vowb *vowb)
{
	unsigned long flags;

	spin_lock_irqsave(&vowb->lock, flags);
	vowb->cur_job = NULL;
	spin_unlock_irqrestore(&vowb->lock, flags);
}

static int rtk_drm_vowb_get_resp(struct rtk_drm_vowb *vowb,
				 struct video_writeback_picture_object *ret_obj)
{
	struct inband_cmd_pkg_header header = {};
	int ret;
	u32 size;

	ret = rtk_drm_ringbuffer_read(&vowb->rx, &header, sizeof(header), false);
	if (ret)
		return ret;

	size = rpmsg32_to_cpu(vowb->rx.rpdev, header.size);
	if (size != sizeof(*ret_obj)) {
		rtk_drm_ringbuffer_read(&vowb->rx, NULL, size, true);
		return -EINVAL;
	}

	rtk_drm_ringbuffer_read(&vowb->rx, ret_obj, sizeof(*ret_obj), true);
	return 0;
}

static void rtk_drm_vowb_check_resp(struct work_struct *work)
{
	struct rtk_drm_vowb *vowb = container_of(work, struct rtk_drm_vowb, work.work);
	struct rpmsg_device *rpdev = vowb->rx.rpdev;
	u64 job_id;
	int ret;

	do {
		struct video_writeback_picture_object ret_obj = {0};
		struct rtk_drm_vowb_job *job = vowb->cur_job;

		if (!job) {
			DRM_DEBUG("no cur job\n");
			return;
		}

		ret = rtk_drm_vowb_get_resp(vowb, &ret_obj);
		if (ret == -ENODATA) {
			if (ktime_to_ms(ktime_sub(ktime_get(), job->time)) >= 1000) {
				DRM_WARN("job %p: timedout\n", job);
				job->status = RTK_DRM_VOWB_JOB_STATUS_TIMEOUT;
				trace_vowb_job_update_status(job);
				rtk_drm_vowb_clear_job(vowb);
				rtk_drm_vowb_signal_completion(vowb, -ETIMEDOUT);
				return;
			}
			goto resched;
		} else if (ret) {
			DRM_ERROR("rtk_drm_vowb_get_resp() returns %d\n", ret);
			return;
		}

		job_id = ((u64)rpmsg32_to_cpu(rpdev, ret_obj.bufferID_H) << 32) |
			rpmsg32_to_cpu(rpdev, ret_obj.bufferID_L);

		if (job_id > atomic64_read(&vowb->resp_job_id)) {
			atomic64_set(&vowb->resp_job_id, job_id);
			trace_vowb_update_resp_job_id(job_id);
		}

		if (job->job_id <= job_id) {
			if (job->job_done_cb)
				job->job_done_cb(vowb, job);

			job->status = RTK_DRM_VOWB_JOB_STATUS_DONE;
			trace_vowb_job_update_status(job);
			rtk_drm_vowb_clear_job(vowb);

			rtk_drm_vowb_signal_completion(vowb, 0);
			return;
		}
	} while (1);

	return;
resched:
	schedule_delayed_work(&vowb->work, 1);
}

static int rtk_drm_vowb_queue_job(struct rtk_drm_vowb *vowb, struct rtk_drm_vowb_job *job,
				  void *cmds, u32 cmds_size)
{
	unsigned long flags;
	int ret = 0;

	ret = rtk_drm_vowb_set_job(vowb, job);
	if (ret) {
		DRM_DEBUG("queue job %p failed (cur_job %p)\n", job, vowb->cur_job);
		return ret;
	}

	spin_lock_irqsave(&vowb->tx_lock, flags);
	ret = rtk_drm_ringbuffer_write(&vowb->tx, cmds, cmds_size);
	spin_unlock_irqrestore(&vowb->tx_lock, flags);
	if (ret) {
		DRM_ERROR("rtk_drm_ringbuffer_write() returns %d\n", ret);
		rtk_drm_vowb_clear_job(vowb);
		return ret;
	}

	job->job_id = vowb->emit_job_id;
	job->time = ktime_get();
	job->status = RTK_DRM_VOWB_JOB_STATUS_START;
	trace_vowb_job_update_status(job);

	schedule_delayed_work(&vowb->work, 1);
	return 0;
}

static void rtk_drm_vowb_wait_job_done(struct rtk_drm_vowb *vowb)
{
	flush_delayed_work(&vowb->work);
}

static void inband_cmd_video_object(struct rpmsg_device *rpdev, struct video_object *cmd,
				    struct rtk_drm_vowb_func1_data *func1, dma_addr_t dst_addr)
{
	cmd->lumaOffTblAddr               = cpu_to_rpmsg32(rpdev, 0xffffffff);
	cmd->chromaOffTblAddr             = cpu_to_rpmsg32(rpdev, 0xffffffff);
	cmd->lumaOffTblAddrR              = cpu_to_rpmsg32(rpdev, 0xffffffff);
	cmd->chromaOffTblAddrR            = cpu_to_rpmsg32(rpdev, 0xffffffff);
	cmd->bufBitDepth                  = cpu_to_rpmsg32(rpdev, 8);
	cmd->matrix_coefficients          = cpu_to_rpmsg32(rpdev, 1);
	cmd->tch_hdr_metadata.specVersion = cpu_to_rpmsg32(rpdev, -1);
	cmd->Y_addr_Right                 = cpu_to_rpmsg32(rpdev, 0xffffffff);
	cmd->U_addr_Right                 = cpu_to_rpmsg32(rpdev, 0xffffffff);
	cmd->pLock_Right                  = cpu_to_rpmsg32(rpdev, 0xffffffff);

	cmd->header.type                  = cpu_to_rpmsg32(rpdev, VIDEO_VO_INBAND_CMD_TYPE_OBJ_PIC);
	cmd->header.size                  = cpu_to_rpmsg32(rpdev, sizeof(struct video_object));
	cmd->version                      = cpu_to_rpmsg32(rpdev, 0x72746b3f);
	cmd->width                        = cpu_to_rpmsg32(rpdev, func1->w);
	cmd->height                       = cpu_to_rpmsg32(rpdev, func1->h);
	cmd->Y_pitch                      = cpu_to_rpmsg32(rpdev, func1->w);
	cmd->mode                         = cpu_to_rpmsg32(rpdev, CONSECUTIVE_FRAME);
	cmd->Y_addr                       = cpu_to_rpmsg32(rpdev, dst_addr + func1->y_offset);
	cmd->U_addr                       = cpu_to_rpmsg32(rpdev, dst_addr + func1->c_offset);
}


static void inband_cmd_video_transcode_picture_object(struct rpmsg_device *rpdev,
						      struct video_transcode_picture_object *cmd,
						      struct rtk_drm_vowb_func1_data *func1,
						      dma_addr_t dst_addr,
						      struct rtk_drm_vowb_src_pic *src,
						      dma_addr_t src_addr,
						      u64 job_id)
{
	cmd->header.size  = cpu_to_rpmsg32(rpdev, sizeof(*cmd));
	cmd->header.type  = cpu_to_rpmsg32(rpdev, VIDEO_TRANSCODE_INBAND_CMD_TYPE_PICTURE_OBJECT);
	cmd->version      = cpu_to_rpmsg32(rpdev, 0x54524134);
	cmd->bufferID_H   = cpu_to_rpmsg32(rpdev, job_id >> 32);
	cmd->bufferID_L   = cpu_to_rpmsg32(rpdev, job_id & 0xffffffff);

	cmd->mode         = cpu_to_rpmsg32(rpdev, src->mode);
	cmd->Y_addr       = cpu_to_rpmsg32(rpdev, src_addr + src->y_offset);
	cmd->U_addr       = cpu_to_rpmsg32(rpdev, src_addr + src->c_offset);
	cmd->width        = cpu_to_rpmsg32(rpdev, src->w);
	cmd->height       = cpu_to_rpmsg32(rpdev, src->h);
	cmd->Y_pitch      = cpu_to_rpmsg32(rpdev, src->y_pitch);
	cmd->C_pitch      = cpu_to_rpmsg32(rpdev, src->c_pitch);
	cmd->crop_width   = cpu_to_rpmsg32(rpdev, src->crop_w);
	cmd->crop_height  = cpu_to_rpmsg32(rpdev, src->crop_h);
	cmd->crop_x       = cpu_to_rpmsg32(rpdev, src->crop_x);
	cmd->crop_y       = cpu_to_rpmsg32(rpdev, src->crop_y);
	cmd->targetFormat = cpu_to_rpmsg32(rpdev, src->target_format);
	cmd->wb_y_addr    = cpu_to_rpmsg32(rpdev, dst_addr + func1->y_offset + src->resize_win_x +
					   src->resize_win_y * func1->w);
	cmd->wb_c_addr    = cpu_to_rpmsg32(rpdev, dst_addr + func1->c_offset + src->resize_win_x +
					   src->resize_win_y / 2 * func1->w);
	cmd->wb_w         = cpu_to_rpmsg32(rpdev, src->resize_win_w);
	cmd->wb_h         = cpu_to_rpmsg32(rpdev, src->resize_win_h);
	cmd->wb_pitch     = cpu_to_rpmsg32(rpdev, func1->w);
	cmd->contrast     = cpu_to_rpmsg32(rpdev, src->contrast);
	cmd->brightness   = cpu_to_rpmsg32(rpdev, src->brightness);
	cmd->hue          = cpu_to_rpmsg32(rpdev, src->hue);
	cmd->saturation   = cpu_to_rpmsg32(rpdev, src->saturation);
	cmd->sharp_en     = cpu_to_rpmsg32(rpdev, src->sharp_en);
	cmd->sharp_value  = cpu_to_rpmsg32(rpdev, src->sharp_value);
}

static int rtk_drm_vowb_func1_prepare_cmds(struct rtk_drm_vowb *vowb,
					   struct rtk_drm_vowb_func1_data *func1,
					   struct rpmsg_device *rpdev,
					   struct video_transcode_picture_object *cmds,
					   u32 num_cmds, u64 job_id, u32 dst_id)
{
	struct drm_file *file_priv = func1->file_priv;
	dma_addr_t src_addr[RTK_DRM_VOWB_MAX_SRC_PIC] = {};
	int i, j = 0;
	int ret;
	unsigned long flags;

	if (num_cmds < func1->num_srcs)
		return -EINVAL;

	spin_lock_irqsave(&func1->data_lock, flags);
	for (i = 0; i < func1->num_srcs; i++) {
		struct rtk_drm_vowb_src_pic *src = &func1->srcs[i];

		if (!src->handle)
			continue;

		ret = rtk_drm_vowb_handle_to_addr(file_priv, src->handle, &src_addr[i]);
		if (ret) {
			DRM_WARN("failed to get addr of src%d\n", i);
			continue;
		}

		inband_cmd_video_transcode_picture_object(rpdev, &cmds[j], func1, func1->addrs[dst_id],
							  &func1->srcs[i], src_addr[i], ++job_id);
		j++;
	}
	spin_unlock_irqrestore(&func1->data_lock, flags);

	return j;
}

static int rtk_drm_vowb_func1_start_cmd(struct rtk_drm_vowb *vowb,
					struct rtk_drm_vowb_func1_data *func1)
{
	struct video_transcode_picture_object *cmds;
	int ret;

	cmds = kcalloc(func1->num_srcs, sizeof(*cmds), GFP_KERNEL);
	if (!cmds)
		return -ENOMEM;

	ret = rtk_drm_vowb_func1_prepare_cmds(vowb, func1, vowb->tx.rpdev, cmds, func1->num_srcs,
					      vowb->emit_job_id, func1->dst_id);
	if (ret <= 0) {
		DRM_INFO("rtk_drm_vowb_func1_prepare_cmds() returns %d\n", ret);
		goto free_cmds;
	}

	vowb->emit_job_id += ret;

	ret = rtk_drm_vowb_queue_job(vowb, &func1->job, cmds, sizeof(*cmds) * ret);
	if (!ret)
		++func1->cnt_vowb;
	trace_vowb_func1_statistics(__func__, func1->cnt_display, func1->cnt_vowb);
free_cmds:
	kfree(cmds);
	return ret;
}

static void rtk_drm_vowb_func1_job_done_cb(struct rtk_drm_vowb *vowb, struct rtk_drm_vowb_job *job)
{
	struct rtk_drm_vowb_func1_data *func1 = container_of(job, struct rtk_drm_vowb_func1_data, job);
	struct video_object cmd = {};
	unsigned long flags;
	int ret;

	if (!func1->enabled)
		return;

	inband_cmd_video_object(vowb->tx.rpdev, &cmd, func1, func1->addrs[func1->dst_id]);
	func1->dst_id = (func1->dst_id + 1) % func1->num_handles;

	spin_lock_irqsave(&vowb->tx_lock, flags);
	ret = rtk_drm_ringbuffer_write(&vowb->tx, &cmd, sizeof(cmd));
	spin_unlock_irqrestore(&vowb->tx_lock, flags);
	if (ret)
		DRM_ERROR("rtk_drm_ringbuffer_write() returns %d\n", ret);
	else
		++func1->cnt_display;

	trace_vowb_func1_statistics(__func__, func1->cnt_display, func1->cnt_vowb);
}

static void rtk_drm_vowb_func1_vsync_isr(struct rtk_drm_vowb *vowb)
{
	struct rtk_drm_vowb_func1_data *func1 = &vowb->func1_data;

	if (!func1->enabled)
		return;

	rtk_drm_vowb_func1_start_cmd(vowb, func1);
}

static inline bool rtk_drm_vowb_func1_check_file(struct rtk_drm_vowb *vowb,
						 struct drm_file *file_priv)
{
	return vowb->func1_data.file_priv == file_priv;
}

static int rtk_drm_vowb_func1_start(struct rtk_drm_vowb *vowb)
{
	struct rtk_drm_vowb_func1_data *func1 = &vowb->func1_data;
	struct vo_rectangle rect = {0, 0, func1->w, func1->h};
	int ret;

	ret = rtk_drm_vowb_display(vowb, 0);
	if (ret)
		return ret;
	ret = rtk_drm_vowb_run(vowb);
	if (ret)
		return ret;
	ret = rtk_drm_config_display_window(vowb, &rect, &rect);
	if (ret)
		return ret;
	ret = rtk_drm_vowb_display(vowb, 1);
	if (ret)
		return ret;

	func1->dst_id = 0;
	func1->should_stop = 1;
	func1->enabled = 1;

	if (func1->create_fence) {
		struct dma_fence *fence;
		unsigned long flags;

		spin_lock_irqsave(&vowb->lock, flags);
		fence = rtk_drm_vowb_fence_create(vowb);
		if (IS_ERR(fence))
			DRM_WARN("failed to create fence\n");
		else
			vowb->fence = fence;
		spin_unlock_irqrestore(&vowb->lock, flags);
	}

	return rtk_drm_vowb_func1_start_cmd(vowb, func1);
}

static int rtk_drm_vowb_func1_stop(struct rtk_drm_vowb *vowb)
{
	struct rtk_drm_vowb_func1_data *func1 = &vowb->func1_data;

	if (!func1->should_stop)
		return 0;

	func1->enabled = 0;
	rtk_drm_vowb_display(vowb, 0);
	rtk_drm_config_display_window(vowb, NULL, NULL);
	rtk_drm_vowb_stop(vowb);
	func1->should_stop = 0;
	return 0;
}

static int rtk_drm_vowb_func1_teardown(struct rtk_drm_vowb *vowb)
{
	struct rtk_drm_vowb_func1_data *func1 = &vowb->func1_data;
	int i = func1->num_handles;

	if (!func1->should_teardown)
		return 0;

	flush_delayed_work(&vowb->work);
	for (i--; i >= 0; i--) {
		drm_gem_handle_delete(func1->file_priv, func1->handles[i]);
		func1->handles[i] = 0;
	}
	func1->file_priv = NULL;
	func1->should_teardown = 0;
	return 0;
}

static void rtk_drm_vowb_func1_init(struct rtk_drm_vowb_func1_data *func1)
{
	memset(func1, 0, sizeof(*func1));
}

static int rtk_drm_vowb_func1_setup(struct drm_device *dev, struct rtk_drm_vowb *vowb,
				    struct drm_file *file_priv, struct rtk_drm_vowb_setup *arg)
{
	struct rtk_drm_vowb_func1_data *func1 = &vowb->func1_data;
	int i;
	int ret;

	rtk_drm_vowb_func1_init(func1);
	spin_lock_init(&func1->data_lock);
	func1->w               = arg->dst.w;
	func1->h               = arg->dst.h;
	func1->format          = arg->dst.format;
	func1->num_handles     = arg->num_handles;
	func1->num_srcs        = arg->num_srcs;
	func1->flags           = arg->flags;
	func1->y_offset        = 0;
	func1->c_offset        = func1->h * func1->w;
	func1->create_fence     = !!(arg->flags & RTK_DRM_VOWB_FLAGS_SETUP_CREATE_FENCE);

	for (i = 0; i < func1->num_handles; i++) {
		struct drm_mode_create_dumb dumb_arg = {
			.height = func1->h * 3 / 2,
			.width = func1->w,
			.bpp = 8,
		};
		void *virt;

		ret = rtk_gem_dumb_create(file_priv, dev, &dumb_arg);
		if (ret)
			goto del_handles;
		func1->handles[i] = dumb_arg.handle;

		ret = rtk_drm_vowb_handle_to_addr(file_priv, func1->handles[i], &func1->addrs[i]);
		if (ret)
			goto del_handles;

		ret = rtk_drm_vowb_handle_to_vaddr(file_priv, func1->handles[i], &virt);
		if (!ret) {
			memset(virt, 16, func1->h * func1->w);
			memset(virt + func1->h * func1->w, 128, func1->h * func1->w / 2);
		}
	}

	func1->should_teardown = 1;
	func1->job.job_done_cb = rtk_drm_vowb_func1_job_done_cb;
	func1->file_priv       = file_priv;
	return 0;

del_handles:
	for (i--; i >= 0; i--) {
		drm_gem_handle_delete(file_priv, func1->handles[i]);
		func1->handles[i] = 0;
	}
	return ret;
}

int rtk_drm_vowb_setup_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb_setup *arg = data;
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;
	struct rtk_drm_vowb_func1_data *func1 = &vowb->func1_data;

	if (!vowb)
		return -ENOIOCTLCMD;
	if (func1->file_priv)
		return -EBUSY;

	return rtk_drm_vowb_func1_setup(dev, vowb, file_priv, arg);;
}

int rtk_drm_vowb_teardown_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;

	if (!vowb)
		return -ENOIOCTLCMD;
	if (!rtk_drm_vowb_func1_check_file(vowb, file_priv))
		return -EINVAL;

	return rtk_drm_vowb_func1_teardown(vowb);
}

int rtk_drm_vowb_set_crtc_vblank_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb_set_crtc_vblank *arg = data;
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, dev) {
		if (crtc->base.id == arg->crtc_id) {
			if (arg->enable)
				return drm_crtc_vblank_get(crtc);
			else {
				drm_crtc_vblank_put(crtc);
				return 0;
			}
		}
	}
	return -EINVAL;
}

int rtk_drm_vowb_release(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev = file_priv->minor->dev;
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;

	if (!vowb)
		return 0;
	if (rtk_drm_vowb_func1_check_file(vowb, file_priv)) {
		rtk_drm_vowb_func1_stop(vowb);
		rtk_drm_vowb_func1_teardown(vowb);
	}
	rtk_drm_vowb_wait_job_done(vowb);
	rtk_drm_vowb_clear_job(vowb);
	return 0;
}

int rtk_drm_vowb_add_src_pic_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb_add_src_pic *arg = data;
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;
	struct rtk_drm_vowb_func1_data *func1 = &vowb->func1_data;
	unsigned long flags;
	int fd = -1;

	if (!vowb)
		return -ENOIOCTLCMD;
	if (!rtk_drm_vowb_func1_check_file(vowb, file_priv))
		return -EINVAL;
	if (arg->index >= func1->num_srcs)
		return -EINVAL;

	spin_lock_irqsave(&func1->data_lock, flags);
	func1->srcs[arg->index] = arg->src;
	spin_unlock_irqrestore(&func1->data_lock, flags);

	arg->fence_fd = -1;

	spin_lock_irqsave(&vowb->lock, flags);
	if (func1->create_fence && vowb->fence) {
		fd = rtk_drm_vowb_sync_file_create(vowb->fence);
		if (fd <= 0) {
			DRM_ERROR("failed to create sync_file: %d\n", fd);
			fd = -1;
		}
		arg->fence_fd = fd;
	}
	spin_unlock_irqrestore(&vowb->lock, flags);

	return 0;
}

int rtk_drm_vowb_get_dst_pic_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb_dst_pic *arg = data;
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;
	struct rtk_drm_vowb_func1_data *func1 = &vowb->func1_data;

	if (!vowb)
		return -ENOIOCTLCMD;
	if (!rtk_drm_vowb_func1_check_file(vowb, file_priv))
		return -EINVAL;

	arg->w = func1->w;
	arg->h = func1->h;
	arg->format = func1->format;
	return 0;
}

int rtk_drm_vowb_start_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;

	if (!vowb)
		return -ENOIOCTLCMD;
	if (!rtk_drm_vowb_func1_check_file(vowb, file_priv))
		return -EINVAL;

	return rtk_drm_vowb_func1_start(vowb);
}

int rtk_drm_vowb_stop_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;

	if (!vowb)
		return -ENOIOCTLCMD;
	if (!rtk_drm_vowb_func1_check_file(vowb, file_priv))
		return -EINVAL;

	return rtk_drm_vowb_func1_stop(vowb);
}

static void func2_inband_cmd_video_transcode_picture_object(struct rpmsg_device *rpdev,
							    struct video_transcode_picture_object *cmd,
							    struct rtk_drm_vowb_pic *pic,
							    dma_addr_t addrs[11],
							    u64 job_id)
{
	cmd->header.size  = cpu_to_rpmsg32(rpdev, sizeof(*cmd));
	cmd->header.type  = cpu_to_rpmsg32(rpdev, VIDEO_TRANSCODE_INBAND_CMD_TYPE_PICTURE_OBJECT);

	cmd->version      = cpu_to_rpmsg32(rpdev, 0x54524136);

	cmd->bufferID_H   = cpu_to_rpmsg32(rpdev, job_id >> 32);
	cmd->bufferID_L   = cpu_to_rpmsg32(rpdev, job_id & 0xffffffff);

	cmd->mode         = cpu_to_rpmsg32(rpdev, pic->mode);
	cmd->Y_addr       = cpu_to_rpmsg32(rpdev, addrs[0]);
	cmd->U_addr       = cpu_to_rpmsg32(rpdev, addrs[1]);
	cmd->width        = cpu_to_rpmsg32(rpdev, pic->w);
	cmd->height       = cpu_to_rpmsg32(rpdev, pic->h);
	cmd->Y_pitch      = cpu_to_rpmsg32(rpdev, pic->y_pitch);
	cmd->C_pitch      = cpu_to_rpmsg32(rpdev, pic->c_pitch);

	cmd->lumaOffTblAddr = cpu_to_rpmsg32(rpdev, addrs[4]);
	cmd->chromaOffTblAddr = cpu_to_rpmsg32(rpdev, addrs[5]);
	cmd->bufBitDepth  = cpu_to_rpmsg32(rpdev, pic->buf_bit_depth);
	cmd->bufFormat    = cpu_to_rpmsg32(rpdev, pic->buf_format);

	cmd->wb_y_addr    = cpu_to_rpmsg32(rpdev, addrs[2]);
	cmd->wb_c_addr    = cpu_to_rpmsg32(rpdev, addrs[3]);
	cmd->wb_w         = cpu_to_rpmsg32(rpdev, pic->wb_w);
	cmd->wb_h         = cpu_to_rpmsg32(rpdev, pic->wb_h);
	cmd->wb_pitch     = cpu_to_rpmsg32(rpdev, pic->wb_pitch);
	cmd->targetFormat = cpu_to_rpmsg32(rpdev, pic->target_format);

	cmd->contrast     = cpu_to_rpmsg32(rpdev, pic->contrast);
	cmd->brightness   = cpu_to_rpmsg32(rpdev, pic->brightness);
	cmd->hue          = cpu_to_rpmsg32(rpdev, pic->hue);
	cmd->saturation   = cpu_to_rpmsg32(rpdev, pic->saturation);

	cmd->sharp_en     = cpu_to_rpmsg32(rpdev, pic->sharp_en);
	cmd->sharp_value  = cpu_to_rpmsg32(rpdev, pic->sharp_value);

	cmd->crop_width   = cpu_to_rpmsg32(rpdev, pic->crop_w);
	cmd->crop_height  = cpu_to_rpmsg32(rpdev, pic->crop_h);
	cmd->crop_x       = cpu_to_rpmsg32(rpdev, pic->crop_x);
	cmd->crop_y       = cpu_to_rpmsg32(rpdev, pic->crop_y);

	if (addrs[6] != -1L) {
		cmd->sub_address    = cpu_to_rpmsg32(rpdev, addrs[6]);
		cmd->sub_w          = cpu_to_rpmsg32(rpdev, pic->sub_w);
		cmd->sub_h          = cpu_to_rpmsg32(rpdev, pic->sub_h);
		cmd->sub_pitch      = cpu_to_rpmsg32(rpdev, pic->sub_pitch);
		cmd->sub_format     = cpu_to_rpmsg32(rpdev, pic->sub_format);
	}

	cmd->Y_addr_prev = cpu_to_rpmsg32(rpdev, addrs[7]);
	cmd->U_addr_prev = cpu_to_rpmsg32(rpdev, addrs[8]);
	cmd->Y_addr_next = cpu_to_rpmsg32(rpdev, addrs[9]);
	cmd->U_addr_next = cpu_to_rpmsg32(rpdev, addrs[10]);

	cmd->tvve_picture_width   = cpu_to_rpmsg32(rpdev, pic->tvve_picture_width);
	cmd->tvve_lossy_en  = cpu_to_rpmsg32(rpdev, pic->tvve_lossy_en);
	cmd->tvve_bypass_en       = cpu_to_rpmsg32(rpdev, pic->tvve_bypass_en);
	cmd->tvve_qlevel_sel_y       = cpu_to_rpmsg32(rpdev, pic->tvve_qlevel_sel_y);
	cmd->tvve_qlevel_sel_c       = cpu_to_rpmsg32(rpdev, pic->tvve_qlevel_sel_c);
}

static int rtk_drm_vowb_run_cmd_get_addrs(struct drm_file *file_priv,
					  struct rtk_drm_vowb_run_cmd *arg,
					  dma_addr_t addrs[11])
{
	int ret;

	ret = rtk_drm_vowb_handle_to_addr_offset(file_priv, arg->pic.src_handle, arg->pic.y_offset,
						 &addrs[0]);
	if (ret)
		return ret;
	ret = rtk_drm_vowb_handle_to_addr_offset(file_priv, arg->pic.src_handle, arg->pic.c_offset,
						 &addrs[1]);
	if (ret)
		return ret;

	ret = rtk_drm_vowb_handle_to_addr_offset(file_priv, arg->pic.wb_handle, arg->pic.wb_y_offset,
						 &addrs[2]);
	if (ret)
		return ret;
	ret = rtk_drm_vowb_handle_to_addr_offset(file_priv, arg->pic.wb_handle, arg->pic.wb_c_offset,
						 &addrs[3]);
	if (ret)
		return ret;

	if (arg->pic.luma_off_tbl_addr)
		addrs[4] = (dma_addr_t)arg->pic.luma_off_tbl_addr;
	else
		addrs[4] = -1L;

	if (arg->pic.chroma_off_tbl_addr)
		addrs[5] = (dma_addr_t)arg->pic.chroma_off_tbl_addr;
	else
		addrs[5] = -1L;

	if (arg->pic.y_addr_prev)
		addrs[7] = (dma_addr_t)arg->pic.y_addr_prev;
	else
		addrs[7] = 0;

	if (arg->pic.c_addr_prev)
		addrs[8] = (dma_addr_t)arg->pic.c_addr_prev;
	else
		addrs[8] = 0;

	if (arg->pic.y_addr_next)
		addrs[9] = (dma_addr_t)arg->pic.y_addr_next;
	else
		addrs[9] = 0;

	if (arg->pic.c_addr_next)
		addrs[10] = (dma_addr_t)arg->pic.c_addr_next;
	else
		addrs[10] = 0;

	addrs[6] = -1L;
	if (arg->pic.sub_enable) {
		ret = rtk_drm_vowb_handle_to_addr_offset(file_priv, arg->pic.sub_handle,
							 arg->pic.sub_offset, &addrs[6]);
		if (ret)
			return ret;
	}
	return 0;
}

int rtk_drm_vowb_run_cmd(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;
	struct rtk_drm_vowb_func2_data *func2 = &vowb->func2_data;
	struct rtk_drm_vowb_run_cmd *arg = data;
	struct video_transcode_picture_object cmd = {};
	dma_addr_t addrs[11] = {};
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&vowb->lock, flags);
	ret = rtk_drm_vowb_run_cmd_get_addrs(file_priv, arg, addrs);
	spin_unlock_irqrestore(&vowb->lock, flags);
	if (ret)
		return ret;

	func2_inband_cmd_video_transcode_picture_object(vowb->tx.rpdev, &cmd, &arg->pic, addrs,
							++vowb->emit_job_id);

	ret = rtk_drm_vowb_queue_job(vowb, &func2->job, &cmd, sizeof(cmd));
	if (!ret)
		arg->job_id = func2->job.job_id;

	return ret;
}

int rtk_drm_vowb_check_cmd(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;
	struct rtk_drm_vowb_check_cmd *arg = data;
	u32 arg_flags = arg->flags & RTK_DRM_VOWB_FLAGS_VALID_CHECK_CMD_FLAGS;
	int ret = -EAGAIN;

	if (arg_flags & RTK_DRM_VOWB_FLAGS_CHECK_CMD_BLOCK) {
		ret = wait_event_interruptible(vowb->wq,
					       arg->job_id <= atomic64_read(&vowb->resp_job_id));
	}

	if (arg->job_id <= atomic64_read(&vowb->resp_job_id)) {
		arg->job_done = 1;
		return 0;
	}
	return ret;
}

void rtk_drm_vowb_isr(struct drm_device *dev)
{
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;

	if (!vowb)
		return;
	rtk_drm_vowb_func1_vsync_isr(vowb);
}

int rtk_drm_vowb_reinit(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct rtk_drm_vowb *vowb = ((struct rtk_drm_private *)dev->dev_private)->vowb;
	int ret;

	if (vowb->instance != U32_MAX)
		rtk_drm_destroy_agent(vowb);
	rtk_drm_ringbuffer_reset(&vowb->tx);
	rtk_drm_ringbuffer_reset(&vowb->rx);

	ret = rtk_drm_vowb_setup_agent(vowb);
	if (ret)
		goto destroy_agent;
	ret = rtk_drm_vowb_setup_refclock(vowb);
	if (ret)
		goto destroy_agent;
	ret = rtk_drm_vowb_setup_ringbuffer(vowb, &vowb->tx, 0);
	if (ret)
		goto destroy_agent;
	ret = rtk_drm_vowb_setup_ringbuffer(vowb, &vowb->rx, 0x20140507);
	if (ret)
		goto destroy_agent;
	return 0;
destroy_agent:
	rtk_drm_destroy_agent(vowb);
	return ret;
}

static int rtk_vowb_bind(struct device *dev, struct device *master, void *data)
{
	struct rtk_drm_vowb *vowb;
	struct drm_device *drm = data;
	struct rtk_drm_private *priv = drm->dev_private;
	int ret;

	vowb = kzalloc(sizeof(*vowb), GFP_KERNEL);
	if (!vowb)
		return -ENOMEM;
	vowb->dev = drm;
	vowb->rpc_info = get_rpc_info(priv);
	vowb->fence_context = dma_fence_context_alloc(1);
	INIT_DELAYED_WORK(&vowb->work, rtk_drm_vowb_check_resp);
	spin_lock_init(&vowb->lock);
	spin_lock_init(&vowb->tx_lock);
	spin_lock_init(&vowb->fence_lock);
	init_waitqueue_head(&vowb->wq);
	vowb->instance = U32_MAX;

	ret = rtk_drm_ringbuffer_alloc(vowb->dev, get_rpdev(vowb->rpc_info), &vowb->tx,
				       RTK_DRM_VOWB_BRINGBUFFER_SIZE);
	if (ret) {
		DRM_ERROR("failed to alloc tx ringbuffer\n");
		return ret;
	}
	ret = rtk_drm_ringbuffer_alloc(vowb->dev, get_rpdev(vowb->rpc_info), &vowb->rx,
				       RTK_DRM_VOWB_BRINGBUFFER_SIZE);
	if (ret) {
		DRM_ERROR("failed to alloc rx ringbuffer\n");
		goto free_txbuf;
	}
	ret = rtk_drm_alloc_refclock(vowb);
	if (ret) {
		DRM_ERROR("failed to alloc refclock\n");
		goto free_rxbuf;
	}
	ret = rtk_drm_vowb_setup_agent(vowb);
	if (ret)
		goto free_refclock;
	ret = rtk_drm_vowb_setup_refclock(vowb);
	if (ret)
		goto destroy_agent;
	ret = rtk_drm_vowb_setup_ringbuffer(vowb, &vowb->tx, 0);
	if (ret)
		goto destroy_agent;
	ret = rtk_drm_vowb_setup_ringbuffer(vowb, &vowb->rx, 0x20140507);
	if (ret)
		goto destroy_agent;

	priv->vowb = vowb;
	return 0;

destroy_agent:
	rtk_drm_destroy_agent(vowb);
free_refclock:
	rtk_drm_vowb_free_refclock(vowb);
free_rxbuf:
	rtk_drm_ringbuffer_free(&vowb->rx);
free_txbuf:
	rtk_drm_ringbuffer_free(&vowb->tx);
	return ret;
}

static void rtk_vowb_unbind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_drm_vowb *vowb = priv->vowb;

	rtk_drm_destroy_agent(vowb);
	rtk_drm_vowb_free_refclock(vowb);
	rtk_drm_ringbuffer_free(&vowb->rx);
	rtk_drm_ringbuffer_free(&vowb->tx);
	kfree(vowb);
	priv->vowb = NULL;
}

const struct component_ops rtk_vowb_component_ops = {
	.bind = rtk_vowb_bind,
	.unbind = rtk_vowb_unbind,
};

static int rtk_vowb_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &rtk_vowb_component_ops);
}

static int rtk_vowb_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_vowb_component_ops);
	return 0;
}

static const struct of_device_id rtk_vowb_of_ids[] = {
	{ .compatible = "realtek,vo-writeback", },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_vowb_of_ids);

struct platform_driver rtk_vowb_driver = {
	.probe = rtk_vowb_probe,
	.remove = rtk_vowb_remove,
	.driver = {
		.name = "rtk-vowb",
		.of_match_table = rtk_vowb_of_ids,
	},
};
