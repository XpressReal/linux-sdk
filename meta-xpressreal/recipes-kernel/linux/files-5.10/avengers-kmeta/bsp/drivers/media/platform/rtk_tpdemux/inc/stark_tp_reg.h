/*
 * stark_tp_reg.h
 *
 * Copyright (C) 2023 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _STARK_TP_REG_H_
#define _STARK_TP_REG_H_

#include "hank_tp_reg.h"

/* TF_CNTL */
#define TP_TF_CNTL_DYNC_CNTL_EN_BIT       (0x00000001 << 31)
#define TP_TF_CNTL_RAW_MODE_VALID_SEL_BIT (0x00000001 << 30)

#define TP_TF_CNTL_DYNC_CNTL_EN(x)        ((x & 0x01) << 31)
#define TP_TF_CNTL_RAW_MODE_VALID_SEL(x)  ((x & 0x01) << 30)

/* TF_INT */
#define TF_INT_CW_CRC_ERROR_TP_BIT (0x01 << 12)
#define TF_INT_CW_ILLEGAL_DEC_BIT  (0x01 << 11)
#define TF_INT_PID_ENC_ILLEGAL_BIT (0x01 << 10)
#define TF_INT_CW_ILLEGAL_ENC_BIT  (0x01 << 6)

#define TP_TF_INT_VALUE (0x1FFE)
#define TP_TF_INT_EN    (TF_INT_CW_ILLEGAL_ACTV_BIT|TF_INT_CW_ILLEGAL_ENC_BIT | \
			 TF_INT_CW_ILLEGAL_ALGO_BIT|TF_INT_CW_ILLEGAL_USAGE_BIT | \
			 TF_INT_CW_ILLEGAL_W_BIT|TF_INT_PID_ENC_ILLEGAL_BIT | \
			 TF_INT_CW_ILLEGAL_DEC_BIT|TF_INT_CW_CRC_ERROR_TP_BIT)
#define TP_TF_INT_EN_MASK(x)    (x & 0x1FFE)
#define TP_TF_INT_VALUE_MASK(x) (x & 0x1FFE)

/* TF_CTRL_SWC invalid def */
#define TP_TF_CTRL_SWC_SCRAMBLE(x) ((x & 0x00) << 3)

/* TP_PID_DATA3 */
#ifdef TP_PID_DATA3_ENC_KEY_ODD
#undef TP_PID_DATA3_ENC_KEY_ODD
#define TP_PID_DATA3_ENC_KEY_ODD(x) ((x & 0x01) << 19)
#endif

#ifdef TP_PID_DATA3_ENC
#undef TP_PID_DATA3_ENC
#define TP_PID_DATA3_ENC(x) ((x & 0x03) << 17)
#endif

/* DES CNTL */
#ifdef TP_KEY_HEADER_ACTIVATE
#undef TP_KEY_HEADER_ACTIVATE
#define TP_KEY_HEADER_ACTIVATE(x)  ((x & 0x03) << 12)
#endif

#define TP_KEY_HEADER_RANDOM_DELAY(x) ((x & 0x03) << 14)

#define TP_KEY_HEADER_ENC(x) ((x & 0x03) << 10)
#define TP_KEY_HEADER_DEC(x) ((x & 0x03) << 8)

#define CW_ENC_OFF (0x1)
#define CW_ENC_ON  (0x2)
#define CW_DEC_OFF (0x1)
#define CW_DEC_ON  (0x2)

#define CW_RANDOM_DELAY_OFF (0x1)

#define CSAv3_CP (0x7)

#endif /* _STARK_TP_REG_H_ */
