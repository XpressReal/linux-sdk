// SPDX-License-Identifier: GPL-2.0
//
// RealTek ALSA SoC Audio DAI I2S Control
//
// Copyright (c) 2024 RealTek Inc.
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "rtk-afe-common.h"

#define RTK_I2S_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
				SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE | \
				SNDRV_PCM_FMTBIT_S24_3LE)

#define RTK_I2S_RATES (SNDRV_PCM_RATE_8000_48000 | \
			SNDRV_PCM_RATE_24000 | \
			SNDRV_PCM_RATE_88200 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | \
			SNDRV_PCM_RATE_192000)

#define RTK_STI2S_RATES (SNDRV_PCM_RATE_8000_768000 | \
			SNDRV_PCM_RATE_24000)

enum {
	ENUM_I2S0_IN_MODE,
	ENUM_I2S1_IN_MODE,
};

struct rtk_dai_i2s_priv {
	unsigned int ai_id;
	unsigned int mode;

	struct rtk_snd_ringbuf *airing;
};

static const struct snd_soc_dapm_route rtk_dai_i2s_routes[] = {
	{"UL0 Source", "i2s0", "I2S0_IN"},
	{"UL1 Source", "i2s0", "I2S0_IN"},
	{"UL2 Source", "i2s0", "I2S0_IN"},
	{"UL0 Source", "i2s1", "I2S1_IN"},
	{"UL1 Source", "i2s1", "I2S1_IN"},
	{"UL2 Source", "i2s1", "I2S1_IN"},
	{"UL0 Source", "sti2s0", "STI2S0_IN"},
	{"UL1 Source", "sti2s0", "STI2S0_IN"},
	{"UL2 Source", "sti2s0", "STI2S0_IN"},
	{"UL0 Source", "sti2s1", "STI2S1_IN"},
	{"UL1 Source", "sti2s1", "STI2S1_IN"},
	{"UL2 Source", "sti2s1", "STI2S1_IN"},
};

static int rtk_i2s_ctrl_info(struct snd_kcontrol *kcontrol,
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

static int rtk_i2s_ctrl_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_i2s_priv *priv;
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	priv = afe->dai_priv[kcontrol->id.device];

	if (kctrl->kctrl_num == ENUM_I2S0_IN_MODE ||
		kctrl->kctrl_num == ENUM_I2S1_IN_MODE)
		ucontrol->value.integer.value[0] = priv->mode;

	return 0;
}

static int rtk_i2s_ctrl_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_i2s_priv *priv;
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	priv = afe->dai_priv[kcontrol->id.device];

	if (kctrl->kctrl_num == ENUM_I2S0_IN_MODE ||
		kctrl->kctrl_num == ENUM_I2S1_IN_MODE)
		priv->mode = ucontrol->value.integer.value[0];

	return 0;
}

#define RTK_SOC_I2S_CONTROL(xname, xnum, xcount, xmin, xmax, \
				id) \
	RTK_SOC_DAI_CONTROL(xname, xnum, xcount, xmin, xmax, \
				rtk_i2s_ctrl_info, \
				rtk_i2s_ctrl_get, \
				rtk_i2s_ctrl_put, \
				id)

static const struct snd_kcontrol_new rtk_dai_i2s_controls[] = {
	RTK_SOC_I2S_CONTROL("Audio I2S0 In Mode", ENUM_I2S0_IN_MODE,
			1, 0, 3, RTK_DAI_I2S_0),
	RTK_SOC_I2S_CONTROL("Audio I2S1 In Mode", ENUM_I2S1_IN_MODE,
			1, 0, 3, RTK_DAI_I2S_1),
};

static void rtk_runtime_priv_free(struct snd_pcm_runtime *runtime)
{
	kfree(runtime->private_data);
	runtime->private_data = NULL;
}

static int rtk_dai_i2s_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_dai_i2s_priv *priv;
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
		if (dai->id == RTK_DAI_STI2S_0 || dai->id == RTK_DAI_STI2S_1)
			pcm_priv->no_lpcm = 1;
	}

	return 0;
}

static int rtk_dai_i2s_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	return 0;
}

