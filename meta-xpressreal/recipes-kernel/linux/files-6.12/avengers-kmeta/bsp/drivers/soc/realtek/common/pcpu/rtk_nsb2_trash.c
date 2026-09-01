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

#include "rtk_nsb2_trash.h"


static
ssize_t __enable_show(struct device *dev, struct device_attribute *attr, char *buf, int idx)
{
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", data->cfgs[idx].enabled);
}

static
int __enable(struct device *dev, int on, int idx)
{
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev);
	struct arm_smccc_res res;

	if (on) {
		/* send config before enable */
		arm_smccc_smc(NSB2_TRASH_SMCCC_EN, idx, virt_to_phys(&data->cfgs[idx].param),
			      sizeof(struct rtk_nsb2_trash_param), 0, 0, 0, 0, &res);
	} else {
		arm_smccc_smc(NSB2_TRASH_SMCCC_DIS, idx, 0, 0, 0, 0, 0, 0, &res);
	}

	if (res.a0)
		dev_err(dev, "%s set-%d failed(%ld)\n", on ? "enable" : "disable", idx, res.a0);

	return !!res.a0;
}

static
ssize_t __enable_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count, int idx)
{
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev);
	int ret;
	int tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = !!tmp;
	if (data->cfgs[idx].enabled == tmp)
		goto exit;

	if (!__enable(dev, tmp, idx))
		data->cfgs[idx].enabled = tmp;

exit:
	return count;
}

static inline
void __warn_config(struct device *dev)
{
	dev_warn(dev, "config takes place after re-enabling\n");
}

static const struct mode {
	const char	*name;
	int		val;
} avail_modes[] = {
	{.name = "rw",		.val = 0},
	{.name = "read",	.val = 1},
	{.name = "write",	.val = 2},
};

static
ssize_t __mode_show(struct device *dev, struct device_attribute *attr, char *buf, int idx)
{
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", avail_modes[data->cfgs[idx].param.mode].name);
}

static
ssize_t __mode_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count, int idx)
{
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(avail_modes); i++) {
		if (!strncmp(buf, avail_modes[i].name, count - 1)) {
			if (data->cfgs[idx].param.mode != avail_modes[i].val) {
				data->cfgs[idx].param.mode = avail_modes[i].val;
				__warn_config(dev);
			}
			goto exit;
		}
	}

	dev_err(dev, "unknown mode, valid: {rw | read | write}\n");

exit:
	return count;
}

static const struct cpu_map {
	const char	*name;
	int		flag;
} avail_cpus[] = {
	{.name = "dcpu",	.flag = BIT(0)},
	{.name = "pcpu",	.flag = BIT(1)},
	{.name = "mcu",		.flag = BIT(2)},
};

static
ssize_t __cpus_show(struct device *dev, struct device_attribute *attr, char *buf, int idx)
{
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev);
	int ret = 0;
	int i;

	if (data->cfgs[idx].param.cpus == 0) {
		ret += sysfs_emit_at(buf, ret, "none, avail: { ");
		for (i = 0; i < ARRAY_SIZE(avail_cpus); i++)
			ret += sysfs_emit_at(buf, ret, "%s ", avail_cpus[i].name);
		ret += sysfs_emit_at(buf, ret, "}");
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(avail_cpus); i++) {
		if (data->cfgs[idx].param.cpus & avail_cpus[i].flag) {
			ret += sysfs_emit_at(buf, ret, "%s ", avail_cpus[i].name);
		}
	}

exit:
	ret += sysfs_emit_at(buf, ret, "\n");
	return ret;
}

static
ssize_t __cpus_store(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count, int idx)
{
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev);
	u32 cpus = data->cfgs[idx].param.cpus;
	int i;

	for (i = 0; i < ARRAY_SIZE(avail_cpus); i++) {
		if (strnstr(buf, avail_cpus[i].name, count - 1)) {
			cpus ^= avail_cpus[i].flag;
		}
	}

	if (data->cfgs[idx].param.cpus == cpus) {
		dev_err(dev, "unknown cpu\n");
	} else {
		data->cfgs[idx].param.cpus = cpus;
		__warn_config(dev);
	}

	return count;
}

#define __ATTR_BASE_OP(name)		\
static ssize_t __##name##_show(struct device *dev, struct device_attribute *attr, \
			       char *buf, int idx) \
{ \
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev); \
	return sysfs_emit(buf, "0x%x\n", data->cfgs[idx].param.name); \
} \
static ssize_t __##name##_store(struct device *dev, struct device_attribute *attr, \
				const char *buf, size_t count, int idx) \
{ \
	struct rtk_nsb2_trash_data *data = dev_get_drvdata(dev); \
	int ret; \
	long tmp; \
	ret = kstrtoul(buf, 0, &tmp); \
	if (ret) \
		return ret; \
	data->cfgs[idx].param.name = tmp; \
	__warn_config(dev); \
	return count; \
}

__ATTR_BASE_OP(start);
__ATTR_BASE_OP(end);

