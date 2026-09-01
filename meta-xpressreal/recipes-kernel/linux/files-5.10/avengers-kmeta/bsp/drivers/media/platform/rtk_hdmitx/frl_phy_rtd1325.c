// SPDX-License-Identifier: GPL-2.0

#include <linux/delay.h>
#include <linux/regmap.h>
#include "hdmitx_dev.h"
#include "hdmitx_reg.h"
#include "hdmi_new_reg.h"
#include "crt_reg.h"

#define FRL_PLL_DELAY_US  20
#define PLL_RSTB_DELAY_US 40
#define HW_OP_TIMEOUT_MS  3

static void reset_mac_frl_pll(struct device *dev)
{
	hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD5, 0x0);
}

void active_pll_control(struct device *dev)
{
	hdmipll_mask32(dev, SYS_PLL_HDMI,
		~SYS_PLL_HDMI_REG_PLL_RSTB_mask,
		SYS_PLL_HDMI_REG_PLL_RSTB(1));

	usleep_range(PLL_RSTB_DELAY_US, PLL_RSTB_DELAY_US+1);
}

static void enable_pll_power(struct device *dev, unsigned char frl_mode)
{
	unsigned char is_high_gain;
	unsigned char ps2_div2;
	unsigned char ps2_ckin_sel;
	unsigned char plldisp_pow;

	is_high_gain = (frl_mode == FRL_3G_MODE) ? 0:1;
	ps2_ckin_sel = 0;
	ps2_div2 = 1;
	plldisp_pow = 0;

	hdmipll_write32(dev, SYS_PLL_HDMI,
		SYS_PLL_HDMI_REG_P2S_CTRL(0x9) |
		SYS_PLL_HDMI_REG_PLLDISP_EXT_LDO_LV(1) |
		SYS_PLL_HDMI_REG_PLL_CAPSEL_VTOI(0) |
		SYS_PLL_HDMI_REG_PLL_SEL_DUAL_R(0x1)|
		SYS_PLL_HDMI_REG_PLL_KVCO_RES(is_high_gain) |
		SYS_PLL_HDMI_REG_PLL_VSET_SEL(0) |
		SYS_PLL_HDMI_REG_PLL_LDO_POW(1) |
		SYS_PLL_HDMI_REG_PLL_BPS_M3(0) |
		SYS_PLL_HDMI_REG_P2S_CKIN_SEL(ps2_ckin_sel) |
		SYS_PLL_HDMI_REG_P2S_DIV2(ps2_div2) |
		SYS_PLL_HDMI_PLLDISP_OEB(0) |
		SYS_PLL_HDMI_PLLDISP_VCORSTB(1) |
		SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN(1) |
		SYS_PLL_HDMI_REG_PLLDISP_RSTB(0) |
		SYS_PLL_HDMI_REG_PLLDISP_POW(plldisp_pow) |
		SYS_PLL_HDMI_REG_TMDS_POW(1) |
		SYS_PLL_HDMI_REG_PLL_RSTB(0) |
		SYS_PLL_HDMI_REG_PLL_POW(1) |
		SYS_PLL_HDMI_REG_HDMI_CK_EN(1));

	usleep_range(FRL_PLL_DELAY_US, FRL_PLL_DELAY_US+1);
}

void enable_dual_loop(struct device *dev)
{
	hdmipll_mask32(dev, SYS_PLL_HDMI_LDO6,
		~(SYS_PLL_HDMI_LDO6_REG_PLL_KVCO_mask),
		SYS_PLL_HDMI_LDO6_REG_PLL_KVCO(1));
}

static void set_module_version(struct device *dev, unsigned char ver)
{
	hdmitx_device_t *tx_dev = dev_get_drvdata(dev);
	unsigned int reg_val;

	regmap_read(tx_dev->top_base, VER_CTRL, &reg_val);
	reg_val &= ~VER_CTRL_block_ver_ctl_mask;
	reg_val |= VER_CTRL_block_ver_ctl(ver);
	regmap_write(tx_dev->top_base, VER_CTRL, reg_val);
}

