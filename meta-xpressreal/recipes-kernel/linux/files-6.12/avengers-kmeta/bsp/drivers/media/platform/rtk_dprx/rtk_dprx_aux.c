// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_dprx.h"

#define _DP_AUX_REPLY_TIMEOUT_USER_SETTING 0x0A
#define AUX_RX_ADJR   0x01

static void rtk_dprx_power_on_initial(struct rtk_dprx *dprx);
static void rtk_dprx_auxset(struct rtk_dprx *dprx);
static void rtk_dprx_dpcd_irq_initial(struct rtk_dprx *dprx);
static void rtk_dprx_source_clk_set(struct rtk_dprx *dprx);
static void rtk_dprx_dpcd_initial(struct rtk_dprx *dprx, u8 lane_count);
static void rtk_dprx_listen_mode_setting(struct rtk_dprx *dprx);
static void rtk_dprx_set_sink_dpcd_ident(struct rtk_dprx *dprx,
		enum RTK_DP_VERSION version, struct drm_dp_dpcd_ident *ident);
static void rtk_dprx_dpcd_free_sync(struct rtk_dprx *dprx,
		bool supported);

static void set_dp_aux_rx0_manual_mode(struct rtk_dprx *dprx)
{
	//SET_INTERRUPT_ENABLE_STATUS(_INT_DP, _DISABLE);
	dprx->rbus_ops->set_bit(PB7_D0_AUX_MODE_SET, ~_BIT1, 0x00);
	//SET_INTERRUPT_ENABLE_STATUS(_INT_DP, _ENABLE);
}

static void set_dp_aux_rx0_auto_mode(struct rtk_dprx *dprx)
{
	//SET_INTERRUPT_ENABLE_STATUS(_INT_DP, _DISABLE);
	dprx->rbus_ops->set_bit(PB7_D0_AUX_MODE_SET, ~_BIT1, _BIT1);
	//SET_INTERRUPT_ENABLE_STATUS(_INT_DP, _ENABLE);
}

static void set_dp_aux_defer_mode(struct rtk_dprx *dprx)
{
	//SET_INTERRUPT_ENABLE_STATUS(_INT_DP, _DISABLE);
	dprx->rbus_ops->set_bit(PB7_D0_AUX_MODE_SET, ~(_BIT4 | _BIT1), _BIT4);
	//SET_INTERRUPT_ENABLE_STATUS(_INT_DP, _ENABLE);
}

static u8 rtk_dprx_get_manual_mode_status(struct rtk_dprx *dprx)
{
	u8 aux_mode;

	aux_mode = dprx->rbus_ops->get_byte(PB7_D0_AUX_MODE_SET);

	return aux_mode;
}

/**
 * rtk_dprx_aux_initial - Initial Setting for DP AUX
 */
static void rtk_dprx_aux_initial(struct rtk_dprx *dprx)
{
	dprx->rbus_ops->mask_write(DPRX14_PLL_MISC,
		DPRX14_PLL_MISC_iso_ana_b_mask,	DPRX14_PLL_MISC_iso_ana_b(1));

	/* Aux Power on Initial Setting */
	rtk_dprx_power_on_initial(dprx);

	// TODO: Check if rtk_dprx_auxset can be removed?
	//rtk_dprx_auxset(dprx);

	rtk_dprx_dpcd_initial(dprx, dprx->phy_dat.lane_count);

	/*
	 * Must be initialized "AFTER DPCD setting" to avoid entering interruption unexpectively
	 */
	rtk_dprx_dpcd_irq_initial(dprx);

	/* Link Training state is managed by rtk_dprx_link_training_init() */
	dprx->fake_lt = false;
}

/**
 * rtk_dprx_set_pn_swap - Set DP Lane Mapping Type
 *
 * @enable_swap:
 *     true - Enable Aux Digital Phy PN Swap
 *     false - Disable Aux Digital Phy PN Swap
 */
static void rtk_dprx_set_pn_swap(struct rtk_dprx *dprx, bool enable_swap)
{
	if (enable_swap)
		dprx->rbus_ops->set_bit(PB7_72_AUX_DIG_PHY2, ~_BIT0, 0x0);
	else
		dprx->rbus_ops->set_bit(PB7_72_AUX_DIG_PHY2, ~_BIT0, _BIT0);
}

/**
 * rtk_dprx_power_on_initial - Aux Power on Initial Setting
 */
