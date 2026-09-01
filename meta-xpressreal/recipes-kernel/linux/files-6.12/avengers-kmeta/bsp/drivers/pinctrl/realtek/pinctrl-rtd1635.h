/* SPDX-License-Identifier: GPL-2.0-or-later*/

/*
 * Realtek DHC pin controller driver
 *
 * Copyright (c) 2019 Realtek Semiconductor Corp.
 */

#ifndef PINCTRL_RTD1635_H
#define PINCTRL_RTD1635_H


#define RTD1635_FUNC(_group, _name) \
	{ \
		.name = # _name, \
		.groups = rtd1635_ ## _group ## _ ## _name ## _groups, \
		.num_groups = ARRAY_SIZE(rtd1635_ ## _group ## _ ## _name ## _groups), \
	}

enum rtd1635_iso_pins_enum {
	RTD1635_ISO_GPIO_30 = 0,
	RTD1635_ISO_GPIO_31,
	RTD1635_ISO_GPIO_32,
	RTD1635_ISO_GPIO_33,
	RTD1635_ISO_GPIO_62,
	RTD1635_ISO_GPIO_63,
	RTD1635_ISO_GPIO_78,
	RTD1635_ISO_GPIO_79,
	RTD1635_ISO_GPIO_80,
	RTD1635_ISO_GPIO_81,
	RTD1635_ISO_GPIO_82,
	RTD1635_ISO_GPIO_83,
	RTD1635_ISO_GPIO_84,
	RTD1635_ISO_GPIO_85,
	RTD1635_ISO_GPIO_86,
	RTD1635_ISO_GPIO_87,
	RTD1635_ISO_GPIO_88,
	RTD1635_ISO_GPIO_89,
	RTD1635_ISO_GPIO_90,
	RTD1635_ISO_GPIO_91,
	RTD1635_ISO_GPIO_92,
	RTD1635_ISO_GPIO_93,
	RTD1635_ISO_GPIO_110,
	RTD1635_ISO_GPIO_111,
	RTD1635_ISO_GPIO_112,
	RTD1635_ISO_GPIO_113,
	RTD1635_ISO_GPIO_114,
	RTD1635_ISO_GPIO_115,
	RTD1635_ISO_GPIO_116,
	RTD1635_ISO_GPIO_117,
	RTD1635_ISO_GPIO_118,
	RTD1635_ISO_GPIO_119,
	RTD1635_ISO_GPIO_120,
	RTD1635_ISO_USB_CC1, /*gpio 121*/
	RTD1635_ISO_USB_CC2, /*gpio 122*/
	RTD1635_ISO_GPIO_123,
	RTD1635_ISO_GPIO_124,
	RTD1635_ISO_SCD_LOC,
	RTD1635_ISO_HI_WIDTH,
	RTD1635_ISO_EJTAG_SCPU_SWD_MODE_EN,
	RTD1635_ISO_SF_EN,
	RTD1635_ISO_ARM_TRACE_DBG_EN,
	RTD1635_ISO_EJTAG_AUCPU0_LOC,
	RTD1635_ISO_EJTAG_VE3_LOC,
	RTD1635_ISO_EJTAG_VE2_LOC,
	RTD1635_ISO_EJTAG_SCPU_LOC,
	RTD1635_ISO_EJTAG_PCPU_LOC,
	RTD1635_ISO_EJTAG_ACPU_LOC,
	RTD1635_ISO_I2C4_LOC,
	RTD1635_ISO_I2C6_LOC,
	RTD1635_ISO_I2C8_LOC,
	RTD1635_ISO_UART0_LOC,
	RTD1635_ISO_UART4_LOC,
	RTD1635_ISO_UART8_LOC,
	RTD1635_ISO_UART9_LOC,
	RTD1635_ISO_EIO_IRQ0_LOC,
	RTD1635_ISO_EIO_IRQ1_LOC,
	RTD1635_ISO_EIO_IRQ2_LOC,
	RTD1635_ISO_AI_I2S0_LOC,
	RTD1635_ISO_AI_I2S1_LOC,
	RTD1635_ISO_HI_ENABLE,
};

static const struct pinctrl_pin_desc rtd1635_iso_pins[] = {
	PINCTRL_PIN(RTD1635_ISO_GPIO_30, "gpio_30"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_31, "gpio_31"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_32, "gpio_32"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_33, "gpio_33"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_62, "gpio_62"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_63, "gpio_63"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_78, "gpio_78"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_79, "gpio_79"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_80, "gpio_80"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_81, "gpio_81"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_82, "gpio_82"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_83, "gpio_83"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_84, "gpio_84"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_85, "gpio_85"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_86, "gpio_86"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_87, "gpio_87"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_88, "gpio_88"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_89, "gpio_89"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_90, "gpio_90"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_91, "gpio_91"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_92, "gpio_92"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_93, "gpio_93"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_110, "gpio_110"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_111, "gpio_111"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_112, "gpio_112"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_113, "gpio_113"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_114, "gpio_114"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_115, "gpio_115"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_116, "gpio_116"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_117, "gpio_117"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_118, "gpio_118"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_119, "gpio_119"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_120, "gpio_120"),
	PINCTRL_PIN(RTD1635_ISO_USB_CC1, "usb_cc1"),
	PINCTRL_PIN(RTD1635_ISO_USB_CC2, "usb_cc2"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_123, "gpio_123"),
	PINCTRL_PIN(RTD1635_ISO_GPIO_124, "gpio_124"),
	PINCTRL_PIN(RTD1635_ISO_SCD_LOC, "scd_loc"),
	PINCTRL_PIN(RTD1635_ISO_HI_WIDTH, "hi_width"),
	PINCTRL_PIN(RTD1635_ISO_EJTAG_SCPU_SWD_MODE_EN, "ejtag_scpu_swd_mode_en"),
	PINCTRL_PIN(RTD1635_ISO_SF_EN, "sf_en"),
	PINCTRL_PIN(RTD1635_ISO_ARM_TRACE_DBG_EN, "arm_trace_dbg_en"),
	PINCTRL_PIN(RTD1635_ISO_EJTAG_AUCPU0_LOC, "ejtag_aucpu0_loc"),
	PINCTRL_PIN(RTD1635_ISO_EJTAG_VE3_LOC, "ejtag_ve3_loc"),
	PINCTRL_PIN(RTD1635_ISO_EJTAG_VE2_LOC, "ejtag_ve2_loc"),
	PINCTRL_PIN(RTD1635_ISO_EJTAG_SCPU_LOC, "ejtag_scpu_loc"),
	PINCTRL_PIN(RTD1635_ISO_EJTAG_PCPU_LOC, "ejtag_pcpu_loc"),
	PINCTRL_PIN(RTD1635_ISO_EJTAG_ACPU_LOC, "ejtag_acpu_loc"),
	PINCTRL_PIN(RTD1635_ISO_I2C4_LOC, "i2c4_loc"),
	PINCTRL_PIN(RTD1635_ISO_I2C6_LOC, "i2c6_loc"),
	PINCTRL_PIN(RTD1635_ISO_I2C8_LOC, "i2c8_loc"),
	PINCTRL_PIN(RTD1635_ISO_UART0_LOC, "uart0_loc"),
	PINCTRL_PIN(RTD1635_ISO_UART4_LOC, "uart4_loc"),
	PINCTRL_PIN(RTD1635_ISO_UART8_LOC, "uart8_loc"),
	PINCTRL_PIN(RTD1635_ISO_UART9_LOC, "uart9_loc"),
	PINCTRL_PIN(RTD1635_ISO_EIO_IRQ0_LOC, "eio_irq0_loc"),
	PINCTRL_PIN(RTD1635_ISO_EIO_IRQ1_LOC, "eio_irq1_loc"),
	PINCTRL_PIN(RTD1635_ISO_EIO_IRQ2_LOC, "eio_irq2_loc"),
	PINCTRL_PIN(RTD1635_ISO_AI_I2S0_LOC, "ai_i2s0_loc"),
	PINCTRL_PIN(RTD1635_ISO_AI_I2S1_LOC, "ai_i2s1_loc"),
	PINCTRL_PIN(RTD1635_ISO_HI_ENABLE, "hi_enable"),
};

enum rtd1635_isom_pins_enum {
	RTD1635_ISOM_GPIO_0 = 0,
	RTD1635_ISOM_GPIO_1,
	RTD1635_ISOM_GPIO_28,
	RTD1635_ISOM_GPIO_29,
	RTD1635_ISOM_IR_RX_LOC,
};

static const struct pinctrl_pin_desc rtd1635_isom_pins[] = {
	PINCTRL_PIN(RTD1635_ISOM_GPIO_0, "gpio_0"),
	PINCTRL_PIN(RTD1635_ISOM_GPIO_1, "gpio_1"),
	PINCTRL_PIN(RTD1635_ISOM_GPIO_28, "gpio_28"),
	PINCTRL_PIN(RTD1635_ISOM_GPIO_29, "gpio_29"),
	PINCTRL_PIN(RTD1635_ISOM_IR_RX_LOC, "ir_rx_loc"),
};

enum rtd1635_main2_pins_enum {
	RTD1635_MAIN2_GPIO_8 = 0,
	RTD1635_MAIN2_GPIO_9,
	RTD1635_MAIN2_GPIO_10,
	RTD1635_MAIN2_GPIO_11,
	RTD1635_MAIN2_GPIO_16,
	RTD1635_MAIN2_GPIO_17,
	RTD1635_MAIN2_GPIO_18,
	RTD1635_MAIN2_GPIO_19,
	RTD1635_MAIN2_GPIO_20,
	RTD1635_MAIN2_GPIO_21,
	RTD1635_MAIN2_GPIO_22,
	RTD1635_MAIN2_GPIO_23,
	RTD1635_MAIN2_HIF_DATA, /*gpio 40*/
	RTD1635_MAIN2_HIF_EN,   /*gpio 41*/
	RTD1635_MAIN2_HIF_RDY,  /*gpio 42*/
	RTD1635_MAIN2_HIF_CLK,  /*gpio 43*/
	RTD1635_MAIN2_GPIO_44,
	RTD1635_MAIN2_GPIO_45,
	RTD1635_MAIN2_EMMC_DATA_0, /*gpio 49*/
	RTD1635_MAIN2_EMMC_DATA_1, /*gpio 50*/
	RTD1635_MAIN2_EMMC_DATA_2, /*gpio 51*/
	RTD1635_MAIN2_EMMC_DATA_3, /*gpio 52*/
	RTD1635_MAIN2_EMMC_DATA_4, /*gpio 53*/
	RTD1635_MAIN2_EMMC_DATA_5, /*gpio 54*/
	RTD1635_MAIN2_EMMC_DATA_6, /*gpio 55*/
	RTD1635_MAIN2_EMMC_DATA_7, /*gpio 56*/
	RTD1635_MAIN2_EMMC_RST_N, /*gpio 57*/
	RTD1635_MAIN2_EMMC_CMD,   /*gpio 58*/
	RTD1635_MAIN2_EMMC_CLK,   /*gpio 59*/
	RTD1635_MAIN2_EMMC_DD_SB, /*gpio 60*/
	RTD1635_MAIN2_GPIO_64,
	RTD1635_MAIN2_GPIO_65,
	RTD1635_MAIN2_GPIO_66,
	RTD1635_MAIN2_GPIO_67,
	RTD1635_MAIN2_GPIO_76,
	RTD1635_MAIN2_GPIO_77,
};

static const struct pinctrl_pin_desc rtd1635_main2_pins[] = {
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_8, "gpio_8"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_9, "gpio_9"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_10, "gpio_10"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_11, "gpio_11"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_16, "gpio_16"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_17, "gpio_17"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_18, "gpio_18"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_19, "gpio_19"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_20, "gpio_20"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_21, "gpio_21"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_22, "gpio_22"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_23, "gpio_23"),
	PINCTRL_PIN(RTD1635_MAIN2_HIF_DATA, "hif_data"),
	PINCTRL_PIN(RTD1635_MAIN2_HIF_EN,   "hif_en"),
	PINCTRL_PIN(RTD1635_MAIN2_HIF_RDY,  "hif_rdy"),
	PINCTRL_PIN(RTD1635_MAIN2_HIF_CLK,  "hif_clk"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_44, "gpio_44"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_45, "gpio_45"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DATA_0, "emmc_data_0"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DATA_1, "emmc_data_1"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DATA_2, "emmc_data_2"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DATA_3, "emmc_data_3"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DATA_4, "emmc_data_4"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DATA_5, "emmc_data_5"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DATA_6, "emmc_data_6"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DATA_7, "emmc_data_7"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_RST_N, "emmc_rst_n"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_CMD,   "emmc_cmd"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_CLK,   "emmc_clk"),
	PINCTRL_PIN(RTD1635_MAIN2_EMMC_DD_SB, "emmc_dd_sb"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_64, "gpio_64"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_65, "gpio_65"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_66, "gpio_66"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_67, "gpio_67"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_76, "gpio_76"),
	PINCTRL_PIN(RTD1635_MAIN2_GPIO_77, "gpio_77"),
};

enum rtd1635_npu_pins_enum {
	RTD1635_NPU_GPIO_4 = 0,
	RTD1635_NPU_GPIO_5,
	RTD1635_NPU_GPIO_6,
	RTD1635_NPU_GPIO_7,
	RTD1635_NPU_GPIO_12,
	RTD1635_NPU_GPIO_13,
	RTD1635_NPU_GPIO_14,
	RTD1635_NPU_GPIO_15,
	RTD1635_NPU_GPIO_38,
	RTD1635_NPU_GPIO_39,
	RTD1635_NPU_GPIO_68,
	RTD1635_NPU_GPIO_69,
	RTD1635_NPU_GPIO_70,
	RTD1635_NPU_GPIO_71,
	RTD1635_NPU_GPIO_72,
	RTD1635_NPU_GPIO_73,
	RTD1635_NPU_GPIO_74,
	RTD1635_NPU_GPIO_75,
	RTD1635_NPU_GPIO_104,
	RTD1635_NPU_GPIO_105,
	RTD1635_NPU_GPIO_129,
	RTD1635_NPU_GPIO_130,
	RTD1635_NPU_GPIO_131,
	RTD1635_NPU_GPIO_132,
	RTD1635_NPU_GPIO_133,
	RTD1635_NPU_GPIO_134,
};

static const struct pinctrl_pin_desc rtd1635_npu_pins[] = {
	PINCTRL_PIN(RTD1635_NPU_GPIO_4, "gpio_4"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_5, "gpio_5"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_6, "gpio_6"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_7, "gpio_7"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_12, "gpio_12"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_13, "gpio_13"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_14, "gpio_14"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_15, "gpio_15"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_38, "gpio_38"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_39, "gpio_39"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_68, "gpio_68"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_69, "gpio_69"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_70, "gpio_70"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_71, "gpio_71"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_72, "gpio_72"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_73, "gpio_73"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_74, "gpio_74"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_75, "gpio_75"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_104, "gpio_104"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_105, "gpio_105"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_129, "gpio_129"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_130, "gpio_130"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_131, "gpio_131"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_132, "gpio_132"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_133, "gpio_133"),
	PINCTRL_PIN(RTD1635_NPU_GPIO_134, "gpio_134"),
};

