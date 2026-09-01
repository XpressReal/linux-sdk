// SPDX-License-Identifier: GPL-2.0
/*
 *  phy-rtk-usb3dprx.c RTK usb3.0 dp rx phy driver
 *
 * copyright (c) 2025 realtek semiconductor corporation
 *
 */

#include <linux/debugfs.h>
#include <linux/extcon.h>
#include <linux/extcon-provider.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/sys_soc.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mfd/syscon.h>
#include <linux/phy/phy.h>
#include <linux/usb/hcd.h>
#include <linux/usb/phy.h>
#include <linux/usb/role.h>
#include <linux/usb/typec_mux.h>
#include <linux/usb/typec_altmode.h>
#include <linux/usb/typec_dp.h>
#include <linux/usb/pd_vdo.h>
#include <dt-bindings/phy/phy.h>

#include "phy-rtk-usb3dprx.h"
#include "phy-rtk-dp-table.h"

#define DUMP_KOFF(name, sel, vmask, smask) do { \
	u3rx_update_bits(typec, U3DP_PHY_HD21_P0_KOFF_REGD08, GENMASK(7, 4), sel); \
	val = u3rx_get_bits(typec, U3DP_PHY_HD21_P0_KOFF_REGD16, vmask); \
	sign = u3rx_get_bits(typec, U3DP_PHY_HD21_P0_KOFF_REGD16, smask); \
	pr_info("%-14s = %+4d\n", name, get_signed_val(val, sign)); \
} while (0)

#define DUMP_DFE(name, sel, vmask, smask) do { \
	u3rx_update_bits(typec, U3DP_PHY_CTRL_R5P0, GENMASK(23, 18), sel); \
	val = u3rx_get_bits(typec, U3DP_PHY_HD21_P0_DFE_REGD01, vmask); \
	if (smask) { \
		sign = u3rx_get_bits(typec, U3DP_PHY_HD21_P0_DFE_REGD01, smask); \
		pr_info("%-14s = %+4d\n", name, get_signed_val(val, sign)); \
	} else { \
		pr_info("%-14s = %4d\n", name, val); \
	} \
} while (0)

static const unsigned int usb_type_c_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_DISP_DP,
	EXTCON_NONE,
};

static void rtk_usb_type_c_lane_config(struct type_c_data *type_c, int cc, int lanes);
static void rtk_phy_restore_reg(struct rtk_phy *rtk_phy);
static int rtk_usb3dprx_send_hpd(struct rtk_phy *rtk_phy, bool high, bool irq);
static int rtk_dp_phy_init(struct phy *phy);
static int rtk_dp_phy_calibrate(struct phy *phy);

static int rtk_type_c_dp01rx_init(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp01_init_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp01_init_table, total_entries);
	return 0;
}

static int rtk_type_c_dp23rx_init(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp23_init_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp23_init_table, total_entries);
	return 0;
}

static int rtk_type_c_dprx_init(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_type_c_dprx_init_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_type_c_dprx_init_table, total_entries);
	return 0;
}

static void rtk_type_c_dp01_configure(struct type_c_data *type_c, unsigned int rate)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp01_configure_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp01_configure_table, total_entries);
	return;
}

static void rtk_type_c_dp23_configure(struct type_c_data *type_c, unsigned int rate)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp23_configure_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp23_configure_table, total_entries);
	return;
}

static int rtk_type_c_4dp_5p4g_configure(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_type_c_4dp_5p4g_configure_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_type_c_4dp_5p4g_configure_table, total_entries);
	return 0;
}

static int rtk_type_c_4dp_2p7g_configure(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_type_c_4dp_2p7g_configure_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_type_c_4dp_2p7g_configure_table, total_entries);

	return 0;
}

static int rtk_type_c_4dp_1p62g_configure(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_type_c_4dp_1p62g_configure_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_type_c_4dp_1p62g_configure_table, total_entries);
	return 0;
}

static int rtk_dp_phy_5p4g_calibrate(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp_phy_5p4g_calibrate_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp_phy_5p4g_calibrate_table, total_entries);
	return 0;
}

static int rtk_dp_phy_2p7g_calibrate(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp_phy_2p7g_calibrate_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp_phy_2p7g_calibrate_table, total_entries);
	return 0;
}

static int rtk_dp_phy_1p62g_calibrate(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp_phy_1p62g_calibrate_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp_phy_1p62g_calibrate_table, total_entries);
	return 0;
}

static int rtk_dp01_phy_5p4g_calibrate(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp01_calibrate_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp01_calibrate_table, total_entries);
        return 0;
}

static int rtk_dp23_phy_5p4g_calibrate(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	int total_entries = sizeof(rtk_dp23_calibrate_table) / sizeof(struct rtk_reg_entry);

	rtk_apply_phy_table(base, rtk_dp23_calibrate_table, total_entries);
	return 0;
}

static int rtk_type_c_dp0123_configure(struct type_c_data *type_c, unsigned int rate)
{
	int ret = 0;

	switch(rate) {
	case 162000:
		ret = rtk_type_c_4dp_1p62g_configure(type_c);
		break;
	case 270000:
		ret = rtk_type_c_4dp_2p7g_configure(type_c);
		break;
	case 540000:
		ret = rtk_type_c_4dp_5p4g_configure(type_c);
		break;
	default:
		pr_err("wrong rate: %d\n", rate);
		return -EINVAL;
	}

	return ret;
}

