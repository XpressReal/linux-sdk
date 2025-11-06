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

#include <linux/err.h>
#include <linux/sys_soc.h>
#if IS_ENABLED(CONFIG_KERN_RPC_HANDLE_COMMAND)
#include <linux/kthread.h>
#include <linux/slab.h>
#endif
#include <linux/io.h>
#include <linux/dma-map-ops.h>
#include <drm/drm_of.h>
//#include <asm/io.h>

#include <linux/of_device.h>
#include <linux/mfd/syscon.h>
#include <linux/hwspinlock.h>
#include <linux/regmap.h>
#include <drm/drm_print.h>

#include "rtk_drm_rpc.h"

const char *krpc_names[] = {
	"ACPU",
	"VCPU",
	"VCPU2",
	"HIFI",
	"KR4",
};

unsigned int get_rtk_flags(unsigned int dumb_flags)
{
	unsigned int ret = 0;

	if (dumb_flags & BUFFER_NONCACHED)
		ret |= RTK_FLAG_NONCACHED;
	if (dumb_flags & BUFFER_SCPUACC)
		ret |= RTK_FLAG_SCPUACC;
	if (dumb_flags & BUFFER_ACPUACC)
		ret |= RTK_FLAG_ACPUACC;
	if (dumb_flags & BUFFER_HWIPACC)
		ret |= RTK_FLAG_HWIPACC;
	if (dumb_flags & BUFFER_VE_SPEC)
		ret |= RTK_FLAG_VE_SPEC;
	if (dumb_flags & BUFFER_SECURE_AUDIO)
		ret |= RTK_FLAG_PROTECTED_V2_AUDIO_POOL;

	if (ret == 0)
		ret |= RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_HWIPACC;

	return ret;
}

bool is_media_heap(unsigned int dumb_flags)
{
	if (dumb_flags == 0)
		return true;
	if (dumb_flags & BUFFER_HEAP_MEDIA)
		return true;
	return false;
}

bool is_audio_heap(unsigned int dumb_flags)
{
	if (dumb_flags & BUFFER_HEAP_AUDIO)
		return true;
	return false;
}

unsigned int ipcReadULONG(u8 *src, unsigned int type)
{
	if (type)
		return readl(src);
	else
		return __be32_to_cpu(readl(src));
}

void ipcCopyMemory(void *p_des, void *p_src, unsigned long len, unsigned int type)
{
	unsigned char *des = (unsigned char *)p_des;
	unsigned char *src = (unsigned char *)p_src;
	int i;

	for (i = 0; i < len; i += 4) {
		if (type)
			writel(readl(&src[i]), &des[i]);
		else
			writel(__cpu_to_be32(readl(&src[i])), &des[i]);
	}
}

void rpc_send_interrupt(struct rtk_rpc_info *rpc_info)
{
	unsigned long flags;
	ktime_t start;
	s64 send_intr_time_ms;

	start = ktime_get();

	DRM_DEBUG_DRIVER("%s\n",
		DC_HAS_BIT(rpc_info->vo_sync_flag, VO_DC_SET_NOTIFY) ?
		"has interrupt bit" : "no interrupt bit");

	if (rpc_info->vo_sync_flag != NULL &&
	    DC_HAS_BIT(rpc_info->vo_sync_flag, VO_DC_SET_NOTIFY)) {

		if (rpc_info->hwlock)
			hwspin_lock_timeout_irqsave(rpc_info->hwlock, UINT_MAX, &flags);

		DC_SET_BIT(rpc_info->vo_sync_flag, VO_DC_FEEDBACK_NOTIFY);

		if (rpc_info->hwlock)
			hwspin_unlock_irqrestore(rpc_info->hwlock, &flags);

		regmap_write(rpc_info->acpu_int_base, 0xa80, (ACPU_INT_SA | ACPU_INT_WRITE));

		DRM_DEBUG_DRIVER("send interrupt done\n");

		send_intr_time_ms = ktime_ms_delta(ktime_get(), start);
		if (send_intr_time_ms > 2)
			DRM_DEBUG_DRIVER("send_intr_time_ms (%lld)\n", send_intr_time_ms);
	}
}

#if IS_ENABLED(CONFIG_KERN_RPC_HANDLE_COMMAND)
static int rpc_get_command_thread(void *data)
{
	struct rtk_rpc_info *rpc_info = (struct rtk_rpc_info *)data;
	RPC_STRUCT *rpc, *rrpc;
	char readbuf[sizeof(RPC_STRUCT)];
	char replybuf[sizeof(RPC_STRUCT) + 2*sizeof(uint32_t)];
	char *rpc_parameter;
	uint32_t *tmp;
	int size;

	while (!kthread_should_stop()) {
		krpc_wait_event(rpc_info->pid);

		if (rpc_kern_read(RPC_AUDIO, readbuf, sizeof(RPC_STRUCT)) != sizeof(RPC_STRUCT)) {
			pr_err("ERROR in read kernel RPC...\n");
			continue;
		}

		rpc = (RPC_STRUCT *)readbuf;
		size = htonl(rpc->parameterSize);
		pr_info("%s got kernel rpc %d\n", __func__, size);
		if (size) {
			rpc_parameter = kmalloc(size, GFP_KERNEL);
			if (!rpc_parameter) {
				pr_err("ERROR kmalloc parameter\n");
				continue;
			}
			if (rpc_kern_read(RPC_AUDIO, rpc_parameter, size) != size) {
				pr_err("ERROR in read kernel RPC...\n");
				kfree(rpc_parameter);
				continue;
			}
			kfree(rpc_parameter);
		}

		if (rpc->taskID) {
			pr_info("%s reply kernel rpc %d", __func__, rpc->taskID);
			rrpc = (RPC_STRUCT *)replybuf;
			/* fill the RPC_STRUCT... */
			rrpc->programID = htonl(REPLYID);
			rrpc->versionID = htonl(REPLYID);
			rrpc->procedureID = 0;
			rrpc->taskID = 0;
			rrpc->sysTID = 0;
			rrpc->sysPID = 0;
			rrpc->parameterSize = htonl(2*sizeof(uint32_t));
			rrpc->mycontext = rpc->mycontext;

			/* fill the parameters... */
			tmp = (uint32_t *)(replybuf + sizeof(RPC_STRUCT));
			*(tmp+0) = rpc->taskID; /* FIXME: should be 64bit */
			*(tmp+1) = 0;

			if (rpc_kern_write(RPC_AUDIO, replybuf, sizeof(replybuf)) !=
					sizeof(replybuf)) {
				pr_err("ERROR in send kernel RPC...\n");
				return -1;
			}
		}
		krpc_done(rpc_info->pid);
	}
	return 0;
}
#endif

static int handle_rpc_command(struct rtk_krpc_ept_info *krpc_ept_info, char *buf)
{
	struct rpc_struct *rpc, *rrpc;
	char replybuf[sizeof(struct rpc_struct) + 2*sizeof(uint32_t)];
	uint32_t *tmp;
	int size;
	int ret;

	rpc = (struct rpc_struct *)buf;
	size = rpc->parameterSize;
	pr_info("%s got kernel rpc %d\n", __func__, size);

	if (rpc->taskID) {
		pr_info("%s reply kernel rpc %d", __func__, rpc->taskID);
		rrpc = (struct rpc_struct *)replybuf;
		/* fill the RPC_STRUCT... */
		rrpc->programID = htonl(REPLYID);
		rrpc->versionID = htonl(REPLYID);
		rrpc->procedureID = 0;
		rrpc->taskID = 0;
		rrpc->sysTID = 0;
		rrpc->sysPID = 0;
		rrpc->parameterSize = htonl(2*sizeof(uint32_t));
		rrpc->mycontext = rpc->mycontext;

		/* fill the parameters... */
		tmp = (uint32_t *)(replybuf + sizeof(struct rpc_struct));
		*(tmp+0) = rpc->taskID; /* FIXME: should be 64bit */
		*(tmp+1) = 0;
		ret = rtk_send_rpc(krpc_ept_info, replybuf, sizeof(replybuf));
		if (ret != sizeof(replybuf)) {
			pr_err("ERROR in send kernel RPC...\n");
			return -1;
		}
	}
	return 0;
}

static int krpc_notify_cb(struct rtk_krpc_ept_info *krpc_ept_info, char *buf)
{
	uint32_t *tmp;
	struct rpc_struct *rpc = (struct rpc_struct *)buf;

	if (rpc->programID == REPLYID) {
		tmp = (uint32_t *)(buf + sizeof(struct rpc_struct));
		*(krpc_ept_info->retval) = *(tmp + 1);

		complete(&krpc_ept_info->ack);
	} else {
		handle_rpc_command(krpc_ept_info, buf);
	}

	return 0;
}


int drm_ept_init(struct rtk_krpc_ept_info *krpc_ept_info)
{
	int ret = 0;

	ret = krpc_info_init(krpc_ept_info, "rtk-drm", krpc_notify_cb);

	return ret;
}

