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
#include "rtk_lsadc_knob.h"

#define CREATE_TRACE_POINTS
#include "rtk_lsadc_knob_trace.h"

#define RTK_LSADC_KNOB_MAX_VSTATE 200

struct rtk_lsadc_knob_device;

struct rtk_lsadc_knob_data {
	struct input_dev *input;
	u32 keycodes[2];
	u32 vs_low_cb;
	u32 vs_low_min;
	u32 vs_low_max;
	u32 vs_low_len;
	u32 vs_high_cb;
	u32 vs_high_min;
	u32 vs_high_max;
	u32 vs_high_len;

	struct rtk_lsadc_knob_device *knob_dev;
	struct kfifo vs_fifo;

	struct {
		u32 state;
		ktime_t begin;
		ktime_t end;
	} cur;

	struct {
		struct rtk_lsadc_knob_vstate vs[20];
		bool start_from_index_1;
	} work;
};

struct rtk_lsadc_knob_device {
	struct device *dev;
	struct iio_cb_buffer *buffer;
	struct rtk_lsadc_knob_data knob;
	struct iio_channel *chan;
	struct workqueue_struct *wq;
	struct work_struct work;
	struct hrtimer timer;
	spinlock_t lock; // fifo lock
};

static u32 rtk_lsadc_knob_vstate(struct rtk_lsadc_knob_data *knob, u32 val)
{
	if (in_range(val, knob->vs_low_min, knob->vs_low_len))
		return RTK_LSADC_KNOB_VOLTAGE_LOW;
	if (in_range(val, knob->vs_high_min, knob->vs_high_len))
		return RTK_LSADC_KNOB_VOLTAGE_HIGH;
	return RTK_LSADC_KNOB_VOLTAGE_ZERO;
}

static void rtk_lsadc_knob_in(struct rtk_lsadc_knob_data *knob, u32 state, u32 duration,
			      bool with_zero)
{
	struct rtk_lsadc_knob_vstate vs[2] = {
		{ .state = state, .duration = duration, },
		{ .state = RTK_LSADC_KNOB_VOLTAGE_ZERO, },
	};
	u32 esize = with_zero ? sizeof(vs) : sizeof(struct rtk_lsadc_knob_vstate);
	int ret;

	ret = kfifo_in(&knob->vs_fifo, vs, esize);
	if (ret != esize)
		dev_warn(knob->knob_dev->dev, "%s: ret=%d, esize=%d, kfifo_len=%d\n",
			 __func__, ret, esize, (int)kfifo_len(&knob->vs_fifo));
}

static void rtk_lsadc_knob_work(struct work_struct *work)
{
	struct rtk_lsadc_knob_device *knob_dev = container_of(work, struct rtk_lsadc_knob_device,
							      work);
	struct rtk_lsadc_knob_data *knob = &knob_dev->knob;
	int ret;
	u32 size;
	u32 len;
	int i;
	void *p;
	u32 key;

again:
	if (knob->work.start_from_index_1) {
		knob->work.start_from_index_1 = false;
		p = &knob->work.vs[1];
		size = min(kfifo_len(&knob->vs_fifo),
			   sizeof(knob->work.vs) - sizeof(knob->work.vs[0]));
	} else {
		p = &knob->work.vs[0];
		size = min(kfifo_len(&knob->vs_fifo), sizeof(knob->work.vs));
	}
	len = size / sizeof(*knob->work.vs);

	if (len == 0)
		return;

	ret = kfifo_out(&knob->vs_fifo, p, size);
	for (i = 0; (i + 1) < len; ) {
		struct rtk_lsadc_knob_vstate *p = &knob->work.vs[i];
		struct rtk_lsadc_knob_vstate *c = &knob->work.vs[i + 1];
		u32 step = 1;
		u32 rotate = RTK_LSADC_KNOB_ROTATE_NONE;
		u32 ratio = 0;

		if (p->state == RTK_LSADC_KNOB_VOLTAGE_ZERO || p->duration == 0) {
			i += 1;
			continue;
		}

		if (p->state == RTK_LSADC_KNOB_VOLTAGE_LOW &&
		    c->state == RTK_LSADC_KNOB_VOLTAGE_HIGH) {
			ratio = c->duration * 100 / p->duration;
			if (200 > ratio && 75 < ratio) {
				rotate = RTK_LSADC_KNOB_ROTATE_ANTICLOCKWISE;
				step = 2;
			}
		} else if (p->state == RTK_LSADC_KNOB_VOLTAGE_HIGH &&
			   c->state == RTK_LSADC_KNOB_VOLTAGE_LOW) {
			ratio = c->duration * 100 / p->duration;
			if (0 < ratio && 25 > ratio) {
				rotate = RTK_LSADC_KNOB_ROTATE_CLOCKWISE;
				step = 2;
			}
		}
		i += step;

		trace_rtk_lsadc_knob_rotate(knob_dev->dev, p, c, ratio, rotate);

		if (rotate != RTK_LSADC_KNOB_ROTATE_NONE) {
			key = rotate == RTK_LSADC_KNOB_ROTATE_ANTICLOCKWISE ? 1 : 0;
			input_report_key(knob->input, knob->keycodes[key], 1);
			input_sync(knob->input);
			input_report_key(knob->input, knob->keycodes[key], 0);
			input_sync(knob->input);
		}
	}

	if ((i + 1) == len) {
		knob->work.vs[0] = knob->work.vs[i];
		knob->work.start_from_index_1 = true;
	}
	goto again;
}

