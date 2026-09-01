// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Realtek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>

#include "rtk_drm_fb.h"
#include "rtk_drm_gem.h"
#include "rtk_drm_splash.h"
#include "drm_splash_png.h"

#include "stb_image.h"

enum pixel_fmt {
    PIXEL_RGBA,
    PIXEL_XRGB
};

static char *drm_splash = "";

static bool splash_is_colorbar(void)
{
    return strcmp(drm_splash, "colorbar") == 0;
}

static inline u32 rtk_mk_color(u8 r, u8 g, u8 b)
{
    return ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}


static inline u8 lerp_u8(u8 a, u8 b, u32 t16)
{
    /* t16: 0..65535 */
    return (u8)(((u32)a * (65535 - t16) + (u32)b * t16 + 32768) >> 16);
}

void resize_bilinear(u8 *dst, int dw, int dh,
                     const u8 *src, int sw, int sh,
                     enum pixel_fmt fmt)
{
    int y;
    DRM_INFO("rtk_drm: resize_bilinear\n");
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
        return;

    DRM_INFO("rtk_drm: resize_bilinear\n");
    for (y = 0; y < dh; y++) {
        u32 fy = (dh == 1) ? 0 : (u32)(((u64)y * (sh - 1) << 16) / (dh - 1));
        int y0 = (int)(fy >> 16);
        int y1 = (y0 + 1 < sh) ? (y0 + 1) : y0;
        u32 ty = fy & 0xFFFF;

        const u8 *row0 = src + y0 * sw * 4;
        const u8 *row1 = src + y1 * sw * 4;
        u8 *drow       = dst + y * dw * 4;

        int x;
        for (x = 0; x < dw; x++) {
            u32 fx = (dw == 1) ? 0 : (u32)(((u64)x * (sw - 1) << 16) / (dw - 1));
            int x0 = (int)(fx >> 16);
            int x1 = (x0 + 1 < sw) ? (x0 + 1) : x0;
            u32 tx = fx & 0xFFFF;

            const u8 *p00 = row0 + x0 * 4;
            const u8 *p10 = row0 + x1 * 4;
            const u8 *p01 = row1 + x0 * 4;
            const u8 *p11 = row1 + x1 * 4;

            u8 r = lerp_u8(lerp_u8(p00[0], p10[0], tx),
                           lerp_u8(p01[0], p11[0], tx), ty);
            u8 g = lerp_u8(lerp_u8(p00[1], p10[1], tx),
                           lerp_u8(p01[1], p11[1], tx), ty);
            u8 b = lerp_u8(lerp_u8(p00[2], p10[2], tx),
                           lerp_u8(p01[2], p11[2], tx), ty);
            u8 a = lerp_u8(lerp_u8(p00[3], p10[3], tx),
                           lerp_u8(p01[3], p11[3], tx), ty);

            u8 *px = drow + x * 4;
            px[0] = b;
            px[1] = g;
            px[2] = r;
            px[3] = (fmt == PIXEL_XRGB) ? 0xFF : a;
        }
    }
}

static inline void blit_rgba_to_xrgb8888(u32 *dst, const u8 *src, u32 width)
{
    u32 x;
    for (x = 0; x < width; ++x) {
        u8 r = src[x * 4 + 0];
        u8 g = src[x * 4 + 1];
        u8 b = src[x * 4 + 2];
        dst[x] = 0x00000000U | (r << 16) | (g << 8) | b;
    }
}

static int rtk_drm_logo_colorbar(struct drm_framebuffer *fb)
{
    struct drm_gem_object *gobj;
    struct rtk_gem_object *rtk_obj;
    void *vaddr = NULL;
    u32 width, height, pitch;
    u32 bar_width;
    u32 x, y, i;
    u32 colors[8];

    if (!fb)
        return -EINVAL;

    gobj = fb->obj[0];
    if (!gobj)
        return -EINVAL;

    rtk_obj = to_rtk_gem_obj(gobj);
    vaddr   = rtk_obj->vaddr;
    width   = fb->width;
    height  = fb->height;
    pitch   = fb->pitches[0];

    if (!vaddr || !width || !height || !pitch) {
        DRM_INFO("rtk_drm_draw_colorbar: invalid vaddr=%p width=%u height=%u pitch=%u\n",
                 vaddr, width, height, pitch);
        return -EINVAL;
    }

    bar_width = width / 8;
    if (!bar_width)
        bar_width = 1;

    DRM_INFO("rtk_drm_draw_colorbar: vaddr=%p width=%u height=%u pitch=%u\n",
             vaddr, width, height, pitch);

    colors[0] = rtk_mk_color(0x00, 0x00, 0x00);
    colors[1] = rtk_mk_color(0xff, 0x00, 0x00);
    colors[2] = rtk_mk_color(0x00, 0xff, 0x00);
    colors[3] = rtk_mk_color(0x00, 0x00, 0xff);
    colors[4] = rtk_mk_color(0xff, 0xff, 0x00);
    colors[5] = rtk_mk_color(0xff, 0x00, 0xff);
    colors[6] = rtk_mk_color(0x00, 0xff, 0xff);
    colors[7] = rtk_mk_color(0xff, 0xff, 0xff);

    for (y = 0; y < height; ++y) {
        u32 *p = (u32 *)((u8 *)vaddr + (size_t)y * pitch);
        for (x = 0; x < width; ++x) {
            i = x / bar_width;
            if (i > 7)
                i = 7;
            p[x] = colors[i];
        }
    }

	return 0;
}

static int rtk_drm_logo_png(struct drm_framebuffer *fb)
{
    struct drm_gem_object *gobj;
    struct rtk_gem_object *rtk_obj;
    void *dst = NULL;
    u32 width, height, pitch;
    int png_width, png_height, channels;
    u8 *img = NULL;

    if (!fb)
        return -EINVAL;

    gobj = fb->obj[0];
    if (!gobj)
        return -EINVAL;

    rtk_obj = to_rtk_gem_obj(gobj);
    dst     = (u8 *)rtk_obj->vaddr;
    width   = fb->width;
    height  = fb->height;
    pitch   = fb->pitches[0];

    if (!dst || !width || !height || !pitch)
        return -EINVAL;

    DRM_INFO("rtk_drm: Panel width=%d, height=%d\n", width, height);

    img = stbi_load_from_memory(drm_splash_png, drm_splash_png_len, &png_width, &png_height, &channels, 4);
    if (!img)
        return -EINVAL;

    DRM_INFO("rtk_drm: PNG width=%d, height=%d\n", png_width, png_height);

	if( width != png_width || height != png_height) {
		resize_bilinear(dst, width, height, img, png_width, png_height, PIXEL_XRGB);
	} else {
		int y;
		for(y = 0; y < height; y++)
			blit_rgba_to_xrgb8888(dst, img, width);
	}
	
    stbi_image_free(img);

    return 0;

}

int rtk_drm_draw_logo(struct drm_framebuffer *fb)
{
	int ret;
	
	ret = splash_is_colorbar();

    DRM_INFO("rtk_drm: draw logo enter\n");
    if (splash_is_colorbar()) {
        DRM_INFO("rtk_drm: draw color-bar\n");
        ret = rtk_drm_logo_colorbar(fb);
	} else {
        DRM_INFO("rtk_drm: draw PNG\n");
		ret = rtk_drm_logo_png(fb);
	}
    DRM_INFO("rtk_drm: draw logo exit\n");

	return ret;
}

module_param(drm_splash, charp, 0444);
MODULE_PARM_DESC(drm_splash, "splash style: logo | colorbar (default: logo)");
