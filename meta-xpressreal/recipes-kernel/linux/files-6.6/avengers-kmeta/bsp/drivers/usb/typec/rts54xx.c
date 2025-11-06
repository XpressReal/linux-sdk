/**
 * rts54xx.c - Realtek RTS54XX Type C driver
 *
 * Copyright (C) 2024 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

//#define VERBOSE_DEBUG
//#define DEBUG

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-smbus.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/usb.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/suspend.h>

#include <linux/usb/pd.h>
#include <linux/usb/typec_dp.h>
#include <linux/usb/typec_mux.h>
#include "ucsi/ucsi.h"

#define RTS54XX_MAX_WRITE_DATA_LEN 32
#define RTS54XX_BLOCK_READ_CMD 0x80

/**
 * @brief SMbus Command struct for Realtek commands
 */
struct smbus_cmd_t {
	/* Command */
	uint8_t cmd;
	/* Number of bytes to write */
	uint8_t len;
	/* Sub-Command */
	uint8_t sub;
};

enum rtk_subcommand {
	CMD_VENDOR_CMD_ENABLE = 0xDA,
	CMD_SET_NOTIFICATION_ENABLE = 0x01,
	CMD_SET_PDO = 0x03,
	CMD_SET_RDO = 0x04,
	CMD_SET_TPC_RP = 0x05,
	CMD_SET_TPC_CSD_OPERATION_MODE = 0x1D,
	CMD_SET_TPC_RECONNECT = 0x1F,
	CMD_FORCE_SET_POWER_SWITCH = 0x21,
	CMD_GET_PDOS = 0x83,
	CMD_GET_RDO = 0x84,
	CMD_GET_VDO = 0x9A,
	CMD_GET_CURRENT_PARTNER_SRC_PDO = 0xA7,
	CMD_GET_RTK_STATUS = 0x00,
	CMD_SET_RETIMER_FW_UPDATE_MODE = 0x00,
	CMD_RTS_UCSI_GET_CABLE_PROPERTY = 0x11,
	CMD_GET_PCH_DATA_STATUS = 0xE0,
	CMD_ACK_CC_CI = 0x00,
	CMD_RTS_UCSI_GET_LPM_PPM_INFO = 0x22,
};

/** @brief Realtek SMbus commands */
static const struct smbus_cmd_t VENDOR_CMD_ENABLE = { 0x01, 0x03, 0xDA };

static const struct smbus_cmd_t TCPM_RESET = { 0x08, 0x03, 0x00 };
static const struct smbus_cmd_t SET_NOTIFICATION_ENABLE = { 0x08, 0x06, 0x01 };
static const struct smbus_cmd_t SET_PDO = { 0x08, 0x03, 0x03 };
static const struct smbus_cmd_t SET_RDO = { 0x08, 0x06, 0x04 };
static const struct smbus_cmd_t AUTO_SET_RDO = { 0x08, 0x03, 0x44 };
static const struct smbus_cmd_t SET_TPC_RP = { 0x08, 0x03, 0x05 };
static const struct smbus_cmd_t SEND_VDM = { 0x08, 0x06, 0x19 };
//static const struct smbus_cmd_t SET_VDO = { 0x08, 0x08, 0x1A };
//static const struct smbus_cmd_t SET_DP_STATUS_VDO_of_SELF = { 0x08, 0x08, 0x1A };
//static const struct smbus_cmd_t SET_DP_CAP_VDO_of_SELF = { 0x08, 0x08, 0x1A };
//static const struct smbus_cmd_t SET_DP_CFG_VDO_of_SELF = { 0x08, 0x08, 0x1A };
//static const struct smbus_cmd_t SET_TPC_CSD_OPERATION_MODE = { 0x08, 0x03, 0x1D };
//static const struct smbus_cmd_t SET_TPC_RECONNECT = { 0x08, 0x03, 0x1F };
static const struct smbus_cmd_t INIT_PD_AMS = { 0x08, 0x03, 0x20 };
static const struct smbus_cmd_t FORCE_SET_POWER_SWITCH = { 0x08, 0x03, 0x21 };
static const struct smbus_cmd_t SET_TPC_DISCONNECT = { 0x08, 0x02, 0x23 };
//static const struct smbus_cmd_t ACK_POWER_CTRL_REQ = { 0x08, 0x02, 0x24 };
//static const struct smbus_cmd_t SET_BYPASS_SVID = { 0x08, 0x02, 0x26 };
//static const struct smbus_cmd_t SET_BBR_CTS = { 0x08, 0x03, 0x27 };
//static const struct smbus_cmd_t SET_BC12 = { 0x08, 0x03, 0x28 };
//static const struct smbus_cmd_t SET_SYS_PWR_STATE = { 0x08, 0x03, 0x2B };
static const struct smbus_cmd_t GET_PDOS = { 0x08, 0x03, 0x83 };
static const struct smbus_cmd_t GET_RDO = { 0x08, 0x02, 0x84 };
static const struct smbus_cmd_t GET_TPC_RP = { 0x08, 0x02, 0x85 };
static const struct smbus_cmd_t GET_VDM = { 0x08, 0x02, 0x99 };
static const struct smbus_cmd_t GET_VDO = { 0x08, 0x03, 0x9A };
//static const struct smbus_cmd_t READ_DP_STATUS_VDO_of_SELF = { 0x08, 0x04, 0x9A };
//static const struct smbus_cmd_t READ_DP_CAP_VDO_of_PARTNER = { 0x08, 0x04, 0x9A };
//static const struct smbus_cmd_t READ_DP_CAP_VDO_of_SELF = { 0x08, 0x04, 0x9A };
//static const struct smbus_cmd_t READ_DP_CFG_VDO_of_PARTNER = { 0x08, 0x04, 0x9A };
//static const struct smbus_cmd_t READ_DP_CFG_VDO_of_SELF = { 0x08, 0x04, 0x9A };
//static const struct smbus_cmd_t GET_TPC_CSD_OPERATION_MODE = { 0x08, 0x03, 0x9D };
//static const struct smbus_cmd_t GET_AMS_STATUS = { 0x08, 0x02, 0xA2 };
//static const struct smbus_cmd_t GET_BYPASS_SVID = { 0x08, 0x03, 0xA6 };
//static const struct smbus_cmd_t GET_CURRENT_PARTNER_SRC_PDO = { 0x08, 0x02, 0xA7 };
//static const struct smbus_cmd_t GET_DATA_MESSAGE = { 0x08, 0x02, 0xA8 };
//static const struct smbus_cmd_t GET_POWER_SWITCH_STATE = { 0x08, 0x02, 0xA9 };
//static const struct smbus_cmd_t GET_EPR_PDO = { 0x08, 0x03, 0xAA };
//static const struct smbus_cmd_t GET_PCH_DATA_STATUS = { 0x08, 0x02, 0xE0 };
//static const struct smbus_cmd_t GET_CSD = { 0x08, 0x02, 0xF0 };

static const struct smbus_cmd_t GET_RTK_STATUS = { 0x09, 0x03 };
static const struct smbus_cmd_t ACK_CC_CI = { 0x0A, 0x07, 0x00 };

static const struct smbus_cmd_t RTS_UCSI_CMD[] = {
			 [UCSI_PPM_RESET] = { 0x0E, 0x02, 0x01 },
			 [UCSI_CONNECTOR_RESET] = { 0x0E, 0x03, 0x03 },
			 [UCSI_GET_CAPABILITY] = { 0x0E, 0x02, 0x06 },
			 [UCSI_GET_CONNECTOR_CAPABILITY] = { 0x0E, 0x03, 0x07 },
			 [UCSI_SET_UOM] = { 0x0E, 0x04, 0x08 },
			 [UCSI_SET_UOR] = { 0x0E, 0x04, 0x09 },
			 [UCSI_SET_PDR] = { 0x0E, 0x04, 0x0B },
			 [UCSI_GET_ALTERNATE_MODES] = { 0x0E, 0x05, 0x0C },
			 [UCSI_GET_CAM_SUPPORTED] = { 0x0E, 0x02, 0x0D },
			 [UCSI_GET_CURRENT_CAM] = { 0x0E, 0x02, 0x0E },
			 [UCSI_SET_NEW_CAM] = { 0x0E, 0x08, 0x0F },
			 [UCSI_GET_PDOS] = { 0x0E, 0x03, 0x10 },
			 [UCSI_GET_CABLE_PROPERTY] = { 0x0E, 0x02, 0x11 },
			 [UCSI_GET_CONNECTOR_STATUS] = { 0x0E, 0x2, 0x12 },
			 [UCSI_GET_ERROR_STATUS] = { 0x0E, 0x02, 0x13 },
			 //[UCSI_READ_POWER_LEVEL] = { 0x0E, 0x03, 0x1E },
			 //[UCSI_SET_USB] = { 0x0E, 0x07, 0x21 },
			 //[UCSI_GET_LPM_PPM_INFO] = { 0x0E, 0x02, 0x22 },
			};

//static const struct smbus_cmd_t SET_FTS_STATUS = { 0x12, 0x03, 0x01 };
//static const struct smbus_cmd_t GET_FTS_STATUS = { 0x12, 0x03, 0x02 };

//static const struct smbus_cmd_t SET_RETIMER_FW_UPDATE_MODE = { 0x20, 0x03, 0x00 };

static const struct smbus_cmd_t GET_IC_STATUS = { 0x3A, 0x03 };

struct tcpm_command {
	u8 cmd;
	u8 data_len;
	u8 data[RTS54XX_MAX_WRITE_DATA_LEN];
} __packed;

#define PING_STATUS_CMD_STATUS(n) (n & 0x3)
#define PING_STATUS_DATA_LEN(n) ((n & 0xFC) >> 2)

struct tcpm_command_status {
	u8 cmd_status:2;
#define PING_STATUS_IN_PORCESS 	0x0
#define PING_STATUS_COMPLETE 	0x1
#define PING_STATUS_DEFERRED 	0x2
#define PING_STATUS_ERROR 	0x3
	u8 data_len:6;
} __packed;

struct pd_ic_info {
	u8 code; /* byte0 */
#define IC_STATUS_ROMCORE 0x0
#define IC_STATUS_FLASHCODE 0x1
	u16 reservrd1;
	u8 fw_main_ver; /* byte3*/
	u8 fw_sub_ver1;
	u8 fw_sub_ver2;
	u16 reservrd2;
	u8 pd_tc; /* byte8 */
#define IC_STATUS_PD_READY(n) (n & 0x1)
#define IC_STATUS_TC_CONNECT(n) ((n >> 3) & 0x1)
	u16 vid;
	u16 pid;
	u8 reservrd3;
	u8 running_flash_bank_offset;
	u8 reservrd4[7];
	u8 pd_reversion_major;
	u8 pd_reversion_minor;
	u8 pd_version_major;
	u8 pd_version_minor;
	u8 reservrd5[6];
} __packed;

