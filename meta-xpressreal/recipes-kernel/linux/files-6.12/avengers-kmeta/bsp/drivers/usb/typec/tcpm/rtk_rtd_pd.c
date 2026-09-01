// SPDX-License-Identifier: GPL-2.0
/*
 * rtk_rtd_pd.c - Realtek RTD SoC Type-C PD driver
 *
 * Copyright (C) 2025 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Stanley Chang <stanley_chang@realtek.com>
 */
//#define DEBUG

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/sched/clock.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/gpio/consumer.h>
#include <linux/usb.h>
#include <linux/usb/typec.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/pd.h>
#include <linux/usb/pd_vdo.h>
#include <linux/property.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/mod_devicetable.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/debugfs.h>

#include "rtk_rtd_pd.h"

/* CC status check timer periods (milliseconds) */
#define tCCCheckUnattached	50	/* Check period when unattached - slower to save power */
#define tCCCheckAttaching	10	/* Check period during connection - faster for quick detection */
#define tCCCheckAttached	100	/* Check period when attached - slowest, connection is stable */
#define tDRPToggleCheck		10	/* Legacy DRP toggle check time (kept for compatibility) */

#define LOG_BUFFER_ENTRIES	4096
#define LOG_BUFFER_ENTRY_SIZE	128

/* PD Message Logger for debugfs */
#define PD_LOG_BUFFER_SIZE 512  /* Circular buffer size */

/* Message direction */
enum pd_msg_direction {
	PD_MSG_TX,
	PD_MSG_RX,
};

/* PD Message log entry */
struct pd_msg_log_entry {
	ktime_t timestamp;                  /* Message timestamp */
	enum pd_msg_direction direction;    /* TX or RX */
	u16 header;                         /* PD message header */
	u8 count;                           /* Number of data objects */
	u32 data[7];                        /* Data objects (max 7) */
	enum tcpm_transmit_type sop_type;   /* SOP, SOP', SOP'' */
	enum typec_role power_role;         /* Source or Sink */
	enum typec_data_role data_role;     /* DFP(Host) or UFP(Device) */
};

/* PD Message Logger */
struct pd_msg_logger {
	struct pd_msg_log_entry entries[PD_LOG_BUFFER_SIZE];
	unsigned int head;                  /* Write position */
	unsigned int count;                 /* Number of entries */
	spinlock_t lock;                    /* Protect concurrent access */
	bool show_raw_data;                 /* Show raw data in second line */
};

struct cc_param {
	u32 rp_4p7k_code;
	u32 rp_36k_code;
	u32 rp_12k_code;
	u32 rd_code;
	u32 ra_code;
	u32 vref_2p6v;
	u32 vref_1p23v;
	u32 vref_0p8v;
	u32 vref_0p66v;
	u32 vref_0p4v;
	u32 vref_0p2v;
	u32 vref_1_1p6v;
	u32 vref_0_1p6v;
};

struct cc_cfg {
	int parameter_ver; /* Parameter version */
	int cc_dfp_mode;
	struct cc_param cc1_param;
	struct cc_param cc2_param;

	u32 debounce_val;
	bool use_defalut_parameter;
	bool use_typec_ctrl1;

	/* type c module setting*/
	u32 dfp_mode_rp_en;
	u32 ufp_mode_rd_en;
	u32 cc1_code;
	u32 cc2_code;
	u32 cc1_vref;
	u32 cc2_vref;
	u32 debounce; /* 1b,1us 7f,4.7us */

	/* CC status */
	u32 cc_role;
	enum typec_cc_status cc_setting;
	enum typec_cc_status cc_setting_prev;

	u32 cc_status;
	enum typec_cc_status cc1_level;
	enum typec_cc_status cc2_level;
	enum typec_cc_status cc1_level_prev;
	enum typec_cc_status cc2_level_prev;
};

/* -------------------- Driver private -------------------- */
struct rtk_pd {
	struct device    *dev;
	struct pd_regmap *pd_regmap;
	int               irq;
	struct mutex      lock;

	struct clk *clk;
	struct reset_control *reset;

	/* TCPM hook */
	struct tcpc_dev   tcpc;
	struct tcpm_port *tcpm;

	/* Parameters */
	struct cc_cfg *cc_cfg;

	/* Board hooks (optional) */
	struct gpio_desc *vbus_en_gpio;
	struct gpio_desc *vconn1_en_gpio;
	struct gpio_desc *vconn2_en_gpio;

	/* CC status and mode */
	enum typec_cc_polarity polarity;  /* Current CC polarity (CC1=0, CC2=1) */
	enum typec_role power_role;
	enum typec_data_role data_role;
	/* Pending roles for swap before tcpc set_roles runs */
	enum typec_role pending_power_role;
	enum typec_data_role pending_data_role;
	bool swap_roles_pending;
	bool is_attached;
	bool vbus_present;
	bool vconn_on;
	ktime_t last_rp_switch_ktime; /* Timestamp of last Rp level switch (for settling) */

	/* Altmode and partner runtime state (updated from VDM messages) */
	bool altmode_entered;     /* True if an altmode has been entered */
	u16 altmode_svid;         /* SVID of the active altmode (0 if none) */
	u8 altmode_mode;          /* Mode index of the active altmode */
	u16 partner_vid;          /* Partner VID from Discover_Identity ACK */
	u16 partner_pid;          /* Partner PID from Discover_Identity ACK */
	u32 dp_configure_rx;      /* DP Configure VDO received FROM partner (cmd 0x11) */
	u32 dp_status_rx;         /* DP Status VDO received FROM partner (cmd 0x10) */
	u32 dp_status_tx;         /* DP Status VDO sent BY us to partner (cmd 0x10) */

	struct hrtimer cc_check_timer;
	struct delayed_work vbus_delayed_work;

	/* TX state */
	bool tx_in_flight;
	bool tx_is_hard_reset;  /* True if current TX is a Hard Reset */

	/* TX delayed transmission (when RX is pending) */
	struct hrtimer tx_delay_timer;
	struct work_struct tx_delay_work;    /* Work to handle delayed TX in process context */
	bool tx_pending;                     /* True if TX is waiting for RX to complete */
	enum tcpm_transmit_type tx_pending_type;
	struct pd_message tx_pending_msg;
	unsigned int tx_pending_rev;

	/* RX state management */
	bool rx_enabled_by_tcpm; /* RX enable state requested by TCPM */
	bool rx_suspended_for_tx; /* RX temporarily suspended during TX */

	/* RX message buffering for TX_GC flow */
	struct pd_message rx_msg_pending; /* Buffered RX message */
	bool rx_msg_has_pending;           /* True if rx_msg_pending contains valid data */
	bool rx_msg_drop_pending;          /* True if buffered msg should be dropped (e.g. SOP') */
	int rx_msg_length;                 /* Length of buffered RX message */
	u32 rx_msg_wait_count;             /* Retry count waiting for TX_GC */
	struct delayed_work rx_msg_timeout_work; /* TX_GC timeout for buffered msg */

	/* regulators */
	struct regulator *vbus_reg;
	struct regulator *otg_reg;
	bool vbus_enabled;
	bool otg_enabled;

	/* Source mode voltage adjustment */
	bool src_pdo_requested;      /* True if Sink sent Request message */
	u32 src_req_voltage_mv;      /* Requested voltage in mV */
	u32 src_req_current_ma;      /* Requested current in mA */
	u32 src_req_pdo_index;       /* Requested PDO index (1-based) */

	/* PDO arrays read from DT/fwnode */
	u32 *src_pdo;                /* Source PDO array (dynamically allocated) */
	unsigned int nr_src_pdo;     /* Number of source PDOs */
	u32 *snk_pdo;                /* Sink PDO array (dynamically allocated) */
	unsigned int nr_snk_pdo;     /* Number of sink PDOs */

	/* Debug log control (sysfs) */
	bool pd_log_enable;	/* PD protocol logs (VBUS, TX/RX, power negotiation) */
	bool cc_log_enable;	/* Type-C CC logs (detection, attach/detach, polarity) */
	bool tc_log_enable;	/* TCPM interface logs (get_vbus, set_vbus, set_roles, etc.) */
	bool raw_data_enable;
	bool log_to_console;

	struct dentry *dentry;
	/* lock for log buffer access */
	struct mutex logbuffer_lock;
	int logbuffer_head;
	int logbuffer_tail;
	u8 *logbuffer[LOG_BUFFER_ENTRIES];

	/* PD message logger */
	struct pd_msg_logger *msg_logger;
};

/*
 * Logging
 */
