/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019 RealTek Inc
 */
#ifndef _RTK_DPTX_UTILS_H
#define _RTK_DPTX_UTILS_H

#include <linux/types.h>

#define GET_H_BYTE(x) (((x) >> 24) & 0xFF)
#define GET_MH_BYTE(x) (((x) >> 16) & 0xFF)
#define GET_ML_BYTE(x) (((x) >> 8) & 0xFF)
#define GET_L_BYTE(x) ((x) & 0xFF)

#define DP_LINK_RATE_1_62	162000
#define DP_LINK_RATE_2_7	270000
#define DP_LINK_RATE_5_4	540000
#define DP_LINK_RATE_8_1	810000

#define RTK_DP_COLORBIT_6	0x0
#define RTK_DP_COLORBIT_8	0x1
#define RTK_DP_COLORBIT_10	0x2
#define RTK_DP_COLORBIT_12	0x3
#define RTK_DP_COLORBIT_16	0x4
#define RTK_COLOR_FORMAT_RGB	0
#define RTK_COLOR_FORMAT_YUV444	1
#define RTK_COLOR_FORMAT_YUV422	2
#define RTK_COLOR_FORMAT_YUV420	3
#define RTK_PATTERN_IDLE	0
#define RTK_PATTERN_1		1
#define RTK_PATTERN_2		2
#define RTK_PATTERN_3		3
#define RTK_PATTERN_VIDEO	4
#define RTK_PATTERN_4		7

//{DPLL_M, F code, DPLL_O, Pixel Freq}
#define RTK_DP_PIXEL_PLL_TABLE_SIZE 64
static const uint32_t RTK_DP_PIXEL_PLL_TABLE[RTK_DP_PIXEL_PLL_TABLE_SIZE][4] = {
	{0x1A, 0x6B2, 5, 25175}, {0x1B, 0x7CF, 5, 26136},
	{0x1F, 0x7B4, 5, 29500}, {0x11, 0x000, 4, 33750},
	{0x12, 0x2AB, 4, 36000}, {0x14, 0x5A1, 4, 40000},
	{0x15, 0x12F, 4, 40750}, {0x15, 0x211, 4, 40936},
	{0x1A, 0x04C, 4, 49000}, {0x1C, 0x472, 4, 53250},
	{0x1C, 0x6D0, 4, 53750}, {0x1F, 0x2F6, 4, 58000},
	{0x21, 0x654, 4, 62085}, {0x0F, 0x555, 3, 63000},
	{0x0F, 0x5ED, 3, 63250}, {0x10, 0x0CE, 3, 64464},
	{0x10, 0x213, 3, 65000}, {0x10, 0x72F, 3, 67156},
	{0x11, 0x277, 3, 68540}, {0x11, 0x555, 3, 69750},
	{0x13, 0x000, 3, 74250}, {0x13, 0x555, 3, 76500},
	{0x14, 0x3C0, 3, 79207}, {0x14, 0x472, 3, 79500},
	{0x15, 0x1C7, 3, 81750}, {0x15, 0x5ED, 3, 83500},
	{0x15, 0x6CB, 3, 83866}, {0x16, 0x02E, 3, 84451},
	{0x16, 0x2AB, 3, 85500}, {0x16, 0x4A4, 3, 86333},
	{0x18, 0x540, 3, 93340}, {0x19, 0x354, 3, 95904},
	{0x1A, 0x472, 3, 99750}, {0x1A, 0x79A, 3, 101082},
	{0x1C, 0x281, 3, 105681}, {0x1C, 0x2AB, 3, 105750},
	{0x1C, 0x472, 3, 106500}, {0x1D, 0x000, 3, 108000},
	{0x1D, 0x000, 3, 108000}, {0x1D, 0x38E, 3, 109500},
	{0x1D, 0x602, 3, 110534}, {0x1E, 0x656, 3, 114048},
	{0x20, 0x6D0, 3, 121000}, {0x10, 0x0E4, 2, 129000},
	{0x10, 0x602, 2, 133320}, {0x11, 0x098, 2, 135500},
	{0x12, 0x000, 2, 141750}, {0x12, 0x555, 2, 146250},
	{0x13, 0x000, 2, 148500}, {0x14, 0x0ff, 2, 156100},
	{0x16, 0x3DE, 2, 172022}, {0x17, 0x092, 2, 175982},
	{0x17, 0x6D1, 2, 181250}, {0x1A, 0x2AB, 2, 198000},
	{0x1F, 0x098, 2, 230000}, {0x0e, 0x71b, 1, 241500},
	{0x13, 0x4F8, 1, 305395}, {0x14, 0x15B, 1, 312787},
	{0x14, 0x57B, 1, 319750}, {0x18, 0x7D7, 1, 377731},
	{0x19, 0x56A, 1, 387139}, {0x1A, 0x213, 1, 395000},
	{0x1C, 0x098, 1, 419500}, {0x10, 0x298, 0, 521750},
};

#endif /* _RTK_DPTX_UTILS_H */
