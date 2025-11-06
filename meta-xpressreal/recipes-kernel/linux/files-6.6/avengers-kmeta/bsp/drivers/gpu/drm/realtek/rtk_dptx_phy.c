// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 RealTek Inc.
 */

#include <drm/drm_modes.h>
#include <linux/math64.h>

#include "rtk_dptx_kent_reg.h"
#include "rtk_crt_reg.h"
#include "rtk_dp_utils.h"
#include "rtk_dptx.h"
#include "rtk_dptx_phy.h"


void rtk_dptx_phy_config_aphy(struct rtk_dptx *dptx)
{
	dev_info(dptx->dev, "dptx phy: config aphy\n");

	// from u3DP Voltage swing_20250315.xlsx
	rtk_dptx14_aphy_write(dptx, 0x11C, 0x22000000);
	rtk_dptx14_aphy_write(dptx, 0x120, 0x88882222);
	rtk_dptx14_aphy_write(dptx, 0x0a0, 0x0CF3CF30);
	rtk_dptx14_aphy_write(dptx, 0x0fc, 0x430C0C00);
	rtk_dptx14_aphy_write(dptx, 0x100, 0x00000001);

	if (dptx->link_rate == DP_LINK_RATE_1_62) {
		rtk_dptx14_aphy_write(dptx, 0xa54, 0x00001400);
		rtk_dptx14_aphy_write(dptx, 0x09c, 0x0db6db60);
		rtk_dptx14_aphy_write(dptx, 0xa4c, 0x20000000);
		rtk_dptx14_aphy_write(dptx, 0x01c, 0x003F33FF); // ##REG_DPCMU_KVCO_RES[11:10]=00
		rtk_dptx14_aphy_write(dptx, 0x02c, 0x095501ff);
		rtk_dptx14_aphy_write(dptx, 0x030, 0x000003ed);
		rtk_dptx14_aphy_write(dptx, 0x034, 0xf00c6c00);
		rtk_dptx14_aphy_write(dptx, 0x024, 0x2210080B); // ##REG_DPCMU_PLLM2[23:20]=0001
		rtk_dptx14_aphy_write(dptx, 0x018, 0x30000000); // ##REG_DPCMU_DIV16[29:28]=11
		rtk_dptx14_aphy_write(dptx, 0x020, 0x91C80EBA); // ##REG_DPCMU_PLL_KVCO[29]=0
		rtk_dptx14_aphy_write(dptx, 0x020, 0xB1C80EBA); // ##REG_DPCMU_PLL_KVCO[29]=1
		rtk_dptx14_aphy_write(dptx, 0x028, 0x04A8002A); // ##REG_DPCMU_PREDIV_SEL[23]=1
		rtk_dptx14_aphy_write(dptx, 0xe0c, 0x00000001);
		rtk_dptx14_aphy_write(dptx, 0xe04, 0x02490000);
		rtk_dptx14_aphy_write(dptx, 0xe08, 0x000f0000);
	} else if (dptx->link_rate == DP_LINK_RATE_2_7) {
		rtk_dptx14_aphy_write(dptx, 0xa54, 0x00001400);
		rtk_dptx14_aphy_write(dptx, 0x09c, 0x0db6db60);
		rtk_dptx14_aphy_write(dptx, 0xa4c, 0x20000000);
		rtk_dptx14_aphy_write(dptx, 0x01c, 0x003F37FF); // ##REG_DPCMU_KVCO_RES[11:10]=01
		rtk_dptx14_aphy_write(dptx, 0x02c, 0x095501ff);
		rtk_dptx14_aphy_write(dptx, 0x030, 0x000003ed);
		rtk_dptx14_aphy_write(dptx, 0x034, 0xf00c6c00);
		rtk_dptx14_aphy_write(dptx, 0x024, 0x32180EBA); // ##REG_DPCMU_PLLM2[23:20]=0001
		rtk_dptx14_aphy_write(dptx, 0x018, 0x30000000); // ##REG_DPCMU_DIV16[29:28]=11
		rtk_dptx14_aphy_write(dptx, 0x020, 0x91D4F020); // ##REG_DPCMU_PLL_KVCO[29]=0
		rtk_dptx14_aphy_write(dptx, 0x020, 0xB1D4F020); // ##REG_DPCMU_PLL_KVCO[29]=0
		rtk_dptx14_aphy_write(dptx, 0x028, 0x04A8002A); // ##REG_DPCMU_PREDIV_SEL[23]=1
		rtk_dptx14_aphy_write(dptx, 0xe0c, 0x00000001);
		rtk_dptx14_aphy_write(dptx, 0xe04, 0x02490000);
		rtk_dptx14_aphy_write(dptx, 0xe08, 0x000f0000);
	} else if (dptx->link_rate == DP_LINK_RATE_5_4) {
		rtk_dptx14_aphy_write(dptx, 0xa54, 0x00001400);
		rtk_dptx14_aphy_write(dptx, 0x09c, 0x0db6db60);
		rtk_dptx14_aphy_write(dptx, 0xa4c, 0x20000000);
		rtk_dptx14_aphy_write(dptx, 0x01c, 0x003F37FF); // ##REG_DPCMU_KVCO_RES[11:10]=01
		rtk_dptx14_aphy_write(dptx, 0x02c, 0x095501ff);
		rtk_dptx14_aphy_write(dptx, 0x030, 0x000003ed);
		rtk_dptx14_aphy_write(dptx, 0x034, 0xf00c6c00);
		rtk_dptx14_aphy_write(dptx, 0x024, 0x32180EBA); // ##REG_DPCMU_PLLM2[23:20]=0001
		rtk_dptx14_aphy_write(dptx, 0x018, 0x30000000); // ##REG_DPCMU_DIV16[29:28]=11
		rtk_dptx14_aphy_write(dptx, 0x020, 0x91D4F020); // ##REG_DPCMU_PLL_KVCO[29]=0
		rtk_dptx14_aphy_write(dptx, 0x020, 0xB1D4F020); // ##REG_DPCMU_PLL_KVCO[29]=0
		rtk_dptx14_aphy_write(dptx, 0x028, 0x04A8002A); // ##REG_DPCMU_PREDIV_SEL[23]=1
		rtk_dptx14_aphy_write(dptx, 0xe0c, 0x00000001);
		rtk_dptx14_aphy_write(dptx, 0xe04, 0x02490000);
		rtk_dptx14_aphy_write(dptx, 0xe08, 0x000f0000);
	} else if (dptx->link_rate == DP_LINK_RATE_8_1) {
		rtk_dptx14_aphy_write(dptx, 0xa54, 0x00001400);
		rtk_dptx14_aphy_write(dptx, 0x09c, 0x0FBEFBE0); // ##111
		rtk_dptx14_aphy_write(dptx, 0xa4c, 0x20000000);
		rtk_dptx14_aphy_write(dptx, 0x01c, 0x003F33FF); // ##REG_DPCMU_KVCO_RES[11:10]=00
		rtk_dptx14_aphy_write(dptx, 0x02c, 0x095501F7);
		rtk_dptx14_aphy_write(dptx, 0x030, 0x000003C0);
		rtk_dptx14_aphy_write(dptx, 0x034, 0xf00c6c00);
		rtk_dptx14_aphy_write(dptx, 0x024, 0x2200080A); // ##REG_DPCMU_PLLM2[23:20]=0000
		rtk_dptx14_aphy_write(dptx, 0x018, 0x20000000); // ##REG_DPCMU_DIV16[29:28]=10
		rtk_dptx14_aphy_write(dptx, 0x020, 0x91C80EBA); // ##REG_DPCMU_PLL_KVCO[29]=0
		rtk_dptx14_aphy_write(dptx, 0x020, 0xB1C80EBA); // ##REG_DPCMU_PLL_KVCO[29]=1
		rtk_dptx14_aphy_write(dptx, 0x028, 0x0400000A); // ##REG_DPCMU_PREDIV_SEL[23]=0
		rtk_dptx14_aphy_write(dptx, 0xe0c, 0x00000001);
		rtk_dptx14_aphy_write(dptx, 0xe04, 0x02490000);
		rtk_dptx14_aphy_write(dptx, 0xe08, 0x000F0000);
	}
}

static int rtk_dptx_lane_setting(struct rtk_dptx *dptx, bool is_flipped)
{
	struct device *dev = dptx->dev;
	const u32 *table;
	u32 table_size;
	int i;

	table = of_get_property(dev->of_node,
					 is_flipped ? "flipped-lane-table" : "normal-lane-table",
					 &table_size);
	if (!table) {
		dev_err(dev, "Failed to read lane-table\n");
		return -EINVAL;
	}

	memset(dptx->tx_lane_enabled, 0, sizeof(dptx->tx_lane_enabled));
	for (i = 0; i < dptx->lane_count; i++)
		dptx->tx_lane_enabled[be32_to_cpu(table[i])] = true;

	for (i = 0; i < 4 ; i++)
		dev_info(dev, "[%s] lane %d: %s", __func__, i, dptx->tx_lane_enabled[i] ? "yes" : "no");

	rtk_dptx14_aphy_write(dptx, 0xe04,
		U3DP_PHY_PCS_USB31_DP14_DPHY2_reg_spd_ctrl3(dptx->tx_lane_enabled[3]) |
		U3DP_PHY_PCS_USB31_DP14_DPHY2_reg_spd_ctrl2(dptx->tx_lane_enabled[2]) |
		U3DP_PHY_PCS_USB31_DP14_DPHY2_reg_spd_ctrl1(dptx->tx_lane_enabled[1]) |
		U3DP_PHY_PCS_USB31_DP14_DPHY2_reg_spd_ctrl0(dptx->tx_lane_enabled[0]));

	rtk_dptx14_aphy_write(dptx, 0xe08,
		U3DP_PHY_PCS_USB31_DP14_DPHY3_reg_dp_tx_en_l3(dptx->tx_lane_enabled[3]) |
		U3DP_PHY_PCS_USB31_DP14_DPHY3_reg_dp_tx_en_l2(dptx->tx_lane_enabled[2]) |
		U3DP_PHY_PCS_USB31_DP14_DPHY3_reg_dp_tx_en_l1(dptx->tx_lane_enabled[1]) |
		U3DP_PHY_PCS_USB31_DP14_DPHY3_reg_dp_tx_en_l0(dptx->tx_lane_enabled[0]));

	rtk_dptx14_aphy_update(dptx, 0xe04,
			U3DP_PHY_PCS_USB31_DP14_DPHY2_reg_rst_dly(3),
			U3DP_PHY_PCS_USB31_DP14_DPHY2_reg_rst_dly(3));

	return 0;
}

