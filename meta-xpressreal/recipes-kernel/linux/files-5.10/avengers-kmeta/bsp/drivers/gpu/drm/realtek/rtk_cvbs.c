
#include <drm/drm_of.h>
#include <drm/drm_print.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>

#include <linux/platform_device.h>
#include <linux/component.h>

#include "rtk_drm_drv.h"

#define to_rtk_cvbs(x) container_of(x, struct rtk_cvbs, x)

#define RTK_CVBS_OFF  0
#define RTK_CVBS_NTSC 1
#define RTK_CVBS_PAL  2
#define RTK_CVBS_AUTO 3

struct rtk_cvbs {
	struct device *dev;
	struct drm_device *drm_dev;
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct rtk_rpc_info *rpc_info;
};


struct drm_display_mode rtk_cvbs_modes[] = {
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

static enum drm_connector_status
rtk_cvbs_conn_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
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

static void rtk_cvbs_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	struct rtk_cvbs *cvbs = to_rtk_cvbs(encoder);
	int ret;
	unsigned int cur_cvbs_fmt, cvbs_fmt;
	u8 vic;

	vic = drm_match_cea_mode(mode);
	if (vic != 6 && vic != 21) {
		dev_err(cvbs->dev, "unsupport vic=%u", vic);
		return;
	}

	if (vic == 6)
		cvbs_fmt = RTK_CVBS_NTSC;
	else
		cvbs_fmt = RTK_CVBS_PAL;

	ret = rpc_get_cvbs_format(cvbs->rpc_info, &cur_cvbs_fmt);
	if (ret) {
		dev_err(cvbs->dev, "get current cvbs format failed");
		return;
	}

	dev_info(cvbs->dev, "Set format %s",
			(cvbs_fmt == RTK_CVBS_NTSC) ? "NTSC" : "PAL");

	if (cvbs_fmt == cur_cvbs_fmt) {
		dev_info(cvbs->dev, "Same as current format, skip mode set");
		return;
	}

	ret = rpc_set_cvbs_format(cvbs->rpc_info, cvbs_fmt);
	if (ret)
		dev_err(cvbs->dev, "set format failed, ret=%d", ret);

}

static void rtk_cvbs_enc_enable(struct drm_encoder *encoder)
{
	DRM_DEBUG_KMS("%s\n", __func__);
}

static void rtk_cvbs_enc_disable(struct drm_encoder *encoder)
{
	DRM_DEBUG_KMS("%s\n", __func__);
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
	.atomic_check = rtk_cvbs_enc_atomic_check,
};

static int rtk_cvbs_bind(struct device *dev, struct device *master,
			void *data)
{
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_cvbs *cvbs;
	int ret;

	cvbs = devm_kzalloc(dev, sizeof(*cvbs), GFP_KERNEL);
	if (!cvbs)
		return -ENOMEM;

	cvbs->drm_dev = drm;
	cvbs->dev = dev;

	encoder = &cvbs->encoder;
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	if (encoder->possible_crtcs == 0) {
		ret = -EPROBE_DEFER;
		goto err_exit;
	}

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

	cvbs->rpc_info = &priv->rpc_info;

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
	},
};
