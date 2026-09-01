// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021,2025 Realtek Semiconductor Corp.
 */

#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/printk.h>
#include <linux/slab.h>

struct rtk_vcpu_data {
	struct device *dev;
	struct clk *clk;
	struct devfreq_dev_profile profile;
	struct devfreq *devfreq;
};

static int rtk_vcpu_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct rtk_vcpu_data *vcpu_data = dev_get_drvdata(dev);
	unsigned long cuf_freq = clk_get_rate(vcpu_data->clk);
	struct dev_pm_opp *opp;

	opp = devfreq_recommended_opp(dev, freq, 0);
	if (IS_ERR(opp)) {
		dev_err(dev, "failed to find recommended opp\n");
		return PTR_ERR(opp);
	}
	dev_pm_opp_put(opp);

	if (cuf_freq == *freq)
		return 0;

	return clk_set_rate(vcpu_data->clk, *freq);
}

static int rtk_vcpu_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct rtk_vcpu_data *vcpu_data = dev_get_drvdata(dev);

	*freq = clk_get_rate(vcpu_data->clk);
	return 0;
}

static int rtk_vcpu_init_devfreq(struct rtk_vcpu_data *vcpu_data)
{
	struct device *dev = vcpu_data->dev;
	struct device_node *np = dev->of_node;
	int ret;

	if (!of_find_property(np, "operating-points-v2", NULL))
		return 0;

	if (!vcpu_data->clk)
		return -EINVAL;

	ret = devm_pm_opp_of_add_table(dev);
	if (ret < 0) {
		dev_err(dev, "failed to get OPP table: %d\n", ret);
		return ret;
	}

	vcpu_data->profile.is_cooling_device = 1;
	vcpu_data->profile.get_cur_freq = rtk_vcpu_get_cur_freq;
	vcpu_data->profile.target       = rtk_vcpu_target;
	vcpu_data->profile.initial_freq = clk_get_rate(vcpu_data->clk);
	vcpu_data->devfreq = devm_devfreq_add_device(dev, &vcpu_data->profile, "performance", NULL);
	return PTR_ERR_OR_ZERO(vcpu_data->devfreq);
}

static int rtk_vcpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_vcpu_data *vcpu_data;
	int ret;

	vcpu_data = devm_kzalloc(dev, sizeof(*vcpu_data), GFP_KERNEL);
	if (!vcpu_data)
		return -ENOMEM;
	vcpu_data->dev = dev;
	platform_set_drvdata(pdev, vcpu_data);

	vcpu_data->clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(vcpu_data->clk))
		return dev_err_probe(dev, PTR_ERR(vcpu_data->clk), "failed to get optional clk\n");

	ret = rtk_vcpu_init_devfreq(vcpu_data);
	if (ret) {
		dev_err(dev, "failed to init devfreq: %d\n", ret);
		return ret;
	}

	return 0;
}

static void rtk_vcpu_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
}

static void rtk_vcpu_shutdown(struct platform_device *pdev)
{
}

static const struct of_device_id rtk_vcpu_match[] = {
	{ .compatible = "realtek,rtd1319-vcpu", },
	{ .compatible = "realtek,rtd1325-vcpu", },
	{}
};

static struct platform_driver rtk_vcpu_driver = {
	.probe    = rtk_vcpu_probe,
	.remove   = rtk_vcpu_remove,
	.shutdown = rtk_vcpu_shutdown,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "rtk-vcpu",
		.of_match_table = of_match_ptr(rtk_vcpu_match),
	},
};
module_platform_driver(rtk_vcpu_driver);

MODULE_DESCRIPTION("Realtek Video CPU driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-vcpu");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