enum rtd1635_ve2_pins_enum {
	RTD1635_VE2_GPIO_46 = 0,
	RTD1635_VE2_GPIO_47,
	RTD1635_VE2_GPIO_48,
	RTD1635_VE2_GPIO_96,
	RTD1635_VE2_GPIO_97,
	RTD1635_VE2_GPIO_98,
	RTD1635_VE2_GPIO_99,
	RTD1635_VE2_GPIO_100,
	RTD1635_VE2_GPIO_101,
	RTD1635_VE2_GPIO_125,
	RTD1635_VE2_GPIO_126,
	RTD1635_VE2_GPIO_127,
	RTD1635_VE2_GPIO_128,
};

static const struct pinctrl_pin_desc rtd1635_ve2_pins[] = {
	PINCTRL_PIN(RTD1635_VE2_GPIO_46, "gpio_46"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_47, "gpio_47"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_48, "gpio_48"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_96, "gpio_96"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_97, "gpio_97"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_98, "gpio_98"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_99, "gpio_99"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_100, "gpio_100"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_101, "gpio_101"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_125, "gpio_125"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_126, "gpio_126"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_127, "gpio_127"),
	PINCTRL_PIN(RTD1635_VE2_GPIO_128, "gpio_128"),
};

enum rtd1635_ve3_pins_enum {
	RTD1635_VE3_GPIO_2 = 0,
	RTD1635_VE3_GPIO_3,
	RTD1635_VE3_GPIO_24,
	RTD1635_VE3_GPIO_25,
	RTD1635_VE3_GPIO_26,
	RTD1635_VE3_GPIO_27,
	RTD1635_VE3_GPIO_34,
	RTD1635_VE3_GPIO_35,
	RTD1635_VE3_GPIO_36,
	RTD1635_VE3_GPIO_37,
	RTD1635_VE3_GPIO_61,
	RTD1635_VE3_GPIO_94,
	RTD1635_VE3_GPIO_95,
	RTD1635_VE3_GPIO_102,
	RTD1635_VE3_GPIO_103,
	RTD1635_VE3_GPIO_106,
	RTD1635_VE3_GPIO_107,
	RTD1635_VE3_GPIO_108,
	RTD1635_VE3_GPIO_109,
};

static const struct pinctrl_pin_desc rtd1635_ve3_pins[] = {
	PINCTRL_PIN(RTD1635_VE3_GPIO_2, "gpio_2"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_3, "gpio_3"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_24, "gpio_24"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_25, "gpio_25"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_26, "gpio_26"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_27, "gpio_27"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_34, "gpio_34"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_35, "gpio_35"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_36, "gpio_36"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_37, "gpio_37"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_61, "gpio_61"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_94, "gpio_94"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_95, "gpio_95"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_102, "gpio_102"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_103, "gpio_103"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_106, "gpio_106"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_107, "gpio_107"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_108, "gpio_108"),
	PINCTRL_PIN(RTD1635_VE3_GPIO_109, "gpio_109"),
};

#define DECLARE_RTD1635_PIN(_pin, _name) static const unsigned int rtd1635_## _name ##_pins[] = { _pin }

DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_30, gpio_30);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_31, gpio_31);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_32, gpio_32);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_33, gpio_33);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_62, gpio_62);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_63, gpio_63);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_78, gpio_78);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_79, gpio_79);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_80, gpio_80);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_81, gpio_81);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_82, gpio_82);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_83, gpio_83);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_84, gpio_84);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_85, gpio_85);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_86, gpio_86);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_87, gpio_87);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_88, gpio_88);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_89, gpio_89);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_90, gpio_90);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_91, gpio_91);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_92, gpio_92);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_93, gpio_93);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_110, gpio_110);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_111, gpio_111);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_112, gpio_112);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_113, gpio_113);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_114, gpio_114);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_115, gpio_115);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_116, gpio_116);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_117, gpio_117);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_118, gpio_118);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_119, gpio_119);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_120, gpio_120);
DECLARE_RTD1635_PIN(RTD1635_ISO_USB_CC1, usb_cc1);
DECLARE_RTD1635_PIN(RTD1635_ISO_USB_CC2, usb_cc2);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_123, gpio_123);
DECLARE_RTD1635_PIN(RTD1635_ISO_GPIO_124, gpio_124);
DECLARE_RTD1635_PIN(RTD1635_ISO_SCD_LOC, scd_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_HI_WIDTH, hi_width);
DECLARE_RTD1635_PIN(RTD1635_ISO_EJTAG_SCPU_SWD_MODE_EN, ejtag_scpu_swd_mode_en);
DECLARE_RTD1635_PIN(RTD1635_ISO_SF_EN, sf_en);
DECLARE_RTD1635_PIN(RTD1635_ISO_ARM_TRACE_DBG_EN, arm_trace_dbg_en);
DECLARE_RTD1635_PIN(RTD1635_ISO_EJTAG_AUCPU0_LOC, ejtag_aucpu0_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_EJTAG_VE3_LOC, ejtag_ve3_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_EJTAG_VE2_LOC, ejtag_ve2_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_EJTAG_SCPU_LOC, ejtag_scpu_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_EJTAG_PCPU_LOC, ejtag_pcpu_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_EJTAG_ACPU_LOC, ejtag_acpu_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_I2C4_LOC, i2c4_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_I2C6_LOC, i2c6_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_I2C8_LOC, i2c8_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_UART0_LOC, uart0_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_UART4_LOC, uart4_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_UART8_LOC, uart8_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_UART9_LOC, uart9_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_EIO_IRQ0_LOC, eio_irq0_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_EIO_IRQ1_LOC, eio_irq1_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_EIO_IRQ2_LOC, eio_irq2_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_AI_I2S0_LOC, ai_i2s0_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_AI_I2S1_LOC, ai_i2s1_loc);
DECLARE_RTD1635_PIN(RTD1635_ISO_HI_ENABLE, hi_enable);
DECLARE_RTD1635_PIN(RTD1635_ISOM_GPIO_0, gpio_0);
DECLARE_RTD1635_PIN(RTD1635_ISOM_GPIO_1, gpio_1);
DECLARE_RTD1635_PIN(RTD1635_ISOM_GPIO_28, gpio_28);
DECLARE_RTD1635_PIN(RTD1635_ISOM_GPIO_29, gpio_29);
DECLARE_RTD1635_PIN(RTD1635_ISOM_IR_RX_LOC, ir_rx_loc);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_8, gpio_8);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_9, gpio_9);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_10, gpio_10);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_11, gpio_11);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_16, gpio_16);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_17, gpio_17);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_18, gpio_18);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_19, gpio_19);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_20, gpio_20);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_21, gpio_21);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_22, gpio_22);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_23, gpio_23);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_HIF_DATA, hif_data);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_HIF_EN, hif_en);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_HIF_RDY, hif_rdy);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_HIF_CLK, hif_clk);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_44, gpio_44);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_45, gpio_45);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DATA_0, emmc_data_0);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DATA_1, emmc_data_1);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DATA_2, emmc_data_2);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DATA_3, emmc_data_3);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DATA_4, emmc_data_4);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DATA_5, emmc_data_5);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DATA_6, emmc_data_6);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DATA_7, emmc_data_7);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_RST_N, emmc_rst_n);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_CMD, emmc_cmd);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_CLK, emmc_clk);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_EMMC_DD_SB, emmc_dd_sb);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_64, gpio_64);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_65, gpio_65);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_66, gpio_66);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_67, gpio_67);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_76, gpio_76);
DECLARE_RTD1635_PIN(RTD1635_MAIN2_GPIO_77, gpio_77);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_4, gpio_4);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_5, gpio_5);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_6, gpio_6);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_7, gpio_7);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_12, gpio_12);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_13, gpio_13);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_14, gpio_14);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_15, gpio_15);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_38, gpio_38);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_39, gpio_39);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_68, gpio_68);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_69, gpio_69);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_70, gpio_70);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_71, gpio_71);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_72, gpio_72);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_73, gpio_73);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_74, gpio_74);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_75, gpio_75);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_104, gpio_104);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_105, gpio_105);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_129, gpio_129);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_130, gpio_130);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_131, gpio_131);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_132, gpio_132);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_133, gpio_133);
DECLARE_RTD1635_PIN(RTD1635_NPU_GPIO_134, gpio_134);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_46, gpio_46);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_47, gpio_47);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_48, gpio_48);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_96, gpio_96);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_97, gpio_97);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_98, gpio_98);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_99, gpio_99);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_100, gpio_100);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_101, gpio_101);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_125, gpio_125);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_126, gpio_126);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_127, gpio_127);
DECLARE_RTD1635_PIN(RTD1635_VE2_GPIO_128, gpio_128);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_2, gpio_2);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_3, gpio_3);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_24, gpio_24);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_25, gpio_25);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_26, gpio_26);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_27, gpio_27);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_34, gpio_34);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_35, gpio_35);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_36, gpio_36);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_37, gpio_37);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_61, gpio_61);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_94, gpio_94);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_95, gpio_95);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_102, gpio_102);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_103, gpio_103);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_106, gpio_106);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_107, gpio_107);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_108, gpio_108);
DECLARE_RTD1635_PIN(RTD1635_VE3_GPIO_109, gpio_109);

