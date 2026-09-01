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

#ifndef __NPP_REGISTER_H__
#define __NPP_REGISTER_H__

struct RegisterAddresses {
	unsigned int NPP_MODE;
	unsigned int NPP_FC;
	unsigned int NPP_INTEN;
	unsigned int NPP_INTST;
	unsigned int NPP_VI;
	unsigned int NPP_VI_DMA;
	unsigned int NPP_VI_SEQ_SA_W_Y;
	unsigned int NPP_VI_SEQ_SA_W_C;
	unsigned int NPP_VI_SEQ_SA_C_Y;
	unsigned int NPP_VI_SEQ_SA_C_C;
	unsigned int NPP_VI_SEQ_PITCH_C_Y;
	unsigned int NPP_VI_SEQ_PITCH_C_C;

	unsigned int NPP_VI_PACKED;

	unsigned int NPP_VI_RANGE_CTRL;
	unsigned int NPP_VI_RANGE_FR;
	unsigned int NPP_VI_RANGE_Y_TH;
	unsigned int NPP_VI_RANGE_Y_OV;
	unsigned int NPP_VI_RANGE_Y_DET;
	unsigned int NPP_VI_RANGE_C_TH;
	unsigned int NPP_VI_RANGE_C_OV;
	unsigned int NPP_VI_RANGE_C_DET;
	unsigned int NPP_VI_INDEX_RR;
	unsigned int NPP_VI_ALPHA;
	unsigned int NPP_VI_SIZE;
	unsigned int NPP_VI_BUF;

	unsigned int NPP_VI_VSYI;
	unsigned int NPP_VI_VSCI;
	unsigned int NPP_VI_VSD;
	unsigned int NPP_VI_VSD_H;

	unsigned int NPP_VI_VSYC_0;
	unsigned int NPP_VI_VSYC_1;
	unsigned int NPP_VI_VSYC_2;
	unsigned int NPP_VI_VSYC_3;
	unsigned int NPP_VI_VSYC_4;
	unsigned int NPP_VI_VSYC_5;
	unsigned int NPP_VI_VSYC_6;
	unsigned int NPP_VI_VSYC_7;
	unsigned int NPP_VI_VSYC_8;
	unsigned int NPP_VI_VSYC_9;
	unsigned int NPP_VI_VSYC_10;
	unsigned int NPP_VI_VSYC_11;
	unsigned int NPP_VI_VSYC_12;
	unsigned int NPP_VI_VSYC_13;
	unsigned int NPP_VI_VSYC_14;
	unsigned int NPP_VI_VSYC_15;

	unsigned int NPP_VI_VSCC_0;
	unsigned int NPP_VI_VSCC_1;
	unsigned int NPP_VI_VSCC_2;
	unsigned int NPP_VI_VSCC_3;
	unsigned int NPP_VI_VSCC_4;
	unsigned int NPP_VI_VSCC_5;
	unsigned int NPP_VI_VSCC_6;
	unsigned int NPP_VI_VSCC_7;
	unsigned int NPP_VI_VSCC_8;
	unsigned int NPP_VI_VSCC_9;
	unsigned int NPP_VI_VSCC_10;
	unsigned int NPP_VI_VSCC_11;
	unsigned int NPP_VI_VSCC_12;
	unsigned int NPP_VI_VSCC_13;
	unsigned int NPP_VI_VSCC_14;
	unsigned int NPP_VI_VSCC_15;

	unsigned int NPP_VI_HSI;
	unsigned int NPP_VI_HSD;
	unsigned int NPP_VI_HSD_W;

	unsigned int NPP_VI_HSYC_0;
	unsigned int NPP_VI_HSYC_1;
	unsigned int NPP_VI_HSYC_2;
	unsigned int NPP_VI_HSYC_3;
	unsigned int NPP_VI_HSYC_4;
	unsigned int NPP_VI_HSYC_5;
	unsigned int NPP_VI_HSYC_6;
	unsigned int NPP_VI_HSYC_7;
	unsigned int NPP_VI_HSYC_8;
	unsigned int NPP_VI_HSYC_9;
	unsigned int NPP_VI_HSYC_10;
	unsigned int NPP_VI_HSYC_11;
	unsigned int NPP_VI_HSYC_12;
	unsigned int NPP_VI_HSYC_13;
	unsigned int NPP_VI_HSYC_14;
	unsigned int NPP_VI_HSYC_15;
	unsigned int NPP_VI_HSYC_16;
	unsigned int NPP_VI_HSYC_17;
	unsigned int NPP_VI_HSYC_18;
	unsigned int NPP_VI_HSYC_19;
	unsigned int NPP_VI_HSYC_20;
	unsigned int NPP_VI_HSYC_21;
	unsigned int NPP_VI_HSYC_22;
	unsigned int NPP_VI_HSYC_23;
	unsigned int NPP_VI_HSYC_24;
	unsigned int NPP_VI_HSYC_25;
	unsigned int NPP_VI_HSYC_26;
	unsigned int NPP_VI_HSYC_27;
	unsigned int NPP_VI_HSYC_28;
	unsigned int NPP_VI_HSYC_29;
	unsigned int NPP_VI_HSYC_30;
	unsigned int NPP_VI_HSYC_31;

