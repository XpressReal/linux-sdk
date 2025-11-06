// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek Embedded DisplayPort driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#include <drm/drm_of.h>
#include <drm/drm_print.h>

#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include <linux/component.h>
#include <linux/platform_device.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_edid.h>
#include <video/videomode.h>

#include "rtk_edp.h"
#include "rtk_drm_drv.h"
#include "rtk_edp_phy.h"
#include "rtk_edp_reg.h"
#include "rtk_crt_reg.h"
#include "rtk_dp_utils.h"

#define to_rtk_edp(x) container_of(x, struct rtk_edp, x)

#define ISO_MUXPAD4	0x10
#define ISO_MUXPAD4_GPIO_128_MASK (0xf << 5)
#define ISO_MUXPAD4_GPIO_128_MUX_EDPTX (1 << 5)
#define ISO_PFUN7 0x3c
#define ISO_MUXPAD4_GPIO_128_PUD_MASK (0x3 << 25)
#define ISO_MUXPAD4_GPIO_128_PUD_DISABLE (0 << 25) // Disable pull up/down func

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
#define RETRY_ERROR_EN		(1 << 3)
#define RETRY_NACK_EN		(1 << 4)
#define RETRY_TIMEOUT_EN	(1 << 5)
#define RETRY_DEFER_EN		(1 << 6)
#define RETRY_EN			(1 << 7)

/* AUXTX_TRAN_CTRL */
#define TX_START	(1 << 0)
#define TX_ADDRONLY	(1 << 7)
/* AUX_TX_CTRL */
#define AUX_EN		(1 << 0)
/* DPTX_IRQ_CTRL */
#define DPTX_IRQ_EN (1 << 7)

/* AUX_TIMEOUT */
#define AUX_TIMEOUT_EN (1 << 7)
#define AUX_TIMEOUT_NUM (0x7f << 0)

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
#define DP_DPCD_IRQ_DATA_SIZE 2

#define RTK_EDP_AUX_WAIT_REPLY_COUNT 20
#define RTK_EDP_MAX_LINK_RATE DP_LINK_RATE_5_4
#define RTK_EDP_MAX_LANE_COUNT 2
#define RTK_POLL_HPD_INTERVAL_MS 1
#define RTK_HPD_GPIO_PLUG_DEB_TIME_US 100
#define RTK_HPD_GPIO_UNPLUG_DEB_TIME_US 50000
#define RTK_HPD_SHORT_PULSE_THRESHOLD_MS 5
#define RTK_MAX_SWING 3
#define RTK_MAX_EMPHASIS 3
#define RTK_EDP_MAX_CLOCK_K 348500 // 2560x1600p60

/* ISO */
#define ISO_PLL_XTAL_CTRL    (0x7a0)

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

struct rtk_edp_data {
	int (*bind)(struct rtk_edp *edp);
	void (*init)(struct rtk_edp *edp);
	int (*get_modes)(struct rtk_edp *edp);
	int (*init_hw)(struct rtk_edp *edp, struct drm_display_mode *mode);
};

static irqreturn_t rtk_edp_hpd_irq(int irq, void *dev_id);

void rtk_edp_update(struct rtk_edp *edp, u32 reg, u32 clear, u32 bits)
{
	unsigned int val;

	if (!edp->reg_base)
		return;

	regmap_read(edp->reg_base, reg, &val);

	val &= ~clear;
	val |= bits;

	regmap_write(edp->reg_base, reg, val);
}

static u32 rtk_edp_read(struct rtk_edp *edp, u32 reg)
{
	unsigned int val;

	if (!edp->reg_base)
		return 0;

	regmap_read(edp->reg_base, reg, &val);
	return val;
}

void rtk_edp_write(struct rtk_edp *edp, u32 reg, u32 val)
{
	if (!edp->reg_base)
		return;

	regmap_write(edp->reg_base, reg, val);
}

void rtk_edp_crt_write(struct rtk_edp *edp, u32 reg, u32 val)
{
	if (!edp->crt_reg_base)
		return;

	regmap_write(edp->crt_reg_base, reg, val);
}

void rtk_edp_crt_update(struct rtk_edp *edp, u32 reg, u32 clear, u32 bits)
{
	unsigned int val;

	if (!edp->crt_reg_base)
		return;

	regmap_read(edp->crt_reg_base, reg, &val);

	val &= ~clear;
	val |= bits;

	regmap_write(edp->crt_reg_base, reg, val);
}

void rtk_edp_wrap_update(struct rtk_edp *edp, u32 reg, u32 clear, u32 bits)
{
	unsigned int val;

	if (!edp->edp_wrapper_reg_base)
		return;

	regmap_read(edp->edp_wrapper_reg_base, reg, &val);

	val &= ~clear;
	val |= bits;

	regmap_write(edp->edp_wrapper_reg_base, reg, val);
}

