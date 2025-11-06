// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025 Realtek Semiconductor Corporation
 */

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "common.h"

#define DIV_DV(_r, _d, _v)    { .rate = _r, .div = _d, .val = _v, }
#define FREQ_NF_MASK          (0x7FFFF)
#define FREQ_NF(_r, _n, _f)   { .rate = _r, .val = ((_n) << 11) | (_f), }

struct rtd1619b_scpu_clk_data {
	struct regmap *regmap;
	struct clk_pll_div *pll_data;
};

static const struct div_table scpu_div_tbl[] = {
	DIV_DV(1000000000,  1, 0x00),
	DIV_DV(500000000,   2, 0x88),
	DIV_DV(250000000,   4, 0x90),
	DIV_DV(200000000,   8, 0xA0),
	DIV_DV(100000000,  10, 0xA8),
	DIV_TABLE_END
};

static const struct freq_table scpu_tbl[] = {
	FREQ_NF(1000000000, 34,   75),
	FREQ_NF(1100000000, 37, 1517),
	FREQ_NF(1200000000, 41,  910),
	FREQ_NF(1200000000, 41,    0),
	FREQ_NF(1300000000, 45,  303),
	FREQ_NF(1400000000, 48, 1745),
	FREQ_NF(1500000000, 52, 1137),
	FREQ_NF(1600000000, 56,  530),
	FREQ_NF(1700000000, 60,    0),
	FREQ_NF(1800000000, 63, 1365),
	FREQ_NF(1900000000, 67,  758),
	FREQ_NF(2000000000, 71,  151),
	FREQ_TABLE_END
};

static struct clk_pll_div pll_scpu = {
	.div_ofs = 0x108,
	.div_shift  = 8,
	.div_width  = 8,
	.div_tbl    = scpu_div_tbl,
	.clkp       = {
		.ssc_ofs   = 0x500,
		.pll_ofs   = CLK_OFS_INVALID,
		.pll_type  = CLK_PLL_TYPE_NF_SSC,
		.freq_tbl  = scpu_tbl,
		.freq_mask = FREQ_NF_MASK,
		.clkr.hw.init = CLK_HW_INIT("pll_scpu", "osc27m", &clk_pll_div_ops,
					    CLK_IGNORE_UNUSED | CLK_GET_RATE_NOCACHE),
	},
};

static int rtd1619b_scpu_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct rtd1619b_scpu_clk_data *data;
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
	pll_scpu.clkp.clkr.regmap = data->regmap;

	ret = devm_clk_hw_register(dev, &pll_scpu.clkp.clkr.hw);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register clk\n");

	platform_set_drvdata(pdev, data);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_simple_get, &pll_scpu.clkp.clkr.hw);
}

static const struct of_device_id rtd1619b_scpu_clk_match[] = {
	{ .compatible = "realtek,rtd1619b-scpu-clk" },
	{ /* sentinel */ }
};

static struct platform_driver rtd1619b_scpu_clk_driver = {
	.probe = rtd1619b_scpu_clk_probe,
	.driver = {
		.name = "rtk-rtd1619b-scpu-clk",
		.of_match_table = rtd1619b_scpu_clk_match,
	},
};
module_platform_driver(rtd1619b_scpu_clk_driver);

MODULE_DESCRIPTION("Reatek RTD1619B CRT PLL_SCPU Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