#define RTD1635_GROUP(_name) \
	{ \
		.name = # _name, \
		.pins = rtd1635_ ## _name ## _pins, \
		.num_pins = ARRAY_SIZE(rtd1635_ ## _name ## _pins), \
	}

static const struct rtd_pin_group_desc rtd1635_iso_pin_groups[] = {
	RTD1635_GROUP(gpio_30),
	RTD1635_GROUP(gpio_31),
	RTD1635_GROUP(gpio_32),
	RTD1635_GROUP(gpio_33),
	RTD1635_GROUP(gpio_62),
	RTD1635_GROUP(gpio_63),
	RTD1635_GROUP(gpio_78),
	RTD1635_GROUP(gpio_79),
	RTD1635_GROUP(gpio_80),
	RTD1635_GROUP(gpio_81),
	RTD1635_GROUP(gpio_82),
	RTD1635_GROUP(gpio_83),
	RTD1635_GROUP(gpio_84),
	RTD1635_GROUP(gpio_85),
	RTD1635_GROUP(gpio_86),
	RTD1635_GROUP(gpio_87),
	RTD1635_GROUP(gpio_88),
	RTD1635_GROUP(gpio_89),
	RTD1635_GROUP(gpio_90),
	RTD1635_GROUP(gpio_91),
	RTD1635_GROUP(gpio_92),
	RTD1635_GROUP(gpio_93),
	RTD1635_GROUP(gpio_110),
	RTD1635_GROUP(gpio_111),
	RTD1635_GROUP(gpio_112),
	RTD1635_GROUP(gpio_113),
	RTD1635_GROUP(gpio_114),
	RTD1635_GROUP(gpio_115),
	RTD1635_GROUP(gpio_116),
	RTD1635_GROUP(gpio_117),
	RTD1635_GROUP(gpio_118),
	RTD1635_GROUP(gpio_119),
	RTD1635_GROUP(gpio_120),
	RTD1635_GROUP(usb_cc1),
	RTD1635_GROUP(usb_cc2),
	RTD1635_GROUP(gpio_123),
	RTD1635_GROUP(gpio_124),
	RTD1635_GROUP(scd_loc),
	RTD1635_GROUP(hi_width),
	RTD1635_GROUP(ejtag_scpu_swd_mode_en),
	RTD1635_GROUP(sf_en),
	RTD1635_GROUP(arm_trace_dbg_en),
	RTD1635_GROUP(ejtag_aucpu0_loc),
	RTD1635_GROUP(ejtag_ve3_loc),
	RTD1635_GROUP(ejtag_ve2_loc),
	RTD1635_GROUP(ejtag_scpu_loc),
	RTD1635_GROUP(ejtag_pcpu_loc),
	RTD1635_GROUP(ejtag_acpu_loc),
	RTD1635_GROUP(i2c4_loc),
	RTD1635_GROUP(i2c6_loc),
	RTD1635_GROUP(i2c8_loc),
	RTD1635_GROUP(uart0_loc),
	RTD1635_GROUP(uart4_loc),
	RTD1635_GROUP(uart8_loc),
	RTD1635_GROUP(uart9_loc),
	RTD1635_GROUP(eio_irq0_loc),
	RTD1635_GROUP(eio_irq1_loc),
	RTD1635_GROUP(eio_irq2_loc),
	RTD1635_GROUP(ai_i2s0_loc),
	RTD1635_GROUP(ai_i2s1_loc),
	RTD1635_GROUP(hi_enable),
};

static const struct rtd_pin_group_desc rtd1635_isom_pin_groups[] = {
	RTD1635_GROUP(gpio_0),
	RTD1635_GROUP(gpio_1),
	RTD1635_GROUP(gpio_28),
	RTD1635_GROUP(gpio_29),
	RTD1635_GROUP(ir_rx_loc),
};

static const struct rtd_pin_group_desc rtd1635_main2_pin_groups[] = {
	RTD1635_GROUP(gpio_8),
	RTD1635_GROUP(gpio_9),
	RTD1635_GROUP(gpio_10),
	RTD1635_GROUP(gpio_11),
	RTD1635_GROUP(gpio_16),
	RTD1635_GROUP(gpio_17),
	RTD1635_GROUP(gpio_18),
	RTD1635_GROUP(gpio_19),
	RTD1635_GROUP(gpio_20),
	RTD1635_GROUP(gpio_21),
	RTD1635_GROUP(gpio_22),
	RTD1635_GROUP(gpio_23),
	RTD1635_GROUP(hif_data),
	RTD1635_GROUP(hif_en),
	RTD1635_GROUP(hif_rdy),
	RTD1635_GROUP(hif_clk),
	RTD1635_GROUP(gpio_44),
	RTD1635_GROUP(gpio_45),
	RTD1635_GROUP(emmc_data_0),
	RTD1635_GROUP(emmc_data_1),
	RTD1635_GROUP(emmc_data_2),
	RTD1635_GROUP(emmc_data_3),
	RTD1635_GROUP(emmc_data_4),
	RTD1635_GROUP(emmc_data_5),
	RTD1635_GROUP(emmc_data_6),
	RTD1635_GROUP(emmc_data_7),
	RTD1635_GROUP(emmc_rst_n),
	RTD1635_GROUP(emmc_cmd),
	RTD1635_GROUP(emmc_clk),
	RTD1635_GROUP(emmc_dd_sb),
	RTD1635_GROUP(gpio_64),
	RTD1635_GROUP(gpio_65),
	RTD1635_GROUP(gpio_66),
	RTD1635_GROUP(gpio_67),
	RTD1635_GROUP(gpio_76),
	RTD1635_GROUP(gpio_77),
};

static const struct rtd_pin_group_desc rtd1635_npu_pin_groups[] = {
	RTD1635_GROUP(gpio_4),
	RTD1635_GROUP(gpio_5),
	RTD1635_GROUP(gpio_6),
	RTD1635_GROUP(gpio_7),
	RTD1635_GROUP(gpio_12),
	RTD1635_GROUP(gpio_13),
	RTD1635_GROUP(gpio_14),
	RTD1635_GROUP(gpio_15),
	RTD1635_GROUP(gpio_38),
	RTD1635_GROUP(gpio_39),
	RTD1635_GROUP(gpio_68),
	RTD1635_GROUP(gpio_69),
	RTD1635_GROUP(gpio_70),
	RTD1635_GROUP(gpio_71),
	RTD1635_GROUP(gpio_72),
	RTD1635_GROUP(gpio_73),
	RTD1635_GROUP(gpio_74),
	RTD1635_GROUP(gpio_75),
	RTD1635_GROUP(gpio_104),
	RTD1635_GROUP(gpio_105),
	RTD1635_GROUP(gpio_129),
	RTD1635_GROUP(gpio_130),
	RTD1635_GROUP(gpio_131),
	RTD1635_GROUP(gpio_132),
	RTD1635_GROUP(gpio_133),
	RTD1635_GROUP(gpio_134),
};

static const struct rtd_pin_group_desc rtd1635_ve2_pin_groups[] = {
	RTD1635_GROUP(gpio_46),
	RTD1635_GROUP(gpio_47),
	RTD1635_GROUP(gpio_48),
	RTD1635_GROUP(gpio_96),
	RTD1635_GROUP(gpio_97),
	RTD1635_GROUP(gpio_98),
	RTD1635_GROUP(gpio_99),
	RTD1635_GROUP(gpio_100),
	RTD1635_GROUP(gpio_101),
	RTD1635_GROUP(gpio_125),
	RTD1635_GROUP(gpio_126),
	RTD1635_GROUP(gpio_127),
	RTD1635_GROUP(gpio_128),
};

static const struct rtd_pin_group_desc rtd1635_ve3_pin_groups[] = {
	RTD1635_GROUP(gpio_2),
	RTD1635_GROUP(gpio_3),
	RTD1635_GROUP(gpio_24),
	RTD1635_GROUP(gpio_25),
	RTD1635_GROUP(gpio_26),
	RTD1635_GROUP(gpio_27),
	RTD1635_GROUP(gpio_34),
	RTD1635_GROUP(gpio_35),
	RTD1635_GROUP(gpio_36),
	RTD1635_GROUP(gpio_37),
	RTD1635_GROUP(gpio_61),
	RTD1635_GROUP(gpio_94),
	RTD1635_GROUP(gpio_95),
	RTD1635_GROUP(gpio_102),
	RTD1635_GROUP(gpio_103),
	RTD1635_GROUP(gpio_106),
	RTD1635_GROUP(gpio_107),
	RTD1635_GROUP(gpio_108),
	RTD1635_GROUP(gpio_109),
};

static const char * const rtd1635_iso_gpio_groups[] = {
	"gpio_110", "gpio_111", "gpio_112", "gpio_113", "gpio_114", "gpio_115",
	"gpio_116", "gpio_117", "gpio_118", "gpio_119", "gpio_120", "gpio_123",
	"gpio_124", "gpio_30", "gpio_31", "gpio_32", "gpio_33", "gpio_62",
	"gpio_63", "gpio_78", "gpio_79", "gpio_80", "gpio_81", "gpio_82", "gpio_83",
	"gpio_84", "gpio_85", "gpio_86", "gpio_87", "gpio_88", "gpio_89", "gpio_90",
	"gpio_91", "gpio_92", "gpio_93", "usb_cc1", "usb_cc2"
};
static const char * const rtd1635_iso_uart8_loc1_groups[] = {
	"gpio_30", "gpio_31", "gpio_32", "gpio_33", "uart8_loc"
};
static const char * const rtd1635_iso_gspi3_groups[] = {
	"gpio_30", "gpio_31", "gpio_32", "gpio_33"
};
static const char * const rtd1635_iso_pwm4_loc0_groups[] = {
	"gpio_30"
};
static const char * const rtd1635_iso_ts1_groups[] = {
	"gpio_30", "gpio_31", "gpio_32", "gpio_33"
};
static const char * const rtd1635_iso_iso_tristate_groups[] = {
	"gpio_110", "gpio_111", "gpio_112", "gpio_113", "gpio_114", "gpio_115",
	"gpio_116", "gpio_117", "gpio_118", "gpio_123", "gpio_124", "gpio_30",
	"gpio_31", "gpio_32", "gpio_33", "gpio_62", "gpio_63", "gpio_83", "gpio_84",
	"gpio_89", "gpio_90", "gpio_91", "gpio_92", "gpio_93", "usb_cc1", "usb_cc2"
};
static const char * const rtd1635_iso_pwm5_loc0_groups[] = {
	"gpio_31"
};
static const char * const rtd1635_iso_pll_test0_loc0_groups[] = {
	"gpio_62"
};
static const char * const rtd1635_iso_pll_test0_loc1_groups[] = {
	"gpio_63"
};
static const char * const rtd1635_iso_vtci_i2s_groups[] = {
	"gpio_78", "gpio_79", "gpio_80", "gpio_81", "gpio_82"
};
static const char * const rtd1635_iso_ai_i2s0_loc0_groups[] = {
	"ai_i2s0_loc", "gpio_78", "gpio_79", "gpio_80", "gpio_81", "gpio_82",
	"gpio_83", "gpio_84"
};
static const char * const rtd1635_iso_ao_i2s0_groups[] = {
	"gpio_78", "gpio_79", "gpio_80", "gpio_85", "gpio_86", "gpio_87", "gpio_88"
};
static const char * const rtd1635_iso_ao_tdm0_groups[] = {
	"gpio_78", "gpio_79", "gpio_80", "gpio_85"
};
static const char * const rtd1635_iso_dbg_out1_groups[] = {
	"gpio_78", "gpio_79", "gpio_80", "gpio_81", "gpio_82"
};
static const char * const rtd1635_iso_ai_saibb0_groups[] = {
	"gpio_81", "gpio_82", "gpio_83", "gpio_84"
};
static const char * const rtd1635_iso_dbg_out0_groups[] = {
	"gpio_119", "gpio_120", "gpio_85", "gpio_86", "gpio_87", "gpio_88"
};
static const char * const rtd1635_iso_ao_tdm1_groups[] = {
	"gpio_86", "gpio_87", "gpio_88", "gpio_93"
};
static const char * const rtd1635_iso_ao_tdm1_loc1_groups[] = {
	"gpio_88"
};
static const char * const rtd1635_iso_vtco_i2s_groups[] = {
	"gpio_89", "gpio_90", "gpio_91", "gpio_92"
};
static const char * const rtd1635_iso_ai_saibb1_groups[] = {
	"gpio_89", "gpio_90", "gpio_91", "gpio_93"
};
static const char * const rtd1635_iso_ai_i2s1_loc0_groups[] = {
	"ai_i2s1_loc", "gpio_89", "gpio_90", "gpio_91", "gpio_92", "gpio_93"
};
static const char * const rtd1635_iso_ai_tdm0_groups[] = {
	"gpio_89", "gpio_90", "gpio_91", "gpio_92"
};

static const char * const rtd1635_iso_vi_dtv_groups[] = {
	"gpio_110", "gpio_111", "gpio_112", "gpio_113", "gpio_114", "gpio_115",
	"gpio_116", "gpio_117", "gpio_118"
};
static const char * const rtd1635_iso_i2c8_loc0_groups[] = {
	"gpio_119", "gpio_120", "i2c8_loc"
};
static const char * const rtd1635_iso_usb_cc1_groups[] = {
	"usb_cc1"
};
static const char * const rtd1635_iso_usb_cc2_groups[] = {
	"usb_cc2"
};
static const char * const rtd1635_iso_uart4_loc2_groups[] = {
	"gpio_123", "gpio_124", "uart4_loc"
};
static const char * const rtd1635_iso_i2c4_loc2_groups[] = {
	"gpio_123", "gpio_124", "i2c4_loc"
};
static const char * const rtd1635_iso_pwm0_loc2_groups[] = {
	"gpio_123"
};
static const char * const rtd1635_iso_eio_irq1_loc1_groups[] = {
	"eio_irq1_loc", "gpio_123"
};
static const char * const rtd1635_iso_uart0_loc2_groups[] = {
	"gpio_123", "gpio_124", "uart0_loc"
};
static const char * const rtd1635_iso_pwm1_loc2_groups[] = {
	"gpio_124"
};
static const char * const rtd1635_iso_eio_irq2_loc1_groups[] = {
	"eio_irq2_loc", "gpio_124"
};
static const char * const rtd1635_iso_scd_scpu_loc0_groups[] = {
	"scd_loc"
};
static const char * const rtd1635_iso_scd_scpu_loc1_groups[] = {
	"scd_loc"
};
static const char * const rtd1635_iso_hi_width_disable_groups[] = {
	"hi_width"
};
static const char * const rtd1635_iso_hi_width_1bit_groups[] = {
	"hi_width"
};
static const char * const rtd1635_iso_scpu_swd_disable_groups[] = {
	"arm_trace_dbg_en", "ejtag_scpu_swd_mode_en"
};
static const char * const rtd1635_iso_scpu_swd_enable_groups[] = {
	"arm_trace_dbg_en", "ejtag_scpu_swd_mode_en"
};
static const char * const rtd1635_iso_sf_disable_groups[] = {
	"sf_en"
};
static const char * const rtd1635_iso_sf_enable_groups[] = {
	"sf_en"
};
static const char * const rtd1635_iso_aucpu0_ejtag_loc0_groups[] = {
	"ejtag_aucpu0_loc"
};
static const char * const rtd1635_iso_aucpu0_ejtag_loc1_groups[] = {
	"ejtag_aucpu0_loc"
};
static const char * const rtd1635_iso_ve3_vcpu_ejtag_loc0_groups[] = {
	"ejtag_ve3_loc"
};
static const char * const rtd1635_iso_ve3_vcpu_ejtag_loc1_groups[] = {
	"ejtag_ve3_loc"
};
static const char * const rtd1635_iso_ve2_vcpu_ejtag_loc0_groups[] = {
	"ejtag_ve2_loc"
};
static const char * const rtd1635_iso_ve2_vcpu_ejtag_loc1_groups[] = {
	"ejtag_ve2_loc"
};
static const char * const rtd1635_iso_scpu_ejtag_loc0_groups[] = {
	"ejtag_scpu_loc"
};
static const char * const rtd1635_iso_scpu_ejtag_loc1_groups[] = {
	"ejtag_scpu_loc"
};
static const char * const rtd1635_iso_pcpu_ejtag_loc0_groups[] = {
	"ejtag_pcpu_loc"
};
static const char * const rtd1635_iso_pcpu_ejtag_loc1_groups[] = {
	"ejtag_pcpu_loc"
};
static const char * const rtd1635_iso_acpu_ejtag_loc0_groups[] = {
	"ejtag_acpu_loc"
};
static const char * const rtd1635_iso_acpu_ejtag_loc1_groups[] = {
	"ejtag_acpu_loc"
};
static const char * const rtd1635_iso_i2c4_loc0_groups[] = {
	"i2c4_loc"
};
static const char * const rtd1635_iso_i2c4_loc1_groups[] = {
	"i2c4_loc"
};
static const char * const rtd1635_iso_i2c6_loc0_groups[] = {
	"i2c6_loc"
};
static const char * const rtd1635_iso_i2c6_loc1_groups[] = {
	"i2c6_loc"
};
static const char * const rtd1635_iso_i2c6_loc2_groups[] = {
	"i2c6_loc"
};
static const char * const rtd1635_iso_i2c6_loc3_groups[] = {
	"i2c6_loc"
};
static const char * const rtd1635_iso_i2c8_loc1_groups[] = {
	"i2c8_loc"
};
static const char * const rtd1635_iso_uart0_loc0_groups[] = {
	"uart0_loc"
};
static const char * const rtd1635_iso_uart0_loc1_groups[] = {
	"uart0_loc"
};
static const char * const rtd1635_iso_uart4_loc0_groups[] = {
	"uart4_loc"
};
static const char * const rtd1635_iso_uart4_loc1_groups[] = {
	"uart4_loc"
};
static const char * const rtd1635_iso_uart8_loc0_groups[] = {
	"uart8_loc"
};
static const char * const rtd1635_iso_uart9_loc0_groups[] = {
	"uart9_loc"
};
static const char * const rtd1635_iso_uart9_loc1_groups[] = {
	"uart9_loc"
};
static const char * const rtd1635_iso_uart9_loc2_groups[] = {
	"uart9_loc"
};
static const char * const rtd1635_iso_eio_irq0_loc0_groups[] = {
	"eio_irq0_loc"
};
static const char * const rtd1635_iso_eio_irq0_loc1_groups[] = {
	"eio_irq0_loc"
};
static const char * const rtd1635_iso_eio_irq1_loc0_groups[] = {
	"eio_irq1_loc"
};
static const char * const rtd1635_iso_eio_irq2_loc0_groups[] = {
	"eio_irq2_loc"
};
static const char * const rtd1635_iso_ai_i2s0_loc1_groups[] = {
	"ai_i2s0_loc"
};
static const char * const rtd1635_iso_ai_i2s1_loc1_groups[] = {
	"ai_i2s1_loc"
};
static const char * const rtd1635_iso_hif_disable_groups[] = {
	"hi_enable"
};
static const char * const rtd1635_iso_hif_enable_groups[] = {
	"hi_enable"
};
static const char * const rtd1635_isom_gpio_groups[] = {
	"gpio_0", "gpio_1", "gpio_28", "gpio_29"
};
static const char * const rtd1635_isom_pctrl_groups[] = {
	"gpio_0", "gpio_1", "gpio_28", "gpio_29"
};
static const char * const rtd1635_isom_eio_irq0_loc0_groups[] = {
	"gpio_0"
};
static const char * const rtd1635_isom_iso_tristate_groups[] = {
	"gpio_0", "gpio_1", "gpio_28", "gpio_29"
};
static const char * const rtd1635_isom_ir_rx_loc0_groups[] = {
	"gpio_1", "ir_rx_loc"
};
static const char * const rtd1635_isom_uart10_groups[] = {
	"gpio_28", "gpio_29"
};
static const char * const rtd1635_isom_isom_dbg_out0_groups[] = {
	"gpio_28", "gpio_29"
};
static const char * const rtd1635_isom_i2c8_loc1_groups[] = {
	"gpio_28", "gpio_29"
};
static const char * const rtd1635_isom_ir_rx_loc1_groups[] = {
	"gpio_29", "ir_rx_loc"
};
static const char * const rtd1635_isom_ir_rx_loc2_groups[] = {
	"ir_rx_loc"
};
static const char * const rtd1635_main2_gpio_groups[] = {
	"emmc_clk", "emmc_cmd", "emmc_data_0", "emmc_data_1", "emmc_data_2",
	"emmc_data_3", "emmc_data_4", "emmc_data_5", "emmc_data_6", "emmc_data_7",
	"emmc_dd_sb", "emmc_rst_n", "gpio_10", "gpio_11", "gpio_16", "gpio_17",
	"gpio_18", "gpio_19", "gpio_20", "gpio_21", "gpio_22", "gpio_23", "gpio_44",
	"gpio_45", "gpio_64", "gpio_65", "gpio_66", "gpio_67", "gpio_76", "gpio_77",
	"gpio_8", "gpio_9", "hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_emmc_groups[] = {
	"emmc_clk", "emmc_cmd", "emmc_data_0", "emmc_data_1", "emmc_data_2",
	"emmc_data_3", "emmc_data_4", "emmc_data_5", "emmc_data_6", "emmc_data_7",
	"emmc_dd_sb", "emmc_rst_n"
};
static const char * const rtd1635_main2_iso_tristate_groups[] = {
	"emmc_clk", "emmc_cmd", "emmc_data_0", "emmc_data_1", "emmc_data_2",
	"emmc_data_3", "emmc_data_4", "emmc_data_5", "emmc_data_6", "emmc_data_7",
	"emmc_dd_sb", "emmc_rst_n", "gpio_10", "gpio_11", "gpio_16", "gpio_17",
	"gpio_18", "gpio_19", "gpio_20", "gpio_21", "gpio_22", "gpio_23", "gpio_44",
	"gpio_45", "gpio_64", "gpio_65", "gpio_66", "gpio_67", "gpio_8", "gpio_9",
	"hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_nf_groups[] = {
	"emmc_data_0", "emmc_data_1", "emmc_data_2", "emmc_data_3", "emmc_data_4",
	"emmc_data_5"
};
static const char * const rtd1635_main2_uart1_groups[] = {
	"gpio_10", "gpio_11", "gpio_8", "gpio_9"
};
static const char * const rtd1635_main2_scpu_ejtag_loc0_groups[] = {
	"gpio_16", "gpio_17", "gpio_18", "gpio_19", "gpio_8"
};
static const char * const rtd1635_main2_pcpu_ejtag_loc0_groups[] = {
	"gpio_16", "gpio_17", "gpio_18", "gpio_19", "gpio_8"
};
static const char * const rtd1635_main2_aucpu0_ejtag_loc0_groups[] = {
	"gpio_16", "gpio_17", "gpio_18", "gpio_19", "gpio_8"
};
static const char * const rtd1635_main2_ve2_vcpu_ejtag_loc0_groups[] = {
	"gpio_16", "gpio_17", "gpio_18", "gpio_19", "gpio_8"
};
static const char * const rtd1635_main2_acpu_ejtag_loc0_groups[] = {
	"gpio_16", "gpio_17", "gpio_18", "gpio_19", "gpio_8"
};
static const char * const rtd1635_main2_gpu_tmbist_ejtag_groups[] = {
	"gpio_16", "gpio_17", "gpio_18", "gpio_19", "gpio_8"
};
static const char * const rtd1635_main2_ve3_vcpu_ejtag_loc0_groups[] = {
	"gpio_16", "gpio_17", "gpio_18", "gpio_19", "gpio_8"
};
static const char * const rtd1635_main2_uart0_loc0_groups[] = {
	"gpio_10", "gpio_11"
};
static const char * const rtd1635_main2_i2c6_loc1_groups[] = {
	"gpio_16", "gpio_17"
};
static const char * const rtd1635_main2_pcm_groups[] = {
	"gpio_16", "gpio_17", "gpio_18", "gpio_19"
};
static const char * const rtd1635_main2_scpu_swd_loc0_groups[] = {
	"gpio_16", "gpio_19"
};
static const char * const rtd1635_main2_scd_scpu_loc0_groups[] = {
	"gpio_16", "gpio_19"
};
static const char * const rtd1635_main2_i2c1_groups[] = {
	"gpio_20", "gpio_21"
};
static const char * const rtd1635_main2_dptx_hpd_groups[] = {
	"gpio_22"
};
static const char * const rtd1635_main2_eio_irq4_groups[] = {
	"gpio_22"
};
static const char * const rtd1635_main2_eio_irq0_loc1_groups[] = {
	"gpio_23"
};
static const char * const rtd1635_main2_sd_groups[] = {
	"gpio_44", "gpio_45", "hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_scpu_ejtag_loc1_groups[] = {
	"gpio_44", "hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_pcpu_ejtag_loc1_groups[] = {
	"gpio_44", "hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_aucpu0_ejtag_loc1_groups[] = {
	"gpio_44", "hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_ve2_vcpu_ejtag_loc1_groups[] = {
	"gpio_44", "hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_acpu_ejtag_loc1_groups[] = {
	"gpio_44", "hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_hi_loc0_groups[] = {
	"hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_hi_m_groups[] = {
	"hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_scpu_swd_loc1_groups[] = {
	"hif_clk", "hif_data"
};
static const char * const rtd1635_main2_scd_scpu_loc1_groups[] = {
	"hif_clk", "hif_data"
};
static const char * const rtd1635_main2_ve3_vcpu_ejtag_loc1_groups[] = {
	"gpio_44", "hif_clk", "hif_data", "hif_en", "hif_rdy"
};
static const char * const rtd1635_main2_spi_groups[] = {
	"gpio_64", "gpio_65", "gpio_66", "gpio_67"
};
static const char * const rtd1635_main2_pwm0_loc1_groups[] = {
	"gpio_64"
};
static const char * const rtd1635_main2_pll_test1_loc0_groups[] = {
	"gpio_65"
};
static const char * const rtd1635_main2_pwm1_loc1_groups[] = {
	"gpio_65"
};
static const char * const rtd1635_main2_pll_test1_loc1_groups[] = {
	"gpio_66"
};
static const char * const rtd1635_main2_pwm2_loc1_groups[] = {
	"gpio_66"
};
static const char * const rtd1635_main2_pwm3_loc1_groups[] = {
	"gpio_67"
};
static const char * const rtd1635_main2_pwm2_loc0_groups[] = {
	"gpio_76"
};
static const char * const rtd1635_main2_eio_irq5_groups[] = {
	"gpio_76"
};
static const char * const rtd1635_main2_dbg_out1_groups[] = {
	"gpio_76", "gpio_77"
};
static const char * const rtd1635_main2_pwm3_loc0_groups[] = {
	"gpio_77"
};
static const char * const rtd1635_main2_eio_irq6_groups[] = {
	"gpio_77"
};
static const char * const rtd1635_npu_gpio_groups[] = {
	"gpio_104", "gpio_105", "gpio_12", "gpio_129", "gpio_13", "gpio_130",
	"gpio_131", "gpio_132", "gpio_133", "gpio_134", "gpio_14", "gpio_15",
	"gpio_38", "gpio_39", "gpio_4", "gpio_5", "gpio_6", "gpio_68", "gpio_69",
	"gpio_7", "gpio_70", "gpio_71", "gpio_72", "gpio_73", "gpio_74", "gpio_75"
};
static const char * const rtd1635_npu_uart2_groups[] = {
	"gpio_4", "gpio_5", "gpio_6", "gpio_7"
};
static const char * const rtd1635_npu_gspi0_groups[] = {
	"gpio_4", "gpio_5", "gpio_6", "gpio_7"
};
static const char * const rtd1635_npu_acpu_tmbist_ejtag_groups[] = {
	"gpio_129", "gpio_133", "gpio_4", "gpio_5", "gpio_6"
};
static const char * const rtd1635_npu_iso_tristate_groups[] = {
	"gpio_104", "gpio_105", "gpio_12", "gpio_13", "gpio_14", "gpio_15",
	"gpio_38", "gpio_39", "gpio_4", "gpio_5", "gpio_6", "gpio_7"
};
static const char * const rtd1635_npu_i2c0_groups[] = {
	"gpio_12", "gpio_13"
};
static const char * const rtd1635_npu_pwm0_loc0_groups[] = {
	"gpio_12"
};
static const char * const rtd1635_npu_test_loop_dis_pad_groups[] = {
	"gpio_13"
};
static const char * const rtd1635_npu_pwm1_loc0_groups[] = {
	"gpio_13"
};
static const char * const rtd1635_npu_sgmii_groups[] = {
	"gpio_14", "gpio_15"
};
static const char * const rtd1635_npu_uart0_loc1_groups[] = {
	"gpio_14", "gpio_15"
};
static const char * const rtd1635_npu_pcie0_groups[] = {
	"gpio_38"
};
static const char * const rtd1635_npu_eio_irq7_groups[] = {
	"gpio_39"
};
static const char * const rtd1635_npu_vtc_dmic_groups[] = {
	"gpio_68", "gpio_69", "gpio_70", "gpio_71"
};
static const char * const rtd1635_npu_dmic0_groups[] = {
	"gpio_68", "gpio_69"
};
static const char * const rtd1635_npu_ai_i2s0_loc1_groups[] = {
	"gpio_68", "gpio_69", "gpio_70", "gpio_71"
};
static const char * const rtd1635_npu_ao_i2s2_groups[] = {
	"gpio_68", "gpio_69", "gpio_70", "gpio_71"
};
static const char * const rtd1635_npu_dbg_out1_groups[] = {
	"gpio_68", "gpio_69", "gpio_70", "gpio_71", "gpio_72", "gpio_73", "gpio_74",
	"gpio_75"
};
static const char * const rtd1635_npu_dmic1_groups[] = {
	"gpio_70", "gpio_71"
};
static const char * const rtd1635_npu_uart9_loc0_groups[] = {
	"gpio_72", "gpio_73"
};
static const char * const rtd1635_npu_pwm5_loc1_groups[] = {
	"gpio_72"
};
static const char * const rtd1635_npu_dmic2_groups[] = {
	"gpio_72", "gpio_73"
};
static const char * const rtd1635_npu_ai_i2s1_loc1_groups[] = {
	"gpio_72", "gpio_73", "gpio_74", "gpio_75"
};
static const char * const rtd1635_npu_ao_i2s1_groups[] = {
	"gpio_72", "gpio_73", "gpio_74", "gpio_75"
};
static const char * const rtd1635_npu_pwm6_loc1_groups[] = {
	"gpio_73"
};
static const char * const rtd1635_npu_i2c6_loc0_groups[] = {
	"gpio_74", "gpio_75"
};
static const char * const rtd1635_npu_dmic3_groups[] = {
	"gpio_74", "gpio_75"
};
static const char * const rtd1635_npu_ai_i2s1_data1_loc1_groups[] = {
	"gpio_74"
};
static const char * const rtd1635_npu_uart6_groups[] = {
	"gpio_104", "gpio_105"
};
static const char * const rtd1635_npu_pwm5_loc3_groups[] = {
	"gpio_104"
};
static const char * const rtd1635_npu_pwm6_loc3_groups[] = {
	"gpio_105"
};
static const char * const rtd1635_npu_uart4_loc0_groups[] = {
	"gpio_129", "gpio_130"
};
static const char * const rtd1635_npu_i2c4_loc1_groups[] = {
	"gpio_129", "gpio_130"
};
static const char * const rtd1635_npu_dbg_out0_groups[] = {
	"gpio_129", "gpio_130", "gpio_131", "gpio_132", "gpio_133", "gpio_134"
};
static const char * const rtd1635_npu_ir_rx_loc2_groups[] = {
	"gpio_130"
};
static const char * const rtd1635_npu_i2c6_loc3_groups[] = {
	"gpio_131", "gpio_132"
};
static const char * const rtd1635_npu_uart9_loc2_groups[] = {
	"gpio_133", "gpio_134"
};
static const char * const rtd1635_ve2_gpio_groups[] = {
	"gpio_100", "gpio_101", "gpio_125", "gpio_126", "gpio_127", "gpio_128",
	"gpio_46", "gpio_47", "gpio_48", "gpio_96", "gpio_97", "gpio_98", "gpio_99"
};
static const char * const rtd1635_ve2_sd_groups[] = {
	"gpio_46", "gpio_47"
};
static const char * const rtd1635_ve2_eio_irq1_loc0_groups[] = {
	"gpio_46"
};
static const char * const rtd1635_ve2_iso_tristate_groups[] = {
	"gpio_100", "gpio_101", "gpio_46", "gpio_47", "gpio_48", "gpio_96",
	"gpio_97", "gpio_98", "gpio_99"
};
static const char * const rtd1635_ve2_eio_irq2_loc0_groups[] = {
	"gpio_47"
};
static const char * const rtd1635_ve2_eio_irq3_groups[] = {
	"gpio_48"
};
static const char * const rtd1635_ve2_uart3_groups[] = {
	"gpio_100", "gpio_101", "gpio_98", "gpio_99"
};
static const char * const rtd1635_ve2_gspi2_groups[] = {
	"gpio_100", "gpio_101", "gpio_98", "gpio_99"
};
static const char * const rtd1635_ve2_pwm3_loc2_groups[] = {
	"gpio_125"
};
static const char * const rtd1635_ve2_dbg_out0_groups[] = {
	"gpio_125", "gpio_126", "gpio_127", "gpio_128"
};
static const char * const rtd1635_ve2_pwm4_loc2_groups[] = {
	"gpio_126"
};
static const char * const rtd1635_ve2_pwm5_loc2_groups[] = {
	"gpio_127"
};
static const char * const rtd1635_ve2_pwm6_loc2_groups[] = {
	"gpio_128"
};
static const char * const rtd1635_ve3_gpio_groups[] = {
	"gpio_102", "gpio_103", "gpio_106", "gpio_107", "gpio_108", "gpio_109",
	"gpio_2", "gpio_24", "gpio_25", "gpio_26", "gpio_27", "gpio_3", "gpio_34",
	"gpio_35", "gpio_36", "gpio_37", "gpio_61", "gpio_94", "gpio_95"
};
static const char * const rtd1635_ve3_uart0_loc0_groups[] = {
	"gpio_2", "gpio_3"
};
static const char * const rtd1635_ve3_iso_tristate_groups[] = {
	"gpio_102", "gpio_103", "gpio_106", "gpio_107", "gpio_108", "gpio_109",
	"gpio_2", "gpio_24", "gpio_25", "gpio_26", "gpio_27", "gpio_3", "gpio_34",
	"gpio_35", "gpio_36", "gpio_37", "gpio_94", "gpio_95"
};
static const char * const rtd1635_ve3_uart9_loc1_groups[] = {
	"gpio_24", "gpio_25"
};
static const char * const rtd1635_ve3_ts0_groups[] = {
	"gpio_24", "gpio_25", "gpio_26", "gpio_27"
};
static const char * const rtd1635_ve3_i2c6_loc2_groups[] = {
	"gpio_26", "gpio_27"
};
static const char * const rtd1635_ve3_uart4_loc1_groups[] = {
	"gpio_34", "gpio_35"
};
static const char * const rtd1635_ve3_i2c4_loc0_groups[] = {
	"gpio_34", "gpio_35"
};
static const char * const rtd1635_ve3_i2c5_groups[] = {
	"gpio_36", "gpio_37"
};
static const char * const rtd1635_ve3_spdif_out_groups[] = {
	"gpio_61"
};
static const char * const rtd1635_ve3_dbg_out1_groups[] = {
	"gpio_61"
};
static const char * const rtd1635_ve3_i2c3_groups[] = {
	"gpio_94", "gpio_95"
};
static const char * const rtd1635_ve3_uart5_groups[] = {
	"gpio_102", "gpio_103"
};
static const char * const rtd1635_ve3_pwm3_loc3_groups[] = {
	"gpio_102"
};
static const char * const rtd1635_ve3_pwm4_loc3_groups[] = {
	"gpio_103"
};
static const char * const rtd1635_ve3_uart7_groups[] = {
	"gpio_106", "gpio_107"
};
static const char * const rtd1635_ve3_gspi1_groups[] = {
	"gpio_106", "gpio_107", "gpio_108", "gpio_109"
};
static const char * const rtd1635_ve3_uart8_loc0_groups[] = {
	"gpio_108", "gpio_109"
};
static const char * const rtd1635_ve3_pwm6_loc0_groups[] = {
	"gpio_108"
};
static const char * const rtd1635_ve3_pwm4_loc1_groups[] = {
	"gpio_109"
};
static const struct rtd_pin_func_desc rtd1635_iso_pin_functions[] = {
	RTD1635_FUNC(iso, gpio),
	RTD1635_FUNC(iso, uart8_loc1),
	RTD1635_FUNC(iso, gspi3),
	RTD1635_FUNC(iso, pwm4_loc0),
	RTD1635_FUNC(iso, ts1),
	RTD1635_FUNC(iso, iso_tristate),
	RTD1635_FUNC(iso, pwm5_loc0),
	RTD1635_FUNC(iso, pll_test0_loc0),
	RTD1635_FUNC(iso, pll_test0_loc1),
	RTD1635_FUNC(iso, vtci_i2s),
	RTD1635_FUNC(iso, ai_i2s0_loc0),
	RTD1635_FUNC(iso, ao_i2s0),
	RTD1635_FUNC(iso, ao_tdm0),
	RTD1635_FUNC(iso, dbg_out1),
	RTD1635_FUNC(iso, ai_saibb0),
	RTD1635_FUNC(iso, dbg_out0),
	RTD1635_FUNC(iso, ao_tdm1),
	RTD1635_FUNC(iso, ao_tdm1_loc1),
	RTD1635_FUNC(iso, vtco_i2s),
	RTD1635_FUNC(iso, ai_saibb1),
	RTD1635_FUNC(iso, ai_i2s1_loc0),
	RTD1635_FUNC(iso, ai_tdm0),
	RTD1635_FUNC(iso, vi_dtv),
	RTD1635_FUNC(iso, i2c8_loc0),
	RTD1635_FUNC(iso, usb_cc1),
	RTD1635_FUNC(iso, usb_cc2),
	RTD1635_FUNC(iso, uart4_loc2),
	RTD1635_FUNC(iso, i2c4_loc2),
	RTD1635_FUNC(iso, pwm0_loc2),
	RTD1635_FUNC(iso, eio_irq1_loc1),
	RTD1635_FUNC(iso, uart0_loc2),
	RTD1635_FUNC(iso, pwm1_loc2),
	RTD1635_FUNC(iso, eio_irq2_loc1),
	RTD1635_FUNC(iso, scd_scpu_loc0),
	RTD1635_FUNC(iso, scd_scpu_loc1),
	RTD1635_FUNC(iso, hi_width_disable),
	RTD1635_FUNC(iso, hi_width_1bit),
	RTD1635_FUNC(iso, scpu_swd_disable),
	RTD1635_FUNC(iso, scpu_swd_enable),
	RTD1635_FUNC(iso, sf_disable),
	RTD1635_FUNC(iso, sf_enable),
	RTD1635_FUNC(iso, aucpu0_ejtag_loc0),
	RTD1635_FUNC(iso, aucpu0_ejtag_loc1),
	RTD1635_FUNC(iso, ve3_vcpu_ejtag_loc0),
	RTD1635_FUNC(iso, ve3_vcpu_ejtag_loc1),
	RTD1635_FUNC(iso, ve2_vcpu_ejtag_loc0),
	RTD1635_FUNC(iso, ve2_vcpu_ejtag_loc1),
	RTD1635_FUNC(iso, scpu_ejtag_loc0),
	RTD1635_FUNC(iso, scpu_ejtag_loc1),
	RTD1635_FUNC(iso, pcpu_ejtag_loc0),
	RTD1635_FUNC(iso, pcpu_ejtag_loc1),
	RTD1635_FUNC(iso, acpu_ejtag_loc0),
	RTD1635_FUNC(iso, acpu_ejtag_loc1),
	RTD1635_FUNC(iso, i2c4_loc0),
	RTD1635_FUNC(iso, i2c4_loc1),
	RTD1635_FUNC(iso, i2c6_loc0),
	RTD1635_FUNC(iso, i2c6_loc1),
	RTD1635_FUNC(iso, i2c6_loc2),
	RTD1635_FUNC(iso, i2c6_loc3),
	RTD1635_FUNC(iso, i2c8_loc1),
	RTD1635_FUNC(iso, uart0_loc0),
	RTD1635_FUNC(iso, uart0_loc1),
	RTD1635_FUNC(iso, uart4_loc0),
	RTD1635_FUNC(iso, uart4_loc1),
	RTD1635_FUNC(iso, uart8_loc0),
	RTD1635_FUNC(iso, uart9_loc0),
	RTD1635_FUNC(iso, uart9_loc1),
	RTD1635_FUNC(iso, uart9_loc2),
	RTD1635_FUNC(iso, eio_irq0_loc0),
	RTD1635_FUNC(iso, eio_irq0_loc1),
	RTD1635_FUNC(iso, eio_irq1_loc0),
	RTD1635_FUNC(iso, eio_irq2_loc0),
	RTD1635_FUNC(iso, ai_i2s0_loc1),
	RTD1635_FUNC(iso, ai_i2s1_loc1),
	RTD1635_FUNC(iso, hif_disable),
	RTD1635_FUNC(iso, hif_enable),
};
static const struct rtd_pin_func_desc rtd1635_isom_pin_functions[] = {
	RTD1635_FUNC(isom, gpio),
	RTD1635_FUNC(isom, pctrl),
	RTD1635_FUNC(isom, eio_irq0_loc0),
	RTD1635_FUNC(isom, iso_tristate),
	RTD1635_FUNC(isom, ir_rx_loc0),
	RTD1635_FUNC(isom, uart10),
	RTD1635_FUNC(isom, isom_dbg_out0),
	RTD1635_FUNC(isom, i2c8_loc1),
	RTD1635_FUNC(isom, ir_rx_loc1),
	RTD1635_FUNC(isom, ir_rx_loc2),
};
static const struct rtd_pin_func_desc rtd1635_main2_pin_functions[] = {
	RTD1635_FUNC(main2, gpio),
	RTD1635_FUNC(main2, emmc),
	RTD1635_FUNC(main2, iso_tristate),
	RTD1635_FUNC(main2, nf),
	RTD1635_FUNC(main2, uart1),
	RTD1635_FUNC(main2, scpu_ejtag_loc0),
	RTD1635_FUNC(main2, pcpu_ejtag_loc0),
	RTD1635_FUNC(main2, aucpu0_ejtag_loc0),
	RTD1635_FUNC(main2, ve2_vcpu_ejtag_loc0),
	RTD1635_FUNC(main2, acpu_ejtag_loc0),
	RTD1635_FUNC(main2, gpu_tmbist_ejtag),
	RTD1635_FUNC(main2, ve3_vcpu_ejtag_loc0),
	RTD1635_FUNC(main2, uart0_loc0),
	RTD1635_FUNC(main2, i2c6_loc1),
	RTD1635_FUNC(main2, pcm),
	RTD1635_FUNC(main2, scpu_swd_loc0),
	RTD1635_FUNC(main2, scd_scpu_loc0),
	RTD1635_FUNC(main2, i2c1),
	RTD1635_FUNC(main2, dptx_hpd),
	RTD1635_FUNC(main2, eio_irq4),
	RTD1635_FUNC(main2, eio_irq0_loc1),
	RTD1635_FUNC(main2, sd),
	RTD1635_FUNC(main2, scpu_ejtag_loc1),
	RTD1635_FUNC(main2, pcpu_ejtag_loc1),
	RTD1635_FUNC(main2, aucpu0_ejtag_loc1),
	RTD1635_FUNC(main2, ve2_vcpu_ejtag_loc1),
	RTD1635_FUNC(main2, acpu_ejtag_loc1),
	RTD1635_FUNC(main2, hi_loc0),
	RTD1635_FUNC(main2, hi_m),
	RTD1635_FUNC(main2, scpu_swd_loc1),
	RTD1635_FUNC(main2, scd_scpu_loc1),
	RTD1635_FUNC(main2, ve3_vcpu_ejtag_loc1),
	RTD1635_FUNC(main2, spi),
	RTD1635_FUNC(main2, pwm0_loc1),
	RTD1635_FUNC(main2, pll_test1_loc0),
	RTD1635_FUNC(main2, pwm1_loc1),
	RTD1635_FUNC(main2, pll_test1_loc1),
	RTD1635_FUNC(main2, pwm2_loc1),
	RTD1635_FUNC(main2, pwm3_loc1),
	RTD1635_FUNC(main2, pwm2_loc0),
	RTD1635_FUNC(main2, eio_irq5),
	RTD1635_FUNC(main2, dbg_out1),
	RTD1635_FUNC(main2, pwm3_loc0),
	RTD1635_FUNC(main2, eio_irq6),
};
static const struct rtd_pin_func_desc rtd1635_npu_pin_functions[] = {
	RTD1635_FUNC(npu, gpio),
	RTD1635_FUNC(npu, uart2),
	RTD1635_FUNC(npu, gspi0),
	RTD1635_FUNC(npu, acpu_tmbist_ejtag),
	RTD1635_FUNC(npu, iso_tristate),
	RTD1635_FUNC(npu, i2c0),
	RTD1635_FUNC(npu, pwm0_loc0),
	RTD1635_FUNC(npu, test_loop_dis_pad),
	RTD1635_FUNC(npu, pwm1_loc0),
	RTD1635_FUNC(npu, sgmii),
	RTD1635_FUNC(npu, uart0_loc1),
	RTD1635_FUNC(npu, pcie0),
	RTD1635_FUNC(npu, eio_irq7),
	RTD1635_FUNC(npu, vtc_dmic),
	RTD1635_FUNC(npu, dmic0),
	RTD1635_FUNC(npu, ai_i2s0_loc1),
	RTD1635_FUNC(npu, ao_i2s2),
	RTD1635_FUNC(npu, dbg_out1),
	RTD1635_FUNC(npu, dmic1),
	RTD1635_FUNC(npu, uart9_loc0),
	RTD1635_FUNC(npu, pwm5_loc1),
	RTD1635_FUNC(npu, dmic2),
	RTD1635_FUNC(npu, ai_i2s1_loc1),
	RTD1635_FUNC(npu, ao_i2s1),
	RTD1635_FUNC(npu, pwm6_loc1),
	RTD1635_FUNC(npu, i2c6_loc0),
	RTD1635_FUNC(npu, dmic3),
	RTD1635_FUNC(npu, ai_i2s1_data1_loc1),
	RTD1635_FUNC(npu, uart6),
	RTD1635_FUNC(npu, pwm5_loc3),
	RTD1635_FUNC(npu, pwm6_loc3),
	RTD1635_FUNC(npu, uart4_loc0),
	RTD1635_FUNC(npu, i2c4_loc1),
	RTD1635_FUNC(npu, dbg_out0),
	RTD1635_FUNC(npu, ir_rx_loc2),
	RTD1635_FUNC(npu, i2c6_loc3),
	RTD1635_FUNC(npu, uart9_loc2),
};
static const struct rtd_pin_func_desc rtd1635_ve2_pin_functions[] = {
	RTD1635_FUNC(ve2, gpio),
	RTD1635_FUNC(ve2, sd),
	RTD1635_FUNC(ve2, eio_irq1_loc0),
	RTD1635_FUNC(ve2, iso_tristate),
	RTD1635_FUNC(ve2, eio_irq2_loc0),
	RTD1635_FUNC(ve2, eio_irq3),
	RTD1635_FUNC(ve2, uart3),
	RTD1635_FUNC(ve2, gspi2),
	RTD1635_FUNC(ve2, pwm3_loc2),
	RTD1635_FUNC(ve2, dbg_out0),
	RTD1635_FUNC(ve2, pwm4_loc2),
	RTD1635_FUNC(ve2, pwm5_loc2),
	RTD1635_FUNC(ve2, pwm6_loc2),
};
static const struct rtd_pin_func_desc rtd1635_ve3_pin_functions[] = {
	RTD1635_FUNC(ve3, gpio),
	RTD1635_FUNC(ve3, uart0_loc0),
	RTD1635_FUNC(ve3, iso_tristate),
	RTD1635_FUNC(ve3, uart9_loc1),
	RTD1635_FUNC(ve3, ts0),
	RTD1635_FUNC(ve3, i2c6_loc2),
	RTD1635_FUNC(ve3, uart4_loc1),
	RTD1635_FUNC(ve3, i2c4_loc0),
	RTD1635_FUNC(ve3, i2c5),
	RTD1635_FUNC(ve3, spdif_out),
	RTD1635_FUNC(ve3, dbg_out1),
	RTD1635_FUNC(ve3, i2c3),
	RTD1635_FUNC(ve3, uart5),
	RTD1635_FUNC(ve3, pwm3_loc3),
	RTD1635_FUNC(ve3, pwm4_loc3),
	RTD1635_FUNC(ve3, uart7),
	RTD1635_FUNC(ve3, gspi1),
	RTD1635_FUNC(ve3, uart8_loc0),
	RTD1635_FUNC(ve3, pwm6_loc0),
	RTD1635_FUNC(ve3, pwm4_loc1),
};

static const struct rtd_pin_desc rtd1635_iso_muxes[] = {
	[RTD1635_ISO_GPIO_30] = RTK_PIN_MUX(gpio_30, 0x0, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "uart8_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "gspi3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 0), "pwm4_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 0), "ts1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_31] = RTK_PIN_MUX(gpio_31, 0x0, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "uart8_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "gspi3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "pwm5_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 4), "ts1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_32] = RTK_PIN_MUX(gpio_32, 0x0, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "uart8_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 8), "gspi3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 8), "ts1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_33] = RTK_PIN_MUX(gpio_33, 0x0, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart8_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "gspi3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 12), "ts1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_62] = RTK_PIN_MUX(gpio_62, 0x0, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "pll_test0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_63] = RTK_PIN_MUX(gpio_63, 0x0, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "pll_test0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_78] = RTK_PIN_MUX(gpio_78, 0x0, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 24), "vtci_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 24), "ai_i2s0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 24), "ao_i2s0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 24), "ao_tdm0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "dbg_out1")
	),
	[RTD1635_ISO_GPIO_79] = RTK_PIN_MUX(gpio_79, 0x0, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 28), "vtci_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 28), "ai_i2s0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 28), "ao_i2s0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 28), "ao_tdm0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "dbg_out1")
	),
	[RTD1635_ISO_GPIO_80] = RTK_PIN_MUX(gpio_80, 0x4, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 0), "vtci_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 0), "ai_i2s0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 0), "ao_i2s0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 0), "ao_tdm0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "dbg_out1")
	),
	[RTD1635_ISO_GPIO_81] = RTK_PIN_MUX(gpio_81, 0x4, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 4), "vtci_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 4), "ai_saibb0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 4), "ai_i2s0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "dbg_out1")
	),
	[RTD1635_ISO_GPIO_82] = RTK_PIN_MUX(gpio_82, 0x4, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 8), "vtci_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 8), "ai_saibb0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 8), "ai_i2s0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "dbg_out1")
	),
	[RTD1635_ISO_GPIO_83] = RTK_PIN_MUX(gpio_83, 0x4, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 12), "ai_saibb0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 12), "ai_i2s0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_84] = RTK_PIN_MUX(gpio_84, 0x4, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 16), "ai_saibb0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 16), "ai_i2s0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_85] = RTK_PIN_MUX(gpio_85, 0x4, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 20), "ao_i2s0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 20), "ao_tdm0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "dbg_out0")
	),
	[RTD1635_ISO_GPIO_86] = RTK_PIN_MUX(gpio_86, 0x4, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 24), "ao_i2s0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 24), "ao_tdm1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "dbg_out0")
	),
	[RTD1635_ISO_GPIO_87] = RTK_PIN_MUX(gpio_87, 0x4, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 28), "ao_i2s0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 28), "ao_tdm1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "dbg_out0")
	),
	[RTD1635_ISO_GPIO_88] = RTK_PIN_MUX(gpio_88, 0x8, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 0), "ao_i2s0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 0), "ao_tdm1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 0), "ao_tdm1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "dbg_out0")
	),
	[RTD1635_ISO_GPIO_89] = RTK_PIN_MUX(gpio_89, 0x8, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 4), "vtco_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 4), "ai_saibb1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 4), "ai_i2s1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x9, 4), "ai_tdm0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_90] = RTK_PIN_MUX(gpio_90, 0x8, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 8), "vtco_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 8), "ai_saibb1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 8), "ai_i2s1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x9, 8), "ai_tdm0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_91] = RTK_PIN_MUX(gpio_91, 0x8, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 12), "vtco_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 12), "ai_saibb1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 12), "ai_i2s1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x9, 12), "ai_tdm0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_92] = RTK_PIN_MUX(gpio_92, 0x8, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 16), "vtco_i2s"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 16), "ai_i2s1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x9, 16), "ai_tdm0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_93] = RTK_PIN_MUX(gpio_93, 0x8, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 20), "ai_saibb1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 20), "ai_i2s1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 20), "ao_tdm1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_110] = RTK_PIN_MUX(gpio_110, 0x8, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_111] = RTK_PIN_MUX(gpio_111, 0x8, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_112] = RTK_PIN_MUX(gpio_112, 0xc, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_113] = RTK_PIN_MUX(gpio_113, 0xc, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_114] = RTK_PIN_MUX(gpio_114, 0xc, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_115] = RTK_PIN_MUX(gpio_115, 0xc, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_116] = RTK_PIN_MUX(gpio_116, 0xc, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_117] = RTK_PIN_MUX(gpio_117, 0xc, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_118] = RTK_PIN_MUX(gpio_118, 0xc, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "vi_dtv"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_119] = RTK_PIN_MUX(gpio_119, 0xc, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 28), "i2c8_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "dbg_out0")
	),
	[RTD1635_ISO_GPIO_120] = RTK_PIN_MUX(gpio_120, 0x10, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 0), "i2c8_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "dbg_out0")
	),
	[RTD1635_ISO_USB_CC1] = RTK_PIN_MUX(usb_cc1, 0x10, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "usb_cc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_ISO_USB_CC2] = RTK_PIN_MUX(usb_cc2, 0x10, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "usb_cc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_123] = RTK_PIN_MUX(gpio_123, 0x10, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart4_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "i2c4_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 12), "pwm0_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 12), "eio_irq1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 12), "uart0_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_ISO_GPIO_124] = RTK_PIN_MUX(gpio_124, 0x10, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "uart4_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "i2c4_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "pwm1_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 16), "eio_irq2_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 16), "uart0_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_ISO_SCD_LOC] = RTK_PIN_MUX(scd_loc, 0x120, GENMASK(2, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "scd_scpu_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "scd_scpu_loc1")
	),
	[RTD1635_ISO_HI_WIDTH] = RTK_PIN_MUX(hi_width, 0x120, GENMASK(9, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "hi_width_disable"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "hi_width_1bit")
	),
	[RTD1635_ISO_EJTAG_SCPU_SWD_MODE_EN] = RTK_PIN_MUX(ejtag_scpu_swd_mode_en, 0x120, GENMASK(10, 10),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 10), "scpu_swd_disable"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 10), "scpu_swd_enable")
	),
	[RTD1635_ISO_SF_EN] = RTK_PIN_MUX(sf_en, 0x120, GENMASK(11, 11),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 11), "sf_disable"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 11), "sf_enable")
	),
	[RTD1635_ISO_ARM_TRACE_DBG_EN] = RTK_PIN_MUX(arm_trace_dbg_en, 0x120, GENMASK(12, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "scpu_swd_disable"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "scpu_swd_enable")
	),
	[RTD1635_ISO_EJTAG_AUCPU0_LOC] = RTK_PIN_MUX(ejtag_aucpu0_loc, 0x120, GENMASK(16, 14),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 14), "aucpu0_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 14), "aucpu0_ejtag_loc1")
	),
	[RTD1635_ISO_EJTAG_VE3_LOC] = RTK_PIN_MUX(ejtag_ve3_loc, 0x120, GENMASK(19, 17),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 17), "ve3_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 17), "ve3_vcpu_ejtag_loc1")
	),
	[RTD1635_ISO_EJTAG_VE2_LOC] = RTK_PIN_MUX(ejtag_ve2_loc, 0x120, GENMASK(22, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "ve2_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "ve2_vcpu_ejtag_loc1")
	),
	[RTD1635_ISO_EJTAG_SCPU_LOC] = RTK_PIN_MUX(ejtag_scpu_loc, 0x120, GENMASK(25, 23),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 23), "scpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 23), "scpu_ejtag_loc1")
	),
	[RTD1635_ISO_EJTAG_PCPU_LOC] = RTK_PIN_MUX(ejtag_pcpu_loc, 0x120, GENMASK(28, 26),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 26), "pcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 26), "pcpu_ejtag_loc1")
	),
	[RTD1635_ISO_EJTAG_ACPU_LOC] = RTK_PIN_MUX(ejtag_acpu_loc, 0x120, GENMASK(31, 29),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 29), "acpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 29), "acpu_ejtag_loc1")
	),
	[RTD1635_ISO_I2C4_LOC] = RTK_PIN_MUX(i2c4_loc, 0x128, GENMASK(2, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "i2c4_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "i2c4_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 0), "i2c4_loc2")
	),
	[RTD1635_ISO_I2C6_LOC] = RTK_PIN_MUX(i2c6_loc, 0x128, GENMASK(6, 3),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 3), "i2c6_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 3), "i2c6_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 3), "i2c6_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 3), "i2c6_loc3")
	),
	[RTD1635_ISO_I2C8_LOC] = RTK_PIN_MUX(i2c8_loc, 0x128, GENMASK(8, 7),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 7), "i2c8_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 7), "i2c8_loc1")
	),
	[RTD1635_ISO_UART0_LOC] = RTK_PIN_MUX(uart0_loc, 0x128, GENMASK(11, 9),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 9), "uart0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 9), "uart0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 9), "uart0_loc2")
	),
	[RTD1635_ISO_UART4_LOC] = RTK_PIN_MUX(uart4_loc, 0x128, GENMASK(14, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart4_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "uart4_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 12), "uart4_loc2")
	),
	[RTD1635_ISO_UART8_LOC] = RTK_PIN_MUX(uart8_loc, 0x128, GENMASK(16, 15),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 15), "uart8_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 15), "uart8_loc1")
	),
	[RTD1635_ISO_UART9_LOC] = RTK_PIN_MUX(uart9_loc, 0x128, GENMASK(19, 17),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart9_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "uart9_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 12), "uart9_loc2")
	),
	[RTD1635_ISO_EIO_IRQ0_LOC] = RTK_PIN_MUX(eio_irq0_loc, 0x128, GENMASK(21, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "eio_irq0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "eio_irq0_loc1")
	),
	[RTD1635_ISO_EIO_IRQ1_LOC] = RTK_PIN_MUX(eio_irq1_loc, 0x128, GENMASK(23, 22),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 22), "eio_irq1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 22), "eio_irq1_loc1")
	),
	[RTD1635_ISO_EIO_IRQ2_LOC] = RTK_PIN_MUX(eio_irq2_loc, 0x128, GENMASK(25, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "eio_irq2_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "eio_irq2_loc1")
	),
	[RTD1635_ISO_AI_I2S0_LOC] = RTK_PIN_MUX(ai_i2s0_loc, 0x128, GENMASK(27, 26),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 26), "ai_i2s0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 26), "ai_i2s0_loc1")
	),
	[RTD1635_ISO_AI_I2S1_LOC] = RTK_PIN_MUX(ai_i2s1_loc, 0x128, GENMASK(29, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "ai_i2s1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "ai_i2s1_loc1")
	),
	[RTD1635_ISO_HI_ENABLE] = RTK_PIN_MUX(hi_enable, 0x12c, GENMASK(2, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "hif_disable"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 0), "hif_enable")
	),
};

