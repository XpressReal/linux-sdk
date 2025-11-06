/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __RTK_LSADC_BUTTON_H
#define __RTK_LSADC_BUTTON_H

#define RTK_LSADC_BUTTON_MAX_NUMS 21
#define RTK_LSADC_BUTTON_MAX_INFO 96
#define RTK_LSADC_BUTTON_MAX_VS 100
#define MAX_CHANNEL 3
#define MAX_CHANNEL_KEY 7

struct button_info {
	u32 vs_cb;
	u32 vs_min;
	u32 vs_max;
};

struct rtk_lsadc_button_device;

struct rtk_lsadc_button_data {
	struct input_dev *input;
	u32 button_nums;
    u32 keycode[RTK_LSADC_BUTTON_MAX_NUMS];
	struct button_info button_info[MAX_CHANNEL][MAX_CHANNEL_KEY];
	struct rtk_lsadc_button_device *button_dev;
    int pre_vs;
	//struct kfifo vs_fifo;
};

struct rtk_lsadc_button_device {
	struct device *dev;
	struct iio_cb_buffer *buffer;
	struct rtk_lsadc_button_data button;
	struct iio_channel *chan;
	//struct workqueue_struct *wq;
	//struct work_struct work;
	struct hrtimer timer;
	spinlock_t lock; // fifo lock
};
#endif /* __RTK_LSADC_BUTTON_H */