static void rtk_type_c_usb01_setting(struct type_c_data *type_c)
{
	//##=====[DPHY_Fix]=====##
	u3rx_usb_10g(type_c, 0x0);
	u3rx_usb_5g(type_c, 0x3);
	u3rx_offset_5g_div(type_c, 0x1);
	u3rx_offset_10g_div(type_c, 0x0);
	u3rx_fld_rst_manual_set(type_c, 0x0);
	u3rx_data_timer0_en(type_c, 0x9);
	u3rx_fld_tx_timing_delay(type_c, 0x1);
	u3rx_DPHY2_rst_n_L0_set(type_c, 0x0);
	u3rx_data_inbuf0(type_c, 0x2);
	u3rx_p0_r_off_div_shift_set(type_c, 0xb);
	u3rx_p0_r_off_timeout_shift_set(type_c, 0x28);
	u3rx_p0_r_off_intope_usb5g_shift_set(type_c, 0x0);
	u3rx_p0_r_off_intopo_usb5g_shift_set(type_c, 0x0);
	u3rx_p0_r_off_intde_usb5g_shift_set(type_c, 0x0);
	u3rx_p0_r_off_intdo_usb5g_shift_set(type_c, 0x0);
	u3rx_p0_r_off_intee_usb5g_shift_set(type_c, 0x0);
	u3rx_p0_r_off_inteo_usb5g_shift_set(type_c, 0x0);
	u3rx_p0_r_off_ini_eq_shift_set(type_c, 0x0);
	u3rx_p0_r_off_dly_cnt_set(type_c, 0xf);

	//##=====[DPHY_Param]=====##
	u3rx_fld_divider_set(type_c, 0x1e);
	u3rx_fld_coarse_lock_up_limit_l0_7_0(type_c, 0xd7);
	u3rx_fld_coarse_lock_up_limit_l0_11_8(type_c, 0x2);
	u3rx_fld_coarse_lock_dn_limit_l0_7_0(type_c, 0x9b);
	u3rx_fld_coarse_lock_dn_limit_l0_11_8(type_c, 0x2);
	u3rx_fld_lock_up_limit_l0_7_0(type_c, 0xd7);
	u3rx_fld_lock_up_limit_l0_11_8(type_c, 0x2);
	u3rx_fld_lock_dn_limit_l0_7_0(type_c, 0x9b);
	u3rx_fld_lock_dn_limit_l0_11_8(type_c, 0x2);
	u3rx_fld_init_time_l0_set(type_c, 0x0);
	u3rx_fld_adp_dly_time_set(type_c, 0xc);
	u3rx_fld_manual_l0_en(type_c, 0x0);
	u3rx_fld_vc_sel(type_c, 0x0);
	u3rx_fld_cdr_lpf_res_set(type_c, 0x0);
	u3rx_fld_rst_n_l0_set(type_c, 0x1);

	//##=====[DPHY_Para_frl]=====##
	u3rx_fld_p0_b_acdr_pll_config_1_L0(type_c, 0xa);
	u3rx_fld_p0_b_acdr_pll_config_2_L0(type_c, 0xbe);
	u3rx_fld_p0_b_acdr_pll_config_3_L0(type_c, 0xf);
	u3rx_fld_p0_b_acdr_pll_config_4_L0(type_c, 0x0);
	u3rx_fld_p0_b_acdr_cdr_config_1_L0(type_c, 0x1b);
	u3rx_fld_p0_b_acdr_cdr_config_2_L0(type_c, 0x81);
	u3rx_fld_p0_b_acdr_cdr_config_3_L0(type_c, 0xf);
	u3rx_fld_p0_b_acdr_cdr_config_4_L0(type_c, 0x0);
	u3rx_fld_p0_ck_acdr_manual_config_1(type_c, 0x0);
	u3rx_fld_p0_ck_acdr_manual_config_2(type_c, 0x0);
	u3rx_fld_p0_ck_acdr_manual_config_3(type_c, 0x0);
	u3rx_fld_p0_ck_acdr_manual_config_4(type_c, 0x0);

	//##=====[DFE_init]=====##
	// R lane
	u3rx_dfe_r0p0_set(type_c, 0x0);
	u3rx_dfe_r1p0_set(type_c, 0x0);
	u3rx_dfe_r2p0_set(type_c, 0x20C00000);
	u3rx_dfe_r3p0_set(type_c, 0x05b47c00);
	u3rx_dfe_r4p0_set(type_c, 0xc0004000);
	u3rx_dfe_r5p0_set(type_c, 0x00280000);
	u3rx_dfe_r6p0_set(type_c, 0x1238ffe0);
	u3rx_dfe_r7p0_set(type_c, 0x7FF07FE0);
	u3rx_dfe_r8p0_set(type_c, 0x3FFF83E0);
	u3rx_dfe_r9p0_set(type_c, 0x0);
	u3rx_dfe_rap0_set(type_c, 0x009C00E7);
	u3rx_dfe_rbp0_set(type_c, 0xFF00FFC0);
	u3rx_dfe_rcp0_set(type_c, 0xFF005400);
	// R-Lane_initial DFE Value_Start
	u3rx_dfe_vthp_init(type_c, 14);
	u3rx_dfe_vthn_init(type_c, 14);
	u3rx_dfe_leq_init(type_c, 10);
	u3rx_dfe_tap0_init(type_c, 15);
	u3rx_dfe_tap1_init(type_c, 0x0);
	u3rx_dfe_tap2_init(type_c, 0x0);
	u3rx_dfe_tap3_init(type_c, 0x0);
	u3rx_dfe_tap4_init(type_c, 0x0);
	// R-Lane DFE COEF LOAD IN
	u3rx_dfe_tap_loadin(type_c, 0x1);
	u3rx_dfe_vth_loadin(type_c, 0x1);
	u3rx_dfe_EQ_selreg(type_c, 0x1);
	u3rx_dfe_tap_loadin(type_c, 0x0);
	u3rx_dfe_vth_loadin(type_c, 0x0);
	u3rx_dfe_EQ_selreg(type_c, 0x0);
	u3rx_dfe_adapt_flow_ctrl_r_set(type_c, 0x6);

	//##=====[APHY_Fix_frl]=====##
	u3rx_reg_z0_adjr_l0_set(type_c, 0x13);
	u3rx_reg_z0_z0pow_fix_l0_set(type_c, 0x1);
	u3rx_reg_z0_n_off_l0_set(type_c, 0x0);
	u3rx_reg_z0_p_off_l0_set(type_c, 0x0);
	u3rx_reg_z0_ft_pn_short_en_l0_set(type_c, 0x0);
	u3rx_reg_pow_rterm_l0_set(type_c, 0x1);
	u3rx_reg_hdmirx_bias_en_l0_set(type_c, 0x1);
	u3rx_reg_rx50_link_l0_set(type_c, 0x1);
	u3rx_reg_sel_rx50_link_l0_set(type_c, 0x1);
	u3rx_reg_sel_rx_en_l0_set(type_c, 0x1);
	u3rx_reg_rx_en_l0_set(type_c, 0x1);
	u3rx_reg_ck_tst_sel_rev_l0_set(type_c, 0x0);
	u3rx_l0_vco_sel_band_v2i_set(type_c, 0x4);

	// L0 [APHY fix]
	u3rx_l0_inoff_en_set(type_c, 0x0);
	u3rx_l0_innoff_single_en_set(type_c, 0x0);
	u3rx_l0_inpoff_single_en_set(type_c, 0x0);
	u3rx_l0_pow_ac_couple_set(type_c, 0x1);
	u3rx_l0_rxvcm_sel_set(type_c, 0x1);
	u3rx_l0_ck_rs_cal_en_set(type_c, 0x1);
	u3rx_l2_ck_le_ihalf_set(type_c, 0x1);
	u3rx_l2_ck_nc_ihalf_set(type_c, 0x0);
	u3rx_l2_ck_tap0_ihalf_set(type_c, 0x0);
	u3rx_l0_dfe_pow_set(type_c, 0x1);
	u3rx_l0_dfe_sumamp_isel_set(type_c, 0x6);
	u3rx_l0_dfe_sumamp_dcgain_max_set(type_c, 0x0);
	u3rx_l0_ck_dummy_set(type_c, 0x0);
	u3rx_l0_ck_dfe_cki_dly_en_set(type_c, 0x0);
	u3rx_l0_ck_dfe_ckib_dly_en_set(type_c, 0x0);
	u3rx_l0_ck_dfe_ckq_dly_en_set(type_c, 0x0);
	u3rx_l0_ck_dfe_ckqb_dly_en_set(type_c, 0x0);
	u3rx_l0_ck_en_eye_mnt_set(type_c, 0x0);
	u3rx_l0_demux_degree_eye_mnt_set(type_c, 0x0);
	u3rx_l0_vth_manual_set(type_c, 0x1);
	u3rx_l0_da_eg_vos_pulllow_set(type_c, 0x0);
	u3rx_l0_reg70_dummy_set(type_c, 0x0);
	u3rx_l3_b_dfe_tap_delay_set(type_c, 0x0);
	u3rx_l3_b_dfe_adapt_en_set(type_c, 0x1);
	u3rx_l0_en_tst_cdr_set(type_c, 0x0);
	u3rx_l0_transition_cnt_en_dummy_set(type_c, 0x35);
	u3rx_l0_pi_en_set(type_c, 0x0);
	u3rx_l0_pi_dummy_set(type_c, 0x0);
	u3rx_l0_bias_pi_cur_sel_set(type_c, 0x0);
	u3rx_l0_pi_eye_en_set(type_c, 0x0);
	u3rx_l0_pi_rega8_dummy_set(type_c, 0x0);
	u3rx_aphy_l0_rstb_div_fld_set(type_c, 0x1);
	u3rx_aphy_l0_rstb_div_ref_set(type_c, 0x0);
	u3rx_aphy_l0_rstb_div_pll_set(type_c, 0x0);
	u3rx_l0_acdr_en_updn_pulse_filter_set(type_c, 0x0);
	u3rx_l0_acdr_rstb_updn_set(type_c, 0x1);
	u3rx_l0_acdr_sel_updn_widt_set(type_c, 0x0);
	u3rx_l0_acdr_pow_lpf_idac_set(type_c, 0x1);
	u3rx_l0_acdr_sel_lpf_idac_set(type_c, 0x3);
	u3rx_aphy_l0_isel_in1_set(type_c, 0x0);
	u3rx_l0_acdr_pow_cp_set(type_c, 0x1);
	u3rx_l0_acdr_pow_idn_bbpd_set(type_c, 0x1);
	u3rx_l0_acdr_sel_tie_idn_bbpd_set(type_c, 0x7);
	u3rx_l0_acdr_pow_ibias_idn_hv_set(type_c, 0x0);
	u3rx_l0_acdr_pow_vco_set(type_c, 0x1);
	u3rx_l0_acdr_pow_vco_vdac_set(type_c, 0x1);
	u3rx_l0_acdr_sel_v15_vdac_set(type_c, 0x0);
	u3rx_l0_acdr_pow_test_mode_set(type_c, 0x0);
	u3rx_l0_acdr_sel_test_mode_set(type_c, 0x0);
	u3rx_l0_tie_vctrl_manual_set(type_c, 0x1);
	u3rx_l0_dfr_adapt_rn_set(type_c, 0x0);
	u3rx_reg_dp0_usb1_l0_set(type_c, 0x1);

	//##=====[APHY_Para_frl]=====##
	u3rx_l0_ck_rlsel_le1_set(type_c, 0x2);
	u3rx_l0_ck_rlsel_le2_set(type_c, 0x3);
	u3rx_l0_ck_rlsel_nc_set(type_c, 0x1);
	u3rx_l0_ck_rlsel_tap0_set(type_c, 0x1);
	u3rx_l0_ck_rssel_le1_1_set(type_c, 0x7);
	u3rx_l0_ck_rssel_le1_2_set(type_c, 0x5);
	u3rx_l0_ck_rssel_le2_set(type_c, 0x4);
	u3rx_l0_ck_rssel_tap0_set(type_c, 0x3);
	u3rx_l0_ck_koff_range_set(type_c, 0x1);
	u3rx_l0_ck_le1_isel_in_set(type_c, 0xd);
	u3rx_l0_ck_le2_isel_in_set(type_c, 0xf);
	u3rx_l0_ck_le_nc_isel_in_set(type_c, 0xf);
	u3rx_l0_ck_tap0_isel_set(type_c, 0xf);
	u3rx_l0_ck_le_ihalf_set(type_c, 0x0);
	u3rx_l0_ck_nc_ihalf_set(type_c, 0x1);
	u3rx_l0_ck_tap0_ihalf_set(type_c, 0x0);
	u3rx_l0_ck_leq6g_en_set(type_c, 0x0);
	u3rx_l0_ck_pow_nc_set(type_c, 0x1);
	u3rx_l0_sel_cmfb_ls_set(type_c, 0x0);
	u3rx_l0_leq_cur_adj_set(type_c, 0x3);
	u3rx_l0_ptat_cur_adj_set(type_c, 0x0);
	u3rx_l0_bias_pow_con_gm_set(type_c, 0x0);
	u3rx_l0_ck_fr_ck_sel_set(type_c, 0x1);
	u3rx_l0_demux_pin_rate_sel_set(type_c, 0x6);
	u3rx_l0_demux_fr_ck_sel_set(type_c, 0x1);
	u3rx_l0_demux_rate_sel_set(type_c, 0x0);
	u3rx_l0_pi_isel_set(type_c, 0x0);
	u3rx_l0_pi_csel_set(type_c, 0x0);
	u3rx_l0_pi_div_sel_set(type_c, 0x4);
	u3rx_l0_dfe_ckin_sel_set(type_c, 0x0);
	u3rx_aphy_l0_acdr_sel_idnbias_lv_set(type_c, 0x0);
	u3rx_b_l0_reg90_up_dummy_set(type_c, 0xd);
	u3rx_b_l0_reg98_up_dummy_set(type_c, 0xd);
	u3rx_l0_acdr_rstb_div_band_2or4_lv_set(type_c, 0x1);
	u3rx_l0_acdr_sel_hs_clk_set(type_c, 0x1);
	u3rx_l0_acdr_sel_0fr_1hr_div_iq_set(type_c, 0x1);
	u3rx_l0_acdr_sel_div_band_2or4_lv_set(type_c, 0x0);
	u3rx_l0_acdr_pow_cp_intg2_core_set(type_c, 0x0);
	u3rx_l0_acdr_sel_band_cap_set(type_c, 0x3);
	u3rx_l0_ck_bbpd_kp_sel_set(type_c, 0x1);
	u3rx_l0_ck_bbpd_ki_sel_set(type_c, 0x1);
	u3rx_l0_ck_bbpd_bypass_ctn_kp_set(type_c, 0x0);
	u3rx_l0_ck_bbpd_bypass_ctn_ki_set(type_c, 0x1);
	u3rx_l0_cmu_sel_m_div_set(type_c, 0x59);

	//##=====[APHY_Init_Flow_frl]=====##
	u3rx_l0_ck_bbpd_rstb_set(type_c, 0x0);
	u3rx_aphy_l0_rstb_clk_cld_set(type_c, 0x0);
	u3rx_l0_acdr_rstb_clk_fld_set(type_c, 0x0);
	u3rx_l0_rstb_bbpd_kp_ki_set(type_c, 0x0);
	u3rx_aphy_ck_rstb_sel(type_c, 0x0);
	u3rx_aphy_l0_rstb_pfb_set(type_c, 0x0);
	u3rx_l0_tie_vctrl_set(type_c, 0x1);
	u3rx_l0_fast_sw_en_set(type_c, 0x0);
	u3rx_l0_fast_sw_dly_en_set(type_c, 0x0);
        u3rx_l0_ck_bbpd_rstb_set(type_c, 0x1);
        u3rx_l0_dcdr_rstb_set(type_c, 0x0);
	u3rx_l0_pi_div_rstb_set(type_c, 0x0);
	u3rx_l0_fkp_en_set(type_c, 0x1);
        u3rx_aphy_l0_rstb_clk_cld_set(type_c, 0x1);
        u3rx_l0_acdr_rstb_clk_fld_set(type_c, 0x1);
	u3rx_l0_rstb_bbpd_kp_ki_set(type_c, 0x1);
	u3rx_aphy_ck_rstb_sel(type_c, 0x3);
	u3rx_aphy_l0_rstb_pfb_set(type_c, 0x1);
	u3rx_l0_tie_vctrl_set(type_c, 0x0);
	// APHY acdr start
	u3rx_reg_demux_rstb_l0_set(type_c, 0x1);

	//##=====[ACDR_settings_frl]##
	// CMU RESET
	u3rx_reg_demux_rstb_l0_set(type_c, 0x0);
	u3rx_reg_demux_rstb_l0_set(type_c, 0x1);
	// FLD reset
	u3rx_fld_acdr_fine_tune_start(type_c, 0x0);

	//##=====[ACDR_settings_frl]afn_en
	u3rx_l0_ck_bbpd_rstb_set(type_c, 0x0);
	u3rx_l0_ck_bbpd_rstb_set(type_c, 0x1);

	//##=====[Koffset_frl]=====##
	u3rx_fore_off_00(type_c, 0x0);
        u3rx_fore_off_00(type_c, 0x1);
        u3rx_fore_off_calibration_l0_en(type_c, 0x1);

	// STEP1 Data_Even KOFF
	u3rx_p0_r_13_vth_offpn_sel_set(type_c, 0x0);
        u3rx_p0_r_9_vth_offpn_sel_set(type_c, 0x0);
        u3rx_l0_dfe_tap0_icom_en_set(type_c, 0x1);
	u3rx_l3_b_dfe_tap_en_set(type_c, 0x0);
	u3rx_p0_r_off_rstn_set(type_c, 0x1);
	u3rx_p0_r_off_auto_en_cnt_set(type_c, 0x2);

	// R LANE KOFF SETTINGS
	u3rx_p0_r_off_manual_do_set(type_c, 0x0);
	u3rx_p0_r_off_manual_de_set(type_c, 0x0);
	u3rx_p0_r_off_manual_eq_set(type_c, 0x0);
	u3rx_p0_r_off_manual_eo_set(type_c, 0x0);
	u3rx_p0_r_off_manual_ee_set(type_c, 0x0);
	u3rx_p0_r_off_manual_opo_set(type_c, 0x0);
	u3rx_p0_r_off_manual_ope_set(type_c, 0x0);
	u3rx_l0_inoff_en_set(type_c, 0x1);
	u3rx_l0_pow_cmfb_1p8_cdm_set(type_c, 0x0);
	u3rx_l0_ck_pow_nc_set(type_c, 0x0);
	u3rx_l0_ck_pow_leq_koff_set(type_c, 0x0);
	u3rx_l2_ck_pow_leq_set(type_c, 0x0);

        // R-Lane z0_ok
	u3rx_p0_r_off_z0_19_ok_eo_set(type_c, 0x1);
	u3rx_p0_r_off_z0_ok_eo_set(type_c, 0x1);
	u3rx_p0_r_off_z0_ok_do_set(type_c, 0x1);
	u3rx_p0_r_off_z0_ok_de_set(type_c, 0x1);
	u3rx_p0_r_off_z0_ok_opo_set(type_c, 0x1);
	u3rx_p0_r_off_z0_ok_ope_set(type_c, 0x1);
	u3rx_p0_r_off_pc_do_set(type_c, 0x0);
	u3rx_p0_r_off_pc_de_set(type_c, 0x0);
	u3rx_p0_r_off_pc_ee_set(type_c, 0x0);
	u3rx_p0_r_off_pc_eo_set(type_c, 0x0);
	u3rx_p0_r_off_pc_eq_set(type_c, 0x0);
	u3rx_p0_r_off_pc_ope_set(type_c, 0x0);
	u3rx_p0_r_off_pc_opo_set(type_c, 0x0);

	//##Enable R-Lane DCVS KOFF
	u3rx_p0_r_off_en_de_set(type_c, 0x1);
	u3rx_p0_r_off_en_do_set(type_c, 0x1);
	u3rx_p0_r_off_pc_en_set(type_c, 0x1);
	u3rx_p0_r_off_en_eo_set(type_c, 0x1);
	u3rx_p0_r_off_en_opo_set(type_c, 0x1);
	u3rx_p0_r_off_en_ope_set(type_c, 0x1);
	//##Check Offset cal. OK Check
	u3rx_p0_r_off_coef_sel_set(type_c, 0x9);
	//##Disable R-Lane DCVS KOFF
	u3rx_p0_r_off_en_de_set(type_c, 0x0);
	u3rx_p0_r_off_en_do_set(type_c, 0x0);
	u3rx_p0_r_off_pc_en_set(type_c, 0x0);
	u3rx_p0_r_off_en_eo_set(type_c, 0x0);
	u3rx_p0_r_off_en_opo_set(type_c, 0x0);
	u3rx_p0_r_off_en_ope_set(type_c, 0x0);
	//##R-Lane DCVS Offset Result
	u3rx_p0_r_off_coef_sel_set(type_c, 0x0);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x1);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x2);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x3);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x4);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x5);

	//LEQ KOFF
	//R LANE KOFF LEQ SETTINGS
	u3rx_l0_ck_pow_nc_set(type_c, 0x1);
	u3rx_l0_ck_pow_leq_koff_set(type_c, 0x1);
	u3rx_l2_ck_pow_leq_set(type_c, 0x1);
	u3rx_l0_pow_cmfb_1p8_cdm_set(type_c, 0x1);
	u3rx_p0_r_off_z0_19_ok_eo_set(type_c, 0x0);
	u3rx_p0_r_off_z0_ok_eo_set(type_c, 0x0);
	u3rx_p0_r_off_z0_ok_do_set(type_c, 0x0);
	u3rx_p0_r_off_en_eq_set(type_c, 0x1);
	udelay(100);
	u3rx_p0_r_off_en_eq_set(type_c, 0);
	//##R-Lane LEQ Offset Result
	u3rx_p0_r_off_coef_sel_set(type_c, 0x8);

	//##===R LANE KOFF Data/Edge
	u3rx_p0_r_off_en_de_set(type_c, 0x1);
	u3rx_p0_r_off_en_do_set(type_c, 0x1);
	u3rx_p0_r_off_pc_en_set(type_c, 0x1);
	u3rx_p0_r_off_en_eo_set(type_c, 0x1);
	u3rx_p0_r_off_en_opo_set(type_c, 0x1);
	u3rx_p0_r_off_en_ope_set(type_c, 0x1);
	//##R-Lane DCVS Offset cal. OK Check
	u3rx_p0_r_off_coef_sel_set(type_c, 0x9);
	//##Disable R-Lane DCVS KOFF
	u3rx_p0_r_off_en_de_set(type_c, 0x0);
	u3rx_p0_r_off_en_do_set(type_c, 0x0);
	u3rx_p0_r_off_pc_en_set(type_c, 0x0);
	u3rx_p0_r_off_en_eo_set(type_c, 0x0);
	u3rx_p0_r_off_en_opo_set(type_c, 0x0);
	u3rx_p0_r_off_en_ope_set(type_c, 0x0);
	//##R-Lane LEQ & DCVS Offset Result
	u3rx_p0_r_off_coef_sel_set(type_c, 0x0);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x1);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x2);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x3);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x8);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x4);
	u3rx_p0_r_off_coef_sel_set(type_c, 0x5);
	//##Input on
	u3rx_l0_inoff_en_set(type_c, 0x0);
	u3rx_l0_dfe_tap0_icom_en_set(type_c, 0x0);
	u3rx_l3_b_dfe_tap_en_set(type_c, 0xf);
	//##=====[ACDR_settings_frl]finetunestart_on
	u3rx_fld_acdr_fine_tune_start(type_c, 0x1);

	//ACDR to koffset mode
	//LEQ_VTH_Tap0_2_Adapt_USB
	u3rx_p0_r_13_vth_offpn_sel_set(type_c, 0x1);
	u3rx_p0_r_9_vth_offpn_sel_set(type_c, 0x1);
	//##DFE_Adaptation
	//##===R LANE TAP0 & LEQ & Tap3 & Tap4 Adapt
	writel(0x00007400, type_c->base + 0xc0c);
	u3rx_dfe_inverse_vth_sign_polarity(type_c, 0x1);
	u3rx_dfe_leq1_inv_set(type_c, 0x1);
	u3rx_dfe_leq2_inv_set(type_c, 0x1);
	u3rx_dfe_leq_gain1_set(type_c, 0x0);
	u3rx_dfe_leq_gain2_set(type_c, 0x0);
	u3rx_dfe_tap1_gain_set(type_c, 0x0);
	u3rx_dfe_tap2_gain_set(type_c, 0x0);
	u3rx_dfe_tap3_gain_set(type_c, 0x0);
	u3rx_dfe_tap4_gain_set(type_c, 0x0);
	u3rx_dfe_leq1_trans_mode(type_c, 0x2);
	u3rx_dfe_leq2_trans_mode(type_c, 0x3);
	u3rx_dfe_tap4_vthp_set(type_c, 0x2);
	u3rx_dfe_tap4_vthn_set(type_c, 0x2);
	u3rx_dfe_tap_timer(type_c, 0x5);
	u3rx_dfe_leq_timer(type_c, 0x5);
	u3rx_dfe_vth_timer(type_c, 0x5);
	u3rx_dfe_eqfe_en(type_c, 0x1);
	u3rx_dfe_apadt_en(type_c, 0x1);
	u3rx_dfe_VTH_en(type_c, 0x1);
	u3rx_dfe_VTH_DFE_en(type_c, 0x1);
	u3rx_dfe_tap0_adjust_set(type_c, 0x1);
	//##Start DFE adapt
	u3rx_p0_r_rstb_eq_set(type_c, 0x1);
	u3rx_p0_r_leq_en_set(type_c, 0x1);
	u3rx_p0_r_vth_en_set(type_c, 0x1);
	u3rx_p0_r_dfe_en_set(type_c, 0x1);
	//##=====[FLD_RST]=====##
	u3rx_fld_rst_n_l0_set(type_c, 0x0);
	u3rx_fld_rst_n_l0_set(type_c, 0x1);
}

