// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>


static struct i2c_client *of_i2c_register_device(struct i2c_adapter *adap,
						 struct device_node *node)
{
	struct i2c_client *client;
	struct i2c_board_info info;
	int ret;

	dev_dbg(&adap->dev, "of_i2c: register %pOF\n", node);

	ret = of_i2c_get_board_info(&adap->dev, node, &info);
	if (ret)
		return ERR_PTR(ret);

	client = i2c_new_client_device(adap, &info);
	if (IS_ERR(client))
		dev_err(&adap->dev, "of_i2c: Failure registering %pOF\n", node);

	return client;
}

static void populate_i2c_devices(struct i2c_adapter *adap, struct device_node *node)
{
	struct device_node *bus;
	struct i2c_client *client;

	bus = of_get_child_by_name(node, "i2c-bus");
	if (!bus)
		return;

	for_each_available_child_of_node(bus, node) {
		if (of_node_test_and_set_flag(node, OF_POPULATED))
			continue;

		client = of_i2c_register_device(adap, node);
		if (IS_ERR(client)) {
			dev_err(&adap->dev, "Failed to create I2C device for %pOF\n", node);
			of_node_clear_flag(node, OF_POPULATED);
		}
	}

	of_node_put(bus);
}

struct device_selector_data {
	struct device *dev;
	struct i2c_adapter *adap;
};

static void populate_platform_devices(struct device_selector_data *data, struct device_node *node)
{
	struct device_node *bus;

	bus = of_get_child_by_name(node, "platform-bus");
	if (!bus)
		return;
	of_platform_populate(bus, NULL, NULL, data->dev);
}

static void populate_devices(struct device_selector_data *data, struct device_node *node)
{
	populate_i2c_devices(data->adap, node);
	populate_platform_devices(data, node);
}

static void populate_device_by_selection(struct device_selector_data *data, int selection)
{
	struct device_node *node;
	u32 addr;
	int ret;

	for_each_available_child_of_node(data->dev->of_node, node) {
		ret = of_property_read_u32(node, "reg", &addr);
		if (ret || addr != selection)
			continue;
		dev_info(data->dev, "populate childs in %pOF\n", node);
		populate_devices(data, node);
	}
}

static int device_selector_i2c_read(struct device_selector_data *data, u16 addr, u8 reg, u8 *val)
{
	struct i2c_msg msg[2] = {
		{
			.addr = addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = val,
		}
	};
	int ret = 0;

	ret = i2c_transfer(data->adap, msg, 2);
	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

/**
 * selection:
 * 0 => pmic
 * 1 => v0.1 discreate dcdc
 * 2 => v0.2 discreate dcdc
 */
static int smallville_get_selection(struct device_selector_data *data, u32 *selection)
{
	int ret;
	u8 val;

	ret = device_selector_i2c_read(data, 0x43, 0x00, &val);
	 *selection = ret ? 0 : 1;

	if (*selection == 1) {
		ret = device_selector_i2c_read(data, 0x40, 0x01, &val);
		if (val == 0x10)
			*selection = 2;
	}

	return 0;
}

static int krypton_get_selection(struct device_selector_data *data, u32 *selection)
{
	int ret;
	u8 val;

	ret = device_selector_i2c_read(data, 0x43, 0x00, &val);
	 *selection = ret ? 0 : 1;
	return 0;
}

static int device_selector_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *adap_node;
	struct i2c_adapter *adap;
	u32 selection = 0;
	struct device_selector_data data = {};
	int (*get_selection)(struct device_selector_data *data, u32 *s);
	int ret = 0;

	get_selection = (void *)of_device_get_match_data(&pdev->dev);
	if (!get_selection)
		return dev_err_probe(dev, -EINVAL, "no get selection func\n");

	adap_node = of_parse_phandle(dev->of_node, "i2c-adapter", 0);
	if (!adap_node)
		return -ENODEV;

	adap = of_get_i2c_adapter_by_node(adap_node);
	of_node_put(adap_node);
	if (!adap)
		return dev_err_probe(dev, -EPROBE_DEFER, "failed to get i2c_adapter\n");

	data.dev = dev;
	data.adap = adap;

	ret = get_selection(&data, &selection);
	if (ret) {
		dev_err_probe(dev, ret, "failed to get selection\n");
		goto fail;
	}

	dev_info(dev, "%s: selection=%d\n", __func__, selection);
	populate_device_by_selection(&data, selection);

fail:
	i2c_put_adapter(adap);

	return ret;
}

static const struct of_device_id device_selector_ids[] = {
	{ .compatible = "smallville-device-selector",  .data = smallville_get_selection, },
	{ .compatible = "krypton-device-selector",  .data = krypton_get_selection, },
	{}
};

static struct platform_driver device_selector_drv = {
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "device-selector",
		.of_match_table = of_match_ptr(device_selector_ids),
		.suppress_bind_attrs = true,
	},
	.probe = device_selector_probe,
};
module_platform_driver(device_selector_drv);

MODULE_DESCRIPTION("Device Selector Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:device-selector");
