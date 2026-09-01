/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2025 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Stanley Chang <stanley_chang@realtek.com>
 */

#ifndef __RTK_RTD_PD_H
#define __RTK_RTD_PD_H

#include <linux/bits.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/types.h>

/* PD RAM window: TX/RX shared SRAM */
#define PD_RAM              0x100   /* base of message RAM window */

#define PD_TX_START_ADDR    0xA0        /* half of 320 byte */
#define PD_RAM_TX           (PD_RAM + PD_TX_START_ADDR)

/* ---------------- Base access ---------------- */
static struct regmap_config mmio_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = 0x100,
	.cache_type = REGCACHE_NONE,
};

struct pd_regmap {
	void __iomem *base;
	void __iomem *typec_base;

	void __iomem *ram_rx;
	void __iomem *ram_tx;
	void __iomem *ram_hdr;
	struct regmap *rx_map;
	struct regmap *tx_map;

	bool use_typec_ctrl1;
};

static inline u32 pd_readl(struct pd_regmap *m, u32 off)
{
	return readl(m->base + off);
}

static inline void pd_writel(struct pd_regmap *m, u32 val, u32 off)
{
	writel(val, m->base + off);
}

static inline void pd_set_bits(struct pd_regmap *m, u32 off, u32 mask)
{
	u32 v = pd_readl(m, off);

	v |= mask;
	pd_writel(m, v, off);
}

static inline void pd_clr_bits(struct pd_regmap *m, u32 off, u32 mask)
{
	u32 v = pd_readl(m, off);

	v &= ~mask;
	pd_writel(m, v, off);
}

static inline void pd_rmw(struct pd_regmap *m, u32 off, u32 mask, u32 val)
{
	u32 v = pd_readl(m, off);

	v &= ~mask;
	v |= (val & mask);
	pd_writel(m, v, off);
}

/*
 * Pd controller register base 0x98013400
 */
/* ----- Register map ----- */
#define OFF_PD_INT              0x00
#define OFF_PD_BASIC_CTRL       0x04
#define OFF_PD_DEBOUNCE_CTRL1   0x08
#define OFF_PD_DEBOUNCE_CTRL2   0x0C
#define OFF_PD_RX_CTRL1         0x10
#define OFF_PD_RX_CTRL2         0x14
#define OFF_PD_RX_ORDERSET      0x18
#define OFF_PD_DUMMY_REG        0x1C
#define OFF_PD_TX_CTRL0         0x20
#define OFF_PD_TX_CTRL1         0x24
#define OFF_PD_TX_CTRL2         0x28
#define OFF_PD_RX_STATUS1       0x30
#define OFF_PD_RX_STATUS2       0x34
#define OFF_PD_BASIC_CTRL2      0x3C
#define OFF_PD_APHY_CTRL_0      0x40
#define OFF_PD_APHY_CTRL_1      0x44
#define OFF_PD_APHY_CTRL_2      0x48
#define OFF_PD_APHY_CTRL_3      0x4C
#define OFF_PD_APHY_CC1_CTRL_0  0x50
#define OFF_PD_APHY_CC1_CTRL_1  0x54
#define OFF_PD_APHY_CC1_CTRL_2  0x58
#define OFF_PD_APHY_CTRL_4      0x5C
#define OFF_PD_APHY_CC2_CTRL_0  0x60
#define OFF_PD_APHY_CC2_CTRL_1  0x64
#define OFF_PD_APHY_CC2_CTRL_2  0x68

#define OFF_PD_OC_PROTECT_CNT1  0x70
#define OFF_PD_OC_PROTECT_CNT2  0x74
#define OFF_PD_OC_PROTECT_CNT3  0x78
#define OFF_PD_OC_PROTECT_STS1  0x7C
#define OFF_PD_OC1_PROTECT_CNT  0x80
#define OFF_PD_OC2_PROTECT_CNT  0x84
#define OFF_PD_RX_TMP1          0x88
#define OFF_PD_RX_TMP2          0x8C
#define OFF_PD_DEBOUNCE_CTRL3   0x90
#define OFF_PD_DEBOUNCE_CTRL4   0x94
#define OFF_PD_TIMER1           0x98
#define OFF_PD_BASIC_CTRL3      0x9C
#define OFF_PD_APAD_CTRL1       0xA0
#define OFF_PD_NEW_VCONN        0xA4
#define OFF_PD_RX_CTRL3         0xA4 /* old name */
#define OFF_PD_TX_CTRL3         0xA8
#define OFF_PD_TX_CTRL4         0xAC
#define OFF_PD_DEBOUNCE_CTRL5   0xB0
#define OFF_PD_TIMER2           0xB4
#define OFF_PD_DET_TX           0xB8
#define OFF_PD_DET_CTRL         0xBC
#define OFF_PD_PREAMBLE_CTRL    0xC0
#define OFF_PD_PREAMBLE_CTRL1   0xC4
#define OFF_PD_BMC_PREAMBLE     0xC8
#define OFF_PD_ORDERSET_00      0xCC
#define OFF_PD_ORDERSET_L1      0xD0
#define OFF_PD_RX_ORDERSET_CTRL 0xD4
#define OFF_PD_DEBUG_CTRL       0xD8
#define OFF_PD_DEBUG            0xDC

/* Some timing constants from pd_timer.h */
#define tSrcTransition_ms   25
#define tPSSourceOn_ms      450   /* (390,480) */
#define tPSTransition_ms    500   /* (450,550) */

/* ADC thresholds - you will likely tune these */
#define ADC_5V              256
#define ADC_9V              460
#define ADC_15V             768
#define ADC_20V             970
#define ADC_0V              40
#define ADC_VSAFE5V_L       (ADC_5V - 13)
#define ADC_VSAFE5V_H       (ADC_5V + 26)

/* ---------------- Helpers: INT ---------------- */
#define PD_INT_MSK_SARADC               BIT(30)
#define PD_INT_MSK_TX_GC                BIT(29)
#define PD_INT_MSK_CC2_FRS              BIT(28)
#define PD_INT_MSK_CC1_FRS              BIT(27)
#define PD_INT_MSK_CC_FRS               (BIT(27) | BIT(28))
#define PD_INT_MSK_VBUS_MON             BIT(26)
#define PD_INT_MSK_LOC_MON              BIT(25)
#define PD_INT_MSK_RX_OK                BIT(24)
#define PD_INT_MSK_CC2_DET              BIT(23)
#define PD_INT_MSK_CC1_DET              BIT(22)
#define PD_INT_MSK_CC_DET               (BIT(22) | BIT(23))
#define PD_INT_MSK_RX_CRV_1ST           BIT(21)
#define PD_INT_MSK_TX_OK                BIT(20)
#define PD_INT_MSK_OC2_DIS_VCONNP       BIT(19)
#define PD_INT_MSK_OC1_DIS_VCONNP       BIT(18)
#define PD_INT_MSK_OCDET_CC2            BIT(17)
#define PD_INT_MSK_OCDET_CC1            BIT(16)

/* W1C register for interrupt */
#define PD_INT_SARADC                   BIT(14)
#define PD_INT_TX_GC                    BIT(13)
#define PD_INT_CC2_FRS                  BIT(12)
#define PD_INT_CC1_FRS                  BIT(11)
#define PD_INT_CC_FRS                   (BIT(11) | BIT(12))
#define PD_INT_VBUS_MON                 BIT(10)
#define PD_INT_LOC_MON                  BIT(9)
#define PD_INT_RX_OK                    BIT(8)
#define PD_INT_CC2_DET                  BIT(7)
#define PD_INT_CC1_DET                  BIT(6)
#define PD_INT_CC_DET                   (BIT(6) | BIT(7))
#define PD_INT_RX_CRV_1ST               BIT(5)
#define PD_INT_TX_OK                    BIT(4)
#define PD_INT_OC2_DIS_VCONNP           BIT(3)
#define PD_INT_OC1_DIS_VCONNP           BIT(2)
#define PD_INT_OCDET_CC2                BIT(1)
#define PD_INT_OCDET_CC1                BIT(0)

/* interrupt mask bit and it will auto shift 16 bit by api */
static inline u32 pd_int_sts_get(struct pd_regmap *m)
{
	u32 val;

	val = pd_readl(m, OFF_PD_INT);
	val &= 0xffff;

	return val;
}

static inline void pd_int_sts_clear(struct pd_regmap *m, u32 mask)
{
	u32 val;

	val = pd_readl(m, OFF_PD_INT);
	val &= 0xffff0000;
	val |= mask & 0xffff;
	pd_writel(m, val, OFF_PD_INT); /* W1C */
}

static inline void pd_int_unmask(struct pd_regmap *m, u32 mask)
{
	u32 val;

	val = pd_readl(m, OFF_PD_INT);
	val &= 0xffff0000;
	val &= ~(mask & 0xffff0000);
	pd_writel(m, val, OFF_PD_INT);
}

static inline void pd_int_mask(struct pd_regmap *m, u32 mask)
{
	u32 val;

	val = pd_readl(m, OFF_PD_INT);
	val &= 0xffff0000;
	val |= (mask & 0xffff0000);
	pd_writel(m, val, OFF_PD_INT);
}

/* ---------------- Helpers: BASIC_CTRL ---------------- */
#define PD_INT_EN_SARADC                BIT(30)
#define PD_INT_EN_TX_GC                 BIT(29)
#define PD_INT_EN_CC2_FRS               BIT(28)
#define PD_INT_EN_CC1_FRS               BIT(27)
#define PD_INT_EN_CC_FRS                (BIT(27) | BIT(28))
#define PD_INT_EN_VBUS_MON              BIT(26)
#define PD_INT_EN_LOC_MON               BIT(25)
#define PD_INT_EN_RX                    BIT(24)
#define PD_INT_EN_CC2_DET               BIT(23)
#define PD_INT_EN_CC1_DET               BIT(22)
#define PD_INT_EN_CC_DET                (BIT(22) | BIT(23))
#define PD_INT_EN_RX_CRV_1ST            BIT(21)
#define PD_INT_EN_TX_OK                 BIT(20)
#define PD_INT_EN_OC2_DIS_VCONNP        BIT(19)
#define PD_INT_EN_OC1_DIS_VCONNP        BIT(18)
#define PD_INT_EN_OCDET_CC2             BIT(17)
#define PD_INT_EN_OCDET_CC1             BIT(16)
#define BASIC_FSM_RST            BIT(12)
#define BASIC_OC_RSET            BIT(11)
#define BASIC_RX_RST             BIT(10)
#define BASIC_TX_RST             BIT(9)
#define BASIC_BIST_RST           BIT(8)
#define BASIC_ROLLBACK_RX_MUX    BIT(4)
#define BASIC_ROLLBACK_MODE_EN   BIT(3)
#define BASIC_RX_EN              BIT(2)
#define BASIC_TX_EN              BIT(1)
#define BASIC_BIST_EN            BIT(0)


