/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (c) 2017-2020 Realtek Semiconductor Corp.
 */

#ifndef SND_HIFI_REALTEK_H
#define SND_HIFI_REALTEK_H

#include <sound/pcm.h>
#include "snd_rtk_audio_enum.h"
#include <soc/realtek/rtk-krpc-agent.h>

extern struct rtk_krpc_ept_info *hifi_ept_info;

/* playback */
#define RTK_DMP_PLAYBACK_INFO (SNDRV_PCM_INFO_INTERLEAVED | \
				SNDRV_PCM_INFO_NONINTERLEAVED | \
				SNDRV_PCM_INFO_RESUME | \
				SNDRV_PCM_INFO_MMAP | \
				SNDRV_PCM_INFO_MMAP_VALID | \
				SNDRV_PCM_INFO_PAUSE)

#define RTK_DMP_PLAYBACK_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
				SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE | \
				SNDRV_PCM_FMTBIT_S24_3LE)

#define RTK_DMP_PLYABACK_RATES (SNDRV_PCM_RATE_8000 | \
				SNDRV_PCM_RATE_16000 | \
				SNDRV_PCM_RATE_22050 | \
				SNDRV_PCM_RATE_32000 | \
				SNDRV_PCM_RATE_44100 | \
				SNDRV_PCM_RATE_48000 | \
				SNDRV_PCM_RATE_88200 | \
				SNDRV_PCM_RATE_96000 | \
				SNDRV_PCM_RATE_176400 | \
				SNDRV_PCM_RATE_192000 | \
				SNDRV_PCM_RATE_384000)

#define RTK_DMP_PLAYBACK_RATE_MIN       8000
#define RTK_DMP_PLAYBACK_RATE_MAX       384000
#define RTK_DMP_PLAYBACK_CHANNELS_MIN   1
#define RTK_DMP_PLAYBACK_CHANNELS_MAX   8
#define RTK_DMP_PLAYBACK_MAX_BUFFER_SIZE         (4096 * 1024)
#define RTK_DMP_PLAYBACK_MIN_PERIOD_SIZE         1024
#define RTK_DMP_PLAYBACK_MAX_PERIOD_SIZE         (2048 * 1024)
#define RTK_DMP_PLAYBACK_PERIODS_MIN             2
#define RTK_DMP_PLAYBACK_PERIODS_MAX             4096
#define RTK_DMP_PLAYBACK_FIFO_SIZE               32

/* capture */
#define RTK_DMP_CAPTURE_INFO RTK_DMP_PLAYBACK_INFO
#define RTK_DMP_CAPTURE_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE | \
				SNDRV_PCM_FMTBIT_S24_3LE | \
				SNDRV_PCM_FMTBIT_S32_LE)

#define RTK_DMP_CAPTURE_RATES (SNDRV_PCM_RATE_8000 | \
				SNDRV_PCM_RATE_16000 | \
				SNDRV_PCM_RATE_22050 | \
				SNDRV_PCM_RATE_32000 | \
				SNDRV_PCM_RATE_44100 | \
				SNDRV_PCM_RATE_48000 | \
				SNDRV_PCM_RATE_88200 | \
				SNDRV_PCM_RATE_96000 | \
				SNDRV_PCM_RATE_176400 | \
				SNDRV_PCM_RATE_192000 | \
				SNDRV_PCM_RATE_384000)

#define RTK_DMP_CAPTURE_RATE_MIN 8000
#define RTK_DMP_CAPTURE_RATE_MAX 384000
#define RTK_DMP_CAPTURE_CHANNELS_MIN 1
#define RTK_DMP_CAPTURE_CHANNELS_MAX 8
#define RTK_DMP_CAPTURE_MAX_BUFFER_SIZE         RTK_DMP_PLAYBACK_MAX_BUFFER_SIZE
#define RTK_DMP_CAPTURE_MIN_PERIOD_SIZE         RTK_DMP_PLAYBACK_MIN_PERIOD_SIZE
#define RTK_DMP_CAPTURE_MAX_PERIOD_SIZE         (16 * 1024)
#define RTK_DMP_CAPTURE_PERIODS_MIN             RTK_DMP_PLAYBACK_PERIODS_MIN
#define RTK_DMP_CAPTURE_PERIODS_MAX             RTK_DMP_PLAYBACK_PERIODS_MAX
#define RTK_DMP_CAPTURE_FIFO_SIZE               RTK_DMP_PLAYBACK_FIFO_SIZE

