/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef RTK_DP_RX_H_
#define RTK_DP_RX_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/videodev2.h>
#include <linux/dma-map-ops.h>
#include <linux/mm_types.h>
#include <linux/fdtable.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/jiffies.h>
#include <linux/math64.h>

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>

#include <drm/display/drm_dp_helper.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/rtk_refclk.h>

#include "rtk_dprx_ip_reg.h"
#include "dprx14_pll_reg.h"
#include "dprx14_reg.h"
#include "dprx14_ddc_reg.h"
#include "rtk_dprx_link_training.h"

#define RTK_ONLY_FOR_TEST  0

#define _BIT0    0x01
#define _BIT1    0x02
#define _BIT2    0x04
#define _BIT3    0x08
#define _BIT4    0x10
#define _BIT5    0x20
#define _BIT6    0x40
#define _BIT7    0x80
#define _BIT8    0x00000100
#define _BIT9    0x00000200
#define _BIT10   0x00000400
#define _BIT11   0x00000800
#define _BIT12   0x00001000
#define _BIT13   0x00002000
#define _BIT14   0x00004000
#define _BIT15   0x00008000
#define _BIT16   0x00010000
#define _BIT17   0x00020000
#define _BIT18   0x00040000
#define _BIT19   0x00080000
#define _BIT20   0x00100000
#define _BIT21   0x00200000
#define _BIT22   0x00400000
#define _BIT23   0x00800000
#define _BIT24   0x01000000
#define _BIT25   0x02000000
#define _BIT26   0x04000000
#define _BIT27   0x08000000
#define _BIT28   0x10000000
#define _BIT29   0x20000000
#define _BIT30   0x40000000
#define _BIT31   0x80000000


#define DMA_ENTRY_0  0
#define DMA_ENTRY_1  1
#define DMA_ENTRY_2  2
#define DMA_ENTRY_3  3

#define DISABLE 0
#define ENABLE  1

#define DATA_MODE_LINE      0
#define DATA_MODE_COMPENC   1

/* DPRX14_CTRL0 src_fmt */
#define SRC_COLOR_FMT_RGB   1
#define SRC_COLOR_FMT_Y444  2
#define SRC_COLOR_FMT_Y422  3
#define SRC_COLOR_FMT_Y420  4

#define HIBYTE(value) ((value >> 8) & 0xFF)
#define LOBYTE(value) (value & 0xFF)
#define HIWORD(value)  ((value >> 16) & 0xFFFF)
#define LOWORD(value)  (value & 0xFFFF)

/* Main-Link rates */
#define _DP_LINK_SPEED_NONE    0x00
#define _DP_LOW_SPEED_162MHZ   0x06
#define _DP_HIGH_SPEED_270MHZ  0x0A
#define _DP_HIGH_SPEED2_540MHZ 0x14 /* DP 1.2 */
#define _DP_HIGH_SPEED3_810MHZ 0x1E /* DP 1.4 */

/* TRAINING_AUX_RD_INTERVAL */
#define _DP_LT_AUX_RD_INTVL_EQ_400US  0x00
#define _DP_LT_AUX_RD_INTVL_EQ_4MS    0x01
#define _DP_LT_AUX_RD_INTVL_EQ_8MS    0x02
#define _DP_LT_AUX_RD_INTVL_EQ_12MS   0x03
#define _DP_LT_AUX_RD_INTVL_EQ_16MS   0x04

#define _DP_one_frame_ms_MAX     42
#define _DP_ONE_FRAME_TIME_MAX   42
#define _DP_TWO_FRAME_TIME_MAX   84

#define _DP_RX_RELOAD_LEQ_INITIAL  0
#define _DP_RX_RELOAD_LEQ_LARGE    1
#define _DP_RX_RELOAD_LEQ_DEFAULT  2

#define _REF_VBID           0
#define _REF_BS_COUNTER     1

#define _DP_RX_MAC_PLL_VCO_MAX    700000000
#define _DP_HSYNC_WIDTH_MEASURE_COUNTER    2 /* HSW msa * Measure clk / Pixel clk */
#define _DE_ONLY_MODE_HSW   20

#define _GDIPHY_RX_GDI_CLK_KHZ    27000 /* 27MHz */
#define _DP_RX_VCO_TARGET_COUNT_2000_HBR3_SAVED  30000
#define _DP_RX_VCO_TARGET_COUNT_2000_HBR2_SAVED  20000
#define _DP_RX_VCO_TARGET_COUNT_2000_HBR_SAVED   10000
#define _DP_RX_VCO_TARGET_COUNT_2000_RBR_SAVED   6000
#define _DP_RX_VCO_TARGET_COUNT_1000_HBR3_SAVED  15000
#define _DP_RX_VCO_TARGET_COUNT_1000_HBR2_SAVED  10000
#define _DP_RX_VCO_TARGET_COUNT_1000_HBR_SAVED   5000
#define _DP_RX_VCO_TARGET_COUNT_1000_RBR_SAVED   3000

#define _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR3_SAVED  39000
#define _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR3_SAVED  33000
#define _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR2_SAVED  26000
#define _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR2_SAVED  22000
#define _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR_SAVED   13000
#define _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR_SAVED   11000
#define _DP_RX_COUNT_SST_UPPER_BOUND_2000_RBR_SAVED   7800
#define _DP_RX_COUNT_SST_LOWER_BOUND_2000_RBR_SAVED   6600

/*
 * Color Bit Depth values in
 * VSC SDP DB17 or MAS MISC0
 */
#define BIT_DEPTH_6BPC    0
#define BIT_DEPTH_8BPC    1
#define BIT_DEPTH_10BPC   2
#define BIT_DEPTH_12BPC   3

/* DP Sync Polarity Type */
#define _SYNC_POLARITY_POSITIVE    0
#define _SYNC_POLARITY_NEGATIVE    1

#define _HW_DP_SDP_PAYLOAD_LENGTH  32

#define _INFOFRAME_SDP_VERSION_1_3 0x13

/* Error code */
#define DPRX_NO_ERR                   0

#define CRT_CLK_INIT_ERR                5

#define DET_NORMAL_FAIL                 11
#define DET_IN_PHY_CTS                  12
#define DET_POWER_DOWN                  13

#define SIGNAL_LINK_NONE                16

