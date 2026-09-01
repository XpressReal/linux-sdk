// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek framebuffer driver
 *
 * Copyright (c) 2019-2020 Realtek Semiconductor Corp.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include <soc/realtek/kernel-rpc.h>
#include <soc/realtek/uapi/ion_rtk.h>
#include <linux/syscalls.h>
#include <linux/scatterlist.h>

#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>


#include "debug.h"

#include "rtk_fb.h"
#include "rtk_fb_rpc.h"
#include "dc2vo.h"
#include "dc_rpc.h"

#include <ion_rtk_alloc.h>

#define ion_alloc ext_rtk_ion_alloc

/* Uncomment to trace rpc  */
//#define _TRACE_FB_RPC

#ifdef _TRACE_FB_RPC
#define TRACE_RPC() printk(KERN_ALERT "rtk_fb_RPC %s\n", __FUNCTION__);
#else
#define TRACE_RPC()
#endif

#define HDMI_OUT_EN         1
#define HDMI_VER            2
#define HDMI_EDID_SIZE      5

#define RPC_ALIGN_SZ 128

void *rtkfb_virt = NULL;
struct dma_buf_attachment *rtkfb_attach = NULL;
struct sg_table *rtkfb_table = NULL;
#define S_OK        0x10000000

typedef enum
{
    ENUM_KERNEL_RPC_CREATE_AGENT,   // 0
    ENUM_KERNEL_RPC_INIT_RINGBUF,
    ENUM_KERNEL_RPC_PRIVATEINFO,
    ENUM_KERNEL_RPC_RUN,
    ENUM_KERNEL_RPC_PAUSE,
    ENUM_KERNEL_RPC_SWITCH_FOCUS,   // 5
    ENUM_KERNEL_RPC_MALLOC_ADDR,
    ENUM_KERNEL_RPC_VOLUME_CONTROL,      // AUDIO_CONFIG_COMMAND
    ENUM_KERNEL_RPC_FLUSH,               // AUDIO_RPC_SENDIO
    ENUM_KERNEL_RPC_CONNECT,             // AUDIO_RPC_CONNECTION
    ENUM_KERNEL_RPC_SETREFCLOCK,    // 10     // AUDIO_RPC_REFCLOCK
    ENUM_KERNEL_RPC_DAC_I2S_CONFIG,      // AUDIO_CONFIG_DAC_I2S
    ENUM_KERNEL_RPC_DAC_SPDIF_CONFIG,    // AUDIO_CONFIG_DAC_SPDIF
    ENUM_KERNEL_RPC_HDMI_OUT_EDID,       // AUDIO_HDMI_OUT_EDID_DATA
    ENUM_KERNEL_RPC_HDMI_OUT_EDID2,      // AUDIO_HDMI_OUT_EDID_DATA2
    ENUM_KERNEL_RPC_HDMI_SET,       // 15     // AUDIO_HDMI_SET
    ENUM_KERNEL_RPC_HDMI_MUTE,           //AUDIO_HDMI_MUTE_INFO
    ENUM_KERNEL_RPC_ASK_DBG_MEM_ADDR,
    ENUM_KERNEL_RPC_DESTROY,
    ENUM_KERNEL_RPC_STOP,
    ENUM_KERNEL_RPC_CHECK_READY,     // 20    // check if Audio get memory pool from AP
    ENUM_KERNEL_RPC_GET_MUTE_N_VOLUME,   // get mute and volume level
    ENUM_KERNEL_RPC_EOS,
    ENUM_KERNEL_RPC_ADC0_CONFIG,
    ENUM_KERNEL_RPC_ADC1_CONFIG,
    ENUM_KERNEL_RPC_ADC2_CONFIG,    // 25
#if defined(AUDIO_TV_PLATFORM)
    ENUM_KERNEL_RPC_BBADC_CONFIG,
    ENUM_KERNEL_RPC_I2SI_CONFIG,
    ENUM_KERNEL_RPC_SPDIFI_CONFIG,
#endif // AUDIO_TV_PLATFORM
    ENUM_KERNEL_RPC_HDMI_OUT_VSDB,
    ENUM_VIDEO_KERNEL_RPC_CONFIG_TV_SYSTEM,
    ENUM_VIDEO_KERNEL_RPC_CONFIG_HDMI_INFO_FRAME,
    ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_WIN,
    ENUM_VIDEO_KERNEL_RPC_PP_INIT_PIN,
    ENUM_KERNEL_RPC_INIT_RINGBUF_AO, //need check this enum
    ENUM_VIDEO_KERNEL_RPC_VOUT_EDID_DATA,
    ENUM_KERNEL_RPC_AUDIO_POWER_SET,
    ENUM_VIDEO_KERNEL_RPC_VOUT_VDAC_SET,
    ENUM_VIDEO_KERNEL_RPC_QUERY_CONFIG_TV_SYSTEM,
    ENUM_KERNEL_RPC_AUDIO_CONFIG,
    ENUM_KERNEL_RPC_AIO_PRIVATEINFO,
    ENUM_KERNEL_RPC_QUERY_FW_DEBUG_INFO,
    ENUM_KERNEL_RPC_HDMI_RX_LATENCY_MEM,
    ENUM_KERNEL_RPC_EQ_CONFIG,
    ENUM_VIDEO_KERNEL_RPC_CREATE,
    ENUM_VIDEO_KERNEL_RPC_DISPLAY,
    ENUM_VIDEO_KERNEL_RPC_CONFIGUREDISPLAYWINDOW,
    ENUM_VIDEO_KERNEL_RPC_SETREFCLOCK,
    ENUM_VIDEO_KERNEL_RPC_RUN,
    ENUM_VIDEO_KERNEL_RPC_INITRINGBUFFER,
    ENUM_VIDEO_KERNEL_RPC_SETRESCALEMODE
} ENUM_FB_KERNEL_RPC_CMD;

enum VIDEO_VF_TYPE {
    VF_TYPE_VIDEO_MPEG2_DECODER = 0,
    VF_TYPE_VIDEO_MPEG4_DECODER = 1,
    VF_TYPE_VIDEO_DIVX_DECODER = 2,
    VF_TYPE_VIDEO_H263_DECODER = 3,
    VF_TYPE_VIDEO_H264_DECODER = 4,
    VF_TYPE_VIDEO_VC1_DECODER = 5,
    VF_TYPE_VIDEO_REAL_DECODER = 6,
    VF_TYPE_VIDEO_JPEG_DECODER = 7,
    VF_TYPE_VIDEO_MJPEG_DECODER = 8,
    VF_TYPE_SPU_DECODER = 9,
    VF_TYPE_VIDEO_OUT = 10,
    VF_TYPE_TRANSITION = 11,
    VF_TYPE_THUMBNAIL = 12,
    VF_TYPE_VIDEO_VP6_DECODER = 13,
    VF_TYPE_VIDEO_IMAGE_DECODER = 14,
    VF_TYPE_FLASH = 15,
    VF_TYPE_VIDEO_AVS_DECODER = 16,
    VF_TYPE_MIXER = 17,
    VF_TYPE_VIDEO_VP8_DECODER = 18,
    VF_TYPE_VIDEO_WMV7_DECODER = 19,
    VF_TYPE_VIDEO_WMV8_DECODER = 20,
    VF_TYPE_VIDEO_RAW_DECODER = 21,
    VF_TYPE_VIDEO_THEORA_DECODER = 22,
    VF_TYPE_VIDEO_FJPEG_DECODER = 23,
    VF_TYPE_VIDEO_H265_DECODER = 24,
    VF_TYPE_VIDEO_VP9_DECODER = 25,
};
typedef enum VIDEO_VF_TYPE VIDEO_VF_TYPE;

