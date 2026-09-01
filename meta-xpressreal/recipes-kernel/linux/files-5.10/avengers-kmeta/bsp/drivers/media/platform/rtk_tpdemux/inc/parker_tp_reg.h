/*
 * parker_tp_reg.h
 *
 * Copyright (C) 2023 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _PARKER_TP_REG_H_
#define _PARKER_TP_REG_H_

#include "stark_tp_reg.h"

/* base A:14000 B:39000 */

/* TP CONTROL */
#define TP_TF0_CNTL2   0x200
#define TP_TF1_CNTL2   0x204

#define TPB_TF0_CNTL2  0x200
#define TPB_TF1_CNTL2  0x204

/* SYNA CONTROL */
#define TP_TF0_SYNA_CTRL  0x2D0
#define TP_TF1_SYNA_CTRL  0x2D4
#define TPB_TF0_SYNA_CTRL 0x2D0
#define TPB_TF1_SYNA_CTRL 0x2D4

#define TP_TF0_SYNA_INT  0x8F0
#define TP_TF1_SYNA_INT  0x8F8
#define TPB_TF0_SYNA_INT 0x8F0
#define TPB_TF1_SYNA_INT 0x8F8

/* TP PID FILTER */
#define TP_PID_DATA4   0x074
#define TP_PID_DATA5   0x078
#define TP_PID_DATA6   0x07C
#define TPB_PID_DATA4  0x074
#define TPB_PID_DATA5  0x078
#define TPB_PID_DATA6  0x07C

#define TP_PID_DATA4_KTE_IDX_CLEAR_VALID(x) ((x & 0x01) << 23)
#define TP_PID_DATA4_KTE_IDX_CLEAR(x)       ((x & 0x7F) << 16)
#define TP_PID_DATA4_KTE_IDX_ODD_VALID(x)   ((x & 0x01) << 15)
#define TP_PID_DATA4_KTE_IDX_ODD(x)         ((x & 0x7F) << 8)
#define TP_PID_DATA4_KTE_IDX_EVEN_VALID(x)  ((x & 0x01) << 7)
#define TP_PID_DATA4_KTE_IDX_EVEN(x)        ((x & 0x7F) << 0)

#define TP_PID_DATA5_OFB_LS(x)     ((x & 0x03) << 22)
#define TP_PID_DATA5_MODE_LS(x)    ((x & 0x03) << 20)
#define TP_PID_DATA5_ALGO_LS(x)    ((x & 0x0F) << 16)
#define TP_PID_DATA5_OFB_ESA(x)     ((x & 0x03) << 14)
#define TP_PID_DATA5_MODE_ESA(x)    ((x & 0x03) << 12)
#define TP_PID_DATA5_ALGO_ESA(x)    ((x & 0x0F) << 8)
#define TP_PID_DATA5_OFB_LD(x)      ((x & 0x03) << 6)
#define TP_PID_DATA5_MODE_LD(x)     ((x & 0x03) << 4)
#define TP_PID_DATA5_ALGO_LD(x)     ((x & 0x0F) << 0)

#define TP_PID_DATA6_CC_MODE_INDEX(x)    ((x & 0xF) << 16)
#define TP_PID_DATA6_CC_MODE(x)          ((x & 0x3) << 14)
#define TP_PID_DATA6_SECONDARY_PID_EN(x) ((x & 0x1) << 13)
#define TP_PID_DATA6_PRIMARY_PID(x)      ((x & 0x1FFF) << 0)

//TP_TPx_DES_CNTL
#define DESC_TDES_ABC_MODE(x)	((x & 0x01)<<22)
#define DESC_TDES_ABC_MODE_BIT	(0x00000001 << 22)

#define DESC_OFB_MODE(x)		((x & 3)<<20)
#define DESC_CSA_MODE(x)		((x & 3)<<18)

#define DESC_MULTI2_ROUND(x)	((x & 0xFF)<<10)
#define DESC_MULTI2_MODE(x)		((x & 1)<<9)

#define DESC_MAP_11(x)			((x & 1)<<8)
#define DESC_MAP_10(x)			((x & 1)<<7)
#define DESC_MAP_01(x)			((x & 1)<<6)

#define DESC_DES_MODE(x)	    ((x & 3)<<4)
#define DESC_MODE(x)            ((x & 0xF))

#define CTR (2)

#define AES_CPCM_LSA_MDI (0x8)
#define AES_CPCM_LSA_MDD (0x9)
#define AES_AVLOCAL      (0xA)
#define AES_NSA_DSS_MDI  (0xB)
#define AES_NSA_DSS_MDD  (0xC)