#define S_OK 0x10000000
#define RTK_DEC_AO_BUFFER_SIZE          (7 * 1024)
#define RTK_ENC_AI_BUFFER_SIZE          (32 * 1024)
#define RTK_ENC_LPCM_BUFFER_SIZE        (32 * 1024)
#define RTK_ENC_PTS_BUFFER_SIZE         (8 * 1024)
#define RTK_AI_PACK_BUFFER_SIZE		(64 * 1024)
#define MAX_CAR_CH 32
#define MAX_DELAY_CH 32

#define HIFI_AEC_SETUP 0x10000030
#define HIFI_AEC_RUN   0x10000031
#define HIFI_AEC_STOP  0x10000032

enum CAPTURE_TYPE {
	ENUM_AIN_HDMIRX = 0,
	ENUM_AIN_I2S = 1,
	ENUM_AIN_NON_PCM = 2,
	ENUM_AIN_AEC_DMIC = 3,
	ENUM_AIN_AEC_I2S = 4,
	ENUM_AIN_AUDIO_V3 = 5,
	ENUM_AIN_AUDIO_V4 = 6,
	ENUM_AIN_LOOPBACK = 7,
	ENUM_AIN_DMIC_PASSTHROUGH = 8,
	ENUM_AIN_PURE_DMIC = 9,
	ENUM_AIN_BTPCM = 10,
	ENUM_AIN_BTPCM_OUT_PASSTHROUGH = 11,
	ENUM_AIN_BTPCM_IN_PASSTHROUGH = 12,
	ENUM_AIN_ANA1_IN = 13,
	ENUM_AIN_ANA2_IN = 14,
	ENUM_AIN_TDM_IN = 15,
	ENUM_AIN_SPDIF_IN = 16,
	ENUM_AIN_I2S1_IN = 17,
	ENUM_AIN_I2S2_IN = 18,
	ENUM_AIN_TDM1_IN = 19,
	ENUM_AIN_TDM2_IN = 20,
};

enum ENUM_AUDIO_AIN_HW_TYPE{
	ENUM_AUDIO_AIN_HW_HDMI = 0,
	ENUM_AUDIO_AIN_HW_I2S,
	ENUM_AUDIO_AIN_HW_ANALOG,
	ENUM_AUDIO_AIN_HW_AMIC,
	ENUM_AUDIO_AIN_HW_DMIC,
	ENUM_AUDIO_AIN_HW_SPF_OPTICAL,
	ENUM_AUDIO_AIN_HW_SPF_COAXIAL,
	ENUM_AUDIO_AIN_HW_ARC,
	ENUM_AUDIO_AIN_HW_TDM,
	ENUM_AUDIO_AIN_HW_EARC,
	ENUM_AUDIO_AIN_HW_AO_I2S_LOOPBACK,
	ENUM_AUDIO_AIN_HW_AO_SPDIF_LOOPBACK,
	ENUM_AUDIO_AIN_HW_AO_HDMI_LOOPBACK,
	ENUM_AUDIO_AIN_HW_I2S_1,
	ENUM_AUDIO_AIN_HW_I2S_2,
	ENUM_AUDIO_AIN_HW_I2S_3,  /* not used */
	ENUM_AUDIO_AIN_HW_TDM_1,
	ENUM_AUDIO_AIN_HW_TDM_2,
	ENUM_AUDIO_AIN_HW_TDM_3,  /* not used */
	ENUM_AUDIO_AIN_HW_BTPCM,
	ENUM_AUDIO_AIN_HW_ANALOG_1,
	ENUM_AUDIO_AIN_HW_NONE
};

enum PLAYBACK_TYPE {
	ENUM_AO_DECODER = 0,
	ENUM_AO_SKIP_DECODER = 1,
};