static void rtk_dprx_power_on_initial(struct rtk_dprx *dprx)
{
	/* Aux_timeout setting */
	dprx->rbus_ops->set_byte(PB7_A2_AUX_TIMEOUT_TARGET,
		_DP_AUX_REPLY_TIMEOUT_USER_SETTING);

	/* Average 8 cycles as Start Postion, Aux Clk Select to 27MHz, no Swap */
	dprx->rbus_ops->set_byte(PB7_72_AUX_DIG_PHY2, 0xC0);

	/* Aux Clk Select Manual Mode, Aux New Mode */
	dprx->rbus_ops->set_bit(PB7_73_AUX_DIG_PHY3, ~(_BIT7 | _BIT6), _BIT6);

	/* HW Fake Link Training Disable */
	dprx->rbus_ops->set_bit(PB7_C6_AUX_PHY_DIG2, ~_BIT4, 0x00);

	/* Set Aux Precharge Number */
	dprx->rbus_ops->set_bit(PB7_74_AUX_DIG_PHY4,
		~(_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), (_BIT3 | _BIT0));

	/* Aux Auto Mode, Using Default Average Number */
	dprx->rbus_ops->set_byte(PB7_75_AUX_DIG_PHY5, 0x0D);

	/* Enble Aux Channel */
	dprx->rbus_ops->set_bit(PB7_D0_AUX_MODE_SET,
		~(_BIT3 | _BIT2 | _BIT0), _BIT0);

	/* Fast IIC Clock */
	dprx->rbus_ops->set_byte(PB7_D1_DP_IIC_SET, 0x02);

	/* Set aux mac clk use xclk */
	dprx->rbus_ops->set_bit(PB7_DA_AUX_FIFO_RST,
		~(_BIT6 | _BIT4 | _BIT3 | _BIT1 | _BIT0), _BIT3);

	/* Not Reply when Aux Error */
	dprx->rbus_ops->set_bit(PB7_DB_AUX_STATUS,
		~(_BIT6 | _BIT5 | _BIT4), _BIT4);
	dprx->rbus_ops->set_bit(PB7_76_AUX_DIG_PHY6,
		~(_BIT7 | _BIT6), _BIT6);

	/* Disable other DPCD, Aux Timeout, Receiving Aux INT */
	dprx->rbus_ops->set_bit(PB7_DC_AUX_IRQ_STATUS,
		~(_BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1), 0x00);

	/* Disable Aux Phy Int */
	dprx->rbus_ops->set_bit(PB7_77_AUX_DIG_PHY7, ~(_BIT7 | _BIT6), 0x00);

	/* Aux Ack Timer */
	dprx->rbus_ops->set_bit(PB7_F0_AUX_TX_TIMER,
		~(_BIT5 | _BIT2 | _BIT1), (_BIT5 | _BIT2));

	/* Set Timeout Target */
	dprx->rbus_ops->set_bit(PB7_F1_AUX_TX_TIMER_2,
		~(_BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
		(_BIT3 | _BIT2 | _BIT0));

	/* Disable Reply IIC Defer Before Data Ready */
	dprx->rbus_ops->set_bit(PB7_F4_MCUIIC,
		~(_BIT6 | _BIT5), 0x00);

	/* Enable Aux Error Handler */
	dprx->rbus_ops->set_bit(PB7_78_AUX_DIG_PHY8,
		~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
		(_BIT5 | _BIT4));

	/* Toggle Aux Auto K */
	dprx->rbus_ops->set_bit(PB7_65_AUX_5, ~_BIT7, 0x00);
	dprx->rbus_ops->set_bit(PB7_65_AUX_5, ~_BIT7, _BIT7);

	/* Aux comm current select max */
	dprx->rbus_ops->set_bit(PB7_64_AUX_4, ~(_BIT7 | _BIT6), (_BIT7 | _BIT6));

	/* Clear the Clock Divider for AUX MAC and PHY */
	dprx->rbus_ops->set_bit(PB7_73_AUX_DIG_PHY3,
		~(_BIT3 | _BIT2 | _BIT1 | _BIT0), 0x00);
	dprx->rbus_ops->set_bit(PB7_B0_AUX_PAYLOAD_CLEAR,
		~(_BIT3 | _BIT2 | _BIT1 | _BIT0), 0x00);

	/* Aux Source Clock Setting */
	rtk_dprx_source_clk_set(dprx);

	/* Set the Clock Divider = 1 for AUX MAC and PHY */
	dprx->rbus_ops->set_bit(PB7_73_AUX_DIG_PHY3,
		~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT0);
	dprx->rbus_ops->set_bit(PB7_B0_AUX_PAYLOAD_CLEAR,
		~(_BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT0);

	/* Set Aux Listen Mode */
	// TODO: Check if rtk_dprx_listen_mode_setting can be removed?
	//rtk_dprx_listen_mode_setting(dprx);

	/*
	 * Enable Aux 50/50 Duty,
	 * Talk Mode High Period and Low Period Will Keep The Same of 1M Counter
	 */
	dprx->rbus_ops->set_bit(PB7_72_AUX_DIG_PHY2, ~_BIT2, _BIT2);

	/*
	 * Set 1M Count = 28, 50/50 Duty Enable,
	 * The High Period and Low Period are The Same = 14 (28M / 28 = 1MHz)
	 */
	dprx->rbus_ops->set_byte(PB7_71_AUX_DIG_PHY1, 0x1C);

	/* Digital Aux Power On, DPCD REG Power On, MSG Power On */
	dprx->rbus_ops->set_bit(PB7_C0_DPCD_CTRL,
		~(_BIT7 | _BIT6 | _BIT5 | _BIT1 | _BIT0), 0x00);

	/* Set end to idle trigger Aux Int Flag */
	dprx->rbus_ops->set_bit(PB7_7A_AUX_DIG_PHYA,
		~(_BIT3 | _BIT2 | _BIT1 | _BIT0), (_BIT3 | _BIT1));
}

/**
 * rtk_dprx_auxset - Set Aux Diff mode or Single-eneded mode
 */
static void __maybe_unused rtk_dprx_auxset(struct rtk_dprx *dprx)
{
	if (!dprx->aux_diff_mode) {
		/* Set Aux Tx LDO = 1.05V */
		// FIXME: dprx->rbus_ops->set_bit(PB7_61_AUX_1, ~(_BIT7 | _BIT6 | _BIT5), (_DP_AUX_SWING_1050_MV << 5));

		/* Open AUX ADJR_P, Rx Common Mode from 3.3V */
		dprx->rbus_ops->set_bit(PB7_61_AUX_1, ~(_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT4);

		/* [5]Enable Single-Ended Mode, [4:3]Aux Vth-->50mV, [0]Aux 50ohm auto K(Enable Big_Z0_P) */
		dprx->rbus_ops->set_byte(PB7_62_AUX_2, 0x29);

		/* [4]Enable Big_Z0_N, [3:0]Open ADJR_N */
		dprx->rbus_ops->set_bit(PB7_66_AUX_6, ~(_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT4);
	} else {
		/* Set Aux Tx LDO = 1.05V */
		// FIXME: dprx->rbus_ops->set_bit(PB7_61_AUX_1, ~(_BIT7 | _BIT6 | _BIT5), (_DP_AUX_SWING_1050_MV << 5));

		/* Rx Common Mode from 3.3V */
		dprx->rbus_ops->set_bit(PB7_61_AUX_1, ~_BIT4, _BIT4);

		/* [5]Disable Single-Ended Mode, [4:3]Aux Vth-->50mV, [0]Aux 50ohm auto K(Enable Big_Z0_P) */
		dprx->rbus_ops->set_byte(PB7_62_AUX_2, 0x09);

		/* [4]Enable Big_Z0_N */
		dprx->rbus_ops->set_bit(PB7_66_AUX_6, ~_BIT4, _BIT4);

		/* AUX RX0 P Channel Resistance Setting */
		dprx->rbus_ops->set_bit(PB7_61_AUX_1,
			~(_BIT3 | _BIT2 | _BIT1 | _BIT0), AUX_RX_ADJR);

		/* AUX RX0 N Channel Resistance Setting */
		dprx->rbus_ops->set_bit(PB7_66_AUX_6,
			~(_BIT3 | _BIT2 | _BIT1 | _BIT0), AUX_RX_ADJR);
	}
}

/**
 * rtk_dprx_listen_mode_setting - Set Aux Diff mode or Single-eneded mode
 */
static void __maybe_unused rtk_dprx_listen_mode_setting(struct rtk_dprx *dprx)
{
	if (!dprx->aux_diff_mode) {
		/* Set Single-Ended Mode Comparator Window to  50mv */
		dprx->rbus_ops->set_bit(PB7_60_DIG_TX_04, ~(_BIT1 | _BIT0), _BIT0);

		/* Set TX VLDO, Open AUX ADJR_P, Rx Common Mode from 1.3V */
		dprx->rbus_ops->set_byte(PB7_61_AUX_1, 0xF0);

		/*
		 * [5]Enable Single-Ended Mode, [4:3]Aux Vth-->50mV,
		 * [0]Aux 50ohm auto K(Enable Big_Z0_P)
		 */
		dprx->rbus_ops->set_byte(PB7_62_AUX_2, 0x29);

		/* [4]Enable Big_Z0_N, [3:0]Open ADJR_N */
		dprx->rbus_ops->set_bit(PB7_66_AUX_6,
			~(_BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT4);

		/* Set Aux D_2 = ~D_1 for Aux Single Ended Mode */
		dprx->rbus_ops->set_bit(PB7_79_AUX_DIG_PHY9,
			~(_BIT7 | _BIT3 | _BIT2 | _BIT1 | _BIT0), _BIT7);
	} else {
		/* Set TX VLDO, Rx Common Mode from 1.2V */
		dprx->rbus_ops->set_bit(PB7_61_AUX_1,
			~(_BIT7 | _BIT6 | _BIT5 | _BIT4), (_BIT7 | _BIT6 | _BIT4));

		/*
		 * [5]Disable Single-Ended Mode,
		 * [4:3]Aux Vth-->50mV, [0]Aux 50ohm auto K(Enable Big_Z0_P)
		 */
		dprx->rbus_ops->set_byte(PB7_62_AUX_2, 0x09);

		/* [4]Enable Big_Z0_N */
		dprx->rbus_ops->set_bit(PB7_66_AUX_6, ~_BIT4, _BIT4);

		/* AUX RX0 P Channel Resistance Setting */
		dprx->rbus_ops->set_bit(PB7_61_AUX_1,
			~(_BIT3 | _BIT2 | _BIT1 | _BIT0), AUX_RX_ADJR);

		/* AUX RX0 N Channel Resistance Setting */
		dprx->rbus_ops->set_bit(PB7_66_AUX_6,
			~(_BIT3 | _BIT2 | _BIT1 | _BIT0), AUX_RX_ADJR);
	}

    /* Set Aux Talk Mode ADJR */
	dprx->rbus_ops->set_byte(PB7_67_DIG_TX_03, 0xFF);
}

/**
 * rtk_dprx_dpcd_irq_initial - Initial DPCD Related Addr IRQ
 */
static void rtk_dprx_dpcd_irq_initial(struct rtk_dprx *dprx)
{
	u32 ulWildCard2Address = 0x0FFFFF;
	u32 ulWildCard3Address = 0x0FFFFF;
	u32 ulWildCard0Address = 0x0FFFFF;

	/* Enable 068xxx INT */
	dprx->rbus_ops->set_bit(PB7_DA_AUX_FIFO_RST,
		~(_BIT6 | _BIT4 | _BIT1 | _BIT0), _BIT1);
	dprx->rbus_ops->set_bit(PB7_DA_AUX_FIFO_RST,
		~(_BIT6 | _BIT4 | _BIT2 | _BIT1 | _BIT0), _BIT2);

	/* Enable DPCD INT */
	dprx->rbus_ops->set_byte(PB7_DE_AUX_DPCD_IRQ_EN,
		DPRX14_AUX_AUX_DPCD_IRQ_EN_wr102_int_en(1) |
		DPRX14_AUX_AUX_DPCD_IRQ_EN_wr100_101_int_en(1) |
		DPRX14_AUX_AUX_DPCD_IRQ_EN_wr103_108_int_en(1) |
		DPRX14_AUX_AUX_DPCD_IRQ_EN_wr201_int_en(0) |
		DPRX14_AUX_AUX_DPCD_IRQ_EN_wr260_261_int_en(0) |
		DPRX14_AUX_AUX_DPCD_IRQ_EN_wr270_int_en(0) |
		DPRX14_AUX_AUX_DPCD_IRQ_EN_wr300_302_int_en(0) |
		DPRX14_AUX_AUX_DPCD_IRQ_EN_wr600_int_en(0));

	/* Disable IRQ for Error Counter */
	dprx->rbus_ops->set_bit(PB7_CC_DPCD_CONFIG_ADDR_FLAG,
		~(_BIT7 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), 0x00);

	/* INT Wildcard 0 address set */
	dprx->rbus_ops->set_byte(PB7_B4_AUX_IRQ_ADDR0_MSB, (ulWildCard0Address & 0xFF0000) >> 16);
	dprx->rbus_ops->set_byte(PB7_B5_AUX_IRQ_ADDR0_MSB1, (ulWildCard0Address & 0x00FF00) >> 8);
	dprx->rbus_ops->set_byte(PB7_B6_AUX_IRQ_ADDR0_LSB, (ulWildCard0Address & 0x0000FF));

	/* INT Wildcard 2 address set */
	dprx->rbus_ops->set_byte(PB7_BA_AUX_IRQ_ADDR2_MSB, (ulWildCard2Address & 0xFF0000) >> 16);
	dprx->rbus_ops->set_byte(PB7_BB_AUX_IRQ_ADDR2_MSB1, (ulWildCard2Address & 0x00FF00) >> 8);
	dprx->rbus_ops->set_byte(PB7_BC_AUX_IRQ_ADDR2_LSB, (ulWildCard2Address & 0x0000FF));

	/* INT Wildcard 3 address set */
	dprx->rbus_ops->set_byte(PB7_BD_AUX_IRQ_ADDR3_MSB, (ulWildCard3Address & 0xFF0000) >> 16);
	dprx->rbus_ops->set_byte(PB7_BE_AUX_IRQ_ADDR3_MSB1, (ulWildCard3Address & 0x00FF00) >> 8);
	dprx->rbus_ops->set_byte(PB7_BF_AUX_IRQ_ADDR3_LSB, (ulWildCard3Address & 0x0000FF));
}

/**
 * rtk_dprx_source_clk_set - Aux Souce Clock Setting
 */
static void __maybe_unused rtk_dprx_source_clk_set(struct rtk_dprx *dprx)
{
	dprx->rbus_ops->set_bit(P0_0B_POWER_CTRL, ~(_BIT5 | _BIT4), 0x00);
}

/**
 * rtk_dprx_dpcd_initial - DPCD Table Initial
 */
static void rtk_dprx_dpcd_initial(struct rtk_dprx *dprx, u8 lane_count)
{
	u32 temp = 0;

	dev_info(dprx->dev, "%s --> %u lanes\n", __func__, lane_count);

	dprx->aux_ops->set_sink_status(dprx, _DP_SINK_REVEICE_PORT0, _DP_SINK_OUT_OF_SYNC);
	dprx->aux_ops->set_sink_status(dprx, _DP_SINK_REVEICE_PORT1, _DP_SINK_OUT_OF_SYNC);

	/* Enhanced Framing Support(Bit7) and DP Lane Count(Bit[4:0]) */
	dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x02,
		(u8)~(_BIT7 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0), (_BIT7 | lane_count));

	/* Down Spread 0.5% */
	dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x03, ~_BIT0, _BIT0);

	if (dprx->audio_support)
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x04, 0x01);
	else
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x04, 0x00);

	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x06, 0x01);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x08, 0x02);

	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x09, 0x00);

	if (dprx->audio_support)
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x0A, 0x06);
	else
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x0A, 0x00);

	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x0B, 0x00);

	/*
	 * Set Video Formats Supported For Fallback Mode (Useful in Dp2.0)
	 * _BIT2: 1920x1080 - 60
	 * _BIT1: 1280x720 - 60
	 * _BIT0: 1024x768 - 60
	 */
	dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x20,
		~(_BIT2 | _BIT1 | _BIT0), (_BIT2 | _BIT1 | _BIT0));

	/* DPCD Link Status Field Setting */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x00, 0x41);
	dprx->aux_ops->set_dpcd_write1_clear_value(dprx, 0x00, 0x02, 0x01, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x05, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x08, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x09, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x0A, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x0B, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x46, 0x20);

	/* Initialize Reserved to 0 */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x10, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x12, 0x00);

	/* _BIT1: Repeater,  _BIT0: HDCP Capable */
	if (dprx->hdcp_support)
		dprx->aux_ops->set_dpcd_value(dprx, 0x06, 0x80, 0x28, 0x01);
	else
		dprx->aux_ops->set_dpcd_value(dprx, 0x06, 0x80, 0x28, 0x00);

	set_dp_aux_rx0_manual_mode(dprx);

	/* Initialize HDCP2.2 DPCD (69XXXh) to 0s for avoiding SRAM initial state values remained */
	for (temp = 0x9000; temp <= 0x94BF; temp++) {
		/* DPCD [0x69000 ~ 0x694BF] */
		dprx->aux_ops->set_dpcd_write_value(dprx, 0x06, HIBYTE(temp), LOBYTE(temp), 0x00);
	}

	/* DPCD 0x6921D RxCaps_2 VERSION, must be 0x2 */
	dprx->aux_ops->set_dpcd_value(dprx, 0x06, 0x92, 0x1D, 0x02);

	set_dp_aux_rx0_auto_mode(dprx);

	dprx->aux_ops->change_dpcd_version(dprx, dprx->dpcd_ver, dprx->max_link_rate);
}