static inline void pd_basic_ctrl_set(struct pd_regmap *m, u32 val)
{
	pd_set_bits(m, OFF_PD_BASIC_CTRL, val);
}

static inline void pd_basic_ctrl_clr(struct pd_regmap *m, u32 val)
{
	pd_clr_bits(m, OFF_PD_BASIC_CTRL, val);
}

static inline void pd_basic_intr_enable(struct pd_regmap *m, u32 val)
{
	pd_basic_ctrl_set(m, val);
}

static inline void pd_basic_intr_disable(struct pd_regmap *m, u32 val)
{
	pd_basic_ctrl_clr(m, val);
}

static inline void pd_basic_bist_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_BIST_EN);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_BIST_EN);
}

static inline void pd_basic_tx_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_TX_EN);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_TX_EN);
}

static inline void pd_basic_rx_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_RX_EN);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_RX_EN);
}

static inline void pd_basic_rollback_mode_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_ROLLBACK_MODE_EN);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_ROLLBACK_MODE_EN);
}

static inline void pd_basic_rollback_rx_mux_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_ROLLBACK_RX_MUX);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_ROLLBACK_RX_MUX);
}

static inline void pd_basic_rollback_test(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL,
				BASIC_TX_EN | BASIC_RX_EN |
				BASIC_ROLLBACK_MODE_EN | BASIC_ROLLBACK_RX_MUX);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL,
				BASIC_TX_EN | BASIC_RX_EN |
				BASIC_ROLLBACK_MODE_EN | BASIC_ROLLBACK_RX_MUX);
}

static inline void pd_basic_bist_reset(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_BIST_RST);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_BIST_RST);
}

static inline void pd_basic_tx_reset(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_TX_RST);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_TX_RST);
}

static inline void pd_basic_rx_reset(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_RX_RST);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_RX_RST);
}

static inline void pd_basic_oc_reset(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_OC_RSET);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_OC_RSET);
}

static inline void pd_basic_fsm_reset(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL, BASIC_FSM_RST);
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL, BASIC_FSM_RST);
}

/* ---------------- Helpers: BASIC_CTRL2/3 ---------------- */
static inline bool pd_basic_rollback_done_status(struct pd_regmap *m)
{
	return !!(pd_readl(m, OFF_PD_BASIC_CTRL2) & BIT(24));
}

static inline bool pd_basic_rollback_fail_status(struct pd_regmap *m)
{
	return !!(pd_readl(m, OFF_PD_BASIC_CTRL2) & BIT(25));
}

static inline bool pd_basic_rollback_data_fail_status(struct pd_regmap *m)
{
	return !!(pd_readl(m, OFF_PD_BASIC_CTRL2) & BIT(26));
}

static inline void pd_basic_frs_auto_clr_cnt(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_BASIC_CTRL2, GENMASK(23, 8), FIELD_PREP(GENMASK(23, 8), v));
}

static inline void pd_basic_frs_auto_clr_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL2, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL2, BIT(1));
}

static inline void pd_basic_frs_auto_set_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_BASIC_CTRL2, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_BASIC_CTRL2, BIT(0));
}

static inline void pd_basic_sar_clk_div_set(struct pd_regmap *m, u32 div)
{
	pd_rmw(m, OFF_PD_BASIC_CTRL3, GENMASK(7, 0), FIELD_PREP(GENMASK(7, 0), div));
}

static inline u32 pd_basic_saradc_val(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(25, 16), pd_readl(m, OFF_PD_BASIC_CTRL3));
}

/* ---------------- Helpers: Debounce 1/2 ---------------- */
static inline void pd_debounce_rx_data_debounce_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_DEBOUNCE_CTRL1, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_DEBOUNCE_CTRL1, BIT(0));
}

static inline void pd_debounce_oc_det_debounce_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_DEBOUNCE_CTRL1, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_DEBOUNCE_CTRL1, BIT(1));
}

static inline void pd_debounce_cc_det_debounce_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_DEBOUNCE_CTRL1, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_DEBOUNCE_CTRL1, BIT(2));
}

static inline void pd_debounce_rx_debounce_value(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_DEBOUNCE_CTRL1, GENMASK(15, 8), FIELD_PREP(GENMASK(15, 8), v));
}

static inline void pd_debounce_cc_det_debounce_value(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_DEBOUNCE_CTRL1, GENMASK(23, 16), FIELD_PREP(GENMASK(23, 16), v));
}

static inline u32 pd_debounce_cc1_det_debounce_out(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(3, 0), pd_readl(m, OFF_PD_DEBOUNCE_CTRL2));
}

static inline u32 pd_debounce_cc2_det_debounce_out(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(7, 4), pd_readl(m, OFF_PD_DEBOUNCE_CTRL2));
}

static inline u32 pd_debounce_oc1_det_debounce_out(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_DEBOUNCE_CTRL2) & BIT(8) >> 8;
}

static inline u32 pd_debounce_oc2_det_debounce_out(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_DEBOUNCE_CTRL2) & BIT(9) >> 9;
}

static inline u32 pd_debounce_rx_data_debounce_out(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_DEBOUNCE_CTRL2) & (BIT(10) | BIT(11)) >> 10;
}

static inline u32 pd_debounce_rx_data_debounce_out_0(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_DEBOUNCE_CTRL2) & BIT(10) >> 10;
}

static inline u32 pd_debounce_rx_data_debounce_out_1(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_DEBOUNCE_CTRL2) & BIT(11) >> 11;
}

static inline u32 pd_debounce_loc_mon_debounce_out(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_DEBOUNCE_CTRL2) & BIT(12) >> 12;
}

static inline u32 pd_debounce_vbus_debounce_out(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_DEBOUNCE_CTRL2) & BIT(13) >> 13;
}

static inline u32 pd_debounce_cc1_det_debounce_mask(struct pd_regmap *m, u32 mask)
{
	return pd_readl(m, OFF_PD_DEBOUNCE_CTRL2) & BIT(13) >> 13;
}

/* bits [19:16]  CC1_DET_DEBOUNCE_MSK */
static inline void pd_debounce_cc1_det_debounce_msk(struct pd_regmap *m, u32 msk)
{
	pd_rmw(m, OFF_PD_DEBOUNCE_CTRL2,  GENMASK(19, 16),
	       FIELD_PREP(GENMASK(19, 16), msk));
}

/* bits [23:20]  CC2_DET_DEBOUNCE_MSK */
static inline void pd_debounce_cc2_det_debounce_msk(struct pd_regmap *m, u32 msk)
{
	pd_rmw(m, OFF_PD_DEBOUNCE_CTRL2, GENMASK(23, 20),
	       FIELD_PREP(GENMASK(23, 20), msk));
}

/* bits [26:24]  RX_DATA_DEBOUNCE_MSK */
static inline void pd_debounce_rx_data_debounce_msk(struct pd_regmap *m, u32 msk)
{
	pd_rmw(m, OFF_PD_DEBOUNCE_CTRL2, GENMASK(26, 24),
	       FIELD_PREP(GENMASK(26, 24), msk));
}

/* bit[26]  LOC_MON_DEBOUNCE_MSK */
static inline void pd_debounce_loc_mon_debounce_msk(struct pd_regmap *m, u32 msk)
{
	if (msk >= 1)
		pd_set_bits(m, OFF_PD_DEBOUNCE_CTRL2, BIT(26));
	else
		pd_clr_bits(m, OFF_PD_DEBOUNCE_CTRL2, BIT(26));
}

/* bit[27]  VBUS_MON_DEBOUNCE_MSK */
static inline void pd_debounce_vbus_mon_debounce_msk(struct pd_regmap *m, u32 msk)
{
	if (msk >= 1)
		pd_set_bits(m, OFF_PD_DEBOUNCE_CTRL2, BIT(27));
	else
		pd_clr_bits(m, OFF_PD_DEBOUNCE_CTRL2, BIT(27));
}

/* ---------------- Helpers: RX CTRL1/2 ---------------- */
static inline void pd_rxctrl_preamble_sw_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL1, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL1, BIT(0));
}

static inline void pd_rxctrl_orderset_cmp_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL1, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL1, BIT(3));
}

static inline void pd_rxctrl_rx_data_bypass(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL1, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL1, BIT(4));
}

static inline void pd_rxctrl_rx_data_sel_msb(struct pd_regmap *m, bool msb)
{
	if (msb)
		pd_set_bits(m, OFF_PD_RX_CTRL1, BIT(5));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL1, BIT(5));
}

/* 1: interrupt CPU if receive EOP first, 0: interrupt CPU if BMC timeout */
static inline void pd_rxctrl_rcv_eop_int_en(struct pd_regmap *m, u32 en)
{
	if (en >= 1)
		pd_set_bits(m, OFF_PD_RX_CTRL1, BIT(6));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL1, BIT(6));
}

static inline void pd_rxctrl_post_preamble_sw_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_RX_CTRL1, GENMASK(19, 8), FIELD_PREP(GENMASK(19, 8), cnt));
}

static inline void pd_rxctrl_pre_preamble_sw_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_RX_CTRL1, GENMASK(31, 20), FIELD_PREP(GENMASK(31, 20), cnt));
}

static inline void pd_rxctrl_cable_rst_mask(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL2, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL2, BIT(0));
}

static inline void pd_rxctrl_hard_rst_mask(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL2, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL2, BIT(1));
}

static inline void pd_rxctrl_sop_mask(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL2, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL2, BIT(2));
}