static bool tcpc_log_full(struct rtk_pd *chip)
{
	return chip->logbuffer_tail ==
		(chip->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;
}

__printf(2, 0)
static void _tcpc_log(struct rtk_pd *chip, const char *fmt,
			 va_list args)
{
	char tmpbuffer[LOG_BUFFER_ENTRY_SIZE];
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec;

	if (!chip->logbuffer[chip->logbuffer_head]) {
		chip->logbuffer[chip->logbuffer_head] =
				kzalloc(LOG_BUFFER_ENTRY_SIZE, GFP_KERNEL);
		if (!chip->logbuffer[chip->logbuffer_head])
			return;
	}

	vsnprintf(tmpbuffer, sizeof(tmpbuffer), fmt, args);

	mutex_lock(&chip->logbuffer_lock);

	if (tcpc_log_full(chip)) {
		chip->logbuffer_head = max(chip->logbuffer_head - 1, 0);
		strscpy(tmpbuffer, "overflow", sizeof(tmpbuffer));
	}

	if (chip->logbuffer_head < 0 ||
	    chip->logbuffer_head >= LOG_BUFFER_ENTRIES) {
		dev_warn(chip->dev,
			 "Bad log buffer index %d\n", chip->logbuffer_head);
		goto abort;
	}

	if (!chip->logbuffer[chip->logbuffer_head]) {
		dev_warn(chip->dev,
			 "Log buffer index %d is NULL\n", chip->logbuffer_head);
		goto abort;
	}

	rem_nsec = do_div(ts_nsec, 1000000000);
	scnprintf(chip->logbuffer[chip->logbuffer_head],
		  LOG_BUFFER_ENTRY_SIZE, "[%5lu.%06lu] %s",
		  (unsigned long)ts_nsec, rem_nsec / 1000,
		  tmpbuffer);
	chip->logbuffer_head = (chip->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;

abort:
	mutex_unlock(&chip->logbuffer_lock);
}

__printf(2, 3)
static void tcpc_log(struct rtk_pd *chip, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_tcpc_log(chip, fmt, args);
	if (chip->log_to_console)
		dev_info(chip->dev, fmt, args);
	va_end(args);
}

/* ========== Dynamic Debug Log Macros ========== */
/* Usage:
 * - pd_log(p, "message")  - PD protocol related (VBUS, TX/RX, negotiation)
 * - cc_log(p, "message")  - Type-C CC related (detection, attach/detach)
 * - tc_log(p, "message")  - TCPM interface (get_vbus, set_vbus, set_roles, etc.)
 * - When sysfs flag is enabled: uses dev_info (always visible)
 * - When sysfs flag is disabled: uses dev_dbg (controlled by dynamic debug)
 *
 * Runtime control via sysfs:
 *   echo 1 > /sys/class/typec/portX/device/pd_log_enable
 *   echo 1 > /sys/class/typec/portX/device/cc_log_enable
 *   echo 1 > /sys/class/typec/portX/device/tc_log_enable
 *   echo 1 > /sys/class/typec/portX/device/raw_data_enable
 *   echo 1 > /sys/class/typec/portX/device/log_to_console
 *
 * Dynamic debug control (each call site can be independently controlled):
 *   # View all log call sites
 *   cat /sys/kernel/debug/dynamic_debug/control | grep rtk_rtd_pd
 *
 *   # Enable specific call site by line number
 *   echo 'file rtk_rtd_pd.c line 1951 +p' > /sys/kernel/debug/dynamic_debug/control
 *
 *   # Enable all TC logs
 *   echo 'file rtk_rtd_pd.c format "[TC]" +p' > /sys/kernel/debug/dynamic_debug/control
 *
 *   # Enable all PD logs
 *   echo 'file rtk_rtd_pd.c format "[PD]" +p' > /sys/kernel/debug/dynamic_debug/control
 *
 *   # Enable all CC logs
 *   echo 'file rtk_rtd_pd.c format "[CC]" +p' > /sys/kernel/debug/dynamic_debug/control
 */

#define pd_log(p, fmt, ...) \
	do { \
		if ((p)->pd_log_enable) \
			tcpc_log(p, "[PD] " fmt, ##__VA_ARGS__); \
		else \
			dev_dbg((p)->dev, "[PD] " fmt, ##__VA_ARGS__); \
	} while (0)

#define tc_log(p, fmt, ...) \
	do { \
		if ((p)->tc_log_enable) \
			tcpc_log(p, "[TC] " fmt, ##__VA_ARGS__); \
		else \
			dev_dbg((p)->dev, "[TC] " fmt, ##__VA_ARGS__); \
	} while (0)

#define cc_log(p, fmt, ...) \
	do { \
		if ((p)->cc_log_enable) \
			tcpc_log(p, "[CC] " fmt, ##__VA_ARGS__); \
		else \
			dev_dbg((p)->dev, "[CC] " fmt, ##__VA_ARGS__); \
	} while (0)

/* Dump all PD registers to seq_file or log */
static void rtk_pd_dump_registers(struct rtk_pd *chip, struct seq_file *s, const char *prefix)
{
	struct pd_regmap *pd_regmap = chip->pd_regmap;
	const char *pfx = prefix ? prefix : "";
	const char *pr_str, *dr_str, *pol_str;
	u32 val;

#define DUMP_REG(seq, reg_name, reg_offset) \
	do { \
		val = pd_readl(pd_regmap, reg_offset); \
		if (seq) \
			seq_printf(seq, "%s%-24s (0x%02x) = 0x%08x\n", pfx, #reg_name, reg_offset, val); \
		else \
			pd_log(chip, "%s%-24s (0x%02x) = 0x%08x\n", pfx, #reg_name, reg_offset, val); \
	} while (0)

#define DUMP_REG_FIELDS(seq, format, ...) \
	do { \
		if (seq) \
			seq_printf(seq, "%s  -> " format "\n", pfx, ##__VA_ARGS__); \
		else \
			pd_log(chip, "%s  -> " format "\n", pfx, ##__VA_ARGS__); \
	} while (0)

#define DUMP_SECTION(seq, title) \
	do { \
		if (seq) \
			seq_printf(seq, "\n%s=== %s ===\n", pfx, title); \
		else \
			pd_log(chip, "\n%s=== %s ===\n", pfx, title); \
	} while (0)

	/* Driver Status Summary */
	DUMP_SECTION(s, "DRIVER STATUS");
	pr_str = (chip->power_role == TYPEC_SOURCE) ? "SOURCE" :
		 (chip->power_role == TYPEC_SINK) ? "SINK" : "UNKNOWN";
	dr_str = (chip->data_role == TYPEC_HOST) ? "HOST" :
		 (chip->data_role == TYPEC_DEVICE) ? "DEVICE" : "UNKNOWN";
	pol_str = (chip->polarity == TYPEC_POLARITY_CC1) ? "CC1" : "CC2";

	DUMP_REG_FIELDS(s, "Power Role: %s, Data Role: %s, Polarity: %s",
			pr_str, dr_str, pol_str);
	DUMP_REG_FIELDS(s, "VBUS Present: %d",
			chip->vbus_present);
	DUMP_REG_FIELDS(s, "TX In Flight: %d, TX Hard Reset: %d",
			chip->tx_in_flight, chip->tx_is_hard_reset);
	DUMP_REG_FIELDS(s, "RX Enabled: %d, RX Suspended: %d, RX Msg Pending: %d",
			chip->rx_enabled_by_tcpm, chip->rx_suspended_for_tx,
			chip->rx_msg_has_pending);
	DUMP_REG_FIELDS(s, "TX Pending: %d",
			chip->tx_pending);
	if (chip->vbus_en_gpio)
		DUMP_REG_FIELDS(s, "GPIO: VBUS=%d", gpiod_get_value(chip->vbus_en_gpio));
	if (chip->vconn1_en_gpio || chip->vconn2_en_gpio)
		DUMP_REG_FIELDS(s, "GPIO: VCONN1=%d, VCONN2=%d",
				chip->vconn1_en_gpio ? gpiod_get_value(chip->vconn1_en_gpio) : -1,
				chip->vconn2_en_gpio ? gpiod_get_value(chip->vconn2_en_gpio) : -1);

	/* PD Interrupt Registers */
	DUMP_SECTION(s, "PD INTERRUPT REGISTERS");
	DUMP_REG(s, PD_INT, OFF_PD_INT);
	DUMP_REG_FIELDS(s, "IRQ Status: TX_OK=%d, RX_OK=%d, TX_GC=%d, VBUS_MON=%d, CC_DET=%d",
			!!(val & PD_INT_TX_OK), !!(val & PD_INT_RX_OK),
			!!(val & PD_INT_TX_GC), !!(val & PD_INT_VBUS_MON),
			!!(val & (PD_INT_CC1_DET | PD_INT_CC2_DET)));
	DUMP_REG_FIELDS(s, "IRQ Mask: TX_OK=%d, RX_OK=%d, TX_GC=%d, VBUS_MON=%d, CC_DET=%d (1=masked)",
			!!(val & PD_INT_MSK_TX_OK), !!(val & PD_INT_MSK_RX_OK),
			!!(val & PD_INT_MSK_TX_GC), !!(val & PD_INT_MSK_VBUS_MON),
			!!(val & (PD_INT_MSK_CC1_DET | PD_INT_MSK_CC2_DET)));

	/* PD Control Registers */
	DUMP_SECTION(s, "PD CONTROL REGISTERS");
	DUMP_REG(s, PD_BASIC_CTRL, OFF_PD_BASIC_CTRL);
	DUMP_REG_FIELDS(s, "INT_EN: TX_OK=%d, RX=%d, TX_GC=%d, VBUS_MON=%d, CC_DET=%d, RX_CRV_1ST=%d",
			!!(val & PD_INT_EN_TX_OK), !!(val & PD_INT_EN_RX),
			!!(val & PD_INT_EN_TX_GC), !!(val & PD_INT_EN_VBUS_MON),
			!!(val & (PD_INT_EN_CC1_DET | PD_INT_EN_CC2_DET)),
			!!(val & PD_INT_EN_RX_CRV_1ST));
	DUMP_REG_FIELDS(s, "BASIC_CTRL: RX_EN=%d, TX_EN=%d, BIST_EN=%d",
			!!(val & BASIC_RX_EN), !!(val & BASIC_TX_EN), !!(val & BASIC_BIST_EN));
	DUMP_REG_FIELDS(s, "BASIC_RST: FSM_RST=%d, TX_RST=%d, RX_RST=%d, BIST_RST=%d",
			!!(val & BASIC_FSM_RST), !!(val & BASIC_TX_RST),
			!!(val & BASIC_RX_RST), !!(val & BASIC_BIST_RST));

	DUMP_REG(s, PD_BASIC_CTRL2, OFF_PD_BASIC_CTRL2);
	DUMP_REG(s, PD_BASIC_CTRL3, OFF_PD_BASIC_CTRL3);

	/* TX Registers */
	DUMP_SECTION(s, "PD TX REGISTERS");
	DUMP_REG(s, PD_TX_CTRL0, OFF_PD_TX_CTRL0);
	{
		u32 tx_len = (val >> 20) & 0xFFF;
		u32 orderset = val & 0xFFFFF;
		const char *orderset_type = "Unknown";

		if (orderset == 0x8E318)
			orderset_type = "SOP";
		else if (orderset == 0xC9CE7)
			orderset_type = "Hard_Reset";
		else if (orderset == 0x8E339)
			orderset_type = "Cable_Reset";

		DUMP_REG_FIELDS(s, "TX_LEN=%d, ORDERSET=0x%05x (%s)",
				tx_len, orderset, orderset_type);
	}

	DUMP_REG(s, PD_TX_CTRL1, OFF_PD_TX_CTRL1);

	DUMP_REG(s, PD_TX_CTRL2, OFF_PD_TX_CTRL2);
	DUMP_REG_FIELDS(s, "CRC_EN=%d, EOP_EN=%d",
			!!(val & BIT(0)), !!(val & BIT(1)));

	DUMP_REG(s, PD_TX_CTRL3, OFF_PD_TX_CTRL3);
	DUMP_REG_FIELDS(s, "AUTO_RESP_GC=%d, SOP_CRC=%d, SOP1_CRC=%d, SOP1_DBG_CRC=%d",
			!!(val & BIT(0)), !!(val & BIT(1)),
			!!(val & BIT(2)), !!(val & BIT(3)));
	DUMP_REG_FIELDS(s, "SOP2_CRC=%d, SOP2_DBG_CRC=%d, WAIT_GC_CNT=0x%04x, TX_START_ADDR=0x%02x",
			!!(val & BIT(4)), !!(val & BIT(5)),
			(int)FIELD_GET(GENMASK(23, 8), val),
			(int)FIELD_GET(GENMASK(31, 24), val));

	DUMP_REG(s, PD_TX_CTRL4, OFF_PD_TX_CTRL4);
	DUMP_REG_FIELDS(s, "DATA_ROLE=%s, SPEC_REV=%d, POWER_ROLE=%s, EXTENDED=%d",
			(val & BIT(0)) ? "DFP" : "UFP",
			(int)FIELD_GET(GENMASK(2, 1), val),
			(val & BIT(3)) ? "SRC" : "SNK",
			!!(val & BIT(4)));
	DUMP_REG_FIELDS(s, "GC_COLLISION=%d, GC_SYMBOL=0x%05x, GC_FSM=0x%x",
			!!(val & BIT(5)),
			(int)FIELD_GET(GENMASK(27, 8), val),
			(int)FIELD_GET(GENMASK(31, 28), val));

	/* RX Registers */
	DUMP_SECTION(s, "PD RX REGISTERS");
	DUMP_REG(s, PD_RX_CTRL1, OFF_PD_RX_CTRL1);
	DUMP_REG(s, PD_RX_CTRL2, OFF_PD_RX_CTRL2);

	DUMP_REG(s, PD_RX_CTRL3, OFF_PD_RX_CTRL3);
	DUMP_REG_FIELDS(s, "AUTO_CMP_EN=%d, AUTO_CMP_VAL=%d, EN_AUTO_CLR=%d, AUTO_SET_RX_EN=%d",
			!!(val & BIT(0)),
			(int)FIELD_GET(GENMASK(2, 1), val),
			!!(val & BIT(3)),
			!!(val & BIT(12)));
	DUMP_REG_FIELDS(s, "AUTO_RX_EN_STATE=0x%x, AUTO_SET_RX_EN_CNT=0x%04x",
			(int)FIELD_GET(GENMASK(15, 13), val),
			(int)FIELD_GET(GENMASK(31, 16), val));

	DUMP_REG(s, PD_RX_ORDERSET, OFF_PD_RX_ORDERSET);
	DUMP_REG_FIELDS(s, "SOP=%d, CABLE_RST=%d, HARD_RST=%d",
			!!(val & BIT(3)), !!(val & BIT(1)), !!(val & BIT(2)));

	DUMP_REG(s, PD_RX_STATUS1, OFF_PD_RX_STATUS1);
	DUMP_REG_FIELDS(s, "BYTE_CNT=%d, CRC_OK=%d",
			(int)FIELD_GET(GENMASK(8, 0), val), !!(val & BIT(10)));

	DUMP_REG(s, PD_RX_STATUS2, OFF_PD_RX_STATUS2);
	{
		u32 rx_fsm = (int)FIELD_GET(GENMASK(30, 24), val);
		const char *rx_fsm_state;

		switch (rx_fsm) {
		case 1:  rx_fsm_state = "IDLE"; break;
		case 2:  rx_fsm_state = "WAIT"; break;
		case 4:  rx_fsm_state = "PREAMBLE"; break;
		case 8:  rx_fsm_state = "RCV_SYM"; break;
		case 16: rx_fsm_state = "DATA"; break;
		case 32: rx_fsm_state = "WAIT_END"; break;
		case 64: rx_fsm_state = "FINISH"; break;
		default: rx_fsm_state = "UNKNOWN"; break;
		}

		DUMP_REG_FIELDS(s, "RX_FSM=%d (%s), TIMEOUT_0=%d, TIMEOUT_1=%d, DEC_FAIL=%d",
				rx_fsm, rx_fsm_state,
				!!(val & BIT(21)), !!(val & BIT(22)), !!(val & BIT(23)));
	}

	DUMP_REG(s, PD_RX_TMP1, OFF_PD_RX_TMP1);
	DUMP_REG(s, PD_RX_TMP2, OFF_PD_RX_TMP2);

	/* Analog PHY Registers */
	DUMP_SECTION(s, "ANALOG PHY REGISTERS");
	DUMP_REG(s, PD_APHY_CTRL_0, OFF_PD_APHY_CTRL_0);
	DUMP_REG_FIELDS(s, "APHY_RX_EN=%d, APHY_TX_EN=%d",
			!!(val & BIT(0)), !!(val & BIT(1)));

	DUMP_REG(s, PD_APHY_CTRL_1, OFF_PD_APHY_CTRL_1);
	DUMP_REG_FIELDS(s, "SINK: EN=%d, VH=0x%02x, VL=0x%02x",
			!!(val & BIT(0)),
			(int)FIELD_GET(GENMASK(10, 6), val),
			(int)FIELD_GET(GENMASK(15, 11), val));
	DUMP_REG_FIELDS(s, "SOURCE: EN=%d, VH=0x%02x, VL=0x%02x",
			!!(val & BIT(16)),
			(int)FIELD_GET(GENMASK(26, 22), val),
			(int)FIELD_GET(GENMASK(31, 27), val));

	DUMP_REG(s, PD_APHY_CTRL_2, OFF_PD_APHY_CTRL_2);

	DUMP_REG(s, PD_APHY_CTRL_3, OFF_PD_APHY_CTRL_3);
	DUMP_REG_FIELDS(s, "HOLDB=%d, POW_PRS=%d, POW_FRS=%d, EN_IBHX=%d, EN_IBHN=%d",
			!!(val & BIT(0)), !!(val & BIT(1)), !!(val & BIT(2)),
			!!(val & BIT(4)), !!(val & BIT(5)));
	DUMP_REG_FIELDS(s, "PRS_TUNE=0x%x, FRS_TUNE=0x%x, SRP=0x%x, SNP=0x%x",
			(int)FIELD_GET(GENMASK(11, 8), val),
			(int)FIELD_GET(GENMASK(15, 12), val),
			(int)FIELD_GET(GENMASK(27, 24), val),
			(int)FIELD_GET(GENMASK(31, 28), val));

	DUMP_REG(s, PD_APHY_CC1_CTRL_0, OFF_PD_APHY_CC1_CTRL_0);
	DUMP_REG_FIELDS(s, "CC1_EN=%d", !!(val & BIT(1)));

	DUMP_REG(s, PD_APHY_CC1_CTRL_1, OFF_PD_APHY_CC1_CTRL_1);
	DUMP_REG(s, PD_APHY_CC1_CTRL_2, OFF_PD_APHY_CC1_CTRL_2);
	DUMP_REG(s, PD_APHY_CTRL_4, OFF_PD_APHY_CTRL_4);

	DUMP_REG(s, PD_APHY_CC2_CTRL_0, OFF_PD_APHY_CC2_CTRL_0);
	DUMP_REG_FIELDS(s, "CC2_EN=%d", !!(val & BIT(1)));

	DUMP_REG(s, PD_APHY_CC2_CTRL_1, OFF_PD_APHY_CC2_CTRL_1);
	DUMP_REG(s, PD_APHY_CC2_CTRL_2, OFF_PD_APHY_CC2_CTRL_2);
	DUMP_REG(s, PD_APHY_CTRL_4, OFF_PD_APHY_CTRL_4);

	/* Debounce and Timers */
	DUMP_SECTION(s, "DEBOUNCE & TIMERS");
	DUMP_REG(s, PD_DEBOUNCE_CTRL1, OFF_PD_DEBOUNCE_CTRL1);
	DUMP_REG(s, PD_DEBOUNCE_CTRL2, OFF_PD_DEBOUNCE_CTRL2);
	DUMP_REG(s, PD_DEBOUNCE_CTRL3, OFF_PD_DEBOUNCE_CTRL3);
	DUMP_REG(s, PD_DEBOUNCE_CTRL4, OFF_PD_DEBOUNCE_CTRL4);
	DUMP_REG(s, PD_DEBOUNCE_CTRL5, OFF_PD_DEBOUNCE_CTRL5);
	DUMP_REG(s, PD_TIMER1, OFF_PD_TIMER1);
	DUMP_REG(s, PD_TIMER2, OFF_PD_TIMER2);

	/* Protection and VCONN */
	DUMP_SECTION(s, "OVER-CURRENT PROTECTION & VCONN");
	DUMP_REG(s, PD_OC_PROTECT_CNT1, OFF_PD_OC_PROTECT_CNT1);
	DUMP_REG(s, PD_OC_PROTECT_CNT2, OFF_PD_OC_PROTECT_CNT2);
	DUMP_REG(s, PD_OC_PROTECT_CNT3, OFF_PD_OC_PROTECT_CNT3);
	DUMP_REG(s, PD_OC_PROTECT_STS1, OFF_PD_OC_PROTECT_STS1);
	DUMP_REG(s, PD_OC1_PROTECT_CNT, OFF_PD_OC1_PROTECT_CNT);
	DUMP_REG(s, PD_OC2_PROTECT_CNT, OFF_PD_OC2_PROTECT_CNT);
	DUMP_REG(s, PD_NEW_VCONN, OFF_PD_NEW_VCONN);

	/* Other Registers */
	DUMP_SECTION(s, "OTHER REGISTERS");
	DUMP_REG(s, PD_DUMMY_REG, OFF_PD_DUMMY_REG);
	DUMP_REG(s, PD_APAD_CTRL1, OFF_PD_APAD_CTRL1);
	DUMP_REG(s, PD_DET_TX, OFF_PD_DET_TX);
	DUMP_REG_FIELDS(s, "TX_COL_EN=%d, TX_COL_CNT=0x%x",
			!!(val & BIT(0)),
			(int)FIELD_GET(GENMASK(7, 4), val));

	DUMP_REG(s, PD_DET_CTRL, OFF_PD_DET_CTRL);
	DUMP_REG_FIELDS(s, "CC1_DET_EN=%d, CC1_DET_SEL=%d, CC2_DET_EN=%d, CC2_DET_SEL=%d",
			!!(val & BIT(0)),
			(int)FIELD_GET(GENMASK(2, 1), val),
			!!(val & BIT(4)),
			(int)FIELD_GET(GENMASK(6, 5), val));
	DUMP_REG_FIELDS(s, "CC1_DET_DATA=0x%03x, CC2_DET_DATA=0x%03x",
			(int)FIELD_GET(GENMASK(19, 8), val),
			(int)FIELD_GET(GENMASK(31, 20), val));
	DUMP_REG(s, PD_PREAMBLE_CTRL, OFF_PD_PREAMBLE_CTRL);
	DUMP_REG(s, PD_PREAMBLE_CTRL1, OFF_PD_PREAMBLE_CTRL1);
	DUMP_REG(s, PD_BMC_PREAMBLE, OFF_PD_BMC_PREAMBLE);
	DUMP_REG(s, PD_ORDERSET_00, OFF_PD_ORDERSET_00);
	DUMP_REG(s, PD_ORDERSET_L1, OFF_PD_ORDERSET_L1);
	DUMP_REG(s, PD_RX_ORDERSET_CTRL, OFF_PD_RX_ORDERSET_CTRL);
	DUMP_REG(s, PD_DEBUG_CTRL, OFF_PD_DEBUG_CTRL);
	DUMP_REG(s, PD_DEBUG, OFF_PD_DEBUG);

#undef DUMP_SECTION
#undef DUMP_REG_FIELDS
#undef DUMP_REG
}

/* Dump all Type-C registers to seq_file or log */
__maybe_unused
static void rtk_typec_dump_registers(struct rtk_pd *chip, struct seq_file *s, const char *prefix)
{
	struct pd_regmap *pd_regmap = chip->pd_regmap;
	const char *pfx = prefix ? prefix : "";
	const char *pr_str, *dr_str, *pol_str;
	u32 val;

#define DUMP_TYPEC_REG(seq, reg_name, reg_offset) \
	do { \
		val = typec_readl(pd_regmap, reg_offset); \
		if (seq) \
			seq_printf(seq, "%s%-24s (0x%03x) = 0x%08x\n", pfx, #reg_name, reg_offset, val); \
		else \
			cc_log(chip, "%s%-24s (0x%03x) = 0x%08x\n", pfx, #reg_name, reg_offset, val); \
	} while (0)

#define DUMP_TYPEC_REG_FIELDS(seq, format, ...) \
	do { \
		if (seq) \
			seq_printf(seq, "%s  -> " format "\n", pfx, ##__VA_ARGS__); \
		else \
			cc_log(chip, "%s  -> " format "\n", pfx, ##__VA_ARGS__); \
	} while (0)

#define DUMP_TYPEC_SECTION(seq, title) \
	do { \
		if (seq) \
			seq_printf(seq, "\n%s=== %s ===\n", pfx, title); \
		else \
			pd_log(chip, "\n%s=== %s ===\n", pfx, title); \
	} while (0)

	/* Driver Status Summary */
	DUMP_TYPEC_SECTION(s, "DRIVER STATUS");
	pr_str = (chip->power_role == TYPEC_SOURCE) ? "SOURCE" :
		 (chip->power_role == TYPEC_SINK) ? "SINK" : "UNKNOWN";
	dr_str = (chip->data_role == TYPEC_HOST) ? "HOST" :
		 (chip->data_role == TYPEC_DEVICE) ? "DEVICE" : "UNKNOWN";
	pol_str = (chip->polarity == TYPEC_POLARITY_CC1) ? "CC1" : "CC2";

	DUMP_TYPEC_REG_FIELDS(s, "Power Role: %s, Data Role: %s, Polarity: %s",
			pr_str, dr_str, pol_str);
	DUMP_TYPEC_REG_FIELDS(s, "VBUS Present: %d",
			chip->vbus_present);
	DUMP_TYPEC_REG_FIELDS(s, "TX In Flight: %d, TX Hard Reset: %d",
			chip->tx_in_flight, chip->tx_is_hard_reset);
	DUMP_TYPEC_REG_FIELDS(s, "RX Enabled: %d, RX Suspended: %d, RX Msg Pending: %d",
			chip->rx_enabled_by_tcpm, chip->rx_suspended_for_tx,
			chip->rx_msg_has_pending);
	DUMP_TYPEC_REG_FIELDS(s, "TX Pending: %d",
			chip->tx_pending);
	if (chip->vbus_en_gpio)
		DUMP_TYPEC_REG_FIELDS(s, "GPIO: VBUS=%d", gpiod_get_value(chip->vbus_en_gpio));
	if (chip->vconn1_en_gpio || chip->vconn2_en_gpio)
		DUMP_TYPEC_REG_FIELDS(s, "GPIO: VCONN1=%d, VCONN2=%d",
				chip->vconn1_en_gpio ? gpiod_get_value(chip->vconn1_en_gpio) : -1,
				chip->vconn2_en_gpio ? gpiod_get_value(chip->vconn2_en_gpio) : -1);

	DUMP_TYPEC_SECTION(s, "TYPEC REGISTERS STATUS");
	DUMP_TYPEC_REG(s, TYPEC_CTRL_CC1_0, USB_TYPEC_CTRL_CC1_0);
	DUMP_TYPEC_REG_FIELDS(s, "PLR_EN=%d, RA_EN=%d, CC_MODE=%d, PD_EN=%d",
			!!(val & PLR_EN), !!(val & EN_CC_RA),
			(val >> 5) & 0x3, !!(val & CC_PD_EN));
	DUMP_TYPEC_REG_FIELDS(s, "RP4P7K=%d, RP36K=%d, RP12K=%d, RD=%d, DET=%d",
			!!(val & EN_RP4P7K), !!(val & EN_RP36K),
			!!(val & EN_RP12K), !!(val & EN_RD), !!(val & EN_CC_DET));
	DUMP_TYPEC_REG_FIELDS(s, "RP4P7K_CODE=0x%x, RP36K_CODE=0x%x, RP12K_CODE=0x%x, RD_CODE=0x%x",
			code_rp4pk(val), code_rp36k(val), code_rp12k(val), code_rd(val));

	DUMP_TYPEC_REG(s, TYPEC_CTRL_CC1_1, USB_TYPEC_CTRL_CC1_1);
	if (chip->cc_cfg && chip->cc_cfg->parameter_ver == 1) {
		DUMP_TYPEC_REG_FIELDS(s, "VREF: 2p6v=0x%x, 1p23v=0x%x, 0p8v=0x%x, 0p66v=0x%x",
				V1_decode_2p6v(val), V1_decode_1p23v(val),
				V1_decode_0p8v(val), V1_decode_0p66v(val));
		DUMP_TYPEC_REG_FIELDS(s, "VREF: 0p4v=0x%x, 0p2v=0x%x, 1_1p6v=0x%x, 0_1p6v=0x%x",
				V1_decode_0p4v(val), V1_decode_0p2v(val),
				V1_decode_1_1p6v(val), V1_decode_0_1p6v(val));
	} else {
		DUMP_TYPEC_REG_FIELDS(s, "VREF: 2p6v=0x%x, 1p23v=0x%x, 0p8v=0x%x, 0p66v=0x%x",
				V0_decode_2p6v(val), V0_decode_1p23v(val),
				V0_decode_0p8v(val), V0_decode_0p66v(val));
		DUMP_TYPEC_REG_FIELDS(s, "VREF: 0p4v=0x%x, 0p2v=0x%x, 1_1p6v=0x%x, 0_1p6v=0x%x",
				V0_decode_0p4v(val), V0_decode_0p2v(val),
				V0_decode_1_1p6v(val), V0_decode_0_1p6v(val));
	}

	DUMP_TYPEC_REG(s, TYPEC_CTRL_CC2_0, USB_TYPEC_CTRL_CC2_0);
	DUMP_TYPEC_REG_FIELDS(s, "PLR_EN=%d, RA_EN=%d, CC_MODE=%d, PD_EN=%d",
			!!(val & PLR_EN), !!(val & EN_CC_RA),
			(val >> 5) & 0x3, !!(val & CC_PD_EN));
	DUMP_TYPEC_REG_FIELDS(s, "RP4P7K=%d, RP36K=%d, RP12K=%d, RD=%d, DET=%d",
			!!(val & EN_RP4P7K), !!(val & EN_RP36K),
			!!(val & EN_RP12K), !!(val & EN_RD), !!(val & EN_CC_DET));
	DUMP_TYPEC_REG_FIELDS(s, "RP4P7K_CODE=0x%x, RP36K_CODE=0x%x, RP12K_CODE=0x%x, RD_CODE=0x%x",
			code_rp4pk(val), code_rp36k(val), code_rp12k(val), code_rd(val));

	DUMP_TYPEC_REG(s, TYPEC_CTRL_CC2_1, USB_TYPEC_CTRL_CC2_1);
	if (chip->cc_cfg && chip->cc_cfg->parameter_ver == 1) {
		DUMP_TYPEC_REG_FIELDS(s, "VREF: 2p6v=0x%x, 1p23v=0x%x, 0p8v=0x%x, 0p66v=0x%x",
				V1_decode_2p6v(val), V1_decode_1p23v(val),
				V1_decode_0p8v(val), V1_decode_0p66v(val));
		DUMP_TYPEC_REG_FIELDS(s, "VREF: 0p4v=0x%x, 0p2v=0x%x, 1_1p6v=0x%x, 0_1p6v=0x%x",
				V1_decode_0p4v(val), V1_decode_0p2v(val),
				V1_decode_1_1p6v(val), V1_decode_0_1p6v(val));
	} else {
		DUMP_TYPEC_REG_FIELDS(s, "VREF: 2p6v=0x%x, 1p23v=0x%x, 0p8v=0x%x, 0p66v=0x%x",
				V0_decode_2p6v(val), V0_decode_1p23v(val),
				V0_decode_0p8v(val), V0_decode_0p66v(val));
		DUMP_TYPEC_REG_FIELDS(s, "VREF: 0p4v=0x%x, 0p2v=0x%x, 1_1p6v=0x%x, 0_1p6v=0x%x",
				V0_decode_0p4v(val), V0_decode_0p2v(val),
				V0_decode_1_1p6v(val), V0_decode_0_1p6v(val));
	}

	DUMP_TYPEC_REG(s, TYPEC_STS, USB_TYPEC_STS);
	DUMP_TYPEC_REG_FIELDS(s, "CC1_DET_STS=%d, CC2_DET_STS=%d",
			(val >> CC1_DET_STS_SHIFT) & CC_DET_STS_MASK,
			(val >> CC2_DET_STS_SHIFT) & CC_DET_STS_MASK);

	DUMP_TYPEC_REG(s, TYPEC_CTRL, USB_TYPEC_CTRL);
	DUMP_TYPEC_REG_FIELDS(s, "DVDD_PWRCUT=%d, PROB_MAIN_ISO=%d, DET_INT_MASK=%d",
			!!(val & DVDD_PWRCUT), !!(val & PROB_MAIN_ISO), !!(val & DET_INT_MASK));
	DUMP_TYPEC_REG_FIELDS(s, "CC2_INT_EN=%d, CC1_INT_EN=%d, CC2_INT_STS=%d, CC1_INT_STS=%d",
			!!(val & CC2_INT_EN), !!(val & CC1_INT_EN),
			!!(val & CC2_INT_STS), !!(val & CC1_INT_STS));
	DUMP_TYPEC_REG_FIELDS(s, "DEBOUNCE_EN=%d, DEBOUNCE_TIME=0x%x",
			!!(val & DEBOUNCE_EN), val & DEBOUNCE_TIME_MASK);

	DUMP_TYPEC_REG(s, TYPEC_CTRL_1, USB_TYPEC_CTRL_1);
	DUMP_TYPEC_REG_FIELDS(s, "TYPEC_CTRL_SEL=%d, CC2_OVP=%d, PLR_EN_CC2=%d, CC1_OVP=%d, PLR_EN_CC1=%d",
			!!(val & TYPEC_CTRL_SEL), !!(val & EN_CC2_OVP), !!(val & PLR_EN_CC2),
			!!(val & EN_CC1_OVP), !!(val & PLR_EN_CC1));
	DUMP_TYPEC_REG_FIELDS(s, "CC2: RA=%d, MODE=%d, PD_EN=%d, RP4P7K=%d, RP36K=%d, RP12K=%d, RD=%d, DET=%d",
			!!(val & EN_CC2_RA), (val >> 13) & 0x3, !!(val & CC2_PD_EN),
			!!(val & EN_CC2_RP4P7K), !!(val & EN_CC2_RP36K),
			!!(val & EN_CC2_RP12K), !!(val & EN_CC2_RD), !!(val & EN_CC2_DET));
	DUMP_TYPEC_REG_FIELDS(s, "CC1: RA=%d, MODE=%d, PD_EN=%d, RP4P7K=%d, RP36K=%d, RP12K=%d, RD=%d, DET=%d",
			!!(val & EN_CC1_RA), (val >> 5) & 0x3, !!(val & CC1_PD_EN),
			!!(val & EN_CC1_RP4P7K), !!(val & EN_CC1_RP36K),
			!!(val & EN_CC1_RP12K), !!(val & EN_CC1_RD), !!(val & EN_CC1_DET));

#undef DUMP_TYPEC_SECTION
#undef DUMP_TYPEC_REG_FIELDS
#undef DUMP_TYPEC_REG
}

/* Get PD message type name */
static const char *pd_ctrl_msg_name(u8 type)
{
	static const char * const names[] = {
		[0] = "Reserved",
		[PD_CTRL_GOOD_CRC] = "GoodCRC",
		[PD_CTRL_GOTO_MIN] = "GotoMin",
		[PD_CTRL_ACCEPT] = "Accept",
		[PD_CTRL_REJECT] = "Reject",
		[PD_CTRL_PING] = "Ping",
		[PD_CTRL_PS_RDY] = "PS_RDY",
		[PD_CTRL_GET_SOURCE_CAP] = "Get_Source_Cap",
		[PD_CTRL_GET_SINK_CAP] = "Get_Sink_Cap",
		[PD_CTRL_DR_SWAP] = "DR_Swap",
		[PD_CTRL_PR_SWAP] = "PR_Swap",
		[PD_CTRL_VCONN_SWAP] = "VCONN_Swap",
		[PD_CTRL_WAIT] = "Wait",
		[PD_CTRL_SOFT_RESET] = "Soft_Reset",
		[PD_CTRL_NOT_SUPP] = "Not_Supported",
		[PD_CTRL_GET_SOURCE_CAP_EXT] = "Get_Source_Cap_Ext",
		[PD_CTRL_GET_STATUS] = "Get_Status",
		[PD_CTRL_FR_SWAP] = "FR_Swap",
		[PD_CTRL_GET_PPS_STATUS] = "Get_PPS_Status",
		[PD_CTRL_GET_COUNTRY_CODES] = "Get_Country_Codes",
	};

	if (type < ARRAY_SIZE(names) && names[type])
		return names[type];
	return "Unknown";
}

static const char *pd_data_msg_name(u8 type)
{
	static const char * const names[] = {
		[0] = "Reserved",
		[PD_DATA_SOURCE_CAP] = "Source_Capabilities",
		[PD_DATA_REQUEST] = "Request",
		[PD_DATA_BIST] = "BIST",
		[PD_DATA_SINK_CAP] = "Sink_Capabilities",
		[PD_DATA_BATT_STATUS] = "Battery_Status",
		[PD_DATA_ALERT] = "Alert",
		[PD_DATA_GET_COUNTRY_INFO] = "Get_Country_Info",
		[PD_DATA_ENTER_USB] = "Enter_USB",
		[PD_DATA_VENDOR_DEF] = "Vendor_Defined",
	};

	if (type < ARRAY_SIZE(names) && names[type])
		return names[type];
	return "Unknown";
}

/* Dump PD message RAM (RX and TX) */
static void rtk_pd_dump_message_ram(struct rtk_pd *chip, struct seq_file *s,
				    bool is_tx, const char *prefix)
{
	struct pd_regmap *pd_regmap = chip->pd_regmap;
	const char *pfx = prefix ? prefix : "";
	void __iomem *ram = is_tx ? pd_regmap->ram_tx : pd_regmap->ram_rx;
	struct pd_message msg;
	u16 header;
	u8 type, cnt, rev, id;
	bool pwr_role, data_role, ext_hdr;
	int i, max_len;
	const char *msg_name;
	bool is_goodcrc;
	u32 tx_ctrl4 = 0;

	if (!ram) {
		if (s)
			seq_printf(s, "%s%s RAM not available\n", pfx, is_tx ? "TX" : "RX");
		else
			pd_log(chip, "%s%s RAM not available\n", pfx, is_tx ? "TX" : "RX");
		return;
	}

	/* Read message from RAM */
	memset(&msg, 0, sizeof(msg));
	max_len = sizeof(msg);
	for (i = 0; i < max_len; i++)
		((u8 *)&msg)[i] = readb(ram + i);

	header = le16_to_cpu(msg.header);

	/* Parse header */
	type = (header >> PD_HEADER_TYPE_SHIFT) & PD_HEADER_TYPE_MASK;
	cnt = (header >> PD_HEADER_CNT_SHIFT) & PD_HEADER_CNT_MASK;
	id = (header >> PD_HEADER_ID_SHIFT) & PD_HEADER_ID_MASK;
	pwr_role = !!(header & PD_HEADER_PWR_ROLE);
	data_role = !!(header & PD_HEADER_DATA_ROLE);
	rev = (header >> PD_HEADER_REV_SHIFT) & PD_HEADER_REV_MASK;
	ext_hdr = !!(header & PD_HEADER_EXT_HDR);
	is_goodcrc = (cnt == 0 && type == PD_CTRL_GOOD_CRC);

	/* GoodCRC is auto-generated by HW; expose the programmed fields */
	if (is_goodcrc && chip->pd_regmap)
		tx_ctrl4 = pd_readl(chip->pd_regmap, OFF_PD_TX_CTRL4);

	/* Determine message name */
	if (cnt == 0)
		msg_name = pd_ctrl_msg_name(type);
	else
		msg_name = pd_data_msg_name(type);

	if (s) {
		seq_printf(s, "%s=== %s Message ===\n", pfx, is_tx ? "TX" : "RX");
		seq_printf(s, "%sHeader: 0x%04x\n", pfx, header);
		seq_printf(s, "%s  Message Type: %s (0x%x)\n", pfx, msg_name, type);
		seq_printf(s, "%s  Data Objects: %d\n", pfx, cnt);
		seq_printf(s, "%s  Message ID: %d\n", pfx, id);
		seq_printf(s, "%s  Power Role: %s\n", pfx, pwr_role ? "Source" : "Sink");
		seq_printf(s, "%s  Data Role: %s\n", pfx, data_role ? "DFP" : "UFP");
		seq_printf(s, "%s  Spec Rev: %d.0\n", pfx, rev + 1);
		seq_printf(s, "%s  Extended: %s\n", pfx, ext_hdr ? "Yes" : "No");
		if (is_goodcrc) {
			seq_printf(s,
				   "%s  GoodCRC cfg (TX_CTRL4): PR=%s DR=%s Rev=%d Ext=%s GC_Sym=0x%x GC_FSM=0x%x Coll=%s\n",
				   pfx,
				   (tx_ctrl4 & BIT(3)) ? "SRC" : "SNK",
				   (tx_ctrl4 & BIT(0)) ? "DFP" : "UFP",
				   (int)FIELD_GET(GENMASK(2, 1), tx_ctrl4) + 1,
				   (tx_ctrl4 & BIT(4)) ? "Y" : "N",
				   (u32)FIELD_GET(GENMASK(27, 8), tx_ctrl4),
				   tx_ctrl4 >> 28,
				   (tx_ctrl4 & BIT(5)) ? "Y" : "N");
		}

		if (cnt > 0) {
			seq_printf(s, "%sData Objects:\n", pfx);
			for (i = 0; i < cnt && i < PD_MAX_PAYLOAD; i++) {
				u32 obj = le32_to_cpu(msg.payload[i]);
				seq_printf(s, "%s  [%d]: 0x%08x", pfx, i, obj);

				/* Parse specific message types */
				if (type == PD_DATA_REQUEST && i == 0) {
					u32 pos = (obj >> 28) & 0x7;
					u32 current_ma = (obj >> 10) & 0x3ff;
					u32 voltage_mv = obj & 0x3ff;

					seq_printf(s, " (Pos:%d, I:%dmA, V:%dmV)",
						   pos, current_ma * 10, voltage_mv * 50);
				} else if (type == PD_DATA_SOURCE_CAP || type == PD_DATA_SINK_CAP) {
					u32 pdo_type = (obj >> 30) & 0x3;
					const char *pdo_names[] = {"Fixed", "Battery", "Variable", "APDO"};
					seq_printf(s, " (%s PDO)", pdo_names[pdo_type]);

					if (pdo_type == 0) { /* Fixed PDO */
						u32 voltage_mv = ((obj >> 10) & 0x3ff) * 50;
						u32 current_ma = (obj & 0x3ff) * 10;

						seq_printf(s, " %dmV/%dmA", voltage_mv, current_ma);
					}
				}
				seq_puts(s, "\n");
			}
		}

		/* Raw data dump */
		seq_printf(s, "%sRaw Data (first 32 bytes):\n", pfx);
		for (i = 0; i < 32 && i < max_len; i++) {
			if (i % 16 == 0)
				seq_printf(s, "%s%04x: ", pfx, i);
			seq_printf(s, "%02x ", ((u8 *)&msg)[i]);
			if (i % 16 == 15 || i == 31)
				seq_puts(s, "\n");
		}
	} else {
		pd_log(chip, "%s=== %s Message ===\n", pfx, is_tx ? "TX" : "RX");
		pd_log(chip, "%sHeader: 0x%04x\n", pfx, header);
		pd_log(chip, "%s  Message Type: %s (0x%x)\n", pfx, msg_name, type);
		pd_log(chip, "%s  Data Objects: %d\n", pfx, cnt);
		pd_log(chip, "%s  Message ID: %d\n", pfx, id);
		pd_log(chip, "%s  Power Role: %s\n", pfx, pwr_role ? "Source" : "Sink");
		pd_log(chip, "%s  Data Role: %s\n", pfx, data_role ? "DFP" : "UFP");
		pd_log(chip, "%s  Spec Rev: %d.0\n", pfx, rev + 1);
		pd_log(chip, "%s  Extended: %s\n", pfx, ext_hdr ? "Yes" : "No");
		if (is_goodcrc) {
			pd_log(chip, "%s  GoodCRC cfg (TX_CTRL4): PR=%s DR=%s Rev=%d Ext=%s GC_Sym=0x%x GC_FSM=0x%x Coll=%s\n",
			       pfx,
			       (tx_ctrl4 & BIT(3)) ? "SRC" : "SNK",
			       (tx_ctrl4 & BIT(0)) ? "DFP" : "UFP",
			       (int)FIELD_GET(GENMASK(2, 1), tx_ctrl4) + 1,
			       (tx_ctrl4 & BIT(4)) ? "Y" : "N",
			       (u32)FIELD_GET(GENMASK(27, 8), tx_ctrl4),
			       tx_ctrl4 >> 28,
			       (tx_ctrl4 & BIT(5)) ? "Y" : "N");
		}

		if (cnt > 0) {
			for (i = 0; i < cnt && i < PD_MAX_PAYLOAD; i++) {
				u32 obj = le32_to_cpu(msg.payload[i]);
				pd_log(chip, "%s  Data[%d]: 0x%08x\n", pfx, i, obj);
			}
		}
	}
}

/* Alternate Modes Info */
/* Update altmode/partner tracking state from a received VDM message.
 * Called under p->lock from the IRQ handler (TX_GC path).
 */
static void rtk_pd_track_vdm_rx(struct rtk_pd *p, const struct pd_message *msg, u8 obj_cnt)
{
	u32 vdm_hdr    = le32_to_cpu(msg->payload[0]);
	u16 svid       = (vdm_hdr >> 16) & 0xFFFF;
	u8 vdm_cmd     = vdm_hdr & 0x1F;
	u8 vdm_cmd_type = (vdm_hdr >> 6) & 0x3;

	/* Discover_Identity ACK (SVID=0xFF00, cmd=1): capture partner VID/PID */
	if (svid == 0xFF00 && vdm_cmd == CMD_DISCOVER_IDENT &&
	    vdm_cmd_type == CMDT_RSP_ACK && obj_cnt >= 4) {
		u32 id_header  = le32_to_cpu(msg->payload[1]);
		u32 product_vdo = le32_to_cpu(msg->payload[3]);

		p->partner_vid = PD_IDH_VID(id_header);
		p->partner_pid = (product_vdo >> 16) & 0xFFFF;
	}

	/* Enter_Mode: mark altmode as active.
	 * DFP role: we TX REQ and RX ACK (detect ACK here).
	 * UFP role: partner TX REQ, we RX REQ and then TX ACK (detect REQ here).
	 * Both cases are handled by accepting either CMDT_INIT or CMDT_RSP_ACK.
	 */
	if (vdm_cmd == CMD_ENTER_MODE &&
	    (vdm_cmd_type == CMDT_RSP_ACK || vdm_cmd_type == CMDT_INIT)) {
		p->altmode_entered = true;
		p->altmode_svid = svid;
		p->altmode_mode = (vdm_hdr >> 8) & 0x7;
	}

	/* Exit_Mode: clear altmode (REQ from partner or ACK to our REQ) */
	if (vdm_cmd == CMD_EXIT_MODE &&
	    (vdm_cmd_type == CMDT_RSP_ACK || vdm_cmd_type == CMDT_INIT) &&
	    svid == p->altmode_svid) {
		p->altmode_entered = false;
		p->altmode_svid = 0;
		p->altmode_mode = 0;
	}

	/* DP Status_Update (cmd=0x10) or Attention (cmd=6): partner's DP status.
	 * HPD state changes are signaled via Attention, not DP_Status_Update.
	 */
	if (svid == 0xFF01 &&
	    (vdm_cmd == 0x10 || vdm_cmd == CMD_ATTENTION) && obj_cnt >= 2)
		p->dp_status_rx = le32_to_cpu(msg->payload[1]);

	/* DP Configure (cmd=0x11): partner configures our DP pin assignment */
	if (svid == 0xFF01 && vdm_cmd == 0x11 && obj_cnt >= 2)
		p->dp_configure_rx = le32_to_cpu(msg->payload[1]);
}

#ifdef CONFIG_DEBUG_FS
static int tcpc_debug_show(struct seq_file *s, void *v)
{
	struct rtk_pd *chip = s->private;
	int tail;

	mutex_lock(&chip->logbuffer_lock);
	tail = chip->logbuffer_tail;
	while (tail != chip->logbuffer_head) {
		seq_printf(s, "%s", chip->logbuffer[tail]);
		tail = (tail + 1) % LOG_BUFFER_ENTRIES;
	}
	if (!seq_has_overflowed(s))
		chip->logbuffer_tail = tail;
	mutex_unlock(&chip->logbuffer_lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(tcpc_debug);

static int pd_registers_show(struct seq_file *s, void *v)
{
	struct rtk_pd *chip = s->private;

	seq_puts(s, "RTK PD Register Dump:\n");
	seq_puts(s, "=====================\n");
	rtk_pd_dump_registers(chip, s, "");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(pd_registers);

static int typec_registers_show(struct seq_file *s, void *v)
{
	struct rtk_pd *chip = s->private;

	seq_puts(s, "RTK Type-C Register Dump:\n");
	seq_puts(s, "=========================\n");
	rtk_typec_dump_registers(chip, s, "");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(typec_registers);

static int pd_message_ram_show(struct seq_file *s, void *v)
{
	struct rtk_pd *chip = s->private;

	seq_puts(s, "RTK PD Message RAM Dump:\n");
	seq_puts(s, "========================\n\n");

	rtk_pd_dump_message_ram(chip, s, false, "");  /* RX */
	seq_puts(s, "\n");
	rtk_pd_dump_message_ram(chip, s, true, "");   /* TX */

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(pd_message_ram);

/* Forward declarations for debugfs functions */
static int rtk_pd_get_cc_pair(struct rtk_pd *p,
			       enum typec_cc_status *cc1,
			       enum typec_cc_status *cc2);
static const char *get_cc_voltage_leve_str(enum typec_cc_status cc_level);

/* Connector Configuration (from DTS) */
static int connector_config_show(struct seq_file *s, void *unused)
{
	struct rtk_pd *p = s->private;
	struct fwnode_handle *fwnode = p->tcpc.fwnode;
	u32 src_pdo[7], snk_pdo[7];
	int nr_src = 0, nr_snk = 0;
	const char *pwr_role_str, *data_role_str, *try_role_str;
	u32 val;
	int i;

	if (!fwnode) {
		seq_puts(s, "No connector node in device tree\n");
		return 0;
	}

	seq_puts(s, "=======================================================================\n");
	seq_puts(s, "           USB Type-C Connector Configuration (DTS)                   \n");
	seq_puts(s, "=======================================================================\n\n");

	/* Power Role */
	if (!fwnode_property_read_string(fwnode, "power-role", &pwr_role_str))
		seq_printf(s, "Power Role:           %s\n", pwr_role_str);
	else
		seq_puts(s, "Power Role:           <not specified>\n");

	/* Data Role */
	if (!fwnode_property_read_string(fwnode, "data-role", &data_role_str))
		seq_printf(s, "Data Role:            %s\n", data_role_str);
	else
		seq_puts(s, "Data Role:            <not specified>\n");

	/* Try Role */
	if (!fwnode_property_read_string(fwnode, "try-power-role", &try_role_str))
		seq_printf(s, "Try Power Role:       %s\n", try_role_str);
	else
		seq_puts(s, "Try Power Role:       <not specified>\n");

	seq_puts(s, "\n");

	/* Source Capabilities */
	nr_src = fwnode_property_count_u32(fwnode, "source-pdos");
	if (nr_src > 0) {
		nr_src = min(nr_src, 7);
		fwnode_property_read_u32_array(fwnode, "source-pdos", src_pdo, nr_src);

		seq_printf(s, "Source PDOs:          %d PDO(s)\n", nr_src);
		for (i = 0; i < nr_src; i++) {
			u32 pdo = src_pdo[i];
			u32 type = (pdo >> 30) & 0x3;

			seq_printf(s, "  PDO[%d]: 0x%08x  ", i, pdo);

			switch (type) {
			case 0: /* Fixed Supply */
			{
				u32 voltage_mv = ((pdo >> 10) & 0x3FF) * 50;
				u32 current_ma = (pdo & 0x3FF) * 10;
				seq_printf(s, "Fixed %umV @ %umA", voltage_mv, current_ma);
				if (i == 0)
					seq_puts(s, " (vSafe5V)");
				break;
			}
			case 1: /* Battery */
			{
				u32 max_v = ((pdo >> 20) & 0x3FF) * 50;
				u32 min_v = ((pdo >> 10) & 0x3FF) * 50;
				u32 power_mw = (pdo & 0x3FF) * 250;
				seq_printf(s, "Battery %u-%umV @ %umW", min_v, max_v, power_mw);
				break;
			}
			case 2: /* Variable Supply */
			{
				u32 max_v = ((pdo >> 20) & 0x3FF) * 50;
				u32 min_v = ((pdo >> 10) & 0x3FF) * 50;
				u32 current_ma = (pdo & 0x3FF) * 10;
				seq_printf(s, "Variable %u-%umV @ %umA", min_v, max_v, current_ma);
				break;
			}
			case 3: /* Augmented (PPS) */
			{
				u32 max_v = ((pdo >> 17) & 0xFF) * 100;
				u32 min_v = ((pdo >> 8) & 0xFF) * 100;
				u32 current_ma = (pdo & 0x7F) * 50;
				seq_printf(s, "PPS %u-%umV @ %umA", min_v, max_v, current_ma);
				break;
			}
			}
			seq_puts(s, "\n");
		}
	} else {
		seq_puts(s, "Source PDOs:          <not configured>\n");
	}

	seq_puts(s, "\n");

	/* Sink Capabilities */
	nr_snk = fwnode_property_count_u32(fwnode, "sink-pdos");
	if (nr_snk > 0) {
		nr_snk = min(nr_snk, 7);
		fwnode_property_read_u32_array(fwnode, "sink-pdos", snk_pdo, nr_snk);

		seq_printf(s, "Sink PDOs:            %d PDO(s)\n", nr_snk);
		for (i = 0; i < nr_snk; i++) {
			u32 pdo = snk_pdo[i];
			u32 type = (pdo >> 30) & 0x3;

			seq_printf(s, "  PDO[%d]: 0x%08x  ", i, pdo);

			switch (type) {
			case 0: /* Fixed Supply */
			{
				u32 voltage_mv = ((pdo >> 10) & 0x3FF) * 50;
				u32 current_ma = (pdo & 0x3FF) * 10;
				seq_printf(s, "Fixed %umV @ %umA", voltage_mv, current_ma);
				if (i == 0)
					seq_puts(s, " (vSafe5V)");
				break;
			}
			case 1: /* Battery */
			{
				u32 max_v = ((pdo >> 20) & 0x3FF) * 50;
				u32 min_v = ((pdo >> 10) & 0x3FF) * 50;
				u32 power_mw = (pdo & 0x3FF) * 250;
				seq_printf(s, "Battery %u-%umV @ %umW", min_v, max_v, power_mw);
				break;
			}
			case 2: /* Variable Supply */
			{
				u32 max_v = ((pdo >> 20) & 0x3FF) * 50;
				u32 min_v = ((pdo >> 10) & 0x3FF) * 50;
				u32 current_ma = (pdo & 0x3FF) * 10;
				seq_printf(s, "Variable %u-%umV @ %umA", min_v, max_v, current_ma);
				break;
			}
			case 3: /* Augmented (PPS) */
			{
				u32 max_v = ((pdo >> 17) & 0xFF) * 100;
				u32 min_v = ((pdo >> 8) & 0xFF) * 100;
				u32 current_ma = (pdo & 0x7F) * 50;
				seq_printf(s, "PPS %u-%umV @ %umA", min_v, max_v, current_ma);
				break;
			}
			}
			seq_puts(s, "\n");
		}
	} else {
		seq_puts(s, "Sink PDOs:            <not configured>\n");
	}

	seq_puts(s, "\n");

	/* Sink VDOs (Discover Identity Response - PD 3.0) */
	{
		u32 snk_vdo[7];
		int nr_vdo = fwnode_property_count_u32(fwnode, "sink-vdos");

		if (nr_vdo > 0) {
			nr_vdo = min(nr_vdo, 7);
			fwnode_property_read_u32_array(fwnode, "sink-vdos", snk_vdo, nr_vdo);

			seq_printf(s, "Sink VDOs (PD 3.0):   %d VDO(s)\n", nr_vdo);
			for (i = 0; i < nr_vdo; i++) {
				seq_printf(s, "  VDO[%d]: 0x%08x", i, snk_vdo[i]);

				/* Decode common VDO types */
				if (i == 0) {
					/* ID Header VDO */
					u16 vid = (snk_vdo[i] >> 16) & 0xFFFF;
					u8 product_type = (snk_vdo[i] >> 27) & 0x7;
					u8 modal = (snk_vdo[i] >> 26) & 0x1;
					seq_printf(s, "  (ID Header: VID=0x%04x, ProductType=%u, Modal=%u)",
						   vid, product_type, modal);
				} else if (i == 1) {
					/* Cert Stat VDO */
					seq_printf(s, "  (Cert Stat: 0x%08x)", snk_vdo[i]);
				} else if (i == 2) {
					/* Product VDO */
					u16 pid = (snk_vdo[i] >> 16) & 0xFFFF;
					u16 bcd = snk_vdo[i] & 0xFFFF;
					seq_printf(s, "  (Product: PID=0x%04x, bcdDevice=0x%04x)",
						   pid, bcd);
				} else if (i == 3) {
					/* UFP VDO (PD 3.0) */
					seq_puts(s, "  (UFP VDO - PD 3.0)");
				}
				seq_puts(s, "\n");
			}
		} else {
			seq_puts(s, "Sink VDOs (PD 3.0):   <not configured>\n");
		}
	}

	seq_puts(s, "\n");

	/* Sink VDOs v1 (Discover Identity Response - PD 2.0) */
	{
		u32 snk_vdo_v1[7];
		int nr_vdo_v1 = fwnode_property_count_u32(fwnode, "sink-vdos-v1");

		if (nr_vdo_v1 > 0) {
			nr_vdo_v1 = min(nr_vdo_v1, 7);
			fwnode_property_read_u32_array(fwnode, "sink-vdos-v1", snk_vdo_v1, nr_vdo_v1);

			seq_printf(s, "Sink VDOs v1 (PD 2.0): %d VDO(s)\n", nr_vdo_v1);
			for (i = 0; i < nr_vdo_v1; i++) {
				seq_printf(s, "  VDO[%d]: 0x%08x", i, snk_vdo_v1[i]);

				/* Decode common VDO types */
				if (i == 0) {
					/* ID Header VDO */
					u16 vid = (snk_vdo_v1[i] >> 16) & 0xFFFF;
					u8 product_type = (snk_vdo_v1[i] >> 27) & 0x7;
					u8 modal = (snk_vdo_v1[i] >> 26) & 0x1;
					seq_printf(s, "  (ID Header: VID=0x%04x, ProductType=%u, Modal=%u)",
						   vid, product_type, modal);
				} else if (i == 1) {
					/* Cert Stat VDO */
					seq_printf(s, "  (Cert Stat: 0x%08x)", snk_vdo_v1[i]);
				} else if (i == 2) {
					/* Product VDO */
					u16 pid = (snk_vdo_v1[i] >> 16) & 0xFFFF;
					u16 bcd = snk_vdo_v1[i] & 0xFFFF;
					seq_printf(s, "  (Product: PID=0x%04x, bcdDevice=0x%04x)",
						   pid, bcd);
				}
				seq_puts(s, "\n");
			}
		} else {
			seq_puts(s, "Sink VDOs v1 (PD 2.0): <not configured>\n");
		}
	}

	seq_puts(s, "\n");

	/* Operating power */
	if (!fwnode_property_read_u32(fwnode, "op-sink-microwatt", &val))
		seq_printf(s, "Op Sink Power:        %u uW\n", val);

	/* PD revision */
	if (!fwnode_property_read_u32(fwnode, "pd-rev", &val))
		seq_printf(s, "PD Revision:          %u.0\n", val);

	seq_puts(s, "\n");

	/* Alternate Modes */
	{
		struct fwnode_handle *altmodes_node, *child;
		int altmode_count = 0;

		altmodes_node = fwnode_get_named_child_node(fwnode, "altmodes");
		if (altmodes_node) {
			seq_puts(s, "Alternate Modes:\n");

			fwnode_for_each_child_node(altmodes_node, child) {
				u16 svid;
				u32 vdo;
				const char *name = fwnode_get_name(child);

				if (!fwnode_property_read_u16(child, "svid", &svid) &&
				    !fwnode_property_read_u32(child, "vdo", &vdo)) {
					seq_printf(s, "  [%d] %s\n", altmode_count, name);
					seq_printf(s, "      SVID: 0x%04x", svid);
					if (svid == 0xFF01)
						seq_puts(s, " (DisplayPort)");
					else if (svid == 0x8087)
						seq_puts(s, " (Thunderbolt)");
					seq_puts(s, "\n");

					seq_printf(s, "      VDO:  0x%08x\n", vdo);

					/* DisplayPort VDO decode */
					if (svid == 0xFF01) {
						u8 port_cap = vdo & 0x3;
						u8 recept = (vdo >> 6) & 0x1;
						u8 ufp_pin = (vdo >> 16) & 0xFF;
						u8 dfp_pin = (vdo >> 8) & 0xFF;

						seq_printf(s, "            Port Cap: %s\n",
							   port_cap == 1 ? "UFP_D only" :
							   port_cap == 2 ? "DFP_D only" : "Both");
						seq_printf(s, "            Connector: %s\n",
							   recept ? "Receptacle" : "Plug");
						seq_printf(s, "            UFP_D Pins: 0x%02x\n", ufp_pin);
						seq_printf(s, "            DFP_D Pins: 0x%02x\n", dfp_pin);
					}
					altmode_count++;
				}
			}
			fwnode_handle_put(altmodes_node);

			if (altmode_count == 0)
				seq_puts(s, "  <none configured>\n");
		} else {
			seq_puts(s, "Alternate Modes:      <not configured>\n");
		}
	}

	seq_puts(s, "\n=======================================================================\n");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(connector_config);

/* Runtime Status */
static int status_show(struct seq_file *s, void *unused)
{
	struct rtk_pd *p = s->private;
	enum typec_cc_status cc1, cc2;
	u32 reg_val;
	int ret;

	seq_puts(s, "=======================================================================\n");
	seq_puts(s, "                 USB Type-C Runtime Status                            \n");
	seq_puts(s, "=======================================================================\n\n");

	mutex_lock(&p->lock);

	/* Connection Status */
	seq_puts(s, "--- Connection Status ---\n");
	ret = rtk_pd_get_cc_pair(p, &cc1, &cc2);
	if (ret == 0) {
		seq_printf(s, "CC1:                  %s (%d)\n",
			   get_cc_voltage_leve_str(cc1), cc1);
		seq_printf(s, "CC2:                  %s (%d)\n",
			   get_cc_voltage_leve_str(cc2), cc2);
	} else {
		seq_puts(s, "Failed to read CC status\n");
	}

	seq_printf(s, "Active Polarity:      %s\n",
		   p->polarity == TYPEC_POLARITY_CC1 ? "CC1" : "CC2");

	seq_printf(s, "VBUS Present:         %s\n",
		   p->vbus_present ? "Yes" : "No");

	seq_puts(s, "\n--- Roles ---\n");
	seq_printf(s, "Power Role:           %s\n",
		   p->power_role == TYPEC_SOURCE ? "Source (Rp)" : "Sink (Rd)");
	seq_printf(s, "Data Role:            %s\n",
		   p->data_role == TYPEC_HOST ? "Host (DFP)" : "Device (UFP)");

	seq_puts(s, "\n--- PD Status ---\n");
	seq_printf(s, "PD RX Enabled:        %s\n",
		   p->rx_enabled_by_tcpm ? "Yes" : "No");
	seq_printf(s, "TX In Flight:         %s\n",
		   p->tx_in_flight ? "Yes" : "No");
	seq_printf(s, "RX Message Pending:   %s\n",
		   p->rx_msg_has_pending ? "Yes" : "No");

	if (p->power_role == TYPEC_SOURCE && p->nr_src_pdo > 0) {
		seq_puts(s, "\n--- Source PDOs (Active) ---\n");
		seq_printf(s, "Number of PDOs:       %u\n", p->nr_src_pdo);
	}

	/* Requested voltage/current (if available) */
	if (p->src_pdo_requested) {
		seq_puts(s, "\n--- Negotiated Power (Source Mode) ---\n");
		seq_printf(s, "Voltage:              %u mV\n", p->src_req_voltage_mv);
		seq_printf(s, "Current:              %u mA\n", p->src_req_current_ma);
		seq_printf(s, "PDO Index:            %u\n", p->src_req_pdo_index);
	}

	/* CC raw register values */
	seq_puts(s, "\n--- CC Raw Register Values ---\n");
	reg_val = typec_readl(p->pd_regmap, USB_TYPEC_CTRL);
	seq_printf(s, "TYPEC_CTRL:           0x%08x\n", reg_val);
	reg_val = typec_readl(p->pd_regmap, USB_TYPEC_STS);
	seq_printf(s, "TYPEC_STS:            0x%08x\n", reg_val);

	/* GPIO Status */
	seq_puts(s, "\n--- GPIO Status ---\n");
	if (p->vbus_en_gpio)
		seq_printf(s, "VBUS EN:              %d\n",
			   gpiod_get_value(p->vbus_en_gpio));
	else
		seq_puts(s, "VBUS EN:              <not configured>\n");

	if (p->vconn1_en_gpio)
		seq_printf(s, "VCONN1 EN:            %d\n",
			   gpiod_get_value(p->vconn1_en_gpio));
	else
		seq_puts(s, "VCONN1 EN:            <not configured>\n");

	if (p->vconn2_en_gpio)
		seq_printf(s, "VCONN2 EN:            %d\n",
			   gpiod_get_value(p->vconn2_en_gpio));
	else
		seq_puts(s, "VCONN2 EN:            <not configured>\n");

	mutex_unlock(&p->lock);

	seq_puts(s, "\n=======================================================================\n");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(status);

/* Helper: decode and print a DP Status VDO */
static void connector_status_print_dp_status(struct seq_file *s, u32 vdo, const char *prefix)
{
	u8 conn    = vdo & 0x3;
	bool multi = !!(vdo & BIT(2));
	bool enabled = !!(vdo & BIT(3));
	bool hpd   = !!(vdo & BIT(7));
	bool irq   = !!(vdo & BIT(8));

	seq_printf(s, "%sDP Status VDO:      0x%08x\n", prefix, vdo);
	seq_printf(s, "%s  Connection:       %s\n", prefix,
		   conn == 0 ? "None" :
		   conn == 1 ? "DFP_D" :
		   conn == 2 ? "UFP_D" : "Both DFP_D+UFP_D");
	seq_printf(s, "%s  Multi-function:   %s\n", prefix, multi ? "Yes" : "No");
	seq_printf(s, "%s  DP Enabled:       %s\n", prefix, enabled ? "Yes" : "No");
	seq_printf(s, "%s  HPD:              %s\n", prefix, hpd ? "High" : "Low");
	seq_printf(s, "%s  IRQ:              %s\n", prefix, irq ? "Yes" : "No");
}

/* Helper: decode and print a DP Configure VDO */
static void connector_status_print_dp_configure(struct seq_file *s, u32 vdo, const char *prefix)
{
	u8 cfg = vdo & 0x3;
	u8 pin = (vdo >> 8) & 0xFF;
	u8 sig = (vdo >> 2) & 0xF;
	const char *pin_name;

	seq_printf(s, "%sDP Configure VDO:   0x%08x\n", prefix, vdo);
	seq_printf(s, "%s  Select:           %s\n", prefix,
		   cfg == 0 ? "USB (exit DP)" :
		   cfg == 1 ? "UFP_D (configure as UFP_D)" :
		   cfg == 2 ? "DFP_D (configure as DFP_D)" : "Reserved");
	switch (pin) {
	case 0x00: pin_name = "None"; break;
	case 0x04: pin_name = "C (4-lane)"; break;
	case 0x08: pin_name = "D (2-lane)"; break;
	case 0x10: pin_name = "E (4-lane)"; break;
	case 0x20: pin_name = "F (2-lane)"; break;
	default:   pin_name = "Unknown"; break;
	}
	seq_printf(s, "%s  Pin Assignment:   %s\n", prefix, pin_name);
	seq_printf(s, "%s  Signaling:        %s\n", prefix,
		   sig == 0 ? "None" :
		   sig == 1 ? "DP v1.3" :
		   sig == 2 ? "Gen2 (DP 2.0)" : "Unknown");
}

static int connector_status_show(struct seq_file *s, void *unused)
{
	struct rtk_pd *p = s->private;
	enum typec_cc_status cc1, cc2;
	bool is_dp = false;
	int ret;

	seq_puts(s, "=======================================================================\n");
	seq_puts(s, "                  USB Type-C Connector Status                         \n");
	seq_puts(s, "=======================================================================\n\n");

	mutex_lock(&p->lock);

	is_dp = (p->altmode_svid == 0xFF01 || p->dp_configure_rx || p->dp_status_rx
		 || p->dp_status_tx);

	/* ===== OUR PORT ===== */
	seq_puts(s, "=== Our Port ===\n");

	/* Connection */
	seq_printf(s, "Attached:             %s\n", p->is_attached ? "Yes" : "No");
	ret = rtk_pd_get_cc_pair(p, &cc1, &cc2);
	if (ret == 0) {
		seq_printf(s, "CC1:                  %s\n", get_cc_voltage_leve_str(cc1));
		seq_printf(s, "CC2:                  %s\n", get_cc_voltage_leve_str(cc2));
	}
	seq_printf(s, "Active Polarity:      %s\n",
		   p->polarity == TYPEC_POLARITY_CC1 ? "CC1" : "CC2");
	seq_printf(s, "VBUS Present:         %s\n", p->vbus_present ? "Yes" : "No");
	seq_printf(s, "VCONN:                %s\n", p->vconn_on ? "On (source)" : "Off");

	/* Roles */
	seq_puts(s, "\n");
	seq_printf(s, "Power Role:           %s\n",
		   p->power_role == TYPEC_SOURCE ? "Source" : "Sink");
	seq_printf(s, "Data Role:            %s\n",
		   p->data_role == TYPEC_HOST ? "Host (DFP)" : "Device (UFP)");

	/* Altmode */
	if (p->altmode_entered) {
		seq_printf(s, "Altmode:              0x%04x%s  Mode %u\n",
			   p->altmode_svid,
			   p->altmode_svid == 0xFF01 ? " (DisplayPort)" :
			   p->altmode_svid == 0x8087 ? " (Thunderbolt)" : "",
			   p->altmode_mode);
		if (p->altmode_svid == 0xFF01)
			seq_puts(s, "DP Role:              UFP_D (sink/receiver)\n");
	} else {
		seq_puts(s, "Altmode:              None\n");
	}

	/* PD Contract (as source) */
	if (p->src_pdo_requested) {
		seq_puts(s, "\n");
		seq_printf(s, "PD Contract:          %u mV / %u mA  (PDO %u)\n",
			   p->src_req_voltage_mv, p->src_req_current_ma,
			   p->src_req_pdo_index);
	}

	/* Our DP Status (what we sent to partner) */
	if (is_dp) {
		seq_puts(s, "\n");
		if (p->dp_status_tx)
			connector_status_print_dp_status(s, p->dp_status_tx, "Our  ");
		else
			seq_puts(s, "Our  DP Status VDO:   (not sent yet)\n");
	}

	/* ===== PARTNER ===== */
	seq_puts(s, "\n=== Partner ===\n");

	if (p->partner_vid) {
		seq_printf(s, "Vendor ID (VID):      0x%04x\n", p->partner_vid);
		seq_printf(s, "Product ID (PID):     0x%04x\n", p->partner_pid);
	} else if (p->data_role == TYPEC_DEVICE) {
		/* UFP: partner (DFP) initiates discovery; DFP sends REQ to us but
		 * never sends its own identity ACK to us, so VID is not available. */
		seq_puts(s, "Identity:             (UFP mode - partner VID not received)\n");
	} else {
		seq_puts(s, "Identity:             (not yet discovered)\n");
	}

	if (p->altmode_entered && p->altmode_svid == 0xFF01)
		seq_puts(s, "DP Role:              DFP_D (source/transmitter)\n");

	/* Partner's DP Status */
	if (is_dp) {
		seq_puts(s, "\n");
		if (p->dp_status_rx)
			connector_status_print_dp_status(s, p->dp_status_rx, "Ptnr ");
		else
			seq_puts(s, "Ptnr DP Status VDO:   (not received)\n");

		/* Partner's DP Configure (what partner asked us to do) */
		if (p->dp_configure_rx)
			connector_status_print_dp_configure(s, p->dp_configure_rx, "Ptnr ");
		else
			seq_puts(s, "Ptnr DP Configure VDO:(not received)\n");
	}

	mutex_unlock(&p->lock);

	seq_puts(s, "\n=======================================================================\n");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(connector_status);

/* ==================== PD Message Logger Functions ==================== */

/* Helper: Get message type name */
static const char *pd_msg_type_name(u16 header)
{
	u8 type = pd_header_type(header);
	bool is_data = pd_header_cnt(header) > 0;

	if (!is_data) {
		/* Control messages */
		switch (type) {
		case PD_CTRL_GOOD_CRC: return "GoodCRC";
		case PD_CTRL_GOTO_MIN: return "GotoMin";
		case PD_CTRL_ACCEPT: return "Accept";
		case PD_CTRL_REJECT: return "Reject";
		case PD_CTRL_PING: return "Ping";
		case PD_CTRL_PS_RDY: return "PS_RDY";
		case PD_CTRL_GET_SOURCE_CAP: return "Get_Source_Cap";
		case PD_CTRL_GET_SINK_CAP: return "Get_Sink_Cap";
		case PD_CTRL_DR_SWAP: return "DR_Swap";
		case PD_CTRL_PR_SWAP: return "PR_Swap";
		case PD_CTRL_VCONN_SWAP: return "VCONN_Swap";
		case PD_CTRL_WAIT: return "Wait";
		case PD_CTRL_SOFT_RESET: return "Soft_Reset";
		case PD_CTRL_NOT_SUPP: return "Not_Supported";
		case PD_CTRL_GET_SOURCE_CAP_EXT: return "Get_Src_Cap_Ext";
		case PD_CTRL_GET_STATUS: return "Get_Status";
		case PD_CTRL_FR_SWAP: return "FR_Swap";
		case PD_CTRL_GET_PPS_STATUS: return "Get_PPS_Status";
		case PD_CTRL_GET_COUNTRY_CODES: return "Get_Country_Codes";
		default: return "Control_Unknown";
		}
	} else {
		/* Data messages */
		switch (type) {
		case PD_DATA_SOURCE_CAP: return "Source_Capabilities";
		case PD_DATA_REQUEST: return "Request";
		case PD_DATA_BIST: return "BIST";
		case PD_DATA_SINK_CAP: return "Sink_Capabilities";
		case PD_DATA_VENDOR_DEF: return "Vendor_Defined";
		default: return "Data_Unknown";
		}
	}
}

/* Helper: Get VDM command name */
static const char *pd_vdm_cmd_name(u32 vdm_header)
{
	u8 cmd = PD_VDO_CMD(vdm_header);

	switch (cmd) {
	case CMD_DISCOVER_IDENT: return "Discover_Identity";
	case CMD_DISCOVER_SVID: return "Discover_SVID";
	case CMD_DISCOVER_MODES: return "Discover_Modes";
	case CMD_ENTER_MODE: return "Enter_Mode";
	case CMD_EXIT_MODE: return "Exit_Mode";
	case CMD_ATTENTION: return "Attention";
	case 0x10: return "DP_Status_Update";
	case 0x11: return "DP_Configure";
	default: return "VDM_Unknown";
	}
}

/* Helper: Get SOP type name */
static const char *pd_sop_type_name(enum tcpm_transmit_type sop)
{
	switch (sop) {
	case TCPC_TX_SOP: return "SOP";
	case TCPC_TX_SOP_PRIME: return "SOP'";
	case TCPC_TX_SOP_PRIME_PRIME: return "SOP''";
	default: return "SOP?";
	}
}

/* Add message to log */
static void pd_msg_log_add(struct pd_msg_logger *logger,
			    enum pd_msg_direction direction,
			    u16 header, const u32 *data, u8 count,
			    enum tcpm_transmit_type sop_type,
			    enum typec_role power_role,
			    enum typec_data_role data_role)
{
	struct pd_msg_log_entry *entry;
	unsigned long flags;
	int i;

	if (!logger)
		return;

	spin_lock_irqsave(&logger->lock, flags);

	/* Get current entry */
	entry = &logger->entries[logger->head];

	/* Fill entry */
	entry->timestamp = ktime_get();
	entry->direction = direction;
	entry->header = header;
	entry->count = min_t(u8, count, 7);
	entry->sop_type = sop_type;
	entry->power_role = power_role;
	entry->data_role = data_role;

	/* Copy data objects */
	for (i = 0; i < entry->count; i++)
		entry->data[i] = data ? data[i] : 0;

	/* Update circular buffer */
	logger->head = (logger->head + 1) % PD_LOG_BUFFER_SIZE;
	if (logger->count < PD_LOG_BUFFER_SIZE)
		logger->count++;

	spin_unlock_irqrestore(&logger->lock, flags);
}

/* Format DisplayPort Capability VDO */
static void pd_msg_format_dp_capability(struct seq_file *s, u32 vdo)
{
	u8 cap = vdo & 0x3;
	u8 receptacle = !!(vdo & BIT(6));
	u8 dfp_d_pin = (vdo >> 8) & 0xff;
	u8 ufp_d_pin = (vdo >> 16) & 0xff;

	seq_printf(s, " Cap:");
	if (cap & 0x1)
		seq_printf(s, "UFP_D");
	if (cap & 0x2)
		seq_printf(s, "%sDFP_D", (cap & 0x1) ? "+" : "");

	seq_printf(s, " %s", receptacle ? "Recpt" : "Plug");

	if (dfp_d_pin) {
		seq_printf(s, " DFP_pins=0x%02x", dfp_d_pin);
		seq_printf(s, "(");
		if (dfp_d_pin & BIT(2)) seq_printf(s, "C");
		if (dfp_d_pin & BIT(3)) seq_printf(s, "D");
		if (dfp_d_pin & BIT(4)) seq_printf(s, "E");
		if (dfp_d_pin & BIT(5)) seq_printf(s, "F");
		seq_printf(s, ")");
	}

	if (ufp_d_pin) {
		seq_printf(s, " UFP_pins=0x%02x", ufp_d_pin);
		seq_printf(s, "(");
		if (ufp_d_pin & BIT(2)) seq_printf(s, "C");
		if (ufp_d_pin & BIT(3)) seq_printf(s, "D");
		if (ufp_d_pin & BIT(4)) seq_printf(s, "E");
		if (ufp_d_pin & BIT(5)) seq_printf(s, "F");
		seq_printf(s, ")");
	}
}

/* Format DisplayPort Status VDO */
static void pd_msg_format_dp_status(struct seq_file *s, u32 vdo)
{
	u8 conn = vdo & 0x3;
	u8 enabled = !!(vdo & BIT(3));
	u8 hpd_state = !!(vdo & BIT(7));
	u8 hpd_irq = !!(vdo & BIT(8));

	seq_printf(s, " Conn:");
	switch (conn) {
	case 0: seq_printf(s, "None"); break;
	case 1: seq_printf(s, "DFP_D"); break;
	case 2: seq_printf(s, "UFP_D"); break;
	case 3: seq_printf(s, "Both"); break;
	}

	if (enabled)
		seq_printf(s, " Enabled");

	seq_printf(s, " HPD:%s", hpd_state ? "HIGH" : "LOW");

	if (hpd_irq)
		seq_printf(s, "+IRQ");
}

/* Format DisplayPort Configure VDO */
static void pd_msg_format_dp_configure(struct seq_file *s, u32 vdo)
{
	u8 cfg = vdo & 0x3;
	u8 pin = (vdo >> 8) & 0xff;
	u8 signaling = (vdo >> 2) & 0xf;
	const char *pin_name;

	seq_printf(s, " Cfg:");
	switch (cfg) {
	case 0: seq_printf(s, "USB"); break;
	case 1: seq_printf(s, "UFP_D"); break;
	case 2: seq_printf(s, "DFP_D"); break;
	default: seq_printf(s, "Rsvd"); break;
	}

	if (pin) {
		switch (pin) {
		case 0x04: pin_name = "C(4L)"; break;
		case 0x08: pin_name = "D(2L)"; break;
		case 0x10: pin_name = "E(4L)"; break;
		case 0x20: pin_name = "F(2L)"; break;
		default: pin_name = "??"; break;
		}
		seq_printf(s, " Pin=%s", pin_name);
	}

	if (signaling) {
		seq_printf(s, " Sig:");
		switch (signaling) {
		case 1: seq_printf(s, "DP1.3"); break;
		case 2: seq_printf(s, "Gen2"); break;
		default: seq_printf(s, "0x%x", signaling); break;
		}
	}
}

/* Format Identity Header VDO */
static void pd_msg_format_id_header(struct seq_file *s, u32 vdo)
{
	u16 vid = PD_IDH_VID(vdo);
	u8 modal = !!(vdo & BIT(26));
	u8 product_type = (vdo >> 27) & 0x7;
	const char *ptype_name;

	seq_printf(s, " VID=0x%04x", vid);

	switch (product_type) {
	case 0: ptype_name = "Undef"; break;
	case 1: ptype_name = "Hub"; break;
	case 2: ptype_name = "Periph"; break;
	case 3: ptype_name = "PassCbl"; break;
	case 4: ptype_name = "ActCbl"; break;
	case 5: ptype_name = "AMA"; break;
	default: ptype_name = "Rsvd"; break;
	}
	seq_printf(s, " PType=%s", ptype_name);

	if (modal)
		seq_printf(s, " Modal");
}

/* Format VDM details */
static void pd_msg_format_vdm(struct seq_file *s, const u32 *data, u8 count)
{
	u32 vdm_hdr;
	u16 svid;
	u8 cmd, cmd_type, opos, vdm_ver;
	int i;

	if (count < 1)
		return;

	vdm_hdr = data[0];
	svid = PD_VDO_VID(vdm_hdr);
	cmd = PD_VDO_CMD(vdm_hdr);
	cmd_type = PD_VDO_CMDT(vdm_hdr);
	opos = PD_VDO_OPOS(vdm_hdr);
	vdm_ver = PD_VDO_SVDM_VER(vdm_hdr);

	/* Line 1: Basic VDM info */
	seq_printf(s, "%s SVID=0x%04x", pd_vdm_cmd_name(vdm_hdr), svid);

	/* Command type */
	switch (cmd_type) {
	case CMDT_INIT: seq_printf(s, " REQ"); break;
	case CMDT_RSP_ACK: seq_printf(s, " ACK"); break;
	case CMDT_RSP_NAK: seq_printf(s, " NAK"); break;
	case CMDT_RSP_BUSY: seq_printf(s, " BUSY"); break;
	}

	seq_printf(s, " objs=%d", count);

	/* VDM version */
	if (vdm_ver)
		seq_printf(s, " Ver=%d.0", vdm_ver);

	seq_printf(s, "]");

	/* Line 2: Command-specific details (if any) */
	switch (cmd) {
	case CMD_DISCOVER_IDENT:
		if (cmd_type == CMDT_RSP_ACK && count >= 2) {
			seq_printf(s, "\n      VDM:");
			pd_msg_format_id_header(s, data[1]);

			/* Show additional VDOs */
			if (count >= 3) {
				u32 cert_stat = data[2];
				seq_printf(s, " XID=0x%08x", cert_stat);
			}
			if (count >= 4) {
				u32 product = data[3];
				seq_printf(s, " PID=0x%04x", (product >> 16) & 0xffff);
			}
		}
		break;

	case CMD_DISCOVER_MODES:
		if (cmd_type == CMDT_RSP_ACK && count >= 2) {
			seq_printf(s, "\n      VDM:");
			/* DisplayPort Alt Mode */
			if (svid == 0xff01) {
				pd_msg_format_dp_capability(s, data[1]);
			} else {
				/* Other Alt Modes - show VDO */
				for (i = 1; i < count; i++)
					seq_printf(s, " VDO%d=0x%08x", i, data[i]);
			}
		}
		break;

	case CMD_ENTER_MODE:
	case CMD_EXIT_MODE:
		if (opos) {
			seq_printf(s, "\n      VDM: Mode=%d", opos);
		}
		break;

	case 0x10: /* DP_Status_Update */
		if (count >= 2) {
			seq_printf(s, "\n      VDM:");
			pd_msg_format_dp_status(s, data[1]);
		}
		break;

	case 0x11: /* DP_Configure */
		if (count >= 2) {
			seq_printf(s, "\n      VDM:");
			pd_msg_format_dp_configure(s, data[1]);
		}
		break;

	case CMD_ATTENTION:
		seq_printf(s, "\n      VDM: Mode=%d", opos);
		/* For DP Attention, show status */
		if (svid == 0xff01 && count >= 2) {
			pd_msg_format_dp_status(s, data[1]);
		}
		break;
	}
}

/* Format PDO (Power Data Object) */
static void pd_msg_format_pdo(struct seq_file *s, u32 pdo)
{
	switch (pdo_type(pdo)) {
	case PDO_TYPE_FIXED:
		seq_printf(s, "%dmV/%dmA",
			   pdo_fixed_voltage(pdo),
			   pdo_max_current(pdo));
		break;
	case PDO_TYPE_BATT:
		seq_printf(s, "%d-%dmV/%dmW",
			   pdo_min_voltage(pdo),
			   pdo_max_voltage(pdo),
			   pdo_max_power(pdo));
		break;
	case PDO_TYPE_VAR:
		seq_printf(s, "%d-%dmV/%dmA",
			   pdo_min_voltage(pdo),
			   pdo_max_voltage(pdo),
			   pdo_max_current(pdo));
		break;
	case PDO_TYPE_APDO:
		seq_printf(s, "PPS");
		break;
	}
}

/* Format message data */
static void pd_msg_format_data(struct seq_file *s, u16 header,
				const u32 *data, u8 count)
{
	u8 type = pd_header_type(header);
	int i;

	if (count == 0)
		return;

	seq_printf(s, " [");

	switch (type) {
	case PD_DATA_SOURCE_CAP:
	case PD_DATA_SINK_CAP:
		/* Power capabilities */
		for (i = 0; i < count; i++) {
			if (i > 0)
				seq_printf(s, ", ");
			pd_msg_format_pdo(s, data[i]);
		}
		break;

	case PD_DATA_REQUEST:
		/* Power request */
		seq_printf(s, "Obj=%d ", (data[0] >> RDO_OBJ_POS_SHIFT) & RDO_OBJ_POS_MASK);
		seq_printf(s, "%dmA", ((data[0] >> RDO_FIXED_OP_CURR_SHIFT) & RDO_CURR_MASK) * 10);
		break;

	case PD_DATA_VENDOR_DEF:
		/* VDM */
		pd_msg_format_vdm(s, data, count);
		return;  /* VDM already adds ] */

	default:
		/* Raw data */
		for (i = 0; i < count; i++) {
			if (i > 0)
				seq_printf(s, " ");
			seq_printf(s, "0x%08x", data[i]);
		}
		break;
	}

	seq_printf(s, "]");
}

/* Show one log entry */
static void pd_msg_log_show_entry(struct seq_file *s,
				   const struct pd_msg_log_entry *entry,
				   bool show_raw_data)
{
	u64 ts_ms;
	const char *pr_str, *dr_str;
	u8 pd_rev;
	const char *pd_rev_str;

	/* Timestamp in format [seconds.milliseconds] */
	ts_ms = ktime_to_ms(entry->timestamp);

	/*
	 * Extract PD Revision from message header:
	 * Bits 7-6: Spec Revision
	 *   00 = PD 1.0
	 *   01 = PD 2.0
	 *   10 = PD 3.0
	 *   11 = Reserved (PD 3.1)
	 */
	pd_rev = (entry->header >> 6) & 0x3;
	switch (pd_rev) {
	case 0: pd_rev_str = "PD1.0"; break;
	case 1: pd_rev_str = "PD2.0"; break;
	case 2: pd_rev_str = "PD3.0"; break;
	case 3: pd_rev_str = "PD3.1"; break;
	default: pd_rev_str = "PD?.?"; break;
	}

	/*
	 * Get role strings:
	 * - For TX: Show our own roles (from entry->power_role/data_role)
	 * - For RX: Show partner's roles (from message header bits)
	 *
	 * PD Message Header format:
	 *   Bit 8: Power Role (0=Sink, 1=Source)
	 *   Bit 5: Data Role in PD 2.0 (0=UFP/Device, 1=DFP/Host)
	 */
	if (entry->direction == PD_MSG_TX) {
		/* TX: Show our own roles */
		pr_str = (entry->power_role == TYPEC_SOURCE) ? "SRC" : "SNK";
		dr_str = (entry->data_role == TYPEC_HOST) ? "HOST" : "DEV ";
	} else {
		/* RX: Extract partner's roles from message header */
		pr_str = (entry->header & BIT(8)) ? "SRC" : "SNK";
		dr_str = (entry->header & BIT(5)) ? "HOST" : "DEV ";
	}

	seq_printf(s, "[%5llu.%03u] %s %-4s %s %s/%s %-20s 0x%04x",
		   ts_ms / 1000, (u32)(ts_ms % 1000),
		   entry->direction == PD_MSG_TX ? "TX" : "RX",
		   pd_sop_type_name(entry->sop_type),
		   pd_rev_str,
		   pr_str, dr_str,
		   pd_msg_type_name(entry->header),
		   entry->header);

	/* Format data */
	pd_msg_format_data(s, entry->header, entry->data, entry->count);

	seq_printf(s, "\n");

	/* Show raw data if enabled */
	if (show_raw_data) {
		int i;
		seq_printf(s, "      RAW: header=0x%04x", entry->header);
		if (entry->count > 0) {
			seq_printf(s, " data=");
			for (i = 0; i < entry->count; i++) {
				if (i > 0)
					seq_printf(s, " ");
				seq_printf(s, "0x%08x", entry->data[i]);
			}
		}
		seq_printf(s, "\n");
	}
}

/* seq_file show function */
static int pd_msg_log_show(struct seq_file *s, void *unused)
{
	struct rtk_pd *pd = s->private;
	struct pd_msg_logger *logger = pd->msg_logger;
	unsigned long flags;
	unsigned int start, i, idx;

	if (!logger)
		return 0;

	seq_printf(s, "=== PD Message Log ===\n");
	seq_printf(s, "Format: [Time] DIR SOP  PR/DR   Message_Type     Header   [Data]\n");
	seq_printf(s, "        PR: SRC=Source SNK=Sink, DR: HOST=DFP DEV=UFP\n");
	seq_printf(s, "Buffer: %u/%u entries, Raw data: %s\n\n",
		   logger->count, PD_LOG_BUFFER_SIZE,
		   logger->show_raw_data ? "ON" : "OFF");

	spin_lock_irqsave(&logger->lock, flags);

	/* Calculate start position (show oldest first) */
	if (logger->count < PD_LOG_BUFFER_SIZE)
		start = 0;
	else
		start = logger->head;

	/* Show entries */
	for (i = 0; i < logger->count; i++) {
		idx = (start + i) % PD_LOG_BUFFER_SIZE;
		pd_msg_log_show_entry(s, &logger->entries[idx], logger->show_raw_data);
	}

	spin_unlock_irqrestore(&logger->lock, flags);

	return 0;
}

/* seq_file open function */
static int pd_msg_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, pd_msg_log_show, inode->i_private);
}

/* debugfs file operations */
static const struct file_operations pd_msg_log_fops = {
	.owner = THIS_MODULE,
	.open = pd_msg_log_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* Initialize PD message logger */
static int pd_msg_log_init(struct rtk_pd *pd)
{
	struct pd_msg_logger *logger;

	logger = devm_kzalloc(pd->dev, sizeof(*logger), GFP_KERNEL);
	if (!logger)
		return -ENOMEM;

	spin_lock_init(&logger->lock);
	logger->head = 0;
	logger->count = 0;
	logger->show_raw_data = false;

	pd->msg_logger = logger;

	/* Create debugfs files */
	if (pd->dentry) {
		debugfs_create_file("pd_messages", 0444, pd->dentry, pd,
				    &pd_msg_log_fops);
		debugfs_create_bool("pd_msg_show_raw", 0644, pd->dentry,
				    &logger->show_raw_data);
	}

	dev_info(pd->dev, "PD message logger initialized (buffer=%d entries)\n",
		 PD_LOG_BUFFER_SIZE);

	return 0;
}

/* Log TX message (called from pd_transmit) */
static inline void pd_msg_log_tx(struct rtk_pd *pd, u16 header,
				  const u32 *data, u8 count,
				  enum tcpm_transmit_type sop_type)
{
	if (pd->msg_logger)
		pd_msg_log_add(pd->msg_logger, PD_MSG_TX, header, data, count,
			       sop_type, pd->power_role, pd->data_role);
}

/* Log RX message (called from pd_receive) */
static inline void pd_msg_log_rx(struct rtk_pd *pd, u16 header,
				  const u32 *data, u8 count,
				  enum tcpm_transmit_type sop_type)
{
	if (pd->msg_logger)
		pd_msg_log_add(pd->msg_logger, PD_MSG_RX, header, data, count,
			       sop_type, pd->power_role, pd->data_role);
}

/* ==================== End PD Message Logger Functions ==================== */

static void tcpc_debugfs_init(struct rtk_pd *chip)
{
	char name[NAME_MAX];

	mutex_init(&chip->logbuffer_lock);
	snprintf(name, NAME_MAX, "tcpc-%s", dev_name(chip->dev));
	chip->dentry = debugfs_create_dir(name, usb_debug_root);
	debugfs_create_file("log", S_IFREG | 0444, chip->dentry, chip,
			    &tcpc_debug_fops);
	debugfs_create_file("pd_registers", S_IFREG | 0444, chip->dentry, chip,
			    &pd_registers_fops);
	debugfs_create_file("typec_registers", S_IFREG | 0444, chip->dentry, chip,
			    &typec_registers_fops);
	debugfs_create_file("pd_message_ram", S_IFREG | 0444, chip->dentry, chip,
			    &pd_message_ram_fops);

	/* Type-C Connector Status and Configuration */
	debugfs_create_file("connector_config", S_IFREG | 0444, chip->dentry, chip,
			    &connector_config_fops);
	debugfs_create_file("status", S_IFREG | 0444, chip->dentry, chip,
			    &status_fops);
	debugfs_create_file("connector_status", S_IFREG | 0444, chip->dentry, chip,
			    &connector_status_fops);

	/* Initialize PD message logger */
	pd_msg_log_init(chip);
}

static void tcpc_debugfs_exit(struct rtk_pd *chip)
{
	debugfs_remove_recursive(chip->dentry);
}

#else /* CONFIG_DEBUG_FS */
static inline void pd_msg_log_tx(struct rtk_pd *pd, u16 header,
				  const u32 *data, u8 count,
				  enum tcpm_transmit_type sop_type) { }

static inline void pd_msg_log_rx(struct rtk_pd *pd, u16 header,
				  const u32 *data, u8 count,
				  enum tcpm_transmit_type sop_type) { }

static void tcpc_debugfs_init(const struct rtk_pd *chip) { }
static void tcpc_debugfs_exit(const struct rtk_pd *chip) { }
#endif /* CONFIG_DEBUG_FS */

/*  regulators */
/* Set VBUS for source mode */
static int rtk_pd_regulators_set_vbus(struct rtk_pd *p, int voltage_mv, int current_ma)
{
	int voltage_uv = voltage_mv * 1000;
	int current_ua = current_ma * 1000;
	int ret;

	if (!p->vbus_reg) {
		dev_info(p->dev, "%s No vbus_reg\n", __func__);
		return 0;
	}

	/* Set minimum values if 0 requested (prepare for disable) */
	if (voltage_mv == 0) {
		voltage_uv = 2400000;  /* Min voltage: 2.4V */
		dev_dbg(p->dev, "Requested 0mV, setting to minimum %duV\n", voltage_uv);
	}
	if (current_ma == 0) {
		current_ua = 256000;  /* Min current: 0.256A */
		dev_dbg(p->dev, "Requested 0mA, setting to minimum %duA\n", current_ua);
	}

	/* Allow tolerance for regulator LSB quantization (±50mV) */
	ret = regulator_set_voltage(p->vbus_reg, voltage_uv, voltage_uv + 50000);
	if (ret < 0) {
		dev_err(p->dev, "Failed to set VBUS voltage: %d\n", ret);
		return ret;
	}

	ret = regulator_set_current_limit(p->vbus_reg, current_ua, current_ua);
	if (ret < 0) {
		dev_err(p->dev, "Failed to set current limit: %d\n", ret);
		return ret;
	}

	dev_info(p->dev, "Setting VBUS to %dmV current limit to %dmA\n",
		 voltage_uv / 1000, current_ua / 1000);

	return 0;
}

/* Enable VBUS output */
static int rtk_pd_regulators_enable_vbus(struct rtk_pd *p)
{
	int ret;
	int is_enabled;

	tc_log(p, "%s vbus_enabled=%d\n", __func__, p->vbus_enabled);

	if (p->vbus_enabled)
		return 0;

	if (!p->vbus_reg) {
		dev_info(p->dev, "%s No vbus_reg\n", __func__);
		return 0;
	}

	is_enabled = regulator_is_enabled(p->vbus_reg);
	if (is_enabled < 0) {
		dev_err(p->dev, "Failed to get VBUS state: %d\n",
			is_enabled);
		return is_enabled;
	}

	tc_log(p, "%s Enabling VBUS output (hw_state=%d)\n", __func__,
	       is_enabled);

	/*
	 * IMPORTANT: Set default voltage BEFORE enabling
	 *
	 * The regulator may still have the previous voltage setting (e.g., 2.4V minimum
	 * from last disconnect). We must set it to default 5V BEFORE enabling to ensure
	 * the partner device sees correct VBUS voltage immediately.
	 *
	 * If we enable first then set voltage, the partner may see wrong voltage and
	 * fail to respond to Source_Capabilities, causing PD negotiation failure.
	 */
	if (!is_enabled) {
		/* Set default 5V voltage before enabling (first PDO) */
		ret = rtk_pd_regulators_set_vbus(p, 5000, 3000);
		if (ret < 0) {
			dev_err(p->dev, "Failed to set default VBUS voltage: %d\n", ret);
			return ret;
		}
		tc_log(p, "%s Set default VBUS to 5V/3A before enabling\n", __func__);
	}

	if (is_enabled)
		return 0;

	ret = regulator_enable(p->vbus_reg);
	if (ret < 0) {
		dev_err(p->dev, "Failed to enable VBUS: %d\n", ret);
		return ret;
	}

	dev_dbg(p->dev, "%s Enabling VBUS output (hw_state=%d)\n", __func__,
		is_enabled);

	/* Delay for regulator to stabilize */
	usleep_range(2000, 3000);

	p->vbus_enabled = true;

	return 0;
}

/* Disable VBUS output */
static int rtk_pd_regulators_disable_vbus(struct rtk_pd *p)
{
	int ret;
	int is_enabled;

	tc_log(p, "%s vbus_enabled=%d\n", __func__, p->vbus_enabled);

	if (!p->vbus_enabled)
		return 0;

	if (!p->vbus_reg) {
		dev_info(p->dev, "%s No vbus_reg\n", __func__);
		return 0;
	}

	is_enabled = regulator_is_enabled(p->vbus_reg);
	if (is_enabled < 0) {
		dev_err(p->dev, "Failed to get VBUS state: %d\n",
			is_enabled);
		return is_enabled;
	}

	tc_log(p, "%s Disabling VBUS output (hw_state=%d)\n", __func__,
		is_enabled);

	if (!is_enabled)
		return 0;

	ret = regulator_disable(p->vbus_reg);
	if (ret < 0) {
		dev_err(p->dev, "Failed to disable VBUS: %d\n", ret);
		return ret;
	}

	dev_dbg(p->dev, "%s Disabling VBUS output (hw_state=%d)\n", __func__,
		is_enabled);

	/* Delay for regulator to settle */
	usleep_range(2000, 3000);

	p->vbus_enabled = false;

	return 0;
}

__maybe_unused
static int rtk_pd_regulators_set_otg(struct rtk_pd *p, int voltage_mv, int current_ma)
{
	int voltage_uv = voltage_mv * 1000;
	int current_ua = current_ma * 1000;
	int ret;

	if (!p->otg_reg) {
		dev_info(p->dev, "%s No otg_reg\n", __func__);
		return 0;
	}

	/* Set minimum values if 0 requested (prepare for disable) */
	if (voltage_mv == 0) {
		voltage_uv = 5004000;  /* Min OTG voltage: 5.004V */
		dev_info(p->dev, "Requested 0mV, setting to minimum %duV\n", voltage_uv);
	}
	if (current_ma == 0) {
		current_ua = 256000;  /* Min current: 0.256A */
		dev_info(p->dev, "Requested 0mA, setting to minimum %duA\n", current_ua);
	}

	/* Set voltage - Allow tolerance for regulator LSB quantization (+/- 50mV) */
	ret = regulator_set_voltage(p->otg_reg, voltage_uv, voltage_uv + 50000);
	if (ret < 0) {
		dev_err(p->dev, "Failed to set OTG voltage: %d\n", ret);
		return ret;
	}

	/* Set current limit */
	ret = regulator_set_current_limit(p->otg_reg, current_ua, current_ua);
	if (ret < 0) {
		dev_err(p->dev, "Failed to set OTG current: %d\n", ret);
		return ret;
	}

	tc_log(p, "Enabling OTG: %dmV %dmA\n", voltage_uv / 1000, current_ua / 1000);

	return 0;
}

/* Enable OTG mode */
static int rtk_pd_regulators_enable_otg(struct rtk_pd *p)
{
	int ret, is_enabled;

	tc_log(p, "%s otg_enabled=%d\n", __func__, p->otg_enabled);

	if (p->otg_enabled)
		return 0;

	if (!p->otg_reg) {
		dev_info(p->dev, "%s No otg_reg\n", __func__);
		return 0;
	}

	is_enabled = regulator_is_enabled(p->otg_reg);
	if (is_enabled < 0) {
		dev_err(p->dev, "Failed to get VBUS state: %d\n",
			is_enabled);
		return is_enabled;
	}

	tc_log(p, "%s Enabling OTG (hw_state=%d)\n", __func__, is_enabled);

	if (is_enabled)
		return 0;

	/* Enable OTG */
	ret = regulator_enable(p->otg_reg);
	if (ret < 0) {
		dev_err(p->dev, "Failed to enable OTG: %d\n", ret);
		return ret;
	}

	dev_info(p->dev, "%s Enabling OTG (hw_state=%d)\n", __func__,
		is_enabled);

	/* Delay for regulator to stabilize */
	usleep_range(2000, 3000);

	p->otg_enabled = true;

	return 0;
}

/* Disable OTG mode */
static int rtk_pd_regulators_disable_otg(struct rtk_pd *p)
{
	int ret;
	int is_enabled;

	tc_log(p, "%s otg_enabled=%d\n", __func__, p->otg_enabled);

	if (!p->otg_enabled)
		return 0;

	if (!p->otg_reg) {
		dev_info(p->dev, "%s No otg_reg\n", __func__);
		return 0;
	}

	is_enabled = regulator_is_enabled(p->otg_reg);
	if (is_enabled < 0) {
		dev_err(p->dev, "Failed to get VBUS state: %d\n",
			is_enabled);
		return is_enabled;
	}

	tc_log(p, "%s Disabling OTG (hw_state=%d)\n", __func__, is_enabled);

	if (!is_enabled)
		return 0;

	ret = regulator_disable(p->otg_reg);
	if (ret < 0) {
		dev_err(p->dev, "Failed to disable OTG: %d\n", ret);
		return ret;
	}

	dev_info(p->dev, "%s Disabling OTG (hw_state=%d)\n", __func__, is_enabled);
	/* Delay for regulator to settle */
	usleep_range(2000, 3000);

	p->otg_enabled = false;

	return 0;
}

static int rtk_pd_regulators_init(struct rtk_pd *p)
{
	struct device *dev = p->dev;
	int ret;

	/* Get VBUS output regulator */
	p->vbus_reg = devm_regulator_get_optional(dev, "vbus");
	if (IS_ERR(p->vbus_reg)) {
		ret = PTR_ERR(p->vbus_reg);
		if (ret == -EPROBE_DEFER) {
			dev_info(dev, "VBUS regulator not ready, deferring probe\n");
			p->vbus_reg = NULL;
			return ret;
		} else if (ret == -ENODEV) {
			dev_info(dev, "VBUS regulator not found, continuing without it\n");
			p->vbus_reg = NULL;
		} else {
			dev_err(dev, "Failed to get VBUS regulator: %d\n", ret);
			return ret;
		}
	}

	/* Get OTG regulator */
	p->otg_reg = devm_regulator_get_optional(dev, "otg");
	if (IS_ERR(p->otg_reg)) {
		ret = PTR_ERR(p->otg_reg);
		if (ret == -EPROBE_DEFER) {
			dev_info(dev, "OTG regulator not ready, deferring probe\n");
			p->vbus_reg = NULL;
			return ret;
		} else if (ret == -ENODEV) {
			dev_info(dev, "OTG regulator not found, continuing without it\n");
			p->otg_reg = NULL;
		} else {
			dev_err(dev, "Failed to get OTG regulator: %d\n", ret);
			return ret;
		}
	}

	rtk_pd_regulators_disable_vbus(p);
	rtk_pd_regulators_disable_otg(p);

	return 0;
}

/*---------------------------------------- */
/* return typec_cc_status
   enum typec_cc_status {
	TYPEC_CC_OPEN,
	TYPEC_CC_RA,
	TYPEC_CC_RD,
	TYPEC_CC_RP_DEF,
	TYPEC_CC_RP_1_5,
	TYPEC_CC_RP_3_0,
  };
 */
static enum typec_cc_status decode_cc_status(struct rtk_pd *p, u32 cc_raw)
{
	struct cc_cfg *cc_cfg = p->cc_cfg;

	if (cc_cfg->cc_role == TYPEC_SOURCE) {
		if ((cc_raw & CC_DET_STS_MASK) == DET_STS_RA)
			return TYPEC_CC_RA;
		else if ((cc_raw & CC_DET_STS_MASK) == DET_STS_RD)
			return TYPEC_CC_RD;
		else
			return TYPEC_CC_OPEN;
	} else if (cc_cfg->cc_role == TYPEC_SINK) {
		if ((cc_raw & CC_DET_STS_MASK) == DET_STS_RP)
			return TYPEC_CC_RP_DEF;
		else if ((cc_raw & CC_DET_STS_MASK) == DET_STS_RP15)
			return TYPEC_CC_RP_1_5;
		else if ((cc_raw & CC_DET_STS_MASK) == DET_STS_RP30)
			return TYPEC_CC_RP_3_0;
		else
			return TYPEC_CC_OPEN;
	} else {
		dev_err(p->dev, "%s unknown cc_role=%d cc_raw=%x\n",
			__func__, cc_cfg->cc_role, cc_raw);
		return TYPEC_CC_OPEN;
	}
}

static const char * const cc_voltage_level_strings[] = {
	[TYPEC_CC_OPEN]    = "VOPEN (>2.6V)",
	[TYPEC_CC_RA]      = "VRA (0.2-0.4V)",
	[TYPEC_CC_RD]      = "VRD (0.25-2.6V)",
	[TYPEC_CC_RP_DEF]  = "VRP_DEF (0.85-1.25V)",
	[TYPEC_CC_RP_1_5]  = "VRP_1_5 (1.31-1.70V)",
	[TYPEC_CC_RP_3_0]  = "VRP_3_0 (1.76-2.04V)",
};

static const char *get_cc_voltage_leve_str(enum typec_cc_status cc_level)
{
	if (cc_level >= 0 && cc_level < ARRAY_SIZE(cc_voltage_level_strings) &&
		cc_voltage_level_strings[cc_level])
		return cc_voltage_level_strings[cc_level];

	return "Unknown CC voltage level";
}

/* CC detection with debouncing */
static void rtk_typec_get_cc_status(struct rtk_pd *p,
		enum typec_cc_status *cc1, enum typec_cc_status *cc2)
{
	struct cc_cfg *cc_cfg;
	u32 cc_status, cc1_status, cc2_status;

	cc_cfg = p->cc_cfg;

	cc_status = typec_cc_sts_get(p->pd_regmap);
	cc_cfg->cc_status = cc_status;

	cc1_status = (cc_status >> CC1_DET_STS_SHIFT) & CC_DET_STS_MASK;
	cc2_status = (cc_status >> CC2_DET_STS_SHIFT) & CC_DET_STS_MASK;
	*cc1 = decode_cc_status(p, cc1_status);
	*cc2 = decode_cc_status(p, cc2_status);

	cc_log(p, "%s:\n", __func__);
	cc_log(p, "    cc_role=%s, cc_setting=%s (%d) --> cc1 %s (cc_status=0x%x)\n",
	       cc_cfg->cc_role == TYPEC_SOURCE ? "SOURCE" : "SINK",
	       get_cc_voltage_leve_str(cc_cfg->cc_setting),
	       cc_cfg->cc_setting,
	       get_cc_voltage_leve_str(cc_cfg->cc1_level), cc_status);
	cc_log(p, "    cc_role=%s, cc_setting=%s (%d) --> cc2 %s (cc_status=0x%x)\n",
	       cc_cfg->cc_role == TYPEC_SOURCE ? "SOURCE" : "SINK",
	       get_cc_voltage_leve_str(cc_cfg->cc_setting),
	       cc_cfg->cc_setting,
	       get_cc_voltage_leve_str(cc_cfg->cc2_level), cc_status);
}

/* Configure CC lines for detection */
static int rtk_typec_set_cc_detection(struct rtk_pd *p, enum typec_cc_status cc)
{
	struct cc_cfg *cc_cfg = p->cc_cfg;
	struct pd_regmap *regmap = p->pd_regmap;
	enum typec_role new_role;
	u32 cc_mode;
	bool role_changed;
	bool optimized_rp_switch = false;

	switch (cc) {
	case TYPEC_CC_RP_DEF:
		cc_mode = dfp_mode(CC_MODE_DFP_USB) | EN_RP36K;
		new_role = TYPEC_SOURCE;
		break;
	case TYPEC_CC_RP_1_5:
		cc_mode = dfp_mode(CC_MODE_DFP_1_5) | EN_RP12K;
		new_role = TYPEC_SOURCE;
		break;
	case TYPEC_CC_RP_3_0:
		cc_mode = dfp_mode(CC_MODE_DFP_3_0) | EN_RP4P7K;
		new_role = TYPEC_SOURCE;
		break;
	case TYPEC_CC_RD:
		cc_mode = EN_RD;
		new_role = TYPEC_SINK;
		break;
	case TYPEC_CC_OPEN:
		/* Disconnect/Open mode - disable CC pull-up/pull-down.
		 * Used during error recovery (ERROR_RECOVERY/PORT_RESET).
		 * Set cc_mode to 0 to disable all CC terminations.
		 */
		cc_log(p, "%s: Setting CC to OPEN (disconnect mode)\n", __func__);
		cc_mode = 0;
		new_role = TYPEC_SINK;  /* Default to Sink when open */
		break;
	case TYPEC_CC_RA:
		/* Audio Accessory mode - not currently supported by hardware.
		 * Would require setting both CC lines to Ra (Audio Adapter Accessory).
		 */
		dev_warn(p->dev, "%s: TYPEC_CC_RA (Audio Accessory) not supported\n",
			 __func__);
		return -EOPNOTSUPP;
	default:
		dev_err(p->dev, "%s: Invalid CC mode %d\n", __func__, cc);
		return -EINVAL;
	}

	/* Detect if this is just an Rp value change (Collision Avoidance).
	 * Optimize for SINK_TX_OK/SINK_TX_NG switching during AMS operations.
	 * This is a critical path for PD 3.0 performance.
	 */
	role_changed = (new_role != cc_cfg->cc_role);

	if (!role_changed && p->is_attached && new_role == TYPEC_SOURCE &&
	    (cc == TYPEC_CC_RP_DEF || cc == TYPEC_CC_RP_1_5 || cc == TYPEC_CC_RP_3_0)) {
		/* Optimized path: Just switching between Rp values (DEF/1.5A/3.0A)
		 * for Collision Avoidance (SINK_TX_OK/SINK_TX_NG). Only use this
		 * path when already attached, because Collision Avoidance only
		 * occurs during PD negotiation. When unattached (SRC_UNATTACHED,
		 * PORT_RESET), always do a full reconfiguration so the CC cache is
		 * reset to OPEN — this guarantees a cache delta when the cable is
		 * detected, ensuring tcpm_cc_change() is called even if the cable
		 * was already connected at probe time.
		 */
		optimized_rp_switch = true;
		cc_log(p, "%s: Fast Rp switch: %s -> %s (Collision Avoidance)\n",
		       __func__,
		       get_cc_voltage_leve_str(cc_cfg->cc_setting),
		       get_cc_voltage_leve_str(cc));
	}

	cc_cfg->cc_setting_prev = cc_cfg->cc_setting;
	cc_cfg->cc_setting = cc;
	cc_cfg->cc_role = new_role;

	/* When switching to SOURCE mode, clear the vbus_present cache.
	 * A stale vbus_present=true from a previous SINK session would cause
	 * the cc_check_timer's vbus_delayed_work to call tcpm_vbus_change with
	 * vbus=present, triggering _tcpm_pd_vbus_on while still in
	 * SRC_ATTACH_WAIT, setting vbus_vsafe0v=false and permanently blocking
	 * the SRC_ATTACH_WAIT -> SRC_ATTACHED transition.
	 */
	if (role_changed && new_role == TYPEC_SOURCE)
		p->vbus_present = false;

	if (optimized_rp_switch) {
		/* Fast path: Only update CC mode register without disabling detection.
		 * This maintains connection stability during AMS operations.
		 */
		typec_cc_mode_set(regmap, cc_mode);

		/* Brief delay for Rp change to propagate to CC lines */
		usleep_range(50, 100);

		cc_log(p, "Fast Rp switch complete: cc_status=0x%x\n",
		       typec_cc_sts_get(regmap));

		/* Return 1 to indicate optimized path was taken.
		 * Caller (rtk_pd_set_cc) should not reset cc_level to OPEN.
		 */
		return 1;
	} else {
		/* Full reconfiguration path: Role change or transition to/from OPEN.
		 * Disable CC detection first for clean transition.
		 */
		cc_log(p, "%s: Full CC reconfiguration: role_changed=%d, %s -> %s\n",
		       __func__, role_changed,
		       get_cc_voltage_leve_str(cc_cfg->cc_setting_prev),
		       get_cc_voltage_leve_str(cc));

		/* Disable CC detection first */
		typec_cc_det_disable(regmap);

		/* Wait for stabilization */
		usleep_range(100, 200);

		/* Configure CC */
		typec_cc_mode_set(regmap, cc_mode);

		/* Remove PLR_EN */
		typec_cc_plr_disable(regmap);

		/* Enable CC detection */
		typec_cc_det_enable(regmap);

		/* Wait for detection to stabilize */
		usleep_range(500, 1000);

		cc_log(p, "Full CC reconfiguration complete: cc_status=0x%x\n",
		       typec_cc_sts_get(regmap));
		cc_log(p, "CC ctrl: typec_ctrl=0x%08x typec_ctrl1=0x%08x\n",
		       typec_ctrl_get(regmap), typec_ctrl1_get(regmap));
		cc_log(p, "CC1 ctrl=0x%08x vref=0x%08x\n",
		       typec_cc1_ctrl_get(regmap), typec_cc1_vref_get(regmap));
		cc_log(p, "CC2 ctrl=0x%08x vref=0x%08x\n",
		       typec_cc2_ctrl_get(regmap), typec_cc2_vref_get(regmap));
	}

	return 0;
}

static int rtk_typec_set_int_enable(struct rtk_pd *p)
{
	struct pd_regmap *regmap = p->pd_regmap;

	typec_int_enable(regmap);

	return 0;
}

static int rtk_typec_set_int_disable(struct rtk_pd *p)
{
	struct pd_regmap *regmap = p->pd_regmap;

	typec_int_disable(regmap);

	return 0;
}

static int rtk_typec_init(struct rtk_pd *p)
{
	struct device *dev = p->dev;
	struct cc_cfg *cc_cfg = p->cc_cfg;

	dev_info(dev, "type c init \n");

	typec_ctrl1_enable(p->pd_regmap);

	/* set parameter */
	typec_cc1_code_set(p->pd_regmap, cc_cfg->cc1_code);
	typec_cc1_vref_set(p->pd_regmap, cc_cfg->cc1_vref);

	typec_cc2_code_set(p->pd_regmap, cc_cfg->cc2_code);
	typec_cc2_vref_set(p->pd_regmap, cc_cfg->cc2_vref);

	typec_int_debounce_set(p->pd_regmap, cc_cfg->debounce);

	/* Enable interrupts */
	typec_int_enable(p->pd_regmap);

	return 0;
}

static int rtk_typec_setup_parameter(struct rtk_pd *p)
{
	struct cc_cfg *cc_cfg = p->cc_cfg;
	struct cc_param *cc_param;

	tc_log(p, "%s: setup parameter \n", __func__);

	cc_cfg->ufp_mode_rd_en = EN_RD;

	cc_param = &cc_cfg->cc1_param;
	cc_cfg->cc1_code = rp4pk_code(cc_param->rp_4p7k_code) |
			   rp36k_code(cc_param->rp_36k_code) |
			   rp12k_code(cc_param->rp_12k_code) |
			   rd_code(cc_param->rd_code);

	if (cc_cfg->parameter_ver == PARAMETER_V0)
		cc_cfg->cc1_vref = V0_vref_2p6v(cc_param->vref_2p6v) |
				   V0_vref_1p23v(cc_param->vref_1p23v) |
				   V0_vref_0p8v(cc_param->vref_0p8v) |
				   V0_vref_0p66v(cc_param->vref_0p66v) |
				   V0_vref_0p4v(cc_param->vref_0p4v) |
				   V0_vref_0p2v(cc_param->vref_0p2v) |
				   V0_vref_1_1p6v(cc_param->vref_1_1p6v) |
				   V0_vref_0_1p6v(cc_param->vref_0_1p6v);
	else if (cc_cfg->parameter_ver == PARAMETER_V1)
		cc_cfg->cc1_vref = V1_vref_2p6v(cc_param->vref_2p6v) |
				   V1_vref_1p23v(cc_param->vref_1p23v) |
				   V1_vref_0p8v(cc_param->vref_0p8v) |
				   V1_vref_0p66v(cc_param->vref_0p66v) |
				   V1_vref_0p4v(cc_param->vref_0p4v) |
				   V1_vref_0p2v(cc_param->vref_0p2v) |
				   V1_vref_1_1p6v(cc_param->vref_1_1p6v) |
				   V1_vref_0_1p6v(cc_param->vref_0_1p6v);
	else
		dev_err(p->dev, "%s: unknown parameter_ver %d\n",
			__func__, cc_cfg->parameter_ver);

	cc_param = &cc_cfg->cc2_param;
	cc_cfg->cc2_code = rp4pk_code(cc_param->rp_4p7k_code)
			 | rp36k_code(cc_param->rp_36k_code)
			 | rp12k_code(cc_param->rp_12k_code)
			 | rd_code(cc_param->rd_code);

	if (cc_cfg->parameter_ver == PARAMETER_V0)
		cc_cfg->cc2_vref = V0_vref_2p6v(cc_param->vref_2p6v) |
				   V0_vref_1p23v(cc_param->vref_1p23v) |
				   V0_vref_0p8v(cc_param->vref_0p8v) |
				   V0_vref_0p66v(cc_param->vref_0p66v) |
				   V0_vref_0p4v(cc_param->vref_0p4v) |
				   V0_vref_0p2v(cc_param->vref_0p2v) |
				   V0_vref_1_1p6v(cc_param->vref_1_1p6v) |
				   V0_vref_0_1p6v(cc_param->vref_0_1p6v);
	else if (cc_cfg->parameter_ver == PARAMETER_V1)
		cc_cfg->cc2_vref = V1_vref_2p6v(cc_param->vref_2p6v) |
				   V1_vref_1p23v(cc_param->vref_1p23v) |
				   V1_vref_0p8v(cc_param->vref_0p8v) |
				   V1_vref_0p66v(cc_param->vref_0p66v) |
				   V1_vref_0p4v(cc_param->vref_0p4v) |
				   V1_vref_0p2v(cc_param->vref_0p2v) |
				   V1_vref_1_1p6v(cc_param->vref_1_1p6v) |
				   V1_vref_0_1p6v(cc_param->vref_0_1p6v);
	else
		dev_err(p->dev, "%s: unknown parameter_ver %d\n",
			__func__, cc_cfg->parameter_ver);

	cc_cfg->debounce = (cc_cfg->debounce_val << 1) | DEBOUNCE_EN;

	return 0;
}

static int rtk_pd_read_rx_message(struct rtk_pd *p,
				  struct pd_message *msg)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;
	int rx_len = RX_BYTE_CNT(pd_regmap->base);
	struct pd_message *m = msg;
	int i, max = min_t(int, rx_len, sizeof(struct pd_message));

	/* Log RX interrupt with state information */
	tc_log(p, "%s RX_OK interrupt: Receiving %d bytes\n",
	       __func__, rx_len);
	pd_log(p, "%s State: tx_busy=%d rx_en=%d\n",
	       __func__, p->tx_in_flight, p->rx_enabled_by_tcpm);

	/* DEBUG: Dump all PD registers for debugging */
	if (p->raw_data_enable) {
		pd_log(p, "%s: [RX REG DUMP] Getting %d bytes from PD_RAM RX\n", __func__, rx_len);
		rtk_pd_dump_registers(p, NULL, "[RX REG DUMP] ");
		rtk_pd_dump_message_ram(p, NULL, false, "[RX MSG] ");
	}

	/* Check for abnormal conditions */
	if (rx_len == 0) {
		dev_warn(p->dev, "%s: WARNING: RX_OK but rx_len=0! Possible HW issue.\n",
			__func__);
		dev_warn(p->dev, "%s: Check if RX was disabled during reception.\n",
			__func__);
	}

	/* Read RAM -> m (header+payload) and pass to tcpm */
	for (i = 0; i < max; i++) {
		((u8 *)m)[i] = readb(pd_regmap->ram_rx + i);
		pd_log(p, "%s: PD_RAM RX i=%d data=%x\n", __func__, i, ((u8 *)m)[i]);
	}

	return rx_len;
}

static int rtk_pd_write_tx_message(struct rtk_pd *p,
				    const struct pd_message *msg)
{
	const u8 *raw = (const u8 *)msg;
	int i, j, aligned_len;
	int len;

	/* TX length = header(2) + payload(4*nr_objs) */
	len = 2 + (pd_header_cnt_le(msg->header) * 4);

	/* Debug: dump source data before writing */
	pd_log(p, "%s: [SOURCE DATA] Writing %d bytes to PD_RAM_TX\n", __func__, len);
	pd_log(p, "%s: [SOURCE DATA] raw[0-3]=0x%02x 0x%02x 0x%02x 0x%02x\n",
		__func__, raw[0], raw[1], raw[2], raw[3]);
	if (len > 4)
		pd_log(p, "%s: [SOURCE DATA] raw[4-7]=0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__, raw[4], raw[5], raw[6], raw[7]);

	/* Write to RAM using 4-byte aligned access */
	/* Hardware may require 4-byte aligned writes to RAM */
	aligned_len = (len + 3) & ~3;  /* Round up to 4-byte boundary */

	for (i = 0; i < aligned_len; i += 4) {
		u32 word = 0;
		/* Pack bytes into 32-bit word (little endian) */
		for (j = 0; j < 4 && (i + j) < len; j++)
			word |= ((u32)raw[i + j]) << (j * 8);

		/* Write 4 bytes at once */
		writel(word, p->pd_regmap->ram_tx + i);
		pd_log(p, "%s: [WRITE] offset=%d, word=0x%08x\n",
			__func__, i, word);
	}

	/* Memory barrier to ensure writes complete */
	wmb();

	return len;
}

static void rtk_pd_aphy_polarity_set(struct rtk_pd *p, enum typec_cc_polarity pol)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;

	/* Enable CC PHY based on which CC line is active */
	pd_aphycc_cc1_enable(pd_regmap, false);
	pd_aphycc_cc2_enable(pd_regmap, false);
	typec_cc_pd_disable(pd_regmap);

	if (pol == TYPEC_POLARITY_CC2) {
		/* CC2 is active - enable CC2 PHY only */
		typec_cc_pd_enable_cc2(pd_regmap);
		pd_aphycc_cc2_enable(pd_regmap, true);
		pd_log(p, "%s Enabled CC2 APHY and Channel\n", __func__);
	} else {
		/* CC1 is active - enable CC1 PHY only */
		typec_cc_pd_enable_cc1(pd_regmap);
		pd_aphycc_cc1_enable(pd_regmap, true);
		pd_log(p, "%s Enabled CC1 APHY Channel\n", __func__);
	}
}

static void rtk_pd_program_goodcrc_roles(struct rtk_pd *p)
{
	enum typec_role pr;
	enum typec_data_role dr;

	if (!p->pd_regmap)
		return;

	pr = p->power_role;
	dr = p->data_role;

	if (p->pending_data_role != p->data_role)
		dr = p->pending_data_role;
	if (p->pending_power_role != p->power_role)
		pr = p->pending_power_role;

	if (p->swap_roles_pending) {
		pr = p->power_role;
		dr = p->data_role;
	}

	pd_txctrl_tx_msg_power_role(p->pd_regmap, pr == TYPEC_SOURCE);
	pd_txctrl_tx_msg_data_role(p->pd_regmap, dr == TYPEC_HOST);
	pd_txctrl_tx_msg_spec_rev(p->pd_regmap, 0x1);  /* PD 2.0 */
	pd_log(p, "%s: GoodCRC role programmed to PR=%s DR=%s\n", __func__,
	       pr == TYPEC_SOURCE ? "SRC" : "SNK",
	       dr == TYPEC_HOST ? "HOST (DFP)" : "DEVICE (UFP)");
}

static void rtk_pd_enable_auto_goodcrc_resp(struct rtk_pd *p)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;

	/* Enable automatic GoodCRC response (hardware requirement: <30us) */
	pd_txctrl_sop_resp_crc_enable(pd_regmap, true);   /* Auto GoodCRC for SOP */
	pd_txctrl_auto_resp_good_crc_enable(pd_regmap, true);  /* Global enable */

	pd_txctrl_tx_crc_enable(pd_regmap, true);
	/* Set GoodCRC header fields based on our role
	 * These values will be used in the auto-generated GoodCRC
	 */
	rtk_pd_program_goodcrc_roles(p);

	pd_log(p, "%s: Enabled auto GoodCRC response\n", __func__);
}

static void rtk_pd_disable_txrx_digital(struct rtk_pd *p)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;

	tc_log(p, "%s\n", __func__);
	pd_basic_rx_enable(pd_regmap, false);
	pd_basic_tx_enable(pd_regmap, false);
}

static void rtk_pd_enable_tx(struct rtk_pd *p)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;

	tc_log(p, "%s\n", __func__);
	/* Disable APHY TX auto-enable for manual control */
	pd_txctrl_tx_ana_auto_enable(pd_regmap, false);

	/* Enable CC PHY based on active line - read from hardware */
	rtk_pd_aphy_polarity_set(p, p->polarity);

	/* Enable APHY TX globally (required for RL6859 architecture)
	 * Reference: HW_PD_tx_enable() calls both PD_tx_en and Aphy_Tx_En
	 */
	pd_aphyctrl_aphy_rx_enable(pd_regmap, true);
	pd_aphyctrl_aphy_tx_enable(pd_regmap, true);

	pd_txctrl_auto_resp_good_crc_enable(pd_regmap, false);
	pd_rxctrl_auto_set_rx_en(pd_regmap, true);

	pd_basic_tx_enable(pd_regmap, true);
}

static void rtk_pd_disable_tx(struct rtk_pd *p)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;

	tc_log(p, "%s\n", __func__);
	pd_rxctrl_auto_set_rx_en(pd_regmap, false);
	pd_txctrl_auto_resp_good_crc_enable(pd_regmap, false);

	pd_aphyctrl_aphy_tx_enable(pd_regmap, false);

	pd_basic_tx_enable(pd_regmap, false);

	/* CRITICAL: Clear any pending TX_OK interrupt immediately after disabling TX
	 * to prevent spurious interrupts.
	 *
	 * Race condition without this fix:
	 * 1. TX completes, TX_OK interrupt fires
	 * 2. Handler processes TX, receives GoodCRC
	 * 3. Call rtk_pd_disable_tx() here, set tx_in_flight=false
	 * 4. Hardware latches another TX_OK between step 3 and interrupt clear
	 * 5. Handler clears interrupts at END (line 3507)
	 * 6. Latched TX_OK fires with tx_in_flight=false -> spurious warning
	 *
	 * Fix: Clear TX_OK interrupt RIGHT AFTER disabling TX to prevent
	 * any latched/pending TX_OK from firing later.
	 */
	pd_int_sts_clear(pd_regmap, PD_INT_TX_OK);
	tc_log(p, "%s: Cleared TX_OK interrupt to prevent spurious triggers\n", __func__);
}

static void rtk_pd_enable_rx(struct rtk_pd *p)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;

	tc_log(p, "%s\n", __func__);
	/* Enable CC PHY based on active line - read from hardware */
	rtk_pd_aphy_polarity_set(p, p->polarity);

	pd_aphyctrl_aphy_rx_enable(p->pd_regmap, true);
	pd_aphyctrl_aphy_tx_enable(p->pd_regmap, true);

	pd_rxctrl_auto_set_rx_en(pd_regmap, false);
	rtk_pd_enable_auto_goodcrc_resp(p);

	pd_basic_rx_reset(pd_regmap, true);
	pd_basic_rx_reset(pd_regmap, false);

	pd_basic_rx_enable(p->pd_regmap, true);
}

static void rtk_pd_disable_rx(struct rtk_pd *p)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;

	tc_log(p, "%s\n", __func__);
	pd_rxctrl_auto_set_rx_en(pd_regmap, false);
	pd_txctrl_auto_resp_good_crc_enable(pd_regmap, false);

	pd_basic_rx_reset(pd_regmap, true);
	pd_basic_fsm_reset(pd_regmap, true);

	pd_aphyctrl_aphy_rx_enable(p->pd_regmap, false);
	pd_basic_rx_enable(p->pd_regmap, false);

	pd_basic_fsm_reset(pd_regmap, false);
	pd_basic_rx_reset(pd_regmap, false);
}

/*
 * rtk_pd_rx_level_set
 * Description: Configure BMC PHY RX voltage threshold based on Power role.
 *
 * Sink mode (TYPEC_SINK):
 *   - Enable sink threshold: detect VBUS from Source (lower voltage)
 *   - Disable source threshold: not needed in sink mode
 *
 * Source mode (TYPEC_SOURCE):
 *   - Disable sink threshold: not needed in source mode
 *   - Enable source threshold: detect own VBUS output (higher voltage)
 */
static void rtk_pd_rx_level_set(struct rtk_pd *p, enum typec_role role)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;

	if (role == TYPEC_SINK) {
		/* Sink mode: detect VBUS from Source */
		pd_aphyctrl_en_sink(pd_regmap, true);
		pd_aphyctrl_en_source(pd_regmap, true);
	} else {  /* TYPEC_SOURCE */
		/* Source mode: detect own VBUS output */
		pd_aphyctrl_en_sink(pd_regmap, false);
		pd_aphyctrl_en_source(pd_regmap, false);
	}
}

/* Init hw layer */
static void rtk_pd_init(struct rtk_pd *p)
{
	struct pd_regmap *pd_regmap;

	dev_info(p->dev, "pd init \n");

	pd_regmap = p->pd_regmap;

	/* APHY holdb enable */
	pd_aphyctrl_reg_holdb(pd_regmap, true);

	/* Disable unwanted interrupts, but keep VBUS_MON for Sink mode */
	pd_basic_intr_disable(pd_regmap, PD_INT_EN_RX_CRV_1ST | PD_INT_EN_CC_FRS);

	if (pd_aphycc_cc1_channel_chk(pd_regmap) || pd_aphycc_cc2_channel_chk(pd_regmap)) {
		/* disable local power detection */
		pd_aphyctrl_reg_pow_frs(pd_regmap, false);

		/* disable cc power */
		pd_aphycc_cc1_pow_det(pd_regmap, false);
		pd_aphycc_cc2_pow_det(pd_regmap, false);

		msleep(20);
	}

	pd_aphycc_cc1_channel_enable(pd_regmap, true);
	pd_aphycc_cc2_channel_enable(pd_regmap, true);
	pd_aphycc_cc1_pow_det(pd_regmap, true);
	pd_aphycc_cc2_pow_det(pd_regmap, true);

	pd_aphycc_cc1_en_cc_det_2_1(pd_regmap, true);
	pd_aphycc_cc1_cc_ref_0p2v_sel(pd_regmap, true);
	pd_aphycc_cc2_en_cc_det_2_1(pd_regmap, true);
	pd_aphycc_cc2_cc_ref_0p2v_sel(pd_regmap, true);

	/* TypeC spec PD debounce min 12ms (0x493E0) */
	pd_debounce_cc_det_debounce_value_32(pd_regmap, 0x493E0);

	pd_txctrl_tx_start_addr(pd_regmap, (PD_TX_START_ADDR / 4));    /* 4 byte alignment */

	pd_debounce_cc_det_debounce_value(pd_regmap, 0xFA);  /* 10us(FA) */
	//pd_cc_det_debounce_value(pd_regmap, 0x30d4);  /* 500us(30D4) 800us(4E20) 1ms(61A8) */

	pd_ocp_prtect2shut_cnt(pd_regmap, 0x4E2);           /* 50ms */

	pd_txctrl_tx_bmc_end_cnt_val(pd_regmap, 0x28);        /* 2us after tDriver_BMC */

	rtk_pd_rx_level_set(p, p->power_role);

	pd_debounce_loc_mon_debounce_cnt(pd_regmap, 0x1388);       /* 120us */
	pd_debounce_frs_det_debounce_cnt(pd_regmap, 0x5DC);        /* 3.76us(5E) 0x60us(0x5DC) */
	pd_debounce_vbus_mon_debounce_cnt(pd_regmap, 0x1388);      /* 120us for VBUS detection */

	pd_txctrl_tx_data_sent_period(pd_regmap, 0x2b);   /* 0x27 304k; 0x29 290k; 0x2b 27MHz */

	pd_rxctrl_rcv_eop_int_en(pd_regmap, false);  /* Interrupt CPU if bmc timeout */
	pd_rxctrl_orderset_cmp_enable(pd_regmap, false); /* close orderset cmp if not match
                                                 interrupt will be ignore */
	pd_rxctrl_hard_rst_mask(pd_regmap, false);  /* Enable Hard Reset ordered set recognition */
	pd_rxctrl_bmc_timeout_cnt(pd_regmap, 0xB0);  /* bmc time out set 7us (0xb0) */

	/*  If USB not connection done, pd reinit will disable reg_pow_frs/prs
	    before fw restart without UPS reset.
	    In this case ,power interrupt still trigger */
	pd_int_sts_clear(pd_regmap, (PD_INT_LOC_MON | PD_INT_VBUS_MON));

	pd_aphyctrl_reg_pow_frs(pd_regmap, true);
	pd_aphyctrl_reg_pow_prs(pd_regmap, true);
	pd_aphyctrl_reg_prs_tune(pd_regmap, 0x08);
	pd_debounce_rx_debounce_value(pd_regmap, 0x13);

	pd_rx_smt_enable(pd_regmap, true);

	//pd_aphyctrl_reg_vh_sink(pd_regmap, 0x5);
	//pd_aphyctrl_reg_vl_sink(pd_regmap, 0x5);

	/* 8. Enable APHY TX SECOND (analog PHY TX)
	 * Required for RL6859 architecture (sample code pd.c:1130-1131)
	 */
	pd_aphyctrl_aphy_rx_enable(pd_regmap, true);
	pd_aphyctrl_aphy_tx_enable(pd_regmap, false);

	/* Allow PD PHY to stabilize after initialization */
	msleep(10);

	/* Enable automatic GoodCRC response (hardware requirement: <30us) */
	pd_txctrl_sop_resp_crc_enable(pd_regmap, true);   /* Auto GoodCRC for SOP */
	pd_txctrl_auto_resp_good_crc_enable(pd_regmap, true);  /* Global enable */
	dev_dbg(p->dev, "%s: Enabled auto GoodCRC response\n", __func__);

	rtk_typec_init(p);

	/* Enable PD interrupts for TX and RX
	 * CRITICAL: All PD protocol interrupts must be enabled here!
	 * Two steps required for each interrupt:
	 * 1. Enable in BASIC_CTRL (PD_INT_EN_*)
	 * 2. Unmask in PD_INT (PD_INT_MSK_*)
	 */
	pd_basic_intr_enable(pd_regmap, PD_INT_EN_RX | PD_INT_EN_RX_CRV_1ST |
	                                 PD_INT_EN_TX_OK | PD_INT_EN_TX_GC);
	pd_int_unmask(pd_regmap, PD_INT_MSK_RX_OK | PD_INT_MSK_RX_CRV_1ST |
	                         PD_INT_MSK_TX_OK | PD_INT_MSK_TX_GC);
	dev_dbg(p->dev, "%s: Enabled PD interrupts (RX_OK, TX_OK, TX_GC)\n", __func__);

	/* Enable VBUS monitoring for Sink mode */
	pd_int_unmask(pd_regmap, PD_INT_MSK_VBUS_MON);
	dev_dbg(p->dev, "%s: Enabled VBUS monitoring\n", __func__);

	pd_int_unmask(pd_regmap, PD_INT_MSK_CC1_DET);
	pd_int_unmask(pd_regmap, PD_INT_MSK_CC2_DET);

	/* Stop timers (Timer1/2) */
	pd_timer1_set(pd_regmap, 0);
	pd_timer2_set(pd_regmap, 0);

	/* Reset FSM / TX / RX */
	pd_basic_ctrl_set(pd_regmap,  BASIC_FSM_RST | BASIC_TX_RST | BASIC_RX_RST);
	udelay(10);
	pd_basic_ctrl_clr(pd_regmap, BASIC_FSM_RST | BASIC_TX_RST | BASIC_RX_RST);

	/* Clear pending interrupts (W1C) */
	pd_int_sts_clear(pd_regmap, 0xFFFF);

	pd_basic_rx_reset(pd_regmap, true);
	msleep(10);
	pd_basic_rx_reset(pd_regmap, false);

	writel(0xffffffff, pd_regmap->ram_tx);
	writel(0xffffffff, pd_regmap->ram_rx);

	/* Enable RX/TX again */
	pd_basic_rx_enable(pd_regmap, false);
	pd_basic_tx_enable(pd_regmap, false);
	pd_rxctrl_auto_set_rx_en_cnt(pd_regmap, 0x3);//0x1C2);

	/* DEBUG */
	pd_rxctrl_bmc_preamble_timeout_en(pd_regmap, false);
}

/* CC ready callback */
__maybe_unused
static void rtk_pd_cc_ready(struct rtk_pd *p)
{
	cc_log(p, "%s: cc_ready\n", __func__);
	tcpm_cc_change(p->tcpm);
}

/* -------------------- Minimal VBUS/CC sensing -------------------- */
/* vbus_present */
static int rtk_pd_get_vbus(struct rtk_pd *p, bool *present)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;
	u32 vbus_status;

	/* Source mode: Return cached value; Sink mode: Read hardware */
	if (p->cc_cfg->cc_role == TYPEC_SOURCE) {
		/* Source mode: trust our cached state from set_vbus() */
		*present = p->vbus_present;
		tc_log(p, "%s: [SOURCE] vbus=%s (cached)\n",
		       __func__, *present ? "present" : "absent");
	} else {
		/* Sink mode: detect VBUS from Source via hardware */
		vbus_status = pd_debounce_vbus_debounce_out(pd_regmap);
		*present = p->vbus_present;
		tc_log(p, "%s: [SINK] vbus=%s (hw=0x%x)\n",
		       __func__, *present ? "present" : "absent", vbus_status);
	}
	return 0;
}

/* Check if VBUS is at vSafe0V (<0.8V)
 * This is separate from get_vbus() which checks for VBUS presence (~4V threshold)
 */
static bool rtk_pd_is_vbus_vsafe0v(struct rtk_pd *p)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;
	u32 vbus_status;
	bool vsafe0v;

	/* Read hardware VBUS detection
	 * If VBUS detection returns 0, VBUS is below threshold (~4V)
	 * This implies VBUS is at vSafe0V (<0.8V) for Source detach safety
	 */
	vbus_status = pd_debounce_vbus_debounce_out(pd_regmap);
	vsafe0v = (vbus_status == 0);

	tc_log(p, "%s: vsafe0v=%s (hw_status=0x%x)\n",
		__func__, vsafe0v ? "YES" : "NO", vbus_status);

	vsafe0v = true;

	return vsafe0v;
}

static int rtk_pd_set_vbus(struct rtk_pd *p, bool on, bool sink)
{
	tc_log(p, "%s: set_vbus on=%d sink=%d (current role=%s)\n",
	       __func__, on, sink, p->power_role == TYPEC_SOURCE ? "SOURCE" : "SINK");

	if (on) { /* Source mode on */
		tc_log(p, "%s: Enabling VBUS (Source mode)\n", __func__);
		rtk_pd_regulators_disable_otg(p);
		rtk_pd_regulators_enable_vbus(p);
		p->vbus_present = true;
		tc_log(p, "%s: VBUS enabled, vbus_present=%d\n",
		       __func__, p->vbus_present);
	} else if (sink) { /* Sink mode on */
		tc_log(p, "%s: Enabling OTG (Sink mode)\n", __func__);
		rtk_pd_regulators_disable_vbus(p);
		rtk_pd_regulators_enable_otg(p);
		//p->vbus_present = false; /* Sink doesn't provide VBUS */
		p->vbus_present = true; /* Sink doesn't provide VBUS */
		tc_log(p, "%s: OTG enabled, vbus_present=%d\n",
		       __func__, p->vbus_present);
	} else { /* All off */
		tc_log(p, "%s: Disabling VBUS and OTG\n", __func__);
		rtk_pd_regulators_disable_vbus(p);
		rtk_pd_regulators_disable_otg(p);
		p->vbus_present = false;
		tc_log(p, "%s: VBUS/OTG disabled, vbus_present=%d\n",
		       __func__, p->vbus_present);
	}

	/* if using external GPIO control VBUS switch */
	if (p->vbus_en_gpio)
		gpiod_direction_output(p->vbus_en_gpio, on);

	/* Implementation depends on hardware design */
#if 0
	if (enable && !type_c->vbus_present) {
		/* Turn on VBUS within tVBUSON (295ms) */
		type_c->vbus_present = true;
		dev_dbg(type_c->dev, "VBUS enabled\n");
	} else if (!enable && type_c->vbus_present) {
		/* Turn off VBUS within tVBUSOFF (650ms) */
		type_c->vbus_present = false;
		dev_dbg(type_c->dev, "VBUS disabled\n");
	}
#endif

	/* Notify TCPM of VBUS change
	 * - VBUS ON: Immediate notification (0ms) - TCPM needs to know ASAP
	 * - VBUS OFF: Delayed notification (50ms) - Wait for VBUS to discharge to vSafe0V
	 *   USB PD spec requires tVBUSOFF max 650ms, but typical discharge is much faster.
	 *   50ms delay ensures is_vbus_vsafe0v() reads hardware correctly.
	 */
	schedule_delayed_work(&p->vbus_delayed_work, msecs_to_jiffies(on ? 0 : 50));
	tc_log(p, "%s: Scheduled VBUS notification in %dms\n",
		__func__, on ? 0 : 50);

	return 0;
}

static int rtk_pd_updated_cc_status(struct rtk_pd *p)
{
	struct cc_cfg *cc_cfg;
	int stable_count = 0;
	int max_attempts = 10;
	enum typec_cc_status cc1_prev, cc2_prev;
	enum typec_cc_status cc1_new, cc2_new;

	cc_cfg = p->cc_cfg;
	cc1_prev = cc_cfg->cc1_level;
	cc2_prev = cc_cfg->cc2_level;

	/* Read initial CC status */
	rtk_typec_get_cc_status(p, &cc1_new, &cc2_new);

	/* Require 3 consecutive stable readings to filter out electrical noise.
	 * This prevents spurious CC change notifications due to transient signals.
	 */
	while (stable_count < 3 && max_attempts-- > 0) {
		if (cc1_new == cc_cfg->cc1_level && cc2_new == cc_cfg->cc2_level) {
			stable_count++;
		} else {
			/* Values changed, restart stability check */
			cc_cfg->cc1_level = cc1_new;
			cc_cfg->cc2_level = cc2_new;
			stable_count = 1;
		}

		if (stable_count < 3) {
			udelay(200);  /* Wait before next reading */
			rtk_typec_get_cc_status(p, &cc1_new, &cc2_new);
		}
	}

	/* If we couldn't get stable readings after max_attempts, use last reading */
	if (stable_count < 3) {
		cc_log(p, "%s: CC not stable after %d attempts, using last reading\n",
		       __func__, 10 - max_attempts);
		cc_cfg->cc1_level = cc1_new;
		cc_cfg->cc2_level = cc2_new;
	}

	/* When VCONN is enabled, the VCONN pin voltage ramps from Ra (~0.3V) to
	 * 5V (OPEN). During ramp-up, it transiently reads as Rd due to Ra/Rd
	 * voltage range overlap (Ra: 0.2-0.4V, Rd: 0.25-0.61V). These transient
	 * readings would contaminate the cache and cause TCPM to incorrectly flip
	 * polarity when it reads CC via get_cc(). Prevent cache contamination by
	 * restoring the VCONN pin's cached value to what it was before VCONN was
	 * enabled. The active CC pin is still updated normally.
	 */
	if (p->vconn_on) {
		if (p->polarity == TYPEC_POLARITY_CC1) {
			/* CC2 is the VCONN pin */
			if (cc_cfg->cc2_level != cc2_prev) {
				cc_log(p, "%s: VCONN on, suppress CC2 cache update (%s -> %s kept as %s)\n",
				       __func__,
				       get_cc_voltage_leve_str(cc2_prev),
				       get_cc_voltage_leve_str(cc_cfg->cc2_level),
				       get_cc_voltage_leve_str(cc2_prev));
				cc_cfg->cc2_level = cc2_prev;
			}
		} else {
			/* CC1 is the VCONN pin */
			if (cc_cfg->cc1_level != cc1_prev) {
				cc_log(p, "%s: VCONN on, suppress CC1 cache update (%s -> %s kept as %s)\n",
				       __func__,
				       get_cc_voltage_leve_str(cc1_prev),
				       get_cc_voltage_leve_str(cc_cfg->cc1_level),
				       get_cc_voltage_leve_str(cc1_prev));
				cc_cfg->cc1_level = cc1_prev;
			}
		}
	}

	/* After an Rp level switch (Collision Avoidance: VRP_3_0 to VRP_1_5), the
	 * detection threshold changes. With VRP_1_5 (12k Rp), a 5.1k Rd cable
	 * produces ~0.62V on the active CC pin - just above the hardware's VRD
	 * detection range (0.25–0.61V) — so the hardware briefly reads it as OPEN.
	 * Suppress active-CC OPEN readings within 50ms of the last Rp switch to
	 * prevent a false disconnect from being reported to TCPM.
	 */
	if (p->vconn_on && p->is_attached &&
	    ktime_to_ms(ktime_sub(ktime_get(), p->last_rp_switch_ktime)) < 50) {
		if (p->polarity == TYPEC_POLARITY_CC2 &&
		    cc_cfg->cc2_level == TYPEC_CC_OPEN && cc2_prev != TYPEC_CC_OPEN) {
			cc_log(p, "%s: Rp settling, suppress CC2 OPEN (%lldms after Rp switch)\n",
			       __func__,
			       ktime_to_ms(ktime_sub(ktime_get(), p->last_rp_switch_ktime)));
			cc_cfg->cc2_level = cc2_prev;
		} else if (p->polarity == TYPEC_POLARITY_CC1 &&
			   cc_cfg->cc1_level == TYPEC_CC_OPEN && cc1_prev != TYPEC_CC_OPEN) {
			cc_log(p, "%s: Rp settling, suppress CC1 OPEN (%lldms after Rp switch)\n",
			       __func__,
			       ktime_to_ms(ktime_sub(ktime_get(), p->last_rp_switch_ktime)));
			cc_cfg->cc1_level = cc1_prev;
		}
	}

	cc_cfg->cc1_level_prev = cc1_prev;
	cc_cfg->cc2_level_prev = cc2_prev;

	/* Check if CC status actually changed compared to previous reading */
	if ((cc_cfg->cc1_level != cc1_prev) || (cc_cfg->cc2_level != cc2_prev)) {
		cc_log(p, "cc_level change detected\n");
		cc_log(p, "%s\n", __func__);
		cc_log(p, "    cc_setting %s: cc1 %s -> %s\n",
		       get_cc_voltage_leve_str(cc_cfg->cc_setting),
		       get_cc_voltage_leve_str(cc1_prev),
		       get_cc_voltage_leve_str(cc_cfg->cc1_level));
		cc_log(p, "    cc_setting %s: cc2 %s -> %s\n",
		       get_cc_voltage_leve_str(cc_cfg->cc_setting),
		       get_cc_voltage_leve_str(cc2_prev),
		       get_cc_voltage_leve_str(cc_cfg->cc2_level));
		return 1;  /* CC status changed */
	}

	return 0;  /* No change */
}

static int rtk_pd_get_cc_pair(struct rtk_pd *p,
			      enum typec_cc_status *cc1,
			      enum typec_cc_status *cc2)
{
	struct cc_cfg *cc_cfg;

	cc_cfg = p->cc_cfg;

	*cc1 = cc_cfg->cc1_level;
	*cc2 = cc_cfg->cc2_level;

	cc_log(p, "%s: cc_role=%s cc_setting %s ==> cc1_level %s\n",
	       __func__,
	       cc_cfg->cc_role == TYPEC_SOURCE ? "SOURCE" : "SINK",
	       get_cc_voltage_leve_str(cc_cfg->cc_setting),
	       get_cc_voltage_leve_str(cc_cfg->cc1_level));
	cc_log(p, "%s: cc_role=%s cc_setting %s ==> cc2_level %s\n",
	       __func__,
	       cc_cfg->cc_role == TYPEC_SOURCE ? "SOURCE" : "SINK",
	       get_cc_voltage_leve_str(cc_cfg->cc_setting),
	       get_cc_voltage_leve_str(cc_cfg->cc2_level));

	return 0;
}

static int rtk_pd_set_cc(struct rtk_pd *p, enum typec_cc_status cc)
{
	struct cc_cfg *cc_cfg;
	int ret;

	cc_cfg = p->cc_cfg;

	hrtimer_cancel(&p->cc_check_timer);
	hrtimer_cancel(&p->tx_delay_timer);
	cancel_work_sync(&p->tx_delay_work);

	/* If there's a pending TX, notify TCPM of failure before clearing.
	 * This prevents message loss during CC transitions.
	 */
	if (p->tx_pending) {
		pd_log(p, "%s: Aborting pending TX due to CC change (cc=%s)\n",
		       __func__, get_cc_voltage_leve_str(cc));
		p->tx_pending = false;
		/* Notify TCPM that transmission failed */
		tcpm_pd_transmit_complete(p->tcpm, TCPC_TX_FAILED);
	}

	rtk_typec_set_int_disable(p);
	ret = rtk_typec_set_cc_detection(p, cc);

	/* Save previous CC levels for change detection */
	cc_cfg->cc1_level_prev = cc_cfg->cc1_level;
	cc_cfg->cc2_level_prev = cc_cfg->cc2_level;

	/* If optimized Rp switch was used (ret == 1), CC status is still valid.
	 * Don't reset to OPEN - hardware is already stable and connection maintained.
	 * Record timestamp so rtk_pd_updated_cc_status() can suppress false OPEN
	 * readings that occur while the new Rp detection threshold is settling.
	 * For full reconfiguration (ret == 0), reset CC levels to OPEN.
	 * Timer callback will read actual hardware values after stabilization.
	 */
	if (ret == 1)
		p->last_rp_switch_ktime = ktime_get();

	if (ret != 1) {
		/* Full reconfiguration: reset CC levels, will be updated by timer */
		cc_cfg->cc1_level = TYPEC_CC_OPEN;
		cc_cfg->cc2_level = TYPEC_CC_OPEN;
	}

	/* Start timer to check CC status after hardware stabilizes.
	 * Don't immediately call rtk_pd_cc_ready() to avoid:
	 * 1. Recursive calls (TCPM set_cc -> driver calls tcpm_cc_change -> TCPM set_cc again)
	 * 2. Reading unstable CC values
	 * 3. Interrupting TCPM's state machine during set_cc execution
	 *
	 * Use adaptive timer period based on connection state:
	 * - Attached: Slow polling (100ms) - connection is stable, save power
	 * - Unattached: Medium polling (50ms) - waiting for device, balance power and responsiveness
	 * - Default: Fast polling (10ms) - during connection/disconnection transitions
	 */
	int check_period = tCCCheckAttaching;  /* Default: fast polling */

	if (p->is_attached) {
		check_period = tCCCheckAttached;  /* Slow polling when connected */
	} else if (cc_cfg->cc1_level == TYPEC_CC_OPEN && cc_cfg->cc2_level == TYPEC_CC_OPEN) {
		check_period = tCCCheckUnattached;  /* Medium polling when clearly unattached */
	}

	cc_log(p, "%s: Starting CC check timer with period %dms (attached=%d)\n",
	       __func__, check_period, p->is_attached);

	hrtimer_start(&p->cc_check_timer, ms_to_ktime(check_period), HRTIMER_MODE_REL);

	return 0;
}

static enum hrtimer_restart rtk_pd_cc_check_timer_callback(struct hrtimer *timer)
{
	struct rtk_pd *p =
		container_of(timer, struct rtk_pd, cc_check_timer);
	struct cc_cfg *cc_cfg;
	enum typec_cc_status cc1, cc2;

	cc_cfg = p->cc_cfg;
	cc_log(p, "CC check timer callback\n");

	if (rtk_pd_updated_cc_status(p)) {
		/* Clear any pending RX message - device disconnected/reconnected */
		if (p->is_attached && p->rx_msg_has_pending) {
			pd_log(p, "%s: Clearing pending RX message due to CC change\n", __func__);
			p->rx_msg_has_pending = false;
			p->rx_msg_drop_pending = false;
			p->rx_msg_length = 0;
			cancel_delayed_work(&p->rx_msg_timeout_work);
			if (p->rx_enabled_by_tcpm && !p->tx_in_flight)
				rtk_pd_enable_rx(p);
		}

		cc_log(p, "%s: CC changed -> tcpm_cc_change (cc1=%s cc2=%s)\n",
		       __func__,
		       get_cc_voltage_leve_str(cc_cfg->cc1_level),
		       get_cc_voltage_leve_str(cc_cfg->cc2_level));
		tcpm_cc_change(p->tcpm);
	}

	if (p->is_attached)
		goto out;

	cc1 = cc_cfg->cc1_level;
	cc2 = cc_cfg->cc2_level;

	if (cc_cfg->cc_role == TYPEC_SINK) {
		if (cc1 >= TYPEC_CC_RP_DEF || cc2 >= TYPEC_CC_RP_DEF) {
			p->vbus_present = true;
			cc_log(p, "%s schedule_delayed_work for TYPEC_SINK\n", __func__);
			schedule_delayed_work(&p->vbus_delayed_work, msecs_to_jiffies(100));
		}
	} else if (cc_cfg->cc_role == TYPEC_SOURCE) {
		if (cc1 == TYPEC_CC_RD || cc2 == TYPEC_CC_RD) {
			cc_log(p, "%s schedule_delayed_work for TYPEC_SOURCE\n", __func__);
			schedule_delayed_work(&p->vbus_delayed_work, msecs_to_jiffies(100));
		}
	}

out:
	return HRTIMER_NORESTART;
}

static void rtk_pd_vbus_work_func(struct work_struct *work)
{
	struct rtk_pd *p = container_of(work, struct rtk_pd,
						  vbus_delayed_work.work);

	if (!p->tcpm) {
		dev_warn(p->dev, "VBUS change ignored: tcpm not ready\n");
		return;
	}
	mutex_lock(&p->lock);
	tc_log(p, "%s: Notifying TCPM of VBUS change (vbus=%d)\n",
	       __func__, p->vbus_present);
	tcpm_vbus_change(p->tcpm);
	mutex_unlock(&p->lock);
}

/* -------------------- TCPM ops Implement -------------------- */
static int rtk_tcpc_init(struct tcpc_dev *tcpc)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);

	dev_info(p->dev, "%s: init\n", __func__);

	mutex_lock(&p->lock);

	/* enable RX/TX and debounce */
	rtk_pd_init(p);

	mutex_unlock(&p->lock);

	/* Notify initial VBUS state to TCPM */
	schedule_delayed_work(&p->vbus_delayed_work, msecs_to_jiffies(10));
	tc_log(p, "%s: Scheduled initial VBUS notification\n", __func__);

	return 0;
}

/* @get_vbus:	Called to read current VBUS state */
static int rtk_tcpc_get_vbus(struct tcpc_dev *tcpc)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);
	bool present;
	int ret;

	tc_log(p, "%s: get_vbus\n", __func__);

	mutex_lock(&p->lock);
	ret = rtk_pd_get_vbus(p, &present);
	mutex_unlock(&p->lock);

	return ret ? ret : present;
}

/* @set_vbus:	Called to enable or disable VBUS */
static int rtk_tcpc_set_vbus(struct tcpc_dev *tcpc, bool on, bool sink)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);
	int ret;

	tc_log(p, "%s: TCPM calls set_vbus on=%d sink=%d\n", __func__, on, sink);

	mutex_lock(&p->lock);

	ret = rtk_pd_set_vbus(p, on, sink);
	if (ret)
		dev_warn(p->dev, "%s: TCPM calls set_vbus fail=%d\n", __func__, ret);

	mutex_unlock(&p->lock);

	return 0;
}

/* @is_vbus_vsafe0v: Called to check if VBUS is at safe 0V level (<0.8V) */
static bool rtk_tcpc_is_vbus_vsafe0v(struct tcpc_dev *tcpc)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);
	bool vsafe0v;

	mutex_lock(&p->lock);
	vsafe0v = rtk_pd_is_vbus_vsafe0v(p);
	mutex_unlock(&p->lock);

	return vsafe0v;
}

/* @get_cc:	Called to read current CC pin values */
static int rtk_tcpc_get_cc(struct tcpc_dev *tcpc,
			   enum typec_cc_status *cc1,
			   enum typec_cc_status *cc2)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);
	int ret;

	cc_log(p, "%s: <===== get_cc (power_role=%s) =====>\n",
	       __func__,
	       p->power_role == TYPEC_SOURCE ? "SOURCE" : "SINK");
	mutex_lock(&p->lock);
	ret = rtk_pd_get_cc_pair(p, cc1, cc2);
	mutex_unlock(&p->lock);
	return ret;
}

/* @set_cc:	Called to set value of CC pins */
static int rtk_tcpc_set_cc(struct tcpc_dev *tcpc, enum typec_cc_status cc)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);
	int ret;

	cc_log(p, "%s: <===== set_cc=%s (%d) =====>\n", __func__,
	       get_cc_voltage_leve_str(cc), cc);

	mutex_lock(&p->lock);
	ret = rtk_pd_set_cc(p, cc);
	mutex_unlock(&p->lock);
	return ret;
}

/* @set_polarity:	Called to set polarity */
static int rtk_tcpc_set_polarity(struct tcpc_dev *tcpc, enum typec_cc_polarity pol)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);

	cc_log(p, "%s: <===== set_polarity=%d (%s) =====>\n", __func__,  pol,
	       pol == TYPEC_POLARITY_CC1 ? "CC1" : "CC2");

	mutex_lock(&p->lock);

	/* Save current polarity */
	p->polarity = pol;

	/* NOTE: Both CC channels should remain enabled (channel_en bit)
	 * The actual TX/RX routing is controlled by En_cc1/En_cc2 during
	 * transmission, not by channel_en.
	 * Reference code always keeps both channels enabled for detection.
	 */
	rtk_pd_aphy_polarity_set(p, pol);

	rtk_typec_set_int_enable(p);

	mutex_unlock(&p->lock);
	return 0;
}

