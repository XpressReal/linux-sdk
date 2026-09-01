// SPDX-License-Identifier: GPL-2.0-only
#include <drm/drm_of.h>
#include <drm/drm_print.h>

#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/clk-provider.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <sound/hdmi-codec.h>
#include <linux/debugfs.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_file.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_edid.h>
#include <drm/display/drm_dp_helper.h>

#include <video/videomode.h>
#include "rtk_drm_drv.h"
#include "rtk_dptx_kent_reg.h"
#include "rtk_edp_reg.h"
#include "rtk_crt_reg.h"
#include "rtk_prince_dptx.h"
#include "rtk_prince_dptx_phy.h"
#include "rtk_prince_dptx_phy.h"
#include "rtk_dp_utils.h"

#define MAIN2_MUXPAD2 0x8
#define MAIN2_MUXPAD2_GPIO_22 BIT(24)

/* AUX_FIFO_CTRL */
#define NA_FIFO_RST			(1 << 0)
#define I2C_FIFO_RST		(1 << 1)
#define FORCE_REQ_INTVAL	(1 << 2)
#define READ_FAIL_AUTO_EN	(1 << 3)
#define I2C_REQ_LEN_SEL		(1 << 4)
#define AUX_FIFO_CTRL_ALL (NA_FIFO_RST | I2C_FIFO_RST | \
			FORCE_REQ_INTVAL | READ_FAIL_AUTO_EN | I2C_REQ_LEN_SEL)
#define AUX_FIFO_CTRL_RESET (NA_FIFO_RST | I2C_FIFO_RST)

/* AUX_IRQ_EN */
#define TIMEOUT		(1 << 0)
#define RETRY		(1 << 1)
#define NACK		(1 << 2)
#define READFAIL	(1 << 3)
#define RXERROR		(1 << 4)
#define AUXDONE		(1 << 5)
#define ALPM		(1 << 6)
#define AUX_ALL_IRQ	(TIMEOUT | RETRY | NACK | READFAIL | \
			RXERROR | AUXDONE | ALPM)

/* AUX_RETRY_1 */
#define RETRY_LOCK (1 << 4)

/* AUX_RETRY_2 */
#define RETRY_ERROR_EN	 (1 << 3)
#define RETRY_NACK_EN	 (1 << 4)
#define RETRY_TIMEOUT_EN (1 << 5)
#define RETRY_DEFER_EN	 (1 << 6)
#define RETRY_EN		 (1 << 7)

/* AUXTX_TRAN_CTRL */
#define TX_START	(1 << 0)
#define TX_ADDRONLY	(1 << 7)
/* AUX_TX_CTRL */
#define AUX_EN		(1 << 0)
/* DPTX_IRQ_CTRL */
#define DPTX_IRQ_EN	(1 << 7)
/* AUX_TIMEOUT */
#define AUX_TIMEOUT_EN (1 << 7)
/* AUX_DIG_PHY2 */
#define AUX_PN_SWAP	(1 << 0)
/* HPD_CTRL */
#define HPD_CTRL_EN (1 << 7)
#define HPD_CTRL_CLK_DIV (1 << 5)
#define HPD_CTRL_DEB (1 << 2)
/* HPD_IRQ */
#define HPD_IRQ_IHPD (1 << 7)
#define HPD_IRQ_SHPD (1 << 6)
#define HPD_IRQ_LHPD (1 << 5)
#define HPD_IRQ_UHPD (1 << 4)
#define HPD_IRQ_UNHPD (1 << 3)
#define HPD_IRQ_RHPD (1 << 2)
#define HPD_IRQ_FHPD (1 << 1)
#define HPD_IRQ_ALL (HPD_IRQ_IHPD | HPD_IRQ_SHPD | HPD_IRQ_LHPD |\
	HPD_IRQ_UHPD | HPD_IRQ_UNHPD | HPD_IRQ_RHPD | HPD_IRQ_FHPD)
/* HPD_IRQ_EN */
#define HPD_IRQ_IHPD_EN (1 << 7)
#define HPD_IRQ_SHPD_EN (1 << 6)
#define HPD_IRQ_LHPD_EN (1 << 5)
#define HPD_IRQ_UHPD_EN (1 << 4)
#define HPD_IRQ_UNHPD_EN (1 << 3)
#define HPD_IRQ_RHPD_EN (1 << 2)
#define HPD_IRQ_FHPD_EN (1 << 1)
#define HPD_IRQ_EN_ALL (HPD_IRQ_IHPD_EN | HPD_IRQ_SHPD_EN | HPD_IRQ_LHPD_EN |\
	HPD_IRQ_UHPD_EN | HPD_IRQ_UNHPD_EN | HPD_IRQ_RHPD_EN | HPD_IRQ_FHPD_EN)

#define DP_DPCD_ADAPTER_CAP 0x220f

#define RTK_DP_AUX_WAIT_REPLY_COUNT 20
// #define RTK_POLL_HPD_INTERVAL_MS 1
#define RTK_HPD_GPIO_PLUG_DEB_TIME_US 100
#define RTK_HPD_GPIO_UNPLUG_DEB_TIME_US 50000
#define RTK_HPD_SHORT_PULSE_THRESHOLD_MS 5
#define RTK_DP_MAX_LINK_RATE DP_LINK_RATE_8_1

// #define RTK_DP_MAX_CLOCK_K 348500 // 4096x2160p60

/* ISO */
#define ISO_SOFT_RESET    (0x88)
#define ISO_CLOCK_ENABLE  (0x8c)
#define RSTN_USB3_P2_MDIO (1 << 28)
#define CLK_EN_USB_P4     (1 << 0)

/* AUDIO */
#define AUDIO_EN BIT(0)

#define MAX_LINK_TRAIN_LOOP 5

enum LINK_RATE {
	LINK_RATE_UNSPECIFIED,
	LINK_RATE_1_62,
	LINK_RATE_2_7,
	LINK_RATE_5_4,
	LINK_RATE_8_1,
};

enum LANE_COUNT {
	LANE_COUNT_UNSPECIFIED,
	LANE_COUNT_1,
	LANE_COUNT_2,
	LANE_COUNT_4 = 4,
};

enum TRAIN_PATTERN {
	TRAIN_PATTERN_UNSPECIFIED,
	TRAIN_PATTERN_2 = 2,
	TRAIN_PATTERN_3 = 3,
	TRAIN_PATTERN_4 = 4,
};

static const struct drm_prop_enum_list link_rate_list[] = {
	{LINK_RATE_UNSPECIFIED, "Unspecified"},
	{LINK_RATE_1_62, "1.62G"},
	{LINK_RATE_2_7, "2.7G"},
	{LINK_RATE_5_4, "5.4G"},
	{LINK_RATE_8_1, "8.1G"},
};

static const struct drm_prop_enum_list lane_count_list[] = {
	{LANE_COUNT_UNSPECIFIED, "Unspecified"},
	{LANE_COUNT_1, "1_lane"},
	{LANE_COUNT_2, "2_lane"},
	{LANE_COUNT_4, "4_lane"},
};

static const struct drm_prop_enum_list train_pattern_list[] = {
	{TRAIN_PATTERN_UNSPECIFIED, "Unspecified"},
	{TRAIN_PATTERN_2, "TPS2"},
	{TRAIN_PATTERN_3, "TPS3"},
	{TRAIN_PATTERN_4, "TPS4"},
};

enum VIDEO_ID_CODE {
	VIC_720X480P60 = 2,
	VIC_1280X720P60 = 4,
	VIC_1920X1080I60 = 5,
	VIC_720X480I60 = 6,
	VIC_1920X1080P60 = 16,
	VIC_720X576P50 = 17,
	VIC_1280X720P50 = 19,
	VIC_1920X1080I50 = 20,
	VIC_720X576I50 = 21,
	VIC_1920X1080P50 = 31,
	VIC_1920X1080P24 = 32,
	VIC_1920X1080P25 = 33,
	VIC_1920X1080P30 = 34,
	VIC_1280X720P24 = 60,
	VIC_1280X720P25 = 61,
	VIC_1280X720P30 = 62,
	VIC_1920X1080P120 = 63,
	VIC_3840X2160P24 = 93,
	VIC_3840X2160P25 = 94,
	VIC_3840X2160P30 = 95,
	VIC_3840X2160P50 = 96,
	VIC_3840X2160P60 = 97,
	VIC_4096X2160P24 = 98,
	VIC_4096X2160P25 = 99,
	VIC_4096X2160P30 = 100,
	VIC_4096X2160P50 = 101,
	VIC_4096X2160P60 = 102,
};

struct rtk_prince_dptx_platform_data {
	unsigned int type;
	int connector_type;
	int (*get_modes)(struct rtk_prince_dptx *dptx);
	const struct drm_encoder_helper_funcs *helper_funcs;
	int max_link_bw;
	enum link_lane_count max_lane_count;
};

static const struct drm_display_mode default_mode = {
	.clock = 148500000 / 1000,
	.hdisplay = 1920,
	.hsync_start = 1920 + 88,
	.hsync_end = 1920 + 88 + 44,
	.htotal = 1920 + 88 + 44 + 148,
	.vdisplay = 1080,
	.vsync_start = 1080 + 4,
	.vsync_end = 1080 + 4 + 5,
	.vtotal = 1080 + 4 + 5 + 36,

	.width_mm = 530, // 340
	.height_mm = 300, // 190
};

int rtk_edp_pattern_gen;
module_param(rtk_edp_pattern_gen, int, 0644);
MODULE_PARM_DESC(rtk_edp_pattern_gen, "Debug level (0-1)");

#if defined(CONFIG_DEBUG_FS)

#define DEBUGFS_REG32(_name) { .name = #_name, .offset = _name }

static const struct debugfs_reg32 rtk_crt_regs[] = {
	DEBUGFS_REG32(SYS_PLL_HDMI),
	DEBUGFS_REG32(SYS_PLL_HDMI2),
	DEBUGFS_REG32(SYS_PLL_HDMI3),
	DEBUGFS_REG32(SYS_DISP_PLL_DIV2),
	DEBUGFS_REG32(SYS_PLL_HDMI_SD1),
	DEBUGFS_REG32(SYS_PLL_HDMI_SD2),
	DEBUGFS_REG32(SYS_PLL_HDMI_SD4),
	DEBUGFS_REG32(SYS_PLL_HDMI_SD5),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO1),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO2),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO3),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO4),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO6),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO7),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO8),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO9),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO10),
	DEBUGFS_REG32(SYS_PLL_HDMI_LDO11),
	DEBUGFS_REG32(SYS_PLL_HDMITX2P1_SD1),
	DEBUGFS_REG32(SYS_PLL_HDMITX2P1_SD2),
	DEBUGFS_REG32(SYS_PLL_HDMITX2P1_SD4),
	DEBUGFS_REG32(SYS_PLL_HDMITX2P1_SD5),
	DEBUGFS_REG32(SYS_PLL_HDMITX2P1_SD6),
};

static const struct debugfs_reg32 rtk_prince_edp_regs[] = {
	/* sst msa */
	DEBUGFS_REG32(MN_STRM_ATTR_HTT_M),
	DEBUGFS_REG32(MN_STRM_ATTR_HTT_L),
	DEBUGFS_REG32(MN_STRM_ATTR_HST_M),
	DEBUGFS_REG32(MN_STRM_ATTR_HST_L),
	DEBUGFS_REG32(MN_STRM_ATTR_HWD_M),
	DEBUGFS_REG32(MN_STRM_ATTR_HWD_L),
	DEBUGFS_REG32(MN_STRM_ATTR_HSW_M),
	DEBUGFS_REG32(MN_STRM_ATTR_HSW_L),
	DEBUGFS_REG32(MN_STRM_ATTR_VTTE_M),
	DEBUGFS_REG32(MN_STRM_ATTR_VTTE_L),
	DEBUGFS_REG32(MN_STRM_ATTR_VST_M),
	DEBUGFS_REG32(MN_STRM_ATTR_VST_L),
	DEBUGFS_REG32(MN_STRM_ATTR_VHT_M),
	DEBUGFS_REG32(MN_STRM_ATTR_VHT_L),
	DEBUGFS_REG32(MN_STRM_ATTR_VSW_M),
	DEBUGFS_REG32(MN_STRM_ATTR_VSW_L),
	/* sst */
	DEBUGFS_REG32(MN_M_VID_H),
	DEBUGFS_REG32(MN_M_VID_M),
	DEBUGFS_REG32(MN_M_VID_L),
	DEBUGFS_REG32(MN_N_VID_H),
	DEBUGFS_REG32(MN_N_VID_M),
	DEBUGFS_REG32(MN_N_VID_L),
	DEBUGFS_REG32(MN_VID_AUTO_EN_1),
	DEBUGFS_REG32(MSA_MISC0),
	DEBUGFS_REG32(MSA_CTRL),
	/* sst dpformat */
	DEBUGFS_REG32(V_DATA_PER_LINE0),
	DEBUGFS_REG32(V_DATA_PER_LINE1),
	DEBUGFS_REG32(TU_DATA_SIZE0),
	DEBUGFS_REG32(TU_DATA_SIZE1),
	DEBUGFS_REG32(HDEALY0),
	DEBUGFS_REG32(HDEALY1),
	DEBUGFS_REG32(LFIFO_WL_SET),
	/* lane */
	DEBUGFS_REG32(DP_PHY_CTRL),
	DEBUGFS_REG32(DP_MAC_CTRL),

	DEBUGFS_REG32(DPTX_PHY_CTRL),
	DEBUGFS_REG32(ARBITER_SEC_END_CNT_HB),
	DEBUGFS_REG32(ARBITER_SEC_END_CNT_LB),

	DEBUGFS_REG32(DPTX_SFIFO_LANE_SWAP1),
	DEBUGFS_REG32(DPTX_PN_SWAP),
};

