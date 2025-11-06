// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek npu_pp video m2m and capture v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-dma-contig.h>
#include "drv_npp.h"
#include "npp_common.h"
#include "npp_capture.h"
#include "npp_m2m.h"
#include "npp_debug.h"
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "npp_ctrl.h"
#include <linux/arm-smccc.h>
#include <linux/sys_soc.h>
#include <linux/version.h>
#include <soc/realtek/rtk-krpc-agent.h>

unsigned int npp_debug;
EXPORT_SYMBOL(npp_debug);
MODULE_PARM_DESC(debug, "activates debug info, where each bit enables a debug category.\n"
"\t\tBit 0 (0x01) will enable input messages (output type)\n"
"\t\tBit 1 (0x02) will enable output messages (capture type)\n");
module_param_named(debug, npp_debug, int, 0600);

#define VIDEO_DEVICE_CAPTURE  "npp-capture"
#define VIDEO_DEVICE_M2M      "npp-m2m"
#define NPP_NAME	          "realtek-npp"

char *type_str[] = {

	"UNKNOWN",              // 0
	"VIDEO_CAPTURE",        // 1
	"VIDEO_OUTPUT",         // 2
	"VIDEO_OVERLAY",        // 3
	"VBI_CAPTURE",          // 4
	"VBI_OUTPUT",           // 5
	"SLICED_VBI_CAPTURE",   // 6
	"SLICED_VBI_OUTPUT",    // 7
	"VIDEO_OUTPUT_OVERLAY", // 8
	"VIDEO_CAPTURE_MPLANE", // 9
	"VIDEO_OUTPUT_MPLANE",  // 10
	"SDR_CAPTURE",          // 11
	"SDR_OUTPUT",           // 12
	"META_CAPTURE",         // 13
	"META_OUTPUT"           // 14
};

char *mem_str[] = {
	"UNKNOWN",  // 0
	"MMAP",     // 1
	"USERPTR",  // 2
	"OVERLAY",  // 3
	"DMABUF"    // 4
};

static const struct soc_device_attribute rtk_soc_stark[] = {
	{ .family = "Realtek Stark", },
	{ /* empty */ }
};

static const struct soc_device_attribute rtk_soc_kent[] = {
	{ .family = "Realtek Kent", },
	{ /* empty */ }
};

static const struct soc_device_attribute rtk_soc_prince[] = {
	{ .family = "Realtek Prince", },
	{ /* empty */ }
};

static inline struct drv_npp_ctx *file2ctx(struct file *file)
{
	return container_of(file->private_data, struct drv_npp_ctx, fh);
}

/*
 * mem2mem callbacks
 */

/**
 * job_ready() - check whether an instance is ready to be scheduled to run
 */

static int job_ready(void *priv)
{
	pr_info("[drv_npp] %s\n", __func__);

	return 1;
}

static void job_abort(void *priv)
{
	struct drv_npp_ctx *ctx = priv;
	struct videc_dev *v_dev = ctx->dev;
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();

	if (!op_m2m)
		return;
	/* Will cancel the transaction in the next interrupt handler */
	op_m2m->npp_abort(priv, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	op_m2m->npp_abort(priv, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	v4l2_m2m_job_finish(v_dev->m2m_dev, ctx->fh.m2m_ctx);

	pr_info("[drv_npp] %s done\n", __func__);
}

/* device_run() - prepares and starts the device
 *
 * This simulates all the immediate preparations required before starting
 * a device. This will be called by the framework when it decides to schedule
 * a particular instance.
 */

static void device_run(void *priv)
{
	pr_info("[drv_npp] %s\n", __func__);
}

/*
 * video ioctls
 */

static int drv_npp_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct drv_npp_ctx *ctx = file2ctx(file);
	char *device_name;

	if (ctx->state == M2MSTATE) {
		device_name = VIDEO_DEVICE_M2M;
		cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	} else { //CAPTURESTATE
		device_name = VIDEO_DEVICE_CAPTURE;
		cap->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
	}

	strscpy(cap->driver, device_name, sizeof(cap->driver));
	strscpy(cap->card, device_name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s", device_name);
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int drv_npp_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret = 0;

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_m2m->npp_enum_fmt_cap(f);
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_capture->npp_enum_fmt_cap(f);
	}
	return ret;
}

static int drv_npp_enum_fmt_vid_out(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	struct videc_dev *v_dev = video_drvdata(file);

	pr_info("[drv_npp] %s\n", __func__);

	if (!op_m2m) {
		dev_err(v_dev->dev, "npp ops is NULL\n");
		return 0;
	}

	return op_m2m->npp_enum_fmt_out(f);
}

static int drv_npp_g_fmt(struct file *file, void *priv,
				struct v4l2_format *f)
{

	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret = 0;

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_m2m->npp_g_fmt(priv, f);
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_capture->npp_g_fmt(priv, f);
	}

	return ret;
}

static int drv_npp_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret = 0;

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_m2m->npp_try_fmt_cap(priv, f);
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_capture->npp_try_fmt_cap(priv, f);
	}

	return ret;
}

static int drv_npp_try_fmt_vid_out(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	struct videc_dev *v_dev = video_drvdata(file);

	pr_info("[drv_npp] %s\n", __func__);

	if (!op_m2m) {
		dev_err(v_dev->dev, "npp ops is NULL\n");
		return 0;
	}

	return op_m2m->npp_try_fmt_out(priv, f);
}

static int drv_npp_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret = 0;

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret =  op_m2m->npp_s_fmt_cap(priv, f);
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret =  op_capture->npp_s_fmt_cap(priv, f);
	}
	return ret;
}

static int drv_npp_s_fmt_vid_out(struct file *file, void *priv, struct v4l2_format *f)
{
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	struct videc_dev *v_dev = video_drvdata(file);

	pr_info("[drv_npp] %s\n", __func__);

	if (!op_m2m) {
		dev_err(v_dev->dev, "npp ops is NULL\n");
		return 0;
	}

	return op_m2m->npp_s_fmt_out(priv, f);
}


