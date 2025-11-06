// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek SPD Driver
 *
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 */

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/slab.h>

struct rtk_spd_drvdata {
	struct generic_pm_domain genpd;
	struct reset_control *rstcs;
	struct clk_bulk_data *clks;
	int num_clks;
	struct regmap *pwr_map;
	u32 pwr_reg_offset;
	u32 pwr_mask;
	u32 pwr_val_on;
	u32 pwr_val_off;
};

static int rtk_spd_genpd_power_on(struct generic_pm_domain *genpd)
{
	struct rtk_spd_drvdata *data = container_of(genpd, struct rtk_spd_drvdata, genpd);
	int ret;

	if (data->pwr_map)
		regmap_update_bits(data->pwr_map, data->pwr_reg_offset, data->pwr_mask,
				   data->pwr_val_on);

	ret = reset_control_deassert(data->rstcs);
	if (ret)
		goto pwr_off;
	ret = clk_bulk_prepare_enable(data->num_clks, data->clks);
	if (ret)
		goto rstc_assert;
	return 0;

rstc_assert:
	reset_control_assert(data->rstcs);
pwr_off:
	if (data->pwr_map)
		regmap_update_bits(data->pwr_map, data->pwr_reg_offset, data->pwr_mask,
				   data->pwr_val_off);
	return ret;
}

static int rtk_spd_genpd_power_off(struct generic_pm_domain *genpd)
{
	struct rtk_spd_drvdata *data = container_of(genpd, struct rtk_spd_drvdata, genpd);

	clk_bulk_disable_unprepare(data->num_clks, data->clks);
	reset_control_assert(data->rstcs);
	if (data->pwr_map)
		regmap_update_bits(data->pwr_map, data->pwr_reg_offset, data->pwr_mask,
				   data->pwr_val_off);
	return 0;
}

static int rtk_spd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_spd_drvdata *data;
	int ret;
	u32 val[4] = {};
	const char *name;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->num_clks = devm_clk_bulk_get_all(dev, &data->clks);
	if (data->num_clks < 0)
		return dev_err_probe(dev, data->num_clks, "failed to get clks\n");

	data->rstcs = devm_reset_control_array_get_optional_exclusive(dev);
	if (IS_ERR(data->rstcs))
		return dev_err_probe(dev, PTR_ERR(data->rstcs), "failed to get reset controls\n");

	data->pwr_map = syscon_regmap_lookup_by_phandle_args(dev->of_node, "pwr-cfg", 4, val);
	if (IS_ERR(data->pwr_map)) {
		ret = PTR_ERR(data->pwr_map);
		if (ret != -ENOENT)
			return dev_err_probe(dev, ret, "failed to get pwr-cfg\n");
		data->pwr_map = NULL;
	} else {
		data->pwr_reg_offset = val[0];
		data->pwr_mask = val[1];
		data->pwr_val_on = val[2];
		data->pwr_val_off = val[3];
	}

	ret = of_property_read_string(dev->of_node, "label", &name);
	if (ret)
		name = dev_name(dev);

	data->genpd.name = name;
	data->genpd.power_on = rtk_spd_genpd_power_on;
	data->genpd.power_off = rtk_spd_genpd_power_off;
	ret = pm_genpd_init(&data->genpd, NULL, 1);
	if (ret) {
		dev_err(dev, "failed to init genpd: %d\n", ret);
		return ret;
	}

	ret = of_genpd_add_provider_simple(dev->of_node, &data->genpd);
	if (ret) {
		dev_err(dev, "failed to add genpd of provider: %d\n", ret);
		pm_genpd_remove(&data->genpd);
	}
	return 0;
}

static const struct of_device_id rtk_spd_match[] = {
	{ .compatible = "realtek,simple-pd", },
	{}
};

static struct platform_driver rtk_spd_driver = {
	.probe = rtk_spd_probe,
	.driver = {
		.name = "rtk-spd",
		.of_match_table = of_match_ptr(rtk_spd_match),
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(rtk_spd_driver);

MODULE_DESCRIPTION("Realtek SPD Driver");
MODULE_LICENSE("GPL v2");
