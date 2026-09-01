/*
 * tpdemux_reg.c
 *
 * Copyright (C) 2020 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
 #include "tpdemux_reg.h"

static void _mapping_tp(int tp_index, struct rtk_tp_reg *reg,
			int __maybe_unused model_id)
{
	// modelTP()
	switch(tp_index)
	{
	case DMX_TP_A_0:
		reg->reg_TP_EXT_PID_CNTL     = reg->base + TP_0_EXT_PID_CNTL;
		break;

	case DMX_TP_A_1:
		reg->reg_TP_EXT_PID_CNTL     = reg->base + TP_1_EXT_PID_CNTL;
		break;

	case DMX_TP_B_0:
		reg->reg_TP_EXT_PID_CNTL     = reg->base + TPB_0_EXT_PID_CNTL;
		break;

	case DMX_TP_B_1:
		reg->reg_TP_EXT_PID_CNTL     = reg->base + TPB_1_EXT_PID_CNTL;
		break;
	}

	// HardwareEnvSetup()
	switch(tp_index)
	{
		case DMX_TP_A_0:
		case DMX_TP_A_1:
			reg->reg_TP_PID_PART = reg->base + TP_PID_PART;
			reg->reg_TP_CRC_INIT = reg->base + TP_CRC_INIT;
			break;
		case DMX_TP_B_0:
		case DMX_TP_B_1:
			reg->reg_TP_PID_PART = reg->base + TPB_PID_PART;
			reg->reg_TP_CRC_INIT = reg->base + TPB_CRC_INIT;
			break;
		default:
			break;
	}
}

// TPFramer::RegInit
static void _mapping_framer(int tp_index, struct rtk_tp_reg *reg, int model_id)
{
	switch(tp_index)        // init register
	{
	case DMX_TP_A_0:
		reg->reg_TF_CNTL        = reg->base + TP_TF0_CNTL;
		//reg->reg_TF_CNTL2       = reg->base + TP_TF0_CNTL2;
		reg->reg_TF_CNT         = reg->base + TP_TF0_CNT;
		reg->reg_TF_DRP_CNT     = reg->base + TP_TF0_DRP_CNT;
		reg->reg_TF_ERR_CNT     = reg->base + TP_TF0_ERR_CNT;
		reg->reg_TF_FRMCFG      = reg->base + TP_TF0_FRMCFG;
		reg->reg_TF_INT         = reg->base + TP_TF0_INT;
		reg->reg_TF_INT_EN      = reg->base + TP_TF0_INT_EN;
		reg->reg_TF_STRM_ID_0   = reg->base + TP_TF0_STRM_ID_0;
		reg->reg_TF_STRM_ID_1   = reg->base + TP_TF0_STRM_ID_1;
		reg->reg_TF_STRM_ID_2   = reg->base + TP_TF0_STRM_ID_2;
		reg->reg_TF_STRM_ID_3   = reg->base + TP_TF0_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TP_TF0_STRM_ID_VAL;
		reg->reg_TP_DES_CNTL    = reg->base + TP_TP0_DES_CNTL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TP_TF0_CTRL_SWC;
		//reg->reg_TP_TF_SYNA_CTRL   = reg->base + TP_TF0_SYNA_CTRL;
		//reg->reg_TP_TF_SYNA_INT    = reg->base + TP_TF0_SYNA_INT;

		//reg->reg_TP_SWC_DMY_A = reg->base + TP_SWC_DMY_A;

		if (model_id == TP_HANK) {
			reg->reg_TP_SWC_DMY_A = reg->base + TP_SWC_DMY_A;
		} else if (model_id == TP_PARKER) {
			reg->reg_TF_CNTL2       = reg->base + TP_TF0_CNTL2;
			//reg->reg_TP_TF_SYNA_CTRL   = reg->base + TP_TF0_SYNA_CTRL;
			//reg->reg_TP_TF_SYNA_INT    = reg->base + TP_TF0_SYNA_INT;
		}
		break;

	case DMX_TP_A_1:
		reg->reg_TF_CNTL        = reg->base + TP_TF1_CNTL;
		//reg->reg_TF_CNTL2       = reg->base + TP_TF1_CNTL2;
		reg->reg_TF_CNT         = reg->base + TP_TF1_CNT;
		reg->reg_TF_DRP_CNT     = reg->base + TP_TF1_DRP_CNT;
		reg->reg_TF_ERR_CNT     = reg->base + TP_TF1_ERR_CNT;
		reg->reg_TF_FRMCFG      = reg->base + TP_TF1_FRMCFG;
		reg->reg_TF_INT         = reg->base + TP_TF1_INT;
		reg->reg_TF_INT_EN      = reg->base + TP_TF1_INT_EN;
		reg->reg_TF_STRM_ID_0   = reg->base + TP_TF1_STRM_ID_0;
		reg->reg_TF_STRM_ID_1   = reg->base + TP_TF1_STRM_ID_1;
		reg->reg_TF_STRM_ID_2   = reg->base + TP_TF1_STRM_ID_2;
		reg->reg_TF_STRM_ID_3   = reg->base + TP_TF1_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TP_TF1_STRM_ID_VAL;
		reg->reg_TP_DES_CNTL    = reg->base + TP_TP1_DES_CNTL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TP_TF1_CTRL_SWC;
		//reg->reg_TP_TF_SYNA_CTRL   = reg->base + TP_TF1_SYNA_CTRL;
		//reg->reg_TP_TF_SYNA_INT    = reg->base + TP_TF1_SYNA_INT;

		//reg->reg_TP_SWC_DMY_A = reg->base + TP_SWC_DMY_A;

		if (model_id == TP_HANK) {
			reg->reg_TP_SWC_DMY_A = reg->base + TP_SWC_DMY_A;
		} else if (model_id == TP_PARKER) {
			reg->reg_TF_CNTL2       = reg->base + TP_TF1_CNTL2;
			//reg->reg_TP_TF_SYNA_CTRL   = reg->base + TP_TF1_SYNA_CTRL;
			//reg->reg_TP_TF_SYNA_INT    = reg->base + TP_TF1_SYNA_INT;
		}
		break;

	case DMX_TP_B_0:
		reg->reg_TF_CNTL        = reg->base + TPB_TF0_CNTL;
		//reg->reg_TF_CNTL2       = reg->base + TPB_TF0_CNTL2;
		reg->reg_TF_CNT         = reg->base + TPB_TF0_CNT;
		reg->reg_TF_DRP_CNT     = reg->base + TPB_TF0_DRP_CNT;
		reg->reg_TF_ERR_CNT     = reg->base + TPB_TF0_ERR_CNT;
		reg->reg_TF_FRMCFG      = reg->base + TPB_TF0_FRMCFG;
		reg->reg_TF_INT         = reg->base + TPB_TF0_INT;
		reg->reg_TF_INT_EN      = reg->base + TPB_TF0_INT_EN;
		reg->reg_TF_STRM_ID_0   = reg->base + TPB_TF0_STRM_ID_0;
		reg->reg_TF_STRM_ID_1   = reg->base + TPB_TF0_STRM_ID_1;
		reg->reg_TF_STRM_ID_2   = reg->base + TPB_TF0_STRM_ID_2;
		reg->reg_TF_STRM_ID_3   = reg->base + TPB_TF0_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TPB_TF0_STRM_ID_VAL;
		reg->reg_TP_DES_CNTL    = reg->base + TPB_TP0_DES_CNTL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TPB_TF0_CTRL_SWC;
		//reg->reg_TP_TF_SYNA_CTRL   = reg->base + TPB_TF0_SYNA_CTRL;
		//reg->reg_TP_TF_SYNA_INT    = reg->base + TPB_TF0_SYNA_INT;

		//reg->reg_TP_SWC_DMY_A = reg->base + TPB_SWC_DMY_A;

		if (model_id == TP_HANK) {
			reg->reg_TP_SWC_DMY_A = reg->base + TPB_SWC_DMY_A;
		} else if (model_id == TP_PARKER) {
			reg->reg_TF_CNTL2       = reg->base + TPB_TF0_CNTL2;
			//reg->reg_TP_TF_SYNA_CTRL   = reg->base + TPB_TF0_SYNA_CTRL;
			//reg->reg_TP_TF_SYNA_INT    = reg->base + TPB_TF0_SYNA_INT;
		}
		break;

	case DMX_TP_B_1:
		reg->reg_TF_CNTL        = reg->base + TPB_TF1_CNTL;
		//reg->reg_TF_CNTL2       = reg->base + TPB_TF1_CNTL2;
		reg->reg_TF_CNT         = reg->base + TPB_TF1_CNT;
		reg->reg_TF_DRP_CNT     = reg->base + TPB_TF1_DRP_CNT;
		reg->reg_TF_ERR_CNT     = reg->base + TPB_TF1_ERR_CNT;
		reg->reg_TF_FRMCFG      = reg->base + TPB_TF1_FRMCFG;
		reg->reg_TF_INT         = reg->base + TPB_TF1_INT;
		reg->reg_TF_INT_EN      = reg->base + TPB_TF1_INT_EN;
		reg->reg_TF_STRM_ID_0   = reg->base + TPB_TF1_STRM_ID_0;
		reg->reg_TF_STRM_ID_1   = reg->base + TPB_TF1_STRM_ID_1;
		reg->reg_TF_STRM_ID_2   = reg->base + TPB_TF1_STRM_ID_2;
		reg->reg_TF_STRM_ID_3   = reg->base + TPB_TF1_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TPB_TF1_STRM_ID_VAL;
		reg->reg_TP_DES_CNTL    = reg->base + TPB_TP1_DES_CNTL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TPB_TF1_CTRL_SWC;
		//reg->reg_TP_TF_SYNA_CTRL   = reg->base + TPB_TF1_SYNA_CTRL;
		//reg->reg_TP_TF_SYNA_INT    = reg->base + TPB_TF1_SYNA_INT;

		//reg->reg_TP_SWC_DMY_A = reg->base + TPB_SWC_DMY_A;

		if (model_id == TP_HANK) {
			reg->reg_TP_SWC_DMY_A = reg->base + TPB_SWC_DMY_A;
		} else if (model_id == TP_PARKER) {
			reg->reg_TF_CNTL2       = reg->base + TPB_TF1_CNTL2;
			//reg->reg_TP_TF_SYNA_CTRL   = reg->base + TPB_TF1_SYNA_CTRL;
			//reg->reg_TP_TF_SYNA_INT    = reg->base + TPB_TF1_SYNA_INT;
		}
		break;

	default:
		break;
	}

	reg->reg_TP_KEY_INFO_SWC_0	 = reg->base + TP_KEY_INFO_SWC_0;
	reg->reg_TP_KEY_INFO_SWC_1	 = reg->base + TP_KEY_INFO_SWC_1;
	reg->reg_TP_KEY_INFO_SWC_2	 = reg->base + TP_KEY_INFO_SWC_2;
	reg->reg_TP_KEY_INFO_SWC_3	 = reg->base + TP_KEY_INFO_SWC_3;
	reg->reg_TP_KEY_INFO_SWC_4	 = reg->base + TP_KEY_INFO_SWC_4;
	reg->reg_TP_KEY_INFO_SWC_5	 = reg->base + TP_KEY_INFO_SWC_5;
	reg->reg_TP_KEY_INFO_SWC_6	 = reg->base + TP_KEY_INFO_SWC_6;
	reg->reg_TP_KEY_INFO_SWC_7	 = reg->base + TP_KEY_INFO_SWC_7;
	reg->reg_TP_KEY_INFO_SWC_8  = reg->base + TP_KEY_INFO_SWC_8;
	reg->reg_TP_KEY_INFO_SWC_9  = reg->base + TP_KEY_INFO_SWC_9;
	reg->reg_TP_KEY_INFO_SWC_A  = reg->base + TP_KEY_INFO_SWC_A;
	reg->reg_TP_KEY_INFO_SWC_B  = reg->base + TP_KEY_INFO_SWC_B;

	reg->reg_TP_KEY_CTRL_SWC	 = reg->base + TP_KEY_CTRL_SWC;
	reg->reg_TP_KEY_HEADER_SWC  = reg->base + TP_KEY_HEADER_SWC;
	reg->reg_TP_KEY_MASK_SWC	= reg->base +  TP_KEY_MASK_SWC;

	if (model_id == TP_PARKER) {
		reg->reg_TP_KEY_INFO_REE_0 = reg->base + TP_KEY_INFO_REE_0;
		reg->reg_TP_KEY_INFO_REE_1 = reg->base + TP_KEY_INFO_REE_1;
		reg->reg_TP_KEY_INFO_REE_2 = reg->base + TP_KEY_INFO_REE_2;
		reg->reg_TP_KEY_INFO_REE_3 = reg->base + TP_KEY_INFO_REE_3;
		reg->reg_TP_KEY_INFO_REE_4 = reg->base + TP_KEY_INFO_REE_4;
		reg->reg_TP_KEY_INFO_REE_5 = reg->base + TP_KEY_INFO_REE_5;
		reg->reg_TP_KEY_INFO_REE_6 = reg->base + TP_KEY_INFO_REE_6;
		reg->reg_TP_KEY_INFO_REE_7 = reg->base + TP_KEY_INFO_REE_7;

		reg->reg_TP_KEY_CTRL_NWC   = reg->base + TP_KEY_CTRL_NWC;
		reg->reg_TP_KEY_HEADER_REE = reg->base + TP_KEY_HEADER_REE;
		reg->reg_TP_KEY_MASK_REE   = reg->base + TP_KEY_MASK_REE;
	}
}

// TPMMBuffer() TPBuffer()
static void _mapping_buffer(int tp_index, struct rtk_tp_reg *reg,
				int __maybe_unused model_id)
{

	// mmbuffer
	switch (tp_index)
	{
		case DMX_TP_A_0:
			reg->reg_TP_M2M_RING_LIMIT	= reg->base + TP0_M2M_RING_LIMIT;
			reg->reg_TP_M2M_RING_BASE		= reg->base + TP0_M2M_RING_BASE;
			reg->reg_TP_M2M_RING_RP		= reg->base + TP0_M2M_RING_RP;
			reg->reg_TP_M2M_RING_WP		= reg->base + TP0_M2M_RING_WP;
			reg->reg_TP_M2M_RING_CTRL		= reg->base + TP0_M2M_RING_CTRL;
			break;
		case DMX_TP_A_1:
			reg->reg_TP_M2M_RING_LIMIT	= reg->base + TP1_M2M_RING_LIMIT;
			reg->reg_TP_M2M_RING_BASE		= reg->base + TP1_M2M_RING_BASE;
			reg->reg_TP_M2M_RING_RP		= reg->base + TP1_M2M_RING_RP;
			reg->reg_TP_M2M_RING_WP		= reg->base + TP1_M2M_RING_WP;
			reg->reg_TP_M2M_RING_CTRL		= reg->base + TP1_M2M_RING_CTRL;
			break;
		case DMX_TP_B_0:
			reg->reg_TP_M2M_RING_LIMIT	= reg->base + TPB_TP0_M2M_RING_LIMIT;
			reg->reg_TP_M2M_RING_BASE		= reg->base + TPB_TP0_M2M_RING_BASE;
			reg->reg_TP_M2M_RING_RP		= reg->base + TPB_TP0_M2M_RING_RP;
			reg->reg_TP_M2M_RING_WP		= reg->base + TPB_TP0_M2M_RING_WP;
			reg->reg_TP_M2M_RING_CTRL		= reg->base + TPB_TP0_M2M_RING_CTRL;
			break;
		case DMX_TP_B_1:
			reg->reg_TP_M2M_RING_LIMIT	= reg->base + TPB_TP1_M2M_RING_LIMIT;
			reg->reg_TP_M2M_RING_BASE		= reg->base + TPB_TP1_M2M_RING_BASE;
			reg->reg_TP_M2M_RING_RP		= reg->base + TPB_TP1_M2M_RING_RP;
			reg->reg_TP_M2M_RING_WP		= reg->base + TPB_TP1_M2M_RING_WP;
			reg->reg_TP_M2M_RING_CTRL		= reg->base + TPB_TP1_M2M_RING_CTRL;
			break;
		default:
			break;
	}

	// tpbuffer
	switch (tp_index)
	{
		case DMX_TP_A_0:
			reg->reg_TP_RING_CTRL		= reg->base + TP_RING_CTRL;
			reg->reg_TP_RING_LIMIT	= reg->base + TP_RING_LIMIT;
			reg->reg_TP_RING_BASE		= reg->base + TP_RING_BASE;
			reg->reg_TP_RING_RP		= reg->base + TP_RING_RP;
			reg->reg_TP_RING_WP		= reg->base + TP_RING_WP;
			reg->reg_TP_FULLNESS		= reg->base + TP_FULLNESS;
			reg->reg_TP_THRESHOLD		= reg->base + TP_THRESHOLD;
			reg->reg_TP_RING_FULL_INT_0	= reg->base + TP_RING_FULL_INT_0;
			reg->reg_TP_RING_FULL_INT_1	= reg->base + TP_RING_FULL_INT_1;
			reg->reg_TP_RING_FULL_INT_2	= reg->base + TP_RING_FULL_INT_2;
			reg->reg_TP_RING_FULL_INT_3	= reg->base + TP_RING_FULL_INT_3;
			reg->reg_TP_RING_AVAIL_INT_0	= reg->base + TP_RING_AVAIL_INT_0;
			reg->reg_TP_RING_AVAIL_INT_1	= reg->base + TP_RING_AVAIL_INT_1;
			reg->reg_TP_RING_AVAIL_INT_2	= reg->base + TP_RING_AVAIL_INT_2;
			reg->reg_TP_RING_AVAIL_INT_3	= reg->base + TP_RING_AVAIL_INT_3;
			reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base + TP_RING_AVAIL_INT_EN_0;
			reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base + TP_RING_AVAIL_INT_EN_1;
			reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base + TP_RING_AVAIL_INT_EN_2;
			reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base + TP_RING_AVAIL_INT_EN_3;
			reg->reg_TP_RING_FULL_INT_EN_0 = reg->base + TP_RING_FULL_INT_EN_0;
			reg->reg_TP_RING_FULL_INT_EN_1 = reg->base + TP_RING_FULL_INT_EN_1;
			reg->reg_TP_RING_FULL_INT_EN_2 = reg->base + TP_RING_FULL_INT_EN_2;
			reg->reg_TP_RING_FULL_INT_EN_3 = reg->base + TP_RING_FULL_INT_EN_3;
			break;
		case DMX_TP_A_1:
			reg->reg_TP_RING_CTRL		= reg->base + TP_RING_CTRL;
			reg->reg_TP_RING_LIMIT	= reg->base + TP_RING_LIMIT;
			reg->reg_TP_RING_BASE		= reg->base + TP_RING_BASE;
			reg->reg_TP_RING_RP		= reg->base + TP_RING_RP;
			reg->reg_TP_RING_WP		= reg->base + TP_RING_WP;
			reg->reg_TP_FULLNESS		= reg->base + TP_FULLNESS;
			reg->reg_TP_THRESHOLD		= reg->base + TP_THRESHOLD;
			reg->reg_TP_RING_FULL_INT_0	= reg->base + TP_RING_FULL_INT_0;
			reg->reg_TP_RING_FULL_INT_1	= reg->base + TP_RING_FULL_INT_1;
			reg->reg_TP_RING_FULL_INT_2	= reg->base + TP_RING_FULL_INT_2;
			reg->reg_TP_RING_FULL_INT_3	= reg->base + TP_RING_FULL_INT_3;
			reg->reg_TP_RING_AVAIL_INT_0	= reg->base + TP_RING_AVAIL_INT_0;
			reg->reg_TP_RING_AVAIL_INT_1	= reg->base + TP_RING_AVAIL_INT_1;
			reg->reg_TP_RING_AVAIL_INT_2	= reg->base + TP_RING_AVAIL_INT_2;
			reg->reg_TP_RING_AVAIL_INT_3	= reg->base + TP_RING_AVAIL_INT_3;
			reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base + TP_RING_AVAIL_INT_EN_0;
			reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base + TP_RING_AVAIL_INT_EN_1;
			reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base + TP_RING_AVAIL_INT_EN_2;
			reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base + TP_RING_AVAIL_INT_EN_3;
			reg->reg_TP_RING_FULL_INT_EN_0 = reg->base + TP_RING_FULL_INT_EN_0;
			reg->reg_TP_RING_FULL_INT_EN_1 = reg->base + TP_RING_FULL_INT_EN_1;
			reg->reg_TP_RING_FULL_INT_EN_2 = reg->base + TP_RING_FULL_INT_EN_2;
			reg->reg_TP_RING_FULL_INT_EN_3 = reg->base + TP_RING_FULL_INT_EN_3;
			break;
		case DMX_TP_B_0:
			reg->reg_TP_RING_CTRL		= reg->base + TPB_RING_CTRL;
			reg->reg_TP_RING_LIMIT	= reg->base + TPB_RING_LIMIT;
			reg->reg_TP_RING_BASE		= reg->base + TPB_RING_BASE;
			reg->reg_TP_RING_RP		= reg->base + TPB_RING_RP;
			reg->reg_TP_RING_WP		= reg->base + TPB_RING_WP;
			reg->reg_TP_FULLNESS		= reg->base + TPB_FULLNESS;
			reg->reg_TP_THRESHOLD		= reg->base + TPB_THRESHOLD;
			reg->reg_TP_RING_FULL_INT_0	= reg->base + TPB_RING_FULL_INT_0;
			reg->reg_TP_RING_FULL_INT_1	= reg->base + TPB_RING_FULL_INT_1;
			reg->reg_TP_RING_FULL_INT_2	= reg->base + TPB_RING_FULL_INT_2;
			reg->reg_TP_RING_FULL_INT_3	= reg->base + TPB_RING_FULL_INT_3;
			reg->reg_TP_RING_AVAIL_INT_0	= reg->base + TPB_RING_AVAIL_INT_0;
			reg->reg_TP_RING_AVAIL_INT_1	= reg->base + TPB_RING_AVAIL_INT_1;
			reg->reg_TP_RING_AVAIL_INT_2	= reg->base + TPB_RING_AVAIL_INT_2;
			reg->reg_TP_RING_AVAIL_INT_3	= reg->base + TPB_RING_AVAIL_INT_3;
			reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base + TPB_RING_AVAIL_INT_EN_0;
			reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base + TPB_RING_AVAIL_INT_EN_1;
			reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base + TPB_RING_AVAIL_INT_EN_2;
			reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base + TPB_RING_AVAIL_INT_EN_3;
			reg->reg_TP_RING_FULL_INT_EN_0 = reg->base + TPB_RING_FULL_INT_EN_0;
			reg->reg_TP_RING_FULL_INT_EN_1 = reg->base + TPB_RING_FULL_INT_EN_1;
			reg->reg_TP_RING_FULL_INT_EN_2 = reg->base + TPB_RING_FULL_INT_EN_2;
			reg->reg_TP_RING_FULL_INT_EN_3 = reg->base + TPB_RING_FULL_INT_EN_3;
			break;
		case DMX_TP_B_1:
			reg->reg_TP_RING_CTRL		= reg->base + TPB_RING_CTRL;
			reg->reg_TP_RING_LIMIT	= reg->base + TPB_RING_LIMIT;
			reg->reg_TP_RING_BASE		= reg->base + TPB_RING_BASE;
			reg->reg_TP_RING_RP		= reg->base + TPB_RING_RP;
			reg->reg_TP_RING_WP		= reg->base + TPB_RING_WP;
			reg->reg_TP_FULLNESS		= reg->base + TPB_FULLNESS;
			reg->reg_TP_THRESHOLD		= reg->base + TPB_THRESHOLD;
			reg->reg_TP_RING_FULL_INT_0	= reg->base + TPB_RING_FULL_INT_0;
			reg->reg_TP_RING_FULL_INT_1	= reg->base + TPB_RING_FULL_INT_1;
			reg->reg_TP_RING_FULL_INT_2	= reg->base + TPB_RING_FULL_INT_2;
			reg->reg_TP_RING_FULL_INT_3	= reg->base + TPB_RING_FULL_INT_3;
			reg->reg_TP_RING_AVAIL_INT_0	= reg->base + TPB_RING_AVAIL_INT_0;
			reg->reg_TP_RING_AVAIL_INT_1	= reg->base + TPB_RING_AVAIL_INT_1;
			reg->reg_TP_RING_AVAIL_INT_2	= reg->base + TPB_RING_AVAIL_INT_2;
			reg->reg_TP_RING_AVAIL_INT_3	= reg->base + TPB_RING_AVAIL_INT_3;
			reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base + TPB_RING_AVAIL_INT_EN_0;
			reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base + TPB_RING_AVAIL_INT_EN_1;
			reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base + TPB_RING_AVAIL_INT_EN_2;
			reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base + TPB_RING_AVAIL_INT_EN_3;
			reg->reg_TP_RING_FULL_INT_EN_0 = reg->base + TPB_RING_FULL_INT_EN_0;
			reg->reg_TP_RING_FULL_INT_EN_1 = reg->base + TPB_RING_FULL_INT_EN_1;
			reg->reg_TP_RING_FULL_INT_EN_2 = reg->base + TPB_RING_FULL_INT_EN_2;
			reg->reg_TP_RING_FULL_INT_EN_3 = reg->base + TPB_RING_FULL_INT_EN_3;
			break;
		default:
			break;
	}
}

// TPPID()
static void _mapping_pid(int tp_index, struct rtk_tp_reg *reg, int model_id)
{

	switch(tp_index)
	{
		case DMX_TP_A_0:
			reg->reg_TP_PID_CTRL	= reg->base + TP_PID_CTRL;
			reg->reg_TP_PID_DATA	= reg->base + TP_PID_DATA;
			reg->reg_TP_PID_DATA2	= reg->base + TP_PID_DATA2;
			reg->reg_TP_PID_DATA3	= reg->base + TP_PID_DATA3;

			//reg->reg_TP_PID_DATA4 = reg->base + TP_PID_DATA4;
			//reg->reg_TP_PID_DATA5 = reg->base + TP_PID_DATA5;
			//reg->reg_TP_PID_DATA6 = reg->base + TP_PID_DATA6;

			reg->reg_TP_PCRCtrlReg = reg->base + TP0_PCR_CTL;
			reg->reg_TP_PCR_LATCH	= reg->base + TP_PCR_LATCH;
			reg->reg_TP_PCR_SYSTEM = reg->base + TP_PCR_SYSTEM;
			reg->reg_TP_PCR_BASE	= reg->base + TP_PCR_BASE;
			reg->reg_TP_PCR_EXT	= reg->base + TP_PCR_EXT;

			if (model_id == TP_PARKER) {
				reg->reg_TP_PID_DATA4 = reg->base + TP_PID_DATA4;
				reg->reg_TP_PID_DATA5 = reg->base + TP_PID_DATA5;
				reg->reg_TP_PID_DATA6 = reg->base + TP_PID_DATA6;
			}

			break;
		case DMX_TP_A_1:
			reg->reg_TP_PID_CTRL	= reg->base + TP_PID_CTRL;
			reg->reg_TP_PID_DATA	= reg->base + TP_PID_DATA;
			reg->reg_TP_PID_DATA2	= reg->base + TP_PID_DATA2;
			reg->reg_TP_PID_DATA3	= reg->base + TP_PID_DATA3;

			//reg->reg_TP_PID_DATA4 = reg->base + TP_PID_DATA4;
			//reg->reg_TP_PID_DATA5 = reg->base + TP_PID_DATA5;
			//reg->reg_TP_PID_DATA6 = reg->base + TP_PID_DATA6;

			reg->reg_TP_PCRCtrlReg = reg->base + TP0_PCR_CTL;
			reg->reg_TP_PCRCtrlReg = reg->base + TP1_PCR_CTL;
			reg->reg_TP_PCR_LATCH	= reg->base + TP_PCR_LATCH;
			reg->reg_TP_PCR_SYSTEM = reg->base + TP_PCR_SYSTEM;
			reg->reg_TP_PCR_BASE	= reg->base + TP_PCR_BASE;
			reg->reg_TP_PCR_EXT	= reg->base + TP_PCR_EXT;
			if (model_id == TP_PARKER) {
				reg->reg_TP_PID_DATA4 = reg->base + TP_PID_DATA4;
				reg->reg_TP_PID_DATA5 = reg->base + TP_PID_DATA5;
				reg->reg_TP_PID_DATA6 = reg->base + TP_PID_DATA6;
			}
			break;
		case DMX_TP_B_0:
			reg->reg_TP_PID_CTRL	= reg->base + TPB_PID_CTRL;
			reg->reg_TP_PID_DATA	= reg->base + TPB_PID_DATA;
			reg->reg_TP_PID_DATA2	= reg->base + TPB_PID_DATA2;
			reg->reg_TP_PID_DATA3	= reg->base + TPB_PID_DATA3;

			//reg->reg_TP_PID_DATA4 = reg->base + TPB_PID_DATA4;
			//reg->reg_TP_PID_DATA5 = reg->base + TPB_PID_DATA5;
			//reg->reg_TP_PID_DATA6 = reg->base + TPB_PID_DATA6;

			reg->reg_TP_PCRCtrlReg = reg->base + TPB0_PCR_CTL;
			reg->reg_TP_PCR_LATCH	= reg->base + TPB_PCR_LATCH;
			reg->reg_TP_PCR_SYSTEM = reg->base + TPB_PCR_SYSTEM;
			reg->reg_TP_PCR_BASE	= reg->base + TPB_PCR_BASE;
			reg->reg_TP_PCR_EXT	= reg->base + TPB_PCR_EXT;

			if (model_id == TP_PARKER) {
				reg->reg_TP_PID_DATA4 = reg->base + TPB_PID_DATA4;
				reg->reg_TP_PID_DATA5 = reg->base + TPB_PID_DATA5;
				reg->reg_TP_PID_DATA6 = reg->base + TPB_PID_DATA6;
			}
			break;
		case DMX_TP_B_1:
			reg->reg_TP_PID_CTRL	= reg->base + TPB_PID_CTRL;
			reg->reg_TP_PID_DATA	= reg->base + TPB_PID_DATA;
			reg->reg_TP_PID_DATA2	= reg->base + TPB_PID_DATA2;
			reg->reg_TP_PID_DATA3	= reg->base + TPB_PID_DATA3;

			//reg->reg_TP_PID_DATA4 = reg->base + TPB_PID_DATA4;
			//reg->reg_TP_PID_DATA5 = reg->base + TPB_PID_DATA5;
			//reg->reg_TP_PID_DATA6 = reg->base + TPB_PID_DATA6;

			reg->reg_TP_PCRCtrlReg = reg->base + TPB1_PCR_CTL;
			reg->reg_TP_PCR_LATCH	= reg->base + TPB_PCR_LATCH;
			reg->reg_TP_PCR_SYSTEM = reg->base + TPB_PCR_SYSTEM;
			reg->reg_TP_PCR_BASE	= reg->base + TPB_PCR_BASE;
			reg->reg_TP_PCR_EXT	= reg->base + TPB_PCR_EXT;

			if (model_id == TP_PARKER) {
				reg->reg_TP_PID_DATA4 = reg->base + TPB_PID_DATA4;
				reg->reg_TP_PID_DATA5 = reg->base + TPB_PID_DATA5;
				reg->reg_TP_PID_DATA6 = reg->base + TPB_PID_DATA6;
			}
			break;
		default:
			break;
	}
}

// TPSEC()
static void _mapping_sec(int tp_index, struct rtk_tp_reg *reg,
			int __maybe_unused model_id)
{

	switch(tp_index)
	{
		case DMX_TP_A_0:
		case DMX_TP_A_1:
			reg->reg_TP_SEC_CTRL  = reg->base + TP_SEC_CTRL;
			reg->reg_TP_SEC_DATA0 = reg->base + TP_SEC_DATA0;
			reg->reg_TP_SEC_DATA1 = reg->base + TP_SEC_DATA1;
			reg->reg_TP_SEC_DATA2 = reg->base + TP_SEC_DATA2;
			reg->reg_TP_SEC_DATA3 = reg->base + TP_SEC_DATA3;
			reg->reg_TP_SEC_DATA4 = reg->base + TP_SEC_DATA4;
			reg->reg_TP_SEC_DATA5 = reg->base + TP_SEC_DATA5;
			reg->reg_TP_SEC_DATA6 = reg->base + TP_SEC_DATA6;
			reg->reg_TP_SEC_DATA7 = reg->base + TP_SEC_DATA7;
			reg->reg_TP_SEC_DATA8 = reg->base + TP_SEC_DATA8;
			reg->reg_TP_SEC_DATA9 = reg->base + TP_SEC_DATA9;
			reg->reg_TP_SEC_DATA10 = reg->base + TP_SEC_DATA10;
			reg->reg_TP_SEC_DATA11 = reg->base + TP_SEC_DATA11;
			break;
		case DMX_TP_B_0:
		case DMX_TP_B_1:
			reg->reg_TP_SEC_CTRL  = reg->base + TPB_SEC_CTRL;
			reg->reg_TP_SEC_DATA0 = reg->base + TPB_SEC_DATA0;
			reg->reg_TP_SEC_DATA1 = reg->base + TPB_SEC_DATA1;
			reg->reg_TP_SEC_DATA2 = reg->base + TPB_SEC_DATA2;
			reg->reg_TP_SEC_DATA3 = reg->base + TPB_SEC_DATA3;
			reg->reg_TP_SEC_DATA4 = reg->base + TPB_SEC_DATA4;
			reg->reg_TP_SEC_DATA5 = reg->base + TPB_SEC_DATA5;
			reg->reg_TP_SEC_DATA6 = reg->base + TPB_SEC_DATA6;
			reg->reg_TP_SEC_DATA7 = reg->base + TPB_SEC_DATA7;
			reg->reg_TP_SEC_DATA8 = reg->base + TPB_SEC_DATA8;
			reg->reg_TP_SEC_DATA9 = reg->base + TPB_SEC_DATA9;
			reg->reg_TP_SEC_DATA10 = reg->base + TPB_SEC_DATA10;
			reg->reg_TP_SEC_DATA11 = reg->base + TPB_SEC_DATA11;
			break;
		default:
			break;
	}
}

#if 0
// TPPES(
static void _mapping_pes(int tp_index, struct rtk_tp_reg *reg, int model)
{

	unsigned int _TP_A_PESRegAddr[16] = {TP_PES_EXTRC_0,
						TP_PES_EXTRC_1,
						TP_PES_EXTRC_2,
						TP_PES_EXTRC_3,
						TP_PES_EXTRC_4,
						TP_PES_EXTRC_5,
						TP_PES_EXTRC_6,
						TP_PES_EXTRC_7,
						TP_PES_EXTRC_8,
						TP_PES_EXTRC_9,
						TP_PES_EXTRC_A,
						TP_PES_EXTRC_B,
						TP_PES_EXTRC_C,
						TP_PES_EXTRC_D,
						TP_PES_EXTRC_E,
						TP_PES_EXTRC_F};
	unsigned int _TP_B_PESRegAddr[16] = {TPB_PES_EXTRC_0,
						TPB_PES_EXTRC_1,
						TPB_PES_EXTRC_2,
						TPB_PES_EXTRC_3,
						TPB_PES_EXTRC_4,
						TPB_PES_EXTRC_5,
						TPB_PES_EXTRC_6,
						TPB_PES_EXTRC_7,
						TPB_PES_EXTRC_8,
						TPB_PES_EXTRC_9,
						TPB_PES_EXTRC_A,
						TPB_PES_EXTRC_B,
						TPB_PES_EXTRC_C,
						TPB_PES_EXTRC_D,
						TPB_PES_EXTRC_E,
						TPB_PES_EXTRC_F};
	switch (tp_index)
	{
	    case DMX_TP_A_0:
	        //m_Pese_Base = 0;
	        m_Extrc_RegAddr = _TP_A_PESRegAddr[m_Id];
	        m_TP_PESE_EXTPID_G0 = TP_PESE_EXTPID_0;
	        m_TP_PESE_EXTPID_G1 = TP_PESE_EXTPID_1;
	        break;
	    case DMX_TP_A_1:
	        m_Pese_Base = TP_PES_EXTRACTION_COUNT;
	        m_Extrc_RegAddr = _TP_A_PESRegAddr[m_Id];
	        m_TP_PESE_EXTPID_G0 = TP_PESE_EXTPID_2;
	        m_TP_PESE_EXTPID_G1 = TP_PESE_EXTPID_3;
	        break;
	    case DMX_TP_B_0:
	        //m_Pese_Base = 0;
	        m_Extrc_RegAddr = m_TP_B_PESRegAddr[m_Id];
	        m_TP_PESE_EXTPID_G0 = TPB_PESE_EXTPID_0;
	        m_TP_PESE_EXTPID_G1 = TPB_PESE_EXTPID_1;
	        break;
	    case DMX_TP_B_1:
	        //m_Pese_Base = TP_PES_EXTRACTION_COUNT;
	        m_Extrc_RegAddr = m_TP_B_PESRegAddr[m_Id];
	        m_TP_PESE_EXTPID_G0 = TPB_PESE_EXTPID_2;
	        m_TP_PESE_EXTPID_G1 = TPB_PESE_EXTPID_3;
	        break;
	    default:
	        break;
	}
}
#endif


#if 1 /* legacy only for Hank */
static void tp_reg_mapping_hank(int tp_index, struct rtk_tp_reg *reg)
{
	switch (tp_index) {
	case DMX_TP_C_0:
		reg->reg_TF_CNTL = reg->base + TPC_TF0_CNTL;
		reg->reg_TF_CNT = reg->base + TPC_TF0_CNT;
		reg->reg_TF_DRP_CNT = reg->base + TPC_TF0_DRP_CNT;
		reg->reg_TF_ERR_CNT = reg->base + TPC_TF0_ERR_CNT;
		reg->reg_TF_FRMCFG = reg->base + TPC_TF0_FRMCFG;
		reg->reg_TF_INT = reg->base + TPC_TF0_INT;
		reg->reg_TF_INT_EN = reg->base + TPC_TF0_INT_EN;
		reg->reg_TF_STRM_ID_0 = reg->base + TPC_TF0_STRM_ID_0;
		reg->reg_TF_STRM_ID_1 = reg->base + TPC_TF0_STRM_ID_1;
		reg->reg_TF_STRM_ID_2 = reg->base + TPC_TF0_STRM_ID_2;
		reg->reg_TF_STRM_ID_3 = reg->base + TPC_TF0_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TPC_TF0_STRM_ID_VAL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TPC_TF0_CTRL_SWC;

		reg->reg_TP_PID_PART = reg->base + TPC_PID_PART;
		reg->reg_TP_CRC_INIT = reg->base + TPC_CRC_INIT;

		reg->reg_TP_EXT_PID_CNTL = reg->base + TPC_0_EXT_PID_CNTL;

		reg->reg_TP_M2M_RING_LIMIT = reg->base +
						TPC_TP0_M2M_RING_LIMIT;
		reg->reg_TP_M2M_RING_BASE = reg->base + TPC_TP0_M2M_RING_BASE;
		reg->reg_TP_M2M_RING_RP	= reg->base + TPC_TP0_M2M_RING_RP;
		reg->reg_TP_M2M_RING_WP	= reg->base + TPC_TP0_M2M_RING_WP;
		reg->reg_TP_M2M_RING_CTRL = reg->base + TPC_TP0_M2M_RING_CTRL;

		reg->reg_TP_RING_CTRL = reg->base + TPC_RING_CTRL;
		reg->reg_TP_RING_LIMIT	= reg->base + TPC_RING_LIMIT;
		reg->reg_TP_RING_BASE		= reg->base + TPC_RING_BASE;
		reg->reg_TP_RING_RP		= reg->base + TPC_RING_RP;
		reg->reg_TP_RING_WP		= reg->base + TPC_RING_WP;
		reg->reg_TP_FULLNESS		= reg->base + TPC_FULLNESS;
		reg->reg_TP_THRESHOLD		= reg->base + TPC_THRESHOLD;
		reg->reg_TP_RING_FULL_INT_0 = reg->base + TPC_RING_FULL_INT_0;
		reg->reg_TP_RING_FULL_INT_1 = reg->base + TPC_RING_FULL_INT_1;
		reg->reg_TP_RING_FULL_INT_2 = reg->base + TPC_RING_FULL_INT_2;
		reg->reg_TP_RING_FULL_INT_3 = reg->base + TPC_RING_FULL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_0 = reg->base +
						TPC_RING_AVAIL_INT_0;
		reg->reg_TP_RING_AVAIL_INT_1 = reg->base +
						TPC_RING_AVAIL_INT_1;
		reg->reg_TP_RING_AVAIL_INT_2 = reg->base +
						TPC_RING_AVAIL_INT_2;
		reg->reg_TP_RING_AVAIL_INT_3 = reg->base +
						TPC_RING_AVAIL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base +
						TPC_RING_AVAIL_INT_EN_0;
		reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base +
						TPC_RING_AVAIL_INT_EN_1;
		reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base +
						TPC_RING_AVAIL_INT_EN_2;
		reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base +
						TPC_RING_AVAIL_INT_EN_3;
		reg->reg_TP_RING_FULL_INT_EN_0 = reg->base +
						TPC_RING_FULL_INT_EN_0;
		reg->reg_TP_RING_FULL_INT_EN_1 = reg->base +
						TPC_RING_FULL_INT_EN_1;
		reg->reg_TP_RING_FULL_INT_EN_2 = reg->base +
						TPC_RING_FULL_INT_EN_2;
		reg->reg_TP_RING_FULL_INT_EN_3 = reg->base +
						TPC_RING_FULL_INT_EN_3;

		reg->reg_TP_PID_CTRL	= reg->base + TPC_PID_CTRL;
		reg->reg_TP_PID_DATA	= reg->base + TPC_PID_DATA;
		reg->reg_TP_PID_DATA2	= reg->base + TPC_PID_DATA2;
		reg->reg_TP_PID_DATA3	= reg->base + TPC_PID_DATA3;
		reg->reg_TP_PCRCtrlReg = reg->base + TPC0_PCR_CTL;
		reg->reg_TP_PCR_LATCH	= reg->base + TPC_PCR_LATCH;
		reg->reg_TP_PCR_SYSTEM = reg->base + TPC_PCR_SYSTEM;
		reg->reg_TP_PCR_BASE	= reg->base + TPC_PCR_BASE;
		reg->reg_TP_PCR_EXT	= reg->base + TPC_PCR_EXT;

		reg->reg_TP_SEC_CTRL  = reg->base + TPC_SEC_CTRL;
		reg->reg_TP_SEC_DATA0 = reg->base + TPC_SEC_DATA0;
		reg->reg_TP_SEC_DATA1 = reg->base + TPC_SEC_DATA1;
		reg->reg_TP_SEC_DATA2 = reg->base + TPC_SEC_DATA2;
		reg->reg_TP_SEC_DATA3 = reg->base + TPC_SEC_DATA3;
		reg->reg_TP_SEC_DATA4 = reg->base + TPC_SEC_DATA4;
		reg->reg_TP_SEC_DATA5 = reg->base + TPC_SEC_DATA5;
		reg->reg_TP_SEC_DATA6 = reg->base + TPC_SEC_DATA6;
		reg->reg_TP_SEC_DATA7 = reg->base + TPC_SEC_DATA7;
		reg->reg_TP_SEC_DATA8 = reg->base + TPC_SEC_DATA8;
		reg->reg_TP_SEC_DATA9 = reg->base + TPC_SEC_DATA9;
		reg->reg_TP_SEC_DATA10 = reg->base + TPC_SEC_DATA10;
		reg->reg_TP_SEC_DATA11 = reg->base + TPC_SEC_DATA11;
		break;
	default:
		break;
	}
}
#else /* else of legacy only for hank */
void tp_reg_mapping(int tp_index, struct rtk_tp_reg *reg)
{
	switch (tp_index) {
	case DMX_TP_A_0:
// framer
		reg->reg_TF_CNTL = reg->base + TP_TF0_CNTL;
		reg->reg_TF_CNT = reg->base + TP_TF0_CNT;
		reg->reg_TF_CNTL = reg->base + TP_TF0_CNTL;
		reg->reg_TF_CNT = reg->base + TP_TF0_CNT;
		reg->reg_TF_DRP_CNT = reg->base + TP_TF0_DRP_CNT;
		reg->reg_TF_ERR_CNT = reg->base + TP_TF0_ERR_CNT;
		reg->reg_TF_FRMCFG = reg->base + TP_TF0_FRMCFG;
		reg->reg_TF_INT = reg->base + TP_TF0_INT;
		reg->reg_TF_INT_EN = reg->base + TP_TF0_INT_EN;
		reg->reg_TF_STRM_ID_0 = reg->base + TP_TF0_STRM_ID_0;
		reg->reg_TF_STRM_ID_1 = reg->base + TP_TF0_STRM_ID_1;
		reg->reg_TF_STRM_ID_2 = reg->base + TP_TF0_STRM_ID_2;
		reg->reg_TF_STRM_ID_3 = reg->base + TP_TF0_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TP_TF0_STRM_ID_VAL;
		reg->reg_TP_DES_CNTL = reg->base + TP_TP0_DES_CNTL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TP_TF0_CTRL_SWC;

		reg->reg_TP_KEY_INFO_SWC_0 = reg->base + TP_KEY_INFO_SWC_0;
		reg->reg_TP_KEY_INFO_SWC_1 = reg->base + TP_KEY_INFO_SWC_1;
		reg->reg_TP_KEY_INFO_SWC_2 = reg->base + TP_KEY_INFO_SWC_2;
		reg->reg_TP_KEY_INFO_SWC_3 = reg->base + TP_KEY_INFO_SWC_3;
		reg->reg_TP_KEY_INFO_SWC_4 = reg->base + TP_KEY_INFO_SWC_4;
		reg->reg_TP_KEY_INFO_SWC_5 = reg->base + TP_KEY_INFO_SWC_5;
		reg->reg_TP_KEY_INFO_SWC_6 = reg->base + TP_KEY_INFO_SWC_6;
		reg->reg_TP_KEY_INFO_SWC_7 = reg->base + TP_KEY_INFO_SWC_7;
		reg->reg_TP_KEY_INFO_SWC_8 = reg->base + TP_KEY_INFO_SWC_8;
		reg->reg_TP_KEY_INFO_SWC_9 = reg->base + TP_KEY_INFO_SWC_9;
		reg->reg_TP_KEY_INFO_SWC_A = reg->base + TP_KEY_INFO_SWC_A;
		reg->reg_TP_KEY_INFO_SWC_B = reg->base + TP_KEY_INFO_SWC_B;

		reg->reg_TP_KEY_CTRL_SWC = reg->base + TP_KEY_CTRL_SWC;
		reg->reg_TP_KEY_HEADER_SWC = reg->base + TP_KEY_HEADER_SWC;
		reg->reg_TP_KEY_MASK_SWC = reg->base + TP_KEY_MASK_SWC;
		reg->reg_TP_SWC_DMY_A = reg->base + TP_SWC_DMY_A;

// tp
		reg->reg_TP_PID_PART = reg->base + TP_PID_PART;
		reg->reg_TP_CRC_INIT = reg->base + TP_CRC_INIT;

		reg->reg_TP_EXT_PID_CNTL = reg->base + TP_0_EXT_PID_CNTL;

// buffer
		reg->reg_TP_M2M_RING_LIMIT = reg->base + TP0_M2M_RING_LIMIT;
		reg->reg_TP_M2M_RING_BASE = reg->base + TP0_M2M_RING_BASE;
		reg->reg_TP_M2M_RING_RP	= reg->base + TP0_M2M_RING_RP;
		reg->reg_TP_M2M_RING_WP	= reg->base + TP0_M2M_RING_WP;
		reg->reg_TP_M2M_RING_CTRL = reg->base + TP0_M2M_RING_CTRL;

		reg->reg_TP_RING_CTRL = reg->base + TP_RING_CTRL;
		reg->reg_TP_RING_LIMIT = reg->base + TP_RING_LIMIT;
		reg->reg_TP_RING_BASE = reg->base + TP_RING_BASE;
		reg->reg_TP_RING_RP = reg->base + TP_RING_RP;
		reg->reg_TP_RING_WP = reg->base + TP_RING_WP;
		reg->reg_TP_FULLNESS = reg->base + TP_FULLNESS;
		reg->reg_TP_THRESHOLD = reg->base + TP_THRESHOLD;
		reg->reg_TP_RING_FULL_INT_0 = reg->base + TP_RING_FULL_INT_0;
		reg->reg_TP_RING_FULL_INT_1 = reg->base + TP_RING_FULL_INT_1;
		reg->reg_TP_RING_FULL_INT_2 = reg->base + TP_RING_FULL_INT_2;
		reg->reg_TP_RING_FULL_INT_3 = reg->base + TP_RING_FULL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_0 = reg->base +
						TP_RING_AVAIL_INT_0;
		reg->reg_TP_RING_AVAIL_INT_1 = reg->base +
						TP_RING_AVAIL_INT_1;
		reg->reg_TP_RING_AVAIL_INT_2 = reg->base +
						TP_RING_AVAIL_INT_2;
		reg->reg_TP_RING_AVAIL_INT_3 = reg->base +
						TP_RING_AVAIL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base +
						TP_RING_AVAIL_INT_EN_0;
		reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base +
						TP_RING_AVAIL_INT_EN_1;
		reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base +
						TP_RING_AVAIL_INT_EN_2;
		reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base +
						TP_RING_AVAIL_INT_EN_3;
		reg->reg_TP_RING_FULL_INT_EN_0 = reg->base +
						TP_RING_FULL_INT_EN_0;
		reg->reg_TP_RING_FULL_INT_EN_1 = reg->base +
						TP_RING_FULL_INT_EN_1;
		reg->reg_TP_RING_FULL_INT_EN_2 = reg->base +
						TP_RING_FULL_INT_EN_2;
		reg->reg_TP_RING_FULL_INT_EN_3 = reg->base +
						TP_RING_FULL_INT_EN_3;

// pid
		reg->reg_TP_PID_CTRL	= reg->base + TP_PID_CTRL;
		reg->reg_TP_PID_DATA	= reg->base + TP_PID_DATA;
		reg->reg_TP_PID_DATA2	= reg->base + TP_PID_DATA2;
		reg->reg_TP_PID_DATA3	= reg->base + TP_PID_DATA3;
		reg->reg_TP_PCRCtrlReg = reg->base + TP0_PCR_CTL;
		reg->reg_TP_PCR_LATCH	= reg->base + TP_PCR_LATCH;
		reg->reg_TP_PCR_SYSTEM = reg->base + TP_PCR_SYSTEM;
		reg->reg_TP_PCR_BASE	= reg->base + TP_PCR_BASE;
		reg->reg_TP_PCR_EXT	= reg->base + TP_PCR_EXT;

// sec
		reg->reg_TP_SEC_CTRL  = reg->base + TP_SEC_CTRL;
		reg->reg_TP_SEC_DATA0 = reg->base + TP_SEC_DATA0;
		reg->reg_TP_SEC_DATA1 = reg->base + TP_SEC_DATA1;
		reg->reg_TP_SEC_DATA2 = reg->base + TP_SEC_DATA2;
		reg->reg_TP_SEC_DATA3 = reg->base + TP_SEC_DATA3;
		reg->reg_TP_SEC_DATA4 = reg->base + TP_SEC_DATA4;
		reg->reg_TP_SEC_DATA5 = reg->base + TP_SEC_DATA5;
		reg->reg_TP_SEC_DATA6 = reg->base + TP_SEC_DATA6;
		reg->reg_TP_SEC_DATA7 = reg->base + TP_SEC_DATA7;
		reg->reg_TP_SEC_DATA8 = reg->base + TP_SEC_DATA8;
		reg->reg_TP_SEC_DATA9 = reg->base + TP_SEC_DATA9;
		reg->reg_TP_SEC_DATA10 = reg->base + TP_SEC_DATA10;
		reg->reg_TP_SEC_DATA11 = reg->base + TP_SEC_DATA11;
		break;
	case DMX_TP_A_1:
		reg->reg_TF_CNTL = reg->base + TP_TF1_CNTL;
		reg->reg_TF_CNT = reg->base + TP_TF1_CNT;
		reg->reg_TF_DRP_CNT = reg->base + TP_TF1_DRP_CNT;
		reg->reg_TF_ERR_CNT = reg->base + TP_TF1_ERR_CNT;
		reg->reg_TF_FRMCFG = reg->base + TP_TF1_FRMCFG;
		reg->reg_TF_INT = reg->base + TP_TF1_INT;
		reg->reg_TF_INT_EN = reg->base + TP_TF1_INT_EN;
		reg->reg_TF_STRM_ID_0 = reg->base + TP_TF1_STRM_ID_0;
		reg->reg_TF_STRM_ID_1 = reg->base + TP_TF1_STRM_ID_1;
		reg->reg_TF_STRM_ID_2 = reg->base + TP_TF1_STRM_ID_2;
		reg->reg_TF_STRM_ID_3 = reg->base + TP_TF1_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TP_TF1_STRM_ID_VAL;
		reg->reg_TP_DES_CNTL = reg->base + TP_TP1_DES_CNTL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TP_TF1_CTRL_SWC;

		reg->reg_TP_KEY_INFO_SWC_0 = reg->base + TP_KEY_INFO_SWC_0;
		reg->reg_TP_KEY_INFO_SWC_1 = reg->base + TP_KEY_INFO_SWC_1;
		reg->reg_TP_KEY_INFO_SWC_2 = reg->base + TP_KEY_INFO_SWC_2;
		reg->reg_TP_KEY_INFO_SWC_3 = reg->base + TP_KEY_INFO_SWC_3;
		reg->reg_TP_KEY_INFO_SWC_4 = reg->base + TP_KEY_INFO_SWC_4;
		reg->reg_TP_KEY_INFO_SWC_5 = reg->base + TP_KEY_INFO_SWC_5;
		reg->reg_TP_KEY_INFO_SWC_6 = reg->base + TP_KEY_INFO_SWC_6;
		reg->reg_TP_KEY_INFO_SWC_7 = reg->base + TP_KEY_INFO_SWC_7;
		reg->reg_TP_KEY_INFO_SWC_8 = reg->base + TP_KEY_INFO_SWC_8;
		reg->reg_TP_KEY_INFO_SWC_9 = reg->base + TP_KEY_INFO_SWC_9;
		reg->reg_TP_KEY_INFO_SWC_A = reg->base + TP_KEY_INFO_SWC_A;
		reg->reg_TP_KEY_INFO_SWC_B = reg->base + TP_KEY_INFO_SWC_B;
		reg->reg_TP_KEY_CTRL_SWC = reg->base + TP_KEY_CTRL_SWC;
		reg->reg_TP_KEY_HEADER_SWC = reg->base + TP_KEY_HEADER_SWC;
		reg->reg_TP_KEY_MASK_SWC = reg->base + TP_KEY_MASK_SWC;
		reg->reg_TP_SWC_DMY_A = reg->base + TP_SWC_DMY_A;

		reg->reg_TP_PID_PART = reg->base + TP_PID_PART;
		reg->reg_TP_CRC_INIT = reg->base + TP_CRC_INIT;

		reg->reg_TP_EXT_PID_CNTL = reg->base + TP_1_EXT_PID_CNTL;

		reg->reg_TP_M2M_RING_LIMIT = reg->base + TP1_M2M_RING_LIMIT;
		reg->reg_TP_M2M_RING_BASE = reg->base + TP1_M2M_RING_BASE;
		reg->reg_TP_M2M_RING_RP	= reg->base + TP1_M2M_RING_RP;
		reg->reg_TP_M2M_RING_WP	= reg->base + TP1_M2M_RING_WP;
		reg->reg_TP_M2M_RING_CTRL = reg->base + TP1_M2M_RING_CTRL;

		reg->reg_TP_RING_CTRL = reg->base + TP_RING_CTRL;
		reg->reg_TP_RING_LIMIT = reg->base + TP_RING_LIMIT;
		reg->reg_TP_RING_BASE = reg->base + TP_RING_BASE;
		reg->reg_TP_RING_RP = reg->base + TP_RING_RP;
		reg->reg_TP_RING_WP = reg->base + TP_RING_WP;
		reg->reg_TP_FULLNESS = reg->base + TP_FULLNESS;
		reg->reg_TP_THRESHOLD = reg->base + TP_THRESHOLD;
		reg->reg_TP_RING_FULL_INT_0 = reg->base + TP_RING_FULL_INT_0;
		reg->reg_TP_RING_FULL_INT_1 = reg->base + TP_RING_FULL_INT_1;
		reg->reg_TP_RING_FULL_INT_2 = reg->base + TP_RING_FULL_INT_2;
		reg->reg_TP_RING_FULL_INT_3 = reg->base + TP_RING_FULL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_0 = reg->base +
						TP_RING_AVAIL_INT_0;
		reg->reg_TP_RING_AVAIL_INT_1 = reg->base +
						TP_RING_AVAIL_INT_1;
		reg->reg_TP_RING_AVAIL_INT_2 = reg->base +
						TP_RING_AVAIL_INT_2;
		reg->reg_TP_RING_AVAIL_INT_3 = reg->base +
						TP_RING_AVAIL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base +
						TP_RING_AVAIL_INT_EN_0;
		reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base +
						TP_RING_AVAIL_INT_EN_1;
		reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base +
						TP_RING_AVAIL_INT_EN_2;
		reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base +
						TP_RING_AVAIL_INT_EN_3;
		reg->reg_TP_RING_FULL_INT_EN_0 = reg->base +
						TP_RING_FULL_INT_EN_0;
		reg->reg_TP_RING_FULL_INT_EN_1 = reg->base +
						TP_RING_FULL_INT_EN_1;
		reg->reg_TP_RING_FULL_INT_EN_2 = reg->base +
						TP_RING_FULL_INT_EN_2;
		reg->reg_TP_RING_FULL_INT_EN_3 = reg->base +
						TP_RING_FULL_INT_EN_3;

		reg->reg_TP_PID_CTRL	= reg->base + TP_PID_CTRL;
		reg->reg_TP_PID_DATA	= reg->base + TP_PID_DATA;
		reg->reg_TP_PID_DATA2	= reg->base + TP_PID_DATA2;
		reg->reg_TP_PID_DATA3	= reg->base + TP_PID_DATA3;
		reg->reg_TP_PCRCtrlReg = reg->base + TP1_PCR_CTL;
		reg->reg_TP_PCR_LATCH	= reg->base + TP_PCR_LATCH;
		reg->reg_TP_PCR_SYSTEM = reg->base + TP_PCR_SYSTEM;
		reg->reg_TP_PCR_BASE	= reg->base + TP_PCR_BASE;
		reg->reg_TP_PCR_EXT	= reg->base + TP_PCR_EXT;

		reg->reg_TP_SEC_CTRL  = reg->base + TP_SEC_CTRL;
		reg->reg_TP_SEC_DATA0 = reg->base + TP_SEC_DATA0;
		reg->reg_TP_SEC_DATA1 = reg->base + TP_SEC_DATA1;
		reg->reg_TP_SEC_DATA2 = reg->base + TP_SEC_DATA2;
		reg->reg_TP_SEC_DATA3 = reg->base + TP_SEC_DATA3;
		reg->reg_TP_SEC_DATA4 = reg->base + TP_SEC_DATA4;
		reg->reg_TP_SEC_DATA5 = reg->base + TP_SEC_DATA5;
		reg->reg_TP_SEC_DATA6 = reg->base + TP_SEC_DATA6;
		reg->reg_TP_SEC_DATA7 = reg->base + TP_SEC_DATA7;
		reg->reg_TP_SEC_DATA8 = reg->base + TP_SEC_DATA8;
		reg->reg_TP_SEC_DATA9 = reg->base + TP_SEC_DATA9;
		reg->reg_TP_SEC_DATA10 = reg->base + TP_SEC_DATA10;
		reg->reg_TP_SEC_DATA11 = reg->base + TP_SEC_DATA11;
		break;

	case DMX_TP_B_0:
		reg->reg_TF_CNTL = reg->base + TPB_TF0_CNTL;
		reg->reg_TF_CNT = reg->base + TPB_TF0_CNT;
		reg->reg_TF_DRP_CNT = reg->base + TPB_TF0_DRP_CNT;
		reg->reg_TF_ERR_CNT = reg->base + TPB_TF0_ERR_CNT;
		reg->reg_TF_FRMCFG = reg->base + TPB_TF0_FRMCFG;
		reg->reg_TF_INT = reg->base + TPB_TF0_INT;
		reg->reg_TF_INT_EN = reg->base + TPB_TF0_INT_EN;
		reg->reg_TF_STRM_ID_0 = reg->base + TPB_TF0_STRM_ID_0;
		reg->reg_TF_STRM_ID_1 = reg->base + TPB_TF0_STRM_ID_1;
		reg->reg_TF_STRM_ID_2 = reg->base + TPB_TF0_STRM_ID_2;
		reg->reg_TF_STRM_ID_3 = reg->base + TPB_TF0_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TPB_TF0_STRM_ID_VAL;
		reg->reg_TP_DES_CNTL = reg->base + TPB_TP0_DES_CNTL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TPB_TF0_CTRL_SWC;

		reg->reg_TP_KEY_INFO_SWC_0 = reg->base + TP_KEY_INFO_SWC_0;
		reg->reg_TP_KEY_INFO_SWC_1 = reg->base + TP_KEY_INFO_SWC_1;
		reg->reg_TP_KEY_INFO_SWC_2 = reg->base + TP_KEY_INFO_SWC_2;
		reg->reg_TP_KEY_INFO_SWC_3 = reg->base + TP_KEY_INFO_SWC_3;
		reg->reg_TP_KEY_INFO_SWC_4 = reg->base + TP_KEY_INFO_SWC_4;
		reg->reg_TP_KEY_INFO_SWC_5 = reg->base + TP_KEY_INFO_SWC_5;
		reg->reg_TP_KEY_INFO_SWC_6 = reg->base + TP_KEY_INFO_SWC_6;
		reg->reg_TP_KEY_INFO_SWC_7 = reg->base + TP_KEY_INFO_SWC_7;
		reg->reg_TP_KEY_INFO_SWC_8 = reg->base + TP_KEY_INFO_SWC_8;
		reg->reg_TP_KEY_INFO_SWC_9 = reg->base + TP_KEY_INFO_SWC_9;
		reg->reg_TP_KEY_INFO_SWC_A = reg->base + TP_KEY_INFO_SWC_A;
		reg->reg_TP_KEY_INFO_SWC_B = reg->base + TP_KEY_INFO_SWC_B;

		reg->reg_TP_KEY_CTRL_SWC = reg->base + TP_KEY_CTRL_SWC;
		reg->reg_TP_KEY_HEADER_SWC = reg->base + TP_KEY_HEADER_SWC;
		reg->reg_TP_KEY_MASK_SWC = reg->base + TP_KEY_MASK_SWC;
		reg->reg_TP_SWC_DMY_A = reg->base + TPB_SWC_DMY_A;

		reg->reg_TP_PID_PART = reg->base + TPB_PID_PART;
		reg->reg_TP_CRC_INIT = reg->base + TPB_CRC_INIT;

		reg->reg_TP_EXT_PID_CNTL = reg->base + TPB_0_EXT_PID_CNTL;

		reg->reg_TP_M2M_RING_LIMIT = reg->base +
						TPB_TP0_M2M_RING_LIMIT;
		reg->reg_TP_M2M_RING_BASE = reg->base + TPB_TP0_M2M_RING_BASE;
		reg->reg_TP_M2M_RING_RP	= reg->base + TPB_TP0_M2M_RING_RP;
		reg->reg_TP_M2M_RING_WP	= reg->base + TPB_TP0_M2M_RING_WP;
		reg->reg_TP_M2M_RING_CTRL = reg->base + TPB_TP0_M2M_RING_CTRL;

		reg->reg_TP_RING_CTRL = reg->base + TPB_RING_CTRL;
		reg->reg_TP_RING_LIMIT = reg->base + TPB_RING_LIMIT;
		reg->reg_TP_RING_BASE = reg->base + TPB_RING_BASE;
		reg->reg_TP_RING_RP = reg->base + TPB_RING_RP;
		reg->reg_TP_RING_WP = reg->base + TPB_RING_WP;
		reg->reg_TP_FULLNESS = reg->base + TPB_FULLNESS;
		reg->reg_TP_THRESHOLD = reg->base + TPB_THRESHOLD;
		reg->reg_TP_RING_FULL_INT_0 = reg->base + TPB_RING_FULL_INT_0;
		reg->reg_TP_RING_FULL_INT_1 = reg->base + TPB_RING_FULL_INT_1;
		reg->reg_TP_RING_FULL_INT_2 = reg->base + TPB_RING_FULL_INT_2;
		reg->reg_TP_RING_FULL_INT_3 = reg->base + TPB_RING_FULL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_0 = reg->base +
						TPB_RING_AVAIL_INT_0;
		reg->reg_TP_RING_AVAIL_INT_1 = reg->base +
						TPB_RING_AVAIL_INT_1;
		reg->reg_TP_RING_AVAIL_INT_2 = reg->base +
						TPB_RING_AVAIL_INT_2;
		reg->reg_TP_RING_AVAIL_INT_3 = reg->base +
						TPB_RING_AVAIL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base +
						TPB_RING_AVAIL_INT_EN_0;
		reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base +
						TPB_RING_AVAIL_INT_EN_1;
		reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base +
						TPB_RING_AVAIL_INT_EN_2;
		reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base +
						TPB_RING_AVAIL_INT_EN_3;
		reg->reg_TP_RING_FULL_INT_EN_0 = reg->base +
						TPB_RING_FULL_INT_EN_0;
		reg->reg_TP_RING_FULL_INT_EN_1 = reg->base +
						TPB_RING_FULL_INT_EN_1;
		reg->reg_TP_RING_FULL_INT_EN_2 = reg->base +
						TPB_RING_FULL_INT_EN_2;
		reg->reg_TP_RING_FULL_INT_EN_3 = reg->base +
						TPB_RING_FULL_INT_EN_3;

		reg->reg_TP_PID_CTRL = reg->base + TPB_PID_CTRL;
		reg->reg_TP_PID_DATA = reg->base + TPB_PID_DATA;
		reg->reg_TP_PID_DATA2 = reg->base + TPB_PID_DATA2;
		reg->reg_TP_PID_DATA3 = reg->base + TPB_PID_DATA3;
		reg->reg_TP_PCRCtrlReg = reg->base + TPB0_PCR_CTL;
		reg->reg_TP_PCR_LATCH = reg->base + TPB_PCR_LATCH;
		reg->reg_TP_PCR_SYSTEM = reg->base + TPB_PCR_SYSTEM;
		reg->reg_TP_PCR_BASE = reg->base + TPB_PCR_BASE;
		reg->reg_TP_PCR_EXT = reg->base + TPB_PCR_EXT;

		reg->reg_TP_SEC_CTRL = reg->base + TPB_SEC_CTRL;
		reg->reg_TP_SEC_DATA0 = reg->base + TPB_SEC_DATA0;
		reg->reg_TP_SEC_DATA1 = reg->base + TPB_SEC_DATA1;
		reg->reg_TP_SEC_DATA2 = reg->base + TPB_SEC_DATA2;
		reg->reg_TP_SEC_DATA3 = reg->base + TPB_SEC_DATA3;
		reg->reg_TP_SEC_DATA4 = reg->base + TPB_SEC_DATA4;
		reg->reg_TP_SEC_DATA5 = reg->base + TPB_SEC_DATA5;
		reg->reg_TP_SEC_DATA6 = reg->base + TPB_SEC_DATA6;
		reg->reg_TP_SEC_DATA7 = reg->base + TPB_SEC_DATA7;
		reg->reg_TP_SEC_DATA8 = reg->base + TPB_SEC_DATA8;
		reg->reg_TP_SEC_DATA9 = reg->base + TPB_SEC_DATA9;
		reg->reg_TP_SEC_DATA10 = reg->base + TPB_SEC_DATA10;
		reg->reg_TP_SEC_DATA11 = reg->base + TPB_SEC_DATA11;
		break;

	case DMX_TP_B_1:
		reg->reg_TF_CNTL = reg->base + TPB_TF1_CNTL;
		reg->reg_TF_CNT = reg->base + TPB_TF1_CNT;
		reg->reg_TF_DRP_CNT = reg->base + TPB_TF1_DRP_CNT;
		reg->reg_TF_ERR_CNT = reg->base + TPB_TF1_ERR_CNT;
		reg->reg_TF_FRMCFG = reg->base + TPB_TF1_FRMCFG;
		reg->reg_TF_INT = reg->base + TPB_TF1_INT;
		reg->reg_TF_INT_EN = reg->base + TPB_TF1_INT_EN;
		reg->reg_TF_STRM_ID_0 = reg->base + TPB_TF1_STRM_ID_0;
		reg->reg_TF_STRM_ID_1 = reg->base + TPB_TF1_STRM_ID_1;
		reg->reg_TF_STRM_ID_2 = reg->base + TPB_TF1_STRM_ID_2;
		reg->reg_TF_STRM_ID_3 = reg->base + TPB_TF1_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TPB_TF1_STRM_ID_VAL;
		reg->reg_TP_DES_CNTL = reg->base + TPB_TP1_DES_CNTL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TPB_TF1_CTRL_SWC;

		reg->reg_TP_KEY_INFO_SWC_0 = reg->base + TP_KEY_INFO_SWC_0;
		reg->reg_TP_KEY_INFO_SWC_1 = reg->base + TP_KEY_INFO_SWC_1;
		reg->reg_TP_KEY_INFO_SWC_2 = reg->base + TP_KEY_INFO_SWC_2;
		reg->reg_TP_KEY_INFO_SWC_3 = reg->base + TP_KEY_INFO_SWC_3;
		reg->reg_TP_KEY_INFO_SWC_4 = reg->base + TP_KEY_INFO_SWC_4;
		reg->reg_TP_KEY_INFO_SWC_5 = reg->base + TP_KEY_INFO_SWC_5;
		reg->reg_TP_KEY_INFO_SWC_6 = reg->base + TP_KEY_INFO_SWC_6;
		reg->reg_TP_KEY_INFO_SWC_7 = reg->base + TP_KEY_INFO_SWC_7;
		reg->reg_TP_KEY_INFO_SWC_8 = reg->base + TP_KEY_INFO_SWC_8;
		reg->reg_TP_KEY_INFO_SWC_9 = reg->base + TP_KEY_INFO_SWC_9;
		reg->reg_TP_KEY_INFO_SWC_A = reg->base + TP_KEY_INFO_SWC_A;
		reg->reg_TP_KEY_INFO_SWC_B = reg->base + TP_KEY_INFO_SWC_B;

		reg->reg_TP_KEY_CTRL_SWC = reg->base + TP_KEY_CTRL_SWC;
		reg->reg_TP_KEY_HEADER_SWC = reg->base + TP_KEY_HEADER_SWC;
		reg->reg_TP_KEY_MASK_SWC = reg->base + TP_KEY_MASK_SWC;
		reg->reg_TP_SWC_DMY_A = reg->base + TPB_SWC_DMY_A;

		reg->reg_TP_PID_PART = reg->base + TPB_PID_PART;
		reg->reg_TP_CRC_INIT = reg->base + TPB_CRC_INIT;

		reg->reg_TP_EXT_PID_CNTL = reg->base + TPB_1_EXT_PID_CNTL;

		reg->reg_TP_M2M_RING_LIMIT = reg->base +
						TPB_TP1_M2M_RING_LIMIT;
		reg->reg_TP_M2M_RING_BASE = reg->base + TPB_TP1_M2M_RING_BASE;
		reg->reg_TP_M2M_RING_RP	= reg->base + TPB_TP1_M2M_RING_RP;
		reg->reg_TP_M2M_RING_WP	= reg->base + TPB_TP1_M2M_RING_WP;
		reg->reg_TP_M2M_RING_CTRL = reg->base + TPB_TP1_M2M_RING_CTRL;

		reg->reg_TP_RING_CTRL = reg->base + TPB_RING_CTRL;
		reg->reg_TP_RING_LIMIT = reg->base + TPB_RING_LIMIT;
		reg->reg_TP_RING_BASE = reg->base + TPB_RING_BASE;
		reg->reg_TP_RING_RP = reg->base + TPB_RING_RP;
		reg->reg_TP_RING_WP = reg->base + TPB_RING_WP;
		reg->reg_TP_FULLNESS = reg->base + TPB_FULLNESS;
		reg->reg_TP_THRESHOLD = reg->base + TPB_THRESHOLD;
		reg->reg_TP_RING_FULL_INT_0 = reg->base + TPB_RING_FULL_INT_0;
		reg->reg_TP_RING_FULL_INT_1 = reg->base + TPB_RING_FULL_INT_1;
		reg->reg_TP_RING_FULL_INT_2 = reg->base + TPB_RING_FULL_INT_2;
		reg->reg_TP_RING_FULL_INT_3 = reg->base + TPB_RING_FULL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_0 = reg->base +
						TPB_RING_AVAIL_INT_0;
		reg->reg_TP_RING_AVAIL_INT_1 = reg->base +
						TPB_RING_AVAIL_INT_1;
		reg->reg_TP_RING_AVAIL_INT_2 = reg->base +
						TPB_RING_AVAIL_INT_2;
		reg->reg_TP_RING_AVAIL_INT_3 = reg->base +
						TPB_RING_AVAIL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base +
						TPB_RING_AVAIL_INT_EN_0;
		reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base +
						TPB_RING_AVAIL_INT_EN_1;
		reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base +
						TPB_RING_AVAIL_INT_EN_2;
		reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base +
						TPB_RING_AVAIL_INT_EN_3;
		reg->reg_TP_RING_FULL_INT_EN_0 = reg->base +
						TPB_RING_FULL_INT_EN_0;
		reg->reg_TP_RING_FULL_INT_EN_1 = reg->base +
						TPB_RING_FULL_INT_EN_1;
		reg->reg_TP_RING_FULL_INT_EN_2 = reg->base +
						TPB_RING_FULL_INT_EN_2;
		reg->reg_TP_RING_FULL_INT_EN_3 = reg->base +
						TPB_RING_FULL_INT_EN_3;

		reg->reg_TP_PID_CTRL	= reg->base + TPB_PID_CTRL;
		reg->reg_TP_PID_DATA	= reg->base + TPB_PID_DATA;
		reg->reg_TP_PID_DATA2	= reg->base + TPB_PID_DATA2;
		reg->reg_TP_PID_DATA3	= reg->base + TPB_PID_DATA3;
		reg->reg_TP_PCRCtrlReg = reg->base + TPB1_PCR_CTL;
		reg->reg_TP_PCR_LATCH	= reg->base + TPB_PCR_LATCH;
		reg->reg_TP_PCR_SYSTEM = reg->base + TPB_PCR_SYSTEM;
		reg->reg_TP_PCR_BASE	= reg->base + TPB_PCR_BASE;
		reg->reg_TP_PCR_EXT	= reg->base + TPB_PCR_EXT;

		reg->reg_TP_SEC_CTRL  = reg->base + TPB_SEC_CTRL;
		reg->reg_TP_SEC_DATA0 = reg->base + TPB_SEC_DATA0;
		reg->reg_TP_SEC_DATA1 = reg->base + TPB_SEC_DATA1;
		reg->reg_TP_SEC_DATA2 = reg->base + TPB_SEC_DATA2;
		reg->reg_TP_SEC_DATA3 = reg->base + TPB_SEC_DATA3;
		reg->reg_TP_SEC_DATA4 = reg->base + TPB_SEC_DATA4;
		reg->reg_TP_SEC_DATA5 = reg->base + TPB_SEC_DATA5;
		reg->reg_TP_SEC_DATA6 = reg->base + TPB_SEC_DATA6;
		reg->reg_TP_SEC_DATA7 = reg->base + TPB_SEC_DATA7;
		reg->reg_TP_SEC_DATA8 = reg->base + TPB_SEC_DATA8;
		reg->reg_TP_SEC_DATA9 = reg->base + TPB_SEC_DATA9;
		reg->reg_TP_SEC_DATA10 = reg->base + TPB_SEC_DATA10;
		reg->reg_TP_SEC_DATA11 = reg->base + TPB_SEC_DATA11;
		break;
#if defined(CONFIG_ARCH_RTD13xx)
	case DMX_TP_C_0:
		reg->reg_TF_CNTL = reg->base + TPC_TF0_CNTL;
		reg->reg_TF_CNT = reg->base + TPC_TF0_CNT;
		reg->reg_TF_DRP_CNT = reg->base + TPC_TF0_DRP_CNT;
		reg->reg_TF_ERR_CNT = reg->base + TPC_TF0_ERR_CNT;
		reg->reg_TF_FRMCFG = reg->base + TPC_TF0_FRMCFG;
		reg->reg_TF_INT = reg->base + TPC_TF0_INT;
		reg->reg_TF_INT_EN = reg->base + TPC_TF0_INT_EN;
		reg->reg_TF_STRM_ID_0 = reg->base + TPC_TF0_STRM_ID_0;
		reg->reg_TF_STRM_ID_1 = reg->base + TPC_TF0_STRM_ID_1;
		reg->reg_TF_STRM_ID_2 = reg->base + TPC_TF0_STRM_ID_2;
		reg->reg_TF_STRM_ID_3 = reg->base + TPC_TF0_STRM_ID_3;
		reg->reg_TF_STRM_ID_VAL = reg->base + TPC_TF0_STRM_ID_VAL;

		reg->reg_TP_TF_CTRL_SWC = reg->base + TPC_TF0_CTRL_SWC;

		reg->reg_TP_PID_PART = reg->base + TPC_PID_PART;
		reg->reg_TP_CRC_INIT = reg->base + TPC_CRC_INIT;

		reg->reg_TP_EXT_PID_CNTL = reg->base + TPC_0_EXT_PID_CNTL;

		reg->reg_TP_M2M_RING_LIMIT = reg->base +
						TPC_TP0_M2M_RING_LIMIT;
		reg->reg_TP_M2M_RING_BASE = reg->base + TPC_TP0_M2M_RING_BASE;
		reg->reg_TP_M2M_RING_RP	= reg->base + TPC_TP0_M2M_RING_RP;
		reg->reg_TP_M2M_RING_WP	= reg->base + TPC_TP0_M2M_RING_WP;
		reg->reg_TP_M2M_RING_CTRL = reg->base + TPC_TP0_M2M_RING_CTRL;

		reg->reg_TP_RING_CTRL = reg->base + TPC_RING_CTRL;
		reg->reg_TP_RING_LIMIT	= reg->base + TPC_RING_LIMIT;
		reg->reg_TP_RING_BASE		= reg->base + TPC_RING_BASE;
		reg->reg_TP_RING_RP		= reg->base + TPC_RING_RP;
		reg->reg_TP_RING_WP		= reg->base + TPC_RING_WP;
		reg->reg_TP_FULLNESS		= reg->base + TPC_FULLNESS;
		reg->reg_TP_THRESHOLD		= reg->base + TPC_THRESHOLD;
		reg->reg_TP_RING_FULL_INT_0 = reg->base + TPC_RING_FULL_INT_0;
		reg->reg_TP_RING_FULL_INT_1 = reg->base + TPC_RING_FULL_INT_1;
		reg->reg_TP_RING_FULL_INT_2 = reg->base + TPC_RING_FULL_INT_2;
		reg->reg_TP_RING_FULL_INT_3 = reg->base + TPC_RING_FULL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_0 = reg->base +
						TPC_RING_AVAIL_INT_0;
		reg->reg_TP_RING_AVAIL_INT_1 = reg->base +
						TPC_RING_AVAIL_INT_1;
		reg->reg_TP_RING_AVAIL_INT_2 = reg->base +
						TPC_RING_AVAIL_INT_2;
		reg->reg_TP_RING_AVAIL_INT_3 = reg->base +
						TPC_RING_AVAIL_INT_3;
		reg->reg_TP_RING_AVAIL_INT_EN_0 = reg->base +
						TPC_RING_AVAIL_INT_EN_0;
		reg->reg_TP_RING_AVAIL_INT_EN_1 = reg->base +
						TPC_RING_AVAIL_INT_EN_1;
		reg->reg_TP_RING_AVAIL_INT_EN_2 = reg->base +
						TPC_RING_AVAIL_INT_EN_2;
		reg->reg_TP_RING_AVAIL_INT_EN_3 = reg->base +
						TPC_RING_AVAIL_INT_EN_3;
		reg->reg_TP_RING_FULL_INT_EN_0 = reg->base +
						TPC_RING_FULL_INT_EN_0;
		reg->reg_TP_RING_FULL_INT_EN_1 = reg->base +
						TPC_RING_FULL_INT_EN_1;
		reg->reg_TP_RING_FULL_INT_EN_2 = reg->base +
						TPC_RING_FULL_INT_EN_2;
		reg->reg_TP_RING_FULL_INT_EN_3 = reg->base +
						TPC_RING_FULL_INT_EN_3;

		reg->reg_TP_PID_CTRL	= reg->base + TPC_PID_CTRL;
		reg->reg_TP_PID_DATA	= reg->base + TPC_PID_DATA;
		reg->reg_TP_PID_DATA2	= reg->base + TPC_PID_DATA2;
		reg->reg_TP_PID_DATA3	= reg->base + TPC_PID_DATA3;
		reg->reg_TP_PCRCtrlReg = reg->base + TPC0_PCR_CTL;
		reg->reg_TP_PCR_LATCH	= reg->base + TPC_PCR_LATCH;
		reg->reg_TP_PCR_SYSTEM = reg->base + TPC_PCR_SYSTEM;
		reg->reg_TP_PCR_BASE	= reg->base + TPC_PCR_BASE;
		reg->reg_TP_PCR_EXT	= reg->base + TPC_PCR_EXT;

		reg->reg_TP_SEC_CTRL  = reg->base + TPC_SEC_CTRL;
		reg->reg_TP_SEC_DATA0 = reg->base + TPC_SEC_DATA0;
		reg->reg_TP_SEC_DATA1 = reg->base + TPC_SEC_DATA1;
		reg->reg_TP_SEC_DATA2 = reg->base + TPC_SEC_DATA2;
		reg->reg_TP_SEC_DATA3 = reg->base + TPC_SEC_DATA3;
		reg->reg_TP_SEC_DATA4 = reg->base + TPC_SEC_DATA4;
		reg->reg_TP_SEC_DATA5 = reg->base + TPC_SEC_DATA5;
		reg->reg_TP_SEC_DATA6 = reg->base + TPC_SEC_DATA6;
		reg->reg_TP_SEC_DATA7 = reg->base + TPC_SEC_DATA7;
		reg->reg_TP_SEC_DATA8 = reg->base + TPC_SEC_DATA8;
		reg->reg_TP_SEC_DATA9 = reg->base + TPC_SEC_DATA9;
		reg->reg_TP_SEC_DATA10 = reg->base + TPC_SEC_DATA10;
		reg->reg_TP_SEC_DATA11 = reg->base + TPC_SEC_DATA11;
		break;
#endif /* CONFIG_ARCH_RTD1xx */
	default:
		break;
	}
}
#endif /* legacy only for HANK */

void tp_reg_mapping(int tp_index, struct rtk_tp_reg *reg, int model_id)
{
	reg->model_id = model_id;
	_mapping_tp(tp_index, reg, model_id);
	_mapping_framer(tp_index, reg, model_id);
	_mapping_buffer(tp_index, reg, model_id);
	_mapping_pid(tp_index, reg, model_id);
	_mapping_sec(tp_index, reg, model_id);
#if 0
	_mapping_pes(tp_index, reg, model);
#endif
	if (model_id == TP_HANK) {
		tp_reg_mapping_hank(tp_index, reg);
	}
}