static int npp_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct drv_npp_ctx *ctx =
			container_of(ctrl->handler, struct drv_npp_ctx, ctrls);

	switch (ctrl->id) {
	case RTK_V4L2_NPP_SVP:
		{
			ctx->params.npp_svp = ctrl->val;
			dev_info(ctx->dev->dev, "svp = %d\n", ctx->params.npp_svp);
			break;
		}
	case RTK_V4L2_SINGLE_RGB_PLANE:
		{
			ctx->params.single_rgb_plane = ctrl->val;
			dev_info(ctx->dev->dev, "single_rgb_plane = %d\n",
			ctx->params.single_rgb_plane);
			break;
		}
	case RTK_V4L2_MIPI_CSI_PARMS_CONFIG:
		{
			struct rtk_mipi_csi_params *param = ctrl->p_new.p;

			memcpy(&ctx->params.mipi_csi_param, param,
			 sizeof(struct rtk_mipi_csi_params));
			dev_info(ctx->dev->dev, "mipi_csi_input = %d, in_queue_metadata = %d\n",
			 ctx->params.mipi_csi_param.mipi_csi_input,
			 ctx->params.mipi_csi_param.in_queue_metadata);
			dev_info(ctx->dev->dev, "data_pitch = %d, header_pitch = %d\n",
			 ctx->params.mipi_csi_param.data_pitch,
			 ctx->params.mipi_csi_param.header_pitch);
			break;
		}
	case RTK_V4L2_SIGNED_RGB_OUTPUT:
		{
			ctx->params.signed_rgb_output = ctrl->val;
			dev_info(ctx->dev->dev, "rgb_output = %d\n",
			 ctx->params.signed_rgb_output);
			break;
		}
	default:
		{
			dev_err(ctx->dev->dev, "Invalid control, id=%d, val=%d\n",
			 ctrl->id, ctrl->val);
			return -EINVAL;
		}
	}
	return 0;
}

#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
static bool npp_ctrl_type_equal(const struct v4l2_ctrl *ctrl,
	union v4l2_ctrl_ptr ptr1,
	union v4l2_ctrl_ptr ptr2)
{
return !memcmp(ptr1.p_const, ptr2.p_const, ctrl->elems * ctrl->elem_size);
}
#else
static bool npp_ctrl_type_equal(const struct v4l2_ctrl *ctrl, u32 idx,
		      union v4l2_ctrl_ptr ptr1,
		      union v4l2_ctrl_ptr ptr2)
{
	idx *= ctrl->elem_size;
	return !memcmp(ptr1.p_const + idx, ptr2.p_const + idx, ctrl->elem_size);
}
#endif

static void npp_ctrl_type_init(const struct v4l2_ctrl *ctrl, u32 idx,
		     union v4l2_ctrl_ptr ptr)
{
	void *p = ptr.p + idx * ctrl->elem_size;

	if (ctrl->p_def.p_const)
		memcpy(p, ctrl->p_def.p_const, ctrl->elem_size);
	else
		memset(p, 0, ctrl->elem_size);
}

#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
static int npp_ctrl_type_validate(const struct v4l2_ctrl *ctrl,
			union v4l2_ctrl_ptr ptr)
#else
static int npp_ctrl_type_validate(const struct v4l2_ctrl *ctrl, u32 idx,
			union v4l2_ctrl_ptr ptr)
#endif
{
	return 0;
}

static const struct v4l2_ctrl_ops npp_ctrl_ops = {
	.s_ctrl = npp_s_ctrl,
};

static const struct v4l2_ctrl_type_ops npp_type_ops = {
	.equal = npp_ctrl_type_equal,
	.init = npp_ctrl_type_init,
	.validate = npp_ctrl_type_validate,
};

static const struct v4l2_ctrl_config rtk_ctrl_npp_single_rgb_plane = {
	.ops = &npp_ctrl_ops,
	.id = RTK_V4L2_SINGLE_RGB_PLANE,
	.name = "NPP single rgb plane",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.def = 0,
	.min = 0,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config rtk_ctrl_npp_svp = {
	.ops = &npp_ctrl_ops,
	.id = RTK_V4L2_NPP_SVP,
	.name = "Enable Secure Buffer(SVP)",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.def = 0,
	.min = 0,
	.max = 1,
	.step = 1,
};

static const struct v4l2_ctrl_config rtk_ctrl_npp_mipi_csi_params = {
	.ops = &npp_ctrl_ops,
	.type_ops = &npp_type_ops,
	.id = RTK_V4L2_MIPI_CSI_PARMS_CONFIG,
	.name = "NPP MIPI CSI parameters",
	.type = V4L2_CTRL_TYPE_RTK_NPP_MIPI_CSI_PARAM,
	.def = 0,
	.min = 0,
	.max = UINT_MAX,
	.step = 1,
	.elem_size = sizeof(struct rtk_mipi_csi_params),
};

static const struct v4l2_ctrl_config rtk_ctrl_npp_signed_rgb_output = {
	.ops = &npp_ctrl_ops,
	.type_ops = &npp_type_ops,
	.id = RTK_V4L2_SIGNED_RGB_OUTPUT,
	.name = "NPP signed RGB output",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.def = 0,
	.min = 0,
	.max = 1,
	.step = 1,
};

int npp_ctrls_setup(struct drv_npp_ctx *ctx)
{

	v4l2_ctrl_handler_init(&ctx->ctrls, 4);

	v4l2_ctrl_new_custom(&ctx->ctrls, &rtk_ctrl_npp_single_rgb_plane, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &rtk_ctrl_npp_svp, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &rtk_ctrl_npp_mipi_csi_params, NULL);
	v4l2_ctrl_new_custom(&ctx->ctrls, &rtk_ctrl_npp_signed_rgb_output, NULL);

	if (ctx->ctrls.error) {
		dev_err(ctx->dev->dev, "control initialization error(%d)\n", ctx->ctrls.error);
		return -EINVAL;
	}

	return v4l2_ctrl_handler_setup(&ctx->ctrls);
}

static int drv_npp_reqbufs(struct file *file, void *priv, struct v4l2_requestbuffers *rb)
{

	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	int ret = 0;

	dev_info(v_dev->dev, " %s, type=%s memory=%s count=%d\n", __func__,
	 type_str[rb->type], mem_str[rb->memory], rb->count);

	if (ctx->state == M2MSTATE) {
		if (strcmp(mem_str[rb->memory], "USERPTR") == 0) {
			if (rb->count != 0) {
				ret = npp_object_dma_allcate_m2m(ctx, rb->type);
				if(ret != 0)
					return ret;
			} else {
				npp_object_dma_free_m2m(ctx, rb->type);
			}
		}
		ret =  v4l2_m2m_ioctl_reqbufs(file, priv, rb);
	} else { //CAPTURESTATE
		if ((rb->count != 0) && (ctx->bufferNum != 0)) {
			dev_err(v_dev->dev, "already request buffer\n");
			return -EINVAL;
		}
		ctx->bufferNum = rb->count;
		if (strcmp(mem_str[rb->memory], "USERPTR") == 0) {
			if (rb->count != 0) {
				ret = npp_object_dma_allcate_capture(ctx, rb->count);
				if(ret != 0)
					return ret;
			} else {
				npp_object_dma_free_capture(ctx);
			}
		}
		ret =  vb2_ioctl_reqbufs(file, priv, rb);
	}

	return ret;
}

static int drv_npp_querybuf(struct file *file, void *priv,
				struct v4l2_buffer *buf)
{
	struct drv_npp_ctx *ctx = file2ctx(file);
	int ret = 0;

	pr_info("[drv_npp] %s\n", __func__);

	if (ctx->state == M2MSTATE)
		ret =  v4l2_m2m_ioctl_querybuf(file, priv, buf);
	else //CAPTURESTATE
		ret =  vb2_ioctl_querybuf(file, priv, buf);

	return ret;
}


static int drv_npp_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct v4l2_fh *fh = file->private_data;
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	int ret = 0;

	dev_info(v_dev->dev, "%s, index=%d type=%s memory=%s\n",
	 __func__, buf->index, type_str[buf->type], mem_str[buf->memory]);

	if (ctx->state == M2MSTATE) {
		if (V4L2_TYPE_IS_OUTPUT(buf->type)) {
			if (buf->field != V4L2_FIELD_NONE) {
				dev_err(v_dev->dev, "OUTPUT queue isn't progreaaive\n");
				return -EINVAL;
			}
		}
		ret =  v4l2_m2m_qbuf(file, fh->m2m_ctx, buf);
	} else { //CAPTURESTATE
		ret =  vb2_ioctl_qbuf(file, priv, buf);
	}

	return ret;
}