static void rtk_type_c_usb23_setting(struct type_c_data *type_c)
{
	//##=====[DPHY_Fix]=====##
	u3rx_usb_10g(type_c, 0x0);
	u3rx_usb_5g(type_c, 0x3);
	u3rx_offset_5g_div(type_c, 0x1);
	u3rx_offset_10g_div(type_c, 0x0);

	u3rx_DPHY2_rst_n_L3_set(type_c, 0x0);
	u3rx_fld_rst_manual_set(type_c, 0x0);
	u3rx_data_inbuf1(type_c, 0x2);
	u3rx_p0_ck_off_div_shift_set(type_c, 0xb);
	u3rx_p0_ck_off_timeout_shift_set(type_c, 0x28);
	u3rx_p0_ck_off_intope_shift_set(type_c, 0x0);
	u3rx_p0_ck_off_intopo_shift_set(type_c, 0x0);
	u3rx_p0_ck_off_intde_shift_set(type_c, 0x0);
	u3rx_p0_ck_off_intdo_shift_set(type_c, 0x0);
	u3rx_p0_ck_off_intee_shift_set(type_c, 0x0);
	u3rx_p0_ck_off_inteo_shift_set(type_c, 0x0);
	u3rx_p0_ck_off_ini_eq_shift_set(type_c, 0x0);
	u3rx_p0_ck_off_dly_cnt_set(type_c, 0xf);
	u3rx_data_timer0_en(type_c, 0x9);
	u3rx_fld_tx_timing_delay(type_c, 0x1);

	//##=====[DPHY_Param]=====##
	u3rx_fld_divider_l3_set(type_c, 0x1e);
	u3rx_fld_coarse_lock_up_limit_l3_7_0(type_c, 0xd7);
	u3rx_fld_coarse_lock_up_limit_l3_11_8(type_c, 0x2);
	u3rx_fld_coarse_lock_dn_limit_l3_7_0(type_c, 0x9b);
	u3rx_fld_coarse_lock_dn_limit_l3_11_8(type_c, 0x2);
	u3rx_fld_lock_up_limit_l3_7_0(type_c, 0xd7);
	u3rx_fld_lock_up_limit_l3_11_8(type_c, 0x2);
	u3rx_fld_lock_dn_limit_l3_7_0(type_c, 0x9b);
	u3rx_fld_lock_dn_limit_l3_11_8(type_c, 0x2);
	u3rx_fld_init_time_l3_set(type_c, 0x0);
	u3rx_fld_adp_dly_time_l3_set(type_c, 0xc);
	u3rx_fld_manual_l3_en(type_c, 0x0);
	u3rx_fld_vc_sel_l3(type_c, 0x0);
	u3rx_fld_cdr_lpf_l3_res_set(type_c, 0x0);
	u3rx_fld_rst_n_l3_set(type_c, 0x1);

	//##=====[DPHY_Para_frl]=====##
	u3rx_fld_p0_b_acdr_pll_config_1_L3(type_c, 0xa);
	u3rx_fld_p0_b_acdr_pll_config_2_L3(type_c, 0xbe);
	u3rx_fld_p0_b_acdr_pll_config_3_L3(type_c, 0xf);
	u3rx_fld_p0_b_acdr_pll_config_4_L3(type_c, 0x0);
	u3rx_fld_p0_b_acdr_cdr_config_1_L3(type_c, 0x1b);
	u3rx_fld_p0_b_acdr_cdr_config_2_L3(type_c, 0x81);
	u3rx_fld_p0_b_acdr_cdr_config_3_L3(type_c, 0xf);
	u3rx_fld_p0_b_acdr_cdr_config_4_L3(type_c, 0x0);
	u3rx_fld_p0_l3_ck_acdr_manual_config_1(type_c, 0x0);
	u3rx_fld_p0_l3_ck_acdr_manual_config_2(type_c, 0x0);
	u3rx_fld_p0_l3_ck_acdr_manual_config_3(type_c, 0x0);
	u3rx_fld_p0_l3_ck_acdr_manual_config_4(type_c, 0x0);

	//##=====[DFE_init]=====##
	// CK lane
	u3rx_dfe_ck0p0_set(type_c, 0x0);
	u3rx_dfe_ck1p0_set(type_c, 0x0);
	u3rx_dfe_ck2p0_set(type_c, 0x20C00000);
	u3rx_dfe_ck3p0_set(type_c, 0x05b47c00);
	u3rx_dfe_ck4p0_set(type_c, 0xc0004000);
	u3rx_dfe_ck5p0_set(type_c, 0x00280000);
	u3rx_dfe_ck6p0_set(type_c, 0x1238ffe0);
	u3rx_dfe_ck7p0_set(type_c, 0x7FF07FE0);
	u3rx_dfe_ck8p0_set(type_c, 0x3FFF83E0);
	u3rx_dfe_ck9p0_set(type_c, 0x0);
	u3rx_dfe_ckap0_set(type_c, 0x009C00E7);
	u3rx_dfe_ckbp0_set(type_c, 0xFF00FFC0);
	u3rx_dfe_ckcp0_set(type_c, 0xFF005400);
	// CK-Lane_initial DFE Value_Start
	u3rx_dfe_ck_vthp_init(type_c, 14);
	u3rx_dfe_ck_vthn_init(type_c, 14);
	u3rx_dfe_ck_leq_init(type_c, 10);
	u3rx_dfe_ck_tap0_init(type_c, 15);
	u3rx_dfe_ck_tap1_init(type_c, 0x0);
	u3rx_dfe_ck_tap2_init(type_c, 0x0);
	u3rx_dfe_ck_tap3_init(type_c, 0x0);
	u3rx_dfe_ck_tap4_init(type_c, 0x0);
	// CK-Lane DFE COEF LOAD IN
	u3rx_dfe_ck_tap_loadin(type_c, 0x1);
	u3rx_dfe_ck_vth_loadin(type_c, 0x1);
	u3rx_dfe_ck_EQ_selreg(type_c, 0x1);
	u3rx_dfe_ck_tap_loadin(type_c, 0x0);
	u3rx_dfe_ck_vth_loadin(type_c, 0x0);
	u3rx_dfe_ck_EQ_selreg(type_c, 0x0);
	u3rx_dfe_adapt_flow_ctrl_ck_set(type_c, 0x6);

	//##=====[APHY_Fix_frl]=====##
	u3rx_reg_z0_adjr_l3_set(type_c, 0x13);
	u3rx_reg_z0_z0pow_fix_l3_set(type_c, 0x1);
	u3rx_reg_z0_n_off_l3_set(type_c, 0x0);
	u3rx_reg_z0_p_off_l3_set(type_c, 0x0);
	u3rx_reg_z0_ft_pn_short_en_l3_set(type_c, 0x0);
	u3rx_reg_pow_rterm_l3_set(type_c, 0x1);
	u3rx_reg_hdmirx_bias_en_l3_set(type_c, 0x1);
        u3rx_reg_rx50_link_l3_set(type_c, 0x1);
        u3rx_reg_sel_rx50_link_l3_set(type_c, 0x1);
        u3rx_reg_sel_rx_en_l3_set(type_c, 0x1);
        u3rx_reg_rx_en_l3_set(type_c, 0x1);
        u3rx_reg_ck_tst_sel_rev_l3_set(type_c, 0x0);
        u3rx_l3_vco_sel_band_v2i_set(type_c, 0x4);

	//##=====[APHY_Para_frl]=====##
	u3rx_l3_inoff_en_set(type_c, 0x0);
	u3rx_l3_innoff_single_en_set(type_c, 0x0);
	u3rx_l3_inpoff_single_en_set(type_c, 0x0);
	u3rx_l3_pow_ac_couple_set(type_c, 0x1);
	u3rx_l3_rxvcm_sel_set(type_c, 0x1);
	u3rx_l1_ck_rs_cal_en_set(type_c, 0x1);
	u3rx_l3_pow_datalane_bias_set(type_c, 0x1);
	u3rx_l3_reg_force_startup_set(type_c, 0x0);
	u3rx_l3_reg_powb_startup_set(type_c, 0x0);
	u3rx_l0_ck_dfe_pow_set(type_c, 0x1);
	u3rx_l0_ck_dfe_sumamp_set(type_c, 0x6);
	u3rx_l0_ck_dfe_sumamp_dcgain_set(type_c, 0x0);
	u3rx_l3_ck_dummy_set(type_c, 0x0);
	u3rx_l3_ck_dfe_cki_dly_en_set(type_c, 0x0);
	u3rx_l3_ck_dfe_ckib_dly_en_set(type_c, 0x0);
	u3rx_l3_ck_dfe_ckq_dly_en_set(type_c, 0x0);
	u3rx_l3_ck_dfe_ckqb_dly_en_set(type_c, 0x0);
	u3rx_l3_b_en_eye_mnt_set(type_c, 0x0);
	u3rx_l3_demux_degree_eye_mnt_set(type_c, 0x0);
	u3rx_l3_vth_manual_set(type_c, 0x1);
	u3rx_l3_da_eg_vos_pulllow_set(type_c, 0x0);
	u3rx_l3_reg78_dummy_set(type_c, 0x0);
	u3rx_l3_dfe_tap_delay_set(type_c, 0x0);
	u3rx_l3_dfe_adapt_en_set(type_c, 0x1);
	u3rx_l3_en_tst_cdr_set(type_c, 0x0);
	u3rx_l3_transition_cnt_en_set(type_c, 0x1);
	u3rx_l3_pi_en_set(type_c, 0x0);
	u3rx_l3_pi_dummy_set(type_c, 0x0);
	u3rx_l3_bias_pi_cur_sel_set(type_c, 0x0);
	u3rx_l3_pi_eye_en_set(type_c, 0x0);
	u3rx_l3_regc0_dummy_up_set(type_c, 0x0);
	u3rx_aphy_l3_rstb_div_fld_set(type_c, 0x1);
	u3rx_aphy_l3_rstb_div_ref_set(type_c, 0x0);
	u3rx_aphy_l3_rstb_div_pll_set(type_c, 0x0);
	u3rx_aphy_l3_acdr_sel_div_training_set(type_c, 0x0);
	u3rx_aphy_l3_acdr_dummy_set(type_c, 0x0);
	u3rx_aphy_l3_acdr_sel_fld_0ckfb_1ckref_set(type_c, 0x0);
	u3rx_l3_acdr_en_updn_pulse_filter_set(type_c, 0x0);
	u3rx_l3_acdr_rstb_updn_set(type_c, 0x1);
	u3rx_l3_acdr_sel_updn_widt_set(type_c, 0x0);
	u3rx_l3_acdr_pow_lpf_idac_set(type_c, 0x1);
	u3rx_l3_acdr_sel_lpf_idac_set(type_c, 0x3);
	u3rx_l3_acdr_pow_cp_set(type_c, 0x1);
	u3rx_l3_acdr_pow_idn_bbpd_set(type_c, 0x1);
	u3rx_l3_acdr_sel_tie_idn_bbpd_set(type_c, 0x7);
	u3rx_l3_acdr_pow_ibias_idn_hv_set(type_c, 0x0);
	u3rx_l3_acdr_pow_vco_set(type_c, 0x1);
	u3rx_l3_acdr_pow_vco_vdac_set(type_c, 0x1);
	u3rx_l3_acdr_sel_v15_vdac_set(type_c, 0x0);
	u3rx_l3_acdr_pow_test_mode_set(type_c, 0x0);
	u3rx_l3_acdr_sel_test_mode_set(type_c, 0x0);
	u3rx_aphy_ck_ref_set(type_c, 0x0);
	u3rx_aphy_l3_isel_in1_set(type_c, 0x0);
	u3rx_l3_tie_vctrl_manual_set(type_c, 0x1);
	u3rx_l3_dfr_adapt_rn_set(type_c, 0x0);
	u3rx_reg_dp0_usb1_l3_set(type_c, 0x1);

	//##=====[APHY_Para_frl]=====##
	u3rx_l3_ck_rlsel_le1_set(type_c, 0x2);
	u3rx_l3_ck_rlsel_le2_set(type_c, 0x3);
	u3rx_l3_ck_rlsel_nc_set(type_c, 0x1);
	u3rx_l3_ck_rlsel_tap0_set(type_c, 0x1);
	u3rx_l3_ck_rssel_le1_1_set(type_c, 0x7);
	u3rx_l3_ck_rssel_le1_2_set(type_c, 0x5);
	u3rx_l3_ck_rssel_le2_set(type_c, 0x4);
	u3rx_l3_ck_rssel_tap0_set(type_c, 0x3);
	u3rx_l3_ck_koff_range_set(type_c, 0x1);
	u3rx_l3_ck_le1_isel_in_set(type_c, 0xd);
	u3rx_l3_ck_le2_isel_in_set(type_c, 0xf);
	u3rx_l3_ck_le_nc_isel_in_set(type_c, 0xf);
	u3rx_l3_ck_tap0_isel_set(type_c, 0xf);
	u3rx_l1_ck_le_ihalf_set(type_c, 0x0);
	u3rx_l1_ck_nc_ihalf_set(type_c, 0x1);
	u3rx_l1_ck_tap0_ihalf_set(type_c, 0x0);
	u3rx_l1_ck_leq6g_en_set(type_c, 0x0);
	u3rx_l1_ck_pow_nc_set(type_c, 0x1);
	u3rx_l3_datalane_bias_isel_set(type_c, 0x3);
	u3rx_l3_pow_leq_rl_set(type_c, 0x1);
	u3rx_reg_sel_cmfb_ls_set(type_c, 0x0);
	u3rx_reg_leq_cur_adj_set(type_c, 0x3);
	u3rx_reg_ptat_cur_set(type_c, 0x0);
	u3rx_reg_bias_pow_con_gm_set(type_c, 0x0);
	u3rx_l0_ck_ptat_cur_adj_fine_set(type_c, 0x0);
	u3rx_l3_ck_fr_ck_sel_set(type_c, 0x1);
	u3rx_l3_demux_pix_rate_sel_set(type_c, 0x6);
	u3rx_l3_demux_fr_ck_sel_set(type_c, 0x1);
	u3rx_l3_demux_fr_rate_sel_set(type_c, 0x0);
	u3rx_b_l3_reg98_up_dummy_set(type_c, 0xd);
	u3rx_r_l3_rega0_up_dummy_set(type_c, 0xd);
	u3rx_l3_pi_isel_set(type_c, 0x0);
	u3rx_l3_pi_csel_set(type_c, 0x0);
	u3rx_l3_pi_div_sel_set(type_c, 0x4);
	u3rx_l3_dfe_ckin_sel_set(type_c, 0x0);
	u3rx_aphy_l3_acdr_sel_idnbias_lv_set(type_c, 0x0);
	u3rx_l3_acdr_rstb_div_band_2or4_lv_set(type_c, 0x1);
	u3rx_l3_acdr_sel_hs_clk_set(type_c, 0x1);
	u3rx_l3_acdr_sel_0fr_1hr_div_iq_set(type_c, 0x1);
	u3rx_l3_acdr_sel_div_band_2or4_lv_set(type_c, 0x0);
	u3rx_l3_acdr_pow_cp_intg2_core_set(type_c, 0x0);
	u3rx_l3_acdr_sel_band_cap_set(type_c, 0x3);
	u3rx_l3_ck_bbpd_kp_sel_set(type_c, 0x1);
	u3rx_l3_ck_bbpd_ki_sel_set(type_c, 0x1);
	u3rx_l3_ck_bbpd_bypass_ctn_kp_set(type_c, 0x0);
	u3rx_l3_ck_bbpd_bypass_ctn_ki_set(type_c, 0x1);
	u3rx_aphy_l3_rlsel_le1_2_set(type_c, 0x2);
	u3rx_aphy_l3_rlsel_nc_2_set(type_c, 0x0);

	//##=====[APHY_Init_Flow_frl]
	u3rx_l3_ck_bbpd_rstb_set(type_c, 0x0);
	u3rx_aphy_l3_rstb_clk_cld_set(type_c, 0x0);
	u3rx_l3_acdr_rstb_div_iq_set(type_c, 0x0);
	u3rx_l3_rstb_bbpd_kp_ki_set(type_c, 0x0);
	u3rx_aphy_ck_cmu_prescale(type_c, 0x0);
	u3rx_aphy_ck_cmu_m_div_set(type_c, 0x0);
	u3rx_aphy_l3_rstb_pfb_set(type_c, 0x0);
	u3rx_l3_tie_vctrl_set(type_c, 0x1);
	u3rx_l3_fast_sw_en_set(type_c, 0x0);
	u3rx_l3_fast_sw_dly_en_set(type_c, 0x0);
	u3rx_l3_ck_bbpd_rstb_set(type_c, 0x1);
	u3rx_l3_dcdr_rstb_set(type_c, 0x0);
	u3rx_l3_pi_div_rstb_set(type_c, 0x0);
	u3rx_l3_fkp_en_set(type_c, 0x1);
	u3rx_aphy_l3_rstb_clk_cld_set(type_c, 0x1);
	u3rx_l3_acdr_rstb_div_iq_set(type_c, 0x1);
	u3rx_l3_rstb_bbpd_kp_ki_set(type_c, 0x1);
	u3rx_aphy_ck_cmu_prescale(type_c, 0x1);
	u3rx_aphy_ck_cmu_m_div_set(type_c, 0x1);
	u3rx_aphy_l3_rstb_pfb_set(type_c, 0x1);
	u3rx_l3_tie_vctrl_set(type_c, 0x0);
	//APHY acdr flow start
	u3rx_reg_demux_rstb_l3_set(type_c, 0x1);
	//##=====[ACDR_settings_frl]
	//CMU RESET
	u3rx_reg_demux_rstb_l3_set(type_c, 0x0);
	u3rx_reg_demux_rstb_l3_set(type_c, 0x1);
	//FLD reset
	u3rx_fld_acdr_l3_fine_tune_start(type_c, 0x0);
	//ACDR_settings_frl
	u3rx_l3_ck_bbpd_rstb_set(type_c, 0x0);
	u3rx_l3_ck_bbpd_rstb_set(type_c, 0x1);

	//Koffset_frl
	u3rx_fld_acdr_l3_fine_tune_start(type_c, 0x0);
	u3rx_fore_off_00(type_c, 0x0);
	u3rx_fore_off_00(type_c, 0x1);
	u3rx_fore_off_calibration_l3_en(type_c, 0x1);

	u3rx_p0_ck_12_vth_offpn_sel_set(type_c, 0x0);
	u3rx_p0_ck_8_vth_offpn_sel_set(type_c, 0x0);
	u3rx_l3_dfe_tap0_icom_en_set(type_c, 0x1);
	u3rx_l3_dfe_tap_en_set(type_c, 0x0);
	u3rx_p0_ck_off_rstn_set(type_c, 0x1);
	u3rx_p0_ck_off_auto_en_cnt_set(type_c, 0x2);
	//CK LANE KOFF SETTINGS
	u3rx_p0_ck_off_manual_do_set(type_c, 0x0);
	u3rx_p0_ck_off_manual_de_set(type_c, 0x0);
	u3rx_p0_ck_off_manual_eq_set(type_c, 0x0);
	u3rx_p0_ck_off_manual_eo_set(type_c, 0x0);
	u3rx_p0_ck_off_manual_ee_set(type_c, 0x0);
	u3rx_p0_ck_off_manual_opo_set(type_c, 0x0);
	u3rx_p0_ck_off_manual_ope_set(type_c, 0x0);
	u3rx_l3_inoff_en_set(type_c, 0x1);
	u3rx_reg_pow_cmfb_1p8_cdm_set(type_c, 0x0);
	u3rx_l1_ck_pow_nc_set(type_c, 0x0);
	u3rx_l1_ck_pow_leq_koff_set(type_c, 0x0);
	u3rx_l3_pow_leq_set(type_c, 0x0);
	//##CK-Lane z0_ok
	u3rx_p0_ck_off_z0_19_ok_eo_set(type_c, 0x1);
	u3rx_p0_ck_off_z0_ok_eo_set(type_c, 0x1);
	u3rx_p0_ck_off_z0_ok_do_set(type_c, 0x1);
	u3rx_p0_ck_off_z0_ok_de_set(type_c, 0x1);
	u3rx_p0_ck_off_z0_ok_opo_set(type_c, 0x1);
	u3rx_p0_ck_off_z0_ok_ope_set(type_c, 0x1);
	u3rx_p0_ck_off_pc_do_set(type_c, 0x0);
	u3rx_p0_ck_off_pc_de_set(type_c, 0x0);
	u3rx_p0_ck_off_pc_ee_set(type_c, 0x0);
	u3rx_p0_ck_off_pc_eo_set(type_c, 0x0);
	u3rx_p0_ck_off_pc_eq_set(type_c, 0x0);
	u3rx_p0_ck_off_pc_ope_set(type_c, 0x0);
	u3rx_p0_ck_off_pc_opo_set(type_c, 0x0);
	//##Enable CK-Lane DCVS KOFF
	u3rx_p0_ck_off_en_de_set(type_c, 0x1);
	u3rx_p0_ck_off_en_do_set(type_c, 0x1);
	u3rx_p0_ck_off_pc_en_set(type_c, 0x1);
	u3rx_p0_ck_off_en_eo_set(type_c, 0x1);
	u3rx_p0_ck_off_en_opo_set(type_c, 0x1);
	u3rx_p0_ck_off_en_ope_set(type_c, 0x1);
	//##Check Offset cal. OK Check
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x9);
	//##Disable CK-Lane DCVS KOFF
	u3rx_p0_ck_off_en_de_set(type_c, 0x0);
	u3rx_p0_ck_off_en_do_set(type_c, 0x0);
	u3rx_p0_ck_off_pc_en_set(type_c, 0x0);
	u3rx_p0_ck_off_en_eo_set(type_c, 0x0);
	u3rx_p0_ck_off_en_opo_set(type_c, 0x0);
	u3rx_p0_ck_off_en_ope_set(type_c, 0x0);
	//##CK-Lane DCVS Offset Result
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x0);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x1);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x2);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x3);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x4);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x5);

	//##===CK LANE KOFF LEQ SETTINGS
	u3rx_l1_ck_pow_nc_set(type_c, 0x1);
	u3rx_l1_ck_pow_leq_koff_set(type_c, 0x1);
	u3rx_l3_pow_leq_set(type_c, 0x1);
	u3rx_reg_pow_cmfb_1p8_cdm_set(type_c, 0x1);
	u3rx_p0_ck_off_z0_19_ok_eo_set(type_c, 0x0);
	u3rx_p0_ck_off_z0_ok_eo_set(type_c, 0x0);
	u3rx_p0_ck_off_z0_ok_do_set(type_c, 0x0);
	u3rx_p0_ck_off_en_eq_set(type_c, 0x1);
	udelay(100);
	u3rx_p0_ck_off_en_eq_set(type_c, 0x0);
	//##CK-Lane LEQ Offset Result
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x8);

	//##===CK LANE KOFF Data/Edge
	u3rx_p0_ck_off_en_de_set(type_c, 0x1);
	u3rx_p0_ck_off_en_do_set(type_c, 0x1);
	u3rx_p0_ck_off_pc_en_set(type_c, 0x1);
	u3rx_p0_ck_off_en_eo_set(type_c, 0x1);
	u3rx_p0_ck_off_en_opo_set(type_c, 0x1);
	u3rx_p0_ck_off_en_ope_set(type_c, 0x1);
	//##CK-Lane DCVS Offset cal. OK Check
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x9);
	//##Disable CK-Lane DCVS KOFF
	u3rx_p0_ck_off_en_de_set(type_c, 0x0);
	u3rx_p0_ck_off_en_do_set(type_c, 0x0);
	u3rx_p0_ck_off_pc_en_set(type_c, 0x0);
	u3rx_p0_ck_off_en_eo_set(type_c, 0x0);
	u3rx_p0_ck_off_en_opo_set(type_c, 0x0);
	u3rx_p0_ck_off_en_ope_set(type_c, 0x0);
	//##CK-Lane LEQ & DCVS Offset Result
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x0);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x1);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x2);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x3);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x8);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x4);
	u3rx_p0_ck_off_coef_sel_set(type_c, 0x5);
	//##Input on
	u3rx_l3_inoff_en_set(type_c, 0x0);
	u3rx_l3_dfe_tap0_icom_en_set(type_c, 0x0);
	u3rx_l3_dfe_tap_en_set(type_c, 0xf);
	//ACDR_settings_frl
	u3rx_fld_acdr_l3_fine_tune_start(type_c, 0x1);

	//LEQ_VTH_Tap0_2_Adapt_USB
	u3rx_p0_ck_12_vth_offpn_sel_set(type_c, 0x1);
	u3rx_p0_ck_8_vth_offpn_sel_set(type_c, 0x1);

	//DFE_Adaptation
	//##===CK LANE TAP0 & LEQ & Tap3 & Tap4 Adapt
	writel(0x00007400, type_c->base + 0xccc);
	u3rx_dfe_inverse_vth_sign_polarity_ck(type_c, 0x1);
	u3rx_dfe_leq1_inv_ck_set(type_c, 0x1);
	u3rx_dfe_leq2_inv_ck_set(type_c, 0x1);
	u3rx_dfe_leq_gain1_ck_set(type_c, 0x0);
	u3rx_dfe_leq_gain2_ck_set(type_c, 0x0);
	u3rx_dfe_tap1_gain_ck_set(type_c, 0x0);
	u3rx_dfe_tap2_gain_ck_set(type_c, 0x0);
	u3rx_dfe_tap3_gain_ck_set(type_c, 0x0);
	u3rx_dfe_tap4_gain_ck_set(type_c, 0x0);
	u3rx_dfe_leq1_trans_ck_mode(type_c, 0x2);
	u3rx_dfe_leq2_trans_ck_mode(type_c, 0x3);
	u3rx_dfe_tap4_ck_vthp_set(type_c, 0x2);
	u3rx_dfe_tap4_ck_vthn_set(type_c, 0x2);
	u3rx_dfe_ck_tap_timer(type_c, 0x5);
	u3rx_dfe_ck_leq_timer(type_c, 0x5);
	u3rx_dfe_ck_vth_timer(type_c, 0x5);
	u3rx_dfe_ck_eqfe_en(type_c, 0x1);
	u3rx_dfe_ck_apadt_en(type_c, 0x1);
	u3rx_dfe_ck_VTH_en(type_c, 0x1);
	u3rx_dfe_ck_VTH_DFE_en(type_c, 0x1);
	u3rx_dfe_tap0_adjust_ck_set(type_c, 0x1);
	//##Start DFE adapt
	u3rx_p0_ck_rstb_eq_set(type_c, 0x1);
	u3rx_p0_ck_leq_en_set(type_c, 0x1);
	u3rx_p0_ck_vth_en_set(type_c, 0x1);
	u3rx_p0_ck_dfe_en_set(type_c, 0x1);
	//##===FLD_RST ===
	u3rx_fld_rst_n_l3_set(type_c, 0x0);
	u3rx_fld_rst_n_l3_set(type_c, 0x1);
}