/* Detection 5-Layer Check Error Codes */
#define DET_CDR_CHECK_FAIL              17   /* CDR unlock failed */
#define DET_SIGNAL_CHECK_FAIL           18   /* Signal quality check failed */
#define DET_MARGIN_CHECK_FAIL           19   /* Margin link check failed (reserved) */
#define DET_ALIGN_CHECK_FAIL            20   /* Lane alignment check failed */
#define DET_DECODE_CHECK_FAIL           27   /* 8b/10b decode error */

#define CRC_NOT_SUPPORT                 21

#define PLL_NF_INPUT_CLK_ERR            30
#define PLL_NF_SET_ERR	                31
#define PLL_NF_OFF_PROC_ERR	            32
#define PLL_NF_ON_PROC_ERR	            33
#define PLL_NF_CALC_ERR                 34  /* N/F PLL parameter calculation failed */

#define FIFO_DELAY_CHECK_ERR            35
#define FIFO_POLLING_CHECK_ERR          36

#define IP_HW_STATE_TIMEOUT             41

#define VSC_COLOR_SPACE_CHANGED	        51
#define VSC_COLOR_DEPTH_CHANGED	        52
#define VSC_COLORIMETRY_EXT_CHANGED	    53
#define VSC_COLORIMETRY_EXT_CHANGE_ERR	54
#define VSC_QUANTIZATION_CHANGED	    55
#define VSC_QUANTIZATION_CHANGE_ERR	    56

#define MISC_COLOR_SPACE_CHANGED	61
#define MISC_COLOR_SPACE_CHANGED_Y422	62
#define MISC_COLOR_SPACE_CHANGED_Y444	63
#define MISC_COLOR_SPACE_CHANGED_Y_ONLY	64
#define MISC_COLOR_SPACE_CHANGED_RAW	65
#define MISC_COLOR_SPACE_CHANGED_RGB	66
#define MISC_COLOR_DEPTH_CHANGED	67
#define MISC_SYNC_CHANGED	68

#define COLORIMETRY_CHANGE_ERR 71
#define UNEXPECTED_COLOR_DEPTH 81

#define SCAN_LT_NOT_PASSED	          85
#define SCAN_HDCP_CHECK_FAIL	      86
#define SCAN_VS_CHECK_FAIL	          87
#define SCAN_GET_MSA_FAIL	          88
#define SCAN_GET_MEASURE_INFO_FAIL	  89
#define SCAN_FMT_GEN_FAIL	          90
#define SCAN_CLK_GEN_FAIL	          91
#define SCAN_TRACKING_FAIL	          92
#define SCAN_MSA_TIMING_INVALID	      93
#define SCAN_MSA_TIMING_ZERO          94   /* MSA timing_info all zero */
#define SCAN_MSA_MVID_ZERO            95   /* MSA Mvid is zero */
#define SCAN_MSA_NVID_ZERO            96   /* MSA Nvid is zero */

#define DPRX_SDP_PKT_NULL_ERROR	      100
#define DPRX_SDP_PKT_LENGTH_ERROR	  101
#define DPRX_SDP_PKT_TYPE_ERROR	      102

#define HDCP_CHECK_ZERO_VTOTAL	      110
#define HDCP_CHECK_IN_FREE_SYNC	      111
#define HDCP_CHECK_VTOTAL	          112

/*============================================================================
 * Error Code Range Check Macros
 * Design: Match rtk_dprx_link_training.h conventions
 * MAC error codes: 1-119 (below LT error codes 120-209)
 *============================================================================*/

#define MAC_ERR_MIN                     1
#define MAC_ERR_MAX                     119

/* Error Code Range Check */
#define MAC_ERR_IS_MAC_ERROR(ret)       ((ret) >= MAC_ERR_MIN && (ret) <= MAC_ERR_MAX)

/* Error Code Category Helpers */
#define MAC_ERR_IS_CRT(err)             ((err) == CRT_CLK_INIT_ERR)
#define MAC_ERR_IS_DET(err)             ((err) >= DET_NORMAL_FAIL && (err) <= DET_POWER_DOWN)
#define MAC_ERR_IS_DET_CHECK(err)       (((err) >= DET_CDR_CHECK_FAIL && (err) <= DET_ALIGN_CHECK_FAIL) || \
                                         (err) == DET_DECODE_CHECK_FAIL)
#define MAC_ERR_IS_PLL(err)             ((err) >= PLL_NF_INPUT_CLK_ERR && (err) <= PLL_NF_ON_PROC_ERR)
#define MAC_ERR_IS_FIFO(err)            ((err) >= FIFO_DELAY_CHECK_ERR && (err) <= FIFO_POLLING_CHECK_ERR)
#define MAC_ERR_IS_VSC(err)             ((err) >= VSC_COLOR_SPACE_CHANGED && (err) <= VSC_QUANTIZATION_CHANGE_ERR)
#define MAC_ERR_IS_MISC(err)            ((err) >= MISC_COLOR_SPACE_CHANGED && (err) <= MISC_SYNC_CHANGED)
#define MAC_ERR_IS_SCAN(err)            ((err) >= SCAN_LT_NOT_PASSED && (err) <= SCAN_MSA_NVID_ZERO)
#define MAC_ERR_IS_SDP(err)             ((err) >= DPRX_SDP_PKT_NULL_ERROR && (err) <= DPRX_SDP_PKT_TYPE_ERROR)
#define MAC_ERR_IS_HDCP(err)            ((err) >= HDCP_CHECK_ZERO_VTOTAL && (err) <= HDCP_CHECK_VTOTAL)

/*============================================================================
 * Link Training Error Codes (120-209)
 *
 * Design: Fine-grained error codes for precise debugging.
 * PHY ops and Extcon functions can set lt_ctx.error_code directly.
 *
 * Range allocation:
 *   120-129: Parameter/State errors
 *   130-139: Clock Recovery errors
 *   140-149: Channel EQ errors
 *   150-159: PHY Configuration errors
 *   160-169: PHY Status/Operation errors
 *   170-179: AUX/DPCD errors
 *   180-189: Extcon/Type-C errors
 *   190-199: Timeout errors
 *   200-209: Training Abort errors
 *============================================================================*/

