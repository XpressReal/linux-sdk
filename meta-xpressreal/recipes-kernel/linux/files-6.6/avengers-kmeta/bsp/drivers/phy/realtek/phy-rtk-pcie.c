// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek PCIe Controller PHY Driver
 *
 * Copyright (C) 2020 Realtek
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/nvmem-consumer.h>

#define DRV_NAME "rtd-pcie-phy"

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
	struct device *dev;
	struct reset_control *rst_phy;
	struct reset_control *rst_phy_mdio;
	struct regmap *pcie_base;
};

static int pcie_front_end_offset_calibrate(struct rtd_pcie_phy *rtd_phy);
static int pcie_OOBS_calibrate(struct rtd_pcie_phy *rtd_phy);

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

static int rtd13xx_pcie_phy_power_on(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);

	reset_control_deassert(rtd_phy->rst_phy);
	reset_control_deassert(rtd_phy->rst_phy_mdio);

	return 0;
}

static int rtd13xx_pcie_phy_power_off(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);

	reset_control_assert(rtd_phy->rst_phy);
	reset_control_assert(rtd_phy->rst_phy_mdio);

	return 0;
}

static int rtd_get_tx_swing(struct rtd_pcie_phy *rtd_phy)
{
	struct device_node *np = rtd_phy->dev->of_node;
	int ret = -EINVAL;

	if (of_find_property(np, "nvmem-cell-names", NULL)) {
		struct nvmem_cell *cell;

		cell = nvmem_cell_get(rtd_phy->dev, "tx_swing");
		if (!IS_ERR(cell)) {
			unsigned char *buf;
			size_t buf_size;

			buf = nvmem_cell_read(cell, &buf_size);
			ret = *buf;
			kfree(buf);
			nvmem_cell_put(cell);
		} else {
			dev_err(rtd_phy->dev, "missing nvmem resource\n");
		}
	} else {
		dev_dbg(rtd_phy->dev, "can't find nvmem cell node\n");
	}

	return ret;
}


static int rtd13xx_pcie_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);

	mdio_reset(rtd_phy);
	write_mdio_reg(rtd_phy, 0x06, 0x000C);
	write_mdio_reg(rtd_phy, 0x04, 0x52F5);
	write_mdio_reg(rtd_phy, 0x06, 0x000C);
	write_mdio_reg(rtd_phy, 0x0A, 0xC210);
	write_mdio_reg(rtd_phy, 0x29, 0xFF00);
	write_mdio_reg(rtd_phy, 0x01, 0xA852);
	write_mdio_reg(rtd_phy, 0x0B, 0xB905);
	write_mdio_reg(rtd_phy, 0x09, 0x620C);
	write_mdio_reg(rtd_phy, 0x24, 0x4F08);
	write_mdio_reg(rtd_phy, 0x0D, 0xF712);
	write_mdio_reg(rtd_phy, 0x23, 0xCB66);
	write_mdio_reg(rtd_phy, 0x20, 0xC466);
	write_mdio_reg(rtd_phy, 0x21, 0x5577);
	write_mdio_reg(rtd_phy, 0x22, 0x0033);
	write_mdio_reg(rtd_phy, 0x2F, 0x61BD);
	write_mdio_reg(rtd_phy, 0x0E, 0x1000);
	write_mdio_reg(rtd_phy, 0x2B, 0xB801);
	write_mdio_reg(rtd_phy, 0x1B, 0x8EA1);
	write_mdio_reg(rtd_phy, 0x09, 0x600C);
	write_mdio_reg(rtd_phy, 0x09, 0x620C);
	write_mdio_reg(rtd_phy, 0x46, 0x000C);
	write_mdio_reg(rtd_phy, 0x44, 0x52F5);
	write_mdio_reg(rtd_phy, 0x4A, 0xC210);
	write_mdio_reg(rtd_phy, 0x69, 0xFF00);
	write_mdio_reg(rtd_phy, 0x41, 0xA84A);
	write_mdio_reg(rtd_phy, 0x4B, 0xB905);
	write_mdio_reg(rtd_phy, 0x49, 0x620C);
	write_mdio_reg(rtd_phy, 0x64, 0x4F0C);
	write_mdio_reg(rtd_phy, 0x4D, 0xF712);
	write_mdio_reg(rtd_phy, 0x63, 0xCB66);
	write_mdio_reg(rtd_phy, 0x60, 0xC466);
	write_mdio_reg(rtd_phy, 0x61, 0x8866);
	write_mdio_reg(rtd_phy, 0x62, 0x0033);
	write_mdio_reg(rtd_phy, 0x6F, 0x91BD);
	write_mdio_reg(rtd_phy, 0x4E, 0x1000);
	write_mdio_reg(rtd_phy, 0x6B, 0xB801);
	write_mdio_reg(rtd_phy, 0x5B, 0x8EA1);
	write_mdio_reg(rtd_phy, 0x5E, 0x2CEB);
	write_mdio_reg(rtd_phy, 0x49, 0x600C);
	write_mdio_reg(rtd_phy, 0x49, 0x620C);

	return 0;
}