static const struct debugfs_reg32 rtk_prince_edp_wrapper_regs[] = {
	/* timing gen */
	DEBUGFS_REG32(EDPTX_DH_WIDTH),
	DEBUGFS_REG32(EDPTX_DH_TOTAL),
	DEBUGFS_REG32(EDPTX_DH_DEN_START_END),
	DEBUGFS_REG32(EDPTX_DV_DEN_START_END_FIELD1),
	DEBUGFS_REG32(EDPTX_DV_TOTAL),
	DEBUGFS_REG32(EDPTX_DV_VS_START_END_FIELD1),
	DEBUGFS_REG32(EDPTX_DH_VS_ADJ_FIELD1),
	/* csc */
	DEBUGFS_REG32(EDPTX_MAIN),
	DEBUGFS_REG32(EDPTX_CSC1),
	DEBUGFS_REG32(EDPTX_CSC2),
	DEBUGFS_REG32(EDPTX_CSC3),
	DEBUGFS_REG32(EDPTX_CSC4),
	DEBUGFS_REG32(EDPTX_CSC5),
	DEBUGFS_REG32(EDPTX_CSC6),
	DEBUGFS_REG32(EDPTX_CSC7),

	DEBUGFS_REG32(EDPTX_DV_SYNC_INTE),
};

static const struct debugfs_reg32 rtk_prince_dptx14_mac_regs[] = {
	/* sst msa */
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_HTT_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_HTT_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_HST_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_HST_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_HWD_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_HWD_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_HSW_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_HSW_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_DPTX_MEAS_BYPASS),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_VTTE_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_VST_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_VST_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_VHT_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_VHT_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_VSW_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_STRM_ATTR_VSW_L),
	/* sst */
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_M_VID_H),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_M_VID_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_M_VID_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_N_VID_H),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_N_VID_M),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_N_VID_L),
	DEBUGFS_REG32(DPTX14_MAC_IP_MN_VID_AUTO_EN_1),
	DEBUGFS_REG32(DPTX14_MAC_IP_MSA_MISC0),
	DEBUGFS_REG32(DPTX14_MAC_IP_MSA_CTRL),
	/* sst dpformat */
	DEBUGFS_REG32(DPTX14_MAC_IP_V_DATA_PER_LINE0),
	DEBUGFS_REG32(DPTX14_MAC_IP_V_DATA_PER_LINE1),
	DEBUGFS_REG32(DPTX14_MAC_IP_TU_DATA_SIZE0),
	DEBUGFS_REG32(DPTX14_MAC_IP_TU_DATA_SIZE1),
	DEBUGFS_REG32(DPTX14_MAC_IP_HDEALY0),
	DEBUGFS_REG32(DPTX14_MAC_IP_HDEALY1),

	DEBUGFS_REG32(DPTX14_MAC_IP_DPTX_PHY_CTRL),

	DEBUGFS_REG32(DPTX14_MAC_IP_ARBITER_SEC_END_CNT_HB),
	DEBUGFS_REG32(DPTX14_MAC_IP_ARBITER_SEC_END_CNT_LB),
	DEBUGFS_REG32(DPTX14_MAC_IP_ARBITER_DEBUG),

	/* lane */
	DEBUGFS_REG32(DPTX14_MAC_IP_DP_PHY_CTRL),
	DEBUGFS_REG32(DPTX14_MAC_IP_DP_MAC_CTRL),
	DEBUGFS_REG32(DPTX14_MAC_IP_DPTX_CLK_GEN),
	DEBUGFS_REG32(DPTX14_MAC_IP_DPTX_SFIFO_CTRL0),
	DEBUGFS_REG32(DPTX14_MAC_IP_DPTX_SFIFO_CTRL0),
};

static const struct debugfs_reg32 rtk_prince_dptx14_regs[] = {
	/* timing gen */
	DEBUGFS_REG32(DPTX14_DH_WIDTH),
	DEBUGFS_REG32(DPTX14_DH_TOTAL),
	DEBUGFS_REG32(DPTX14_DH_DEN_START_END),
	DEBUGFS_REG32(DPTX14_DV_DEN_START_END_FIELD1),
	DEBUGFS_REG32(DPTX14_DV_TOTAL),
	DEBUGFS_REG32(DPTX14_DV_VS_START_END_FIELD1),
	DEBUGFS_REG32(DPTX14_DH_VS_ADJ_FIELD1),
	/* csc */
	DEBUGFS_REG32(DPTX14_MAIN),
	DEBUGFS_REG32(DPTX14_LANE),
	DEBUGFS_REG32(DPTX14_DV_SYNC_INTE),
};

static int rtk_dptx_show_regs(struct seq_file *s, void *data)
{
	struct drm_info_node *node = s->private;
	struct rtk_prince_dptx *dptx = node->info_ent->data;
	struct drm_crtc *crtc = dptx->encoder.crtc;
	struct drm_device *drm = node->minor->dev;
	unsigned int i;
	int err = 0;

	drm_modeset_lock_all(drm);

	if (!crtc || !crtc->state->active) {
		dev_err(dptx->dev, "No crtc or not activated\n");
		err = -EBUSY;
		goto unlock;
	}

	for (i = 0; i < ARRAY_SIZE(rtk_crt_regs); i++) {
		unsigned int offset = rtk_crt_regs[i].offset;

		dev_info(dptx->dev, "%-32s %#05x %08x\n",
			rtk_crt_regs[i].name, offset,
			rtk_dptx_read(dptx->crt_reg_base, offset));
	}

	for (i = 0; i < ARRAY_SIZE(rtk_prince_edp_regs); i++) {
		unsigned int offset = rtk_prince_edp_regs[i].offset;

		dev_info(dptx->dev, "%-32s %#05x %08x\n",
			rtk_prince_edp_regs[i].name, offset,
			rtk_dptx_read(dptx->dptx14_edp_reg_base, offset));
	}

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP) {
		for (i = 0; i < ARRAY_SIZE(rtk_prince_edp_wrapper_regs); i++) {
			unsigned int offset = rtk_prince_edp_wrapper_regs[i].offset;

			dev_info(dptx->dev, "%-32s %#05x %08x\n",
				rtk_prince_edp_wrapper_regs[i].name, offset,
				rtk_dptx_read(dptx->edp_wrapper_reg_base, offset));
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(rtk_prince_dptx14_mac_regs); i++) {
			unsigned int offset = rtk_prince_dptx14_mac_regs[i].offset;

			dev_info(dptx->dev, "%-32s %#05x %08x\n",
				rtk_prince_dptx14_mac_regs[i].name, offset,
				rtk_dptx_read(dptx->dptx14_mac_reg_base, offset));
		}

		for (i = 0; i < ARRAY_SIZE(rtk_prince_dptx14_regs); i++) {
			unsigned int offset = rtk_prince_dptx14_regs[i].offset;

			dev_info(dptx->dev, "%-32s %#05x %08x\n",
				rtk_prince_dptx14_regs[i].name, offset,
				rtk_dptx_read(dptx->dptx14_reg_base, offset));
		}
	}

unlock:
	drm_modeset_unlock_all(drm);
	return err;
}

static int rtk_dptx_show_train_info(struct seq_file *s, void *data)
{
	struct drm_info_node *node = s->private;
	struct rtk_prince_dptx *dptx = node->info_ent->data;
	struct drm_crtc *crtc = dptx->encoder.crtc;
	struct drm_device *drm = node->minor->dev;
	int err = 0;

	drm_modeset_lock_all(drm);

	if (!crtc || !crtc->state->active) {
		dev_err(dptx->dev, "No crtc or not activated\n");
		err = -EBUSY;
		goto unlock;
	}

	dev_info(dptx->dev, "Show train info\n");

	dev_info(dptx->dev, "RX Max link bw/Lane count is 0x%x/%d !\n",
			dptx->link_train.rx_link_bw, dptx->link_train.rx_lane_count);

	dev_info(dptx->dev, "TX Max link bw/Lane count is 0x%x/%d !\n",
			dptx->video_info.max_link_bw, dptx->video_info.max_lane_count);

	dev_info(dptx->dev, "TX actual link bw/Lane count is 0x%x/%d !\n",
			dptx->link_train.link_bw, dptx->link_train.lane_count);

	dev_err(dptx->dev, "max rate (%d)\n",
		dptx->link_train.lane_count * dptx->link_train.link_rate);

	dev_info(dptx->dev, "dptx->bpc = %d, dptx->color_format = %d\n",
		dptx->bpc, dptx->color_format);

	dev_info(dptx->dev, "property link rate/Lane count is %d/%d !\n",
			dptx->prop_link_rate, dptx->prop_lane_count);

	/**
	 * TODO: keep adding useful info
	 */
unlock:
	drm_modeset_unlock_all(drm);
	return err;
}

static struct drm_info_list debugfs_files[] = {
	{ "show_regs", rtk_dptx_show_regs, 0, NULL },
	{ "show_train_info", rtk_dptx_show_train_info, 0, NULL },
};

static int rtk_dptx_late_register(struct drm_connector *connector)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(connector);
	unsigned int i, count = ARRAY_SIZE(debugfs_files);
	struct drm_minor *minor = connector->dev->primary;
	struct dentry *root = connector->debugfs_entry;

	dev_info(dptx->dev, "prince dptx: late register\n");

	dptx->debugfs_files = kmemdup(debugfs_files, sizeof(debugfs_files),
				     GFP_KERNEL);
	if (!dptx->debugfs_files)
		return -ENOMEM;

	for (i = 0; i < count; i++)
		dptx->debugfs_files[i].data = dptx;

	drm_debugfs_create_files(dptx->debugfs_files, count, root, minor);

	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static void ensure_clock_enabled(struct rtk_prince_dptx *dptx)
{
	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP) {
		if (__clk_is_enabled(dptx->clk_edptx) &&	(!reset_control_status(dptx->rstc_edptx))) {
			dev_dbg(dptx->dev, "prince edp clk already on\n");
		} else {
			dev_info(dptx->dev, "prince edp clk off, reinit\n");

			if (__clk_is_enabled(dptx->clk_edptx)) {
				clk_prepare_enable(dptx->clk_edptx);
				clk_disable_unprepare(dptx->clk_edptx);
			}
			reset_control_assert(dptx->rstc_edptx);
			reset_control_deassert(dptx->rstc_edptx);
			clk_prepare_enable(dptx->clk_edptx);

			// rtk_1920_edp_init(edp);
		}
	} else {
		if (__clk_is_enabled(dptx->clk_dptx) &&
			(!reset_control_status(dptx->rstc_dptx))) {
			dev_dbg(dptx->dev, "prince dptx clk already on\n");
		} else {

			dev_info(dptx->dev, "prince dptx clk off, reinit\n");

			clk_prepare_enable(dptx->clk_dptx);
			reset_control_deassert(dptx->rstc_dptx);
		}
	}
}

static void rtk_prince_dptx_aux_init(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "prince dptx: aux init\n");

	/* DPTX shares the eDPTX AUX */
	/* enable aux channel */
	rtk_dptx_write(dptx->dptx14_edp_reg_base, AUX_TX_CTRL, AUX_EN);
	rtk_dptx_write(dptx->dptx14_edp_reg_base, AUX_IRQ_EN, AUX_ALL_IRQ);

	// Disable NACK retry
	rtk_dptx_update(dptx->dptx14_edp_reg_base, AUX_RETRY_2, RETRY_NACK_EN, 0);

	rtk_dptx_update(dptx->dptx14_edp_reg_base, HPD_CTRL, HPD_CTRL_EN, HPD_CTRL_EN);
	rtk_dptx_write(dptx->dptx14_edp_reg_base, HPD_IRQ_EN, HPD_IRQ_EN_ALL);
}

static void rtk_dptx_deinit_hw(struct rtk_prince_dptx *dptx)
{
	struct drm_encoder *encoder = &dptx->encoder;
	struct drm_display_mode *mode = &dptx->encoder.crtc->state->adjusted_mode;
	struct rtk_rpc_info *rpc_info = dptx->rpc_info;
	struct rpc_set_display_out_interface interface;
	int ret = 0;

	ret = drm_of_encoder_active_endpoint_id(dptx->dev->of_node, encoder);
	if (ret < 0 || ret > DISPLAY_INTERFACE_MIXER3)
		dev_err(dptx->dev, "Invalid endpoint id %d\n", ret);

	memset(&interface, 0, sizeof(interface));

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP)
		interface.display_interface = DISPLAY_INTERFACE_eDP;
	else
		interface.display_interface = DISPLAY_INTERFACE_DP;

	interface.width                   = mode->hdisplay;
	interface.height                  = mode->vdisplay;
	interface.frame_rate              = drm_mode_vrefresh(mode);
	interface.display_interface_mixer = DISPLAY_INTERFACE_MIXER_NONE;

	DRM_INFO("[%s] disable %s on %s\n", __func__,
		interface_names[interface.display_interface], mixer_names[ret]);

	ret = rpc_set_out_interface(rpc_info, &interface);
	if (ret)
		DRM_ERROR("rpc_set_out_interface rpc fail\n");

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP)
		rtk_prince_edp_phy_disable_timing_gen(dptx);
	else
		rtk_prince_dptx_phy_disable_timing_gen(dptx);
}

