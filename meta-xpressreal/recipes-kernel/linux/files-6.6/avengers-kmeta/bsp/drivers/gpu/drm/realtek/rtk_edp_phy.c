// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 RealTek Inc.
 */

#include <drm/drm_modes.h>
#include <linux/math64.h>

#include "rtk_edp.h"
#include "rtk_edp_reg.h"
#include "rtk_crt_reg.h"
#include "rtk_dp_utils.h"

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

void rtk_edp_phy_dppll_setting(struct rtk_edp *edp, struct drm_display_mode *mode)
{
	uint32_t plltmds_ncode = 0, plltmds_fcode = 0;
	uint32_t edp_ncode_ssc = 0, edp_fcode_ssc = 0, edp_gran_est = 0;

	switch (edp->link_rate) {
	case DP_LINK_RATE_1_62:
		plltmds_fcode = 0x0;
		plltmds_ncode = 0x1b;
		edp_ncode_ssc = 0x1a;
		edp_fcode_ssc = 0x726;
		edp_gran_est = 0x03e8;
		break;
	case DP_LINK_RATE_2_7:
		plltmds_fcode = 0x0;
		plltmds_ncode = 0x2f;
		edp_ncode_ssc = 0x2e;
		edp_fcode_ssc = 0x622;
		edp_gran_est = 0x92e;
		break;
	case DP_LINK_RATE_5_4:
		plltmds_fcode = 0x0;
		plltmds_ncode = 0x61;
		edp_ncode_ssc = 0x60;
		edp_fcode_ssc = 0x41a;
		edp_gran_est = 0x1324;
		break;
	}

	// PIXEL/Link clock PLL setting
	rtk_edp_crt_write(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL0,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_time_rdy_ckout(0x2)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_time2_rst_width(0x1)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_pcr_rst_n(0)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_time0_ck(0x3)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_f390k(0x1)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_pll_en(1)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_en_wdog(0)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_ssc_ckinv(1)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_fcode(plltmds_fcode)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_ncode(plltmds_ncode));
	//[0]=pll OC disable
	rtk_edp_crt_write(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL1, 0x50203ffe);
	rtk_edp_crt_write(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL3,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ssc_en(0)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_fcode_ssc(edp_fcode_ssc)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ncode_ssc(edp_ncode_ssc));
	rtk_edp_crt_write(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL4,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_RSTB_EDP(1)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_gran_auto_rst(0)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_dot_gran(0x4)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_gran_est(edp_gran_est)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_gran_auto_rst(1));
	rtk_edp_crt_update(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL1,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL1_oc_en_mask,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL1_oc_en(1));
	msleep_interruptible(10); // simulation insert delay 20ms

	rtk_edp_crt_write(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL5,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL5_REG_BYPASS_DIVN(1)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL5_FCW_SSC_DEFAULT_EDP(0));
	rtk_edp_crt_write(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL5,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL5_REG_BYPASS_DIVN(0)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL5_FCW_SSC_DEFAULT_EDP(0));
	msleep_interruptible(1);

	rtk_edp_crt_write(edp, SYS_EDPTX_PHY_CTRL8, 0xc8);
	rtk_edp_crt_write(edp, SYS_EDPTX_PHY_CTRL9, 0xc8);
}

int rtk_edp_phy_pixelpll_setting(struct rtk_edp *edp,
							 struct drm_display_mode *mode)
{
	struct device *dev = edp->dev;
	int i;
	int dpll_m = 0, f_code = 0, pll_o = 0;
	uint32_t edp_ncode_ssc = 0, edp_fcode_ssc = 0, edp_gran_est = 0;
	uint32_t ctrl0_val = 0, ctrl7_val = 0;
	uint32_t pixel_clk = (uint32_t) mode->clock;
	uint32_t table_pixel_clk;
	uint64_t mod_percentage_10000, mf_value_10000, mf_value;
	uint32_t mf_value_rem;
	uint32_t dpll_m_new, f_code_new, fvco;

