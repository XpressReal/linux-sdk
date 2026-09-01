// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_dprx.h"

// TODO: FIXME
#define ScalerDpPhyRxGetPCBLanePNSwap(...)   (0)
#define ScalerDpPhyRxGetPCBLaneMapping(...)  (0)
#define ScalerDpPhyRxSetLanePNSwap(...)      {}
#define ScalerDpPhyRxSetLaneMux(...)         {}
#define ScalerDpPhyRxGetPhyCTSFlag(...)      (false)
#define ScalerDpPhyRxPhyCTS(...)             {}
#define ScalerDpHdcpRxGetHdcpMode(...)       (0)
#define ScalerDpMacRx0SetHdcpMode(...)       {}
#define ScalerDpMacRx0HdcpMeasureCheck(...)  (false)
#define ScalerDpAuxRxHpdIrqAssert(...)       {}
#define ScalerAudioDpAudioEnable(...)        {}
#define GET_DP_MAC_RX0_H_PORCH_ENLARGE(...)  (0)
#define ABSDWORD(...)     (0)
#define _HW_DP_RX_VIDEO_FIFO_SIZE  24576

#define GET_COLOR_DEPTH           8
#define _HW_DATA_PATH_SPEED_LIMIT 6000
#define _DP_MEASURE_POLLING_TIMEOUT  30
#define _HDCP_14   1
#define _HW_DP_D0_MAX_LINK_RATE_SUPPORT    _DP_LINK_HBR2
#define _VGIP_IH_CAPTURE_MIN_VALUE  0
#define _VGIP_IV_CAPTURE_MIN_VALUE  0
#define _EDID_ANALYSIS_HTOTAL_MARGIN 1
#define _8_BIT     (0xFF)
#define _PANEL_DP_FREESYNC_MIN_FRAME_RATE 1
#define _PANEL_DP_FREESYNC_MAX_FRAME_RATE 1

static u32 rtk_dprx_compute_mul_div(struct rtk_dprx *dprx, u32 a, u32 b, u32 c);
static void rtk_dprx_clk_select(struct rtk_dprx *dprx,
	enum RTK_DP_MAC_CLK_SELECT clk_sel);
static void rtk_dprx_set_sdp_buff_rcv_initial(struct rtk_dprx *dprx);
static void rtk_dprx_set_sdp_buff_rcv_mode(struct rtk_dprx *dprx,
		enum RTK_DP_SDP_BUFF index, enum RTK_DP_SDP_TYPE type);
static void rtk_dprx_set_meta_sdp_rcv_inital(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE type);
static void rtk_dprx_set_vsc_sdp_rcv_initial(struct rtk_dprx *dprx);
static int rtk_dprx_vsc_check(struct rtk_dprx *dprx);
static int rtk_dprx_get_amd_spd_info(struct rtk_dprx *dprx,
		enum RTK_DP_SPD_INFO spd_info);
static int rtk_dprx_get_video_info(struct rtk_dprx *dprx);
static void rtk_dprx_set_color_info(struct rtk_dprx *dprx,
		struct rtk_timing_info *timing_info);
static int rtk_dprx_video_setting(struct rtk_dprx *dprx,
	enum RTK_DP_COLOR_SPACE color_space, u8 pre_color_depth);
static int rtk_dprx_colorimetry_setting(struct rtk_dprx *dprx);
static int rtk_dprx_colorimetry_ext_setting(struct rtk_dprx *dprx);
static int rtk_dprx_quantization_setting(struct rtk_dprx *dprx);
static int rtk_dprx_pll_input_clock_setting(struct rtk_dprx *dprx,
	enum RTK_DP_LINK_RATE link_rate, u32 *input_clk_hz, u32 link_clk_hz);
static int rtk_dprx_set_dprxpll_freq_nf(struct rtk_dprx *dprx,
	u32 target_clk_hz, u32 input_clk_hz);
static void rtk_dprx_set_pll_p_gain_clamp(struct rtk_dprx *dprx,
	u8 gain_b3, u8 gain_b2, u8 gain_b1);
static void rtk_dprx_pll_p_code_spread_ctrl(struct rtk_dprx *dprx,
	bool en_spread);
static void rtk_dprx_hs_active_tracking_mode(struct rtk_dprx *dprx,
	enum RTK_DP_HS_TRACKING_TYPE mode);
static void rtk_dprx_reset(struct rtk_dprx *dprx);
static int rtk_dprx_valid_signal_detection(struct rtk_dprx *dprx);
static u8 rtk_dprx_link_rate_detect(struct rtk_dprx *dprx);
static bool rtk_dprx_link_rate_check(struct rtk_dprx *dprx, u8 link_rate);
static u32 rtk_dprx_signal_detect_measure_count(struct rtk_dprx *dprx,
		u8 lane_sel, enum RTK_MEASURE_TARGET target, enum RTK_MEASURE_PERIOD period);
static bool rtk_dprx_measure_info_check(struct rtk_dprx *dprx,
	struct rtk_dprx_stream_info *stream_info);
static bool rtk_dprx_signal_check(struct rtk_dprx *dprx, u8 link_rate, u8 dpcd_lane);
static bool rtk_dprx_vbios_msa_check(struct rtk_dprx *dprx, bool de_skew_enhanced);
static bool rtk_dprx_change_sramble_seed(struct rtk_dprx *dprx);
static int rtk_dprx_scan_input_port(struct rtk_dprx *dprx);
static int rtk_dprx_fifo_check_proc(struct rtk_dprx *dprx, enum RTK_DP_FIFO_CHECK_CONDITION check_condition,
		struct rtk_dprx_stream_info *stream_info);
static void rtk_dprx_mac_setting(struct rtk_dprx *dprx);
static void rtk_dprx_sdp_setting(struct rtk_dprx *dprx);
static bool rtk_dprx_get_vbid_info(struct rtk_dprx *dprx,
		enum RTK_DP_VBID_INFO vbid_info);
static void rtk_dprx_set_sdp_reset(struct rtk_dprx *dprx);
static void rtk_dprx_set_sdp_init_status(struct rtk_dprx *dprx,
	enum RTK_DP_SDP_BUFF index);
static void rtk_dprx_sdp_data_detect(struct rtk_dprx *dprx);
static void rtk_dprx_sdp_packet_check(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type);
static void rtk_dprx_sdp_rev_detect(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type);
static int rtk_dprx_sdp_chg_detect(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type);
static bool rtk_dprx_get_sdp_received(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type);
static bool rtk_dprx_get_sdp_changed(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type);
static int rtk_dprx_get_sdp_pkt_data(struct rtk_dprx *dprx,
	enum RTK_DP_SDP_TYPE sdp_type, u8 *sdp_data, u8 offset, u8 length);
static u8 rtk_dprx_get_sdp_info_hb3(struct rtk_dprx *dprx,
		enum RTK_DP_SDP_TYPE sdp_type);
static void rtk_dprx_set_no_video_stream_irq(struct rtk_dprx *dprx, bool en);
static bool rtk_dprx_cdr_check(struct rtk_dprx *dprx, u8 link_rate, u8 dpcd_lane);
static bool rtk_dprx_get_measure_link_info(struct rtk_dprx *dprx,
		struct rtk_dprx_stream_info *stream_info);
static bool rtk_dprx_display_format_setting(struct rtk_dprx *dprx,
		struct rtk_dprx_stream_info *stream_info);
static int rtk_dprx_fifo_check(struct rtk_dprx *dprx,
	enum RTK_DP_FIFO_CHECK_CONDITION condition);
static bool rtk_dprx_measure_lane_cdr_clk(struct rtk_dprx *dprx,
	u8 Link_Rate, u8 lane_number);
static void rtk_dprx_set_fifo_irq(struct rtk_dprx *dprx, bool en);
static void rtk_dprx_set_fifo_wd(struct rtk_dprx *dprx, bool en);
static void rtk_dprx_interlace_reset(struct rtk_dprx *dprx);
static void rtk_dprx_set_measure_pop_up(struct rtk_dprx *dprx);
static enum RTK_DP_SDP_BUFF rtk_dprx_get_sdp_buffer_type(struct rtk_dprx *dprx,
	enum RTK_DP_SDP_TYPE sdp_type);
static void rtk_dprx_set_porch_color_rgb(struct rtk_dprx *dprx);
static void rtk_dprx_set_porch_color_ycc_limit(struct rtk_dprx *dprx,
	enum RTK_DP_COLOR_SPACE color_space);
static void rtk_dprx_set_porch_color_ycc_full(struct rtk_dprx *dprx,
	enum RTK_DP_COLOR_SPACE color_space);
static int rtk_dprx_wait_flag(struct rtk_dprx *dprx,
	u32 timeout_ms, u32 reg, u32 bit_mask);

#if 1 // TODO: Only for test, should removed after finish driver implementation
/* Link rate specific blanking parameters */
struct dprx_mac_linkrate_params {
	u32 bs2hs_0;
	u32 bs2hs_1;
	u32 evblk2vs_m;
	u32 evblk2vs_l;
	u32 ovblk2vs_m;
	u32 ovblk2vs_l;
};

/* Blanking parameters for each link rate */
static const struct dprx_mac_linkrate_params params_1p62g = {
	.bs2hs_0    = 0x00000001,
	.bs2hs_1    = 0x00000047,
	.evblk2vs_m = 0x00000018,
	.evblk2vs_l = 0x000000b7,
	.ovblk2vs_m = 0x00000014,
	.ovblk2vs_l = 0x00000007,
};

static const struct dprx_mac_linkrate_params params_2p7g = {
	.bs2hs_0    = 0x00000002,
	.bs2hs_1    = 0x00000021,
	.evblk2vs_m = 0x00000029,
	.evblk2vs_l = 0x00000031,
	.ovblk2vs_m = 0x00000021,
	.ovblk2vs_l = 0x00000061,
};

static const struct dprx_mac_linkrate_params params_5p4g = {
	.bs2hs_0    = 0x00000004,
	.bs2hs_1    = 0x00000042,
	.evblk2vs_m = 0x00000052,
	.evblk2vs_l = 0x00000062,
	.ovblk2vs_m = 0x00000042,
	.ovblk2vs_l = 0x000000c2,
};

static void __maybe_unused prince_1080p_rgb(struct rtk_dprx *dprx,
	const struct dprx_mac_linkrate_params *params)
{
	int i;

	dev_info(dprx->dev, "%s\n", __func__);

	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_PHY_DIG_RESET_CTRL, 0x0000003e);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_PHY_DIG_RESET_CTRL, 0x0000003c);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_PHY_DIG_RESET_CTRL, 0x0000003c);

	/* Lane and control settings */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_LANE_MUX, 0x0000004E);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_STHD_CTRL_1, 0x00000081);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_PG_CTRL_0, 0x00000001);

	/* Blanking parameters (link rate specific) */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_BS2HS_0, params->bs2hs_0);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_BS2HS_1, params->bs2hs_1);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_EVBLK2VS_H, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_EVBLK2VS_M, params->evblk2vs_m);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_EVBLK2VS_L, params->evblk2vs_l);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_OVBLK2VS_H, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_OVBLK2VS_M, params->ovblk2vs_m);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_OVBLK2VS_L, params->ovblk2vs_l);

	/* Video timing settings */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_VS_FRONT_PORCH, 0x00000004);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_HTT_M, 0x00000008);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_HTT_L, 0x00000098);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_HST_M, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_HST_L, 0x000000c0);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_HWD_M, 0x00000007);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_HWD_L, 0x00000080);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_HSW_M, 0x00000080);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_HSW_L, 0x0000002c);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_VTT_M, 0x00000004);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_VTT_L, 0x00000065);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_VST_M, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_VST_L, 0x00000029);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_VHT_M, 0x00000004);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_VHT_L, 0x00000038);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_VSW_M, 0x00000080);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_DPF_VSW_L, 0x00000005);

	usleep_range(5000, 5100);
    dev_info(dprx->dev, "MAC_IP_DESKEW_PHY Read Reg [0x%08x]=%08x\n",
		0x98166038, dprx->rbus_ops->get_byte(DPRX14_MAC_IP_DESKEW_PHY));

	/* HDCP settings */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_HDCP_TYPE_AES_0, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_HDCP_TYPE_AES_1, 0x00000001);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_HDCP_ST_TYPE, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_HDCP_IRQ, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_HDCP_INTGT_VRF, 0x000000b4);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_HDCP_DEBUG, 0x00000011);

	/* Audio settings */
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
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUD_FREQUENY_DET_0, 0x00000083);
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

	/* SDP settings */
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

	/* PLL and tracking settings */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_PLL_OUT_CONTROL, 0x00000090);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_N_F_CODE_1, 0x00000028);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_N_F_CODE_2, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_N_F_CODE_3, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_N_F_CODE_4, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_N_F_MIN, 0x00000011);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_TRACKING_7, 0x00000011);

	/* PLL sequence */
	dprx->rbus_ops->write(DPRX14_PLL_VID_1, 0x95050014);
	dprx->rbus_ops->write(DPRX14_PLL_VID_1, 0x95050016);
	dprx->rbus_ops->write(DPRX14_PLL_VID_1, 0x94050016);
	dprx->rbus_ops->write(DPRX14_PLL_AUD_1, 0x93050014);
	dprx->rbus_ops->write(DPRX14_PLL_AUD_1, 0x93050016);
	dprx->rbus_ops->write(DPRX14_PLL_AUD_1, 0x92050016);

	/* M2PLL and tracking settings */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_M2PLL_CONTROL, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUDIO_PLL_CONTROL, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_TRACKING_5, 0x000000a0);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_PI_CODE_0, 0x000000f0);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_VS_TRACK_MODE, 0x00000088);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_PI_CODE_1, 0x00000000);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_PI_CODE_2, 0x0000007f);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_PI_CODE_3, 0x000000ff);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_PI_CODE_4, 0x000000ff);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_VS_TRACK_EN, 0x00000080);

	msleep(40);

    dev_info(dprx->dev, "MAC_IP_VS_TRACK3 Read Reg [0x%08x]=0x%08x\n",
		0x98166794, dprx->rbus_ops->get_byte(DPRX14_MAC_IP_VS_TRACK3));

	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_VS_TRACK3, 0x000000ff);

	/* Output control settings */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DPF_CTRL_0, 0x000000a0);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_OUTPUT_CTRL, 0x00000027);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_INTERLACE_MODE_CONFIG, 0x00000061);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_MEAS_CTRL, 0x00000080);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_DP_CRC_CTRL, 0x00000080);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_MN_STRM_ATTR_CTRL, 0x000000fa);

	for (i = 0; i < 5; i++) {
		u8 crc0, crc1, crc2, crc3, crc4, crc5;

		crc0 = dprx->rbus_ops->get_byte(PB5_72_DP_CRC_R_L);
		crc1 = dprx->rbus_ops->get_byte(PB5_71_DP_CRC_R_M);
		crc2 = dprx->rbus_ops->get_byte(PB5_74_DP_CRC_G_L);
		crc3 = dprx->rbus_ops->get_byte(PB5_73_DP_CRC_G_M);
		crc4 = dprx->rbus_ops->get_byte(PB5_76_DP_CRC_B_L);
		crc5 = dprx->rbus_ops->get_byte(PB5_75_DP_CRC_B_M);

		dev_info(dprx->dev, "crc 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			crc0, crc1, crc2, crc3, crc4, crc5);
		msleep(50);
	}
}
#endif

/**
 * rtk_dprx_compute_mul_div - Compute (a * b) / c with overflow protection
 * @a: multiplicand
 * @b: multiplier
 * @c: divisor
 *
 * Uses 64-bit intermediate calculation to avoid overflow
 *
 * Return: Calculation result, returns 0 if c=0, returns U32_MAX if overflow
 */
static u32 rtk_dprx_compute_mul_div(struct rtk_dprx *dprx, u32 a, u32 b, u32 c)
{
	u64 result;

	/* Handle division by zero */
	if (unlikely(c == 0)) {
		dev_err(dprx->dev, "%s: Division by zero\n", __func__);
		return 0;
	}

	/* Use 64-bit arithmetic to avoid overflow */
	result = mul_u32_u32(a, b);

	/* Perform division */
	result = div_u64(result, c);

	/* Check if result exceeds u32 range */
	if (unlikely(result > U32_MAX)) {
		dev_err(dprx->dev, "%s: Result overflow, clamping to U32_MAX\n", __func__);
		return U32_MAX;
	}

	return (u32)result;
}

/**
 * rtk_dprx_margin_link_check - DP Margin Link Check
 */
static bool rtk_dprx_margin_link_check(void)
{
	/* Not support MARGIN_LINK */

	return true;
}

/**
 * rtk_dprx_scramble_setting - DP Scramble Setting
 */
static void rtk_dprx_scramble_setting(struct rtk_dprx *dprx)
{
	u8 back_up = dprx->aux_ops->get_manual_mode_status(dprx);
	bool enhance_change = false;
	bool scramble_change = false;

	dprx->aux_ops->set_manual_mode(dprx);

	if ((dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x01, 0x01, _BIT7) == _BIT7) !=
		(dprx->rbus_ops->get_bit(PB_01_PHY_DIG_RESET_CTRL, _BIT2) == _BIT2)) {

		enhance_change = true;
		if (dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x01, 0x01, _BIT7) == _BIT7) {
			/* Enable Enhancement Control Mode --> MAC */
			dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT2, _BIT2);
			dev_info(dprx->dev, "enhance_change, enable enhancement\n");
		} else {
			/* Disable Enhancement Control Mode --> MAC */
			dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT2, 0x00);
			dev_info(dprx->dev, "enhance_change, disable enhancement\n");
		}
	}

	if ((dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x01, 0x02, _BIT5) == _BIT5) !=
		(dprx->rbus_ops->get_bit(PB_07_SCRAMBLE_CTRL, _BIT5) == _BIT5)) {

		scramble_change = true;
		if (dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x01, 0x02, _BIT5) == _BIT5) {
			/* Disable Scrambling */
			dprx->rbus_ops->set_bit(PB_07_SCRAMBLE_CTRL, ~_BIT5, _BIT5);
			dev_info(dprx->dev, "scramble_change, disable scrambling\n");
		} else {
			/* Enable Scrambling */
			dprx->rbus_ops->set_bit(PB_07_SCRAMBLE_CTRL, ~_BIT5, 0x00);
			dev_info(dprx->dev, "scramble_change, enable scrambling\n");
		}
	}

	if ((enhance_change == true) || (scramble_change == true)) {
		dprx->aux_ops->set_auto_mode(dprx);

		/* Delay 2ms for Scramble */
		usleep_range(2000, 2100);

		dprx->aux_ops->set_manual_mode(dprx);
	}

	if ((back_up & _BIT1) == _BIT1)
		dprx->aux_ops->set_auto_mode(dprx);

}

/**
 * rtk_dprx_decode_error_count_reset - Reset 8b10b Error Count value
 */
static void rtk_dprx_decode_error_count_reset(struct rtk_dprx *dprx,
	enum RTK_DP_DECODE_METHOD method)
{
	/* Reset 8b10b Error Count Value */
	dprx->rbus_ops->set_bit(PB_08_BIST_PATTERN_SEL,
		~(_BIT2 | _BIT1 | _BIT0), 0x00);

	switch (method) {
	case _DP_MAC_DECODE_METHOD_PRBS7:
		/* Reverse PRBS7 Pattern Gen */
		dprx->rbus_ops->set_bit(PB_08_BIST_PATTERN_SEL, ~_BIT7, _BIT7);

		/* Start Record PRBS7 Error Count Value */
		dprx->rbus_ops->set_bit(PB_08_BIST_PATTERN_SEL,
			~(_BIT2 | _BIT1 | _BIT0), _BIT0);

		break;

	case _DP_MAC_DECODE_METHOD_8B10B_DISPARITY:
		/* Start Record 8b10b or Disparity Error Count Value */
		dprx->rbus_ops->set_bit(PB_08_BIST_PATTERN_SEL,
			~(_BIT2 | _BIT1 | _BIT0), (_BIT2 | _BIT1 | _BIT0));

		break;

	case _DP_MAC_DECODE_METHOD_8B10B:
		fallthrough;
	default:
		/* Start Record 8b10b Error Count Value */
		dprx->rbus_ops->set_bit(PB_08_BIST_PATTERN_SEL,
			~(_BIT2 | _BIT1 | _BIT0), _BIT1);

		break;
	}
}

/**
 * rtk_dprx_decode_error_count_lane_measure - Measure 8b10b Error Count Per Lane
 */
static bool __maybe_unused rtk_dprx_decode_error_count_lane_measure(struct rtk_dprx *dprx,
		u32 error_criteria, u8 lane_number)
{
	u32 error_count = 0;

	/* Select Lane */
	dprx->rbus_ops->set_bit(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), (lane_number << 3));

	error_count = ((((u32)dprx->rbus_ops->get_byte(PB_0B_BIST_PATTERN3)) << 8) |
			dprx->rbus_ops->get_byte(PB_0C_BIST_PATTERN4));

	if (error_count > error_criteria) {
		dev_info(dprx->dev, "DP MAC RX0: Lane%u\n", lane_number);
		dev_info(dprx->dev, "DP MAC RX0: Lane Burst error_count=%u\n", error_count);
		return false;
	}

	return true;
}

/**
 * rtk_dprx_get_decode_error_count - Get 8b10b Error Count Per Lane
 */
static u32 __maybe_unused rtk_dprx_get_decode_error_count(struct rtk_dprx *dprx, u8 lane_number)
{
	u32 errorcount;

	/* Select Lane */
	dprx->rbus_ops->set_bit(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), (lane_number << 3));

	errorcount = ((((u32)dprx->rbus_ops->get_byte(PB_0B_BIST_PATTERN3)) << 8) |
			dprx->rbus_ops->get_byte(PB_0C_BIST_PATTERN4));

	return errorcount;
}

/**
 * rtk_dprx_mac_initial - Dp Mac Initial Setting
 */
static void rtk_dprx_mac_initial(struct rtk_dprx *dprx)
{
	/* Set Digital Phy to Normal */
	dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT1, 0x00);

	/* Enable K28.1 Detection */
	dprx->rbus_ops->set_bit(PB_4B_DP_COMMA_DET_MANU, ~_BIT2, _BIT2);

	/* err_8b10b_ctrl_gate = 1'b1 --> Avoid 8b10b Error Decode to FS */
	dprx->rbus_ops->set_bit(PB_00_HD_DP_SEL, ~_BIT1, _BIT1);

	/* DP RGB Output Enable */
	dprx->rbus_ops->set_bit(PB5_31_DP_OUTPUT_CTRL,
		~(_BIT3 | _BIT2 | _BIT1 | _BIT0), (_BIT2 | _BIT1 | _BIT0));

	/* DP CLK Output Enable */
	dprx->rbus_ops->set_bit(PB5_A2_PLL_OUT_CONTROL, ~(_BIT7), (_BIT7));

	/* 0x00:disable error correction, 0xF4:enable all error correction */
	dprx->rbus_ops->set_bit(PB6_00_MN_STRM_ATTR_CTRL,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT2 | _BIT1 | _BIT0), _BIT1);

	/* Disable DP Fifo Under/OverFlow Watch Dog */
	rtk_dprx_set_fifo_wd(dprx, false);

	/* Disable DP Fifo Under/OverFlow IRQ */
	rtk_dprx_set_fifo_irq(dprx, false);

	/* Disable DP No Video Stream IRQ */
	rtk_dprx_set_no_video_stream_irq(dprx, false);

	rtk_dprx_set_sdp_reset(dprx);

	dprx->mac_dat.msa_fail_reset_count = 0;

	// TODO: Restore HDCP Mode

#if 1 // TODO: Only for test, should removed after finish driver implementation
	/* Lane and control settings */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_LANE_MUX, 0x0000004E);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_STHD_CTRL_1, 0x00000081);
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_PG_CTRL_0, 0x00000001);


	rtk_dprx_clk_select(dprx, _DP_MAC_CLOCK_SELECT_LINK_CLOCK);

	dprx->rbus_ops->write(DPRX14_PLL_VID_1, 0x95050014);
	dprx->rbus_ops->write(DPRX14_PLL_VID_1, 0x95050016);
	dprx->rbus_ops->write(DPRX14_PLL_VID_1, 0x94050016);

	dprx->rbus_ops->write(DPRX14_PLL_AUD_1, 0x93050014);
	dprx->rbus_ops->write(DPRX14_PLL_AUD_1, 0x93050016);
	dprx->rbus_ops->write(DPRX14_PLL_AUD_1, 0x92050016);
#endif
#if RTK_ONLY_FOR_TEST // TODO: Only for test, should removed after finish driver implementation
	/* Simulate Link Training pass for testing */
	rtk_dprx_lt_set_link_integrity_fail(dprx, false);
	rtk_dprx_lt_set_fake_training_mode(dprx, false);
	rtk_dprx_lt_set_vbios_mode(dprx, false);
	dprx->mac_dat.pre_color_depth = BIT_DEPTH_8BPC;
	prince_1080p_rgb(dprx, &params_5p4g);
#endif

}

/**
 * rtk_dprx_sdp_initial - Dp Secondary Data Initial
 */
static void rtk_dprx_sdp_initial(struct rtk_dprx *dprx)
{
	/* Sec Ram Receive Initial */
	rtk_dprx_set_sdp_buff_rcv_initial(dprx);

	/* Mac Receive Metadata SDP Type Inital, Set by EDID/Display ID */
	rtk_dprx_set_meta_sdp_rcv_inital(dprx, _DP_SDP_TYPE_VSC_EXT_VESA);

	/* VSC Inital */
	rtk_dprx_set_vsc_sdp_rcv_initial(dprx);
}

/**
 * rtk_dprx_lane_swap_select - Set DP PHY to MAC Lane Swap / PN Swap / Clock Selection
 */
static void __maybe_unused rtk_dprx_lane_swap_select(struct rtk_dprx *dprx, u8 lane_select)
{
	/* DP PN Swap Setting */
	dprx->rbus_ops->set_bit(PB_06_DECODE_10B8B_ERROR,
		~(_BIT7 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
		ScalerDpPhyRxGetPCBLanePNSwap(_DP_LANE_3) << 7);
	dprx->rbus_ops->set_bit(PB_06_DECODE_10B8B_ERROR,
		~(_BIT6 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
		ScalerDpPhyRxGetPCBLanePNSwap(_DP_LANE_2) << 6);
	dprx->rbus_ops->set_bit(PB_06_DECODE_10B8B_ERROR,
		~(_BIT5 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
		ScalerDpPhyRxGetPCBLanePNSwap(_DP_LANE_1) << 5);
	dprx->rbus_ops->set_bit(PB_06_DECODE_10B8B_ERROR,
		~(_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
		ScalerDpPhyRxGetPCBLanePNSwap(_DP_LANE_0) << 4);

	/* DP Lane Swap Setting */
	dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
		~(_BIT7 | _BIT6), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_3) << 6);
	dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
		~(_BIT5 | _BIT4), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_2) << 4);
	dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
		~(_BIT3 | _BIT2), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_1) << 2);
	dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
		~(_BIT1 | _BIT0), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_0));

	ScalerDpPhyRxSetLanePNSwap(_DP_LANE_3,
		dprx->rbus_ops->get_bit(PB_06_DECODE_10B8B_ERROR, _BIT7) >> 7);
	ScalerDpPhyRxSetLanePNSwap(_DP_LANE_2,
		dprx->rbus_ops->get_bit(PB_06_DECODE_10B8B_ERROR, _BIT6) >> 6);
	ScalerDpPhyRxSetLanePNSwap(_DP_LANE_1,
		dprx->rbus_ops->get_bit(PB_06_DECODE_10B8B_ERROR, _BIT5) >> 5);
	ScalerDpPhyRxSetLanePNSwap(_DP_LANE_0,
		dprx->rbus_ops->get_bit(PB_06_DECODE_10B8B_ERROR, _BIT4) >> 4);

	ScalerDpPhyRxSetLaneMux(_DP_LANE_3,
		(dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT7 | _BIT6)) >> 6));
	ScalerDpPhyRxSetLaneMux(_DP_LANE_2,
		(dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT5 | _BIT4)) >> 4));
	ScalerDpPhyRxSetLaneMux(_DP_LANE_1,
		(dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT3 | _BIT2)) >> 2));
	ScalerDpPhyRxSetLaneMux(_DP_LANE_0,
		(dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT1 | _BIT0))));

	if ((ScalerDpPhyRxGetPhyCTSFlag() != true) &&
		(dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, _BIT7) != _BIT7))
		lane_select = (dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT1 | _BIT0)) << 6);

	/* MAC Clock from PHY Lane Selection */
	dprx->rbus_ops->set_bit(PB_02_PHY_DIG_RESET2_CTRL, ~(_BIT7 | _BIT6), lane_select);
}

/**
 * rtk_dprx_ext_lane_swap - Set DP PHY to MAC Lane Swap when External PD Exists and Lane Swap is Defined by Scaler
 */
static void __maybe_unused rtk_dprx_ext_lane_swap(struct rtk_dprx *dprx,
	enum RTK_DP_TYPE_C_PIN_CFG pin_cfg)
{
	if (pin_cfg == _TYPE_C_PIN_ASSIGNMENT_E) {
		/* DP Lane Swap Setting */
		dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
			~(_BIT7 | _BIT6), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_2) << 6);
		dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
			~(_BIT5 | _BIT4), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_3) << 4);
		dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
			~(_BIT3 | _BIT2), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_0) << 2);
		dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
			~(_BIT1 | _BIT0), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_1));
	} else {
		/* DP Lane Swap Setting */
		dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
			~(_BIT7 | _BIT6), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_0) << 6);
		dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
			~(_BIT5 | _BIT4), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_1) << 4);
		dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
			~(_BIT3 | _BIT2), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_2) << 2);
		dprx->rbus_ops->set_bit(PB_03_LANE_MUX,
			~(_BIT1 | _BIT0), ScalerDpPhyRxGetPCBLaneMapping(_DP_LANE_3));
	}
}

/**
 * rtk_dprx_get_lane_mux_mapping - Get DP MAC Lane Count
 */
static u8 __maybe_unused rtk_dprx_get_lane_mux_mapping(struct rtk_dprx *dprx,
	enum RTK_DP_LANE lane)
{
	u8 lane_cnt;

	switch (lane) {
	case _DP_LANE_0:
		lane_cnt = dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT1 | _BIT0));
		break;
	case _DP_LANE_1:
		lane_cnt = dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT3 | _BIT2)) >> 2;
		break;
	case _DP_LANE_2:
		lane_cnt = dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT5 | _BIT4)) >> 4;
		break;
	case _DP_LANE_3:
		lane_cnt = dprx->rbus_ops->get_bit(PB_03_LANE_MUX, (_BIT7 | _BIT6)) >> 6;
		break;
	default:
		lane_cnt = 0x00;
		break;
	}

	return lane_cnt;
}

/**
 * rtk_dprx_lane_count_set - Set DP MAC Lane Count
 */
static void rtk_dprx_lane_count_set(struct rtk_dprx *dprx, u8 lane_count)
{
	switch (lane_count) {
	case _DP_ONE_LANE:
		/* [4:3] DP MAC Select One Lane */
		dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~(_BIT4 | _BIT3), _BIT3);
		break;

	case _DP_TWO_LANE:
		/* [4:3] DP MAC Select Two Lane */
		dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~(_BIT4 | _BIT3), _BIT4);
		break;

	case _DP_FOUR_LANE:
		/* [4:3] DP MAC Select Four Lane */
		dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~(_BIT4 | _BIT3), (_BIT4 | _BIT3));
		break;

	default:
		break;
	}
}

