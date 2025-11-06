// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_mipi_csi.h"
#include "rtk_mipi_csi_trace.h"

static struct rtk_mipicsi_crt g_crt = {
	.crt_obtained = false,
	.phy_inited = false,
};

static const struct soc_device_attribute rtk_soc_kent[] = {
	{ .family = "Realtek Kent", },
	{ /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_prince[] = {
	{ .family = "Realtek Prince", },
	{ /* sentinel */ }
};

const struct rtk_mipicsi_phy dft_phy_cfg = {
	.hsamp0 = 0x8, .hsamp1 = 0x8, .hsamp2 = 0x8, .hsamp3 = 0x8,
	.offset_range_ckk = 0x0,
	.offset_range_lane3 = 0x0,
	.offset_range_lane2 = 0x0,
	.offset_range_lane1 = 0x0,
	.offset_range_lane0 = 0x0,
	.hsampck = 0x8, .lpamp = 0x7,
	.ctune = 9, /* BCD code */
	.rtune = 8, /* BCD code */
	.ileq_adj = 3,
	.ivga_adj = 2,
	.z0 = 0x32,
};

const unsigned long scale_ratio8[] = {
	0x3800, 0x4800, 0x5000, 0x5800, 0x6000, 0x6800, 0x7000, 0x7800,
	0x8000, 0x8800, 0x9000, 0x9800, 0xa000, 0xa800, 0xb000, 0xb800,
	0xc000, 0xc800, 0xd000, 0xd800, 0xe000, 0xe800, 0xf000, 0xf800,
	0x12000, 0x14000, 0x16000, 0x18000, 0x1a000, 0x1c000, 0x1e000, 0x20000
};

/* 4taps-- Vertical sampling */
const short coef_4t8p_ratio8[][4][4] = {
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

static void rtk_mask_write(struct regmap *map,
		u32 offset, u32 mask, u32 val)
{
	u32 orig = 0;
	u32 tmp = 0;

	regmap_read(map, offset, &orig);
	tmp = orig & ~mask;
	tmp |= val & mask;
	regmap_write(map, offset, tmp);
}

static u32 get_phy_offset(u8 top_index)
{
	u32 offset;

	if (top_index > 1)
		return 0;

	offset = (MIPI1_APHY_REG0 - MIPI0_APHY_REG0) * top_index;

	return offset;
}

static u32 get_app_offset(u8 ch_index)
{
	u32 offset;

	switch (ch_index) {
	case CH_0:
		offset = 0;
		break;
	case CH_1:
		offset = 0x200;
		break;
	case CH_2:
		offset = 0x400;
		break;
	case CH_3:
		offset = 0x600;
		break;
	case CH_4:
		offset = 0x800;
		break;
	case CH_5:
		offset = 0xa00;
		break;
	default:
		offset = 0;
		break;
	}

	return offset;
}

static u32 bcd_to_gray(u32 bcd)
{
	return bcd ^ (bcd >> 1);
}

static u32 gray_to_bcd(u32 gray)
{
	u32 bcd;

	switch (gray) {
	case 0x0:
		bcd = 0;
		break;
	case 0x1:
		bcd = 1;
		break;
	case 0x3:
		bcd = 2;
		break;
	case 0x2:
		bcd = 3;
		break;
	case 0x6:
		bcd = 4;
		break;
	case 0x7:
		bcd = 5;
		break;
	case 0x5:
		bcd = 6;
		break;
	case 0x4:
		bcd = 7;
		break;
	case 0xc:
		bcd = 8;
		break;
	case 0xd:
		bcd = 9;
		break;
	case 0xf:
		bcd = 10;
		break;
	case 0xe:
		bcd = 11;
		break;
	case 0xa:
		bcd = 12;
		break;
	case 0xb:
		bcd = 13;
		break;
	case 0x9:
		bcd = 14;
		break;
	case 0x8:
		bcd = 15;
		break;
	default:
		bcd = 0;
		break;
	}

	return bcd;
}

static u8 is_frame_done(u32 done_st,
	u8 ch_index, u8 entry_index)
{
	u8 is_done;
	u32 bit_shift;

	if ((ch_index > CH_5) || (entry_index > ENTRY_3))
		return 0;

	/* TOP_0_INT_STS_SCPU_1 */
	bit_shift = 1 + ch_index * 4 + entry_index;

	is_done = (done_st & BIT(bit_shift)) >> bit_shift;

	if (is_done)
		trace_mipicsi_frame_done(done_st, ch_index, entry_index);

	return is_done;
}

static bool is_hsamp_valid(u32 hsamp)
{
	bool valid = false;

	if ((hsamp != 0) && (hsamp < 0xF))
		valid = true;

	return valid;
}

static u8 rtk_mipi_read_hsamp_r2(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags, struct rtk_mipicsi_phy *k_phy)
{
	u32 offset;
	u8 k_flags;
	bool valid;

	k_flags = lane_flags;

	offset = get_phy_offset(top_index);

	if (k_flags & TOP_LANE_3) {
		regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG16 + offset, &k_phy->hsamp3);
		valid = is_hsamp_valid(k_phy->hsamp3);
		if (valid)
			k_flags &= (~TOP_LANE_3);
	}

	if (k_flags & TOP_LANE_2) {
		regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG17 + offset, &k_phy->hsamp2);
		valid = is_hsamp_valid(k_phy->hsamp2);
		if (valid)
			k_flags &= (~TOP_LANE_2);
	}

	if (k_flags & TOP_LANE_1) {
		regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG18 + offset, &k_phy->hsamp1);
		valid = is_hsamp_valid(k_phy->hsamp1);
		if (valid)
			k_flags &= (~TOP_LANE_1);
	}

	if (k_flags & TOP_LANE_0) {
		regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG19 + offset, &k_phy->hsamp0);
		valid = is_hsamp_valid(k_phy->hsamp0);
		if (valid)
			k_flags &= (~TOP_LANE_0);
	}

	if (k_flags & TOP_LANE_CK) {
		regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG20 + offset, &k_phy->hsampck);
		valid = is_hsamp_valid(k_phy->hsampck);
		if (valid)
			k_flags &= (~TOP_LANE_CK);
	}

	return k_flags;
}

static u32 rtk_mipi_reverse_hsamp(u8 range, u32 hsamp)
{
	u32 new_hsamp = 0;

	switch (range) {
	case OFFSET_RANGE0:
		if (hsamp <= 8)
			new_hsamp = hsamp + 6;
		else
			new_hsamp = hsamp - 6;
		break;
	case OFFSET_RANGE1:
		if (hsamp <= 8)
			new_hsamp = hsamp + 3;
		else
			new_hsamp = hsamp - 3;
		break;
	case OFFSET_RANGE2:
		if (hsamp <= 8)
			new_hsamp = hsamp + 2;
		else
			new_hsamp = hsamp - 2;
		break;
	case MAX_OFFSET_RANGE:
		if (hsamp <= 8)
			new_hsamp = hsamp + 2;
		else
			new_hsamp = hsamp - 2;
		break;
	default:
		new_hsamp = hsamp;
		break;
	}

	return new_hsamp;
}

static void __maybe_unused rtk_mipi_reverse_hsamp_cfg(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags, struct rtk_mipicsi_phy *phy)
{
	u32 offset;
	u32 reg_val = 0;
	u8 ckk, lane3, lane2, lane1, lane0;

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG13 + offset, &reg_val);
	ckk = MIPI0_APHY_REG13_get_offset_range_ckk(reg_val);
	lane3 = MIPI0_APHY_REG13_get_offset_range_lane3(reg_val);
	lane2 = MIPI0_APHY_REG13_get_offset_range_lane2(reg_val);
	lane1 = MIPI0_APHY_REG13_get_offset_range_lane1(reg_val);
	lane0 = MIPI0_APHY_REG13_get_offset_range_lane0(reg_val);

	/* No need to adjust the clock lane */

	if (lane_flags & TOP_LANE_0)
		phy->hsamp0 = rtk_mipi_reverse_hsamp(lane0, phy->hsamp0);

	if (lane_flags & TOP_LANE_1)
		phy->hsamp1 = rtk_mipi_reverse_hsamp(lane1, phy->hsamp1);

	if (lane_flags & TOP_LANE_2)
		phy->hsamp2 = rtk_mipi_reverse_hsamp(lane2, phy->hsamp2);

	if (lane_flags & TOP_LANE_3)
		phy->hsamp3 = rtk_mipi_reverse_hsamp(lane3, phy->hsamp3);
}

static void rtk_mipi_hsamp_cfg(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags, const struct rtk_mipicsi_phy *phy)
{
	u32 offset;

	offset = get_phy_offset(top_index);

	if (lane_flags & TOP_LANE_0)
		regmap_write(mipicsi->phyreg, MIPI0_APHY_REG3 + offset,
				MIPI0_APHY_REG3_hsamp0_rl_rg(phy->hsamp0));

	if (lane_flags & TOP_LANE_1)
		regmap_write(mipicsi->phyreg, MIPI0_APHY_REG4 + offset,
				MIPI0_APHY_REG4_hsamp1_rl_rg(phy->hsamp1));

	if (lane_flags & TOP_LANE_2)
		regmap_write(mipicsi->phyreg, MIPI0_APHY_REG5 + offset,
				MIPI0_APHY_REG5_hsamp2_rl_rg(phy->hsamp2));

	if (lane_flags & TOP_LANE_3)
		regmap_write(mipicsi->phyreg, MIPI0_APHY_REG6 + offset,
				MIPI0_APHY_REG6_hsamp3_rl_rg(phy->hsamp3));

	if (lane_flags & TOP_LANE_CK)
		regmap_write(mipicsi->phyreg, MIPI0_APHY_REG11 + offset,
			MIPI0_APHY_REG11_hsampck_rl_rg(phy->hsampck));
}

static void rtk_mipi_dphy_cfg(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags)
{
	u32 offset;
	u32 vc_ch_fmt;

	offset = get_phy_offset(top_index);

	regmap_write(mipicsi->phyreg, MIPI0_DPHY_CFG0 + offset,
		MIPI0_DPHY_CFG0_clk_lane_cfg(0) |
		MIPI0_DPHY_CFG0_pix_din_mipi_sel(1) |
		MIPI0_DPHY_CFG0_bit_swp(0) |
		MIPI0_DPHY_CFG0_crc16_en(1) |
		MIPI0_DPHY_CFG0_ecc_en(1) |
		MIPI0_DPHY_CFG0_clk_mode(0));

	regmap_write(mipicsi->phyreg, MIPI0_DPHY_LANE_EN + offset,
		lane_flags & 0x1F);

	regmap_write(mipicsi->phyreg, MIPI0_DPHY_LANE_SEL + offset,
		MIPI0_DPHY_LANE_SEL_lane3_sel(3) |
		MIPI0_DPHY_LANE_SEL_lane2_sel(2) |
		MIPI0_DPHY_LANE_SEL_lane1_sel(1) |
		MIPI0_DPHY_LANE_SEL_lane0_sel(0));

	vc_ch_fmt = MIPI0_DPHY_VC0_DATA_FORMAT_yuv_src_sel0(PHY_YUV_SRC_UYVY) |
		MIPI0_DPHY_VC0_DATA_FORMAT_dec_format0(PHY_DEC_FMT_Y422) |
		MIPI0_DPHY_VC0_DATA_FORMAT_dec_id0(PHY_DEC_TYPE_Y422);

	regmap_write(mipicsi->phyreg, MIPI0_DPHY_VC0_DATA_FORMAT + offset,
		MIPI0_DPHY_VC0_DATA_FORMAT_vc(0) | vc_ch_fmt);
	regmap_write(mipicsi->phyreg, MIPI0_DPHY_VC1_DATA_FORMAT + offset,
		MIPI0_DPHY_VC0_DATA_FORMAT_vc(1) | vc_ch_fmt);

	if (top_index == MIPI_TOP_0) {
		regmap_write(mipicsi->phyreg, MIPI0_DPHY_VC2_DATA_FORMAT + offset,
			MIPI0_DPHY_VC0_DATA_FORMAT_vc(2) | vc_ch_fmt);
		regmap_write(mipicsi->phyreg, MIPI0_DPHY_VC3_DATA_FORMAT + offset,
			MIPI0_DPHY_VC0_DATA_FORMAT_vc(3) | vc_ch_fmt);
	}

	regmap_write(mipicsi->phyreg, MIPI0_DPHY_HSCLK_SEL + offset, 0);

	/*
	 * Set DPHY hl_interval for MIPI D-PHY spec Table24.
	 * Ignore any Data Lane HS transitions during THS-SETTLE time interval.
	 * The time of one tap of hi_interval = 16*UI
	 * All tap = 0xF*16*UI (Maximum value of hi_interval is 0xF)
	 * The time of one UI = 1/1.5G = 0.66667ns
	 */
	regmap_write(mipicsi->phyreg, MIPI0_DPHY_HSTERM0 + offset,
		MIPI0_DPHY_HSTERM0_hl_interval_1(0xE) |
		MIPI0_DPHY_HSTERM0_hl_interval_0(0xE));
	regmap_write(mipicsi->phyreg, MIPI0_DPHY_HSTERM1 + offset,
		MIPI0_DPHY_HSTERM1_hl_interval_3(0xE) |
		MIPI0_DPHY_HSTERM1_hl_interval_2(0xE));

	regmap_write(mipicsi->phyreg, MIPI0_DPHY_DIVSEL + offset,
		MIPI0_DPHY_DIVSEL_drst_sel(0) |
		MIPI0_DPHY_DIVSEL_div_sel(1));

	regmap_write(mipicsi->phyreg, MIPI0_DPHY_RXPOS_LANE + offset,
		MIPI0_DPHY_RXPOS_LANE_rxpos_lane(0xff));
}

