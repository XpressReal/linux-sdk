/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _RTK_DSI_H
#define _RTK_DSI_H

#define XTAL_FREQ 27
#define MHZ(v) ((unsigned int)((v) * 1000000U))
#define TOTAL_LANE_NUM 4

#define FIXED_SHIFT 10

#define LANE0_SEL 0
#define LANE1_SEL 1
#define LANE2_SEL 2
#define LANE3_SEL 3
#define LANE_CLK_SEL 4

#define LANE0_PN_SWAP 0
#define LANE1_PN_SWAP 0
#define LANE2_PN_SWAP 0
#define LANE3_PN_SWAP 0
#define LANE_CLK_PN_SWAP 0

enum dsi_fmt {
	DSI_FMT_720P_60 = 0,
	DSI_FMT_1080P_60,
	DSI_FMT_1200_1920P_60,
	DSI_FMT_800_1280P_60,
	DSI_FMT_600_1024P_60,
	DSI_FMT_1920_720P_60,
	DSI_FMT_1920_720P_30,
	DSI_FMT_600_1024P_30,
	DSI_FMT_800_480P_60,
};

enum dsi_pat_gen {
	DSI_PAT_GEN_COLORBAR    = 0,
	DSI_PAT_GEN_BLACK       = 1,
	DSI_PAT_GEN_WHITE       = 2,
	DSI_PAT_GEN_RED         = 3,
	DSI_PAT_GEN_BLUE        = 4,
	DSI_PAT_GEN_YELLOW      = 5,
	DSI_PAT_GEN_MAGENTA     = 6,
	DSI_PAT_GEN_USER_DEFINE = 7,
	DSI_PAT_GEN_MAX,
};

#endif /* _RTK_DSI_H */
