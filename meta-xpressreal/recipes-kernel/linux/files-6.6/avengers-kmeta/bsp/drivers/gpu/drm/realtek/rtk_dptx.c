// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek DisplayPort driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#include <drm/drm_of.h>
#include <drm/drm_print.h>

#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/extcon.h>
#include <linux/clk-provider.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <sound/hdmi-codec.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_edid.h>
#include <drm/display/drm_dp_helper.h>

#include <video/videomode.h>
#include "rtk_drm_drv.h"
#include "rtk_dptx_kent_reg.h"
#include "rtk_crt_reg.h"
#include "rtk_dp_utils.h"
#include "rtk_dptx.h"
#include "rtk_dptx_phy.h"

#define to_rtk_dptx(x) container_of(x, struct rtk_dptx, x)

#define ISO_MUXPAD1	0x4
#define ISO_MUXPAD1_GPIO_16_MASK (0x4 << 5)
#define ISO_MUXPAD1_GPIO_16_MUX_DPTX (1 << 4)
#define ISO_PFUN1 0x20
#define ISO_MUXPAD1_GPIO_16_PUD_MASK (0x3 << 19)
#define ISO_MUXPAD1_GPIO_16_PUD_DISABLE (0 << 19) // Disable pull up/down func


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
#define RTK_POLL_HPD_INTERVAL_MS 1
#define RTK_HPD_GPIO_PLUG_DEB_TIME_US 100
#define RTK_HPD_GPIO_UNPLUG_DEB_TIME_US 50000
#define RTK_HPD_SHORT_PULSE_THRESHOLD_MS 5
#define RTK_DP_MAX_LINK_RATE DP_LINK_RATE_8_1
#define RTK_DP_MAX_SWING 3
#define RTK_DP_MAX_EMPHASIS 3
#define RTK_DP_MAX_CLOCK_K 348500 // 4096x2160p60

/* ISO */
#define ISO_SOFT_RESET    (0x88)
#define ISO_CLOCK_ENABLE  (0x8c)
#define RSTN_USB3_P2_MDIO (1 << 28)
#define CLK_EN_USB_P4     (1 << 0)

/* AUDIO */
#define AUDIO_EN BIT(0)

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

struct rtk_dptx_platform_data {
	unsigned int type;
	int (*get_modes)(struct rtk_dptx *dptx);
	unsigned int max_phy;
	const struct drm_encoder_helper_funcs *helper_funcs;
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

	.width_mm = 340,
	.height_mm = 190,
};

void rtk_dptx14_update(struct rtk_dptx *dptx, u32 reg, u32 clear, u32 bits)
{
	unsigned int val;

	if (!dptx->dptx14_reg_base)
		return;

	regmap_read(dptx->dptx14_reg_base, reg, &val);

	val &= ~clear;
	val |= bits;

	regmap_write(dptx->dptx14_reg_base, reg, val);
}

static u32 rtk_dptx14_read(struct rtk_dptx *dptx, u32 reg)
{
	unsigned int val;

	if (!dptx->dptx14_reg_base)
		return 0;

	regmap_read(dptx->dptx14_reg_base, reg, &val);
	return val;
}

void rtk_dptx14_write(struct rtk_dptx *dptx, u32 reg, u32 val)
{
	if (!dptx->dptx14_reg_base)
		return;

	regmap_write(dptx->dptx14_reg_base, reg, val);
}

void rtk_dptx14_mac_update(struct rtk_dptx *dptx, u32 reg, u32 clear, u32 bits)
{
	unsigned int val;

	if (!dptx->dptx14_mac_reg_base)
		return;

	regmap_read(dptx->dptx14_mac_reg_base, reg, &val);

	val &= ~clear;
	val |= bits;

	regmap_write(dptx->dptx14_mac_reg_base, reg, val);
}

void rtk_dptx14_mac_write(struct rtk_dptx *dptx, u32 reg, u32 val)
{
	if (!dptx->dptx14_mac_reg_base)
		return;

	regmap_write(dptx->dptx14_mac_reg_base, reg, val);
}

void rtk_dptx14_aphy_update(struct rtk_dptx *dptx, u32 reg, u32 clear, u32 bits)
{
	unsigned int val;

	if (!dptx->dptx14_aphy_reg_base)
		return;

	regmap_read(dptx->dptx14_aphy_reg_base, reg, &val);

	val &= ~clear;
	val |= bits;

	regmap_write(dptx->dptx14_aphy_reg_base, reg, val);
}

void rtk_dptx14_aphy_write(struct rtk_dptx *dptx, u32 reg, u32 val)
{
	if (!dptx->dptx14_aphy_reg_base)
		return;

	regmap_write(dptx->dptx14_aphy_reg_base, reg, val);
}

void rtk_dptx14_crt_update(struct rtk_dptx *dptx, u32 reg, u32 clear, u32 bits)
{
	unsigned int val;

	if (!dptx->crt_reg_base)
		return;

	regmap_read(dptx->crt_reg_base, reg, &val);

	val &= ~clear;
	val |= bits;

	regmap_write(dptx->crt_reg_base, reg, val);
}

void rtk_dptx14_crt_write(struct rtk_dptx *dptx, u32 reg, u32 val)
{
	if (!dptx->crt_reg_base)
		return;

	regmap_write(dptx->crt_reg_base, reg, val);
}

static int rtk_dptx_get_bpc(struct rtk_dptx *dptx)
{
	struct drm_display_info *display_info = &dptx->connector.display_info;

	switch (display_info->bpc) {
	//	bpc 10 has some problem
	//	case 10:
	//	return 10;
	case 8:
		return 8;
	case 6:
		return 6;
	default:
		return 8;
	}
	return 8;
}

static void ensure_clock_enabled(struct rtk_dptx *dptx)
{
	if (__clk_is_enabled(dptx->clk_dptx) &&
		(!reset_control_status(dptx->rstc_dptx))) {
		dev_dbg(dptx->dev, "dptx clk already on\n");
	} else {
		dev_info(dptx->dev, "dptx clk off, reinit\n");

		clk_prepare_enable(dptx->clk_dptx);
		reset_control_deassert(dptx->rstc_dptx);
	}
}

static void rtk_dptx_init(struct rtk_dptx *dptx)
{
	dev_info(dptx->dev, "dptx: init\n");

	ensure_clock_enabled(dptx);

	/* enable aux channel */
	rtk_dptx14_update(dptx, DPTX14_MAIN, 0x100, 0x100);
	rtk_dptx14_write(dptx, DPTX14_IP_AUX_TX_CTRL, AUX_EN);
	rtk_dptx14_write(dptx, DPTX14_IP_AUX_IRQ_EN, AUX_ALL_IRQ);

	// Diable NACK retry
	rtk_dptx14_update(dptx, DPTX14_IP_AUX_RETRY_2, RETRY_NACK_EN, 0);

	/* DP Compliance Test 4.2.1.1, 4.2.1.2 */
	// Disable hw timeout/error retry after HPD plug event.
	rtk_dptx14_update(dptx, DPTX14_IP_AUX_RETRY_2,
					 RETRY_TIMEOUT_EN | RETRY_ERROR_EN, 0);
	rtk_dptx14_update(dptx, DPTX14_IP_AUX_TIMEOUT, AUX_TIMEOUT_EN, 0);
}

static int rtk_dptx_deinit_hw(struct rtk_dptx *dptx)
{
	struct drm_display_mode *mode = &dptx->encoder.crtc->state->adjusted_mode;
	struct rtk_rpc_info *rpc_info = (dptx->rpc_info_vo == NULL) ?
									 dptx->rpc_info : dptx->rpc_info_vo;
	struct rpc_set_display_out_interface interface;
	int ret;
	int mixer;

#ifdef CONFIG_CHROME_PLATFORMS
	mixer = DISPLAY_INTERFACE_MIXER1;
#else
	mixer = DISPLAY_INTERFACE_MIXER3;
#endif

	interface.display_interface       = DISPLAY_INTERFACE_DP;
	interface.width                   = mode->hdisplay;
	interface.height                  = mode->vdisplay;
	interface.frame_rate              = drm_mode_vrefresh(mode);
	interface.display_interface_mixer = DISPLAY_INTERFACE_MIXER_NONE;

	DRM_INFO("[%s] disable %s on %s\n", __func__,
		interface_names[interface.display_interface], mixer_names[mixer]);

	ret = rpc_set_out_interface(rpc_info, &interface);
	if (ret)
		DRM_ERROR("rpc_set_out_interface rpc fail\n");


	rtk_dptx_phy_disable_timing_gen(dptx);
	return ret;
}

static void rtk_dptx_poweroff(struct rtk_dptx *dptx)
{
	mutex_lock(&dptx->lock);

	if (!dptx->active) {
		dev_info(dptx->dev, "dptx: already poweroff\n");
		goto unlock;
	}

	dev_info(dptx->dev, "dptx: poweroff\n");

	rtk_dptx_deinit_hw(dptx);

	drm_dp_dpcd_writeb(&dptx->aux, DP_SET_POWER, DP_SET_POWER_D3);
	usleep_range(2000, 3000);

	dptx->active = false;

	if (__clk_is_enabled(dptx->clk_dptx))
		clk_disable_unprepare(dptx->clk_dptx);
	reset_control_assert(dptx->rstc_dptx);

unlock:
	mutex_unlock(&dptx->lock);
}

static void check_iso_clock_is_on(struct rtk_dptx *dptx)
{
	unsigned int val;

	regmap_read(dptx->iso_base, ISO_SOFT_RESET, &val);
	if (val & RSTN_USB3_P2_MDIO) {
		dev_info(dptx->dev, "RSTN_USB3_P2_MDIO already set\n");
	} else {
		val |= RSTN_USB3_P2_MDIO;
		regmap_write(dptx->iso_base, ISO_SOFT_RESET, val);
	}

	regmap_read(dptx->iso_base, ISO_CLOCK_ENABLE, &val);
	if (val & CLK_EN_USB_P4) {
		dev_info(dptx->dev, "CLK_EN_USB_P4 already set\n");
	} else {
		val |= CLK_EN_USB_P4;
		regmap_write(dptx->iso_base, ISO_CLOCK_ENABLE, val);
	}
}

static bool rtk_dptx_enc_mode_fixup(struct drm_encoder *encoder,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	DRM_INFO("dptx: enc mode fixup\n");

	return true;
}

static void rtk_dptx_set_bad_connector_link_status(struct rtk_dptx *dptx)
{
	struct drm_device *dev;
	struct drm_connector *connector;

	connector = &dptx->connector;
	dev = connector->dev;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	connector->state->link_status = DRM_MODE_LINK_STATUS_BAD;
	drm_modeset_unlock(&dev->mode_config.connection_mutex);
}

