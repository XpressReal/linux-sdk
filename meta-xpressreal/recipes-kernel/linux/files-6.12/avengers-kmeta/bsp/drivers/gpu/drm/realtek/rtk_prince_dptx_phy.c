// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 RealTek Inc.
 */

#include <linux/math64.h>

#include "rtk_dptx_kent_reg.h"
#include "rtk_edp_reg.h"
#include "rtk_crt_reg.h"
#include "rtk_prince_dptx.h"
#include "rtk_prince_dptx_phy.h"
#include "rtk_dp_utils.h"

#define DP14_TX_1_SETTING_CONFIG_CSC_M00 (526)
#define DP14_TX_1_SETTING_CONFIG_CSC_M01 (-303)
#define DP14_TX_1_SETTING_CONFIG_CSC_M02 (899)
#define DP14_TX_1_SETTING_CONFIG_CSC_M10 (1032)
#define DP14_TX_1_SETTING_CONFIG_CSC_M11 (-596)
#define DP14_TX_1_SETTING_CONFIG_CSC_M12 (-754)
#define DP14_TX_1_SETTING_CONFIG_CSC_M20 (201)
#define DP14_TX_1_SETTING_CONFIG_CSC_M21 (899)
#define DP14_TX_1_SETTING_CONFIG_CSC_M22 (-145)
#define DP14_TX_1_SETTING_CONFIG_CSC_A0  (64)
#define DP14_TX_1_SETTING_CONFIG_CSC_A1  (512)
#define DP14_TX_1_SETTING_CONFIG_CSC_A2  (512)

#define DPTX14_CC1_m01_mask (0x3fff0000)
#define DPTX14_CC1_m00_mask (0x3fff)
#define DPTX14_CC2_m10_mask (0x3fff0000)
#define DPTX14_CC2_m02_mask (0x3fff)
#define DPTX14_CC3_m12_mask (0x3fff0000)
#define DPTX14_CC3_m11_mask (0x3fff)
#define DPTX14_CC4_m21_mask (0x3fff0000)
#define DPTX14_CC4_m20_mask (0x3fff)
#define DPTX14_CC5_m22_mask (0x3fff)
#define DPTX14_CC6_a1_mask  (0x7ff0000)
#define DPTX14_CC6_a0_mask  (0x7ff)
#define DPTX14_CC7_a2_mask  (0x7ff)

#define DPTX14_CC1_m01(data) (0x3fff0000&((data)<<16))
#define DPTX14_CC1_m00(data) (0x3fff&((data)<<0))
#define DPTX14_CC2_m10(data) (0x3fff0000&((data)<<16))
#define DPTX14_CC2_m02(data) (0x3fff&((data)<<0))
#define DPTX14_CC3_m12(data) (0x3fff0000&((data)<<16))
#define DPTX14_CC3_m11(data) (0x3fff&((data)<<0))
#define DPTX14_CC4_m21(data) (0x3fff0000&((data)<<16))
#define DPTX14_CC4_m20(data) (0x3fff&((data)<<0))
#define DPTX14_CC5_m22(data) (0x3fff&((data)<<0))
#define DPTX14_CC6_a1(data) (0x7ff0000&((data)<<16))
#define DPTX14_CC6_a0(data) (0x7ff&((data)<<0))
#define DPTX14_CC7_a2(data) (0x7ff&((data)<<0))

#define EDP_TX_1_SETTING_CONFIG_CSC_M00 (526)
#define EDP_TX_1_SETTING_CONFIG_CSC_M10 (-303)
#define EDP_TX_1_SETTING_CONFIG_CSC_M20 (899)
#define EDP_TX_1_SETTING_CONFIG_CSC_M01 (1032)
#define EDP_TX_1_SETTING_CONFIG_CSC_M11 (-596)
#define EDP_TX_1_SETTING_CONFIG_CSC_M21 (-754)
#define EDP_TX_1_SETTING_CONFIG_CSC_M02 (201)
#define EDP_TX_1_SETTING_CONFIG_CSC_M12 (899)
#define EDP_TX_1_SETTING_CONFIG_CSC_M22 (-145)
#define EDP_TX_1_SETTING_CONFIG_CSC_A0  (32)
#define EDP_TX_1_SETTING_CONFIG_CSC_A1  (256)
#define EDP_TX_1_SETTING_CONFIG_CSC_A2  (256)

/* ISO_SYS */
#define POWER_CTRL1 (0x300)
#define ISO_DISP BIT(2)

static const unsigned int disp_dpll_table[62][7] = {
	/* DPLL_M, F code, PLL_N, PLL_K, reserve, DIV, Pixel Freq */
	{0x0F, 0x52F, 0x0, 0x1, 0x1, 0x5, 25175},
	{0x10, 0x2E2, 0x0, 0x1, 0x1, 0x5, 26136},
	{0x12, 0x6D0, 0x0, 0x3, 0x1, 0x4, 29500},
	{0x25, 0x0,	  0x1, 0x3, 0x0, 0x4, 33750},
	{0x14, 0x5A1, 0x0, 0x3, 0x0, 0x4, 40000},
	{0x15, 0x210, 0x0, 0x3, 0x0, 0x4, 40936},
	{0x0F, 0x12F, 0x0, 0x3, 0x1, 0x2, 49000},
	{0x23, 0x19B, 0x1, 0x3, 0x0, 0x2, 64464},
	{0x23, 0x425, 0x1, 0x3, 0x0, 0x2, 65000},
	{0x24, 0x65E, 0x1, 0x3, 0x0, 0x2, 67156},
	{0x25, 0x4EE, 0x1, 0x3, 0x0, 0x2, 68540},
	{0x3E, 0x2BC, 0x2, 0x3, 0x0, 0x2, 74250}, //ok
	{0x14, 0x470, 0x0, 0x1, 0x0, 0x4, 79500},
	{0x34, 0x554, 0x2, 0x2, 0x0, 0x1, 83500},
	{0x16, 0x2E,  0x0, 0x1, 0x0, 0x4, 84451},
	{0x36, 0x0,	  0x2, 0x2, 0x0, 0x1, 85500},
	{0x17, 0x2C5, 0x0, 0x1, 0x0, 0x4, 88920},
	{0x18, 0x540, 0x0, 0x1, 0x0, 0x4, 93340},
	{0x19, 0x353, 0x0, 0x1, 0x0, 0x4, 95904},
	{0x44, 0x0,	  0x2, 0x2, 0x0, 0x2, 106500},
	{0xD,  0x0,	  0x0, 0x3, 0x0, 0x0, 108000},
	{0x1E, 0xE4,  0x1, 0x0, 0x0, 0x4, 111750},
	{0x1E, 0x656, 0x1, 0x0, 0x0, 0x4, 114048},
	{0x20, 0x6D0, 0x1, 0x0, 0x0, 0x4, 121000},
	{0x3E, 0x0,	  0x2, 0x0, 0x0, 0x4, 146250},
	{0x12, 0x7A5, 0x0, 0x3, 0x0, 0x0, 148200},
	{0x3E, 0x2BC, 0x2, 0x3, 0x0, 0x0, 148500}, //ok
	{0x13, 0x352, 0x0, 0x3, 0x0, 0x0, 151300},
	{0x15, 0x0,	  0x0, 0x0, 0x0, 0x4, 162000},
	{0x18, 0x40C, 0x0, 0x3, 0x0, 0x0, 185664},
	{0x19, 0x0,	  0x0, 0x3, 0x0, 0x0, 189000},
	{0x19, 0x509, 0x0, 0x0, 0x0, 0x4, 193250},
	{0x58, 0x0,	  0x2, 0x0, 0x0, 0x4, 204750},
	{0x1B, 0x426, 0x0, 0x0, 0x0, 0x4, 206000},
	{0x5E, 0x0,	  0x2, 0x0, 0x0, 0x4, 218250},
	{0x31, 0x0,	  0x2, 0x1, 0x0, 0x0, 234000},
	{0xE,  0x304, 0x0, 0x1, 0x0, 0x0, 234590}, //ok
	{0xE,  0x5A2, 0x0, 0x1, 0x0, 0x0, 239000},
	{0x10, 0x426, 0x0, 0x1, 0x0, 0x0, 263500},
	{0x10, 0x7C5, 0x0, 0x1, 0x0, 0x0, 269614},
	{0x14, 0x109, 0x0, 0x0, 0x0, 0x2, 312250},
	{0x16, 0x685, 0x0, 0x0, 0x0, 0x2, 348500},
	{0x16, 0x7DC, 0x0, 0x0, 0x0, 0x2, 350760},
	{0x12, 0x747, 0x0, 0x0, 0x1, 0x0, 473250},
	{0x15, 0x190, 0x0, 0x0, 0x1, 0x0, 522614},
	{0x15, 0x580, 0x0, 0x0, 0x1, 0x0, 533250},
	{0x3e, 0x2BC, 0x2, 0x0, 0x0, 0x0, 594000},
};

void rtk_dptx_update(struct regmap *reg_base, u32 reg, u32 clear, u32 bits)
{
	unsigned int val;

	if (!reg_base)
		return;

	regmap_read(reg_base, reg, &val);

	val &= ~clear;
	val |= bits;

	regmap_write(reg_base, reg, val);
}

unsigned int rtk_dptx_read(struct regmap *reg_base, u32 reg)
{
	unsigned int val;

	if (!reg_base)
		return 0;

	regmap_read(reg_base, reg, &val);
	return val;
}

void rtk_dptx_write(struct regmap *reg_base, u32 reg, u32 val)
{
	if (!reg_base)
		return;

	regmap_write(reg_base, reg, val);
}

/**
 * DisplayPort
 */

void rtk_prince_dptx_csc_setting(struct rtk_prince_dptx *dptx)
{
	/* DPTX CSC (color transform matrix) setting */
	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN,
		DPTX14_CC1_m01_mask
		| DPTX14_CC1_m00_mask,
		DPTX14_CC1_m01(DP14_TX_1_SETTING_CONFIG_CSC_M01)
		| DPTX14_CC1_m00(DP14_TX_1_SETTING_CONFIG_CSC_M00));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN,
		DPTX14_CC2_m10_mask
		| DPTX14_CC2_m02_mask,
		DPTX14_CC2_m10(DP14_TX_1_SETTING_CONFIG_CSC_M10)
		| DPTX14_CC2_m02(DP14_TX_1_SETTING_CONFIG_CSC_M02));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN,
		DPTX14_CC3_m12_mask
		| DPTX14_CC3_m11_mask,
		DPTX14_CC3_m12(DP14_TX_1_SETTING_CONFIG_CSC_M12)
		| DPTX14_CC3_m11(DP14_TX_1_SETTING_CONFIG_CSC_M11));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN,
		DPTX14_CC4_m21_mask
		| DPTX14_CC4_m20_mask,
		DPTX14_CC4_m21(DP14_TX_1_SETTING_CONFIG_CSC_M21)
		| DPTX14_CC4_m20(DP14_TX_1_SETTING_CONFIG_CSC_M20));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN,
		DPTX14_CC5_m22_mask,
		DPTX14_CC5_m22(DP14_TX_1_SETTING_CONFIG_CSC_M22));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN,
		DPTX14_CC6_a1_mask
		| DPTX14_CC6_a0_mask,
		DPTX14_CC6_a1(DP14_TX_1_SETTING_CONFIG_CSC_A1)
		| DPTX14_CC6_a0(DP14_TX_1_SETTING_CONFIG_CSC_A0));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN,
		DPTX14_CC7_a2_mask,
		DPTX14_CC7_a2(DP14_TX_1_SETTING_CONFIG_CSC_A2));
}