/**
 * rtk_dprx_set_comma_detect - Set DP MAC Comma Detect
 */
static void __maybe_unused rtk_dprx_set_comma_detect(struct rtk_dprx *dprx, bool en)
{
	if (en) {
		/* [4] Enable Comma Detection */
		dprx->rbus_ops->set_bit(PB_05_SAMPLE_EDGE, ~_BIT4, 0x00);
	} else	{
		/* [4] Disable Comma Detection */
		dprx->rbus_ops->set_bit(PB_05_SAMPLE_EDGE, ~_BIT4, _BIT4);
	}
}

/**
 * rtk_dprx_clk_select - Set DP MAC Clock Selection
 */
static void __maybe_unused rtk_dprx_clk_select(struct rtk_dprx *dprx, enum RTK_DP_MAC_CLK_SELECT clk_sel)
{
	dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT6, clk_sel);
}

/**
 * rtk_dprx_sec_data_block_reset - Dp Secondary Data Reset
 */
static void rtk_dprx_sec_data_block_reset(struct rtk_dprx *dprx)
{
	/* Sec Data Block Reset */
	dprx->rbus_ops->set_bit(PB5_1E_MAC_DIG_RESET_CTRL, ~_BIT4, _BIT4);
	dprx->rbus_ops->set_bit(PB5_1E_MAC_DIG_RESET_CTRL, ~_BIT4, 0x00);
}

/**
 * rtk_dprx_align_check - Check Valid Lane Alignment
 */
static bool rtk_dprx_align_check(struct rtk_dprx *dprx)
{
	bool is_align = true;

	if ((dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01) & 0x1F) == _DP_ONE_LANE)
		goto exit;

	if (dprx->rbus_ops->get_bit(PB_0E_DESKEW_PHY, _BIT4) == 0x00) {
		if (dprx->rbus_ops->get_bit(PB_0E_DESKEW_PHY, (_BIT7 | _BIT6)) != (_BIT7 | _BIT6)) {
			dprx->rbus_ops->set_bit(PB_0E_DESKEW_PHY,
				~(_BIT7 | _BIT6 | _BIT4 | _BIT1), (_BIT7 | _BIT6));

			usleep_range(3000, 3100);
		} else {
			is_align = false;
		}
	}

exit:
	return is_align;
}

/**
 * rtk_dprx_decode_check - check DP 8b/10b Decode Error
 */
static bool rtk_dprx_decode_check(struct rtk_dprx *dprx)
{
	u8 temp = 3;
	u8 lane_count_set;
	bool is_ok = false;

	while (temp > 0) {
		/* Clear 8b/10b Decode Error Flag */
		dprx->rbus_ops->set_bit(PB_06_DECODE_10B8B_ERROR,
			~(_BIT3 | _BIT2 | _BIT1 | _BIT0), (_BIT3 | _BIT2 | _BIT1 | _BIT0));

		/* Delay Time 150 us */
		usleep_range(150, 160);

		lane_count_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01) & 0x1F;
		switch (lane_count_set) {
		case _DP_ONE_LANE:
			if (dprx->rbus_ops->get_bit(PB_06_DECODE_10B8B_ERROR, _BIT0) == 0x00)
				is_ok = true;
			break;

		case _DP_TWO_LANE:
			if (dprx->rbus_ops->get_bit(PB_06_DECODE_10B8B_ERROR, (_BIT1 | _BIT0)) == 0x00)
				is_ok = true;

			break;

		case _DP_FOUR_LANE:
			if (dprx->rbus_ops->get_bit(PB_06_DECODE_10B8B_ERROR, (_BIT3 | _BIT2 | _BIT1 | _BIT0)) == 0x00)
				is_ok = true;
			break;

		default:
				break;
		}

		if (is_ok)
			break;

		temp--;
	}

	if (!is_ok)
		dev_info(dprx->dev, "7. DP MAC RX0: 8b/10b Decode Error\n");

	return is_ok;
}

/**
 * rtk_dprx_five_layer_check - Perform 5-layer link quality check
 * @dprx: DPRX device structure
 * @link_bw_set: Link bandwidth setting from DPCD 0x100
 * @lane_count_set: Lane count setting from DPCD 0x101
 *
 * Performs sequential checks: CDR -> Signal -> Margin -> Align -> Decode.
 * This function provides fine-grained error codes for debugging.
 *
 * Return: DPRX_NO_ERR (0) on success, or DET_*_CHECK_FAIL error code.
 */
static int rtk_dprx_five_layer_check(struct rtk_dprx *dprx,
				     u8 link_bw_set, u8 lane_count_set)
{
	u8 lane_count = lane_count_set & 0x1F;

	if (!rtk_dprx_cdr_check(dprx, link_bw_set, lane_count))
		return DET_CDR_CHECK_FAIL;

#if 0 // TODO: Fix LOWER_BOUND/UPPER_BOUND
	if (!rtk_dprx_signal_check(dprx, link_bw_set, lane_count))
		return DET_SIGNAL_CHECK_FAIL;
#endif

	if (!rtk_dprx_margin_link_check())
		return DET_MARGIN_CHECK_FAIL;

	if (!rtk_dprx_align_check(dprx))
		return DET_ALIGN_CHECK_FAIL;

	if (!rtk_dprx_decode_check(dprx))
		return DET_DECODE_CHECK_FAIL;

	return DPRX_NO_ERR;
}

/**
 * rtk_dprx_get_video_stream - Check Dp Video Straam VBID
 */
static bool rtk_dprx_get_video_stream(struct rtk_dprx *dprx)
{
	return rtk_dprx_get_vbid_info(dprx, _DP_VBID_VIDEO_STREAM);
}

/**
 * rtk_dprx_get_msa_timing_info - Get Dp MSA Timing Info
 *
 * Return: DPRX_NO_ERR on success, or error code on failure
 */
static int rtk_dprx_get_msa_timing_info(struct rtk_dprx *dprx,
	struct rtk_dprx_stream_info *stream_info)
{
	int ret = SCAN_GET_MSA_FAIL;
	u32 byte_23_16;
	u32 byte_15_8;
	u32 byte_7_0;

	/* Pop up Main Stream Attributes */
	dprx->rbus_ops->set_bit(PB6_00_MN_STRM_ATTR_CTRL,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT0),
		(_BIT7 | _BIT6 | _BIT5 | _BIT3));

	/* Get HWidth */
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_0C_MSA_HWD_0);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_0D_MSA_HWD_1);
	stream_info->timing_info.HWidth = (byte_15_8 << 8) | byte_7_0;

	/* Get VHeight */
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_16_MSA_VHT_0);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_17_MSA_VHT_1);
	stream_info->timing_info.VHeight = (byte_15_8 << 8) | byte_7_0;

	// TODO: Implement _DP_FREESYNC_SUPPORT

	/* Get HTotal */
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_08_MSA_HTT_0);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_09_MSA_HTT_1);
	stream_info->timing_info.HTotal = (byte_15_8 << 8) | byte_7_0;

	/* Get HStart */
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_0A_MSA_HST_0);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_0B_MSA_HST_1);
	stream_info->timing_info.HStart = (byte_15_8 << 8) | byte_7_0;

	/* Get HSW */
	byte_15_8 = dprx->rbus_ops->get_bit(PB6_0E_MSA_HSW_0,
		(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0));
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_0F_MSA_HSW_1);
	stream_info->timing_info.HSWidth = (byte_15_8 << 8) | byte_7_0;

	/* Get HS Polarity */
	stream_info->timing_info.HSP = dprx->rbus_ops->get_bit(PB6_0E_MSA_HSW_0, _BIT7);

	/* Get VTotal */
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_10_MSA_VTTE_0);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_11_MSA_VTTE_1);
	stream_info->timing_info.VTotal = (byte_15_8 << 8) | byte_7_0;

	/* Get VTotal Odd */
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_12_MSA_VTTO_0);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_13_MSA_VTTO_1);
	stream_info->timing_info.VTotalOdd = (byte_15_8 << 8) | byte_7_0;

	/* Get VStart */
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_14_MSA_VST_0);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_15_MSA_VST_1);
	stream_info->timing_info.VStart = (byte_15_8 << 8) | byte_7_0;

	/* Get VSW */
	byte_15_8 = dprx->rbus_ops->get_bit(PB6_18_MSA_VSW_0,
		(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0));
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_19_MSA_VSW_1);
	stream_info->timing_info.VSWidth = (byte_15_8 << 8) | byte_7_0;

	/* Get VS Polarity */
	stream_info->timing_info.VSP = dprx->rbus_ops->get_bit(PB6_18_MSA_VSW_0, _BIT7);

	/* Check if MSA is Valid */
	if ((stream_info->timing_info.HTotal == 0x00) || (stream_info->timing_info.HStart == 0x00) ||
	   (stream_info->timing_info.HWidth == 0x00) || (stream_info->timing_info.HSWidth == 0x00) ||
	   (stream_info->timing_info.VTotal == 0x00) || (stream_info->timing_info.VStart == 0x00) ||
	   (stream_info->timing_info.VHeight == 0x00) || (stream_info->timing_info.VSWidth == 0x00)) {

		if ((stream_info->timing_info.HTotal == 0x00) && (stream_info->timing_info.HStart == 0x00) &&
		   (stream_info->timing_info.HWidth == 0x00) && (stream_info->timing_info.HSWidth == 0x00) &&
		   (stream_info->timing_info.VTotal == 0x00) && (stream_info->timing_info.VStart == 0x00) &&
		   (stream_info->timing_info.VHeight == 0x00) && (stream_info->timing_info.VSWidth == 0x00)) {
#if 1 // TODO: FIXME HDCP
			dev_info(dprx->dev, "MSA timing_info is zero\n");
			ret = SCAN_MSA_TIMING_ZERO;
			goto exit;
#else
			if (ScalerDpMacRx0HdcpCheckValid() == _TRUE) {
				if (dprx->mac_dat.msa_fail_reset_count < 10) {
					dprx->mac_dat.msa_fail_reset_count++;
				} else {
					dprx->mac_dat.msa_fail_reset_count = 0;

					dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT1, _BIT1);
					msleep(30);
					dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT1, 0x00);

					/* Delay a Complete Frame */
					msleep(_DP_ONE_FRAME_TIME_MAX);

					ScalerDpMacRx0DecodeErrorCountReset(_DP_MAC_DECODE_METHOD_8B10B);
				}
			} else {
				dev_info(dprx->dev, "MSA timing_info is zero\n");
				goto exit;
			}
#endif
		}
		dev_info(dprx->dev, "MSA is not valid\n");
		goto exit;
	}

	dprx->mac_dat.msa_fail_reset_count = 0;

	/* Calculate V Front Porch from MSA timing info */
	if (stream_info->timing_info.VTotal <=
	    stream_info->timing_info.VHeight + stream_info->timing_info.VStart) {
		dev_err(dprx->dev, "MSA timing invalid: VTotal=%u <= VHeight=%u + VStart=%u\n",
			stream_info->timing_info.VTotal,
			stream_info->timing_info.VHeight,
			stream_info->timing_info.VStart);
		ret = SCAN_MSA_TIMING_INVALID;
		goto exit;
	}
	dprx->mac_dat.vfront_porch = stream_info->timing_info.VTotal -
				     stream_info->timing_info.VHeight -
				     stream_info->timing_info.VStart;
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_VS_FRONT_PORCH,
				 dprx->mac_dat.vfront_porch & 0xFF);

	/* Get Mvid */
	byte_23_16 = dprx->rbus_ops->get_byte(PB6_1A_MSA_MVID_0);
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_1B_MSA_MVID_1);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_1C_MSA_MVID_2);
	stream_info->link_info.Mvid = (byte_23_16 << 16) | (byte_15_8 << 8) | byte_7_0;

	/* Check if Mvid is Valid */
	if (stream_info->link_info.Mvid == 0x00) {
		dev_info(dprx->dev, "Mvid is not valid\n");
		ret = SCAN_MSA_MVID_ZERO;
		goto exit;
	}

	/* Get Nvid */
	byte_23_16 = dprx->rbus_ops->get_byte(PB6_1D_MSA_NVID_0);
	byte_15_8 = dprx->rbus_ops->get_byte(PB6_1E_MSA_NVID_1);
	byte_7_0 = dprx->rbus_ops->get_byte(PB6_1F_MSA_NVID_2);
	stream_info->link_info.Nvid = (byte_23_16 << 16) | (byte_15_8 << 8) | byte_7_0;

	/* Check if Nvid is Valid */
	if (stream_info->link_info.Nvid == 0x00) {
		dev_info(dprx->dev, "Nvid is not valid\n");
		ret = SCAN_MSA_NVID_ZERO;
		goto exit;
	}

	dev_info(dprx->dev, "DP MAC RX0: MSA HTotal=%u\n", stream_info->timing_info.HTotal);
	dev_info(dprx->dev, "DP MAC RX0: MSA HStart=%u\n", stream_info->timing_info.HStart);
	dev_info(dprx->dev, "DP MAC RX0: MSA HWidth=%u\n", stream_info->timing_info.HWidth);
	dev_info(dprx->dev, "DP MAC RX0: MSA HSW=%u\n", stream_info->timing_info.HSWidth);
	dev_info(dprx->dev, "DP MAC RX0: MSA HSP=%u\n", stream_info->timing_info.HSP);
	dev_info(dprx->dev, "DP MAC RX0: MSA VTotal=%u\n", stream_info->timing_info.VTotal);
	dev_info(dprx->dev, "DP MAC RX0: MSA VStart=%u\n", stream_info->timing_info.VStart);
	dev_info(dprx->dev, "DP MAC RX0: MSA VHeight=%u\n", stream_info->timing_info.VHeight);
	dev_info(dprx->dev, "DP MAC RX0: MSA VSW=%u\n", stream_info->timing_info.VSWidth);
	dev_info(dprx->dev, "DP MAC RX0: MSA VSP=%u\n", stream_info->timing_info.VSP);
	dev_info(dprx->dev, "DP MAC RX0: Mvid=0x%x Nvid=0x%x\n",
		stream_info->link_info.Mvid, stream_info->link_info.Nvid);

	ret = DPRX_NO_ERR;

exit:
	return ret;
}

/**
 * rtk_dprx_misc_check - Check DP Misc Information
 */
static int rtk_dprx_misc_check(struct rtk_dprx *dprx)
{
	int ret = DPRX_NO_ERR;
	enum RTK_DP_COLOR_SPACE color_space;
	enum RTK_DP_COLORIMETRY colorimetry;
	u8 misc0;
	u8 color_space_val;
	u8 color_depth_val;
	u8 colorimetry_val;
	u8 quantization;
	bool vsc_sdp_color_mode;

	color_space = dprx->mac_dat.color_space;
	colorimetry = dprx->mac_dat.colorimetry;

	/* Check MISC1[6] */
	if ((dprx->rbus_ops->get_bit(PB6_03_MN_STRM_ATTR_MISC1, _BIT6)) == _BIT6) {
		ret = rtk_dprx_vsc_check(dprx);
		goto exit;
	}

	/* Get MISC Info */
	misc0 = dprx->rbus_ops->get_byte(PB6_02_MN_STRM_ATTR_MISC);
	color_space_val = (dprx->rbus_ops->get_bit(PB6_03_MN_STRM_ATTR_MISC1, _BIT7)) |
		(misc0 & (_BIT2 | _BIT1));
	vsc_sdp_color_mode = dprx->mac_dat.vsc_sdp_color_mode;

	/*
	 * Color Space Check
	 * Color format info source change from VSC --> MISC
	 */
	if (vsc_sdp_color_mode) {
		switch (color_space_val) {
		case _BIT1:
			if (color_space != _COLOR_SPACE_YCBCR422) {
				dev_info(dprx->dev, "DP MAC RX0: Misc change: Color Format, _COLOR_SPACE_YCBCR422\n");
				ret = MISC_COLOR_SPACE_CHANGED_Y422;
			}
			break;

		case _BIT2:
			if (color_space != _COLOR_SPACE_YCBCR444) {
				dev_info(dprx->dev, "DP MAC RX0: Misc change: Color Format, _COLOR_SPACE_YCBCR444\n");
				ret = MISC_COLOR_SPACE_CHANGED_Y444;
			}
			break;

		case _BIT7:
			if (color_space != _COLOR_SPACE_Y_ONLY) {
				dev_info(dprx->dev, "DP MAC RX0: Misc change: Color Format, _COLOR_SPACE_Y_ONLY\n");
				ret = MISC_COLOR_SPACE_CHANGED_Y_ONLY;
			}
			break;

		case (_BIT7 | _BIT1):
			if (color_space != _COLOR_SPACE_RAW) {
				dev_info(dprx->dev, "DP MAC RX0: Misc change: Color Format, _COLOR_SPACE_RAW\n");
				ret = MISC_COLOR_SPACE_CHANGED_RAW;
			}
			break;

		default:
			if (color_space != _COLOR_SPACE_RGB) {
				dev_info(dprx->dev, "DP MAC RX0: Misc change: Color Format, _COLOR_SPACE_RGB\n");
				ret = MISC_COLOR_SPACE_CHANGED_RGB;
			}
			break;
		}
	}

	if (ret)
		goto exit;

	if (color_space_val != dprx->mac_dat.pre_color_space) {
		dev_info(dprx->dev, "DP MAC RX0: Misc change: Color Space\n");
		ret = MISC_COLOR_SPACE_CHANGED;
		goto exit;
	}

	/* Color Depth Check */
	color_depth_val = (misc0 & (_BIT7 | _BIT6 | _BIT5)) >> 5;
	if (color_depth_val != dprx->mac_dat.pre_color_depth) {
		dev_info(dprx->dev, "DP MAC RX0: Misc change: Color Depth %u -> %u\n",
			dprx->mac_dat.pre_color_depth, color_depth_val);
		ret = MISC_COLOR_DEPTH_CHANGED;
		goto exit;
	}

	/* Colorimetry Check */
	colorimetry_val = (misc0 & (_BIT4 | _BIT3 | _BIT2 | _BIT1)) >> 1;
	if (colorimetry != _COLORIMETRY_EXT) {
		if (colorimetry_val != dprx->mac_dat.pre_colorimetry) {
			dev_info(dprx->dev, "DP MAC RX0: Misc change: Colorimetry\n");

			dprx->mac_dat.pre_colorimetry = colorimetry_val;
			ret = rtk_dprx_colorimetry_setting(dprx);
		}
	} else {
		dev_info(dprx->dev, "DP MAC RX0: Misc change: Colorimetry\n");

		dprx->mac_dat.pre_colorimetry = colorimetry_val;
		ret = rtk_dprx_colorimetry_setting(dprx);
	}

	/* Dynamic Range Check */
	quantization = (misc0 & _BIT3) >> 3;
	if (quantization != dprx->mac_dat.pre_quantization) {
		dev_info(dprx->dev, "DP MAC RX0: Misc change: Dynamic Range %u -> %u\n",
			dprx->mac_dat.pre_quantization, quantization);

		dprx->mac_dat.pre_quantization = quantization;
		ret = rtk_dprx_quantization_setting(dprx);
	}

	if (!dprx->mac_dat.en_free_sync) {
		u32 hsw;
		u32 vsw;

		/* Get HSW */
		hsw = dprx->rbus_ops->get_word(PB6_0E_MSA_HSW_0);
		hsw &= 0x7FFF;

		/* Get VSW */
		vsw = dprx->rbus_ops->get_word(PB6_18_MSA_VSW_0);
		vsw &= 0x7FFF;

		if ((hsw == 0x00) || (vsw == 0x00)) {
			ret = MISC_SYNC_CHANGED;
			goto exit;
		}
	}

exit:
	return ret;
}

/**
 * rtk_dprx_set_sdp_buff_rcv_initial - DP Rx0 SEC Type Initial
 */
static void rtk_dprx_set_sdp_buff_rcv_initial(struct rtk_dprx *dprx)
{
	/* Clr Status Register, Ram Receive SDP Type Inital */
	rtk_dprx_set_sdp_buff_rcv_mode(dprx, _DP_SDP_BUFF_SPD, _DP_SDP_TYPE_INFOFRAME_SPD);

	if (dprx->audio_support)
		rtk_dprx_set_sdp_buff_rcv_mode(dprx, _DP_SDP_BUFF_ISRC, _DP_SDP_TYPE_INFOFRAME_AUDIO);
}

/**
 * rtk_dprx_set_sdp_buff_rcv_mode - DP Rx0 Set SDP Ram Receive SDP Type(None Audio Releted)
 */
static void rtk_dprx_set_sdp_buff_rcv_mode(struct rtk_dprx *dprx,
		enum RTK_DP_SDP_BUFF index, enum RTK_DP_SDP_TYPE type)
{
	switch (index) {
	case _DP_SDP_BUFF_SPD:
		/* Set SPD Ram Receive SDP HB1(Infoframe Type Value) */
		dprx->rbus_ops->set_byte(PB6_2C_DP_SDP_SPD_MAT_HB1, type);

		/* Set SPD Ram Receive SDP Offset */
		dprx->rbus_ops->set_bit(PB6_32_DP_SDP_SPD_ADR, ~(_BIT7 | _BIT6 | _BIT5), 0x00);
		break;

	case _DP_SDP_BUFF_ISRC:
		/* Set ISRC Ram Receive SDP HB1(Infoframe Type Value) */
		dprx->rbus_ops->set_byte(PB6_E6_DP_SDP_ISRC_MAT_HB1, type);

		/* Set ISRC Ram Receive SDP Offset */
		dprx->rbus_ops->set_bit(PB6_E4_SDP_ISRC_ADR, ~(_BIT7 | _BIT6 | _BIT5), 0x00);

		/* ISRC SDP HB3[3] ignore */
		if (type == _DP_SDP_TYPE_ISRC)
			dprx->rbus_ops->set_bit(PB6_E3_SDP_ISRC_0, ~_BIT0, _BIT0);
		break;

	case _DP_SDP_BUFF_RSV0:
		/* Set RSV0 Ram Receive SDP HB1(Infoframe Type Value) */
		dprx->rbus_ops->set_byte(PB6_F0_DP_SDP_RSV0_MAT_HB1, type);

		/* Set RSV0 Ram Receive SDP Offset */
		dprx->rbus_ops->set_bit(PB6_F4_DP_SDP_RSV0_ADR, ~(_BIT7 | _BIT6 | _BIT5), 0x00);
		break;

	case _DP_SDP_BUFF_RSV1:
		/* Set RSV1 Ram Receive SDP HB1(Infoframe Type Value) */
		dprx->rbus_ops->set_byte(PB6_F6_DP_SDP_RSV1_MAT_HB1, type);

		/* Set RSV1 Ram Receive SDP Offset */
		dprx->rbus_ops->set_bit(PB6_FA_DP_SDP_RSV1_ADR, ~(_BIT7 | _BIT6 | _BIT5), 0x00);
		break;

	default:
		break;
	}
}

/**
 * rtk_dprx_set_meta_sdp_rcv_inital - DP Rx0 Set Mac Receive PPS or Metadata SDP
 */
static void rtk_dprx_set_meta_sdp_rcv_inital(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE type)
{
	/* Metadata SDP HB1 Set */
	dprx->rbus_ops->set_byte(PB6_E8_DP_MAC_DYN_MDATA_HB1, type);

	/* Metadata SDP HB0 Set */
	dprx->rbus_ops->set_byte(PB6_FC_DP_MAC_DYN_MDATA_HB0, 0x00);
}

/**
 * rtk_dprx_set_vsc_sdp_rcv_initial - DP Rx0 Set VSC SDP Initial
 */
static void rtk_dprx_set_vsc_sdp_rcv_initial(struct rtk_dprx *dprx)
{
	/* Set VSC Receive SDP HB0(Secondary-Data Packet ID) */
	dprx->rbus_ops->set_byte(PB6_42_VSC2, 0x00);

	/* Set VSC Receive SDP HB1(Infoframe Type Value) */
	dprx->rbus_ops->set_byte(PB6_43_VSC3, _DP_SDP_TYPE_VSC);

	/* Set VSC Receive SDP HB2(Revision Number) */
	dprx->rbus_ops->set_byte(PB6_44_VSC4, 0x02);

	/* Set VSC Receive SDP HB3(Number of Valid Data Bytes) */
	dprx->rbus_ops->set_byte(PB6_45_VSC5, 0x08);

	/* Set VSC Receive SDP HB2/HB3 Care */
	dprx->rbus_ops->set_bit(PB6_40_VSC0, ~_BIT7, 0x00);
}

/**
 * rtk_dprx_vsc_check - Check DP VSC SDP Information
 */
int rtk_dprx_vsc_check(struct rtk_dprx *dprx)
{
	int ret;
	u8 color_info_byte[3];
	u8 pixel_encodeing;
	u8 colorimetry_ext;
	u8 color_depth;
	u8 dynamic_range;

	memset_io(color_info_byte, 0, sizeof(color_info_byte));

	/* Read DB16~18 --> color_info_byte[0:2] */
	/* Get VSC SDP Data Packet */
	ret = rtk_dprx_get_sdp_pkt_data(dprx, _DP_SDP_TYPE_VSC, color_info_byte, 16, 3);
	if (ret)
		return ret;

	dprx->mac_dat.content_type = color_info_byte[2] & 0x7;

	/* Color Space Check */
	pixel_encodeing = (color_info_byte[0] >> 4) & 0xF;
	if (!dprx->mac_dat.vsc_sdp_color_mode) {

		/* Color format info source change from MISC --> VSC */
		if (dprx->mac_dat.color_space != pixel_encodeing) {
			dev_info(dprx->dev, "DP MAC RX0: VSC change: Color Space, %u -> %u\n",
				dprx->mac_dat.color_space, pixel_encodeing);
			ret = VSC_COLOR_SPACE_CHANGED;
			goto exit;
		}
	} else {

		if (dprx->mac_dat.pre_color_space != pixel_encodeing) {
			dev_info(dprx->dev, "DP MAC RX0: VSC change: Color Space, pre %u -> %u\n",
				dprx->mac_dat.pre_color_space, pixel_encodeing);
			ret = VSC_COLOR_SPACE_CHANGED;
			goto exit;
		}
	}

	/* Color Depth Check */
	color_depth = color_info_byte[1] & (_BIT2 | _BIT1 | _BIT0);
	if (dprx->mac_dat.pre_color_depth != color_depth) {
		dev_info(dprx->dev, "DP MAC RX0: VSC change: Color Depth, pre %u -> %u\n",
			 dprx->mac_dat.pre_color_depth, color_depth);
		ret = VSC_COLOR_DEPTH_CHANGED;
		goto exit;
	}

	/* Colorimetry Ext Check */
	colorimetry_ext = color_info_byte[0] & 0xF;
	if ((dprx->mac_dat.colorimetry != _COLORIMETRY_EXT) ||
		(dprx->mac_dat.pre_colorimetry_ext != colorimetry_ext)) {
		dev_info(dprx->dev, "DP MAC RX0: VSC change: Colorimetry Ext");

		dprx->mac_dat.pre_colorimetry_ext = colorimetry_ext;
		ret = rtk_dprx_colorimetry_ext_setting(dprx);
		goto exit;
	}

	/* Dynamic Range Check */
	dynamic_range = (color_info_byte[1] & _BIT7) >> 7;
	if ((dprx->mac_dat.color_space == _COLOR_SPACE_YCBCR444) ||
		(dprx->mac_dat.color_space == _COLOR_SPACE_YCBCR422) ||
		(dprx->mac_dat.color_space == _COLOR_SPACE_YCBCR420)) {

		if (dprx->mac_dat.pre_quantization != dynamic_range) {
			dev_info(dprx->dev, "DP MAC RX0: VSC change: Dynamic Range %u -> %u",
				dprx->mac_dat.pre_quantization, dynamic_range);

			dprx->mac_dat.pre_quantization = dynamic_range;
			ret = rtk_dprx_quantization_setting(dprx);
			goto exit;
		}
	} else {

		if (dprx->mac_dat.pre_quantization != dynamic_range) {
			dev_info(dprx->dev, "DP MAC RX0: VSC change: Dynamic Range %u -> %u",
				dprx->mac_dat.pre_quantization, dynamic_range);

			dprx->mac_dat.pre_quantization = dynamic_range;
			ret = rtk_dprx_quantization_setting(dprx);
			goto exit;
		}

	}

	ret = DPRX_NO_ERR;

exit:
	return ret;
}

/**
 * rtk_dprx_msa_active_change - Check DP MSA Vactive / Hactive Change
 */
static bool rtk_dprx_msa_active_change(struct rtk_dprx *dprx)
{
	u32 display_hactive;
	u32 display_vactive;
	u32 msa_hactive;
	u32 msa_vactive;

	/* Pop up Main Stream Attributes */
	dprx->rbus_ops->set_bit(PB6_00_MN_STRM_ATTR_CTRL,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT0),
		(_BIT7 | _BIT6 | _BIT5 | _BIT3));

	/* Get Display Format Hactive */
	display_hactive = dprx->rbus_ops->get_word(PB5_45_MN_DPF_HWD_M);

	/* Get MSA Hactive */
	msa_hactive = dprx->rbus_ops->get_word(PB6_0C_MSA_HWD_0);

	if (dprx->mac_dat.color_space == _COLOR_SPACE_YCBCR420)
		msa_hactive /= 2;

	/* Get Display Format Vactive */
	display_vactive = dprx->rbus_ops->get_word(PB5_4D_MN_DPF_VHT_M);

	/* Get MSA Vactive */
	msa_vactive = dprx->rbus_ops->get_word(PB6_16_MSA_VHT_0);

	if ((display_hactive != msa_hactive) || (display_vactive != msa_vactive))
		return true;

	return false;
}

/**
 * rtk_dprx_avmute - DP RGB Output Disable
 */
static void __maybe_unused rtk_dprx_avmute(struct rtk_dprx *dprx)
{
	if (dprx->rbus_ops->get_bit(PB6_3E_DP_IRQ_CTRL0, (_BIT1 | _BIT0)) == (_BIT1 | _BIT0)) {
			// TODO:  Enable D-domain force-to-background
	}

	/* Disable fifo overflwo/ underflwo IRQ */
	dprx->rbus_ops->set_bit(PB6_3E_DP_IRQ_CTRL0, ~(_BIT1 | _BIT0), 0x00);

	/* Disable RGB Output */
	dprx->rbus_ops->set_bit(PB5_31_DP_OUTPUT_CTRL, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), 0x00);

	/* Disable Audio Channel */
	// if (dprx->audio_support)
		// TODO: ScalerAudioDpAudioEnable(_DISABLE);
}

/**
 * rtk_dprx_get_free_sync_status_change - Check FREESYNC Enable
 */
static bool rtk_dprx_get_free_sync_status_change(struct rtk_dprx *dprx)
{
	if (dprx->free_sync_support != dprx->mac_dat.en_free_sync) {
		dev_info(dprx->dev, "free_sync_support=%s en_free_sync=%s\n",
			dprx->free_sync_support ? "Y":"N",
			dprx->mac_dat.en_free_sync ? "Y":"N");
		return true;
	}

	return false;
}

/**
 * rtk_dprx_get_amd_spd_info - Get AMD SPD Info Contents
 */