#define AVLOCAL_MPEG_MSC_12 (0)
#define AVLOCAL_MPEG_MSC_28 (1)
#define AVLOCAL_DSS_MSC_2   (2)
#define AVLOCAL_DSS_MSC_18  (3)

#define NSA_DSS_MSC_4  (2)
#define NSA_DSS_MSC_18 (3)

#define TP_KEY_INFO_REE_0 0x900
#define TP_KEY_INFO_REE_1 0x904
#define TP_KEY_INFO_REE_2 0x908
#define TP_KEY_INFO_REE_3 0x90C
#define TP_KEY_INFO_REE_4 0x910
#define TP_KEY_INFO_REE_5 0x914
#define TP_KEY_INFO_REE_6 0x918
#define TP_KEY_INFO_REE_7 0x91C

#define TP_KEY_HEADER_REE 0x930
#define TP_KEY_MASK_REE   0x934
#define TP_KEY_CTRL_NWC   0x960

#define TP_SECU0_SADDR_SWC 0x280
#define TP_SECU0_EADDR_SWC 0x284
#define TP_SECU1_SADDR_SWC 0x288
#define TP_SECU1_EADDR_SWC 0x28C
#define TP_SECU2_SADDR_SWC 0x290
#define TP_SECU2_EADDR_SWC 0x294
#define TP_SECU3_SADDR_SWC 0x298
#define TP_SECU3_EADDR_SWC 0x29C
#define TP_SECU4_SADDR_SWC 0x2A0
#define TP_SECU4_EADDR_SWC 0x2A4
#define TP_SECU5_SADDR_SWC 0x2A8
#define TP_SECU5_EADDR_SWC 0x2AC
#define TP_SECU6_SADDR_SWC 0x2B0
#define TP_SECU6_EADDR_SWC 0x2B4
#define TP_SECU7_SADDR_SWC 0x2B8
#define TP_SECU7_EADDR_SWC 0x2BC

#define TPB_SECU0_SADDR_SWC 0x280
#define TPB_SECU0_EADDR_SWC 0x284
#define TPB_SECU1_SADDR_SWC 0x288
#define TPB_SECU1_EADDR_SWC 0x28C
#define TPB_SECU2_SADDR_SWC 0x290
#define TPB_SECU2_EADDR_SWC 0x294
#define TPB_SECU3_SADDR_SWC 0x298
#define TPB_SECU3_EADDR_SWC 0x29C
#define TPB_SECU4_SADDR_SWC 0x2A0
#define TPB_SECU4_EADDR_SWC 0x2A4
#define TPB_SECU5_SADDR_SWC 0x2A8
#define TPB_SECU5_EADDR_SWC 0x2AC
#define TPB_SECU6_SADDR_SWC 0x2B0
#define TPB_SECU6_EADDR_SWC 0x2B4
#define TPB_SECU7_SADDR_SWC 0x2B8
#define TPB_SECU7_EADDR_SWC 0x2BC

#define TP_SYNA_CC_INIT  0x2DC
#define TPB_SYNA_CC_INIT 0x2DC

/* TF_CNTL 2*/
#define TP_TF_CNTL2_OVERFLOW_DROP_DIS_BIT (0x00000001 << 3)
#define TP_TF_CNTL2_ALGO_PID_MODE_BIT     (0x00000001 << 2)
#define TP_TF_CNTL2_TI_ALL_EN_BIT         (0x00000001 << 1)

#define TP_TF_CNTL2_OVERFLOW_DROP_DIS(x) ((x & 0x01) << 3)
#define TP_TF_CNTL2_ALGO_PID_MODE(x)     ((x & 0x01) << 2)
#define TP_TF_CNTL2_TI_ALL_EN(x)         ((x & 0x01) << 1)

/* TF_INT */
#define TF_INT_REE_CW_ILLEGAL_DEC_BIT   (0x01 << 23)
#define TF_INT_REE_CW_ILLEGAL_W_BIT     (0x01 << 22)
#define TF_INT_REE_CW_ILLEGAL_ALGO_BIT  (0x01 << 21)
#define TF_INT_REE_CW_ILLEGAL_ENC_BIT   (0x01 << 20)
#define TF_INT_REE_CW_ILLEGAL_ACTV_BIT  (0x01 << 19)

#define TP_TFX_INT_VALUE (0xF8FFFE)
#define TP_TFX_INT_EN  (TP_TF_INT_EN | \
			TF_INT_REE_CW_ILLEGAL_ACTV_BIT | \
			TF_INT_REE_CW_ILLEGAL_ENC_BIT | \
			TF_INT_REE_CW_ILLEGAL_ALGO_BIT | \
			TF_INT_REE_CW_ILLEGAL_W_BIT | \
			TF_INT_REE_CW_ILLEGAL_DEC_BIT)