void rtk_prince_dptx_phy_config_lane(struct rtk_prince_dptx *dptx)
{
	uint32_t ctrl_lane_num = 1;
	uint32_t v2analog = 0;

	dev_info(dptx->dev, "dp config lane = %d\n", dptx->link_train.lane_count);
	switch (dptx->link_train.lane_count) {
	case 1:
		ctrl_lane_num = 1;
		v2analog = 0;
		break;
	case 2:
		ctrl_lane_num = 2;
		v2analog = 3;
		break;
	case 4:
		ctrl_lane_num = 3;
		v2analog = 0xf;
		break;
	default:
		break;
	}

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN, 0,
		DPTX14_MAIN_iso_ana_b(1)
		| DPTX14_MAIN_pow_pll(1));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DP_PHY_CTRL,
		DPTX14_MAC_IP_DP_PHY_CTRL_v2analog_mask
		| DPTX14_MAC_IP_DP_PHY_CTRL_lane_num_mask
		| DPTX14_MAC_IP_DP_PHY_CTRL_mst_en_mask,
		DPTX14_MAC_IP_DP_PHY_CTRL_v2analog(v2analog)
		| DPTX14_MAC_IP_DP_PHY_CTRL_lane_num(ctrl_lane_num)
		| DPTX14_MAC_IP_DP_PHY_CTRL_mst_en(0x0));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DP_MAC_CTRL,
		DPTX14_MAC_IP_DP_MAC_CTRL_enhance_md_mask
		| DPTX14_MAC_IP_DP_MAC_CTRL_lane_num_mask,
		DPTX14_MAC_IP_DP_MAC_CTRL_enhance_md(1)
		| DPTX14_MAC_IP_DP_MAC_CTRL_lane_num(ctrl_lane_num));

	rtk_dptx_write(dptx->dptx14_reg_base, DPTX14_LANE, DPTX14_LANE_num(0x4));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_CLK_GEN,
			DPTX14_MAC_IP_DPTX_CLK_GEN_div_num_mask,
			DPTX14_MAC_IP_DPTX_CLK_GEN_div_num(1));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_SFIFO_CTRL0,
			DPTX14_MAC_IP_DPTX_SFIFO_CTRL0_reg_rd_start_pos_mask,
			DPTX14_MAC_IP_DPTX_SFIFO_CTRL0_reg_rd_start_pos(4));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_SFIFO_CTRL0,
			DPTX14_MAC_IP_DPTX_SFIFO_CTRL0_reg_tx_en_mask,
			DPTX14_MAC_IP_DPTX_SFIFO_CTRL0_reg_tx_en(1));
}

static void rtk_prince_dptx_sst_setting(struct rtk_prince_dptx *dptx,
							struct drm_display_mode *mode)
{
	uint64_t mvid =  0;
	uint32_t nvid = 32768;
	int bpc = (dptx->bpc == 6) ? RTK_DP_COLORBIT_6 :
			 (dptx->bpc == 8) ? RTK_DP_COLORBIT_8 :
			 (dptx->bpc == 10) ? RTK_DP_COLORBIT_10 :
			 (dptx->bpc == 12) ? RTK_DP_COLORBIT_12 :
			 (dptx->bpc == 16) ? RTK_DP_COLORBIT_16 : RTK_DP_COLORBIT_8;
	int component_format = (dptx->color_format == RTK_COLOR_FORMAT_RGB) ? 0x0 :
			 (dptx->color_format == RTK_COLOR_FORMAT_YUV444) ? 0x2 :
			 (dptx->color_format == RTK_COLOR_FORMAT_YUV422) ? 0x1 : 0x0;
	uint32_t link_rate = dptx->link_train.link_rate;

	mvid = div_u64(mul_u32_u32(mode->clock, nvid), link_rate);
	dev_info(dptx->dev, "[%s] MVID: %llu\n", __func__, mvid);

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_M_VID_H,
		DPTX14_MAC_IP_MN_M_VID_H_mvid_23_16(GET_MH_BYTE(mvid)));

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_M_VID_M,
		DPTX14_MAC_IP_MN_M_VID_M_mvid_15_8(GET_ML_BYTE(mvid)));

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_M_VID_L,
		DPTX14_MAC_IP_MN_M_VID_L_mvid_7_0(GET_L_BYTE(mvid)));

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_N_VID_H, 0);
	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_N_VID_M, 0x80);
	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_N_VID_L, 0);

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_VID_AUTO_EN_1,
		DPTX14_MAC_IP_MN_VID_AUTO_EN_1_mn_vid_auto_en_mask
		| DPTX14_MAC_IP_MN_VID_AUTO_EN_1_mn_vid_db_mask,
		DPTX14_MAC_IP_MN_VID_AUTO_EN_1_mn_vid_auto_en(0)
		| DPTX14_MAC_IP_MN_VID_AUTO_EN_1_mn_vid_db(0x1)); //0x40

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MSA_MISC0,
		DPTX14_MAC_IP_MSA_MISC0_colorbit_mask
		| DPTX14_MAC_IP_MSA_MISC0_ycc_col_mask
		| DPTX14_MAC_IP_MSA_MISC0_dyn_range_mask
		| DPTX14_MAC_IP_MSA_MISC0_component_format_mask,
		DPTX14_MAC_IP_MSA_MISC0_colorbit(bpc)
		| DPTX14_MAC_IP_MSA_MISC0_ycc_col(0x0) // ITU-R BT601-5
		| DPTX14_MAC_IP_MSA_MISC0_dyn_range(0x0) // VESA
		| DPTX14_MAC_IP_MSA_MISC0_component_format(component_format));
	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MSA_CTRL,
		DPTX14_MAC_IP_MSA_CTRL_msa_db_mask,
		DPTX14_MAC_IP_MSA_CTRL_msa_db(1)); //0x80
}

static void rtk_prince_dptx_sst_dpformat_setting(struct rtk_prince_dptx *dptx,
								 struct drm_display_mode *mode)
{
	/* RGB,YUV444: 3, YUV422: 2, YUV420: 1.5*/
	uint32_t comp_x = (dptx->color_format == RTK_COLOR_FORMAT_YUV422) ? 2 : 3;
	uint32_t comp_y = (dptx->color_format == RTK_COLOR_FORMAT_YUV420) ? 2 : 1;
	uint32_t bpc = (uint32_t) dptx->bpc;
	uint32_t v_data_per_line;
	uint32_t tu_size, tu_size_decimal;
	uint32_t tu_size_x, tu_size_y;
	uint32_t link_rate = (uint32_t) dptx->link_train.link_rate / 1000;
	uint32_t lane_count = (uint32_t) dptx->link_train.lane_count;
	uint32_t hdisplay = (uint32_t) mode->hdisplay;
	uint32_t hsync_len = (uint32_t) (mode->hsync_end - mode->hsync_start);
	uint32_t hback_porch = (uint32_t) (mode->htotal - mode->hsync_end);
	uint32_t clock = (uint32_t) mode->clock; /* in kHz */
	uint32_t hdelay;
	uint32_t normal_image;
	uint32_t v_data = hdisplay * comp_x * bpc / comp_y;
	int sram_width; /* dp : 96, edp : 64 */

	sram_width = 96;
	normal_image = 256 * sram_width / 2;
	v_data_per_line = v_data / (8 * lane_count);
	tu_size_x = 64 * bpc * comp_x * clock;
	tu_size_y = link_rate * lane_count * 8 * comp_y * 1000;
	tu_size = tu_size_x / tu_size_y;
	tu_size_decimal = tu_size_x * 10 / tu_size_y % 10;

	if (tu_size >= 63) {
		dev_err(dptx->dev, "tu_size > 64 buffer overflow\n");
		return;
	}

	hdelay = (v_data > normal_image) ? // normal
		(normal_image * comp_y / (bpc * comp_x) + hsync_len + hback_porch) * link_rate * 1000 / clock + 1 :
		// small image
		(hdisplay / 2 + hsync_len + hback_porch) * link_rate * 1000 / clock + 1;

	dev_info(dptx->dev, "[%s] v_data_per_line: %u, tu_size: %u.%u, hdelay: %u\n",
		 __func__, v_data_per_line, tu_size, tu_size_decimal, hdelay);

	dev_info(dptx->dev, "link_rate = %d, lane_count = %d, bpc = %d, color_format = %d\n",
		link_rate, lane_count, dptx->bpc, dptx->color_format);

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_V_DATA_PER_LINE0,
		DPTX14_MAC_IP_V_DATA_PER_LINE0_v_data_per_line_15_8_mask,
		DPTX14_MAC_IP_V_DATA_PER_LINE0_v_data_per_line_15_8(GET_ML_BYTE(v_data_per_line)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_V_DATA_PER_LINE1,
		DPTX14_MAC_IP_V_DATA_PER_LINE1_v_data_per_line_7_0_mask,
		DPTX14_MAC_IP_V_DATA_PER_LINE1_v_data_per_line_7_0(GET_L_BYTE(v_data_per_line)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_TU_DATA_SIZE0,
		DPTX14_MAC_IP_TU_DATA_SIZE0_tu_data_size_9_3_mask,
		DPTX14_MAC_IP_TU_DATA_SIZE0_tu_data_size_9_3(tu_size));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_TU_DATA_SIZE1,
		DPTX14_MAC_IP_TU_DATA_SIZE1_tu_data_size_2_0_mask,
		DPTX14_MAC_IP_TU_DATA_SIZE1_tu_data_size_2_0(tu_size_decimal));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_HDEALY0,
		DPTX14_MAC_IP_HDEALY0_hdelay_15_8_mask,
		DPTX14_MAC_IP_HDEALY0_hdelay_15_8(GET_ML_BYTE(hdelay)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_HDEALY1,
		DPTX14_MAC_IP_HDEALY1_hdelay_7_0_mask,
		DPTX14_MAC_IP_HDEALY1_hdelay_7_0(GET_L_BYTE(hdelay)));
}


void rtk_prince_dptx_phy_config_video_timing(struct rtk_prince_dptx *dptx,
			struct drm_display_mode *mode)
{
	uint32_t dh_den_sta, dh_den_end;
	uint32_t dv_vs_sta_field1, dv_vs_end_field1;
	uint32_t dv_den_sta_field1, dv_den_end_field1;

	uint16_t hactive, hfront_porch, hback_porch, hsync_len;
	uint16_t vactive, vfront_porch, vback_porch, vsync_len;
	uint16_t htotal, vtotal;
	bool hsp, vsp; // 0: positive, 1: negative

	htotal		 = mode->htotal;
	hactive      = mode->hdisplay;
	hfront_porch = mode->hsync_start - mode->hdisplay;
	hback_porch  = mode->htotal - mode->hsync_end;
	hsync_len    = mode->hsync_end - mode->hsync_start;
	vtotal		 = mode->vtotal;
	vactive      = mode->vdisplay;
	vfront_porch = mode->vsync_start - mode->vdisplay;
	vback_porch  = mode->vtotal - mode->vsync_end;
	vsync_len    = mode->vsync_end - mode->vsync_start;

	hsp = !((bool) (mode->flags & DRM_MODE_FLAG_PHSYNC));
	vsp = !((bool) (mode->flags & DRM_MODE_FLAG_PVSYNC));

	dh_den_sta = hsync_len + hback_porch + 1;
	dh_den_end = dh_den_sta + hactive;
	dv_den_sta_field1 = vsync_len + vback_porch + 1;
	dv_den_end_field1 = dv_den_sta_field1 + vactive;
	dv_vs_sta_field1 = 1;
	dv_vs_end_field1 = dv_vs_sta_field1 + vsync_len;

	dev_info(dptx->dev, "[%s] h: %u, hfp: %u, hbp: %u, hsync: %u, hsp: %d\n",
		 __func__, hactive, hfront_porch, hback_porch, hsync_len, hsp);
	dev_info(dptx->dev, "[%s] v: %u, vfp: %u, vbp: %u, vsync: %u, vsp: %d\n",
		 __func__, vactive, vfront_porch, vback_porch, vsync_len, vsp);
	dev_info(dptx->dev, "[%s] dv_den_sta_field1: %u, dv_den_end_field1: %u\n",
		 __func__, dv_den_sta_field1, dv_den_end_field1);
	dev_info(dptx->dev, "[%s] dv_vs_sta_field1: %u, dv_vs_end_field1: %u\n",
		 __func__, dv_vs_sta_field1, dv_vs_end_field1);