	// table: <DPLL_M, F code, DPLL_O, Pixel Freq>
	// Use pixel freq to look up the table
	for (i = 0; i < RTK_DP_PIXEL_PLL_TABLE_SIZE; i++) {
		if (pixel_clk <= RTK_DP_PIXEL_PLL_TABLE[i][3])
			break;
	}
	dpll_m = RTK_DP_PIXEL_PLL_TABLE[i][0];
	f_code = RTK_DP_PIXEL_PLL_TABLE[i][1];
	pll_o = RTK_DP_PIXEL_PLL_TABLE[i][2];
	table_pixel_clk = RTK_DP_PIXEL_PLL_TABLE[i][3];
	dev_info(dev, "[%s] dpll_m: 0x%x, f_code: 0x%x, dpll_o: 0x%x, table clk: %u\n",
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

	switch (edp->link_rate) {
	case DP_LINK_RATE_1_62:
		ctrl0_val = 0x03087891;
		ctrl7_val = 0x133708;
		edp_ncode_ssc = 0x1a;
		edp_fcode_ssc = 0x726;
		edp_gran_est = 0x03e8;
		break;
	case DP_LINK_RATE_2_7:
		ctrl0_val = 0x03087891;
		ctrl7_val = 0x163708;
		edp_ncode_ssc = 0x2e;
		edp_fcode_ssc = 0x622;
		edp_gran_est = 0x92e;
		break;
	case DP_LINK_RATE_5_4:
		ctrl0_val = 0x030B7891;
		ctrl7_val = 0x573708;
		edp_ncode_ssc = 0x60;
		edp_fcode_ssc = 0x41a;
		edp_gran_est = 0x1324;
		break;
	}

	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL2,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_POW_mask,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_POW(1));
	rtk_edp_crt_write(edp, SYS_EDPTX_PHY_CTRL5, 0x2CD80039);
	rtk_edp_crt_write(edp, SYS_EDPTX_PHY_CTRL6, 0x2CD80039);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL2,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_RSTB_mask,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_RSTB(1));
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL0,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_CMU_BIG_KVCO_mask
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_EN_LV_LDO_mask
		| SYS_EDPTX_PHY_CTRL0_REG_POW_EDP_mask, 0);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL0,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_POW_mask
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_RSTB_mask, 0);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL3,
		SYS_EDPTX_PHY_CTRL3_REG_TXPLL_PREDIV_BYPASS_mask, 0);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL0,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_RSTB_mask, 0);
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL6,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL6_DPLL_SSC_RSTB_mask, 0);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL0,
		SYS_EDPTX_PHY_CTRL0_REG_POW_EDP_mask
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_EN_LV_LDO_mask
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_CMU_BIG_KVCO_mask,
		SYS_EDPTX_PHY_CTRL0_REG_POW_EDP(1)
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_EN_LV_LDO(1)
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_CMU_BIG_KVCO(1));
	msleep_interruptible(1);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL4, 0, 0xd);
	rtk_edp_crt_write(edp, SYS_EDPTX_PHY_CTRL8, 0x20c8);
	rtk_edp_crt_write(edp, SYS_EDPTX_PHY_CTRL9, 0x20c8);

	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL2,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_RSTB_mask,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_RSTB(1));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL0,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Oc_en_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Oc_en(0));
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL2,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_O_mask,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_O(pll_o));
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL2,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_BPSIN_mask,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_BPSIN(1));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL3,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL3_Ncode_t_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL3_Ncode_t(0x14));

	// ssc setting
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL6,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL6_DPLL_SSC_RSTB_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL6_DPLL_SSC_RSTB(1));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL6,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL6_DPLL_SSC_RSTB_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL6_DPLL_SSC_RSTB(1));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL2,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL2_Bypass_pi_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL2_Bypass_pi(0));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL4,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL4_Dot_gran_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL4_Dot_gran(0x3));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL2,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL2_En_pi_debug_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL2_En_pi_debug(0));
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL2,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_N_mask,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_N(0x1));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL3,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL3_Ncode_t_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL3_Ncode_t(dpll_m));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL0,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Fcode_t_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Fcode_t(f_code));
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL2,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_N_mask,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_BPSPI(0));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL3,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL3_Ncode_ssc_mask
		| SYS_DPLL_SSC_DIG_EDPTX_CTRL3_Fcode_ssc_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL3_Ncode_ssc(0x13)
		| SYS_DPLL_SSC_DIG_EDPTX_CTRL3_Fcode_ssc(0x305));
	rtk_edp_crt_write(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL4, 0x03730000);
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL1,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL1_Hs_oc_stop_diff_mask
		| SYS_DPLL_SSC_DIG_EDPTX_CTRL1_Oc_done_delay_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL1_Hs_oc_stop_diff(0x0)
		| SYS_DPLL_SSC_DIG_EDPTX_CTRL1_Oc_done_delay(0x1));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL0,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Oc_step_set_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Oc_step_set(0x0a9));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL2,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL2_Sdm_order_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL2_Sdm_order(0x1));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL1,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL1_Sel_oc_mode_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL1_Sel_oc_mode(0x0));

	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL2,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL2_Sdm_order_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL2_Sdm_order(0x1));
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL0,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Oc_en_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Oc_en(1));
	usleep_range(1000, 2000);
	rtk_edp_crt_update(edp, SYS_DPLL_SSC_DIG_EDPTX_CTRL0,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Oc_en_mask,
		SYS_DPLL_SSC_DIG_EDPTX_CTRL0_Oc_en(0));
	usleep_range(1000, 2000);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL3,
		SYS_EDPTX_PHY_CTRL3_REG_DPLL_FREEZE_mask,
		SYS_EDPTX_PHY_CTRL3_REG_DPLL_FREEZE(0));
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL0,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_POW_mask
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_RSTB_mask,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_POW(1)
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_RSTB(0));
	usleep_range(1000, 2000);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL0,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_POW_mask
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_BW_SET_mask,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_POW(1)
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_BW_SET(0));
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL3,
		SYS_EDPTX_PHY_CTRL3_REG_TXPLL_CKIN_DIV_N_mask
		| SYS_EDPTX_PHY_CTRL3_REG_TXPLL_PREDIV_BYPASS_mask,
		SYS_EDPTX_PHY_CTRL3_REG_TXPLL_CKIN_DIV_N(0)
		| SYS_EDPTX_PHY_CTRL3_REG_TXPLL_PREDIV_BYPASS(1));
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL0,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_RSTB_mask,
		SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_RSTB(1));
	usleep_range(1000, 2000);

	rtk_edp_crt_write(edp, SYS_EDPTX_PHY_CTRL0, ctrl0_val);
	rtk_edp_crt_write(edp, SYS_EDPTX_PHY_CTRL7, ctrl7_val);
	rtk_edp_crt_update(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL4,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_RSTB_EDP_mask
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_gran_est_mask,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_RSTB_EDP(1)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL4_gran_est(edp_gran_est));
	rtk_edp_crt_update(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL1, 0,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL1_oc_en_mask);
	rtk_edp_crt_update(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL3,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ssc_en_mask
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_fcode_ssc_mask
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ncode_ssc_mask,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_fcode_ssc(edp_fcode_ssc)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ncode_ssc(edp_ncode_ssc));
	usleep_range(1000, 2000);
	rtk_edp_crt_update(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL0, 0,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL0_pll_en_mask);
	rtk_edp_crt_update(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL1,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL1_oc_en_mask, 0);

	//enable ssc
	rtk_edp_crt_update(edp, SYS_TXPLL_SSC_DIG_EDPTX_CTRL3,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ssc_en_mask
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_fcode_ssc_mask
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ncode_ssc_mask,
		SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ssc_en(0)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_fcode_ssc(edp_fcode_ssc)
		| SYS_TXPLL_SSC_DIG_EDPTX_CTRL3_ncode_ssc(edp_ncode_ssc));

	reset_control_deassert(edp->rstc_disp);
	//clk_en_dptx_pxl=1
	rtk_edp_crt_write(edp, SYS_CLOCK_ENABLE8,
		SYS_CLOCK_ENABLE8_write_en16(1) | SYS_CLOCK_ENABLE8_clk_en_dptx_pxl(1));

	rtk_edp_update(edp, DPTX_SFIFO_CTRL0,
		DP_IP_DPTX_SFIFO_CTRL0_tx_en_mask,
		DP_IP_DPTX_SFIFO_CTRL0_tx_en(1));
	return 0;
}

