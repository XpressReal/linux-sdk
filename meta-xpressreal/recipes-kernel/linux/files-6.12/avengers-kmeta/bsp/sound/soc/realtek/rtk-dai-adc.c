// SPDX-License-Identifier: GPL-2.0
//
// RealTek ALSA SoC Audio DAI ADC Control
//
// Copyright (c) 2024 RealTek Inc.
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "rtk-afe-common.h"

enum {
	ENUM_ANALOG_IN1_PATH = 0,
	ENUM_ANALOG_IN2_PATH,
	ENUM_ANALOG_IN1_AGC,
	ENUM_ANALOG_IN2_AGC,
	ENUM_ANALOG_IN1_DGC,
	ENUM_ANALOG_IN2_DGC,
	ENUM_ANALOG_IN1_DIFFERENTIAL,
	ENUM_ANALOG_IN2_DIFFERENTIAL,
};

struct rtk_dai_adc_priv {
	struct rtk_snd_ringbuf *airing;
	struct rpc_analog_config config;
	unsigned int ai_id;
	int in_path[2];
};

#define RTK_ADC_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
			 SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S24_3LE)

#define RTK_ADC_RATES (SNDRV_PCM_RATE_8000_48000 | \
			SNDRV_PCM_RATE_88200 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | \
			SNDRV_PCM_RATE_192000)

static const struct snd_soc_dapm_route rtk_dai_adc_routes[] = {
	{"UL0 Source", "adc0", "ADC0_IN"},
	{"UL1 Source", "adc0", "ADC0_IN"},
	{"UL2 Source", "adc0", "ADC0_IN"},
	{"UL0 Source", "adc1", "ADC1_IN"},
	{"UL1 Source", "adc1", "ADC1_IN"},
	{"UL2 Source", "adc1", "ADC1_IN"},
};

static int rtk_adc_ctrl_info(struct snd_kcontrol *kcontrol,
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

static int rtk_adc_ctrl_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_adc_priv *priv;
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	priv = afe->dai_priv[kcontrol->id.device];

	if (kctrl->kctrl_num == ENUM_ANALOG_IN1_PATH ||
		kctrl->kctrl_num == ENUM_ANALOG_IN2_PATH) {
		ucontrol->value.integer.value[0] = priv->in_path[0];
	} else if (kctrl->kctrl_num == ENUM_ANALOG_IN1_AGC ||
		kctrl->kctrl_num == ENUM_ANALOG_IN2_AGC) {
		ucontrol->value.integer.value[0] = priv->config.analog_gain_l;
		ucontrol->value.integer.value[1] = priv->config.analog_gain_r;
	} else if (kctrl->kctrl_num == ENUM_ANALOG_IN1_DGC ||
		kctrl->kctrl_num == ENUM_ANALOG_IN2_DGC) {
		ucontrol->value.integer.value[0] = priv->config.digital_gain_en;
		ucontrol->value.integer.value[1] = priv->config.digital_gain_l;
		ucontrol->value.integer.value[2] = priv->config.digital_gain_r;
	} else if (kctrl->kctrl_num == ENUM_ANALOG_IN1_DIFFERENTIAL ||
		kctrl->kctrl_num == ENUM_ANALOG_IN2_DIFFERENTIAL) {
		ucontrol->value.integer.value[0] = priv->config.adc_differential_l;
		ucontrol->value.integer.value[1] = priv->config.adc_differential_r;
	}

	return 0;
}

static int rtk_adc_ctrl_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_adc_priv *priv;
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;

	priv = afe->dai_priv[kcontrol->id.device];

	if (kctrl->kctrl_num == ENUM_ANALOG_IN1_PATH ||
		kctrl->kctrl_num == ENUM_ANALOG_IN2_PATH) {
		priv->in_path[0] = ucontrol->value.integer.value[0];
	} else if (kctrl->kctrl_num == ENUM_ANALOG_IN1_AGC ||
		kctrl->kctrl_num == ENUM_ANALOG_IN2_AGC) {
		priv->config.analog_gain_l = ucontrol->value.integer.value[0];
		priv->config.analog_gain_r = ucontrol->value.integer.value[1];
	} else if (kctrl->kctrl_num == ENUM_ANALOG_IN1_DGC ||
		kctrl->kctrl_num == ENUM_ANALOG_IN2_DGC) {
		priv->config.digital_gain_en = ucontrol->value.integer.value[0];
		priv->config.digital_gain_l = ucontrol->value.integer.value[1];
		priv->config.digital_gain_r = ucontrol->value.integer.value[2];
	} else if (kctrl->kctrl_num == ENUM_ANALOG_IN1_DIFFERENTIAL ||
		kctrl->kctrl_num == ENUM_ANALOG_IN2_DIFFERENTIAL) {
		priv->config.adc_differential_l = ucontrol->value.integer.value[0];
		priv->config.adc_differential_r = ucontrol->value.integer.value[1];
	}
	return 0;
}

