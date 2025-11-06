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
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/asound.h>
#include <asm/cacheflush.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include "snd-hifi-realtek.h"
#include "snd-rtk-compress.h"

static int krpc_hifi_cb(struct rtk_krpc_ept_info *krpc_ept_info, char *buf)
{
	u32 *tmp;
	struct rpc_struct *rpc = (struct rpc_struct *)buf;

	if (rpc->programID == REPLYID) {
		tmp = (u32 *)(buf + sizeof(struct rpc_struct));
		*(krpc_ept_info->retval) = *(tmp + 1);

		complete(&krpc_ept_info->ack);
	}

	return 0;
}

int snd_ept_init(struct rtk_krpc_ept_info *krpc_ept_info)
{
	int ret = 0;

	ret = krpc_info_init(krpc_ept_info, "snd", krpc_hifi_cb);

	return ret;
}

static char *prepare_rpc_data(struct rtk_krpc_ept_info *krpc_ept_info,
			      u32 command, u32 param1,
			      u32 param2, int *len)
{
	struct rpc_struct *rpc;
	u32 *tmp;
	char *buf;

	*len = sizeof(struct rpc_struct) + 3 * sizeof(u32);
	buf = kmalloc(sizeof(struct rpc_struct) + 3 * sizeof(u32), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	rpc = (struct rpc_struct *)buf;
	rpc->programID = KERNELID;
	rpc->versionID = KERNELID;
	rpc->procedureID = 0;
	rpc->taskID = krpc_ept_info->id;
	rpc->sysTID = 0;
	rpc->sysPID = 0;
	rpc->parameterSize = 3 * sizeof(u32);
	rpc->mycontext = 0;
	tmp = (u32 *)(buf + sizeof(struct rpc_struct));
	*tmp = command;
	*(tmp + 1) = param1;
	*(tmp + 2) = param2;

	return buf;
}

static int snd_send_rpc(struct rtk_krpc_ept_info *krpc_ept_info, char *buf, int len, u32 *retval)
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
		pr_err("ALSA HIFI: kernel rpc timeout: %s...\n", krpc_ept_info->name);
		rtk_krpc_dump_ringbuf_info(krpc_ept_info);
		WARN_ON(1);
		mutex_unlock(&krpc_ept_info->send_mutex);
		return -EINVAL;
	}
	mutex_unlock(&krpc_ept_info->send_mutex);

	return 0;
}

static int send_rpc(struct rtk_krpc_ept_info *krpc_ept_info,
		    u32 command, u32 param1,
		    u32 param2, u32 *retval)
{
	int ret = 0;
	int len;
	char *buf;

	buf = prepare_rpc_data(krpc_ept_info, command, param1, param2, &len);
	if (!IS_ERR(buf)) {
		ret = snd_send_rpc(krpc_ept_info, buf, len, retval);
		kfree(buf);
	}

	return ret;
}

int rpc_get_global_pp_pin(phys_addr_t paddr, void *vaddr, int *pinId)
{
	struct audio_rpc_privateinfo_parameters *cmd = NULL;
	struct audio_rpc_privateinfo_returnval *res;
	u32 RPC_ret;
	int ret = -1;
	phys_addr_t dat;
	unsigned long offset;

	cmd = (struct audio_rpc_privateinfo_parameters *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct audio_rpc_privateinfo_parameters));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (struct audio_rpc_privateinfo_returnval *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct audio_rpc_privateinfo_parameters));

	cmd->type = ENUM_PRIVATEINFO_AUDIO_GET_PP_FREE_PINID;

	if (send_rpc(hifi_compr_ept_info,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&RPC_ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}

	if (res != NULL && res->private_info[0] == 0x50504944) {
		*pinId = res->private_info[2];
		ret = 0;
	} else {
		pr_err("[ALSA %s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}

	pr_info("[%s %s %d] success\n", __FILE__, __FUNCTION__, __LINE__);
exit:
	return ret;
}

int rpc_create_pp_agent(phys_addr_t paddr, void *vaddr,
			int *ppId, int *pinId)
{
	struct rpc_create_ao_agent_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_create_ao_agent_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_create_ao_agent_t));
	rpc->info.instance_id = 0;
	rpc->info.type = AUDIO_PP_OUT;

	if (send_rpc(hifi_compr_ept_info,
		ENUM_KERNEL_RPC_CREATE_AGENT,
		dat,
		dat + get_rpc_alignment_offset(sizeof(rpc->info)),
		&rpc->ret)) {
		pr_err("[ALSA %s %d RPC fail]\n",__FUNCTION__, __LINE__);
		goto exit;
	}

	if (rpc->retval.result != S_OK || rpc->ret != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n",__FUNCTION__, __LINE__);
		goto exit;
	}

	*ppId = rpc->retval.data;
	ret = rpc_get_global_pp_pin(paddr, vaddr, pinId);

	printk("[%s %s %d] success\n", __FILE__, __FUNCTION__, __LINE__);
exit:
	return ret;
}

