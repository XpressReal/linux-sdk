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
#ifdef CONFIG_KERN_RPC_HANDLE_COMMAND
#include <linux/kthread.h>
#include <linux/slab.h>
#endif
#include <linux/io.h>
//#include <asm/io.h>
#include "rtk_drm_rpc.h"

#ifdef ENABLE_TEE_DRM_FLOW
#include <linux/tee_drv.h>
#endif

#ifdef DMABUF_HEAPS_RTK
#define ION_FLAG_PROTECTED_V2_AUDIO_POOL        (ION_FLAG_PROTECTED_BITS(ION_PROTECTED_TYPE_1))

#ifdef ENABLE_TEE_DRM_FLOW
extern int ta_TEEapi_init(struct tee_context **teeapi_ctx, unsigned int *teeapi_tee_session);
extern int ta_TEEapi_deinit(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session);
#endif

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

/* reserved for future use
static void rtk_rpc_free(struct dma_buf_attachment *attach)
{
	struct dma_buf *dmabuf;

	pr_debug("%s free dmabuf\n", __func__);
	dmabuf = attach->dmabuf;
	BUG_ON(!dmabuf);
	BUG_ON(!dmabuf->ops);
	BUG_ON(!dmabuf->ops->end_cpu_access);

	if (dmabuf->vmap_ptr)
		dma_buf_vunmap(dmabuf, dmabuf->vmap_ptr);

	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	dma_buf_unmap_attachment(attach, attach->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}
*/

static int get_rpc_opt(void)
{
	int opt;

	if (IS_ENABLED(CONFIG_REALTEK_RPC_V2)) {
		if (is_rpc_available(RPC_HIFI))
			opt = RPC_HIFI;
		else
			opt = RPC_AUDIO;

	} else {
		if (IS_ENABLED(CONFIG_REALTEK_RPC_HIFI))
			opt = RPC_HIFI;
		else
			opt = RPC_AUDIO;
	}

	return opt;
}