static void rtk_dptx_ssc_setting(struct rtk_dptx *dptx)
{
	int ssc_n_code;
	int ssc_step_in;

	switch (dptx->link_rate) {
	case DP_LINK_RATE_1_62:
		ssc_n_code = 57;
		ssc_step_in = 385;
		break;
	case DP_LINK_RATE_2_7:
	case DP_LINK_RATE_5_4:
		ssc_n_code = 97;
		ssc_step_in = 641;
		break;
	case DP_LINK_RATE_8_1:
		ssc_n_code = 72;
		ssc_step_in = 481;
		break;
	}

	rtk_dptx14_aphy_write(dptx, 0xa04, 0x20000); // ssc off
	rtk_dptx14_aphy_write(dptx, 0xa08,
			U3DP_PHY_RG_CMU_DP_DPHY3_reg_rg_cmu_dp_ssc_n_code(ssc_n_code)
			| U3DP_PHY_RG_CMU_DP_DPHY3_reg_rg_cmu_dp_ssc_f_code(0));
	rtk_dptx14_aphy_write(dptx, 0xa10,
			U3DP_PHY_RG_CMU_DP_DPHY5_reg_rg_cmu_dp_ssc_tbase(818));
	rtk_dptx14_aphy_update(dptx, 0xa4c,
			U3DP_PHY_RG_CMU_DP_DPHY6_reg_rg_cmu_dp_ssc_step_in_mask,
			U3DP_PHY_RG_CMU_DP_DPHY6_reg_rg_cmu_dp_ssc_step_in(ssc_step_in));
	rtk_dptx14_aphy_write(dptx, 0xa00,
			U3DP_PHY_RG_CMU_DP_DPHY1_reg_rg_cmu_dp_ssc_bypass_pi(0));
}

static void rtk_dptx_set_crt_on(struct rtk_dptx *dptx, int dpll_m, int f_code)
{

	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL0,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_en_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_en(0));

	//ssc_init
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL6,
			SYS_DPLL_SSC_DIG_DPTX_CTRL6_DPLL_SSC_RSTB_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL6_DPLL_SSC_RSTB(1));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL2,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_Bypass_pi_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_Bypass_pi(0));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL4,
			SYS_DPLL_SSC_DIG_DPTX_CTRL4_Dot_gran_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL4_Dot_gran(0x3));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL2,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_En_pi_debug_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_En_pi_debug(0));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL3,
			SYS_DPLL_SSC_DIG_DPTX_CTRL3_Ncode_t_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL3_Ncode_t(dpll_m));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL3,
			SYS_DPLL_SSC_DIG_DPTX_CTRL3_Ncode_ssc_mask
			| SYS_DPLL_SSC_DIG_DPTX_CTRL3_Fcode_ssc_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL3_Ncode_ssc(0x13)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL3_Fcode_ssc(0x305));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL4,
			SYS_DPLL_SSC_DIG_DPTX_CTRL4_Gran_auto_rst_mask
			| SYS_DPLL_SSC_DIG_DPTX_CTRL4_Gran_est_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL4_Gran_auto_rst(0x0)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL4_Gran_est(0x73000));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL1,
			SYS_DPLL_SSC_DIG_DPTX_CTRL1_Hs_oc_stop_diff_mask
			| SYS_DPLL_SSC_DIG_DPTX_CTRL1_Oc_done_delay_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL1_Hs_oc_stop_diff(0x0)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL1_Oc_done_delay(0x1));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL0,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_step_set_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_step_set(0x0a9));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL2,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_Sdm_order_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_Sdm_order(0x1));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL1,
			SYS_DPLL_SSC_DIG_DPTX_CTRL1_Sel_oc_mode_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL1_Sel_oc_mode(0x0));
	//ssc_init done

	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL2,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_Sdm_order_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_Sdm_order(0x1));
	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL0,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_en_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_en(1));
	msleep_interruptible(1);

	rtk_dptx14_crt_update(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL0,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_en_mask,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_en(0));
	msleep_interruptible(1);
}

int rtk_dptx_phy_dppll_setting(struct rtk_dptx *dptx,
			 struct drm_display_mode *mode, bool polarity)
{
	struct device *dev = dptx->dev;
	int i;
	int dpll_m = 0, f_code = 0, pll_o = 0;
	int pll_n = 1, pll_bpsin = 1;
	uint32_t pixel_clk = (uint32_t) mode->clock;
	uint32_t table_pixel_clk;
	int dp_ssc_n_code = (dptx->link_rate == DP_LINK_RATE_8_1) ? 72 :
						(dptx->link_rate == DP_LINK_RATE_1_62) ? 57 : 97;
	uint64_t mod_percentage_10000, mf_value_10000, mf_value;
	uint32_t mf_value_rem;
	uint32_t dpll_m_new, f_code_new, fvco;

	// table: <DPLL_M, F code, DPLL_O, Pixel Freq>
	for (i = 0; i < RTK_DP_PIXEL_PLL_TABLE_SIZE; i++) {
		if (pixel_clk <= RTK_DP_PIXEL_PLL_TABLE[i][3])
			break;
	}
	dpll_m = RTK_DP_PIXEL_PLL_TABLE[i][0];
	f_code = RTK_DP_PIXEL_PLL_TABLE[i][1];
	pll_o = RTK_DP_PIXEL_PLL_TABLE[i][2];
	table_pixel_clk = RTK_DP_PIXEL_PLL_TABLE[i][3];

	dev_info(dev, "[%s] dpll_m: %x, f_code: %x, dpll_o: %x, table clk: %u\n",
				 __func__, dpll_m, f_code, pll_o, table_pixel_clk);


	/* If the clk is not in the table, calculate it */
	if (pixel_clk != table_pixel_clk) {
		mod_percentage_10000 = div_u64(mul_u32_u32(pixel_clk, 10000), table_pixel_clk);
		mf_value_10000 = div_u64(mod_percentage_10000 * (mul_u32_u32(dpll_m, 10000)
						 + 30000 + div_u64(mul_u32_u32(f_code, 10000),2048)), 10000);
		dpll_m_new = div_u64(mf_value_10000 - 30000, 10000);
		f_code_new = div_u64((mf_value_10000 - (dpll_m_new * 10000 + 30000))
						 * 2048 + 5000, 10000); // +5000 to round up
		fvco = div_u64(27 * mf_value_10000, 10000);
		if (fvco >= 500 && fvco <= 1000 && f_code_new <= 0x7ff && dpll_m_new <= 0xff) {
			dpll_m = dpll_m_new;
			f_code = f_code_new;
		} else {
			dev_err(dev, "[%s] need to add table!, pixel_clk: %u\n", __func__, pixel_clk);
		}

		mf_value = div_u64_rem(mf_value_10000, 10000, &mf_value_rem);
		dev_info(dev, "[%s] mod_percentage: %llu%%, mf_value: %llu.%u\n", __func__,
					 div_u64(mod_percentage_10000, 100), mf_value, mf_value_rem);
		dev_info(dev, "[%s] dpll_m_new: %x, f_code_new: %x, fvco: %u\n", __func__,
					 dpll_m_new, f_code_new, fvco);
	}


	rtk_dptx14_aphy_update(dptx, 0xa54,
			U3DP_PHY_RG_CMU_DP_DPHY8_reg_rg_cmu_dp_wd_time_rdy_ckout_mask
			| U3DP_PHY_RG_CMU_DP_DPHY8_reg_rg_cmu_dp_wd_time0_ck_mask,
			U3DP_PHY_RG_CMU_DP_DPHY8_reg_rg_cmu_dp_wd_time_rdy_ckout(1)
			| U3DP_PHY_RG_CMU_DP_DPHY8_reg_rg_cmu_dp_wd_time0_ck(1));