/* Parameter/State Errors (120-129) */
#define LT_ERR_INVALID_LINK_RATE        120
#define LT_ERR_INVALID_LANE_COUNT       121
#define LT_ERR_INVALID_TP               122
#define LT_ERR_INVALID_STATE            123
#define LT_ERR_NOT_INITIALIZED          124
#define LT_ERR_NULL_POINTER             125
#define LT_ERR_OPS_NOT_SET              126

/* Clock Recovery Errors (130-139) */
#define LT_ERR_CR_FAILED                130
#define LT_ERR_CR_LANE0_FAILED          131
#define LT_ERR_CR_LANE1_FAILED          132
#define LT_ERR_CR_LANE2_FAILED          133
#define LT_ERR_CR_LANE3_FAILED          134
#define LT_ERR_CR_MAX_RETRY             135
#define LT_ERR_CR_SAME_VS_RETRY         136
#define LT_ERR_CR_START_FAIL            137  /* Failed to start TP1 */
#define LT_ERR_CR_CDR_READ_FAIL         138  /* Failed to read CDR status */
#define LT_ERR_CR_LOST                  139  /* CR lost during EQ training */

/* Channel EQ Errors (140-149) */
#define LT_ERR_EQ_FAILED                140
#define LT_ERR_EQ_LANE0_FAILED          141
#define LT_ERR_EQ_LANE1_FAILED          142
#define LT_ERR_EQ_LANE2_FAILED          143
#define LT_ERR_EQ_LANE3_FAILED          144
#define LT_ERR_EQ_MAX_RETRY             145
#define LT_ERR_EQ_START_FAIL            146  /* Failed to start TP2/3/4 */
#define LT_ERR_EQ_SYMBOL_READ_FAIL      147  /* Failed to read symbol lock */
#define LT_ERR_EQ_ALIGN_READ_FAIL       148  /* Failed to read lane align */
#define LT_ERR_EQ_CR_LOST               149  /* CR lost during EQ (alias) */

/* PHY Configuration Errors (150-159) */
#define LT_ERR_PHY_NOT_READY            150
#define LT_ERR_PHY_CDR_LOCK_FAIL        151
#define LT_ERR_PHY_PLL_LOCK_FAIL        152
#define LT_ERR_PHY_CONFIG_FAIL          153
#define LT_ERR_PHY_RESET_FAIL           154
#define LT_ERR_PHY_LANE_CONFIG_FAIL     155  /* Failed to configure lanes */
#define LT_ERR_PHY_RATE_NOT_SUPPORTED   156  /* Link rate not supported */
#define LT_ERR_PHY_SSC_CONFIG_FAIL      157  /* SSC configuration failed */
#define LT_ERR_PHY_POLARITY_FAIL        158  /* Lane polarity config failed */

/* PHY Status/Operation Errors (160-169) */
#define LT_ERR_PHY_STATUS_READ          160  /* Failed to read PHY status */
#define LT_ERR_PHY_LANE_STATUS_READ     161  /* Failed to read lane status */
#define LT_ERR_PHY_ADJUST_REQ_READ      162  /* Failed to read adjust request */
#define LT_ERR_PHY_ERROR_COUNT_READ     163  /* Failed to read error count */
#define LT_ERR_PHY_CDR_STATUS_READ      164  /* Failed to read CDR status */
#define LT_ERR_PHY_EQ_STATUS_READ       165  /* Failed to read EQ status */
#define LT_ERR_PHY_SYMBOL_STATUS_READ   166  /* Failed to read symbol lock */
#define LT_ERR_PHY_REG_ACCESS           167  /* Register access error */

/* AUX/DPCD Errors (170-179) */
#define LT_ERR_AUX_WRITE_FAIL           170
#define LT_ERR_AUX_READ_FAIL            171
#define LT_ERR_DPCD_WRITE_FAIL          172
#define LT_ERR_DPCD_READ_FAIL           173
#define LT_ERR_DPCD_INVALID_DATA        174
#define LT_ERR_AUX_DEFER                175
#define LT_ERR_AUX_NACK                 176

/* Extcon/Type-C Errors (180-189) */
#define LT_ERR_EXTCON_NOT_FOUND         180
#define LT_ERR_EXTCON_REG_FAIL          181
#define LT_ERR_EXTCON_UNREG_FAIL        182
#define LT_ERR_EXTCON_STATE_READ        183
#define LT_ERR_EXTCON_NOTIFY_REG        184  /* Notifier registration failed */
#define LT_ERR_TYPEC_ORIENTATION        185  /* Failed to get orientation */
#define LT_ERR_TYPEC_DP_NOT_CONNECTED   186  /* DP Alt Mode not active */
#define LT_ERR_EXTCON_PROVIDER_REG      187  /* Provider registration failed */

/* Timeout Errors (190-199) */
#define LT_ERR_CR_TIMEOUT               190
#define LT_ERR_EQ_TIMEOUT               191
#define LT_ERR_AUX_TIMEOUT              192
#define LT_ERR_PHY_TIMEOUT              193
#define LT_ERR_SYMBOL_LOCK_TIMEOUT      194
#define LT_ERR_CDR_LOCK_TIMEOUT         195
#define LT_ERR_LANE_ALIGN_TIMEOUT       196

/* Training Abort Errors (200-209) */
#define LT_ERR_SOURCE_ABORT             200  /* Source sent TP0 during training */
#define LT_ERR_TRAINING_LOST            201  /* Training lost (HPD or signal) */
#define LT_ERR_UNKNOWN                  202  /* Unknown failure reason */

/* Error Code Range Check */
#define LT_ERR_IS_LT_ERROR(ret)    ((ret) >= 120 && (ret) <= 209)

/* Error Code Category Helpers */
#define LT_ERR_IS_PARAM(err)       ((err) >= 120 && (err) <= 129)
#define LT_ERR_IS_CR(err)          ((err) >= 130 && (err) <= 139)
#define LT_ERR_IS_EQ(err)          ((err) >= 140 && (err) <= 149)
#define LT_ERR_IS_PHY_CONFIG(err)  ((err) >= 150 && (err) <= 159)
#define LT_ERR_IS_PHY_STATUS(err)  ((err) >= 160 && (err) <= 169)
#define LT_ERR_IS_AUX(err)         ((err) >= 170 && (err) <= 179)
#define LT_ERR_IS_EXTCON(err)      ((err) >= 180 && (err) <= 189)
#define LT_ERR_IS_TIMEOUT(err)     ((err) >= 190 && (err) <= 199)
#define LT_ERR_IS_ABORT(err)       ((err) >= 200 && (err) <= 209)