static int rtd16xxb_pcie_phy_general_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);

	mdio_reset(rtd_phy);
	/*Gen1*/
	write_mdio_reg(rtd_phy, 0x29, 0xFF13);
	write_mdio_reg(rtd_phy, 0x2A, 0x3D60);
	write_mdio_reg(rtd_phy, 0x05, 0xFAD3);
	write_mdio_reg(rtd_phy, 0x06, 0x0013);
	write_mdio_reg(rtd_phy, 0x01, 0xA852);
	write_mdio_reg(rtd_phy, 0x0A, 0xB650);
	write_mdio_reg(rtd_phy, 0x28, 0xF802);
	write_mdio_reg(rtd_phy, 0x0A, 0xB670);
	write_mdio_reg(rtd_phy, 0x24, 0x4F10);
	write_mdio_reg(rtd_phy, 0x23, 0xCB66);
	write_mdio_reg(rtd_phy, 0x20, 0xC4CC);
	write_mdio_reg(rtd_phy, 0x22, 0x0013);
	write_mdio_reg(rtd_phy, 0x21, 0x55AA);
	write_mdio_reg(rtd_phy, 0x2F, 0xA008);
	write_mdio_reg(rtd_phy, 0x0B, 0x9905);
	write_mdio_reg(rtd_phy, 0x09, 0x720C);
	write_mdio_reg(rtd_phy, 0x29, 0xFF13);
	write_mdio_reg(rtd_phy, 0x2B, 0xA801);
	/*Gen2*/
	write_mdio_reg(rtd_phy, 0x69, 0xFF13);
	write_mdio_reg(rtd_phy, 0x6A, 0x3D60);
	write_mdio_reg(rtd_phy, 0x45, 0xFAD3);
	write_mdio_reg(rtd_phy, 0x5E, 0x6EEB);
	write_mdio_reg(rtd_phy, 0x46, 0x0013);
	write_mdio_reg(rtd_phy, 0x41, 0x484A);
	write_mdio_reg(rtd_phy, 0x4A, 0xB650);
	write_mdio_reg(rtd_phy, 0x68, 0xF802);
	write_mdio_reg(rtd_phy, 0x63, 0xCB66);
	write_mdio_reg(rtd_phy, 0x60, 0xC4EE);
	write_mdio_reg(rtd_phy, 0x62, 0x0013);
	write_mdio_reg(rtd_phy, 0x61, 0x55AA);
	write_mdio_reg(rtd_phy, 0x6F, 0xA008);
	write_mdio_reg(rtd_phy, 0x4B, 0x9905);
	write_mdio_reg(rtd_phy, 0x49, 0x720C);
	write_mdio_reg(rtd_phy, 0x69, 0xFF13);
	write_mdio_reg(rtd_phy, 0x6B, 0xA801);

	return 0;

}

static int rtd16xxb_pcie1_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);
	int tx_swing_otp;
	u8 tx_swing_val;

	mdio_reset(rtd_phy);
	rtd16xxb_pcie_phy_general_init(phy);

	/*tx_swing*/
	tx_swing_otp = rtd_get_tx_swing(rtd_phy);
	if (tx_swing_otp >= 0) {
		int val = 0;

		tx_swing_val = (tx_swing_otp & GENMASK(3, 0)) ^ 0xb;
		val = read_mdio_reg(rtd_phy, 0x20);
		val &= ~GENMASK(7, 0);
		val |= (tx_swing_val | (tx_swing_val << 4));
		write_mdio_reg(rtd_phy, 0x20, val);
		write_mdio_reg(rtd_phy, 0x23, 0x0B66);
		tx_swing_val = ((tx_swing_otp & GENMASK(7, 4)) >> 4) ^ 0xb;
		val = read_mdio_reg(rtd_phy, 0x60);
		val &= ~GENMASK(7, 0);
		val |= (tx_swing_val | (tx_swing_val << 4));
		write_mdio_reg(rtd_phy, 0x60, val);
		write_mdio_reg(rtd_phy, 0x63, 0x0B66);
	}

	pcie_OOBS_calibrate(rtd_phy);
	pcie_front_end_offset_calibrate(rtd_phy);

	return 0;

}

