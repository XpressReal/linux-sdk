/* SPDX-License-Identifier: GPL-2.0 */
/*
 * rtk-afe-common.h  --  Realtek audio driver definitions
 *
 * Copyright (c) 2024 RealTek Inc.
 * Author: Simon Hsu <simon_hsu@realtek.com>
 */

#ifndef RTK_AFE_COMMON_H_
#define RTK_AFE_COMMON_H_

#define AFE_PCM_NAME "rtk-afe-pcm"

#include <sound/soc.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include "rtk-hifi-rpc.h"

#define RTK_PCM_INFO	SNDRV_PCM_INFO_INTERLEAVED | \
			SNDRV_PCM_INFO_NONINTERLEAVED | \
			SNDRV_PCM_INFO_RESUME | \
			SNDRV_PCM_INFO_MMAP | \
			SNDRV_PCM_INFO_MMAP_VALID | \
			SNDRV_PCM_INFO_PAUSE

#define RTK_PCM_RATES	SNDRV_PCM_RATE_8000_48000 |\
			SNDRV_PCM_RATE_88200 |\
			SNDRV_PCM_RATE_96000 |\
			SNDRV_PCM_RATE_176400 |\
			SNDRV_PCM_RATE_192000

#define RTK_PCM_FORMATS SNDRV_PCM_FMTBIT_S8 | \
			SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S24_3LE

#define RTK_PCM_RATE_MIN		8000
#define RTK_PCM_RATE_MAX		192000
#define RTK_PCM_CHANNELS_MIN		1
#define RTK_PCM_CHANNELS_MAX		8
#define RTK_PCM_MAX_BUFFER_SIZE		(4096 * 1024)
#define RTK_PCM_MIN_PERIOD_SIZE		1024
#define RTK_PCM_MAX_PERIOD_SIZE		(2048 * 1024)
#define RTK_PCM_PERIODS_MIN		2
#define RTK_PCM_PERIODS_MAX		4096
#define RTK_PCM_FIFO_SIZE		32

#define RTK_HIFI_FLAGS	RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_HIFIACC
#define RTK_ACPU_FLAGS	RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC
#define RTK_SECURE_FLAGS \
	RTK_FLAG_PROTECTED_V2_AO_POOL | RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC

#define RTK_ENC_AI_BUFFER_SIZE		(32 * 1024)
#define RTK_PACK_AI_BUFFER_SIZE		(64 * 1024)
#define RTK_ENC_FAI_BUFFER_SIZE		(4096 * 1024)
#define RTK_NON_PCM_BUFFER_SIZE		(36 * 1024)
#define RTK_DEC_BS_SIZE			(256 * 1024)
#define RTK_DEC_ICQ_SIZE		(4096)
#define RTK_DEC_DWNSTRM_SIZE		(2048)
#define RTK_DEC_OUT_SAMPLE_SIZE		(16 * 1024)
#define RTK_DEC_OUT_BYTE_SIZE		(RTK_DEC_OUT_SAMPLE_SIZE << 2)

#define RTK_AUDIO_OUT_I2S_2_CHANNEL	0
#define RTK_AUDIO_OUT_I2S_8_CHANNEL	1

#define RTK_AUDIO_OUT_I2S_MODE_MASTER	0
#define RTK_AUDIO_OUT_I2S_MODE_SLAVE	1
#define RTK_PCM_CAPTURE_FORMATS SNDRV_PCM_FMTBIT_S16_LE | \
                                SNDRV_PCM_FMTBIT_S24_LE | \
                                SNDRV_PCM_FMTBIT_S24_3LE | \
                                SNDRV_PCM_FMTBIT_S32_LE

#define RTK_PCM_CAPTURE_RATES SNDRV_PCM_RATE_8000 | \
                                SNDRV_PCM_RATE_16000 | \
                                SNDRV_PCM_RATE_22050 | \
                                SNDRV_PCM_RATE_32000 | \
                                SNDRV_PCM_RATE_44100 | \
                                SNDRV_PCM_RATE_48000 | \
                                SNDRV_PCM_RATE_88200 | \
                                SNDRV_PCM_RATE_96000 | \
                                SNDRV_PCM_RATE_176400 | \
                                SNDRV_PCM_RATE_192000 | \
                                SNDRV_PCM_RATE_384000