/* bit[3] SOP1_MASK */
static inline void pd_rxctrl_sop1_mask(struct pd_regmap *m, u32 en)
{
	if (en >= 1)
		pd_set_bits(m, OFF_PD_RX_CTRL2, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL2, BIT(3));
}

/* bit[4] SOP1_DBG_MASK */
static inline void pd_rxctrl_sop1_dbg_mask(struct pd_regmap *m, u32 en)
{
	if (en >= 1)
		pd_set_bits(m, OFF_PD_RX_CTRL2, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL2, BIT(4));
}

/* bit[5] SOP2_MASK */
static inline void pd_rxctrl_sop2_mask(struct pd_regmap *m, u32 en)
{
	if (en >= 1)
		pd_set_bits(m, OFF_PD_RX_CTRL2, BIT(5));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL2, BIT(5));
}

/* bit[6] SOP2_DBG_MASK */
static inline void pd_rxctrl_sop2_dbg_mask(struct pd_regmap *m, u32 en)
{
	if (en >= 1)
		pd_set_bits(m, OFF_PD_RX_CTRL2, BIT(6));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL2, BIT(6));
}

static inline void pd_rxctrl_bmc_preamble_timeout_en(struct pd_regmap *m, u32 en)
{
	if (en >= 1)
		pd_set_bits(m, OFF_PD_RX_CTRL2, BIT(7));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL2, BIT(7));
}

static inline void pd_rxctrl_bmc_timeout_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_RX_CTRL2, GENMASK(19, 8), FIELD_PREP(GENMASK(19, 8), cnt));
}

static inline void pd_rxctrl_preamble_freq_sw(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_RX_CTRL2, GENMASK(31, 20), FIELD_PREP(GENMASK(31, 20), v));
}

/* ---------------- Helpers: Ordered set match ---------------- */
static inline bool pd_rxctrl_orderedset_match(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return !!FIELD_GET(BIT(0), v);
}

static inline bool pd_rxctrl_cable_rst_match(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return !!FIELD_GET(BIT(1), v);
}

static inline bool pd_rxctrl_hard_rst_match(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return !!FIELD_GET(BIT(2), v);
}

static inline bool pd_rxctrl_sop_match(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return !!FIELD_GET(BIT(3), v);
}

static inline bool pd_rxctrl_sop1_match(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return !!FIELD_GET(BIT(4), v);
}

static inline bool pd_rxctrl_sop1_dbg_match(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return !!FIELD_GET(BIT(5), v);
}

static inline bool pd_rxctrl_sop2_match(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return !!FIELD_GET(BIT(6), v);
}

static inline bool pd_rxctrl_sop2_dbg_match(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return !!FIELD_GET(BIT(7), v);
}

/* PD_RX_ORDERSET register bit fields (from sample code: hard_rst_match etc.) */
#define PD_RX_ORDERSET_MATCH    BIT(0)  /* Any ordered set matched */
#define PD_RX_CABLE_RST_MATCH   BIT(1)  /* Cable Reset ordered set */
#define PD_RX_HARD_RST_MATCH    BIT(2)  /* Hard Reset ordered set */
#define PD_RX_SOP_MATCH         BIT(3)  /* SOP */
#define PD_RX_SOP1_MATCH        BIT(4)  /* SOP' */
#define PD_RX_SOP1_DBG_MATCH    BIT(5)  /* SOP'' debug */
#define PD_RX_SOP2_MATCH        BIT(6)  /* SOP'' */
#define PD_RX_SOP2_DBG_MATCH    BIT(7)  /* SOP'' debug */

static inline u32 pd_rxctrl_orderset_cmp_stat(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_RX_ORDERSET);
}

static inline u32 pd_rxctrl_rx_orderedset_data(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_ORDERSET);

	return FIELD_GET(GENMASK(31, 12), v);
}

/* ---------------- Helpers: RX status ---------------- */
static inline u32 pd_rxctrl_rx_data_byte_cnt(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(8, 0), pd_readl(m, OFF_PD_RX_STATUS1));
}

static inline bool pd_rxctrl_rx_crc_cmp_result(struct pd_regmap *m)
{
	return !!(pd_readl(m, OFF_PD_RX_STATUS1) & BIT(10));
}

/* RX Status helpers */
#define RX_BYTE_CNT(v)      ((readl(v + OFF_PD_RX_STATUS1)) & 0x1FF)
#define RX_CRC_OK(v)        (readl(v + OFF_PD_RX_STATUS1) & BIT(10))

/* ---------------- Helpers: RX_STATUS2 ---------------- */
/* BIT0-8 byte location of RX receive EOP */
static inline u32 pd_rxctrl_rev_eop_loc(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_STATUS2);

	return FIELD_GET(GENMASK(8, 0), v);
}

/* RX data bit counts (bypass mode): bits [11:9] */
static inline u32 pd_rxctrl_rx_bypass_bit_count(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_STATUS2);

	return FIELD_GET(GENMASK(11, 9), v);
}

/* BIT 12-20 byte location of RX 5b4b decode fail */
static inline u32 pd_rxctrl_fail_dec_loc(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_STATUS2);

	return FIELD_GET(GENMASK(20, 12), v);
}

/* RX receive data timeout (data = 0), bit[21] */
static inline bool pd_rxctrl_bmc_timeout_0(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_STATUS2);

	return !!FIELD_GET(BIT(21), v);
}

/* RX receive data timeout (data = 1), bit[22] */
static inline bool pd_rxctrl_bmc_timeout_1(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_STATUS2);
	return !!FIELD_GET(BIT(22), v);
}

/* RX 5b4b decode fail, bit[23] */
static inline bool pd_rxctrl_fail_dec_5b4b(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_STATUS2);

	return !!FIELD_GET(BIT(23), v);
}

/* RX FSM state: bits [30:24] */
static inline u32 pd_rxctrl_rx_fsm_state(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_STATUS2);

	return FIELD_GET(GENMASK(30, 24), v);
}

/* RX received EOP flag: bit[31] */
static inline bool pd_rxctrl_rx_rec_eop(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_RX_STATUS2);

	return !!FIELD_GET(BIT(31), v);
}

/* ---------------- Helpers: Dummy regs ---------------- */
/*
 *  PD dummy0 reg (ECO for SARADC)
 *      0: SARADC avg. enable
 *      1: SARADC avg. end
 *      2: SARADC data mux sel
 *      3: PD RX SMIT enable
 */
static inline void pd_adc_avg_ctrl(struct pd_regmap *m, u32 ctrl)
{
	pd_writel(m, ctrl & 0xffff, OFF_PD_DUMMY_REG);
}

static inline bool pd_is_adc_en(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_DUMMY_REG);
	return !!(v & BIT(0));
}

static inline void pd_rx_smt_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_DUMMY_REG, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_DUMMY_REG, BIT(3));
}

/* PD dummy1 (ECO to plr_en_CC1/CC2) */
static inline void pd_rd_plr_en_cc1(struct pd_regmap *m, u32 en)
{
	if (en >= 1)
		pd_set_bits(m, OFF_PD_DUMMY_REG, BIT(16));
	else
		pd_clr_bits(m, OFF_PD_DUMMY_REG, BIT(16));
}

static inline void pd_rd_plr_en_cc2(struct pd_regmap *m, u32 en)
{
	if (en >= 1)
		pd_set_bits(m, OFF_PD_DUMMY_REG, BIT(17));
	else
		pd_clr_bits(m, OFF_PD_DUMMY_REG, BIT(17));
}

/* ---------------- Helpers: TX CTRLs ---------------- */
/* PD_TX_CTRL0 */
static inline void pd_txctrl_tx_orderset(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_TX_CTRL0, GENMASK(19, 0), FIELD_PREP(GENMASK(19, 0), val));
}

static inline void pd_txctrl_tx_length(struct pd_regmap *m, u32 len)
{
	pd_rmw(m, OFF_PD_TX_CTRL0, GENMASK(31, 20), FIELD_PREP(GENMASK(31, 20), len));
}

/* PD_TX_CTRL1 */
static inline void pd_txctrl_tx_data_sent_period(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_TX_CTRL1, GENMASK(15, 0), FIELD_PREP(GENMASK(15, 0), cnt));
}

static inline void pd_txctrl_tx_bmc_end_cnt_val(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_TX_CTRL1, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), cnt));
}

/* PD_TX_CTRL2 */
static inline void pd_txctrl_tx_crc_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL2, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL2, BIT(0));
}

static inline void pd_txctrl_tx_eop_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL2, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL2, BIT(1));
}

static inline void pd_txctrl_tx_data_by_sw_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL2, BIT(8));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL2, BIT(8));
}

static inline void pd_txctrl_tx_bmc_cnt_rst(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL2, BIT(9));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL2, BIT(9));
}

static inline void pd_txctrl_tx_bmc_cnt_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL2, BIT(10));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL2, BIT(10));
}

static inline void pd_txctrl_tx_data_sw(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL2, BIT(11));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL2, BIT(11));
}

static inline void pd_txctrl_tx_data_en_sw(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL2, BIT(12));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL2, BIT(12));
}

static inline void pd_txctrl_tx_ana_auto_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL2, BIT(13));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL2, BIT(13));
}

static inline void pd_txctrl_tx_en_value(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_TX_CTRL2, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), val));
}

/* ---------------- Helpers: APHY_CTRL0 ---------------- */
static inline void pd_aphyctrl_aphy_rx_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_0, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_0, BIT(0));
}

static inline void pd_aphyctrl_aphy_tx_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_0, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_0, BIT(1));
}

static inline void pd_aphyctrl_reg_verf_low(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_0, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_0, BIT(2));
}

static inline void pd_aphyctrl_aphy_set_vswing(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_0, GENMASK(5, 3),
	       FIELD_PREP(GENMASK(5, 3), val));
}

static inline void pd_aphyctrl_aphy_set_sr_n(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_0, GENMASK(11, 8),
	       FIELD_PREP(GENMASK(11, 8), val));
}

static inline void pd_aphyctrl_aphy_set_sr_p(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_0, GENMASK(15, 12),
	       FIELD_PREP(GENMASK(15, 12), val));
}

static inline void pd_aphyctrl_aphy_set_tx_cur(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_0, GENMASK(19, 16),
	       FIELD_PREP(GENMASK(19, 16), val));
}

