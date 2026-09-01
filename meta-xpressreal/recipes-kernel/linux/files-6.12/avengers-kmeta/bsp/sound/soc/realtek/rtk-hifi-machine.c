// SPDX-License-Identifier: GPL-2.0
//
// rtk-hifi.c  --
//	Realtek ALSA SoC machine driver
//
// Copyright (c) 2024 Realtek Inc.
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <sound/soc.h>

#include <sound/jack.h>
#include "../codecs/rt5645.h"

#define DP_CODEC_DAI	"spdif-hifi"
#define HDMI_CODEC_DAI	"spdif-hifi"

enum {
	DAI_LINK_DL0_FE = 0,
	DAI_LINK_DL1_FE,
	DAI_LINK_DL2_FE,
	DAI_LINK_DL3_FE,
	DAI_LINK_DL4_FE,
	DAI_LINK_DL5_FE,
	DAI_LINK_DL6_FE,
	DAI_LINK_DL7_FE,
	DAI_LINK_UL0_FE,
	DAI_LINK_UL1_FE,
	DAI_LINK_UL2_FE,
	DAI_LINK_I2S0,
	DAI_LINK_I2S1,
	DAI_LINK_STI2S0,
	DAI_LINK_STI2S1,
	DAI_LINK_I2S_LOOPBACK,
	DAI_LINK_AO0_BE,
	DAI_LINK_DMIC_BE,
	DAI_LINK_PDM_BE,
	DAI_LINK_TDM0_BE,
	DAI_LINK_TDM1_BE,
	DAI_LINK_TDM2_BE,
	DAI_LINK_ADC0_BE,
	DAI_LINK_ADC1_BE,
	DAI_LINK_DP_AUDIO_BE,
	DAI_LINK_HDMI_AUDIO_BE,
};

struct rtk_soc_card_data {
	void *mach_priv;
	void *sof_priv;
};

/* FE */
SND_SOC_DAILINK_DEFS(DL0_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DL1_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DL2_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DL3_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL3")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DL4_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL4")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DL5_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL5")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DL6_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL6")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DL7_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL7")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(UL0_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(UL1_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(UL2_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("UL2")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(HDMI_AO, DAILINK_COMP_ARRAY(COMP_CPU("SOF_HDMI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
                     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DP_AO, DAILINK_COMP_ARRAY(COMP_CPU("SOF_DP")),
                     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