struct pd_status_change {
	u8 command_complete:1; /* unused */
	u8 extenal_supply_change:1;
	u8 power_operatin_mode_change:1;
	u8 reservrd1:2;
	u8 supported_provider_capabilities_change:1;
	u8 negotiated_power_level_change:1;
	u8 pd_reset_complete:1;
	u8 supported_cam_change:1;
	u8 battery_changing_status_change:1;
	u8 reservrd2:1; /* bit 10 */
	u8 port_partner_change:1;
	u8 power_direction_change:1;
	u8 reservrd3:1;
	u8 connect_change:1;
	u8 error:1; /*unused */
	u8 power_reading_ready:1;
	u8 reservrd4_set0:1;
	u8 reservrd5_set0:1;
	u8 reservrd6_set0:1;
	u8 alternate_flow_change:1; /* bit 20 */
	u8 dp_status_change:1;
	u8 dfp_ocp_status:1;
	u8 port_operation_mode_change:1;
	u8 power_control_request:1;
	u8 vdm_received:1;
	u8 pdfw_start:1;
	u8 data_message_received:1;
	u8 reservrd7_set0:1;
	u8 reservrd8_set0:1;
	u8 reservrd9_set0:1; /* bit 30 */
	u8 pd_ams_change:1;
} __packed;

struct pd_status_info {
	struct pd_status_change status_change;
	u8 supply:1; /* byte 5 */
#define PD_STATUS_INFO_NO_SUPPULY 	0x0
#define PD_STATUS_INFO_SUPPULY 		0x1
	u8 port_op_mode:3;
#define PD_STATUS_INFO_NO_CONSUMER 	0x0
#define PD_STATUS_INFO_USB_DEFAULT 	0x1
#define PD_STATUS_INFO_BC 			0x2
#define PD_STATUS_INFO_PD 			0x3
#define PD_STATUS_INFO_USB_1_5A 	0x4
#define PD_STATUS_INFO_USB_3_0A 	0x5
	u8 insertion_detect:1;
#define PD_STATUS_INFO_NO_CABLE 	0x0
#define PD_STATUS_INFO_CABLE_DETECT 0x1
	u8 pd_capable_cable:1;
#define PD_STATUS_INFO_NO_PD_CABLE 	0x0
#define PD_STATUS_INFO_PD_CABLE 	0x1
	u8 power_direction:1;
#define PD_STATUS_INFO_CONSUMER 	0x0
#define PD_STATUS_INFO_PROVIDER 	0x1
	u8 connect_status:1;
#define PD_STATUS_INFO_UNATTACHED 	0x0
#define PD_STATUS_INFO_ATTACHED 	0x1
	u8 port_partner_flag; /* byte 6 */
#define PD_STATUS_INFO_PARTNER_USB 	0x0
#define PD_STATUS_INFO_PARTNER_ALT 	0x1
#define PD_STATUS_INFO_PARTNER_TBT 	0x2
#define PD_STATUS_INFO_PARTNER_USB4 	0x3
	uint32_t request_data_obj; /* byte 7-10*/
	u8 port_partner_type:3; /* byte 11 */
#define PD_STATUS_INFO_PARTNER_DFP 	0x1
#define PD_STATUS_INFO_PARTNER_UFP 	0x2
#define PD_STATUS_INFO_PARTNER_PC 	0x3
#define PD_STATUS_INFO_PARTNER_PC_UFP 	0x4
#define PD_STATUS_INFO_PARTNER_DA 	0x5
#define PD_STATUS_INFO_PARTNER_AAA 	0x6
	u8 battery_charging_status:2;
	u8 pd_sourcing_Vconn:1;
	u8 pd_responsible_for_Vconn:1;
	u8 pd_ams:1;
	u8 last_ams:4; /* byte 12 */
#define PD_STATUS_INFO_POWER_ROLE_SWAP 	0x1
#define PD_STATUS_INFO_DATA_ROLE_SWAP 	0x2
#define PD_STATUS_INFO_VCONN_SWAP 	0x3
#define PD_STATUS_INFO_POWER_NEGOTIATION	0x4
#define PD_STATUS_INFO_RESERVED 	0x5
#define PD_STATUS_INFO_SOFT_RESET	0x6
#define PD_STATUS_INFO_HARD_RESET	0x7
#define PD_STATUS_INFO_GOTO_MIN 	0x8
#define PD_STATUS_INFO_GET_SINK_CAPS 	0x9
#define PD_STATUS_INFO_GET_SOURCE_CAPS 	0xa
#define PD_STATUS_INFO_VDM 		0xb
	u8 port_partner_not_support_pd:1;
	u8 plug_direction:1;
	u8 dp_role:1;
#define PD_STATUS_INFO_DP_SINK 		0x0
#define PD_STATUS_INFO_DP_SOURCE 	0x1
	u8 pd_connect:1;
#define PD_STATUS_INFO_PD_NOT_EXCHANGE 0x0
#define PD_STATUS_INFO_PD_EXCHANGE 	0x1
	u8 vbsin_en_switch_status:2; /* byte 13 */
	u8 lp_en_switch_status:2;
	u8 cable_spec_version:2;
	u8 port_partner_spec_version:2;
	u8 alt_mode_status:3; /* byte 14 */
	u8 dp_lane_swap_setting:1;
	u8 contract_valid:1;
	u8 unchunked_extenfed_message_support:1;
	u8 fr_swap_suppprt:1;
	u8 reservrd14:1;
	u8 average_current_low_byte; /* byte 15 */
	u8 average_current_high_byte;
	u8 voltage_reading_low_byte;
	u8 voltage_reading_high_byte;
} __packed;

#define PDO_TYPE_(n) (n&0x1)
#define PDO_TYPE_SINK 0x0
#define PDO_TYPE_SOURCE 0x1
#define PDO_TCPM(n) ((n&0x1) << 1)
#define PDO_TCPM_TCPM 0x0
#define PDO_TCPM_PARTNER 0x1
#define PDO_OFFEST(n) ((n&0x7) << 2)
#define PDO_NUM(n) ((n&0x7) << 5)

struct source_fix_pdo {
	u16 max_current:10;
	u16 voltage:10;
	u8 peak_current:2;
	u16 reserved:3;
	u8 data_role_swap:1;
	u8 usb_comm_capable:1;
	u8 external_power:1;
	u8 usb_suspend:1;
	u8 dual_role_power:1;
	u8 power_type:2;
} __packed;

struct source_var_pdo {
	u16 max_current:10;
	u16 min_voltage:10;
	u16 max_voltage:10;
	u8 power_type:2;
} __packed;

struct source_bat_pdo {
	u16 max_current:10;
	u16 min_voltage:10;
	u16 max_voltage:10;
	u8 power_type:2;
} __packed;

struct sink_fix_pdo {
	u16 op_current:10;
	u16 voltage:10;
	u8 peak_current:2;
	u16 reserved:3;
	u8 data_role_swap:1;
	u8 usb_comm_capable:1;
	u8 external_power:1;
	u8 higher_capability:1;
	u8 dual_role_power:1;
	u8 power_type:2;
} __packed;

struct sink_var_pdo {
	u16 op_current:10;
	u16 min_voltage:10;
	u16 max_voltage:10;
	u8 power_type:2;
} __packed;

struct sink_bat_pdo {
	u16 op_current:10;
	u16 min_voltage:10;
	u16 max_voltage:10;
	u8 power_type:2;
} __packed;

struct common_pdo {
	uint32_t reservrd:30;
	u8 power_type:2;
#define PDO_INFO_POWER_TYPE_FIX 0x0
#define PDO_INFO_POWER_TYPE_BAT 0x1
#define PDO_INFO_POWER_TYPE_VAR 0x2
} __packed;

/* pdo: Power Data Object
 * three type:
 * 1. fixed supply
 * 2. variable supply
 * 3. Battery supply
 */
union pdo_info {
	struct source_fix_pdo so_f;
	struct source_var_pdo so_v;
	struct source_bat_pdo so_b;
	struct sink_fix_pdo si_f;
	struct sink_var_pdo si_v;
	struct sink_bat_pdo si_b;
	struct common_pdo com;
};

/* spr: Standard Power Range */
/* epr: Extended Power Range */
#define SET_SPR_PDO_NUM(x) ((x & 0x3))
#define SET_PDO_TYPE(x)    ((x & 0x1) << 3)
#define SET_EPR_PDO_NUM(x) ((x & 0x3) << 4)

/* rdo: Request Data Object */
struct rdo_info {
	uint32_t max_op_current:10;
	uint32_t op_current:10;
	uint32_t reservrd20:4;
	uint32_t no_usb_suspend:1;
	uint32_t usb_comm_cap:1;
	uint32_t cap_mismatch:1;
	uint32_t give_back_flag:1;
	uint32_t obj_pos:3;
	uint32_t reservrd:1;
} __packed;

struct vdm_header {
	uint32_t data; //TODO
} __packed;

struct vdo_info {
	uint32_t data; //TODO
} __packed;

/* ams: Automatic Messaging Sequences */
#define PD_AMS_PR_SWAP 		0x01
#define PD_AMS_DR_SWAP 		0x02
#define PD_AMS_VCONN_SWAP 	0x03
#define PD_AMS_SOURCE_CAP 	0x04
#define PD_AMS_REQUEST 		0x05
#define PD_AMS_SOFT_RESET 	0x06
#define PD_AMS_HARD_RESET 	0x07
#define PD_AMS_GOTO_MIN 	0x08
#define PD_AMS_GET_SNK_CAP 	0x09
#define PD_AMS_GET_SRC_CAP 	0x0A

struct rts54xx_dev {
	struct device *dev;
	struct i2c_client *client;
	struct ucsi *ucsi;
	bool no_device;
	struct mutex transfer_lock;

	struct pd_ic_info ic_info;
	struct pd_status_info status_info;

	unsigned long flags;
#define RESET_PENDING	0
#define DEV_CMD_PENDING	1

	struct i2c_client *alert;
	struct i2c_smbus_alert_setup alert_data;
	struct gpio_desc *irq_gpiod;
	struct mutex lock; /* to sync between user and driver thread */

	struct completion complete;

	u64 last_cmd_sent;
	u32 last_cmd_cci;

	struct delayed_work delayed_work;

	enum typec_orientation		orientation;
	struct typec_switch		*sw;
	struct typec_mux		*mux;

	struct dentry *debug_dir;
};

static void rtk_rts54xx_alert(struct i2c_client *client,
				enum i2c_alert_protocol protocol,
				unsigned int data);

/* I2C smbus transfer */

/* __rts54xx_get_ping_status - Get RTS54xx Ping Status
 * @client: Handle to slave device.
 *
 * Return: The TCPM command status by struct tcpm_command_status.
 */
static struct tcpm_command_status __rts54xx_get_ping_status(
		const struct i2c_client *client)
{
	int ret;
	struct tcpm_command_status status;
	status.cmd_status = 0;
	status.data_len = 0;

	dev_vdbg(&client->dev, "Enter %s\n", __func__);

	ret = i2c_smbus_read_byte(client);
	if (ret < 0) {
		dev_err(&client->dev, "%s get error %d\n", __func__, ret);
		return status;
	}
	status.cmd_status = PING_STATUS_CMD_STATUS(ret);
	status.data_len = PING_STATUS_DATA_LEN(ret);

	dev_vdbg(&client->dev, "Exit %s\n", __func__);

	return status;
}

/* __rts54xx_block_read - Read RTS54xx block data by command
 * @client: Handle to slave device.
 * @command: tcpm_command
 *
 * Return: negative errno else the number of data byte.
 */
static int __rts54xx_block_read(const struct i2c_client *client,
		struct tcpm_command *command)
{
	int ret;
	int data_len = command->data_len + 1;
	u8 *buffer;

	dev_vdbg(&client->dev, "Enter %s\n", __func__);

	buffer = kzalloc(data_len, GFP_KERNEL);