static inline void pd_aphyctrl_aphy_set_tx_term(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_0, GENMASK(23, 20),
	       FIELD_PREP(GENMASK(23, 20), val));
}

static inline void pd_aphyctrl_aphy_set_bmc_lpf(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_0, GENMASK(27, 24),
	       FIELD_PREP(GENMASK(27, 24), val));
}

static inline void pd_aphyctrl_aphy_en_clk_dbrd(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_0, BIT(28));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_0, BIT(28));
}

/* ---------------- Helpers: APHY_CTRL1 (BMC PHY RX) ---------------- */
static inline void pd_aphyctrl_en_sink(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_1, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_1, BIT(0));
}

static inline void pd_aphyctrl_reg_vh_sink(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_1, GENMASK(10, 6),
	       FIELD_PREP(GENMASK(10, 6), val));
}

static inline void pd_aphyctrl_reg_vl_sink(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_1, GENMASK(15, 11),
	       FIELD_PREP(GENMASK(15, 11), val));
}

static inline void pd_aphyctrl_en_source(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_1, BIT(16));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_1, BIT(16));
}

static inline void pd_aphyctrl_reg_vh_source(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_1, GENMASK(26, 22),
	       FIELD_PREP(GENMASK(26, 22), val));
}

static inline void pd_aphyctrl_reg_vl_source(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_1, GENMASK(31, 27),
	       FIELD_PREP(GENMASK(31, 27), val));
}

/* ---------------- Helpers: APHY_CTRL2 ---------------- */
static inline void pd_aphyctrl_reg_dum_a0(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_2, GENMASK(7, 0),
	       FIELD_PREP(GENMASK(7, 0), val));
}

static inline void pd_aphyctrl_reg_dum_a1(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_2, GENMASK(15, 8),
	       FIELD_PREP(GENMASK(15, 8), val));
}

static inline void pd_aphyctrl_reg_dum_b0(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_2, GENMASK(23, 16),
	       FIELD_PREP(GENMASK(23, 16), val));
}

static inline void pd_aphyctrl_reg_dum_b1(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_2, GENMASK(31, 24),
	       FIELD_PREP(GENMASK(31, 24), val));
}

/* ---------------- Helpers: APHY_CTRL3 ---------------- */
static inline void pd_aphyctrl_reg_holdb(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_3, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_3, BIT(0));
}

static inline void pd_aphyctrl_reg_pow_prs(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_3, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_3, BIT(1));
}

static inline void pd_aphyctrl_reg_pow_frs(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_3, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_3, BIT(2));
}

static inline void pd_aphyctrl_reg_en_ibhx(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_3, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_3, BIT(4));
}

static inline void pd_aphyctrl_reg_en_ibhn(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_3, BIT(5));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_3, BIT(5));
}

static inline void pd_aphyctrl_reg_prs_tune(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_3, GENMASK(11, 8),
	       FIELD_PREP(GENMASK(11, 8), val));
}

static inline void pd_aphyctrl_reg_frs_tune(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_3, GENMASK(15, 12),
	       FIELD_PREP(GENMASK(15, 12), val));
}

static inline void pd_aphyctrl_reg_srp(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_3, GENMASK(27, 24),
	       FIELD_PREP(GENMASK(27, 24), val));
}

static inline void pd_aphyctrl_reg_snp(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_3, GENMASK(31, 28),
	       FIELD_PREP(GENMASK(31, 28), val));
}

/* ---------------- Helpers: APHY_CTRL4 ---------------- */
static inline void pd_aphyctrl_reg_sar_pow(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_4, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_4, BIT(0));
}

static inline void pd_aphyctrl_reg_sar_ldo_pow(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_4, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_4, BIT(1));
}

static inline void pd_aphyctrl_reg_sar_lpf_bypass(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_4, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_4, BIT(2));
}

static inline void pd_aphyctrl_reg_sar_amp_pow(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_4, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_4, BIT(3));
}

static inline void pd_aphyctrl_reg_sar_amp_bypass(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CTRL_4, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_APHY_CTRL_4, BIT(4));
}

static inline void pd_aphyctrl_reg_sar_amp_gain(struct pd_regmap *m, u32 sel)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_4, GENMASK(7, 5),
	       FIELD_PREP(GENMASK(7, 5), sel));
}

static inline void pd_aphyctrl_reg_sar_verf_sel(struct pd_regmap *m, u32 sel)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_4, GENMASK(11, 8),
	       FIELD_PREP(GENMASK(11, 8), sel));
}

static inline void pd_aphyctrl_reg_sar_dummy(struct pd_regmap *m, u32 sel)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_4, GENMASK(15, 12),
	       FIELD_PREP(GENMASK(15, 12), sel));
}

static inline void pd_aphyctrl_reg_sar_chsel(struct pd_regmap *m, u32 ch)
{
	pd_rmw(m, OFF_PD_APHY_CTRL_4, GENMASK(19, 16),
	       FIELD_PREP(GENMASK(19, 16), ch));
}

/* ---------------- Helpers: APHY_CC1_CTRL0 ---------------- */
static inline void pd_aphycc_cc1_pow_det(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(0));
}

static inline void pd_aphycc_cc1_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(1));
}

static inline void pd_aphycc_cc1_cct_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(2));
}

static inline void pd_aphycc_cc1_vconn_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(3));
}

static inline void pd_aphycc_cc1_plr_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(4));
}

static inline void pd_aphycc_cc1_channel_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(5));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(5));
}

static inline bool pd_aphycc_cc1_channel_chk(struct pd_regmap *m)
{
	return !!(pd_readl(m, OFF_PD_APHY_CC1_CTRL_0) & BIT(5));
}

static inline void pd_aphycc_cc1_ocpadj_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(6));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(6));
}

static inline void pd_aphycc_cc1_vconnp_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(7));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(7));
}

static inline void pd_aphycc_cc1_ra_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(8));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(8));
}

static inline void pd_aphycc_cc1_ra_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_0, GENMASK(11, 9),
	       FIELD_PREP(GENMASK(11, 9), val));
}

static inline void pd_aphycc_cc1_rd_frs_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(12));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(12));
}

static inline void pd_aphycc_cc1_rd_frs_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_0, GENMASK(14, 13),
	       FIELD_PREP(GENMASK(14, 13), val));
}

static inline void pd_aphycc_cc1_en_cc_det_2_1(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(15));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(15));
}

static inline void pd_aphycc_cc1_rd_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(16));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(16));
}

static inline void pd_aphycc_cc1_rd_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_0, GENMASK(21, 17),
	       FIELD_PREP(GENMASK(21, 17), val));
}

static inline void pd_aphycc_cc1_ref_sel0(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_0, GENMASK(25, 24),
	       FIELD_PREP(GENMASK(25, 24), val));
}

static inline void pd_aphycc_cc1_ref_sel1(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_0, GENMASK(27, 26),
	       FIELD_PREP(GENMASK(27, 26), val));
}

static inline void pd_aphycc_cc1_cc_ref_0p2v_sel(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(28));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(28));
}

static inline void pd_aphycc_cc1_cc_ref_0p66v_sel(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(29));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(29));
}

static inline void pd_aphycc_cc1_cc_ref_1p23v_sel(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(30));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_0, BIT(30));
}

/* ---------------- Helpers: APHY_CC1_CTRL1 ---------------- */
static inline void pd_aphycc_cc1_rp4p7k_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(0));
}

static inline void pd_aphycc_cc1_rp4p7k_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_1, GENMASK(5, 1),
	       FIELD_PREP(GENMASK(5, 1), val));
}

static inline void pd_aphycc_cc1_rp12k_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(8));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(8));
}

static inline void pd_aphycc_cc1_rp12k_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_1, GENMASK(13, 9),
	       FIELD_PREP(GENMASK(13, 9), val));
}

static inline void pd_aphycc_cc1_rp36k_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(16));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(16));
}

static inline void pd_aphycc_cc1_rp36k_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_1, GENMASK(21, 17),
	       FIELD_PREP(GENMASK(21, 17), val));
}

static inline void pd_aphycc_cc1_ocpvref_sel(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_1, GENMASK(26, 24),
	       FIELD_PREP(GENMASK(26, 24), val));
}

static inline void pd_aphycc_cc1_ocp_deg_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(27));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(27));
}

static inline void pd_aphycc_cc1_ocp_rb_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(28));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(28));
}

static inline void pd_aphycc_cc1_rp_disable(struct pd_regmap *m)
{
	pd_clr_bits(m, OFF_PD_APHY_CC1_CTRL_1, BIT(16) | BIT(8) | BIT(0));
}

/* ---------------- Helpers: APHY_CC1_CTRL2 ---------------- */
static inline void pd_aphycc_cc1_vref_2p6v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_2, GENMASK(3, 0),
	       FIELD_PREP(GENMASK(3, 0), val));
}

static inline void pd_aphycc_cc1_vref_1p23v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_2, GENMASK(7, 4),
	       FIELD_PREP(GENMASK(7, 4), val));
}

static inline void pd_aphycc_cc1_vref_1p6v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_2, GENMASK(11, 8),
	       FIELD_PREP(GENMASK(11, 8), val));
}

static inline void pd_aphycc_cc1_vref_0p8v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_2, GENMASK(15, 12),
	       FIELD_PREP(GENMASK(15, 12), val));
}

static inline void pd_aphycc_cc1_vref_0p66v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_2, GENMASK(19, 16),
	       FIELD_PREP(GENMASK(19, 16), val));
}

static inline void pd_aphycc_cc1_vref_0p49v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_2, GENMASK(23, 20),
	       FIELD_PREP(GENMASK(23, 20), val));
}

static inline void pd_aphycc_cc1_vref_0p4v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_2, GENMASK(27, 24),
	       FIELD_PREP(GENMASK(27, 24), val));
}

static inline void pd_aphycc_cc1_vref_0p2v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC1_CTRL_2, GENMASK(31, 28),
	       FIELD_PREP(GENMASK(31, 28), val));
}

/* ---------------- Helpers: APHY_CC2_CTRL0 ---------------- */
static inline void pd_aphycc_cc2_pow_det(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(0));
}

static inline void pd_aphycc_cc2_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(1));
}

