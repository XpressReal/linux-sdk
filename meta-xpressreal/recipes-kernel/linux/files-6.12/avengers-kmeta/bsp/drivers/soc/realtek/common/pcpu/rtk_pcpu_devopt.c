// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */
#include <linux/arm-smccc.h>
#include <linux/bitops.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>


#define IPC_POLL_TIME_US		1000
#define IPC_POLL_RETRY			100
#define IPC_CMD_OFFSET			0x470
#define IPC_ARG_OFFSET			0x474
#define IPC_INTR_OFFSET			0xa80
#define IPC_INTR_EN_OFFSET		0xa84
#define IPC_INTR_BIT			BIT(3)
#define __WE				BIT(0)


#define DEVOPT_SUPPORT			0xb0310000
#define DEVOPT_FEATURES			0xb0300001
#define DEVOPT_PWR_BYPASS_READ		0xb0201001
#define DEVOPT_PWR_BYPASS		0xb0301011
#define DEVOPT_FUSA_RESET_READ		0xb0102001
#define DEVOPT_FUSA_RESET		0xb0002011
#define DEVOPT_FUSA_WDT_READ		0xb0132002
#define DEVOPT_FUSA_WDT			0xb0032012

#define DEVOPT_PWR_FLAG			BIT(1)
#define DEVOPT_FUSA_FLAG                BIT(2)


struct pcpu_devopt {
	struct device *dev;

	struct regmap	*intr_regmap;
	struct regmap	*ipc_regmap;

	u32		features;
	u32		pwr_bypass;
	u32		fusa_reset;
	u32		fusa_wdt;
};

static
int ipc_send_blocking(struct pcpu_devopt *opt, u32 cmd, u32 arg, u32 *ack)
{
	int ret;
	int val;

	regmap_write(opt->ipc_regmap, IPC_CMD_OFFSET, cmd);
	regmap_write(opt->ipc_regmap, IPC_ARG_OFFSET, arg);
	regmap_write(opt->intr_regmap, IPC_INTR_OFFSET, IPC_INTR_BIT | __WE);
	ret = regmap_read_poll_timeout(opt->intr_regmap, IPC_INTR_OFFSET,
				       val, !(val & IPC_INTR_BIT),
				       IPC_POLL_TIME_US,
				       IPC_POLL_RETRY * IPC_POLL_TIME_US);
	if (ret) {
		dev_err(opt->dev, "send pcpu IPC(%08x) timeout\n", cmd);
		goto exit;
	}

	regmap_read(opt->ipc_regmap, IPC_ARG_OFFSET, ack);
	regmap_write(opt->ipc_regmap, IPC_ARG_OFFSET, 0);

exit:
	return ret;
}

static
int devopt_ipc_send(struct pcpu_devopt *opt, u32 cmd, u32 arg)
{
	u32 ack;
	int ret = 0;

	ret = ipc_send_blocking(opt, cmd, arg, &ack);
	if (ret)
		goto err;

	ret = ipc_send_blocking(opt, ack, cmd, &ack);
	if (ret)
		goto err;

	if (ack)
		dev_err(opt->dev, "devopt(%08x) failed(%08x)\n", cmd, ack);

	return ack;

err:
	return ret;
}

static
ssize_t pwr_bypass_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct pcpu_devopt *opt = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", opt->pwr_bypass);
}

static
ssize_t pwr_bypass_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct pcpu_devopt *opt = dev_get_drvdata(dev);
	int ret;
	int tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = !!tmp;
	if (opt->pwr_bypass == tmp)
		goto exit;

	ret = devopt_ipc_send(opt, DEVOPT_PWR_BYPASS, tmp);
	if (ret)
		goto exit;

	opt->pwr_bypass = tmp;

exit:
	return count;
}

static
ssize_t fusa_reset_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct pcpu_devopt *opt = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", opt->fusa_reset);
}

static
ssize_t fusa_reset_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct pcpu_devopt *opt = dev_get_drvdata(dev);
	int ret;
	int tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (ret)
		return ret;

	tmp = !!tmp;
	if (opt->fusa_reset == tmp)
		goto exit;

	ret = devopt_ipc_send(opt, DEVOPT_FUSA_RESET, tmp);
	if (ret)
		goto exit;

	opt->fusa_reset = tmp;

exit:
	return count;
}

static
ssize_t fusa_wdt_show(struct device *dev, struct device_attribute *attr,
		      char *buf)
{
	struct pcpu_devopt *opt = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", opt->fusa_wdt);
}

static
ssize_t fusa_wdt_store(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct pcpu_devopt *opt = dev_get_drvdata(dev);
	int ret;
	int tmp;

	ret = kstrtoint(buf, 10, &tmp);
	if (ret)
		return ret;

	if (opt->fusa_wdt == tmp)
		goto exit;

	ret = devopt_ipc_send(opt, DEVOPT_FUSA_WDT, tmp);
	if (ret)
		goto exit;

	opt->fusa_wdt = tmp;

exit:
	return count;
}

static DEVICE_ATTR_RW(pwr_bypass);
static DEVICE_ATTR_RW(fusa_reset);
static DEVICE_ATTR_RW(fusa_wdt);

#define __ATTR_REF(_name)	(&dev_attr_##_name.attr)
static struct attribute *pwr_attrs[] = {
	__ATTR_REF(pwr_bypass),
	NULL
};

static struct attribute_group pwr_attr_group = {
	.name = "pwr",
	.attrs = pwr_attrs,
};

static struct attribute *fusa_attrs[] = {
	__ATTR_REF(fusa_reset),
	__ATTR_REF(fusa_wdt),
	NULL
};

