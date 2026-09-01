// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek TEE API driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tee_drv.h>
#include <linux/uaccess.h>
#include "tee_TEEapi_api.h"

static const uuid_t tee_api_uuid = UUID_INIT(0x7aaaf200, 0x2450, 0x11e4,
								0xab, 0xe2, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b);

#define TEE_PARAM_ARRY_SIZE         4

enum TEEAPI_CMD_FOR_TA
{
	TA_TEE_MEMCPY                   = 0,
	TA_TEE_SET_PROTECT_MEM          = 1,
	TA_TEE_SET_VE_PROTECT_REGION    = 2,
	TA_TEE_VE_WRITE_DATA            = 3,
	TA_TEE_GET_AUDIO_DETECT         = 4,
	/*
	TA_TEE_GET_WAVE_INFO            = 4,
	*/
	TA_TEE_GET_AAC_INFO             = 5,
	TA_TEE_SET_VE_BLOCK_NWC         = 6,
	TA_TEE_PARSE_HDR                = 7,
	TA_TEE_GET_HDR_FLAG             = 8,
	TA_TEE_INITIALIZE_HDR_PARSER    = 9,
	TA_TEE_CC_INIT                  = 10,
	TA_TEE_CC_PROCESS               = 11,
	TA_TEE_MEMCPY_A7                = 12,
	TA_TEE_MEM_STATUS               = 13,
	TA_TEE_VE3_API                  = 14,
	TA_TEE_DBG_BS_PRINT             = 15,
	TA_TEE_DBG_BS_OUT               = 16,
	TA_TEE_ENDIAN_SWAP              = 17,
	TA_TEE_SET_HDR_SWAP             = 18,
	TA_TEE_DVBSP_RESET              = 19,
	TA_TEE_DVBSP_INIT               = 20,
	TA_TEE_DVBSP_DEC_SEG            = 21,
	TA_TEE_DVBSP_SEND_PAGE          = 22,
	TA_TEE_HDR_COMMON_API_PLUS1     = 23,
	TA_TEE_API_KEY_CRYPTO           = 24,
	TA_TEE_DVBSP_SEND_PAGE_VO       = 25,
	TA_TEE_OMX_CC_API               = 32,
};

static int TEEapi_optee_match(struct tee_ioctl_version_data *data,
	const void *vers)
{
	return 1;
}

int ta_TEEapi_init(struct tee_context **teeapi_ctx, unsigned int *teeapi_tee_session)
{
	int ret = 0;
	int err_code = 0;
	struct tee_ioctl_open_session_arg arg = {0};
	struct tee_param  param[TEE_PARAM_ARRY_SIZE] = {0};
	struct tee_ioctl_version_data vers = {
		.impl_id = TEE_OPTEE_CAP_TZ,
		.impl_caps = TEE_IMPL_ID_OPTEE,
		.gen_caps = TEE_GEN_CAP_GP,
	};

	pr_err("[+] [%d]%s.\n",__LINE__,__func__);

	if ((teeapi_ctx == NULL) || (teeapi_tee_session == NULL))
	{
		pr_err("[%d]%s.input parameter is NULL\n",__LINE__,__func__);
		err_code = -EINVAL;
		goto exit;
	}


	*teeapi_ctx = tee_client_open_context(NULL, TEEapi_optee_match, NULL, &vers);

	if (IS_ERR(*teeapi_ctx))
	{
		pr_err("[%d]%s.tee_client_open_context() fail\n",__LINE__,__func__);
		err_code = -EPERM;
		goto exit;
	}

	export_uuid(arg.uuid, &tee_api_uuid);

	arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	arg.num_params = TEE_PARAM_ARRY_SIZE;

	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_open_session(*teeapi_ctx, &arg, param);
	if ((ret < 0) || (arg.ret != 0))
	{
		pr_err("[%d]%s.tee_client_open_session() fail\n",__LINE__,__func__);
		tee_client_close_context(*teeapi_ctx);
		err_code = -EPERM;
		goto exit;
	}

	*teeapi_tee_session = arg.session;
	pr_err("[-] [%d]%s.\n",__LINE__,__func__);
exit:
	return err_code;
}
EXPORT_SYMBOL(ta_TEEapi_init);

int ta_TEEapi_deinit(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session)
{
	pr_err("[+] [%d]%s.\n",__LINE__,__func__);

	if (teeapi_ctx == NULL)
	{
		pr_err("[%d]%s.input parameter is NULL\n",__LINE__,__func__);
		return -1;
	}

	tee_client_close_session(teeapi_ctx, teeapi_tee_session);
	tee_client_close_context(teeapi_ctx);

    teeapi_ctx = NULL;
    teeapi_tee_session = 0;

	pr_err("[-] [%d]%s.\n",__LINE__,__func__);
	return 0;
}
EXPORT_SYMBOL(ta_TEEapi_deinit);

