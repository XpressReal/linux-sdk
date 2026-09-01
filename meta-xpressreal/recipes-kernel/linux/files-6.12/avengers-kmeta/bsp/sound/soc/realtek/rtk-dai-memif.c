// SPDX-License-Identifier: GPL-2.0
//
// RealTek ALSA SoC Audio DAI MEMIF
//
// Copyright (c) 2024 RealTek Inc.
// Author: Simon Hsu <simon_hsu@realtek.com>
//

#include "rtk-afe-common.h"

enum {
	ENUM_MEMIF_VOLUME,
	ENUM_MEMIF_MIXIDX_EN,
	ENUM_MEMIF_MIXIDX,
};

static void rtk_memif_update_mixidx(struct work_struct *work)
{
	struct rtk_afe_memif *memif = container_of(work, struct rtk_afe_memif,
						   mixidx_work.work);

	rpc_set_ao_mixidx(memif->rpc_priv, memif->ao_id, memif->pin, memif->mixidx);
}

static int rtk_memif_ctrl_info(struct snd_kcontrol *kcontrol,
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

static int rtk_memif_ctrl_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl *)kcontrol->private_value;
	struct rtk_afe_memif *memif;
	int i;

	memif = afe->dai_priv[kcontrol->id.device];

	if (kctrl->kctrl_num == ENUM_MEMIF_VOLUME)
		ucontrol->value.integer.value[0] = memif->volume;
	else if (kctrl->kctrl_num == ENUM_MEMIF_MIXIDX_EN)
		ucontrol->value.integer.value[0] = memif->mixidx_en;
	else if (kctrl->kctrl_num == ENUM_MEMIF_MIXIDX)
		for (i = 0; i < 32; i++)
			ucontrol->value.integer.value[i] = memif->mixidx[i];
	else
		dev_err(afe->dev, "kctrl:%d not support\n", kctrl->kctrl_num);

	return 0;
}

static int rtk_memif_ctrl_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(cmpnt);
	struct rtk_dai_kctrl *kctrl =
			(struct rtk_dai_kctrl*)kcontrol->private_value;
	struct rtk_afe_memif *memif;
	struct snd_pcm_substream *substream;
	int i;

	memif = afe->dai_priv[kcontrol->id.device];

	mutex_lock(&memif->lock);

	if (kctrl->kctrl_num == ENUM_MEMIF_VOLUME)
		memif->volume = ucontrol->value.integer.value[0];
	else if (kctrl->kctrl_num == ENUM_MEMIF_MIXIDX_EN)
		memif->mixidx_en = ucontrol->value.integer.value[0];
	else if (kctrl->kctrl_num == ENUM_MEMIF_MIXIDX)
		for (i = 0; i < 32; i++)
			memif->mixidx[i] = ucontrol->value.integer.value[i];
	else
		dev_err(afe->dev, "kctrl:%d not support\n", kctrl->kctrl_num);

	substream = memif->substream;
	if (!substream || !substream->runtime || !substream->runtime->private_data) {
		dev_warn(afe->dev, "no pcm/runtime/privdata, only save value\n");
		goto exit;
	}

	if (kctrl->kctrl_num == ENUM_MEMIF_VOLUME)
		rpc_set_ao_flash_volume(memif->rpc_priv, memif->ao_id, memif->pin, memif->volume);
	else if (kctrl->kctrl_num == ENUM_MEMIF_MIXIDX)
		schedule_delayed_work(&memif->mixidx_work, msecs_to_jiffies(10));
	else
		dev_err(afe->dev, "kctrl:%d not support\n", kctrl->kctrl_num);

exit:
	mutex_unlock(&memif->lock);
	return 0;
}

#define RTK_SOC_MEMIF_CONTROL(xname, xnum, xcount, xmin, xmax, \
				id) \
	RTK_SOC_DAI_CONTROL(xname, xnum, xcount, xmin, xmax, \
				rtk_memif_ctrl_info, \
				rtk_memif_ctrl_get, \
				rtk_memif_ctrl_put, \
				id)

static const char * const src_sel_mux_text[] = {
	"i2s0", "i2s1", "ao_lb", "pdm", "adc0", "adc1", "tdm0", "tdm1", "tdm2", "hsi2s0", "hsi2s1",
};

static SOC_ENUM_SINGLE_DECL(src_sel_mux_enum,
	SND_SOC_NOPM, 0, src_sel_mux_text);