static int rtd16xxb_pcie2_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);
	int tx_swing_otp;
	u8 tx_swing_val;

	mdio_reset(rtd_phy);
	rtd16xxb_pcie_phy_general_init(phy);

	/*tx_swing*/
	tx_swing_otp = rtd_get_tx_swing(rtd_phy);
	if (tx_swing_otp >= 0) {
		int val = 0;

		tx_swing_val = ((tx_swing_otp & GENMASK(11, 8)) >> 8) ^ 0xb;
		val = read_mdio_reg(rtd_phy, 0x20);
		val &= ~GENMASK(7, 0);
		val |= (tx_swing_val | (tx_swing_val << 4));
		write_mdio_reg(rtd_phy, 0x20, val);
		tx_swing_val = ((tx_swing_otp & GENMASK(15, 12)) >> 12) ^ 0xb;
		val = read_mdio_reg(rtd_phy, 0x60);
		val &= ~GENMASK(7, 0);
		val |= (tx_swing_val | (tx_swing_val << 4));
		write_mdio_reg(rtd_phy, 0x60, val);
	}

	pcie_OOBS_calibrate(rtd_phy);
	pcie_front_end_offset_calibrate(rtd_phy);

	return 0;

}



static int rtd13xxd_pcie_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);
	int tx_swing_otp;
	u8 tx_swing_val;

	mdio_reset(rtd_phy);
	/*Gen1*/
	write_mdio_reg(rtd_phy, 0x01, 0xA852);
	write_mdio_reg(rtd_phy, 0x04, 0xD2F5);
	write_mdio_reg(rtd_phy, 0x06, 0x0017);
	write_mdio_reg(rtd_phy, 0x09, 0x420C);
	write_mdio_reg(rtd_phy, 0x0A, 0x9270);
	write_mdio_reg(rtd_phy, 0x0B, 0xA905);
	write_mdio_reg(rtd_phy, 0x0C, 0xE000);
	write_mdio_reg(rtd_phy, 0x0D, 0xF71E);
	write_mdio_reg(rtd_phy, 0x0E, 0x1000);
	write_mdio_reg(rtd_phy, 0x21, 0x77AA);
	write_mdio_reg(rtd_phy, 0x22, 0x3813);
	write_mdio_reg(rtd_phy, 0x23, 0x0B62);
	write_mdio_reg(rtd_phy, 0x24, 0x4724);
	write_mdio_reg(rtd_phy, 0x28, 0xF802);
	write_mdio_reg(rtd_phy, 0x29, 0xFF10);
	write_mdio_reg(rtd_phy, 0x2A, 0x3D61);
	write_mdio_reg(rtd_phy, 0x2B, 0xB001);

	/*Gen2*/
	write_mdio_reg(rtd_phy, 0x41, 0x304A);
	write_mdio_reg(rtd_phy, 0x44, 0xD2F5);
	write_mdio_reg(rtd_phy, 0x46, 0x0017);
	write_mdio_reg(rtd_phy, 0x49, 0x420C);
	write_mdio_reg(rtd_phy, 0x4A, 0x9250);
	write_mdio_reg(rtd_phy, 0x4B, 0xA905);
	write_mdio_reg(rtd_phy, 0x4C, 0xE000);
	write_mdio_reg(rtd_phy, 0x4D, 0xF71E);
	write_mdio_reg(rtd_phy, 0x4E, 0x1000);
	write_mdio_reg(rtd_phy, 0x5E, 0x6EEB);
	write_mdio_reg(rtd_phy, 0x60, 0xC4CC);
	write_mdio_reg(rtd_phy, 0x61, 0x66AA);
	write_mdio_reg(rtd_phy, 0x62, 0x3813);
	write_mdio_reg(rtd_phy, 0x63, 0x0B62);
	write_mdio_reg(rtd_phy, 0x68, 0xF802);
	write_mdio_reg(rtd_phy, 0x69, 0xFF10);
	write_mdio_reg(rtd_phy, 0x6A, 0x3D61);
	write_mdio_reg(rtd_phy, 0x6B, 0xB001);
	write_mdio_reg(rtd_phy, 0x6F, 0x9008);

	/*tx_swing*/
	tx_swing_otp = rtd_get_tx_swing(rtd_phy);
	if (tx_swing_otp >= 0) {
		int val = 0;

		tx_swing_val = tx_swing_otp ^ 0xc;
		val = read_mdio_reg(rtd_phy, 0x20);
		val &= ~GENMASK(7, 0);
		val |= (tx_swing_val | (tx_swing_val << 4));
		write_mdio_reg(rtd_phy, 0x20, val);
		val = read_mdio_reg(rtd_phy, 0x60);
		val &= ~GENMASK(7, 0);
		val |= (tx_swing_val | (tx_swing_val << 4));
		write_mdio_reg(rtd_phy, 0x60, val);
	}

	pcie_OOBS_calibrate(rtd_phy);
	pcie_front_end_offset_calibrate(rtd_phy);

	return 0;
}