static const struct rtd_pin_desc rtd1635_isom_muxes[] = {
	[RTD1635_ISOM_GPIO_0] = RTK_PIN_MUX(gpio_0, 0x0, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "pctrl"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 0), "eio_irq0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_ISOM_GPIO_1] = RTK_PIN_MUX(gpio_1, 0x0, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "pctrl"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "ir_rx_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_ISOM_GPIO_28] = RTK_PIN_MUX(gpio_28, 0x0, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "uart10"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 8), "pctrl"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 8), "isom_dbg_out0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 8), "i2c8_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_ISOM_GPIO_29] = RTK_PIN_MUX(gpio_29, 0x0, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart10"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "pctrl"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 12), "ir_rx_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 12), "isom_dbg_out0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 12), "i2c8_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_ISOM_IR_RX_LOC] = RTK_PIN_MUX(ir_rx_loc, 0x30, GENMASK(2, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "ir_rx_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "ir_rx_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 0), "ir_rx_loc2")
	),
};

static const struct rtd_pin_desc rtd1635_main2_muxes[] = {
	[RTD1635_MAIN2_EMMC_RST_N] = RTK_PIN_MUX(emmc_rst_n, 0x0, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DD_SB] = RTK_PIN_MUX(emmc_dd_sb, 0x0, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_CLK] = RTK_PIN_MUX(emmc_clk, 0x0, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 8), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_CMD] = RTK_PIN_MUX(emmc_cmd, 0x0, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DATA_0] = RTK_PIN_MUX(emmc_data_0, 0x0, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "nf"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DATA_1] = RTK_PIN_MUX(emmc_data_1, 0x0, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 20), "nf"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DATA_2] = RTK_PIN_MUX(emmc_data_2, 0x0, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 24), "nf"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DATA_3] = RTK_PIN_MUX(emmc_data_3, 0x0, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 28), "nf"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DATA_4] = RTK_PIN_MUX(emmc_data_4, 0x4, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 0), "nf"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DATA_5] = RTK_PIN_MUX(emmc_data_5, 0x4, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "nf"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DATA_6] = RTK_PIN_MUX(emmc_data_6, 0x4, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 8), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_MAIN2_EMMC_DATA_7] = RTK_PIN_MUX(emmc_data_7, 0x4, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "emmc"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_8] = RTK_PIN_MUX(gpio_8, 0x4, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "uart1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "scpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 16), "pcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 16), "aucpu0_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 16), "ve2_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 16), "acpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 16), "gpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 16), "ve3_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_9] = RTK_PIN_MUX(gpio_9, 0x4, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "uart1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_10] = RTK_PIN_MUX(gpio_10, 0x4, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "uart1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 24), "uart0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_11] = RTK_PIN_MUX(gpio_11, 0x4, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "uart1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 28), "uart0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_16] = RTK_PIN_MUX(gpio_16, 0x8, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "i2c6_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 0), "scpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 0), "pcm"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 0), "pcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 0), "aucpu0_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 0), "ve2_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 0), "acpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 0), "scpu_swd_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 0), "gpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xd, 0), "scd_scpu_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 0), "ve3_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_17] = RTK_PIN_MUX(gpio_17, 0x8, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "i2c6_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "scpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 4), "pcm"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 4), "pcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 4), "aucpu0_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 4), "ve2_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 4), "acpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 4), "gpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 4), "ve3_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_18] = RTK_PIN_MUX(gpio_18, 0x8, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "scpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 8), "pcm"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 8), "pcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 8), "aucpu0_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 8), "ve2_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 8), "acpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 8), "gpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 8), "ve3_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_19] = RTK_PIN_MUX(gpio_19, 0x8, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 12), "scpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 12), "pcm"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 12), "pcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 12), "aucpu0_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 12), "ve2_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 12), "acpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 12), "scpu_swd_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 12), "gpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xd, 12), "scd_scpu_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 12), "ve3_vcpu_ejtag_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_20] = RTK_PIN_MUX(gpio_20, 0x8, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "i2c1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_21] = RTK_PIN_MUX(gpio_21, 0x8, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "i2c1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_22] = RTK_PIN_MUX(gpio_22, 0x8, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "dptx_hpd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 24), "eio_irq4"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_23] = RTK_PIN_MUX(gpio_23, 0x8, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 28), "eio_irq0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_MAIN2_HIF_DATA] = RTK_PIN_MUX(hif_data, 0xc, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "sd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 0), "scpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 0), "pcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 0), "aucpu0_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 0), "ve2_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 0), "acpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x9, 0), "hi_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 0), "hi_m"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 0), "scpu_swd_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xd, 0), "scd_scpu_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 0), "ve3_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_MAIN2_HIF_EN] = RTK_PIN_MUX(hif_en, 0xc, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "sd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "scpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 4), "pcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 4), "aucpu0_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 4), "ve2_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 4), "acpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x9, 4), "hi_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 4), "hi_m"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 4), "ve3_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_MAIN2_HIF_RDY] = RTK_PIN_MUX(hif_rdy, 0xc, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "sd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "scpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 8), "pcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 8), "aucpu0_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 8), "ve2_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 8), "acpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x9, 8), "hi_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 8), "hi_m"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 8), "ve3_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_MAIN2_HIF_CLK] = RTK_PIN_MUX(hif_clk, 0xc, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "sd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 12), "scpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 12), "pcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 12), "aucpu0_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 12), "ve2_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 12), "acpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x9, 12), "hi_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 12), "hi_m"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xb, 12), "scpu_swd_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xd, 12), "scd_scpu_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 12), "ve3_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_44] = RTK_PIN_MUX(gpio_44, 0xc, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "sd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "scpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 16), "pcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 16), "aucpu0_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 16), "ve2_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 16), "acpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xe, 16), "ve3_vcpu_ejtag_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_45] = RTK_PIN_MUX(gpio_45, 0xc, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "sd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_64] = RTK_PIN_MUX(gpio_64, 0xc, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "spi"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 24), "pwm0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_65] = RTK_PIN_MUX(gpio_65, 0xc, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "pll_test1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "spi"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 28), "pwm1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_66] = RTK_PIN_MUX(gpio_66, 0x10, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "pll_test1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "spi"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 0), "pwm2_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_67] = RTK_PIN_MUX(gpio_67, 0x10, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "spi"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "pwm3_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_MAIN2_GPIO_76] = RTK_PIN_MUX(gpio_76, 0x10, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "pwm2_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 8), "eio_irq5"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "dbg_out1")
	),
	[RTD1635_MAIN2_GPIO_77] = RTK_PIN_MUX(gpio_77, 0x10, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 12), "pwm3_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 12), "eio_irq6"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "dbg_out1")
	),
};

