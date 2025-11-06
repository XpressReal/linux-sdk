/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __RTK_DRM_VOWB_PRIV_H__
#define __RTK_DRM_VOWB_PRIV_H__

#include "rtk_drm_drv.h"

struct rtk_rpc_info;
struct rpmsg_device;

static inline struct rpmsg_device *get_rpdev(struct rtk_rpc_info *rpc_info)
{
	return rpc_info->krpc_ept_info->rpdev;
}

static inline struct rtk_rpc_info *get_rpc_info(struct rtk_drm_private *priv)
{
	return &priv->rpc_info[RTK_RPC_MAIN];
}

#endif
