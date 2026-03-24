/*
 *
 * Copyright (C) 2025 Realtek Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <log.h>
#include <dm.h>
#include <common.h>
#include <asm/io.h>
#include <clk-uclass.h>
#include "clk_rtk.h"

static bool clk_rtk_ctrl_is_enable(struct clk *clk)
{
	struct clk_rtk_ctrl_priv *priv = dev_get_priv(clk->dev);
	struct rtk_gate *gate = priv->gates[clk->id];
	uint32_t val;

	val = readl(priv->reg_base + gate->ofs);
	return (val & (1 << gate->bit_idx)) ? true : false;
}

static int clk_rtk_ctrl_of_xlate(struct clk *clk,
				struct ofnode_phandle_args *args)
{
	if (!args || args->args_count > 1) {
		log_err("Invalid args_count: %d\n", args->args_count);
		return -EINVAL;
	}

	clk->id = (args->args_count) ? args->args[0] : 0;

	return 0;
}

static int clk_rtk_ctrl_disable(struct clk *clk)
{
	struct clk_rtk_ctrl_priv *priv = dev_get_priv(clk->dev);
	struct rtk_gate *gate = priv->gates[clk->id];
	uint32_t val;

	if (!gate) {
		log_err("Invalid clk, id: %lu\n", clk->id);
		return -EINVAL;
	}

	/* TODO: handle parent clk*/

	val = readl(priv->reg_base + gate->ofs);
	val &= ~(1 << gate->bit_idx);
	if (gate->write_en)
		val |= (1 << (gate->bit_idx + 1));
	writel(val, priv->reg_base + gate->ofs);
	gate->enable_count--;

	return 0;
}

static int clk_rtk_ctrl_enable_parent(struct clk *clk)
{
	struct clk_rtk_ctrl_priv *clk_priv = dev_get_priv(clk->dev);
	struct rtk_gate *gate = clk_priv->gates[clk->id];
	struct clk_rtk_ctrl_priv *priv;
	struct udevice *dev;
	struct uclass *uc;
	struct clk parent_clk;
	int ret;

	if (!gate->parent)
		return 0;

	ret = uclass_get(UCLASS_CLK, &uc);
	if (ret)
		return ret;

	uclass_foreach_dev(dev, uc) {
		/* only find clk in rtk driver */
		if (strncmp(dev->driver->name, "rtk", 3) != 0)
			continue;

		priv = (struct clk_rtk_ctrl_priv *) dev_get_priv(dev);
		if (!priv)
			continue;

		for (int i = 0; i < priv->gate_count; i++) {
			if (!priv->gates[i])
				continue;

			if (strcmp(gate->parent, priv->gates[i]->name) == 0) {
				parent_clk.id = i;
				parent_clk.dev = dev;
				clk_enable(&parent_clk);
				return 0;
			}
		}
	}

	log_err("Invalid parent clk, name: %s\n", gate->parent);
	return -1;
}

static int clk_rtk_ctrl_enable(struct clk *clk)
{
	struct clk_rtk_ctrl_priv *priv = dev_get_priv(clk->dev);
	struct rtk_gate *gate = priv->gates[clk->id];
	uint32_t val;

	if (!gate) {
		log_err("Invalid clk, id: %lu\n", clk->id);
		return -EINVAL;
	}

	/* iterate all parent clk and enable them */
	clk_rtk_ctrl_enable_parent(clk);

	gate->enable_count++;
	if (clk_rtk_ctrl_is_enable(clk))
		return 0;

	val = readl(priv->reg_base + gate->ofs);
	val |= (1 << gate->bit_idx);
	if (gate->write_en)
		val |= (1 << (gate->bit_idx + 1));
	writel(val, priv->reg_base + gate->ofs);

	return 0;
}

const struct clk_ops clk_rtk_ctrl_ops = {
	.of_xlate = clk_rtk_ctrl_of_xlate,
	.enable = clk_rtk_ctrl_enable,
	.disable = clk_rtk_ctrl_disable,
};

