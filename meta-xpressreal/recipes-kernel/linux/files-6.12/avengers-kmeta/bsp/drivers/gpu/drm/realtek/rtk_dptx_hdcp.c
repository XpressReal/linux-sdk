// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek HDCP driver
 *
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */

//#include "rtk_drm_drv.h"
#include "rtk_dptx.h"
#include "rtk_dptx_kent_reg.h"

#define TEEC_SUCCESS            0x0

enum HDCP14_CMD_FOR_TA {
	TA_TEE_HDP_ST 	= 0x1,
	TA_TEE_HDP_INIT	= 0x2,
};

static const uuid_t ta_do_detect_uuid = UUID_INIT(0x29b9dc95, 0xc9f8, 0x4eb5,
						0x96, 0xdd, 0xb5, 0x46, 0x23,
						0x28, 0x6d, 0x2d);


static int hdcp1_optee_match(struct tee_ioctl_version_data *data,
			    const void *vers)
{
	return 1;
}

static int rtk_dptx_hdcp1_tee_api_init(struct rtk_hdcp1_tee *hdcp1_tee)
{
	int ret;
	struct tee_param invoke_param[4];
	struct tee_ioctl_version_data vers = {
		.impl_id = TEE_OPTEE_CAP_TZ,
		.impl_caps = TEE_IMPL_ID_OPTEE,
		.gen_caps = TEE_GEN_CAP_GP,
	};

	hdcp1_tee->hdcp1_ctx = tee_client_open_context(NULL, hdcp1_optee_match,
						       NULL, &vers);
	if (IS_ERR(hdcp1_tee->hdcp1_ctx)) {
		pr_err("%s open context fail\n", __func__);
		return -1;
	}
	memcpy(hdcp1_tee->hdcp1_arg.uuid, ta_do_detect_uuid.b, TEE_IOCTL_UUID_LEN);

	hdcp1_tee->hdcp1_arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	hdcp1_tee->hdcp1_arg.num_params = 4;

	memset(&invoke_param, 0, sizeof(invoke_param));

	invoke_param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_open_session(hdcp1_tee->hdcp1_ctx, &hdcp1_tee->hdcp1_arg,
				      invoke_param);
	if ((ret < 0) || (hdcp1_tee->hdcp1_arg.ret != TEEC_SUCCESS)) {
		pr_err("%s tee open session fail, %d 0x%x\n", __func__, ret, hdcp1_tee->hdcp1_arg.ret);
		return -1;
	}

	return 0;
}

int rtk_dptx_hdcp_detect_status(struct rtk_hdcp1_tee *hdcp1_tee)
{
	struct tee_param  invoke_param[4];
	struct tee_ioctl_invoke_arg arg_in;
	int ret;

	if (!hdcp1_tee->init_hdcp1_ta_flag) {
		pr_err("%s - ta not init, return fail\n", __func__);
		return -1;
	}

	memset(&invoke_param, 0, sizeof(invoke_param));
	memset(&arg_in, 0, sizeof(arg_in));

	arg_in.func = TA_TEE_HDP_ST;
	arg_in.session = hdcp1_tee->hdcp1_arg.session;
	arg_in.num_params = 4;

	invoke_param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_invoke_func(hdcp1_tee->hdcp1_ctx, &arg_in, invoke_param);
	if ((ret < 0) || (arg_in.ret != TEEC_SUCCESS)) {
		pr_err("%s invoke fail, %d, 0x%x\n", __func__, ret, arg_in.ret);
		return -1;
	}
	return 0;
}

static int rtk_dptx_hdcp_detect_init(struct rtk_hdcp1_tee *hdcp1_tee)
{
	struct tee_param  invoke_param[4];
	struct tee_ioctl_invoke_arg arg_in;
	int ret;

	if (!hdcp1_tee->init_hdcp1_ta_flag) {
		pr_err("%s - ta not init, return fail\n", __func__);
		return -1;
	}

	memset(&invoke_param, 0, sizeof(invoke_param));
	memset(&arg_in, 0, sizeof(arg_in));

	arg_in.func = TA_TEE_HDP_INIT;
	arg_in.session = hdcp1_tee->hdcp1_arg.session;
	arg_in.num_params = 4;

	invoke_param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_invoke_func(hdcp1_tee->hdcp1_ctx, &arg_in, invoke_param);
	if ((ret < 0) || (arg_in.ret != TEEC_SUCCESS)) {
		pr_err("%s invoke fail, %d, 0x%x\n", __func__, ret, arg_in.ret);
		return -1;
	}
	return 0;
}

int rtk_dptx_hdcp_init(struct rtk_dptx *dptx, u32 hdcp_support)
{
	struct rtk_hdcp *hdcp = &dptx->hdcp;
	struct rtk_hdcp1_tee *hdcp1_tee = &hdcp->hdcp1_tee;
	int ret;

	ret = rtk_dptx_hdcp1_tee_api_init(hdcp1_tee);
	if(ret) {
		pr_err("%s tee api init fail\n", __func__);
		goto exit;
	}

	hdcp1_tee->init_hdcp1_ta_flag = 1;

	ret = rtk_dptx_hdcp_detect_init(hdcp1_tee);
	if(ret) {
		pr_err("%s detect ta init fail\n", __func__);
		goto exit;
	}
exit:
	return ret;
}