static int drv_npp_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	int ret = 0;

	dev_info(v_dev->dev, "%s, type=%s memory=%s\n",
	 __func__, type_str[buf->type], mem_str[buf->memory]);

	if (ctx->state == M2MSTATE) {
		ret =  v4l2_m2m_ioctl_dqbuf(file, priv, buf);
	} else { //CAPTURESTATE
		ret =  vb2_ioctl_dqbuf(file, priv, buf);
		if (ret < 0) {
			if (npp_querydisplaywin(v_dev->rpc_opt) < 0) {
				dev_warn(v_dev->dev, "[drv_npp] %s npp_querydisplaywin return no vo\n", __func__);
				return  -EIO;
			}
			return ret;
		}
	}

	return ret;
}

static int drv_npp_create_bufs(struct file *file, void *priv, struct v4l2_create_buffers *create)
{
	struct drv_npp_ctx *ctx = file2ctx(file);
	int ret = 0;

	pr_info("[drv_npp] %s\n", __func__);

	if (ctx->state == M2MSTATE)
		ret =  v4l2_m2m_ioctl_create_bufs(file, priv, create);
	else //CAPTURESTATE
		ret =  vb2_ioctl_create_bufs(file, priv, create);

	return ret;
}

static int drv_npp_expbuf(struct file *file, void *priv, struct v4l2_exportbuffer *eb)
{
	struct drv_npp_ctx *ctx = file2ctx(file);
	int ret = 0;

	pr_info("[drv_npp] %s\n", __func__);

	if (ctx->state == M2MSTATE)
		ret = v4l2_m2m_ioctl_expbuf(file, priv, eb);
	else //CAPTURESTATE
		ret = vb2_ioctl_expbuf(file, priv, eb);

	return ret;
}

int drv_npp_prepare_buf(struct file *file, void *priv,
				   struct v4l2_buffer *buf)
{
	struct drv_npp_ctx *ctx = file2ctx(file);
	int ret = 0;

	pr_info("[drv_npp] %s\n", __func__);

	if (ctx->state == M2MSTATE)
		ret = v4l2_m2m_ioctl_prepare_buf(file, priv, buf);
	else //CAPTURESTATE
		ret = vb2_ioctl_prepare_buf(file, priv, buf);

	return ret;
}

static int drv_npp_stream_on(struct file *file, void *priv,
			enum v4l2_buf_type buf_type)
{
	struct videc_dev *v_dev = video_drvdata(file);
	int ret;

	pr_info("[drv_npp] %s\n", __func__);

	ret = npp_run_start(v_dev);
	if (ret < 0) {
		dev_err(v_dev->dev, "[drv_npp] %s npp_run_start failed\n", __func__);
		return  -EIO;
	}

	v_dev->npp_activate = true;

	return vb2_ioctl_streamon(file, priv, buf_type);
}

static int drv_npp_stream_off(struct file *file, void *priv,
			enum v4l2_buf_type buf_type)
{
	pr_info("[drv_npp] %s\n", __func__);

	return vb2_ioctl_streamoff(file, priv, buf_type);
}

static int drv_npp_g_selection(struct file *file, void *fh, struct v4l2_selection *sel)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	struct v4l2_rect rsel;
	int ret;

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}

		ret = op_m2m->npp_g_crop(fh, &rsel);

		switch (sel->target) {
		case V4L2_SEL_TGT_CROP_DEFAULT:
		case V4L2_SEL_TGT_CROP_BOUNDS:
		case V4L2_SEL_TGT_CROP:
			if (sel->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
				return -EINVAL;
			break;
		case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		case V4L2_SEL_TGT_COMPOSE_PADDED:
		case V4L2_SEL_TGT_COMPOSE:
		case V4L2_SEL_TGT_COMPOSE_DEFAULT:
			if (sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
				return -EINVAL;
			break;
		default:
			return -EINVAL;
		}

	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}

		if (v_dev->target_soc == STARK) {
			dev_err(v_dev->dev, "not support soc\n");
			return -EINVAL;
		}

		ret = op_capture->npp_g_crop(fh, &rsel);

		switch (sel->target) {
		case V4L2_SEL_TGT_CROP:
			break;
		default:
			dev_err(v_dev->dev, "invalid target\n");
			return -EINVAL;
		}
	}

	memcpy(&sel->r, &rsel, sizeof(struct v4l2_rect));

	return ret;
}

static int drv_npp_s_selection(struct file *file, void *fh, struct v4l2_selection *sel)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret;

	pr_info("[drv_npp] %s\n", __func__);

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_m2m->npp_s_crop(fh, sel);
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}

		if (v_dev->target_soc == STARK) {
			dev_err(v_dev->dev, "not support soc\n");
			return -EINVAL;
		}

		if (sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
			sel->target != V4L2_SEL_TGT_CROP) {
			dev_err(v_dev->dev, "invalid type or target\n");
			return -EINVAL;
		}

		ret = op_capture->npp_s_crop(fh, sel);
	}

	return ret;
}

static int drv_npp_frmsizeenum(struct file *file, void *priv,
				  struct v4l2_frmsizeenum *fsize)
{

	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret = 0;

	pr_info("[drv_npp] %s\n", __func__);

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_m2m->npp_enum_framesize(fsize);
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(v_dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_capture->npp_enum_framesize(fsize);
	}

	return ret;
}

