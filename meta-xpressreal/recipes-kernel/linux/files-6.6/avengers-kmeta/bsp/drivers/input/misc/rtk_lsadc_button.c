// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 */
#define pr_fmt(fmt)  KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include "rtk_lsadc_button.h"

#define CREATE_TRACE_POINTS


static inline int in_range(u32 val, u32 min, u32 max)
{
	return (val >= min)&&(val <= max);
}


static int rtk_lsdac_button_vstate(struct rtk_lsadc_button_data *button, const u8 *val)
{
	int i,j;
	for(i = 0;i < MAX_CHANNEL;i++)
	{
		for(j = 0;j < MAX_CHANNEL_KEY;j++)
		{
			if (in_range(val[i], button->button_info[i][j].vs_min, button->button_info[i][j].vs_max))
				return i*MAX_CHANNEL_KEY+j;
		}
	}
	return -1;
}


static enum hrtimer_restart rtk_lsdac_button_timeout_handler(struct hrtimer *timer)
{
	struct rtk_lsadc_button_device *button_dev = container_of(timer, struct rtk_lsadc_button_device,
							      timer);
	struct rtk_lsadc_button_data *button = &button_dev->button;
	//unsigned long flags;
	if(button->pre_vs >= 0)
	{
		input_report_key(button->input, button->keycode[button->pre_vs], 0);
		input_sync(button->input);
		button->pre_vs = -1;
	}
	return HRTIMER_NORESTART;
}

static void rtk_lsdac_button_check(struct rtk_lsadc_button_device *button_dev,
				 struct rtk_lsadc_button_data *button, const u8 *val)
{
	unsigned long flags;
	int vs = rtk_lsdac_button_vstate(button, val);
	hrtimer_cancel(&button_dev->timer);
	spin_lock_irqsave(&button_dev->lock, flags);
	if(vs >= 0 && button->pre_vs == -1)
	{
		input_report_key(button->input, button->keycode[vs], 1);
		input_sync(button->input);
		button->pre_vs = vs;
	}
	spin_unlock_irqrestore(&button_dev->lock, flags);
	hrtimer_start(&button_dev->timer, ms_to_ktime(50), HRTIMER_MODE_REL);
	//queue_work(button_dev->wq, &button_dev->work);
}

static int rtk_lsdac_button_handle(const void *data, void *private)
{
	struct rtk_lsadc_button_device *button_dev = private;
	const u8 *buffer = data;
	int i;
	for (i = 0;i<MAX_CHANNEL;i++)
	{
		dev_dbg(button_dev->dev, "%s: data=%d\n", __func__, buffer[i]);
	}
	rtk_lsdac_button_check(button_dev, &button_dev->button, buffer);
	return 0;
}

static int rtk_lsadc_button_parse_dt(struct rtk_lsadc_button_device *button_dev,
				   struct device_node *np,
				   struct rtk_lsadc_button_data *button)
{
	int ret,len;
	int i = 0,j = 0;
	u32 val[RTK_LSADC_BUTTON_MAX_INFO];
	struct property *prop = NULL;

	prop = of_find_property(np, "buttonkey", NULL);
    if (prop) {
        button->button_nums = prop->length / sizeof(u32);
		len = button->button_nums;
    }
	else{
		return dev_err_probe(button_dev->dev, -EINVAL, "%pOF: failed to find buttonkey\n", np);
	}
	
	ret = of_property_read_u32_array(np, "buttonkey", val, len);
	if (ret)
		return dev_err_probe(button_dev->dev, ret, "%pOF: failed to get buttoncfg\n", np);
	for(i = 0;i < len;i++)
	{
		button->keycode[i] = val[i];
	}
	
	for(i = 0;i < MAX_CHANNEL;i ++)
	{
		for(j = 0;j < MAX_CHANNEL_KEY;j++)
		{
			button->button_info[i][j].vs_cb = j;
			button->button_info[i][j].vs_min = 84 + j*20;
			button->button_info[i][j].vs_max = 94 + j*20;
		}
	}

	return 0;
}

static int rtk_lsdac_button_unset_lsadc_cmpblk(struct rtk_lsadc_button_device *button_dev)
{
	char name[40];
	char buf[40];
	int ret;
	int i,j;
	struct rtk_lsadc_button_data button = button_dev->button;
	for(i = 0;i < MAX_CHANNEL;i++)
	{
		for(j = 0;j < MAX_CHANNEL_KEY;j++)
		{
			ret = snprintf(name, sizeof(name), "cmpblk%d_raw", button.button_info[i][j].vs_cb);
			ret = snprintf(buf, sizeof(buf), "%d:%d", 0, 0);
			ret = iio_write_channel_ext_info(&button_dev->chan[i], name, buf, ret);
		}
	}
	return ret;
}

static int rtk_lsdac_button_set_lsadc_cmpblk(struct rtk_lsadc_button_device *button_dev)
{
	char name[40];
	char buf[40];
	int ret;
	int i,j;
	struct rtk_lsadc_button_data button = button_dev->button;
	for(i = 0;i < MAX_CHANNEL;i++)
	{
		for(j = 0;j < MAX_CHANNEL_KEY;j++)
		{
			ret = snprintf(name, sizeof(name), "cmpblk%d_raw", button.button_info[i][j].vs_cb);
			ret = snprintf(buf, sizeof(buf), "%d:%d",button.button_info[i][j].vs_max,button.button_info[i][j].vs_min);
			ret = iio_write_channel_ext_info(&button_dev->chan[i], name, buf, ret);
			if (ret < 0)
				return ret;
		}
	}
	return ret;
}

