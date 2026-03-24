/*
 *
 * Copyright (C) 2025 Realtek Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/bitops.h>
#include <asm/io.h>
#include "rtk_i2c.h"

#define I2C_MASK 0xF
#define I2C_EN 0x2
#define I2C6_EN 0x3

int rtk_i2c_reset_deassert(struct rtk_i2c_priv *priv)
{
	/* need rtk reset driver */
	unsigned int *rst;
	int ret = 0;

	switch (priv->nr)
	{
	case 0:
		rst = (unsigned int *)0x98007088;
		*rst |= BIT(11);
		break;
	case 1:
		rst = (unsigned int *)0x98007088;
		*rst |= BIT(12);
		break;
	case 3:
		rst = (unsigned int *)0x98007058;
		*rst |= BIT(0) | BIT(1);
		break;
	case 4:
		rst = (unsigned int *)0x98000068;
		*rst |= BIT(2) | BIT(3);
		break;
	case 5:
		rst = (unsigned int *)0x98000068;
		*rst |= BIT(4) | BIT(5);
		break;
	case 6:
		rst = (unsigned int *)0x98000880;
		*rst |= BIT(30) | BIT(31);
		break;
	case 7:
		rst = (unsigned int *)0x98000880;
		*rst |= BIT(28) | BIT(29);
		break;
	default:
		log_err("[%s] wrong i2c nr: %d\n", priv->nr);
		ret = -1;
		break;
	}

	return ret;
}

int rtk_i2c_reset_assert(struct rtk_i2c_priv *priv)
{
	/* need rtk reset driver */
	unsigned int *rst;
	int ret = 0;

	switch (priv->nr)
	{
	case 0:
		rst = (unsigned int *)0x98007088;
		*rst &= ~BIT(11);
		break;
	case 1:
		rst = (unsigned int *)0x98007088;
		*rst &= ~BIT(12);
		break;
	case 3:
		rst = (unsigned int *)0x98007058;
		*rst &= ~(BIT(0) | BIT(1));
		break;
	case 4:
		rst = (unsigned int *)0x98000068;
		*rst &= ~(BIT(2) | BIT(3));
		break;
	case 5:
		rst = (unsigned int *)0x98000068;
		*rst &= ~(BIT(4) | BIT(5));
		break;
	case 6:
		rst = (unsigned int *)0x98000880;
		*rst &= ~(BIT(30) | BIT(31));
		break;
	case 7:
		rst = (unsigned int *)0x98000880;
		*rst &= ~(BIT(28) | BIT(29));
		break;
	default:
		log_err("[%s] wrong i2c nr: %d\n", priv->nr);
		ret = -1;
		break;
	}

	return ret;
}

static int rtk_i2c_mux_to_i2c(struct rtk_i2c_priv *priv,
						 uint32_t *addr, uint32_t offset)
{
	uint32_t tmp;
	uint32_t mask = I2C_MASK << offset;
	uint32_t en = (priv->nr == 6) ? (I2C6_EN << offset) : (I2C_EN << offset);

    tmp = readl(addr);
    tmp = (tmp & ~mask) | (en & mask);
    writel(tmp, addr);

	return 0;
}

int rtk_i2c_set_pinmux(struct rtk_i2c_priv *priv)
{
	/* need rtk pinmux driver */
	int ret = 0;

	switch (priv->nr)
	{
	case 0:
		rtk_i2c_mux_to_i2c(priv, 0x9814e000, 25);
		rtk_i2c_mux_to_i2c(priv, 0x9814e004, 0);
		break;
	case 1:
		rtk_i2c_mux_to_i2c(priv, 0x9804f204, 24);
		rtk_i2c_mux_to_i2c(priv, 0x9804f204, 28);
		break;
	case 3:
		rtk_i2c_mux_to_i2c(priv, 0x9814e004, 24);
		rtk_i2c_mux_to_i2c(priv, 0x9814e004, 28);
		break;
	case 4:
		rtk_i2c_mux_to_i2c(priv, 0x9814e010, 0);
		rtk_i2c_mux_to_i2c(priv, 0x9814e010, 4);
		break;
	case 5:
		rtk_i2c_mux_to_i2c(priv, 0x9814e018, 16);
		rtk_i2c_mux_to_i2c(priv, 0x9814e018, 20);
		break;
	case 6:
		rtk_i2c_mux_to_i2c(priv, 0x9814e014, 16);
		rtk_i2c_mux_to_i2c(priv, 0x9814e014, 20);
		break;
	case 7:
		rtk_i2c_mux_to_i2c(priv, 0x9814e010, 24);
		rtk_i2c_mux_to_i2c(priv, 0x9814e010, 20);
		break;
	default:
		log_err("[%s] wrong i2c nr: %d\n", priv->nr);
		ret = -1;
		break;
	}

	return ret;
}

static int rtk_i2c_clear_mux(uint32_t *addr, uint32_t offset)
{
	uint32_t tmp;
	uint32_t mask = I2C_MASK << offset;

    tmp = readl(addr);
    tmp = (tmp & ~mask);
    writel(tmp, addr);

	return 0;
}

int rtk_i2c_reset_pinmux(struct rtk_i2c_priv *priv)
{
	/* need rtk pinmux driver */
	int ret = 0;

	switch (priv->nr)
	{
	case 0:
		rtk_i2c_clear_mux(0x9814e000, 25);
		rtk_i2c_clear_mux(0x9814e004, 0);
		break;
	case 1:
		rtk_i2c_clear_mux(0x9804f204, 24);
		rtk_i2c_clear_mux(0x9804f204, 28);
		break;
	case 3:
		rtk_i2c_clear_mux(0x9814e004, 24);
		rtk_i2c_clear_mux(0x9814e004, 28);
		break;
	case 4:
		rtk_i2c_clear_mux(0x9814e010, 0);
		rtk_i2c_clear_mux(0x9814e010, 4);
		break;
	case 5:
		rtk_i2c_clear_mux(0x9814e018, 16);
		rtk_i2c_clear_mux(0x9814e018, 20);
		break;
	case 6:
		rtk_i2c_clear_mux(0x9814e014, 16);
		rtk_i2c_clear_mux(0x9814e014, 20);
		break;
	case 7:
		rtk_i2c_clear_mux(0x9814e010, 24);
		rtk_i2c_clear_mux(0x9814e010, 20);
		break;
	default:
		log_err("[%s] wrong i2c nr: %d\n", priv->nr);
		ret = -1;
		break;
	}
	return 0;
}