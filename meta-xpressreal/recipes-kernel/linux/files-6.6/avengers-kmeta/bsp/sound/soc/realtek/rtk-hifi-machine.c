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

#define RT5650_CODEC_DAI1	"rt5645-aif1"
#define RT5650_CODEC_DAI2	"rt5645-aif2"

#define DP_CODEC_DAI	"spdif-hifi"
#define HDMI_CODEC_DAI	"spdif-hifi"

enum {
	DAI_LINK_SOF_RDL2_BE = 0,
	DAI_LINK_SOF_RUL4_BE,
	DAI_LINK_DL1_BE,
	DAI_LINK_UL1_BE,
	DAI_LINK_DL1_FE,
	DAI_LINK_I2S_RT5650,
	DAI_LINK_DMIC_BE,
	DAI_LINK_DP_AUDIO_BE,
	DAI_LINK_HDMI_AUDIO_BE
};

struct rtk_soc_card_data {
	void *mach_priv;
	void *sof_priv;
};

/* FE */
SND_SOC_DAILINK_DEFS(DL1_FE,
		     DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(RDAI_I2S1, DAILINK_COMP_ARRAY(COMP_CPU("SOF_DL2")),
		     DAILINK_COMP_ARRAY(COMP_CODEC(NULL,
						RT5650_CODEC_DAI2)),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(RDAI_I2S0, DAILINK_COMP_ARRAY(COMP_CPU("SOF_UL4")),
		     DAILINK_COMP_ARRAY(COMP_CODEC(NULL,
						RT5650_CODEC_DAI1)),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(HDMI_AO, DAILINK_COMP_ARRAY(COMP_CPU("SOF_HDMI")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
                     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DP_AO, DAILINK_COMP_ARRAY(COMP_CPU("SOF_DP")),
                     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));
/* BE */
SND_SOC_DAILINK_DEFS(i2s0,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S0")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s1,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S1")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s0_rt5650,
		     DAILINK_COMP_ARRAY(COMP_CPU("I2S0")),
		     DAILINK_COMP_ARRAY(COMP_CODEC(NULL,
						   RT5650_CODEC_DAI1)),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(DMIC_PDM,
		     DAILINK_COMP_ARRAY(COMP_CPU("SOF_DMIC")),
		     DAILINK_COMP_ARRAY(COMP_DUMMY()),
		     DAILINK_COMP_ARRAY(COMP_EMPTY()));

static int rtk_sof_be_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	return 0;
}

static int rtk_rt5650_i2s_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
//	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0);
	int ret = 0;
	int mclk;

	switch (params_rate(params)) {
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		mclk = 24576000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		mclk = 22579200;
		break;
	default:
		return -EINVAL;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, 0, mclk,
				     SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev, "Can't set codec clock %d\n", ret);
		return ret;
	}

	return ret;
}

static struct snd_soc_jack rtk_rt5650_jack;

static int rtk_rt5650_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_card *card = runtime->card;
	struct snd_soc_component *component = asoc_rtd_to_codec(runtime, 0)->component;
	int ret;
	int type =	SND_JACK_HEADPHONE | SND_JACK_MICROPHONE |
				SND_JACK_BTN_0 | SND_JACK_BTN_1 |
				SND_JACK_BTN_2 | SND_JACK_BTN_3;

	rt5645_sel_asrc_clk_src(component,
				RT5645_DA_STEREO_FILTER |
				RT5645_AD_STEREO_FILTER,
				RT5645_CLK_SEL_I2S1_ASRC);

	/* enable jack detection */
	ret = snd_soc_card_jack_new(card, "Headphone Jack",
				    type,
				    &rtk_rt5650_jack);

	if (ret) {
		dev_err(card->dev, "Can't new Headphone Jack %d\n", ret);
		return ret;
	}

	ret = snd_soc_component_set_jack(component, &rtk_rt5650_jack, &type);
	if (ret) {
		dev_err(card->dev,"comp set jack FAIL\n");
	}

	return ret;

}

static struct snd_soc_jack rtk_hdmi_jack;
static int rtk_hdmi_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_jack *jack = &rtk_hdmi_jack;
	struct snd_soc_component *cmpnt_codec =	asoc_rtd_to_codec(rtd,0)->component;
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
        struct snd_soc_component *cmpnt_codec = asoc_rtd_to_codec(rtd,0)->component;
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

static const struct snd_soc_ops rtk_rt5650_i2s_ops = {
	.hw_params = rtk_rt5650_i2s_hw_params,
};

static struct snd_soc_dai_link rtk_hifi_dai_links[] = {
	[DAI_LINK_SOF_RDL2_BE] = {
		.name = "RDAI_I2S1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		//.dpcm_capture = 1,
		.ops = &rtk_rt5650_i2s_ops,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		SND_SOC_DAILINK_REG(RDAI_I2S1),
	},
	[DAI_LINK_SOF_RUL4_BE] = {
		.name = "RDAI_I2S",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = rtk_rt5650_init,
		.ops = &rtk_rt5650_i2s_ops,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		SND_SOC_DAILINK_REG(RDAI_I2S0),
	},
	[DAI_LINK_DMIC_BE] = {
		.name = "RDAI_DMIC",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(DMIC_PDM),
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
	[DAI_LINK_DL1_BE] = {
		.name = "RDAI_SOC_DL1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ops = &rtk_sof_be_ops,
		SND_SOC_DAILINK_REG(i2s0),
	},
	[DAI_LINK_UL1_BE] = {
		.name = "RDAI_SOC_UL1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ops = &rtk_sof_be_ops,
		SND_SOC_DAILINK_REG(i2s1),
	},
	[DAI_LINK_DL1_FE] = {
		.name = "RDAI_DL1_FE",
		.stream_name = "DL2 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(DL1_FE),
	},
	[DAI_LINK_I2S_RT5650] = {
		.name = "RDAI_I2S_RT5650",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.ops = &rtk_rt5650_i2s_ops,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		SND_SOC_DAILINK_REG(i2s0_rt5650),
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

int rtk_soc_dailink_parse_of(struct snd_soc_card *card, struct device_node *np,
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

int rtk_soc_card_probe(struct snd_soc_card *card)
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
		if (strcmp(dai_link->name, "RDAI_I2S_RT5650") == 0) {
			dai_link->codecs->of_node = of_parse_phandle(dev->of_node, "realtek,audio-codec", 0);
		} else if (strcmp(dai_link->name, "RDAI_I2S") == 0 ||
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

static const struct dev_pm_ops rtk_hifi_pm_ops = {
	.poweroff = snd_soc_poweroff,
	.restore = snd_soc_resume,
};

static const struct of_device_id rtk_hifi_dt_match[] = {
	{
		.compatible = "realtek,rtd1920s-hifi-mc",
	},
	{},
};

static struct platform_driver rtk_hifi_driver = {
	.driver = {
		.name = "rtk-hifi",
#ifdef CONFIG_OF
		.of_match_table = rtk_hifi_dt_match,
#endif
		.pm = &rtk_hifi_pm_ops,
	},
	.probe = rtk_hifi_dev_probe,
};
module_platform_driver(rtk_hifi_driver);

/* Module information */
MODULE_DESCRIPTION("REALTEK ALSA SoC machine driver");
MODULE_AUTHOR("Simon Hsu <simon_hsu@realtek.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("realtek soc card");