int rpc_create_decoder_agent(phys_addr_t paddr, void *vaddr, int *dec_id,
			     int *pin_id)
{
	struct rpc_create_pcm_decoder_ctrl_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_create_pcm_decoder_ctrl_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_create_pcm_decoder_ctrl_t));
	rpc->instance.type = AUDIO_DECODER;
	rpc->instance.instance_id = -1;

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_CREATE_AGENT,
		     dat,
		     dat + get_rpc_alignment_offset(sizeof(rpc->instance)),
		     &rpc->ret)) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->ret != S_OK) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	*dec_id = rpc->res.data;
	*pin_id = BASE_BS_IN;

	pr_info("[ALSA Create Decoder instance %d]\n", *dec_id);
	ret = 0;
exit:
	return ret;
}

int rpc_create_ao_agent(phys_addr_t paddr, void *vaddr, int *ao_id,
			int pin_id)
{
	struct rpc_create_ao_agent_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_create_ao_agent_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_create_ao_agent_t));
	rpc->info.instance_id = 0;
	rpc->info.type = pin_id;

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_CREATE_AGENT,
		     dat,
		     dat + get_rpc_alignment_offset(sizeof(rpc->info)),
		     &rpc->ret)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->retval.result != S_OK || rpc->ret != S_OK) {
		pr_err("[ALSA %x %x %s %d RPC fail]\n",
		       rpc->retval.result, rpc->ret, __func__, __LINE__);
		goto exit;
	}

	*ao_id = rpc->retval.data;
	ret = 0;
	pr_info("[%s %s %d] success\n", __FILE__, __func__, __LINE__);
exit:
	return ret;
}

int rpc_initringbuffer_header(phys_addr_t paddr, void *vaddr,
			      struct audio_rpc_ringbuffer_header *header,
			      int buffer_count)
{
	struct rpc_initringbuffer_header_t *rpc = NULL;
	int ch;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_initringbuffer_header_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_initringbuffer_header_t));
	rpc->header.instance_id = header->instance_id;
	rpc->header.pin_id = header->pin_id;
	rpc->header.read_idx = header->read_idx;
	rpc->header.list_size = header->list_size;

	pr_info(" header instance ID %d\n", header->instance_id);
	pr_info(" header pin_id       %d\n", header->pin_id);
	pr_info(" header read_idx     %d\n", header->read_idx);
	pr_info(" header list_size    %d\n", header->list_size);

	for (ch = 0; ch < buffer_count; ch++)
		rpc->header.ringbuffer_header_list[ch] = header->ringbuffer_header_list[ch];

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_INIT_RINGBUF,
		     dat,
		     dat + get_rpc_alignment_offset(sizeof(rpc->header)),
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

int rpc_connect_svc(phys_addr_t paddr, void *vaddr,
		    struct audio_rpc_connection *pconnection)
{
	struct rpc_connection_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_connection_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_connection_t));
	rpc->out.src_instance_id = pconnection->src_instance_id;
	rpc->out.src_pin_id = pconnection->src_pin_id;
	rpc->out.des_instance_id = pconnection->des_instance_id;
	rpc->out.des_pin_id = pconnection->des_pin_id;

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_CONNECT,
		     dat,
		     dat + get_rpc_alignment_offset(sizeof(rpc->out)),
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

