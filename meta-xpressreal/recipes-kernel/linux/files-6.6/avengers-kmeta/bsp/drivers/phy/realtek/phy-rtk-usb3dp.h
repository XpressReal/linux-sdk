/*
 *  Copyright (C) 2024 Realtek Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PHY_RTK_U3DP_H
#define __PHY_RTK_U3DP_H

#define USB_MDIO_CTRL_PHY_BUSY BIT(7)
#define USB_MDIO_CTRL_PHY_WRITE BIT(0)
#define USB_MDIO_CTRL_PHY_ADDR_SHIFT 8
#define USB_MDIO_CTRL_PHY_DATA_SHIFT 16
#define MAX_USB_PHY_DATA_SIZE 0x30
#define PHY_ADDR_0X09 0x09
#define PHY_ADDR_0X0B 0x0b
#define PHY_ADDR_0X0D 0x0d
#define PHY_ADDR_0X10 0x10
#define PHY_ADDR_0X1F 0x1f
#define PHY_ADDR_0X20 0x20
#define PHY_ADDR_0X21 0x21
#define PHY_ADDR_0X30 0x30
#define REG_0X09_FORCE_CALIBRATION BIT(9)
#define REG_0X0B_RX_OFFSET_RANGE_MASK 0xc
#define REG_0X0D_RX_DEBUG_TEST_EN BIT(6)
#define REG_0X10_DEBUG_MODE_SETTING 0x3c0
#define REG_0X10_DEBUG_MODE_SETTING_MASK 0x3f8
#define REG_0X1F_RX_OFFSET_CODE_MASK 0x1e
#define USB_U3_TX_LFPS_SWING_TRIM_SHIFT 4
#define USB_U3_TX_LFPS_SWING_TRIM_MASK 0xf
#define AMPLITUDE_CONTROL_COARSE_MASK 0xff
#define AMPLITUDE_CONTROL_FINE_MASK 0xffff
#define AMPLITUDE_CONTROL_COARSE_DEFAULT 0xff
#define AMPLITUDE_CONTROL_FINE_DEFAULT 0xffff
/* Type C port control */
#define disable_cc 0x0
#define enable_cc1 0x1
#define enable_cc2 0x2
#define REG_U3DP_004C                   0x4c /* 0x9814f04c */
#define REG_U3DP_004C_usb_OOBS_REGSIG_L1_mask  0x8
#define REG_U3DP_004C_usb_OOBS_REGSIG_L1(data) (0x8 & ((data) << 3))
#define REG_U3DP_004C_dp_OOBS_REGSIG_L1_mask  0x10
#define REG_U3DP_004C_dp_OOBS_REGSIG_L1(data) (0x10 & ((data) << 4))
#define REG_U3DP_004C_usb_OOBS_REGSIG_L2_mask  0x20
#define REG_U3DP_004C_usb_OOBS_REGSIG_L2(data) (0x20 & ((data) << 5))
#define REG_U3DP_004C_dp_OOBS_REGSIG_L2_mask  0x40
#define REG_U3DP_004C_dp_OOBS_REGSIG_L2(data) (0x40 & ((data) << 6))
#define REG_U3DP_004C_usb_OOBS_L1_mask	0x80
#define REG_U3DP_004C_usb_OOBS_L1(data)	(0x80 & ((data) << 7))
#define REG_U3DP_004C_dp_OOBS_L1_mask  0x100
#define REG_U3DP_004C_dp_OOBS_L1(data) (0x100 & ((data) << 8))
#define REG_U3DP_004C_usb_OOBS_L2_mask  0x200
#define REG_U3DP_004C_usb_OOBS_L2(data) (0x200 & ((data) << 9))
#define REG_U3DP_004C_dp_OOBS_L2_mask  0x400
#define REG_U3DP_004C_dp_OOBS_L2(data) (0x400 & ((data) << 10))
#define POWER_CUT_EN0                   0x5c /* 0x9814f05c */
#define USB_AVDD09_TRX_CK_L2            (BIT(29) | BIT(30) | BIT(31))
#define DP_AVDD09_TRX_CK_L1             (BIT(26) | BIT(27) | BIT(28))
#define USB_AVDD09_TRX_CK_L1            (BIT(23) | BIT(24) | BIT(25))
#define DP_AVDD09_RX_OOBS_IN_L2         BIT(22)
#define USB_AVDD09_RX_OOBS_IN_L2        BIT(21)
#define DP_AVDD09_RX_OOBS_IN_L1         BIT(20)
#define USB_AVDD09_RX_OOBS_IN_L1        BIT(19)
#define DP_AVDD09_DPCMU_CK_CUT          BIT(18)
#define USB_AVDD09_DPCMU_CK_CUT         BIT(17)
#define DP_AVDD09_CMU_CK                BIT(16)
#define USB_AVDD09_CMU_CK               BIT(15)
#define POWER_CUT_EN1                   0x60 /* 0x9814f060 */
#define DP_AVDD09_TX_CK_L3              (BIT(28) | BIT(29) | BIT(30))
#define USB_AVDD09_TX_CK_L3             (BIT(25) | BIT(26) | BIT(27))
#define DP_AVDD09_TX_CK_L0              (BIT(22) | BIT(23) | BIT(24))
#define USB_AVDD09_TX_CK_L0             (BIT(19) | BIT(20) | BIT(21))
#define DP_AVDD09_TRX_RX_L2             BIT(18)
#define USB_AVDD09_TRX_RX_L2            BIT(17)
#define DP_AVDD09_TRX_RX_L1             BIT(16)
#define USB_AVDD09_TRX_RX_L1            BIT(15)
#define DP_AVDD09_TRX_L2                (BIT(12) | BIT(13) | BIT(14))
#define USB_AVDD09_TRX_L2               (BIT(9) | BIT(10) | BIT(11))
#define DP_AVDD09_TRX_L1                (BIT(6) | BIT(7) | BIT(8))
#define USB_AVDD09_TRX_L1               (BIT(3) | BIT(4) | BIT(5))
#define DP_AVDD09_TRX_CK_L2             (BIT(0) | BIT(1) | BIT(2))
#define POWER_CUT_EN2                   0x64 /* 0x9814f064 */
#define USB_AVDD18_TRX_L2               (BIT(28) | BIT(29) | BIT(30))
#define DP_AVDD18_TRX_L1                (BIT(25) | BIT(26) | BIT(27))
#define USB_AVDD18_TRX_L1               (BIT(22) | BIT(23) | BIT(24))
#define DP_AVDD18_RX_OOBS_IN_L2         BIT(21)
#define USB_AVDD18_RX_OOBS_IN_L2        BIT(20)
#define DP_AVDD18_RX_OOBS_IN_L1         BIT(19)
#define USB_AVDD18_RX_OOBS_IN_L1        BIT(18)
#define DP_AVDD18_DPCMU_CUT             BIT(17)
#define USB_AVDD18_DPCMU_CUT            BIT(16)
#define DP_AVDD18_CMU_RX_L2             BIT(15)
#define USB_AVDD18_CMU_RX_L2            BIT(14)
#define DP_AVDD18_CMU_RX_L1             BIT(13)
#define USB_AVDD18_CMU_RX_L1            BIT(12)
#define DP_AVDD09_TX_L3                 (BIT(9) | BIT(10) | BIT(11))
#define USB_AVDD09_TX_L3                (BIT(6) | BIT(7) | BIT(8))
#define DP_AVDD09_TX_L0                 (BIT(3) | BIT(4) | BIT(5))
#define USB_AVDD09_TX_L0                (BIT(0) | BIT(1) | BIT(2))
#define POWER_CUT_EN3                   0x68 /* 0x9814f068 */
#define POWERCUT_EN_USB_L2		(BIT(28) | BIT(29) | BIT(30))
#define POWERCUT_EN_DP_L1		(BIT(25) | BIT(26) | BIT(27))
#define POWERCUT_EN_USB_L1		(BIT(22) | BIT(23) | BIT(24))
#define POWERCUT_EN_DP_L0		(BIT(19) | BIT(20) | BIT(21))
#define POWERCUT_EN_USB_L0		(BIT(16) | BIT(17) | BIT(18))
#define DP_AVDD18_TX_L3                 (BIT(12) | BIT(13) | BIT(14))
#define USB_AVDD18_TX_L3                (BIT(9) | BIT(10) | BIT(11))
#define DP_AVDD18_TX_L0                 (BIT(6) | BIT(7) | BIT(8))
#define USB_AVDD18_TX_L0                (BIT(3) | BIT(4) | BIT(5))
#define DP_AVDD18_TRX_L2                (BIT(0) | BIT(1) | BIT(2))
#define POWER_CUT_EN4			0x6c /* 0x9814f06C */
#define POWERCUT_EN_DP_L3		(BIT(6) | BIT(7) | BIT(8))
#define POWERCUT_EN_USB_L3		(BIT(3) | BIT(4) | BIT(5))
#define POWERCUT_EN_DP_L2		(BIT(0) | BIT(1) | BIT(2))
#define REG_U3DP_0084                     0x84  //0x9814f084
#define REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L1_mask	(0x00000010)
#define REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L1(data)	(0x00000010 &((data) << 4))
#define REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L1_mask	(0x00000020)
#define REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L1(data)      (0x00000020 &((data) << 5))
#define REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L2_mask	(0x00000040)
#define REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L2(data)     (0x00000040 &((data) << 6))
#define REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L2_mask	(0x00000080)
#define REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L2(data)     (0x00000080 &((data) << 7))
#define REG_U3DP_0088                     0x88 //0x9814f088
#define REG_U3DP_0090                     0x90 //0x9814f090
#define REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L1_mask	(0x00100000)
#define REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L1(data)	(0x00100000 &((data) << 20))
#define REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L1_mask	(0x00200000)
#define REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L1(data)	(0x00200000 &((data) << 21))
#define REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L2_mask	(0x00400000)
#define REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L2(data)    (0x00400000 &((data) << 22))
#define REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L2_mask	(0x00800000)
#define REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L2(data)     (0x00800000 &((data) << 23))
#define RX_L1_DPHY1			0x400 /* 0x9814f400 */
#define TOP_AIF_11C			0x11C /* 0x9814f11C */
#define TOP_AIF_0A0			0x0A0 /* 0x9814f0A0 */
#define TOP_AIF_0FC			0x0FC /* 0x9814f0FC */
#define CMU_USB_DPHY5			0x810 /* 0x9814f810 */
#define CMU_USB_DPHY6			0x84C /* 0x9814f84c */
#define PCS_USB31_DP14_DPHY1            0xe00 /* 0x9814fe00 */
#define DP_LAN0_1_2_3_SELECT            0x0
#define USB_LAN0_1_SELECT		0x1
#define USB_LAN2_3_SELECT               0x2
#define PCS_USB31_DP14_DPHY2            0xE04 /* 0x9814fe04 */
#define SPD_CTRL3_MASK			(BIT(25) | BIT(26) | BIT(27))
#define SPD_CTRL2_MASK                  (BIT(22) | BIT(23) | BIT(24))
#define SPD_CTRL1_MASK                  (BIT(19) | BIT(20) | BIT(21))
#define SPD_CTRL0_MASK                  (BIT(16) | BIT(17) | BIT(18))
#define RST_DLY_MASK			(BIT(7) | BIT(8))
#define SPD_CTRL3(data)			(SPD_CTRL3_MASK & ((data) << 25))
#define SPD_CTRL2(data)			(SPD_CTRL2_MASK & ((data) << 22))
#define SPD_CTRL1(data)			(SPD_CTRL1_MASK & ((data) << 19))
#define SPD_CTRL0(data)			(SPD_CTRL0_MASK & ((data) << 16))
#define RST_DLY(data)			(RST_DLY_MASK & ((data) << 7))
#define PCS_USB31_DP14_DPHY3            0xE08 /* 0x9814fe08 */
#define DP_TX_EN_L3(data)		(0x80000 & ((data) << 19))
#define DP_TX_EN_L2(data)		(0x40000 & ((data) << 18))
#define DP_TX_EN_L1(data)		(0x20000 & ((data) << 17))
#define DP_TX_EN_L0(data)		(0x10000 & ((data) << 16))
#define PCS_USB31_DP14_DPHY4            0xE0C /* 0x9814fe0C */
#define U3DP_PHY_PCS_USB31_DP14_DPHY4_reg_dpll_rstb(data)   (0x2&((data)<<1))
#define U3DP_PHY_PCS_USB31_DP14_DPHY4_reg_dpll_pow(data)   (0x1&((data)<<0))
//0x9814f024
#define REG_U3DP_0012_usb_REG_DPCMU_PLL_LDO_POW_mask      (0x00000004)
#define REG_U3DP_0012_usb_REG_DPCMU_PLL_LDO_POW(data)     (0x4&((data)<<2))
#define REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_POW_mask       (0x00000008)
#define REG_U3DP_0012_dp_REG_DPCMU_PLL_LDO_POW(data)      (0x8&((data)<<3))
#define REG_U3DP_0012_usb_REG_DPCMU_PLL_POW_mask          (0x01000000)
#define REG_U3DP_0012_usb_REG_DPCMU_PLL_POW(data)         (0x1000000&((data)<<24))
#define REG_U3DP_0012_dp_REG_DPCMU_PLL_POW_mask           (0x02000000)
#define REG_U3DP_0012_dp_REG_DPCMU_PLL_POW(data)          (0x2000000&((data)<<25))
//0x9814f01c
#define REG_U3DP_000C_usb_REG_DPCMU_DISP_EXT_LD0LV_mask		(0x01000000)
#define REG_U3DP_000C_usb_REG_DPCMU_DISP_EXT_LD0LV(data)        (0x01000000 & ((data) << 24))
#define REG_U3DP_000C_dp_REG_DPCMU_DISP_EXT_LD0LV_mask		(0x02000000)
#define REG_U3DP_000C_dp_REG_DPCMU_DISP_EXT_LD0LV(data)		(0x02000000 & ((data) << 25))
#define REG_U3DP_000E_usb_REG_DPCMU_MBIAS_POW_mask	(0x00001000)
#define REG_U3DP_000E_usb_REG_DPCMU_MBIAS_POW(data)	(0x1000&((data)<<12))
#define REG_U3DP_000E_dp_REG_DPCMU_MBIAS_POW_mask	(0x00002000)
#define REG_U3DP_000E_dp_REG_DPCMU_MBIAS_POW(data)	(0x2000&((data)<<13))
#define PHY_LANE_MUX_USB			0
#define PHY_LANE_MUX_DP				1
#define PHY_ADDR_MAP_ARRAY_INDEX(addr) (addr)
#define ARRAY_INDEX_MAP_PHY_ADDR(index) (index)

