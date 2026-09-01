// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_dprx.h"

/* Register base addresses */
#define DPRX_REG_BASE		0x98165000
#define DPRX_IP_REG_BASE	0x98166000

/* Debug module parameter */
static bool rbus_debug = false;
module_param(rbus_debug, bool, 0644);
MODULE_PARM_DESC(rbus_debug, "Enable register write debug log (default: 0)");

static struct rtk_dprx *g_dprx;

/*
 * DPRX Register Operations (base: 0x98165000)
 */

static int rtk_dprx_rbus_read(u32 offset, u32 *value)
{
	int ret;

	ret = regmap_read(g_dprx->dprx_reg, offset, value);

	return ret;
}

static int rtk_dprx_rbus_write(u32 offset, u32 value)
{
	if (rbus_debug)
		dev_info(g_dprx->dev, "[RBUS] Reg 0x%08x = 0x%08x\n",
			 DPRX_REG_BASE + offset, value);

	return regmap_write(g_dprx->dprx_reg, offset, value);
}

static int rtk_dprx_rbus_mask_write(u32 offset, u32 mask, u32 val)
{
	u32 orig = 0;
	u32 tmp = 0;

	rtk_dprx_rbus_read(offset, &orig);
	tmp = orig & ~mask;
	tmp |= val & mask;
	return rtk_dprx_rbus_write(offset, tmp);
}

static void rtk_dprx_dump_dprx_reg(struct rtk_dprx *dprx,
		u32 start_offset, u32 end_offset)
{
	u32 offset;
	u32 reg_val = 0;

	dev_err(dprx->dev, "Dump dprx registers from offset 0x%x to 0x%x\n",
		start_offset, end_offset);

	for (offset = start_offset; offset <= end_offset; offset += 4) {
		usleep_range(100, 200);
		regmap_read(dprx->dprx_reg, offset, &reg_val);
		dev_info(dprx->dev, "Read reg0x%08x = 0x%08x\n",
			DPRX_REG_BASE + offset, reg_val);
	}
}

/*
 * DPRX IP Register Operations (base: 0x98166000)
 */

static void rtk_dprx_rbus_set_byte(u32 offset, u32 value)
{
	if (value > 0xFF)
		dev_err(g_dprx->dev, "[RBUS] set invalid ip_reg value, offset=0x%08x value=0x%x\n",
			offset, value);

	if (rbus_debug)
		dev_info(g_dprx->dev, "[RBUS] Reg 0x%08x = 0x%02x\n",
			 DPRX_IP_REG_BASE + offset, value);

	regmap_write(g_dprx->ip_reg, offset, value);
}

static u8 rtk_dprx_rbus_get_byte(u32 offset)
{
	u32 reg_val = 0;

	regmap_read(g_dprx->ip_reg, offset, &reg_val);

	if (reg_val > 0xFF)
		dev_err(g_dprx->dev, "[RBUS] unexpected ip_reg value, 0x%08x=0x%08x\n",
			DPRX_IP_REG_BASE + offset, reg_val);

	return (u8)reg_val;
}

static void rtk_dprx_rbus_set_bit(u32 offset, u32 and_mask, u32 or_mask)
{
	u32 reg_val = 0;

	regmap_read(g_dprx->ip_reg, offset, &reg_val);
	reg_val &=  and_mask;
	reg_val |=  or_mask;

	if (rbus_debug)
		dev_info(g_dprx->dev, "[RBUS] Reg 0x%08x = 0x%02x (mask: AND=0x%02x OR=0x%02x)\n",
			 DPRX_IP_REG_BASE + offset, reg_val, and_mask, or_mask);

	regmap_write(g_dprx->ip_reg, offset, reg_val);
}

static u8 rtk_dprx_rbus_get_bit(u32 offset, u32 and_mask)
{
	u32 reg_val = 0;
	u8 byte = 0;

	regmap_read(g_dprx->ip_reg, offset, &reg_val);
	reg_val &=  and_mask;
	byte = (u8)reg_val;

	return byte;
}