static const struct snd_kcontrol_new memif_kctl[] = {
	RTK_SOC_MEMIF_CONTROL("DL0 Volume", ENUM_MEMIF_VOLUME, 1, 0, 31, RTK_DAI_MEMIF_DL0),
	RTK_SOC_MEMIF_CONTROL("DL1 Volume", ENUM_MEMIF_VOLUME, 1, 0, 31, RTK_DAI_MEMIF_DL1),
	RTK_SOC_MEMIF_CONTROL("DL2 Volume", ENUM_MEMIF_VOLUME, 1, 0, 31, RTK_DAI_MEMIF_DL2),
	RTK_SOC_MEMIF_CONTROL("DL3 Volume", ENUM_MEMIF_VOLUME, 1, 0, 31, RTK_DAI_MEMIF_DL3),
	RTK_SOC_MEMIF_CONTROL("DL4 Volume", ENUM_MEMIF_VOLUME, 1, 0, 31, RTK_DAI_MEMIF_DL4),
	RTK_SOC_MEMIF_CONTROL("DL5 Volume", ENUM_MEMIF_VOLUME, 1, 0, 31, RTK_DAI_MEMIF_DL5),
	RTK_SOC_MEMIF_CONTROL("DL6 Volume", ENUM_MEMIF_VOLUME, 1, 0, 31, RTK_DAI_MEMIF_DL6),
	RTK_SOC_MEMIF_CONTROL("DL7 Volume", ENUM_MEMIF_VOLUME, 1, 0, 31, RTK_DAI_MEMIF_DL7),

	RTK_SOC_MEMIF_CONTROL("DL0 Mixing Mode Enable", ENUM_MEMIF_MIXIDX_EN, 1, 0, 1, RTK_DAI_MEMIF_DL0),
	RTK_SOC_MEMIF_CONTROL("DL0 Mixing Index", ENUM_MEMIF_MIXIDX, 32, 0, 2, RTK_DAI_MEMIF_DL0),
	RTK_SOC_MEMIF_CONTROL("DL1 Mixing Mode Enable", ENUM_MEMIF_MIXIDX_EN, 1, 0, 1, RTK_DAI_MEMIF_DL1),
	RTK_SOC_MEMIF_CONTROL("DL1 Mixing Index", ENUM_MEMIF_MIXIDX, 32, 0, 2, RTK_DAI_MEMIF_DL1),
	RTK_SOC_MEMIF_CONTROL("DL2 Mixing Mode Enable", ENUM_MEMIF_MIXIDX_EN, 1, 0, 1, RTK_DAI_MEMIF_DL2),
	RTK_SOC_MEMIF_CONTROL("DL2 Mixing Index", ENUM_MEMIF_MIXIDX, 32, 0, 2, RTK_DAI_MEMIF_DL2),
	RTK_SOC_MEMIF_CONTROL("DL3 Mixing Mode Enable", ENUM_MEMIF_MIXIDX_EN, 1, 0, 1, RTK_DAI_MEMIF_DL3),
	RTK_SOC_MEMIF_CONTROL("DL3 Mixing Index", ENUM_MEMIF_MIXIDX, 32, 0, 2, RTK_DAI_MEMIF_DL3),
	RTK_SOC_MEMIF_CONTROL("DL4 Mixing Mode Enable", ENUM_MEMIF_MIXIDX_EN, 1, 0, 1, RTK_DAI_MEMIF_DL4),
	RTK_SOC_MEMIF_CONTROL("DL4 Mixing Index", ENUM_MEMIF_MIXIDX, 32, 0, 2, RTK_DAI_MEMIF_DL4),
	RTK_SOC_MEMIF_CONTROL("DL5 Mixing Mode Enable", ENUM_MEMIF_MIXIDX_EN, 1, 0, 1, RTK_DAI_MEMIF_DL5),
	RTK_SOC_MEMIF_CONTROL("DL5 Mixing Index", ENUM_MEMIF_MIXIDX, 32, 0, 2, RTK_DAI_MEMIF_DL5),
	RTK_SOC_MEMIF_CONTROL("DL6 Mixing Mode Enable", ENUM_MEMIF_MIXIDX_EN, 1, 0, 1, RTK_DAI_MEMIF_DL6),
	RTK_SOC_MEMIF_CONTROL("DL6 Mixing Index", ENUM_MEMIF_MIXIDX, 32, 0, 2, RTK_DAI_MEMIF_DL6),
	RTK_SOC_MEMIF_CONTROL("DL7 Mixing Mode Enable", ENUM_MEMIF_MIXIDX_EN, 1, 0, 1, RTK_DAI_MEMIF_DL7),
	RTK_SOC_MEMIF_CONTROL("DL7 Mixing Index", ENUM_MEMIF_MIXIDX, 32, 0, 2, RTK_DAI_MEMIF_DL7),
};