static int rtk_dprx_get_amd_spd_info(struct rtk_dprx *dprx,
		enum RTK_DP_SPD_INFO spd_info)
{
	int ret = 0;
	u8 amd_spd_data[_HW_DP_SDP_PAYLOAD_LENGTH];
	int vfreq_max = 0;
	int vfreq_min = 0;
	int pixel_rate = 0;
	u32 vfreq_max_bit = 0;

	vfreq_max_bit = _8_BIT;

	memset_io(amd_spd_data, 0, sizeof(amd_spd_data));

	ret = rtk_dprx_get_sdp_pkt_data(dprx, _DP_SDP_TYPE_INFOFRAME_SPD,
		amd_spd_data, 0, _HW_DP_SDP_PAYLOAD_LENGTH);
	if (ret)
		return ret;

	/* Check AMD OUI */
	if ((dprx->i_state.amd_spd_infoframe_receive == true) &&
	   ((amd_spd_data[1] == 0x1A) && (amd_spd_data[2] == 0x00) && (amd_spd_data[3] == 0x00))) {
		switch (spd_info) {
		case _SPD_INFO_FREESYNC_SUPPORT:
			ret = (((amd_spd_data[6] & _BIT0) != 0x00) ? 1 : 0);
			break;

		case _SPD_INFO_FREESYNC_ENABLE:
			ret = (((amd_spd_data[6] & _BIT1) != 0x00) ? 1 : 0);
			break;

		case _SPD_INFO_FREESYNC_ACTIVE:
			ret = (((amd_spd_data[6] & _BIT2) != 0x00) ? 1 : 0);
			break;

		case _SPD_INFO_SEAMLESS_LOCAL_DIMMING_DISABLE_CONTROL:
			ret = (((amd_spd_data[6] & _BIT5) != 0x00) ? 1 : 0);
			break;

		case _SPD_INFO_FREESYNC_MIN_VFREQ:

			vfreq_min = ((amd_spd_data[11] << 8) | amd_spd_data[7]);

			vfreq_min &= vfreq_max_bit;

			if (((vfreq_min * 10) < _PANEL_DP_FREESYNC_MIN_FRAME_RATE) ||
			 ((vfreq_min * 10) > _PANEL_DP_FREESYNC_MAX_FRAME_RATE))
				dev_info(dprx->dev, "Freesync SPD info abnormal\n");

			ret = vfreq_min;
			break;

		case _SPD_INFO_FREESYNC_MAX_VFREQ:

			vfreq_max = ((amd_spd_data[12] << 8) | amd_spd_data[8]);

			vfreq_max &= vfreq_max_bit;

			if (((vfreq_max * 10) < _PANEL_DP_FREESYNC_MIN_FRAME_RATE) ||
				((vfreq_max * 10) > _PANEL_DP_FREESYNC_MAX_FRAME_RATE))
				dev_info(dprx->dev, "Freesync SPD info abnormal\n");

			ret = vfreq_max;
			break;

		case _SPD_INFO_TARGET_OUTPUT_PIXEL_RATE:

			pixel_rate = (((u32)amd_spd_data[15] << 16) | ((u32)amd_spd_data[14] << 8) | amd_spd_data[13]);

			ret = pixel_rate;
			break;
		case _SPD_INFO_FIXED_RATE_CONTENT_ACTIVE:

			ret = (((amd_spd_data[16] & _BIT0) != 0x00) ? 1 : 0);
			break;
		default:
			ret = 0x00;
			break;
		}
	}

	return ret;
}

/**
 * rtk_dprx_set_bs_to_hs_delay - DP BS to HS Delay Calculation
 */
static void rtk_dprx_set_bs_to_hs_delay(struct rtk_dprx *dprx, struct rtk_timing_info *timing)
{
	u32 fifo_offset = 0;
	u32 delay = 0;
	enum RTK_DP_COLOR_SPACE color_space;
	u8 color_depth;

	color_space = dprx->mac_dat.color_space;
	color_depth = 8;

	/* DP fifo size = 256 x 96bit */
	if (color_space == _COLOR_SPACE_YCBCR422) {
		/*
		 * FifoOffset = (1/2 * (256 * 96) / (2 * depth per color)),
		 * color format is YCbCr 4:2:2
		 */
		fifo_offset = _HW_DP_RX_VIDEO_FIFO_SIZE / (4 * color_depth);
	} else {
		/*
		 * FifoOffset = (1/2 * (256 * 96) / (3 * depth per color)),
		 * color format is RGB or YCbCr 4:4:4 or YCbCr 4:2:0 or others
		 */
		fifo_offset = _HW_DP_RX_VIDEO_FIFO_SIZE / (6 * color_depth);
	}

	/*
	 * De Only Mode HW constraint: BS2HS delay < Htotal => 1/2 FIFO < H Width,
	 * Enough Margin = 1/2 H Width
	 */
	if (fifo_offset > (timing->HWidth >> 1))
		fifo_offset = (timing->HWidth >> 1);

	/*
	 * Get BS to HS delay according to (HBlanking + 1/2 FIFO - HStart),
	 * Unit is 2 pixel mode
	 */
	delay = ((timing->HTotal - timing->HWidth) +
		fifo_offset - timing->HStart) >> 1;

	/* Set BS to HS Delay */
	dprx->rbus_ops->set_byte(PB5_38_BS2HS_0, HIBYTE(delay));
	dprx->rbus_ops->set_byte(PB5_39_BS2HS_1, LOBYTE(delay));
}

/**
 * rtk_dprx_get_h_period - Get Current DP H Period
 *
 * @return: H period in nano sec
 */
static u32 __maybe_unused rtk_dprx_get_h_period(struct rtk_dprx *dprx)
{
	u8 hln_m;
	u8 hln_l;
	u32 h_period_ns = 0;
	u32 hbs2bs_count = 0;

	/* Pop up The result */
	rtk_dprx_set_measure_pop_up(dprx);

	/* HBs2Bs count in Link Clk / 2 */
	hln_m = dprx->rbus_ops->get_byte(PB5_5B_MN_MEAS_HLN_M);
	hln_l = dprx->rbus_ops->get_byte(PB5_5C_MN_MEAS_HLN_L);
	hbs2bs_count = (hln_m << 8) | hln_l;

	h_period_ns = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
	h_period_ns = h_period_ns * 27 / 2;
	h_period_ns = (hbs2bs_count * 1000) / h_period_ns;

	return h_period_ns;
}

/**
 * rtk_dprx_get_hsw - Get Current DP H Sync Width
 *
 * @return: H Sync Width in pixel clk
 */
static u32 __maybe_unused rtk_dprx_get_hsw(struct rtk_dprx *dprx)
{
	u8 hsw_m;
	u8 hsw_l;
	u32 hs_width;

	hsw_m = dprx->rbus_ops->get_byte(PB5_47_MN_DPF_HSW_M);
	hsw_l = dprx->rbus_ops->get_byte(PB5_48_MN_DPF_HSW_L);
	hs_width = ((hsw_m << 8) | hsw_l) & 0x7FFF;

	return hs_width;
}

/**
 * rtk_dprx_get_v_freq - Get Current DP V Freq
 *
 * @return: VFreq in 0.1Hz
 */
static u32 __maybe_unused rtk_dprx_get_v_freq(struct rtk_dprx *dprx)
{
	u8 data_20_16;
	u8 data_15_8;
	u8 data_7_0;
	u32 vbs2bs_count = 0;

    /* Pop up The result */
	rtk_dprx_set_measure_pop_up(dprx);

	data_20_16 = dprx->rbus_ops->get_byte(PB5_58_MN_MEAS_CTRL) & 0x1F;
	data_15_8 = dprx->rbus_ops->get_byte(PB5_59_MN_MEAS_VLN_M);
	data_7_0 = dprx->rbus_ops->get_byte(PB5_5A_MN_MEAS_VLN_L);

	vbs2bs_count = (data_20_16 << 16) | (data_15_8 << 8) | data_7_0;

	return (u32)_GDIPHY_RX_GDI_CLK_KHZ * 1000 / (vbs2bs_count / 10);
}

/**
 * rtk_dprx_get_v_period - Get Current DP V Period
 *
 * @return: V period in line
 */
static u32 __maybe_unused rtk_dprx_get_v_period(struct rtk_dprx *dprx)
{
	u8 data_20_16;
	u8 data_15_8;
	u8 data_7_0;
	u32 vbs2bs_count = 0;
	u32 hbs2bs_count = 0;
	u32 link_bw_set;

	/* Pop up The result */
	rtk_dprx_set_measure_pop_up(dprx);

	/* VBs2BS count in GDI clk */
	data_20_16 = dprx->rbus_ops->get_byte(PB5_58_MN_MEAS_CTRL) & 0x1F;
	data_15_8 = dprx->rbus_ops->get_byte(PB5_59_MN_MEAS_VLN_M);
	data_7_0 = dprx->rbus_ops->get_byte(PB5_5A_MN_MEAS_VLN_L);
	vbs2bs_count = (data_20_16 << 16) | (data_15_8 << 8) | data_7_0;

	/* HBs2Bs count in Link Clk / 2 */
	data_15_8 = dprx->rbus_ops->get_byte(PB5_5B_MN_MEAS_HLN_M);
	data_7_0 = dprx->rbus_ops->get_byte(PB5_5C_MN_MEAS_HLN_L);
	hbs2bs_count = (data_15_8 << 8) | data_7_0;

	/* VTotal in line, Link Clk / 2 : (Link Rate * 27 / 2) */
	link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);

	return (vbs2bs_count * (link_bw_set * 27 / 2) / hbs2bs_count * 1000 / _GDIPHY_RX_GDI_CLK_KHZ);
}

/**
 * rtk_dprx_get_spd_v_freq_max_min - Set Freesync AMD SPD info Vfreq max and min
 */
static void rtk_dprx_get_spd_v_freq_max_min(struct rtk_dprx *dprx)
{
	if (!dprx->mac_dat.en_free_sync)
		return;

	if (rtk_dprx_get_amd_spd_info(dprx, _SPD_INFO_FREESYNC_SUPPORT) == true) {
		/* Get DP DRR SPD Vmax */
		dprx->mac_dat.drr_vfreq_max = rtk_dprx_get_amd_spd_info(dprx, _SPD_INFO_FREESYNC_MAX_VFREQ) * 10;
		dprx->mac_dat.drr_vfreq_min = rtk_dprx_get_amd_spd_info(dprx, _SPD_INFO_FREESYNC_MIN_VFREQ) * 10;
	} else {
		dprx->mac_dat.drr_vfreq_max = 0;
		dprx->mac_dat.drr_vfreq_min = 0;
		dev_info(dprx->dev, "DRR without SPD info\n");
	}
}

/**
 * rtk_dprx_set_drr_msa_for_lut - Set DP Freesync Htotal info
 */
static void rtk_dprx_set_drr_msa_for_lut(struct rtk_dprx *dprx,
	struct rtk_timing_info *timing_info)
{
	u32 measure_htotal;
	u32 msa_htotal;
	u32 msa_htotal_margin;

	measure_htotal = timing_info->HTotal;
	msa_htotal = dprx->rbus_ops->get_word(PB6_08_MSA_HTT_0);
	msa_htotal_margin = (measure_htotal * _EDID_ANALYSIS_HTOTAL_MARGIN / 100);

	if ((msa_htotal > (measure_htotal + msa_htotal_margin)) ||
		(msa_htotal < (measure_htotal - msa_htotal_margin))) {
		dprx->mac_dat.drr_msa_htotal = measure_htotal;
		dprx->mac_dat.drr_htotal_margin = msa_htotal_margin;

		dev_info(dprx->dev, "Freesync Get MSA Htotal Fail, msa_htotal=%u\n", msa_htotal);
	} else {
		dprx->mac_dat.drr_msa_htotal = msa_htotal;
		dprx->mac_dat.drr_htotal_margin = 1;

		timing_info->HTotal = msa_htotal;
	}
}

/**
 * rtk_dprx_set_bs_to_vs_delay - DP BS to VS Delay Calculation
 */
static void rtk_dprx_set_bs_to_vs_delay(struct rtk_dprx *dprx,
		struct rtk_dprx_stream_info *stream_info)
{
	struct rtk_timing_info *timing_info;
	struct rtk_link_info *link_info;
	u32 vfront_porch = 0;
	u32 hfront_porch = 0;
	u32 bs_to_vs_delay = 0;
	u32 fifo_offset = 0;

	timing_info = &stream_info->timing_info;
	link_info = &stream_info->link_info;

	/* Get V Front Porch, unit is pixel clock */
	vfront_porch = dprx->mac_dat.vfront_porch * timing_info->HTotal;

	/* Get H Front Porch, unit is pixel clock */
	hfront_porch = (timing_info->HTotal - timing_info->HWidth - timing_info->HStart);

	/* Get 1/2 Video FIFO Size, unit is pixel clock */
	if (dprx->mac_dat.color_space == _COLOR_SPACE_YCBCR422) {
		/*
		 * FifoOffset = (1/2 * Video FIFO Size / (2 * depth per color))
		 * color format is YCbCr 4:2:2
		 */
		fifo_offset = _HW_DP_RX_VIDEO_FIFO_SIZE / (4 * GET_COLOR_DEPTH);
	} else {
		/*
		 * FifoOffset = (1/2 * Video FIFO Size / (3 * depth per color))
		 * color format is RGB or YCbCr 4:4:4 or others
		 */
		fifo_offset = _HW_DP_RX_VIDEO_FIFO_SIZE / (6 * GET_COLOR_DEPTH);
	}

	/* Get BS to VS Delay, unit is 1/2 link clock */
	bs_to_vs_delay = rtk_dprx_compute_mul_div(dprx, (vfront_porch + hfront_porch + fifo_offset),
		link_info->LinkClockHz / 2, link_info->StreamClockHz);

	/* Set BS to VS Delay of Odd Field */
	dprx->rbus_ops->set_byte(PB5_35_OVBLK2VS_H, LOBYTE(HIWORD(bs_to_vs_delay)));
	dprx->rbus_ops->set_byte(PB5_36_OVBLK2VS_M, HIBYTE(LOWORD(bs_to_vs_delay)));
	dprx->rbus_ops->set_byte(PB5_37_OVBLK2VS_L, LOBYTE(LOWORD(bs_to_vs_delay)));

	if (link_info->InterlaceOddMode == true) {
		/* BStoVSDelay = BStoVSDelay + 1 Line(Unit is 1/2 Link Clock) */
		bs_to_vs_delay += rtk_dprx_compute_mul_div(dprx, (u32)timing_info->HTotal,
			link_info->LinkClockHz / 2, link_info->StreamClockHz);
	}

	/* Set BS to VS Delay of Even Field */
	dprx->rbus_ops->set_byte(PB5_32_EVBLK2VS_H, LOBYTE(HIWORD(bs_to_vs_delay)));
	dprx->rbus_ops->set_byte(PB5_33_EVBLK2VS_M, HIBYTE(LOWORD(bs_to_vs_delay)));
	dprx->rbus_ops->set_byte(PB5_34_EVBLK2VS_L, LOBYTE(LOWORD(bs_to_vs_delay)));
}

/**
 * rtk_dprx_interlace_check - Check Dp Interlace by VBID or Measure
 */
static bool __maybe_unused rtk_dprx_interlace_check(struct rtk_dprx *dprx,
		struct rtk_dprx_stream_info *stream_info)
{
	return false;
}

/**
 * rtk_dprx_interlace_vtotal_get_msa_check - Judge Vtotal from MSA or not
 */
static bool __maybe_unused rtk_dprx_interlace_vtotal_get_msa_check(struct rtk_dprx *dprx,
		struct rtk_dprx_stream_info *stream_info)
{
	u64 v_freq = 0;
	u32 htt;
	u32 vtte;

	/* Vfreq (unit : 0.01Hz) */
	v_freq = (u64)_GDIPHY_RX_GDI_CLK_KHZ * 1000 * 100 / stream_info->link_info.VBsToBsCountN;
	htt = dprx->rbus_ops->get_word(PB6_08_MSA_HTT_0);
	vtte = dprx->rbus_ops->get_word(PB6_10_MSA_VTTE_0);

	// Central Vfreq = 120Hz +/- 1% = 118.8Hz ~ 121.2Hz
	if ((htt == 2200) && (vtte == 562) &&
	   ((v_freq > 11880) && (v_freq < 12120)) &&
	   (dprx->rbus_ops->get_bit(PB6_01_DP_VBID, _BIT2) == _BIT2)) {
		return true;
	}

	return false;

}

/**
 * rtk_dprx_get_video_info - DP Get Video Info
 */
static int rtk_dprx_get_video_info(struct rtk_dprx *dprx)
{
	int ret;
	u8 color_info[3];

	memset_io(color_info, 0, sizeof(color_info));

	dprx->mac_dat.vsc_sdp_color_mode = false;

	/* Check MISC1[6] */
	if ((dprx->rbus_ops->get_bit(PB6_03_MN_STRM_ATTR_MISC1, _BIT6)) == _BIT6) {
		/* Use VSC SDP */
		dprx->mac_dat.vsc_sdp_color_mode = true;

		/* Read DB16~18 --> color_info[0:2] */
		/* Get VSC SDP Data Packet */
		ret = rtk_dprx_get_sdp_pkt_data(dprx, _DP_SDP_TYPE_VSC, color_info, 16, 3);
		if (ret)
			goto exit;

		/* Set Color Info PreValue */
		dprx->mac_dat.pre_colorimetry_ext = color_info[0] & (_BIT3 | _BIT2 | _BIT1 | _BIT0);
		dprx->mac_dat.pre_quantization = (color_info[1] & _BIT7) >> 7;
		dprx->mac_dat.pre_color_depth = color_info[1] & (_BIT2 | _BIT1 | _BIT0);
		dprx->mac_dat.pre_color_space = color_info[0] & (_BIT7 | _BIT6 | _BIT5 | _BIT4);
		dprx->mac_dat.content_type = color_info[2] & (_BIT2 | _BIT1 | _BIT0);

		dev_info(dprx->dev, "VSC DB16=0x%02x DB17=%02x\n", color_info[0], color_info[1]);
	} else {
		/* Get MISC Info */
		color_info[0] = dprx->rbus_ops->get_byte(PB6_02_MN_STRM_ATTR_MISC);
		color_info[1] = dprx->rbus_ops->get_byte(PB6_03_MN_STRM_ATTR_MISC1);

		/* Set Color Info PreValue */
		dprx->mac_dat.pre_colorimetry = (color_info[0] & (_BIT4 | _BIT3 | _BIT2 | _BIT1)) >> 1;
		dprx->mac_dat.pre_quantization = (color_info[0] & _BIT3) >> 3;
		dprx->mac_dat.pre_color_depth = (color_info[0] & (_BIT7 | _BIT6 | _BIT5)) >> 5;
		dprx->mac_dat.pre_color_space = (color_info[1] & _BIT7) | (color_info[0] & (_BIT2 | _BIT1));
	}

	ret = DPRX_NO_ERR;
exit:
	return ret;
}

/**
 * rtk_dprx_set_color_info - DP Set Color Info Macros
 */
static void rtk_dprx_set_color_info(struct rtk_dprx *dprx,
		struct rtk_timing_info *timing_info)
{
	enum RTK_DP_COLOR_SPACE color_space;
	u8 pre_color_space;
	u8 pre_quantization;

	pre_quantization = dprx->mac_dat.pre_quantization;
	pre_color_space = dprx->mac_dat.pre_color_space;

	/* Set Color Space Macro */
	if (dprx->mac_dat.vsc_sdp_color_mode == true) {
		/* Use VSC SDP */
		switch (pre_color_space) {
		case _VSC_COLOR_SPACE_0:
			color_space = _COLOR_SPACE_RGB;
			break;
		case _VSC_COLOR_SPACE_1:
			color_space = _COLOR_SPACE_YCBCR444;
			break;
		case _VSC_COLOR_SPACE_2:
			color_space = _COLOR_SPACE_YCBCR422;
			break;
		case _VSC_COLOR_SPACE_3:
			color_space = _COLOR_SPACE_YCBCR420;
			break;
		case _VSC_COLOR_SPACE_4:
			color_space = _COLOR_SPACE_Y_ONLY;
			break;
		case _VSC_COLOR_SPACE_5:
			color_space = _COLOR_SPACE_RAW;
			break;
		default:
			color_space = _COLOR_SPACE_RGB;
			break;
		}

		dprx->mac_dat.color_space = color_space;

		/* Set Colorimetry Ext */
		rtk_dprx_colorimetry_ext_setting(dprx);

	} else {
		/* Use MSA */
		switch (pre_color_space) {
		case _BIT1:
			color_space = _COLOR_SPACE_YCBCR422;
			break;
		case _BIT2:
			color_space = _COLOR_SPACE_YCBCR444;
			break;
		case _BIT7:
			color_space = _COLOR_SPACE_Y_ONLY;
			break;
		case (_BIT7 | _BIT1):
			color_space = _COLOR_SPACE_RAW;
			break;
		default:
			color_space = _COLOR_SPACE_RGB;
			break;
		}

		dprx->mac_dat.color_space = color_space;

		/* Set Colorimetry */
		rtk_dprx_colorimetry_setting(dprx);
	}

	/* Set Porch Color */
	if ((color_space == _COLOR_SPACE_YCBCR444) ||
		(color_space == _COLOR_SPACE_YCBCR422) ||
		(color_space == _COLOR_SPACE_YCBCR420)) {

		if (pre_quantization == _DP_COLOR_QUANTIZATION_LIMIT)
			rtk_dprx_set_porch_color_ycc_limit(dprx, color_space);
		else if (pre_quantization == _DP_COLOR_QUANTIZATION_FULL)
			rtk_dprx_set_porch_color_ycc_full(dprx, color_space);

	} else {
		rtk_dprx_set_porch_color_rgb(dprx);
	}

	if (color_space == _COLOR_SPACE_YCBCR420) {
		dev_info(dprx->dev, "Horizontal Timing Info Modify(Div 2) for YCbCr420\n");
		timing_info->HTotal /= 2;
		timing_info->HStart /= 2;
		timing_info->HWidth /= 2;
		timing_info->HSWidth /= 2;
	}

	/* Set Quantization Range */
	rtk_dprx_quantization_setting(dprx);
}

/**
 * rtk_dprx_video_setting - DP Video Setting for PG
 */
static int rtk_dprx_video_setting(struct rtk_dprx *dprx,
	enum RTK_DP_COLOR_SPACE color_space, u8 pre_color_depth)
{
	int ret_val = DPRX_NO_ERR;

	/* Set Color Space for Display Format Gen */
	switch (color_space) {
	case _COLOR_SPACE_RGB:
		dev_info(dprx->dev, "DP MAC RX0: _COLOR_SPACE_RGB\n");
		dprx->rbus_ops->set_bit(PB5_20_PG_CTRL_0, ~(_BIT5 | _BIT4 | _BIT3), 0x00);
		break;

	case _COLOR_SPACE_YCBCR444:
		dev_info(dprx->dev, "DP MAC RX0: _COLOR_SPACE_YCBCR444\n");
		dprx->rbus_ops->set_bit(PB5_20_PG_CTRL_0, ~(_BIT5 | _BIT4 | _BIT3), _BIT4);
		break;

	case _COLOR_SPACE_YCBCR422:
		dev_info(dprx->dev, "DP MAC RX0: _COLOR_SPACE_YCBCR422\n");
		dprx->rbus_ops->set_bit(PB5_20_PG_CTRL_0, ~(_BIT5 | _BIT4 | _BIT3), _BIT3);
		break;

	case _COLOR_SPACE_YCBCR420:
		dev_info(dprx->dev, "DP MAC RX0: _COLOR_SPACE_YCBCR420\n");
		dprx->rbus_ops->set_bit(PB5_20_PG_CTRL_0, ~(_BIT5 | _BIT4 | _BIT3), _BIT5);
		break;

	case _COLOR_SPACE_Y_ONLY:
		dev_info(dprx->dev, "DP MAC RX0: _COLOR_SPACE_Y_ONLY\n");
		dprx->rbus_ops->set_bit(PB5_20_PG_CTRL_0, ~(_BIT5 | _BIT4 | _BIT3), (_BIT4 | _BIT3));
		break;

	case _COLOR_SPACE_RAW:
		dev_info(dprx->dev, "DP MAC RX0: _COLOR_SPACE_RAW\n");
		dprx->rbus_ops->set_bit(PB5_20_PG_CTRL_0, ~(_BIT5 | _BIT4 | _BIT3), 0x00);
		break;

	default:
		/* RGB */
		dprx->rbus_ops->set_bit(PB5_20_PG_CTRL_0, ~(_BIT5 | _BIT4 | _BIT3), 0x00);
		break;
	}

	/* Set Color Depth for Display Format Gen */
	if (pre_color_depth != BIT_DEPTH_8BPC) {
		dev_err(dprx->dev, "Unexpected pre_color_depth=%u\n", pre_color_depth);
		ret_val = UNEXPECTED_COLOR_DEPTH;
	}

	dprx->rbus_ops->set_bit(PB5_20_PG_CTRL_0, ~(_BIT2 | _BIT1 | _BIT0), pre_color_depth);

	return ret_val;
}

/**
 * rtk_dprx_colorimetry_setting - DP Rx0 Colorimetry Setting
 */
static int rtk_dprx_colorimetry_setting(struct rtk_dprx *dprx)
{
	enum RTK_DP_COLORIMETRY colorimetry = _COLORIMETRY_NONE;
	enum RTK_DP_COLOR_SPACE color_space;
	u8 colorimetry_value = (dprx->mac_dat.pre_colorimetry << 1);

	color_space = dprx->mac_dat.color_space;
	if ((color_space == _COLOR_SPACE_YCBCR422) ||
		(color_space == _COLOR_SPACE_YCBCR444)) {

		switch (colorimetry_value & (_BIT4 | _BIT3)) {
		case 0:
			colorimetry = _COLORIMETRY_YCC_XVYCC601;
			break;
		case _BIT3:
			colorimetry = _COLORIMETRY_YCC_ITUR_BT601;
			break;
		case _BIT4:
			colorimetry = _COLORIMETRY_YCC_XVYCC709;
			break;
		case (_BIT4 | _BIT3):
			colorimetry = _COLORIMETRY_YCC_ITUR_BT709;
			break;
		default:
			break;
		}
	} else if (color_space == _COLOR_SPACE_RGB) {

		switch (colorimetry_value & (_BIT4 | _BIT3 | _BIT2 | _BIT1)) {
		case 0:
			colorimetry = _COLORIMETRY_RGB_SRGB;
			break;

		case _BIT3:
			colorimetry = _COLORIMETRY_RGB_SRGB;
			break;

		case (_BIT2 | _BIT1):
			colorimetry = _COLORIMETRY_RGB_XRRGB;
			break;

		case (_BIT4 | _BIT2 | _BIT1):
			colorimetry = _COLORIMETRY_RGB_SCRGB;
			break;

		case (_BIT4 | _BIT3):
			colorimetry = _COLORIMETRY_RGB_ADOBERGB;
			break;

		case (_BIT3 | _BIT2 | _BIT1):
			colorimetry = _COLORIMETRY_RGB_DCI_P3;
			break;

		case (_BIT4 | _BIT3 | _BIT2 | _BIT1):
			colorimetry = _COLORIMETRY_RGB_COLOR_PROFILE;
			break;

		default:
			colorimetry = _COLORIMETRY_RGB_SRGB;
			break;
		}
	} else if (color_space == _COLOR_SPACE_Y_ONLY) {
		colorimetry = _COLORIMETRY_Y_ONLY;
	} else if (color_space == _COLOR_SPACE_RAW) {
		colorimetry = _COLORIMETRY_RAW;
	}

	if (colorimetry != _COLORIMETRY_NONE) {
		dprx->mac_dat.colorimetry = colorimetry;
		dev_info(dprx->dev, "Colorimetry=%u\n", dprx->mac_dat.colorimetry);
		return DPRX_NO_ERR;
	}

	return COLORIMETRY_CHANGE_ERR;
}

/**
 * rtk_dprx_colorimetry_ext_setting - DP Rx0 Colorimetry Extended Setting
 */
static int rtk_dprx_colorimetry_ext_setting(struct rtk_dprx *dprx)
{
	int ret = VSC_COLORIMETRY_EXT_CHANGED;
	enum RTK_DP_COLOR_SPACE color_space;
	u8 pre_colorimetry_ext;
	u8 colorimetry_ext = _COLORIMETRY_EXT_RESERVED;

	dprx->mac_dat.colorimetry = _COLORIMETRY_EXT;

	color_space = dprx->mac_dat.color_space;
	pre_colorimetry_ext = dprx->mac_dat.pre_colorimetry_ext;

	if ((color_space == _COLOR_SPACE_YCBCR420) ||
		(color_space == _COLOR_SPACE_YCBCR422) ||
		(color_space == _COLOR_SPACE_YCBCR444)) {
		switch (pre_colorimetry_ext) {
		case _VSC_COLORIMETRY_0:
			colorimetry_ext = _COLORIMETRY_EXT_YCC_ITUR_BT601;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_YCC_ITUR_BT601\n");
			break;

		case _VSC_COLORIMETRY_1:
			colorimetry_ext = _COLORIMETRY_EXT_YCC_ITUR_BT709;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_YCC_ITUR_BT709\n");
			break;

		case _VSC_COLORIMETRY_2:
			colorimetry_ext = _COLORIMETRY_EXT_YCC_XVYCC601;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_YCC_XVYCC601\n");
			break;

		case _VSC_COLORIMETRY_3:
			colorimetry_ext = _COLORIMETRY_EXT_YCC_XVYCC709;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_YCC_XVYCC709\n");
			break;

		case _VSC_COLORIMETRY_4:
			colorimetry_ext = _COLORIMETRY_EXT_YCC_SYCC601;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_YCC_SYCC601\n");
			break;

		case _VSC_COLORIMETRY_5:
			colorimetry_ext = _COLORIMETRY_EXT_YCC_ADOBEYCC601;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_YCC_ADOBEYCC601\n");
			break;

		case _VSC_COLORIMETRY_6:
			colorimetry_ext = _COLORIMETRY_EXT_YCC_ITUR_BT2020_CL;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_YCC_ITUR_BT2020_CL\n");
			break;

		case _VSC_COLORIMETRY_7:
			colorimetry_ext = _COLORIMETRY_EXT_YCC_ITUR_BT2020_NCL;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_YCC_ITUR_BT2020_NCL\n");
			break;

		default:
			colorimetry_ext = _COLORIMETRY_EXT_RESERVED;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RESERVED\n");
			break;
		}
	} else if (color_space == _COLOR_SPACE_RGB) {
		switch (pre_colorimetry_ext) {
		case _VSC_COLORIMETRY_0:
			colorimetry_ext = _COLORIMETRY_EXT_RGB_SRGB;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RGB_SRGB\n");
			break;

		case _VSC_COLORIMETRY_1:
			colorimetry_ext = _COLORIMETRY_EXT_RGB_XRRGB;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RGB_XRRGB\n");
			break;

		case _VSC_COLORIMETRY_2:
			colorimetry_ext = _COLORIMETRY_EXT_RGB_SCRGB;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RGB_SCRGB\n");
			break;

		case _VSC_COLORIMETRY_3:
			colorimetry_ext = _COLORIMETRY_EXT_RGB_ADOBERGB;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RGB_ADOBERGB\n");
			break;

		case _VSC_COLORIMETRY_4:
			colorimetry_ext = _COLORIMETRY_EXT_RGB_DCI_P3;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RGB_DCI_P3\n");
			break;

		case _VSC_COLORIMETRY_5:
			colorimetry_ext = _COLORIMETRY_EXT_RGB_CUSTOM_COLOR_PROFILE;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RGB_CUSTOM_COLOR_PROFILE\n");
			break;

		case _VSC_COLORIMETRY_6:
			colorimetry_ext = _COLORIMETRY_EXT_RGB_ITUR_BT2020;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RGB_ITUR_BT2020\n");
			break;

		default:
			colorimetry_ext = _COLORIMETRY_EXT_RESERVED;
			dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RESERVED\n");
			break;
		}
	} else if (color_space == _COLOR_SPACE_Y_ONLY) {
		colorimetry_ext = _COLORIMETRY_EXT_Y_ONLY_DICOM_PART14;
		dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_Y_ONLY_DICOM_PART14\n");
	} else if (color_space == _COLOR_SPACE_RAW) {
		colorimetry_ext = _COLORIMETRY_EXT_RAW_CUSTOM_COLOR_PROFILE;
		dev_info(dprx->dev, "DP MAC RX0: _COLORIMETRY_EXT_RAW_CUSTOM_COLOR_PROFILE\n");
	} else {
		ret = VSC_COLORIMETRY_EXT_CHANGE_ERR;
	}

	dprx->mac_dat.colorimetry_ext = colorimetry_ext;

	return ret;
}