/**
 * rtk_dprx_change_dpcd_version - Dp Version Switch
 */
static void rtk_dprx_change_dpcd_version(struct rtk_dprx *dprx,
		enum RTK_DP_VERSION version, enum RTK_DP_LINK_RATE max_link_rate)
{
	struct drm_dp_dpcd_ident *ident;
	struct drm_dp_dpcd_ident empty_ident;
	u32 bas_link_rate;
	u32 ext_link_rate;
	u8 ext_present;

	ident = &dprx->ident;
	memset_io(&empty_ident, 0, sizeof(empty_ident));

	if (version >= _DP_VERSION_1_4) {
		/* DPCD Capability Field Initial */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x00, _DP_VERSION_1_2);

		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x22, 0x00, version);

		/* Set EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT bit */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x0E,
			(u8)~_BIT7, (u8)_BIT7);

		/* SST Split SDP support */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x22, 0x10,
			(u8)~_BIT1, (u8)_BIT1);

		if (max_link_rate >= _DP_HIGH_SPEED2_540MHZ)
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x01, _DP_LINK_HBR2);
		else
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x01, max_link_rate);

		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x22, 0x01, max_link_rate);

		rtk_dprx_set_sink_dpcd_ident(dprx, version, ident);

		/* Down Sream Port isn't Present */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x05, 0x00);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x22, 0x05, 0x00);
		dprx->aux_ops->set_branch_dpcd_ident(dprx, &empty_ident); /* Clear branch dpcd ident */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x21, 0x00);

	} else if (version == _DP_VERSION_1_2) {
		/* DPCD Capability Field Initial */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x00, _DP_VERSION_1_2);

		/* Set EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT bit */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x0E,
			(u8)~_BIT7, (u8)0x00);

		if (max_link_rate > _DP_HIGH_SPEED2_540MHZ)
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x01, _DP_LINK_HBR2);
		else
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x01, max_link_rate);

		rtk_dprx_set_sink_dpcd_ident(dprx, version, ident);

		/* Down Sream Port isn't Present */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x05, 0x00);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x22, 0x05, 0x00);
		dprx->aux_ops->set_branch_dpcd_ident(dprx, &empty_ident); /* Clear branch dpcd ident */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x21, 0x00);

	} else {
		/* DPCD Capability Field Initial */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x00, _DP_VERSION_1_1);

		/* Set EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT bit */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x0E,
			(u8)~_BIT7, (u8)0x00);

		if (max_link_rate > _DP_HIGH_SPEED_270MHZ)
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x01, _DP_LINK_HBR);
		else
			dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x01, max_link_rate);

		rtk_dprx_set_sink_dpcd_ident(dprx, version, ident);

		/* Down Sream Port isn't Present */
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x00, 0x05, 0x00);
		dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x22, 0x05, 0x00);
		dprx->aux_ops->set_branch_dpcd_ident(dprx, &empty_ident); /* Clear branch dpcd ident */
	}

	rtk_dprx_dpcd_free_sync(dprx, true);

	ext_present = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x00, 0x0E, _BIT7);
	bas_link_rate = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x00, 0x01);
	ext_link_rate = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x22, 0x01);

	if ((ext_present == _BIT7) && (ext_link_rate >= _DP_LINK_HBR3)) {
		/* TPS3 Support */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x02,
			(u8)~_BIT6, (u8)_BIT6);

		/* TPS4 Support */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x03,
			(u8)~_BIT7, (u8)_BIT7);

		/* Set TRAINING_AUX_RD_INTERVAL = 16ms for EQ phase */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x0E,
			(u8)~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
			_DP_LT_AUX_RD_INTVL_EQ_16MS);
	} else if ((bas_link_rate == _DP_LINK_HBR2) ||
			((ext_present == _BIT7) && (ext_link_rate == _DP_LINK_HBR2))) {
		/* TPS3 Support */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x02,
			(u8)~_BIT6, (u8)_BIT6);

		/* TPS4 NonSupport */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x03,
			(u8)~_BIT7, 0x00);

		/* Set TRAINING_AUX_RD_INTERVAL = 16ms for EQ phase */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x0E,
			(u8)~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
			_DP_LT_AUX_RD_INTVL_EQ_16MS);
	} else {
		/* TPS3 NonSupport */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x02,
			(u8)~_BIT6, 0x00);

		/* TPS4 NonSupport */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x03,
			(u8)~_BIT7, 0x00);

		/* Set TRAINING_AUX_RD_INTERVAL = 16ms for EQ phase */
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x0E,
			(u8)~(_BIT6 | _BIT5 | _BIT4 | _BIT3 | _BIT2 | _BIT1 | _BIT0),
			_DP_LT_AUX_RD_INTVL_EQ_16MS);
	}
}

