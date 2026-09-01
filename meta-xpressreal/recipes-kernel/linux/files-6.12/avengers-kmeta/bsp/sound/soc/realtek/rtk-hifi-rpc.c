// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017-2020 Realtek Semiconductor Corp.
 */

#include "rtk-hifi-rpc.h"

static int rpc_hifi_cb(struct rtk_krpc_ept_info *ept, char *buf)
{
	u32 *tmp;
	struct rpc_struct *rpc = (struct rpc_struct *)buf;

	if (rpc->programID == REPLYID) {
		tmp = (u32 *)(buf + sizeof(struct rpc_struct));
		*(ept->retval) = *(tmp + 1);

		complete(&ept->ack);
	}
	return 0;
}

static int send_rpc(struct rtk_rpc_priv *priv, u32 *retval)
{
	struct rtk_krpc_ept_info *ept = priv->ept;
	char *buf;
	int ret = 0;

	ept->retval = retval;
	buf = (char *)&priv->data;
	ret = rtk_send_rpc(ept, buf, sizeof(struct rpc_data));
	if (ret < 0) {
		pr_err("[%s] send rpc failed\n", ept->name);
		return ret;
	}

	if (!wait_for_completion_timeout(&ept->ack, RPC_TIMEOUT)) {
		pr_err("[%s] kernel rpc timeout\n", ept->name);
		rtk_krpc_dump_ringbuf_info(ept);
		WARN_ON(1);
		return -EINVAL;
	}
	return 0;
}

static void rpc_set_channel_map(int ch, char *p)
{
	switch (ch) {
	case 1:
		p[0] = ENUM_AUDIO_LEFT_FRONT_INDEX;
		break;
	case 2:
		p[0] = ENUM_AUDIO_LEFT_FRONT_INDEX;
		p[1] = ENUM_AUDIO_RIGHT_FRONT_INDEX;
		break;
	case 3:
		p[0] = ENUM_AUDIO_LEFT_FRONT_INDEX;
		p[1] = ENUM_AUDIO_RIGHT_FRONT_INDEX;
		p[2] = ENUM_AUDIO_CENTER_FRONT_INDEX;
		break;
	case 4:
		p[0] = ENUM_AUDIO_LEFT_FRONT_INDEX;
		p[1] = ENUM_AUDIO_RIGHT_FRONT_INDEX;
		p[2] = ENUM_AUDIO_LEFT_SURROUND_REAR_INDEX;
		p[3] = ENUM_AUDIO_RIGHT_SURROUND_REAR_INDEX;
		break;
	case 5:
		p[0] = ENUM_AUDIO_LEFT_FRONT_INDEX;
		p[1] = ENUM_AUDIO_RIGHT_FRONT_INDEX;
		p[2] = ENUM_AUDIO_CENTER_FRONT_INDEX;
		p[3] = ENUM_AUDIO_LEFT_SURROUND_REAR_INDEX;
		p[4] = ENUM_AUDIO_RIGHT_SURROUND_REAR_INDEX;
		break;
	case 6:
		p[0] = ENUM_AUDIO_LEFT_FRONT_INDEX;
		p[1] = ENUM_AUDIO_RIGHT_FRONT_INDEX;
		p[2] = ENUM_AUDIO_CENTER_FRONT_INDEX;
		p[3] = ENUM_AUDIO_LFE_INDEX;
		p[4] = ENUM_AUDIO_LEFT_SURROUND_REAR_INDEX;
		p[5] = ENUM_AUDIO_RIGHT_SURROUND_REAR_INDEX;
		break;
	case 7:
	case 8:
		p[0] = ENUM_AUDIO_LEFT_FRONT_INDEX;
		p[1] = ENUM_AUDIO_RIGHT_FRONT_INDEX;
		p[2] = ENUM_AUDIO_CENTER_FRONT_INDEX;
		p[3] = ENUM_AUDIO_LFE_INDEX;
		p[4] = ENUM_AUDIO_LEFT_SURROUND_REAR_INDEX;
		p[5] = ENUM_AUDIO_RIGHT_SURROUND_REAR_INDEX;
		p[6] = ENUM_AUDIO_LEFT_OUTSIDE_FRONT_INDEX;
		p[7] = ENUM_AUDIO_RIGHT_OUTSIDE_FRONT_INDEX;
		break;
	default:
		pr_err("channel not support %d\n", ch);
		break;
	}
}

