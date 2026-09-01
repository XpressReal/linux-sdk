// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_dprx.h"
#include <trace/events/rtk_dprx_trace.h>

#define SCALE_COEF_SIZE  32
static const unsigned long dprx_scale_ratio8[SCALE_COEF_SIZE] = {
	0x3800, 0x4800, 0x5000, 0x5800, 0x6000, 0x6800, 0x7000, 0x7800,
	0x8000, 0x8800, 0x9000, 0x9800, 0xa000, 0xa800, 0xb000, 0xb800,
	0xc000, 0xc800, 0xd000, 0xd800, 0xe000, 0xe800, 0xf000, 0xf800,
	0x12000, 0x14000, 0x16000, 0x18000, 0x1a000, 0x1c000, 0x1e000, 0x20000
};

/* 4taps-- Vertical sampling */
static const short dprx_coef_4t8p_ratio8[SCALE_COEF_SIZE][4][4] = {
	/* for all upscale ratio */
	{
		{   -8,   256,    16,    -8 },
		{  -24,   240,    48,    -8 },
		{  -28,   212,    88,   -16 },
		{  -28,   176,   132,   -24 },
	},
	/* I=8, D=9, wp= 0.111, ws= 0.122, alpha= 0.100 */
	{
		{  -24,   241,     5,    34 },
		{  -47,   249,    46,     8 },
		{  -60,   237,    99,   -20 },
		{  -59,   204,   155,   -44 },
	},
	/* I=8, D=10, wp= 0.100, ws= 0.110, alpha= 0.100 */
	{
		{   -1,   222,    28,     7 },
		{  -26,   230,    67,   -15 },
		{  -44,   223,   113,   -36 },
		{  -53,   198,   160,   -49 },
	},
	/* I=8, D=11, wp= 0.091, ws= 0.100, alpha= 0.100 */
	{
		{   22,   208,    47,   -21 },
		{   -4,   212,    82,   -34 },
		{  -25,   204,   120,   -43 },
		{  -40,   186,   156,   -46 },
	},
	/* I=8, D=12, wp= 0.083, ws= 0.092, alpha= 0.100 */
	{
		{   36,   198,    62,   -40 },
		{   12,   197,    92,   -45 },
		{   -9,   188,   122,   -45 },
		{  -27,   172,   150,   -39 },
	},
	/* I=8, D=13, wp= 0.077, ws= 0.085, alpha= 0.100 */
	{
		{   47,   190,    72,   -53 },
		{   24,   186,    97,   -51 },
		{    2,   177,   121,   -44 },
		{  -17,   163,   143,   -33 },
	},
	/* I=8, D=14, wp= 0.071, ws= 0.079, alpha= 0.100 */
	{
		{   55,   184,    78,   -61 },
		{   32,   176,   100,   -52 },
		{   10,   167,   120,   -41 },
		{   -9,   154,   138,   -27 },
	},
	/* I=8, D=15, wp= 0.067, ws= 0.073, alpha= 0.100 */
	{
		{   61,   178,    82,   -65 },
		{   38,   169,   101,   -52 },
		{   16,   160,   118,   -38 },
		{   -3,   146,   134,   -21 },
	},
	/* I=8, D=16, wp= 0.063, ws= 0.069, alpha= 0.100 */
	{
		{   65,   172,    85,   -66 },
		{   42,   163,   101,   -50 },
		{   21,   153,   116,   -34 },
		{    1,   142,   130,   -17 },
	},
	/* I=8, D=17, wp= 0.059, ws= 0.065, alpha= 0.100 */
	{
		{   67,   167,    87,   -65 },
		{   45,   158,   101,   -48 },
		{   25,   147,   114,   -30 },
		{    6,   137,   126,   -13 },
	},
	/* I=8, D=18, wp= 0.056, ws= 0.061, alpha= 0.100 */
	{
		{   69,   162,    88,   -63 },
		{   48,   152,   100,   -44 },
		{   28,   142,   112,   -26 },
		{   10,   133,   122,    -9 },
	},
	/* I=8, D=19, wp= 0.053, ws= 0.058, alpha= 0.100 */
	{
		{   71,   156,    88,   -59 },
		{   50,   147,    99,   -40 },
		{   31,   137,   110,   -22 },
		{   13,   129,   119,    -5 },
	},
	/* I=8, D=20, wp= 0.050, ws= 0.055, alpha= 0.100 */
	{
		{   72,   151,    88,   -55 },
		{   52,   142,    98,   -36 },
		{   34,   133,   107,   -18 },
		{   16,   125,   116,    -1 },
	},
	/* I=8, D=21, wp= 0.048, ws= 0.052, alpha= 0.100 */
	{
		{   72,   147,    87,   -50 },
		{   53,   138,    96,   -31 },
		{   36,   128,   105,   -13 },
		{   19,   121,   113,     3 },
	},
	/* I=8, D=22, wp= 0.045, ws= 0.050, alpha= 0.100 */
	{
		{   73,   140,    87,   -44 },
		{   55,   132,    95,   -26 },
		{   38,   125,   102,    -9 },
		{   23,   116,   110,     7 },
	},
	/* I=8, D=23, wp= 0.043, ws= 0.048, alpha= 0.100 */
	{
		{   73,   135,    86,   -38 },
		{   56,   127,    93,   -20 },
		{   40,   120,   100,    -4 },
		{   25,   113,   107,    11 },
	},
	/* I=8, D=24, wp= 0.042, ws= 0.046, alpha= 0.100 */
	{
		{   73,   129,    85,   -31 },
		{   57,   123,    91,   -15 },
		{   42,   117,    97,     0 },
		{   28,   109,   104,    15 },
	},
	/* I=8, D=25, wp= 0.040, ws= 0.044, alpha= 0.100 */
	{
		{   72,   125,    83,   -24 },
		{   58,   118,    89,    -9 },
		{   44,   112,    95,     5 },
		{   31,   106,   101,    18 },
	},
	/* I=8, D=26, wp= 0.038, ws= 0.042, alpha= 0.100 */
	{
		{   72,   120,    82,   -18 },
		{   59,   114,    87,    -4 },
		{   46,   108,    93,     9 },
		{   34,   102,    98,    22 },
	},
	/* I=8, D=27, wp= 0.037, ws= 0.041, alpha= 0.100 */
	{
		{   72,   114,    81,   -11 },
		{   59,   109,    86,     2 },
		{   48,   104,    90,    14 },
		{   36,   100,    95,    25 },
	},
	/* I=8, D=28, wp= 0.036, ws= 0.039, alpha= 0.100 */
	{
		{   71,   111,    79,    -5 },
		{   60,   105,    84,     7 },
		{   49,   101,    88,    18 },
		{   39,    97,    92,    28 },
	},
	/* I=8, D=29, wp= 0.034, ws= 0.038, alpha= 0.100 */
	{
		{   71,   106,    78,     1 },
		{   60,   102,    82,    12 },
		{   51,    97,    86,    22 },
		{   41,    94,    90,    31 },
	},
	/* I=8, D=30, wp= 0.033, ws= 0.037, alpha= 0.100 */
	{
		{   70,   103,    77,     6 },
		{   61,    98,    81,    16 },
		{   52,    95,    84,    25 },
		{   43,    91,    88,    34 },
	},
	/* I=8, D=31, wp= 0.032, ws= 0.035, alpha= 0.100 */
	{
		{   70,    99,    76,    11 },
		{   61,    96,    79,    20 },
		{   53,    92,    82,    29 },
		{   45,    88,    86,    37 },
	},
	/* I=8, D=36, wp= 0.028, ws= 0.031, alpha= 0.100 */
	{
		{   68,    86,    72,    30 },
		{   62,    84,    74,    36 },
		{   57,    82,    76,    41 },
		{   52,    79,    78,    47 },
	},
	/* I=8, D=40, wp= 0.025, ws= 0.028, alpha= 0.100 */
	{
		{   67,    79,    70,    40 },
		{   63,    78,    71,    44 },
		{   59,    76,    73,    48 },
		{   55,    76,    74,    51 },
	},
	/* I=8, D=44, wp= 0.023, ws= 0.025, alpha= 0.100 */
	{
		{   66,    76,    68,    46 },
		{   63,    75,    69,    49 },
		{   60,    74,    70,    52 },
		{   57,    72,    72,    55 },
	},
	/* I=8, D=48, wp= 0.021, ws= 0.023, alpha= 0.100 */
	{
		{   66,    73,    67,    50 },
		{   63,    73,    68,    52 },
		{   61,    72,    69,    54 },
		{   59,    70,    70,    57 },
	},
	/* I=8, D=52, wp= 0.019, ws= 0.021, alpha= 0.100 */
	{
		{   65,    71,    67,    53 },
		{   64,    70,    67,    55 },
		{   62,    70,    68,    56 },
		{   60,    69,    69,    58 },
	},
	/* I=8, D=56, wp= 0.018, ws= 0.020, alpha= 0.100 */
	{
		{   65,    70,    66,    55 },
		{   64,    69,    67,    56 },
		{   62,    69,    67,    58 },
		{   61,    68,    68,    59 },
	},
	/* I=8, D=60, wp= 0.017, ws= 0.018, alpha= 0.100 */
	{
		{   65,    69,    66,    56 },
		{   64,    68,    66,    58 },
		{   63,    67,    67,    59 },
		{   61,    68,    67,    60 },
	},
	/* I=8, D=64, wp= 0.016, ws= 0.017, alpha= 0.100 */
	{
		{   65,    67,    66,    58 },
		{   64,    67,    66,    59 },
		{   63,    67,    66,    60 },
		{   62,    66,    67,    61 },
	},
};

