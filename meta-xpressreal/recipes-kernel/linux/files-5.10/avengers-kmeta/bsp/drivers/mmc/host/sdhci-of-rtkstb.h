/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Realtek DHC SoC Secure Digital Host Controller Interface.
 *
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */
#ifndef _DRIVERS_MMC_SDHCI_OF_RTKSTB_H
#define _DRIVERS_MMC_SDHCI_OF_RTKSTB_H

#define MAX_PHASE				32
#define TUNING_CNT				3
#define MINIMUM_CONTINUE_LENGTH			16
#define PAD_PWR_CHK_CNT				3
/* Controller clock has large jitter when SSC enable */
#define RTKQUIRK_SSC_CLK_JITTER			BIT(0)
/* Controller needs to take care AXI burst boundary */
#define RTKQUIRK_AXI_BURST_BOUNDARY		BIT(1)
/* CRT register */
#define SYS_PLL_SD1				0x1E0
#define  PHRT0					BIT(1)
#define  PHSEL0_MASK				GENMASK(7, 3)
#define  PHSEL0_SHIFT				3
#define  PHSEL1_MASK				GENMASK(12, 8)
#define  PHSEL1_SHIFT				8
#define  REG_SEL3318_MASK			GENMASK(14, 13)
#define  REG_SEL3318_3V3			0x1
#define  REG_SEL3318_0V				0x2
#define  REG_SEL3318_1V8			0x3
#define SYS_PLL_SD2				0x1E4
#define  SSC_DEPTH				GENMASK(15, 13)
#define  SSC_DEPTH_1_N				0x3
#define  RTD13XXE_SSC_FLAG_INIT			BIT(13)
#define  RTD13XXE_SSC_OC_EN			BIT(14)
#define SYS_PLL_SD3				0x1E8
#define SYS_PLL_SD4				0x1EC
#define  SSC_RSTB				BIT(0)
#define  SSC_PLL_RSTB				BIT(1)
#define  SSC_PLL_POW				BIT(2)
/* ISO register */
#define ISO_CTRL_1				0xC4
#define  SD3_SSCLDO_EN				BIT(2)
#define  SD3_BIAS_EN				BIT(3)
/* ISO testmux register */
#define ISO_DBG_STATUS				0x190
#define  SDIO0_H3L1_STATUS_HI			BIT(1)
#define  SDIO0_H3L1_DETECT_EN			BIT(4)
/* wrapper control register */
#define SDHCI_RTK_SSC1				0x04
#define  NCODE_SSC				GENMASK(7, 0)
#define  NCODE_SSC_100M				0x0C
#define  NCODE_SSC_200M				0x1B
#define  NCODE_SSC_INIT_VALUE			0x1C
#define  FCODE_SSC				GENMASK(20, 8)
#define  FCODE_SSC_INIT_VALUE			0x1A12
#define  FCODE_SSC_100M_1PERCENT		0x1555
#define  FCODE_SSC_200M_1PERCENT		0x0147
#define  FCODE_SSC_208M_1PERCENT		0x1036
#define  FCODE_SSC_208M_2PERCENT		0x65A
#define  DOT_GRAN				GENMASK(26, 24)
#define  DOT_GRAN_INIT_VALUE			0x4
#define  EN_SSC					BIT(27)
#define SDHCI_RTK_SSC2				0x08
#define  GRAN_SET				GENMASK(20, 0)
#define  GRAN_SET_100M_1PERCENT			0x17BD
#define  GRAN_SET_200M_1PERCENT			0x2F00
#define  GRAN_SET_208M_1PERCENT			0x3160
#define  GRAN_SET_208M_2PERCENT			0x62C0
#define SDHCI_RTK_ISREN				0x34
#define  ISREN_WRITE_DATA			BIT(0)
#define  INT1_EN				BIT(1)
#define  INT3_EN				BIT(3)
#define  INT4_EN				BIT(4)
#define SDHCI_RTK_DUTY_CTRL			0x7C
#define  SDIO_STOP_CLK_EN			BIT(4)

#endif /* _DRIVERS_MMC_SDHCI_OF_RTKSTB_H */