static void rtk_edp_sst_setting(struct rtk_edp *edp,
							struct drm_display_mode *mode)
{
	uint64_t mvid =  0;
	uint32_t nvid = 32768;
	int bpc = (edp->bpc == 6) ? RTK_DP_COLORBIT_6 :
			 (edp->bpc == 8) ? RTK_DP_COLORBIT_8 :
			 (edp->bpc == 10) ? RTK_DP_COLORBIT_10 :
			 (edp->bpc == 12) ? RTK_DP_COLORBIT_12 :
			 (edp->bpc == 16) ? RTK_DP_COLORBIT_16 : RTK_DP_COLORBIT_8;
	int component_format = (edp->color_format == RTK_COLOR_FORMAT_RGB) ? 0x0 :
			 (edp->color_format == RTK_COLOR_FORMAT_YUV444) ? 0x2 :
			 (edp->color_format == RTK_COLOR_FORMAT_YUV422) ? 0x1 : 0x0;
	uint32_t link_rate = edp->link_rate;

	mvid = div_u64(mul_u32_u32(mode->clock, nvid), link_rate);

	dev_info(edp->dev, "[%s] MVID: %llu\n", __func__, mvid);

	rtk_edp_update(edp, MN_M_VID_H,
		DP_IP_MN_M_VID_H_mvid_23_16_mask,
		DP_IP_MN_M_VID_H_mvid_23_16(GET_MH_BYTE(mvid)));
	rtk_edp_update(edp, MN_M_VID_M,
		DP_IP_MN_M_VID_M_mvid_15_8_mask,
		DP_IP_MN_M_VID_M_mvid_15_8(GET_ML_BYTE(mvid)));
	rtk_edp_update(edp, MN_M_VID_L,
		DP_IP_MN_M_VID_L_mvid_7_0_mask,
		DP_IP_MN_M_VID_L_mvid_7_0(GET_L_BYTE(mvid)));
	rtk_edp_update(edp, MN_N_VID_H,
		DP_IP_MN_N_VID_H_nvid_23_16_mask,
		DP_IP_MN_N_VID_H_nvid_23_16(GET_MH_BYTE(nvid)));
	rtk_edp_update(edp, MN_N_VID_M,
		DP_IP_MN_N_VID_M_nvid_15_8_mask,
		DP_IP_MN_N_VID_M_nvid_15_8(GET_ML_BYTE(nvid)));
	rtk_edp_update(edp, MN_N_VID_L,
		DP_IP_MN_N_VID_L_nvid_7_0_mask,
		DP_IP_MN_N_VID_L_nvid_7_0(GET_L_BYTE(nvid)));