	if (dptx->link_rate == DP_LINK_RATE_8_1) {
		rtk_dptx14_aphy_update(dptx, 0x09c,
			REG_U3DP_004E_dp_REG_SPDSEL_L0_mask
			| REG_U3DP_004E_dp_REG_SPDSEL_L1_mask
			| REG_U3DP_004E_dp_REG_SPDSEL_L2_mask
			| REG_U3DP_004E_dp_REG_SPDSEL_L3_mask,
			REG_U3DP_004E_dp_REG_SPDSEL_L0(7)
			| REG_U3DP_004E_dp_REG_SPDSEL_L1(7)
			| REG_U3DP_004E_dp_REG_SPDSEL_L2(7)
			| REG_U3DP_004E_dp_REG_SPDSEL_L3(7));
	} else {
		rtk_dptx14_aphy_update(dptx, 0x09c,
			REG_U3DP_004E_dp_REG_SPDSEL_L0_mask
			| REG_U3DP_004E_dp_REG_SPDSEL_L1_mask
			| REG_U3DP_004E_dp_REG_SPDSEL_L2_mask
			| REG_U3DP_004E_dp_REG_SPDSEL_L3_mask,
			REG_U3DP_004E_dp_REG_SPDSEL_L0(6)
			| REG_U3DP_004E_dp_REG_SPDSEL_L1(6)
			| REG_U3DP_004E_dp_REG_SPDSEL_L2(6)
			| REG_U3DP_004E_dp_REG_SPDSEL_L3(6));
	}

	rtk_dptx14_aphy_write(dptx, 0xa4c, 0x20000000);
	rtk_dptx14_aphy_update(dptx, 0x01c,
			REG_U3DP_000E_usb_REG_DPCMU_DPLL_TEST_DIV_mask
			| REG_U3DP_000E_dp_REG_DPCMU_DPLL_TEST_DIV_mask
			| REG_U3DP_000E_usb_REG_DPCMU_DPLL_TEST_DIV_EN_mask
			| REG_U3DP_000E_dp_REG_DPCMU_DPLL_TEST_DIV_EN_mask
			| REG_U3DP_000E_usb_REG_DPCMU_KVCO_RES_mask
			| REG_U3DP_000E_dp_REG_DPCMU_KVCO_RES_mask
			| REG_U3DP_000E_usb_REG_DPCMU_MBIAS_POW_mask
			| REG_U3DP_000E_dp_REG_DPCMU_MBIAS_POW_mask,
			REG_U3DP_000E_usb_REG_DPCMU_DPLL_TEST_DIV(7)
			| REG_U3DP_000E_dp_REG_DPCMU_DPLL_TEST_DIV(7)
			| REG_U3DP_000E_usb_REG_DPCMU_DPLL_TEST_DIV_EN(1)
			| REG_U3DP_000E_dp_REG_DPCMU_DPLL_TEST_DIV_EN(1)
			| REG_U3DP_000E_usb_REG_DPCMU_KVCO_RES(0)
			| REG_U3DP_000E_dp_REG_DPCMU_KVCO_RES(3)
			| REG_U3DP_000E_usb_REG_DPCMU_MBIAS_POW(1)
			| REG_U3DP_000E_dp_REG_DPCMU_MBIAS_POW(1));

	rtk_dptx14_aphy_update(dptx, 0x02c,
			REG_U3DP_0016_usb_REG_DPCMU_VSET_SEL_mask
			| REG_U3DP_0016_dp_REG_DPCMU_VSET_SEL_mask
			| REG_U3DP_0016_usb_REG_DPLL_BPSIN_mask
			| REG_U3DP_0016_dp_REG_DPLL_BPSIN_mask
			| REG_U3DP_0016_usb_REG_DPLL_BPSPI_mask
			| REG_U3DP_0016_dp_REG_DPLL_BPSPI_mask
			| REG_U3DP_0016_usb_REG_DPLL_IP_mask
			| REG_U3DP_0016_dp_REG_DPLL_IP_mask
			| REG_U3DP_0016_usb_REG_DPLL_N_mask
			| REG_U3DP_0016_dp_REG_DPLL_N_mask,
			REG_U3DP_0016_usb_REG_DPCMU_VSET_SEL(0)
			| REG_U3DP_0016_dp_REG_DPCMU_VSET_SEL(1)
			| REG_U3DP_0016_usb_REG_DPLL_BPSIN(pll_bpsin)
			| REG_U3DP_0016_dp_REG_DPLL_BPSIN(pll_bpsin)
			| REG_U3DP_0016_usb_REG_DPLL_BPSPI(1)
			| REG_U3DP_0016_dp_REG_DPLL_BPSPI(1)
			| REG_U3DP_0016_usb_REG_DPLL_IP(5)
			| REG_U3DP_0016_dp_REG_DPLL_IP(5)
			| REG_U3DP_0016_usb_REG_DPLL_N(pll_n)
			| REG_U3DP_0016_dp_REG_DPLL_N(pll_n));

	rtk_dptx14_aphy_update(dptx, 0x030,
			REG_U3DP_0018_usb_REG_DPLL_O_mask
			| REG_U3DP_0018_dp_REG_DPLL_O_mask
			| REG_U3DP_0018_usb_REG_DPLL_PI_IBSEL_mask
			| REG_U3DP_0018_dp_REG_DPLL_PI_IBSEL_mask,
			REG_U3DP_0018_usb_REG_DPLL_O(pll_o)
			| REG_U3DP_0018_dp_REG_DPLL_O(pll_o)
			| REG_U3DP_0018_usb_REG_DPLL_PI_IBSEL(3)
			| REG_U3DP_0018_dp_REG_DPLL_PI_IBSEL(3));

	rtk_dptx14_aphy_update(dptx, 0x034,
			REG_U3DP_001A_usb_REG_DPLL_RS_mask
			| REG_U3DP_001A_dp_REG_DPLL_RS_mask
			| REG_U3DP_001A_usb_REG_DPLL_WDSET_mask
			| REG_U3DP_001A_dp_REG_DPLL_WDSET_mask,
			REG_U3DP_001A_usb_REG_DPLL_RS(3)
			| REG_U3DP_001A_dp_REG_DPLL_RS(3)
			| REG_U3DP_001A_usb_REG_DPLL_WDSET(1)
			| REG_U3DP_001A_dp_REG_DPLL_WDSET(0));

	if (dptx->link_rate == DP_LINK_RATE_8_1) {
		rtk_dptx14_aphy_update(dptx, 0x024,
			REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_LV_SEL_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_LDO_POW_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_POW_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_M1_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_M1_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_M2_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_M2_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_POW_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_POW_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_RS_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_RS_mask,
			REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_LV_SEL(2)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_LDO_POW(1)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_POW(1)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_M1(1)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_M1(1)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_M2(0)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_M2(0)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_POW(1)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_POW(1)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_RS(2)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_RS(4));

		rtk_dptx14_aphy_update(dptx, 0x018,
			REG_U3DP_000C_usb_REG_DPCMU_DIV16_mask
			| REG_U3DP_000C_dp_REG_DPCMU_DIV16_mask,
			REG_U3DP_000C_usb_REG_DPCMU_DIV16(2)
			| REG_U3DP_000C_dp_REG_DPCMU_DIV16(2));
	} else {
		rtk_dptx14_aphy_update(dptx, 0x024,
			REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_LV_SEL_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_LDO_POW_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_POW_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_M1_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_M1_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_M2_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_M2_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_POW_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_POW_mask
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_RS_mask
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_RS_mask,
			REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_LV_SEL(2)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_LDO_POW(1)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_POW(1)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_M1(1)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_M1(1)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_M2(1)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_M2(1)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_POW(1)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_POW(1)
			| REG_U3DP_0012_usb_REG_DPCMU_PLL_RS(2)
			| REG_U3DP_0012_dp_REG_DPCMU_PLL_RS(4));

		rtk_dptx14_aphy_update(dptx, 0x018,
			REG_U3DP_000C_usb_REG_DPCMU_DIV16_mask
			|REG_U3DP_000C_dp_REG_DPCMU_DIV16_mask,
			REG_U3DP_000C_usb_REG_DPCMU_DIV16(3)
			| REG_U3DP_000C_dp_REG_DPCMU_DIV16(3));
	}

	rtk_dptx14_aphy_update(dptx, 0x020,
			REG_U3DP_0010_usb_REG_DPCMU_PLL_CS_mask
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_CS_mask
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_DIVNSEL_mask
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_DIVNSEL_mask
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_EN_CAP_mask
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_EN_CAP_mask
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_EXT_LDO_LV_mask
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_EXT_LDO_LV_mask
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_IP_mask
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_IP_mask
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_KVCO_mask
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_KVCO_mask,
			REG_U3DP_0010_usb_REG_DPCMU_PLL_CS(2)
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_CS(2)
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_DIVNSEL(1)
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_DIVNSEL(1)
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_EN_CAP(2)
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_EN_CAP(2)
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_EXT_LDO_LV(1)
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_EXT_LDO_LV(1)
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_IP(8)
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_IP(0x7)
			| REG_U3DP_0010_usb_REG_DPCMU_PLL_KVCO(1)
			| REG_U3DP_0010_dp_REG_DPCMU_PLL_KVCO(0));