	unsigned int NPP_VI_HSCC_0;
	unsigned int NPP_VI_HSCC_1;
	unsigned int NPP_VI_HSCC_2;
	unsigned int NPP_VI_HSCC_3;
	unsigned int NPP_VI_HSCC_4;
	unsigned int NPP_VI_HSCC_5;
	unsigned int NPP_VI_HSCC_6;
	unsigned int NPP_VI_HSCC_7;
	unsigned int NPP_VI_HSCC_8;
	unsigned int NPP_VI_HSCC_9;
	unsigned int NPP_VI_HSCC_10;
	unsigned int NPP_VI_HSCC_11;
	unsigned int NPP_VI_HSCC_12;
	unsigned int NPP_VI_HSCC_13;
	unsigned int NPP_VI_HSCC_14;
	unsigned int NPP_VI_HSCC_15;

	unsigned int NPP_VI_CC1;
	unsigned int NPP_VI_CC2;
	unsigned int NPP_VI_CC3;
	unsigned int NPP_VI_CC4;
	unsigned int NPP_VI_CC5;
	unsigned int NPP_VI_CC6;
	unsigned int NPP_VI_CC7;
	unsigned int NPP_VI_CC8;
	unsigned int NPP_VI_CC9;
	unsigned int NPP_VI_CC10;

	unsigned int NPP_NORM_R;
	unsigned int NPP_NORM_G;
	unsigned int NPP_NORM_B;
	unsigned int NPP_NORM_M_R;
	unsigned int NPP_NORM_S_R;
	unsigned int NPP_NORM_M_G;
	unsigned int NPP_NORM_S_G;
	unsigned int NPP_NORM_M_B;
	unsigned int NPP_NORM_S_B;
	unsigned int NPP_NORM_MAX_R;
	unsigned int NPP_NORM_MIN_R;
	unsigned int NPP_NORM_MAX_G;
	unsigned int NPP_NORM_MIN_G;
	unsigned int NPP_NORM_MAX_B;
	unsigned int NPP_NORM_MIN_B;
	unsigned int NPP_NORM;

	unsigned int NPP_VI_TVVE;
	unsigned int NPP_VI_TVVE_CROP;
	unsigned int NPP_VI_TVVE_CROP_POS;
	unsigned int NPP_VI_TVVE_HEAD;
	unsigned int NPP_VI_TVVE_DATA;
	unsigned int NPP_VI_TVVE_HEAD_LU;
	unsigned int NPP_VI_TVVE_HEAD_CH;
	unsigned int NPP_VI_TVVE_DATA_LU;
	unsigned int NPP_VI_TVVE_DATA_CH;
	unsigned int NPP_VI_TVVE_CORE_ERR;

	unsigned int NPP_VI_DMY;
	unsigned int NPP_WB;
	unsigned int NPP_WB_DMA;
	unsigned int NPP_WB_SEQ_SA_R;
	unsigned int NPP_WB_SEQ_SA_G;
	unsigned int NPP_WB_SEQ_SA_B;
	unsigned int NPP_WB_SEQ_PITCH;
	unsigned int NPP_WB_TPC;

	unsigned int NPP_DMA;
	unsigned int NPP_DMA_PRT_MODE;
	unsigned int NPP_DMA_PV;
	unsigned int NPP_DMA_PW;
	unsigned int NPP_DMA_DCWBUF;
	unsigned int NPP_DMA_UPDATE;
	unsigned int NPP_DMA_VI_Y0;
	unsigned int NPP_DMA_VI_Y1;
	unsigned int NPP_DMA_VI_C0;
	unsigned int NPP_DMA_VI_C1;
	unsigned int NPP_DMA_WB_R;
	unsigned int NPP_DMA_WB_G;
	unsigned int NPP_DMA_WB_B;
	unsigned int NPP_DMA_METER0;
	unsigned int NPP_DMA_METER1;
	unsigned int NPP_DMA_METER2;
	unsigned int NPP_DMA_RST;

	unsigned int NPP_SOFT;
	unsigned int NPP_SRAM_LS;
	unsigned int NPP_SRAM_SD;
	unsigned int NPP_DEBUG;
	unsigned int NPP_DBG;
	unsigned int NPP_CMDQ;
	unsigned int NPP_TIMEOUT;
	unsigned int NPP_SRAM_RM;
};

const struct RegisterAddresses *get_register_addresses(int id);

#endif