static bool rtk_mipi_increase_offset_range(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags)
{
	u32 offset;
	u32 reg_val = 0;
	u8 ckk, lane3, lane2, lane1, lane0;
	bool success = false;

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG13 + offset, &reg_val);
	ckk = MIPI0_APHY_REG13_get_offset_range_ckk(reg_val);
	lane3 = MIPI0_APHY_REG13_get_offset_range_lane3(reg_val);
	lane2 = MIPI0_APHY_REG13_get_offset_range_lane2(reg_val);
	lane1 = MIPI0_APHY_REG13_get_offset_range_lane1(reg_val);
	lane0 = MIPI0_APHY_REG13_get_offset_range_lane0(reg_val);

	if ((ckk >= MAX_OFFSET_RANGE) &&
		(lane3 >= MAX_OFFSET_RANGE) && (lane2 >= MAX_OFFSET_RANGE) &&
		(lane1 >= MAX_OFFSET_RANGE) && (lane0 >= MAX_OFFSET_RANGE))
		goto exit;

	if ((ckk < MAX_OFFSET_RANGE) && (lane_flags & TOP_LANE_CK))
		ckk++;

	if (lane3 < MAX_OFFSET_RANGE && (lane_flags & TOP_LANE_3))
		lane3++;

	if (lane2 < MAX_OFFSET_RANGE && (lane_flags & TOP_LANE_2))
		lane2++;

	if (lane1 < MAX_OFFSET_RANGE && (lane_flags & TOP_LANE_1))
		lane1++;

	if (lane0 < MAX_OFFSET_RANGE && (lane_flags & TOP_LANE_0))
		lane0++;

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG13 + offset,
			MIPI0_APHY_REG13_offset_range_ckk_mask |
			MIPI0_APHY_REG13_offset_range_lane3_mask |
			MIPI0_APHY_REG13_offset_range_lane2_mask |
			MIPI0_APHY_REG13_offset_range_lane1_mask |
			MIPI0_APHY_REG13_offset_range_lane0_mask,
			MIPI0_APHY_REG13_offset_range_ckk(ckk) |
			MIPI0_APHY_REG13_offset_range_lane3(lane3) |
			MIPI0_APHY_REG13_offset_range_lane2(lane2) |
			MIPI0_APHY_REG13_offset_range_lane1(lane1) |
			MIPI0_APHY_REG13_offset_range_lane0(lane0));

	success = true;
exit:
	return success;
}

static void rtk_mipi_toggle_offset_cal(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags)
{
	u32 reg_val = 0;
	u32 offset;
	u32 cal_mask = 0;

	if ((lane_flags & TOP_LANE_NUM_4) == 0)
		return;

	offset = get_phy_offset(top_index);
	regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG43 + offset, &reg_val);

	if (lane_flags & TOP_LANE_0)
		cal_mask |= MIPI0_APHY_REG43_offset_cal_lane0_mask;

	if (lane_flags & TOP_LANE_1)
		cal_mask |= MIPI0_APHY_REG43_offset_cal_lane1_mask;

	if (lane_flags & TOP_LANE_2)
		cal_mask |= MIPI0_APHY_REG43_offset_cal_lane2_mask;

	if (lane_flags & TOP_LANE_3)
		cal_mask |= MIPI0_APHY_REG43_offset_cal_lane3_mask;

	if (lane_flags & TOP_LANE_CK)
		cal_mask |= MIPI0_APHY_REG43_offset_cal_ck_mask;

	regmap_write(mipicsi->phyreg, MIPI0_APHY_REG43 + offset,
			reg_val | cal_mask);

	regmap_write(mipicsi->phyreg, MIPI0_APHY_REG43 + offset,
			reg_val & (~cal_mask));

	regmap_write(mipicsi->phyreg, MIPI0_APHY_REG43 + offset,
			reg_val | cal_mask);

}

/*
 * Internal P/N offset calibration for each lane.
 */
static bool rtk_mipi_offset_calibration(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags)
{
	u32 offset;
	u32 reg_val = 0;
	struct rtk_mipicsi_phy k_phy;
	u8 range;
	u8 offset_ok;
	u8 k_flags;
	bool k_done = false;

	memset(&k_phy, 0x0, sizeof(struct rtk_mipicsi_phy));

	offset = get_phy_offset(top_index);

	regmap_write(mipicsi->phyreg, MIPI0_APHY_REG28 + offset,
			MIPI0_APHY_REG28_force_hs(1));

	udelay(10);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG13 + offset,
		MIPI0_APHY_REG13_offset_autok_mask,
		MIPI0_APHY_REG13_offset_autok(1));

	k_flags = lane_flags;
	for (range = 0; range <= MAX_OFFSET_RANGE; range++) {

		rtk_mipi_toggle_offset_cal(mipicsi, top_index, k_flags);

		udelay(10);

		regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG12 + offset, &reg_val);
		offset_ok = MIPI0_APHY_REG12_get_offset_ok(reg_val) & lane_flags;

		/* Try again */
		if (offset_ok != lane_flags) {
			udelay(10);
			regmap_read(mipicsi->phyreg,  MIPI0_APHY_REG12 + offset, &reg_val);
			offset_ok = MIPI0_APHY_REG12_get_offset_ok(reg_val);
		}

		if (offset_ok == lane_flags)
			k_flags = rtk_mipi_read_hsamp_r2(mipicsi, top_index, k_flags, &k_phy);

		if (k_flags == 0) {
			k_done = true;
			break;
		}

		rtk_mipi_increase_offset_range(mipicsi, top_index, k_flags);
	}

	/* Apply calibration result  */
	if (k_done) {
		dev_info(mipicsi->dev, "k hsamp0=%u hsamp1=%u hsamp2=%u hsamp3=%u hsampck=%u\n",
			k_phy.hsamp0, k_phy.hsamp1, k_phy.hsamp2, k_phy.hsamp3, k_phy.hsampck);

		rtk_mipi_hsamp_cfg(mipicsi, top_index, lane_flags,
			(const struct rtk_mipicsi_phy *)&k_phy);
	}

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG13 + offset,
			MIPI0_APHY_REG13_offset_autok_mask,
			MIPI0_APHY_REG13_offset_autok(0));

	regmap_write(mipicsi->phyreg, MIPI0_APHY_REG28 + offset,
			MIPI0_APHY_REG28_force_hs(0));

	return k_done;
}

static void rtk_mipi_set_dphy_pwd(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags, u8 enable)
{
	u32 offset;
	u32 reg_val;

	offset = get_phy_offset(top_index);

	if (enable) {
		/* Disable autok before  PWDB=1 */
		rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG13 + offset,
			MIPI0_APHY_REG13_offset_range_ckk_mask |
			MIPI0_APHY_REG13_offset_range_lane3_mask |
			MIPI0_APHY_REG13_offset_range_lane2_mask |
			MIPI0_APHY_REG13_offset_range_lane1_mask |
			MIPI0_APHY_REG13_offset_range_lane0_mask |
			MIPI0_APHY_REG13_offset_autok_mask,
			MIPI0_APHY_REG13_offset_range_ckk(0) |
			MIPI0_APHY_REG13_offset_range_lane3(0) |
			MIPI0_APHY_REG13_offset_range_lane2(0) |
			MIPI0_APHY_REG13_offset_range_lane1(0) |
			MIPI0_APHY_REG13_offset_range_lane0(0) |
			MIPI0_APHY_REG13_offset_autok(0));

		reg_val = lane_flags & 0xF;
	} else {
		reg_val = 0;
	}

	regmap_write(mipicsi->phyreg, MIPI0_DPHY_PWDB + offset, reg_val);
}

static void rtk_mipi_set_skew(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_flags, u8 enable)
{
	u32 offset;

	offset = get_phy_offset(top_index);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG1 + offset,
		MIPI0_APHY_REG1_en_skw_mask,
		MIPI0_APHY_REG1_en_skw(enable));

	if (enable)
		mipicsi->hw_ops->aphy_set_manual_skw(mipicsi, top_index, DISABLE);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG32 + offset,
		MIPI0_APHY_REG32_reg_cfg_code_adj_cyc_lane3_mask |
		MIPI0_APHY_REG32_reg_cfg_code_adj_cyc_lane2_mask |
		MIPI0_APHY_REG32_reg_cfg_code_adj_cyc_lane1_mask |
		MIPI0_APHY_REG32_reg_cfg_code_adj_cyc_lane0_mask,
		MIPI0_APHY_REG32_reg_cfg_code_adj_cyc_lane3(5) |
		MIPI0_APHY_REG32_reg_cfg_code_adj_cyc_lane2(5) |
		MIPI0_APHY_REG32_reg_cfg_code_adj_cyc_lane1(5) |
		MIPI0_APHY_REG32_reg_cfg_code_adj_cyc_lane0(5));

	rtk_mask_write(mipicsi->phyreg, MIPI0_DESKEW_EN + offset,
		MIPI0_DESKEW_EN_deskew_lane3_delay_cycle_mask |
		MIPI0_DESKEW_EN_deskew_lane2_delay_cycle_mask |
		MIPI0_DESKEW_EN_deskew_lane1_delay_cycle_mask |
		MIPI0_DESKEW_EN_deskew_lane0_delay_cycle_mask |
		MIPI0_DESKEW_EN_deskew_hw_sel_lane3_mask |
		MIPI0_DESKEW_EN_deskew_hw_sel_lane2_mask |
		MIPI0_DESKEW_EN_deskew_hw_sel_lane1_mask |
		MIPI0_DESKEW_EN_deskew_hw_sel_lane0_mask |
		MIPI0_DESKEW_EN_deskew_en_sel_lane3_mask |
		MIPI0_DESKEW_EN_deskew_en_sel_lane2_mask |
		MIPI0_DESKEW_EN_deskew_en_sel_lane1_mask |
		MIPI0_DESKEW_EN_deskew_en_sel_lane0_mask |
		MIPI0_DESKEW_EN_deskew_en_fw_lane3_mask |
		MIPI0_DESKEW_EN_deskew_en_fw_lane2_mask |
		MIPI0_DESKEW_EN_deskew_en_fw_lane1_mask |
		MIPI0_DESKEW_EN_deskew_en_fw_lane0_mask,
		MIPI0_DESKEW_EN_deskew_lane3_delay_cycle(8) |
		MIPI0_DESKEW_EN_deskew_lane2_delay_cycle(8) |
		MIPI0_DESKEW_EN_deskew_lane1_delay_cycle(8) |
		MIPI0_DESKEW_EN_deskew_lane0_delay_cycle(8) |
		MIPI0_DESKEW_EN_deskew_hw_sel_lane3(enable) |
		MIPI0_DESKEW_EN_deskew_hw_sel_lane2(enable) |
		MIPI0_DESKEW_EN_deskew_hw_sel_lane1(enable) |
		MIPI0_DESKEW_EN_deskew_hw_sel_lane0(enable) |
		MIPI0_DESKEW_EN_deskew_en_sel_lane3(enable) |
		MIPI0_DESKEW_EN_deskew_en_sel_lane2(enable) |
		MIPI0_DESKEW_EN_deskew_en_sel_lane1(enable) |
		MIPI0_DESKEW_EN_deskew_en_sel_lane0(enable) |
		MIPI0_DESKEW_EN_deskew_en_fw_lane3(enable) |
		MIPI0_DESKEW_EN_deskew_en_fw_lane2(enable) |
		MIPI0_DESKEW_EN_deskew_en_fw_lane1(enable) |
		MIPI0_DESKEW_EN_deskew_en_fw_lane0(enable));
}

static void rtk_mipi_aphy_set_manual_skw(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 enable)
{
	u32 offset;
	u32 reg_val = 0;

	offset = get_phy_offset(top_index);

	/* Read auto k result and apply to manual */
	if (enable) {
		u8 ds_clk[4];
		u8 ds_data[4];
		u8 i;

		rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG1 + offset,
			MIPI0_APHY_REG1_en_skw_mask,
			MIPI0_APHY_REG1_en_skw(1));

		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG36 + offset, &reg_val);
		ds_clk[0] = MIPI0_APHY_REG36_get_ds_cal_clk_dly_lane0(reg_val);

		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG37 + offset, &reg_val);
		ds_data[0] = MIPI0_APHY_REG37_get_ds_cal_data_dly_lane0(reg_val);

		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG38 + offset, &reg_val);
		ds_clk[1] = MIPI0_APHY_REG38_get_ds_cal_clk_dly_lane1(reg_val);
		ds_data[1] = MIPI0_APHY_REG38_get_ds_cal_data_dly_lane1(reg_val);

		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG39 + offset, &reg_val);
		ds_clk[2] = MIPI0_APHY_REG39_get_ds_cal_clk_dly_lane2(reg_val);
		ds_data[2] = MIPI0_APHY_REG39_get_ds_cal_data_dly_lane2(reg_val);

		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG40 + offset, &reg_val);
		ds_clk[3] = MIPI0_APHY_REG40_get_ds_cal_clk_dly_lane3(reg_val);
		ds_data[3] = MIPI0_APHY_REG40_get_ds_cal_data_dly_lane3(reg_val);

		for (i = LANE_0; i <= LANE_3; i++) {
			mipicsi->hw_ops->aphy_set_skw_sclk(mipicsi, top_index, i, ds_clk[i]);
			mipicsi->hw_ops->aphy_set_skw_sdata(mipicsi, top_index, i, ds_data[i]);
		}
	}

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG29 + offset,
		MIPI0_APHY_REG29_reg_manual3_mode_mask |
		MIPI0_APHY_REG29_reg_manual2_mode_mask |
		MIPI0_APHY_REG29_reg_manual1_mode_mask |
		MIPI0_APHY_REG29_reg_manual0_mode_mask,
		MIPI0_APHY_REG29_reg_manual3_mode(enable) |
		MIPI0_APHY_REG29_reg_manual2_mode(enable) |
		MIPI0_APHY_REG29_reg_manual1_mode(enable) |
		MIPI0_APHY_REG29_reg_manual0_mode(enable));
}

