// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/rtd1635-clk.h>
#include "common.h"

static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_p4, CLK_IGNORE_UNUSED, 0x08c, 0, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_p3, CLK_IGNORE_UNUSED, 0x08c, 1, 0);
static CLK_REGMAP_GATE(clk_en_misc_cec0, "clk_en_misc", CLK_IGNORE_UNUSED, 0x08c, 2, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cbusrx_sys, CLK_IGNORE_UNUSED, 0x08c, 3, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cbustx_sys, CLK_IGNORE_UNUSED, 0x08c, 4, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cbus_sys, CLK_IGNORE_UNUSED, 0x08c, 5, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cbus_osc, CLK_IGNORE_UNUSED, 0x08c, 6, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_i2c0, CLK_IGNORE_UNUSED, 0x08c, 9, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_i2c1, CLK_IGNORE_UNUSED, 0x08c, 10, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_etn_250m, CLK_IGNORE_UNUSED, 0x08c, 11, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_etn_sys, CLK_IGNORE_UNUSED, 0x08c, 12, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_drd, CLK_IGNORE_UNUSED, 0x08c, 13, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_u3_host, CLK_IGNORE_UNUSED, 0x08c, 14, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb_host, CLK_IGNORE_UNUSED, 0x08c, 15, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_usb, CLK_IGNORE_UNUSED, 0x08c, 16, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_vtc, CLK_IGNORE_UNUSED, 0x08c, 17, 0);
static CLK_REGMAP_GATE(clk_en_misc_vfd, "clk_en_misc", CLK_IGNORE_UNUSED, 0x08c, 18, 0);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_typec, CLK_IGNORE_UNUSED, 0x08c, 19, 0);

static struct clk_regmap *rtd1635_clk_regmap_list[] = {
	&clk_en_usb_p4.clkr,
	&clk_en_usb_p3.clkr,
	&clk_en_misc_cec0.clkr,
	&clk_en_cbusrx_sys.clkr,
	&clk_en_cbustx_sys.clkr,
	&clk_en_cbus_sys.clkr,
	&clk_en_cbus_osc.clkr,
	&clk_en_i2c0.clkr,
	&clk_en_i2c1.clkr,
	&clk_en_etn_250m.clkr,
	&clk_en_etn_sys.clkr,
	&clk_en_usb_drd.clkr,
	&clk_en_usb_host.clkr,
	&clk_en_usb_u3_host.clkr,
	&clk_en_usb.clkr,
	&clk_en_vtc.clkr,
	&clk_en_misc_vfd.clkr,
	&clk_en_typec.clkr
};

static struct clk_hw_onecell_data rtd1635_iso_clk_data = {
	.num = RTD1635_ISO_CLK_MAX,
	.hws = {
		[RTD1635_ISO_CLK_EN_USB_P4] = &__clk_regmap_gate_hw(&clk_en_usb_p4),
		[RTD1635_ISO_CLK_EN_USB_P3] = &__clk_regmap_gate_hw(&clk_en_usb_p3),
		[RTD1635_ISO_CLK_EN_MISC_CEC0] = &__clk_regmap_gate_hw(&clk_en_misc_cec0),
		[RTD1635_ISO_CLK_EN_CBUSRX_SYS] = &__clk_regmap_gate_hw(&clk_en_cbusrx_sys),
		[RTD1635_ISO_CLK_EN_CBUSTX_SYS] = &__clk_regmap_gate_hw(&clk_en_cbustx_sys),
		[RTD1635_ISO_CLK_EN_CBUS_SYS] = &__clk_regmap_gate_hw(&clk_en_cbus_sys),
		[RTD1635_ISO_CLK_EN_CBUS_OSC] = &__clk_regmap_gate_hw(&clk_en_cbus_osc),
		[RTD1635_ISO_CLK_EN_I2C0] = &__clk_regmap_gate_hw(&clk_en_i2c0),
		[RTD1635_ISO_CLK_EN_I2C1] = &__clk_regmap_gate_hw(&clk_en_i2c1),
		[RTD1635_ISO_CLK_EN_ETN_250M] = &__clk_regmap_gate_hw(&clk_en_etn_250m),
		[RTD1635_ISO_CLK_EN_ETN_SYS] = &__clk_regmap_gate_hw(&clk_en_etn_sys),
		[RTD1635_ISO_CLK_EN_USB_DRD] = &__clk_regmap_gate_hw(&clk_en_usb_drd),
		[RTD1635_ISO_CLK_EN_USB_U3_HOST] = &__clk_regmap_gate_hw(&clk_en_usb_u3_host),
		[RTD1635_ISO_CLK_EN_USB_HOST] = &__clk_regmap_gate_hw(&clk_en_usb_host),
		[RTD1635_ISO_CLK_EN_USB] = &__clk_regmap_gate_hw(&clk_en_usb),
		[RTD1635_ISO_CLK_EN_VTC] = &__clk_regmap_gate_hw(&clk_en_vtc),
		[RTD1635_ISO_CLK_EN_MISC_VFD] = &__clk_regmap_gate_hw(&clk_en_misc_vfd),
		[RTD1635_ISO_CLK_EN_TYPEC] = &__clk_regmap_gate_hw(&clk_en_typec),
		[RTD1635_ISO_CLK_MAX] = NULL,
	},
};