enum EQ_BAND_TYPE {
	ENUM_10_TONE_CONTROL = 0,
	ENUM_16_TONE_CONTROL = 1,
	ENUM_32_TONE_CONTROL = 2,
	ENUM_TONE_CONTROL_MAX = 3,
	ENUM_BALANCE_CONTROL = 60,
	ENUM_DELAY_CONTROL = 61,
	ENUM_ANALOG_IN1_PATH = 62,
	ENUM_ANALOG_IN2_PATH = 63,
	ENUM_BTPCM_PASSTHROUGH_AI_PATH = 64,
	ENUM_BTPCM_AEC_CONTROL = 65,
	ENUM_ANA1_AEC_CONTROL = 66,
	ENUM_ANA2_AEC_CONTROL = 67,
	ENUM_TDM_AEC_CONTROL = 68,
	ENUM_ANA1_AGC = 69,
	ENUM_ANA2_AGC = 70,
	ENUM_ANA1_DGC = 71,
	ENUM_ANA2_DGC = 72,
	ENUM_BTPCM_DEBUG = 73,
	ENUM_BTPCM_MIC_CH_SEL = 74,
	ENUM_AI_I2S0_MODE=75,
	ENUM_AI_I2S1_MODE=76,
	ENUM_AI_I2S2_MODE=77,
	ENUM_ANA1_DIFFERENTIAL_EN = 78,
	ENUM_ANA2_DIFFERENTIAL_EN = 79,
	ENUM_MIC_MUTE_EN = 80,
	ENUM_AI_TDM0_MODE= 81,
	ENUM_AI_TDM1_MODE= 82,
	ENUM_AI_TDM2_MODE= 83,
	ENUM_BTPCM_MODE= 84,
	ENUM_BTPCM_QUEUE_BUF = 85,
};

#define MAX_PCM_DEVICES     1
#define MAX_PCM_SUBSTREAMS  3
#define MAX_AI_DEVICES      2

#define MIXER_ADDR_MASTER   0
#define MIXER_ADDR_LINE     1
#define MIXER_ADDR_MIC      2
#define MIXER_ADDR_SYNTH    3
#define MIXER_ADDR_CD       4
#define MIXER_ADDR_LAST     4

#define RTK_VOLUME(xname, xindex, addr)    \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.index = xindex, \
	.info = snd_RTK_volume_info, \
	.get = snd_RTK_volume_get, \
	.put = snd_RTK_volume_put, \
	.private_value = addr \
}

#define RTK_CAPSRC(xname, xindex, addr)    \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = xname, \
	.index = xindex, \
	.info = snd_RTK_capsrc_info, \
	.get = snd_RTK_capsrc_get, \
	.put = snd_RTK_capsrc_put, \
	.private_value = addr \
}

#define RTK_FEATURE_CTRL(band_name, ctl_index, band_num) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name = band_name, \
	.index = ctl_index, \
	.info = snd_feature_ctrl_info, \
	.get = snd_feature_ctrl_get, \
	.put = snd_feature_ctrl_put, \
	.private_value = band_num \
}

/************************************************************************/
/* ENUM                                                                 */
/************************************************************************/
struct audio_config_command {
	enum AUDIO_CONFIG_CMD_MSG msd_id;
	unsigned int value[6];
};

struct rpcres_long {
	u32 result;
	int data;
};

struct audio_rpc_privateinfo_parameters {
	int instance_id;
	enum AUDIO_ENUM_PRIVAETINFO type;
	volatile int private_info[16];
};

struct audio_rpc_privateinfo_parameters_ext {
	int instance_id;
	enum AUDIO_ENUM_PRIVAETINFO type;
	volatile int private_info[16];
	short parameter[32];
};

struct audio_rpc_privateinfo_returnval {
	int instance_id;
	volatile int private_info[16];
};

struct audio_rpc_instance {
	int instance_id;
	int type;
	int reserved[30];
};

struct rpc_create_ao_agent_t {
	struct audio_rpc_instance info;
	struct rpcres_long retval;
	u32 ret;
};

struct rpc_create_pcm_decoder_ctrl_t {
	struct audio_rpc_instance instance;
	struct rpcres_long res;
	u32 ret;
};

struct rpc_btpcm_config {
	int version;    	// upgrade when data members changed.
	int pcm_clk_fs; 	// 0x98000360[3:1]
	char pcm_bits;  	// 0x98006A10[3]
	char slave_mode; 	// 0x98006A10[4], 0:master, 1:slave
	char format;    	// 0x98006A10[6:5]
	char padding_sel; 	// 0x98006A10[20:19]
	char slot_mode; 	// 0x98006A10[23:21]
	unsigned char frame_thld;   // 0x98006A10[18:11]
	char rvd2;
	char rvd3;
	int rvd[15];
};

enum ENUM_I2S_MODE_USER
{
	ENUM_AI_I2S_SHARE = 0,
	ENUM_AI_I2S_MASTER,
	ENUM_AI_I2S_SLAVE,
	ENUM_AI_I2S_SLAVE_ENFORCE,
};