/**
 * rtk_dprx_set_sink_dpcd_ident -
 * Dp Set IEEE OUI Support and Dp Set Sink Device Specific Field 00400h - 004FFh
 */
static void rtk_dprx_set_sink_dpcd_ident(struct rtk_dprx *dprx,
		enum RTK_DP_VERSION version, struct drm_dp_dpcd_ident *ident)
{
	/* _BIT7 = 1: Always IEEE OUI Support Under DP1.2 */
	if (version >= _DP_VERSION_1_2)
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x07,
			(u8)~_BIT7, _BIT7);
	else
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x07,
			(u8)~_BIT7, 0x00);

	/* Sink IEEE OUI */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x00, ident->oui[0]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x01, ident->oui[1]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x02, ident->oui[2]);

	/* Sink Device Identification String */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x03, ident->device_id[0]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x04, ident->device_id[1]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x05, ident->device_id[2]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x06, ident->device_id[3]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x07, ident->device_id[4]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x08, ident->device_id[5]);

	/* Sink HW/FW Version */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x09, ident->hw_rev);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x0A, ident->sw_major_rev);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x04, 0x0B, ident->sw_minor_rev);
}

/**
 * rtk_dprx_set_branch_dpcd_ident -
 * Dp Set Branch Device Specific Field 00500h - 005FFh
 */