static struct rtk_reset_bank rtd1635_iso_reset_banks[] = {
	{ .ofs = 0x88, },
};

static const struct rtk_clk_desc rtd1635_iso_desc = {
	.clk_data        = &rtd1635_iso_clk_data,
	.clks            = rtd1635_clk_regmap_list,
	.num_clks        = ARRAY_SIZE(rtd1635_clk_regmap_list),
	.reset_banks     = rtd1635_iso_reset_banks,
	.num_reset_banks = ARRAY_SIZE(rtd1635_iso_reset_banks),
};

static CLK_REGMAP_GATE_NO_PARENT(clk_en_irda, CLK_IGNORE_UNUSED, 0x314, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_ur10, CLK_IGNORE_UNUSED, 0x314, 8, 1);

static struct clk_regmap *rtd1635_iso_s_clk_regmap_list[] = {
	&clk_en_irda.clkr,
	&clk_en_ur10.clkr,
};

static struct clk_hw_onecell_data rtd1635_iso_s_clk_data = {
	.num = RTD1635_ISO_S_CLK_MAX,
	.hws = {
		[RTD1635_ISO_S_CLK_EN_IRDA] = &__clk_regmap_gate_hw(&clk_en_irda),
		[RTD1635_ISO_S_CLK_EN_UR10] = &__clk_regmap_gate_hw(&clk_en_ur10),
		[RTD1635_ISO_S_CLK_MAX] = NULL,
	},
};

static struct rtk_reset_bank rtd1635_iso_s_reset_banks[] = {
	{ .ofs = 0x310, .write_en = 1, },
};

static const struct rtk_clk_desc rtd1635_iso_s_desc = {
	.clk_data        = &rtd1635_iso_s_clk_data,
	.clks            = rtd1635_iso_s_clk_regmap_list,
	.num_clks        = ARRAY_SIZE(rtd1635_iso_s_clk_regmap_list),
	.reset_banks     = rtd1635_iso_s_reset_banks,
	.num_reset_banks = ARRAY_SIZE(rtd1635_iso_s_reset_banks),
};

static int rtd1635_iso_probe(struct platform_device *pdev)
{
	const struct rtk_clk_desc *desc;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	return rtk_clk_probe(pdev, desc);
}

static const struct of_device_id rtd1635_iso_match[] = {
	{ .compatible = "realtek,rtd1635-iso-clk", .data = &rtd1635_iso_desc },
	{ .compatible = "realtek,rtd1635-iso-s-clk", .data = &rtd1635_iso_s_desc },
	{ /* sentinel */ }
};

static struct platform_driver rtd1635_iso_driver = {
	.probe = rtd1635_iso_probe,
	.driver = {
		.name = "rtk-rtd1635-iso-clk",
		.of_match_table = rtd1635_iso_match,
	},
};

static int __init rtd1635_iso_init(void)
{
	return platform_driver_register(&rtd1635_iso_driver);
}
subsys_initcall(rtd1635_iso_init);

MODULE_DESCRIPTION("Realtek RTD1635 ISO Controller Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