static void rtk_type_c_usb_parameter(struct type_c_data *type_c)
{
        void __iomem *base;

        base = type_c->base;

	writel(0x88822000, base + 0xf98);
	writel(0x000e3836, base + 0xf5c);
	writel(0x000e3800, base + 0xf88);
	writel(0x00000010, base + 0xac8);
	writel(0x0415044, base + 0xf44);
	writel(0x88822005, base + 0xf98);
	writel(0x00000003, base + 0x918);
	writel(0x000000fb, base + 0x91C);
	writel(0x00000059, base + 0x924);
	writel(0x00000043, base + 0x900);
	writel(0x00000003, base + 0x900);

	writel(0x3906dbf, base + 0xf30);
	writel(0x7cf5f50, base + 0xf08);
	writel(0xc0003d04, base + 0xf0c);
	writel(0xc800005, base + 0xf18);
	writel(0x0, base + 0x340);
	writel(0x80, base + 0x340);
	writel(0x180000, base + 0xf78);
	writel(0x6, base + 0xf7c);

	writel(0x29c01ce, base + 0xc28);
	writel(0x6a7a0508, base + 0xf9c);
	writel(0x4b4b4b4b, base + 0xe50);
	writel(0x4040404, base + 0xe20);
	writel(0x75757575, base + 0xe38);
	writel(0x4b4b4b4b, base + 0xe54);
	writel(0x4040404, base + 0xe24);
	writel(0x75757575, base + 0xe3c);
	writel(0x57775e5e, base + 0xec8);
	writel(0x74745777, base + 0xed0);
	writel(0xfdfd7474, base + 0xed8);
	writel(0x34, base + 0xb18);
	writel(0x30, base + 0xb2c);
	writel(0x2a803c0, base + 0xf34);
	writel(0x7baa8014, base + 0xf38);
	writel(0xf0ff05eb, base + 0xf14);
	writel(0xa0ff05eb, base + 0xf14);
	writel(0xf0ff05eb, base + 0xf14);
	writel(0x30, base + 0x348);

	writel(0x3fff83e5, base + 0xc20);
	writel(0x3fff83e5, base + 0xce0);
	writel(0x029C01CE, base + 0xce8);
	writel(0xc, base + 0x34c);
	writel(0x8, base + 0xbe0);
	writel(0x19, base + 0x9c4);
}