enum ENUM_I2S_MODE
{
	ENUM_AI_I2S_PIN_SHARE = 0,  // using AO i2s0
	ENUM_AI_I2S_PIN_INDEPENDENT_MASTER,
	ENUM_AI_I2S_PIN_INDEPENDENT_SLAVE,
	ENUM_AI_I2S_PIN_SHARE_AO_I2S1,  // using AO i2s1
	ENUM_AI_I2S_PIN_SHARE_AO_I2S2,  // using AO i2s2
	ENUM_AI_I2S_PIN_SHARE_AI_I2S0,  // using AI i2s0
	ENUM_AI_I2S_PIN_SLAVE_ENFORCE,  // using AI i2s0
	ENUM_AI_I2S_PIN_SHARE_TOTAL // keep the last
};

enum ENUM_TDM_MODE
{
	ENUM_TDM_PIN_SHARE_NONE = 0,
	ENUM_TDM_PIN_SHARE_AO_TDM_0,
	ENUM_TDM_PIN_SHARE_AO_TDM_1,
	ENUM_TDM_PIN_SHARE_AO_TDM_2,
	ENUM_TDM_PIN_SHARE_AI_TDM_0,
};

struct rpc_tdmin_config {
	int version;
	int data_format;
	int bclk;
	int lrck;
	int sample_rate;
	char i2s_channel_len;
	char slave_mode;
	char channel_num;
	char slot;
	char data_len;
	char tdm_channel_len;
	char pin_share;
	char rvd1[1];
	int rvd[7];
};

struct audio_rpc_ringbuffer_header {
	int instance_id;
	int pin_id;
	int ringbuffer_header_list[8];
	int read_idx;
	int list_size;
	int reserved[20];
};

#ifdef CONFIG_RTK_CACHEABLE_HEADER
/* Ring Buffer header structure (cacheable) */
struct ringbuffer_header {
	/* align 128 bytes */
	unsigned int write_ptr;
	unsigned char w_reserved[124];

	/* align 128 bytes */
	unsigned int read_ptr[4];
	unsigned char  r_reserved[112];

	unsigned int magic;
	unsigned int begin_addr;
	unsigned int size;
	unsigned int buffer_id;
	unsigned int num_read_ptr;
	unsigned int reserve2;
	unsigned int reserve3;

	int          file_offset;
	int          requested_file_offset;
	int          file_size;
	int          seekable;

	unsigned char  readonly[84];
};
#else
/* Ring Buffer header is the shared memory structure */
struct ringbuffer_header {
	unsigned int magic;
	unsigned int begin_addr;
	unsigned int size;
	unsigned int buffer_id;

	unsigned int write_ptr;
	unsigned int num_read_ptr;
	unsigned int reserve2;
	unsigned int reserve3;

	unsigned int read_ptr[4];

	int          file_offset;
	int          requested_file_offset;
	int          file_size;

	int          seekable;
/*
 * file_offset:
 * the offset to the streaming file from the beginning of the file.
 * It is set by system to tell FW that the current streaming is starting from file_offset bytes.
 * For example, the TIFF file display will set file_offset to 0 at beginning.
 *
 * requested_file_offset:
 * the offset to be set by video firmware, to request system to seek to other place.
 * The initial is -1.When it is not equal to -1, that means FW side is requesting a new seek.
 *
 * file_size:
 * file size. At current implementation, only TIFF decode needs the file_size,
 * other decoding does not pay attention to this field
 *
 * the behavior for TIFF seek:
 * At the initial value, file_offset = 0, or at any initial offset
 *(for example, resume from bookmark), requested_file_offset=-1. file_size= file size.
 * 1. If FW needs to perform seek operation,
 *    FW set requested_file_offset to the value it need to seek.
 * 2. Once system see RequestedOffset is not -1,
 *    system reset the ring buffer
 *    (FW need to make sure it will not use ring buffer after request the seek),
 *    set file_offset to the new location (the value of requested_file_offset),
 *    then set RequestedOffset to -1. From now on,
 *    system will stream data from byte file_offset of the file.
 * 3. FW needs to wait until RequestedOffset== -1,
 *    then check the value inside file_offset.
 *    If file_offset is -1, that means read out of bound.
 *    If system already finish the streaming before FW issue a seek,
 *    system will still continue polling.
 */
};
#endif

struct rpc_initringbuffer_header_t {
	struct audio_rpc_ringbuffer_header header;
	struct rpcres_long ret;
	u32 res;
};

struct audio_rpc_connection {
	int src_instance_id;
	int src_pin_id;
	int des_instance_id;
	int des_pin_id;
	int media_type;
	int reserved[27];
};