static void rtk_dptx_poweroff(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "prince dptx: poweroff\n");

	rtk_dptx_deinit_hw(dptx);

	drm_dp_dpcd_writeb(&dptx->aux, DP_SET_POWER, DP_SET_POWER_D3);
	usleep_range(2000, 3000);

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP) {
		if (__clk_is_enabled(dptx->clk_edptx))
			clk_disable_unprepare(dptx->clk_edptx);
		reset_control_assert(dptx->rstc_edptx);
	} else {
		if (__clk_is_enabled(dptx->clk_dptx))
			clk_disable_unprepare(dptx->clk_dptx);
		reset_control_assert(dptx->rstc_dptx);
	}
}

static bool rtk_dptx_enc_mode_fixup(struct drm_encoder *encoder,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	DRM_INFO("prince dptx: enc mode fixup\n");

	return true;
}

static void rtk_dptx_update_plugged_status(struct rtk_prince_dptx *dptx)
{
	// mutex_lock(&dptx->update_plugged_status_lock);
	if (dptx->plugged_cb && dptx->codec_dev) {
		dev_info(dptx->dev, "[update plugged status] dptx %s, %s audio\n",
			dptx->connected ? "connected" : "disconnected",
			dptx->sink_has_audio ? "Has" : "No");

		dptx->plugged_cb(dptx->codec_dev,
				   dptx->connected & dptx->sink_has_audio);
	}
	// mutex_unlock(&dptx->update_plugged_status_lock);
}

static void rtk_prince_dptx_get_max_rx_bandwidth(struct rtk_prince_dptx *dptx,
					     u8 *bandwidth)
{
	u8 data;

	/*
	 * For DP rev.1.1, Maximum link rate of Main Link lanes
	 * 0x06 = 1.62 Gbps, 0x0a = 2.7 Gbps
	 * For DP rev.1.2, Maximum link rate of Main Link lanes
	 * 0x06 = 1.62 Gbps, 0x0a = 2.7 Gbps, 0x14 = 5.4Gbps
	 */
	drm_dp_dpcd_readb(&dptx->aux, DP_MAX_LINK_RATE, &data);
	*bandwidth = data;
}

static void rtk_prince_dptx_get_max_rx_lane_count(struct rtk_prince_dptx *dptx,
					      u8 *lane_count)
{
	u8 data;

	/*
	 * For DP rev.1.1, Maximum number of Main Link lanes
	 * 0x01 = 1 lane, 0x02 = 2 lanes, 0x04 = 4 lanes
	 */
	drm_dp_dpcd_readb(&dptx->aux, DP_MAX_LANE_COUNT, &data);
	*lane_count = DPCD_MAX_LANE_COUNT(data);
}

static void rtk_dptx_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(encoder);
	struct drm_display_info *display_info = &dptx->connector.display_info;
	int vic __attribute__((unused));

	vic = drm_match_cea_mode(mode);

	dev_info(dptx->dev, "RX Max link bw/Lane count is 0x%x/%d !\n",
			dptx->link_train.rx_link_bw, dptx->link_train.rx_lane_count);

	dev_info(dptx->dev, "TX Max link bw/Lane count is 0x%x/%d !\n",
			dptx->video_info.max_link_bw, dptx->video_info.max_lane_count);

	dptx->color_format = RTK_COLOR_FORMAT_RGB;

	dev_info(dptx->dev, "dptx->bpc = %d, dptx->color_format = %d, display_info->color_formats = %d\n",
		dptx->bpc, dptx->color_format, display_info->color_formats);
}

static u8 rtk_dp_get_lane_status(u8 link_status[2], int lane)
{
	int shift = (lane & 1) * 4;
	u8 link_value = link_status[lane >> 1];

	return (link_value >> shift) & 0xf;
}

static int rtk_drm_dp_clock_recovery_ok(u8 link_status[2], int lane_count)
{
	int lane;
	u8 lane_status;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = rtk_dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_LANE_CR_DONE) == 0)
			return -EINVAL;
	}
	return 0;
}

static int rtk_drm_dp_channel_eq_ok(u8 link_status[2], u8 link_align,
				     int lane_count)
{
	int lane;
	u8 lane_status;

	if ((link_align & DP_INTERLANE_ALIGN_DONE) == 0)
		return -EINVAL;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = rtk_dp_get_lane_status(link_status, lane);
		lane_status &= DP_CHANNEL_EQ_BITS;
		if (lane_status != DP_CHANNEL_EQ_BITS)
			return -EINVAL;
	}

	return 0;
}

static int rtk_dptx_get_gpio_hpd_status(struct rtk_prince_dptx *dptx)
{
	if (!dptx->hpd_gpio)
		return -EINVAL;

	schedule_delayed_work(&dptx->hpd_gpio_work,
		 msecs_to_jiffies(RTK_HPD_SHORT_PULSE_THRESHOLD_MS));

	return 0;
}

static int rtk_dptx_detect_gpio_hpd(struct rtk_prince_dptx *dptx)
{
	if (rtk_dptx_get_gpio_hpd_status(dptx) >= 0) {
		dev_dbg(dptx->dev, "success to get hpd status\n");
		return 0;
	}

	/**
	 * Some dptx screen do not have hpd, add DT property force-hpd
	 */

	dev_dbg(dptx->dev, "fail to get hpd status\n");

	return -ETIMEDOUT;
}

static unsigned char
rtk_prince_dptx_get_adjust_request_voltage(u8 adjust_request[2], int lane)
{
	int shift = (lane & 1) * 4;
	u8 link_value = adjust_request[lane >> 1];

	return (link_value >> shift) & 0x3;
}

static unsigned char rtk_prince_dptx_get_adjust_request_pre_emphasis(
					u8 adjust_request[2], int lane)
{
	int shift = (lane & 1) * 4;
	u8 link_value = adjust_request[lane >> 1];

	return ((link_value >> shift) & 0xc) >> 2;
}

static int rtk_prince_dptx_training_pattern_disable(struct rtk_prince_dptx *dptx)
{
	int retval;

	retval = drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
				 DP_TRAINING_PATTERN_DISABLE);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_TRAINING_PATTERN_SET disable fail %d\n", retval);
		return retval;
	}

	return retval < 0 ? retval : 0;
}

static int rtk_prince_dptx_set_enhanced_mode(struct rtk_prince_dptx *dptx)
{
	u8 enhanced_mode_support;
	u8 data;
	int retval;

	retval = drm_dp_dpcd_readb(&dptx->aux, DP_MAX_LANE_COUNT, &data);
	if (retval != 1) {
		dev_info(dptx->dev, "DP_MAX_LANE_COUNT %d\n", retval);
		enhanced_mode_support = 0;
		return retval;
	}

	enhanced_mode_support = DPCD_ENHANCED_FRAME_CAP(data);

	retval = drm_dp_dpcd_readb(&dptx->aux, DP_LANE_COUNT_SET, &data);
	if (retval != 1) {
		dev_info(dptx->dev, "DP_LANE_COUNT_SET %d\n", retval);
		return retval;
	}

	if (enhanced_mode_support)
		retval = drm_dp_dpcd_writeb(&dptx->aux, DP_LANE_COUNT_SET,
					 DP_LANE_COUNT_ENHANCED_FRAME_EN |
					 DPCD_LANE_COUNT_SET(data));
	else
		retval = drm_dp_dpcd_writeb(&dptx->aux, DP_LANE_COUNT_SET,
					 DPCD_LANE_COUNT_SET(data));

	return 0;
}

static void rtk_prince_dptx_reduce_link_rate(struct rtk_prince_dptx *dptx)
{
	rtk_prince_dptx_training_pattern_disable(dptx);
	rtk_prince_dptx_set_enhanced_mode(dptx);

	dptx->link_train.lt_state = FAILED;
}

static void rtk_prince_dptx_get_adjust_training_lane(struct rtk_prince_dptx *dptx,
						 u8 adjust_request[2])
{
	int lane, lane_count;
	u8 voltage_swing, pre_emphasis, training_lane;

	lane_count = dptx->link_train.lane_count;
	for (lane = 0; lane < lane_count; lane++) {
		voltage_swing = rtk_prince_dptx_get_adjust_request_voltage(
						adjust_request, lane);
		pre_emphasis = rtk_prince_dptx_get_adjust_request_pre_emphasis(
						adjust_request, lane);
		training_lane = DPCD_VOLTAGE_SWING_SET(voltage_swing) |
				DPCD_PRE_EMPHASIS_SET(pre_emphasis);

		if (voltage_swing == MAX_VOLTAGE_SWING_LEVEL)
			training_lane |= DP_TRAIN_MAX_SWING_REACHED;
		if (pre_emphasis == MAX_EMPHASIS_LEVEL)
			training_lane |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

		dptx->link_train.training_lane[lane] = training_lane;
		dev_dbg(dptx->dev, "training_lane[%d] = 0x%x\n", lane,
			dptx->link_train.training_lane[lane]);
	}
}

static int rtk_prince_dptx_link_start(struct rtk_prince_dptx *dptx)
{
	u8 buf[4];
	int lane;
	int lane_count;
	int retval;

	dev_info(dptx->dev, "link train start\n");

	lane_count = dptx->link_train.lane_count;

	dptx->link_train.lt_state = CLOCK_RECOVERY;
	// dptx->link_train.eq_loop = 0;

	for (lane = 0; lane < lane_count; lane++)
		dptx->link_train.cr_loop[lane] = 0;

	// rtk_dptx_mac_signal_setting(dptx, signals);
	// rtk_dptx_aphy_signal_setting(dptx, signals);

	/* Spec says link_bw = link_rate / 0.27Gbps */
	retval = drm_dp_dpcd_writeb(&dptx->aux, DP_SET_POWER, DP_SET_POWER_D0);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_SET_POWER fail %d\n", retval);
		return retval;
	}

	dev_info(dptx->dev, "link train bw/Lane count is 0x%x/%d !\n",
			dptx->link_train.link_bw, dptx->link_train.lane_count);

	/* Setup RX configuration */
	buf[0] = dptx->link_train.link_bw;
	buf[1] = dptx->link_train.lane_count;
	// drm_dp_dpcd_writeb(&dptx->aux, DP_LINK_BW_SET, dptx->link_train.link_bw / 27000);
	retval = drm_dp_dpcd_write(&dptx->aux, DP_LINK_BW_SET, buf, 2);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_LINK_BW_SET fail %d\n", retval);
		return retval;
	}

	retval = drm_dp_dpcd_writeb(&dptx->aux, DP_LANE_COUNT_SET,
					 dptx->link_train.lane_count | DP_LANE_COUNT_ENHANCED_FRAME_EN);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_LANE_COUNT_SET fail %d\n", retval);
		return retval;
	}

	retval = drm_dp_dpcd_writeb(&dptx->aux, DP_MAIN_LINK_CHANNEL_CODING_SET,
					DP_SET_ANSI_8B10B);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_MAIN_LINK_CHANNEL_CODING_SET fail %d\n", retval);
		return retval;
	}

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP)
		rtk_prince_edp_phy_set_pattern(dptx, RTK_PATTERN_1);
	else
		rtk_prince_dptx_phy_set_pattern(dptx, RTK_PATTERN_1);

	retval = drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
					DP_TRAINING_PATTERN_1 | DP_LINK_SCRAMBLING_DISABLE);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_TRAINING_PATTERN_SET 1 fail %d\n", retval);
		return retval;
	}

	for (lane = 0; lane < lane_count; lane++)
		buf[lane] = DP_TRAIN_PRE_EMPH_LEVEL_0 |
			    DP_TRAIN_VOLTAGE_SWING_LEVEL_0;

	retval = drm_dp_dpcd_write(&dptx->aux, DP_TRAINING_LANE0_SET, buf,
					lane_count);
	if (retval < 0)
		return retval;

	// rtk_edp_write_dpcd_lane_set(dptx, signals);

	return 0;
}

static int rtk_prince_dptx_train_clock_recovery(struct rtk_prince_dptx *dptx)
{
	int retval;
	int lane_count;
	// int lane;
	// uint8_t status[DP_LINK_STATUS_SIZE];
	// u8 training_lane;
	// u8 rx_swing;
	// u8 rx_emphasis;
	// u8 prev_swing = 0;
	// u8 prev_emphasis = 0;
	u8 link_status[2];
	u8 adjust_request[2];

	dev_dbg(dptx->dev, "train clock recovery\n");

	usleep_range(100, 200); // LANEx_CR_DONE (Minimum)

	lane_count = dptx->link_train.lane_count;

	retval = drm_dp_dpcd_read(&dptx->aux, DP_LANE0_1_STATUS, link_status, 2);
	if (retval < 0)
		return retval;

	retval = drm_dp_dpcd_read(&dptx->aux, DP_ADJUST_REQUEST_LANE0_1,
				  adjust_request, 2);
	if (retval < 0)
		return retval;

	/*
	 * Condition of CR fail:
	 * 1. Failed to pass CR using the same voltage
	 *    level over five times.
	 * 2. Failed to pass CR when the current voltage
	 *    level is the same with previous voltage
	 *    level and reach max voltage level (3).
	 */

	if (rtk_drm_dp_clock_recovery_ok(link_status, lane_count) == 0) {
		/* set training pattern 2 for EQ */

		if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP)
			rtk_prince_edp_phy_set_pattern(dptx, RTK_PATTERN_2);
		else
			rtk_prince_dptx_phy_set_pattern(dptx, RTK_PATTERN_2);

		retval = drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
						DP_TRAINING_PATTERN_2 | DP_LINK_SCRAMBLING_DISABLE);
		if (retval < 0) {
			dev_err(dptx->dev, "DP_TRAINING_PATTERN_SET 2 fail %d\n", retval);
			return retval;
		}

		dev_info(dptx->dev, "Link Training Clock Recovery success\n");
		dptx->link_train.lt_state = EQUALIZER_TRAINING;
	} else {
		rtk_prince_dptx_get_adjust_training_lane(dptx, adjust_request);

		retval = drm_dp_dpcd_write(&dptx->aux, DP_TRAINING_LANE0_SET,
						dptx->link_train.training_lane, lane_count);
		if (retval < 0) {
			dev_err(dptx->dev, "DP_TRAINING_LANE0_SET in cr fail %d\n", retval);
			return retval;
		}
	}

	return 0;
}