	if (dptx->link_rate == DP_LINK_RATE_8_1 || dptx->link_rate == DP_LINK_RATE_5_4) {
		rtk_dptx14_aphy_update(dptx, 0x028,
			REG_U3DP_0014_usb_REG_DPCMU_PLL_RSTB_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_RSTB_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_TSPC_SEL_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_TSPC_SEL_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_TST_POW_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_TST_POW_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_WDRST_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_WDRST_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_WDSET_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_WDSET_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PREDIV_SEL_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PREDIV_SEL_mask
			| REG_U3DP_0014_usb_REG_DPCMU_SEL_DUAL_R_mask
			| REG_U3DP_0014_dp_REG_DPCMU_SEL_DUAL_R_mask,
			REG_U3DP_0014_usb_REG_DPCMU_PLL_RSTB(0)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_RSTB(0)
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_TSPC_SEL(1)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_TSPC_SEL(1)
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_TST_POW(1)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_TST_POW(0)
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_WDRST(0)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_WDRST(0)
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_WDSET(0)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_WDSET(0)
			| REG_U3DP_0014_usb_REG_DPCMU_PREDIV_SEL(0)
			| REG_U3DP_0014_dp_REG_DPCMU_PREDIV_SEL(0)
			| REG_U3DP_0014_usb_REG_DPCMU_SEL_DUAL_R(1)
			| REG_U3DP_0014_dp_REG_DPCMU_SEL_DUAL_R(1));
	} else {
		rtk_dptx14_aphy_update(dptx, 0x028,
			REG_U3DP_0014_usb_REG_DPCMU_PLL_RSTB_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_RSTB_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_TSPC_SEL_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_TSPC_SEL_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_TST_POW_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_TST_POW_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_WDRST_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_WDRST_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_WDSET_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_WDSET_mask
			| REG_U3DP_0014_usb_REG_DPCMU_PREDIV_SEL_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PREDIV_SEL_mask
			| REG_U3DP_0014_usb_REG_DPCMU_SEL_DUAL_R_mask
			| REG_U3DP_0014_dp_REG_DPCMU_SEL_DUAL_R_mask,
			REG_U3DP_0014_usb_REG_DPCMU_PLL_RSTB(0)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_RSTB(0)
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_TSPC_SEL(1)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_TSPC_SEL(1)
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_TST_POW(1)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_TST_POW(0)
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_WDRST(0)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_WDRST(0)
			| REG_U3DP_0014_usb_REG_DPCMU_PLL_WDSET(0)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_WDSET(0)
			| REG_U3DP_0014_usb_REG_DPCMU_PREDIV_SEL(1)
			| REG_U3DP_0014_dp_REG_DPCMU_PREDIV_SEL(1)
			| REG_U3DP_0014_usb_REG_DPCMU_SEL_DUAL_R(1)
			| REG_U3DP_0014_dp_REG_DPCMU_SEL_DUAL_R(1));
	}

	rtk_dptx14_aphy_write(dptx, 0xe0c,
			U3DP_PHY_PCS_USB31_DP14_DPHY4_reg_dpll_rstb(0)
			| U3DP_PHY_PCS_USB31_DP14_DPHY4_reg_dpll_pow(1));


	if (rtk_dptx_lane_setting(dptx, polarity))
		return -1;

	rtk_dptx14_aphy_update(dptx, 0x028,
			REG_U3DP_0014_usb_REG_DPCMU_PLL_RSTB_mask
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_RSTB_mask,
			REG_U3DP_0014_usb_REG_DPCMU_PLL_RSTB(1)
			| REG_U3DP_0014_dp_REG_DPCMU_PLL_RSTB(1));

	//CRT SSC
	rtk_dptx14_crt_write(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL0,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_step_set(0x3ff));

	rtk_dptx14_aphy_write(dptx, 0xe0c,
			U3DP_PHY_PCS_USB31_DP14_DPHY4_reg_dpll_rstb(1)
			| U3DP_PHY_PCS_USB31_DP14_DPHY4_reg_dpll_pow(1));

	rtk_dptx14_crt_write(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL2,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_En_pi_debug(0)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL2_Sdm_order(0)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL2_Bypass_pi(0));

	rtk_dptx14_aphy_write(dptx, 0xa04, 0x00010000);

	rtk_dptx14_crt_write(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL0,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Fcode_t(f_code)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_step_set(0x0a9));

	rtk_dptx14_crt_write(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL1,
			SYS_DPLL_SSC_DIG_DPTX_CTRL1_Oc_done_delay(1)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL1_Hs_oc_stop_diff(0)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL1_Sel_oc_mode(0));

	rtk_dptx14_crt_write(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL2,
			SYS_DPLL_SSC_DIG_DPTX_CTRL2_En_pi_debug(0)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL2_Sdm_order(1)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL2_Bypass_pi(0));

	//link SSC
	rtk_dptx14_aphy_write(dptx, 0xa04, 0x00020000);

	rtk_dptx14_crt_write(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL0,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Fcode_t(f_code)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_step_set(0x0a9)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_en(1));

	rtk_dptx14_aphy_write(dptx, 0xa08,
			U3DP_PHY_RG_CMU_DP_DPHY3_reg_rg_cmu_dp_wd_pll_en_ref_en(1)
			| U3DP_PHY_RG_CMU_DP_DPHY3_reg_rg_cmu_dp_ssc_n_code(dp_ssc_n_code)
			| U3DP_PHY_RG_CMU_DP_DPHY3_reg_rg_cmu_dp_ssc_f_code(0));

	rtk_dptx14_crt_write(dptx, SYS_DPLL_SSC_DIG_DPTX_CTRL0,
			SYS_DPLL_SSC_DIG_DPTX_CTRL0_Fcode_t(f_code)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_step_set(0x0a9)
			| SYS_DPLL_SSC_DIG_DPTX_CTRL0_Oc_en(0));

	rtk_dptx_ssc_setting(dptx);
	msleep_interruptible(1);

	rtk_dptx14_crt_write(dptx, SYS_CLOCK_ENABLE8,
			SYS_CLOCK_ENABLE8_write_en16(1)
			| SYS_CLOCK_ENABLE8_clk_en_dptx_pxl(1)); //clk_en_dptx_pxl=1

	rtk_dptx_set_crt_on(dptx, dpll_m, f_code);

	return 0;
}

void rtk_dptx_phy_config_lane(struct rtk_dptx *dptx)
{
	uint32_t ctrl_lane_num = 1;
	uint32_t v2analog = 0;

	dev_info(dptx->dev, "dptx phy: config lane\n");
	switch (dptx->lane_count) {
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

	rtk_dptx14_update(dptx, DPTX14_MAIN, 0,
			DPTX14_MAIN_iso_ana_b(1) | DPTX14_MAIN_pow_pll(1));

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DP_PHY_CTRL,
			DPTX14_MAC_IP_DP_PHY_CTRL_v2analog_mask
			| DPTX14_MAC_IP_DP_PHY_CTRL_lane_num_mask
			| DPTX14_MAC_IP_DP_PHY_CTRL_mst_en_mask,
			DPTX14_MAC_IP_DP_PHY_CTRL_v2analog(v2analog)
			| DPTX14_MAC_IP_DP_PHY_CTRL_lane_num(ctrl_lane_num)
			| DPTX14_MAC_IP_DP_PHY_CTRL_mst_en(0x0));

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DP_MAC_CTRL,
			DPTX14_MAC_IP_DP_MAC_CTRL_enhance_md_mask
			| DPTX14_MAC_IP_DP_MAC_CTRL_lane_num_mask,
			DPTX14_MAC_IP_DP_MAC_CTRL_enhance_md(1)
			| DPTX14_MAC_IP_DP_MAC_CTRL_lane_num(ctrl_lane_num));

	rtk_dptx14_write(dptx, DPTX14_LANE, DPTX14_LANE_num(0x4));

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DPTX_CLK_GEN,
			DPTX14_MAC_IP_DPTX_CLK_GEN_div_num_mask,
			DPTX14_MAC_IP_DPTX_CLK_GEN_div_num(1));

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DPTX_SFIFO_CTRL0,
			DPTX14_MAC_IP_DPTX_SFIFO_CTRL0_reg_rd_start_pos_mask,
			DPTX14_MAC_IP_DPTX_SFIFO_CTRL0_reg_rd_start_pos(4));

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DPTX_SFIFO_CTRL0,
			DPTX14_MAC_IP_DPTX_SFIFO_CTRL0_reg_tx_en_mask,
			DPTX14_MAC_IP_DPTX_SFIFO_CTRL0_reg_tx_en(1));
}

static void rtk_dptx_sst_setting(struct rtk_dptx *dptx,
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
	uint32_t link_rate = dptx->link_rate;

	mvid = div_u64(mul_u32_u32(mode->clock, nvid), link_rate);
	dev_info(dptx->dev, "[%s] MVID: %llu\n", __func__, mvid);

	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_MN_M_VID_H,
		DPTX14_MAC_IP_MN_M_VID_H_mvid_23_16(GET_MH_BYTE(mvid)));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_MN_M_VID_M,
		DPTX14_MAC_IP_MN_M_VID_M_mvid_15_8(GET_ML_BYTE(mvid)));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_MN_M_VID_L,
		DPTX14_MAC_IP_MN_M_VID_L_mvid_7_0(GET_L_BYTE(mvid)));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_MN_N_VID_H, 0);
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_MN_N_VID_M, 0x80);
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_MN_N_VID_L, 0);

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_VID_AUTO_EN_1,
		DPTX14_MAC_IP_MN_VID_AUTO_EN_1_mn_vid_auto_en_mask
		| DPTX14_MAC_IP_MN_VID_AUTO_EN_1_mn_vid_db_mask,
		DPTX14_MAC_IP_MN_VID_AUTO_EN_1_mn_vid_auto_en(0)
		| DPTX14_MAC_IP_MN_VID_AUTO_EN_1_mn_vid_db(0x1)); //0x40
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MSA_MISC0,
		DPTX14_MAC_IP_MSA_MISC0_colorbit_mask
		| DPTX14_MAC_IP_MSA_MISC0_ycc_col_mask
		| DPTX14_MAC_IP_MSA_MISC0_dyn_range_mask
		| DPTX14_MAC_IP_MSA_MISC0_component_format_mask,
		DPTX14_MAC_IP_MSA_MISC0_colorbit(bpc)
		| DPTX14_MAC_IP_MSA_MISC0_ycc_col(0x0) // ITU-R BT601-5
		| DPTX14_MAC_IP_MSA_MISC0_dyn_range(0x0) // VESA
		| DPTX14_MAC_IP_MSA_MISC0_component_format(component_format));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MSA_CTRL,
		DPTX14_MAC_IP_MSA_CTRL_msa_db_mask,
		DPTX14_MAC_IP_MSA_CTRL_msa_db(1)); //0x80
}