static inline void rtk_lsadc_knob_new_state(struct rtk_lsadc_knob_data *knob, u32 state,
					    ktime_t time)
{
	knob->cur.begin = time;
	knob->cur.end = time;
	knob->cur.state = state;
}

static enum hrtimer_restart rtk_lsadc_knob_timeout_handler(struct hrtimer *timer)
{
	struct rtk_lsadc_knob_device *knob_dev = container_of(timer, struct rtk_lsadc_knob_device,
							      timer);
	struct rtk_lsadc_knob_data *knob = &knob_dev->knob;
	unsigned long flags;

	spin_lock_irqsave(&knob_dev->lock, flags);
	rtk_lsadc_knob_in(knob, knob->cur.state,
			  ktime_to_us(ktime_sub(knob->cur.end, knob->cur.begin)), true);

	rtk_lsadc_knob_new_state(knob, RTK_LSADC_KNOB_VOLTAGE_ZERO, ktime_get());
	spin_unlock_irqrestore(&knob_dev->lock, flags);

	queue_work(knob_dev->wq, &knob_dev->work);
	return HRTIMER_NORESTART;
}

static void rtk_lsadc_knob_check(struct rtk_lsadc_knob_device *knob_dev,
				 struct rtk_lsadc_knob_data *knob, u32 val)
{
	u32 vs = rtk_lsadc_knob_vstate(knob, val);
	ktime_t time = ktime_get();
	unsigned long flags;
	bool work_should_start = false;

	hrtimer_cancel(&knob_dev->timer);

	spin_lock_irqsave(&knob_dev->lock, flags);
	if (ktime_after(time, ktime_add(knob->cur.end, ms_to_ktime(50)))) {
		rtk_lsadc_knob_in(knob, knob->cur.state,
				  ktime_to_us(ktime_sub(knob->cur.end, knob->cur.begin)), true);
		work_should_start = true;

		rtk_lsadc_knob_new_state(knob, vs, time);
	} else if (vs == knob->cur.state) {
		knob->cur.end = time;
	} else {
		rtk_lsadc_knob_in(knob, knob->cur.state,
				  ktime_to_us(ktime_sub(knob->cur.end, knob->cur.begin)),
				  false);
		rtk_lsadc_knob_new_state(knob, vs, time);
	}
	spin_unlock_irqrestore(&knob_dev->lock, flags);

	hrtimer_start(&knob_dev->timer, ms_to_ktime(50), HRTIMER_MODE_REL);
	if (work_should_start)
		queue_work(knob_dev->wq, &knob_dev->work);
}

static int rtk_lsadc_knob_handle(const void *data, void *private)
{
	struct rtk_lsadc_knob_device *knob_dev = private;
	const u8 *buffer = data;

	dev_dbg(knob_dev->dev, "%s: data=%d\n", __func__, buffer[0]);
	rtk_lsadc_knob_check(knob_dev, &knob_dev->knob, buffer[0]);

	return 0;
}

static int rtk_lsadc_knob_parse_dt(struct rtk_lsadc_knob_device *knob_dev,
				   struct device_node *np,
				   struct rtk_lsadc_knob_data *knob)
{
	u32 val[6] = {};
	int ret;

	ret = of_property_read_u32_array(np, "linux,keycodes", knob->keycodes, 2);
	if (ret)
		return dev_err_probe(knob_dev->dev, ret, "%pOF: failed to get keycodes\n", np);