static int rtk_prince_dptx_train_equalizer(struct rtk_prince_dptx *dptx)
{
	int lane_count;
	int retval;
	u8 link_align, link_status[2], adjust_request[2];

	dev_info(dptx->dev, "train equalizer\n");

	lane_count = dptx->link_train.lane_count;

	retval = drm_dp_dpcd_read(&dptx->aux, DP_LANE0_1_STATUS, link_status, 2);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_LANE0_1_STATUS fail %d\n", retval);
		return retval;
	}

	if (rtk_drm_dp_clock_recovery_ok(link_status, lane_count)) {
		rtk_prince_dptx_reduce_link_rate(dptx);
		return -EIO;
	}

	retval = drm_dp_dpcd_read(&dptx->aux, DP_ADJUST_REQUEST_LANE0_1,
				  adjust_request, 2);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_ADJUST_REQUEST_LANE0_1 fail %d\n", retval);
		return retval;
	}

	dev_info(dptx->dev, "adjust_request[0] = 0x%x adjust_request[1] = 0x%x\n",
		adjust_request[0], adjust_request[1]);

	retval = drm_dp_dpcd_readb(&dptx->aux, DP_LANE_ALIGN_STATUS_UPDATED,
				   &link_align);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_LANE_ALIGN_STATUS_UPDATED fail %d\n", retval);
		return retval;
	}

	dev_info(dptx->dev, "link_align = 0x%x\n", link_align);

	rtk_prince_dptx_get_adjust_training_lane(dptx, adjust_request);

	/*
	 * Condition of EQ fail:
	 * 1. Failed to pass EQ over six times.
	 */

	if (!rtk_drm_dp_channel_eq_ok(link_status, link_align, lane_count)) {
		/* traing pattern Set to Normal */
		retval = rtk_prince_dptx_training_pattern_disable(dptx);
		if (retval < 0)
			return retval;

		dev_info(dptx->dev, "Link Training Equalizer success\n");

		dptx->link_train.lt_state = FINISHED;

		return 0;
	}

	/* not all locked */
	dptx->link_train.eq_loop++;

	if (dptx->link_train.eq_loop > MAX_EQUALIZER_LOOP) {
		dev_err(dptx->dev, "EQ Max loop\n");
		rtk_prince_dptx_reduce_link_rate(dptx);
		return -EIO;
	}

	retval = drm_dp_dpcd_write(&dptx->aux, DP_TRAINING_LANE0_SET,
				   dptx->link_train.training_lane, lane_count);
	if (retval < 0) {
		dev_err(dptx->dev, "DP_TRAINING_LANE0_SET in eq fail %d\n", retval);
		return retval;
	}

	return 0;
}

static int rtk_prince_dptx_fast_link_train(struct rtk_prince_dptx *dptx)
{
	return 0;
}

static int rtk_prince_dptx_full_link_train(struct rtk_prince_dptx *dptx)
{
	int retval = 0;
	bool training_finished = false;

	dptx->link_train.lt_state = START;

	/* Process here */
	while (!retval && !training_finished) {
		switch (dptx->link_train.lt_state) {
		case START:
			retval = rtk_prince_dptx_link_start(dptx);
			if (retval)
				dev_err(dptx->dev, "LT link start failed!\n");
			break;
		case CLOCK_RECOVERY:
			retval = rtk_prince_dptx_train_clock_recovery(dptx);
			if (retval)
				dev_err(dptx->dev, "LT CR failed!\n");
			break;
		case EQUALIZER_TRAINING:
			retval = rtk_prince_dptx_train_equalizer(dptx);
			if (retval)
				dev_err(dptx->dev, "LT EQ failed!\n");
			break;
		case FINISHED:
			dev_info(dptx->dev, "LT finished\n");
			training_finished = 1;
			break;
		case FAILED:
			dev_err(dptx->dev, "LT failed!\n");
			return -EREMOTEIO;
		}
	}
	if (retval)
		dev_err(dptx->dev, "eDP link training failed (%d)\n", retval);

	return retval;
}

static int rtk_prince_dptx_link_training(struct rtk_prince_dptx *dptx)
{
	if (dptx->fast_train_enable)
		return rtk_prince_dptx_fast_link_train(dptx);

	return rtk_prince_dptx_full_link_train(dptx);
}

static void rtk_prince_dptx_config_vo(struct rtk_prince_dptx *dptx, struct drm_display_mode *mode)
{
	struct drm_encoder *encoder = &dptx->encoder;
	struct rtk_rpc_info *rpc_info = dptx->rpc_info;
	struct rpc_set_display_out_interface interface;
	int ret;

	ret = drm_of_encoder_active_endpoint_id(dptx->dev->of_node, encoder);
	if (ret < 0 || ret > DISPLAY_INTERFACE_MIXER3) {
		dev_err(dptx->dev, "Invalid endpoint id %d\n", ret);
		return;
	}

	memset(&interface, 0, sizeof(interface));

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP)
		interface.display_interface = DISPLAY_INTERFACE_eDP;
	else
		interface.display_interface = DISPLAY_INTERFACE_DP;

	interface.width                   = mode->hdisplay;
	interface.height                  = mode->vdisplay;
	interface.frame_rate              = drm_mode_vrefresh(mode);
	interface.display_interface_mixer = ret;

	DRM_INFO("[rtk_dptx_enc_enable] enable %s on %s (%dx%d@%d)\n",
		interface_names[interface.display_interface], mixer_names[interface.display_interface_mixer],
		interface.width, interface.height, interface.frame_rate);

	ret = rpc_set_out_interface(rpc_info, &interface);
	if (ret)
		DRM_ERROR("rpc_set_out_interface rpc fail\n");
}

static int rtk_prince_dptx_init_hw(struct rtk_prince_dptx *dptx, struct drm_display_mode *mode)
{
	int ret = 0;
	int timeout_loop = 0;

	dev_info(dptx->dev, "[%s] lane(%u) rate(%d) pclk(%d)\n", __func__,
				 dptx->link_train.lane_count, dptx->link_train.link_rate, mode->clock);

	ret = rtk_prince_dptx_combo_phy_setting(dptx, mode);
	if (ret)
		return ret;

	while (timeout_loop < MAX_LINK_TRAIN_LOOP) {
		if (rtk_prince_dptx_link_training(dptx) == 0) {
			dev_info(dptx->dev, "Link Training success!\n");
			break;
		}

		dev_err(dptx->dev, "link train fail, retry: %d\n",
			timeout_loop);
		timeout_loop++;
		usleep_range(10, 11);
	}

	drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);

	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_LANE_SWAP, 0x000000e4);
	rtk_dptx_write(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_SFIFO_CTRL2, 0x00000030);

	rtk_prince_dptx_phy_config_video_timing(dptx, mode);
	rtk_prince_dptx_phy_config_lane(dptx);

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_DPTX_PHY_CTRL,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_sr_insert_en_mask,
		DPTX14_MAC_IP_DPTX_PHY_CTRL_sr_insert_en(1));

	rtk_prince_dptx_csc_setting(dptx);
	rtk_prince_dptx_phy_start_video(dptx, mode);

	rtk_prince_dptx_config_vo(dptx, mode);

	dev_info(dptx->dev, "init hw donw\n");

	return 0;
}

static int rtk_prince_edptx_init_hw(struct rtk_prince_dptx *dptx, struct drm_display_mode *mode)
{
	int ret = 0;
	int timeout_loop = 0;

	dev_info(dptx->dev, "[%s] lane(%u) rate(%d) pclk(%d)\n", __func__,
				 dptx->link_train.lane_count, dptx->link_train.link_rate, mode->clock);

	ret = rtk_prince_dptx_combo_phy_setting(dptx, mode);
	if (ret)
		return ret;

	while (timeout_loop < MAX_LINK_TRAIN_LOOP) {
		if (rtk_prince_dptx_link_training(dptx) == 0) {
			dev_info(dptx->dev, "Link Training success!\n");
			break;
		}

		dev_err(dptx->dev, "link train fail, retry: %d\n",
			timeout_loop);
		timeout_loop++;
		usleep_range(10, 11);
	}

	drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);

	rtk_dptx_write(dptx->dptx14_edp_reg_base, DPTX_SFIFO_LANE_SWAP1, 0x000000e4);
	rtk_dptx_write(dptx->dptx14_edp_reg_base, DPTX_PN_SWAP, 0x00000003);

	rtk_prince_edp_phy_config_video_timing(dptx, mode);

	rtk_prince_edp_phy_set_scramble(dptx, true);
	// rtk_prince_edp_phy_config_csc(dptx);

	usleep_range(10, 11);

	rtk_prince_edp_phy_start_video(dptx, mode);

	rtk_prince_dptx_config_vo(dptx, mode);

	dev_info(dptx->dev, "init hw donw\n");

	return ret;
}

static int rtk_dptx_fast_link_train_detection(struct rtk_prince_dptx *dptx)
{
	int ret;
	u8 spread;

	ret = drm_dp_dpcd_readb(&dptx->aux, DP_MAX_DOWNSPREAD, &spread);
	if (ret != 1) {
		dev_err(dptx->dev, "failed to read downspread %d\n", ret);
		return ret;
	}
	dptx->fast_train_enable = !!(spread & DP_NO_AUX_HANDSHAKE_LINK_TRAINING);
	dev_info(dptx->dev, "fast link training %s\n",
		dptx->fast_train_enable ? "supported" : "unsupported");
	return 0;
}

static void rtk_prince_dptx_set_link_train_info(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "property link rate/Lane count is %d/%d !\n",
			dptx->prop_link_rate, dptx->prop_lane_count);

	if (dptx->prop_lane_count == LANE_COUNT_UNSPECIFIED ||
		dptx->prop_lane_count > dptx->link_train.rx_lane_count)
		dptx->link_train.lane_count = dptx->link_train.rx_lane_count;
	else
		dptx->link_train.lane_count = dptx->prop_lane_count;

	if (dptx->prop_link_rate == LINK_RATE_UNSPECIFIED ||
		dptx->prop_link_rate > drm_dp_bw_code_to_link_rate(dptx->link_train.rx_link_bw)) {
		dptx->link_train.link_bw = dptx->link_train.rx_link_bw;
		dptx->link_train.link_rate = drm_dp_bw_code_to_link_rate(dptx->link_train.link_bw);
	} else
		dptx->link_train.link_rate = dptx->prop_link_rate;

	dev_info(dptx->dev, "TX actual link bw/Lane count is 0x%x/%d !\n",
			dptx->link_train.link_bw, dptx->link_train.lane_count);
}

static void rtk_dptx_enc_enable(struct drm_encoder *encoder)
{
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(encoder);
	int ret;

	pm_runtime_get_sync(dptx->dev);

	if (!dptx->connected) {
		DRM_ERROR("prince dptx: not connected\n");
		goto out;
	}

	dev_info(dptx->dev, "prince dptx: enc enable");

	ensure_clock_enabled(dptx);

	drm_dp_dpcd_writeb(&dptx->aux, DP_SET_POWER, DP_SET_POWER_D0);
	usleep_range(2000, 5000);

	rtk_prince_dptx_set_link_train_info(dptx);

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP) {
		ret = rtk_prince_edptx_init_hw(dptx, mode);
		if (ret)
			DRM_ERROR("prince edptx hw setting fail\n");
	} else {
		ret = rtk_prince_dptx_init_hw(dptx, mode);
		if (ret)
			DRM_ERROR("prince dptx hw setting fail\n");
	}

	/* Check whether panel supports fast training */
	ret = rtk_dptx_fast_link_train_detection(dptx);
	if (ret)
		DRM_ERROR("prince dptx fast link train detection fail\n");

	dptx->is_autotest = false;
	dptx->prop_lane_count = LANE_COUNT_UNSPECIFIED;
	dptx->prop_link_rate  = LINK_RATE_UNSPECIFIED;

	rtk_dptx_update_plugged_status(dptx);

	return;
out:
	pm_runtime_put_sync(dptx->dev);
}

static void rtk_dptx_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(encoder);

	dev_info(dptx->dev, "prince dptx: enc disable\n");

	rtk_dptx_update_plugged_status(dptx);

	rtk_dptx_poweroff(dptx);

	pm_runtime_put_sync(dptx->dev);
}

