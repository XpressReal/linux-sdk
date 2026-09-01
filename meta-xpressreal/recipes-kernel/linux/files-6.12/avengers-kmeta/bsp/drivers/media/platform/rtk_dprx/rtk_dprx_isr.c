// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_dprx.h"

/**
 * rtk_dprx_decode_error_count_reset - Reset 8b10b Error Count value
 */
static void rtk_dprx_decode_error_count_reset(struct rtk_dprx *dprx,
	enum RTK_DP_DECODE_METHOD decode_method)
{
	/* Reset 8b10b Error Count Value */
	dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL,
		~(_BIT2 | _BIT1 | _BIT0), 0x00);

	switch(decode_method) {
	case _DP_MAC_DECODE_METHOD_PRBS7:
		/* Reverse PRBS7 Pattern Gen */
		// TODO: ScalerDpMacRx0PrbsReverse_EXINT0(_ENABLE);

		/* Start Record PRBS7 Error Count Value */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL,
			~(_BIT2 | _BIT1 | _BIT0), _BIT0);
		break;

	case _DP_MAC_DECODE_METHOD_8B10B_DISPARITY:
		/* Start Record 8b10b or Disparity Error Count Value */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL,
			~(_BIT2 | _BIT1 | _BIT0), (_BIT2 | _BIT1 | _BIT0));
		break;

	default:
	case _DP_MAC_DECODE_METHOD_8B10B:
		/* Start Record 8b10b Error Count Value */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL,
			~(_BIT2 | _BIT1 | _BIT0), _BIT1);
		break;
	}
}

/**
 * rtk_dprx_phy_cts_error_counter_update - DP Error Counter Update
 */
static void rtk_dprx_phy_cts_error_counter_update(struct rtk_dprx *dprx)
{
	u8 link_bw_set = 0;
	u8 err_cnt_lane_sel = 0;
	u32 err_cnt = 0;

	link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
	if ((link_bw_set == _DP_LINK_RBR) || (link_bw_set == _DP_LINK_HBR)) {
		/* PRBS Pattern Follow PHY Lane */
		err_cnt_lane_sel = dprx->phy_ops->get_phy_cts_testlane(dprx);
	} else {
		/* TPS Pattern Follow MAC Lane */
		err_cnt_lane_sel = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, (_BIT5 | _BIT4));
	}

	/* Lane Select */
	dprx->rbus_ops->set_bit(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), err_cnt_lane_sel >> 1);

	/* Error Count Readout */
	err_cnt = ((dprx->rbus_ops->get_byte(PB_0B_BIST_PATTERN3)) << 8) |
		dprx->rbus_ops->get_byte(PB_0C_BIST_PATTERN4);

	err_cnt_lane_sel = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, (_BIT5 | _BIT4));
	switch (err_cnt_lane_sel) {
	case (_BIT5 | _BIT4):
		/* Store Lane3 Error */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x16, LOBYTE(err_cnt));
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x17,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), HIBYTE(err_cnt));
		break;

	case (_BIT5):
		/* Store Lane2 Error */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x14, LOBYTE(err_cnt));
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x15,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), HIBYTE(err_cnt));
		break;

	case (_BIT4):
		/* Store Lane1 Error */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x12, LOBYTE(err_cnt));
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x13,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
			HIBYTE(err_cnt));
		break;

	case 0x00:
	default:
		/* Store Lane0 Error */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x10, LOBYTE(err_cnt));
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x11,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
			HIBYTE(err_cnt));
		break;
	}

	link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
	/* Reset 8B/10B Error Counter */
	if ((link_bw_set == _DP_LINK_RBR) || (link_bw_set == _DP_LINK_HBR)) {
		/* Reset PRBS7 Error Counter */
		rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_PRBS7);
	} else {
		/* Reset 8B/10B Error Counter */
		rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_8B10B_DISPARITY);
	}
}

/**
 * rtk_dprx_error_counter_update - DP Error Counter Update
 */