	// sst msa setting
	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_HTT_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_HTT_M_htotal_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HTT_M_htotal_15_8(GET_ML_BYTE(htotal)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_HTT_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_HTT_L_htotal_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HTT_L_htotal_7_0(GET_L_BYTE(htotal)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_HST_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_HST_M_hstart_15_8_mask, // hstart = hs_width + hs_bp
		DPTX14_MAC_IP_MN_STRM_ATTR_HST_M_hstart_15_8(GET_ML_BYTE(hsync_len + hback_porch)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_HST_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_HST_L_hstart_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HST_L_hstart_7_0(GET_L_BYTE(hsync_len + hback_porch)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_HWD_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_HWD_M_hwidth_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HWD_M_hwidth_15_8(GET_ML_BYTE(hactive)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_HWD_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_HWD_L_hwidth_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HWD_L_hwidth_7_0(GET_L_BYTE(hactive)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M_hsp_mask
		| DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M_hsw_14_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M_hsp(hsp)
		| DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M_hsw_14_8(GET_ML_BYTE(hsync_len)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_HSW_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_HSW_L_hsw_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HSW_L_hsw_7_0(GET_L_BYTE(hsync_len)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_MEAS_BYPASS,
		DPTX14_MAC_IP_DPTX_MEAS_BYPASS_measure_bypass_en_mask,
		DPTX14_MAC_IP_DPTX_MEAS_BYPASS_measure_bypass_en(1));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_M_vtotal_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_M_vtotal_15_8(GET_ML_BYTE(vtotal)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_L_vtotal_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_L_vtotal_7_0(GET_L_BYTE(vtotal)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_VST_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_VST_M_vstart_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VST_M_vstart_15_8(GET_ML_BYTE(vsync_len + vback_porch)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_VST_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_VST_L_vstart_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VST_L_vstart_7_0(GET_L_BYTE(vsync_len + vback_porch)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_VHT_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_VHT_M_vheight_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VHT_M_vheight_15_8(GET_ML_BYTE(vactive)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_VHT_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_VHT_L_vheight_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VHT_L_vheight_7_0(GET_L_BYTE(vactive)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M_vsp_mask
		| DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M_vsw_14_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M_vsp(vsp)
		| DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M_vsw_14_8(GET_ML_BYTE(vsync_len)));

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_MN_STRM_ATTR_VSW_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_VSW_L_vsw_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VSW_L_vsw_7_0(GET_L_BYTE(vsync_len)));

	rtk_prince_dptx_sst_setting(dptx, mode);
	rtk_prince_dptx_sst_dpformat_setting(dptx, mode);

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_PHY_CTRL,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_skew_en_mask
		| DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_scb_en_mask,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_skew_en(1)
		| DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_scb_en(1)); //0x15

	msleep_interruptible(1);

	// DPTX timing gen setting
	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DH_WIDTH,
		DPTX14_DH_WIDTH_dh_width_mask,
		DPTX14_DH_WIDTH_dh_width(hsync_len));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DH_TOTAL,
		DPTX14_DH_TOTAL_dh_total_mask
		| DPTX14_DH_TOTAL_dh_total_last_line_mask,
		DPTX14_DH_TOTAL_dh_total(htotal)
		| DPTX14_DH_TOTAL_dh_total_last_line(htotal));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DH_DEN_START_END,
		DPTX14_DH_DEN_START_END_dh_den_sta_mask
		| DPTX14_DH_DEN_START_END_dh_den_end_mask,
		DPTX14_DH_DEN_START_END_dh_den_sta(dh_den_sta)
		| DPTX14_DH_DEN_START_END_dh_den_end(dh_den_end));

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_ARBITER_SEC_END_CNT_HB, 0x0);
	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_ARBITER_SEC_END_CNT_LB, 0x10); //SEC_END_CNT = 0x10

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_ARBITER_DEBUG,
		DPTX14_MAC_IP_ARBITER_DEBUG_sec_end_cnt_db_mask,
		DPTX14_MAC_IP_ARBITER_DEBUG_sec_end_cnt_db(1));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DV_DEN_START_END_FIELD1,
		DPTX14_DV_DEN_START_END_FIELD1_dv_den_sta_field1_mask
		| DPTX14_DV_DEN_START_END_FIELD1_dv_den_end_field1_mask,
		DPTX14_DV_DEN_START_END_FIELD1_dv_den_sta_field1(dv_den_sta_field1)
		| DPTX14_DV_DEN_START_END_FIELD1_dv_den_end_field1(dv_den_end_field1));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DV_TOTAL,
		DPTX14_DV_TOTAL_dv_total_mask,
		DPTX14_DV_TOTAL_dv_total(vtotal));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DV_VS_START_END_FIELD1,
		DPTX14_DV_VS_START_END_FIELD1_dv_vs_sta_field1_mask
		| DPTX14_DV_VS_START_END_FIELD1_dv_vs_end_field1_mask,
		DPTX14_DV_VS_START_END_FIELD1_dv_vs_sta_field1(dv_vs_sta_field1)
		| DPTX14_DV_VS_START_END_FIELD1_dv_vs_end_field1(dv_vs_end_field1));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DH_VS_ADJ_FIELD1,
		DPTX14_DH_VS_ADJ_FIELD1_dh_vs_adj_field1_mask,
		DPTX14_DH_VS_ADJ_FIELD1_dh_vs_adj_field1(2));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_MAIN,
		DPTX14_MAIN_avg_mode_mask
		| DPTX14_MAIN_pixel_8bit_mask
		| DPTX14_MAIN_color_mode_mask
		| DPTX14_MAIN_iso_ana_b_mask
		| DPTX14_MAIN_csc_enable_mask
		| DPTX14_MAIN_dither_enable_mask,
		DPTX14_MAIN_avg_mode(1)
		| DPTX14_MAIN_pixel_8bit(1) // 0:10bit, 1:8bit
		| DPTX14_MAIN_color_mode(dptx->color_format)
		| DPTX14_MAIN_iso_ana_b(1)
		| DPTX14_MAIN_csc_enable((dptx->color_format) ? 1 : 0)
		| DPTX14_MAIN_dither_enable(0));
}

void rtk_prince_dptx_phy_set_scramble(struct rtk_prince_dptx *dptx, bool scramble)
{
	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_PHY_CTRL,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_scb_en_mask,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_scb_en(scramble));
}

void rtk_prince_dptx_phy_set_pattern(struct rtk_prince_dptx *dptx, int pattern)
{
	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_ML_PAT_SEL,
		DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_switch_pattern_auto(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(0));

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_ML_PAT_SEL,
		DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_switch_pattern_auto(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(0));

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_ML_PAT_SEL,
		DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_switch_pattern_auto(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(1));
}

void rtk_prince_dptx_phy_start_video(struct rtk_prince_dptx *dptx,
							 struct drm_display_mode *mode)
{
	rtk_prince_dptx_phy_set_scramble(dptx, true);
	rtk_prince_dptx_phy_set_pattern(dptx, RTK_PATTERN_VIDEO);

	// start DPTX video transmission
	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_ARBITER_CTRL,
		DPTX14_MAC_IP_ARBITER_CTRL_hwidth_div2_mask
		| DPTX14_MAC_IP_ARBITER_CTRL_vactive_md_mask
		| DPTX14_MAC_IP_ARBITER_CTRL_arbiter_en_mask,
		DPTX14_MAC_IP_ARBITER_CTRL_hwidth_div2(0)
		| DPTX14_MAC_IP_ARBITER_CTRL_vactive_md(1)
		| DPTX14_MAC_IP_ARBITER_CTRL_arbiter_en(1));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DV_SYNC_INTE,
		DPTX14_DV_SYNC_INTE_write_en1_mask
		| DPTX14_DV_SYNC_INTE_dv_sync_int_mask,
		DPTX14_DV_SYNC_INTE_write_en1(1)
		| DPTX14_DV_SYNC_INTE_dv_sync_int(mode->vtotal + 1));
}

void rtk_prince_dptx_phy_disable_timing_gen(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "prince dptx phy: disable timing gen\n");

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DH_WIDTH,
		DPTX14_DH_WIDTH_dh_width_mask,
		DPTX14_DH_WIDTH_dh_width(0));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DH_TOTAL,
		DPTX14_DH_TOTAL_dh_total_mask
		| DPTX14_DH_TOTAL_dh_total_last_line_mask,
		DPTX14_DH_TOTAL_dh_total(0)
		| DPTX14_DH_TOTAL_dh_total_last_line(0));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DH_DEN_START_END,
		DPTX14_DH_DEN_START_END_dh_den_sta_mask
		| DPTX14_DH_DEN_START_END_dh_den_end_mask,
		DPTX14_DH_DEN_START_END_dh_den_sta(0)
		| DPTX14_DH_DEN_START_END_dh_den_end(0));

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_ARBITER_SEC_END_CNT_HB, 0x0);
	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_ARBITER_SEC_END_CNT_LB, 0x0);

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_ARBITER_DEBUG,
		DPTX14_MAC_IP_ARBITER_DEBUG_sec_end_cnt_db_mask,
		DPTX14_MAC_IP_ARBITER_DEBUG_sec_end_cnt_db(0));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DV_DEN_START_END_FIELD1,
		DPTX14_DV_DEN_START_END_FIELD1_dv_den_sta_field1_mask
		| DPTX14_DV_DEN_START_END_FIELD1_dv_den_end_field1_mask,
		DPTX14_DV_DEN_START_END_FIELD1_dv_den_sta_field1(0)
		| DPTX14_DV_DEN_START_END_FIELD1_dv_den_end_field1(0));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DV_TOTAL,
		DPTX14_DV_TOTAL_dv_total_mask,
		DPTX14_DV_TOTAL_dv_total(0));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DV_VS_START_END_FIELD1,
		DPTX14_DV_VS_START_END_FIELD1_dv_vs_sta_field1_mask
		| DPTX14_DV_VS_START_END_FIELD1_dv_vs_end_field1_mask,
		DPTX14_DV_VS_START_END_FIELD1_dv_vs_sta_field1(0)
		| DPTX14_DV_VS_START_END_FIELD1_dv_vs_end_field1(0));

	rtk_dptx_update(dptx->dptx14_reg_base, DPTX14_DH_VS_ADJ_FIELD1,
		DPTX14_DH_VS_ADJ_FIELD1_dh_vs_adj_field1_mask,
		DPTX14_DH_VS_ADJ_FIELD1_dh_vs_adj_field1(0));
}

/**
 * e DisplayPort
 */

static void rtk_prince_dptx_set_open_loop_table(struct rtk_prince_dptx *dptx,
			struct drm_display_mode *mode)
{
	static const unsigned int open_loop_table[62][7] = {
	//{ol_div,	pll_div, pll_k, pll_l, Pixel Freq}
		{0x7, 0x0, 0x6, 0x0, 74250},
		{0x2, 0x5, 0x1, 0x0, 148500},
		{0x2, 0x1, 0x8, 0x8, 234590},
		{0x0, 0x5, 0x1, 0x0, 594000},
	};

	unsigned int pixel_clk = (unsigned int) mode->clock;
	int	i = 0;
	int ol_div = 0, pll_div = 0, pll_k = 0, pll_l = 0;

	for (i = 0 ; i < sizeof(open_loop_table) / sizeof(open_loop_table[0]) - 1 ; i++) {
		if (pixel_clk <= open_loop_table[i][4]) {
			dev_info(dptx->dev, "open loop got pixel_clk %d\n", pixel_clk);
			break;
		}
	}

	ol_div  = open_loop_table[i][0];
	pll_div = open_loop_table[i][1];
	pll_k   = open_loop_table[i][2];
	pll_l   = open_loop_table[i][3];

