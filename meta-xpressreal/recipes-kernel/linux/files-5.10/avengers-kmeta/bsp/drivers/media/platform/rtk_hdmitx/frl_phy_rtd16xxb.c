/*
 * frl_phy_rtd16xxb.c - HDMI2.1 FRL PHY setting
 *
 * Copyright (C) 2019 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include "hdmitx.h"
#include "hdmitx_reg.h"
#include "crt_reg.h"

#define FRL_PLL_DELAY 20

struct PLL_HDMI_SD_T {
	unsigned int sd1;
	unsigned int sd2;
	unsigned int sd4;
	unsigned int sd5;
};

struct PLL_HDMI_LDO_T {
	unsigned int ldo1;
	unsigned int ldo2;
	unsigned int ldo3;
	unsigned int ldo4;
	unsigned int ldo5;
	unsigned int pll_hdmi2;
	unsigned int ldo6;
	unsigned int ldo7;
	unsigned int ldo8;
	unsigned int ldo9;
	unsigned int ldo10;
	unsigned int ldo11;
};

static const struct PLL_HDMI_SD_T frl_sd[] = {
	/* 27MHz */
	{.sd1 = 0x9368000d, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 27MHz*1.25 */
	{.sd1 = 0x93680025, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 27MHz*1.5 */
	{.sd1 = 0x93680015, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 54MHz */
	{.sd1 = 0x9368000d, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 54MHz*1.25 */
	{.sd1 = 0x93680025, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 54MHz*1.5 */
	{.sd1 = 0x93680015, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 59.4MHz, Unsupported */
	{.sd1 = 0x0, .sd2 = 0x0, .sd4 = 0x0, .sd5 = 0x0},
	/* 59.4MHz*1.25, Unsupported */
	{.sd1 = 0x0, .sd2 = 0x0, .sd4 = 0x0, .sd5 = 0x0},
	/* 59.4MHz*1.5, Unsupported */
	{.sd1 = 0x0, .sd2 = 0x0, .sd4 = 0x0, .sd5 = 0x0},
	/* 74.25MHz */
	{.sd1 = 0x93680013, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 74.25MHz*1.25 */
	{.sd1 = 0x93680034, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 74.25MHz*1.5 */
	{.sd1 = 0x9368001e, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 148.5MHz */
	{.sd1 = 0x93680013, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 148.5MHz*1.25 */
	{.sd1 = 0x93680034, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 148.5MHz*1.5 */
	{.sd1 = 0x9368001e, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 297MHz */
	{.sd1 = 0x93680013, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 297MHz*1.25  */
	{.sd1 = 0x93680034, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 297MHz*1.5 */
	{.sd1 = 0x9368001e, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 594MHz_420 */
	{.sd1 = 0x93680013, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 594MHz_420*1.25 */
	{.sd1 = 0x93680034, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 594MHz_420*1.5 */
	{.sd1 = 0x9368001E, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
	/* 594MHz */
	{.sd1 = 0x93680013, .sd2 = 0xd0201ffe, .sd4 = 0x00000000, .sd5 = 0x00800000},
};

static const struct PLL_HDMI_LDO_T frl_ldo[] = {
	/* 27MHz */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00419100,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 27MHz*1.25 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00439400,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 27MHz*1.5 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00419880,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 54MHz */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00501180,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 54MHz*1.25 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00521480,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 54MHz*1.5 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00501900,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 59.4MHz, Unsupported */
	{.ldo1 = 0x0, .ldo2 = 0x0, .ldo3 = 0x0,
	 .ldo4 = 0x0, .ldo5 = 0x0,
	 .pll_hdmi2 = 0x0,
	 .ldo6 = 0x0, .ldo7 = 0x0, .ldo8 = 0x0,
	 .ldo9 = 0x0},
	/* 59.4MHz*1.25, Unsupported */
	{.ldo1 = 0x0, .ldo2 = 0x0, .ldo3 = 0x0,
	 .ldo4 = 0x0, .ldo5 = 0x0,
	 .pll_hdmi2 = 0x0,
	 .ldo6 = 0x0, .ldo7 = 0x0, .ldo8 = 0x0,
	 .ldo9 = 0x0},
	/* 59.4MHz*1.5, Unsupported */
	{.ldo1 = 0x0, .ldo2 = 0x0, .ldo3 = 0x0,
	 .ldo4 = 0x0, .ldo5 = 0x0,
	 .pll_hdmi2 = 0x0,
	 .ldo6 = 0x0, .ldo7 = 0x0, .ldo8 = 0x0,
	 .ldo9 = 0x0},
	/* 74.25MHz */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00501180,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 74.25MHz*1.25 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x005214C0,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 74.25MHz*1.5 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00421880,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 148.5MHz */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00401100,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 148.5MHz*1.25 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00421400,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 148.5MHz*1.5 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00221800,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 297MHz */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00201080,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 297MHz*1.25 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00227000,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 297MHz*1.5 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00100800,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 594MHz_420 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00201000,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 594MHz_420*1.25 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00223000,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 594MHz_420*1.5 */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00204000,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
	/* 594MHz */
	{.ldo1 = 0x221E2088, .ldo2 = 0x377454F0, .ldo3 = 0x6739CE00,
	 .ldo4 = 0x00310842, .ldo5 = 0x00050706,
	 .pll_hdmi2 = 0x00001008,
	 .ldo6 = 0x34540001, .ldo7 = 0x04222030, .ldo8 = 0x2AAAA924,
	 .ldo9 = 0x0D050000},
};

void enable_pll_power(struct device *dev)
{
	hdmipll_write32(dev, SYS_PLL_HDMI,
		SYS_PLL_HDMI_PLLDISP_OEB(0) |
		SYS_PLL_HDMI_PLLDISP_VCORSTB(1) |
		SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN(1) |
		SYS_PLL_HDMI_REG_PLLDISP_RSTB(0) |
		SYS_PLL_HDMI_REG_PLLDISP_POW(1) |
		SYS_PLL_HDMI_REG_TMDS_POW(0) |
		SYS_PLL_HDMI_REG_PLL_RSTB(0) |
		SYS_PLL_HDMI_REG_PLL_POW(1) |
		SYS_PLL_HDMI_REG_HDMI_CK_EN(1));

	usleep_range(FRL_PLL_DELAY, FRL_PLL_DELAY+1);
}

void active_pll_control(struct device *dev)
{
	hdmipll_write32(dev, SYS_PLL_HDMI,
			SYS_PLL_HDMI_PLLDISP_OEB(0) |
			SYS_PLL_HDMI_PLLDISP_VCORSTB(1) |
			SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN(1) |
			SYS_PLL_HDMI_REG_PLLDISP_RSTB(1) |
			SYS_PLL_HDMI_REG_PLLDISP_POW(1) |
			SYS_PLL_HDMI_REG_TMDS_POW(0) |
			SYS_PLL_HDMI_REG_PLL_RSTB(0) |
			SYS_PLL_HDMI_REG_PLL_POW(1) |
			SYS_PLL_HDMI_REG_HDMI_CK_EN(1));

	usleep_range(FRL_PLL_DELAY, FRL_PLL_DELAY+1);

	hdmipll_write32(dev, SYS_PLL_HDMI,
			SYS_PLL_HDMI_PLLDISP_OEB(0) |
			SYS_PLL_HDMI_PLLDISP_VCORSTB(1) |
			SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN(1) |
			SYS_PLL_HDMI_REG_PLLDISP_RSTB(1) |
			SYS_PLL_HDMI_REG_PLLDISP_POW(1) |
			SYS_PLL_HDMI_REG_TMDS_POW(1) |
			SYS_PLL_HDMI_REG_PLL_RSTB(0) |
			SYS_PLL_HDMI_REG_PLL_POW(1) |
			SYS_PLL_HDMI_REG_HDMI_CK_EN(1));

	usleep_range(FRL_PLL_DELAY, FRL_PLL_DELAY+1);

	hdmipll_write32(dev, SYS_PLL_HDMI,
			SYS_PLL_HDMI_PLLDISP_OEB(0) |
			SYS_PLL_HDMI_PLLDISP_VCORSTB(1) |
			SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN(1) |
			SYS_PLL_HDMI_REG_PLLDISP_RSTB(1) |
			SYS_PLL_HDMI_REG_PLLDISP_POW(1) |
			SYS_PLL_HDMI_REG_TMDS_POW(1) |
			SYS_PLL_HDMI_REG_PLL_RSTB(1) |
			SYS_PLL_HDMI_REG_PLL_POW(1) |
			SYS_PLL_HDMI_REG_HDMI_CK_EN(1));

	usleep_range(FRL_PLL_DELAY, FRL_PLL_DELAY+1);
}

void reset_mac_frl_pll(struct device *dev)
{
	hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD5, 0x0);
}

/**
 * set_frl_pll - CRT & PHY config
 * @mode: enum PLL_MODE
 * @freq_shift: freq shift
 * @lane: enum FRL_LANE_MODE
 */
static void set_frl_pll(struct device *dev,
	unsigned char mode, unsigned char freq_shift, unsigned char lane)
{
	unsigned int sd1;
	unsigned int sd2;
	unsigned int reg_ldo2;

	sd1 = frl_sd[mode].sd1;
	sd2 = frl_sd[mode].sd2;
	if (freq_shift) {
		unsigned int fcode;
		unsigned int ncode;

		switch (mode) {
		case PLL_74p25MHz:
		case PLL_148p5MHz:
		case PLL_297MHz:
		case PLL_594MHz_420:
		case PLL_594MHz:
			fcode = 2003;
			ncode = 18;
			break;
		case PLL_74p25x1p25:
		case PLL_148p5x1p25:
		case PLL_297x1p25:
		case PLL_594MHz_420x1p25:
			fcode = 1936;
			ncode = 51;
			break;
		case PLL_74p25x1p5:
		case PLL_148p5x1p5:
		case PLL_297x1p5:
		case PLL_594MHz_420x1p5:
			fcode = 1980;
			ncode = 29;
			break;
		default:
			fcode = SYS_PLL_HDMI_SD1_get_fcode(sd1);
			ncode = SYS_PLL_HDMI_SD1_get_ncode(sd1);
			dev_err(dev, "%s Unknown mode=%u", __func__, mode);
			break;
		}

		sd1 = sd1 & ~(SYS_PLL_HDMI_SD1_fcode_mask | SYS_PLL_HDMI_SD1_ncode_mask);
		sd1 = sd1 | SYS_PLL_HDMI_SD1_fcode(fcode) | SYS_PLL_HDMI_SD1_ncode(ncode);

		sd2 = sd2 & ~SYS_PLL_HDMI_SD2_bypass_pi_mask;
		sd2 = sd2 | SYS_PLL_HDMI_SD2_bypass_pi(0);
	}

	hdmipll_write32(dev, SYS_PLL_HDMI_SD1, sd1);
	hdmipll_write32(dev, SYS_PLL_HDMI_SD2, sd2);
	hdmipll_write32(dev, SYS_PLL_HDMI_SD4, frl_sd[mode].sd4);
	hdmipll_write32(dev, SYS_PLL_HDMI_SD5, frl_sd[mode].sd5);

	/* Enable PLL OC */
	hdmipll_mask32(dev, SYS_PLL_HDMI_SD2,
		~SYS_PLL_HDMI_SD2_oc_en_mask, SYS_PLL_HDMI_SD2_oc_en(1));

	usleep_range(FRL_PLL_DELAY, FRL_PLL_DELAY+1);

	/* Disable CK output in 3 lane mode */
	reg_ldo2 = frl_ldo[mode].ldo2 & (~SYS_PLL_HDMI_LDO2_REG_TMDS_POWCK_mask);
	if (lane == FRL_4LANE)
		reg_ldo2 |= SYS_PLL_HDMI_LDO2_REG_TMDS_POWCK_mask;

	hdmipll_write32(dev, SYS_PLL_HDMI_LDO1, frl_ldo[mode].ldo1);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO2, reg_ldo2);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO3, frl_ldo[mode].ldo3);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO4, frl_ldo[mode].ldo4);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO5, frl_ldo[mode].ldo5);
	hdmipll_write32(dev, SYS_PLL_HDMI2, frl_ldo[mode].pll_hdmi2);
	usleep_range(FRL_PLL_DELAY, FRL_PLL_DELAY+1);

	hdmipll_write32(dev, SYS_PLL_HDMI_LDO6, frl_ldo[mode].ldo6);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO7, frl_ldo[mode].ldo7);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO8, frl_ldo[mode].ldo8);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO10, 0x00924924);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO11, 0x0);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO9, frl_ldo[mode].ldo9);
	usleep_range(FRL_PLL_DELAY, FRL_PLL_DELAY+1);
}

void set_frl_mode(struct device *dev, unsigned char mode)
{
	switch (mode) {
	case FRL_3G_MODE:
		/* 3G mode Low gain */
		hdmipll_mask32(dev, SYS_PLL_HDMI_LDO1,
			~SYS_PLL_HDMI_LDO1_REG_PLL_VCOGAIN_mask,
			SYS_PLL_HDMI_LDO1_REG_PLL_VCOGAIN(0));

		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD1, 0x9368E46C);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD2, 0x50207FFE);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD5, 0x00800000);
		/* Enable PLL OC */
		hdmipll_mask32(dev, SYS_PLL_HDMITX2P1_SD2,
			~SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
			SYS_PLL_HDMITX2P1_SD2_oc_en(1));

		active_pll_control(dev);
		break;
	case FRL_6G_MODE:
		/* Low gain */
		hdmipll_mask32(dev, SYS_PLL_HDMI_LDO1,
			~SYS_PLL_HDMI_LDO1_REG_PLL_VCOGAIN_mask,
			SYS_PLL_HDMI_LDO1_REG_PLL_VCOGAIN(0));

		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD1, 0x9368E46C);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD2, 0x50207FFE);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD5, 0x00800000);
		/* Enable PLL OC */
		hdmipll_mask32(dev, SYS_PLL_HDMITX2P1_SD2,
			~SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
			SYS_PLL_HDMITX2P1_SD2_oc_en(1));

		active_pll_control(dev);

		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD1, 0x9369C7DB);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD2, 0x50207FFE);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD5, 0x00800000);
		/* Enable PLL OC */
		hdmipll_mask32(dev, SYS_PLL_HDMITX2P1_SD2,
			~SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
			SYS_PLL_HDMITX2P1_SD2_oc_en(1));

		/* High gain */
		hdmipll_mask32(dev, SYS_PLL_HDMI_LDO1,
			~SYS_PLL_HDMI_LDO1_REG_PLL_VCOGAIN_mask,
			SYS_PLL_HDMI_LDO1_REG_PLL_VCOGAIN(1));
		break;
	default:
		dev_err(dev, "%s Unknown frl mode=%u", __func__, mode);
		break;
	}

	usleep_range(FRL_PLL_DELAY, FRL_PLL_DELAY+1);
}