enum VO_VIDEO_PLANE {
    VO_VIDEO_PLANE_V1 = 0,
    VO_VIDEO_PLANE_V2 = 1,
    VO_VIDEO_PLANE_SUB1 = 2,
    VO_VIDEO_PLANE_OSD1 = 3,
    VO_VIDEO_PLANE_OSD2 = 4,
    VO_VIDEO_PLANE_WIN1 = 5,
    VO_VIDEO_PLANE_WIN2 = 6,
    VO_VIDEO_PLANE_WIN3 = 7,
    VO_VIDEO_PLANE_WIN4 = 8,
    VO_VIDEO_PLANE_WIN5 = 9,
    VO_VIDEO_PLANE_WIN6 = 10,
    VO_VIDEO_PLANE_WIN7 = 11,
    VO_VIDEO_PLANE_WIN8 = 12,
    VO_VIDEO_PLANE_SUB2 = 13,
    VO_VIDEO_PLANE_CSR = 14,
    VO_VIDEO_PLANE_NONE = 255,
};
typedef enum VO_VIDEO_PLANE VO_VIDEO_PLANE;

enum VO_OSD_COLOR_FORMAT {
    VO_OSD_COLOR_FORMAT_2BIT = 0,
    VO_OSD_COLOR_FORMAT_4BIT = 1,
    VO_OSD_COLOR_FORMAT_8BIT = 2,
    VO_OSD_COLOR_FORMAT_RGB332 = 3,
    VO_OSD_COLOR_FORMAT_RGB565 = 4,
    VO_OSD_COLOR_FORMAT_ARGB1555 = 5,
    VO_OSD_COLOR_FORMAT_ARGB4444 = 6,
    VO_OSD_COLOR_FORMAT_ARGB8888 = 7,
    VO_OSD_COLOR_FORMAT_Reserved0 = 8,
    VO_OSD_COLOR_FORMAT_Reserved1 = 9,
    VO_OSD_COLOR_FORMAT_Reserved2 = 10,
    VO_OSD_COLOR_FORMAT_YCBCRA4444 = 11,
    VO_OSD_COLOR_FORMAT_YCBCRA8888 = 12,
    VO_OSD_COLOR_FORMAT_RGBA5551 = 13,
    VO_OSD_COLOR_FORMAT_RGBA4444 = 14,
    VO_OSD_COLOR_FORMAT_RGBA8888 = 15,
    VO_OSD_COLOR_FORMAT_420 = 16,
    VO_OSD_COLOR_FORMAT_422 = 17,
    VO_OSD_COLOR_FORMAT_RGB323 = 18,
    VO_OSD_COLOR_FORMAT_RGB233 = 19,
    VO_OSD_COLOR_FORMAT_RGB556 = 20,
    VO_OSD_COLOR_FORMAT_RGB655 = 21,
    VO_OSD_COLOR_FORMAT_RGB888 = 22,
    VO_OSD_COLOR_FORMAT_RGB565_LITTLE = 36,
    VO_OSD_COLOR_FORMAT_ARGB1555_LITTLE = 37,
    VO_OSD_COLOR_FORMAT_ARGB4444_LITTLE = 38,
    VO_OSD_COLOR_FORMAT_ARGB8888_LITTLE = 39,
    VO_OSD_COLOR_FORMAT_YCBCRA4444_LITTLE = 43,
    VO_OSD_COLOR_FORMAT_YCBCRA8888_LITTLE = 44,
    VO_OSD_COLOR_FORMAT_RGBA5551_LITTLE = 45,
    VO_OSD_COLOR_FORMAT_RGBA4444_LITTLE = 46,
    VO_OSD_COLOR_FORMAT_RGBA8888_LITTLE = 47,
    VO_OSD_COLOR_FORMAT_RGB556_LITTLE = 52,
    VO_OSD_COLOR_FORMAT_RGB655_LITTLE = 53,
    VO_OSD_COLOR_FORMAT_RGB888_LITTLE = 54,
};

enum VO_OSD_RGB_ORDER {
    VO_OSD_COLOR_RGB = 0,
    VO_OSD_COLOR_BGR = 1,
    VO_OSD_COLOR_GRB = 2,
    VO_OSD_COLOR_GBR = 3,
    VO_OSD_COLOR_RBG = 4,
    VO_OSD_COLOR_BRG = 5,
};

enum VO_3D_MODE_TYPE {
    VO_2D_MODE = 0,
    VO_3D_SIDE_BY_SIDE_HALF = 1,
    VO_3D_TOP_AND_BOTTOM = 2,
    VO_3D_FRAME_PACKING = 3,
};

struct VO_SIZE {
    u_short w;
    u_short h;
};

