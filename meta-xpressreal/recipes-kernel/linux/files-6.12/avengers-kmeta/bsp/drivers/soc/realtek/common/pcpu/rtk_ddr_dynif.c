// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2026 Realtek Semiconductor Corp.
 */
#include <linux/arm-smccc.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "rtk_ddr_dynif.h"


#define DDR_TM_DEF_INTERVAL		2000                    /* ms */
#define DDR_TM_MAX_INTERVAL		20000			/* ms */
#define DDR_TM_DEF_DELTA                TM_VAL_H(10000)		/* 10 degree */
#define DDR_TM_MAX_DELTA                TM_VAL_H(20000)		/* 20 degree */
#define DDR_MR4_DEF_INTERVAL            150                     /* ms */
#define DDR_MR4_MAX_INTERVAL            1000                    /* ms */
#define DDR_TIMER_WAIT_TIME             100


#define TM_MUL		1024
#define TM_COEFF	1000
#define TM_VAL_H(_v)	((_v) * TM_MUL / TM_COEFF)
#define TM_VAL_P(_v)	((_v) * TM_COEFF / TM_MUL)


static const char *bind_str[] = {
	[0] = "tx_dq",
	[1] = "zq_cal",
	[2] = "dq_cal",
	[3] = "zqc",
	[4] = "ref_chg"
};

static int __out_bind_str(char *buf, u32 flags)
{
	int ret = 0;
	int i;

	if (flags == 0) {
		ret += sysfs_emit_at(buf, ret, "none, { ");
		for (i = 0; i < ARRAY_SIZE(bind_str); i++) {
			ret += sysfs_emit_at(buf, ret, "%s ", bind_str[i]);
		}
		ret += sysfs_emit_at(buf, ret, "}\n");
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(bind_str); i++) {
		if (flags & BIT(i)) {
			ret += sysfs_emit_at(buf, ret, "%s ", bind_str[i]);
		}
	}
	ret += sysfs_emit_at(buf, ret, "\n");

exit:
	return ret;
}

static u32 __in_bind_str(const char *buf, size_t count, u32 flags)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bind_str); i++) {
		if (strnstr(buf, bind_str[i], count - 1)) {
			flags ^= BIT(i);
		}
	}
	return flags;
}

static
void __warn_config(struct device *dev, int enabled)
{
	if (enabled)
		dev_warn(dev, "config takes place after re-enabling\n");
}

static
ssize_t dpi_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", data->dpi_enabled);
}

static
int __dpi_enable(struct device *dev, int on)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	struct arm_smccc_res res;

	if (on && data->dpi_dirty) {
		/* send config before enable */
		arm_smccc_smc(DPI_SMCCC_CFG, virt_to_phys(&data->dpi),
			      sizeof(data->dpi), 0, 0, 0, 0, 0, &res);
		if (res.a0) {
			dev_err(dev, "update dpi_configs failed(%ld)\n",
				res.a0);
			goto exit;
		} else {
			data->dpi_dirty = 0;
		}
	}

	arm_smccc_smc(DPI_SMCCC_CTRL, on, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		dev_err(dev, "%s failed(%ld)\n", on ? "enabled" : "disabled", res.a0);

exit:
	return !!res.a0;
}

static
ssize_t dpi_enable_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	int ret;
	int tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = !!tmp;
	if (data->dpi_enabled == tmp)
		goto exit;

	if (!__dpi_enable(dev, tmp))
		data->dpi_enabled = tmp;

exit:
	return count;
}

static
ssize_t dpi_interval_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", data->dpi.interval_ms);
}

static
ssize_t dpi_interval_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	int ret;
	long tmp;

	ret = kstrtoul(buf, 10, &tmp);
	if (ret)
		return ret;

	if (tmp > DDR_TM_MAX_INTERVAL)
		return -EINVAL;

	if (data->dpi.interval_ms != tmp) {
		data->dpi_dirty = 1;
		data->dpi.interval_ms = tmp;

		__warn_config(dev, data->dpi_enabled);
	}

	return count;
}

static
ssize_t dpi_step_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", TM_VAL_P(data->dpi.tm_delta.val));
}

static
ssize_t dpi_step_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	int ret;
	long tmp;

	ret = kstrtoul(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = tmp * TM_MUL / TM_COEFF;
	if (tmp > DDR_TM_MAX_DELTA)
		return -EINVAL;

	if (data->dpi.tm_delta.val != tmp) {
		data->dpi_dirty = 1;
		data->dpi.tm_delta.val = tmp;

		__warn_config(dev, data->dpi_enabled);
	}

	return count;
}

