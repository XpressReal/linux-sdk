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
#include <linux/mfd/syscon.h>

#define AUT_RCAM_REG_OFFSET	0x460

struct autrcam_device {
	struct device *dev;
	struct regmap *state_base;
};

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct autrcam_device *autrcam = dev_get_drvdata(dev);
	unsigned long stat;

	if (kstrtoul(buf, 0, &stat))
		return -EINVAL;

	regmap_write(autrcam->state_base, AUT_RCAM_REG_OFFSET, (unsigned int)stat);

	return count;
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct autrcam_device *autrcam = dev_get_drvdata(dev);
	unsigned int stat;

	regmap_read(autrcam->state_base, AUT_RCAM_REG_OFFSET, &stat);
	snprintf(buf, 12, "0x%08x\n", stat);
	return strnlen(buf, PAGE_SIZE);
}
DEVICE_ATTR_RW(state);

static struct attribute *autrcam_attrs[] = {
	&dev_attr_state.attr,
	NULL
};

static struct attribute_group autrcam_attr_group = {
	.name = "autrcam",
	.attrs = autrcam_attrs,
};

static int autrcam_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct autrcam_device *autrcam;
	int ret = 0;

	autrcam = devm_kzalloc(dev, sizeof(*autrcam), GFP_KERNEL);
	if (!autrcam)
		return -ENOMEM;
	autrcam->dev = dev;

	autrcam->state_base = syscon_regmap_lookup_by_phandle(np, "reg-base");
	if (IS_ERR_OR_NULL(autrcam->state_base)) {
		dev_err(dev, "no reg base found\n");
		return -ENODEV;
	}

	ret = sysfs_create_group(&dev->kobj, &autrcam_attr_group);
	if (ret) {
		dev_err(dev, "failed to create sysfs group: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, autrcam);

	return 0;
}

static int autrcam_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	sysfs_remove_group(&pdev->dev.kobj, &autrcam_attr_group);
	return 0;
}

static const struct of_device_id autrcam_match[] = {
	{.compatible = "realtek,autrcam"},
	{}
};
MODULE_DEVICE_TABLE(of, autrcam_match);

static struct platform_driver autrcam_driver = {
	.probe  = autrcam_probe,
	.remove = autrcam_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "rtk-autrcam",
		.of_match_table = of_match_ptr(autrcam_match),
	},
};
module_platform_driver(autrcam_driver);

MODULE_DESCRIPTION("Realtek Automotive Rear-CAM driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-autrcam");