static const struct v4l2_ioctl_ops drv_npp_ioctl_ops_capture = {
	.vidioc_querycap          = drv_npp_querycap,               // VIDIOC_QUERYCAP

	.vidioc_enum_fmt_vid_cap  = drv_npp_enum_fmt_vid_cap,       // VIDIOC_ENUM_FMT
	.vidioc_g_fmt_vid_cap_mplane     = drv_npp_g_fmt,                  // VIDIOC_G_FMT
	.vidioc_try_fmt_vid_cap_mplane   = drv_npp_try_fmt_vid_cap,        // VIDIOC_TRY_FMT
	.vidioc_s_fmt_vid_cap_mplane     = drv_npp_s_fmt_vid_cap,          // VIDIOC_S_FMT

	.vidioc_reqbufs           = drv_npp_reqbufs,                // VIDIOC_REQBUFS
	.vidioc_querybuf          = drv_npp_querybuf,               // VIDIOC_QUERYBUF
	.vidioc_qbuf              = drv_npp_qbuf,                   // VIDIOC_QBUF
	.vidioc_dqbuf             = drv_npp_dqbuf,                  // VIDIOC_DQBUF
	.vidioc_prepare_buf	      = drv_npp_prepare_buf,            // VIDIOC_PREPARE_BUF
	.vidioc_create_bufs	      = drv_npp_create_bufs,            // VIDIOC_CREATE_BUFS
	.vidioc_expbuf            = drv_npp_expbuf,                 // VIDIOC_EXPBUF
	.vidioc_g_selection       = drv_npp_g_selection,            // VIDIOC_G_SELECTION
	.vidioc_s_selection       = drv_npp_s_selection,            // VIDIOC_S_SELECTION
	.vidioc_streamon          = drv_npp_stream_on,              // VIDIOC_STREAMON
	.vidioc_streamoff         = drv_npp_stream_off,             // VIDIOC_STREAMOFF

	.vidioc_enum_framesizes   = drv_npp_frmsizeenum,            // VIDIOC_ENUM_FRAMESIZES
};

static const struct v4l2_ioctl_ops drv_npp_ioctl_ops_m2m = {
	.vidioc_querycap          = drv_npp_querycap,               // VIDIOC_QUERYCAP

	.vidioc_enum_fmt_vid_cap  = drv_npp_enum_fmt_vid_cap,       // VIDIOC_ENUM_FMT
	.vidioc_g_fmt_vid_cap_mplane     = drv_npp_g_fmt,                  // VIDIOC_G_FMT
	.vidioc_try_fmt_vid_cap_mplane   = drv_npp_try_fmt_vid_cap,        // VIDIOC_TRY_FMT
	.vidioc_s_fmt_vid_cap_mplane     = drv_npp_s_fmt_vid_cap,          // VIDIOC_S_FMT

	.vidioc_enum_fmt_vid_out  = drv_npp_enum_fmt_vid_out,       // VIDIOC_ENUM_FMT
	.vidioc_g_fmt_vid_out_mplane     = drv_npp_g_fmt,                  // VIDIOC_G_FMT
	.vidioc_try_fmt_vid_out_mplane   = drv_npp_try_fmt_vid_out,        // VIDIOC_TRY_FMT
	.vidioc_s_fmt_vid_out_mplane     = drv_npp_s_fmt_vid_out,          // VIDIOC_S_FMT

	.vidioc_reqbufs           = drv_npp_reqbufs,                // VIDIOC_REQBUFS
	.vidioc_querybuf          = drv_npp_querybuf,               // VIDIOC_QUERYBUF
	.vidioc_qbuf              = drv_npp_qbuf,                   // VIDIOC_QBUF
	.vidioc_dqbuf             = drv_npp_dqbuf,                  // VIDIOC_DQBUF
	.vidioc_prepare_buf	      = drv_npp_prepare_buf,            // VIDIOC_PREPARE_BUF
	.vidioc_create_bufs	      = drv_npp_create_bufs,            // VIDIOC_CREATE_BUFS
	.vidioc_expbuf            = drv_npp_expbuf,                 // VIDIOC_EXPBUF
	.vidioc_g_selection       = drv_npp_g_selection,            // VIDIOC_G_SELECTION
	.vidioc_s_selection       = drv_npp_s_selection,            // VIDIOC_S_SELECTION
	.vidioc_streamon          = v4l2_m2m_ioctl_streamon,        // VIDIOC_STREAMON
	.vidioc_streamoff         = v4l2_m2m_ioctl_streamoff,       // VIDIOC_STREAMOFF

	.vidioc_enum_framesizes   = drv_npp_frmsizeenum,            // VIDIOC_ENUM_FRAMESIZES
};

/*
 * Queue operations
 */

static int drv_npp_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers, unsigned int *nplanes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct drv_npp_ctx *ctx = vb2_get_drv_priv(vq);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret = 0;

	dev_info(ctx->dev->dev, "%s, memory=%s\n", __func__, mem_str[vq->memory]);

	vq->mem_ops = (vq->memory == V4L2_MEMORY_USERPTR)
	 ? &vb2_vmalloc_memops : &vb2_dma_contig_memops;

	if (nplanes)
		*nplanes = 1;

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		alloc_devs[0] = ctx->dev->v4l2_dev.dev;
		ret =  op_m2m->npp_queue_info(vq, nbuffers, &sizes[0]);
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret =  op_capture->npp_queue_info(vq, nbuffers, &sizes[0]);
	}

	return ret;
}

static int drv_npp_buf_prepare(struct vb2_buffer *vb)
{

	struct drv_npp_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	int sizeimages, ret;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();

	dev_info(ctx->dev->dev, "%s, index=%d type=%s memory=%s\n", __func__,
	vb->index, type_str[vb->type], mem_str[vb->memory]);

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		if (vbuf->field == V4L2_FIELD_ANY)
			vbuf->field = V4L2_FIELD_NONE;
		if (vbuf->field != V4L2_FIELD_NONE) {
			dev_err(ctx->dev->dev, "field isn't supported\n");
			return -EINVAL;
		}
	}

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_m2m->npp_queue_info(vb->vb2_queue, NULL, &sizeimages);
		if (ret < 0)
			return ret;
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		ret = op_capture->npp_queue_info(vb->vb2_queue, NULL, &sizeimages);
		if (ret < 0)
			return ret;
	}

	if (vb2_plane_size(vb, 0) < sizeimages) {
		dev_err(ctx->dev->dev, "data will not fit into plane (%lu < %lu)\n",
		 vb2_plane_size(vb, 0), (long)sizeimages);
		return -EINVAL;
	}

	return 0;
}