	ret = i2c_smbus_read_i2c_block_data(client, command->cmd,
			data_len, buffer);
	if (ret < 0)
		dev_err(&client->dev, "block read failed (ret=%d)\n", ret);
	else {
		if (ret != data_len)
			dev_warn(&client->dev, "read data len check: (ret=%d != data_len=%d\n",
				 ret, data_len);

		memcpy(&command->data, buffer + 1, command->data_len);
		ret = buffer[0];
	}

	kfree(buffer);

	dev_vdbg(&client->dev, "Exit %s\n", __func__);

	return ret;
}

/* __rts54xx_block_write - write RTS54xx block data by command
 * @client: Handle to slave device.
 * @command: tcpm_command
 *
 * Return: negative errno else zero on success.
 */
static int __rts54xx_block_write(struct i2c_client *client,
		struct tcpm_command *command)
{
	int ret;

	dev_vdbg(&client->dev, "Enter %s\n", __func__);

	ret = i2c_smbus_write_block_data(client, command->cmd,
			command->data_len, (const u8 *)&command->data);

	dev_vdbg(&client->dev, "Exit %s\n", __func__);

	return ret;
}

/* __rts54xx_transfer - run tcpm_command write, ping status and read
 * @rts54xx: Handle for device rts54xx
 * @ w_cmd: rts54xx smbus write tcpm_command
 * @ r_cmd: rts54xx smbus read tcpm_command
 *
 * Return: negative errno else zero on success.
 */
static int __rts54xx_transfer(struct rts54xx_dev *rts54xx,
		struct tcpm_command *w_cmd, struct tcpm_command *r_cmd)
{
	struct device *dev = rts54xx->dev;
	struct i2c_client *client = rts54xx->client;
	struct tcpm_command_status status;
	int retry = 100;
	int ret = -ENODEV;

	if (rts54xx->no_device) {
		dev_dbg(dev, "%s Error! NO rts54xx\n", __func__);
		return ret;
	}

	dev_dbg(dev, "%s start write command 0x%x, data_len %d, sub_command 0x%x\n",
			__func__, w_cmd->cmd, w_cmd->data_len, w_cmd->data[0]);
	print_hex_dump_debug("w_cmd data: ", DUMP_PREFIX_ADDRESS, 16, 1,
			     w_cmd->data, w_cmd->data_len, 1);

	mutex_lock(&rts54xx->transfer_lock);

	ret = __rts54xx_block_write(client, w_cmd);
	if (ret < 0) {
		dev_err(dev, "%s block write error %d", __func__, ret);
		goto out;
	}

	while (--retry) {
		mdelay(1);
		status = __rts54xx_get_ping_status(client);
		if (status.cmd_status == PING_STATUS_COMPLETE) {
			break;
		} else if (status.cmd_status == PING_STATUS_ERROR) {
			dev_err(dev, "%s get ping status error %d\n", __func__, status.cmd_status);
			break;
		}
	}
	if (!retry) {
		dev_err(dev, "%s get ping status TIMEOUT\n", __func__);
		ret = -ETIMEDOUT;
		goto out;
	} else {
		dev_dbg(dev, "%s get ping status 0x%x, data_len %d\n",
				__func__, status.cmd_status, status.data_len);
	}

	if (r_cmd->data_len == 0xff)
		r_cmd->data_len = status.data_len;

	if (r_cmd->data_len > 0) {
		dev_dbg(dev, "%s start read command 0x%x, data_len %d\n",
			__func__, r_cmd->cmd, r_cmd->data_len);

		ret = __rts54xx_block_read(client, r_cmd);
		if (ret < 0)
			dev_err(dev, "%s block read error %d\n", __func__, ret);
		else
			ret = 0;

		print_hex_dump_debug("r_cmd data: ", DUMP_PREFIX_ADDRESS, 16, 1,
				     r_cmd->data, r_cmd->data_len, 1);
	}

out:
	mutex_unlock(&rts54xx->transfer_lock);

	if (status.cmd_status == PING_STATUS_ERROR)
		return -PING_STATUS_ERROR;
	else
		return ret;
}

/* ------------------------------------------------------------ */
/* rts54xx command list */

static int rtk_rts54xx_vendor_command_enable(struct rts54xx_dev *rts54xx)
{
	struct device *dev = rts54xx->dev;
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = VENDOR_CMD_ENABLE.cmd;
	w_cmd.data_len = VENDOR_CMD_ENABLE.len;
	w_cmd.data[0] = VENDOR_CMD_ENABLE.sub;
	w_cmd.data[1] = 0x0b;
	w_cmd.data[2] = 0x01;
	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_vdbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_vdbg(dev, "Exit %s\n", __func__);

	return ret;
}

static int rtk_rts54xx_tcpm_reset(struct rts54xx_dev *rts54xx)
{
	struct device *dev = rts54xx->dev;
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = TCPM_RESET.cmd;
	w_cmd.data_len = TCPM_RESET.len;
	w_cmd.data[0] = TCPM_RESET.sub;
	w_cmd.data[1] = 0x00;
	w_cmd.data[2] = 0x01;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_vdbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s __rts54xx_transfer fails\n", __func__);
		return ret;
	}

	dev_vdbg(dev, "Exit %s\n", __func__);

	return ret;
}

static int rtk_rts54xx_set_notification(struct rts54xx_dev *rts54xx,
					u32 notification_en)
{
	struct device *dev = rts54xx->dev;
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = SET_NOTIFICATION_ENABLE.cmd;
	w_cmd.data_len = SET_NOTIFICATION_ENABLE.len;
	w_cmd.data[0] = SET_NOTIFICATION_ENABLE.sub;
	w_cmd.data[1] = 0x00;
	w_cmd.data[2] = (notification_en & 0x000000ff);
	w_cmd.data[3] = (notification_en & 0x0000ff00) >> 8;
	w_cmd.data[4] = (notification_en & 0x00ff0000) >> 16;
	w_cmd.data[5] = (notification_en & 0xff000000) >> 24;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_vdbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s __rts54xx_transfer fails\n", __func__);
		return ret;
	}

	dev_vdbg(dev, "Exit %s\n", __func__);

	return ret;
}

__maybe_unused
static int rtk_rts54xx_set_pdo(struct rts54xx_dev *rts54xx, int pdo_type,
			       int pdo_num, union pdo_info* pdo)
{
	struct device *dev = rts54xx->dev;
	struct tcpm_command w_cmd, r_cmd;
	int ret;
	int i;
	int spr_num = 0;
	int epr_num = 0;

	w_cmd.cmd = SET_PDO.cmd;
	w_cmd.data_len = SET_PDO.len + 4 * pdo_num;
	w_cmd.data[0] = SET_PDO.sub;
	w_cmd.data[1] = 0x00; /* PORT_NUM */
	w_cmd.data[2] = SET_SPR_PDO_NUM(spr_num) | SET_EPR_PDO_NUM(epr_num) |
			SET_PDO_TYPE(pdo_type);

	for (i = 3; i < w_cmd.data_len; i++) {
		char *tmp = (char *)pdo;

		w_cmd.data[i] = tmp[i - 3];
	}

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s __rts54xx_transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(dev, "Exit %s\n", __func__);

	return ret;
}

__maybe_unused
static int rtk_rts54xx_set_rdo(struct rts54xx_dev *rts54xx, struct rdo_info *rdo)
{
	struct device *dev = rts54xx->dev;
	struct tcpm_command w_cmd, r_cmd;
	int ret;
	int i;

	w_cmd.cmd = SET_RDO.cmd;
	w_cmd.data_len = SET_RDO.len;
	w_cmd.data[0] = SET_RDO.sub;
	w_cmd.data[1] = 0x00; /* PORT_NUM */

	for (i = 2; i < w_cmd.data_len; i++) {
		char *tmp = (char *)rdo;

		dev_dbg(dev, "tmp --> %x\n", (int)tmp[i - 2]);
		w_cmd.data[i] = tmp[i - 2];
	}

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s __rts54xx_transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(dev, "Exit %s\n", __func__);

	return ret;
}

__maybe_unused
static int rtk_rts54xx_auto_set_rdo(struct rts54xx_dev *rts54xx, bool high_pdo)
{
	struct device *dev = rts54xx->dev;
	struct tcpm_command w_cmd, r_cmd;
	int ret;

	w_cmd.cmd = AUTO_SET_RDO.cmd;
	w_cmd.data_len = AUTO_SET_RDO.len;
	w_cmd.data[0] = AUTO_SET_RDO.sub;
	w_cmd.data[1] = 0x00; /* PORT_NUM */
	w_cmd.data[2] = high_pdo ? 0x1 : 0x0;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s __rts54xx_transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(dev, "Exit %s\n", __func__);

	return ret;
}

/* set type c Rp resistor on cc*/
__maybe_unused
static int rtk_rts54xx_set_tpc_rp(struct rts54xx_dev *rts54xx, int tpc_rp, int pd_rp)
{
	struct device *dev = rts54xx->dev;
	struct tcpm_command w_cmd, r_cmd;
	int ret;

	w_cmd.cmd = SET_TPC_RP.cmd;
	w_cmd.data_len = SET_TPC_RP.len;
	w_cmd.data[0] = SET_TPC_RP.sub;
	w_cmd.data[1] = 0x00; /* PORT_NUM */

#define TPC_RP(x) ((x & 0x3) << 2)
#define PD_RP(x) ((x & 0x3) << 4)
	w_cmd.data[2] = TPC_RP(tpc_rp) | PD_RP(pd_rp);

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s __rts54xx_transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(dev, "Exit %s\n", __func__);

	return ret;

}

/* vdm: vendor defined message */
__maybe_unused
static int rtk_rts54xx_send_vdm(struct rts54xx_dev *rts54xx,
				struct vdm_header *vdm_header)
{
	struct device *dev = rts54xx->dev;
	struct tcpm_command w_cmd, r_cmd;
	int ret;
	int i;

	w_cmd.cmd = SEND_VDM.cmd;
	w_cmd.data_len = SEND_VDM.len;
	w_cmd.data[0] = SEND_VDM.sub;
	w_cmd.data[1] = 0x00; /* PORT_NUM */

	for (i = 0; i < 4; i++) {
		char *tmp = (char*)vdm_header;
		dev_dbg(dev, "tmp[%d]=%x\n", i, tmp[i]);
		w_cmd.data[i + 2] = tmp[i];
	}

	/* TODO VDM data */

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s __rts54xx_transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(dev, "Exit %s\n", __func__);

	return ret;
}

__maybe_unused
static int rtk_rts54xx_set_vdo(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_tpc_csd_operation_mode(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_set_tpc_reconnect(struct rts54xx_dev *rts54xx)
{
	return 0;
}

static int rtk_rts54xx_init_pd_ams(struct rts54xx_dev *rts54xx, u8 pd_ams)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = INIT_PD_AMS.cmd;
	w_cmd.data_len = INIT_PD_AMS.len;
	w_cmd.data[0] = INIT_PD_AMS.sub;
	w_cmd.data[1] = 0;
	w_cmd.data[2] = pd_ams;
	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(rts54xx->dev, "Enter %s pd_ams=%x\n", __func__, pd_ams);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(rts54xx->dev, "Exit %s\n", __func__);

	return ret;
}

__maybe_unused
static int rtk_rts54xx_force_set_power_switch(struct rts54xx_dev *rts54xx,
					      bool vbsin_ctrl, int vbsin_en,
					      bool lp_ctrl, int lp_en)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = FORCE_SET_POWER_SWITCH.cmd;
	w_cmd.data_len = FORCE_SET_POWER_SWITCH.len;
	w_cmd.data[0] = FORCE_SET_POWER_SWITCH.sub;
	w_cmd.data[1] = 0;
	w_cmd.data[2] = 0;
#define VBSIN_EN(x) ((x & 0x3) | BIT(6))
#define LP_EN(x) ((x & 0x3) << 2 | BIT(7))
	if (vbsin_ctrl)
		w_cmd.data[2] |= VBSIN_EN(vbsin_en);
	if (lp_ctrl)
		w_cmd.data[2] |= LP_EN(lp_en);

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(rts54xx->dev, "Exit %s\n", __func__);

	return ret;
}

