/*
 * switch_hdmitx.c - RTK hdmitx driver
 *
 * Copyright (C) 2017 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/extcon-provider.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/regmap.h>

#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>

#include <media/cec-notifier.h>
#include "hdmitx_dev.h"
#include "hdmitx_api.h"
#include "rtk_edid.h"
#include "hdmitx_scdc.h"
#include "hdmitx_trace.h"
#include "hdmitx_reg.h"

static const unsigned int rtk_hdmi_cable[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

void hdmitx_hpd_debounce(struct device *dev, unsigned char ms)
{
	hdmitx_device_t *tx_dev = dev_get_drvdata(dev);
	struct hdmitx_extcon_data *e_data;

	e_data = tx_dev->e_data;
	gpiod_set_debounce(e_data->pin, ms*1000);
}

int hdmitx_switch_get_state(struct device *dev)
{
	hdmitx_device_t *tx_dev = dev_get_drvdata(dev);

	return tx_dev->e_data->connect_state;
}

static void hdmitx_hpd_work_func(struct work_struct *work)
{
	struct hdmitx_extcon_data *e_data = container_of(work,
					struct hdmitx_extcon_data, work);
	hdmitx_device_t *tx_dev;
	asoc_hdmi_t *tx_data;
	int pre_state;
	int cur_state;
	int hpd;
	int rxsense;
	struct edid_information *hdmitx_edid_info;

	tx_dev = e_data->tx_dev;
	tx_data = (asoc_hdmi_t *)tx_dev->hdmi_data;
	hdmitx_edid_info = &tx_data->hdmitx_edid_info;

	if (tx_dev->frl) {
		if (tx_dev->frl->in_training) {
			dev_info(tx_dev->dev, "Skip switch_work in link training");
			return;
		}
	}

	mutex_lock(&e_data->state_lock);

	hpd = gpiod_get_value(e_data->pin);

	if (tx_dev->rxsense_mode) {
		rxsense = hdmitx_get_rxsense(tx_dev);
		e_data->rxsense_state = rxsense;
		dev_info(tx_dev->dev, "HPD(%d) RxSense(%d)", hpd, rxsense);
	} else {
		rxsense = 1;
		dev_info(tx_dev->dev, "HPD(%d)", hpd);
	}

	mutex_unlock(&e_data->state_lock);

	pre_state = extcon_get_state(e_data->edev, EXTCON_DISP_HDMI);
	cur_state = hpd & rxsense;
	trace_hdmitx_hotplug_state(hpd, cur_state, rxsense);

	if (hpd != e_data->hpd_state) {

		if (hpd == 1) {
			hdmitx_get_sink_capability(tx_dev);

			kill_fasync(&tx_dev->fasync, SIGIO, POLL_IN);

			if (hdmitx_edid_info->scdc_capable & SCDC_RR_CAPABLE)
				enable_hdmitx_scdcrr(tx_dev, 1);

			set_i2s_output(tx_dev->dev, I2S_OUT_OFF);
			hdmitx_hpd_debounce(tx_dev->dev, 1);

			cec_notifier_set_phys_addr_from_edid(tx_dev->cec, hdmitx_edid_info->raw_edid);
		} else {
			kill_fasync(&tx_dev->fasync, SIGIO, POLL_HUP);

			enable_hdmitx_scdcrr(tx_dev, 0);
			set_i2s_output(tx_dev->dev, I2S_OUT_ON);
			hdmitx_hpd_debounce(tx_dev->dev, 30);
		}

		e_data->hpd_state = hpd;
	}

	if (cur_state != pre_state) {
		e_data->connect_state = cur_state;
		extcon_set_state_sync(e_data->edev, EXTCON_DISP_HDMI, cur_state);
		dev_info(tx_dev->dev, "Switch extcon state to %u", cur_state);
	}

	if (hpd && tx_dev->rxsense_mode == RXSENSE_TIMER_MODE) {
		mod_timer(&e_data->rxsense_timer, jiffies + msecs_to_jiffies(30));
	} else if (!hpd) {
		mutex_lock(&tx_data->info_lock);
		hdmitx_reset_sink_capability(tx_data);
		mutex_unlock(&tx_data->info_lock);

		/* keep cec enabled */
		//cec_notifier_phys_addr_invalidate(tx_dev->cec);
	}

	wake_up_interruptible(&tx_dev->hpd_wait);

}

