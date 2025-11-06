/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek npu_pp video m2m and capture v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#ifndef __NPP_CTRL_H__
#define __NPP_CTRL_H__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include "npp_inband.h"
#include "npp_common.h"

#define NPP_VI_TVVE_qlevel_queue_sel_y_mask    (0x00000060)
#define NPP_VI_TVVE_qlevel_queue_sel_c_mask    (0x00000010)
#define NPP_VI_TVVE_qlevel_queue_sel_y(data)   (0x00000060&((data)<<5))
#define NPP_VI_TVVE_qlevel_queue_sel_c(data)   (0x00000010&((data)<<4))

#define NPP_VI_TVVE_CROP_width(data)  (0x1FFF0000&((data)<<16))
#define NPP_VI_TVVE_CROP_height(data) (0x00001FFF&((data)<<0))
#define NPP_VI_TVVE_CROP_POS_x(data)  (0x1FFF0000&((data)<<16))
#define NPP_VI_TVVE_CROP_POS_y(data)  (0x00001FFF&((data)<<0))

#define NPP_VI_TVVE_HEAD_endian_mask       (0x80000000)
#define NPP_VI_TVVE_HEAD_endian_4byte_mask (0x40000000)
#define NPP_VI_TVVE_HEAD_pitch_mask        (0x000003FF)
#define NPP_VI_TVVE_HEAD_endian(data)       (0x80000000&((data)<<31))
#define NPP_VI_TVVE_HEAD_endian_4byte(data) (0x40000000&((data)<<30))
#define NPP_VI_TVVE_HEAD_pitch(data)  (0x000003FF&((data)<<0))

#define NPP_VI_TVVE_DATA_endian_mask      (0x80000000)
#define NPP_VI_TVVE_DATA_blk_pitch_mask   (0x3FFF0000)
#define NPP_VI_TVVE_DATA_pitch_mask       (0x00003FFF)
#define NPP_VI_TVVE_DATA_blk_pitch(data)  (0x3FFF0000&((data)<<16))
#define NPP_VI_TVVE_DATA_pitch(data)      (0x00003FFF&((data)<<0))

#define NPP_VI_TVVE_HEAD_LU_str_addr(data)  (0xFFFFFFFF&((data)<<0))
#define NPP_VI_TVVE_HEAD_CH_str_addr(data)  (0xFFFFFFFF&((data)<<0))
#define NPP_VI_TVVE_DATA_LU_str_addr(data)  (0xFFFFFFFF&((data)<<0))
#define NPP_VI_TVVE_DATA_CH_str_addr(data)  (0xFFFFFFFF&((data)<<0))

#define NPP_DMA_PRT_MODE_mb(data)  (0x00000007&((data)<<0))

//FC
#define NPP_FC_go_shift (1)
#define NPP_FC_go(data) (0x00000002&((data)<<1))
#define NPP_FC_write_data(data) (0x00000001&((data)<<0))

//INTEN
#define NPP_INTEN_fin_shift (1)
#define NPP_INTEN_vi_tvve_core(data)  (0x00000004&((data)<<2))
#define NPP_INTEN_fin(data, id) ((id) <= 1 ? (0x00000002&((data)<<1)) : (0x00000010&((data)<<4)))
#define NPP_INTEN_write_data(data) (0x00000001&((data)<<0))
// INTST
#define NPP_INTST_vi_tvve_core_mask  (0x00000004)
#define NPP_INTST_fin_mask           (0x00000002)
#define NPP_INTST_vi_tvve_core(data) (0x00000004&((data)<<2))
#define NPP_INTST_fin(data)          (0x00000002&((data)<<1))
#define NPP_INTST_write_data(data)   (0x00000001&((data)<<0))

// VI setting
#define NPP_VI_yuv_packed_mask    (0x10000000)
#define NPP_VI_source_mask        (0x02000000)
#define NPP_VI_nv21_mask          (0x01000000)
#define NPP_VI_ppc10b_mask        (0x00400000)
#define NPP_VI_vhs_order_mask     (0x00080000)
#define NPP_VI_st_mask            (0x00040000)
#define NPP_VI_cu_mask            (0x00002000)
#define NPP_VI_f422_mask          (0x00001000)
#define NPP_VI_chu_1_mask         (0x00000800)
#define NPP_VI_chu_0_mask         (0x00000400)
#define NPP_VI_vs_mask            (0x00000200)
#define NPP_VI_vsdn_mask          (0x00000100)
#define NPP_VI_vs_yodd_mask       (0x00000080)
#define NPP_VI_vs_codd_mask       (0x00000040)
#define NPP_VI_hs_mask            (0x00000020)
#define NPP_VI_hs_yodd_mask       (0x00000010)
#define NPP_VI_hs_codd_mask       (0x00000008)
#define NPP_VI_topfield_mask      (0x00000004)
#define NPP_VI_dmaweave_mask      (0x00000002)