int ta_TEEapi_memcpy_a7(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int dstPAddr, unsigned char *buf, int size)
{
	int ret;
	int err_code = 0;
	struct tee_param invoke_param[TEE_PARAM_ARRY_SIZE] = {0};
	struct tee_ioctl_invoke_arg arg = {0};
	struct tee_shm *shm;

	if (teeapi_ctx == NULL)
	{
		pr_err("[%d]%s.input parameter is NULL\n",__LINE__,__func__);
		err_code = -EINVAL;
		goto exit;
	}

	arg.func = TA_TEE_MEMCPY_A7;
	arg.session = teeapi_tee_session;
	arg.num_params = TEE_PARAM_ARRY_SIZE;

	shm = tee_shm_alloc(teeapi_ctx, size, TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
	if (IS_ERR(shm))
	{
		pr_err("[%d]%s.tee_shm_alloc() size %d fail\n",__LINE__,__func__, size);
		err_code = -ENOMEM;
		goto exit;
	}

	//pr_info("[%d]%s.dstPAddr:0x%08x.size:%d.buf:0x%px.shm:0x%px\n",__LINE__,__func__,dstPAddr,size,buf,shm);
	invoke_param[0].u.value.a = dstPAddr;
	invoke_param[1].u.memref.size = size;
	invoke_param[1].u.memref.shm = shm;

	memcpy(invoke_param[1].u.memref.shm->kaddr, buf, size);

	invoke_param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	invoke_param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	invoke_param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_invoke_func(teeapi_ctx, &arg, invoke_param);
	if ((ret < 0) || (arg.ret != 0))
	{
		pr_err("[%d]%s.tee_client_invoke_func() fail.ret:0x%x\n",__LINE__,__func__,arg.ret);
		err_code = -EPERM;
		goto free_shm;
	}

free_shm:
	tee_shm_free(shm);
exit:
	return err_code;
}
EXPORT_SYMBOL(ta_TEEapi_memcpy_a7);

int ta_TEEapi_bitstreamprint(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int phy_addr, int size)
{
	int ret = 0;
	int err_code = 0;
	struct tee_param invoke_param[TEE_PARAM_ARRY_SIZE] = {0};
	struct tee_ioctl_invoke_arg arg = {0};

	if (teeapi_ctx == NULL)
	{
		pr_err("[%d]%s.input parameter is NULL\n",__LINE__,__func__);
		err_code = -EINVAL;
		goto exit;
	}

	arg.func = TA_TEE_DBG_BS_PRINT;
	arg.session = teeapi_tee_session;
	arg.num_params = TEE_PARAM_ARRY_SIZE;

	//pr_err("[%d]%s.phy_addr:0x%08x.size:%d\n",__LINE__,__func__,phy_addr,size);
	invoke_param[0].u.value.a = phy_addr;
	invoke_param[0].u.value.b = size;

	invoke_param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	invoke_param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_invoke_func(teeapi_ctx, &arg, invoke_param);
	if ((ret < 0) || (arg.ret != 0))
	{
		pr_err("[%d]%s.tee_client_invoke_func() fail.ret:0x%x\n",__LINE__,__func__,arg.ret);
		err_code = -EPERM;
		goto exit;
	}
exit:
	return err_code;
}
EXPORT_SYMBOL(ta_TEEapi_bitstreamprint);

int ta_TEEapi_bitstreamout(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int srcPAddr, unsigned char *buf, int size)
{
	int ret = 0;
	int err_code = 0;
	struct tee_param invoke_param[TEE_PARAM_ARRY_SIZE] = {0};
	struct tee_ioctl_invoke_arg arg = {0};
	struct tee_shm *shm;

	if (teeapi_ctx == NULL)
	{
		pr_err("[%d]%s.input parameter is NULL\n",__LINE__,__func__);
		err_code = -EINVAL;
		goto exit;
	}

	arg.func = TA_TEE_DBG_BS_OUT;
	arg.session = teeapi_tee_session;
	arg.num_params = TEE_PARAM_ARRY_SIZE;

	shm = tee_shm_alloc(teeapi_ctx, size, TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
	if (IS_ERR(shm))
	{
		pr_err("[%d]%s.tee_shm_alloc() fail\n",__LINE__,__func__);
		err_code = -ENOMEM;
		goto exit;
	}

	//pr_err("[%d]%s.srcPAddr:0x%08x.size:%d.buf:0x%px.shm:0x%px\n",__LINE__,__func__,srcPAddr,size,buf,shm);
	invoke_param[0].u.value.a = srcPAddr;
	invoke_param[1].u.memref.size = size;
	invoke_param[1].u.memref.shm = shm;

	invoke_param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	invoke_param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	invoke_param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_invoke_func(teeapi_ctx, &arg, invoke_param);
	if ((ret < 0) || (arg.ret != 0))
	{
		pr_err("[%d]%s.tee_client_invoke_func() fail.ret:0x%x\n",__LINE__,__func__,arg.ret);
		err_code = -EPERM;
		goto free_shm;
	}

	memcpy(buf, invoke_param[1].u.memref.shm->kaddr, size);

free_shm:
	tee_shm_free(shm);
exit:
	return err_code;
}
EXPORT_SYMBOL(ta_TEEapi_bitstreamout);

int ta_TEEapi_memcpy(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int dstPhysAddr, unsigned int srtPhysAddr, int size)
{
	int ret = 0;
	int err_code = 0;
	struct tee_param invoke_param[TEE_PARAM_ARRY_SIZE] = {0};
	struct tee_ioctl_invoke_arg arg = {0};

	if (teeapi_ctx == NULL)
	{
		pr_err("[%d]%s.input parameter is NULL\n",__LINE__,__func__);
		err_code = -EINVAL;
		goto exit;
	}

	arg.func = TA_TEE_MEMCPY;
	arg.session = teeapi_tee_session;
	arg.num_params = TEE_PARAM_ARRY_SIZE;

	//pr_info("[%d]%s.dstPhysAddr:0x%08x.srtPhysAddr:0x%08x.size:%d\n",__LINE__,__func__,dstPhysAddr,srtPhysAddr,size);
	invoke_param[0].u.value.a = dstPhysAddr;
	invoke_param[0].u.value.b = size;
	invoke_param[1].u.value.a = srtPhysAddr;

	invoke_param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	invoke_param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	invoke_param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
	invoke_param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_invoke_func(teeapi_ctx, &arg, invoke_param);
	if ((ret < 0) || (arg.ret != 0))
	{
		pr_err("[%d]%s.tee_client_invoke_func() fail.ret:0x%x\n",__LINE__,__func__,arg.ret);
		err_code = -EPERM;
		goto exit;
	}

exit:
	return err_code;
}
EXPORT_SYMBOL(ta_TEEapi_memcpy);

int ta_TEEapi_OMX_CC_API(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int src_addr, unsigned char *dst_addr, unsigned int size, unsigned int codec_type, unsigned int mode)
{
	int ret = 0;
	int err_code = 0;
	struct tee_param invoke_param[TEE_PARAM_ARRY_SIZE] = {0};
	struct tee_ioctl_invoke_arg arg = {0};
	struct tee_shm *shm;

	if (teeapi_ctx == NULL)
	{
		pr_err("[%d]%s.input parameter is NULL\n",__LINE__,__func__);
		err_code = -EINVAL;
		goto exit;
	}

	arg.func = TA_TEE_OMX_CC_API;
	arg.session = teeapi_tee_session;
	arg.num_params = TEE_PARAM_ARRY_SIZE;

	shm = tee_shm_alloc(teeapi_ctx, size, TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
	if (IS_ERR(shm)) {
		shm = NULL;
		err_code = -ENOMEM;
		goto exit;
	}

	invoke_param[0].u.value.a        = src_addr;
	invoke_param[0].u.value.b        = size;
	invoke_param[1].u.value.a        = codec_type;
	invoke_param[1].u.value.b        = mode;
	invoke_param[2].u.memref.shm     = shm;
	invoke_param[2].u.memref.size    = size;

	invoke_param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	invoke_param[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
	invoke_param[2].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT; //TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT
	invoke_param[3].attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;

	ret = tee_client_invoke_func(teeapi_ctx, &arg, invoke_param);

	if ((ret < 0) || (arg.ret != 0))
	{
		pr_err("[%d]%s.tee_client_invoke_func() fail.ret:0x%x\n",__LINE__,__func__,arg.ret);
		err_code = -EPERM;
		goto free_param;
	}

	memcpy(dst_addr, invoke_param[2].u.memref.shm->kaddr, size);

free_param:
	if (shm)
		tee_shm_free(shm);
exit:
	return err_code;
}
EXPORT_SYMBOL(ta_TEEapi_OMX_CC_API);

MODULE_LICENSE("GPL v2");