/* Legacy/Combined helpers */
#define LT_ERR_IS_PHY(err)         (LT_ERR_IS_PHY_CONFIG(err) || LT_ERR_IS_PHY_STATUS(err))

#define LT_ERR_IS_CR_LANE_FAIL(err) \
	((err) >= LT_ERR_CR_LANE0_FAILED && (err) <= LT_ERR_CR_LANE3_FAILED)
#define LT_ERR_IS_EQ_LANE_FAIL(err) \
	((err) >= LT_ERR_EQ_LANE0_FAILED && (err) <= LT_ERR_EQ_LANE3_FAILED)
#define LT_ERR_GET_FAILED_LANE(err) \
	(LT_ERR_IS_CR_LANE_FAIL(err) ? ((err) - LT_ERR_CR_LANE0_FAILED) : \
	 LT_ERR_IS_EQ_LANE_FAIL(err) ? ((err) - LT_ERR_EQ_LANE0_FAILED) : -1)

/**
 * LT_CHECK_RET - Check return value and set error_code
 * @lt: Link Training context pointer
 * @ret: Return value from PHY/Extcon ops
 * @fallback: Fallback error code if ret is negative errno
 *
 * Usage:
 *   ret = ops->check_cdr_lock(dprx, ...);
 *   LT_CHECK_RET(lt, ret, LT_ERR_PHY_CDR_STATUS_READ);
 */
#define LT_CHECK_RET(lt, ret, fallback) do { \
	if (ret) { \
		(lt)->error_code = LT_ERR_IS_LT_ERROR(ret) ? (ret) : (fallback); \
		return ((ret) < 0) ? (ret) : -EIO; \
	} \
} while (0)

/* Enumerations of DP Sink Reveive Port */
enum RTK_DP_SINK_PORT {
	_DP_SINK_REVEICE_PORT0 = 0,
	_DP_SINK_REVEICE_PORT1,
};

enum RTK_DP_LANE_COUNT {
	_DP_ONE_LANE = 1,
	_DP_TWO_LANE = 2,
	_DP_FOUR_LANE = 4,
};

/* Definitions of DP Lane */
enum RTK_DP_LANE {
	_DP_LANE_0,
	_DP_LANE_1,
	_DP_LANE_2,
	_DP_LANE_3,
};

/* Enumerations of DP Link Rate */
enum RTK_DP_LINK_RATE {
	_DP_LINK_NONE = _DP_LINK_SPEED_NONE,
	_DP_LINK_RBR = _DP_LOW_SPEED_162MHZ,
	_DP_LINK_HBR = _DP_HIGH_SPEED_270MHZ,
	_DP_LINK_HBR2 = _DP_HIGH_SPEED2_540MHZ,
	_DP_LINK_HBR3 = _DP_HIGH_SPEED3_810MHZ,
};

/* Enumerations of DP Link Training Pattern */
enum RTK_LT_PATTERN {
	_DP_TRAINING_PATTERN_END = 0,
	_DP_TRAINING_PATTERN_1 = 1,
	_DP_TRAINING_PATTERN_2 = 2,
	_DP_TRAINING_PATTERN_3 = 3,
	_DP_TRAINING_PATTERN_4 = 7,
};

/* Enumerations of DP Link Training Status */
/* enum RTK_DP_LT_STATUS removed - replaced by rtk_dprx_lt_context */

/* Enumerations of DP Version Type */
enum RTK_DP_VERSION {
	_DP_VERSION_1_0 = 0x10,
	_DP_VERSION_1_1 = 0x11,
	_DP_VERSION_1_2 = 0x12,
	_DP_VERSION_1_3 = 0x13,
	_DP_VERSION_1_4 = 0x14,
};

/* Enumerations of DP Sink Status */
enum RTK_DP_SINK_STATUS {
	_DP_SINK_OUT_OF_SYNC = 0,
	_DP_SINK_IN_SYNC,
};

/* Enumerations of DP Reset Status */
enum RTK_DP_RESET_STATUS {
	_DP_DPCD_LINK_STATUS_INITIAL = 0x00,
	_DP_DPCD_LINK_STATUS_IRQ = 0x01,
};

/* Enumerations of signal measure target */
enum RTK_MEASURE_TARGET {
	_DP_MEASURE_TARGET_RAW_DATA = 0x00,
	_DP_MEASURE_TARGET_CDR_CLOCK = _BIT5,
};

/* Enumerations of signal measure period */
enum RTK_MEASURE_PERIOD {
	_DP_MEASURE_PERIOD_125_CYCLE,
	_DP_MEASURE_PERIOD_250_CYCLE,
	_DP_MEASURE_PERIOD_1000_CYCLE,
	_DP_MEASURE_PERIOD_2000_CYCLE,
};

enum RTK_PIXEL_MODE {
	_DP_RX_MAC_ONE_PIXEL_MODE,
	_DP_RX_MAC_TWO_PIXEL_MODE,
	_DP_RX_MAC_PIXEL_MODE_NONE,
};

enum RTK_DP_COLOR_SPACE {
	_COLOR_SPACE_RGB = 0x00,
	_COLOR_SPACE_YCBCR444 = 0x01,
	_COLOR_SPACE_YCBCR422 = 0x02,
	_COLOR_SPACE_YCBCR420 = 0x03,
	_COLOR_SPACE_Y_ONLY = 0x04,
	_COLOR_SPACE_RAW = 0x05,
};

enum RTK_DP_COLORIMETRY {
	_COLORIMETRY_YCC_XVYCC601,
	_COLORIMETRY_YCC_ITUR_BT601,
	_COLORIMETRY_YCC_XVYCC709,
	_COLORIMETRY_YCC_ITUR_BT709,
	_COLORIMETRY_RGB_SRGB,
	_COLORIMETRY_RGB_XRRGB,
	_COLORIMETRY_RGB_SCRGB,
	_COLORIMETRY_RGB_ADOBERGB,
	_COLORIMETRY_RGB_DCI_P3,
	_COLORIMETRY_RGB_COLOR_PROFILE,
	_COLORIMETRY_Y_ONLY,
	_COLORIMETRY_RAW,
	_COLORIMETRY_EXT,
	_COLORIMETRY_NONE,
};