	rtk_dptx_write(dptx->crt_reg_base, SYS_REG_OPEN_LOOP_PLL_0,
		SYS_REG_OPEN_LOOP_PLL_0_REG_HDMI_OPEN_LOOP_CLK_GATE_DIS(1)
		| SYS_REG_OPEN_LOOP_PLL_0_REG_HDMI_CLK_TMDS_QUATER_SEL(0)
		| SYS_REG_OPEN_LOOP_PLL_0_REG_HDMI_OPEN_LOOP_SEL(0)
		| SYS_REG_OPEN_LOOP_PLL_0_REG_OL_DIV(ol_div)
		| SYS_REG_OPEN_LOOP_PLL_0_REG_CKMUX_SEL(0)
		| SYS_REG_OPEN_LOOP_PLL_0_REG_TMDS_DIV(0)
		| SYS_REG_OPEN_LOOP_PLL_0_REG_TMDS_DIV1P25(0)
		| SYS_REG_OPEN_LOOP_PLL_0_REG_PIXEL_DIV(0)
		| SYS_REG_OPEN_LOOP_PLL_0_REG_HDMI_CK_EN(0)
		| SYS_REG_OPEN_LOOP_PLL_0_PLL_SWAP_PI(0)
		| SYS_REG_OPEN_LOOP_PLL_0_PLL_CP_SEL(0)
		| SYS_REG_OPEN_LOOP_PLL_0_PLL_BYPASS_PI(0)
		| SYS_REG_OPEN_LOOP_PLL_0_PLL_POW(1)
		| SYS_REG_OPEN_LOOP_PLL_0_PLL_OEB(1)
		| SYS_REG_OPEN_LOOP_PLL_0_PLL_RSTB(0));

	rtk_dptx_write(dptx->crt_reg_base, SYS_REG_OPEN_LOOP_PLL_1,
		0x0a250000
		| SYS_REG_OPEN_LOOP_PLL_1_PLL_DIV(pll_div)
		| SYS_REG_OPEN_LOOP_PLL_1_PLL_K(pll_k)
		| SYS_REG_OPEN_LOOP_PLL_1_PLL_L(pll_l));

	rtk_dptx_write(dptx->crt_reg_base, SYS_REG_OPEN_LOOP_PLL_2,	0x00122000);
	rtk_dptx_write(dptx->crt_reg_base, SYS_REG_OPEN_LOOP_PLL_2, 0x02002001);

	rtk_dptx_update(dptx->crt_reg_base, SYS_REG_OPEN_LOOP_PLL_0,
		SYS_REG_OPEN_LOOP_PLL_0_PLL_OEB_mask,
		SYS_REG_OPEN_LOOP_PLL_0_PLL_OEB(0));
}

int rtk_prince_edptx_phy_dppll_setting(struct rtk_prince_dptx *dptx,
			struct drm_display_mode *mode)
{
	unsigned int pixel_clk = (unsigned int) mode->clock;
	int i = 0;
	int plldisp_fcode = 0, plldisp_ncode = 0;
	int disp_n = 0, disp_k = 0, tmds_div = 0;
	int plldisp_vco, plldisp_vco_ckssc, plldisp_vco_low, temp;
	int plldisp_ckssc_n, plldisp_ckssc_f;

	unsigned int plltmds_fcode = 0, plltmds_ncode = 0;
	unsigned int pll_m1 = 0, pll_m3 = 0;
	unsigned int edp_gran_est = 0;

	unsigned int p2s_ckin_sel = 0;

	for (i = 0 ; i < (sizeof(disp_dpll_table)/sizeof(disp_dpll_table[0])) - 1 ; i++) {
		if (pixel_clk <= disp_dpll_table[i][6]) {
			dev_info(dptx->dev, "disp dpll got pixel_clk %d\n", pixel_clk);
			break;
		}
	}

	plldisp_ncode = disp_dpll_table[i][0];
	plldisp_fcode = disp_dpll_table[i][1];
	disp_n        = disp_dpll_table[i][2];
	disp_k        = disp_dpll_table[i][3];
	tmds_div      = disp_dpll_table[i][5];

	plldisp_vco = 27000 * (plldisp_ncode + 3) + (27000 * plldisp_fcode) / 2048;
	plldisp_vco /= (disp_n + 1);

	plldisp_vco_ckssc = plldisp_vco * 1000 / 1001;

	plldisp_vco_low = 2 * plldisp_vco_ckssc - plldisp_vco;

	temp = 27000;
	plldisp_ckssc_n = plldisp_vco_low / temp;
	plldisp_ckssc_f = (plldisp_vco_low - plldisp_ckssc_n * temp) * 2048 / temp;

	plldisp_ckssc_n -= 3;

	switch (dptx->link_train.link_rate) {
	case DP_LINK_RATE_1_62:
		plltmds_fcode = 0;
		plltmds_ncode = 0x39;
		pll_m1 = 1;
		pll_m3 = 0;
		//plltmds_ckssc_N = 57;
		//plltmds_ckssc_F = 0;
		edp_gran_est = 0xcbc;
		p2s_ckin_sel = 0;
		break;
	case DP_LINK_RATE_2_7:
		plltmds_fcode = 0;
		plltmds_ncode = 0x61;
		pll_m1 = 1;
		pll_m3 = 0;
		//plltmds_ckssc_N = 97;
		//plltmds_ckssc_F = 0;
		edp_gran_est = 0xa9c;
		p2s_ckin_sel = 0;
		break;
	case DP_LINK_RATE_5_4:
		plltmds_fcode = 0;
		plltmds_ncode = 0x61;
		pll_m1 = 1;
		pll_m3 = 0;
		//plltmds_ckssc_N = 97;
		//plltmds_ckssc_F = 0;
		edp_gran_est = 0x1539;

		p2s_ckin_sel = 1;
		break;
	default:
		dev_err(dptx->dev, "Not support %d yet\n", dptx->link_train.link_rate);
		break;
	}

