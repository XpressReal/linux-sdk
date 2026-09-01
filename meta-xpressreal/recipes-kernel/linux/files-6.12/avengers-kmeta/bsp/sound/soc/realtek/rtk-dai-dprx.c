// SPDX-License-Identifier: GPL-2.0
//
// RealTek ALSA SoC Audio DAI DPRX Control
//
// Copyright (c) 2024 RealTek Inc.
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "rtk-afe-common.h"

enum {
	ENUM_DPRX_AUDIO_EN,
};

#define RTK_DPRX_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
				SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE | \
				SNDRV_PCM_FMTBIT_S24_3LE)

#define RTK_DPRX_RATES (SNDRV_PCM_RATE_8000_48000 | \
			SNDRV_PCM_RATE_88200 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | \
			SNDRV_PCM_RATE_192000)

struct rtk_dai_dprx_priv {
	unsigned int id;
	unsigned int ao_id;
	unsigned int pin_id;
	unsigned int ai_id;
	unsigned int dec_id;
	unsigned int en;

	struct rtk_snd_ringbuf *airing;
	struct rtk_snd_ringbuf *decring;
	struct rtk_snd_ringbuf *icqring;
	struct rtk_snd_ringbuf *aoring;
};

static const struct snd_soc_dapm_route rtk_dai_dprx_routes[] = {
	{"UL0 Source", "dprx", "DPRX_IN"},
	{"UL1 Source", "dprx", "DPRX_IN"},
	{"UL2 Source", "dprx", "DPRX_IN"},
};

static int rtk_dprx_ctrl_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	if (kctrl->min == 0 && kctrl->max == 1)
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = kctrl->count;
	uinfo->value.integer.min = kctrl->min;
	uinfo->value.integer.max = kctrl->max;

	return 0;
}

static int rtk_dprx_ctrl_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_dprx_priv *priv;
	struct rtk_dai_kctrl __maybe_unused *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	priv = afe->dai_priv[kcontrol->id.device];

	ucontrol->value.integer.value[0] = priv->en;

	return 0;
}
static int rtk_dprx_ctrl_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_dprx_priv *priv;
	struct rtk_dai_kctrl __maybe_unused *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;
	struct rpc_ringbuffer_header ring_header[2] = {0};
	int ret, pin;

	priv = afe->dai_priv[kcontrol->id.device];

	priv->en = ucontrol->value.integer.value[0];

	if (!priv->en) {
		rpc_pause_svc(&afe->rpc_priv, priv->ao_id | priv->pin_id);
		rpc_stop_svc(&afe->rpc_priv, priv->ao_id | priv->pin_id);
		rpc_pause_svc(&afe->rpc_priv, priv->dec_id);
		rpc_stop_svc(&afe->rpc_priv, priv->dec_id);
		rpc_pause_svc(&afe->rpc_priv, priv->ai_id);
		rpc_stop_svc(&afe->rpc_priv, priv->ai_id);

		rpc_destroy_svc(&afe->rpc_priv, priv->dec_id);
		rpc_destroy_svc(&afe->rpc_priv, priv->ai_id);
		rpc_put_ao_flash_pin(&afe->rpc_priv, priv->ao_id, &priv->pin_id);

		rtk_snd_free_ringbuf(&priv->airing);
		rtk_snd_free_ringbuf(&priv->decring);
		rtk_snd_free_ringbuf(&priv->icqring);
		rtk_snd_free_ringbuf(&priv->aoring);

		return 0;
	}

	rpc_create_audio_agent(&afe->rpc_priv, AUDIO_IN, &priv->ai_id);
	rpc_create_audio_agent(&afe->rpc_priv, AUDIO_DECODER, &priv->dec_id);
	rpc_get_global_ao(&afe->rpc_priv, &priv->ao_id);
	ret = rpc_get_ao_flash_pin(&afe->rpc_priv, priv->ao_id, &pin);
	if (ret || pin < FLASH_AUDIO_PIN_1 || pin > FLASH_AUDIO_PIN_8) {
		dev_err(afe->dev, "[%s] flash pin get fail, %d\n", __func__, pin);
		return -1;
	}
	priv->pin_id = pin;

	ring_header[0].instance_id = priv->ai_id;
	ring_header[0].pin_id = PCM_OUT;
	ring_header[0].read_idx = 0;
	ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_DEC_OUT_BYTE_SIZE,
				      8,
				      &priv->airing, &ring_header[0],
				      1, RTK_ACPU_FLAGS);
	if (ret)
		goto err_alloc_ringbuf;

	ring_header[0].instance_id = priv->ai_id;
	ring_header[0].pin_id = NON_PCM_IN;
	ring_header[1].instance_id = priv->dec_id;
	ring_header[1].pin_id = BASE_BS_IN;
	ring_header[1].read_idx = 0;
	ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_NON_PCM_BUFFER_SIZE, 1,
				      &priv->decring, &ring_header[0],
				      2, RTK_ACPU_FLAGS);
	if (ret)
		goto err_alloc_ringbuf;

	ring_header[0].pin_id = DWNSTRM_INBAND_QUEUE;
	ring_header[1].instance_id = priv->dec_id;
	ring_header[1].pin_id = INBAND_QUEUE;
	ring_header[1].read_idx = 0;
	ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_DEC_ICQ_SIZE, 1,
				      &priv->icqring, &ring_header[0],
				      2, RTK_ACPU_FLAGS);
	if (ret)
		goto err_alloc_ringbuf;

	ring_header[0].instance_id = priv->dec_id;
	ring_header[0].pin_id = PCM_OUT;
	ring_header[1].instance_id = priv->ao_id;
	ring_header[1].pin_id = priv->pin_id;
	ring_header[1].read_idx = 0;
	ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_DEC_OUT_BYTE_SIZE, 8,
				      &priv->aoring, &ring_header[0],
				      2, RTK_ACPU_FLAGS);
	if (ret)
		goto err_alloc_ringbuf;

	rpc_connect_svc(&afe->rpc_priv, priv->ai_id, PCM_OUT, priv->dec_id, BASE_BS_IN);
	rpc_connect_svc(&afe->rpc_priv, priv->dec_id, PCM_OUT, priv->ao_id, priv->pin_id);
	ret = rpc_config_dprx_in(&afe->rpc_priv, priv->ai_id, priv->ao_id, priv->pin_id);
	if (ret)
		goto err_send_rpc;

	rpc_run_svc(&afe->rpc_priv, priv->ao_id | priv->pin_id);
	rpc_run_svc(&afe->rpc_priv, priv->dec_id);
	rpc_run_svc(&afe->rpc_priv, priv->ai_id);

