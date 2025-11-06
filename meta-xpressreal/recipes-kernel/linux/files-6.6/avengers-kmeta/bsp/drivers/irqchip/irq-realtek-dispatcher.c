// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek IRQ dispatcher
 *
 * Copyright (c) 2017 - 2021 Realtek Semiconductor Corporation
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

struct realtek_irq_dispatcher_info {
	struct irq_domain *domain;
	struct device *dev;
	int cfg_num;
	unsigned long int_en;
	int irq;
};

struct realtek_irq_int_cfg {
	char *name;
	unsigned int int_offset;
	unsigned int default_en;
};

struct realtek_irq_int_cfgs {
	const struct realtek_irq_int_cfg *cfg;
	int cfg_num;
};

static const struct realtek_irq_int_cfg rtd1625_cfg[] = {
    {
        .name = "iso",
        .int_offset = 0x40,
        .default_en = 0x4,
    },
    {
        .name = "isom",
        .int_offset = 0x10,
        .default_en = 0x0,
    },
};

static const struct realtek_irq_int_cfgs rtd1625_int_cfgs = {
    .cfg = rtd1625_cfg,
    .cfg_num = sizeof(rtd1625_cfg)/sizeof(rtd1625_cfg[0]),
};


static void realtek_irq_dispatcher_handle(struct irq_desc *desc)
{
	struct realtek_irq_dispatcher_info *info = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int i;
	unsigned int irq;

	chained_irq_enter(chip, desc);

	for_each_set_bit(i, &info->int_en, info->cfg_num) {
		irq = irq_find_mapping(info->domain, i);
		if (unlikely(irq <= 0))
			dev_err_ratelimited(info->dev, "failed to find mapping for hwirq %d\n", i);
		else
			generic_handle_irq(irq);
	}

	chained_irq_exit(chip, desc);
}

static int realtek_dispatcher_set_affinity(struct irq_data *d,
			const struct cpumask *mask_val, bool force)
{
	struct realtek_irq_dispatcher_info *info = irq_data_get_irq_chip_data(d);
	struct irq_chip *chip;
	struct irq_data *data;

	chip = irq_get_chip(info->irq);
	data = irq_get_irq_data(info->irq);

	irq_data_update_effective_affinity(d, cpu_online_mask);
	if (chip && chip->irq_set_affinity)
		return chip->irq_set_affinity(data, mask_val, force);
	else
		return -EINVAL;
}

static void realtek_dispatcher_mask_irq(struct irq_data *data)
{

}

static void realtek_dispatcher_unmask_irq(struct irq_data *data)
{

}

static void realtek_mux_enable_irq(struct irq_data *data)
{
	struct realtek_irq_dispatcher_info *info = irq_data_get_irq_chip_data(data);

	info->int_en |= BIT(data->hwirq);
}

static void realtek_mux_disable_irq(struct irq_data *data)
{
	struct realtek_irq_dispatcher_info *info = irq_data_get_irq_chip_data(data);

	info->int_en &= ~BIT(data->hwirq);
}

static struct irq_chip realtek_dispatcher_irq_chip = {
	.name			= "realtek-irq-dispatcher",
	.irq_mask		= realtek_dispatcher_mask_irq,
	.irq_unmask		= realtek_dispatcher_unmask_irq,
	.irq_set_affinity	= realtek_dispatcher_set_affinity,
	.irq_enable		= realtek_mux_enable_irq,
	.irq_disable		= realtek_mux_disable_irq,
};

static int realtek_dispatcher_irq_domain_map(struct irq_domain *d,
		unsigned int irq, irq_hw_number_t hw)
{
	struct realtek_irq_dispatcher_info *data = d->host_data;

	irq_set_chip_and_handler(irq, &realtek_dispatcher_irq_chip, handle_level_irq);
	irq_set_chip_data(irq, data);
	irq_set_probe(irq);

	return 0;
}

static const struct irq_domain_ops realtek_dispatcher_irq_domain_ops = {
	.xlate	= irq_domain_xlate_onecell,
	.map	= realtek_dispatcher_irq_domain_map,
};


static int realtek_irq_dispatcher_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct realtek_irq_dispatcher_info *info;
	const struct realtek_irq_int_cfgs *cfgs;
	struct device_node *syscon_np;
	struct regmap *base;
	int irq;
	int i;

	cfgs = of_device_get_match_data(dev);
	if (!cfgs)
		return -EINVAL;

	info = devm_kzalloc(dev, sizeof(struct realtek_irq_dispatcher_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	irq = irq_of_parse_and_map(dev->of_node, 0);
	if (irq <= 0)
		return irq;

	info->irq = irq;
	info->dev = dev;

	for (i = 0; i < cfgs->cfg_num; i++) {
		syscon_np = of_parse_phandle(dev->of_node, cfgs->cfg[i].name, 0);
		if (IS_ERR_OR_NULL(syscon_np))
			return -ENODEV;
		base = device_node_to_regmap(syscon_np);
		if (IS_ERR_OR_NULL(base)) {
			of_node_put(syscon_np);
			return -EINVAL;
		}
		regmap_write(base, cfgs->cfg[i].int_offset, cfgs->cfg[i].default_en);
	}

	info->cfg_num = cfgs->cfg_num;

	info->domain = irq_domain_add_linear(dev->of_node, 32, &realtek_dispatcher_irq_domain_ops, info);
	if (!info->domain)
		return -ENOMEM;

	irq_set_chained_handler_and_data(info->irq, realtek_irq_dispatcher_handle, info);

	return 0;
}

static const struct of_device_id realtek_irq_dispatcher_dt_matches[] = {
	{ .compatible = "realtek,rtd1625-iso-irq-dispatcher", .data = &rtd1625_int_cfgs },
	{ }
};
MODULE_DEVICE_TABLE(of, realtek_irq_dispatcher_dt_matches);

static struct platform_driver realtek_irq_dispatcher_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "irq-realtek-dispatcher",
		.of_match_table = realtek_irq_dispatcher_dt_matches,
		.suppress_bind_attrs = true,
	},
	.probe = realtek_irq_dispatcher_probe,
};

static int __init realtek_irq_dispatcher_init(void)
{
	return platform_driver_register(&realtek_irq_dispatcher_driver);
}
core_initcall(realtek_irq_dispatcher_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek DHC SoC Family interrupt dispatcher");
