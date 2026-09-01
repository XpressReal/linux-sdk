// SPDX-License-Identifier: GPL-2.0
/*
 * Realtek PCI VGA DRM Driver for Linux 6.6
 * Minimal simple KMS DRM driver for fixed 1024x768 XRGB8888 output
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_mode_config.h>
#include <drm/drm_modes.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>

#define DRIVER_NAME "rtk_vga_drm"
#define DRIVER_DESC "Realtek PCI VGA DRM Driver for Smart NIC"
#define FB_BAR_INDEX 2

#define RTK_WIDTH        1024
#define RTK_HEIGHT        768
#define RTK_BPP            32
#define RTK_CPP             4
#define RTK_LINE_BYTES  (RTK_WIDTH * RTK_CPP)
#define RTK_FB_SIZE     (RTK_HEIGHT * RTK_LINE_BYTES)

struct rtk_vga_drm {
	struct drm_device drm;
	struct pci_dev *pdev;

	struct drm_simple_display_pipe pipe;
	struct drm_connector connector;

	resource_size_t bar2_phys;
	resource_size_t bar2_len;
	void __iomem *bar2_virt;

	struct delayed_work flush_work;
	unsigned int flush_jiffies;
	bool flush_run;
	bool enabled;
};

static inline struct rtk_vga_drm *pipe_to_rtk(struct drm_simple_display_pipe *pipe)
{
	return container_of(pipe, struct rtk_vga_drm, pipe);
}

/* ------------------------------------------------------------------------- */
/* BAR2 copy                                                                  */
/* ------------------------------------------------------------------------- */

static void rtk_vga_copy_fb_to_bar(struct rtk_vga_drm *rtk,
				   struct drm_framebuffer *fb)
{
	struct drm_gem_dma_object *dma_obj;
	void *src_base;
	unsigned int y, x;
	unsigned int src_pitch;
	size_t required_bytes;

	if (!rtk->bar2_virt || !fb)
		return;

	dma_obj = drm_fb_dma_get_gem_obj(fb, 0);
	if (!dma_obj) {
		dev_warn(rtk->drm.dev, "no dma gem object for fb\n");
		return;
	}

	if (!dma_obj->vaddr) {
		dev_warn(rtk->drm.dev, "dma gem object has no vaddr\n");
		return;
	}

	if (fb->width != RTK_WIDTH || fb->height != RTK_HEIGHT)
		return;

	src_base = dma_obj->vaddr;
	src_pitch = fb->pitches[0];
	required_bytes = (size_t)RTK_HEIGHT * RTK_LINE_BYTES;

	if (src_pitch < RTK_LINE_BYTES)
		return;

	if (rtk->bar2_len < required_bytes)
		return;

	for (y = 0; y < RTK_HEIGHT; y++) {
		const u32 *src32 = (const u32 *)((const u8 *)src_base +
						 (size_t)y * src_pitch);
		u32 __iomem *dst32 = (u32 __iomem *)((u8 __iomem *)rtk->bar2_virt +
						     (size_t)y * RTK_LINE_BYTES);

		for (x = 0; x < RTK_WIDTH; x++)
			iowrite32(src32[x], &dst32[x]);
	}

	wmb();
}

#if 0
static void rtk_vga_copy_fb_to_bar(struct rtk_vga_drm *rtk,
				   struct drm_framebuffer *fb)
{
	struct drm_gem_dma_object *dma_obj;
	void *src_base;
	unsigned int y;
	unsigned int src_pitch;
	size_t required_bytes;

	if (!rtk->bar2_virt || !fb)
		return;

	dma_obj = drm_fb_dma_get_gem_obj(fb, 0);
	if (!dma_obj) {
		dev_warn(rtk->drm.dev, "no dma gem object for fb\n");
		return;
	}

	if (!dma_obj->vaddr) {
		dev_warn(rtk->drm.dev, "dma gem object has no vaddr\n");
		return;
	}

	if (fb->width != RTK_WIDTH || fb->height != RTK_HEIGHT)
		return;

	src_base = dma_obj->vaddr;
	src_pitch = fb->pitches[0];
	required_bytes = (size_t)RTK_HEIGHT * RTK_LINE_BYTES;

	if (src_pitch < RTK_LINE_BYTES)
		return;

	if (rtk->bar2_len < required_bytes)
		return;

	for (y = 0; y < RTK_HEIGHT; y++) {
		const void *src = (const u8 *)src_base + (size_t)y * src_pitch;
		void __iomem *dst = (u8 __iomem *)rtk->bar2_virt +
				    (size_t)y * RTK_LINE_BYTES;
		memcpy_toio(dst, src, RTK_LINE_BYTES);
	}

	wmb();
}
#endif

