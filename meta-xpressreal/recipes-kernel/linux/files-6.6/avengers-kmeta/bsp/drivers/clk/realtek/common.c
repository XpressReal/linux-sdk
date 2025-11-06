// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/hwspinlock.h>
#include <linux/platform_device.h>
#include "common.h"

MODULE_LICENSE("GPL v2");

void clk_regmap_write(struct clk_regmap *clkr, u32 ofs, u32 val)
{
	pr_debug("%s: ofs=%03x, val=%08x\n", __func__, ofs, val);

	regmap_write(clkr->regmap, ofs, val);
}
EXPORT_SYMBOL_GPL(clk_regmap_write);

u32 clk_regmap_read(struct clk_regmap *clkr, u32 ofs)
{
	u32 val = 0;

	regmap_read(clkr->regmap, ofs, &val);
	pr_debug("%s: ofs=%03x, val=%08x\n", __func__, ofs, val);
	return val;
}
EXPORT_SYMBOL_GPL(clk_regmap_read);

void clk_regmap_update_bits(struct clk_regmap *clkr, u32 ofs, u32 mask, u32 val)
{
	pr_debug("%s: ofs=%03x, mask=%08x, val=%08x\n", __func__, ofs, mask, val);

	regmap_update_bits(clkr->regmap, ofs, mask, val);
}
EXPORT_SYMBOL_GPL(clk_regmap_update_bits);

struct rtk_clk_drvdata {
	const struct rtk_clk_desc *desc;
	struct regmap *regmap;
};

static int rtk_clk_setup_map(struct platform_device *pdev, struct rtk_clk_drvdata *data)
{
	data->regmap = device_node_to_regmap(pdev->dev.of_node);
	return PTR_ERR_OR_ZERO(data->regmap);
}

int rtk_clk_probe(struct platform_device *pdev, const struct rtk_clk_desc *desc)
{
	int i;
	struct device *dev = &pdev->dev;
	int ret;
	struct rtk_reset_initdata reset_initdata = {0};
	struct rtk_clk_drvdata *data;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->desc = desc;
	ret = rtk_clk_setup_map(pdev, data);
	if (ret)
		return ret;

	for (i = 0; i < desc->num_clks; i++)
		desc->clks[i]->regmap = data->regmap;

	for (i = 0; i < desc->clk_data->num; i++) {
		struct clk_hw *hw = desc->clk_data->hws[i];
		if (!hw)
			continue;
		ret = devm_clk_hw_register(dev, hw);
		if (ret)
			dev_warn(dev, "failed to register hw of clk%d: %d\n", i, ret);
	}

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, desc->clk_data);
	if (ret)
		return ret;

	if (!desc->num_reset_banks)
		return 0;

	reset_initdata.regmap = data->regmap;
	reset_initdata.num_banks = desc->num_reset_banks;
	reset_initdata.banks = desc->reset_banks;
	return rtk_reset_controller_add(dev, &reset_initdata);
}
EXPORT_SYMBOL_GPL(rtk_clk_probe);
