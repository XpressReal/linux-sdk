// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek HDMI driver
 *
 * Copyright (c) 2021 Realtek Semiconductor Corp.
 */

#include <drm/display/drm_hdmi_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_print.h>

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/extcon-provider.h>
#include <linux/sys_soc.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/mfd/syscon.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include <media/cec-notifier.h>
#include <uapi/linux/i2c.h>

#include "rtk_hdmi.h"
#include "rtk_hdmi_reg.h"
#include "rtk_hdmi_new_reg.h"
#include "rtk_crt_reg.h"

#define I2C_BUS_ID 1

#define OFF_FLAG_NONE  0
#define OFF_FLAG_HDCP       (1 << 0)
#define OFF_FLAG_AVMUTE     (1 << 1)
#define OFF_FLAG_DIS_AUDIO  (1 << 2)
#define OFF_FLAG_NORMAL (OFF_FLAG_HDCP | OFF_FLAG_AVMUTE | OFF_FLAG_DIS_AUDIO)
#define OFF_FLAG_SKIP_AVMUTE (OFF_FLAG_HDCP | OFF_FLAG_DIS_AUDIO)

#define RTK_HDMI_MAX_CLOCK_K 594000 // 4096x2160p60
static const struct of_device_id audio_match_table[] = {
	{ .compatible = "realtek,audio-out", },
	{ .compatible = "realtek,rtd1619b-hifi-afe",},
	{ .compatible = "realtek,rtd1920s-hifi-afe", },
	{ /*sentinel*/ }
};