#ifdef CONFIG_CHROME_PLATFORMS
static int rtk_dptx_get_link_status(struct rtk_dptx *dptx)
{
	if (!dptx->connected)
		return -ENODEV;

	if (drm_dp_dpcd_read(&dptx->aux, DP_LANE0_1_STATUS, dptx->link_status,
			     DP_LINK_STATUS_SIZE) < 0) {
		DRM_ERROR("Failed to get link status\n");
		return -ENODEV;
	}

	return 0;
}

static bool rtk_dptx_is_link_status_ok(struct rtk_dptx *dptx)
{
	return drm_dp_channel_eq_ok(dptx->link_status, dptx->lane_count);
}

static void rtk_dptx_retrain_link_worker(struct work_struct *retrain_link_work)
{
	struct rtk_dptx *dptx = to_rtk_dptx(retrain_link_work);

	if (!dptx->connected)
		return;

	rtk_dptx_set_bad_connector_link_status(dptx);
	drm_kms_helper_connector_hotplug_event(&dptx->connector);

	dev_info(dptx->dev, "[%s] dptx hotplug event\n", __func__);
}

static uint8_t rtk_dptx_handle_automated_link_training_test(struct rtk_dptx *dptx)
{
	/* DP Compliance Test 4.3.3.1 */
	int status = 0;
	uint8_t test_lane_count;
	uint8_t test_link_rate;

	status = drm_dp_dpcd_readb(&dptx->aux, DP_TEST_LANE_COUNT,
				   &test_lane_count);
	if (status <= 0) {
		dev_err(dptx->dev, "dptx read test lane count error\n");
		return DP_TEST_NAK;
	}

	status = drm_dp_dpcd_readb(&dptx->aux, DP_TEST_LINK_RATE,
				   &test_link_rate);
	if (status <= 0) {
		dev_err(dptx->dev, "dptx read test link rate error\n");
		return DP_TEST_NAK;
	}

	if (test_lane_count <= RTK_DP_MAX_LANE_COUNT &&
		 drm_dp_bw_code_to_link_rate(test_link_rate) <= RTK_DP_MAX_LINK_RATE) {
		dptx->lane_count = test_lane_count;
		dptx->link_rate = drm_dp_bw_code_to_link_rate(test_link_rate);
		dptx->is_autotest = true;
	}

	return DP_TEST_ACK;
}

static void rtk_dptx_handle_automated_test_request(struct rtk_dptx *dptx)
{
	int status;
	uint8_t request;
	uint8_t response = DP_TEST_NAK;

	status = drm_dp_dpcd_readb(&dptx->aux, DP_TEST_REQUEST, &request);
	if (status < 0) {
		dev_err(dptx->dev, "dptx read test request error\n");
		return;
	}

	switch (request) {
	case DP_TEST_LINK_TRAINING:
		response = rtk_dptx_handle_automated_link_training_test(dptx);
		break;
	case DP_TEST_LINK_VIDEO_PATTERN:
	case DP_TEST_LINK_PHY_TEST_PATTERN:
		dev_err(dptx->dev, "Not available test request '%02x'\n", request);
		break;
	case DP_TEST_LINK_EDID_READ:
		// Handle in rtk_dptx_get_edid().
		dptx->is_autotest = false;
		return;
	default:
		dev_err(dptx->dev, "Invalid test request '%02x'\n", request);
		break;
	}

	status = drm_dp_dpcd_writeb(&dptx->aux, DP_TEST_RESPONSE, response);
	if (status < 0)
		dev_err(dptx->dev, "dptx write test response error\n");
}

static bool rtk_dptx_short_pulse_need_retrain(struct rtk_dptx *dptx)
{
	uint8_t sink_count;
	uint8_t irq_vector;
	int ret = false;

	mutex_lock(&dptx->lock);
	/* DEVICE_SERVICE_IRQ_VECTOR */
	if (drm_dp_dpcd_read(&dptx->aux, DP_DEVICE_SERVICE_IRQ_VECTOR,
						 &irq_vector, 1) < 0) {
		dev_err(dptx->dev, "dptx read service irq vector error\n");
		ret = true;
		goto out;
	}

	if (irq_vector && irq_vector & DP_AUTOMATED_TEST_REQUEST) {
		rtk_dptx_handle_automated_test_request(dptx);
		drm_dp_dpcd_writeb(&dptx->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, irq_vector);
		ret = true;
	}

	/* Just for CTS, DP Compliance Test 4.3.2.4 */
	if (drm_dp_dpcd_read(&dptx->aux, DP_SINK_COUNT, &sink_count, 1) < 0) {
		dev_err(dptx->dev, "dptx read sink count error\n");
		ret = true;
		goto out;
	}

	/* DPCD 202h-207h */
	if (rtk_dptx_get_link_status(dptx) < 0) {
		ret = true;
		goto out;
	}

	/* DP Compliance Test 4.3.2.1, 4.3.2.2, 4.3.2.3 */
	if (!rtk_dptx_is_link_status_ok(dptx)) {
		ret = true;
		goto out;
	}

out:
	mutex_unlock(&dptx->lock);
	return ret;
}
#endif

static void rtk_dptx_update_plugged_status(struct rtk_dptx *dptx)
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

static void rtk_dptx_get_edid(struct rtk_dptx *dptx)
{
	struct edid *edid;
	struct drm_connector *connector = &dptx->connector;

	edid = drm_get_edid(connector, &dptx->aux.ddc);
	if (drm_edid_is_valid(edid)) {
		kfree(dptx->edid);
		dptx->edid = edid;
#ifdef CONFIG_CHROME_PLATFORMS
		/* DP Compliance Test 4.2.2.3 */
		drm_dp_send_real_edid_checksum(&dptx->aux, edid->checksum);
#endif
	} else {
#ifdef CONFIG_CHROME_PLATFORMS
		/* DP Compliance Test 4.2.2.6 */
		if (connector->edid_corrupt)
			drm_dp_send_real_edid_checksum(&dptx->aux, connector->real_edid_checksum);

#endif
		kfree(edid);
	}
}

static void rtk_dptx_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	// struct rtk_dptx *dptx = to_rtk_dptx(encoder);
	int vic;

	vic = drm_match_cea_mode(mode);

	// dptx_set_video_timing(dptx, adj_mode);
	// dptx_set_sst_setting(dptx);
}

#ifdef CONFIG_CHROME_PLATFORMS
static bool rtk_dptx_has_sink_count(struct rtk_dptx *dptx)
{
	struct drm_connector *connector = &dptx->connector;

	return drm_dp_read_sink_count_cap(connector, dptx->rx_cap, &dptx->desc);
}

static int rtk_dptx_get_sink_count(struct rtk_dptx *dptx)
{
	int sink_count = -1;

	if (!rtk_dptx_has_sink_count(dptx))
		return -1;

	sink_count = drm_dp_read_sink_count(&dptx->aux);
	if (sink_count < 0) {
		dev_info(dptx->dev, "dptx read sink count error\n");
		return -1;
	}

	return sink_count;
}

static int rtk_dptx_get_max_link_rate(struct rtk_dptx *dptx)
{
	return min_t(int, drm_dp_max_link_rate(dptx->rx_cap),
				 RTK_DP_MAX_LINK_RATE);
}

static unsigned int rtk_dptx_get_max_lane_count(struct rtk_dptx *dptx)
{
	return min_t(unsigned int, drm_dp_max_lane_count(dptx->rx_cap),
		 min_t(int, dptx->lane_count, RTK_DP_MAX_LANE_COUNT));
}

static int rtk_dptx_change_link_rate_with_display_mode(struct rtk_dptx *dptx,
										 struct drm_display_mode *mode)
{
	int ret = rtk_dptx_get_max_link_rate(dptx);
	int peak_bw = (int) mode->clock * 3 * dptx->bpc / 1000;
	// int bw_1_62 = 1620 * 8 * dptx->lane_count / 10;
	int bw_2_7 = 2700 * 8 * dptx->lane_count / 10;
	int bw_5_4 = 5400 * 8 * dptx->lane_count / 10;
	int bw_8_1 = 8100 * 8 * dptx->lane_count / 10;
	int bw_usage = 0;

#if 0
	/* There's issue with 4 lane 1.62G */
	if (peak_bw <= bw_1_62) {
		ret = DP_LINK_RATE_1_62;
		bw_usage = 100 * peak_bw / bw_1_62;
	} else
#endif
	if (peak_bw <= bw_2_7 && ret >= DP_LINK_RATE_2_7) {
		ret = DP_LINK_RATE_2_7;
		bw_usage = 100 * peak_bw / bw_2_7;
	} else if (peak_bw <= bw_5_4 && ret >= DP_LINK_RATE_5_4) {
		ret = DP_LINK_RATE_5_4;
		bw_usage = 100 * peak_bw / bw_5_4;
	} else if (peak_bw <= bw_8_1 && ret >= DP_LINK_RATE_8_1) {
		ret = DP_LINK_RATE_8_1;
		bw_usage = 100 * peak_bw / bw_8_1;
	}

	dev_info(dptx->dev, "dptx link bw usage: %d %%\n", bw_usage);
	return ret;
}

static int rtk_dptx_reduce_lane_count(struct rtk_dptx *dptx)
{
	if (dptx->lane_count > 1)
		dptx->lane_count = dptx->lane_count / 2;
	else
		return -EINVAL;
	return 0;
}

static int rtk_dptx_reduce_link_rate(struct rtk_dptx *dptx)
{
	int ret = 0;

	switch (dptx->link_rate) {
	case DP_LINK_RATE_1_62:
		ret = rtk_dptx_reduce_lane_count(dptx);
		dptx->link_rate = rtk_dptx_get_max_link_rate(dptx);
		break;
	case DP_LINK_RATE_2_7:
		dptx->link_rate = DP_LINK_RATE_1_62;
		break;
	case DP_LINK_RATE_5_4:
		dptx->link_rate = DP_LINK_RATE_2_7;
		break;
	case DP_LINK_RATE_8_1:
		dptx->link_rate = DP_LINK_RATE_5_4;
		break;
	default:
		return -EINVAL;
	};
	return ret;
}

static int rtk_dptx_fallback_video_format(struct rtk_dptx *dptx)
{
	int ret = 0;

	if (!drm_dp_clock_recovery_ok(dptx->link_status, dptx->lane_count))
		ret = rtk_dptx_reduce_link_rate(dptx);
	else if (!drm_dp_channel_eq_ok(dptx->link_status, dptx->lane_count))
		ret = rtk_dptx_reduce_lane_count(dptx);

	dptx->is_fallback_mode = true;
	return ret;
}