struct rpc_connection_t {
	struct audio_rpc_connection out;
	struct rpcres_long ret;
	u32 res;
};

struct rpc_t {
	int inst_id;
	int reserved[31];
	struct rpcres_long retval;
	u32 res;
};

struct rpc_pause_t {
	int inst_id;
	int reserved[31];
	u32 retval;
	u32 res;
};

struct audio_rpc_sendio {
	int instance_id;
	int pin_id;
	int reserved[30];
};

struct rpc_flash_t {
	struct audio_rpc_sendio sendio;
	struct rpcres_long retval;
	u32 res;
};

struct audio_inband_cmd_pkt_header {
	enum AUDIO_INBAND_CMD_TYPE type;
	int size;
};

/* private_info[6] is used for choosing decoder sync pts method */
struct audio_dec_new_format {
	struct audio_inband_cmd_pkt_header header;
	unsigned int w_ptr;
	enum AUDIO_DEC_TYPE audio_type;
	int private_info[8];
};

struct audio_dec_pts_info {
	struct audio_inband_cmd_pkt_header header;
	unsigned int w_ptr;
	unsigned int PTSH;
	unsigned int PTSL;
};

struct rpc_stop_t {
	int instance_id;
	int reserved[31];
	struct rpcres_long retval;
	u32 res;
};

struct rpc_destroy_t {
	int instance_id;
	int reserved[31];
	struct rpcres_long retval;
	u32 res;
};

struct rpc_get_volume_t {
	struct rpcres_long res;
	u32 ret;
	struct audio_rpc_privateinfo_parameters param;
};

struct audio_ringbuf_ptr_64 {
	unsigned long base;
	unsigned long limit;
	unsigned long cp;
	unsigned long rp;
	unsigned long wp;
};

struct alsa_mem_info {
	phys_addr_t p_phy;
	unsigned int *p_virt;
	unsigned int size;
};

struct alsa_latency_info {
	unsigned int latency;
	unsigned int ptsl;
	unsigned int ptsh;
	unsigned int sum; /* latency + ptsL */
	unsigned int decin_wp;
	unsigned int sync;
	unsigned int dec_in_delay;
	unsigned int dec_out_delay;
	unsigned int ao_delay;
	int rvd[8];
};

enum {
	ENUM_ANALOG_MIC_VOLTAGE_DEFAULT = 0,
	ENUM_ANALOG_MIC_VOLTAGE_1P575V,
	ENUM_ANALOG_MIC_VOLTAGE_1P65V,
	ENUM_ANALOG_MIC_VOLTAGE_1P725V,
	ENUM_ANALOG_MIC_VOLTAGE_1P8V,
};
enum {
	ENUM_ANALOG_GAIN_0dB = 0,
	ENUM_ANALOG_GAIN_12dB,
	ENUM_ANALOG_GAIN_30dB,
	ENUM_ANALOG_GAIN_42dB,
};
enum {
	ENUM_I2S_IN = 0,
	ENUM_TDM_IN = 1,
	ENUM_DMIC_IN = 2,
	ENUM_ANA1_IN = 3,
	ENUM_ANA2_IN = 4,
};

struct audio_analog_hw_config {
	char amic_voltage;
	char analog_gain_l;
	char digital_gain_l;
	char amic_differential_l;
	char digital_gain_en;
	char digital_gain_r;
	char analog_gain_r;
	char amic_differential_r;
	int rvd[2];
};

struct audio_rpc_aio_privateinfo_parameters {
	int instance_id;
	enum AUDIO_ENUM_AIO_PRIVAETINFO type;
	int argate_info[16];
};

struct audio_rpc_aio_privateinfo_parameters_ext {
	int instance_id;
	enum AUDIO_ENUM_AIO_PRIVAETINFO type;
	int argate_info[16];
	short parameter[32];
};

struct audio_rpc_dec_privateinfo_parameters {
	int instance_id;
	enum AUDIO_ENUM_DEC_PRIVAETINFO type;
	long private_info[16];
};

struct audio_mixing_index {
	int enable;
	int mixing_channel[32];
	int reserved[15];
};

struct audio_channel_index {
	int enable;
	int index[2];
	int reserved[29];
};

struct audio_equalizer_mode {
	int mode;
	int gain[32];
};

struct audio_delay_ctrl {
	int mode;
	int is_enable;
	short ch_delay_time[MAX_DELAY_CH];
};