	/* HDMI_SET_PLL_OFF */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMI,
		SYS_PLL_HDMI_dptx_hdmi_sel(1)  /* select dp */
		| SYS_PLL_HDMI_REG_PRE_DRIVER(0)
		| SYS_PLL_HDMI_REG_PLLDISP_EXT_LDO_LV(0)
		| SYS_PLL_HDMI_REG_PLL_CAPSEL_VTOI(0)
		| SYS_PLL_HDMI_REG_PLL_SEL_DUAL_R(1)
		| SYS_PLL_HDMI_REG_PLL_KVCO_RES(0)
		| SYS_PLL_HDMI_REG_PLL_VSET_SEL(0)
		| SYS_PLL_HDMI_REG_PLL_LDO_POW(0)
		| SYS_PLL_HDMI_REG_PLL_BPS_M3(0)
		| SYS_PLL_HDMI_REG_P2S_CKIN_SEL(0)
		| SYS_PLL_HDMI_REG_P2S_DIV2(0)
		| SYS_PLL_HDMI_PLLDISP_OEB(1)
		| SYS_PLL_HDMI_PLLDISP_VCORSTB(0)
		| SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN(1)
		| SYS_PLL_HDMI_REG_PLLDISP_RSTB(0)
		| SYS_PLL_HDMI_REG_PLLDISP_POW(0)
		| SYS_PLL_HDMI_REG_TMDS_POW(0)
		| SYS_PLL_HDMI_REG_PLL_RSTB(0)
		| SYS_PLL_HDMI_REG_PLL_POW(0)
		| SYS_PLL_HDMI_REG_HDMI_CK_EN(0));

	/* set_SYS_CLOCK_ENABLE1_reg(SYS_CLOCK_ENABLE1_write_en8(1)|SYS_CLOCK_ENABLE1_clk_en_hdmi(0)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_CLOCK_ENABLE1,
		SYS_CLOCK_ENABLE1_write_en8_mask
		| SYS_CLOCK_ENABLE1_clk_en_hdmi_mask,
		SYS_CLOCK_ENABLE1_write_en8(1)
		| SYS_CLOCK_ENABLE1_clk_en_hdmi(0));

	/* set_SYS_SOFT_RESET3_reg(SYS_SOFT_RESET3_write_en15(1)|SYS_SOFT_RESET3_rstn_hdmi(0)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_SOFT_RESET3,
		SYS_SOFT_RESET3_write_en15_mask
		| SYS_SOFT_RESET3_rstn_hdmi_mask,
		SYS_SOFT_RESET3_write_en15(1)
		| SYS_SOFT_RESET3_rstn_hdmi(0));


	/* set_SYS_CLOCK_ENABLE7_reg(SYS_CLOCK_ENABLE7_write_en11(1)|SYS_CLOCK_ENABLE7_clk_en_hdmitop(0)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_SOFT_RESET3,
		SYS_CLOCK_ENABLE7_write_en11_mask
		| SYS_CLOCK_ENABLE7_clk_en_hdmitop_mask,
		SYS_CLOCK_ENABLE7_write_en11(1)
		| SYS_CLOCK_ENABLE7_clk_en_hdmitop(0));

	/* set_SYS_SOFT_RESET4_reg(SYS_SOFT_RESET4_write_en16(1)|SYS_SOFT_RESET4_rstn_hdmitop(0)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_SOFT_RESET4,
		SYS_SOFT_RESET4_write_en16_mask
		| SYS_SOFT_RESET4_rstn_hdmitop_mask,
		SYS_SOFT_RESET4_write_en16(1)
		| SYS_SOFT_RESET4_rstn_hdmitop(0));

	/* ISO_SYS_POWER_CTRL */
	/* writel(0x007f770b, ioremap(0x98129300, 0x1)); */
	/* bit[2] = 0, iso_disp */
	rtk_dptx_update(dptx->iso_sys_base, POWER_CTRL1, ISO_DISP, 0);

	//TMDS PLL setting for link rate 2.7G
	/* SYS_PLL_HDMI */
	/* writel(0x1202a2db, ioremap(0x98000190, 0x1)); */
	/* 5.4G 0x1202a6db, 2.7G 0x1202a2db */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMI,
		SYS_PLL_HDMI_REG_PRE_DRIVER_mask
		| SYS_PLL_HDMI_REG_PLL_KVCO_RES_mask
		| SYS_PLL_HDMI_REG_PLL_LDO_POW_mask
		| SYS_PLL_HDMI_REG_P2S_CKIN_SEL_mask
		| SYS_PLL_HDMI_REG_P2S_DIV2_mask
		| SYS_PLL_HDMI_PLLDISP_OEB_mask
		| SYS_PLL_HDMI_PLLDISP_VCORSTB_mask
		| SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN_mask
		| SYS_PLL_HDMI_REG_PLLDISP_POW_mask
		| SYS_PLL_HDMI_REG_TMDS_POW_mask
		| SYS_PLL_HDMI_REG_PLL_POW_mask
		| SYS_PLL_HDMI_REG_HDMI_CK_EN_mask,
		SYS_PLL_HDMI_REG_PRE_DRIVER(1)
		| SYS_PLL_HDMI_REG_PLL_KVCO_RES(1)
		| SYS_PLL_HDMI_REG_PLL_LDO_POW(1)
		| SYS_PLL_HDMI_REG_P2S_CKIN_SEL(p2s_ckin_sel)
		| SYS_PLL_HDMI_REG_P2S_DIV2(1)
		| SYS_PLL_HDMI_PLLDISP_OEB(0)
		| SYS_PLL_HDMI_PLLDISP_VCORSTB(1)
		| SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN(1)
		| SYS_PLL_HDMI_REG_PLLDISP_POW(1)
		| SYS_PLL_HDMI_REG_TMDS_POW(1)
		| SYS_PLL_HDMI_REG_PLL_POW(1)
		| SYS_PLL_HDMI_REG_HDMI_CK_EN(1));

	/* SYS_PLL_HDMI3 */
	writel(0x00900000, ioremap(0x98000198, 0x1));

	/* HDMI PLL clk setting */
	writel(0x00000110, ioremap(0x98000024, 0x1));

	/* set PLL_HDMI_SD1 */
	/* writel(0x936FF03E, ioremap(0x98000204, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMI_SD1,
		SYS_PLL_HDMI_SD1_time_rdy_ckout(0x2)
		| SYS_PLL_HDMI_SD1_time2_rst_width(0x1)
		| SYS_PLL_HDMI_SD1_pcr_rst_n(0)
		| SYS_PLL_HDMI_SD1_time0_ck(0x3)
		| SYS_PLL_HDMI_SD1_f390k(0x1)
		| SYS_PLL_HDMI_SD1_pll_en(0x1)
		| SYS_PLL_HDMI_SD1_en_wdog(0)
		| SYS_PLL_HDMI_SD1_ssc_ckinv(1)
		| SYS_PLL_HDMI_SD1_fcode(plldisp_fcode)
		| SYS_PLL_HDMI_SD1_ncode(plldisp_ncode));

	/* PLL_HDMI_SD2 */
	writel(0x50201ffe, ioremap(0x98000208, 0x1));

	/* PLL_HDMI_SD4 */
	/* writel(0x0004fc12, ioremap(0x9800021c, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMI_SD4,
		SYS_PLL_HDMI_SD4_ssc_en(0)
		| SYS_PLL_HDMI_SD4_fcode_ssc(plldisp_ckssc_f)
		| SYS_PLL_HDMI_SD4_ncode_ssc(plldisp_ckssc_n));

	/* PLL_HDMI_SD5 */
	/* writel(0x00c00100, ioremap(0x98000220, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMI_SD5,
		SYS_PLL_HDMI_SD5_RSTB_HDMITX(1)
		| SYS_PLL_HDMI_SD5_gran_auto_rst(1)
		| SYS_PLL_HDMI_SD5_dot_gran(0)
		| SYS_PLL_HDMI_SD5_gran_est(256));

	/* PLL_HDMI_SD2 */
	/* writel(0xd0201fff, ioremap(0x98000208, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMI_SD2,
		SYS_PLL_HDMI_SD2_bypass_pi(1)
		| SYS_PLL_HDMI_SD2_en_pi_debug(1)
		| SYS_PLL_HDMI_SD2_hs_oc_stop_diff(1)
		| SYS_PLL_HDMI_SD2_sel_oc_mode(0)
		| SYS_PLL_HDMI_SD2_oc_done_delay(32)
		| SYS_PLL_HDMI_SD2_pi_cur_sel(1)
		| SYS_PLL_HDMI_SD2_oc_step_set(1023)
		| SYS_PLL_HDMI_SD2_sdm_order(1)
		| SYS_PLL_HDMI_SD2_oc_en(1));

	/* SYS_PLL_HDMI2 */
	/* writel(0x00340101, ioremap(0x98000194, 0x1)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMI2,
		SYS_PLL_HDMI2_REG_PLL_M3_mask
		| SYS_PLL_HDMI2_REG_TMDS_DIV_mask
		| SYS_PLL_HDMI2_REG_PLLDISP_N_mask
		| SYS_PLL_HDMI2_REG_PLLDISP_K_mask
		| SYS_PLL_HDMI2_REG_PIXEL_DIV_mask
		| SYS_PLL_HDMI2_REG_PLL_M2_mask
		| SYS_PLL_HDMI2_REG_PLL_M1_mask,
		SYS_PLL_HDMI2_REG_PLL_M3(pll_m3)
		| SYS_PLL_HDMI2_REG_TMDS_DIV(tmds_div)
		| SYS_PLL_HDMI2_REG_PLLDISP_N(disp_n)
		| SYS_PLL_HDMI2_REG_PLLDISP_K(disp_k)
		| SYS_PLL_HDMI2_REG_PIXEL_DIV(2)
		| SYS_PLL_HDMI2_REG_PLL_M2(0)
		| SYS_PLL_HDMI2_REG_PLL_M1(pll_m1));

	/* PLL_HDMI_LDO1 */
	writel(0x201c3188, ioremap(0x98000230, 0x1));

	/* PLL_HDMI_LDO2 */
	writel(0x2eec00f0, ioremap(0x98000234, 0x1));

	/* PLL_HDMI_LDO3 */
	/* TODO: 0x6fffff00 */
	writel(0x6739ce00, ioremap(0x98000238, 0x1));

	/* PLL_HDMI_LDO4 */
	if (dptx->link_train.link_rate == DP_LINK_RATE_5_4)
		writel(0x00310842, ioremap(0x9800023c, 0x1));
	else
		writel(0x0036b5ad, ioremap(0x9800023c, 0x1));

	/* PLL_HDMI_LDO6 */
	writel(0x14540001, ioremap(0x98000244, 0x1));
	/* PLL_HDMI_LDO7 */
	writel(0x04222080, ioremap(0x98000248, 0x1));
	/* PLL_HDMI_LDO8 */
	writel(0x0aaaacb2, ioremap(0x9800024c, 0x1));
	/* PLL_HDMI_LDO10 */
	writel(0x00cb2cb2, ioremap(0x9800025c, 0x1));
	/* PLL_HDMI_LDO11 */
	writel(0x00000000, ioremap(0x9800026c, 0x1));
	/* PLL_HDMI_LDO9 */
	writel(0x0b000000, ioremap(0x98000268, 0x1));

	/* PLL_HDMITX2P1_SD1 */
	/* writel(0x93680061, ioremap(0x98000654, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD1,
		SYS_PLL_HDMITX2P1_SD1_time_rdy_ckout(0x2)
		| SYS_PLL_HDMITX2P1_SD1_time2_rst_width(0x1)
		| SYS_PLL_HDMITX2P1_SD1_pcr_rst_n(0)
		| SYS_PLL_HDMITX2P1_SD1_time0_ck(0x3)
		| SYS_PLL_HDMITX2P1_SD1_f390k(0x1)
		| SYS_PLL_HDMITX2P1_SD1_pll_en(1)
		| SYS_PLL_HDMITX2P1_SD1_en_wdog(0)
		| SYS_PLL_HDMITX2P1_SD1_ssc_ckinv(1)
		| SYS_PLL_HDMITX2P1_SD1_fcode(plltmds_fcode)
		| SYS_PLL_HDMITX2P1_SD1_ncode(plltmds_ncode));

	/* PLL_HDMITX2P1_SD2 */
	/* writel(0x50203ffe, ioremap(0x98000658, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD2,
		SYS_PLL_HDMITX2P1_SD2_bypass_pi(0)
		| SYS_PLL_HDMITX2P1_SD2_en_pi_debug(1)
		| SYS_PLL_HDMITX2P1_SD2_hs_oc_stop_diff(1)
		| SYS_PLL_HDMITX2P1_SD2_sel_oc_mode(0)
		| SYS_PLL_HDMITX2P1_SD2_oc_done_delay(0x20)
		| SYS_PLL_HDMITX2P1_SD2_pi_cur_sel(3)
		| SYS_PLL_HDMITX2P1_SD2_oc_step_set(0x3ff)
		| SYS_PLL_HDMITX2P1_SD2_sdm_order(1)
		| SYS_PLL_HDMITX2P1_SD2_oc_en(0));

	/* PLL_HDMITX2P1_SD4 */
	writel(0x00066660, ioremap(0x98000660, 0x1));

	/* set PLL_HDMITX2P1_SD5 */
	/* writel(0x00800100, ioremap(0x98000664, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD5,
		SYS_PLL_HDMITX2P1_SD5_RSTB_HDMITX(1)
		| SYS_PLL_HDMITX2P1_SD5_gran_auto_rst(0)
		| SYS_PLL_HDMITX2P1_SD5_dot_gran(0)
		| SYS_PLL_HDMITX2P1_SD5_gran_est(256));

	/* PLL_HDMITX2P1_SD2 */
	/* writel(0x50203fff, ioremap(0x98000658, 0x1)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD2,
		SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
		SYS_PLL_HDMITX2P1_SD2_oc_en(1));

	/* PLL_HDMITX2P1_SD6 */
	writel(0x00080000, ioremap(0x98000668, 0x1));

	/* SYS_PLL_HDMI */
	/* writel(0x1202a2fb, ioremap(0x98000190, 0x1)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMI,
		SYS_PLL_HDMI_REG_PLLDISP_RSTB_mask,
		SYS_PLL_HDMI_REG_PLLDISP_RSTB(1));

	/* SYS_PLL_HDMI3 */
	writel(0x00900000, ioremap(0x98000198, 0x1));

	/* SYS_PLL_HDMI */
	/* writel(0x1202a2ff, ioremap(0x98000190, 0x1)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMI,
		SYS_PLL_HDMI_REG_PLL_RSTB_mask,
		SYS_PLL_HDMI_REG_PLL_RSTB(1));

	/* SYS_PLL_HDMI3 */
	writel(0x00900000, ioremap(0x98000198, 0x1));
	/* PLL_HDMI_LDO6 */
	writel(0x34540001, ioremap(0x98000244, 0x1));
	/* PLL_HDMITX2P1_SD6 */
	writel(0x00000000, ioremap(0x98000668, 0x1));

	/* PLL_HDMITX2P1_SD2 */
	/* writel(0x50203ffe, ioremap(0x98000658, 0x1)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD2,
		SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
		SYS_PLL_HDMITX2P1_SD2_oc_en(0));

	/* PLL_HDMITX2P1_SD4 */
	writel(0x0003c260, ioremap(0x98000660, 0x1));

	/* PLL_HDMITX2P1_SD5 */
	/* writel(0x00e01539, ioremap(0x98000664, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD5,
		SYS_PLL_HDMITX2P1_SD5_RSTB_HDMITX(1)
		| SYS_PLL_HDMITX2P1_SD5_gran_auto_rst(0)
		| SYS_PLL_HDMITX2P1_SD5_dot_gran(0x4)
		| SYS_PLL_HDMITX2P1_SD5_gran_est(edp_gran_est)
		| SYS_PLL_HDMITX2P1_SD5_gran_auto_rst(1));

	/* PLL_HDMITX2P1_SD2 */
	/* writel(0x50203fff, ioremap(0x98000658, 0x1)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD2,
		SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
		SYS_PLL_HDMITX2P1_SD2_oc_en(1));

	/* PLL_HDMITX2P1_SD6 */
	/* writel(0x00080000, ioremap(0x98000668, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD6,
		SYS_PLL_HDMITX2P1_SD6_REG_BYPASS_DIVN(1)
		| SYS_PLL_HDMITX2P1_SD6_FCW_SSC_DEFAULT_HDMITX(0));

	/* PLL_HDMITX2P1_SD6 */
	/* writel(0x00000000, ioremap(0x98000668, 0x1)); */
	rtk_dptx_write(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD6,
	   SYS_PLL_HDMITX2P1_SD6_REG_BYPASS_DIVN(0)
	   | SYS_PLL_HDMITX2P1_SD6_FCW_SSC_DEFAULT_HDMITX(0));

	/* PLL_HDMITX2P1_SD2 */
	/* writel(0x50203fff, ioremap(0x98000658, 0x1)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD2,
		SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
		SYS_PLL_HDMITX2P1_SD2_oc_en(1));

	/* PLL_HDMITX2P1_SD4 */
	writel(0x0003c260, ioremap(0x98000660, 0x1));

	/* wait OC DONE */
	usleep_range(10000, 20000);

	/* PLL_HDMITX2P1_SD1 */
	/* disable PLL */
	writel(0x93480061, ioremap(0x98000654, 0x1));

	/* set PLL_HDMITX2P1_SD2 */
	/* disable OC */
	/* writel(0x50203ffe, ioremap(0x98000658, 0x1)); */
	rtk_dptx_update(dptx->crt_reg_base, SYS_PLL_HDMITX2P1_SD2,
		SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
		SYS_PLL_HDMITX2P1_SD2_oc_en(0));

	/* PLL_HDMITX2P1_SD4 */
	/* enable SSC */
	writel(0x000bc260, ioremap(0x98000660, 0x1));

	rtk_prince_dptx_set_open_loop_table(dptx, mode);

	/* VODMA PLL setting */
	writel(0x00000005, ioremap(0x98000264, 0x1));
	writel(0x00000007, ioremap(0x98000264, 0x1));
	writel(0x000152a0, ioremap(0x98000260, 0x1));
	writel(0x00000003, ioremap(0x98000264, 0x1));

	return 0;
}

