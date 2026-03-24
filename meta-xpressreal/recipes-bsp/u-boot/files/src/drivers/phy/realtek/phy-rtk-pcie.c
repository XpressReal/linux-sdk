// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek PCIe Controller PHY Driver
 *
 * Copyright (C) 2020 Realtek
 */

#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <generic-phy.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <regmap.h>
#include <syscon.h>

#define PCIE_MDIO_CTR 0xC1C
#define PCIE_MDIO_CTL1 0xDB8
#define PCIE_MDIO_PCTL1 0xF1C
#define PCIE_MAC_ST 0xCB4
#define PCIE_PHY_CTR 0xC68
#define LINK_CONTROL_LINK_STATUS_REG 0x80
#define MDIO_BUSY BIT(7)
#define MDIO_RDY BIT(4)
#define MDIO_SRST BIT(1)
#define MDIO_WRITE BIT(0)
#define MDIO_REG_SHIFT 8
#define MDIO_DATA_SHIFT 16
#define MCLK_RATE 0xc

struct rtd_pcie_phy {
	struct udevice *dev;
	struct regmap *pcie_base;
};

static void mdio_reset(struct rtd_pcie_phy *rtd_phy)
{
	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_CTR, MDIO_SRST | MCLK_RATE);
}

static int mdio_wait_busy(struct rtd_pcie_phy *rtd_phy)
{
	unsigned int val;
	int cnt = 0;

	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
	while ((val & MDIO_BUSY) && cnt < 10) {
		udelay(10);
		regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
		cnt++;
	}

	if (val & MDIO_BUSY)
		return -EBUSY;

	return 0;
}

static int write_mdio_reg(struct rtd_pcie_phy *rtd_phy, u8 reg, u16 data)
{
	unsigned int val;

	val = ((unsigned int)reg << MDIO_REG_SHIFT) |
			((unsigned int)data << MDIO_DATA_SHIFT) | MDIO_WRITE;
	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_CTR, val | MCLK_RATE);

	if (mdio_wait_busy(rtd_phy))
		goto mdio_busy;

	return 0;
mdio_busy:
	dev_err(rtd_phy->dev, "%s - mdio is busy\n", __func__);
	return -EBUSY;
}

static int read_mdio_reg(struct rtd_pcie_phy *rtd_phy, u8 reg)
{
	unsigned int addr;
	unsigned int val;

	addr = reg << MDIO_REG_SHIFT;
	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_CTR, addr | MCLK_RATE);

	if (mdio_wait_busy(rtd_phy))
		goto mdio_busy;

	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);

	return val >> MDIO_DATA_SHIFT;
mdio_busy:
	dev_err(rtd_phy->dev, "%s - mdio is busy\n", __func__);
	return -EBUSY;
}

static int __maybe_unused write_mdio_parallel_reg(struct rtd_pcie_phy *rtd_phy, u16 reg, u16 data)
{

	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_PCTL1, reg);

	return write_mdio_reg(rtd_phy, 0, data);
}

static int __maybe_unused read_mdio_parallel_reg(struct rtd_pcie_phy *rtd_phy, u16 reg)
{

	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_PCTL1, reg);

	return read_mdio_reg(rtd_phy, 0);
}

static void rtd1625_oobs_manual_write_back(struct rtd_pcie_phy *rtd_phy, u16 debug_off, u16 wb_offset,
				      u32 wb_bit_off, u32 mask_bits, u32 en_bit)
{
	int val;
	int tmp1;
	int tmp2;
	int tmp3;
	int mask_gap = mask_bits -1;

	write_mdio_parallel_reg(rtd_phy, 0x102e, debug_off);
	tmp1 = read_mdio_parallel_reg(rtd_phy, 0x1030);
	val = (tmp1 & GENMASK(8 + mask_gap, 8)) >> 8;
	tmp2 = read_mdio_parallel_reg(rtd_phy, wb_offset);
	tmp3 = (tmp2 & ~GENMASK(wb_bit_off + mask_gap, wb_bit_off)) | (val << wb_bit_off);
	tmp3 |= BIT(en_bit);
	write_mdio_parallel_reg(rtd_phy, wb_offset, tmp3);
}