__maybe_unused
static int rtk_rts54xx_set_tpc_disconnect(struct rts54xx_dev *rts54xx)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = SET_TPC_DISCONNECT.cmd;
	w_cmd.data_len = SET_TPC_DISCONNECT.len;
	w_cmd.data[0] = SET_TPC_DISCONNECT.sub;
	w_cmd.data[1] = 0;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x0;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(rts54xx->dev, "Exit %s\n", __func__);

	return ret;
}

__maybe_unused
static int rtk_rts54xx_ack_power_ctrl_req(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_set_bypass_svid(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_set_bbr_cts(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_set_bc12(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_set_sys_pwr_state(struct rts54xx_dev *rts54xx)
{
	return 0;
}

static int rtk_rts54xx_get_pdo(struct rts54xx_dev *rts54xx,
			       int pdo_type, int pdo, int offset, int num,
			       u8 *value)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;
	union pdo_info *pdo_inf;
	int i;

	w_cmd.cmd = GET_PDOS.cmd;
	w_cmd.data_len = GET_PDOS.len;
	w_cmd.data[0] = GET_PDOS.sub;
	w_cmd.data[1] = 0;
	w_cmd.data[2] = PDO_TYPE_(pdo_type) | PDO_TCPM(pdo) | PDO_OFFEST(offset) | PDO_NUM(num);
	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(rts54xx->dev, "[PDO status] %s %s PDO, offset %d, num %d\n",
			pdo?"Partner":"TCPM", pdo_type?"Source":"Sink", offset, num);

	for (i = 0; i < r_cmd.data_len; i = i + 4) {
		pdo_inf = (union pdo_info *)&r_cmd.data[i];

		if (pdo == PDO_TYPE_SOURCE && pdo_inf->com.power_type == PDO_INFO_POWER_TYPE_FIX) {
			dev_dbg(rts54xx->dev, "[PDO status] %s Fix PDO, voltage %d mV\n",
				pdo_type?"Source":"Sink", (pdo_inf->so_f.voltage) * 50);
		} else if (pdo == PDO_TYPE_SOURCE && pdo_inf->com.power_type == PDO_INFO_POWER_TYPE_VAR) {
			dev_dbg(rts54xx->dev, "[PDO status] %s Var PDO\n", pdo_type?"Source":"Sink");
		} else if (pdo == PDO_TYPE_SOURCE && pdo_inf->com.power_type == PDO_INFO_POWER_TYPE_BAT) {
			dev_dbg(rts54xx->dev, "[PDO status] %s Bat PDO\n", pdo_type?"Source":"Sink");
		} else if (pdo == PDO_TYPE_SINK && pdo_inf->com.power_type == PDO_INFO_POWER_TYPE_FIX) {
			dev_dbg(rts54xx->dev, "[PDO status] %s Fix PDO, voltage %d mV\n",
				pdo_type?"Source":"Sink", (pdo_inf->si_f.voltage) * 50);
		} else if (pdo == PDO_TYPE_SINK && pdo_inf->com.power_type == PDO_INFO_POWER_TYPE_VAR) {
			dev_dbg(rts54xx->dev, "[PDO status] %s Var PDO\n", pdo_type?"Source":"Sink");
		} else if (pdo == PDO_TYPE_SINK && pdo_inf->com.power_type == PDO_INFO_POWER_TYPE_BAT) {
			dev_dbg(rts54xx->dev, "[PDO status] %s Var PDO\n", pdo_type?"Source":"Sink");
		}
	}
	dev_dbg(rts54xx->dev, "Exit %s return cmd %x, data_len %d\n", __func__,
			r_cmd.cmd, r_cmd.data_len);

	if (ret >= 0) {
		if (value != NULL) {
			memcpy(value, &r_cmd.data, r_cmd.data_len);
		}
		return r_cmd.data_len;
	}
	return ret;
}

__maybe_unused
static int rtk_rts54xx_get_rdo(struct rts54xx_dev *rts54xx, struct rdo_info *rdo)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = GET_RDO.cmd;
	w_cmd.data_len = GET_RDO.len;
	w_cmd.data[0] = GET_RDO.sub;
	w_cmd.data[1] = 0;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(rts54xx->dev, "Exit %s return cmd %x, data_len %d\n", __func__,
			r_cmd.cmd, r_cmd.data_len);

	if (ret >= 0) {
		if (rdo != NULL) {
			memcpy(rdo, &r_cmd.data, r_cmd.data_len);
		}
		return r_cmd.data_len;
	}
	return ret;
}

__maybe_unused
static int rtk_rts54xx_get_tpc_rp(struct rts54xx_dev *rts54xx)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = GET_TPC_RP.cmd;
	w_cmd.data_len = GET_TPC_RP.len;
	w_cmd.data[0] = GET_TPC_RP.sub;
	w_cmd.data[1] = 0;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(rts54xx->dev, "Exit %s return cmd %x, data_len %d\n", __func__,
			r_cmd.cmd, r_cmd.data_len);

	//TODO return tcp_rp

	if (ret > 0) {
//		if (rdo != NULL) {
//			memcpy(rdo, &r_cmd.data, r_cmd.data_len);
//		}
		return r_cmd.data_len;
	}
	return ret;
}

__maybe_unused
static int rtk_rts54xx_get_vdm(struct rts54xx_dev *rts54xx)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = GET_VDM.cmd;
	w_cmd.data_len = GET_VDM.len;
	w_cmd.data[0] = GET_VDM.sub;
	w_cmd.data[1] = 0;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(rts54xx->dev, "Exit %s return cmd %x, data_len %d\n", __func__,
			r_cmd.cmd, r_cmd.data_len);

	//TODO return vdm

	if (ret > 0) {
//		if (vdm != NULL) {
//			memcpy(vdm, &r_cmd.data, r_cmd.data_len);
//		}
		return r_cmd.data_len;
	}
	return ret;
}

__maybe_unused
static int rtk_rts54xx_get_vdo(struct rts54xx_dev *rts54xx)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = GET_VDO.cmd;
	w_cmd.data_len = GET_VDO.len;
	w_cmd.data[0] = GET_VDO.sub;
	w_cmd.data[1] = 0;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_dbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	dev_dbg(rts54xx->dev, "Exit %s return cmd %x, data_len %d\n", __func__,
			r_cmd.cmd, r_cmd.data_len);

	//TODO return vdo

	if (ret > 0) {
//		if (vdm != NULL) {
//			memcpy(vdm, &r_cmd.data, r_cmd.data_len);
//		}
		return r_cmd.data_len;
	}
	return ret;
}

__maybe_unused
static int rtk_rts54xx_get_tpc_csd_operation_mode(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_get_amd_status(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_get_csd(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_get_bypass_svid(struct rts54xx_dev *rts54xx)
{
	return 0;
}


__maybe_unused
static int rtk_rts54xx_get_current_partner_src_pdo(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_get_data_message(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_get_power_switch_status(struct rts54xx_dev *rts54xx)
{
	return 0;
}

__maybe_unused
static int rtk_rts54xx_get_epr_pdo(struct rts54xx_dev *rts54xx)
{
	return 0;
}

static int rtk_rts54xx_get_status(struct rts54xx_dev *rts54xx, u8 offset, u8 len,
				  struct pd_status_info *status_info)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = GET_RTK_STATUS.cmd;
	w_cmd.data_len = GET_RTK_STATUS.len;
	w_cmd.data[0] = offset;//0x0;
	w_cmd.data[1] = 0;
	w_cmd.data[2] = len;//0x14;
	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_vdbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	memset(status_info, 0, sizeof(struct pd_status_info));
	memcpy(status_info, &r_cmd.data, r_cmd.data_len);

	ret = r_cmd.data_len;

	dev_vdbg(rts54xx->dev, "Exit %s return cmd %x, data_len %d\n", __func__,
			r_cmd.cmd, r_cmd.data_len);

	return ret;
}

static int rtk_rts54xx_get_status_dump(struct rts54xx_dev *rts54xx)
{
	int ret = 0;
	struct pd_status_info status_info;

	dev_vdbg(rts54xx->dev, "Enter %s\n", __func__);

	ret = rtk_rts54xx_get_status(rts54xx, 0, 14, &status_info);
	if (ret < 0) {
		dev_err(rts54xx->dev, "rtk_rts54xx_get_status fail (ret=%d)\n", ret);
		return -ENODEV;
	}

	dev_info(rts54xx->dev, "[Status Info] status_change:\n");
	dev_info(rts54xx->dev, "[Status Info]     extenal_supply_change=%x\n",
		 status_info.status_change.extenal_supply_change);
	dev_info(rts54xx->dev, "[Status Info]     power_operatin_mode_change=%x\n",
		 status_info.status_change.power_operatin_mode_change);
	dev_info(rts54xx->dev, "[Status Info]     supported_provider_capabilities_change=%x\n",
		 status_info.status_change.supported_provider_capabilities_change);

	dev_info(rts54xx->dev, "[Status Info] Externally Powered supply %d, Port OP mode %d, Insertion Detect %d\n",
			status_info.supply, status_info.port_op_mode, status_info.insertion_detect);
	dev_info(rts54xx->dev, "[Status Info] PD Capable Cable %d, Power Direction %d\n",
			status_info.pd_capable_cable, status_info.power_direction);
	dev_info(rts54xx->dev, "[Status Info] Connect Status %d, Port Partner Flags %d\n",
			status_info.connect_status, status_info.port_partner_flag);
	dev_info(rts54xx->dev, "[Status Info] Request Data Object 0x%x\n", status_info.request_data_obj);
	dev_info(rts54xx->dev, "[Status Info] Port Partner Type %d\n", status_info.port_partner_type);
	dev_info(rts54xx->dev, "[Status Info] Battery Charging Status %d\n", status_info.battery_charging_status);
	dev_info(rts54xx->dev, "[Status Info] PD Sourcing Vconn %d\n", status_info.pd_sourcing_Vconn);
	dev_info(rts54xx->dev, "[Status Info] PD AMS is %s\n", status_info.pd_ams?"in progress":"ready");
	dev_info(rts54xx->dev, "[Status Info] Last PD AMS is %x\n", status_info.last_ams);
	if (status_info.port_partner_not_support_pd)
		dev_info(rts54xx->dev, "[Status Info] Port Partner NOT support PD\n");
	dev_info(rts54xx->dev, "[Status Info] Plug Direction: %s\n", status_info.plug_direction?"Reverse":"Obverse");
	dev_info(rts54xx->dev, "[Status Info] DP Role: %s\n", status_info.dp_role?"Sink":"Source");
	dev_info(rts54xx->dev, "[Status Info] PD %sexchanged pd message and goodcrc\n", status_info.pd_connect?"":"NOT ");
	dev_info(rts54xx->dev, "[Status Info] Alt mode status 0x%x\n", status_info.alt_mode_status);
	dev_info(rts54xx->dev, "[Status Info] DP lane swap setting: %s\n", status_info.dp_lane_swap_setting?"SWAP":"Not SWAP");

	return ret;
}

static int rtk_rts54xx_ucsi_ack_cc_ci(struct rts54xx_dev *rts54xx)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = ACK_CC_CI.cmd;
	w_cmd.data_len = ACK_CC_CI.len;
	w_cmd.data[0] = ACK_CC_CI.sub;
	w_cmd.data[1] = 0x0;
	w_cmd.data[2] = 0xff;
	w_cmd.data[3] = 0xff;
	w_cmd.data[4] = 0xff;
	w_cmd.data[5] = 0xff;
	w_cmd.data[6] = 0x1;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	return ret;
}

static int rtk_rts54xx_ppm_reset(struct rts54xx_dev *rts54xx)
{
	struct device *dev = rts54xx->dev;
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = RTS_UCSI_CMD[UCSI_PPM_RESET].cmd;
	w_cmd.data_len = RTS_UCSI_CMD[UCSI_PPM_RESET].len;
	w_cmd.data[0] = RTS_UCSI_CMD[UCSI_PPM_RESET].sub;
	w_cmd.data[1] = 0x00;
	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	dev_vdbg(dev, "Enter %s\n", __func__);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(dev, "%s __rts54xx_transfer fails\n", __func__);
		return ret;
	}

	dev_vdbg(dev, "Exit %s\n", __func__);

	return ret;
}

static int rtk_rts54xx_get_ic_info(struct rts54xx_dev *rts54xx)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;
	struct pd_ic_info *ic_info = &rts54xx->ic_info;

	dev_vdbg(rts54xx->dev, "Enter %s\n", __func__);

	w_cmd.cmd = GET_IC_STATUS.cmd;
	w_cmd.data_len = GET_IC_STATUS.len;
	w_cmd.data[0] = 0x0;
	w_cmd.data[1] = 0x0;
	w_cmd.data[2] = 0x14;
	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0x14;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	memcpy(ic_info, &r_cmd.data, r_cmd.data_len);

	dev_info(rts54xx->dev, "[IC info] VID=0x%x, PID=0x%x\n",
		 ic_info->vid, ic_info->pid);
	dev_info(rts54xx->dev, "[IC info] RTS54xx Main_version=%d Sub_version1=%d Sub_version2=%d\n",
		 ic_info->fw_main_ver, ic_info->fw_sub_ver1, ic_info->fw_sub_ver2);
	dev_info(rts54xx->dev, "[IC info] PD major revision %d minor revision %d\n",
		 ic_info->pd_reversion_major, ic_info->pd_reversion_minor);
	dev_info(rts54xx->dev, "[IC info] PD major version %d minor version %d\n",
		 ic_info->pd_version_major, ic_info->pd_version_minor);
	dev_info(rts54xx->dev, "[IC info] PD %sready, TypeC %sconnect\n",
		 IC_STATUS_PD_READY(ic_info->pd_tc)?"":"NOT ",
		 IC_STATUS_TC_CONNECT(ic_info->pd_tc)?"":"NOT ");

	dev_vdbg(rts54xx->dev, "Exit %s return ret=%d, cmd %x, data_len %d\n", __func__,
			ret, r_cmd.cmd, r_cmd.data_len);

	return ret;
}

__maybe_unused
static int rtk_rts54xx_ucsi_get_capability(struct rts54xx_dev *rts54xx)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = RTS_UCSI_CMD[UCSI_GET_CAPABILITY].cmd;
	w_cmd.data_len = RTS_UCSI_CMD[UCSI_GET_CAPABILITY].len;
	w_cmd.data[0] = RTS_UCSI_CMD[UCSI_GET_CAPABILITY].sub;
	w_cmd.data[1] = 0x0;
	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	}

	ret = rtk_rts54xx_ucsi_ack_cc_ci(rts54xx);
	if (ret < 0)
		return ret;

	return ret;
}