enum VO_STANDARD {
    VO_STANDARD_NTSC_M = 0,
    VO_STANDARD_NTSC_J = 1,
    VO_STANDARD_NTSC_443 = 2,
    VO_STANDARD_PAL_B = 3,
    VO_STANDARD_PAL_D = 4,
    VO_STANDARD_PAL_G = 5,
    VO_STANDARD_PAL_H = 6,
    VO_STANDARD_PAL_I = 7,
    VO_STANDARD_PAL_N = 8,
    VO_STANDARD_PAL_NC = 9,
    VO_STANDARD_PAL_M = 10,
    VO_STANDARD_PAL_60 = 11,
    VO_STANDARD_SECAM = 12,
    VO_STANDARD_HDTV_720P_60 = 13,
    VO_STANDARD_HDTV_720P_50 = 14,
    VO_STANDARD_HDTV_720P_30 = 15,
    VO_STANDARD_HDTV_720P_25 = 16,
    VO_STANDARD_HDTV_720P_24 = 17,
    VO_STANDARD_HDTV_1080I_60 = 18,
    VO_STANDARD_HDTV_1080I_50 = 19,
    VO_STANDARD_HDTV_1080P_30 = 20,
    VO_STANDARD_HDTV_1080P_25 = 21,
    VO_STANDARD_HDTV_1080P_24 = 22,
    VO_STANDARD_VGA = 23,
    VO_STANDARD_SVGA = 24,
    VO_STANDARD_HDTV_1080P_60 = 25,
    VO_STANDARD_HDTV_1080P_50 = 26,
    VO_STANDARD_HDTV_1080I_59 = 27,
    VO_STANDARD_HDTV_720P_59 = 28,
    VO_STANDARD_HDTV_1080P_23 = 29,
    VO_STANDARD_HDTV_1080P_59 = 30,
    VO_STANDARD_HDTV_1080P_60_3D = 31,
    VO_STANDARD_HDTV_1080P_50_3D = 32,
    VO_STANDARD_HDTV_1080P_30_3D = 33,
    VO_STANDARD_HDTV_1080P_24_3D = 34,
    VO_STANDARD_HDTV_720P_60_3D = 35,
    VO_STANDARD_HDTV_720P_50_3D = 36,
    VO_STANDARD_HDTV_720P_30_3D = 37,
    VO_STANDARD_HDTV_720P_24_3D = 38,
    VO_STANDARD_HDTV_720P_59_3D = 39,
    VO_STANDARD_HDTV_1080I_60_3D = 40,
    VO_STANDARD_HDTV_1080I_59_3D = 41,
    VO_STANDARD_HDTV_1080I_50_3D = 42,
    VO_STANDARD_HDTV_1080P_23_3D = 43,
    VO_STANDARD_HDTV_2160P_30 = 44,
    VO_STANDARD_HDTV_2160P_29 = 45,
    VO_STANDARD_HDTV_2160P_25 = 46,
    VO_STANDARD_HDTV_2160P_24 = 47,
    VO_STANDARD_HDTV_2160P_23 = 48,
    VO_STANDARD_HDTV_4096_2160P_24 = 49,
    VO_STANDARD_HDTV_2160P_60 = 50,
    VO_STANDARD_HDTV_2160P_50 = 51,
    VO_STANDARD_HDTV_4096_2160P_25 = 52,
    VO_STANDARD_HDTV_4096_2160P_30 = 53,
    VO_STANDARD_HDTV_4096_2160P_50 = 54,
    VO_STANDARD_HDTV_4096_2160P_60 = 55,
    VO_STANDARD_HDTV_2160P_60_420 = 56,
    VO_STANDARD_HDTV_2160P_50_420 = 57,
    VO_STANDARD_HDTV_4096_2160P_60_420 = 58,
    VO_STANDARD_HDTV_4096_2160P_50_420 = 59,
    VO_STANDARD_DP_FORMAT_1920_1080P_60 = 60,
    VO_STANDARD_DP_FORMAT_2160P_30 = 61,
    VO_STANDARD_HDTV_2160P_24_3D = 62,
    VO_STANDARD_HDTV_2160P_23_3D = 63,
    VO_STANDARD_HDTV_2160P_59 = 64,
    VO_STANDARD_HDTV_2160P_59_420 = 65,
    VO_STANDARD_HDTV_2160P_25_3D = 66,
    VO_STANDARD_HDTV_2160P_30_3D = 67,
    VO_STANDARD_HDTV_2160P_50_3D = 68,
    VO_STANDARD_HDTV_2160P_60_3D = 69,
    VO_STANDARD_HDTV_4096_2160P_24_3D = 70,
    VO_STANDARD_HDTV_4096_2160P_25_3D = 71,
    VO_STANDARD_HDTV_4096_2160P_30_3D = 72,
    VO_STANDARD_HDTV_4096_2160P_50_3D = 73,
    VO_STANDARD_HDTV_4096_2160P_60_3D = 74,
    VO_STANDARD_DP_FORMAT_1280_720P_60 = 75,
    VO_STANDARD_DP_FORMAT_3840_2160P_60 = 76,
    VO_STANDARD_DP_FORMAT_1024_768P_60 = 77,
    VO_STANDARD_HDTV_2160P_50_422_12bit = 78,
    VO_STANDARD_HDTV_2160P_60_422_12bit = 79,
    VO_STANDARD_DP_FORMAT_1280_800P_60 = 80,
    VO_STANDARD_DP_FORMAT_1440_900P_60 = 81,
    VO_STANDARD_DP_FORMAT_1440_768P_60 = 82,
    VO_STANDARD_DP_FORMAT_960_544P_60 = 83,
    VO_STANDARD_HDTV_720P_120_3D = 84,
    VO_STANDARD_DP_FORMAT_800_480P_60 = 85,
    VO_STANDARD_HDTV_2160P_59_422_12bit = 86,
    VO_STANDARD_DP_FORMAT_800_1280P_60 = 87,
    VO_STANDARD_DP_FORMAT_1280_720P_50 = 88,
    VO_STANDARD_HDTV_1080P_144 = 89,
    VO_STANDARD_ERROR = 90,
};

enum VO_RESCALE_MODE {
    VO_RESCALE_MODE_KEEP_AR_AUTO = 0,
    VO_RESCALE_MODE_KEEP_AR_LB_CNTR = 1,
    VO_RESCALE_MODE_KEEP_AR_LB_TOP = 2,
    VO_RESCALE_MODE_KEEP_AR_PS_CNTR = 3,
    VO_RESCALE_MODE_KEEP_AR_PS_AUTO = 4,
    VO_RESCALE_MODE_FULL_SCALE = 5,
    VO_RESCALE_MODE_USER_DEFINE = 6,
};
typedef enum VO_RESCALE_MODE VO_RESCALE_MODE;

////////////////////////////////

typedef uint32_t HRESULT;

typedef struct RPCRES_LONG {
    HRESULT result;
    int data;
}RPCRES_LONG;

typedef struct {
    uint32_t inst_id;
    RPCRES_LONG retval;
    HRESULT res;
} VIDEO_RPC_RUN_T;

struct VIDEO_RPC_INSTANCE {
    enum VIDEO_VF_TYPE type;
};
typedef struct VIDEO_RPC_INSTANCE VIDEO_RPC_INSTANCE;

typedef struct {
    VIDEO_RPC_INSTANCE info;
    RPCRES_LONG retval;
    HRESULT ret;
} RPC_CREATE_VIDEO_AGENT_T;

struct VIDEO_RPC_VO_FILTER_DISPLAY {
    u_int instanceID;
    enum VO_VIDEO_PLANE videoPlane;
    u_char zeroBuffer;
    u_char realTimeSrc;
};
typedef struct VIDEO_RPC_VO_FILTER_DISPLAY VIDEO_RPC_VO_FILTER_DISPLAY;

typedef struct {
    u_int instanceID;
    enum VO_VIDEO_PLANE videoPlane;
    u_char zeroBuffer;
    u_char realTimeSrc;
    RPCRES_LONG retval;
    HRESULT ret;
} VIDEO_RPC_VO_FILTER_DISPLAY_T;

struct VO_COLOR {
    u_char c1;
    u_char c2;
    u_char c3;
    u_char isRGB;
};
typedef struct VO_COLOR VO_COLOR;

struct VIDEO_RPC_VOUT_CONFIG_DISP_WIN {
    enum VO_VIDEO_PLANE videoPlane;
    struct VO_RECTANGLE videoWin;
    struct VO_RECTANGLE borderWin;
    struct VO_COLOR borderColor;
    u_char enBorder;
};
typedef struct VIDEO_RPC_VOUT_CONFIG_DISP_WIN VIDEO_RPC_VOUT_CONFIG_DISP_WIN;

struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN {
    enum VO_VIDEO_PLANE plane;
};
typedef struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN;

struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT {
    short result;
    enum VO_VIDEO_PLANE plane;
    u_short numWin;
    u_short zOrder;
    struct VO_RECTANGLE configWin;
    struct VO_RECTANGLE contentWin;
    short deintMode;
    u_short pitch;
    enum VO_OSD_COLOR_FORMAT colorType;
    enum VO_OSD_RGB_ORDER RGBOrder;
    enum VO_3D_MODE_TYPE format3D;
    struct VO_SIZE mix1_size;
    enum VO_STANDARD standard;
    u_char enProg;
    u_char reserved1;
    u_short reserved2;
    u_int reserved3;
};
typedef struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT;

typedef struct {
    enum VO_VIDEO_PLANE videoPlane;
    struct VO_RECTANGLE videoWin;
    struct VO_RECTANGLE borderWin;
    struct VO_COLOR borderColor;
    u_char enBorder;
    RPCRES_LONG retval;
    HRESULT ret;
} VIDEO_RPC_VOUT_CONFIG_DISP_WIN_T;

typedef struct {
    int instanceID;
    int pRefClock;
} VIDEO_RPC_REFCLOCK;

typedef struct {
    int instanceID;
    int pRefClock;
    RPCRES_LONG ret;
    HRESULT res;
} VIDEO_RPC_REFCLOCK_T;

