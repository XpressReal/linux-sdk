/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _RTK_XDI_REG_RTD1635_H_
#define _RTK_XDI_REG_RTD1635_H_

#define XDI_REG_MODE                      0x000
#define XDI_REG_FC                        0x004
#define XDI_REG_INTEN                     0x008
#define XDI_REG_INTST                     0x00c
#define XDI_REG_CORE                      0x010
#define XDI_REG_CORE_DI                   0x014
#define XDI_REG_CORE_SIZE                 0x018
#define XDI_REG_CORE_DMA                  0x01c
#define XDI_REG_CORE_SEQ_SA_C_Y           0x020
#define XDI_REG_CORE_SEQ_SA_C_C           0x024
#define XDI_REG_CORE_SEQ_SA_N_Y           0x028
#define XDI_REG_CORE_SEQ_SA_N_C           0x02c
#define XDI_REG_CORE_SEQ_SA_P_Y           0x030
#define XDI_REG_CORE_SEQ_SA_P_C           0x034
#define XDI_REG_CORE_SEQ_PITCH_C          0x038
#define XDI_REG_CORE_SEQ_PITCH_N          0x03c
#define XDI_REG_CORE_SEQ_PITCH_P          0x040
#define XDI_REG_CORE_MA_FR                0x044
#define XDI_REG_CORE_MA_FW                0x048
#define XDI_REG_CORE_MA_FR2               0x04c
#define XDI_REG_CORE_RANGE_CTRL           0x050
#define XDI_REG_CORE_RANGE_FR             0x054
#define XDI_REG_CORE_RANGE_Y_TH           0x058
#define XDI_REG_CORE_RANGE_Y_OV           0x05c
#define XDI_REG_CORE_RANGE_Y_DET          0x060
#define XDI_REG_CORE_RANGE_C_TH           0x064
#define XDI_REG_CORE_RANGE_C_OV           0x068
#define XDI_REG_CORE_RANGE_C_DET          0x06c
#define XDI_REG_CORE_INDEX_RR_C           0x070
#define XDI_REG_CORE_INDEX_RR_P           0x074
#define XDI_REG_CORE_INDEX_RR_N           0x078
#define XDI_REG_CORE_DI_LPF1X5            0x07c
#define XDI_REG_CORE_DI_LPF1X3            0x080
#define XDI_REG_CORE_DI_CHK1              0x084
#define XDI_REG_CORE_DI_CHK2              0x088
#define XDI_REG_CORE_DI_MF                0x08c
#define XDI_REG_CORE_DI_LIGHT_COMB0       0x090
#define XDI_REG_CORE_DI_LIGHT_COMB1       0x094
#define XDI_REG_CORE_DI_BOB_INPUT_LPF     0x098
#define XDI_REG_CORE_DI_BOB_OVERWIDE      0x09c
#define XDI_REG_CORE_DI_BOB_DIFF          0x0a0
#define XDI_REG_CORE_DI_BOB               0x0a4
#define XDI_REG_CORE_DI_NC_COMB           0x0a8
#define XDI_REG_CORE_DI_FINAL_COMB        0x0ac
#define XDI_REG_CORE_STA_WADTB            0x0b0
#define XDI_REG_CORE_STA_YWIN             0x0d4
#define XDI_REG_CORE_STA_CWIN             0x0d8
#define XDI_REG_CORE_STA_WINX             0x0dc
#define XDI_REG_CORE_STA_FILM_SWAD        0x0e0
#define XDI_REG_CORE_STA_FILM_TH          0x0f8
#define XDI_REG_CORE_STA_TOTAL            0x110
#define XDI_REG_CORE_STA_SUM              0x114
#define XDI_REG_CORE_STA_BKT              0x118
#define XDI_REG_CORE_STA_AD               0x140
#define XDI_REG_CORE_STA_NC_COMB          0x168
#define XDI_REG_CORE_STA_FINAL_COMB       0x16c
#define XDI_REG_CORE_DI_HMC_0             0x170
#define XDI_REG_CORE_DI_HMC_1             0x174
#define XDI_REG_CORE_DI_VMC_0             0x178
#define XDI_REG_CORE_DI_TICKER_0          0x17c
#define XDI_REG_CORE_DI_TICKER_1          0x180
#define XDI_REG_CORE_DI_TICKER_2          0x184
#define XDI_REG_CORE_DI_TICKER_3          0x188
#define XDI_REG_CORE_DI_GLOBAL            0x18c
#define XDI_REG_CORE_STA_NEXT             0x190
#define XDI_REG_CORE_STA_LPF              0x194
#define XDI_REG_CORE_STA_GLO_COMB         0x198
#define XDI_REG_CORE_DI_DYNAMIC_0         0x19c
#define XDI_REG_CORE_DI_DYNAMIC_1         0x1a0
#define XDI_REG_CORE_DI_SMOOTH            0x1a4
#define XDI_REG_CORE_DI_STILL             0x1a8
#define XDI_REG_CORE_DI_VOTE              0x1ac
#define XDI_REG_CORE_DI_TEETH             0x1b0
#define XDI_REG_CORE_DI_WGTFILT_THD       0x1b4
#define XDI_REG_CORE_DI_WGTFILT_WGT       0x1b8
#define XDI_REG_CORE_DI_WGTFILT_WIN0      0x1bc
#define XDI_REG_CORE_DI_WGTFILT_WIN1      0x1c0
#define XDI_REG_CORE_DI_WGTFILT_WIN2      0x1c4
#define XDI_REG_CORE_DI_NOISE             0x1c8
#define XDI_REG_CORE_DI_NOISE_WIN_H       0x1cc
#define XDI_REG_CORE_DI_NOISE_WIN_W       0x1d0
#define XDI_REG_CORE_DI_NOISE_LEVEL_SUM   0x1d4
#define XDI_REG_CORE_DI_NOISE_LEVEL       0x1d8
#define XDI_REG_CORE_DI_C_TYPE            0x1dc
#define XDI_REG_CORE_DI_C_WIN0_H          0x1e0
#define XDI_REG_CORE_DI_C_WIN0_W          0x1e4
#define XDI_REG_CORE_DI_C_WIN1_H          0x1e8
#define XDI_REG_CORE_DI_C_WIN1_W          0x1ec
#define XDI_REG_CORE_DI_C_WIN2_H          0x1f0
#define XDI_REG_CORE_DI_C_WIN2_W          0x1f4
#define XDI_REG_DMA                       0x1f8
#define XDI_REG_DMA_PRT_MODE              0x1fc
#define XDI_REG_DMA_PV                    0x200
#define XDI_REG_DMA_PW                    0x204
#define XDI_REG_DMA_DCWBUF                0x208
#define XDI_REG_DMA_UPDATE                0x20c
#define XDI_REG_DMA_VI_Y0                 0x210
#define XDI_REG_DMA_VI_Y1                 0x214
#define XDI_REG_DMA_VI_Y2                 0x218
#define XDI_REG_DMA_VI_C0                 0x21c
#define XDI_REG_DMA_VI_C1                 0x220
#define XDI_REG_DMA_VI_C2                 0x224
#define XDI_REG_DMA_FM                    0x228
#define XDI_REG_DMA_FM2                   0x22c
#define XDI_REG_DMA_WB_Y                  0x230
#define XDI_REG_DMA_WB_C                  0x234
#define XDI_REG_DMA_WB_FW                 0x238
#define XDI_REG_DMA_METER0                0x23c
#define XDI_REG_DMA_METER1                0x240
#define XDI_REG_DMA_METER2                0x244
#define XDI_REG_DMA_RST                   0x248
#define XDI_REG_DMA_SGRP                  0x24c
#define XDI_REG_WB                        0x250
#define XDI_REG_WB_SEQ_SA_Y               0x254
#define XDI_REG_WB_SEQ_SA_C               0x258
#define XDI_REG_WB_SEQ_PITCH              0x25c