static void rtk_dptx_sst_dpformat_setting(struct rtk_dptx *dptx,
								 struct drm_display_mode *mode)
{
	/* RGB,YUV444: 3, YUV422: 2, YUV420: 1.5*/
	uint32_t comp_x = (dptx->color_format == RTK_COLOR_FORMAT_YUV422) ? 2 : 3;
	uint32_t comp_y = (dptx->color_format == RTK_COLOR_FORMAT_YUV420) ? 2 : 1;
	uint32_t bpc = (uint32_t) dptx->bpc;
	uint32_t v_data_per_line;
	uint32_t tu_size, tu_size_decimal;
	uint32_t tu_size_x, tu_size_y;
	uint32_t link_rate = (uint32_t) dptx->link_rate / 1000;
	uint32_t lane_count = (uint32_t) dptx->lane_count;
	uint32_t hdisplay = (uint32_t) mode->hdisplay;
	uint32_t hsync_len = (uint32_t) (mode->hsync_end - mode->hsync_start);
	uint32_t hback_porch = (uint32_t) (mode->htotal - mode->hsync_end);
	uint32_t clock = (uint32_t) mode->clock; /* in kHz */
	uint32_t hdelay;
	uint32_t normal_image = 256 * 64 / 2;
	uint32_t v_data = hdisplay * comp_x * bpc / comp_y;

	v_data_per_line = v_data / (8 * lane_count);
	tu_size_x = 64 * bpc * comp_x * clock;
	tu_size_y = link_rate * lane_count * 8 * comp_y * 1000;
	tu_size = tu_size_x / tu_size_y;
	tu_size_decimal = tu_size_x * 10 / tu_size_y % 10;
	hdelay = (v_data > normal_image) ? // normal
		(normal_image * comp_y / (bpc * comp_x) + hsync_len + hback_porch) * link_rate * 1000 / clock + 1 :
		// small image
		(hdisplay / 2 + hsync_len + hback_porch) * link_rate * 1000 / clock + 1;

	dev_info(dptx->dev, "[%s] v_data_per_line: %u, tu_size: %u.%u, hdelay: %u\n",
		 __func__, v_data_per_line, tu_size, tu_size_decimal, hdelay);

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_V_DATA_PER_LINE0,
		DPTX14_MAC_IP_V_DATA_PER_LINE0_v_data_per_line_15_8_mask,
		DPTX14_MAC_IP_V_DATA_PER_LINE0_v_data_per_line_15_8(GET_ML_BYTE(v_data_per_line)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_V_DATA_PER_LINE1,
		DPTX14_MAC_IP_V_DATA_PER_LINE1_v_data_per_line_7_0_mask,
		DPTX14_MAC_IP_V_DATA_PER_LINE1_v_data_per_line_7_0(GET_L_BYTE(v_data_per_line)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_TU_DATA_SIZE0,
		DPTX14_MAC_IP_TU_DATA_SIZE0_tu_data_size_9_3_mask,
		DPTX14_MAC_IP_TU_DATA_SIZE0_tu_data_size_9_3(tu_size));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_TU_DATA_SIZE1,
		DPTX14_MAC_IP_TU_DATA_SIZE1_tu_data_size_2_0_mask,
		DPTX14_MAC_IP_TU_DATA_SIZE1_tu_data_size_2_0(tu_size_decimal));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_HDEALY0,
		DPTX14_MAC_IP_HDEALY0_hdelay_15_8_mask,
		DPTX14_MAC_IP_HDEALY0_hdelay_15_8(GET_ML_BYTE(hdelay)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_HDEALY1,
		DPTX14_MAC_IP_HDEALY1_hdelay_7_0_mask,
		DPTX14_MAC_IP_HDEALY1_hdelay_7_0(GET_L_BYTE(hdelay)));
}