/* ------------------------------------------------------------------------- */
/* Periodic flush                                                             */
/* ------------------------------------------------------------------------- */

static void rtk_vga_flush_workfn(struct work_struct *work)
{
	struct rtk_vga_drm *rtk =
		container_of(to_delayed_work(work), struct rtk_vga_drm, flush_work);
	struct drm_plane_state *state;

	if (!rtk->flush_run) {
		schedule_delayed_work(&rtk->flush_work, rtk->flush_jiffies);
		return;
	}

	if (!rtk->enabled) {
		schedule_delayed_work(&rtk->flush_work, rtk->flush_jiffies);
		return;
	}

	state = rtk->pipe.plane.state;
	if (state && state->fb)
		rtk_vga_copy_fb_to_bar(rtk, state->fb);

	schedule_delayed_work(&rtk->flush_work, rtk->flush_jiffies);
}

/* ------------------------------------------------------------------------- */
/* Simple display pipe                                                        */
/* ------------------------------------------------------------------------- */

static int rtk_vga_pipe_check(struct drm_simple_display_pipe *pipe,
			      struct drm_plane_state *plane_state,
			      struct drm_crtc_state *crtc_state)
{
	struct drm_framebuffer *fb = plane_state->fb;

	if (!fb)
		return 0;

	if (fb->format->format != DRM_FORMAT_XRGB8888)
		return -EINVAL;

	if (fb->width != RTK_WIDTH || fb->height != RTK_HEIGHT)
		return -EINVAL;

	if (fb->pitches[0] < RTK_LINE_BYTES)
		return -EINVAL;

	return 0;
}

static void rtk_vga_pipe_enable(struct drm_simple_display_pipe *pipe,
				struct drm_crtc_state *crtc_state,
				struct drm_plane_state *plane_state)
{
	struct rtk_vga_drm *rtk = pipe_to_rtk(pipe);

	rtk->enabled = true;

	if (plane_state && plane_state->fb)
		rtk_vga_copy_fb_to_bar(rtk, plane_state->fb);
}

static void rtk_vga_pipe_disable(struct drm_simple_display_pipe *pipe)
{
	struct rtk_vga_drm *rtk = pipe_to_rtk(pipe);

	rtk->enabled = false;
}

static void rtk_vga_pipe_update(struct drm_simple_display_pipe *pipe,
				struct drm_plane_state *old_state)
{
	struct rtk_vga_drm *rtk = pipe_to_rtk(pipe);
	struct drm_plane_state *new_state = pipe->plane.state;

	if (!new_state || !new_state->fb)
		return;

	rtk_vga_copy_fb_to_bar(rtk, new_state->fb);
}

static const struct drm_simple_display_pipe_funcs rtk_vga_pipe_funcs = {
	.enable = rtk_vga_pipe_enable,
	.disable = rtk_vga_pipe_disable,
	.update = rtk_vga_pipe_update,
	.check = rtk_vga_pipe_check,
};

static const uint32_t rtk_vga_formats[] = {
	DRM_FORMAT_XRGB8888,
};

/* ------------------------------------------------------------------------- */
/* Connector                                                                  */
/* ------------------------------------------------------------------------- */

static enum drm_connector_status
rtk_vga_conn_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static int rtk_vga_conn_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_cvt_mode(connector->dev, RTK_WIDTH, RTK_HEIGHT, 60,
			    false, false, false);
	if (!mode)
		return -ENOMEM;

	mode->type |= DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_connector_helper_funcs rtk_vga_conn_helper = {
	.get_modes = rtk_vga_conn_get_modes,
};