int init_ringbuf_header_ptrs(struct ringbuf_header_ptrs *ptrs,
			     void *base, bool cacheable)
{
	int size;

	if (!cacheable) {
		ptrs->p_magic = base + 0;
		ptrs->p_begin_addr = base + 4;
		ptrs->p_size = base + 8;
		ptrs->p_buffer_id = base + 12;
		ptrs->p_write_ptr = base + 16;
		ptrs->p_num_read_ptr = base + 20;
		ptrs->p_read_ptr = base + 32;
		ptrs->p_file_offset = base + 48;
		ptrs->p_requested_file_offset = base + 52;
		ptrs->p_file_size = base + 56;
		ptrs->p_seekable = base + 60;
		ptrs->p_latency = base + SHMEM_OFFSET;
		size = 512;
	} else {
		ptrs->p_magic = base + 256;
		ptrs->p_begin_addr = base + 260;
		ptrs->p_size = base + 264;
		ptrs->p_buffer_id = base + 268;
		ptrs->p_write_ptr = base + 0;
		ptrs->p_num_read_ptr = base + 272;
		ptrs->p_read_ptr = base + 128;
		ptrs->p_file_offset = base + 284;
		ptrs->p_requested_file_offset = base + 288;
		ptrs->p_file_size = base + 292;
		ptrs->p_seekable = base + 296;
		ptrs->p_latency = base + SHMEM_OFFSET;
		size = 512;
	}
	return size;
}

static int rpc_send_command(struct rtk_rpc_priv *priv, char *rpc_src, size_t size,
			    struct rpc_result *result, int cmd)
{
	char *rpc;
	struct rpc_result *retval;
	int ret = -1, rpc_ret, offset;

	mutex_lock(&priv->ept->send_mutex);

	rpc = (char *)priv->vaddr;
	memset(rpc, 0, size);
	offset = ALIGN(size, 128);
	retval = (struct rpc_result *)(rpc + offset);
	memset(retval, 0, sizeof(*retval));

	memcpy(rpc, rpc_src, size);
	priv->data.command = cmd;
	priv->data.param2 =  priv->paddr + offset;

	if (send_rpc(priv, &rpc_ret)) {
		pr_err("[%s] fail, cmd(%d)\n", __func__, cmd);
		goto exit;
	}
	if (rpc_ret != S_OK || (result && retval->result != S_OK)) {
		pr_err("[%s] ret fail, cmd(%d)\n", __func__, cmd);
		goto exit;
	}
	if (result)
		memcpy(result, retval, sizeof(*result));
	ret = 0;
exit:
	mutex_unlock(&priv->ept->send_mutex);
	return ret;
}

static int rpc_send_private(struct rtk_rpc_priv *priv,
			    struct rpc_privateinfo_param *param,
			    struct rpc_privateinfo_result *result,
			    int cmd)
{
	struct rpc_privateinfo_param *rpc;
	struct rpc_privateinfo_result *retval;
	int ret = -1, rpc_ret, offset;

	mutex_lock(&priv->ept->send_mutex);

	rpc = (struct rpc_privateinfo_param *)priv->vaddr;
	memset(rpc, 0, sizeof(*rpc));
	offset = ALIGN(sizeof(*rpc), 128);
	retval = (struct rpc_privateinfo_result *)((char *)rpc + offset);
	memset(retval, 0, sizeof(*retval));

	memcpy(rpc, param, sizeof(*rpc));
	priv->data.command = cmd;
	priv->data.param2 =  priv->paddr + offset;

	if (send_rpc(priv, &rpc_ret)) {
		pr_err("[%s] fail, type(%d)\n", __func__, rpc->type);
		goto exit;
	}
	if (rpc_ret != S_OK) {
		pr_err("[%s] ret fail, type(%x)\n", __func__, rpc->type);
		goto exit;
	}

	if (result)
		memcpy(result, retval, sizeof(*result));
	ret = 0;
exit:
	mutex_unlock(&priv->ept->send_mutex);
	return ret;
}

int rpc_create_audio_agent(struct rtk_rpc_priv *priv, int pin, int *ao_id)
{
	struct rpc_create_ao_agent rpc = {0};
	struct rpc_result result = {0};
	int ret;

	rpc.instance_id = 0;
	rpc.type = pin;

	ret = rpc_send_command(priv, (char *)&rpc, sizeof(rpc), &result,
			       ENUM_KERNEL_RPC_CREATE_AGENT);
	if (!ret)
		*ao_id = result.data;

	pr_info("[audio rpc] %s\n", __func__);
	return ret;
}