/* BE */
SND_SOC_DAILINK_DEFS(I2S0,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(I2S1,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(STI2S0,
		     DAILINK_COMP_ARRAY(COMP_CPU("STI2S0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(STI2S1,
		     DAILINK_COMP_ARRAY(COMP_CPU("STI2S1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(I2S0_LOOPBACK,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S0_LOOPBACK")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(SOC_ADC0,
		     DAILINK_COMP_ARRAY(COMP_CPU("ADC0_IN")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(SOC_ADC1,
		     DAILINK_COMP_ARRAY(COMP_CPU("ADC1_IN")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(SOC_TDM0,
		     DAILINK_COMP_ARRAY(COMP_CPU("TDM0_IN")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(SOC_TDM1,
		     DAILINK_COMP_ARRAY(COMP_CPU("TDM1_IN")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(SOC_TDM2,
		     DAILINK_COMP_ARRAY(COMP_CPU("TDM2_IN")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(SOC_PDM,
		     DAILINK_COMP_ARRAY(COMP_CPU("PDM_IN")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DMIC_PDM,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_DMIC")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(AUDIO_OUT0,
		     DAILINK_COMP_ARRAY(COMP_CPU("AUDIO_OUT0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

static int rtk_sof_be_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	return 0;
}

static struct snd_soc_jack rtk_hdmi_jack;
static int rtk_hdmi_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_jack *jack = &rtk_hdmi_jack;
	struct snd_soc_component *cmpnt_codec =	snd_soc_rtd_to_codec(rtd,0)->component;
	int ret = 0;

	ret = snd_soc_card_jack_new(rtd->card, "HDMI Jack", SND_JACK_LINEOUT, jack);
	if(ret) {
		pr_err("HDMI Jack Fail!");
		return ret;
	}

	ret = snd_soc_component_set_jack(cmpnt_codec, jack, NULL);

	if(ret) {
		pr_err("component set HDMI Jack Fail!");
		return ret;
	}

	return 0;
}

static struct snd_soc_jack rtk_dp_jack;
static int rtk_dp_codec_init(struct snd_soc_pcm_runtime *rtd)
{
        struct snd_soc_jack *jack = &rtk_dp_jack;
        struct snd_soc_component *cmpnt_codec = snd_soc_rtd_to_codec(rtd,0)->component;
        int ret = 0;

        ret = snd_soc_card_jack_new(rtd->card, "DP Jack", SND_JACK_LINEOUT, jack);
        if(ret) {
                pr_err("DP Jack Fail!");
                return ret;
        }

        ret = snd_soc_component_set_jack(cmpnt_codec, jack, NULL);

        if(ret) {
                pr_err("component set DP Jack Fail!");
                return ret;
        }

        return 0;
}

static const struct snd_soc_ops rtk_sof_be_ops = {
	.hw_params = rtk_sof_be_hw_params,
};

static struct snd_soc_dai_link rtk_hifi_dai_links[] = {
	[DAI_LINK_DMIC_BE] = {
		.name = "RDAI_DMIC",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(DMIC_PDM),
	},
	[DAI_LINK_ADC0_BE] = {
		.name = "RDAI_SOC_ADC0",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(SOC_ADC0),
	},
	[DAI_LINK_ADC1_BE] = {
		.name = "RDAI_SOC_ADC1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(SOC_ADC1),
	},
	[DAI_LINK_TDM0_BE] = {
		.name = "RDAI_SOC_TDM0",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(SOC_TDM0),
	},
	[DAI_LINK_TDM1_BE] = {
		.name = "RDAI_SOC_TDM1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(SOC_TDM1),
	},
	[DAI_LINK_TDM2_BE] = {
		.name = "RDAI_SOC_TDM2",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(SOC_TDM2),
	},
	[DAI_LINK_PDM_BE] = {
		.name = "RDAI_SOC_PDM",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(SOC_PDM),
	},
	[DAI_LINK_HDMI_AUDIO_BE] = {
		.name = "RDAI_HDMI",
		.no_pcm = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(HDMI_AO),
	},
	[DAI_LINK_DP_AUDIO_BE] = {
		.name = "RDAI_DP",
		.no_pcm = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DP_AO),
	},
	[DAI_LINK_AO0_BE] = {
		.name = "RDAI_SOC_AO0",
		.no_pcm = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(AUDIO_OUT0),
	},
	[DAI_LINK_I2S0] = {
		.name = "RDAI_SOC_I2S",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(I2S0),
	},
	[DAI_LINK_I2S1] = {
		.name = "RDAI_SOC_I2S1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(I2S1),
	},
	[DAI_LINK_STI2S0] = {
		.name = "RDAI_SOC_STI2S0",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(STI2S0),
	},
	[DAI_LINK_STI2S1] = {
		.name = "RDAI_SOC_STI2S1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(STI2S1),
	},
	[DAI_LINK_I2S_LOOPBACK] = {
		.name = "RDAI_SOC_I2S0_LOOPBACK",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(I2S0_LOOPBACK),
	},
	[DAI_LINK_DL0_FE] = {
		.name = "RDAI_DL0_FE",
		.stream_name = "DL0 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL0_FE),
	},
	[DAI_LINK_DL1_FE] = {
		.name = "RDAI_DL1_FE",
		.stream_name = "DL1 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL1_FE),
	},
	[DAI_LINK_DL2_FE] = {
		.name = "RDAI_DL2_FE",
		.stream_name = "DL2 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL2_FE),
	},
	[DAI_LINK_DL3_FE] = {
		.name = "RDAI_DL3_FE",
		.stream_name = "DL3 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL3_FE),
	},
	[DAI_LINK_DL4_FE] = {
		.name = "RDAI_DL4_FE",
		.stream_name = "DL4 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL4_FE),
	},
	[DAI_LINK_DL5_FE] = {
		.name = "RDAI_DL5_FE",
		.stream_name = "DL5 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL5_FE),
	},
	[DAI_LINK_DL6_FE] = {
		.name = "RDAI_DL6_FE",
		.stream_name = "DL6 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL6_FE),
	},
	[DAI_LINK_DL7_FE] = {
		.name = "RDAI_DL7_FE",
		.stream_name = "DL7 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(DL7_FE),
	},
	[DAI_LINK_UL0_FE] = {
		.name = "RDAI_UL0_FE",
		.stream_name = "UL0 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL0_FE),
	},
	[DAI_LINK_UL1_FE] = {
		.name = "RDAI_UL1_FE",
		.stream_name = "UL1 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL1_FE),
	},
	[DAI_LINK_UL2_FE] = {
		.name = "RDAI_UL2_FE",
		.stream_name = "UL2 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(UL2_FE),
	},
};


static const struct snd_soc_dapm_widget rtk_hifi_widgets[] = {
	SND_SOC_DAPM_SPK("Left Spk", NULL),
	SND_SOC_DAPM_SPK("Right Spk", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
};

static const struct snd_soc_dapm_route rtk_hifi_routes[] = {
	/* speaker */
	{ "Left Spk", NULL, "SPOL" },
	{ "Right Spk", NULL, "SPOR" },
	/* headset */
	{ "Headphone Jack", NULL, "HPOL" },
	{ "Headphone Jack", NULL, "HPOR" },

	{ "Headphone Jack", NULL, "I2S0" },
};

static const struct snd_kcontrol_new rtk_hifi_controls[] = {
	SOC_DAPM_PIN_SWITCH("Left Spk"),
	SOC_DAPM_PIN_SWITCH("Right Spk"),
	SOC_DAPM_PIN_SWITCH("Headphone Jack"),
};

static struct snd_soc_card rtk_hifi_soc_card = {
	.owner = THIS_MODULE,
	.dai_link = rtk_hifi_dai_links,
	.num_links = ARRAY_SIZE(rtk_hifi_dai_links),
	.controls = rtk_hifi_controls,
	.num_controls = ARRAY_SIZE(rtk_hifi_controls),
	.dapm_widgets = rtk_hifi_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rtk_hifi_widgets),
	.dapm_routes = rtk_hifi_routes,
	.num_dapm_routes = ARRAY_SIZE(rtk_hifi_routes),
};

static int rtk_soc_dailink_parse_of(struct snd_soc_card *card, struct device_node *np,
				    const char *propname,
				    struct snd_soc_dai_link *pre_dai_links,
				    int pre_num_links)
{
	struct device *dev = card->dev;
	struct snd_soc_dai_link *parsed_dai_link;
	const char *dai_name = NULL;
	int i, j, ret, num_links, parsed_num_links = 0;

	num_links = of_property_count_strings(np, "realtek,dai-link");
	if (num_links < 0 || num_links > card->num_links) {
		dev_err(dev, "number of dai-link is invalid %d\n", num_links);
		return -EINVAL;
	}

	parsed_dai_link = devm_kcalloc(dev, num_links, sizeof(*parsed_dai_link),
				       GFP_KERNEL);
	if (!parsed_dai_link)
		return -ENOMEM;

	for (i = 0; i < num_links; i++) {
		ret = of_property_read_string_index(np, propname, i, &dai_name);
		if (ret) {
			dev_err(dev,
				"ASoC: Property '%s' index %d could not be read: %d\n",
				propname, i, ret);
			return ret;
		}
		dev_dbg(dev, "ASoC: Property get dai_name:%s\n", dai_name);
		for (j = 0; j < pre_num_links; j++) {
			if (!strcmp(dai_name, pre_dai_links[j].name)) {
				memcpy(&parsed_dai_link[parsed_num_links++],
				       &pre_dai_links[j],
				       sizeof(struct snd_soc_dai_link));
				break;
			}
		}
	}

	if (parsed_num_links != num_links)
		return -EINVAL;

	card->dai_link = parsed_dai_link;
	card->num_links = parsed_num_links;

	return 0;
}

static int rtk_soc_card_probe(struct snd_soc_card *card)
{
	int i;
	struct snd_soc_dai_link *dai_link;

	/* Set stream_name to help sof bind widgets */
	for_each_card_prelinks(card, i, dai_link) {
		if (dai_link->no_pcm && !dai_link->stream_name &&
		    dai_link->name)
			dai_link->stream_name = dai_link->name;
	}

	return 0;
}

static int rtk_hifi_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &rtk_hifi_soc_card;
	struct snd_soc_dai_link *dai_link;
	struct device *dev = &pdev->dev;
	struct device_node *platform_node, *codec_node;
	struct rtk_soc_card_data *soc_card_data;
	int i, ret = 0;

	dev_dbg(dev, "Realtek ASOC machine detected 0x%x\n",SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS);

	card->dev = dev;

	ret = snd_soc_of_parse_card_name(card, "model");
	if (ret) {
		dev_err(dev, "%s card name parsing error %d\n", __func__, ret);
		return ret;
	}

	soc_card_data = devm_kzalloc(dev, sizeof(*soc_card_data), GFP_KERNEL);
	if (!soc_card_data)
		return -ENOMEM;

	platform_node = of_parse_phandle(dev->of_node, "realtek,audio-platform", 0);
	if (!platform_node) {
		dev_err(dev, "%s can't find platform node\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_bool(dev->of_node, "realtek,dai-link")) {
		ret = rtk_soc_dailink_parse_of(card, dev->of_node,
					       "realtek,dai-link",
					       rtk_hifi_dai_links,
					       ARRAY_SIZE(rtk_hifi_dai_links));
		if (ret) {
			dev_err(&pdev->dev, "Parse dai-link fail\n");
			return ret;
		}
	}

	card->probe = rtk_soc_card_probe;

	/* assign the node of sof driver to platforms in dai_link */
	for_each_card_prelinks(card, i, dai_link) {
		dai_link->platforms->of_node = platform_node;
		if (strcmp(dai_link->name, "RDAI_I2S") == 0 ||
			   strcmp(dai_link->name, "RDAI_I2S1") == 0) {
			card->dapm_routes = NULL;
			card->num_dapm_routes = 0;//ARRAY_SIZE(rtk_hifi_routes),
			dai_link->codecs->of_node = of_parse_phandle(dev->of_node, "realtek,audio-codec", 0);
		} else if (strcmp(dai_link->name,"RDAI_HDMI") == 0){
			codec_node = of_parse_phandle(dev->of_node, "realtek,hdmi-audio", 0);

			if(of_device_is_available(codec_node)) {
				dai_link->codecs->name = NULL;
				dai_link->codecs->dai_name = HDMI_CODEC_DAI;
				dai_link->codecs->of_node = codec_node;
				dai_link->init = rtk_hdmi_codec_init;
			} else
				pr_info("Cant find CODEC,RDAI_HDMI use DUMMY CODEC");
		} else if (strcmp(dai_link->name, "RDAI_DP") == 0) {
                        codec_node = of_parse_phandle(dev->of_node, "realtek,dp-audio", 0);

                        if(of_device_is_available(codec_node)) {
				dai_link->codecs->name = NULL;
				dai_link->codecs->dai_name = DP_CODEC_DAI;
				dai_link->codecs->of_node = codec_node;
				dai_link->init = rtk_dp_codec_init;
                        } else
				pr_info("Cant find CODEC,RDAI_DP use DUMMY CODEC");
		} else if (strcmp(dai_link->name, "RDAI_SOC_AO0") == 0) {
			card->dapm_routes = NULL;
			card->num_dapm_routes = 0;//ARRAY_SIZE(rtk_hifi_routes),
		}
	}

	snd_soc_card_set_drvdata(card, soc_card_data);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_err(dev, "Soc register card failed %d\n", ret);
		goto put_exist_node;
	}

	return ret;

put_exist_node:
	of_node_put(platform_node);
	if (codec_node)
		of_node_put(codec_node);
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id rtk_hifi_dt_match[] = {
	{
		.compatible = "realtek,rtd1920s-hifi-mc",
	},
	{
		.compatible = "realtek,rtd1619b-hifi-mc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_hifi_dt_match);
#endif

static struct platform_driver rtk_hifi_driver = {
	.driver = {
		.name = "rtk-hifi",
#ifdef CONFIG_OF
		.of_match_table = rtk_hifi_dt_match,
#endif
		.pm = &snd_soc_pm_ops,
	},
	.probe = rtk_hifi_dev_probe,
};
module_platform_driver(rtk_hifi_driver);

/* Module information */
MODULE_DESCRIPTION("REALTEK ALSA SoC machine driver");
MODULE_AUTHOR("Simon Hsu <simon_hsu@realtek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("realtek soc card");