err_alloc_ringbuf:
err_send_rpc:

	return 0;
}

#define RTK_SOC_DPRX_CONTROL(xname, xnum, xcount, xmin, xmax, \
				id) \
	RTK_SOC_DAI_CONTROL(xname, xnum, xcount, xmin, xmax, \
				rtk_dprx_ctrl_info, \
				rtk_dprx_ctrl_get, \
				rtk_dprx_ctrl_put, \
				id)

static const struct snd_kcontrol_new rtk_dai_dprx_controls[] = {
	RTK_SOC_DPRX_CONTROL("DPRX Audio En", ENUM_DPRX_AUDIO_EN,
			1, 0, 1, RTK_DAI_DPRX),
};

static void rtk_runtime_priv_free(struct snd_pcm_runtime *runtime)
{
	kfree(runtime->private_data);
	runtime->private_data = NULL;
}

static int rtk_dai_dprx_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_dai_dprx_priv *priv;
	struct rtk_pcm_runtime_priv *pcm_priv;
	int pin, ret = 0;

	priv = afe->dai_priv[dai->id];

	pcm_priv = kzalloc(sizeof(*pcm_priv), GFP_KERNEL);
	if (!pcm_priv)
		return -ENOMEM;
	runtime->private_data = pcm_priv;
	runtime->private_free = rtk_runtime_priv_free;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		rpc_create_audio_agent(&afe->rpc_priv, AUDIO_IN, &priv->ai_id);
		rpc_create_audio_agent(&afe->rpc_priv, AUDIO_DECODER, &priv->dec_id);
		rpc_get_global_ao(&afe->rpc_priv, &priv->ao_id);
		ret = rpc_get_ao_flash_pin(&afe->rpc_priv, priv->ao_id, &pin);
		if (ret || pin < FLASH_AUDIO_PIN_1 || pin > FLASH_AUDIO_PIN_8) {
			dev_err(afe->dev, "[%s] flash pin get fail, %d\n", __func__, pin);
			return -1;
		}
		priv->pin_id = pin;
		pcm_priv->ai_id = priv->ai_id;
	}

	return 0;
}
static int rtk_dai_dprx_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	return 0;
}

