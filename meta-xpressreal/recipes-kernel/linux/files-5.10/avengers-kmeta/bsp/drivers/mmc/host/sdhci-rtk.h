/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Realtek SDIO host driver
 *
 * Copyright (c) 2017-2020 Realtek Semiconductor Corp.
 */

#ifndef _DRIVERS_MMC_SDHCI_OF_RTK_H
#define _DRIVERS_MMC_SDHCI_OF_RTK_H

#define MAX_PHASE				32
#define TUNING_CNT				3
#define MINIMUM_CONTINUE_LENGTH			16
/* Controller clock has large jitter when SSC enable */
#define RTKQUIRK_SSC_CLK_JITTER			BIT(0)

#define SDIO_LOC_0				0
#define SDIO_LOC_1				1

/* CRT register */
#define SYS_PLL_SDIO1				0x1A0
#define  BIAS_EN				BIT(0)
#define  PHRT0					BIT(1)
#define  PHSEL0_MASK				GENMASK(7, 3)
#define  PHSEL0_SHIFT				3
#define  PHSEL1_MASK				GENMASK(12, 8)
#define  PHSEL1_SHIFT				8
#define SYS_PLL_SDIO2				0x1A4
#define  SSCLDO_EN				BIT(0)
#define  REG_TUNE11				GENMASK(2, 1)
#define  REG_TUNE11_1V9				0x2
#define  SSCPLL_CS1				GENMASK(4, 3)
#define  SSCPLL_CS1_INIT_VALUE			0x1
#define  SSCPLL_ICP				GENMASK(9, 5)
#define  SSCPLL_ICP_10U				0x01
#define  SSCPLL_ICP_20U				0x03
#define  SSCPLL_RS				GENMASK(12, 10)
#define  SSCPLL_RS_10K				0x4
#define  SSCPLL_RS_13K				0x5
#define  SSC_DEPTH				GENMASK(15, 13)
#define  SSC_DEPTH_1_N				0x3
#define  SSC_8X_EN				BIT(16)
#define  SSC_DIV_EXT_F				GENMASK(25, 18)
#define  SSC_DIV_EXT_F_50M			0x71
#define  SSC_DIV_EXT_F_100M			0xE3
#define  SSC_DIV_EXT_F_200M			0x0
#define  SSC_DIV_EXT_F_208M			0xE3
#define  EN_CPNEW				BIT(26)
#define  PLL_V2_PI_IBSELH			GENMASK(2, 1)
#define  PLL_V2_PI_IBSELH_50_80M		0x0
#define  PLL_V2_PI_IBSELH_80_150M		0x1
#define  PLL_V2_PI_IBSELH_150_255M		0x2
#define  PLL_V2_SSC_PLL_ICP			GENMASK(9, 5)
#define  PLL_V2_SSC_PLL_ICP_50M		0x00
#define  PLL_V2_SSC_PLL_ICP_INIT_VALUE	0x01
#define  PLL_V2_SSC_PLL_RS			GENMASK(12, 10)
#define  PLL_V2_SSC_PLL_RS_4K			0x0
#define  PLL_V2_SSC_PLL_RS_6K			0x2
#define  PLL_V2_SSC_PLL_RS_8K			0x3
#define  PLL_V2_SSC_FLAG_INIT			BIT(13)
#define  PLL_V2_SSC_OC_EN			BIT(14)
#define  PLL_V2_SSC_DIV_EXT_F			GENMASK(28, 16)
#define  PLL_V2_SSC_DIV_EXT_F_50M		0x0D09
#define  PLL_V2_SSC_DIV_EXT_F_100M		0x1A12
#define  PLL_V2_SSC_DIV_EXT_F_200M		0x1425
#define  PLL_V2_SSC_DIV_EXT_F_208M		0x1A12
#define SYS_PLL_SDIO3				0x1A8
#define  SSC_TBASE				GENMASK(7, 0)
#define  SSC_TBASE_INIT_VALUE			0x88
#define  SSC_STEP_IN				GENMASK(14, 8)
#define  SSC_STEP_IN_INIT_VALUE			0x43
#define  SSC_DIV_N				GENMASK(25, 16)
#define  SSC_DIV_N_50M				0x28
#define  SSC_DIV_N_100M				0x56
#define  SSC_DIV_N_200M				0xAE
#define  SSC_DIV_N_208M				0xB6
#define  PLL_V2_SSC_DIV_N			GENMASK(7, 0)
#define  PLL_V2_SSC_DIV_N_50M			0x05
#define  PLL_V2_SSC_DIV_N_100M			0x0C
#define  PLL_V2_SSC_DIV_N_200M			0x1B
#define  PLL_V2_SSC_DIV_N_208M			0x1C
#define SYS_PLL_SDIO4				0x1AC
#define  SSC_RSTB				BIT(0)
#define  SSC_PLL_RSTB				BIT(1)
#define  SSC_PLL_POW				BIT(2)
#define SYS_PLL_SD1				0x1E0
#define  REG_SEL3318_MASK			GENMASK(14, 13)
#define  REG_SEL3318_3V3			0x1
#define  REG_SEL3318_0V				0x2
#define  REG_SEL3318_1V8			0x3
/* ISO register */
#define ISO_RESERVED				0x64
#define ISO_CTRL_1				0xC4
#define  SDIO_SSCLDO_EN				BIT(4)
#define  SDIO_BIAS_EN				BIT(5)
#define ISO_USB_TYPEC_CTRL_CC1			0x220
#define  PLR_EN					BIT(29)
/* SDIO wrapper control register */
#define SDHCI_RTK_SSC1				0x04
#define  NCODE_SSC				GENMASK(7, 0)
#define  NCODE_SSC_100M				0x0C
#define  NCODE_SSC_200M				0x1B
#define  NCODE_SSC_INIT_VALUE			0x1C
#define  FCODE_SSC				GENMASK(20, 8)
#define  FCODE_SSC_INIT_VALUE			0x1A12
#define  FCODE_SSC_100M_1PERCENT		0x1555
#define  FCODE_SSC_200M_1PERCENT		0x0AAA
#define  FCODE_SSC_208M_1PERCENT		0x1036
#define  FCODE_SSC_208M_2PERCENT		0x65A
#define  DOT_GRAN				GENMASK(26, 24)
#define  DOT_GRAN_INIT_VALUE			0x4
#define  EN_SSC					BIT(27)
#define SDHCI_RTK_SSC2				0x08
#define  GRAN_SET				GENMASK(20, 0)
#define  GRAN_SET_100M_1PERCENT			0x17BD
#define  GRAN_SET_200M_1PERCENT			0x2F7A
#define  GRAN_SET_208M_1PERCENT			0x3160
#define  GRAN_SET_208M_2PERCENT			0x62C0
#define SDIO_RTK_CTL				0x10
#define  SUSPEND_N				BIT(0)
#define  L4_GATED_DISABLE			BIT(1)
#define SDIO_RTK_ISREN				0x34
#define  WRITE_DATA				BIT(0)
#define  INT4EN					BIT(4)
#define SDIO_RTK_DUMMY_SYS1			0x58
/* ISO SYS register */
#define ISO_SYS_POWER_CTRL			0x300
#define  ISO_PHY				BIT(21)
/* Testmux register */
#define ISO_PFUNC4				0x24
#define ISO_PFUNC5				0x28
#define ISO_PFUNC6				0x2C

#endif /* _DRIVERS_MMC_SDHCI_OF_RTK_H */