struct audio_equalizer_config {
	int instance_id;
	int gbl_var_eq_id;
	unsigned char ena;
	struct audio_equalizer_mode app_eq_config;
};

struct audio_ext_equalizer_mode {
	int mode;
	int filternum;
	int gain[32];
	int Reserve[8];
};

struct audio_ext_equalizer_config {
	int instance_id;
	int gbl_var_eq_id;
	unsigned char ena;
	int len;
	unsigned int magic;
	unsigned int peq_config;
};

struct hifi_aec_rpc_setup {
	int private_info[32];
	u32 retval;
	u32 res;
};

/* rtk sound mixer */
struct rtk_snd_mixer {
	struct device *dev;
	spinlock_t mixer_lock;
	int mixer_volume[MIXER_ADDR_LAST + 1][2];
	int capture_source[MIXER_ADDR_LAST + 1][2];
	int balance[MAX_CAR_CH];
	int dec_volume;
	int mixing_cnt;
	int analog_in1_path[2];
	int analog_in2_path[2];
	int btpcm_in_path;
	int btpcm_aec_en;
	int btpcm_mode;
	int btpcm_queue_buf;
	int ana1_aec_en;
	int ana2_aec_en;
	int tdm_aec_en;
	int ana1_agc[2];
	int ana2_agc[2];
	int ana1_dgc[3];
	int ana2_dgc[3];
	int btpcm_dbg[2];
	int btpcm_mic_ch;
	int ai_i2s0_mode;
	int ai_i2s1_mode;
	int ai_i2s2_mode;
	int ai_tdm0_mode;
	int ai_tdm1_mode;
	int ai_tdm2_mode;
	int ana1_differential_en[2];
	int ana2_differential_en[2];
	int mic_mute_en[2];
	struct work_struct work_volume;
	struct work_struct work_10eq;
	struct work_struct work_16eq;
	struct work_struct work_32eq;
	struct work_struct work_balance;
	struct work_struct work_audio_delay;
	struct work_struct work_mixing_idx;
	struct work_struct work_dec_volume;
	struct work_struct work_mic_mute;
	struct audio_ext_equalizer_mode eq_mode[ENUM_TONE_CONTROL_MAX];
	struct audio_mixing_index mixing_idx;
	struct audio_channel_index channel_idx;
	struct audio_delay_ctrl delay_ctrl;
	int ao_agent_id;
	int multi_ao;
};

/* RTK PCM instance */
struct snd_rtk_pcm {
	/* inband ring at the first, because of 128 align for HIFI */
	unsigned int dec_inband_data[64];
	struct ringbuffer_header *dec_out_ring[8];
	struct rtk_snd_mixer *mixer;
	struct snd_pcm_substream *substream;
	struct ringbuffer_header *dec_inband_ring;
	struct ringbuffer_header *dec_inring;
	struct audio_mixing_index mixing_idx;
	struct audio_channel_index channel_idx;

	snd_pcm_uframes_t hw_ptr;
	snd_pcm_uframes_t prehw_ptr;
	snd_pcm_uframes_t total_read;
	snd_pcm_uframes_t total_write;

#ifdef DEBUG_RECORD
	struct file *fp;
	loff_t pos;
	mm_segment_t fs;
#endif

	/* hrtimer member */
	struct hrtimer hr_timer;
	enum hrtimer_restart en_hr_timer;
	ktime_t ktime;

	int dec_agent_id;
	int dec_pin_id;
	int ao_agent_id;
	int ao_pin_id;
	int volume;
	int init_ring;
	int hw_init;
	int last_channel;
	int playback_mode; /* 0: Legacy, 1: skip decode, 2: AO2 */
	int dec_out_msec;
	int set_mixing_index;
	int set_channel_index;
	int *g_sharemem_ptr;
	struct alsa_latency_info *g_sharemem_ptr2;
	struct alsa_latency_info *g_sharemem_ptr3;
	unsigned int period_bytes;
	unsigned int ring_size;
	unsigned int dbg_count;
	phys_addr_t phy_dec_out_data[8];
	phys_addr_t phy_addr;
	phys_addr_t phy_addr_rpc;
	phys_addr_t g_sharemem_ptr_dat;
	phys_addr_t g_sharemem_ptr_dat2;
	phys_addr_t g_sharemem_ptr_dat3;
	phys_addr_t phy_dec_inring;
	phys_addr_t phy_dec_inband_ring;
	phys_addr_t phy_dec_out_ring[8];
	void *vaddr_rpc;
	void *vaddr_dec_out_data[8];
	spinlock_t playback_lock;
};