static const struct rtd_pin_desc rtd1635_npu_muxes[] = {
	[RTD1635_NPU_GPIO_4] = RTK_PIN_MUX(gpio_4, 0x0, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "uart2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "gspi0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 0), "acpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_5] = RTK_PIN_MUX(gpio_5, 0x0, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "uart2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "gspi0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 4), "acpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_6] = RTK_PIN_MUX(gpio_6, 0x0, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "uart2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 8), "gspi0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 8), "acpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_7] = RTK_PIN_MUX(gpio_7, 0x0, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "gspi0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_12] = RTK_PIN_MUX(gpio_12, 0x0, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "i2c0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "pwm0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_13] = RTK_PIN_MUX(gpio_13, 0x0, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "test_loop_dis_pad"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "i2c0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 20), "pwm1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_14] = RTK_PIN_MUX(gpio_14, 0x0, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "sgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 24), "uart0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_15] = RTK_PIN_MUX(gpio_15, 0x0, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "sgmii"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 28), "uart0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_38] = RTK_PIN_MUX(gpio_38, 0x4, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "pcie0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_39] = RTK_PIN_MUX(gpio_39, 0x4, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 4), "eio_irq7"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_68] = RTK_PIN_MUX(gpio_68, 0x4, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 8), "vtc_dmic"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 8), "dmic0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 8), "ai_i2s0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 8), "ao_i2s2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "dbg_out1")
	),
	[RTD1635_NPU_GPIO_69] = RTK_PIN_MUX(gpio_69, 0x4, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 12), "vtc_dmic"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 12), "dmic0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 12), "ai_i2s0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 12), "ao_i2s2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "dbg_out1")
	),
	[RTD1635_NPU_GPIO_70] = RTK_PIN_MUX(gpio_70, 0x4, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 16), "vtc_dmic"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 16), "dmic1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 16), "ai_i2s0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 16), "ao_i2s2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "dbg_out1")
	),
	[RTD1635_NPU_GPIO_71] = RTK_PIN_MUX(gpio_71, 0x4, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 20), "vtc_dmic"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 20), "dmic1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 20), "ai_i2s0_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 20), "ao_i2s2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "dbg_out1")
	),
	[RTD1635_NPU_GPIO_72] = RTK_PIN_MUX(gpio_72, 0x4, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "uart9_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 24), "pwm5_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 24), "dmic2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 24), "ai_i2s1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 24), "ao_i2s1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "dbg_out1")
	),
	[RTD1635_NPU_GPIO_73] = RTK_PIN_MUX(gpio_73, 0x4, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "uart9_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 28), "pwm6_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 28), "dmic2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 28), "ai_i2s1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 28), "ao_i2s1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "dbg_out1")
	),
	[RTD1635_NPU_GPIO_74] = RTK_PIN_MUX(gpio_74, 0x8, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "i2c6_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 0), "dmic3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x7, 0), "ai_i2s1_data1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 0), "ai_i2s1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 0), "ao_i2s1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "dbg_out1")
	),
	[RTD1635_NPU_GPIO_75] = RTK_PIN_MUX(gpio_75, 0x8, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "i2c6_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 4), "dmic3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x8, 4), "ai_i2s1_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xa, 4), "ao_i2s1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "dbg_out1")
	),
	[RTD1635_NPU_GPIO_104] = RTK_PIN_MUX(gpio_104, 0x8, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "uart6"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "pwm5_loc3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_105] = RTK_PIN_MUX(gpio_105, 0x8, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart6"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 12), "pwm6_loc3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_NPU_GPIO_129] = RTK_PIN_MUX(gpio_129, 0x8, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 16), "uart4_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "i2c4_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 16), "acpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "dbg_out0")
	),
	[RTD1635_NPU_GPIO_130] = RTK_PIN_MUX(gpio_130, 0x8, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "uart4_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "i2c4_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 20), "ir_rx_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "dbg_out0")
	),
	[RTD1635_NPU_GPIO_131] = RTK_PIN_MUX(gpio_131, 0x8, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "i2c6_loc3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "dbg_out0")
	),
	[RTD1635_NPU_GPIO_132] = RTK_PIN_MUX(gpio_132, 0x8, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "i2c6_loc3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "dbg_out0")
	),
	[RTD1635_NPU_GPIO_133] = RTK_PIN_MUX(gpio_133, 0x8, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "uart9_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xc, 0), "acpu_tmbist_ejtag"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "dbg_out0")
	),
	[RTD1635_NPU_GPIO_134] = RTK_PIN_MUX(gpio_134, 0x8, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "uart9_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "dbg_out0")
	),
};