/**
 * rtk_dprx_quantization_setting - DP Rx0 Quantization Setting
 */
static int rtk_dprx_quantization_setting(struct rtk_dprx *dprx)
{
	int ret = VSC_QUANTIZATION_CHANGED;
	enum RTK_DP_COLOR_SPACE color_space;
	enum RTK_DP_COLORIMETRY colorimetry;
	u8 pre_quantization;
	u8 rgb_quantization;
	u8 ycc_quantization;

	color_space = dprx->mac_dat.color_space;
	colorimetry = dprx->mac_dat.colorimetry;
	pre_quantization = dprx->mac_dat.pre_quantization;

	switch (color_space) {
	case _COLOR_SPACE_YCBCR420:
		fallthrough;
	case _COLOR_SPACE_YCBCR422:
		fallthrough;
	case _COLOR_SPACE_YCBCR444:
		/* Special case for xvYCC */
		if ((colorimetry == _COLORIMETRY_YCC_XVYCC601) ||
			(colorimetry == _COLORIMETRY_YCC_XVYCC709)) {
			ycc_quantization = _YCC_QUANTIZATION_LIMIT_RANGE;
			dev_info(dprx->dev, "DP MAC RX0: _YCC_QUANTIZATION_LIMIT_RANGE\n");
		} else {
			if (pre_quantization == _DP_COLOR_QUANTIZATION_FULL) {
				ycc_quantization = _YCC_QUANTIZATION_FULL_RANGE;
				dev_info(dprx->dev, "DP MAC RX0: _YCC_QUANTIZATION_FULL_RANGE\n");
			} else {
				ycc_quantization = _YCC_QUANTIZATION_LIMIT_RANGE;
				dev_info(dprx->dev, "DP MAC RX0: _YCC_QUANTIZATION_LIMIT_RANGE\n");
			}
		}

		rgb_quantization = _RGB_QUANTIZATION_RESERVED;

		break;

	case _COLOR_SPACE_RGB:
		fallthrough;
	default:

		/* Special case for AdobeRGB */
		if (colorimetry == _COLORIMETRY_RGB_ADOBERGB) {
			rgb_quantization = _RGB_QUANTIZATION_FULL_RANGE;
			dev_info(dprx->dev, "DP MAC RX0: _RGB_QUANTIZATION_FULL_RANGE\n");
		} else {
			if (pre_quantization == _DP_COLOR_QUANTIZATION_FULL) {
				rgb_quantization = _RGB_QUANTIZATION_FULL_RANGE;
				dev_info(dprx->dev, "DP MAC RX0: _RGB_QUANTIZATION_FULL_RANGE\n");
			} else {
				rgb_quantization  = _RGB_QUANTIZATION_LIMIT_RANGE;
				dev_info(dprx->dev, "DP MAC RX0: _RGB_QUANTIZATION_LIMIT_RANGE\n");
			}
		}

		ycc_quantization = _YCC_QUANTIZATION_FULL_RANGE;

		break;
	}

	dprx->mac_dat.rgb_quantization = rgb_quantization;
	dprx->mac_dat.ycc_quantization = ycc_quantization;

	return ret;
}

/**
 * rtk_dprx_stream_clk_regenerate - DP Stream Clk PLL Setting
 */
static int rtk_dprx_stream_clk_regenerate(struct rtk_dprx *dprx,
	struct rtk_link_info *link_info)
{
	int ret;
	enum RTK_DP_COLOR_SPACE color_space;
	u8 link_rate;
	u32 input_clk_hz = 0;
	u32 target_clk_hz = 0;
	u32 speed_limit = (u32)(_HW_DATA_PATH_SPEED_LIMIT * 100000);

	color_space = dprx->mac_dat.color_space;

	/*
	 * PLL Input Clock Setting
	 * Set PLL Input Clock and Divider for Link Clock
	 */
	link_rate = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
	ret = rtk_dprx_pll_input_clock_setting(dprx, link_rate, &input_clk_hz,
		link_info->LinkClockHz);
	if (ret)
		goto exit;

	dev_info(dprx->dev, "DP MAC RX0: PLL In Clk=%u\n", input_clk_hz);

	/*
	 * PLL Target Clock Setting
	 * Use 1-Pixel Mode to Gen Stream Clk Output to I Domain
	 */
	dprx->rbus_ops->set_bit(PB5_1F_SOURCE_SEL_4, ~(_BIT7 | _BIT0), 0x00);

	if (color_space == _COLOR_SPACE_YCBCR420) {
		/* Enable YUV420 Output Clock */
		dprx->rbus_ops->set_bit(PB5_A2_PLL_OUT_CONTROL, ~_BIT6, _BIT6);

		/* Set DPRX14_PLL_PLL_SEL ip_oclk_sel for YUV420 */
		dprx->rbus_ops->mask_write(DPRX14_PLL_PLL_SEL,
			DPRX14_PLL_PLL_SEL_ip_oclk_sel_mask,
			DPRX14_PLL_PLL_SEL_ip_oclk_sel(1));

		/* Set Pll Target Clock using original PixelClockHz (before 0.07% reduction) */
		target_clk_hz = link_info->PixelClockHz * 2;
	} else {
		/* Disable YUV420 Output Clock */
		dprx->rbus_ops->set_bit(PB5_A2_PLL_OUT_CONTROL, ~_BIT6, 0x00);

		dprx->rbus_ops->mask_write(DPRX14_PLL_PLL_SEL,
			DPRX14_PLL_PLL_SEL_ip_oclk_sel_mask,
			DPRX14_PLL_PLL_SEL_ip_oclk_sel(0));

		/* Set Pll Target Clock using original PixelClockHz (before 0.07% reduction) */
		target_clk_hz = link_info->PixelClockHz;
	}

	dev_info(dprx->dev, "DP MAC RX0: PLL Out Clk=%u\n", target_clk_hz);

	/* PLL Output Divider Setting */
	if ((color_space == _COLOR_SPACE_YCBCR420) &&
	   (link_info->StreamClockHz < speed_limit)) {
		/* sclk2x = sclk4x/2, sclk = sclk2x/2 */
		dprx->rbus_ops->set_bit(PB5_A2_PLL_OUT_CONTROL,
			~(_BIT5 | _BIT4 | _BIT3), (_BIT5 | _BIT4));
	} else {
		/* sclk2x = sclk4x, sclk = sclk2x/2 */
		dprx->rbus_ops->set_bit(PB5_A2_PLL_OUT_CONTROL,
			~(_BIT5 | _BIT4 | _BIT3), _BIT4);
	}

	if (dprx->audio_support)
		dprx->audio_ops->initial(dprx);

	/* PLL VCO Setting - Use DPRXPLL */
	ret = rtk_dprx_set_dprxpll_freq_nf(dprx, target_clk_hz, input_clk_hz);
	if (ret)
		goto exit;

	ret = DPRX_NO_ERR;

exit:
	return ret;
}

/**
 * rtk_dprx_tracking_setting - DP NF PLL Tracking Enable
 */
static bool rtk_dprx_tracking_setting(struct rtk_dprx *dprx,
	struct rtk_dprx_stream_info *stream_info)
{
	u32 line_number = 0;
	u32 delay_cnt = 0;
	u32 temp = 0;
	u32 traking_frames = 0;
	u32 one_frame_ms = 0;
	bool first_pe_converged = false;

	/* Reset tracking_disabled flag for this scan attempt */
	dprx->mac_dat.tracking_disabled = false;

	/* Disable Tracking */
	dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN, ~(_BIT7 | _BIT0), 0x00);

	/* HS Tracking Setting */

	/* Set HS Tracking */
	dprx->rbus_ops->set_bit(PB5_E1_VS_TRACK_MODE, ~_BIT7, _BIT7);

	/* Avoid Too Large PE When Stream Clock is Smaller than 20MHz */
	if (stream_info->link_info.StreamClockHz < 20000000) {
		/* PE Clock = VCO Clock div 8 */
		dprx->rbus_ops->set_byte(PB5_DA_MN_TRACKING_DIVS, 0x04);
	} else {
		/* PE Clock = VCO Clock div 4 */
		dprx->rbus_ops->set_byte(PB5_DA_MN_TRACKING_DIVS, 0x02);
	}

	/* First Set Tracking Period with every line Tracking */
	dprx->rbus_ops->set_byte(PB5_E3_VS_TRACK1, 0x00);

	/* DE Only Mode */
	if (dprx->rbus_ops->get_bit(PB5_30_DPF_CTRL_0, (_BIT5 | _BIT4)) == _BIT4) {
		/* Get Half BS to BS delay (Counted by GDI_CLK) = HBs2BsCount / (1/2 * Link_clk) * GDI_clk / 2 */
		delay_cnt = rtk_dprx_compute_mul_div(dprx, (u32)stream_info->link_info.HBsToBsCount,
			(u32)_GDIPHY_RX_GDI_CLK_KHZ, stream_info->link_info.LinkClockHz / 1000);
	} else {
		/* Get Half BS to BS delay (Counted by GDI_CLK) = HTotal / 2 * GDI_CLK(27MHz) / Stream Clock */
		delay_cnt = rtk_dprx_compute_mul_div(dprx, (u32)stream_info->timing_info.HTotal,
			(u32)_GDIPHY_RX_GDI_CLK_KHZ, stream_info->link_info.StreamClockHz / 500);
	}

	/* Set Half BS to BS delay For Precision Mode */
	dprx->rbus_ops->set_bit(PB5_EA_HS_TRACKING_NEW_MODE1,
		~(_BIT3 | _BIT2 | _BIT1 | _BIT0), HIBYTE(delay_cnt));
	dprx->rbus_ops->set_byte(PB5_EB_HS_TRACKING_NEW_MODE2, LOBYTE(delay_cnt));

	dev_info(dprx->dev, "DP MAC RX0: Half BS2BS %u\n", delay_cnt);

	/* Disable manual Htotal */
	dprx->rbus_ops->set_bit(PB5_F2_DSC_HTT_0, ~_BIT7, 0x00);

	/* Only Enable Tracking at Active Region */
	rtk_dprx_hs_active_tracking_mode(dprx, _DP_HS_TRACKING_FW_MODE);

	/* Get 1 Frame Time, unit is ms */
	one_frame_ms = stream_info->link_info.VBsToBsCountN / _GDIPHY_RX_GDI_CLK_KHZ + 1;
	if (one_frame_ms > _DP_ONE_FRAME_TIME_MAX)
		one_frame_ms = _DP_ONE_FRAME_TIME_MAX;

	dev_info(dprx->dev, "DP MAC RX0: Loop 1 Frame Time %u ms", one_frame_ms);

	/* 1st Tracking Fast Lock Mode */

	/* I Gain Clamp = 0x00 01 00 00 */

	/* P Gain Clamp = 0x00 10 00 00 */
	rtk_dprx_set_pll_p_gain_clamp(dprx, 0x00, 0x10, 0x00);

	/* P Code = 0x0 00 FF FF */
	dprx->rbus_ops->set_bit(PB5_C9_MN_PI_CODE_1,
		~(_BIT2 | _BIT1 | _BIT0), 0x00);
	dprx->rbus_ops->set_byte(PB5_CA_MN_PI_CODE_2, 0x00);
	dprx->rbus_ops->set_byte(PB5_CB_MN_PI_CODE_3, 0xFF);
	dprx->rbus_ops->set_byte(PB5_CC_MN_PI_CODE_4, 0xFF);

	/* I Code = 0x08 */
	dprx->rbus_ops->set_byte(PB5_C8_MN_PI_CODE_0, 0x08);

	/* DE Only Mode */
	if (dprx->rbus_ops->get_bit(PB5_30_DPF_CTRL_0, (_BIT5 | _BIT4)) == _BIT4)
		traking_frames = 5;
	else
		traking_frames = 3;

	/* Set PE Nonlock Threshold */
	dprx->rbus_ops->set_byte(PB5_E4_VS_TRACK2, 0x3F);

	/* Set Fast Lock Mode */
	dprx->rbus_ops->set_bit(PB5_E1_VS_TRACK_MODE,
		~(_BIT3 | _BIT2 | _BIT1), _BIT3);

	/* Enable Tracking */
	dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN,
		~(_BIT7 | _BIT0), _BIT7);

	/* Waiting for Stream Clk Stable */
	temp = 0;
	while (temp < 10) {
		/* Clear PE Max Record */
		dprx->rbus_ops->set_byte(PB5_E5_VS_TRACK3, 0xFF);

		/* Clear PE Flag */
		dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN, ~_BIT0, _BIT0);

		/* Delay 1 Frame */
		usleep_range(one_frame_ms*1000, one_frame_ms*1000 + 100);

		temp++;

		/* Fast lock mode tracking at least 2 Frames */
		if ((temp >= traking_frames) &&
			((dprx->rbus_ops->get_bit(PB5_E0_VS_TRACK_EN, _BIT0) == 0x00))) {
			break;
		}
	}

	dev_info(dprx->dev, "DP MAC RX0: 1st PE Loop %u\n", temp);
	dev_info(dprx->dev, "DP MAC RX0: 1st PE Max Record %u\n",
		dprx->rbus_ops->get_byte(PB5_E5_VS_TRACK3));

	/* Check if 1st PE (Fast Lock) converged */
	first_pe_converged = (temp < 10) &&
		(dprx->rbus_ops->get_bit(PB5_E0_VS_TRACK_EN, _BIT0) == 0x00);

	/*
	 * If the tracking hardware is completely non-functional (PE Max Record
	 * stuck at the reset value 0xFF), skip precision mode entirely and
	 * proceed without NCO tracking. This occurs at very low pixel clocks
	 * (e.g., 25.2MHz for 640x480p60) where the tracking loop cannot operate.
	 */
	if (!first_pe_converged &&
		dprx->rbus_ops->get_byte(PB5_E5_VS_TRACK3) == 0xFF) {
		dev_info(dprx->dev,
			"DP MAC RX0: Tracking hardware non-functional at %uHz, proceeding without NCO tracking\n",
			stream_info->link_info.StreamClockHz);
		dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN,
			~(_BIT7 | _BIT0), 0x00);
		dprx->mac_dat.tracking_disabled = true;

		/*
		 * Without tracking, PLL free-runs at VBs2Bs-derived frequency
		 * which may have measurement error (GDI_CLK tolerance).
		 * Recalculate PLL target using Mvid/Nvid ratio for better
		 * accuracy, reducing FIFO overflow risk.
		 */
		if (stream_info->link_info.Mvid && stream_info->link_info.Nvid) {
			u32 mvid_stream_clk;

			mvid_stream_clk = rtk_dprx_compute_mul_div(dprx,
				stream_info->link_info.Mvid,
				stream_info->link_info.LinkClockHz,
				stream_info->link_info.Nvid);

			dev_info(dprx->dev,
				"DP MAC RX0: Recalculating PLL from Mvid/Nvid: %uHz (was %uHz)\n",
				mvid_stream_clk, stream_info->link_info.PixelClockHz);

			stream_info->link_info.PixelClockHz = mvid_stream_clk;
			stream_info->link_info.StreamClockHz =
				rtk_dprx_compute_mul_div(dprx,
					mvid_stream_clk, 9993, 10000);

			rtk_dprx_stream_clk_regenerate(dprx,
				&stream_info->link_info);
			rtk_dprx_set_bs_to_vs_delay(dprx, stream_info);
		}

		goto skip_precision;
	}

	/* Disable Tracking */
	dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN, ~(_BIT7 | _BIT0), 0x00);

	/* 2nd Tracking Precision Mode */

	/* I Gain Clamp = 0x00 10 00 00 */

	/* P Gain Clamp = 0x00 10 00 00 */
	rtk_dprx_set_pll_p_gain_clamp(dprx, 0x00, 0x10, 0x00);

	/* P Code = 0x0 00 FF FF */
	dprx->rbus_ops->set_bit(PB5_C9_MN_PI_CODE_1,
		~(_BIT2 | _BIT1 | _BIT0), 0x00);
	dprx->rbus_ops->set_byte(PB5_CA_MN_PI_CODE_2, 0x00);
	dprx->rbus_ops->set_byte(PB5_CB_MN_PI_CODE_3, 0xFF);
	dprx->rbus_ops->set_byte(PB5_CC_MN_PI_CODE_4, 0xFF);

	/* I Code = 0x04 */
	dprx->rbus_ops->set_byte(PB5_C8_MN_PI_CODE_0, 0x04);

	/* Set PE Nonlock Threshold */
	dprx->rbus_ops->set_byte(PB5_E4_VS_TRACK2, 0x1F);

	/* Enable Precision Mode */
	dprx->rbus_ops->set_bit(PB5_E1_VS_TRACK_MODE, ~(_BIT3 | _BIT2 | _BIT1), _BIT2);

	/* Enable Tracking */
	dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN, ~(_BIT7 | _BIT0), _BIT7);

	/* Waiting for Stream Clk Stable */
	temp = 0;
	do {
		/* Clear PE Max Record */
		dprx->rbus_ops->set_byte(PB5_E5_VS_TRACK3, 0xFF);

		/* Clear PE Flag */
		dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN, ~_BIT0, _BIT0);

		/* Delay 1 Frame */
		msleep(one_frame_ms);

		temp++;
	} while ((dprx->rbus_ops->get_bit(PB5_E0_VS_TRACK_EN, _BIT0) == _BIT0) && (temp < 10));

	dev_info(dprx->dev, "DP MAC RX0: 2nd PE Loop %u\n", temp);
	dev_info(dprx->dev, "DP MAC RX0: 2nd PE Max Record %u\n",
		dprx->rbus_ops->get_byte(PB5_E5_VS_TRACK3));

	/*
	 * PE convergence check for 2nd loop.
	 * If precision mode fails but fast lock succeeded, fall back to
	 * fast lock mode - the PLL is locked well enough for display.
	 * Some PLL N/F configurations (large fractional code) produce
	 * excessive phase noise that precision mode cannot tolerate.
	 */
	if ((temp >= 10) &&
		(dprx->rbus_ops->get_bit(PB5_E0_VS_TRACK_EN, _BIT0) == _BIT0)) {
		if (first_pe_converged) {
			dev_info(dprx->dev,
				"DP MAC RX0: Precision mode failed, falling back to fast lock mode\n");

			/* Disable Tracking */
			dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN,
				~(_BIT7 | _BIT0), 0x00);

			/* Restore Fast Lock parameters */
			dprx->rbus_ops->set_byte(PB5_C8_MN_PI_CODE_0, 0x08);
			dprx->rbus_ops->set_byte(PB5_E4_VS_TRACK2, 0x3F);
			dprx->rbus_ops->set_bit(PB5_E1_VS_TRACK_MODE,
				~(_BIT3 | _BIT2 | _BIT1), _BIT3);

			/* Re-enable Tracking in Fast Lock mode */
			dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN,
				~(_BIT7 | _BIT0), _BIT7);

			/* Wait for re-lock */
			msleep(one_frame_ms);
		} else {
			dev_err(dprx->dev,
				"DP MAC RX0: PE tracking failed to converge (loop=%u, StreamClk=%uHz)\n",
				temp, stream_info->link_info.StreamClockHz);
			dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN,
				~(_BIT7 | _BIT0), 0x00);
			return false;
		}
	}

skip_precision:
	/* Get How Many Lines Per SSC Period */
	/* SSC(33khz)'s time to update */
	line_number = (stream_info->link_info.StreamClockHz / stream_info->timing_info.HTotal + 11500) / 33000;

	if (line_number > 0)
		line_number--;

	/* Set Tracking Period */
	dprx->rbus_ops->set_byte(PB5_E3_VS_TRACK1, line_number);

	dev_info(dprx->dev, "DP MAC RX0: Tracking Per Line %u\n",
		dprx->rbus_ops->get_byte(PB5_E3_VS_TRACK1));

	/* DE Only Mode */
	if (dprx->rbus_ops->get_bit(PB5_30_DPF_CTRL_0, (_BIT5 | _BIT4)) == _BIT4) {
		/* Disable Tracking */
		dprx->rbus_ops->set_bit(PB5_E0_VS_TRACK_EN, ~(_BIT7 | _BIT0), 0x00);
	}

	/* Display Format Generator Enable */

	/* Start Generate Display Format */
	dprx->rbus_ops->set_bit(PB5_30_DPF_CTRL_0, ~_BIT7, _BIT7);

	/* Polling Vertical BS */
	rtk_dprx_wait_flag(dprx, _DP_one_frame_ms_MAX, PB6_01_DP_VBID, _BIT0);

	/* Delay 1 Frame Time for PG Stable */
	msleep(one_frame_ms);

	return true;
}

/**
 * rtk_dprx_pll_input_clock_setting - DP NF PLL Input Clock Setting
 */
static int rtk_dprx_pll_input_clock_setting(struct rtk_dprx *dprx,
	enum RTK_DP_LINK_RATE link_rate, u32 *input_clk_hz, u32 link_clk_hz)
{
	int ret = PLL_NF_INPUT_CLK_ERR;

	/*
	 * [7:6] PLL Input Clock(Fin) : Link_Clk(27MHz),
	 * [1] Reference Input Clock : pllin sel(Fin),
	 * [0] Tracking Sclk Clock : from local PLL
	 */
	dprx->rbus_ops->set_bit(PB5_A3_PLL_IN_CONTROL,
		~(_BIT7 | _BIT6 | _BIT1 | _BIT0), (_BIT6 | _BIT1));

	/* If Fin = Link CLock, Set Divdier for Link Clock */
	if (dprx->rbus_ops->get_bit(PB5_A3_PLL_IN_CONTROL, (_BIT7 | _BIT6)) == _BIT6) {
		switch (link_rate) {
		case _DP_LINK_HBR3:
			/* Link Rate : 810MHz */
			dprx->rbus_ops->set_bit(PB5_A5_M2PLL_DIVIDER1, ~(_BIT6 | _BIT5), (_BIT6 | _BIT5));

			/* Set Pll Input Clock, Unit is Hz */
			*input_clk_hz = link_clk_hz / 30;
			break;

		case _DP_LINK_HBR2:
			/* Link Rate : 540MHz */
			dprx->rbus_ops->set_bit(PB5_A5_M2PLL_DIVIDER1, ~(_BIT6 | _BIT5), _BIT6);

			/* Set Pll Input Clock, Unit is Hz */
			*input_clk_hz = link_clk_hz / 20;
			break;

		case _DP_LINK_HBR:
			/* Link Rate : 270MHz */
			dprx->rbus_ops->set_bit(PB5_A5_M2PLL_DIVIDER1, ~(_BIT6 | _BIT5), _BIT5);

			/* Set Pll Input Clock, Unit is Hz */
			*input_clk_hz = link_clk_hz / 10;
			break;

		case _DP_LINK_RBR:
			/* Link Rate : 162MHz */
			dprx->rbus_ops->set_bit(PB5_A5_M2PLL_DIVIDER1, ~(_BIT6 | _BIT5), 0x00);

			/* Set Pll Input Clock, Unit is Hz */
			*input_clk_hz = link_clk_hz / 6;
			break;

		default:
			goto exit;
		}
	} else {
		/* Unit is Hz */
		*input_clk_hz = _GDIPHY_RX_GDI_CLK_KHZ * 1000;
	}

	ret = DPRX_NO_ERR;

exit:
	return ret;
}

/**
 * rtk_dprx_calc_pll_params - Calculate PLL parameters from target frequency
 * @target_freq_hz: Target frequency in Hz
 * @prediv: Output prediv value (0 or 1)
 * @output_div: Output divider (1, 2, 4, 8)
 * @divs: Digital divider (0, 1, 3)
 * @n_code: Output N code
 * @f_code: Output F code (20-bit)
 *
 * Formula: F_out = 27 * (N + 4 + (F / 2^20)) / divrate / output_div / (divs + 1)
 * where divrate = prediv + 2
 *
 * Return: 0 on success, negative error code on failure
 */
static int rtk_dprx_calc_pll_params(u32 target_freq_hz,
	u8 *prediv, u8 *output_div, u8 *divs,
	u8 *n_code, u32 *f_code)
{
	u32 target_freq_khz = target_freq_hz / 1000;
	u64 pll_target, vco_freq;
	u64 n_float_scaled;
	/* Prefer smaller output_div to match CSV reference */
	u8 div_list[] = {1, 2, 4, 8};
	/* Digital divider: /1, /2, /4 */
	u8 divs_list[] = {0, 1, 3};
	int i, j;
	s32 n_temp;

	/* Try all divs combinations */
	for (j = 0; j < 3; j++) {
		u8 ds = divs_list[j];

		/* Calculate pll_target (kHz) */
		pll_target = (u64)target_freq_khz * (ds + 1);

		/* Try all output_divider values (prefer smaller div) */
		for (i = 0; i < 4; i++) {
			u8 div = div_list[i];
			u8 pred;

			/* Try prediv = 0 first (preferred), then 1 */
			for (pred = 0; pred <= 1; pred++) {
				u8 divrate = pred + 2;

				/* Calculate VCO frequency (kHz) */
				vco_freq = pll_target * div;

				/* Check VCO >= 300 MHz */
				if (vco_freq < 300000)
					continue;

				/* Calculate N using fixed-point (scaled by 2^20)
				 * N_float = (vco_freq * divrate) / 27000 - 4
				 */
				n_float_scaled = (vco_freq * divrate * (1ULL << 20)) / 27000;
				n_temp = (s32)((n_float_scaled >> 20) - 4);

				/* Check N range (N can be 0, see CSV 32.060 MHz config) */
				if (n_temp >= 0 && n_temp <= 127) {
					/* Calculate F */
					u64 n_plus_4 = (u64)(n_temp + 4) << 20;
					*f_code = (u32)(n_float_scaled - n_plus_4);

					/* Validate F range (20-bit) */
					if (*f_code <= 0xFFFFF) {
						*n_code = (u8)n_temp;
						*prediv = pred;
						*output_div = div;
						*divs = ds;
						return 0;
					}
				}
			}
		}
	}

	return PLL_NF_CALC_ERR;
}

/**
 * rtk_dprx_dprxpll_power_cycle - DPRXPLL power cycle sequence
 * @dprx: DPRX device
 *
 * Power cycle the DPRXPLL for N/F code update.
 */
static int rtk_dprx_dprxpll_power_cycle(struct rtk_dprx *dprx)
{
	/* Step 1: OEB=1, POW=0 */
	dprx->rbus_ops->mask_write(DPRX14_PLL_VID_1,
		DPRX14_PLL_VID_1_REG_DPRXPLL_OEB_mask |
		DPRX14_PLL_VID_1_REG_DPRXPLL_POW_mask,
		DPRX14_PLL_VID_1_REG_DPRXPLL_OEB(1) |
		DPRX14_PLL_VID_1_REG_DPRXPLL_POW(0));

	/* Step 2: POW=1 */
	dprx->rbus_ops->mask_write(DPRX14_PLL_VID_1,
		DPRX14_PLL_VID_1_REG_DPRXPLL_POW_mask,
		DPRX14_PLL_VID_1_REG_DPRXPLL_POW(1));

	/* Step 3: OEB=0 */
	dprx->rbus_ops->mask_write(DPRX14_PLL_VID_1,
		DPRX14_PLL_VID_1_REG_DPRXPLL_OEB_mask,
		DPRX14_PLL_VID_1_REG_DPRXPLL_OEB(0));


	if (dprx->audio_support) {
		dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUDIO_PLL_CONTROL, 0x00000001);
		dprx->rbus_ops->mask_write(DPRX14_PLL_AUD_1,
			DPRX14_PLL_AUD_1_REG_DPRXPLL_OEB_mask |
			DPRX14_PLL_AUD_1_REG_DPRXPLL_POW_mask,
			DPRX14_PLL_AUD_1_REG_DPRXPLL_OEB(1) |
			DPRX14_PLL_AUD_1_REG_DPRXPLL_POW(0));
		dprx->rbus_ops->mask_write(DPRX14_PLL_AUD_1,
			DPRX14_PLL_AUD_1_REG_DPRXPLL_POW_mask,
			DPRX14_PLL_AUD_1_REG_DPRXPLL_POW(1));
		dprx->rbus_ops->mask_write(DPRX14_PLL_AUD_1,
			DPRX14_PLL_AUD_1_REG_DPRXPLL_OEB_mask,
			DPRX14_PLL_AUD_1_REG_DPRXPLL_OEB(0));
		dprx->rbus_ops->set_byte(DPRX14_MAC_IP_AUDIO_PLL_CONTROL, 0x00000000);
	}

	return DPRX_NO_ERR;
}

/**
 * rtk_dprx_dprxpll_write_nf - Write N/F code to DPRXPLL
 * @dprx: DPRX device
 * @n_code: N code (0-127)
 * @f_code: F code (20-bit)
 * @is_high_freq: true if target frequency >= 660 MHz
 *
 * Write N and F codes to PLL registers and trigger load.
 */
static int rtk_dprx_dprxpll_write_nf(struct rtk_dprx *dprx,
	u8 n_code, u32 f_code, bool is_high_freq)
{
	/* N code range 0~127, N=0 is valid for some low-freq configs (e.g. 32.060 MHz) */

	/* Set N code [7:0] */
	dprx->rbus_ops->set_byte(PB5_AD_N_F_CODE_1, n_code);

	/* Set F code [19:16] */
	dprx->rbus_ops->set_byte(PB5_AE_N_F_CODE_2, (f_code >> 16) & 0x0F);

	/* Set F code [15:8] */
	dprx->rbus_ops->set_byte(PB5_AF_N_F_CODE_3, (f_code >> 8) & 0xFF);

	/* Set F code [7:0] */
	dprx->rbus_ops->set_byte(PB5_B0_N_F_CODE_4, f_code & 0xFF);

	/* PLL HW limitation */
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_N_F_MIN,
		DPRX14_MAC_IP_N_F_MIN_min_n_code(0x11));
	dprx->rbus_ops->set_byte(DPRX14_MAC_IP_TRACKING_7,
		DPRX14_MAC_IP_TRACKING_7_pi_min_n(0x11));

	dev_dbg(dprx->dev, "PLL N/F: N=0x%02x, F=0x%05x\n", n_code, f_code);

	return DPRX_NO_ERR;
}

