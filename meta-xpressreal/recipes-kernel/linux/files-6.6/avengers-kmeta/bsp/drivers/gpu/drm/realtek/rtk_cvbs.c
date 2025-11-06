// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 RealTek Inc.
 */

#include <drm/display/drm_hdmi_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_print.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>

#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/extcon-provider.h>
#include <linux/of_address.h>

#ifdef CONFIG_CHROME_PLATFORMS
#include <linux/of_reserved_mem.h>
#endif

#include "rtk_drm_drv.h"

#define to_rtk_cvbs(x) container_of(x, struct rtk_cvbs, x)

#define RTK_CVBS_OFF  0 /* DAC OFF */
#define RTK_CVBS_NTSC 1
#define RTK_CVBS_PAL  2
#define RTK_CVBS_AUTO 3
#define RTK_CVBS_OFF_DAC_ON  4

#define OHM_DET_REG 0x4
#define CTRL_DET_REG 0x8
#define INT_EN_REG 0xc
#define INT_ST_REG 0x10
#define CVBS_DET_OFF_INT_EN BIT(2)
#define CVBS_DET_ON_INT_EN BIT(1)

#define RTK_ZOOM95_ON   95
#define RTK_ZOOM95_OFF  100

/* DTS Extension Flags, for of_property ext-flags */
#define RTK_CVBS_EXT_HDMI_HPD        BIT(0)
#define RTK_CVBS_EXT_ZOOM95          BIT(1)

struct rtk_cvbs {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct rtk_rpc_info *rpc_info;
	void __iomem *ctrl_base;
	struct delayed_work work;
	struct extcon_dev *edev;
	int plugin;
	u32 previous_fmt;
	bool is_cvbs_on;
	u32 ext_flags;
	struct extcon_dev *hdmi_edev;
};

static const unsigned int rtk_cvbs_cable[] = {
	EXTCON_JACK_VIDEO_OUT,
	EXTCON_NONE,
};

struct drm_display_mode rtk_cvbs_modes[] = {
	/* Quirk off mode - 640x480@60Hz 4:3 */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25175, 640, 656,
		   752, 800, 0, 480, 490, 492, 525, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
	/* NTSC */
	{ DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 13500, 720, 739,
		   801, 858, 0, 480, 488, 494, 525, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		   DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
	/* PAL */
	{ DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 13500, 720, 732,
		   795, 864, 0, 576, 580, 586, 625, 0,
		   DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
		   DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLCLK),
	  .picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3, },
};

static const char * const str_cvbs_mode[] = {
	"OFF", "NTSC", "PAL", "AUTO",
	"OFF_DAC_ON", "UNKNOWN", "UNKNOWN", "UNKNOWN"
};

static enum drm_connector_status
rtk_cvbs_conn_detect(struct drm_connector *connector, bool force)
{
	struct rtk_cvbs *cvbs = to_rtk_cvbs(connector);
	int status;

	if (extcon_get_state(cvbs->edev, EXTCON_JACK_VIDEO_OUT))
		status =  connector_status_connected;
	else
		status = connector_status_disconnected;

	return status;
}

static void rtk_cvbs_conn_destroy(struct drm_connector *connector)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	drm_connector_cleanup(connector);
}

static int rtk_cvbs_conn_get_modes(struct drm_connector *connector)
{
	int i;

	DRM_DEBUG_KMS("%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(rtk_cvbs_modes); ++i) {
		struct drm_display_mode *mode;

		mode = drm_mode_duplicate(connector->dev, &rtk_cvbs_modes[i]);
		drm_mode_probed_add(connector, mode);
	}

	return i;
}

static enum drm_mode_status
rtk_cvbs_conn_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	u8 vic;

	DRM_DEBUG_KMS("%s\n", __func__);

	vic = drm_match_cea_mode(mode);
	if (vic == 6 || vic == 21)
		return MODE_OK;

	return MODE_BAD;
}