#define __ATTR_OP_DUP(name, idx)		\
static ssize_t name##_##idx##_show(struct device *dev, struct device_attribute *attr, \
				   char *buf)	\
{ \
	return __##name##_show(dev, attr, buf, 0); \
} \
static \
ssize_t name##_##idx##_store(struct device *dev, struct device_attribute *attr,	\
			     const char *buf, size_t count) \
{ \
	return __##name##_store(dev, attr, buf, count, 0); \
} \

__ATTR_OP_DUP(enable,	0);
__ATTR_OP_DUP(start,	0);
__ATTR_OP_DUP(end,	0);
__ATTR_OP_DUP(mode,	0);
__ATTR_OP_DUP(cpus,	0);
__ATTR_OP_DUP(enable,	1);
__ATTR_OP_DUP(start,	1);
__ATTR_OP_DUP(end,	1);
__ATTR_OP_DUP(mode,	1);
__ATTR_OP_DUP(cpus,	1);


static DEVICE_ATTR_RW(enable_0);
static DEVICE_ATTR_RW(start_0);
static DEVICE_ATTR_RW(end_0);
static DEVICE_ATTR_RW(mode_0);
static DEVICE_ATTR_RW(cpus_0);
static DEVICE_ATTR_RW(enable_1);
static DEVICE_ATTR_RW(start_1);
static DEVICE_ATTR_RW(end_1);
static DEVICE_ATTR_RW(mode_1);
static DEVICE_ATTR_RW(cpus_1);


#define __ATTR_REF(_name)	(&dev_attr_##_name.attr)
static struct attribute *nsb2_trash_attrs[] = {
	__ATTR_REF(enable_0),
	__ATTR_REF(start_0),
	__ATTR_REF(end_0),
	__ATTR_REF(mode_0),
	__ATTR_REF(cpus_0),
	__ATTR_REF(enable_1),
	__ATTR_REF(start_1),
	__ATTR_REF(end_1),
	__ATTR_REF(mode_1),
	__ATTR_REF(cpus_1),
	NULL
};

static struct attribute_group nsb2_trash_attr_group = {
	.name = "trash",
	.attrs = nsb2_trash_attrs,
};

static struct rtk_nsb2_trash_meta nsb2_trash_meta = {
	.nr_set		= 2,
	.attr_group	= &nsb2_trash_attr_group,
};

static
int __nsb2_trash_probe(struct platform_device *pdev, const struct of_device_id *of_table)
{
	const struct of_device_id *of_id = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node = NULL;
	struct rtk_nsb2_trash_data *trash_data;
	struct rtk_nsb2_trash_meta *meta;
	int ret;

	node = pdev->dev.of_node;
	if (node) {
		of_id = of_match_node(of_table, node);
	} else {
		ret = -ENODEV;
		goto err;
	}

	if (of_id) {
		meta = (struct rtk_nsb2_trash_meta *)(of_id->data);
	} else {
		ret = -ENODEV;
		goto err;
	}

	dev_info(dev, "probe...\n");

	trash_data = devm_kzalloc(&pdev->dev, sizeof(*trash_data), GFP_KERNEL);
	if (!trash_data) {
		ret = -ENOMEM;
		goto err;
	}

	platform_set_drvdata(pdev, trash_data);
	trash_data->pdev = pdev;
	trash_data->version = 0;
	trash_data->meta = meta;
	trash_data->nr_set = meta->nr_set;
	trash_data->cfgs = devm_kzalloc(&pdev->dev,
					sizeof(*trash_data->cfgs) * trash_data->nr_set,
					GFP_KERNEL);
	if (trash_data->cfgs == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	ret = sysfs_create_group(&dev->kobj, meta->attr_group);
	if (ret < 0) {
		dev_err(dev, "create sysfs failed(%d)\n", ret);
		goto err;
	}

	dev_info(dev, "probe done\n");

	return 0;

err:
	dev_err(dev, "probe failed(%d)\n", ret);
	return ret;
}

static const struct of_device_id rtk_nsb2_trash_ids[] = {
	{
		.compatible	= "realtek,prince-nsb2-trash",
		.data		= &nsb2_trash_meta,
	},
	{ /* Sentinel */ },
};

static
int rtk_nsb2_trash_probe(struct platform_device *pdev)
{
	return __nsb2_trash_probe(pdev, rtk_nsb2_trash_ids);
}

static void rtk_nsb2_trash_remove(struct platform_device *pdev)
{
	struct rtk_nsb2_trash_data *data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < data->nr_set; i++) {
		if (data->cfgs[i].enabled)
			__enable(&pdev->dev, 0, i);
	}

	sysfs_remove_group(&pdev->dev.kobj, data->meta->attr_group);
	platform_set_drvdata(pdev, NULL);
}

static struct platform_driver rtk_nsb2_trash_driver = {
	.probe	= rtk_nsb2_trash_probe,
	.remove	= rtk_nsb2_trash_remove,
	.driver = {
		.name = "rtk-nsb2-trash",
		.of_match_table = of_match_ptr(rtk_nsb2_trash_ids),
	},
};
module_platform_driver(rtk_nsb2_trash_driver);

MODULE_AUTHOR("phelic <phelic@realtek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Realtek NSB2 trash driver");