static const struct rtd_pin_desc rtd1635_ve2_muxes[] = {
	[RTD1635_VE2_GPIO_46] = RTK_PIN_MUX(gpio_46, 0x0, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "sd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 0), "eio_irq1_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_47] = RTK_PIN_MUX(gpio_47, 0x0, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "sd"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 4), "eio_irq2_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_48] = RTK_PIN_MUX(gpio_48, 0x0, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 8), "eio_irq3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_96] = RTK_PIN_MUX(gpio_96, 0x0, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_97] = RTK_PIN_MUX(gpio_97, 0x0, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_98] = RTK_PIN_MUX(gpio_98, 0x0, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "uart3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "gspi2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_99] = RTK_PIN_MUX(gpio_99, 0x0, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "uart3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "gspi2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_100] = RTK_PIN_MUX(gpio_100, 0x0, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "uart3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "gspi2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_101] = RTK_PIN_MUX(gpio_101, 0x4, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "uart3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "gspi2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_VE2_GPIO_125] = RTK_PIN_MUX(gpio_125, 0x4, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "pwm3_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "dbg_out0")
	),
	[RTD1635_VE2_GPIO_126] = RTK_PIN_MUX(gpio_126, 0x4, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "pwm4_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "dbg_out0")
	),
	[RTD1635_VE2_GPIO_127] = RTK_PIN_MUX(gpio_127, 0x4, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "pwm5_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "dbg_out0")
	),
	[RTD1635_VE2_GPIO_128] = RTK_PIN_MUX(gpio_128, 0x4, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 16), "pwm6_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "dbg_out0")
	),
};