static inline void pd_aphycc_cc2_cct_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(2));
}

static inline void pd_aphycc_cc2_vconn_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(3));
}

static inline void pd_aphycc_cc2_plr_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(4));
}

static inline void pd_aphycc_cc2_channel_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(5));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(5));
}

static inline bool pd_aphycc_cc2_channel_chk(struct pd_regmap *m)
{
	return !!(pd_readl(m, OFF_PD_APHY_CC2_CTRL_0) & BIT(5));
}

static inline void pd_aphycc_cc2_ocpadj_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(6));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(6));
}

static inline void pd_aphycc_cc2_vconnp_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(7));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(7));
}

static inline void pd_aphycc_cc2_ra_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(8));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(8));
}

static inline void pd_aphycc_cc2_ra_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_0, GENMASK(11, 9),
	       FIELD_PREP(GENMASK(11, 9), val));
}

static inline void pd_aphycc_cc2_rd_frs_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(12));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(12));
}

static inline void pd_aphycc_cc2_rd_frs_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_0, GENMASK(14, 13),
	       FIELD_PREP(GENMASK(14, 13), val));
}

static inline void pd_aphycc_cc2_en_cc_det_2_1(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(15));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(15));
}

static inline void pd_aphycc_cc2_rd_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(16));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(16));
}

static inline void pd_aphycc_cc2_rd_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_0, GENMASK(21, 17),
	       FIELD_PREP(GENMASK(21, 17), val));
}

static inline void pd_aphycc_cc2_ref_sel0(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_0, GENMASK(25, 24),
	       FIELD_PREP(GENMASK(25, 24), val));
}

static inline void pd_aphycc_cc2_ref_sel1(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_0, GENMASK(27, 26),
	       FIELD_PREP(GENMASK(27, 26), val));
}

static inline void pd_aphycc_cc2_cc_ref_0p2v_sel(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(28));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(28));
}

static inline void pd_aphycc_cc2_cc_ref_0p66v_sel(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(29));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(29));
}

static inline void pd_aphycc_cc2_cc_ref_1p23v_sel(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(30));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_0, BIT(30));
}

/* ---------------- Helpers: APHY_CC2_CTRL1 ---------------- */
static inline void pd_aphycc_cc2_rp4p7k_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(0));
}

static inline void pd_aphycc_cc2_rp4p7k_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_1, GENMASK(5, 1),
	       FIELD_PREP(GENMASK(5, 1), val));
}

static inline void pd_aphycc_cc2_rp12k_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(8));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(8));
}

static inline void pd_aphycc_cc2_rp12k_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_1, GENMASK(13, 9),
	       FIELD_PREP(GENMASK(13, 9), val));
}

static inline void pd_aphycc_cc2_rp36k_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(16));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(16));
}

static inline void pd_aphycc_cc2_rp36k_code(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_1, GENMASK(21, 17),
	       FIELD_PREP(GENMASK(21, 17), val));
}

static inline void pd_aphycc_cc2_ocpvref_sel(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_1, GENMASK(26, 24),
	       FIELD_PREP(GENMASK(26, 24), val));
}

static inline void pd_aphycc_cc2_ocp_deg_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(27));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(27));
}

static inline void pd_aphycc_cc2_ocp_rb_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(28));
	else
		pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(28));
}

static inline void pd_aphycc_cc2_rp_disable(struct pd_regmap *m)
{
	pd_clr_bits(m, OFF_PD_APHY_CC2_CTRL_1, BIT(16) | BIT(8) | BIT(0));
}

/* ---------------- Helpers: APHY_CC2_CTRL2 ---------------- */
static inline void pd_aphycc_cc2_vref_2p6v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_2, GENMASK(3, 0),
	       FIELD_PREP(GENMASK(3, 0), val));
}

static inline void pd_aphycc_cc2_vref_1p23v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_2, GENMASK(7, 4),
	       FIELD_PREP(GENMASK(7, 4), val));
}

static inline void pd_aphycc_cc2_vref_1p6v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_2, GENMASK(11, 8),
	       FIELD_PREP(GENMASK(11, 8), val));
}

static inline void pd_aphycc_cc2_vref_0p8v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_2, GENMASK(15, 12),
	       FIELD_PREP(GENMASK(15, 12), val));
}

static inline void pd_aphycc_cc2_vref_0p66v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_2, GENMASK(19, 16),
	       FIELD_PREP(GENMASK(19, 16), val));
}

static inline void pd_aphycc_cc2_vref_0p49v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_2, GENMASK(23, 20),
	       FIELD_PREP(GENMASK(23, 20), val));
}

static inline void pd_aphycc_cc2_vref_0p4v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_2, GENMASK(27, 24),
	       FIELD_PREP(GENMASK(27, 24), val));
}

static inline void pd_aphycc_cc2_vref_0p2v(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_APHY_CC2_CTRL_2, GENMASK(31, 28),
	       FIELD_PREP(GENMASK(31, 28), val));
}

/* ---------------- Helpers: TX CTRLs ---------------- */
static inline void pd_txctrl_auto_resp_good_crc_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL3, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL3, BIT(0));
}

static inline void pd_txctrl_sop_resp_crc_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL3, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL3, BIT(1));
}

static inline void pd_txctrl_sop1_resp_crc_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL3, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL3, BIT(2));
}

static inline void pd_txctrl_sop1_dbg_resp_crc_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL3, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL3, BIT(3));
}

static inline void pd_txctrl_sop2_resp_crc_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL3, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL3, BIT(4));
}

static inline void pd_txctrl_sop2_dbg_resp_crc_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL3, BIT(5));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL3, BIT(5));
}

static inline void pd_txctrl_wait_tx_gc_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_TX_CTRL3, GENMASK(23, 8), FIELD_PREP(GENMASK(23, 8), cnt));
}

static inline void pd_txctrl_tx_start_addr(struct pd_regmap *m, u32 addr)
{
	pd_rmw(m, OFF_PD_TX_CTRL3, GENMASK(31, 24), FIELD_PREP(GENMASK(31, 24), addr));
}

/* ---------------- Helpers: TX_CTRL4 (GoodCRC HW mode) ---------------- */
static inline void pd_txctrl_tx_msg_data_role(struct pd_regmap *m, bool dfp)
{
	if (dfp)
		pd_set_bits(m, OFF_PD_TX_CTRL4, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL4, BIT(0));
}

static inline void pd_txctrl_tx_msg_spec_rev(struct pd_regmap *m, u32 rev)
{
	pd_rmw(m, OFF_PD_TX_CTRL4, GENMASK(2, 1), FIELD_PREP(GENMASK(2, 1), rev));
}

static inline void pd_txctrl_tx_msg_power_role(struct pd_regmap *m, bool src)
{
	if (src)
		pd_set_bits(m, OFF_PD_TX_CTRL4, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL4, BIT(3));
}

static inline void pd_txctrl_tx_msg_extended(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_TX_CTRL4, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_TX_CTRL4, BIT(4));
}

static inline bool pd_txctrl_tx_gc_collision(struct pd_regmap *m)
{
	return !!(pd_readl(m, OFF_PD_TX_CTRL4) & BIT(5));
}

static inline void pd_txctrl_tx_gc_collision_clear(struct pd_regmap *m)
{
	pd_set_bits(m, OFF_PD_TX_CTRL4, BIT(5));
}

static inline void pd_txctrl_tx_msg_gc_symbol(struct pd_regmap *m, u32 order)
{
	pd_rmw(m, OFF_PD_TX_CTRL4, GENMASK(27, 8),
	       FIELD_PREP(GENMASK(27, 8), order));
}

static inline u32 pd_txctrl_tx_gc_fsm(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_TX_CTRL4) >> 28;
}

/* ---------------- Helpers: DET_TX ---------------- */
static inline void pd_txctrl_detect_tx_col_enable(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_DET_TX, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_DET_TX, BIT(0));
}

static inline void pd_txctrl_detect_tx_col_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_DET_TX, GENMASK(7, 4), FIELD_PREP(GENMASK(7, 4), cnt));
}

/* ---------------- Helpers: OCP ---------------- */
static inline void pd_ocp_shut2off_cnt(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_OC_PROTECT_CNT1, GENMASK(15, 0), FIELD_PREP(GENMASK(15, 0), v));
}

static inline void pd_ocp_shut_cnt(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_OC_PROTECT_CNT1, GENMASK(19, 16), FIELD_PREP(GENMASK(19, 16), v));
}

static inline void pd_ocp_on2prot_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_OC_PROTECT_CNT1, GENMASK(31, 20), FIELD_PREP(GENMASK(31, 20), cnt));
}

static inline void pd_ocp_prtect2on_cnt(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_OC_PROTECT_CNT2, GENMASK(15, 0), FIELD_PREP(GENMASK(15, 0), v));
}

static inline void pd_ocp_start2on_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_OC_PROTECT_CNT2, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), cnt));
}

static inline void pd_ocp_start2shut_cnt(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_OC_PROTECT_CNT3, GENMASK(15, 0), FIELD_PREP(GENMASK(15, 0), v));
}

static inline void pd_ocp_prtect2shut_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_OC_PROTECT_CNT3, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), cnt));
}

static inline u32 pd_oc1_prtect_sts(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_OC_PROTECT_STS1);

	return FIELD_GET(GENMASK(4, 0), v);
}

static inline u32 pd_oc1_prtect_sd_cnt(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_OC_PROTECT_STS1);

	return FIELD_GET(GENMASK(11, 8), v);
}

static inline u32 pd_oc2_prtect_sts(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_OC_PROTECT_STS1);

	return FIELD_GET(GENMASK(20, 16), v);
}

static inline u32 pd_oc2_prtect_sd_cnt(struct pd_regmap *m)
{
	u32 v = pd_readl(m, OFF_PD_OC_PROTECT_STS1);

	return FIELD_GET(GENMASK(27, 24), v);
}

static inline u32 oc1_protect_cnt(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_OC1_PROTECT_CNT);
}

static inline u32 oc2_protect_cnt(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_OC2_PROTECT_CNT);
}

/* ----------- pd RX_TMP register */
static inline void pd_rxctrl_rx_data_tmp_r1(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_RX_TMP1, GENMASK(1, 0), FIELD_PREP(GENMASK(1, 0), v));
}