int rpc_switch_focus(phys_addr_t paddr, void *vaddr, struct audio_rpc_focus *focus)
{
	struct audio_rpc_focus_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	dat = paddr;
	rpc = (struct audio_rpc_focus_t *)vaddr;

	rpc->instance_id = focus->instance_id;
	rpc->focus_id = focus->focus_id;

	if (send_rpc(hifi_compr_ept_info,
		ENUM_KERNEL_RPC_SWITCH_FOCUS,
		dat,
		dat +
		sizeof(rpc->instance_id) +
		sizeof(rpc->focus_id) +
		sizeof(rpc->reserved),
		&rpc->res)) {
		pr_err("[%s RPC fail\n]", __func__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->ret.result != S_OK) {
		pr_err("[%s RPC fail %d]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int rpc_set_low_water_level(phys_addr_t paddr, void *vaddr, bool is_low_water)
{
	struct audio_rpc_privateinfo_parameters *cmd = NULL;
	struct audio_rpc_privateinfo_returnval *res;
	u32 rpc_ret = 0;
	int ret = -1;
	phys_addr_t dat;
	unsigned long offset;

	cmd = (struct audio_rpc_privateinfo_parameters *)vaddr;
	dat = paddr;

	offset = get_rpc_alignment_offset(sizeof(struct audio_rpc_privateinfo_parameters));
	offset = ALIGN(offset, 128); /* HIFI 128 Align */

	res = (struct audio_rpc_privateinfo_returnval *)((unsigned long)cmd + offset);
	memset(cmd, 0, sizeof(struct audio_rpc_privateinfo_parameters));

	cmd->type = ENUM_PRIVATEINFO_AUDIO_SET_LOW_WATERLEVEL;
	if (is_low_water) {
		cmd->private_info[0] = 0x1;
		pr_info("Enable pp ao low waterlevel mode\n");
	} else {
		cmd->private_info[0] = 0;
		pr_info("Disable pp ao low waterlevel mode\n");
	}
	cmd->private_info[1] = 0;
	cmd->private_info[2] = 0;
	cmd->private_info[3] = 0;

	if (send_rpc(hifi_compr_ept_info,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		dat,
		dat + offset,
		&rpc_ret)) {
		pr_err("[%s RPC fail\n]", __func__);
		goto exit;
	}

	if (rpc_ret != S_OK) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int rpc_set_refclock(phys_addr_t paddr, void *vaddr, struct audio_rpc_refclock *p_clock)
{
	struct audio_rpc_refclock_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct audio_rpc_refclock_t *)vaddr;
	dat = paddr;

	rpc->instance_id = p_clock->instance_id;
	rpc->pref_clock_id = p_clock->pref_clock_id;
	rpc->pref_clock = p_clock->pref_clock;

	if (send_rpc(hifi_compr_ept_info,
		ENUM_KERNEL_RPC_SETREFCLOCK,
		dat,
		dat +
		sizeof(rpc->instance_id) +
		sizeof(rpc->pref_clock_id) +
		sizeof(rpc->pref_clock) +
		sizeof(rpc->reserved),
		&rpc->res)) {
		pr_err("[%s RPC fail\n]", __func__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->ret.result != S_OK) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int rpc_flush_svc(phys_addr_t paddr, void *vaddr,
		  struct audio_rpc_sendio *sendio)
{
	struct rpc_flash_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_flash_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_flash_t));
	rpc->sendio.instance_id = sendio->instance_id;
	rpc->sendio.pin_id = sendio->pin_id;

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_FLUSH,
		     dat,
		     dat + sizeof(rpc->sendio),
		     &rpc->res)) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("[ALSA %s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int rpc_run_svc(phys_addr_t paddr, void *vaddr, int instance_id)
{
	struct rpc_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_t));
	rpc->inst_id = instance_id;

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_RUN,
		     dat,
		     dat +
		     sizeof(rpc->inst_id) +
		     sizeof(rpc->reserved),
		&rpc->res)) {
		pr_err("[%s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("[%s %d RPC fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:

	return ret;
}

int rpc_pause_svc(phys_addr_t paddr, void *vaddr, int instance_id)
{
	struct rpc_pause_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_pause_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_pause_t));
	rpc->inst_id = instance_id;

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_PAUSE,
		     dat,
		     dat +
		     sizeof(rpc->inst_id) +
		     sizeof(rpc->reserved),
		&rpc->res)) {
		pr_err("[%s %d RPC fail\n]", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK) {
		pr_err("[%s %d RPC fail\n]", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int rpc_stop_svc(phys_addr_t paddr, void *vaddr, int instance_id)
{
	struct rpc_stop_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_stop_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_stop_t));
	rpc->instance_id = instance_id;

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_STOP,
		     dat,
		     dat +
		     sizeof(rpc->instance_id) +
		     sizeof(rpc->reserved),
		&rpc->res)) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int rpc_destroy_svc(phys_addr_t paddr, void *vaddr, int instance_id)
{
	struct rpc_destroy_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_destroy_t *)vaddr;
	dat = paddr;

	memset(rpc, 0, sizeof(struct rpc_destroy_t));
	rpc->instance_id = instance_id;

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_DESTROY,
		     dat,
		     dat +
		     sizeof(rpc->instance_id) +
		     sizeof(rpc->reserved),
		&rpc->res)) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int rpc_pp_init_pin_svc(phys_addr_t paddr, void *vaddr, int instance_id)
{
	struct rpc_t *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;

	rpc = (struct rpc_t *)vaddr;
	dat = paddr;

	rpc->inst_id = instance_id;

	if (send_rpc(hifi_compr_ept_info,
		ENUM_VIDEO_KERNEL_RPC_PP_INIT_PIN,
		dat,
		dat +
		sizeof(rpc->inst_id) +
		sizeof(rpc->reserved),
		&rpc->res)) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	if (rpc->res != S_OK || rpc->retval.result != S_OK) {
		pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
		goto exit;
	}

	pr_info("[%s %d] success\n", __func__, __LINE__);
	ret = 0;
exit:
	return ret;
}

