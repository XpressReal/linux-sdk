// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

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

	regulator = devm_regulator_get(dev, data->supply_name);
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

static int regulator_supply_alias_legacy_probe(struct platform_device *pdev)
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

#define ALIAS_MAP_MAX 20

struct alias_map {
	struct device *dev;
	const char *id;
};

struct regulator_supply_alias_data {
	struct device *dev;
	const char *id;
	u32 n_maps;
	struct alias_map maps[ALIAS_MAP_MAX];
};

static int strcmp_suffix(const char *str, const char *suffix)
{
	unsigned int len, suffix_len;

	len = strlen(str);
	suffix_len = strlen(suffix);
	if (len <= suffix_len)
		return -1;
	return strcmp(str + len - suffix_len, suffix);
}

static const char *duplicate_supply_name(struct regulator_supply_alias_data *data,
					 const char *supply_property_name)
{
	char *id;

	id = devm_kstrdup(data->dev, supply_property_name, GFP_KERNEL);
	if (!id)
		return NULL;
	id[strlen(id) - 7] = '\0';
	return id;
}

static int lookup_regulator(struct regulator_supply_alias_data *data)
{
	struct device *dev = data->dev;
	struct property *p;

	for_each_property_of_node(dev->of_node, p) {
		const char *id;
		struct regulator *reg;

		if (strcmp_suffix(p->name, "-supply"))
			continue;

		id = duplicate_supply_name(data, p->name);
		if (!id)
			return -ENOMEM;

		reg = devm_regulator_get(dev, id);
		if (IS_ERR(reg)) {
			pr_debug("devm_regulator_get() id=%s returns %ld\n", id, PTR_ERR(reg));
		} else  {
			data->id = id;
			pr_debug("regulator id=%s\n", id);
			return 0;
		}
	}

	return -EPROBE_DEFER;
}

static int new_alias_map(struct regulator_supply_alias_data *data,
		   struct device *consumer_dev, const char *consumer_id)
{
	if (data->n_maps >= ALIAS_MAP_MAX)
		return -EINVAL;
	data->maps[data->n_maps].dev = consumer_dev;
	data->maps[data->n_maps].id = consumer_id;
	data->n_maps += 1;
	return 0;
}

static int consumer_setup_supplies(struct regulator_supply_alias_data *data,
				   struct device *consumer_dev)
{
	struct device_node *supply_np = data->dev->of_node;
	struct property *p;
	const char *consumer_id;
	int ret;

	for_each_property_of_node(consumer_dev->of_node, p) {
		if (strcmp_suffix(p->name, "-supply"))
			continue;

		if (of_parse_phandle(consumer_dev->of_node, p->name, 0) == supply_np) {
			consumer_id = duplicate_supply_name(data, p->name);
			if (!consumer_id)
				return -ENOMEM;

			ret = new_alias_map(data, consumer_dev, consumer_id);
			if (ret)
				dev_warn(data->dev, "new_alias_map(dev=%s, id=%s) returns %d\n",
					 dev_name(consumer_dev), consumer_id, ret);
		}
	}
	return 0;
}

static int setup_consumers(struct regulator_supply_alias_data *data)
{
	struct device *dev = data->dev;
	struct of_phandle_iterator it;
	int ret;

	of_for_each_phandle(&it, ret, dev->of_node, "consumer-devices", NULL, 0) {
		struct device *consumer_dev = NULL;
		struct device_node *consumer_np = it.node;
		int cpu_id;
		struct platform_device *pdev;

		dev_dbg(data->dev, "%s: phandle=%pOF\n", __func__, consumer_np);
		if (of_node_is_type(consumer_np, "cpu")) {
			cpu_id = of_cpu_node_to_id(consumer_np);
			consumer_dev = get_cpu_device(cpu_id);
		} else {
			pdev = of_find_device_by_node(consumer_np);
			if (pdev) {
				consumer_dev = &pdev->dev;
				platform_device_put(pdev);
			}
		}

		if (!consumer_dev)
			continue;

		dev_dbg(data->dev, "%s: consumer_dev=%s\n", __func__, dev_name(consumer_dev));
		ret = consumer_setup_supplies(data, consumer_dev);
		if (ret)
			dev_warn(data->dev, "consumer_setup_supplies() returns %d\n", ret);
	}

	return 0;
}

static int setup_aliases(struct regulator_supply_alias_data *data)
{
	u32 i;
	int ret;

	for (i = 0; i < data->n_maps; i++) {
		ret = regulator_register_supply_alias(data->maps[i].dev, data->maps[i].id,
						      data->dev, data->id);
		if (ret)
			dev_warn(data->dev, "map%d: regulator_register_supply_alias() returns %d\n",
				 i, ret);
	}

	return 0;
}

static int regulator_supply_alias_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regulator_supply_alias_data *data;
	int ret;

	if (of_get_child_count(dev->of_node) != 0) {
		dev_info(dev, "use legacy driver\n");
		return regulator_supply_alias_legacy_probe(pdev);
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->dev = dev;

	ret = lookup_regulator(data);
	if (ret)
		return dev_err_probe(dev, ret, "failed in lookup_regulator()\n");
	ret = setup_consumers(data);
	if (ret)
		return dev_err_probe(dev, ret, "failed in setup_consumers()\n");
	ret = setup_aliases(data);
	if (ret)
		return dev_err_probe(dev, ret, "failed in setup_aliases()\n");
	return 0;
}

static const struct of_device_id regulator_supply_alias_ids[] = {
	{ .compatible = "regulator-supply-alias" },
	{}
};

static struct platform_driver regulator_supply_alias_drv = {
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "regulator-supply-alias",
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
