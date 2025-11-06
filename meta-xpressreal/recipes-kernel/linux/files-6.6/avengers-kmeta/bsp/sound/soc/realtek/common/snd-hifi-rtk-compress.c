/**
 * snd-hifi-rtk-compress.c - Realtek alsa compress driver
 *
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mpage.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/syscalls.h>
#include <linux/dma-map-ops.h>
#include <sound/asound.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include <sound/hwdep.h>
#include <soc/realtek/rtk_refclk.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <asm/cacheflush.h>
#include "snd-hifi-realtek.h"
#include "snd-rtk-compress.h"

struct snd_card_data {
	int card_id;
};

static const struct snd_card_data rtk_sound_card0[] =
{
	{ .card_id = 1, },
};

static const struct snd_card_data rtk_sound_card1[] =
{
	{ .card_id = 2, },
};

static int snd_compress_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo);
static int snd_compress_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol);
static int snd_compress_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol);

static int rtk_snd_alloc(struct device *dev, size_t size,
			 void *ion_phy, void **ion_virt,
			 unsigned long heap_flags)
{
	mutex_lock(&dev->mutex);
	rheap_setup_dma_pools(dev, "rtk_media_heap", heap_flags, __func__);
	*ion_virt = dma_alloc_coherent(dev, size, ion_phy, GFP_KERNEL);
	mutex_unlock(&dev->mutex);
	if (!*ion_virt)
		return -ENOMEM;

	return 0;
}

static struct snd_kcontrol_new snd_feature_ctrl[] = {
	RTK_COMPRESS_CONTROL("Mixing Index", 0, ENUM_MIXING_INDEX),
	RTK_COMPRESS_CONTROL("Dec Volume", 0, ENUM_DEC_VOLUME),
};

struct rtk_krpc_ept_info *hifi_compr_ept_info;
struct rtk_compr_dev {
	struct device *dev;
	struct snd_card *card;
	int ao_agent_id;
};

struct rtk_compr_stream {
	int audio_dec_id;
	int audio_dec_pin_id;
	int audio_pp_id;
	int audio_app_pin_id;
	int audio_out_id;
	int status;
	int log_lv;
	int data_len;
	int dec_insize;
	int dec_outsize;
	int refclk_from_video;
	int avsync_hdr_offset;
	int delim_format;
	int delim_head_size;

	unsigned int codec_id;
	unsigned int audio_channel;
	unsigned int audio_sampling_rate;
	long last_inring_rp;
	bool is_get_info;
	bool is_low_water;
	bool compr_first_pts;
	bool is_check_format;

	size_t bytes_written;
	size_t consume_total;
	unsigned long out_frames;

	struct ringbuffer_header *dec_inring_header;
	struct ringbuffer_header *inbandring_header;
	struct ringbuffer_header *dwnstrmring_header;
	struct ringbuffer_header *dec_outring_header[8];

	unsigned char *dec_inring;
	unsigned char *inbandring;
	unsigned char *dwnstrmring;
	unsigned char *dec_outring[8];
	unsigned char *dwnring_lower;
	unsigned char *dwnring_upper;
	unsigned char *dec_outring_lower[8];
	unsigned char *dec_outring_upper[8];

	phys_addr_t phy_dec_inring_header;
	phys_addr_t phy_inbandring_header;
	phys_addr_t phy_dwnstrmring_header;
	phys_addr_t phy_dec_outring_header[8];
	phys_addr_t phy_dec_inring;
	phys_addr_t phy_inbandring;
	phys_addr_t phy_dwnstrmring;
	phys_addr_t phy_dec_outring[8];

	struct tag_refclock *refclock;
	struct dma_buf *refclk_dmabuf;
	struct dma_buf_attachment *refclk_attach;
	struct sg_table *refclk_table;
	struct dma_buf_map refclk_map;
	phys_addr_t phy_refclock;
	phys_addr_t phy_addr;
	phys_addr_t phy_addr_rpc;
	phys_addr_t phy_rawdelay;
	phys_addr_t phy_rawdelay2;
	phys_addr_t phy_rawdelay3;

	void *vaddr_rpc;
	int *rawdelay_mem;
	struct alsa_raw_latency *rawdelay_mem2;
	struct alsa_raw_latency *rawdelay_mem3;
	unsigned int multi_ao;
};

unsigned long get_buffer_from_ring(unsigned char *upper,
				   unsigned char *lower,
				   unsigned char *rptr,
				   unsigned char *dest,
				   unsigned long size)
{
	unsigned long space = 0;

	if (rptr + size < upper) {
		memcpy(dest, rptr, size);
	} else {
		space = upper - rptr;
		memcpy(dest, rptr, space);
		if (size > space)
			memcpy(dest + space, lower, size - space);
	}

	return space;
}

void update_ring_ptr(unsigned char *upper, unsigned char *lower,
		     unsigned char *rptr, struct rtk_compr_stream *rtk_stream,
		     unsigned long size, unsigned long space)
{
	if ((rptr + size) < upper)
		rtk_stream->dwnstrmring_header->read_ptr[0] = rtk_stream->dwnstrmring_header->read_ptr[0] + size;
	else
		rtk_stream->dwnstrmring_header->read_ptr[0] = rtk_stream->dwnstrmring_header->begin_addr + size - space;
}

static long ring_valid_data(long ring_base, long ring_limit, long ring_rp, long ring_wp)
{
	if (ring_wp >= ring_rp)
		return (ring_wp - ring_rp);
	else
		return (ring_limit - ring_base) - (ring_rp - ring_wp);
}

static long valid_free_size(long base, long limit, long rp, long wp)
{
	return (limit - base) - ring_valid_data(base, limit, rp, wp) - 1;
}

long get_general_instance_id(long instanceID, long pinID)
{
	return ((instanceID & 0xFFFFF000) | (pinID & 0xFFF));
}

void reset_pointer(struct rtk_compr_stream *rtk_stream)
{
	rtk_stream->consume_total = 0;
	rtk_stream->bytes_written = 0;
	rtk_stream->out_frames = 0;
	rtk_stream->is_get_info = false;
	rtk_stream->dwnstrmring_header->read_ptr[0] = rtk_stream->dwnstrmring_header->write_ptr;
	rtk_stream->last_inring_rp = rtk_stream->dec_inring_header->write_ptr;
}

int get_sample_index(unsigned int sample_rate)
{
	int i;
	static const unsigned int aac_samplerate_tab[12] = {
		96000, 88200, 64000, 48000, 44100,
		32000, 24000, 22050, 16000, 12000,
		11025, 8000};

	for (i = 0; i < ARRAY_SIZE(aac_samplerate_tab); i++) {
		if (sample_rate == aac_samplerate_tab[i])
			return i;
	}

	return -1;
}

static unsigned int snd_channel_mapping(int num_channels)
{
	unsigned int channel_mapping_info;
	switch (num_channels) {
	case 1:
		channel_mapping_info = (0x1) << 6;
		break;
	case 2:
		channel_mapping_info = (0x3) << 6;
		break;
	case 4:
		channel_mapping_info = (0x33) << 6;
		break;
	case 6:
		channel_mapping_info = (0x3F) << 6;
		break;
	case 8:
		channel_mapping_info = (0x63F) << 6;
		break;
	default:
		channel_mapping_info = (0x3) << 6;
		break;
	}

	return channel_mapping_info;
}

static unsigned long buf_memcpy2_ring(unsigned long base,
				      unsigned long limit,
				      unsigned long ptr,
				      char *buf,
				      unsigned long size)
{
	if (ptr + size <= limit) {
		if (copy_from_user((char *)ptr, buf, size))
			pr_err("%s remain data need to write\n",__func__);
	} else {
		int i = limit - ptr;

		if (copy_from_user((char *)ptr,(char *)buf, i))
			pr_err("%s remain data need to write\n",__func__);

		if (copy_from_user((char *)base,(char *)(buf + i), size - i))
			pr_err("%s remain data need to write\n",__func__);
	}

	ptr += size;
	if (ptr >= limit)
		ptr = base + (ptr - limit);

	return ptr;
}

static unsigned long buf_memcpy3_ring(unsigned long base,
				      unsigned long limit,
				      unsigned long ptr,
				      char* buf,
				      unsigned long size)
{
	if (ptr + size <= limit) {
		memcpy((char*)ptr, buf, size);
	} else {
		int i = limit - ptr;
		memcpy((char*)ptr,(char*)buf, i);
		memcpy((char*)base,(char*)(buf + i), size - i);
	}

	ptr += size;
	if (ptr >= limit)
		ptr = base + (ptr - limit);

	return ptr;
}

static void write_data(struct rtk_compr_stream *rtk_stream, void *data, int len)
{
	unsigned long base, limit, wp;

	base = (unsigned long)rtk_stream->dec_inring;
	limit = base + rtk_stream->dec_inring_header->size;
	wp = base + rtk_stream->dec_inring_header->write_ptr - rtk_stream->dec_inring_header->begin_addr;

	wp = buf_memcpy2_ring(base, limit, wp, (char *)data, (unsigned long)len);
	rtk_stream->dec_inring_header->write_ptr = (int)(wp - base) + rtk_stream->dec_inring_header->begin_addr;
}

static void write_inband_cmd(struct rtk_compr_stream *rtk_stream, void *data, int len)
{
	unsigned long base, limit, wp;

	base = (unsigned long)rtk_stream->inbandring;
	limit = base + rtk_stream->inbandring_header->size;
	wp = base + rtk_stream->inbandring_header->write_ptr - rtk_stream->inbandring_header->begin_addr;

	wp = buf_memcpy3_ring(base, limit, wp, (char *)data, (unsigned long)len);
	rtk_stream->inbandring_header->write_ptr = (int)(wp - base) + rtk_stream->inbandring_header->begin_addr;
}

/*
 * For Netflix diretOutput Used
 * Need write PTS Inband Command and Data
 */
