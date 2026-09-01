// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "rtk_ddr_dfs.h"


#define __DATA_GRAN		16	/* byte */
#define __DEFAULT_TIME_US	1000
#define __DEFAULT_LOW_DEBOUNCE	5
#define __DEFAULT_HIGH_DEBOUNCE	1
#define __DEFAULT_LOW_BW	(1 << 30)	/* 1 GiB */
#define __DEFAULT_HIGH_BW	(3 << 30)	/* 3 GiB */

#define __BW_TO_ACK(_b)		((_b) / __DATA_GRAN)
#define __ACK_TO_BW(_a)		((_a) * __DATA_GRAN)


static
ssize_t enable_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->enabled);
}

static
int __enable(struct device *dev, int on)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);
	struct arm_smccc_res res;

	if (on && data->dirty) {
		/* send config before enable */
		arm_smccc_smc(DFS_SMCCC_CFG, virt_to_phys(&data->cfg),
			      sizeof(struct rtk_ddr_dfs_data), 0, 0, 0, 0, 0,
			      &res);
		if (res.a0) {
			dev_err(dev, "update configs failed(%ld)\n",
				res.a0);
			goto exit;
		} else {
			data->dirty = 0;
		}
	}

	arm_smccc_smc(DFS_SMCCC_CTRL, on, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		dev_err(dev, "%s failed(%ld)\n", on ? "enabled" : "disabled",
			res.a0);

exit:
	return !!res.a0;
}

static
ssize_t enable_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);
	int ret;
	int tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = !!tmp;
	if (data->enabled == tmp)
		goto exit;

	if (!__enable(dev, tmp))
		data->enabled = tmp;

exit:
	return count;
}

static inline
void __warn_config(struct device *dev, struct rtk_ddr_dfs_data *data)
{
	if (data->enabled)
		dev_warn(dev, "config takes place after re-enabling\n");
}

static
ssize_t time_window_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->cfg.time_us);
}

static
ssize_t time_window_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);
	int ret;
	long tmp;

	ret = kstrtoul(buf, 10, &tmp);
	if (ret)
		return ret;

	if (data->cfg.time_us != tmp) {
		data->dirty = 1;
		data->cfg.time_us = tmp;

		__warn_config(dev, data);
	}

	return count;
}

static
ssize_t low_bw_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%llu\n",
			__ACK_TO_BW((u64)data->cfg.low_ack));
}

static
ssize_t low_bw_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);
	int ret;
	u64 tmp;

	ret = kstrtoull(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = __BW_TO_ACK(tmp);
	if (data->cfg.low_ack != tmp) {
		data->dirty = 1;
		data->cfg.low_ack = tmp;

		__warn_config(dev, data);
	}

	return count;
}

static
ssize_t low_debounce_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%hhu\n", data->cfg.low_debounce);
}

static
ssize_t low_debounce_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);
	int ret;
	u32 tmp;

	ret = kstrtouint(buf, 10, &tmp);
	if (ret)
		return ret;

	if (data->cfg.low_debounce != tmp) {
		data->dirty = 1;
		data->cfg.low_debounce = tmp;

		__warn_config(dev, data);
	}

	return count;
}

static
ssize_t high_bw_show(struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%llu\n",
			__ACK_TO_BW((u64)data->cfg.high_ack));
}

static
ssize_t high_bw_store(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);
	int ret;
	u64 tmp;

	ret = kstrtoull(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = __BW_TO_ACK(tmp);
	if (data->cfg.high_ack != tmp) {
		data->dirty = 1;
		data->cfg.high_ack = tmp;

		__warn_config(dev, data);
	}

	return count;
}

static
ssize_t high_debounce_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%hhu\n", data->cfg.high_debounce);
}

static
ssize_t high_debounce_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);
	int ret;
	u32 tmp;

	ret = kstrtouint(buf, 10, &tmp);
	if (ret)
		return ret;

	if (data->cfg.high_debounce != tmp) {
		data->dirty = 1;
		data->cfg.high_debounce = tmp;

		__warn_config(dev, data);
	}

	return count;
}

static DEVICE_ATTR_RW(enable);
static DEVICE_ATTR_RW(time_window);
static DEVICE_ATTR_RW(high_bw);
static DEVICE_ATTR_RW(high_debounce);
static DEVICE_ATTR_RW(low_bw);
static DEVICE_ATTR_RW(low_debounce);