struct rtk_dai_kctrl {
	int kctrl_num;
	int count;
	int min;
	int max;
};

#define RTK_SOC_DAI_CONTROL(xname, xnum, xcount, xmin, xmax, \
				xhandler_info, xhandler_get, xhandler_put, \
				id) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = xhandler_info, \
	.get = xhandler_get, \
	.put = xhandler_put, \
	.device = id, \
	.private_value = (unsigned long)&(struct rtk_dai_kctrl) \
		{.kctrl_num = xnum, .count = xcount, .min = xmin, .max = xmax} }

#define RTK_SOC_SINGLE_BOOL_EXT(xname, xdata, xhandler_get, xhandler_put, id) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_bool_ext, \
	.get = xhandler_get, .put = xhandler_put, \
	.device = id, \
	.private_value = xdata }

struct rtk_pcm_mmap_fd {
	int dir;
	int fd;
	int size;
	int actual_size;
};

struct audio_mixing_index {
	int enable;
	int mixing_channel[32];
	int reserved[15];
};

#define SNDRV_PCM_IOCTL_MMAP_DATA_FD _IOWR('A', 0xE4, struct rtk_pcm_mmap_fd)
#define SNDRV_PCM_IOCTL_GET_LATENCY  _IOR('A', 0xF0, int)

enum {
	RTK_DAI_START,
	RTK_DAI_MEMIF_START = RTK_DAI_START,
	RTK_DAI_MEMIF_DL0 = RTK_DAI_MEMIF_START,
	RTK_DAI_MEMIF_DL1,
	RTK_DAI_MEMIF_DL2,
	RTK_DAI_MEMIF_DL3,
	RTK_DAI_MEMIF_DL4,
	RTK_DAI_MEMIF_DL5,
	RTK_DAI_MEMIF_DL6,
	RTK_DAI_MEMIF_DL7,
	RTK_DAI_MEMIF_UL0,
	RTK_DAI_MEMIF_UL1,
	RTK_DAI_MEMIF_UL2,
	RTK_DAI_MEMIF_END,
	RTK_DAI_MEMIF_NUM = (RTK_DAI_MEMIF_END - RTK_DAI_MEMIF_START),
	RTK_DAI_AIO_START = RTK_DAI_MEMIF_END,
	RTK_DAI_AO_START = RTK_DAI_AIO_START,
	RTK_DAI_AUDIO_OUT0 = RTK_DAI_AO_START,
	RTK_DAI_AUDIO_OUT1,
	RTK_DAI_AO_END,
	RTK_DAI_AI_START = RTK_DAI_AO_END,
	RTK_DAI_I2S_START = RTK_DAI_AI_START,
	RTK_DAI_I2S_0 = RTK_DAI_I2S_START,
	RTK_DAI_I2S_1,
	RTK_DAI_STI2S_0,
	RTK_DAI_STI2S_1,
	RTK_DAI_I2S_END,
	RTK_DAI_ADC_START = RTK_DAI_I2S_END,
	RTK_DAI_ADC_0 = RTK_DAI_ADC_START,
	RTK_DAI_ADC_1,
	RTK_DAI_ADC_END,
	RTK_DAI_I2S0_LOOPBACK = RTK_DAI_ADC_END,
	RTK_DAI_PDM,
	RTK_DAI_DPRX,
	RTK_DAI_TDM_START,
	RTK_DAI_TDM_0 = RTK_DAI_TDM_START,
	RTK_DAI_TDM_1,
	RTK_DAI_TDM_2,
	RTK_DAI_TDM_END,
	RTK_DAI_AI_END = RTK_DAI_TDM_END,
	RTK_DAI_AIO_END = RTK_DAI_AI_END,
	RTK_DAI_END = RTK_DAI_AIO_END,
	RTK_DAI_NUM = (RTK_DAI_END - RTK_DAI_START),
};

struct audio_ringbuf_ptr {
	unsigned long base;
	unsigned long limit;
	unsigned long cp;
	unsigned long rp;
	unsigned long wp;
};

struct rtk_snd_ringbuf_hdr {
	dma_addr_t paddr;
	void *vaddr;
	int hdr_size;
	struct ringbuf_header_ptrs *ptrs;
	struct rpc_ringbuffer_header rpc_hdr;
};

