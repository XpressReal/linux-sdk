// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025, Realtek Semiconductor Corporation
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/regulator/coupler.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/suspend.h>

#define to_rtk_coupler(x)	container_of(x, struct rtk_regulator_coupler, coupler)

struct rtk_regulator_coupler {
	struct regulator_coupler coupler;
	struct regulator_dev *vsram_rdev;
};

static int rtk_regulator_balance_voltage(struct regulator_coupler *coupler,
					 struct regulator_dev *rdev,
					 suspend_state_t state)
{
	struct rtk_regulator_coupler *cdata = to_rtk_coupler(coupler);
	int max_spread = rdev->constraints->max_spread[0];
	int vsram_min_uV = cdata->vsram_rdev->constraints->min_uV;
	int vsram_max_uV = cdata->vsram_rdev->constraints->max_uV;
	int vsram_target_min_uV, vsram_target_max_uV;
	int min_uV = 0;
	int max_uV = INT_MAX;
	int cur_uV;
	int ret;

	if (rdev == cdata->vsram_rdev)
		return -EPERM;

	ret = regulator_check_consumers(rdev, &min_uV, &max_uV, state);
	if (ret < 0)
		return ret;

	ret = regulator_get_voltage_rdev(rdev);
	if (ret < 0)
		return ret;
	cur_uV = ret;
	if (min_uV == 0)
		min_uV = cur_uV;

	ret = regulator_check_voltage(rdev, &min_uV, &max_uV);
	if (ret < 0)
		return ret;

	vsram_target_min_uV = max(vsram_min_uV, min_uV + max_spread);
	vsram_target_max_uV = vsram_max_uV;
	vsram_target_min_uV = min(vsram_target_min_uV, vsram_max_uV);
	if (vsram_target_min_uV >= 1050000)
		vsram_target_min_uV = 1050000;

	if (min_uV < cur_uV)
		ret = regulator_set_voltage_rdev(rdev, min_uV, max_uV, state);
	ret = regulator_set_voltage_rdev(cdata->vsram_rdev, vsram_target_min_uV,
					 vsram_target_max_uV, state);
	if (min_uV > cur_uV)
		ret = regulator_set_voltage_rdev(rdev, min_uV, max_uV, state);
	return 0;
}

static int rtk_regulator_attach(struct regulator_coupler *coupler,
				struct regulator_dev *rdev)
{
	struct rtk_regulator_coupler *cdata = to_rtk_coupler(coupler);
	const char *rdev_name = rdev_get_name(rdev);

	if (rdev->coupling_desc.n_coupled > 2)
		return 1;

	if (strstr(rdev_name, "cpusram")) {
		if (cdata->vsram_rdev)
			return -EINVAL;
		cdata->vsram_rdev = rdev;
	} else if (!strstr(rdev_name, "cpudvs")) {
		return 1;
	}

	return 0;
}

static int rtk_regulator_detach(struct regulator_coupler *coupler,
				struct regulator_dev *rdev)
{
	struct rtk_regulator_coupler *cdata = to_rtk_coupler(coupler);

	if (rdev == cdata->vsram_rdev)
		cdata->vsram_rdev = NULL;

	return 0;
}

static struct rtk_regulator_coupler rtk_coupler = {
	.coupler = {
		.attach_regulator = rtk_regulator_attach,
		.detach_regulator = rtk_regulator_detach,
		.balance_voltage = rtk_regulator_balance_voltage,
	},
};

int rtk_regulator_coupler_init(void)
{
	return regulator_coupler_register(&rtk_coupler.coupler);
}
arch_initcall(rtk_regulator_coupler_init);

MODULE_DESCRIPTION("Realtek Regulator Coupler driver");
MODULE_LICENSE("GPL");