static void rtk_car_dptx_enc_enable(struct drm_encoder *encoder)
{
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(encoder);
	struct rtk_rpc_info *rpc_info = dptx->rpc_info;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	int ret = 0;

	dev_info(dptx->dev, "prince dptx: car enc enable");

	memset(&hw_init_rpc, 0, sizeof(hw_init_rpc));

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP)
		hw_init_rpc.display_interface = DISPLAY_INTERFACE_eDP;
	else
		hw_init_rpc.display_interface = DISPLAY_INTERFACE_DP;

	hw_init_rpc.enable = 1;

	hw_init_rpc.frame_rate   = drm_mode_vrefresh(mode);
	hw_init_rpc.pixel_clock  = mode->clock;
	hw_init_rpc.hactive      = mode->hdisplay;
	hw_init_rpc.hfront_porch = mode->hsync_start - mode->hdisplay;
	hw_init_rpc.hback_porch  = mode->htotal - mode->hsync_end;
	hw_init_rpc.hsync_len    = mode->hsync_end - mode->hsync_start;
	hw_init_rpc.vactive      = mode->vdisplay;
	hw_init_rpc.vfront_porch = mode->vsync_start - mode->vdisplay;
	hw_init_rpc.vback_porch  = mode->vtotal - mode->vsync_end;
	hw_init_rpc.vsync_len    = mode->vsync_end - mode->vsync_start;
	hw_init_rpc.is_positive_vsync = (bool) (mode->flags & DRM_MODE_FLAG_PVSYNC);
	hw_init_rpc.is_positive_hsync = (bool) (mode->flags & DRM_MODE_FLAG_PHSYNC);
	hw_init_rpc.link_rate    = dptx->link_train.link_rate;
	hw_init_rpc.lane_count   = dptx->link_train.lane_count;
	hw_init_rpc.bpc          = dptx->bpc;

	DRM_INFO("[%s] enable interface %s\n", __func__,
		interface_names[hw_init_rpc.display_interface]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret)
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");
}

static void rtk_car_dptx_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(encoder);
	struct rtk_rpc_info *rpc_info = dptx->rpc_info;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	int ret = 0;

	dev_info(dptx->dev, "prince dptx: car enc disable\n");

	memset(&hw_init_rpc, 0, sizeof(hw_init_rpc));

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP)
		hw_init_rpc.display_interface = DISPLAY_INTERFACE_eDP;
	else
		hw_init_rpc.display_interface = DISPLAY_INTERFACE_DP;

	hw_init_rpc.enable = 0;

	DRM_INFO("[%s] disable interface %s\n", __func__,
		interface_names[hw_init_rpc.display_interface]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret)
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");
}

static int rtk_dptx_enc_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	return 0;
}

static const struct drm_encoder_funcs rtk_dptx_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs rtk_dptx_encoder_helper_funcs = {
	.mode_fixup = rtk_dptx_enc_mode_fixup,
	.mode_set   = rtk_dptx_enc_mode_set,
	.enable     = rtk_dptx_enc_enable,
	.disable    = rtk_dptx_enc_disable,
	.atomic_check = rtk_dptx_enc_atomic_check,
};

static const struct drm_encoder_helper_funcs rtk_car_dptx_encoder_helper_funcs = {
	.mode_fixup = rtk_dptx_enc_mode_fixup,
	.mode_set   = rtk_dptx_enc_mode_set,
	.enable     = rtk_car_dptx_enc_enable,
	.disable    = rtk_car_dptx_enc_disable,
	.atomic_check = rtk_dptx_enc_atomic_check,
};

static void rtk_dptx_set_bad_connector_link_status(struct rtk_prince_dptx *dptx)
{
	struct drm_device *dev;
	struct drm_connector *connector;

	connector = &dptx->connector;
	dev = connector->dev;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	connector->state->link_status = DRM_MODE_LINK_STATUS_BAD;
	drm_modeset_unlock(&dev->mode_config.connection_mutex);
}

/* HPD */
static int rtk_dptx_handle_short_pulse(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "short dptx %s\n", dptx->connected ? "connected" : "disconnected");

	drm_kms_helper_hotplug_event(dptx->drm_dev);

	rtk_dptx_update_plugged_status(dptx);

	return 0;
}

static int rtk_dptx_handle_long_pulse(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "long dptx %s\n", dptx->connected ? "connected" : "disconnected");

	// if (dptx->connected)
	// 	rtk_dptx_enable(dptx);
	// else
	// 	rtk_dptx_reset(dptx);

	rtk_dptx_set_bad_connector_link_status(dptx);
	drm_kms_helper_hotplug_event(dptx->drm_dev);

	rtk_dptx_update_plugged_status(dptx);

	return 0;
}

/* Detect HPD with gpio */
static void rtk_dptx_hpd_gpio_worker(struct work_struct *work)
{
	struct delayed_work *hpd_gpio_work = to_delayed_work(work);
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(hpd_gpio_work);
	bool prev_connected = dptx->connected;
	int val;

	val = gpiod_get_value(dptx->hpd_gpio);
	if (val < 0)
		dev_err(dptx->dev, "failed to get hpd_gpio val");

	mutex_lock(&dptx->lock);
	dptx->connected = (val) ? true : false;
	dev_info(dptx->dev, "[%s] %s\n", __func__, (val) ? "connected" : "disconnected");
	mutex_unlock(&dptx->lock);

	dev_info(dptx->dev, "prev connected %d, now connected %d",
		prev_connected, dptx->connected);

	// short pulse, gpio will be high after 0.5 ~ 1ms
	if (prev_connected == true && dptx->connected == true) {
		rtk_dptx_handle_short_pulse(dptx);
		return;
	}

	rtk_dptx_handle_long_pulse(dptx);
}

static irqreturn_t rtk_dptx_hpd_irq(int irq, void *dev_id)
{
	struct rtk_prince_dptx *dptx = dev_id;

	int __attribute__((unused)) val = 0;

	dev_info(dptx->dev, "prince dptx: hpd irq\n");

	val = rtk_dptx_detect_gpio_hpd(dptx);

	return IRQ_HANDLED;
}

#if 0 /* TODO: Detect HPD with dptx_hpd */
static int poll_hpd(struct rtk_prince_dptx *dptx)
{
	unsigned int val = 0;
	bool old_conn_state = dptx->connected;
	bool is_short_pulse;
	bool is_long_pulse;

	mutex_lock(&dptx->lock);

	val = rtk_dptx_read(dptx->dptx14_reg_base, DPTX14_IP_HPD_CTRL);
	dptx->connected = (bool) (val & HPD_CTRL_DEB);
	val = rtk_dptx_read(dptx->dptx14_reg_base, DPTX14_IP_HPD_IRQ);
	is_short_pulse = (bool) (val & HPD_IRQ_SHPD);
	is_long_pulse = (bool) (val & HPD_IRQ_UHPD || val & HPD_IRQ_LHPD);
	rtk_dptx_write(dptx->dptx14_reg_base, DPTX14_IP_HPD_IRQ, HPD_IRQ_ALL);

	mutex_unlock(&dptx->lock);


	if (is_long_pulse) {
		dev_info(dptx->dev, "prince dptx unplug or long pulse\n");
		rtk_dptx_set_bad_connector_link_status(dptx);
	} else if (is_short_pulse) {
		dev_info(dptx->dev, "prince dptx short pulse\n");
		rtk_dptx_handle_short_pulse(dptx);
		return val;
	}

	if (old_conn_state == dptx->connected)
		return val;

	rtk_dptx_handle_long_pulse(dptx);
	return val;
}

static int rtk_dptx_hpd_thread(void *data)
{
	struct rtk_prince_dptx *dptx = (struct rtk_prince_dptx *) data;

	// Enable HPD interrupt
	rtk_dptx14_update(dptx, DPTX14_IP_HPD_CTRL, HPD_CTRL_EN, HPD_CTRL_EN);
	rtk_dptx_write(dptx->dptx14_reg_base, DPTX14_IP_HPD_IRQ_EN, HPD_IRQ_EN_ALL);

	while (!kthread_should_stop()) {
		poll_hpd(dptx);
		msleep_interruptible(RTK_POLL_HPD_INTERVAL_MS);
	}

	return 0;
}

static int rtk_dptx_start_hpd_thread(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "prince dptx: start hpd thread\n");

	ensure_clock_enabled(dptx);

	if (dptx->hpd_thread) {
		dev_info(dptx->dev, "hpd_thread already exsist\n");
		return 0;
	}

	rtk_dptx_write(dptx->dptx14_reg_base, DPTX14_IP_HPD_IRQ, HPD_IRQ_ALL);
	dptx->hpd_thread = kthread_run(rtk_dptx_hpd_thread, dptx, "prince dptx_hpd_thread");
	if (IS_ERR(dptx->hpd_thread)) {
		dev_err(dptx->dev, "Failed to create kernel thread\n");
		dptx->hpd_thread = NULL;
		return PTR_ERR(dptx->hpd_thread);
	}

	return 0;
}

static int rtk_dptx_stop_hpd_thread(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "prince dptx: stop hpd thread\n");

	if (dptx->hpd_thread) {
		kthread_stop(dptx->hpd_thread);
		dptx->hpd_thread = NULL;
	}

	dptx->connected = false;

	return 0;
}
#endif

/* HPD */
static void rtk_dptx_start_detect_hpd(struct rtk_prince_dptx *dptx)
{
	int ret;

	dev_info(dptx->dev, "prince dptx: start detect hpd\n");
	if (dptx->hpd_gpio) {
		if (dptx->hpd_irq)
			return;

		dptx->hpd_irq = gpiod_to_irq(dptx->hpd_gpio);
		if (dptx->hpd_irq < 0) {
			dev_err(dptx->dev, "Fail to get hpd irq");
			return;
		}

		irq_set_irq_type(dptx->hpd_irq, IRQ_TYPE_EDGE_BOTH);
		ret = request_threaded_irq(dptx->hpd_irq, NULL,
					rtk_dptx_hpd_irq, IRQF_ONESHOT,
					"prince dptx_hpd_irq", dptx);
		if (ret) {
			dev_err(dptx->dev, "can't request hpd gpio irq\n");
			return;
		}
	} else {
		/* TODO: Detect HPD with dptx_hpd */
		// rtk_dptx_start_hpd_thread(dptx);
	}
}

#if 0
static void rtk_dptx_stop_detect_hpd(struct rtk_prince_dptx *dptx)
{
	dev_info(dptx->dev, "prince dptx: stop detect hpd\n");
	if (dptx->hpd_gpio) {
		if (dptx->hpd_irq) {
			free_irq(dptx->hpd_irq, dptx);
			dptx->hpd_irq = 0;
		}
	} else {
		/* TODO: Detect HPD with dptx_hpd */
		// rtk_dptx_stop_hpd_thread(dptx);
	}
}
#endif

static enum drm_connector_status rtk_dptx_conn_detect
(struct drm_connector *connector, bool force)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(connector);
	enum drm_connector_status status = connector_status_disconnected;

	mutex_lock(&dptx->lock);
	if (dptx->connected) {
		dev_info(dptx->dev, "rtk prince dptx connected\n");
		status = connector_status_connected;
	}
	mutex_unlock(&dptx->lock);

	return status;
}

static void rtk_dptx_conn_destroy(struct drm_connector *connector)
{
	DRM_INFO("prince dptx: conn destroy\n");

	drm_connector_cleanup(connector);
}

static bool check_use_default_mode(struct rtk_prince_dptx *dptx)
{
	const struct drm_display_mode *m = &default_mode;
	struct drm_display_mode *mode;
	struct drm_connector *connector;
	struct device_node *node;

	dev_info(dptx->dev, "prince dptx: check use default mode\n");

	connector = &dptx->connector;

	node = of_parse_phandle(dptx->dev->of_node, "default-mode", 0);
	if (node) {
		mode = drm_mode_duplicate(connector->dev, m);
		if (!mode) {
			dev_err(dptx->dev, "failed to add mode %ux%ux@%u\n",
				m->hdisplay,
				m->vdisplay,
				drm_mode_vrefresh(m));
			return -ENOMEM;
		}

		drm_mode_set_name(mode);
		mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
		mode->flags |= DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
		drm_mode_probed_add(connector, mode);

		connector->display_info.width_mm = mode->width_mm;
		connector->display_info.height_mm = mode->height_mm;

		dev_info(dptx->dev, "use default mode (%dx%d)@%d\n",
			m->hdisplay, m->vdisplay, drm_mode_vrefresh(m));
		dev_info(dptx->dev, "width_mm x height_mm (%dx%d)\n",
			mode->width_mm, mode->height_mm);

		return 1;
	}

	return 0;
}

static int rtk_dptx_get_modes(struct rtk_prince_dptx *dptx)
{
	struct drm_connector *connector;
	const struct drm_edid *drm_edid;
	int num_modes = 0;

	if (check_use_default_mode(dptx))
		return 1;

	connector = &dptx->connector;

	if (connector->force == DRM_FORCE_ON) {
		num_modes += rtk_dptx_add_force_modes(connector);
		dev_info(dptx->dev, "prince dptx force mode, num_modes (%d)\n",
				num_modes);
	} else {
		drm_edid = drm_edid_read_ddc(connector, &dptx->aux.ddc);

		drm_edid_connector_update(&dptx->connector, drm_edid);

		if (drm_edid) {
			const struct edid *edid = drm_edid_raw(drm_edid);

			num_modes += drm_edid_connector_add_modes(&dptx->connector);
			dptx->sink_has_audio = drm_detect_monitor_audio(edid);

			dev_info(dptx->dev, "prince dptx get edid, num_modes (%d), %s audio\n",
				num_modes, dptx->sink_has_audio ? "Has" : "No");

			drm_edid_free(drm_edid);
		} else
			dev_err(dptx->dev, "prince dptx no edid!\n");
	}

	return num_modes;
}