	rtk_edp_update(edp, MN_VID_AUTO_EN_1,
		DP_IP_MN_VID_AUTO_EN_1_mn_vid_auto_en_mask
		| DP_IP_MN_VID_AUTO_EN_1_mn_vid_db_mask,
		DP_IP_MN_VID_AUTO_EN_1_mn_vid_auto_en(0)
		| DP_IP_MN_VID_AUTO_EN_1_mn_vid_db(0x1)); // 0x40
	rtk_edp_update(edp, MSA_MISC0,
		DP_IP_MSA_MISC0_colorbit_mask
		| DP_IP_MSA_MISC0_ycc_col_mask
		| DP_IP_MSA_MISC0_dyn_range_mask
		| DP_IP_MSA_MISC0_component_format_mask,
		DP_IP_MSA_MISC0_colorbit(bpc)
		| DP_IP_MSA_MISC0_ycc_col(0x0) // ITU-R BT601-5
		| DP_IP_MSA_MISC0_dyn_range(0x0) // VESA
		| DP_IP_MSA_MISC0_component_format(component_format));
	rtk_edp_update(edp, MSA_CTRL,
		DP_IP_MSA_CTRL_msa_db_mask,
		DP_IP_MSA_CTRL_msa_db(1));
}

static void rtk_edp_sst_dpformat_setting(struct rtk_edp *edp,
							struct drm_display_mode *mode)
{
	/* RGB,YUV444: 3, YUV422: 2, YUV420: 1.5*/
	uint32_t comp_x = (edp->color_format == RTK_COLOR_FORMAT_YUV422) ? 2 : 3;
	uint32_t comp_y = (edp->color_format == RTK_COLOR_FORMAT_YUV420) ? 2 : 1;
	uint32_t bpc = (uint32_t) edp->bpc;
	uint32_t v_data_per_line;
	uint32_t tu_size, tu_size_decimal;
	uint32_t tu_size_x, tu_size_y;
	uint32_t link_rate = (uint32_t) edp->link_rate / 1000;
	uint32_t lane_count = (uint32_t) edp->lane_count;
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
	tu_size =  tu_size_x / tu_size_y;
	tu_size_decimal = tu_size_x * 10 / tu_size_y % 10;

	hdelay = (v_data > normal_image) ? // normal
		(normal_image * comp_y / (bpc * comp_x) + hsync_len + hback_porch) * link_rate * 1000 / clock + 1 :
		// small image
		(hdisplay / 2 + hsync_len + hback_porch) * link_rate * 1000 / clock + 1;

	dev_info(edp->dev, "[%s] v_data_per_line: %u, tu_size: %u.%u, hdelay: %u\n",
		 __func__, v_data_per_line, tu_size, tu_size_decimal, hdelay);

	// eDPTX video setting
	rtk_edp_update(edp, V_DATA_PER_LINE0,
		DP_IP_V_DATA_PER_LINE0_v_data_per_line_14_8_mask,
		DP_IP_V_DATA_PER_LINE0_v_data_per_line_14_8(GET_ML_BYTE(v_data_per_line)));
	rtk_edp_update(edp, V_DATA_PER_LINE1,
		DP_IP_V_DATA_PER_LINE1_v_data_per_line_7_0_mask,
		DP_IP_V_DATA_PER_LINE1_v_data_per_line_7_0(GET_L_BYTE(v_data_per_line)));
	rtk_edp_update(edp, TU_DATA_SIZE0,
		DP_IP_TU_DATA_SIZE0_tu_data_size_9_3_mask,
		DP_IP_TU_DATA_SIZE0_tu_data_size_9_3(tu_size));
	rtk_edp_update(edp, TU_DATA_SIZE1,
		DP_IP_TU_DATA_SIZE1_tu_data_size_2_0_mask,
		DP_IP_TU_DATA_SIZE1_tu_data_size_2_0(tu_size_decimal));
	rtk_edp_update(edp, HDEALY0,
		DP_IP_HDEALY0_hdelay_14_8_mask,
		DP_IP_HDEALY0_hdelay_14_8(GET_ML_BYTE(hdelay)));
	rtk_edp_update(edp, HDEALY1,
		DP_IP_HDEALY1_hdelay_7_0_mask,
		DP_IP_HDEALY1_hdelay_7_0(GET_L_BYTE(hdelay)));
	rtk_edp_update(edp, LFIFO_WL_SET,
		DP_IP_LFIFO_WL_SET_wl_mid_regen_mask
		| DP_IP_LFIFO_WL_SET_wl_mid_set_mask,
		DP_IP_LFIFO_WL_SET_wl_mid_regen(1)
		| DP_IP_LFIFO_WL_SET_wl_mid_set(0x40));
}