static int rtk_rts54xx_ucsi_get_alt_mode(struct rts54xx_dev *rts54xx,
					 int recipient, int offset, int alt_num,
					 u8 *value)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = RTS_UCSI_CMD[UCSI_GET_ALTERNATE_MODES].cmd;
	w_cmd.data_len = RTS_UCSI_CMD[UCSI_GET_ALTERNATE_MODES].len;
	w_cmd.data[0] = RTS_UCSI_CMD[UCSI_GET_ALTERNATE_MODES].sub;
	w_cmd.data[1] = 0x0;
	w_cmd.data[2] = recipient & 0x7;
	w_cmd.data[3] = (u8)offset;
	w_cmd.data[4] = alt_num & 0x3;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	} else {
		if (value != NULL) {
			memcpy(value, &r_cmd.data, r_cmd.data_len);
		}
		return r_cmd.data_len;
	}

	return ret;
}

static int rtk_rts54xx_ucsi_get_cam_supported(struct rts54xx_dev *rts54xx, int *value)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = RTS_UCSI_CMD[UCSI_GET_CAM_SUPPORTED].cmd;
	w_cmd.data_len = RTS_UCSI_CMD[UCSI_GET_CAM_SUPPORTED].len;
	w_cmd.data[0] = RTS_UCSI_CMD[UCSI_GET_CAM_SUPPORTED].sub;
	w_cmd.data[1] = 0x0;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	} else {
		if (value != NULL) {
			memcpy(value, &r_cmd.data, r_cmd.data_len);
		}
		return r_cmd.data_len;
	}

	return ret;
}

static int rtk_rts54xx_ucsi_get_current_cam(struct rts54xx_dev *rts54xx, u8 *value)
{
	int ret;
	struct tcpm_command w_cmd, r_cmd;

	w_cmd.cmd = RTS_UCSI_CMD[UCSI_GET_CURRENT_CAM].cmd;
	w_cmd.data_len = RTS_UCSI_CMD[UCSI_GET_CURRENT_CAM].len;
	w_cmd.data[0] = RTS_UCSI_CMD[UCSI_GET_CURRENT_CAM].sub;
	w_cmd.data[1] = 0x0;

	r_cmd.cmd = 0x80;
	r_cmd.data_len = 0xff;
	memset(&r_cmd.data, 0, RTS54XX_MAX_WRITE_DATA_LEN);

	ret = __rts54xx_transfer(rts54xx, &w_cmd, &r_cmd);
	if (ret < 0) {
		dev_err(rts54xx->dev, "%s I2C transfer fails\n", __func__);
		return ret;
	} else {
		if (value != NULL) {
			memcpy(value, &r_cmd.data, r_cmd.data_len);
		}
		return r_cmd.data_len;
	}

	return ret;
}

static int rtk_rts54xx_get_cam_state(struct rts54xx_dev *rts54xx,
				     struct typec_mux_state *state)
{
	struct ucsi_altmode alt;
	int ret;
	u8 current_cam = 0;
	u16 svid = 0;
	u32 vdo = 0;

	/* default mode set TYPEC_STATE_USB */
	state->mode = TYPEC_STATE_USB;

	ret = rtk_rts54xx_ucsi_get_current_cam(rts54xx, &current_cam);
	if (ret < 0) {
		dev_err(rts54xx->dev, "Get current connector ALT mode error: ret = %d\n",
			(int)ret);
		return ret;
	} else {
		dev_dbg(rts54xx->dev, "Get current  connector ALT mode: 0x%x\n",
			(int)current_cam);
	}

	memset(&alt, 0, sizeof(alt));

	if (current_cam > 0) {
		ret = rtk_rts54xx_ucsi_get_alt_mode(rts54xx, UCSI_RECIPIENT_SOP,
					    current_cam - 1, 0, (u8 *)&alt);
		if (ret < 0) {
			dev_err(rts54xx->dev, "Get current#%d ALT mode error: ret = %d\n",
				(int)current_cam, (int)ret);
			return ret;
		} else {
			svid = alt.svid;
			vdo = alt.mid;
		}
	}

	if (svid && vdo) {
		int pin_assign;

		dev_dbg(rts54xx->dev, "SVID=%x VDO=0x%x\n", svid, vdo);

		if (DP_CAP_CAPABILITY(vdo) == DP_CAP_UFP_D)
			pin_assign = DP_CAP_PIN_ASSIGN_UFP_D(vdo);
		else if (DP_CAP_CAPABILITY(vdo) == DP_CAP_DFP_D)
			pin_assign = DP_CAP_PIN_ASSIGN_DFP_D(vdo);
		else if (DP_CAP_CAPABILITY(vdo) == DP_CAP_DFP_D_AND_UFP_D)
			pin_assign = DP_CAP_PIN_ASSIGN_DFP_D(vdo);
		else
			pin_assign = 0;

		dev_dbg(rts54xx->dev, "SVID=%x VDO=0x%x ==> pin_assign=0x%x\n",
			svid, vdo, pin_assign);

		if (pin_assign & BIT(DP_PIN_ASSIGN_D))
			/* Set type c 2 usb + 2 dp */
			state->mode = TYPEC_DP_STATE_D;
		else if (pin_assign & BIT(DP_PIN_ASSIGN_C))
			/* Set type c 4 lane dp */
			state->mode = TYPEC_DP_STATE_C;
		else if (pin_assign & BIT(DP_PIN_ASSIGN_E))
			/* set dp */
			state->mode = TYPEC_DP_STATE_E;
		else
			state->mode = TYPEC_STATE_USB;
	}

	return 0;
}

/* Type C port Manager*/
__maybe_unused
static bool rtk_rts54xx_is_enabled(struct rts54xx_dev *rts54xx)
{
	return !rts54xx->no_device;
}

__maybe_unused
static bool rtk_rts54xx_is_ufp_attached(struct rts54xx_dev *rts54xx)
{
	struct pd_status_info *status_info = &rts54xx->status_info;

	return status_info->port_partner_type == PD_STATUS_INFO_PARTNER_UFP;
}

int rtk_rts54xx_set_type_c_soft_reset(struct rts54xx_dev *rts54xx)
{
	struct device *dev = rts54xx->dev;
	int ret = 0;

	dev_vdbg(dev, "Enter %s\n", __func__);
	ret = rtk_rts54xx_init_pd_ams(rts54xx, PD_AMS_SOFT_RESET);

	dev_vdbg(dev, "Exit %s\n", __func__);
	return ret;
}