int rpc_init_ringbuffer_header(struct rtk_rpc_priv *priv,
			       struct rpc_ringbuffer_header *rpc)
{
	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_command(priv, (char *)rpc, sizeof(*rpc), NULL,
				ENUM_KERNEL_RPC_INIT_RINGBUF);
}

int rpc_put_shmem_latency(struct rtk_rpc_priv *priv, int ao_id, int pin, void *p)
{
	struct rpc_privateinfo_param rpc = {0};

	rpc.instance_id = ao_id;
	rpc.type = ENUM_PRIVATEINFO_AUDIO_GET_SHARE_MEMORY_FROM_ALSA;
	rpc.private_info[0] = 0;
	rpc.private_info[1] = 2379;
	rpc.private_info[2] = (u32)(long)p;
	rpc.private_info[3] = 0;
	rpc.private_info[4] = ao_id;

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_PRIVATEINFO);
}


int rpc_connect_svc(struct rtk_rpc_priv *priv, int src_id, int src_pin,
		    int des_id, int des_pin)
{
	struct rpc_connection rpc = {0};
	rpc.src_instance_id = src_id;
	rpc.src_pin_id = src_pin;
	rpc.des_instance_id = des_id;
	rpc.des_pin_id = des_pin;
	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_command(priv, (char *)&rpc, sizeof(rpc), NULL,
				ENUM_KERNEL_RPC_CONNECT);
}

int rpc_pause_svc(struct rtk_rpc_priv *priv, int id)
{
	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_command(priv, (char *)&id, sizeof(id), NULL,
				ENUM_KERNEL_RPC_PAUSE);
}

int rpc_run_svc(struct rtk_rpc_priv *priv, int id)
{
	struct rpc_result result = {0};

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_command(priv, (char *)&id, sizeof(id), &result,
				ENUM_KERNEL_RPC_RUN);
}

int rpc_stop_svc(struct rtk_rpc_priv *priv, int id)
{
	struct rpc_result result = {0};

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_command(priv, (char *)&id, sizeof(id), &result,
				ENUM_KERNEL_RPC_STOP);
}

int rpc_destroy_svc(struct rtk_rpc_priv *priv, int id)
{
	struct rpc_result result = {0};
	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_command(priv, (char *)&id, sizeof(id), &result,
				ENUM_KERNEL_RPC_DESTROY);
}
int rpc_config_dprx_in(struct rtk_rpc_priv *priv, int ai_id, int ao_id, int pin)
{
	struct rpc_privateinfo_param rpc = {0};
	int ret;
	pr_info("[audio rpc] %s %d\n", __func__, __LINE__);
	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AUDIO_AI_NON_PCM_IN;
	ret = rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_PRIVATEINFO);
	if (ret)
		return ret;
	pr_info("[audio rpc] %s %d\n", __func__, __LINE__);
	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	rpc.private_info[0] = ENUM_AI_PRIVATE_DPRX;
	ret = rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
	if (ret)
		return ret;
	pr_info("[audio rpc] %s %d\n", __func__, __LINE__);
	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AO_FLASH_CHANGE_HW_SAMPLE_RATE;
	rpc.private_info[0] = ENUM_AO_FLASH_CHANGE_HW_SAMPLE_RATE_ENABLED;
	ret = rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
	if (ret)
		return ret;
	pr_info("[audio rpc] %s %d\n", __func__, __LINE__);
	rpc.instance_id = ao_id;
	rpc.type = ENUM_PRIVATEINFO_AUDIO_CONTROL_FLASH_VOLUME;
	rpc.private_info[0] = pin;
	rpc.private_info[1] = 0;
//	rpc.private_info[1] = ENUM_AUDIO_VOLUME_LEVEL_0_DB;
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_PRIVATEINFO);
}