typedef enum _tagAutoMasterState{
  AUTOMASTER_NOT_MASTER,
  AUTOMASTER_IS_MASTER
} AUTOMASTER_STATE;

typedef struct {
    u_int instanceID;
    u_int pinID;
    u_int readPtrIndex;
    u_int pRINGBUFF_HEADER;
}VIDEO_RPC_RINGBUFFER;

typedef struct {
#ifndef CONFIG_RTK_VO_ON_HIFI
    RPCRES_LONG ret;
    HRESULT         res;
#endif
    u_int instanceID;
    u_int pinID;
    u_int readPtrIndex;
    u_int pRINGBUFF_HEADER;
#ifdef CONFIG_RTK_VO_ON_HIFI
    RPCRES_LONG ret;
    HRESULT         res;
#endif
} VIDEO_RPC_RINGBUFFER_T;

struct VIDEO_RPC_VOUT_SET_RESCALE_MODE {
    enum VO_VIDEO_PLANE videoPlane;
    enum VO_RESCALE_MODE rescaleMode;
    struct VO_RECTANGLE rescaleWindow;
    u_char delayExec;
};
typedef struct VIDEO_RPC_VOUT_SET_RESCALE_MODE VIDEO_RPC_VOUT_SET_RESCALE_MODE;

typedef struct {
    enum VO_VIDEO_PLANE videoPlane;
    enum VO_RESCALE_MODE rescaleMode;
    struct VO_RECTANGLE rescaleWindow;
    u_char delayExec;
    RPCRES_LONG retval;
    HRESULT ret;
} VIDEO_RPC_VOUT_SET_RESCALE_MODE_T;

struct AUDIO_HDMI_OUT_EDID_DATA2 {
    u_int Version;
    u_int HDMI_output_enable;
    u_int EDID_DATA_addr;
};
typedef struct AUDIO_HDMI_OUT_EDID_DATA2 AUDIO_HDMI_OUT_EDID_DATA2;

typedef struct {
    u_int Version;
    u_int HDMI_output_enable;
    u_int EDID_DATA_addr;
    RPCRES_LONG retval;
    HRESULT ret;
} AUDIO_HDMI_OUT_EDID_DATA2_T;

///////////////////////////////


uint64_t htonll(long long val) {
    return (((long long) htonl(val)) << 32) + htonl(val >> 32);
}

static int send_rpc(struct rtk_fb *fb, int opt, uint32_t command, uint32_t param1, uint32_t param2, uint32_t *retval)
{
	int ret = 0;

#ifdef CONFIG_RTK_VO_ON_HIFI
	ret = send_rpc_command(RPC_HIFI, command, param1, param2, retval);
#else
	ret = send_rpc_command(opt, command, param1, param2, retval);
#endif

	return ret;
}


static void rtk_rpc_free(struct dma_buf_attachment *attach,
		 struct sg_table *table)
{
	struct dma_buf *dmabuf;

	pr_info("%s free dmabuf\n", __func__);
	dmabuf = attach->dmabuf;
	BUG_ON(!dmabuf);
	BUG_ON(!dmabuf->ops);
	BUG_ON(!dmabuf->ops->end_cpu_access);

	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}


static struct dma_buf *rtk_rpc_alloc(struct device *dev, size_t size,
			 struct dma_buf_attachment **attach,
			 struct sg_table **table,
			 void **ion_phy, void **ion_virt)
{
	unsigned int alloc_flags, acc = 0;
	struct dma_buf *dmabuf, *ret;

#ifdef CONFIG_RTK_VO_ON_HIFI
	alloc_flags = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			 RTK_FLAG_HIFIACC;
#else
	alloc_flags = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			 RTK_FLAG_ACPUACC;
#endif
	dmabuf = rheap_alloc("rtk_audio_heap", size, alloc_flags);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(dev, "Failed to dma_buf_get\n");
		ret = dmabuf;
		goto rheap_err;
	}

	dma_buf_set_name(dmabuf, __func__);

	*attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(*attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		ret = ERR_CAST(*attach);
		goto attach_err;
	}

	*table = dma_buf_map_attachment(*attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(*table)) {
		dev_err(dev, "Failed to map attachment \n");
		ret = ERR_CAST(*table);
		goto map_err;
	}

	acc = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (acc) {
		ret = ERR_PTR(acc);
		goto access_err;
	}

	*ion_virt = dma_buf_vmap(dmabuf);
	if (!*ion_virt) {
		dev_err(dev, "dma_buf_vmap failed\n");
		ret = ERR_PTR(-ENOMEM);
		goto kmap_err;
	}

	/*or using sg_dma_address(table->sgl)) ?*/
	*ion_phy = (void *)sg_phys((*table)->sgl);

	dev_info(dev, "%s *ion_phy = 0x%lx , *ion_virt = 0x%lx", __func__,
			 (unsigned long)*ion_phy, (unsigned long)*ion_virt);
	return dmabuf;
kmap_err:
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
access_err:
	dma_buf_unmap_attachment(*attach, *table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(dmabuf, *attach);
attach_err:
	dma_buf_put(dmabuf);
rheap_err:

	return ret;
}