#endif

static int rtk_dptx_read_dpcd(struct rtk_dptx *dptx)
{
	int ret;

#ifdef CONFIG_CHROME_PLATFORMS
	struct drm_dp_aux *aux = &dptx->aux;
	uint8_t downstream_ports[DP_MAX_DOWNSTREAM_PORTS];
	uint8_t tmp;

	ret = drm_dp_read_dpcd_caps(aux, dptx->rx_cap);
	if (ret < 0)
		return ret;

	dptx->link_rate = rtk_dptx_get_max_link_rate(dptx);
	dptx->lane_count = rtk_dptx_get_max_lane_count(dptx);

	/* DP Compliance Test 4.2.2.1 */
	drm_dp_dpcd_read(&dptx->aux, DP_ADAPTER_CAP, &tmp, 1);
	/* DP Compliance Test 4.2.2.8, drm_dp_read_dpcd_caps dosn't read 0x220f */
	drm_dp_dpcd_read(&dptx->aux, DP_DPCD_ADAPTER_CAP, &tmp, 1);
	ret = drm_dp_read_desc(aux, &dptx->desc, drm_dp_is_branch(dptx->rx_cap));
	if (ret < 0) {
		dev_info(dptx->dev, "dptx read desc error\n");
		return ret;
	}

	/* DP Compliance Test 4.2.2.8 */
	rtk_dptx_get_sink_count(dptx);

	ret = drm_dp_read_downstream_info(&dptx->aux, dptx->rx_cap, downstream_ports);
#else
	// Temporarily use this api until Google agrees to use the symbol on STB
	ret = drm_dp_dpcd_read(&dptx->aux, DP_DPCD_REV, dptx->rx_cap,
							 DP_RECEIVER_CAP_SIZE);
#endif
	if (ret < 0) {
		dev_info(dptx->dev, "dptx read dpcd error\n");
		return ret;
	}
	return ret;
}

static int rtk_dptx_get_sink_capability(struct rtk_dptx *dptx)
{
	int ret;

	dev_info(dptx->dev, "dptx: get sink capability\n");

	ret = rtk_dptx_read_dpcd(dptx);
	if (ret < 0) {
		dev_err(dptx->dev, "dptx read dpcd error\n");
		goto out;
	}

	rtk_dptx_get_edid(dptx);

out:
	return 0;
}

static bool rtk_dptx_get_polarity(struct rtk_dptx_port *port)
{
	struct rtk_dptx *dptx;
	struct extcon_dev *edev = port->extcon;
	union extcon_property_value property;
	int ret;
	bool polarity;

	dptx = port->dptx;

	ret = extcon_get_state(edev, EXTCON_DISP_DP);
	if (ret > 0) {
		extcon_get_property(edev, EXTCON_DISP_DP,
				    EXTCON_PROP_USB_TYPEC_POLARITY, &property);
		if (property.intval)
			polarity = 1;
		else
			polarity = 0;
	} else {
		polarity = 0;
	}

	dev_info(dptx->dev, "dptx get polarity (%d)\n", polarity);

	return polarity;
}

static int rtk_dptx_get_port_lanes(struct rtk_dptx_port *port)
{
	struct rtk_dptx *dptx;
	struct extcon_dev *edev = port->extcon;
	union extcon_property_value property;
	int ret;
	u8 lanes;

	dptx = port->dptx;

	ret = extcon_get_state(edev, EXTCON_DISP_DP);
	if (ret > 0) {
		extcon_get_property(edev, EXTCON_DISP_DP,
				    EXTCON_PROP_USB_SS, &property);
		if (property.intval)
			lanes = 2;
		else
			lanes = 4;
	} else
		lanes = 0;

	dev_info(dptx->dev, "dptx get %d lanes\n", lanes);

	return lanes;
}

static struct rtk_dptx_port *rtk_dptx_connected_port(struct rtk_dptx *dptx)
{
	struct rtk_dptx_port *port;
	int i, lanes;

	for (i = 0; i < dptx->ports; i++) {
		port = dptx->port[i];
		lanes = rtk_dptx_get_port_lanes(port);
		if (lanes)
			return port;
	}

	return NULL;
}

static void rtk_dptx_reset_hw(struct rtk_dptx *dptx)
{
	dev_info(dptx->dev, "dptx: reset hw\n");

	// reset_control_deassert(dptx->rstc_misc);
	clk_prepare_enable(dptx->clk_dptx);
	clk_prepare_enable(dptx->clk_usb_p4);
}

static void rtk_dptx_write_dpcd_lane_set(struct rtk_dptx *dptx,
						 struct rtk_dptx_train_signal signals[4])
{
	uint8_t val[4] = {};
	int i;

	for (i = 0; i < dptx->lane_count; i++) {
		val[i] = signals[i].swing << DP_TRAIN_VOLTAGE_SWING_SHIFT |
		      signals[i].emphasis << DP_TRAIN_PRE_EMPHASIS_SHIFT;
		if (signals[i].swing == RTK_DP_MAX_SWING)
			val[i] |= DP_TRAIN_MAX_SWING_REACHED;
		if (signals[i].emphasis == RTK_DP_MAX_EMPHASIS)
			val[i] |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
	}
	drm_dp_dpcd_write(&dptx->aux, DP_TRAINING_LANE0_SET, val, dptx->lane_count);
}

static int rtk_dptx_signal_setting(struct rtk_dptx *dptx,
						 struct rtk_dptx_train_signal signals[4])
{
	int ret;
	int i;

	for (i = 0; i < RTK_DP_MAX_LANE_COUNT; i++) {
		if (signals[i].swing + signals[i].emphasis > 3) {
			dev_err(dptx->dev, "[%s] lane: %d invalid swing: %u, emp: %u\n",
				__func__, i, signals[i].swing, signals[i].emphasis);
			return -EINVAL;
		}
	}

	ret = rtk_dptx_mac_signal_setting(dptx, signals);
	if (ret)
		return ret;
	ret = rtk_dptx_aphy_signal_setting(dptx, signals);
	if (ret)
		return ret;

	rtk_dptx_write_dpcd_lane_set(dptx, signals);

	return 0;
}

/* tmp copy from drm dp helper, remove after has symbol */
static u8 rtk_dp_link_status(const u8 link_status[DP_LINK_STATUS_SIZE], int r)
{
	return link_status[r - DP_LANE0_1_STATUS];
}

static u8 rtk_drm_dp_get_adjust_request_pre_emphasis(const u8 link_status[DP_LINK_STATUS_SIZE],
					  int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ?
		 DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT :
		 DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT);
	u8 l = rtk_dp_link_status(link_status, i);

	return ((l >> s) & 0x3);
}

static u8 rtk_drm_dp_get_adjust_request_voltage(const u8 link_status[DP_LINK_STATUS_SIZE],
				     int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ?
		 DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT :
		 DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT);
	u8 l = rtk_dp_link_status(link_status, i);

	return ((l >> s) & 0x3) << DP_TRAIN_VOLTAGE_SWING_SHIFT;
}

static u8 rtk_dp_get_lane_status(const u8 link_status[DP_LINK_STATUS_SIZE],
			     int lane)
{
	int i = DP_LANE0_1_STATUS + (lane >> 1);
	int s = (lane & 1) * 4;
	u8 l = rtk_dp_link_status(link_status, i);

	return (l >> s) & 0xf;
}

static bool rtk_drm_dp_clock_recovery_ok(const u8 link_status[DP_LINK_STATUS_SIZE],
			      int lane_count)
{
	int lane;
	u8 lane_status;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = rtk_dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_LANE_CR_DONE) == 0)
			return false;
	}
	return true;
}

static bool rtk_drm_dp_channel_eq_ok(const u8 link_status[DP_LINK_STATUS_SIZE],
			  int lane_count)
{
	u8 lane_align;
	u8 lane_status;
	int lane;

	lane_align = rtk_dp_link_status(link_status,
				    DP_LANE_ALIGN_STATUS_UPDATED);
	if ((lane_align & DP_INTERLANE_ALIGN_DONE) == 0)
		return false;
	for (lane = 0; lane < lane_count; lane++) {
		lane_status = rtk_dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_CHANNEL_EQ_BITS) != DP_CHANNEL_EQ_BITS)
			return false;
	}
	return true;
}
/* ---------------------- */

static void rtk_dptx_get_signals(uint8_t status[DP_LINK_STATUS_SIZE],
							 struct rtk_dptx_train_signal signals[4])
{
	int i;

	for (i = 0; i < 4; i++) {
		signals[i].swing = rtk_drm_dp_get_adjust_request_voltage(status, i);
		signals[i].emphasis = rtk_drm_dp_get_adjust_request_pre_emphasis(status, i);
	}
}

static int rtk_dptx_train_cr(struct rtk_dptx *dptx,
						 struct rtk_dptx_train_signal signals[4])
{
	int i;
	uint8_t status[DP_LINK_STATUS_SIZE];
	uint8_t prev_swing = 0;
	uint8_t prev_emphasis = 0;

	dev_info(dptx->dev, "[%s] start train cr\n", __func__);
	rtk_dptx_phy_set_scramble(dptx, false);
	rtk_dptx_phy_set_pattern(dptx, RTK_PATTERN_1);
	drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
					 DP_TRAINING_PATTERN_1 | DP_LINK_SCRAMBLING_DISABLE);

	rtk_dptx_write_dpcd_lane_set(dptx, signals);

	/*
	 * Condition of CR fail:
	 * 1. Failed to pass CR using the same voltage
	 *    level over five times.
	 * 2. Failed to pass CR when the current voltage
	 *    level is the same with previous voltage
	 *    level and reach max voltage level (3).
	 */
	for (i = 0; i < 4; i++) {
		//8b10b training rx aux rd interval
		usleep_range(100, 200); // LANEx_CR_DONE (Minimum)

		drm_dp_dpcd_read(&dptx->aux, DP_LANE0_1_STATUS,
						 status, DP_LINK_STATUS_SIZE);
		if (rtk_drm_dp_clock_recovery_ok(status, dptx->lane_count)) {
			dev_dbg(dptx->dev, "Link train CR pass\n");
			return 0;
		}

		rtk_dptx_get_signals(status, signals);
		if (prev_swing == RTK_DP_MAX_SWING && prev_swing == signals[0].swing)
			goto out;
		if (prev_emphasis == RTK_DP_MAX_EMPHASIS && prev_emphasis == signals[0].emphasis)
			goto out;
		prev_swing = signals[0].swing;
		prev_emphasis = signals[0].emphasis;

		rtk_dptx_signal_setting(dptx, signals);
	}

