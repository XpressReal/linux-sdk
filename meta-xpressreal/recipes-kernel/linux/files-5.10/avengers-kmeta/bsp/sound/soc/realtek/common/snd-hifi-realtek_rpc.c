// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017-2020 Realtek Semiconductor Corp.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/syscalls.h>
#include <linux/mpage.h>
#include <linux/dcache.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/fdtable.h>
#include <linux/ion.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/asound.h>
#include <asm/cacheflush.h>
#include "snd-hifi-realtek.h"
#include <soc/realtek/kernel-rpc.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>

static void rtk_snd_rpc_free(struct dma_buf_attachment *attach)
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

static int rtk_snd_rpc_alloc(struct device *dev, size_t size,
			 struct dma_buf_attachment **attach,
			 struct sg_table **table,
			 void **ion_phy, void **ion_virt)
{
	int ret = 0;
	unsigned int alloc_flags;
	struct dma_buf *dmabuf;

	alloc_flags = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			RTK_FLAG_ACPUACC;

	dmabuf = rheap_alloc("rtk_audio_heap", size, alloc_flags);
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

	dev_dbg(dev, "%s *ion_phy = 0x%x , *ion_virt = 0x%x \n", __func__,
			 (unsigned int)(uintptr_t)*ion_phy,
			 (unsigned int)(uintptr_t)*ion_virt);

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

int send_rpc(int opt, uint32_t command, uint32_t param1, uint32_t param2, uint32_t *retval)
{
	int ret = 0;

	ret = send_rpc_command(opt, command, param1, param2, retval);

	return ret;
}
EXPORT_SYMBOL(send_rpc);

int RPC_TOAGENT_CREATE_AO_AGENT(phys_addr_t paddr, void *vaddr, int *aoId,
				 int pinId)
{
	struct RPC_CREATE_AO_AGENT_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_CREATE_AO_AGENT_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_CREATE_AO_AGENT_T));
	rpc->info.instanceID = 0;
	rpc->info.type = pinId;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_CREATE_AGENT,
		dat, //rpc->info address
		dat + get_rpc_alignment_offset(sizeof(rpc->info)),//rpc->retval address
		&rpc->ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->retval.result != S_OK || rpc->ret != S_OK) {
		pr_err("[ALSA %x %x %s %d RPC fail]\n", rpc->retval.result, rpc->ret, __func__, __LINE__);
		goto exit;
	}

	*aoId = rpc->retval.data;
	ret = 0;
	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
exit:
	return ret;
}

int RPC_TOAGENT_PUT_SHARE_MEMORY_LATENCY(phys_addr_t paddr, void *vaddr,
			void *p, void *p2, int decID, int aoID, int type)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	int ret = 0;
	int magic_num = 2379;
	uint32_t RPC_ret;
	phys_addr_t dat;
	unsigned long offset;

	pr_info("[%s ion_alloc p1 %x p2 %x decID %d aoID %d type %d\n", __FUNCTION__,
			(uint32_t)((long)p), (uint32_t)((long)p2), decID, aoID, type);

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));

	cmd->type = type;
	cmd->privateInfo[0] = (uint32_t)(long)p;
	cmd->privateInfo[1] = magic_num;
	cmd->privateInfo[2] = (uint32_t)(long)p2;
	cmd->privateInfo[3] = (uint32_t)decID;
	cmd->privateInfo[4] = (uint32_t)aoID;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
exit:

	return ret;
}

int RPC_TOAGENT_PUT_SHARE_MEMORY(phys_addr_t paddr, void *vaddr, void *p, int type)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	int ret = 0;
	uint32_t RPC_ret;
	phys_addr_t dat;
	unsigned long offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);

	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	cmd->type = type;
	cmd->privateInfo[0] = (uint32_t)(long)p;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
exit:

	return ret;
}