static int rtd13xxe_pcie_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);
	u8 tx_swing_val;
	int tx_swing_otp;

	mdio_reset(rtd_phy);
	/*Gen1*/
	write_mdio_reg(rtd_phy, 0x01, 0x4052);
	write_mdio_reg(rtd_phy, 0x04, 0xD2F5);
	write_mdio_reg(rtd_phy, 0x09, 0x420C);
	write_mdio_reg(rtd_phy, 0x0A, 0x9270);
	write_mdio_reg(rtd_phy, 0x0B, 0x9B05);
	write_mdio_reg(rtd_phy, 0x0E, 0x1001);
	write_mdio_reg(rtd_phy, 0x19, 0x7D63);
	write_mdio_reg(rtd_phy, 0x22, 0x7823);
	write_mdio_reg(rtd_phy, 0x23, 0x0EA2);
	write_mdio_reg(rtd_phy, 0x24, 0x4720);
	write_mdio_reg(rtd_phy, 0x28, 0xF802);
	write_mdio_reg(rtd_phy, 0x29, 0xFF10);
	write_mdio_reg(rtd_phy, 0x2A, 0x3D62);

	/*Gen2*/
	write_mdio_reg(rtd_phy, 0x41, 0x404A);
	write_mdio_reg(rtd_phy, 0x44, 0xD2F5);
	write_mdio_reg(rtd_phy, 0x49, 0x420C);
	write_mdio_reg(rtd_phy, 0x4B, 0x9B05);
	write_mdio_reg(rtd_phy, 0x4E, 0x1001);
	write_mdio_reg(rtd_phy, 0x59, 0x7D63);
	write_mdio_reg(rtd_phy, 0x5E, 0x6EEB);
	write_mdio_reg(rtd_phy, 0x60, 0xC4CC);
	write_mdio_reg(rtd_phy, 0x61, 0x66AA);
	write_mdio_reg(rtd_phy, 0x62, 0x7823);
	write_mdio_reg(rtd_phy, 0x63, 0x0EA2);
	write_mdio_reg(rtd_phy, 0x68, 0xF802);
	write_mdio_reg(rtd_phy, 0x69, 0xFF10);
	write_mdio_reg(rtd_phy, 0x6F, 0x9008);
	write_mdio_reg(rtd_phy, 0x6A, 0x3D62);

	/*tx_swing*/
	tx_swing_otp = rtd_get_tx_swing(rtd_phy);
	if (tx_swing_otp >= 0) {
		int val = 0;

		tx_swing_val = tx_swing_otp ^ 0xc;
		val = read_mdio_reg(rtd_phy, 0x20);
		val &= ~GENMASK(7, 0);
		val |= (tx_swing_val | (tx_swing_val << 4));
		write_mdio_reg(rtd_phy, 0x20, val);
		val = read_mdio_reg(rtd_phy, 0x60);
		val &= ~GENMASK(7, 0);
		val |= (tx_swing_val | (tx_swing_val << 4));
		write_mdio_reg(rtd_phy, 0x60, val);
	}

	pcie_OOBS_calibrate(rtd_phy);
	pcie_front_end_offset_calibrate(rtd_phy);

	return 0;
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
	int offset_k_timeout = 10000;

	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
	regmap_write(rtd_phy->pcie_base, PCIE_PHY_CTR, (val & ~GENMASK(7, 6)) | (0x1 << 6) | BIT(3));

	/*Start*/
	write_mdio_parallel_reg(rtd_phy, 0x1a4a, 0xcffc);
	write_mdio_parallel_reg(rtd_phy, 0x1a48, 0x838f);
	write_mdio_parallel_reg(rtd_phy, 0x1990, 0x0554);
	write_mdio_parallel_reg(rtd_phy, 0x1a40, 0x4c04);
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
	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
	regmap_write(rtd_phy->pcie_base, PCIE_PHY_CTR, val & ~BIT(3));
	write_mdio_parallel_reg(rtd_phy, 0x1a40, 0x4c00);
	write_mdio_parallel_reg(rtd_phy, 0x1990, 0x0554);
	write_mdio_parallel_reg(rtd_phy, 0x1990, 0x0570);
	regmap_read(rtd_phy->pcie_base, PCIE_MDIO_CTR, &val);
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
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);
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

	set_tx_matrix_table(rtd_phy);

	ret = rtd1625_pcie0_phy_offset_calibrate(rtd_phy);
	if (ret)
		dev_err(rtd_phy->dev, "offset calibrate failed\n");

	return 0;
}