static int rtk_lsdac_button_enable(struct rtk_lsadc_button_data *button)
{
	struct rtk_lsadc_button_device *button_dev = button->button_dev;
	int ret;

	ret = rtk_lsdac_button_set_lsadc_cmpblk(button_dev);
	if (ret < 0)
		rtk_lsdac_button_unset_lsadc_cmpblk(button_dev);

	ret = iio_channel_start_all_cb(button_dev->buffer);
	if (ret) {
		rtk_lsdac_button_unset_lsadc_cmpblk(button_dev);
	}
	return ret;
}

static void rtk_lsdac_button_disable(struct rtk_lsadc_button_data *button)
{
	struct rtk_lsadc_button_device *button_dev = button->button_dev;
	iio_channel_stop_all_cb(button_dev->buffer);
	rtk_lsdac_button_unset_lsadc_cmpblk(button_dev);
}

static int rtk_lsdac_button_open(struct input_dev *input)
{
	struct rtk_lsadc_button_data *button = input_get_drvdata(input);
	pm_runtime_get_sync(button->button_dev->dev);
	return 0;
}

static void rtk_lsdac_button_close(struct input_dev *input)
{
	struct rtk_lsadc_button_data *button = input_get_drvdata(input);
	pm_runtime_put(button->button_dev->dev);
}

static int rtk_lsadc_button_runtime_suspend(struct device *dev)
{
	struct rtk_lsadc_button_device *button_dev = dev_get_drvdata(dev);
	rtk_lsdac_button_disable(&button_dev->button);
	return 0;
}

static int rtk_lsadc_button_runtime_resume(struct device *dev)
{
	struct rtk_lsadc_button_device *button_dev = dev_get_drvdata(dev);
	rtk_lsdac_button_enable(&button_dev->button);
	return 0;
}

static const struct dev_pm_ops rtk_lsadc_button_pm_ops = {
	SET_RUNTIME_PM_OPS(rtk_lsadc_button_runtime_suspend,
			   rtk_lsadc_button_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static void __rtk_lsdac_button_remove(void *data)
{
	struct rtk_lsadc_button_device *button_dev = data;
	//destroy_workqueue(button_dev->wq);
	//kfifo_free(&button_dev->button.vs_fifo);
	iio_channel_release_all_cb(button_dev->buffer);
}

static int rtk_lsdac_button_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_lsadc_button_device *button_dev;
	struct rtk_lsadc_button_data *button;
	struct input_dev *input;
	int ret;
	int i = 0;
	button_dev = devm_kzalloc(dev, sizeof(*button_dev), GFP_KERNEL);
	if (!button_dev)
		return -ENOMEM;
	button_dev->dev = dev;
	button_dev->buffer = iio_channel_get_all_cb(dev, rtk_lsdac_button_handle, button_dev);
	if (IS_ERR(button_dev->buffer))
		return dev_err_probe(dev, PTR_ERR(button_dev->buffer),
				     "failed to allocate cb buffer\n");

	ret = devm_add_action_or_reset(dev, __rtk_lsdac_button_remove, button_dev);
	if (ret) {
		ret = dev_err_probe(dev, ret, "failed to add action\n");
		goto free_cb_buffer;
	}
	button_dev->chan = iio_channel_cb_get_channels(button_dev->buffer);
	//INIT_WORK(&button_dev->work, rtk_lsdac_button_work);
	hrtimer_init(&button_dev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	button_dev->timer.function = rtk_lsdac_button_timeout_handler;
	spin_lock_init(&button_dev->lock);
	button_dev->button.button_dev = button_dev;
	ret = rtk_lsadc_button_parse_dt(button_dev, dev->of_node, &button_dev->button);
	if (ret)
		return ret;
	button_dev->button.input = devm_input_allocate_device(dev);
	if (!button_dev->button.input)
		return dev_err_probe(dev, -ENOMEM, "failed to allocate input device\n");
	button = &button_dev->button;
	button->pre_vs = -1;
	input = button_dev->button.input;
	input->name = pdev->name;
	input->id.bustype = BUS_HOST;
	input->dev.parent = dev;
	input->open = rtk_lsdac_button_open;
	input->close = rtk_lsdac_button_close;
	__set_bit(EV_KEY, input->evbit);
	for(i = 0;i < button->button_nums;i++)
	{
		__set_bit(button->keycode[i], input->keybit);
	}
	input->keycode = button->keycode;
	input->keycodesize = sizeof(button->keycode[0]);
	input->keycodemax = button->button_nums;
	input_set_drvdata(input, &button_dev->button);
	platform_set_drvdata(pdev, button_dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);
	ret = input_register_device(input);
	if (ret) {
		pm_runtime_disable(dev);
		return dev_err_probe(dev, ret, "failed to register input device\n");
	}
	return 0;

free_cb_buffer:
	iio_channel_release_all_cb(button_dev->buffer);
	return ret;
}

static int rtk_lsdac_button_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id rtk_lsdac_button_match[] = {
	{ .compatible = "realtek,lsadc-button", },
	{}
};

static struct platform_driver rtk_lsdac_button_driver = {
	.probe    = rtk_lsdac_button_probe,
	.remove   = rtk_lsdac_button_remove,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "rtk-lsadc-button",
		.of_match_table = of_match_ptr(rtk_lsdac_button_match),
		.pm             = &rtk_lsadc_button_pm_ops,
	},
};

module_platform_driver(rtk_lsdac_button_driver);
MODULE_DESCRIPTION("Realtek LSADC Button Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-lsadc-button");