static void set_frl_pll(struct device *dev, unsigned char pll_mode,
	unsigned char frl_mode, unsigned char lane)
{
	unsigned int ldo1;
	unsigned int ldo2;
	unsigned int ldo4;
	unsigned int pll_hdmi2;

	if (frl_mode == FRL_3G_MODE) {
		ldo1 = 0x221C1888;
		ldo4 = 0x00210842;
		pll_hdmi2 = 0;
	} else {
		ldo1 = 0x221C3088;
		ldo4 = 0x00310842;
		pll_hdmi2 = 0x10000000;
	}

	/* Disable CK output in 3 lane mode */
	ldo2 = 0x2EEC00F0 & (~SYS_PLL_HDMI_LDO2_REG_TMDS_POWCK_mask);
	if (lane == FRL_4LANE)
		ldo2 |= SYS_PLL_HDMI_LDO2_REG_TMDS_POWCK(1);

	hdmipll_write32(dev, SYS_PLL_HDMI_LDO1, ldo1);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO2, ldo2);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO3, 0x6739CE00);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO4, ldo4);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO5, 0x00000000);
	hdmipll_write32(dev, SYS_PLL_HDMI2, pll_hdmi2);

	usleep_range(FRL_PLL_DELAY_US, FRL_PLL_DELAY_US+1);

	hdmipll_write32(dev, SYS_PLL_HDMI_LDO6, 0x14540001);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO7, 0x04222000);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO8, 0x0AAAACB2);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO10, 0x00CB2CB2);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO11, 0x0);
	hdmipll_write32(dev, SYS_PLL_HDMI_LDO9, 0x0B000080);

	usleep_range(FRL_PLL_DELAY_US, FRL_PLL_DELAY_US+1);
}

static void set_frl_mode(struct device *dev, unsigned char mode)
{
	switch (mode) {
	case FRL_3G_MODE:
		/* 98000190[16:15] = 0, Low gain*/
		hdmipll_mask32(dev, SYS_PLL_HDMI,
			~(SYS_PLL_HDMI_REG_PLL_KVCO_RES_mask |
			SYS_PLL_HDMI_REG_P2S_CTRL_mask),
			SYS_PLL_HDMI_REG_P2S_CTRL(0x9) |
			SYS_PLL_HDMI_REG_PLL_KVCO_RES(0));

		/* 98000230[13:11]=0x3, scan from RDC_Danvers */
		hdmipll_mask32(dev, SYS_PLL_HDMI_LDO1,
			~(SYS_PLL_HDMI_LDO1_REG_PLL_RS_mask),
			SYS_PLL_HDMI_LDO1_REG_PLL_RS(0x3));

		/* 9800023C [25:20]=0x2 , scan from RDC_Danvers*/
		hdmipll_mask32(dev, SYS_PLL_HDMI_LDO4,
			~(SYS_PLL_HDMI_LDO4_REG_PLL_IP_mask),
			SYS_PLL_HDMI_LDO4_REG_PLL_IP(0x2));

		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD1, 0x9368E46C);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD2, 0x50203FFE);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD5, 0x00800000);
		break;
	case FRL_6G_MODE:
		/* 98000190[16:15] = 1, High gain */
		hdmipll_mask32(dev, SYS_PLL_HDMI,
			~(SYS_PLL_HDMI_REG_PLL_KVCO_RES_mask |
			SYS_PLL_HDMI_REG_P2S_CTRL_mask),
			SYS_PLL_HDMI_REG_P2S_CTRL(0x9) |
			SYS_PLL_HDMI_REG_PLL_KVCO_RES(1));

		/* 98000230[13:11]=0x6, scan from RDC_Danvers */
		hdmipll_mask32(dev, SYS_PLL_HDMI_LDO1,
			~(SYS_PLL_HDMI_LDO1_REG_PLL_RS_mask),
			SYS_PLL_HDMI_LDO1_REG_PLL_RS(0x6));

		/* 9800023C [25:20]=0x3 , scan from RDC_Danvers*/
		hdmipll_mask32(dev, SYS_PLL_HDMI_LDO4,
			~(SYS_PLL_HDMI_LDO4_REG_PLL_IP_mask),
			SYS_PLL_HDMI_LDO4_REG_PLL_IP(0x3));

		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD1, 0x9369C7DB);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD2, 0x50203FFE);
		hdmipll_write32(dev, SYS_PLL_HDMITX2P1_SD5, 0x00800000);
		break;
	default:
		dev_err(dev, "%s: Unknown frl mode %u\n", __func__, mode);
		break;
	}

	/* Enable PLL OC */
	hdmipll_mask32(dev, SYS_PLL_HDMITX2P1_SD2,
		~SYS_PLL_HDMITX2P1_SD2_oc_en_mask,
		SYS_PLL_HDMITX2P1_SD2_oc_en(1));

	active_pll_control(dev);

	usleep_range(FRL_PLL_DELAY_US, FRL_PLL_DELAY_US+1);
}

static unsigned char wait_hw_op_finished(struct device *dev,
		unsigned int reg_addr, unsigned int mask, unsigned char condition)
{
	unsigned int reg_val;
	unsigned long start_time;
	unsigned char bit_val;
	unsigned char is_finished;

	start_time = jiffies;
	is_finished = 0;

	/* OPEN_LOOP_PLL_STAT, max 2ms when CK_in=3G */
	while ((is_finished == 0) &&
		(jiffies_to_msecs(jiffies - start_time) < HW_OP_TIMEOUT_MS)) {
		reg_val = hdmipll_read32(dev, reg_addr);
		bit_val = (reg_val & mask) ? 1:0;
		is_finished = (bit_val == condition) ? 1:0;

		usleep_range(FRL_PLL_DELAY_US, FRL_PLL_DELAY_US*2);
	}

	return is_finished;
}