static void rtk_dprx_error_counter_update(struct rtk_dprx *dprx)
{
	u8 bist_pattern3;
	u8 bist_pattern4;

	switch (dprx->phy_dat.lane_count) {
	case _DP_ONE_LANE:
		/* Store Lane0 Error */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL,
			~(_BIT4 | _BIT3), (_DP_LANE_0 << 3));
		bist_pattern4 = dprx->rbus_ops->get_byte_extint(PB_0C_BIST_PATTERN4);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x10, bist_pattern4);
		bist_pattern3 = dprx->rbus_ops->get_byte_extint(PB_0B_BIST_PATTERN3);
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x11,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), bist_pattern3);
		break;

	case _DP_TWO_LANE:
		/* Store Lane0 Error */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), (_DP_LANE_0 << 3));
		bist_pattern4 = dprx->rbus_ops-> get_byte_extint(PB_0C_BIST_PATTERN4);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x10, bist_pattern4);
		bist_pattern3 = dprx->rbus_ops->get_byte_extint(PB_0B_BIST_PATTERN3);
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x11,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), bist_pattern3);

		/* Store Lane1 Error */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), (_DP_LANE_1 << 3));
		bist_pattern4 = dprx->rbus_ops-> get_byte_extint(PB_0C_BIST_PATTERN4);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x12, bist_pattern4);
		bist_pattern3 = dprx->rbus_ops->get_byte_extint(PB_0B_BIST_PATTERN3);
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x13,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), bist_pattern3);
		break;

	case _DP_FOUR_LANE:
	default:
		/* Store Lane0 Error */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), (_DP_LANE_0 << 3));
		bist_pattern4 = dprx->rbus_ops-> get_byte_extint(PB_0C_BIST_PATTERN4);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x10, bist_pattern4);
		bist_pattern3 = dprx->rbus_ops->get_byte_extint(PB_0B_BIST_PATTERN3);
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x11,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), bist_pattern3);

		/* Store Lane1 Error */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), (_DP_LANE_1 << 3));
		bist_pattern4 = dprx->rbus_ops-> get_byte_extint(PB_0C_BIST_PATTERN4);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x12, bist_pattern4);
		bist_pattern3 = dprx->rbus_ops->get_byte_extint(PB_0B_BIST_PATTERN3);
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x13,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), bist_pattern3);

		/* Store Lane2 Error */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), (_DP_LANE_2 << 3));
		bist_pattern4 = dprx->rbus_ops-> get_byte_extint(PB_0C_BIST_PATTERN4);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x14, bist_pattern4);
		bist_pattern3 = dprx->rbus_ops->get_byte_extint(PB_0B_BIST_PATTERN3);
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x15,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), bist_pattern3);

		/* Store Lane3 Error */
		dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL, ~(_BIT4 | _BIT3), (_DP_LANE_3 << 3));
		bist_pattern4 = dprx->rbus_ops-> get_byte_extint(PB_0C_BIST_PATTERN4);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x16, bist_pattern4);
		bist_pattern3 = dprx->rbus_ops->get_byte_extint(PB_0B_BIST_PATTERN3);
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x17,
			~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), bist_pattern3);

		break;
	}

	/* Reset 8B/10B Error Counter */
	dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL,
        ~(_BIT2 | _BIT1 | _BIT0), 0x00);
	dprx->rbus_ops->set_bit_extint(PB_08_BIST_PATTERN_SEL,
        ~(_BIT2 | _BIT1 | _BIT0), _BIT1);
}

/**
 * rtk_dprx_wait_rcv - Wait for Aux Mac State switch to RCV_STANBY
 */
static void rtk_dprx_wait_rcv(struct rtk_dprx *dprx)
{
	u8 cnt_temp = 0;
	u8 tp1_occr;

	// TODO: FIXME
	for (cnt_temp = 0; cnt_temp < 60; cnt_temp++) {
		udelay(5);
		tp1_occr = dprx->rbus_ops->get_bit_extint(PB7_C7_TP1_OCCR, (_BIT2 | _BIT1 | _BIT0));
		if (tp1_occr == _BIT0)
			break;
	}
}