int RPC_TOAGENT_GET_AO_FLASH_PIN(phys_addr_t paddr, void *vaddr, int AOAgentID)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned long offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	memset(res, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_RETURNVAL));
	cmd->instanceID = AOAgentID;
	cmd->type = ENUM_PRIVATEINFO_AUDIO_GET_FLASH_PIN;
	cmd->privateInfo[0] = 0xFF;
	cmd->privateInfo[1] = 0xFF;
	cmd->privateInfo[2] = 0xFF;
	cmd->privateInfo[3] = 0xFF;
	cmd->privateInfo[4] = 0xFF;
	cmd->privateInfo[5] = 0xFF;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat, //cmd address
		dat + offset,//res address
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	ret = res->privateInfo[0];

	if (ret < FLASH_AUDIO_PIN_1 || ret > FLASH_AUDIO_PIN_3) {
		pr_err("[ALSA %s %d RPC %d fail]\n", __func__, __LINE__, ret);
		ret = -1;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
exit:
	return ret;
}

int RPC_TOAGENT_GET_GLOBAL_PP_PIN(phys_addr_t paddr, void *vaddr, int *pinId)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned long offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);

	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	cmd->type = ENUM_PRIVATEINFO_AUDIO_GET_PP_FREE_PINID;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat, //cmd address
		dat + offset, //res address
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (res != NULL && res->privateInfo[0] == 0x44495050) {
		*pinId = res->privateInfo[2];
		ret = 0;
	} else {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		ret = -1;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
exit:

	return ret;
}

int RPC_TOAGENT_SET_AO_FLASH_VOLUME(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_pcm *dpcm)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t rpc_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned long offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 Align */

	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));

	cmd->instanceID = dpcm->AOAgentID;
	cmd->type = ENUM_PRIVATEINFO_AUDIO_CONTROL_FLASH_VOLUME;
	cmd->privateInfo[0] = dpcm->AOpinID;
	cmd->privateInfo[1] = 31-dpcm->volume;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&rpc_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[ALSA set AO_pin %d volume %d]\n", dpcm->AOpinID, dpcm->volume);
	ret = 0;
exit:
	return ret;
}

int RPC_TOAGENT_CREATE_DECODER_AGENT(phys_addr_t paddr, void *vaddr, int *decID,
				     int *pinID)
{
	struct RPC_CREATE_PCM_DECODER_CTRL_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_CREATE_PCM_DECODER_CTRL_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_CREATE_PCM_DECODER_CTRL_T));
	rpc->instance.type = AUDIO_DECODER;
	rpc->instance.instanceID = -1;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_CREATE_AGENT,
		dat, //rpc->instance address
		dat + get_rpc_alignment_offset(sizeof(rpc->instance)), //rpc->res address
		&rpc->ret)) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->ret != S_OK) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	*decID = rpc->res.data;
	*pinID = BASE_BS_IN;

	pr_info("[ALSA Create Decoder instance %d]\n", *decID/*dpcm->DECAgentID*/);
	ret = 0;
exit:

	return ret;
}