static int rtd1625_pcie0_phy_offset_calibrate(struct rtd_pcie_phy *rtd_phy)
{
	int cnt;
	int val;
	int tmp;
	int ret;
	int offset_k_timeout = 10000;

	regmap_read(rtd_phy->pcie_base, PCIE_PHY_CTR, &val);
	val |= BIT(3);
	regmap_write(rtd_phy->pcie_base, PCIE_PHY_CTR, val);
	ret = regmap_read_poll_timeout(rtd_phy->pcie_base, PCIE_MAC_ST, tmp,
					       tmp & BIT(16), 100, 10000);
	if (ret) {
		dev_err(rtd_phy->dev, "wait pipe_clock timeout\n");
		return -EBUSY;
	}

	regmap_write(rtd_phy->pcie_base, PCIE_PHY_CTR, (val & ~GENMASK(7, 6)) | (0x1 << 6));

	/*Start*/
	write_mdio_parallel_reg(rtd_phy, 0x1a4a, 0xcffc);
	write_mdio_parallel_reg(rtd_phy, 0x1a48, 0x838f);
	write_mdio_parallel_reg(rtd_phy, 0x1990, 0x0554);
	write_mdio_parallel_reg(rtd_phy, 0x1a40, 0x4c05);

	/*Gen1*/
	write_mdio_parallel_reg(rtd_phy, 0x1a40, 0x4c04);
	write_mdio_parallel_reg(rtd_phy, 0x1c00, 0x125d);
	write_mdio_parallel_reg(rtd_phy, 0x1c00, 0x125f);
	write_mdio_parallel_reg(rtd_phy, 0x102e, 0x0010);

	cnt = 0;
	val = read_mdio_parallel_reg(rtd_phy, 0x1030);
	while (!(val & BIT(15)) && cnt < offset_k_timeout) {
		udelay(10);
		val = read_mdio_parallel_reg(rtd_phy, 0x1030);
		cnt++;
	}
	if (cnt == offset_k_timeout) {
		dev_err(rtd_phy->dev, "gen1 offset calibrate: wait reg(0x1030) bit15 == 1 timeout\n");
		return -EBUSY;
	}
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0002, 0x1126, 8, 5, 13);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0003, 0x1126, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0004, 0x1228, 0, 7, 8);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0005, 0x1128, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0006, 0x112a, 8, 5, 13);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0007, 0x112a, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0001, 0x112c, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0000, 0x112e, 0, 8, 10);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0014, 0x1125, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0015, 0x1127, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0016, 0x1129, 0, 7, 7);

	/*CMU Manual*/
	tmp = read_mdio_parallel_reg(rtd_phy, 0x1872) & GENMASK(5, 0);
	val = (read_mdio_parallel_reg(rtd_phy, 0x1810) & ~GENMASK(5, 0)) | tmp;
	write_mdio_parallel_reg(rtd_phy, 0x1810, val);
	val = (read_mdio_parallel_reg(rtd_phy, 0x1820) & ~GENMASK(5, 0)) | tmp;
	write_mdio_parallel_reg(rtd_phy, 0x1820, val);
	val = (read_mdio_parallel_reg(rtd_phy, 0x1830) & ~GENMASK(5, 0)) | tmp;
	write_mdio_parallel_reg(rtd_phy, 0x1830, val);
	write_mdio_parallel_reg(rtd_phy, 0x1802, 0x0007);
	write_mdio_parallel_reg(rtd_phy, 0x1800, 0x0041);

	/*GEN2*/
	write_mdio_parallel_reg(rtd_phy, 0x1a40, 0x4c05);
	write_mdio_parallel_reg(rtd_phy, 0x1c00, 0x125d);
	write_mdio_parallel_reg(rtd_phy, 0x1c00, 0x125f);
	write_mdio_parallel_reg(rtd_phy, 0x102e, 0x0010);
	cnt = 0;
	val = read_mdio_parallel_reg(rtd_phy, 0x1030);
	while (!(val & BIT(15)) && cnt < offset_k_timeout) {
		udelay(10);
		val = read_mdio_parallel_reg(rtd_phy, 0x1030);
		cnt++;
	}
	if (cnt == offset_k_timeout) {
		dev_err(rtd_phy->dev, "gen2 offset calibrate: wait reg(0x1030) bit15 == 1 timeout\n");
		return -EBUSY;
	}
	mdelay(1);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0002, 0x1156, 8, 5, 13);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0003, 0x1156, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0004, 0x1258, 0, 7, 8);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0005, 0x1158, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0006, 0x115a, 8, 5, 13);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0007, 0x115a, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0001, 0x115c, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0000, 0x115e, 0, 8, 10);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0014, 0x1155, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0015, 0x1157, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0016, 0x1159, 0, 7, 7);


	/*GEN3*/
	write_mdio_parallel_reg(rtd_phy, 0x1f82, 0x001f);
	write_mdio_parallel_reg(rtd_phy, 0x1f02, 0xfff1);
	write_mdio_parallel_reg(rtd_phy, 0x1f04, 0x00ff);
	write_mdio_parallel_reg(rtd_phy, 0x1f00, 0x002e);
	write_mdio_parallel_reg(rtd_phy, 0x1f22, 0x0048);
	write_mdio_parallel_reg(rtd_phy, 0x1f32, 0x0048);
	write_mdio_parallel_reg(rtd_phy, 0x1f42, 0x0048);
	write_mdio_parallel_reg(rtd_phy, 0x1f24, 0x2f68);
	val = (read_mdio_parallel_reg(rtd_phy, 0x1f26) & ~GENMASK(3, 0)) | 0x1;
	write_mdio_parallel_reg(rtd_phy, 0x1f26, val);
	write_mdio_parallel_reg(rtd_phy, 0x1f34, 0x2f68);
	val = (read_mdio_parallel_reg(rtd_phy, 0x1f36) & ~GENMASK(3, 0)) | 0x1;
	write_mdio_parallel_reg(rtd_phy, 0x1f36, val);
	write_mdio_parallel_reg(rtd_phy, 0x1f44, 0x2f68);
	val = (read_mdio_parallel_reg(rtd_phy, 0x1f46) & ~GENMASK(3, 0)) | 0x1;
	write_mdio_parallel_reg(rtd_phy, 0x1f46, val);
	write_mdio_parallel_reg(rtd_phy, 0x1f28, 0x0048);
	write_mdio_parallel_reg(rtd_phy, 0x1f38, 0x0048);
	write_mdio_parallel_reg(rtd_phy, 0x1f48, 0x0048);
	write_mdio_parallel_reg(rtd_phy, 0x1f2a, 0x00a0);
	write_mdio_parallel_reg(rtd_phy, 0x1f3a, 0x00a0);
	write_mdio_parallel_reg(rtd_phy, 0x1f4a, 0x00a0);
	write_mdio_parallel_reg(rtd_phy, 0x1f2c, 0x02e4);
	write_mdio_parallel_reg(rtd_phy, 0x1f3c, 0x02e4);
	write_mdio_parallel_reg(rtd_phy, 0x1f4c, 0x02e4);
	write_mdio_parallel_reg(rtd_phy, 0x1f2e, 0x02e4);
	write_mdio_parallel_reg(rtd_phy, 0x1f3e, 0x02e4);
	write_mdio_parallel_reg(rtd_phy, 0x1f4e, 0x02e4);
	write_mdio_parallel_reg(rtd_phy, 0x1f00, 0x002f);

	write_mdio_parallel_reg(rtd_phy, 0x1a40, 0x4c06);
	write_mdio_parallel_reg(rtd_phy, 0x1c00, 0x125d);
	write_mdio_parallel_reg(rtd_phy, 0x1c00, 0x125f);
	write_mdio_parallel_reg(rtd_phy, 0x102e, 0x0010);
	cnt = 0;
	val = read_mdio_parallel_reg(rtd_phy, 0x1030);
	while (!(val & BIT(15)) && cnt < offset_k_timeout) {
		udelay(10);
		val = read_mdio_parallel_reg(rtd_phy, 0x1030);
		cnt++;
	}
	if (cnt == offset_k_timeout) {
		dev_err(rtd_phy->dev, "gen3 offset calibrate: wait reg(0x1030) bit15 == 1 timeout(val:%x)\n", val);
		return -EBUSY;
	}
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0002, 0x1186, 8, 5, 13);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0003, 0x1186, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0004, 0x1288, 0, 7, 8);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0005, 0x1188, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0006, 0x118a, 8, 5, 13);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0007, 0x118a, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0001, 0x118c, 0, 5, 5);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0000, 0x118e, 0, 8, 10);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0014, 0x1185, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0015, 0x1187, 0, 7, 7);
	rtd1625_oobs_manual_write_back(rtd_phy, 0x0016, 0x1189, 0, 7, 7);

	/*End*/
	write_mdio_parallel_reg(rtd_phy, 0x1a4a, 0x0);
	write_mdio_parallel_reg(rtd_phy, 0x1a48, 0x8380);
	write_mdio_parallel_reg(rtd_phy, 0x1f00, 0x0);
	regmap_read(rtd_phy->pcie_base, PCIE_PHY_CTR, &val);
	regmap_write(rtd_phy->pcie_base, PCIE_PHY_CTR, val & ~BIT(3));
	write_mdio_parallel_reg(rtd_phy, 0x1a40, 0x4c00);
	write_mdio_parallel_reg(rtd_phy, 0x1990, 0x0554);
	write_mdio_parallel_reg(rtd_phy, 0x1990, 0x0570);
	regmap_read(rtd_phy->pcie_base, PCIE_PHY_CTR, &val);
	regmap_write(rtd_phy->pcie_base, PCIE_PHY_CTR, val & ~GENMASK(7, 6));

	return 0;

}