enum RTK_DP_VSC_COLOR_SPACE {
	_VSC_COLOR_SPACE_0 = 0x00,
	_VSC_COLOR_SPACE_1 = 0x10,
	_VSC_COLOR_SPACE_2 = 0x20,
	_VSC_COLOR_SPACE_3 = 0x30,
	_VSC_COLOR_SPACE_4 = 0x40,
	_VSC_COLOR_SPACE_5 = 0x50,
};

enum RTK_DP_VSC_COLORIMETRY {
	_VSC_COLORIMETRY_0 = 0x00,
	_VSC_COLORIMETRY_1,
	_VSC_COLORIMETRY_2,
	_VSC_COLORIMETRY_3,
	_VSC_COLORIMETRY_4,
	_VSC_COLORIMETRY_5,
	_VSC_COLORIMETRY_6,
	_VSC_COLORIMETRY_7,
};

enum RTK_DP_COLORIMETRY_EXT {
	_COLORIMETRY_EXT_YCC_ITUR_BT601,
	_COLORIMETRY_EXT_YCC_ITUR_BT709,
	_COLORIMETRY_EXT_YCC_XVYCC601,
	_COLORIMETRY_EXT_YCC_XVYCC709,
	_COLORIMETRY_EXT_YCC_SYCC601,
	_COLORIMETRY_EXT_YCC_ADOBEYCC601,
	_COLORIMETRY_EXT_YCC_ITUR_BT2020_CL,
	_COLORIMETRY_EXT_YCC_ITUR_BT2020_NCL,
	_COLORIMETRY_EXT_RGB_SRGB,
	_COLORIMETRY_EXT_RGB_XRRGB,
	_COLORIMETRY_EXT_RGB_SCRGB,
	_COLORIMETRY_EXT_RGB_ADOBERGB,
	_COLORIMETRY_EXT_RGB_DCI_P3,
	_COLORIMETRY_EXT_RGB_CUSTOM_COLOR_PROFILE,
	_COLORIMETRY_EXT_RGB_ITUR_BT2020,
	_COLORIMETRY_EXT_Y_ONLY_DICOM_PART14,
	_COLORIMETRY_EXT_RAW_CUSTOM_COLOR_PROFILE,
	_COLORIMETRY_EXT_RESERVED,
};

// TODO: FIXME
enum RTK_DP_COLOR_QUANTIZATION {
	_DP_COLOR_QUANTIZATION_LIMIT,
	_DP_COLOR_QUANTIZATION_FULL
};

enum RTK_DP_RGB_QUANTIZATION {
	_RGB_QUANTIZATION_RESERVED,
	_RGB_QUANTIZATION_LIMIT_RANGE,
	_RGB_QUANTIZATION_FULL_RANGE,
};

enum RTK_DP_YCC_QUANTIZATION {
	_YCC_QUANTIZATION_LIMIT_RANGE,
	_YCC_QUANTIZATION_FULL_RANGE,
};

/* Enumerations of SDP index */
enum RTK_DP_SDP_BUFF {
	_DP_SDP_BUFF_NONE,
	_DP_SDP_BUFF_HDR,
	_DP_SDP_BUFF_SPD,
	_DP_SDP_BUFF_ISRC,
	_DP_SDP_BUFF_RSV0,
	_DP_SDP_BUFF_RSV1,
	_DP_SDP_BUFF_VSC,
};

/* Enumerations of SDP type */
enum RTK_DP_SDP_TYPE {
	_DP_SDP_TYPE_AUD_TIMESTAMP = 0x01,
	_DP_SDP_TYPE_AUD_STREAM,
	_DP_SDP_TYPE_EXTENSION = 0x04,
	_DP_SDP_TYPE_AUD_COPYMANAGEMENT,
	_DP_SDP_TYPE_ISRC,
	_DP_SDP_TYPE_VSC,
	_DP_SDP_TYPE_CAM_GEN_0,
	_DP_SDP_TYPE_CAM_GEN_1,
	_DP_SDP_TYPE_CAM_GEN_2,
	_DP_SDP_TYPE_CAM_GEN_3,
	_DP_SDP_TYPE_CAM_GEN_4,
	_DP_SDP_TYPE_CAM_GEN_5,
	_DP_SDP_TYPE_CAM_GEN_6,
	_DP_SDP_TYPE_CAM_GEN_7,
	_DP_SDP_TYPE_PPS,
	_DP_SDP_TYPE_VSC_EXT_VESA = 0x20,
	_DP_SDP_TYPE_VSC_EXT_CEA,
	_DP_SDP_TYPE_INFOFRAME_RSV = 0x80,
	_DP_SDP_TYPE_INFOFRAME_VENDOR_SPEC,
	_DP_SDP_TYPE_INFOFRAME_AVI,
	_DP_SDP_TYPE_INFOFRAME_SPD,
	_DP_SDP_TYPE_INFOFRAME_AUDIO,
	_DP_SDP_TYPE_INFOFRAME_MPEG,
	_DP_SDP_TYPE_INFOFRAME_NTSC_VBI,
	_DP_SDP_TYPE_INFOFRAME_HDR = 0x87,
};

enum RTK_DP_DECODE_METHOD {
	_DP_MAC_DECODE_METHOD_PRBS7,
	_DP_MAC_DECODE_METHOD_8B10B,
	_DP_MAC_DECODE_METHOD_8B10B_DISPARITY,
};

enum RTK_DP_TYPE_C_PIN_CFG {
	_TYPE_C_PIN_ASSIGNMENT_E
};

enum RTK_DP_MAC_CLK_SELECT {
	_DP_MAC_CLOCK_SELECT_LINK_CLOCK = 0x00,
	_DP_MAC_CLOCK_SELECT_XTAL_CLOCK = _BIT6,
};

enum RTK_DP_VBID_INFO {
	_DP_VBID_INTERLACE_MODE = 0x00,
	_DP_VBID_VIDEO_STREAM,
	_DP_VBID_AUDIO_STREAM,
};

