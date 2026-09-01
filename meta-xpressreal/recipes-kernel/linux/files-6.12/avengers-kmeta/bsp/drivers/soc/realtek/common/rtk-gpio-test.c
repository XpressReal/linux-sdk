#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/seq_buf.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>


#define RTD_INPUT_VOLT (PIN_CONFIG_END + 4)

struct gpio_info {
	int recv_irq_num;
	int irq;
	int is_output;
};

struct test_group_info {
	struct gpio_descs *gpios;
	struct gpio_desc *output_gpio;
	struct gpio_info *gpio_state;
	int need_external_output;
	int is_vio_group;
	int test_volt;
	char name[32];
	struct kobject kobj;
	struct device *dev;
};

#define MAX_CHILD 40
struct gpio_test_info {
	struct kobject *kobj;
	struct test_group_info *groups[MAX_CHILD];
	int group_num;
};

static int check_test_result(struct test_group_info *info)
{
	int expect_time = 2;
	int i;
	int result = 0;
	int output_gpio;
	int failed_gpio[15];
	int failed_recv[15];
	int failed_count = 0;

	if (info->need_external_output) {
		output_gpio = desc_to_gpio(info->output_gpio) - GPIO_DYNAMIC_BASE;
	}

	for (i = 0; i < info->gpios->ndescs; i++) {
		if (info->gpio_state[i].is_output == 1) {
			output_gpio = desc_to_gpio(info->gpios->desc[i]) - GPIO_DYNAMIC_BASE;
			continue;
		}
		if (info->gpio_state[i].recv_irq_num != expect_time) {
			failed_gpio[failed_count] = desc_to_gpio(info->gpios->desc[i]) - GPIO_DYNAMIC_BASE;
			failed_recv[failed_count] = info->gpio_state[i].recv_irq_num;
			failed_count++;
			result = -1;
		}
	}

	if (result) {
		dev_err(info->dev, "Failed case:\n");
		dev_err(info->dev, "   output gpio: gpio_%d\n", output_gpio);
		dev_err(info->dev, "   input failed status:\n");
		for (i = 0; i < failed_count; i++)
			dev_err(info->dev, "       gpio_%d  recv_cnt:%d\n", failed_gpio[i], failed_recv[i]);
		return -1;
	} else {
		return 0;
	}
}

static irqreturn_t gpio_test_irq(int irq, void *priv)
{
	struct gpio_info *gpio_state = (struct gpio_info *)priv;

	gpio_state->recv_irq_num++;

	return IRQ_HANDLED;
}

static void test_normal_group(struct test_group_info *info)
{
	int i, j;
	int ret;
	int result = 0;

	for (i = 0; i < info->gpios->ndescs; i++) {
		gpiod_set_config(info->gpios->desc[i], pinconf_to_config_packed(PIN_CONFIG_POWER_SOURCE, info->test_volt));
		gpiod_direction_output(info->gpios->desc[i], 0);
		info->gpio_state[i].is_output = 1;
		for (j = 0; j < info->gpios->ndescs; j++) {
			char irq_name[32];

			if (j != i) {
				gpiod_set_config(info->gpios->desc[j], pinconf_to_config_packed(RTD_INPUT_VOLT, info->test_volt));
				gpiod_direction_input(info->gpios->desc[j]);
				info->gpio_state[j].irq = gpiod_to_irq(info->gpios->desc[j]);
				info->gpio_state[j].recv_irq_num = 0;
				info->gpio_state[j].is_output = 0;
				if(info->gpio_state[j].irq > 0) {
					snprintf(irq_name, sizeof(irq_name), "gpio_%d", desc_to_gpio(info->gpios->desc[j]) - GPIO_DYNAMIC_BASE);
					irq_set_irq_type(info->gpio_state[j].irq, IRQ_TYPE_EDGE_BOTH);
					ret = request_irq(info->gpio_state[j].irq, gpio_test_irq, IRQF_SHARED, irq_name, &info->gpio_state[j]);
					if (ret) {
						dev_err(info->dev, "cannot request irq %d, err:%d\n", info->gpio_state[j].irq, ret);
						return;
					}
				}
			}
		}
		gpiod_set_value(info->gpios->desc[i], 1);
		msleep(100);
		gpiod_set_value(info->gpios->desc[i], 0);
		msleep(100);
		result += check_test_result(info);
		for (j = 0; j < info->gpios->ndescs; j++) {
			if (j == i)
				continue;
			free_irq(info->gpio_state[j].irq, &info->gpio_state[j]);
		};
	}
	if (result)
		dev_err(info->dev, "Test Result: Failed\n");
	else
		dev_err(info->dev, "Test Result: Pass\n");
}


