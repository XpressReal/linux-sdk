/*
 * hdmitx_dev.h - RTK hdmitx driver header file
 *
 * Copyright (C) 2017 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HDMITX_DEV_H__
#define __HDMITX_DEV_H__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include "hdmitx.h"

#define RPC_ALIGN_SZ 128

struct hdmitx_rpc_info {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	void *vaddr;
	dma_addr_t paddr;
	struct mutex lock;
};

typedef struct {
	char *name;
	struct miscdevice miscdev;
	struct device *dev;
	void __iomem *reg_base;
	void __iomem *pll_base;
	void __iomem *top_base;
	struct reset_control *reset_hdmi;
	struct clk *clk_hdmi;
	struct gpio_desc *hpd_gpio;
	struct gpio_desc *ctrl_5v_gpio;
	unsigned int hpd_irq;
	unsigned int hdmi_irq;
	enum HDMI_RXSENSE_MODE rxsense_mode;
	unsigned int use_new_mac;
	asoc_hdmi_t *hdmi_data;
	struct hdmitx_scdc_data	*tx_scdc; 
	struct hdmitx_extcon_data *e_data;
	struct rtk_hdmitx_frl *frl;
	struct fasync_struct *fasync;
	wait_queue_head_t hpd_wait;
	struct timer_list mute_gpio_timer;
	struct cec_notifier *cec;
	struct hdmitx_rpc_info rpc_info;
} hdmitx_device_t;

struct hdmitx_extcon_data {
	int			hpd_state;
	int			rxsense_state;
	int			connect_state;
	unsigned int irq;
	unsigned int hdmi_irq;
	struct gpio_desc	*pin;
	struct work_struct	work;
	struct timer_list rxsense_timer;
	struct mutex state_lock;
	struct extcon_dev   *edev;
	hdmitx_device_t     *tx_dev;
};

#define to_hdmitx_device(x)  container_of(x, hdmitx_device_t, dev)



extern int register_hdmitx_extcon_dev(hdmitx_device_t *device);
extern void deregister_hdmitx_extcon_dev(hdmitx_device_t *device);
extern int show_hpd_status(struct device *dev, bool real_time);
extern int rtk_hdmitx_extcon_suspend(hdmitx_device_t *tx_dev);
extern int rtk_hdmitx_extcon_resume(hdmitx_device_t *tx_dev);

int hdmitx_get_raw_edid(struct device *dev, unsigned char *edid);

#endif /* __HDMITX_DEV__H__ */