void rtk_dptx_phy_config_video_timing(struct rtk_dptx *dptx,
			struct drm_display_mode *mode, bool polarity)
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
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_HTT_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_HTT_M_htotal_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HTT_M_htotal_15_8(GET_ML_BYTE(htotal)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_HTT_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_HTT_L_htotal_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HTT_L_htotal_7_0(GET_L_BYTE(htotal)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_HST_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_HST_M_hstart_15_8_mask, // hstart = hs_width + hs_bp
		DPTX14_MAC_IP_MN_STRM_ATTR_HST_M_hstart_15_8(GET_ML_BYTE(hsync_len + hback_porch)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_HST_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_HST_L_hstart_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HST_L_hstart_7_0(GET_L_BYTE(hsync_len + hback_porch)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_HWD_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_HWD_M_hwidth_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HWD_M_hwidth_15_8(GET_ML_BYTE(hactive)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_HWD_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_HWD_L_hwidth_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HWD_L_hwidth_7_0(GET_L_BYTE(hactive)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M_hsp_mask
		| DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M_hsw_14_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M_hsp(hsp)
		| DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M_hsw_14_8(GET_ML_BYTE(hsync_len)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_HSW_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_HSW_L_hsw_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_HSW_L_hsw_7_0(GET_L_BYTE(hsync_len)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DPTX_MEAS_BYPASS,
		DPTX14_MAC_IP_DPTX_MEAS_BYPASS_measure_bypass_en_mask,
		DPTX14_MAC_IP_DPTX_MEAS_BYPASS_measure_bypass_en(1));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_M_vtotal_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_M_vtotal_15_8(GET_ML_BYTE(vtotal)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_L_vtotal_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_L_vtotal_7_0(GET_L_BYTE(vtotal)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_VST_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_VST_M_vstart_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VST_M_vstart_15_8(GET_ML_BYTE(vsync_len + vback_porch)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_VST_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_VST_L_vstart_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VST_L_vstart_7_0(GET_L_BYTE(vsync_len + vback_porch)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_VHT_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_VHT_M_vheight_15_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VHT_M_vheight_15_8(GET_ML_BYTE(vactive)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_VHT_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_VHT_L_vheight_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VHT_L_vheight_7_0(GET_L_BYTE(vactive)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M,
		DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M_vsp_mask
		| DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M_vsw_14_8_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M_vsp(vsp)
		| DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M_vsw_14_8(GET_ML_BYTE(vsync_len)));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_MN_STRM_ATTR_VSW_L,
		DPTX14_MAC_IP_MN_STRM_ATTR_VSW_L_vsw_7_0_mask,
		DPTX14_MAC_IP_MN_STRM_ATTR_VSW_L_vsw_7_0(GET_L_BYTE(vsync_len)));

	rtk_dptx_sst_setting(dptx, mode);
	rtk_dptx_sst_dpformat_setting(dptx, mode);

	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_DPTX_LANE_SWAP, (polarity) ? 0x4e : 0xb1);
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DPTX_SFIFO_CTRL2,
		DPTX14_MAC_IP_DPTX_SFIFO_CTRL2_reg_sfifo_water_level_2_0_mask,
		DPTX14_MAC_IP_DPTX_SFIFO_CTRL2_reg_sfifo_water_level_2_0(0x7));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DPTX_PHY_CTRL,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_sr_insert_en_mask
		| DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_skew_en_mask
		| DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_scb_en_mask,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_sr_insert_en(1)
		| DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_skew_en(0)
		| DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_scb_en(1)); //0x15
	msleep_interruptible(1);

	// DPTX timing gen setting
	rtk_dptx14_update(dptx, DPTX14_DH_WIDTH,
		DPTX14_DH_WIDTH_dh_width_mask,
		DPTX14_DH_WIDTH_dh_width(hsync_len));
	rtk_dptx14_update(dptx, DPTX14_DH_TOTAL,
		DPTX14_DH_TOTAL_dh_total_mask
		| DPTX14_DH_TOTAL_dh_total_last_line_mask,
		DPTX14_DH_TOTAL_dh_total(htotal)
		| DPTX14_DH_TOTAL_dh_total_last_line(htotal));
	rtk_dptx14_update(dptx, DPTX14_DH_DEN_START_END,
		DPTX14_DH_DEN_START_END_dh_den_sta_mask
		| DPTX14_DH_DEN_START_END_dh_den_end_mask,
		DPTX14_DH_DEN_START_END_dh_den_sta(dh_den_sta)
		| DPTX14_DH_DEN_START_END_dh_den_end(dh_den_end));

	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_ARBITER_SEC_END_CNT_HB, 0x0);
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_ARBITER_SEC_END_CNT_LB, 0x10); //SEC_END_CNT = 0x10
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_ARBITER_DEBUG,
		DPTX14_MAC_IP_ARBITER_DEBUG_sec_end_cnt_db_mask,
		DPTX14_MAC_IP_ARBITER_DEBUG_sec_end_cnt_db(1));

	rtk_dptx14_update(dptx, DPTX14_DV_DEN_START_END_FIELD1,
		DPTX14_DV_DEN_START_END_FIELD1_dv_den_sta_field1_mask
		| DPTX14_DV_DEN_START_END_FIELD1_dv_den_end_field1_mask,
		DPTX14_DV_DEN_START_END_FIELD1_dv_den_sta_field1(dv_den_sta_field1)
		| DPTX14_DV_DEN_START_END_FIELD1_dv_den_end_field1(dv_den_end_field1));
	rtk_dptx14_update(dptx, DPTX14_DV_TOTAL,
		DPTX14_DV_TOTAL_dv_total_mask,
		DPTX14_DV_TOTAL_dv_total(vtotal));
	rtk_dptx14_update(dptx, DPTX14_DV_VS_START_END_FIELD1,
		DPTX14_DV_VS_START_END_FIELD1_dv_vs_sta_field1_mask
		| DPTX14_DV_VS_START_END_FIELD1_dv_vs_end_field1_mask,
		DPTX14_DV_VS_START_END_FIELD1_dv_vs_sta_field1(dv_vs_sta_field1)
		| DPTX14_DV_VS_START_END_FIELD1_dv_vs_end_field1(dv_vs_end_field1));
	rtk_dptx14_update(dptx, DPTX14_DH_VS_ADJ_FIELD1,
		DPTX14_DH_VS_ADJ_FIELD1_dh_vs_adj_field1_mask,
		DPTX14_DH_VS_ADJ_FIELD1_dh_vs_adj_field1(2));
	rtk_dptx14_update(dptx, DPTX14_MAIN,
		DPTX14_MAIN_avg_mode_mask
		| DPTX14_MAIN_pixel_8bit_mask
		| DPTX14_MAIN_color_mode_mask
		| DPTX14_MAIN_iso_ana_b_mask
		| DPTX14_MAIN_mbias_en_mask
		| DPTX14_MAIN_pow_xtal_mask
		| DPTX14_MAIN_csc_enable_mask
		| DPTX14_MAIN_dither_enable_mask,
		DPTX14_MAIN_avg_mode(0)
		| DPTX14_MAIN_pixel_8bit(1) // 0:10bit, 1:8bit
		| DPTX14_MAIN_color_mode(dptx->color_format)
		| DPTX14_MAIN_iso_ana_b(1)
		| DPTX14_MAIN_mbias_en(1)
		| DPTX14_MAIN_pow_xtal(1)
		| DPTX14_MAIN_csc_enable((dptx->color_format) ? 1 : 0)
		| DPTX14_MAIN_dither_enable(0));
}

void rtk_dptx_phy_set_scramble(struct rtk_dptx *dptx, bool scramble)
{
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DPTX_PHY_CTRL,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_scb_en_mask,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_dptx_scb_en(scramble));
}

void rtk_dptx_phy_set_pattern(struct rtk_dptx *dptx, uint8_t pattern)
{
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_DPTX_ML_PAT_SEL,
		DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_switch_pattern_auto(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(0));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_DPTX_ML_PAT_SEL,
		DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_switch_pattern_auto(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(0));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_DPTX_ML_PAT_SEL,
		DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_switch_pattern_auto(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DPTX14_MAC_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(1));

}

int rtk_dptx_mac_signal_setting(struct rtk_dptx *dptx,
						 struct rtk_dptx_train_signal signals[4])
{
	struct device *dev = dptx->dev;
	u32 table_size;
	const u32 *table;
	u32 drv[4][4][3];
	int i, j, k, l = 0;
	uint8_t swing, emphasis;
	/*	emp		0			1		2		3
	 *	sw
	 *	0		v1,	v2,	v3	...
	 *	1		...
	 *	2
	 *	3
	 */

	table = of_get_property(dev->of_node, "drv-table", &table_size);
	if (!table || table_size != sizeof(drv)) {
		dev_err(dev, "Failed to read drv-table\n");
		return -EINVAL;
	}

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			for (k = 0; k < 3; k++)
				drv[i][j][k] = be32_to_cpu(table[l++]);

	for (i = 0; i < RTK_DP_MAX_LANE_COUNT; i++) {
		if (dptx->tx_lane_enabled[i] == false)
			continue;

		swing = signals[i].swing;
		emphasis = signals[i].emphasis;
		rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_RIV0,
			(0x1 << (16 + i)) | 0xF << (i * 4),
			drv[swing][emphasis][0] << (16 + i)
			| drv[swing][emphasis][1] << (i * 4) | 0x00300000);
		rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_RIV1,
			(0xF << (i * 4)) | 0xFF000000,
			drv[swing][emphasis][2] << (i * 4) | 0x1F000000);
	}

	return 0;
}

// --------------- lane signallevel -------------------------- //
/* from u3DP Voltage swing_20250315
 *	tx_level[0]: tx_ldo_p_en (1 Byte)
 *	tx_level[1]: tx_ldo_p_ref (4 Byte)
 *	tx_level[2]: tx_main_deg (5 Byte)
 *	tx_level[3]: post1_amp_6db (6 Byte)
 */
static void rtk_dptx_aphy_signal_lane0_setting(struct rtk_dptx *dptx, u32 *tx_level)
{
	rtk_dptx14_aphy_update(dptx, 0x0c0, 0x1 << 29, tx_level[0] << 29);
	rtk_dptx14_aphy_update(dptx, 0x0c4, 0xf << 8, tx_level[1] << 8);
	rtk_dptx14_aphy_update(dptx, 0x0d0, 0x1f << 21, tx_level[2] << 21);
	rtk_dptx14_aphy_update(dptx, 0x0d8, 0x3f << 16, tx_level[3] << 16);
}

static void rtk_dptx_aphy_signal_lane1_setting(struct rtk_dptx *dptx, u32 *tx_level)
{
	rtk_dptx14_aphy_update(dptx, 0x0c0, 0x1 << 31, tx_level[0] << 31);
	rtk_dptx14_aphy_update(dptx, 0x0c4, 0xf << 16, tx_level[1] << 16);
	rtk_dptx14_aphy_update(dptx, 0x0d4, 0x1f << 0, tx_level[2] << 0);
	rtk_dptx14_aphy_update(dptx, 0x0dc, 0x3f << 0, tx_level[3] << 0);
}

static void rtk_dptx_aphy_signal_lane2_setting(struct rtk_dptx *dptx, u32 *tx_level)
{
	rtk_dptx14_aphy_update(dptx, 0x0c4, 1 << 1, tx_level[0] << 1);
	rtk_dptx14_aphy_update(dptx, 0x0c4, 0xf << 24, tx_level[1] << 24);
	rtk_dptx14_aphy_update(dptx, 0x0d4, 0x1f << 10, tx_level[2] << 10);
	rtk_dptx14_aphy_update(dptx, 0x0dc, 0x3f << 16, tx_level[3] << 16);
}

static void rtk_dptx_aphy_signal_lane3_setting(struct rtk_dptx *dptx, u32 *tx_level)
{
	rtk_dptx14_aphy_update(dptx, 0x0c4, 1 << 3, tx_level[0] << 3);
	rtk_dptx14_aphy_update(dptx, 0x0c8, 0xf << 0, tx_level[1] << 0);
	rtk_dptx14_aphy_update(dptx, 0x0d4, 0x1f << 21, tx_level[2] << 21);
	rtk_dptx14_aphy_update(dptx, 0x0e0, 0x3f << 0, tx_level[3] << 0);
}

int rtk_dptx_aphy_signal_setting(struct rtk_dptx *dptx,
						 struct rtk_dptx_train_signal signals[4])
{
	struct device *dev = dptx->dev;
	u32 table_size;
	const u32 *table;
	u32 sp[4][4][4];
	u32 *tx_level;
	int i, j, k, l = 0;
	uint8_t swing, emphasis;

	void (*set_signallevel[4])(struct rtk_dptx *dptx, u32 *tx_level) = {
		 rtk_dptx_aphy_signal_lane0_setting, rtk_dptx_aphy_signal_lane1_setting,
		 rtk_dptx_aphy_signal_lane2_setting, rtk_dptx_aphy_signal_lane3_setting};
	/*	-------- from u3DP Voltage swing_20250315.xlsx --------
	 *emp		0		1		2		3
	 *	sw
	 *	0		sp00	sp01	sp02	sp03
	 *	1		sp10	sp11	sp12	x
	 *	2		sp20	sp21	x		x
	 *	3		sp30	x		x		x
	 */
	/* spxx: <REX_TX_LDO_P_EN, REX_TX_LDO_P_REF, REX_TX_MAIN_DEG, REX_POST1_AMP_6dB*/

	table = of_get_property(dev->of_node,
			 (dptx->link_rate == DP_LINK_RATE_1_62 ||
			 dptx->link_rate == DP_LINK_RATE_2_7) ?
			 "hbr-rbr-sw-emp-table" : "hbr2-hbr3-sw-emp-table",
			 &table_size);
	if (!table || table_size != sizeof(sp)) {
		dev_err(dev, "Failed to read hbr-rbr-sw-emp-table\n");
		return -EINVAL;
	}

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			for (k = 0; k < 4; k++)
				sp[i][j][k] = be32_to_cpu(table[l++]);


	for (i = 0; i < RTK_DP_MAX_LANE_COUNT; i++) {
		if (dptx->tx_lane_enabled[i] == false)
			continue;

		swing = signals[i].swing;
		emphasis = signals[i].emphasis;
		tx_level = sp[swing][emphasis];
		set_signallevel[i](dptx, tx_level);
		dev_info(dev, "[%s] lane: %d, sw: %u, emp: %u, level: %x %x %x %x\n",
				 __func__, i, swing, emphasis, tx_level[0],
				 tx_level[1], tx_level[2], tx_level[3]);
	}


	return 0;
}

void rtk_dptx_phy_start_video(struct rtk_dptx *dptx,
							 struct drm_display_mode *mode)
{
	rtk_dptx_phy_set_scramble(dptx, true);
	rtk_dptx_phy_set_pattern(dptx, RTK_PATTERN_VIDEO);

	// start DPTX video transmission
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_ARBITER_CTRL,
		DPTX14_MAC_IP_ARBITER_CTRL_hwidth_div2_mask
		| DPTX14_MAC_IP_ARBITER_CTRL_vactive_md_mask
		| DPTX14_MAC_IP_ARBITER_CTRL_arbiter_en_mask,
		DPTX14_MAC_IP_ARBITER_CTRL_hwidth_div2(0)
		| DPTX14_MAC_IP_ARBITER_CTRL_vactive_md(1)
		| DPTX14_MAC_IP_ARBITER_CTRL_arbiter_en(1));
	rtk_dptx14_update(dptx, DPTX14_DV_SYNC_INTE,
		DPTX14_DV_SYNC_INTE_write_en1_mask
		| DPTX14_DV_SYNC_INTE_dv_sync_int_mask,
		DPTX14_DV_SYNC_INTE_write_en1(1)
		| DPTX14_DV_SYNC_INTE_dv_sync_int(mode->vtotal + 1));
}