static char *prepare_rpc_data(struct rtk_krpc_ept_info *krpc_ept_info, uint32_t command, uint32_t param1, uint32_t param2, int *len)
{
	struct rpc_struct *rpc;
	uint32_t *tmp;
	char *buf;

	*len = sizeof(struct rpc_struct) + 3 * sizeof(uint32_t);
	buf = kmalloc(sizeof(struct rpc_struct) + 3 * sizeof(uint32_t), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	rpc = (struct rpc_struct *)buf;
	rpc->programID = KERNELID;
	rpc->versionID = KERNELID;
	rpc->procedureID = 0;
	rpc->taskID = krpc_ept_info->id;
	rpc->sysTID = krpc_ept_info->id;
	rpc->sysPID = krpc_ept_info->id;
	rpc->parameterSize = 3*sizeof(uint32_t);
	rpc->mycontext = 0;
	tmp = (uint32_t *)(buf+sizeof(struct rpc_struct));
	*tmp = command;
	*(tmp+1) = param1;
	*(tmp+2) = param2;

	return buf;
}

int drm_send_rpc(struct device *dev, struct rtk_krpc_ept_info *krpc_ept_info, char *buf, int len, uint32_t *retval)
{
	int ret = 0;

	mutex_lock(&krpc_ept_info->send_mutex);

	krpc_ept_info->retval = retval;
	ret = rtk_send_rpc(krpc_ept_info, buf, len);
	if (ret < 0) {
		pr_err("[%s] send rpc failed\n", krpc_ept_info->name);
		mutex_unlock(&krpc_ept_info->send_mutex);
		return ret;
	}
	if (!wait_for_completion_timeout(&krpc_ept_info->ack, RPC_TIMEOUT)) {
		dev_err(dev, "kernel rpc timeout: %s...\n", krpc_ept_info->name);
		rtk_krpc_dump_ringbuf_info(krpc_ept_info);
		mutex_unlock(&krpc_ept_info->send_mutex);
		return -EINVAL;
	}
	mutex_unlock(&krpc_ept_info->send_mutex);

	return 0;
}

static int send_rpc(struct rtk_rpc_info *rpc_info, uint32_t command, uint32_t param1, uint32_t param2, uint32_t *retval)
{
	int ret = 0;
	char *buf;
	int len;

	if (!IS_ERR_OR_NULL(rpc_info->krpc_ept_info)) {
		buf = prepare_rpc_data(rpc_info->krpc_ept_info, command, param1, param2, &len);
		if (!IS_ERR(buf)) {
			ret = drm_send_rpc(rpc_info->dev, rpc_info->krpc_ept_info, buf, len, retval);
			kfree(buf);
		}
	}

	return ret;
}

int rpc_destroy_video_agent(struct rtk_rpc_info *rpc_info, u32 pinId)
{
	struct rpc_create_video_agent *rpc = NULL;
	struct rpc_result *retval;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_create_video_agent *)rpc_info->vaddr;
	offset = ALIGN(sizeof(rpc->instance), RPC_ALIGN_SZ);
	retval = (struct rpc_result *)((unsigned char *)rpc + offset);

	memset_io(rpc, 0, sizeof(*rpc));

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO)
		rpc->instance = pinId;
	else
		rpc->instance = htonl(pinId);

	if (send_rpc(rpc_info, ENUM_VIDEO_KERNEL_RPC_DESTROY,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if (retval->result != S_OK || rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_create_video_agent(struct rtk_rpc_info *rpc_info, u32 *videoId, u32 pinId)
{
	struct rpc_create_video_agent *rpc = NULL;
	struct rpc_result *retval;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_create_video_agent *)rpc_info->vaddr;
	offset = ALIGN(sizeof(rpc->instance), RPC_ALIGN_SZ);
	retval = (struct rpc_result *)((unsigned char *)rpc + offset);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO)
		rpc->instance = pinId;
	else
		rpc->instance = htonl(pinId);

#if IS_ENABLED(CONFIG_KERN_RPC_HANDLE_COMMAND)
	if (send_rpc_command_with_pid(opt, rpc_info->pid, ENUM_VIDEO_KERNEL_RPC_CREATE,
#else
	if (send_rpc(rpc_info, ENUM_VIDEO_KERNEL_RPC_CREATE,
#endif
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	if (opt == RPC_AUDIO) {
		retval->result = ntohl(retval->result);
		retval->data = ntohl(retval->data);
	}

	if (retval->result != S_OK || rpc_ret != S_OK)
		goto exit;

	*videoId = retval->data;
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_display(struct rtk_rpc_info *rpc_info,
		      struct rpc_vo_filter_display *argp)
{
	struct rpc_vo_filter_display *rpc = NULL;
	struct rpc_result *retval;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_vo_filter_display *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*argp), RPC_ALIGN_SZ);
	retval = (struct rpc_result *)((unsigned char *)rpc + offset);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		memcpy(rpc, argp, sizeof(*argp));
	} else {
		rpc->instance = htonl(argp->instance);
		rpc->videoPlane = htonl(argp->videoPlane);
		rpc->zeroBuffer = argp->zeroBuffer;
		rpc->realTimeSrc = argp->realTimeSrc;
	}
	if (send_rpc(rpc_info, ENUM_VIDEO_KERNEL_RPC_DISPLAY,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if (retval->result != S_OK || rpc_ret != S_OK)
		goto exit;
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_config_disp_win(struct rtk_rpc_info *rpc_info,
			      struct rpc_config_disp_win *argp)
{
	struct rpc_config_disp_win *rpc = NULL;
	struct rpc_result *retval;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_disp_win *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*argp), RPC_ALIGN_SZ);
	retval = (struct rpc_result *)((unsigned char *)rpc + offset);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		memcpy(rpc, argp, sizeof(*argp));
	} else {
		rpc->videoPlane = htonl(argp->videoPlane);
		rpc->videoWin.x = htons(argp->videoWin.x);
		rpc->videoWin.y = htons(argp->videoWin.y);
		rpc->videoWin.width = htons(argp->videoWin.width);
		rpc->videoWin.height = htons(argp->videoWin.height);
		rpc->borderWin.x = htons(argp->borderWin.x);
		rpc->borderWin.y = htons(argp->borderWin.y);
		rpc->borderWin.width = htons(argp->borderWin.width);
		rpc->borderWin.height = htons(argp->borderWin.height);
		rpc->borderColor.c1 = htons(argp->borderColor.c1);
		rpc->borderColor.c2 = htons(argp->borderColor.c2);
		rpc->borderColor.c3 = htons(argp->borderColor.c3);
		rpc->borderColor.isRGB = htons(argp->borderColor.isRGB);
		rpc->enBorder = argp->enBorder;
	}
	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_CONFIGUREDISPLAYWINDOW,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if (retval->result != S_OK || rpc_ret != S_OK)
		goto exit;
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_query_dis_win(struct rtk_rpc_info *rpc_info,
	struct rpc_query_disp_win_in *argp_in,
	struct rpc_query_disp_win_out *argp_out)
{
	struct rpc_query_disp_win_in *i_rpc = NULL;
	struct rpc_query_disp_win_out *o_rpc = NULL;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_disp_win_in *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*argp_in), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_query_disp_win_out *)((unsigned char *)i_rpc + offset);

	memset_io(i_rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO)
		i_rpc->plane = argp_in->plane;
	else
		i_rpc->plane = htonl(argp_in->plane);

	if (send_rpc(rpc_info,
		ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_WIN,
		rpc_info->paddr, rpc_info->paddr + offset,
		&rpc_ret)) {
		goto exit;
	}

	if (rpc_ret != S_OK)
		goto exit;

	if (opt != RPC_AUDIO) {
		memcpy(argp_out, o_rpc, sizeof(*o_rpc));
	} else {
		argp_out->plane = ntohl(o_rpc->plane);
		argp_out->configWin.x = ntohs(o_rpc->configWin.x);
		argp_out->configWin.y = ntohs(o_rpc->configWin.y);
		argp_out->configWin.width = ntohs(o_rpc->configWin.width);
		argp_out->configWin.height = ntohs(o_rpc->configWin.height);
		argp_out->contentWin.x = ntohs(o_rpc->contentWin.x);
		argp_out->contentWin.y = ntohs(o_rpc->contentWin.y);
		argp_out->contentWin.width = ntohs(o_rpc->contentWin.width);
		argp_out->contentWin.height = ntohs(o_rpc->contentWin.height);
		argp_out->mix1_size.w = ntohs(o_rpc->mix1_size.w);
		argp_out->mix1_size.h = ntohs(o_rpc->mix1_size.h);
	}
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_config_graphic(struct rtk_rpc_info *rpc_info,
	struct rpc_config_graphic_canvas *argp)
{
	struct rpc_config_graphic_canvas *rpc = NULL;
	struct rpc_result *retval;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_graphic_canvas *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*argp), RPC_ALIGN_SZ);
	retval = (struct rpc_result *)((unsigned char *)rpc + offset);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		memcpy(rpc, argp, sizeof(*argp));
	} else {
		rpc->plane = htonl(argp->plane);
		rpc->srcWin.x = htons(argp->srcWin.x);
		rpc->srcWin.y = htons(argp->srcWin.y);
		rpc->srcWin.width = htons(argp->srcWin.width);
		rpc->srcWin.height = htons(argp->srcWin.height);
		rpc->dispWin.x = htons(argp->dispWin.x);
		rpc->dispWin.y = htons(argp->dispWin.y);
		rpc->dispWin.width = htons(argp->dispWin.width);
		rpc->dispWin.height = htons(argp->dispWin.height);
		rpc->go = argp->go;
	}

	if (send_rpc(rpc_info,
		ENUM_VIDEO_KERNEL_RPC_CONFIGURE_GRAPHIC_CANVAS,
		rpc_info->paddr, rpc_info->paddr + offset,
		&rpc_ret)) {
		goto exit;
	}

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if (retval->result != S_OK || rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_set_refclock(struct rtk_rpc_info *rpc_info,
			   struct rpc_refclock *argp)
{
	struct rpc_refclock *rpc = NULL;
	struct rpc_result *retval;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_refclock *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*argp), RPC_ALIGN_SZ);
	retval = (struct rpc_result *)((unsigned char *)rpc + offset);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		memcpy(rpc, argp, sizeof(*argp));
	} else {
		rpc->instance = htonl(argp->instance);
		rpc->pRefClock = htonl(argp->pRefClock);
	}

	if (send_rpc(rpc_info, ENUM_VIDEO_KERNEL_RPC_SETREFCLOCK,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if (retval->result != S_OK || rpc_ret != S_OK)
		goto exit;
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_init_ringbuffer(struct rtk_rpc_info *rpc_info,
			      struct rpc_ringbuffer *argp)
{
	struct rpc_ringbuffer *rpc = NULL;
	struct rpc_result *retval;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_ringbuffer *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*argp), RPC_ALIGN_SZ);
	retval = (struct rpc_result *)((unsigned char *)rpc + offset);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		memcpy(rpc, argp, sizeof(*argp));
	} else {
		rpc->instance = htonl(argp->instance);
		rpc->readPtrIndex = htonl(argp->readPtrIndex);
		rpc->pinID = htonl(argp->pinID);
		rpc->pRINGBUFF_HEADER = htonl(argp->pRINGBUFF_HEADER);
	}

	if (send_rpc(rpc_info, ENUM_VIDEO_KERNEL_RPC_INITRINGBUFFER,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if (retval->result != S_OK || rpc_ret != S_OK)
		goto exit;
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_run_cmd(struct rtk_rpc_info *rpc_info, unsigned int instance,
			unsigned int command)
{
	unsigned int *rpc = NULL;
	struct rpc_result *retval;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (unsigned int *)rpc_info->vaddr;
	offset = ALIGN(sizeof(unsigned int), RPC_ALIGN_SZ);
	retval = (struct rpc_result *)((unsigned char *)rpc + offset);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO)
		*rpc = instance;
	else
		*rpc = htonl(instance);

	if (send_rpc(rpc_info, command,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (opt == RPC_AUDIO)
		retval->result = ntohl(retval->result);

	if (retval->result != S_OK || rpc_ret != S_OK)
		goto exit;
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_run(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	return rpc_video_run_cmd(rpc_info, instance, ENUM_VIDEO_KERNEL_RPC_RUN);
}
int rpc_video_pause(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	return rpc_video_run_cmd(rpc_info, instance, ENUM_VIDEO_KERNEL_RPC_PAUSE);
}

int rpc_video_stop(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	return rpc_video_run_cmd(rpc_info, instance, ENUM_VIDEO_KERNEL_RPC_STOP);
}

int rpc_video_flush(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	return rpc_video_run_cmd(rpc_info, instance, ENUM_VIDEO_KERNEL_RPC_FLUSH);
}

int rpc_video_destroy(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	return rpc_video_run_cmd(rpc_info, instance, ENUM_VIDEO_KERNEL_RPC_DESTROY);
}

int rpc_video_set_q_param(struct rtk_rpc_info *rpc_info,
			struct rpc_set_q_param *arg)
{
	struct rpc_set_q_param *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_q_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_set_q_param), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_Q_PARAMETER,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_config_channel_lowdelay(struct rtk_rpc_info *rpc_info, struct rpc_config_channel_lowdelay *arg)
{
	struct rpc_config_channel_lowdelay *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_channel_lowdelay *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_config_channel_lowdelay), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_CONFIGCHANNELLOWDELAY,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;

}

int rpc_video_privateinfo_param(struct rtk_rpc_info *rpc_info, struct rpc_privateinfo_param *arg)
{
	struct rpc_privateinfo_param *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_privateinfo_param), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;

}

int rpc_video_query_disp_win_new(struct rtk_rpc_info *rpc_info,
				struct rpc_query_disp_win_in *argp_in,
				struct rpc_query_disp_win_out_new *argp_out)
{
	struct rpc_query_disp_win_in *i_rpc = NULL;
	struct rpc_query_disp_win_out_new *o_rpc = NULL;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_disp_win_in *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*argp_in), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_query_disp_win_out_new *)((unsigned char *)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (opt != RPC_AUDIO)
		i_rpc->plane = argp_in->plane;
	else
		i_rpc->plane = htonl(argp_in->plane);

	if (send_rpc(rpc_info,
		ENUM_VIDEO_KERNEL_RPC_QUERYDISPLAYWINNEW,
		rpc_info->paddr, rpc_info->paddr + offset,
		&rpc_ret)) {
		goto exit;
	}

	if (rpc_ret != S_OK)
		goto exit;

	if (opt != RPC_AUDIO) {
		memcpy(argp_out, o_rpc, sizeof(*o_rpc));
	} else {
		argp_out->plane = ntohl(o_rpc->plane);
		argp_out->numWin = ntohs(o_rpc->numWin);
		argp_out->zOrder = ntohs(o_rpc->zOrder);
		argp_out->configWin.x = ntohs(o_rpc->configWin.x);
		argp_out->configWin.y = ntohs(o_rpc->configWin.y);
		argp_out->configWin.width = ntohs(o_rpc->configWin.width);
		argp_out->configWin.height = ntohs(o_rpc->configWin.height);
		argp_out->contentWin.x = ntohs(o_rpc->contentWin.x);
		argp_out->contentWin.y = ntohs(o_rpc->contentWin.y);
		argp_out->contentWin.width = ntohs(o_rpc->contentWin.width);
		argp_out->contentWin.height = ntohs(o_rpc->contentWin.height);
		argp_out->deintMode = ntohs(o_rpc->deintMode);
		argp_out->pitch = ntohs(o_rpc->pitch);
		argp_out->colorType = ntohl(o_rpc->colorType);
		argp_out->RGBOrder = ntohl(o_rpc->RGBOrder);
		argp_out->format3D = ntohl(o_rpc->format3D);
		argp_out->mix1_size.w = ntohs(o_rpc->mix1_size.w);
		argp_out->mix1_size.h = ntohs(o_rpc->mix1_size.h);
		argp_out->standard = ntohl(o_rpc->standard);
		argp_out->enProg = o_rpc->enProg;
		argp_out->cvbs_off = o_rpc->cvbs_off;
		argp_out->srcZoomWin.x = ntohs(o_rpc->srcZoomWin.x);
		argp_out->srcZoomWin.y = ntohs(o_rpc->srcZoomWin.y);
		argp_out->srcZoomWin.width = ntohs(o_rpc->srcZoomWin.width);
		argp_out->srcZoomWin.height = ntohs(o_rpc->srcZoomWin.height);
		argp_out->mix2_size.w = ntohs(o_rpc->mix2_size.w);
		argp_out->mix2_size.h = ntohs(o_rpc->mix2_size.h);
		argp_out->mixdd_size.w = ntohs(o_rpc->mixdd_size.w);
		argp_out->mixdd_size.h = ntohs(o_rpc->mixdd_size.h);
		argp_out->wb_usedFormat = ntohl(o_rpc->wb_usedFormat);
		argp_out->channel_total_drop_rpc = ntohl(o_rpc->channel_total_drop_rpc);
		argp_out->channel_total_drop_rpc_anycase = ntohl(o_rpc->channel_total_drop_rpc_anycase);
		argp_out->is_swmixer_on = ntohl(o_rpc->is_swmixer_on);
		argp_out->is_on_mixer = ntohl(o_rpc->is_on_mixer);
	}
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_set_speed(struct rtk_rpc_info *rpc_info, struct rpc_set_speed *arg)
{
	struct rpc_set_speed *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_speed *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_set_speed), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_SETSPEED,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_set_background(struct rtk_rpc_info *rpc_info,
			struct rpc_set_background *arg)
{
	struct rpc_set_background *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_background *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {
		// fix me simon
		rpc->bgColor.c1 = ntohs(arg->bgColor.c1);
		rpc->bgColor.c2 = ntohs(arg->bgColor.c2);
		rpc->bgColor.c3 = ntohs(arg->bgColor.c3);
		rpc->bgColor.isRGB = ntohs(arg->bgColor.isRGB);
		rpc->bgEnable = arg->bgEnable;
	}
	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_SETBACKGROUND,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_keep_curpic(struct rtk_rpc_info *rpc_info, struct rpc_keep_curpic *arg)
{
	struct rpc_keep_curpic *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_keep_curpic *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_keep_curpic), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_KEEPCURPIC,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_keep_curpic_fw(struct rtk_rpc_info *rpc_info, struct rpc_keep_curpic *arg)
{
	struct rpc_keep_curpic *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_keep_curpic *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_keep_curpic), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_KEEPCURPIC_FW_MALLOC,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_keep_curpic_svp(struct rtk_rpc_info *rpc_info, struct rpc_keep_curpic_svp *arg)
{
	struct rpc_keep_curpic_svp *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_keep_curpic_svp *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_keep_curpic_svp), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_KEEPCURPICSVP,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_set_deintflag(struct rtk_rpc_info *rpc_info, struct rpc_set_deintflag *arg)
{
	struct rpc_set_deintflag *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_deintflag *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_set_deintflag), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_SET_DEINTFLAG,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_create_graphic_win(struct rtk_rpc_info *rpc_info, struct rpc_create_graphic_win *arg)
{
	struct rpc_create_graphic_win *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_create_graphic_win *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {
		rpc->plane = ntohl(arg->plane);
		rpc->winPos.x = ntohs(arg->winPos.x);
		rpc->winPos.y = ntohs(arg->winPos.y);
		rpc->winPos.width = ntohs(arg->winPos.width);
		rpc->winPos.height = ntohs(arg->winPos.height);
		rpc->colorFmt = ntohl(arg->colorFmt);
		rpc->rgbOrder = ntohl(arg->rgbOrder);
		rpc->colorKey = ntohl(arg->colorKey);
		rpc->alpha = arg->alpha;
	}

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_CREATEGRAPHICWINDOW,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_draw_graphic_win(struct rtk_rpc_info *rpc_info, struct rpc_draw_graphic_win *arg)
{
	struct rpc_draw_graphic_win *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int i;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_draw_graphic_win *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {
		rpc->plane = ntohl(arg->plane);
		rpc->winID = ntohs(arg->winID);
		rpc->storageMode = ntohl(arg->storageMode);
		rpc->paletteIndex = arg->paletteIndex;
		rpc->compressed = arg->compressed;
		rpc->interlace_Frame = arg->interlace_Frame;
		rpc->bottomField = arg->bottomField;
		for (i = 0; i < 4; i++) {
			rpc->startX[i] = ntohs(arg->startX[i]);
			rpc->startY[i] = ntohs(arg->startY[i]);
			rpc->imgPitch[i] = ntohs(arg->imgPitch[i]);
			rpc->pImage[i] = ntohl(arg->pImage[i]);
		}
		rpc->go = arg->go;
	}

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_DRAWGRAPHICWINDOW,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_modify_graphic_win(struct rtk_rpc_info *rpc_info, struct rpc_modify_graphic_win *arg)
{
	struct rpc_modify_graphic_win *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1, i;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_modify_graphic_win *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {
		rpc->plane = ntohl(arg->plane);
		rpc->winID = arg->winID;
		rpc->reqMask = arg->reqMask;
		rpc->winPos.x = ntohs(arg->winPos.x);
		rpc->winPos.y = ntohs(arg->winPos.y);
		rpc->winPos.width = ntohs(arg->winPos.width);
		rpc->winPos.height = ntohs(arg->winPos.height);
		rpc->colorFmt = ntohl(arg->colorFmt);
		rpc->rgbOrder = ntohl(arg->rgbOrder);
		rpc->colorKey = ntohl(arg->colorKey);
		rpc->alpha = arg->alpha;
		rpc->storageMode = ntohl(arg->storageMode);
		rpc->paletteIndex = arg->paletteIndex;
		rpc->compressed = arg->compressed;
		rpc->interlace_Frame = arg->interlace_Frame;
		rpc->bottomField = arg->bottomField;
		for (i = 0; i < 4; i++) {
			rpc->startX[i] = ntohs(arg->startX[i]);
			rpc->startY[i] = ntohs(arg->startY[i]);
			rpc->imgPitch[i] = ntohs(arg->imgPitch[i]);
			rpc->pImage[i] = ntohl(arg->pImage[i]);
		}
		rpc->go = arg->go;
	}

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_MODIFYGRAPHICWINDOW,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_delete_graphic_win(struct rtk_rpc_info *rpc_info, struct rpc_delete_graphic_win *arg)
{
	struct rpc_delete_graphic_win *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_delete_graphic_win *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {
		rpc->plane = ntohl(arg->plane);
		rpc->winID = ntohs(arg->winID);
		rpc->go = arg->go;
	}

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_DELETEGRAPHICWINDOW,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_config_osd_palette(struct rtk_rpc_info *rpc_info, struct rpc_config_osd_palette *arg)
{
	struct rpc_config_osd_palette *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_osd_palette *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {
		rpc->paletteIndex = arg->paletteIndex;
		rpc->pPalette = ntohl(arg->pPalette);
	}

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_CONFIGUREOSDPALETTE,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_config_plane_mixer(struct rtk_rpc_info *rpc_info, struct rpc_config_plane_mixer *arg)
{
	struct rpc_config_plane_mixer *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1, i;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_plane_mixer *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {
		rpc->instanceId = ntohl(arg->instanceId);
		rpc->targetPlane = ntohl(arg->targetPlane);
		for (i = 0; i < 8; i++) {
			rpc->mixOrder[i] = ntohl(arg->mixOrder[i]);
			rpc->win[i].winID = ntohl(arg->win[i].winID);
			rpc->win[i].opacity = ntohs(arg->win[i].opacity);
			rpc->win[i].alpha = ntohs(arg->win[i].alpha);
		}
		rpc->dataIn0 = ntohl(arg->dataIn0);
		rpc->dataIn1 = ntohl(arg->dataIn1);
		rpc->dataIn2 = ntohl(arg->dataIn2);
	}

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PMIXER_CONFIGUREPLANEMIXER,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_set_sdrflag(struct rtk_rpc_info *rpc_info, struct rpc_set_sdrflag *arg)
{
	struct rpc_set_sdrflag *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;
	int opt;
	uint32_t type;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_sdrflag *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	type = (opt != RPC_AUDIO) ? 1:0;

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_set_sdrflag), type);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_SET_ENHANCEDSDR,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_set_display_ratio(struct rtk_rpc_info *rpc_info, u32 ratio)
{
	struct rpc_privateinfo_param *i_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret;
	int opt;

	if (ratio == 0 || ratio > 100)
		return -EINVAL;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (opt != RPC_AUDIO) {
		i_rpc->instanceId = 0;
		i_rpc->type = ENUM_PRIVATEINFO_VIDEO_DISPLAY_RATIO;
		i_rpc->privateInfo[0] = ratio;
	} else {
		i_rpc->instanceId = htonl(0);
		i_rpc->type = htonl(ENUM_PRIVATEINFO_VIDEO_DISPLAY_RATIO);
		i_rpc->privateInfo[0] = htonl(ratio);
	}

	ret = send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	mutex_unlock(&rpc_info->lock);

	return ret;
}

int rpc_set_spd_infoframe(struct rtk_rpc_info *rpc_info,
			unsigned int enable, char *vendor_str, char *product_str,
			unsigned int sdi)
{
	struct rpc_privateinfo_param *i_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	char vendor[8];
	char product[16];
	int ret;
	int opt;

	memset_io(vendor, 0, sizeof(vendor));
	memset_io(product, 0, sizeof(product));
	memcpy(vendor, vendor_str, min(sizeof(vendor), strlen(vendor_str)));
	memcpy(product, product_str, min(sizeof(product), strlen(product_str)));

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (opt != RPC_AUDIO) {
		i_rpc->instanceId = 0;
		i_rpc->type = ENUM_PRIVATEINFO_HDMI_SPDINFOFRAME_ENABLE;
		i_rpc->privateInfo[0] = enable;

		i_rpc->privateInfo[1] = (vendor[0] << 24) | (vendor[1] << 16) | (vendor[2] << 8) | (vendor[3]);
		i_rpc->privateInfo[2] = (vendor[4] << 24) | (vendor[5] << 16) | (vendor[6] << 8) | (vendor[7]);

		i_rpc->privateInfo[3] = (product[0] << 24) | (product[1] << 16) | (product[2] << 8) | (product[3]);
		i_rpc->privateInfo[4] = (product[4] << 24) | (product[5] << 16) | (product[6] << 8) | (product[7]);
		i_rpc->privateInfo[5] = (product[8] << 24) | (product[9] << 16) | (product[10] << 8) | (product[11]);
		i_rpc->privateInfo[6] = (product[12] << 24) | (product[13] << 16) | (product[14] << 8) | (product[15]);

		i_rpc->privateInfo[7] =  sdi;
	} else {
		i_rpc->instanceId = htonl(0);
		i_rpc->type = htonl(ENUM_PRIVATEINFO_HDMI_SPDINFOFRAME_ENABLE);
		i_rpc->privateInfo[0] = htonl(enable);

		i_rpc->privateInfo[1] = (vendor[3] << 24) | (vendor[2] << 16) | (vendor[1] << 8) | (vendor[0]);
		i_rpc->privateInfo[2] = (vendor[7] << 24) | (vendor[6] << 16) | (vendor[5] << 8) | (vendor[4]);

		i_rpc->privateInfo[3] = (product[3] << 24) | (product[2] << 16) | (product[1] << 8) | (product[0]);
		i_rpc->privateInfo[4] = (product[7] << 24) | (product[6] << 16) | (product[5] << 8) | (product[4]);
		i_rpc->privateInfo[5] = (product[11] << 24) | (product[10] << 16) | (product[9] << 8) | (product[8]);
		i_rpc->privateInfo[6] = (product[15] << 24) | (product[14] << 16) | (product[13] << 8) | (product[12]);

		i_rpc->privateInfo[7] =  htonl(sdi);
	}

	ret = send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	mutex_unlock(&rpc_info->lock);

	return ret;
}

int rpc_set_infoframe(struct rtk_rpc_info *rpc_info, unsigned int flag,
	unsigned int content_type, unsigned int scan_mode)
{
	struct rpc_privateinfo_param *i_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (opt != RPC_AUDIO) {
		i_rpc->instanceId = 0;
		i_rpc->type = ENUM_PRIVATEINFO_VIDEO_SET_HDMI_AVIINFOFRAME;
		i_rpc->privateInfo[0] = flag;
		i_rpc->privateInfo[1] = content_type;
		i_rpc->privateInfo[2] = scan_mode;
	} else {
		i_rpc->instanceId = htonl(0);
		i_rpc->type = htonl(ENUM_PRIVATEINFO_VIDEO_SET_HDMI_AVIINFOFRAME);
		i_rpc->privateInfo[0] = htonl(flag);
		i_rpc->privateInfo[1] = htonl(content_type);
		i_rpc->privateInfo[2] = htonl(scan_mode);
	}

	ret = send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	mutex_unlock(&rpc_info->lock);

	return ret;
}

int rpc_send_edid_raw_data(struct rtk_rpc_info *rpc_info,
			u8 *edid_data, u32 edid_size)
{
	int ret;
	u32 rpc_ret;
	struct rpc_vout_edid_raw_data *rpc;
	unsigned int offset;
	unsigned long edid_offset;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_vout_edid_raw_data *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_vout_edid_raw_data), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;
	edid_offset = offset * 2;
	memset_io(rpc, 0, sizeof(*rpc));

	memcpy(rpc_info->vaddr + edid_offset, edid_data, edid_size);

	if (opt != RPC_AUDIO) {
		rpc->paddr = rpc_info->paddr + edid_offset;
		rpc->size = edid_size;
	} else {
		rpc->paddr = htonl(rpc_info->paddr + edid_offset);
		rpc->size = htonl(edid_size);
	}

	ret = send_rpc(rpc_info,
			ENUM_KERNEL_RPC_HDMI_EDID_RAW_DATA,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);
	if (ret)
		goto exit;

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_set_hdmi_audio_onoff(struct rtk_rpc_info *rpc_info_ao,
			 struct rpc_audio_ctrl_data *arg)
{
	struct rpc_audio_ctrl_data *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret;

	mutex_lock(&rpc_info_ao->lock);

	if (rpc_info_ao->ao_in_hifi == NULL) {
		ret = -ENXIO;
		goto exit;
	}

	rpc = (struct rpc_audio_ctrl_data *)rpc_info_ao->vaddr;
	offset = ALIGN(sizeof(struct rpc_audio_ctrl_data), RPC_ALIGN_SZ);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (*rpc_info_ao->ao_in_hifi) {
		rpc->version = arg->version;
		rpc->hdmi_en_state = arg->hdmi_en_state;
	} else {
		rpc->version = htonl(arg->version);
		rpc->hdmi_en_state = htonl(arg->hdmi_en_state);
	}

#ifdef CONFIG_CHROME_PLATFORMS
	ret = send_rpc(rpc_info_ao, ENUM_KERNEL_RPC_HDMI_OUT_EDID2,
			rpc_info_ao->paddr, rpc_info_ao->paddr + offset,
			&rpc_ret);
#else
	ret = send_rpc(rpc_info_ao, ENUM_KERNEL_RPC_HDMI_AO_ONOFF,
			rpc_info_ao->paddr, rpc_info_ao->paddr + offset,
			&rpc_ret);
#endif

	if (ret)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info_ao->lock);
	return ret;
}

int rpc_send_hdmi_freq(struct rtk_rpc_info *rpc_info_ao,
			 struct rpc_audio_hdmi_freq *arg)
{
	struct rpc_audio_hdmi_freq *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret;

	mutex_lock(&rpc_info_ao->lock);

	if (rpc_info_ao->ao_in_hifi == NULL) {
		ret = -ENXIO;
		goto exit;
	}

	rpc = (struct rpc_audio_hdmi_freq *)rpc_info_ao->vaddr;
	offset = ALIGN(sizeof(struct rpc_audio_hdmi_freq), RPC_ALIGN_SZ);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (*rpc_info_ao->ao_in_hifi) {
		rpc->tmds_freq = arg->tmds_freq;
	} else {
		rpc->tmds_freq = htonl(arg->tmds_freq);
	}

	ret = send_rpc(rpc_info_ao, ENUM_KERNEL_RPC_HDMI_SET,
			rpc_info_ao->paddr, rpc_info_ao->paddr + offset,
			&rpc_ret);
exit:
	mutex_unlock(&rpc_info_ao->lock);
	return ret;
}

int rpc_set_vrr(struct rtk_rpc_info *rpc_info,
			struct rpc_vout_hdmi_vrr *arg)
{
	struct rpc_vout_hdmi_vrr *rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO, i;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_vout_hdmi_vrr *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {
		rpc->vrr_function = htonl(arg->vrr_function);
		rpc->vrr_act = htonl(arg->vrr_act);

		for (i = 0; i < 15; i++)
			rpc->reserved[i] = htonl(arg->reserved[i]);
	}
	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_SET_HDMI_VRR,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_query_out_interface(struct rtk_rpc_info *rpc_info,
			    struct rpc_set_display_out_interface *arg)
{
	struct rpc_set_display_out_interface *i_rpc = NULL;
	struct rpc_set_display_out_interface *o_rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_set_display_out_interface *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_set_display_out_interface *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO)
		i_rpc->display_interface = arg->display_interface;
	else
		i_rpc->display_interface = htonl(arg->display_interface);

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_GET_DISPLAY_OUTPUT_INTERFACE,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	if (opt != RPC_AUDIO) {
		memcpy(arg, o_rpc, sizeof(*o_rpc));
	} else {
		arg->cmd_version = htonl(o_rpc->cmd_version);
		arg->display_interface = htonl(o_rpc->display_interface);
		arg->width = htonl(o_rpc->width);
		arg->height = htonl(o_rpc->height);
		arg->frame_rate = htonl(o_rpc->frame_rate);
		arg->hdr_mode = htonl(o_rpc->hdr_mode);
		arg->display_interface_mixer = htonl(o_rpc->display_interface_mixer);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_set_out_interface(struct rtk_rpc_info *rpc_info,
			    struct rpc_set_display_out_interface *arg)
{
	struct rpc_set_display_out_interface *rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_display_out_interface *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		rpc->display_interface       = arg->display_interface;
		rpc->width                   = arg->width;
		rpc->height                  = arg->height;
		rpc->frame_rate              = arg->frame_rate;
		rpc->display_interface_mixer = arg->display_interface_mixer;
	} else {
		rpc->display_interface       = htonl(arg->display_interface);
		rpc->width                   = htonl(arg->width);
		rpc->height                  = htonl(arg->height);
		rpc->frame_rate              = htonl(arg->frame_rate);
		rpc->display_interface_mixer = htonl(arg->display_interface_mixer);
	}

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_SET_DISPLAY_OUTPUT_INTERFACE,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret)) {
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_hw_init_out_interface(struct rtk_rpc_info *rpc_info,
			    struct rpc_hw_init_display_out_interface *arg)
{
	struct rpc_hw_init_display_out_interface *rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO;
	int opt;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_hw_init_display_out_interface *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		rpc->display_interface = arg->display_interface;
		rpc->enable            = arg->enable;
		rpc->assr_en           = arg->assr_en;
		rpc->hactive           = arg->hactive;
		rpc->hfront_porch      = arg->hfront_porch;
		rpc->hback_porch       = arg->hback_porch;
		rpc->hsync_len         = arg->hsync_len;
		rpc->vactive           = arg->vactive;
		rpc->vfront_porch      = arg->vfront_porch;
		rpc->vback_porch       = arg->vback_porch;
		rpc->vsync_len         = arg->vsync_len;
		rpc->frame_rate        = arg->frame_rate;
		rpc->pixel_clock       = arg->pixel_clock;
		rpc->is_positive_vsync = arg->is_positive_vsync;
		rpc->is_positive_hsync = arg->is_positive_hsync;
		rpc->link_rate         = arg->link_rate;
		rpc->lane_count        = arg->lane_count;
		rpc->bpc               = arg->bpc;
		rpc->is_flipped        = arg->is_flipped;
	} else {
		rpc->display_interface = htonl(arg->display_interface);
		rpc->enable            = htonl(arg->enable);
		rpc->assr_en           = htonl(arg->assr_en);
		rpc->hactive           = htonl(arg->hactive);
		rpc->hfront_porch      = htonl(arg->hfront_porch);
		rpc->hback_porch       = htonl(arg->hback_porch);
		rpc->hsync_len         = htonl(arg->hsync_len);
		rpc->vactive           = htonl(arg->vactive);
		rpc->vfront_porch      = htonl(arg->vfront_porch);
		rpc->vback_porch       = htonl(arg->vback_porch);
		rpc->vsync_len         = htonl(arg->vsync_len);
		rpc->frame_rate        = htonl(arg->frame_rate);
		rpc->pixel_clock       = htonl(arg->pixel_clock);
		rpc->is_positive_vsync = htonl(arg->is_positive_vsync);
		rpc->is_positive_hsync = htonl(arg->is_positive_hsync);
		rpc->link_rate         = htonl(arg->link_rate);
		rpc->lane_count        = htonl(arg->lane_count);
		rpc->bpc               = htonl(arg->bpc);
		rpc->is_flipped        = htonl(arg->is_flipped);
	}

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_HW_INIT_DISPLAY_OUTPUT_INTERFACE,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret)) {
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_query_out_interface_timing(struct rtk_rpc_info *rpc_info,
			    struct rpc_query_display_out_interface_timing *arg)
{
	struct rpc_query_display_out_interface_timing *i_rpc = NULL;
	struct rpc_query_display_out_interface_timing *o_rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_display_out_interface_timing *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_query_display_out_interface_timing *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		i_rpc->display_interface = arg->display_interface;
	} else {
		i_rpc->display_interface = htonl(arg->display_interface);
	}

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_OUTPUT_INTERFACE_TIMING,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret)) {
		goto exit;
	}

	if (opt != RPC_AUDIO) {
		arg->clock       = o_rpc->clock;
		arg->hdisplay    = o_rpc->hdisplay;
		arg->hsync_start = o_rpc->hsync_start;
		arg->hsync_end   = o_rpc->hsync_end;
		arg->htotal      = o_rpc->htotal;
		arg->vdisplay    = o_rpc->vdisplay;
		arg->vsync_start = o_rpc->vsync_start;
		arg->vsync_end   = o_rpc->vsync_end;
		arg->vtotal      = o_rpc->vtotal;
		arg->framerate   = o_rpc->framerate;
		arg->mixer       = o_rpc->mixer;
	} else {
		arg->clock       = htonl(o_rpc->clock);
		arg->hdisplay    = htonl(o_rpc->hdisplay);
		arg->hsync_start = htonl(o_rpc->hsync_start);
		arg->hsync_end   = htonl(o_rpc->hsync_end);
		arg->htotal      = htonl(o_rpc->htotal);
		arg->vdisplay    = htonl(o_rpc->vdisplay);
		arg->vsync_start = htonl(o_rpc->vsync_start);
		arg->vsync_end   = htonl(o_rpc->vsync_end);
		arg->vtotal      = htonl(o_rpc->vtotal);
		arg->framerate   = htonl(o_rpc->framerate);
		arg->mixer       = htonl(o_rpc->mixer);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_query_display_panel_usage(struct rtk_rpc_info *rpc_info,
			    struct rpc_query_display_panel_usage *arg)
{
	struct rpc_query_display_panel_usage *i_rpc = NULL;
	struct rpc_query_display_panel_usage *o_rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_display_panel_usage *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_query_display_panel_usage *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		i_rpc->display_interface_mixer = arg->display_interface_mixer;
	} else {
		i_rpc->display_interface_mixer = htonl(arg->display_interface_mixer);
	}

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_QUERY_PANNEL_DISPLAY_TYPE,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret)) {
		goto exit;
	}

	if (opt != RPC_AUDIO) {
		arg->display_panel_usage = o_rpc->display_panel_usage;
	} else {
		arg->display_panel_usage = htonl(o_rpc->display_panel_usage);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_query_panel_usage_pos(struct rtk_rpc_info *rpc_info,
			    struct rpc_query_panel_usage_pos *arg)
{
	struct rpc_query_panel_usage_pos *i_rpc = NULL;
	struct rpc_query_panel_usage_pos *o_rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_panel_usage_pos *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_query_panel_usage_pos *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		i_rpc->display_panel_usage = arg->display_panel_usage;
	} else {
		i_rpc->display_panel_usage = htonl(arg->display_panel_usage);
	}

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_QUERY_PANEL_USAGE_POSITION,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret)) {
		goto exit;
	}

	if (opt != RPC_AUDIO) {
		arg->x = o_rpc->x;
		arg->y = o_rpc->y;
	} else {
		arg->x = htonl(o_rpc->x);
		arg->y = htonl(o_rpc->y);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_query_panel_cluster_size(struct rtk_rpc_info *rpc_info,
			    struct rpc_query_panel_cluster_size *arg)
{
	struct rpc_query_panel_cluster_size *i_rpc = NULL;
	struct rpc_query_panel_cluster_size *o_rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_panel_cluster_size *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_query_panel_cluster_size *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_QUERY_CLUSTER_SIZE,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret)) {
		goto exit;
	}

	if (opt != RPC_AUDIO) {
		arg->width = o_rpc->width;
		arg->height = o_rpc->height;
	} else {
		arg->width = htonl(o_rpc->width);
		arg->height = htonl(o_rpc->height);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_query_mixer_by_plane(struct rtk_rpc_info *rpc_info,
				struct rpc_query_mixer_by_plane_in *argp_in,
				struct rpc_query_mixer_by_plane_out *argp_out)
{
	struct rpc_query_mixer_by_plane_in *i_rpc = NULL;
	struct rpc_query_mixer_by_plane_out *o_rpc = NULL;
	unsigned int offset, rpc_ret;
	int ret = -1;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_mixer_by_plane_in *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*argp_in), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_query_mixer_by_plane_out *)((unsigned char *)i_rpc + offset);

	memset_io(i_rpc, 0, RPC_CMD_BUFFER_SIZE);

	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		i_rpc->video_plane = argp_in->video_plane;
		i_rpc->config_no = argp_in->config_no;
	} else {
		i_rpc->video_plane = htonl(argp_in->video_plane);
		i_rpc->config_no = htonl(argp_in->config_no);
	}

	if (send_rpc(rpc_info,
		ENUM_VIDEO_KERNEL_RPC_QUERY_PLANE_WINDOW,
		rpc_info->paddr, rpc_info->paddr + offset,
		&rpc_ret)) {
		goto exit;
	}

	if (rpc_ret != S_OK)
		goto exit;

	if (opt != RPC_AUDIO) {
		argp_out->mixer = o_rpc->mixer;
	} else {
		argp_out->mixer = ntohl(o_rpc->mixer);
	}
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_query_tv_system(struct rtk_rpc_info *rpc_info,
			struct rpc_config_tv_system *arg)
{
	struct rpc_config_tv_system *i_rpc = NULL;
	struct rpc_config_tv_system *o_rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO, i;
	int opt;

	mutex_lock(&rpc_info->lock);

	if (rpc_info->hdmi_new_mac == NULL) {
		ret = -ENXIO;
		goto exit;
	}

	if (*rpc_info->hdmi_new_mac) {
		ret = -EPERM;
		goto exit;
	}

	i_rpc = (struct rpc_config_tv_system *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_config_tv_system *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_QUERY_CONFIG_TV_SYSTEM,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	for (i = 0; i < sizeof(struct rpc_config_tv_system); i++)
		((char *)arg)[i] = ((char *)o_rpc)[i];

	if (opt != RPC_AUDIO) {
		memcpy(arg, o_rpc, sizeof(*o_rpc));
	} else {
		arg->interfaceType = htonl(arg->interfaceType);
		arg->videoInfo.standard = htonl(arg->videoInfo.standard);
		arg->videoInfo.pedType  = htonl(arg->videoInfo.pedType);
		arg->videoInfo.dataInt0 = htonl(arg->videoInfo.dataInt0);
		arg->videoInfo.dataInt1 = htonl(arg->videoInfo.dataInt1);

		arg->info_frame.hdmiMode  = htonl(arg->info_frame.hdmiMode);
		arg->info_frame.audioSampleFreq = htonl(arg->info_frame.audioSampleFreq);
		arg->info_frame.dataInt0  = htonl(arg->info_frame.dataInt0);
		arg->info_frame.hdmi2px_feature = htonl(arg->info_frame.hdmi2px_feature);
		arg->info_frame.hdmi_off_mode = htonl(arg->info_frame.hdmi_off_mode);
		arg->info_frame.hdr_ctrl_mode = htonl(arg->info_frame.hdr_ctrl_mode);
		arg->info_frame.reserved4 = htonl(arg->info_frame.reserved4);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_set_tv_system(struct rtk_rpc_info *rpc_info,
			 struct rpc_config_tv_system *arg)
{
	struct rpc_config_tv_system *rpc = NULL;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO, i;
	int opt;

	mutex_lock(&rpc_info->lock);

	if (rpc_info->hdmi_new_mac == NULL) {
		ret = -ENXIO;
		goto exit;
	}

	if (*rpc_info->hdmi_new_mac) {
		ret = -EPERM;
		goto exit;
	}

	rpc = (struct rpc_config_tv_system *)rpc_info->vaddr;
	offset = ALIGN(sizeof(*arg), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	if (opt != RPC_AUDIO) {
		memcpy(rpc, arg, sizeof(*arg));
	} else {

		for (i = 0; i < sizeof(struct rpc_config_tv_system); i++)
			((char *)rpc)[i] = ((char *)arg)[i];

		rpc->interfaceType = htonl(arg->interfaceType);
		rpc->videoInfo.standard = htonl(arg->videoInfo.standard);
		rpc->videoInfo.pedType  = htonl(arg->videoInfo.pedType);
		rpc->videoInfo.dataInt0 = htonl(arg->videoInfo.dataInt0);
		rpc->videoInfo.dataInt1 = htonl(arg->videoInfo.dataInt1);

		rpc->info_frame.hdmiMode  = htonl(arg->info_frame.hdmiMode);
		rpc->info_frame.audioSampleFreq = htonl(arg->info_frame.audioSampleFreq);
		rpc->info_frame.dataInt0  = htonl(arg->info_frame.dataInt0);
		rpc->info_frame.hdmi2px_feature = htonl(arg->info_frame.hdmi2px_feature);
		rpc->info_frame.hdmi_off_mode = htonl(arg->info_frame.hdmi_off_mode);
		rpc->info_frame.hdr_ctrl_mode = htonl(arg->info_frame.hdr_ctrl_mode);
		rpc->info_frame.reserved4 = htonl(arg->info_frame.reserved4);
	}

	if (send_rpc(rpc_info,
			     ENUM_VIDEO_KERNEL_RPC_CONFIG_TV_SYSTEM,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_get_display_format(struct rtk_rpc_info *rpc_info,
			struct rpc_display_output_format *output_fmt)
{
	struct rpc_display_output_format *i_rpc;
	struct rpc_display_output_format *o_rpc;
	unsigned int offset;
	unsigned int rpc_ret = 0;
	int ret;
	int opt;

	mutex_lock(&rpc_info->lock);

	if (rpc_info->hdmi_new_mac == NULL) {
		ret = -ENXIO;
		goto exit;
	}

	if (!*rpc_info->hdmi_new_mac) {
		ret = -EPERM;
		goto exit;
	}

	i_rpc = (struct rpc_display_output_format *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_config_tv_system), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_display_output_format *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_GET_DISPLAY_OUTPUT_FORMAT,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret)) {
		ret = -EIO;
		goto exit;
	}

	if (rpc_ret != FW_RETURN_SUCCESS) {
		ret = -ENOEXEC;
		goto exit;
	}

	if (opt != RPC_AUDIO) {
		memcpy(output_fmt, o_rpc, sizeof(*o_rpc));
	} else {
		output_fmt->cmd_version = htonl(o_rpc->cmd_version);
		output_fmt->display_mode = htonl(o_rpc->display_mode);
		output_fmt->vic = htonl(o_rpc->vic);
		output_fmt->clock = htonl(o_rpc->clock);
		output_fmt->is_fractional_fps = htonl(o_rpc->is_fractional_fps);
		output_fmt->colorspace = htonl(o_rpc->colorspace);
		output_fmt->color_depth = htonl(o_rpc->color_depth);
		output_fmt->tmds_config = htonl(o_rpc->tmds_config);
		output_fmt->hdr_mode = htonl(o_rpc->hdr_mode);
		memcpy(&output_fmt->avi_infoframe, &o_rpc->avi_infoframe,
			sizeof(struct rtk_infoframe_packet));
		output_fmt->src_3d_fmt = htonl(o_rpc->src_3d_fmt);
		output_fmt->dst_3d_fmt = htonl(o_rpc->dst_3d_fmt);
		output_fmt->en_dithering = htonl(o_rpc->en_dithering);
		output_fmt->quick_dv_switch = htonl(o_rpc->quick_dv_switch);
		output_fmt->is_fw_skip_set_hdmi_phy = htonl(o_rpc->is_fw_skip_set_hdmi_phy);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;

}

int rpc_set_display_format(struct rtk_rpc_info *rpc_info,
			struct rpc_display_output_format *output_fmt)
{
	struct rpc_display_output_format *i_rpc;
	unsigned int offset;
	unsigned int rpc_ret = 0;
	int ret;
	int opt;

	mutex_lock(&rpc_info->lock);

	if (rpc_info->hdmi_new_mac == NULL) {
		ret = -ENXIO;
		goto exit;
	}

	if (!*rpc_info->hdmi_new_mac) {
		ret = -EPERM;
		goto exit;
	}

	i_rpc = (struct rpc_display_output_format *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_config_tv_system), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (opt != RPC_AUDIO) {
		memcpy(i_rpc, output_fmt, sizeof(*output_fmt));
	} else {
		i_rpc->cmd_version = htonl(output_fmt->cmd_version);
		i_rpc->display_mode = htonl(output_fmt->display_mode);
		i_rpc->vic = htonl(output_fmt->vic);
		i_rpc->clock = htonl(output_fmt->clock);
		i_rpc->is_fractional_fps = htonl(output_fmt->is_fractional_fps);
		i_rpc->colorspace = htonl(output_fmt->colorspace);
		i_rpc->color_depth = htonl(output_fmt->color_depth);
		i_rpc->tmds_config = htonl(output_fmt->tmds_config);
		i_rpc->hdr_mode = htonl(output_fmt->hdr_mode);
		memcpy(&i_rpc->avi_infoframe, &output_fmt->avi_infoframe,
			sizeof(struct rtk_infoframe_packet));
		i_rpc->src_3d_fmt = htonl(output_fmt->src_3d_fmt);
		i_rpc->dst_3d_fmt = htonl(output_fmt->dst_3d_fmt);
		i_rpc->en_dithering = htonl(output_fmt->en_dithering);
		i_rpc->quick_dv_switch = htonl(output_fmt->quick_dv_switch);
		i_rpc->is_fw_skip_set_hdmi_phy = htonl(output_fmt->is_fw_skip_set_hdmi_phy);
	}

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_SET_DISPLAY_OUTPUT_FORMAT,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret)) {
		ret = -EIO;
		goto exit;
	}

	if (rpc_ret != FW_RETURN_SUCCESS) {
		ret = -ENOEXEC;
		goto exit;
	}

	ret = 0;

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_set_hdmi_audio_mute(struct rtk_rpc_info *rpc_info_ao,
		struct rpc_audio_mute_info *mute_info)
{
	struct rpc_audio_mute_info *i_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret;

	mutex_lock(&rpc_info_ao->lock);

	if (rpc_info_ao->ao_in_hifi == NULL) {
		ret = -ENXIO;
		goto exit;
	}

	i_rpc = (struct rpc_audio_mute_info *)rpc_info_ao->vaddr;
	offset = ALIGN(sizeof(struct rpc_audio_mute_info), RPC_ALIGN_SZ);

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (*rpc_info_ao->ao_in_hifi) {
		i_rpc->instanceID = mute_info->instanceID;
	} else {
		i_rpc->instanceID = htonl(mute_info->instanceID);
	}

	i_rpc->hdmi_mute = mute_info->hdmi_mute;

	if (send_rpc(rpc_info_ao,
			ENUM_KERNEL_RPC_HDMI_MUTE,
			rpc_info_ao->paddr, rpc_info_ao->paddr + offset,
			&rpc_ret)) {
		ret = -EIO;
		goto exit;
	}

	ret = 0;

exit:
	mutex_unlock(&rpc_info_ao->lock);
	return ret;
}

int rpc_set_cvbs_auto_detection(struct rtk_rpc_info *rpc_info,
		unsigned int enable)
{
	struct rpc_privateinfo_param *i_rpc;
	struct rpc_privateinfo_returnval *o_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_privateinfo_returnval *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (opt != RPC_AUDIO) {
		i_rpc->instanceId = 0;
		i_rpc->type = ENUM_PRIVATEINFO_VO_CVBS_PLUG_DETECTION_ENABLE;
		i_rpc->privateInfo[0] = enable;
	} else {
		i_rpc->instanceId = htonl(0);
		i_rpc->type = htonl(ENUM_PRIVATEINFO_VO_CVBS_PLUG_DETECTION_ENABLE);
		i_rpc->privateInfo[0] = htonl(enable);
	}

	ret = send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	mutex_unlock(&rpc_info->lock);

	return ret;
}

int rpc_get_cvbs_format(struct rtk_rpc_info *rpc_info,
		unsigned int *p_cvbs_fmt)
{
	struct rpc_privateinfo_param *i_rpc;
	struct rpc_privateinfo_returnval *o_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_privateinfo_returnval *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (opt != RPC_AUDIO) {
		i_rpc->instanceId = 0;
		i_rpc->type = ENUM_PRIVATEINFO_VIDEO_GET_CVBS_FORMAT;
	} else {
		i_rpc->instanceId = htonl(0);
		i_rpc->type = htonl(ENUM_PRIVATEINFO_VIDEO_GET_CVBS_FORMAT);
	}

	ret = send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	if (ret)
		goto exit;

	if (opt != RPC_AUDIO)
		*p_cvbs_fmt = o_rpc->privateInfo[0];
	else
		*p_cvbs_fmt = htonl(o_rpc->privateInfo[0]);

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_set_cvbs_format(struct rtk_rpc_info *rpc_info,
		unsigned int cvbs_fmt)
{
	struct rpc_privateinfo_param *i_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), RPC_ALIGN_SZ);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (opt != RPC_AUDIO) {
		i_rpc->instanceId = 0;
		i_rpc->type = ENUM_PRIVATEINFO_VIDEO_SET_CVBS_FORMAT;
		i_rpc->privateInfo[0] = cvbs_fmt;
	} else {
		i_rpc->instanceId = htonl(0);
		i_rpc->type = htonl(ENUM_PRIVATEINFO_VIDEO_SET_CVBS_FORMAT);
		i_rpc->privateInfo[0] = htonl(cvbs_fmt);
	}

	ret = send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	mutex_unlock(&rpc_info->lock);

	return ret;
}

#ifdef CONFIG_CHROME_PLATFORMS
int rpc_set_mixer_order(struct rtk_rpc_info *rpc_info,
           struct rpc_disp_mixer_order *arg)
{
	struct rpc_disp_mixer_order *rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = 0;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_disp_mixer_order *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_disp_mixer_order));
	offset = ALIGN(offset, 128);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	memcpy((unsigned char *)rpc, (unsigned char *)arg,
           sizeof(struct rpc_disp_mixer_order));

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_SET_MIXER_ORDER,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret)) {
		ret = -EIO;
		goto exit;
	}

	if (rpc_ret != FW_RETURN_SUCCESS) {
		ret = -ENOEXEC;
		goto exit;
	}

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_get_mixer_order(struct rtk_rpc_info *rpc_info,
           struct rpc_disp_mixer_order *mixer_order)
{
	struct rpc_disp_mixer_order *i_rpc;
	struct rpc_disp_mixer_order *o_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = 0;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_disp_mixer_order *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_disp_mixer_order));
	offset = ALIGN(offset, 128);
	o_rpc = (struct rpc_disp_mixer_order *)((unsigned long)i_rpc + offset);

	if (send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_GET_MIXER_ORDER,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret)) {
		ret = -EIO;
		goto exit;
	}

	if (rpc_ret != FW_RETURN_SUCCESS) {
		ret = -ENOEXEC;
		goto exit;
	}

	mixer_order->pic  = o_rpc->pic;
	mixer_order->dd   = o_rpc->dd;
	mixer_order->v1   = o_rpc->v1;
	mixer_order->sub1 = o_rpc->sub1;
	mixer_order->v2   = o_rpc->v2;
	mixer_order->osd1 = o_rpc->osd1;
	mixer_order->osd2 = o_rpc->osd2;
	mixer_order->csr  = o_rpc->csr;
	mixer_order->sub2 = o_rpc->sub2;
	mixer_order->v3   = o_rpc->v3;
	mixer_order->v4   = o_rpc->v4;
	mixer_order->osd3 = o_rpc->osd3;
	mixer_order->osd4 = o_rpc->osd4;

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}
#endif
int rpc_get_cvbs_connection_status(struct rtk_rpc_info *rpc_info,
		unsigned int *status)
{
	struct rpc_privateinfo_param *i_rpc;
	struct rpc_privateinfo_returnval *o_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret;
	int opt;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), RPC_ALIGN_SZ);
	o_rpc = (struct rpc_privateinfo_returnval *)((unsigned long)i_rpc + offset);
	opt = rpc_info->krpc_vo_opt;

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (opt != RPC_AUDIO) {
		i_rpc->instanceId = 0;
		i_rpc->type = ENUM_PRIVATEINFO_VIDEO_GET_CVBS_STATUS;
	} else {
		i_rpc->instanceId = htonl(0);
		i_rpc->type = htonl(ENUM_PRIVATEINFO_VIDEO_GET_CVBS_STATUS);
	}

	ret = send_rpc(rpc_info,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	if (ret)
		goto exit;

	if (opt != RPC_AUDIO)
		*status = o_rpc->privateInfo[0];
	else
		*status = htonl(o_rpc->privateInfo[0]);

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rtk_rpc_init(struct device *dev, struct rtk_rpc_info *rpc_info, int of_index)
{
	struct device_node *syscon_np;
	struct rtk_ipc_shm __iomem *ipc = (void __iomem *)IPC_SHM_VIRT;
	int ret = -1;
	int lock_id;
	unsigned int val = 0;

	rpc_info->dev = dev;

	mutex_init(&rpc_info->lock);

	rpc_info->krpc_ept_info = of_krpc_ept_info_get(dev->of_node, of_index);
	if (IS_ERR(rpc_info->krpc_ept_info)) {
		ret = PTR_ERR(rpc_info->krpc_ept_info);
		if (ret == -EPROBE_DEFER) {
			dev_err(dev, "krpc ept info not ready, retry\n");
			return ret;
		}

		rpc_info->krpc_ept_info = NULL;
	}

	if (rpc_info->krpc_ept_info == NULL) {
		dev_err(dev, "can't find krpc[%d] ept info\n", of_index);
		return -ENODEV;
	}

	if (rpc_info->krpc_ept_info) {
		drm_ept_init(rpc_info->krpc_ept_info);
	}

	of_property_read_u32_index(dev->of_node, "krpc-vo-opt", of_index, &rpc_info->krpc_vo_opt);

	dev_info(dev, "krpc[%d] is %s\n", of_index, krpc_names[rpc_info->krpc_vo_opt]);

	if (rpc_info->krpc_vo_opt == RPC_HIFI) {
		val = readl(ioremap(0x9801a604, 0x1));
		if ((val & (1 << 21)) && (val & (1 << 23))) {
			dev_info(dev, "VO HIFI is on\n");
			rpc_info->vo_sync_flag = ioremap(0x9810060c, 0x1);
			writel(0, rpc_info->vo_sync_flag);
		}
	} else if (rpc_info->krpc_vo_opt == RPC_KR4) {
		rpc_info->vo_sync_flag = ioremap(0x9801a588 , 0x1);
		writel(0, rpc_info->vo_sync_flag);
	} else
		rpc_info->vo_sync_flag = (void __iomem *)&ipc->vo_int_sync;

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "can't find syscon for display subsystem\n");
		return -ENODEV;
	} else {
		rpc_info->acpu_int_base = syscon_node_to_regmap(syscon_np);
	}

	lock_id = of_hwspin_lock_get_id(dev->of_node, 0);
	if (lock_id > 0 || (IS_ENABLED(CONFIG_HWSPINLOCK) && lock_id == 0)) {
		struct hwspinlock *lock = devm_hwspin_lock_request_specific(dev, lock_id);

		if (lock) {
			dev_info(dev, "use hwlock%d\n", lock_id);
			rpc_info->hwlock = lock;
		}
	} else {
		if (lock_id != -ENOENT)
			dev_err(dev, "failed to get hwlock: %pe\n", ERR_PTR(lock_id));
	}

#ifndef CONFIG_CHROME_PLATFORMS
	set_dma_ops(dev, &rheap_dma_ops);
	rheap_setup_dma_pools(dev, "rtk_audio_heap",
			RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			 RTK_FLAG_ACPUACC, __func__);
#endif

	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	dev->dma_mask = (u64 *)&dev->coherent_dma_mask;

	rpc_info->vaddr = dma_alloc_coherent(dev, RPC_CMD_BUFFER_SIZE,
					&rpc_info->paddr,
					GFP_KERNEL | __GFP_NOWARN);
	if (!rpc_info->vaddr) {
		pr_err("%s failed to allocate rpc buffer\n", __func__);
		return -1;
	}

	dev_info(dev, "rpc_info(%p) is %s\n",
			rpc_info, krpc_names[rpc_info->krpc_vo_opt]);

#if IS_ENABLED(CONFIG_KERN_RPC_HANDLE_COMMAND)
	rpc_info->pid = register_kernel_rpc("drm_rpc", RPC_AUDIO);
	rpc_info->rpc_thread = kthread_run(rpc_get_command_thread, rpc_info, "rpc_thread");
#endif
	return 0;
}