static const struct drm_connector_funcs rtk_vga_conn_funcs = {
	.detect = rtk_vga_conn_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

/* ------------------------------------------------------------------------- */
/* Mode config                                                                */
/* ------------------------------------------------------------------------- */

static const struct drm_mode_config_funcs rtk_vga_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

/* ------------------------------------------------------------------------- */
/* DRM driver                                                                 */
/* ------------------------------------------------------------------------- */

DEFINE_DRM_GEM_DMA_FOPS(rtk_vga_fops);

static struct drm_driver rtk_vga_drm_driver = {
	.driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops = &rtk_vga_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = "20260318",
	.major = 1,
	.minor = 0,

	DRM_GEM_DMA_DRIVER_OPS_VMAP_WITH_DUMB_CREATE(drm_gem_dma_dumb_create),
};

/* ------------------------------------------------------------------------- */
/* PCI probe/remove                                                           */
/* ------------------------------------------------------------------------- */

static int rtk_vga_drm_probe(struct pci_dev *pdev,
			     const struct pci_device_id *id)
{
	struct rtk_vga_drm *rtk;
	int ret;

	dev_info(&pdev->dev, "%s: probing\n", DRIVER_NAME);

	ret = pcim_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pcim_enable_device failed: %d\n", ret);
		return ret;
	}

	pci_set_master(pdev);

	ret = pcim_iomap_regions(pdev, BIT(FB_BAR_INDEX), DRIVER_NAME);
	if (ret) {
		dev_err(&pdev->dev, "pcim_iomap_regions failed: %d\n", ret);
		return ret;
	}

	rtk = devm_drm_dev_alloc(&pdev->dev, &rtk_vga_drm_driver,
				 struct rtk_vga_drm, drm);
	if (IS_ERR(rtk))
		return PTR_ERR(rtk);

	rtk->pdev = pdev;
	rtk->bar2_phys = pci_resource_start(pdev, FB_BAR_INDEX);
	rtk->bar2_len  = pci_resource_len(pdev, FB_BAR_INDEX);
	rtk->bar2_virt = pcim_iomap_table(pdev)[FB_BAR_INDEX];
	rtk->flush_jiffies = msecs_to_jiffies(100);
	rtk->flush_run = true;
	rtk->enabled = false;

	if (!rtk->bar2_virt) {
		dev_err(&pdev->dev, "failed to map BAR%d\n", FB_BAR_INDEX);
		return -ENOMEM;
	}

	if (rtk->bar2_len < RTK_FB_SIZE) {
		dev_err(&pdev->dev,
			"BAR%d too small: len=%pa required=%u\n",
			FB_BAR_INDEX, &rtk->bar2_len, RTK_FB_SIZE);
		return -EINVAL;
	}

	ret = drmm_mode_config_init(&rtk->drm);
	if (ret) {
		dev_err(&pdev->dev, "drmm_mode_config_init failed: %d\n", ret);
		return ret;
	}

	rtk->drm.mode_config.min_width = RTK_WIDTH;
	rtk->drm.mode_config.min_height = RTK_HEIGHT;
	rtk->drm.mode_config.max_width = RTK_WIDTH;
	rtk->drm.mode_config.max_height = RTK_HEIGHT;
	rtk->drm.mode_config.funcs = &rtk_vga_mode_config_funcs;

	ret = drm_connector_init(&rtk->drm, &rtk->connector,
				 &rtk_vga_conn_funcs,
				 DRM_MODE_CONNECTOR_VGA);
	if (ret) {
		dev_err(&pdev->dev, "drm_connector_init failed: %d\n", ret);
		return ret;
	}

	drm_connector_helper_add(&rtk->connector, &rtk_vga_conn_helper);

	ret = drm_simple_display_pipe_init(&rtk->drm, &rtk->pipe,
					   &rtk_vga_pipe_funcs,
					   rtk_vga_formats,
					   ARRAY_SIZE(rtk_vga_formats),
					   NULL,
					   &rtk->connector);
	if (ret) {
		dev_err(&pdev->dev,
			"drm_simple_display_pipe_init failed: %d\n", ret);
		return ret;
	}

	drm_mode_config_reset(&rtk->drm);

	INIT_DELAYED_WORK(&rtk->flush_work, rtk_vga_flush_workfn);
	schedule_delayed_work(&rtk->flush_work, rtk->flush_jiffies);

	pci_set_drvdata(pdev, rtk);

	ret = drm_dev_register(&rtk->drm, 0);
	if (ret) {
		dev_err(&pdev->dev, "drm_dev_register failed: %d\n", ret);
		cancel_delayed_work_sync(&rtk->flush_work);
		return ret;
	}

	drm_fbdev_generic_setup(&rtk->drm, RTK_BPP);

	dev_info(&pdev->dev,
		 "%s: registered /dev/dri/card%d\n",
		 DRIVER_NAME, rtk->drm.primary->index);

	return 0;
}

static void rtk_vga_drm_remove(struct pci_dev *pdev)
{
	struct rtk_vga_drm *rtk = pci_get_drvdata(pdev);

	if (!rtk)
		return;

	rtk->flush_run = false;
	cancel_delayed_work_sync(&rtk->flush_work);
	drm_dev_unregister(&rtk->drm);
}

/* ------------------------------------------------------------------------- */
/* PCI table                                                                  */
/* ------------------------------------------------------------------------- */

static const struct pci_device_id rtk_vga_id_table[] = {
	{ PCI_DEVICE(0x10ef, 0x816f) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, rtk_vga_id_table);

static struct pci_driver rtk_vga_pci_driver = {
	.name     = DRIVER_NAME,
	.id_table = rtk_vga_id_table,
	.probe    = rtk_vga_drm_probe,
	.remove   = rtk_vga_drm_remove,
};

module_pci_driver(rtk_vga_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Realtek");
MODULE_DESCRIPTION("Realtek PCI VGA DRM Driver for Linux 6.6");