static int direct_writedata(struct rtk_compr_stream *rtk_stream,
			    void *data, int len, int *hdr_size)
{
	char *cbuf = (char *)data;
	char *header_buf = NULL;
	char delim_v1[4] = {0x55, 0x55, 0x00, 0x01};
	char delim_v2[4] = {0x55, 0x55, 0x00, 0x02};
	struct audio_dec_pts_info cmd;
	int remain_len = len;
	int write_len = 0;
	int ret_len = 0;
	int size_inbyte = 0;
	uint64_t timestamp, timestampref;
	bool is_header = false;

	if (rtk_stream->avsync_hdr_offset != -1) {
		/* With hw av sync header */
		header_buf = kmalloc(sizeof(char) * len, GFP_KERNEL);
		if (!header_buf) {
			pr_err("%s kmalloc fail\n",__func__);
			return -ENOMEM;
		}

		/* copy data to kernel space to parse */
		if (copy_from_user(header_buf, cbuf, len)) {
			pr_err("%s copy data fail\n",__func__);
			return -EIO;
		}

		/* write the remain data */
		if (rtk_stream->avsync_hdr_offset > 0) {
			if (rtk_stream->avsync_hdr_offset <= remain_len)
				write_len = rtk_stream->avsync_hdr_offset;
			else
				write_len = remain_len;

			write_data(rtk_stream, cbuf, write_len);
			ret_len = ret_len + write_len;
			remain_len = remain_len - write_len;
			rtk_stream->avsync_hdr_offset -= write_len;
			cbuf = cbuf + write_len;
		}

		/* check header format once */
		if (!rtk_stream->is_check_format) {
			if (!memcmp(header_buf, delim_v2, 4)) {
				rtk_stream->delim_format = 2;
				rtk_stream->is_check_format = true;
				rtk_stream->delim_head_size = (int)header_buf[19];
				pr_info("delim format v2 delim_head_size %d\n", rtk_stream->delim_head_size);
			} else if (!memcmp(header_buf, delim_v1, 4)) {
				rtk_stream->delim_format = 1;
				rtk_stream->is_check_format = true;
				rtk_stream->delim_head_size = 16;
				pr_info("delim format v1\n");
			}
		}

		/* With hw av sync header */
		while (remain_len > 0) {
			if (!memcmp(header_buf, delim_v2, 4) ||
				!memcmp(header_buf, delim_v1, 4)) {
				is_header = true;
				if (remain_len < rtk_stream->delim_head_size)
					return ret_len;
			}

			if (is_header) {
				/* Write header */
				memset(&cmd, 0, sizeof(cmd));
				size_inbyte = header_buf[7] | (header_buf[6] << 8) | (header_buf[5] << 16)| (header_buf[4] << 24);
				cmd.header.type = AUDIO_DEC_INBAND_CMD_TYPE_PTS;
				cmd.header.size = sizeof(struct audio_dec_pts_info);
				cmd.PTSH = header_buf[11] | (header_buf[10] << 8) | (header_buf[9] << 16) | (header_buf[8] << 24);
				cmd.PTSL = header_buf[15] | (header_buf[14] << 8) | (header_buf[13] << 16) | (header_buf[12] << 24);
				cmd.w_ptr = rtk_stream->dec_inring_header->write_ptr;
				timestamp = (((int64_t)cmd.PTSH << 32) | ((int64_t)cmd.PTSL & 0xffffffff)) * 9;
				timestamp = div64_ul(timestamp, 100000);
				cmd.PTSH = (int32_t)(timestamp >> 32)& 0xffffffff;
				cmd.PTSL = (int32_t)(timestamp & 0xffffffffLL);
				if (rtk_stream->compr_first_pts) {
					timestampref = __cpu_to_be64(rtk_stream->refclock->audioSystemPTS);
					if (timestamp < timestampref) {
						rtk_stream->refclock->mastership.audioMode = AVSYNC_AUTO_MASTER;
						rtk_stream->refclock->mastership.videoMode = AVSYNC_AUTO_SLAVE;
						pr_info("[seek forward] Change to auto master mode\n");
					}
					rtk_stream->compr_first_pts = false;
				}

				/* Print more message according user flag */
				if (rtk_stream->log_lv >= 1) {
					if (rtk_stream->data_len != size_inbyte) {
						pr_info(" old data len %d new data len %d \n", rtk_stream->data_len, size_inbyte);
						rtk_stream->data_len = size_inbyte;
					}
					if (rtk_stream->log_lv == 3)
						pr_info("timestamp_90k 0x%llx size_inbyte %d \n", timestamp, size_inbyte);
				}

				write_inband_cmd(rtk_stream, &cmd, sizeof(cmd));

				/* move header part to data */
				*hdr_size = rtk_stream->delim_head_size;
				cbuf += rtk_stream->delim_head_size;
				remain_len -= rtk_stream->delim_head_size;

				/* Write data */
				if (remain_len > size_inbyte)
					write_len = size_inbyte;
				else
					write_len = remain_len;

				write_data(rtk_stream, cbuf, write_len);
				ret_len = ret_len + write_len;
				remain_len = remain_len - write_len;
				cbuf = cbuf + write_len;
				rtk_stream->avsync_hdr_offset += size_inbyte - write_len;
			} else {
				if (remain_len > 0)
					write_len = remain_len;
				else
					write_len = 0;

				write_data(rtk_stream, cbuf, write_len);
				ret_len = ret_len + write_len;
				remain_len = remain_len - write_len;
				cbuf = cbuf + write_len;
			}
		}

		kfree(header_buf);
	} else {
		/* without hw av sync */
		if (remain_len > 0)
			write_len = remain_len;
		else
			write_len = 0;

		write_data(rtk_stream, cbuf, write_len);
		ret_len = ret_len + write_len;
		remain_len = remain_len - write_len;
		cbuf = cbuf + write_len;
	}

	return ret_len;
}

int trigger_audio(struct rtk_compr_stream *rtk_stream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_info("TRIGGER_START/TRIGGER_PAUSE_RELEASE\n");
		if (rtk_stream->status != SNDRV_PCM_TRIGGER_START) {
			rpc_run_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				    rtk_stream->audio_out_id);
			if (!rtk_stream->multi_ao)
				rpc_run_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
					    get_general_instance_id(rtk_stream->audio_pp_id, rtk_stream->audio_app_pin_id));
			rpc_run_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				    rtk_stream->audio_dec_id);
		}
		rtk_stream->status = SNDRV_PCM_TRIGGER_START;
		rtk_stream->compr_first_pts = true;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_info("TRIGGER_STOP\n");
		if (rtk_stream->status != SNDRV_PCM_TRIGGER_STOP) {
			struct audio_rpc_sendio sendio;

			rtk_stream->refclock->RCD = -1;
			rtk_stream->refclock->mastership.systemMode = AVSYNC_FORCED_SLAVE;
			rtk_stream->refclock->mastership.audioMode = AVSYNC_FORCED_MASTER;
			rtk_stream->refclock->mastership.videoMode = AVSYNC_FORCED_MASTER;
			rtk_stream->refclock->mastership.masterState = AUTOMASTER_NOT_MASTER;
			rtk_stream->refclock->videoFreeRunThreshold = 0x7FFFFFFF;
			rtk_stream->refclock->audioFreeRunThreshold = 0x7FFFFFFF;

			/* decoder flush */
			sendio.instance_id = rtk_stream->audio_dec_id;
			sendio.pin_id = rtk_stream->audio_dec_pin_id;
			if (rpc_flush_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc, &sendio)) {
				pr_err("[%s fail]\n", __func__);
				return -1;
			}

			reset_pointer(rtk_stream);

			rpc_stop_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				     rtk_stream->audio_out_id);
 
			if (!rtk_stream->multi_ao)
				rpc_stop_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
					     get_general_instance_id(rtk_stream->audio_pp_id, rtk_stream->audio_app_pin_id));
			rpc_stop_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				     rtk_stream->audio_dec_id);
		}
		rtk_stream->status = SNDRV_PCM_TRIGGER_STOP;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_info("TRIGGER_PAUSE_PUSH\n");
		if (rtk_stream->status != SNDRV_PCM_TRIGGER_PAUSE_PUSH) {
			rpc_pause_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				      rtk_stream->audio_out_id);

			if (!rtk_stream->multi_ao)
				rpc_pause_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
					      get_general_instance_id(rtk_stream->audio_pp_id, rtk_stream->audio_app_pin_id));

			rpc_pause_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				      rtk_stream->audio_dec_id);
		}
		rtk_stream->refclock->RCD = -1;
		rtk_stream->refclock->mastership.audioMode = AVSYNC_FORCED_MASTER;
		rtk_stream->refclock->mastership.videoMode = AVSYNC_FORCED_SLAVE;
		rtk_stream->status = SNDRV_PCM_TRIGGER_PAUSE_PUSH;
		break;
	default:
		break;
	}

	return 0;
}