enum RTK_DP_SPD_INFO {
	_SPD_INFO_FREESYNC_SUPPORT,
	_SPD_INFO_FREESYNC_ENABLE,
	_SPD_INFO_FREESYNC_ACTIVE,
	_SPD_INFO_SEAMLESS_LOCAL_DIMMING_DISABLE_CONTROL,
	_SPD_INFO_FREESYNC_MIN_VFREQ,
	_SPD_INFO_FREESYNC_MAX_VFREQ,
	_SPD_INFO_TARGET_OUTPUT_PIXEL_RATE,
	_SPD_INFO_FIXED_RATE_CONTENT_ACTIVE,
};

/* Enumerations of DP HS Tracking Type */
enum RTK_DP_HS_TRACKING_TYPE {
	_DP_HS_TRACKING_HW_MODE = 0x00,
	_DP_HS_TRACKING_FW_MODE,
};

/* Enumerations of DP Fifo Check Condition */
enum RTK_DP_FIFO_CHECK_CONDITION {
	_DP_FIFO_DELAY_CHECK = 0x00,
	_DP_FIFO_POLLING_CHECK,
};

/* Enumerations of DP HDCP BStatus Type */
enum RTK_DP_RX_BSTATUS_TYPE {
	_DP_HDCP_BSTATUS_V_READY = 0x01,
	_DP_HDCP_BSTATUS_R0_AVAILABLE = 0x02,
	_DP_HDCP_BSTATUS_LINK_INTEGRITY_FAIL = 0x04,
	_DP_HDCP_BSTATUS_REAUTH_REQ = 0x08,
};

enum RTK_HDCP_TYPE {
	_HDCP_14,
	_HDCP_22,
};

/**
 * struct rtk_timing_info - timing information from MSA data
 *
 * @HSP: Horizontal Sync Polarity
 * @VSP: Vertical Sync Polarity
 * @Interlace: Interlace mode flag
 * @DpInterlaceVBID: Interlace For Dp VBID
 * @VideoField: Field for video compensation
 * @InputVheightOdd: Vheight is odd
 * @HFreq: Horizontal Freq. (unit: 0.1kHz)
 * @HFreqAdjusted: Horizontal Freq. adjusted (unit: 0.1kHz)
 * @HTotal: Horizontal Total length (unit: Pixel)
 * @HWidth: Horizontal Active Width (unit: Pixel)
 * @HStart: Horizontal Start (unit: Pixel)
 * @HSWidth: Horizontal Sync Pulse Count (unit: SyncProc Clock)
 * @VFreq: Vertical Freq. (unit: 0.1Hz)
 * @VFreqAdjusted: Vertical Freq. adjusted (unit: 0.1Hz)
 * @VTotal: Vertical Total length (unit: HSync)
 * @VTotalOdd: Vertical Total Odd length (unit: HSync)
 * @VHeight: Vertical Active Height (unit: HSync)
 * @VStart: Vertical Start (unit: HSync)
 * @VSWidth: Vertical Sync Width (unit: HSync)
 */
struct rtk_timing_info {
	u8  HSP;
	u8  VSP;
	u8  Interlace;
	u8  DpInterlaceVBID;
	u8  VideoField;
	u8  InputVheightOdd;
	u32 HFreq;
	u32 HFreqAdjusted;
	u32 HTotal;
	u32 HWidth;
	u32 HStart;
	u32 HSWidth;
	u32 VFreq;
	u32 VFreqAdjusted;
	u32 VTotal;
	u32 VTotalOdd;
	u32 VHeight;
	u32 VStart;
	u32 VSWidth;
};

/**
 * struct rtk_link_info
 *
 * @LinkClockHz: Link Clock
 * @Mvid: Mvid
 * @Nvid: Nvid
 * @StreamClockHz: Stream Clock (reduced 0.07% for tracking)
 * @PixelClockHz: Original Pixel Clock (before 0.07% reduction)
 * @VBsToBsCountN: V BS to BS Count of The Nth Frame
 * @VBsToBsCountN1: V BS to BS Count of The (N+1)th Frame
 * @HBsToBsCount: H BS to BS Count
 * @HwInterlaceDetect: HW Detect Interlace Flag
 * @HwFakeInterlaceDetect: HW Detect Fake Interlace Flag
 * @InterlaceFieldN: VBID[1] Inerlace Field Flag of The Nth Frame
 * @InterlaceFieldN1: VBID[1] Inerlace Field Flag of The (N+1)th Frame
 * @InterlaceOddMode: VBID[1] Inerlace Field Mode (Even or Odd)
 */
struct rtk_link_info {
	u32 LinkClockHz;
	u32 Mvid;
	u32 Nvid;
	u32 StreamClockHz;
	u32 PixelClockHz;
	u32 VBsToBsCountN;
	u32 VBsToBsCountN1;
	u32 HBsToBsCount;
	u8 HwInterlaceDetect;
	u8 HwFakeInterlaceDetect;
	u8 InterlaceFieldN;
	u8 InterlaceFieldN1;
	bool InterlaceOddMode;
};

struct rtk_dprx_stream_info {
	struct rtk_timing_info timing_info;
	struct rtk_link_info link_info;
};

#define to_rtk_dprx(x) container_of(x, struct rtk_dprx, x)

struct rtk_dprx;

struct rtk_dprx_rbus_ops {
	/* dprx_reg ops*/
	int (*read)(u32 offset, u32 *value);
	int (*write)(u32 offset, u32 value);
	int (*mask_write)(u32 offset, u32 mask, u32 val);
	void (*dump_dprx_reg)(struct rtk_dprx *dprx, u32 start_offset, u32 end_offset);
	/* ip_reg ops*/
	void (*set_byte)(u32 offset, u32 value);
	u8 (*get_byte)(u32 offset);
	void (*set_byte_extint)(u32 offset, u32 value);
	u8 (*get_byte_extint)(u32 offset);
	void (*set_bit)(u32 offset, u32 and_mask, u32 or_mask);
	u8 (*get_bit)(u32 offset, u32 and_mask);
	void (*set_bit_extint)(u32 offset, u32 and_mask, u32 or_mask);
	u8 (*get_bit_extint)(u32 offset, u32 and_mask);
	u32 (*get_word)(u32 offset);
	u32 (*get_dword)(u32 offset);
	void (*dump_ip_reg)(struct rtk_dprx *dprx, u32 start_offset, u32 end_offset);
};