static void test_vio_group(struct test_group_info *info)
{
	int i, j;
	int ret;
	int result = 0;

	for (i = 0; i < info->gpios->ndescs; i++) {
		gpiod_set_config(info->gpios->desc[i], pinconf_to_config_packed(PIN_CONFIG_POWER_SOURCE, info->test_volt));
		gpiod_direction_output(info->gpios->desc[i], 0);
		info->gpio_state[i].is_output = 1;
		for (j = 0; j < info->gpios->ndescs; j++) {
			char irq_name[32];
			if (j != i) {
				gpiod_set_config(info->gpios->desc[j], pinconf_to_config_packed(RTD_INPUT_VOLT, info->test_volt));
				gpiod_direction_input(info->gpios->desc[j]);
				info->gpio_state[j].irq = gpiod_to_irq(info->gpios->desc[j]);
				info->gpio_state[j].recv_irq_num = 0;
				info->gpio_state[j].is_output = 0;
				if(info->gpio_state[j].irq > 0) {
					snprintf(irq_name, sizeof(irq_name), "gpio_%d", desc_to_gpio(info->gpios->desc[j]) - GPIO_DYNAMIC_BASE);
					irq_set_irq_type(info->gpio_state[j].irq, IRQ_TYPE_EDGE_BOTH);
					ret = request_irq(info->gpio_state[j].irq, gpio_test_irq, IRQF_SHARED, irq_name, &info->gpio_state[j]);
					if (ret) {
						dev_err(info->dev, "cannot request irq %d, err:%d\n", info->gpio_state[j].irq, ret);
						return;
					}
				}
			}
		}
		gpiod_set_value(info->gpios->desc[i], 1);
		msleep(100);
		gpiod_set_value(info->gpios->desc[i], 0);
		msleep(100);
		result = check_test_result(info);
		for (j = 0; j < info->gpios->ndescs; j++) {
			if (j == i)
				continue;
			free_irq(info->gpio_state[j].irq, &info->gpio_state[j]);
		};
	}
	if (result)
		dev_err(info->dev, "Test Result: Failed\n");
	else
		dev_err(info->dev, "Test Result: Pass\n");
}


static void test_external_output(struct test_group_info *info)
{
	int i;
	int ret;
	int result = 0;

	info->output_gpio = gpiod_get(info->dev, "output", GPIOD_ASIS);
	if (IS_ERR(info->output_gpio)) {
		dev_err(info->dev, "cannot get output gpios\n");
		return;
	}
	gpiod_set_config(info->output_gpio, pinconf_to_config_packed(PIN_CONFIG_POWER_SOURCE, info->test_volt));
	gpiod_direction_output(info->output_gpio, 0);

	for (i = 0; i < info->gpios->ndescs; i++) {
		char irq_name[32];
		gpiod_set_config(info->gpios->desc[i], pinconf_to_config_packed(RTD_INPUT_VOLT, info->test_volt));
		gpiod_direction_input(info->gpios->desc[i]);
		info->gpio_state[i].irq = gpiod_to_irq(info->gpios->desc[i]);
		info->gpio_state[i].recv_irq_num = 0;
		info->gpio_state[i].is_output = 0;
		if(info->gpio_state[i].irq > 0) {
			snprintf(irq_name, sizeof(irq_name), "gpio_%d", desc_to_gpio(info->gpios->desc[i]) - GPIO_DYNAMIC_BASE);
			irq_set_irq_type(info->gpio_state[i].irq, IRQ_TYPE_EDGE_BOTH);
			ret = request_irq(info->gpio_state[i].irq, gpio_test_irq, IRQF_SHARED, irq_name, &info->gpio_state[i]);
			if (ret) {
				dev_err(info->dev, "cannot request irq %d, err:%d\n", info->gpio_state[i].irq, ret);
				return;
			}
		}
	}
	gpiod_direction_output(info->output_gpio, 1);
	msleep(100);
	gpiod_direction_output(info->output_gpio, 0);
	msleep(100);
	result = check_test_result(info);
	for (i = 0; i < info->gpios->ndescs; i++) {
		free_irq(info->gpio_state[i].irq, &info->gpio_state[i]);
	};
	gpiod_put(info->output_gpio);
	if (result)
		dev_err(info->dev, "Test Result: Failed\n");
	else
		dev_err(info->dev, "Test Result: Pass\n");
}

static void gpio_test(struct test_group_info *info)
{
	info->gpios = gpiod_get_array_optional(info->dev, "input", GPIOD_ASIS);
	if (IS_ERR_OR_NULL(info->gpios)) {
		dev_err(info->dev, "cannot get input gpios\n");
		return;
	}

	info->gpio_state = kcalloc(info->gpios->ndescs, sizeof(struct gpio_info), GFP_KERNEL);
	if (!info->gpio_state) {
		dev_err(info->dev, "cannot allocate gpio_state\n");
		return;
	}
	dev_err(info->dev, "============Test Start============\n");
	if (info->need_external_output) {
		test_external_output(info);
	} else if(info->is_vio_group) {
		test_vio_group(info);
	} else {
		test_normal_group(info);
	}

	kfree(info->gpio_state);
	gpiod_put_array(info->gpios);

	dev_err(info->dev, "============Test End============\n");

	return;
};

static ssize_t name_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct device *dev = kobj_to_dev(kobj);
	struct test_group_info *info = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", info->name);
}