#define NPP_VI_yuv_packed_src(data) ((0x10000000&(data))>>28)
#define NPP_VI_st_src(data)         ((0x00040000&(data))>>18)
#define NPP_VI_f422_src(data)       ((0x00001000&(data))>>12)
#define NPP_VI_topfield_src(data)   ((0x00000004&(data))>>2)
#define NPP_VI_dmaweave_src(data)   ((0x00000002&(data))>>1)
#define NPP_VI_write_data(data)     (0x00000001&((data)<<0))

//DMA weave and current
#define NPP_VI_SEQ_SA_W_Y_a(data) (0xFFFFFFFF&((data)<<0))
#define NPP_VI_SEQ_SA_W_C_a(data) (0xFFFFFFFF&((data)<<0))
#define NPP_VI_SEQ_SA_C_Y_a(data) (0xFFFFFFFF&((data)<<0))
#define NPP_VI_SEQ_SA_C_C_a(data) (0xFFFFFFFF&((data)<<0))

// DMA pitch
#define NPP_VI_SEQ_PITCH_C_Y_p(data) (0x0000FFFF&((data)<<0))
#define NPP_VI_SEQ_PITCH_C_C_p(data) (0x0000FFFF&((data)<<0))

// VI size
#define NPP_VI_SIZE_w(data)       (0x01FFF000&((data)<<12))
#define NPP_VI_SIZE_h(data)       (0x00000FFF&((data)<<0))

// VI PACKED
#define NPP_VI_PACKED_fmt(data) (0x00000003&((data)<<0))

// Scaler
#define NPP_VI_VSYI_offset_mask  (0x3FFF0000)
#define NPP_VI_VSCI_offset_mask  (0x3FFF0000)
#define NPP_VI_HSI_offset_mask   (0x1FFF0000)
#define NPP_VI_VSYI_offset(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_VSYI_phase(data)  (0x0000FFFF&((data)<<0))
#define NPP_VI_VSCI_offset(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_VSCI_phase(data)  (0x0000FFFF&((data)<<0))
#define NPP_VI_HSI_offset(data)  (0x1FFF0000&((data)<<16))
#define NPP_VI_HSI_phase(data)   (0x0000FFFF&((data)<<0))

#define NPP_VI_VSYI_get_offset(data) ((0x3FFF0000&(data))>>16)
#define NPP_VI_VSYI_get_phase(data)  ((0x0000FFFF&(data))>>0)
#define NPP_VI_VSCI_get_offset(data) ((0x3FFF0000&(data))>>16)
#define NPP_VI_VSCI_get_phase(data)  ((0x0000FFFF&(data))>>0)
#define NPP_VI_HSI_get_offset(data)  ((0x1FFF0000&(data))>>16)
#define NPP_VI_HSI_get_phase(data)   ((0x0000FFFF&(data))>>0)

#define NPP_VI_VSYC_c1(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_VSYC_c0(data) (0x00003FFF&((data)<<0))
#define NPP_VI_VSCC_c1(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_VSCC_c0(data) (0x00003FFF&((data)<<0))
#define NPP_VI_HSYC_c1(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_HSYC_c0(data) (0x00003FFF&((data)<<0))
#define NPP_VI_HSCC_c1(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_HSCC_c0(data) (0x00003FFF&((data)<<0))

#define NPP_VI_VSD_H_out(data) (0x00000FFF&((data)<<0))
#define NPP_VI_HSD_W_out(data) (0x00001FFF&((data)<<0))

#define NPP_VI_VSD_delta(data) (0x001FFFFF&((data)<<0))
#define NPP_VI_HSD_delta(data) (0x001FFFFF&((data)<<0))

// Color space conversion
#define NPP_VI_CC1_m01(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_CC1_m00(data) (0x00003FFF&((data)<<0))
#define NPP_VI_CC2_m10(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_CC2_m02(data) (0x00003FFF&((data)<<0))
#define NPP_VI_CC3_m12(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_CC3_m11(data) (0x00003FFF&((data)<<0))
#define NPP_VI_CC4_m21(data) (0x3FFF0000&((data)<<16))
#define NPP_VI_CC4_m20(data) (0x00003FFF&((data)<<0))
#define NPP_VI_CC5_m22(data) (0x00003FFF&((data)<<0))
#define NPP_VI_CC6_a1(data)  (0x07FF0000&((data)<<16))
#define NPP_VI_CC6_a0(data)  (0x000007FF&((data)<<0))
#define NPP_VI_CC7_a2(data)  (0x000007FF&((data)<<0))