static int rtk_car_dptx_get_modes(struct rtk_prince_dptx *dptx)
{
	struct drm_connector *connector;
	struct rtk_rpc_info *rpc_info = dptx->rpc_info;
	struct drm_display_mode *mode;
	struct drm_display_mode disp_mode;
	struct rpc_query_display_out_interface_timing timing;

	memset(&disp_mode, 0, sizeof(disp_mode));

	connector = &dptx->connector;

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP)
		timing.display_interface = DISPLAY_INTERFACE_eDP;
	else
		timing.display_interface = DISPLAY_INTERFACE_DP;

	rpc_query_out_interface_timing(rpc_info, &timing);

	disp_mode.clock       = timing.clock;
	disp_mode.hdisplay    = timing.hdisplay;
	disp_mode.hsync_start = timing.hsync_start;
	disp_mode.hsync_end   = timing.hsync_end;
	disp_mode.htotal      = timing.htotal;
	disp_mode.vdisplay    = timing.vdisplay;
	disp_mode.vsync_start = timing.vsync_start;
	disp_mode.vsync_end   = timing.vsync_end;
	disp_mode.vtotal      = timing.vtotal;
	disp_mode.flags       = 0;
	drm_mode_set_name(&disp_mode);

	dptx->mixer = timing.mixer;

	DRM_INFO("%s (%dx%d)@%d on %s\n", __func__,
		timing.hdisplay, timing.vdisplay,
		drm_mode_vrefresh(&disp_mode), mixer_names[timing.mixer]);

	mode = drm_mode_duplicate(connector->dev, &disp_mode);

	if (!mode) {
		DRM_ERROR("bad mode or failed to add mode\n");
		return -EINVAL;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	drm_mode_probed_add(connector, mode);

	return 1;
}

static int rtk_dptx_conn_get_modes(struct drm_connector *connector)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(connector);
	int num_modes = 0;

	if (dptx->dptx_data->get_modes != NULL) {
		num_modes = dptx->dptx_data->get_modes(dptx);
		if (num_modes < 0)
			dev_err(dptx->dev, "prince dptx get modes fail\n");
	}

	return num_modes;
}

static enum drm_mode_status rtk_dptx_conn_mode_valid
(struct drm_connector *connector, struct drm_display_mode *mode)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(connector);
	struct drm_display_info *display_info = &dptx->connector.display_info;
	u8 vic;
	int max_rate;

	if (connector->force == DRM_FORCE_ON)
		return MODE_OK;

	/* Input video bpc and color_formats */
	switch (display_info->bpc) {
	case 10:
		dptx->bpc = 10;
		break;
	case 8:
		dptx->bpc = 8;
		break;
	case 6:
		dptx->bpc = 6;
		break;
	default:
		dptx->bpc = 8;
		break;
	}

	if (!dptx->connected)
		return MODE_BAD;

	if (dptx->dptx_data->type == RTK_AUTOMOTIVE_TYPE)
		return MODE_OK;

	rtk_prince_dptx_get_max_rx_bandwidth(dptx, &dptx->link_train.rx_link_bw);
	rtk_prince_dptx_get_max_rx_lane_count(dptx, &dptx->link_train.rx_lane_count);

	if ((dptx->link_train.rx_link_bw != DP_LINK_BW_1_62) &&
	    (dptx->link_train.rx_link_bw != DP_LINK_BW_2_7) &&
	    (dptx->link_train.rx_link_bw != DP_LINK_BW_5_4)) {
		dev_err(dptx->dev, "RX Max Link BW is abnormal :%x !\n",
			dptx->link_train.rx_link_bw);
		dptx->link_train.rx_link_bw = DP_LINK_BW_2_7;
	}

	if (dptx->link_train.rx_lane_count == 0) {
		dev_err(dptx->dev, "RX Max Lane count is abnormal :%x !\n",
			dptx->link_train.rx_lane_count);
		dptx->link_train.rx_lane_count = (u8)LINK_LANE_COUNT_4;
	}

	if (dptx->link_train.rx_lane_count > dptx->video_info.max_lane_count)
		dptx->link_train.rx_lane_count = dptx->video_info.max_lane_count;
	if (dptx->link_train.rx_link_bw > dptx->video_info.max_link_bw)
		dptx->link_train.rx_link_bw = dptx->video_info.max_link_bw;

	max_rate = dptx->link_train.rx_lane_count *
				drm_dp_bw_code_to_link_rate(dptx->link_train.rx_link_bw);

	vic = drm_match_cea_mode(mode);

	if (vic >= VIC_3840X2160P24) {
		dev_err(dptx->dev, "prince dptx mode clock high (%d) >= (%d)\n",
			vic, VIC_3840X2160P24);
		return MODE_CLOCK_HIGH;
	}

	if (mode->clock > dptx->max_clock_k)
		return MODE_CLOCK_HIGH;

	/* efficiency is about 0.8 */
	if (max_rate < mode->clock * 3 * dptx->bpc / 8) {
		dev_err(dptx->dev, "prince dptx mode clock high (%d) < (%d)\n",
			max_rate, mode->clock * 3 * dptx->bpc / 8);
		return MODE_CLOCK_HIGH;
	}

	return MODE_OK;
}

static int rtk_dptx_conn_atomic_check(struct drm_connector *connector,
					  struct drm_atomic_state *state)
{
	struct drm_connector_state *old_connector_state;
	struct drm_crtc_state *new_crtc_state;
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(connector);

	old_connector_state = drm_atomic_get_old_connector_state(state, connector);

	if (old_connector_state->crtc) {
		new_crtc_state = drm_atomic_get_new_crtc_state(state, old_connector_state->crtc);
		if (dptx->is_autotest) {
			new_crtc_state->connectors_changed = true;
			dev_info(dptx->dev, "atomic check format changed");
		}
	}

	return 0;
}

static int rtk_dptx_conn_set_property(struct drm_connector *connector,
				struct drm_connector_state *state,
				struct drm_property *property,
				uint64_t val)
{
	int ret;
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(connector);

	ret = -EINVAL;
	if (property == dptx->link_rate_property) {
		ret = 0;
		dptx->prop_link_rate = (val == LINK_RATE_1_62) ? DP_LINK_RATE_1_62 :
						(val == LINK_RATE_2_7) ? DP_LINK_RATE_2_7 :
						(val == LINK_RATE_5_4) ? DP_LINK_RATE_5_4 :
						(val == LINK_RATE_8_1) ? DP_LINK_RATE_8_1 :
						LINK_RATE_UNSPECIFIED;
		if (dptx->prop_link_rate > LINK_RATE_UNSPECIFIED) {
			dptx->is_autotest = true;
			dev_info(dptx->dev, "[%s] link rate: %d(prop: %llu)\n",
				 __func__, dptx->prop_link_rate, val);
		}
	} else if (property == dptx->lane_count_property) {
		ret = 0;
		dptx->prop_lane_count = (val == LANE_COUNT_1) ? LANE_COUNT_1 :
						(val == LANE_COUNT_2) ? LANE_COUNT_2 :
						(val == LANE_COUNT_4) ? LANE_COUNT_4 :
						LANE_COUNT_UNSPECIFIED;
		if (dptx->prop_lane_count > LANE_COUNT_UNSPECIFIED) {
			dptx->is_autotest = true;
			dev_info(dptx->dev, "[%s] lane count: %u(prop: %llu)\n",
				 __func__, dptx->prop_lane_count, val);
		}
	} else if (property == dptx->train_pattern_property) {
		ret = 0;
		dptx->test_train_pattern = (val == TRAIN_PATTERN_2) ? DP_TRAINING_PATTERN_2 :
								(val == TRAIN_PATTERN_3) ? DP_TRAINING_PATTERN_3 :
								(val == TRAIN_PATTERN_4) ? DP_TRAINING_PATTERN_4 :
								TRAIN_PATTERN_UNSPECIFIED;
		if (dptx->test_train_pattern > TRAIN_PATTERN_UNSPECIFIED) {
			dptx->is_autotest = true;
			dev_info(dptx->dev, "[%s] test_train_pattern: %d(prop: %llu)\n",
				 __func__, dptx->test_train_pattern, val);
		}
	}

	return ret;
}

static int rtk_dptx_conn_get_property(struct drm_connector *connector,
				const struct drm_connector_state *state,
				struct drm_property *property,
				uint64_t *val)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(connector);
	int tmp;

	if (property == dptx->link_rate_property) {
		*val = (dptx->prop_link_rate == DP_LINK_RATE_1_62) ? LINK_RATE_1_62 :
			(dptx->prop_link_rate == DP_LINK_RATE_2_7) ? LINK_RATE_2_7 :
			(dptx->prop_link_rate == DP_LINK_RATE_5_4) ? LINK_RATE_5_4 :
			(dptx->prop_link_rate == DP_LINK_RATE_8_1) ? LINK_RATE_8_1 :
			LINK_RATE_UNSPECIFIED;
		return 0;
	} else if (property == dptx->lane_count_property) {
		*val = (dptx->prop_lane_count == LANE_COUNT_1) ? LANE_COUNT_1 :
			(dptx->prop_lane_count == LANE_COUNT_2) ? LANE_COUNT_2 :
			(dptx->prop_lane_count == LANE_COUNT_4) ? LANE_COUNT_4 :
			LANE_COUNT_UNSPECIFIED;
		return 0;
	} else if (property == dptx->train_pattern_property) {
		tmp = dptx->test_train_pattern;
		*val = (tmp == DP_TRAINING_PATTERN_2) ? TRAIN_PATTERN_2 :
			(tmp == DP_TRAINING_PATTERN_3) ? TRAIN_PATTERN_3 :
			(tmp == DP_TRAINING_PATTERN_4) ? TRAIN_PATTERN_4 :
			TRAIN_PATTERN_UNSPECIFIED;
		return 0;
	}

	return -EINVAL;
}

static const struct drm_connector_funcs rtk_dptx_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = rtk_dptx_conn_detect,
	.destroy = rtk_dptx_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_set_property = rtk_dptx_conn_set_property,
	.atomic_get_property = rtk_dptx_conn_get_property,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
#if defined(CONFIG_DEBUG_FS)
	.late_register = rtk_dptx_late_register,
#endif
};

static struct drm_connector_helper_funcs rtk_dptx_connector_helper_funcs = {
	.get_modes = rtk_dptx_conn_get_modes,
	.mode_valid = rtk_dptx_conn_mode_valid,
	.atomic_check = rtk_dptx_conn_atomic_check,
};

static int dptx_aux_isr(struct rtk_prince_dptx *dptx)
{
	unsigned int val;
	int ret = 0;
	int i;

	for (i = 0; i < RTK_DP_AUX_WAIT_REPLY_COUNT; i++) {
		val = rtk_dptx_read(dptx->dptx14_edp_reg_base, AUX_IRQ_EVENT);
		if (val & AUXDONE)
			break;
		mdelay(1);
	}

	if (val & RXERROR) {
		dev_dbg(dptx->dev, "prince dptx aux error\n");
		ret = -1;
	} else if (val & AUXDONE) {
		dev_dbg(dptx->dev, "prince dptx aux done\n");
		ret = 0;
	} else {
		dev_err(dptx->dev, "prince dptx aux not done, IRQ_EVENT: 0x%x\n", val);
		ret = -1;
		rtk_dptx_update(dptx->dptx14_edp_reg_base, AUX_RETRY_2, RETRY_TIMEOUT_EN | RETRY_ERROR_EN,
				 RETRY_TIMEOUT_EN | RETRY_ERROR_EN);
		rtk_dptx_update(dptx->dptx14_edp_reg_base, AUX_TIMEOUT, AUX_TIMEOUT_EN, AUX_TIMEOUT_EN);
	}

	if (val & NACK)
		dev_err(dptx->dev, "prince dptx aux NACK\n");


	val = rtk_dptx_read(dptx->dptx14_edp_reg_base, AUX_RETRY_1);
	if (val & RETRY_LOCK) {
		// unlock retry lock
		dev_err(dptx->dev, "prince dptx aux is lock\n");
		rtk_dptx_update(dptx->dptx14_edp_reg_base, AUX_RETRY_2, RETRY_EN, 0);
		rtk_dptx_update(dptx->dptx14_edp_reg_base, AUX_RETRY_2, RETRY_EN, RETRY_EN);
	}

	rtk_dptx_write(dptx->dptx14_edp_reg_base, AUX_IRQ_EVENT, AUX_ALL_IRQ);

	return ret;
}

static irqreturn_t rtk_dptx_aux_irq(int irq, void *dev_id)
{
	/* No use */

	return IRQ_HANDLED;
}

static int dptx_aux_get_data(struct rtk_prince_dptx *dptx, struct drm_dp_aux_msg *msg)
{
	u8 *buffer = msg->buffer;
	int i;
	int size = 0;
	unsigned int rd_ptr, wr_ptr;

	rd_ptr = rtk_dptx_read(dptx->dptx14_edp_reg_base, AUX_FIFO_RD_PTR);
	wr_ptr = rtk_dptx_read(dptx->dptx14_edp_reg_base, AUX_FIFO_WR_PTR);
	size = wr_ptr - rd_ptr;

	if (size > msg->size) {
		dev_err(dptx->dev, "wrong size: fifo(%u), msg(%zu)\n", size, msg->size);
		size = msg->size;
	}

	for (i = 0; i < size; i++)
		buffer[i] = rtk_dptx_read(dptx->dptx14_edp_reg_base, AUX_REPLY_DATA);

	return size;
}