static void rtk_dprx_set_branch_dpcd_ident(struct rtk_dprx *dprx,
		struct drm_dp_dpcd_ident *ident)
{
	/* Branch IEEE OUI */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x00, ident->oui[0]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x01, ident->oui[1]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x02, ident->oui[2]);

	/* Branch IEEE OUI LSB */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x03, ident->device_id[0]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x04, ident->device_id[1]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x05, ident->device_id[2]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x06, ident->device_id[3]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x07, ident->device_id[4]);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x08, ident->device_id[5]);

	/* Branch HW/FW Version */
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x09, ident->hw_rev);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x0A, ident->sw_major_rev);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x05, 0x0B, ident->sw_minor_rev);
}

/**
 * rtk_dprx_reset_dpcd_link_status - DPCD Link Status Field Reset
 */
static void rtk_dprx_reset_dpcd_link_status(struct rtk_dprx *dprx,
		enum RTK_DP_RESET_STATUS rst_status)
{
	u8 aux_mode;

	aux_mode = dprx->rbus_ops->get_byte(PB7_D0_AUX_MODE_SET);

	set_dp_aux_rx0_manual_mode(dprx);

	if (rst_status == _DP_DPCD_LINK_STATUS_INITIAL) {
		/* Set DPCD 00600h to 0x01 */
		dprx->aux_ops->set_dpcd_bit_write_value(dprx, 0x00, 0x06, 0x00,
			~(_BIT2 | _BIT1 | _BIT0), _BIT0);
	}

	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x02, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x03, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x04, 0x80);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x05, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x06, 0x00);
	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x07, 0x00);

	/* Clear all MAC layer LT status flags when resetting DPCD link status */
	rtk_dprx_lt_set_link_integrity_fail(dprx, false);
	rtk_dprx_lt_set_fake_training_mode(dprx, false);
	rtk_dprx_lt_set_vbios_mode(dprx, false);

	// TODO: CLR_DP_PHY_RX0_BACKUP_PD_LINK_STATUS_FLG();

	if((aux_mode & _BIT1) == _BIT1)
		set_dp_aux_rx0_auto_mode(dprx);
}