static const struct soc_device_attribute rtk_soc_stark[] = {
    { .family = "Realtek Stark", },
    { /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_kent[] = {
	{ .family = "Realtek Kent", },
	{ /* empty */ }
};

static const unsigned int rtk_hdmi_cable[] = {
	EXTCON_DISP_DVI,
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
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
	/* Use forbidden VIC for non-CEA format */
	VIC_1920X1080P144 = 128,
	/* OVT, Bit[13:8]=RID Bit[7:5]=0 Bit[4:0]=FR */
	VIC_2560X1080P144 = 1551, /* RID=6, FR=b'01111 */
	VIC_2560X1440P120 = 2061, /* RID=8, FR=b'01101 */
	/* DMT, Bit[14]=1 Bit[13:8]=0 Bit[7:0]=DMT ID code */
	VIC_1280X800P60RB  = 0x0000401B,
	VIC_1280X800P60    = 0x0000401C,
	VIC_1440X900P60RB  = 0x0000402E,
	VIC_1440X900P60    = 0x0000402F,
	VIC_1680X1050P60RB = 0x00004039,
	VIC_1680X1050P60   = 0x0000403A,
	VIC_1600X900P60    = 0x00004053,
};

static struct rtk_hdmi_pll_table pll_mode_list[] = {
	{ PLL_27MHZ,  "pll-27m" },
	{ PLL_27x1p25, "pll-27x1p25" },
	{ PLL_27x1p5, "pll-27x1p5" },
	{ PLL_54MHZ, "pll-54m" },
	{ PLL_54x1p25, "pll-54x1p25" },
	{ PLL_54x1p5, "pll-54x1p5" },
	{ PLL_59p4MHZ, "pll-59p4m" },
	{ PLL_59p4x1p25, "pll-59p4x1p25" },
	{ PLL_59p4x1p5, "pll-59p4x1p5" },
	{ PLL_74p25MHZ, "pll-74p25m" },
	{ PLL_74p25x1p25, "pll-74p25x1p25" },
	{ PLL_74p25x1p5, "pll-74p25x1p5" },
	{ PLL_83p5MHZ, "pll-83p5m" },
	{ PLL_106p5MHZ, "pll-106p5m" },
	{ PLL_108MHZ, "pll-108m" },
	{ PLL_146p25MHZ, "pll-146p25m" },
	{ PLL_148p5MHZ, "pll-148p5m" },
	{ PLL_148p5x1p25, "pll-148p5x1p25" },
	{ PLL_148p5x1p5, "pll-148p5x1p5" },
	{ PLL_297MHZ, "pll-297m" },
	{ PLL_297x1p25, "pll-297x1p25" },
	{ PLL_297x1p5, "pll-297x1p5" },
	{ PLL_348p5MHZ, "pll-348p5m" },
	{ PLL_594MHZ_420, "pll-594m_y420" },
	{ PLL_594MHZ_420x1p25, "pll-594m_y420x1p25" },
	{ PLL_594MHZ_420x1p5, "pll-594m_y420x1p5" },
	{ PLL_453p456MHZ, "pll-453p456m" },
	{ PLL_486p048MHZ, "pll-486p048m" },
	{ PLL_594MHZ, "pll-594m" },
	{ PLL_INVALIDMHZ, "pll-invalid" },
};

static const struct drm_prop_enum_list colorspace_mode_list[] = {
	{ HDMI_COLORSPACE_RGB, "RGB" },
	{ HDMI_COLORSPACE_YUV422, "Y422" },
	{ HDMI_COLORSPACE_YUV444, "Y444" },
	{ HDMI_COLORSPACE_YUV420, "Y420" },
};

static const struct drm_prop_enum_list hdr_mode_list[] = {
	{ HDR_CTRL_NA0, "Not_Applicable" },
	{ HDR_CTRL_DV_ON, "DV_ON" },
	{ HDR_CTRL_SDR, "SDR" },
	{ HDR_CTRL_HDR_GAMMA, "HDR_GAMMA" },
	{ HDR_CTRL_HDR10, "HDR10" },
	{ HDR_CTRL_HLG, "HLG" },
	{ HDR_CTRL_INPUT, "INPUT" },
	{ HDR_CTRL_DV_LOW_LATENCY_12b_YUV422, "DV_LL_12b_Y422" },
	{ HDR_CTRL_DV_LOW_LATENCY_10b_YUV444, "DV_LL_10b_Y444" },
	{ HDR_CTRL_DV_LOW_LATENCY_10b_RGB444, "DV_LL_10b_RGB" },
	{ HDR_CTRL_DV_LOW_LATENCY_12b_YUV444, "DV_LL_12b_Y444" },
	{ HDR_CTRL_DV_LOW_LATENCY_12b_RGB444, "DV_LL_12b_RGB" },
	{ HDR_CTRL_DV_ON_INPUT, "DV_ON_INPUT" },
	{ HDR_CTRL_DV_ON_LOW_LATENCY_12b422_INPUT, "DV_LL_12b422_INPUT" },
	{ HDR_CTRL_INPUT_BT2020, "INPUT_BT2020" },
	{ HDR_CTRL_DV_ON_HDR10_VS10, "DV_ON_HDR10_VS10"},
	{ HDR_CTRL_DV_ON_SDR_VS10, "DV_ON_SDR_VS10"},
};

static const struct drm_prop_enum_list hdmi_5v_status_list[] = {
	{ HDMI_5V_DISABLE, "Disable" },
	{ HDMI_5V_ENABLE, "Enable" },
};

static const struct drm_prop_enum_list hdmi_ao_status_list[] = {
	{ HDMI_AO_DISABLE, "Disable" },
	{ HDMI_AO_ENABLE, "Enable" },
	{ HDMI_AO_AUTO, "Auto" },
};

static const struct drm_prop_enum_list allm_status_list[] = {
	{ ALLM_DISABLE, "Disable" },
	{ ALLM_ENABLE, "Enable" },
	{ ALLM_UNSUPPORTED, "Unsupported" },
};

static const struct drm_prop_enum_list qms_status_list[] = {
	{ QMS_VRR_DISABLE, "Disable" },
	{ QMS_VRR_EN_VRR, "Enable_VRR" },
	{ QMS_VRR_EN_QMS, "Enable_QMS" },
	{ QMS_VRR_UNSUPPORTED, "Unsupported" },
};

static const struct drm_prop_enum_list vrr_rate_list[] = {
	{ RATE_60HZ, "RATE_60HZ" },
	{ RATE_50HZ, "RATE_50HZ" },
	{ RATE_48HZ, "RATE_48HZ" },
	{ RATE_24HZ, "RATE_24HZ" },
	{ RATE_59HZ, "RATE_59HZ" },
	{ RATE_47HZ, "RATE_47HZ" },
	{ RATE_30HZ, "RATE_30HZ" },
	{ RATE_29HZ, "RATE_29HZ" },
	{ RATE_25HZ, "RATE_25HZ" },
	{ RATE_23HZ, "RATE_23HZ" },
	{ RATE_BASE, "RATE_BASE" },
	{ RATE_UNSPECIFIED, "UNSPECIFIED" },
};

static const struct drm_prop_enum_list hdmi_auto_mode_list[] = {
	{ HDMI_AUTO_DISABLE, "Disable" },
	{ HDMI_AUTO_SDR_8B, "SDR_8b" },
	{ HDMI_AUTO_SDR_DC, "SDR_DC" },
	{ HDMI_AUTO_INPUT, "HDR_INPUT" },
};

static const struct drm_prop_enum_list skip_avmute_status_list[] = {
	{ SKIP_DISABLE, "Disable" },
	{ SKIP_ENABLE, "Enable" },
};

static const struct drm_prop_enum_list hdmi_scan_mode_list[] = {
	{ HDMI_SCAN_MODE_NONE, "None" },
	{ HDMI_SCAN_MODE_OVERSCAN, "Overscan" },
	{ HDMI_SCAN_MODE_UNDERSCAN, "Underscan" },
	{ HDMI_SCAN_MODE_RESERVED, "Reserved" },
};

static struct drm_display_mode rtk_non_cea_modes[] = {
	/* 1920x720@60Hz */
	{ DRM_MODE("1920x720", DRM_MODE_TYPE_DRIVER, 89162, 1920, 1921,
			1941, 1992, 0, 720, 721, 726, 746, 0,
			DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC) },
};

static struct drm_display_mode rtk_default_modes[] = {
	/* 2 - 720x480@60Hz 4:3 */
	{ DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736,
		   798, 858, 0, 480, 489, 495, 525, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
	/* 17 - 720x576@50Hz 4:3 */
	{ DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732,
		   796, 864, 0, 576, 581, 586, 625, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
};

#define LINE_BUF_SIZE  (EDID_LENGTH * 2 + 2)
static void rtk_hdmi_edid_dump(struct rtk_hdmi *hdmi, u8 *buf, u32 len)
{
	u32 i, j;
	u8 *edid_row;

	if ((len % EDID_LENGTH) != 0) {
		dev_err(hdmi->dev, "%s failed, invalid len=%u", __func__, len);
		return;
	}

	edid_row = buf;

	for (i = 0; i < len; i += EDID_LENGTH) {
		u8 linebuf[LINE_BUF_SIZE] = {0};
		u32 ret_count = 0;
		u32 lx = 0;

		for (j = 0; j < EDID_LENGTH; j++) {
			ret_count = snprintf(linebuf + lx, LINE_BUF_SIZE - lx,
					"%02x", edid_row[j]);
			lx += ret_count;
		}

		dev_info(hdmi->dev, "%s", linebuf);

		edid_row += EDID_LENGTH;
	}
}

void rtk_content_type_commit_state(struct drm_connector *connector,
		unsigned int old_ct, unsigned int new_ct)
{
	struct rtk_hdmi *hdmi = to_rtk_hdmi(connector);
	int ret = 0;

	if (old_ct == new_ct)
		return;

	dev_info(hdmi->dev, "Change content_type %u -> %u", old_ct, new_ct);

	ret = rpc_set_infoframe(hdmi->rpc_info, AVI_CONTENT_TYPE, new_ct, 0);
	if (ret)
		dev_err(hdmi->dev, "Fail to set content type, ret=%d\n", ret);
}

int rtk_hdmi_set_5v(struct rtk_hdmi *hdmi,
		enum HDMI_5V_STATUS en_hdmi_5v)
{
	int ret = -ENODEV;

	if (IS_ERR_OR_NULL(hdmi->hdmi5v_gpio) && IS_ERR_OR_NULL(hdmi->supply))
		goto exit;

	if (IS_ERR_OR_NULL(hdmi->hdmi5v_gpio)) {

		if (en_hdmi_5v == HDMI_5V_ENABLE)
			ret = regulator_enable(hdmi->supply);
		else
			ret = regulator_disable(hdmi->supply);

		if (ret) {
			dev_err(hdmi->dev, "failed to %s hdmi_5v via regulator",
				(en_hdmi_5v == HDMI_5V_ENABLE) ? "enable":"disable");
			goto exit;
		}
	} else {
		gpiod_set_value(hdmi->hdmi5v_gpio,
			(en_hdmi_5v == HDMI_5V_ENABLE) ? 1:0);
	}

	dev_info(hdmi->dev, "HDMI_5V %s",
		(en_hdmi_5v == HDMI_5V_ENABLE) ? "enabled":"disabled");

	ret = 0;

exit:
	return ret;
}

static bool is_rtk_non_cea_mode(struct drm_display_mode *mode)
{
	int i;

	if (!soc_device_match(rtk_soc_stark))
		return false;

	for (i = 0; i < ARRAY_SIZE(rtk_non_cea_modes); i++) {
		if (mode->hdisplay == rtk_non_cea_modes[i].hdisplay &&
			mode->vdisplay == rtk_non_cea_modes[i].vdisplay)
			return true;
	}

	return false;
}

static bool rtk_hdmi_valid_vic(u8 vic)
{
	bool valid;

	switch (vic) {
	case VIC_720X480P60:
	case VIC_1280X720P60:
	case VIC_1920X1080I60:
	case VIC_720X480I60:
	case VIC_1920X1080P60:
	case VIC_720X576P50:
	case VIC_1280X720P50:
	case VIC_1920X1080I50:
	case VIC_720X576I50:
	case VIC_1920X1080P50:
	case VIC_1920X1080P24:
	case VIC_1920X1080P25:
	case VIC_1920X1080P30:
	case VIC_1280X720P24:
	case VIC_1280X720P25:
	case VIC_1280X720P30:
        /* case VIC_1920X1080P120: */
	case VIC_3840X2160P24:
	case VIC_3840X2160P25:
	case VIC_3840X2160P30:
	case VIC_3840X2160P50:
	case VIC_3840X2160P60:
	case VIC_4096X2160P24:
	case VIC_4096X2160P25:
	case VIC_4096X2160P30:
	case VIC_4096X2160P50:
	case VIC_4096X2160P60:
		valid = true;
		break;
	default:
		valid = false;
	}

	return valid;
}

static unsigned int
non_cea_to_vo_standard(struct rtk_hdmi *hdmi, struct drm_display_mode *mode)
{
	if (mode->hdisplay == 1920 && mode->vdisplay == 720)
		return VO_STANDARD_HDTV_1920_720P_60;

	return VO_STANDARD_HDTV_1080P_60;
}

static unsigned int
vic_to_vo_standard(unsigned char vic, struct rtk_hdmi *hdmi, bool is_fractional)
{
	unsigned int ret_val;
	enum hdmi_colorspace rgb_or_yuv;
	u8 requested_bpc;

	requested_bpc = hdmi->connector.state->max_requested_bpc;
	rgb_or_yuv = hdmi->rgb_or_yuv;

	switch (vic) {
	case VIC_720X480P60:
		ret_val = VO_STANDARD_NTSC_J;
		break;
	case VIC_1280X720P60:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_720P_59;
		else
			ret_val = VO_STANDARD_HDTV_720P_60;
		break;
	case VIC_1920X1080I60:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_1080I_59;
		else
			ret_val = VO_STANDARD_HDTV_1080I_60;
		break;
	case VIC_720X480I60:
		ret_val = VO_STANDARD_NTSC_J;
		break;
	case VIC_1920X1080P60:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_1080P_59;
		else
			ret_val = VO_STANDARD_HDTV_1080P_60;
		break;
	case VIC_720X576P50:
		ret_val = VO_STANDARD_PAL_I;
		break;
	case VIC_1280X720P50:
		ret_val = VO_STANDARD_HDTV_720P_50;
		break;
	case VIC_1920X1080I50:
		ret_val = VO_STANDARD_HDTV_1080I_50;
		break;
	case VIC_720X576I50:
		ret_val = VO_STANDARD_PAL_I;
		break;
	case VIC_1920X1080P50:
		ret_val = VO_STANDARD_HDTV_1080P_50;
		break;
	case VIC_1920X1080P24:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_1080P_23;
		else
			ret_val = VO_STANDARD_HDTV_1080P_24;
		break;
	case VIC_1920X1080P25:
		ret_val = VO_STANDARD_HDTV_1080P_25;
		break;
	case VIC_1920X1080P30:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_1080P_29;
		else
			ret_val = VO_STANDARD_HDTV_1080P_30;
		break;
	case VIC_1280X720P24:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_720P_P23;
		else
			ret_val = VO_STANDARD_HDTV_720P_P24;
		break;
	case VIC_1280X720P25:
		ret_val = VO_STANDARD_HDTV_720P_P25;
		break;
	case VIC_1280X720P30:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_720P_P29;
		else
			ret_val = VO_STANDARD_HDTV_720P_P30;
		break;
	case VIC_1920X1080P120:
		ret_val = VO_STANDARD_HDTV_1080P_120;
		break;
	case VIC_3840X2160P24:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_2160P_23;
		else
			ret_val = VO_STANDARD_HDTV_2160P_24;
		break;
	case VIC_3840X2160P25:
		ret_val = VO_STANDARD_HDTV_2160P_25;
		break;
	case VIC_3840X2160P30:
		if (is_fractional)
			ret_val = VO_STANDARD_HDTV_2160P_29;
		else
			ret_val = VO_STANDARD_HDTV_2160P_30;
		break;
	case VIC_3840X2160P50:
		if (rgb_or_yuv == HDMI_COLORSPACE_YUV420)
			ret_val = VO_STANDARD_HDTV_2160P_50_420;
		else if ((rgb_or_yuv == HDMI_COLORSPACE_YUV422) &&
					(requested_bpc >= 10))
			ret_val = VO_STANDARD_HDTV_2160P_50_422_12bit;
		else
			ret_val = VO_STANDARD_HDTV_2160P_50;
		break;
	case VIC_3840X2160P60:
		if (rgb_or_yuv == HDMI_COLORSPACE_YUV420) {
			if (is_fractional)
				ret_val = VO_STANDARD_HDTV_2160P_59_420;
			else
				ret_val = VO_STANDARD_HDTV_2160P_60_420;
		} else if ((rgb_or_yuv == HDMI_COLORSPACE_YUV422)
					&& (requested_bpc >= 10)) {
			if (is_fractional)
				ret_val = VO_STANDARD_HDTV_2160P_59_422_12bit;
			else
				ret_val = VO_STANDARD_HDTV_2160P_60_422_12bit;
		} else {
			if (is_fractional)
				ret_val = VO_STANDARD_HDTV_2160P_59;
			else
				ret_val = VO_STANDARD_HDTV_2160P_60;
		}
		break;
	case VIC_4096X2160P24:
		ret_val = VO_STANDARD_HDTV_4096_2160P_24;
		break;
	case VIC_4096X2160P25:
		ret_val = VO_STANDARD_HDTV_4096_2160P_25;
		break;
	case VIC_4096X2160P30:
		ret_val = VO_STANDARD_HDTV_4096_2160P_30;
		break;
	case VIC_4096X2160P50:
		if (rgb_or_yuv == HDMI_COLORSPACE_YUV420)
			ret_val = VO_STANDARD_HDTV_4096_2160P_50_420;
		else
			ret_val = VO_STANDARD_HDTV_4096_2160P_50;
		break;
	case VIC_4096X2160P60:
		if (rgb_or_yuv == HDMI_COLORSPACE_YUV420)
			ret_val = VO_STANDARD_HDTV_4096_2160P_60_420;
		else
			ret_val = VO_STANDARD_HDTV_4096_2160P_60;
		break;
	default:
		ret_val = VO_STANDARD_HDTV_1080P_60;
		break;
	} /* end of switch (vic) */

	return ret_val;
}

static bool is_hdmi_clock_on(struct rtk_hdmi *hdmi)
{
	unsigned int pll_mask;
	unsigned int pll_hdmi;
	bool clk_on;

	if (hdmi->is_new_mac)
		pll_mask = 0xCF;
	else
		pll_mask = 0xBF;

	clk_on = false;

	if (__clk_is_enabled(hdmi->clk_hdmi) &&
		(!reset_control_status(hdmi->reset_hdmi))) {
		regmap_read(hdmi->crtreg, SYS_PLL_HDMI, &pll_hdmi);
		if ((pll_hdmi&pll_mask) == pll_mask)
			clk_on = true;
	}

	return clk_on;
}

static u32 get_ms_per_frame(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode)
{
	u32 molecular;
	u32 mspf;

	if (mode == NULL)
		return 0;

	molecular = mode->htotal * mode->vtotal;

	mspf = molecular / mode->clock;

	/* minimum frame rate 23.98Hz, max 42ms/frame */
	return max(mspf, (u32)42);
}

static bool is_fractional_mode(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode)
{
	bool is_fractional;
	u32 fp10s;

	is_fractional = false;

	if (mode == NULL)
		return is_fractional;

	fp10s = get_frame_per_10s(hdmi, mode);

	if ((fp10s % 10) != 0)
		is_fractional = true;

	return is_fractional;
}

static bool is_same_tvsystem(struct rtk_hdmi *hdmi,
	struct rpc_config_tv_system *tvsystem1,
	struct rpc_config_tv_system *tvsystem2)
{
	u32 color1, color2;
	u32 depth1, depth2;

	if (tvsystem1 == NULL || tvsystem2 == NULL) {
		dev_err(hdmi->dev, "%s failed, tvsystem is NULL", __func__);
		return false;
	}

	color1 = (tvsystem1->info_frame.dataByte1 >> 5) & 0x3;
	color2 = (tvsystem2->info_frame.dataByte1 >> 5) & 0x3;

	/* dataInt0 [Bit5:2] 4-24bits 5-30bit 6-36bits [Bit1] deep color */
	depth1 = (tvsystem1->info_frame.dataInt0 >> 2) & 0xF;
	depth2 = (tvsystem2->info_frame.dataInt0 >> 2) & 0xF;

	if (depth1 == 0)
		depth1 = 4;

	if (depth2 == 0)
		depth2 = 4;

	if ((tvsystem1->videoInfo.standard == tvsystem2->videoInfo.standard) &&
		(tvsystem1->info_frame.hdmiMode == tvsystem2->info_frame.hdmiMode) &&
		(color1 == color2) && (depth1 == depth2) &&
		(tvsystem1->videoInfo.enProg == tvsystem2->videoInfo.enProg) &&
		(tvsystem1->info_frame.hdr_ctrl_mode == tvsystem2->info_frame.hdr_ctrl_mode))
		return true;

	DRM_DEBUG_DRIVER("standard %u->%u, hdmiMode %u->%u, color %u->%u",
		tvsystem1->videoInfo.standard, tvsystem2->videoInfo.standard,
		tvsystem1->info_frame.hdmiMode, tvsystem2->info_frame.hdmiMode,
		color1, color2);

	DRM_DEBUG_DRIVER("depth %u->%u, enProg %u->%u, hdr %u->%u",
		depth1, depth2,
		tvsystem1->videoInfo.enProg, tvsystem2->videoInfo.enProg,
		tvsystem1->info_frame.hdr_ctrl_mode, tvsystem2->info_frame.hdr_ctrl_mode);

	return false;
}

static bool rtk_scdc_get_scrambling_status(struct i2c_adapter *adapter)
{
	u8 status;
	int ret;

	ret = drm_scdc_readb(adapter, SCDC_SCRAMBLER_STATUS, &status);
	if (ret < 0) {
		DRM_DEBUG_KMS("Failed to read scrambling status: %d\n", ret);
		return false;
	}

	return status & SCDC_SCRAMBLING_STATUS;
}

static bool is_same_scramble_status(struct rtk_hdmi *hdmi, unsigned int tmdsconfig)
{
	bool sink_status = false;
	bool cur_status = false;

#if 1 // TODO: Remove this workaround when fix symbol issue
	sink_status = rtk_scdc_get_scrambling_status(hdmi->ddc);
#else
	sink_status = drm_scdc_get_scrambling_status(hdmi->ddc);
#endif
	cur_status = tmdsconfig & SCDC_SCRAMBLING_ENABLE;

	dev_info(hdmi->dev, "tv scramb: %u, tmdsconfig scramb: %u", sink_status, cur_status);

	return sink_status & cur_status;
}

static unsigned int get_scdc_tmdsconfig(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode, u8 bpc)
{
	struct drm_scdc *scdc = &hdmi->connector.display_info.hdmi.scdc;
	int clock;
	unsigned int tmdsconfig = 0;

	clock = mode->clock;

	if (hdmi->rgb_or_yuv == HDMI_COLORSPACE_YUV420)
		clock = clock/2;

	if (hdmi->rgb_or_yuv != HDMI_COLORSPACE_YUV422) {
		if (bpc == 12)
			clock += clock/4;
		else if (bpc == 10)
			clock += clock/2;
	}

	if (clock > 340000) {
		tmdsconfig = (SCDC_TMDS_BIT_CLOCK_RATIO_BY_40 | SCDC_SCRAMBLING_ENABLE);
	} else if (scdc->scrambling.low_rates &&
			(hdmi->rgb_or_yuv == HDMI_COLORSPACE_YUV420)) {
		tmdsconfig = SCDC_SCRAMBLING_ENABLE;
	} else if (scdc->scrambling.low_rates && hdmi->lr_scramble) {
		tmdsconfig = SCDC_SCRAMBLING_ENABLE;
	}

	return tmdsconfig;

}

static void send_scdc_tmdsconfig(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode, unsigned int tmdsconfig)
{
	struct drm_display_info *display_info = &hdmi->connector.display_info;
	struct drm_scdc *scdc = &hdmi->connector.display_info.hdmi.scdc;
	struct drm_connector *connector = &hdmi->connector;
	bool clock_ratio = false;
	bool en_scramb = false;

	if (tmdsconfig & SCDC_SCRAMBLING_ENABLE) {
		en_scramb = true;
		if (tmdsconfig & SCDC_TMDS_BIT_CLOCK_RATIO_BY_40)
			clock_ratio = true;
	}

	if ((display_info->color_formats & DRM_COLOR_FORMAT_YCBCR420) ||
		(hdmi->hdcp.sink_hdcp_ver == HDCP_2x) ||
		(hdmi->is_force_on)) {
		scdc->supported = 1;
	}

	dev_info(hdmi->dev, "clock=%d, edid_scdc=%u, clock_ratio=%u, en_scramb=%u",
		mode->clock, scdc->supported, clock_ratio, en_scramb);

	if (en_scramb || scdc->supported) {
		drm_scdc_set_high_tmds_clock_ratio(connector, clock_ratio);
		drm_scdc_set_scrambling(connector, en_scramb);
	}
}

static void rtk_hdmi_send_avmute(struct rtk_hdmi *hdmi, unsigned char mute)
{
	u32 uspf;

	if (hdmi->ext_flags & RTK_EXT_DIS_AVMUTE) {
		dev_info(hdmi->dev, "Skip AVmute control");
		return;
	}

	if (!is_hdmi_clock_on(hdmi))
		return;

	/*
	 * CTA-861 recommendation:
	 * Wait a minimum of 3 frames after SetAvmute before stopping video timing.
	 * And wait a minimum of 5 frames after video timing change before ClearAvmute.
	 */
	uspf = get_ms_per_frame(hdmi, &hdmi->previous_mode) * 1000;

	if (!mute)
		usleep_range(uspf * 5, uspf * 5 + 100);

	dev_info(hdmi->dev, "%s AVmute", mute ? "Set":"Clear");

	if (hdmi->is_new_mac) {
		unsigned int reg_val;

		regmap_read(hdmi->hdmireg, HDMI_NEW_PKT_GCP0, &reg_val);
		reg_val = reg_val & ~(HDMI_NEW_PKT_GCP0_gcp_en_mask |
				HDMI_NEW_PKT_GCP0_set_avmute_mask |
				HDMI_NEW_PKT_GCP0_clr_avmute_mask);

		regmap_write(hdmi->hdmireg, HDMI_NEW_PKT_GCP0,
			reg_val |
			HDMI_NEW_PKT_GCP0_gcp_en(1) |
			HDMI_NEW_PKT_GCP0_set_avmute(mute) |
			HDMI_NEW_PKT_GCP0_clr_avmute(!mute));
	} else {
		regmap_write(hdmi->hdmireg, HDMI_GCPCR,
			HDMI_GCPCR_enablegcp(1) |
			HDMI_GCPCR_gcp_clearavmute(1) |
			HDMI_GCPCR_gcp_setavmute(1) |
			HDMI_GCPCR_write_data(0));

		regmap_write(hdmi->hdmireg, HDMI_GCPCR,
			HDMI_GCPCR_enablegcp(1) |
			HDMI_GCPCR_gcp_clearavmute(!mute) |
			HDMI_GCPCR_gcp_setavmute(mute) |
			HDMI_GCPCR_write_data(1));
	}

	if (mute)
		usleep_range(uspf * 4, uspf * 4 + 100);

}

static int rtk_hdmi_get_rxsense(struct rtk_hdmi *hdmi)
{
	unsigned int reg_val;
	int rxsense;

	switch (hdmi->rxsense_mode) {
	case RXSENSE_PASSIVE_MODE:
		if (is_hdmi_clock_on(hdmi)) {
			regmap_read(hdmi->hdmireg, HDMI_PHY_STATUS, &reg_val);
			rxsense = HDMI_PHY_STATUS_get_Rxstatus(reg_val);
		} else {
			rxsense = HDMI_RXSENSE_UNKNOWN;
		}
		break;
	case RXSENSE_TIMER_MODE:
	case RXSENSE_INTERRUPT_MODE:
		regmap_read(hdmi->topreg, RXST, &reg_val);
		rxsense = RXST_get_Rxstatus(reg_val);
		break;
	default:
		rxsense = HDMI_RXSENSE_UNKNOWN;
	}

	return rxsense;
}

static irqreturn_t rtk_hdmi_rxsense_irq(int irq, void *dev_id)
{
	struct rtk_hdmi *hdmi = dev_id;
	unsigned int reg_val;
	unsigned int rxupdated;

	regmap_read(hdmi->topreg, RXST, &reg_val);
	rxupdated = RXST_get_rxupdated(reg_val);

	if (rxupdated) {
		reg_val = reg_val & (~RXST_rxupdated_mask);
		regmap_write(hdmi->topreg, RXST, reg_val);
		schedule_work(&hdmi->hpd_work);
	}

	return IRQ_HANDLED;
}

static int rtk_hdmi_enable_rxsense_int(struct rtk_hdmi *hdmi)
{
	int ret;
	unsigned int reg_val;

	if (IS_ERR_OR_NULL(hdmi->topreg))
		return -ENXIO;

	ret = devm_request_irq(hdmi->dev, hdmi->rxsense_irq, rtk_hdmi_rxsense_irq,
				IRQF_SHARED, dev_name(hdmi->dev), hdmi);
	if (ret) {
		dev_err(hdmi->dev, "Fail to request rxsense_irq");
		return ret;
	}

	hdmi->rxsense_state = rtk_hdmi_get_rxsense(hdmi);

	regmap_read(hdmi->topreg, RXST, &reg_val);
	reg_val |= RXST_rxsenseint(1);
	regmap_write(hdmi->topreg, RXST, reg_val);

	dev_info(hdmi->dev, "Enable rxsenseint, rxsense_irq=%d",
			hdmi->rxsense_irq);

	return 0;
}

static u8 rtk_hdmi_auto_input(struct rtk_hdmi *hdmi,
			struct drm_display_mode *mode)
{
	struct drm_display_info *info;
	bool vdb_y420;
	bool cmdb_y420;
	u32 clk_khz;
	u8 colorspace;
	u8 vic;

	vic = drm_match_cea_mode(mode);
	info = &hdmi->connector.display_info;
	clk_khz = mode->clock;

	vdb_y420 = drm_mode_is_420_only(info, mode);
	cmdb_y420 = test_bit(vic, info->hdmi.y420_cmdb_modes);

	hdmi->auto_mode = HDMI_AUTO_SDR_8B;

	if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
		colorspace =  HDMI_COLORSPACE_YUV422;
	else if (cmdb_y420)
		colorspace =  HDMI_COLORSPACE_YUV420;
	else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
		colorspace =  HDMI_COLORSPACE_YUV444;
	else
		colorspace =  HDMI_COLORSPACE_RGB;

	if (vdb_y420)
		colorspace = HDMI_COLORSPACE_YUV420;

	if (colorspace == HDMI_COLORSPACE_YUV420)
		clk_khz = clk_khz/2;

	if (colorspace == HDMI_COLORSPACE_YUV422)
		if (clk_khz <= info->max_tmds_clock)
			hdmi->auto_mode = HDMI_AUTO_INPUT;

	if ((clk_khz/4*5) <= info->max_tmds_clock) {
		switch (colorspace) {
		case HDMI_COLORSPACE_RGB:
			if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_30)
				hdmi->auto_mode = HDMI_AUTO_INPUT;
			break;
		case HDMI_COLORSPACE_YUV444:
			if (info->edid_hdmi_ycbcr444_dc_modes & DRM_EDID_HDMI_DC_30)
				hdmi->auto_mode = HDMI_AUTO_INPUT;
			break;
		case HDMI_COLORSPACE_YUV420:
			if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_30)
				hdmi->auto_mode = HDMI_AUTO_INPUT;
			break;
		default:
			break;
		}
	}

	if (hdmi->auto_mode == HDMI_AUTO_INPUT)
		hdmi->hdr_mode = HDR_CTRL_INPUT;

	return colorspace;
}

static u8 rtk_hdmi_get_color_space(struct rtk_hdmi *hdmi,
			struct drm_display_mode *mode)
{
	struct drm_display_info *info;
	bool vdb_y420;
	bool cmdb_y420;
	u8 colorspace;
	u8 vic;

	vic = drm_match_cea_mode(mode);
	info = &hdmi->connector.display_info;

	vdb_y420 = drm_mode_is_420_only(info, mode);
	cmdb_y420 = test_bit(vic, info->hdmi.y420_cmdb_modes);

	if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
		colorspace =  HDMI_COLORSPACE_YUV444;
	else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
		colorspace =  HDMI_COLORSPACE_YUV422;
	else if (cmdb_y420)
		colorspace =  HDMI_COLORSPACE_YUV420;
	else
		colorspace =  HDMI_COLORSPACE_RGB;

	if (((hdmi->auto_mode == HDMI_AUTO_SDR_DC) && cmdb_y420) ||
		vdb_y420)
		colorspace = HDMI_COLORSPACE_YUV420;

	return colorspace;
}

static u8 rtk_hdmi_get_bpc(struct rtk_hdmi *hdmi,
			struct drm_display_mode *mode)
{
	u32 clk_khz;
	u8 bpc = 8;
	u8 requested_bpc;
	u8 colorspace;
	struct drm_display_info *info;

	clk_khz = mode->clock;
	colorspace = hdmi->rgb_or_yuv;
	info = &hdmi->connector.display_info;

	if (hdmi->auto_mode == HDMI_AUTO_INPUT)
		requested_bpc = 10;
	else
		requested_bpc = hdmi->connector.state->max_requested_bpc;

	if ((mode->clock >= 593000) &&
		(colorspace != HDMI_COLORSPACE_YUV422) &&
		(colorspace != HDMI_COLORSPACE_YUV420))
		goto exit;

	if (hdmi->auto_mode == HDMI_AUTO_SDR_8B)
		goto exit;

	if (hdmi->auto_mode == HDMI_AUTO_SDR_DC)
		requested_bpc = 12;

	if ((hdmi->is_force_on) ||
		(colorspace == HDMI_COLORSPACE_YUV422)) {
		bpc = requested_bpc;
		goto exit;
	}

	if (colorspace == HDMI_COLORSPACE_YUV420)
		clk_khz = clk_khz/2;

	if ((requested_bpc >= 10) &&
		((clk_khz/4*5) <= info->max_tmds_clock)) {
		switch (colorspace) {
		case HDMI_COLORSPACE_RGB:
			if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_30)
				bpc = 10;
			break;
		case HDMI_COLORSPACE_YUV444:
			if (info->edid_hdmi_ycbcr444_dc_modes & DRM_EDID_HDMI_DC_30)
				bpc = 10;
			break;
		case HDMI_COLORSPACE_YUV420:
			if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_30)
				bpc = 10;
			break;
		default:
			break;
		}
	}

	if ((requested_bpc >= 12) &&
		((clk_khz/2*3) <= info->max_tmds_clock)) {
		switch (colorspace) {
		case HDMI_COLORSPACE_RGB:
			if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_36)
				bpc = 12;
			break;
		case HDMI_COLORSPACE_YUV444:
			if (info->edid_hdmi_ycbcr444_dc_modes & DRM_EDID_HDMI_DC_36)
				bpc = 12;
			break;
		case HDMI_COLORSPACE_YUV420:
			if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_36)
				bpc = 12;
			break;
		default:
			break;
		}
	}