static const struct drm_connector_funcs rtk_cvbs_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = rtk_cvbs_conn_detect,
	.destroy = rtk_cvbs_conn_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static struct drm_connector_helper_funcs rtk_cvbs_connector_helper_funcs = {
	.get_modes = rtk_cvbs_conn_get_modes,
	.mode_valid = rtk_cvbs_conn_mode_valid,
};

static int rtk_cvbs_plug_detect(struct rtk_cvbs *cvbs)
{
	int cvbs_connected;

	if (readl(cvbs->ctrl_base + OHM_DET_REG) & BIT(0)) {
		dev_dbg(cvbs->dev, "cvbs plug out");
		cvbs_connected = 0;
	} else {
		dev_dbg(cvbs->dev, "cvbs plug in");
		cvbs_connected = 1;
	}

	return cvbs_connected;
}

static void rtk_cvbs_setup(struct rtk_cvbs *cvbs,
			     unsigned int cvbs_fmt)
{
	unsigned int cur_cvbs_fmt;
	unsigned int new_cvbs_fmt;
	int ret;

	if ((cvbs_fmt != RTK_CVBS_OFF) && (cvbs_fmt != RTK_CVBS_NTSC) &&
		(cvbs_fmt != RTK_CVBS_PAL) && (cvbs_fmt != RTK_CVBS_AUTO)) {
		dev_err(cvbs->dev, "%s failed, invalid fmt=%u", __func__, cvbs_fmt);
		return;
	}

	new_cvbs_fmt = cvbs_fmt;

	if ((cvbs->ext_flags & RTK_CVBS_EXT_HDMI_HPD) &&
		!IS_ERR(cvbs->hdmi_edev)) {
		int hdmi_connected;

		hdmi_connected = extcon_get_state(cvbs->hdmi_edev, EXTCON_DISP_HDMI);
		if (hdmi_connected) {
			new_cvbs_fmt = RTK_CVBS_OFF;
			dev_info(cvbs->dev, "Force CVBS OFF when HDMI connected");
		}
	}

	ret = rpc_get_cvbs_format(cvbs->rpc_info, &cur_cvbs_fmt);
	if (ret) {
		dev_err(cvbs->dev, "get current cvbs format failed");
		return;
	}

	if (new_cvbs_fmt == cur_cvbs_fmt) {
		dev_info(cvbs->dev, "Same as current format %s, skip setup",
			str_cvbs_mode[cur_cvbs_fmt&0x7]);
		return;
	}

	if (rtk_cvbs_plug_detect(cvbs) && (new_cvbs_fmt == RTK_CVBS_OFF)) {
		dev_info(cvbs->dev, "Keep cvbs dac_on when cvbs connected");
		new_cvbs_fmt = RTK_CVBS_OFF_DAC_ON;
	}

	dev_info(cvbs->dev, "Set format %s", str_cvbs_mode[new_cvbs_fmt&0x7]);

	ret = rpc_set_cvbs_format(cvbs->rpc_info, new_cvbs_fmt);
	if (ret) {
		dev_err(cvbs->dev, "set format failed, ret=%d", ret);
		return;
	}

	if ((new_cvbs_fmt == RTK_CVBS_OFF) ||
		(new_cvbs_fmt == RTK_CVBS_OFF_DAC_ON))
		cvbs->is_cvbs_on = false;
	else
		cvbs->is_cvbs_on = true;
}

static void rtk_cvbs_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	struct rtk_cvbs *cvbs = to_rtk_cvbs(encoder);
	unsigned int cvbs_fmt;
	u8 vic;

	vic = drm_match_cea_mode(mode);

	if (vic == 6) {
		cvbs_fmt = RTK_CVBS_NTSC;
	} else if (vic == 21) {
		cvbs_fmt = RTK_CVBS_PAL;
	} else {
		dev_info(cvbs->dev, "vic=%u, skip mode set", vic);
		return;
	}

	rtk_cvbs_setup(cvbs, cvbs_fmt);

	cvbs->previous_fmt = cvbs_fmt;
}

static void rtk_cvbs_enc_enable(struct drm_encoder *encoder)
{
	struct rtk_cvbs *cvbs = to_rtk_cvbs(encoder);

	dev_info(cvbs->dev, "Enable encoder");

	if (!cvbs->is_cvbs_on)
		rtk_cvbs_setup(cvbs, cvbs->previous_fmt);
}

