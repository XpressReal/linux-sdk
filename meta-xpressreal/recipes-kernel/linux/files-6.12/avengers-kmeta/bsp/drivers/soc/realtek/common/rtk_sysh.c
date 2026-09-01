// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */
 #define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/printk.h>
#include <linux/slab.h>

struct rtk_sysh_data;

struct constraint_data {
	struct rtk_sysh_data *bus;
	unsigned long max_freq;
	struct clk *clk;
	struct notifier_block clk_notifier_nb;
	struct list_head node;
};

struct rtk_sysh_data {
	struct device *dev;
	struct clk *clk;
	struct list_head constraint_list;
};

static int rtk_sysh_apply_constraint(struct rtk_sysh_data *bus_data)
{
	struct constraint_data *c;
	unsigned long max_freq = ULONG_MAX;
	unsigned long rounded_freq;

	list_for_each_entry(c, &bus_data->constraint_list, node)
		max_freq = min(max_freq, c->max_freq);

	rounded_freq = clk_round_rate(bus_data->clk, max_freq);
	pr_debug("%pC: freq: input=%lu, rounded=%lu\n", bus_data->clk, max_freq, rounded_freq);
	if (!rounded_freq)
		return -EINVAL;

	return clk_set_rate(bus_data->clk, rounded_freq);
}

static int rtk_sysh_constraint_clk_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct clk_notifier_data *cnd = data;
	struct constraint_data *c = container_of(nb, struct constraint_data, clk_notifier_nb);
	int ret = 0;
	unsigned long old_max_freq = c->max_freq;

	pr_debug("%pC: event=%lu, old=%lu new=%lu", cnd->clk, event, cnd->old_rate, cnd->new_rate);

	switch (event) {
	case PRE_RATE_CHANGE:
		if (cnd->old_rate > cnd->new_rate)
			c->max_freq = cnd->new_rate;
		break;
	case POST_RATE_CHANGE:
		if (cnd->old_rate < cnd->new_rate)
			c->max_freq = cnd->new_rate;
		break;

	default:
		WARN_ON_ONCE(1);
		return NOTIFY_DONE;
	}

	if (old_max_freq != c->max_freq)
		ret = rtk_sysh_apply_constraint(c->bus);
	return notifier_from_errno(ret);
}

static int rtk_sysh_add_constraint(struct rtk_sysh_data *bus_data, struct device_node *np)
{
	struct constraint_data *c;
	int ret;

	c = devm_kzalloc(bus_data->dev, sizeof(*c), GFP_KERNEL);
	if (!c)
		return -ENOMEM;

	c->clk = devm_get_clk_from_child(bus_data->dev, np, NULL);
	if (IS_ERR(c->clk)) {
		dev_err(bus_data->dev, "failed to get clk for %s\n", np->name);
		return PTR_ERR(c->clk);
	}

	c->clk_notifier_nb.notifier_call = rtk_sysh_constraint_clk_cb;
	ret = clk_notifier_register(c->clk, &c->clk_notifier_nb);
	if (ret) {
		dev_err(bus_data->dev, "failed to register clk notifier for %s\n", np->name);
		return ret;
	}

	c->max_freq = ULONG_MAX;
	c->bus = bus_data;
	list_add(&c->node, &bus_data->constraint_list);
	return 0;
}

static int rtk_sysh_remove_constraints(struct rtk_sysh_data *bus_data)
{
	struct constraint_data *c, *tmp;

	list_for_each_entry_safe(c, tmp, &bus_data->constraint_list, node) {
		list_del(&c->node);
		clk_notifier_unregister(c->clk, &c->clk_notifier_nb);
	}
	return 0;
}

static int rtk_sysh_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_sysh_data *bus_data;
	struct device_node *child_np;
	int ret;

	bus_data = devm_kzalloc(dev, sizeof(*bus_data), GFP_KERNEL);
	if (!bus_data)
		return -ENOMEM;
	bus_data->dev = dev;
	INIT_LIST_HEAD(&bus_data->constraint_list);
	platform_set_drvdata(pdev, bus_data);

	bus_data->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(bus_data->clk))
		return dev_err_probe(dev, PTR_ERR(bus_data->clk), "failed to get clk\n");

	for_each_child_of_node(dev->of_node, child_np) {
		ret = rtk_sysh_add_constraint(bus_data, child_np);
		of_node_put(child_np);
		if (ret) {
			rtk_sysh_remove_constraints(bus_data);
			return dev_err_probe(dev, ret, "failed in %pOFn\n", child_np);
		}
	}

	return 0;
}

static void rtk_sysh_remove(struct platform_device *pdev)
{
	struct rtk_sysh_data *bus_data = platform_get_drvdata(pdev);

	rtk_sysh_remove_constraints(bus_data);
	platform_set_drvdata(pdev, NULL);
}

static const struct of_device_id rtk_sysh_match[] = {
	{ .compatible = "realtek,rtd1325-sysh", },
	{}
};

static struct platform_driver rtk_sysh_driver = {
	.probe    = rtk_sysh_probe,
	.remove   = rtk_sysh_remove,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "rtk-sysh",
		.of_match_table = of_match_ptr(rtk_sysh_match),
	},
};
module_platform_driver(rtk_sysh_driver);

MODULE_DESCRIPTION("Realtek Bus Controller driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-sysh");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