static int rtk_edp_get_bpc(struct rtk_edp *edp)
{
	struct drm_display_info *display_info = &edp->connector.display_info;

	switch (display_info->bpc) {
	//	bpc 10 has some problem
	//case 10:
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

#ifdef CONFIG_CHROME_PLATFORMS
static bool rtk_edp_has_sink_count(struct rtk_edp *edp)
{
	struct drm_connector *connector = &edp->connector;

	return drm_dp_read_sink_count_cap(connector, edp->rx_cap, &edp->desc);
}

static int rtk_edp_get_sink_count(struct rtk_edp *edp)
{
	int sink_count = -1;

	if (!rtk_edp_has_sink_count(edp))
		return -1;

	sink_count = drm_dp_read_sink_count(&edp->aux);
	if (sink_count < 0) {
		dev_info(edp->dev, "edp read sink count error\n");
		return -1;
	}

	return sink_count;
}

static int rtk_edp_get_max_link_rate(struct rtk_edp *edp)
{
	return min_t(int, drm_dp_max_link_rate(edp->rx_cap),
					 RTK_EDP_MAX_LINK_RATE);
}

static unsigned int rtk_edp_get_max_lane_count(struct rtk_edp *edp)
{
	return min_t(unsigned int, drm_dp_max_lane_count(edp->rx_cap),
					 RTK_EDP_MAX_LANE_COUNT);
}

static int rtk_edp_change_link_rate_with_display_mode(struct rtk_edp *edp,
										 struct drm_display_mode *mode)
{
	int ret = rtk_edp_get_max_link_rate(edp);
	// RGB 8 bits
	int peak_bw = (int) mode->clock * 3 * edp->bpc / 1000;
	int bw_1_62 = 1620 * 8 * edp->lane_count / 10;
	int bw_2_7 = 2700 * 8 * edp->lane_count / 10;
	int bw_5_4 = 5400 * 8 * edp->lane_count / 10;
	int bw_usage = 0;

	if (peak_bw <= bw_1_62) {
		ret = DP_LINK_RATE_1_62;
		bw_usage = 100 * peak_bw / bw_1_62;
	} else if (peak_bw <= bw_2_7 && ret >= DP_LINK_RATE_2_7) {
		ret = DP_LINK_RATE_2_7;
		bw_usage = 100 * peak_bw / bw_2_7;
	} else if (peak_bw <= bw_5_4 && ret >= DP_LINK_RATE_5_4) {
		ret = DP_LINK_RATE_5_4;
		bw_usage = 100 * peak_bw / bw_5_4;
	}

	dev_info(edp->dev, "edp link bw usage: %d %%\n", bw_usage);
	return ret;
}

static int rtk_edp_reduce_lane_count(struct rtk_edp *edp)
{
	if (edp->lane_count > 1)
		edp->lane_count = edp->lane_count / 2;
	else
		return -EINVAL;
	return 0;
}

static int rtk_edp_reduce_link_rate(struct rtk_edp *edp)
{
	int ret = 0;

	switch (edp->link_rate) {
	case DP_LINK_RATE_1_62:
		ret = rtk_edp_reduce_lane_count(edp);
		edp->link_rate = rtk_edp_get_max_link_rate(edp);
		break;
	case DP_LINK_RATE_2_7:
		edp->link_rate = DP_LINK_RATE_1_62;
		break;
	case DP_LINK_RATE_5_4:
		edp->link_rate = DP_LINK_RATE_2_7;
		break;
	default:
		return -EINVAL;
	};
	return ret;
}

static int rtk_edp_fallback_video_format(struct rtk_edp *edp)
{
	int ret = 0;

	if (!drm_dp_clock_recovery_ok(edp->link_status, edp->lane_count))
		ret = rtk_edp_reduce_link_rate(edp);
	else if (!drm_dp_channel_eq_ok(edp->link_status, edp->lane_count))
		ret = rtk_edp_reduce_lane_count(edp);

	edp->is_fallback_mode = true;
	return ret;
}
#endif

static int rtk_edp_read_dpcd(struct rtk_edp *edp)
{
	int ret;

#ifdef CONFIG_CHROME_PLATFORMS
	struct drm_dp_aux *aux = &edp->aux;
	uint8_t downstream_ports[DP_MAX_DOWNSTREAM_PORTS];
	uint8_t tmp;

	ret = drm_dp_read_dpcd_caps(aux, edp->rx_cap);
	if (ret < 0)
		return ret;

	edp->link_rate = rtk_edp_get_max_link_rate(edp);
	edp->lane_count = rtk_edp_get_max_lane_count(edp);

	/* DP Compliance Test 4.2.2.1 */
	drm_dp_dpcd_read(&edp->aux, DP_ADAPTER_CAP, &tmp, 1);
	/* DP Compliance Test 4.2.2.8, drm_dp_read_dpcd_caps dosn't read 0x220f */
	drm_dp_dpcd_read(&edp->aux, DP_DPCD_ADAPTER_CAP, &tmp, 1);
	ret = drm_dp_read_desc(aux, &edp->desc, drm_dp_is_branch(edp->rx_cap));
	if (ret < 0) {
		dev_info(edp->dev, "edp read desc error\n");
		return ret;
	}

	/* DP Compliance Test 4.2.2.8 */
	rtk_edp_get_sink_count(edp);

	ret = drm_dp_read_downstream_info(&edp->aux, edp->rx_cap, downstream_ports);
#else
	// Temporarily use this api until Google agrees to use the symbol on STB
	ret = drm_dp_dpcd_read(&edp->aux, DP_DPCD_REV, edp->rx_cap,
							 DP_RECEIVER_CAP_SIZE);

	edp->link_rate = DP_LINK_RATE_2_7;
	edp->lane_count = 2;
#endif
	if (ret < 0) {
		dev_info(edp->dev, "edp read dpcd error\n");
		return ret;
	}
	return ret;
}

static void rtk_edp_get_edid(struct rtk_edp *edp)
{
	struct edid *edid;
	struct drm_connector *connector = &edp->connector;

	edid = drm_get_edid(connector, &edp->aux.ddc);
	if (drm_edid_is_valid(edid)) {
		kfree(edp->edid);
		edp->edid = edid;
#ifdef CONFIG_CHROME_PLATFORMS
		/* DP Compliance Test 4.2.2.3 */
		drm_dp_send_real_edid_checksum(&edp->aux, edid->checksum);
#endif
	} else {
#ifdef CONFIG_CHROME_PLATFORMS
		/* DP Compliance Test 4.2.2.6 */
		if (connector->edid_corrupt)
			drm_dp_send_real_edid_checksum(&edp->aux, connector->real_edid_checksum);
#endif
		kfree(edid);
	}
}

static int rtk_edp_get_sink_capability(struct rtk_edp *edp)
{
	int ret = 0;

	dev_info(edp->dev, "edp: get sink capability\n");

	mutex_lock(&edp->lock);

	ret = rtk_edp_read_dpcd(edp);
	if (ret < 0) {
		dev_err(edp->dev, "edp read dpcd error\n");
		goto out;
	}

	rtk_edp_get_edid(edp);

out:
	mutex_unlock(&edp->lock);
	return ret;
}

#ifdef CONFIG_CHROME_PLATFORMS
static int rtk_edp_get_link_status(struct rtk_edp *edp)
{
	if (!edp->connected)
		return -ENODEV;

	if (drm_dp_dpcd_read(&edp->aux, DP_LANE0_1_STATUS, edp->link_status,
			     DP_LINK_STATUS_SIZE) < 0) {
		DRM_ERROR("Failed to get link status\n");
		return -ENODEV;
	}

	return 0;
}

static bool rtk_edp_is_link_status_ok(struct rtk_edp *edp)
{
	return drm_dp_channel_eq_ok(edp->link_status, edp->lane_count);
}
#endif

static void rtk_edp_set_bad_connector_link_status(struct rtk_edp *edp)
{
	struct drm_device *dev;
	struct drm_connector *connector;

	connector = &edp->connector;
	dev = connector->dev;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	connector->state->link_status = DRM_MODE_LINK_STATUS_BAD;
	drm_modeset_unlock(&dev->mode_config.connection_mutex);
}

#ifdef CONFIG_CHROME_PLATFORMS
static void rtk_edp_retrain_link_worker(struct work_struct *retrain_link_work)
{
	struct rtk_edp *edp = to_rtk_edp(retrain_link_work);

	rtk_edp_set_bad_connector_link_status(edp);
	drm_kms_helper_connector_hotplug_event(&edp->connector);

	dev_info(edp->dev, "[%s] eDP hotplug evenet\n", __func__);
}

static uint8_t rtk_edp_handle_automated_link_training_test(struct rtk_edp *edp)
{
	/* DP Compliance Test 4.3.3.1 */
	int status = 0;
	uint8_t test_lane_count;
	uint8_t test_link_rate;

	status = drm_dp_dpcd_readb(&edp->aux, DP_TEST_LANE_COUNT,
				   &test_lane_count);
	if (status <= 0) {
		dev_err(edp->dev, "edp read test lane count error\n");
		return DP_TEST_NAK;
	}

	status = drm_dp_dpcd_readb(&edp->aux, DP_TEST_LINK_RATE,
				   &test_link_rate);
	if (status <= 0) {
		dev_err(edp->dev, "edp read test link rate error\n");
		return DP_TEST_NAK;
	}

	if (test_lane_count <= RTK_EDP_MAX_LANE_COUNT &&
		 drm_dp_bw_code_to_link_rate(test_link_rate) <= RTK_EDP_MAX_LINK_RATE) {
		edp->lane_count = test_lane_count;
		edp->link_rate = drm_dp_bw_code_to_link_rate(test_link_rate);
		edp->is_autotest = true;
	}

	return DP_TEST_ACK;
}

static void rtk_edp_handle_automated_test_request(struct rtk_edp *edp)
{
	int status;
	uint8_t request;
	uint8_t response = DP_TEST_NAK;

	status = drm_dp_dpcd_readb(&edp->aux, DP_TEST_REQUEST, &request);
	if (status < 0) {
		dev_err(edp->dev, "edp read test request error\n");
		return;
	}

	switch (request) {
	case DP_TEST_LINK_TRAINING:
		response = rtk_edp_handle_automated_link_training_test(edp);
		break;
	case DP_TEST_LINK_VIDEO_PATTERN:
	case DP_TEST_LINK_PHY_TEST_PATTERN:
		dev_err(edp->dev, "Not available test request '%02x'\n", request);
		break;
	case DP_TEST_LINK_EDID_READ:
		// Handle in rtk_edp_get_edid().
		edp->is_autotest = false;
		return;
	default:
		dev_err(edp->dev, "Invalid test request '%02x'\n", request);
		break;
	}

	status = drm_dp_dpcd_writeb(&edp->aux, DP_TEST_RESPONSE, response);
	if (status < 0)
		dev_err(edp->dev, "edp write test response error\n");
}

static bool rtk_edp_short_pulse_need_retrain(struct rtk_edp *edp)
{
	uint8_t sink_count;
	uint8_t irq_vector;
	int ret = false;

	mutex_lock(&edp->lock);
	/* DEVICE_SERVICE_IRQ_VECTOR */
	if (drm_dp_dpcd_read(&edp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR,
						 &irq_vector, 1) < 0) {
		dev_err(edp->dev, "edp read service irq vector error\n");
		ret = true;
		goto out;
	}

	if (irq_vector && irq_vector & DP_AUTOMATED_TEST_REQUEST) {
		rtk_edp_handle_automated_test_request(edp);
		drm_dp_dpcd_writeb(&edp->aux, DP_DEVICE_SERVICE_IRQ_VECTOR, irq_vector);
		ret = true;
	} else {
		edp->is_autotest = false;
	}

	/* Just for CTS, DP Compliance Test 4.3.2.4 */
	if (drm_dp_dpcd_read(&edp->aux, DP_SINK_COUNT, &sink_count, 1) < 0) {
		dev_err(edp->dev, "edp read sink count error\n");
		ret = true;
		goto out;
	}

	/* DPCD 202h-207h */
	if (rtk_edp_get_link_status(edp) < 0) {
		ret = true;
		goto out;
	}

	/* DP Compliance Test 4.3.2.1, 4.3.2.2, 4.3.2.3 */
	if (!rtk_edp_is_link_status_ok(edp)) {
		ret = true;
		goto out;
	}

out:
	mutex_unlock(&edp->lock);
	return ret;
}
#endif

static bool rtk_edp_is_clock_on(struct rtk_edp *edp)
{
	return __clk_is_enabled(edp->clk_edp) || __clk_is_enabled(edp->clk_edptx);
}

static void rtk_edp_shutdown(struct rtk_edp *edp)
{
	struct rtk_rpc_info *rpc_info_vo = (edp->rpc_info_vo == NULL) ?
									 edp->rpc_info : edp->rpc_info_vo;
	struct rpc_set_display_out_interface interface;
	int ret;

	dev_info(edp->dev, "edp: shutdown\n");

	DRM_INFO("[%s] disable %s on %s\n", __func__,
		interface_names[interface.display_interface], mixer_names[DISPLAY_INTERFACE_MIXER2]);
	interface.display_interface       = DISPLAY_INTERFACE_eDP;
	interface.display_interface_mixer = DISPLAY_INTERFACE_MIXER_NONE;
	ret = rpc_set_out_interface(rpc_info_vo, &interface);
	if (ret)
		DRM_ERROR("rpc_set_out_interface rpc fail\n");

	rtk_edp_phy_disable_timing_gen(edp);
	edp->finished_training = false;

	if (edp->connected) {
		drm_dp_dpcd_writeb(&edp->aux, DP_SET_POWER, DP_SET_POWER_D3);
		usleep_range(2000, 3000);
	}

	// eDPTX PHY POW, TXPLL reset
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL0,
		SYS_EDPTX_PHY_CTRL0_REG_POW_EDP_mask
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_POW_mask
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_RSTB_mask,
		SYS_EDPTX_PHY_CTRL0_REG_POW_EDP(0)
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_POW(0)
		| SYS_EDPTX_PHY_CTRL0_REG_EDP_H_CMU_RSTB(0));
	// DPLL reset
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL2,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_RSTB_mask
		| SYS_EDPTX_PHY_CTRL2_REG_DPLL_POW_mask,
		SYS_EDPTX_PHY_CTRL2_REG_DPLL_POW(0)
		| SYS_EDPTX_PHY_CTRL2_REG_DPLL_RSTB(0));
	// PHY Lane0 POW
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL5,
		SYS_EDPTX_PHY_CTRL5_REG_EDP_TX0_POW_mask,
		SYS_EDPTX_PHY_CTRL5_REG_EDP_TX0_POW(0));
	// PHY Lane1 POW
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL6,
		SYS_EDPTX_PHY_CTRL6_REG_EDP_TX1_POW_mask,
		SYS_EDPTX_PHY_CTRL6_REG_EDP_TX1_POW(0));
	// PHY LDO P2S POW
	rtk_edp_crt_update(edp, SYS_EDPTX_PHY_CTRL4,
		SYS_EDPTX_PHY_CTRL4_REG_POW_LDO_P2S_mask,
		SYS_EDPTX_PHY_CTRL4_REG_POW_LDO_P2S(0));

	// Disable AUX
	rtk_edp_update(edp, AUX_TX_CTRL, AUX_EN, 0);

	if (__clk_is_enabled(edp->clk_edp))
		clk_disable_unprepare(edp->clk_edp);
	reset_control_assert(edp->rstc_edp);

	if (__clk_is_enabled(edp->clk_edptx))
		clk_disable_unprepare(edp->clk_edptx);
	// This reset may make AUX_EN become default value(1).
	// reset_control_assert(edp->rstc_edptx);
}