	ret = of_property_read_u32_array(np, "realtek,vscfg", val, 6);
	if (ret)
		return dev_err_probe(knob_dev->dev, ret, "%pOF: failed to get vscfg\n", np);

	knob->vs_low_cb = val[0];
	knob->vs_low_max = val[1];
	knob->vs_low_min = val[2];
	knob->vs_low_len = knob->vs_low_max - knob->vs_low_min + 1;
	knob->vs_high_cb = val[3];
	knob->vs_high_max = val[4];
	knob->vs_high_min = val[5];
	knob->vs_high_len = knob->vs_high_max - knob->vs_high_min + 1;

	if (in_range(knob->vs_high_min, knob->vs_low_min, knob->vs_low_len))
		return dev_err_probe(knob_dev->dev, -EINVAL, "%pOF: invalid range\n", np);
	if (knob->vs_low_cb == knob->vs_high_cb)
		return dev_err_probe(knob_dev->dev, -EINVAL,
				     "%pOF: duplicated cmpblk in realtek,vscfg\n", np);
	return 0;
}

static int rtk_lsadc_knob_set_lsadc_cmpblk(struct rtk_lsadc_knob_device *knob_dev,
					   u32 cmpblk, u32 vmax, u32 vmin)
{
	char name[40];
	char buf[40];
	int ret;

	ret = snprintf(name, sizeof(name), "cmpblk%d_raw", cmpblk);
	ret = snprintf(buf, sizeof(buf), "%d:%d", vmax, vmin);
	return iio_write_channel_ext_info(knob_dev->chan, name, buf, ret);
}

static int rtk_lsadc_knob_enable(struct rtk_lsadc_knob_data *knob)
{
	struct rtk_lsadc_knob_device *knob_dev = knob->knob_dev;
	int ret;

	ret = iio_channel_start_all_cb(knob_dev->buffer);
	if (ret < 0)
		return ret;
	ret = rtk_lsadc_knob_set_lsadc_cmpblk(knob_dev, knob->vs_low_cb, knob->vs_low_max,
					      knob->vs_low_min);
	if (ret < 0)
		goto stop_all_cb;
	ret = rtk_lsadc_knob_set_lsadc_cmpblk(knob_dev, knob->vs_high_cb, knob->vs_high_max,
					      knob->vs_high_min);
	if (ret < 0)
		goto clear_vs_low_cb;
	return 0;

clear_vs_low_cb:
	rtk_lsadc_knob_set_lsadc_cmpblk(knob_dev, knob->vs_low_cb, 0, 0);
stop_all_cb:
	iio_channel_stop_all_cb(knob_dev->buffer);
	return ret;
}

static void rtk_lsadc_knob_disable(struct rtk_lsadc_knob_data *knob)
{
	struct rtk_lsadc_knob_device *knob_dev = knob->knob_dev;

	rtk_lsadc_knob_set_lsadc_cmpblk(knob_dev, knob->vs_high_cb, 0, 0);
	rtk_lsadc_knob_set_lsadc_cmpblk(knob_dev, knob->vs_low_cb, 0, 0);
	iio_channel_stop_all_cb(knob_dev->buffer);
}

static int rtk_lsadc_knob_open(struct input_dev *input)
{
	struct rtk_lsadc_knob_data *knob = input_get_drvdata(input);

	pm_runtime_get_sync(knob->knob_dev->dev);
	return 0;
}

static void rtk_lsadc_knob_close(struct input_dev *input)
{
	struct rtk_lsadc_knob_data *knob = input_get_drvdata(input);

	pm_runtime_put(knob->knob_dev->dev);
}

static int rtk_lsadc_knob_runtime_suspend(struct device *dev)
{
	struct rtk_lsadc_knob_device *knob_dev = dev_get_drvdata(dev);

	rtk_lsadc_knob_disable(&knob_dev->knob);
	return 0;
}

static int rtk_lsadc_knob_runtime_resume(struct device *dev)
{
	struct rtk_lsadc_knob_device *knob_dev = dev_get_drvdata(dev);
	int ret;

	ret = rtk_lsadc_knob_enable(&knob_dev->knob);
	if (ret)
		dev_warn(dev, "failed to enable lsadc\n");
	return ret;
}