static u8 rtk_mipi_aphy_get_manual_skw(struct rtk_mipicsi *mipicsi,
		u8 top_index)
{
	u32 offset;
	u32 reg_val = 0;
	u8 enable;

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg, MIPI0_APHY_REG29 + offset, &reg_val);
	enable = (MIPI0_APHY_REG29_get_reg_manual3_mode(reg_val) << 3) |
			(MIPI0_APHY_REG29_get_reg_manual2_mode(reg_val) << 2) |
			(MIPI0_APHY_REG29_get_reg_manual1_mode(reg_val) << 1) |
			MIPI0_APHY_REG29_get_reg_manual0_mode(reg_val);

	return enable;
}

static void rtk_mipi_aphy_set_skw_sclk(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_index, u8 sclk)
{
	u32 offset;
	u32 reg_addr;
	u32 mask;
	u32 reg_val;

	offset = get_phy_offset(top_index);

	if (lane_index > LANE_3)
		return;

	switch (lane_index) {
	case LANE_0:
		reg_addr = MIPI0_APHY_REG9;
		mask = MIPI0_APHY_REG9_sclk0_skw_rg_mask;
		reg_val = MIPI0_APHY_REG9_sclk0_skw_rg(sclk);
		break;
	case LANE_1:
		reg_addr = MIPI0_APHY_REG9;
		mask = MIPI0_APHY_REG9_sclk1_skw_rg_mask;
		reg_val = MIPI0_APHY_REG9_sclk1_skw_rg(sclk);
		break;
	case LANE_2:
		reg_addr = MIPI0_APHY_REG10;
		mask = MIPI0_APHY_REG10_sclk2_skw_rg_mask;
		reg_val = MIPI0_APHY_REG10_sclk2_skw_rg(sclk);
		break;
	case LANE_3:
		reg_addr = MIPI0_APHY_REG10;
		mask = MIPI0_APHY_REG10_sclk3_skw_rg_mask;
		reg_val = MIPI0_APHY_REG10_sclk3_skw_rg(sclk);
		break;
	}

	rtk_mask_write(mipicsi->phyreg, reg_addr + offset, mask, reg_val);
}

static u8 rtk_mipi_aphy_get_skw_sclk(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_index)
{
	u32 offset;
	u32 reg_val = 0;
	u8 sclk = 0;

	offset = get_phy_offset(top_index);

	if (lane_index > LANE_3)
		goto exit;

	switch (lane_index) {
	case LANE_0:
		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG9 + offset, &reg_val);
		sclk = MIPI0_APHY_REG9_get_sclk0_skw_rg(reg_val);
		break;
	case LANE_1:
		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG9 + offset, &reg_val);
		sclk = MIPI0_APHY_REG9_get_sclk1_skw_rg(reg_val);
		break;
	case LANE_2:
		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG10 + offset, &reg_val);
		sclk = MIPI0_APHY_REG10_get_sclk2_skw_rg(reg_val);
		break;
	case LANE_3:
		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG10 + offset, &reg_val);
		sclk = MIPI0_APHY_REG10_get_sclk3_skw_rg(reg_val);
		break;
	}

exit:
	return sclk;
}

static void rtk_mipi_aphy_set_skw_sdata(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_index, u8 sdata)
{
	u32 offset;
	u32 reg_addr;
	u32 mask;
	u32 reg_val;

	offset = get_phy_offset(top_index);

	if (lane_index > LANE_3)
		return;

	switch (lane_index) {
	case LANE_0:
		reg_addr = MIPI0_APHY_REG7;
		mask = MIPI0_APHY_REG7_sdata0_skw_rg_mask;
		reg_val = MIPI0_APHY_REG7_sdata0_skw_rg(sdata);
		break;
	case LANE_1:
		reg_addr = MIPI0_APHY_REG7;
		mask = MIPI0_APHY_REG7_sdata1_skw_rg_mask;
		reg_val = MIPI0_APHY_REG7_sdata1_skw_rg(sdata);
		break;
	case LANE_2:
		reg_addr = MIPI0_APHY_REG8;
		mask = MIPI0_APHY_REG8_sdata2_skw_rg_mask;
		reg_val = MIPI0_APHY_REG8_sdata2_skw_rg(sdata);
		break;
	case LANE_3:
		reg_addr = MIPI0_APHY_REG8;
		mask = MIPI0_APHY_REG8_sdata3_skw_rg_mask;
		reg_val = MIPI0_APHY_REG8_sdata3_skw_rg(sdata);
		break;
	}

	rtk_mask_write(mipicsi->phyreg, reg_addr + offset, mask, reg_val);
}

static u8 rtk_mipi_aphy_get_skw_sdata(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_index)
{
	u32 offset;
	u32 reg_val = 0;
	u8 sdata = 0;

	offset = get_phy_offset(top_index);

	if (lane_index > LANE_3)
		goto exit;

	switch (lane_index) {
	case LANE_0:
		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG7 + offset, &reg_val);
		sdata = MIPI0_APHY_REG7_get_sdata0_skw_rg(reg_val);
		break;
	case LANE_1:
		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG7 + offset, &reg_val);
		sdata = MIPI0_APHY_REG7_get_sdata1_skw_rg(reg_val);
		break;
	case LANE_2:
		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG8 + offset, &reg_val);
		sdata = MIPI0_APHY_REG8_get_sdata2_skw_rg(reg_val);
		break;
	case LANE_3:
		regmap_read(mipicsi->phyreg, MIPI0_APHY_REG8 + offset, &reg_val);
		sdata = MIPI0_APHY_REG8_get_sdata3_skw_rg(reg_val);
		break;
	}

exit:
	return sdata;
}

static void rtk_mipi_aphy_set_adj(struct rtk_mipicsi *mipicsi,
		u8 top_index, u32 ileq_adj, u32 ivga_adj)
{
	u32 offset;
	u32 rx_ileq_adj;
	u32 rx_ivga_adj;

	dev_dbg(mipicsi->dev, "TOP%u ileq_adj=%u ivga_adj=%u\n",
		top_index, ileq_adj, ivga_adj);

	offset = get_phy_offset(top_index);

	rx_ileq_adj = (ileq_adj << 6) | (ileq_adj << 4) | (ileq_adj << 2) | ileq_adj;

	rx_ivga_adj = (ivga_adj << 6) | (ivga_adj << 4) | (ivga_adj << 2) | ivga_adj;

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG47 + offset,
			MIPI0_APHY_REG47_rx_ileq_adj_ckk_mask |
			MIPI0_APHY_REG47_rx_ileq_adj_mask,
			MIPI0_APHY_REG47_rx_ileq_adj_ckk(ileq_adj) |
			MIPI0_APHY_REG47_rx_ileq_adj(rx_ileq_adj));

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG49 + offset,
			MIPI0_APHY_REG49_rx_ivga_adj_ckk_mask |
			MIPI0_APHY_REG49_rx_ivga_adj_mask,
			MIPI0_APHY_REG49_rx_ivga_adj_ckk(ivga_adj) |
			MIPI0_APHY_REG49_rx_ivga_adj(rx_ivga_adj));
}

static void rtk_mipi_aphy_set_ctune(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 ctune)
{
	u32 offset;
	u8 ctune_gray;

	dev_dbg(mipicsi->dev, "TOP%u ctune=%u\n", top_index, ctune);

	offset = get_phy_offset(top_index);

	ctune_gray = bcd_to_gray(ctune);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG44 + offset,
		MIPI0_APHY_REG44_rx_acgain_lane3_mask |
		MIPI0_APHY_REG44_rx_acgain_lane2_mask |
		MIPI0_APHY_REG44_rx_acgain_lane1_mask |
		MIPI0_APHY_REG44_rx_acgain_lane0_mask |
		MIPI0_APHY_REG44_rx_acgain_ckk_mask,
		MIPI0_APHY_REG44_rx_acgain_lane3(ctune_gray) |
		MIPI0_APHY_REG44_rx_acgain_lane2(ctune_gray) |
		MIPI0_APHY_REG44_rx_acgain_lane1(ctune_gray) |
		MIPI0_APHY_REG44_rx_acgain_lane0(ctune_gray) |
		MIPI0_APHY_REG44_rx_acgain_ckk(ctune_gray));
}

static u8 rtk_mipi_aphy_get_ctune(struct rtk_mipicsi *mipicsi,
		u8 top_index)
{
	u32 offset;
	u32 reg_val = 0;
	u8 ctune_gray;

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg, MIPI0_APHY_REG44 + offset, &reg_val);
	ctune_gray = MIPI0_APHY_REG44_get_rx_acgain_ckk(reg_val);

	return (u8)gray_to_bcd(ctune_gray);
}

static void rtk_mipi_aphy_set_rtune(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 rtune)
{
	u32 offset;
	u8 rtune_gray;
	u8 rtune_msb;

	dev_dbg(mipicsi->dev, "TOP%u rtune=%u\n", top_index, rtune);

	offset = get_phy_offset(top_index);

	rtune_gray = bcd_to_gray(rtune);
	rtune_msb = (rtune_gray & 0x8) >> 3;

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG46 + offset,
		MIPI0_APHY_REG46_rx_eq_ac_range_ckk_mask |
		MIPI0_APHY_REG46_rx_eq_ac_range_lane3_mask |
		MIPI0_APHY_REG46_rx_eq_ac_range_lane2_mask |
		MIPI0_APHY_REG46_rx_eq_ac_range_lane1_mask |
		MIPI0_APHY_REG46_rx_eq_ac_range_lane0_mask,
		MIPI0_APHY_REG46_rx_eq_ac_range_ckk(rtune_msb) |
		MIPI0_APHY_REG46_rx_eq_ac_range_lane3(rtune_msb) |
		MIPI0_APHY_REG46_rx_eq_ac_range_lane2(rtune_msb) |
		MIPI0_APHY_REG46_rx_eq_ac_range_lane1(rtune_msb) |
		MIPI0_APHY_REG46_rx_eq_ac_range_lane0(rtune_msb));

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG50 + offset,
		MIPI0_APHY_REG50_rx_leq_c_tune_ckk_mask |
		MIPI0_APHY_REG50_rx_leq_c_tune_lane3_mask |
		MIPI0_APHY_REG50_rx_leq_c_tune_lane2_mask |
		MIPI0_APHY_REG50_rx_leq_c_tune_lane1_mask |
		MIPI0_APHY_REG50_rx_leq_c_tune_lane0_mask,
		MIPI0_APHY_REG50_rx_leq_c_tune_ckk(rtune_gray) |
		MIPI0_APHY_REG50_rx_leq_c_tune_lane3(rtune_gray) |
		MIPI0_APHY_REG50_rx_leq_c_tune_lane2(rtune_gray) |
		MIPI0_APHY_REG50_rx_leq_c_tune_lane1(rtune_gray) |
		MIPI0_APHY_REG50_rx_leq_c_tune_lane0(rtune_gray));
}

static u8 rtk_mipi_aphy_get_rtune(struct rtk_mipicsi *mipicsi,
		u8 top_index)
{
	u32 offset;
	u32 reg_val = 0;
	u8 rtune_gray;
	u8 rtune_msb;

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg, MIPI0_APHY_REG46 + offset, &reg_val);
	rtune_msb = MIPI0_APHY_REG46_get_rx_eq_ac_range_ckk(reg_val);

	regmap_read(mipicsi->phyreg, MIPI0_APHY_REG50 + offset, &reg_val);
	rtune_gray = MIPI0_APHY_REG50_get_rx_leq_c_tune_ckk(reg_val);

	rtune_gray = (rtune_msb << 3) | rtune_gray;

	return (u8)gray_to_bcd(rtune_gray);
}

static void rtk_mipi_aphy_set_d2s(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 d2s_isel)
{
	u32 offset;

	dev_dbg(mipicsi->dev, "TOP%u d2s_isel=%u\n", top_index, d2s_isel);

	offset = get_phy_offset(top_index);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG45 + offset,
		MIPI0_APHY_REG45_rx_d2s_isel_mask,
		MIPI0_APHY_REG45_rx_d2s_isel(d2s_isel));
}

static u8 rtk_mipi_aphy_get_d2s(struct rtk_mipicsi *mipicsi,
		u8 top_index)
{
	u32 offset;
	u32 reg_val = 0;
	u8 d2s_isel;

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg, MIPI0_APHY_REG45 + offset, &reg_val);
	d2s_isel = MIPI0_APHY_REG45_get_rx_d2s_isel(reg_val);

	return d2s_isel;
}

static void rtk_mipi_aphy_set_ibn_dc(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 ibn_dc)
{
	u32 offset;

	dev_dbg(mipicsi->dev, "TOP%u ibn_dc=%u\n", top_index, ibn_dc);

	offset = get_phy_offset(top_index);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG51 + offset,
		MIPI0_APHY_REG51_rx_leq_ibn_const_sel_dc_mask,
		MIPI0_APHY_REG51_rx_leq_ibn_const_sel_dc(ibn_dc));
}