static struct kobj_attribute name_attr = __ATTR_RO(name);

static ssize_t external_gpio_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{

	return snprintf(buf, PAGE_SIZE, "Not implemented\n");
}
static struct kobj_attribute ext_gpio_attr = __ATTR_RO(external_gpio);


static ssize_t gpio_test_list_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct device *dev = kobj_to_dev(kobj);
	struct test_group_info *info = dev_get_drvdata(dev);
	struct seq_buf s;
	int count;
	int i;
	int gpio_num;

	seq_buf_init(&s, buf, PAGE_SIZE);

	if (info->is_vio_group) {
		seq_buf_printf(&s, "VIO group\n");
	} else if (info->need_external_output) {
		gpio_num = of_get_named_gpio(info->dev->of_node, "output-gpios", 0);
		seq_buf_printf(&s, "external output: gpio_%d\n", gpio_num - GPIO_DYNAMIC_BASE);
	} else {
		seq_buf_printf(&s, "normal group\n");
	}

	count = gpiod_count(info->dev, "input");
	if (count <= 0) {
		seq_buf_printf(&s, "no input gpio\n");
		goto out;
	}

	seq_buf_printf(&s, "input gpio:\n");
	for (i = 0; i < count; i++) {
		gpio_num = of_get_named_gpio(info->dev->of_node, "input-gpios", i);
		seq_buf_printf(&s, "    gpio_%d\n", gpio_num - GPIO_DYNAMIC_BASE);
	}

out:
	return seq_buf_used(&s);
}
static struct kobj_attribute test_list_attr = __ATTR_RO(gpio_test_list);


static ssize_t test_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct test_group_info *info = dev_get_drvdata(dev);
	int val;

	if (kstrtoint(buf, 10, &val) == 0 && val == 1) {
		gpio_test(info);
	}

	return count;
}

static struct kobj_attribute test_attr = __ATTR_WO(test);


static struct attribute *gpio_test_attrs[] = {
	&name_attr.attr,
	&ext_gpio_attr.attr,
	&test_list_attr.attr,
	&test_attr.attr,
	NULL,
};

static struct attribute_group gpio_test_attr_group = {
	.attrs = gpio_test_attrs,
};

static int gpio_group_config(struct test_group_info *info)
{
	int ret = 0;

	if (of_find_property(info->dev->of_node, "vio-group", NULL))
		info->is_vio_group = 1;
	else
		info->is_vio_group = 0;

	if (of_find_property(info->dev->of_node, "external-output", NULL))
		info->need_external_output = 1;
	else
		info->need_external_output = 0;

	ret = of_property_read_u32(info->dev->of_node, "test-volt", &info->test_volt);
	if (ret)
		info->test_volt = 0;

	return 0;
}

static int rtd_gpio_test_probe(struct platform_device *pdev)
{
	struct gpio_test_info *priv;
	struct device_node *child_np;
	int ret = 0;
	int id = 0;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	for_each_child_of_node(pdev->dev.of_node, child_np) {
		struct test_group_info *info = kzalloc(sizeof(*info), GFP_KERNEL);
		if (!info)
			continue;
		snprintf(info->name, sizeof(info->name), "%s", child_np->name);
		info->dev = kzalloc(sizeof(struct device), GFP_KERNEL);
		if (!info->dev) {
			kfree(info);
			continue;
		};

		device_initialize(info->dev);
		info->dev->parent = &pdev->dev;
		info->dev->of_node = child_np;
		info->dev->fwnode = of_fwnode_handle(child_np);
		dev_set_name(info->dev, child_np->name);
		ret = device_add(info->dev);
		if (ret) {
			dev_err(&pdev->dev, "device_add() error\n");
			kfree(info->dev);
			kfree(info);
			continue;
		}

		dev_set_drvdata(info->dev, info);

		ret = gpio_group_config(info);
		if (ret) {
			kfree(info);
			continue;
		}

		ret = sysfs_create_group(&info->dev->kobj, &gpio_test_attr_group);
		if (ret) {
			kobject_put(&info->kobj);
			continue;
		}

		priv->groups[id] = info;
		id++;
		if (id >= MAX_CHILD)
			break;
	}

	priv->group_num = id;
	platform_set_drvdata(pdev, priv);

        return ret;
}

static const struct of_device_id rtd_gpio_test_of_matches[] = {
	{ .compatible = "realtek,gpio-test" },
	{ /* Sentinel */ },
};


static struct platform_driver rtd_gpio_test_driver = {
	.driver = {
		.name = "rtd-gpio-test",
		.of_match_table = rtd_gpio_test_of_matches,
	},
	.probe = rtd_gpio_test_probe,
};


static int rtd_gpio_test(void)
{
	return platform_driver_register(&rtd_gpio_test_driver);
}

module_init(rtd_gpio_test);

MODULE_AUTHOR("TYChang <tychang@realtek.com>");
MODULE_DESCRIPTION("Realtek GPIO Test driver");
MODULE_LICENSE("GPL v2");