static int rtd1625_pcie1_phy_init(struct phy *phy)
{
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);

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

static u8 gray_to_binary(u8 gray)
{
	u8 binary;

	binary = gray & BIT(4);
	binary |= (gray ^ (binary >> 1)) & BIT(3);
	binary |= (gray ^ (binary >> 1)) & BIT(2);
	binary |= (gray ^ (binary >> 1)) & BIT(1);
	binary |= (gray ^ (binary >> 1)) & BIT(0);

	pr_debug("[PCIE] gray:0x%x  binary:0x%x\n", gray, binary);

	return binary;
}


static void pcie_LEQ_calibrate(struct rtd_pcie_phy *rtd_phy, int speed)
{
	int val;
	u8 gray_code;
	u8 binary_code;

	if (speed == 1) {
		val = read_mdio_reg(rtd_phy, 0x1f);
		gray_code = (val & GENMASK(15, 11)) >> 11;
		binary_code = gray_to_binary(gray_code);

		val = read_mdio_reg(rtd_phy, 0x24);
		val = (val & ~GENMASK(6, 2)) | (binary_code << 2);
		write_mdio_reg(rtd_phy, 0x24, val);

		val = read_mdio_reg(rtd_phy, 0x0a);
		val = val | BIT(5);
		write_mdio_reg(rtd_phy, 0x0a, val);

	} else if (speed == 2) {
		val = read_mdio_reg(rtd_phy, 0x5f);
		gray_code = (val & GENMASK(15, 11)) >> 11;
		binary_code = gray_to_binary(gray_code);

		val = read_mdio_reg(rtd_phy, 0x64);
		val = (val & ~GENMASK(6, 2)) | (binary_code << 2);
		write_mdio_reg(rtd_phy, 0x64, val);

		val = read_mdio_reg(rtd_phy, 0x4a);
		val = val | BIT(5);
		write_mdio_reg(rtd_phy, 0x4a, val);
	}

}

static int pcie_front_end_offset_calibrate(struct rtd_pcie_phy *rtd_phy)
{
	int val;
	int cnt;

	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x1f);
	while (!(val & BIT(15)) && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x1f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "Front End: wait mdio reg(0x1f) bit15 == 1 timeout\n");
		return -EBUSY;
	}
	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x5f);
	while (!(val & BIT(15)) && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x5f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "Front End: wait mdio reg(0x5f) bit15 == 1 timeout\n");
		return -EBUSY;
	}

	val = read_mdio_reg(rtd_phy, 0x0D);
	val &= ~BIT(6);
	write_mdio_reg(rtd_phy, 0x0D, val);

	val = read_mdio_reg(rtd_phy, 0x4D);
	val &= ~BIT(6);
	write_mdio_reg(rtd_phy, 0x4D, val);

	val = read_mdio_reg(rtd_phy, 0x19);
	val &= ~BIT(2);
	write_mdio_reg(rtd_phy, 0x19, val);

	val = read_mdio_reg(rtd_phy, 0x59);
	val &= ~BIT(2);
	write_mdio_reg(rtd_phy, 0x59, val);

	write_mdio_reg(rtd_phy, 0x10, 0x000C);

	write_mdio_reg(rtd_phy, 0x50, 0x000C);

	val = read_mdio_reg(rtd_phy, 0x1f);
	val = (val & GENMASK(4, 1)) >> 1;
	if ((val != 0x0 && val != 0x1111))
		return 0;

	val = read_mdio_reg(rtd_phy, 0x5f);
	val = (val & GENMASK(4, 1)) >> 1;
	if ((val != 0x0 && val != 0x1111))
		return 0;

	val = read_mdio_reg(rtd_phy, 0x0B);
	val |= 0x3 << 2;
	write_mdio_reg(rtd_phy, 0x0B, val);

	val = read_mdio_reg(rtd_phy, 0x4B);
	val |= 0x3 << 2;
	write_mdio_reg(rtd_phy, 0x4B, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val &= ~BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val &= ~BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x0D);
	val |= BIT(6);
	write_mdio_reg(rtd_phy, 0x0D, val);

	val = read_mdio_reg(rtd_phy, 0x4D);
	val |= BIT(6);
	write_mdio_reg(rtd_phy, 0x4D, val);

	val = read_mdio_reg(rtd_phy, 0x19);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x19, val);

	val = read_mdio_reg(rtd_phy, 0x59);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x59, val);

	write_mdio_reg(rtd_phy, 0x10, 0x3C4);

	write_mdio_reg(rtd_phy, 0x50, 0x3C4);

	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x1f);
	while (!(val & BIT(15)) && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x1f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "Front End again: wait mdio reg(0x1f) bit15 == 1 timeout\n");
		return -EBUSY;
	}

	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x5f);
	while (!(val & BIT(15)) && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x5f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "Front End again: wait mdio reg(0x5f) bit15 == 1 timeout\n");
		return -EBUSY;
	}

	return 0;
}



