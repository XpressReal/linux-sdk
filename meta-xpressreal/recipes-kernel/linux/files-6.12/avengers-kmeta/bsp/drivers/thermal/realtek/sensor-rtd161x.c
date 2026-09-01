// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include "sensor.h"

/*
 * thermal sensor offset
 */
#define TM_SENSOR_CTRL0    0x00
#define TM_SENSOR_CTRL1    0x04
#define TM_SENSOR_CTRL2    0x08
#define TM_SENSOR_STATUS0  0x40
#define TM_SENSOR_STATUS1  0x44
#define TM_SENSOR_STATUS2  0x48

/* external control reg offset */
#define TM2_SENSOR_CTRL3  0x604

#define SYS_TM_SENSOR_CTRL0    0x0
#define SYS_TM_SENSOR_CTRL1    0x4
#define SYS_TM_SENSOR_CTRL2    0x8
#define SYS_TM_SENSOR_CTRL3    0xc
#define SYS_TM_SENSOR_STATUS0  0x10
#define SYS_TM_SENSOR_STATUS1  0x14
#define SYS_TM_SENSOR_STATUS2  0x18

static void rtk_sc_wrap_sensor_reg_dump(struct thermal_sensor_device *tdev)
{
	struct device *dev = tdev->dev;
	u32 val;

	dev_warn(dev, "CTRL0   = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL0));
	dev_warn(dev, "CTRL1   = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL1));
	dev_warn(dev, "CTRL2   = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL2));
	if (tdev->regmap) {
		regmap_read(tdev->regmap, TM2_SENSOR_CTRL3, &val);
		dev_warn(dev, "CTRL3   = %08x\n", val);
	}
	dev_warn(dev, "STATUS0 = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS0));
	dev_warn(dev, "STATUS1 = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS1));
	dev_warn(dev, "STATUS2 = %08x\n", thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS2));
}

static void rtd1619_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x07ce7ae1);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x00378228);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00011114);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00031114);
}

static void sc_wrap_disable_sensor_latch(struct thermal_sensor_device *tdev)
{
	regmap_write(tdev->regmap, TM2_SENSOR_CTRL3, 0xfffffffe);
}

static void rtd1319_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x08130000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x003723ff);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00011114);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00031114);

	sc_wrap_disable_sensor_latch(tdev);
}

static void rtd1619b_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x09000000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x00364400);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00011194);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00031194);

	sc_wrap_disable_sensor_latch(tdev);
}

static void rtd1312c_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x080A0000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x00372732);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00011114);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00031114);

	sc_wrap_disable_sensor_latch(tdev);
}

static int simple_sensor_init(struct thermal_sensor_device *tdev)
{
	tdev->desc->hw_ops->reset(tdev);
	return 0;
}

static inline int rtd1619_sensor_get_temp(struct thermal_sensor_device *tdev, int *temp)
{
	u32 val = thermal_sensor_device_reg_read(tdev, TM_SENSOR_STATUS0);

	*temp = sign_extend32(val, 18) * 1000 / 1024;
	return 0;
}

static const struct thermal_sensor_hw_ops rtd1619_sensor_hw_ops = {
	.get_temp = rtd1619_sensor_get_temp,
	.reset    = rtd1619_sensor_reset,
	.init     = simple_sensor_init,
	.dump_reg = rtk_sc_wrap_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1619_sensor_desc = {
	.hw_ops = &rtd1619_sensor_hw_ops,
	.reset_time_ms = 12,
};

static const struct thermal_sensor_hw_ops rtd1319_sensor_hw_ops = {
	.get_temp = rtd1619_sensor_get_temp,
	.reset    = rtd1319_sensor_reset,
	.init     = simple_sensor_init,
	.dump_reg = rtk_sc_wrap_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1319_sensor_desc = {
	.hw_ops = &rtd1319_sensor_hw_ops,
	.reset_time_ms = 12,
};

static const struct thermal_sensor_hw_ops rtd1619b_sensor_hw_ops = {
	.get_temp = rtd1619_sensor_get_temp,
	.reset    = rtd1619b_sensor_reset,
	.init     = simple_sensor_init,
	.dump_reg = rtk_sc_wrap_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1619b_sensor_desc = {
	.hw_ops = &rtd1619b_sensor_hw_ops,
	.reset_time_ms = 12,
};

static const struct thermal_sensor_hw_ops rtd1312c_sensor_hw_ops = {
	.get_temp = rtd1619_sensor_get_temp,
	.reset    = rtd1312c_sensor_reset,
	.init     = simple_sensor_init,
	.dump_reg = rtk_sc_wrap_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1312c_sensor_desc = {
	.hw_ops = &rtd1312c_sensor_hw_ops,
	.reset_time_ms = 12,
};

static void rtd1319d_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x08e80000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x00365199);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00011194);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00031194);

	regmap_write(tdev->regmap, TM2_SENSOR_CTRL3, 0);
}

static const struct thermal_sensor_hw_ops rtd1319d_sensor_hw_ops = {
	.get_temp = rtd1619_sensor_get_temp,
	.reset    = rtd1319d_sensor_reset,
	.init     = simple_sensor_init,
	.dump_reg = rtk_sc_wrap_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1319d_sensor_desc = {
	.hw_ops = &rtd1319d_sensor_hw_ops,
	.reset_time_ms = 12,
};

static void rtd1315e_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x08f83333);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x00365800);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00011194);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00031194);

	regmap_write(tdev->regmap, TM2_SENSOR_CTRL3, 0);
}