static void rtk_prince_edp_sst_setting(struct rtk_prince_dptx *dptx,
							struct drm_display_mode *mode)
{
	uint64_t mvid =  0;
	uint32_t nvid = 32768;
	int bpc = (dptx->bpc == 6) ? RTK_DP_COLORBIT_6 :
			 (dptx->bpc == 8) ? RTK_DP_COLORBIT_8 :
			 (dptx->bpc == 10) ? RTK_DP_COLORBIT_10 :
			 (dptx->bpc == 12) ? RTK_DP_COLORBIT_12 :
			 (dptx->bpc == 16) ? RTK_DP_COLORBIT_16 : RTK_DP_COLORBIT_8;
	int component_format = (dptx->color_format == RTK_COLOR_FORMAT_RGB) ? 0x0 :
			 (dptx->color_format == RTK_COLOR_FORMAT_YUV444) ? 0x2 :
			 (dptx->color_format == RTK_COLOR_FORMAT_YUV422) ? 0x1 : 0x0;
	uint32_t link_rate = dptx->link_train.link_rate;

	mvid = div_u64(mul_u32_u32(mode->clock, nvid), link_rate);

	dev_info(dptx->dev, "[%s] MVID: %llu\n", __func__, mvid);
	dev_info(dptx->dev, "link_rate = %d, bpc = %d, color_format = %d\n",
		link_rate, dptx->bpc, dptx->color_format);

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_M_VID_H,
		DP_IP_MN_M_VID_H_mvid_23_16_mask,
		DP_IP_MN_M_VID_H_mvid_23_16(GET_MH_BYTE(mvid)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_M_VID_M,
		DP_IP_MN_M_VID_M_mvid_15_8_mask,
		DP_IP_MN_M_VID_M_mvid_15_8(GET_ML_BYTE(mvid)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_M_VID_L,
		DP_IP_MN_M_VID_L_mvid_7_0_mask,
		DP_IP_MN_M_VID_L_mvid_7_0(GET_L_BYTE(mvid)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_N_VID_H,
		DP_IP_MN_N_VID_H_nvid_23_16_mask,
		DP_IP_MN_N_VID_H_nvid_23_16(GET_MH_BYTE(nvid)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_N_VID_M,
		DP_IP_MN_N_VID_M_nvid_15_8_mask,
		DP_IP_MN_N_VID_M_nvid_15_8(GET_ML_BYTE(nvid)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_N_VID_L,
		DP_IP_MN_N_VID_L_nvid_7_0_mask,
		DP_IP_MN_N_VID_L_nvid_7_0(GET_L_BYTE(nvid)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_VID_AUTO_EN_1,
		DP_IP_MN_VID_AUTO_EN_1_mn_vid_auto_en_mask
		| DP_IP_MN_VID_AUTO_EN_1_mn_vid_db_mask,
		DP_IP_MN_VID_AUTO_EN_1_mn_vid_auto_en(0)
		| DP_IP_MN_VID_AUTO_EN_1_mn_vid_db(0x1)); // 0x40

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MSA_MISC0,
		DP_IP_MSA_MISC0_colorbit_mask
		| DP_IP_MSA_MISC0_ycc_col_mask
		| DP_IP_MSA_MISC0_dyn_range_mask
		| DP_IP_MSA_MISC0_component_format_mask,
		DP_IP_MSA_MISC0_colorbit(bpc)
		| DP_IP_MSA_MISC0_ycc_col(0x0) // ITU-R BT601-5
		| DP_IP_MSA_MISC0_dyn_range(0x0) // VESA
		| DP_IP_MSA_MISC0_component_format(component_format));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MSA_CTRL,
		DP_IP_MSA_CTRL_msa_db_mask,
		DP_IP_MSA_CTRL_msa_db(1));
}

static void rtk_prince_edp_sst_dpformat_setting(struct rtk_prince_dptx *dptx,
							struct drm_display_mode *mode)
{
	/* RGB,YUV444: 3, YUV422: 2, YUV420: 1.5*/
	uint32_t comp_x = (dptx->color_format == RTK_COLOR_FORMAT_YUV422) ? 2 : 3;
	uint32_t comp_y = (dptx->color_format == RTK_COLOR_FORMAT_YUV420) ? 2 : 1;
	uint32_t bpc = (uint32_t) dptx->bpc;
	uint32_t v_data_per_line;
	uint32_t tu_size, tu_size_decimal;
	uint32_t tu_size_x, tu_size_y;
	uint32_t link_rate = (uint32_t) dptx->link_train.link_rate / 1000;
	uint32_t lane_count = (uint32_t) dptx->link_train.lane_count;
	uint32_t hdisplay = (uint32_t) mode->hdisplay;
	uint32_t hsync_len = (uint32_t) (mode->hsync_end - mode->hsync_start);
	uint32_t hback_porch = (uint32_t) (mode->htotal - mode->hsync_end);
	uint32_t clock = (uint32_t) mode->clock; /* in kHz */
	uint32_t hdelay;
	uint32_t normal_image;
	uint32_t v_data = hdisplay * comp_x * bpc / comp_y;
	int sram_width; /* dp : 96, edp : 64 */

	sram_width = 64;
	normal_image = 256 * sram_width / 2;
	v_data_per_line = v_data / (8 * lane_count);
	tu_size_x = 64 * bpc * comp_x * clock;
	tu_size_y = link_rate * lane_count * 8 * comp_y * 1000;
	tu_size = tu_size_x / tu_size_y;
	tu_size_decimal = tu_size_x * 10 / tu_size_y % 10;

	if (tu_size >= 63) {
		dev_err(dptx->dev, "tu_size > 64 buffer overflow\n");
		return;
	}

	hdelay = (v_data > normal_image) ? // normal
		(normal_image * comp_y / (bpc * comp_x) + hsync_len + hback_porch) * link_rate * 1000 / clock + 1 :
		// small image
		(hdisplay / 2 + hsync_len + hback_porch) * link_rate * 1000 / clock + 1;

	dev_info(dptx->dev, "[%s] v_data_per_line: %u, tu_size: %u.%u, hdelay: %u\n",
		 __func__, v_data_per_line, tu_size, tu_size_decimal, hdelay);

	dev_info(dptx->dev, "link_rate = %d, lane_count = %d, bpc = %d, color_format = %d\n",
		link_rate, lane_count, dptx->bpc, dptx->color_format);

	// eDPTX video setting
	rtk_dptx_update(dptx->dptx14_edp_reg_base, V_DATA_PER_LINE0,
		DP_IP_V_DATA_PER_LINE0_v_data_per_line_14_8_mask,
		DP_IP_V_DATA_PER_LINE0_v_data_per_line_14_8(GET_ML_BYTE(v_data_per_line)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, V_DATA_PER_LINE1,
		DP_IP_V_DATA_PER_LINE1_v_data_per_line_7_0_mask,
		DP_IP_V_DATA_PER_LINE1_v_data_per_line_7_0(GET_L_BYTE(v_data_per_line)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, TU_DATA_SIZE0,
		DP_IP_TU_DATA_SIZE0_tu_data_size_9_3_mask,
		DP_IP_TU_DATA_SIZE0_tu_data_size_9_3(tu_size));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, TU_DATA_SIZE1,
		DP_IP_TU_DATA_SIZE1_tu_data_size_2_0_mask,
		DP_IP_TU_DATA_SIZE1_tu_data_size_2_0(tu_size_decimal));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, HDEALY0,
		DP_IP_HDEALY0_hdelay_14_8_mask,
		DP_IP_HDEALY0_hdelay_14_8(GET_ML_BYTE(hdelay)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, HDEALY1,
		DP_IP_HDEALY1_hdelay_7_0_mask,
		DP_IP_HDEALY1_hdelay_7_0(GET_L_BYTE(hdelay)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, LFIFO_WL_SET,
		DP_IP_LFIFO_WL_SET_wl_mid_regen_mask
		| DP_IP_LFIFO_WL_SET_wl_mid_set_mask,
		DP_IP_LFIFO_WL_SET_wl_mid_regen(1)
		| DP_IP_LFIFO_WL_SET_wl_mid_set(0x40));
}

static void rtk_prince_edp_config_lane(struct rtk_prince_dptx *dptx)
{
	uint32_t ctrl_lane_num = 1;
	uint32_t v2analog = 0;

	dev_info(dptx->dev, "edp config lane = %d\n", dptx->link_train.lane_count);

	switch (dptx->link_train.lane_count) {
	case 1:
		ctrl_lane_num = 1;
		v2analog = 0;
		break;
	case 2:
		ctrl_lane_num = 2;
		v2analog = 3;
		break;
	case 4:
		ctrl_lane_num = 3;
		v2analog = 0xf;
		break;
	}

	rtk_dptx_update(dptx->dptx14_edp_reg_base, DP_PHY_CTRL,
		DP_IP_DP_PHY_CTRL_v2analog_mask
		| DP_IP_DP_PHY_CTRL_lane_num_mask
		| DP_IP_DP_PHY_CTRL_mst_en_mask,
		DP_IP_DP_PHY_CTRL_v2analog(v2analog)
		| DP_IP_DP_PHY_CTRL_lane_num(ctrl_lane_num)
		| DP_IP_DP_PHY_CTRL_mst_en(0x0));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, DP_MAC_CTRL,
		DP_IP_DP_MAC_CTRL_enhance_md_mask
		| DP_IP_DP_MAC_CTRL_lane_num_mask,
		DP_IP_DP_MAC_CTRL_enhance_md(1)
		| DP_IP_DP_MAC_CTRL_lane_num(ctrl_lane_num));
}

static void rtk_prince_edp_config_timing_gen(struct rtk_prince_dptx *dptx,
								 struct drm_display_mode *mode)
{
	uint16_t hactive, hback_porch, hsync_len;
	uint16_t vactive, vback_porch, vsync_len;
	uint16_t htotal, vtotal;
	uint32_t dh_den_sta, dh_den_end;
	uint32_t dv_vs_sta_field1, dv_vs_end_field1;
	uint32_t dv_den_sta_field1, dv_den_end_field1;

	htotal		 = mode->htotal;
	hactive      = mode->hdisplay;
	hback_porch  = mode->htotal - mode->hsync_end;
	hsync_len    = mode->hsync_end - mode->hsync_start;
	vtotal		 = mode->vtotal;
	vactive      = mode->vdisplay;
	vback_porch  = mode->vtotal - mode->vsync_end;
	vsync_len    = mode->vsync_end - mode->vsync_start;

	dh_den_sta = hsync_len + hback_porch + 1;
	dh_den_end = dh_den_sta + hactive;
	dv_den_sta_field1 = vsync_len + vback_porch + 1;
	dv_den_end_field1 = dv_den_sta_field1 + vactive;
	dv_vs_sta_field1 = 1;
	dv_vs_end_field1 = dv_vs_sta_field1 + vsync_len;

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DH_WIDTH,
		EDPTX_DH_WIDTH_dh_width_mask,
		EDPTX_DH_WIDTH_dh_width(hsync_len));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DH_TOTAL,
		EDPTX_DH_TOTAL_dh_total_mask
		| EDPTX_DH_TOTAL_dh_total_last_line_mask,
		EDPTX_DH_TOTAL_dh_total(htotal)
		| EDPTX_DH_TOTAL_dh_total_last_line(htotal));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DH_DEN_START_END,
		EDPTX_DH_DEN_START_END_dh_den_sta_mask
		| EDPTX_DH_DEN_START_END_dh_den_end_mask,
		EDPTX_DH_DEN_START_END_dh_den_sta(dh_den_sta)
		| EDPTX_DH_DEN_START_END_dh_den_end(dh_den_end));

	rtk_dptx_write(dptx->dptx14_edp_reg_base, ARBITER_SEC_END_CNT_HB, 0x0);

	rtk_dptx_write(dptx->dptx14_edp_reg_base, ARBITER_SEC_END_CNT_LB, 0x10); // SEC_END_CNT = 0x10

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DV_DEN_START_END_FIELD1,
		EDPTX_DV_DEN_START_END_FIELD1_dv_den_sta_field1_mask
		| EDPTX_DV_DEN_START_END_FIELD1_dv_den_end_field1_mask,
		EDPTX_DV_DEN_START_END_FIELD1_dv_den_sta_field1(dv_den_sta_field1)
		| EDPTX_DV_DEN_START_END_FIELD1_dv_den_end_field1(dv_den_end_field1));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DV_TOTAL,
		EDPTX_DV_TOTAL_dv_total_mask,
		EDPTX_DV_TOTAL_dv_total(vtotal));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DV_VS_START_END_FIELD1,
		EDPTX_DV_VS_START_END_FIELD1_dv_vs_sta_field1_mask
		| EDPTX_DV_VS_START_END_FIELD1_dv_vs_end_field1_mask,
		EDPTX_DV_VS_START_END_FIELD1_dv_vs_sta_field1(dv_vs_sta_field1)
		| EDPTX_DV_VS_START_END_FIELD1_dv_vs_end_field1(dv_vs_end_field1));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DH_VS_ADJ_FIELD1,
		EDPTX_DH_VS_ADJ_FIELD1_dh_vs_adj_field1_mask,
		EDPTX_DH_VS_ADJ_FIELD1_dh_vs_adj_field1(2));
}