int RPC_TOAGENT_CREATE_VIDEO_AGENT(struct rtk_fb *fb, uint32_t* videoId,
					 VIDEO_VF_TYPE pinId)
{
	RPC_CREATE_VIDEO_AGENT_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat = 0;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;
	unsigned int offset;
#ifdef CONFIG_RTK_VO_ON_HIFI
	RPCRES_LONG *retval;
#endif

	TRACE_RPC();
	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
				 (void **)&rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	rpc->info.type = pinId;
	offset = ALIGN(get_rpc_alignment_offset(sizeof(rpc->info)), RPC_ALIGN_SZ);
#else
	rpc->info.type = htonl(pinId);
	offset = get_rpc_alignment_offset(sizeof(rpc->info));
#endif
	if (send_rpc(fb, RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_CREATE,
		CONVERT_FOR_AVCPU(dat), //rpc->info address
		CONVERT_FOR_AVCPU(dat + offset),//rpc->retval address
		&rpc->ret)) {
		dev_err(dev, "[RTKFB %s %d RPC fail]\n",__FUNCTION__, __LINE__);
		goto exit;
	}
#ifdef CONFIG_RTK_VO_ON_HIFI
	retval = (RPCRES_LONG *)((unsigned char *)rpc + offset);
	if (retval->result != S_OK || rpc->ret != S_OK) {
#else
	if (ntohl(rpc->retval.result) != S_OK || rpc->ret != S_OK) {
#endif
		 dev_err(dev, "[RTKFB %x %x %s %d RPC fail]\n",
		 rpc->retval.result, rpc->ret, __FUNCTION__, __LINE__);
		goto exit;
	}
#ifdef CONFIG_RTK_VO_ON_HIFI
	*videoId = retval->data;
#else
	*videoId = ntohl(rpc->retval.data);
#endif
	ret = 0;

exit:
	dma_buf_vunmap(dmabuf, rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;

}


int RPC_TOAGENT_VIDEO_DISPLAY(struct rtk_fb *fb,
			VIDEO_RPC_VO_FILTER_DISPLAY* argp)
{
	VIDEO_RPC_VO_FILTER_DISPLAY_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;
	unsigned int offset;
#ifdef CONFIG_RTK_VO_ON_HIFI
	RPCRES_LONG *retval;
#endif

	TRACE_RPC();

	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
				 (void **)&rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	memcpy(rpc, argp, sizeof(*argp));
	offset = ALIGN(get_rpc_alignment_offset(sizeof(VIDEO_RPC_VO_FILTER_DISPLAY)), RPC_ALIGN_SZ);
#else
	rpc->instanceID = htonl(argp->instanceID);
	rpc->videoPlane = htonl(argp->videoPlane);
	rpc->zeroBuffer = htonl(argp->zeroBuffer);
	rpc->realTimeSrc = htonl(argp->realTimeSrc);
	offset = get_rpc_alignment_offset(sizeof(VIDEO_RPC_VO_FILTER_DISPLAY));
#endif
	if (send_rpc(fb, RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_DISPLAY,
		CONVERT_FOR_AVCPU(dat), //rpc->info address
		CONVERT_FOR_AVCPU(dat + offset),
		//rpc->retval address
		&rpc->ret)) {
		dbg_warn("[RTKFB %s %d RPC fail]\n",__FUNCTION__, __LINE__);
		goto exit;
	}
#ifdef CONFIG_RTK_VO_ON_HIFI
	retval = (RPCRES_LONG *)((unsigned char *)rpc + offset);
	if (retval->result != S_OK || rpc->ret != S_OK) {
#else
	if (ntohl(rpc->retval.result) != S_OK || rpc->ret != S_OK) {
#endif
		dbg_warn("[RTKFB %x %x %s %d RPC fail]\n", rpc->retval.result,
			 rpc->ret, __FUNCTION__, __LINE__);
		goto exit;
	}
	ret = 0;
exit:
	dma_buf_vunmap(dmabuf, rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;
}

int RPC_TOAGENT_QUERYDISPLAYWIN_0(struct rtk_fb *fb,
			struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT *arg)
{
	struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN *i_rpc = NULL;
	struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT *o_rpc = NULL;
	int ret = -1;
	u32 RPC_ret;
	phys_addr_t dat;
	unsigned int offset;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;

	TRACE_RPC();

	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
				 (void **)&i_rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	offset = ALIGN(get_rpc_alignment_offset(
		 sizeof(struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN)), RPC_ALIGN_SZ);
#else
	offset = get_rpc_alignment_offset(
		 sizeof(struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN));
#endif
	o_rpc = (struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT *)
			((unsigned long)i_rpc + offset);

	if (send_rpc(fb, RPC_AUDIO, ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_WIN,
		CONVERT_FOR_AVCPU(dat),
		CONVERT_FOR_AVCPU(dat + offset),
		&RPC_ret)) {
		dbg_warn("[%s %d RPC fail]", __func__, __LINE__);
		goto exit;
	}

	if (RPC_ret != S_OK) {
		dbg_warn("[%s %d RPC fail]", __func__, __LINE__);
		goto exit;
	}
		//arg->standard = htonl(o_rpc->standard);
#ifdef CONFIG_RTK_VO_ON_HIFI
	arg->mix1_size.w = o_rpc->mix1_size.w;
	arg->mix1_size.h = o_rpc->mix1_size.h;
#else
	arg->mix1_size.w = htons(o_rpc->mix1_size.w);
	arg->mix1_size.h = htons(o_rpc->mix1_size.h);
#endif
	//dbg_warn("%s %d standard %d\n", __func__, __LINE__, arg->standard);
	dbg_warn("%s %d mix1_size w %d h %d\n", __func__, __LINE__,
		 arg->mix1_size.w, arg->mix1_size.h);
	ret = 0;

exit:
	dma_buf_vunmap(dmabuf, i_rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;
}

int RPC_TOAGENT_VIDEO_CONFIGUREDISPLAYWINDOW(struct rtk_fb *fb,
			VIDEO_RPC_VOUT_CONFIG_DISP_WIN* argp)
{
	VIDEO_RPC_VOUT_CONFIG_DISP_WIN_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;
	unsigned int offset;
#ifdef CONFIG_RTK_VO_ON_HIFI
	RPCRES_LONG *retval;
#endif

	TRACE_RPC();

	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
				 (void **)&rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	memcpy(rpc, argp, sizeof(*argp));
	offset = ALIGN(get_rpc_alignment_offset(sizeof(VIDEO_RPC_VOUT_CONFIG_DISP_WIN)), RPC_ALIGN_SZ);
#else
	rpc->videoPlane = htonl(argp->videoPlane);
	rpc->videoWin.x = htons(argp->videoWin.x);
	rpc->videoWin.y = htons(argp->videoWin.y);
	rpc->videoWin.width = htons(argp->videoWin.width);
	rpc->videoWin.height = htons(argp->videoWin.height);
	rpc->borderWin.x = htons(argp->borderWin.x);
	rpc->borderWin.y = htons(argp->borderWin.y);
	rpc->borderWin.width = htons(argp->borderWin.width);
	rpc->borderWin.height = htons(argp->borderWin.height);
	rpc->borderColor.c1 = htonl(argp->borderColor.c1);
	rpc->borderColor.c2 = htonl(argp->borderColor.c2);
	rpc->borderColor.c3 = htonl(argp->borderColor.c3);
	rpc->borderColor.isRGB = htonl(argp->borderColor.isRGB);
	rpc->enBorder = htonl(argp->enBorder);
	offset = get_rpc_alignment_offset(sizeof(VIDEO_RPC_VOUT_CONFIG_DISP_WIN));
#endif
	if (send_rpc(fb, RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_CONFIGUREDISPLAYWINDOW,
		CONVERT_FOR_AVCPU(dat), //rpc->info address
		CONVERT_FOR_AVCPU(dat + offset),//rpc->retval address
		//rpc->retval address
		&rpc->ret)) {
		dbg_warn("[RTKFB %s %d RPC fail]\n",__FUNCTION__, __LINE__);
		goto exit;
	}

#ifdef CONFIG_RTK_VO_ON_HIFI
	retval = (RPCRES_LONG *)((unsigned char *)rpc + offset);
	if (retval->result != S_OK || rpc->ret != S_OK) {
#else
	if (ntohl(rpc->retval.result) != S_OK || rpc->ret != S_OK) {
#endif
		dbg_warn("[RTKFB %x %x %s %d RPC fail]\n", rpc->retval.result,
		rpc->ret, __FUNCTION__, __LINE__);
		goto exit;
	}

	ret = 0;
exit:
	dma_buf_vunmap(dmabuf, rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;

}


int RPC_TOAGENT_VIDEO_SETREFCLOCK(struct rtk_fb *fb,
				VIDEO_RPC_REFCLOCK *pClock)
{

	VIDEO_RPC_REFCLOCK_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;
	unsigned int offset;
#ifdef CONFIG_RTK_VO_ON_HIFI
	RPCRES_LONG *retval;
#endif

	TRACE_RPC();

	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
				 (void **)&rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	rpc->instanceID = pClock->instanceID;
	rpc->pRefClock = pClock->pRefClock;
	offset = ALIGN(get_rpc_alignment_offset(sizeof(rpc->instanceID) + sizeof(rpc->pRefClock)), RPC_ALIGN_SZ);
#else
	rpc->instanceID = htonl(pClock->instanceID);
	rpc->pRefClock = htonl(pClock->pRefClock);
	offset = get_rpc_alignment_offset(sizeof(rpc->instanceID) + sizeof(rpc->pRefClock));
#endif
	if (send_rpc(fb, RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_SETREFCLOCK,
		CONVERT_FOR_AVCPU(dat), //rpc->out address
		CONVERT_FOR_AVCPU(dat + offset),//rpc->retval address
		&rpc->res)) {
		dbg_warn("[%s RPC fail %d]\n", __FUNCTION__, __LINE__);
		goto exit;
	}

#ifdef CONFIG_RTK_VO_ON_HIFI
	retval = (RPCRES_LONG *)((unsigned char *)rpc + offset);
	if (retval->result != S_OK || rpc->res != S_OK) {
#else
	if (rpc->res != S_OK || ntohl(rpc->ret.result) != S_OK) {
#endif
		dbg_warn("[%s RPC fail %d]\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	ret = 0;
exit:
	dma_buf_vunmap(dmabuf, rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;

}

int RPC_TOAGENT_VIDEO_RUN(struct rtk_fb *fb, uint32_t instance_id)
{
	VIDEO_RPC_RUN_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;
	unsigned int offset;
#ifdef CONFIG_RTK_VO_ON_HIFI
	RPCRES_LONG *retval;
#endif

	TRACE_RPC();

	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
				 (void **)&rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	rpc->inst_id = instance_id;
	offset = ALIGN(get_rpc_alignment_offset(sizeof(rpc->inst_id)), RPC_ALIGN_SZ);
#else
	rpc->inst_id = htonl(instance_id);
	offset = get_rpc_alignment_offset(sizeof(rpc->inst_id));
#endif
	if (send_rpc(fb, RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_RUN,
		CONVERT_FOR_AVCPU(dat), //rpc->inst_id address
		CONVERT_FOR_AVCPU(dat + offset),//rpc->retval address
		&rpc->res)) {
		dbg_warn("[%s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}
#ifdef CONFIG_RTK_VO_ON_HIFI
	retval = (RPCRES_LONG *)((unsigned char *)rpc + offset);
	if (retval->result != S_OK || rpc->res != S_OK) {
#else
	if (rpc->res != S_OK || ntohl(rpc->retval.result) != S_OK) {
#endif
		dbg_warn("[%s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}

	ret = 0;
exit:
	dma_buf_vunmap(dmabuf, rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;
}

int RPC_TOAGENT_VIDEO_INITRINGBUFFER(struct rtk_fb *fb,
				VIDEO_RPC_RINGBUFFER* argp)
{
	VIDEO_RPC_RINGBUFFER_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;
	unsigned int offset;
#ifdef CONFIG_RTK_VO_ON_HIFI
	RPCRES_LONG *retval;
#endif

	TRACE_RPC();

	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
				 (void **)&rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	rpc->instanceID = argp->instanceID;
	rpc->readPtrIndex = argp->readPtrIndex;
	rpc->pinID = argp->pinID;
	rpc->pRINGBUFF_HEADER = argp->pRINGBUFF_HEADER;
	offset = ALIGN(get_rpc_alignment_offset(sizeof(VIDEO_RPC_RINGBUFFER)), RPC_ALIGN_SZ);
#else
	rpc->instanceID = htonl(argp->instanceID);
	rpc->readPtrIndex = htonl(argp->readPtrIndex);
	rpc->pinID = htonl(argp->pinID);
	rpc->pRINGBUFF_HEADER = htonl(argp->pRINGBUFF_HEADER);
	offset = get_rpc_alignment_offset(sizeof(rpc->ret) + sizeof(rpc->res));
#endif
	if (send_rpc(fb, RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_INITRINGBUFFER,
#ifdef CONFIG_RTK_VO_ON_HIFI
		CONVERT_FOR_AVCPU(dat), //rpc->info address
		CONVERT_FOR_AVCPU(dat + offset),//rpc->retval address
#else
		CONVERT_FOR_AVCPU(dat + get_rpc_alignment_offset(
			sizeof(rpc->ret) + sizeof(rpc->res))),
			//rpc->header address
		CONVERT_FOR_AVCPU(dat), //rpc->ret address
#endif
		&rpc->res)) {
		dbg_warn("[%s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}
#ifdef CONFIG_RTK_VO_ON_HIFI
	retval = (RPCRES_LONG *)((unsigned char *)rpc + offset);
	if (retval->result != S_OK || rpc->res != S_OK) {
#else
	if (rpc->res != S_OK || ntohl(rpc->ret.result) != S_OK) {
#endif
		dbg_warn("[%s %d RPC fail]\n", __FUNCTION__, __LINE__);
		goto exit;
	}
	ret = 0;

exit:
	dma_buf_vunmap(dmabuf, rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;

}


int RPC_TOAGENT_VIDEO_SETRESCALEMODE(struct rtk_fb *fb,
				 VIDEO_RPC_VOUT_SET_RESCALE_MODE* argp)
{
	VIDEO_RPC_VOUT_SET_RESCALE_MODE_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;
	unsigned int offset;
#ifdef CONFIG_RTK_VO_ON_HIFI
	RPCRES_LONG *retval;
#endif

	TRACE_RPC();

	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
				 (void **)&rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	memcpy(rpc, argp, sizeof(*argp));
	offset = ALIGN(get_rpc_alignment_offset(sizeof(VIDEO_RPC_VOUT_SET_RESCALE_MODE)), RPC_ALIGN_SZ);
#else
	rpc->videoPlane = htonl(argp->videoPlane);
	rpc->rescaleMode = htonl(argp->rescaleMode);
	rpc->rescaleWindow.x = htonl(argp->rescaleWindow.x);
	rpc->rescaleWindow.y = htonl(argp->rescaleWindow.y);
	rpc->rescaleWindow.width = htonl(argp->rescaleWindow.width);
	rpc->rescaleWindow.height = htonl(argp->rescaleWindow.height);
	rpc->delayExec = htonl(argp->delayExec);
	offset = get_rpc_alignment_offset(sizeof(VIDEO_RPC_VOUT_SET_RESCALE_MODE));
#endif
	if (send_rpc(fb, RPC_AUDIO,
		ENUM_VIDEO_KERNEL_RPC_SETRESCALEMODE,
		CONVERT_FOR_AVCPU(dat), //rpc->info address
		CONVERT_FOR_AVCPU(dat + offset),//rpc->retval address
		//rpc->retval address
		&rpc->ret)) {
		dbg_warn("[RTKFB %s %d RPC fail]\n",__FUNCTION__, __LINE__);
		goto exit;
	}
#ifdef CONFIG_RTK_VO_ON_HIFI
	retval = (RPCRES_LONG *)((unsigned char *)rpc + offset);
	if (retval->result != S_OK || rpc->ret != S_OK) {
#else
	if (ntohl(rpc->retval.result) != S_OK || rpc->ret != S_OK) {
#endif
		dbg_warn("[RTKFB %x %x %s %d RPC fail]\n",
		 rpc->retval.result, rpc->ret, __FUNCTION__, __LINE__);
		goto exit;
	}

	ret = 0;
exit:
	dma_buf_vunmap(dmabuf, rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;

}

int RPC_ToAgent_AUDIO_HDMI_OUT_EDID_0(struct rtk_fb *fb,
			 struct AUDIO_HDMI_OUT_EDID_DATA2 *argp)
{
	AUDIO_HDMI_OUT_EDID_DATA2_T *rpc = NULL;
	int ret = -1;
	phys_addr_t dat;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = fb->dev;

	TRACE_RPC();

	dmabuf = rtk_rpc_alloc(dev, 4096, &attach, &table, (void **)&dat,
			 (void **)&rpc);
	if (IS_ERR(dmabuf))
		goto alloc_err;
#ifdef CONFIG_RTK_VO_ON_HIFI
	rpc->Version = argp->Version;
	rpc->HDMI_output_enable = argp->HDMI_output_enable;
	rpc->EDID_DATA_addr = argp->EDID_DATA_addr;
#else
	rpc->Version = htonl(argp->Version);
	rpc->HDMI_output_enable = htonl(argp->HDMI_output_enable);
	rpc->EDID_DATA_addr = htonl(argp->EDID_DATA_addr);
#endif
	if (send_rpc(fb, RPC_AUDIO,
		ENUM_KERNEL_RPC_HDMI_OUT_EDID2,
		CONVERT_FOR_AVCPU(dat),
#ifdef CONFIG_RTK_VO_ON_HIFI
		CONVERT_FOR_AVCPU(dat + ALIGN(get_rpc_alignment_offset(
			sizeof(AUDIO_HDMI_OUT_EDID_DATA2)), RPC_ALIGN_SZ)),
#else
		CONVERT_FOR_AVCPU(dat + get_rpc_alignment_offset(
			sizeof(AUDIO_HDMI_OUT_EDID_DATA2))),
#endif
		&rpc->ret)) {
		dbg_warn("[RTKFB %s %d RPC fail]", __func__, __LINE__);
		goto exit;
	}

	ret = 0;
exit:
	dma_buf_vunmap(dmabuf, rpc);
	rtk_rpc_free(attach, table);
alloc_err:
	return ret;

}

int SetAudioHdmiOutEdid2(struct rtk_fb *fb, unsigned char *ion_virt,
			 unsigned long ion_phy)
{
	struct AUDIO_HDMI_OUT_EDID_DATA2 audio_edid_data2;
	//unsigned char edid[HDMI_EDID_SIZE] = { 0x04, 0x09, 0x04, 0x07, 0x00 };
	audio_edid_data2.HDMI_output_enable = HDMI_OUT_EN;
	audio_edid_data2.Version = HDMI_VER;
	//memcpy((void*)ion_virt, edid, HDMI_EDID_SIZE);
	ion_virt[0]=0x04;
	ion_virt[1]=0x09;
	ion_virt[2]=0x04;
	ion_virt[3]=0x07;
	ion_virt[4]=0x00;
	audio_edid_data2.EDID_DATA_addr = (long)(0xffffffff&ion_phy);

	if (RPC_ToAgent_AUDIO_HDMI_OUT_EDID_0(fb, &audio_edid_data2) < 0) {
		dbg_warn("[%s %d fail]\n", __FUNCTION__, __LINE__);
		return -1;
	};

	return 0;

}

int RTK_FB_RPC_OSD_init(struct rtk_fb *fb)
{
	phys_addr_t ion_phy;
	void *ion_virt = NULL;
	int ret = -1;
	unsigned int osd_width = 1920, osd_height = 1080;
	VO_RECTANGLE rect;
	REFCLOCK* m_core;
	RINGBUFFER_HEADER *ringheader;
	uint32_t VIDEOAgentID = 0;
	VIDEO_VF_TYPE type;
	DCRT_PARAM_RATE_INFO rateParam;
	unsigned int hz=60;
	VIDEO_RPC_VO_FILTER_DISPLAY info;
	VO_COLOR blueBorder = {0,0,255,1};
	VIDEO_RPC_VOUT_CONFIG_DISP_WIN structConfigDispWin;
	VIDEO_RPC_REFCLOCK rpc_refclock;
	VIDEO_RPC_RINGBUFFER ringbuffer;
	DCRT_PARAM_RPC_VIRT_ADDR inbandParam_virt;
	DCRT_PARAM_RPC_ADDR inbandParam;
	VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT out;
	struct device *dev = fb->dev;
	struct dma_buf *dmabuf;

	/* OSD init */
	dev_err(dev, "OSD init,[%s %s][%d] \n", __FILE__, __func__, __LINE__);

	dmabuf = rtk_rpc_alloc(dev, 67*1024, &rtkfb_attach, &rtkfb_table,
				 (void **)&ion_phy, (void **)&ion_virt);
	if (IS_ERR(dmabuf))
		goto alloc_err;

	rtkfb_virt = ion_virt;

	memset_io(ion_virt, 0, 67*1024);

	/* gldc_osd_main */
	m_core = (REFCLOCK*)((unsigned long)(ion_virt)+(65*1024));
	ringheader = (RINGBUFFER_HEADER*)((unsigned long)(ion_virt)+(64*1024));

	osd_width = fb->fb.var.xres;
	osd_height = fb->fb.var.yres;

	rect.x = 0;
	rect.y = 0;
	rect.width = osd_width;
	rect.height = osd_height;

	/* VIDEO_RPC_ToAgent_Create_0 */
	type = VF_TYPE_VIDEO_OUT;
	if (RPC_TOAGENT_CREATE_VIDEO_AGENT(fb, &VIDEOAgentID, type))
	{
		dev_err(dev, "[No Video agent %s %d]\n", __FUNCTION__, __LINE__);
		goto fail;
	}

	/* DCRT_CMD_SET_OUT_RATE_INFO */
	memset(&rateParam, 0, sizeof(rateParam));
	rateParam.param_size = sizeof(rateParam);
	rateParam.pts_gap =  (unsigned int)90000/hz;
	rateParam.hz = hz*1000;
	rateParam.clockAddrLow = -1;
	rateParam.clockAddrHi = -1;

	if (DC_Set_RateInfo(&fb->fb, &fb->video_info, &rateParam) != 0){
		dev_err(dev, "[DC_Set_RateInfo fail %s %d]\n",
			 __FUNCTION__, __LINE__);
		goto fail;
	}

	/* VIDEO_RPC_VO_FILTER_ToAgent_Display_0 */
	memset(&info, 0, sizeof(info));
	info.instanceID = VIDEOAgentID;
	info.videoPlane = VO_VIDEO_PLANE_OSD1;
	info.zeroBuffer  = 0;
	info.realTimeSrc = 0;
	if (RPC_TOAGENT_VIDEO_DISPLAY(fb, &info))
	{
		dev_err(dev, "[RPC_TOAGENT_VIDEO_DISPLAY fail %s %d]\n",
			 __FUNCTION__, __LINE__);
		goto fail;
	}

	if (RPC_TOAGENT_QUERYDISPLAYWIN_0(fb, &out))
	{
		dev_err(dev, "[RPC_TOAGENT_QUERYDISPLAYWIN_0 fail %s %d]\n",
			 __FUNCTION__, __LINE__);
		goto fail;
	} else {
		rect.width = out.mix1_size.w;
		rect.height = out.mix1_size.h;
		//dbg_warn("width %d height %d\n", rect.width, rect.height);
	}

	/* VIDEO_RPC_VOUT_ToAgent_ConfigureDisplayWindow_0 */
	memset(&structConfigDispWin, 0, sizeof(structConfigDispWin));
	structConfigDispWin.videoPlane = VO_VIDEO_PLANE_OSD1;
	structConfigDispWin.videoWin = rect;
	structConfigDispWin.borderWin = rect;
	structConfigDispWin.borderColor = blueBorder;
	structConfigDispWin.enBorder = 0;
	if (RPC_TOAGENT_VIDEO_CONFIGUREDISPLAYWINDOW(fb, &structConfigDispWin))
	{
		dev_err(dev, "[RPC_TOAGENT_VIDEO_CONFIGUREDISPLAYWINDOW fail %s %d]\n",
			 __FUNCTION__, __LINE__);
		goto fail;
	}

	/* VIDEO_RPC_ToAgent_SetRefClock_0 */
#ifdef CONFIG_RTK_VO_ON_HIFI
	m_core->RCD = -1LL;
	m_core->RCD_ext = -1L;
	m_core->masterGPTS = -1LL;
	m_core->GPTSTimeout = 0LL;
	m_core->videoSystemPTS = -1LL;
	m_core->audioSystemPTS = -1LL;
	m_core->videoRPTS = -1LL;
	m_core->audioRPTS = -1LL;
	m_core->videoContext = -1;
	m_core->audioContext = -1;
	m_core->videoEndOfSegment = -1;
	m_core->videoFreeRunThreshold = 0x7FFFFFFF;
	m_core->audioFreeRunThreshold = 0x7FFFFFFF;
	m_core->VO_Underflow = 0;
	m_core->AO_Underflow = 0;
#else
	m_core->RCD = htonll(-1LL);
	m_core->RCD_ext = htonl(-1L);
	m_core->masterGPTS = htonll(-1LL);
	m_core->GPTSTimeout = htonll(0LL);
	m_core->videoSystemPTS = htonll(-1LL);
	m_core->audioSystemPTS = htonll(-1LL);
	m_core->videoRPTS = htonll(-1LL);
	m_core->audioRPTS = htonll(-1LL);
	m_core->videoContext = htonl(-1);
	m_core->audioContext = htonl(-1);
	m_core->videoEndOfSegment = htonl(-1);
	m_core->videoFreeRunThreshold = htonl(0x7FFFFFFF);
	m_core->audioFreeRunThreshold = htonl(0x7FFFFFFF);
	m_core->VO_Underflow = htonl(0);
	m_core->AO_Underflow = htonl(0);
#endif
	m_core->mastership.systemMode  = (unsigned char)AVSYNC_FORCED_SLAVE;
	m_core->mastership.videoMode   = (unsigned char)AVSYNC_FORCED_MASTER;
	m_core->mastership.audioMode   = (unsigned char)AVSYNC_FORCED_MASTER;
	m_core->mastership.masterState = (unsigned char)AUTOMASTER_NOT_MASTER;
	memset(&rpc_refclock, 0, sizeof(rpc_refclock));
	rpc_refclock.instanceID = VIDEOAgentID;
	rpc_refclock.pRefClock = (long)(0xffffffff&ion_phy)+65*1024;
	if (RPC_TOAGENT_VIDEO_SETREFCLOCK(fb, &rpc_refclock)) {
		dev_err(dev, "[RPC_TOAGENT_VIDEO_SETREFCLOCK fail %s %d]\n",
		 __FUNCTION__, __LINE__);
		goto fail;
	}

	/* VIDEO_RPC_ToAgent_Run_0 */
	if (RPC_TOAGENT_VIDEO_RUN(fb, VIDEOAgentID)){
		dev_err(dev, "[RPC_TOAGENT_VIDEO_RUN fail %s %d]\n",
			 __FUNCTION__, __LINE__);
		goto fail;
	}

	/*  VIDEO_RPC_ToAgent_InitRingBuffer_0 */
#ifdef CONFIG_RTK_VO_ON_HIFI
	ringheader->beginAddr = (long)(0xffffffff&ion_phy);
	ringheader->size = 64*1024;
#else
	ringheader->beginAddr = htonl((long)(0xffffffff&ion_phy));
	ringheader->size = htonl(64*1024);
#endif
	ringheader->writePtr = ringheader->beginAddr;
	ringheader->readPtr[0] = ringheader->beginAddr;
#ifdef CONFIG_RTK_VO_ON_HIFI
	ringheader->bufferID = 1;
#else
	ringheader->bufferID = htonl(1);
#endif
	memset(&ringbuffer, 0, sizeof(ringbuffer));
	ringbuffer.instanceID = VIDEOAgentID;
	ringbuffer.readPtrIndex = 0;
	ringbuffer.pinID = 0;
	ringbuffer.pRINGBUFF_HEADER = (long)(0xffffffff&ion_phy)+64*1024;
	if (RPC_TOAGENT_VIDEO_INITRINGBUFFER(fb, &ringbuffer) < 0) {
		dev_err(dev, "[RPC_TOAGENT_VIDEO_INITRINGBUFFER fail %s %d]\n",
			 __FUNCTION__, __LINE__);
		goto fail;
	};

	/* DC2VO_SET_ION_SHARE_MEMORY */
	memset(&inbandParam_virt, 0, sizeof(inbandParam_virt));
	inbandParam_virt.ringheaderbaseVirtAddr = (void *)(
				(unsigned long)(ion_virt));
	inbandParam_virt.ringVirtAddr = (void *)((unsigned long)
				(ion_virt)+(64*1024));
	inbandParam_virt.refclockVirtAddr = (void *)((unsigned long)
				(ion_virt)+(65*1024));
	inbandParam_virt.vo_instance_id = VIDEOAgentID;

	if (DC_Set_RPCAddr_Virt(&fb->fb, &fb->video_info, &inbandParam_virt)
				!= 0){
		dev_err(dev, "SET_RING_INFO_VIRT fail \n");
		goto fail;
	}

	/* DCRT_CMD_SET_RING_INFO DC2VO_SET_RING_INFO */
	memset(&inbandParam, 0, sizeof(inbandParam));
	inbandParam.ringPhyAddr = ringbuffer.pRINGBUFF_HEADER;
	inbandParam.refclockAddr = rpc_refclock.pRefClock;

	if (DC_Set_RPCAddr(&fb->fb, &fb->video_info, &inbandParam)!= 0){
		dev_err(dev, "SET_RING_INFO fail \n");
		goto fail;
	}

	/* VIDEO_RPC_VO_FILTER_ToAgent_Display_0 */
	memset(&info, 0, sizeof(info));
	info.instanceID = VIDEOAgentID;
	info.videoPlane = VO_VIDEO_PLANE_OSD1;
	info.zeroBuffer  = 1;
	info.realTimeSrc = 0;
	if (RPC_TOAGENT_VIDEO_DISPLAY(fb, &info))
	{
		dev_err(dev, "[%s %d]\n", __FUNCTION__, __LINE__);
		goto fail;
	}

	if (SetAudioHdmiOutEdid2(fb, (unsigned char*)((unsigned long)
			ion_virt+66*1024), ion_phy+66*1024)!= 0){
		dev_err(dev, "SetAudioHdmiOutEdid2 fail \n");
		goto fail;
	}

	/* Show buffer which preallocated in rtk frame buffer */
	if (dc_do_static_post_config(&fb->video_info)!= 0){
		dev_err(dev, "dc_do_static_post_config fail \n");
		goto fail;
	}
	ret = 0;
	dbg_warn("OSD init success,[%s %s][%d] \n", __FILE__, __func__,
			 __LINE__);

	return ret;

fail:
	dma_buf_vunmap(dmabuf, rtkfb_virt);
	rtk_rpc_free(rtkfb_attach, rtkfb_table);
alloc_err:
	dbg_warn("OSD init fail,[%s %s][%d] \n", __FILE__, __func__, __LINE__);
	return ret;
}


void RTK_FB_RPC_OSD_uninit(void)
{
	pr_err("OSD uninit,[%s %s][%d] \n", __FILE__, __func__, __LINE__);

	dma_buf_vunmap(rtkfb_attach->dmabuf, rtkfb_virt);
	rtk_rpc_free(rtkfb_attach, rtkfb_table);

}

MODULE_LICENSE("GPL v2");