static struct attribute_group fusa_attr_group = {
	.name = "fusa",
	.attrs = fusa_attrs,
};

static
int rtk_pcpu_devopt_of_init(struct device *dev, struct pcpu_devopt *opt)
{
	int ret = 0;

	opt->ipc_regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "ipc-syscon");
	if (IS_ERR_OR_NULL(opt->ipc_regmap)) {
		dev_err(dev, "cannot get ipc regmap\n");
		ret = -EINVAL;
		goto err;
	}

	opt->intr_regmap = syscon_regmap_lookup_by_phandle(dev->of_node,
						    "intr-syscon");
	if (IS_ERR_OR_NULL(opt->intr_regmap)) {
		dev_err(dev, "cannot get intr regmap\n");
		ret = -EINVAL;
		goto err;
	}

	regmap_write(opt->intr_regmap, IPC_INTR_EN_OFFSET, IPC_INTR_BIT | __WE);

err:
	return ret;
}

static inline
int __create_sysfs_group(struct device *dev, const struct attribute_group *grp)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, grp);
	if (ret < 0)
		dev_err(dev, "create %s sysfs failed(%d)\n", grp->name, ret);

	return ret;
}

static
int feature_sysfs_create(struct device *dev, struct pcpu_devopt *opt)
{
	int ret = 0;
	u32 tmp;

	ret = ipc_send_blocking(opt, DEVOPT_SUPPORT, 0, &tmp);
	if (ret < 0) {
		dev_err(dev, "not support(%d)\n", ret);
		ret = -ENXIO;
		goto err;
	} else {
		if (tmp != 1) {
			dev_err(dev, "not support(%d)\n", tmp);
			ret = -EPERM;
			goto err;
		}
	}

	ret = ipc_send_blocking(opt, DEVOPT_FEATURES, 0, &opt->features);
	if (ret < 0) {
		dev_err(dev, "get features failed(%d)\n", ret);
		ret = -ENXIO;
		goto err;
	}

	if (opt->features & DEVOPT_PWR_FLAG) {
		dev_info(dev, "create pwr devopt\n");
		ret = __create_sysfs_group(dev, &pwr_attr_group);
		if (ret)
			goto err;
	}

	if (opt->features & DEVOPT_FUSA_FLAG) {
		dev_info(dev, "create fusa devopt\n");
		ret = __create_sysfs_group(dev, &fusa_attr_group);
		if (ret)
			goto err1;
	}

	return ret;

err1:
	sysfs_remove_group(&dev->kobj, &pwr_attr_group);
err:
	return ret;
}

static
void update_state(struct device *dev, struct pcpu_devopt *opt)
{
	if (opt->features & DEVOPT_PWR_FLAG) {
		if (ipc_send_blocking(opt, DEVOPT_PWR_BYPASS_READ, 0,
				      &opt->pwr_bypass) < 0)
			dev_err(dev, "update pwr_bypass failed\n");
	}

	if (opt->features & DEVOPT_FUSA_FLAG) {
		if (ipc_send_blocking(opt, DEVOPT_FUSA_RESET_READ, 0,
				      &opt->fusa_reset) < 0)
			dev_err(dev, "update fusa_reset failed\n");
		if (ipc_send_blocking(opt, DEVOPT_FUSA_WDT_READ, 0,
				      &opt->fusa_wdt) < 0)
			dev_err(dev, "update fusa_wdt failed\n");
	}
}

static
int rtk_pcpu_devopt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = NULL;
	struct pcpu_devopt *opt;
	int ret;

	node = pdev->dev.of_node;
	if (!node) {
		dev_err(dev, "get device tree node failed\n");
		return -ENODEV;
	}

	dev_info(dev, "probe...\n");

	opt = devm_kzalloc(&pdev->dev, sizeof(*opt), GFP_KERNEL);
	if (!opt)
		return -ENOMEM;

	opt->dev = dev;

	ret = rtk_pcpu_devopt_of_init(dev, opt);
	if (ret < 0)
		goto err;

	ret = feature_sysfs_create(dev, opt);
	if (ret < 0)
		goto err;

	update_state(dev, opt);

	platform_set_drvdata(pdev, opt);

	dev_info(dev, "probe done\n");
	return 0;

err:
	dev_err(dev, "probe failed(%d)\n", ret);
	return ret;
}

static void rtk_pcpu_devopt_remove(struct platform_device *pdev)
{
	struct pcpu_devopt *opt = platform_get_drvdata(pdev);

	if (opt->features & DEVOPT_PWR_FLAG)
		sysfs_remove_group(&pdev->dev.kobj, &pwr_attr_group);
	if (opt->features & DEVOPT_FUSA_FLAG)
		sysfs_remove_group(&pdev->dev.kobj, &fusa_attr_group);
	platform_set_drvdata(pdev, NULL);
}

static const struct of_device_id rtk_pcpu_devopt_ids[] = {
	{ .compatible = "realtek,kent-pcpu-devopt", },
	{ /* Sentinel */ },
};

static struct platform_driver rtk_pcpu_devopt_driver = {
	.probe = rtk_pcpu_devopt_probe,
	.remove	= rtk_pcpu_devopt_remove,
	.driver = {
		.name = "rtk-pcpu-devopt",
		.of_match_table = of_match_ptr(rtk_pcpu_devopt_ids),
	},
};
module_platform_driver(rtk_pcpu_devopt_driver);

MODULE_AUTHOR("phelic <phelic@realtek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek PCPU developer-options driver");