static u8 rtk_mipi_aphy_get_ibn_dc(struct rtk_mipicsi *mipicsi,
		u8 top_index)
{
	u32 offset;
	u32 reg_val = 0;
	u8 ibn_dc;

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg, MIPI0_APHY_REG51 + offset, &reg_val);
	ibn_dc = MIPI0_APHY_REG51_get_rx_leq_ibn_const_sel_dc(reg_val);

	return ibn_dc;
}

static void rtk_mipi_aphy_set_ibn_gm(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 ibn_gm)
{
	u32 offset;

	dev_dbg(mipicsi->dev, "TOP%u ibn_gm=%u\n", top_index, ibn_gm);

	offset = get_phy_offset(top_index);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG51 + offset,
		MIPI0_APHY_REG51_rx_leq_ibn_const_gm_sel_mask,
		MIPI0_APHY_REG51_rx_leq_ibn_const_gm_sel(ibn_gm));
}

static u8 rtk_mipi_aphy_get_ibn_gm(struct rtk_mipicsi *mipicsi,
		u8 top_index)
{
	u32 offset;
	u32 reg_val = 0;
	u8 ibn_gm;

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg, MIPI0_APHY_REG51 + offset, &reg_val);
	ibn_gm = MIPI0_APHY_REG51_get_rx_leq_ibn_const_gm_sel(reg_val);

	return ibn_gm;
}

static void rtk_mipi_aphy_set_eq(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 ctune, u8 rtune)
{
	rtk_mipi_aphy_set_ctune(mipicsi, top_index, ctune);
	rtk_mipi_aphy_set_rtune(mipicsi, top_index, rtune);
	// TODO: REG_RX_D2S_ISEL
}

static bool rtk_mipi_eq_tuning(struct rtk_mipicsi *mipicsi,
		u8 top_index, u32 check_ms, u8 max_sample)
{
	u32 offset;
	u8 rtune;
	u8 ok_rtune[MAX_RTUNE] = {0};
	u8 ok_count = 0;
	bool phy_ok;

	offset = get_phy_offset(top_index);

	for (rtune = 0; rtune < MAX_RTUNE; rtune++) {
		rtk_mipi_aphy_set_eq(mipicsi, top_index, dft_phy_cfg.ctune, rtune);
		usleep_range(1, 2);
		phy_ok = mipicsi->hw_ops->phy_check(mipicsi, top_index, check_ms);

		if (phy_ok) {
			ok_rtune[ok_count] = rtune;
			ok_count++;
		}

		if ((ok_count >= max_sample) ||
			(ok_count >= MAX_RTUNE))
			break;
	}

	if (ok_count) {
		rtk_mipi_aphy_set_eq(mipicsi, top_index,
			dft_phy_cfg.ctune, ok_rtune[ok_count/2]);
		dev_info(mipicsi->dev, "rtune ok_count=%u, new rtune=%u\n",
			ok_count, ok_rtune[ok_count/2]);
	}

	return ok_count ? true:false;
}

static void rtk_mipi_set_phy(struct rtk_mipicsi *mipicsi,
		const struct rtk_mipicsi_phy *phy, u8 top_index, u8 lane_flags)
{
	u32 offset;
	bool k_done;

	offset = get_phy_offset(top_index);

	rtk_mipi_dphy_cfg(mipicsi, top_index, lane_flags);

	rtk_mipi_hsamp_cfg(mipicsi, top_index, lane_flags, phy);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG13 + offset,
		MIPI0_APHY_REG13_offset_range_ckk_mask |
		MIPI0_APHY_REG13_offset_range_lane3_mask |
		MIPI0_APHY_REG13_offset_range_lane2_mask |
		MIPI0_APHY_REG13_offset_range_lane1_mask |
		MIPI0_APHY_REG13_offset_range_lane0_mask,
		MIPI0_APHY_REG13_offset_range_ckk(phy->offset_range_ckk) |
		MIPI0_APHY_REG13_offset_range_lane3(phy->offset_range_lane3) |
		MIPI0_APHY_REG13_offset_range_lane3(phy->offset_range_lane2) |
		MIPI0_APHY_REG13_offset_range_lane3(phy->offset_range_lane1) |
		MIPI0_APHY_REG13_offset_range_lane3(phy->offset_range_lane0));

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG14 + offset,
		MIPI0_APHY_REG14_lpamp_ref_mask,
		MIPI0_APHY_REG14_lpamp_ref(phy->lpamp));

	/* Enable level 1 VGA; No SWAP */
	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG43 + offset,
		MIPI0_APHY_REG43_level_1_vga_mask |
		MIPI0_APHY_REG43_level_2_vga_mask |
		MIPI0_APHY_REG43_pn_swap_clk_mask |
		MIPI0_APHY_REG43_pn_swap_lane3_mask |
		MIPI0_APHY_REG43_pn_swap_lane2_mask |
		MIPI0_APHY_REG43_pn_swap_lane1_mask |
		MIPI0_APHY_REG43_pn_swap_lane0_mask,
		MIPI0_APHY_REG43_level_1_vga(0x1F) |
		MIPI0_APHY_REG43_level_2_vga(0) |
		MIPI0_APHY_REG43_pn_swap_clk(0) |
		MIPI0_APHY_REG43_pn_swap_lane3(0) |
		MIPI0_APHY_REG43_pn_swap_lane2(0) |
		MIPI0_APHY_REG43_pn_swap_lane1(0) |
		MIPI0_APHY_REG43_pn_swap_lane0(0));

	rtk_mipi_aphy_set_eq(mipicsi, top_index, phy->ctune, phy->rtune);

	rtk_mipi_aphy_set_adj(mipicsi, top_index, phy->ileq_adj, phy->ivga_adj);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG52 + offset,
		MIPI0_APHY_REG52_z0_mask,
		MIPI0_APHY_REG52_z0(phy->z0));

	rtk_mipi_set_dphy_pwd(mipicsi, top_index, lane_flags, ENABLE);

	udelay(100);

	k_done = rtk_mipi_offset_calibration(mipicsi, top_index, lane_flags);
	if (!k_done)
		dev_err(mipicsi->dev, "TOP%u offset calibration failed\n", top_index);
	else
		dev_info(mipicsi->dev, "TOP%u offset calibration success\n", top_index);

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG2 + offset,
		MIPI0_APHY_REG2_pwdb_osk_mask,
		MIPI0_APHY_REG2_pwdb_osk(0));

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG13 + offset,
		MIPI0_APHY_REG13_offset_enk_mask |
		MIPI0_APHY_REG13_offset_autok_mask,
		MIPI0_APHY_REG13_offset_enk(1) |
		MIPI0_APHY_REG13_offset_autok(0));

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG27 + offset,
		MIPI0_APHY_REG27_rt_pow1_mask,
		MIPI0_APHY_REG27_rt_pow1(1));

	rtk_mask_write(mipicsi->phyreg, MIPI0_APHY_REG27 + offset,
		MIPI0_APHY_REG27_rt_pow1_mask,
		MIPI0_APHY_REG27_rt_pow1(0));
}

static void rtk_mipi_phy_start(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 enable)
{
	u32 offset;

	offset = get_phy_offset(top_index);

	regmap_write(mipicsi->phyreg, MIPI0_START_TRIG + offset,
		MIPI0_START_TRIG_start_mipi(enable));
}

static void set_hs_scaler(struct rtk_mipicsi *mipicsi, u8 ch_index,
		unsigned int hsi_offset, unsigned int hsi_phase,
		unsigned int hsd_out, unsigned int hsd_delta)
{
	u32 offset;

	offset = get_app_offset(ch_index);

	dev_dbg(mipicsi->dev, "hsd_out=%u, hsd_delta=0x%x\n",
		hsd_out, hsd_delta);

	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSI + offset,
		APP_MIPI_SCALER_HSI_hsi_offset(hsi_offset) |
		APP_MIPI_SCALER_HSI_hsi_phase(hsi_phase));

	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSD + offset,
		APP_MIPI_SCALER_HSD_hsd_out(hsd_out) |
		APP_MIPI_SCALER_HSD_hsd_delta(hsd_delta));
}

static int get_ratio8_index(unsigned int delta)
{
	unsigned int i;
	unsigned int idx;

	if (delta < 0x4000) { /* upscaling */
		idx = 0;
		goto exit;
	}

	/*
	 * VscaleRatio8 is incrementing, we find the first one that's bigger
	 * than or equal to delta instead of min ads diff.
	 * This way it will round up to lower pass filter to avoid alias
	 */
	for (i = 1; i < ARRAY_SIZE(scale_ratio8); i++) {
		if (scale_ratio8[i] >= delta) {
			idx = i;
			goto exit;
		}
	}

	/* last index */
	idx = ARRAY_SIZE(scale_ratio8) - 1;

exit:
	return idx;
}

static void get_scaling_coeffs(struct rtk_mipicsi *mipicsi,
	unsigned int *coeff, int delta)
{
	int i;
	int x;
	int y;
	int idx;
	int taps;
	short const *p;

	taps = 4;
	idx = get_ratio8_index(delta);
	p = coef_4t8p_ratio8[idx][0];

	dev_dbg(mipicsi->dev, "%s idx=%d\n", __func__, idx);

	for (i = 0; i < (taps << 2); i++) {
		x = i &  7;
		y = i >> 3;

		coeff[i] =
			(x < 4 ? p[x * taps + taps-1-y] : p[(7-x) * taps + y]) << 4;
	}
}

