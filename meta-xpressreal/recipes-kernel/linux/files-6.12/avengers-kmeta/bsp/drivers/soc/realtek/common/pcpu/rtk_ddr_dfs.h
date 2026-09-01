/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */
#ifndef __RTK_DDR_DFS_H__
#define __RTK_DDR_DFS_H__

#include <linux/types.h>


#define HWM_CFG_VER			1

#define DFS_SMCCC_VER			0x82000830
#define DFS_SMCCC_CFG			0x82000831
#define DFS_SMCCC_CTRL			0x82000832


struct platform_device;

struct rtk_hwm_cfg {
	u32		version;
	u32		time_us;		/* time window in us	*/
	u32		low_ack;		/* bandwidth in gran	*/
	u32		high_ack;		/* bandwidth in gran	*/
	u32		low_debounce;		/* low debounce		*/
	u32		high_debounce;		/* high debounce	*/
};

struct rtk_ddr_dfs_data {
	struct platform_device	*pdev;

	int			enabled;
	int			dirty;
	struct rtk_hwm_cfg	cfg;
};

#endif	/* __RTK_DDR_DFS_H__ */