out:
	dev_err(dptx->dev, "Link train CR fail!\n");
	dev_err(dptx->dev, "[%s] LANE0_1_STATUS(202) 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		 __func__, status[0], status[1], status[2],
		 status[3], status[4], status[5]);
	return -1;
}

static int rtk_dptx_train_eq(struct rtk_dptx *dptx,
						 struct rtk_dptx_train_signal signals[4])
{
	int i;
	uint8_t status[DP_LINK_STATUS_SIZE];
	uint8_t aux_rd_interval = (dptx->rx_cap[14] & 0x7f) * 4;
	uint8_t eq_pattern =
		(drm_dp_tps4_supported(dptx->rx_cap)) ? DP_TRAINING_PATTERN_4 :
		(drm_dp_tps3_supported(dptx->rx_cap)) ? DP_TRAINING_PATTERN_3 :
		DP_TRAINING_PATTERN_2;

	dev_info(dptx->dev, "[%s] start train eq, pattern: %u\n", __func__, eq_pattern);
	if (eq_pattern == DP_TRAINING_PATTERN_4) {
		rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_COMPLIANCE_EYE_PATTERN,
			DPTX14_MAC_IP_COMPLIANCE_EYE_PATTERN_eye_pat_sel_mask,
			DPTX14_MAC_IP_COMPLIANCE_EYE_PATTERN_eye_pat_sel(0x1)); // should be 0x2?
		rtk_dptx_phy_set_scramble(dptx, true);
		rtk_dptx_phy_set_pattern(dptx, RTK_PATTERN_4);
		drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
						 DP_TRAINING_PATTERN_4);
	} else {
		rtk_dptx_phy_set_pattern(dptx, eq_pattern);
		drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
					 eq_pattern | DP_LINK_SCRAMBLING_DISABLE);
	}

	/*
	 * Condition of EQ fail:
	 * 1. Failed to pass EQ over six times.
	 */
	for (i = 0; i < 6; i++) {
		drm_dp_dpcd_read(&dptx->aux, DP_LANE0_1_STATUS,
						 status, DP_LINK_STATUS_SIZE);
		rtk_dptx_get_signals(status, signals);
		rtk_dptx_signal_setting(dptx, signals);

		msleep_interruptible(aux_rd_interval);

		drm_dp_dpcd_read(&dptx->aux, DP_LANE0_1_STATUS,
						 status, DP_LINK_STATUS_SIZE);
		if (rtk_drm_dp_channel_eq_ok(status, dptx->lane_count)) {
			dev_dbg(dptx->dev, "Link train EQ pass\n");
			return 0;
		}
	}

	dev_err(dptx->dev, "Link train EQ fail!\n");
	dev_err(dptx->dev, "[%s] LANE0_1_STATUS(202) 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		 __func__, status[0], status[1], status[2],
		 status[3], status[4], status[5]);
	return -1;
}

static int rtk_dptx_link_training(struct rtk_dptx *dptx)
{
	int ret;
	struct rtk_dptx_train_signal signals[4] = {};

	dev_info(dptx->dev, "[%s] start training\n", __func__);

	rtk_dptx_mac_signal_setting(dptx, signals);
	rtk_dptx_aphy_signal_setting(dptx, signals);

	/* Spec says link_bw = link_rate / 0.27Gbps */
	drm_dp_dpcd_writeb(&dptx->aux, DP_SET_POWER, DP_SET_POWER_D0);
	drm_dp_dpcd_writeb(&dptx->aux, DP_LINK_BW_SET, dptx->link_rate / 27000);
	drm_dp_dpcd_writeb(&dptx->aux, DP_LANE_COUNT_SET,
					 dptx->lane_count | DP_LANE_COUNT_ENHANCED_FRAME_EN);
	drm_dp_dpcd_writeb(&dptx->aux, DP_MAIN_LINK_CHANNEL_CODING_SET,
					 DP_SET_ANSI_8B10B);

	ret = rtk_dptx_train_cr(dptx, signals);
	if (ret)
		goto out;
	ret = rtk_dptx_train_eq(dptx, signals);
	if (ret)
		goto out;

out:
	if (ret) {
		dev_err(dptx->dev, "[%s] link training fail!\n", __func__);
		return ret;
	}
	return 0;
}

static void rtk_dptx_config_vo(struct rtk_dptx *dptx, struct drm_display_mode *mode)
{
	struct rtk_rpc_info *rpc_info = (dptx->rpc_info_vo == NULL) ?
									 dptx->rpc_info : dptx->rpc_info_vo;
	struct rpc_set_display_out_interface interface;
	int ret;
	int mixer;

#ifdef CONFIG_CHROME_PLATFORMS
	mixer = DISPLAY_INTERFACE_MIXER1;
#else
	mixer = DISPLAY_INTERFACE_MIXER3;
#endif

	interface.display_interface       = DISPLAY_INTERFACE_DP;
	interface.width                   = mode->hdisplay;
	interface.height                  = mode->vdisplay;
	interface.frame_rate              = drm_mode_vrefresh(mode);
	interface.display_interface_mixer = mixer;

	DRM_INFO("[rtk_dptx_enc_enable] enable %s on %s (%dx%d@%d)\n",
		interface_names[interface.display_interface], mixer_names[interface.display_interface_mixer],
		interface.width, interface.height, interface.frame_rate);

	ret = rpc_set_out_interface(rpc_info, &interface);
	if (ret)
		DRM_ERROR("rpc_set_out_interface rpc fail\n");
}

static int rtk_dptx_init_hw(struct rtk_dptx *dptx, struct drm_display_mode *mode)
{
	struct rtk_dptx_port *port;
	int ret, i;
	bool polarity;

	for (i = 0; i < dptx->ports; i++) {
		port = dptx->port[i];
		polarity = rtk_dptx_get_polarity(port);
	}

	// need check:
	// 1. reset usb3_p2_mdio
	dev_info(dptx->dev, "[%s] lane(%u) rate(%d) pclk(%d)\n", __func__,
				 dptx->lane_count, dptx->link_rate, mode->clock);

	rtk_dptx_reset_hw(dptx);
	rtk_dptx_phy_config_aphy(dptx);
	ret = rtk_dptx_phy_dppll_setting(dptx, mode, polarity);
	if (ret)
		return ret;
	rtk_dptx_phy_config_lane(dptx);
	rtk_dptx_phy_config_video_timing(dptx, mode, polarity);
	ret = rtk_dptx_link_training(dptx);
	if (ret) {
		// will vblank panic if return here
		dev_err(dptx->dev, "[%s] link train fail!\n", __func__);
	}
	// rtk_dptx_phy_config_audio(dptx);
	rtk_dptx_config_vo(dptx, mode);
	rtk_dptx_phy_start_video(dptx, mode);


	dev_info(dptx->dev, "[%s] hw setting is good!\n", __func__);
	return ret;
}

static int rtk_dptx_enable(struct rtk_dptx *dptx)
{
	struct rtk_dptx_port *port;
	int lanes, i;
	bool polarity;

	if (dptx->active) {
		dev_info(dptx->dev, "rtk dptx already actived\n");
		return 0;
	}

	rtk_dptx_init(dptx);

	for (i = 0; i < dptx->ports; i++) {
		port = dptx->port[i];
		polarity = rtk_dptx_get_polarity(port);
		if (polarity)
			rtk_dptx14_update(dptx, DPTX14_IP_AUX_DIG_PHY2, AUX_PN_SWAP, AUX_PN_SWAP);
		else
			rtk_dptx14_update(dptx, DPTX14_IP_AUX_DIG_PHY2, AUX_PN_SWAP, 0);

		lanes = rtk_dptx_get_port_lanes(port);
		if (lanes) {
			dptx->lane_count = lanes;
			rtk_dptx_get_sink_capability(dptx);
		}
	}

	dptx->active = true;
	dptx->is_autotest = false;
	dptx->is_fallback_mode = false;

	return 0;
}

static void rtk_dptx_enc_enable(struct drm_encoder *encoder)
{
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	struct rtk_dptx *dptx = to_rtk_dptx(encoder);
	int ret;

	if (!dptx->connected)
		dev_err(dptx->dev, "dptx: not connected, force enable!");

	if (!dptx->active) {
		dev_info(dptx->dev, "dptx: enable dptx");
		rtk_dptx_enable(dptx);
	}

	dev_info(dptx->dev, "dptx: enc enable");
	mutex_lock(&dptx->lock);

	ensure_clock_enabled(dptx);

	drm_dp_dpcd_writeb(&dptx->aux, DP_SET_POWER, DP_SET_POWER_D0);
	usleep_range(2000, 5000);

	dptx->bpc = rtk_dptx_get_bpc(dptx);
	dptx->color_format = RTK_COLOR_FORMAT_RGB;
#ifdef CONFIG_CHROME_PLATFORMS
	if (!dptx->is_autotest && !dptx->is_fallback_mode)
		dptx->link_rate = rtk_dptx_change_link_rate_with_display_mode(dptx, mode);
#endif

	ret = rtk_dptx_init_hw(dptx, mode);
	if (ret)
		DRM_ERROR("dptx hw setting fail\n");

#ifdef CONFIG_CHROME_PLATFORMS
	ret = rtk_dptx_get_link_status(dptx);
	if (ret)
		goto unlock;
	if (rtk_dptx_is_link_status_ok(dptx))
		goto out;

	// Some RX needs to disable training pattern first to get correct status.
	drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);

	ret = rtk_dptx_get_link_status(dptx);
	if (ret)
		goto unlock;
	if (rtk_dptx_is_link_status_ok(dptx))
		goto unlock;

	/* if training fail, start retrain */
	DRM_ERROR("dptx link status fail!\n");
	ret = rtk_dptx_fallback_video_format(dptx);
	if (ret) {
		DRM_ERROR("dptx fallback rate/lane fail!\n");
		goto out;
	}

	schedule_work(&dptx->retrain_link_work);
out:
	drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
unlock:
#else
	drm_dp_dpcd_writeb(&dptx->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
#endif
	mutex_unlock(&dptx->lock);

}

static void rtk_dptx_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_dptx *dptx = to_rtk_dptx(encoder);

	dev_info(dptx->dev, "dptx: enc disable\n");

	rtk_dptx_poweroff(dptx);
}