static void set_hs_coeff(struct rtk_mipicsi *mipicsi,
	u8 ch_index, unsigned int delta)
{
	unsigned int c[16];
	u32 offset;

	offset = get_app_offset(ch_index);

	get_scaling_coeffs(mipicsi, c, delta);

	/* for Y */
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSYC0 + offset,
		 APP_MIPI_SCALER_HSYC0_hsyc0_c1(c[1]) |
		 APP_MIPI_SCALER_HSYC0_hsyc0_c0(c[0]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSYC1 + offset,
		 APP_MIPI_SCALER_HSYC1_hsyc1_c1(c[3]) |
		 APP_MIPI_SCALER_HSYC1_hsyc1_c0(c[2]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSYC2 + offset,
		 APP_MIPI_SCALER_HSYC2_hsyc2_c1(c[5]) |
		 APP_MIPI_SCALER_HSYC2_hsyc2_c0(c[4]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSYC3 + offset,
		 APP_MIPI_SCALER_HSYC3_hsyc3_c1(c[7]) |
		 APP_MIPI_SCALER_HSYC3_hsyc3_c0(c[6]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSYC4 + offset,
		 APP_MIPI_SCALER_HSYC4_hsyc4_c1(c[9]) |
		 APP_MIPI_SCALER_HSYC4_hsyc4_c0(c[8]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSYC5 + offset,
		 APP_MIPI_SCALER_HSYC5_hsyc5_c1(c[11]) |
		 APP_MIPI_SCALER_HSYC5_hsyc5_c0(c[10]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSYC6 + offset,
		 APP_MIPI_SCALER_HSYC6_hsyc6_c1(c[13]) |
		 APP_MIPI_SCALER_HSYC6_hsyc6_c0(c[12]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSYC7 + offset,
		 APP_MIPI_SCALER_HSYC7_hsyc7_c1(c[15]) |
		 APP_MIPI_SCALER_HSYC7_hsyc7_c0(c[14]));

	/* for U,V */
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSCC0 + offset,
		 APP_MIPI_SCALER_HSCC0_hscc0_c1(c[1]) |
		 APP_MIPI_SCALER_HSCC0_hscc0_c0(c[0]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSCC1 + offset,
		 APP_MIPI_SCALER_HSCC1_hscc1_c1(c[3]) |
		 APP_MIPI_SCALER_HSCC1_hscc1_c0(c[2]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSCC2 + offset,
		 APP_MIPI_SCALER_HSCC2_hscc2_c1(c[5]) |
		 APP_MIPI_SCALER_HSCC2_hscc2_c0(c[4]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSCC3 + offset,
		 APP_MIPI_SCALER_HSCC3_hscc3_c1(c[7]) |
		 APP_MIPI_SCALER_HSCC3_hscc3_c0(c[6]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSCC4 + offset,
		 APP_MIPI_SCALER_HSCC4_hscc4_c1(c[9]) |
		 APP_MIPI_SCALER_HSCC4_hscc4_c0(c[8]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSCC5 + offset,
		 APP_MIPI_SCALER_HSCC5_hscc5_c1(c[11]) |
		 APP_MIPI_SCALER_HSCC5_hscc5_c0(c[10]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSCC6 + offset,
		 APP_MIPI_SCALER_HSCC6_hscc6_c1(c[13]) |
		 APP_MIPI_SCALER_HSCC6_hscc6_c0(c[12]));
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_HSCC7 + offset,
		 APP_MIPI_SCALER_HSCC7_hscc7_c1(c[15]) |
		 APP_MIPI_SCALER_HSCC7_hscc7_c0(c[14]));
}

static void set_vs_scaler(struct rtk_mipicsi *mipicsi, u8 ch_index,
		unsigned int vsi_offset, unsigned int vsi_phase,
		unsigned int vsd_out, unsigned int vsd_delta)
{
	u32 offset;

	offset = get_app_offset(ch_index);

	dev_dbg(mipicsi->dev, "vsd_out=%u,vsd_delta=0x%x\n",
		vsd_out, vsd_delta);

	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSI + offset,
		APP_MIPI_SCALER_VSI_vsi_offset(vsi_offset) |
		APP_MIPI_SCALER_VSI_vsi_phase(vsi_phase));

	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSD + offset,
		APP_MIPI_SCALER_VSD_vsd_out(vsd_out) |
		APP_MIPI_SCALER_VSD_vsd_delta(vsd_delta));
}

static void set_vs_coeff(struct rtk_mipicsi *mipicsi, u8 ch_index)
{
	u32 offset;
	u32 c0;
	u32 c1;
	u32 c2;
	u32 c3;

	offset = get_app_offset(ch_index);

	c0 = 0x02e80203;
	c1 = 0x06d604a5;
	c2 = 0x0b5b092a;
	c3 = 0x0dfd0d18;

	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSYC0 + offset, c0);
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSYC1 + offset, c1);
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSYC2 + offset, c2);
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSYC3 + offset, c3);

	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSCC0 + offset, c0);
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSCC1 + offset, c1);
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSCC2 + offset, c2);
	regmap_write(mipicsi->appreg, APP_MIPI_SCALER_VSCC3 + offset, c3);
}

static void rtk_mipi_scale_down(struct rtk_mipicsi *mipicsi,
		u8 ch_index)
{
	unsigned int src_width, src_height;
	unsigned int dst_width, dst_height;
	unsigned int delta_num, delta_den, offset, phase;

	src_width = mipicsi->src_width;
	src_height = mipicsi->src_height;
	dst_width = mipicsi->dst_width;
	dst_height = mipicsi->dst_height;

	/* set hs_scaler */
	offset = 0;
	phase = 0;
	delta_num = (src_width / dst_width) << 14;
	delta_den = ((src_width % dst_width)*0x4000) / dst_width;
	/* set_hs_scaler: offset,phase,out,delta */
	set_hs_scaler(mipicsi, ch_index, offset, phase,
		dst_width, (delta_num | delta_den));
	set_hs_coeff(mipicsi, ch_index, delta_num);

	/* set vs_scaler */
	offset = 0;
	phase = 0;
	delta_num = (src_height / dst_height) << 14;
	delta_den = ((src_height % dst_height)*0x4000) / dst_height;
	/* set_vs_scaler: offset,phase,out,delta */
	set_vs_scaler(mipicsi, ch_index, offset, phase,
		dst_height, (delta_num | delta_den));
	set_vs_coeff(mipicsi, ch_index);
}

static void rtk_mipi_app_config(struct rtk_mipicsi *mipicsi,
		u32 reg_offset, u8 scaling_down, u8 is_compenc, u8 en_mirror, u8 enable)
{
	u8 src_422_seq = 0;

	if (mipicsi->debug.en_colorbar)
		src_422_seq = 3;

	regmap_write(mipicsi->appreg, APP_MIPI_CTRL0 + reg_offset,
		APP_MIPI_CTRL0_line_mirror_mode(1) |
		APP_MIPI_CTRL0_line_mirror_en(en_mirror) |
		APP_MIPI_CTRL0_yuv422_chroma_ds_en(1) |
		APP_MIPI_CTRL0_yuv444_chroma_ds_mode(1) |
		APP_MIPI_CTRL0_yuv444_chroma_ds_en(scaling_down) |
		APP_MIPI_CTRL0_vs_en(scaling_down) |
		APP_MIPI_CTRL0_vs_near(0) |
		APP_MIPI_CTRL0_vs_yodd(0) |
		APP_MIPI_CTRL0_vs_codd(0) |
		APP_MIPI_CTRL0_hs_en(scaling_down) |
		APP_MIPI_CTRL0_hs_yodd(0) |
		APP_MIPI_CTRL0_hs_codd(0) |
		APP_MIPI_CTRL0_yuv422_chroma_us_mode(1) |
		APP_MIPI_CTRL0_yuv422_chroma_us_en(scaling_down) |
		APP_MIPI_CTRL0_src_422_seq(src_422_seq) |
		APP_MIPI_CTRL0_src_fmt(3) |
		APP_MIPI_CTRL0_comp_linear(is_compenc) |
		APP_MIPI_CTRL0_sw_rst(0) |
		APP_MIPI_CTRL0_go(enable));
}

static void rtk_mipi_sw_reset(struct rtk_mipicsi *mipicsi,
		u8 ch_index, u32 reg_offset)
{
	u32 reg_val = 0;

	regmap_read(mipicsi->appreg, APP_MIPI_CH_STS + reg_offset, &reg_val);
	if (APP_MIPI_CH_STS_get_ch_fsm(reg_val) ||
		APP_MIPI_CH_STS_get_cti_buf_req(reg_val) ||
		APP_MIPI_CH_STS_get_cti_data_left_cnt(reg_val)) {
		dev_err(mipicsi->dev, "Abnormal HW state Ch%u APP_MIPI_CH_STS=0x%08x, skip sw_rst\n",
			ch_index, reg_val);
		return;
	}

	dev_dbg(mipicsi->dev, "channel%u do sw_rst\n", ch_index);

	rtk_mask_write(mipicsi->appreg, APP_MIPI_CTRL0 + reg_offset,
		APP_MIPI_CTRL0_sw_rst_mask, APP_MIPI_CTRL0_sw_rst(1));

	rtk_mask_write(mipicsi->appreg, APP_MIPI_CTRL0 + reg_offset,
		APP_MIPI_CTRL0_sw_rst_mask, APP_MIPI_CTRL0_sw_rst(0));
}

static void rtk_mipi_app_ctrl(struct rtk_mipicsi *mipicsi,
		u8 ch_index, u8 enable)
{
	u32 offset;
	u8 is_compenc;
	u8 scaling_down;
	u8 en_mirror;

	offset = get_app_offset(ch_index);
	is_compenc = mipicsi->mode;
	en_mirror = (u8)mipicsi->mirror_mode;

	if ((mipicsi->src_width > mipicsi->dst_width) ||
		(mipicsi->src_height > mipicsi->dst_height))
		scaling_down = 1;
	else
		scaling_down = 0;

	dev_dbg(mipicsi->dev, "%s channel%u scaling_down=%s is_compenc=%s\n",
			__func__, ch_index,
			scaling_down ? "Y" : "N",
			is_compenc ? "Y" : "N");

	if (scaling_down && enable)
		rtk_mipi_scale_down(mipicsi, ch_index);

	rtk_mipi_app_config(mipicsi, offset, scaling_down,
		is_compenc, en_mirror, enable);

	trace_mipicsi_app_crtl(ch_index, scaling_down, is_compenc, enable);

	if (enable)
		return;

	/* Wait HW finished cti process for 10us before sw_rst */
	udelay(10);

	rtk_mipi_sw_reset(mipicsi, ch_index, offset);
}

static void rtk_mipi_app_ctrl_g(struct rtk_mipicsi *mipicsi,
		u8 ch_max, u8 enable)
{
	u32 offset;
	u8 ch_index;
	u8 is_compenc;
	u8 scaling_down;
	u8 en_mirror;

	is_compenc = mipicsi->mode;
	en_mirror = (u8)mipicsi->mirror_mode;

	if ((mipicsi->src_width > mipicsi->dst_width) ||
		(mipicsi->src_height > mipicsi->dst_height))
		scaling_down = 1;
	else
		scaling_down = 0;

	dev_dbg(mipicsi->dev, "%s channel0-%u scaling_down=%s is_compenc=%s\n",
			__func__, ch_max,
			scaling_down ? "Y" : "N",
			is_compenc ? "Y" : "N");

	if (scaling_down && enable) {
		for (ch_index = 0; ch_index <= ch_max; ch_index++) {
			offset = get_app_offset(ch_index);
			rtk_mipi_scale_down(mipicsi, ch_index);
		}
	}

	for (ch_index = 0; ch_index <= ch_max; ch_index++) {
		offset = get_app_offset(ch_index);
		rtk_mipi_app_config(mipicsi, offset, scaling_down,
			is_compenc, en_mirror, enable);
	}

	if (enable)
		return;

	/* Wait HW finished cti process for 10us before sw_rst */
	udelay(10);

	for (ch_index = 0; ch_index <= ch_max; ch_index++) {
		offset = get_app_offset(ch_index);
		rtk_mipi_sw_reset(mipicsi, ch_index, offset);
	}
}

static u32 rtk_mipi_calculate_line_pitch(u32 dst_width, u8 mode)
{
	u32 pitch = 0;

	if (mode == DATA_MODE_COMPENC) {
		pitch = (dst_width + 15) / 16;
		pitch = (pitch + 3) / 4;
		pitch = pitch * 4 * 64;
	} else {
		pitch = roundup(dst_width, 16);
	}

	return pitch;
}

static u32 rtk_mipi_calculate_header_pitch(u32 dst_width, u8 mode)
{
	u32 pitch = 0;

	if (mode == DATA_MODE_LINE)
		return pitch;

	pitch = (dst_width + 15) / 16;
	pitch = (pitch + 3) / 4;
	pitch = pitch * 8;
	pitch = (pitch + 63) / 64;
	pitch = pitch * 64;

	return pitch;
}

static u32 rtk_mipi_calculate_video_size(u32 dst_width, u32 dst_height, u8 mode)
{
	u32 line_size;
	u32 header_size;
	u32 line_pitch;
	u32 header_pitch;

	line_pitch = rtk_mipi_calculate_line_pitch(dst_width, mode);
	header_pitch = rtk_mipi_calculate_header_pitch(dst_width, mode);

	if (mode == DATA_MODE_LINE) {
		line_size = line_pitch * dst_height;
	} else {
		line_size = (dst_height + 3)/4;
		line_size = line_pitch * line_size;
	}

	line_size += line_size/2;

	if (mode == DATA_MODE_LINE)
		return line_size;

	header_size = (dst_height + 3)/4;
	header_size = header_pitch * header_size;
	header_size += header_size/2;

	return line_size + header_size;
}

static void rtk_mipi_app_size_cfg(struct rtk_mipicsi *mipicsi, u8 ch_index)
{
	u32 offset;

	dev_dbg(mipicsi->dev, "%s channel%u src_width=%u src_height=%u dst_width=%u dst_height=%u  mode=%s\n",
		__func__, ch_index,
		mipicsi->src_width, mipicsi->src_height,
		mipicsi->dst_width, mipicsi->dst_height,
		mipicsi->mode ? "COMPENC":"LINE");

	offset = get_app_offset(ch_index);

	regmap_write(mipicsi->appreg, APP_MIPI_SIZE1 + offset,
		APP_MIPI_SIZE1_src_width(mipicsi->src_width) |
		APP_MIPI_SIZE1_src_height(mipicsi->src_height));


	mipicsi->line_pitch = rtk_mipi_calculate_line_pitch(mipicsi->dst_width, mipicsi->mode);

	/* HW, line_pitch unit- 16byte */
	regmap_write(mipicsi->appreg, APP_MIPI_LINE_PITCH + offset,
		APP_MIPI_LINE_PITCH_line_pitch_c(mipicsi->line_pitch/16) |
		APP_MIPI_LINE_PITCH_line_pitch_y(mipicsi->line_pitch/16));

	if (mipicsi->mode == DATA_MODE_LINE) {
		mipicsi->header_pitch = 0;
		goto exit;
	}

	mipicsi->header_pitch = rtk_mipi_calculate_header_pitch(mipicsi->dst_width, mipicsi->mode);

	/* HW, header_pitch unit - 64byte */
	regmap_write(mipicsi->appreg, APP_COMPENC_3 + offset,
			APP_COMPENC_3_header_pitch_c(mipicsi->header_pitch/64) |
			APP_COMPENC_3_header_pitch_y(mipicsi->header_pitch/64));

exit:
	mipicsi->video_size = rtk_mipi_calculate_video_size(mipicsi->dst_width,
			mipicsi->dst_height, mipicsi->mode);

	trace_mipicsi_app_size_cfg(ch_index, mipicsi);

}

static void rtk_mipi_app_size_cfg_g(struct rtk_mipicsi *mipicsi, u8 ch_max)
{
	u8 ch;

	for (ch = 0; ch <= ch_max; ch++)
		rtk_mipi_app_size_cfg(mipicsi, ch);
}

static void rtk_mipi_dma_buf_cfg(struct rtk_mipicsi *mipicsi,
		uint64_t start_addr, u8 ch_index, u8 entry_index)
{
	unsigned int offset;
	uint64_t size;
	uint64_t y_header_sa, y_header_ea;
	uint64_t c_header_sa, c_header_ea;
	uint64_t y_body_sa, y_body_ea;
	uint64_t c_body_sa, c_body_ea;
	uint64_t meta_sa;

	offset = get_app_offset(ch_index);
	offset += (APP_DMA_ENTRY_10 - APP_DMA_ENTRY_00) * entry_index;

	if (mipicsi->mode == DATA_MODE_LINE) {
		size = (uint64_t)mipicsi->line_pitch * mipicsi->dst_height;
	} else {
		size = (mipicsi->dst_height + 3)/4;
		size = mipicsi->header_pitch * size;
	}

	y_header_sa = start_addr;
	y_header_ea = y_header_sa + size;

	c_header_sa = y_header_ea;
	c_header_ea = c_header_sa + size/2;

	trace_mipicsi_dma_buf_cfg(ch_index, entry_index, start_addr,
		c_header_ea - y_header_sa);

	/* Y Header SA/EA */
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_00 + offset,
			APP_DMA_ENTRY_00_entry0_sa_y_header(y_header_sa/16));
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_01 + offset,
			APP_DMA_ENTRY_01_entry0_ea_y_header(y_header_ea/16));

	/* C Header SA/EA */
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_04 + offset,
			APP_DMA_ENTRY_04_entry0_sa_c_header(c_header_sa/16));
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_05 + offset,
			APP_DMA_ENTRY_05_entry0_ea_c_header(c_header_ea/16));

	if (mipicsi->mode == DATA_MODE_LINE) {
		meta_sa = c_header_ea;
		goto skip_body;
	}

	size = (mipicsi->dst_height + 3)/4;
	size = mipicsi->line_pitch * size;
	y_body_sa = c_header_ea;
	y_body_ea = y_body_sa + size;
	c_body_sa = y_body_ea;
	c_body_ea = c_body_sa + size/2;

	/* Y Body SA/EA */
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_02 + offset,
			APP_DMA_ENTRY_02_entry0_sa_y_body(y_body_sa/16));
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_03 + offset,
			APP_DMA_ENTRY_03_entry0_ea_y_body(y_body_ea/16));

	/* C Body SA/EA */
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_06 + offset,
			APP_DMA_ENTRY_06_entry0_sa_c_body(c_body_sa/16));
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_07 + offset,
			APP_DMA_ENTRY_07_entry0_ea_c_body(c_body_ea/16));

	meta_sa = c_body_ea;