static int rtk_rpc_alloc(struct device *dev, size_t size,
			 struct dma_buf_attachment **attach,
			 struct sg_table **table,
			 void **ion_phy, void **ion_virt)
{
	int ret = 0;
	struct dma_buf *dmabuf;

	dmabuf = rheap_alloc("rtk_audio_heap", size, AUDIO_RTK_FLAG);
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

unsigned int ipcReadULONG(u8 *src)
{
	return __be32_to_cpu(readl(src));
}

void ipcCopyMemory(void *p_des, void *p_src, unsigned long len)
{
	unsigned char *des = (unsigned char *)p_des;
	unsigned char *src = (unsigned char *)p_src;
	int i;

	for (i = 0; i < len; i += 4)
		writel(__cpu_to_be32(readl(&src[i])), &des[i]);
}

#ifdef CONFIG_RTK_V4L2_DECODER
static void drm_handle_acpu_rpc(RPC_STRUCT *rpc, char *buf, void *data)
{
	pr_err("enter drm_handle_acpu_rpc\n");
}
#endif

static int send_rpc(struct rtk_rpc_info *rpc_info, int opt, uint32_t command, uint32_t param1, uint32_t param2, uint32_t *retval)
{
	int ret = 0;

	ret = send_rpc_command(opt, command, param1, param2, retval);

	return ret;
}

int rpc_destroy_video_agent(struct rtk_rpc_info *rpc_info, u32 pinId)
{
	struct rpc_create_video_agent *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_create_video_agent *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(rpc->instance));

	memset_io(rpc, 0, sizeof(*rpc));

	rpc->instance = htonl(pinId);
#ifdef CONFIG_RTK_V4L2_DECODER
        if (send_rpc_command_with_pid(RPC_AUDIO, rpc_info->pid, ENUM_VIDEO_KERNEL_RPC_DESTROY,
#else
	if (send_rpc(rpc_info, RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_DESTROY,
#endif
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_create_video_agent(struct rtk_rpc_info *rpc_info, u32 *videoId, u32 pinId)
{
	struct rpc_create_video_agent *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_create_video_agent *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(rpc->instance));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(pinId);
#ifdef CONFIG_RTK_V4L2_DECODER
	if (send_rpc_command_with_pid(RPC_AUDIO, rpc_info->pid, ENUM_VIDEO_KERNEL_RPC_CREATE,
#else
	if (send_rpc(rpc_info, RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_CREATE,
#endif
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	*videoId = ntohl(rpc->data);

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_display(struct rtk_rpc_info *rpc_info,
		      struct rpc_vo_filter_display *argp)
{
	struct rpc_vo_filter_display_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_vo_filter_display_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(*argp));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(argp->instance);
	rpc->videoPlane = htonl(argp->videoPlane);
	rpc->zeroBuffer = argp->zeroBuffer;
	rpc->realTimeSrc = argp->realTimeSrc;

	if (send_rpc(rpc_info, RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_DISPLAY,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_config_disp_win(struct rtk_rpc_info *rpc_info,
			      struct rpc_config_disp_win *argp)
{
	struct rpc_config_disp_win_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_disp_win_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(*argp));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

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

	if (send_rpc(rpc_info, RPC_AUDIO,
			     ENUM_VIDEO_KERNEL_RPC_CONFIGUREDISPLAYWINDOW,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;
	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_query_dis_win(struct rtk_rpc_info *rpc_info,
	struct rpc_query_disp_win_in *argp_in,
	struct rpc_query_disp_win_out* argp_out)
{
	struct rpc_query_disp_win_in *i_rpc = NULL;
	struct rpc_query_disp_win_out *o_rpc = NULL;
	unsigned int offset;
	int ret = -1;
	unsigned int RPC_ret;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_disp_win_in *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_query_disp_win_in));
	o_rpc = (struct rpc_query_disp_win_out *)((unsigned long)i_rpc + offset);

	memset_io(i_rpc, 0, RPC_CMD_BUFFER_SIZE);
	i_rpc->plane =  htonl(argp_in->plane);

	if (send_rpc_command(RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_WIN,
		rpc_info->paddr, rpc_info->paddr + offset,
		&RPC_ret)) {
		goto exit;
	}

	if (RPC_ret != S_OK)
		goto exit;

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

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_config_graphic(struct rtk_rpc_info *rpc_info,
	struct rpc_config_graphic_canvas* argp)
{
	struct rpc_config_graphic_canvas_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_graphic_canvas_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_config_graphic_canvas));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);
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

	if (send_rpc_command(RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_CONFIGURE_GRAPHIC_CANVAS,
		rpc_info->paddr, rpc_info->paddr + offset,
		&rpc->ret)) {
		goto exit;
	}

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_set_refclock(struct rtk_rpc_info *rpc_info,
			   struct rpc_refclock *argp)
{
	struct rpc_refclock_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_refclock_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(*argp));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(argp->instance);
	rpc->pRefClock = htonl(argp->pRefClock);

	if (send_rpc(rpc_info, RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_SETREFCLOCK,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_init_ringbuffer(struct rtk_rpc_info *rpc_info,
			      struct rpc_ringbuffer *argp)
{
	struct rpc_ringbuffer_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_ringbuffer_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(unsigned int)*3);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(argp->instance);
	rpc->readPtrIndex = htonl(argp->readPtrIndex);
	rpc->pinID = htonl(argp->pinID);
	rpc->pRINGBUFF_HEADER = htonl(argp->pRINGBUFF_HEADER);

	if (send_rpc(rpc_info, RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_INITRINGBUFFER,
			     rpc_info->paddr + offset, rpc_info->paddr,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_run(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	struct rpc_video_run_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_video_run_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(unsigned int));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(instance);

	if (send_rpc(rpc_info, RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_RUN,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_pause(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	struct rpc_video_run_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_video_run_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(unsigned int));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(instance);

	if (send_rpc_command(RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_PAUSE,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_stop(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	struct rpc_video_run_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_video_run_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(unsigned int));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(instance);

	if (send_rpc_command(RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_STOP,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_flush(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	struct rpc_video_run_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_video_run_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(unsigned int));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(instance);

	if (send_rpc_command(RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_FLUSH,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_destroy(struct rtk_rpc_info *rpc_info, unsigned int instance)
{
	struct rpc_video_run_t *rpc = NULL;
	unsigned int offset;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_video_run_t *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(unsigned int));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instance = htonl(instance);

	if (send_rpc_command(RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_DESTROY,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc->ret))
		goto exit;

	if (ntohl(rpc->result) != S_OK || rpc->ret != S_OK)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_video_set_q_param(struct rtk_rpc_info *rpc_info,
			struct rpc_set_q_param *arg)
{
	struct rpc_set_q_param *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_q_param *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_set_q_param));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_set_q_param));

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_channel_lowdelay *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_config_channel_lowdelay));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_config_channel_lowdelay));

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_privateinfo_param));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_privateinfo_param));

	if (send_rpc_command(RPC_AUDIO,
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
	unsigned int offset;
	int ret = -1;
	unsigned int RPC_ret;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_query_disp_win_in *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_query_disp_win_in));
	o_rpc = (struct rpc_query_disp_win_out_new *)((unsigned long)i_rpc + offset);

	memset_io(i_rpc, 0, RPC_CMD_BUFFER_SIZE);
	i_rpc->plane =  htonl(argp_in->plane);

	if (send_rpc_command(RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_QUERYDISPLAYWINNEW,
		rpc_info->paddr, rpc_info->paddr + offset,
		&RPC_ret)) {
		goto exit;
	}

	if (RPC_ret != S_OK)
		goto exit;

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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_speed *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_set_speed));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_set_speed));

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_background *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_set_background));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->bgColor.c1 = ntohs(arg->bgColor.c1);
	rpc->bgColor.c2 = ntohs(arg->bgColor.c2);
	rpc->bgColor.c3 = ntohs(arg->bgColor.c3);
	rpc->bgColor.isRGB = ntohs(arg->bgColor.isRGB);
	rpc->bgEnable = arg->bgEnable;

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_keep_curpic *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_keep_curpic));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_keep_curpic));

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_keep_curpic *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_keep_curpic));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_keep_curpic));

	if (send_rpc_command(RPC_AUDIO,
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
	struct rpc_keep_curpic_svp *i_rpc = NULL;
	struct rpc_keep_curpic_svp *o_rpc = NULL;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_keep_curpic_svp *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_keep_curpic_svp));
	o_rpc = (struct rpc_keep_curpic_svp *)((unsigned long)i_rpc + offset);

	memset_io(i_rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)i_rpc, (unsigned char *)arg,
			sizeof(struct rpc_keep_curpic_svp));

	if (send_rpc_command(RPC_AUDIO,
			ENUM_VIDEO_KERNEL_RPC_KEEPCURPICSVP,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

	if (rpc_ret != S_OK)
		goto exit;

	if(arg->type == ENUM_VIDEO_KEEP_CUR_SVP_TYPE_GET_FW_MALLOC_SVP_BUFFER )
	{
		arg->Yaddr = ntohl(o_rpc->Yaddr);
		arg->Ysize = ntohl(o_rpc->Ysize);
	}
	else if(arg->type == ENUM_VIDEO_KEEP_CUR_SVP_TYPE_GET_CUR)
	{
		arg->Yaddr = ntohl(o_rpc->Yaddr);
		arg->Ysize = ntohl(o_rpc->Ysize);
		arg->Caddr = ntohl(o_rpc->Caddr);
		arg->Csize = ntohl(o_rpc->Csize);
		arg->offsetTable_yaddr = ntohl(o_rpc->offsetTable_yaddr);
		arg->offsetTable_ysize = ntohl(o_rpc->offsetTable_ysize);
		arg->offsetTable_caddr = ntohl(o_rpc->offsetTable_caddr);
		arg->offsetTable_csize = ntohl(o_rpc->offsetTable_csize);
	}

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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_deintflag *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_set_deintflag));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_set_deintflag));

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_create_graphic_win *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_create_graphic_win));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->plane = ntohl(arg->plane);
	rpc->winPos.x = ntohs(arg->winPos.x);
	rpc->winPos.y = ntohs(arg->winPos.y);
	rpc->winPos.width = ntohs(arg->winPos.width);
	rpc->winPos.height = ntohs(arg->winPos.height);
	rpc->colorFmt = ntohl(arg->colorFmt);
	rpc->rgbOrder = ntohl(arg->rgbOrder);
	rpc->colorKey = ntohl(arg->colorKey);
	rpc->alpha = arg->alpha;

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_draw_graphic_win *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_draw_graphic_win));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

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

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_modify_graphic_win *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_modify_graphic_win));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

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

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_delete_graphic_win *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_delete_graphic_win));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->plane = ntohl(arg->plane);
	rpc->winID = ntohs(arg->winID);
	rpc->go = arg->go;

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_osd_palette *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_config_osd_palette));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->paletteIndex = arg->paletteIndex;
	rpc->pPalette = ntohl(arg->pPalette);

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_config_plane_mixer *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_config_plane_mixer));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

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

	if (send_rpc_command(RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_set_sdrflag *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_set_sdrflag));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	ipcCopyMemory((unsigned char *)rpc, (unsigned char *)arg,
			sizeof(struct rpc_set_sdrflag));

	if (send_rpc_command(RPC_AUDIO,
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

int rpc_video_set_priority_mode(struct rtk_rpc_info *rpc_info,
		uint8_t video_priority_mode)
{
	struct rpc_privateinfo_param *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int ret = -1;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_privateinfo_param));

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	rpc->instanceId = htonl(0);
	rpc->type = htonl(ENUM_PRIVATEINFO_DV_VIDEO_PRIORITY_MODE);
	rpc->privateInfo[0] = htonl(video_priority_mode);

	if (send_rpc_command(RPC_AUDIO,
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

	memset_io(vendor, 0, sizeof(vendor));
	memset_io(product, 0, sizeof(product));
	memcpy(vendor, vendor_str, min(sizeof(vendor), strlen(vendor_str)));
	memcpy(product, product_str, min(sizeof(product), strlen(product_str)));

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), RPC_ALIGN_SZ);

	memset_io(i_rpc, 0, sizeof(*i_rpc));

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

	ret = send_rpc(rpc_info, RPC_AUDIO,
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

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_vout_edid_raw_data *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_vout_edid_raw_data), RPC_ALIGN_SZ);
	edid_offset = offset * 2;
	memset_io(rpc, 0, sizeof(*rpc));

	memcpy(rpc_info->vaddr + edid_offset, edid_data, edid_size);
	rpc->paddr = htonl(rpc_info->paddr + edid_offset);
	rpc->size = htonl(edid_size);

	ret = send_rpc(rpc_info, RPC_AUDIO,
			ENUM_KERNEL_RPC_HDMI_EDID_RAW_DATA,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);
	if (ret)
		goto exit;

	if (get_rpc_opt() == RPC_HIFI) {
		rpc->paddr = rpc_info->paddr + edid_offset;
		rpc->size = edid_size;

		ret = send_rpc(rpc_info, RPC_HIFI,
				ENUM_KERNEL_RPC_HDMI_EDID_RAW_DATA,
				rpc_info->paddr, rpc_info->paddr + offset,
				&rpc_ret);
	}

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_set_hdmi_audio_onoff(struct rtk_rpc_info *rpc_info,
			 struct rpc_audio_ctrl_data *arg)
{
	struct rpc_audio_ctrl_data *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int opt;
	int ret;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_audio_ctrl_data *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_audio_ctrl_data), RPC_ALIGN_SZ);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (get_rpc_opt() == RPC_HIFI) {
		opt = RPC_HIFI;
		rpc->version = arg->version;
		rpc->hdmi_en_state = arg->hdmi_en_state;
	} else {
		opt = RPC_AUDIO;
		rpc->version = htonl(arg->version);
		rpc->hdmi_en_state = htonl(arg->hdmi_en_state);
	}

	ret = send_rpc(rpc_info, opt, ENUM_KERNEL_RPC_HDMI_AO_ONOFF,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);
	if (ret)
		goto exit;

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_send_hdmi_freq(struct rtk_rpc_info *rpc_info,
			 struct rpc_audio_hdmi_freq *arg)
{
	struct rpc_audio_hdmi_freq *rpc;
	uint32_t offset;
	uint32_t rpc_ret;
	int opt;
	int ret;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_audio_hdmi_freq *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_audio_hdmi_freq), RPC_ALIGN_SZ);

	memset_io(rpc, 0, RPC_CMD_BUFFER_SIZE);

	if (get_rpc_opt() == RPC_HIFI) {
		opt = RPC_HIFI;
		rpc->tmds_freq = arg->tmds_freq;
	} else {
		opt = RPC_AUDIO;
		rpc->tmds_freq = htonl(arg->tmds_freq);
	}

	ret = send_rpc(rpc_info, opt, ENUM_KERNEL_RPC_HDMI_SET,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_set_vrr(struct rtk_rpc_info *rpc_info,
			struct rpc_vout_hdmi_vrr *arg)
{
	struct rpc_vout_hdmi_vrr *rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int ret = -EIO, i;

	mutex_lock(&rpc_info->lock);

	rpc = (struct rpc_vout_hdmi_vrr *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_vout_hdmi_vrr));

	rpc->vrr_function = htonl(arg->vrr_function);
	rpc->vrr_act = htonl(arg->vrr_act);

	for (i = 0; i < 15; i++)
		rpc->reserved[i] = htonl(arg->reserved[i]);

	if (send_rpc_command(RPC_AUDIO,
			ENUM_VIDEO_KERNEL_RPC_SET_HDMI_VRR,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret))
		goto exit;

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

	mutex_lock(&rpc_info->lock);

	if (*rpc_info->hdmi_new_mac) {
		ret = -EPERM;
		goto exit;
	}

	i_rpc = (struct rpc_config_tv_system *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_config_tv_system));
	o_rpc = (struct rpc_config_tv_system *)((unsigned long)i_rpc + offset);

	if (send_rpc(rpc_info, RPC_AUDIO,
			     ENUM_VIDEO_KERNEL_RPC_QUERY_CONFIG_TV_SYSTEM,
			     rpc_info->paddr, rpc_info->paddr + offset,
			     &rpc_ret))
		goto exit;

	for (i = 0; i < sizeof(struct rpc_config_tv_system); i++)
		((char *)arg)[i] = ((char *)o_rpc)[i];

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

	mutex_lock(&rpc_info->lock);

	if (*rpc_info->hdmi_new_mac) {
		ret = -EPERM;
		goto exit;
	}

	rpc = (struct rpc_config_tv_system *)rpc_info->vaddr;
	offset = get_rpc_alignment_offset(sizeof(struct rpc_config_tv_system));

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

	if (send_rpc(rpc_info, RPC_AUDIO,
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
	unsigned int rpc_ret;
	int ret;

	mutex_lock(&rpc_info->lock);

	if (!*rpc_info->hdmi_new_mac) {
		ret = -EPERM;
		goto exit;
	}

	i_rpc = (struct rpc_display_output_format *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_config_tv_system), 128);
	o_rpc = (struct rpc_display_output_format *)((unsigned long)i_rpc + offset);

	if (send_rpc_command(RPC_AUDIO,
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
	unsigned int rpc_ret;
	int ret;

	mutex_lock(&rpc_info->lock);

	if (!*rpc_info->hdmi_new_mac) {
		ret = -EPERM;
		goto exit;
	}

	i_rpc = (struct rpc_display_output_format *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_config_tv_system), 128);

	memset_io(i_rpc, 0, sizeof(*i_rpc));
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

	if (send_rpc_command(RPC_AUDIO,
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

int rpc_set_hdmi_audio_mute(struct rtk_rpc_info *rpc_info,
		struct rpc_audio_mute_info *mute_info)
{
	struct rpc_audio_mute_info *i_rpc;
	unsigned int offset;
	unsigned int rpc_ret;
	int opt;
	int ret;

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_audio_mute_info *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_audio_mute_info), 128);

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	if (get_rpc_opt() == RPC_HIFI) {
		opt = RPC_HIFI;
		i_rpc->instanceID = mute_info->instanceID;
	} else {
		opt = RPC_AUDIO;
		i_rpc->instanceID = htonl(mute_info->instanceID);
	}

	i_rpc->hdmi_mute = mute_info->hdmi_mute;

	if (send_rpc(rpc_info, opt,
			ENUM_KERNEL_RPC_HDMI_MUTE,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret)) {
		ret = -EIO;
		goto exit;
	}

	ret = 0;

exit:
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

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), 128);
	o_rpc = (struct rpc_privateinfo_returnval *)((unsigned long)i_rpc + offset);

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	i_rpc->instanceId = htonl(0);
	i_rpc->type = htonl(ENUM_PRIVATEINFO_VIDEO_GET_CVBS_FORMAT);

	ret = send_rpc(rpc_info, RPC_AUDIO,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);
	if (ret)
		goto exit;

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

	mutex_lock(&rpc_info->lock);

	i_rpc = (struct rpc_privateinfo_param *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct rpc_privateinfo_param), 128);

	memset_io(i_rpc, 0, sizeof(*i_rpc));

	i_rpc->instanceId = htonl(0);
	i_rpc->type = htonl(ENUM_PRIVATEINFO_VIDEO_SET_CVBS_FORMAT);
	i_rpc->privateInfo[0] = htonl(cvbs_fmt);

	ret = send_rpc(rpc_info, RPC_AUDIO,
			ENUM_VIDEO_KERNEL_RPC_PRIVATEINFO,
			rpc_info->paddr, rpc_info->paddr + offset,
			&rpc_ret);

	mutex_unlock(&rpc_info->lock);

	return ret;
}