static void rtk_car_dptx_enc_enable(struct drm_encoder *encoder)
{
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	struct rtk_dptx *dptx = to_rtk_dptx(encoder);
	struct rtk_rpc_info *rpc_info = dptx->rpc_info;
	struct rtk_dptx_port *port;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	int ret = 0, i;
	bool polarity;
	int lanes;

	dev_info(dptx->dev, "dptx: car enc enable");

	for (i = 0; i < dptx->ports; i++) {
		port = dptx->port[i];
		polarity = rtk_dptx_get_polarity(port);
		lanes = rtk_dptx_get_port_lanes(port);
		dptx->lane_count = lanes;
	}

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
	hw_init_rpc.link_rate    = dptx->link_rate;
	hw_init_rpc.lane_count   = dptx->lane_count;
	hw_init_rpc.bpc          = dptx->bpc;
	hw_init_rpc.is_flipped   = polarity;

	DRM_INFO("[%s] enable interface %s\n", __func__,
		interface_names[hw_init_rpc.display_interface]);

	ret = rpc_hw_init_out_interface(rpc_info, &hw_init_rpc);
	if (ret)
		DRM_ERROR("rpc_hw_init_out_interface rpc fail\n");
}

static void rtk_car_dptx_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_dptx *dptx = to_rtk_dptx(encoder);
	struct rtk_rpc_info *rpc_info = dptx->rpc_info;
	struct rpc_hw_init_display_out_interface hw_init_rpc;
	int ret = 0;

	dev_info(dptx->dev, "dptx: car enc disable\n");

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

static const struct drm_encoder_helper_funcs rtk_car_dptx_encoder_helper_funcs = {
	.mode_fixup = rtk_dptx_enc_mode_fixup,
	.mode_set   = rtk_dptx_enc_mode_set,
	.enable     = rtk_car_dptx_enc_enable,
	.disable    = rtk_car_dptx_enc_disable,
	.atomic_check = rtk_dptx_enc_atomic_check,
};

static const struct drm_encoder_helper_funcs rtk_dptx_encoder_helper_funcs = {
	.mode_fixup = rtk_dptx_enc_mode_fixup,
	.mode_set   = rtk_dptx_enc_mode_set,
	.enable     = rtk_dptx_enc_enable,
	.disable    = rtk_dptx_enc_disable,
	.atomic_check = rtk_dptx_enc_atomic_check,
};

/* HPD */
static int rtk_dptx_handle_short_pulse(struct rtk_dptx *dptx)
{
#ifdef CONFIG_CHROME_PLATFORMS
	if (rtk_dptx_short_pulse_need_retrain(dptx)) {
		dev_info(dptx->dev, "dptx start retraining\n");
		rtk_dptx_set_bad_connector_link_status(dptx);
		rtk_dptx_get_edid(dptx); // edid might change while running CTS
	}
#endif
	drm_kms_helper_hotplug_event(dptx->drm_dev);

	rtk_dptx_update_plugged_status(dptx);

	return 0;
}

static int rtk_dptx_handle_long_pulse(struct rtk_dptx *dptx)
{
	dev_info(dptx->dev, "dptx %s\n", dptx->connected ? "connect" : "disconnect");
	if (dptx->connected)
		rtk_dptx_enable(dptx);
	else
		dptx->active = false;

	rtk_dptx_set_bad_connector_link_status(dptx);
	drm_kms_helper_hotplug_event(dptx->drm_dev);

	rtk_dptx_update_plugged_status(dptx);

	return 0;
}

/* Detect HPD with gpio */
static void rtk_dptx_hpd_gpio_worker(struct work_struct *work)
{
	struct delayed_work *hpd_gpio_work = to_delayed_work(work);
	struct rtk_dptx *dptx = to_rtk_dptx(hpd_gpio_work);
	bool prev_connected = dptx->connected;
	int val;

	val = gpiod_get_value(dptx->hpd_gpio);
	if (val < 0)
		dev_err(dptx->dev, "failed to get hpd_gpio val");

	mutex_lock(&dptx->lock);
	dptx->connected = (val) ? true : false;
	dev_info(dptx->dev, "[%s] %s\n", __func__, (val) ? "connect" : "disconnect");
	mutex_unlock(&dptx->lock);

	// short pulse, gpio will be high after 0.5 ~ 1ms
	if (prev_connected == true && dptx->connected == true) {
		rtk_dptx_handle_short_pulse(dptx);
		return;
	}

	rtk_dptx_handle_long_pulse(dptx);
}

static int rtk_dptx_get_gpio_hpd_status(struct rtk_dptx *dptx)
{
	if (!dptx->hpd_gpio)
		return -EINVAL;

	schedule_delayed_work(&dptx->hpd_gpio_work,
		 msecs_to_jiffies(RTK_HPD_SHORT_PULSE_THRESHOLD_MS));

	return 0;
}

static int rtk_dptx_detect_gpio_hpd(struct rtk_dptx *dptx)
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

static irqreturn_t rtk_dptx_hpd_irq(int irq, void *dev_id)
{
	struct rtk_dptx *dptx = dev_id;

	int val = 0;

	dev_info(dptx->dev, "dptx: hpd irq\n");
	val = rtk_dptx_detect_gpio_hpd(dptx);

	return IRQ_HANDLED;
}

/* Detect HPD with dptx_hpd */
static int poll_hpd(struct rtk_dptx *dptx)
{
	unsigned int val = 0;
	bool old_conn_state = dptx->connected;
	bool is_short_pulse;
	bool is_long_pulse;

	mutex_lock(&dptx->lock);

	val = rtk_dptx14_read(dptx, DPTX14_IP_HPD_CTRL);
	dptx->connected = (bool) (val & HPD_CTRL_DEB);
	val = rtk_dptx14_read(dptx, DPTX14_IP_HPD_IRQ);
	is_short_pulse = (bool) (val & HPD_IRQ_SHPD);
	is_long_pulse = (bool) (val & HPD_IRQ_UHPD || val & HPD_IRQ_LHPD);
	rtk_dptx14_write(dptx, DPTX14_IP_HPD_IRQ, HPD_IRQ_ALL);

	mutex_unlock(&dptx->lock);


	if (is_long_pulse) {
		dev_info(dptx->dev, "dptx unplug or long pulse\n");
		rtk_dptx_set_bad_connector_link_status(dptx);
	} else if (is_short_pulse) {
		dev_info(dptx->dev, "dptx short pulse\n");
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
	struct rtk_dptx *dptx = (struct rtk_dptx *) data;

	// Enable HPD interrupt
	rtk_dptx14_update(dptx, DPTX14_IP_HPD_CTRL, HPD_CTRL_EN, HPD_CTRL_EN);
	rtk_dptx14_write(dptx, DPTX14_IP_HPD_IRQ_EN, HPD_IRQ_EN_ALL);

	while (!kthread_should_stop()) {
		poll_hpd(dptx);
		msleep_interruptible(RTK_POLL_HPD_INTERVAL_MS);
	}

	return 0;
}

static int rtk_dptx_start_hpd_thread(struct rtk_dptx *dptx)
{
	dev_info(dptx->dev, "dptx: start hpd thread\n");

	ensure_clock_enabled(dptx);

	if (dptx->hpd_thread) {
		dev_info(dptx->dev, "hpd_thread already exsist\n");
		return 0;
	}

	rtk_dptx14_write(dptx, DPTX14_IP_HPD_IRQ, HPD_IRQ_ALL);
	dptx->hpd_thread = kthread_run(rtk_dptx_hpd_thread, dptx, "dptx_hpd_thread");
	if (IS_ERR(dptx->hpd_thread)) {
		dev_err(dptx->dev, "Failed to create kernel thread\n");
		dptx->hpd_thread = NULL;
		return PTR_ERR(dptx->hpd_thread);
	}

	return 0;
}

static int rtk_dptx_stop_hpd_thread(struct rtk_dptx *dptx)
{
	dev_info(dptx->dev, "dptx: stop hpd thread\n");

	if (dptx->hpd_thread) {
		kthread_stop(dptx->hpd_thread);
		dptx->hpd_thread = NULL;
	}

	dptx->connected = false;

	return 0;
}

/* HPD */
static void rtk_dptx_start_detect_hpd(struct rtk_dptx *dptx)
{
	int ret;

	dev_info(dptx->dev, "dptx: start detect hpd\n");
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
					"dptx_hpd_irq", dptx);
		if (ret) {
			dev_err(dptx->dev, "can't request hpd gpio irq\n");
			return;
		}
	} else {
		rtk_dptx_start_hpd_thread(dptx);
	}
}

static void rtk_dptx_stop_detect_hpd(struct rtk_dptx *dptx)
{
	dev_info(dptx->dev, "dptx: stop detect hpd\n");
	if (dptx->hpd_gpio) {
		if (dptx->hpd_irq) {
			free_irq(dptx->hpd_irq, dptx);
			dptx->hpd_irq = 0;
		}
	} else {
		rtk_dptx_stop_hpd_thread(dptx);
	}
}

static enum drm_connector_status rtk_dptx_conn_detect
(struct drm_connector *connector, bool force)
{
	struct rtk_dptx *dptx = to_rtk_dptx(connector);
	enum drm_connector_status status = connector_status_disconnected;
	struct rtk_drm_private *priv = dptx->drm_dev->dev_private;

	if (dptx->check_connector_limit &&
		 !rtk_drm_can_add_connector(priv, RTK_CONNECTOR_DP)) {
		dev_info(dptx->dev, "block dptx because the limit is reached\n");
		return connector_status_disconnected;
	}

#ifdef CONFIG_CHROME_PLATFORMS
	/* DP Compliance Test 4.2.2.7 */
	if (rtk_dptx_get_sink_count(dptx) == 0) {
		dev_info(dptx->dev, "dptx sink count is 0\n");
		return connector_status_disconnected;
	}
#endif
	mutex_lock(&dptx->lock);
	if (dptx->connected) {
		dev_info(dptx->dev, "rtk dptx connected\n");
		status = connector_status_connected;
	}
	mutex_unlock(&dptx->lock);

	if (dptx->check_connector_limit)
		rtk_drm_update_connector(priv, RTK_CONNECTOR_DP, dptx->connected);

	return status;
}

static void rtk_dptx_conn_destroy(struct drm_connector *connector)
{
	DRM_INFO("dptx: conn destroy\n");

	drm_connector_cleanup(connector);
}

static bool check_use_default_mode(struct rtk_dptx *dptx)
{
	const struct drm_display_mode *m = &default_mode;
	struct drm_display_mode *mode;
	struct drm_connector *connector;
	struct device_node *node;

	dev_info(dptx->dev, "dptx: check use default mode\n");

	connector = &dptx->connector;

	node = of_parse_phandle(dptx->dev->of_node, "extcon", 0);
	if (node == NULL) {
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

		dev_info(dptx->dev, "use default mode (%dx%d)\n",
			m->hdisplay, m->vdisplay);

		dptx->link_rate = DP_LINK_RATE_2_7;
		dptx->lane_count = 2;
		dptx->bpc = 8;

		return 1;
	}

	return 0;
}