skip_body:

	/* Meta data(48bytes) SA */
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_08 + offset,
			APP_DMA_ENTRY_08_entry0_ma(meta_sa/16));

	/* Indicate dma buf is ready */
	regmap_write(mipicsi->appreg, APP_DMA_ENTRY_09 + offset,
			APP_DMA_ENTRY_09_write_en1(1) |
			APP_DMA_ENTRY_09_entry0_valid(1));

}

static void rtk_mipi_clear_done_flag(struct rtk_mipicsi *mipicsi,
		u8 ch_index, u8 entry_index)
{
	unsigned int bit_shift;

	/* TOP_0_INT_STS_SCPU_1 */
	bit_shift = 1 + ch_index * 4 + entry_index;
	regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_1, BIT(bit_shift));
}

static void rtk_mipi_clear_all_flags(struct rtk_mipicsi *mipicsi,
		u8 ch_index)
{
	unsigned int mask0 = 0;
	unsigned int mask1 = 0;

	if (ch_index > CH_MAX)
		return;

	dev_dbg(mipicsi->dev, "%s ch%u\n", __func__, ch_index);

	switch (ch_index) {
	case CH_0:
		mask0 = TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch0_mask;
		mask1 = TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch0_mask;
		break;
	case CH_1:
		mask0 = TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch1_mask;
		mask1 = TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch1_mask;
		break;
	case CH_2:
		mask0 = TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch2_mask;
		mask1 = TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch2_mask;
		break;
	case CH_3:
		mask0 = TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch3_mask;
		mask1 = TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch3_mask;
		break;
	case CH_4:
		mask0 = TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch4_mask;
		mask1 = TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch4_mask;
		break;
	case CH_5:
		mask0 = TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch5_mask;
		mask1 = TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch5_mask;
		break;
	}

	regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_0, mask0);
	regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_1, mask1);
}

static void rtk_mipi_clear_all_flags_g(struct rtk_mipicsi *mipicsi,
		u8 ch_max)
{
	unsigned int mask0 = 0;
	unsigned int mask1 = 0;

	dev_dbg(mipicsi->dev, "%s ch0-%u\n", __func__, ch_max);

	switch (ch_max) {
	case CH_5:
		mask0 |= TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch5_mask;
		mask1 |= TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch5_mask;
		fallthrough;
	case CH_4:
		mask0 |= TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch4_mask;
		mask1 |= TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch4_mask;
		fallthrough;
	case CH_3:
		mask0 |= TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch3_mask;
		mask1 |= TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch3_mask;
		fallthrough;
	case CH_2:
		mask0 |= TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch2_mask;
		mask1 |= TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch2_mask;
		fallthrough;
	case CH_1:
		mask0 |= TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch1_mask;
		mask1 |= TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch1_mask;
		fallthrough;
	case CH_0:
		mask0 |= TOP_0_INT_STS_SCPU_0_scpu_error_sts_ch0_mask;
		mask1 |= TOP_0_INT_STS_SCPU_1_scpu_done_sts_ch0_mask;
		break;
	}

	regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_0, mask0);
	regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_1, mask1);
}

static void rtk_mipi_dump_frame_state(struct rtk_mipicsi *mipicsi,
		u8 ch_index)
{
	u8 lane_flags = 0;
	u32 offset;
	u32 reg_val = 0;
	u32 frame_cnt;
	u32 line_cnt;
	u32 pixel_cnt;
	u32 frame_height;
	u32 frame_width;
	u32 errcnt;

	if (ch_index > CH_MAX)
		return;

	switch (ch_index) {
	case CH_0:
		offset = get_phy_offset(MIPI_TOP_0);
		lane_flags = TOP_LANE_0;
		break;
	case CH_1:
		offset = get_phy_offset(MIPI_TOP_0);
		lane_flags = TOP_LANE_1;
		break;
	case CH_4:
		offset = get_phy_offset(MIPI_TOP_1);
		lane_flags = TOP_LANE_0;
		break;
	case CH_5:
		offset = get_phy_offset(MIPI_TOP_1);
		lane_flags = TOP_LANE_1;
		break;
	}

	if (lane_flags & TOP_LANE_0) {
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_FRAME_CNT_CH0 + offset, &reg_val);
		frame_cnt = reg_val & 0xFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_LINE_CNT_CH0 + offset, &reg_val);
		line_cnt = reg_val & 0xFFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_PIX_CNT_CH0 + offset, &reg_val);
		pixel_cnt = reg_val & 0xFFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_FRAME_HEIGHT_CH0 + offset, &reg_val);
		frame_height = reg_val & 0xFFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_FRAME_WIDTH_CH0 + offset, &reg_val);
		frame_width = reg_val & 0xFFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_MIPI2ISP_DEBUG + offset, &reg_val);
		errcnt = MIPI0_DPHY_MIPI2ISP_DEBUG_get_errcnt(reg_val);

		trace_mipicsi_dump_frame_state(ch_index, frame_width, frame_height,
			errcnt, frame_cnt, line_cnt, pixel_cnt);

		/* Clear count */
		regmap_write(mipicsi->phyreg, MIPI0_DPHY_FRAME_CNT_CH0 + offset,
			MIPI0_DPHY_FRAME_CNT_CH0_frame_cnt_clr_ch0(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_LINE_CNT_CH0 + offset,
			MIPI0_DPHY_LINE_CNT_CH0_line_cnt_clr_ch0(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_PIX_CNT_CH0 + offset,
			MIPI0_DPHY_PIX_CNT_CH0_pix_cnt_clr_ch0(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_FRAME_HEIGHT_CH0 + offset,
			MIPI0_DPHY_FRAME_HEIGHT_CH0_frame_height_clr_ch0(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_FRAME_WIDTH_CH0 + offset,
			MIPI0_DPHY_FRAME_WIDTH_CH0_frame_width_clr_ch0(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_MIPI2ISP_DEBUG + offset,
			MIPI0_DPHY_MIPI2ISP_DEBUG_errcnt_clr(1));
	}

	if (lane_flags & TOP_LANE_1) {
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_FRAME_CNT_CH1 + offset, &reg_val);
		frame_cnt = reg_val & 0xFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_LINE_CNT_CH1 + offset, &reg_val);
		line_cnt = reg_val & 0xFFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_PIX_CNT_CH1 + offset, &reg_val);
		pixel_cnt = reg_val & 0xFFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_FRAME_HEIGHT_CH1 + offset, &reg_val);
		frame_height = reg_val & 0xFFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_FRAME_WIDTH_CH1 + offset, &reg_val);
		frame_width = reg_val & 0xFFF;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_MIPI2ISP_DEBUG + offset, &reg_val);
		errcnt = MIPI0_DPHY_MIPI2ISP_DEBUG_get_errcnt(reg_val);

		trace_mipicsi_dump_frame_state(ch_index, frame_width, frame_height,
			errcnt, frame_cnt, line_cnt, pixel_cnt);

		/* Clear count */
		regmap_write(mipicsi->phyreg, MIPI0_DPHY_FRAME_CNT_CH1 + offset,
			MIPI0_DPHY_FRAME_CNT_CH1_frame_cnt_clr_ch1(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_LINE_CNT_CH1 + offset,
			MIPI0_DPHY_LINE_CNT_CH1_line_cnt_clr_ch1(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_PIX_CNT_CH1 + offset,
			MIPI0_DPHY_PIX_CNT_CH1_pix_cnt_clr_ch1(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_FRAME_HEIGHT_CH1 + offset,
			MIPI0_DPHY_FRAME_HEIGHT_CH1_frame_height_clr_ch1(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_FRAME_WIDTH_CH1 + offset,
			MIPI0_DPHY_FRAME_WIDTH_CH1_frame_width_clr_ch1(1));
		regmap_write(mipicsi->phyreg,  MIPI0_DPHY_MIPI2ISP_DEBUG + offset,
			MIPI0_DPHY_MIPI2ISP_DEBUG_errcnt_clr(1));
	}

	/* HW not support LANE_2 and LANE_3 frame state  */

}

static void rtk_mipi_frame_detect_ctrl(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 enable)
{
	u32 offset;
	u32 inten_mask = 0;
	u32 inten_val = 0;

	offset = get_phy_offset(top_index);

	rtk_mask_write(mipicsi->phyreg, MIPI0_DPHY_CFG0 + offset,
		MIPI0_DPHY_CFG0_crc16_en_mask |
		MIPI0_DPHY_CFG0_ecc_en_mask,
		MIPI0_DPHY_CFG0_crc16_en(enable) |
		MIPI0_DPHY_CFG0_ecc_en(enable));

	inten_mask |= (MIPI0_DPHY_INTP_EN_mipi_frame_start_ch0_inten_mask |
					MIPI0_DPHY_INTP_EN_mipi_frame_end_ch0_inten_mask);
	inten_val |= (MIPI0_DPHY_INTP_EN_mipi_frame_start_ch0_inten(enable) |
					MIPI0_DPHY_INTP_EN_mipi_frame_end_ch0_inten(enable));

	inten_mask |= (MIPI0_DPHY_INTP_EN_mipi_frame_start_ch1_inten_mask |
					MIPI0_DPHY_INTP_EN_mipi_frame_end_ch1_inten_mask);
	inten_val |= (MIPI0_DPHY_INTP_EN_mipi_frame_start_ch1_inten(enable) |
					MIPI0_DPHY_INTP_EN_mipi_frame_end_ch1_inten(enable));

	if (top_index == MIPI_TOP_0) {
		inten_mask |= (MIPI0_DPHY_INTP_EN_mipi_frame_start_ch2_inten_mask |
						MIPI0_DPHY_INTP_EN_mipi_frame_end_ch2_inten_mask);
		inten_val |= (MIPI0_DPHY_INTP_EN_mipi_frame_start_ch2_inten(enable) |
						MIPI0_DPHY_INTP_EN_mipi_frame_end_ch2_inten(enable));

		inten_mask |= (MIPI0_DPHY_INTP_EN_mipi_frame_start_ch3_inten_mask |
						MIPI0_DPHY_INTP_EN_mipi_frame_end_ch3_inten_mask);
		inten_val |= (MIPI0_DPHY_INTP_EN_mipi_frame_start_ch3_inten(enable) |
						MIPI0_DPHY_INTP_EN_mipi_frame_end_ch3_inten(enable));
	}

	rtk_mask_write(mipicsi->phyreg, MIPI0_DPHY_INTP_EN + offset,
			inten_mask |
			MIPI0_DPHY_INTP_EN_crc16_err_inten_mask |
			MIPI0_DPHY_INTP_EN_ecc_err_inten_mask,
			inten_val |
			MIPI0_DPHY_INTP_EN_crc16_err_inten(enable) |
			MIPI0_DPHY_INTP_EN_ecc_err_inten(enable));

	if (!enable) {
		u32 intp_flag = 0;
		u32 reg_val = 0;

		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_INTP_FLAG + offset, &intp_flag);
		regmap_write(mipicsi->phyreg, MIPI0_DPHY_INTP_CLR + offset, intp_flag);

		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_CHECK + offset, &reg_val);
		if (MIPI0_DPHY_CHECK_get_crc16_err(reg_val))
			regmap_write(mipicsi->phyreg, MIPI0_DPHY_INTP_FLAG + offset,
				MIPI0_DPHY_INTP_FLAG_crc16_err_st(1));

		if (MIPI0_DPHY_CHECK_get_ecc_error(reg_val))
			regmap_write(mipicsi->phyreg, MIPI0_DPHY_INTP_FLAG + offset,
				MIPI0_DPHY_INTP_FLAG_ecc_err_st(1));
	}
}