exit:
	return bpc;
}

static int rtk_hdmi_set_allm(struct rtk_hdmi *hdmi, enum ALLM_STATUS en_allm)
{
	int ret;
	struct rpc_vout_hdmi_vrr arg;

	memset(&arg, 0, sizeof(arg));
	arg.vrr_function = HDMI_ALLM_ON_OFF;

	if (en_allm == ALLM_ENABLE)
		arg.vrr_act = HDMI_ALLM_ENABLE;
	else
		arg.vrr_act = HDMI_ALLM_DISABLE;

	ret = rpc_set_vrr(hdmi->rpc_info, &arg);

	return ret;
}

static int rtk_hdmi_set_qms(struct rtk_hdmi *hdmi,
	struct drm_connector_state *state, enum QMS_VRR_STATUS en_qms)
{
	int ret;
	int vrefresh;
	int vtotal;
	struct rpc_vout_hdmi_vrr arg;

	ret = -ENOEXEC;

	if (state->crtc == NULL)
		goto exit;

	if ((en_qms == QMS_VRR_EN_VRR) || (en_qms == QMS_VRR_EN_QMS)) {
		vrefresh = (state->crtc->mode.clock * 1000) /
				(state->crtc->mode.vtotal * state->crtc->mode.htotal);
		vtotal = state->crtc->mode.vtotal;
		if ((vrefresh != 60) || (vtotal < 1080))
			goto exit;
	}

	memset(&arg, 0, sizeof(arg));
	arg.vrr_function = HDMI_VRR_ON_OFF;
	arg.vrr_act = (enum ENUM_HDMI_VRR_ACT)en_qms;

	ret = rpc_set_vrr(hdmi->rpc_info, &arg);

	if (!ret) {
		if ((en_qms == QMS_VRR_EN_VRR) || (en_qms == QMS_VRR_EN_QMS))
			hdmi->vrr_rate = RATE_BASE;
		else
			hdmi->vrr_rate = RATE_UNSPECIFIED;
	}
exit:
	return ret;
}

static int rtk_hdmi_set_vrr_rate(struct rtk_hdmi *hdmi,
			enum QMS_VRR_RATE vrr_rate)
{
	int ret;
	struct rpc_vout_hdmi_vrr arg;

	ret = -ENOEXEC;

	if ((hdmi->en_qms_vrr != QMS_VRR_EN_VRR) &&
		(hdmi->en_qms_vrr != QMS_VRR_EN_QMS))
		goto exit;

	memset(&arg, 0, sizeof(arg));
	arg.vrr_function = HDMI_VRR_TARGET_RATE;
	arg.vrr_act = (enum ENUM_HDMI_VRR_ACT)vrr_rate;

	ret = rpc_set_vrr(hdmi->rpc_info, &arg);
exit:
	return ret;
}

static int rtk_hdmi_audio_output_ctrl(struct rtk_hdmi *hdmi,
				u8 enable)
{
	int ret;
	struct rpc_audio_hdmi_freq audio_arg;
	struct rpc_audio_ctrl_data rpc_data;

	memset(&audio_arg, 0, sizeof(audio_arg));
	memset(&rpc_data, 0, sizeof(rpc_data));

	if (enable) {
		audio_arg.tmds_freq = max(hdmi->previous_mode.clock/1000, 27);
		dev_info(hdmi->dev, "set audio tmds_freq=%u", audio_arg.tmds_freq);
		rpc_send_hdmi_freq(hdmi->rpc_info_ao, &audio_arg);
	}

	rpc_data.version = AUDIO_CTRL_VERSION;
	rpc_data.hdmi_en_state = enable ? 1:0;

	dev_info(hdmi->dev, "set audio output %s",  enable ? "on":"off");

	ret = rpc_set_hdmi_audio_onoff(hdmi->rpc_info_ao, &rpc_data);

	return ret;
}

#define DDC_SEGMENT_ADDR 0x30
static int rtk_read_edid_block(struct rtk_hdmi *hdmi, u8 *buf, u8 block_index)
{
	int ret;
	int retry = 3;
	int i;
	struct i2c_msg msgs[3];
	unsigned char start = (block_index % 2) ? EDID_LENGTH:0;
	unsigned char segment = block_index/2;
	unsigned char xfers = segment ? 3 : 2;

	msgs[0].addr = DDC_SEGMENT_ADDR;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &segment;

	msgs[1].addr = DDC_ADDR;
	msgs[1].flags = 0;
	msgs[1].len = 1;
	msgs[1].buf = &start;

	msgs[2].addr = DDC_ADDR;
	msgs[2].flags = I2C_M_RD;
	msgs[2].len = EDID_LENGTH;
	msgs[2].buf = buf;

	for (i = 0; i < retry; i++) {
		ret = i2c_transfer(hdmi->ddc, &msgs[3 - xfers], xfers);
		if (ret == xfers)
			return 0;

		if (hdmi->hpd_state)
			msleep(100);
		else
			break;
	}

	return -EIO;
}

static int rtk_hdmi_parse_tmds_phy(struct rtk_hdmi *hdmi)
{
	struct device_node *phy_np;
	struct device_node *pll_np;
	int ret = -1;
	int i;

	phy_np = of_find_node_by_name(hdmi->dev->of_node, "hdmi-tmds-phy");
	if (phy_np) {
		for (i = 0; i < TMDS_PLL_NUM; i++) {
			pll_np = of_find_node_by_name(phy_np, pll_mode_list[i].name);
			if (pll_np) {
				hdmi->tmds_phy[i].pll_mode = pll_mode_list[i].mode;
				ret = of_property_read_u32(pll_np, "sd1",
						&hdmi->tmds_phy[i].sd1);
				ret = of_property_read_u32(pll_np, "ldo1",
						&hdmi->tmds_phy[i].ldo1);
				ret = of_property_read_u32(pll_np, "ldo2",
						&hdmi->tmds_phy[i].ldo2);
				ret = of_property_read_u32(pll_np, "ldo3",
						&hdmi->tmds_phy[i].ldo3);
				ret = of_property_read_u32(pll_np, "ldo4",
						&hdmi->tmds_phy[i].ldo4);
				ret = of_property_read_u32(pll_np, "ldo5",
						&hdmi->tmds_phy[i].ldo5);
				ret = of_property_read_u32(pll_np, "ldo6",
						&hdmi->tmds_phy[i].ldo6);
				ret = of_property_read_u32(pll_np, "ldo7",
						&hdmi->tmds_phy[i].ldo7);
				ret = of_property_read_u32(pll_np, "ldo8",
						&hdmi->tmds_phy[i].ldo8);
				ret = of_property_read_u32(pll_np, "ldo9",
						&hdmi->tmds_phy[i].ldo9);
				ret = of_property_read_u32(pll_np, "ldo10",
						&hdmi->tmds_phy[i].ldo10);
				ret = of_property_read_u32(pll_np, "ldo11",
						&hdmi->tmds_phy[i].ldo11);
				ret = of_property_read_u32(pll_np, "pll-hdmi2",
						&hdmi->tmds_phy[i].pll_hdmi2);
				ret = of_property_read_u32(pll_np, "pll-hdmi3",
						&hdmi->tmds_phy[i].pll_hdmi3);
				ret = of_property_read_u32(pll_np, "pll-hdmi4",
						&hdmi->tmds_phy[i].pll_hdmi4);
				ret = of_property_read_u32(pll_np, "kvco-res",
						&hdmi->tmds_phy[i].kvco_res);
				ret = of_property_read_u32(pll_np, "ps2-ckin-sel",
						&hdmi->tmds_phy[i].ps2_ckin_sel);
			}
		}
	}

	return ret;
}

static int pll_mode_to_clock_khz(unsigned char pll_mode)
{
	int clock_khz;

	switch (pll_mode) {
	case PLL_27MHZ:
		clock_khz = 27000;
		break;
	case PLL_27x1p25:
		clock_khz = 33750;
		break;
	case PLL_27x1p5:
		clock_khz = 40500;
		break;
	case PLL_54MHZ:
		clock_khz = 54000;
		break;
	case PLL_54x1p25:
		clock_khz = 67500;
		break;
	case PLL_54x1p5:
		clock_khz = 81000;
		break;
	case PLL_59p4MHZ:
		clock_khz = 59400;
		break;
	case PLL_59p4x1p25:
		clock_khz = 74250;
		break;
	case PLL_59p4x1p5:
		clock_khz = 89100;
		break;
	case PLL_74p25MHZ:
		clock_khz = 74250;
		break;
	case PLL_74p25x1p25:
		clock_khz = 92812;
		break;
	case PLL_74p25x1p5:
		clock_khz = 111375;
		break;
	case PLL_83p5MHZ:
		clock_khz = 83500;
		break;
	case PLL_106p5MHZ:
		clock_khz = 106500;
		break;
	case PLL_108MHZ:
		clock_khz = 108000;
		break;
	case PLL_146p25MHZ:
		clock_khz = 146250;
		break;
	case PLL_148p5MHZ:
		clock_khz = 148500;
		break;
	case PLL_148p5x1p25:
		clock_khz = 185625;
		break;
	case PLL_148p5x1p5:
		clock_khz = 222750;
		break;
	case PLL_297MHZ:
		clock_khz = 297000;
		break;
	case PLL_297x1p25:
		clock_khz = 371250;
		break;
	case PLL_297x1p5:
		clock_khz = 445500;
		break;
	case PLL_594MHZ_420:
		clock_khz = 297000;
		break;
	case PLL_594MHZ_420x1p25:
		clock_khz = 371250;
		break;
	case PLL_594MHZ_420x1p5:
		clock_khz = 445500;
		break;
	case PLL_453p456MHZ:
		clock_khz = 453456;
		break;
	case PLL_486p048MHZ:
		clock_khz = 486048;
		break;
	case PLL_594MHZ:
		clock_khz = 594000;
		break;
	default:
		clock_khz = 0;
		break;
	}

	return clock_khz;
}

static unsigned char get_tmds_pll_mode(struct rpc_display_output_format format)
{
	unsigned char dpc_enable;
	unsigned char pll_mode;
	unsigned int vic;

	vic = format.vic;

	if (format.colorspace != COLORSPACE_YUV422) {
		dpc_enable = (format.color_depth > 8) ? 1:0;
	} else {
		dpc_enable = 0;
		format.color_depth = 8;
	}

	switch (vic) {
	case VIC_1280X720P24:
		if (dpc_enable == 1 && format.color_depth == 10)
			pll_mode = PLL_59p4x1p25;
		else if (dpc_enable == 1 && format.color_depth == 12)
			pll_mode = PLL_59p4x1p5;
		else
			pll_mode = PLL_59p4MHZ;
		break;

	case VIC_1280X720P25:
	case VIC_1280X720P30:
	case VIC_1280X720P60:
	case VIC_1920X1080I60:
	case VIC_1280X720P50:
	case VIC_1920X1080I50:
	case VIC_1920X1080P24:
	case VIC_1920X1080P25:
	case VIC_1920X1080P30:
		if (dpc_enable == 1 && format.color_depth == 10) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_148p5x1p25;
			else
				pll_mode = PLL_74p25x1p25;
		} else if (dpc_enable == 1 && format.color_depth == 12) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_148p5x1p5;
			else
				pll_mode = PLL_74p25x1p5;
		} else {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_148p5MHZ;
			else
				pll_mode = PLL_74p25MHZ;
		}
		break;

	case VIC_720X480P60:
	case VIC_720X480I60:
	case VIC_720X576P50:
	case VIC_720X576I50:
		if (dpc_enable == 1 && format.color_depth == 10) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_54x1p25;
			else
				pll_mode = PLL_27x1p25;
		} else if (dpc_enable == 1 && format.color_depth == 12) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_54x1p5;
			else
				pll_mode = PLL_27x1p5;
		} else {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_54MHZ;
			else
				pll_mode = PLL_27MHZ;
		}
		break;

	case VIC_1920X1080P60:
	case VIC_1920X1080P50:
		if (dpc_enable == 1 && format.color_depth == 10) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_297x1p25;
			else
				pll_mode = PLL_148p5x1p25;
		} else if (dpc_enable == 1 && format.color_depth == 12) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_297x1p5;
			else
				pll_mode = PLL_148p5x1p5;
		} else {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_297MHZ;
			else
				pll_mode = PLL_148p5MHZ;
		}
		break;

	case VIC_1920X1080P120:
	case VIC_3840X2160P24:
	case VIC_3840X2160P25:
	case VIC_3840X2160P30:
	case VIC_4096X2160P24:
	case VIC_4096X2160P25:
	case VIC_4096X2160P30:
		if (dpc_enable == 1 && format.color_depth == 10) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_INVALIDMHZ;
			else
				pll_mode = PLL_297x1p25;
		} else if (dpc_enable == 1 && format.color_depth == 12) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_INVALIDMHZ;
			else
				pll_mode = PLL_297x1p5;
		} else {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_594MHZ;
			else
				pll_mode = PLL_297MHZ;
		}
		break;

	case VIC_3840X2160P50:
	case VIC_3840X2160P60:
	case VIC_4096X2160P50:
	case VIC_4096X2160P60:
		if (format.colorspace != COLORSPACE_YUV420) {
			pll_mode = PLL_594MHZ;
			break;
		}
		/* YUV420 */
		if (dpc_enable == 1 && format.color_depth == 10) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_INVALIDMHZ;
			else
				pll_mode = PLL_594MHZ_420x1p25;
		} else if (dpc_enable == 1 && format.color_depth == 12) {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_INVALIDMHZ;
			else
				pll_mode = PLL_594MHZ_420x1p5;
		} else {
			if (format.dst_3d_fmt == HDMI_3D_FRAME_PACKING)
				pll_mode = PLL_594MHZ;
			else
				pll_mode = PLL_594MHZ_420;
		}
		break;
	case VIC_2560X1080P144:
		pll_mode = PLL_453p456MHZ;
		break;
	case VIC_2560X1440P120:
		pll_mode = PLL_486p048MHZ;
		break;
	case VIC_1280X800P60RB:
	case VIC_1280X800P60:
		pll_mode = PLL_83p5MHZ;
		break;
	case VIC_1440X900P60RB:
	case VIC_1440X900P60:
		pll_mode = PLL_106p5MHZ;
		break;
	case VIC_1680X1050P60RB:
	case VIC_1680X1050P60:
		pll_mode = PLL_146p25MHZ;
		break;
	case VIC_1600X900P60:
		pll_mode = PLL_108MHZ;
		break;
	default:
		pll_mode = PLL_INVALIDMHZ;
		break;
	}

	return pll_mode;
}

void enable_hdmitx_pll_power(struct rtk_hdmi *hdmi, unsigned char pll_mode)
{
	unsigned char is_high_gain;
	unsigned char ps2_div2;
	unsigned char ps2_ckin_sel;
	unsigned char plldisp_pow;
	unsigned int reg_val;

	is_high_gain = hdmi->tmds_phy[pll_mode].kvco_res;
	ps2_ckin_sel = hdmi->tmds_phy[pll_mode].ps2_ckin_sel;
	ps2_div2 = (pll_mode >= PLL_453p456MHZ) ? 0:1;
	plldisp_pow = 0;

	regmap_write(hdmi->crtreg, SYS_PLL_HDMI,
		SYS_PLL_HDMI_REG_MUX_PLLTMDS(0)|
		SYS_PLL_HDMI_REG_PLLTMDS_POW_594M(0)|
		SYS_PLL_HDMI_REG_POW_594M(0)|
		SYS_PLL_HDMI_REG_PRE_DRIVER(0) |
		SYS_PLL_HDMI_REG_PLLDISP_EXT_LDO_LV(1) |
		SYS_PLL_HDMI_REG_PLL_CAPSEL_VTOI(0) |
		SYS_PLL_HDMI_REG_PLL_SEL_DUAL_R(0x1)|
		SYS_PLL_HDMI_REG_PLL_KVCO_RES(is_high_gain) |
		SYS_PLL_HDMI_REG_PLL_VSET_SEL(0) |
		SYS_PLL_HDMI_REG_PLL_LDO_POW(1) |
		SYS_PLL_HDMI_REG_PLL_BPS_M3(0) |
		SYS_PLL_HDMI_REG_P2S_CKIN_SEL(ps2_ckin_sel) |
		SYS_PLL_HDMI_REG_P2S_DIV2(ps2_div2) |
		SYS_PLL_HDMI_PLLDISP_OEB(0) |
		SYS_PLL_HDMI_PLLDISP_VCORSTB(1) |
		SYS_PLL_HDMI_REG_PLL_MHL3_DIV_EN(1) |
		SYS_PLL_HDMI_REG_PLLDISP_RSTB(0) |
		SYS_PLL_HDMI_REG_PLLDISP_POW(plldisp_pow) |
		SYS_PLL_HDMI_REG_TMDS_POW(1) |
		SYS_PLL_HDMI_REG_PLL_RSTB(0) |
		SYS_PLL_HDMI_REG_PLL_POW(1) |
		SYS_PLL_HDMI_REG_HDMI_CK_EN(1));

	udelay(20);

	regmap_read(hdmi->crtreg, SYS_PLL_HDMI, &reg_val);
	reg_val &= ~SYS_PLL_HDMI_REG_PRE_DRIVER_mask;

	regmap_write(hdmi->crtreg, SYS_PLL_HDMI,
		reg_val |
		SYS_PLL_HDMI_REG_PRE_DRIVER(1));

	udelay(20);
}