/**
 * rtk_dprx_pll_debug_dump - Dump PLL register values for debugging
 * @dprx: DPRX device
 */
static void __maybe_unused rtk_dprx_pll_debug_dump(struct rtk_dprx *dprx)
{
	u32 pll_vid_1, n_code, f_code;
	u8 prediv, output_div_code, divs;
	u64 vco_freq, calculated_freq;

	dprx->rbus_ops->read(DPRX14_PLL_VID_1, &pll_vid_1);
	output_div_code = (pll_vid_1 >> 25) & 0x7;
	prediv = (pll_vid_1 >> 8) & 0xFF;

	n_code = dprx->rbus_ops->get_byte(PB5_AD_N_F_CODE_1) & 0xFF;
	f_code = ((dprx->rbus_ops->get_byte(PB5_AE_N_F_CODE_2) & 0x0F) << 16) |
		 ((dprx->rbus_ops->get_byte(PB5_AF_N_F_CODE_3) & 0xFF) << 8) |
		 (dprx->rbus_ops->get_byte(PB5_B0_N_F_CODE_4) & 0xFF);

	divs = (dprx->rbus_ops->get_byte(PB5_A2_PLL_OUT_CONTROL) >> 1) & 0x3;

	dev_info(dprx->dev, "=== PLL Debug ===\n");
	dev_info(dprx->dev, "  PLL_VID_1=0x%08x\n", pll_vid_1);
	dev_info(dprx->dev, "  output_div_code=%u, prediv=%u, divs=%u\n",
		output_div_code, prediv, divs);
	dev_info(dprx->dev, "  N=%u, F=0x%05x (%u)\n", n_code, f_code, f_code);

	/* Calculate frequency for verification
	 * VCO = 27 * (N + 4 + F/2^20) / divrate
	 * F_out = VCO / output_div / (divs + 1)
	 */
	vco_freq = 27000ULL * (n_code + 4) + ((27000ULL * f_code) >> 20);
	vco_freq = vco_freq / (prediv + 2);
	calculated_freq = vco_freq / (1 << output_div_code) / (divs + 1);

	dev_info(dprx->dev, "  VCO=%llu kHz, Freq=%llu kHz\n",
		vco_freq, calculated_freq);
}

/**
 * rtk_dprx_set_dprxpll_freq_nf - Set PLL frequency using DPRXPLL
 * @dprx: DPRX device
 * @target_clk_hz: Target clock frequency in Hz
 * @input_clk_hz: Input clock frequency in Hz (unused, kept for API compatibility)
 *
 * Calculate and configure PLL parameters to generate the target frequency.
 * Uses dynamic calculation based on the implementation guide formula.
 */
static int rtk_dprx_set_dprxpll_freq_nf(struct rtk_dprx *dprx,
	u32 target_clk_hz, u32 input_clk_hz)
{
	int ret;
	u8 prediv, output_div, divs, n_code;
	u32 f_code;
	u8 output_div_code;
	bool is_high_freq = (target_clk_hz >= 660000000);

	/* 1. Calculate PLL parameters */
	ret = rtk_dprx_calc_pll_params(target_clk_hz,
		&prediv, &output_div, &divs, &n_code, &f_code);
	if (ret) {
		dev_err(dprx->dev, "Failed to calc PLL params for %u Hz\n",
			target_clk_hz);
		return ret;
	}

	dev_dbg(dprx->dev, "PLL: freq=%uHz, prediv=%u, output_div=%u, divs=%u, N=%u, F=%u\n",
		target_clk_hz, prediv, output_div, divs, n_code, f_code);

	/* 2. Configure DPRX14_PLL_VID_1 (Output Divider + Prediv)
	 * Output Divider encoding: /1=0, /2=1, /4=2, /8=3
	 */
	switch (output_div) {
	case 1:
		output_div_code = 0;
		break;
	case 2:
		output_div_code = 1;
		break;
	case 4:
		output_div_code = 2;
		break;
	case 8:
		output_div_code = 3;
		break;
	default:
		return PLL_NF_SET_ERR;
	}

	/* Configure output divider and prediv */
	dprx->rbus_ops->mask_write(DPRX14_PLL_VID_1,
		DPRX14_PLL_VID_1_REG_DPRXPLL_O_mask |
		DPRX14_PLL_VID_1_REG_DPRXPLL_SEL_PREDIV_mask,
		DPRX14_PLL_VID_1_REG_DPRXPLL_O(output_div_code) |
		DPRX14_PLL_VID_1_REG_DPRXPLL_SEL_PREDIV(prediv));

	/* 3. Configure digital divider (PB5_A2_PLL_OUT_CONTROL[2:1])
	 * divs=0 -> reg=0x0, divs=1 -> reg=0x2, divs=3 -> reg=0x6
	 */
	dprx->rbus_ops->set_bit(PB5_A2_PLL_OUT_CONTROL,
		~(_BIT2 | _BIT1), (divs << 1) & 0x6);

	/* 4. Write N/F codes */
	ret = rtk_dprx_dprxpll_write_nf(dprx, n_code, f_code, is_high_freq);
	if (ret)
		return ret;

	/* 5. Execute PLL power cycle sequence */
	ret = rtk_dprx_dprxpll_power_cycle(dprx);
	if (ret)
		return ret;

	dev_info(dprx->dev, "DP MAC RX0: DPRXPLL freq=%uHz, N=%u, F=0x%05x\n",
		target_clk_hz, n_code, f_code);

	return DPRX_NO_ERR;
}

/**
 * rtk_dprx_set_pll_p_gain_clamp - DP NF PLL P Gain Clamp Setting
 *
 * @gain_b3: p_gain_clamp_value[26:24]
 * @gain_b2: p_gain_clamp_value[15:8]
 * @gain_b1: p_gain_clamp_value[7:0]
 */
static void rtk_dprx_set_pll_p_gain_clamp(struct rtk_dprx *dprx,
	u8 gain_b3, u8 gain_b2, u8 gain_b1)
{
	/* 1st tracking : 0x0 10 00 00 */
	/* 2nd tracking : 0x0 01 00 00 */
	dprx->rbus_ops->set_byte(PB5_D1_MN_PI_CODE_9, gain_b3);
	dprx->rbus_ops->set_byte(PB5_D2_MN_PI_CODE_A, gain_b2);
	dprx->rbus_ops->set_byte(PB5_D3_MN_PI_CODE_B, gain_b1);
	dprx->rbus_ops->set_byte(PB5_D4_MN_PI_CODE_C, 0x00);
}

/**
 * rtk_dprx_pll_p_code_spread_ctrl - DP NF PLL P Code Spread Control Setting
 */
static void __maybe_unused rtk_dprx_pll_p_code_spread_ctrl(struct rtk_dprx *dprx,
	bool en_spread)
{
	if (en_spread) {
		u32 p_gain = dprx->rbus_ops->get_dword(PB5_D1_MN_PI_CODE_9);

		/* Multiple P_Gain_Clamp */
		p_gain <<= 4;

		/* set New P_Gain_Clamp */
		dprx->rbus_ops->set_bit(PB5_D1_MN_PI_CODE_9,
			~(_BIT2 | _BIT1 | _BIT0),
			((u8)(p_gain >> 24) & (_BIT2 | _BIT1 | _BIT0)));
		dprx->rbus_ops->set_byte(PB5_D2_MN_PI_CODE_A, (u8)(p_gain >> 16));
		dprx->rbus_ops->set_byte(PB5_D3_MN_PI_CODE_B, (u8)(p_gain >> 8));
		dprx->rbus_ops->set_byte(PB5_D4_MN_PI_CODE_C, (u8)(p_gain >> 0));

		dprx->rbus_ops->set_byte(PB5_F0_P_CODE_SPREAD_2, 0x06);
		dprx->rbus_ops->set_bit(PB5_EE_P_CODE_SPREAD_0, ~_BIT7, _BIT7);
	} else {
		dprx->rbus_ops->set_bit(PB5_EE_P_CODE_SPREAD_0, ~_BIT7, 0x00);
		dprx->rbus_ops->set_byte(PB5_F0_P_CODE_SPREAD_2, 0x00);
	}
}

/**
 * rtk_dprx_adjust_vsync_delay - DP Adjust Vsync Delay
 */
static void rtk_dprx_adjust_vsync_delay(struct rtk_dprx *dprx,
	struct rtk_dprx_stream_info *stream_info)
{
	u32 delay_delta = 0;
	u32 even_delay;
	u32 odd_delay;
	u32 val_h, val_m, val_l;
	u8 frame_ms = 0;

	if (dprx->rbus_ops->get_bit(PB5_21_PG_CTRL_1, (_BIT1 | _BIT0)) == 0x00) {
		/* Fifo Ok */
		return;
	}

	if (dprx->rbus_ops->get_bit(PB5_21_PG_CTRL_1, _BIT0) == _BIT0) {
		/* Fifo Overflow */
		/* 1 Line = HTotal * (1/2 Link Clock) / Stream Clock */
		delay_delta = rtk_dprx_compute_mul_div(dprx, (u32)stream_info->timing_info.HTotal,
			stream_info->link_info.LinkClockHz / 2, stream_info->link_info.StreamClockHz);

		dev_info(dprx->dev, "Fifo Overflow --> nVidia Case, delay_delta=%u\n", delay_delta);
	} else if (dprx->rbus_ops->get_bit(PB5_21_PG_CTRL_1, _BIT1) == _BIT1) {
		u32 dpf_hwd;

		/* Fifo Underflow */
		if (stream_info->timing_info.HWidth != 1366)
			return;

		dpf_hwd = stream_info->timing_info.HWidth - 2;

		/* Set HWidth */
		dprx->rbus_ops->set_byte(PB5_45_MN_DPF_HWD_M, HIBYTE(dpf_hwd));
		dprx->rbus_ops->set_byte(PB5_46_MN_DPF_HWD_L, LOBYTE(dpf_hwd));

		/* BS2BS Delta = 2 * (1/2 Link Clock) / Stream Clock */
		delay_delta = stream_info->link_info.LinkClockHz / stream_info->link_info.StreamClockHz;

		dev_info(dprx->dev, "Fifo Underflow --> QD882 Case, delay_delta=%u\n", delay_delta);
	}

	/* Get BStoVSDelay of Even Field */
	val_h = dprx->rbus_ops->get_byte(PB5_32_EVBLK2VS_H);
	val_m = dprx->rbus_ops->get_byte(PB5_33_EVBLK2VS_M);
	val_l = dprx->rbus_ops->get_byte(PB5_34_EVBLK2VS_L);
	even_delay = (val_h << 16) | (val_m << 8) | val_l;

	/* Get BStoVSDelay of Odd Field */
	val_h = dprx->rbus_ops->get_byte(PB5_35_OVBLK2VS_H);
	val_m = dprx->rbus_ops->get_byte(PB5_36_OVBLK2VS_M);
	val_l = dprx->rbus_ops->get_byte(PB5_37_OVBLK2VS_L);
	odd_delay = (val_h << 16) | (val_m << 8) | val_l;

	dev_info(dprx->dev, "Vsync Delay Before: even=%u, odd=%u, delta=%u\n",
		 even_delay, odd_delay, delay_delta);

	if ((even_delay > delay_delta) && (odd_delay > delay_delta)) {
		/* BStoVSDelay of Even Field = BStoVSDelay of Even Field - BStoVSDelayDelta */
		even_delay -= delay_delta;

		/* BStoVSDelay of Odd Field = BStoVSDelay of Odd Field - BStoVSDelayDelta */
		odd_delay -= delay_delta;

		dev_info(dprx->dev, "Vsync Delay After:  even=%u, odd=%u (adjusted)\n",
			 even_delay, odd_delay);
	} else {
		dev_info(dprx->dev, "Vsync Delay: Cannot adjust (even=%u, odd=%u < delta=%u)\n",
			 even_delay, odd_delay, delay_delta);
	}

	/* Set Even Field BS to VS Delay */
	dprx->rbus_ops->set_byte(PB5_32_EVBLK2VS_H, (even_delay >> 16) & 0xFF);
	dprx->rbus_ops->set_byte(PB5_33_EVBLK2VS_M, (even_delay >> 8) & 0xFF);
	dprx->rbus_ops->set_byte(PB5_34_EVBLK2VS_L, (even_delay >> 0) & 0xFF);

	/* Set Odd Field BS to VS Delay */
	dprx->rbus_ops->set_byte(PB5_35_OVBLK2VS_H, (odd_delay >> 16) & 0xFF);
	dprx->rbus_ops->set_byte(PB5_36_OVBLK2VS_M, (odd_delay >> 8) & 0xFF);
	dprx->rbus_ops->set_byte(PB5_37_OVBLK2VS_L, (odd_delay >> 0) & 0xFF);

	/* Reset Display Format Gen */
	dprx->rbus_ops->set_bit(PB5_30_DPF_CTRL_0, ~_BIT7, 0x00);
	dprx->rbus_ops->set_bit(PB5_30_DPF_CTRL_0, ~_BIT7, _BIT7);

	/* Polling Vertical BS */
	rtk_dprx_wait_flag(dprx, 50, PB6_01_DP_VBID, _BIT0);

	/* Get 1 Frame Time, unit is ms */
	frame_ms = stream_info->link_info.VBsToBsCountN / _GDIPHY_RX_GDI_CLK_KHZ + 1;

	/* Delay 1 Frame Time for PG Stable */
	msleep(frame_ms);

	/* Clear FIFO overflow/underflow flags after delay adjustment */
	dprx->rbus_ops->set_bit(PB5_21_PG_CTRL_1,
				~(_BIT4 | _BIT2 | _BIT1 | _BIT0), 0x00);
}


/**
 * rtk_dprx_calculate_crc - DP CRC Calculate
 */
static int rtk_dprx_calculate_crc(struct rtk_dprx *dprx)
{
	int ret = CRC_NOT_SUPPORT;
	u8 crc0, crc1, crc2, crc3, crc4, crc5;

	if (!dprx->mac_dat.en_crc_cal)
		goto exit;

	if (dprx->mac_dat.color_space == _COLOR_SPACE_YCBCR422)
		dprx->rbus_ops->set_bit(PB5_70_DP_CRC_CTRL, ~(_BIT5 | _BIT4), _BIT4);
	else
		dprx->rbus_ops->set_bit(PB5_70_DP_CRC_CTRL, ~(_BIT5 | _BIT4), 0x00);

	/* Start CRC Calculation */
	dprx->rbus_ops->set_bit(PB5_70_DP_CRC_CTRL, ~(_BIT7 | _BIT5), _BIT7);

	ret = rtk_dprx_wait_flag(dprx, _DP_MEASURE_POLLING_TIMEOUT, PB5_70_DP_CRC_CTRL, _BIT6);
	if (ret)
		goto exit;

	ret = rtk_dprx_fifo_check(dprx, _DP_FIFO_POLLING_CHECK);
	if (ret)
		goto exit;

	crc0 = dprx->rbus_ops->get_byte(PB5_72_DP_CRC_R_L);
	crc1 = dprx->rbus_ops->get_byte(PB5_71_DP_CRC_R_M);
	crc2 = dprx->rbus_ops->get_byte(PB5_74_DP_CRC_G_L);
	crc3 = dprx->rbus_ops->get_byte(PB5_73_DP_CRC_G_M);
	crc4 = dprx->rbus_ops->get_byte(PB5_76_DP_CRC_B_L);
	crc5 = dprx->rbus_ops->get_byte(PB5_75_DP_CRC_B_M);

	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x40, crc0);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x41, crc1);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x42, crc2);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x43, crc3);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x44, crc4);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x45, crc5);

	dprx->mac_dat.en_crc_cal = false;

	/* Update _TEST_CRC_COUNT */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x46, 0x21);

	ret = DPRX_NO_ERR;
exit:
	return ret;
}

/**
 * rtk_dprx_hs_active_tracking_mode - DP Hs Tracking Active Region Setting
 */
static void rtk_dprx_hs_active_tracking_mode(struct rtk_dprx *dprx,
	enum RTK_DP_HS_TRACKING_TYPE mode)
{
	if (mode == _DP_HS_TRACKING_FW_MODE) {
		/* HS Tracking Region By FW Setting, Pick BE as hsync tracking */
		dprx->rbus_ops->set_bit(PB5_EA_HS_TRACKING_NEW_MODE1,
			~(_BIT5 | _BIT4), (_BIT5 | _BIT4));

		/* BE Start Num = 2 line */
		dprx->rbus_ops->set_bit(PB5_EC_VBID_MAN_MADE,
			~(_BIT7 | _BIT6 | _BIT5 | _BIT4), _BIT5);

		/* BE End Num = 2 line */
		dprx->rbus_ops->set_bit(PB5_EC_VBID_MAN_MADE,
			~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT1);
	} else {
		/* HS Tracking Region By FW Setting, Pick BE as hsync tracking */
		dprx->rbus_ops->set_bit(PB5_EA_HS_TRACKING_NEW_MODE1,
			~(_BIT5 | _BIT4), _BIT4);
	}
}

/**
 * rtk_dprx_hdcp14_reset_proc - HDCP 1.4 Reset Proc for MAC RX0
 */
static void __maybe_unused rtk_dprx_hdcp14_reset_proc(struct rtk_dprx *dprx)
{
	/* Reset HDCP Block */
	dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT0, _BIT0);
	dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT0, 0x00);
}

/**
 * rtk_dprx_hdcp_unplug_reset - HDCP Unplug Reset
 */
static void __maybe_unused rtk_dprx_hdcp_unplug_reset(struct rtk_dprx *dprx)
{

	// TODO: ScalerDpHdcp14RxResetProc(enumInputPort);

	/* Clear CPIRQ flag while unplug */
	dprx->aux_ops->set_dpcd_write1_clear_value(dprx, 0x00, 0x02, 0x01,
		(dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x02, 0x01) & ~_BIT2));
}

/**
 * rtk_dprx_check_hdcp_cp_irq_status - Clear CPIRQ Flag
 */
static void rtk_dprx_check_hdcp_cp_irq_status(struct rtk_dprx *dprx)
{
	if ((dprx->rbus_ops->get_byte(PB_1E_HDCP_INTGT_VRF_ANS_MSB) == 0x53) &&
		(dprx->rbus_ops->get_byte(PB_1F_HDCP_INTGT_VRF_ANS_LSB) == 0x1F)) {

		if ((dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x02, 0x01) & _BIT2) == _BIT2) {

			if (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x29) == 0x00) {
				/* Clear Link Status CPIRQ Flag */
				dprx->aux_ops->set_dpcd_write1_clear_value(dprx, 0x00, 0x02, 0x01,
					(dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x02, 0x01) & ~_BIT2));
			}
		}
	}
}

/**
 * rtk_dprx_hdcp_check - Check DP Link Integrity
 *
 * @return: false - DP Link Integrity Fail
 */
static bool rtk_dprx_hdcp_check(struct rtk_dprx *dprx)
{
	/* Check for HDCP Block work in 1.4 */
	if (ScalerDpHdcpRxGetHdcpMode() == _HDCP_14) {
		if ((dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x07) == 0x00) &&
		   (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x08) == 0x00) &&
		   (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x09) == 0x00) &&
		   (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x0A) == 0x00) &&
		   (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x0B) == 0x00)) {
			return true;
		}
	}

	if (((dprx->rbus_ops->get_byte(PB_1E_HDCP_INTGT_VRF_ANS_MSB) == 0x53) ||
		(dprx->rbus_ops->get_byte(PB_1E_HDCP_INTGT_VRF_ANS_MSB) == 0x00)) &&
		((dprx->rbus_ops->get_byte(PB_1F_HDCP_INTGT_VRF_ANS_LSB) == 0x1F) ||
		(dprx->rbus_ops->get_byte(PB_1F_HDCP_INTGT_VRF_ANS_LSB) == 0x00))) {
		return true;
	}

	/* Check for HDCP Block work in 1.4 */
	if (ScalerDpHdcpRxGetHdcpMode() == _HDCP_14) {
		if ((dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x11) & _BIT2) == 0x00)
			return false;

		if (ScalerDpMacRx0HdcpMeasureCheck() == true)
			return true;
	}

	return false;
}

/**
 * rtk_dprx_hdcp_reauth_status_check - Check DP Hdcp ReAuth Process
 */
static bool rtk_dprx_hdcp_reauth_status_check(struct rtk_dprx *dprx)
{
	if ((dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x07) != 0x00) ||
	   (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x08) != 0x00) ||
	   (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x09) != 0x00) ||
	   (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x0A) != 0x00) ||
	   (dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x0B) != 0x00)) {
		return true;
	}

	return false;
}

/**
 * rtk_dprx_hdcp_measure_check - DP HDCP Measure Check
 */
static int __maybe_unused rtk_dprx_hdcp_measure_check(struct rtk_dprx *dprx)
{
	int ret;
	u32 v_total = 0;
	u32 link_clk = 0;
	u32 h_total_cnt = 0;
	u32 data1[4] = {0};
	u32 data2[2] = {0};
	u32 val_20_16;
	u32 val_15_8;
	u32 val_7_0;
	u8 lane_sel;

	/* Pop up Main Stream Attributes */
	dprx->rbus_ops->set_bit(PB6_00_MN_STRM_ATTR_CTRL,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT0),
		(_BIT7 | _BIT6 | _BIT5 | _BIT3));

	/* Get Vtotal */
	v_total = dprx->rbus_ops->get_word(PB6_10_MSA_VTTE_0);

	if (v_total == 0) {
		ret = HDCP_CHECK_ZERO_VTOTAL;
		goto exit;
	}

	if (dprx->mac_dat.en_free_sync) {
		ret = HDCP_CHECK_IN_FREE_SYNC;
		goto exit;
	}

	lane_sel = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
	data1[0] = rtk_dprx_signal_detect_measure_count(dprx, lane_sel,
		_DP_MEASURE_TARGET_CDR_CLOCK, _DP_MEASURE_PERIOD_2000_CYCLE);

	if (data1[0] == 0) {
		switch (dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00)) {
		case _DP_LINK_HBR3:
			data1[0] = _DP_RX_VCO_TARGET_COUNT_2000_HBR3_SAVED;
			break;

		case _DP_LINK_HBR2:
			data1[0] = _DP_RX_VCO_TARGET_COUNT_2000_HBR2_SAVED;
			break;

		case _DP_LINK_HBR:
			data1[0] = _DP_RX_VCO_TARGET_COUNT_2000_HBR_SAVED;
			break;

		case _DP_LINK_RBR:
			fallthrough;
		default:
			data1[0] = _DP_RX_VCO_TARGET_COUNT_2000_RBR_SAVED;

			break;
		}
	}

	data2[0] = (data1[0] * _GDIPHY_RX_GDI_CLK_KHZ);

	/* Link Clk in KHz */
	link_clk = (data2[0] / 1000);

	dev_info(dprx->dev, "DP MAC RX0: Current link_clk=%u\n", link_clk);

	/* Start to Measure Vertical BS to BS Counter by GDI Clock */
	dprx->rbus_ops->set_bit(PB5_58_MN_MEAS_CTRL, ~(_BIT7 | _BIT5), 0x00);
	dprx->rbus_ops->set_bit(PB5_58_MN_MEAS_CTRL, ~(_BIT7 | _BIT5), _BIT7);

	ret = rtk_dprx_wait_flag(dprx, _DP_MEASURE_POLLING_TIMEOUT, PB5_58_MN_MEAS_CTRL, _BIT6);
	if (ret)
		goto exit;

	/* Pop up The result */
	rtk_dprx_set_measure_pop_up(dprx);

	/* Get Measure Result */
	val_20_16 = dprx->rbus_ops->get_byte(PB5_58_MN_MEAS_CTRL) & 0x1F;
	val_15_8 = dprx->rbus_ops->get_byte(PB5_59_MN_MEAS_VLN_M);
	val_7_0 = dprx->rbus_ops->get_byte(PB5_5A_MN_MEAS_VLN_L);
	data2[0] = (val_20_16 << 16) | (val_15_8 << 8) | val_7_0;

	/* Get Measure Htotal Counts */
	data1[3] = dprx->rbus_ops->get_word(PB5_5B_MN_MEAS_HLN_M);

	/* Disable Measure Block */
	dprx->rbus_ops->set_bit(PB5_58_MN_MEAS_CTRL, ~(_BIT7 | _BIT5), 0x00);

	/* Convert HTotal Count from 1/2 Link Clk to GDI Clk */
	h_total_cnt = (data1[3] * _GDIPHY_RX_GDI_CLK_KHZ * 2 + (link_clk >> 1)) / link_clk;
	data2[1] = (h_total_cnt & 0x0000FFFF);

	data1[2] = data2[0] / data2[1];

	/* 2% Tolerance */
	if ((ABSDWORD(data1[2], v_total)) > (v_total * 2 / 100)) {
		ret = HDCP_CHECK_VTOTAL;
		goto exit;
	}

	ret = DPRX_NO_ERR;
exit:
	return ret;
}

/**
 * rtk_dprx_cp_irq - DP Content Protection Interrupt Request
 */
static void rtk_dprx_cp_irq(struct rtk_dprx *dprx,
	enum RTK_DP_RX_BSTATUS_TYPE status_type)
{
	u8 bstatus;
	u8 irq_vector;

	switch (status_type) {
	case _DP_HDCP_BSTATUS_LINK_INTEGRITY_FAIL:
		if (ScalerDpHdcpRxGetHdcpMode() == _HDCP_14) {
			/* Reset HDCP Block */
			dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT0, _BIT0);
			dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT0, 0x00);

			dprx->aux_ops->set_manual_mode(dprx);

			/* Clear Aksv */
			dprx->aux_ops->set_dpcd_write_value(dprx, 0x06, 0x80, 0x07, 0x00);
			dprx->aux_ops->set_dpcd_write_value(dprx, 0x06, 0x80, 0x08, 0x00);
			dprx->aux_ops->set_dpcd_write_value(dprx, 0x06, 0x80, 0x09, 0x00);
			dprx->aux_ops->set_dpcd_write_value(dprx, 0x06, 0x80, 0x0A, 0x00);
			dprx->aux_ops->set_dpcd_write_value(dprx, 0x06, 0x80, 0x0B, 0x00);

			dprx->aux_ops->set_auto_mode(dprx);

			/* Set B Status */
			bstatus = dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x29);
			dprx->aux_ops->set_dpcd_value(dprx, 0x06, 0x80, 0x29, (bstatus | status_type));
		}

		break;

	case _DP_HDCP_BSTATUS_REAUTH_REQ:
		bstatus = dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x29);
		dprx->aux_ops->set_dpcd_value(dprx, 0x06, 0x80, 0x29, (bstatus | _BIT3));
		break;

	case _DP_HDCP_BSTATUS_V_READY:
		fallthrough;
	case _DP_HDCP_BSTATUS_R0_AVAILABLE:
		/* Set B Status */
		bstatus = dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x29);
		dprx->aux_ops->set_dpcd_value(dprx, 0x06, 0x80, 0x29, (bstatus | status_type));
		break;

	default:
		break;
	}

	/* Link Status CPIRQ Flag */
	irq_vector = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x02, 0x01);
	dprx->aux_ops->set_dpcd_write1_clear_value(dprx, 0x00, 0x02, 0x01, (irq_vector | _BIT2));

	ScalerDpAuxRxHpdIrqAssert();
}

/**
 * rtk_dprx_hdcp_check_valid - Check whether HDCP is valid
 */
static bool __maybe_unused rtk_dprx_hdcp_check_valid(struct rtk_dprx *dprx)
{
	if (ScalerDpHdcpRxGetHdcpMode() == _HDCP_14) {
		u8 hdcp_debug;

		hdcp_debug = dprx->rbus_ops->get_bit(PB_20_HDCP_DEBUG, (_BIT7 | _BIT6 | _BIT5));
		if (hdcp_debug == (_BIT7 | _BIT6 | _BIT5))
			return true;
	}

	return false;
}

/**
 * rtk_dprx_hdcp_check_enabled - Check whether HDCP is enabled
 */
static bool __maybe_unused rtk_dprx_hdcp_check_enabled(struct rtk_dprx *dprx)
{
	if (ScalerDpHdcpRxGetHdcpMode() == _HDCP_14) {
		u8 hdcp_debug;

		hdcp_debug = dprx->rbus_ops->get_bit(PB_20_HDCP_DEBUG, (_BIT7 | _BIT5));
		if (hdcp_debug == (_BIT7 | _BIT5)) {
			u8 aksv[5];

			aksv[0] = dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x07);
			aksv[1] = dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x08);
			aksv[2] = dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x09);
			aksv[3] = dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x0A);
			aksv[4] = dprx->aux_ops->get_dpcd_info(dprx, 0x06, 0x80, 0x0B);

			if ((aksv[0] != 0x00) || (aksv[1] != 0x00) || (aksv[2] != 0x00) ||
			   (aksv[3] != 0x00) || (aksv[4] != 0x00))
				return true;
		}
	}

	return false;
}

/**
 * rtk_dprx_power_data_recover - Recover Data from Power Cut State
 */
static void __maybe_unused rtk_dprx_power_data_recover(struct rtk_dprx *dprx)
{
	/* Set R0' Available HW Mode */
	dprx->rbus_ops->set_bit(PB_1A_HDCP_IRQ, ~(_BIT5 | _BIT4), _BIT5);

	/* Enable DP Link Integrity Enable */
	dprx->rbus_ops->set_bit(PB_1B_HDCP_INTGT_VRF, ~_BIT7, _BIT7);

	/* DP Mac Clock Select to Xtal Clock */
	dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT6, _BIT6);
}

/**
 * rtk_dprx_hdcp_down_load_key - Enable/Disable DownLoad HDCP Key
 */
static void __maybe_unused rtk_dprx_hdcp_down_load_key(struct rtk_dprx *dprx, bool enable)
{
	if (enable) {
		/* Enable HDCP Block and Key download port */
		dprx->rbus_ops->set_bit(PB_17_DP_HDCP_CONTROL, ~(_BIT7 | _BIT0), (_BIT7 | _BIT0));

		/* Set Km Clock to Xtal Clock */
		dprx->rbus_ops->set_bit(PB_1A_HDCP_IRQ, ~_BIT3, 0x00);
	} else {
		/* Disable Key download port */
		dprx->rbus_ops->set_bit(PB_17_DP_HDCP_CONTROL, ~_BIT0, 0x00);
	}
}

/**
 * rtk_dprx_hdcp_down_load_key_to_sram - DownLoad HDCP Key to SRAM
 */
static void __maybe_unused rtk_dprx_hdcp_down_load_key_to_sram(struct rtk_dprx *dprx,
	u32 length, u8 *read_array)
{
	// TODO: ScalerWrite(PB_18_DP_HDCP_DOWNLOAD_PORT, length, read_array, _NON_AUTOINC);
}

/**
 * rtk_dprx_hdcp_mode_restore - HDCP Mode Alignment between Variable & Register
 */
static void __maybe_unused rtk_dprx_hdcp_mode_restore(struct rtk_dprx *dprx)
{
	/* Enable HDCP MAC0 for HDCP 1.4 Mode */
	ScalerDpMacRx0SetHdcpMode(_HDCP_14);

	if (rtk_dprx_hdcp_reauth_status_check(dprx) == true) {
		/* HDCP R0 Calculate */
		dprx->rbus_ops->set_bit(PB_63_HDCP_OTHER, ~_BIT7, _BIT7);
		dprx->rbus_ops->set_bit(PB_63_HDCP_OTHER, ~_BIT7, 0x00);
	}
}