int rpc_set_mixing_idx(struct device *dev, struct rtk_snd_mixer *mixer)
{
	struct audio_rpc_aio_privateinfo_parameters_ext *rpc = NULL;
	struct audio_rpc_privateinfo_returnval *res;
	u32 ret = 0;
	phys_addr_t dat;
	unsigned long offset;
	void *vaddr;
	size_t size = SZ_4K;
	int ch;

	mutex_lock(&dev->mutex);
	rheap_setup_dma_pools(dev, "rtk_media_heap",
			      RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			      RTK_FLAG_ACPUACC, __func__);

	vaddr = dma_alloc_coherent(dev, size, &dat, GFP_KERNEL);
	if (!vaddr) {
		ret = -1;
		mutex_unlock(&dev->mutex);
		goto exit;
	}
	mutex_unlock(&dev->mutex);
	rpc = (struct audio_rpc_aio_privateinfo_parameters_ext *)vaddr;

	memset(rpc, 0, sizeof(struct audio_rpc_aio_privateinfo_parameters_ext));
	offset = get_rpc_alignment_offset(sizeof(struct audio_rpc_aio_privateinfo_parameters_ext));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (struct audio_rpc_privateinfo_returnval *)((unsigned long)rpc + offset);
	rpc->type = ENUM_PRIVATEINFO_AIO_AO_PCM_PIN_LPCM;
	rpc->instance_id = mixer->ao_agent_id;

	rpc->argate_info[0] = 1;
	rpc->argate_info[1] = sizeof(*rpc);

	for (ch=0; ch<MAX_CAR_CH; ch++)
		rpc->parameter[ch] = mixer->mixing_idx.mixing_channel[ch];

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
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
	if (vaddr)
		dma_free_coherent(dev, size, vaddr, dat);

	return ret;
}