/* data of AUDIO_RPC_RINGBUFFER_HEADER is "hose side" */
int RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(phys_addr_t paddr, void *vaddr,
			struct AUDIO_RPC_RINGBUFFER_HEADER *header, int buffer_count)
{
	struct RPC_INITRINGBUFFER_HEADER_T *rpc = NULL;
	int ch;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_INITRINGBUFFER_HEADER_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_INITRINGBUFFER_HEADER_T));
	rpc->header.instanceID = header->instanceID;
	rpc->header.pinID = header->pinID;
	rpc->header.readIdx = header->readIdx;
	rpc->header.listSize = header->listSize;

	pr_info(" header instance ID %d\n", header->instanceID);
	pr_info(" header pinID       %d\n", header->pinID);
	pr_info(" header readIdx     %d\n", header->readIdx);
	pr_info(" header listSize    %d\n", header->listSize);

	for (ch = 0; ch < buffer_count; ch++)
		rpc->header.pRingBufferHeaderList[ch] = (unsigned int)header->pRingBufferHeaderList[ch];

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_INIT_RINGBUF,
		dat, //rpc->header address
		dat + get_rpc_alignment_offset(sizeof(rpc->header)), //rpc->ret address
		&rpc->res)) {
		pr_err("[%s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->ret.result != S_OK) {
		pr_err("[%s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_CONNECT_SVC(phys_addr_t paddr, void *vaddr,
			struct AUDIO_RPC_CONNECTION *pconnection)
{
	struct RPC_CONNECTION_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_CONNECTION_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_CONNECTION_T));
	rpc->out.srcInstanceID = pconnection->srcInstanceID;
	rpc->out.srcPinID = pconnection->srcPinID;
	rpc->out.desInstanceID = pconnection->desInstanceID;
	rpc->out.desPinID = pconnection->desPinID;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_CONNECT,
		dat, //rpc->out address
		dat + get_rpc_alignment_offset(sizeof(rpc->out)), //rpc->ret
		&rpc->res)) {
		pr_err("[%s RPC fail %d]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->ret.result != S_OK) {
		pr_err("[%s RPC fail %d]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_PAUSE_SVC(phys_addr_t paddr, void *vaddr, int instance_id)
{
	struct RPC_TOAGENT_PAUSE_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_TOAGENT_PAUSE_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_TOAGENT_PAUSE_T));
	rpc->inst_id = instance_id;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PAUSE,
		dat, //rpc->inst_id address
		dat +
			sizeof(rpc->inst_id) +
			sizeof(rpc->reserved), //rpc->retval address
		&rpc->res)) {
		pr_err("[%s %d RPC fail\n]", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK) {
		pr_err("[%s %d RPC fail\n]", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_DESTROY_AI_FLOW_SVC(phys_addr_t paddr, void *vaddr, int instance_id)
{
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *rpc = NULL;
	int offset, ret = -1;
	uint32_t res;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 Align */

	memset(rpc, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	rpc->type = ENUM_PRIVATEINFO_AIO_ALSA_DESTROY_AI_FLOW;
	rpc->instanceID = instance_id;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&res)) {
		pr_err("[%s RPC fail\n]", __func__);
		goto exit;
	}

	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_RUN_SVC(phys_addr_t paddr, void *vaddr, int instance_id)
{
	struct RPC_TOAGENT_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_TOAGENT_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_TOAGENT_T));
	rpc->inst_id = instance_id;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_RUN,
		dat, //rpc->inst_id address
		dat +
			sizeof(rpc->inst_id) +
			sizeof(rpc->reserved), //rpc->retval address
		&rpc->res)) {
		pr_err("[%s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("[%s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_FLUSH_SVC(phys_addr_t paddr, void *vaddr,
			struct AUDIO_RPC_SENDIO *sendio)
{
	struct RPC_TOAGENT_FLASH_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_TOAGENT_FLASH_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_TOAGENT_FLASH_T));
	rpc->sendio.instanceID = sendio->instanceID;
	rpc->sendio.pinID = sendio->pinID;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_FLUSH,
		dat, //rpc->sendio address
		dat + sizeof(rpc->sendio), //rpc->retval address
		&rpc->res)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_RELEASE_AO_FLASH_PIN(phys_addr_t paddr, void *vaddr,
			int AOAgentID, int AOpinID)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t rpc_ret = 0;
	int ret = -1;
	phys_addr_t dat;
	unsigned long offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 Align */

	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));

	cmd->instanceID = AOAgentID;
	cmd->type = ENUM_PRIVATEINFO_AUDIO_RELEASE_FLASH_PIN;
	cmd->privateInfo[0] = AOpinID;
	cmd->privateInfo[1] = 0xFF;
	cmd->privateInfo[2] = 0xFF;
	cmd->privateInfo[3] = 0xFF;
	cmd->privateInfo[4] = 0xFF;
	cmd->privateInfo[5] = 0xFF;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat, //cmd address
		dat + offset, //res address
		&rpc_ret)) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc_ret != S_OK) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_AO_CONFIG_WITHOUT_DECODER(phys_addr_t paddr, void *vaddr,
			struct snd_pcm_runtime *runtime)
{
	struct snd_card_RTK_pcm *dpcm = runtime->private_data;
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	int ret = -1;
	int offset, tmp, ch;
	char *p;
	uint32_t RPC_ret;
	phys_addr_t dat;

	cmd = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 Align */

	p = (char *)&cmd->argateInfo[3];
	memset(cmd, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);

	cmd->type = ENUM_PRIVATEINFO_AIO_AO_FLASH_LPCM;
	cmd->instanceID = dpcm->AOAgentID;