static void rtk_cvbs_enc_disable(struct drm_encoder *encoder)
{
	struct rtk_cvbs *cvbs = to_rtk_cvbs(encoder);

	dev_info(cvbs->dev, "Disable encoder");

	rtk_cvbs_setup(cvbs, RTK_CVBS_OFF);
}

static void rtk_cvbs_enc_atomic_disable(struct drm_encoder *encoder,
				struct drm_atomic_state *state)
{
	struct rtk_cvbs *cvbs = to_rtk_cvbs(encoder);

	dev_info(cvbs->dev, "Atomic disable encoder");

	if (state->crtcs == NULL)
		goto dis_cvbs;

	if ((state->crtcs->old_state == NULL) || (state->crtcs->new_state == NULL))
		goto dis_cvbs;

	dev_info(cvbs->dev, "crtcs active %u -> %u",
		state->crtcs->old_state->active, state->crtcs->new_state->active);

	if (state->crtcs->new_state->active)
		goto skip_dis;

dis_cvbs:
	rtk_cvbs_setup(cvbs, RTK_CVBS_OFF);

skip_dis:
	return;
}

static int rtk_cvbs_enc_atomic_check(struct drm_encoder *encoder,
				struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	DRM_DEBUG_KMS("%s\n", __func__);
	return 0;
}

static const struct drm_encoder_funcs rtk_cvbs_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct drm_encoder_helper_funcs rtk_cvbs_encoder_helper_funcs = {
	.mode_set   = rtk_cvbs_enc_mode_set,
	.enable     = rtk_cvbs_enc_enable,
	.disable    = rtk_cvbs_enc_disable,
	.atomic_disable = rtk_cvbs_enc_atomic_disable,
	.atomic_check = rtk_cvbs_enc_atomic_check,
};

static void rtk_cvbs_detection_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rtk_cvbs *cvbs = container_of(dwork, struct rtk_cvbs, work);
	int pre_state, current_state;

	pre_state = extcon_get_state(cvbs->edev, EXTCON_JACK_VIDEO_OUT);

	current_state = rtk_cvbs_plug_detect(cvbs);

	if (current_state != pre_state) {
		extcon_set_state_sync(cvbs->edev, EXTCON_JACK_VIDEO_OUT, current_state);

		if ((cvbs->ext_flags & RTK_CVBS_EXT_ZOOM95) &&
			(current_state == 1)) {
			int ret;

			ret = rpc_video_set_display_ratio(cvbs->rpc_info, RTK_ZOOM95_ON);
			dev_info(cvbs->dev, "ZOOM95_ON, ext_flags=0x%08x, ret=%d\n",
				cvbs->ext_flags, ret);
		}
	}

	schedule_delayed_work(&cvbs->work, HZ);
}

static void rtk_cvbs_detection_init(struct rtk_cvbs *cvbs)
{

	writel(0x200003, cvbs->ctrl_base + CTRL_DET_REG);
	schedule_delayed_work(&cvbs->work, msecs_to_jiffies(5));
}