static bool rtk_dprx_symbol_err_cnt(struct rtk_dprx *dprx)
{
    bool value_d0 = false;
    u8 dpcd_addr_flag;
    u8 test_sink = 0;
    u8 link_bw_set = 0;

	/* Symbol Error Count Read IRQ */
    dpcd_addr_flag = dprx->rbus_ops->get_bit_extint(PB7_CC_DPCD_CONFIG_ADDR_FLAG, (_BIT5 | _BIT4));
	if (dpcd_addr_flag != (_BIT5 | _BIT4)) {
        /* Reset Read Error Count Flag Beside Symbol Error Count 00210h ~ 00217h */
        dprx->is_source_read_err_cnt = false;
        goto exit;
    }

    /* DP Source read 00210h ~ 00217h */
    /* Clear IRQ Flag */
    dprx->rbus_ops->set_bit_extint(PB7_CC_DPCD_CONFIG_ADDR_FLAG,
        ~(_BIT7 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),  _BIT4);

	if (dprx->is_source_read_err_cnt == false) {
        u8 auxch_st;

		/* Only Reply when State Machine is at Transmit Idle Standby State */
        auxch_st = dprx->rbus_ops->get_bit_extint(PB7_C7_TP1_OCCR, 0x7);
		if (auxch_st == 0x02) {
			/* Reset Aux FIFO */
            dprx->rbus_ops->set_bit_extint(PB7_DA_AUX_FIFO_RST,
                ~(_BIT6 | _BIT4 | _BIT1 | _BIT0), _BIT0);

            test_sink = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, _BIT7);
			if (test_sink == _BIT7)
                rtk_dprx_phy_cts_error_counter_update(dprx);
			else
				rtk_dprx_error_counter_update(dprx);

			/* Wait for RCV_STANBY in Order to Reply AUX DEFER */
			rtk_dprx_wait_rcv(dprx);

            dprx->is_source_read_err_cnt = true;
		} else {
            test_sink = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, _BIT7);
			if (test_sink == _BIT7) {
                link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
				if ((link_bw_set == _DP_LINK_RBR) || (link_bw_set == _DP_LINK_HBR)) {
					/* Reset PRBS7 Error Counter */
					rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_PRBS7);
				} else {
					/* Reset 8B/10B Error Counter */
					rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_8B10B_DISPARITY);
				}
			} else {
				/* Reset 8B/10B Error Counter */
				rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_8B10B);
			}
		}
	} else {
		/* Update Symbol Error Count by HW Auto Reply */
        dprx->is_source_read_err_cnt = false;

        test_sink = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, _BIT7);
		if (test_sink == _BIT7) {
            link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
			if((link_bw_set == 0x06) || (link_bw_set == 0x0A)) {
				/* Reset PRBS7 Error Counter */
				rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_PRBS7);
			} else {
				/* Reset 8B/10B Error Counter */
				rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_8B10B_DISPARITY);
			}
		} else {
			/* Reset 8B/10B Error Counter */
			rtk_dprx_decode_error_count_reset(dprx, _DP_MAC_DECODE_METHOD_8B10B);
		}
	}

	value_d0 = false;

exit:
    return value_d0;
}

/**
 * rtk_dprx_pre_int_handler - DP Interrupt Rx0 Pre-Handler
 */
static bool rtk_dprx_pre_int_handler(struct rtk_dprx *dprx)
{
	/* Aux Firmware Control -> Reply Defer */
	dprx->aux_ops->set_manual_mode(dprx);

	return true;
}

/**
 * rtk_dprx_lt_tps1 - Link Training LANEx_CR_DONE Sequence
 */