static int rtk_dptx_get_modes(struct rtk_dptx *dptx)
{
	struct edid *edid;
	int num_modes = 0;

	if (check_use_default_mode(dptx))
		return 1;

	mutex_lock(&dptx->lock);
	edid = dptx->edid;
	if (edid) {
		dptx->sink_has_audio = drm_detect_monitor_audio(edid);
		drm_connector_update_edid_property(&dptx->connector, edid);
		num_modes += drm_add_edid_modes(&dptx->connector, edid);

		dev_info(dptx->dev, "dptx get edid, num_modes (%d), %s audio\n",
			num_modes, dptx->sink_has_audio ? "Has" : "No");
	} else {
		dev_err(dptx->dev, "dptx no edid!\n");
	}
	mutex_unlock(&dptx->lock);

	return num_modes;
}

static int rtk_car_dptx_get_modes(struct rtk_dptx *dptx)
{
	struct drm_connector *connector;
	struct rtk_rpc_info *rpc_info = dptx->rpc_info;
	struct drm_display_mode *mode;
	struct drm_display_mode disp_mode;
	struct rpc_query_display_out_interface_timing timing;

	connector = &dptx->connector;
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
	struct rtk_dptx *dptx = to_rtk_dptx(connector);
	int num_modes = 0;

	if (dptx->dptx_data->get_modes != NULL) {
		num_modes = dptx->dptx_data->get_modes(dptx);
		if (num_modes < 0)
			dev_err(dptx->dev, "dptx get modes fail\n");
	}

	return num_modes;
}

static enum drm_mode_status rtk_dptx_conn_mode_valid
(struct drm_connector *connector, struct drm_display_mode *mode)
{
	struct rtk_dptx *dptx = to_rtk_dptx(connector);
	u8 vic;
	int max_rate;
	int bpc;

#ifdef CONFIG_CHROME_PLATFORMS
	if (dptx->is_fallback_mode || dptx->is_autotest) {
		max_rate = dptx->link_rate * (int) dptx->lane_count;
	} else {
		max_rate = rtk_dptx_get_max_link_rate(dptx) *
				 rtk_dptx_get_max_lane_count(dptx);
	}
#else
	max_rate = dptx->link_rate * (int) dptx->lane_count;
#endif
	bpc = rtk_dptx_get_bpc(dptx);

	if (!dptx->connected)
		return MODE_BAD;

	if (dptx->dptx_data->type == RTK_AUTOMOTIVE_TYPE)
		return MODE_OK;

	vic = drm_match_cea_mode(mode);

	if (vic >= VIC_3840X2160P24) {
		dev_err(dptx->dev, "dptx mode clock high (%d) >= (%d)\n",
			vic, VIC_3840X2160P24);
		return MODE_CLOCK_HIGH;
	}

	if (mode->clock > dptx->max_clock_k)
		return MODE_CLOCK_HIGH;

	/* efficiency is about 0.8 */
	if (max_rate < mode->clock * 3 * bpc / 8) {
		dev_err(dptx->dev, "dptx mode clock high (%d) < (%d)\n",
			max_rate, mode->clock * 3 * bpc / 8);
		return MODE_CLOCK_HIGH;
	}

	return MODE_OK;
}