static int pcie_OOBS_calibrate(struct rtd_pcie_phy *rtd_phy)
{
	int val;
	int cnt;
	int tmp;

	val = read_mdio_reg(rtd_phy, 0x09);
	val &= ~BIT(4);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val &= ~BIT(4);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val &= ~BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val &= ~BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(9);
	write_mdio_reg(rtd_phy, 0x49, val);

	val = read_mdio_reg(rtd_phy, 0x0D);
	val |= BIT(6);
	write_mdio_reg(rtd_phy, 0x0D, val);

	val = read_mdio_reg(rtd_phy, 0x4D);
	val |= BIT(6);
	write_mdio_reg(rtd_phy, 0x4D, val);

	val = read_mdio_reg(rtd_phy, 0x19);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x19, val);

	val = read_mdio_reg(rtd_phy, 0x59);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x59, val);

	write_mdio_reg(rtd_phy, 0x10, 0x03C4);

	write_mdio_reg(rtd_phy, 0x50, 0x03C4);

	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x1f);
	while ((val & BIT(6)) != 0 && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x1f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "wait mdio reg(0x1f) bit6 == 0 timeout\n");
		return -EBUSY;
	}

	cnt = 0;
	val = read_mdio_reg(rtd_phy, 0x5f);
	while ((val & BIT(6)) != 0 && cnt < 10) {
		udelay(10);
		val = read_mdio_reg(rtd_phy, 0x5f);
		cnt++;
	}
	if (cnt == 10) {
		dev_err(rtd_phy->dev, "wait mdio reg(0x5f) bit6 == 0 timeout\n");
		return -EBUSY;
	}

	mdelay(1);

	val = read_mdio_reg(rtd_phy, 0x19);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x19, val);

	val = read_mdio_reg(rtd_phy, 0x59);
	val |= BIT(2);
	write_mdio_reg(rtd_phy, 0x59, val);

	write_mdio_reg(rtd_phy, 0x10, 0x03C4);

	write_mdio_reg(rtd_phy, 0x50, 0x03C4);

	tmp = read_mdio_reg(rtd_phy, 0x1f);
	tmp = (tmp & GENMASK(12, 8)) >> 8;
	val = read_mdio_reg(rtd_phy, 0x03);
	val = (val & ~ GENMASK(5, 1)) | (tmp << 1);
	write_mdio_reg(rtd_phy, 0x03, val);

	tmp = read_mdio_reg(rtd_phy, 0x5f);
	tmp = (tmp & GENMASK(12, 8)) >> 8;
	val = read_mdio_reg(rtd_phy, 0x43);
	val = (val & ~ GENMASK(5, 1)) | (tmp << 1);
	write_mdio_reg(rtd_phy, 0x43, val);

	val = read_mdio_reg(rtd_phy, 0x09);
	val |= BIT(4);
	write_mdio_reg(rtd_phy, 0x09, val);

	val = read_mdio_reg(rtd_phy, 0x49);
	val |= BIT(4);
	write_mdio_reg(rtd_phy, 0x49, val);

	return 0;
}

static void rtd13xx_pcie_front_end_offset_calibrate(struct rtd_pcie_phy *rtd_phy, int speed)
{
	int val;
	int tmp;

	if (speed == 1) {
		val = read_mdio_reg(rtd_phy, 0x1f);
		val = (val & GENMASK(4, 1)) >> 1;

		tmp = read_mdio_reg(rtd_phy, 0x0b);
			val = (tmp & ~GENMASK(8, 5)) | (val << 5);
		write_mdio_reg(rtd_phy, 0x0b, val);

		val = read_mdio_reg(rtd_phy, 0x0d);
		val &= ~BIT(13);
		write_mdio_reg(rtd_phy, 0x0d, val);
	} else if (speed ==2) {
		val = read_mdio_reg(rtd_phy, 0x5f);
		val = (val & GENMASK(4, 1)) >> 1;

		tmp = read_mdio_reg(rtd_phy, 0x4b);
			val = (tmp & ~GENMASK(8, 5)) | (val << 5);
		write_mdio_reg(rtd_phy, 0x4b, val);

		val = read_mdio_reg(rtd_phy, 0x4d);
		val &= ~BIT(13);
		write_mdio_reg(rtd_phy, 0x4d, val);
	}

}

