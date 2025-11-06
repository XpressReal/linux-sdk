// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 */

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "common.h"

struct rtd1625_scpu_clk_data {
	struct regmap *regmap;
	struct clk_pll2 *pll_data;
};

#define RTD1625_REG_PLL_SSC_DIG_SCPU0      0x500
#define RTD1625_REG_PLL_SSC_DIG_SCPU1      0x504
#define RTD1625_REG_PLL_SSC_DIG_SCPU_DBG2  0x51c

static struct reg_sequence pll_scpu_seq_set_freq[] = {
	{RTD1625_REG_PLL_SSC_DIG_SCPU0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_SCPU1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_SCPU0, 0x00000005},
};

static const struct freq_table scpu_freq_table[] = {
	{ .rate =  800000000UL, .val = 0x0000cd09,},
	{ .rate =  900000000UL, .val = 0x0000eaaa,},
	{ .rate = 1000000000UL, .val = 0x0001084b,},
	{ .rate = 1100000000UL, .val = 0x000125ed,},
	{ .rate = 1200000000UL, .val = 0x0001438e,},
	{ .rate = 1300000000UL, .val = 0x0001612f,},
	{ .rate = 1400000000UL, .val = 0x00017ed0,},
	{ .rate = 1500000000UL, .val = 0x00019c71,},
	{ .rate = 1600000000UL, .val = 0x0001ba12,},
	{ .rate = 1700000000UL, .val = 0x0001d7b4,},
	{ .rate = 1800000000UL, .val = 0x0001f555,},
	{ .rate = 1900000000UL, .val = 0x000212f6,},
	{ .rate = 2000000000UL, .val = 0x00023097,},
	{ .rate = 2100000000UL, .val = 0x00024e38,},
	{ .rate = 2200000000UL, .val = 0x00026bda,},
	{ .rate = 2300000000UL, .val = 0x0002897b,},
	{}
};

static struct clk_pll2 pll_scpu = {
        .clkr.hw.init = CLK_HW_INIT("pll_scpu", "osc27m", &clk_pll2_ops,
                                    CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL),
        .seq_set_freq = pll_scpu_seq_set_freq,
        .num_seq_set_freq = ARRAY_SIZE(pll_scpu_seq_set_freq),
        .freq_reg = RTD1625_REG_PLL_SSC_DIG_SCPU1,
        .freq_tbl  = scpu_freq_table,
        .freq_mask = 0x7ffff,
        .freq_ready_reg = RTD1625_REG_PLL_SSC_DIG_SCPU_DBG2,
        .freq_ready_mask = BIT(20),
        .freq_ready_val = BIT(20),
};

static int rtd1625_scpu_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct rtd1625_scpu_clk_data *data;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	np = of_parse_phandle(pdev->dev.of_node, "realtek,cc", 0);
	if (!np)
		return -EINVAL;

	data->regmap = device_node_to_regmap(np);
	if (IS_ERR(data->regmap))
		return dev_err_probe(dev, PTR_ERR(data->regmap), "failed to get cc regmap\n");

	data->pll_data = &pll_scpu;
	pll_scpu.clkr.regmap = data->regmap;

	ret = devm_clk_hw_register(dev, &pll_scpu.clkr.hw);
        if (ret)
		return dev_err_probe(dev, ret, "failed to register clk\n");

	platform_set_drvdata(pdev, data);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_simple_get, &pll_scpu.clkr.hw);
}

static const struct of_device_id rtd1625_scpu_clk_match[] = {
	{ .compatible = "realtek,rtd1625-scpu-clk" },
	{ /* sentinel */ }
};

static struct platform_driver rtd1625_scpu_clk_driver = {
	.probe = rtd1625_scpu_clk_probe,
	.driver = {
		.name = "rtk-rtd1625-scpu-clk",
		.of_match_table = rtd1625_scpu_clk_match,
	},
};
module_platform_driver(rtd1625_scpu_clk_driver);

MODULE_DESCRIPTION("Reatek RTD1625 CRT PLL_SCPU Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