static inline void pd_rxctrl_rx_bmc_width_cnt_r1(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_RX_TMP1, GENMASK(15, 4), FIELD_PREP(GENMASK(15, 4), v));
}

static inline void pd_rxctrl_rx_bmc_num_cnt_r1(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_RX_TMP1, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), v));
}

static inline void pd_rxctrl_rx_data_tmp_r2(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_RX_TMP2, GENMASK(1, 0), FIELD_PREP(GENMASK(1, 0), v));
}

static inline void pd_rxctrl_rx_bmc_width_cnt_r2(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_RX_TMP2, GENMASK(15, 4), FIELD_PREP(GENMASK(15, 4), v));
}

static inline void pd_rxctrl_rx_bmc_num_cnt_r2(struct pd_regmap *m, u32 v)
{
	pd_rmw(m, OFF_PD_RX_TMP2, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), v));
}

/* ---------------- Debounce 3/4/5 ---------------- */
static inline void pd_debounce_loc_mon_debounce_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_writel(m, cnt, OFF_PD_DEBOUNCE_CTRL3);
}

static inline void pd_debounce_vbus_mon_debounce_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_DEBOUNCE_CTRL3, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), cnt));
}

static inline void pd_debounce_oc_det_debounce_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_DEBOUNCE_CTRL4, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), cnt));
}

static inline void pd_debounce_frs_det_debounce_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_DEBOUNCE_CTRL4, GENMASK(15, 0), FIELD_PREP(GENMASK(15, 0), cnt));
}

static inline void pd_debounce_cc_det_debounce_value_32(struct pd_regmap *m, u32 v)
{
	pd_writel(m, v, OFF_PD_DEBOUNCE_CTRL5);
}

/* ---------------- Helpers: Timer ---------------- */
static inline u32 pd_timer1_get(struct pd_regmap *m, u32 cnt)
{
	return pd_readl(m, OFF_PD_TIMER1);
}

static inline void pd_timer1_set(struct pd_regmap *m, u32 cnt)
{
	pd_writel(m, cnt, OFF_PD_TIMER1);
}

static inline u32 pd_timer2_get(struct pd_regmap *m)
{
	return pd_readl(m, OFF_PD_TIMER2);
}

static inline void pd_timer2_set(struct pd_regmap *m, u32 cnt)
{
	pd_writel(m, cnt, OFF_PD_TIMER2);
}

/* ---------------- Helpers: APAD_CTRL1 ---------------- */

static inline void pd_apad_i_imon(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(0));
}

static inline void pd_apad_i_dummy(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(1));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(1));
}

static inline void pd_apad_ie_imon(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(2));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(2));
}

static inline void pd_apad_ie_dummy(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(3));
}

static inline void pd_apad_e_imon(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(4));
}

static inline void pd_apad_e_dummy(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(5));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(5));
}

static inline void pd_apad_e2_imon(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(6));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(6));
}

static inline void pd_apad_e2_dummy(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(7));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(7));
}

static inline void pd_apad_o_imon_d2(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(16));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(16));
}

static inline void pd_apad_o_dummy_d2(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_APAD_CTRL1, BIT(17));
	else
		pd_clr_bits(m, OFF_PD_APAD_CTRL1, BIT(17));
}

/* ---------------- Helpers: RX_CTRL3 (PD_NEW_VCONN) ---------------- */
static inline void pd_rxctrl_rx_auto_cmp_en(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL3, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL3, BIT(0));
}

static inline void pd_rxctrl_rx_auto_cmp_value(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_RX_CTRL3, GENMASK(2, 1), FIELD_PREP(GENMASK(2, 1), val));
}

static inline void pd_rxctrl_rx_en_auto_clr_en(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL3, BIT(3));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL3, BIT(3));
}

static inline void pd_rxctrl_auto_set_rx_en(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_RX_CTRL3, BIT(12));
	else
		pd_clr_bits(m, OFF_PD_RX_CTRL3, BIT(12));
}

static inline u32 pd_rxctrl_auto_rx_en_state(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(15, 13), pd_readl(m, OFF_PD_RX_CTRL3));
}

static inline void pd_rxctrl_auto_set_rx_en_cnt(struct pd_regmap *m, u32 cnt)
{
	pd_rmw(m, OFF_PD_RX_CTRL3, GENMASK(31, 16), FIELD_PREP(GENMASK(31, 16), cnt));
}

/* ---------------- Helpers: DET_CTRL ---------------- */
static inline void pd_detctrl_cc1_det_ctrl_en(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_DET_CTRL, BIT(0));
	else
		pd_clr_bits(m, OFF_PD_DET_CTRL, BIT(0));
}

static inline void pd_detctrl_cc1_det_ctrl_sel(struct pd_regmap *m, u32 sel)
{
	pd_rmw(m, OFF_PD_DET_CTRL, GENMASK(2, 1), FIELD_PREP(GENMASK(2, 1), sel));
}

static inline void pd_detctrl_cc2_det_ctrl_en(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_DET_CTRL, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_DET_CTRL, BIT(4));
}

static inline void pd_detctrl_cc2_det_ctrl_sel(struct pd_regmap *m, u32 sel)
{
	pd_rmw(m, OFF_PD_DET_CTRL, GENMASK(6, 5), FIELD_PREP(GENMASK(6, 5), sel));
}

static inline u32 pd_detctrl_cc1_det_ctrl_data_get(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(19, 8), pd_readl(m, OFF_PD_DET_CTRL));
}

static inline u32 pd_detctrl_cc2_det_ctrl_data_get(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(31, 20), pd_readl(m, OFF_PD_DET_CTRL));
}

/* ---------------- Helpers: PREAMBLE_CTRL ---------------- */
static inline void pd_preamblectrl_preamble_sel(struct pd_regmap *m, u32 sel)
{
	pd_rmw(m, OFF_PD_PREAMBLE_CTRL, GENMASK(0, 0), FIELD_PREP(GENMASK(0, 0), sel));
}

static inline void pd_preamblectrl_preamble_end_sw(struct pd_regmap *m, u32 sel)
{
	pd_rmw(m, OFF_PD_PREAMBLE_CTRL, GENMASK(1, 1), FIELD_PREP(GENMASK(1, 1), sel));
}

static inline void pd_preamblectrl_preamble_cnt_timeout_en(struct pd_regmap *m, bool en)
{
	if (en)
		pd_set_bits(m, OFF_PD_PREAMBLE_CTRL, BIT(4));
	else
		pd_clr_bits(m, OFF_PD_PREAMBLE_CTRL, BIT(4));
}

static inline void pd_preamblectrl_lon_width_min(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_PREAMBLE_CTRL, GENMASK(15, 8), FIELD_PREP(GENMASK(15, 8), val));
}

static inline u32 pd_preamblectrl_long_cnt_get(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(17, 16), pd_readl(m, OFF_PD_PREAMBLE_CTRL));
}

static inline u32 pd_preamblectrl_short_cnt_get(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(19, 18), pd_readl(m, OFF_PD_PREAMBLE_CTRL));
}

static inline u32 pd_preamblectrl_rx_freq_fsm_2g_get(struct pd_regmap *m)
{
	return FIELD_GET(GENMASK(22, 20), pd_readl(m, OFF_PD_PREAMBLE_CTRL));
}

/* ---------------- Helpers: PREAMBLE_CTRL1 ---------------- */
static inline void pd_preamblectrl_preamble_timeout(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_PREAMBLE_CTRL1, GENMASK(7, 0), FIELD_PREP(GENMASK(7, 0), val));
}

static inline void pd_preamblectrl_preamble_min_start(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_PREAMBLE_CTRL1, GENMASK(15, 8), FIELD_PREP(GENMASK(15, 8), val));
}

static inline void pd_preamblectrl_preamble_max_end(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_PREAMBLE_CTRL1, GENMASK(23, 16), FIELD_PREP(GENMASK(23, 16), val));
}

/* ---------------- Helpers: BMC_PREAMBLE ---------------- */
static inline void pd_preamblectrl_bmc_preamble_timeout_cnt(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_BMC_PREAMBLE, GENMASK(11, 0), FIELD_PREP(GENMASK(11, 0), val));
}

/* ---------------- Helpers: ORDERSET ---------------- */
static inline void pd_orderset_00(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_ORDERSET_00, GENMASK(19, 0), FIELD_PREP(GENMASK(19, 0), val));
}

static inline void pd_orderset_l1(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_ORDERSET_L1, GENMASK(19, 0), FIELD_PREP(GENMASK(19, 0), val));
}

/* ---------------- Helpers: RX_ORDERSET_CTRL ---------------- */
static inline void pd_rx_orderset_worst_cnt(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_RX_ORDERSET_CTRL, GENMASK(4, 0), FIELD_PREP(GENMASK(4, 0), val));
}

static inline void pd_rx_orderset_best_cnt(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_RX_ORDERSET_CTRL, GENMASK(9, 5), FIELD_PREP(GENMASK(9, 5), val));
}

static inline void pd_rx_orderset_shift_en(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_RX_ORDERSET_CTRL, GENMASK(14, 10), FIELD_PREP(GENMASK(14, 10), val));
}

static inline void pd_rx_orderset_reset_type_timeout_bit(struct pd_regmap *m, u32 val)
{
	pd_rmw(m, OFF_PD_RX_ORDERSET_CTRL, GENMASK(17, 16), FIELD_PREP(GENMASK(17, 16), val));
}

/* For type c cc detect module */
/* Type C register offset (register base 0x98007220) */
#define USB_TYPEC_CTRL_CC1_0	0x0
#define USB_TYPEC_CTRL_CC1_1	0x4
#define USB_TYPEC_CTRL_CC2_0	0x8
#define USB_TYPEC_CTRL_CC2_1	0xC
#define USB_TYPEC_STS		0x10
#define USB_TYPEC_CTRL		0x14
#define USB_TYPEC_CTRL_1	0x518

#define ENABLE_CC1	0x1
#define ENABLE_CC2	0x2
#define DISABLE_CC	0x0

