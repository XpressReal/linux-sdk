// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#ifndef AUTCTL_H
#define AUTCTL_H

#include <linux/device.h>
#include <linux/regmap.h>

#define PART_NAME_LEN		32

struct autctl_device {
	struct device *dev;
	struct regmap *sb2_base;
	struct regmap *sb2_pmu_base;
	int irq;
	bool part_valid;
	const char *clus_part;
};

int rtk_aut_load_cluster(struct autctl_device *autctl);

#endif /* AUTCTL_H */