static
ssize_t dpi_bind_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);

	return __out_bind_str(buf, data->dpi.bind);
}

static
ssize_t dpi_bind_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	u32 bind;

	bind = __in_bind_str(buf, count, data->dpi.bind);
	if (bind != data->dpi.bind) {
		if ((bind & data->ref.bind) == 0) {
			data->dpi.bind = bind;
			data->dpi_dirty = 1;

			__warn_config(dev, data->dpi_enabled);
		} else {
			pr_err("dpi_bind can not overlap with ref_bind\n");
		}
	}

	return count;
}

static
ssize_t refresh_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", data->ref_enabled);
}

static
int __ref_enable(struct device *dev, int on)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	struct arm_smccc_res res;

	if (on && data->ref_dirty) {
		/* send config before enable */
		arm_smccc_smc(REF_SMCCC_CFG, virt_to_phys(&data->ref),
			      sizeof(data->ref), 0, 0, 0, 0, 0, &res);
		if (res.a0) {
			dev_err(dev, "update ref_configs failed(%ld)\n", res.a0);
			goto exit;
		} else {
			data->ref_dirty = 0;
		}
	}

	arm_smccc_smc(REF_SMCCC_CTRL, on, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		dev_err(dev, "%s failed(%ld)\n", on ? "enabled" : "disabled", res.a0);

exit:
	return !!res.a0;
}

static
ssize_t refresh_enable_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	int ret;
	int tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = !!tmp;
	if (data->ref_enabled == tmp)
		goto exit;

	if (!__ref_enable(dev, tmp))
		data->ref_enabled = tmp;

exit:
	return count;
}

static
ssize_t refresh_interval_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", data->ref.interval_ms);
}

static
ssize_t refresh_interval_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	int ret;
	long tmp;

	ret = kstrtoul(buf, 10, &tmp);
	if (ret)
		return ret;

	if (tmp > DDR_MR4_MAX_INTERVAL)
		return -EINVAL;

	if (data->ref.interval_ms != tmp) {
		data->ref_dirty = 1;
		data->ref.interval_ms = tmp;

		__warn_config(dev, data->ref_enabled);
	}

	return count;
}

static
ssize_t refresh_bind_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);

	return __out_bind_str(buf, data->ref.bind);
}

static
ssize_t refresh_bind_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	u32 bind;

	bind = __in_bind_str(buf, count, data->ref.bind);
	if (bind != data->ref.bind) {
		if ((bind & data->dpi.bind) == 0) {
			data->ref.bind = bind;
			data->ref_dirty = 1;

			__warn_config(dev, data->ref_enabled);
		} else {
			pr_err("ref_bind can not overlap with dpi_bind\n");
		}
	}

	return count;
}

static DEVICE_ATTR_RW(dpi_enable);
static DEVICE_ATTR_RW(dpi_interval);
static DEVICE_ATTR_RW(dpi_step);
static DEVICE_ATTR_RW(dpi_bind);
static DEVICE_ATTR_RW(refresh_enable);
static DEVICE_ATTR_RW(refresh_interval);
static DEVICE_ATTR_RW(refresh_bind);

#define __ATTR_REF(_name)	(&dev_attr_##_name.attr)
static struct attribute *ddr_dynif_attrs[] = {
	__ATTR_REF(dpi_enable),
	__ATTR_REF(dpi_interval),
	__ATTR_REF(dpi_step),
	__ATTR_REF(dpi_bind),
	__ATTR_REF(refresh_enable),
	__ATTR_REF(refresh_interval),
	__ATTR_REF(refresh_bind),
	NULL
};

static struct attribute_group ddr_dynif_attr_group = {
	.name = "dynamic_interface",
	.attrs = ddr_dynif_attrs,
};

static
int __pm_suspend(struct device *dev)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	int ret0, ret1;

	ret0 = data->dpi_enabled ? __dpi_enable(dev, 0) : 0;
	ret1 = data->ref_enabled ? __ref_enable(dev, 0) : 0;

	return ret0 == 0 ? ret1 : ret0;
}