/* Bit mapping USB_TYPEC_CTRL_CC1_0 and USB_TYPEC_CTRL_CC2_0 */
#define PLR_EN		BIT(29)
#define EN_CC_RA	BIT(27)
#define CC_CODE_MASK	(0xfffff << 7)
#define rp4pk_code(val)	((0x1f & (val)) << 22)
#define code_rp4pk(val)	(((val) >> 22) & 0x1f)
#define rp36k_code(val)	((0x1f & (val)) << 17)
#define code_rp36k(val)	(((val) >> 17) & 0x1f)
#define rp12k_code(val)	((0x1f & (val)) << 12)
#define code_rp12k(val)	(((val) >> 12) & 0x1f)
#define rd_code(val)	((0x1f & (val)) << 7)
#define code_rd(val)	(((val) >> 7) & 0x1f)
#define dfp_mode(val)	((0x3 & (val)) << 5)
#define CC_MODE		(BIT(6)|BIT(5))
#define CC_PD_EN	BIT(5)
#define EN_RP4P7K	BIT(4)
#define EN_RP36K	BIT(3)
#define EN_RP12K	BIT(2)
#define EN_RD		BIT(1)
#define EN_CC_DET	BIT(0)

/*
 * cc_mode:
 * CC_MODE_UFP     rd     vref_ufp    : 1p23v,  0p66v, 0p2v
 * CC_MODE_DFP_USB rp36k  vref_dfp_usb: 0_1p6v, 0p2v,  unused
 * CC_MODE_DFP_1_5 rp12k  vref_dfp_1_5: 1_1p6v, 0p4v,  0p2v
 * CC_MODE_DFP_3_0 rp4p7k vref_dfp_3_0: 2p6v,   0p8v,  0p2v
 */
#define CC_MODE_UFP	0x0
#define CC_MODE_DFP_USB	0x1
#define CC_MODE_DFP_1_5	0x2
#define CC_MODE_DFP_3_0	0x3

/*
 * PARAMETER_V0:
 *  Realtek Kylin    rtd1295
 *  Realtek Hercules rtd1395
 *  Realtek Thor     rtd1619
 *  Realtek Hank     rtd1319
 *  Realtek Groot    rtd1312c
 * PARAMETER_V1:
 *  Realtek Stark    rtd1619b
 *  Realtek Parker   rtd1319d
 *  Realtek Danvers  rtd1315e
 */
enum parameter_version {
	PARAMETER_V0 = 0,
	PARAMETER_V1 = 1,
};

/* Bit mapping USB_TYPEC_CTRL_CC1_1 and USB_TYPEC_CTRL_CC2_1 */
#define V0_vref_2p6v(val)	((0xf & (val)) << 26) /* Bit 29 for groot */
#define V0_vref_1p23v(val)	((0xf & (val)) << 22)
#define V0_vref_0p8v(val)	((0xf & (val)) << 18)
#define V0_vref_0p66v(val)	((0xf & (val)) << 14)
#define V0_vref_0p4v(val)	((0x7 & (val)) << 11)
#define V0_vref_0p2v(val)	((0x7 & (val)) << 8)
#define V0_vref_1_1p6v(val)	((0xf & (val)) << 4)
#define V0_vref_0_1p6v(val)	((0xf & (val)) << 0)

#define V0_decode_2p6v(val)	(((val) >> 26) & 0xf) /* Bit 29 for groot */
#define V0_decode_1p23v(val)	(((val) >> 22) & 0xf)
#define V0_decode_0p8v(val)	(((val) >> 18) & 0xf)
#define V0_decode_0p66v(val)	(((val) >> 14) & 0xf)
#define V0_decode_0p4v(val)	(((val) >> 11) & 0x7)
#define V0_decode_0p2v(val)	(((val) >> 8) & 0x7)
#define V0_decode_1_1p6v(val)	(((val) >> 4) & 0xf)
#define V0_decode_0_1p6v(val)	(((val) >> 0) & 0xf)

/* new Bit mapping USB_TYPEC_CTRL_CC1_1 and USB_TYPEC_CTRL_CC2_1 */
#define V1_vref_2p6v(val)	((0xf & (val)) << 28)
#define V1_vref_1p23v(val)	((0xf & (val)) << 24)
#define V1_vref_0p8v(val)	((0xf & (val)) << 20)
#define V1_vref_0p66v(val)	((0xf & (val)) << 16)
#define V1_vref_0p4v(val)	((0xf & (val)) << 12)
#define V1_vref_0p2v(val)	((0xf & (val)) << 8)
#define V1_vref_1_1p6v(val)	((0xf & (val)) << 4)
#define V1_vref_0_1p6v(val)	((0xf & (val)) << 0)

#define V1_decode_2p6v(val)	(((val) >> 28) & 0xf)
#define V1_decode_1p23v(val)	(((val) >> 24) & 0xf)
#define V1_decode_0p8v(val)	(((val) >> 20) & 0xf)
#define V1_decode_0p66v(val)	(((val) >> 16) & 0xf)
#define V1_decode_0p4v(val)	(((val) >> 12) & 0xf)
#define V1_decode_0p2v(val)	(((val) >> 8) & 0xf)
#define V1_decode_1_1p6v(val)	(((val) >> 4) & 0xf)
#define V1_decode_0_1p6v(val)	(((val) >> 0) & 0xf)

/* CC detection status definitions */
#define CC_DET_STS_MASK		0x7
#define CC1_DET_STS_SHIFT	0
#define CC2_DET_STS_SHIFT	3

/* Bit mapping USB_TYPEC_STS */
#define DET_STS		0x7
#define CC1_DET_STS	(DET_STS)
#define CC2_DET_STS	(DET_STS << 3)
#define DET_STS_RA	0x1
#define DET_STS_RD	0x3
#define DET_STS_RP	0x1
#define DET_STS_RP15	0x3
#define DET_STS_RP30	0x7
#define CC1_DET_STS_RA	(DET_STS_RA)
#define CC1_DET_STS_RD	(DET_STS_RD)
#define CC1_DET_STS_RP	(DET_STS_RP)
#define CC2_DET_STS_RA	(DET_STS_RA << 3)
#define CC2_DET_STS_RD	(DET_STS_RD << 3)
#define CC2_DET_STS_RP	(DET_STS_RP << 3)

/* Bit mapping USB_TYPEC_CTRL */
#define DVDD_PWRCUT		BIT(27)
#define PROB_MAIN_ISO		BIT(26)
#define DET_INT_MASK		BIT(24)
#define CC2_RA_CODE_MASK	(0xf<<20)
#define CC2_VREF_BIAS 		(0x3<<18)
#define CC1_RA_CODE_MASK	(0xf<<14)
#define CC1_VREF_BIAS 		(0x3<<12)
#define CC2_INT_EN		BIT(11)
#define CC1_INT_EN		BIT(10)
#define CC2_INT_STS		BIT(9)
#define CC1_INT_STS		BIT(8)
#define DEBOUNCE_TIME_MASK	0xff
#define DEBOUNCE_EN		BIT(0)
#define ENABLE_TYPE_C_DETECT	(CC1_INT_EN | CC2_INT_EN)
#define ALL_CC_INT_STS		(CC1_INT_STS | CC2_INT_STS)

/* Bit mapping USB_TYPEC_CTRL_1 */
#define TYPEC_CTRL_SEL		BIT(31)
#define EN_CC2_OVP		BIT(29)
#define REF_SEL1_CC2		(BIT(27)|BIT(28))
#define REF_SEL0_CC2		(BIT(25)|BIT(26))
#define PLR_EN_CC2		BIT(24)
#define EN_CC1_OVP		BIT(21)
#define REF_SEL1_CC1		(BIT(19)|BIT(20))
#define REF_SEL0_CC1		(BIT(17)|BIT(18))
#define PLR_EN_CC1		BIT(16)
#define EN_CC2_RA		BIT(15)
#define CC2_MODE		(BIT(13)|BIT(14))
#define CC2_PD_EN		BIT(13)
#define EN_CC2_RP4P7K		BIT(12)
#define EN_CC2_RP36K		BIT(11)
#define EN_CC2_RP12K		BIT(10)
#define EN_CC2_RD		BIT(9)
#define EN_CC2_DET		BIT(8)
#define EN_CC1_RA		BIT(7)
#define CC1_MODE		(BIT(5)|BIT(6))
#define CC1_PD_EN		BIT(5)
#define EN_CC1_RP4P7K		BIT(4)
#define EN_CC1_RP36K		BIT(3)
#define EN_CC1_RP12K		BIT(2)
#define EN_CC1_RD		BIT(1)
#define EN_CC1_DET		BIT(0)

static inline u32 typec_readl(struct pd_regmap *m, u32 off)
{
	return readl(m->typec_base + off);
}

static inline void typec_writel(struct pd_regmap *m, u32 val, u32 off)
{
	writel(val, m->typec_base + off);
}

static inline void typec_int_debounce_set(struct pd_regmap *m, u32 debounce)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL);
	val = (~DEBOUNCE_TIME_MASK & val) | (debounce & DEBOUNCE_TIME_MASK);
	typec_writel(m, val, USB_TYPEC_CTRL);
}

static inline u32 typec_int_sts_get(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL);
	val &= ALL_CC_INT_STS;

	return val;
}

static inline void typec_int_sts_clear(struct pd_regmap *m, u32 mask)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL);
	val &= ~ALL_CC_INT_STS;
	typec_writel(m, val, USB_TYPEC_CTRL);
}

static inline void typec_int_disable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL);
	val &= ~(ALL_CC_INT_STS | ENABLE_TYPE_C_DETECT);
	typec_writel(m, val, USB_TYPEC_CTRL);
}

static inline void typec_int_enable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL);
	val &= ~ALL_CC_INT_STS;
	val |= ENABLE_TYPE_C_DETECT;
	typec_writel(m, val, USB_TYPEC_CTRL);
}

static inline u32 typec_ctrl_get(struct pd_regmap *m)
{
	return typec_readl(m, USB_TYPEC_CTRL);
}

static inline u32 typec_cc_sts_get(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_STS);
	val &= CC1_DET_STS | CC2_DET_STS;

	return val;
}

static inline void typec_cc1_det_enable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC1_0);
	val |= EN_CC_DET;
	typec_writel(m, val, USB_TYPEC_CTRL_CC1_0);
}