static void rtk_edp_power_on_panel(struct rtk_edp *edp)
{
	if (edp->vcc_gpio && !gpiod_get_value(edp->vcc_gpio)) {
		gpiod_set_value_cansleep(edp->vcc_gpio, 1);
		msleep_interruptible(210); // edp panel power sequence
	}
}

static void ensure_clock_enabled(struct rtk_edp *edp)
{
	rtk_edp_power_on_panel(edp);

	if (__clk_is_enabled(edp->clk_edp) && (!reset_control_status(edp->rstc_edp)) &&
		__clk_is_enabled(edp->clk_edptx) &&	(!reset_control_status(edp->rstc_edptx))) {
		dev_dbg(edp->dev, "edp clk already on\n");
	} else {
		dev_info(edp->dev, "edp clk off, reinit\n");

		if (__clk_is_enabled(edp->clk_edp))
			clk_disable_unprepare(edp->clk_edp);
		reset_control_assert(edp->rstc_edp);
		reset_control_deassert(edp->rstc_edp);
		clk_prepare_enable(edp->clk_edp);

		if (__clk_is_enabled(edp->clk_edptx))
			clk_disable_unprepare(edp->clk_edptx);
		reset_control_assert(edp->rstc_edptx);
		reset_control_deassert(edp->rstc_edptx);
		clk_prepare_enable(edp->clk_edptx);

		if (edp->edp_data->init)
			edp->edp_data->init(edp);
	}
}

static int rtk_edp_handle_short_pulse(struct rtk_edp *edp)
{
	if (!edp->connected || !edp->finished_training)
		return 0;

	dev_info(edp->dev, "edp: short pulse\n");

#ifdef CONFIG_CHROME_PLATFORMS
	if (rtk_edp_short_pulse_need_retrain(edp)) {
		dev_info(edp->dev, "eDP start retraining\n");
		rtk_edp_set_bad_connector_link_status(edp);
		rtk_edp_get_edid(edp); // edid might change while running CTS
		edp->finished_training = false;
	}
#endif
	drm_kms_helper_hotplug_event(edp->drm_dev);

	return 0;
}

static int rtk_edp_handle_long_pulse(struct rtk_edp *edp)
{
	dev_info(edp->dev, "edp: long pulse\n");

	if (edp->connected) {
		ensure_clock_enabled(edp);
		/* DP Compliance Test 4.2.1.1, 4.2.1.2 */
		// Disable hw timeout/error retry after HPD plug event.
		rtk_edp_update(edp, AUX_RETRY_2, RETRY_TIMEOUT_EN | RETRY_ERROR_EN, 0);
		rtk_edp_update(edp, AUX_TIMEOUT, AUX_TIMEOUT_EN, 0);

		rtk_edp_get_sink_capability(edp);
		edp->is_autotest = false;
		edp->is_fallback_mode = false;
	}

	dev_info(edp->dev, "edp status changed, send hotplug event\n");
	rtk_edp_set_bad_connector_link_status(edp);
	drm_kms_helper_hotplug_event(edp->drm_dev);

	return 0;
}


static int poll_hpd(struct rtk_edp *edp)
{
	unsigned int val = 0;
	bool old_conn_state = edp->connected;
	struct drm_connector *connector;
	struct drm_device *dev;
	bool is_short_pulse;
	bool is_long_pulse;

	connector = &edp->connector;
	dev = connector->dev;

	mutex_lock(&edp->lock);

	val = rtk_edp_read(edp, HPD_CTRL);
	edp->connected = (val & HPD_CTRL_DEB);
	val = rtk_edp_read(edp, HPD_IRQ);
	is_short_pulse = (bool) (val & HPD_IRQ_SHPD);
	is_long_pulse = (val & HPD_IRQ_UHPD || val & HPD_IRQ_LHPD);
	rtk_edp_write(edp, HPD_IRQ, HPD_IRQ_ALL);

	mutex_unlock(&edp->lock);


	if (is_long_pulse) {
		dev_info(edp->dev, "eDP unplug or long pulse\n");
		rtk_edp_set_bad_connector_link_status(edp);
	} else if (is_short_pulse) {
		dev_info(edp->dev, "eDP short pulse\n");
		rtk_edp_handle_short_pulse(edp);
		return val;
	}

	if (old_conn_state != edp->connected) {
		dev_info(edp->dev, "eDP %s\n", edp->connected ? "conn" : "disconnect");
		rtk_edp_handle_long_pulse(edp);
	}

	return val;
}

static int rtk_edp_hpd_thread(void *data)
{
	struct rtk_edp *edp = (struct rtk_edp *) data;

	while (!kthread_should_stop()) {
		poll_hpd(edp);
		msleep_interruptible(RTK_POLL_HPD_INTERVAL_MS);
	}

	return 0;
}

static int rtk_edp_start_hpd_thread(struct rtk_edp *edp)
{
	dev_info(edp->dev, "edp: start hpd thread\n");

	ensure_clock_enabled(edp);

	if (edp->hpd_thread) {
		dev_info(edp->dev, "hpd_thread already exsist\n");
		return 0;
	}

	edp->hpd_thread = kthread_run(rtk_edp_hpd_thread, edp, "edp_hpd_thread");
	if (IS_ERR(edp->hpd_thread)) {
		dev_err(edp->dev, "Failed to create kernel thread\n");
		edp->hpd_thread = NULL;
		return PTR_ERR(edp->hpd_thread);
	}

	return 0;
}

