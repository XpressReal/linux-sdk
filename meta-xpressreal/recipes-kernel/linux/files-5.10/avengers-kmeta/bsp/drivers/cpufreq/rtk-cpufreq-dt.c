// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "cpufreq-dt.h"

static struct platform_device *cpufreq_dt_pdev;

static unsigned int rtk_get_intermediate(struct cpufreq_policy *policy,
					 unsigned int index)
{
	unsigned int freq = policy->freq_table[index].frequency;

	if (freq == 1000000 || freq == 1100000)
		return 1400000000;
	return 0;
}

static int rtk_target_intermediate(struct cpufreq_policy *policy,
					unsigned int index)
{
	unsigned int cpu = cpumask_first(policy->cpus);
	struct device *cpu_dev = get_cpu_device(cpu);

	return dev_pm_opp_set_rate(cpu_dev, 1400000000);
}

static struct cpufreq_dt_platform_data rtk_cpufreq_dt_platform_data = {
	.target_intermediate = rtk_target_intermediate,
	.get_intermediate = rtk_get_intermediate,
};

static __init int rtk_cpufreq_dt_init(void)
{
	int ret;

	cpufreq_dt_pdev = platform_device_register_data(NULL, "cpufreq-dt", -1,
							&rtk_cpufreq_dt_platform_data,
							sizeof(rtk_cpufreq_dt_platform_data));
	if (IS_ERR(cpufreq_dt_pdev)) {
		ret = PTR_ERR(cpufreq_dt_pdev);
		pr_err("Failed to register cpufreq-dt: %d\n", ret);
		return ret;
	}
	return 0;
}
core_initcall(rtk_cpufreq_dt_init);

static __exit void rtk_cpufreq_dt_exit(void)
{
	platform_device_unregister(cpufreq_dt_pdev);
}
module_exit(rtk_cpufreq_dt_exit);

MODULE_ALIAS("platform:rtk-cpufreq-dt");
MODULE_LICENSE("GPL v2");