int rtk_rpc_init(struct device *dev, struct rtk_rpc_info *rpc_info)
{
	struct rtk_ipc_shm __iomem *ipc = (void __iomem *)IPC_SHM_VIRT;
#ifdef DMABUF_HEAPS_RTK
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	int ret = 0;
#endif

	rpc_info->vo_sync_flag = &ipc->vo_int_sync;
#ifdef DMABUF_HEAPS_RTK
	ret = rtk_rpc_alloc(dev, RPC_CMD_BUFFER_SIZE, &attach, &table,
				 (void **)&rpc_info->paddr,
				 (void **)&rpc_info->vaddr);
	if (ret) {
		pr_err("[%s %d alloc fail]\n", __func__, __LINE__);
		return -1;
	}
	rpc_info->attach = attach;
	rpc_info->dmabuf = attach->dmabuf;

#else
	rpc_info->vaddr = dma_alloc_attrs(dev, RPC_CMD_BUFFER_SIZE,
					&rpc_info->paddr,
					GFP_KERNEL | __GFP_NOWARN,
					DMA_ATTR_WRITE_COMBINE);
#endif
	if (!rpc_info->vaddr) {
		pr_err("%s failed to allocate rpc buffer\n", __func__);
		return -1;
	}
	rpc_info ->dev = dev;

	mutex_init(&rpc_info->lock);
#ifdef CONFIG_RTK_V4L2_DECODER
	rpc_info->pid = register_kernel_rpc("drm_rpc", RPC_AUDIO, &drm_handle_acpu_rpc, rpc_info);
#endif
	return 0;
}

int rtk_rpc_uninit(struct rtk_rpc_info *rpc_info)
{
	int ret = 0;

#if defined(ENABLE_TEE_DRM_FLOW)
    ret = ta_TEEapi_deinit((struct tee_context *)rpc_info->teeapi_ctx, rpc_info->teeapi_tee_session);
    if (ret < 0)
    {
        pr_err("[-] [%d]%s.ta_TEEapi_deinit() fail.ret:%d\n",__LINE__,__func__,ret);
        return ret;
    }
#endif
	mutex_destroy(&rpc_info->lock);
	return ret;
}