static void set_hs_scaler(struct rtk_dprx *dprx,
		u32 hsi_offset, u32 hsi_phase,
		u32 hsd_out, u32 hsd_delta)
{
	dev_dbg(dprx->dev, "hsd_out=%u, hsd_delta=0x%x\n",
		hsd_out, hsd_delta);

	dprx->rbus_ops->write(DPRX14_SCALER_HSI,
		DPRX14_SCALER_HSI_hsi_offset(hsi_offset) |
		DPRX14_SCALER_HSI_hsi_phase(hsi_phase));

	dprx->rbus_ops->write(DPRX14_SCALER_HSD,
		DPRX14_SCALER_HSD_hsd_out(hsd_out) |
		DPRX14_SCALER_HSD_hsd_delta(hsd_delta));
}

static u32 get_ratio8_index(u32 delta)
{
	u32 i;
	u32 idx = 0;

	/* upscaling */
	if (delta < 0x4000)
		goto exit;

	/*
	 * VscaleRatio8 is incrementing, we find the first one that's bigger
	 * than or equal to delta instead of min ads diff.
	 * This way it will round up to lower pass filter to avoid alias
	 */
	for (i = 1; i < SCALE_COEF_SIZE; i++) {
		if (dprx_scale_ratio8[i] >= delta) {
			idx = i;
			goto exit;
		}
	}

	/* last index */
	idx = SCALE_COEF_SIZE - 1;

exit:
	return idx;
}