// TODO: ScalerDpAuxRx0SetHotPlugEvent

// TODO: ScalerDpAuxRx0BeforeHpdToggleProc

// TODO: ScalerDpAuxRx0AfterHpdToggleProc

/**
 * rtk_dprx_link_status_irq - Dp Interrupt Request
 */
static void rtk_dprx_link_status_irq(struct rtk_dprx *dprx)
{
	u8 set_power_state;

	dprx->aux_ops->set_manual_mode(dprx);

	if ((rtk_dprx_lt_get_state(dprx) == LT_STATE_FAILED) || rtk_dprx_lt_get_link_integrity_fail(dprx)) {
		dprx->aux_ops->reset_dpcd_link_status(dprx, _DP_DPCD_LINK_STATUS_IRQ);

		dprx->aux_ops->set_auto_mode(dprx);

		if (dprx->hdcp_support) {
			// TODO: ScalerDpHdcp14RxResetProc(enumInputPort);
		}

		set_power_state = dprx->aux_ops->get_dpcd_bit_info(dprx, 0x00, 0x06, 0x00, (_BIT2 | _BIT1 | _BIT0));
		if ((set_power_state != _BIT1) &&
			(set_power_state != (_BIT2 | _BIT0))) {
			// TODO: ScalerTimerCancelTimerEvent(_SCALER_TIMER_EVENT_DP_RX0_HDCP_LONG_HOTPLUG_EVENT);

			dprx->aux_ops->hpd_irq_assert(dprx);
		}
	}

	dprx->aux_ops->set_auto_mode(dprx);
}

// TODO: ScalerDpAuxRx0ChangeHdcpDpcdCapability

/**
 * rtk_dprx_aux_power_on - Dp Aux Power On
 */
static void rtk_dprx_aux_power_on(struct rtk_dprx *dprx)
{
	/* Disable Aux Power Saving Mode */
	dprx->rbus_ops->set_bit(PB7_62_AUX_2, ~_BIT1, 0x00);

	/* Switch Aux PHY to GDI BandGap */
	dprx->rbus_ops->set_bit(PB7_63_AUX_3, ~_BIT7, 0x00);

	/* Disable Aux INT */
	dprx->rbus_ops->set_bit(PB7_7A_AUX_DIG_PHYA, ~(_BIT4 | _BIT0), 0x00);
}

/**
 * rtk_dprx_hpd_irq_assert - Dp IRQ Assert Proc
 */
static void rtk_dprx_hpd_irq_assert(struct rtk_dprx *dprx)
{
	// TODO: FIXME
}

// TODO: ScalerDpAuxRx0FakeLTProtect

/**
 * rtk_dprx_set_sink_status - DP Sink Status Setting
 */
static void rtk_dprx_set_sink_status(struct rtk_dprx *dprx,
		enum RTK_DP_SINK_PORT port, enum RTK_DP_SINK_STATUS sync_status)
{
	u8 sink_status;

	sink_status = dprx->aux_ops->get_dpcd_info(dprx, 0x00, 0x02, 0x05);

	if (sync_status == _DP_SINK_IN_SYNC) {
		if (port == _DP_SINK_REVEICE_PORT0)
			sink_status |= _BIT0; /* Set Receive Port 0 in Sync */
		else if (port == _DP_SINK_REVEICE_PORT1)
			sink_status |= _BIT1; /* Set Receive Port 1 in Sync */
	} else {
		if (port == _DP_SINK_REVEICE_PORT0)
			sink_status &= ~_BIT0; /* Set Receive Port 0 Out of Sync */
		else if (port == _DP_SINK_REVEICE_PORT1)
			sink_status &= ~_BIT1; /* Set Receive Port 1 Out of Sync */
	}

	dprx->aux_ops->set_dpcd_value(dprx, 0x00, 0x02, 0x05, sink_status);
}

/**
 * rtk_dprx_get_dpcd_info - Get DPCD Table Information
 *
 * @port_h: High Byte of DPCD Address
 * @port_m: Middle Byte of DPCD Address
 * @port_l: Low Bye of DPCD Adress
 *
 * Return: DPCD Information
 */
static u8 rtk_dprx_get_dpcd_info(struct rtk_dprx *dprx,
		u8 port_h, u8 port_m, u8 port_l)
{
	u8 info = 0;

	/* Release Aux */
	dprx->rbus_ops->set_bit(PB7_C0_DPCD_CTRL, ~_BIT7, 0x00);

	dprx->rbus_ops->set_byte(PB7_C1_DPCD_ADDR_PORT_H, port_h);
	dprx->rbus_ops->set_byte(PB7_C2_DPCD_ADDR_PORT_M, port_m);
	dprx->rbus_ops->set_byte(PB7_C3_DPCD_ADDR_PORT_L, port_l);

	info = dprx->rbus_ops->get_byte(PB7_C4_DPCD_DATA_PORT);

	return info;
}

/**
 * rtk_dprx_get_dpcd_bit_info - Get DPCD Table Information
 *
 * @port_h: High Byte of DPCD Address
 * @port_m: Middle Byte of DPCD Address
 * @port_l: Low Bye of DPCD Adress
 *
 * Return: DPCD Information
 */
static u8 rtk_dprx_get_dpcd_bit_info(struct rtk_dprx *dprx,
		u8 port_h, u8 port_m, u8 port_l, u8 dpcd_bit)
{
	/* Release Aux */
	dprx->rbus_ops->set_bit(PB7_C0_DPCD_CTRL, ~_BIT7, 0x00);

	dprx->rbus_ops->set_byte(PB7_C1_DPCD_ADDR_PORT_H, port_h);
	dprx->rbus_ops->set_byte(PB7_C2_DPCD_ADDR_PORT_M, port_m);
	dprx->rbus_ops->set_byte(PB7_C3_DPCD_ADDR_PORT_L, port_l);

	return dprx->rbus_ops->get_bit(PB7_C4_DPCD_DATA_PORT, dpcd_bit);
}

