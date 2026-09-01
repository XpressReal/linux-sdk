/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2026 Realtek Semiconductor Corp.
 */
#ifndef __RTK_DDR_DYNIF_H__
#define __RTK_DDR_DYNIF_H__

#include <linux/types.h>


#define DPI_SMCCC_CFG			0x82000811
#define DPI_SMCCC_CTRL			0x82000812

#define REF_SMCCC_CFG			0x82000821
#define REF_SMCCC_CTRL			0x82000822


#define DDR_DYNIF_TX_DQ			BIT(0)
#define DDR_DYNIF_ZQ_CAL		BIT(1)
#define DDR_DYNIF_DQ_CAL		BIT(2)
#define DDR_DYNIF_ZQC			BIT(3)
#define DDR_DYNIF_REF_CHG		BIT(4)


struct platform_device;

struct tm_val {
	s32			val:19;
};

struct rtk_dpi_cfg {
	u32			interval_ms;		/* time interval in ms	*/
	struct tm_val		tm_delta;		/* thermal delta	*/
	u32			bind;
};

struct rtk_ref_cfg {
	u32			interval_ms;		/* interval in ms	*/
	u32			bind;
};

struct rtk_ddr_dynif_data {
	struct platform_device	*pdev;

	u32			dpi_enabled;
	u32			ref_enabled;
	char			dpi_dirty;
	char			ref_dirty;

	struct rtk_dpi_cfg	dpi;
	struct rtk_ref_cfg	ref;
};

#endif	/* __RTK_DDR_DYNIF_H__ */