static int rtk_cvbs_bind(struct device *dev, struct device *master,
			void *data)
{
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_cvbs *cvbs;
	struct device_node *hdmi_np;
	int ret;

	cvbs = devm_kzalloc(dev, sizeof(*cvbs), GFP_KERNEL);
	if (!cvbs)
		return -ENOMEM;

	cvbs->drm_dev = drm;
	cvbs->dev = dev;

	cvbs->ctrl_base = of_iomap(dev->of_node, 0);
	if (!cvbs->ctrl_base) {
		dev_err(dev, "failed to get ctrl address\n");
		ret = -EINVAL;
		goto err_exit;
	}

#ifdef CONFIG_CHROME_PLATFORMS
	ret = of_reserved_mem_device_init(dev);
	if (ret)
		dev_warn(dev, "init reserved memory failed");
#endif

	encoder = &cvbs->encoder;
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	dev_info(dev, "cvbs possible_crtcs (0x%x)\n", encoder->possible_crtcs);

	if (encoder->possible_crtcs == 0) {
		ret = -EPROBE_DEFER;
		goto err_exit;
	}

	ret = of_property_read_u32(dev->of_node, "ext-flags",
			&cvbs->ext_flags);
	if (ret < 0)
		cvbs->ext_flags = 0;

	if (cvbs->ext_flags)
		dev_info(dev, "ext_flags=0x%08x", cvbs->ext_flags);

	if (!(cvbs->ext_flags & RTK_CVBS_EXT_HDMI_HPD))
		goto skip_hdmi_extcon;

	hdmi_np = of_find_compatible_node(NULL, NULL, "realtek,rtk-hdmi");
	if (!hdmi_np)
		goto skip_hdmi_extcon;

	if (of_device_is_available(hdmi_np)) {
		cvbs->hdmi_edev = extcon_get_extcon_dev("hdmi");

		if (IS_ERR(cvbs->hdmi_edev))
			dev_err(dev, "Failed to get hdmi extcon");
	}
skip_hdmi_extcon:

	drm_encoder_init(drm, encoder, &rtk_cvbs_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS, NULL);

	drm_encoder_helper_add(encoder, &rtk_cvbs_encoder_helper_funcs);

	connector = &cvbs->connector;
	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->interlace_allowed = true;
	ret = drm_connector_init(drm, connector, &rtk_cvbs_connector_funcs,
			   DRM_MODE_CONNECTOR_Composite);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err_exit;
	}
	drm_connector_helper_add(connector, &rtk_cvbs_connector_helper_funcs);

	drm_connector_attach_encoder(connector, encoder);

	cvbs->rpc_info = &priv->rpc_info[RTK_RPC_MAIN];
	cvbs->previous_fmt = RTK_CVBS_AUTO;

	cvbs->edev = devm_extcon_dev_allocate(dev, rtk_cvbs_cable);
	if (IS_ERR(cvbs->edev)) {
		dev_err(dev, "failed to allocate extcon device");
		ret =  PTR_ERR(cvbs->edev);
		goto err_exit;
	}

	ret = devm_extcon_dev_register(dev, cvbs->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device");
		goto err_exit;
	}

	INIT_DELAYED_WORK(&cvbs->work, rtk_cvbs_detection_work);

	rtk_cvbs_detection_init(cvbs);

	//rpc_set_cvbs_auto_detection(cvbs->rpc_info, 0);

	dev_set_drvdata(dev, cvbs);

	return 0;

err_exit:
	return ret;
}

static void rtk_cvbs_unbind(struct device *dev, struct device *master,
			     void *data)
{

}

static const struct component_ops rtk_cvbs_ops = {
	.bind	= rtk_cvbs_bind,
	.unbind	= rtk_cvbs_unbind,
};

static int rtk_cvbs_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &rtk_cvbs_ops);
}

static int rtk_cvbs_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rtk_cvbs_ops);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rtk_cvbs_suspend(struct device *dev)
{
	struct rtk_cvbs *cvbs = dev_get_drvdata(dev);

	if (!cvbs)
		return 0;

	cancel_delayed_work_sync(&cvbs->work);

	return 0;
}

static int rtk_cvbs_resume(struct device *dev)
{
	struct rtk_cvbs *cvbs = dev_get_drvdata(dev);

	if (!cvbs)
		return 0;

	rtk_cvbs_detection_init(cvbs);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rtk_cvbs_pm_ops, rtk_cvbs_suspend,
			 rtk_cvbs_resume);
#else
static const struct dev_pm_ops rtk_drm_pm_ops = {};
#endif


static const struct of_device_id rtk_cvbs_dt_ids[] = {
	{ .compatible = "realtek,rtk-cvbs",
	},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_cvbs_dt_ids);

struct platform_driver rtk_cvbs_driver = {
	.probe  = rtk_cvbs_probe,
	.remove = rtk_cvbs_remove,
	.driver = {
		.name = "rtk-cvbs",
		.of_match_table = rtk_cvbs_dt_ids,
		.pm     = &rtk_cvbs_pm_ops,
	},
};