void write_tx_matrix_entry(struct rtd_pcie_phy *rtd_phy, int addr, int value)
{
	int write_enable = 0x3c0;
	int val;

	write_mdio_parallel_reg(rtd_phy, 0x1a60, value);
	val = read_mdio_parallel_reg(rtd_phy, 0x1a62);
	val = val & ~GENMASK(9, 0);
	write_mdio_parallel_reg(rtd_phy, 0x1a62, val| addr | write_enable);
	write_mdio_parallel_reg(rtd_phy, 0x1a62, val | addr);
}

void set_tx_matrix_table(struct rtd_pcie_phy *rtd_phy)
{
	int i;
	int value[32] = { 0x0, 0x821, 0x1042, 0x1863, 0x2084, 0x28A6, 0x30C7, 0x40E8,
			  0x4909, 0x512A, 0x594B, 0x616C, 0x698D, 0x69AE, 0x71CF, 0x79F0,
			  0x8210, 0x8A50, 0x8A70, 0x8A90, 0x8AB0, 0x8AF0, 0x8B10, 0x8B30,
			  0x8B70, 0x8B90, 0x8BD0, 0x8C10, 0x8C50, 0x8C90, 0x8CD0, 0x8D10,
			};

	for (i = 0; i < 32; i++)
		write_tx_matrix_entry(rtd_phy, i, value[i]);
}


