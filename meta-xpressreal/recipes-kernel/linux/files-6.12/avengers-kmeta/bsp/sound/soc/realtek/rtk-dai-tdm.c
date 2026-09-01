// SPDX-License-Identifier: GPL-2.0
//
// RealTek ALSA SoC Audio DAI TDM Control
//
// Copyright (c) 2024 RealTek Inc.
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "rtk-afe-common.h"

#define RTK_TDM_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
			 SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S24_3LE)

#define RTK_TDM_RATES (SNDRV_PCM_RATE_8000_48000 | \
			SNDRV_PCM_RATE_88200 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | \
			SNDRV_PCM_RATE_192000)

enum {
	ENUM_TDM0_IN_MODE,
	ENUM_TDM1_IN_MODE,
	ENUM_TDM2_IN_MODE,
};

struct rtk_dai_tdm_priv {
	unsigned int id;
	unsigned int ai_id;
	unsigned int mode;
	struct rpc_tdmin_config config;
	struct rtk_snd_ringbuf *airing;
	struct rtk_snd_ringbuf *packring;
};

static const struct snd_soc_dapm_route rtk_dai_tdm_routes[] = {
	{"UL0 Source", "tdm0", "TDM0_IN"},
	{"UL1 Source", "tdm0", "TDM0_IN"},
	{"UL2 Source", "tdm0", "TDM0_IN"},
	{"UL0 Source", "tdm1", "TDM1_IN"},
	{"UL1 Source", "tdm1", "TDM1_IN"},
	{"UL2 Source", "tdm1", "TDM1_IN"},
	{"UL0 Source", "tdm2", "TDM2_IN"},
	{"UL1 Source", "tdm2", "TDM2_IN"},
	{"UL2 Source", "tdm2", "TDM2_IN"},
};

static int rtk_tdm_ctrl_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = kctrl->count;
	uinfo->value.integer.min = kctrl->min;
	uinfo->value.integer.max = kctrl->max;

	return 0;
}

static int rtk_tdm_ctrl_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_tdm_priv *priv;
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	priv = afe->dai_priv[kcontrol->id.device];

	if (kctrl->kctrl_num == ENUM_TDM0_IN_MODE ||
		kctrl->kctrl_num == ENUM_TDM1_IN_MODE ||
		kctrl->kctrl_num == ENUM_TDM2_IN_MODE)
		ucontrol->value.integer.value[0] = priv->mode;

	return 0;
}

static int rtk_tdm_ctrl_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_tdm_priv *priv;
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	priv = afe->dai_priv[kcontrol->id.device];

	if (kctrl->kctrl_num == ENUM_TDM0_IN_MODE ||
		kctrl->kctrl_num == ENUM_TDM1_IN_MODE ||
		kctrl->kctrl_num == ENUM_TDM2_IN_MODE)
		priv->mode = ucontrol->value.integer.value[0];

	return 0;
}

#define RTK_SOC_TDM_CONTROL(xname, xnum, xcount, xmin, xmax, \
				id) \
	RTK_SOC_DAI_CONTROL(xname, xnum, xcount, xmin, xmax, \
				rtk_tdm_ctrl_info, \
				rtk_tdm_ctrl_get, \
				rtk_tdm_ctrl_put, \
				id)

static const struct snd_kcontrol_new rtk_dai_tdm_controls[] = {
	RTK_SOC_TDM_CONTROL("Audio TDM0 In Mode", ENUM_TDM0_IN_MODE,
			1, 0, 3, RTK_DAI_TDM_0),
	RTK_SOC_TDM_CONTROL("Audio TDM1 In Mode", ENUM_TDM1_IN_MODE,
			1, 0, 3, RTK_DAI_TDM_1),
	RTK_SOC_TDM_CONTROL("Audio TDM2 In Mode", ENUM_TDM2_IN_MODE,
			1, 0, 3, RTK_DAI_TDM_2),
};

static void rtk_runtime_priv_free(struct snd_pcm_runtime *runtime)
{
	kfree(runtime->private_data);
	runtime->private_data = NULL;
}

static int rtk_dai_tdm_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_dai_tdm_priv *priv;
	struct rtk_pcm_runtime_priv *pcm_priv;

	priv = afe->dai_priv[dai->id];

	pcm_priv = kzalloc(sizeof(*pcm_priv), GFP_KERNEL);
	if (!pcm_priv)
		return -ENOMEM;
	runtime->private_data = pcm_priv;
	runtime->private_free = rtk_runtime_priv_free;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		rpc_create_audio_agent(&afe->rpc_priv, AUDIO_IN, &priv->ai_id);

		pcm_priv->ai_id = priv->ai_id;
	}

	return 0;
}