static void drv_npp_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct drv_npp_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int  ret;

	dev_info(ctx->dev->dev, "%s, index=%d type=%s memory=%s\n", __func__,
	vb->index, type_str[vb->type], mem_str[vb->memory]);

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return;
		}
		v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
		op_m2m->npp_qbuf(&ctx->fh, vb);
	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return;
		}

		ret = op_capture->npp_qbuf(ctx->dev, &ctx->fh, vb,
			ctx->params.single_rgb_plane, ctx->bufferNum);
		if (ret < 0)
			dev_err(ctx->dev->dev, "npp_qbuf error\n");
	}
}

static int drv_npp_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct drv_npp_ctx *ctx = vb2_get_drv_priv(q);
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret;

	dev_info(ctx->dev->dev, "%s, %s\n", __func__, V4L2_TYPE_TO_STR(q->type));

	if (ctx->state == M2MSTATE) {
		if (!op_m2m) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}

		ret = op_m2m->npp_start_streaming(q, count);
		if (ret)
			dev_err(ctx->dev->dev, "npp_start_streaming failed, ret(%d)\n", ret);

	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return -EINVAL;
		}
		op_capture->npp_start_streaming(q, count);
	}
	return 0;
}

static void drv_npp_stop_streaming(struct vb2_queue *q)
{
	struct drv_npp_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vbuf;
	unsigned long flags;
	const struct npp_fmt_ops_m2m *op_m2m = get_npp_fmt_ops_m2m();
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret;

	dev_info(ctx->dev->dev, "%s, %s\n", __func__, V4L2_TYPE_TO_STR(q->type));

	if (ctx->state == M2MSTATE) {
		for (;;) {
			if (V4L2_TYPE_IS_OUTPUT(q->type))
				vbuf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
			else
				vbuf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
			if (vbuf == NULL)
				break;
			spin_lock_irqsave(&ctx->dev->irqlock, flags);
			v4l2_m2m_buf_done(vbuf, VB2_BUF_STATE_ERROR);
			spin_unlock_irqrestore(&ctx->dev->irqlock, flags);
		}
		if (!op_m2m) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return;
		}
		ret = op_m2m->npp_stop_streaming(q);
		if (ret)
			dev_err(ctx->dev->dev, "npp_stop_streaming failed, ret(%d)\n", ret);

	} else { //CAPTURESTATE
		if (!op_capture) {
			dev_err(ctx->dev->dev, "npp ops is NULL\n");
			return;
		}
		ret = op_capture->npp_stop_streaming(q);
		if (ret < 0)
			dev_err(ctx->dev->dev, "npp_stop_streaming failed\n");
	}
}

static const struct vb2_ops drv_npp_qops = {
	.queue_setup	 = drv_npp_queue_setup,
	.buf_prepare	 = drv_npp_buf_prepare,
	.buf_queue		 = drv_npp_buf_queue,
	.start_streaming = drv_npp_start_streaming,
	.stop_streaming  = drv_npp_stop_streaming,
	.wait_prepare	 = vb2_ops_wait_prepare,
	.wait_finish	 = vb2_ops_wait_finish,
};

/*
 * File operations
 */

static int queue_init(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq)
{
	struct drv_npp_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->ops = &drv_npp_qops;
	src_vq->mem_ops = &vb2_vmalloc_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->dev->dev_mutex_m2m;
	src_vq->dev = ctx->dev->dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &drv_npp_qops;
	dst_vq->mem_ops = &vb2_vmalloc_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->dev->dev_mutex_m2m;
	dst_vq->dev = ctx->dev->dev;

	return vb2_queue_init(dst_vq);
}

static int drv_npp_open_m2m(struct file *file)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = NULL;
	int ret = 0;
	unsigned long flags;
#ifndef ENABLE_NPP_FPGA_TEST
	struct arm_smccc_res res;
#endif

	if (mutex_lock_interruptible(&v_dev->dev_mutex_m2m))
		return -ERESTARTSYS;

	spin_lock_irqsave(&v_dev->device_spin_lock, flags);
	if (v_dev->state == AVAILABLE)
		v_dev->state = M2MSTATE;
	else if (v_dev->state == CAPTURESTATE) {
		dev_warn(v_dev->dev, "Unable to open device ,capture device is using\n");
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
		ret = -EBUSY;
		goto unlock_mutex;
	} else {
		dev_info(v_dev->dev, "Already M2MSTATE\n");
	}
	spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		dev_err(v_dev->dev, "kzalloc drv_npp_ctx failed\n");
		goto set_available;
	}

	ctx->state = M2MSTATE;

	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->dev = v_dev;
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(v_dev->m2m_dev, ctx, &queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		v4l2_fh_del(&ctx->fh);
		v4l2_fh_exit(&ctx->fh);
		kfree(ctx);
		dev_err(v_dev->dev, "v4l2_m2m_ctx_init failed\n");
		goto set_available;
	}

	ctx->file = file;

	ret = npp_ctrls_setup(ctx);
	if (ret < 0) {
		v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
		ctx->fh.m2m_ctx = NULL;
		v4l2_fh_del(&ctx->fh);
		v4l2_fh_exit(&ctx->fh);
		kfree(ctx);
		dev_err(v_dev->dev, "npp_ctrls_setup failed\n");
		goto set_available;
	}

	ctx->fh.ctrl_handler = &ctx->ctrls;

	memset(&ctx->params, 0, sizeof(struct vid_params));
	ctx->params.single_rgb_plane = 0;
	ctx->params.mipi_csi_param.mipi_csi_input = 0;
	ctx->params.mipi_csi_param.in_queue_metadata = 0;
	ctx->params.npp_svp = 0;
	ctx->params.signed_rgb_output = 0;

	ctx->npp_ctx = npp_alloc_context_m2m(&ctx->fh, v_dev);
	if (!ctx->npp_ctx) {
		ret = -ENOMEM;
		v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
		ctx->fh.m2m_ctx = NULL;
		v4l2_ctrl_handler_free(&ctx->ctrls);
		v4l2_fh_del(&ctx->fh);
		v4l2_fh_exit(&ctx->fh);
		kfree(ctx);
		dev_err(v_dev->dev, "npp_alloc_context failed\n");
		goto set_available;
	}

	if (v_dev->dev_open_cnt_m2m == 0) {

#ifndef ENABLE_NPP_FPGA_TEST
		if (v_dev->target_soc == STARK)
			arm_smccc_smc(RTK_NN_CONTROL, NPU_PP_OP_RELEASE_RESET, 0, 0, 0, 0, 0, 0, &res);
		else
			arm_smccc_smc(SIP_NN_OP, NPU_PP_OP_RELEASE_RESET, 0, 0, 0, 0, 0, 0, &res);
#endif
		NPP_NPP_HwClock(1, v_dev->target_soc);

		if (NPP_Init(v_dev->target_soc) != 0) {
			ret = -EFAULT;
			npp_free_context_m2m(v_dev, ctx->npp_ctx);
			v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
			ctx->fh.m2m_ctx = NULL;
			v4l2_ctrl_handler_free(&ctx->ctrls);
			v4l2_fh_del(&ctx->fh);
			v4l2_fh_exit(&ctx->fh);
			kfree(ctx);
			dev_err(v_dev->dev, "NPP_Init failed\n");
			goto set_available;
		}
	}

	v_dev->dev_open_cnt_m2m += 1;
	dev_info(v_dev->dev, "Created instance: %p, m2m_ctx: %p\n", ctx, ctx->fh.m2m_ctx);
	mutex_unlock(&v_dev->dev_mutex_m2m);
	return ret;