static int rtk_edp_stop_hpd_thread(struct rtk_edp *edp)
{
	dev_info(edp->dev, "edp: stop hpd thread\n");

	if (edp->hpd_thread) {
		kthread_stop(edp->hpd_thread);
		edp->hpd_thread = NULL;
	}

	edp->connected = false;


	return 0;
}

static void rtk_edp_reset_hw(struct rtk_edp *edp)
{
	//release disp, rstn
	regmap_write(edp->iso_base, ISO_PLL_XTAL_CTRL, 0x000000a5);
	reset_control_deassert(edp->rstc_disp);
	reset_control_deassert(edp->rstc_vo);
	reset_control_deassert(edp->rstc_edp);
}

static void rtk_edp_write_dpcd_lane_set(struct rtk_edp *edp,
						 struct rtk_edp_train_signal signals[4])
{
	uint8_t val[4] = {};
	int i;

	for (i = 0; i < edp->lane_count; i++) {
		val[i] = signals[i].swing << DP_TRAIN_VOLTAGE_SWING_SHIFT |
		      signals[i].emphasis << DP_TRAIN_PRE_EMPHASIS_SHIFT;
		if (signals[i].swing == RTK_MAX_SWING)
			val[i] |= DP_TRAIN_MAX_SWING_REACHED;
		if (signals[i].emphasis == RTK_MAX_EMPHASIS)
			val[i] |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
	}
	drm_dp_dpcd_write(&edp->aux, DP_TRAINING_LANE0_SET, val, edp->lane_count);
}

static int rtk_edp_signal_setting(struct rtk_edp *edp,
						 struct rtk_edp_train_signal signals[4])
{
	int ret;
	int i;

	for (i = 0; i < edp->lane_count; i++) {
		if (signals[i].swing + signals[i].emphasis > 3) {
			dev_err(edp->dev, "[%s] lane: %d invalid swing: %u, emp: %u\n",
				__func__, i, signals[i].swing, signals[i].emphasis);
			return -EINVAL;
		}
	}

	ret = rtk_edp_mac_signal_setting(edp, signals);
	if (ret)
		return ret;
	ret = rtk_edp_aphy_signal_setting(edp, signals);
	if (ret)
		return ret;

	rtk_edp_write_dpcd_lane_set(edp, signals);

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

static void rtk_edp_get_signals(uint8_t status[DP_LINK_STATUS_SIZE],
							 struct rtk_edp_train_signal signals[4])
{
	int i;

	for (i = 0; i < 4; i++) {
		signals[i].swing = rtk_drm_dp_get_adjust_request_voltage(status, i);
		signals[i].emphasis = rtk_drm_dp_get_adjust_request_pre_emphasis(status, i);
	}
}

static int rtk_edp_train_cr(struct rtk_edp *edp,
						 struct rtk_edp_train_signal signals[4])
{
	int i;
	uint8_t status[DP_LINK_STATUS_SIZE];
	uint8_t prev_swing = 0;
	uint8_t prev_emphasis = 0;
	int ret;

	dev_info(edp->dev, "[%s] start train cr\n", __func__);

	// rtk_edp_phy_set_scramble(edp, false);
	rtk_edp_phy_set_pattern(edp, RTK_PATTERN_1);
	drm_dp_dpcd_writeb(&edp->aux, DP_TRAINING_PATTERN_SET,
					 DP_TRAINING_PATTERN_1 | DP_LINK_SCRAMBLING_DISABLE);

	rtk_edp_write_dpcd_lane_set(edp, signals);
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

		ret = drm_dp_dpcd_read(&edp->aux, DP_LANE0_1_STATUS,
							 status, DP_LINK_STATUS_SIZE);
		if (ret < 0) {
			dev_info(edp->dev, "Get link status fail\n");
			return ret;
		}

		if (rtk_drm_dp_clock_recovery_ok(status, edp->lane_count)) {
			dev_info(edp->dev, "Link train CR pass\n");
			return 0;
		}

		rtk_edp_get_signals(status, signals);
		if (prev_swing == RTK_MAX_SWING && prev_swing == signals[0].swing)
			goto out;
		if (prev_emphasis == RTK_MAX_EMPHASIS && prev_emphasis == signals[0].emphasis)
			goto out;
		prev_swing = signals[0].swing;
		prev_emphasis = signals[0].emphasis;

		rtk_edp_signal_setting(edp, signals);
	}

out:
	dev_err(edp->dev, "Link train CR fail!\n");
	dev_err(edp->dev, "[%s] LANE0_1_STATUS(202) 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		 __func__, status[0], status[1], status[2],
		 status[3], status[4], status[5]);
	return -1;
}

static int rtk_edp_train_eq(struct rtk_edp *edp,
						 struct rtk_edp_train_signal signals[4])
{
	int i;
	uint8_t status[DP_LINK_STATUS_SIZE];
	uint8_t aux_rd_interval = (edp->rx_cap[14] & 0x7f) * 4;
	int eq_pattern =
		(drm_dp_tps3_supported(edp->rx_cap)) ? DP_TRAINING_PATTERN_3 :
		DP_TRAINING_PATTERN_2;

	dev_info(edp->dev, "[%s] start train eq\n", __func__);
	rtk_edp_phy_set_pattern(edp, eq_pattern);
	drm_dp_dpcd_writeb(&edp->aux, DP_TRAINING_PATTERN_SET,
					eq_pattern | DP_LINK_SCRAMBLING_DISABLE);

	/*
	 * Condition of EQ fail:
	 * 1. Failed to pass EQ over six times.
	 */
	for (i = 0; i < 6; i++) {
		drm_dp_dpcd_read(&edp->aux, DP_LANE0_1_STATUS,
						 status, DP_LINK_STATUS_SIZE);
		rtk_edp_get_signals(status, signals);
		rtk_edp_signal_setting(edp, signals);

		msleep_interruptible(aux_rd_interval);

		drm_dp_dpcd_read(&edp->aux, DP_LANE0_1_STATUS,
						 status, DP_LINK_STATUS_SIZE);
		dev_err(edp->dev, "[%s] LANE0_1_STATUS(202) 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		 __func__, status[0], status[1], status[2],
		 status[3], status[4], status[5]);
		if (rtk_drm_dp_channel_eq_ok(status, edp->lane_count)) {
			dev_info(edp->dev, "Link train EQ pass\n");
			return 0;
		}
	}

	dev_err(edp->dev, "Link train EQ fail!\n");
	dev_err(edp->dev, "[%s] LANE0_1_STATUS(202) 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		 __func__, status[0], status[1], status[2],
		 status[3], status[4], status[5]);
	return -1;
}

static int rtk_edp_link_training(struct rtk_edp *edp)
{
	int ret;
	struct rtk_edp_train_signal signals[4] = {};

	dev_info(edp->dev, "[%s] start training\n", __func__);

	rtk_edp_mac_signal_setting(edp, signals);
	rtk_edp_aphy_signal_setting(edp, signals);

	/* Spec says link_bw = link_rate / 0.27Gbps */
	drm_dp_dpcd_writeb(&edp->aux, DP_SET_POWER, DP_SET_POWER_D0);
	drm_dp_dpcd_writeb(&edp->aux, DP_LINK_BW_SET, edp->link_rate / 27000);
	drm_dp_dpcd_writeb(&edp->aux, DP_LANE_COUNT_SET,
					 edp->lane_count | DP_LANE_COUNT_ENHANCED_FRAME_EN);
	drm_dp_dpcd_writeb(&edp->aux, DP_MAIN_LINK_CHANNEL_CODING_SET,
					 DP_SET_ANSI_8B10B);

	ret = rtk_edp_train_cr(edp, signals);
	if (ret)
		goto out;
	ret = rtk_edp_train_eq(edp, signals);
	if (ret)
		goto out;
	msleep_interruptible(100);

out:
	if (ret) {
		dev_err(edp->dev, "[%s] link training fail!\n", __func__);
		return ret;
	}
	return 0;
}