static const struct drm_connector_funcs rtk_dptx_connector_funcs = {
//	.dpms = drm_atomic_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = rtk_dptx_conn_detect,
	.destroy = rtk_dptx_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs rtk_dptx_connector_helper_funcs = {
	.get_modes = rtk_dptx_conn_get_modes,
	.mode_valid = rtk_dptx_conn_mode_valid,
};

int dptx_aux_isr(struct rtk_dptx *dptx)
{
	unsigned int val;
	int ret = 0;
	int i;

	for (i = 0; i < RTK_DP_AUX_WAIT_REPLY_COUNT; i++) {
		val = rtk_dptx14_read(dptx, DPTX14_IP_AUX_IRQ_EVENT);
		if (val & AUXDONE)
			break;
		mdelay(1);
	}

	if (val & RXERROR) {
		dev_dbg(dptx->dev, "dptx aux error\n");
		ret = -1;
	} else if (val & AUXDONE) {
		dev_dbg(dptx->dev, "dptx aux done\n");
		ret = 0;
	} else {
		dev_err(dptx->dev, "dptx aux not done, IRQ_EVENT: 0x%x\n", val);
		ret = -1;
		// Enable timeout and error retry after first aux transfer error.
		rtk_dptx14_update(dptx, DPTX14_IP_AUX_RETRY_2,
				 RETRY_TIMEOUT_EN | RETRY_ERROR_EN,
				 RETRY_TIMEOUT_EN | RETRY_ERROR_EN);
		rtk_dptx14_update(dptx, DPTX14_IP_AUX_TIMEOUT,
				 AUX_TIMEOUT_EN, AUX_TIMEOUT_EN);
	}

	if (val & NACK)
		dev_err(dptx->dev, "dptx aux NACK\n");


	val = rtk_dptx14_read(dptx, DPTX14_IP_AUX_RETRY_1);
	if (val & RETRY_LOCK) {
		// unlock retry lock
		dev_err(dptx->dev, "dptx aux is lock\n");
		rtk_dptx14_update(dptx, DPTX14_IP_AUX_RETRY_2, RETRY_EN, 0);
		rtk_dptx14_update(dptx, DPTX14_IP_AUX_RETRY_2, RETRY_EN, RETRY_EN);
	}

	rtk_dptx14_write(dptx, DPTX14_IP_AUX_IRQ_EVENT, AUX_ALL_IRQ);

	return ret;
}

static irqreturn_t rtk_dptx_aux_irq(int irq, void *dev_id)
{
	struct rtk_dptx *dptx = dev_id;
	// int ret;

	dev_info(dptx->dev, "[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	// ret = dptx_aux_isr(dptx);
	//	if (!ret)
	//	up(&dptx->sem);

	return IRQ_HANDLED;
}

static int dptx_aux_get_data(struct rtk_dptx *dptx, struct drm_dp_aux_msg *msg)
{
	u8 *buffer = msg->buffer;
	int i;
	int size = 0;
	unsigned int rd_ptr, wr_ptr;

	rd_ptr = rtk_dptx14_read(dptx, DPTX14_IP_AUX_FIFO_RD_PTR);
	wr_ptr = rtk_dptx14_read(dptx, DPTX14_IP_AUX_FIFO_WR_PTR);
	size = wr_ptr - rd_ptr;

	if (size > msg->size) {
		dev_err(dptx->dev, "wrong size: fifo(%u), msg(%zu)\n", size, msg->size);
		size = msg->size;
	}

	for (i = 0; i < size; i++)
		buffer[i] = rtk_dptx14_read(dptx, DPTX14_IP_AUX_REPLY_DATA);

	return size;
}

static void dptx_aux_transfer(struct rtk_dptx *dptx, struct drm_dp_aux_msg *msg)
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
		rtk_dptx14_write(dptx, DPTX14_IP_AUX_FIFO_CTRL,
			 READ_FAIL_AUTO_EN | AUX_FIFO_CTRL_RESET);
	else
		rtk_dptx14_write(dptx, DPTX14_IP_AUX_FIFO_CTRL, AUX_FIFO_CTRL_RESET);

	rtk_dptx14_write(dptx, DPTX14_IP_AUX_IRQ_EVENT, AUX_ALL_IRQ);
	rtk_dptx14_write(dptx, DPTX14_IP_AUXTX_REQ_CMD, (addr >> 16) & 0xFF);
	rtk_dptx14_write(dptx, DPTX14_IP_AUXTX_REQ_ADDR_M, (addr >> 8) & 0xFF);
	rtk_dptx14_write(dptx, DPTX14_IP_AUXTX_REQ_ADDR_L, addr & 0xFF);
	rtk_dptx14_write(dptx, DPTX14_IP_AUXTX_REQ_LEN, (size > 0) ? (size - 1) : 0);

	if (!(msg->request & DP_AUX_I2C_READ)) {
		for (i = 0; i < size; i++)
			rtk_dptx14_write(dptx, DPTX14_IP_AUXTX_REQ_DATA, buffer[i]);
	}

	if (msg->size == 0)
		rtk_dptx14_update(dptx, DPTX14_IP_AUXTX_TRAN_CTRL,
			 TX_ADDRONLY | TX_START, TX_ADDRONLY | TX_START);
	else
		rtk_dptx14_update(dptx, DPTX14_IP_AUXTX_TRAN_CTRL,
			 TX_ADDRONLY | TX_START, TX_START);

}

static ssize_t rtk_dptx_aux_transfer(struct drm_dp_aux *aux,
				     struct drm_dp_aux_msg *msg)
{
	struct rtk_dptx *dptx = to_rtk_dptx(aux);
	int ret;

	if (!dptx->connected) {
		dev_dbg(dptx->dev, "dptx disconnected, do not do aux transfer\n");
		msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
		return -ENODEV;
	}

	if (WARN_ON(msg->size > 16)) {
		DRM_ERROR("dptx aux msg size %ld too big\n", msg->size);
		return -E2BIG;
	}

	ensure_clock_enabled(dptx);

	dptx_aux_transfer(dptx, msg);

	ret = dptx_aux_isr(dptx);
	if (ret < 0) {
		dev_err(dptx->dev, "aux transfer error\n");
		return -ETIMEDOUT;
	}

	if ((msg->request & DP_AUX_I2C_READ) && msg->size != 0)
		ret = dptx_aux_get_data(dptx, msg);
	else
		ret = msg->size;

	rtk_dptx14_update(dptx, DPTX14_IP_AUX_FIFO_CTRL,
		 AUX_FIFO_CTRL_ALL, AUX_FIFO_CTRL_RESET);

	if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_WRITE ||
	    (msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_READ)
		msg->reply = DP_AUX_I2C_REPLY_ACK;
	else if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_WRITE ||
		 (msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_READ)
		msg->reply = DP_AUX_NATIVE_REPLY_ACK;

	return ret;
}

static int rtk_dptx_register(struct drm_device *drm, struct rtk_dptx *dptx)
{
	struct drm_encoder *encoder = &dptx->encoder;
	struct drm_connector *connector = &dptx->connector;
	struct device *dev = dptx->dev;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	dev_info(dev, "dptx possible_crtcs (0x%x)\n", encoder->possible_crtcs);

	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	drm_encoder_init(drm, encoder, &rtk_dptx_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);

	drm_encoder_helper_add(encoder, dptx->dptx_data->helper_funcs);

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	drm_connector_init(drm, connector, &rtk_dptx_connector_funcs,
			   DRM_MODE_CONNECTOR_DisplayPort);
	drm_connector_helper_add(connector, &rtk_dptx_connector_helper_funcs);

	drm_connector_attach_encoder(connector, encoder);

	return 0;
}

static void rtk_dptx_pd_event_work(struct work_struct *work)
{
	struct rtk_dptx *dptx = container_of(work, struct rtk_dptx,
						event_work);
	struct drm_connector *connector = &dptx->connector;
	enum drm_connector_status old_status;
	int ret;
	int val = 1;

	dev_info(dptx->dev, "dptx: pd event work\n");
	mutex_lock(&dptx->lock);

#ifdef CONFIG_CHROME_PLATFORMS
	val = gpiod_get_value(dptx->hpd_gpio);
	if (val < 0)
		dev_err(dptx->dev, "failed to get hpd_gpio val");
	dev_info(dptx->dev, "[%s] hpd: %s\n", __func__,
						 (val) ? "connect" : "disconnect");
	dptx->connected = val ? true : false;
#else
	dptx->connected = true;
#endif

	if (!rtk_dptx_connected_port(dptx)) {
		dev_info(dptx->dev, "Not connected. Disabling dptx.\n");
		dptx->connected = false;

		if (dptx->dptx_data->type == RTK_NORMAL_TYPE) {
			mutex_unlock(&dptx->lock);
			rtk_dptx_stop_detect_hpd(dptx);
			mutex_lock(&dptx->lock);
		}
	} else if (!dptx->active && val) {
		dev_info(dptx->dev, "Connected. Not enabled.\n");

		if (dptx->dptx_data->type == RTK_NORMAL_TYPE) {
			ensure_clock_enabled(dptx);
			ret = rtk_dptx_enable(dptx);
			if (ret) {
				DRM_ERROR("Enable dptx failed %d\n", ret);
				dptx->connected = false;
			}
			rtk_dptx_start_detect_hpd(dptx);
		}
	} else if (!val) {
		dev_info(dptx->dev, "May be type-c dock, haven't insert dp device\n");
		rtk_dptx_start_detect_hpd(dptx);
	}
	// TODO: sink ? dongle ?


	// TODO: no ec platform HPD ?
	if (!dptx->ports) {
		dev_info(dptx->dev, "Temporary for no ec platform\n");

		if (dptx->dptx_data->type == RTK_NORMAL_TYPE) {
			ensure_clock_enabled(dptx);
			ret = rtk_dptx_enable(dptx);
			if (ret) {
				DRM_ERROR("Enable dptx failed %d\n", ret);
				dptx->connected = false;
			}
			// rtk_dptx_start_detect_hpd(dptx);
		}

		dptx->connected = true;
	}

	old_status = connector->status;

	mutex_unlock(&dptx->lock);

	connector->status = connector->funcs->detect(connector, false);

	dev_info(dptx->dev, "old_status = %d, new_status = %d\n",
						old_status, connector->status);

	if (old_status != connector->status) {
		dev_info(dptx->dev, "dptx status changed, send hotplug event\n");
		drm_kms_helper_hotplug_event(dptx->drm_dev);
	}

	rtk_dptx_update_plugged_status(dptx);
}

static int rtk_dptx_pd_event(struct notifier_block *nb,
			   unsigned long event, void *priv)
{
	struct rtk_dptx_port *port = container_of(nb, struct rtk_dptx_port,
						event_nb);
	struct rtk_dptx *dptx = port->dptx;

	dev_info(dptx->dev, "dptx: pd event\n");

	schedule_work(&dptx->event_work);

	return NOTIFY_DONE;
}

static int rtk_dptx_init_properties(struct rtk_dptx *dptx)
{
	int ret;

	dev_info(dptx->dev, "dptx: init properties\n");

	if (dptx->connector.funcs->reset)
		dptx->connector.funcs->reset(&dptx->connector);

	ret = drm_connector_attach_max_bpc_property(&dptx->connector, 6, 16);
	if (ret) {
		dev_err(dptx->dev, "drm dptx attach max bpc property fail\n");
		return ret;
	}

	return 0;
}

static int rtk_dptx_parse_dt(struct rtk_dptx *dptx)
{
	struct platform_device *pdev;
	struct device *dev;
	struct device_node *syscon_np;
	struct regmap *iso_pinctl;
	int ret = 0;
	unsigned int val;

	if (dptx->dptx_data->type == RTK_AUTOMOTIVE_TYPE)
		return 0;

	dev = dptx->dev;
	pdev = to_platform_device(dev);

	dev_info(dev, "dptx: parse dt\n");

	dptx->clk_dptx = devm_clk_get(dev, "clk_en_dptx");
	if (IS_ERR(dptx->clk_dptx)) {
		dev_err(dev, "failed to get clock\n");
		return PTR_ERR(dptx->clk_dptx);
	}
	clk_prepare_enable(dptx->clk_dptx);

	dptx->clk_iso_misc = devm_clk_get(dev, "clk_iso_misc");
	if (IS_ERR(dptx->clk_iso_misc)) {
		dev_err(dev, "failed to get clk_iso_misc clock\n");
		return PTR_ERR(dptx->clk_iso_misc);
	}

	dptx->clk_usb_p4 = devm_clk_get(dev, "clk_usb_p4");
	if (IS_ERR(dptx->clk_usb_p4)) {
		dev_err(dev, "failed to get clk_usb_p4 clock\n");
		return PTR_ERR(dptx->clk_usb_p4);
	}

	dptx->rstc_dptx = devm_reset_control_get(dev, "rstn_dptx");
	if (IS_ERR(dptx->rstc_dptx)) {
		dev_err(dev, "failed to get reset controller\n");
		return PTR_ERR(dptx->rstc_dptx);
	}
	reset_control_deassert(dptx->rstc_dptx);

	/* Temporarily removed, no need to deassert again */
	// dptx->rstc_misc = devm_reset_control_get(dev, "rstn_misc");
	// if (IS_ERR(dptx->rstc_misc)) {
	// 	dev_err(dev, "failed to get reset misc controller\n");
	// 	return PTR_ERR(dptx->rstc_misc);
	// }

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "dptx parse syscon phandle 0 fail");
		return -ENODEV;
	}

	dptx->dptx14_reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(dptx->dptx14_reg_base)) {
		dev_err(dev, "remap syscon 0 to dptx14_reg_base fail");
		of_node_put(syscon_np);
		return PTR_ERR(dptx->dptx14_reg_base);
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 1);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "dptx parse syscon phandle 1 fail");
		return -ENODEV;
	}

	dptx->dptx14_mac_reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(dptx->dptx14_mac_reg_base)) {
		dev_err(dev, "remap syscon 1 to dptx14_mac_reg_base fail");
		of_node_put(syscon_np);
		return PTR_ERR(dptx->dptx14_mac_reg_base);
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 2);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "dptx parse syscon phandle 2 fail");
		return -ENODEV;
	}

	dptx->dptx14_aphy_reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(dptx->dptx14_aphy_reg_base)) {
		dev_err(dev, "remap syscon 2 to dptx14_aphy fail");
		of_node_put(syscon_np);
		return PTR_ERR(dptx->dptx14_aphy_reg_base);
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 3);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "dptx parse syscon phandle 3 fail");
		return -ENODEV;
	}

	dptx->crt_reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(dptx->crt_reg_base)) {
		dev_err(dev, "remap syscon 2 to crt_reg_base fail");
		of_node_put(syscon_np);
		return PTR_ERR(dptx->crt_reg_base);
	}

	dptx->iso_base = syscon_regmap_lookup_by_phandle(dev->of_node, "realtek,iso");
	if (IS_ERR(dptx->iso_base)) {
		dev_err(dev, "couldn't get iso register base address\n");
		return PTR_ERR(dptx->iso_base);
	}

	dptx->check_connector_limit = of_property_read_bool(dev->of_node,
												 "check-connector-limit");
	if (dptx->check_connector_limit)
		dev_info(dev, "check connector limit\n");

	ret = of_property_read_u32(dev->of_node, "max-clock-k",
				&dptx->max_clock_k);
	if (ret < 0)
		dptx->max_clock_k = RTK_DP_MAX_CLOCK_K;
	dev_info(dev, "max-clock-k=%u\n", dptx->max_clock_k);

	check_iso_clock_is_on(dptx);

#ifdef CONFIG_CHROME_PLATFORMS
	INIT_WORK(&dptx->retrain_link_work, rtk_dptx_retrain_link_worker);
#endif

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

	dptx->aux.name = "dptx-AUX";
	dptx->aux.transfer = rtk_dptx_aux_transfer;
	dptx->aux.dev = dev;
	dptx->aux.drm_dev = dptx->drm_dev;
	ret = drm_dp_aux_register(&dptx->aux);
	if (ret) {
		dev_err(dev, "drm dp aux register fail\n");
		return ret;
	}

	iso_pinctl = syscon_regmap_lookup_by_phandle(dev->of_node,
												 "realtek,pinctrl");
	if (IS_ERR(iso_pinctl)) {
		DRM_ERROR("fail to get iso pinctl reg\n");
		return PTR_ERR(iso_pinctl);
	}

	// Disable gpio 16 pull up/down function
	regmap_read(iso_pinctl, ISO_PFUN1, &val);
	regmap_write(iso_pinctl, ISO_PFUN1,
			(val & ~ISO_MUXPAD1_GPIO_16_PUD_MASK) | ISO_MUXPAD1_GPIO_16_PUD_DISABLE);


	dptx->hpd_gpio = devm_gpiod_get_optional(dev, "hpd", GPIOD_IN);
	if (dptx->hpd_gpio) {
		if (IS_ERR(dptx->hpd_gpio))
			return dev_err_probe(dev, PTR_ERR(dptx->hpd_gpio),
						"Could not get hpd gpio\n");
		dev_info(dev, "dptx hotplug gpio(%d)\n", desc_to_gpio(dptx->hpd_gpio));

		ret = gpiod_direction_input(dptx->hpd_gpio);
		if (ret)
			dev_err(dev, "failed to set hpd_gpio direction");

		ret = gpiod_set_debounce(dptx->hpd_gpio, RTK_HPD_GPIO_PLUG_DEB_TIME_US);
		if (ret)
			dev_err(dptx->dev, "failed to set hpd_gpio debounce");

		INIT_DELAYED_WORK(&dptx->hpd_gpio_work, rtk_dptx_hpd_gpio_worker);
	} else {
		dev_info(dev, "no hpd_gpios node, utilze dptx_hpd\n");
		// Mux to dptx_hpd
		regmap_read(iso_pinctl, ISO_MUXPAD1, &val);
		regmap_write(iso_pinctl, ISO_MUXPAD1,
				(val & ~ISO_MUXPAD1_GPIO_16_MASK) | ISO_MUXPAD1_GPIO_16_MUX_DPTX);
	}

	dptx->connected   = false;
	dptx->active      = false;

	return ret;
}

