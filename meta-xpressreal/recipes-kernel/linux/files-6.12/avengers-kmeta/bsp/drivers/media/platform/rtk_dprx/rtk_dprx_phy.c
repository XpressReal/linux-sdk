// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_dprx.h"

// TODO: FIXME
#define _DP_PHY_RX0_D0_LANE0 0
#define _DP_PHY_RX0_D0_LANE1 1
#define _DP_PHY_RX0_D0_LANE2 2
#define _DP_PHY_RX0_D0_LANE3 3

/**
 * rtk_dprx_get_target_clock - Get PHY CDR Clock
 */
static s32 rtk_dprx_get_target_clock(struct rtk_dprx *dprx, u8 lane_number)
{
	// TODO: FIXME
	return 0;
}

/**
 * rtk_dprx_get_phy_cts_flag - DP PHY CTS Process
 */
static bool rtk_dprx_get_phy_cts_flag(struct rtk_dprx *dprx)
{
	// TODO: FIXME
	return false;
}

/**
 * rtk_dprx_get_phy_cts_testlane - Get PHY CTS Test Lane
 */
static u8 rtk_dprx_get_phy_cts_testlane(struct rtk_dprx *dprx)
{
	return (dprx->phy_dat.cts_ctrl & (_BIT5 | _BIT4));
}

/**
 * rtk_dprx_get_lane_mapping - Get DP PHY Lane Mapping
 */
static u8 rtk_dprx_get_lane_mapping(struct rtk_dprx *dprx, enum RTK_DP_LANE lane)
{
	// TODO: FIXME
	switch (lane) {
	case _DP_LANE_3:
		return _DP_PHY_RX0_D0_LANE3;

	case _DP_LANE_2:
		return _DP_PHY_RX0_D0_LANE2;

	case _DP_LANE_1:
		return _DP_PHY_RX0_D0_LANE1;

	case _DP_LANE_0:
		fallthrough;
	default:
		return _DP_PHY_RX0_D0_LANE0;
	}
}

/**
 * rtk_dprx_phy_cts - DP PHY CTS Process
 */
static void rtk_dprx_phy_cts(struct rtk_dprx *dprx)
{
	// TODO: FIXME
}

/**
 * rtk_dprx_phy_cts_auto_mode - DP PHY CTS Process
 */
static void rtk_dprx_phy_cts_auto_mode(struct rtk_dprx *dprx)
{
	// TODO: FIXME
}

/**
 * rtk_dprx_rebuild_phy - Rebuilding DP PHY
 */
static void rtk_dprx_rebuild_phy(struct rtk_dprx *dprx, u8 link_rate, u8 lane_count)
{
	// TODO: Call USB phy driver
}

/**
 * rtk_dprx_signal_detect_initial - Initial Signal check
 */
static void rtk_dprx_signal_detect_initial(struct rtk_dprx *dprx, u8 link_rate, u8 leq_scan_val)
{
	// TODO: Call USB phy driver
}

/**
 * rtk_dprx_signal_detection - Enable/Disable Signal Detection
 */
static void rtk_dprx_signal_detection(struct rtk_dprx *dprx, bool enable)
{
	// TODO: Call USB phy driver
}

/**
 * rtk_dprx_set_lane_count - Set Link Training Lane Count
 */
static void rtk_dprx_set_lane_count(struct rtk_dprx *dprx, u8 lane_count)
{
	if ((lane_count != _DP_ONE_LANE) &&
		(lane_count != _DP_TWO_LANE) &&
		(lane_count != _DP_FOUR_LANE))
		return;

	// TODO: Call USB phy driver
	dprx->phy_dat.lane_count = lane_count;
}

/**
 * rtk_dprx_get_lane_count - Get Link Training Lane Count
 */
static u8 rtk_dprx_get_lane_count(struct rtk_dprx *dprx)
{
	return dprx->phy_dat.lane_count;
}

/**
 * rtk_dprx_set_link_rate - Set Link Training Link Rate
 */
static void rtk_dprx_set_link_rate(struct rtk_dprx *dprx, u8 link_rate)
{
	if ((link_rate != _DP_LINK_NONE) &&
		(link_rate != _DP_LINK_RBR) &&
		(link_rate != _DP_LINK_HBR) &&
		(link_rate != _DP_LINK_HBR2))
		return;

	// TODO: Call USB phy driver
	dprx->phy_dat.link_rate = link_rate;
}

/**
 * rtk_dprx_get_link_rate - Get Link Training Link Rate
 */
static u8 rtk_dprx_get_link_rate(struct rtk_dprx *dprx)
{
	return dprx->phy_dat.link_rate;
}

/**
 * rtk_dprx_set_training_pattern - Set Link Training Pattern
 * @pattern: DPCD 00102h TRAINING_PATTERN_SET Bit[3:0]
 */