void destroy_audio_component(struct rtk_compr_stream *rtk_stream)
{
	struct audio_rpc_ringbuffer_header ring_header;

	trigger_audio(rtk_stream, SNDRV_PCM_TRIGGER_STOP);

	if (!rtk_stream->multi_ao) {
		rpc_pp_init_pin_svc(rtk_stream->phy_addr_rpc,
				    rtk_stream->vaddr_rpc,
				    get_general_instance_id(rtk_stream->audio_pp_id, rtk_stream->audio_app_pin_id));

		rpc_destroy_svc(rtk_stream->phy_addr_rpc,
				rtk_stream->vaddr_rpc,
				get_general_instance_id(rtk_stream->audio_pp_id, rtk_stream->audio_app_pin_id));
	}

	rpc_destroy_svc(rtk_stream->phy_addr_rpc,
			rtk_stream->vaddr_rpc,
			rtk_stream->audio_dec_id);

	if (rtk_stream->multi_ao) {
		ring_header.instance_id = rtk_stream->audio_out_id;
		ring_header.pin_id = PCM_IN;

		if (rpc_initringbuffer_header(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
					      &ring_header, 0) < 0) {
			pr_err("[%s %d fail]\n", __func__, __LINE__);
		}
	}

	rtk_stream->audio_out_id = 0;
	rtk_stream->audio_dec_id = 0;
	rtk_stream->audio_dec_pin_id = 0;
	if (!rtk_stream->multi_ao)
		rtk_stream->audio_pp_id = 0;
	rtk_stream->audio_app_pin_id = 0;
}

int create_audio_component(struct rtk_compr_stream *rtk_stream)
{
	/* create decoder agent */
	if (rpc_create_decoder_agent(rtk_stream->phy_addr_rpc,
				     rtk_stream->vaddr_rpc,
				     &rtk_stream->audio_dec_id,
				     &rtk_stream->audio_dec_pin_id)) {
		pr_err("[%s fail]\n", __func__);
		return -ENOMEM;
	}

	if (!rtk_stream->multi_ao) {
		/* create pp agent and get global pp pin */
		if (rpc_create_pp_agent(rtk_stream->phy_addr_rpc,
					rtk_stream->vaddr_rpc,
					&rtk_stream->audio_pp_id,
					&rtk_stream->audio_app_pin_id)) {
			pr_err("[%s fail]\n", __func__);
			return -ENOMEM;
		}
	}
	if (rtk_stream->multi_ao)
		rtk_stream->audio_app_pin_id = PCM_IN;

	if (!rtk_stream->multi_ao)
		rtk_stream->audio_out_id =
				get_general_instance_id(rtk_stream->audio_out_id,
							rtk_stream->audio_app_pin_id);

	return 0;
}

static void destroy_ringbuf(struct rtk_compr_stream *rtk_stream, struct device *dev)
{
	int ch;
	phys_addr_t dat;
	void *vaddr;
	size_t size;

	if (rtk_stream->is_low_water) {
		rpc_set_low_water_level(rtk_stream->phy_addr_rpc,
					rtk_stream->vaddr_rpc, false);
	}

	if (rtk_stream->refclock) {
		if (rtk_stream->refclk_from_video) {
			dma_buf_vunmap(rtk_stream->refclk_dmabuf, &rtk_stream->refclk_map);
			dma_buf_end_cpu_access(rtk_stream->refclk_dmabuf,
					DMA_BIDIRECTIONAL);
			dma_buf_unmap_attachment(rtk_stream->refclk_attach,
						rtk_stream->refclk_attach->sgt,
						DMA_BIDIRECTIONAL);
			dma_buf_detach(rtk_stream->refclk_dmabuf, rtk_stream->refclk_attach);
			dma_buf_put(rtk_stream->refclk_dmabuf);
			rtk_stream->refclk_dmabuf = NULL;
		} else {
			dat = rtk_stream->phy_refclock;
			vaddr = rtk_stream->refclock;
			size = sizeof(struct tag_refclock);
			dma_free_coherent(dev, size, vaddr, dat);
		}
		rtk_stream->refclock = NULL;
	}

	if (rtk_stream->dec_inring) {
		dat = rtk_stream->phy_dec_inring;
		vaddr = rtk_stream->dec_inring;
		size = rtk_stream->dec_insize;
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->dec_inring = NULL;
	}

	if (rtk_stream->dec_inring_header) {
		dat = rtk_stream->phy_dec_inring_header;
		vaddr = rtk_stream->dec_inring_header;
		size = SZ_4K;
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->dec_inring_header = NULL;
	}

	if (rtk_stream->inbandring) {
		dat = rtk_stream->phy_inbandring;
		vaddr = rtk_stream->inbandring;
		size = DEC_INBAND_BUFFER_SIZE;
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->inbandring = NULL;
	}

	if (rtk_stream->inbandring_header) {
		dat = rtk_stream->phy_inbandring_header;
		vaddr = rtk_stream->inbandring_header;
		size = SZ_4K;
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->inbandring_header = NULL;
	}

	if (rtk_stream->dwnstrmring) {
		dat = rtk_stream->phy_dwnstrmring;
		vaddr = rtk_stream->dwnstrmring;
		size = DEC_DWNSTRM_BUFFER_SIZE;
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->dwnstrmring = NULL;
	}

	if (rtk_stream->dwnstrmring_header) {
		dat = rtk_stream->phy_dwnstrmring_header;
		vaddr = rtk_stream->dwnstrmring_header;
		size = SZ_4K;
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->dwnstrmring_header = NULL;
	}

	for (ch=0; ch<AUDIO_DEC_OUTPIN; ch++) {
		if (rtk_stream->dec_outring[ch]) {
			dat = rtk_stream->phy_dec_outring[ch];
			vaddr = rtk_stream->dec_outring[ch];
			size = rtk_stream->dec_outsize;
			dma_free_coherent(dev, size, vaddr, dat);
			rtk_stream->dec_outring[ch] = NULL;
		}
	}

	for (ch=0; ch<AUDIO_DEC_OUTPIN; ch++) {
		if (rtk_stream->dec_outring_header[ch]) {
			dat = rtk_stream->phy_dec_outring_header[ch];
			vaddr = rtk_stream->dec_outring_header[ch];
			size = SZ_4K;
			dma_free_coherent(dev, size, vaddr, dat);
			rtk_stream->dec_outring_header[ch] = NULL;
		}
	}
}

static int create_ringbuf(struct rtk_compr_stream *rtk_stream,
			  struct device *dev)
{
	int ch, ret;
	phys_addr_t dat;
	void *vaddr;
	size_t size;
	struct audio_rpc_ringbuffer_header ring_header;
	struct audio_rpc_connection connection;
	struct audio_rpc_focus focus;