void rtk_prince_edp_phy_config_video_timing(struct rtk_prince_dptx *dptx,
								struct drm_display_mode *mode)
{
	uint16_t hactive, hfront_porch, hback_porch, hsync_len;
	uint16_t vactive, vfront_porch, vback_porch, vsync_len;
	uint16_t htotal, vtotal;
	bool hsp, vsp; /* 0: positive, 1: negative */

	htotal		 = mode->htotal;
	hactive      = mode->hdisplay;
	hfront_porch = mode->hsync_start - mode->hdisplay;
	hback_porch  = mode->htotal - mode->hsync_end;
	hsync_len    = mode->hsync_end - mode->hsync_start;
	vtotal		 = mode->vtotal;
	vactive      = mode->vdisplay;
	vfront_porch = mode->vsync_start - mode->vdisplay;
	vback_porch  = mode->vtotal - mode->vsync_end;
	vsync_len    = mode->vsync_end - mode->vsync_start;
	hsp = !((bool) (mode->flags & DRM_MODE_FLAG_PHSYNC));
	vsp = !((bool) (mode->flags & DRM_MODE_FLAG_PVSYNC));

	dev_info(dptx->dev, "[%s] h: %u, hss: %u, hse: %u, htt: %u\n",
		 __func__, mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal);
	dev_info(dptx->dev, "[%s] v: %u, vss: %u, vse: %u, vtt: %u\n",
		 __func__, mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal);

	dev_info(dptx->dev, "[%s] mode->flags: %u, mode->clock: %u\n",
		 __func__, mode->flags, mode->clock);

	dev_info(dptx->dev, "[%s] h: %u, hfp: %u, hbp: %u, hsync: %u, hsp: %d\n",
		 __func__, hactive, hfront_porch, hback_porch, hsync_len, hsp);
	dev_info(dptx->dev, "[%s] v: %u, vfp: %u, vbp: %u, vsync: %u, vsp: %d\n",
		 __func__, vactive, vfront_porch, vback_porch, vsync_len, vsp);

	// sst msa setting
	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_HTT_M,
		DP_IP_MN_STRM_ATTR_HTT_M_htotal_15_8_mask,
		DP_IP_MN_STRM_ATTR_HTT_M_htotal_15_8(GET_ML_BYTE(htotal)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_HTT_L,
		DP_IP_MN_STRM_ATTR_HTT_L_htotal_7_0_mask,
		DP_IP_MN_STRM_ATTR_HTT_L_htotal_7_0(GET_L_BYTE(htotal)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_HST_M,
		DP_IP_MN_STRM_ATTR_HST_M_hstart_15_8_mask, // hstart = hs_width + hs_bp
		DP_IP_MN_STRM_ATTR_HST_M_hstart_15_8(GET_ML_BYTE(hsync_len + hback_porch)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_HST_L,
		DP_IP_MN_STRM_ATTR_HST_L_hstart_7_0_mask,
		DP_IP_MN_STRM_ATTR_HST_L_hstart_7_0(GET_L_BYTE(hsync_len + hback_porch)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_HWD_M,
		DP_IP_MN_STRM_ATTR_HWD_M_hwidth_15_8_mask,
		DP_IP_MN_STRM_ATTR_HWD_M_hwidth_15_8(GET_ML_BYTE(hactive)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_HWD_L,
		DP_IP_MN_STRM_ATTR_HWD_L_hwidth_7_0_mask,
		DP_IP_MN_STRM_ATTR_HWD_L_hwidth_7_0(GET_L_BYTE(hactive)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_HSW_M,
		DP_IP_MN_STRM_ATTR_HSW_M_hsp_mask
		| DP_IP_MN_STRM_ATTR_HSW_M_hsw_14_8_mask,
		DP_IP_MN_STRM_ATTR_HSW_M_hsp(hsp)
		| DP_IP_MN_STRM_ATTR_HSW_M_hsw_14_8(GET_ML_BYTE(hsync_len)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_HSW_L,
		DP_IP_MN_STRM_ATTR_HSW_L_hsw_7_0_mask,
		DP_IP_MN_STRM_ATTR_HSW_L_hsw_7_0(GET_L_BYTE(hsync_len)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_VTTE_M,
		DP_IP_MN_STRM_ATTR_VTTE_M_vtotal_15_8_mask,
		DP_IP_MN_STRM_ATTR_VTTE_M_vtotal_15_8(GET_ML_BYTE(vtotal)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_VTTE_L,
		DP_IP_MN_STRM_ATTR_VTTE_L_vtotal_7_0_mask,
		DP_IP_MN_STRM_ATTR_VTTE_L_vtotal_7_0(GET_L_BYTE(vtotal)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_VST_M,
		DP_IP_MN_STRM_ATTR_VST_M_vstart_15_8_mask,
		DP_IP_MN_STRM_ATTR_VST_M_vstart_15_8(GET_ML_BYTE(vsync_len + vback_porch)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_VST_L,
		DP_IP_MN_STRM_ATTR_VST_L_vstart_7_0_mask,
		DP_IP_MN_STRM_ATTR_VST_L_vstart_7_0(GET_L_BYTE(vsync_len + vback_porch)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_VHT_M,
		DP_IP_MN_STRM_ATTR_VHT_M_vheight_15_8_mask,
		DP_IP_MN_STRM_ATTR_VHT_M_vheight_15_8(GET_ML_BYTE(vactive)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_VHT_L,
		DP_IP_MN_STRM_ATTR_VHT_L_vheight_7_0_mask,
		DP_IP_MN_STRM_ATTR_VHT_L_vheight_7_0(GET_L_BYTE(vactive)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_VSW_M,
		DP_IP_MN_STRM_ATTR_VSW_M_vsp_mask
		| DP_IP_MN_STRM_ATTR_VSW_M_vsw_14_8_mask,
		DP_IP_MN_STRM_ATTR_VSW_M_vsp(vsp)
		| DP_IP_MN_STRM_ATTR_VSW_M_vsw_14_8(GET_ML_BYTE(vsync_len)));

	rtk_dptx_update(dptx->dptx14_edp_reg_base, MN_STRM_ATTR_VSW_L,
		DP_IP_MN_STRM_ATTR_VSW_L_vsw_7_0_mask,
		DP_IP_MN_STRM_ATTR_VSW_L_vsw_7_0(GET_L_BYTE(vsync_len)));

	rtk_prince_edp_sst_setting(dptx, mode);
	rtk_prince_edp_sst_dpformat_setting(dptx, mode);
	rtk_prince_edp_config_lane(dptx);

	rtk_dptx_update(dptx->dptx14_edp_reg_base, DPTX_PHY_CTRL,
		DP_IP_DPTX_PHY_CTRL_dptx_skew_en_mask
		| DP_IP_DPTX_PHY_CTRL_dptx_scb_en_mask,
		DP_IP_DPTX_PHY_CTRL_dptx_skew_en(1)
		| DP_IP_DPTX_PHY_CTRL_dptx_scb_en(1));

	rtk_prince_edp_config_timing_gen(dptx, mode);

	/* color bar enable */
	/* [27]=pattern gen en, [26:24]=000=color bar, [23:0]=RGB color space */
	/* writel(0x08808080, ioremap(0x98009320, 0x1)); */
}

void rtk_prince_edp_phy_set_scramble(struct rtk_prince_dptx *dptx, bool scramble)
{
	rtk_dptx_update(dptx->dptx14_edp_reg_base, DPTX_PHY_CTRL,
		DP_IP_DPTX_PHY_CTRL_sr_insert_en_mask
		| DP_IP_DPTX_PHY_CTRL_dptx_scb_en_mask,
		DP_IP_DPTX_PHY_CTRL_sr_insert_en(1)
		| DP_IP_DPTX_PHY_CTRL_dptx_scb_en(scramble));
}

void rtk_prince_edp_phy_set_pattern(struct rtk_prince_dptx *dptx, int pattern)
{
	rtk_dptx_update(dptx->dptx14_edp_reg_base, DPTX_ML_PAT_SEL,
		DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_mask
		| DP_IP_DPTX_ML_PAT_SEL_switch_pattern_auto_mask
		| DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en_mask
		| DP_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf_mask,
		DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DP_IP_DPTX_ML_PAT_SEL_switch_pattern_auto(1)
		| DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DP_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(0));

	usleep_range(1000, 2000);

	rtk_dptx_update(dptx->dptx14_edp_reg_base, DPTX_ML_PAT_SEL,
		DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_mask
		| DP_IP_DPTX_ML_PAT_SEL_switch_pattern_auto_mask
		| DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en_mask
		| DP_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf_mask,
		DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DP_IP_DPTX_ML_PAT_SEL_switch_pattern_auto(1)
		| DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DP_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(1));

	msleep(20);
}

void rtk_prince_edp_phy_config_csc(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "edp phy: config csc\n");
	dev_info(dptx->dev, "color_format = %d\n", dptx->color_format);

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_MAIN,
		EDPTX_MAIN_avg_mode_mask
		| EDPTX_MAIN_pixel_8bit_mask
		| EDPTX_MAIN_color_mode_mask
		| EDPTX_MAIN_csc_enable_mask,
		EDPTX_MAIN_avg_mode(0)
		| EDPTX_MAIN_pixel_8bit(0x1) // 0:10bit, 1:8bit
		| EDPTX_MAIN_color_mode(dptx->color_format)
		| EDPTX_MAIN_csc_enable((dptx->color_format) ? 1 : 0));

	// eDPTX CSC (color transform matrix) setting.
	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_CSC1,
		EDPTX_CSC1_m01_mask
		| EDPTX_CSC1_m00_mask,
		EDPTX_CSC1_m01(EDP_TX_1_SETTING_CONFIG_CSC_M01)
		| EDPTX_CSC1_m00(EDP_TX_1_SETTING_CONFIG_CSC_M00));
	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_CSC2,
		EDPTX_CSC2_m10_mask
		| EDPTX_CSC2_m02_mask,
		EDPTX_CSC2_m10(EDP_TX_1_SETTING_CONFIG_CSC_M10)
		| EDPTX_CSC2_m02(EDP_TX_1_SETTING_CONFIG_CSC_M02));
	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_CSC3,
		EDPTX_CSC3_m12_mask
		| EDPTX_CSC3_m11_mask,
		EDPTX_CSC3_m12(EDP_TX_1_SETTING_CONFIG_CSC_M12)
		| EDPTX_CSC3_m11(EDP_TX_1_SETTING_CONFIG_CSC_M11));
	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_CSC4,
		EDPTX_CSC4_m21_mask
		| EDPTX_CSC4_m20_mask,
		EDPTX_CSC4_m21(EDP_TX_1_SETTING_CONFIG_CSC_M21)
		| EDPTX_CSC4_m20(EDP_TX_1_SETTING_CONFIG_CSC_M20));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_CSC5,
		EDPTX_CSC5_m22_mask,
		EDPTX_CSC5_m22(EDP_TX_1_SETTING_CONFIG_CSC_M22));
	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_CSC6,
		EDPTX_CSC6_a1_mask
		| EDPTX_CSC6_a0_mask,
		EDPTX_CSC6_a1(EDP_TX_1_SETTING_CONFIG_CSC_A1)
		| EDPTX_CSC6_a0(EDP_TX_1_SETTING_CONFIG_CSC_A0));
	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_CSC7,
		EDPTX_CSC7_a2_mask,
		EDPTX_CSC7_a2(EDP_TX_1_SETTING_CONFIG_CSC_A2));
}

void rtk_prince_edp_phy_start_video(struct rtk_prince_dptx *dptx,
					 struct drm_display_mode *mode)
{
	dev_info(dptx->dev, "edp phy: start video\n");

	/* enable eDPTX tx_en */
	rtk_dptx_update(dptx->dptx14_edp_reg_base, DPTX_SFIFO_CTRL0,
		DP_IP_DPTX_SFIFO_CTRL0_tx_en_mask,
		DP_IP_DPTX_SFIFO_CTRL0_tx_en(1));

	/* change eDPTX output from TPS to video data */
	rtk_prince_edp_phy_set_pattern(dptx, RTK_PATTERN_VIDEO);

	/* start eDPTX video transmission */
	rtk_dptx_update(dptx->dptx14_edp_reg_base, ARBITER_CTRL,
		DP_IP_ARBITER_CTRL_vactive_md_mask
		| DP_IP_ARBITER_CTRL_arbiter_en_mask,
		DP_IP_ARBITER_CTRL_vactive_md(0)
		| DP_IP_ARBITER_CTRL_arbiter_en(1));

	/* check interrupt when frame done */
	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DV_SYNC_INTE,
		EDPTX_DV_SYNC_INTE_dv_sync_int_mask,
		EDPTX_DV_SYNC_INTE_dv_sync_int(mode->vtotal + 1));
}

void rtk_prince_edp_phy_disable_timing_gen(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "edp phy: disable timing gen\n");

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DH_WIDTH,
		EDPTX_DH_WIDTH_dh_width_mask,
		EDPTX_DH_WIDTH_dh_width(0));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DH_TOTAL,
		EDPTX_DH_TOTAL_dh_total_mask
		| EDPTX_DH_TOTAL_dh_total_last_line_mask,
		EDPTX_DH_TOTAL_dh_total(0)
		| EDPTX_DH_TOTAL_dh_total_last_line(0));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DH_DEN_START_END,
		EDPTX_DH_DEN_START_END_dh_den_sta_mask
		| EDPTX_DH_DEN_START_END_dh_den_end_mask,
		EDPTX_DH_DEN_START_END_dh_den_sta(0)
		| EDPTX_DH_DEN_START_END_dh_den_end(0));

	rtk_dptx_write(dptx->dptx14_edp_reg_base, ARBITER_SEC_END_CNT_HB, 0x0);

	rtk_dptx_write(dptx->dptx14_edp_reg_base, ARBITER_SEC_END_CNT_LB, 0x0);

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DV_DEN_START_END_FIELD1,
		EDPTX_DV_DEN_START_END_FIELD1_dv_den_sta_field1_mask
		| EDPTX_DV_DEN_START_END_FIELD1_dv_den_end_field1_mask,
		EDPTX_DV_DEN_START_END_FIELD1_dv_den_sta_field1(0)
		| EDPTX_DV_DEN_START_END_FIELD1_dv_den_end_field1(0));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DV_TOTAL,
		EDPTX_DV_TOTAL_dv_total_mask,
		EDPTX_DV_TOTAL_dv_total(0));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DV_VS_START_END_FIELD1,
		EDPTX_DV_VS_START_END_FIELD1_dv_vs_sta_field1_mask
		| EDPTX_DV_VS_START_END_FIELD1_dv_vs_end_field1_mask,
		EDPTX_DV_VS_START_END_FIELD1_dv_vs_sta_field1(0)
		| EDPTX_DV_VS_START_END_FIELD1_dv_vs_end_field1(0));

	rtk_dptx_update(dptx->edp_wrapper_reg_base, EDPTX_DH_VS_ADJ_FIELD1,
		EDPTX_DH_VS_ADJ_FIELD1_dh_vs_adj_field1_mask,
		EDPTX_DH_VS_ADJ_FIELD1_dh_vs_adj_field1(0));
}