#define __ATTR_REF(_name)	(&dev_attr_##_name.attr)
static struct attribute *ddr_dfs_attrs[] = {
	__ATTR_REF(enable),
	__ATTR_REF(time_window),
	__ATTR_REF(high_bw),
	__ATTR_REF(high_debounce),
	__ATTR_REF(low_bw),
	__ATTR_REF(low_debounce),
	NULL
};

static struct attribute_group ddr_dfs_attr_group = {
	.name = "hwm",
	.attrs = ddr_dfs_attrs,
};

static
int __pm_suspend(struct device *dev)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);

	return data->enabled ? __enable(dev, 0) : 0;
}

static
int __pm_resume(struct device *dev)
{
	struct rtk_ddr_dfs_data *data = dev_get_drvdata(dev);

	return data->enabled ? __enable(dev, 1) : 0;
}

static inline
void __get_prop(struct device *dev, const char *pname, u32 def, u32 *ptr)
{
	int ret;

	ret = of_property_read_u32(dev->of_node, pname, ptr);
	if (ret < 0) {
		dev_warn(dev, "lack %s info, set to %d\n", pname, def);
		*ptr = def;
	}
}

static
void rtk_ddr_dfs_of_init(struct device *dev, struct rtk_ddr_dfs_data *data)
{
	__get_prop(dev, "time-window", __DEFAULT_TIME_US, &data->cfg.time_us);
	__get_prop(dev, "low-bw", __DEFAULT_LOW_BW, &data->cfg.low_ack);
	__get_prop(dev, "high-bw", __DEFAULT_HIGH_BW, &data->cfg.high_ack);
	__get_prop(dev, "low-debounce", __DEFAULT_LOW_DEBOUNCE,
		   &data->cfg.low_debounce);
	__get_prop(dev, "high-debounce", __DEFAULT_HIGH_DEBOUNCE,
		   &data->cfg.high_debounce);
	__get_prop(dev, "auto-enable", 0, &data->enabled);

	/* transform the unit */
	data->cfg.low_ack = __BW_TO_ACK(data->cfg.low_ack);
	data->cfg.high_ack = __BW_TO_ACK(data->cfg.high_ack);
}

static
int rtk_ddr_dfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = NULL;
	struct rtk_ddr_dfs_data *dfs_data;
	int ret;

	node = pdev->dev.of_node;
	if (!node) {
		dev_err(dev, "get device tree node failed\n");
		return -ENODEV;
	}

	dev_info(dev, "probe...\n");

	dfs_data = devm_kzalloc(&pdev->dev, sizeof(*dfs_data), GFP_KERNEL);
	if (!dfs_data)
		return -ENOMEM;

	dfs_data->cfg.version = HWM_CFG_VER;
	dfs_data->dirty = 1;
	dfs_data->pdev = pdev;
	rtk_ddr_dfs_of_init(dev, dfs_data);

	ret = sysfs_create_group(&dev->kobj, &ddr_dfs_attr_group);
	if (ret < 0) {
		dev_err(dev, "create sysfs failed(%d)\n", ret);
		goto err;
	}

	platform_set_drvdata(pdev, dfs_data);

	dev_info(dev, "probe done\n");

	if (dfs_data->enabled)
		__enable(dev, dfs_data->enabled);

	return 0;

err:
	dev_err(dev, "probe failed(%d)\n", ret);
	return ret;
}

static void rtk_ddr_dfs_remove(struct platform_device *pdev)
{
	struct rtk_ddr_dfs_data *dfs_data = platform_get_drvdata(pdev);

	if (dfs_data->enabled)
		__enable(&pdev->dev, 0);

	sysfs_remove_group(&pdev->dev.kobj, &ddr_dfs_attr_group);
	platform_set_drvdata(pdev, NULL);
}

static const struct dev_pm_ops __pm_ops = {
	.suspend = __pm_suspend,
	.resume = __pm_resume,
};

static const struct of_device_id rtk_ddr_dfs_ids[] = {
	{ .compatible = "realtek,kent-ddr-dfs", },
	{ /* Sentinel */ },
};

static struct platform_driver rtk_ddr_dfs_driver = {
	.probe = rtk_ddr_dfs_probe,
	.remove	= rtk_ddr_dfs_remove,
	.driver = {
		.name = "rtk-ddr-dfs",
		.pm = &__pm_ops,
		.of_match_table = of_match_ptr(rtk_ddr_dfs_ids),
	},
};
module_platform_driver(rtk_ddr_dfs_driver);

MODULE_AUTHOR("phelic <phelic@realtek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Realtek DDR DFS driver");