static int rtk_dai_tdm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	return 0;
}

static int rtk_dai_tdm_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_dai_tdm_priv *priv;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		rtk_snd_free_ringbuf(&priv->airing);
		rtk_snd_free_ringbuf(&priv->packring);
	}

	return 0;
}

static int rtk_dai_tdm_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rpc_ringbuffer_header ring_header = {0};
	struct rtk_dai_tdm_priv *priv;
	int ret;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		ring_header.instance_id = priv->ai_id;
		ring_header.pin_id = PCM_OUT;
		ring_header.read_idx = -1;
		ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_ENC_AI_BUFFER_SIZE,
					      runtime->channels,
					      &priv->airing, &ring_header, 1,
					      RTK_ACPU_FLAGS);
		if (ret)
			goto err_alloc_ringbuf;

		ring_header.pin_id = PCM_PACKED_IN;
		ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_PACK_AI_BUFFER_SIZE,
					      1, &priv->packring, &ring_header, 1,
					      RTK_ACPU_FLAGS);
		if (ret)
			goto err_alloc_ringbuf;

		priv->config.slot = dai->id - RTK_DAI_TDM_START;
		priv->config.sample_rate = runtime->rate;
		priv->config.channel_num = runtime->channels;
		if (!priv->mode) {
			pr_err("enable pin share mode\n");
			priv->config.slave_mode = 1;
			priv->config.pin_share = ENUM_TDM_PIN_SHARE_AO_TDM_0 +
						 priv->config.slot;
		} else
			priv->config.slave_mode = priv->config.pin_share = 0;
		ret = rpc_config_tdm_in(&afe->rpc_priv, priv->ai_id, &priv->config);
		if (ret)
			goto err_send_rpc;

		ret = rpc_ai_connect_alsa(&afe->rpc_priv, runtime, priv->ai_id);
		if (ret)
			goto err_send_rpc;
	}
	return 0;

err_send_rpc:
err_alloc_ringbuf:
	rtk_snd_free_ringbuf(&priv->airing);
	return ret;
}

static const struct snd_soc_dai_ops rtk_dai_tdm_ops = {
	.startup	= rtk_dai_tdm_startup,
	.hw_params	= rtk_dai_tdm_hw_params,
	.hw_free	= rtk_dai_tdm_hw_free,
	.prepare	= rtk_dai_tdm_prepare,
};

static struct snd_soc_dai_driver rtk_dai_tdm_driver[] = {
	{
		.name = "TDM0_IN",
		.id = RTK_DAI_TDM_0,
		.capture = {
			.stream_name = "TDM0_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_TDM_RATES,
			.formats = RTK_TDM_FORMATS,
		},
		.ops = &rtk_dai_tdm_ops,
	},
	{
		.name = "TDM1_IN",
		.id = RTK_DAI_TDM_1,
		.capture = {
			.stream_name = "TDM1_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_TDM_RATES,
			.formats = RTK_TDM_FORMATS,
		},
		.ops = &rtk_dai_tdm_ops,
	},
	{
		.name = "TDM2_IN",
		.id = RTK_DAI_TDM_2,
		.capture = {
			.stream_name = "TDM2_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_TDM_RATES,
			.formats = RTK_TDM_FORMATS,
		},
		.ops = &rtk_dai_tdm_ops,
	},
};

static int init_tdm_priv_data(struct rtk_afe_priv *afe)
{
	struct rtk_dai_tdm_priv *priv;
	int i;

	for (i = RTK_DAI_TDM_START; i < RTK_DAI_TDM_END; i++) {
		priv = devm_kzalloc(afe->dev,
					sizeof(struct rtk_dai_tdm_priv),
					GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		afe->dai_priv[i] = priv;
	}

	return 0;
}

int rtk_dai_tdm_register(struct rtk_afe_priv *afe)
{
	struct rtk_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = rtk_dai_tdm_driver;
	dai->num_dai_drivers = ARRAY_SIZE(rtk_dai_tdm_driver);
	dai->dapm_routes = rtk_dai_tdm_routes;
	dai->num_dapm_routes = ARRAY_SIZE(rtk_dai_tdm_routes);
	dai->controls = rtk_dai_tdm_controls;
	dai->num_controls = ARRAY_SIZE(rtk_dai_tdm_controls);

	return init_tdm_priv_data(afe);
}