static unsigned int adjust_fcode_ncode_new(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode, unsigned char pll_mode)
{
	unsigned int fcode;
	unsigned int ncode;
	unsigned int sd1;
	unsigned int adjusted_sd1;

	sd1 = hdmi->tmds_phy[pll_mode].sd1;

	if (!is_fractional_mode(hdmi, mode))
		return sd1;

	switch (pll_mode) {
	case PLL_74p25MHZ:
		fcode = 968;
		ncode = 24;
		break;
	case PLL_74p25x1p25:
		fcode = 698;
		ncode = 31;
		break;
	case PLL_74p25x1p5:
		fcode = 428;
		ncode = 38;
		break;
	case PLL_148p5MHZ:
		fcode = 1935;
		ncode = 51;
		break;
	case PLL_148p5x1p25:
		fcode = 1395;
		ncode = 65;
		break;
	case PLL_148p5x1p5:
		fcode = 855;
		ncode = 79;
		break;
	case PLL_297MHZ:
	case PLL_594MHZ_420:
	case PLL_594MHZ:
		fcode = 1823;
		ncode = 106;
		break;
	case PLL_297x1p25:
	case PLL_594MHZ_420x1p25:
		fcode = 742;
		ncode = 134;
		break;
	case PLL_297x1p5:
	case PLL_594MHZ_420x1p5:
		fcode = 1710;
		ncode = 161;
		break;
	default:
		fcode = SYS_PLL_HDMITX2P1_SD1_get_fcode(sd1);
		ncode = SYS_PLL_HDMITX2P1_SD1_get_ncode(sd1);
		break;
	}

	adjusted_sd1 = sd1 & ~(SYS_PLL_HDMITX2P1_SD1_fcode_mask | SYS_PLL_HDMITX2P1_SD1_ncode_mask);
	adjusted_sd1 = adjusted_sd1 |
		SYS_PLL_HDMITX2P1_SD1_fcode(fcode) | SYS_PLL_HDMITX2P1_SD1_ncode(ncode);

	return adjusted_sd1;
}

static void swap_combine_new_ldo(struct hdmi_tmds_phy *phy,
		struct pll_ldo_element *a, struct pll_ldo_element *b,
		struct pll_ldo_element *c, struct pll_ldo_element *d)
{

	phy->ldo2 = phy->ldo2 &
				~(SYS_PLL_HDMI_LDO2_REG_TMDS_RSELC_mask |
				SYS_PLL_HDMI_LDO2_REG_TMDS_RSELB_mask |
				SYS_PLL_HDMI_LDO2_REG_TMDS_RSELA_mask |
				SYS_PLL_HDMI_LDO2_REG_TMDS_EMCK_mask |
				SYS_PLL_HDMI_LDO2_REG_TMDS_EMC_mask |
				SYS_PLL_HDMI_LDO2_REG_TMDS_EMB_mask |
				SYS_PLL_HDMI_LDO2_REG_TMDS_EMA_mask);

	phy->ldo2 = phy->ldo2 |
				SYS_PLL_HDMI_LDO2_REG_TMDS_RSELC(c->rsel) |
				SYS_PLL_HDMI_LDO2_REG_TMDS_RSELB(b->rsel) |
				SYS_PLL_HDMI_LDO2_REG_TMDS_RSELA(a->rsel) |
				SYS_PLL_HDMI_LDO2_REG_TMDS_EMCK(d->em_en) |
				SYS_PLL_HDMI_LDO2_REG_TMDS_EMC(c->em_en) |
				SYS_PLL_HDMI_LDO2_REG_TMDS_EMB(b->em_en) |
				SYS_PLL_HDMI_LDO2_REG_TMDS_EMA(a->em_en);

	phy->ldo3 = phy->ldo3 &
				~(SYS_PLL_HDMI_LDO3_REG_TMDS_IDRVCK_mask |
				SYS_PLL_HDMI_LDO3_REG_TMDS_IDRVC_mask |
				SYS_PLL_HDMI_LDO3_REG_TMDS_IDRVB_mask |
				SYS_PLL_HDMI_LDO3_REG_TMDS_IDRVA_mask);

	phy->ldo3 = phy->ldo3 |
				SYS_PLL_HDMI_LDO3_REG_TMDS_IDRVCK(d->idrv) |
				SYS_PLL_HDMI_LDO3_REG_TMDS_IDRVC(c->idrv) |
				SYS_PLL_HDMI_LDO3_REG_TMDS_IDRVB(b->idrv) |
				SYS_PLL_HDMI_LDO3_REG_TMDS_IDRVA(a->idrv);

	phy->ldo4 = phy->ldo4 &
				~(SYS_PLL_HDMI_LDO4_REG_TMDS_IEMCK_mask |
				SYS_PLL_HDMI_LDO4_REG_TMDS_IEMC_mask |
				SYS_PLL_HDMI_LDO4_REG_TMDS_IEMB_mask |
				SYS_PLL_HDMI_LDO4_REG_TMDS_IEMA_mask);

	phy->ldo4 = phy->ldo4 |
				SYS_PLL_HDMI_LDO4_REG_TMDS_IEMCK(d->iem_level) |
				SYS_PLL_HDMI_LDO4_REG_TMDS_IEMC(c->iem_level) |
				SYS_PLL_HDMI_LDO4_REG_TMDS_IEMB(b->iem_level) |
				SYS_PLL_HDMI_LDO4_REG_TMDS_IEMA(a->iem_level);

	phy->ldo7 = phy->ldo7 &
				~(SYS_PLL_HDMI_LDO7_REG_TMDS_IEMPRECK_mask |
				SYS_PLL_HDMI_LDO7_REG_TMDS_IEMPREC_mask |
				SYS_PLL_HDMI_LDO7_REG_TMDS_IEMPREB_mask |
				SYS_PLL_HDMI_LDO7_REG_TMDS_IEMPREA_mask |
				SYS_PLL_HDMI_LDO7_REG_TMDS_EMPRECK_mask |
				SYS_PLL_HDMI_LDO7_REG_TMDS_EMPREC_mask |
				SYS_PLL_HDMI_LDO7_REG_TMDS_EMPREB_mask |
				SYS_PLL_HDMI_LDO7_REG_TMDS_EMPREA_mask);

	phy->ldo7 = phy->ldo7 |
				SYS_PLL_HDMI_LDO7_REG_TMDS_IEMPRECK(d->iempre_level) |
				SYS_PLL_HDMI_LDO7_REG_TMDS_IEMPREC(c->iempre_level) |
				SYS_PLL_HDMI_LDO7_REG_TMDS_IEMPREB(b->iempre_level) |
				SYS_PLL_HDMI_LDO7_REG_TMDS_IEMPREA(a->iempre_level) |
				SYS_PLL_HDMI_LDO7_REG_TMDS_EMPRECK(d->empre_en) |
				SYS_PLL_HDMI_LDO7_REG_TMDS_EMPREC(c->empre_en) |
				SYS_PLL_HDMI_LDO7_REG_TMDS_EMPREB(b->empre_en) |
				SYS_PLL_HDMI_LDO7_REG_TMDS_EMPREA(a->empre_en);

	phy->ldo8 = phy->ldo8 &
				~(SYS_PLL_HDMI_LDO8_REG_TMDS_IPDRVEM_mask |
				SYS_PLL_HDMI_LDO8_REG_TMDS_IPDRV_mask);

	phy->ldo8 = phy->ldo8 |
				SYS_PLL_HDMI_LDO8_REG_TMDS_IPDRVEM(b->ipdrv) |
				SYS_PLL_HDMI_LDO8_REG_TMDS_IPDRV(a->ipdrv);

	phy->ldo9 = phy->ldo9 &
				~(SYS_PLL_HDMI_LDO9_REG_TMDS_RSELCK_mask);

	phy->ldo9 = phy->ldo9 |
				SYS_PLL_HDMI_LDO9_REG_TMDS_RSELCK(d->rsel);

	phy->ldo10 = phy->ldo10 &
				~(SYS_PLL_HDMI_LDO10_REG_TMDS_IPDRVEMPRECK_mask |
				SYS_PLL_HDMI_LDO10_REG_TMDS_IPDRVEMCK_mask |
				SYS_PLL_HDMI_LDO10_REG_TMDS_IPDRVCK_mask |
				SYS_PLL_HDMI_LDO10_REG_TMDS_IPDRVEMPRE_mask);

	phy->ldo10 = phy->ldo10 |
				SYS_PLL_HDMI_LDO10_REG_TMDS_IPDRVEMPRECK(d->ipdrv) |
				SYS_PLL_HDMI_LDO10_REG_TMDS_IPDRVEMCK(d->ipdrv) |
				SYS_PLL_HDMI_LDO10_REG_TMDS_IPDRVCK(d->ipdrv) |
				SYS_PLL_HDMI_LDO10_REG_TMDS_IPDRVEMPRE(c->ipdrv);
}

static void swap_relocation_ldo(struct rtk_hdmi *hdmi,
	unsigned char pll_mode, struct hdmi_tmds_phy *phy)
{
	struct pll_ldo_element lane0;
	struct pll_ldo_element lane1;
	struct pll_ldo_element lane2;
	struct pll_ldo_element lane3;

	lane0.em_en = SYS_PLL_HDMI_LDO2_get_REG_TMDS_EMA(hdmi->tmds_phy[pll_mode].ldo2);
	lane0.iem_level = SYS_PLL_HDMI_LDO4_get_REG_TMDS_IEMA(hdmi->tmds_phy[pll_mode].ldo4);
	lane0.idrv = SYS_PLL_HDMI_LDO3_get_REG_TMDS_IDRVA(hdmi->tmds_phy[pll_mode].ldo3);
	lane0.empre_en = SYS_PLL_HDMI_LDO7_get_REG_TMDS_EMPREA(hdmi->tmds_phy[pll_mode].ldo7);
	lane0.iempre_level = SYS_PLL_HDMI_LDO7_get_REG_TMDS_IEMPREA(hdmi->tmds_phy[pll_mode].ldo7);
	lane0.rsel = SYS_PLL_HDMI_LDO2_get_REG_TMDS_RSELA(hdmi->tmds_phy[pll_mode].ldo2);
	lane0.ipdrv = SYS_PLL_HDMI_LDO8_get_REG_TMDS_IPDRV(hdmi->tmds_phy[pll_mode].ldo8);

	lane1.em_en = SYS_PLL_HDMI_LDO2_get_REG_TMDS_EMB(hdmi->tmds_phy[pll_mode].ldo2);
	lane1.iem_level = SYS_PLL_HDMI_LDO4_get_REG_TMDS_IEMB(hdmi->tmds_phy[pll_mode].ldo4);
	lane1.idrv = SYS_PLL_HDMI_LDO3_get_REG_TMDS_IDRVB(hdmi->tmds_phy[pll_mode].ldo3);
	lane1.empre_en = SYS_PLL_HDMI_LDO7_get_REG_TMDS_EMPREB(hdmi->tmds_phy[pll_mode].ldo7);
	lane1.iempre_level = SYS_PLL_HDMI_LDO7_get_REG_TMDS_IEMPREB(hdmi->tmds_phy[pll_mode].ldo7);
	lane1.rsel = SYS_PLL_HDMI_LDO2_get_REG_TMDS_RSELB(hdmi->tmds_phy[pll_mode].ldo2);
	lane1.ipdrv = SYS_PLL_HDMI_LDO8_get_REG_TMDS_IPDRVEM(hdmi->tmds_phy[pll_mode].ldo8);

	lane2.em_en = SYS_PLL_HDMI_LDO2_get_REG_TMDS_EMC(hdmi->tmds_phy[pll_mode].ldo2);
	lane2.iem_level = SYS_PLL_HDMI_LDO4_get_REG_TMDS_IEMC(hdmi->tmds_phy[pll_mode].ldo4);
	lane2.idrv = SYS_PLL_HDMI_LDO3_get_REG_TMDS_IDRVC(hdmi->tmds_phy[pll_mode].ldo3);
	lane2.empre_en = SYS_PLL_HDMI_LDO7_get_REG_TMDS_EMPREC(hdmi->tmds_phy[pll_mode].ldo7);
	lane2.iempre_level = SYS_PLL_HDMI_LDO7_get_REG_TMDS_IEMPREC(hdmi->tmds_phy[pll_mode].ldo7);
	lane2.rsel = SYS_PLL_HDMI_LDO2_get_REG_TMDS_RSELC(hdmi->tmds_phy[pll_mode].ldo2);
	lane2.ipdrv = SYS_PLL_HDMI_LDO10_get_REG_TMDS_IPDRVEMPRE(hdmi->tmds_phy[pll_mode].ldo10);

	lane3.em_en = SYS_PLL_HDMI_LDO2_get_REG_TMDS_EMCK(hdmi->tmds_phy[pll_mode].ldo2);
	lane3.iem_level = SYS_PLL_HDMI_LDO4_get_REG_TMDS_IEMCK(hdmi->tmds_phy[pll_mode].ldo4);
	lane3.idrv = SYS_PLL_HDMI_LDO3_get_REG_TMDS_IDRVCK(hdmi->tmds_phy[pll_mode].ldo3);
	lane3.empre_en = SYS_PLL_HDMI_LDO7_get_REG_TMDS_EMPRECK(hdmi->tmds_phy[pll_mode].ldo7);
	lane3.iempre_level = SYS_PLL_HDMI_LDO7_get_REG_TMDS_IEMPRECK(hdmi->tmds_phy[pll_mode].ldo7);
	lane3.rsel = SYS_PLL_HDMI_LDO9_get_REG_TMDS_RSELCK(hdmi->tmds_phy[pll_mode].ldo9);
	lane3.ipdrv = SYS_PLL_HDMI_LDO10_get_REG_TMDS_IPDRVEMPRECK(hdmi->tmds_phy[pll_mode].ldo10);

	switch (CHSWAP_TYPE(hdmi->swap_config)) {
	case SWAP_0_LANE_0_1_2_3:
		swap_combine_new_ldo(phy, &lane0, &lane1, &lane2, &lane3);
		break;
	case SWAP_1_LANE_3_1_2_0:
		swap_combine_new_ldo(phy, &lane3, &lane1, &lane2, &lane0);
		break;
	case SWAP_2_LANE_1_0_2_3:
		swap_combine_new_ldo(phy, &lane1, &lane0, &lane2, &lane3);
		break;
	case SWAP_3_LANE_3_0_2_1:
		swap_combine_new_ldo(phy, &lane3, &lane0, &lane2, &lane1);
		break;
	case SWAP_4_LANE_0_3_2_1:
		swap_combine_new_ldo(phy, &lane0, &lane3, &lane2, &lane1);
		break;
	case SWAP_5_LANE_1_3_2_0:
		swap_combine_new_ldo(phy, &lane1, &lane3, &lane2, &lane0);
		break;
	case SWAP_6_LANE_0_2_1_3:
		swap_combine_new_ldo(phy, &lane0, &lane2, &lane1, &lane3);
		break;
	case SWAP_7_LANE_3_2_1_0:
		swap_combine_new_ldo(phy, &lane3, &lane2, &lane1, &lane0);
		break;
	case SWAP_8_LANE_2_0_1_3:
		swap_combine_new_ldo(phy, &lane2, &lane0, &lane1, &lane3);
		break;
	case SWAP_9_LANE_3_0_1_2:
		swap_combine_new_ldo(phy, &lane3, &lane0, &lane1, &lane2);
		break;
	case SWAP_10_LANE_0_3_1_2:
		swap_combine_new_ldo(phy, &lane0, &lane3, &lane1, &lane2);
		break;
	case SWAP_11_LANE_2_3_1_0:
		swap_combine_new_ldo(phy, &lane2, &lane3, &lane1, &lane0);
		break;
	case SWAP_12_LANE_2_1_0_3:
		swap_combine_new_ldo(phy, &lane2, &lane1, &lane0, &lane3);
		break;
	case SWAP_13_LANE_3_1_0_2:
		swap_combine_new_ldo(phy, &lane3, &lane1, &lane0, &lane2);
		break;
	case SWAP_14_LANE_1_2_0_3:
		swap_combine_new_ldo(phy, &lane1, &lane2, &lane0, &lane3);
		break;
	case SWAP_15_LANE_3_2_0_1:
		swap_combine_new_ldo(phy, &lane3, &lane2, &lane0, &lane1);
		break;
	case SWAP_16_LANE_2_3_0_1:
		swap_combine_new_ldo(phy, &lane2, &lane3, &lane0, &lane1);
		break;
	case SWAP_17_LANE_1_3_0_2:
		swap_combine_new_ldo(phy, &lane1, &lane3, &lane0, &lane2);
		break;
	case SWAP_18_LANE_0_1_3_2:
		swap_combine_new_ldo(phy, &lane0, &lane1, &lane3, &lane2);
		break;
	case SWAP_19_LANE_2_1_3_0:
		swap_combine_new_ldo(phy, &lane2, &lane1, &lane3, &lane0);
		break;
	case SWAP_20_LANE_1_0_3_2:
		swap_combine_new_ldo(phy, &lane1, &lane0, &lane3, &lane2);
		break;
	case SWAP_21_LANE_2_0_3_1:
		swap_combine_new_ldo(phy, &lane2, &lane0, &lane3, &lane1);
		break;
	case SWAP_22_LANE_0_2_3_1:
		swap_combine_new_ldo(phy, &lane0, &lane2, &lane3, &lane1);
		break;
	case SWAP_23_LANE_1_2_3_0:
		swap_combine_new_ldo(phy, &lane1, &lane2, &lane3, &lane0);
		break;
	default:
		dev_err(hdmi->dev, "Error: %s Unknown CHSWAP_TYPE=%u\n",
			__func__, CHSWAP_TYPE(hdmi->swap_config));
		break;
	}
}

void active_hdmitx_pll_control(struct rtk_hdmi *hdmi)
{
	unsigned int reg_val;

	regmap_read(hdmi->crtreg, SYS_PLL_HDMI, &reg_val);
	reg_val &= ~SYS_PLL_HDMI_REG_PLL_RSTB_mask;

	regmap_write(hdmi->crtreg, SYS_PLL_HDMI,
		reg_val |
		SYS_PLL_HDMI_REG_PLL_RSTB(1));

	udelay(40);
}