static int rtd1625_pcie0_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = dev_get_priv(phy->dev);
	int ret;

	regmap_write(rtd_phy->pcie_base, PCIE_MDIO_CTL1, 0x3);
	mdio_reset(rtd_phy);
	write_mdio_parallel_reg(rtd_phy, 0x45, 0x1764);
	write_mdio_parallel_reg(rtd_phy, 0x95, 0x1764);
	write_mdio_parallel_reg(rtd_phy, 0xe5, 0x1764);
	write_mdio_parallel_reg(rtd_phy, 0x1e, 0x11);
	write_mdio_parallel_reg(rtd_phy, 0x1d, 0x18);
	write_mdio_parallel_reg(rtd_phy, 0x1b, 0xe0);
	write_mdio_parallel_reg(rtd_phy, 0x6e, 0x11);
	write_mdio_parallel_reg(rtd_phy, 0x6d, 0x18);
	write_mdio_parallel_reg(rtd_phy, 0x6b, 0xe0);
	write_mdio_parallel_reg(rtd_phy, 0xbe, 0x11);
	write_mdio_parallel_reg(rtd_phy, 0xbd, 0x18);
	write_mdio_parallel_reg(rtd_phy, 0xbb, 0xe0);
	write_mdio_parallel_reg(rtd_phy, 0x13, 0x2c02);
	write_mdio_parallel_reg(rtd_phy, 0x63, 0x2c03);
	write_mdio_parallel_reg(rtd_phy, 0xb3, 0x2f03);
	write_mdio_parallel_reg(rtd_phy, 0x43, 0x787);
	write_mdio_parallel_reg(rtd_phy, 0x93, 0x787);
	write_mdio_parallel_reg(rtd_phy, 0xe3, 0x787);
	write_mdio_parallel_reg(rtd_phy, 0x47, 0x7e40);
	write_mdio_parallel_reg(rtd_phy, 0x09, 0x7505);
	write_mdio_parallel_reg(rtd_phy, 0x59, 0x7505);
	write_mdio_parallel_reg(rtd_phy, 0xa9, 0x7505);
	write_mdio_parallel_reg(rtd_phy, 0x1122, 0xe87f);
	write_mdio_parallel_reg(rtd_phy, 0x1152, 0xe95f);
	write_mdio_parallel_reg(rtd_phy, 0x1182, 0xe95f);
	write_mdio_parallel_reg(rtd_phy, 0x1104, 0x1640);
	write_mdio_parallel_reg(rtd_phy, 0x1134, 0x13c0);
	write_mdio_parallel_reg(rtd_phy, 0x1102, 0x1640);
	write_mdio_parallel_reg(rtd_phy, 0x1132, 0x13c0);
	write_mdio_parallel_reg(rtd_phy, 0x1a6a, 0xf140);
	write_mdio_parallel_reg(rtd_phy, 0x1a2e, 0x0870);
	write_mdio_parallel_reg(rtd_phy, 0x1a72, 0xe188);
	write_mdio_parallel_reg(rtd_phy, 0x1898, 0x330);
	write_mdio_parallel_reg(rtd_phy, 0x18a8, 0x330);
	write_mdio_parallel_reg(rtd_phy, 0x18b8, 0x330);
	write_mdio_parallel_reg(rtd_phy, 0x189a, 0x0);
	write_mdio_parallel_reg(rtd_phy, 0x18aa, 0x0);
	write_mdio_parallel_reg(rtd_phy, 0x18ba, 0x0);
	write_mdio_parallel_reg(rtd_phy, 0x03, 0xe0f3);
	write_mdio_parallel_reg(rtd_phy, 0x53, 0xe0f3);
	write_mdio_parallel_reg(rtd_phy, 0xa3, 0xe0f3);
	write_mdio_parallel_reg(rtd_phy, 0xa6, 0x100c);
	write_mdio_parallel_reg(rtd_phy, 0x1010, 0xd927);
	write_mdio_parallel_reg(rtd_phy, 0x1176, 0x1f8);
	write_mdio_parallel_reg(rtd_phy, 0x1278, 0x2000);
	write_mdio_parallel_reg(rtd_phy, 0x117e, 0x2780);
	write_mdio_parallel_reg(rtd_phy, 0x1360, 0x3c4);
	write_mdio_parallel_reg(rtd_phy, 0x1362, 0x147);
	write_mdio_parallel_reg(rtd_phy, 0x1114, 0xa000);
	write_mdio_parallel_reg(rtd_phy, 0x1008, 0x20f4);
	write_mdio_parallel_reg(rtd_phy, 0x1116, 0x1f4);
	write_mdio_parallel_reg(rtd_phy, 0x1146, 0x1f4);
	write_mdio_parallel_reg(rtd_phy, 0x1f14, 0x0);
	write_mdio_parallel_reg(rtd_phy, 0xa, 0x832);
	write_mdio_parallel_reg(rtd_phy, 0x5a, 0x832);
	write_mdio_parallel_reg(rtd_phy, 0x97, 0x5240);
	write_mdio_parallel_reg(rtd_phy, 0x1990, 0x570);
	write_mdio_parallel_reg(rtd_phy, 0x1144, 0xc00);
	write_mdio_parallel_reg(rtd_phy, 0x0c, 0xcf93);
	write_mdio_parallel_reg(rtd_phy, 0x5c, 0xcf93);
	write_mdio_parallel_reg(rtd_phy, 0xac, 0xca91);

	set_tx_matrix_table(rtd_phy);

	ret = rtd1625_pcie0_phy_offset_calibrate(rtd_phy);
	if (ret)
		dev_err(rtd_phy->dev, "offset calibrate failed\n");

	return 0;
}

