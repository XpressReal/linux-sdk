// SPDX-License-Identifier: GPL-2.0
//
// RealTek ALSA SoC Audio DAI AIO Control
//
// Copyright (c) 2024 RealTek Inc.
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "rtk-afe-common.h"

#define RTK_AIO_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
			SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S24_3LE)

#define RTK_AIO_RATES (SNDRV_PCM_RATE_8000_48000 | \
			SNDRV_PCM_RATE_88200 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | \
			SNDRV_PCM_RATE_192000)


static const char * const ao_lb_src_sel_mux_text[] = {
	"i2s", "spdif",
};

static SOC_ENUM_SINGLE_DECL(ao_lb_src_sel_mux_enum,
	SND_SOC_NOPM, 0, ao_lb_src_sel_mux_text);

static const struct snd_kcontrol_new ao_lb_src_sel_mux =
	SOC_DAPM_ENUM("ao lb src sel", ao_lb_src_sel_mux_enum);

static const struct snd_soc_dapm_widget rtk_dai_aio_widgets[] = {
	SND_SOC_DAPM_MUX("AO Loopback Source", SND_SOC_NOPM, 0, 0, &ao_lb_src_sel_mux),
};

static const struct snd_soc_dapm_route rtk_dai_aio_routes[] = {
	{ "AUDIO_OUT0", NULL, "DL0" },
	{ "AUDIO_OUT0", NULL, "DL1" },
	{ "AUDIO_OUT0", NULL, "DL2" },
	{ "AUDIO_OUT0", NULL, "DL3" },

	{ "UL0 Source", "ao_lb", "AO Loopback Source" },
	{ "UL1 Source", "ao_lb", "AO Loopback Source" },
	{ "UL2 Source", "ao_lb", "AO Loopback Source" },

	{ "AO Loopback Source", "i2s", "I2S0_LOOPBACK" },
};

static int rtk_aio_ctrl_info(struct snd_kcontrol *kcontrol,
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

static int rtk_aio_ctrl_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;
	struct rtk_dai_aio_priv *priv;
	struct rpc_aio_ctrl *ctrl;
	int value;

	priv = afe->dai_priv[kcontrol->id.device];
	ctrl = &priv->ctrl;

	value = (ctrl->bitmap & (1 << kctrl->kctrl_num)) >> kctrl->kctrl_num;
	ucontrol->value.integer.value[0] = !value;

	return 0;
};

static int rtk_aio_ctrl_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;
	struct rtk_dai_aio_priv *priv;
	struct rpc_aio_ctrl *ctrl;
	int value;

	priv = afe->dai_priv[kcontrol->id.device];
	ctrl = &priv->ctrl;

	value = (~ctrl->bitmap & (1 << kctrl->kctrl_num)) >> kctrl->kctrl_num;
	if (value != ucontrol->value.integer.value[0]) {
		value = ctrl->bitmap & ~(1 << kctrl->kctrl_num);
		value |= (!ucontrol->value.integer.value[0]) << kctrl->kctrl_num;
		ctrl->bitmap = value;
		return rpc_ctrl_aio(&afe->rpc_priv, priv->ao_id, ctrl);
	}

	return 0;
}

#define RTK_SOC_AIO_CONTROL(xname, xnum, xcount, xmin, xmax, \
				id) \
	RTK_SOC_DAI_CONTROL(xname, xnum, xcount, xmin, xmax, \
				rtk_aio_ctrl_info, \
				rtk_aio_ctrl_get, \
				rtk_aio_ctrl_put, \
				id)