static void hdmitx_rxsense_timer_cb(struct timer_list *t)
{
	struct hdmitx_extcon_data *e_data;
	hdmitx_device_t *tx_dev;
	int rxsense;
	int state_changed;

	e_data = from_timer(e_data, t, rxsense_timer);
	tx_dev = e_data->tx_dev;

	mutex_lock(&e_data->state_lock);

	rxsense = hdmitx_get_rxsense(tx_dev);
	state_changed = (rxsense != e_data->rxsense_state) ? 1:0;

	mutex_unlock(&e_data->state_lock);

	if (state_changed)
		schedule_work(&e_data->work);

	if (e_data->hpd_state)
		mod_timer(t, jiffies + msecs_to_jiffies(30));
}

static irqreturn_t hdmitx_switch_isr(int irq, void *data)
{
	struct hdmitx_extcon_data *e_data = data;

	schedule_work(&e_data->work);
	dev_dbg(e_data->tx_dev->dev, "%s", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t hdmitx_rxsense_isr(int irq, void *data)
{
	struct hdmitx_extcon_data *e_data = data;
	unsigned int reg_val;
	unsigned int rxupdated;

	regmap_read(e_data->tx_dev->top_base, RXST, &reg_val);
	rxupdated = RXST_get_rxupdated(reg_val);

	if (rxupdated) {
		reg_val = reg_val & (~RXST_rxupdated_mask);
		regmap_write(e_data->tx_dev->top_base, RXST, reg_val);
		schedule_work(&e_data->work);
	}

	return IRQ_HANDLED;
}

int register_hdmitx_extcon_dev(hdmitx_device_t *tx_dev)
{
	struct device *dev = tx_dev->dev;
	struct hdmitx_extcon_data *e_data;
	int ret;
	int hpd_state;

	dev_info(dev, "Register extcon dev");

	e_data = devm_kzalloc(dev, sizeof(struct hdmitx_extcon_data), GFP_KERNEL);
	tx_dev->e_data = e_data;
	/* Get hotplug pin state */
	e_data->pin  = tx_dev->hpd_gpio;
	e_data->hpd_state = 0;
	e_data->rxsense_state = 0;
	e_data->connect_state = 0;
	e_data->tx_dev = tx_dev;

	e_data->edev = devm_extcon_dev_allocate(dev, rtk_hdmi_cable);

	if (IS_ERR(e_data->edev)) {
		dev_err(dev, "failed to allocate extcon device");
		ret = -ENOMEM;
		goto end;
	}

	ret = devm_extcon_dev_register(dev, e_data->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device");
		goto free_edev;
	}

	mutex_init(&e_data->state_lock);

	gpiod_direction_input(e_data->pin);
	/* debounce time as 30ms */
	gpiod_set_debounce(e_data->pin, 30*1000);
	hpd_state = gpiod_get_value(e_data->pin);

	/* Init hotplug/RxSense work function and ISR */
	if (tx_dev->rxsense_mode == RXSENSE_TIMER_MODE)
		timer_setup(&e_data->rxsense_timer, hdmitx_rxsense_timer_cb, 0);

	INIT_WORK(&e_data->work, hdmitx_hpd_work_func);

	e_data->irq = tx_dev->hpd_irq;
	e_data->hdmi_irq = tx_dev->hdmi_irq;

	if (hpd_state)
		schedule_work(&e_data->work);

	irq_set_irq_type(e_data->irq, IRQ_TYPE_EDGE_BOTH);
	ret = request_irq(e_data->irq, hdmitx_switch_isr,
			IRQF_SHARED, "switch_hdmitx", e_data);
	if (ret) {
		dev_err(dev, "Cannot register hpd IRQ %d", e_data->irq);
		goto free_edev;
	}

	if ((tx_dev->rxsense_mode == RXSENSE_INTERRUPT_MODE) &&
			e_data->hdmi_irq) {

		ret = request_irq(e_data->hdmi_irq, hdmitx_rxsense_isr,
				IRQF_SHARED, "rxsense_isr", e_data);
		if (ret) {
			dev_err(dev, "Cannot register rxsense IRQ %d", e_data->hdmi_irq);
			tx_dev->rxsense_mode = RXSENSE_PASSIVE_MODE;
		} else {
			hdmitx_enable_rxsense_int(tx_dev);
			dev_info(tx_dev->dev, "Enable RxSense interrupt");
		}
	}

	goto end;

free_edev:
	devm_extcon_dev_free(dev, e_data->edev);
end:
	return ret;
}


void deregister_hdmitx_extcon_dev(hdmitx_device_t *tx_dev)
{
	return devm_extcon_dev_unregister(tx_dev->dev, tx_dev->e_data->edev);
}


int show_hpd_status(struct device *dev, bool real_time)
{
	hdmitx_device_t *tx_dev = dev_get_drvdata(dev);
	struct hdmitx_extcon_data *e_data;

	e_data = tx_dev->e_data; 

	if (real_time)
		return gpiod_get_value(e_data->pin);
	else
		return e_data->connect_state;
}
EXPORT_SYMBOL(show_hpd_status);


int rtk_hdmitx_extcon_suspend(hdmitx_device_t *tx_dev)
{
	struct hdmitx_extcon_data *e_data;

	e_data = tx_dev->e_data;

	/* hcy : dont need disable first */
	disable_irq(e_data->irq);
	dev_dbg(tx_dev->dev, "%s free irq=%x ", __func__, e_data->irq);

	if ((tx_dev->rxsense_mode == RXSENSE_INTERRUPT_MODE) &&
			e_data->hdmi_irq)
			disable_irq(e_data->hdmi_irq);

	if (tx_dev->rxsense_mode == RXSENSE_TIMER_MODE)
		del_timer_sync(&e_data->rxsense_timer);

	/* Cancel work and wait for it to finish */
	cancel_work_sync(&e_data->work);

	return 0;
}

int rtk_hdmitx_extcon_resume(hdmitx_device_t *tx_dev)
{
	asoc_hdmi_t *tx_data;
	struct hdmitx_extcon_data *e_data;
	struct edid_information *hdmitx_edid_info;
	int rxsense;

	tx_data = tx_dev->hdmi_data;
	e_data = tx_dev->e_data;
	hdmitx_edid_info = &tx_data->hdmitx_edid_info;

	/* set debounce time as 30ms */
	gpiod_set_debounce(e_data->pin, 30*1000);

	e_data->hpd_state = gpiod_get_value(e_data->pin);
	if (tx_dev->rxsense_mode) {
		rxsense = hdmitx_get_rxsense(tx_dev);
		e_data->rxsense_state = rxsense;
		dev_info(tx_dev->dev, "HPD(%d) RxSense(%u)",
					e_data->hpd_state, rxsense);
	} else {
		rxsense = 1;
		dev_info(tx_dev->dev, "HPD(%d)", e_data->hpd_state);
	}

	if (e_data->hpd_state == 0) {
		hdmitx_reset_sink_capability(tx_data);
	} else {
		if (!hdmitx_check_same_edid(tx_dev)) {
			hdmitx_reset_sink_capability(tx_data);
			hdmitx_get_sink_capability(tx_dev);
		}
		cec_notifier_set_phys_addr_from_edid(tx_dev->cec, hdmitx_edid_info->raw_edid);

		if (hdmitx_edid_info->scdc_capable & SCDC_RR_CAPABLE)
			enable_hdmitx_scdcrr(tx_dev, 1);
	}

	e_data->connect_state = e_data->hpd_state & rxsense;
	extcon_set_state_sync(e_data->edev, EXTCON_DISP_HDMI, e_data->connect_state);
	dev_info(tx_dev->dev, "Switch extcon state to %u", e_data->connect_state);

	enable_irq(e_data->irq);

	if ((tx_dev->rxsense_mode == RXSENSE_INTERRUPT_MODE) && e_data->hdmi_irq)
		enable_irq(e_data->hdmi_irq);
	else if ((tx_dev->rxsense_mode == RXSENSE_TIMER_MODE) && e_data->hpd_state)
		mod_timer(&e_data->rxsense_timer, jiffies + msecs_to_jiffies(300));

	return 0;
}