struct rtk_snd_ringbuf {
	struct rtk_afe_priv *afe;
	struct snd_dma_buffer *dmab;
	struct kref ref;
	int bufnum;

	/* for ringbuf header */
	int num_hdrs;
	struct rtk_snd_ringbuf_hdr *hdrs;
};

struct rtk_dai_aio_priv {
	unsigned int ao_id;
	struct rpc_aio_ctrl ctrl;

	unsigned int ai_id;
	struct rtk_snd_ringbuf *airing;
	unsigned int lb_secure;
};

struct rtk_afe_memif {
	struct rtk_snd_ringbuf *ringbuf;
	struct rtk_rpc_priv *rpc_priv;

	struct hrtimer timer;
	enum hrtimer_restart stat;
	ktime_t ktime;
	struct snd_pcm_substream *substream;
	struct mutex lock;

	unsigned int ao_id;
	unsigned int pin;
	unsigned int volume;
	unsigned int mixidx_en;
	unsigned int mixidx[MAX_CAR_CH];

	struct delayed_work mixidx_work;

	snd_pcm_uframes_t hw_ptr;
	snd_pcm_uframes_t prehw_ptr;
	snd_pcm_uframes_t total_read;
	snd_pcm_uframes_t total_write;
};

struct rtk_pcm_runtime_priv {
	struct rtk_afe_memif *memif;
	int ao_id;
	int ai_id;
	int no_lpcm;
};

struct rtk_afe_data {
	unsigned int ao_num;
	unsigned int ao_bitmap_mask;
	unsigned int i2s_out_ch;
	unsigned int lb_secure;
};

struct rtk_afe_priv {
	struct device *dev;
	struct rtk_rpc_priv rpc_priv;
	struct rtk_afe_memif *memif;

	struct list_head sub_dais;
	struct snd_soc_dai_driver *dai_drivers;
	unsigned int num_dai_drivers;
	void *dai_priv[RTK_DAI_NUM];

	const struct snd_pcm_hardware *rtk_afe_hardware;
	const struct snd_pcm_hardware *rtk_afe_capture_hardware;
	const struct rtk_afe_data *data;

	bool fw_mem_cfg;
};

struct rtk_afe_dai {
	struct snd_soc_dai_driver *dai_drivers;
	unsigned int num_dai_drivers;

	const struct snd_kcontrol_new *controls;
	unsigned int num_controls;
	const struct snd_soc_dapm_widget *dapm_widgets;
	unsigned int num_dapm_widgets;
	const struct snd_soc_dapm_route *dapm_routes;
	unsigned int num_dapm_routes;

	struct list_head list;
};

unsigned long ring_valid_data(unsigned long ring_base,
			      unsigned long ring_limit,
			      unsigned long ring_rp,
			      unsigned long ring_wp);
unsigned long ring_add(unsigned long ring_base,
		       unsigned long ring_limit,
		       unsigned long ptr,
		       unsigned int bytes);
unsigned long valid_free_size(unsigned long base,
			      unsigned long limit,
			      unsigned long rp,
			      unsigned long wp);
void rtk_snd_ringbuf_destroy(struct kref *kref);
int rtk_snd_reinit_ringheader(struct rtk_snd_ringbuf *ringbuf);
void rtk_snd_free_ringbuf(struct rtk_snd_ringbuf **ringbuf);
int rtk_snd_prepare_ringbuf(struct rtk_afe_priv *afe,
			    struct snd_dma_buffer *dma_buffer,
			    size_t size, size_t bufnum,
			    struct rtk_snd_ringbuf **ringbuf,
			    struct rpc_ringbuffer_header *hdr,
			    int num_hdrs,
			    unsigned long heap_flags);

int rtk_dai_i2s_register(struct rtk_afe_priv *afe);
int rtk_dai_pdm_register(struct rtk_afe_priv *afe);
int rtk_dai_tdm_register(struct rtk_afe_priv *afe);
int rtk_dai_adc_register(struct rtk_afe_priv *afe);
int rtk_dai_aio_register(struct rtk_afe_priv *afe);
int rtk_dai_dprx_register(struct rtk_afe_priv *afe);
int rtk_dai_memif_register(struct rtk_afe_priv *afe);

#endif