static const struct snd_kcontrol_new ul0_src_sel_mux =
	SOC_DAPM_ENUM("UL0 src sel", src_sel_mux_enum);
static const struct snd_kcontrol_new ul1_src_sel_mux =
	SOC_DAPM_ENUM("UL1 src sel", src_sel_mux_enum);
static const struct snd_kcontrol_new ul2_src_sel_mux =
	SOC_DAPM_ENUM("UL2 src sel", src_sel_mux_enum);

static const struct snd_soc_dapm_widget rtk_dai_memif_widgets[] = {
	SND_SOC_DAPM_MUX("UL0 Source", SND_SOC_NOPM, 0, 0, &ul0_src_sel_mux),
	SND_SOC_DAPM_MUX("UL1 Source", SND_SOC_NOPM, 0, 0, &ul1_src_sel_mux),
	SND_SOC_DAPM_MUX("UL2 Source", SND_SOC_NOPM, 0, 0, &ul2_src_sel_mux),
};

static const struct snd_soc_dapm_route rtk_dai_memif_routes[] = {
	{"UL0", NULL, "UL0 Source"},
	{"UL1", NULL, "UL1 Source"},
	{"UL2", NULL, "UL2 Source"},
};

static void ring1_to_ring2_general(struct audio_ringbuf_ptr *ring1,
				   struct audio_ringbuf_ptr *ring2,
				   long size)
{
	if (ring1->rp + size <= ring1->limit) {
		if (ring2->wp + size <= ring2->limit) {
			memcpy((char *)ring2->wp, (char *)ring1->rp, size);
		} else {
			int i = ring2->limit - ring2->wp;

			memcpy((char *)ring2->wp, (char *)ring1->rp, i);
			memcpy((char *)ring2->base, (char *)(ring1->rp + i), size - i);
		}
	} else {
		if (ring2->wp + size <= ring2->limit) {
			int i = ring1->limit - ring1->rp;

			memcpy((char *)ring2->wp, (char *)ring1->rp, i);
			memcpy((char *)(ring2->wp + i), (char *)(ring1->base), size - i);
		} else {
			int i, j;

			i = ring1->limit - ring1->rp;
			j = ring2->limit - ring2->wp;

			if (j <= i) {
				memcpy((char *)ring2->wp, (char *)ring1->rp, j);
				memcpy((char *)ring2->base, (char *)(ring1->rp + j), i - j);
				memcpy((char *)(ring2->base + i - j),
				       (char *)(ring1->base), size - i);
			} else {
				memcpy((char *)ring2->wp, (char *)ring1->rp, i);
				memcpy((char *)(ring2->wp + i), (char *)ring1->base, j - i);
				memcpy((char *)ring2->base,
				       (char *)(ring1->base + j - i), size - j);
			}
		}
	}

	ring1->rp += size;
	if (ring1->rp >= ring1->limit)
		ring1->rp = ring1->base + (ring1->rp - ring1->limit);

	ring2->wp += size;
	if (ring2->wp >= ring2->limit)
		ring2->wp = ring2->base + (ring2->wp - ring2->limit);
}

static void snd_card_capture_lpcm_copy(struct snd_pcm_runtime *runtime,
				       long period_count,
				       unsigned int total_write,
				       unsigned int *lpcm_base,
				       struct ringbuf_header_ptrs *header)
{
	snd_pcm_uframes_t frame_size = period_count * runtime->period_size;
	snd_pcm_uframes_t dma_wp = total_write % runtime->buffer_size;
	struct audio_ringbuf_ptr src_ring, dst_ring;

	src_ring.base = (unsigned long)lpcm_base;
	src_ring.limit = (unsigned long)(src_ring.base + *header->p_size);
	src_ring.rp = src_ring.base
		+ (unsigned long)(*header->p_read_ptr - *header->p_begin_addr);