static void dptx_aux_transfer(struct rtk_prince_dptx *dptx, struct drm_dp_aux_msg *msg)
{
	size_t size = msg->size;
	u32 addr = msg->address;
	u8 *buffer = msg->buffer;
	u8 data;
	int i;

	switch (msg->request & ~DP_AUX_I2C_MOT) {
	case DP_AUX_I2C_WRITE:
		if ((msg->request & DP_AUX_I2C_MOT) && size != 0)
			addr |= (0x1 << 6) << 16;

		if (size == 0) {
			size = 1;
			data = 0;
			buffer = &data;
		}
		break;
	case DP_AUX_I2C_READ:
		if (msg->request & DP_AUX_I2C_MOT)
			addr |= (0x10 | (0x1 << 6)) << 16;
		else
			addr |= 0x10 << 16;

		break;
	case DP_AUX_NATIVE_WRITE:
		addr |= 0x80 << 16;
		break;
	case DP_AUX_NATIVE_READ:
		addr |= 0x90 << 16;
		break;
	default:
		pr_err("transfer command not support !!!\n");
		return;
	}

	if (msg->size != 0)
		rtk_dptx_write(dptx->dptx14_edp_reg_base, AUX_FIFO_CTRL,
			 READ_FAIL_AUTO_EN | AUX_FIFO_CTRL_RESET);
	else
		rtk_dptx_write(dptx->dptx14_edp_reg_base, AUX_FIFO_CTRL, AUX_FIFO_CTRL_RESET);

	rtk_dptx_write(dptx->dptx14_edp_reg_base, AUX_IRQ_EVENT, AUX_ALL_IRQ);
	rtk_dptx_write(dptx->dptx14_edp_reg_base, AUXTX_REQ_CMD, (addr >> 16) & 0xFF);
	rtk_dptx_write(dptx->dptx14_edp_reg_base, AUXTX_REQ_ADDR_M, (addr >> 8) & 0xFF);
	rtk_dptx_write(dptx->dptx14_edp_reg_base, AUXTX_REQ_ADDR_L, addr & 0xFF);
	rtk_dptx_write(dptx->dptx14_edp_reg_base, AUXTX_REQ_LEN, (size > 0) ? (size - 1) : 0);

	if (!(msg->request & DP_AUX_I2C_READ)) {
		for (i = 0; i < size; i++)
			rtk_dptx_write(dptx->dptx14_edp_reg_base, AUXTX_REQ_DATA, buffer[i]);
	}

	if (msg->size == 0)
		rtk_dptx_update(dptx->dptx14_edp_reg_base, AUXTX_TRAN_CTRL,
			 TX_ADDRONLY | TX_START, TX_ADDRONLY | TX_START);
	else
		rtk_dptx_update(dptx->dptx14_edp_reg_base, AUXTX_TRAN_CTRL,
			 TX_ADDRONLY | TX_START, TX_START);
}

static ssize_t rtk_dptx_aux_transfer(struct drm_dp_aux *aux,
				     struct drm_dp_aux_msg *msg)
{
	struct rtk_prince_dptx *dptx = to_rtk_prince_dptx(aux);
	int ret = 0;

	pm_runtime_get_sync(dptx->dev);

	if (!dptx->connected) {
		dev_dbg(dptx->dev, "prince dptx disconnected, do not do aux transfer\n");
		msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
		ret = -ENODEV;
		goto out;
	}

	if (WARN_ON(msg->size > 16)) {
		DRM_ERROR("prince dptx aux msg size %ld too big\n", msg->size);
		goto out;
		// return -E2BIG;
	}

	ensure_clock_enabled(dptx);

	dptx_aux_transfer(dptx, msg);

	ret = dptx_aux_isr(dptx);
	if (ret < 0) {
		dev_err(dptx->dev, "aux transfer error\n");
		goto out;
		// return -ETIMEDOUT;
	}

	if ((msg->request & DP_AUX_I2C_READ) && msg->size != 0)
		ret = dptx_aux_get_data(dptx, msg);
	else
		ret = msg->size;

	rtk_dptx_update(dptx->dptx14_edp_reg_base, AUX_FIFO_CTRL,
		 AUX_FIFO_CTRL_ALL, AUX_FIFO_CTRL_RESET);

	if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_WRITE ||
	    (msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_READ)
		msg->reply = DP_AUX_I2C_REPLY_ACK;
	else if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_WRITE ||
		 (msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_READ)
		msg->reply = DP_AUX_NATIVE_REPLY_ACK;

out:
	pm_runtime_mark_last_busy(dptx->dev);
	pm_runtime_put_autosuspend(dptx->dev);

	return ret;
}

static int rtk_dptx_register(struct drm_device *drm, struct rtk_prince_dptx *dptx)
{
	struct drm_encoder *encoder = &dptx->encoder;
	struct drm_connector *connector = &dptx->connector;
	struct device *dev = dptx->dev;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	dev_info(dev, "prince dptx possible_crtcs (0x%x)\n", encoder->possible_crtcs);

	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	drm_encoder_init(drm, encoder, &rtk_dptx_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);

	drm_encoder_helper_add(encoder, dptx->dptx_data->helper_funcs);

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	drm_connector_init(drm, connector, &rtk_dptx_connector_funcs,
		dptx->connector_type);
	drm_connector_helper_add(connector, &rtk_dptx_connector_helper_funcs);

	drm_connector_attach_encoder(connector, encoder);

	return 0;
}

static int rtk_dptx_init_properties(struct rtk_prince_dptx *dptx)
{
	int ret;
	struct drm_property *prop;

	dev_info(dptx->dev, "prince dptx: init properties\n");

	if (dptx->connector.funcs->reset)
		dptx->connector.funcs->reset(&dptx->connector);

	ret = drm_connector_attach_max_bpc_property(&dptx->connector, 6, 16);
	if (ret) {
		dev_err(dptx->dev, "drm dptx attach max bpc property fail\n");
		return ret;
	}

	/* link rate property */
	prop = drm_property_create_enum(dptx->drm_dev, 0, "link rate",
				link_rate_list,
				ARRAY_SIZE(link_rate_list));
	if (!prop) {
		dev_err(dptx->dev, "create link rate enum property failed");
		return -ENOMEM;
	}
	dptx->link_rate_property = prop;
	drm_object_attach_property(&dptx->connector.base, prop, LINK_RATE_UNSPECIFIED);

	/* lane count property */
	prop = drm_property_create_enum(dptx->drm_dev, 0, "lane count",
				lane_count_list,
				ARRAY_SIZE(lane_count_list));
	if (!prop) {
		dev_err(dptx->dev, "create lane count enum property failed");
		return -ENOMEM;
	}
	dptx->lane_count_property = prop;
	drm_object_attach_property(&dptx->connector.base, prop, LANE_COUNT_UNSPECIFIED);

	/* train pattern property */
	prop = drm_property_create_enum(dptx->drm_dev, 0, "train pattern",
				train_pattern_list,
				ARRAY_SIZE(train_pattern_list));
	if (!prop) {
		dev_err(dptx->dev, "create train pattern enum property failed");
		return -ENOMEM;
	}
	dptx->train_pattern_property = prop;
	drm_object_attach_property(&dptx->connector.base, prop, TRAIN_PATTERN_UNSPECIFIED);

	return 0;
}

static int rtk_dptx_parse_dt(struct rtk_prince_dptx *dptx)
{
	struct platform_device *pdev __attribute__((unused));
	struct device *dev;
	struct device_node *syscon_np;
	struct regmap *iso_pinctl;
	int ret = 0;
	int initial_gpio_level;

	dev = dptx->dev;
	pdev = to_platform_device(dev);

	if (dptx->dptx_data->type == RTK_AUTOMOTIVE_TYPE)
		goto init_dp_hpd;

	dev_info(dev, "prince dptx: parse dt\n");

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "dptx parse syscon phandle 0 fail");
		return -ENODEV;
	}

	dptx->crt_reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(dptx->crt_reg_base)) {
		dev_err(dev, "regmap syscon 0 to crt_reg_base fail");
		of_node_put(syscon_np);
		return PTR_ERR(dptx->crt_reg_base);
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 1);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "dptx parse syscon phandle 1 fail");
		return -ENODEV;
	}

	dptx->dptx14_edp_reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(dptx->dptx14_edp_reg_base)) {
		dev_err(dev, "regmap syscon 1 to dptx14_edp_reg_base fail");
		of_node_put(syscon_np);
		return PTR_ERR(dptx->dptx14_edp_reg_base);
	}

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP) {
		dptx->clk_edptx = devm_clk_get(dev, "clk_en_edptx");
		if (IS_ERR(dptx->clk_edptx)) {
			dev_err(dev, "failed to get clock\n");
			return PTR_ERR(dptx->clk_edptx);
		}

		dptx->rstc_edptx = devm_reset_control_get(dev, "rstn_edptx");
		if (IS_ERR(dptx->rstc_edptx)) {
			dev_err(dev, "failed to get reset controller\n");
			return PTR_ERR(dptx->rstc_edptx);
		}

		syscon_np = of_parse_phandle(dev->of_node, "syscon", 2);
		if (IS_ERR_OR_NULL(syscon_np)) {
			dev_err(dev, "dptx parse syscon phandle 2 fail");
			return -ENODEV;
		}

		dptx->edp_wrapper_reg_base = syscon_node_to_regmap(syscon_np);
		if (IS_ERR(dptx->edp_wrapper_reg_base)) {
			dev_err(dev, "regmap syscon 2 to edp_wrapper_reg_base fail");
			of_node_put(syscon_np);
			return PTR_ERR(dptx->edp_wrapper_reg_base);
		}
	} else {
		dptx->clk_dptx = devm_clk_get(dev, "clk_en_dptx");
		if (IS_ERR(dptx->clk_dptx)) {
			dev_err(dev, "failed to get clock\n");
			return PTR_ERR(dptx->clk_dptx);
		}
		clk_prepare_enable(dptx->clk_dptx);

		dptx->rstc_dptx = devm_reset_control_get(dev, "rstn_dptx");
		if (IS_ERR(dptx->rstc_dptx)) {
			dev_err(dev, "failed to get reset controller\n");
			return PTR_ERR(dptx->rstc_dptx);
		}
		reset_control_deassert(dptx->rstc_dptx);

		syscon_np = of_parse_phandle(dev->of_node, "syscon", 2);
		if (IS_ERR_OR_NULL(syscon_np)) {
			dev_err(dev, "prince dptx parse syscon phandle 2 fail");
			return -ENODEV;
		}

		dptx->dptx14_reg_base = syscon_node_to_regmap(syscon_np);
		if (IS_ERR(dptx->dptx14_reg_base)) {
			dev_err(dev, "regmap syscon 2 to dptx14_reg_base fail");
			of_node_put(syscon_np);
			return PTR_ERR(dptx->dptx14_reg_base);
		}

		syscon_np = of_parse_phandle(dev->of_node, "syscon", 3);
		if (IS_ERR_OR_NULL(syscon_np)) {
			dev_err(dev, "prince dptx parse syscon phandle 3 fail");
			return -ENODEV;
		}

		dptx->dptx14_mac_reg_base = syscon_node_to_regmap(syscon_np);
		if (IS_ERR(dptx->dptx14_mac_reg_base)) {
			dev_err(dev, "regmap syscon 3 to dptx14_mac_reg_base fail");
			of_node_put(syscon_np);
			return PTR_ERR(dptx->dptx14_mac_reg_base);
		}
	}

	ret = of_property_read_u32(dev->of_node, "max-clock-k",
				&dptx->max_clock_k);
	if (ret < 0)
		dptx->max_clock_k = RTK_EDP_MAX_CLOCK_K;
	dev_info(dev, "eDP max-clock-k=%u\n", dptx->max_clock_k);

	dptx->iso_sys_base = syscon_regmap_lookup_by_phandle(dev->of_node, "realtek,iso_sys");
	if (IS_ERR(dptx->iso_sys_base)) {
		dev_err(dev, "couldn't get iso register base address\n");
		return PTR_ERR(dptx->iso_sys_base);
	}

	dptx->aux_irq = platform_get_irq(pdev, 0);
	if (dptx->aux_irq < 0) {
		dev_err(dev, "can't get aux irq resource\n");
		return -ENODEV;
	}

	irq_set_irq_type(dptx->aux_irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(dev, dptx->aux_irq, rtk_dptx_aux_irq,
			       IRQF_SHARED, dev_name(dev), dptx);
	if (ret) {
		dev_err(dev, "can't request aux irq resource\n");
		return -ENODEV;
	}

	dptx->aux.name = "prince dptx-AUX";
	dptx->aux.transfer = rtk_dptx_aux_transfer;
	dptx->aux.dev = dev;
	dptx->aux.drm_dev = dptx->drm_dev;
	ret = drm_dp_aux_register(&dptx->aux);
	if (ret) {
		dev_err(dev, "drm dp aux register fail\n");
		return ret;
	}

	iso_pinctl = syscon_regmap_lookup_by_phandle(dev->of_node, "realtek,pinctrl");
	if (IS_ERR(iso_pinctl)) {
		DRM_ERROR("fail to get iso pinctl reg\n");
		return PTR_ERR(iso_pinctl);
	}

	dptx->dp5v_gpio = NULL;

	dptx->dp5v_gpio = devm_gpiod_get(dev, "dp5v", GPIOD_OUT_HIGH);
	if (IS_ERR(dptx->dp5v_gpio))
		dev_info(dev, "Not support dp_5v control\n");
	else
		dev_info(dev, "dp5v gpio(%d)\n", desc_to_gpio(dptx->dp5v_gpio));

init_dp_hpd:

	dptx->hpd_gpio = devm_gpiod_get_optional(dev, "hpd", GPIOD_IN);
	if (dptx->hpd_gpio) {
		if (IS_ERR(dptx->hpd_gpio))
			return dev_err_probe(dev, PTR_ERR(dptx->hpd_gpio),
						"Could not get hpd gpio\n");
		dev_info(dev, "prince dptx hotplug gpio(%d)\n", desc_to_gpio(dptx->hpd_gpio));

		ret = gpiod_direction_input(dptx->hpd_gpio);
		if (ret)
			dev_err(dev, "failed to set hpd_gpio direction");

		ret = gpiod_set_debounce(dptx->hpd_gpio, RTK_HPD_GPIO_PLUG_DEB_TIME_US);
		if (ret)
			dev_err(dptx->dev, "failed to set hpd_gpio debounce");

		INIT_DELAYED_WORK(&dptx->hpd_gpio_work, rtk_dptx_hpd_gpio_worker);

		rtk_dptx_start_detect_hpd(dptx);
	} else {
		dev_info(dev, "no hpd_gpios node, utilze dptx_hpd\n");
		/**
		 * TODO: Mux to dptx_hpd
		 * rtk_dptx_update(iso_pinctl, MAIN2_MUXPAD2,
		 * MAIN2_MUXPAD2_GPIO_22, MAIN2_MUXPAD2_GPIO_22);
		 */
	}

	initial_gpio_level = gpiod_get_value(dptx->hpd_gpio);
	if (initial_gpio_level < 0) {
		dev_err(dptx->dev, "Failed to read initial GPIO: %d\n",
			initial_gpio_level);
		return initial_gpio_level;
	}

	dptx->connected = (initial_gpio_level == 1);

	dev_info(dptx->dev, "Initial HPD state: %s (GPIO=%d)\n",
		dptx->connected ? "Connected" : "Disconnected",
		initial_gpio_level);

	if (dptx->connected) {
		dev_info(dptx->dev,
		"Device already connected at boot, triggering initialization\n");

		schedule_delayed_work(&dptx->hpd_gpio_work,
			msecs_to_jiffies(RTK_HPD_SHORT_PULSE_THRESHOLD_MS));
	}

	return ret;
}