/* XDI_REG_CORE REG_FIELD */
#define XDI_REG_FIELD_TOPFIELD			BIT(1)
#define XDI_REG_FIELD_F422			BIT(2)
#define XDI_REG_FIELD_PPC10B			BIT(3)
#define XDI_REG_FIELD_NV21			BIT(4)
#define XDI_REG_FIELD_ST			BIT(5)
/* XDI_REG_CORE_DI REG_FIELD */
#define XDI_REG_FIELD_WEAVE_TEETH_EN		BIT(0)
#define XDI_REG_FIELD_NOISE_LEVEL_EN		BIT(1)
#define XDI_REG_FIELD_CU			BIT(2)
#define XDI_REG_FIELD_LIGHT_COMB_EN		BIT(3)
#define XDI_REG_FIELD_WEIGHT_FILTER_EN		BIT(4)
#define XDI_REG_FIELD_MF_SELECT			BIT(6)
#define XDI_REG_FIELD_COMB_CHK_EN		BIT(7)
#define XDI_REG_FIELD_CHK_USE_MAX_MF		BIT(8)
#define XDI_REG_FIELD_WB_CHK_RESULT		BIT(9)
#define XDI_REG_FIELD_WB_MAX_DIFF		BIT(10)
#define XDI_REG_FIELD_USE_WB_DIFF		BIT(11)
#define XDI_REG_FIELD_SOURCE			GENMASK(13, 12)
#define XDI_REG_FIELD_BOB_CHK0_EN		BIT(16)
#define XDI_REG_FIELD_BOB_CHK1_EN		BIT(17)
#define XDI_REG_FIELD_BOB_CHK2_EN		BIT(18)
#define XDI_REG_FIELD_CHK4_EN			BIT(19)
#define XDI_REG_FIELD_SMOOTH_EN			BIT(20)
#define XDI_REG_FIELD_STILL_EN			BIT(21)
#define XDI_REG_FIELD_STILL_RESET		BIT(22)
#define XDI_REG_FIELD_VOTE_EN			BIT(23)
#define XDI_REG_FIELD_DBG_BLEND_RATIO		BIT(24)
#define XDI_REG_FIELD_DBG_COMBING		BIT(25)
#define XDI_REG_FIELD_DBG_BOB			BIT(27)
#define XDI_REG_FIELD_HCS_420_SEL_PN		BIT(29)
#define XDI_REG_FIELD_MODE			GENMASK(31, 30)
/* XDI_REG_CORE_SIZE REG_FIELD */
#define XDI_REG_FIELD_H				GENMASK(11, 0)
#define XDI_REG_FIELD_W				GENMASK(24, 12)
/* XDI_REG_CORE_SEQ_PITCH_* REG_FIELD */
#define XDI_REG_FIELD_C_PTICH			GENMASK(31, 16)
#define XDI_REG_FIELD_Y_PTICH			GENMASK(15, 0)
/* XDI_REG_WB REG_FIELD */
#define XDI_REG_FIELD_WB_F420			BIT(0)
#define XDI_REG_FIELD_WB_TPC_NUM		GENMASK(8, 1)
#define XDI_REG_FIELD_WB_PPC10B			BIT(9)
#define XDI_REG_FIELD_WB_P010			BIT(10)
#define XDI_REG_FIELD_WB_TR			BIT(11)

#endif /* _RTK_XDI_REG_RTD1635_H_ */