static void set_tmds_pll_new(struct rtk_hdmi *hdmi, struct drm_display_mode *mode,
	unsigned char pll_mode)
{
	struct hdmi_tmds_phy phy;
	unsigned int sd1;
	unsigned int reg_val;

	enable_hdmitx_pll_power(hdmi, pll_mode);

	sd1 = adjust_fcode_ncode_new(hdmi, mode, pll_mode);

	regmap_write(hdmi->crtreg, SYS_PLL_HDMITX2P1_SD1, sd1);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMITX2P1_SD2, 0x50203FFE);

	regmap_write(hdmi->crtreg, SYS_PLL_HDMITX2P1_SD5,
		SYS_PLL_HDMITX2P1_SD5_RSTB_HDMITX(1) |
		SYS_PLL_HDMITX2P1_SD5_gran_auto_rst(0) |
		SYS_PLL_HDMITX2P1_SD5_dot_gran(0) |
		SYS_PLL_HDMITX2P1_SD5_gran_est(0));

	/* Enable PLL OC */
	regmap_read(hdmi->crtreg, SYS_PLL_HDMITX2P1_SD2, &reg_val);
	reg_val &= ~SYS_PLL_HDMITX2P1_SD2_oc_en_mask;

	regmap_write(hdmi->crtreg, SYS_PLL_HDMITX2P1_SD2,
		reg_val |
		SYS_PLL_HDMITX2P1_SD2_oc_en(1));

	udelay(20);

	memset(&phy, 0, sizeof(phy));
	memcpy(&phy, &hdmi->tmds_phy[pll_mode], sizeof(phy));

	if (hdmi->swap_config)
		swap_relocation_ldo(hdmi, pll_mode, &phy);

	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO1, phy.ldo1);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO2, phy.ldo2);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO3, phy.ldo3);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO4, phy.ldo4);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO5, phy.ldo5);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI2, phy.pll_hdmi2);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI3, phy.pll_hdmi3);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI4, phy.pll_hdmi4);
	udelay(20);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO6, phy.ldo6);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO7, phy.ldo7);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO8, phy.ldo8);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO10, phy.ldo10);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO11, phy.ldo11);
	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO9,
		phy.ldo9 |
		SYS_PLL_HDMI_LDO9_REG_HDMITX_RESERVE_RXONS_SENSE(1));
	udelay(20);

	active_hdmitx_pll_control(hdmi);

	/* 0x98000244 [29] = 1 */
	regmap_read(hdmi->crtreg, SYS_PLL_HDMI_LDO6, &reg_val);
	reg_val &= ~SYS_PLL_HDMI_LDO6_REG_PLL_KVCO_mask;

	regmap_write(hdmi->crtreg, SYS_PLL_HDMI_LDO6,
		reg_val |
		SYS_PLL_HDMI_LDO6_REG_PLL_KVCO(1));
}

/**
 * check_phy_state - Check if PHY state meets the expectation
 * @format: struct rpc_display_output_format format
 *
 * Return: 1 on pll clock is correct, 0 on incorrect
 */
static unsigned char check_phy_state(struct rtk_hdmi *hdmi,
	struct rpc_display_output_format format)
{
	unsigned char pll_mode;
	int clk_khz;
	unsigned int upper_bound;
	unsigned int lower_bound;
	unsigned int reg_val = 0;
	unsigned char cmp_result = 0;
	unsigned char clkdet_done = 0;
	unsigned char i;

	pll_mode = get_tmds_pll_mode(format);
	clk_khz = pll_mode_to_clock_khz(pll_mode);

	upper_bound = clk_khz/100 + 10;
	lower_bound = clk_khz/100 - 10;

	dev_info(hdmi->dev, "detect clk range from %u to %u\n",
		lower_bound, upper_bound);

	if (format.display_mode == DISPLAY_MODE_FRL)
		regmap_write(hdmi->topreg, PLLDIV_SEL,
			PLLDIV_SEL_tmds_src_sel(1));
	else if (format.display_mode != DISPLAY_MODE_OFF)
		regmap_write(hdmi->topreg, PLLDIV_SEL,
			PLLDIV_SEL_tmds_src_sel(0));

	/* Reset detector */
	regmap_write(hdmi->topreg, PLLDIV0, 0);

	regmap_write(hdmi->topreg, PLLDIV0_SETUP,
		PLLDIV0_SETUP_hdmitx_plltmds_upper_bound(upper_bound) |
		PLLDIV0_SETUP_hdmitx_plltmds_lower_bound(lower_bound));

	/* Release reset */
	regmap_write(hdmi->topreg, PLLDIV0, PLLDIV0_hdmitx_plltmds_rstn(1));
	regmap_write(hdmi->topreg, PLLDIV0,
		PLLDIV0_hdmitx_plltmds_count_en(1) |
		PLLDIV0_hdmitx_plltmds_rstn(1));

	/* Wait until detection is done or timeout; it may take about 10 us */
	for (i = 0; i < 15; i++) {
		regmap_read(hdmi->topreg, PLLDIV0_MP_STATUS, &reg_val);
		clkdet_done = PLLDIV0_MP_STATUS_get_hdmitx_plltmds_clkdet_done(reg_val);

		if (clkdet_done) {
			cmp_result = PLLDIV0_MP_STATUS_get_hdmitx_plltmds_clkdet_cmp_result(reg_val);
			if (cmp_result)
				break;
		}

		udelay(1);
	}

	regmap_read(hdmi->topreg, PLLDIV0, &reg_val);
	dev_info(hdmi->dev, "plltmds clk_count=%u refclk_count=%u\n",
		PLLDIV0_get_hdmitx_plltmds_clk_count(reg_val),
		PLLDIV0_get_hdmitx_plltmds_refclk_count(reg_val));

	return cmp_result;
}

static int rtk_hdmi_set_tmds_phy(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode, struct rpc_display_output_format format)
{
	unsigned char pll_mode;
	int ret = 0;

	pll_mode = get_tmds_pll_mode(format);

	dev_info(hdmi->dev, "Get pll_mode=%s\n", pll_mode_list[pll_mode].name);

	set_tmds_pll_new(hdmi, mode, pll_mode);

	return ret;
}

static int rtk_hdmi_off(struct rtk_hdmi *hdmi, u32 flags)
{
	int ret;
	struct rpc_config_tv_system cur_arg, arg;
	struct rpc_display_output_format cur_format, format;

	/* Disable hdcp before turn off hdmi */
	if (hdmi->hdcp_support && (flags & OFF_FLAG_HDCP)) {
		hdmi->hdcp.do_cancel_hdcp = true;
		cancel_delayed_work_sync(&hdmi->hdcp.commit_work);
		hdmi->hdcp.do_cancel_hdcp = false;
		if (hdmi->hdcp.value == DRM_MODE_CONTENT_PROTECTION_ENABLED) {
			rtk_hdcp_disable(hdmi);
			schedule_work(&hdmi->hdcp.prop_work);
		}
	}

	if (hdmi->is_new_mac) {
		memset(&cur_format, 0, sizeof(cur_format));
		ret = rpc_get_display_format(hdmi->rpc_info, &cur_format);
		if (ret)
			return ret;

		if (cur_format.display_mode == DISPLAY_MODE_OFF) {
			DRM_DEBUG_DRIVER("hdmi is already off, skip");
			return ret;
		}
	} else {
		memset(&cur_arg, 0, sizeof(cur_arg));
		ret = rpc_query_tv_system(hdmi->rpc_info, &cur_arg);
		if (ret)
			return ret;

		if (cur_arg.info_frame.hdmiMode == VO_HDMI_OFF) {
			DRM_DEBUG_DRIVER("hdmi is already off, skip");
			return ret;
		}
	}

	hdmi->is_hdmi_on = false;

	/* Send set_avmute before turn off hdmi */
	if (flags & OFF_FLAG_AVMUTE)
		rtk_hdmi_send_avmute(hdmi, 1);

	if ((hdmi->en_audio == HDMI_AO_AUTO) && (flags & OFF_FLAG_DIS_AUDIO))
		hdmi->hdmi_ops->audio_output_ctrl(hdmi, 0);

	dev_info(hdmi->dev, "%sset hdmi off",
				hdmi->is_new_mac ? "hdmi_new " : "");

	if (hdmi->is_new_mac) {
		memset(&format, 0, sizeof(format));

		format.display_mode = DISPLAY_MODE_OFF;

		ret = rpc_set_display_format(hdmi->rpc_info, &format);
	} else {
		memset(&arg, 0, sizeof(arg));

		arg.info_frame.hdmiMode = VO_HDMI_OFF;
		arg.videoInfo.standard = VO_STANDARD_NTSC_J;
		arg.videoInfo.enProg = 0;
		arg.videoInfo.pedType = 0x1;
		arg.videoInfo.dataInt0 = 0x4;

		ret = rpc_set_tv_system(hdmi->rpc_info, &arg);
	}

	if (hdmi->edid_info.scds_pb5 & SCDS_ALLM)
		hdmi->en_allm = ALLM_DISABLE;
	else
		hdmi->en_allm = ALLM_UNSUPPORTED;

	if ((hdmi->edid_info.vrr_min != 0) ||
		(hdmi->edid_info.scds_pb5 & SCDS_QMS))
		hdmi->en_qms_vrr = QMS_VRR_DISABLE;
	else
		hdmi->en_qms_vrr = QMS_VRR_UNSUPPORTED;

	hdmi->vrr_rate = RATE_UNSPECIFIED;

	if (flags & OFF_FLAG_HDCP)
		rtk_hdcp_set_state(hdmi, HDCP_HDMI_DISABLED, HDCP_NO_ERR);

	return ret;
}

static int rtk_hdmi_compute_avi_infoframe(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode, void *avi_buffer)
{
	int ret;
	struct hdmi_avi_infoframe infoframe;

	ret = drm_hdmi_avi_infoframe_from_display_mode(&infoframe,
				&hdmi->connector, mode);
	if (ret < 0) {
		dev_err(hdmi->dev, "Failed to fill avi infoframe");
		return ret;
	}

	infoframe.colorspace = hdmi->rgb_or_yuv;

#if 1 // TODO: Remove this workaround when fix symbol issue
	switch (hdmi->connector.state->content_type) {
	case DRM_MODE_CONTENT_TYPE_GRAPHICS:
		infoframe.content_type = HDMI_CONTENT_TYPE_GRAPHICS;
		break;
	case DRM_MODE_CONTENT_TYPE_CINEMA:
		infoframe.content_type = HDMI_CONTENT_TYPE_CINEMA;
		break;
	case DRM_MODE_CONTENT_TYPE_GAME:
		infoframe.content_type = HDMI_CONTENT_TYPE_GAME;
		break;
	case DRM_MODE_CONTENT_TYPE_PHOTO:
		infoframe.content_type = HDMI_CONTENT_TYPE_PHOTO;
		break;
	default:
		/* Graphics is the default(0) */
		infoframe.content_type = HDMI_CONTENT_TYPE_GRAPHICS;
	}

	infoframe.itc = hdmi->connector.state->content_type != DRM_MODE_CONTENT_TYPE_NO_DATA;
#else
	drm_hdmi_avi_infoframe_content_type(&infoframe, hdmi->connector.state);
#endif

	if (hdmi->connector.state->content_type != DRM_MODE_CONTENT_TYPE_NO_DATA)
		dev_info(hdmi->dev, "avi_infoframe itc=1 content_type=%u",
			infoframe.content_type);

	infoframe.scan_mode = hdmi->scan_mode;

	if (hdmi->scan_mode != HDMI_SCAN_MODE_UNDERSCAN)
		dev_info(hdmi->dev, "avi_infoframe scan_mode=%u", hdmi->scan_mode);

	ret = hdmi_avi_infoframe_pack(&infoframe, avi_buffer,
				HDMI_INFOFRAME_SIZE(AVI));
	if (ret < 0) {
		dev_err(hdmi->dev, "Failed to pack avi infoframe");
		return ret;
	}

	return 0;
}

static void hdmitx_set_video_timing(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode)
{
	int err;
	u8 vic;
	struct rpc_config_tv_system cur_tvsystem;
	struct rpc_config_tv_system tvsystem;
	unsigned int dataint0;
	u8 bpc;
	u8 avi_buffer[HDMI_INFOFRAME_SIZE(AVI)];
	unsigned int tmds_config;
	bool is_fractional;

	memset_io(&cur_tvsystem, 0, sizeof(cur_tvsystem));
	memset_io(&tvsystem, 0, sizeof(tvsystem));

	vic = drm_match_cea_mode(mode);
	bpc = rtk_hdmi_get_bpc(hdmi, mode);

	if (!rtk_hdmi_valid_vic(vic) && !is_rtk_non_cea_mode(mode)) {
		dev_err(hdmi->dev, "Invalid vic %u", vic);
		return;
	}

	is_fractional = is_fractional_mode(hdmi, mode);

	err = rtk_hdmi_compute_avi_infoframe(hdmi, mode, avi_buffer);
	if (err)
		dev_err(hdmi->dev, "Fail to set avi infoframe, err=%d", err);

	rpc_query_tv_system(hdmi->rpc_info, &cur_tvsystem);

	/* Config info_frame dataint0 */
	if (bpc >= 12)
		dataint0 = RPC_BPC_12;
	else if (bpc >= 10)
		dataint0 = RPC_BPC_10;
	else
		dataint0 = RPC_BPC_8;

	tvsystem.info_frame.dataInt0 = dataint0;

	/* Config videoInfo dataint0 */
	if (get_frame_per_10s(hdmi, mode) == 500)
		dataint0 |= 0x2; /* CVBS PAL */
	else
		dataint0 |= 0x4; /* CVBS NTSC */

	tvsystem.videoInfo.dataInt0 = dataint0;

	if (vic)
		tvsystem.videoInfo.standard = vic_to_vo_standard(vic, hdmi, is_fractional);
	else
		tvsystem.videoInfo.standard = non_cea_to_vo_standard(hdmi, mode);

	tvsystem.videoInfo.enProg = !(mode->flags & DRM_MODE_FLAG_INTERLACE);
	tvsystem.videoInfo.enDIF = 0x1;
	tvsystem.videoInfo.enCompRGB = 0;
	tvsystem.videoInfo.pedType = VO_PEDESTAL_TYPE_300_700_OFF;

	if (hdmi->edid_info.sink_is_hdmi || is_rtk_non_cea_mode(mode) ||
	    hdmi->is_force_on)
		tvsystem.info_frame.hdmiMode = VO_HDMI_ON;
	else
		tvsystem.info_frame.hdmiMode = VO_DVI_ON;

	tvsystem.info_frame.audioSampleFreq = VO_HDMI_AUDIO_48K;
	tvsystem.info_frame.audioChannelCount = 0x1;

	tvsystem.info_frame.dataByte1 = avi_buffer[HDMI_INFOFRAME_HEADER_SIZE];
	tvsystem.info_frame.dataByte2 = avi_buffer[HDMI_INFOFRAME_HEADER_SIZE+1];
	tvsystem.info_frame.dataByte3 = avi_buffer[HDMI_INFOFRAME_HEADER_SIZE+2];
	tvsystem.info_frame.dataByte4 = avi_buffer[HDMI_INFOFRAME_HEADER_SIZE+3];
	tvsystem.info_frame.dataByte5 = avi_buffer[HDMI_INFOFRAME_HEADER_SIZE+4];

	tvsystem.info_frame.hdr_ctrl_mode = hdmi->hdr_mode;

	tmds_config = get_scdc_tmdsconfig(hdmi, mode, bpc);

	if (cur_tvsystem.interfaceType != VO_DIGITAL_ONLY)
		tvsystem.interfaceType = VO_ANALOG_AND_DIGITAL;

	if (is_same_tvsystem(hdmi, &tvsystem, &cur_tvsystem)) {
		if (!is_same_scramble_status(hdmi, tmds_config))
			send_scdc_tmdsconfig(hdmi, mode, tmds_config);

		dev_info(hdmi->dev, "Skip set same tv_system rpc");
		return;
	}

	rtk_hdmi_off(hdmi, OFF_FLAG_AVMUTE);

	send_scdc_tmdsconfig(hdmi, mode, tmds_config);
	if (tmds_config)
		tvsystem.info_frame.hdmi2px_feature |= HDMI2PX_SCRAMBLE;

	dev_info(hdmi->dev, "vic=%u %ux%u %s-%ubit %s",
		vic,
		mode->hdisplay, mode->vdisplay,
		colorspace_mode_list[hdmi->rgb_or_yuv].name,
		bpc,
		hdr_mode_list[hdmi->hdr_mode].name);

	rpc_set_tv_system(hdmi->rpc_info, &tvsystem);

	if (tmds_config)
		send_scdc_tmdsconfig(hdmi, mode, tmds_config);

}

static void rtk_hdmi_set_display_format(struct rtk_hdmi *hdmi,
	struct drm_display_mode *mode)
{
	struct rpc_display_output_format cur_format, format;
	unsigned char phy_state;
	int ret;

	memset_io(&cur_format, 0, sizeof(cur_format));
	memset_io(&format, 0, sizeof(format));

	if ((hdmi->ext_flags & RTK_EXT_SKIP_EDID_AUDIO) ||
		(hdmi->is_force_on) ||
		hdmi->edid_info.sink_is_hdmi)
		format.display_mode = DISPLAY_MODE_HDMI;
	else
		format.display_mode = DISPLAY_MODE_DVI;

	format.vic = drm_match_cea_mode(mode);
	format.is_fractional_fps = is_fractional_mode(hdmi, mode);
	format.colorspace = (enum rtk_hdmi_colorspace)hdmi->rgb_or_yuv;
	format.color_depth = rtk_hdmi_get_bpc(hdmi, mode);
	format.hdr_mode = hdmi->hdr_mode;
	format.src_3d_fmt = HDMI_2D;
	format.dst_3d_fmt = HDMI_2D;
	format.tmds_config = get_scdc_tmdsconfig(hdmi, mode, format.color_depth);

	if (format.color_depth == 8 &&
		(format.hdr_mode == HDR_CTRL_HDR10 || format.hdr_mode == HDR_CTRL_INPUT))
		format.en_dithering = 1;

	ret = rpc_get_display_format(hdmi->rpc_info, &cur_format);
	if (ret)
		dev_err(hdmi->dev, "Fail to get display_format, ret=%d", ret);

	if (format.display_mode == cur_format.display_mode &&
		format.vic == cur_format.vic &&
		format.is_fractional_fps == cur_format.is_fractional_fps &&
		format.colorspace == cur_format.colorspace &&
		format.color_depth == cur_format.color_depth &&
		format.tmds_config == cur_format.tmds_config &&
		format.hdr_mode == cur_format.hdr_mode) {
		if (!is_same_scramble_status(hdmi, format.tmds_config))
			send_scdc_tmdsconfig(hdmi, mode, format.tmds_config);

		dev_info(hdmi->dev, "Skip set same display format rpc");
		return;
	}