static int rtk_edp_config_vo(struct rtk_edp *edp, struct drm_display_mode *mode)
{
	int ret;
	struct rpc_set_display_out_interface interface;
	struct rtk_rpc_info *rpc_info_vo = (edp->rpc_info_vo == NULL) ?
								 edp->rpc_info : edp->rpc_info_vo;

	interface.display_interface       = DISPLAY_INTERFACE_eDP;
	interface.width                   = mode->hdisplay;
	interface.height                  = mode->vdisplay;
	interface.frame_rate              = drm_mode_vrefresh(mode);
	interface.display_interface_mixer = DISPLAY_INTERFACE_MIXER2;

	DRM_INFO("[rtk_edp_enc_enable] enable %s on %s (%dx%d@%d)\n",
		interface_names[interface.display_interface], mixer_names[interface.display_interface_mixer],
		interface.width, interface.height, interface.frame_rate);

	ret = rpc_set_out_interface(rpc_info_vo, &interface);
	if (ret)
		DRM_ERROR("rpc_set_out_interface rpc fail\n");

	return ret;
}

static int rtk_edp_init_hw(struct rtk_edp *edp, struct drm_display_mode *mode)
{
	int ret;

	dev_info(edp->dev, "[%s] lane(%u) rate(%d) pclk(%d)\n", __func__,
				 edp->lane_count, edp->link_rate, mode->clock);

	rtk_edp_reset_hw(edp);
	rtk_edp_phy_dppll_setting(edp, mode);
	ret = rtk_edp_phy_pixelpll_setting(edp, mode);
	if (ret)
		return ret;
	rtk_edp_phy_config_video_timing(edp, mode);
	ret = rtk_edp_link_training(edp);
	if (ret) {
		// will vblank panic if return here
		dev_err(edp->dev, "[%s] link train fail!\n", __func__);
	}
	rtk_edp_phy_set_scramble(edp, true);
	rtk_edp_phy_config_csc(edp);
	rtk_edp_config_vo(edp, mode);
	rtk_edp_phy_start_video(edp, mode);


	dev_err(edp->dev, "[%s] hw setting is good!\n", __func__);
	return ret;
}

static void rtk_1920_edp_init(struct rtk_edp *edp)
{
	dev_info(edp->dev, "edp: init\n");

	/* enable aux channel */
	rtk_edp_write(edp, AUX_TX_CTRL, AUX_EN);
	rtk_edp_write(edp, AUX_IRQ_EN, AUX_ALL_IRQ);

	// Disable NACK retry
	rtk_edp_update(edp, AUX_RETRY_2, RETRY_NACK_EN, 0);

	rtk_edp_update(edp, HPD_CTRL, HPD_CTRL_EN, HPD_CTRL_EN);
	rtk_edp_write(edp, HPD_IRQ_EN, HPD_IRQ_EN_ALL);
}

static int rtk_edp_get_hpd_gpio(struct rtk_edp *edp)
{
	int val;

	val = gpiod_get_value(edp->hpd_gpio);
	if (val < 0) {
		dev_err(edp->dev, "failed to get hpd_gpio val");
	}

	return val;
}

static bool rtk_edp_enc_mode_fixup(struct drm_encoder *encoder,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	DRM_INFO("edp: mode fixup\n");

	return true;
}

static void rtk_edp_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	// struct rtk_dptx *dptx = to_rtk_dptx(encoder);
	int vic;

	vic = drm_match_cea_mode(mode);

	// dptx_set_video_timing(dptx, adj_mode);
	// dptx_set_sst_setting(dptx);
}

static void rtk_edp_enc_enable(struct drm_encoder *encoder)
{
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	struct rtk_edp *edp = to_rtk_edp(encoder);
	int ret;
	int val;

	dev_info(edp->dev, "edp: enc enable");
	mutex_lock(&edp->lock);

	ensure_clock_enabled(edp);

	val = rtk_edp_get_hpd_gpio(edp);
	if (val < 0) {
		dev_err(edp->dev, "failed to get hpd_gpio val");
		mutex_unlock(&edp->lock);
		return;
	}

	edp->connected = (val) ? true : false;
	if (!edp->connected) {
		dev_err(edp->dev, "disconnect, skip enc enable");
		mutex_unlock(&edp->lock);
		return;
	}


	edp->color_format = RTK_COLOR_FORMAT_RGB;
	edp->bpc = rtk_edp_get_bpc(edp);
#ifdef CONFIG_CHROME_PLATFORMS
	if (!edp->is_autotest && !edp->is_fallback_mode)
		edp->link_rate = rtk_edp_change_link_rate_with_display_mode(edp, mode);
#endif

	drm_dp_dpcd_writeb(&edp->aux, DP_SET_POWER, DP_SET_POWER_D0);
	usleep_range(2000, 5000);

	if (edp->edp_data->init_hw != NULL) {
		ret = edp->edp_data->init_hw(edp, mode);
		if (ret)
			DRM_ERROR("edp hw setting fail\n");

	}
#ifdef CONFIG_CHROME_PLATFORMS
	ret = rtk_edp_get_link_status(edp);
	if (ret)
		goto unlock;
	if (rtk_edp_is_link_status_ok(edp)) {
		edp->finished_training = true;
		goto out;
	}

	// Some RX needs to disable training pattern first to get correct status.
	drm_dp_dpcd_writeb(&edp->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
	ret = rtk_edp_get_link_status(edp);
	if (ret)
		goto unlock;
	if (rtk_edp_is_link_status_ok(edp)) {
		edp->finished_training = true;
		goto unlock;
	}

	/* if training fail, start retrain */
	DRM_ERROR("edp link status fail!\n");
	ret = rtk_edp_fallback_video_format(edp);
	if (ret) {
		DRM_ERROR("edp fallback rate/lane fail!\n");
		goto out;
	}

	schedule_work(&edp->retrain_link_work);
out:
	drm_dp_dpcd_writeb(&edp->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
unlock:
#else
	drm_dp_dpcd_writeb(&edp->aux, DP_TRAINING_PATTERN_SET,
			   DP_TRAINING_PATTERN_DISABLE);
#endif
	mutex_unlock(&edp->lock);
}

static void rtk_edp_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_edp *edp = to_rtk_edp(encoder);

	dev_info(edp->dev, "edp: enc disable\n");

	if (rtk_edp_is_clock_on(edp))
		rtk_edp_shutdown(edp);
}

static int rtk_edp_enc_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	return 0;
}

static const struct drm_encoder_funcs rtk_edp_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs rtk_edp_encoder_helper_funcs = {
	.mode_fixup = rtk_edp_enc_mode_fixup,
	.mode_set   = rtk_edp_enc_mode_set,
	.enable     = rtk_edp_enc_enable,
	.disable    = rtk_edp_enc_disable,
	.atomic_check = rtk_edp_enc_atomic_check,
};

static int rtk_edp_start_detect_hpd(struct rtk_edp *edp)
{
	int irq;
	int ret;
	struct device *dev = edp->dev;

	if (edp->hpd_irq) {
		dev_err(dev, "hpd irq is exist\n");
		return 0;
	}

	dev_info(dev, "edp: start detect hpd\n");
	irq = gpiod_to_irq(edp->hpd_gpio);
	if (irq < 0) {
		dev_err(dev, "Fail to get hpd irq");
		return -ENODEV;
	}
	edp->hpd_irq = irq;

	irq_set_irq_type(edp->hpd_irq, IRQ_TYPE_EDGE_BOTH);
	ret = devm_request_threaded_irq(dev, edp->hpd_irq, NULL,
				rtk_edp_hpd_irq, IRQF_ONESHOT,
				dev_name(dev), edp);
	if (ret) {
		dev_err(dev, "can't request hpd gpio irq\n");
	}

	return ret;
}

static void rtk_dptx_stop_detect_hpd(struct rtk_edp *edp)
{
	struct device *dev = edp->dev;

	dev_info(dev, "edp: stop detect hpd\n");
	if (edp->hpd_irq) {
		free_irq(edp->hpd_irq, edp);
		edp->hpd_irq = 0;
	}
}

static void rtk_edp_config_hpd_gpio(struct rtk_edp *edp)
{
	int ret;
	int debounce_time = (edp->connected) ? RTK_HPD_GPIO_PLUG_DEB_TIME_US :
									 RTK_HPD_GPIO_UNPLUG_DEB_TIME_US;

	ret = gpiod_set_debounce(edp->hpd_gpio, debounce_time);
	if (ret)
		dev_err(edp->dev, "failed to set hpd_gpio debounce");
}