	dst_ring.base = (unsigned long)runtime->dma_area;
	dst_ring.limit = (unsigned long)(runtime->dma_area +
					 frames_to_bytes(runtime, runtime->buffer_size));
	dst_ring.wp = (unsigned long)(runtime->dma_area + frames_to_bytes(runtime, dma_wp));

	ring1_to_ring2_general(&src_ring, &dst_ring, frames_to_bytes(runtime, frame_size));
}

static enum hrtimer_restart rtk_afe_play_timer_func(struct hrtimer *timer)
{
	struct rtk_afe_memif *memif =
		container_of(timer, struct rtk_afe_memif, timer);

	if (memif->stat == HRTIMER_RESTART) {
		snd_pcm_period_elapsed(memif->substream);
		hrtimer_forward_now(timer, memif->ktime);
		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart rtk_afe_cap_timer_func(struct hrtimer *timer)
{
	struct rtk_afe_memif *memif =
		container_of(timer, struct rtk_afe_memif, timer);
	struct snd_pcm_substream *substream = memif->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ringbuf_header_ptrs *ptrs = memif->ringbuf->hdrs[0].ptrs;
	snd_pcm_uframes_t data_frame;
	unsigned long data_size, free_size;
	unsigned int period_count = 0, free_period;

	if (memif->stat == HRTIMER_RESTART) {
		data_size = ring_valid_data(*ptrs->p_begin_addr,
					*ptrs->p_begin_addr + *ptrs->p_size,
					*ptrs->p_read_ptr,
					*ptrs->p_write_ptr);
		data_frame = bytes_to_frames(runtime, data_size);
		if (data_frame >= runtime->period_size) {
			period_count = data_frame / runtime->period_size;
			if (period_count == runtime->periods)
				period_count--;

			/* check overflow */
			free_size = runtime->buffer_size - ring_valid_data(0,
					(unsigned long)runtime->boundary,
					(unsigned long)runtime->control->appl_ptr,
					(unsigned long)runtime->status->hw_ptr);
			free_period = free_size / runtime->period_size;
			period_count = min(period_count, free_period);
			if (period_count == 0)
				goto SET_TIMER;

			snd_card_capture_lpcm_copy(runtime, period_count,
						   memif->total_write,
						   (unsigned int *)memif->ringbuf->dmab->area,
						   ptrs);

			*ptrs->p_read_ptr = ring_add(*ptrs->p_begin_addr,
						*ptrs->p_begin_addr + *ptrs->p_size,
						*ptrs->p_read_ptr,
				 		frames_to_bytes(runtime,
						runtime->period_size * period_count));
			memif->total_write += period_count * runtime->period_size;
			snd_pcm_period_elapsed(substream);
		}
SET_TIMER:
		/* Set up the next time */
		hrtimer_forward_now(timer, memif->ktime);
		return HRTIMER_RESTART;
	}

	return HRTIMER_NORESTART;
}

static int rtk_dai_fe_pcm_new(struct snd_soc_pcm_runtime *rtd,
			      struct snd_soc_dai *dai)
{
	struct snd_kcontrol *kctl;
	int i, ret;

	for(i = 0; i < ARRAY_SIZE(memif_kctl); i++) {
		if (dai->id != memif_kctl[i].device)
			continue;

		kctl = snd_ctl_new1(&memif_kctl[i], dai->component);
		ret = snd_ctl_add(rtd->card->snd_card, kctl);
		if (ret < 0)
			goto add_kctl_fail;
	}
	return 0;

add_kctl_fail:
	dev_err(rtd->dev, "failed to create memif%d kcontrol\n", dai->id);
	return ret;
}

static int rtk_dai_fe_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_afe_memif *memif = afe->dai_priv[dai->id];
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_set_runtime_hwparams(substream, afe->rtk_afe_hardware);
	else
		snd_soc_set_runtime_hwparams(substream, afe->rtk_afe_capture_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(afe->dev, "snd_pcm_hw_constraint_integer failed\n");
		return ret;
	}

	hrtimer_init(&memif->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		memif->timer.function = &rtk_afe_play_timer_func;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		memif->timer.function = &rtk_afe_cap_timer_func;

	return 0;
}

static int rtk_dai_fe_hw_free(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_afe_memif *memif = afe->dai_priv[dai->id];
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_pcm_runtime_priv *pcm_priv = runtime->private_data;
	ktime_t remaining;
	int ret, configured = 0;

	if (!pcm_priv)
		return 0;

	mutex_lock(&memif->lock);

	remaining = hrtimer_get_remaining(&memif->timer);
	if (ktime_to_ns(remaining) > 0)
		ndelay(ktime_to_ns(remaining));

	ret = hrtimer_cancel(&memif->timer);
	if (ret) {
		dev_err(afe->dev, "the timer still alive...\n");
		goto err_send_rpc;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (memif->pin) {
			ret = rpc_put_shmem_latency(&afe->rpc_priv, pcm_priv->ao_id,
						    memif->pin, NULL);
			if (ret)
				goto err_send_rpc;
			ret = rpc_pause_svc(&afe->rpc_priv, (pcm_priv->ao_id | memif->pin));
			if (ret)
				goto err_send_rpc;
			ret = rpc_stop_svc(&afe->rpc_priv, (pcm_priv->ao_id | memif->pin));
			if (ret)
				goto err_send_rpc;
			ret = rpc_put_ao_flash_pin(&afe->rpc_priv, pcm_priv->ao_id, &memif->pin);
			if (ret)
				goto err_send_rpc;
		}

		if (memif->ringbuf) {
			snd_pcm_set_runtime_buffer(substream, NULL);
			kref_put(&memif->ringbuf->ref, rtk_snd_ringbuf_destroy);
			memif->ringbuf = NULL;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (memif->substream)
			configured = 1;
		/* todo flow not correct */
		ret = rpc_destroy_ai_flow(&afe->rpc_priv, pcm_priv->ai_id, configured);
		if (ret)
			goto err_send_rpc;

		pcm_priv->ai_id = 0;
		if (memif->ringbuf) {
			memif->ringbuf->hdrs[0].rpc_hdr.instance_id = 0;
			memif->ringbuf->hdrs[0].rpc_hdr.pin_id = 0;
		}
		rtk_snd_free_ringbuf(&memif->ringbuf);
	}

	memif->total_read = memif->total_write = memif->prehw_ptr = 0;

	memif->substream = NULL;
	mutex_unlock(&memif->lock);
	return 0;

err_send_rpc:
	mutex_unlock(&memif->lock);
	return ret;
}

static void rtk_runtime_priv_free(struct snd_pcm_runtime *runtime)
{
	kfree(runtime->private_data);
	runtime->private_data = NULL;
}

static int rtk_dai_fe_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_afe_memif *memif = afe->dai_priv[dai->id];
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_pcm_runtime_priv *pcm_priv = runtime->private_data;
	struct rpc_ringbuffer_header ring_header = {0};
	int ret, pin;
	size_t size;

	struct rtk_dai_aio_priv *aio_priv;

	if (!pcm_priv) {
		pcm_priv = kzalloc(sizeof(*pcm_priv), GFP_KERNEL);
		if (!pcm_priv)
			return -ENOMEM;
		runtime->private_data = pcm_priv;
		runtime->private_free = rtk_runtime_priv_free;

		aio_priv = afe->dai_priv[RTK_DAI_AUDIO_OUT0];
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			pcm_priv->ao_id = aio_priv->ao_id;
			dev_info(afe->dev, "%s ao_id = 0x%x\n", __func__, aio_priv->ao_id);
		}
	}
	if (memif->ringbuf)
		rtk_dai_fe_hw_free(substream, dai);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* get flash pin from fw */
		ret = rpc_get_ao_flash_pin(&afe->rpc_priv, pcm_priv->ao_id, &pin);
		if (ret || pin < FLASH_AUDIO_PIN_1 || pin > FLASH_AUDIO_PIN_8) {
			dev_err(afe->dev, "[%s] flash pin get fail, %d\n", __func__, pin);
			return -1;
		}
		memif->ao_id = pcm_priv->ao_id;
		memif->pin = pin;

		/* prepare input ring buffer */
		ring_header.instance_id = pcm_priv->ao_id;
		ring_header.pin_id = pin;

		size = params_buffer_bytes(params);
		ret = rtk_snd_prepare_ringbuf(afe, NULL, size, 1, &memif->ringbuf,
					      &ring_header, 1, RTK_ACPU_FLAGS);
		if (ret)
			goto err_prepare_ringbuf;

		kref_init(&memif->ringbuf->ref);

		memif->ringbuf->hdrs[0].ptrs->p_latency->sync = 0x23792379;

		snd_pcm_set_runtime_buffer(substream, memif->ringbuf->dmab);
		runtime->dma_bytes = size;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (pcm_priv->no_lpcm) {
			ring_header.instance_id = pcm_priv->ai_id;
			ring_header.pin_id = PCM_OUT;
			ring_header.read_idx = -1;
			size = RTK_ENC_FAI_BUFFER_SIZE;
		} else {
			ring_header.instance_id = pcm_priv->ai_id;
			ring_header.pin_id = BASE_BS_OUT;
			ring_header.read_idx = -1;
			size = RTK_ENC_AI_BUFFER_SIZE;
		}

		ret = rtk_snd_prepare_ringbuf(afe, NULL, size,
					      1, &memif->ringbuf, &ring_header,
					      1, RTK_ACPU_FLAGS);
		if (ret)
			goto err_prepare_ringbuf;
	}
	return 0;

err_prepare_ringbuf:
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		rpc_put_ao_flash_pin(&afe->rpc_priv, pcm_priv->ao_id, &memif->pin);