	tmp = dpcm->AOpinID & 0xff;
	cmd->argateInfo[1] |= tmp;
	tmp = ((runtime->sample_bits >> 3) << 8);
	cmd->argateInfo[1] |= tmp;
	tmp = AUDIO_LITTLE_ENDIAN << 16;
	cmd->argateInfo[1] |= tmp;

	// disable ADFS
	if (dpcm->ao_paramter.disable_adfs) {
		tmp = dpcm->ao_paramter.disable_adfs << 24;
		cmd->argateInfo[1] |= tmp;
	}

	cmd->argateInfo[1] = cmd->argateInfo[1];

	cmd->argateInfo[2] = runtime->rate;
	for (ch = 0; ch < runtime->channels; ++ch)
		p[ch] = ch + 1;

	// config ao lpcm out delay and ao hw buffer delay
	if (dpcm->ao_paramter.hw_buffer_delay) {
		cmd->argateInfo[5] = (dpcm->ao_paramter.lpcm_out_delay << 16);
		cmd->argateInfo[5] |= dpcm->ao_paramter.hw_buffer_delay;
		cmd->argateInfo[5] = cmd->argateInfo[5];
	} else {
		// Default value for buffer size
		cmd->argateInfo[5] = (15 << 16);
		cmd->argateInfo[5] |= 20;
		cmd->argateInfo[5] = cmd->argateInfo[5];
	}

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[%s %d RPC fail\n]", __FUNCTION__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __FUNCTION__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_STOP_SVC(phys_addr_t paddr, void *vaddr, int instanceID)
{
	struct RPC_TOAGENT_STOP_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_TOAGENT_STOP_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_TOAGENT_STOP_T));
	rpc->instanceID = instanceID;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_STOP,
		dat, //rpc->instanceID address
		dat +
			sizeof(rpc->instanceID) +
			sizeof(rpc->reserved), //rpc->retval address
		&rpc->res)) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_DESTROY_SVC(phys_addr_t paddr, void *vaddr, int instanceID)
{
	struct RPC_TOAGENT_DESTROY_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_TOAGENT_DESTROY_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_TOAGENT_DESTROY_T));
	rpc->instanceID = instanceID;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_DESTROY,
		dat, //rpc->instanceID address
		dat +
			sizeof(rpc->instanceID) +
			sizeof(rpc->reserved), //rpc->retval address
		&rpc->res)) {
		pr_err("%s %d RPC fail\n", __FILE__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("%s %d RPC fail\n", __FILE__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_INBAND_EOS_SVC(struct snd_card_RTK_pcm *dpcm)
{
	struct AUDIO_DEC_EOS cmd;

	cmd.header.type = AUDIO_DEC_INBAND_CMD_TYPE_EOS;
	cmd.header.size = sizeof(struct AUDIO_DEC_EOS);
	cmd.EOSID = 0;
	cmd.wPtr = dpcm->decInRing->writePtr;
	writeInbandCmd(dpcm, &cmd, sizeof(struct AUDIO_DEC_EOS));

	return 0;
}

/* set AO volume */
int RPC_TOAGENT_SET_VOLUME(struct device *dev, int volume)
{
	struct AUDIO_CONFIG_COMMAND *config = NULL;
	unsigned int *res;
	uint32_t ret = 0;
	phys_addr_t dat;
	unsigned long offset;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	int handle;

	handle = rtk_snd_rpc_alloc(dev, 4096,
				 &attach, &table, (void **)&dat,
				 (void **)&config);
	if (handle < 0) {
		pr_err("[%s %d alloc fail]\n", __func__, __LINE__);
		goto exit;;
	}

	memset(config, 0, sizeof(struct AUDIO_CONFIG_COMMAND));
	config->msgID = AUDIO_CONFIG_CMD_VOLUME;
	config->value[0] = 31 - volume;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_CONFIG_COMMAND));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (unsigned int *)((unsigned long)config + offset);

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_VOLUME_CONTROL,
		dat,
		dat + offset,
		&ret)) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}

	if (ret != S_OK) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:
	if (handle >= 0)
		rtk_snd_rpc_free(attach);

	return ret;
}