static const struct snd_kcontrol_new rtk_dai_aio_controls[] = {
	RTK_SOC_AIO_CONTROL("DAC Audio Out", ENUM_DT_AO_DAC, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("I2S Audio Out", ENUM_DT_AO_I2S, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("SPDIF Audio Out", ENUM_DT_AO_SPDIF, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("HDMI Audio Out", ENUM_DT_AO_HDMI, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("TDM Audio Out", ENUM_DT_AO_TDM, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("TDM1 Audio Out", ENUM_DT_AO_TDM1, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("TDM2 Audio Out", ENUM_DT_AO_TDM2, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("I2S1 Audio Out", ENUM_DT_AO_I2S1, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("I2S2 Audio Out", ENUM_DT_AO_I2S2, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
	RTK_SOC_AIO_CONTROL("BTPCM Audio Out", ENUM_DT_AO_BTPCM, 1, 0, 1, RTK_DAI_AUDIO_OUT0),
};

static int rtk_dai_aio_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	return 0;
}

static int rtk_dai_aio_hw_free(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_dai_aio_priv *priv;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		rtk_snd_free_ringbuf(&priv->airing);

	return 0;
}

static void rtk_runtime_priv_free(struct snd_pcm_runtime *runtime)
{
	kfree(runtime->private_data);
	runtime->private_data = NULL;
}

static int rtk_dai_aio_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_dai_aio_priv *priv;
	struct rtk_pcm_runtime_priv *pcm_priv;

	priv = afe->dai_priv[dai->id];

	pcm_priv = kzalloc(sizeof(*pcm_priv), GFP_KERNEL);
	if (!pcm_priv)
		return -ENOMEM;
	runtime->private_data = pcm_priv;
	runtime->private_free = rtk_runtime_priv_free;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pcm_priv->ao_id = priv->ao_id;
		dev_info(afe->dev, "%s ao_id = 0x%x\n", __func__, priv->ao_id);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		rpc_create_audio_agent(&afe->rpc_priv, AUDIO_IN, &priv->ai_id);
		pcm_priv->ai_id = priv->ai_id;
		dev_info(afe->dev, "%s ai_id = 0x%x\n", __func__, priv->ai_id);
	}

	return 0;
}

static int rtk_dai_aio_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rpc_ringbuffer_header ring_header = {0};
	struct rtk_dai_aio_priv *priv;
	unsigned long heap_flags;
	int ret;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (priv->lb_secure)
			heap_flags = RTK_SECURE_FLAGS;
		else
			heap_flags = RTK_ACPU_FLAGS;
		if (dai->id == RTK_DAI_I2S0_LOOPBACK) {
			ring_header.instance_id = priv->ai_id;
			ring_header.pin_id = PCM_OUT;
			ring_header.read_idx = -1;
			ret = rtk_snd_prepare_ringbuf(afe, NULL,
						      RTK_ENC_AI_BUFFER_SIZE,
						      runtime->channels,
						      &priv->airing,
						      &ring_header, 1,
						      heap_flags);
			if (ret)
				goto err_alloc_ringbuf;

			ret = rpc_config_loopback_in(&afe->rpc_priv, priv->ai_id);
			if (ret)
				goto err_send_rpc;
		}
	}
	return 0;

err_send_rpc:
err_alloc_ringbuf:
	rtk_snd_free_ringbuf(&priv->airing);
	return ret;
}

static const struct snd_soc_dai_ops rtk_dai_aio_ops = {
	.startup	= rtk_dai_aio_startup,
	.hw_params	= rtk_dai_aio_hw_params,
	.hw_free	= rtk_dai_aio_hw_free,
	.prepare	= rtk_dai_aio_prepare,
};

static struct snd_soc_dai_driver rtk_dai_aio_driver[] = {
	{
		.name = "AUDIO_OUT0",
		.id = RTK_DAI_AUDIO_OUT0,
		.playback = {
			.stream_name = "AUDIO_OUT0",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_AIO_RATES,
			.formats = RTK_AIO_FORMATS,
		},
		.ops = &rtk_dai_aio_ops,
	},
	{
		.name = "AUDIO_OUT1",
		.id = RTK_DAI_AUDIO_OUT1,
		.playback = {
			.stream_name = "AUDIO_OUT1",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_AIO_RATES,
			.formats = RTK_AIO_FORMATS,
		},
		.ops = &rtk_dai_aio_ops,
	},
	{
		.name = "I2S0_LOOPBACK",
		.id = RTK_DAI_I2S0_LOOPBACK,
		.capture = {
			.stream_name = "I2S0_LOOPBACK",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_AIO_RATES,
			.formats = RTK_AIO_FORMATS,
		},
		.ops = &rtk_dai_aio_ops,
	},
};

static int init_aio_priv_data(struct rtk_afe_priv *afe)
{
	struct rtk_dai_aio_priv *priv;
	struct rpc_aio_ctrl *ctrl;
	int ret, i, type;

	for (i = RTK_DAI_AO_START; i < (RTK_DAI_AO_START + afe->data->ao_num); i++) {
		priv = devm_kzalloc(afe->dev,
				    sizeof(struct rtk_dai_aio_priv),
				    GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		afe->dai_priv[i] = priv;

		if (i == RTK_DAI_AUDIO_OUT0)
			type = AUDIO_OUT;
		else if (i == RTK_DAI_AUDIO_OUT1)
			type = AUDIO_OUT2;

		ret = rpc_create_audio_agent(&afe->rpc_priv, type, &priv->ao_id);
		if (ret)
			goto err_send_rpc;

		ctrl = &priv->ctrl;
		ctrl->bitmap = afe->data->ao_bitmap_mask;
		ctrl->bitmap &= ~(1 << ENUM_DT_AO_HDMI);
		ctrl->bitmap &= ~(1 << ENUM_DT_AO_I2S);
		ctrl->bitmap &= ~(1 << ENUM_DT_AO_SPDIF);
		ctrl->bitmap &= ~(1 << ENUM_DT_AO_DAC);
		ctrl->bitmap &= ~(1 << ENUM_DT_AO_GLOBAL);
		ctrl->i2s_ch = afe->data->i2s_out_ch;
		ctrl->i2s_mode = RTK_AUDIO_OUT_I2S_MODE_MASTER;

		dev_info(afe->dev, "%s bitmap=0x%x, i2s_ch=0x%x\n", __func__,
			 ctrl->bitmap, ctrl->i2s_ch);

		ret = rpc_ctrl_aio(&afe->rpc_priv, priv->ao_id, ctrl);
		if (ret)
			goto err_send_rpc;
	}
	priv = devm_kzalloc(afe->dev,
			    sizeof(struct rtk_dai_aio_priv),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	afe->dai_priv[RTK_DAI_I2S0_LOOPBACK] = priv;
	priv->lb_secure = afe->data->lb_secure;

err_send_rpc:
	return ret;
}

int rtk_dai_aio_register(struct rtk_afe_priv *afe)
{
	struct rtk_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = rtk_dai_aio_driver;
	dai->num_dai_drivers = ARRAY_SIZE(rtk_dai_aio_driver);
	dai->dapm_widgets = rtk_dai_aio_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(rtk_dai_aio_widgets);
	dai->dapm_routes = rtk_dai_aio_routes;
	dai->num_dapm_routes = ARRAY_SIZE(rtk_dai_aio_routes);
	dai->controls = rtk_dai_aio_controls;
	dai->num_controls = ARRAY_SIZE(rtk_dai_aio_controls);

	return init_aio_priv_data(afe);
}
