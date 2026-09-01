// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include "sensor.h"

/*
 * Thermal sensor offset
 */
#define TM_SENSOR_CTRL0    0x00
#define TM_SENSOR_CTRL1    0x04
#define TM_SENSOR_CTRL2    0x08
#define TM_SENSOR_CTRL3    0x0C
#define TM_SENSOR_CTRL4    0x10
#define TM_SENSOR_CTRL5    0x14
#define TM_SENSOR_STATUS1  0x18
#define TM_SENSOR_STATUS2  0x1C

static void rtd129x_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x01904001);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x01924001);
}

static int rtd129x_sensor_init(struct thermal_sensor_device *tdev)
{
	rtd129x_sensor_reset(tdev);
	return 0;
}

static inline int rtd129x_sensor_get_temp(struct thermal_sensor_device *tdev,
	int *temp)
{
	u32 val = thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS1);

	*temp = sign_extend32(val, 18) * 1000 / 1024;
	return 0;
}

static void rtd129x_sensor_dump_reg(struct thermal_sensor_device *tdev)
{
	struct device *dev = tdev->dev;

	dev_warn(dev, "CTRL2   = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL2));
	dev_warn(dev, "STATUS1 = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS1));
	dev_warn(dev, "STATUS2 = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS2));
}

static void rtd139x_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x11904000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x11924000);
}

static int rtd139x_sensor_init(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x081fc000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x05772000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x11824000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x11924000);
	return 0;
}

static void rtd139x_sensor_dump_reg(struct thermal_sensor_device *tdev)
{
	struct device *dev = tdev->dev;

	dev_warn(dev, "CTRL0   = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL0));
	dev_warn(dev, "CTRL1   = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL1));
	dev_warn(dev, "CTRL2   = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL2));
	dev_warn(dev, "STATUS1 = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS1));
	dev_warn(dev, "STATUS2 = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS2));
}

static const struct thermal_sensor_hw_ops rtd129x_hw_ops = {
	.get_temp = rtd129x_sensor_get_temp,
	.reset    = rtd129x_sensor_reset,
	.init     = rtd129x_sensor_init,
	.dump_reg = rtd129x_sensor_dump_reg,
};

const struct thermal_sensor_desc rtd129x_sensor_desc = {
	.hw_ops = &rtd129x_hw_ops,
	.reset_time_ms = 25,
};

static const struct thermal_sensor_hw_ops rtd139x_sensor_hw_ops = {
	.get_temp = rtd129x_sensor_get_temp,
	.reset    = rtd139x_sensor_reset,
	.init     = rtd139x_sensor_init,
	.dump_reg = rtd139x_sensor_dump_reg,
};

const struct thermal_sensor_desc rtd139x_sensor_desc = {
	.hw_ops = &rtd139x_sensor_hw_ops,
	.reset_time_ms = 25,
};