set_available:
	if (v_dev->dev_open_cnt_m2m == 0) {
		spin_lock_irqsave(&v_dev->device_spin_lock, flags);
		v_dev->state = AVAILABLE;
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
	}
unlock_mutex:
	mutex_unlock(&v_dev->dev_mutex_m2m);
	return ret;
}

static int drv_npp_release_m2m(struct file *file)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	unsigned long flags;

#ifdef ENABLE_NPP_MEASURE_TIME
	npp_time_measure(ctx->npp_ctx);
#endif
	mutex_lock(&v_dev->dev_mutex_m2m);
	v_dev->dev_open_cnt_m2m -= 1;
	if(ctx->fh.m2m_ctx)
		v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	if(ctx->npp_ctx)
		npp_free_context_m2m(v_dev, ctx->npp_ctx);

	v4l2_ctrl_handler_free(&ctx->ctrls);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	if (v_dev->dev_open_cnt_m2m == 0) {
		spin_lock_irqsave(&v_dev->device_spin_lock, flags);
		v_dev->state = AVAILABLE;
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
	}
	mutex_unlock(&v_dev->dev_mutex_m2m);

	pr_info("[drv_npp] %s done\n", __func__);
	return 0;
}


static int drv_npp_open_capture(struct file *file)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = NULL;
	struct vb2_queue *vbq;
	struct v4l2_device *v4l2_dev = &v_dev->v4l2_dev;
	struct video_device *video_dev = &v_dev->video_dev_capture;
	unsigned long flags;
	int ret = 0;

	mutex_lock(&v_dev->dev_mutex_capture);

	spin_lock_irqsave(&v_dev->device_spin_lock, flags);
	if (v_dev->state == AVAILABLE)
		v_dev->state = CAPTURESTATE;
	else if (v_dev->state == M2MSTATE) {
		dev_warn(v_dev->dev, "Unable to open device ,m2m device is using\n");
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
		ret = -EBUSY;
		goto unlock_mutex;
	} else {
		dev_info(v_dev->dev, "Already CAPTURESTATE\n");
	}
	spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);

	if (v_dev->dev_open_cnt_capture >= 1) {
		dev_warn(v_dev->dev, "%s: instance = %d ,capture device is using\n", __func__,
			   v_dev->dev_open_cnt_capture);
		ret = -EBUSY;
		goto unlock_mutex;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto set_available;
	}

	ctx->state = CAPTURESTATE;

	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->dev = v_dev;
	ctx->file = file;

	vbq = &ctx->queue;

	vbq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	vbq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	vbq->dev = v4l2_dev->dev;
	vbq->lock = &v_dev->dev_mutex_capture;
	vbq->ops = &drv_npp_qops;
	vbq->mem_ops = &vb2_vmalloc_memops;
	vbq->drv_priv = ctx;
	vbq->buf_struct_size = sizeof(struct npp_buffer);
	vbq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	ret = vb2_queue_init(vbq);
	if (ret < 0) {
		dev_err(v_dev->dev, "Failed to init vb2 queue\n");
		goto free_v4l2_fh_ctx;
	}
	video_dev->queue = vbq;

	ret = npp_ctrls_setup(ctx);
	if (ret < 0) {
		dev_err(v_dev->dev, "npp_ctrls_setup failed\n");
		goto free_vbq;
	}

	ctx->fh.ctrl_handler = &ctx->ctrls;

	memset(&ctx->params, 0, sizeof(struct vid_params));
	ctx->params.single_rgb_plane = 0;
	ctx->params.npp_svp = 0;

	ctx->npp_ctx = npp_alloc_context_capture(&ctx->fh, v_dev);
	if (!ctx->npp_ctx) {
		dev_err(v_dev->dev, "npp_alloc_context failed\n");
		ret = -ENOMEM;
		goto free_v4l2_ctrl;
	}

	if (npp_rpc_buffer_allocate(v_dev) != 0) {
		npp_free_context_capture(ctx->npp_ctx);
		ret = -ENOMEM;
		dev_err(v_dev->dev, "npp_rpc_buffer_allocate failed\n");

		goto free_v4l2_ctrl;
	}
	if (npp_ringbuffer_buffer_allocate(v_dev) != 0) {
		npp_rpc_buffer_release(v_dev);
		npp_free_context_capture(ctx->npp_ctx);
		ret = -ENOMEM;
		dev_err(v_dev->dev, "npp_ringbuffer_buffer_allocate failed\n");

		goto free_v4l2_ctrl;
	}
	if (npp_initialize(v_dev) != 0) {
		npp_ringbuffer_buffer_release(v_dev);
		npp_rpc_buffer_release(v_dev);
		npp_free_context_capture(ctx->npp_ctx);
		ret = -EIO;
		dev_err(v_dev->dev, "npp_initialize failed\n");

		goto free_v4l2_ctrl;
	}
	v_dev->dev_open_cnt_capture += 1;
	dev_info(v_dev->dev, "Open capture device success\n");
	mutex_unlock(&v_dev->dev_mutex_capture);
	return ret;

free_v4l2_ctrl:
		v4l2_ctrl_handler_free(&ctx->ctrls);
free_vbq:
		vb2_queue_release(vbq);
free_v4l2_fh_ctx:
		v4l2_fh_del(&ctx->fh);
		v4l2_fh_exit(&ctx->fh);
		kfree(ctx);
set_available:
	if (v_dev->dev_open_cnt_capture == 0) {
		spin_lock_irqsave(&v_dev->device_spin_lock, flags);
		v_dev->state = AVAILABLE;
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
	}
unlock_mutex:
	mutex_unlock(&v_dev->dev_mutex_capture);
	return ret;
}