static
int __pm_resume(struct device *dev)
{
	struct rtk_ddr_dynif_data *data = dev_get_drvdata(dev);
	int ret0, ret1;

	ret0 = data->dpi_enabled ? __dpi_enable(dev, 1) : 0;
	ret1 = data->ref_enabled ? __ref_enable(dev, 1) : 0;

	return ret0 == 0 ? ret1 : ret0;
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
int rtk_ddr_dynif_of_init(struct device *dev, struct rtk_ddr_dynif_data *data)
{
	struct device_node *tm_node;
	struct platform_device *tm_pdev;
	int tmp;

	tm_node = of_parse_phandle(dev->of_node, "dpi-tm", 0);
	if (tm_node == NULL) {
		dev_err(dev, "no related sensor\n");
		return -ENODEV;
	}

	tm_pdev = of_find_device_by_node(tm_node);
	of_node_put(tm_node);
	if (tm_pdev == NULL) {
		dev_err(dev, "thermal sensor not found\n");
		return -ENODEV;
	}
	device_link_add(dev, &tm_pdev->dev, DL_FLAG_STATELESS);
	platform_device_put(tm_pdev);

	__get_prop(dev, "dpi-enable", 0, &data->dpi_enabled);
	__get_prop(dev, "dpi-bind", DDR_DYNIF_TX_DQ | DDR_DYNIF_ZQ_CAL | DDR_DYNIF_DQ_CAL,
		   &data->dpi.bind);

	__get_prop(dev, "dpi-interval", DDR_TM_DEF_INTERVAL, &tmp);
	if (tmp < DDR_TM_MAX_INTERVAL)
		data->dpi.interval_ms = tmp;

	__get_prop(dev, "dpi-step", DDR_TM_DEF_DELTA, &tmp);
	if (tmp < DDR_TM_MAX_DELTA)
		data->dpi.tm_delta.val = tmp;

	__get_prop(dev, "refresh-enable", 0, &data->ref_enabled);
	__get_prop(dev, "refresh-bind",	DDR_DYNIF_ZQC | DDR_DYNIF_REF_CHG, &data->ref.bind);

	__get_prop(dev, "refresh-interval", DDR_MR4_DEF_INTERVAL, &tmp);
	if (tmp < DDR_MR4_MAX_INTERVAL)
		data->ref.interval_ms = tmp;

	return 0;
}

static
int rtk_ddr_dynif_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = NULL;
	struct rtk_ddr_dynif_data *dyn_data;
	int ret;

	node = pdev->dev.of_node;
	if (!node) {
		dev_err(dev, "get device tree node failed\n");
		return -ENODEV;
	}

	dev_info(dev, "probe...\n");

	dyn_data = devm_kzalloc(&pdev->dev, sizeof(*dyn_data), GFP_KERNEL);
	if (!dyn_data)
		return -ENOMEM;

	dyn_data->pdev = pdev;
	ret = rtk_ddr_dynif_of_init(dev, dyn_data);
	if (ret < 0)
		goto err;

	dyn_data->dpi_dirty = 1;
	dyn_data->ref_dirty = 1;
	platform_set_drvdata(pdev, dyn_data);

	ret = sysfs_create_group(&dev->kobj, &ddr_dynif_attr_group);
	if (ret < 0) {
		dev_err(dev, "create sysfs failed(%d)\n", ret);
		goto err;
	}

	dev_info(dev, "probe done\n");

	if (dyn_data->dpi_enabled)
		if (__dpi_enable(dev, dyn_data->dpi_enabled))
			dyn_data->dpi_enabled = false;

	if (dyn_data->ref_enabled)
		if (__ref_enable(dev, dyn_data->ref_enabled))
			dyn_data->ref_enabled = false;

	return 0;

err:
	dev_err(dev, "probe failed(%d)\n", ret);
	return ret;
}

static void rtk_ddr_dynif_remove(struct platform_device *pdev)
{
	struct rtk_ddr_dynif_data *dyn_data = platform_get_drvdata(pdev);

	if (dyn_data->dpi_enabled)
		__dpi_enable(&pdev->dev, 0);
	if (dyn_data->ref_enabled)
		__ref_enable(&pdev->dev, 0);

	sysfs_remove_group(&pdev->dev.kobj, &ddr_dynif_attr_group);
	platform_set_drvdata(pdev, NULL);
}

static const struct dev_pm_ops __pm_ops = {
	.suspend = __pm_suspend,
	.resume = __pm_resume,
};

static const struct of_device_id rtk_ddr_dynif_ids[] = {
	{ .compatible = "realtek,rtk-ddr-dynif", },
	{ /* Sentinel */ },
};

static struct platform_driver rtk_ddr_dynif_driver = {
	.probe = rtk_ddr_dynif_probe,
	.remove	= rtk_ddr_dynif_remove,
	.driver = {
		.name = "rtk-ddr-dynif",
		.pm = &__pm_ops,
		.of_match_table = of_match_ptr(rtk_ddr_dynif_ids),
	},
};
module_platform_driver(rtk_ddr_dynif_driver);

MODULE_AUTHOR("phelic <phelic@realtek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Realtek DDR Dynamic calibration driver");