	rtk_hdmi_off(hdmi, OFF_FLAG_AVMUTE);

	ret = rtk_hdmi_compute_avi_infoframe(hdmi, mode, &format.avi_infoframe);
	if (ret)
		dev_err(hdmi->dev, "Fail to set avi infoframe, ret=%d", ret);

	send_scdc_tmdsconfig(hdmi, mode, format.tmds_config);

	dev_info(hdmi->dev, "hdmi_new vic=%u fract_fps=%u %s-%ubit %s%s",
		format.vic,
		format.is_fractional_fps,
		colorspace_mode_list[format.colorspace].name,
		format.color_depth,
		hdr_mode_list[format.hdr_mode].name,
		format.en_dithering ? " en_dither":"");

	if (hdmi->ext_flags & RTK_EXT_TMDS_PHY) {
		hdmi->hdmi_ops->set_tmds_phy(hdmi, mode, format);
		format.is_fw_skip_set_hdmi_phy = 1;
	}

	ret = rpc_set_display_format(hdmi->rpc_info, &format);
	if (ret)
		dev_err(hdmi->dev, "Fail to set display_format, ret=%d", ret);

	if (format.tmds_config)
		send_scdc_tmdsconfig(hdmi, mode, format.tmds_config);

	if (!ret) {
		phy_state = check_phy_state(hdmi, format);
		if (!phy_state)
			dev_err(hdmi->dev, "Error: Incorrect PHY state\n");
		else
			dev_info(hdmi->dev, "PHY state is correct\n");
	}

}

static void rtk_hdmi_update_sink_info(struct rtk_hdmi *hdmi)
{
	struct rtk_rpc_info *rpc_info;
	struct rtk_rpc_info *rpc_info_ao;

	memset(&hdmi->edid_info, 0, sizeof(hdmi->edid_info));

	if (hdmi->edid_cache == NULL)
		return;

	rtk_parse_cea_ext(hdmi, hdmi->edid_cache);
	hdmi->edid_info.sink_is_hdmi = drm_detect_hdmi_monitor(hdmi->edid_cache);
	hdmi->edid_info.sink_has_audio = drm_detect_monitor_audio(hdmi->edid_cache);
	hdmi->edid_info.edid_quirks = rtk_edid_get_quirks(hdmi, hdmi->edid_cache);

	rpc_info    = hdmi->rpc_info;
	rpc_info_ao = hdmi->rpc_info_ao;

	if (hdmi->edid_cache->extensions < 8) {
		rpc_send_edid_raw_data(rpc_info, (u8 *)hdmi->edid_cache,
				(hdmi->edid_cache->extensions + 1) * EDID_LENGTH);

		if (rpc_info_ao->ao_in_hifi != NULL) {
			if (*rpc_info_ao->ao_in_hifi && (rpc_info->krpc_vo_opt != rpc_info_ao->krpc_vo_opt))
				rpc_send_edid_raw_data(rpc_info_ao, (u8 *)hdmi->edid_cache,
						(hdmi->edid_cache->extensions + 1) * EDID_LENGTH);
		}
	}

	if ((hdmi->edid_info.scds_pb5 & SCDS_ALLM) &&
			(hdmi->en_allm == ALLM_UNSUPPORTED))
		hdmi->en_allm = ALLM_DISABLE;
	else if (!(hdmi->edid_info.scds_pb5 & SCDS_ALLM) &&
			(hdmi->en_allm == ALLM_DISABLE))
		hdmi->en_allm = ALLM_UNSUPPORTED;

	if (((hdmi->edid_info.vrr_min != 0) ||
			(hdmi->edid_info.scds_pb5 & SCDS_QMS)) &&
			(hdmi->en_qms_vrr == QMS_VRR_UNSUPPORTED))
		hdmi->en_qms_vrr = QMS_VRR_DISABLE;
	else if ((hdmi->edid_info.vrr_min == 0) &&
			!(hdmi->edid_info.scds_pb5 & SCDS_QMS) &&
			(hdmi->en_qms_vrr == QMS_VRR_DISABLE))
		hdmi->en_qms_vrr = QMS_VRR_UNSUPPORTED;
}

static void send_cec_hpd_uevent(struct rtk_hdmi *hdmi, unsigned int hpd)
{
	struct kobject *kobj = &hdmi->dev->kobj;
	char *logaddr_str[2];

	logaddr_str[0] = kasprintf(GFP_KERNEL, "HDMI_HPD=%d", hpd);
	logaddr_str[1] = NULL;
	kobject_uevent_env(kobj, KOBJ_CHANGE, logaddr_str);
	kfree(logaddr_str[0]);
}

static bool rtk_hdmi_update_hpd_state(struct rtk_hdmi *hdmi)
{
	int hpd;
	int rxsense;
	int pre_state;
	int pre_rxsense;
	bool is_connected;
	bool updae_state;

	updae_state = false;

	mutex_lock(&hdmi->hpd_lock);

	hpd = gpiod_get_value(hdmi->hpd_gpio);
	if (hdmi->rxsense_mode)
		rxsense = rtk_hdmi_get_rxsense(hdmi);
	else
		rxsense = hpd;

	if ((hdmi->hpd_state == 0) && (hpd == 1)) {
		if (!IS_ERR_OR_NULL(hdmi->edid_cache)) {
			kfree(hdmi->edid_cache);
			hdmi->edid_cache = NULL;
		}

		hdmi->edid_cache = drm_get_edid(&hdmi->connector, hdmi->ddc);
		if (hdmi->edid_cache) {
			u32 edid_size;

			rtk_hdmi_update_sink_info(hdmi);
			cec_notifier_set_phys_addr_from_edid(hdmi->cec, hdmi->edid_cache);

			if (hdmi->edid_cache->extensions < 8) {
				edid_size = (hdmi->edid_cache->extensions + 1) * EDID_LENGTH;
				dev_info(hdmi->dev, "get edid done, %uBytes", edid_size);

				rtk_hdmi_edid_dump(hdmi, (u8 *)hdmi->edid_cache, edid_size);
			} else {
				dev_err(hdmi->dev, "Invalid edid extensions=%u",
					hdmi->edid_cache->extensions);
			}
		} else {
			dev_err(hdmi->dev, "Failed to get edid");
		}
	}

	if ((hpd != hdmi->hpd_state) || (rxsense != hdmi->rxsense_state)) {
		if (hdmi->rxsense_mode)
			dev_info(hdmi->dev, "HPD(%d) RxSense(%d)\n", hpd, rxsense);
		else
			dev_info(hdmi->dev, "HPD(%d)\n", hpd);
	}

	if (hpd != hdmi->hpd_state)
		if (((hpd == 1) && (hdmi->edid_cache)) || (hpd == 0))
			send_cec_hpd_uevent(hdmi, hpd);

	hdmi->hpd_state = hpd;
	hdmi->rxsense_state = rxsense;

	is_connected = hpd && rxsense;

	mutex_unlock(&hdmi->hpd_lock);

	pre_rxsense = extcon_get_state(hdmi->edev, EXTCON_DISP_DVI);
	if (rxsense != pre_rxsense)
		extcon_set_state_sync(hdmi->edev, EXTCON_DISP_DVI, rxsense);

	pre_state = extcon_get_state(hdmi->edev, EXTCON_DISP_HDMI);
	if (is_connected != pre_state) {
		updae_state = true;
		drm_helper_hpd_irq_event(hdmi->connector.dev);
		extcon_set_state_sync(hdmi->edev, EXTCON_DISP_HDMI, is_connected);
		dev_info(hdmi->dev, "is_connected=%d\n", is_connected);
	}

	if (IS_ENABLED(CONFIG_CHROME_PLATFORMS) &&
		soc_device_match(rtk_soc_kent)) {
		bool is_hdmi_audio;

		is_hdmi_audio = is_connected && hdmi->edid_info.sink_has_audio;

		if ((is_connected != pre_state) && (is_connected == is_hdmi_audio)) {
			if (hdmi->plugged_cb && hdmi->codec_dev)
				hdmi->plugged_cb(hdmi->codec_dev, is_hdmi_audio);
			else
				dev_err(hdmi->dev, "Failed to plugged_cb");
		}
	}

	return updae_state;
}

static const struct rtk_hdmi_ops hdmi_ops = {
	.update_hpd_state = rtk_hdmi_update_hpd_state,
	.audio_output_ctrl = rtk_hdmi_audio_output_ctrl,
	.read_edid_block = rtk_read_edid_block,
	.set_tmds_phy = rtk_hdmi_set_tmds_phy,
};

static int rtk_hdmi_init_properties(struct rtk_hdmi *hdmi)
{
	int ret;
	struct drm_property *prop;

	if (hdmi->connector.funcs->reset)
		hdmi->connector.funcs->reset(&hdmi->connector);

	ret = -ENOMEM;

	hdmi->format_changed = false;

	hdmi->rgb_or_yuv = HDMI_COLORSPACE_RGB;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "RGB or YCbCr",
				colorspace_mode_list,
				ARRAY_SIZE(colorspace_mode_list));
	if (!prop) {
		dev_err(hdmi->dev, "create colorspace enum property failed");
		goto exit;
	}
	hdmi->rgb_or_yuv_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop, hdmi->rgb_or_yuv);

	hdmi->hdr_mode = HDR_CTRL_SDR;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "HDR mode",
				hdr_mode_list,
				ARRAY_SIZE(hdr_mode_list));
	if (!prop) {
		dev_err(hdmi->dev, "create hdr_mode enum property failed");
		goto exit;
	}
	hdmi->hdr_mode_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop, hdmi->hdr_mode);

	hdmi->en_hdmi_5v = HDMI_5V_ENABLE;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "en_hdmi_5v",
				hdmi_5v_status_list,
				ARRAY_SIZE(hdmi_5v_status_list));
	if (!prop) {
		dev_err(hdmi->dev, "create en_hdmi_5v enum property failed");
		goto exit;
	}
	hdmi->hdmi_5v_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop,
		hdmi->en_hdmi_5v);

	hdmi->en_audio = HDMI_AO_AUTO;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "en_hdmi_ao",
				hdmi_ao_status_list,
				ARRAY_SIZE(hdmi_ao_status_list));
	if (!prop) {
		dev_err(hdmi->dev, "create en_hdmi_ao enum property failed");
		goto exit;
	}
	hdmi->en_ao_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop,
			hdmi->en_audio);

	hdmi->en_allm = ALLM_UNSUPPORTED;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "allm",
				allm_status_list,
				ARRAY_SIZE(allm_status_list));
	if (!prop) {
		dev_err(hdmi->dev, "create en_allm enum property failed");
		goto exit;
	}
	hdmi->allm_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop,
		hdmi->en_allm);

	hdmi->en_qms_vrr = QMS_VRR_UNSUPPORTED;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "qms_vrr",
				qms_status_list,
				ARRAY_SIZE(qms_status_list));
	if (!prop) {
		dev_err(hdmi->dev, "create qms_vrr enum property failed");
		goto exit;
	}
	hdmi->qms_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop,
		hdmi->en_qms_vrr);

	hdmi->vrr_rate = RATE_UNSPECIFIED;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "vrr_rate",
				vrr_rate_list,
				ARRAY_SIZE(vrr_rate_list));
	if (!prop) {
		dev_err(hdmi->dev, "create vrr_rate enum property failed");
		goto exit;
	}
	hdmi->vrr_rate_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop,
		hdmi->vrr_rate);

	hdmi->request_auto_mode = HDMI_AUTO_INPUT;
	hdmi->auto_mode = HDMI_AUTO_INPUT;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "Auto mode",
				hdmi_auto_mode_list,
				ARRAY_SIZE(hdmi_auto_mode_list));
	if (!prop) {
		dev_err(hdmi->dev, "create auto mode enum property failed");
		goto exit;
	}
	hdmi->auto_mode_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop,
		hdmi->request_auto_mode);

	hdmi->skip_avmute = SKIP_DISABLE;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "skip_avmute_once",
				skip_avmute_status_list,
				ARRAY_SIZE(skip_avmute_status_list));
	if (!prop) {
		dev_err(hdmi->dev, "create skip avmute once enum property failed");
		goto exit;
	}
	hdmi->skp_avmute_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop,
		hdmi->skip_avmute);

	hdmi->scan_mode = HDMI_SCAN_MODE_UNDERSCAN;
	prop = drm_property_create_enum(hdmi->drm_dev, 0, "scan mode",
				hdmi_scan_mode_list,
				ARRAY_SIZE(hdmi_scan_mode_list));
	if (!prop) {
		dev_err(hdmi->dev, "create scan mode enum property failed");
		goto exit;
	}
	hdmi->scan_mode_property = prop;
	drm_object_attach_property(&hdmi->connector.base, prop,
		hdmi->scan_mode);

	ret = drm_connector_attach_max_bpc_property(&hdmi->connector, 8, 12);
	if (ret)
		goto exit;

	ret = drm_connector_attach_content_type_property(&hdmi->connector);

exit:
	return ret;
}

int rtk_hdmi_conn_set_property(struct drm_connector *connector,
				struct drm_connector_state *state,
				struct drm_property *property,
				uint64_t val)
{
	int ret;
	struct rtk_hdmi *hdmi = to_rtk_hdmi(connector);

	ret = -EINVAL;
	if (property == hdmi->rgb_or_yuv_property) {
		if (hdmi->rgb_or_yuv != val)
			hdmi->format_changed = true;
		hdmi->rgb_or_yuv = val;
		ret = 0;
	} else if ((property == hdmi->hdr_mode_property) &&
				(val != HDR_CTRL_NA0)) {
		hdmi->hdr_mode = val;
		ret = 0;
	} else if (property == hdmi->hdmi_5v_property) {
		ret = rtk_hdmi_set_5v(hdmi, val);
		if (!ret)
			hdmi->en_hdmi_5v = val;
	} else if (property == hdmi->en_ao_property) {
		ret = hdmi->hdmi_ops->audio_output_ctrl(hdmi, (u8)val);
		if (!ret)
			hdmi->en_audio = val;
	} else if ((property == hdmi->allm_property) &&
				(val != ALLM_UNSUPPORTED)) {
		ret = rtk_hdmi_set_allm(hdmi, val);
		DRM_DEBUG_KMS("allm_property val=%llu ret=%d\n", val, ret);
		if (!ret)
			hdmi->en_allm = val;
	} else if ((property == hdmi->qms_property) &&
				(val != hdmi->en_qms_vrr) &&
				(val != QMS_VRR_UNSUPPORTED)) {
		ret = rtk_hdmi_set_qms(hdmi, state, val);
		DRM_DEBUG_KMS("qms_property val=%llu ret=%d\n", val, ret);
		if (!ret)
			hdmi->en_qms_vrr = val;
	} else if ((property == hdmi->vrr_rate_property) &&
				(val != hdmi->vrr_rate) &&
				(val != RATE_UNSPECIFIED) && (val != RATE_BASE)) {
		ret = rtk_hdmi_set_vrr_rate(hdmi, val);
		DRM_DEBUG_KMS("vrr_rate_property val=%llu ret=%d\n", val, ret);
		if (!ret)
			hdmi->vrr_rate = val;
	} else if (property == hdmi->hdcp.force_hdcp14_property) {
		hdmi->hdcp.force_hdcp14 = val;
		ret = 0;
	} else if (property == hdmi->auto_mode_property) {
		if (hdmi->request_auto_mode != val)
			hdmi->format_changed = true;
		hdmi->request_auto_mode = val;
		hdmi->auto_mode = val;
		ret = 0;
	} else if (property == hdmi->skp_avmute_property) {
		hdmi->skip_avmute = val;
		ret = 0;
	} else if (property == hdmi->scan_mode_property) {
		ret = rpc_set_infoframe(hdmi->rpc_info, AVI_SCAN_MODE, 0, val);
		if (!ret)
			hdmi->scan_mode = val;
	}

	return ret;
}

int rtk_hdmi_conn_get_property(struct drm_connector *connector,
				const struct drm_connector_state *state,
				struct drm_property *property,
				uint64_t *val)
{
	struct rtk_hdmi *hdmi = to_rtk_hdmi(connector);

	if (property == hdmi->rgb_or_yuv_property) {
		*val = hdmi->rgb_or_yuv;
		return 0;
	} else if (property == hdmi->hdr_mode_property) {
		*val = hdmi->hdr_mode;
		return 0;
	} else if (property == hdmi->hdmi_5v_property) {
		*val = hdmi->en_hdmi_5v;
		return 0;
	} else if (property == hdmi->en_ao_property) {
		*val = hdmi->en_audio;
		return 0;
	} else if (property == hdmi->allm_property) {
		*val = hdmi->en_allm;
		return 0;
	} else if (property == hdmi->qms_property) {
		*val = hdmi->en_qms_vrr;
		return 0;
	} else if (property == hdmi->vrr_rate_property) {
		*val = hdmi->vrr_rate;
		return 0;
	} else if (property == hdmi->hdcp.force_hdcp14_property) {
		*val = hdmi->hdcp.force_hdcp14;
		return 0;
	} else if (property == hdmi->auto_mode_property) {
		*val = hdmi->request_auto_mode;
		return 0;
	} else if (property == hdmi->skp_avmute_property) {
		*val = hdmi->skip_avmute;
		return 0;
	} else if (property == hdmi->scan_mode_property) {
		*val = hdmi->scan_mode;
		return 0;
	}

	return -EINVAL;
}

static void rtk_hdmi_setup(struct rtk_hdmi *hdmi,
			     struct drm_display_mode *mode)
{
	struct rtk_hdcp *hdcp = &hdmi->hdcp;
	int ret;

	if (hdmi->request_auto_mode == HDMI_AUTO_INPUT)
		hdmi->rgb_or_yuv = rtk_hdmi_auto_input(hdmi, mode);

	if ((hdmi->auto_mode != HDMI_AUTO_DISABLE) &&
		(hdmi->auto_mode != HDMI_AUTO_INPUT)) {
		hdmi->hdr_mode = HDR_CTRL_SDR;
		hdmi->rgb_or_yuv = rtk_hdmi_get_color_space(hdmi, mode);
	}

	dev_info(hdmi->dev, "Set auto mode = %s", hdmi_auto_mode_list[hdmi->auto_mode].name);

	if (hdmi->is_new_mac)
		rtk_hdmi_set_display_format(hdmi, mode);
	else
		hdmitx_set_video_timing(hdmi, mode);

