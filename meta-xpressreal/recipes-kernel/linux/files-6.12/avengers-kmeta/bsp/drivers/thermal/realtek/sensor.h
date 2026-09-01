// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#ifndef __REALTEK_THERMAL_H
#define __REALTEK_THERMAL_H

#include <linux/device.h>
#include <linux/list.h>
#include <linux/thermal.h>
#include <linux/notifier.h>
#include <linux/ktime.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/mutex.h>

struct thermal_sensor_device;
struct thermal_cooling_device;

struct thermal_sensor_hw_ops {
	int (*init)(struct thermal_sensor_device *);
	void (*reset)(struct thermal_sensor_device *);
	int (*get_temp)(struct thermal_sensor_device *, int *);
	void (*dump_reg)(struct thermal_sensor_device *);
};

struct thermal_sensor_desc {
	const struct thermal_sensor_hw_ops *hw_ops;
	int reset_time_ms;
	u32 has_valid_temp : 1;
	int valid_min_temp;
	int valid_max_temp;
};

struct thermal_sensor_device {
	struct device *dev;
	const struct thermal_zone_of_device_ops *ops;
	void *base;
	struct regmap *regmap;
	const struct thermal_sensor_desc *desc;
	struct mutex lock;
};

static inline u32 thermal_sensor_device_reg_read(struct thermal_sensor_device *tdev, u32 offset)
{
	return readl(tdev->base + offset);
}

static inline void thermal_sensor_device_reg_write(struct thermal_sensor_device *tdev,
						   u32 offset, u32 val)
{
	writel(val, tdev->base + offset);
}

extern const struct thermal_sensor_desc rtd129x_sensor_desc;
extern const struct thermal_sensor_desc rtd139x_sensor_desc;
extern const struct thermal_sensor_desc rtd1619_sensor_desc;
extern const struct thermal_sensor_desc rtd1319_sensor_desc;
extern const struct thermal_sensor_desc rtd1619b_sensor_desc;
extern const struct thermal_sensor_desc rtd1312c_sensor_desc;
extern const struct thermal_sensor_desc rtd1319d_sensor_desc;
extern const struct thermal_sensor_desc rtd1315e_sensor_desc;
extern const struct thermal_sensor_desc rtd1625_sc_wrap_sensor_desc;
extern const struct thermal_sensor_desc rtd1635_sc_wrap_sensor_desc;
extern const struct thermal_sensor_desc rtd1625_sys_sensor_desc;

#endif
