/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __RTK_DSI_REG_H__
#define __RTK_DSI_REG_H__

#define DPHY_EXTERNAL_RESET (1 << 24)
#define APHY_RESET (1 << 25)
#define APHY_CORE_POWER_DOWN (1 << 26)

#define SYNC_PULSE_MODE 16
#define BURST_MODE 17
#define CRC_LEN 24
#define HEADER_LEN 20

#define REG_TX_LPX_TIME 24
#define REG_HS_EXIT_TIME 16
#define REG_TX_INIT_TIME 0

#define REG_CLK_PRE_TIME 24
#define REG_CLK_ZERO_TIME 16
#define REG_CLK_PRPR_TIME 8
#define REG_CLK_POST_TIME 0

#define REG_TX_VLD_TIME 24
#define REG_TX_TAIL_TIME 16
#define REG_HS_ZERO_TIME 8
#define REG_HS_PRPR_TIME 0

#define TX_DATA3_31_24_MASK GENMASK(31, 24)
#define TX_DATA3_23_16_MASK GENMASK(23, 16)
#define TX_DATA3_15_8_MASK GENMASK(15, 8)
#define TX_DATA3_7_0_MASK GENMASK(7, 0)

#define REG_CK_ESCAPE_DISABLE 12
#define CLOCL_GEN_11_8_MASK GENMASK(11, 8)
#define CLOCL_GEN_LANE_ENABLE_MASK GENMASK(7, 4)
#define CLOCL_GEN_3_0_MASK GENMASK(3, 0)

#define LANE_CLK_ENABLE BIT(8)
#define RX_RESET BIT(9)
#define TX_RESET BIT(10)
#define LANE0_ENABLE BIT(4)
#define LANE1_ENABLE BIT(5)
#define LANE2_ENABLE BIT(6)
#define LANE3_ENABLE BIT(7)

#define CMD_MODE 0
#define VIDEO_MODE BIT(0)

#define LANE_NUM_MASK GENMASK(5, 4)
#define LANE_NUM_1 0
#define LANE_NUM_2 BIT(4)
#define LANE_NUM_3 BIT(5)
#define LANE_NUM_4 (BIT(4) | BIT(5))

#define EOTP_EN BIT(12)
#define RX_ECC_EN BIT(22)
#define RX_CRC_EN BIT(21)

/**
 * Main Control
 */
#define CTRL_REG	0x000
#define INTE		0x010
#define INTS		0x014

/**
 * Timing Control Register
 */
#define TC0 0x100
#define TC1 0x104
#define	TC2 0x108
#define	TC3 0x10C
#define	TC4 0x110
#define	TC5 0x114

/**
 * In-Direct Memory Access
 */
#define IDMA0 0x200
#define IDMA1 0x204
#define IDMA2 0x208
#define IDMA3 0x20C

/**
 * Timer
 */
#define TO0 0x300
#define TO1 0x304
#define TO2 0x308
#define TO3 0x30C

/**
 * Command Mode Control Resister
 */
#define CMD_GO 0x400
#define CMD0   0x404

/**
 * Status and Error Report
 */
#define DPHY_STATUS0 0x500
#define DPHY_STATUS1 0x504
#define DPHY_ERR     0x508

/**
 * Pattern Generator
 */
#define PAT_GEN 0x610

/**
 * Dummy Register
 */
#define CLK_CONTINUE 0x708

/**
 * D-PHY Control
 */
#define CLOCK_GEN	0x800
#define TX_DATA0	0x808
#define TX_DATA1	0x80C
#define TX_DATA2	0x810
#define TX_DATA3	0x814
#define SSC0		0x840
#define SSC1		0x844
#define SSC2		0x848
#define SSC3		0x84C
#define WATCHDOG	0x850
#define TX_SWAP		0x868
#define RX_SWAP		0x86C
#define MPLL		0xC00
#define LF			0xC04
#define TXF			0xC08
#define DF			0xC0C

#endif /* __RTK_DSI_REG_H__ */