static bool rtk_mipi_phy_check(struct rtk_mipicsi *mipicsi,
		u8 top_index, u32 timeout_ms)
{
	bool result = false;
	u32 offset;
	u32 reg_val = 0;
	u32 crc16_err = 0;
	u32 ecc_error = 0;
	u32 end_cnt_ch0 = 0;
	u32 end_cnt_ch1 = 0;
	u32 end_cnt_ch2 = 0;
	u32 end_cnt_ch3 = 0;
	u32 crc16_err_st = 0;
	u32 ecc_err_st = 0;
	unsigned long time_start;

	rtk_mipi_frame_detect_ctrl(mipicsi, top_index, ENABLE);

	offset = get_phy_offset(top_index);

	regmap_read(mipicsi->phyreg,  MIPI0_DPHY_CFG0 + offset, &reg_val);

	if (!reg_val) {
		dev_info(mipicsi->dev, "frame_detect disabled, skip frame check\n");
		return result;
	}

	time_start = jiffies;
	while (1) {
		u32 intp_flag;

		reg_val = 0;
		intp_flag = 0;
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_CHECK + offset, &reg_val);
		regmap_read(mipicsi->phyreg,  MIPI0_DPHY_INTP_FLAG + offset, &intp_flag);

		if (MIPI0_DPHY_CHECK_get_crc16_err(reg_val))
			crc16_err += MIPI0_DPHY_CHECK_get_crc16_err(reg_val);

		if (MIPI0_DPHY_CHECK_get_ecc_error(reg_val))
			ecc_error += MIPI0_DPHY_CHECK_get_ecc_error(reg_val);

		if (MIPI0_DPHY_INTP_FLAG_get_mipi_frame_end_ch0_int(intp_flag))
			end_cnt_ch0++;

		if (MIPI0_DPHY_INTP_FLAG_get_mipi_frame_end_ch1_int(intp_flag))
			end_cnt_ch1++;

		if (MIPI0_DPHY_INTP_FLAG_get_mipi_frame_end_ch2_int(intp_flag))
			end_cnt_ch2++;

		if (MIPI0_DPHY_INTP_FLAG_get_mipi_frame_end_ch3_int(intp_flag))
			end_cnt_ch3++;

		if (MIPI0_DPHY_INTP_FLAG_get_crc16_err_st(intp_flag))
			crc16_err_st++;

		if (MIPI0_DPHY_INTP_FLAG_get_ecc_err_st(intp_flag))
			ecc_err_st++;

		regmap_write(mipicsi->phyreg, MIPI0_DPHY_INTP_CLR + offset, intp_flag);

		if (jiffies_to_msecs(jiffies - time_start) > timeout_ms)
			break;

		if (crc16_err || ecc_error || crc16_err_st || ecc_err_st)
			break;
	}

	dev_info(mipicsi->dev, "Finish frame check, %ums",
		jiffies_to_msecs(jiffies - time_start));

	dev_info(mipicsi->dev, "frame_cnt ch0=%u ch1=%u ch2=%u ch3=%u\n",
				end_cnt_ch0, end_cnt_ch1, end_cnt_ch2, end_cnt_ch3);
	dev_info(mipicsi->dev, "crc16_err=%u ecc_error=%u crc16_err_st=%u ecc_err_st=%u\n",
				crc16_err, ecc_error, crc16_err_st, ecc_err_st);

	rtk_mipi_frame_detect_ctrl(mipicsi, top_index, DISABLE);

	if (crc16_err == 0 && ecc_error == 0 &&
		crc16_err_st == 0 && ecc_err_st == 0)
		result = true;

	return result;
}

static void rtk_mipi_dump_aphy_cfg(struct rtk_mipicsi *mipicsi,
		u8 top_index)
{
	u32 offset;
	u32 aphy_reg;
	u32 index;
	u32 reg_val = 0;

	offset = get_phy_offset(top_index);

	index = 0;
	for (aphy_reg = MIPI0_APHY_REG0; aphy_reg <= MIPI0_APHY_REG20; aphy_reg += 4) {
		regmap_read(mipicsi->phyreg,  aphy_reg + offset, &reg_val);
		dev_info(mipicsi->dev, "MIPI%u_APHY_REG%u=0x%08x\n",
			top_index, index++, reg_val);
	}

	index = 27;
	for (aphy_reg = MIPI0_APHY_REG27; aphy_reg <= MIPI0_APHY_REG29; aphy_reg += 4) {
		regmap_read(mipicsi->phyreg,  aphy_reg + offset, &reg_val);
		dev_info(mipicsi->dev, "MIPI%u_APHY_REG%u=0x%08x\n",
			top_index, index++, reg_val);
	}

	index = 32;
	for (aphy_reg = MIPI0_APHY_REG32; aphy_reg <= MIPI0_APHY_REG41; aphy_reg += 4) {
		regmap_read(mipicsi->phyreg,  aphy_reg + offset, &reg_val);
		dev_info(mipicsi->dev, "MIPI%u_APHY_REG%u=0x%08x\n",
			top_index, index++, reg_val);
	}

	index = 43;
	for (aphy_reg = MIPI0_APHY_REG43; aphy_reg <= MIPI0_APHY_REG52; aphy_reg += 4) {
		regmap_read(mipicsi->phyreg,  aphy_reg + offset, &reg_val);
		dev_info(mipicsi->dev, "MIPI%u_APHY_REG%u=0x%08x\n",
			top_index, index++, reg_val);
	}
}

static void rtk_mipi_dump_meta_data(struct rtk_mipicsi *mipicsi,
		struct rtk_mipi_meta_data *meta_data)
{
	uint64_t f_start_ts;
	uint64_t f_end_ts;

	/* Dump meta data info */
	f_start_ts = meta_data->f_start_ts_m;
	f_start_ts = (f_start_ts << 32) | meta_data->f_start_ts_l;
	f_end_ts = meta_data->f_end_ts_m;
	f_end_ts = (f_end_ts << 32) | meta_data->f_end_ts_l;

	dev_info(mipicsi->dev, "f_start_ts=0x%16llx f_end_ts=0x%16llx\n",
		f_start_ts, f_end_ts);
	dev_info(mipicsi->dev, "sa_y_header=0x%08x sa_y_body=0x%08x\n",
		meta_data->sa_y_header, meta_data->sa_y_body);
	dev_info(mipicsi->dev, "sa_c_header=0x%08x sa_c_body=0x%08x\n",
		meta_data->sa_c_header, meta_data->sa_c_body);
	dev_info(mipicsi->dev, "height_mis=%u width_mis=%u\n",
		get_meta_misc0_height_mis(meta_data->misc0),
		get_meta_misc0_width_mis(meta_data->misc0));

	dev_info(mipicsi->dev, "crc=0x%02x fmt=%u mode=%u frame_cnt=%u\n",
		get_meta_misc0_crc(meta_data->misc0),
		get_meta_misc0_fmt(meta_data->misc0),
		get_meta_misc0_mode(meta_data->misc0),
		get_meta_misc0_frame_cnt(meta_data->misc0));

}

static void rtk_mipi_dump_entry_state(struct rtk_mipicsi *mipicsi,
		u8 ch_index, u8 entry_index)
{
	unsigned int offset;
	unsigned int reg_val = 0;

	offset = get_app_offset(ch_index);
	offset += (APP_DMA_ENTRY_10 - APP_DMA_ENTRY_00) * entry_index;

	regmap_read(mipicsi->appreg, APP_DMA_ENTRY_09 + offset, &reg_val);
	dev_dbg(mipicsi->dev, "Ch%u entry%u size_mismatch=%u ov_range=%u frame_cnt=%u done=%u\n",
		ch_index, entry_index,
		APP_DMA_ENTRY_09_get_entry0_size_mismatch(reg_val),
		APP_DMA_ENTRY_09_get_entry0_ov_range(reg_val),
		APP_DMA_ENTRY_09_get_entry0_frame_cnt(reg_val),
		APP_DMA_ENTRY_09_get_entry0_done(reg_val));

	regmap_read(mipicsi->topreg, TOP_0_CTI_BUF_UTI, &reg_val);
	dev_dbg(mipicsi->dev, "cti_buf: full_num_src=%u max_det_sel=%u count_clr=%u max_count=%u\n",
		TOP_0_CTI_BUF_UTI_get_cti_buf_full_num(reg_val),
		TOP_0_CTI_BUF_UTI_get_cti_buf_max_det_sel(reg_val),
		TOP_0_CTI_BUF_UTI_get_cti_buf_count_clr(reg_val),
		TOP_0_CTI_BUF_UTI_get_cti_buf_max_count(reg_val));

}

static void rtk_mipi_interrupt_ctrl(struct rtk_mipicsi *mipicsi,
		u8 ch_index, u8 enable)
{
	unsigned int mask0, mask1;
	unsigned int val0 = 0, val1 = 0;

	if (ch_index > CH_MAX)
		return;

	if (enable)
		rtk_mipi_clear_all_flags(mipicsi, ch_index);

	dev_dbg(mipicsi->dev, "%s ch%u interrupt\n",
		enable ? "Enable":"Disable", ch_index);

	switch (ch_index) {
	case CH_0:
		mask0 = TOP_0_INTEN_SCPU_0_err_inten_mask(ch0);
		mask1 = TOP_0_INTEN_SCPU_1_done_inten_mask(ch0);
		break;
	case CH_1:
		mask0 = TOP_0_INTEN_SCPU_0_err_inten_mask(ch1);
		mask1 = TOP_0_INTEN_SCPU_1_done_inten_mask(ch1);
		break;
	case CH_2:
		mask0 = TOP_0_INTEN_SCPU_0_err_inten_mask(ch2);
		mask1 = TOP_0_INTEN_SCPU_1_done_inten_mask(ch2);
		break;
	case CH_3:
		mask0 = TOP_0_INTEN_SCPU_0_err_inten_mask(ch3);
		mask1 = TOP_0_INTEN_SCPU_1_done_inten_mask(ch3);
		break;
	case CH_4:
		mask0 = TOP_0_INTEN_SCPU_0_err_inten_mask(ch4);
		mask1 = TOP_0_INTEN_SCPU_1_done_inten_mask(ch4);
		break;
	case CH_5:
		mask0 = TOP_0_INTEN_SCPU_0_err_inten_mask(ch5);
		mask1 = TOP_0_INTEN_SCPU_1_done_inten_mask(ch5);
		break;
	}

	if (enable) {
		val0 = mask0;
		val1 = mask1;
	}

	dev_dbg(mipicsi->dev, "TOP_0_INTEN_SCPU_0 mask=0x%08x val=0x%08x\n", mask0, val0);
	dev_dbg(mipicsi->dev, "TOP_0_INTEN_SCPU_1 mask=0x%08x val=0x%08x\n", mask1, val1);

	rtk_mask_write(mipicsi->topreg, TOP_0_INTEN_SCPU_0, mask0, val0);
	rtk_mask_write(mipicsi->topreg, TOP_0_INTEN_SCPU_1, mask1, val1);

	if (!enable)
		rtk_mipi_clear_all_flags(mipicsi, ch_index);
}

static void rtk_mipi_interrupt_ctrl_g(struct rtk_mipicsi *mipicsi,
		u8 ch_max, u8 enable)
{
	unsigned int mask0 = 0, mask1 = 0;
	unsigned int val0 = 0, val1 = 0;

	if (enable)
		rtk_mipi_clear_all_flags_g(mipicsi, ch_max);

	dev_dbg(mipicsi->dev, "%s ch0-%u interrupt\n",
		enable ? "Enable":"Disable", ch_max);

	switch (ch_max) {
	case CH_5:
		mask0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch5);
		mask1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch5);
		if (enable) {
			val0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch5);
			val1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch5);
		}
		fallthrough;
	case CH_4:
		mask0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch4);
		mask1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch4);
		if (enable) {
			val0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch4);
			val1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch4);
		}
		fallthrough;
	case CH_3:
		mask0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch3);
		mask1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch3);
		if (enable) {
			val0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch3);
			val1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch3);
		}
		fallthrough;
	case CH_2:
		mask0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch2);
		mask1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch2);
		if (enable) {
			val0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch2);
			val1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch2);
		}
		fallthrough;
	case CH_1:
		mask0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch1);
		mask1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch1);
		if (enable) {
			val0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch1);
			val1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch1);
		}
		fallthrough;
	case CH_0:
		mask0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch0);
		mask1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch0);
		if (enable) {
			val0 |= TOP_0_INTEN_SCPU_0_err_inten_mask(ch0);
			val1 |= TOP_0_INTEN_SCPU_1_done_inten_mask(ch0);
		}
		break;
	}

	dev_dbg(mipicsi->dev, "TOP_0_INTEN_SCPU_0 mask=0x%08x val=0x%08x\n", mask0, val0);
	dev_dbg(mipicsi->dev, "TOP_0_INTEN_SCPU_1 mask=0x%08x val=0x%08x\n", mask1, val1);
	rtk_mask_write(mipicsi->topreg, TOP_0_INTEN_SCPU_0, mask0, val0);
	rtk_mask_write(mipicsi->topreg, TOP_0_INTEN_SCPU_1, mask1, val1);

	if (!enable)
		rtk_mipi_clear_all_flags_g(mipicsi, ch_max);
}

static int rtk_mipi_get_intr_state(struct rtk_mipicsi *mipicsi,
		u32 *p_done_st, u8 ch_index)
{
	int ret = 0;
	u32 err_st;
	u32 mismatch_mask = 0;

	regmap_read(mipicsi->topreg, TOP_0_INT_STS_SCPU_0, &err_st);
	regmap_read(mipicsi->topreg, TOP_0_INT_STS_SCPU_1, p_done_st);

	if (ch_index > CH_MAX)
		return ret;

	switch (ch_index) {
	case CH_0:
		mismatch_mask = TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch0_mask;
		break;
	case CH_1:
		mismatch_mask = TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch1_mask;
		break;
	case CH_2:
		mismatch_mask = TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch2_mask;
		break;
	case CH_3:
		mismatch_mask = TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch3_mask;
		break;
	case CH_4:
		mismatch_mask = TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch4_mask;
		break;
	case CH_5:
		mismatch_mask = TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch5_mask;
		break;
	}

	if (*p_done_st & mismatch_mask) {
		dev_err(mipicsi->dev, "Error: TOP_0_INT_STS_SCPU_1=0x%08x\n", *p_done_st);
		regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_1, mismatch_mask);
		rtk_mipi_dump_frame_state(mipicsi, ch_index);
		ret = -EIO;
	}

	if (err_st) {
		dev_err(mipicsi->dev, "Error: TOP_0_INT_STS_SCPU_0=0x%08x\n", err_st);
		regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_0, err_st);
	}

	return ret;
}