	/* create dec_inring */
	size = rtk_stream->dec_insize;
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_ACPUACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		goto fail;
	}

	rtk_stream->dec_inring = vaddr;
	rtk_stream->phy_dec_inring = dat;

	/* create dec_inring header */
	size = SZ_4K;
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_HIFIACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		goto fail;
	}

	rtk_stream->dec_inring_header = vaddr;
	rtk_stream->phy_dec_inring_header = dat;

	rtk_stream->dec_inring_header->begin_addr = rtk_stream->phy_dec_inring;
	rtk_stream->dec_inring_header->size = rtk_stream->dec_insize;
	rtk_stream->dec_inring_header->write_ptr = rtk_stream->dec_inring_header->begin_addr;
	rtk_stream->dec_inring_header->read_ptr[0] = rtk_stream->dec_inring_header->begin_addr;
	rtk_stream->dec_inring_header->num_read_ptr = 1;
	rtk_stream->last_inring_rp = rtk_stream->dec_inring_header->read_ptr[0];

	ring_header.instance_id = rtk_stream->audio_dec_id;
	ring_header.pin_id = BASE_BS_IN;
	ring_header.read_idx = 0;
	ring_header.list_size = 1;
	ring_header.ringbuffer_header_list[0] = rtk_stream->phy_dec_inring_header;

	if (rpc_initringbuffer_header(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				      &ring_header, ring_header.list_size) < 0) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		ret = -EINVAL;
		goto fail;
	}

	/* create dec_inband_inring */
	size = DEC_INBAND_BUFFER_SIZE;
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_ACPUACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		goto fail;
	}

	rtk_stream->inbandring = vaddr;
	rtk_stream->phy_inbandring = dat;

	/* create dec_inband_inring header */
	size = SZ_4K;
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_HIFIACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		goto fail;;
	}

	rtk_stream->inbandring_header = vaddr;
	rtk_stream->phy_inbandring_header = dat;

	rtk_stream->inbandring_header->begin_addr = rtk_stream->phy_inbandring;
	rtk_stream->inbandring_header->size = DEC_INBAND_BUFFER_SIZE;
	rtk_stream->inbandring_header->write_ptr = rtk_stream->inbandring_header->begin_addr;
	rtk_stream->inbandring_header->read_ptr[0] = rtk_stream->inbandring_header->begin_addr;
	rtk_stream->inbandring_header->num_read_ptr = 1;

	ring_header.instance_id = rtk_stream->audio_dec_id;
	ring_header.pin_id = INBAND_QUEUE;
	ring_header.read_idx = 0;
	ring_header.list_size = 1;
	ring_header.ringbuffer_header_list[0] = rtk_stream->phy_inbandring_header;

	if (rpc_initringbuffer_header(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				      &ring_header, ring_header.list_size) < 0) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		ret = -EINVAL;
		goto fail;
	}

	/* create dec_dwnstrmring */
	size = DEC_DWNSTRM_BUFFER_SIZE;
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_ACPUACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		goto fail;
	}

	rtk_stream->dwnstrmring = vaddr;
	rtk_stream->phy_dwnstrmring = dat;

	/* setup dwnstrm low and up */
	rtk_stream->dwnring_lower = rtk_stream->dwnstrmring;
	rtk_stream->dwnring_upper = rtk_stream->dwnstrmring + DEC_DWNSTRM_BUFFER_SIZE;

	/* create dec_dwnstrmring header */
	size = SZ_4K;
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_HIFIACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		goto fail;
	}

	rtk_stream->dwnstrmring_header = vaddr;
	rtk_stream->phy_dwnstrmring_header = dat;

	rtk_stream->dwnstrmring_header->begin_addr = rtk_stream->phy_dwnstrmring;
	rtk_stream->dwnstrmring_header->size = DEC_DWNSTRM_BUFFER_SIZE;
	rtk_stream->dwnstrmring_header->write_ptr = rtk_stream->dwnstrmring_header->begin_addr;
	rtk_stream->dwnstrmring_header->read_ptr[0] = rtk_stream->dwnstrmring_header->begin_addr;
	rtk_stream->dwnstrmring_header->num_read_ptr = 1;

	ring_header.instance_id = rtk_stream->audio_dec_id;
	ring_header.pin_id = DWNSTRM_INBAND_QUEUE;
	ring_header.read_idx = -1;
	ring_header.list_size = 1;
	ring_header.ringbuffer_header_list[0] = rtk_stream->phy_dwnstrmring_header;

	if (rpc_initringbuffer_header(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				      &ring_header, ring_header.list_size) < 0) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		ret = -EINVAL;
		goto fail;
	}

	/* create dec outring */
	for (ch=0; ch<AUDIO_DEC_OUTPIN; ch++) {
		size = rtk_stream->dec_outsize;
		ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
				RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
				RTK_FLAG_ACPUACC);
		if (ret) {
			dev_err(dev, "%s dma_alloc fail\n", __func__);
			goto fail;
		}

		rtk_stream->phy_dec_outring[ch] = dat;
		rtk_stream->dec_outring[ch] = vaddr;

		/* setup outring low and up */
		rtk_stream->dec_outring_lower[ch] = vaddr;
		rtk_stream->dec_outring_upper[ch] = vaddr + rtk_stream->dec_outsize;
	}

	/* create dec outring header */
	for (ch=0; ch<AUDIO_DEC_OUTPIN; ch++) {
		size = SZ_4K;
		ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
				RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
				RTK_FLAG_HIFIACC);
		if (ret) {
			dev_err(dev, "%s dma_alloc fail\n", __func__);
			goto fail;
		}

		rtk_stream->phy_dec_outring_header[ch] = dat;
		rtk_stream->dec_outring_header[ch] = vaddr;

		rtk_stream->dec_outring_header[ch]->begin_addr = rtk_stream->phy_dec_outring[ch];
		rtk_stream->dec_outring_header[ch]->size = rtk_stream->dec_outsize;
		rtk_stream->dec_outring_header[ch]->write_ptr = rtk_stream->dec_outring_header[ch]->begin_addr;
		rtk_stream->dec_outring_header[ch]->read_ptr[0] = rtk_stream->dec_outring_header[ch]->begin_addr;
		rtk_stream->dec_outring_header[ch]->read_ptr[1] = rtk_stream->dec_outring_header[ch]->begin_addr;
		rtk_stream->dec_outring_header[ch]->read_ptr[2] = rtk_stream->dec_outring_header[ch]->begin_addr;
		rtk_stream->dec_outring_header[ch]->read_ptr[3] = rtk_stream->dec_outring_header[ch]->begin_addr;
		rtk_stream->dec_outring_header[ch]->num_read_ptr = 1;
	}

	ring_header.instance_id = rtk_stream->audio_dec_id;
	ring_header.pin_id = PCM_OUT;
	ring_header.read_idx = -1;
	ring_header.list_size = AUDIO_DEC_OUTPIN;
	for (ch=0; ch<AUDIO_DEC_OUTPIN; ch++)
		ring_header.ringbuffer_header_list[ch] = rtk_stream->phy_dec_outring_header[ch];

	if (rpc_initringbuffer_header(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
				      &ring_header, ring_header.list_size) < 0) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		ret = -EINVAL;
		goto fail;
	}
	if (!rtk_stream->multi_ao) {
		ring_header.instance_id = rtk_stream->audio_pp_id;
		ring_header.pin_id = rtk_stream->audio_app_pin_id;
		ring_header.read_idx = 0;
		ring_header.list_size = AUDIO_DEC_OUTPIN;
		for (ch=0; ch<AUDIO_DEC_OUTPIN; ch++)
			ring_header.ringbuffer_header_list[ch] = rtk_stream->phy_dec_outring_header[ch];

		if (rpc_initringbuffer_header(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
					      &ring_header, ring_header.list_size) < 0) {
			pr_err("[%s %d fail]\n", __func__, __LINE__);
			ret = -EINVAL;
			goto fail;
		}
	} else {
		ring_header.instance_id = rtk_stream->audio_out_id;
		ring_header.pin_id = PCM_IN;
		ring_header.read_idx = 0;
		ring_header.list_size = AUDIO_DEC_OUTPIN;
		for (ch=0; ch<AUDIO_DEC_OUTPIN; ch++)
			ring_header.ringbuffer_header_list[ch] = rtk_stream->phy_dec_outring_header[ch];

		if (rpc_initringbuffer_header(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
					      &ring_header, ring_header.list_size) < 0) {
			pr_err("[%s %d fail]\n", __func__, __LINE__);
			ret = -EINVAL;
			goto fail;
		}
	}

	/* connection */
	if (!rtk_stream->multi_ao)
		connection.des_instance_id = rtk_stream->audio_pp_id;
	else
		connection.des_instance_id = rtk_stream->audio_out_id;

	connection.des_pin_id = rtk_stream->audio_app_pin_id;
	connection.src_instance_id = rtk_stream->audio_dec_id;
	connection.src_pin_id = PCM_OUT;
	if (rpc_connect_svc(rtk_stream->phy_addr_rpc,
			    rtk_stream->vaddr_rpc,
			    &connection)) {
		pr_err("[%s connection fail]\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	/* set focus on specific pp */
	if (!rtk_stream->multi_ao) {
		focus.instance_id = rtk_stream->audio_pp_id;
		focus.focus_id = rtk_stream->audio_app_pin_id;
	} else {
		focus.instance_id = rtk_stream->audio_out_id;
		focus.focus_id = 0;
	}

	if (rpc_switch_focus(rtk_stream->phy_addr_rpc,
			     rtk_stream->vaddr_rpc, &focus)) {
		pr_err("[%s] set focus fail\n", __func__);
		ret = -EINVAL;
		goto fail;
	}

	return 0;
fail:
	destroy_ringbuf(rtk_stream, dev);
	return ret;
}

static int snd_card_compr_open(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_card *card = cstream->device->private_data;
	struct device *dev = card->dev;
	struct device_node *node = dev->of_node;
	struct rtk_compr_dev *compr_dev = dev_get_drvdata(dev);
	struct rtk_compr_stream *rtk_stream = NULL;
	struct rtk_snd_mixer *mixer = (struct rtk_snd_mixer *)card->private_data;
	int ret = 0;
	phys_addr_t dat;
	void *vaddr;
	size_t size;

	pr_info("ALSA Compress open: Device number is %d\n", cstream->device->device);
	switch (cstream->device->device) {
	case 0:
		pr_info("ALSA Compress: First AO flow.\n");
		break;
	case 1:
		pr_info("ALSA Compress: Second AO flow.\n");
		break;
	default:
		pr_info("ALSA Compress: Unknown device number.\n");
		break;
	}

	size = sizeof(struct rtk_compr_stream);
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_ACPUACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	rtk_stream = vaddr;
	memset(rtk_stream, 0, sizeof(struct rtk_compr_stream));
	rtk_stream->phy_addr = dat;
	of_property_read_u32(node, "multi-ao", &rtk_stream->multi_ao);

	/* Preparing a memory for all rpc */
	size = SZ_4K;
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_ACPUACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		ret = -ENOMEM;
		goto fail_rpc;
	}

	rtk_stream->vaddr_rpc = vaddr;
	rtk_stream->phy_addr_rpc = dat;
	rtk_stream->audio_out_id = compr_dev->ao_agent_id;
	runtime->private_data = rtk_stream;

	if (create_audio_component(rtk_stream)) {
		dev_err(dev, "create audio component failed\n");
		ret = -EBUSY;
		goto fail_dat;
	}
	rtk_stream->status = SNDRV_PCM_TRIGGER_STOP;

	rpc_set_dec_volume_config(dev, 1, 0, rtk_stream->audio_out_id);
	mixer->ao_agent_id = rtk_stream->audio_out_id;
	/* Preparing the ion buffer for share memory 1 */
	size = SZ_4K;
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_ACPUACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		ret = -ENOMEM;
		goto fail_dat;
	}

	rtk_stream->phy_rawdelay = dat;
	rtk_stream->rawdelay_mem = vaddr;
	memset(rtk_stream->rawdelay_mem, 0, 4096);

	/* Preparing the ion buffer for share memory 2 */
	size = sizeof(struct alsa_raw_latency);
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_ACPUACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		ret = -ENOMEM;
		goto fail_dat2;
	}

	rtk_stream->phy_rawdelay2 = dat;
	rtk_stream->rawdelay_mem2 = vaddr;
	memset(rtk_stream->rawdelay_mem2, 0, sizeof(struct alsa_raw_latency));

	/* Set up the sync word for new latency structure */
	rtk_stream->rawdelay_mem2->sync = 0x23792379;

	/* Preparing the ion buffer for share memory 3 */
	size = sizeof(struct alsa_raw_latency);
	ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
			    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			    RTK_FLAG_ACPUACC);
	if (ret) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		ret = -ENOMEM;
		goto fail_dat3;
	}

	rtk_stream->phy_rawdelay3 = dat;
	rtk_stream->rawdelay_mem3 = vaddr;
	memset(rtk_stream->rawdelay_mem3, 0, sizeof(struct alsa_raw_latency));

	/* Set up the sync word for new latency structure */
	rtk_stream->rawdelay_mem3->sync = 0x23792379;

	return ret;
fail_dat3:
	size = sizeof(struct alsa_raw_latency);
	dat = rtk_stream->phy_rawdelay2;
	vaddr = rtk_stream->rawdelay_mem2;
	dma_free_coherent(dev, size, vaddr, dat);