int rpc_set_dec_volume(struct device *dev, int volume)
{
	struct audio_config_command *config = NULL;
	unsigned int *res;
	u32 ret = 0;
	phys_addr_t dat;
	unsigned long offset;
	void *vaddr;
	size_t size = SZ_4K;

	mutex_lock(&dev->mutex);
	rheap_setup_dma_pools(dev, "rtk_media_heap",
			      RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			      RTK_FLAG_ACPUACC, __func__);

	vaddr = dma_alloc_coherent(dev, size, &dat, GFP_KERNEL);
	if (!vaddr) {
		ret = -1;
		mutex_unlock(&dev->mutex);
		goto exit;
	}
	mutex_unlock(&dev->mutex);
	config = (struct audio_config_command *)vaddr;

	memset(config, 0, sizeof(struct audio_config_command));
	config->msd_id = AUDIO_CONFIG_CMD_AO_DEC_VOLUME;
	config->value[0] = 100 - volume;

	offset = get_rpc_alignment_offset(sizeof(struct audio_config_command));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (unsigned int *)((unsigned long)config + offset);

	if (send_rpc(hifi_compr_ept_info,
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
	if (vaddr)
		dma_free_coherent(dev, size, vaddr, dat);

	return ret;
}

int rpc_set_dec_volume_config(struct device *dev, int steps, int direct, int id)
{
	struct audio_config_command *config = NULL;
	unsigned int *res;
	u32 ret = 0;
	phys_addr_t dat;
	unsigned long offset;
	void *vaddr;
	size_t size = SZ_4K;

	mutex_lock(&dev->mutex);
	rheap_setup_dma_pools(dev, "rtk_media_heap",
			      RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			      RTK_FLAG_ACPUACC, __func__);

	vaddr = dma_alloc_coherent(dev, size, &dat, GFP_KERNEL);
	if (!vaddr) {
		ret = -1;
		mutex_unlock(&dev->mutex);
		goto exit;
	}
	mutex_unlock(&dev->mutex);
	config = (struct audio_config_command *)vaddr;

	memset(config, 0, sizeof(struct audio_config_command));

	config->msd_id = AUDIO_CONFIG_CMD_AO_DEC_VOLUME_CONFIG;
	config->value[0] = steps;
	config->value[1] = direct;
	config->value[5] = id;

	offset = get_rpc_alignment_offset(sizeof(struct audio_config_command));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (unsigned int *)((unsigned long)config + offset);

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_AUDIO_MULTI_AO_CONFIG,
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
	if (vaddr)
		dma_free_coherent(dev, size, vaddr, dat);

	return ret;
}

int rpc_set_dec_volume_new(struct device *dev, int volume, int id)
{
	struct audio_config_command *config = NULL;
	unsigned int *res;
	u32 ret = 0;
	phys_addr_t dat;
	unsigned long offset;
	void *vaddr;
	size_t size = SZ_4K;

	mutex_lock(&dev->mutex);
	rheap_setup_dma_pools(dev, "rtk_media_heap",
			      RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			      RTK_FLAG_ACPUACC, __func__);

	vaddr = dma_alloc_coherent(dev, size, &dat, GFP_KERNEL);
	if (!vaddr) {
		ret = -1;
		mutex_unlock(&dev->mutex);
		goto exit;
	}
	mutex_unlock(&dev->mutex);
	config = (struct audio_config_command *)vaddr;

	memset(config, 0, sizeof(struct audio_config_command));
	config->msd_id = AUDIO_CONFIG_CMD_AO_DEC_VOLUME;
	config->value[0] = 100 - volume;
	config->value[5] = id;

	offset = get_rpc_alignment_offset(sizeof(struct audio_config_command));
	offset = ALIGN(offset, 128); /* HIFI 128 align */

	res = (unsigned int *)((unsigned long)config + offset);

	if (send_rpc(hifi_compr_ept_info,
		     ENUM_KERNEL_RPC_AUDIO_MULTI_AO_CONFIG,
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
	if (vaddr)
		dma_free_coherent(dev, size, vaddr, dat);

	return ret;
}

MODULE_LICENSE("GPL v2");