/**
 * rtk_dprx_assr_mode_setting - DP ASSR Mode On/Off
 */
static void __maybe_unused rtk_dprx_assr_mode_setting(struct rtk_dprx *dprx)
{
	if ((dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x0A) & _BIT0) == _BIT0) {
		/* Scramble seed equal to 0xFFFE */
		dprx->rbus_ops->set_bit(PB_07_SCRAMBLE_CTRL, ~_BIT4, _BIT4);
	} else {
		/* Scramble seed equal to 0xFFFF */
		dprx->rbus_ops->set_bit(PB_07_SCRAMBLE_CTRL, ~_BIT4, 0x00);
	}
}

/**
 * rtk_dprx_normal_pre_detect - Signal PreDetection for DP(Power Normal)
 */
static int rtk_dprx_normal_pre_detect(struct rtk_dprx *dprx)
{
	int ret = DET_NORMAL_FAIL;
	int check_ret;
	u8 test_sink;
	u8 link_bw_set;
	u8 lane_count_set;
	u8 set_power_state;

	/* For Dp PHY CTS Test */
	if (dprx->phy_ops->get_phy_cts_flag(dprx) == true) {
		dprx->phy_ops->phy_cts(dprx);

		ret = DET_IN_PHY_CTS;
		goto exit;
	}
#if 0 // TODO: FIXME
	/*
	 * Check if source doing HDCP handshake while HDCP capability disable,
	 * return _TRUE for switching to _SOURCE_SEARCH_DELAY_REACTIVE_MODE
	 */
	if (ScalerDpAuxRxGetHdcpHandshakeWithoutCap() == true) {
		ret = DPRX_NO_ERR;
		goto exit;
	}
#endif
	/* Normal Link Training Pass or Link Status Fail recovery path */
	if (rtk_dprx_lt_is_normal_pass(dprx) || rtk_dprx_lt_get_link_integrity_fail(dprx)) {
		dprx->aux_ops->clr_valid_video_check(dprx);

		test_sink = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, _BIT7);
		if (test_sink == _BIT7) {
			dev_info(dprx->dev, "DP MAC RX0: Normal LT -> PHY CTS Loop\n");

			/* PHY CTS Auto Test Mode after Link Training */
			dprx->phy_ops->phy_cts_auto_mode(dprx);
		}

		link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
		lane_count_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01);
		check_ret = rtk_dprx_five_layer_check(dprx, link_bw_set, lane_count_set);
		if (check_ret == DPRX_NO_ERR) {
			dev_info(dprx->dev, "DP MAC RX0: Normal LT Pass\n");

			dprx->aux_ops->cancel_link_status_irq(dprx);

			if (dprx->lt_setphy_finish == true) {
				/* Mac Secondary Data Block Reset */
				rtk_dprx_sec_data_block_reset(dprx);

				msleep(_DP_TWO_FRAME_TIME_MAX);

				dprx->lt_setphy_finish = false;
			}

			dprx->aux_ops->set_manual_mode(dprx);

			if (rtk_dprx_lt_get_link_integrity_fail(dprx)) {
				rtk_dprx_lt_set_link_integrity_fail(dprx, false);

				rtk_dprx_scramble_setting(dprx);
			}

			dprx->aux_ops->set_auto_mode(dprx);

			set_power_state = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x06, 0x00, (_BIT2 | _BIT1 | _BIT0));
			if ((set_power_state == _BIT1) ||
			   (set_power_state == (_BIT2 | _BIT0))) {
				ret = DET_POWER_DOWN;
				goto exit;
			}

			ret = DPRX_NO_ERR;
			goto exit;
		}

		ret = check_ret;
		dev_info(dprx->dev, "DP MAC RX0: Link Status Fail IRQ (err=%d)\n", check_ret);

		dprx->aux_ops->set_manual_mode(dprx);

		rtk_dprx_lt_set_link_integrity_fail(dprx, true);
		dprx->aux_ops->active_link_status_irq(dprx);

		dprx->aux_ops->set_auto_mode(dprx);

	/* Link Training Failed path */
	} else if (rtk_dprx_lt_get_state(dprx) == LT_STATE_FAILED) {

		dprx->aux_ops->cancel_link_status_irq(dprx);

		dprx->aux_ops->link_status_irq(dprx);

	/* Fake Training or VBIOS mode path */
	} else if (rtk_dprx_lt_get_fake_training_mode(dprx) || rtk_dprx_lt_get_vbios_mode(dprx)) {
		dprx->aux_ops->clr_valid_video_check(dprx);

		msleep(30);

		dev_info(dprx->dev, "DP MAC RX0: Rebuild PHY\n");

		dprx->aux_ops->cancel_link_status_irq(dprx);

		dprx->aux_ops->set_manual_mode(dprx);

		link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
		lane_count_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01);
		if (rtk_dprx_lt_get_fake_training_mode(dprx) ||
		    rtk_dprx_lt_get_vbios_mode(dprx)) {
			dprx->phy_ops->rebuild_phy(dprx, link_bw_set, lane_count_set);
		}

		dprx->aux_ops->set_auto_mode(dprx);

		/* Mac Reset After Link Clock Stable */
		rtk_dprx_reset(dprx);

		if (rtk_dprx_lt_get_fake_training_mode(dprx))
			rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_8B10B);

		link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
		lane_count_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01);
		check_ret = rtk_dprx_five_layer_check(dprx, link_bw_set, lane_count_set);
		if (check_ret == DPRX_NO_ERR) {
			dev_info(dprx->dev, "DP MAC RX0: Rebuild Phy Pass\n");

			dprx->aux_ops->set_manual_mode(dprx);

			if (rtk_dprx_lt_get_fake_training_mode(dprx) ||
			    rtk_dprx_lt_get_vbios_mode(dprx)) {
				/* Clear fake/vbios mode flags and mark link integrity OK */
				rtk_dprx_lt_set_fake_training_mode(dprx, false);
				rtk_dprx_lt_set_vbios_mode(dprx, false);
				rtk_dprx_lt_set_link_integrity_fail(dprx, false);
				dprx->lt_setphy_finish = false;

				rtk_dprx_scramble_setting(dprx);
			}

			dprx->aux_ops->set_auto_mode(dprx);

			msleep(_DP_ONE_FRAME_TIME_MAX);

			if ((dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x06, 0x00, (_BIT2 | _BIT1 | _BIT0)) == _BIT1) ||
			   (dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x06, 0x00, (_BIT2 | _BIT1 | _BIT0)) == (_BIT2 | _BIT0))) {
				return DET_POWER_DOWN;
			}

			ret = DPRX_NO_ERR;
			goto exit;
		} else {
			ret = check_ret;
			dev_info(dprx->dev, "DP MAC RX0: Rebuild PHY Fail (err=%d)\n", check_ret);

			dprx->aux_ops->set_manual_mode(dprx);

			if (rtk_dprx_lt_get_vbios_mode(dprx)) {
				dprx->aux_ops->reset_dpcd_link_status(dprx, _DP_DPCD_LINK_STATUS_INITIAL);

				if (dprx->hdcp_support) {
					// TODO: ScalerDpHdcp14RxResetProc();
				}

			} else if (rtk_dprx_lt_get_fake_training_mode(dprx)) {
				rtk_dprx_lt_set_link_integrity_fail(dprx, true);
			}

			dprx->aux_ops->set_auto_mode(dprx);
		}

	/* Link Training Not Started (VBIOS check) */
	} else if (!rtk_dprx_lt_is_trained(dprx)) {
		if (dprx->aux_ops->get_valid_video_check(dprx) == true) {
			int det_result;

			det_result = rtk_dprx_valid_signal_detection(dprx);
			if (det_result)
				ret = det_result;

			dev_info(dprx->dev, "DP MAC RX0: VBIOS Check\n");
		}
	}

	if (dprx->aux_ops->get_valid_video_check(dprx) == true)
		dprx->aux_ops->clr_valid_video_check(dprx);

exit:
	return ret;
}

/**
 * rtk_dprx_reset - DP MAC Reset
 */
static void rtk_dprx_reset(struct rtk_dprx *dprx)
{
	/* De-Skew Circuit Reset */
	dprx->rbus_ops->set_bit(PB_0E_DESKEW_PHY, ~(_BIT7 | _BIT6 | _BIT4 | _BIT1), 0x00);
	dprx->rbus_ops->set_bit(PB_0E_DESKEW_PHY, ~(_BIT7 | _BIT6 | _BIT4 | _BIT1), _BIT6);

	/* Mac Reset After Link Clock Stable */
	dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT1, _BIT1);
	dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT1, 0x00);

	/* Delay for Lane Alignment after Mac Reset */
	usleep_range(2000, 2100);

	if (dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x01, 0x20, _BIT0) == _BIT0) {
		/* [4] Disable Comma Detection */
		dprx->rbus_ops->set_bit(PB_05_SAMPLE_EDGE, ~_BIT4, _BIT4);
	}

	/* Mac Secondary Data Block Reset */
	rtk_dprx_sec_data_block_reset(dprx);
}

/**
 * rtk_dprx_valid_signal_detection - Check Valid Video Data
 */
static int rtk_dprx_valid_signal_detection(struct rtk_dprx *dprx)
{
	int ret = DPRX_NO_ERR;
	u8 link_rate = 0;
	u8 lane_count = _DP_FOUR_LANE;

	dprx->aux_ops->set_manual_mode(dprx);

	if (!rtk_dprx_lt_is_trained(dprx) &&
	   (dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00) == 0x00)) {
		link_rate = rtk_dprx_link_rate_detect(dprx);

		dprx->aux_ops->set_auto_mode(dprx);

		if (link_rate == _DP_LINK_NONE) {
			ret = SIGNAL_LINK_NONE;
			goto exit;
		}

		do {
			dprx->aux_ops->set_manual_mode(dprx);

			if (!rtk_dprx_lt_is_trained(dprx) &&
			   (dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00) == 0x00)) {
				dprx->phy_ops->rebuild_phy(dprx, link_rate, (lane_count | _BIT7));

				dprx->aux_ops->set_auto_mode(dprx);

				if (rtk_dprx_vbios_msa_check(dprx, false) == true)
					break;

				if (rtk_dprx_vbios_msa_check(dprx, true) == true)
					break;

				if (lane_count == _DP_FOUR_LANE)
					lane_count = _DP_TWO_LANE;
				else if (lane_count == _DP_TWO_LANE)
					lane_count = _DP_ONE_LANE;
				else if (lane_count == _DP_ONE_LANE)
					lane_count = 0x00;

			} else {
				dprx->aux_ops->set_auto_mode(dprx);
				ret = DPRX_NO_ERR;
				goto exit;
			}
		} while ((lane_count == _DP_TWO_LANE) || (lane_count == _DP_ONE_LANE));

		dprx->aux_ops->set_manual_mode(dprx);

		if (!rtk_dprx_lt_is_trained(dprx) &&
			(dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00) == 0x00)) {
			u8 lane_count_set;

			dprx->aux_ops->set_dpcd_write_value(dprx, 0x00, 0x01, 0x00, link_rate);
			lane_count_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01);
			lane_count_set = ((lane_count_set & 0x60) | (lane_count | _BIT7));
			dprx->aux_ops->set_dpcd_write_value(dprx, 0x00, 0x01, 0x01, lane_count_set);

			if (lane_count == _DP_FOUR_LANE) {
				dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x02, 0x77);
				dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x03, 0x77);
			} else if (lane_count == _DP_TWO_LANE) {
				dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x02, 0x77);
				dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x03, 0x00);
			} else {
				dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x02, 0x07);
				dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x03, 0x00);
			}

			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x04, 0x01);

			/* Set VBIOS mode and transition to TRAINED state */
			rtk_dprx_lt_set_vbios_mode(dprx, true);
			if (rtk_dprx_lt_get_state(dprx) != LT_STATE_TRAINED) {
				/* Use TP_END event to trigger state transition to TRAINED */
				rtk_dprx_lt_handle_event(dprx, LT_EVENT_TP_END);
			}
		}

		dprx->aux_ops->set_auto_mode(dprx);
	}

	dprx->aux_ops->set_auto_mode(dprx);

	dev_info(dprx->dev, "VBIOS_Link_Rate=%u VBIOS_Lane_Count=%u\n",
		dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00),
		dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01));

exit:
	return ret;
}

/**
 * rtk_dprx_link_rate_detect - DP Link Rate Detect
 */
static u8 rtk_dprx_link_rate_detect(struct rtk_dprx *dprx)
{
	u8 link_rate = 0;
	u8 link_rate_decide = _DP_LINK_NONE;
	u8 max_link_rate;

	do {
		if (link_rate == 0) {
			max_link_rate = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x00, 0x01);
			if (max_link_rate <= _HW_DP_D0_MAX_LINK_RATE_SUPPORT)
				link_rate = _HW_DP_D0_MAX_LINK_RATE_SUPPORT;
			else
				link_rate = max_link_rate;
		} else if (link_rate == _DP_LINK_HBR3) {
			link_rate = _DP_LINK_HBR2;
		} else if (link_rate == _DP_LINK_HBR2) {
			link_rate = _DP_LINK_HBR;
		} else if (link_rate == _DP_LINK_HBR) {
			link_rate = _DP_LINK_RBR;
		}

		if (rtk_dprx_link_rate_check(dprx, link_rate) == true) {
			link_rate_decide = link_rate;
			break;
		}
	} while (link_rate != _DP_LINK_RBR);

	return link_rate_decide;
}

/**
 * rtk_dprx_link_rate_check - Check Valid Lane
 */
static bool rtk_dprx_link_rate_check(struct rtk_dprx *dprx, u8 link_rate)
{
	u32 data_stream_l0 = 0;
	u8 leq_scan_val = _DP_RX_RELOAD_LEQ_INITIAL;
	u32 upper_bound = 0;
	u32 lower_bound = 0;
	u8 lane_sel;

	do {
		if (leq_scan_val == _DP_RX_RELOAD_LEQ_INITIAL)
			leq_scan_val = _DP_RX_RELOAD_LEQ_LARGE;
		else if (leq_scan_val == _DP_RX_RELOAD_LEQ_LARGE)
			leq_scan_val = _DP_RX_RELOAD_LEQ_DEFAULT;

		dprx->phy_ops->signal_detect_initial(dprx, link_rate, leq_scan_val);

		lane_sel = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
		data_stream_l0 = rtk_dprx_signal_detect_measure_count(dprx, lane_sel,
			_DP_MEASURE_TARGET_RAW_DATA, _DP_MEASURE_PERIOD_2000_CYCLE);

		switch (link_rate) {
		case _DP_LINK_HBR3:
			upper_bound = _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR3_SAVED;
			lower_bound = _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR3_SAVED;
			break;

		case _DP_LINK_HBR2:
			upper_bound = _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR2_SAVED;
			lower_bound = _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR2_SAVED;
			break;

		case _DP_LINK_HBR:
			upper_bound = _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR_SAVED;
			lower_bound = _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR_SAVED;
			break;

		case _DP_LINK_RBR:
			upper_bound = _DP_RX_COUNT_SST_UPPER_BOUND_2000_RBR_SAVED;
			lower_bound = _DP_RX_COUNT_SST_LOWER_BOUND_2000_RBR_SAVED;
			break;

		default:
			break;
		}

		if ((data_stream_l0 < upper_bound) && (data_stream_l0 > lower_bound))
			return true;

	} while (leq_scan_val != _DP_RX_RELOAD_LEQ_DEFAULT);

	return false;
}

/**
 * rtk_dprx_signal_detect_measure_count - DP Signal Detection Measure
 */
static u32 rtk_dprx_signal_detect_measure_count(struct rtk_dprx *dprx,
		u8 lane_sel, enum RTK_MEASURE_TARGET target, enum RTK_MEASURE_PERIOD period)
{
	u32 measure_count = 0;
	u32 i;

	if (target == _DP_MEASURE_TARGET_RAW_DATA)
		dprx->phy_ops->signal_detection(dprx, true);

	/* [1:0] freqdet_lane_sel */
	dprx->rbus_ops->set_bit(PB_51_DP_SIG_DET_1,
		~(_BIT1 | _BIT0), (lane_sel & (_BIT1 | _BIT0)));

	/* [5] ln_ck_sel */
	dprx->rbus_ops->set_bit(PB_50_DP_SIG_DET_0, ~_BIT5, target);

	switch (period) {
	case _DP_MEASURE_PERIOD_1000_CYCLE:
		/* [4:0] DP_XTAL_CYCLE = 5'b00011 */
		dprx->rbus_ops->set_bit(PB_50_DP_SIG_DET_0,
			~(_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), (_BIT1 | _BIT0));
		break;

	case _DP_MEASURE_PERIOD_2000_CYCLE:
		fallthrough;
	default:
		/* [4:0] DP_XTAL_CYCLE = 5'b00100 */
		dprx->rbus_ops->set_bit(PB_50_DP_SIG_DET_0,
			~(_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT2);
		break;
	}

	/* [6] LANE_COUNT_CLEAR = 0 -> Keep the LANE_COUNT value */
	dprx->rbus_ops->set_bit(PB_52_DP_SIG_DET_2, ~_BIT6, 0x00);

	/* [7] DP_SIG_DET_EN = 1'b1 -> Enable Signal Detection */
	dprx->rbus_ops->set_bit(PB_50_DP_SIG_DET_0, ~_BIT7, 0x00);
	dprx->rbus_ops->set_bit(PB_50_DP_SIG_DET_0, ~_BIT7, _BIT7);

	/* Delay Time us [5,150] Polling for Measure Done */
	for (i = 0; i <= 30; i++) {
		usleep_range(5, 6);

		if (dprx->rbus_ops->get_bit(PB_50_DP_SIG_DET_0, _BIT6) == _BIT6) {
			measure_count = dprx->rbus_ops->get_word(PB_53_DP_SIG_DET_3);

			break;
		}
	}

	/* [7] DP_SIG_DET_EN = 0 -> Disable Signal Detection */
	dprx->rbus_ops->set_bit(PB_50_DP_SIG_DET_0, ~_BIT7, 0x00);

	dprx->phy_ops->signal_detection(dprx, false);

	return measure_count;
}

/**
 * rtk_dprx_measure_info_check() - Check DP Link Info from Measure Function
 * @dprx: pointer to rtk_dprx structure
 * @stream_info: pointer to stream info structure
 *
 * Check if Bs2Bs count is valid (CNT=0 means IDLE pattern).
 *
 * Return: true if link info is valid, false otherwise.
 */
static bool rtk_dprx_measure_info_check(struct rtk_dprx *dprx,
	struct rtk_dprx_stream_info *stream_info)
{
	/* Bs2Bs Count Check, CNT=0 => IDLE pattern */
	if (stream_info->link_info.HBsToBsCount == 0 ||
		stream_info->link_info.VBsToBsCountN == 0 ||
		stream_info->link_info.VBsToBsCountN1 == 0)
		return false;

	return true;
}

/**
 * rtk_dprx_polarity - Update timing info HS/VS polarity
 */
static void __maybe_unused rtk_dprx_polarity(struct rtk_dprx *dprx)
{
	// TODO: REMOVE?
}

/**
 * rtk_dprx_signal_check - Check Valid Signal
 */
static bool __maybe_unused rtk_dprx_signal_check(struct rtk_dprx *dprx, u8 link_rate, u8 dpcd_lane)
{
	u8 lane_sel0;
	u8 lane_sel1;
	u8 lane_sel2;
	u8 lane_sel3;
	u32 stream_l0 = 0;
	u32 stream_l1 = 0;
	u32 stream_l2 = 0;
	u32 stream_l3 = 0;
	u32 upper_bound = 0;
	u32 lower_bound = 0;

	/* Scrambling Disable Check */
	if (dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x01, 0x02, _BIT5) == _BIT5)
		return true;

	/* Measure Data Stream Count */
	switch (dpcd_lane) {
	case _DP_ONE_LANE:
		lane_sel0 = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
		stream_l0 = rtk_dprx_signal_detect_measure_count(dprx, lane_sel0, _DP_MEASURE_TARGET_RAW_DATA, _DP_MEASURE_PERIOD_2000_CYCLE);
		break;

	case _DP_TWO_LANE:
		lane_sel0 = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
		lane_sel1 = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_1);
		stream_l0 = rtk_dprx_signal_detect_measure_count(dprx, lane_sel0, _DP_MEASURE_TARGET_RAW_DATA, _DP_MEASURE_PERIOD_2000_CYCLE);
		stream_l1 = rtk_dprx_signal_detect_measure_count(dprx, lane_sel1, _DP_MEASURE_TARGET_RAW_DATA, _DP_MEASURE_PERIOD_2000_CYCLE);
		break;

	case _DP_FOUR_LANE:
		fallthrough;
	default:
		lane_sel0 = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
		lane_sel1 = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_1);
		lane_sel2 = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_2);
		lane_sel3 = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_3);
		stream_l0 = rtk_dprx_signal_detect_measure_count(dprx, lane_sel0, _DP_MEASURE_TARGET_RAW_DATA, _DP_MEASURE_PERIOD_2000_CYCLE);
		stream_l1 = rtk_dprx_signal_detect_measure_count(dprx, lane_sel1, _DP_MEASURE_TARGET_RAW_DATA, _DP_MEASURE_PERIOD_2000_CYCLE);
		stream_l2 = rtk_dprx_signal_detect_measure_count(dprx, lane_sel2, _DP_MEASURE_TARGET_RAW_DATA, _DP_MEASURE_PERIOD_2000_CYCLE);
		stream_l3 = rtk_dprx_signal_detect_measure_count(dprx, lane_sel3, _DP_MEASURE_TARGET_RAW_DATA, _DP_MEASURE_PERIOD_2000_CYCLE);
		break;
	}

	dev_info(dprx->dev, "lane_count=%u measure_count=%u %u %u %u\n",
				dpcd_lane, stream_l0, stream_l1, stream_l2, stream_l3);

	/* Data Stream Count Upper Bound = (VCO target count) x 2 x 0.65 */
	/* Data Stream Count Lower Bound = (VCO target count) x 2 x 0.55 */
	switch (link_rate) {
	case _DP_LINK_HBR3:
		upper_bound = _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR3_SAVED;
		lower_bound = _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR3_SAVED;
		break;

	case _DP_LINK_HBR2:
		upper_bound = _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR2_SAVED;
		lower_bound = _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR2_SAVED;
		break;

	case _DP_LINK_HBR:
		upper_bound = _DP_RX_COUNT_SST_UPPER_BOUND_2000_HBR_SAVED;
		lower_bound = _DP_RX_COUNT_SST_LOWER_BOUND_2000_HBR_SAVED;
		break;

	case _DP_LINK_RBR:
		upper_bound = _DP_RX_COUNT_SST_UPPER_BOUND_2000_RBR_SAVED;
		lower_bound = _DP_RX_COUNT_SST_LOWER_BOUND_2000_RBR_SAVED;
		break;

	default:
		break;
	}

	/* Link Rate is a Unreasonable Value */
	if ((upper_bound == 0) || (lower_bound == 0))
		return false;

	/* Check Data Stream Count */
	switch (dpcd_lane) {
	case _DP_ONE_LANE:
		if ((stream_l0 > upper_bound) || (stream_l0 < lower_bound))
			return false;
		break;

	case _DP_TWO_LANE:

		if ((stream_l0 > upper_bound) || (stream_l0 < lower_bound) ||
			(stream_l1 > upper_bound) || (stream_l1 < lower_bound))
			return false;
		break;

	case _DP_FOUR_LANE:
		fallthrough;
	default:
		if ((stream_l0 > upper_bound) || (stream_l0 < lower_bound) ||
			(stream_l1 > upper_bound) || (stream_l1 < lower_bound) ||
			(stream_l2 > upper_bound) || (stream_l2 < lower_bound) ||
			(stream_l3 > upper_bound) || (stream_l3 < lower_bound))
			return false;
		break;
	}

	return true;
}

/**
 * rtk_dprx_vbios_msa_check - VBIOS Lane Adjust
 */
static bool rtk_dprx_vbios_msa_check(struct rtk_dprx *dprx, bool de_skew_enhanced)
{
	bool check_result = false;
	u8 msa_nvid[3];
	u32 h_active;
	u32 v_active;
	u32 h_start;
	u32 v_start;
	unsigned long time_start;

	time_start = jiffies;
	do {
		/* De-Skew Circuit Reset */
		dprx->rbus_ops->set_bit(PB_0E_DESKEW_PHY, ~(_BIT7 | _BIT6 | _BIT4 | _BIT1), 0x00);
		dprx->rbus_ops->set_bit(PB_0E_DESKEW_PHY,
			~(_BIT7 | _BIT6 | _BIT4 | _BIT1),
			(((de_skew_enhanced == true) ? _BIT7 : 0x00) | _BIT6));

		/* Mac Reset After Link Clock Stable */
		dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT1, _BIT1);
		dprx->rbus_ops->set_bit(PB_01_PHY_DIG_RESET_CTRL, ~_BIT1, 0x00);

		/* SEC Reset */
		dprx->rbus_ops->set_bit(PB5_1E_MAC_DIG_RESET_CTRL, ~_BIT4, _BIT4);
		dprx->rbus_ops->set_bit(PB5_1E_MAC_DIG_RESET_CTRL, ~_BIT4, 0x00);

		/* Wait Two Frame Time to Get MSA */
		msleep(_DP_TWO_FRAME_TIME_MAX);

		/* Pop up Main Stream Attributes */
		dprx->rbus_ops->set_bit(PB6_00_MN_STRM_ATTR_CTRL,
			~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT0),
			(_BIT7 | _BIT6 | _BIT5 | _BIT3));

		/* Get Nvid */
		msa_nvid[0] = dprx->rbus_ops->get_byte(PB6_1D_MSA_NVID_0);
		msa_nvid[1] = dprx->rbus_ops->get_byte(PB6_1E_MSA_NVID_1);
		msa_nvid[2] = dprx->rbus_ops->get_byte(PB6_1F_MSA_NVID_2);

		/* Get Hactive */
		h_active = dprx->rbus_ops->get_word(PB6_0C_MSA_HWD_0);

		/* Get Vactive */
		v_active = dprx->rbus_ops->get_word(PB6_16_MSA_VHT_0);

		/* Get Hstart */
		h_start = dprx->rbus_ops->get_word(PB6_0A_MSA_HST_0);

		/* Get Vstart */
		v_start = dprx->rbus_ops->get_word(PB6_14_MSA_VST_0);

		if (((((u32)msa_nvid[0] << 16) | ((u32)msa_nvid[1] << 8) | ((u32)msa_nvid[2] << 0)) != 0x00) &&
		   (h_active != 0x00) && (v_active != 0x00) && (h_start != 0x00) && (v_start != 0x00) &&
		   (h_active > h_start) && (v_active > v_start)) {
			check_result = true;

			break;
		}

		if (jiffies_to_msecs(jiffies - time_start) > _DP_TWO_FRAME_TIME_MAX*10) {
			dev_err(dprx->dev, "vbios_msa_check timeout, h_active=%u v_active=%u h_start=%u v_start=%u\n",
				h_active, v_active, h_start, v_start);
			break;
		}
	} while (rtk_dprx_change_sramble_seed(dprx) == true);

	return check_result;
}

/**
 * rtk_dprx_change_sramble_seed - Change Dp Scramble Seed
 */
bool rtk_dprx_change_sramble_seed(struct rtk_dprx *dprx)
{
	return false;
}

/**
 * rtk_dprx_update_v4l2_timings - Update V4L2 timing info after successful scan
 */
static void rtk_dprx_update_v4l2_timings(struct rtk_dprx *dprx,
	const struct rtk_dprx_stream_info *stream_info)
{
	const struct rtk_timing_info *ti = &stream_info->timing_info;
	const struct rtk_link_info *li = &stream_info->link_info;
	struct v4l2_bt_timings *bt = &dprx->detected_timings;
	u32 polarities = 0;

	memset(bt, 0, sizeof(*bt));

	bt->height = ti->VHeight;

	/*
	 * YCbCr 4:2:0: timing_info horizontal fields were halved in
	 * rtk_dprx_set_color_info for DFG dual-pixel mode (DFG runs at
	 * half the pixel clock, e.g. 297 MHz for 4K60). Restore original
	 * values for V4L2 so applications see the true 3840x2160 resolution
	 * with consistent timing (htotal/vtotal at 594 MHz → 60 Hz).
	 */
	if (dprx->mac_dat.color_space == _COLOR_SPACE_YCBCR420) {
		bt->width       = ti->HWidth  * 2;
		bt->hsync       = ti->HSWidth * 2;
		bt->hbackporch  = (ti->HStart - ti->HSWidth) * 2;
		bt->hfrontporch = (ti->HTotal - ti->HStart - ti->HWidth) * 2;
		bt->pixelclock  = li->PixelClockHz * 2;
	} else {
		bt->width       = ti->HWidth;
		bt->hsync       = ti->HSWidth;
		bt->hbackporch  = ti->HStart - ti->HSWidth;
		bt->hfrontporch = ti->HTotal - ti->HStart - ti->HWidth;
		bt->pixelclock  = li->PixelClockHz;
	}

	bt->vsync = ti->VSWidth;
	bt->vbackporch = ti->VStart - ti->VSWidth;
	bt->vfrontporch = ti->VTotal - ti->VStart - ti->VHeight;

	if (ti->HSP)
		polarities |= V4L2_DV_HSYNC_POS_POL;
	if (ti->VSP)
		polarities |= V4L2_DV_VSYNC_POS_POL;
	bt->polarities = polarities;

	if (ti->Interlace)
		bt->interlaced = V4L2_DV_INTERLACED;
	else
		bt->interlaced = V4L2_DV_PROGRESSIVE;

	/* VFreq in 0.1Hz: same formula as rtk_dprx_get_v_freq() */
	if (li->VBsToBsCountN)
		dprx->src_vfreq = (u32)_GDIPHY_RX_GDI_CLK_KHZ * 1000 /
				   (li->VBsToBsCountN / 10);
	else
		dprx->src_vfreq = 0;

	dprx->v4l2_input_status = 0;

	dev_info(dprx->dev, "V4L2 timings: %ux%u pixclk=%u vfreq=%u.%uHz\n",
		 bt->width, bt->height, li->PixelClockHz,
		 dprx->src_vfreq / 10, dprx->src_vfreq % 10);
}

/**
 * rtk_dprx_scan_input_port - DP Port Source/Sync Scanning and Setting
 */