	rtk_snd_free_ringbuf(&memif->ringbuf);
	return ret;
}

static int rtk_dai_fe_prepare(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_afe_memif *memif = afe->dai_priv[dai->id];
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_pcm_runtime_priv *pcm_priv = runtime->private_data;
	unsigned int *mixidx = NULL;
	int ret, pin;

	dev_info(afe->dev, "%s %s:\n", __func__, substream->name);
	dev_info(afe->dev, "rate %d channel %d format %d\n", runtime->rate, runtime->channels, runtime->format);
	dev_info(afe->dev, "period_size %ld peroid %d\n", runtime->period_size, runtime->periods);
	dev_info(afe->dev, "buffer_size %ld dma_bytes %ld\n", runtime->buffer_size, runtime->dma_bytes);

	mutex_lock(&memif->lock);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pin = memif->pin;

		ret = rpc_put_shmem_latency(&afe->rpc_priv, pcm_priv->ao_id,
					    pin, NULL);
		if (ret)
			goto err_send_rpc;
		ret = rpc_pause_svc(&afe->rpc_priv, (pcm_priv->ao_id | pin));
		if (ret)
			goto err_send_rpc;
		ret = rpc_stop_svc(&afe->rpc_priv, (pcm_priv->ao_id | pin));
		if (ret)
			goto err_send_rpc;

		rtk_snd_reinit_ringheader(memif->ringbuf);

		memif->total_read = memif->prehw_ptr = 0;

		memif->ktime = ktime_set(0, (runtime->period_size * 1000) * 1000 /
					      runtime->rate * 1000);

		ret = rpc_put_shmem_latency(&afe->rpc_priv, pcm_priv->ao_id, pin,
				(void *)memif->ringbuf->hdrs[0].paddr + SHMEM_OFFSET);

		if (memif->mixidx_en)
			mixidx = memif->mixidx;
		ret = rpc_config_ao(&afe->rpc_priv, pcm_priv->ao_id, pin,
				    mixidx, runtime);
		if (ret)
			goto err_send_rpc;
		ret = rpc_pause_svc(&afe->rpc_priv, (pcm_priv->ao_id | pin));
		if (ret)
			goto err_send_rpc;
		ret = rpc_run_svc(&afe->rpc_priv, (pcm_priv->ao_id | pin));
		if (ret)
			goto err_send_rpc;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		memif->total_write = 0;

		if (runtime->rate >= 384000)
			memif->ktime = ktime_set(0, (runtime->period_size * 1000) /
						2000000 * 1000 * 1000);
		else
			memif->ktime = ktime_set(0, (runtime->period_size * 1000) /
						runtime->rate * 1000 * 1000);

		ret = rpc_pause_svc(&afe->rpc_priv, pcm_priv->ai_id);
		if (ret)
			goto err_send_rpc;
		ret = rpc_run_svc(&afe->rpc_priv, pcm_priv->ai_id);
		if (ret)
			goto err_send_rpc;
	}
	memif->substream = substream;
	mutex_unlock(&memif->lock);
	return 0;

