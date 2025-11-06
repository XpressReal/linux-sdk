/**
 * snd-rtk-compress.h - Realtek alsa compress driver
 *
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef SND_RTK_COMPRESS
#define SND_RTK_COMPRESS

#include <sound/compress_params.h>
#include <sound/compress_offload.h>
#include <sound/compress_driver.h>
#include <soc/realtek/rtk-krpc-agent.h>

extern struct rtk_krpc_ept_info *hifi_compr_ept_info;

#define AUTOMASTER_NOT_MASTER   0
#define AUDIO_DEC_OUTPIN        8
#define NUM_CODEC		7
#define MIN_FRAGMENT		2
#define MAX_FRAGMENT		16
#define MIN_FRAGMENT_SIZE	(1024)
#define MAX_FRAGMENT_SIZE	(8192 * 64)
#define DEC_INBAND_BUFFER_SIZE  (16 * 1024)
#define DEC_DWNSTRM_BUFFER_SIZE (32 * 1024)
#define DEC_OUT_BUFFER_SIZE     (36 * 1024)
#define DEC_OUT_BUFFER_SIZE_LOW (8 * 1024)

/* Rtk define */
#define SND_AUDIOCODEC_EAC3                  ((__u32) 0x00000011)
#define SND_AUDIOCODEC_DTS                   ((__u32) 0x00000012)
#define SND_AUDIOCODEC_DTS_HD                ((__u32) 0x00000013)
#define SND_AUDIOCODEC_TRUEHD                ((__u32) 0x00000014)
#define SND_AUDIOCODEC_AC3                   ((__u32) 0x00000015)


enum {
	OBJT_TYPE_NULL = 0,
	OBJT_TYPE_MAIN = 1,
	OBJT_TYPE_LC   = 2,
	OBJT_TYPE_SSR  = 3,
	OBJT_TYPE_SBR  = 5
};

enum COMPRESS_CONTROL_TYPE {
	ENUM_MIXING_INDEX = 0,
	ENUM_DEC_VOLUME = 1,
};

#define RTK_COMPRESS_CONTROL(control_name, ctl_index, control_num) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = control_name, \
	.index = ctl_index, \
	.info = snd_compress_info, \
	.get = snd_compress_get, \
	.put = snd_compress_put, \
	.private_value = control_num \
}

#ifdef CONFIG_RTK_CACHEABLE_HEADER
struct MASTERSHIP {
    unsigned char systemMode;  /* enum AVSYNC_MODE */
    unsigned char videoMode;   /* enum AVSYNC_MODE */
    unsigned char audioMode;   /* enum AVSYNC_MODE */
    unsigned char masterState; /* enum AUTOMASTER_STATE */
};

struct tag_refclock {
	union
	{
		struct
		{
			volatile int64_t   GPTSTimeout;
			volatile int64_t   videoSystemPTS;
			volatile int64_t   videoRPTS;
			volatile uint32_t  videoContext;
			volatile uint32_t  videoFreeRunThreshold;
			volatile uint32_t  audioFreeRunThreshold;
			volatile int32_t   audioPauseFlag;
			volatile int32_t   VO_Underflow;
			volatile int32_t   videoEndOfSegment;
			volatile int64_t   vsyncPTS;
			volatile uint8_t   reserved[8];
		};
		char read_only[128];
	};
	union
	{
		struct
		{
			volatile int64_t   RCD;
			volatile uint32_t  RCD_ext;
			volatile int64_t   masterGPTS;
		};
		char sync_with_VO[128];
	};
	union
	{
		struct
		{
			volatile int64_t   audioSystemPTS;
			volatile int64_t   audioRPTS;
			volatile uint32_t  audioContext;
			volatile int32_t   audioFullness;
			volatile int32_t   AO_Underflow;
			volatile int32_t   audioEndOfSegment;
		};
		char sync_with_ARM[128];
	};
	union
	{
		struct MASTERSHIP mastership;
		char mastership_sync_with_ARM[128];
	};
};
#else
struct tag_master_ship {
	unsigned char systemMode;	/* enum AVSYNC_MODE */
	unsigned char videoMode;	/* enum AVSYNC_MODE */
	unsigned char audioMode;	/* enum AVSYNC_MODE */
	unsigned char masterState;	/* enum AUTOMASTER_STATE */
};

struct tag_refclock {
	long long RCD;
	unsigned int RCD_ext;
	long long GPTSTimeout;
	long long videoSystemPTS;
	long long audioSystemPTS;
	long long videoRPTS;
	long long audioRPTS;
	unsigned int videoContext;
	unsigned int audioContext;