__maybe_unused
int rtk_rts54xx_get_current_pdo(struct rts54xx_dev *rts54xx, union pdo_info *cur_pdo)
{
	struct device *dev = rts54xx->dev;
	struct pd_status_info status_info;
	struct rdo_info *rdo = NULL;
	union pdo_info *pdo = NULL;
	int offset;
	int ret = -1;
	u8 *buffer;

	dev_vdbg(dev, "Enter %s\n", __func__);

	ret = rtk_rts54xx_get_status(rts54xx, 0, 14, &status_info);
	if (ret < 0) {
		dev_err(rts54xx->dev, "rtk_rts54xx_get_status fail (ret=%d)\n", ret);
		return -ENODEV;
	}

	if (status_info.request_data_obj == 0x0) {
		dev_err(dev, "%s NO request_data_obj", __func__);
		return 0;
	} else
		rdo = (struct rdo_info *)&status_info.request_data_obj;

	offset = (rdo->obj_pos - 1) * 4;

	dev_dbg(dev, "%s rdo->obj_pos=%d, offset=%d", __func__,
			rdo->obj_pos, offset);

	if (offset < 0) {
		dev_err(dev, "%s Error! rdo->obj_pos=%d, offset=%d",
			__func__, rdo->obj_pos, offset);
		return 0;
	}

	buffer = kzalloc(7*4, GFP_KERNEL);

	ret = rtk_rts54xx_get_pdo(rts54xx, PDO_TYPE_SOURCE, PDO_TCPM_PARTNER, 0, 7, buffer);
	if (ret < 0) {
		dev_err(dev, "%s rtk_rts54xx_get_pdo fail(ret=%d)", __func__, ret);
		kfree(buffer);
		return ret;
	}
	pdo = (union pdo_info *)(buffer + offset);
	if (pdo) {
		memcpy(cur_pdo, pdo, sizeof(union pdo_info));

		if (pdo->com.power_type == PDO_INFO_POWER_TYPE_FIX)
			dev_dbg(dev, "Current Partner Source Fix PDO, "
				 "voltage %d mV\n", (pdo->so_f.voltage) * 50);
		else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_VAR)
			dev_dbg(dev, "[PDO status] Var PDO\n");
		else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_BAT)
			dev_dbg(dev, "[PDO status] Bat PDO\n");
	}

	kfree(buffer);
	dev_vdbg(dev, "Exit %s\n", __func__);

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *create_debug_root(void)
{
	struct dentry *debug_root;

	debug_root = debugfs_lookup("ucsi", usb_debug_root);
	if (!debug_root)
		debug_root = debugfs_create_dir("ucsi", usb_debug_root);

	return debug_root;
}

static int rts54xx_get_ic_info_show(struct seq_file *s, void *unused)
{
	struct rts54xx_dev *rts54xx = s->private;
	struct pd_ic_info *ic_info;
	int ret;

	ret = rtk_rts54xx_get_ic_info(rts54xx);
	if (ret < 0) {
		dev_err(rts54xx->dev, "rtk_rts54xx_get_ic_info fail (ret=%d)\n", ret);
		return -ENODEV;
	}
	ic_info = &rts54xx->ic_info;

	seq_printf(s, "[IC info] VID=0x%x, PID=0x%x\n",
		 ic_info->vid, ic_info->pid);
	seq_printf(s, "[IC info] RTS54xx Main_version=%d Sub_version1=%d Sub_version2=%d\n",
		 ic_info->fw_main_ver, ic_info->fw_sub_ver1, ic_info->fw_sub_ver2);
	seq_printf(s, "[IC info] PD major revision %d minor revision %d\n",
		 ic_info->pd_reversion_major, ic_info->pd_reversion_minor);
	seq_printf(s, "[IC info] PD major version %d minor version %d\n",
		 ic_info->pd_version_major, ic_info->pd_version_minor);
	seq_printf(s, "[IC info] PD %sready, TypeC %sconnect\n",
		 IC_STATUS_PD_READY(ic_info->pd_tc)?"":"NOT ",
		 IC_STATUS_TC_CONNECT(ic_info->pd_tc)?"":"NOT ");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(rts54xx_get_ic_info);

static int rts54xx_get_status_info_show(struct seq_file *s, void *unused)
{
	struct rts54xx_dev *rts54xx = s->private;
	struct pd_status_info status_info;
	int ret;

	ret = rtk_rts54xx_get_status(rts54xx, 0, 14, &status_info);
	if (ret < 0) {
		dev_err(rts54xx->dev, "rtk_rts54xx_get_status fail (ret=%d)\n", ret);
		return -ENODEV;
	}
	seq_printf(s, "[Status Info] Externally Powered supply %d, Port OP mode %d, Insertion Detect %d\n",
			status_info.supply, status_info.port_op_mode, status_info.insertion_detect);
	seq_printf(s, "[Status Info] PD Capable Cable %d, Power Direction %d\n",
			status_info.pd_capable_cable, status_info.power_direction);
	seq_printf(s, "[Status Info] Connect Status %d, Port Partner Flags %d\n",
			status_info.connect_status, status_info.port_partner_flag);
	seq_printf(s, "[Status Info] Request Data Object 0x%x\n", status_info.request_data_obj);
	seq_printf(s, "[Status Info] Port Partner Type %d\n", status_info.port_partner_type);
	seq_printf(s, "[Status Info] Battery Charging Status %d\n", status_info.battery_charging_status);
	seq_printf(s, "[Status Info] PD Sourcing Vconn %d\n", status_info.pd_sourcing_Vconn);
	seq_printf(s, "[Status Info] PD AMS is %s\n", status_info.pd_ams?"in progress":"ready");
	seq_printf(s, "[Status Info] Last PD AMS is %x\n", status_info.last_ams);
	if (status_info.port_partner_not_support_pd)
		seq_printf(s, "[Status Info] Port Partner NOT support PD\n");
	seq_printf(s, "[Status Info] Plug Direction: %s\n", status_info.plug_direction?"Reverse":"Obverse");
	seq_printf(s, "[Status Info] DP Role: %s\n", status_info.dp_role?"Sink":"Source");
	seq_printf(s, "[Status Info] PD %sexchanged pd message and goodcrc\n", status_info.pd_connect?"":"NOT ");
	seq_printf(s, "[Status Info] Alt mode status 0x%x\n", status_info.alt_mode_status);
	seq_printf(s, "[Status Info] DP lane swap setting: %s\n", status_info.dp_lane_swap_setting?"SWAP":"Not SWAP");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(rts54xx_get_status_info);

static int rts54xx_get_pdo_show(struct seq_file *s, void *unused)
{
	struct rts54xx_dev *rts54xx = s->private;
	union pdo_info *pdo;
	int pdo_num_max = 7;
	int length = 0;
	int i, ret;
	u8 *buffer;

	buffer = kzalloc(pdo_num_max * sizeof(u32), GFP_KERNEL);

	ret = rtk_rts54xx_get_pdo(rts54xx, PDO_TYPE_SOURCE, PDO_TCPM_TCPM,
				  0, pdo_num_max, buffer);
	if (ret < 0) {
		seq_printf(s, "get_pdo (PDO_TYPE_SOURCE, PDO_TCPM_TCPM) fail (ret=%d)\n", ret);
	} else {
		length = ret;

		seq_printf(s, "PDO info PDO_TYPE_SOURCE of TCPM\n");
		for (i = 0; i < length; i = i + 4) {
			pdo = (union pdo_info *)&buffer[i];

			if (pdo->com.power_type == PDO_INFO_POWER_TYPE_FIX)
				seq_printf(s, "[PDO status] Fix PDO, voltage %d mV\n",
					   (pdo->so_f.voltage) * 50);
			else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_VAR)
				seq_printf(s, "[PDO status] Var PDO\n");
			else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_BAT)
				seq_printf(s, "[PDO status] Bat PDO\n");
		}
	}

	memset(buffer, 0, pdo_num_max * sizeof(u32));
	ret = rtk_rts54xx_get_pdo(rts54xx, PDO_TYPE_SINK, PDO_TCPM_TCPM,
				  0, pdo_num_max, buffer);
	if (ret < 0) {
		seq_printf(s, "get_pdo (PDO_TYPE_SINK, PDO_TCPM_TCPM) fail (ret=%d)\n", ret);
	} else {
		length = ret;

		seq_printf(s, "PDO info PDO_TYPE_SINK of TCPM\n");
		for (i = 0; i < length; i = i + 4) {
			pdo = (union pdo_info *)&buffer[i];

			if (pdo->com.power_type == PDO_INFO_POWER_TYPE_FIX)
				seq_printf(s, "[PDO status] Fix PDO, voltage %d mV\n",
					   (pdo->si_f.voltage) * 50);
			else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_VAR)
				seq_printf(s, "[PDO status] Var PDO\n");
			else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_BAT)
				seq_printf(s, "[PDO status] Var PDO\n");
		}
	}

	memset(buffer, 0, pdo_num_max * sizeof(u32));
	ret = rtk_rts54xx_get_pdo(rts54xx, PDO_TYPE_SOURCE, PDO_TCPM_PARTNER,
				  0, pdo_num_max, buffer);
	if (ret < 0) {
		seq_printf(s, "get_pdo (PDO_TYPE_SOURCE, PDO_TCPM_PARTNER) fail (ret=%d)\n", ret);
	} else {
		length = ret;

		seq_printf(s, "PDO info PDO_TYPE_SOURCE of PARTNER\n");
		for (i = 0; i < length; i = i + 4) {
			pdo = (union pdo_info *)&buffer[i];

			if (pdo->com.power_type == PDO_INFO_POWER_TYPE_FIX)
				seq_printf(s, "[PDO status] Fix PDO, voltage %d mV\n",
					   (pdo->so_f.voltage) * 50);
			else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_VAR)
				seq_printf(s, "[PDO status] Var PDO\n");
			else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_BAT)
				seq_printf(s, "[PDO status] Bat PDO\n");
		}
	}

	memset(buffer, 0, pdo_num_max * sizeof(u32));
	ret = rtk_rts54xx_get_pdo(rts54xx, PDO_TYPE_SINK, PDO_TCPM_PARTNER,
				  0, pdo_num_max, buffer);
	if (ret < 0) {
		seq_printf(s, "get_pdo (PDO_TYPE_SINK, PDO_TCPM_PARTNER) fail (ret=%d)\n", ret);
	} else {
		length = ret;

		seq_printf(s, "PDO info PDO_TYPE_SINK of PARTNER\n");
		for (i = 0; i < length; i = i + 4) {
			pdo = (union pdo_info *)&buffer[i];

			if (pdo->com.power_type == PDO_INFO_POWER_TYPE_FIX)
				seq_printf(s, "[PDO status] Fix PDO, voltage %d mV\n",
					   (pdo->si_f.voltage) * 50);
			else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_VAR)
				seq_printf(s, "[PDO status] Var PDO\n");
			else if (pdo->com.power_type == PDO_INFO_POWER_TYPE_BAT)
				seq_printf(s, "[PDO status] Var PDO\n");
		}
	}

	kfree(buffer);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(rts54xx_get_pdo);