static unsigned int get_ol_pll_1(unsigned char pll_mode,
		struct hdmi_format_setting *hdmi_format)
{
	unsigned int ol_pll_1;

	switch (pll_mode) {
	case PLL_27MHz:
		ol_pll_1 = 0x00001135;
		break;
	case PLL_27x1p25:
		ol_pll_1 = 0x00000C26;
		break;
	case PLL_27x1p5:
		ol_pll_1 = 0x00001AB3;
		break;
	case PLL_54MHz:
		ol_pll_1 = 0x00001135;
		break;
	case PLL_54x1p25:
		ol_pll_1 = 0x00000C26;
		break;
	case PLL_54x1p5:
		ol_pll_1 = 0x00000857;
		break;
	case PLL_59p4MHz:
		ol_pll_1 = 0x00000ED8;
		break;
	case PLL_59p4x1p25:
		ol_pll_1 = 0x00000A23;
		break;
	case PLL_59p4x1p5:
		ol_pll_1 = 0x00000690;
		break;
	case PLL_74p25MHz:
		ol_pll_1 = 0x1E521E46;
		break;
	case PLL_74p25x1p25:
		ol_pll_1 = 0x16441635;
		break;
	case PLL_74p25x1p5:
		ol_pll_1 = 0x10AC10A4;
		break;
	case PLL_148p5MHz:
		ol_pll_1 = 0x1E521E46;
		break;
	case PLL_148p5x1p25:
		ol_pll_1 = 0x16441635;
		break;
	case PLL_148p5x1p5:
		ol_pll_1 = 0x10AC10A4;
		break;
	case PLL_297MHz:
		ol_pll_1 = 0x0A290A23;
		break;
	case PLL_297x1p25:
		ol_pll_1 = 0x06220618;
		break;
	case PLL_297x1p5:
		ol_pll_1 = 0x03060302;
		break;
	case PLL_594MHz_420:
		ol_pll_1 = 0x0A290A23;
		break;
	case PLL_594MHz_420x1p25:
		ol_pll_1 = 0x06220618;
		break;
	case PLL_594MHz_420x1p5:
		ol_pll_1 = 0x03060302;
		break;
	case PLL_594MHz:
		ol_pll_1 = 0x0012000F;
		break;
	default:
		ol_pll_1 = 0;
	}

	return ol_pll_1;
}

static int set_open_loop(struct device *dev,
	unsigned char frl_mode, unsigned char pll_mode,
	struct hdmi_format_setting *hdmi_format)
{
	unsigned int ol_pll_1;
	unsigned int ol_div;
	unsigned char is_finished;

	ol_pll_1 = get_ol_pll_1(pll_mode, hdmi_format);

	hdmipll_mask32(dev, SYS_REG_OPEN_LOOP_PLL_2,
		~SYS_REG_OPEN_LOOP_PLL_2_PLL_SSC_EN_mask,
		SYS_REG_OPEN_LOOP_PLL_2_PLL_SSC_EN(0));

	is_finished = wait_hw_op_finished(dev, SYS_REG_OPEN_LOOP_PLL_STAT,
			SYS_REG_OPEN_LOOP_PLL_STAT_PLL_SSC_UNDERGO_mask, 0);
	if (is_finished)
		dev_info(dev, "%s: PLL_SSC_UNDERGO done", __func__);

	hdmipll_mask32(dev, SYS_REG_OPEN_LOOP_PLL_0,
		~(SYS_REG_OPEN_LOOP_PLL_0_REG_HDMI_CLK_TMDS_QUATER_SEL_mask |
		SYS_REG_OPEN_LOOP_PLL_0_REG_HDMI_OPEN_LOOP_SEL_mask |
		SYS_REG_OPEN_LOOP_PLL_0_PLL_POW_mask |
		SYS_REG_OPEN_LOOP_PLL_0_PLL_OEB_mask |
		SYS_REG_OPEN_LOOP_PLL_0_PLL_RSTB_mask),
		SYS_REG_OPEN_LOOP_PLL_0_REG_HDMI_CLK_TMDS_QUATER_SEL(1) |
		SYS_REG_OPEN_LOOP_PLL_0_REG_HDMI_OPEN_LOOP_SEL(1) |
		SYS_REG_OPEN_LOOP_PLL_0_PLL_POW(1) |
		SYS_REG_OPEN_LOOP_PLL_0_PLL_OEB(1) |
		SYS_REG_OPEN_LOOP_PLL_0_PLL_RSTB(0));