/* @set_vconn:	Called to enable or disable VCONN
 *
 * According to USB Type-C spec, Vconn should be applied to the non-connected CC pin:
 *   - If CC1 is connected (polarity=CC1): apply Vconn to CC2 (vconn2_en_gpio)
 *   - If CC2 is connected (polarity=CC2): apply Vconn to CC1 (vconn1_en_gpio)
 */
static int rtk_tcpc_set_vconn(struct tcpc_dev *tcpc, bool on)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);

	tc_log(p, "%s: set_vconn=%d polarity=%s\n", __func__, on,
	       (p->polarity == TYPEC_POLARITY_CC1) ? "CC1" : "CC2");

	mutex_lock(&p->lock);

	/* Vconn should be applied to the non-connected CC pin */
	if (p->polarity == TYPEC_POLARITY_CC1) {
		/* CC1 is connected, apply Vconn to CC2 */
		if (p->vconn2_en_gpio)
			gpiod_set_value_cansleep(p->vconn2_en_gpio, on);
		/* Ensure vconn1 is off */
		if (p->vconn1_en_gpio && on)
			gpiod_set_value_cansleep(p->vconn1_en_gpio, 0);
	} else {
		/* CC2 is connected, apply Vconn to CC1 */
		if (p->vconn1_en_gpio)
			gpiod_set_value_cansleep(p->vconn1_en_gpio, on);
		/* Ensure vconn2 is off */
		if (p->vconn2_en_gpio && on)
			gpiod_set_value_cansleep(p->vconn2_en_gpio, 0);
	}

	/* When disabling Vconn, turn off both */
	if (!on) {
		if (p->vconn1_en_gpio)
			gpiod_set_value_cansleep(p->vconn1_en_gpio, 0);
		if (p->vconn2_en_gpio)
			gpiod_set_value_cansleep(p->vconn2_en_gpio, 0);
	}

	/* Log current Vconn state */
	if (p->vconn1_en_gpio || p->vconn2_en_gpio)
		tc_log(p, "  Vconn state: vconn1=%d vconn2=%d\n",
		       p->vconn1_en_gpio ? gpiod_get_value(p->vconn1_en_gpio) : -1,
		       p->vconn2_en_gpio ? gpiod_get_value(p->vconn2_en_gpio) : -1);

	/* Track vconn state for debugfs */
	p->vconn_on = on;

	mutex_unlock(&p->lock);
	return 0;
}