static void rtk_dprx_lt_tps1(struct rtk_dprx *dprx, u8 aux_dpcd_irq)
{
	u8 test_sink;

	dprx->phy_dat.backup_pd_link_status_flg = false;

	/* Check Source Only Write 0x102 without 0x103~0x106 */
	if ((aux_dpcd_irq & (_BIT7 | _BIT5)) == _BIT7) {
		if ((dprx->lt_status != _DP_NORMAL_TRAINING_PATTERN_1_PASS) &&
		   (dprx->lt_status != _DP_FAKE_TRAINING_PATTERN_1_PASS) &&
		   (dprx->lt_status != _DP_NORMAL_TRAINING_PATTERN_1_FAIL) &&
		   (dprx->lt_status != _DP_FAKE_TRAINING_PATTERN_1_FAIL)) {
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x01, 0x03, 0x00);
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x01, 0x04, 0x00);
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x01, 0x05, 0x00);
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x01, 0x06, 0x00);

			dprx->phy_dat.link_request01 = 0x00;
			dprx->phy_dat.link_request23 = 0x00;

			dprx->phy_dat.tp1_initial_done = true;
		}
	}

	test_sink = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x70, _BIT7);
	if (test_sink == _BIT7) {
		if (dprx->lt_status != _DP_NORMAL_TRAINING_PATTERN_1_PASS) {
			if (dprx->phy_ops->check_cts_tp1(dprx) == true)
				dprx->lt_status = _DP_NORMAL_TRAINING_PATTERN_1_PASS;
			else
				dprx->lt_status  = _DP_NORMAL_TRAINING_PATTERN_1_FAIL;
		}
	} else {
		if ((dprx->lt_status != _DP_NORMAL_TRAINING_PATTERN_1_PASS) &&
		   (dprx->lt_status != _DP_FAKE_TRAINING_PATTERN_1_PASS)) {
			if (GET_DP_PHY_RX0_FAKE_LINK_TRAINING() == _FALSE) {
				if (ScalerDpAuxRx0TrainingPattern1_EXINT0(_DP_NORMAL_LT) == _TRUE)
					dprx->lt_status = _DP_NORMAL_TRAINING_PATTERN_1_PASS;
				else
					dprx->lt_status = _DP_NORMAL_TRAINING_PATTERN_1_FAIL;
			} else {
				if (ScalerDpAuxRx0TrainingPattern1_EXINT0(_DP_FAKE_LT) == _TRUE)
					dprx->lt_status = _DP_FAKE_TRAINING_PATTERN_1_PASS;
				else
					dprx->lt_status = _DP_FAKE_TRAINING_PATTERN_1_FAIL;
			}
		}
	}
}

/**
 * rtk_dprx_lt_tps2_3_4 - Link Training LLANEx_CHANNEL_EQ_DONE Sequence
 */
static void rtk_dprx_lt_tps2_3_4(struct rtk_dprx *dprx)
{
	dprx->phy_dat.tp1_initial_done = false;

	if (ScalerDpAuxRx0GetDpcdBitInfo_EXINT0(0x00 0x02 0x70 _BIT7) == _BIT7)
	{
		if((dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_1_PASS) ||
		   (dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_2_FAIL))
		{
			if(ScalerDpAuxRx0PhyCtsTrainingPattern2_EXINT0() == _TRUE)
			{
				SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_NORMAL_TRAINING_PATTERN_2_PASS);
			}
			else
			{
				SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_NORMAL_TRAINING_PATTERN_2_FAIL);
			}
		}
	}
	else
	{
		if((GET_DP_PHY_RX0_FAKE_LINK_TRAINING() == _FALSE) &&
		   ((dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_1_PASS) ||
			(dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_2_FAIL)))
		{
			if(ScalerDpAuxRx0TrainingPattern2_EXINT0(_DP_NORMAL_LT) == _TRUE)
			{
				SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_NORMAL_TRAINING_PATTERN_2_PASS);

				ScalerDpMacRxLaneCountSet_EXINT0(enumInputPort _DP_LANE_AUTO_MODE);
			}
			else
			{
				SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_NORMAL_TRAINING_PATTERN_2_FAIL);
			}
		}
		else if((dprx->lt_status == _DP_FAKE_TRAINING_PATTERN_1_PASS) ||
				(dprx->lt_status == _DP_FAKE_TRAINING_PATTERN_2_FAIL) ||
				(dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_1_PASS) ||
				(dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_2_FAIL))
		{
			if(ScalerDpAuxRx0TrainingPattern2_EXINT0(_DP_FAKE_LT) == _TRUE)
			{
				SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_FAKE_TRAINING_PATTERN_2_PASS);
			}
			else
			{
				SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_FAKE_TRAINING_PATTERN_2_FAIL);
			}
		}
	}
}

/**
 * rtk_dprx_lt_end 
 */
