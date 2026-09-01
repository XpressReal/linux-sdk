// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_dprx.h"

static int rtk_dprx_audio_initial(struct rtk_dprx *dprx)
{
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_0, 0x00000036);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_1, 0x00000040);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_2, 0x00000079);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_3, 0x00000031);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_4, 0x00000086);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_5, 0x000000a3);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_6, 0x00000036);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_7, 0x00000040);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_SEC_N_F_CODE_8, 0x00000079);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_RS_DEC_CTRL, 0x00000080);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_FREQUENY_DET_0, 0x000000c3);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_FREQUENY_DET_1, 0x000000a9);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_BUFFER_CTRL_0, 0x00000008);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DVC_CTRL, 0x0000000f);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_FSM_CTRL_0, 0x000000d7);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_FSM_CTRL_2, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_PS_CTRL_0, 0x000000cc);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_BDRY_0, 0x000000b0);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_BDRY_2, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_BDRY_3, 0x000000ff);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_BDRY_5, 0x0000007f);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_BDRY_6, 0x000000ff);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_N_F_CODE_0, 0x00000080);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_N_F_CODE_1, 0x00000036);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_N_F_CODE_2, 0x00000040);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_N_F_CODE_3, 0x00000079);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_N_F_MIN, 0x00000011);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_AUD_CTRL, 0x000000c0);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_AUD_ID, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_CHANNEL_EN, 0x000000ff);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_I2S_CTRL, 0x0000000f);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_SPDIF_TX_0, 0x000000cf);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SEC_MISC, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_VSC0, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_VSC2, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_VSC3, 0x00000007);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_SPD_MAT_HB1, 0x00000083);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_HDR_INFO_MAT_HB1, 0x00000087);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_RSV0_MAT_HB1, 0x00000081);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_RSV1_MAT_HB1, 0x00000082);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_ISRC_MAT_HB1, 0x00000006);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_SPD_MAT_HB3, 0x00000044);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_HDR_INFO_MAT_HB3, 0x00000044);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_RSV0_MAT_HB3, 0x00000044);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_RSV1_MAT_HB3, 0x00000044);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_SPD_CTRL, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_HDR_INFO_CTRL, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_RSV0_CTRL, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_RSV1_CTRL, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_SPD_CHG_MNT, 0x000000fc);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_SDP_HDR_INFO_MNT, 0x000000a0);

	return 0;
}

static const struct rtk_dprx_audio_ops dprx_audio_ops = {
	.initial = rtk_dprx_audio_initial,
};

int rtk_dprx_audio_init(struct rtk_dprx *dprx)
{

	dprx->audio_ops = &dprx_audio_ops;

	return 0;
}