/* @set_current_limit:
 *		Called when Sink requests voltage change or when PD negotiation completes.
 *		This is the key function for responding to Sink's voltage change requests.
 *
 * Flow when Sink changes voltage:
 *   1. Sink sends Request message (e.g., requesting 9V @ 2A)
 *   2. TCPM validates and sends Accept
 *   3. TCPM calls this function to set new voltage/current
 *   4. Hardware adjusts VBUS output
 *   5. TCPM sends PS_RDY to indicate voltage transition complete
 */
static int rtk_tcpc_set_current_limit(struct tcpc_dev *tcpc, u32 max_ma, u32 mv)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);
	int ret;

	tc_log(p, "%s: Setting VBUS to %dmV @ %dmA (Sink requested voltage change)\n",
	       __func__, mv, max_ma);

	mutex_lock(&p->lock);

	/* Only adjust VBUS if we are Source */
	if (p->power_role == TYPEC_SOURCE) {
		ret = rtk_pd_regulators_set_vbus(p, mv, max_ma);
		if (ret < 0) {
			dev_err(p->dev, "Failed to set VBUS to %dmV @ %dmA: %d\n",
				mv, max_ma, ret);
			mutex_unlock(&p->lock);
			return ret;
		}

		dev_info(p->dev, "VBUS: %dmV/%dmA (%dmW)\n", mv, max_ma, (mv * max_ma) / 1000);
	} else {
		/* Sink mode: This sets input current limit */
		dev_info(p->dev, "%s: Sink mode - setting input current limit to %dmA\n",
			__func__, max_ma);
	}

	mutex_unlock(&p->lock);
	return 0;
}