static void rtk_edp_config_lane(struct rtk_edp *edp)
{
	uint32_t ctrl_lane_num = 1;
	uint32_t v2analog = 0;

	switch (edp->lane_count) {
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

	rtk_edp_update(edp, DP_PHY_CTRL,
		DP_IP_DP_PHY_CTRL_v2analog_mask
		| DP_IP_DP_PHY_CTRL_lane_num_mask
		| DP_IP_DP_PHY_CTRL_mst_en_mask,
		DP_IP_DP_PHY_CTRL_v2analog(v2analog)
		| DP_IP_DP_PHY_CTRL_lane_num(ctrl_lane_num)
		| DP_IP_DP_PHY_CTRL_mst_en(0x0));
	rtk_edp_update(edp, DP_MAC_CTRL,
		DP_IP_DP_MAC_CTRL_enhance_md_mask
		| DP_IP_DP_MAC_CTRL_lane_num_mask,
		DP_IP_DP_MAC_CTRL_enhance_md(1)
		| DP_IP_DP_MAC_CTRL_lane_num(ctrl_lane_num));
}

static void rtk_edp_config_timing_gen(struct rtk_edp *edp,
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

	rtk_edp_wrap_update(edp, EDPTX_DH_WIDTH,
		EDPTX_DH_WIDTH_dh_width_mask,
		EDPTX_DH_WIDTH_dh_width(hsync_len));
	rtk_edp_wrap_update(edp, EDPTX_DH_TOTAL,
		EDPTX_DH_TOTAL_dh_total_mask
		| EDPTX_DH_TOTAL_dh_total_last_line_mask,
		EDPTX_DH_TOTAL_dh_total(htotal)
		| EDPTX_DH_TOTAL_dh_total_last_line(htotal));
	rtk_edp_wrap_update(edp, EDPTX_DH_DEN_START_END,
		EDPTX_DH_DEN_START_END_dh_den_sta_mask
		| EDPTX_DH_DEN_START_END_dh_den_end_mask,
		EDPTX_DH_DEN_START_END_dh_den_sta(dh_den_sta)
		| EDPTX_DH_DEN_START_END_dh_den_end(dh_den_end));

	rtk_edp_write(edp, ARBITER_SEC_END_CNT_HB, 0x0);
	rtk_edp_write(edp, ARBITER_SEC_END_CNT_LB, 0x10); // SEC_END_CNT = 0x10

	rtk_edp_wrap_update(edp, EDPTX_DV_DEN_START_END_FIELD1,
		EDPTX_DV_DEN_START_END_FIELD1_dv_den_sta_field1_mask
		| EDPTX_DV_DEN_START_END_FIELD1_dv_den_end_field1_mask,
		EDPTX_DV_DEN_START_END_FIELD1_dv_den_sta_field1(dv_den_sta_field1)
		| EDPTX_DV_DEN_START_END_FIELD1_dv_den_end_field1(dv_den_end_field1));
	rtk_edp_wrap_update(edp, EDPTX_DV_TOTAL,
		EDPTX_DV_TOTAL_dv_total_mask,
		EDPTX_DV_TOTAL_dv_total(vtotal));
	rtk_edp_wrap_update(edp, EDPTX_DV_VS_START_END_FIELD1,
		EDPTX_DV_VS_START_END_FIELD1_dv_vs_sta_field1_mask
		| EDPTX_DV_VS_START_END_FIELD1_dv_vs_end_field1_mask,
		EDPTX_DV_VS_START_END_FIELD1_dv_vs_sta_field1(dv_vs_sta_field1)
		| EDPTX_DV_VS_START_END_FIELD1_dv_vs_end_field1(dv_vs_end_field1));
	rtk_edp_wrap_update(edp, EDPTX_DH_VS_ADJ_FIELD1,
		EDPTX_DH_VS_ADJ_FIELD1_dh_vs_adj_field1_mask,
		EDPTX_DH_VS_ADJ_FIELD1_dh_vs_adj_field1(2));
}

void rtk_edp_phy_config_video_timing(struct rtk_edp *edp,
								 struct drm_display_mode *mode)
{
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

	dev_info(edp->dev, "[%s] h: %u, hfp: %u, hbp: %u, hsync: %u, hsp: %d\n",
		 __func__, hactive, hfront_porch, hback_porch, hsync_len, hsp);
	dev_info(edp->dev, "[%s] v: %u, vfp: %u, vbp: %u, vsync: %u, vsp: %d\n",
		 __func__, vactive, vfront_porch, vback_porch, vsync_len, vsp);

	// sst msa setting
	rtk_edp_update(edp, MN_STRM_ATTR_HTT_M,
		DP_IP_MN_STRM_ATTR_HTT_M_htotal_15_8_mask,
		DP_IP_MN_STRM_ATTR_HTT_M_htotal_15_8(GET_ML_BYTE(htotal)));
	rtk_edp_update(edp, MN_STRM_ATTR_HTT_L,
		DP_IP_MN_STRM_ATTR_HTT_L_htotal_7_0_mask,
		DP_IP_MN_STRM_ATTR_HTT_L_htotal_7_0(GET_L_BYTE(htotal)));
	rtk_edp_update(edp, MN_STRM_ATTR_HST_M,
		DP_IP_MN_STRM_ATTR_HST_M_hstart_15_8_mask, // hstart = hs_width + hs_bp
		DP_IP_MN_STRM_ATTR_HST_M_hstart_15_8(GET_ML_BYTE(hsync_len + hback_porch)));
	rtk_edp_update(edp, MN_STRM_ATTR_HST_L,
		DP_IP_MN_STRM_ATTR_HST_L_hstart_7_0_mask,
		DP_IP_MN_STRM_ATTR_HST_L_hstart_7_0(GET_L_BYTE(hsync_len + hback_porch)));
	rtk_edp_update(edp, MN_STRM_ATTR_HWD_M,
		DP_IP_MN_STRM_ATTR_HWD_M_hwidth_15_8_mask,
		DP_IP_MN_STRM_ATTR_HWD_M_hwidth_15_8(GET_ML_BYTE(hactive)));
	rtk_edp_update(edp, MN_STRM_ATTR_HWD_L,
		DP_IP_MN_STRM_ATTR_HWD_L_hwidth_7_0_mask,
		DP_IP_MN_STRM_ATTR_HWD_L_hwidth_7_0(GET_L_BYTE(hactive)));
	rtk_edp_update(edp, MN_STRM_ATTR_HSW_M,
		DP_IP_MN_STRM_ATTR_HSW_M_hsp_mask
		| DP_IP_MN_STRM_ATTR_HSW_M_hsw_14_8_mask,
		DP_IP_MN_STRM_ATTR_HSW_M_hsp(hsp)
		| DP_IP_MN_STRM_ATTR_HSW_M_hsw_14_8(GET_ML_BYTE(hsync_len)));
	rtk_edp_update(edp, MN_STRM_ATTR_HSW_L,
		DP_IP_MN_STRM_ATTR_HSW_L_hsw_7_0_mask,
		DP_IP_MN_STRM_ATTR_HSW_L_hsw_7_0(GET_L_BYTE(hsync_len)));
	rtk_edp_update(edp, MN_STRM_ATTR_VTTE_M,
		DP_IP_MN_STRM_ATTR_VTTE_M_vtotal_15_8_mask,
		DP_IP_MN_STRM_ATTR_VTTE_M_vtotal_15_8(GET_ML_BYTE(vtotal)));
	rtk_edp_update(edp, MN_STRM_ATTR_VTTE_L,
		DP_IP_MN_STRM_ATTR_VTTE_L_vtotal_7_0_mask,
		DP_IP_MN_STRM_ATTR_VTTE_L_vtotal_7_0(GET_L_BYTE(vtotal)));
	rtk_edp_update(edp, MN_STRM_ATTR_VST_M,
		DP_IP_MN_STRM_ATTR_VST_M_vstart_15_8_mask,
		DP_IP_MN_STRM_ATTR_VST_M_vstart_15_8(GET_ML_BYTE(vsync_len + vback_porch)));
	rtk_edp_update(edp, MN_STRM_ATTR_VST_L,
		DP_IP_MN_STRM_ATTR_VST_L_vstart_7_0_mask,
		DP_IP_MN_STRM_ATTR_VST_L_vstart_7_0(GET_L_BYTE(vsync_len + vback_porch)));
	rtk_edp_update(edp, MN_STRM_ATTR_VHT_M,
		DP_IP_MN_STRM_ATTR_VHT_M_vheight_15_8_mask,
		DP_IP_MN_STRM_ATTR_VHT_M_vheight_15_8(GET_ML_BYTE(vactive)));
	rtk_edp_update(edp, MN_STRM_ATTR_VHT_L,
		DP_IP_MN_STRM_ATTR_VHT_L_vheight_7_0_mask,
		DP_IP_MN_STRM_ATTR_VHT_L_vheight_7_0(GET_L_BYTE(vactive)));
	rtk_edp_update(edp, MN_STRM_ATTR_VSW_M,
		DP_IP_MN_STRM_ATTR_VSW_M_vsp_mask
		| DP_IP_MN_STRM_ATTR_VSW_M_vsw_14_8_mask,
		DP_IP_MN_STRM_ATTR_VSW_M_vsp(vsp)
		| DP_IP_MN_STRM_ATTR_VSW_M_vsw_14_8(GET_ML_BYTE(vsync_len)));
	rtk_edp_update(edp, MN_STRM_ATTR_VSW_L,
		DP_IP_MN_STRM_ATTR_VSW_L_vsw_7_0_mask,
		DP_IP_MN_STRM_ATTR_VSW_L_vsw_7_0(GET_L_BYTE(vsync_len)));

	rtk_edp_sst_setting(edp, mode);
	rtk_edp_sst_dpformat_setting(edp, mode);
	rtk_edp_config_lane(edp);

	rtk_edp_update(edp, DPTX_PHY_CTRL,
		DP_IP_DPTX_PHY_CTRL_sr_insert_en_mask
		| DP_IP_DPTX_PHY_CTRL_dptx_skew_en_mask
		| DP_IP_DPTX_PHY_CTRL_dptx_scb_en_mask,
		DP_IP_DPTX_PHY_CTRL_sr_insert_en(1)
		| DP_IP_DPTX_PHY_CTRL_dptx_skew_en(1)
		| DP_IP_DPTX_PHY_CTRL_dptx_scb_en(1));

	rtk_edp_config_timing_gen(edp, mode);
}

void rtk_edp_phy_set_scramble(struct rtk_edp *edp, bool scramble)
{
	rtk_edp_update(edp, DPTX_PHY_CTRL,
		DP_IP_DPTX_PHY_CTRL_dptx_scb_en_mask,
		DP_IP_DPTX_PHY_CTRL_dptx_scb_en(scramble));
}

void rtk_edp_phy_set_pattern(struct rtk_edp *edp, int pattern)
{
	rtk_edp_update(edp, DPTX_ML_PAT_SEL,
		DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_mask
		| DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en_mask
		| DP_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf_mask,
		DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DP_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(0));
	rtk_edp_update(edp, DPTX_ML_PAT_SEL,
		DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_mask
		| DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en_mask
		| DP_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf_mask,
		DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel(pattern)
		| DP_IP_DPTX_ML_PAT_SEL_tx_ml_pat_sel_en(1)
		| DP_IP_DPTX_ML_PAT_SEL_pat_sel_dbuf(1));
}

int rtk_edp_mac_signal_setting(struct rtk_edp *edp,
						 struct rtk_edp_train_signal signals[4])
{
	struct device *dev = edp->dev;
	u32 table_size;
	const u32 *table;
	u32 drv[4][4][3];
	int i, j, k, l = 0;
	uint8_t swing, emphasis;
	/*	emp		0			1		2		3
	 *	sw
	 *	0		v1, v2, v3	...
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

	for (i = 0; i < edp->lane_count; i++) {
		swing = signals[i].swing;
		emphasis = signals[i].emphasis;
		rtk_edp_update(edp, RIV0,
			(0x1 << (16 + i)) | 0xF << (i * 4),
			drv[swing][emphasis][0] << (16 + i)
			| drv[swing][emphasis][1] << (i * 4) | 0x00300000);
		rtk_edp_update(edp, RIV1,
			(0xF << (i * 4)) | 0xFF000000,
			drv[swing][emphasis][2] << (i * 4) | 0x1F000000);
	}

	return 0;
}

int rtk_edp_aphy_signal_setting(struct rtk_edp *edp,
						 struct rtk_edp_train_signal signals[4])
{
	struct device *dev = edp->dev;
	u32 table_size;
	const u32 *table;
	u32 sp[4][4][4];
	u32 *tx_level;
	int i, j, k, l = 0;
	uint8_t swing = 0, emphasis = 0;

	/* -------- from u3DP Voltage swing_20250315.xlsx --------
	 *	emp		0		1		2		3
	 *	sw
	 *	0		sp00	sp01	sp02	sp03
	 *	1		sp10	sp11	sp12	x
	 *	2		sp20	sp21	x		x
	 *	3		sp30	x		x		x
	 */
	/* spxx: { iDRV[6], iDRV[5:0], tx_level[2][5:0], tx_level[3] } */
	table = of_get_property(dev->of_node,
		 (edp->link_rate == DP_LINK_RATE_1_62) ? "rbr-sw-emp-table" :
		 (edp->link_rate == DP_LINK_RATE_2_7) ? "hbr-sw-emp-table" :
		 (edp->link_rate == DP_LINK_RATE_5_4) ? "hbr2-sw-emp-table" :
		 "hbr2-sw-emp-table", &table_size);
	if (!table || table_size != sizeof(sp)) {
		dev_err(dev, "Failed to read sw-emp-table\n");
		return -EINVAL;
	}

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			for (k = 0; k < 4; k++)
				sp[i][j][k] = be32_to_cpu(table[l++]);

	swing = signals[0].swing;
	emphasis = signals[0].emphasis;
	tx_level = sp[swing][emphasis];
	dev_info(dev, "[%s] , sw: %u, emp: %u, level: %x %x %x %x\n",
				 __func__, swing, emphasis, tx_level[0],
				 tx_level[1], tx_level[2], tx_level[3]);

	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL8,
		 0x1 << 7, tx_level[0] << 7);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL9,
		 0x1 << 7, tx_level[0] << 7);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL5,
		 0x3f << 14 | 0x3f << 6,
		 tx_level[1] << 14 | tx_level[2] << 6);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL6,
		 0x3f << 14 | 0x3f << 6,
		 tx_level[1] << 14 | tx_level[2] << 6);
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL7,
		 0xf << 8, tx_level[3] << 7);

	return 0;
}