static void rtk_edp_hpd_gpio_worker(struct work_struct *work)
{
	struct delayed_work *hpd_gpio_work = to_delayed_work(work);
	struct rtk_edp *edp = to_rtk_edp(hpd_gpio_work);
	bool prev_connected = edp->connected;
	struct device *dev = edp->dev;
	int val;

	val = rtk_edp_get_hpd_gpio(edp);
	if (val < 0)
		return;

	edp->connected = (val) ? true : false;
	dev_info(dev, "hpd %s\n", edp->connected ? "connected" : "disconnected");

	// short pulse, gpio will be high after 0.5 ~ 1ms
	if (prev_connected == true && edp->connected == true) {
		rtk_edp_handle_short_pulse(edp);
		return;
	}

	rtk_edp_config_hpd_gpio(edp);
	rtk_edp_handle_long_pulse(edp);
}

static int rtk_edp_get_gpio_hpd_status(struct rtk_edp *edp)
{
	int delay_ms;

	if (!edp->hpd_gpio)
		return -EINVAL;

	delay_ms = (edp->connected) ? RTK_HPD_SHORT_PULSE_THRESHOLD_MS : 0;
	schedule_delayed_work(&edp->hpd_gpio_work, msecs_to_jiffies(delay_ms));

	return 0;
}

static int rtk_edp_detect_gpio_hpd(struct rtk_edp *edp)
{
	if (rtk_edp_get_gpio_hpd_status(edp) >= 0) {
		dev_dbg(edp->dev, "success to get hpd status\n");
		return 0;
	}

	/**
	 * Some edp screen do not have hpd, add DT property force-hpd
	 */

	dev_dbg(edp->dev, "fail to get hpd status\n");

	return -ETIMEDOUT;
}

static enum drm_connector_status rtk_edp_conn_detect
(struct drm_connector *connector, bool force)
{
	struct rtk_edp *edp = to_rtk_edp(connector);
	enum drm_connector_status status = connector_status_disconnected;
	struct rtk_drm_private *priv = edp->drm_dev->dev_private;

#ifdef CONFIG_CHROME_PLATFORMS
	/* DP Compliance Test 4.2.2.7 */
	if (rtk_edp_get_sink_count(edp) == 0) {
		dev_info(edp->dev, "edp sink count is 0\n");
		return connector_status_disconnected;
	}
#endif

	if (edp->connected)
		status = connector_status_connected;

	if (edp->check_connector_limit)
		rtk_drm_update_connector(priv, RTK_CONNECTOR_EDP, edp->connected);

	return status;
}

static void rtk_edp_conn_destroy(struct drm_connector *connector)
{
	DRM_INFO("edp: conn destroy\n");

	drm_connector_cleanup(connector);
}

static int rtk_1920_edp_get_modes(struct rtk_edp *edp)
{
	struct edid *edid;
	struct drm_connector *connector = &edp->connector;
	int num_modes = 0;

	mutex_lock(&edp->lock);
	edid = edp->edid;
	if (edid) {
		drm_connector_update_edid_property(connector, edid);
		num_modes += drm_add_edid_modes(connector, edid);
		dev_info(edp->dev, "eDP get edid, num_modes (%d)\n", num_modes);
	}
	mutex_unlock(&edp->lock);

	return num_modes;
}