static int rtd1625_pcie1_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = dev_get_priv(phy->dev);

	/*Gen1*/
	write_mdio_reg(rtd_phy, 0x1, 0x5013);
	write_mdio_reg(rtd_phy, 0x4, 0x92f7);
	write_mdio_reg(rtd_phy, 0x6, 0x7);
	write_mdio_reg(rtd_phy, 0x8, 0x34c1);
	write_mdio_reg(rtd_phy, 0x9, 0x420c);
	write_mdio_reg(rtd_phy, 0xa, 0xa670);
	write_mdio_reg(rtd_phy, 0xb, 0x8d1d);
	write_mdio_reg(rtd_phy, 0xc, 0xc007);
	write_mdio_reg(rtd_phy, 0xd, 0xef28);
	write_mdio_reg(rtd_phy, 0xe, 0x1001);
	write_mdio_reg(rtd_phy, 0x21, 0x65aa);
	write_mdio_reg(rtd_phy, 0x23, 0xea6);
	write_mdio_reg(rtd_phy, 0x24, 0x4514);
	write_mdio_reg(rtd_phy, 0x25, 0x1260);
	write_mdio_reg(rtd_phy, 0x27, 0x4206);
	write_mdio_reg(rtd_phy, 0x2b, 0xb0d0);
	write_mdio_reg(rtd_phy, 0x2f, 0xa013);
	write_mdio_reg(rtd_phy, 0x32, 0xc401);
	/*Gen2*/
	write_mdio_reg(rtd_phy, 0x41, 0x5009);
	write_mdio_reg(rtd_phy, 0x44, 0x92f7);
	write_mdio_reg(rtd_phy, 0x46, 0x7);
	write_mdio_reg(rtd_phy, 0x48, 0x34c1);
	write_mdio_reg(rtd_phy, 0x49, 0x420c);
	write_mdio_reg(rtd_phy, 0x4a, 0xa650);
	write_mdio_reg(rtd_phy, 0x4b, 0x8d1d);
	write_mdio_reg(rtd_phy, 0x4c, 0xc007);
	write_mdio_reg(rtd_phy, 0x4d, 0xef28);
	write_mdio_reg(rtd_phy, 0x4e, 0x1001);
	write_mdio_reg(rtd_phy, 0x60, 0xc4ef);
	write_mdio_reg(rtd_phy, 0x61, 0xa5aa);
	write_mdio_reg(rtd_phy, 0x63, 0xea6);
	write_mdio_reg(rtd_phy, 0x65, 0x1260);
	write_mdio_reg(rtd_phy, 0x67, 0x4206);
	write_mdio_reg(rtd_phy, 0x6a, 0x7d69);
	write_mdio_reg(rtd_phy, 0x6b, 0xb0d0);
	write_mdio_reg(rtd_phy, 0x6f, 0xc008);
	write_mdio_reg(rtd_phy, 0x72, 0xc401);

	return 0;
}