static int drv_npp_release_capture(struct file *file)
{
	struct videc_dev *v_dev = video_drvdata(file);
	struct drv_npp_ctx *ctx = file2ctx(file);
	const struct npp_fmt_ops_capture *op_capture = get_npp_fmt_ops_capture();
	int ret;
	unsigned long flags;

	if (!op_capture) {
		dev_err(v_dev->dev, "npp ops is NULL\n");
		return -EINVAL;
	}

	pr_info("[drv_npp] %s npp_activate =%d\n", __func__, v_dev->npp_activate);
	if (v_dev->npp_activate == true) {
		ret = op_capture->npp_stop_streaming(&ctx->queue);
		v_dev->npp_activate = false;
		if (ret < 0) {
			dev_err(ctx->dev->dev, "close device, npp_stop_streaming failed\n");
			return  -EIO;
		}
	}

	ret = npp_destroy(v_dev);
	if (ret < 0) {
		dev_err(v_dev->dev, "npp_destroy failed\n");
		return -EIO;
	}

	npp_ringbuffer_buffer_release(v_dev);
	npp_rpc_buffer_release(v_dev);
	npp_free_context_capture(ctx->npp_ctx);

	v4l2_ctrl_handler_free(&ctx->ctrls);
	vb2_queue_release(&ctx->queue);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	mutex_lock(&v_dev->dev_mutex_capture);
	v_dev->dev_open_cnt_capture -= 1;
	if (v_dev->dev_open_cnt_capture == 0) {
		spin_lock_irqsave(&v_dev->device_spin_lock, flags);
		v_dev->state = AVAILABLE;
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
	}
	mutex_unlock(&v_dev->dev_mutex_capture);

	pr_info("[drv_npp] %s done\n", __func__);

	return 0;
}



static const struct v4l2_file_operations drv_npp_fops_m2m = {
	.owner		= THIS_MODULE,
	.open		= drv_npp_open_m2m,
	.release	= drv_npp_release_m2m,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

static struct video_device drv_npp_videodev_m2m = {
	.name		= VIDEO_DEVICE_M2M,
	.vfl_dir	= VFL_DIR_M2M,
	.fops		= &drv_npp_fops_m2m,
	.ioctl_ops	= &drv_npp_ioctl_ops_m2m,
	.minor		= -1,
	.release	= video_device_release_empty,
};

static struct v4l2_m2m_ops m2m_ops = {
	.device_run	= device_run,
	.job_ready	= job_ready,
	.job_abort	= job_abort,
};

static const struct v4l2_file_operations drv_npp_fops_capture = {
	.owner		= THIS_MODULE,
	.open		= drv_npp_open_capture,
	.release	= drv_npp_release_capture,
	.poll		= vb2_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= vb2_fop_mmap,
};

static struct video_device drv_npp_videodev_capture = {
	.name		= VIDEO_DEVICE_CAPTURE,
	.vfl_dir	= VFL_DIR_RX,
	.vfl_type   = VFL_TYPE_VIDEO,
	.fops		= &drv_npp_fops_capture,
	.ioctl_ops	= &drv_npp_ioctl_ops_capture,
	.minor		= -1,
	.release	= video_device_release_empty,
};

static int drv_npp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct videc_dev *v_dev;
	struct video_device *video_dev_m2m;
	struct video_device *video_dev_capture;
	int ret;
	bool unreg_m2m = false;
	bool unreg_capture = false;

	/* Allocate a new instance */
	v_dev = devm_kzalloc(dev, sizeof(*v_dev), GFP_KERNEL);
	if (!v_dev)
		return -ENOMEM;
	v_dev->dev = dev;
	platform_set_drvdata(pdev, v_dev);

	spin_lock_init(&v_dev->irqlock);

	/* Initialize the top-level structure */
	ret = v4l2_device_register(dev, &v_dev->v4l2_dev);
	if (ret < 0) {
		dev_err(dev, "v4l2_device_register failed, ret(%d)\n", ret);
		return ret;
	}

	mutex_init(&v_dev->dev_mutex_m2m);
	mutex_init(&v_dev->dev_mutex_capture);
	mutex_init(&v_dev->hw_mutex);
	sema_init(&v_dev->hw_sem, 1);
	v_dev->nppctx_activate = NULL;
	v_dev->dev_open_cnt_capture = 0;
	v_dev->dev_open_cnt_m2m = 0;

	if (soc_device_match(rtk_soc_stark)) {
		v_dev->target_soc = STARK;
		v_dev->rpc_opt = RPC_AUDIO;
	} else if (soc_device_match(rtk_soc_kent)) {
		v_dev->target_soc = KENT;
		v_dev->rpc_opt = RPC_HIFI;
#ifndef ENABLE_NPP_FPGA_TEST
	} else if (soc_device_match(rtk_soc_prince)) {
		v_dev->target_soc = PRINCE;
		v_dev->rpc_opt = RPC_HIFI;
	} else {
		dev_err(dev, "not support soc\n");
		unreg_m2m = true;
		unreg_capture = true;
		ret = -EIO;
		goto unreg_dev;
	}
#else
	} else {
		v_dev->target_soc = PRINCE;
		v_dev->rpc_opt = RPC_HIFI;
	}
#endif

#if defined(ENABLE_NPP_ISR)
	v_dev->irq = platform_get_irq(pdev, 0);
	if (v_dev->irq < 0) {
		dev_err(dev, "failed to get irq: %d\n", v_dev->irq);
		ret = v_dev->irq;
		unreg_m2m = true;
		goto capture_reg;
	}
	dev_info(dev, "dev->irq = %d\n", v_dev->irq);
#endif
	/* Initialize per-driver m2m data */
	v_dev->m2m_dev = v4l2_m2m_init(&m2m_ops);

	if (v_dev->m2m_dev == NULL ||IS_ERR(v_dev->m2m_dev)) {
		ret = PTR_ERR(v_dev->m2m_dev);
		dev_err(dev, "v4l2_m2m_init failed, ret(%d)\n", ret);
		unreg_m2m = true;
		goto capture_reg;
	}

	/* Initialize the m2m video_device structure */
	v_dev->video_dev_m2m = drv_npp_videodev_m2m;
	video_dev_m2m = &v_dev->video_dev_m2m;
	video_dev_m2m->lock = &v_dev->dev_mutex_m2m;
	video_dev_m2m->v4l2_dev = &v_dev->v4l2_dev;
	video_dev_m2m->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;

	/* Set private data */
	video_set_drvdata(video_dev_m2m, v_dev);
	snprintf(video_dev_m2m->name, sizeof(video_dev_m2m->name), "%s", drv_npp_videodev_m2m.name);

	/* Register video4linux device */
	ret = video_register_device(video_dev_m2m, VFL_TYPE_VIDEO, -1);
	if (ret) {
		dev_err(dev, "video_register_m2m_device failed, ret(%d)\n", ret);
		unreg_m2m = true;
		goto capture_reg;
	}
	dev_info(dev, "m2m device registered as /dev/video%d\n", video_dev_m2m->num);