static int rtk_1861_edp_get_modes(struct rtk_edp *edp)
{
	struct drm_connector *connector;
	struct rtk_rpc_info *rpc_info = edp->rpc_info;
	struct drm_display_mode *mode;
	struct drm_display_mode disp_mode;
	struct rpc_query_display_out_interface_timing timing;

	connector = &edp->connector;
	timing.display_interface = DISPLAY_INTERFACE_eDP;

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

	edp->mixer = timing.mixer;

	DRM_INFO("rtk_edp_conn_get_modes (%dx%d)@%d on %s\n",
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

static int rtk_edp_conn_get_modes(struct drm_connector *connector)
{
	struct rtk_edp *edp = to_rtk_edp(connector);
	int num_modes = 0;

	if (edp->edp_data->get_modes != NULL) {
		num_modes = edp->edp_data->get_modes(edp);
		if (num_modes < 0)
			dev_err(edp->dev, "edp get modes fail\n");
	}

	return num_modes;
}

static enum drm_mode_status rtk_edp_conn_mode_valid
(struct drm_connector *connector, struct drm_display_mode *mode)
{
	u8 vic;
	struct rtk_edp *edp = to_rtk_edp(connector);
	int max_rate;
	int bpc;

#ifdef CONFIG_CHROME_PLATFORMS
	if (edp->is_fallback_mode || edp->is_autotest) {
		max_rate = edp->link_rate * (int) edp->lane_count;
	} else {
		max_rate = rtk_edp_get_max_link_rate(edp) *
					 rtk_edp_get_max_lane_count(edp);
	}
#else
	// remove until stb has related symbol.
	max_rate = edp->link_rate * (int) edp->lane_count;
#endif
	bpc = rtk_edp_get_bpc(edp);

	vic = drm_match_cea_mode(mode);

	if (vic >= VIC_3840X2160P24)
		return MODE_CLOCK_HIGH;

	/* efficiency is about 0.8 */
	if (max_rate < mode->clock * 3 * bpc / 8)
		return MODE_CLOCK_HIGH;

	if (vic == 0 && (mode->hdisplay > 2560 || mode->vdisplay > 1600 || mode->clock > 348500))
		return MODE_ERROR;

	if (mode->clock > edp->max_clock_k)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static const struct drm_connector_funcs rtk_edp_connector_funcs = {
//	.dpms = drm_atomic_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = rtk_edp_conn_detect,
	.destroy = rtk_edp_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs rtk_edp_connector_helper_funcs = {
	.get_modes = rtk_edp_conn_get_modes,
	.mode_valid = rtk_edp_conn_mode_valid,
};

static irqreturn_t rtk_edp_hpd_irq(int irq, void *dev_id)
{
	struct rtk_edp *edp = dev_id;

	int val = 0;

	dev_info(edp->dev, "edp: hpd irq\n");
	val = rtk_edp_detect_gpio_hpd(edp);

	return IRQ_HANDLED;
}

int edp_aux_isr(struct rtk_edp *edp)
{
	unsigned int val;
	int ret = 0;
	int i;

	for (i = 0; i < RTK_EDP_AUX_WAIT_REPLY_COUNT; i++) {
		val = rtk_edp_read(edp, AUX_IRQ_EVENT);
		if (val & AUXDONE)
			break;
		mdelay(1);
	}

	if (val & RXERROR) {
		dev_dbg(edp->dev, "edp aux error\n");
		ret = -1;
	} else if (val & AUXDONE) {
		dev_dbg(edp->dev, "edp aux done\n");
		ret = 0;
	} else {
		dev_err(edp->dev, "edp aux not done\n");
		ret = -1;
		rtk_edp_update(edp, AUX_RETRY_2, RETRY_TIMEOUT_EN | RETRY_ERROR_EN,
				 RETRY_TIMEOUT_EN | RETRY_ERROR_EN);
		rtk_edp_update(edp, AUX_TIMEOUT, AUX_TIMEOUT_EN, AUX_TIMEOUT_EN);
	}

	if (val & NACK)
		dev_err(edp->dev, "edp aux NACK\n");

	val = rtk_edp_read(edp, AUX_RETRY_1);
	if (val & RETRY_LOCK) {
		// unlock retry lock
		dev_err(edp->dev, "edp aux is lock\n");
		rtk_edp_update(edp, AUX_RETRY_2, RETRY_EN, 0);
		rtk_edp_update(edp, AUX_RETRY_2, RETRY_EN, RETRY_EN);
	}

	rtk_edp_write(edp, AUX_IRQ_EVENT, AUX_ALL_IRQ);

	return ret;
}

static irqreturn_t rtk_edp_aux_irq(int irq, void *dev_id)
{
	struct rtk_edp *edp = dev_id;
	int ret;

	dev_info(edp->dev, "[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	ret = edp_aux_isr(edp);
	if (!ret) {
		dev_dbg(edp->dev, "edp aux not done\n");
		wait_for_completion(&edp->comp);
	}

	return IRQ_HANDLED;
}

static int edp_aux_get_data(struct rtk_edp *edp, struct drm_dp_aux_msg *msg)
{
	u8 *buffer = msg->buffer;
	int i;
	int size = 0;
	unsigned int rd_ptr, wr_ptr;

	rd_ptr = rtk_edp_read(edp, AUX_FIFO_RD_PTR);
	wr_ptr = rtk_edp_read(edp, AUX_FIFO_WR_PTR);
	size = wr_ptr - rd_ptr;

	if (size > msg->size) {
		dev_err(edp->dev, "wrong size: fifo(%u), msg(%zu)\n", size, msg->size);
		size = msg->size;
	}

	for (i = 0; i < size; i++)
		buffer[i] = rtk_edp_read(edp, AUX_REPLY_DATA);

	return size;
}

static void edp_aux_transfer(struct rtk_edp *edp, struct drm_dp_aux_msg *msg)
{
	size_t size = msg->size;
	u32 addr = msg->address;
	u8 *buffer = msg->buffer;
	u8 data;
	int i;

	switch (msg->request & ~DP_AUX_I2C_MOT) {
	case DP_AUX_I2C_WRITE:
		if (addr == 0x37) {
			/* workaround, will let ASUS TUF GAMING VG289Q Monitor crash */
			DRM_INFO("Block 0x37 aux i2c write\n");
			return;
		}

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
		rtk_edp_write(edp, AUX_FIFO_CTRL, READ_FAIL_AUTO_EN | AUX_FIFO_CTRL_RESET);
	else
		rtk_edp_write(edp, AUX_FIFO_CTRL, AUX_FIFO_CTRL_RESET);

	rtk_edp_write(edp, AUX_IRQ_EVENT, AUX_ALL_IRQ);
	rtk_edp_write(edp, AUXTX_REQ_CMD, (addr >> 16) & 0xFF);
	rtk_edp_write(edp, AUXTX_REQ_ADDR_M, (addr >> 8) & 0xFF);
	rtk_edp_write(edp, AUXTX_REQ_ADDR_L, addr & 0xFF);
	rtk_edp_write(edp, AUXTX_REQ_LEN, (size > 0) ? (size - 1) : 0);

	if (!(msg->request & DP_AUX_I2C_READ)) {
		for (i = 0; i < size; i++)
			rtk_edp_write(edp, AUXTX_REQ_DATA, buffer[i]);
	}

	if (msg->size == 0)
		rtk_edp_update(edp, AUXTX_TRAN_CTRL, TX_START | TX_ADDRONLY,
											TX_START | TX_ADDRONLY);
	else
		rtk_edp_update(edp, AUXTX_TRAN_CTRL, TX_START | TX_ADDRONLY,
											TX_START);
}

static ssize_t rtk_edp_aux_transfer(struct drm_dp_aux *aux,
				     struct drm_dp_aux_msg *msg)
{
	struct rtk_edp *edp = to_rtk_edp(aux);
	int ret = 0;

	if (WARN_ON(msg->size > 16)) {
		DRM_ERROR("edp aux msg size %ld too big\n", msg->size);
		return -E2BIG;
	}

	if (!edp->connected) {
		// dev_info(edp->dev, "edp disconnected, do not do aux transfer\n");
		msg->reply = DP_AUX_NATIVE_REPLY_NACK | DP_AUX_I2C_REPLY_NACK;
		return -ENODEV;
	}

	ensure_clock_enabled(edp);

	edp_aux_transfer(edp, msg);

	ret = edp_aux_isr(edp);
	if (ret < 0) {
		dev_err(edp->dev, "aux transfer error\n");
		return -ETIMEDOUT;
	}

	if ((msg->request & DP_AUX_I2C_READ) && msg->size != 0)
		ret = edp_aux_get_data(edp, msg);
	else
		ret = msg->size;

	rtk_edp_update(edp, AUX_FIFO_CTRL,
		AUX_FIFO_CTRL_ALL, AUX_FIFO_CTRL_RESET);

	if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_WRITE ||
	    (msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_READ) {
		msg->reply = DP_AUX_I2C_REPLY_ACK;
	} else if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_WRITE ||
		 (msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_READ) {
		msg->reply = DP_AUX_NATIVE_REPLY_ACK;
	}

	return ret;
}

static int rtk_edp_register(struct drm_device *drm, struct rtk_edp *edp)
{
	struct drm_encoder *encoder = &edp->encoder;
	struct drm_connector *connector = &edp->connector;
	struct device *dev = edp->dev;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	dev_info(dev, "edp possible_crtcs (0x%x)\n", encoder->possible_crtcs);

	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	drm_encoder_init(drm, encoder, &rtk_edp_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);

	drm_encoder_helper_add(encoder, &rtk_edp_encoder_helper_funcs);

	connector->polled = DRM_CONNECTOR_POLL_HPD;
	drm_connector_init(drm, connector, &rtk_edp_connector_funcs,
			DRM_MODE_CONNECTOR_eDP);
			/* Set to DP connector while testing CTS/Google autotest */
			// DRM_MODE_CONNECTOR_DisplayPort);
	drm_connector_helper_add(connector, &rtk_edp_connector_helper_funcs);

	drm_connector_attach_encoder(connector, encoder);

	return 0;
}

static int rtk_edp_init_properties(struct rtk_edp *edp)
{
	int ret;

	dev_info(edp->dev, "edp: init properties\n");

	if (edp->connector.funcs->reset)
		edp->connector.funcs->reset(&edp->connector);

	ret = drm_connector_attach_max_bpc_property(&edp->connector, 6, 16);
	if (ret) {
		dev_err(edp->dev, "drm edp attach max bpc property fail\n");
		return ret;
	}

	return 0;
}

static int rtk_1920_edp_bind(struct rtk_edp *edp)
{
	struct platform_device *pdev;
	struct device *dev;
	int ret = 0;
	struct regmap *iso_pinctl;
	unsigned int val;
	struct device_node *syscon_np;

	dev = edp->dev;
	pdev = to_platform_device(dev);

	dev_info(dev, "1920 edp: bind\n");

	edp->clk_edp = devm_clk_get(dev, "clk_en_edp");
	if (IS_ERR(edp->clk_edp)) {
		dev_err(dev, "failed to get clock\n");
		return PTR_ERR(edp->clk_edp);
	}

	edp->rstc_edp = devm_reset_control_get(dev, "rstn_edp");
	if (IS_ERR(edp->rstc_edp)) {
		dev_err(dev, "failed to get reset controller\n");
		return PTR_ERR(edp->rstc_edp);
	}

	edp->clk_edptx = devm_clk_get(dev, "clk_en_edptx");
	if (IS_ERR(edp->clk_edptx)) {
		dev_err(dev, "failed to get clock\n");
		return PTR_ERR(edp->clk_edptx);
	}

	edp->rstc_edptx = devm_reset_control_get(dev, "rstn_edptx");
	if (IS_ERR(edp->rstc_edptx)) {
		dev_err(dev, "failed to get reset controller\n");
		return PTR_ERR(edp->rstc_edptx);
	}

	edp->rstc_disp = devm_reset_control_get(dev, "rstn_disp");
	if (IS_ERR(edp->rstc_disp)) {
		dev_err(dev, "failed to get rstc_disp\n");
		return PTR_ERR(edp->rstc_disp);
	}

	edp->rstc_vo = devm_reset_control_get(dev, "rstn_vo");
	if (IS_ERR(edp->rstc_vo)) {
		dev_err(dev, "failed to get rstc_vo\n");
		return PTR_ERR(edp->rstc_vo);
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "edp parse syscon phandle 0 fail");
		return -ENODEV;
	}
	edp->reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(edp->reg_base)) {
		dev_err(dev, "remap syscon 0 to reg_base fail");
		of_node_put(syscon_np);
		return PTR_ERR(edp->reg_base);
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 1);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "edp parse syscon phandle 1 fail");
		return -ENODEV;
	}
	edp->crt_reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(edp->crt_reg_base)) {
		dev_err(dev, "remap syscon 1 to crt_reg_base fail");
		of_node_put(syscon_np);
		return PTR_ERR(edp->crt_reg_base);
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 2);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "edp parse syscon phandle 2 fail");
		return -ENODEV;
	}
	edp->edp_wrapper_reg_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(edp->edp_wrapper_reg_base)) {
		dev_err(dev, "remap syscon 2 to edp_wrapper_reg_base fail");
		of_node_put(syscon_np);
		return PTR_ERR(edp->edp_wrapper_reg_base);
	}

	edp->iso_base = syscon_regmap_lookup_by_phandle(dev->of_node, "realtek,iso");
	if (IS_ERR(edp->iso_base)) {
		DRM_ERROR("couldn't get iso register base address\n");
		return PTR_ERR(edp->iso_base);
	}

	edp->check_connector_limit = of_property_read_bool(dev->of_node,
												 "check-connector-limit");
	if (edp->check_connector_limit)
		dev_info(dev, "check connector limit\n");

	ret = of_property_read_u32(dev->of_node, "max-clock-k",
				&edp->max_clock_k);
	if (ret < 0)
		edp->max_clock_k = RTK_EDP_MAX_CLOCK_K;
	dev_info(dev, "max-clock-k=%u\n", edp->max_clock_k);

	init_completion(&edp->comp);