static int rtk_dai_dprx_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_dai_dprx_priv *priv;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		rpc_pause_svc(&afe->rpc_priv, priv->ao_id | priv->pin_id);
		rpc_stop_svc(&afe->rpc_priv, priv->ao_id | priv->pin_id);
		rpc_pause_svc(&afe->rpc_priv, priv->dec_id);
		rpc_stop_svc(&afe->rpc_priv, priv->dec_id);
		rpc_destroy_svc(&afe->rpc_priv, priv->dec_id);
		rpc_put_ao_flash_pin(&afe->rpc_priv, priv->ao_id, &priv->pin_id);
		rtk_snd_free_ringbuf(&priv->airing);
		rtk_snd_free_ringbuf(&priv->decring);
		rtk_snd_free_ringbuf(&priv->icqring);
		rtk_snd_free_ringbuf(&priv->aoring);
	}

	return 0;
}
static int rtk_dai_dprx_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rpc_ringbuffer_header ring_header[2] = {0};
	struct rtk_dai_dprx_priv *priv;
	int ret;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		ring_header[0].instance_id = priv->ai_id;
		ring_header[0].pin_id = PCM_OUT;
		ring_header[0].read_idx = 0;
		ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_DEC_BS_SIZE,
					      runtime->channels,
					      &priv->airing, &ring_header[0],
					      1, RTK_ACPU_FLAGS);
		if (ret)
			goto err_alloc_ringbuf;

		ring_header[0].instance_id = priv->ai_id;
		ring_header[0].pin_id = NON_PCM_IN;
		ring_header[1].instance_id = priv->dec_id;
		ring_header[1].pin_id = BASE_BS_IN;
		ring_header[1].read_idx = 0;
		ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_NON_PCM_BUFFER_SIZE, 1,
					      &priv->decring, &ring_header[0],
					      2, RTK_ACPU_FLAGS);
		if (ret)
			goto err_alloc_ringbuf;

		ring_header[0].pin_id = DWNSTRM_INBAND_QUEUE;
		ring_header[1].instance_id = priv->dec_id;
		ring_header[1].pin_id = INBAND_QUEUE;
		ring_header[1].read_idx = 0;
		ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_DEC_ICQ_SIZE, 1,
					      &priv->icqring, &ring_header[0],
					      2, RTK_ACPU_FLAGS);
		if (ret)
			goto err_alloc_ringbuf;

		ring_header[0].instance_id = priv->dec_id;
		ring_header[0].pin_id = PCM_OUT;
		ring_header[1].instance_id = priv->ao_id;
		ring_header[1].pin_id = priv->pin_id;
		ring_header[1].read_idx = 0;
		ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_DEC_OUT_BYTE_SIZE, 8,
					      &priv->aoring, &ring_header[0],
					      2, RTK_ACPU_FLAGS);
		if (ret)
			goto err_alloc_ringbuf;

		rpc_connect_svc(&afe->rpc_priv, priv->ai_id, PCM_OUT, priv->dec_id, BASE_BS_IN);
		rpc_connect_svc(&afe->rpc_priv, priv->dec_id, PCM_OUT, priv->ao_id, priv->pin_id);
		ret = rpc_config_dprx_in(&afe->rpc_priv, priv->ai_id, priv->ao_id, priv->pin_id);
		if (ret)
			goto err_send_rpc;

		rpc_run_svc(&afe->rpc_priv, priv->ao_id | priv->pin_id);
		rpc_run_svc(&afe->rpc_priv, priv->dec_id);
	}
	return 0;

err_send_rpc:
err_alloc_ringbuf:
	rtk_snd_free_ringbuf(&priv->airing);
	return ret;
}

static const struct snd_soc_dai_ops rtk_dai_dprx_ops = {
	.startup	= rtk_dai_dprx_startup,
	.hw_params	= rtk_dai_dprx_hw_params,
	.hw_free	= rtk_dai_dprx_hw_free,
	.prepare	= rtk_dai_dprx_prepare,
};

static struct snd_soc_dai_driver rtk_dai_dprx_driver[] = {
	{
		.name = "DPRX_IN",
		.id = RTK_DAI_DPRX,
		.capture = {
			.stream_name = "DPRX_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_DPRX_RATES,
			.formats = RTK_DPRX_FORMATS,
		},
		.ops = &rtk_dai_dprx_ops,
	},
};

static int init_dprx_priv_data(struct rtk_afe_priv *afe)
{
	struct rtk_dai_dprx_priv *priv;

	priv = devm_kzalloc(afe->dev,
			    sizeof(struct rtk_dai_dprx_priv),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	afe->dai_priv[RTK_DAI_DPRX] = priv;

	return 0;
}

int rtk_dai_dprx_register(struct rtk_afe_priv *afe)
{
	struct rtk_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = rtk_dai_dprx_driver;
	dai->num_dai_drivers = ARRAY_SIZE(rtk_dai_dprx_driver);
	dai->dapm_routes = rtk_dai_dprx_routes;
	dai->num_dapm_routes = ARRAY_SIZE(rtk_dai_dprx_routes);
	dai->controls = rtk_dai_dprx_controls;
	dai->num_controls = ARRAY_SIZE(rtk_dai_dprx_controls);

	return init_dprx_priv_data(afe);
}