/**
 * rtk_dprx_set_dpcd_value - Set DPCD Table Information
 *
 * @port_h: High Byte of DPCD Address
 * @port_m: Middle Byte of DPCD Address
 * @port_l: Low Bye of DPCD Adress
 * @value: DPCD Value
 */
static void rtk_dprx_set_dpcd_value(struct rtk_dprx *dprx,
		u8 port_h, u8 port_m, u8 port_l, u8 value)
{
	/* Release Aux */
	dprx->rbus_ops->set_bit(PB7_C0_DPCD_CTRL, ~_BIT7, 0x00);

	dprx->rbus_ops->set_byte(PB7_C1_DPCD_ADDR_PORT_H, port_h);
	dprx->rbus_ops->set_byte(PB7_C2_DPCD_ADDR_PORT_M, port_m);
	dprx->rbus_ops->set_byte(PB7_C3_DPCD_ADDR_PORT_L, port_l);

	dprx->rbus_ops->set_byte(PB7_C4_DPCD_DATA_PORT, value);
}

/**
 * rtk_dprx_set_dpcd_write_value - Set DPCD Table Information
 *
 * @port_h: High Byte of DPCD Address
 * @port_m: Middle Byte of DPCD Address
 * @port_l: Low Bye of DPCD Adress
 * @value: DPCD Value
 */
static void rtk_dprx_set_dpcd_write_value(struct rtk_dprx *dprx,
		u8 port_h, u8 port_m, u8 port_l, u8 value)
{
	u8 backup = 0;

	/* Release Aux */
	dprx->rbus_ops->set_bit(PB7_C0_DPCD_CTRL, ~_BIT7, 0x00);

	backup = dprx->rbus_ops->get_byte(PB7_D0_AUX_MODE_SET);

	/* SET AUX MANUAL MODE */
	set_dp_aux_rx0_manual_mode(dprx);

	dprx->rbus_ops->set_byte(PB7_C1_DPCD_ADDR_PORT_H, port_h);
	dprx->rbus_ops->set_byte(PB7_C2_DPCD_ADDR_PORT_M, port_m);
	dprx->rbus_ops->set_byte(PB7_C3_DPCD_ADDR_PORT_L, port_l);
	dprx->rbus_ops->set_byte(PB7_C4_DPCD_DATA_PORT, value);

	if ((backup & _BIT1) == _BIT1) {
		/* SET AUX AUTO MODE */
		set_dp_aux_rx0_auto_mode(dprx);
	}
}

/**
 * rtk_dprx_set_dpcd_write1_clear_value - Set DPCD Table Information
 *
 * @port_h: High Byte of DPCD Address
 * @port_m: Middle Byte of DPCD Address
 * @port_l: Low Bye of DPCD Adress
 * @value: DPCD Value
 */
static void rtk_dprx_set_dpcd_write1_clear_value(struct rtk_dprx *dprx,
		u8 port_h, u8 port_m, u8 port_l, u8 value)
{
	u8 backup = 0;

	backup = dprx->rbus_ops->get_byte(PB7_D0_AUX_MODE_SET);

	/* Release Aux */
	dprx->rbus_ops->set_bit(PB7_C0_DPCD_CTRL, ~_BIT7, 0x00);

	/* SET AUX AUTO MODE */
	set_dp_aux_rx0_auto_mode(dprx);

	dprx->rbus_ops->set_byte(PB7_C1_DPCD_ADDR_PORT_H, port_h);
	dprx->rbus_ops->set_byte(PB7_C2_DPCD_ADDR_PORT_M, port_m);
	dprx->rbus_ops->set_byte(PB7_C3_DPCD_ADDR_PORT_L, port_l);
	dprx->rbus_ops->set_byte(PB7_C4_DPCD_DATA_PORT, value);

	if ((backup & _BIT1) == 0x00) {
		/* SET AUX MANUAL MODE */
		set_dp_aux_rx0_manual_mode(dprx);
	}
}

/**
 * rtk_dprx_set_dpcd_bit_value - Set DPCD Table Information
 *
 * @port_h: High Byte of DPCD Address
 * @port_m: Middle Byte of DPCD Address
 * @port_l: Low Bye of DPCD Adress
 * @not_bit:
 * @bit_value:
 */
static void rtk_dprx_set_dpcd_bit_value(struct rtk_dprx *dprx,
		u8 port_h, u8 port_m, u8 port_l, u8 not_bit, u8 bit_value)
{
	/* Release Aux */
	dprx->rbus_ops->set_bit(PB7_C0_DPCD_CTRL, ~_BIT7, 0x00);

	dprx->rbus_ops->set_byte(PB7_C1_DPCD_ADDR_PORT_H, port_h);
	dprx->rbus_ops->set_byte(PB7_C2_DPCD_ADDR_PORT_M, port_m);
	dprx->rbus_ops->set_byte(PB7_C3_DPCD_ADDR_PORT_L, port_l);
	dprx->rbus_ops->set_bit(PB7_C4_DPCD_DATA_PORT, ~(~not_bit), bit_value);
}

/**
 * rtk_dprx_set_dpcd_bit_write_value - Set DPCD Table Information
 *
 * @port_h: High Byte of DPCD Address
 * @port_m: Middle Byte of DPCD Address
 * @port_l: Low Bye of DPCD Adress
 * @not_bit:
 * @bit_value:
 */
static void rtk_dprx_set_dpcd_bit_write_value(struct rtk_dprx *dprx,
		u8 port_h, u8 port_m, u8 port_l, u8 not_bit, u8 bit_value)
{
	u8 backup = 0;

    /* Release Aux */
	dprx->rbus_ops->set_bit(PB7_C0_DPCD_CTRL, ~_BIT7, 0x00);

	backup = dprx->rbus_ops->get_byte(PB7_D0_AUX_MODE_SET);

	/* SET AUX MANUAL MODE */
    set_dp_aux_rx0_manual_mode(dprx);

	dprx->rbus_ops->set_byte(PB7_C1_DPCD_ADDR_PORT_H, port_h);
	dprx->rbus_ops->set_byte(PB7_C2_DPCD_ADDR_PORT_M, port_m);
	dprx->rbus_ops->set_byte(PB7_C3_DPCD_ADDR_PORT_L, port_l);
	dprx->rbus_ops->set_bit(PB7_C4_DPCD_DATA_PORT, ~(~not_bit), bit_value);

	if ((backup & _BIT1) == _BIT1) {
		/* SET AUX AUTO MODE */
		set_dp_aux_rx0_auto_mode(dprx);
    }
}