capture_reg:
	if (npp_rpc_buffer_allocate(v_dev) != 0) {
		dev_err(v_dev->dev, "npp_rpc_buffer_allocate failed\n");
		unreg_capture = true;
		ret = -ENOMEM;
		goto unreg_dev;
	} else {
		if (npp_rpc_init_check(v_dev) != 0) {
			dev_warn(dev, "npp_rpc_init_check, rpc not ready\n");
			npp_rpc_buffer_release(v_dev);
			unreg_capture = true;
			ret = -EIO;
			goto unreg_dev;
		}
		npp_rpc_buffer_release(v_dev);
	}

	/* Initialize the capture video_device structure */
	v_dev->video_dev_capture = drv_npp_videodev_capture;
	video_dev_capture = &v_dev->video_dev_capture;
	video_dev_capture->lock = &v_dev->dev_mutex_capture;
	video_dev_capture->v4l2_dev = &v_dev->v4l2_dev;
	video_dev_capture->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;

	/* Set private data */
	video_set_drvdata(video_dev_capture, v_dev);
	snprintf(video_dev_capture->name, sizeof(video_dev_capture->name), "%s"
		, drv_npp_videodev_capture.name);

	/* Register video4linux device */
	ret = video_register_device(video_dev_capture, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		dev_err(dev, "video_register_capture_device failed, ret(%d)\n", ret);
		unreg_capture = true;
		goto unreg_dev;
	}

	dev_info(dev, "capture device registered as /dev/video%d\n", video_dev_capture->num);

	return ret;

unreg_dev:
	if (unreg_m2m == true && unreg_capture == true)
		v4l2_device_unregister(&v_dev->v4l2_dev);
	else
		ret = 0;

	return ret;
}

static int drv_npp_remove(struct platform_device *pdev)
{
	struct videc_dev *v_dev = platform_get_drvdata(pdev);

	if (v_dev->m2m_dev)
		v4l2_m2m_release(v_dev->m2m_dev);
	video_unregister_device(&v_dev->video_dev_m2m);
	video_unregister_device(&v_dev->video_dev_capture);
	v4l2_device_unregister(&v_dev->v4l2_dev);

	return 0;
}

static int drv_npp_suspend(struct device *dev)
{
	struct videc_dev *v_dev = dev_get_drvdata(dev);
	unsigned long flags_state;
	long timeout = msecs_to_jiffies(20);
#ifndef ENABLE_NPP_FPGA_TEST
	struct arm_smccc_res res;
#endif

	dev_info(dev, "[rtk_npp] suspend\n");

	spin_lock_irqsave(&v_dev->device_spin_lock, flags_state);
	if (v_dev->state == M2MSTATE) {
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags_state);

		if (down_timeout(&v_dev->hw_sem, timeout)) {
			dev_err(dev, "suspend failed, npp not finished\n");
			return -ETIMEDOUT; // timeout
		}

		NPP_UnInit(v_dev->target_soc);


		if (v_dev->target_soc == STARK) {
			NPP_NPP_HwClock(0, v_dev->target_soc);
#ifndef ENABLE_NPP_FPGA_TEST
			arm_smccc_smc(RTK_NN_CONTROL, NPU_PP_OP_HOLD_RESET, 0, 0, 0, 0, 0, 0, &res);
#endif
		} else if (v_dev->target_soc == KENT) {
			NPP_NPP_HwClock(0, v_dev->target_soc);
#ifndef ENABLE_NPP_FPGA_TEST
			arm_smccc_smc(SIP_NN_OP, NPU_PP_OP_HOLD_RESET, 0, 0, 0, 0, 0, 0, &res);
#endif
		} else {
			dev_info(dev, "not need to do hw reset and close clock\n");
		}

	} else if (v_dev->state == CAPTURESTATE) {
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags_state);
		if (v_dev->npp_activate == true) {
			dev_err(dev, "suspend failed, npp not finished\n");
			return -EBUSY;
		}

		if (npp_destroy(v_dev) < 0) {
			dev_err(dev, "suspend failed, npp_destroy failed\n");
			return -EIO;
		}
	} else {
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags_state);
	}

	return 0;
}

static int drv_npp_resume(struct device *dev)
{
	int ret = 0;
	struct videc_dev *v_dev = dev_get_drvdata(dev);
	unsigned long flags;
#ifndef ENABLE_NPP_FPGA_TEST
	struct arm_smccc_res res;
#endif
	dev_info(dev, "[rtk_npp] resume\n");

	spin_lock_irqsave(&v_dev->device_spin_lock, flags);
	if (v_dev->state == M2MSTATE) {
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
#ifndef ENABLE_NPP_FPGA_TEST
		if (v_dev->target_soc == STARK)
			arm_smccc_smc(RTK_NN_CONTROL, NPU_PP_OP_RELEASE_RESET, 0, 0, 0, 0, 0, 0, &res);
		else
			arm_smccc_smc(SIP_NN_OP, NPU_PP_OP_RELEASE_RESET, 0, 0, 0, 0, 0, 0, &res);
#endif
		NPP_NPP_HwClock(1, v_dev->target_soc);
		if (NPP_Init(v_dev->target_soc) != 0) {
			dev_err(dev, "resume failed, NPP_Init failed\n");
			ret = -EFAULT;
		}

		up(&v_dev->hw_sem);
	} else if (v_dev->state == CAPTURESTATE) {
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
		if (npp_initialize(v_dev) < 0) {
			dev_err(v_dev->dev, "[drv_npp] %s npp_initialize failed\n", __func__);
			ret = -EIO;
		}
	} else {
		spin_unlock_irqrestore(&v_dev->device_spin_lock, flags);
	}

	return ret;
}

static const struct of_device_id rtk_npp_ids[] = {
	{.compatible = "realtek,npu-pp"},
	{}
};
MODULE_DEVICE_TABLE(of, rtk_npp_ids);

static const struct dev_pm_ops drv_npp_pm = {
	.suspend = drv_npp_suspend,
	.resume = drv_npp_resume,
};

static struct platform_driver npp_pdrv = {
	.probe		= drv_npp_probe,
	.remove		= drv_npp_remove,
	.driver		= {
		.name	= NPP_NAME,
		.owner  = THIS_MODULE,
		.pm     = &drv_npp_pm,
		.of_match_table = of_match_ptr(rtk_npp_ids),
	},
};
module_platform_driver(npp_pdrv);


MODULE_VERSION(xstr(GIT_VERSION));
MODULE_AUTHOR("Chunju Lin <chj_lin@realtek.com>");
MODULE_DESCRIPTION("Realtek NPU_PP Driver");
MODULE_LICENSE("GPL");