struct snd_rtk_cap_pcm {
	struct rtk_snd_mixer *mixer;
	struct snd_pcm_substream *substream;
	struct ringbuffer_header *airing[8];
	struct ringbuffer_header *lb_airing[8];
	struct ringbuffer_header *lpcm_ring;
	struct ringbuffer_header *lpcm_pack_ring;
	struct ringbuffer_header *lb_lpcm_ring;
	struct ringbuffer_header *mix_lpcm_ring;
	struct ringbuffer_header *pts_ring_hdr;
	snd_pcm_uframes_t total_write;
	enum AUDIO_FORMAT_OF_AI_SEND_TO_ALSA ai_format;

	/* hrtimer member */
	struct hrtimer hr_timer;
	enum hrtimer_restart en_hr_timer;
	ktime_t ktime;

	int ai_agent_id;
	int ai_lb_agent_id;
	int ao_agent_id;
	int ao_pin_id;
	int init_ring;
	int source_in;
	int aec_feature;
	int dmic_volume[2];
	unsigned int *lpcm_data;
	unsigned int *lpcm_pack_data;
	unsigned int *lb_lpcm_data;
	unsigned int *mix_lpcm_data;
	unsigned int period_bytes;
	unsigned int frame_bytes;
	unsigned int ring_size;
	unsigned int lpcm_ring_size;
	unsigned int lpcm_pack_ring_size;
	unsigned int mix_lpcm_ring_size;
	phys_addr_t phy_airing_data[8];
	phys_addr_t phy_lb_airing_data[8];
	phys_addr_t phy_lpcm_data;
	phys_addr_t phy_lpcm_pack_data;
	phys_addr_t phy_lb_lpcm_data;
	phys_addr_t phy_mix_lpcm_data;
	phys_addr_t phy_addr;
	phys_addr_t phy_addr_rpc;
	phys_addr_t phy_airing[8];
	phys_addr_t phy_lb_airing[8];
	phys_addr_t phy_pts_ring_hdr;
	phys_addr_t phy_lpcm_ring;
	phys_addr_t phy_lpcm_pack_ring;
	phys_addr_t phy_lb_lpcm_ring;
	phys_addr_t phy_mix_lpcm_ring;
	struct alsa_mem_info pts_mem;
	struct audio_ringbuf_ptr_64 pts_ring;
	struct timespec64 ts;
	void *vaddr_rpc;
	void *va_airing_data[8];
	void *va_lb_airing_data[8];
	size_t size_airing[8];
	spinlock_t capture_lock;
};

struct snd_pcm_mmap_fd {
	s32 dir;
	s32 fd;
	s32 size;
	s32 actual_size;
};

struct snd_dma_buffer_kref {
	struct snd_dma_buffer dmab;
	struct kref ref;
};

struct ai_gain_set {
	int gain_sign;
	int gain_level;
};

#define SNDRV_PCM_IOCTL_VOLUME_SET   _IOW('A', 0xE0, int)
#define SNDRV_PCM_IOCTL_GET_LATENCY  _IOR('A', 0xF0, int)
#define SNDRV_PCM_IOCTL_GET_FW_DELAY _IOR('A', 0xF1, unsigned int)
#define SNDRV_PCM_IOCTL_MMAP_DATA_FD _IOWR('A', 0xE4, struct snd_pcm_mmap_fd)
#define SNDRV_PCM_IOCTL_DMIC_VOL_SET _IOW('A', 0xE6, int)
#define SNDRV_PCM_IOCTL_MIX_IDX_SET  _IOW('A', 0xE7, struct audio_mixing_index)
#define SNDRV_PCM_IOCTL_AI_GAIN_SET  _IOW('A', 0xE8, struct ai_gain_set)
#define SNDRV_PCM_IOCTL_CHANNEL_IDX_SET  _IOW('A', 0xE9, struct audio_channel_index)
/************************************************************************/
/* PROTOTYPE                                                            */
/************************************************************************/
int snd_realtek_hw_ring_write(struct ringbuffer_header *ring,
			      void *data, int len, unsigned int offset);
int snd_ept_init(struct rtk_krpc_ept_info *krpc_ept_info);

/* RPC function */
int rpc_connect_svc(phys_addr_t paddr, void *vaddr,
		    struct audio_rpc_connection *pconnection);