static void rtk_dprx_lt_end(struct rtk_dprx *dprx)
{
	dprx->phy_dat.tp1_initial_done = false;

	if (ScalerDpAuxRx0GetDpcdBitInfo_EXINT0(0x00 0x02 0x70 _BIT7) == _BIT7) {
		if (dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_2_PASS) {
			ScalerDpAuxRx0PhyCtsTrainingPatternEnd_EXINT0();
			SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_NORMAL_LINK_TRAINING_PASS);
		}
	} else {
		if ((dprx->lt_status == _DP_NORMAL_LINK_TRAINING_PASS) ||
			(dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_END_REBUILD_PHY)) {
			ScalerDpAuxRx0TrainingPatternEnd_EXINT0();
		} else if ((dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_2_PASS) ||
			(dprx->lt_status == _DP_FAKE_TRAINING_PATTERN_2_PASS)) {
			ScalerDpAuxRx0SetDpcdBitWriteValue_EXINT0(0x00 0x06 0x00 ~(_BIT2 | _BIT1 | _BIT0) _BIT0);

			if (dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_2_PASS) {
				ScalerDpAuxRx0TrainingPatternEnd_EXINT0();
				{
					SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_NORMAL_TRAINING_PATTERN_END_REBUILD_PHY);

					ScalerTimerWDCancelTimerEvent_EXINT0(_SCALER_WD_TIMER_EVENT_RX0_DP_LINK_TRAINING_REBUILD_PHY);

					ScalerTimerWDActivateTimerEvent_EXINT0(30 _SCALER_WD_TIMER_EVENT_RX0_DP_LINK_TRAINING_REBUILD_PHY);
				}
			} else {
				SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_FAKE_LINK_TRAINING_PASS);
			}

			/* Set DP Receive Port0 In Sync */
			ScalerDpAuxRx0SinkStatusSet_EXINT0(_DP_SINK_REVEICE_PORT0 _DP_SINK_IN_SYNC);

			ScalerTimerCancelTimerEvent_EXINT0(_SCALER_TIMER_EVENT_DP_RX0_HDCP_LONG_HOTPLUG_EVENT);
		} else if ((dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_1_FAIL) ||
				(dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_2_FAIL) ||
				(dprx->lt_status == _DP_FAKE_TRAINING_PATTERN_1_FAIL) ||
				(dprx->lt_status == _DP_FAKE_TRAINING_PATTERN_2_FAIL)) {
			SET_DP_AUX_RX0_LINK_TRAINING_STATUS(_DP_LINK_TRAINING_FAIL);
		}
	}
}

/**
 * rtk_dprx_lt_int_handler - Link Config Field Interrupt Handler
 * Source has written DPCD 00100h to 00108h
 */
static bool rtk_dprx_lt_int_handler(struct rtk_dprx *dprx)
{
	bool value_d1 = false;
	u8 aux_dpcd_irq;
	u8 pattern;
	u8 link_bw_set = 0;
	u8 lane_count_set = 0;

	pattern = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x02) & 0x0F;
	dprx->phy_ops->set_training_pattern(dprx, pattern);

	/* Backup Source Write Continous 0x102 and 0x103 */
	aux_dpcd_irq = dprx->rbus_ops->get_byte_extint(PB7_DD_AUX_DPCD_IRQ);
	link_bw_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x00);
	lane_count_set = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x01, 0x01);

	/* SVN Record 1167 */
	if (((dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_1_FAIL) ||
		(dprx->lt_status == _DP_NORMAL_TRAINING_PATTERN_2_FAIL)) &&
	   ((aux_dpcd_irq & (_BIT7 | _BIT6 | _BIT5)) == _BIT6) &&
	   ((link_bw_set != dprx->phy_ops->get_link_rate(dprx)) ||
		((link_bw_set & 0x1F)!= dprx->phy_ops->get_lane_count(dprx)))) {
		dprx->phy_ops->set_training_pattern(dprx, _DP_TRAINING_PATTERN_END);

		dprx->lt_status = _DP_LINK_TRAINING_NONE;
	}

	dprx->phy_ops->set_link_rate(dprx, link_bw_set);
	dprx->phy_ops->set_lane_count(dprx, (lane_count_set & 0x1F));

	/* Clear Flag */
	dprx->rbus_ops->set_bit_extint(PB7_DD_AUX_DPCD_IRQ,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
		(_BIT7 | _BIT6 | _BIT5));

	switch (dprx->phy_ops->get_training_pattern(dprx)) {
	case _DP_TRAINING_PATTERN_1:
		rtk_dprx_lt_tps1(dprx, aux_dpcd_irq);
		value_d1 = true;
		break;

	case _DP_TRAINING_PATTERN_2:
	case _DP_TRAINING_PATTERN_3:
	case _DP_TRAINING_PATTERN_4:
		rtk_dprx_lt_tps2_3_4(dprx);
		value_d1 = true;
		break;

	case _DP_TRAINING_PATTERN_END:
		rtk_dprx_lt_end(dprx);
		value_d1 = true;
		break;

	default:
		break;
	}

	return value_d1;
}