void rtk_edp_phy_config_csc(struct rtk_edp *edp)
{
	rtk_edp_wrap_update(edp, EDPTX_MAIN,
		EDPTX_MAIN_avg_mode_mask
		| EDPTX_MAIN_pixel_8bit_mask
		| EDPTX_MAIN_color_mode_mask
		| EDPTX_MAIN_csc_enable_mask,
		EDPTX_MAIN_avg_mode(0)
		| EDPTX_MAIN_pixel_8bit(0x1) // 0:10bit, 1:8bit
		| EDPTX_MAIN_color_mode(edp->color_format)
		| EDPTX_MAIN_csc_enable((edp->color_format) ? 1 : 0));

	// eDPTX CSC (color transform matrix) setting.
	rtk_edp_wrap_update(edp, EDPTX_CSC1,
		EDPTX_CSC1_m01_mask
		| EDPTX_CSC1_m00_mask,
		EDPTX_CSC1_m01(EDP_TX_1_SETTING_CONFIG_CSC_M01)
		| EDPTX_CSC1_m00(EDP_TX_1_SETTING_CONFIG_CSC_M00));
	rtk_edp_wrap_update(edp, EDPTX_CSC2,
		EDPTX_CSC2_m10_mask
		| EDPTX_CSC2_m02_mask,
		EDPTX_CSC2_m10(EDP_TX_1_SETTING_CONFIG_CSC_M10)
		| EDPTX_CSC2_m02(EDP_TX_1_SETTING_CONFIG_CSC_M02));
	rtk_edp_wrap_update(edp, EDPTX_CSC3,
		EDPTX_CSC3_m12_mask
		| EDPTX_CSC3_m11_mask,
		EDPTX_CSC3_m12(EDP_TX_1_SETTING_CONFIG_CSC_M12)
		| EDPTX_CSC3_m11(EDP_TX_1_SETTING_CONFIG_CSC_M11));
	rtk_edp_wrap_update(edp, EDPTX_CSC4,
		EDPTX_CSC4_m21_mask
		| EDPTX_CSC4_m20_mask,
		EDPTX_CSC4_m21(EDP_TX_1_SETTING_CONFIG_CSC_M21)
		| EDPTX_CSC4_m20(EDP_TX_1_SETTING_CONFIG_CSC_M20));

	rtk_edp_wrap_update(edp, EDPTX_CSC5,
		EDPTX_CSC5_m22_mask,
		EDPTX_CSC5_m22(EDP_TX_1_SETTING_CONFIG_CSC_M22));
	rtk_edp_wrap_update(edp, EDPTX_CSC6,
		EDPTX_CSC6_a1_mask
		| EDPTX_CSC6_a0_mask,
		EDPTX_CSC6_a1(EDP_TX_1_SETTING_CONFIG_CSC_A1)
		| EDPTX_CSC6_a0(EDP_TX_1_SETTING_CONFIG_CSC_A0));
	rtk_edp_wrap_update(edp, EDPTX_CSC7,
		EDPTX_CSC7_a2_mask,
		EDPTX_CSC7_a2(EDP_TX_1_SETTING_CONFIG_CSC_A2));
}

