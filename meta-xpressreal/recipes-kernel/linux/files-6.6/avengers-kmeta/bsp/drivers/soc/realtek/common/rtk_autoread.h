/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Realtek Autoread driver
 *
 * Copyright (C) 2025 Realtek Semiconductor Corporation.
 */

#ifndef __RTK_AUTOREAD_H_
#define __RTK_AUTOREAD_H_

#define I2C_7_INT_MASK		0x400
#define INA_START_BASE		0x00
#define DMA_LSB		0x80
#define DMA_MSB		0x84
#define DMA_LOOPCNT		0x88
#define DMA_WAIT		0x8C
#define DMA_CTRL		0x90
#define DMA_ST			0xA0

#define DMA_ST_FALL1		0x80000000
#define DMA_ST_FALL0		0x40000000
#define I2C_IP_INT_MASK	0x01000000
#define AUTOREAD_INT_MASK	0xDE000000

#endif /*__RTK_AUTOREAD_H_ */