static void rtd13xx_pcie_LEQ_calibrate(struct rtd_pcie_phy *rtd_phy, int speed)
{
	int val;
	u8 gray_code;
	u8 binary_code;

	if (speed == 1) {
		val = read_mdio_reg(rtd_phy, 0x1f);
		gray_code = (val & GENMASK(15, 11)) >> 11;
		binary_code = gray_to_binary(gray_code);

		val = read_mdio_reg(rtd_phy, 0x24);
		val = (val & ~GENMASK(6, 2)) | (binary_code << 2);
		write_mdio_reg(rtd_phy, 0x24, val);

		val = read_mdio_reg(rtd_phy, 0x0a);
		val = val | BIT(5);
		write_mdio_reg(rtd_phy, 0x0a, val);

		val = read_mdio_reg(rtd_phy, 0x0a);
		val = val | BIT(6);
		write_mdio_reg(rtd_phy, 0x0a, val);

	} else if (speed == 2) {
		val = read_mdio_reg(rtd_phy, 0x5f);
		gray_code = (val & GENMASK(15, 11)) >> 11;
		binary_code = gray_to_binary(gray_code);

		val = read_mdio_reg(rtd_phy, 0x64);
		val = (val & ~GENMASK(6, 2)) | (binary_code << 2);
		write_mdio_reg(rtd_phy, 0x64, val);

		val = read_mdio_reg(rtd_phy, 0x4a);
		val = val | BIT(5);
		write_mdio_reg(rtd_phy, 0x4a, val);

		val = read_mdio_reg(rtd_phy, 0x4a);
		val = val | BIT(6);
		write_mdio_reg(rtd_phy, 0x4a, val);
	}

}


static int rtd16xxb_pcie_phy_calibrate(struct phy *phy)
{
	unsigned int val;
	int speed;
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);

	regmap_read(rtd_phy->pcie_base, LINK_CONTROL_LINK_STATUS_REG, &val);
	speed = (val & GENMASK(19, 16)) >> 16;
	pcie_LEQ_calibrate(rtd_phy, speed);

	return 0;
}

static int rtd13xx_pcie_phy_calibrate(struct phy *phy)
{
	unsigned int val;
	int speed;
	struct rtd_pcie_phy *rtd_phy = phy_get_drvdata(phy);

	regmap_read(rtd_phy->pcie_base, LINK_CONTROL_LINK_STATUS_REG, &val);
	speed = (val & GENMASK(19, 16)) >> 16;
	rtd13xx_pcie_front_end_offset_calibrate(rtd_phy, speed);
	rtd13xx_pcie_LEQ_calibrate(rtd_phy, speed);

	return 0;
}

static const struct phy_ops rtd13xx_pcie_phy_ops = {
	.init		= rtd13xx_pcie_phy_init,
	.power_on	= rtd13xx_pcie_phy_power_on,
	.power_off	= rtd13xx_pcie_phy_power_off,
	.calibrate	= rtd13xx_pcie_phy_calibrate,
	.owner		= THIS_MODULE,
};

static const struct phy_ops rtd16xxb_pcie1_phy_ops = {
	.init		= rtd16xxb_pcie1_phy_init,
	.power_on	= rtd13xx_pcie_phy_power_on,
	.power_off	= rtd13xx_pcie_phy_power_off,
	.calibrate	= rtd16xxb_pcie_phy_calibrate,
	.owner		= THIS_MODULE,
};

static const struct phy_ops rtd16xxb_pcie2_phy_ops = {
	.init		= rtd16xxb_pcie2_phy_init,
	.power_on	= rtd13xx_pcie_phy_power_on,
	.power_off	= rtd13xx_pcie_phy_power_off,
	.calibrate	= rtd16xxb_pcie_phy_calibrate,
	.owner		= THIS_MODULE,
};


static const struct phy_ops rtd13xxd_pcie_phy_ops = {
	.init		= rtd13xxd_pcie_phy_init,
	.power_on	= rtd13xx_pcie_phy_power_on,
	.power_off	= rtd13xx_pcie_phy_power_off,
	.calibrate	= rtd16xxb_pcie_phy_calibrate,
	.owner		= THIS_MODULE,
};

static const struct phy_ops rtd13xxe_pcie_phy_ops = {
	.init		= rtd13xxe_pcie_phy_init,
	.power_on	= rtd13xx_pcie_phy_power_on,
	.power_off	= rtd13xx_pcie_phy_power_off,
	.calibrate	= rtd16xxb_pcie_phy_calibrate,
	.owner		= THIS_MODULE,
};