void rtk_edp_phy_start_video(struct rtk_edp *edp,
					 struct drm_display_mode *mode)
{
	dev_info(edp->dev, "edp phy: start video\n");
	//enable eDPTX tx_en
	rtk_edp_update(edp, DPTX_SFIFO_CTRL0,
		DP_IP_DPTX_SFIFO_CTRL0_tx_en_mask,
		DP_IP_DPTX_SFIFO_CTRL0_tx_en(1));

	// change eDPTX output from TPS to video data
	rtk_edp_phy_set_pattern(edp, RTK_PATTERN_VIDEO);

	// start eDPTX video transmission
	rtk_edp_update(edp, ARBITER_CTRL,
		DP_IP_ARBITER_CTRL_vactive_md_mask
		| DP_IP_ARBITER_CTRL_arbiter_en_mask,
		DP_IP_ARBITER_CTRL_vactive_md(0)
		| DP_IP_ARBITER_CTRL_arbiter_en(1));

	// check interrupt when frame done
	rtk_edp_wrap_update(edp, EDPTX_DV_SYNC_INTE,
		EDPTX_DV_SYNC_INTE_dv_sync_int_mask,
		EDPTX_DV_SYNC_INTE_dv_sync_int(mode->vtotal + 1));
}

void rtk_edp_phy_disable_timing_gen(struct rtk_edp *edp)
{
	dev_info(edp->dev, "edp phy: disable timing gen\n");

	rtk_edp_wrap_update(edp, EDPTX_DH_WIDTH,
		EDPTX_DH_WIDTH_dh_width_mask,
		EDPTX_DH_WIDTH_dh_width(0));
	rtk_edp_wrap_update(edp, EDPTX_DH_TOTAL,
		EDPTX_DH_TOTAL_dh_total_mask
		| EDPTX_DH_TOTAL_dh_total_last_line_mask,
		EDPTX_DH_TOTAL_dh_total(0)
		| EDPTX_DH_TOTAL_dh_total_last_line(0));
	rtk_edp_wrap_update(edp, EDPTX_DH_DEN_START_END,
		EDPTX_DH_DEN_START_END_dh_den_sta_mask
		| EDPTX_DH_DEN_START_END_dh_den_end_mask,
		EDPTX_DH_DEN_START_END_dh_den_sta(0)
		| EDPTX_DH_DEN_START_END_dh_den_end(0));
	rtk_edp_write(edp, ARBITER_SEC_END_CNT_HB, 0x0);
	rtk_edp_write(edp, ARBITER_SEC_END_CNT_LB, 0x0);
	rtk_edp_wrap_update(edp, EDPTX_DV_DEN_START_END_FIELD1,
		EDPTX_DV_DEN_START_END_FIELD1_dv_den_sta_field1_mask
		| EDPTX_DV_DEN_START_END_FIELD1_dv_den_end_field1_mask,
		EDPTX_DV_DEN_START_END_FIELD1_dv_den_sta_field1(0)
		| EDPTX_DV_DEN_START_END_FIELD1_dv_den_end_field1(0));
	rtk_edp_wrap_update(edp, EDPTX_DV_TOTAL,
		EDPTX_DV_TOTAL_dv_total_mask,
		EDPTX_DV_TOTAL_dv_total(0));
	rtk_edp_wrap_update(edp, EDPTX_DV_VS_START_END_FIELD1,
		EDPTX_DV_VS_START_END_FIELD1_dv_vs_sta_field1_mask
		| EDPTX_DV_VS_START_END_FIELD1_dv_vs_end_field1_mask,
		EDPTX_DV_VS_START_END_FIELD1_dv_vs_sta_field1(0)
		| EDPTX_DV_VS_START_END_FIELD1_dv_vs_end_field1(0));
	rtk_edp_wrap_update(edp, EDPTX_DH_VS_ADJ_FIELD1,
		EDPTX_DH_VS_ADJ_FIELD1_dh_vs_adj_field1_mask,
		EDPTX_DH_VS_ADJ_FIELD1_dh_vs_adj_field1(0));
}