static void get_scaling_coeffs(struct rtk_dprx *dprx,
	u32 *coeff, int delta)
{
	int i;
	int x;
	int y;
	u32 idx;
	int taps;
	short const *p;

	taps = 4;
	idx = get_ratio8_index(delta);
	p = dprx_coef_4t8p_ratio8[idx][0];

	dev_dbg(dprx->dev, "%s idx=%d\n", __func__, idx);

	for (i = 0; i < (taps << 2); i++) {
		x = i &  7;
		y = i >> 3;

		coeff[i] =
			(x < 4 ? p[x * taps + taps-1-y] : p[(7-x) * taps + y]) << 4;
	}
}

static void set_hs_coeff(struct rtk_dprx *dprx, u32 delta)
{
	u32 c[16];

	get_scaling_coeffs(dprx, c, delta);

	/* for Y */
	dprx->rbus_ops->write(DPRX14_SCALER_HSYC0,
		 DPRX14_SCALER_HSYC0_hsyc0_c1(c[1]) |
		 DPRX14_SCALER_HSYC0_hsyc0_c0(c[0]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSYC1,
		 DPRX14_SCALER_HSYC1_hsyc1_c1(c[3]) |
		 DPRX14_SCALER_HSYC1_hsyc1_c0(c[2]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSYC2,
		 DPRX14_SCALER_HSYC2_hsyc2_c1(c[5]) |
		 DPRX14_SCALER_HSYC2_hsyc2_c0(c[4]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSYC3,
		 DPRX14_SCALER_HSYC3_hsyc3_c1(c[7]) |
		 DPRX14_SCALER_HSYC3_hsyc3_c0(c[6]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSYC4,
		 DPRX14_SCALER_HSYC4_hsyc4_c1(c[9]) |
		 DPRX14_SCALER_HSYC4_hsyc4_c0(c[8]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSYC5,
		 DPRX14_SCALER_HSYC5_hsyc5_c1(c[11]) |
		 DPRX14_SCALER_HSYC5_hsyc5_c0(c[10]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSYC6,
		 DPRX14_SCALER_HSYC6_hsyc6_c1(c[13]) |
		 DPRX14_SCALER_HSYC6_hsyc6_c0(c[12]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSYC7,
		 DPRX14_SCALER_HSYC7_hsyc7_c1(c[15]) |
		 DPRX14_SCALER_HSYC7_hsyc7_c0(c[14]));

	/* for U,V */
	dprx->rbus_ops->write(DPRX14_SCALER_HSCC0,
		 DPRX14_SCALER_HSCC0_hscc0_c1(c[1]) |
		 DPRX14_SCALER_HSCC0_hscc0_c0(c[0]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSCC1,
		 DPRX14_SCALER_HSCC1_hscc1_c1(c[3]) |
		 DPRX14_SCALER_HSCC1_hscc1_c0(c[2]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSCC2,
		 DPRX14_SCALER_HSCC2_hscc2_c1(c[5]) |
		 DPRX14_SCALER_HSCC2_hscc2_c0(c[4]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSCC3,
		 DPRX14_SCALER_HSCC3_hscc3_c1(c[7]) |
		 DPRX14_SCALER_HSCC3_hscc3_c0(c[6]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSCC4,
		 DPRX14_SCALER_HSCC4_hscc4_c1(c[9]) |
		 DPRX14_SCALER_HSCC4_hscc4_c0(c[8]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSCC5,
		 DPRX14_SCALER_HSCC5_hscc5_c1(c[11]) |
		 DPRX14_SCALER_HSCC5_hscc5_c0(c[10]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSCC6,
		 DPRX14_SCALER_HSCC6_hscc6_c1(c[13]) |
		 DPRX14_SCALER_HSCC6_hscc6_c0(c[12]));
	dprx->rbus_ops->write(DPRX14_SCALER_HSCC7,
		 DPRX14_SCALER_HSCC7_hscc7_c1(c[15]) |
		 DPRX14_SCALER_HSCC7_hscc7_c0(c[14]));
}

static void set_vs_scaler(struct rtk_dprx *dprx,
		u32 vsi_offset, u32 vsi_phase,
		u32 vsd_out, u32 vsd_delta)
{
	dev_dbg(dprx->dev, "vsd_out=%u,vsd_delta=0x%x\n",
		vsd_out, vsd_delta);

	dprx->rbus_ops->write(DPRX14_SCALER_VSI,
		DPRX14_SCALER_VSI_vsi_offset(vsi_offset) |
		DPRX14_SCALER_VSI_vsi_phase(vsi_phase));

	dprx->rbus_ops->write(DPRX14_SCALER_VSD,
		DPRX14_SCALER_VSD_vsd_out(vsd_out) |
		DPRX14_SCALER_VSD_vsd_delta(vsd_delta));
}

static void set_vs_coeff(struct rtk_dprx *dprx)
{
	u32 c0;
	u32 c1;
	u32 c2;
	u32 c3;

	c0 = 0x02e80203;
	c1 = 0x06d604a5;
	c2 = 0x0b5b092a;
	c3 = 0x0dfd0d18;

	dprx->rbus_ops->write(DPRX14_SCALER_VSYC0, c0);
	dprx->rbus_ops->write(DPRX14_SCALER_VSYC1, c1);
	dprx->rbus_ops->write(DPRX14_SCALER_VSYC2, c2);
	dprx->rbus_ops->write(DPRX14_SCALER_VSYC3, c3);

	dprx->rbus_ops->write(DPRX14_SCALER_VSCC0, c0);
	dprx->rbus_ops->write(DPRX14_SCALER_VSCC1, c1);
	dprx->rbus_ops->write(DPRX14_SCALER_VSCC2, c2);
	dprx->rbus_ops->write(DPRX14_SCALER_VSCC3, c3);
}

static void rtk_dprx_scale_down(struct rtk_dprx *dprx,
		u32 src_width, u32 src_height, u32 dst_width, u32 dst_height)
{
	u32 delta_num, delta_den, offset, phase;

	/* set hs_scaler */
	offset = 0;
	phase = 0;
	delta_num = (src_width / dst_width) << 14;
	delta_den = ((src_width % dst_width)*0x4000) / dst_width;
	/* set_hs_scaler: offset,phase,out,delta */
	set_hs_scaler(dprx, offset, phase,
		dst_width, (delta_num | delta_den));
	set_hs_coeff(dprx, delta_num);

	/* set vs_scaler */
	offset = 0;
	phase = 0;
	delta_num = (src_height / dst_height) << 14;
	delta_den = ((src_height % dst_height)*0x4000) / dst_height;
	/* set_vs_scaler: offset,phase,out,delta */
	set_vs_scaler(dprx, offset, phase,
		dst_height, (delta_num | delta_den));
	set_vs_coeff(dprx);
}

static u32 rtk_dprx_calculate_line_pitch(u32 dst_width, bool compenc_mode)
{
	u32 pitch = 0;

	if (compenc_mode) {
		pitch = (dst_width + 15) / 16;
		pitch = (pitch + 3) / 4;
		pitch = pitch * 4 * 64;
	} else {
		/* line mode */
		pitch = roundup(dst_width, 16);
	}

	return pitch;
}

static u32 rtk_dprx_calculate_header_pitch(u32 dst_width, bool compenc_mode)
{
	u32 pitch = 0;

	/* There is no header in line mode */
	if (!compenc_mode)
		return pitch;

	pitch = (dst_width + 15) / 16;
	pitch = (pitch + 3) / 4;
	pitch = pitch * 8;
	pitch = (pitch + 63) / 64;
	pitch = pitch * 64;

	return pitch;
}

static u32 rtk_dprx_calculate_video_size(u32 dst_width, u32 dst_height, bool compenc_mode)
{
	u32 line_size;
	u32 header_size;
	u32 line_pitch;
	u32 header_pitch;

	line_pitch = rtk_dprx_calculate_line_pitch(dst_width, compenc_mode);
	header_pitch = rtk_dprx_calculate_header_pitch(dst_width, compenc_mode);

	if (!compenc_mode) {
		line_size = line_pitch * dst_height;
	} else {
		line_size = (dst_height + 3)/4;
		line_size = line_pitch * line_size;
	}

	line_size += line_size/2;

	if (!compenc_mode)
		return line_size;

	header_size = (dst_height + 3)/4;
	header_size = header_pitch * header_size;
	header_size += header_size/2;

	return line_size + header_size;
}

static void rtk_dprx_video_size_cfg(struct rtk_dprx *dprx)
{
	dev_info(dprx->dev, "%s src_width=%u src_height=%u dst_width=%u dst_height=%u  mode=%s\n",
		__func__,
		dprx->src_width, dprx->src_height,
		dprx->dst_width, dprx->dst_height,
		dprx->compenc_mode ? "COMPENC":"LINE");

	dprx->rbus_ops->write(DPRX14_SIZE1,
		DPRX14_SIZE1_src_width(dprx->src_width) |
		DPRX14_SIZE1_src_height(dprx->src_height));


	dprx->line_pitch = rtk_dprx_calculate_line_pitch(dprx->dst_width, dprx->compenc_mode);

	/* HW, line_pitch unit- 16byte */
	dprx->rbus_ops->write(DPRX14_LINE_PITCH,
		DPRX14_LINE_PITCH_line_pitch_c(dprx->line_pitch/16) |
		DPRX14_LINE_PITCH_line_pitch_y(dprx->line_pitch/16));

	if (!dprx->compenc_mode) {
		dprx->header_pitch = 0;
		goto exit;
	}

	dprx->header_pitch = rtk_dprx_calculate_header_pitch(dprx->dst_width, dprx->compenc_mode);

	/* HW, header_pitch unit - 64byte */
	dprx->rbus_ops->write(DPRX14_COMPENC_3,
			DPRX14_COMPENC_3_header_pitch_c(dprx->header_pitch/64) |
			DPRX14_COMPENC_3_header_pitch_y(dprx->header_pitch/64));

exit:
	dprx->video_size = rtk_dprx_calculate_video_size(dprx->dst_width,
			dprx->dst_height, dprx->compenc_mode);

	trace_dprx_video_size_cfg(dprx->src_width, dprx->src_height,
		dprx->dst_width, dprx->dst_height, dprx->compenc_mode,
		dprx->line_pitch, dprx->header_pitch, dprx->video_size);
}

static void rtk_dprx_dma_buf_cfg(struct rtk_dprx *dprx,
		u8 entry_index, uint64_t start_addr)
{
	u32 offset;
	uint64_t size;
	uint64_t y_header_sa, y_header_ea;
	uint64_t c_header_sa, c_header_ea;
	uint64_t y_body_sa, y_body_ea;
	uint64_t c_body_sa, c_body_ea;
	uint64_t meta_sa;

	if (entry_index > DMA_ENTRY_3) {
		dev_err(dprx->dev, "dma_buf_cfg failed, invalid entry_index=%u\n", entry_index);
		return;
	}

	offset = (DPRX14_DMA_ENTRY_10 - DPRX14_DMA_ENTRY_00) * entry_index;

	if (!dprx->compenc_mode) {
		size = (uint64_t)dprx->line_pitch * dprx->dst_height;
	} else {
		size = (dprx->dst_height + 3)/4;
		size = dprx->header_pitch * size;
	}

	y_header_sa = start_addr;
	y_header_ea = y_header_sa + size;

	c_header_sa = y_header_ea;
	c_header_ea = c_header_sa + size/2;

	/* Y Header SA/EA */
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_00 + offset,
			DPRX14_DMA_ENTRY_00_entry0_sa_y_header(y_header_sa/16));
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_01 + offset,
			DPRX14_DMA_ENTRY_01_entry0_ea_y_header(y_header_ea/16));

	/* C Header SA/EA */
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_04 + offset,
			DPRX14_DMA_ENTRY_04_entry0_sa_c_header(c_header_sa/16));
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_05 + offset,
			DPRX14_DMA_ENTRY_05_entry0_ea_c_header(c_header_ea/16));

	if (!dprx->compenc_mode) {
		c_body_ea = c_header_ea;
		meta_sa = c_header_ea;
		goto skip_body;
	}

	size = (dprx->dst_height + 3)/4;
	size = dprx->line_pitch * size;
	y_body_sa = c_header_ea;
	y_body_ea = y_body_sa + size;
	c_body_sa = y_body_ea;
	c_body_ea = c_body_sa + size/2;

	/* Y Body SA/EA */
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_02 + offset,
			DPRX14_DMA_ENTRY_02_entry0_sa_y_body(y_body_sa/16));
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_03 + offset,
			DPRX14_DMA_ENTRY_03_entry0_ea_y_body(y_body_ea/16));

	/* C Body SA/EA */
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_06 + offset,
			DPRX14_DMA_ENTRY_06_entry0_sa_c_body(c_body_sa/16));
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_07 + offset,
			DPRX14_DMA_ENTRY_07_entry0_ea_c_body(c_body_ea/16));

	meta_sa = c_body_ea;

skip_body:

	/* Meta data(48bytes) SA */
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_08 + offset,
			DPRX14_DMA_ENTRY_08_entry0_ma(meta_sa/16));

	/* Indicate dma buf is ready */
	dprx->rbus_ops->write(DPRX14_DMA_ENTRY_09 + offset,
			DPRX14_DMA_ENTRY_09_write_en1(1) |
			DPRX14_DMA_ENTRY_09_entry0_valid(1));

	trace_dprx_dma_buf_cfg(entry_index, start_addr,
			c_body_ea - y_header_sa);

}

static u8 rtk_dprx_is_frame_done(u32 done_st, u8 entry_index)
{
	u8 is_done;
	u32 bit_shift;

	if (entry_index > DMA_ENTRY_3)
		return 0;

	/* DPRX14_INT_STS_SCPU_1 */
	bit_shift = 1 + entry_index;

	is_done = (done_st & BIT(bit_shift)) >> bit_shift;

	// TODO: trace frame done

	return is_done;
}

static void rtk_dprx_clear_done_flag(struct rtk_dprx *dprx,
		u8 entry_index)
{
	u32 bit_shift;

	bit_shift = 1 + entry_index;
	dprx->rbus_ops->write(DPRX14_INT_STS_SCPU_1, BIT(bit_shift));
}

static void rtk_dprx_clear_all_flags(struct rtk_dprx *dprx)
{
	u32 reg_val = 0;

	dprx->rbus_ops->read(DPRX14_INT_STS_SCPU_0, &reg_val);
	dprx->rbus_ops->write(DPRX14_INT_STS_SCPU_0, reg_val);

	dprx->rbus_ops->read(DPRX14_INT_STS_SCPU_1, &reg_val);
	dprx->rbus_ops->write(DPRX14_INT_STS_SCPU_1, reg_val);
}

static void rtk_dprx_meta_swap(struct rtk_dprx *dprx, u8 enable)
{
	dev_dbg(dprx->dev, "%s wrap meta SWAP\n", enable ? "Enable":"Disable");

	dprx->rbus_ops->write(DPRX14_SWAP_SEL,
			DPRX14_SWAP_SEL_cti_wdata_swap(0) |
			DPRX14_SWAP_SEL_reg_metadata_byte_swap(enable) |
			DPRX14_SWAP_SEL_reg_header_byte_swap(0) |
			DPRX14_SWAP_SEL_reg_body_byte_swap(0));
}

static void rtk_dprx_crc(struct rtk_dprx *dprx, u8 enable)
{
	dev_dbg(dprx->dev, "%s CRC function\n", enable ? "Enable":"Disable");

	dprx->rbus_ops->write(DPRX14_CRC, DPRX14_CRC_crc_enable(enable));
}

static void rtk_dprx_color_bar(struct rtk_dprx *dprx, u8 enable)
{
	dev_dbg(dprx->dev, "%s color bar width=%u height=%u\n",
		enable ? "Enable":"Disable",  dprx->src_width, dprx->src_height);

	dprx->rbus_ops->write(DPRX14_COLOR_BAR_0,
			DPRX14_COLOR_BAR_0_color_bar_hf(6) |
			DPRX14_COLOR_BAR_0_color_bar_heigh(dprx->src_height) |
			DPRX14_COLOR_BAR_0_color_bar_width(dprx->src_width));

	dprx->rbus_ops->write(DPRX14_COLOR_BAR_1,
			DPRX14_COLOR_BAR_1_color_bar_sel(enable) |
			DPRX14_COLOR_BAR_1_color_bar_en(enable) |
			DPRX14_COLOR_BAR_1_color_bar_vb(3) |
			DPRX14_COLOR_BAR_1_color_bar_vs(5) |
			DPRX14_COLOR_BAR_1_color_bar_vf(2));
}

static void rtk_dprx_wrapper_config(struct rtk_dprx *dprx,
		u8 src_fmt, u8 scaling_down, u8 is_compenc, u8 enable)
{
	u8 rgb_cc_en = 0;
	u8 y422_chroma_us_en = 0;
	u8 y444_chroma_ds_en = 1;

	if (src_fmt == SRC_COLOR_FMT_RGB) {
		rgb_cc_en = 1;
	} else if ((src_fmt == SRC_COLOR_FMT_Y422) || (src_fmt == SRC_COLOR_FMT_Y420)) {
		if (scaling_down)
			y422_chroma_us_en = 1;
		else
			y444_chroma_ds_en = 0;
	}

	dev_info(dprx->dev, "src_fmt=%u scaling_down=%u rgb_cc_en=%u y422_chroma_us_en=%u y444_chroma_ds_en=%u\n",
		src_fmt, scaling_down, rgb_cc_en, y422_chroma_us_en, y444_chroma_ds_en);

	if (enable && rgb_cc_en) {
		dprx->rbus_ops->write(DPRX14_CC1, 0x3ed1020e);
		dprx->rbus_ops->write(DPRX14_CC2, 0x04080383);
		dprx->rbus_ops->write(DPRX14_CC3, 0x3d0e3dac);
		dprx->rbus_ops->write(DPRX14_CC4, 0x038300c9);
		dprx->rbus_ops->write(DPRX14_CC5, 0x00003f6f);
		dprx->rbus_ops->write(DPRX14_CC6, 0x00800010);
		dprx->rbus_ops->write(DPRX14_CC7, 0x00000080);
		usleep_range(1000, 1050);
	}

	if ((src_fmt == SRC_COLOR_FMT_Y420) && enable) {
		unsigned long time_start;
		u32 reg_val = 0;

		/* Disable 420to422 */
		dprx->rbus_ops->mask_write(DPRX14_DMY1, _BIT0, 0);
		/* Clear vsync ints */
		dprx->rbus_ops->write(DPRX14_DMY0, _BIT1);

		/* Wait next vsync ints */
		time_start = jiffies;
		while (time_before(jiffies, time_start + msecs_to_jiffies(21))) {
			dprx->rbus_ops->read(DPRX14_DMY0, &reg_val);
			if (reg_val & _BIT0)
				break;
		}
		/* Enable 420to422 immediately, must enable before active data arrives (around 7us) */
		dprx->rbus_ops->mask_write(DPRX14_DMY1, _BIT0, _BIT0);
		if (!(reg_val & _BIT0))
			dev_warn(dprx->dev, "420to422: vsync not detected within 21ms, enabling anyway\n");
	}

	dprx->rbus_ops->write(DPRX14_CTRL0,
		DPRX14_CTRL0_frame_ed_sel(0) |
		DPRX14_CTRL0_rgb_cc_en(rgb_cc_en) |
		DPRX14_CTRL0_yuv420_chroma_us_en(scaling_down) |
		DPRX14_CTRL0_fs_clr_option(1) |
		DPRX14_CTRL0_yuv422_chroma_ds_en(1) |
		DPRX14_CTRL0_yuv444_chroma_ds_mode(1) |
		DPRX14_CTRL0_yuv444_chroma_ds_en(y444_chroma_ds_en) |
		DPRX14_CTRL0_vs_en(scaling_down) |
		DPRX14_CTRL0_vs_near(0) |
		DPRX14_CTRL0_vs_yodd(0) |
		DPRX14_CTRL0_vs_codd(0) |
		DPRX14_CTRL0_hs_en(scaling_down) |
		DPRX14_CTRL0_hs_yodd(0) |
		DPRX14_CTRL0_hs_codd(0) |
		DPRX14_CTRL0_yuv422_chroma_us_mode(1) |
		DPRX14_CTRL0_yuv422_chroma_us_en(y422_chroma_us_en) |
		DPRX14_CTRL0_src_fmt(src_fmt) |
		DPRX14_CTRL0_comp_linear(is_compenc) |
		DPRX14_CTRL0_sw_rst(0) |
		DPRX14_CTRL0_go(enable));

	trace_dprx_wrapper_config(src_fmt, scaling_down, is_compenc, enable);
}

static void rtk_dprx_sw_reset(struct rtk_dprx *dprx)
{
	u32 reg_val = 0;

	dprx->rbus_ops->read(DPRX14_DMA_STS, &reg_val);
	if (DPRX14_DMA_STS_get_ch_fsm(reg_val) ||
		DPRX14_DMA_STS_get_cti_buf_req(reg_val) ||
		DPRX14_DMA_STS_get_cti_data_left_cnt(reg_val)) {
		dev_err(dprx->dev, "Abnormal HW state DPRX14_DMA_STS=0x%08x, skip sw_rst\n",
			reg_val);
		return;
	}

	dev_dbg(dprx->dev, "do sw_rst\n");

	dprx->rbus_ops->mask_write(DPRX14_CTRL0,
		DPRX14_CTRL0_sw_rst_mask, DPRX14_CTRL0_sw_rst(1));

	dprx->rbus_ops->mask_write(DPRX14_CTRL0,
		DPRX14_CTRL0_sw_rst_mask, DPRX14_CTRL0_sw_rst(0));
}

static void rtk_dprx_dma_go_ctrl(struct rtk_dprx *dprx, u8 enable)
{
	u8 is_compenc;
	u8 scaling_down;
	u8 src_fmt;

	is_compenc = dprx->compenc_mode;
	src_fmt = dprx->src_fmt;

	if ((dprx->src_width > dprx->dst_width) ||
		(dprx->src_height > dprx->dst_height))
		scaling_down = 1;
	else
		scaling_down = 0;

	dev_dbg(dprx->dev, "%s scaling_down=%s is_compenc=%s\n",
			__func__,
			scaling_down ? "Y" : "N",
			is_compenc ? "Y" : "N");

	if (scaling_down && enable)
		rtk_dprx_scale_down(dprx, dprx->src_width, dprx->src_height,
			dprx->dst_width, dprx->dst_height);

	rtk_dprx_wrapper_config(dprx, src_fmt, scaling_down, is_compenc, enable);

	if (enable)
		return;

	/* Wait HW finished cti process for 10us before sw_rst */
	udelay(10);

	rtk_dprx_sw_reset(dprx);
}

static void rtk_dprx_interrupt_ctrl(struct rtk_dprx *dprx, u8 enable)
{
	u32 mask0, mask1, mask2;
	u32 val0 = 0, val1 = 0, val2 = 0;

	/* Clear all flags before enable interrupt */
	if (enable)
		rtk_dprx_clear_all_flags(dprx);

	dev_dbg(dprx->dev, "%s wrap interrupt\n",
		enable ? "Enable":"Disable");

	mask0 = DPRX14_INTEN_SCPU_0_entry_invalid_scpu_inten_mask |
		DPRX14_INTEN_SCPU_0_ovf_scpu_inten_e3_mask |
		DPRX14_INTEN_SCPU_0_ovf_scpu_inten_e2_mask |
		DPRX14_INTEN_SCPU_0_ovf_scpu_inten_e1_mask |
		DPRX14_INTEN_SCPU_0_ovf_scpu_inten_e0_mask;

	mask1 = DPRX14_INTEN_SCPU_1_mismatch_scpu_inten_mask |
		DPRX14_INTEN_SCPU_1_done_scpu_inten_e3_mask |
		DPRX14_INTEN_SCPU_1_done_scpu_inten_e2_mask |
		DPRX14_INTEN_SCPU_1_done_scpu_inten_e1_mask |
		DPRX14_INTEN_SCPU_1_done_scpu_inten_e0_mask;

	mask2 = DPRX14_INTEN_dprx_mac_inten_mask;

	if (enable) {
		val0 = mask0;
		val1 = mask1;
		val2 = mask2;
	}

	dev_dbg(dprx->dev, "DPRX14_INTEN_SCPU_0 mask=0x%08x val=0x%08x\n", mask0, val0);
	dev_dbg(dprx->dev, "DPRX14_INTEN_SCPU_1 mask=0x%08x val=0x%08x\n", mask1, val1);
	dev_dbg(dprx->dev, "DPRX14_INTEN mask=0x%08x val=0x%08x\n", mask2, val2);

	dprx->rbus_ops->mask_write(DPRX14_INTEN_SCPU_0, mask0, val0);
	dprx->rbus_ops->mask_write(DPRX14_INTEN_SCPU_1, mask1, val1);
	dprx->rbus_ops->mask_write(DPRX14_INTEN, mask2, val2);

	/* Clear all flags after disable interrupt */
	if (!enable)
		rtk_dprx_clear_all_flags(dprx);
}

static int rtk_dprx_get_intr_state(struct rtk_dprx *dprx, u32 *p_done_st)
{
	int ret = 0;
	u32 err_st = 0;
	u32 mismatch_mask = 0;

	dprx->rbus_ops->read(DPRX14_INT_STS_SCPU_0, &err_st);
	dprx->rbus_ops->read(DPRX14_INT_STS_SCPU_1, p_done_st);

	mismatch_mask = DPRX14_INT_STS_SCPU_1_scpu_mismatch_sts_mask;

	if (*p_done_st & mismatch_mask) {
		dprx->rbus_ops->write(DPRX14_INT_STS_SCPU_1, mismatch_mask);
		if (time_before(jiffies, dprx->streaming_start_jiffies + msecs_to_jiffies(50))) {
			dev_dbg(dprx->dev, "mismatch suppressed (streaming warmup)\n");
		} else {
			dev_err_ratelimited(dprx->dev, "mismatch: DPRX14_INT_STS_SCPU_1=0x%08x\n", *p_done_st);
			ret = -EIO;
		}
	}

	if (err_st) {
		dev_err_ratelimited(dprx->dev, "Error: DPRX14_INT_STS_SCPU_0=0x%08x\n", err_st);
		dprx->rbus_ops->write(DPRX14_INT_STS_SCPU_0, err_st);
	}

	return ret;
}

static const struct rtk_dprx_wrap_ops dprx_wrap_ops = {
	.scale_down = rtk_dprx_scale_down,
	.calculate_video_size = rtk_dprx_calculate_video_size,
	.video_size_cfg = rtk_dprx_video_size_cfg,
	.dma_buf_cfg = rtk_dprx_dma_buf_cfg,
	.is_frame_done = rtk_dprx_is_frame_done,
	.clear_done_flag = rtk_dprx_clear_done_flag,
	.meta_swap = rtk_dprx_meta_swap,
	.crc_ctrl = rtk_dprx_crc,
	.color_bar_test = rtk_dprx_color_bar,
	.dma_go_ctrl = rtk_dprx_dma_go_ctrl,
	.interrupt_ctrl = rtk_dprx_interrupt_ctrl,
	.get_intr_state = rtk_dprx_get_intr_state,
};

int rtk_dprx_wrap_init(struct rtk_dprx *dprx)
{

	dprx->wrap_ops = &dprx_wrap_ops;
	dprx->compenc_mode = false;

	dprx->src_width = 1920;
	dprx->src_height = 1080;
	dprx->src_fmt = SRC_COLOR_FMT_RGB;

	return 0;
}