static int rtk_dai_i2s_hw_free(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_dai_i2s_priv *priv;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		rtk_snd_free_ringbuf(&priv->airing);

	return 0;
}

static int rtk_dai_i2s_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rpc_ringbuffer_header ring_header = {0};
	struct rtk_dai_i2s_priv *priv;
	int ret;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		ring_header.instance_id = priv->ai_id;
		if (dai->id == RTK_DAI_STI2S_0 || dai->id == RTK_DAI_STI2S_1) {
			ret = rpc_config_sti2s_in(&afe->rpc_priv, priv->ai_id);
			if (ret)
				goto err_send_rpc;
			return 0;
		}
		ring_header.instance_id = priv->ai_id;
		ring_header.pin_id = PCM_OUT;
		ring_header.read_idx = -1;
		ret = rtk_snd_prepare_ringbuf(afe, NULL, RTK_ENC_AI_BUFFER_SIZE,
					      runtime->channels,
					      &priv->airing, &ring_header,
					      1, RTK_ACPU_FLAGS);
		if (ret)
			goto err_alloc_ringbuf;

		if (dai->id == RTK_DAI_I2S_0)
			ret = rpc_config_i2s_in(&afe->rpc_priv, priv->ai_id, 0, priv->mode);
		else if (dai->id == RTK_DAI_I2S_1)
			ret = rpc_config_i2s_in(&afe->rpc_priv, priv->ai_id, 1, priv->mode);
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

static const struct snd_soc_dai_ops rtk_dai_i2s_ops = {
	.startup	= rtk_dai_i2s_startup,
	.hw_params	= rtk_dai_i2s_hw_params,
	.hw_free	= rtk_dai_i2s_hw_free,
	.prepare	= rtk_dai_i2s_prepare,
};

static struct snd_soc_dai_driver rtk_dai_i2s_driver[] = {
	{
		.name = "I2S0",
		.id = RTK_DAI_I2S_0,
		.capture = {
			.stream_name = "I2S0_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_I2S_RATES,
			.formats = RTK_I2S_FORMATS,
		},
		.ops = &rtk_dai_i2s_ops,
	},
	{
		.name = "I2S1",
		.id = RTK_DAI_I2S_1,
		.capture = {
			.stream_name = "I2S1_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_I2S_RATES,
			.formats = RTK_I2S_FORMATS,
		},
		.ops = &rtk_dai_i2s_ops,
	},
	{
		.name = "STI2S0",
		.id = RTK_DAI_STI2S_0,
		.capture = {
			.stream_name = "STI2S0_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_STI2S_RATES,
			.formats = RTK_I2S_FORMATS,
		},
		.ops = &rtk_dai_i2s_ops,
	},
	{
		.name = "STI2S1",
		.id = RTK_DAI_STI2S_1,
		.capture = {
			.stream_name = "STI2S1_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_STI2S_RATES,
			.formats = RTK_I2S_FORMATS,
		},
		.ops = &rtk_dai_i2s_ops,
	},
};

static int init_i2s_priv_data(struct rtk_afe_priv *afe)
{
	struct rtk_dai_i2s_priv *i2s_priv;
	int i;

	for (i = RTK_DAI_I2S_START; i < RTK_DAI_I2S_END; i++) {
		i2s_priv = devm_kzalloc(afe->dev,
					sizeof(struct rtk_dai_i2s_priv),
					GFP_KERNEL);
		if (!i2s_priv)
			return -ENOMEM;

		afe->dai_priv[i] = i2s_priv;
	}
	return 0;
}

int rtk_dai_i2s_register(struct rtk_afe_priv *afe)
{
	struct rtk_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = rtk_dai_i2s_driver;
	dai->num_dai_drivers = ARRAY_SIZE(rtk_dai_i2s_driver);
	dai->dapm_routes = rtk_dai_i2s_routes;
	dai->num_dapm_routes = ARRAY_SIZE(rtk_dai_i2s_routes);
	dai->controls = rtk_dai_i2s_controls;
	dai->num_controls = ARRAY_SIZE(rtk_dai_i2s_controls);

	return init_i2s_priv_data(afe);
}