/* get AI ID */
int RPC_TOAGENT_GET_AI_AGENT(phys_addr_t paddr, void *vaddr,
		struct snd_card_RTK_capture_pcm *dpcm)
{
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *in = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *out;
	int offset;
	uint32_t ret = 0;
	phys_addr_t dat;

	in = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(in, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	out = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)in + offset);
	in->type = ENUM_PRIVATEINFO_AIO_GET_AUDIO_PROCESSING_AI;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&ret)) {
		pr_err("[fail %s %d]\n", __func__, __LINE__);
		goto exit;
	}

	dpcm->AIAgentID = out->privateInfo[0];
	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

/* get AO volume */
int RPC_TOAGENT_GET_VOLUME(phys_addr_t paddr, void *vaddr)
{
	int volume = 0;
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *pArg = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *pRet;
	int offset;
	uint32_t rc;
	phys_addr_t dat;

	pArg = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(pArg, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 Align */

	pRet = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)pArg + offset);
	pArg->type = ENUM_PRIVATEINFO_AUDIO_GET_MUTE_N_VOLUME;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_GET_MUTE_N_VOLUME,
		dat,
		dat + offset,
		&rc)) {
		pr_err("[fail %s %d]\n", __func__, __LINE__);
		volume = -1;
		goto exit;
	}

	volume = pRet->privateInfo[1];
	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
exit:
	return volume;
}