static void rtk_usb_type_c_lane_config(struct type_c_data *type_c, int cc, int lanes)
{
	void __iomem *base = type_c->base;
	void __iomem *iso_base = type_c->iso_base;
	void __iomem *pwrctrl = type_c->pwr_base;
	int val;
	u32 dphy_01_before, dphy_01_after;

	pr_debug("[DPRX] === LANE CONFIG: cc=%d, lanes=%d ===\n", cc, lanes);

	dphy_01_before = readl(base + U3DP_PHY_DPHY_01);
	pr_debug("[DPRX]   U3DP_PHY_DPHY_01 BEFORE: 0x%x\n", dphy_01_before);

	if (lanes == 4) {
		pr_debug("[DPRX]   4-lane DP mode configuration\n");
		val = readl(iso_base);
		val = val &~ 0x10000; //lane config
		val = val | 0x20000; //lane mode
		writel(val, iso_base);
		pr_debug("[DPRX]   ISO_BASE after:  0x%x\n", val);
		writel(readl(base + U3DP_PHY_DPHY_01) &~ 0x3, base + U3DP_PHY_DPHY_01);
		pr_debug("[DPRX]   Set U3DP_PHY_DPHY_01 = 0x0 (4-lane DP)\n");
	} else {
		if (cc == enable_cc1) {
			pr_debug("[DPRX]   CC1 configuration (USB on L0-1)\n");
			writel(0x2aa, pwrctrl + 0x8);
			pr_debug("[DPRX]   Set PWRCTRL+0x8 = 0x2aa\n");
			writel(0x0, iso_base);
			pr_debug("[DPRX]   Set ISO_BASE = 0x0\n");
			writel(0x5, base + U3DP_PHY_DPHY_01);
			pr_debug("[DPRX]   Set U3DP_PHY_DPHY_01 = 0x5 (CC1 USB mode)\n");
			writel(0x9, base + U3DP_PHY_SPPHY_21);
			pr_debug("[DPRX]   Set U3DP_PHY_SPPHY_21 = 0x9\n");
		} else if (cc == enable_cc2) {
			pr_debug("[DPRX]   CC2 configuration (USB on L2-3)\n");
			writel(0x102aa, pwrctrl + 0x8);
			pr_debug("[DPRX]   Set PWRCTRL+0x8 = 0x102aa\n");
			writel(0x10000, iso_base);
			pr_debug("[DPRX]   Set ISO_BASE = 0x10000\n");
			writel(0x6, base + U3DP_PHY_DPHY_01);
			pr_debug("[DPRX]   Set U3DP_PHY_DPHY_01 = 0x6 (CC2 USB mode)\n");
			writel(0x9, base + U3DP_PHY_SPPHY_21);
			pr_debug("[DPRX]   Set U3DP_PHY_SPPHY_21 = 0x9\n");
		} else {
			pr_info("[DPRX]   Invalid CC: %d\n", cc);
			return;
		}
	}

	dphy_01_after = readl(base + U3DP_PHY_DPHY_01);
	pr_debug("[DPRX]   U3DP_PHY_DPHY_01 AFTER:  0x%x\n", dphy_01_after);
	return;
}

static void rtk_usb_k_offset(struct type_c_data *type_c)
{
	unsigned long phy_init_time = jiffies;
	uint32_t tap0_init_values[] = {2, 6, 10, 14, 18, 22, 26, 30};
	uint32_t reg_leq_offset_addr_lane0[] = {
		0x1c0, 0x1c4, 0x1c8, 0x1cc,
		0x1d0, 0x1d4, 0x1d8, 0x1dc
	};
	unsigned int sign, value, cal_result;
	int i;

	for (i = 0; i < 8; i++) {
		u3rx_dfe_tap0_init_val(type_c, tap0_init_values[i]);

		// LEQ Offset Cal. Sequence
		u3rx_l0_inoff_en_set(type_c, 0x1);
		u3rx_l3_inoff_en_set(type_c, 0x1);
		u3rx_l1_ck_pow_nc_set(type_c, 0x1);
		u3rx_l1_ck_pow_leq_koff_set(type_c, 0x1);
		u3rx_l3_pow_leq_set(type_c, 0x1);
		u3rx_reg_pow_cmfb_1p8_cdm_set(type_c, 0x1);
		u3rx_p0_r_off_z0_19_ok_eo_set(type_c, 0x0);
		u3rx_p0_r_off_z0_ok_eo_set(type_c, 0x0);
		u3rx_l0_ck_pow_nc_set(type_c, 0x1);
		u3rx_l0_ck_pow_leq_koff_set(type_c, 0x1);
		u3rx_p0_r_off_z0_ok_do_set(type_c, 0x0);
		u3rx_l2_ck_pow_leq_set(type_c, 0x1);
		u3rx_l0_pow_cmfb_1p8_cdm_set(type_c, 0x1);
		u3rx_p0_ck_off_z0_19_ok_eo_set(type_c, 0x0);
		u3rx_p0_ck_off_z0_ok_eo_set(type_c, 0x0);
		u3rx_p0_ck_off_z0_ok_do_set(type_c, 0x0);

		// Toggle Cal Start
		u3rx_p0_b_off_en_eq_set(type_c, 0x1);
		u3rx_p0_g_off_en_eq_set(type_c, 0x1);
		u3rx_p0_r_off_en_eq_set(type_c, 0x1);
		u3rx_p0_ck_off_en_eq_set(type_c, 0x1);
		udelay(100);
		u3rx_p0_b_off_en_eq_set(type_c, 0x0);
		u3rx_p0_g_off_en_eq_set(type_c, 0x0);
		u3rx_p0_r_off_en_eq_set(type_c, 0x0);
		u3rx_p0_ck_off_en_eq_set(type_c, 0x0);
		u3rx_l0_inoff_en_set(type_c, 0x0);
		u3rx_l3_inoff_en_set(type_c, 0x0);
		u3rx_p0_r_off_coef_sel_set(type_c, 0x8);

		sign = u3rx_get_bits(type_c, 0xd84, BIT(20));
		value = u3rx_get_bits(type_c, 0xd84, GENMASK(19, 16));

		if (sign)
			cal_result = 0x10 | (((~value) + 1) & 0xF);
		else
			cal_result = value;
		u3rx_update_bits(type_c, reg_leq_offset_addr_lane0[i], GENMASK(4, 0), cal_result);
	}

	u3rx_dfe_tap0_init_val(type_c, 15); // Tap0_init = 15
	u3rx_p0_r_off_manual_eq_set(type_c, 0x1); //offset_manual_eq = 1

	pr_debug("Initialized RTK USB 3.0 DP RX PHY (take %dms)\n",
		jiffies_to_msecs(jiffies - phy_init_time));
}

static int rtk_usb_type_c_plug_config(struct type_c_data *type_c, int cc, int lanes)
{
	struct rtk_phy *rtk_phy;
	int ret = 0;
	const char *state_str;

	rtk_phy = container_of(type_c, struct rtk_phy, type_c);

	pr_debug("[U3DPRX] ----- TYPE-C PLUG CONFIG START -----\n");
	pr_debug("[U3DPRX] Configuration Parameters:\n");
	pr_debug("[U3DPRX]   CC:           %d (1=CC1, 2=CC2, 0=Disable)\n", cc);
	pr_debug("[U3DPRX]   Target Lanes: %d (0=USB only, 2=DP+USB, 4=DP only)\n", lanes);

	switch (type_c->state) {
	case DP_4:
		state_str = "DP_4 (4-lane DP)";
		break;
	case USB_NON_FLIP:
		state_str = "USB_NON_FLIP (USB on CC1)";
		break;
	case USB_FLIP:
		state_str = "USB_FLIP (USB on CC2)";
		break;
	case USB_DP:
		state_str = "USB_DP (USB+DP combo)";
		break;
	case DP_USB:
		state_str = "DP_USB (DP+USB combo)";
		break;
	default:
		state_str = "UNKNOWN";
		break;
	}
	pr_debug("[U3DPRX]   Previous State: %s (%d)\n", state_str, type_c->state);

	if (cc == TYPEC_ORIENTATION_NONE) {
		pr_debug("[DPRX] CC disabled, skipping configuration\n");
		return ret;
	}

	if (type_c->state != DP_4)
		rtk_phy_restore_reg(rtk_phy);

	rtk_usb_type_c_lane_config(type_c, cc, lanes);
	switch (lanes)
	{
	case 4:
		break;
	case 2:
	case 0:
		if (cc == 1)
			rtk_type_c_usb01_setting(type_c);
		else if (cc == 2)
			rtk_type_c_usb23_setting(type_c);
		rtk_type_c_usb_parameter(type_c);
		rtk_usb_k_offset(type_c);
		break;
	default:
		break;
	}

	pr_debug("[DPRX] ----- TYPE-C PLUG CONFIG END -------\n");
	return ret;
}

static ssize_t dump_phy_calibration(struct type_c_data *typec, char *buf, size_t size)
{
	u32 val, sign;
	ssize_t len = 0;

	DUMP_KOFF("DATA_ODD",   0x1, GENMASK(19, 16), BIT(20));
	DUMP_KOFF("EDGE_EVEN",  0x2, GENMASK(19, 16), BIT(20));
	DUMP_KOFF("OFFP_ODD",   0x5, GENMASK(20, 17), BIT(21));
	DUMP_KOFF("EQ",         0x8, GENMASK(19, 16), BIT(20));

	DUMP_DFE("tap0_coef_bin", 0x0, GENMASK(22, 16), 0);
	DUMP_DFE("tap1_coef_bin", 0x1, GENMASK(19, 16), BIT(20));
	DUMP_DFE("tap2_coef_bin", 0x2, GENMASK(19, 16), BIT(20));
	DUMP_DFE("tap3_coef_bin", 0x3, GENMASK(19, 16), BIT(20));
	DUMP_DFE("tap4_coef_bin", 0x4, GENMASK(19, 16), BIT(20));
	DUMP_DFE("vthp_coef_bin", 0x7, GENMASK(20, 16), 0);
	DUMP_DFE("filter_bin_eq", 0xa, GENMASK(22, 18), 0);

	return len;
}

static ssize_t rx_state_change_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;

	return sprintf(buf, "0x%x\n", type_c->state);
}

static ssize_t rx_state_change_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;
	int cc_stat = 0;
	char tbuf[PAGE_SIZE];

	if (sysfs_streq(buf, "u1")) {
		dev_info(dev, "%s curret status: USB, cc1 host connect\n", __func__);
		type_c->state = USB_NON_FLIP;
		rtk_usb_type_c_plug_config(type_c, 1, 0);
		dump_phy_calibration(type_c, tbuf, PAGE_SIZE);
	} else if (sysfs_streq(buf, "u2")) {
		dev_info(dev, "%s curret status: USB, cc2 host connect\n", __func__);
		type_c->state = USB_FLIP;
		rtk_usb_type_c_plug_config(type_c, 2, 0);
	} else if (sysfs_streq(buf, "d")) {
		if (type_c->state == USB_NON_FLIP)
			cc_stat = 2;
		else if (type_c->state == USB_FLIP)
			cc_stat = 1;
		dev_info(dev, "%s curret status: DP\n", __func__);
		rtk_usb_type_c_lane_config(type_c, cc_stat, 4);
		rtk_dp_phy_init(rtk_phy->phy_dp);
		rtk_type_c_4dp_5p4g_configure(type_c);
		rtk_dp_phy_calibrate(rtk_phy->phy_dp);
		type_c->state = DP_4;
	} else if (sysfs_streq(buf, "c1")) {
		dev_info(dev, "%s curret status: USB_DP\n", __func__);
		type_c->state = USB_DP;
		rtk_usb_type_c_plug_config(type_c, 1, 2);
		dump_phy_calibration(type_c, tbuf, PAGE_SIZE);

		rtk_type_c_dp23rx_init(type_c);
		rtk_type_c_dp23_configure(type_c, 540000);
		rtk_dp23_phy_5p4g_calibrate(type_c);
	} else if (sysfs_streq(buf, "c2")) {
		dev_info(dev, "%s curret status: DP_USB\n", __func__);
		type_c->state = DP_USB;
		rtk_usb_type_c_plug_config(type_c, 2, 2);
		dump_phy_calibration(type_c, tbuf, PAGE_SIZE);

		rtk_type_c_dp01rx_init(type_c);
		rtk_type_c_dp01_configure(type_c, 540000);
		rtk_dp01_phy_5p4g_calibrate(type_c);
	} else {
		dev_info(dev, "%s ERROR INPUT\n", __func__);
		type_c->state = -1;
	}

	return count;
}
DEVICE_ATTR(rx_state_change, S_IRUGO | S_IWUSR, rx_state_change_show, rx_state_change_store);

static ssize_t rx_reg_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;
	ssize_t len = 0;
	unsigned int i;

	for (i = 0; i <= 0xFFC; i += 0x4)
		pr_info("0x%04X: 0x%08X\n", i, readl(type_c->base + i));

	return len;
}
static DEVICE_ATTR_RO(rx_reg_dump);

static ssize_t rx_reg_diff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;
	ssize_t len = 0;
	unsigned int i, current_val, saved_val;
	unsigned int start_idx = type_c->start_addr / 4;

	if (!rtk_phy->saved_reg)
		return scnprintf(buf, PAGE_SIZE, "Error: No saved registers found. Call save_reg first.\n");

	len += scnprintf(buf + len, PAGE_SIZE - len, "--- PHY Register Diff (Saved vs Current) ---\n");
	len += scnprintf(buf + len, PAGE_SIZE - len, "ADDR   | SAVED      | CURRENT\n");
	len += scnprintf(buf + len, PAGE_SIZE - len, "------------------------------\n");

	for (i = start_idx; i < 1024; i++) {
		current_val = readl(type_c->base + (i * 4));
		saved_val = rtk_phy->saved_reg[i];

		if (current_val != saved_val) {
			if (len + 60 >= PAGE_SIZE) {
				len += scnprintf(buf + len, PAGE_SIZE - len,
						"--- MORE DIFFS: echo 0x%X > start_addr ---\n", i * 4);
				break;
			}
			len += scnprintf(buf + len, PAGE_SIZE - len,
					"0x%04X | 0x%08X | 0x%08X\n", i * 4, saved_val, current_val);
		}
	}

	if (len <= 100)
		len += scnprintf(buf + len, PAGE_SIZE - len, "All registers match perfectly!\n");

	return len;
}

static ssize_t rx_reg_diff_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;
	unsigned int addr;

	if (kstrtouint(buf, 0, &addr) == 0) {
		addr &= ~0xFF;
		type_c->start_addr = addr;
		return count;
	} else {
		return -EINVAL;
	}
}

