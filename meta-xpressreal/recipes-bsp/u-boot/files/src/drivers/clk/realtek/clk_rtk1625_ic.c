/*
 *
 * Copyright (C) 2025 Realtek Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <log.h>
#include <dm.h>
#include <common.h>
#include <dt-bindings/clock/rtd1625-clk.h>
#include "clk_rtk.h"

static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_p4, 0, 0x08c, 0, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_p3, 0, 0x08c, 1, 0);
static CLK_REGMAP_GATE(clk_en_misc_cec0, "clk_en_misc", 0, 0x08c, 2, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cbusrx_sys, 0, 0x08c, 3, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cbustx_sys, 0, 0x08c, 4, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cbus_sys, 0, 0x08c, 5, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cbus_osc, 0, 0x08c, 6, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_i2c0, 0, 0x08c, 9, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_i2c1, 0, 0x08c, 10, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_etn_250m, 0, 0x08c, 11, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_etn_sys, 0, 0x08c, 12, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_drd, 0, 0x08c, 13, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_host, 0, 0x08c, 14, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_u3_host, 0, 0x08c, 15, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb, 0, 0x08c, 16, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_vtc, 0, 0x08c, 17, 0);
static CLK_REGMAP_GATE(clk_en_misc_vfd, "clk_en_misc", 0, 0x08c, 18, 0);


static const struct rtk_gate *ic_gates[] = {
	[RTD1625_ISO_CLK_EN_USB_P4] = &clk_en_usb_p4,
	[RTD1625_ISO_CLK_EN_USB_P3] = &clk_en_usb_p3,
	[RTD1625_ISO_CLK_EN_MISC_CEC0] = &clk_en_misc_cec0,
	[RTD1625_ISO_CLK_EN_CBUSRX_SYS] = &clk_en_cbusrx_sys,
	[RTD1625_ISO_CLK_EN_CBUSTX_SYS] = &clk_en_cbustx_sys,
	[RTD1625_ISO_CLK_EN_CBUS_SYS] = &clk_en_cbus_sys,
	[RTD1625_ISO_CLK_EN_CBUS_OSC] = &clk_en_cbus_osc,
	[RTD1625_ISO_CLK_EN_I2C0] = &clk_en_i2c0,
	[RTD1625_ISO_CLK_EN_I2C1] = &clk_en_i2c1,
	[RTD1625_ISO_CLK_EN_ETN_250M] = &clk_en_etn_250m,
	[RTD1625_ISO_CLK_EN_ETN_SYS] = &clk_en_etn_sys,
	[RTD1625_ISO_CLK_EN_USB_DRD] = &clk_en_usb_drd,
	[RTD1625_ISO_CLK_EN_USB_HOST] = &clk_en_usb_host,
	[RTD1625_ISO_CLK_EN_USB_U3_HOST] = &clk_en_usb_u3_host,
	[RTD1625_ISO_CLK_EN_USB] = &clk_en_usb,
	[RTD1625_ISO_CLK_EN_VTC] = &clk_en_vtc,
	[RTD1625_ISO_CLK_EN_MISC_VFD] = &clk_en_misc_vfd,
	[RTD1625_ISO_CLK_MAX] = NULL,
};


static const struct udevice_id clk_rtk_ctrl_ids[] = {
	{.compatible = "realtek,rtd1625-iso-clk"},
	{},
};

static int clk_rtk_ctrl_probe(struct udevice *dev)
{
	struct clk_rtk_ctrl_priv *priv = dev_get_priv(dev);

	priv->reg_base = dev_read_addr_ptr(dev);
	if (!priv->reg_base)
		return -ENOENT;

	priv->gates = (struct rtk_gate **) ic_gates;
	priv->gate_count = RTD1625_ISO_CLK_MAX;

	return 0;
}


U_BOOT_DRIVER(clk_rtk_ctrl_ic) = {
	.name = "rtk_ctrl_ic",
	.id = UCLASS_CLK,
	.of_match = clk_rtk_ctrl_ids,
	.ops = &clk_rtk_ctrl_ops,
	.probe = clk_rtk_ctrl_probe,
	.priv_auto = sizeof(struct clk_rtk_ctrl_priv),
};