	hdmipll_mask32(dev, SYS_REG_OPEN_LOOP_PLL_0,
		~SYS_REG_OPEN_LOOP_PLL_0_PLL_RSTB_mask,
		SYS_REG_OPEN_LOOP_PLL_0_PLL_RSTB(1));

	hdmipll_write32(dev, SYS_REG_OPEN_LOOP_PLL_1, ol_pll_1);

	hdmipll_mask32(dev, SYS_REG_OPEN_LOOP_PLL_2,
		~(SYS_REG_OPEN_LOOP_PLL_2_PLL_SSC_STEP_mask |
		SYS_REG_OPEN_LOOP_PLL_2_PLL_OCUC_STEP_mask |
		SYS_REG_OPEN_LOOP_PLL_2_PLL_OCUC_EN_mask),
		SYS_REG_OPEN_LOOP_PLL_2_PLL_SSC_STEP(4) |
		SYS_REG_OPEN_LOOP_PLL_2_PLL_OCUC_STEP(10) |
		SYS_REG_OPEN_LOOP_PLL_2_PLL_OCUC_EN(0));

	hdmipll_mask32(dev, SYS_REG_OPEN_LOOP_PLL_2,
		~SYS_REG_OPEN_LOOP_PLL_2_PLL_OCUC_EN_mask,
		SYS_REG_OPEN_LOOP_PLL_2_PLL_OCUC_EN(1));


	is_finished = wait_hw_op_finished(dev, SYS_REG_OPEN_LOOP_PLL_STAT,
		SYS_REG_OPEN_LOOP_PLL_STAT_PLL_OCUC_DONE_mask, 1);
	if (is_finished)
		dev_info(dev, "%s: PLL_OCUC_DONE", __func__);

	if (pll_mode <= PLL_27x1p25)
		ol_div = 0x33;
	else if (pll_mode <= PLL_59p4x1p5)
		ol_div = 0x32;
	else if (pll_mode <= PLL_74p25x1p5)
		ol_div = 0x31;
	else
		ol_div = 0x30;

	hdmipll_mask32(dev, SYS_REG_OPEN_LOOP_PLL_0,
		~(SYS_REG_OPEN_LOOP_PLL_0_REG_OL_DIV_mask |
		SYS_REG_OPEN_LOOP_PLL_0_PLL_OEB_mask),
		SYS_REG_OPEN_LOOP_PLL_0_REG_OL_DIV(ol_div) |
		SYS_REG_OPEN_LOOP_PLL_0_PLL_OEB(0));

	hdmipll_mask32(dev, SYS_REG_OPEN_LOOP_PLL_2,
		~SYS_REG_OPEN_LOOP_PLL_2_PLL_OCUC_EN_mask,
		SYS_REG_OPEN_LOOP_PLL_2_PLL_OCUC_EN(0));

	if (hdmi_format->freq_shift)
		hdmipll_mask32(dev, SYS_REG_OPEN_LOOP_PLL_2,
			~SYS_REG_OPEN_LOOP_PLL_2_PLL_SSC_EN_mask,
			SYS_REG_OPEN_LOOP_PLL_2_PLL_SSC_EN(1));

	return 0;
}

/*
 * frl_set_phy - PHY setting when FRL mode is actived
 */
int frl_set_phy(struct device *dev,
		unsigned char link_rate, struct hdmi_format_setting *hdmi_format)
{
	int ret;
	unsigned char frl_mode;
	unsigned char lane;
	int pll_mode;

	switch (link_rate) {
	case FRL_3G3LANES:
		frl_mode = FRL_3G_MODE;
		lane = FRL_3LANE;
		break;
	case FRL_6G3LANES:
		frl_mode = FRL_6G_MODE;
		lane = FRL_3LANE;
		break;
	case FRL_6G4LANES:
		frl_mode = FRL_6G_MODE;
		lane = FRL_4LANE;
		break;
	default:
		frl_mode = FRL_6G_MODE;
		lane = FRL_4LANE;
		dev_err(dev, "%s Unknown FRL_LinkRate=%u", __func__, link_rate);
		return -EINVAL;
	}

	pll_mode = get_pll_mode(dev, hdmi_format);
	if (pll_mode < 0)
		return -EINVAL;

	reset_mac_frl_pll(dev);

	enable_pll_power(dev, frl_mode);

	set_frl_pll(dev, pll_mode, frl_mode, lane);

	set_frl_mode(dev, frl_mode);

	enable_dual_loop(dev);

	ret = set_open_loop(dev, frl_mode, pll_mode, hdmi_format);
	if (ret < 0)
		return ret;

	/* Use new HDMI module */
	set_module_version(dev, 1);

	return 0;
}