static const struct thermal_sensor_hw_ops rtd1315e_sensor_hw_ops = {
	.get_temp = rtd1619_sensor_get_temp,
	.reset    = rtd1315e_sensor_reset,
	.init     = simple_sensor_init,
	.dump_reg = rtk_sc_wrap_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1315e_sensor_desc = {
	.hw_ops = &rtd1315e_sensor_hw_ops,
	.reset_time_ms = 12,
};

static void rtd1625_sc_wrap_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x09060000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x003642b8);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00011194);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00031194);
	regmap_write(tdev->regmap, TM2_SENSOR_CTRL3, 0);
}


static int rtd1625_sc_wrap_sensor_init(struct thermal_sensor_device *tdev)
{
	u32 val = 0;

	val = thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL2);
	if ((val & 0x30000) == 0x30000)
		return 1;
	rtd1625_sc_wrap_sensor_reset(tdev);
	return 0;
}

static const struct thermal_sensor_hw_ops rtd1625_sc_wrap_sensor_hw_ops = {
	.get_temp = rtd1619_sensor_get_temp,
	.reset    = rtd1625_sc_wrap_sensor_reset,
	.init     = rtd1625_sc_wrap_sensor_init,
	.dump_reg = rtk_sc_wrap_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1625_sc_wrap_sensor_desc = {
	.hw_ops = &rtd1625_sc_wrap_sensor_hw_ops,
	.reset_time_ms = 12,
	.has_valid_temp = 1,
	.valid_max_temp = 130000,
	.valid_min_temp = -40000,
};

static void rtd1635_sc_wrap_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL0, 0x09060000);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL1, 0x00363800);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00011194);
	thermal_sensor_device_reg_write(tdev, TM_SENSOR_CTRL2, 0x00031194);
	regmap_write(tdev->regmap, TM2_SENSOR_CTRL3, 0);
}

static int rtd1635_sc_wrap_sensor_init(struct thermal_sensor_device *tdev)
{
	u32 val = 0;

	val = thermal_sensor_device_reg_read(tdev, TM_SENSOR_CTRL2);
	if ((val & 0x30000) == 0x30000)
		return 1;
	rtd1635_sc_wrap_sensor_reset(tdev);
	return 0;
}

static const struct thermal_sensor_hw_ops rtd1635_sc_wrap_sensor_hw_ops = {
	.get_temp = rtd1619_sensor_get_temp,
	.reset    = rtd1635_sc_wrap_sensor_reset,
	.init     = rtd1635_sc_wrap_sensor_init,
	.dump_reg = rtk_sc_wrap_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1635_sc_wrap_sensor_desc = {
	.hw_ops = &rtd1635_sc_wrap_sensor_hw_ops,
	.reset_time_ms = 12,
	.has_valid_temp = 1,
	.valid_max_temp = 130000,
	.valid_min_temp = -40000,
};

static void rtd1625_sys_sensor_reg_dump(struct thermal_sensor_device *tdev)
{
	struct device *dev = tdev->dev;

	dev_warn(dev, "CTRL0   = %08x\n", thermal_sensor_device_reg_read(tdev, SYS_TM_SENSOR_CTRL0));
	dev_warn(dev, "CTRL1   = %08x\n", thermal_sensor_device_reg_read(tdev, SYS_TM_SENSOR_CTRL1));
	dev_warn(dev, "CTRL2   = %08x\n", thermal_sensor_device_reg_read(tdev, SYS_TM_SENSOR_CTRL2));
	dev_warn(dev, "CTRL3   = %08x\n", thermal_sensor_device_reg_read(tdev, SYS_TM_SENSOR_CTRL3));
	dev_warn(dev, "STATUS0 = %08x\n", thermal_sensor_device_reg_read(tdev, SYS_TM_SENSOR_STATUS0));
	dev_warn(dev, "STATUS1 = %08x\n", thermal_sensor_device_reg_read(tdev, SYS_TM_SENSOR_STATUS1));
	dev_warn(dev, "STATUS2 = %08x\n", thermal_sensor_device_reg_read(tdev, SYS_TM_SENSOR_STATUS2));
}

static void rtd1625_sys_sensor_reset(struct thermal_sensor_device *tdev)
{
	thermal_sensor_device_reg_write(tdev, SYS_TM_SENSOR_CTRL0, 0x09060000);
	thermal_sensor_device_reg_write(tdev, SYS_TM_SENSOR_CTRL1, 0x003642b8);
	thermal_sensor_device_reg_write(tdev, SYS_TM_SENSOR_CTRL2, 0x00011194);
	thermal_sensor_device_reg_write(tdev, SYS_TM_SENSOR_CTRL2, 0x00031194);
	thermal_sensor_device_reg_write(tdev, SYS_TM_SENSOR_CTRL3, 0x00000000);
}

static inline int rtd1625_sys_sensor_get_temp(struct thermal_sensor_device *tdev, int *temp)
{
	u32 val = thermal_sensor_device_reg_read(tdev, SYS_TM_SENSOR_STATUS0);

	*temp = sign_extend32(val, 18) * 1000 / 1024;
	return 0;
}

static const struct thermal_sensor_hw_ops rtd1625_sys_sensor_hw_ops = {
	.get_temp = rtd1625_sys_sensor_get_temp,
	.reset    = rtd1625_sys_sensor_reset,
	.init     = simple_sensor_init,
	.dump_reg = rtd1625_sys_sensor_reg_dump,
};

const struct thermal_sensor_desc rtd1625_sys_sensor_desc = {
	.hw_ops = &rtd1625_sys_sensor_hw_ops,
	.reset_time_ms = 12,
	.has_valid_temp = 1,
	.valid_max_temp = 130000,
	.valid_min_temp = -40000,
};
