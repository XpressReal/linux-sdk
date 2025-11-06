/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019 RealTek Inc.
 */

#ifndef _RTK_DRM_FB_H
#define _RTK_DRM_FB_H

#define RTK_DRM_FB_MIN_WIDTH  64
#define RTK_DRM_FB_MIN_HEIGHT 64

#define RTK_AFBC_MOD \
	DRM_FORMAT_MOD_ARM_AFBC( \
		AFBC_FORMAT_MOD_BLOCK_SIZE_16x16 | AFBC_FORMAT_MOD_YTR \
	)

#define RTK_AFRC_CU16_MOD \
	DRM_FORMAT_MOD_ARM_AFRC( \
		AFRC_FORMAT_MOD_CU_SIZE_16 | AFRC_FORMAT_MOD_LAYOUT_SCAN \
	)

#define RTK_AFRC_CU24_MOD \
	DRM_FORMAT_MOD_ARM_AFRC( \
		AFRC_FORMAT_MOD_CU_SIZE_24 | AFRC_FORMAT_MOD_LAYOUT_SCAN \
	)

#define RTK_AFRC_CU32_MOD \
	DRM_FORMAT_MOD_ARM_AFRC( \
		AFRC_FORMAT_MOD_CU_SIZE_32 | AFRC_FORMAT_MOD_LAYOUT_SCAN \
	)

struct drm_gem_object *rtk_fb_get_gem_obj(struct drm_framebuffer *fb,
					  unsigned int plane);
void rtk_drm_mode_config_init(struct drm_device *dev);

#endif  /* _RTK_DRM_FB_H_ */