static void rtk_dprx_set_training_pattern(struct rtk_dprx *dprx, u8 pattern)
{
	if ((pattern != _DP_TRAINING_PATTERN_END) &&
		(pattern != _DP_TRAINING_PATTERN_1) &&
		(pattern != _DP_TRAINING_PATTERN_2) &&
		(pattern != _DP_TRAINING_PATTERN_3) &&
		(pattern != _DP_TRAINING_PATTERN_4))
		return;

	// TODO: Call USB phy driver?
	dprx->phy_dat.training_pattern = pattern;
}

/**
 * rtk_dprx_get_training_pattern - Get Link Training Pattern
 */
static u8 rtk_dprx_get_training_pattern(struct rtk_dprx *dprx)
{
	return dprx->phy_dat.training_pattern;
}

/**
 * rtk_dprx_check_cts_tp1 - Check Dp PHY CTS Training Pattern 1
 *
 * @return: true-CTS TP1 Pass; false-CTS TP1 Fail
 */
static u8 rtk_dprx_check_cts_tp1(struct rtk_dprx *dprx)
{
	u8 link_rate;
	u8 test_sink;

	// TODO: ScalerDpAuxRx0ErrorCounterDisable_EXINT0();

	link_rate = rtk_dprx_get_link_rate(dprx);

	/* Link Rate */
	switch (link_rate) {
	case 0x1E:
		dprx->phy_dat.cts_ctrl = ((dprx->phy_dat.cts_ctrl & 0x3F) | (_BIT7 | _BIT6));
		break;
	case 0x14:
		dprx->phy_dat.cts_ctrl = ((dprx->phy_dat.cts_ctrl & 0x3F) | (_BIT7));
		break;
	case 0x0A:
		dprx->phy_dat.cts_ctrl = ((dprx->phy_dat.cts_ctrl & 0x3F) | (_BIT6));
		break;
	case 0x06:
	default:
		dprx->phy_dat.cts_ctrl = (dprx->phy_dat.cts_ctrl & 0x3F);
		break;
	}

	/* Lane Select */
	test_sink = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, (_BIT5 | _BIT4));
	switch (test_sink) {

	case 0x10:
		/* Source Lane1 */
		dprx->phy_dat.cts_ctrl = ((dprx->phy_dat.cts_ctrl & 0xCF) | (_DP_PHY_RX0_D0_LANE1 << 4));
		break;
	case 0x20:
		/* Source Lane2 */
		dprx->phy_dat.cts_ctrl = ((dprx->phy_dat.cts_ctrl & 0xCF) | (_DP_PHY_RX0_D0_LANE2 << 4));
		break;
	case 0x30:
		/* Source Lane3 */
		dprx->phy_dat.cts_ctrl = ((dprx->phy_dat.cts_ctrl & 0xCF) | (_DP_PHY_RX0_D0_LANE3 << 4));
		break;
	case 0x00:
	default:
		/* Source Lane0 */
		dprx->phy_dat.cts_ctrl = ((dprx->phy_dat.cts_ctrl & 0xCF) | (_DP_PHY_RX0_D0_LANE0 << 4));
		break;
	}

	// TODO: ScalerDpPhyRxPhyCtsTp1SetPhy_EXINT0(enumInputPort, _DP_NF_REF_XTAL);

	// TODO: return ScalerDpPhyRxPhyCtsTp1Check_EXINT0(enumInputPort);
	return false;
}

static const struct rtk_dprx_phy_ops dprx_phy_ops = {
	.get_target_clock = rtk_dprx_get_target_clock,
	.get_phy_cts_flag = rtk_dprx_get_phy_cts_flag,
	.get_phy_cts_testlane = rtk_dprx_get_phy_cts_testlane,
	.get_lane_mapping = rtk_dprx_get_lane_mapping,
	.phy_cts = rtk_dprx_phy_cts,
	.phy_cts_auto_mode = rtk_dprx_phy_cts_auto_mode,
	.rebuild_phy = rtk_dprx_rebuild_phy,
	.signal_detect_initial = rtk_dprx_signal_detect_initial,
	.signal_detection = rtk_dprx_signal_detection,
	.set_lane_count = rtk_dprx_set_lane_count,
	.get_lane_count = rtk_dprx_get_lane_count,
	.set_link_rate = rtk_dprx_set_link_rate,
	.get_link_rate = rtk_dprx_get_link_rate,
	.set_training_pattern = rtk_dprx_set_training_pattern,
	.get_training_pattern = rtk_dprx_get_training_pattern,
	.check_cts_tp1 = rtk_dprx_check_cts_tp1,
};

int rtk_dprx_phy_init(struct rtk_dprx *dprx)
{

	dprx->phy_ops = &dprx_phy_ops;

	return 0;
}