fail_dat2:
	size = SZ_4K;
	dat = rtk_stream->phy_rawdelay;
	vaddr = rtk_stream->rawdelay_mem;
	dma_free_coherent(dev, size, vaddr, dat);
fail_dat:
	size = SZ_4K;
	dat = rtk_stream->phy_addr_rpc;
	vaddr = rtk_stream->vaddr_rpc;
	dma_free_coherent(dev, size, vaddr, dat);
fail_rpc:
	size = sizeof(struct rtk_compr_stream);
	vaddr = rtk_stream;
	dat = rtk_stream->phy_addr;
	dma_free_coherent(dev, size, vaddr, dat);
	rtk_stream = NULL;
	runtime->private_data = NULL;
fail:
	return ret;
}

static int snd_card_compr_free(struct snd_compr_stream *cstream)
{
	struct rtk_compr_stream *rtk_stream = cstream->runtime->private_data;
	struct snd_card *card = cstream->device->private_data;
	struct device *dev = card->dev;
	phys_addr_t dat;
	void *vaddr;
	size_t size;

	if (rtk_stream->rawdelay_mem) {
		dat = rtk_stream->phy_rawdelay;
		vaddr = rtk_stream->rawdelay_mem;
		size = SZ_4K;
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->rawdelay_mem = NULL;
	}

	if (rtk_stream->rawdelay_mem2) {
		dat = rtk_stream->phy_rawdelay2;
		vaddr = rtk_stream->rawdelay_mem2;
		size = sizeof(struct alsa_raw_latency);
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->rawdelay_mem2 = NULL;
	}

	if (rtk_stream->rawdelay_mem3) {
		dat = rtk_stream->phy_rawdelay3;
		vaddr = rtk_stream->rawdelay_mem3;
		size = sizeof(struct alsa_raw_latency);
		dma_free_coherent(dev, size, vaddr, dat);
		rtk_stream->rawdelay_mem3 = NULL;
	}

	if (rtk_stream->audio_dec_id != 0)
		destroy_audio_component(rtk_stream);

	/* free buffer used in decoder */
	destroy_ringbuf(rtk_stream, dev);

	/* free buf for rpc */
	dat = rtk_stream->phy_addr_rpc;
	vaddr = rtk_stream->vaddr_rpc;
	size = SZ_4K;
	dma_free_coherent(dev, size, vaddr, dat);
	rtk_stream->vaddr_rpc = NULL;

	/* free rtk_compr_stream */
	dat = rtk_stream->phy_addr;
	vaddr = rtk_stream;
	size = SZ_4K;
	dma_free_coherent(dev, size, vaddr, dat);
	rtk_stream = NULL;

	return 0;
}

static int snd_card_compr_set_params(struct snd_compr_stream *cstream,
				     struct snd_compr_params *params)
{
	struct rtk_compr_stream *rtk_stream = cstream->runtime->private_data;
	struct snd_card *card = cstream->device->private_data;
	struct device *dev = card->dev;
	struct audio_rpc_refclock audio_ref_clock;
	struct audio_rpc_sendio sendio;
	struct audio_dec_new_format cmd;
	int ret = 0, refclk_handle;
	phys_addr_t dat;
	void *vaddr;
	size_t size;

	/* add 128 byte because audio fw can NOT set wp = rp.
	 * max buffer can write is (buffer size - 1)
	 * also for HIFI 128 align
	 */
	rtk_stream->dec_insize = params->buffer.fragment_size * params->buffer.fragments + 128;

	/* bit[2] and bit[1] contain message log level */
	if (params->codec.reserved[1] & 0x6) {
		rtk_stream->log_lv = (params->codec.reserved[1] & 0x6) >> 1;
		pr_info("snd-hifi-rtk-compress log_lv %d", rtk_stream->log_lv);
	}

	rtk_stream->is_low_water = false;
	if (params->codec.reserved[1] & 0x1) {
		/*
		 * bit 0 is for low waterlevel mode
		 * must be set before creating ring buffer
		 */
		rtk_stream->is_low_water = true;
	}

	/* Set low water mode for according to format & sample rate */
	if (rtk_stream->is_low_water) {
		rtk_stream->dec_outsize = DEC_OUT_BUFFER_SIZE_LOW;
		rpc_set_low_water_level(rtk_stream->phy_addr_rpc,
					rtk_stream->vaddr_rpc, true);
	} else {
		rtk_stream->dec_outsize = DEC_OUT_BUFFER_SIZE;
	}

	if (create_ringbuf(rtk_stream, dev)) {
		pr_err("[%s] create_ringbuf failed\n", __func__);
		return -1;
	}

	/*
	 * For Netflix
	 * Previous refclk_handle should free by audio hal
	 */
	if (params->codec.reserved[0] > 0) {
		/* with hw av sync */
		rtk_stream->refclk_from_video = 1;
		rtk_stream->avsync_hdr_offset = 0;

		refclk_handle = params->codec.reserved[0];
		if (refclk_handle < 0) {
			pr_err("[%s refclk_handle fail]\n", __func__);
			return -1;
		} else {
			pr_info("refclk_handle:%d\n", refclk_handle);
		}

		rtk_stream->refclk_dmabuf = dma_buf_get(refclk_handle);
		if (IS_ERR(rtk_stream->refclk_dmabuf)) {
			pr_err("[ALSA %s %d fail]\n", __func__, __LINE__);
			return PTR_ERR(rtk_stream->refclk_dmabuf);
		}

		rtk_stream->refclk_attach = dma_buf_attach(rtk_stream->refclk_dmabuf, dev);
		if (IS_ERR(rtk_stream->refclk_attach)) {
			dev_err(dev, "failed to attach dma-buf: %d\n", ret);
			goto attach_err;
		}

		rtk_stream->refclk_table =
			dma_buf_map_attachment(rtk_stream->refclk_attach,
					       DMA_BIDIRECTIONAL);
		if (IS_ERR(rtk_stream->refclk_table)) {
			dev_err(dev, "failed to map refclk_attach: %d\n", ret);
			goto map_err;
		}

		ret = dma_buf_begin_cpu_access(rtk_stream->refclk_dmabuf, DMA_BIDIRECTIONAL);
		if (ret) {
			dev_err(dev, "failed dma_buf_begin_cpu_access\n");
			goto access_err;
		}

		ret = dma_buf_vmap(rtk_stream->refclk_dmabuf, &rtk_stream->refclk_map);
		if (ret) {
			dev_err(dev, "Failed to vmap buffer for refclk\n");
			goto kmap_err;
		}
		rtk_stream->refclock = (struct tag_refclock *)rtk_stream->refclk_map.vaddr;
		rtk_stream->phy_refclock = sg_phys(rtk_stream->refclk_table->sgl);
		pr_info("rtk_stream->phy_refclock:%lx\n", (unsigned long)rtk_stream->phy_refclock);

		rtk_stream->refclock->mastership.systemMode = AVSYNC_FORCED_SLAVE;
		rtk_stream->refclock->mastership.audioMode = AVSYNC_FORCED_MASTER;
		rtk_stream->refclock->mastership.videoMode = AVSYNC_FORCED_SLAVE;
		rtk_stream->refclock->mastership.masterState = AUTOMASTER_NOT_MASTER;
		rtk_stream->refclock->videoFreeRunThreshold = 0x7FFFFFFF;
		rtk_stream->refclock->audioFreeRunThreshold = 0x7FFFFFFF;
	} else {
		/* without hw av sync */
		rtk_stream->refclk_from_video = 0;
		rtk_stream->avsync_hdr_offset = -1;

		size = sizeof(struct tag_refclock);
		ret = rtk_snd_alloc(dev, size, (void *)&dat, (void **)&vaddr,
				    RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
				    RTK_FLAG_HIFIACC);
		if (ret) {
			dev_err(dev, "%s dma_alloc fail\n", __func__);
			return ret;
		}

		rtk_stream->phy_refclock = dat;
		rtk_stream->refclock = vaddr;
		memset(rtk_stream->refclock, 0, sizeof(struct tag_refclock));

		rtk_stream->refclock->mastership.systemMode = AVSYNC_FORCED_SLAVE;
		rtk_stream->refclock->mastership.audioMode = AVSYNC_FORCED_MASTER;
		rtk_stream->refclock->mastership.videoMode = AVSYNC_FORCED_SLAVE;
		rtk_stream->refclock->mastership.masterState = AVSYNC_FORCED_SLAVE;
		rtk_stream->refclock->videoFreeRunThreshold = 0x7FFFFFFF;
		rtk_stream->refclock->audioFreeRunThreshold = 0x7FFFFFFF;
	}

	audio_ref_clock.instance_id = rtk_stream->audio_out_id;
	if (!rtk_stream->multi_ao)
		audio_ref_clock.pref_clock_id = rtk_stream->audio_app_pin_id;
	else
		audio_ref_clock.pref_clock_id = 0;

	audio_ref_clock.pref_clock = rtk_stream->phy_refclock;

	if (rpc_set_refclock(rtk_stream->phy_addr_rpc,
			     rtk_stream->vaddr_rpc,
			     &audio_ref_clock)) {
		pr_err("[%s] rpc_set_refclock failed\n", __func__);
		goto rpc_ret_err;
	}

	rtk_stream->codec_id = params->codec.id;
	rtk_stream->audio_channel = params->codec.ch_in;
	rtk_stream->audio_sampling_rate = params->codec.sample_rate;

	/* decoder flush */
	sendio.instance_id = rtk_stream->audio_dec_id;
	sendio.pin_id = rtk_stream->audio_dec_pin_id;
	if (rpc_flush_svc(rtk_stream->phy_addr_rpc, rtk_stream->vaddr_rpc,
			  &sendio)) {
		pr_err("[%s %d fail]\n", __func__, __LINE__);
		goto rpc_ret_err;
	}

	/* run audio component */
	trigger_audio(rtk_stream, SNDRV_PCM_TRIGGER_START);