/**
 * rtk_dprx_power_ctrl_int_handler - Device Power Control Field Interrupt Handler
 * Source has written DPCD 00600h
 */
static void rtk_dprx_power_ctrl_int_handler(struct rtk_dprx *dprx)
{
	u8 set_power_state;
	bool backup_flag;
	u8 link_status[3];

	backup_flag = dprx->phy_dat.backup_pd_link_status_flg;
	backup_status[0] = dprx->phy_dat.link_status_backup[0];
	backup_status[1] = dprx->phy_dat.link_status_backup[1];
	backup_status[2] = dprx->phy_dat.link_status_backup[2];

	/* Clear Flag */
	dprx->rbus_ops->set_bit_extint(PB7_DD_AUX_DPCD_IRQ,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT0);

	set_power_state = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x06, 0x00, (_BIT2 | _BIT1 | _BIT0));
	if ((set_power_state == _BIT1) ||
	   (set_power_state == (_BIT2 | _BIT0))) {
		/* DP Power Down */
		if (backup_flag == false) {
			backup_flag = true;

			backup_status[0] = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x02, 0x02);
			backup_status[1] = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x02, 0x03);
			backup_status[2] = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x02, 0x04, _BIT0);
		}

		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x02, 0x00);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x03, 0x00);
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x04,
			~(_BIT7 | _BIT0), _BIT7);
	} else if (set_power_state == _BIT0) {
		/* DP Power Normal */
		if (backup_flag == true) {
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x02, backup_status[0]);
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x03, backup_status[1]);
			dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x02, 0x04,
				~(_BIT7 | _BIT0), (_BIT7 | backup_status[2]));
		}

		backup_flag = false;
		backup_status[0] = 0x00;
		backup_status[1] = 0x00;
		backup_status[2] = 0x00;
	}

	dprx->phy_dat.backup_pd_link_status_flg = backup_flag;
	dprx->phy_dat.link_status_backup[0] = backup_status[0];
	dprx->phy_dat.link_status_backup[1] = backup_status[1];
	dprx->phy_dat.link_status_backup[2] = backup_status[2];
}

/**
 * rtk_dprx_aux_handler - DP Int Handler
 */
static int rtk_dprx_aux_handler(struct rtk_dprx *dprx)
{
	bool value_d0 = false;
	bool value_d1 = false;
	u8 irq_status;
	u8 aux_dpcd_irq;
	u8 aux_dpcd_irq_bit0;
	u8 dpcd_addr[3] = {0};

	irq_status = dprx->rbus_ops->get_bit_extint(PB7_DC_AUX_IRQ_STATUS, _BIT7);
	/* D0 DP Global IRQ */
	if (irq_status != _BIT7)
        goto skip_global_irq;

	/* Backup the DPCD port access registers */
	dprx->aux_ops->get_dpcd_addr(dprx, &dpcd_addr[0]);

	/* Aux Manual Mode */
	if (!rtk_dprx_pre_int_handler(dprx))
        goto skip_pre_int_handler;

    value_d0 = rtk_dprx_symbol_err_cnt(dprx);

	aux_dpcd_irq = dprx->rbus_ops->get_byte_extint(PB7_DD_AUX_DPCD_IRQ);
	if ((aux_dpcd_irq & (_BIT7 | _BIT6 | _BIT5)) != 0x00)
		value_d1 = rtk_dprx_lt_int_handler(dprx);

	aux_dpcd_irq_bit0 = dprx->rbus_ops->get_bit_extint(PB7_DD_AUX_DPCD_IRQ, _BIT0);
	if (aux_dpcd_irq_bit0 == _BIT0) {
		rtk_dprx_power_ctrl_int_handler(dprx);
		value_d0 = true;
	}

skip_pre_int_handler:

	/* Restore the DPCD port access registers */
	dprx->aux_ops->set_dpcd_addr(dprx, &dpcd_addr[0]);

skip_global_irq:

	if (value_d0 || value_d1) {
		/* Aux Hardware Control */
		dprx->aux_ops->set_auto_mode(dprx);
		return true;
	}

	return false;
}

static const struct rtk_dprx_isr_ops dprx_isr_ops = {
	.aux_handler = rtk_dprx_aux_handler,
};

int rtk_dprx_isr_init(struct rtk_dprx *dprx)
{

	dprx->isr_ops = &dprx_isr_ops;

	return 0;
}