static void rtk_dprx_get_dpcd_addr(struct rtk_dprx *dprx, u8 *dpcd_addr)
{
	if (!dprx || !dpcd_addr)
		return;

	dpcd_addr[0] = dprx->rbus_ops->get_byte(PB7_C1_DPCD_ADDR_PORT_H);
	dpcd_addr[1] = dprx->rbus_ops->get_byte(PB7_C2_DPCD_ADDR_PORT_M);
	dpcd_addr[2] = dprx->rbus_ops->get_byte(PB7_C3_DPCD_ADDR_PORT_L);
}

static void rtk_dprx_set_dpcd_addr(struct rtk_dprx *dprx, u8 *dpcd_addr)
{
	if (!dprx || !dpcd_addr)
		return;

	dprx->rbus_ops->set_byte(PB7_C1_DPCD_ADDR_PORT_H, dpcd_addr[0]);
	dprx->rbus_ops->set_byte(PB7_C2_DPCD_ADDR_PORT_M, dpcd_addr[1]);
	dprx->rbus_ops->set_byte(PB7_C3_DPCD_ADDR_PORT_L, dpcd_addr[2]);
}

/**
 * rtk_dprx_dpcd_free_sync - DP Freesync Aux Rx0 DPCD Setting
 *
 * @supported: MSA_TIMING_PAR_IGNORED
 */
static void rtk_dprx_dpcd_free_sync(struct rtk_dprx *dprx,
		bool supported)
{
	if (supported)
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x07,
			(u8)~_BIT6, _BIT6);
	else
		dprx->aux_ops->set_dpcd_bit_value(dprx, 0x00, 0x00, 0x07,
			(u8)~_BIT6, 0x00);
}

/**
 * rtk_dprx_get_valid_video_check - Get DP Valid Video Check
 */
static bool rtk_dprx_get_valid_video_check(struct rtk_dprx *dprx)
{
	// TODO: FIXME
	return true;
}

/**
 * rtk_dprx_clr_valid_video_check - Clr DP Valid Video Check
 */
static void rtk_dprx_clr_valid_video_check(struct rtk_dprx *dprx)
{
	// TODO: FIXME
}

/**
 * rtk_dprx_active_link_status_irq - Active DP IRQ
 */
static void rtk_dprx_active_link_status_irq(struct rtk_dprx *dprx)
{
	// TODO: FIXME
	// ScalerTimerActiveTimerEvent(SEC(0.1), _SCALER_TIMER_EVENT_DP_RX0_LINK_STATUS_IRQ);
}

/**
 * rtk_dprx_cancel_link_status_irq - Cancel DP IRQ
 */
static void rtk_dprx_cancel_link_status_irq(struct rtk_dprx *dprx)
{
	// TODO: FIXME
	// ScalerTimerCancelTimerEvent(_SCALER_TIMER_EVENT_DP_RX0_LINK_STATUS_IRQ);
}

static const struct rtk_dprx_aux_ops dprx_aux_ops = {
	.set_manual_mode = set_dp_aux_rx0_manual_mode,
	.set_auto_mode = set_dp_aux_rx0_auto_mode,
	.set_defer_mode = set_dp_aux_defer_mode,
	.get_manual_mode_status = rtk_dprx_get_manual_mode_status,
	.initial = rtk_dprx_aux_initial,
	.set_pn_swap = rtk_dprx_set_pn_swap,
	.change_dpcd_version = rtk_dprx_change_dpcd_version,
	.set_branch_dpcd_ident = rtk_dprx_set_branch_dpcd_ident,
	.reset_dpcd_link_status = rtk_dprx_reset_dpcd_link_status,
	.link_status_irq = rtk_dprx_link_status_irq,
	.aux_power_on = rtk_dprx_aux_power_on,
	.hpd_irq_assert = rtk_dprx_hpd_irq_assert,
	.set_sink_status = rtk_dprx_set_sink_status,
	.get_dpcd_info = rtk_dprx_get_dpcd_info,
	.get_dpcd_bit_info = rtk_dprx_get_dpcd_bit_info,
	.set_dpcd_value = rtk_dprx_set_dpcd_value,
	.set_dpcd_write_value = rtk_dprx_set_dpcd_write_value,
	.set_dpcd_write1_clear_value = rtk_dprx_set_dpcd_write1_clear_value,
	.set_dpcd_bit_value = rtk_dprx_set_dpcd_bit_value,
	.set_dpcd_bit_write_value = rtk_dprx_set_dpcd_bit_write_value,
	.get_dpcd_addr = rtk_dprx_get_dpcd_addr,
	.set_dpcd_addr = rtk_dprx_set_dpcd_addr,
	.get_valid_video_check = rtk_dprx_get_valid_video_check,
	.clr_valid_video_check = rtk_dprx_clr_valid_video_check,
	.active_link_status_irq = rtk_dprx_active_link_status_irq,
	.cancel_link_status_irq = rtk_dprx_cancel_link_status_irq,
};

int rtk_dprx_aux_init(struct rtk_dprx *dprx)
{

	dprx->aux_ops = &dprx_aux_ops;

	/* Sink Device-specific field */
	dprx->ident.oui[0] = 0x00;
	dprx->ident.oui[1] = 0xE0;
	dprx->ident.oui[2] = 0x4C;
	dprx->ident.device_id[0] = 'd';
	dprx->ident.device_id[1] = 'p';
	dprx->ident.device_id[2] = 'r';
	dprx->ident.device_id[3] = 'x';
	dprx->ident.device_id[4] = ' ';
	dprx->ident.device_id[5] = ' ';
	dprx->ident.hw_rev = 0;
	dprx->ident.sw_major_rev = 0;
	dprx->ident.sw_minor_rev = 0;

	return 0;
}