int rpc_destroy_svc(phys_addr_t paddr, void *vaddr, int instance_id);
int rpc_flush_svc(phys_addr_t paddr, void *vaddr, struct audio_rpc_sendio *sendio);
int rpc_initringbuffer_header(phys_addr_t paddr, void *vaddr,
			      struct audio_rpc_ringbuffer_header *header, int buffer_count);
int rpc_pause_svc(phys_addr_t paddr, void *vaddr, int instance_id);
int rpc_run_svc(phys_addr_t paddr, void *vaddr, int instance_id);
int rpc_stop_svc(phys_addr_t paddr, void *vaddr, int instance_id);
int rpc_create_ao_agent(phys_addr_t paddr, void *vaddr, int *ao_id, int pin_id);
int rpc_get_ao_flash_pin(phys_addr_t paddr, void *vaddr, int ao_agent_id);
int rpc_create_decoder_agent(phys_addr_t paddr, void *vaddr, int *dec_id, int *pin_id);
int rpc_set_ao_flash_volume(struct device *dev, int agent_id, int pin_id, int volume);
int rpc_release_ao_flash_pin(phys_addr_t paddr, void *vaddr, int ao_agent_id, int ao_pin_id);
int rpc_set_volume(struct device *dev, int volume);
int rpc_set_volume_new(struct device *dev, int volume, int id);
int rpc_get_volume(phys_addr_t paddr, void *vaddr);
int rpc_put_share_memory_latency(phys_addr_t paddr, void *vaddr,
				 void *p, void *p2, int dec_id, int ao_id, int ao_agent_id, int type);
int rpc_create_ai_agent(phys_addr_t paddr, void *vaddr, int *ai_id);
int rpc_ai_connect_alsa(phys_addr_t paddr, void *vaddr,
			struct snd_pcm_runtime *runtime,
			int loopback);
int rpc_ai_config_hdmi_rx_in(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_ai_config_i2s_in(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_ai_config_audio_in(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_ai_config_loopback_in(phys_addr_t paddr, void *vaddr,
				  int instance_id, int loopback_type);
int rpc_ai_config_dmic_in(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_ai_config_btpcm_in(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_ao_config_btpcm_out(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_ai_config_tdm_in(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_ai_config_spdif_in(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_ai_config_analog_in(phys_addr_t paddr, void *vaddr, struct snd_pcm_runtime *runtime);
int rpc_destroy_ai_flow_svc(phys_addr_t paddr, void *vaddr,
			    int instance_id, int hw_configed);
int rpc_ai_config_nonpcm_in(phys_addr_t paddr, void *vaddr,
			    struct snd_rtk_cap_pcm *dpcm);
int rpc_set_ai_flash_volume(phys_addr_t paddr, void *vaddr,
			    struct snd_rtk_cap_pcm *dpcm, unsigned int volume);
int rpc_ao_config_without_decoder(phys_addr_t paddr, void *vaddr, struct snd_pcm_runtime *runtime);
int rpc_create_global_ao(phys_addr_t paddr, void *vaddr, int *ao_id);
int rpc_ai_connect_ao(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_set_max_latency(phys_addr_t paddr, void *vaddr, struct snd_rtk_pcm *dpcm);
int rpc_set_eq(struct device *dev, struct audio_equalizer_mode *eq_mode);
int rpc_set_ext_eq(struct device *dev, struct audio_ext_equalizer_mode *ext_eq_mode, int id);
int rpc_set_balance(struct device *dev, struct rtk_snd_mixer *mixer);
int rpc_set_audio_delay(struct device *dev, struct rtk_snd_mixer *mixer);
int rpc_set_btpcm_config(phys_addr_t paddr, void *vaddr, struct rtk_snd_mixer *mixer);
int rpc_set_btpcm_queue_buf(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm);
int rpc_set_tdmin_config(phys_addr_t paddr, void *vaddr, struct snd_pcm_runtime *runtime);
int rpc_run_aec(void);
int rpc_stop_aec(void);
int rpc_setup_aec(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm, struct snd_pcm_runtime *runtime);
int rpc_setup_aec_test(phys_addr_t paddr, void *vaddr, struct snd_rtk_cap_pcm *dpcm, struct snd_pcm_runtime *runtime);
int rpc_set_ai_gain_volume(struct device *dev, int ai_agent_id, struct ai_gain_set *ai_gain_param);
int rpc_config_amic_voltage(phys_addr_t paddr, void *vaddr, int en, int voltage);
int rpc_config_mic_mute(struct device *dev, struct rtk_snd_mixer *mixer);
#endif /* SND_HIFI_REALTEK_H */