int RPC_TOAGENT_AI_CONFIG_HDMI_RX_IN(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm)
{
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *rpc = NULL;
	uint32_t RPC_ret;
	unsigned int offset;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	rpc->instanceID = dpcm->AIAgentID;
	rpc->type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	rpc->argateInfo[0] = ENUM_AI_PRIVATE_HDMI_RX;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_AI_CONFIG_I2S_IN(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *rpc = NULL;
	uint32_t RPC_ret;
	unsigned int offset;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	rpc->instanceID = dpcm->AIAgentID;
	rpc->type = ENUM_PRIVATEINFO_AUDIO_AI_PAD_IN;
	rpc->privateInfo[0] = 48000;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int RPC_TOAGENT_AI_CONFIG_AUDIO_IN(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm)
{
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *rpc = NULL;
	uint32_t RPC_ret;
	unsigned int offset;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	rpc->instanceID = dpcm->AIAgentID;
	rpc->type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;

	if (dpcm->source_in == ENUM_AIN_AUDIO_V2)
		rpc->argateInfo[0] = ENUM_AI_PRIVATE_DUAL_DMIC_AND_LOOPBACK;
	else if (dpcm->source_in == ENUM_AIN_AUDIO_V3) {
		rpc->argateInfo[0] = ENUM_AI_PRIVATE_SPEECH_RECOGNITION_FROM_DMIC;
		rpc->argateInfo[1] = ENUM_AIN_AUDIO_PROCESSING_DMIC;
	} else if (dpcm->source_in == ENUM_AIN_AUDIO_V4) {
		rpc->argateInfo[0] = ENUM_AI_PRIVATE_SPEECH_RECOGNITION_FROM_DMIC;
		rpc->argateInfo[1] = ENUM_AIN_AUDIO_PROCESSING_I2S;
	}

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_AI_CONFIG_I2S_LOOPBACK_IN(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm)
{
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *rpc = NULL;
	uint32_t RPC_ret;
	unsigned int offset;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	rpc->instanceID = dpcm->AIAgentID;
	rpc->type = ENUM_PRIVATEINFO_AIO_AI_LOOPBACK_AO;
	rpc->argateInfo[0] |= (1 << ENUM_RPC_AI_LOOPBACK_FROM_AO_I2S);
	rpc->argateInfo[0] = rpc->argateInfo[0];

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int RPC_TOAGENT_AI_CONFIG_DMIC_PASSTHROUGH_IN(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm)
{
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *rpc = NULL;
	uint32_t RPC_ret;
	unsigned int offset;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	rpc->instanceID = dpcm->AIAgentID;
	rpc->type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	rpc->argateInfo[0] = ENUM_AI_PRIVATE_ADC_DMIC;
	rpc->argateInfo[1] = 16000;

	if (dpcm->dmic_volume[0] == 0 && dpcm->dmic_volume[1] == 0)
		rpc->argateInfo[4] = htonl(0x303); /* Enlarge the volume */
	else {
		if (dpcm->dmic_volume[0] < 0)
			dpcm->dmic_volume[0] = 0;
		else if (dpcm->dmic_volume[0] > 3)
			dpcm->dmic_volume[0] = 3;

		if (dpcm->dmic_volume[1] < 0)
			dpcm->dmic_volume[1] = 0;
		else if (dpcm->dmic_volume[1] > 3)
			dpcm->dmic_volume[1] = 3;

		rpc->argateInfo[4] = (dpcm->dmic_volume[0] << 8) + dpcm->dmic_volume[1];
		rpc->argateInfo[4] = htonl(rpc->argateInfo[4]);
	}

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_CREATE_GLOBAL_AO(phys_addr_t paddr, void *vaddr, int *aoId)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *rpc = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	unsigned int offset;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)rpc + offset);
	rpc->type = ENUM_PRIVATEINFO_AUDIO_GET_GLOBAL_AO_INSTANCEID;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	*aoId = res->privateInfo[0];
	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_SET_AI_FLASH_VOLUME(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm, unsigned int volume)
{
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *rpc = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t rpc_ret;
	unsigned int offset;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)rpc + offset);
	rpc->instanceID = dpcm->AIAgentID;
	rpc->type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	rpc->argateInfo[0] = ENUM_AI_PRIVATE_ADC_SET_VOLUME;
	rpc->argateInfo[1] = volume; // ADC left channel digital volume in 0.5 dB step, -33.5dB~30dB
	rpc->argateInfo[2] = volume; // ADC right channel digital volume in 0.5 dB step, -33.5dB~30dB

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&rpc_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_SET_SOFTWARE_AI_FLASH_VOLUME(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm, unsigned int volume)
{
	struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *rpc = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t rpc_ret;
	unsigned int offset;
	int ret = -1, rpc_volume;
	phys_addr_t dat;

	/* The range of software control volume index is from 0 to 32. */
	if (volume >= 0 && volume < 17)
		rpc_volume = ENUM_AUDIO_VOLUME_CTRL_0_DB + volume;
	else if (volume >= 17 && volume < 33)
		rpc_volume = ENUM_AUDIO_VOLUME_CTRL_P1_DB + volume % 17;
	else
		goto exit;

	rpc = (struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_AIO_PRIVATEINFO_PARAMETERS));
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)rpc + offset);
	rpc->instanceID = dpcm->AIAgentID;
	rpc->type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	rpc->argateInfo[0] = ENUM_AI_PRIVATE_VOLUME_CTRL;
	rpc->argateInfo[1] = rpc_volume; // one of AUDIO_VOLUME_CTRL

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
		dat,
		dat + offset,
		&rpc_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_SET_EQ(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_pcm *dpcm,
			struct AUDIO_RPC_EQUALIZER_MODE *equalizer_mode)
{
	struct AUDIO_EQUALIZER_CONFIG *rpc = NULL;
	uint32_t rpc_ret;
	unsigned int offset;
	int i, ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_EQUALIZER_CONFIG *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_EQUALIZER_CONFIG));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_EQUALIZER_CONFIG));
	rpc->instanceID = dpcm->AOAgentID;
	rpc->gbl_var_eq_ID = ENUM_EQUALIZER_AO;
	rpc->ena = 0x1;

	/* Set up EQ data */
	rpc->app_eq_config.mode = equalizer_mode->mode;
	for (i = 0; i < 10; i++)
		rpc->app_eq_config.gain[i] = equalizer_mode->gain[i];

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_EQ_CONFIG,
		dat,
		dat + offset,
		&rpc_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_AI_CONFIG_NONPCM_IN(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *rpc = NULL;
	uint32_t RPC_ret;
	unsigned int offset;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	rpc->instanceID = dpcm->AIAgentID;
	rpc->type = ENUM_PRIVATEINFO_AUDIO_AI_NON_PCM_IN;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_CREATE_AI_AGENT(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm)
{
	struct RPC_CREATE_AO_AGENT_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct RPC_CREATE_AO_AGENT_T *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct RPC_CREATE_AO_AGENT_T));
	rpc->info.instanceID = -1;
	rpc->info.type = AUDIO_IN;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_CREATE_AGENT,
		dat,
		dat + get_rpc_alignment_offset(sizeof(rpc->info)),
		&rpc->ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->retval.result != S_OK || rpc->ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	dpcm->AIAgentID = rpc->retval.data;
	pr_info("[%s] [ALSA Create AI instance %d]\n", __func__, dpcm->AIAgentID);

	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_AI_DISCONNECT_ALSA_AUDIO(phys_addr_t paddr, void *vaddr,
			struct snd_pcm_runtime *runtime)
{
	struct snd_card_RTK_capture_pcm *dpcm = runtime->private_data;
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned int offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);

	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	cmd->instanceID = dpcm->AIAgentID;
	cmd->type = ENUM_PRIVATEINFO_AUDIO_AI_CONNECT_ALSA;
	cmd->privateInfo[0] = AUDIO_ALSA_FORMAT_NONE;
	cmd->privateInfo[1] = runtime->rate;

	if (dpcm->source_in == ENUM_AIN_AUDIO)
		cmd->privateInfo[2] = 1;
	else
		cmd->privateInfo[2] = 0;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_AI_CONNECT_ALSA(phys_addr_t paddr, void *vaddr,
			struct snd_pcm_runtime *runtime)
{
	struct snd_card_RTK_capture_pcm *dpcm = runtime->private_data;
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned int offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));

	cmd->instanceID = dpcm->AIAgentID;
	cmd->type = ENUM_PRIVATEINFO_AUDIO_AI_CONNECT_ALSA;
	cmd->privateInfo[0] = dpcm->nAIFormat;
	cmd->privateInfo[1] = runtime->rate;

	switch (dpcm->source_in) {
	case ENUM_AIN_AUDIO:
		cmd->privateInfo[2] = 1;
		break;
	default:
		cmd->privateInfo[2] = 0;
		break;
	}

	switch (dpcm->source_in) {
	case ENUM_AIN_AUDIO_V2:
	case ENUM_AIN_AUDIO_V3:
		cmd->privateInfo[3] = 1; //1 channel
		break;
	default:
		cmd->privateInfo[3] = 0; //channels
		break;
	}

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_AI_CONNECT_AO(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_capture_pcm *dpcm)
{
	struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd = NULL;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned int offset;

	cmd = (struct AUDIO_RPC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_PARAMETERS));

	cmd->instanceID = dpcm->AIAgentID;
	cmd->type = ENUM_PRIVATEINFO_AUDIO_AI_SET_AO_FLASH_PIN;
	cmd->privateInfo[0] = dpcm->AOpinID;

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int RPC_TOAGENT_SET_MAX_LATENCY(phys_addr_t paddr, void *vaddr,
			struct snd_card_RTK_pcm *dpcm)
{
	struct AUDIO_RPC_DEC_PRIVATEINFO_PARAMETERS *cmd;
	struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *res;
	uint32_t RPC_ret;
	int ret = 0;
	phys_addr_t dat;
	unsigned long offset;

	cmd = (struct AUDIO_RPC_DEC_PRIVATEINFO_PARAMETERS *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct AUDIO_RPC_DEC_PRIVATEINFO_PARAMETERS));
	res = (struct AUDIO_RPC_PRIVATEINFO_RETURNVAL *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct AUDIO_RPC_DEC_PRIVATEINFO_PARAMETERS));
	memset(res, 0, sizeof(struct AUDIO_RPC_PRIVATEINFO_RETURNVAL));
	cmd->instanceID = dpcm->DECAgentID;
	cmd->type = ENUM_PRIVATEINFO_DEC_ALSA_CONFIG;
	cmd->privateInfo[0] = 11; /* max latency of dec out(ms) */
	cmd->privateInfo[1] = 30; /* max latency of ao out(ms) */

	if (send_rpc(RPC_HIFI,
		ENUM_KERNEL_RPC_DEC_PRIVATEINFO,
		dat, //cmd address
		dat + offset,//res address
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}

exit:

	return ret;
}
MODULE_LICENSE("GPL v2");
