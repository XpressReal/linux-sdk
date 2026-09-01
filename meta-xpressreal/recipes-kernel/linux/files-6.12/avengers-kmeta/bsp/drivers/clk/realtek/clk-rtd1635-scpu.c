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

struct rtd1635_scpu_clk_data {
	struct regmap *regmap;
	struct clk *pll_scpu;
	struct notifier_block pll_scpu_nb;
	struct clk *pll_big_scpu;
	struct notifier_block pll_big_scpu_nb;
};

#define pll2_to_clk_hw(p)   (&((p)->clkr.hw))
#define pll2_to_clk(p)      (pll2_to_clk_hw(p)->clk)

#define RTD1635_REG_PLL_SCPU1                  0x104
#define RTD1635_REG_PLL_SCPU4                  0x11c
#define RTD1635_REG_PLL_BIG_SCPU1              0x6d4
#define RTD1635_REG_PLL_SSC_DIG_SCPU0          0x500
#define RTD1635_REG_PLL_SSC_DIG_SCPU1          0x504
#define RTD1635_REG_PLL_SSC_DIG_SCPU_DBG2      0x51c
#define RTD1635_REG_PLL_SSC_DIG_BIG_SCPU0      0x600
#define RTD1635_REG_PLL_SSC_DIG_BIG_SCPU1      0x604
#define RTD1635_REG_PLL_SSC_DIG_BIG_SCPU_DBG2  0x61c

/* RTD1635_REG_PLL_SCPU4 Mask & Sel */
#define SCPU_CORE_SEL_MASK             BIT(0)
#define SCPU_BIG_CORE_SEL_MASK         BIT(1)
#define SCPU_DSU_SEL_MASK              BIT(2)
#define SCPU_CORE_SEL_PLL_SCPU           (0 << 0)
#define SCPU_CORE_SEL_PLL_BIG_SCPU       (1 << 0)
#define SCPU_BIG_CORE_SEL_PLL_SCPU       (1 << 1)
#define SCPU_BIG_CORE_SEL_PLL_BIG_SCPU   (0 << 1)
#define SCPU_DSU_SEL_PLL_SCPU            (0 << 2)
#define SCPU_DSU_SEL_PLL_BIG_SCPU        (1 << 2)

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
	{ .rate = 1650000000UL, .val = 0x0001c8e3,},
	{ .rate = 1700000000UL, .val = 0x0001d7b4,},
	{ .rate = 1800000000UL, .val = 0x0001f555,},
	{ .rate = 1900000000UL, .val = 0x000212f6,},
	{ .rate = 2000000000UL, .val = 0x00023097,},
	{ .rate = 2100000000UL, .val = 0x00024e38,},
	{ .rate = 2200000000UL, .val = 0x00026bda,},
	{ .rate = 2300000000UL, .val = 0x0002897b,},
	{}
};

static struct reg_sequence pll_scpu_seq_set_freq[] = {
	{RTD1635_REG_PLL_SSC_DIG_SCPU0, 0x00000004},
	{RTD1635_REG_PLL_SSC_DIG_SCPU1, 0x00000000},
	{RTD1635_REG_PLL_SSC_DIG_SCPU0, 0x00000005},
};

static struct clk_pll2 pll_scpu = {
	.clkr.hw.init = CLK_HW_INIT("pll_scpu", "osc27m", &clk_pll2_ops,
				    CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL),
	.seq_set_freq = pll_scpu_seq_set_freq,
	.num_seq_set_freq = ARRAY_SIZE(pll_scpu_seq_set_freq),
	.freq_reg = RTD1635_REG_PLL_SSC_DIG_SCPU1,
	.freq_tbl  = scpu_freq_table,
	.freq_mask = 0x7ffff,
	.freq_ready_reg = RTD1635_REG_PLL_SSC_DIG_SCPU_DBG2,
	.freq_ready_mask = BIT(20),
	.freq_ready_val = BIT(20),
};

static struct reg_sequence pll_big_scpu_seq_set_freq[] = {
	{RTD1635_REG_PLL_SSC_DIG_BIG_SCPU0, 0x00000004},
	{RTD1635_REG_PLL_SSC_DIG_BIG_SCPU1, 0x00000000},
	{RTD1635_REG_PLL_SSC_DIG_BIG_SCPU0, 0x00000005},
};

static struct clk_pll2 pll_big_scpu = {
	.clkr.hw.init = CLK_HW_INIT("pll_big_scpu", "osc27m", &clk_pll2_ops,
				    CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL),
	.seq_set_freq = pll_big_scpu_seq_set_freq,
	.num_seq_set_freq = ARRAY_SIZE(pll_big_scpu_seq_set_freq),
	.freq_reg = RTD1635_REG_PLL_SSC_DIG_BIG_SCPU1,
	.freq_tbl  = scpu_freq_table,
	.freq_mask = 0x7ffff,
	.freq_ready_reg = RTD1635_REG_PLL_SSC_DIG_BIG_SCPU_DBG2,
	.freq_ready_mask = BIT(20),
	.freq_ready_val = BIT(20),
};