//Normalization
#define NPP_NORM_sign(data) (0x00000001&((data)<<0))
#define NPP_NORM_mean(data, id)  ((id) <= 1 ? (0x000003FF&(((data)<<2)<<0)) : (0x07FFFFFF&(((data)<<2)<<0))) // to 10-bit scale
#define NPP_NORM_scale_8bit(data, id) ((id) <= 1 ? (0x07FFFE00&((data)<<10)) : ((0x01FFFFFF&((data)<<0))>>2))
#define NPP_NORM_scale_16bit(data) ((0x01FFFFFF&((data)<<0))<<6)
#define NPP_NORM_max(data) (0x0000FFFF&((data)<<0))
#define NPP_NORM_min(data) (0x0000FFFF&((data)<<0))

// WB
#define NPP_WB_bit_mask (0x00000010)
#define NPP_WB_yuv_mask (0x00000008)
#define NPP_WB_fmt_mask (0x00000002)
#define NPP_WB_fmt_src(data) ((0x00000002&(data))>>1)
#define NPP_WB_write_data(data) (0x00000001&((data)<<0))
#define NPP_WB_SEQ_SA_R_a(data) (0xFFFFFFFF&((data)<<0))
#define NPP_WB_SEQ_SA_G_a(data) (0xFFFFFFFF&((data)<<0))
#define NPP_WB_SEQ_SA_B_a(data) (0xFFFFFFFF&((data)<<0))

// WB pitch
#define NPP_WB_SEQ_PITCH_p(data) (0x0000FFFF&((data)<<0))

//TVVE
#define NPP_VI_TVVE_new_packing_mask  (0x00000080)
#define NPP_VI_TVVE_lossy_en_mask     (0x00000004)
#define NPP_VI_TVVE_bypass_en_mask    (0x00000002)
#define NPP_VI_TVVE_bpp_mask          (0x00000001)
#define NPP_VI_TVVE_new_packing(data) (0x00000001&((data)<<7))
#define NPP_VI_TVVE_lossy_en(data)    (0x00000004&((data)<<2))
#define NPP_VI_TVVE_bypass_en(data)   (0x00000002&((data)<<1))
#define NPP_VI_TVVE_bpp(data)         (0x00000001&((data)<<0))

// Npp status
#define NPP_STATUS_OK              (0x00000000)
#define NPP_STATUS_INVALID_REQUEST (0x00000001)
#define NPP_STATUS_FAILED          (0x000000FF)

struct NPP_VI_VSYC_STR {
	unsigned int c1:16;
	unsigned int c0:16;
};

struct NPP_VI_VSCC_STR {
	unsigned int c1:16;
	unsigned int c0:16;
};

struct NPP_VI_HSYC_STR {
	unsigned int c1:16;
	unsigned int c0:16;
};

struct NPP_VI_HSCC_STR {
	unsigned int c1:16;
	unsigned int c0:16;
};

enum NPP_SOURCE_TYPE {
	SOURCE_LINEAR_MODE = 0,
	SOURCE_VE_MODE
};

enum NPP_YUV_FORMAT {
	PACKED_YUYV = 0,
	PACKED_YVYU,
	PACKED_UYVY,
	PACKED_VYUY,
	NV12_UV,
	NV21_VU,
	NV16
};

enum NPP_YUV_SAMPLE {
	YUV_420 = 0,
	YUV_422
};

enum NPP_WB_FORMAT {
	RGB_MODE = 0,
	YUV_MODE
};

enum NPP_RGB_FORMAT {
	THREE_PLANE_RGB = 0,
	SINGLE_PLANE_RGB
};

enum NPP_RGB_LEVEL {
	POSITIVE_LEVEL = 0, // 0 ~ 255
	NEGATIVE_LEVEL      // -128 ~ 127
};

enum NPP_RGB_BIT_MODE {
	MODE_8bit = 0,
	MODE_16bit
};

enum NPP_COMPRESS_MODE {
	LOSSLESS_MODE = 0,
	LOSSY_MODE
};

struct Npp_SrcCropInfo {
	unsigned int win_x; // crop x position
	unsigned int win_y; // crop y position
	unsigned int win_w; // crop window width and height
	unsigned int win_h; // if no crop, set all of them are 0
};

struct Npp_InputImageFormat {
	enum NPP_SOURCE_TYPE source;    // VE mode means the TVVE compressed data;
	// normal case is linear mode
	enum NPP_YUV_FORMAT yuv_format;
	enum NPP_YUV_SAMPLE yuv_sample;
	unsigned char bit_depth;   // 8 bits-per-pixel or 10 bits-per-pixel
	unsigned short src_width;  // must be even number due to 4:2:2 and 4:2:0 format
	unsigned short src_height;
	unsigned short pitch;      // must be 16-byte alignment
};