	drm_mode_copy(&hdmi->previous_mode, mode);

	if (hdmi->is_new_mac && hdcp->hdcp_ops) {
		ret = -EFAULT;

		if (hdcp->hdcp_ops->set_rekey_win)
			ret = hdcp->hdcp_ops->set_rekey_win(hdmi, 4);

		if (ret == 0)
			dev_info(hdmi->dev, "Set rekey_win = 4");
	}

	if (hdmi->ext_flags & RTK_EXT_SKIP_EDID_AUDIO) {
		hdmi->edid_info.sink_has_audio = true;
		dev_info(hdmi->dev, "Skip check edid audio\n");
	}

	if (hdmi->en_audio == HDMI_AO_AUTO &&
		hdmi->edid_info.sink_has_audio &&
		!is_rtk_non_cea_mode(mode))
		hdmi->hdmi_ops->audio_output_ctrl(hdmi, 1);

	if (!hdmi->is_new_mac) {
		ret = rpc_set_spd_infoframe(hdmi->rpc_info, RTK_HDMI_SPD_ENABLE,
				"Realtek", "media player", HDMI_SPD_SDI_DSTB);
		if (ret)
			dev_err(hdmi->dev, "Fail to set SPD infoframe, ret=%d\n", ret);
	}

	rtk_hdmi_send_avmute(hdmi, 0);
	hdmi->is_hdmi_on = true;

	if (hdmi->hdcp_support &&
		hdmi->hdcp.value == DRM_MODE_CONTENT_PROTECTION_DESIRED) {
		u32 delay_ms;

		if (hdmi->edid_info.edid_quirks & RTK_EDID_QUIRK_HDCP_DELAY_2S)
			delay_ms = 2000;
		else
			delay_ms = 0;

		schedule_delayed_work(&hdmi->hdcp.commit_work,
			msecs_to_jiffies(delay_ms));
	}

}

static bool get_cvbs_extcon_state(struct rtk_hdmi *hdmi)
{
	struct device_node *cvbs_np;
	bool cvbs_connected = false;

	cvbs_np = of_find_compatible_node(NULL, NULL, "realtek,rtk-cvbs");
	if (!cvbs_np)
		return false;

	if (of_device_is_available(cvbs_np)) {
		hdmi->cvbs_edev = extcon_get_extcon_dev("9804d140.cvbs");

		if (IS_ERR(hdmi->cvbs_edev)) {
			dev_err(hdmi->dev, "Failed to get cvbs extcon");
			return false;
		}

		cvbs_connected = extcon_get_state(hdmi->cvbs_edev, EXTCON_JACK_VIDEO_OUT);
	}

	return cvbs_connected;

}

static bool rtk_hdmi_enc_mode_fixup(struct drm_encoder *encoder,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	return true;
}
static int rtk_hdmi_enc_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	return 0;
}

static void rtk_hdmi_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	struct rtk_hdmi *hdmi = to_rtk_hdmi(encoder);
	u32 interlace;
	u32 fp10s;

	interlace = mode->flags & DRM_MODE_FLAG_INTERLACE;
	fp10s = get_frame_per_10s(hdmi, mode);

	dev_info(hdmi->dev, "Mode set %ux%u%s fp10s=%u",
		mode->hdisplay,
		mode->vdisplay,
		interlace ? "i":"p",
		fp10s);

	drm_mode_copy(&hdmi->previous_mode, mode);
}

static void rtk_hdmi_enc_enable(struct drm_encoder *encoder)
{
	struct rtk_hdmi *hdmi = to_rtk_hdmi(encoder);

	dev_info(hdmi->dev, "Enable encoder");

	if (hdmi->ext_flags & RTK_EXT_CVBS_HPD) {
		if (get_cvbs_extcon_state(hdmi) &&
			(hdmi->connector.status != connector_status_connected)) {
			dev_info(hdmi->dev,
				"Skip enable hdmi when HDMI disconnected and CVBS connected");
			return;
		}
	}

	rtk_hdmi_setup(hdmi, &hdmi->previous_mode);

	hdmi->format_changed = false;
}

static void rtk_hdmi_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_hdmi *hdmi = to_rtk_hdmi(encoder);

	dev_info(hdmi->dev, "Disable encoder");

	if (hdmi->skip_avmute == SKIP_ENABLE)
		rtk_hdmi_off(hdmi, OFF_FLAG_SKIP_AVMUTE);
	else
		rtk_hdmi_off(hdmi, OFF_FLAG_NORMAL);

	hdmi->skip_avmute = SKIP_DISABLE;

}

static const struct drm_encoder_funcs rtk_hdmi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs rtk_hdmi_encoder_helper_funcs = {
	.mode_fixup = rtk_hdmi_enc_mode_fixup,
	.atomic_check = rtk_hdmi_enc_atomic_check,
	.mode_set   = rtk_hdmi_enc_mode_set,
	.enable     = rtk_hdmi_enc_enable,
	.disable    = rtk_hdmi_enc_disable,
};

static enum drm_connector_status
rtk_hdmi_conn_detect(struct drm_connector *connector, bool force)
{
	struct rtk_hdmi *hdmi;
	bool is_connected;
	struct rtk_drm_private *priv;

	hdmi = to_rtk_hdmi(connector);
	priv = hdmi->drm_dev->dev_private;

	if (hdmi->check_connector_limit &&
		 !rtk_drm_can_add_connector(priv, RTK_CONNECTOR_HDMI)) {
		dev_info(hdmi->dev, "block hdmi because the limit is reached\n");
		return connector_status_disconnected;
	}

	mutex_lock(&hdmi->hpd_lock);

	if (hdmi->rxsense_mode)
		is_connected = hdmi->hpd_state && hdmi->rxsense_state;
	else
		is_connected = hdmi->hpd_state;

	mutex_unlock(&hdmi->hpd_lock);

	if (hdmi->check_connector_limit)
		rtk_drm_update_connector(priv, RTK_CONNECTOR_HDMI, is_connected);

	return is_connected ?
		connector_status_connected : connector_status_disconnected;
}

static void rtk_hdmi_conn_destroy(struct drm_connector *connector)
{
	DRM_DEBUG_DRIVER("destroy hdmi connector");
	drm_connector_cleanup(connector);
}

static int rtk_hdmi_conn_get_modes(struct drm_connector *connector)
{
	struct rtk_hdmi *hdmi = to_rtk_hdmi(connector);
	struct drm_display_mode *mode;
	bool match_rtk_mode = false;
	int ret = 0;
	int i;
	u8 vic;
	int hpd;

	DRM_DEBUG_DRIVER("get hdmi modes, connector.status=%u",
		hdmi->connector.status);

	if ((!hdmi->ddc) ||
		(hdmi->connector.status != connector_status_connected))
		return 0;

	mutex_lock(&hdmi->hpd_lock);

	if (IS_ERR_OR_NULL(hdmi->edid_cache)) {
		hpd = gpiod_get_value(hdmi->hpd_gpio);
		if (hpd == 1) {
			hdmi->edid_cache = drm_get_edid(&hdmi->connector, hdmi->ddc);
			if (hdmi->edid_cache) {
				rtk_hdmi_update_sink_info(hdmi);
				cec_notifier_set_phys_addr_from_edid(hdmi->cec, hdmi->edid_cache);
			}
		}
	}

	if (hdmi->is_force_on) {
		ret += rtk_add_force_modes(connector);
	} else {
		if (!IS_ERR_OR_NULL(hdmi->edid_cache)) {
			drm_connector_update_edid_property(connector, hdmi->edid_cache);
			ret += drm_add_edid_modes(connector, hdmi->edid_cache);
			ret += rtk_add_more_ext_modes(connector, hdmi->edid_cache);
			ret += rtk_add_ovt_modes(connector, hdmi->edid_cache);

			if (hdmi->edid_info.edid_quirks & RTK_EDID_QUIRK_MODES_ADD)
				ret += rtk_add_quirk_modes(connector, hdmi->edid_cache);
		}
	}

	list_for_each_entry(mode, &connector->probed_modes, head) {
		vic = drm_match_cea_mode(mode);
		if (rtk_hdmi_valid_vic(vic)) {
			match_rtk_mode = true;
			break;
		}
	}

	if (ret == 0 || !match_rtk_mode) {
		for (i = 0; i < ARRAY_SIZE(rtk_default_modes); i++) {
			mode = drm_mode_duplicate(connector->dev, &rtk_default_modes[i]);
			if (mode) {
				drm_mode_probed_add(connector, mode);
				++ret;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(rtk_non_cea_modes); i++) {
		mode = drm_mode_duplicate(connector->dev, &rtk_non_cea_modes[i]);
		if (mode) {
			drm_mode_probed_add(connector, mode);
			++ret;
		}
	}

	mutex_unlock(&hdmi->hpd_lock);

	return ret;
}

static enum drm_mode_status
rtk_hdmi_conn_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	struct rtk_hdmi *hdmi = to_rtk_hdmi(connector);
	u8 vic;
	int max_tmds_khz;

	vic = drm_match_cea_mode(mode);

	DRM_DEBUG_DRIVER("vic=%u\n", vic);

	max_tmds_khz = connector->display_info.max_tmds_clock;

	if (hdmi->is_force_on)
		return MODE_OK;

	if ((hdmi->ext_flags & RTK_EXT_DIS_4K) && (vic >= VIC_3840X2160P24))
		return MODE_CLOCK_HIGH;

	if ((hdmi->ext_flags & RTK_EXT_DIS_4KP30_UPPER) && (vic > VIC_3840X2160P30))
		return MODE_CLOCK_HIGH;

	if ((vic >= VIC_3840X2160P24) && (max_tmds_khz < 297000))
		return MODE_CLOCK_HIGH;

#ifdef CONFIG_CHROME_PLATFORMS
	if (mode->clock > hdmi->max_clock_k)
		return MODE_CLOCK_HIGH;
#endif

	if (rtk_hdmi_valid_vic(vic))
		return MODE_OK;

	if (is_rtk_non_cea_mode(mode))
		return MODE_OK;

	return MODE_ERROR;
}

static int rtk_hdmi_conn_atomic_check(struct drm_connector *connector,
					  struct drm_atomic_state *state)
{
	struct drm_connector_state *old_connector_state;
	struct drm_crtc_state *new_crtc_state;
	struct rtk_hdmi *hdmi = to_rtk_hdmi(connector);

	old_connector_state = drm_atomic_get_old_connector_state(state, connector);

	if (old_connector_state->crtc) {
		new_crtc_state = drm_atomic_get_new_crtc_state(state, old_connector_state->crtc);
		if (hdmi->format_changed) {
			new_crtc_state->connectors_changed = true;
			dev_info(hdmi->dev,"atomic check format changed");
		}
	}

	return 0;
}

static const struct drm_connector_funcs rtk_hdmi_connector_funcs = {
//	.dpms = drm_atomic_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = rtk_hdmi_conn_detect,
	.destroy = rtk_hdmi_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_set_property = rtk_hdmi_conn_set_property,
	.atomic_get_property = rtk_hdmi_conn_get_property,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs rtk_hdmi_connector_helper_funcs = {
	.get_modes = rtk_hdmi_conn_get_modes,
	.mode_valid = rtk_hdmi_conn_mode_valid,
	.atomic_check = rtk_hdmi_conn_atomic_check,
};

static void rtk_hdmi_rxsense_timer_cb(struct timer_list *rxsense_timer)
{
	struct rtk_hdmi *hdmi;
	int curr_rxsense;
	unsigned char need_update;

	hdmi = to_rtk_hdmi(rxsense_timer);

	curr_rxsense = rtk_hdmi_get_rxsense(hdmi);
	need_update = (curr_rxsense != hdmi->rxsense_state) ? 1:0;

	if (need_update)
		schedule_work(&hdmi->hpd_work);

	if (hdmi->hpd_state)
		mod_timer(rxsense_timer, jiffies + msecs_to_jiffies(30));
}

static void rtk_hdmi_hpd_worker(struct work_struct *hpd_work)
{
	struct rtk_hdmi *hdmi;
	bool updae_state;

	hdmi = to_rtk_hdmi(hpd_work);

	updae_state = hdmi->hdmi_ops->update_hpd_state(hdmi);

	if (hdmi->hdcp_support && updae_state &&
	   hdmi->connector.status == connector_status_connected) {
		rtk_hdcp_update_cap(hdmi, WV_HDCP_NONE);
		rtk_hdcp_set_state(hdmi, HDCP_UNAUTH, HDCP_NO_ERR);
	} else if (hdmi->hdcp_support && updae_state &&
		hdmi->connector.status == connector_status_disconnected) {
		cancel_delayed_work_sync(&hdmi->hdcp.commit_work);
		rtk_hdcp_disable(hdmi);
		schedule_work(&hdmi->hdcp.prop_work);
		rtk_hdcp_update_cap(hdmi, WV_HDCP_NO_DIGITAL_OUTPUT);
		rtk_hdcp_set_state(hdmi, HDCP_HDMI_DISCONNECT, HDCP_NO_ERR);
	}

	if (hdmi->hpd_state && hdmi->rxsense_mode == RXSENSE_TIMER_MODE)
		mod_timer(&hdmi->rxsense_timer, jiffies + msecs_to_jiffies(30));
}

static irqreturn_t rtk_hdmi_hpd_irq(int irq, void *dev_id)
{
	struct rtk_hdmi *hdmi = dev_id;

	schedule_work(&hdmi->hpd_work);

	return IRQ_HANDLED;
}

static int rtk_hdmi_check_ops(const struct rtk_hdmi_ops *hdmi_ops)
{
	if (!hdmi_ops)
		return -EINVAL;

	if (!hdmi_ops->update_hpd_state ||
		!hdmi_ops->audio_output_ctrl ||
		!hdmi_ops->read_edid_block ||
		!hdmi_ops->set_tmds_phy)
		return -EFAULT;

	return 0;
}

static int rtk_hdmi_audio_hw_params(struct device *dev, void *data,
	struct hdmi_codec_daifmt *daifmt, struct hdmi_codec_params *params)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);
	int ret = 0;

	dev_info(hdmi->dev, "audio hw params");

	return ret;
}

static void rtk_hdmi_audio_shutdown(struct device *dev, void *data)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);

	dev_info(hdmi->dev, "audio shutdown");
}

static int rtk_hdmi_audio_hook_plugged_cb(struct device *dev, void *data,
	hdmi_codec_plugged_cb fn, struct device *codec_dev)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);
	bool is_connected;
	bool is_hdmi_audio;

	dev_info(hdmi->dev, "audio hook plugged cb");

	hdmi->plugged_cb = fn;
	hdmi->codec_dev = codec_dev;

	is_connected = extcon_get_state(hdmi->edev, EXTCON_DISP_HDMI);
	is_hdmi_audio = is_connected && hdmi->edid_info.sink_has_audio;

	if (hdmi->plugged_cb && hdmi->codec_dev)
		hdmi->plugged_cb(hdmi->codec_dev, is_hdmi_audio);
	else
		dev_err(hdmi->dev, "Failed to plugged_cb");

	return 0;
}

static const struct hdmi_codec_ops rtk_hdmi_audio_codec_ops = {
	.hw_params = rtk_hdmi_audio_hw_params,
	.audio_shutdown = rtk_hdmi_audio_shutdown,
	.hook_plugged_cb = rtk_hdmi_audio_hook_plugged_cb,
};

static int rtk_hdmi_audio_codec_register(struct device *dev)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);
	struct platform_device *pdev;
	struct hdmi_codec_pdata codec_data = {
		.spdif   = 1,
		.data = hdmi,
		.ops   = &rtk_hdmi_audio_codec_ops,
	};

	pdev = platform_device_register_data(dev,
		HDMI_CODEC_DRV_NAME, PLATFORM_DEVID_AUTO,
		&codec_data, sizeof(codec_data));

	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	dev_info(hdmi->dev, "%s driver bound to HDMI\n", HDMI_CODEC_DRV_NAME);

	return 0;
}

static ssize_t show_rtk_force_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);
	int force;

	if (hdmi->is_force_on)
		force = 1;
	else
		force = 0;

	return sprintf(buf, "%d\n", force);
}

static ssize_t store_rtk_force_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);
	int force;

	if (sscanf(buf, "%d", &force) != 1 ||
		(force < 0 || force > 1))
		return -EINVAL;

	if (force == 0) {
		hdmi->connector.force = DRM_FORCE_UNSPECIFIED;
		hdmi->is_force_on = false;
	} else {
		hdmi->connector.force = DRM_FORCE_ON;
		hdmi->is_force_on = true;
	}

	return count;
}

/* /sys/devices/platform/hdmi/force_mode */
static DEVICE_ATTR(force_mode, 0644, show_rtk_force_mode, store_rtk_force_mode);

static void register_rtk_hdmi_sysfs(struct device *dev)
{
	device_create_file(dev, &dev_attr_force_mode);
}

static int rtk_hdmi_bind(struct device *dev, struct device *master,
				 void *data)
{
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct device_node *syscon_np;
	struct device_node *audio_out_np;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_hdmi *hdmi;
	int ret;
	int size;
	int hpd_state;
	int rxsense;
	const u32 *prop;

	hdmi = devm_kzalloc(dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_CHROME_PLATFORMS)) {
		ret = of_reserved_mem_device_init(dev);
		if (ret)
			dev_warn(dev, "init reserved memory failed");
	}

	hdmi->drm_dev = drm;
	hdmi->dev = dev;
	dev_set_drvdata(dev, hdmi);

	/* Register sysfs */
	register_rtk_hdmi_sysfs(dev);

	prop = of_get_property(dev->of_node, "is-new-mac", &size);
	if (prop)
		hdmi->is_new_mac = of_read_number(prop, 1);
	else
		hdmi->is_new_mac = 0;

	dev_info(dev, "is_new_mac=%u\n", hdmi->is_new_mac);

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 0 fail");
		ret = -ENODEV;
		goto err_exit;
	}

	hdmi->hdmireg = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(hdmi->hdmireg)) {
		dev_err(dev, "Remap syscon 0 to hdmireg fail");
		of_node_put(syscon_np);
		ret = PTR_ERR(hdmi->hdmireg);
		goto err_exit;
	}

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 1);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 1 fail");
		ret = -ENODEV;
		goto err_exit;
	}

	hdmi->crtreg = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(hdmi->crtreg)) {
		dev_err(dev, "Remap syscon 1 to crtreg fail");
		of_node_put(syscon_np);
		ret = PTR_ERR(hdmi->crtreg);
		goto err_exit;
	}

	ret = of_property_read_u32(dev->of_node, "rxsense-mode",
				&hdmi->rxsense_mode);

	if (ret < 0 || hdmi->rxsense_mode > RXSENSE_INTERRUPT_MODE)
		hdmi->rxsense_mode = RXSENSE_PASSIVE_MODE;

	if (hdmi->rxsense_mode == RXSENSE_PASSIVE_MODE && !hdmi->is_new_mac)
		goto skip_topreg;

	syscon_np = of_parse_phandle(dev->of_node, "syscon", 2);
	if (IS_ERR_OR_NULL(syscon_np)) {
		dev_err(dev, "Parse syscon phandle 2 fail");
		ret = -ENODEV;
		goto err_exit;
	}

	hdmi->topreg = syscon_node_to_regmap(syscon_np);
	if (IS_ERR(hdmi->topreg)) {
		dev_err(dev, "Remap syscon 2 to topreg fail");
		of_node_put(syscon_np);
		ret = PTR_ERR(hdmi->topreg);
		goto err_exit;
	}