err_send_rpc:
	mutex_unlock(&memif->lock);
	return ret;
}

static int rtk_dai_fe_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
	struct rtk_afe_memif *memif = afe->dai_priv[dai->id];

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_STOP:
			memif->stat = HRTIMER_NORESTART;
			hrtimer_try_to_cancel(&memif->timer);
			break;
		case SNDRV_PCM_TRIGGER_START:
			memif->stat = HRTIMER_RESTART;
			hrtimer_start(&memif->timer, memif->ktime, HRTIMER_MODE_REL);
			break;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_STOP:
			memif->stat = HRTIMER_NORESTART;
			hrtimer_try_to_cancel(&memif->timer);
			break;
		case SNDRV_PCM_TRIGGER_START:
			memif->stat = HRTIMER_RESTART;
			hrtimer_start(&memif->timer, memif->ktime, HRTIMER_MODE_REL);
			break;
		}
	}
	return 0;
}

static void rtk_dai_fe_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
//	struct rtk_afe_priv *afe = snd_soc_dai_get_drvdata(dai);
//	struct rtk_afe_memif *memif = afe->dai_priv[dai->id];
//	memset(memif, 0, sizeof(*memif));
}

const struct snd_soc_dai_ops rtk_dai_fe_ops = {
	.startup	= rtk_dai_fe_startup,
	.shutdown	= rtk_dai_fe_shutdown,
	.hw_params	= rtk_dai_fe_hw_params,
	.hw_free	= rtk_dai_fe_hw_free,
	.prepare	= rtk_dai_fe_prepare,
	.trigger	= rtk_dai_fe_trigger,
	.pcm_new	= rtk_dai_fe_pcm_new,
};