static u32 rtk_dprx_rbus_get_word(u32 offset)
{
	u32 val_m = 0;
	u32 val_l = 0;
	u32 ret_val = 0;

	regmap_read(g_dprx->ip_reg, offset, &val_m);
	regmap_read(g_dprx->ip_reg, offset + 4, &val_l);

	if ((val_m > 0xFF) || (val_l > 0xFF))
		dev_err(g_dprx->dev, "[RBUS] unexpected ip_reg value, 0x%08x=0x%08x 0x%08x=0x%08x\n",
			DPRX_IP_REG_BASE + offset, val_m,
			DPRX_IP_REG_BASE + offset + 4, val_l);

	ret_val = (val_m << 8) | val_l;

	return ret_val;
}

static u32 rtk_dprx_rbus_get_dword(u32 offset)
{
	u32 val_26_24 = 0;
	u32 val_23_16 = 0;
	u32 val_15_8 = 0;
	u32 val_7_0 = 0;
	u32 ret_val = 0;

	regmap_read(g_dprx->ip_reg, offset, &val_26_24);
	regmap_read(g_dprx->ip_reg, offset + 4, &val_23_16);
	regmap_read(g_dprx->ip_reg, offset + 8, &val_15_8);
	regmap_read(g_dprx->ip_reg, offset + 16, &val_7_0);

	if ((val_26_24 > 0xFF) || (val_23_16 > 0xFF))
		dev_err(g_dprx->dev, "[RBUS] unexpected ip_reg value, 0x%08x=0x%08x 0x%08x=0x%08x\n",
			DPRX_IP_REG_BASE + offset, val_26_24,
			DPRX_IP_REG_BASE + offset + 4, val_23_16);

	if ((val_15_8 > 0xFF) || (val_7_0 > 0xFF))
		dev_err(g_dprx->dev, "[RBUS] unexpected ip_reg value, 0x%08x=0x%08x 0x%08x=0x%08x\n",
			DPRX_IP_REG_BASE + offset + 8, val_15_8,
			DPRX_IP_REG_BASE + offset + 16, val_7_0);

	ret_val = (val_26_24 << 24) | (val_23_16 << 16) | (val_15_8 << 8) | val_7_0;

	return ret_val;
}

static void rtk_dprx_dump_ip_reg(struct rtk_dprx *dprx,
		u32 start_offset, u32 end_offset)
{
	u32 offset;
	u32 reg_val = 0;

	dev_err(dprx->dev, "Dump dprx registers from offset 0x%x to 0x%x\n",
		start_offset, end_offset);

	for (offset = start_offset; offset <= end_offset; offset += 4) {
		usleep_range(100, 200);
		regmap_read(dprx->ip_reg, offset, &reg_val);
		dev_info(dprx->dev, "Read reg0x%08x = 0x%08x\n",
			DPRX_IP_REG_BASE + offset, reg_val);
	}
}

static const struct rtk_dprx_rbus_ops dprx_rbus_ops = {
	.read = rtk_dprx_rbus_read,
	.write = rtk_dprx_rbus_write,
	.mask_write = rtk_dprx_rbus_mask_write,
	.dump_dprx_reg = rtk_dprx_dump_dprx_reg,
	.set_byte = rtk_dprx_rbus_set_byte,
	.get_byte = rtk_dprx_rbus_get_byte,
	.set_byte_extint = rtk_dprx_rbus_set_byte,
	.get_byte_extint = rtk_dprx_rbus_get_byte,
	.set_bit = rtk_dprx_rbus_set_bit,
	.get_bit = rtk_dprx_rbus_get_bit,
	.set_bit_extint = rtk_dprx_rbus_set_bit,
	.get_bit_extint = rtk_dprx_rbus_get_bit,
	.get_word = rtk_dprx_rbus_get_word,
	.get_dword = rtk_dprx_rbus_get_dword,
	.dump_ip_reg = rtk_dprx_dump_ip_reg,
};

int rtk_dprx_rbus_init(struct rtk_dprx *dprx)
{
	g_dprx = dprx;
	dprx->rbus_ops = &dprx_rbus_ops;

	return 0;
}