struct rtk_dprx_phy_ops {
	s32 (*get_target_clock)(struct rtk_dprx *dprx, u8 lane_number);
	bool (*get_phy_cts_flag)(struct rtk_dprx *dprx);
	u8 (*get_phy_cts_testlane)(struct rtk_dprx *dprx);
	u8 (*get_lane_mapping)(struct rtk_dprx *dprx, enum RTK_DP_LANE);
	void (*phy_cts)(struct rtk_dprx *dprx);
	void (*phy_cts_auto_mode)(struct rtk_dprx *dprx);
	void (*rebuild_phy)(struct rtk_dprx *dprx, u8 link_rate, u8 lane_count);
	void (*signal_detect_initial)(struct rtk_dprx *dprx, u8 link_rate, u8 leq_scan_val);
	void (*signal_detection)(struct rtk_dprx *dprx, bool enable);
	void (*set_lane_count)(struct rtk_dprx *dprx, u8 lane_count);
	u8 (*get_lane_count)(struct rtk_dprx *dprx);
	void (*set_link_rate)(struct rtk_dprx *dprx, u8 link_rate);
	u8 (*get_link_rate)(struct rtk_dprx *dprx);
	void (*set_training_pattern)(struct rtk_dprx *dprx, u8 pattern);
	u8 (*get_training_pattern)(struct rtk_dprx *dprx);
	u8 (*check_cts_tp1)(struct rtk_dprx *dprx);
};

struct rtk_dprx_aux_ops {
	void (*set_manual_mode)(struct rtk_dprx *dprx);
	void (*set_auto_mode)(struct rtk_dprx *dprx);
	void (*set_defer_mode)(struct rtk_dprx *dprx);
	u8 (*get_manual_mode_status)(struct rtk_dprx *dprx);
	void (*initial)(struct rtk_dprx *dprx);
	void (*set_pn_swap)(struct rtk_dprx *dprx, bool enable_swap);
	void (*change_dpcd_version)(struct rtk_dprx *dprx,
		enum RTK_DP_VERSION version, enum RTK_DP_LINK_RATE max_link_rate);
	void (*set_branch_dpcd_ident)(struct rtk_dprx *dprx,
		struct drm_dp_dpcd_ident *ident);
	void (*reset_dpcd_link_status)(struct rtk_dprx *dprx,
		enum RTK_DP_RESET_STATUS rst_status);
	void (*link_status_irq)(struct rtk_dprx *dprx);
	void (*aux_power_on)(struct rtk_dprx *dprx);
	void (*hpd_irq_assert)(struct rtk_dprx *dprx);
	void (*set_sink_status)(struct rtk_dprx *dprx,
			enum RTK_DP_SINK_PORT port, enum RTK_DP_SINK_STATUS sync_status);
	u8 (*get_dpcd_info)(struct rtk_dprx *dprx, u8 port_h, u8 port_m, u8 port_l);
	u8 (*get_dpcd_bit_info)(struct rtk_dprx *dprx,
			u8 port_h, u8 port_m, u8 port_l, u8 dpcd_bit);
	void (*set_dpcd_value)(struct rtk_dprx *dprx,
			u8 port_h, u8 port_m, u8 port_l, u8 value);
	void (*set_dpcd_write_value)(struct rtk_dprx *dprx,
			u8 port_h, u8 port_m, u8 port_l, u8 value);
	void (*set_dpcd_write1_clear_value)(struct rtk_dprx *dprx,
			u8 port_h, u8 port_m, u8 port_l, u8 value);
	void (*set_dpcd_bit_value)(struct rtk_dprx *dprx, u8 port_h, u8 port_m, u8 port_l,
			u8 not_bit, u8 bit_value);
	void (*set_dpcd_bit_write_value)(struct rtk_dprx *dprx,
			u8 port_h, u8 port_m, u8 port_l, u8 not_bit, u8 bit_value);
	void (*get_dpcd_addr)(struct rtk_dprx *dprx, u8 *dpcd_addr);
	void (*set_dpcd_addr)(struct rtk_dprx *dprx, u8 *dpcd_addr);
	bool (*get_valid_video_check)(struct rtk_dprx *dprx);
	void (*clr_valid_video_check)(struct rtk_dprx *dprx);
	void (*active_link_status_irq)(struct rtk_dprx *dprx);
	void (*cancel_link_status_irq)(struct rtk_dprx *dprx);
};

struct rtk_dprx_mac_ops {
	int (*crt_clk_init)(struct rtk_dprx *dprx);
	int (*crt_clk_deinit)(struct rtk_dprx *dprx);
	void (*mac_reset)(struct rtk_dprx *dprx);
	void (*mac_initial)(struct rtk_dprx *dprx);
	void (*sdp_initial)(struct rtk_dprx *dprx);
	void (*decode_error_count_reset)(struct rtk_dprx *dprx,	enum RTK_DP_DECODE_METHOD method);
	void (*lane_count_set)(struct rtk_dprx *dprx, u8 lane_count);
	int (*pre_detect)(struct rtk_dprx *dprx);
	int (*scan_input_port)(struct rtk_dprx *dprx);
	int (*fifo_check)(struct rtk_dprx *dprx,
			  enum RTK_DP_FIFO_CHECK_CONDITION check_condition);
};

struct rtk_dprx_wrap_ops {
	void (*scale_down)(struct rtk_dprx *dprx,
		u32 src_width, u32 src_height, u32 dst_width, u32 dst_height);
	u32 (*calculate_video_size)(u32 dst_width, u32 dst_height, bool compenc_mode);
	void (*video_size_cfg)(struct rtk_dprx *dprx);
	void (*dma_buf_cfg)(struct rtk_dprx *dprx, u8 entry_index, u64 start_addr);
	u8 (*is_frame_done)(u32 done_st, u8 entry_index);
	void (*clear_done_flag)(struct rtk_dprx *dprx, u8 entry_index);
	void (*meta_swap)(struct rtk_dprx *dprx, u8 enable);
	void (*crc_ctrl)(struct rtk_dprx *dprx, u8 enable);
	void (*color_bar_test)(struct rtk_dprx *dprx, u8 enable);
	void (*dma_go_ctrl)(struct rtk_dprx *dprx, u8 enable);
	void (*interrupt_ctrl)(struct rtk_dprx *dprx, u8 enable);
	int (*get_intr_state)(struct rtk_dprx *dprx, u32 *p_done_st);
};