static struct clk_hw_onecell_data rtd1635_clk_hw_data = {
	.num = 2,
	.hws = {
		[0] = &pll_scpu.clkr.hw,
		[1] = &pll_big_scpu.clkr.hw,
	},
};

static void rtd1635_set_scpu_sel(struct rtd1635_scpu_clk_data *drvdata, u32 mask, u32 val)
{
	regmap_update_bits(drvdata->regmap, RTD1635_REG_PLL_SCPU4, mask, val);
}

static void rtd1635_update_dsu_sel(struct rtd1635_scpu_clk_data *drvdata,
				   unsigned long pll_scpu_rate, unsigned long pll_scpu_big_rate)
{
	unsigned long pll_scpu_dsu_rate = pll_scpu_rate, pll_scpu_big_dsu_rate = pll_scpu_big_rate;

	do_div(pll_scpu_dsu_rate, 5);
	pll_scpu_dsu_rate *= 4;

	do_div(pll_scpu_big_dsu_rate, 3);
	pll_scpu_big_dsu_rate *= 2;

	if (pll_scpu_dsu_rate > pll_scpu_big_dsu_rate)
		rtd1635_set_scpu_sel(drvdata, SCPU_DSU_SEL_MASK, SCPU_DSU_SEL_PLL_SCPU);
	else
		rtd1635_set_scpu_sel(drvdata, SCPU_DSU_SEL_MASK, SCPU_DSU_SEL_PLL_BIG_SCPU);
}

static int rtd1635_pll_scpu_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct rtd1635_scpu_clk_data *drvdata = container_of(nb, struct rtd1635_scpu_clk_data,
							     pll_scpu_nb);
	struct clk_notifier_data *cnd = data;
	unsigned long pll_scpu_big_rate = clk_get_rate(drvdata->pll_big_scpu);
	u32 val;

	regmap_read(drvdata->regmap, RTD1635_REG_PLL_BIG_SCPU1, &val);
	if (val != 0xb)
		return NOTIFY_DONE;

	switch (event) {
	case POST_RATE_CHANGE:
		rtd1635_update_dsu_sel(drvdata, cnd->new_rate, pll_scpu_big_rate);
		return NOTIFY_OK;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int rtd1635_pll_big_scpu_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct rtd1635_scpu_clk_data *drvdata = container_of(nb, struct rtd1635_scpu_clk_data,
							     pll_big_scpu_nb);
	struct clk_notifier_data *cnd = data;
	unsigned long pll_scpu_rate = clk_get_rate(drvdata->pll_scpu);
	u32 val;

	regmap_read(drvdata->regmap, RTD1635_REG_PLL_SCPU1, &val);
	if (val != 0xb)
		return NOTIFY_DONE;

	switch (event) {
	case POST_RATE_CHANGE:
		rtd1635_update_dsu_sel(drvdata, pll_scpu_rate, cnd->new_rate);
		return NOTIFY_OK;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int rtd1635_scpu_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct rtd1635_scpu_clk_data *data;
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

	pll_scpu.clkr.regmap = data->regmap;
	ret = devm_clk_hw_register(dev, pll2_to_clk_hw(&pll_scpu));
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to register pll_scpu\n");

	pll_big_scpu.clkr.regmap = data->regmap;
	ret = devm_clk_hw_register(dev, pll2_to_clk_hw(&pll_big_scpu));
	if (ret)
		return dev_err_probe(dev, ret, "failed to register pll_big_scpu\n");

	data->pll_scpu_nb.notifier_call = rtd1635_pll_scpu_cb;
	data->pll_scpu = pll2_to_clk(&pll_scpu);
	data->pll_big_scpu_nb.notifier_call = rtd1635_pll_big_scpu_cb;
	data->pll_big_scpu = pll2_to_clk(&pll_big_scpu);

	ret = devm_clk_notifier_register(dev, data->pll_scpu, &data->pll_scpu_nb);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to register pll_scpu clk_notifier\n");

	ret = devm_clk_notifier_register(dev, data->pll_big_scpu, &data->pll_big_scpu_nb);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to register pll_big_scpu clk_notifier\n");

	platform_set_drvdata(pdev, data);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, &rtd1635_clk_hw_data);
}

static const struct of_device_id rtd1635_scpu_clk_match[] = {
	{ .compatible = "realtek,rtd1635-scpu-clk" },
	{ /* sentinel */ }
};

static struct platform_driver rtd1635_scpu_clk_driver = {
	.probe = rtd1635_scpu_clk_probe,
	.driver = {
		.name = "rtk-rtd1635-scpu-clk",
		.of_match_table = rtd1635_scpu_clk_match,
	},
};
module_platform_driver(rtd1635_scpu_clk_driver);

MODULE_DESCRIPTION("Reatek RTD1635 CRT PLL_SCPU Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
