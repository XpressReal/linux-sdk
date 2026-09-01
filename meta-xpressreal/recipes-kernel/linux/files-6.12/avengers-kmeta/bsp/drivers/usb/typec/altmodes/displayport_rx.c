// SPDX-License-Identifier: GPL-2.0
/*
 * USB Type-C DisplayPort Alternate Mode Driver - UFP_D (Sink/Receiver)
 *
 * Copyright (C) 2026 Realtek Semiconductor Corp.
 *
 * This driver handles DisplayPort Alt Mode for UFP_D (Sink) role.
 * Unlike displayport.c which is designed for DFP_D (Source),
 * this driver properly handles VDM requests from DFP_D and provides
 * appropriate responses.
 */
//#define DEBUG

#include <linux/delay.h>
#include <linux/log2.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/usb/pd_vdo.h>
#include <linux/usb/typec_dp.h>
#include <linux/usb/typec_altmode.h>
#include "displayport.h"

#define DP_HEADER(_dp, ver, cmd)	(VDO((_dp)->alt->svid, 1, ver, cmd) \
					 | VDO_OPOS(USB_TYPEC_DP_MODE))

enum dp_rx_state {
	DP_RX_STATE_IDLE,
	DP_RX_STATE_ENTERED,
	DP_RX_STATE_CONFIGURED,
};

struct dp_rx_altmode {
	struct typec_altmode *alt;
	const struct typec_altmode *port;
	struct device *dev;

	enum dp_rx_state state;
	struct mutex lock;

	/* Current DP configuration */
	u32 conf;			/* Configuration VDO */
	u32 status;			/* Status VDO */

	/* HPD state */
	bool hpd_state;			/* Current HPD level (HIGH/LOW) */
	bool hpd_irq;			/* HPD IRQ pending */

	/* Workqueue for async Attention send (deferred until VDM response completes) */
	struct workqueue_struct *wq;
	/* True while a VDM response is queued but not yet acknowledged by DFP_D */
	bool vdm_pending;
	/* Attention workqueue waits here until vdm_pending is cleared */
	wait_queue_head_t vdm_done;
};

/*
 * dp_rx_dump_status_vdo - Dump Status VDO details
 */
static void dp_rx_dump_status_vdo(struct device *dev, u32 status)
{
	dev_dbg(dev, "[DP_RX]   Status VDO breakdown:\n");
	dev_dbg(dev, "[DP_RX]     Bit 31-9: Reserved (0x%06x)\n",
		(status >> 9) & 0xffffff);
	dev_dbg(dev, "[DP_RX]     Bit 8: HPD IRQ = %d\n",
		!!(status & DP_STATUS_IRQ_HPD));
	dev_dbg(dev, "[DP_RX]     Bit 7: HPD State = %d (%s)\n",
		!!(status & DP_STATUS_HPD_STATE),
		(status & DP_STATUS_HPD_STATE) ? "HIGH" : "LOW");
	dev_dbg(dev, "[DP_RX]     Bit 6: Exit DP = %d\n",
		!!(status & DP_STATUS_EXIT_DP_MODE));
	dev_dbg(dev, "[DP_RX]     Bit 5: To USB Config = %d\n",
		!!(status & DP_STATUS_SWITCH_TO_USB));
	dev_dbg(dev, "[DP_RX]     Bit 4: MF Pref = %d\n",
		!!(status & DP_STATUS_PREFER_MULTI_FUNC));
	dev_dbg(dev, "[DP_RX]     Bit 3: Enabled = %d\n",
		!!(status & DP_STATUS_ENABLED));
	dev_dbg(dev, "[DP_RX]     Bit 2: Power Low = %d\n",
		!!(status & DP_STATUS_POWER_LOW));
	dev_dbg(dev, "[DP_RX]     Bit 1-0: Connection = 0x%x (%s)\n",
		status & (DP_STATUS_CON_DFP_D | DP_STATUS_CON_UFP_D),
		(status & DP_STATUS_CON_UFP_D) ? "UFP_D" :
		(status & DP_STATUS_CON_DFP_D) ? "DFP_D" : "None");
}

/*
 * dp_rx_build_status_vdo - Build Status Update VDO
 *
 * Constructs the Status VDO that will be sent in response to
 * DFP_D's Status Update query.
 */
static u32 dp_rx_build_status_vdo(struct dp_rx_altmode *dp_rx)
{
	u32 status = 0;

	/* UFP_D Connected */
	status |= DP_STATUS_CON_UFP_D;

	/* Enabled */
	status |= DP_STATUS_ENABLED;

	/* HPD State */
	if (dp_rx->hpd_state)
		status |= DP_STATUS_HPD_STATE;

	/* HPD IRQ */
	if (dp_rx->hpd_irq) {
		status |= DP_STATUS_IRQ_HPD;
		dev_info(dp_rx->dev, "[DP_RX] Including HPD IRQ in status\n");
		dp_rx->hpd_irq = false;  /* Clear after reporting */
	}

	/* Multi-function preferred (if we want USB + DP) */
	/* status |= DP_STATUS_PREFER_MULTI_FUNC; */

	dev_info(dp_rx->dev, "[DP_RX] Built Status VDO: 0x%08x\n", status);
	dp_rx_dump_status_vdo(dp_rx->dev, status);

	return status;
}

/*
 * dp_rx_state_name - Get state name string
 */
static const char *dp_rx_state_name(enum dp_rx_state state)
{
	switch (state) {
	case DP_RX_STATE_IDLE:
		return "IDLE";
	case DP_RX_STATE_ENTERED:
		return "ENTERED";
	case DP_RX_STATE_CONFIGURED:
		return "CONFIGURED";
	default:
		return "UNKNOWN";
	}
}

/*
 * dp_rx_handle_enter_mode - Handle Enter Mode VDM from DFP_D
 */
static int dp_rx_handle_enter_mode(struct dp_rx_altmode *dp_rx,
				    const u32 hdr, const u32 *vdo, int count);

/*
 * dp_rx_check_data_role_for_ufp_d - Check if current data role is suitable for UFP_D
 *
 * UFP_D (DisplayPort Sink) requires UFP (TYPEC_DEVICE) data role.
 * This function logs the requirement for role checking.
 */