void rtk_dptx_phy_disable_timing_gen(struct rtk_dptx *dptx)
{
	dev_info(dptx->dev, "dptx phy: disable timing gen\n");

	rtk_dptx14_update(dptx, DPTX14_DH_WIDTH,
		DPTX14_DH_WIDTH_dh_width_mask,
		DPTX14_DH_WIDTH_dh_width(0));
	rtk_dptx14_update(dptx, DPTX14_DH_TOTAL,
		DPTX14_DH_TOTAL_dh_total_mask
		| DPTX14_DH_TOTAL_dh_total_last_line_mask,
		DPTX14_DH_TOTAL_dh_total(0)
		| DPTX14_DH_TOTAL_dh_total_last_line(0));
	rtk_dptx14_update(dptx, DPTX14_DH_DEN_START_END,
		DPTX14_DH_DEN_START_END_dh_den_sta_mask
		| DPTX14_DH_DEN_START_END_dh_den_end_mask,
		DPTX14_DH_DEN_START_END_dh_den_sta(0)
		| DPTX14_DH_DEN_START_END_dh_den_end(0));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_ARBITER_SEC_END_CNT_HB, 0x0);
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_ARBITER_SEC_END_CNT_LB, 0x0);
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_ARBITER_DEBUG,
		DPTX14_MAC_IP_ARBITER_DEBUG_sec_end_cnt_db_mask,
		DPTX14_MAC_IP_ARBITER_DEBUG_sec_end_cnt_db(0));
	rtk_dptx14_update(dptx, DPTX14_DV_DEN_START_END_FIELD1,
		DPTX14_DV_DEN_START_END_FIELD1_dv_den_sta_field1_mask
		| DPTX14_DV_DEN_START_END_FIELD1_dv_den_end_field1_mask,
		DPTX14_DV_DEN_START_END_FIELD1_dv_den_sta_field1(0)
		| DPTX14_DV_DEN_START_END_FIELD1_dv_den_end_field1(0));
	rtk_dptx14_update(dptx, DPTX14_DV_TOTAL,
		DPTX14_DV_TOTAL_dv_total_mask,
		DPTX14_DV_TOTAL_dv_total(0));
	rtk_dptx14_update(dptx, DPTX14_DV_VS_START_END_FIELD1,
		DPTX14_DV_VS_START_END_FIELD1_dv_vs_sta_field1_mask
		| DPTX14_DV_VS_START_END_FIELD1_dv_vs_end_field1_mask,
		DPTX14_DV_VS_START_END_FIELD1_dv_vs_sta_field1(0)
		| DPTX14_DV_VS_START_END_FIELD1_dv_vs_end_field1(0));
	rtk_dptx14_update(dptx, DPTX14_DH_VS_ADJ_FIELD1,
		DPTX14_DH_VS_ADJ_FIELD1_dh_vs_adj_field1_mask,
		DPTX14_DH_VS_ADJ_FIELD1_dh_vs_adj_field1(0));
}

/* Audio */
static void rtk_dptx_audio_set_pkt(struct rtk_dptx *dptx,
							 uint32_t pkt_type, uint8_t *pkt)
{
	// pkt_type -
	// 1 : timestamp
	// 2 : audio stream
	// 7 : vsc
	// 8 : rsv
	// 9 : vsif
	// 10: avi infoframe
	// 12: audio infoframe
	// 13: mpeg infoframe

	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_PACKET_TYPE,
		DPTX14_MAC_IP_SEC_PH_PACKET_TYPE_packet_type(pkt_type));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_HB0,
		DPTX14_MAC_IP_SEC_PH_HB0_packet_header_hb_7_0(pkt[0]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_HB1,
		DPTX14_MAC_IP_SEC_PH_HB1_packet_header_hb_15_8(pkt[1]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_HB2,
		DPTX14_MAC_IP_SEC_PH_HB2_packet_header_hb_23_16(pkt[2]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_HB3,
		DPTX14_MAC_IP_SEC_PH_HB3_packet_header_hb_31_24(pkt[3]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_PB0,
		DPTX14_MAC_IP_SEC_PH_PB0_packet_header_pb_7_0(pkt[4]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_PB1,
		DPTX14_MAC_IP_SEC_PH_PB1_packet_header_pb_15_8(pkt[5]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_PB2,
		DPTX14_MAC_IP_SEC_PH_PB2_packet_header_pb_23_16(pkt[6]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_PH_PB3,
		DPTX14_MAC_IP_SEC_PH_PB3_packet_header_pb_31_24(pkt[7]));
	// rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_DBUF_CTRL, 2); // YC
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_DBUF_CTRL,
		DPTX14_MAC_IP_SEC_DBUF_CTRL_sec_db_vblank(1)
		| DPTX14_MAC_IP_SEC_DBUF_CTRL_sec_db(1));
}

void rtk_dptx_audio_set_info(struct rtk_dptx *dptx, uint16_t sample_freq,
				 uint16_t sample_size, uint16_t channel_cnt)
{
	uint8_t CC2CC1CC0 = channel_cnt - 1;
	uint8_t CT3CT2CT1CT0 = 0; //Audio Coding Type
	uint8_t SS1SS0 = 0;
	uint8_t SF2SF1SF0 = 0;
	uint8_t LSV30 = 0;
	uint8_t DM_INH = 0;
	uint8_t pkt[32];
	uint32_t i;

	switch (sample_freq) {
	case 0: //48k
		SF2SF1SF0 = 3;
		break;
	case 1: //44.1k
		SF2SF1SF0 = 2;
		break;
	case 2: //32k
		SF2SF1SF0 = 1;
		break;
	case 3: //96k
		SF2SF1SF0 = 5;
		break;
	case 8: //88.2k
		SF2SF1SF0 = 4;
		break;
	case 9: //192k
		SF2SF1SF0 = 7;
		break;
	case 10: //176.4k
		SF2SF1SF0 = 6;
		break;
	default:
		SF2SF1SF0 = 0;
		break;
	}

	switch (sample_size) {
	case 0: //16b
		SS1SS0 = 1;
		break;
	case 1: //18b
		SS1SS0 = 0;
		break;
	case 2: //20b
		SS1SS0 = 2;
		break;
	case 3: //24b
		SS1SS0 = 3;
		break;
	default:
		SS1SS0 = 0;
		break;
	}

	//SF2SF1SF0 = 0;
	//SS1SS0 = 0;

	//clear buffer===============================
	for (i = 0; i < 31; i++)
		pkt[i] = 0;

	//data=======================================
	pkt[0] = 0xf7 & ((CT3CT2CT1CT0 << 4) | (CC2CC1CC0));
	pkt[1] = (0x1f & (SF2SF1SF0 << 2)) | SS1SS0; // need check
	pkt[2] = 0;

	switch (CC2CC1CC0) {
	case 1: //2ch
		pkt[3] = 0x0; //FL + FR
		break;
	case 2: //3ch
		pkt[3] = 0x1; //FL + FR + LFE
		break;
	case 3: //4ch
		pkt[3] = 0x8; //FL + FR + RL + RR
		break;
	case 4: //5ch
		pkt[3] = 0x9; //FL + FR + LEF + RL + RR
		break;
	case 5: //6ch
		pkt[3] = 0xb;
		break;
	case 6: //7ch
		pkt[3] = 0xf;
		break;
	case 7: //8ch
		pkt[3] = 0x13;
		break;
	default: //2ch
		break;
	}
	pkt[4] = 0xf8 & ((DM_INH << 7) | (LSV30 << 3));

	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_RESERVED_8D,
		DPTX14_MAC_IP_RESERVED_8D_sdp_type_sel(1));
	for (i = 0; i < 31; i++) {
		rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_INFO_VSIF_ADDR_8B,
			DPTX14_MAC_IP_SEC_INFO_VSIF_ADDR_8B_sdp_addr(i));
		rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_INFO_VSIF_DATA_8C,
				DPTX14_MAC_IP_SEC_INFO_VSIF_DATA_8C_sdp_data(pkt[i]));
	}
}

static void rtk_dptx_audio_set_cs(struct rtk_dptx *dptx)
{
	uint8_t cs[5];

	// channel status
	cs[0] =	0x4C;
	cs[1] =	0xF6;
	cs[2] =	0x1C;
	cs[3] =	0x02;
	cs[4] =	0x02;

	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_0,
		DPTX14_MAC_IP_CH_STATUS_0_ch_status_7_0(cs[0]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_1,
		DPTX14_MAC_IP_CH_STATUS_1_ch_status_15_8(cs[1]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_2,
		DPTX14_MAC_IP_CH_STATUS_2_ch_status_23_16(cs[2]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_3,
		DPTX14_MAC_IP_CH_STATUS_3_ch_status_31_24(cs[3]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_4,
		DPTX14_MAC_IP_CH_STATUS_4_ch_status_39_32(cs[4]));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_CH12,
		DPTX14_MAC_IP_CH_STATUS_CH12_ch1_ch_num(0x0)
		| DPTX14_MAC_IP_CH_STATUS_CH12_ch2_ch_num(0x1));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_CH34,
		DPTX14_MAC_IP_CH_STATUS_CH34_ch3_ch_num(0)
		| DPTX14_MAC_IP_CH_STATUS_CH34_ch4_ch_num(0));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_CH56,
		DPTX14_MAC_IP_CH_STATUS_CH56_ch5_ch_num(0)
		| DPTX14_MAC_IP_CH_STATUS_CH56_ch6_ch_num(0));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_CH_STATUS_CH78,
		DPTX14_MAC_IP_CH_STATUS_CH78_ch7_ch_num(0)
		| DPTX14_MAC_IP_CH_STATUS_CH78_ch8_ch_num(0));
}