#define TP_TFX_INT_EN_MASK(x)    (x & 0xF8FFFE)
#define TP_TFX_INT_VALUE_MASK(x) (x & 0xF8FFFE)

/* DES CNTL */
#ifdef TP_KEY_HEADER_RANDOM_DELAY
#undef TP_KEY_HEADER_RANDOM_DELAY
#define TP_KEY_HEADER_RANDOM_DELAY(x) ((x & 0x03) << 15)
#endif

#ifdef TP_KEY_HEADER_ACTIVATE
#undef TP_KEY_HEADER_ACTIVATE
#define TP_KEY_HEADER_ACTIVATE(x)  ((x & 0x03) << 13)
#endif

#ifdef TP_KEY_HEADER_ENC
#undef TP_KEY_HEADER_ENC
#define TP_KEY_HEADER_ENC(x) ((x & 0x03) << 11)
#endif

#ifdef TP_KEY_HEADER_DEC
#undef TP_KEY_HEADER_DEC
#define TP_KEY_HEADER_DEC(x) ((x & 0x03) << 9)
#endif

#ifdef TP_KEY_HEADER_ALGORITHM
#undef TP_KEY_HEADER_ALGORITHM
#define TP_KEY_HEADER_ALGORITHM(x) ((x & 0x1F) << 4)
#endif

#ifdef TP_KEY_HEADER_USAGE
#undef TP_KEY_HEADER_USAGE
#define TP_KEY_HEADER_USAGE(x) (x & 0x0F)
#endif

/* invalid(not true defs) */
#define TF_CTRL_SWC_SCRAMBLE_SET(x) ((x & 0x00) << 4)
#define TF_CTRL_SWC_SOLITARY_KEEP_CLEAR_SET(x) ((x & 0x00) << 3)
#define TF_CTRL_SWC_PACKET_SIZE_SET(x)         (x & 0x00)

#define CW_HEADER_ALGO_CW1_DES      (0x1)
#define CW_HEADER_ALGO_CW1_TDES_ABA (0x2)
#define CW_HEADER_ALGO_CW1_AES_128  (0x4)
#define CW_HEADER_ALGO_CW1_SM4      (0x8)
#define CW_HEADER_ALGO_CW1_CSA2     (0xD)
#define CW_HEADER_ALGO_CW1_CSA3     (0xE)
#define CW_HEADER_ALGO_CW1_ANY_64b  (0x10)
#define CW_HEADER_ALGO_CW1_ANY_128b (0x13)

#define CW_HEADER_ALGO_CW2_TDES_ABC (0x1)
#define CW_HEADER_ALGO_CW2_AES_192  (0x2)
#define CW_HEADER_ALGO_CW2_AES_256  (0x4)
#define CW_HEADER_ALGO_CW2_MULTI2   (0x8)

#define CW_HEADER_USAGE_TP            (0x1)
#define CW_HEADER_USAGE_CP_TEE        (0x2)
#define CW_HEADER_USAGE_CP_REE        (0x4)
#define CW_HEADER_USAGE_TP_CP_TEE     (0x8)
#define CW_HEADER_USAGE_TP_CP_REE_TEE (0xB)
#define CW_HEADER_USAGE_RTK_KL        (0xD)

#define M2M_EXT_PID(x)		((x & 0x3F) << 8)
#define EMM_EXT_PID(x)		((x & 0x3F) << 16)

/* SYNA ones are invalid(not true defs) */
#define TP_SYNA_CTRL_MERGE_DUP_DROP_EN_BIT (0x00000001 << 0)
#define TP_SYNA_CTRL_AES_REE_BIT    (0x00000001 << 0)
#define TP_SYNA_CTRL_UPPER_PATH_BIT (0x00000001 << 0)
#define TP_SYNA_CTRL_EMM_SEL_BIT    (0x00000001 << 0)
#define TP_SYNA_CTRL_SYNA_EN_BIT    (0x00000001 << 0)

#define TP_SYNA_CTRL_MERGE_DUP_DROP_EN(x) ((x & 0x1) << 0)
#define TP_SYNA_CTRL_AES_REE(x)    ((x & 0x1) << 0)
#define TP_SYNA_CTRL_UPPER_PATH(x) ((x & 0x1) << 0)
#define TP_SYNA_CTRL_EMM_SEL(x)    ((x & 0x1) << 0)
#define TP_SYNA_CTRL_SYNA_EN(x)    ((x & 0x1) << 0)

#define TP_SECURE_REGION_ADDR_SET(x) ((x) << 8)
#define TP_SECURE_REGION_ENABLE_SET(x) ((x & 0x01) << 0)

#endif /* _PARKER_TP_REG_H_ */