static int rtk_mipi_get_intr_state_g(struct rtk_mipicsi *mipicsi,
			u32 *p_done_st, u8 ch_max)
{
	int ret = 0;
	u32 err_st;
	u32 mismatch_mask = 0;

	regmap_read(mipicsi->topreg, TOP_0_INT_STS_SCPU_0, &err_st);
	regmap_read(mipicsi->topreg, TOP_0_INT_STS_SCPU_1, p_done_st);

	if (ch_max > CH_MAX)
		return ret;

	switch (ch_max) {
	case CH_5:
		mismatch_mask |= TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch5_mask;
		fallthrough;
	case CH_4:
		mismatch_mask |= TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch4_mask;
		fallthrough;
	case CH_3:
		mismatch_mask |= TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch3_mask;
		fallthrough;
	case CH_2:
		mismatch_mask |= TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch2_mask;
		fallthrough;
	case CH_1:
		mismatch_mask |= TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch1_mask;
		fallthrough;
	case CH_0:
		mismatch_mask |= TOP_0_INT_STS_SCPU_1_scpu_mismatch_sts_ch0_mask;
		break;
	}

	if (*p_done_st & mismatch_mask) {
		dev_err(mipicsi->dev, "Error: TOP_0_INT_STS_SCPU_1=0x%08x\n", *p_done_st);
		regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_1, mismatch_mask);
		ret = -EIO;
	}

	if (err_st) {
		dev_err(mipicsi->dev, "Error: TOP_0_INT_STS_SCPU_0=0x%08x\n", err_st);
		regmap_write(mipicsi->topreg, TOP_0_INT_STS_SCPU_0, err_st);
	}

	return ret;
}

static void rtk_mipi_csi_meta_swap(struct rtk_mipicsi *mipicsi,
		u8 enable)
{
	dev_dbg(mipicsi->dev, "%s SWAP\n", enable ? "Enable":"Disable");

	regmap_write(mipicsi->topreg, TOP_0_SWAP_SEL,
			TOP_0_SWAP_SEL_cti_wdata_swap(0) |
			TOP_0_SWAP_SEL_reg_metadata_byte_swap(enable) |
			TOP_0_SWAP_SEL_reg_header_byte_swap(0) |
			TOP_0_SWAP_SEL_reg_body_byte_swap(0));
}

static void rtk_mipi_csi_crc(struct rtk_mipicsi *mipicsi, u8 enable)
{
	dev_dbg(mipicsi->dev, "%s CRC function\n", enable ? "Enable":"Disable");

	regmap_write(mipicsi->topreg, TOP_0_CRC,
			TOP_0_CRC_crc_enable(enable));
}

static void rtk_mipi_csi_color_bar(struct rtk_mipicsi *mipicsi, u8 enable)
{

	dev_dbg(mipicsi->dev, "%s color bar width=%u height=%u\n",
		enable ? "Enable":"Disable",  mipicsi->src_width, mipicsi->src_height);

	regmap_write(mipicsi->topreg, TOP_0_COLOR_BAR_0,
			TOP_0_COLOR_BAR_0_color_bar_hf(6) |
			TOP_0_COLOR_BAR_0_color_bar_heigh(mipicsi->src_height) |
			TOP_0_COLOR_BAR_0_color_bar_width(mipicsi->src_width));

	regmap_write(mipicsi->topreg, TOP_0_COLOR_BAR_1,
			TOP_0_COLOR_BAR_1_color_bar_sel(enable) |
			TOP_0_COLOR_BAR_1_color_bar_en(enable) |
			TOP_0_COLOR_BAR_1_color_bar_vb(3) |
			TOP_0_COLOR_BAR_1_color_bar_vs(5) |
			TOP_0_COLOR_BAR_1_color_bar_vf(2) |
			TOP_0_COLOR_BAR_1_color_bar_hb(8) |
			TOP_0_COLOR_BAR_1_color_bar_hs(4));
}

static void rtk_mipi_urgent_ctrl(struct rtk_mipicsi *mipicsi,
			unsigned char enable)
{
	dev_dbg(mipicsi->dev, "%s urgent_ctrl\n", enable ? "Enable":"Disable");

	regmap_write(mipicsi->topreg, TOP_0_CTI_URGENT_CTRL,
			TOP_0_CTI_URGENT_CTRL_urgent_enable(enable) |
			TOP_0_CTI_URGENT_CTRL_urgent_thr_high(6) |
			TOP_0_CTI_URGENT_CTRL_urgent_thr_low(3));
}

static const struct rtk_mipicsi_ops mipicsi_ops = {
	.is_frame_done = is_frame_done,
	.scale_down = rtk_mipi_scale_down,
	.calculate_video_size = rtk_mipi_calculate_video_size,
	.app_size_cfg = rtk_mipi_app_size_cfg,
	.dma_buf_cfg = rtk_mipi_dma_buf_cfg,
	.clear_done_flag = rtk_mipi_clear_done_flag,
	.dump_frame_state = rtk_mipi_dump_frame_state,
	.phy_check = rtk_mipi_phy_check,
	.eq_tuning = rtk_mipi_eq_tuning,
	.aphy_set_manual_skw = rtk_mipi_aphy_set_manual_skw,
	.aphy_get_manual_skw = rtk_mipi_aphy_get_manual_skw,
	.aphy_set_skw_sclk = rtk_mipi_aphy_set_skw_sclk,
	.aphy_get_skw_sclk = rtk_mipi_aphy_get_skw_sclk,
	.aphy_set_skw_sdata = rtk_mipi_aphy_set_skw_sdata,
	.aphy_get_skw_sdata = rtk_mipi_aphy_get_skw_sdata,
	.aphy_set_ctune = rtk_mipi_aphy_set_ctune,
	.aphy_get_ctune = rtk_mipi_aphy_get_ctune,
	.aphy_set_rtune = rtk_mipi_aphy_set_rtune,
	.aphy_get_rtune = rtk_mipi_aphy_get_rtune,
	.aphy_set_d2s = rtk_mipi_aphy_set_d2s,
	.aphy_get_d2s = rtk_mipi_aphy_get_d2s,
	.aphy_set_ibn_dc = rtk_mipi_aphy_set_ibn_dc,
	.aphy_get_ibn_dc = rtk_mipi_aphy_get_ibn_dc,
	.aphy_set_ibn_gm = rtk_mipi_aphy_set_ibn_gm,
	.aphy_get_ibn_gm = rtk_mipi_aphy_get_ibn_gm,
	.dump_aphy_cfg = rtk_mipi_dump_aphy_cfg,
	.dump_meta_data = rtk_mipi_dump_meta_data,
	.dump_entry_state = rtk_mipi_dump_entry_state,
	.meta_swap = rtk_mipi_csi_meta_swap,
	.crc_ctrl = rtk_mipi_csi_crc,
	.urgent_ctrl = rtk_mipi_urgent_ctrl,
	.color_bar_test = rtk_mipi_csi_color_bar,
	.app_ctrl = rtk_mipi_app_ctrl,
	.interrupt_ctrl = rtk_mipi_interrupt_ctrl,
	.get_intr_state = rtk_mipi_get_intr_state,
};

static const struct rtk_mipicsi_ops mipicsi_ops_g = {
	.is_frame_done = is_frame_done,
	.scale_down = rtk_mipi_scale_down,
	.calculate_video_size = rtk_mipi_calculate_video_size,
	.app_size_cfg = rtk_mipi_app_size_cfg_g,
	.dma_buf_cfg = rtk_mipi_dma_buf_cfg,
	.clear_done_flag = rtk_mipi_clear_done_flag,
	.dump_frame_state = rtk_mipi_dump_frame_state,
	.phy_check = rtk_mipi_phy_check,
	.eq_tuning = rtk_mipi_eq_tuning,
	.aphy_set_manual_skw = rtk_mipi_aphy_set_manual_skw,
	.aphy_get_manual_skw = rtk_mipi_aphy_get_manual_skw,
	.aphy_set_skw_sclk = rtk_mipi_aphy_set_skw_sclk,
	.aphy_get_skw_sclk = rtk_mipi_aphy_get_skw_sclk,
	.aphy_set_skw_sdata = rtk_mipi_aphy_set_skw_sdata,
	.aphy_get_skw_sdata = rtk_mipi_aphy_get_skw_sdata,
	.aphy_set_ctune = rtk_mipi_aphy_set_ctune,
	.aphy_get_ctune = rtk_mipi_aphy_get_ctune,
	.aphy_set_rtune = rtk_mipi_aphy_set_rtune,
	.aphy_get_rtune = rtk_mipi_aphy_get_rtune,
	.aphy_set_d2s = rtk_mipi_aphy_set_d2s,
	.aphy_get_d2s = rtk_mipi_aphy_get_d2s,
	.aphy_set_ibn_dc = rtk_mipi_aphy_set_ibn_dc,
	.aphy_get_ibn_dc = rtk_mipi_aphy_get_ibn_dc,
	.aphy_set_ibn_gm = rtk_mipi_aphy_set_ibn_gm,
	.aphy_get_ibn_gm = rtk_mipi_aphy_get_ibn_gm,
	.dump_aphy_cfg = rtk_mipi_dump_aphy_cfg,
	.dump_meta_data = rtk_mipi_dump_meta_data,
	.dump_entry_state = rtk_mipi_dump_entry_state,
	.meta_swap = rtk_mipi_csi_meta_swap,
	.crc_ctrl = rtk_mipi_csi_crc,
	.urgent_ctrl = rtk_mipi_urgent_ctrl,
	.color_bar_test = rtk_mipi_csi_color_bar,
	.app_ctrl = rtk_mipi_app_ctrl_g,
	.interrupt_ctrl = rtk_mipi_interrupt_ctrl_g,
	.get_intr_state = rtk_mipi_get_intr_state_g,
};

int rtk_mipi_csi_save_crt(struct rtk_mipicsi *mipicsi,
		struct rtk_mipicsi_crt *crt)
{
	if (IS_ERR_OR_NULL(crt->reset_mipi) || IS_ERR_OR_NULL(crt->clk_mipi) ||
		IS_ERR_OR_NULL(crt->clk_npu_mipi) || IS_ERR_OR_NULL(crt->clk_npu_pll)) {
		dev_err(mipicsi->dev, "Failed to save clk/crt\n");
		return -EINVAL;
	}

	g_crt.reset_mipi = crt->reset_mipi;
	g_crt.clk_mipi = crt->clk_mipi;
	g_crt.clk_npu_mipi = crt->clk_npu_mipi;
	g_crt.clk_npu_pll = crt->clk_npu_pll;

	g_crt.crt_obtained = true;

	return 0;
}

int rtk_mipi_csi_hw_deinit(struct rtk_mipicsi *mipicsi)
{
	if (g_crt.phy_inited && g_crt.crt_obtained) {
		g_crt.phy_inited = false;

		/* Disable all channels */
		rtk_mipi_app_ctrl_g(mipicsi, CH_5, DISABLE);
		rtk_mipi_interrupt_ctrl_g(mipicsi, CH_5, DISABLE);

		rtk_mipi_phy_start(mipicsi, MIPI_TOP_0, DISABLE);
		rtk_mipi_phy_start(mipicsi, MIPI_TOP_1, DISABLE);

		clk_disable_unprepare(g_crt.clk_mipi);
		clk_disable_unprepare(g_crt.clk_npu_mipi);
		reset_control_assert(g_crt.reset_mipi);
	}

	return 0;
}

int rtk_mipi_csi_hw_init(struct rtk_mipicsi *mipicsi)
{
	if (mipicsi->hw_init_done)
		return 0;

	if (soc_device_match(rtk_soc_kent) && (mipicsi->mirror_mode != MIRROR_DIS))
		return -EINVAL;

	if (soc_device_match(rtk_soc_prince) &&
		((mipicsi->ch_index == CH_0) || (mipicsi->ch_index == CH_1))) {
		if ((mipicsi->src_width > 3840) || (mipicsi->src_height > 2160))
			return -EINVAL;
	} else {
		if ((mipicsi->src_width > 1920) || (mipicsi->src_height > 1080))
			return -EINVAL;
	}

	if (mipicsi->conf->en_group_dev)
		mipicsi->hw_ops = &mipicsi_ops_g;
	else
		mipicsi->hw_ops = &mipicsi_ops;

	mipicsi->mode = DATA_MODE_LINE;

	mipicsi->dst_width = 1920;
	mipicsi->dst_height = 1080;

	if (!g_crt.phy_inited && g_crt.crt_obtained) {
		g_crt.phy_inited = true;

		clk_prepare_enable(g_crt.clk_npu_pll); /* npu pll */

		clk_prepare_enable(g_crt.clk_mipi);
		clk_disable(g_crt.clk_mipi);

		reset_control_assert(g_crt.reset_mipi);
		reset_control_deassert(g_crt.reset_mipi);

		clk_prepare_enable(g_crt.clk_npu_mipi); /* clk_en_npu_mipi_csi */

		clk_enable(g_crt.clk_mipi);

		rtk_mipi_set_phy(mipicsi, &dft_phy_cfg, MIPI_TOP_0, TOP_LANE_NUM_4);
		rtk_mipi_set_phy(mipicsi, &dft_phy_cfg, MIPI_TOP_1, TOP_LANE_NUM_4);

		if (mipicsi->skew_mode) {
			rtk_mipi_set_skew(mipicsi, MIPI_TOP_0, TOP_LANE_NUM_4, ENABLE);
			rtk_mipi_set_skew(mipicsi, MIPI_TOP_1, TOP_LANE_NUM_4, ENABLE);
		}

		rtk_mipi_phy_start(mipicsi, MIPI_TOP_0, ENABLE);
		rtk_mipi_phy_start(mipicsi, MIPI_TOP_1, ENABLE);
	}

	mipicsi->hw_init_done = true;

	return 0;
}