DEVICE_ATTR(rx_reg_diff, S_IRUGO | S_IWUSR, rx_reg_diff_show, rx_reg_diff_store);

static ssize_t usb_koff_result_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;

	return dump_phy_calibration(type_c, buf, PAGE_SIZE);
}
static DEVICE_ATTR_RO(usb_koff_result);

/*
 * HPD (Hot Plug Detect) Functions for UFP_D (DP Sink/Rx)
 */
/**
 * rtk_usb3dprx_send_hpd - Send HPD status via Attention VDM
 * @rtk_phy: The RTK PHY device
 * @hpd: HPD state (true = HIGH, false = LOW)
 * @irq: IRQ HPD state
 *
 * This function sends HPD status change to the DFP_D (DP Source) via
 * USB PD Attention VDM. This is the standard way for UFP_D to notify
 * the source about monitor connection status.
 *
 * Return: 0 on success, negative error code on failure
 */
static int rtk_usb3dprx_send_hpd(struct rtk_phy *rtk_phy, bool high, bool irq)
{
	struct typec_altmode *alt = rtk_phy->dp_altmode;
	u32 status_vdo;
	int ret;

	dev_dbg(rtk_phy->dev, "[DPRX] === HPD Send Request ===\n");
	dev_dbg(rtk_phy->dev, "[DPRX]  dp_hpd_high=%d ==> HPD state: %s%s\n",
		 rtk_phy->dp_hpd_high, high ? "HIGH" : "LOW", irq ? " (IRQ)" : "");
	dev_dbg(rtk_phy->dev, "[DPRX]   dp_altmode: %p\n", alt);

	if (!alt) {
		dev_err(rtk_phy->dev, "[DPRX] FAILED: No DP alt mode registered!\n");
		dev_err(rtk_phy->dev, "[DPRX] Possible reasons:\n");
		dev_err(rtk_phy->dev, "[DPRX]   1. Type-C port not in DP Alt Mode\n");
		dev_err(rtk_phy->dev, "[DPRX]   2. mux_set not called with DP mode yet\n");
		dev_err(rtk_phy->dev, "[DPRX]   3. state->alt was NULL in mux_set\n");
		dev_err(rtk_phy->dev, "[DPRX] Check: dmesg | grep 'DP Alt Mode registered'\n");
		return -ENODEV;
	}

	if (!irq && rtk_phy->dp_hpd_high == high)
		return 0;

	/*
	 * Build Status VDO with HPD bits.  The full Attention VDM header
	 * (SVID, SVDM version, CMD_ATTENTION) is constructed inside
	 * dp_rx_send_attention() which runs on the dp_rx workqueue after
	 * any pending VDM response has cleared.
	 */
	status_vdo = DP_STATUS_ENABLED | DP_STATUS_CON_UFP_D;
	if (high)
		status_vdo |= DP_STATUS_HPD_STATE;
	if (irq)
		status_vdo |= DP_STATUS_IRQ_HPD;

	dev_dbg(rtk_phy->dev, "[DPRX] Sending HPD %s via typec_altmode_attention\n",
		 high ? "HIGH" : "LOW");
	dev_dbg(rtk_phy->dev, "[DPRX]   Status VDO: 0x%08x\n", status_vdo);

	/*
	 * typec_altmode_attention(alt, vdo) routes to
	 *   partner_of(alt)->ops->attention  =  dp_rx_altmode_ops.attention
	 * which queues the actual Attention VDM send on dp_rx's workqueue.
	 *
	 * alt (dp_altmode) is the same altmode TCPM passes to
	 * typec_altmode_vdm() for incoming VDMs — its partner carries the
	 * driver ops.
	 */
	ret = typec_altmode_attention(alt, status_vdo);

	if (ret)
		dev_err(rtk_phy->dev,
			"[DPRX] Failed to queue HPD %s Attention, ret=%d\n",
			high ? "HIGH" : "LOW", ret);
	else
		rtk_phy->dp_hpd_high = high;

	return ret;
}

/* Sysfs: Register DP Alt Mode */
static ssize_t dp_altmode_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "dp_altmode: %s\n",
			 rtk_phy->dp_altmode ? "registered" : "not registered");
}

static ssize_t dp_altmode_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	if (val) {
		if (rtk_phy->dp_altmode) {
			dev_info(dev, "DP alt mode already registered\n");
		} else {
			dev_info(dev, "Note: DP alt mode should be registered by Type-C framework\n");
			dev_info(dev, "This sysfs is for manual testing only\n");
		}
	} else {
		if (rtk_phy->dp_altmode) {
			rtk_phy->dp_altmode = NULL;
			dev_info(dev, "DP alt mode unregistered\n");
		}
	}

	return count;
}
DEVICE_ATTR(dp_altmode_register, S_IRUGO | S_IWUSR, dp_altmode_register_show, dp_altmode_register_store);

/* Sysfs: Send HPD */
static ssize_t hpd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
			 "Usage: echo 0/1 > hpd\n"
			 "  0 = Send HPD LOW\n"
			 "  1 = Send HPD HIGH\n");
}

static ssize_t hpd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	ret = rtk_usb3dprx_send_hpd(rtk_phy, !!val, false);
	if (ret)
		return ret;

	return count;
}
DEVICE_ATTR(hpd, S_IRUGO | S_IWUSR, hpd_show, hpd_store);

/* Sysfs: DP Alt Mode Status */
static ssize_t dp_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;
	ssize_t len = 0;

	len += scnprintf(buf + len, PAGE_SIZE - len, "=== DP Alt Mode Status ===\n");
	len += scnprintf(buf + len, PAGE_SIZE - len, "DP Alt Mode: %s\n",
			 rtk_phy->dp_altmode ? "Registered" : "Not Registered");
	len += scnprintf(buf + len, PAGE_SIZE - len, "Lanes: %u\n", rtk_phy->lanes);
	len += scnprintf(buf + len, PAGE_SIZE - len, "Link Rate: %u Mbps\n", rtk_phy->link_rate);
	len += scnprintf(buf + len, PAGE_SIZE - len, "Type-C State: ");

	switch (type_c->state) {
	case DP_4:
		len += scnprintf(buf + len, PAGE_SIZE - len, "DP_4 (4-lane DP)\n");
		break;
	case USB_NON_FLIP:
		len += scnprintf(buf + len, PAGE_SIZE - len, "USB_NON_FLIP (USB only)\n");
		break;
	case USB_DP:
		len += scnprintf(buf + len, PAGE_SIZE - len, "USB_DP (USB + 2-lane DP)\n");
		break;
	case USB_FLIP:
		len += scnprintf(buf + len, PAGE_SIZE - len, "USB_FLIP (USB only, flipped)\n");
		break;
	case DP_USB:
		len += scnprintf(buf + len, PAGE_SIZE - len, "DP_USB (2-lane DP + USB)\n");
		break;
	default:
		len += scnprintf(buf + len, PAGE_SIZE - len, "Unknown (%d)\n", type_c->state);
		break;
	}

	len += scnprintf(buf + len, PAGE_SIZE - len, "Flip: %d\n", rtk_phy->flip);

	return len;
}
DEVICE_ATTR(dp_status, S_IRUGO, dp_status_show, NULL);

static int rtk_udphy_orien_sw_set(struct typec_switch_dev *sw,
                                 enum typec_orientation orien)
{
	struct rtk_phy *rtk_phy = typec_switch_get_drvdata(sw);
	int cc = 0;
	const char *orien_str;

	pr_debug("[DPRX] ===== ORIENTATION SWITCH CALLED =====\n");

	mutex_lock(&rtk_phy->mutex);
	if (orien == TYPEC_ORIENTATION_NORMAL) {
		cc = enable_cc1;
		orien_str = "NORMAL (CC1)";
	} else if (orien == TYPEC_ORIENTATION_REVERSE) {
		cc = enable_cc2;
		orien_str = "REVERSE (CC2)";
	} else if (orien == TYPEC_ORIENTATION_NONE) {
		cc = disable_cc;
		orien_str = "NONE (Disconnected)";
	} else {
		orien_str = "UNKNOWN";
	}

	pr_debug("[DPRX]   Orientation: %s (enum=%d)\n", orien_str, orien);
	pr_debug("[DPRX]   CC Setting:  %d (1=CC1, 2=CC2, 0=Disabled)\n", cc);
	pr_debug("[DPRX]   Previous flip: %d\n", rtk_phy->flip);

	rtk_phy->flip = cc;

	pr_debug("[DPRX]   New flip:      %d\n", rtk_phy->flip);

	mutex_unlock(&rtk_phy->mutex);
	return 0;
}

static void rtk_udphy_orien_switch_unregister(void *data)
{
	struct rtk_phy *rtk_phy = data;

	typec_switch_unregister(rtk_phy->sw);
}

static int rtk_typec_switch_register(struct rtk_phy *rtk_phy)
{
	struct typec_switch_desc sw_desc = { };
	struct device *dev = rtk_phy->dev;

	sw_desc.drvdata = rtk_phy;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = rtk_udphy_orien_sw_set;

	rtk_phy->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(rtk_phy->sw)) {
		dev_err(rtk_phy->dev, "Error register typec orientation switch: %ld\n",
			PTR_ERR(rtk_phy->sw));
		return PTR_ERR(rtk_phy->sw);
	}

	pr_info("rtk_u3dprx_phy_setup_typec_switch\n");
	return devm_add_action_or_reset(dev,
				rtk_udphy_orien_switch_unregister, rtk_phy);
}

static int rtk_udphy_typec_mux_set(struct typec_mux_dev *mux,
                                 struct typec_mux_state *state)
{
        struct rtk_phy *rtk_phy = typec_mux_get_drvdata(mux);
	struct type_c_data *type_c = &rtk_phy->type_c;
	struct typec_displayport_data *data = state->data;
	int cc = 0;
	int old_state;
	u8 lanes;
	bool polarity = false;
	bool dp = false;
	const char *mode_str;

	pr_debug("[U3DPRX] ===== MODE/MUX SWITCH CALLED =====\n");
	pr_debug("[U3DPRX]   State mode:     0x%lx\n", state->mode);
	pr_debug("[U3DPRX]   State alt:      %p\n", state->alt);
	pr_debug("[U3DPRX]   Current flip:   %d (1=CC1, 2=CC2)\n", rtk_phy->flip);
	pr_debug("[U3DPRX]   Previous lanes: %d\n", rtk_phy->lanes);

	if (state->alt && state->alt->svid == USB_TYPEC_DP_SID) {
		rtk_phy->dp_altmode = state->alt;
		pr_debug("[DPRX] DP Alt Mode registered: %p (SVID=0x%04x)\n",
			rtk_phy->dp_altmode, state->alt->svid);
	} else if (state->alt) {
		pr_debug("[DPRX] Alt mode present but not DP (SVID=0x%04x)\n",
			state->alt->svid);
	} else {
		pr_err("[DPRX] No alt mode in state\n");
		rtk_phy->dp_altmode = NULL;
		rtk_phy->dp_hpd_high = false;
	}

	mutex_lock(&rtk_phy->mutex);
	switch (state->mode) {
	case TYPEC_STATE_SAFE:
		rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_USB;
		rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_USB;
		rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_USB;
		rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_USB;
		lanes = 0;
		mode_str = "Unplug / Safe mode";
		break;

	case TYPEC_STATE_USB:
		rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_USB;
		rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_USB;
		rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_USB;
		rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_USB;
		lanes = 0;
		if (rtk_phy->flip == enable_cc1)
			type_c->state = USB_NON_FLIP;
		else
			type_c->state = USB_FLIP;
		mode_str = "USB Mode (All 4 lanes USB)";
		break;

	case TYPEC_DP_STATE_C:
	case TYPEC_DP_STATE_E:
		rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_DP;
		rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_DP;
		rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_DP;
		rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_DP;
		lanes = 4;
		type_c->state = DP_4;
		mode_str = (state->mode == TYPEC_DP_STATE_C) ?
			   "DP Mode C (4-lane DP)" : "DP Mode E (4-lane DP)";
		break;

	case TYPEC_DP_STATE_D:
	default:
		if (rtk_phy->flip == enable_cc2) {
			rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_DP;
			rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_DP;
			rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_USB;
			rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_USB;
			type_c->state = DP_USB;
			mode_str = "DP Mode D - CC2 (Lane0-1=DP, Lane2-3=USB)";
		} else {
			rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_USB;
			rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_USB;
			rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_DP;
			rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_DP;
			type_c->state = USB_DP;
			mode_str = "DP Mode D - CC1 (Lane0-1=USB, Lane2-3=DP)";
		}
		lanes = 2;
		break;
	}

	cc = rtk_phy->flip;
	rtk_phy->lanes = lanes;

	pr_debug("[DPRX]   Mode:           %s\n", mode_str);
	pr_debug("[DPRX]   DP Lanes:       %d\n", lanes);
	pr_debug("[DPRX]   Lane Mux Config: [L0=%s, L1=%s, L2=%s, L3=%s]\n",
		rtk_phy->lane_mux_sel[0] == PHY_LANE_MUX_DP ? "DP" : "USB",
		rtk_phy->lane_mux_sel[1] == PHY_LANE_MUX_DP ? "DP" : "USB",
		rtk_phy->lane_mux_sel[2] == PHY_LANE_MUX_DP ? "DP" : "USB",
		rtk_phy->lane_mux_sel[3] == PHY_LANE_MUX_DP ? "DP" : "USB");

	if (state->mode != TYPEC_STATE_SAFE) {
		pr_info("[DPRX] Calling rtk_usb_type_c_plug_config(cc=%d, lanes=%d)\n", cc, lanes);
		rtk_usb_type_c_plug_config(type_c, cc, lanes);
	} else {
		pr_info("[DPRX] Unplug\n");
	}

	if (state->alt && state->alt->svid == USB_TYPEC_DP_SID && data)
		dp = true;
	else
                dp = false;
	polarity = (rtk_phy->flip == enable_cc2) ? 1 : 0;
	old_state = extcon_get_state(type_c->phy_edev, EXTCON_DISP_DP);

	pr_debug("[DPRX] Setting EXTCON properties:\n");
	pr_debug("[DPRX]   EXTCON_DISP_DP: %s\n", dp ? "Connected" : "Disconnected");
	pr_debug("[DPRX]   Polarity:       %s\n", polarity ? "Reversed (CC2)" : "Normal (CC1)");

	if (dp != old_state) {
		if (dp) {
			extcon_set_property(type_c->phy_edev, EXTCON_DISP_DP,
				EXTCON_PROP_USB_TYPEC_POLARITY,
				(union extcon_property_value)(int)polarity);
			extcon_set_state_sync(type_c->phy_edev, EXTCON_DISP_DP, dp);
		} else {
			pr_debug("[DPRX] DP Disconnected.\n");
			extcon_set_state_sync(type_c->phy_edev, EXTCON_DISP_DP, false);
		}
	}
	mutex_unlock(&rtk_phy->mutex);

        return 0;
}

static void rtk_udphy_typec_mux_unregister(void *data)
{
	struct rtk_phy *rtk_phy = data;

	typec_mux_unregister(rtk_phy->mux);
}