	struct tag_master_ship mastership;
	unsigned int videoFreeRunThreshold;
	unsigned int audioFreeRunThreshold;
	long long masterGPTS;	// this is the value of GPTS (hardware PTS) when master set the reference clock
	int audioFullness;	// This flag will be turned on when AE's output buffer is almost full.
				// VE which is monitoring this flag will issue auto-pause then.
				// (0: still have enough output space for encoding.   1: AE request pause)
	int audioPauseFlag;	// This flag will be turned on when VE decides to auto-pause.
				// AE which is monitoring this flag will auto-pause itself then.
				// (0: ignore.  1: AE have to pauseEncode immediately)
	int VO_Underflow;	// (0: VO is working properly; otherwise, VO underflow)
	int AO_Underflow;	// (0: AO is working properly; otherwise, AO underflow)
	int videoEndOfSegment;	// set to the flow EOS.eventID by VO after presenting the EOS sample
	int audioEndOfSegment;	// set to the flow EOS.eventID by AO after presenting the EOS sample
	unsigned char  reserved[16];
};
#endif

struct audio_pcm_format {
	int chnum;
	int samplebit;
	int samplerate;
	unsigned int dynamicRange;
	unsigned char emphasis;
	unsigned char mute;
};

struct audio_info_pcm_format {
	enum audio_info_type info_type;
	struct audio_pcm_format pcm_format;
	unsigned int start_addr;
	int max_bit_rate;
};

struct audio_info_channel_index_old {
	enum audio_info_type info_type;
	char channel_index[8];
	unsigned int start_addr;
	char dual_mono_flag;
};

struct audio_info_channel_index_new {
	enum audio_info_type info_type;
	unsigned int channel_index[8];
	unsigned int start_addr;
	unsigned int dual_mono_flag;
};

/* following this packet, has private_size number of bytes is the privateinfo to deliver out */
struct audio_inband_private_info {
	struct audio_inband_cmd_pkt_header header;
	enum enum_audio_inband_private_info info_type;
	unsigned int info_size;
};

struct alsa_raw_latency {
	unsigned int latency;
	unsigned int ptsL;
	unsigned int ptsH;
	unsigned int sum; // latency + PTSL
	unsigned int decin_wp;
	unsigned int sync;
	unsigned int decfrm_smpl;
	int rvd[8];
};

struct audio_out_cs_info {
	char non_pcm_valid;
	char non_pcm_format;
	int audio_format;
	char spdif_consumer_use;
	char copy_right;
	char pre_emphasis;
	char stereo_channel;
};

struct audio_rpc_focus {
	int instance_id;
	int focus_id;
};

struct audio_rpc_focus_t {
	int instance_id;
	int focus_id;
	int reserved[30];
	struct rpcres_long ret;
	u32 res;
};

struct audio_rpc_refclock {
	int instance_id;
	int pref_clock_id;
	int pref_clock;
};

struct audio_rpc_refclock_t {
    int instance_id;
    int pref_clock_id;
    int pref_clock;
    int reserved[29];
    struct rpcres_long ret;
    u32 res;
};

struct audio_dec_eos {
	struct audio_inband_cmd_pkt_header header;
	unsigned int w_ptr;
	int eosid;
};

struct audio_ms12idk_private_data {
    enum audio_info_type info_type;
    int private_data[8];
};

#define INBAND_PTS_INFO_SIZE sizeof(struct audio_dec_pts_info)
#define INBAND_PCM_SIZE      sizeof(struct audio_info_pcm_format)
#define INBAND_EOS_SIZE      sizeof(struct audio_dec_eos)
#define INBAND_INDEX_SIZE    sizeof(struct audio_info_channel_index_new)
#define INBAND_INFO_SIZE     sizeof(struct audio_inband_private_info)
#define AUDIO_START_THRES    (INBAND_PCM_SIZE + INBAND_INDEX_SIZE + (2 * INBAND_INFO_SIZE))
#define NEW_CHANNEL_INDEX_INFO_SIZE  sizeof(struct audio_info_channel_index_new)
#define OLD_CHANNEL_INDEX_INFO_SIZE  sizeof(struct audio_info_channel_index_old)

int rpc_switch_focus(phys_addr_t paddr, void *vaddr, struct audio_rpc_focus *focus);
int rpc_set_low_water_level(phys_addr_t paddr, void *vaddr, bool is_low_water);
int rpc_create_pp_agent(phys_addr_t paddr, void *vaddr, int *ppId, int *pinId);
int rpc_set_refclock(phys_addr_t paddr, void *vaddr, struct audio_rpc_refclock *p_clock);
int rpc_pp_init_pin_svc(phys_addr_t paddr, void *vaddr, int instance_id);
int rpc_set_mixing_idx(struct device *dev, struct rtk_snd_mixer *mixer);
int rpc_set_dec_volume(struct device *dev, int volume);
int rpc_set_dec_volume_config(struct device *dev, int steps, int direct, int id);
int rpc_set_dec_volume_new(struct device *dev, int volume, int id);
#endif