#define RTK_SOC_ADC_CONTROL(xname, xnum, xcount, xmin, xmax, \
				id) \
	RTK_SOC_DAI_CONTROL(xname, xnum, xcount, xmin, xmax, \
				rtk_adc_ctrl_info, \
				rtk_adc_ctrl_get, \
				rtk_adc_ctrl_put, \
				id)

static const struct snd_kcontrol_new rtk_dai_adc_controls[] = {
	RTK_SOC_ADC_CONTROL("Analog In1 Path", ENUM_ANALOG_IN1_PATH,
			1, 0, 3, RTK_DAI_ADC_0),
	RTK_SOC_ADC_CONTROL("Analog In2 Path", ENUM_ANALOG_IN2_PATH,
			1, 0, 3, RTK_DAI_ADC_1),
	RTK_SOC_ADC_CONTROL("Analog In1 ADC Analog Gain", ENUM_ANALOG_IN1_AGC,
			2, 0, 3, RTK_DAI_ADC_0),
	RTK_SOC_ADC_CONTROL("Analog In2 ADC Analog Gain", ENUM_ANALOG_IN2_AGC,
			2, 0, 3, RTK_DAI_ADC_1),
	RTK_SOC_ADC_CONTROL("Analog In1 ADC Digital Gain", ENUM_ANALOG_IN1_DGC,
			3, 0, 255, RTK_DAI_ADC_0),
	RTK_SOC_ADC_CONTROL("Analog In2 ADC Digital Gain", ENUM_ANALOG_IN2_DGC,
			3, 0, 255, RTK_DAI_ADC_1),
	RTK_SOC_ADC_CONTROL("Analog In1 Differential Enable",
			ENUM_ANALOG_IN1_DIFFERENTIAL,
			2, 0, 1, RTK_DAI_ADC_0),
	RTK_SOC_ADC_CONTROL("Analog In2 Differential Enable",
			ENUM_ANALOG_IN2_DIFFERENTIAL,
			2, 0, 1, RTK_DAI_ADC_1),
};

static void rtk_runtime_priv_free(struct snd_pcm_runtime *runtime)
{
	kfree(runtime->private_data);
	runtime->private_data = NULL;
}

static int rtk_dai_adc_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_dai_adc_priv *priv;
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

static int rtk_dai_adc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	return 0;
}

static int rtk_dai_adc_hw_free(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_dai_adc_priv *priv;

	priv = afe->dai_priv[dai->id];
	priv->in_path[1] = -1;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		rtk_snd_free_ringbuf(&priv->airing);

	return 0;
}

static int rtk_dai_adc_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rpc_ringbuffer_header ring_header = {0};
	struct rtk_dai_adc_priv *priv;
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

		ret = rpc_config_adc_in(&afe->rpc_priv, runtime, priv->ai_id,
					priv->in_path[0], &priv->config);
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

static const struct snd_soc_dai_ops rtk_dai_adc_ops = {
	.startup	= rtk_dai_adc_startup,
	.hw_params	= rtk_dai_adc_hw_params,
	.hw_free	= rtk_dai_adc_hw_free,
	.prepare	= rtk_dai_adc_prepare,
};

static struct snd_soc_dai_driver rtk_dai_adc_driver[] = {
	{
		.name = "ADC0_IN",
		.id = RTK_DAI_ADC_0,
		.capture = {
			.stream_name = "ADC0_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_ADC_RATES,
			.formats = RTK_ADC_FORMATS,
		},
		.ops = &rtk_dai_adc_ops,
	},
	{
		.name = "ADC1_IN",
		.id = RTK_DAI_ADC_1,
		.capture = {
			.stream_name = "ADC1_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_ADC_RATES,
			.formats = RTK_ADC_FORMATS,
		},
		.ops = &rtk_dai_adc_ops,
	},
};

static int init_adc_priv_data(struct rtk_afe_priv *afe)
{
	struct rtk_dai_adc_priv *priv;
	int i;

	for (i = RTK_DAI_ADC_START; i < RTK_DAI_ADC_END; i++) {
		priv = devm_kzalloc(afe->dev, sizeof(struct rtk_dai_adc_priv),
				    GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		afe->dai_priv[i] = priv;
	}
	return 0;
}

int rtk_dai_adc_register(struct rtk_afe_priv *afe)
{
	struct rtk_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = rtk_dai_adc_driver;
	dai->num_dai_drivers = ARRAY_SIZE(rtk_dai_adc_driver);
	dai->dapm_routes = rtk_dai_adc_routes;
	dai->num_dapm_routes = ARRAY_SIZE(rtk_dai_adc_routes);
	dai->controls = rtk_dai_adc_controls;
	dai->num_controls = ARRAY_SIZE(rtk_dai_adc_controls);

	return init_adc_priv_data(afe);
}