/* @set_roles:	Called to set power and data roles */
static int rtk_tcpc_set_roles(struct tcpc_dev *tcpc, bool attached,
			      enum typec_role pr, enum typec_data_role dr)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);
	const char *pr_str = (pr == TYPEC_SOURCE) ? "SOURCE" :
			     (pr == TYPEC_SINK) ? "SINK" : "UNKNOWN";
	const char *dr_str = (dr == TYPEC_HOST) ? "HOST" :
			     (dr == TYPEC_DEVICE) ? "DEVICE" : "UNKNOWN";
	bool pr_changed, dr_changed, attached_changed;

	mutex_lock(&p->lock);

	/* Check what has changed */
	pr_changed = (p->power_role != pr);
	dr_changed = (p->data_role != dr);
	attached_changed = (p->is_attached != attached);

	/* If nothing changed, skip update */
	if (!pr_changed && !dr_changed && !attached_changed) {
		tc_log(p, "%s: No change (attached=%d pr=%s dr=%s), skipping update\n",
		       __func__, attached, pr_str, dr_str);
		mutex_unlock(&p->lock);
		return 0;
	}

	tc_log(p, "%s: set_roles attached=%d->%d pr=%s->%s dr=%s->%s\n",
	       __func__,
	       p->is_attached, attached,
	       (p->power_role == TYPEC_SOURCE) ? "SOURCE" : "SINK", pr_str,
	       (p->data_role == TYPEC_HOST) ? "HOST" : "DEVICE", dr_str);

	/* Update PHY RX level only if power role changed */
	if (pr_changed) {
		tc_log(p, "%s: Power role changed, updating PHY RX level\n", __func__);
		rtk_pd_rx_level_set(p, pr);
	}

	/* Update cached values */
	p->power_role = pr;
	p->data_role = dr;
	p->is_attached = attached;
	p->swap_roles_pending = false;
	p->pending_power_role = p->power_role;
	p->pending_data_role = p->data_role;

	/* Print connection status message only on attach/detach change */
	if (attached_changed) {
		if (attached) {
			enum typec_cc_status cc1, cc2;
			const char *pol_str = (p->polarity == TYPEC_POLARITY_CC1) ? "CC1" : "CC2";

			/* Get current CC status */
			rtk_pd_get_cc_pair(p, &cc1, &cc2);

			dev_info(p->dev, "attached: pr=%s dr=%s polarity=%s CC1=%s CC2=%s\n",
				 pr_str, dr_str, pol_str,
				 get_cc_voltage_leve_str(cc1), get_cc_voltage_leve_str(cc2));
		} else {
			enum typec_cc_status cc1, cc2;
			const char *pol_str = (p->polarity == TYPEC_POLARITY_CC1) ? "CC1" : "CC2";

			/* Get current CC status */
			rtk_pd_get_cc_pair(p, &cc1, &cc2);

			dev_info(p->dev, "detached: pr=%s dr=%s polarity=%s CC1=%s CC2=%s\n",
				 pr_str, dr_str, pol_str,
				 get_cc_voltage_leve_str(cc1), get_cc_voltage_leve_str(cc2));

			/* Clear altmode and partner tracking on disconnect */
			p->altmode_entered = false;
			p->altmode_svid = 0;
			p->altmode_mode = 0;
			p->partner_vid = 0;
			p->partner_pid = 0;
			p->dp_configure_rx = 0;
			p->dp_status_rx = 0;
			p->dp_status_tx = 0;
		}
	} else if (dr_changed) {
		dev_info(p->dev, "DR_Swap: dr=%s pr=%s\n", dr_str, pr_str);
	} else if (pr_changed) {
		dev_info(p->dev, "PR_Swap: pr=%s dr=%s\n", pr_str, dr_str);
	}

	mutex_unlock(&p->lock);
	return 0;
}