static const struct dev_pm_ops rtk_lsadc_knob_pm_ops = {
	SET_RUNTIME_PM_OPS(rtk_lsadc_knob_runtime_suspend,
			   rtk_lsadc_knob_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static void __rtk_lsadc_knob_remove(void *data)
{
	struct rtk_lsadc_knob_device *knob_dev = data;

	destroy_workqueue(knob_dev->wq);
	kfifo_free(&knob_dev->knob.vs_fifo);
	iio_channel_release_all_cb(knob_dev->buffer);
}

static int rtk_lsadc_knob_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_lsadc_knob_device *knob_dev;
	struct rtk_lsadc_knob_data *knob;
	struct input_dev *input;
	int ret;

	knob_dev = devm_kzalloc(dev, sizeof(*knob_dev), GFP_KERNEL);
	if (!knob_dev)
		return -ENOMEM;

	knob_dev->dev = dev;
	knob_dev->buffer = iio_channel_get_all_cb(dev, rtk_lsadc_knob_handle, knob_dev);
	if (IS_ERR(knob_dev->buffer))
		return dev_err_probe(dev, PTR_ERR(knob_dev->buffer),
				     "failed to allocate cb buffer\n");

	ret = kfifo_alloc(&knob_dev->knob.vs_fifo,
			  sizeof(struct rtk_lsadc_knob_vstate) * RTK_LSADC_KNOB_MAX_VSTATE,
			  GFP_KERNEL);
	if (ret < 0) {
		ret = dev_err_probe(dev, ret, "failed to alloc vs fifo\n");
		goto free_cb_buffer;
	}

	knob_dev->wq = alloc_workqueue("lsadc_knob-%s", WQ_UNBOUND, 1, dev_name(dev));
	if (!knob_dev->wq) {
		ret = dev_err_probe(dev, -ENOMEM, "failed to alloc wq\n");
		goto free_kfifo;
	}

	ret = devm_add_action_or_reset(dev, __rtk_lsadc_knob_remove, knob_dev);
	if (ret) {
		ret = dev_err_probe(dev, ret, "failed to add action\n");
		goto destroy_wq;
	}

	knob_dev->chan = iio_channel_cb_get_channels(knob_dev->buffer);
	INIT_WORK(&knob_dev->work, rtk_lsadc_knob_work);
	hrtimer_init(&knob_dev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	knob_dev->timer.function = rtk_lsadc_knob_timeout_handler;
	spin_lock_init(&knob_dev->lock);

	knob_dev->knob.knob_dev = knob_dev;
	ret = rtk_lsadc_knob_parse_dt(knob_dev, dev->of_node, &knob_dev->knob);
	if (ret)
		return ret;

	knob_dev->knob.input = devm_input_allocate_device(dev);
	if (!knob_dev->knob.input)
		return dev_err_probe(dev, -ENOMEM, "failed to allocate input device\n");

	knob = &knob_dev->knob;
	input = knob_dev->knob.input;
	input->name = "realtek-lsadc-knob";
	input->id.bustype = BUS_HOST;
	input->dev.parent = dev;
	input->open = rtk_lsadc_knob_open;
	input->close = rtk_lsadc_knob_close;

	__set_bit(EV_KEY, input->evbit);
	__set_bit(knob->keycodes[0], input->keybit);
	__set_bit(knob->keycodes[1], input->keybit);
	input->keycode = knob->keycodes;
	input->keycodesize = sizeof(knob->keycodes[0]);
	input->keycodemax = 2;

	input_set_drvdata(input, &knob_dev->knob);
	platform_set_drvdata(pdev, knob_dev);

	pm_runtime_set_suspended(dev);
	pm_runtime_enable(dev);

	ret = input_register_device(input);
	if (ret) {
		pm_runtime_disable(dev);
		return dev_err_probe(dev, ret, "failed to register input device\n");
	}

	return 0;

destroy_wq:
	destroy_workqueue(knob_dev->wq);
free_kfifo:
	kfifo_free(&knob_dev->knob.vs_fifo);
free_cb_buffer:
	iio_channel_release_all_cb(knob_dev->buffer);
	return ret;
}

static void rtk_lsadc_knob_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
}

static const struct of_device_id rtk_lsadc_knob_match[] = {
	{ .compatible = "realtek,lsadc-knob", },
	{}
};

static struct platform_driver rtk_lsadc_knob_driver = {
	.probe    = rtk_lsadc_knob_probe,
	.remove   = rtk_lsadc_knob_remove,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "rtk-lsadc-knob",
		.of_match_table = of_match_ptr(rtk_lsadc_knob_match),
		.pm             = &rtk_lsadc_knob_pm_ops,
	},
};
module_platform_driver(rtk_lsadc_knob_driver);
MODULE_DESCRIPTION("Realtek LSADC Knob Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtk-lsadc-knob");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");