static const struct rtd_pin_desc rtd1635_ve3_muxes[] = {
	[RTD1635_VE3_GPIO_2] = RTK_PIN_MUX(gpio_2, 0x0, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 0), "uart0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_3] = RTK_PIN_MUX(gpio_3, 0x0, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x5, 4), "uart0_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_24] = RTK_PIN_MUX(gpio_24, 0x0, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "uart9_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 8), "ts0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_25] = RTK_PIN_MUX(gpio_25, 0x0, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 12), "uart9_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 12), "ts0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_26] = RTK_PIN_MUX(gpio_26, 0x0, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "i2c6_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 16), "ts0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_27] = RTK_PIN_MUX(gpio_27, 0x0, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 20), "i2c6_loc2"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x6, 20), "ts0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_34] = RTK_PIN_MUX(gpio_34, 0x0, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "uart4_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 24), "i2c4_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_35] = RTK_PIN_MUX(gpio_35, 0x0, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "uart4_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "i2c4_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_36] = RTK_PIN_MUX(gpio_36, 0x4, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "i2c5"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_37] = RTK_PIN_MUX(gpio_37, 0x4, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "i2c5"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_61] = RTK_PIN_MUX(gpio_61, 0x4, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x4, 8), "spdif_out"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "dbg_out1")
	),
	[RTD1635_VE3_GPIO_94] = RTK_PIN_MUX(gpio_94, 0x4, GENMASK(15, 12),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 12), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 12), "i2c3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 12), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_95] = RTK_PIN_MUX(gpio_95, 0x4, GENMASK(19, 16),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 16), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 16), "i2c3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 16), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_102] = RTK_PIN_MUX(gpio_102, 0x4, GENMASK(23, 20),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 20), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 20), "uart5"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 20), "pwm3_loc3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 20), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_103] = RTK_PIN_MUX(gpio_103, 0x4, GENMASK(27, 24),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 24), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 24), "uart5"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 24), "pwm4_loc3"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 24), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_106] = RTK_PIN_MUX(gpio_106, 0x4, GENMASK(31, 28),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 28), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 28), "uart7"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 28), "gspi1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 28), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_107] = RTK_PIN_MUX(gpio_107, 0x8, GENMASK(3, 0),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 0), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 0), "uart7"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 0), "gspi1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 0), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_108] = RTK_PIN_MUX(gpio_108, 0x8, GENMASK(7, 4),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 4), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 4), "uart8_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 4), "gspi1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 4), "pwm6_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 4), "iso_tristate")
	),
	[RTD1635_VE3_GPIO_109] = RTK_PIN_MUX(gpio_109, 0x8, GENMASK(11, 8),
		RTK_PIN_FUNC(SHIFT_LEFT(0x0, 8), "gpio"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x1, 8), "uart8_loc0"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x2, 8), "gspi1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0x3, 8), "pwm4_loc1"),
		RTK_PIN_FUNC(SHIFT_LEFT(0xf, 8), "iso_tristate")
	),
};

static const struct rtd_pin_config_desc rtd1635_iso_configs[] = {
	[RTD1635_ISO_GPIO_30] = RTK_PIN_CONFIG_V2(gpio_30, 0x14, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_31] = RTK_PIN_CONFIG_V2(gpio_31, 0x14, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_32] = RTK_PIN_CONFIG_V2(gpio_32, 0x14, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_33] = RTK_PIN_CONFIG_V2(gpio_33, 0x14, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_62] = RTK_PIN_CONFIG_V2(gpio_62, 0x14, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_63] = RTK_PIN_CONFIG_V2(gpio_63, 0x18, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_78] = RTK_PIN_CONFIG_V2(gpio_78, 0x18, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_79] = RTK_PIN_CONFIG_V2(gpio_79, 0x18, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_80] = RTK_PIN_CONFIG_V2(gpio_80, 0x18, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_81] = RTK_PIN_CONFIG_V2(gpio_81, 0x18, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_82] = RTK_PIN_CONFIG_V2(gpio_82, 0x1c, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_83] = RTK_PIN_CONFIG_V2(gpio_83, 0x1c, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_84] = RTK_PIN_CONFIG_V2(gpio_84, 0x1c, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_85] = RTK_PIN_CONFIG_V2(gpio_85, 0x1c, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_86] = RTK_PIN_CONFIG_V2(gpio_86, 0x1c, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_87] = RTK_PIN_CONFIG_V2(gpio_87, 0x20, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_88] = RTK_PIN_CONFIG_V2(gpio_88, 0x20, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_89] = RTK_PIN_CONFIG_V2(gpio_89, 0x20, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_90] = RTK_PIN_CONFIG_V2(gpio_90, 0x20, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_91] = RTK_PIN_CONFIG_V2(gpio_91, 0x20, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_92] = RTK_PIN_CONFIG_V2(gpio_92, 0x24, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_93] = RTK_PIN_CONFIG_V2(gpio_93, 0x24, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_110] = RTK_PIN_CONFIG_V2(gpio_110, 0x24, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_111] = RTK_PIN_CONFIG_V2(gpio_111, 0x24, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_112] = RTK_PIN_CONFIG_V2(gpio_112, 0x24, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_113] = RTK_PIN_CONFIG_V2(gpio_113, 0x28, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_114] = RTK_PIN_CONFIG_V2(gpio_114, 0x28, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_115] = RTK_PIN_CONFIG_V2(gpio_115, 0x28, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_116] = RTK_PIN_CONFIG_V2(gpio_116, 0x28, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_117] = RTK_PIN_CONFIG_V2(gpio_117, 0x28, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_118] = RTK_PIN_CONFIG_V2(gpio_118, 0x2c, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_119] = RTK_PIN_CONFIG_V2(gpio_119, 0x2c, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_120] = RTK_PIN_CONFIG_V2(gpio_120, 0x2c, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_123] = RTK_PIN_CONFIG_V2(gpio_123, 0x2c, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_GPIO_124] = RTK_PIN_CONFIG_V2(gpio_124, 0x2c, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISO_USB_CC1] = RTK_PIN_CONFIG_V2(usb_cc1, 0x30, 0, PCONF_UNSUPP, PCONF_UNSUPP, 0, 1, 2, 3, PADDRI_4_8),
	[RTD1635_ISO_USB_CC2] = RTK_PIN_CONFIG_V2(usb_cc2, 0x30, 4, PCONF_UNSUPP, PCONF_UNSUPP, 0, 1, 2, 3, PADDRI_4_8),
};