/* @set_pd_rx:	Called to enable or disable reception of PD messages */
static int rtk_tcpc_set_pd_rx(struct tcpc_dev *tcpc, bool on)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);

	tc_log(p, "%s: set_pd_rx %s\n", __func__, on ? "on" : "off");

	mutex_lock(&p->lock);

	/* Track the RX enable state requested by TCPM */
	p->rx_enabled_by_tcpm = on;

	if (on)
		rtk_pd_enable_rx(p);
	else
		rtk_pd_disable_rx(p);

	mutex_unlock(&p->lock);

	return 0;
}

/* Internal function to perform actual TX transmission
 * Called by both pd_transmit and tx_delay_timer callback
 * MUST be called with p->lock held
 */
static int rtk_pd_do_transmit_locked(struct rtk_pd *p,
				     enum tcpm_transmit_type type,
				     const struct pd_message *msg,
				     unsigned int negotiated_rev)
{
	struct pd_regmap *pd_regmap = p->pd_regmap;
	int len = 0;

	/* Temporarily disable RX during TX to avoid interference */
	rtk_pd_disable_txrx_digital(p);

	/* Hard/Cable reset trigger ordered set */
	if (type == TCPC_TX_HARD_RESET) {
		tc_log(p, "%s: Sending Hard Reset\n", __func__);

		/* Set Hard Reset orderset in PD_TX_CTRL0
		 *    Hard Reset = K-code sequence: RST-1, RST-1, RST-1, RST-2
		 *    = 0x07 | (0x07<<5) | (0x07<<10) | (0x19<<15) = 0xC9CE7
		 */
		pd_txctrl_tx_orderset(pd_regmap, 0xC9CE7);  /* Hard Reset orderset */
		pd_txctrl_tx_length(pd_regmap, 0);  /* No data payload */

		/* 3. Enable CRC and EOP */
		pd_txctrl_tx_crc_enable(pd_regmap, false);  /* No CRC for Hard Reset */
		pd_txctrl_tx_eop_enable(pd_regmap, true);

		/* 4. Clear any residual TX interrupt status before enabling */
		pd_int_sts_clear(pd_regmap, PD_INT_TX_OK | PD_INT_TX_GC | PD_INT_RX_OK | PD_INT_RX_CRV_1ST);

		rtk_pd_enable_tx(p);

		pd_log(p, "%s: Hard Reset configured and triggered\n", __func__);

		p->tx_in_flight = true;
		p->tx_is_hard_reset = true;  /* Mark as Hard Reset */
		p->rx_suspended_for_tx = true;
		return 0;
	}

	/* SOP */
	/* Write header+payload to PD_RAM_TX (TX window), and trigger send message */
	if (msg) {
		len = rtk_pd_write_tx_message(p, msg);

		/* Set TX length and trigger transmission */
		pd_log(p, "%s: Setting TX length=%d and triggering transmission\n",
			__func__, len);
		/* Set TX length in PD_TX_CTRL0 (bits[31:20]) */
		pd_txctrl_tx_length(pd_regmap, len);

		/* Set SOP orderset in PD_TX_CTRL0 (bits[19:0])
		 *    SOP = K-code sequence: SYNC-1, SYNC-1, SYNC-1, SYNC-2
		 *    = 0x18 | (0x18<<5) | (0x18<<10) | (0x11<<15) = 0x8E318
		 */
		pd_txctrl_tx_orderset(pd_regmap, 0x8E318);  /* SOP orderset */

		/* Enable CRC and EOP in PD_TX_CTRL2 */
		pd_txctrl_tx_crc_enable(pd_regmap, true);
		pd_txctrl_tx_eop_enable(pd_regmap, true);

		/* Clear any residual interrupt status before transmission
		 * CRITICAL: Also clear RX_OK to prevent spurious RX interrupts
		 * that can occur when RX is disabled during TX
		 * NOTE: TX_OK and TX_GC interrupts are already globally enabled in init
		 */
		pd_int_sts_clear(pd_regmap, PD_INT_TX_OK | PD_INT_TX_GC | PD_INT_RX_OK | PD_INT_RX_CRV_1ST);

		rtk_pd_enable_tx(p);

		pd_log(p, "%s: TX configured - len=%d, orderset=0x0001, CRC/EOP enabled\n",
			__func__, len);

		p->tx_in_flight = true;
		p->tx_is_hard_reset = false;  /* This is a normal PD message */
		p->rx_suspended_for_tx = true;
	}

	return 0;
}

/* TX delay work - handles delayed TX in process context (can use mutex safely) */
static void rtk_pd_tx_delay_work_func(struct work_struct *work)
{
	struct rtk_pd *p = container_of(work, struct rtk_pd, tx_delay_work);
	const char *type_name;

	pd_log(p, "%s: TX delay work executing\n", __func__);

	mutex_lock(&p->lock);

	/* Check if we still have pending TX and RX is no longer pending */
	if (p->tx_pending && !p->rx_msg_has_pending) {
		switch (p->tx_pending_type) {
		case TCPC_TX_SOP:               type_name = "SOP"; break;
		case TCPC_TX_SOP_PRIME:         type_name = "SOP'"; break;
		case TCPC_TX_SOP_PRIME_PRIME:   type_name = "SOP''"; break;
		case TCPC_TX_HARD_RESET:        type_name = "Hard_Reset"; break;
		case TCPC_TX_CABLE_RESET:       type_name = "Cable_Reset"; break;
		case TCPC_TX_BIST_MODE_2:       type_name = "BIST_Mode_2"; break;
		default:                        type_name = "Unknown"; break;
		}

		pd_log(p, "%s: Executing delayed TX %s\n", __func__, type_name);
		tc_log(p, "%s: Delayed TX now starting\n", __func__);

		/* Execute the delayed transmission - safe to use mutex here */
		rtk_pd_do_transmit_locked(p, p->tx_pending_type,
					  &p->tx_pending_msg, p->tx_pending_rev);

		/* Clear pending flag */
		p->tx_pending = false;
	} else if (p->tx_pending && p->rx_msg_has_pending) {
		/* RX is still pending, reschedule work after short delay */
		p->rx_msg_wait_count++;

		/*
		 * Soft_Reset timeout: if TX_GC hasn't fired within 20 ms
		 * (20 × 1 ms retries), the hardware is stuck in TX mode with
		 * auto GoodCRC disabled and TX_GC will never come.  Abandon
		 * the buffered Soft_Reset so the TX delay loop can unblock.
		 * The partner will retransmit Soft_Reset if needed.
		 */
		if (p->rx_msg_wait_count >= 20) {
			u16 hdr = le16_to_cpu(p->rx_msg_pending.header);
			u8 mtype = (hdr >> PD_HEADER_TYPE_SHIFT) & PD_HEADER_TYPE_MASK;
			u8 ocnt  = (hdr >> PD_HEADER_CNT_SHIFT)  & PD_HEADER_CNT_MASK;

			if (ocnt == 0 && mtype == PD_CTRL_SOFT_RESET) {
				pd_log(p, "%s: TX_GC timeout (%u ms) waiting for Soft_Reset GoodCRC - abandoning\n",
				       __func__, p->rx_msg_wait_count);
				p->rx_msg_has_pending = false;
				p->rx_msg_wait_count = 0;
				cancel_delayed_work(&p->rx_msg_timeout_work);
				/* Fall through: tx_pending && !rx_msg_has_pending → TX proceeds */
			}
		}

		if (p->rx_msg_has_pending) {
			pd_log(p, "%s: RX still pending (count=%u), rescheduling work\n",
			       __func__, p->rx_msg_wait_count);
			mutex_unlock(&p->lock);
			hrtimer_start(&p->tx_delay_timer, ms_to_ktime(1), HRTIMER_MODE_REL);
			return;
		}

		/* rx_msg_has_pending was just cleared (timeout path) — execute TX now */
		rtk_pd_do_transmit_locked(p, p->tx_pending_type,
					  &p->tx_pending_msg, p->tx_pending_rev);
		p->tx_pending = false;
	}

	mutex_unlock(&p->lock);
}