static int rtk_typec_mux_register(struct rtk_phy *rtk_phy)
{
	struct typec_mux_desc mux_desc = {};
        struct device *dev = rtk_phy->dev;

        mux_desc.drvdata = rtk_phy;
        mux_desc.fwnode = dev->fwnode;
        mux_desc.set = rtk_udphy_typec_mux_set;

        rtk_phy->mux = typec_mux_register(dev, &mux_desc);
        if (IS_ERR(rtk_phy->mux)) {
                dev_err(rtk_phy->dev, "Error register typec orientation switch: %ld\n",
                        PTR_ERR(rtk_phy->mux));
                return PTR_ERR(rtk_phy->mux);
        }

	pr_info("rtk_u3dprx_phy_setup_mux_switch\n");
        return devm_add_action_or_reset(dev,
			rtk_udphy_typec_mux_unregister, rtk_phy);
}

static int rtk_dprx_get_port_lanes(struct type_c_data *type_c)
{
	union extcon_property_value property;
	int dprx;
	u8 lanes;

	dprx = extcon_get_state(type_c->edev, EXTCON_DISP_DP);
	if (dprx > 0) {
		extcon_get_property(type_c->edev, EXTCON_DISP_DP,
				EXTCON_PROP_USB_SS, &property);
		if (property.intval)
			lanes = 2;
		else
			lanes = 4;
	} else {
		lanes = 0;
	}

	pr_info("[U3DPRX] get lanes: %d\n", lanes);
	return lanes;
}

static int __rtk_u3dprx_update(struct type_c_data *type_c, int device_state)
{
	enum typec_orientation orientation;
	enum usb_role usb_role = USB_ROLE_NONE;
	int polarity = 0;
	int cc = 0;
	int lanes = 0;
	int host_state = 0;
	int mux = 0;
	bool is_attach = false;

	if (device_state) {
		usb_role = USB_ROLE_DEVICE;
		extcon_get_property(type_c->edev, EXTCON_USB,
			    EXTCON_PROP_USB_TYPEC_POLARITY,
			    (union extcon_property_value *)&polarity);
		extcon_get_property(type_c->edev, EXTCON_USB,
			    EXTCON_PROP_USB_SS,
			    (union extcon_property_value *)&mux);
		is_attach = true;
	} else {
		usb_role = USB_ROLE_HOST;
		lanes = rtk_dprx_get_port_lanes(type_c);

		if (lanes == 4) {
			extcon_get_property(type_c->edev, EXTCON_DISP_DP,
					EXTCON_PROP_USB_TYPEC_POLARITY,
					(union extcon_property_value *)&polarity);
			extcon_get_property(type_c->edev, EXTCON_DISP_DP,
					EXTCON_PROP_USB_SS,
					(union extcon_property_value *)&mux);
		} else {
			extcon_get_property(type_c->edev, EXTCON_USB_HOST,
					EXTCON_PROP_USB_TYPEC_POLARITY,
					(union extcon_property_value *)&polarity);
			extcon_get_property(type_c->edev, EXTCON_USB_HOST,
					EXTCON_PROP_USB_SS,
				(union extcon_property_value *)&mux);
		}

		host_state = extcon_get_state(type_c->edev, EXTCON_USB_HOST);
			if (host_state > 0)
				is_attach = true;
        }

	if (polarity == 0) {
		cc = enable_cc1;
		orientation = TYPEC_ORIENTATION_NORMAL;
	} else if (polarity == 1) {
		cc = enable_cc2;
		orientation = TYPEC_ORIENTATION_REVERSE;
	} else {
		pr_err("error polarity value\n");
	}

	if (!is_attach)
		cc = TYPEC_ORIENTATION_NONE;

	pr_info("[U3DPRX] polarity = %d cc = %d mux = %d lanes = %d, is_attach = %d\n",
			polarity, cc, mux, lanes, is_attach);

	rtk_usb_type_c_plug_config(type_c, cc, lanes);

	return NOTIFY_DONE;
}

static int __rtk_u3dprx_notifier(struct notifier_block *nb,
                             unsigned long event, void *ptr)
{
	struct type_c_data *type_c = container_of(nb, struct type_c_data, edev_nb);
	int state = event;

	__rtk_u3dprx_update(type_c, state);
        return NOTIFY_DONE;
}

static void __rtk_u3_type_c_check(struct type_c_data *type_c)
{
	int state;

	if (!type_c->edev)
		return;

	state = extcon_get_state(type_c->edev, EXTCON_USB);
	if (state < 0)
		state = 0;

	pr_info("%s EXTCON_USB state = %d\n", __func__, state);

	__rtk_u3dprx_update(type_c, state);
}