static void rtk_dptx_set_audio(struct rtk_dptx *dptx, uint16_t sample_freq,
				 uint16_t sample_size, uint16_t channel_cnt, int pll_spd)
{
	uint8_t pkt[8];
	uint32_t maud = 0;

	dev_info(dptx->dev, "dptx: set audio\n");

	// dp14 audio settings
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_SEC_FUNCTION_CTRL, 0,
		DPTX14_MAC_IP_SEC_FUNCTION_CTRL_infoframe_audio_en(1)
		| DPTX14_MAC_IP_SEC_FUNCTION_CTRL_audio_timestamp_en(1));

	// vb-id no audiomute flag off
	// rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_VBID, 0x6); // YC
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_VBID,
		DPTX14_MAC_IP_VBID_audiomute_flag_mask,
		DPTX14_MAC_IP_VBID_audiomute_flag(0));

	// SDP audio infoframe
	pkt[0] = 0x00;
	pkt[1] = 0x84;
	pkt[2] = 0x1B;
	pkt[3] = 0x44;
	pkt[4] = 0x00;
	pkt[5] = 0x84;
	pkt[6] = 0xD7;
	pkt[7] = 0xD1;
	rtk_dptx_audio_set_pkt(dptx, 12, pkt);
	rtk_dptx_audio_set_info(dptx, sample_freq, sample_size, channel_cnt);

	// SDP audio timestamp header
	//pkt[0] = 0x0D;
	pkt[0] = 0x00;
	pkt[1] = 0x01;
	pkt[2] = 0x17;
	pkt[3] = 0x44;
	//pkt[4] = 0x85;
	pkt[4] = 0x00;
	pkt[5] = 0x67;
	pkt[6] = 0x35;
	pkt[7] = 0xD1;
	rtk_dptx_audio_set_pkt(dptx, 1, pkt);

	maud = (512 * 48 * 32768) / (pll_spd * 1000);
	if (((512 * 48 * 32768) % (pll_spd * 1000)) > 0)
		maud += 1;
	dev_info(dptx->dev, "[%s] maud: %u\n", __func__, maud);
	// audio timestamp data byte
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_AUD_TS_MAUD_H,
		DPTX14_MAC_IP_AUD_TS_MAUD_H_maud_23_16(GET_MH_BYTE(maud)));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_AUD_TS_MAUD_M,
		DPTX14_MAC_IP_AUD_TS_MAUD_M_maud_15_8(GET_ML_BYTE(maud)));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_AUD_TS_MAUD_L,
		DPTX14_MAC_IP_AUD_TS_MAUD_L_maud_7_0(GET_L_BYTE(maud)));

	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_AUD_TS_NAUD_H,
		DPTX14_MAC_IP_AUD_TS_NAUD_H_naud_23_16(0x0));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_AUD_TS_NAUD_M,
		DPTX14_MAC_IP_AUD_TS_NAUD_M_naud_15_8(0x80));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_AUD_TS_NAUD_L,
		DPTX14_MAC_IP_AUD_TS_NAUD_L_naud_7_0(0x00));

	// SDP audio stream
	//pkt[0] = 0x0D;
	pkt[0] = 0x00;
	pkt[1] = 0x02;
	pkt[2] = 0x00;
	pkt[3] = 0x01;
	//pkt[4] = 0x85;
	pkt[4] = 0x00;
	pkt[5] = 0xCE;
	pkt[6] = 0x00;
	pkt[7] = 0x67;
	rtk_dptx_audio_set_pkt(dptx, 2, pkt);

	rtk_dptx_audio_set_cs(dptx);

	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_AUD_FUNCTION_CTRL1,
		DPTX14_MAC_IP_AUD_FUNCTION_CTRL1_audio_source(0)
		| DPTX14_MAC_IP_AUD_FUNCTION_CTRL1_eight_channel_layout(channel_cnt != 2)
		| DPTX14_MAC_IP_AUD_FUNCTION_CTRL1_sec_aud_ptr_errc(0)
		| DPTX14_MAC_IP_AUD_FUNCTION_CTRL1_audio_idle_send(1)
		| DPTX14_MAC_IP_AUD_FUNCTION_CTRL1_audio_sfifo_send_wl(0x4));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_AUD_FUNCTION_CTRL2,
		DPTX14_MAC_IP_AUD_FUNCTION_CTRL2_audio_long_packet(1)
		| DPTX14_MAC_IP_AUD_FUNCTION_CTRL2_Dummy(0)
		| DPTX14_MAC_IP_AUD_FUNCTION_CTRL2_max_long_packet_cnt(15));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_AUD_FUNCTION_CTRL3,
		DPTX14_MAC_IP_AUD_FUNCTION_CTRL3_audio_i2s_8ch_mask,
		DPTX14_MAC_IP_AUD_FUNCTION_CTRL3_audio_i2s_8ch(channel_cnt != 2));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_DPTX_I2S_CTRL,
		DPTX14_MAC_IP_DPTX_I2S_CTRL_i2s_0_switch(1)
		| DPTX14_MAC_IP_DPTX_I2S_CTRL_i2s_1_switch(1)
		| DPTX14_MAC_IP_DPTX_I2S_CTRL_i2s_2_switch(1)
		| DPTX14_MAC_IP_DPTX_I2S_CTRL_i2s_3_switch(1)
		| DPTX14_MAC_IP_DPTX_I2S_CTRL_pr_field_ch_exist(1)
		| DPTX14_MAC_IP_DPTX_I2S_CTRL_parity_type(0)
		| DPTX14_MAC_IP_DPTX_I2S_CTRL_as_payload_byte3_auto(1)
		| DPTX14_MAC_IP_DPTX_I2S_CTRL_ch_status_cnt_en(1));

	// rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_AUD_SAMPLE_CNT_HB, 0); // YC
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_AUD_SAMPLE_CNT_HB,
		DPTX14_MAC_IP_SEC_AUD_SAMPLE_CNT_HB_aud_freq_range(0xf)
		| DPTX14_MAC_IP_SEC_AUD_SAMPLE_CNT_HB_aud_sp_cnt_11_8(0xb));

	// rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_SEC_AUD_FREQDET_CTRL, 0x80); // YC
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_SEC_AUD_FREQDET_CTRL,
		DPTX14_MAC_IP_SEC_AUD_FREQDET_CTRL_afreq_det_mask,
		DPTX14_MAC_IP_SEC_AUD_FREQDET_CTRL_afreq_det(1));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DUMMY_98,
		0, DPTX14_MAC_IP_DUMMY_98_support_1s1d_en(1));
	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_DUMMY_99, 0,
		DPTX14_MAC_IP_DUMMY_99_info_rsv0_idle_send(1)
		| DPTX14_MAC_IP_DUMMY_99_i2s_decode_en(1));
	rtk_dptx14_mac_write(dptx, DPTX14_MAC_IP_DUMMY_90,
		DPTX14_MAC_IP_DUMMY_90_i2s_pr_md(0));

}

void rtk_dptx_phy_config_audio(struct rtk_dptx *dptx, struct audio_info *ainfo)
{
	// uint16_t sample_freq = 0;
	// uint16_t sample_size = 1;
	// uint16_t channel_cnt = 2;

	// --- aio settings ---
	//sample_freq = 0:48k, 1:44.1k, 2:32k, 3:96k, 4:24k, 5:22.05k, 6:16k, 7:64k, 8:88.2k, 9:192k, 10:176.4k, 11:8k, 12:11.025k, 13:12k, 14:128k, 15:384k
	//sample_size = 0:16bit, 1:18bit, 2:20bit, 3:24bit, 4:32bit

	rtk_dptx_set_audio(dptx, ainfo->sample_rate, ainfo->sample_width,
					 ainfo->channels, dptx->link_rate / 1000);

	// rtk_dptx_set_audio(dptx, sample_freq, sample_size,
	// 				 channel_cnt, dptx->link_rate / 1000);
	usleep_range(10, 20);
}