skip_topreg:

	hdmi->reset_hdmi = devm_reset_control_get_optional_exclusive(dev, "rstn_hdmi");
	if (IS_ERR(hdmi->reset_hdmi))
		return dev_err_probe(dev, PTR_ERR(hdmi->reset_hdmi),
					"Can't get reset_control rstn_hdmi\n");

	hdmi->clk_hdmi = devm_clk_get(dev, "clk_en_hdmi");
	if (IS_ERR(hdmi->clk_hdmi))
		return dev_err_probe(dev, PTR_ERR(hdmi->clk_hdmi),
					"Can't get clk clk_hdmi\n");

	prop = of_get_property(dev->of_node, "hdcp", &size);
	if (prop) {
		hdmi->hdcp_support = of_read_number(prop, 1);
		dev_info(dev, "get hdcp on/off setting: 0x%x\n",
			 hdmi->hdcp_support);
	} else {
		dev_info(dev, "no hdcp on/off setting\n");
		hdmi->hdcp_support = 0;
	}

	hdmi->hpd_gpio = devm_gpiod_get(dev, "hpd", GPIOD_IN);
	if (IS_ERR(hdmi->hpd_gpio))
		return dev_err_probe(dev, PTR_ERR(hdmi->hpd_gpio),
					"Could not get hpd gpio\n");

	dev_info(dev, "hotplug gpio(%d)\n", desc_to_gpio(hdmi->hpd_gpio));

	hdmi->hdmi5v_gpio = NULL;

	hdmi->supply = devm_regulator_get_optional(dev, "hdmi");
	if (IS_ERR(hdmi->supply)) {
		if (PTR_ERR(hdmi->supply) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		hdmi->supply = NULL;
	} else {
		dev_info(dev, "Got hdmi 5v regulator\n");
		ret = regulator_enable(hdmi->supply);
		if (ret)
			dev_err(dev, "failed to enable hdmi_5v via regulator");
		goto skip_5v_gpio;
	}

	hdmi->hdmi5v_gpio = devm_gpiod_get(dev, "hdmi5v", GPIOD_OUT_HIGH);
	if (IS_ERR(hdmi->hdmi5v_gpio))
		dev_info(dev, "Not support hdmi_5v control\n");
	else
		dev_info(dev, "hdmi5v gpio(%d)\n", desc_to_gpio(hdmi->hdmi5v_gpio));

skip_5v_gpio:

	hdmi->edev = devm_extcon_dev_allocate(dev, rtk_hdmi_cable);
	if (IS_ERR(hdmi->edev))
		return dev_err_probe(dev, PTR_ERR(hdmi->edev),
					"failed to allocate extcon device\n");

	ret = devm_extcon_dev_register(dev, hdmi->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device");
		goto err_dis_reg;
	}

	prop = of_get_property(dev->of_node, "low-rates-scramble", &size);
	if (prop)
		hdmi->lr_scramble = of_read_number(prop, 1);
	else
		hdmi->lr_scramble = 0;

	dev_info(dev, "lr_scramble=%u\n", hdmi->lr_scramble);

	hdmi->ao_in_hifi = 0;
	for_each_matching_node(audio_out_np, audio_match_table) {
		if (of_device_is_available(audio_out_np)) {
			dev_info(dev, "Found %s node", audio_out_np->name);

			ret = of_property_read_u32(audio_out_np, "remote-cpu", &hdmi->ao_in_hifi);
			if (ret)
				hdmi->ao_in_hifi = 0;
		}
	}
	dev_info(dev, "ao_in_hifi=%u\n", hdmi->ao_in_hifi);

	ret = gpiod_direction_input(hdmi->hpd_gpio);
	if (ret) {
		dev_err(dev, "failed to set hpd_gpio direction");
		goto err_dis_reg;
	}

	ret = gpiod_set_debounce(hdmi->hpd_gpio, 30*1000); /* 30ms */
	if (ret)
		dev_info(dev, "failed to set hpd_gpio debounce");

	ret = of_property_read_u32(dev->of_node, "ext-flags",
				&hdmi->ext_flags);
	if (ret < 0)
		hdmi->ext_flags = 0;

	if (hdmi->ext_flags)
		dev_info(dev, "ext_flags=0x%08x", hdmi->ext_flags);

	hdmi->ddc = i2c_get_adapter(I2C_BUS_ID);
	if (IS_ERR(hdmi->ddc) || !hdmi->ddc) {
		dev_err(dev, "Get i2c adapter fail\n");
		ret = -EPROBE_DEFER;//PTR_ERR(hdmi->ddc);
		hdmi->ddc = NULL;
		goto err_dis_reg;
	}

	if (hdmi->ext_flags & RTK_EXT_TMDS_PHY) {
		ret = rtk_hdmi_parse_tmds_phy(hdmi);
		if (ret < 0)
			hdmi->ext_flags &= ~RTK_EXT_TMDS_PHY;
	}

	ret = of_property_read_u32(dev->of_node, "swap-config",
			&hdmi->swap_config);
	if (ret < 0)
		hdmi->swap_config = 0;

	dev_info(dev, "swap_config=%u\n", hdmi->swap_config);

	hdmi->check_connector_limit = of_property_read_bool(dev->of_node,
												 "check-connector-limit");
	if (hdmi->check_connector_limit)
		dev_info(dev, "check connector limit\n");

	ret = of_property_read_u32(dev->of_node, "max-clock-k",
				&hdmi->max_clock_k);
	if (ret < 0)
		hdmi->max_clock_k = RTK_HDMI_MAX_CLOCK_K;
	dev_info(dev, "max-clock-k=%u\n", hdmi->max_clock_k);

	encoder = &hdmi->encoder;
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	dev_info(dev, "hdmi possible_crtcs (0x%x)\n", encoder->possible_crtcs);

	if (encoder->possible_crtcs == 0) {
		ret = -EPROBE_DEFER;
		goto err_put_i2c;
	}

	drm_encoder_init(drm, encoder, &rtk_hdmi_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);

	drm_encoder_helper_add(encoder, &rtk_hdmi_encoder_helper_funcs);

	connector = &hdmi->connector;
	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->interlace_allowed = true;
	connector->ycbcr_420_allowed = true;
	ret = drm_connector_init_with_ddc(drm, connector, &rtk_hdmi_connector_funcs,
			   DRM_MODE_CONNECTOR_HDMIA, hdmi->ddc);
	if (ret) {
		dev_err(dev, "connector_init failed");
		goto err_put_i2c;
	}
	drm_connector_helper_add(connector, &rtk_hdmi_connector_helper_funcs);

	drm_connector_attach_encoder(connector, encoder);

	rtk_hdmi_init_properties(hdmi);

	hdmi->hdmi_ops = &hdmi_ops;
	ret = rtk_hdmi_check_ops(hdmi->hdmi_ops);
	if (ret)
		goto err_put_i2c;

	if (priv->krpc_second == 1)
		hdmi->rpc_info = &priv->rpc_info[RTK_RPC_SECONDARY];
	else
		hdmi->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];

	if (priv->rpc_info_num == 1)
		hdmi->rpc_info_ao = &priv->rpc_info[RTK_RPC_MAIN];
	else
		hdmi->rpc_info_ao = &priv->rpc_info[RTK_RPC_SECONDARY];

	dev_info(dev, "hdmi vo rpc_info (%p) is on %s\n",
		hdmi->rpc_info, krpc_names[hdmi->rpc_info->krpc_vo_opt]);

	dev_info(dev, "hdmi ao rpc_info (%p) is on %s\n",
		hdmi->rpc_info_ao, krpc_names[hdmi->rpc_info_ao->krpc_vo_opt]);

	hdmi->rpc_info->hdmi_new_mac = &hdmi->is_new_mac;
	hdmi->rpc_info_ao->ao_in_hifi = &hdmi->ao_in_hifi;

	/* get cec notifier */
	hdmi->cec = cec_notifier_conn_register(dev, NULL, NULL);
	if (!hdmi->cec) {
		ret = -EPROBE_DEFER;
		goto err_put_i2c;
	}

	cec_notifier_phys_addr_invalidate(hdmi->cec);

	if (hdmi->hdcp_support) {
		ret = rtk_hdcp_init(hdmi, hdmi->hdcp_support);
		if (ret) {
			dev_err(dev, "RealTek HDCP init failed\n");
			hdmi->hdcp_support = 0;
		}
	}

	mutex_init(&hdmi->hpd_lock);
	INIT_WORK(&hdmi->hpd_work, rtk_hdmi_hpd_worker);

	hdmi->rxsense_state = 0;
	if (hdmi->rxsense_mode == RXSENSE_INTERRUPT_MODE) {
		hdmi->rxsense_irq = irq_of_parse_and_map(dev->of_node, 0);
		if (hdmi->rxsense_irq < 0) {
			dev_err(dev, "Fail to get rxsense_irq");
			hdmi->rxsense_mode = RXSENSE_PASSIVE_MODE;
		} else {
			ret = rtk_hdmi_enable_rxsense_int(hdmi);
			if (ret)
				hdmi->rxsense_mode = RXSENSE_PASSIVE_MODE;
		}
	} else if (hdmi->rxsense_mode == RXSENSE_TIMER_MODE) {
		timer_setup(&hdmi->rxsense_timer, rtk_hdmi_rxsense_timer_cb, 0);
	}
	dev_info(dev, "rxsense_mode=%d", hdmi->rxsense_mode);

	hdmi->hpd_irq = gpiod_to_irq(hdmi->hpd_gpio);
	if (hdmi->hpd_irq < 0) {
		dev_err(dev, "Fail to get hpd_irq");
		ret = hdmi->hpd_irq;
		goto err_unreg_cec;
	}

	irq_set_irq_type(hdmi->hpd_irq, IRQ_TYPE_EDGE_BOTH);
	ret = devm_request_irq(dev, hdmi->hpd_irq, rtk_hdmi_hpd_irq,
				IRQF_SHARED, dev_name(dev), hdmi);

	hpd_state = gpiod_get_value(hdmi->hpd_gpio);
	if (hpd_state)
		schedule_work(&hdmi->hpd_work);

	if (hdmi->rxsense_mode)
		rxsense = rtk_hdmi_get_rxsense(hdmi);
	else
		rxsense = hpd_state;

	if (!hpd_state || !rxsense)
		rtk_hdmi_off(hdmi, OFF_FLAG_NONE);

	if (ret) {
		dev_err(dev, "can't request hpd gpio irq\n");
		goto err_unreg_cec;
	}

	if (IS_ENABLED(CONFIG_CHROME_PLATFORMS) &&
		soc_device_match(rtk_soc_kent)) {
		ret = rtk_hdmi_audio_codec_register(dev);
		if (ret) {
			dev_err(dev, "Failed to register audio driver");
			goto err_unreg_cec;
		}
	}

	return 0;

err_unreg_cec:
	//cec_notifier_conn_unregister(hdmi->cec);
err_put_i2c:
	i2c_put_adapter(hdmi->ddc);
err_dis_reg:
	if (!IS_ERR_OR_NULL(hdmi->supply))
		regulator_disable(hdmi->supply);
err_exit:
	return ret;
}

static void rtk_hdmi_unbind(struct device *dev, struct device *master,
			     void *data)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);

	devm_free_irq(dev, hdmi->hpd_irq, hdmi);

	cancel_work_sync(&hdmi->hpd_work);

	if (hdmi->rxsense_mode == RXSENSE_TIMER_MODE)
		del_timer_sync(&hdmi->rxsense_timer);

	i2c_put_adapter(hdmi->ddc);

	if (!IS_ERR_OR_NULL(hdmi->supply))
		regulator_disable(hdmi->supply);

	//cec_notifier_conn_unregister(hdmi->cec);
}

static const struct component_ops rtk_hdmi_ops = {
	.bind	= rtk_hdmi_bind,
	.unbind	= rtk_hdmi_unbind,
};

static int rtk_hdmi_suspend(struct device *dev)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);
	struct drm_scdc *scdc = &hdmi->connector.display_info.hdmi.scdc;
	struct drm_connector *connector = &hdmi->connector;

	if (!hdmi)
		return 0;

	hdmi->in_suspend = true;

	disable_irq(hdmi->hpd_irq);

	if (hdmi->rxsense_mode == RXSENSE_INTERRUPT_MODE)
		disable_irq(hdmi->rxsense_irq);
	else if (hdmi->rxsense_mode == RXSENSE_TIMER_MODE)
		del_timer_sync(&hdmi->rxsense_timer);

	cancel_work_sync(&hdmi->hpd_work);

	if (hdmi->hdcp_support &&
		hdmi->hdcp.value != DRM_MODE_CONTENT_PROTECTION_UNDESIRED) {
		hdmi->hdcp.do_cancel_hdcp = true;
		dev_err(dev, "userspace didn't set hdcp to UNDESIRED before suspend");
	}

	rtk_hdmi_off(hdmi, OFF_FLAG_HDCP | OFF_FLAG_DIS_AUDIO);

	/* DV mode doesn't support S3 resume */
	if ((hdmi->hdr_mode != HDR_CTRL_HDR10) &&
		(hdmi->hdr_mode != HDR_CTRL_HLG) &&
		(hdmi->hdr_mode != HDR_CTRL_INPUT) &&
		(hdmi->hdr_mode != HDR_CTRL_INPUT_BT2020)) {
		hdmi->hdr_mode = HDR_CTRL_SDR;
	}

	/* Send no scramble for prevent HDMI 5V still exist after suspend */
	if (hdmi->hpd_state && scdc->supported) {
		drm_scdc_set_high_tmds_clock_ratio(connector, false);
		drm_scdc_set_scrambling(connector, false);
	}

	hdmi->hpd_state = 0;
	hdmi->rxsense_state = 0;

	return 0;
}

static int rtk_hdmi_resume(struct device *dev)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(dev);
	struct rtk_hdcp *hdcp = &hdmi->hdcp;
	int hpd;

	if (!hdmi)
		return 0;

	hdmi->in_suspend = false;

	gpiod_set_debounce(hdmi->hpd_gpio, 30*1000);

	if (!hdmi->hdcp_support)
		goto skip_hdcp;

	hpd = gpiod_get_value(hdmi->hpd_gpio);
	if (hpd) {
		bool capable = false;

		hdcp->hdcp_state = HDCP_UNAUTH;
		hdcp->hdcp_ops->hdcp2_capable(hdmi, &capable);
		if (capable) {
			hdcp->sink_hdcp_ver = HDCP_2x;
		} else {
			hdcp->hdcp_ops->hdcp_capable(hdmi, &capable);
			hdcp->sink_hdcp_ver = capable ? HDCP_1x : HDCP_NONE;
		}
	} else {
		hdcp->sink_hdcp_ver = HDCP_NONE;
		hdcp->hdcp_state = HDCP_HDMI_DISCONNECT;
	}

	hdmi->hdcp.do_cancel_hdcp = false;
skip_hdcp:

	hdmi->hdmi_ops->update_hpd_state(hdmi);

	enable_irq(hdmi->hpd_irq);

	if (hdmi->rxsense_mode == RXSENSE_INTERRUPT_MODE)
		enable_irq(hdmi->rxsense_irq);
	else if (hdmi->rxsense_mode == RXSENSE_TIMER_MODE)
		mod_timer(&hdmi->rxsense_timer, jiffies + msecs_to_jiffies(300));

	return 0;
}

static void rtk_hdmi_shutdown(struct platform_device *pdev)
{
	struct rtk_hdmi *hdmi = dev_get_drvdata(&pdev->dev);

	if (!hdmi)
		return;

	hdmi->in_suspend = true;

	disable_irq(hdmi->hpd_irq);

	if (hdmi->rxsense_mode == RXSENSE_INTERRUPT_MODE)
		disable_irq(hdmi->rxsense_irq);
	else if (hdmi->rxsense_mode == RXSENSE_TIMER_MODE)
		del_timer_sync(&hdmi->rxsense_timer);

	cancel_work_sync(&hdmi->hpd_work);

	rtk_hdmi_off(hdmi, OFF_FLAG_HDCP | OFF_FLAG_DIS_AUDIO);

	rtk_hdmi_set_5v(hdmi, HDMI_5V_DISABLE);
}

static int rtk_hdmi_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &rtk_hdmi_ops);
}

static int rtk_hdmi_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_hdmi_ops);
	return 0;
}

static const struct of_device_id rtk_hdmi_dt_ids[] = {
	{ .compatible = "realtek,rtk-hdmi",
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_hdmi_dt_ids);

static const struct dev_pm_ops rtk_hdmi_pm_ops = {
	.suspend    = rtk_hdmi_suspend,
	.resume     = rtk_hdmi_resume,
	.freeze     = rtk_hdmi_suspend,
	.thaw       = rtk_hdmi_resume,
	.restore    = rtk_hdmi_resume,
};

struct platform_driver rtk_hdmi_driver = {
	.probe  = rtk_hdmi_probe,
	.remove = rtk_hdmi_remove,
	.driver = {
		.name = "rtk-hdmi",
		.of_match_table = rtk_hdmi_dt_ids,
#if IS_ENABLED(CONFIG_PM)
		.pm = &rtk_hdmi_pm_ops,
#endif
	},
	.shutdown = rtk_hdmi_shutdown,
};