static int rtk_dptx_suspend(struct device *dev)
{
	struct rtk_prince_dptx *dptx = dev_get_drvdata(dev);

	if (!dptx)
		return 0;

	dev_info(dptx->dev, "prince dptx: suspend\n");

	// rtk_dptx_stop_detect_hpd(dptx);

	if (dptx->dptx_data->type == RTK_AUTOMOTIVE_TYPE)
		return 0;

	if (dptx->connector_type == DRM_MODE_CONNECTOR_eDP) {
		if (__clk_is_enabled(dptx->clk_edptx))
			clk_disable_unprepare(dptx->clk_edptx);
		reset_control_assert(dptx->rstc_edptx);
	} else {
		if (__clk_is_enabled(dptx->clk_dptx))
			clk_disable_unprepare(dptx->clk_dptx);
		reset_control_assert(dptx->rstc_dptx);
	}

	return 0;
}

static int rtk_dptx_resume(struct device *dev)
{
	struct rtk_prince_dptx *dptx = dev_get_drvdata(dev);

	if (!dptx)
		return 0;

	dev_info(dptx->dev, "prince dptx: resume\n");

	if (dptx->dptx_data->type == RTK_NORMAL_TYPE) {
		ensure_clock_enabled(dptx);
		rtk_prince_dptx_aux_init(dptx);
	}

	return 0;
}

static DEFINE_RUNTIME_DEV_PM_OPS(rtk_dptx_pm_ops, rtk_dptx_suspend,
		rtk_dptx_resume, NULL);

static int rtk_dptx_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct drm_device *drm = data;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_prince_dptx *dptx = dev_get_drvdata(dev);
	int ret;

	dev_info(dev, "prince dptx: bind\n");

	dptx->drm_dev = drm;

	dptx->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	dev_info(dev, "prince dptx->rpc_info (%p)\n", dptx->rpc_info);

	ret = rtk_dptx_register(drm, dptx);
	if (ret) {
		dev_err(dptx->dev, "drm dptx register fail\n");
		return ret;
	}

	ret = rtk_dptx_init_properties(dptx);
	if (ret) {
		dev_err(dptx->dev, "rtk_dptx_init_properties fail\n");
		return ret;
	}

	ret = rtk_dptx_parse_dt(dptx);
	if (ret) {
		dev_err(dptx->dev, "rtk_dptx_parse_dt fail\n");
		return ret;
	}

	dptx->prop_lane_count = LANE_COUNT_UNSPECIFIED;
	dptx->prop_link_rate  = LINK_RATE_UNSPECIFIED;

	pm_runtime_use_autosuspend(dptx->dev);
	pm_runtime_set_autosuspend_delay(dptx->dev, 100);
	pm_runtime_enable(dptx->dev);

	return 0;
}

static void rtk_dptx_unbind(struct device *dev, struct device *master,
			     void *data)
{
	struct rtk_prince_dptx *dptx = dev_get_drvdata(dev);

	dev_info(dev, "prince dptx: unbind\n");

	if (IS_ENABLED(CONFIG_PM)) {
		pm_runtime_dont_use_autosuspend(dptx->dev);
		pm_runtime_disable(dptx->dev);
	} else {
		rtk_dptx_suspend(dev);
	}
}

static const struct component_ops rtk_dptx_ops = {
	.bind	= rtk_dptx_bind,
	.unbind	= rtk_dptx_unbind,
};

static const struct rtk_prince_dptx_platform_data rtk_1761_dptx_data = {
	.type = RTK_NORMAL_TYPE,
	.connector_type = DRM_MODE_CONNECTOR_DisplayPort,
	.get_modes = rtk_dptx_get_modes,
	.helper_funcs = &rtk_dptx_encoder_helper_funcs,
	.max_link_bw = DP_LINK_BW_5_4,
	.max_lane_count = 4,
};

static const struct rtk_prince_dptx_platform_data rtk_1761_edptx_data = {
	.type = RTK_NORMAL_TYPE,
	.connector_type = DRM_MODE_CONNECTOR_eDP,
	.get_modes = rtk_dptx_get_modes,
	.helper_funcs = &rtk_dptx_encoder_helper_funcs,
	.max_link_bw = DP_LINK_BW_5_4,
	.max_lane_count = 4,
};

static const struct rtk_prince_dptx_platform_data rtk_1761_car_dptx_data = {
	.type = RTK_AUTOMOTIVE_TYPE,
	.connector_type = DRM_MODE_CONNECTOR_DisplayPort,
	.get_modes = rtk_car_dptx_get_modes,
	.helper_funcs = &rtk_car_dptx_encoder_helper_funcs,
	.max_link_bw = 0,
	.max_lane_count = 0,
};

static const struct rtk_prince_dptx_platform_data rtk_1761_car_edptx_data = {
	.type = RTK_AUTOMOTIVE_TYPE,
	.connector_type = DRM_MODE_CONNECTOR_eDP,
	.get_modes = rtk_car_dptx_get_modes,
	.helper_funcs = &rtk_car_dptx_encoder_helper_funcs,
	.max_link_bw = 0,
	.max_lane_count = 0,
};

static const struct of_device_id rtk_prince_dptx_dt_ids[] = {
	{
		.compatible = "realtek,rtk-1761-dptx",
		.data = &rtk_1761_dptx_data,
	},
	{
		.compatible = "realtek,rtk-1761-edptx",
		.data = &rtk_1761_edptx_data,
	},
	{
		.compatible = "realtek,rtk-1761-car-dptx",
		.data = &rtk_1761_car_dptx_data,
	},
	{
		.compatible = "realtek,rtk-1761-car-edptx",
		.data = &rtk_1761_car_edptx_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_prince_dptx_dt_ids);

static int rtk_dptx_audio_hw_params(struct device *dev,  void *data,
				  struct hdmi_codec_daifmt *daifmt,
				  struct hdmi_codec_params *params)
{
	struct rtk_prince_dptx *dptx = dev_get_drvdata(dev);
	struct dp_audio_info ainfo = {
		.sample_width = params->sample_width,
		.sample_rate = params->sample_rate,
		.channels = params->channels,
	};
	int ret = 0;

	dev_info(dev, "prince dptx: audio hw params\n");
	dev_info(dev, "prince dptx (%p), dev (%p), dptx->audio_pdev (%p)\n",
					dptx, dev, dptx->audio_pdev);

	dev_info(dev, "prince dptx: sample_rate = %d, sample_width = %d, channels = %d\n",
		ainfo.sample_rate, ainfo.sample_width, ainfo.channels);

	switch (daifmt->fmt) {
	case HDMI_I2S:
		ainfo.format = DP_AUDIO_FMT_I2S;
		break;
	case HDMI_SPDIF:
		ainfo.format = DP_AUDIO_FMT_SPDIF;
		break;
	default:
		DRM_ERROR("Invalid audio format %d\n", daifmt->fmt);
		ret = -EINVAL;
		goto out;
	}

	/* TODO */
	// rtk_dptx_phy_config_audio(dptx, &ainfo);

out:
	return ret;
}

static void rtk_dptx_audio_shutdown(struct device *dev, void *data)
{
	struct rtk_prince_dptx *dptx = dev_get_drvdata(dev);

	dev_info(dev, "prince dptx: audio shutdown\n");
	dev_info(dev, "prince dptx (%p), dev (%p), dptx->audio_pdev (%p)\n",
					dptx, dev, dptx->audio_pdev);

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_SEC_FUNCTION_CTRL,
									AUDIO_EN, 0);
}

static int rtk_dptx_audio_startup(struct device *dev, void *data)
{
	struct rtk_prince_dptx *dptx = dev_get_drvdata(dev);

	dev_info(dev, "prince dptx: audio startup\n");
	dev_info(dev, "prince dptx (%p), dev (%p), dptx->audio_pdev (%p)\n",
			dptx, dev, dptx->audio_pdev);

	rtk_dptx_update(dptx->dptx14_mac_reg_base, DPTX14_MAC_IP_SEC_FUNCTION_CTRL,
									AUDIO_EN, AUDIO_EN);
	return 0;
}

static int rtk_dptx_audio_hook_plugged_cb(struct device *dev, void *data,
					hdmi_codec_plugged_cb fn,
					struct device *codec_dev)
{
	struct rtk_prince_dptx *dptx = data;

	dev_info(dev, "prince dptx: audio hook plugged cb\n");
	dev_info(dev, "prince dptx (%p), dev (%p), dptx->audio_pdev (%p)\n",
					dptx, dev, dptx->audio_pdev);

	// mutex_lock(&dptx->update_plugged_status_lock);
	dptx->plugged_cb = fn;
	dptx->codec_dev = codec_dev;
	// mutex_unlock(&dptx->update_plugged_status_lock);

	rtk_dptx_update_plugged_status(dptx);

	return 0;
}

static int rtk_dptx_audio_get_eld(struct device *dev, void *data, uint8_t *buf,
				size_t len)
{
	struct rtk_prince_dptx *dptx = dev_get_drvdata(dev);

	dev_info(dev, "prince dptx: audio get eld\n");

	if (dptx->connected)
		memcpy(buf, dptx->connector.eld, len);
	else
		memset(buf, 0, len);

	return 0;
}

static const struct hdmi_codec_ops rtk_dptx_audio_codec_ops = {
	.hw_params = rtk_dptx_audio_hw_params,
	.audio_shutdown = rtk_dptx_audio_shutdown,
	.audio_startup = rtk_dptx_audio_startup,
	.hook_plugged_cb = rtk_dptx_audio_hook_plugged_cb,
	// .mute_stream = cdn_dp_audio_mute_stream,
	.get_eld = rtk_dptx_audio_get_eld,
	// .no_capture_mute = 1,
};

static int rtk_dptx_audio_codec_init(struct rtk_prince_dptx *dptx,
				   struct device *dev)
{
	struct hdmi_codec_pdata codec_data = {
		.i2s   = 1,
		.spdif = 1,
		.ops   = &rtk_dptx_audio_codec_ops,
		.data = dptx,
	};

	dev_info(dev, "prince dptx: audio codec init\n");

	dptx->audio_pdev = platform_device_register_data(
			 dev, HDMI_CODEC_DRV_NAME, PLATFORM_DEVID_AUTO,
			 &codec_data, sizeof(codec_data));

	dev_info(dev, "prince dptx (%p), dev (%p), dptx->audio_pdev (%p), &codec_data (%p)\n",
		dptx, dev, dptx->audio_pdev, &codec_data);

	return PTR_ERR_OR_ZERO(dptx->audio_pdev);
}

static int rtk_dptx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_prince_dptx *dptx;
	const struct rtk_prince_dptx_platform_data *dptx_data;
	int ret;

	dev_info(dev, "prince dptx: probe\n");

	dptx = devm_kzalloc(dev, sizeof(*dptx), GFP_KERNEL);
	if (!dptx)
		return -ENOMEM;

	dptx->dev = dev;
	dptx->dptx_data = of_device_get_match_data(&pdev->dev);
	dptx_data = dptx->dptx_data;

	dptx->video_info.max_link_bw = dptx_data->max_link_bw;
	dptx->video_info.max_lane_count = dptx_data->max_lane_count;

	dptx->connector_type = dptx_data->connector_type;

	mutex_init(&dptx->lock);
	dev_set_drvdata(dev, dptx);

	ret = rtk_dptx_audio_codec_init(dptx, dev);
	if (ret)
		return ret;

	return component_add(&pdev->dev, &rtk_dptx_ops);
}

static void rtk_dptx_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_dptx_ops);
}

struct platform_driver rtk_prince_dptx_driver = {
	.probe  = rtk_dptx_probe,
	.remove = rtk_dptx_remove,
	.driver = {
		.name = "rtk-prince-dptx",
		.of_match_table = rtk_prince_dptx_dt_ids,
#if IS_ENABLED(CONFIG_PM)
		.pm = &rtk_dptx_pm_ops,
#endif
	},
};

MODULE_AUTHOR("Ray Tang <ray.tang@realtek.com>");
MODULE_DESCRIPTION("Realtek Prince DisplayPort Driver");
MODULE_LICENSE("GPL v2");