#ifdef CONFIG_CHROME_PLATFORMS
	INIT_WORK(&edp->retrain_link_work, rtk_edp_retrain_link_worker);
#endif

	edp->aux_irq = platform_get_irq(pdev, 0);
	if (edp->aux_irq < 0) {
		dev_err(dev, "can't get aux irq resource\n");
		return -ENODEV;
	}

	irq_set_irq_type(edp->aux_irq, IRQ_TYPE_LEVEL_HIGH);
	ret = devm_request_irq(dev, edp->aux_irq, rtk_edp_aux_irq,
			       IRQF_SHARED, dev_name(dev), edp);
	if (ret) {
		dev_err(dev, "can't request aux irq resource\n");
		return -ENODEV;
	}

	edp->aux.name = "eDP-AUX";
	edp->aux.transfer = rtk_edp_aux_transfer;
	edp->aux.dev = dev;
	edp->aux.drm_dev = edp->drm_dev;
	ret = drm_dp_aux_register(&edp->aux);
	if (ret) {
		dev_err(edp->dev, "drm dp aux register fail\n");
		return ret;
	}


	iso_pinctl = syscon_regmap_lookup_by_phandle(dev->of_node,
												"realtek,pinctrl");
	if (IS_ERR(iso_pinctl)) {
		DRM_ERROR("fail to get iso pinctl reg\n");
		return PTR_ERR(iso_pinctl);
	}

	// Disable gpio 128 pull up/down function
	regmap_read(iso_pinctl, ISO_PFUN7, &val);
	regmap_write(iso_pinctl, ISO_PFUN7,
			(val & ~ISO_MUXPAD4_GPIO_128_PUD_MASK) | ISO_MUXPAD4_GPIO_128_PUD_DISABLE);

	// Mux to edptx_hpd
	if (!edp->hpd_gpio) {
		regmap_read(iso_pinctl, ISO_MUXPAD4, &val);
		regmap_write(iso_pinctl, ISO_MUXPAD4,
			 (val & ~ISO_MUXPAD4_GPIO_128_MASK) | ISO_MUXPAD4_GPIO_128_MUX_EDPTX);
	}

	return ret;
}

static int rtk_edp_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct drm_device *drm = data;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_edp *edp = dev_get_drvdata(dev);
	int ret;

	dev_info(dev, "edp: bind\n");

	edp->drm_dev = drm;
	mutex_init(&edp->lock);

	if (edp->edp_data->bind != NULL) {
		ret = edp->edp_data->bind(edp);
		if (ret < 0) {
			dev_err(edp->dev, "edp data bind fail\n");
			return ret;
		}
	}

	edp->vcc_gpio = devm_gpiod_get_optional(dev, "vcc", GPIOD_OUT_HIGH);
	if (edp->vcc_gpio) {
		if (IS_ERR(edp->vcc_gpio))
			return dev_err_probe(dev, PTR_ERR(edp->vcc_gpio),
						"Could not get vcc gpio\n");
		dev_info(dev, "edp vcc gpio(%d)\n", desc_to_gpio(edp->vcc_gpio));
	} else {
		dev_info(dev, "no vcc-gpios node\n");
	}

	edp->hpd_gpio = devm_gpiod_get_optional(dev, "hpd", GPIOD_IN);
	if (edp->hpd_gpio) {
		if (IS_ERR(edp->hpd_gpio))
			return dev_err_probe(dev, PTR_ERR(edp->hpd_gpio),
						"Could not get hpd gpio\n");
		dev_info(dev, "edp hotplug gpio(%d)\n", desc_to_gpio(edp->hpd_gpio));

		ret = gpiod_direction_input(edp->hpd_gpio);
		if (ret)
			dev_err(dev, "failed to set hpd_gpio direction");

		rtk_edp_config_hpd_gpio(edp);

		ret = rtk_edp_start_detect_hpd(edp);
		if (ret)
			return -ENODEV;

		INIT_DELAYED_WORK(&edp->hpd_gpio_work, rtk_edp_hpd_gpio_worker);
	} else {
		dev_info(dev, "no hpd_gpios node\n");
	}

	of_property_read_u32(dev->of_node, "assr-en", &edp->assr_en);

	edp->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if (priv->krpc_second == 1)
		edp->rpc_info_vo = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		edp->rpc_info_vo = NULL;

	dev_info(dev, "edp->rpc_info (%p), edp->rpc_info_vo (%p), assr_en (%d)\n",
		edp->rpc_info, edp->rpc_info_vo, edp->assr_en);

	ret = rtk_edp_register(drm, edp);
	if (ret) {
		dev_err(edp->dev, "drm edp register fail\n");
		return ret;
	}

	ret = rtk_edp_init_properties(edp);
	if (ret) {
		dev_err(edp->dev, "rtk_edp_init_properties fail\n");
		return ret;
	}

	if (edp->hpd_gpio)
		schedule_delayed_work(&edp->hpd_gpio_work, 0);
	else
		rtk_edp_start_hpd_thread(edp);

	if (edp->edp_data->init != NULL)
		edp->edp_data->init(edp);

	return 0;
}

static void rtk_edp_unbind(struct device *dev, struct device *master,
			     void *data)
{
	struct rtk_edp *edp = dev_get_drvdata(dev);

	kfree(edp->edid);
	edp->edid = NULL;
}

static const struct component_ops rtk_edp_ops = {
	.bind	= rtk_edp_bind,
	.unbind	= rtk_edp_unbind,
};

static int rtk_edp_suspend(struct device *dev)
{
	struct rtk_edp *edp = dev_get_drvdata(dev);

	if (!edp)
		return 0;

	if (edp->hpd_gpio)
		rtk_dptx_stop_detect_hpd(edp);
	else
		rtk_edp_stop_hpd_thread(edp);

	edp->connected = false;

	if (rtk_edp_is_clock_on(edp))
		rtk_edp_shutdown(edp);

	if (edp->vcc_gpio && gpiod_get_value(edp->vcc_gpio))
		gpiod_set_value_cansleep(edp->vcc_gpio, 0);

	return 0;
}

static int rtk_edp_resume(struct device *dev)
{
	struct rtk_edp *edp = dev_get_drvdata(dev);

	if (!edp)
		return 0;

	rtk_edp_power_on_panel(edp);

	if (edp->hpd_gpio) {
		schedule_delayed_work(&edp->hpd_gpio_work, 0);
		rtk_edp_start_detect_hpd(edp);
	} else {
		rtk_edp_start_hpd_thread(edp);
	}

	return 0;
}

static const struct dev_pm_ops rtk_edp_pm_ops = {
	.suspend    = rtk_edp_suspend,
	.resume     = rtk_edp_resume,
};

static const struct rtk_edp_data rtk_1920_edp_data = {
	.init = rtk_1920_edp_init,
	.bind = rtk_1920_edp_bind,
	.get_modes = rtk_1920_edp_get_modes,
	.init_hw = rtk_edp_init_hw,
};

static const struct rtk_edp_data rtk_1861_edp_data = {
	.init = NULL,
	.bind = NULL,
	.get_modes = rtk_1861_edp_get_modes,
	.init_hw = NULL,
};

static const struct of_device_id rtk_edp_dt_ids[] = {
	{
		.compatible = "realtek,rtk-edp",
		.data = &rtk_1920_edp_data,
	},
	{
		.compatible = "realtek,rtk-1861-edp",
		.data = &rtk_1861_edp_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_edp_dt_ids);

static int rtk_edp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_edp *edp;
	const struct of_device_id *match;

	dev_info(dev, "edp: probe\n");

	edp = devm_kzalloc(dev, sizeof(*edp), GFP_KERNEL);
	if (!edp)
		return -ENOMEM;

	edp->dev = dev;
	dev_set_drvdata(dev, edp);

	match = of_match_node(rtk_edp_dt_ids, dev->of_node);
	if (!match)
		return -ENODEV;

	edp->edp_data = match->data;

	return component_add(&pdev->dev, &rtk_edp_ops);
}

static int rtk_edp_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_edp_ops);
	return 0;
}

struct platform_driver rtk_edp_driver = {
	.probe  = rtk_edp_probe,
	.remove = rtk_edp_remove,
	.driver = {
		.name = "rtk-edp",
		.of_match_table = rtk_edp_dt_ids,
#if IS_ENABLED(CONFIG_PM)
		.pm = &rtk_edp_pm_ops,
#endif
	},
};

MODULE_AUTHOR("Ray Tang <ray.tang@realtek.com>");
MODULE_DESCRIPTION("Realtek Embedded DisplayPort Driver");
MODULE_LICENSE("GPL v2");