static int rtk_dprx_scan_input_port(struct rtk_dprx *dprx)
{
	int ret = DPRX_NO_ERR;
	struct rtk_dprx_stream_info stream_info;

	memset_io(&stream_info, 0, sizeof(stream_info));

	if (!rtk_dprx_lt_can_scan_video(dprx)) {
		ret = SCAN_LT_NOT_PASSED;
		goto exit;
	}

	/* 2nd Scramble Setting Sync */
	rtk_dprx_scramble_setting(dprx);

	if (!dprx->hdcp_support)
		goto skip_hdcp;

	if (rtk_dprx_hdcp_check(dprx) == false) {
		rtk_dprx_cp_irq(dprx, _DP_HDCP_BSTATUS_LINK_INTEGRITY_FAIL);
		ret = SCAN_HDCP_CHECK_FAIL;
		goto exit;
	}
skip_hdcp:

	if (rtk_dprx_get_video_stream(dprx) == false) {
		ret = SCAN_VS_CHECK_FAIL;
		goto exit;
	}

	if (dprx->free_sync_support == true)
		dprx->mac_dat.en_free_sync = true;
	else
		dprx->mac_dat.en_free_sync = false;

	ret = rtk_dprx_get_msa_timing_info(dprx, &stream_info);
	if (ret)
		goto exit;

	ret = rtk_dprx_get_video_info(dprx);
	if (ret)
		goto exit;

	rtk_dprx_set_color_info(dprx, &stream_info.timing_info);

	if (rtk_dprx_get_measure_link_info(dprx, &stream_info) == false) {
		ret = SCAN_GET_MEASURE_INFO_FAIL;
		goto exit;
	}

	if (rtk_dprx_display_format_setting(dprx, &stream_info) == false) {
		ret = SCAN_FMT_GEN_FAIL;
		goto exit;
	}

	/* For YCbCr 4:2:0, timing_info.HWidth was halved by rtk_dprx_set_color_info()
	 * (e.g. 3840→1920). DPRX14_SIZE1_src_width must reflect the true pixel width. */
	if (dprx->mac_dat.color_space == _COLOR_SPACE_YCBCR420)
		dprx->src_width = stream_info.timing_info.HWidth * 2;
	else
		dprx->src_width = stream_info.timing_info.HWidth;

	dprx->src_height = stream_info.timing_info.VHeight;

	/* Set wrapper src_fmt based on detected color space */
	switch (dprx->mac_dat.color_space) {
	case _COLOR_SPACE_YCBCR422:
		dprx->src_fmt = SRC_COLOR_FMT_Y422;
		break;
	case _COLOR_SPACE_YCBCR444:
		dprx->src_fmt = SRC_COLOR_FMT_Y444;
		break;
	case _COLOR_SPACE_YCBCR420:
		dprx->src_fmt = SRC_COLOR_FMT_Y420;
		break;
	default:
		dprx->src_fmt = SRC_COLOR_FMT_RGB;
		break;
	}

	rtk_dprx_update_v4l2_timings(dprx, &stream_info);

	ret = rtk_dprx_stream_clk_regenerate(dprx, &stream_info.link_info);
	if (ret != DPRX_NO_ERR) {
		dev_info(dprx->dev, "Stream CLK Regeneration Failed, ret=%d\n", ret);
		goto exit;
	}

	if (rtk_dprx_tracking_setting(dprx, &stream_info) == false) {
		ret = SCAN_TRACKING_FAIL;
		goto exit;
	}

	ret = rtk_dprx_fifo_check_proc(dprx, _DP_FIFO_DELAY_CHECK, &stream_info);
	if (ret)
		goto exit;

	/* Use for Video/DisplayFormat/Measure relative Settings */
	rtk_dprx_mac_setting(dprx);

	/* Use for Audio/Sdp relative Settings */
	rtk_dprx_sdp_setting(dprx);

exit:
	return ret;
}

/**
 * rtk_dprx_fifo_check_proc - Mac FIFO Check
 */
static int rtk_dprx_fifo_check_proc(struct rtk_dprx *dprx, enum RTK_DP_FIFO_CHECK_CONDITION check_condition,
		struct rtk_dprx_stream_info *stream_info)
{
	int ret;

	ret = rtk_dprx_fifo_check(dprx, check_condition);
	if (ret) {
		if (dprx->mac_dat.en_free_sync)
			goto exit;

		rtk_dprx_adjust_vsync_delay(dprx, stream_info);

		ret = rtk_dprx_fifo_check(dprx, check_condition);
		if (ret) {
			rtk_dprx_adjust_vsync_delay(dprx, stream_info);

			ret = rtk_dprx_fifo_check(dprx, check_condition);
			if (ret)
				goto exit;
		}
	}

	ret = DPRX_NO_ERR;
exit:
	return ret;
}

/**
 * rtk_dprx_mac_setting - Video/DisplayFormat/Measure relative Settings
 */
static void rtk_dprx_mac_setting(struct rtk_dprx *dprx)
{
	/* Measurement Enable for On-Line VFreq check */
	if (dprx->mac_dat.en_free_sync)
		dprx->rbus_ops->set_bit(PB5_58_MN_MEAS_CTRL, ~(_BIT7 | _BIT5), _BIT7);

	/* RGB Output */
	dprx->rbus_ops->set_bit(PB5_31_DP_OUTPUT_CTRL,
		~(_BIT3 | _BIT2 | _BIT1 | _BIT0), (_BIT2 | _BIT1 | _BIT0));
}

/**
 * rtk_dprx_sdp_setting - Audio/Sdp relative Settings
 */
static void rtk_dprx_sdp_setting(struct rtk_dprx *dprx)
{
	/* Enable Audio Channel */
//	if (dprx->audio_support)
//		ScalerAudioDpAudioEnable(true);

	rtk_dprx_sdp_data_detect(dprx);

	rtk_dprx_get_spd_v_freq_max_min(dprx);
}

/**
 * rtk_dprx_get_vbid_info - Get VBID Information
 */
bool rtk_dprx_get_vbid_info(struct rtk_dprx *dprx,
		enum RTK_DP_VBID_INFO vbid_info)
{
	bool is_get_info = false;

	switch (vbid_info) {
	case _DP_VBID_INTERLACE_MODE:
		if (dprx->rbus_ops->get_bit(PB6_01_DP_VBID, _BIT2) == _BIT2)
			is_get_info = true;
		break;
	case _DP_VBID_VIDEO_STREAM:
		if (dprx->rbus_ops->get_bit(PB6_01_DP_VBID, _BIT3) == 0x00)
			is_get_info = true;
		break;
	case _DP_VBID_AUDIO_STREAM:
		if (dprx->rbus_ops->get_bit(PB6_01_DP_VBID, _BIT4) == 0x00)
			is_get_info = true;
		break;
	default:
		break;
	}

	return is_get_info;
}

/**
 * rtk_dprx_set_sdp_reset - DP Rx0 SDP Reset
 */
static void rtk_dprx_set_sdp_reset(struct rtk_dprx *dprx)
{
	rtk_dprx_set_sdp_init_status(dprx, _DP_SDP_BUFF_SPD);

	if (dprx->audio_support)
		rtk_dprx_set_sdp_init_status(dprx, _DP_SDP_BUFF_ISRC);
}

/**
 * rtk_dprx_set_sdp_init_status - DP Rx0 Set Clr Status Flag
 */
static void rtk_dprx_set_sdp_init_status(struct rtk_dprx *dprx,
	enum RTK_DP_SDP_BUFF index)
{
	switch (index) {
	case _DP_SDP_BUFF_SPD:
		/* Clr SPD Received Flag, Reset Sdp */
		dprx->rbus_ops->set_bit(PB6_2F_DP_SDP_SPD_CTRL,
			~(_BIT6 | _BIT0), (_BIT6 | _BIT0));
		dprx->rbus_ops->set_bit(PB6_2F_DP_SDP_SPD_CTRL,
			~(_BIT6 | _BIT0), 0x00);

		/* Clr SPD Chg Flag */
		dprx->rbus_ops->set_byte(PB6_31_DP_SDP_SPD_CHG, 0xFF);

		/* Set Info Data Byte Re-mapping */
		dprx->rbus_ops->set_bit(PB6_2F_DP_SDP_SPD_CTRL,
			~(_BIT6 | _BIT4), _BIT4);

		dprx->i_state.spd_ifnoframe_detecting = false;
		dprx->mac_dat.amd_spd_local_dimming = false;
		break;

	/* Audio Infoframe use ISRC SDP buff */
	case _DP_SDP_BUFF_ISRC:
		/* Clr ISRC Flag, Reset Sdp */
		dprx->rbus_ops->set_bit(PB6_E0_SDP_ACM_ISRC_INT,
			~(_BIT6 | _BIT5 | _BIT2 | _BIT1 | _BIT0),
			(_BIT6 | _BIT5 | _BIT2 | _BIT1 | _BIT0));
		dprx->rbus_ops->set_bit(PB6_E0_SDP_ACM_ISRC_INT,
			~(_BIT6 | _BIT5 | _BIT2 | _BIT1 | _BIT0), 0x00);

		if (dprx->audio_support) {
			/* Initial Audio Info Sdp Data */
			memset_io(dprx->mac_dat.AudioInfoSdpData, 0, _HW_DP_SDP_PAYLOAD_LENGTH);
			dprx->i_state.audio_ifnoframe_detecting = false;
		}
		break;

	/* PR Mode VSC SDP use RSV0 SDP buff */
	case _DP_SDP_BUFF_RSV0:
		/* Clr RSV0 Received Flag, Reset Sdp */
		dprx->rbus_ops->set_bit(PB6_F3_DP_SDP_RSV0_CTRL,
			~(_BIT6 | _BIT4), (_BIT6 | _BIT4));
		dprx->rbus_ops->set_bit(PB6_F3_DP_SDP_RSV0_CTRL,
			~(_BIT6 | _BIT4), 0x00);
		break;

	/* PR Mode VSC SDP(CRC Check) use RSV1 SDP buff */
	case _DP_SDP_BUFF_RSV1:
		/* Clr RSV1 Received Flag, Reset Sdp */
		dprx->rbus_ops->set_bit(PB6_F9_DP_SDP_RSV1_CTRL,
			~(_BIT6 | _BIT4), (_BIT6 | _BIT4));
		dprx->rbus_ops->set_bit(PB6_F9_DP_SDP_RSV1_CTRL,
			~(_BIT6 | _BIT4), 0x00);
		break;

	default:
		break;
	}
}

/**
 * rtk_dprx_sdp_data_detect - Sdp Dtect
 */
static void rtk_dprx_sdp_data_detect(struct rtk_dprx *dprx)
{
	rtk_dprx_sdp_packet_check(dprx, _DP_SDP_TYPE_INFOFRAME_SPD);

	if (dprx->audio_support)
		rtk_dprx_sdp_packet_check(dprx, _DP_SDP_TYPE_INFOFRAME_AUDIO);
}

/**
 * rtk_dprx_sdp_packet_check - Sdp Packet Check
 */
static void rtk_dprx_sdp_packet_check(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type)
{
	switch (sdp_type) {
	case _DP_SDP_TYPE_INFOFRAME_SPD:
		if (!dprx->i_state.spd_ifnoframe_detecting) {
			dprx->i_state.spd_ifnoframe_detecting = true;

			rtk_dprx_sdp_rev_detect(dprx, _DP_SDP_TYPE_INFOFRAME_SPD);

			if (dprx->i_state.amd_spd_infoframe_receive == true) {
				rtk_dprx_sdp_chg_detect(dprx, _DP_SDP_TYPE_INFOFRAME_SPD);

				/* HB3[7:2]: Infoframe Version */
				if ((rtk_dprx_get_sdp_info_hb3(dprx, _DP_SDP_TYPE_INFOFRAME_SPD) >> 2) < _INFOFRAME_SDP_VERSION_1_3)
					dprx->rbus_ops->set_bit(PB6_2F_DP_SDP_SPD_CTRL, ~(_BIT6 | _BIT4), 0x00);
			}
		}

		break;

	case _DP_SDP_TYPE_INFOFRAME_AUDIO:
		if (dprx->audio_support &&
			!dprx->i_state.audio_ifnoframe_detecting) {

			dprx->i_state.audio_ifnoframe_detecting = true;

			rtk_dprx_sdp_rev_detect(dprx, _DP_SDP_TYPE_INFOFRAME_AUDIO);

			if (dprx->i_state.audio_infoframe_receive == true)
				rtk_dprx_sdp_chg_detect(dprx, _DP_SDP_TYPE_INFOFRAME_AUDIO);

			// TODO: ScalerTimerActiveTimerEvent((_DP_ONE_FRAME_TIME_MAX * 3), _SCALER_TIMER_EVENT_DP_RX0_AUDIO_INFOFRAME_DETECTING_DONE);
		}
		break;

	default:
		break;
	}
}

/**
 * rtk_dprx_sdp_rev_detect - Sdp Receive Check
 */
static void rtk_dprx_sdp_rev_detect(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type)
{
	switch (sdp_type) {
	case _DP_SDP_TYPE_INFOFRAME_SPD:
		if (dprx->rbus_ops->get_bit(PB6_2F_DP_SDP_SPD_CTRL, _BIT6) == _BIT6) {
			dprx->rbus_ops->set_bit(PB6_2F_DP_SDP_SPD_CTRL, ~_BIT6, _BIT6);

			dprx->i_state.amd_spd_infoframe_receive = true;
		} else {
			dprx->i_state.amd_spd_infoframe_receive = false;
		}
		break;

	case _DP_SDP_TYPE_INFOFRAME_AUDIO:
		if (!dprx->audio_support)
			break;

		if (dprx->rbus_ops->get_bit(PB6_E0_SDP_ACM_ISRC_INT, _BIT1) == _BIT1) {
			dprx->rbus_ops->set_bit(PB6_E0_SDP_ACM_ISRC_INT, ~(_BIT6 | _BIT5 | _BIT2 | _BIT1), _BIT1);

			dprx->i_state.audio_infoframe_receive = true;
		} else {
			dprx->i_state.audio_infoframe_receive = false;
		}
		break;

	default:
		break;
	}
}

/**
 * rtk_dprx_sdp_chg_detect - Sdp Change Dtect
 */
static int rtk_dprx_sdp_chg_detect(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type)
{
	u8 sdp_data[_HW_DP_SDP_PAYLOAD_LENGTH];
	bool disable_ctrl = false;
	int ret = DPRX_NO_ERR;

	memset_io(sdp_data, 0, sizeof(sdp_data));

	switch (sdp_type) {
	case _DP_SDP_TYPE_INFOFRAME_SPD:
		ret = rtk_dprx_get_amd_spd_info(dprx, _SPD_INFO_SEAMLESS_LOCAL_DIMMING_DISABLE_CONTROL);
		if (ret != DPRX_NO_ERR)
			goto exit;

		disable_ctrl = (bool)ret;

		if ((dprx->rbus_ops->get_bit(PB6_31_DP_SDP_SPD_CHG, (_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2)) != 0x00) ||
			(disable_ctrl != dprx->mac_dat.amd_spd_local_dimming)) {
			dprx->rbus_ops->set_bit(PB6_31_DP_SDP_SPD_CHG,
				~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2),
				(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2));

			dprx->mac_dat.amd_spd_local_dimming = disable_ctrl;

			dprx->i_state.amd_spd_infoframe_change = true;
		} else {
			dprx->i_state.amd_spd_infoframe_change = false;
		}
		break;

	case _DP_SDP_TYPE_INFOFRAME_AUDIO:
		if (!dprx->audio_support)
			goto exit;

		ret = rtk_dprx_get_sdp_pkt_data(dprx, _DP_SDP_TYPE_INFOFRAME_AUDIO,
				sdp_data, 0, _HW_DP_SDP_PAYLOAD_LENGTH);
		if (ret != DPRX_NO_ERR)
			goto exit;

		if (memcmp(sdp_data, dprx->mac_dat.AudioInfoSdpData, _HW_DP_SDP_PAYLOAD_LENGTH) != 0) {
			memcpy(dprx->mac_dat.AudioInfoSdpData, sdp_data, _HW_DP_SDP_PAYLOAD_LENGTH);

			dprx->i_state.audio_infoframe_change = true;
		} else {
			dprx->i_state.audio_infoframe_change = false;
		}
		break;

	default:
		break;
	}

exit:
	return ret;
}

/**
 * rtk_dprx_get_sdp_received - Get Sdp Received
 */
static bool __maybe_unused rtk_dprx_get_sdp_received(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type)
{
	bool sdp_received = false;

	switch (sdp_type) {
	case _DP_SDP_TYPE_INFOFRAME_SPD:

		sdp_received = dprx->i_state.amd_spd_infoframe_receive;
		break;

	case _DP_SDP_TYPE_INFOFRAME_AUDIO:
		if (!dprx->audio_support)
			break;

		sdp_received = dprx->i_state.audio_infoframe_receive;
		break;

	default:
		break;
	}

	return sdp_received;
}

/**
 * rtk_dprx_get_sdp_changed - Get Sdp Changed
 */
static bool __maybe_unused rtk_dprx_get_sdp_changed(struct rtk_dprx *dprx, enum RTK_DP_SDP_TYPE sdp_type)
{
	bool sdp_changed = false;

	switch (sdp_type) {
	case _DP_SDP_TYPE_INFOFRAME_SPD:
		sdp_changed = dprx->i_state.amd_spd_infoframe_change;
		break;

	case _DP_SDP_TYPE_INFOFRAME_AUDIO:
		if (!dprx->audio_support)
			break;

		sdp_changed = dprx->i_state.audio_infoframe_change;
		break;

	default:
		break;
	}

	return sdp_changed;
}

/**
 * rtk_dprx_get_sdp_pkt_data - Get Sdp Packet Data
 *
 * @dprx: struct rtk_dprx
 * @sdp_type: enum RTK_DP_SDP_TYPE
 * @sdp_data: data buffer pointer, buffer size should greater than @length
 * @offset: data offset
 * @length: size of data read
 */
static int rtk_dprx_get_sdp_pkt_data(struct rtk_dprx *dprx,
	enum RTK_DP_SDP_TYPE sdp_type, u8 *sdp_data, u8 offset, u8 length)
{
	int ret;
	u8 index;
	u8 debounce_cnt;
	enum RTK_DP_SDP_BUFF buffer_type;
	u8 tmep_data1[_HW_DP_SDP_PAYLOAD_LENGTH];
	u8 tmep_data2[_HW_DP_SDP_PAYLOAD_LENGTH];
	u32 info_frame_addr = 0x00;
	u32 info_dada_addr = 0x00;

	if (sdp_data == NULL) {
		ret = DPRX_SDP_PKT_NULL_ERROR;
		goto exit;
	}

	if ((length + offset) > _HW_DP_SDP_PAYLOAD_LENGTH) {
		ret = DPRX_SDP_PKT_LENGTH_ERROR;
		goto exit;
	}

	buffer_type = rtk_dprx_get_sdp_buffer_type(dprx, sdp_type);
	switch (buffer_type) {
	case _DP_SDP_BUFF_SPD:
		info_frame_addr = PB6_32_DP_SDP_SPD_ADR;
		info_dada_addr = PB6_33_DP_SDP_SPD_DAT;
		break;

	case _DP_SDP_BUFF_HDR:
		info_frame_addr = PB6_39_DP_SDP_HDR_INFO_ADR;
		info_dada_addr = PB6_3A_DP_SDP_HDR_INFO_DAT;
		break;

	case _DP_SDP_BUFF_ISRC:
		info_frame_addr = PB6_E4_SDP_ISRC_ADR;
		info_dada_addr = PB6_E5_SDP_ISRC_DATA;
		break;

	case _DP_SDP_BUFF_VSC:
		info_frame_addr = PB6_47_VSC7;
		info_dada_addr = PB6_48_VSC8;
		break;

	default:
		break;
	}

	if ((info_frame_addr == 0x00) || (info_dada_addr == 0x00)) {
		ret = DPRX_SDP_PKT_TYPE_ERROR;
		goto exit;
	}

	for (index = 0; index < length; index++) {
		dprx->rbus_ops->set_byte(info_frame_addr, (offset + index));
		tmep_data1[index] = dprx->rbus_ops->get_byte(info_dada_addr);
	}

	for (debounce_cnt = 0; debounce_cnt < 5; debounce_cnt++) {

		for (index = 0; index < length; index++) {
			dprx->rbus_ops->set_byte(info_frame_addr, (offset + index));
			tmep_data2[index] = dprx->rbus_ops->get_byte(info_dada_addr);
		}

		if (memcmp(tmep_data1, tmep_data2, length) == 0)
			break;

		memcpy(tmep_data1, tmep_data2, length);
	}

	memcpy(sdp_data, tmep_data2, length);
	ret = DPRX_NO_ERR;
exit:
	return ret;
}

/**
 * rtk_dprx_get_sdp_info_hb3 - Get Spd Infoframe HB3
 */
static u8 rtk_dprx_get_sdp_info_hb3(struct rtk_dprx *dprx,
		enum RTK_DP_SDP_TYPE sdp_type)
{
	u8 hb3 = 0;

	switch (sdp_type) {
	case _DP_SDP_TYPE_INFOFRAME_SPD:
		hb3 = dprx->rbus_ops->get_byte(PB6_2E_DP_SDP_SPD_HB3);
		break;
	case _DP_SDP_TYPE_INFOFRAME_HDR:
		hb3 = dprx->rbus_ops->get_byte(PB6_36_DP_SDP_HDR_INFO_HB3);
		break;
	default:
		break;
	}

	return hb3;
}

/**
 * rtk_dprx_set_spd_info_detecting_done - Set Spd Infoframe Detecting Done
 */
static void __maybe_unused  rtk_dprx_set_spd_info_detecting_done(struct rtk_dprx *dprx)
{
	dprx->i_state.spd_ifnoframe_detecting = 0;
}

/**
 * rtk_dprx_set_audio_info_detecting_done - Set Audio Infoframe Detecting Done
 */
static void __maybe_unused rtk_dprx_set_audio_info_detecting_done(struct rtk_dprx *dprx)
{
	dprx->i_state.audio_ifnoframe_detecting = false;
}

/**
 * rtk_dprx_ps_pre_detect - Signal Detection for DP(Power Saving)
 */
static bool __maybe_unused rtk_dprx_ps_pre_detect(struct rtk_dprx *dprx)
{
	if (rtk_dprx_lt_is_normal_pass(dprx)) {
		dev_info(dprx->dev, "Normal Link Training under Fake Power Saving Case\n");
		return true;
	}

	if (rtk_dprx_lt_get_fake_training_mode(dprx) || rtk_dprx_lt_get_vbios_mode(dprx)) {
		/* For Fake Training mode, check power state */
		if (rtk_dprx_lt_get_fake_training_mode(dprx) &&
		    ((dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x06, 0x00, (_BIT2 | _BIT1 | _BIT0)) == _BIT1) ||
		     (dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x06, 0x00, (_BIT2 | _BIT1 | _BIT0)) == (_BIT2 | _BIT0)))) {
			/* Power down state, don't allow power saving */
			return false;
		}

		dev_info(dprx->dev, "Idle Pattern Case\n");
		return true;
	}

	return false;
}

/**
 * rtk_dprx_low_power_proc - Set DP MAC Clock Output / PLL Power Off
 */
static void __maybe_unused rtk_dprx_low_power_proc(struct rtk_dprx *dprx)
{
	/* Turn Off Output Clock */
	dprx->rbus_ops->set_bit(PB5_A2_PLL_OUT_CONTROL, ~_BIT7, 0x00);

	/* Turn Off SCLK PLL */
	dprx->rbus_ops->set_bit(PB5_A8_M2PLL_CONTROL, ~_BIT0, _BIT0);
}

/**
 * rtk_dprx_stable_detect - On Line Check DP stability
 */
static bool __maybe_unused rtk_dprx_stable_detect(struct rtk_dprx *dprx)
{
	u8 set_power_state;
	u8 link_bw_set;
	u8 lane_count_set;
	bool cdr_valid;
	bool signal_valid;
	int ret;

	set_power_state = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x06, 0x00, (_BIT2 | _BIT1 | _BIT0));
	if ((set_power_state == _BIT1) ||
	   (set_power_state == (_BIT2 | _BIT0))) {
		dev_info(dprx->dev, "stable_detect: Power Down\n");
	}

	link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
	lane_count_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01);
	cdr_valid = rtk_dprx_cdr_check(dprx, link_bw_set, (lane_count_set & 0x1F));
	signal_valid = rtk_dprx_signal_check(dprx, link_bw_set, (lane_count_set & 0x1F));
	if ((cdr_valid == false) ||
	   (rtk_dprx_align_check(dprx) == false) ||
	   (rtk_dprx_decode_check(dprx) == false) ||
	   (signal_valid == false)) {
		dev_info(dprx->dev, "stable_detect: Link Fail\n");
		return false;
	}

	if (rtk_dprx_get_video_stream(dprx) == false) {
		dev_info(dprx->dev, "stable_detect: Video Stream Fail\n");
		return false;
	}

	ret = rtk_dprx_misc_check(dprx);
	if (ret != DPRX_NO_ERR) {
		dev_info(dprx->dev, "stable_detect: Misc Change, ret=%d\n", ret);
		return false;
	}

	ret = rtk_dprx_fifo_check(dprx, _DP_FIFO_POLLING_CHECK);
	if (ret != DPRX_NO_ERR) {
		dev_info(dprx->dev, "stable_detect: Fifo Under/Overflow, ret=%d\n", ret);
		return false;
	}

	if (rtk_dprx_msa_active_change(dprx) == true) {
		dev_info(dprx->dev, "stable_detect: MSA Timing Change\n");
		return false;
	}

	if (dprx->mac_dat.en_free_sync &&
		rtk_dprx_get_free_sync_status_change(dprx) == true) {
		return false;
	}

	/* Clear CPIRQ Flag */
	rtk_dprx_check_hdcp_cp_irq_status(dprx);

	rtk_dprx_calculate_crc(dprx);

	rtk_dprx_sdp_data_detect(dprx);

	return true;
}

/**
 * rtk_dprx_check_vgip_vs_bypass - Check VGIP VS bypass for DP
 */
static bool __maybe_unused rtk_dprx_check_vgip_vs_bypass(struct rtk_dprx *dprx)
{
	u32 h_start;
	u32 v_start;

	/* Get Hstart */
	h_start = dprx->rbus_ops->get_word(PB5_43_MN_DPF_HST_M);

	/* Get Vstart */
	v_start = dprx->rbus_ops->get_word(PB5_4B_MN_DPF_VST_M);

	/* Check for nVedia 2560x1440@144Hz timing (reduced blanking) */
	if ((h_start < _VGIP_IH_CAPTURE_MIN_VALUE) ||
	   (v_start <= _VGIP_IV_CAPTURE_MIN_VALUE)) {
		dev_info(dprx->dev, "VGIP VS Bypass Mode, h_start=%u\n", h_start);

		return true;
	}

	return false;
}

/**
 * rtk_dprx_set_no_video_stream_irq - Enable/Disable DP VB-ID[3] NoVideoStream_Flag irq
 */
static void  rtk_dprx_set_no_video_stream_irq(struct rtk_dprx *dprx, bool en)
{
	/* VB-ID[3] Change Flag */
	dprx->rbus_ops->set_bit(PB6_3B_DP_GLB_STATUS,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1), _BIT6);

	/* VB-ID[3] Change IRQ */
	dprx->rbus_ops->set_bit(PB6_3F_DP_IRQ_CTRL1,
		~_BIT4, (en ? _BIT4 : 0x00));
}

/**
 * rtk_dprx_input_pixel_mode - Check Rx Input Pixel Mode
 */
static enum RTK_PIXEL_MODE __maybe_unused rtk_dprx_input_pixel_mode(struct rtk_dprx *dprx)
{
	if (dprx->rbus_ops->get_bit(PB5_1F_SOURCE_SEL_4, _BIT0) == 0x00)
		return _DP_RX_MAC_ONE_PIXEL_MODE;

	return _DP_RX_MAC_TWO_PIXEL_MODE;
}

/**
 * rtk_dprx_interlace_mode_config - Dp Rx Interlace Mode Check
 */
static bool __maybe_unused rtk_dprx_interlace_mode_config(struct rtk_dprx *dprx, u8 reference)
{
	if (reference == _REF_VBID) {
		if (dprx->rbus_ops->get_bit(PB5_57_INTERLACE_MODE_CONFIG, (_BIT7 | _BIT6)) == _BIT7)
			return true;
	} else if (reference == _REF_BS_COUNTER) {
		if (dprx->rbus_ops->get_bit(PB5_57_INTERLACE_MODE_CONFIG, (_BIT7 | _BIT6)) == (_BIT7 | _BIT6))
			return true;
	}

	return false;
}

/**
 * rtk_dprx_cdr_check - Check Valid Lane CDR
 */
static bool rtk_dprx_cdr_check(struct rtk_dprx *dprx, u8 link_rate, u8 dpcd_lane)
{
	u8 lane0_map;
	u8 lane1_map;
	u8 lane2_map;
	u8 lane3_map;

	switch (dpcd_lane) {
	case _DP_ONE_LANE:
		lane0_map = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
		if (rtk_dprx_measure_lane_cdr_clk(dprx, link_rate, lane0_map) == true)
			return true;
		break;

	case _DP_TWO_LANE:
		lane0_map = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
		lane1_map = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_1);
		if ((rtk_dprx_measure_lane_cdr_clk(dprx, link_rate, lane0_map) == true) &&
		   (rtk_dprx_measure_lane_cdr_clk(dprx, link_rate, lane1_map) == true))
			return true;
		break;

	case _DP_FOUR_LANE:
	default:
		lane0_map = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
		lane1_map = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_1);
		lane2_map = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_2);
		lane3_map = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_3);
		if ((rtk_dprx_measure_lane_cdr_clk(dprx, link_rate, lane0_map) == true) &&
		   (rtk_dprx_measure_lane_cdr_clk(dprx, link_rate, lane1_map) == true) &&
		   (rtk_dprx_measure_lane_cdr_clk(dprx, link_rate, lane2_map) == true) &&
		   (rtk_dprx_measure_lane_cdr_clk(dprx, link_rate, lane3_map) == true))
			return true;
		break;
	}

	dev_info(dprx->dev, "CDR Unlock\n");

	return false;
}

/**
 * rtk_dprx_get_measure_link_info - Get DP Link Info from Measure Function
 */
static bool rtk_dprx_get_measure_link_info(struct rtk_dprx *dprx,
		struct rtk_dprx_stream_info *stream_info)
{
	int ret;
	u32 us_temp = 0;
	u32 delay_ms;
	u32 val_23_16;
	u32 val_15_8;
	u32 val_7_0;
	u8 lane0_map;

	/* Reset Interlace mode */
	rtk_dprx_interlace_reset(dprx);

	/* Enable Measurement */
	dprx->rbus_ops->set_bit(PB5_58_MN_MEAS_CTRL, ~(_BIT7 | _BIT5), 0x00);
	dprx->rbus_ops->set_bit(PB5_58_MN_MEAS_CTRL, ~(_BIT7 | _BIT5), _BIT7);

	ret = rtk_dprx_wait_flag(dprx, _DP_MEASURE_POLLING_TIMEOUT, PB5_58_MN_MEAS_CTRL, _BIT6);
	if (ret)
		return false;

	/*
	 * Delay 1 frame time be used to wait time passed for
	 * upstream dptx generated different picture size of
	 * the firt frame and second frame
	 */
	/* Pop up The Measured Result */
	rtk_dprx_set_measure_pop_up(dprx);

	/* Get BS To BS Count of Frame N */
	val_23_16 = dprx->rbus_ops->get_bit(PB5_58_MN_MEAS_CTRL, (_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0));
	val_15_8 = dprx->rbus_ops->get_byte(PB5_59_MN_MEAS_VLN_M);
	val_7_0 = dprx->rbus_ops->get_byte(PB5_5A_MN_MEAS_VLN_L);
	stream_info->link_info.VBsToBsCountN = (val_23_16 << 16) | (val_15_8 << 8) | val_7_0;