static void set_pi_current(struct device *dev, unsigned char cur_sel)
{
	hdmipll_mask32(dev, SYS_PLL_HDMITX2P1_SD2,
		~SYS_PLL_HDMITX2P1_SD2_pi_cur_sel_mask,
		SYS_PLL_HDMITX2P1_SD2_pi_cur_sel(cur_sel));
}

/*
 * frl_set_phy - PHY setting when FRL mode is actived
 */
int frl_set_phy(struct device *dev,
	unsigned char link_rate, struct hdmi_format_setting *hdmi_format)
{
	unsigned char frl_mode;
	unsigned char lane;
	unsigned char freq_shift;
	int pll_mode;

	freq_shift = hdmi_format->freq_shift;

	switch (link_rate) {
	case FRL_3G3LANES:
		frl_mode = FRL_3G_MODE;
		lane = FRL_3LANE;
		break;
	case FRL_6G3LANES:
	case FRL_6G4LANES:
		frl_mode = FRL_6G_MODE;
		lane = FRL_4LANE;
		break;
	default:
		dev_err(dev, "%s Unknown FRL_LinkRate=%u", __func__, link_rate);
		return -EINVAL;
	}

	pll_mode = get_pll_mode(dev, hdmi_format);
	if (pll_mode < 0)
		return pll_mode;

	reset_mac_frl_pll(dev);

	enable_pll_power(dev);

	set_frl_pll(dev, (unsigned char) pll_mode, freq_shift, lane);

	set_frl_mode(dev, frl_mode);

	set_pi_current(dev, 3);

	return 0;
}