	/* send audio format to inband cmd */
	memset(&cmd, 0, sizeof(cmd));

	switch (params->codec.id) {
	case SND_AUDIOCODEC_PCM:
		pr_info("SND_AUDIOCODEC_PCM\n");
		cmd.audio_type = AUDIO_LPCM_DECODER_TYPE;
		cmd.private_info[0] = params->codec.ch_in;
		cmd.private_info[1] = 16;
		cmd.private_info[2] = params->codec.sample_rate;
		cmd.private_info[3] = 0;
		cmd.private_info[4] = snd_channel_mapping(params->codec.ch_in);
		cmd.private_info[5] = 0;
		cmd.private_info[6] = 0;
		cmd.private_info[7] = AUDIO_LITTLE_ENDIAN;
		break;
	case SND_AUDIOCODEC_MP3:
		pr_info("SND_AUDIO_MP3\n");
		cmd.audio_type = AUDIO_MPEG_DECODER_TYPE;
		break;
	case SND_AUDIOCODEC_AAC:
		pr_info("SND_AUDIO_AAC\n");
		cmd.audio_type = AUDIO_NEW_AAC_DECODER_TYPE;
		cmd.private_info[0] = 0;
		break;
	case SND_AUDIOCODEC_AC3:
		pr_info("SND_AUDIO_AC3\n");
		cmd.audio_type = AUDIO_AC3_DECODER_TYPE;
		break;
	case SND_AUDIOCODEC_EAC3:
		pr_info("SND_AUDIO_EAC3\n");
		cmd.audio_type = AUDIO_DDP_DECODER_TYPE;
		break;
	case SND_AUDIOCODEC_DTS:
		pr_info("SND_AUDIO_DTS\n");
		cmd.audio_type = AUDIO_DTS_DECODER_TYPE;
		break;
	case SND_AUDIOCODEC_DTS_HD:
		pr_info("SND_AUDIO_DTS_HD\n");
		cmd.audio_type = AUDIO_DTS_HD_DECODER_TYPE;
		break;
	case SND_AUDIOCODEC_TRUEHD:
		pr_info("SND_AUDIO_TRUEHD\n");
		cmd.audio_type = AUDIO_MLP_DECODER_TYPE;
		break;
	case SND_AUDIOCODEC_APE:
		pr_info("SND_AUDIO_APE\n");
		cmd.audio_type = AUDIO_APE_DECODER_TYPE;
		break;
	default:
		pr_err("[%s %d] audio format not support\n", __func__, __LINE__);
		break;
	}

	cmd.header.type = AUDIO_DEC_INBAMD_CMD_TYPE_NEW_FORMAT;
	cmd.header.size = sizeof(struct audio_dec_new_format);
	cmd.w_ptr = rtk_stream->phy_dec_inring;

	write_inband_cmd(rtk_stream, &cmd, sizeof(struct audio_dec_new_format));

	return 0;

rpc_ret_err:
	if (!rtk_stream->refclk_from_video)
		return -1;

	dma_buf_vunmap(rtk_stream->refclk_dmabuf,
		       &rtk_stream->refclk_map);
kmap_err:
	dma_buf_end_cpu_access(rtk_stream->refclk_dmabuf,
			       DMA_BIDIRECTIONAL);
access_err:
	dma_buf_unmap_attachment(rtk_stream->refclk_attach,
				 rtk_stream->refclk_attach->sgt,
				 DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(rtk_stream->refclk_dmabuf,
		       rtk_stream->refclk_attach);
attach_err:
	dma_buf_put(rtk_stream->refclk_dmabuf);
	return -ENOMEM;
}

static int snd_card_compr_get_params(struct snd_compr_stream *cstream,
				     struct snd_codec *params)
{
	return 0;
}

static int snd_card_compr_set_metadata(struct snd_compr_stream *cstream,
				       struct snd_compr_metadata *metadata)
{
	return 0;
}

static int snd_card_compr_get_metadata(struct snd_compr_stream *cstream,
				       struct snd_compr_metadata *metadata)
{
	return 0;
}

static int snd_card_compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct rtk_compr_stream *rtk_stream = cstream->runtime->private_data;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (trigger_audio(rtk_stream, cmd))
			pr_err("[%s fail]\n", __func__);
		break;
	case SND_COMPR_TRIGGER_DRAIN:
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
		pr_info("TRIGGER_DRAIN/TRIGGER_PARTIAL_DRAIN\n");
		break;
	case SND_COMPR_TRIGGER_NEXT_TRACK:
		pr_info("TRIGGER_NEXT_TRACK\n");
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int snd_card_compr_pointer(struct snd_compr_stream *cstream,
				  struct snd_compr_tstamp *tstamp)
{
	struct rtk_compr_stream *rtk_stream = cstream->runtime->private_data;
	unsigned char *ptswp = rtk_stream->dwnstrmring +
		rtk_stream->dwnstrmring_header->write_ptr - rtk_stream->dwnstrmring_header->begin_addr;
	unsigned char *ptsrp = rtk_stream->dwnstrmring +
		rtk_stream->dwnstrmring_header->read_ptr[0] - rtk_stream->dwnstrmring_header->begin_addr;
	unsigned long space = 0;
	unsigned long frame_size = 0;
	unsigned int decfrm = 0;
	long cosume_size = 0;
	long now_rp;
	struct audio_inband_private_info inbandInfo;
	struct audio_dec_pts_info p_pts_info;
	struct audio_dec_eos p_eos_info;
	struct audio_inband_cmd_pkt_header pkt_header;
	struct audio_ms12idk_private_data ms12idk_data;

	while (ptswp != ptsrp) {
		memset(&pkt_header, 0, sizeof(struct audio_inband_cmd_pkt_header));
		get_buffer_from_ring(rtk_stream->dwnring_upper,
				     rtk_stream->dwnring_lower, ptsrp,
				     (unsigned char *)&pkt_header,
				     sizeof(struct audio_inband_cmd_pkt_header));

		if (pkt_header.type == AUDIO_DEC_INBAND_CMD_TYPE_PRIVATE_INFO) {
			/* skip private info */
			memset(&inbandInfo, 0, INBAND_INFO_SIZE);
			space = get_buffer_from_ring(rtk_stream->dwnring_upper,
						     rtk_stream->dwnring_lower, ptsrp,
						     (unsigned char*)&inbandInfo,
						     INBAND_INFO_SIZE);
			update_ring_ptr(rtk_stream->dwnring_upper,
					rtk_stream->dwnring_lower, ptsrp,
					rtk_stream, INBAND_INFO_SIZE, space);

			/* Get rp again */
			ptsrp = rtk_stream->dwnstrmring +
				rtk_stream->dwnstrmring_header->read_ptr[0] -
				rtk_stream->dwnstrmring_header->begin_addr;

			/* skip pcminfo or channel index info */
			if (inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_PCM_FMT
			|| inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_CH_IDX
			|| inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_IEC_MODE_INFO
			|| inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_NONPCM_CFG
			|| inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_DDP_METADATA_INFO
			|| inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_AAC_METADATA_INFO
			|| inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_AC4_METADATA_INFO
			|| inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_MAT_METADATA_INFO) {
				space = rtk_stream->dwnring_upper - ptsrp;

				update_ring_ptr(rtk_stream->dwnring_upper,
						rtk_stream->dwnring_lower, ptsrp,
						rtk_stream,
						inbandInfo.info_size, space);
			} else if (inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_MS12IDK_PRV_DATA) {
				memset(&ms12idk_data, 0, sizeof(struct audio_ms12idk_private_data));

				space = get_buffer_from_ring(rtk_stream->dwnring_upper,
							     rtk_stream->dwnring_lower, ptsrp,
							     (unsigned char*)&ms12idk_data,
							     sizeof(struct audio_ms12idk_private_data));

				update_ring_ptr(rtk_stream->dwnring_upper,
						rtk_stream->dwnring_lower, ptsrp,
						rtk_stream,
						sizeof(struct audio_ms12idk_private_data), space);

				pr_info("Atmos info: %d\n", ms12idk_data.private_data[0] & 0x1);
			} else {
				pr_err("%s %d should not be there\n", __func__, __LINE__);
				if (inbandInfo.info_type == AUDIO_INBAND_CMD_PRIVATE_BS_ERR)
					pr_err("AUDIO_INBAND_CMD_PRIVATE_BS_ERR\n");

				pr_err("infoType:%d size:%d, drop this cmd.", inbandInfo.info_type, inbandInfo.info_size);
				space = rtk_stream->dwnring_upper - ptsrp;

				update_ring_ptr(rtk_stream->dwnring_upper,
						rtk_stream->dwnring_lower, ptsrp,
						rtk_stream,
						inbandInfo.info_size, space);
				break;
			}
		} else if (pkt_header.type == AUDIO_DEC_INBAND_CMD_TYPE_PTS) {
			/* get ptsinfo */
			memset(&p_pts_info, 0, INBAND_PTS_INFO_SIZE);
			space = get_buffer_from_ring(rtk_stream->dwnring_upper,
						     rtk_stream->dwnring_lower, ptsrp,
						     (unsigned char*)&p_pts_info,
						     INBAND_PTS_INFO_SIZE);

			/* w_ptr is used to indicate the audio frame length */
			if (p_pts_info.w_ptr != 0) {
				frame_size += p_pts_info.w_ptr;
				update_ring_ptr(rtk_stream->dwnring_upper,
						rtk_stream->dwnring_lower, ptsrp,
						rtk_stream,
						INBAND_PTS_INFO_SIZE, space);
			} else {
				pr_err("%s %d should not be there\n", __func__, __LINE__);
				break;
			}
		} else if (pkt_header.type == AUDIO_DEC_INBAND_CMD_TYPE_EOS) {
			memset(&p_eos_info, 0, INBAND_EOS_SIZE);
			space = get_buffer_from_ring(rtk_stream->dwnring_upper,
						     rtk_stream->dwnring_lower, ptsrp,
						     (unsigned char*)&p_eos_info,
						     INBAND_EOS_SIZE);

			update_ring_ptr(rtk_stream->dwnring_upper,
					rtk_stream->dwnring_lower, ptsrp,
					rtk_stream,
					INBAND_EOS_SIZE, space);

			if (p_eos_info.header.size == INBAND_EOS_SIZE && p_eos_info.eosid == 0) {
				pr_info("get audio eos\n");
			} else if (p_eos_info.header.size == INBAND_EOS_SIZE && p_eos_info.eosid == -1) {
				pr_info("should not be there %d\n", __LINE__);
				break;
			}
		} else {
			pr_err("%s %d should not be there\n", __func__, __LINE__);
			pr_err("infoType:%d size:%d, drop this cmd.", pkt_header.type, pkt_header.size);

			space = rtk_stream->dwnring_upper - ptsrp;

			update_ring_ptr(rtk_stream->dwnring_upper,
					rtk_stream->dwnring_lower, ptsrp,
					rtk_stream,
					pkt_header.size, space);

			break;
		}

		ptswp = rtk_stream->dwnstrmring +
			rtk_stream->dwnstrmring_header->write_ptr - rtk_stream->dwnstrmring_header->begin_addr;
		ptsrp = rtk_stream->dwnstrmring +
			rtk_stream->dwnstrmring_header->read_ptr[0] - rtk_stream->dwnstrmring_header->begin_addr;
	}

