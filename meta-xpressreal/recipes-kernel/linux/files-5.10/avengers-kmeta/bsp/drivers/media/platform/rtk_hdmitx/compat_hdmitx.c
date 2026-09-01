/*
 * compat_hdmitx.c - RTK hdmitx driver
 *
 * Copyright (C) 2017 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/compat.h>
#include <linux/fs.h>

#include "compat_hdmitx.h"
#include "hdmitx_dev.h"
#include "hdmitx_api.h"

#if defined(CONFIG_CPU_V7)
/**
 * rtk_compat_hdmitx_ioctl - ioctl function of hdmitx miscdev
 * @file: hdmitx miscdev to be registered
 * @cmd: control command
 * @arg: arguments
 *
 * Return: 0 on success, -E* on failure
 */
long rtk_compat_hdmitx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret_value = -EFAULT;

	if (!file->f_op->unlocked_ioctl) {
		ret_value = -ENOTTY;
		goto exit;
	}

	ret_value = file->f_op->unlocked_ioctl(file, cmd, arg);

exit:
	return ret_value;
}

#else

/**
 * rtk_compat_hdmitx_ioctl - ioctl function of hdmitx miscdev
 * @file: hdmitx miscdev to be registered
 * @cmd: control command
 * @arg: arguments
 *
 * Return: 0 on success, -E* on failure
 */
long rtk_compat_hdmitx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *mdev = file->private_data;
	hdmitx_device_t *tx_dev = container_of(mdev, hdmitx_device_t, miscdev);
	void __user *up = compat_ptr(arg);
	struct full_edid32 __user f_edid32;
	struct full_edid __user f_edid;
	struct block_edid32 __user bedid32;
	struct block_edid __user bedid;
	struct fake_edid32 __user fake32;
	struct fake_edid __user fake;
	long err = -EFAULT;

	if (!file->f_op->unlocked_ioctl) {
		err = -ENOTTY;
		goto exit;
	}

	switch (cmd) {
	case HDMI_GET_FULL_EDID32:
		if (copy_from_user(&f_edid32, compat_ptr(arg), sizeof(f_edid32)))
			goto exit;

		f_edid = (struct full_edid) {
			.buf_size = f_edid32.buf_size,
			.edid_ptr = compat_ptr(f_edid32.edid_ptr),
		};

		err = get_full_edid(tx_dev, &f_edid);
		break;
	case HDMI_GET_EDID_BLOCK32:
		if (copy_from_user(&bedid32, compat_ptr(arg), sizeof(bedid32)))
			goto exit;

		bedid = (struct block_edid) {
			.block_index = bedid32.block_index,
			.block_size = bedid32.block_size,
			.edid_ptr = compat_ptr(bedid32.edid_ptr),
		};

		err = get_edid_block(tx_dev, &bedid);
		if (err)
			goto exit;
		break;
	case HDMI_SET_FAKE_EDID32:
		if (copy_from_user(&fake32, compat_ptr(arg), sizeof(fake32)))
			goto exit;

		fake = (struct fake_edid) {
			.size    = fake32.size,
			.data_ptr = compat_ptr(fake32.data_ptr),
		};

		err = set_fake_edid(tx_dev, &fake);
		break;
	default:
		err = file->f_op->unlocked_ioctl(file, cmd,
					(unsigned long)up);
	}

exit:
	return err;
}
#endif /* CONFIG_CPU_V7 */