static int u3dprx_phy_setup_type_c_device(struct rtk_phy *rtk_phy)
{
	struct type_c_data *type_c;
	struct device *dev = rtk_phy->dev;
	struct device_node *node = NULL;
	int ret;

	type_c = &rtk_phy->type_c;

	if (!rtk_phy || !rtk_phy->dev) {
		pr_err("[U3DPRX] rtk_phy or dev is NULL!\n");
		return -EINVAL;
	}

	node = of_parse_phandle(dev->of_node, "realtek,extcon-dev", 0);
	if (node) {
		type_c->edev = extcon_find_edev_by_node(node);
		of_node_put(node);

		if (IS_ERR(type_c->edev)) {
			ret = PTR_ERR(type_c->edev);
			if (ret == -EPROBE_DEFER) {
				dev_info(dev, "Type-C driver not ready yet, deferring probe..., ret = %d\n", ret);
				return -EPROBE_DEFER;
			}
			dev_err(dev, "[U3DPRX] Failed to get extcon edev (ret = %d)\n", ret);
			return ret;
		}

		type_c->edev_nb.notifier_call = __rtk_u3dprx_notifier;
		ret = extcon_register_notifier(type_c->edev, EXTCON_USB, &type_c->edev_nb);
		if (ret < 0) {
			dev_err(dev, "[U3DPRX] couldn't register cable notifier\n");
			return ret;
		} else {
			dev_dbg(dev, "success\n");
			__rtk_u3_type_c_check(type_c);
		}
	} else {
		dev_info(dev, "NO realtek,extcon-dev phandle exist\n");
	}

	ret = rtk_typec_switch_register(rtk_phy);
	if (ret) {
		pr_err("[U3DPRX] rtk_typec_switch_register failed\n");
		return ret;
	}
	ret = rtk_typec_mux_register(rtk_phy);
	if (ret) {
		pr_err("[U3DPRX] rtk_typec_mux_register failed\n");
		return ret;
	}

	type_c->phy_edev = devm_extcon_dev_allocate(dev, usb_type_c_cable);
	if (IS_ERR(type_c->phy_edev)) {
		dev_err(dev, "[U3DPRX] failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, type_c->phy_edev);
	if (ret < 0) {
		dev_err(dev, "[U3DPRX] failed to register extcon device\n");
		return ret;
	}

	extcon_set_property_capability(type_c->phy_edev, EXTCON_DISP_DP,
		EXTCON_PROP_USB_TYPEC_POLARITY);

        return 0;
}

static int do_rtk_phy_init(struct rtk_phy *rtk_phy)
{
	struct type_c_data *type_c;

	type_c = &rtk_phy->type_c;

	rtk_usb_type_c_lane_config(type_c, 1, 0);
	rtk_type_c_usb01_setting(type_c);
	rtk_type_c_usb_parameter(type_c);

	type_c->ss = true;
	type_c->state = USB_NON_FLIP;
	mdelay(1);

	return 0;
}

static void rtk_phy_restore_reg(struct rtk_phy *rtk_phy)
{
	struct type_c_data *type_c;
	void __iomem *base;
	int i;

	type_c = &rtk_phy->type_c;
	base = type_c->base;

	if (!rtk_phy->saved_reg) {
		pr_warn("USB PHY: No reg is available to restore!\n");
		return;
	}

	pr_debug("USB PHY: Restoring registers...\n");

	for (i = 0; i < 1024; i++)
		writel(rtk_phy->saved_reg[i], base + (i * 4));

	pr_debug("USB PHY: Restore complete.\n");
}

static void rtk_phy_save_reg(struct rtk_phy *rtk_phy)
{
	struct type_c_data *type_c;
	void __iomem *base;
	int i;

	type_c = &rtk_phy->type_c;
	base = type_c->base;

	if (!rtk_phy->saved_reg) {
		rtk_phy->saved_reg = kmalloc(1024 * sizeof(u32), GFP_KERNEL);
		if (!rtk_phy->saved_reg) {
			pr_err("USB PHY: Failed to allocate memory for saved_reg!\n");
			return;
		}
	}

	for (i = 0; i < 1024; i++)
		rtk_phy->saved_reg[i] = readl(base + (i * 4));

	pr_debug("USB PHY: reg saved successfully.\n");
}

static int rtk_phy_init(struct phy *phy)
{
	struct rtk_phy *rtk_phy = phy_get_drvdata(phy);
	struct type_c_data *type_c;

	mutex_lock(&rtk_phy->mutex);

	rtk_phy_save_reg(rtk_phy);
	do_rtk_phy_init(rtk_phy);

	type_c = &rtk_phy->type_c;
	rtk_usb_k_offset(type_c);

	mutex_unlock(&rtk_phy->mutex);
	return 0;
}

static int rtk_phy_exit(struct phy *phy)
{
	return 0;
}

static const struct phy_ops usb3_ops = {
	.init           = rtk_phy_init,
	.exit           = rtk_phy_exit,
	.owner          = THIS_MODULE,
};

static int rtk_dp_phy_init(struct phy *phy)
{
	struct rtk_phy *rtk_phy = phy_get_drvdata(phy);
	struct type_c_data *type_c;

	type_c = &rtk_phy->type_c;

	if (rtk_phy->lanes == 4) {
		rtk_phy_restore_reg(rtk_phy);
		rtk_usb_type_c_lane_config(type_c, 0, 4);
		rtk_type_c_dprx_init(type_c);
	} else {
		if (rtk_phy->flip)
			rtk_type_c_dp23rx_init(type_c);
		else
			rtk_type_c_dp01rx_init(type_c);
	}

	return 0;
}

static int rtk_dp_phy_exit(struct phy *phy)
{
	return 0;
}

static int rtk_dp_phy_verify_link_rate(struct rtk_phy *rtk_phy,
					struct phy_configure_opts_dp *dp)
{
	switch (dp->link_rate) {
	case 162000:
	case 270000:
	case 540000:
		rtk_phy->link_rate = dp->link_rate;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rtk_dp_phy_verify_lanes(struct rtk_phy *rtk_phy,
                                        struct phy_configure_opts_dp *dp)
{
        switch (dp->lanes) {
        case 0:
        case 2:
        case 4:
                /* valid lane count. */
                rtk_phy->lanes = dp->lanes;
                break;
        default:
                return -EINVAL;
        }

        return 0;
}

static int rtk_dp_phy_configure(struct phy *phy,
				union phy_configure_opts *opts)
{
	struct rtk_phy *rtk_phy = phy_get_drvdata(phy);
	struct phy_configure_opts_dp *dp = &opts->dp;
	struct type_c_data *type_c = &rtk_phy->type_c;
	int ret = 0;

	if (dp->set_rate) {
		ret = rtk_dp_phy_verify_link_rate(rtk_phy, dp);
		if (ret)
			return ret;
	}

	if (dp->set_lanes) {
		ret = rtk_dp_phy_verify_lanes(rtk_phy, dp);
		if (ret)
			return ret;
	}

	if (dp->set_rate) {
		if (dp->lanes == 4) {
			ret = rtk_type_c_dp0123_configure(type_c, dp->link_rate);
		} else if (dp->lanes == 2 && rtk_phy->flip == 1) {
			rtk_type_c_dp23_configure(type_c, dp->link_rate);
		} else if (dp->lanes == 2 && rtk_phy->flip == 2) {
			rtk_type_c_dp01_configure(type_c, dp->link_rate);
		} else {
			pr_err("wrong input, lanes = %d, rate = %d, cc = %d\n",
				rtk_phy->lanes, dp->link_rate, rtk_phy->flip);
			return -EINVAL;
		}
	}
	return ret;
}

static int rtk_dp_phy_calibrate_setting(struct type_c_data *type_c, struct rtk_phy *rtk_phy)
{
	if (rtk_phy->lanes == 4) {
		switch(rtk_phy->link_rate) {
		case 540000:
			rtk_dp_phy_5p4g_calibrate(type_c);
			break;
		case 270000:
			rtk_dp_phy_2p7g_calibrate(type_c);
			break;
		case 162000:
			rtk_dp_phy_1p62g_calibrate(type_c);
			break;
		default:
			pr_err("unsupported\n");
			return -EINVAL;
		}
	} else if (rtk_phy->lanes == 2) {
		if (rtk_phy->flip == 1)
			rtk_dp23_phy_5p4g_calibrate(type_c);
		else
			rtk_dp01_phy_5p4g_calibrate(type_c);
	} else {
		pr_err("invlaid calibrate case\n");
		return -EINVAL;
	}

	return 0;
}

static int rtk_dp_phy_calibrate(struct phy *phy)
{
	struct rtk_phy *rtk_phy = phy_get_drvdata(phy);
	struct type_c_data *type_c;
	int ret = 0;

	type_c = &rtk_phy->type_c;
	ret = rtk_dp_phy_calibrate_setting(type_c, rtk_phy);

	return ret;
}

/**
 * rtk_dp_phy_connect - Handle DP PHY connection event
 * @phy: The PHY device
 * @port: Port number (unused for DP)
 *
 * Called when the DP PHY is connected. This automatically sends
 * HPD HIGH to notify the DFP_D that UFP_D is ready.
 *
 * Return: 0 on success, negative error code on failure
 */
static int rtk_dp_phy_connect(struct phy *phy, int port)
{
	struct rtk_phy *rtk_phy = phy_get_drvdata(phy);
	int ret;

	dev_dbg(rtk_phy->dev, "[DPRX] DP PHY connect event (port=%d)\n", port);

	/* Send HPD HIGH to notify DFP_D */
	ret = rtk_usb3dprx_send_hpd(rtk_phy, true, false);
	if (ret) {
		dev_err(rtk_phy->dev, "[DPRX] Failed to send HPD HIGH on connect: %d\n", ret);
		return ret;
	}

	dev_dbg(rtk_phy->dev, "[DPRX] DP PHY connected, HPD HIGH sent\n");
	return 0;
}

/**
 * rtk_dp_phy_disconnect - Handle DP PHY disconnection event
 * @phy: The PHY device
 * @port: Port number (unused for DP)
 *
 * Called when the DP PHY is disconnected. This automatically sends
 * HPD LOW to notify the DFP_D that UFP_D is no longer available.
 *
 * Return: 0 on success, negative error code on failure
 */
static int rtk_dp_phy_disconnect(struct phy *phy, int port)
{
	struct rtk_phy *rtk_phy = phy_get_drvdata(phy);
	int ret;

	dev_dbg(rtk_phy->dev, "[DPRX] DP PHY disconnect event (port=%d)\n", port);

	/* Send HPD LOW to notify DFP_D */
	ret = rtk_usb3dprx_send_hpd(rtk_phy, false, false);
	if (ret) {
		dev_err(rtk_phy->dev, "[DPRX] Failed to send HPD LOW on disconnect: %d\n", ret);
		return ret;
	}

	dev_dbg(rtk_phy->dev, "[DPRX] DP PHY disconnected, HPD LOW sent\n");
	return 0;
}

static const struct phy_ops dp_phy_ops = {
	.init           = rtk_dp_phy_init,
	.exit           = rtk_dp_phy_exit,
	.configure      = rtk_dp_phy_configure,
	.calibrate	= rtk_dp_phy_calibrate,
	.connect        = rtk_dp_phy_connect,
	.disconnect     = rtk_dp_phy_disconnect,
	.owner          = THIS_MODULE,
};

static struct phy *rtk_udphy_phy_xlate(struct device *dev, const struct of_phandle_args *args)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);

	if (args->args_count == 0)
		return ERR_PTR(-EINVAL);

	switch (args->args[0]) {
	case PHY_TYPE_USB3:
		return rtk_phy->phy_u3;
	case PHY_TYPE_DP:
		return rtk_phy->phy_dp;
	}

	return ERR_PTR(-EINVAL);
}

static struct attribute *rtk_dprx_phy_attrs[] = {
	&dev_attr_rx_state_change.attr,
	&dev_attr_rx_reg_dump.attr,
	&dev_attr_rx_reg_diff.attr,
	&dev_attr_usb_koff_result.attr,
	&dev_attr_dp_altmode_register.attr,
	&dev_attr_hpd.attr,
	&dev_attr_dp_status.attr,
	NULL,
};

static const struct attribute_group rtk_dprx_phy_attr_group = {
	.attrs = rtk_dprx_phy_attrs,
};

static void rtk_u3dprx_sub_probe_work(struct work_struct *work)
{
	struct rtk_phy *rtk_phy = container_of(work, struct rtk_phy, delayed_work.work);
	struct device *dev = rtk_phy->dev;
	int ret = 0;
	unsigned long probe_time = jiffies;

	dev_info(dev, "%s Start ...\n", __func__);

	ret = u3dprx_phy_setup_type_c_device(rtk_phy);
	if (ret == -EPROBE_DEFER)
                queue_delayed_work(rtk_phy->wq_typec_phy, &rtk_phy->delayed_work,
                        msecs_to_jiffies(5000));


	dev_info(dev, "%s ... End (take %d ms)\n", __func__,
			jiffies_to_msecs(jiffies - probe_time));
}

static const struct rtk_pwr_reg_entry type_c_phy_stage1[] = {
	{ 0xe00, 0x1c001c00 }, { 0xe04, 0x1c001c00 }, { 0xe08, 0x20002000 },
	{ 0xe0c, 0x20002000 }, { 0xe10, 0x80008000 }, { 0xe14, 0x80008000 },
	{ 0xe18, 0x07000700 }, { 0xe1c, 0x07000700 }, { 0xe20, 0x04000400 },
	{ 0xe24, 0x04000400 }, { 0xe28, 0x1f001f00 }, { 0xe2c, 0x1f001f00 },
	{ 0xe30, 0x70007000 }, { 0xe34, 0x70007000 }, { 0xe38, 0x75007500 },
	{ 0xe3c, 0x75007500 }, { 0xe40, 0x33003300 }, { 0xe44, 0x33003300 },
	{ 0xe48, 0xc000c000 }, { 0xe4c, 0xc000c000 }, { 0xe50, 0x4b004b00 },
	{ 0xe54, 0x4b004b00 }, { 0xe58, 0x60006000 }, { 0xe5c, 0x60006000 },
	{ 0xe60, 0x06100050 }, { 0xe64, 0x61006100 }, { 0xe68, 0x03006100 },
	{ 0xe6c, 0x03000300 }, { 0xe70, 0x04000300 }, { 0xe74, 0x04000400 },
	{ 0xe78, 0xf8000400 }, { 0xe7c, 0xf800f800 }, { 0xe80, 0x1c00f800 },
	{ 0xe84, 0x1c001c00 }, { 0xe88, 0xb5001c00 }, { 0xe8c, 0xb500b500 },
	{ 0xe90, 0x0000b500 }, { 0xe94, 0x00000000 }, { 0xe98, 0x00000000 },
	{ 0xe9c, 0x00000000 }, { 0xea0, 0x00000000 }, { 0xea4, 0x00000000 },
	{ 0xea8, 0x50000000 }, { 0xeac, 0x50005000 }, { 0xeb0, 0x18005000 },
	{ 0xeb4, 0x18001800 }, { 0xeb8, 0x00001800 }, { 0xebc, 0x00000000 },
	{ 0xec0, 0x5e000000 }, { 0xec4, 0x5e005e00 }, { 0xec8, 0x57005e00 },
	{ 0xecc, 0x57005700 }, { 0xed0, 0x74005700 }, { 0xed4, 0x74007400 },
	{ 0xed8, 0xfd007400 }, { 0xedc, 0xfd00fd00 }, { 0xee0, 0xff00fd00 },
	{ 0xee4, 0xff00ff00 }, { 0xee8, 0xe400ff00 }, { 0xeec, 0xe400e400 },
	{ 0xef0, 0xb300e400 }, { 0xef4, 0xb300b300 }, { 0xef8, 0x0d00b300 },
	{ 0xefc, 0x0d000d00 }, { 0xf00, 0x0c300d00 }, { 0xf14, 0xf0aa05ea },
	{ 0xf10, 0xa802abc3 },
};

static const struct rtk_pwr_reg_entry type_c_phy_stage2[] = {
	{ 0xe00, 0x1c1c1c1c }, { 0xe04, 0x1c1c1c1c }, { 0xe08, 0x20202020 },
	{ 0xe0c, 0x20202020 }, { 0xe10, 0x80808080 }, { 0xe14, 0x80808080 },
	{ 0xe18, 0x07070707 }, { 0xe1c, 0x07070707 }, { 0xe20, 0x04040404 },
	{ 0xe24, 0x04040404 }, { 0xe28, 0x1f1f1f1f }, { 0xe2c, 0x1f1f1f1f },
	{ 0xe30, 0x70707070 }, { 0xe34, 0x70707070 }, { 0xe38, 0x75757575 },
	{ 0xe3c, 0x75757575 }, { 0xe40, 0x33333333 }, { 0xe44, 0x33333333 },
	{ 0xe48, 0xc0c0c0c0 }, { 0xe4c, 0xc0c0c0c0 }, { 0xe50, 0x4b4b4b4b },
	{ 0xe54, 0x4b4b4b4b }, { 0xe58, 0x60596059 }, { 0xe5c, 0x60596059 },
	{ 0xe60, 0x06161050 }, { 0xe64, 0x61616161 }, { 0xe68, 0x03036161 },
	{ 0xe6c, 0x03030303 }, { 0xe70, 0x04040303 }, { 0xe74, 0x04040404 },
	{ 0xe78, 0xf8f80404 }, { 0xe7c, 0xf8f8f8f8 }, { 0xe80, 0x1c1cf8f8 },
	{ 0xe84, 0x1c1c1c1c }, { 0xe88, 0xb5b51c1c }, { 0xe8c, 0xb5b5b5b5 },
	{ 0xe90, 0x0000b5b5 }, { 0xe94, 0x00000000 }, { 0xe98, 0x00000000 },
	{ 0xe9c, 0x00000000 }, { 0xea0, 0x00000000 }, { 0xea4, 0x00000000 },
	{ 0xea8, 0x50500000 }, { 0xeac, 0x50505050 }, { 0xeb0, 0x18185050 },
	{ 0xeb4, 0x18181818 }, { 0xeb8, 0x00001818 }, { 0xebc, 0x00000000 },
	{ 0xec0, 0x5e5e0000 }, { 0xec4, 0x5e5e5e5e }, { 0xec8, 0x57575e5e },
	{ 0xecc, 0x57575757 }, { 0xed0, 0x74745757 }, { 0xed4, 0x74747474 },
	{ 0xed8, 0xfdfd7474 }, { 0xedc, 0xfdfdfdfd }, { 0xee0, 0xfffffdfd },
	{ 0xee4, 0xffffffff }, { 0xee8, 0xe4e4ffff }, { 0xeec, 0xe4e4e4e4 },
	{ 0xef0, 0xb3b3e4e4 }, { 0xef4, 0xb3b3b3b3 }, { 0xef8, 0x0d0db3b3 },
	{ 0xefc, 0x0d0d0d0d }, { 0xf00, 0x0c300d0d }, { 0xf14, 0xf0ff05eb },
	{ 0xf10, 0xfc02abc3 },
};

static void rtk_write_reg_table(void __iomem *base, const struct rtk_pwr_reg_entry *table, int size)
{
	int i;
	for (i = 0; i < size; i++)
		writel(table[i].val, base + table[i].offset);
}

static void u3dprx_pwr_cut(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	void __iomem *pwr_base = type_c->pwr_base;

	rtk_write_reg_table(base, type_c_phy_stage1, ARRAY_SIZE(type_c_phy_stage1));

	writel(0x00000000, pwr_base + 0x36c);

	rtk_write_reg_table(base, type_c_phy_stage2, ARRAY_SIZE(type_c_phy_stage2));

	writel(0x00008000, pwr_base + 0x36c);
}

static int rtk_u3dprx_phy_probe(struct platform_device *pdev)
{
	struct rtk_phy *rtk_phy;
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct resource *res;
	struct type_c_data *type_c;
	int ret;

	rtk_phy = devm_kzalloc(dev, sizeof(*rtk_phy), GFP_KERNEL);
	if (!rtk_phy)
		return -ENOMEM;

	rtk_phy->dev			= &pdev->dev;
	rtk_phy->dp_altmode		= NULL;
	rtk_phy->dp_hpd_high		= false;

	type_c = &rtk_phy->type_c;
	rtk_phy->wq_typec_phy = create_singlethread_workqueue("rtk_u3dprx_phy");

	rtk_phy->sb2_regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
				"realtek,sb2-syscon");
	if (IS_ERR(rtk_phy->sb2_regmap)) {
		dev_err(&pdev->dev, "failed to get sb2 regmap,(ret = %ld)\n",
				PTR_ERR(rtk_phy->sb2_regmap));
		return PTR_ERR(rtk_phy->sb2_regmap);
	} else {
		regmap_write(rtk_phy->sb2_regmap, 0xfd0, 0xff);
	}

	rtk_phy->misc_regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
				"realtek,misc-syscon");
	if (IS_ERR(rtk_phy->misc_regmap)) {
		dev_err(&pdev->dev, "failed to get misc regmap,(ret = %ld)\n",
				PTR_ERR(rtk_phy->misc_regmap));
		return PTR_ERR(rtk_phy->misc_regmap);
	} else {
		regmap_write(rtk_phy->misc_regmap, 0x518, BIT(31));
	}

	INIT_DELAYED_WORK(&rtk_phy->delayed_work, rtk_u3dprx_sub_probe_work);
	rtk_u3dprx_sub_probe_work(&rtk_phy->delayed_work.work);

	type_c->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(type_c->base))
		return PTR_ERR(type_c->base);

	type_c->pwr_base = devm_platform_get_and_ioremap_resource(pdev, 1, &res);
	if (IS_ERR(type_c->pwr_base))
		return PTR_ERR(type_c->pwr_base);

	type_c->iso_base = devm_platform_get_and_ioremap_resource(pdev, 2, &res);
	if (IS_ERR(type_c->iso_base))
		return PTR_ERR(type_c->iso_base);

	mutex_init(&rtk_phy->mutex);
	platform_set_drvdata(pdev, rtk_phy);

	rtk_phy->phy_u3 = devm_phy_create(rtk_phy->dev, NULL, &usb3_ops);
	if (IS_ERR(rtk_phy->phy_u3)) {
		ret = PTR_ERR(rtk_phy->phy_u3);
		return dev_err_probe(dev, ret, "failed to create USB3 phy\n");
	}
	phy_set_drvdata(rtk_phy->phy_u3, rtk_phy);

	rtk_phy->phy_dp = devm_phy_create(rtk_phy->dev, NULL, &dp_phy_ops);
	if (IS_ERR(rtk_phy->phy_dp)) {
		ret = PTR_ERR(rtk_phy->phy_dp);
		return dev_err_probe(dev, ret, "failed to create DP phy\n");
	}
	phy_set_drvdata(rtk_phy->phy_dp, rtk_phy);

	phy_provider = devm_of_phy_provider_register(rtk_phy->dev, rtk_udphy_phy_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	u3dprx_pwr_cut(type_c);

	ret = sysfs_create_group(&pdev->dev.kobj, &rtk_dprx_phy_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create sysfs group\n");
		return ret;
	}

	type_c->pre_lane = -1;
	type_c->start_addr = 0x0;
	type_c->ss = false;

	return ret;
}

static void rtk_u3dprx_phy_remove(struct platform_device *pdev)
{
	struct rtk_phy *rtk_phy = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &rtk_dprx_phy_attr_group);
	if (rtk_phy->dp_hpd_high)
		rtk_usb3dprx_send_hpd(rtk_phy, false, false);

	/* Free register backup memory */
	if (rtk_phy->saved_reg)
		kfree(rtk_phy->saved_reg);

	return;
}

#ifdef CONFIG_PM
static int rtk_u3dprx_phy_suspend(struct device *dev)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c;

	dev_info(dev, "%s enter u3dprx suspend\n", __func__);
	type_c = &rtk_phy->type_c;

	return 0;
}

static int rtk_u3dprx_phy_resume(struct device *dev)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c;

	dev_info(dev, "%s enter u3dprx resume\n", __func__);
	type_c = &rtk_phy->type_c;

	return 0;
}
#else
static int rtk_u3dprx_phy_suspend(struct device *dev)
{
	dev_err(dev,"User should enable CONFIG_PM kernel config\n");

	return 0;
}
static int rtk_u3dprx_phy_resume(struct device *dev)
{
        dev_err(dev, "User should enable CONFIG_PM kernel config\n");

        return 0;
}
#endif /*CONFIG_PM*/

static const struct dev_pm_ops rtk_u3dprx_phy_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rtk_u3dprx_phy_suspend, rtk_u3dprx_phy_resume)
};

static const struct of_device_id usbphy_rtk_dt_match[] = {
	{ .compatible = "realtek,usb3dprx-phy"},
	{},
};
MODULE_DEVICE_TABLE(of, usbphy_rtk_dt_match);

static struct platform_driver rtk_usb3dprx_phy_driver = {
	.probe		= rtk_u3dprx_phy_probe,
	.remove		= rtk_u3dprx_phy_remove,
	.driver		= {
		.name	= "rtk-usb3-dp-rx-phy",
		.pm	= &rtk_u3dprx_phy_pm_ops,
		.of_match_table = usbphy_rtk_dt_match,
	},
};

module_platform_driver(rtk_usb3dprx_phy_driver);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: rtk-usb3-dp-rx-phy");
MODULE_AUTHOR("Jyan Chou <jyanchou@realtek.com>");
MODULE_DESCRIPTION("Realtek usb 3.0 dprx phy driver");
