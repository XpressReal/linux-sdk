// SPDX-License-Identifier: GPL-2.0
//
// RealTek ALSA SoC Audio DAI PDM Control
//
// Copyright (c) 2024 RealTek Inc.
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "rtk-afe-common.h"

#define RTK_PDM_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
			 SNDRV_PCM_FMTBIT_S16_LE | \
			 SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S24_3LE)

#define RTK_PDM_RATES (SNDRV_PCM_RATE_8000_48000 | \
			SNDRV_PCM_RATE_88200 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_176400 | \
			SNDRV_PCM_RATE_192000)

struct rtk_dai_pdm_priv {
	unsigned int ai_id;
	struct rtk_snd_ringbuf *airing;
};

static const struct snd_soc_dapm_route rtk_dai_pdm_routes[] = {
	{"UL0 Source", "pdm", "PDM_IN"},
	{"UL1 Source", "pdm", "PDM_IN"},
	{"UL2 Source", "pdm", "PDM_IN"},
};

static void rtk_runtime_priv_free(struct snd_pcm_runtime *runtime)
{
	kfree(runtime->private_data);
	runtime->private_data = NULL;
}

static int rtk_dai_pdm_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_dai_pdm_priv *priv;
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

static int rtk_dai_pdm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	return 0;
}

static int rtk_dai_pdm_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_dai_pdm_priv *priv;

	priv = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		rtk_snd_free_ringbuf(&priv->airing);

	return 0;
}

static int rtk_dai_pdm_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rpc_ringbuffer_header ring_header = {0};
	struct rtk_dai_pdm_priv *priv;
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

		ret = rpc_config_pdm_in(&afe->rpc_priv, priv->ai_id);
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

static const struct snd_soc_dai_ops rtk_dai_pdm_ops = {
	.startup	= rtk_dai_pdm_startup,
	.hw_params	= rtk_dai_pdm_hw_params,
	.hw_free	= rtk_dai_pdm_hw_free,
	.prepare	= rtk_dai_pdm_prepare,
};

static struct snd_soc_dai_driver rtk_dai_pdm_driver[] = {
	{
		.name = "PDM_IN",
		.id = RTK_DAI_PDM,
		.capture = {
			.stream_name = "PDM_IN",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PDM_RATES,
			.formats = RTK_PDM_FORMATS,
		},
		.ops = &rtk_dai_pdm_ops,
	},
};

static int init_pdm_priv_data(struct rtk_afe_priv *afe)
{
	struct rtk_dai_pdm_priv *priv;

	priv = devm_kzalloc(afe->dev,
			    sizeof(struct rtk_dai_pdm_priv),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	afe->dai_priv[RTK_DAI_PDM] = priv;

	return 0;
}

int rtk_dai_pdm_register(struct rtk_afe_priv *afe)
{
	struct rtk_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = rtk_dai_pdm_driver;
	dai->num_dai_drivers = ARRAY_SIZE(rtk_dai_pdm_driver);
	dai->dapm_routes = rtk_dai_pdm_routes;
	dai->num_dapm_routes = ARRAY_SIZE(rtk_dai_pdm_routes);

	return init_pdm_priv_data(afe);
}