struct Npp_InputBufInfo {
	enum NPP_IMAGE_SCAN_MODE scan_mode;
	unsigned long top_filed_y_addr;
	// all of them are physical addresses, must be 16-byte alignment
	unsigned long top_filed_c_addr;     // progressive ONLY sets the top_filed y and c address
	unsigned long bottom_filed_y_addr;
	// interlace needs to set top_filed and bottom_filed address
	unsigned long bottom_filed_c_addr;  // yuv packed ONLY sets the top_filed y address
};

struct Npp_OutputImageFormat {
	enum NPP_WB_FORMAT wb_format;
	enum NPP_RGB_FORMAT rgb_format;
	enum NPP_RGB_LEVEL rgb_level; //unsigned output or signed output
	enum NPP_RGB_BIT_MODE rgb_bit_mode; //8 bit or 16 bit output
	unsigned short tgt_width;
	unsigned short tgt_height;
};

struct Npp_OutputBufInfo {
	unsigned long r_addr;  //all of them are physical addresses, must be 16-byte alignment
	unsigned long g_addr;  //if rgb_format is single-plane RGB, ONLY sets r_addr
	unsigned long b_addr;
};

struct Npp_ImageScalingInfo {
	struct Npp_SrcCropInfo crop_pos;
	unsigned int input_width;
	unsigned int input_height;
	unsigned int output_width;
	unsigned int output_height;
};

struct Npp_MIPICSI_ImageFormat {
	unsigned int header_pitch;
	unsigned int data_pitch;
	unsigned char qlevel_queue_sel_y;
	unsigned char qlevel_queue_sel_c;
	unsigned int width;
	unsigned int height;
};

struct Npp_MIPI_CSI_InputBufInfo {
	uint64_t y_header_addr;  // all of them are physical addresses, must be 16-byte alignment
	uint64_t c_header_addr;
	uint64_t y_data_addr;
	uint64_t c_data_addr;
};

unsigned int NPP_NPP_HwClock(unsigned char enable_clock, enum TARGET_SOC target_soc);
unsigned int NPP_Init(enum TARGET_SOC target_soc);
unsigned int NPP_UnInit(enum TARGET_SOC target_soc);
unsigned int NPP_Set_InputImageFormat(struct Npp_InputImageFormat *src_input, enum TARGET_SOC target_soc);
unsigned int NPP_Set_InputDataBuffer(struct Npp_InputBufInfo *input_buffer, enum TARGET_SOC target_soc);
unsigned int NPP_Set_OutputImageFormat(struct Npp_OutputImageFormat *tgt_output, enum TARGET_SOC target_soc);
unsigned int NPP_Set_OutputRGBFormat(enum NPP_RGB_FORMAT rgb_format, enum TARGET_SOC target_soc);
unsigned int NPP_Set_OutputDataBuffer(struct Npp_OutputBufInfo *output_buffer, enum TARGET_SOC target_soc);
unsigned int NPP_Set_ColorSpaceConversion(enum TARGET_SOC target_soc);
unsigned int NPP_Set_ScalerCoefficient(struct Npp_ImageScalingInfo *image_scaling, enum TARGET_SOC target_soc);
void NPP_Set_EngineGo(enum TARGET_SOC target_soc);
void NPP_Close_INTEN(enum TARGET_SOC target_soc);
void NPP_Clear_INTStatus(unsigned int int_mask, enum TARGET_SOC target_soc);
void NPP_Get_FinishStatus(unsigned char *is_finish, enum TARGET_SOC target_soc);
void NPP_Get_INTStatus(unsigned int *int_status, enum TARGET_SOC target_soc);
void NPP_Get_RegInfo(unsigned int *base_reg_addr, unsigned int *reg_addr_range, enum TARGET_SOC target_soc);

void NPP_Set_MIPICSI_ImageFormat(struct Npp_MIPICSI_ImageFormat *mipi_csi_image_param, enum TARGET_SOC target_soc);
void NPP_Set_MIPICSI_InputDataBuffer(struct Npp_MIPI_CSI_InputBufInfo *mipi_csi_input_buf, enum TARGET_SOC target_soc);
void NPP_Set_MIPICSI_CropWindow(unsigned int crop_x, unsigned int crop_y,
unsigned int crop_width, unsigned int crop_height, enum TARGET_SOC target_soc);

void NPP_Set_SecureVideoPath(unsigned char enable_svp, enum TARGET_SOC target_soc);

#endif