int rpc_config_i2s_in(struct rtk_rpc_priv *priv, int ai_id, int id, int mode)
{
	struct rpc_privateinfo_param rpc = {0};
	unsigned int command = 0;

	rpc.instance_id = ai_id;
	if (id == 0) {
		rpc.type = ENUM_PRIVATEINFO_AUDIO_AI_PAD_IN;
		command = ENUM_KERNEL_RPC_PRIVATEINFO;
		rpc.private_info[0] = 48000;
		rpc.private_info[1] = 0x11224466;
		rpc.private_info[2] = mode;
	} else if (id == 1) {
		rpc.type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
		command = ENUM_KERNEL_RPC_AIO_PRIVATEINFO;
		rpc.private_info[0] = ENUM_AI_PRIVATE_I2S_1;
		rpc.private_info[1] = 48000;
		rpc.private_info[2] = 0x11224466;
		if (mode == ENUM_AI_I2S_SHARE)
			rpc.private_info[3] = ENUM_AI_I2S_PIN_SHARE_AO_I2S1;
		else
			rpc.private_info[3] = mode;
	}

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, command);
}

int rpc_config_sti2s_in(struct rtk_rpc_priv *priv, int ai_id)
{
	struct rpc_privateinfo_param rpc = {0};

	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	rpc.private_info[0] = ENUM_AI_PRIVATE_STI2S;
	rpc.private_info[1] |= ENUM_AIN_STI2S_NORMAL_MUX << 8;
	rpc.private_info[1] |= 1 << 16;

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
}

int rpc_config_pdm_in(struct rtk_rpc_priv *priv, int ai_id)
{
	struct rpc_privateinfo_param rpc = {0};

	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	rpc.private_info[0] = ENUM_AI_PRIVATE_ADC_DMIC;
	rpc.private_info[1] = 16000;
	rpc.private_info[4] = 0x303;

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
}

int rpc_config_tdm_in(struct rtk_rpc_priv *priv, int ai_id, struct rpc_tdmin_config *config)
{
	struct rpc_privateinfo_param rpc = {0};

	config->version = 1;
	config->data_format = ENUM_TDM_FMT_MODE_A;
	config->bclk = 256;
	config->lrck = 256;
	config->i2s_channel_len = 32;
	config->data_len = 24;
	config->tdm_channel_len = 32;

	if (rpc_send_command(priv, (char *)config, sizeof(*config), NULL,
				ENUM_KERNEL_RPC_SET_AI_TDM_CFG))
		return -1;

	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	if (config->slot == 0)
		rpc.private_info[0] = ENUM_AI_PRIVATE_TDM;
	else if (config->slot == 1)
		rpc.private_info[0] = ENUM_AI_PRIVATE_TDM_1;
	else if (config->slot == 2)
		rpc.private_info[0] = ENUM_AI_PRIVATE_TDM_2;
	else
		pr_err("%s slot number not support\n", __func__);

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
}

int rpc_config_adc_in(struct rtk_rpc_priv *priv, struct snd_pcm_runtime *runtime,
			int ai_id, int path, struct rpc_analog_config *config)
{
	struct rpc_privateinfo_param rpc = {0};
	struct rpc_privateinfo_result result = {0};
	int free_pin;

	rpc.type = ENUM_PRIVATEINFO_AIO_AI_GLOBAL_CONFIG;
	rpc.private_info[0] = ENUM_AUDIO_AI_GLOBAL_CONFIG_MIC_VOLTAGE;
	rpc.private_info[1] = 1;
	rpc.private_info[2] = 1;
	if (rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO)) {
		pr_err("[audio rpc] %s get free pin fail\n", __func__);
		goto exit;
	}

	rpc.type = ENUM_PRIVATEINFO_AIO_AI_ADC_ANALOG;
	rpc.private_info[0] = 0x1122eeff;
	rpc.private_info[1] = rpc.private_info[2] = 0;
	if (rpc_send_private(priv, &rpc, &result, ENUM_KERNEL_RPC_AIO_PRIVATEINFO)) {
		pr_err("[audio rpc] %s get free pin fail\n", __func__);
		goto exit;
	}

	free_pin = result.private_info[0] == 0 ? ENUM_AI_PRIVATE_ANGLOG
		: result.private_info[0] == 1 ? ENUM_AI_PRIVATE_ANGLOG_1
		: -1;
	if (free_pin < 0) {
		pr_err("[audio rpc] %s free pin invalid\n", __func__);
		goto exit;
	}

	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AI_PRIVATEINFO;
	rpc.private_info[0] = free_pin;
	rpc.private_info[1] = runtime->rate;
	rpc.private_info[2] = path;
	memcpy((char *)&rpc.private_info[3], config, sizeof(*config));

	if (rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO)) {
		pr_err("[audio rpc] %s set config fail\n", __func__);
		goto exit;
	}

	pr_info("[audio rpc] %s\n", __func__);
	return result.private_info[0];