static int rts54xx_get_rdo_show(struct seq_file *s, void *unused)
{
	struct rts54xx_dev *rts54xx = s->private;
	struct rdo_info rdo;
	int ret;

	ret = rtk_rts54xx_get_rdo(rts54xx, &rdo);
	if (ret < 0) {
		seq_printf(s, "get_rdo fail (ret=%d)\n", ret);
	} else {
		seq_printf(s, "RDO info: 0x%x\n", *(int *)&rdo);
		seq_printf(s, "[RDO] index %d\n", rdo.obj_pos);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(rts54xx_get_rdo);

static int rts54xx_get_current_pdo_show(struct seq_file *s, void *unused)
{
	struct rts54xx_dev *rts54xx = s->private;
	union pdo_info pdo;
	int ret;

	ret = rtk_rts54xx_get_current_pdo(rts54xx, &pdo);
	if (ret <= 0) {
		seq_printf(s, "get_current_pdo fail (ret=%d)\n", ret);
	} else {
		seq_printf(s, "PDO info: %x\n", *(int *)&pdo);
		if (pdo.com.power_type == PDO_INFO_POWER_TYPE_FIX)
			seq_printf(s, "[PDO status] Fix PDO, voltage %d mV\n",
				   (pdo.si_f.voltage) * 50);
		else if (pdo.com.power_type == PDO_INFO_POWER_TYPE_VAR)
			seq_printf(s, "[PDO status] Var PDO\n");
		else if (pdo.com.power_type == PDO_INFO_POWER_TYPE_BAT)
			seq_printf(s, "[PDO status] Var PDO\n");
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(rts54xx_get_current_pdo);

static int rts54xx_get_alt_mode_show(struct seq_file *s, void *unused)
{
	struct rts54xx_dev *rts54xx = s->private;
	int max_altmodes = UCSI_MAX_ALTMODES;
	struct ucsi_altmode alt;
	int i, ret, length;
	int support = 0;
	u8 current_cam = 0;

	for (i = 0; i < max_altmodes; i++) {
		memset(&alt, 0, sizeof(alt));

		ret = rtk_rts54xx_ucsi_get_alt_mode(rts54xx, UCSI_RECIPIENT_CON,
						    i, 0, (u8 *)&alt);
		if (ret <= 0) {
			break;
		} else {
			u16 svid;
			u32 vdo;

			length = ret;

			svid = alt.svid;
			vdo = alt.mid;

			seq_printf(s, "UCSI_RECIPIENT_CON alt_mode info #%d: \n", i);
			seq_printf(s, "SVID: 0x%x\n", svid);
			seq_printf(s, "VDO: 0x%x\n", vdo);
		}
	}

	for (i = 0; i < max_altmodes; i++) {
		memset(&alt, 0, sizeof(alt));

		ret = rtk_rts54xx_ucsi_get_alt_mode(rts54xx, UCSI_RECIPIENT_SOP,
						    i, 0, (u8 *)&alt);
		if (ret <= 0) {
			break;
		} else {
			u16 svid;
			u32 vdo;

			length = ret;

			svid = alt.svid;
			vdo = alt.mid;

			seq_printf(s, "UCSI_RECIPIENT_SOP alt_mode info #%d: \n", i);
			seq_printf(s, "SVID: 0x%x\n", svid);
			seq_printf(s, "VDO: 0x%x\n", vdo);
		}
	}

	ret = rtk_rts54xx_ucsi_get_cam_supported(rts54xx, &support);
	if (ret > 0)
		seq_printf(s, "Get connector ALT mode support: 0x%x\n", support);

	ret = rtk_rts54xx_ucsi_get_current_cam(rts54xx, &current_cam);
	if (ret > 0)
		seq_printf(s, "Get current  connector ALT mode: 0x%x\n", (int)current_cam);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(rts54xx_get_alt_mode);

static int rts54xx_set_notification_show(struct seq_file *s, void *unused)
{
	const struct file *file = s->file;
	const char *file_name = file_dentry(file)->d_iname;

	seq_puts(s, "Set notification by following command\n");
	seq_printf(s, "echo \"value\" > %s\n", file_name);

	return 0;
}

static int rts54xx_set_notification_open(struct inode *inode, struct file *file)
{
	return single_open(file, rts54xx_set_notification_show, inode->i_private);
}

static ssize_t rts54xx_set_notification_write(struct file *file,
					    const char __user *ubuf, size_t count,
					    loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct rts54xx_dev *rts54xx = s->private;
	int ret = 0;
	char buffer[40] = {0};
	u32 notification_en;

	if (copy_from_user(&buffer, ubuf,
			   min_t(size_t, sizeof(buffer) - 1, count)))
		return -EFAULT;

	ret = kstrtouint(buffer, 16, &notification_en);
	if (ret < 0)
		return -EINVAL;

	ret = rtk_rts54xx_set_notification(rts54xx, notification_en);
	if (ret < 0)
		return ret;

	return count;
}

static const struct file_operations rts54xx_set_notification_fops = {
	.open			= rts54xx_set_notification_open,
	.write			= rts54xx_set_notification_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

static int rts54xx_enable_vendor_command_show(struct seq_file *s, void *unused)
{
	const struct file *file = s->file;
	const char *file_name = file_dentry(file)->d_iname;

	seq_puts(s, "Set 1 to enable vendor command by following command\n");
	seq_printf(s, "echo \"1\" > %s\n", file_name);

	return 0;
}

static int rts54xx_enable_vendor_command_open(struct inode *inode, struct file *file)
{
	return single_open(file, rts54xx_enable_vendor_command_show, inode->i_private);
}

static ssize_t rts54xx_enable_vendor_command_write(struct file *file,
					    const char __user *ubuf, size_t count,
					    loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct rts54xx_dev *rts54xx = s->private;
	int ret = 0;
	char buffer[40] = {0};
	u32 enable;

	if (copy_from_user(&buffer, ubuf,
			   min_t(size_t, sizeof(buffer) - 1, count)))
		return -EFAULT;

	ret = kstrtouint(buffer, 16, &enable);
	if (ret < 0)
		return -EINVAL;

	if (enable)
		ret = rtk_rts54xx_vendor_command_enable(rts54xx);
	if (ret < 0)
		return ret;

	return count;
}

static const struct file_operations rts54xx_enable_vendor_command_fops = {
	.open			= rts54xx_enable_vendor_command_open,
	.write			= rts54xx_enable_vendor_command_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

static int rts54xx_reset_show(struct seq_file *s, void *unused)
{
	const struct file *file = s->file;
	const char *file_name = file_dentry(file)->d_iname;

	seq_puts(s, "Set 1 to reset ppm\n");
	seq_puts(s, "Set 2 to reset tcpm\n by following command\n");
	seq_printf(s, "echo \"value\" > %s\n", file_name);

	return 0;
}

static int rts54xx_reset_open(struct inode *inode, struct file *file)
{
	return single_open(file, rts54xx_reset_show, inode->i_private);
}

static ssize_t rts54xx_reset_write(struct file *file,
					    const char __user *ubuf, size_t count,
					    loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct rts54xx_dev *rts54xx = s->private;
	int ret = 0;
	char buffer[40] = {0};
	u32 enable;

	if (copy_from_user(&buffer, ubuf,
			   min_t(size_t, sizeof(buffer) - 1, count)))
		return -EFAULT;

	ret = kstrtouint(buffer, 16, &enable);
	if (ret < 0)
		return -EINVAL;

	if (enable == 1)
		ret = rtk_rts54xx_ppm_reset(rts54xx);
	else if (enable == 2)
		ret =  rtk_rts54xx_tcpm_reset(rts54xx);
	else if (enable == 3)
		ret = rtk_rts54xx_set_type_c_soft_reset(rts54xx);

	if (ret < 0)
		return ret;

	return count;
}

static const struct file_operations rts54xx_reset_fops = {
	.open			= rts54xx_reset_open,
	.write			= rts54xx_reset_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

static int rts54xx_set_rdo_show(struct seq_file *s, void *unused)
{
	const struct file *file = s->file;
	const char *file_name = file_dentry(file)->d_iname;

	seq_puts(s, "Set index of pdo to request data object\n by following command\n");
	seq_printf(s, "echo \"value\" > %s\n", file_name);

	return 0;
}

static int rts54xx_set_rdo_open(struct inode *inode, struct file *file)
{
	return single_open(file, rts54xx_set_rdo_show, inode->i_private);
}

static ssize_t rts54xx_set_rdo_write(struct file *file,
					    const char __user *ubuf, size_t count,
					    loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct rts54xx_dev *rts54xx = s->private;
	struct rdo_info rdo;
	int ret = 0;
	char buf[40] = {0};
	u32 index;
	u8 *buffer;
	int pdo_num_max = 7;
	int pdo_num = 0;

	buffer = kzalloc(pdo_num_max * sizeof(u32), GFP_KERNEL);

	if (copy_from_user(&buf, ubuf,
			   min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	ret = kstrtouint(buf, 16, &index);
	if (ret < 0)
		return -EINVAL;

	memset(buffer, 0, pdo_num_max * sizeof(u32));
	ret = rtk_rts54xx_get_pdo(rts54xx, PDO_TYPE_SINK, PDO_TCPM_TCPM,
				  0, pdo_num_max, buffer);
	if (ret < 0)
		seq_printf(s, "get_pdo (PDO_TYPE_SINK, PDO_TCPM_TCPM) fail (ret=%d)\n", ret);
	else
		pdo_num = ret;

	if (index > pdo_num) {
		seq_printf(s, "set index %d > TCPM sink pdo_num=%d\n", index, pdo_num);
		return count;
	}

	memset(buffer, 0, pdo_num_max * sizeof(u32));
	ret = rtk_rts54xx_get_pdo(rts54xx, PDO_TYPE_SOURCE, PDO_TCPM_PARTNER,
				  0, pdo_num_max, buffer);
	if (ret < 0)
		seq_printf(s, "get_pdo (PDO_TYPE_SOURCE, PDO_TCPM_PARTNER) fail (ret=%d)\n", ret);
	else
		pdo_num = ret;

	if (index > pdo_num) {
		seq_printf(s, "set index %d > PARTENER source pdo_num=%d\n", index, pdo_num);
		return count;
	}

	rdo.obj_pos = index;
	rdo.usb_comm_cap = 1;
	rdo.no_usb_suspend = 1;
	rdo.max_op_current = 300;
	rdo.op_current = 300;

	ret = rtk_rts54xx_set_rdo(rts54xx, &rdo);
	if (ret < 0)
		return ret;

	return count;
}

static const struct file_operations rts54xx_set_rdo_fops = {
	.open			= rts54xx_set_rdo_open,
	.write			= rts54xx_set_rdo_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

static int rts54xx_set_rdo_auto_show(struct seq_file *s, void *unused)
{
	const struct file *file = s->file;
	const char *file_name = file_dentry(file)->d_iname;

	seq_puts(s, "Set 1 to auto set rdo\n by following command\n");
	seq_printf(s, "echo \"value\" > %s\n", file_name);

	return 0;
}

static int rts54xx_set_rdo_auto_open(struct inode *inode, struct file *file)
{
	return single_open(file, rts54xx_set_rdo_auto_show, inode->i_private);
}

static ssize_t rts54xx_set_rdo_auto_write(struct file *file,
					    const char __user *ubuf, size_t count,
					    loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct rts54xx_dev *rts54xx = s->private;
	int ret = 0;
	char buf[40] = {0};
	u32 high = 0;

	if (copy_from_user(&buf, ubuf,
			   min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	ret = kstrtouint(buf, 16, &high);
	if (ret < 0)
		return -EINVAL;

	ret = rtk_rts54xx_auto_set_rdo(rts54xx, high);
	if (ret < 0)
		return ret;

	return count;
}

static const struct file_operations rts54xx_set_rdo_auto_fops = {
	.open			= rts54xx_set_rdo_auto_open,
	.write			= rts54xx_set_rdo_auto_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

static inline void create_debug_files(struct rts54xx_dev *rts54xx)
{
	struct dentry *debug_root = NULL;
	struct dentry *debug_dir = NULL;

	debug_root = create_debug_root();
	if (!debug_root)
		return;

	debug_dir = debugfs_create_dir("rts54xx", debug_root);
	if (!debug_dir)
		return;

	rts54xx->debug_dir = debug_dir;

	if (!debugfs_create_file("get_ic_info", 0444, debug_dir, rts54xx,
				 &rts54xx_get_ic_info_fops))
		goto file_error;

	if (!debugfs_create_file("get_status_info", 0444, debug_dir, rts54xx,
				 &rts54xx_get_status_info_fops))
		goto file_error;

	if (!debugfs_create_file("get_pdo_info", 0444, debug_dir, rts54xx,
				 &rts54xx_get_pdo_fops))
		goto file_error;

	if (!debugfs_create_file("get_rdo_info", 0444, debug_dir, rts54xx,
				 &rts54xx_get_rdo_fops))
		goto file_error;

	if (!debugfs_create_file("get_current_pdo", 0444, debug_dir, rts54xx,
				 &rts54xx_get_current_pdo_fops))
		goto file_error;

	if (!debugfs_create_file("get_alt_mode", 0444, debug_dir, rts54xx,
				 &rts54xx_get_alt_mode_fops))
		goto file_error;

	if (!debugfs_create_file("set_notification", 0444, debug_dir, rts54xx,
				 &rts54xx_set_notification_fops))
		goto file_error;

	if (!debugfs_create_file("enable_vendor_command", 0444, debug_dir, rts54xx,
				 &rts54xx_enable_vendor_command_fops))
		goto file_error;

	if (!debugfs_create_file("reset", 0444, debug_dir, rts54xx,
				 &rts54xx_reset_fops))
		goto file_error;

	if (!debugfs_create_file("set_rdo", 0444, debug_dir, rts54xx,
				 &rts54xx_set_rdo_fops))
		goto file_error;

	if (!debugfs_create_file("set_rdo_auto", 0444, debug_dir, rts54xx,
				 &rts54xx_set_rdo_auto_fops))
		goto file_error;

	return;

file_error:
	debugfs_remove_recursive(rts54xx->debug_dir);
}

static inline void remove_debug_files(struct rts54xx_dev *rts54xx)
{
	debugfs_remove_recursive(rts54xx->debug_dir);
}
#else
static inline void create_debug_files(struct rts54xx_dev *rts54xx) { }
static inline void remove_debug_files(struct rts54xx_dev *rts54xx) { }
#endif /* CONFIG_DEBUG_FS */

/* init and probe */
static void rts54xx_init_work(struct work_struct *work)
{
	struct rts54xx_dev *rts54xx = container_of(work, struct rts54xx_dev,
						  delayed_work.work);
	struct device *dev = rts54xx->dev;
	struct typec_altmode_desc desc;
	int ret;

	dev_dbg(rts54xx->dev, "Enter %s\n", __func__);

	rts54xx->sw = typec_switch_get(dev);
	if (IS_ERR(rts54xx->sw)) {
		ret = PTR_ERR(rts54xx->sw);
		dev_err(dev, "[RTS54XX] typec_switch_get fail (ret=%d)\n", ret);
		rts54xx->sw = NULL;
	}
	if (!rts54xx->sw)
		dev_err(dev, "[RTS54XX] typec_switch_get NULL\n");

	memset(&desc, 0, sizeof(desc));
	desc.svid = USB_TYPEC_DP_SID;

	rts54xx->mux = typec_mux_get(dev);
	if (IS_ERR(rts54xx->mux)) {
		ret = PTR_ERR(rts54xx->mux);
		dev_err(dev, "[RTS54XX] typec_mux_get fail (ret=%d)\n", ret);
		rts54xx->mux = NULL;
	}
	if (!rts54xx->mux)
		dev_err(dev, "[RTS54XX] typec_mux_get NULL\n");

	if (rts54xx->alert)
		if (!i2c_get_clientdata(rts54xx->alert))
			goto out_reschedule;

	dev_dbg(rts54xx->dev, "Exit %s\n", __func__);
	rtk_rts54xx_alert(rts54xx->client, 0, 0);

	return;

out_reschedule:

	schedule_delayed_work(&rts54xx->delayed_work, msecs_to_jiffies(500));
}

static int rtk_rts54xx_init(struct rts54xx_dev *rts54xx)
{
	struct device *dev = rts54xx->dev;
	int ret;

	ret = rtk_rts54xx_vendor_command_enable(rts54xx);
	if (ret < 0) {
		dev_err(dev, "[RTS54XX] rtk_rts54xx_vendor_command_enable fail (ret=%d)\n", ret);
		return ret;
	}

	ret = rtk_rts54xx_set_notification(rts54xx, 0xffff);
	if (ret < 0) {
		dev_err(dev, "[RTS54XX] rtk_rts54xx_set_notification (ret=%d)\n", ret);
		return ret;
	}

	ret = rtk_rts54xx_ppm_reset(rts54xx);
	if (ret < 0) {
		dev_err(dev, "[RTS54XX] rtk_rts54xx_ppm_reset (ret=%d)\n", ret);
		return ret;
	}
	ret = rtk_rts54xx_get_ic_info(rts54xx);
	if (ret < 0) {
		dev_err(dev, "[RTS54XX] rtk_rts54xx_get_ic_info fail (ret=%d)\n", ret);
		rts54xx->no_device = true;
	} else {
		ret = rtk_rts54xx_get_status_dump(rts54xx);
		if (ret < 0)
			dev_err(dev, "[RTS54XX] rtk_rts54xx_get_status fail (ret=%d)\n", ret);

		ret = rtk_rts54xx_ucsi_get_capability(rts54xx);
	}

	ret = 0;
	return ret;
}

static int rtk_rts54xx_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct rts54xx_dev *rts54xx;
	int ret = 0;

	dev_dbg(dev, "Enter %s\n", __func__);

	rts54xx = devm_kzalloc(dev, sizeof(*rts54xx), GFP_KERNEL);
	if (!rts54xx)
		return -ENOMEM;

	rts54xx->client = client;
	rts54xx->dev = dev;
	mutex_init(&rts54xx->transfer_lock);
	mutex_init(&rts54xx->lock);
	init_completion(&rts54xx->complete);

	/* reset rts54xx device and initialize ucsi */
	ret = rtk_rts54xx_init(rts54xx);
	if (ret < 0) {
		dev_err(rts54xx->dev, "rtk_rts54xx_init failed %d\n", ret);
		return ret;
	}

	rts54xx->irq_gpiod = devm_gpiod_get(dev, "notify", GPIOD_IN);
	if (IS_ERR(rts54xx->irq_gpiod)) {
		dev_err(dev, "can't request notify gpio\n");
		return -EPROBE_DEFER;
	}

	rts54xx->alert_data.irq = gpiod_to_irq(rts54xx->irq_gpiod);
	if (rts54xx->alert_data.irq > 0) {
		irq_set_irq_type(rts54xx->alert_data.irq, IRQ_TYPE_EDGE_FALLING);
		rts54xx->alert = i2c_new_smbus_alert_device(client->adapter, &rts54xx->alert_data);
	}

	if (!IS_ERR(rts54xx->alert)) {
		s32 status;

		status = i2c_smbus_read_byte(rts54xx->alert);
		dev_info(&rts54xx->alert->dev, "smbus_alert status=%d\n", status);
	}

	i2c_set_clientdata(client, rts54xx);

	INIT_DELAYED_WORK(&rts54xx->delayed_work, rts54xx_init_work);

	schedule_delayed_work(&rts54xx->delayed_work, msecs_to_jiffies(500));

	create_debug_files(rts54xx);

	dev_dbg(dev, "Exit %s\n", __func__);

	return 0;
}

static void rtk_rts54xx_remove(struct i2c_client *client)
{
	struct rts54xx_dev *rts54xx = i2c_get_clientdata(client);

	typec_mux_put(rts54xx->mux);
	typec_switch_put(rts54xx->sw);

	remove_debug_files(rts54xx);
}

static void rtk_rts54xx_shutdown(struct i2c_client *client)
{
	/* nothing */
}

static void rtk_rts54xx_alert(struct i2c_client *client,
			      enum i2c_alert_protocol protocol,
			      unsigned int data)
{
	struct device *dev = &client->dev;
	struct rts54xx_dev *rts54xx = i2c_get_clientdata(client);
	struct pd_status_info status_info;
	enum typec_orientation orien = TYPEC_ORIENTATION_NORMAL;
	struct typec_mux_state state = { };
	int retry, retry_count = 10;
	int ret;

	dev_dbg(dev, "Enter %s\n", __func__);

	for (retry = 0; retry < retry_count; retry++) {
		int *tmp;

		ret = rtk_rts54xx_get_status(rts54xx, 0, 14, &status_info);
		if (ret < 0)
			return;

		if (ret == 14)
			break;

		tmp = (int *)&status_info;
		if (0xffffffff != (u32)*tmp && 0x0 != (u8)*tmp)
			break;

		dev_info(dev, "%s status_info dump=%x (retry=%d)\n", __func__, *tmp, retry);
		dev_info(dev, "%s try again to get status (retry=%d)\n", __func__, retry);

		mdelay(100);
	}

	//rtk_rts54xx_get_status_dump(rts54xx);

	ret = rtk_rts54xx_get_cam_state(rts54xx, &state);
	if (ret < 0)
		return;

	ret = rtk_rts54xx_ucsi_ack_cc_ci(rts54xx);
	if (ret < 0)
		return;

	if (status_info.connect_status)
		dev_info(dev, "%s Attached ###############\n", __func__);
	else
		dev_info(dev, "%s Unattached #############\n", __func__);

	if (!status_info.plug_direction)
		orien = TYPEC_ORIENTATION_REVERSE;
	if (!status_info.connect_status)
		orien = TYPEC_ORIENTATION_NONE;

	dev_info(dev, "%s typec_switch_set orien=%d\n", __func__, orien);
	ret = typec_switch_set(rts54xx->sw, orien);

	dev_info(dev, "%s typec_mux_state state.mode=%d\n", __func__, (int)state.mode);
	ret = typec_mux_set(rts54xx->mux, &state);

	dev_dbg(dev, "Exit %s\n", __func__);
}

#ifdef CONFIG_SUSPEND
static int rtk_rts54xx_suspend(struct device *dev)
{
	dev_dbg(dev, "Enter %s\n", __func__);

	dev_dbg(dev, "Exit %s\n", __func__);
	return 0;
}

static int rtk_rts54xx_resume(struct device *dev)
{
	dev_dbg(dev, "Enter %s\n", __func__);

	dev_dbg(dev, "Exit %s\n", __func__);
	return 0;
}
#else

#define rtk_rts54xx_suspend NULL
#define rtk_rts54xx_resume NULL

#endif

static const struct dev_pm_ops rtk_rts54xx_pm_ops = {
	.suspend    = rtk_rts54xx_suspend,
	.resume     = rtk_rts54xx_resume,
};

static const struct i2c_device_id rtk_rts54xx_ids[] = {
	{"rtk-rts5450", 0},
	{"rtk-rts5452", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, rtk_rts54xx_ids);

static struct i2c_driver rtk_rts54xx_driver = {
	.driver = {
		.name = "rtk_rts54xx",
		.owner = THIS_MODULE,
		.pm = &rtk_rts54xx_pm_ops,
	},
	.probe = rtk_rts54xx_probe,
	.remove = rtk_rts54xx_remove,
	.shutdown = rtk_rts54xx_shutdown,
	.alert = rtk_rts54xx_alert,
	.id_table = rtk_rts54xx_ids,
};

module_i2c_driver(rtk_rts54xx_driver);

MODULE_DESCRIPTION("RTS54XX Type C Port Manager Driver");
MODULE_AUTHOR("Stanley Chang");
MODULE_LICENSE("GPL");