	/* calculate to total read into decoder */
	now_rp = (long)rtk_stream->dec_inring_header->read_ptr[0];
	if (now_rp != rtk_stream->last_inring_rp) {
		long rp = rtk_stream->last_inring_rp;
		long lower = (long)rtk_stream->dec_inring_header->begin_addr;
		long upper = lower + (long)rtk_stream->dec_inring_header->size;
		cosume_size = ring_valid_data(lower, upper, rp, now_rp);
		rtk_stream->last_inring_rp = now_rp;
	}

	rtk_stream->consume_total += cosume_size;
	tstamp->copied_total = rtk_stream->consume_total;
	tstamp->byte_offset = tstamp->copied_total % (u32)cstream->runtime->buffer_size;

	if (frame_size)
		rtk_stream->out_frames += (frame_size >> 2);

	/* for IOCTL TSTAMP to get render position in user space */
	decfrm = rtk_stream->rawdelay_mem2->decfrm_smpl;
	if (decfrm == 0)
		tstamp->pcm_io_frames = rtk_stream->out_frames;
	else
		tstamp->pcm_io_frames = rtk_stream->rawdelay_mem2->decfrm_smpl;

	tstamp->sampling_rate = rtk_stream->audio_sampling_rate;

	return 0;
}

static int snd_card_compr_copy(struct snd_compr_stream *cstream,
			       char __user *buf, size_t count)
{
	struct rtk_compr_stream *rtk_stream = cstream->runtime->private_data;
	unsigned int wp = rtk_stream->dec_inring_header->write_ptr;
	unsigned int rp = rtk_stream->dec_inring_header->read_ptr[0];
	unsigned int lower = rtk_stream->dec_inring_header->begin_addr;
	unsigned int upper = lower + rtk_stream->dec_inring_header->size;
	long writable_size = 0;
	int buffull_count = 0;
	int write_frame = 0;
	int hdr_size = 0;

	writable_size = valid_free_size(lower, upper, rp, wp);
	while (count > writable_size) {
		if (buffull_count++ > 5) {
			pr_err("writable_size %ld count %d buffull_count %d return\n",
			       writable_size, count, buffull_count);
			return 0;
		}
		usleep_range(500, 1000);
		rp = rtk_stream->dec_inring_header->read_ptr[0];
		writable_size = valid_free_size(lower, upper, rp, wp);
	}

	write_frame = direct_writedata(rtk_stream, (char *)buf, count, &hdr_size);
	if (write_frame < 0) {
		pr_err("%s copy fail ERR: %d\n", __func__, write_frame);
		return write_frame;
	}

	/*
	 * With hw av sync header       : count = write_frame + header
	 * Without hw av sync header    : count = write_frame
	 */
	rtk_stream->consume_total += hdr_size;

	return write_frame + hdr_size;
}

static int snd_card_compr_get_caps(struct snd_compr_stream *cstream,
				   struct snd_compr_caps *caps)
{
	caps->num_codecs = NUM_CODEC;
	caps->direction = SND_COMPRESS_PLAYBACK;
	caps->min_fragment_size = MIN_FRAGMENT_SIZE;
	caps->max_fragment_size = MAX_FRAGMENT_SIZE;
	caps->min_fragments = MIN_FRAGMENT;
	caps->max_fragments = MAX_FRAGMENT;
	caps->codecs[0] = SND_AUDIOCODEC_MP3;
	caps->codecs[1] = SND_AUDIOCODEC_AAC;
	caps->codecs[2] = SND_AUDIOCODEC_AC3;
	caps->codecs[3] = SND_AUDIOCODEC_DTS;
	caps->codecs[4] = SND_AUDIOCODEC_DTS_HD;
	caps->codecs[5] = SND_AUDIOCODEC_EAC3;
	caps->codecs[6] = SND_AUDIOCODEC_TRUEHD;
	caps->codecs[7] = SND_AUDIOCODEC_APE;
	return 0;
}

static struct snd_compr_codec_caps caps_mp3 = {
	.num_descriptors = 1,
	.descriptor[0].max_ch = 2,
	.descriptor[0].sample_rates[0] = 48000,
	.descriptor[0].sample_rates[1] = 44100,
	.descriptor[0].sample_rates[2] = 32000,
	.descriptor[0].sample_rates[3] = 16000,
	.descriptor[0].sample_rates[4] = 8000,
	.descriptor[0].num_sample_rates = 5,
	.descriptor[0].bit_rate[0] = 320,
	.descriptor[0].bit_rate[1] = 192,
	.descriptor[0].num_bitrates = 2,
	.descriptor[0].profiles = 0,
	.descriptor[0].modes = SND_AUDIOCHANMODE_MP3_STEREO,
	.descriptor[0].formats = 0,
};

static struct snd_compr_codec_caps caps_non_mp3 = {
	.num_descriptors = 1,
	.descriptor[0].max_ch = 8,
	.descriptor[0].sample_rates[0] = 48000,
	.descriptor[0].sample_rates[1] = 44100,
	.descriptor[0].sample_rates[2] = 32000,
	.descriptor[0].sample_rates[3] = 16000,
	.descriptor[0].sample_rates[4] = 8000,
	.descriptor[0].num_sample_rates = 5,
	.descriptor[0].bit_rate[0] = 320,
	.descriptor[0].bit_rate[1] = 192,
	.descriptor[0].num_bitrates = 2,
	.descriptor[0].profiles = 0,
	.descriptor[0].modes = 0,
	.descriptor[0].formats = 0,
};

static int snd_card_compr_get_codec_caps(struct snd_compr_stream *cstream,
					 struct snd_compr_codec_caps *codec)
{
	switch (codec->codec) {
	case SND_AUDIOCODEC_MP3:
		*codec = caps_mp3;
		break;
	case SND_AUDIOCODEC_AAC:
	case SND_AUDIOCODEC_AC3:
	case SND_AUDIOCODEC_EAC3:
	case SND_AUDIOCODEC_DTS:
	case SND_AUDIOCODEC_DTS_HD:
	case SND_AUDIOCODEC_TRUEHD:
	case SND_AUDIOCODEC_APE:
		*codec = caps_non_mp3;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static struct snd_compr_ops rtk_snd_compr_ops = {
	.open           = snd_card_compr_open,
	.free           = snd_card_compr_free,
	.set_params     = snd_card_compr_set_params,
	.get_params     = snd_card_compr_get_params,
	.set_metadata   = snd_card_compr_set_metadata,
	.get_metadata   = snd_card_compr_get_metadata,
	.trigger        = snd_card_compr_trigger,
	.pointer        = snd_card_compr_pointer,
	.copy           = snd_card_compr_copy,
	.get_caps       = snd_card_compr_get_caps,
	.get_codec_caps = snd_card_compr_get_codec_caps
};

static int snd_compress_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	if (kcontrol->private_value == ENUM_MIXING_INDEX) {
		uinfo->count = MAX_CAR_CH + 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = MAX_CAR_CH;
	} else if (kcontrol->private_value == ENUM_DEC_VOLUME) {
		uinfo->count = 1;
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 100;
	}

	return 0;
}

static int snd_compress_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct rtk_snd_mixer *mixer = snd_kcontrol_chip(kcontrol);
	int ch, band_num = kcontrol->private_value;
	unsigned long flags;

	spin_lock_irqsave(&mixer->mixer_lock, flags);

	if (band_num == ENUM_MIXING_INDEX) {
		ucontrol->value.integer.value[0] = mixer->mixing_idx.enable;

		for (ch=0; ch<MAX_CAR_CH; ch++) {
			ucontrol->value.integer.value[ch+1] =
				mixer->mixing_idx.mixing_channel[ch];
		}
	} else if (band_num == ENUM_DEC_VOLUME) {
		ucontrol->value.integer.value[0] = mixer->dec_volume;
	}

	spin_unlock_irqrestore(&mixer->mixer_lock, flags);

	return 0;
}

static int snd_compress_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct rtk_snd_mixer *mixer = snd_kcontrol_chip(kcontrol);
	int ch, value, change = 0;
	int band_num = kcontrol->private_value;
	unsigned long flags;

	spin_lock_irqsave(&mixer->mixer_lock, flags);