exit:
	return -1;
}

int rpc_config_loopback_in(struct rtk_rpc_priv *priv, int ai_id)
{
	struct rpc_privateinfo_param rpc = {0};

	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AI_LOOPBACK_AO;
	rpc.private_info[0] |= 1 << ENUM_RPC_AI_LOOPBACK_FROM_AO_I2S;

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
}

int rpc_ai_connect_alsa(struct rtk_rpc_priv *priv,
			struct snd_pcm_runtime *runtime, int ai_id)
{
	struct rpc_privateinfo_param rpc = {0};
	int ai_format;

	switch (runtime->access) {
	case SNDRV_PCM_ACCESS_MMAP_INTERLEAVED:
	case SNDRV_PCM_ACCESS_RW_INTERLEAVED:
		switch (runtime->format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			ai_format = AUDIO_ALSA_FORMAT_16BITS_LE_LPCM;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			ai_format = AUDIO_ALSA_FORMAT_24BITS_LE_LPCM;
			break;
		case SNDRV_PCM_FORMAT_S24_3LE:
			ai_format = AUDIO_ALSA_FORMAT_24BITS_LE_LPCM;
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			ai_format = AUDIO_ALSA_FORMAT_32BITS_LE_LPCM;
			break;
		default:
			pr_err("[%s] unsupport format, 0x%x\n", __func__,
				runtime->format);
			return -1;
		}
		break;
	case SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED:
	case SNDRV_PCM_ACCESS_RW_NONINTERLEAVED:
	default:
		pr_err("[%s] unsupport access\n", __func__);
		return -1;
	}

	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AUDIO_AI_CONNECT_ALSA;
	rpc.private_info[0] = ai_format;
	rpc.private_info[1] = runtime->rate;
	rpc.private_info[3] = runtime->channels;

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_PRIVATEINFO);
}

int rpc_destroy_ai_flow(struct rtk_rpc_priv *priv, int ai_id, int configured)
{
	struct rpc_privateinfo_param rpc = {0};

	rpc.instance_id = ai_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_ALSA_DESTROY_AI_FLOW;
	if (configured == 0)
		rpc.private_info[0] = 0x23792379;

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
}

int rpc_get_global_ao(struct rtk_rpc_priv *priv, int *ao_id)
{
	struct rpc_privateinfo_param rpc = {0};
	struct rpc_privateinfo_result result = {0};
	int ret;
	rpc.type = ENUM_PRIVATEINFO_AUDIO_GET_GLOBAL_AO_INSTANCEID;
	pr_info("[audio rpc] %s\n", __func__);
	ret = rpc_send_private(priv, &rpc, &result,
			       ENUM_KERNEL_RPC_PRIVATEINFO);
	if (!ret)
		*ao_id = result.private_info[0];
	return ret;
}

int rpc_get_ao_flash_pin(struct rtk_rpc_priv *priv, int ao_id, int *pin)
{
	struct rpc_privateinfo_param rpc = {0};
	struct rpc_privateinfo_result result = {0};
	int i, ret;

	rpc.instance_id = ao_id;
	rpc.type = ENUM_PRIVATEINFO_AUDIO_GET_FLASH_PIN;
	for (i = 0; i < 6; i++)
		rpc.private_info[i] = 0xff;

	pr_info("[audio rpc] %s\n", __func__);
	ret = rpc_send_private(priv, &rpc, &result,
			       ENUM_KERNEL_RPC_PRIVATEINFO);
	if (!ret)
		*pin = result.private_info[0];

	return ret;
}

int rpc_get_chache_config(struct rtk_rpc_priv *priv, int *cacheable)
{
	struct rpc_privateinfo_param rpc = {0};
	struct rpc_privateinfo_result result = {0};
	int ret;

	pr_info("[audio rpc] %s\n", __func__);

	ret = rpc_send_private(priv, &rpc, &result, ENUM_KERNEL_RPC_GET_CACHE_CONFIG);

	if (!ret)
		*cacheable = result.private_info[0];

	return ret;
}