static const struct rtd_pin_config_desc rtd1635_isom_configs[] = {
	[RTD1635_ISOM_GPIO_0] = RTK_PIN_CONFIG_V2(gpio_0, 0x4, 5, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISOM_GPIO_1] = RTK_PIN_CONFIG_V2(gpio_1, 0x4, 11, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISOM_GPIO_28] = RTK_PIN_CONFIG_V2(gpio_28, 0x4, 17, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_ISOM_GPIO_29] = RTK_PIN_CONFIG_V2(gpio_29, 0x4, 23, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
};

static const struct rtd_pin_config_desc rtd1635_main2_configs[] = {
	[RTD1635_MAIN2_EMMC_CLK] = RTK_PIN_CONFIG(emmc_clk, 0x14, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_CMD] = RTK_PIN_CONFIG(emmc_cmd, 0x14, 13, 0, 1, PCONF_UNSUPP, 2, 13, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DATA_0] = RTK_PIN_CONFIG(emmc_data_0, 0x18, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DATA_1] = RTK_PIN_CONFIG(emmc_data_1, 0x18, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DATA_2] = RTK_PIN_CONFIG(emmc_data_2, 0x1c, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DATA_3] = RTK_PIN_CONFIG(emmc_data_3, 0x1c, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DATA_4] = RTK_PIN_CONFIG(emmc_data_4, 0x20, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DATA_5] = RTK_PIN_CONFIG(emmc_data_5, 0x20, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DATA_6] = RTK_PIN_CONFIG(emmc_data_6, 0x24, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DATA_7] = RTK_PIN_CONFIG(emmc_data_7, 0x24, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_DD_SB] = RTK_PIN_CONFIG(emmc_dd_sb, 0x28, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_EMMC_RST_N] = RTK_PIN_CONFIG(emmc_rst_n, 0x28, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_GPIO_8] = RTK_PIN_CONFIG_V2(gpio_8, 0x28, 26, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_9] = RTK_PIN_CONFIG_V2(gpio_9, 0x2c, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_10] = RTK_PIN_CONFIG_V2(gpio_10, 0x2c, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_11] = RTK_PIN_CONFIG_V2(gpio_11, 0x2c, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_16] = RTK_PIN_CONFIG_V2(gpio_16, 0x2c, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_17] = RTK_PIN_CONFIG_V2(gpio_17, 0x2c, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_18] = RTK_PIN_CONFIG_V2(gpio_18, 0x30, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_19] = RTK_PIN_CONFIG_V2(gpio_19, 0x30, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_20] = RTK_PIN_CONFIG_I2C(gpio_20, 0x30, 12, 1, 2, 0, 3, 4, 5, 7, 8, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_21] = RTK_PIN_CONFIG_I2C(gpio_21, 0x30, 21, 1, 2, 0, 3, 4, 5, 7, 8, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_22] = RTK_PIN_CONFIG_V2(gpio_22, 0x34, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_23] = RTK_PIN_CONFIG_V2(gpio_23, 0x34, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_44] = RTK_PIN_CONFIG(gpio_44, 0x34, 12, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_GPIO_45] = RTK_PIN_CONFIG(gpio_45, 0x38, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_GPIO_64] = RTK_PIN_CONFIG(gpio_64, 0x38, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_GPIO_65] = RTK_PIN_CONFIG(gpio_65, 0x3c, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_GPIO_66] = RTK_PIN_CONFIG(gpio_66, 0x3c, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_GPIO_67] = RTK_PIN_CONFIG(gpio_67, 0x40, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_GPIO_76] = RTK_PIN_CONFIG_V2(gpio_76, 0x40, 13, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_GPIO_77] = RTK_PIN_CONFIG_V2(gpio_77, 0x40, 19, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_MAIN2_HIF_CLK] = RTK_PIN_CONFIG(hif_clk, 0x44, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_HIF_DATA] = RTK_PIN_CONFIG(hif_data, 0x44, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_HIF_EN] = RTK_PIN_CONFIG(hif_en, 0x48, 0, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
	[RTD1635_MAIN2_HIF_RDY] = RTK_PIN_CONFIG(hif_rdy, 0x48, 13, 0, 1, PCONF_UNSUPP, 2, 12, PCONF_UNSUPP),
};

static const struct rtd_pin_config_desc rtd1635_npu_configs[] = {
	[RTD1635_NPU_GPIO_4] = RTK_PIN_CONFIG_V2(gpio_4, 0x10, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_5] = RTK_PIN_CONFIG_V2(gpio_5, 0x10, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_6] = RTK_PIN_CONFIG_V2(gpio_6, 0x10, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_7] = RTK_PIN_CONFIG_V2(gpio_7, 0x10, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_12] = RTK_PIN_CONFIG_V2(gpio_12, 0x10, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_13] = RTK_PIN_CONFIG_V2(gpio_13, 0x14, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8), /*OE_LED*/
	[RTD1635_NPU_GPIO_14] = RTK_PIN_CONFIG_V2(gpio_14, 0x14, 7, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_15] = RTK_PIN_CONFIG_V2(gpio_15, 0x14, 13, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_38] = RTK_PIN_CONFIG_V2(gpio_38, 0x14, 19, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_39] = RTK_PIN_CONFIG_V2(gpio_39, 0x14, 25, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_68] = RTK_PIN_CONFIG_V2(gpio_68, 0x18, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_69] = RTK_PIN_CONFIG_V2(gpio_69, 0x18, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_70] = RTK_PIN_CONFIG_V2(gpio_70, 0x18, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_71] = RTK_PIN_CONFIG_V2(gpio_71, 0x18, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_72] = RTK_PIN_CONFIG_V2(gpio_72, 0x18, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_73] = RTK_PIN_CONFIG_V2(gpio_73, 0x1c, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_74] = RTK_PIN_CONFIG_V2(gpio_74, 0x1c, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_75] = RTK_PIN_CONFIG_V2(gpio_75, 0x1c, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_104] = RTK_PIN_CONFIG_V2(gpio_104, 0x1c, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_105] = RTK_PIN_CONFIG_V2(gpio_105, 0x1c, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_129] = RTK_PIN_CONFIG_V2(gpio_129, 0x20, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_130] = RTK_PIN_CONFIG_V2(gpio_130, 0x20, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_131] = RTK_PIN_CONFIG_V2(gpio_131, 0x20, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_132] = RTK_PIN_CONFIG_V2(gpio_132, 0x20, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_133] = RTK_PIN_CONFIG_V2(gpio_133, 0x20, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_NPU_GPIO_134] = RTK_PIN_CONFIG_V2(gpio_134, 0x24, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
};


static const struct rtd_pin_config_desc rtd1635_ve2_configs[] = {
	[RTD1635_VE2_GPIO_46] = RTK_PIN_CONFIG_V2(gpio_46, 0x8, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_47] = RTK_PIN_CONFIG_V2(gpio_47, 0x8, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_48] = RTK_PIN_CONFIG_V2(gpio_48, 0x8, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_96] = RTK_PIN_CONFIG_V2(gpio_96, 0x8, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_97] = RTK_PIN_CONFIG_V2(gpio_97, 0x8, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_98] = RTK_PIN_CONFIG_V2(gpio_98, 0xc, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_99] = RTK_PIN_CONFIG_V2(gpio_99, 0xc, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_100] = RTK_PIN_CONFIG_V2(gpio_100, 0xc, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_101] = RTK_PIN_CONFIG_V2(gpio_101, 0xc, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE2_GPIO_125] = RTK_PIN_CONFIG_V2(gpio_125, 0xc, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8), /*OE_LED*/
	[RTD1635_VE2_GPIO_126] = RTK_PIN_CONFIG_V2(gpio_126, 0x10, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8), /*OE_LED*/
	[RTD1635_VE2_GPIO_127] = RTK_PIN_CONFIG_V2(gpio_127, 0x10, 7, 1, 2, 0, 3, 4, 5, PADDRI_4_8), /*OE_LED*/
	[RTD1635_VE2_GPIO_128] = RTK_PIN_CONFIG_V2(gpio_128, 0x10, 14, 1, 2, 0, 3, 4, 5, PADDRI_4_8), /*OE_LED*/
};

static const struct rtd_pin_config_desc rtd1635_ve3_configs[] = {
	[RTD1635_VE3_GPIO_2] = RTK_PIN_CONFIG_V2(gpio_2, 0xc, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_3] = RTK_PIN_CONFIG_V2(gpio_3, 0xc, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_24] = RTK_PIN_CONFIG_V2(gpio_24, 0xc, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_25] = RTK_PIN_CONFIG_V2(gpio_25, 0xc, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_26] = RTK_PIN_CONFIG_V2(gpio_26, 0xc, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_27] = RTK_PIN_CONFIG_V2(gpio_27, 0x10, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_34] = RTK_PIN_CONFIG_V2(gpio_34, 0x10, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_35] = RTK_PIN_CONFIG_V2(gpio_35, 0x10, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_36] = RTK_PIN_CONFIG_V2(gpio_36, 0x10, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_37] = RTK_PIN_CONFIG_V2(gpio_37, 0x10, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_61] = RTK_PIN_CONFIG_V2(gpio_61, 0x14, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_94] = RTK_PIN_CONFIG_V2(gpio_94, 0x14, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_95] = RTK_PIN_CONFIG_V2(gpio_95, 0x14, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_102] = RTK_PIN_CONFIG_V2(gpio_102, 0x14, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_103] = RTK_PIN_CONFIG_V2(gpio_103, 0x14, 24, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_106] = RTK_PIN_CONFIG_V2(gpio_106, 0x18, 0, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_107] = RTK_PIN_CONFIG_V2(gpio_107, 0x18, 6, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_108] = RTK_PIN_CONFIG_V2(gpio_108, 0x18, 12, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
	[RTD1635_VE3_GPIO_109] = RTK_PIN_CONFIG_V2(gpio_109, 0x18, 18, 1, 2, 0, 3, 4, 5, PADDRI_4_8),
};

static const struct rtd_pin_sconfig_desc rtd1635_main2_sconfigs[] = {
	RTK_PIN_SCONFIG(emmc_clk, 0x14, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(emmc_cmd, 0x14, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(emmc_data_0, 0x18, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(emmc_data_1, 0x18, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(emmc_data_2, 0x1c, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(emmc_data_3, 0x1c, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(emmc_data_4, 0x20, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(emmc_data_5, 0x20, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(emmc_data_6, 0x24, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(emmc_data_7, 0x24, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(emmc_dd_sb, 0x28, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(emmc_rst_n, 0x28, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(gpio_44, 0x34, 15, 3, 18, 3, 21, 3),
	RTK_PIN_SCONFIG(gpio_45, 0x38, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(gpio_64, 0x38, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(gpio_65, 0x3c, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(gpio_66, 0x3c, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(gpio_67, 0x40, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(hif_clk, 0x44, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(hif_data, 0x44, 16, 3, 19, 3, 22, 3),
	RTK_PIN_SCONFIG(hif_en, 0x48, 3, 3, 6, 3, 9, 3),
	RTK_PIN_SCONFIG(hif_rdy, 0x48, 16, 3, 19, 3, 22, 3),
};

static const struct rtd_reg_range rtd1635_iso_reg_ranges[] = {
	{ .offset = 0x0,  .len = 0x34 },
	{ .offset = 0x120, .len = 0x10 },
	{ .offset = 0x180, .len = 0xc },
	{ .offset = 0x1A0, .len = 0x10 },
};

static const struct rtd_pin_range rtd1635_iso_pin_ranges = {
	.ranges = rtd1635_iso_reg_ranges,
	.num_ranges = ARRAY_SIZE(rtd1635_iso_reg_ranges),
};

static const struct rtd_reg_range rtd1635_isom_reg_ranges[] = {
	{ .offset = 0x0,  .len = 0xc },
	{ .offset = 0x30, .len = 0x4 },
};

static const struct rtd_pin_range rtd1635_isom_pin_ranges = {
	.ranges = rtd1635_isom_reg_ranges,
	.num_ranges = ARRAY_SIZE(rtd1635_isom_reg_ranges),
};

static const struct rtd_reg_range rtd1635_main2_reg_ranges[] = {
	{ .offset = 0x0,  .len = 0x4c },
};

static const struct rtd_pin_range rtd1635_main2_pin_ranges = {
	.ranges = rtd1635_main2_reg_ranges,
	.num_ranges = ARRAY_SIZE(rtd1635_main2_reg_ranges),
};

static const struct rtd_reg_range rtd1635_npu_reg_ranges[] = {
	{ .offset = 0x0,  .len = 0x28 },
};

static const struct rtd_pin_range rtd1635_npu_pin_ranges = {
	.ranges = rtd1635_npu_reg_ranges,
	.num_ranges = ARRAY_SIZE(rtd1635_npu_reg_ranges),
};

static const struct rtd_reg_range rtd1635_ve2_reg_ranges[] = {
	{ .offset = 0x0,  .len = 0x14 },
};

static const struct rtd_pin_range rtd1635_ve2_pin_ranges = {
	.ranges = rtd1635_ve2_reg_ranges,
	.num_ranges = ARRAY_SIZE(rtd1635_ve2_reg_ranges),
};

static const struct rtd_reg_range rtd1635_ve3_reg_ranges[] = {
	{ .offset = 0x0,  .len = 0x1c },
};

static const struct rtd_pin_range rtd1635_ve3_pin_ranges = {
	.ranges = rtd1635_ve3_reg_ranges,
	.num_ranges = ARRAY_SIZE(rtd1635_ve3_reg_ranges),
};

static const struct rtd_pinctrl_desc rtd1635_iso_pinctrl_desc = {
	.pins = rtd1635_iso_pins,
	.num_pins = ARRAY_SIZE(rtd1635_iso_pins),
	.groups = rtd1635_iso_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1635_iso_pin_groups),
	.functions = rtd1635_iso_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1635_iso_pin_functions),
	.muxes = rtd1635_iso_muxes,
	.num_muxes = ARRAY_SIZE(rtd1635_iso_muxes),
	.configs = rtd1635_iso_configs,
	.num_configs = ARRAY_SIZE(rtd1635_iso_configs),
	.need_restore = 1,
	.pin_range = &rtd1635_iso_pin_ranges,
};

static const struct rtd_pinctrl_desc rtd1635_isom_pinctrl_desc = {
	.pins = rtd1635_isom_pins,
	.num_pins = ARRAY_SIZE(rtd1635_isom_pins),
	.groups = rtd1635_isom_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1635_isom_pin_groups),
	.functions = rtd1635_isom_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1635_isom_pin_functions),
	.muxes = rtd1635_isom_muxes,
	.num_muxes = ARRAY_SIZE(rtd1635_isom_muxes),
	.configs = rtd1635_isom_configs,
	.num_configs = ARRAY_SIZE(rtd1635_isom_configs),
	.gpio_func_name = "pctrl",
	.need_restore = 1,
	.pin_range = &rtd1635_isom_pin_ranges,
};

static const struct rtd_pinctrl_desc rtd1635_main2_pinctrl_desc = {
	.pins = rtd1635_main2_pins,
	.num_pins = ARRAY_SIZE(rtd1635_main2_pins),
	.groups = rtd1635_main2_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1635_main2_pin_groups),
	.functions = rtd1635_main2_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1635_main2_pin_functions),
	.muxes = rtd1635_main2_muxes,
	.num_muxes = ARRAY_SIZE(rtd1635_main2_muxes),
	.configs = rtd1635_main2_configs,
	.num_configs = ARRAY_SIZE(rtd1635_main2_configs),
	.sconfigs = rtd1635_main2_sconfigs,
	.num_sconfigs = ARRAY_SIZE(rtd1635_main2_sconfigs),
	.need_restore = 1,
	.pin_range = &rtd1635_main2_pin_ranges,
};

static const struct rtd_pinctrl_desc rtd1635_npu_pinctrl_desc = {
	.pins = rtd1635_npu_pins,
	.num_pins = ARRAY_SIZE(rtd1635_npu_pins),
	.groups = rtd1635_npu_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1635_npu_pin_groups),
	.functions = rtd1635_npu_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1635_npu_pin_functions),
	.muxes = rtd1635_npu_muxes,
	.num_muxes = ARRAY_SIZE(rtd1635_npu_muxes),
	.configs = rtd1635_npu_configs,
	.num_configs = ARRAY_SIZE(rtd1635_npu_configs),
	.need_restore = 1,
	.pin_range = &rtd1635_npu_pin_ranges,
};

static const struct rtd_pinctrl_desc rtd1635_ve2_pinctrl_desc = {
	.pins = rtd1635_ve2_pins,
	.num_pins = ARRAY_SIZE(rtd1635_ve2_pins),
	.groups = rtd1635_ve2_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1635_ve2_pin_groups),
	.functions = rtd1635_ve2_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1635_ve2_pin_functions),
	.muxes = rtd1635_ve2_muxes,
	.num_muxes = ARRAY_SIZE(rtd1635_ve2_muxes),
	.configs = rtd1635_ve2_configs,
	.num_configs = ARRAY_SIZE(rtd1635_ve2_configs),
	.need_restore = 1,
	.pin_range = &rtd1635_ve2_pin_ranges,
};

static const struct rtd_pinctrl_desc rtd1635_ve3_pinctrl_desc = {
	.pins = rtd1635_ve3_pins,
	.num_pins = ARRAY_SIZE(rtd1635_ve3_pins),
	.groups = rtd1635_ve3_pin_groups,
	.num_groups = ARRAY_SIZE(rtd1635_ve3_pin_groups),
	.functions = rtd1635_ve3_pin_functions,
	.num_functions = ARRAY_SIZE(rtd1635_ve3_pin_functions),
	.muxes = rtd1635_ve3_muxes,
	.num_muxes = ARRAY_SIZE(rtd1635_ve3_muxes),
	.configs = rtd1635_ve3_configs,
	.num_configs = ARRAY_SIZE(rtd1635_ve3_configs),
	.need_restore = 1,
	.pin_range = &rtd1635_ve3_pin_ranges,
};
#endif

