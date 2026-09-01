// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/regmap.h>


struct ecc_device {
	struct device *dev;
};

static int ecc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ecc_device *ecc;

	ecc = devm_kzalloc(dev, sizeof(*ecc), GFP_KERNEL);
	if (!ecc)
		return -ENOMEM;
	ecc->dev = dev;
	platform_set_drvdata(pdev, ecc);

	return 0;
}

static int ecc_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id realtek_match[] = {
	{.compatible = "realtek,rtd1625-ecc"},
	{}
};
MODULE_DEVICE_TABLE(of, autrcam_match);

static struct platform_driver realtek_ecc_driver = {
	.probe  = ecc_probe,
	.remove = ecc_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "rtk-ecc",
		.of_match_table = of_match_ptr(realtek_match),
	},
};
module_platform_driver(realtek_ecc_driver);

MODULE_DESCRIPTION("Realtek ecc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-ecc");