int rpc_put_ao_flash_pin(struct rtk_rpc_priv *priv, int ao_id, int *pin)
{
	struct rpc_privateinfo_param rpc = {0};
	int i, ret;

	rpc.instance_id = ao_id;
	rpc.type = ENUM_PRIVATEINFO_AUDIO_RELEASE_FLASH_PIN;
	rpc.private_info[0] = *pin;
	for (i = 1; i < 6; i++)
		rpc.private_info[i] = 0xff;

	pr_info("[audio rpc] %s\n", __func__);
	ret = rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_PRIVATEINFO);
	if (!ret)
		*pin = 0;
	return ret;
}

int rpc_set_ao_flash_volume(struct rtk_rpc_priv *priv, int ao_id, int pin, int volume)
{
	struct rpc_privateinfo_param rpc = {0};

	rpc.instance_id = ao_id;
	rpc.type = ENUM_PRIVATEINFO_AUDIO_CONTROL_FLASH_VOLUME;
	rpc.private_info[0] = pin;
	rpc.private_info[1] = 31 - volume;

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_PRIVATEINFO);
}

int rpc_set_ao_mixidx(struct rtk_rpc_priv *priv, int ao_id, int pin, int *idx)
{
	struct rpc_privateinfo_param rpc = {0};
	int ch;

	rpc.instance_id = ao_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AO_FLASH_PIN_MIX_CH_MAP;
	rpc.private_info[1] = pin;
	rpc.private_info[2] = 1;
	rpc.private_info[3] = sizeof(rpc);

	for (ch = 0; ch < MAX_CAR_CH; ch++)
		rpc.parameter[ch] = idx[ch];

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_PRIVATEINFO);
}

int rpc_config_ao(struct rtk_rpc_priv *priv, int ao_id, int pin, int *idx,
		  struct snd_pcm_runtime *runtime)
{
	struct rpc_privateinfo_param rpc = {0};
	int ch;

	rpc.instance_id = ao_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AO_FLASH_LPCM;
	rpc.private_info[1] = pin | ((runtime->sample_bits >> 3) << 8) |
				AUDIO_LITTLE_ENDIAN << 16;

	rpc.private_info[2] = runtime->rate;
	rpc_set_channel_map(runtime->channels, (char *)&rpc.private_info[3]);

	rpc.private_info[5] = (15 << 16);
	rpc.private_info[5] |= 20;

	if (idx) {
		rpc.private_info[7] = 1;
		rpc.private_info[8] = sizeof(rpc);
		for (ch = 0; ch < MAX_CAR_CH; ch++)
			rpc.parameter[ch] = idx[ch];
	} else
		rpc.private_info[7] = 0;

	pr_info("[audio rpc] %s\n", __func__);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
}

int rpc_ctrl_aio(struct rtk_rpc_priv *priv, int ao_id, struct rpc_aio_ctrl *ctrl)
{
	struct rpc_privateinfo_param rpc = {0};
	unsigned int ch;

	/* todo hard code private_info[3] now */
	rpc.instance_id = ao_id;
	rpc.type = ENUM_PRIVATEINFO_AIO_AO_INTERFACE_SWITCH_CONTROL;
	rpc.private_info[0] = ctrl->bitmap;
	if (ctrl->i2s_ch == 2)
		ch = 0;
	else if (ctrl->i2s_ch == 8)
		ch = 1;
	else if (ctrl->i2s_ch == 6)
		ch = 2;
	else
		return -1;
	rpc.private_info[1] = ch;
	rpc.private_info[2] = ctrl->i2s_mode;
	rpc.private_info[3] = AUDIO_OUT;

	pr_info("[audio rpc] %s, bitmap = 0x%x\n", __func__, ctrl->bitmap);
	return rpc_send_private(priv, &rpc, NULL, ENUM_KERNEL_RPC_AIO_PRIVATEINFO);
}

int rpc_ept_init(struct rtk_rpc_priv *priv)
{
	struct rpc_struct *info;
	int ret = 0;

	ret = krpc_info_init(priv->ept, "snd", rpc_hifi_cb);
	if (ret)
		return ret;

	info = &priv->data.info;

	info->programID = KERNELID;
	info->versionID = KERNELID;
	info->procedureID = 0;
	info->taskID = priv->ept->id;
	info->sysTID = 0;
	info->sysPID = 0;
	info->parameterSize = 3 * sizeof(u32);
	info->mycontext = 0;
	priv->data.param1 = priv->paddr;

	return ret;
}