	if (band_num == ENUM_MIXING_INDEX) {
		mixer->mixing_cnt++;

		value = ucontrol->value.integer.value[0];
		change |= value != mixer->mixing_idx.enable;
		mixer->mixing_idx.enable = value;

		for (ch=0; ch<MAX_CAR_CH; ch++) {
			value = ucontrol->value.integer.value[ch+1];

			change |= value != mixer->mixing_idx.mixing_channel[ch];
			mixer->mixing_idx.mixing_channel[ch] = value;
		}
	} else if (band_num == ENUM_DEC_VOLUME) {
		value = ucontrol->value.integer.value[0];
		if (value < 0)
			value = 0;
		if (value > 100)
			value = 100;

		change |= value != mixer->dec_volume;
		mixer->dec_volume = value;
	}

	if (band_num == ENUM_MIXING_INDEX && mixer->mixing_cnt == (MAX_CAR_CH + 1)) {
		schedule_work(&mixer->work_mixing_idx);
		mixer->mixing_cnt = 0;
	} else if (band_num == ENUM_DEC_VOLUME) {
		schedule_work(&mixer->work_dec_volume);
	}

	spin_unlock_irqrestore(&mixer->mixer_lock, flags);

	return change;
}

int rtk_create_compress_instance(struct snd_card *card,
				 int instance_idx,
				 char *id,
				 int direction)
{
	struct snd_compr *compr = NULL;
	int ret = 0;

	compr = kzalloc(sizeof(*compr), GFP_KERNEL);
	if (!compr) {
		pr_err("cannot allocate compr for %d\n", instance_idx);
		ret = -ENOMEM;
		goto fail;
	}

	compr->ops = kzalloc(sizeof(rtk_snd_compr_ops), GFP_KERNEL);
	if (compr->ops == NULL) {
		pr_err("cannot allocate compressed ops for %d\n", instance_idx);
		ret = -ENOMEM;
		goto fail_compr;
	}
	memcpy(compr->ops, &rtk_snd_compr_ops, sizeof(rtk_snd_compr_ops));

	mutex_init(&compr->lock);
	ret = snd_compress_new(card, instance_idx, direction, id, compr);
	if (ret < 0) {
		pr_err("rtk_create_compress_instance %d failed\n", instance_idx);
		goto fail_compr_ops;
	}

	compr->private_data = card;

	return 0;
fail_compr_ops:
	kfree(compr->ops);
fail_compr:
	kfree(compr);
fail:
	return ret;
}

static void snd_rtk_dec_volume_work(struct work_struct *work)
{
	struct rtk_snd_mixer *mixer =
			container_of(work, struct rtk_snd_mixer, work_dec_volume);
	if (mixer->multi_ao)
		rpc_set_dec_volume_new(mixer->dev, mixer->dec_volume, mixer->ao_agent_id);
	else
		rpc_set_dec_volume(mixer->dev, mixer->dec_volume);
}

static void snd_rtk_mixing_idx_work(struct work_struct *work)
{
	struct rtk_snd_mixer *mixer =
			container_of(work, struct rtk_snd_mixer, work_mixing_idx);

	rpc_set_mixing_idx(mixer->dev, mixer);
}

static int rtk_compr_mixer_new(struct snd_card *card,
			       struct rtk_snd_mixer *mixer)
{
	unsigned int idx;
	int err;

	spin_lock_init(&mixer->mixer_lock);
	INIT_WORK(&mixer->work_mixing_idx, snd_rtk_mixing_idx_work);
	INIT_WORK(&mixer->work_dec_volume, snd_rtk_dec_volume_work);

	strncpy(card->mixername, "Rtk_Compress_Mixer", sizeof(card->mixername));

	/* Initialize dec volume */
	mixer->dec_volume = 100;

	for (idx = 0; idx < ARRAY_SIZE(snd_feature_ctrl); idx++) {
		err = snd_ctl_add(card, snd_ctl_new1(&snd_feature_ctrl[idx], mixer));
		if (err < 0) {
			pr_err("[snd_ctl_add faile %s]\n", __func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int rtk_compress_probe(struct platform_device *pdev)
{
	struct rtk_compr_dev *compr_dev = NULL;
	struct snd_card *card = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	const struct snd_card_data *data;
	struct rtk_snd_mixer *mixer;
	int err;
	phys_addr_t dat;
	void *vaddr;
	int pin_id;

	compr_dev = devm_kzalloc(dev, sizeof(*compr_dev), GFP_KERNEL);
	if (!compr_dev)
		return -ENOMEM;
	compr_dev->dev = dev;

	if (WARN_ON(!np))
		dev_err(dev, "can not found device node\n");

	if (IS_ENABLED(CONFIG_RPMSG_RTK_RPC)) {
		hifi_compr_ept_info = of_krpc_ept_info_get(np, 1);
		if (IS_ERR(hifi_compr_ept_info))
			return dev_err_probe(dev, PTR_ERR(hifi_compr_ept_info),
					     "failed to get HIFI krpc ept info: 0x%lx\n",
					     PTR_ERR(hifi_compr_ept_info));

		snd_ept_init(hifi_compr_ept_info);
	}

	data = of_device_get_match_data(dev);

	err = snd_card_new(dev, -1, "rtk_snd_compr",
			   THIS_MODULE, sizeof(struct rtk_snd_mixer),
			   &card);
	if (err < 0) {
		pr_err("snd_card_new fail\n");
		return -EINVAL;
	}

	/* set up dma buffer operation */
	set_dma_ops(card->dev, &rheap_dma_ops);

	compr_dev->card = card;
	mixer = (struct rtk_snd_mixer *)card->private_data;
	mixer->dev = dev;

	err = rtk_create_compress_instance(card, 0, "compr_ao1", SND_COMPRESS_PLAYBACK);
	err |= rtk_create_compress_instance(card, 1, "compr_ao2", SND_COMPRESS_PLAYBACK);
	err |= rtk_create_compress_instance(card, 2, "compr_ai", SND_COMPRESS_CAPTURE);
	if (err < 0) {
		pr_err("[%s create compress instance fail]\n", __func__);
		goto __nodev;
	}

	err = rtk_compr_mixer_new(card, mixer);
	if (err < 0) {
		pr_err("[%s add mixer fail]\n", __func__);
		goto __nodev;
	}

	of_property_read_u32(np, "multi-ao", &mixer->multi_ao);

	strncpy(card->driver, "rtk_snd_compr", sizeof(card->driver));
	strncpy(card->shortname, "rtk_snd_compr", sizeof(card->shortname));
	strncpy(card->longname, "rtk_snd_compr", sizeof(card->longname));

	err = snd_card_register(card);
	if (err) {
		pr_err("[%s card register fail]\n", __func__);
		goto __nodev;
	}

	if (data->card_id == 1)
		pin_id = AUDIO_OUT;
	else if (data->card_id == 2)
		pin_id = AUDIO_OUT2;

	rheap_setup_dma_pools(dev, "rtk_media_heap", RTK_FLAG_NONCACHED |
			      RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC, __func__);
	vaddr = dma_alloc_coherent(dev, SZ_4K, &dat, GFP_KERNEL);
	if (!vaddr) {
		dev_err(dev, "%s dma_alloc fail\n", __func__);
		err = -ENOMEM;
		goto __nodev;
	}
	if (rpc_create_ao_agent(dat, vaddr, &compr_dev->ao_agent_id, pin_id)) {
		pr_err("[No AO agent %s %d]\n", __func__, __LINE__);
		err = -1;
		goto __nodev;
	}
	dma_free_coherent(dev, SZ_4K, vaddr, dat);
	mixer->ao_agent_id = compr_dev->ao_agent_id;

	platform_set_drvdata(pdev, compr_dev);
	dev_info(dev, "initialized\n");
	return 0;
__nodev:
	snd_card_free(card);
	return err;
}

static int rtk_compress_remove(struct platform_device *pdev)
{
	struct rtk_compr_dev *compr_dev = platform_get_drvdata(pdev);

	if (compr_dev->card)
		snd_card_free(compr_dev->card);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int rtk_compr_suspend(struct device *pdev)
{
	struct rtk_compr_dev *compr_dev = dev_get_drvdata(pdev);

	snd_power_change_state(compr_dev->card, SNDRV_CTL_POWER_D3hot);

	return 0;
}

static int rtk_compr_resume(struct device *pdev)
{
	struct rtk_compr_dev *compr_dev = dev_get_drvdata(pdev);

	snd_power_change_state(compr_dev->card, SNDRV_CTL_POWER_D0);

	return 0;
}

static const struct dev_pm_ops rtk_compr_pm = {
	.resume = rtk_compr_resume,
	.suspend = rtk_compr_suspend,
};

static const struct of_device_id rtk_compress_dt_match[] = {
	{ .compatible = "realtek,rtk-alsa-compress", &rtk_sound_card0 },
	{ .compatible = "realtek,rtk-alsa-compress-ao1", &rtk_sound_card1 },
	{}
};

static struct platform_driver rtk_compress_driver = {
	.probe =	rtk_compress_probe,
	.remove =	rtk_compress_remove,
	.driver = {
		.name =		"rtk_alsa_compress",
		.pm =		&rtk_compr_pm,
		.of_match_table = rtk_compress_dt_match,
	},
};

static int __init rtk_alsa_compress_init(void)
{
	int err;

	err = platform_driver_register(&rtk_compress_driver);
	if (err < 0)
		goto fail;

	return 0;
fail:
	pr_err("%s error\n", __func__);

	return err;
}

static void __exit rtk_alsa_compress_exit(void)
{
	platform_driver_unregister(&rtk_compress_driver);
}
module_init(rtk_alsa_compress_init);
module_exit(rtk_alsa_compress_exit);

MODULE_LICENSE("GPL v2");