#define POWERCUT_1_USB				0xE380E38
#define POWERCUT_2_USB				0x71C00000

struct phy_reg {
	void __iomem *reg_mdio_ctl;
};

struct phy_data {
	u8 addr;
	u16 data;
};

struct phy_cfg {
	int param_size;
	struct phy_data param[MAX_USB_PHY_DATA_SIZE];
	bool check_efuse;
	bool do_toggle;
	bool do_toggle_once;
	bool use_default_parameter;
	bool check_rx_front_end_offset;
};

struct phy_parameter {
	struct phy_reg phy_reg;
	/* Get from efuse */
	u8 efuse_usb_u3_tx_lfps_swing_trim;
	/* Get from dts */
	u32 amplitude_control_coarse;
	u32 amplitude_control_fine;
};

struct type_c_data {
	void __iomem *base;
	void __iomem *iso_base;
        /* node for type_c driver */
	struct device_node *node;
        /* for extcon type c connector */
	struct extcon_dev *edev;
	struct notifier_block edev_nb;
	u32 pre_lane;
	u32 state;
	bool is_enable;
	bool is_attach;
	bool ss; /* flag for super-speed */
};

struct rtk_phy {
	struct usb_phy phy;
	struct device *dev;
	struct phy_cfg *phy_cfg;
	int num_phy;
	struct phy_parameter *phy_parameter;
	struct dentry *debug_dir;
	struct type_c_data type_c;
	struct workqueue_struct *wq_typec_phy;
	struct delayed_work delayed_work;
	struct typec_switch_dev *sw;
	struct typec_mux_dev *mux;
	struct typec_mux_state state;
	struct mutex mutex; /* mutex to protect access to individual PHYs */
	u32 lane_mux_sel[4];
	int flip;
};

enum {
	DP_4,
	USB_NON_FLIP,	/*usb0,1*/
	USB_FLIP,	/*usb2,3*/
	USB_DP,
	DP_USB,
};

#endif