/* TX delay timer callback - called in softirq context, schedules work
 * IMPORTANT: This runs in softirq context (cannot use mutex or sleep).
 * We only schedule work here, actual TX processing happens in work function.
 */
static enum hrtimer_restart rtk_pd_tx_delay_timer_callback(struct hrtimer *timer)
{
	struct rtk_pd *p = container_of(timer, struct rtk_pd, tx_delay_timer);

	pd_log(p, "%s: TX delay timer expired, scheduling work\n", __func__);

	/* Schedule work to handle TX in process context where mutex is safe */
	schedule_work(&p->tx_delay_work);

	return HRTIMER_NORESTART;
}

/*
 * TX_GC timeout work for buffered RX messages.
 *
 * Normally TX_GC fires within microseconds of RX_OK (hardware auto-sends
 * GoodCRC and generates TX_GC).  If TX_GC never comes (e.g. auto-GoodCRC was
 * inadvertently disabled when a spurious TX_OK and RX_OK land in the same ISR
 * call), the GoodCRC was NEVER sent to the partner.  The partner's 30 µs
 * GoodCRC timer already expired; it has either retransmitted or moved on to
 * error recovery.
 *
 * Passing the stale message to TCPM at this point is wrong: TCPM would reply
 * to a message the partner no longer expects, corrupting the protocol state.
 * Instead, DISCARD the message and re-enable the RX/auto-GoodCRC path so
 * subsequent messages are handled correctly.
 */
static void rtk_pd_rx_msg_timeout_work_func(struct work_struct *work)
{
	struct rtk_pd *p = container_of(to_delayed_work(work),
					struct rtk_pd, rx_msg_timeout_work);
	u16 hdr;
	u8 mtype, ocnt;

	mutex_lock(&p->lock);

	if (!p->rx_msg_has_pending)
		goto unlock; /* TX_GC already handled it */

	hdr   = le16_to_cpu(p->rx_msg_pending.header);
	mtype = (hdr >> PD_HEADER_TYPE_SHIFT) & PD_HEADER_TYPE_MASK;
	ocnt  = (hdr >> PD_HEADER_CNT_SHIFT)  & PD_HEADER_CNT_MASK;

	pd_log(p, "%s: TX_GC never fired - GoodCRC not sent, discarding msg type=0x%x ocnt=%d\n",
	       __func__, mtype, ocnt);

	p->rx_msg_has_pending = false;
	p->rx_msg_length = 0;

	/* Re-enable RX / auto-GoodCRC so the next incoming message works */
	if (p->rx_enabled_by_tcpm && !p->tx_in_flight)
		rtk_pd_enable_rx(p);

unlock:
	mutex_unlock(&p->lock);
}

/* Check if received message is a GoodCRC control message
 * Reference: sample code pd.c:66-73 HW_PD_msg_is_goodcrc()
 * GoodCRC format: Message Type = 0x01, Number of Data Objects = 0
 */
static bool rtk_pd_msg_is_goodcrc(const struct pd_message *msg)
{
	u16 header = msg->header;

	/* Check Message Type (bits 4:0) == 0x01 (GoodCRC) */
	if ((header & 0x1F) != 0x01)
		return false;

	/* Check Number of Data Objects (bits 14:12) == 0 */
	if ((header & 0x7000) != 0)
		return false;

	return true;
}

/* -------------------- Source Mode Voltage Adjustment -------------------- */

/**
 * rtk_pd_parse_sink_request() - Parse Sink Request message and cache voltage info
 * @p: rtk_pd structure
 * @msg: Received PD Request message
 *
 * Parses the Request Data Object (RDO) to extract requested PDO index,
 * voltage, and current. Stores this info for later voltage adjustment.
 */
static void rtk_pd_parse_sink_request(struct rtk_pd *p, const struct pd_message *msg)
{
	u32 rdo;
	unsigned int pdo_index;
	u32 pdo;
	u32 voltage_mv, current_ma;

	/* Only process in Source mode */
	if (p->power_role != TYPEC_SOURCE)
		return;

	/* Check if we have source PDOs configured */
	if (!p->src_pdo || p->nr_src_pdo == 0) {
		pd_log(p, "%s: No source PDOs configured\n", __func__);
		return;
	}

	/* Extract RDO from message payload */
	rdo = le32_to_cpu(msg->payload[0]);

	/* Get PDO index (1-based) */
	pdo_index = rdo_index(rdo);

	if (pdo_index == 0 || pdo_index > p->nr_src_pdo) {
		pd_log(p, "%s: Invalid PDO index %u (have %u PDOs)\n",
		       __func__, pdo_index, p->nr_src_pdo);
		return;
	}

	/* Get the corresponding PDO from our source capabilities */
	pdo = p->src_pdo[pdo_index - 1];

	/* Extract voltage and current based on PDO type */
	switch (pdo_type(pdo)) {
	case PDO_TYPE_FIXED:
		voltage_mv = pdo_fixed_voltage(pdo);
		current_ma = rdo_op_current(rdo);
		break;
	case PDO_TYPE_VAR:
		voltage_mv = pdo_max_voltage(pdo);
		current_ma = rdo_op_current(rdo);
		break;
	case PDO_TYPE_BATT:
		voltage_mv = pdo_max_voltage(pdo);
		current_ma = 0; /* Battery PDO uses power instead */
		pd_log(p, "%s: Battery PDO not fully supported\n", __func__);
		break;
	default:
		pd_log(p, "%s: Unknown PDO type %d\n", __func__, pdo_type(pdo));
		return;
	}

	/* Cache the values */
	p->src_pdo_requested = true;
	p->src_req_voltage_mv = voltage_mv;
	p->src_req_current_ma = current_ma;
	p->src_req_pdo_index = pdo_index;

	pd_log(p, "%s: Sink requested PDO#%u: %umV @ %umA\n",
	       __func__, pdo_index, voltage_mv, current_ma);
}

/**
 * rtk_pd_adjust_source_voltage() - Adjust VBUS voltage for Source mode
 * @p: rtk_pd structure
 *
 * Called when TCPM sends Accept message. Adjusts VBUS to the voltage
 * requested by Sink (previously cached by rtk_pd_parse_sink_request).
 */
static void rtk_pd_adjust_source_voltage(struct rtk_pd *p)
{
	int ret;

	tc_log(p, "%s: Adjusting VBUS\n", __func__);

	/* Only adjust if we have a pending request */
	if (!p->src_pdo_requested)
		return;

	/* Only adjust in Source mode */
	if (p->power_role != TYPEC_SOURCE) {
		pd_log(p, "%s: Not in Source mode, skipping voltage adjustment\n", __func__);
		p->src_pdo_requested = false;
		return;
	}

	tc_log(p, "%s: Adjusting VBUS to %umV @ %umA (PDO#%u)\n",
	       __func__, p->src_req_voltage_mv, p->src_req_current_ma, p->src_req_pdo_index);

	/* Adjust VBUS voltage and current limit */
	ret = rtk_pd_regulators_set_vbus(p, p->src_req_voltage_mv, p->src_req_current_ma);
	if (ret < 0) {
		dev_err(p->dev, "%s: Failed to adjust VBUS to %umV @ %umA: %d\n",
			__func__, p->src_req_voltage_mv, p->src_req_current_ma, ret);
		/* Don't return error - TCPM already sent Accept */
	} else {
		dev_info(p->dev, "VBUS adjusted: PDO#%u %umV/%umA (%umW)\n",
			 p->src_req_pdo_index, p->src_req_voltage_mv,
			 p->src_req_current_ma,
			 (p->src_req_voltage_mv * p->src_req_current_ma) / 1000);
	}

	/* Clear the request flag */
	p->src_pdo_requested = false;
}

/* @pd_transmit:Called to transmit PD message */
static int rtk_tcpc_pd_transmit(struct tcpc_dev *tcpc,
				enum tcpm_transmit_type type,
				const struct pd_message *msg,
				unsigned int negotiated_rev)
{
	struct rtk_pd *p = container_of(tcpc, struct rtk_pd, tcpc);
	const char *type_name;

	/* Convert transmit type to human-readable string */
	switch (type) {
	case TCPC_TX_SOP:               type_name = "SOP"; break;
	case TCPC_TX_SOP_PRIME:         type_name = "SOP'"; break;
	case TCPC_TX_SOP_PRIME_PRIME:   type_name = "SOP''"; break;
	case TCPC_TX_HARD_RESET:        type_name = "Hard_Reset"; break;
	case TCPC_TX_CABLE_RESET:       type_name = "Cable_Reset"; break;
	case TCPC_TX_BIST_MODE_2:       type_name = "BIST_Mode_2"; break;
	default:                        type_name = "Unknown"; break;
	}

	/* Enhanced log with detailed transmission information */
	if (msg) {
		u16 header = le16_to_cpu(msg->header);
		u8 msg_type = (header >> PD_HEADER_TYPE_SHIFT) & PD_HEADER_TYPE_MASK;
		u8 msg_id = (header >> PD_HEADER_ID_SHIFT) & PD_HEADER_ID_MASK;
		u8 obj_cnt = (header >> PD_HEADER_CNT_SHIFT) & PD_HEADER_CNT_MASK;
		const char *msg_name;

		/* Determine if control or data message and get name */
		if (obj_cnt == 0) {
			msg_name = pd_ctrl_msg_name(msg_type);
		} else {
			msg_name = pd_data_msg_name(msg_type);
		}

		/* If Accept for DR/PR swap, clear swap_roles_pending */
		if (msg_type == PD_CTRL_ACCEPT && p->swap_roles_pending)
			p->swap_roles_pending = false;

		tc_log(p, "%s: TX %s [%s type=0x%x id=%d objs=%d] rev=%u\n",
		       __func__, type_name, msg_name, msg_type, msg_id, obj_cnt, negotiated_rev);
		pd_log(p, "%s: State: tx_busy=%d rx_en=%d rx_msg_has_pending=%d\n",
		       __func__,
		       p->tx_in_flight, p->rx_enabled_by_tcpm, p->rx_msg_has_pending);

		/* Check if this is a VDM (Vendor Defined Message) for Alt Mode */
		if (obj_cnt > 0 && msg_type == PD_DATA_VENDOR_DEF) {
			u32 vdo_header = le32_to_cpu(msg->payload[0]);
			u16 svid = (vdo_header >> 16) & 0xFFFF;
			u8 vdm_cmd = vdo_header & 0x1F;
			const char *vdm_cmd_str;

			switch (vdm_cmd) {
			case 1: vdm_cmd_str = "Discover_Identity"; break;
			case 2: vdm_cmd_str = "Discover_SVIDs"; break;
			case 3: vdm_cmd_str = "Discover_Modes"; break;
			case 4: vdm_cmd_str = "Enter_Mode"; break;
			case 5: vdm_cmd_str = "Exit_Mode"; break;
			case 6: vdm_cmd_str = "Attention"; break;
			default: vdm_cmd_str = "Unknown"; break;
			}

			pd_log(p, "=== VDM Transmitting ===\n");
			pd_log(p, "  Command: %s (0x%02x)\n", vdm_cmd_str, vdm_cmd);
			pd_log(p, "  SVID: 0x%04x %s\n", svid,
				 svid == 0xFF01 ? "(DisplayPort)" : "");
			pd_log(p, "  Objects: %d\n", obj_cnt);
			pd_log(p, "========================\n");

			/* Track TX DP_Status_Update (cmd=0x10) or Attention (cmd=6).
			 * HPD state changes are signaled via Attention.
			 */
			if (svid == 0xFF01 &&
			    (vdm_cmd == 0x10 || vdm_cmd == CMD_ATTENTION) && obj_cnt >= 2)
				p->dp_status_tx = le32_to_cpu(msg->payload[1]);
		}

		/* Check if we're sending PS_RDY - time to adjust voltage */
		if (obj_cnt == 0 && msg_type == PD_CTRL_PS_RDY) {
			rtk_pd_adjust_source_voltage(p);
		}
	} else {
		/* No message payload (e.g., Hard Reset) */
		tc_log(p, "%s: TX %s (no payload) rev=%u\n",
		       __func__, type_name, negotiated_rev);
		pd_log(p, "%s: State: tx_busy=%d rx_en=%d rx_msg_has_pending=%d\n",
		       __func__,
		       p->tx_in_flight, p->rx_enabled_by_tcpm, p->rx_msg_has_pending);
	}

	/* Log TX message to debugfs */
	if (msg) {
		u16 header = le16_to_cpu(msg->header);
		u8 obj_cnt = (header >> PD_HEADER_CNT_SHIFT) & PD_HEADER_CNT_MASK;
		u32 data[7];
		int i;

		/* Convert payload to native endian for logging */
		for (i = 0; i < obj_cnt && i < 7; i++)
			data[i] = le32_to_cpu(msg->payload[i]);

		pd_msg_log_tx(p, header, data, obj_cnt, type);
	}

	/* Read hardware registers before transmission for stability */
	/* Log register values if debugging is enabled */
	if (p->raw_data_enable) {
		pd_log(p, "%s: [REG DUMP BEFORE] Dumping all registers:\n", __func__);
		rtk_pd_dump_registers(p, NULL, "[REG DUMP BEFORE] ");
	}

	mutex_lock(&p->lock);

	/* Check if RX message is currently being processed (received but GoodCRC not sent yet)
	 * If so, delay TX until RX processing completes (TX_GC interrupt)
	 */
	if (p->rx_msg_has_pending) {
		pd_log(p, "%s: RX message pending, delaying TX until RX completes\n", __func__);
		tc_log(p, "%s: Delaying TX - waiting for RX processing to complete\n", __func__);

		/* Buffer the TX request */
		p->tx_pending = true;
		p->tx_pending_type = type;
		if (msg)
			memcpy(&p->tx_pending_msg, msg, sizeof(*msg));
		p->tx_pending_rev = negotiated_rev;

		/* Start timer to retry TX after short delay (1ms) */
		hrtimer_start(&p->tx_delay_timer, ms_to_ktime(1), HRTIMER_MODE_REL);

		mutex_unlock(&p->lock);
		return 0;
	}

	/* Execute transmission immediately */
	rtk_pd_do_transmit_locked(p, type, msg, negotiated_rev);

	/* Unlock mutex immediately to allow interrupt handler to process TX_OK/TX_GC!
	 * The previous code held mutex for 80-100ms in Sink mode, blocking interrupt handler.
	 * This caused GoodCRC responses to be missed, leading to PD negotiation failures.
	 */
	mutex_unlock(&p->lock);

	/* Log register values if debugging is enabled */
	if (p->raw_data_enable) {
		pd_log(p, "%s: [REG DUMP AFTER] Dumping all registers:\n", __func__);
		rtk_pd_dump_registers(p, NULL, "[REG DUMP AFTER] ");
		rtk_pd_dump_message_ram(p, NULL, true, "[TX MSG] ");
		rtk_pd_dump_message_ram(p, NULL, false, "[RX MSG] ");
	}

	/* Minimal delay to ensure TX hardware has latched configuration.
	 * This is now outside mutex to avoid blocking interrupt handler.
	 * Register dumps above provide ~2-5ms implicit delay, so minimal delay is sufficient.
	 */
	usleep_range(100, 200);  /* 100-200us for hardware stabilization */

	return 0;
}

/*
 * rtk_pd_hard_reset_received - clean up local state on Hard Reset
 *
 * Called (with p->lock held) whenever a Hard Reset ordered set is
 * detected from the partner.  Aborts any in-flight TX, discards any
 * buffered RX message, and cancels deferred work so the driver is in
 * a clean state before handing control to TCPM via tcpm_pd_hard_reset().
 */
static void rtk_pd_hard_reset_received(struct rtk_pd *p)
{
	/* Abort any in-flight TX (partner will not send GoodCRC) */
	if (p->tx_in_flight) {
		rtk_pd_disable_tx(p);
		if (!p->tx_is_hard_reset)
			tcpm_pd_transmit_complete(p->tcpm, TCPC_TX_DISCARDED);
		p->tx_in_flight = false;
		p->tx_is_hard_reset = false;
	}

	/* Cancel pending delayed TX */
	if (p->tx_pending) {
		hrtimer_cancel(&p->tx_delay_timer);
		p->tx_pending = false;
	}

	/* Discard any buffered RX message */
	if (p->rx_msg_has_pending) {
		cancel_delayed_work(&p->rx_msg_timeout_work);
		p->rx_msg_has_pending = false;
		p->rx_msg_length = 0;
	}

	p->swap_roles_pending = false;

	/* Clear altmode tracking state — hard reset exits all modes */
	p->altmode_entered = false;
	p->altmode_svid = 0;
	p->altmode_mode = 0;
	p->dp_configure_rx = 0;
	p->dp_status_rx = 0;
	p->dp_status_tx = 0;

	rtk_pd_enable_auto_goodcrc_resp(p);
}

/* ----- IRQ handler -------------------- */
static irqreturn_t rtk_pd_irq_thread(int irq, void *data)
{
	struct rtk_pd *p = data;
	struct pd_regmap *pd_regmap;
	u32 sts, typec_sts, tmp_sts = 0;
	bool vbus_now;

	mutex_lock(&p->lock);

	pd_regmap = p->pd_regmap;

	sts = pd_int_sts_get(pd_regmap);

	if (sts & PD_INT_RX_CRV_1ST) {
		u32 reg_rx_status1 = pd_readl(pd_regmap, OFF_PD_RX_STATUS1);
		u32 reg_rx_status2 = pd_readl(pd_regmap, OFF_PD_RX_STATUS2);

		tmp_sts |= PD_INT_RX_CRV_1ST;

		pd_log(p, "%s: PD_INT_RX_CRV_1ST (sts=0x%x)\n", __func__, sts);

		/* DEBUG: Dump all PD registers for debugging */
		if (p->raw_data_enable) {
			rtk_pd_dump_registers(p, NULL, "[RX REG DUMP] ");
			rtk_pd_dump_message_ram(p, NULL, false, "[RX MSG] ");
		}

		pd_log(p, "%s: [RX REG DUMP] PD_RX_STATUS1=0x%08x (BYTE_CNT=%d CRC_OK=%d)\n",
			__func__, reg_rx_status1,
			(int)FIELD_GET(GENMASK(8, 0), reg_rx_status1),
			!!(reg_rx_status1 & BIT(10)));
		pd_log(p, "%s: [RX REG DUMP] PD_RX_STATUS2=0x%08x (FSM=%d TIMEOUT_0=%d TIMEOUT_1=%d DEC_FAIL=%d)\n",
			__func__, reg_rx_status2,
			(int)FIELD_GET(GENMASK(30, 24), reg_rx_status2),
			!!(reg_rx_status2 & BIT(21)),
			!!(reg_rx_status2 & BIT(22)),
			!!(reg_rx_status2 & BIT(23)));
	}

	/* TX_OK interrupt: We finished transmitting our message */
	if (sts & PD_INT_TX_OK) {
		tmp_sts |= PD_INT_TX_OK;

		tc_log(p, "%s: TX_OK interrupt - our message transmitted\n", __func__);
		pd_log(p, "%s: PD_INT_TX_OK (sts=0x%x)\n", __func__, sts);

		if (p->tx_in_flight) {
			bool is_hard_reset = p->tx_is_hard_reset;
			u32 reg_basic_ctrl;

			/* DEBUG: Dump TX hardware state when TX interrupt fires */
			//if (p->raw_data_enable) {
			//	pd_log(p, "%s: [TX REG DUMP] Dumping all registers:\n", __func__);
			//	rtk_pd_dump_registers(p, NULL, "[TX REG DUMP] ");
			//	rtk_pd_dump_message_ram(p, NULL, true, "[TX MSG] ");
			//}

			/* NOTE: TX_OK interrupt remains enabled - no need to mask
			 * Multiple TX_OK interrupts are prevented by tx_in_flight flag check
			 */

			reg_basic_ctrl = pd_readl(pd_regmap, OFF_PD_BASIC_CTRL);
			pd_log(p, "%s: [TX REG DUMP] PD_BASIC_CTRL=0x%08x (RX_EN=%d TX_EN=%d)\n",
				__func__, reg_basic_ctrl,
				!!(reg_basic_ctrl & BASIC_RX_EN),
				!!(reg_basic_ctrl & BASIC_TX_EN));
			pd_log(p, "%s: pd_int_sts 0x%x (0x%x)\n", __func__, sts, pd_readl(pd_regmap, OFF_PD_INT));

			/* Handle different TX completion scenarios */
			if (is_hard_reset) {
				/* Hard Reset: TX_OK is sufficient (no GoodCRC expected) */
				if (sts & PD_INT_TX_OK) {
					rtk_pd_disable_tx(p);

					p->tx_in_flight = false;
					p->tx_is_hard_reset = false;

					/*
					 * TCPM requested the Hard Reset.
					 * Signal TCPM that transmission is done.
					 * TCPM blocks in tcpm_pd_transmit() waiting for this
					 * before it can proceed to HARD_RESET_START.
					 */
					tc_log(p, "Hard Reset sent successfully\n");
					tcpm_pd_transmit_complete(p->tcpm, TCPC_TX_SUCCESS);
				}
			} else {
				/* Normal PD message: TX_OK means message was sent
				 * IMPORTANT: We DON'T complete TX here!
				 * We wait for RX_OK with GoodCRC response from partner.
				 * RX handler will call tcpm_pd_transmit_complete() when GoodCRC arrives.
				 *
				 * Flow:
				 * 1. TX_OK fires (here) - message transmitted
				 * 2. RX_OK fires - partner's GoodCRC received
				 * 3. RX handler checks if it's GoodCRC
				 * 4. If yes: complete TX successfully
				 * 5. If no GoodCRC within timeout: TCPM retries
				 */
				if (sts & PD_INT_TX_OK) {
					tc_log(p, "TX_OK: Message sent, waiting for RX GoodCRC response\n");
					/* Keep tx_in_flight=true, tx_is_hard_reset as-is */
					/* RX handler will clear these when GoodCRC arrives */
				}
			}
		} else {
			/* TX_OK but tx_in_flight=false
			 *
			 * This is NORMAL when TX_GC is also set - hardware auto-sent GoodCRC
			 * and triggered both TX_OK and TX_GC interrupts.
			 * Only warn if this is NOT a TX_GC auto-response.
			 */
			if (!(sts & PD_INT_TX_GC)) {
				u32 reg_basic_ctrl = pd_readl(pd_regmap, OFF_PD_BASIC_CTRL);

				dev_warn(p->dev, "%s: TX interrupt but tx_in_flight=false (not TX_GC)\n",
					 __func__);
				pd_log(p, "%s: [SPURIOUS TX] PD_BASIC_CTRL=0x%08x (RX_EN=%d TX_EN=%d)\n",
					__func__, reg_basic_ctrl,
					!!(reg_basic_ctrl & BASIC_RX_EN),
					!!(reg_basic_ctrl & BASIC_TX_EN));
				pd_log(p, "%s: [SPURIOUS TX] Disabling TX_EN to stop spurious interrupts\n",
					__func__);

				/* Force disable TX to stop spurious interrupts
				 * rtk_pd_disable_tx() will clear the TX_OK interrupt internally
				 */
				rtk_pd_disable_tx(p);
			} else {
				/* TX_OK from hardware auto-sending GoodCRC - this is normal */
				tc_log(p, "%s: TX_OK from hardware auto-GoodCRC (normal)\n", __func__);
			}
		}
	}

	/* RX okay. Get RX packet, report to TCPM */
	if (sts & PD_INT_RX_OK) {
		struct pd_message m = { 0 };
		int rx_len;
		u32 reg_rx_status1;
		u32 orderset = pd_rxctrl_orderset_cmp_stat(pd_regmap);

		tmp_sts |= PD_INT_RX_OK;
		pd_log(p, "%s: PD_INT_RX_OK (sts=0x%x orderset=0x%x)\n",
		       __func__, sts, orderset);

		/* Hard Reset ordered set received from partner */
		if (orderset & PD_RX_HARD_RST_MATCH) {
			tc_log(p, "RX Hard Reset from partner — resetting state\n");
			rtk_pd_hard_reset_received(p);
			pd_int_sts_clear(pd_regmap, PD_INT_RX_CRV_1ST | PD_INT_RX_OK);
			mutex_unlock(&p->lock);
			tcpm_pd_hard_reset(p->tcpm);
			return IRQ_HANDLED;
		}

		/* SOP' / SOP'' messages are for cable plug communication.
		 * This driver does not support cable plug (EMCA) negotiation.
		 * Mark for drop but still buffer so TX_GC fires and RX state is
		 * properly restored — early return would leave HW stuck post-TX.
		 */
		if (orderset & (PD_RX_SOP1_MATCH | PD_RX_SOP1_DBG_MATCH |
				PD_RX_SOP2_MATCH | PD_RX_SOP2_DBG_MATCH)) {
			pd_log(p, "%s: SOP'/SOP'' message (orderset=0x%x), will drop after TX_GC\n",
			       __func__, orderset);
			p->rx_msg_drop_pending = true;
		} else {
			p->rx_msg_drop_pending = false;
		}

		/* Read RX_STATUS1 for specific checks */
		reg_rx_status1 = pd_readl(pd_regmap, OFF_PD_RX_STATUS1);
		if (!(reg_rx_status1 & BIT(10))) {
			dev_warn(p->dev, "%s: WARNING: RX_OK but CRC_OK=0! Message may be corrupted.\n",
				__func__);
		}

		rx_len = rtk_pd_read_rx_message(p, &m);

		/* Only pass valid messages to TCPM.
		 * rx_len=0 indicates spurious RX_OK or HW error.
		 * Passing empty messages causes TCPM to send spurious GoodCRC.
		 */
		if (rx_len > 0) {
			bool is_goodcrc = rtk_pd_msg_is_goodcrc(&m);
			u16 header = le16_to_cpu(m.header);
			u8 msg_type = (header >> PD_HEADER_TYPE_SHIFT) & PD_HEADER_TYPE_MASK;
			u8 msg_id = (header >> PD_HEADER_ID_SHIFT) & PD_HEADER_ID_MASK;
			u8 obj_cnt = (header >> PD_HEADER_CNT_SHIFT) & PD_HEADER_CNT_MASK;
			u8 pwr_role = (header & PD_HEADER_PWR_ROLE) ? 1 : 0;
			u8 data_role = (header & PD_HEADER_DATA_ROLE) ? 1 : 0;
			u8 spec_rev = (header >> PD_HEADER_REV_SHIFT) & PD_HEADER_REV_MASK;
			const char *msg_name;
			bool crc_ok = !!(reg_rx_status1 & BIT(10));

			/* Determine message name */
			if (obj_cnt == 0)
				msg_name = pd_ctrl_msg_name(msg_type);
			else
				msg_name = pd_data_msg_name(msg_type);

			/* Check if this is a GoodCRC response to our TX */
			if (is_goodcrc && p->tx_in_flight && !p->tx_is_hard_reset) {
				/* SUCCESS: Received GoodCRC response to our transmitted message!
				 * This acknowledges our TX was received by partner.
				 * DO NOT pass GoodCRC to TCPM - it's just an ACK, not a new message.
				 * Complete TX and reset non-PD detection counters.
				 */
				pd_log(p, "RX: GoodCRC for TX [%s type=0x%x id=%d] len=%d CRC=%s\n",
				       msg_name, msg_type, msg_id, rx_len, crc_ok ? "OK" : "BAD");
				pd_log(p, "State: tx_busy=%d->0 rx_en=%d rx_susp=%d->0\n",
				       p->tx_in_flight, p->rx_enabled_by_tcpm, p->rx_suspended_for_tx);
				tc_log(p, "TX SUCCESS - completing transmission\n");

				/* Disable TX hardware to prevent spurious TX_OK interrupts
				 * IMPORTANT: Must disable TX_EN after transmission completes
				 * to stop hardware from generating additional TX_OK interrupts.
				 * Without this, hardware continues triggering TX_OK even when
				 * tx_in_flight=false, causing "TX interrupt but tx_in_flight=false" warnings.
				 */
				rtk_pd_disable_tx(p);

				/* Clear TX state */
				p->tx_in_flight = false;
				p->tx_is_hard_reset = false;

				/* Notify TCPM that TX completed successfully */
				tcpm_pd_transmit_complete(p->tcpm, TCPC_TX_SUCCESS);
			} else if (is_goodcrc) {
				/* GoodCRC but not waiting for it - hardware auto-response
				 * This happens when partner sends us a message and hardware
				 * auto-sends GoodCRC. TX_GC interrupt also fires.
				 * Don't pass to TCPM as it's not a real message.
				 */
				tc_log(p, "%s RX: GoodCRC auto-response HW auto-ACK, not passed to TCPM\n",
				       __func__);
				pd_log(p, "%s RX: GoodCRC auto-response [%s type=0x%x id=%d] len=%d CRC=%s\n",
				       __func__,
				       msg_name, msg_type, msg_id, rx_len, crc_ok ? "OK" : "BAD");
			} else {
				/* Normal PD message - buffer it for TX_GC */
				tc_log(p, "%s RX: Message from partner [%s type=0x%x id=%d objs=%d]\n",
				       __func__, msg_name, msg_type, msg_id, obj_cnt);
				pd_log(p, "%s RX: len=%d CRC=%s rev=%d pwr=%s data=%s | Buffering for TX_GC\n",
				       __func__,
				       rx_len, crc_ok ? "OK" : "BAD", spec_rev,
				       pwr_role ? "SRC" : "SNK", data_role ? "DFP" : "UFP");

				/* Track pending role swap so GoodCRC uses new roles even before set_roles */
				if (msg_type == PD_CTRL_DR_SWAP) {
					p->swap_roles_pending = true;
					p->pending_power_role = p->power_role;
					p->pending_data_role = (p->data_role == TYPEC_HOST) ? TYPEC_DEVICE : TYPEC_HOST;
					pd_log(p, "%s RX: DR_Swap pending -> PR=%s DR=%s (for Rx send GoodCRC preprogrammed)\n",
					       __func__,
					       p->pending_power_role == TYPEC_SOURCE ? "SRC" : "SNK",
					       p->pending_data_role == TYPEC_HOST ? "DFP" : "UFP");
				} else if (msg_type == PD_CTRL_PR_SWAP) {
					p->swap_roles_pending = true;
					p->pending_power_role = (p->power_role == TYPEC_SOURCE) ? TYPEC_SINK : TYPEC_SOURCE;
					p->pending_data_role = p->data_role;
					pd_log(p, "%s RX: PR_Swap pending -> PR=%s DR=%s (for Rx send GoodCRC preprogrammed)\n",
					       __func__,
					       p->pending_power_role == TYPEC_SOURCE ? "SRC" : "SNK",
					       p->pending_data_role == TYPEC_HOST ? "DFP" : "UFP");
				}

				/* Buffer the message to be passed to TCPM after TX_GC */
				memcpy(&p->rx_msg_pending, &m, sizeof(m));
				p->rx_msg_length = rx_len;
				p->rx_msg_has_pending = true;
				p->rx_msg_wait_count = 0;
				pd_log(p, "%s RX: Message buffered, waiting for TX_GC to pass to TCPM\n", __func__);
				/* Safety net: if TX_GC never fires, unblock after 10 ms */
				schedule_delayed_work(&p->rx_msg_timeout_work, msecs_to_jiffies(10));

				/*
				 * Deadlock prevention: if TX is in-flight when we receive a
				 * message, the partner sent this instead of the expected GoodCRC
				 * for our TX.  In TX mode, auto GoodCRC is disabled
				 * (pd_txctrl_auto_resp_good_crc_enable=false), so the hardware
				 * will NOT auto-send GoodCRC for the received message, meaning
				 * TX_GC will never fire and rx_msg_has_pending stays true forever.
				 *
				 * Fix: abort the in-flight TX (partner discarded it), then
				 * re-enable auto GoodCRC so the hardware can respond to the
				 * partner's message.  The partner will retry if the 30 us
				 * GoodCRC window has already passed.
				 */
				if (p->tx_in_flight) {
					pd_log(p, "%s RX: msg arrived while TX in-flight - abort TX, re-enable auto GoodCRC\n",
					       __func__);
					rtk_pd_disable_tx(p);
					p->tx_in_flight = false;
					p->tx_is_hard_reset = false;
					rtk_pd_enable_auto_goodcrc_resp(p);
					tcpm_pd_transmit_complete(p->tcpm, TCPC_TX_DISCARDED);
				}
			}
		} else {
			pd_log(p, "%s: Ignoring RX with rx_len=0 (spurious interrupt)\n",
				__func__);
		}
	}

	/* TX_GC interrupt: We received a message and auto-sent GoodCRC response
	 * This is separate from TX_OK which indicates we sent our own message.
	 * TX_GC confirms hardware auto-reply worked - now pass buffered message to TCPM.
	 */
	if (sts & PD_INT_TX_GC) {
		tmp_sts |= PD_INT_TX_GC;

		tc_log(p, "%s: TX_GC - Auto-sent GoodCRC to partner's message\n", __func__);
		pd_log(p, "%s: PD_INT_TX_GC (sts=0x%x)\n", __func__, sts);

		/* Now that GoodCRC has been sent, pass the buffered message to TCPM */
		if (p->rx_msg_has_pending) {
			u16 header = le16_to_cpu(p->rx_msg_pending.header);
			u8 msg_type = (header >> PD_HEADER_TYPE_SHIFT) & PD_HEADER_TYPE_MASK;
			u8 msg_id = (header >> PD_HEADER_ID_SHIFT) & PD_HEADER_ID_MASK;
			u8 obj_cnt = (header >> PD_HEADER_CNT_SHIFT) & PD_HEADER_CNT_MASK;
			const char *msg_name;

			/* Determine message name */
			if (obj_cnt == 0) {
				msg_name = pd_ctrl_msg_name(msg_type);
			} else {
				msg_name = pd_data_msg_name(msg_type);
			}

			pd_log(p, "%s: TX_GC complete, passing buffered message to TCPM\n",
			       __func__);
			pd_log(p, "[%s type=0x%x id=%d objs=%d len=%d]\n",
			       msg_name, msg_type, msg_id, obj_cnt, p->rx_msg_length);
			tc_log(p, "%s: Passing buffered RX message to TCPM after GoodCRC sent\n", __func__);

			/* Check if this is a Request message from Sink and parse it */
			if (obj_cnt > 0 && msg_type == PD_DATA_REQUEST) {
				rtk_pd_parse_sink_request(p, &p->rx_msg_pending);
			}

			/* Check if this is a VDM (Vendor Defined Message) for Alt Mode */
			if (obj_cnt > 0 && msg_type == PD_DATA_VENDOR_DEF) {
				u32 vdo_header = le32_to_cpu(p->rx_msg_pending.payload[0]);
				u16 svid = (vdo_header >> 16) & 0xFFFF;
				u8 vdm_cmd = vdo_header & 0x1F;
				const char *vdm_cmd_str;

				switch (vdm_cmd) {
				case 1: vdm_cmd_str = "Discover_Identity"; break;
				case 2: vdm_cmd_str = "Discover_SVIDs"; break;
				case 3: vdm_cmd_str = "Discover_Modes"; break;
				case 4: vdm_cmd_str = "Enter_Mode"; break;
				case 5: vdm_cmd_str = "Exit_Mode"; break;
				case 6: vdm_cmd_str = "Attention"; break;
				default: vdm_cmd_str = "Unknown"; break;
				}

				pd_log(p, "=== VDM Received ===\n");
				pd_log(p, "  Command: %s (0x%02x)\n", vdm_cmd_str, vdm_cmd);
				pd_log(p, "  SVID: 0x%04x %s\n", svid,
				       svid == 0xFF01 ? "(DisplayPort)" : "");
				pd_log(p, "  Objects: %d\n", obj_cnt);
				pd_log(p, "====================\n");

				/* Update connector_status tracking state */
				rtk_pd_track_vdm_rx(p, &p->rx_msg_pending, obj_cnt);
			}

			/* Drop SOP'/SOP'' messages — do not pass to TCPM */
			if (p->rx_msg_drop_pending) {
				pd_log(p, "%s: Dropping SOP'/SOP'' buffered message, not passing to TCPM\n",
				       __func__);
				p->rx_msg_has_pending = false;
				p->rx_msg_drop_pending = false;
				p->rx_msg_length = 0;
				cancel_delayed_work(&p->rx_msg_timeout_work);
				goto tx_gc_done;
			}

			/* Pass the buffered message to TCPM */
			/* Log RX message to debugfs */
			{
				u32 data[7];
				int i;

				/* Convert payload to native endian for logging */
				for (i = 0; i < obj_cnt && i < 7; i++)
					data[i] = le32_to_cpu(p->rx_msg_pending.payload[i]);

				pd_msg_log_rx(p, header, data, obj_cnt, TCPC_TX_SOP);
			}

			tcpm_pd_receive(p->tcpm, &p->rx_msg_pending, TCPC_TX_SOP);

			/* Clear the buffer flag */
			p->rx_msg_has_pending = false;
			p->rx_msg_length = 0;
			/* Cancel the safety-net timeout - TX_GC handled the message */
			cancel_delayed_work(&p->rx_msg_timeout_work);

			/* If TX was delayed waiting for RX, trigger it now */
			if (p->tx_pending) {
				pd_log(p, "%s: RX complete, triggering delayed TX immediately\n", __func__);
				tc_log(p, "%s: Executing pending TX after RX complete\n", __func__);

				/* Cancel the timer (but not work_sync to avoid deadlock with mutex)
				 * The work function will check tx_pending flag and do nothing if false
				 */
				hrtimer_cancel(&p->tx_delay_timer);

				/* Execute the delayed transmission */
				rtk_pd_do_transmit_locked(p, p->tx_pending_type,
							  &p->tx_pending_msg, p->tx_pending_rev);

				/* Clear pending flag - work will see this and abort if it runs */
				p->tx_pending = false;
			}
		} else {
			pd_log(p, "%s: TX_GC but no pending message to pass to TCPM\n", __func__);
		}
tx_gc_done:
		;
	}

	if (sts & PD_INT_TX_GC || sts & PD_INT_TX_OK || sts & PD_INT_RX_OK) {
		if (p->rx_enabled_by_tcpm &&
		    !p->tx_in_flight && !p->rx_msg_has_pending) {
			p->rx_suspended_for_tx = false;
			tc_log(p, "%s call rtk_pd_enable_rx\n", __func__);
			rtk_pd_enable_rx(p);
		}
	}

	/* CC change */
	typec_sts = typec_int_sts_get(pd_regmap);
	if ((sts & PD_INT_CC_DET) || (typec_sts & ALL_CC_INT_STS)) {
		tmp_sts |= (sts & PD_INT_CC_DET); /* only mark bits that actually fired */

		cc_log(p, "%s: CC_DET sts=0x%x typec_sts=0x%x -> cc_check_timer\n",
		       __func__, sts, typec_sts);

		hrtimer_start(&p->cc_check_timer, ms_to_ktime(0), HRTIMER_MODE_REL);
	}

	/* VBUS change */
	if (sts & PD_INT_VBUS_MON) {
		bool old_vbus = p->vbus_present;

		tmp_sts |= PD_INT_VBUS_MON;
		if (!rtk_pd_get_vbus(p, &vbus_now)) {
			if (vbus_now != old_vbus) {
				pd_log(p, "VBUS changed: %s -> %s\n",
				       old_vbus ? "present" : "absent",
				       vbus_now ? "present" : "absent");
				tcpm_vbus_change(p->tcpm);
			}
		}
	}

	if (sts != tmp_sts)
		pd_log(p, "%s: some interrupt no process (sts=0x%x, tmp_sts=0x%x)\n",
			 __func__, sts, tmp_sts);

	/* clean interrupt stauts (W1C) */
	pd_int_sts_clear(pd_regmap, sts);
	typec_int_sts_clear(pd_regmap, typec_sts);

	mutex_unlock(&p->lock);

	return IRQ_HANDLED;
}

