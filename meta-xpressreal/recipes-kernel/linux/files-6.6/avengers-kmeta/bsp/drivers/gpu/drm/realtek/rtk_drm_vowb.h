/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __RTK_DRM_VOWB_H__
#define __RTK_DRM_VOWB_H__

#include <linux/errno.h>
#include <linux/platform_device.h>

struct rtk_drm_vowb;
struct inode;
struct file;
struct drm_file;
struct drm_device;

extern struct platform_driver rtk_vowb_driver;

int rtk_drm_vowb_release(struct inode *inode, struct file *filp);
void rtk_drm_vowb_isr(struct drm_device *dev);

int rtk_drm_vowb_setup_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_teardown_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_add_src_pic_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_start_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_stop_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_set_crtc_vblank_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_get_dst_pic_ioctl(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_run_cmd(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_check_cmd(struct drm_device *dev, void *data, struct drm_file *file_priv);
int rtk_drm_vowb_reinit(struct drm_device *dev, void *data, struct drm_file *file_priv);

#endif