static inline void typec_cc1_det_disable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC1_0);
	val &= ~EN_CC_DET;
	typec_writel(m, val, USB_TYPEC_CTRL_CC1_0);
}

static inline void typec_cc1_code_set(struct pd_regmap *m, u32 mask)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC1_0);
	val = (val & ~CC_CODE_MASK) | (mask & CC_CODE_MASK);
	typec_writel(m, val, USB_TYPEC_CTRL_CC1_0);
}

static inline void typec_cc1_mode_set(struct pd_regmap *m, u32 cc_mode)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC1_0);
	val = (val & ~(dfp_mode(0x3) | EN_RP4P7K | EN_RP36K | EN_RP12K | EN_RD));
	val |= cc_mode;
	typec_writel(m, val, USB_TYPEC_CTRL_CC1_0);
}

static inline void typec_cc1_pd_enable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC1_0);
	val = (val & ~CC_MODE);
	val |= CC_PD_EN;
	typec_writel(m, val, USB_TYPEC_CTRL_CC1_0);
}

static inline void typec_cc1_pd_disable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC1_0);
	val = (val & ~CC_MODE);
	typec_writel(m, val, USB_TYPEC_CTRL_CC1_0);
}

static inline void typec_cc1_plr_enable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC1_0);
	val = (val | PLR_EN);
	typec_writel(m, val, USB_TYPEC_CTRL_CC1_0);
}

static inline void typec_cc1_plr_disable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC1_0);
	val = (val & ~PLR_EN);
	typec_writel(m, val, USB_TYPEC_CTRL_CC1_0);
}

static inline u32 typec_cc1_ctrl_get(struct pd_regmap *m)
{
	return typec_readl(m, USB_TYPEC_CTRL_CC1_0);
}

static inline void typec_cc1_vref_set(struct pd_regmap *m, u32 mask)
{
	typec_writel(m, mask, USB_TYPEC_CTRL_CC1_1);
}

static inline u32 typec_cc1_vref_get(struct pd_regmap *m)
{
	return typec_readl(m, USB_TYPEC_CTRL_CC1_1);
}

static inline void typec_cc2_det_enable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC2_0);
	val |= EN_CC_DET;
	typec_writel(m, val, USB_TYPEC_CTRL_CC2_0);
}

static inline void typec_cc2_det_disable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC2_0);
	val &= ~EN_CC_DET;
	typec_writel(m, val, USB_TYPEC_CTRL_CC2_0);
}

static inline void typec_cc2_code_set(struct pd_regmap *m, u32 mask)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC2_0);
	val = (val & ~CC_CODE_MASK) | (mask & CC_CODE_MASK);
	typec_writel(m, val, USB_TYPEC_CTRL_CC2_0);
}

static inline void typec_cc2_mode_set(struct pd_regmap *m, u32 cc_mode)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC2_0);
	val = (val & ~(dfp_mode(0x3) | EN_RP4P7K | EN_RP36K | EN_RP12K | EN_RD));
	val |= cc_mode;
	typec_writel(m, val, USB_TYPEC_CTRL_CC2_0);
}

static inline void typec_cc2_pd_enable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC2_0);
	val = (val & ~CC_MODE);
	val |= CC_PD_EN;
	typec_writel(m, val, USB_TYPEC_CTRL_CC2_0);
}

static inline void typec_cc2_pd_disable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC2_0);
	val = (val & ~CC_MODE);
	typec_writel(m, val, USB_TYPEC_CTRL_CC2_0);
}

static inline void typec_cc2_plr_enable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC2_0);
	val = (val | PLR_EN);
	typec_writel(m, val, USB_TYPEC_CTRL_CC2_0);
}

static inline void typec_cc2_plr_disable(struct pd_regmap *m)
{
	u32 val;

	val = typec_readl(m, USB_TYPEC_CTRL_CC2_0);
	val = (val & ~PLR_EN);
	typec_writel(m, val, USB_TYPEC_CTRL_CC2_0);
}

static inline u32 typec_cc2_ctrl_get(struct pd_regmap *m)
{
	return typec_readl(m, USB_TYPEC_CTRL_CC2_0);
}

static inline void typec_cc2_vref_set(struct pd_regmap *m, u32 mask)
{
	typec_writel(m, mask, USB_TYPEC_CTRL_CC2_1);
}

static inline u32 typec_cc2_vref_get(struct pd_regmap *m)
{
	return typec_readl(m, USB_TYPEC_CTRL_CC2_1);
}

/* TYPEC_CTRL_1 */
static inline void typec_ctrl1_enable(struct pd_regmap *m)
{
	u32 val;

	m->use_typec_ctrl1 = true;

	val = typec_readl(m, USB_TYPEC_CTRL_1);
	val |= TYPEC_CTRL_SEL;
	typec_writel(m, val, USB_TYPEC_CTRL_1);
}

static inline void typec_ctrl1_disalbe(struct pd_regmap *m)
{
	u32 val;

	m->use_typec_ctrl1 = false;

	val = typec_readl(m, USB_TYPEC_CTRL_1);
	val &= ~TYPEC_CTRL_SEL;
	typec_writel(m, val, USB_TYPEC_CTRL_1);
}

static inline void typec_cc_det_enable(struct pd_regmap *m)
{
	if (!m->use_typec_ctrl1) {
		typec_cc1_det_enable(m);
		typec_cc2_det_enable(m);
	} else {
		u32 val;

		val = typec_readl(m, USB_TYPEC_CTRL_1);
		val |= EN_CC1_DET | EN_CC2_DET;
		typec_writel(m, val | TYPEC_CTRL_SEL, USB_TYPEC_CTRL_1);
	}
}

static inline void typec_cc_det_disable(struct pd_regmap *m)
{
	if (!m->use_typec_ctrl1) {
		typec_cc1_det_disable(m);
		typec_cc2_det_disable(m);
	} else {
		u32 val;

		val = typec_readl(m, USB_TYPEC_CTRL_1);
		val &= ~(EN_CC1_DET | EN_CC2_DET);
		typec_writel(m, val | TYPEC_CTRL_SEL, USB_TYPEC_CTRL_1);
	}
}

/*
 * cc_mode:
 * 00=sink
 * 01=defautl usb
 * 10=5V1.5A
 * 11=5V3A
 */
static inline void typec_cc_mode_set(struct pd_regmap *m, u32 cc_mode)
{
	if (!m->use_typec_ctrl1) {
		typec_cc1_mode_set(m, cc_mode);
		typec_cc2_mode_set(m, cc_mode);
	} else {
		u32 val, cc_mode_2;

		cc_mode_2 = (cc_mode >> 5) & 0x3;

		val = typec_readl(m, USB_TYPEC_CTRL_1);
		val &= ~(EN_CC1_RP4P7K | EN_CC1_RP36K | EN_CC1_RP12K | EN_CC1_RD);
		val &= ~(REF_SEL0_CC1 | REF_SEL1_CC1);
		val &= ~(EN_CC2_RP4P7K | EN_CC2_RP36K | EN_CC2_RP12K | EN_CC2_RD);
		val &= ~(REF_SEL0_CC2 | REF_SEL1_CC2);

		cc_mode &= (EN_RP4P7K | EN_RP36K | EN_RP12K | EN_RD);
		val |= (cc_mode | (cc_mode << 8));
		val |= ((cc_mode_2 << 17) | (cc_mode_2 << 19));
		val |= ((cc_mode_2 << 25) | (cc_mode_2 << 27));

		typec_writel(m, val | TYPEC_CTRL_SEL, USB_TYPEC_CTRL_1);
	}
}

static inline void typec_cc_pd_enable_cc1(struct pd_regmap *m)
{
	if (!m->use_typec_ctrl1) {
		typec_cc1_pd_enable(m);
	} else {
		u32 val;

		val = typec_readl(m, USB_TYPEC_CTRL_1);
		val &= ~(CC1_MODE | CC2_MODE);
		val |= CC1_PD_EN;
		typec_writel(m, val | TYPEC_CTRL_SEL, USB_TYPEC_CTRL_1);
	}
}

static inline void typec_cc_pd_enable_cc2(struct pd_regmap *m)
{
	if (!m->use_typec_ctrl1) {
		typec_cc2_pd_enable(m);
	} else {
		u32 val;

		val = typec_readl(m, USB_TYPEC_CTRL_1);
		val &= ~(CC1_MODE | CC2_MODE);
		val |= CC2_PD_EN;
		typec_writel(m, val | TYPEC_CTRL_SEL, USB_TYPEC_CTRL_1);
	}
}

static inline void typec_cc_pd_disable(struct pd_regmap *m)
{
	if (!m->use_typec_ctrl1) {
		typec_cc1_pd_disable(m);
		typec_cc2_pd_disable(m);
	} else {
		u32 val;

		val = typec_readl(m, USB_TYPEC_CTRL_1);
		val &= ~(CC1_MODE | CC2_MODE);
		typec_writel(m, val | TYPEC_CTRL_SEL, USB_TYPEC_CTRL_1);
	}
}

static inline void typec_cc_plr_enable(struct pd_regmap *m)
{
	if (!m->use_typec_ctrl1) {
		typec_cc1_plr_enable(m);
		typec_cc2_plr_enable(m);
	} else {
		u32 val;

		val = typec_readl(m, USB_TYPEC_CTRL_1);
		val |= (PLR_EN_CC1 | PLR_EN_CC2);
		typec_writel(m, val | TYPEC_CTRL_SEL, USB_TYPEC_CTRL_1);
	}
}

static inline void typec_cc_plr_disable(struct pd_regmap *m)
{
	if (!m->use_typec_ctrl1) {
		typec_cc1_plr_disable(m);
		typec_cc2_plr_disable(m);
	} else {
		u32 val;

		val = typec_readl(m, USB_TYPEC_CTRL_1);
		val &= ~(PLR_EN_CC1 | PLR_EN_CC2);
		typec_writel(m, val | TYPEC_CTRL_SEL, USB_TYPEC_CTRL_1);
	}
}

static inline u32 typec_ctrl1_get(struct pd_regmap *m)
{
	return typec_readl(m, USB_TYPEC_CTRL_1);
}

#endif /* __RTK_RTD_PD_H */