static struct snd_soc_dai_driver rtk_dai_memif_driver[] = {
	// FE DAIs: memory interfaces to CPU
	{
		.name = "DL0",
		.id = RTK_DAI_MEMIF_DL0,
		.playback = {
			.stream_name = "DL0",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_RATES,
			.formats = RTK_PCM_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "DL1",
		.id = RTK_DAI_MEMIF_DL1,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_RATES,
			.formats = RTK_PCM_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "DL2",
		.id = RTK_DAI_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_RATES,
			.formats = RTK_PCM_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "DL3",
		.id = RTK_DAI_MEMIF_DL3,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_RATES,
			.formats = RTK_PCM_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "DL4",
		.id = RTK_DAI_MEMIF_DL4,
		.playback = {
			.stream_name = "DL4",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_RATES,
			.formats = RTK_PCM_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "DL5",
		.id = RTK_DAI_MEMIF_DL5,
		.playback = {
			.stream_name = "DL5",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_RATES,
			.formats = RTK_PCM_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "DL6",
		.id = RTK_DAI_MEMIF_DL6,
		.playback = {
			.stream_name = "DL6",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_RATES,
			.formats = RTK_PCM_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "DL7",
		.id = RTK_DAI_MEMIF_DL7,
		.playback = {
			.stream_name = "DL7",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_RATES,
			.formats = RTK_PCM_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "UL0",
		.id = RTK_DAI_MEMIF_UL0,
		.capture = {
			.stream_name = "UL0",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_CAPTURE_RATES,
			.formats = RTK_PCM_CAPTURE_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "UL1",
		.id = RTK_DAI_MEMIF_UL1,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_CAPTURE_RATES,
			.formats = RTK_PCM_CAPTURE_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
	{
		.name = "UL2",
		.id = RTK_DAI_MEMIF_UL2,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 8,
			.rates = RTK_PCM_CAPTURE_RATES,
			.formats = RTK_PCM_CAPTURE_FORMATS,
		},
		.ops = &rtk_dai_fe_ops,
	},
};

static void init_memif_priv_data(struct rtk_afe_priv *afe)
{
	int i;

	for (i = RTK_DAI_MEMIF_START; i < RTK_DAI_MEMIF_END; i++) {
		afe->dai_priv[i] = (void *)&afe->memif[i];
		afe->memif[i].rpc_priv = &afe->rpc_priv;
		mutex_init(&afe->memif[i].lock);

		INIT_DELAYED_WORK(&afe->memif[i].mixidx_work, rtk_memif_update_mixidx);
		afe->memif[i].volume = 31;
	}
}

int rtk_dai_memif_register(struct rtk_afe_priv *afe)
{
	struct rtk_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = rtk_dai_memif_driver;
	dai->num_dai_drivers = ARRAY_SIZE(rtk_dai_memif_driver);
	dai->dapm_widgets = rtk_dai_memif_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(rtk_dai_memif_widgets);
	dai->dapm_routes = rtk_dai_memif_routes;
	dai->num_dapm_routes = ARRAY_SIZE(rtk_dai_memif_routes);

	init_memif_priv_data(afe);

	return 0;
}