/* control clock and reset */
static inline struct reset_control *typec_reset_get(struct device_node *node,
	    const char *str)
{
	struct reset_control *reset;

	reset = of_reset_control_get_exclusive(node, str);
	if (IS_ERR(reset))
		reset = NULL;

	return reset;
}

static inline void typec_reset_put(struct reset_control *reset)
{
	if (reset)
		reset_control_put(reset);
}

static inline int typec_reset_deassert(struct reset_control *reset)
{
	if (!reset)
		return 0;

	return reset_control_deassert(reset);
}

static inline int typec_reset_assert(struct reset_control *reset)
{
	if (!reset)
		return 0;

	return reset_control_assert(reset);
}

static inline struct clk *typec_clk_get(struct device_node *node, const char *str)
{
	struct clk *clk;

	clk = of_clk_get_by_name(node, str);
	if (IS_ERR(clk))
		clk = NULL;

	return clk;
}

static inline void typec_clk_put(struct clk *clk)
{
	if (clk)
		clk_put(clk);
}

static void typec_clk_enable(struct clk *clk)
{
	if (clk)
		clk_prepare_enable(clk);
}

static void typec_clk_disable(struct clk *clk)
{
	if (clk)
		clk_disable_unprepare(clk);
}

/* Source and Sink PDO definitions */
/* Source Power Data Objects (PDOs)
 * When acting as Source, advertise these power capabilities to Sink devices.
 * Format: PDO_FIXED(voltage_mV, current_mA, flags)
 *
 * PDO1: 5V 3A - Always required as base power (USB PD spec requirement)
 * PDO2: 9V 3A - Additional power level for higher voltage charging
 *
 * Total power capability: 27W (9V × 3A)
 */
static const u32 rtk_src_pdo[] = {
	/* PDO1: 5V 1.5A (7.5W) - Base power level (mandatory) */
	PDO_FIXED(5000, 1500, PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_USB_COMM),
	/* Uncomment below for additional voltage levels: */
	/* PDO1: 5V 3A (15W) - Base power level (mandatory) */
	//PDO_FIXED(5000, 3000, PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_USB_COMM),
	/* PDO2: 9V 3A (27W) - Higher voltage level */
	// PDO_FIXED(9000, 3000, 0),   /* PDO2: 9V 3A (27W) */
	/* PDO3: 12V 2.5A (30W) - Higher voltage level */
	// PDO_FIXED(12000, 2500, 0),  /* PDO3: 12V 2.5A (30W) */
};

/* Sink Power Data Objects (PDOs) */
static const u32 rtk_snk_pdo[] = {
	PDO_FIXED(5000, 500, PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_USB_COMM),
};

static const struct property_entry rtk_props[] = {
	PROPERTY_ENTRY_STRING("data-role", "dual"),
	PROPERTY_ENTRY_STRING("power-role", "dual"),
	PROPERTY_ENTRY_STRING("try-power-role", "sink"),
	PROPERTY_ENTRY_U32_ARRAY("source-pdos", rtk_src_pdo),
	PROPERTY_ENTRY_U32_ARRAY("sink-pdos", rtk_snk_pdo),
	PROPERTY_ENTRY_U32("op-sink-microwatt", 15000000),
	/* follows for pd-disable */
	//PROPERTY_ENTRY_BOOL("pd-disable"),
	PROPERTY_ENTRY_STRING("typec-power-opmode", "1.5A"),
	{ }
};

/* -------------------- Sysfs Attributes -------------------- */
static ssize_t pd_log_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", p->pd_log_enable);
}

static ssize_t pd_log_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	p->pd_log_enable = enable;
	dev_info(dev, "PD log %s\n", enable ? "enabled" : "disabled");

	return count;
}
static DEVICE_ATTR_RW(pd_log_enable);

static ssize_t cc_log_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", p->cc_log_enable);
}

static ssize_t cc_log_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	p->cc_log_enable = enable;
	dev_info(dev, "CC log %s\n", enable ? "enabled" : "disabled");

	return count;
}
static DEVICE_ATTR_RW(cc_log_enable);

static ssize_t tc_log_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", p->tc_log_enable);
}

static ssize_t tc_log_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	p->tc_log_enable = enable;
	dev_info(dev, "TC log %s\n", enable ? "enabled" : "disabled");

	return count;
}
static DEVICE_ATTR_RW(tc_log_enable);

static ssize_t raw_data_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", p->raw_data_enable);
}

static ssize_t raw_data_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	p->raw_data_enable = enable;
	dev_info(dev, "Raw data log %s\n", enable ? "enabled" : "disabled");

	return count;
}
static DEVICE_ATTR_RW(raw_data_enable);

static ssize_t log_to_console_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", p->log_to_console);
}

static ssize_t log_to_console_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	p->log_to_console = enable;
	dev_info(dev, "log to console %s\n", enable ? "enabled" : "disabled");

	return count;
}
static DEVICE_ATTR_RW(log_to_console);

static ssize_t connection_status_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	const char *pr_str = (p->power_role == TYPEC_SOURCE) ? "SOURCE" :
			     (p->power_role == TYPEC_SINK) ? "SINK" : "UNKNOWN";

	return sprintf(buf, "Power Role: %s\n"
			    "VBUS Present: %d\n"
			    "Polarity: %s\n",
			    pr_str,
			    p->vbus_present,
			    (p->polarity == TYPEC_POLARITY_CC1) ? "CC1" : "CC2");
}
static DEVICE_ATTR_RO(connection_status);

/* GPIO control sysfs attributes */
static ssize_t vbus_gpio_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	int value = 0;

	if (p->vbus_en_gpio)
		value = gpiod_get_value(p->vbus_en_gpio);

	return sprintf(buf, "%d\n", value);
}

static ssize_t vbus_gpio_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	int value;
	int ret;

	if (!p->vbus_en_gpio) {
		dev_err(dev, "VBUS GPIO not available\n");
		return -ENODEV;
	}

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	if (value != 0 && value != 1)
		return -EINVAL;

	gpiod_set_value(p->vbus_en_gpio, value);
	dev_info(dev, "VBUS GPIO set to %d\n", value);

	return count;
}
static DEVICE_ATTR_RW(vbus_gpio);

static ssize_t vconn1_gpio_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	int value = 0;

	if (p->vconn1_en_gpio)
		value = gpiod_get_value(p->vconn1_en_gpio);

	return sprintf(buf, "%d\n", value);
}

static ssize_t vconn1_gpio_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	int value;
	int ret;

	if (!p->vconn1_en_gpio) {
		dev_err(dev, "VCONN1 GPIO not available\n");
		return -ENODEV;
	}

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	if (value != 0 && value != 1)
		return -EINVAL;

	gpiod_set_value_cansleep(p->vconn1_en_gpio, value);
	dev_info(dev, "VCONN1 GPIO set to %d\n", value);

	return count;
}
static DEVICE_ATTR_RW(vconn1_gpio);

static ssize_t vconn2_gpio_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	int value = 0;

	if (p->vconn2_en_gpio)
		value = gpiod_get_value(p->vconn2_en_gpio);

	return sprintf(buf, "%d\n", value);
}

static ssize_t vconn2_gpio_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct rtk_pd *p = dev_get_drvdata(dev);
	int value;
	int ret;

	if (!p->vconn2_en_gpio) {
		dev_err(dev, "VCONN2 GPIO not available\n");
		return -ENODEV;
	}

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	if (value != 0 && value != 1)
		return -EINVAL;

	gpiod_set_value_cansleep(p->vconn2_en_gpio, value);
	dev_info(dev, "VCONN2 GPIO set to %d\n", value);

	return count;
}
static DEVICE_ATTR_RW(vconn2_gpio);


static struct attribute *rtk_pd_attrs[] = {
	&dev_attr_pd_log_enable.attr,
	&dev_attr_cc_log_enable.attr,
	&dev_attr_tc_log_enable.attr,
	&dev_attr_raw_data_enable.attr,
	&dev_attr_log_to_console.attr,
	&dev_attr_connection_status.attr,
	&dev_attr_vbus_gpio.attr,
	&dev_attr_vconn1_gpio.attr,
	&dev_attr_vconn2_gpio.attr,
	NULL,
};

static const struct attribute_group rtk_pd_attr_group = {
	.attrs = rtk_pd_attrs,
};

/* -------------------- Probe / Remove -------------------- */
static int rtk_pd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_pd *p;
	struct pd_regmap *pd_regmap;
	struct resource *res;
	const struct cc_cfg *cc_cfg;
	int ret;

	dev_dbg(dev, "%s: probe\n", __func__);
	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	pd_regmap = devm_kzalloc(dev, sizeof(struct pd_regmap), GFP_KERNEL);
	if (!pd_regmap)
		return -ENOMEM;

	mutex_init(&p->lock);
	p->dev = dev;
	p->pd_regmap = pd_regmap;

	/* Initialize power role to SINK (DRP will negotiate actual role later) */
	p->power_role = TYPEC_SINK;
	p->pending_power_role = p->power_role;

	/* Initialize debug log control (default: disabled, use dynamic debug) */
	p->pd_log_enable = false;
	p->cc_log_enable = false;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pd_regmap->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pd_regmap->base))
		return PTR_ERR(pd_regmap->base);

	pd_regmap->ram_rx = pd_regmap->base + PD_RAM;
	pd_regmap->ram_tx = pd_regmap->base + PD_RAM_TX;
	pd_regmap->ram_hdr = pd_regmap->base + PD_RAM;

	pd_regmap->rx_map = devm_regmap_init_mmio(dev, pd_regmap->ram_rx, &mmio_regmap_config);
	if (IS_ERR(pd_regmap->rx_map))
		return PTR_ERR(pd_regmap->rx_map);

	pd_regmap->tx_map = devm_regmap_init_mmio(dev, pd_regmap->ram_tx, &mmio_regmap_config);
	if (IS_ERR(pd_regmap->tx_map))
		return PTR_ERR(pd_regmap->tx_map);

	/* setup type c module */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -EINVAL;
	pd_regmap->typec_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pd_regmap->typec_base)
		return -ENOMEM;

	cc_cfg = device_get_match_data(dev);
	if (!cc_cfg) {
		dev_err(dev, "type_c config are not assigned!\n");
		ret = -EINVAL;
		return ret;
	}

	p->cc_cfg = devm_kzalloc(dev, sizeof(*cc_cfg), GFP_KERNEL);

	memcpy(p->cc_cfg, cc_cfg, sizeof(*cc_cfg));

	if (rtk_typec_setup_parameter(p)) {
		dev_err(dev, "ERROR: %s to setup type c parameter!!", __func__);
		ret = -EINVAL;
		return ret;
	}

	p->irq = platform_get_irq(pdev, 0);
	if (p->irq < 0)
		return p->irq;

	/* Optional board controls */
	p->vbus_en_gpio  = devm_gpiod_get_optional(dev, "vbus-en", GPIOD_OUT_LOW);
	p->vconn1_en_gpio = devm_gpiod_get_optional(dev, "vconn1-en", GPIOD_OUT_LOW);
	p->vconn2_en_gpio = devm_gpiod_get_optional(dev, "vconn2-en", GPIOD_OUT_LOW);

	if (IS_ERR_OR_NULL(p->vbus_en_gpio)) {
		dev_err(dev, "Error vbus-en-gpio no found (err=%d)\n",
			(int)PTR_ERR(p->vbus_en_gpio));
		p->vbus_en_gpio = NULL;
	} else {
		dev_info(dev, "%s get vbus-en-gpio (id=%d) OK\n",
			__func__, desc_to_gpio(p->vbus_en_gpio));
		gpiod_direction_output(p->vbus_en_gpio, 0);
	}
	if (IS_ERR_OR_NULL(p->vconn1_en_gpio)) {
		dev_err(dev, "Error vconn1-en-gpio no found (err=%d)\n",
			(int)PTR_ERR(p->vconn1_en_gpio));
		p->vconn1_en_gpio = NULL;
	} else {
		dev_info(dev, "%s get vconn1-en-gpio (id=%d) OK\n",
			__func__, desc_to_gpio(p->vconn1_en_gpio));
		gpiod_direction_output(p->vconn1_en_gpio, 0);
	}
	if (IS_ERR_OR_NULL(p->vconn2_en_gpio)) {
		dev_err(dev, "Error vconn2-en-gpio no found (err=%d)\n",
			(int)PTR_ERR(p->vconn2_en_gpio));
		p->vconn2_en_gpio = NULL;
	} else {
		dev_info(dev, "%s get vconn2-en-gpio (id=%d) OK\n",
			__func__, desc_to_gpio(p->vconn2_en_gpio));
		gpiod_direction_output(p->vconn2_en_gpio, 0);
	}

	p->clk = typec_clk_get(dev->of_node, "type-c");
	p->reset = typec_reset_get(dev->of_node, "type-c");

	typec_reset_deassert(p->reset);
	typec_clk_enable(p->clk);

	/* Initialize timers */
	hrtimer_init(&p->cc_check_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	p->cc_check_timer.function = rtk_pd_cc_check_timer_callback;

	hrtimer_init(&p->tx_delay_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	p->tx_delay_timer.function = rtk_pd_tx_delay_timer_callback;

	INIT_WORK(&p->tx_delay_work, rtk_pd_tx_delay_work_func);
	INIT_DELAYED_WORK(&p->vbus_delayed_work, rtk_pd_vbus_work_func);
	INIT_DELAYED_WORK(&p->rx_msg_timeout_work, rtk_pd_rx_msg_timeout_work_func);

	ret = rtk_pd_regulators_init(p);
	if (ret < 0)
		dev_warn(dev, "RTK_PD: rtk_pd_regulators_init FAILED with error %d\n", ret);

	/* tcpc_dev ops */
	p->tcpc.init              = rtk_tcpc_init;
	p->tcpc.get_vbus          = rtk_tcpc_get_vbus;
	p->tcpc.set_vbus          = rtk_tcpc_set_vbus;
	p->tcpc.is_vbus_vsafe0v   = rtk_tcpc_is_vbus_vsafe0v;
	p->tcpc.get_cc            = rtk_tcpc_get_cc;
	p->tcpc.set_cc            = rtk_tcpc_set_cc;
	p->tcpc.set_polarity      = rtk_tcpc_set_polarity;
	p->tcpc.set_roles         = rtk_tcpc_set_roles;
	p->tcpc.set_vconn         = rtk_tcpc_set_vconn;
	p->tcpc.set_current_limit = rtk_tcpc_set_current_limit;

	p->tcpc.set_pd_rx         = rtk_tcpc_set_pd_rx;
	p->tcpc.pd_transmit       = rtk_tcpc_pd_transmit;

	p->tcpc.fwnode = device_get_named_child_node(dev, "connector");
	if (!p->tcpc.fwnode) {
		dev_info(dev, "No connector node in DT, using software node\n");
		p->tcpc.fwnode = fwnode_create_software_node(rtk_props, NULL);
		if (IS_ERR(p->tcpc.fwnode))
			return PTR_ERR(p->tcpc.fwnode);
	} else {
		dev_info(dev, "Using DT connector node for fwnode\n");
	}

	p->tcpm = tcpm_register_port(dev, &p->tcpc);
	if (IS_ERR(p->tcpm)) {
		ret = PTR_ERR(p->tcpm);
		dev_err(dev, "RTK_PD: tcpm_register_port() FAILED with error %d\n", ret);
		goto err_swnode;
	}

	dev_info(dev, "RTK_PD: tcpm_register_port() SUCCESS!\n");

	/* Read source PDOs from fwnode for voltage adjustment */
	ret = fwnode_property_count_u32(p->tcpc.fwnode, "source-pdos");
	if (ret > 0) {
		p->nr_src_pdo = ret;
		p->src_pdo = devm_kcalloc(dev, p->nr_src_pdo, sizeof(u32), GFP_KERNEL);
		if (!p->src_pdo) {
			ret = -ENOMEM;
			goto err_unregister;
		}

		ret = fwnode_property_read_u32_array(p->tcpc.fwnode, "source-pdos",
						     p->src_pdo, p->nr_src_pdo);
		if (ret < 0) {
			dev_err(dev, "Failed to read source-pdos: %d\n", ret);
			goto err_unregister;
		}

		dev_info(dev, "Loaded %u source PDOs from DT/fwnode\n", p->nr_src_pdo);
	} else {
		/* Use default PDOs if not specified in DT */
		p->nr_src_pdo = ARRAY_SIZE(rtk_src_pdo);
		p->src_pdo = devm_kmemdup(dev, rtk_src_pdo,
					  sizeof(rtk_src_pdo), GFP_KERNEL);
		if (!p->src_pdo) {
			ret = -ENOMEM;
			goto err_unregister;
		}
		dev_info(dev, "Using default %u source PDOs\n", p->nr_src_pdo);
	}

	ret = devm_request_threaded_irq(dev, p->irq, NULL, rtk_pd_irq_thread,
					IRQF_ONESHOT,
					dev_name(dev), p);
	if (ret)
		goto err_unregister;

	platform_set_drvdata(pdev, p);

	/* Create sysfs attributes for debug log control */
	ret = sysfs_create_group(&dev->kobj, &rtk_pd_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create sysfs group: %d\n", ret);
		goto err_unregister;
	}

	tcpc_debugfs_init(p);

	dev_info(dev, "RTK PD driver probed successfully\n");
	p->tc_log_enable = true;
	dev_info(dev, "Debug control: pd_log_enable=%d cc_log_enable=%d tc_log_enable=%d\n",
		 p->pd_log_enable, p->cc_log_enable, p->tc_log_enable);

	return 0;

err_unregister:
	tcpm_unregister_port(p->tcpm);
err_swnode:
	if (fwnode_property_present(p->tcpc.fwnode, "data-role"))
		fwnode_handle_put(p->tcpc.fwnode);
	else
		fwnode_remove_software_node(p->tcpc.fwnode);
	tcpc_debugfs_exit(p);
	return ret;
}

static void rtk_pd_remove(struct platform_device *pdev)
{
	struct rtk_pd *p = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s: remove\n", __func__);

	/* Cancel timers and work */
	hrtimer_cancel(&p->cc_check_timer);
	hrtimer_cancel(&p->tx_delay_timer);
	cancel_work_sync(&p->tx_delay_work);
	cancel_delayed_work_sync(&p->vbus_delayed_work);
	cancel_delayed_work_sync(&p->rx_msg_timeout_work);

	/* Remove sysfs attributes */
	sysfs_remove_group(&pdev->dev.kobj, &rtk_pd_attr_group);

	tcpm_unregister_port(p->tcpm);
	fwnode_remove_software_node(p->tcpc.fwnode);
	tcpc_debugfs_exit(p);

	typec_reset_assert(p->reset);
	typec_clk_disable(p->clk);

	typec_reset_put(p->reset);
	typec_clk_put(p->clk);

	return;
}

static const struct cc_cfg rtd1635_typec_pd_cfg = {
	.parameter_ver = PARAMETER_V1,
	.cc_dfp_mode = CC_MODE_DFP_1_5,
	.cc1_param = { .rp_4p7k_code = 0xf,
		       .rp_36k_code = 0xf,
		       .rp_12k_code = 0xf,
		       .rd_code = 0xf,
		       .ra_code = 0x7,
		       .vref_2p6v = 0x7,
		       .vref_1p23v = 0x7,
		       .vref_0p8v = 0x7,
		       .vref_0p66v = 0x7,
		       .vref_0p4v = 0x7,
		       .vref_0p2v = 0x7,
		       .vref_1_1p6v = 0x7,
		       .vref_0_1p6v = 0x7 },
	.cc2_param = { .rp_4p7k_code = 0xf,
		       .rp_36k_code = 0xf,
		       .rp_12k_code = 0xf,
		       .rd_code = 0xf,
		       .ra_code = 0x7,
		       .vref_2p6v = 0x7,
		       .vref_1p23v = 0x7,
		       .vref_0p8v = 0x7,
		       .vref_0p66v = 0x7,
		       .vref_0p4v = 0x7,
		       .vref_0p2v = 0x7,
		       .vref_1_1p6v = 0x7,
		       .vref_0_1p6v = 0x7 },
	.debounce_val = 0x7f, /* 1b,1us 7f,4.7us */
	.use_defalut_parameter = true,
};

static const struct of_device_id rtk_pd_of_match[] = {
	{ .compatible = "realtek,rtk-rtd1635-pd-tcpc", .data = &rtd1635_typec_pd_cfg },
	{ }
};
MODULE_DEVICE_TABLE(of, rtk_pd_of_match);

static struct platform_driver rtk_pd_driver = {
	.probe  = rtk_pd_probe,
	.remove = rtk_pd_remove,
	.driver = {
		.name           = "rtk_rtd_pd_tcpc",
		.of_match_table = rtk_pd_of_match,
	},
};
module_platform_driver(rtk_pd_driver);

MODULE_AUTHOR("Stanley Chang <stanley_chang@realtek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Realtek SoC Type C PD Controller TCPC driver");
