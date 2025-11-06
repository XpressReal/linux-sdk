// SPDX-License-Identifier: GPL-2.0-only

#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

struct setter_consumer_data {
	struct device *dev;
	const char *name;
	int status;
};

struct setter_data {
	const char *supply_name;
	int num_consumers;
	struct setter_consumer_data consumers[];
};

static int regulator_supply_alias_setter_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct regulator *regulator;
	struct setter_data *data;
	const char *list_name = "consumer-devices";
	int num_consumers;
	int ret;
	int i;

	num_consumers = of_count_phandle_with_args(np, list_name, NULL);
	if (num_consumers <= 0) {
		num_consumers = 1;
		list_name = "consumer-dev";
	}

	data = devm_kzalloc(dev, struct_size(data, consumers, num_consumers), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->num_consumers = num_consumers;

	ret = of_property_read_string(dev->of_node, "supply-name", &data->supply_name);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get supply-name\n");

	regulator = devm_regulator_get(dev, data->supply_name);;
	if (IS_ERR(regulator)) {
		ret = PTR_ERR(regulator);
		if (ret == -EPROBE_DEFER)
			return dev_err_probe(dev, -EPROBE_DEFER, "wait for regulator\n");
		dev_dbg(dev, "ignore regulator: %d\n", ret);
		return 0;
	}

	for (i = 0; i < data->num_consumers; i++) {
		struct device_node *consumer_np = of_parse_phandle(dev->of_node, list_name, i);

		if (!consumer_np)
			continue;

		if (of_node_is_type(consumer_np, "cpu")) {
			int cpu_id = of_cpu_node_to_id(consumer_np);

			of_node_put(consumer_np);
			data->consumers[i].dev = get_cpu_device(cpu_id);
		} else {
			struct platform_device *consumer_pdev = of_find_device_by_node(consumer_np);

			of_node_put(consumer_np);
			if (consumer_pdev)
				data->consumers[i].dev = &consumer_pdev->dev;
		}

		if (of_property_read_string_index(np, "consumer-supply-names", i, &data->consumers[i].name))
			data->consumers[i].name = data->supply_name;

		if (!data->consumers[i].dev)
			return dev_err_probe(dev, -EPROBE_DEFER, "wait for consumer device%d in %s\n",
					     i, list_name);
	}

	for (i = 0; i < data->num_consumers; i++) {
		struct setter_consumer_data *c = &data->consumers[i];

		c->status = regulator_register_supply_alias(c->dev, c->name, dev, data->supply_name);
	}

	platform_set_drvdata(pdev, data);
	return 0;
}

static int regulator_supply_alias_setter_remove(struct platform_device *pdev)
{
	struct setter_data *data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < data->num_consumers; i++) {
		struct setter_consumer_data *c = &data->consumers[i];

		if (c->status)
			continue;

		regulator_unregister_supply_alias(c->dev, c->name);
	}

	return 0;
}

static const struct of_device_id regulator_supply_alias_setter_ids[] = {
	{ .compatible = "device-supply-alias-setter" },
	{}
};

static struct platform_driver regulator_supply_alias_setter_drv = {
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "regulator-supply-alia-setter",
		.of_match_table = of_match_ptr(regulator_supply_alias_setter_ids),
		.suppress_bind_attrs = true,
	},
	.probe = regulator_supply_alias_setter_probe,
	.remove = regulator_supply_alias_setter_remove,
};

static int device_match_of_parent(struct device *dev, const void *data)
{
	return dev->of_node && dev->of_node->parent == data;
}

static int regulator_supply_alias_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *found;
	int ret;

	ret = devm_of_platform_populate(dev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to populate child devices\n");

	found = driver_find_device(&regulator_supply_alias_setter_drv.driver, NULL, dev->of_node,
				   device_match_of_parent);
	if (!found)
		return dev_err_probe(dev, -EPROBE_DEFER, "no bound alias device found\n");
	put_device(found);

	return 0;
}

static const struct of_device_id regulator_supply_alias_ids[] = {
	{ .compatible = "regulator-supply-alias" },
	{}
};

static struct platform_driver regulator_supply_alias_drv = {
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "regulator-supply-alia",
		.of_match_table = of_match_ptr(regulator_supply_alias_ids),
		.suppress_bind_attrs = true,
	},
	.probe = regulator_supply_alias_probe,
};

static int __init regulator_supply_alias_init(void)
{
	int ret;

	ret = platform_driver_register(&regulator_supply_alias_drv);
	if (ret)
		return ret;

	ret = platform_driver_register(&regulator_supply_alias_setter_drv);
	if (ret)
		platform_driver_unregister(&regulator_supply_alias_drv);
	return ret;
}

static void __exit regulator_supply_alias_exit(void)
{
	platform_driver_unregister(&regulator_supply_alias_setter_drv);
	platform_driver_unregister(&regulator_supply_alias_drv);
}

module_init(regulator_supply_alias_init);
module_exit(regulator_supply_alias_exit);

MODULE_DESCRIPTION("Regulator Supply Alias Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:regulator-supply-alia");