static int rtk_dptx_extcon_register(struct rtk_dptx *dptx)
{
	struct platform_device *pdev;
	struct device *dev;
	struct rtk_dptx_port *port;
	int i;
	int ret = 0;

	dev = dptx->dev;
	pdev = to_platform_device(dev);

	dev_info(dev, "dptx: extcon_register\n");

	sema_init(&dptx->sem, 0);

	for (i = 0; i < dptx->ports; i++) {
		port = dptx->port[i];

		port->event_nb.notifier_call = rtk_dptx_pd_event;
		ret = devm_extcon_register_notifier(dptx->dev, port->extcon,
						    EXTCON_DISP_DP,
						    &port->event_nb);
		if (ret) {
			dev_err(dev, "register EXTCON_DISP_DP notifier err\n");
			return -ENODEV;
		}

		dev_info(dev, "register notifier success\n");
	}

	return ret;
}

static int rtk_dptx_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct drm_device *drm = data;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_dptx *dptx = dev_get_drvdata(dev);
	int ret;

	dev_info(dev, "dptx: bind\n");

	dptx->drm_dev = drm;

	INIT_WORK(&dptx->event_work, rtk_dptx_pd_event_work);

	dptx->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if (priv->krpc_second == 1)
		dptx->rpc_info_vo = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		dptx->rpc_info_vo = NULL;

	dev_info(dev, "dptx->rpc_info (%p), dptx->rpc_info_vo (%p)\n",
		dptx->rpc_info, dptx->rpc_info_vo);

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

	ret = rtk_dptx_extcon_register(dptx);
	if (ret) {
		dev_err(dptx->dev, "rtk_dptx_extcon_register fail\n");
		return ret;
	}

	schedule_work(&dptx->event_work);

	return 0;
}

static void rtk_dptx_unbind(struct device *dev, struct device *master,
			     void *data)
{
	struct rtk_dptx *dptx = dev_get_drvdata(dev);

	kfree(dptx->edid);
	dptx->edid = NULL;
}

static const struct component_ops rtk_dptx_ops = {
	.bind	= rtk_dptx_bind,
	.unbind	= rtk_dptx_unbind,
};

static int rtk_dptx_suspend(struct device *dev)
{
	struct rtk_dptx *dptx = dev_get_drvdata(dev);

	if (!dptx)
		return 0;

	dev_info(dptx->dev, "dptx: suspend\n");

	dptx->connected = false;

	if (dptx->dptx_data->type == RTK_NORMAL_TYPE) {
		rtk_dptx_stop_detect_hpd(dptx);
		rtk_dptx_poweroff(dptx);
	}

	return 0;
}

static int rtk_dptx_resume(struct device *dev)
{
	struct rtk_dptx *dptx = dev_get_drvdata(dev);

	if (!dptx)
		return 0;

	dev_info(dptx->dev, "dptx: resume\n");

	schedule_work(&dptx->event_work);

	return 0;
}

static const struct dev_pm_ops rtk_dptx_pm_ops = {
	.suspend    = rtk_dptx_suspend,
	.resume     = rtk_dptx_resume,
};

static struct rtk_dptx_platform_data rtk_dptx_data = {
	.type = RTK_NORMAL_TYPE,
	.get_modes = rtk_dptx_get_modes,
	.max_phy = 1,
	.helper_funcs = &rtk_dptx_encoder_helper_funcs,
};

static struct rtk_dptx_platform_data rtk_car_dptx_data = {
	.type = RTK_AUTOMOTIVE_TYPE,
	.get_modes = rtk_car_dptx_get_modes,
	.max_phy = 1,
	.helper_funcs = &rtk_car_dptx_encoder_helper_funcs,
};

static const struct of_device_id rtk_dptx_dt_ids[] = {
	{
		.compatible = "realtek,rtk-dptx",
		.data = &rtk_dptx_data,
	},
	{
		.compatible = "realtek,rtk-car-dptx",
		.data = &rtk_car_dptx_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_dptx_dt_ids);

static int rtk_dptx_audio_hw_params(struct device *dev,  void *data,
				  struct hdmi_codec_daifmt *daifmt,
				  struct hdmi_codec_params *params)
{
	struct rtk_dptx *dptx = dev_get_drvdata(dev);
	struct audio_info ainfo = {
		.sample_width = params->sample_width,
		.sample_rate = params->sample_rate,
		.channels = params->channels,
	};
	int ret = 0;

	dev_info(dev, "dptx: audio hw params\n");
	dev_info(dev, "dptx (%p), dev (%p), dptx->audio_pdev (%p)\n",
					dptx, dev, dptx->audio_pdev);

	dev_info(dev, "dptx: sample_rate = %d, sample_width = %d, channels = %d\n",
		ainfo.sample_rate, ainfo.sample_width, ainfo.channels);

	if (!dptx->active) {
		DRM_ERROR("dptx not active\n");
		ret = -ENODEV;
		goto out;
	}

	switch (daifmt->fmt) {
	case HDMI_I2S:
		ainfo.format = AUDIO_FMT_I2S;
		break;
	case HDMI_SPDIF:
		ainfo.format = AUDIO_FMT_SPDIF;
		break;
	default:
		DRM_ERROR("Invalid audio format %d\n", daifmt->fmt);
		ret = -EINVAL;
		goto out;
	}

	rtk_dptx_phy_config_audio(dptx, &ainfo);

out:
	return ret;
}

static void rtk_dptx_audio_shutdown(struct device *dev, void *data)
{
	struct rtk_dptx *dptx = dev_get_drvdata(dev);

	dev_info(dev, "dptx: audio shutdown\n");
	dev_info(dev, "dptx (%p), dev (%p), dptx->audio_pdev (%p)\n",
					dptx, dev, dptx->audio_pdev);

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_SEC_FUNCTION_CTRL,
									AUDIO_EN, 0);
}

static int rtk_dptx_audio_startup(struct device *dev, void *data)
{
	struct rtk_dptx *dptx = dev_get_drvdata(dev);

	dev_info(dev, "dptx: audio startup\n");
	dev_info(dev, "dptx (%p), dev (%p), dptx->audio_pdev (%p)\n",
			dptx, dev, dptx->audio_pdev);

	rtk_dptx14_mac_update(dptx, DPTX14_MAC_IP_SEC_FUNCTION_CTRL,
									AUDIO_EN, AUDIO_EN);
	return 0;
}

static int rtk_dptx_audio_hook_plugged_cb(struct device *dev, void *data,
					hdmi_codec_plugged_cb fn,
					struct device *codec_dev)
{
	struct rtk_dptx *dptx = data;

	dev_info(dev, "dptx: audio hook plugged cb\n");
	dev_info(dev, "dptx (%p), dev (%p), dptx->audio_pdev (%p)\n",
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
	struct rtk_dptx *dptx = dev_get_drvdata(dev);

	dev_info(dev, "dptx: audio get eld\n");

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

static int rtk_dptx_audio_codec_init(struct rtk_dptx *dptx,
				   struct device *dev)
{
	struct hdmi_codec_pdata codec_data = {
		.i2s   = 1,
		.spdif = 1,
		.ops   = &rtk_dptx_audio_codec_ops,
		.data = dptx,
	};

	dev_info(dev, "dptx: audio codec init\n");

	dptx->audio_pdev = platform_device_register_data(
			 dev, HDMI_CODEC_DRV_NAME, PLATFORM_DEVID_AUTO,
			 &codec_data, sizeof(codec_data));

	dev_info(dev, "dptx (%p), dev (%p), dptx->audio_pdev (%p), &codec_data (%p)\n",
		dptx, dev, dptx->audio_pdev, &codec_data);

	return PTR_ERR_OR_ZERO(dptx->audio_pdev);
}

static int rtk_dptx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_dptx *dptx;
	const struct rtk_dptx_platform_data *dptx_data;
	struct rtk_dptx_port *port;
	struct device_node *node;
	struct extcon_dev *extcon;
	int i;
	int ret;

	dev_info(dev, "dptx: probe\n");

	dptx = devm_kzalloc(dev, sizeof(*dptx), GFP_KERNEL);
	if (!dptx)
		return -ENOMEM;

	dptx->dev = dev;

	dptx->dptx_data = of_device_get_match_data(&pdev->dev);
	dptx_data = dptx->dptx_data;

	node = of_parse_phandle(dev->of_node, "extcon", 0);
	if (node) {
		dev_info(dev, "dptx has extcon node (%p)\n", node);

		for (i = 0; i < dptx_data->max_phy; i++) {
			extcon = extcon_get_edev_by_phandle(dev, i);

			if (PTR_ERR(extcon) == -EPROBE_DEFER) {
				dev_err(dev, "extcon probe defer\n");
				return -EPROBE_DEFER;
			}

			if (IS_ERR(extcon) /*|| IS_ERR(phy)*/) {
				dev_err(dev, "extcon is error\n");
				continue;
			}

			port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
			if (!port)
				return -ENOMEM;

			port->extcon = extcon;
			// port->phy = phy;
			port->dptx = dptx;
			port->id = i;
			dptx->port[dptx->ports++] = port;
		}

		if (!dptx->ports) {
			dev_err(dev, "dptx missing extcon or phy\n");
			return -EINVAL;
		}

	} else {
		dev_info(dev, "dptx no extcon node\n");
	}

	mutex_init(&dptx->lock);
	dev_set_drvdata(dev, dptx);

	ret = rtk_dptx_audio_codec_init(dptx, dev);
	if (ret)
		return ret;

	return component_add(&pdev->dev, &rtk_dptx_ops);
}

static int rtk_dptx_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_dptx_ops);
	return 0;
}

struct platform_driver rtk_dptx_driver = {
	.probe  = rtk_dptx_probe,
	.remove = rtk_dptx_remove,
	.driver = {
		.name = "rtk-dptx",
		.of_match_table = rtk_dptx_dt_ids,
#if IS_ENABLED(CONFIG_PM)
		.pm = &rtk_dptx_pm_ops,
#endif
	},
};

MODULE_AUTHOR("Ray Tang <ray.tang@realtek.com>");
MODULE_DESCRIPTION("Realtek DisplayPort Driver");
MODULE_LICENSE("GPL v2");
