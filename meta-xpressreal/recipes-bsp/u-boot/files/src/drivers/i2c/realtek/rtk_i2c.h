/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __RTK_I2C_H
#define __RTK_I2C_H

#include <common.h>
#include <i2c.h>
#include <linux/clk-provider.h>

struct rtk_i2c_quirks {
	int high_speed;
};

struct rtk_i2c_priv {
	int	nr;
	uint32_t bus_freq_hz; /* i2c speed, unit: hz */
	struct clk clk;
	void __iomem *base;
	void __iomem *irqbase;


	const struct rtk_i2c_quirks *quirks;

	struct i2c_msg *msgs;
	unsigned int tx_buf_len;
	unsigned char *tx_buf;
	unsigned int rx_buf_len;
	unsigned char *rx_buf;
	int msgs_num;
	int msg_w_idx;
	int msg_r_idx;
	int msg_err;
	int rx_outstanding;
	int abort_source;
	int tx_fifo_depth;
	int rx_fifo_depth;
	int status;
};

int rtk_i2c_reset_deassert(struct rtk_i2c_priv *priv);
int rtk_i2c_reset_assert(struct rtk_i2c_priv *priv);
int rtk_i2c_set_pinmux(struct rtk_i2c_priv *priv);
int rtk_i2c_reset_pinmux(struct rtk_i2c_priv *priv);

#endif /* __RTK_I2C_H */