static int rtd_pcie_phy_probe(struct udevice *dev)
{
	struct rtd_pcie_phy *rtd_phy;
	struct udevice *syscon_dev;
	int ret;

	rtd_phy = dev_get_priv(dev);
	rtd_phy->dev = dev;

	/* Get PCIe controller syscon (for MDIO register access) */
	ret = uclass_get_device_by_phandle(UCLASS_SYSCON, dev, "syscon", &syscon_dev);
	if (ret) {
		dev_err(dev, "Failed to get syscon device: %d\n", ret);
		return ret;
	}

	rtd_phy->pcie_base = syscon_get_regmap(syscon_dev);
	if (!rtd_phy->pcie_base) {
		dev_err(dev, "Failed to get regmap\n");
		return -EINVAL;
	}

	dev_info(dev, "PHY driver initialized\n");

	return 0;
}

static const struct phy_ops rtd1625_pcie0_phy_ops = {
	.init		= rtd1625_pcie0_phy_init,
};

static const struct udevice_id rtd1625_pcie0_phy_dt_match[] = {
	{ .compatible = "realtek,rtd1625-pcie-slot0-phy", },
	{},
};

U_BOOT_DRIVER(rtd1625_pcie0_phy) = {
	.name	= "rtd1625-pcie0-phy",
	.id	= UCLASS_PHY,
	.of_match = rtd1625_pcie0_phy_dt_match,
	.ops = &rtd1625_pcie0_phy_ops,
	.probe = rtd_pcie_phy_probe,
	.priv_auto	= sizeof(struct rtd_pcie_phy),
};

static const struct phy_ops rtd1625_pcie1_phy_ops = {
	.init		= rtd1625_pcie1_phy_init,
};

static const struct udevice_id rtd1625_pcie1_phy_dt_match[] = {
	{ .compatible = "realtek,rtd1625-pcie-slot1-phy", },
	{},
};

U_BOOT_DRIVER(rtd1625_pcie1_phy) = {
	.name	= "rtd1625-pcie1-phy",
	.id	= UCLASS_PHY,
	.of_match = rtd1625_pcie1_phy_dt_match,
	.ops = &rtd1625_pcie1_phy_ops,
	.probe = rtd_pcie_phy_probe,
	.priv_auto	= sizeof(struct rtd_pcie_phy),
};