int rtk_prince_dptx_combo_phy_setting(struct rtk_prince_dptx *dptx, struct drm_display_mode *mode)
{
	int ret;

	// set_SYS_CLOCK_ENABLE5_SECURE_reg(SYS_CLOCK_ENABLE5_SECURE_write_en7(1)|SYS_CLOCK_ENABLE5_SECURE_clk_en_dptx_hdcp(0));
	rtk_dptx_update(dptx->crt_reg_base, SYS_CLOCK_ENABLE5_SECURE,
		SYS_CLOCK_ENABLE5_SECURE_write_en7_mask |
		SYS_CLOCK_ENABLE5_SECURE_clk_en_dptx_hdcp_mask,
		SYS_CLOCK_ENABLE5_SECURE_write_en7(1) |
		SYS_CLOCK_ENABLE5_SECURE_clk_en_dptx_hdcp(0));

	// set_SYS_SOFT_RESET10_SECURE_reg(SYS_SOFT_RESET10_SECURE_write_en16(1)|SYS_SOFT_RESET10_SECURE_rstn_dptx_hdcp(0));
	rtk_dptx_update(dptx->crt_reg_base, SYS_SOFT_RESET10_SECURE,
		SYS_SOFT_RESET10_SECURE_write_en16_mask |
		SYS_SOFT_RESET10_SECURE_rstn_dptx_hdcp_mask,
		SYS_SOFT_RESET10_SECURE_write_en16(1) |
		SYS_SOFT_RESET10_SECURE_rstn_dptx_hdcp(0));

	// set_SYS_CLOCK_ENABLE10_reg(SYS_CLOCK_ENABLE10_write_en15(1)|SYS_CLOCK_ENABLE10_clk_en_edptx(0));
	clk_disable_unprepare(dptx->clk_edptx);

	// set_SYS_SOFT_RESET12_reg(SYS_SOFT_RESET12_write_en15(1)|SYS_SOFT_RESET12_rstn_edptx(0));
	reset_control_assert(dptx->rstc_edptx);

	ret = rtk_prince_edptx_phy_dppll_setting(dptx, mode);
	if (ret)
		return ret;

	// set_SYS_SOFT_RESET4_reg(SYS_SOFT_RESET4_write_en16(1)|SYS_SOFT_RESET4_rstn_hdmitop(1));
	rtk_dptx_update(dptx->crt_reg_base, SYS_SOFT_RESET4,
		SYS_SOFT_RESET4_write_en16_mask |
		SYS_SOFT_RESET4_rstn_hdmitop_mask,
		SYS_SOFT_RESET4_write_en16(1) |
		SYS_SOFT_RESET4_rstn_hdmitop(1));

	// set_SYS_CLOCK_ENABLE7_reg(SYS_CLOCK_ENABLE7_write_en11(1)|SYS_CLOCK_ENABLE7_clk_en_hdmitop(1));
	rtk_dptx_update(dptx->crt_reg_base, SYS_SOFT_RESET3,
		SYS_CLOCK_ENABLE7_write_en11_mask |
		SYS_CLOCK_ENABLE7_clk_en_hdmitop_mask,
		SYS_CLOCK_ENABLE7_write_en11(1) |
		SYS_CLOCK_ENABLE7_clk_en_hdmitop(1));

	// set_SYS_SOFT_RESET3_reg(SYS_SOFT_RESET3_write_en16(1)|SYS_SOFT_RESET3_rstn_disp(1) |
	//						SYS_SOFT_RESET3_write_en15(1)|SYS_SOFT_RESET3_rstn_hdmi(1));
	rtk_dptx_update(dptx->crt_reg_base, SYS_SOFT_RESET3,
		SYS_SOFT_RESET3_write_en16_mask |
		SYS_SOFT_RESET3_rstn_disp_mask |
		SYS_SOFT_RESET3_write_en15_mask |
		SYS_SOFT_RESET3_rstn_hdmi_mask,
		SYS_SOFT_RESET3_write_en16(1) |
		SYS_SOFT_RESET3_rstn_disp(1) |
		SYS_SOFT_RESET3_write_en15(1) |
		SYS_SOFT_RESET3_rstn_hdmi(1));

	// set_SYS_CLOCK_ENABLE1_reg(SYS_CLOCK_ENABLE1_write_en8(1)|SYS_CLOCK_ENABLE1_clk_en_hdmi(1));
	rtk_dptx_update(dptx->crt_reg_base, SYS_CLOCK_ENABLE1,
		SYS_CLOCK_ENABLE1_write_en8_mask |
		SYS_CLOCK_ENABLE1_clk_en_hdmi_mask,
		SYS_CLOCK_ENABLE1_write_en8(1) |
		SYS_CLOCK_ENABLE1_clk_en_hdmi(1));

	// set_SYS_CLOCK_ENABLE5_SECURE_reg(SYS_CLOCK_ENABLE5_SECURE_write_en7(1)|SYS_CLOCK_ENABLE5_SECURE_clk_en_dptx_hdcp(1));
	rtk_dptx_update(dptx->crt_reg_base, SYS_CLOCK_ENABLE5_SECURE,
		SYS_CLOCK_ENABLE5_SECURE_write_en7_mask |
		SYS_CLOCK_ENABLE5_SECURE_clk_en_dptx_hdcp_mask,
		SYS_CLOCK_ENABLE5_SECURE_write_en7(1) |
		SYS_CLOCK_ENABLE5_SECURE_clk_en_dptx_hdcp(1));

	// set_SYS_SOFT_RESET10_SECURE_reg(SYS_SOFT_RESET10_SECURE_write_en16(1)|SYS_SOFT_RESET10_SECURE_rstn_dptx_hdcp(1));
	rtk_dptx_update(dptx->crt_reg_base, SYS_SOFT_RESET10_SECURE,
		SYS_SOFT_RESET10_SECURE_write_en16_mask |
		SYS_SOFT_RESET10_SECURE_rstn_dptx_hdcp_mask,
		SYS_SOFT_RESET10_SECURE_write_en16(1) |
		SYS_SOFT_RESET10_SECURE_rstn_dptx_hdcp(1));

	// set_SYS_CLOCK_ENABLE10_reg(SYS_CLOCK_ENABLE10_write_en15(1)|SYS_CLOCK_ENABLE10_clk_en_edptx(1));
	clk_prepare_enable(dptx->clk_edptx);

	// set_SYS_SOFT_RESET12_reg(SYS_SOFT_RESET12_write_en15(1)|SYS_SOFT_RESET12_rstn_edptx(1));
	reset_control_deassert(dptx->rstc_edptx);

	return 0;
}