	/* Delay 1 Frame Time, unit is ms */
	delay_ms = (stream_info->link_info.VBsToBsCountN / _GDIPHY_RX_GDI_CLK_KHZ) + 2;
	if (delay_ms > 44)
		delay_ms = 44;
	usleep_range(delay_ms*1000, delay_ms*1000 + 100);

	/* Pop up The Measured Result */
	rtk_dprx_set_measure_pop_up(dprx);

	/* Get BS To BS Count of Frame N */
	val_23_16 = dprx->rbus_ops->get_bit(PB5_58_MN_MEAS_CTRL, (_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0));
	val_15_8 = dprx->rbus_ops->get_byte(PB5_59_MN_MEAS_VLN_M);
	val_7_0 = dprx->rbus_ops->get_byte(PB5_5A_MN_MEAS_VLN_L);
	stream_info->link_info.VBsToBsCountN = (val_23_16 << 16) | (val_15_8 << 8) | val_7_0;

	/* Delay 2ms To Get VBID */
	usleep_range(2000, 2050);

	/* Get Interlace Field Flag VBID[1] of Frame N */
	stream_info->link_info.InterlaceFieldN = dprx->rbus_ops->get_bit(PB6_01_DP_VBID, _BIT1);

	/* Delay 1 Frame Time, unit is ms */
	delay_ms = (stream_info->link_info.VBsToBsCountN / _GDIPHY_RX_GDI_CLK_KHZ) + 2;
	if (delay_ms > 44)
		delay_ms = 44;
	usleep_range(delay_ms*1000, delay_ms*1000 + 100);

	/* Pop up The Measured Result */
	rtk_dprx_set_measure_pop_up(dprx);

	/* Get BS To BS Count of Frame N+1 */
	val_23_16 = dprx->rbus_ops->get_bit(PB5_58_MN_MEAS_CTRL, (_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0));
	val_15_8 = dprx->rbus_ops->get_byte(PB5_59_MN_MEAS_VLN_M);
	val_7_0 = dprx->rbus_ops->get_byte(PB5_5A_MN_MEAS_VLN_L);
	stream_info->link_info.VBsToBsCountN1 = (val_23_16 << 16) | (val_15_8 << 8) | val_7_0;

	/* Get Interlace Field Flag VBID[1] of Frame N+1 */
	stream_info->link_info.InterlaceFieldN1 = dprx->rbus_ops->get_bit(PB6_01_DP_VBID, _BIT1);

	/* Get Interlace HW Detect Result */
	stream_info->link_info.HwInterlaceDetect = dprx->rbus_ops->get_bit(PB5_57_INTERLACE_MODE_CONFIG, _BIT5);
	stream_info->link_info.HwFakeInterlaceDetect = dprx->rbus_ops->get_bit(PB5_57_INTERLACE_MODE_CONFIG, _BIT0);

	/* Disable Measurement */
	dprx->rbus_ops->set_bit(PB5_58_MN_MEAS_CTRL, ~(_BIT7 | _BIT5), 0x00);

	/* Get HBs2Bs Count By Link Clk / 2 */
	val_15_8 = dprx->rbus_ops->get_byte(PB5_5B_MN_MEAS_HLN_M);
	val_7_0 = dprx->rbus_ops->get_byte(PB5_5C_MN_MEAS_HLN_L);
	stream_info->link_info.HBsToBsCount = (val_15_8 << 8) | val_7_0;

	/* Check the value */
	if (rtk_dprx_measure_info_check(dprx, stream_info) == false)
		return false;

	/* Get Link Clock */
	lane0_map = dprx->phy_ops->get_lane_mapping(dprx, _DP_LANE_0);
	us_temp = rtk_dprx_signal_detect_measure_count(dprx, lane0_map,
		_DP_MEASURE_TARGET_CDR_CLOCK, _DP_MEASURE_PERIOD_2000_CYCLE);

	if (us_temp == 0) {
		switch (dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00)) {
		case _DP_LINK_HBR3:
			us_temp = _DP_RX_VCO_TARGET_COUNT_2000_HBR3_SAVED;
			break;

		case _DP_LINK_HBR2:
			us_temp = _DP_RX_VCO_TARGET_COUNT_2000_HBR2_SAVED;
			break;

		case _DP_LINK_HBR:
			us_temp = _DP_RX_VCO_TARGET_COUNT_2000_HBR_SAVED;
			break;

		case _DP_LINK_RBR:
		default:
			us_temp = _DP_RX_VCO_TARGET_COUNT_2000_RBR_SAVED;
			break;
		}
	}

	/* Get Link Clock, Unit is Hz */
	stream_info->link_info.LinkClockHz = (us_temp * _GDIPHY_RX_GDI_CLK_KHZ);

	/* Get Stream Clock */
	if (dprx->mac_dat.en_free_sync == true) {
		/* Get IVfreq */
		us_temp = ((u32)_GDIPHY_RX_GDI_CLK_KHZ * 1000 / (stream_info->link_info.VBsToBsCountN / 10));

		/* VTotal in line, Link Clk / 2 : (Link Rate * 27 / 2) */
		stream_info->timing_info.VTotal = rtk_dprx_compute_mul_div(dprx, stream_info->link_info.VBsToBsCountN, (stream_info->link_info.LinkClockHz / 1000 / 2), stream_info->link_info.HBsToBsCount / _GDIPHY_RX_GDI_CLK_KHZ);

		/* Calculate Stream Clock (Unit is Hz) */
		stream_info->link_info.StreamClockHz = stream_info->link_info.Mvid * (stream_info->link_info.LinkClockHz / stream_info->link_info.Nvid);
		stream_info->link_info.StreamClockHz += stream_info->link_info.LinkClockHz % stream_info->link_info.Nvid * 100 / stream_info->link_info.Nvid * stream_info->link_info.Mvid / 100;

		/* HTotal in Pixel Clk, Link Clk / 2 : (Link Rate * 27 / 2) */
		stream_info->timing_info.HTotal = rtk_dprx_compute_mul_div(dprx, stream_info->link_info.HBsToBsCount, (stream_info->link_info.StreamClockHz / 1000), stream_info->link_info.LinkClockHz / 1000 / 2);

		rtk_dprx_set_drr_msa_for_lut(dprx, &stream_info->timing_info);

		/* Get New Stream Clock After Enlarging HTotal */
		stream_info->link_info.StreamClockHz = rtk_dprx_compute_mul_div(dprx, stream_info->link_info.StreamClockHz, stream_info->timing_info.HTotal, (stream_info->timing_info.HTotal - GET_DP_MAC_RX0_H_PORCH_ENLARGE()));

		/* Save original Pixel Clock before tracking margin reduction */
		stream_info->link_info.PixelClockHz = stream_info->link_info.StreamClockHz;

		/* Initial Value Need to be Lower than the Target Value for Tracking (Margin is 0.07%) */
		stream_info->link_info.StreamClockHz = rtk_dprx_compute_mul_div(dprx, stream_info->link_info.StreamClockHz, 9993, 10000);
	} else {
		/* Calculate Stream Clock (Unit is Hz) */
		stream_info->link_info.StreamClockHz = (u64)stream_info->timing_info.HTotal * stream_info->timing_info.VTotal * _GDIPHY_RX_GDI_CLK_KHZ * 1000 / stream_info->link_info.VBsToBsCountN;

		dev_info(dprx->dev, "Target Stream Clock=%u\n", stream_info->link_info.StreamClockHz);

		/* Save original Pixel Clock before tracking margin reduction */
		stream_info->link_info.PixelClockHz = stream_info->link_info.StreamClockHz;

		/* Initial Value Need to be Lower than the Target Value for Tracking (Margin is 0.07%) */
		stream_info->link_info.StreamClockHz = rtk_dprx_compute_mul_div(dprx, stream_info->link_info.StreamClockHz, 9993, 10000);
	}

	dev_info(dprx->dev, "VBs2Bs N=%u\n", stream_info->link_info.VBsToBsCountN);
	dev_info(dprx->dev, "VBs2Bs N+1=%u\n", stream_info->link_info.VBsToBsCountN1);
	dev_info(dprx->dev, "HBs2Bs=%u\n", stream_info->link_info.HBsToBsCount);
	dev_info(dprx->dev, "Link Clock=%u\n", stream_info->link_info.LinkClockHz);
	dev_info(dprx->dev, "Mvid=0x%x\n", stream_info->link_info.Mvid);
	dev_info(dprx->dev, "Nvid=0x%x\n", stream_info->link_info.Nvid);
	dev_info(dprx->dev, "Initial Stream Clock=%u\n", stream_info->link_info.StreamClockHz);

	return true;
}

/**
 * rtk_dprx_display_format_setting - DP Display Format Generator Setting
 */
static bool rtk_dprx_display_format_setting(struct rtk_dprx *dprx,
		struct rtk_dprx_stream_info *stream_info)
{
	u32 temp[2];

	/* Display Format Generator Reset */

	/* Disable Generate Display Format */
	dprx->rbus_ops->set_bit(PB5_30_DPF_CTRL_0, ~_BIT7, 0x00);

	/* Display Format Generator Reset */
	dprx->rbus_ops->set_bit(PB5_1E_MAC_DIG_RESET_CTRL, ~_BIT7, _BIT7);
	dprx->rbus_ops->set_bit(PB5_1E_MAC_DIG_RESET_CTRL, ~_BIT7, 0x00);

	if (dprx->mac_dat.en_free_sync)
		goto skip_adjust;

	/* Adjust Timing Info */
	/* Check if MSA HSW is more than Hstart */
	if (stream_info->timing_info.HSWidth >= stream_info->timing_info.HStart) {
		/* Check if _DE_ONLY_MODE_HSW is more than Hstart */
		if (stream_info->timing_info.HStart < _DE_ONLY_MODE_HSW) {
			/* Adjust HSW = Hstart - 2 */
			stream_info->timing_info.HSWidth = stream_info->timing_info.HStart - 2;
		} else {
			/* Adjust HSW = _DE_ONLY_MODE_HSW */
			stream_info->timing_info.HSWidth = _DE_ONLY_MODE_HSW;
		}
	}

	/* Get HSW min by Measure clock */
	/* min_pixels = COUNTER * (PixelClockHz / RefClockHz): minimum measurable HSW in pixels */
	temp[0] = (u32)(((u64)_DP_HSYNC_WIDTH_MEASURE_COUNTER *
		stream_info->link_info.PixelClockHz) /
		(_GDIPHY_RX_GDI_CLK_KHZ * 1000)) + 1;

	/* Get Current H Blanking */
	temp[1] = stream_info->timing_info.HTotal - stream_info->timing_info.HWidth;

	/* Check if HSW is less thane HSW min */
	if (stream_info->timing_info.HSWidth <= temp[0]) {
		/* Check if H Blanking is more than HSW min */
		if ((temp[1]) >= temp[0]) {
			/* Adjust HSW = HSW min */
			stream_info->timing_info.HSWidth = temp[0];
		} else {
			/* Adjust HSW = Max H Porch */
			stream_info->timing_info.HSWidth = temp[1];
		}
	}

	/* Adjust VStart because V front porch must not be less than 2 line in scaling down block */
	stream_info->timing_info.VStart = stream_info->timing_info.VTotal - stream_info->timing_info.VHeight - dprx->mac_dat.vfront_porch;

skip_adjust:

	/* Display Format Timing Setting */

	/* Set HTotal */
	dprx->rbus_ops->set_byte(PB5_41_MN_DPF_HTT_M, HIBYTE(stream_info->timing_info.HTotal));
	dprx->rbus_ops->set_byte(PB5_42_MN_DPF_HTT_L, LOBYTE(stream_info->timing_info.HTotal));

	/* Set HStart */
	dprx->rbus_ops->set_byte(PB5_43_MN_DPF_HST_M, HIBYTE(stream_info->timing_info.HStart));
	dprx->rbus_ops->set_byte(PB5_44_MN_DPF_HST_L, LOBYTE(stream_info->timing_info.HStart));

	/* Set HWidth */
	dprx->rbus_ops->set_byte(PB5_45_MN_DPF_HWD_M, HIBYTE(stream_info->timing_info.HWidth));
	dprx->rbus_ops->set_byte(PB5_46_MN_DPF_HWD_L, LOBYTE(stream_info->timing_info.HWidth));

	/* Set HSW */
	dprx->rbus_ops->set_byte(PB5_47_MN_DPF_HSW_M, HIBYTE(stream_info->timing_info.HSWidth));
	dprx->rbus_ops->set_byte(PB5_48_MN_DPF_HSW_L, LOBYTE(stream_info->timing_info.HSWidth));

	/* Set HSP = Positive */
	dprx->rbus_ops->set_bit(PB5_47_MN_DPF_HSW_M, ~_BIT7, ((u8)_SYNC_POLARITY_POSITIVE) << 7);

	/* Set Vtotal */
	dprx->rbus_ops->set_byte(PB5_49_MN_DPF_VTT_M, HIBYTE(stream_info->timing_info.VTotal));
	dprx->rbus_ops->set_byte(PB5_4A_MN_DPF_VTT_L, LOBYTE(stream_info->timing_info.VTotal));

	/* Set VStart */
	dprx->rbus_ops->set_byte(PB5_4B_MN_DPF_VST_M, HIBYTE(stream_info->timing_info.VStart));
	dprx->rbus_ops->set_byte(PB5_4C_MN_DPF_VST_L, LOBYTE(stream_info->timing_info.VStart));

	/* Set VHeight */
	dprx->rbus_ops->set_byte(PB5_4D_MN_DPF_VHT_M, HIBYTE(stream_info->timing_info.VHeight));
	dprx->rbus_ops->set_byte(PB5_4E_MN_DPF_VHT_L, LOBYTE(stream_info->timing_info.VHeight));

	/* Set VSW */
	dprx->rbus_ops->set_byte(PB5_4F_MN_DPF_VSW_M, HIBYTE(stream_info->timing_info.VSWidth));
	dprx->rbus_ops->set_byte(PB5_50_MN_DPF_VSW_L, LOBYTE(stream_info->timing_info.VSWidth));

	/* Set VSP = Positive */
	dprx->rbus_ops->set_bit(PB5_4F_MN_DPF_VSW_M, ~_BIT7, ((u8)_SYNC_POLARITY_POSITIVE) << 7);

	/* Set Color Format */
	rtk_dprx_video_setting(dprx, dprx->mac_dat.color_space, dprx->mac_dat.pre_color_depth);

	/* Display Format Generator Setting */
	if (dprx->mac_dat.en_free_sync == true) {
		/* DE Only Mode */

		/* Set Vsync Front Porch for DE Only Mode */
		dprx->rbus_ops->set_byte(PB5_40_VS_FRONT_PORCH, dprx->mac_dat.vfront_porch & 0xFF);

		/* Set BS to HS Delay */
		rtk_dprx_set_bs_to_hs_delay(dprx, &stream_info->timing_info);

		/* Enable DP Freesync Mode(DRR Mode) */
		dprx->rbus_ops->set_bit(PB5_90_DP_RSV0, ~_BIT0, _BIT0);

		/* Set DE Only Mode */
		dprx->rbus_ops->set_bit(PB5_30_DPF_CTRL_0, ~(_BIT5 | _BIT4), _BIT4);

		dev_info(dprx->dev, "Freesync Mode Enabled, DPF_CTRL_0=0x%02x\n",
			dprx->rbus_ops->get_byte(PB5_30_DPF_CTRL_0));
	} else {
		/* Full Last Line Mode */

		/* Set BS to VS Delay */
		rtk_dprx_set_bs_to_vs_delay(dprx, stream_info);

		/* Disable DP Freesync Mode(DRR mode) */
		dprx->rbus_ops->set_bit(PB5_90_DP_RSV0, ~_BIT0, 0x00);

		/* Set frame sync mode */
		dprx->rbus_ops->set_bit(PB5_30_DPF_CTRL_0, ~(_BIT5 | _BIT4), _BIT5);
	}

	/* Choose VS Rising to Reset FIFO */
	dprx->rbus_ops->set_bit(PB5_21_PG_CTRL_1,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT0), 0x00);

	/* Solve Abnormal Full Flag and Max. FIFO Level While Resetting by VSync */
	dprx->rbus_ops->set_bit(PB5_26_SRAM_BIST_1, ~_BIT7, _BIT7);

	return true;
}

/**
 * rtk_dprx_fifo_check - DP Video Fifo Check
 */
static int rtk_dprx_fifo_check(struct rtk_dprx *dprx,
	enum RTK_DP_FIFO_CHECK_CONDITION condition)
{
	int ret;
	u32 count;

	if (condition == _DP_FIFO_DELAY_CHECK) {
		/*
		 * When tracking hardware is non-functional, PLL free-runs
		 * without rate compensation. The resulting frequency mismatch
		 * causes per-frame FIFO overflow that the 144ms delay check
		 * will always detect. Skip the strict monitoring since FIFO
		 * resets at VS Rising each frame, keeping overflow bounded.
		 */
		if (dprx->mac_dat.tracking_disabled)
			goto fifo_ok;

		dprx->rbus_ops->set_bit(PB5_21_PG_CTRL_1,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT0),
			(_BIT4 | _BIT2 | _BIT0));

		msleep(_DP_TWO_FRAME_TIME_MAX);

		count = 0;
		while (count < 6) {
			usleep_range(10000, 11000);

			if ((dprx->rbus_ops->get_bit(PB5_21_PG_CTRL_1, _BIT4) == _BIT4) ||
				(dprx->rbus_ops->get_bit(PB5_21_PG_CTRL_1, _BIT2) == _BIT2)) {
				ret = FIFO_DELAY_CHECK_ERR;
				goto exit;
			}

			count++;
		}
	} else {
		if ((dprx->rbus_ops->get_bit(PB5_21_PG_CTRL_1, _BIT4) == _BIT4) ||
			(dprx->rbus_ops->get_bit(PB5_21_PG_CTRL_1, _BIT2) == _BIT2)) {
			ret = FIFO_POLLING_CHECK_ERR;
			goto exit;
		}
	}

fifo_ok:
	if (dprx->rbus_ops->get_bit(PB6_3F_DP_IRQ_CTRL1, _BIT4) == 0x00) {
		/* Enable DP No Video Stream IRQ */
		rtk_dprx_set_no_video_stream_irq(dprx, true);
	}

	if (dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x05, _BIT0) == 0x00) {
		/* Set DP Receive Port0 In Sync */
		dprx->aux_ops->set_sink_status(dprx, _DP_SINK_REVEICE_PORT0, _DP_SINK_IN_SYNC);
	}

	ret = DPRX_NO_ERR;
exit:
	return ret;
}

/**
 * rtk_dprx_measure_lane_cdr_clk - check DP CDR locked or unlocked
 *
 * @return: true-Locked; false-Unlocked
 */
static bool rtk_dprx_measure_lane_cdr_clk(struct rtk_dprx *dprx,
	u8 link_rate, u8 lane_number)
{
	u32 cdr_clk_count = 0;

	switch (link_rate) {
	case _DP_LINK_HBR3:
		if ((abs(dprx->phy_ops->get_target_clock(dprx, lane_number) - _DP_RX_VCO_TARGET_COUNT_1000_HBR3_SAVED)) > (_DP_RX_VCO_TARGET_COUNT_1000_HBR3_SAVED >> 4))
			return true;
		break;

	case _DP_LINK_HBR2:
		if ((abs(dprx->phy_ops->get_target_clock(dprx, lane_number) - _DP_RX_VCO_TARGET_COUNT_1000_HBR2_SAVED)) > (_DP_RX_VCO_TARGET_COUNT_1000_HBR2_SAVED >> 4))
			return true;
		break;

	case _DP_LINK_HBR:
		if ((abs(dprx->phy_ops->get_target_clock(dprx, lane_number) - _DP_RX_VCO_TARGET_COUNT_1000_HBR_SAVED)) > (_DP_RX_VCO_TARGET_COUNT_1000_HBR_SAVED >> 4))
			return true;
		break;

	case _DP_LINK_RBR:
	default:
		if ((abs(dprx->phy_ops->get_target_clock(dprx, lane_number) - _DP_RX_VCO_TARGET_COUNT_1000_RBR_SAVED)) > (_DP_RX_VCO_TARGET_COUNT_1000_RBR_SAVED >> 4))
			return true;
		break;
	}

	cdr_clk_count = rtk_dprx_signal_detect_measure_count(dprx,
		lane_number, _DP_MEASURE_TARGET_CDR_CLOCK, _DP_MEASURE_PERIOD_1000_CYCLE);

	if (cdr_clk_count == 0)
		return false;

	if (abs(dprx->phy_ops->get_target_clock(dprx, lane_number) - cdr_clk_count) > (dprx->phy_ops->get_target_clock(dprx, lane_number) >> 7))
		return false;

	return true;
}

/**
 * rtk_dprx_set_fifo_irq - Enable/Disable DP Fifo Under/OverFlow IRQ
 */
static void rtk_dprx_set_fifo_irq(struct rtk_dprx *dprx, bool en)
{
	dprx->rbus_ops->set_bit(PB6_3E_DP_IRQ_CTRL0,
		~(_BIT1 | _BIT0), (en ? (_BIT1 | _BIT0) : 0x00));
}

/**
 * rtk_dprx_set_fifo_wd - Enable/Disable DP Fifo Under/OverFlow Watch Dog
 */
static void rtk_dprx_set_fifo_wd(struct rtk_dprx *dprx, bool en)
{
	dprx->rbus_ops->set_bit(PB6_3C_DP_WD_CTRL_0,
		~(_BIT5 | _BIT4), (en ? (_BIT5 | _BIT4) : 0x00));
}

/**
 * rtk_dprx_get_fifo_wd_status - Get DP Fifo Watch Dog Enable Status
 */
static bool __maybe_unused rtk_dprx_get_fifo_wd_status(struct rtk_dprx *dprx)
{
	u8 wd_ctrl;
	bool enabled;

	wd_ctrl = dprx->rbus_ops->get_bit(PB6_3C_DP_WD_CTRL_0, _BIT5);
	enabled = (wd_ctrl == _BIT5) ? true : false;

	return enabled;
}

/**
 * rtk_dprx_set_hdcp_mode - Set HDCP Mode
 */
static void __maybe_unused rtk_dprx_set_hdcp_mode(struct rtk_dprx *dprx, enum RTK_HDCP_TYPE type)
{
	dprx->rbus_ops->set_bit(PB_1A_HDCP_IRQ, ~_BIT1, 0x00);
}

/**
 * rtk_dprx_interlace_reset - Reset Interlace Mode
 */
static void rtk_dprx_interlace_reset(struct rtk_dprx *dprx)
{
	/*
	 * Interlace mode disable, refer to VBID[1], Field Inverse Disable,
	 * Clear Hwardware Detect Interlace Flag, Clear Fake Interlace Flag
	 */
	dprx->rbus_ops->set_bit(PB5_57_INTERLACE_MODE_CONFIG,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT0), (_BIT5 | _BIT0));

	/* Enable Field Sync by VBID */
	dprx->rbus_ops->set_bit(PB5_91_DP_RSV1, ~(_BIT6 | _BIT0), 0x00);

	/* Set Vtt odd fw mode */
	// TODO: SET_DP_RX0_INTERLACE_VTT_FW_MODE(_ODD);
}

/**
 * rtk_dprx_set_measure_pop_up - Get VBID Information
 */
static void rtk_dprx_set_measure_pop_up(struct rtk_dprx *dprx)
{
	dprx->rbus_ops->set_bit(PB5_58_MN_MEAS_CTRL, ~_BIT5, _BIT5);
	usleep_range(5, 6);
}

/**
 * rtk_dprx_get_sdp_buffer_type - Get Spd HW Buffer Type
 */
enum RTK_DP_SDP_BUFF rtk_dprx_get_sdp_buffer_type(struct rtk_dprx *dprx,
	enum RTK_DP_SDP_TYPE sdp_type)
{
	enum RTK_DP_SDP_BUFF buf_type;

	switch (sdp_type) {
	case _DP_SDP_TYPE_INFOFRAME_HDR:
		buf_type = _DP_SDP_BUFF_HDR;
		break;
	case _DP_SDP_TYPE_INFOFRAME_SPD:
		buf_type = _DP_SDP_BUFF_SPD;
		break;
	case _DP_SDP_TYPE_INFOFRAME_AUDIO:
		buf_type = _DP_SDP_BUFF_ISRC;
		break;
	case _DP_SDP_TYPE_VSC:
		buf_type = _DP_SDP_BUFF_VSC;
		break;
	default:
		buf_type = _DP_SDP_BUFF_NONE;
		break;
	}

	return buf_type;
}

/**
 * rtk_dprx_set_porch_color_rgb
 */
static void rtk_dprx_set_porch_color_rgb(struct rtk_dprx *dprx)
{
	dprx->rbus_ops->set_byte(PB5_51_MN_DPF_BG_RED_M, 0x00);
	dprx->rbus_ops->set_byte(PB5_52_MN_DPF_BG_RED_L, 0x00);
	dprx->rbus_ops->set_byte(PB5_53_MN_DPF_BG_GRN_M, 0x00);
	dprx->rbus_ops->set_byte(PB5_54_MN_DPF_BG_GRN_L, 0x00);
	dprx->rbus_ops->set_byte(PB5_55_MN_DPF_BG_BLU_M, 0x00);
	dprx->rbus_ops->set_byte(PB5_56_MN_DPF_BG_BLU_L, 0x00);
}

/**
 * rtk_dprx_set_porch_color_ycc_limit
 */
static void rtk_dprx_set_porch_color_ycc_limit(struct rtk_dprx *dprx,
	enum RTK_DP_COLOR_SPACE color_space)
{
	if (color_space == _COLOR_SPACE_YCBCR420) {
		dprx->rbus_ops->set_bit(PB5_51_MN_DPF_BG_RED_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT0);
		dprx->rbus_ops->set_byte(PB5_52_MN_DPF_BG_RED_L, 0x00);
		dprx->rbus_ops->set_bit(PB5_53_MN_DPF_BG_GRN_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT0);
		dprx->rbus_ops->set_byte(PB5_54_MN_DPF_BG_GRN_L, 0x00);
		dprx->rbus_ops->set_bit(PB5_55_MN_DPF_BG_BLU_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT3);
		dprx->rbus_ops->set_byte(PB5_56_MN_DPF_BG_BLU_L, 0x00);
	} else {
		dprx->rbus_ops->set_bit(PB5_51_MN_DPF_BG_RED_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT3);
		dprx->rbus_ops->set_byte(PB5_52_MN_DPF_BG_RED_L, 0x00);
		dprx->rbus_ops->set_bit(PB5_53_MN_DPF_BG_GRN_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT0);
		dprx->rbus_ops->set_byte(PB5_54_MN_DPF_BG_GRN_L, 0x00);
		dprx->rbus_ops->set_bit(PB5_55_MN_DPF_BG_BLU_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT3);
		dprx->rbus_ops->set_byte(PB5_56_MN_DPF_BG_BLU_L, 0x00);
	}
}

/**
 * rtk_dprx_set_porch_color_ycc_full
 */
static void rtk_dprx_set_porch_color_ycc_full(struct rtk_dprx *dprx,
	enum RTK_DP_COLOR_SPACE color_space)
{
	if (color_space == _COLOR_SPACE_YCBCR420) {
		dprx->rbus_ops->set_bit(PB5_51_MN_DPF_BG_RED_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT0);
		dprx->rbus_ops->set_byte(PB5_52_MN_DPF_BG_RED_L, 0x00);
		dprx->rbus_ops->set_bit(PB5_53_MN_DPF_BG_GRN_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT0);
		dprx->rbus_ops->set_byte(PB5_54_MN_DPF_BG_GRN_L, 0x00);
		dprx->rbus_ops->set_bit(PB5_55_MN_DPF_BG_BLU_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT3);
		dprx->rbus_ops->set_byte(PB5_56_MN_DPF_BG_BLU_L, 0x00);
	} else {
		dprx->rbus_ops->set_bit(PB5_51_MN_DPF_BG_RED_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT3);
		dprx->rbus_ops->set_byte(PB5_52_MN_DPF_BG_RED_L, 0x00);
		dprx->rbus_ops->set_bit(PB5_53_MN_DPF_BG_GRN_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), 0x00);
		dprx->rbus_ops->set_byte(PB5_54_MN_DPF_BG_GRN_L, 0x00);
		dprx->rbus_ops->set_bit(PB5_55_MN_DPF_BG_BLU_M, ~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT3);
		dprx->rbus_ops->set_byte(PB5_56_MN_DPF_BG_BLU_L, 0x00);
	}
}

static int rtk_dprx_wait_flag(struct rtk_dprx *dprx,
	u32 timeout_ms, u32 reg, u32 bit_mask)
{
	u32 i;
	u32 reg_val;

	for (i = 0; i < timeout_ms; i++) {
		reg_val = (u32)dprx->rbus_ops->get_byte(reg);
		if (reg_val & bit_mask)
			break;

		usleep_range(1000, 1050);
	}

	if (i >= timeout_ms) {
		dev_err(dprx->dev, "Wait HW state timeout, reg=0x%08x bit_mask=0x%08x timeout_ms=%u\n",
			reg, bit_mask, timeout_ms);
		return IP_HW_STATE_TIMEOUT;
	}

	return DPRX_NO_ERR;
}

/**
 * rtk_dprx_crt_clk_init
 */
static int rtk_dprx_crt_clk_init(struct rtk_dprx *dprx)
{

	if (dprx->crt_clk_inited)
		return DPRX_NO_ERR;

	if (IS_ERR_OR_NULL(dprx->clk_dprx) || IS_ERR_OR_NULL(dprx->clk_dprx)) {
		dev_err(dprx->dev, "Failed to set clk/crt\n");
		return CRT_CLK_INIT_ERR;
	}

	clk_prepare_enable(dprx->clk_dprx);
	clk_disable(dprx->clk_dprx);

	reset_control_assert(dprx->reset_dprx);
	reset_control_deassert(dprx->reset_dprx);

	clk_enable(dprx->clk_dprx);

	dprx->crt_clk_inited = true;

	dev_info(dprx->dev, "crt_clk_inited done\n");

	return DPRX_NO_ERR;
}

/**
 * rtk_dprx_crt_clk_deinit
 */
static int rtk_dprx_crt_clk_deinit(struct rtk_dprx *dprx)
{

	if (!dprx->crt_clk_inited)
		return DPRX_NO_ERR;

	clk_disable_unprepare(dprx->clk_dprx);
	reset_control_assert(dprx->reset_dprx);

	dprx->crt_clk_inited = false;

	return DPRX_NO_ERR;
}

static const struct rtk_dprx_mac_ops dprx_mac_ops = {
	.crt_clk_init = rtk_dprx_crt_clk_init,
	.crt_clk_deinit = rtk_dprx_crt_clk_deinit,
	.mac_reset = rtk_dprx_reset,
	.mac_initial = rtk_dprx_mac_initial,
	.sdp_initial = rtk_dprx_sdp_initial,
	.decode_error_count_reset = rtk_dprx_decode_error_count_reset,
	.lane_count_set = rtk_dprx_lane_count_set,
	.pre_detect = rtk_dprx_normal_pre_detect,
	.scan_input_port = rtk_dprx_scan_input_port,
	.fifo_check = rtk_dprx_fifo_check,
};

int rtk_dprx_mac_init(struct rtk_dprx *dprx)
{

	dprx->mac_ops = &dprx_mac_ops;

	return 0;
}