static const struct phy_ops rtd1625_pcie0_phy_ops = {
	.init		= rtd1625_pcie0_phy_init,
	.power_on	= rtd13xx_pcie_phy_power_on,
	.power_off	= rtd13xx_pcie_phy_power_off,
	//.calibrate	= rtd16xxb_pcie_phy_calibrate,
	.owner		= THIS_MODULE,
};

static const struct phy_ops rtd1625_pcie1_phy_ops = {
	.init		= rtd1625_pcie1_phy_init,
	.power_on	= rtd13xx_pcie_phy_power_on,
	.power_off	= rtd13xx_pcie_phy_power_off,
	//.calibrate	= rtd16xxb_pcie_phy_calibrate,
	.owner		= THIS_MODULE,
};

static int rtd_pcie_phy_probe(struct platform_device *pdev)
{
	struct device_node *syscon_np;
	struct rtd_pcie_phy *rtd_phy;
	struct phy_ops *ops;
	struct phy_provider *phy_provider;
	struct phy *phy;

	rtd_phy = devm_kzalloc(&pdev->dev, sizeof(*rtd_phy), GFP_KERNEL);
	if (!rtd_phy)
		return -ENOMEM;

	rtd_phy->dev = &pdev->dev;

	ops = (struct phy_ops *)of_device_get_match_data(rtd_phy->dev);

	syscon_np = of_parse_phandle(rtd_phy->dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np))
		return -ENODEV;

	rtd_phy->pcie_base = device_node_to_regmap(syscon_np);
	if (IS_ERR_OR_NULL(rtd_phy->pcie_base)) {
		of_node_put(syscon_np);
		return -EINVAL;
	}

	rtd_phy->rst_phy = devm_reset_control_get(rtd_phy->dev, "phy");
	if (rtd_phy->rst_phy == NULL) {
		dev_err(rtd_phy->dev, "phy source missing or invalid\n");
		return -EINVAL;
	}

	rtd_phy->rst_phy_mdio = devm_reset_control_get(rtd_phy->dev, "phy_mdio");
	if (rtd_phy->rst_phy_mdio == NULL) {
		dev_err(rtd_phy->dev, "phy_mdio source missing. or invalid\n");
		return -EINVAL;
	}

	phy = devm_phy_create(rtd_phy->dev, rtd_phy->dev->of_node, ops);
	if (IS_ERR(phy)) {
		dev_err(rtd_phy->dev, "failed to create phy\n");
		return PTR_ERR(phy);
	}

	phy_set_drvdata(phy, rtd_phy);

	phy_provider = devm_of_phy_provider_register(rtd_phy->dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		dev_err(rtd_phy->dev, "failed to register phy provider\n");

	dev_info(rtd_phy->dev, "init done\n");
	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id rtd_pcie_phy_of_match[] = {
	{ .compatible = "realtek,rtd13xx-pcie-slot0-phy", .data = &rtd13xx_pcie_phy_ops},
	{ .compatible = "realtek,rtd13xx-pcie-slot1-phy", .data = &rtd13xx_pcie_phy_ops},
	{ .compatible = "realtek,rtd13xx-pcie-slot2-phy", .data = &rtd13xx_pcie_phy_ops},
	{ .compatible = "realtek,rtd16xxb-pcie-slot1-phy", .data = &rtd16xxb_pcie1_phy_ops},
	{ .compatible = "realtek,rtd16xxb-pcie-slot2-phy", .data = &rtd16xxb_pcie2_phy_ops},
	{ .compatible = "realtek,rtd13xxd-pcie-slot1-phy", .data = &rtd13xxd_pcie_phy_ops},
	{ .compatible = "realtek,rtd13xxe-pcie-slot1-phy", .data = &rtd13xxe_pcie_phy_ops},
	{ .compatible = "realtek,rtd1625-pcie-slot0-phy", .data = &rtd1625_pcie0_phy_ops},
	{ .compatible = "realtek,rtd1625-pcie-slot1-phy", .data = &rtd1625_pcie1_phy_ops},
	{ },
};

static struct platform_driver rtd_pcie_phy_driver = {
	.probe	= rtd_pcie_phy_probe,
	.driver	= {
		.name = DRV_NAME,
		.of_match_table	= rtd_pcie_phy_of_match,
	},
};

module_platform_driver(rtd_pcie_phy_driver);

MODULE_DESCRIPTION("Realtek PCIe PHY driver");
MODULE_AUTHOR("TYChang <tychang@realtek.com>");
MODULE_LICENSE("GPL v2");
