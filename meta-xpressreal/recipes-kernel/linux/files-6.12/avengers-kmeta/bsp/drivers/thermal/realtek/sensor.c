// SPDX-License-Identifier: GPL-2.0-only
/*
 * sensor.c - Realtek generic thermal sensor driver
 *
 * Copyright (C) 2017-2018,2020 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include "sensor.h"

static inline int thermal_sensor_hw_init(struct thermal_sensor_device *tdev)
{
	const struct thermal_sensor_desc *desc = tdev->desc;
	u32 sleep_time = desc->reset_time_ms ? desc->reset_time_ms : 10;
	int ret = 0;

	mutex_lock(&tdev->lock);
	if (desc->hw_ops->init)
		ret = desc->hw_ops->init(tdev);
	if (ret == 0)
		msleep(sleep_time);
	mutex_unlock(&tdev->lock);

	return ret;
}

static inline void thermal_sensor_hw_reset_locked(struct thermal_sensor_device *tdev)
{
	const struct thermal_sensor_desc *desc = tdev->desc;
	u32 sleep_time = desc->reset_time_ms ? desc->reset_time_ms : 10;

	lockdep_assert_held(&tdev->lock);

	if (!desc->hw_ops->reset)
		return;

	desc->hw_ops->reset(tdev);
	msleep(sleep_time);
}

static inline int thermal_sensor_hw_get_temp_locked(struct thermal_sensor_device *tdev,
					     int *temp)
{
	const struct thermal_sensor_desc *desc = tdev->desc;

	lockdep_assert_held(&tdev->lock);

	if (!desc->hw_ops->get_temp)
		return -EINVAL;

	return desc->hw_ops->get_temp(tdev, temp);
}

static inline
void thermal_sensor_dump_reg_locked(struct thermal_sensor_device *tdev)
{
	const struct thermal_sensor_desc *desc = tdev->desc;

	lockdep_assert_held(&tdev->lock);

	if (!desc->hw_ops->dump_reg)
		return;
	desc->hw_ops->dump_reg(tdev);
}

static inline int is_valid_temp(int temp, const struct thermal_sensor_desc *desc)
{
	int max_temp = 150000;
	int min_temp = -3000;

	if (desc->has_valid_temp) {
		max_temp = desc->valid_max_temp;
		min_temp = desc->valid_min_temp;
	}

	if (temp < min_temp || temp > max_temp)
		return 0;
	return 1;
}

static int thermal_sensor_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct thermal_sensor_device *tdev = thermal_zone_device_priv(tz);
	int ret;

	mutex_lock(&tdev->lock);
	ret = thermal_sensor_hw_get_temp_locked(tdev, temp);

	if (ret < 0 || !is_valid_temp(*temp, tdev->desc)) {
		dev_warn(tdev->dev, "invalid temp=%d\n", *temp);
		thermal_sensor_dump_reg_locked(tdev);

		thermal_sensor_hw_reset_locked(tdev);
		ret = thermal_sensor_hw_get_temp_locked(tdev, temp);
	}
	mutex_unlock(&tdev->lock);

	dev_dbg(tdev->dev, "temp=%d\n", *temp);
	return ret;
}

static const struct thermal_zone_device_ops rtk_thermal_device_ops = {
	.get_temp  = thermal_sensor_get_temp,
};

static int thermal_sensor_resume(struct device *dev)
{
	struct thermal_sensor_device *tdev = dev_get_drvdata(dev);
	int ret;

	dev_info(dev, "enter %s\n", __func__);
	ret = thermal_sensor_hw_init(tdev);
	dev_info(dev, "exit %s: thermal_sensor_hw_init() returns %d\n", __func__, ret);
	return 0;
}

static const struct dev_pm_ops thermal_sensor_pm_ops = {
	.resume = thermal_sensor_resume,
};

static int thermal_sensor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct thermal_sensor_device *tdev;
	int ret = 0;
	struct resource res;
	const struct thermal_sensor_desc *desc;
	u32 num, i;

	desc = of_device_get_match_data(dev);
	if (!desc)
		return -EINVAL;

	tdev = devm_kzalloc(dev, sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return -ENOMEM;

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		return ret;

	tdev->base = devm_ioremap(dev, res.start, resource_size(&res));
	if (!tdev->base)
		return -ENOMEM;

	if (of_find_property(np, "realtek,scpu-wrapper", NULL)) {
		tdev->regmap = syscon_regmap_lookup_by_phandle(np, "realtek,scpu-wrapper");
		if (IS_ERR(tdev->regmap))
			return PTR_ERR(tdev->regmap);
	}

	mutex_init(&tdev->lock);
	tdev->dev = dev;
	tdev->desc = desc;
	platform_set_drvdata(pdev, tdev);

	ret = thermal_sensor_hw_init(tdev);
	dev_info(dev, "thermal_sensor_hw_init() returns %d\n", ret);
	if (ret < 0)
		return ret;

	/*
	 * NOTE: This implementation works around a limitation in the current kernel
	 * thermal framework, which does not natively support having a single thermal
	 * sensor provide data to multiple thermal zones. This driver registers
	 * multiple logical sensors (exposed as thermal zones), which all access
	 * the same underlying hardware device for their data.
	 *
	 * This section should be refactored if future kernel versions add native
	 * support for a one-to-many sensor-to-zone relationship.
	 */
	if (of_property_read_u32(np, "realtek,sensor-instances", &num))
		num = 1;

	for (i = 0; i < num; i++) {
		struct thermal_zone_device *tz;

		tz = devm_thermal_of_zone_register(dev, i, tdev,
								&rtk_thermal_device_ops);
		if (IS_ERR(tz)) {
			ret = PTR_ERR(tz);
			dev_err(dev, "failed to add thermal sensor: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static void thermal_sensor_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
}

static const struct of_device_id thermal_sensor_of_match[] = {
	{ .compatible = "realtek,rtd129x-thermal-sensor", .data = &rtd129x_sensor_desc, },
	{ .compatible = "realtek,rtd139x-thermal-sensor", .data = &rtd139x_sensor_desc, },
	{ .compatible = "realtek,rtd161x-thermal-sensor", .data = &rtd1619_sensor_desc, },
	{ .compatible = "realtek,rtd131x-thermal-sensor", .data = &rtd1319_sensor_desc, },
	{ .compatible = "realtek,rtd1619b-thermal-sensor", .data = &rtd1619b_sensor_desc, },
	{ .compatible = "realtek,rtd1312c-thermal-sensor", .data = &rtd1312c_sensor_desc, },
	{ .compatible = "realtek,rtd1319d-thermal-sensor", .data = &rtd1319d_sensor_desc, },
	{ .compatible = "realtek,rtd1315e-thermal-sensor", .data = &rtd1315e_sensor_desc, },
	{ .compatible = "realtek,rtd1625-sc-wrap-thermal-sensor", .data = &rtd1625_sc_wrap_sensor_desc, },
	{ .compatible = "realtek,rtd1635-sc-wrap-thermal-sensor", .data = &rtd1635_sc_wrap_sensor_desc, },
	{ .compatible = "realtek,rtd1625-sys-thermal-sensor", .data = &rtd1625_sys_sensor_desc, },
	{}
};
MODULE_DEVICE_TABLE(of, thermal_sensor_of_match);

static struct platform_driver thermal_sensor_drv = {
	.driver = {
		.name           = "rtk-thermal-sensor",
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(thermal_sensor_of_match),
		.pm             = &thermal_sensor_pm_ops,
	},
	.probe    = thermal_sensor_probe,
	.remove   = thermal_sensor_remove,
};
module_platform_driver(thermal_sensor_drv);

MODULE_DESCRIPTION("Realtek Thermal Sensor Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