struct rtk_dprx_edid_ops {
	int (*set_dft)(struct rtk_dprx *dprx);
};

struct rtk_dprx_audio_ops {
	int (*initial)(struct rtk_dprx *dprx);
};

struct rtk_dprx_isr_ops {
	int (*aux_handler)(struct rtk_dprx *dprx);
};

struct rtk_dprx_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head link;
	dma_addr_t phy_addr;
	u8 entry_index;
};

#define to_dprx_buffer(buf)	container_of(buf, struct rtk_dprx_buffer, vb)

struct rtk_dprx_infoframe_state {
	bool amd_spd_infoframe_change;
	bool amd_spd_infoframe_receive;
	bool spd_ifnoframe_detecting;
	bool hdr_infoframe_change;
	bool hdr_infoframe_receive;
	bool hdr_info_detecting;
	bool audio_infoframe_change;
	bool audio_infoframe_receive;
	bool audio_ifnoframe_detecting;
};

struct rtk_dprx_mac_data {
	u8 msa_fail_reset_count;
	bool amd_spd_local_dimming;
	u8 AudioInfoSdpData[_HW_DP_SDP_PAYLOAD_LENGTH];
	bool vsc_sdp_color_mode;
	bool en_free_sync;
	bool en_crc_cal;
	enum RTK_DP_COLOR_SPACE color_space;
	u8 pre_colorimetry_ext;
	u8 colorimetry_ext;
	enum RTK_DP_COLORIMETRY colorimetry;
	u8 pre_color_space;
	u8 pre_color_depth;
	u8 pre_colorimetry;
	u8 content_type;
	u8 pre_quantization;
	u8 rgb_quantization;
	u8 ycc_quantization;
	u32 vfront_porch;
	u32 drr_msa_htotal;
	u32 drr_htotal_margin;
	u32 drr_vfreq_max;
	u32 drr_vfreq_min;
	bool tracking_disabled;
};

struct rtk_dprx_phy_data {
	u8 lane_count;
	u8 link_rate;
	bool tp1_initial_done;
	u8 link_request01;
	u8 link_request23;
	u8 training_pattern;
	u8 link_status01;
	u8 link_status23;
	u8 cts_ctrl;
	bool backup_pd_link_status_flg;
	u8 link_status_backup[3];
};

/**
 * struct rtk_dprx
 *
 * @aux_diff_mode: True if the aux is differential, false if aux is single-ended
 * @src_fmt: 1-RGB, 2-Y444, 3-Y422, 4-Y420
 */
struct rtk_dprx {
	struct reset_control *reset_dprx;
	struct clk *clk_dprx;
	bool crt_clk_inited;
	struct regmap *dprx_reg;
	struct regmap *ip_reg;
	int irq;

	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	struct vb2_queue queue;
	struct v4l2_pix_format pix_fmt;
	struct v4l2_bt_timings detected_timings;
	unsigned int v4l2_input_status;
	struct mutex video_lock;

	struct list_head buffers;
	struct mutex buffer_lock; /* buffer list lock */
	unsigned int sequence;

	const struct rtk_dprx_rbus_ops *rbus_ops;
	const struct rtk_dprx_phy_ops *phy_ops;
	const struct rtk_dprx_aux_ops *aux_ops;
	const struct rtk_dprx_mac_ops *mac_ops;
	const struct rtk_dprx_wrap_ops *wrap_ops;
	const struct rtk_dprx_edid_ops *edid_ops;
	const struct rtk_dprx_audio_ops *audio_ops;
	const struct rtk_dprx_isr_ops *isr_ops;
	struct rtk_dprx_buffer *cur_buf[4];

	struct rtk_dprx_phy_data phy_dat;
	struct rtk_dprx_mac_data mac_dat;
	struct rtk_dprx_infoframe_state i_state;

	u32 max_link_rate;
	u8 hdcp_support;
	enum RTK_DP_VERSION dpcd_ver;
	struct drm_dp_dpcd_ident ident;
	bool aux_diff_mode;
	bool free_sync_support;
	bool audio_support;

	bool compenc_mode;
	u8 src_fmt;
	u32 src_width;
	u32 src_height;
	u32 src_vfreq; /* Vertical Freq in 0.1Hz (e.g. 600 = 60.0Hz) */
	u32 dst_width;
	u32 dst_height;
	u32 line_pitch;
	u32 header_pitch;
	u32 video_size;

	bool fake_lt;
	/* lt_status removed - replaced by lt_ctx (link_integrity_fail, fake_training_mode, vbios_mode) */
	bool lt_setphy_finish;

	wait_queue_head_t detect_wait;
	bool is_source_read_err_cnt;
	bool detect_done;

	/* Link Training DPCD IRQ pending status (used by threaded IRQ) */
	atomic_t lt_dpcd_irq_pending;

	/* Workaround: suppress mismatch false alarm within 50ms after streaming start */
	unsigned long streaming_start_jiffies;
	unsigned int mismatch_err_count;

	/* Link Training Module Context */
	struct rtk_dprx_lt_context lt_ctx;
};

struct rtk_dprx_fmt {
	unsigned int fourcc;
};

extern int rtk_dprx_rbus_init(struct rtk_dprx *dprx);
extern int rtk_dprx_phy_init(struct rtk_dprx *dprx);
extern int rtk_dprx_aux_init(struct rtk_dprx *dprx);
extern int rtk_dprx_mac_init(struct rtk_dprx *dprx);
extern int rtk_dprx_wrap_init(struct rtk_dprx *dprx);
extern int rtk_dprx_edid_init(struct rtk_dprx *dprx);
extern int rtk_dprx_audio_init(struct rtk_dprx *dprx);
extern int rtk_dprx_isr_init(struct rtk_dprx *dprx);

/* Link Training */
void rtk_dprx_lt_dpcd_irq_handler(struct rtk_dprx *dprx, u8 irq_status);

#endif /* RTK_DP_RX_H_ */