static void dp_rx_check_data_role_for_ufp_d(struct dp_rx_altmode *dp_rx)
{
	dev_dbg(dp_rx->dev, "\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
	dev_dbg(dp_rx->dev, "[DP_RX] === Data Role Check for UFP_D ===\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
	dev_dbg(dp_rx->dev, "[DP_RX] UFP_D (DP Sink) requires UFP data role\n");
	dev_dbg(dp_rx->dev, "[DP_RX] Current mode: DisplayPort SVID\n");

	/*
	 * Note: We cannot directly check the data role from altmode driver context.
	 * The data role is managed by tcpm and will be checked when VDMs arrive.
	 *
	 * Strategy:
	 * 1. Log that UFP_D requires UFP role
	 * 2. Hybrid DR_Swap in tcpm.c will handle automatic role swap
	 * 3. When Enter Mode VDM arrives, we can check the role at that time
	 * 4. If role is wrong at Enter Mode, we can NAK and wait for DR_Swap
	 */

	dev_dbg(dp_rx->dev, "[DP_RX] Role check will be performed when VDMs arrive\n");
	dev_dbg(dp_rx->dev, "[DP_RX] Hybrid DR_Swap in tcpm handles automatic swap\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
	dev_dbg(dp_rx->dev, "\n");
}

static int dp_rx_handle_enter_mode(struct dp_rx_altmode *dp_rx,
				    const u32 hdr, const u32 *vdo, int count)
{
	dev_dbg(dp_rx->dev, "[DP_RX] === Handle Enter Mode ===\n");
	dev_dbg(dp_rx->dev, "[DP_RX]   Current state: %s\n",
		dp_rx_state_name(dp_rx->state));

	if (dp_rx->state != DP_RX_STATE_IDLE) {
		dev_warn(dp_rx->dev, "[DP_RX] Enter Mode in wrong state: %s (expected IDLE)\n",
			 dp_rx_state_name(dp_rx->state));
		return -EBUSY;
	}

	/* Enter DP Alt Mode */
	dev_dbg(dp_rx->dev, "[DP_RX] State transition: %s → %s\n",
		dp_rx_state_name(dp_rx->state),
		dp_rx_state_name(DP_RX_STATE_ENTERED));
	dp_rx->state = DP_RX_STATE_ENTERED;
	typec_altmode_update_active(dp_rx->alt, true);

	dev_info(dp_rx->dev, "[DP_RX] DP Alt Mode entered successfully\n");

	/* Response: ACK (no VDO needed for Enter Mode) */
	return 0;
}

/* Forward declaration for dp_rx_altmode_notify */
static int dp_rx_altmode_notify(struct dp_rx_altmode *dp_rx);

/*
 * dp_rx_handle_exit_mode - Handle Exit Mode VDM from DFP_D
 */
static int dp_rx_handle_exit_mode(struct dp_rx_altmode *dp_rx,
				   const u32 hdr, const u32 *vdo, int count)
{
	int ret;

	dev_dbg(dp_rx->dev, "[DP_RX] === Handle Exit Mode ===\n");
	dev_dbg(dp_rx->dev, "[DP_RX]   Current state: %s\n",
		dp_rx_state_name(dp_rx->state));

	if (dp_rx->state == DP_RX_STATE_IDLE) {
		dev_warn(dp_rx->dev, "[DP_RX] Exit Mode when not entered\n");
		return -EINVAL;
	}

	/* Reset state */
	dev_dbg(dp_rx->dev, "[DP_RX] State transition: %s → %s\n",
		dp_rx_state_name(dp_rx->state),
		dp_rx_state_name(DP_RX_STATE_IDLE));
	dev_dbg(dp_rx->dev, "[DP_RX] Clearing configuration (conf=0x%08x)\n",
		dp_rx->conf);

	/* Clear HPD state on exit */
	dp_rx->hpd_state = false;
	dp_rx->hpd_irq = false;

	dp_rx->state = DP_RX_STATE_IDLE;
	dp_rx->conf = 0;
	dp_rx->status = 0;
	typec_altmode_update_active(dp_rx->alt, false);

	/*
	 * Notify mux to switch back to USB mode.
	 * This triggers PHY's mux_set callback with TYPEC_STATE_USB.
	 */
	dev_dbg(dp_rx->dev, "[DP_RX] Notifying mux to switch to USB mode...\n");
	ret = dp_rx_altmode_notify(dp_rx);
	if (ret)
		dev_warn(dp_rx->dev, "[DP_RX] mux notify failed: %d\n", ret);

	dev_info(dp_rx->dev, "[DP_RX] DP Alt Mode exited successfully\n");

	/* Response: ACK */
	return 0;
}

/*
 * dp_rx_handle_status_update - Handle Status Update VDM from DFP_D
 *
 * DFP_D queries our status. We respond with current HPD state.
 */
static int dp_rx_handle_status_update(struct dp_rx_altmode *dp_rx,
				       const u32 hdr, const u32 *vdo,
				       int count, u32 *response)
{
	dev_dbg(dp_rx->dev, "[DP_RX] === Handle Status Update ===\n");
	dev_dbg(dp_rx->dev, "[DP_RX]   Current state: %s\n",
		dp_rx_state_name(dp_rx->state));

	if (dp_rx->state == DP_RX_STATE_IDLE) {
		dev_warn(dp_rx->dev, "[DP_RX] Status Update when not entered\n");
		return -EINVAL;
	}

	/* Dump DFP_D's status if provided */
	if (count >= 2) {
		dev_info(dp_rx->dev, "[DP_RX] DFP_D Status VDO: 0x%08x\n", vdo[0]);
		dp_rx_dump_status_vdo(dp_rx->dev, vdo[0]);
	}

	/* Build Status VDO with current HPD state */
	dev_dbg(dp_rx->dev, "[DP_RX] Building UFP_D Status VDO...\n");
	dev_dbg(dp_rx->dev, "[DP_RX]   Current HPD state: %s\n",
		dp_rx->hpd_state ? "HIGH" : "LOW");
	dev_dbg(dp_rx->dev, "[DP_RX]   HPD IRQ pending: %s\n",
		dp_rx->hpd_irq ? "Yes" : "No");
	dp_rx->status = dp_rx_build_status_vdo(dp_rx);

	dev_info(dp_rx->dev, "[DP_RX] Sending Status VDO: 0x%08x\n",
		 dp_rx->status);

	/* Response: ACK + Status VDO */
	response[0] = dp_rx->status;
	return 1;  /* 1 VDO in response */
}

/*
 * dp_rx_dump_configure_vdo - Dump Configure VDO details
 */
static void dp_rx_dump_configure_vdo(struct device *dev, u32 conf)
{
	u8 pin_assign = DP_CONF_GET_PIN_ASSIGN(conf);
	u8 signaling = (conf >> 2) & 0xf;
	const char *pin_name;

	/* pin_assign is a bitmask: 0x04=C, 0x08=D, 0x10=E, 0x20=F */
	switch (pin_assign) {
	case BIT(DP_PIN_ASSIGN_C):
		pin_name = "C (4-lane)";
		break;
	case BIT(DP_PIN_ASSIGN_D):
		pin_name = "D (2-lane)";
		break;
	case BIT(DP_PIN_ASSIGN_E):
		pin_name = "E (4-lane)";
		break;
	case BIT(DP_PIN_ASSIGN_F):
		pin_name = "F (2-lane)";
		break;
	default:
		pin_name = "Unknown";
		break;
	}

	dev_dbg(dev, "[DP_RX]   Configure VDO breakdown:\n");
	dev_dbg(dev, "[DP_RX]     Bit 31-24: Reserved (0x%02x)\n",
		(conf >> 24) & 0xff);
	dev_dbg(dev, "[DP_RX]     Bit 15-8: Pin Assignment = 0x%02x (%s)\n",
		pin_assign, pin_name);
	dev_dbg(dev, "[DP_RX]     Bit 5-2: Signaling = 0x%x (%s)\n",
		signaling,
		signaling == 0 ? "Unspecified" :
		signaling == 1 ? "DP v1.3" :
		signaling == 2 ? "Gen 2" : "Reserved");
	dev_dbg(dev, "[DP_RX]     Bit 1-0: Select Config = 0x%x (%s)\n",
		conf & 0x3,
		(conf & 0x3) == 0 ? "USB" :
		(conf & 0x3) == 1 ? "UFP_D" :
		(conf & 0x3) == 2 ? "DFP_D" : "Reserved");
}

/*
 * dp_rx_altmode_notify - Notify typec_mux of configuration change
 *
 * Triggers PHY's mux_set callback to configure lanes.
 * HPD is sent later by PHY when DRM DP RX calls phy_connect().
 */
static int dp_rx_altmode_notify(struct dp_rx_altmode *dp_rx)
{
	struct typec_displayport_data dp_data;
	unsigned long conf_state;
	u8 pin_state;
	int ret;

	if (!dp_rx->conf) {
		conf_state = TYPEC_STATE_USB;
	} else {
		pin_state = get_count_order(DP_CONF_GET_PIN_ASSIGN(dp_rx->conf));
		conf_state = TYPEC_MODAL_STATE(pin_state);
	}

	dp_data.status = dp_rx->status;
	dp_data.conf = dp_rx->conf;

	dev_info(dp_rx->dev, "[DP_RX] typec_altmode_notify(state=0x%lx)\n", conf_state);

	ret = typec_altmode_notify(dp_rx->alt, conf_state, &dp_data);
	if (ret)
		dev_err(dp_rx->dev, "[DP_RX] typec_altmode_notify failed: %d\n", ret);

	return ret;
}

/*
 * dp_rx_validate_configure - Validate Configure VDO from DFP_D
 *
 * Validates the pin assignment in Configure VDO and logs the configuration.
 * Actual PHY configuration is handled via typec_altmode_notify() which
 * triggers the PHY's mux_set callback.
 */
static int dp_rx_validate_configure(struct dp_rx_altmode *dp_rx, u32 conf)
{
	u8 pin_assign;
	u8 lanes;
	const char *pin_name;

	pin_assign = DP_CONF_GET_PIN_ASSIGN(conf);
	dev_info(dp_rx->dev, "[DP_RX] Parsing Configure VDO: 0x%08x\n", conf);
	dp_rx_dump_configure_vdo(dp_rx->dev, conf);

	/*
	 * Determine lane count based on pin assignment.
	 * Note: pin_assign is a bitmask value (BIT(n)), not enum value.
	 * - Pin C = 0x04 = BIT(DP_PIN_ASSIGN_C) = BIT(2)
	 * - Pin D = 0x08 = BIT(DP_PIN_ASSIGN_D) = BIT(3)
	 * - Pin E = 0x10 = BIT(DP_PIN_ASSIGN_E) = BIT(4)
	 * - Pin F = 0x20 = BIT(DP_PIN_ASSIGN_F) = BIT(5)
	 */
	switch (pin_assign) {
	case BIT(DP_PIN_ASSIGN_C):	/* 0x04 */
		lanes = 4;
		pin_name = "C (4-lane DP)";
		break;
	case BIT(DP_PIN_ASSIGN_E):	/* 0x10 */
		lanes = 4;
		pin_name = "E (4-lane DP)";
		break;
	case BIT(DP_PIN_ASSIGN_D):	/* 0x08 */
		lanes = 2;
		pin_name = "D (2-lane DP + USB)";
		break;
	case BIT(DP_PIN_ASSIGN_F):	/* 0x20 */
		lanes = 2;
		pin_name = "F (2-lane DP + USB)";
		break;
	default:
		dev_err(dp_rx->dev, "[DP_RX] Unsupported pin assignment: 0x%02x\n",
			pin_assign);
		return -EINVAL;
	}

	dev_info(dp_rx->dev, "[DP_RX] Configuration: %d lanes, Pin %s\n",
		 lanes, pin_name);

	return 0;
}

/*
 * dp_rx_handle_configure - Handle Configure VDM from DFP_D
 *
 * DFP_D tells us the lane configuration. We validate and notify PHY via
 * typec_altmode_notify() which triggers the PHY's mux_set callback.
 * HPD is sent by PHY driver when DRM DP RX driver calls phy_connect().
 */
static int dp_rx_handle_configure(struct dp_rx_altmode *dp_rx,
				   const u32 hdr, const u32 *vdo, int count)
{
	u32 conf;
	int ret;

	dev_dbg(dp_rx->dev, "[DP_RX] === Handle Configure ===\n");
	dev_dbg(dp_rx->dev, "[DP_RX]   Current state: %s\n",
		dp_rx_state_name(dp_rx->state));

	if (count < 2) {
		dev_err(dp_rx->dev, "[DP_RX] Configure VDM missing VDO (count=%d)\n",
			count);
		return -EINVAL;
	}

	conf = vdo[0];
	dev_info(dp_rx->dev, "[DP_RX] Received Configure VDO: 0x%08x\n", conf);

	if (dp_rx->state != DP_RX_STATE_ENTERED &&
	    dp_rx->state != DP_RX_STATE_CONFIGURED) {
		dev_warn(dp_rx->dev, "[DP_RX] Configure in wrong state: %s\n",
			 dp_rx_state_name(dp_rx->state));
		dev_warn(dp_rx->dev, "[DP_RX]   Expected: ENTERED or CONFIGURED\n");
		return -EINVAL;
	}

	/* Validate configuration */
	ret = dp_rx_validate_configure(dp_rx, conf);
	if (ret) {
		dev_err(dp_rx->dev, "[DP_RX] Invalid configuration: %d\n", ret);
		return ret;
	}

	/* Update state */
	if (dp_rx->state == DP_RX_STATE_ENTERED) {
		dev_dbg(dp_rx->dev, "[DP_RX] State: %s -> %s\n",
			dp_rx_state_name(dp_rx->state),
			dp_rx_state_name(DP_RX_STATE_CONFIGURED));
	}
	dp_rx->conf = conf;
	dp_rx->state = DP_RX_STATE_CONFIGURED;

	/*
	 * Notify typec_mux of configuration change.
	 * This triggers PHY's mux_set callback to configure lanes.
	 * HPD will be sent by PHY when DRM DP RX driver calls phy_connect().
	 */
	dev_dbg(dp_rx->dev, "[DP_RX] Notifying mux...\n");
	ret = dp_rx_altmode_notify(dp_rx);
	if (ret)
		dev_warn(dp_rx->dev, "[DP_RX] mux notify failed: %d\n", ret);

	dev_info(dp_rx->dev, "[DP_RX] DP Alt Mode configured\n");

	return 0;
}

/*
 * dp_rx_dump_vdm_header - Dump VDM header details
 */
static void dp_rx_dump_vdm_header(struct device *dev, u32 hdr)
{
	int cmd_type = PD_VDO_CMDT(hdr);
	int cmd = PD_VDO_CMD(hdr);
	int svid = PD_VDO_VID(hdr);
	int opos = PD_VDO_OPOS(hdr);
	int ver = PD_VDO_SVDM_VER(hdr);

	dev_dbg(dev, "[DP_RX]   VDM header breakdown:\n");
	dev_dbg(dev, "[DP_RX]     Bit 31-16: SVID = 0x%04x (%s)\n",
		svid, svid == 0xff01 ? "DisplayPort" : "Other");
	dev_dbg(dev, "[DP_RX]     Bit 15: VDM Type = %d (Structured)\n",
		!!(hdr & (1 << 15)));
	dev_dbg(dev, "[DP_RX]     Bit 14-13: SVDM Version = %d\n", ver);
	dev_dbg(dev, "[DP_RX]     Bit 12-8: Object Position = %d\n", opos);
	dev_dbg(dev, "[DP_RX]     Bit 7: Reserved\n");
	dev_dbg(dev, "[DP_RX]     Bit 6-5: Command Type = %d (%s)\n",
		cmd_type,
		cmd_type == CMDT_INIT ? "INIT" :
		cmd_type == CMDT_RSP_ACK ? "RSP_ACK" :
		cmd_type == CMDT_RSP_NAK ? "RSP_NAK" : "RSP_BUSY");
	dev_dbg(dev, "[DP_RX]     Bit 4-0: Command = %d (%s)\n",
		cmd,
		cmd == CMD_DISCOVER_IDENT ? "Discover Identity" :
		cmd == CMD_DISCOVER_SVID ? "Discover SVID" :
		cmd == CMD_DISCOVER_MODES ? "Discover Modes" :
		cmd == CMD_ENTER_MODE ? "Enter Mode" :
		cmd == CMD_EXIT_MODE ? "Exit Mode" :
		cmd == CMD_ATTENTION ? "Attention" :
		cmd == DP_CMD_STATUS_UPDATE ? "DP Status Update" :
		cmd == DP_CMD_CONFIGURE ? "DP Configure" : "Unknown");
}

/*
 * dp_rx_dump_vdos - Dump VDO array
 */
static void dp_rx_dump_vdos(struct device *dev, const u32 *vdo, int count)
{
	int i;

	if (count <= 1) {
		dev_dbg(dev, "[DP_RX]   (No VDOs)\n");
		return;
	}

	dev_dbg(dev, "[DP_RX]   VDOs (%d):\n", count - 1);
	for (i = 0; i < count - 1; i++) {
		dev_dbg(dev, "[DP_RX]     VDO[%d]: 0x%08x\n", i, vdo[i]);
	}
}

/* --- Attention workqueue support --- */

/*
 * PD_T_VDM_RESPONSE_MS: how long to wait for the VDM response to clear
 * the TCPM state machine before sending Attention.  Covers
 * PD_T_VDM_RESPONSE (100 ms @ PD 3.0) plus margin.
 */
#define PD_T_VDM_RESPONSE_MS	200

struct dp_rx_attention_work {
	struct work_struct work;
	struct dp_rx_altmode *dp_rx;
	bool hpd_high;
	bool hpd_irq;
};

/*
 * dp_rx_do_send_attention - workqueue handler that sends the Attention VDM.
 *
 * Waits on vdm_done until vdm_pending is cleared (i.e. the next VDM from
 * DFP_D has arrived, confirming the previous response exchange is complete)
 * before sending the Attention VDM via port ops->vdm().
 */
static void dp_rx_do_send_attention(struct work_struct *work)
{
	struct dp_rx_attention_work *aw =
		container_of(work, struct dp_rx_attention_work, work);
	struct dp_rx_altmode *dp_rx = aw->dp_rx;
	bool hpd_high = aw->hpd_high;
	bool hpd_irq = aw->hpd_irq;
	int svdm_version;
	u32 header, status_vdo;
	int ret;

	/* Wait until the previous VDM response exchange completes */
	if (!wait_event_timeout(dp_rx->vdm_done, !dp_rx->vdm_pending,
				msecs_to_jiffies(PD_T_VDM_RESPONSE_MS)))
		dev_warn(dp_rx->dev, "[DP_RX] [WQ] Timed out waiting for VDM response to complete\n");

	msleep(50);

	mutex_lock(&dp_rx->lock);

	dev_dbg(dp_rx->dev, "\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
	dev_dbg(dp_rx->dev, "[DP_RX] === Sending Attention VDM to DFP_D ===\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
	dev_dbg(dp_rx->dev, "[DP_RX] HPD change request:\n");
	dev_dbg(dp_rx->dev, "[DP_RX]   HPD State: %s\n", hpd_high ? "HIGH" : "LOW");
	dev_dbg(dp_rx->dev, "[DP_RX]   HPD IRQ: %s\n", hpd_irq ? "Yes" : "No");
	dev_dbg(dp_rx->dev, "[DP_RX]   Current state: %s\n",
		dp_rx_state_name(dp_rx->state));

	if (dp_rx->state == DP_RX_STATE_IDLE) {
		dev_warn(dp_rx->dev, "[DP_RX] Cannot send Attention: not in DP mode\n");
		dev_warn(dp_rx->dev, "[DP_RX]   Must be in ENTERED or CONFIGURED state\n");
		ret = -EINVAL;
		goto unlock;
	}

	/* Update HPD state */
	dev_dbg(dp_rx->dev, "[DP_RX] Updating internal HPD state:\n");
	dev_dbg(dp_rx->dev, "[DP_RX]   Old HPD state: %s\n",
		dp_rx->hpd_state ? "HIGH" : "LOW");
	dev_dbg(dp_rx->dev, "[DP_RX]   New HPD state: %s\n",
		hpd_high ? "HIGH" : "LOW");
	dp_rx->hpd_state = hpd_high;
	if (hpd_irq) {
		dev_info(dp_rx->dev, "[DP_RX]   Setting HPD IRQ flag\n");
		dp_rx->hpd_irq = true;
	}

	/* Build Attention VDM */
	dev_dbg(dp_rx->dev, "[DP_RX] Building Attention VDM...\n");
	svdm_version = typec_altmode_get_svdm_version(dp_rx->alt);
	if (svdm_version < 0) {
		dev_err(dp_rx->dev, "[DP_RX] Failed to get SVDM version: %d\n",
			svdm_version);
		ret = svdm_version;
		goto unlock;
	}
	dev_dbg(dp_rx->dev, "[DP_RX]   SVDM version: %d\n", svdm_version);

	header = DP_HEADER(dp_rx, svdm_version, CMD_ATTENTION);
	status_vdo = dp_rx_build_status_vdo(dp_rx);

	dev_dbg(dp_rx->dev, "[DP_RX] Attention VDM constructed:\n");
	dev_dbg(dp_rx->dev, "[DP_RX]   Header: 0x%08x\n", header);
	dp_rx_dump_vdm_header(dp_rx->dev, header);
	dev_dbg(dp_rx->dev, "[DP_RX]   Status VDO: 0x%08x\n", status_vdo);

	if (dp_rx->port && dp_rx->port->ops && dp_rx->port->ops->vdm) {
		ret = dp_rx->port->ops->vdm((struct typec_altmode *)dp_rx->port,
					     header, &status_vdo, 2);
		if (ret)
			dev_err(dp_rx->dev, "[DP_RX] [WQ] Failed to send Attention: %d\n", ret);
		else
			dev_info(dp_rx->dev, "[DP_RX] [WQ] Attention VDM sent successfully\n");
	} else {
		dev_err(dp_rx->dev, "[DP_RX] [WQ] No port ops to send Attention!\n");
	}

unlock:
	kfree(aw);
	mutex_unlock(&dp_rx->lock);
}

/*
 * dp_rx_altmode_vdm - Handle VDM from DFP_D
 *
 * This is the main VDM handler for UFP_D. It processes Initiator VDMs
 * from DFP_D and generates appropriate responses.
 */
static int dp_rx_altmode_vdm(struct typec_altmode *alt,
			      const u32 hdr, const u32 *vdo, int count)
{
	struct dp_rx_altmode *dp_rx = typec_altmode_get_drvdata(alt);
	int cmd_type = PD_VDO_CMDT(hdr);
	int cmd = PD_VDO_CMD(hdr);
	int svdm_version;
	u32 response[8] = {};
	int rlen = 0;
	int ret = 0;

	mutex_lock(&dp_rx->lock);

	/*
	 * A new VDM from DFP_D means the previous VDM response exchange is
	 * complete.  Wake any pending Attention work so it can proceed.
	 */
	if (dp_rx->vdm_pending)
		dp_rx->vdm_pending = false;

	dev_dbg(dp_rx->dev, "\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
	dev_dbg(dp_rx->dev, "[DP_RX] === VDM Received from DFP_D ===\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
	dev_dbg(dp_rx->dev, "[DP_RX] Header: 0x%08x\n", hdr);
	dp_rx_dump_vdm_header(dp_rx->dev, hdr);
	dp_rx_dump_vdos(dp_rx->dev, vdo, count);
	dev_dbg(dp_rx->dev, "[DP_RX] Current driver state: %s\n",
		 dp_rx_state_name(dp_rx->state));

	/* UFP_D handles Initiator VDMs from DFP_D */
	if (cmd_type != CMDT_INIT) {
		dev_warn(dp_rx->dev, "[DP_RX] Unexpected VDM type: %d\n", cmd_type);
		goto unlock;
	}

	/* Get SVDM version for response */
	svdm_version = typec_altmode_get_svdm_version(alt);
	if (svdm_version < 0) {
		dev_err(dp_rx->dev, "[DP_RX] Failed to get SVDM version\n");
		ret = svdm_version;
		goto unlock;
	}

	/* Process command */
	switch (cmd) {
	case CMD_ENTER_MODE:
		ret = dp_rx_handle_enter_mode(dp_rx, hdr, vdo, count);
		if (!ret) {
			/* ACK */
			response[0] = hdr | VDO_CMDT(CMDT_RSP_ACK);
			response[0] = (response[0] & ~VDO_SVDM_VERS_MASK) |
				      VDO_SVDM_VERS(svdm_version);
			rlen = 1;
		}
		break;

	case CMD_EXIT_MODE:
		ret = dp_rx_handle_exit_mode(dp_rx, hdr, vdo, count);
		if (!ret) {
			/* ACK */
			response[0] = hdr | VDO_CMDT(CMDT_RSP_ACK);
			response[0] = (response[0] & ~VDO_SVDM_VERS_MASK) |
				      VDO_SVDM_VERS(svdm_version);
			rlen = 1;
		}
		break;

	case DP_CMD_STATUS_UPDATE:
		ret = dp_rx_handle_status_update(dp_rx, hdr, vdo, count,
						  &response[1]);
		if (ret > 0) {
			/* ACK + Status VDO */
			response[0] = hdr | VDO_CMDT(CMDT_RSP_ACK);
			response[0] = (response[0] & ~VDO_SVDM_VERS_MASK) |
				      VDO_SVDM_VERS(svdm_version);
			rlen = ret + 1;  /* header + VDOs */
			ret = 0;
		}
		break;

	case DP_CMD_CONFIGURE:
		ret = dp_rx_handle_configure(dp_rx, hdr, vdo, count);
		if (!ret) {
			/* ACK */
			response[0] = hdr | VDO_CMDT(CMDT_RSP_ACK);
			response[0] = (response[0] & ~VDO_SVDM_VERS_MASK) |
				      VDO_SVDM_VERS(svdm_version);
			rlen = 1;
		}
		break;

	default:
		dev_warn(dp_rx->dev, "[DP_RX] Unsupported command: %d\n", cmd);
		/* NAK for unsupported commands */
		response[0] = hdr | VDO_CMDT(CMDT_RSP_NAK);
		response[0] = (response[0] & ~VDO_SVDM_VERS_MASK) |
			      VDO_SVDM_VERS(svdm_version);
		rlen = 1;
		break;
	}

	/* If error occurred, send NAK */
	if (ret < 0) {
		dev_err(dp_rx->dev, "[DP_RX] Command failed: %d, sending NAK\n", ret);
		response[0] = hdr | VDO_CMDT(CMDT_RSP_NAK);
		response[0] = (response[0] & ~VDO_SVDM_VERS_MASK) |
			      VDO_SVDM_VERS(svdm_version);
		rlen = 1;
		ret = 0;  /* Don't propagate error, we sent NAK */
	}

	/* Send response */
	if (rlen > 0) {
		dev_dbg(dp_rx->dev, "\n");
		dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
		dev_dbg(dp_rx->dev, "[DP_RX] === Sending Response to DFP_D ===\n");
		dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n");
		dev_dbg(dp_rx->dev, "[DP_RX] Response header: 0x%08x\n", response[0]);
		dp_rx_dump_vdm_header(dp_rx->dev, response[0]);
		if (rlen > 1) {
			dev_dbg(dp_rx->dev, "[DP_RX] Response VDOs (%d):\n", rlen - 1);
			for (int i = 1; i < rlen; i++) {
				dev_dbg(dp_rx->dev, "[DP_RX]   VDO[%d]: 0x%08x\n",
					 i - 1, response[i]);
			}
		}

		/* Use port altmode ops->vdm to send response */
		if (dp_rx->port && dp_rx->port->ops && dp_rx->port->ops->vdm) {
			/*
			 * Response is now queued into TCPM.  Block subsequent
			 * Attention sends until the next VDM from DFP_D arrives
			 * (confirming this exchange is done).
			 */
			dp_rx->vdm_pending = true;

			dev_dbg(dp_rx->dev, "[DP_RX] Calling port ops->vdm() to send response...\n");
			ret = dp_rx->port->ops->vdm((struct typec_altmode *)dp_rx->port,
						     response[0], &response[1], rlen);
			if (ret) {
				dev_err(dp_rx->dev, "[DP_RX] Failed to send response: %d\n", ret);
				dp_rx->vdm_pending = false;
			} else {
				dev_info(dp_rx->dev, "[DP_RX] Response queued successfully\n");
			}
		} else {
			dev_err(dp_rx->dev, "[DP_RX] No port ops to send response!\n");
			dev_err(dp_rx->dev, "[DP_RX]   port=%p, port->ops=%p\n",
				dp_rx->port, dp_rx->port ? dp_rx->port->ops : NULL);
			ret = -EOPNOTSUPP;
		}
		dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n\n");
	}

unlock:
	if (!dp_rx->vdm_pending)
		wake_up(&dp_rx->vdm_done);
	mutex_unlock(&dp_rx->lock);
	return ret;
}

/*
 * dp_rx_send_attention - Send Attention VDM to notify DFP_D of HPD change
 *
 * This is called when HPD state changes. It's an optional feature -
 * not all DFP_D devices support receiving Attention VDMs.
 */
static int dp_rx_send_attention(struct typec_altmode *alt, bool hpd_high, bool hpd_irq)
{
	struct dp_rx_altmode *dp_rx = typec_altmode_get_drvdata(alt);
	struct dp_rx_attention_work *aw;
	int ret = 0;

	if (!dp_rx || !dp_rx->wq)
		return -ENODEV;

	/* Queue Attention send to workqueue — it waits for any pending
	 * VDM response to complete before actually transmitting. */
	aw = kzalloc(sizeof(*aw), GFP_KERNEL);
	if (!aw) {
		dev_err(dp_rx->dev, "[DP_RX] Failed to allocate attention work\n");
		return -ENOMEM;
	}
	aw->dp_rx = dp_rx;
	aw->hpd_high = hpd_high;
	aw->hpd_irq = hpd_irq;
	INIT_WORK(&aw->work, dp_rx_do_send_attention);
	queue_work(dp_rx->wq, &aw->work);
	dev_info(dp_rx->dev, "[DP_RX] Attention work queued (will wait for pending response)\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ========================================\n\n");

	return ret;
}

/*
 * dp_rx_altmode_attention - Attention callback from typec framework.
 *
 * Called via typec_altmode_attention(port_altmode, vdo) when a consumer
 * (e.g. the PHY driver) wants to notify the DFP_D of an HPD change.
 * Extracts HPD state from the Status VDO and delegates to
 * dp_rx_send_attention which queues the actual send to the workqueue.
 */
static void dp_rx_altmode_attention(struct typec_altmode *alt, u32 vdo)
{
	struct dp_rx_altmode *dp_rx;

	if (!alt)
		return;

	dp_rx = typec_altmode_get_drvdata(alt);
	if (!dp_rx)
		return;

	dev_dbg(dp_rx->dev, "[DP_RX] attention callback: status_vdo=0x%08x\n", vdo);

	dp_rx_send_attention(alt,
			     !!(vdo & DP_STATUS_HPD_STATE),
			     !!(vdo & DP_STATUS_IRQ_HPD));
}

static const struct typec_altmode_ops dp_rx_altmode_ops = {
	.vdm	  = dp_rx_altmode_vdm,
	.attention = dp_rx_altmode_attention,
};

/*
 * ============================================================
 * Sysfs interface for HPD testing
 * ============================================================
 */

/*
 * hpd_state_show - Show current HPD state
 *
 * Usage: cat /sys/bus/typec/devices/<port>/<altmode>/hpd_state
 */
static ssize_t hpd_state_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct typec_altmode *alt = to_typec_altmode(dev);
	struct dp_rx_altmode *dp_rx = typec_altmode_get_drvdata(alt);

	if (!dp_rx)
		return -ENODEV;

	return sysfs_emit(buf, "%d\n", dp_rx->hpd_state ? 1 : 0);
}

/*
 * hpd_trigger_store - Trigger HPD via Attention VDM
 *
 * Usage:
 *   echo 1 > /sys/bus/typec/devices/<port>/<altmode>/hpd_trigger  # HPD HIGH
 *   echo 0 > /sys/bus/typec/devices/<port>/<altmode>/hpd_trigger  # HPD LOW
 *   echo 2 > /sys/bus/typec/devices/<port>/<altmode>/hpd_trigger  # HPD IRQ (pulse)
 *
 * Values:
 *   0 = HPD LOW (disconnect)
 *   1 = HPD HIGH (connect)
 *   2 = HPD IRQ (keep HIGH + send IRQ for EDID re-read)
 *
 * Note: If in IDLE state (DP Alt Mode not entered), this will only update
 *       the internal HPD state. The Attention VDM will only be sent when
 *       in ENTERED or CONFIGURED state.
 */
static ssize_t hpd_trigger_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct typec_altmode *alt = to_typec_altmode(dev);
	struct dp_rx_altmode *dp_rx = typec_altmode_get_drvdata(alt);
	unsigned int val;
	bool hpd_high, hpd_irq;
	int ret;

	if (!dp_rx)
		return -ENODEV;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;

	dev_info(dp_rx->dev, "[DP_RX] sysfs: HPD trigger request: %u\n", val);
	dev_info(dp_rx->dev, "[DP_RX] sysfs: Current state: %s\n",
		 dp_rx_state_name(dp_rx->state));

	switch (val) {
	case 0:
		/* HPD LOW */
		hpd_high = false;
		hpd_irq = false;
		dev_info(dp_rx->dev, "[DP_RX] sysfs: Setting HPD LOW (disconnect)\n");
		break;
	case 1:
		/* HPD HIGH */
		hpd_high = true;
		hpd_irq = false;
		dev_info(dp_rx->dev, "[DP_RX] sysfs: Setting HPD HIGH (connect)\n");
		break;
	case 2:
		/* HPD IRQ (keep HIGH + IRQ) */
		hpd_high = true;
		hpd_irq = true;
		dev_info(dp_rx->dev, "[DP_RX] sysfs: Setting HPD IRQ (re-read EDID)\n");
		break;
	default:
		dev_err(dp_rx->dev, "[DP_RX] sysfs: Invalid value %u (use 0, 1, or 2)\n", val);
		return -EINVAL;
	}

	/*
	 * If in IDLE state, just update internal HPD state.
	 * The HPD will be reported when DFP_D queries Status Update.
	 * Attention VDM can only be sent after Enter Mode.
	 */
	if (dp_rx->state == DP_RX_STATE_IDLE) {
		mutex_lock(&dp_rx->lock);
		dp_rx->hpd_state = hpd_high;
		if (hpd_irq)
			dp_rx->hpd_irq = true;
		mutex_unlock(&dp_rx->lock);

		dev_info(dp_rx->dev, "[DP_RX] sysfs: Updated internal HPD state (IDLE mode)\n");
		dev_info(dp_rx->dev, "[DP_RX] sysfs:   hpd_state=%d, hpd_irq=%d\n",
			 dp_rx->hpd_state, dp_rx->hpd_irq);
		dev_info(dp_rx->dev, "[DP_RX] sysfs:   Note: Attention VDM not sent (not in DP mode)\n");
		dev_info(dp_rx->dev, "[DP_RX] sysfs:   HPD will be reported when DFP_D queries status\n");
		return count;
	}

	/* In ENTERED or CONFIGURED state, send Attention VDM */
	ret = dp_rx_send_attention(alt, hpd_high, hpd_irq);
	if (ret) {
		dev_err(dp_rx->dev, "[DP_RX] sysfs: HPD trigger failed: %d\n", ret);
		return ret;
	}

	dev_info(dp_rx->dev, "[DP_RX] sysfs: HPD trigger success (Attention VDM sent)\n");
	return count;
}

/*
 * dp_rx_state_show - Show current DP RX driver state
 */
static ssize_t dp_rx_state_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	struct typec_altmode *alt = to_typec_altmode(dev);
	struct dp_rx_altmode *dp_rx = typec_altmode_get_drvdata(alt);

	if (!dp_rx)
		return -ENODEV;

	return sysfs_emit(buf, "%s\n", dp_rx_state_name(dp_rx->state));
}

/*
 * dp_rx_conf_show - Show current configuration VDO
 */
static ssize_t dp_rx_conf_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct typec_altmode *alt = to_typec_altmode(dev);
	struct dp_rx_altmode *dp_rx = typec_altmode_get_drvdata(alt);

	if (!dp_rx)
		return -ENODEV;

	return sysfs_emit(buf, "0x%08x\n", dp_rx->conf);
}

static DEVICE_ATTR_RO(hpd_state);
static DEVICE_ATTR_WO(hpd_trigger);
static DEVICE_ATTR(state, 0444, dp_rx_state_show, NULL);
static DEVICE_ATTR(conf, 0444, dp_rx_conf_show, NULL);

static struct attribute *dp_rx_attrs[] = {
	&dev_attr_hpd_state.attr,
	&dev_attr_hpd_trigger.attr,
	&dev_attr_state.attr,
	&dev_attr_conf.attr,
	NULL,
};

static const struct attribute_group dp_rx_attr_group = {
	.attrs = dp_rx_attrs,
};

/*
 * dp_rx_dump_capability_vdo - Dump Capability VDO details
 */
static void dp_rx_dump_capability_vdo(struct device *dev, u32 vdo, const char *name)
{
	u8 cap = DP_CAP_CAPABILITY(vdo);
	u8 receptacle = (DP_CAP_RECEPTACLE & vdo);
	u8 usb2_0 = (DP_CAP_USB & vdo);
	u8 dfp_d_pin = DP_CAP_PIN_ASSIGN_DFP_D(vdo);
	u8 ufp_d_pin = DP_CAP_PIN_ASSIGN_UFP_D(vdo);

	dev_dbg(dev, "[DP_RX] %s Capability VDO: 0x%08x\n", name, vdo);
	dev_dbg(dev, "[DP_RX]   Bit 1-0: DP Capability = 0x%x (%s%s)\n",
		cap,
		(cap & DP_CAP_UFP_D) ? "UFP_D " : "",
		(cap & DP_CAP_DFP_D) ? "DFP_D" : "");
	dev_dbg(dev, "[DP_RX]   Bit 6: Receptacle = %d (%s)\n",
		receptacle, receptacle ? "Yes" : "Plug");
	dev_dbg(dev, "[DP_RX]   Bit 7: USB 2.0 = %d (%s)\n",
		usb2_0, usb2_0 ? "Not used" : "Required");
	dev_dbg(dev, "[DP_RX]   Bit 15-8: DFP_D Pin Assignment = 0x%02x\n", dfp_d_pin);
	if (dfp_d_pin) {
		dev_dbg(dev, "[DP_RX]     Supports:%s%s%s%s%s%s\n",
			(dfp_d_pin & BIT(DP_PIN_ASSIGN_A)) ? " A" : "",
			(dfp_d_pin & BIT(DP_PIN_ASSIGN_B)) ? " B" : "",
			(dfp_d_pin & BIT(DP_PIN_ASSIGN_C)) ? " C" : "",
			(dfp_d_pin & BIT(DP_PIN_ASSIGN_D)) ? " D" : "",
			(dfp_d_pin & BIT(DP_PIN_ASSIGN_E)) ? " E" : "",
			(dfp_d_pin & BIT(DP_PIN_ASSIGN_F)) ? " F" : "");
	} else {
		dev_dbg(dev, "[DP_RX]     Supports: (none)\n");
	}
	dev_dbg(dev, "[DP_RX]   Bit 23-16: UFP_D Pin Assignment = 0x%02x\n", ufp_d_pin);
	if (ufp_d_pin) {
		dev_dbg(dev, "[DP_RX]     Supports:%s%s%s%s%s%s\n",
			(ufp_d_pin & BIT(DP_PIN_ASSIGN_A)) ? " A" : "",
			(ufp_d_pin & BIT(DP_PIN_ASSIGN_B)) ? " B" : "",
			(ufp_d_pin & BIT(DP_PIN_ASSIGN_C)) ? " C" : "",
			(ufp_d_pin & BIT(DP_PIN_ASSIGN_D)) ? " D" : "",
			(ufp_d_pin & BIT(DP_PIN_ASSIGN_E)) ? " E" : "",
			(ufp_d_pin & BIT(DP_PIN_ASSIGN_F)) ? " F" : "");
	} else {
		dev_dbg(dev, "[DP_RX]     Supports: (none)\n");
	}
}

/*
 * dp_rx_altmode_probe - Probe function for UFP_D Alt Mode
 */
static int dp_rx_altmode_probe(struct typec_altmode *alt)
{
	const struct typec_altmode *port = typec_altmode_get_partner(alt);
	struct typec_altmode __maybe_unused *plug = typec_altmode_get_plug(alt, TYPEC_PLUG_SOP_P);
	struct dp_rx_altmode *dp_rx;
	struct device *dev = &alt->dev;
	u8 port_cap, partner_cap;
	int ret;

	dev_dbg(dev, "\n");
	dev_dbg(dev, "[DP_RX] ================================================\n");
	dev_dbg(dev, "[DP_RX] === DisplayPort RX (UFP_D) Driver Probe ===\n");
	dev_dbg(dev, "[DP_RX] ================================================\n");

	/* Verify this is DP Alt Mode */
	dev_dbg(dev, "[DP_RX] Step 1: Verifying Alt Mode identity\n");
	dev_dbg(dev, "[DP_RX]   Alt Mode SVID: 0x%04x\n", alt->svid);
	if (alt->svid != USB_TYPEC_DP_SID) {
		dev_err(dev, "[DP_RX] Not DisplayPort Alt Mode (expected 0x%04x)\n",
			USB_TYPEC_DP_SID);
		return -ENODEV;
	}
	dev_dbg(dev, "[DP_RX] Confirmed DisplayPort Alt Mode\n");

	/* Check pin assignment compatibility */
	dev_dbg(dev, "[DP_RX] Step 2: Checking port altmode\n");
	if (!port) {
		dev_err(dev, "[DP_RX] No port altmode available\n");
		return -ENODEV;
	}
	dev_dbg(dev, "[DP_RX] Port altmode present\n");

	/* Dump capability VDOs */
	dev_dbg(dev, "[DP_RX] Step 3: Analyzing capabilities\n");
	dp_rx_dump_capability_vdo(dev, port->vdo, "Port");
	dp_rx_dump_capability_vdo(dev, alt->vdo, "Partner");

	/*
	 * IMPORTANT: Role detection to avoid conflict with displayport.c
	 *
	 * This driver is for UFP_D (Sink) role only.
	 * If the port is DFP_D only, let displayport.c handle it instead.
	 */
	dev_dbg(dev, "[DP_RX] Step 4: Role detection (conflict avoidance)\n");
	port_cap = DP_CAP_CAPABILITY(port->vdo);
	partner_cap = DP_CAP_CAPABILITY(alt->vdo);

	dev_dbg(dev, "[DP_RX]   Port capability: 0x%x (%s)\n",
		 port_cap,
		 port_cap == DP_CAP_UFP_D ? "UFP_D only" :
		 port_cap == DP_CAP_DFP_D ? "DFP_D only" :
		 port_cap == (DP_CAP_DFP_D | DP_CAP_UFP_D) ? "Both UFP_D+DFP_D" : "Unknown");
	dev_dbg(dev, "[DP_RX]   Partner capability: 0x%x (%s)\n",
		 partner_cap,
		 partner_cap == DP_CAP_UFP_D ? "UFP_D only" :
		 partner_cap == DP_CAP_DFP_D ? "DFP_D only" :
		 partner_cap == (DP_CAP_DFP_D | DP_CAP_UFP_D) ? "Both UFP_D+DFP_D" : "Unknown");

	/*
	 * Skip probe if:
	 * 1. Port is DFP_D only (we need UFP_D role)
	 * 2. Partner is UFP_D only (invalid - can't have two sinks)
	 */
	dev_dbg(dev, "[DP_RX]   Checking role compatibility...\n");
	if (port_cap == DP_CAP_DFP_D) {
		dev_info(dev, "[DP_RX] Port is DFP_D only - skipping UFP_D driver\n");
		dev_info(dev, "[DP_RX]   Reason: This port acts as DP Source, not Sink\n");
		dev_info(dev, "[DP_RX]   Solution: Let displayport.c (DFP_D driver) handle this\n");
		return -ENODEV;
	}

	if (partner_cap == DP_CAP_UFP_D) {
		dev_warn(dev, "[DP_RX] Partner is UFP_D only - invalid configuration\n");
		dev_warn(dev, "[DP_RX]   Reason: Cannot have two DisplayPort sinks\n");
		return -ENODEV;
	}

	/* Check if another driver is already bound */
	dev_dbg(dev, "[DP_RX] Step 5: Checking for driver conflicts\n");
	dev_dbg(dev, "[DP_RX]   Current ops: %ps\n", alt->ops);
	if (alt->ops && alt->ops != &dp_rx_altmode_ops) {
		dev_warn(dev, "[DP_RX] Another driver already bound\n");
		dev_warn(dev, "[DP_RX]   Conflicting ops: %ps\n", alt->ops);
		dev_warn(dev, "[DP_RX]   Skipping to avoid conflict\n");
		return -EBUSY;
	}
	dev_dbg(dev, "[DP_RX] No driver conflicts detected\n");

	/*
	 * Check pin assignment compatibility for UFP_D role
	 *
	 * As UFP_D (DP Sink), we receive DP signals from partner (DFP_D Source).
	 * Nominally we check UFP_D pins (ours) against DFP_D pins (partner's).
	 *
	 * Some devices encode their capability only in one of the two pin fields:
	 * e.g. a display with UFP_D_pins=None and DFP_D_pins=C+E, or a source
	 * with DFP_D_pins=None and UFP_D_pins=C+E.  When the primary field is
	 * zero, fall back to the complementary field so we don't reject a
	 * perfectly workable combination.
	 */
	{
		u8 port_pins = DP_CAP_PIN_ASSIGN_UFP_D(port->vdo);
		u8 partner_pins = DP_CAP_PIN_ASSIGN_DFP_D(alt->vdo);

		/* Fallback: use DFP_D field when UFP_D field is empty */
		if (!port_pins)
			port_pins = DP_CAP_PIN_ASSIGN_DFP_D(port->vdo);
		/* Fallback: use UFP_D field when DFP_D field is empty */
		if (!partner_pins)
			partner_pins = DP_CAP_PIN_ASSIGN_UFP_D(alt->vdo);

		dev_dbg(dev, "[DP_RX] Step 6: Checking pin assignment compatibility\n");
		dev_dbg(dev, "[DP_RX]   Port pins (UFP_D or fallback DFP_D): 0x%02x\n",
			 (unsigned int)port_pins);
		dev_dbg(dev, "[DP_RX]   Partner pins (DFP_D or fallback UFP_D): 0x%02x\n",
			 (unsigned int)partner_pins);
		if (!partner_pins) {
			/*
			 * Partner advertises DFP_D capability but provides no pin
			 * bitmask in Discover_Modes.  Some DFP_D implementations
			 * omit pin assignments here and select the pin exclusively
			 * via the Configure command.  Accept the probe and let the
			 * incoming Configure VDO dictate the pin assignment.
			 */
			dev_info(dev, "[DP_RX] Partner provides no pin bitmask, "
				 "will accept any Configure\n");
		} else if (!(port_pins & partner_pins)) {
			dev_err(dev, "[DP_RX] Incompatible pin assignments (no common pins)\n");
			dev_err(dev, "[DP_RX]   Port pins (UFP_D|DFP_D): 0x%02x|0x%02x\n",
				(unsigned int)DP_CAP_PIN_ASSIGN_UFP_D(port->vdo),
				(unsigned int)DP_CAP_PIN_ASSIGN_DFP_D(port->vdo));
			dev_err(dev, "[DP_RX]   Partner pins (DFP_D|UFP_D): 0x%02x|0x%02x\n",
				(unsigned int)DP_CAP_PIN_ASSIGN_DFP_D(alt->vdo),
				(unsigned int)DP_CAP_PIN_ASSIGN_UFP_D(alt->vdo));
			return -ENODEV;
		} else {
			dev_dbg(dev, "[DP_RX] Compatible pin assignments found: 0x%02x\n",
				 (unsigned int)(port_pins & partner_pins));
		}
	}

	dev_dbg(dev, "[DP_RX] All checks passed - proceeding with UFP_D driver\n");

	/* Allocate driver data */
	dev_dbg(dev, "[DP_RX] Step 7: Allocating driver data\n");
	dp_rx = devm_kzalloc(dev, sizeof(*dp_rx), GFP_KERNEL);
	if (!dp_rx) {
		dev_err(dev, "[DP_RX] Failed to allocate memory\n");
		return -ENOMEM;
	}

	/* Initialize driver data */
	dev_dbg(dev, "[DP_RX] Step 8: Initializing driver data\n");
	mutex_init(&dp_rx->lock);
	dp_rx->vdm_pending = false;
	init_waitqueue_head(&dp_rx->vdm_done);
	dp_rx->wq = create_singlethread_workqueue("dp_rx_altmode");
	dp_rx->port = port;
	dp_rx->alt = alt;
	dp_rx->dev = dev;
	dp_rx->state = DP_RX_STATE_IDLE;

	/* Initial HPD state: LOW */
	dp_rx->hpd_state = false;
	dp_rx->hpd_irq = false;
	dev_dbg(dev, "[DP_RX]   Initial state: %s\n",
		 dp_rx_state_name(dp_rx->state));
	dev_dbg(dev, "[DP_RX]   Initial HPD: LOW\n");

	/* Register driver */
	dev_dbg(dev, "[DP_RX] Step 9: Registering driver operations\n");
	alt->desc = "DisplayPort UFP_D (Sink/Receiver)";
	typec_altmode_set_ops(alt, &dp_rx_altmode_ops);
	typec_altmode_set_drvdata(alt, dp_rx);

	/* Create sysfs attributes for HPD testing */
	dev_dbg(dev, "[DP_RX] Step 10: Creating sysfs attributes\n");
	ret = sysfs_create_group(&dev->kobj, &dp_rx_attr_group);
	if (ret) {
		dev_warn(dev, "[DP_RX] Failed to create sysfs group: %d (non-fatal)\n", ret);
	} else {
		dev_dbg(dev, "[DP_RX] sysfs attributes created:\n");
		dev_dbg(dev, "[DP_RX]   - hpd_state  (RO): Current HPD state\n");
		dev_dbg(dev, "[DP_RX]   - hpd_trigger (WO): Trigger HPD (0=LOW, 1=HIGH, 2=IRQ)\n");
		dev_dbg(dev, "[DP_RX]   - state      (RO): Driver state (IDLE/ENTERED/CONFIGURED)\n");
		dev_dbg(dev, "[DP_RX]   - conf       (RO): Configuration VDO\n");
	}

	dev_dbg(dev, "[DP_RX] ================================================\n");
	dev_dbg(dev, "[DP_RX] UFP_D Alt Mode Driver Registered\n");
	dev_dbg(dev, "[DP_RX] ================================================\n");
	dev_dbg(dev, "[DP_RX] Summary:\n");
	dev_dbg(dev, "[DP_RX]   Role: UFP_D (DisplayPort Sink/Receiver)\n");
	dev_info(dev, "[DP_RX]   SVID: 0x%04x Mode: %d VDO: 0x%08x\n",
		 alt->svid, alt->mode, alt->vdo);
	dev_info(dev, "[DP_RX]   Port UFP_D pins (we receive on): 0x%02x\n",
		 (unsigned int)DP_CAP_PIN_ASSIGN_UFP_D(port->vdo));
	dev_info(dev, "[DP_RX]   Partner DFP_D pins (they transmit on): 0x%02x\n",
		 (unsigned int)DP_CAP_PIN_ASSIGN_DFP_D(alt->vdo));
	dev_dbg(dev, "[DP_RX]   Ready to receive VDMs from DFP_D\n");
	dev_dbg(dev, "[DP_RX] ================================================\n\n");

	/* Check data role requirement for UFP_D */
	dp_rx_check_data_role_for_ufp_d(dp_rx);

	return 0;
}

static void dp_rx_altmode_remove(struct typec_altmode *alt)
{
	struct dp_rx_altmode *dp_rx = typec_altmode_get_drvdata(alt);

	if (!dp_rx)
		return;

	dev_dbg(dp_rx->dev, "\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ================================================\n");
	dev_dbg(dp_rx->dev, "[DP_RX] === Removing UFP_D Alt Mode Driver ===\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ================================================\n");
	dev_dbg(dp_rx->dev, "[DP_RX] Final state: %s\n",
		 dp_rx_state_name(dp_rx->state));
	dev_dbg(dp_rx->dev, "[DP_RX] Final HPD state: %s\n",
		 dp_rx->hpd_state ? "HIGH" : "LOW");
	dev_dbg(dp_rx->dev, "[DP_RX] Configuration VDO: 0x%08x\n", dp_rx->conf);

	/* Remove sysfs attributes */
	sysfs_remove_group(&alt->dev.kobj, &dp_rx_attr_group);
	dev_dbg(dp_rx->dev, "[DP_RX] sysfs attributes removed\n");

	dev_info(dp_rx->dev, "[DP_RX] Driver removed successfully\n");
	dev_dbg(dp_rx->dev, "[DP_RX] ================================================\n\n");

	/* Flush and destroy the attention workqueue */
	wake_up_all(&dp_rx->vdm_done);
	if (dp_rx->wq) {
		flush_workqueue(dp_rx->wq);
		destroy_workqueue(dp_rx->wq);
		dp_rx->wq = NULL;
	}
	dev_dbg(dp_rx->dev, "[DP_RX] Workqueue destroyed\n");
}

static const struct typec_device_id dp_rx_typec_id[] = {
	{ USB_TYPEC_DP_SID, USB_TYPEC_DP_MODE },
	{ },
};
MODULE_DEVICE_TABLE(typec, dp_rx_typec_id);

static struct typec_altmode_driver dp_rx_altmode_driver = {
	.id_table = dp_rx_typec_id,
	.probe = dp_rx_altmode_probe,
	.remove = dp_rx_altmode_remove,
	.driver = {
		.name = "typec_displayport_rx",
		.owner = THIS_MODULE,
	},
};
module_typec_altmode_driver(dp_rx_altmode_driver);

MODULE_AUTHOR("Stanley Chang <stanley_chang@realtek.com>");
MODULE_DESCRIPTION("USB Type-C DisplayPort Alt Mode Driver - UFP_D (Sink/Receiver)");
MODULE_LICENSE("GPL");
