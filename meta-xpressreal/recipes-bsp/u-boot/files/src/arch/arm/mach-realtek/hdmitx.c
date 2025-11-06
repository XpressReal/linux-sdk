/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2025 by Gary Gen <gary_gen@realtek.com>
 *
 */

#include <common.h>
#include <command.h>
#include <linux/libfdt.h>
#include <rtk/rtk_rpc.h>
#include <mach/system.h>
#include <asm/arch/io.h>
#include <asm/arch/cpu.h>
#include <fdt_support.h>
#include <usb.h>

void set_hdmi_off(void);
bool rtk_fdt_hdmi_is_off(void *fdt);

bool rtk_fdt_hdmi_is_off(void *fdt)
{
	int nodeoffset;
	int lenp;
	const void *prop;
	const char *status;
	const char *path_drm_hdmi = "/hdmi";

	nodeoffset = fdt_path_offset(fdt, path_drm_hdmi);
	if (nodeoffset < 0)
		return false;

	prop = fdt_getprop(fdt, nodeoffset, "status", &lenp);
	if (prop == NULL)
		return false;

	if (lenp < 3)
		return false;

	status = prop;

	if (status[0] == 'o' && status[1] == 'k')
		return false;

	return true;
}

void set_hdmi_off(void)
{
	struct _BOOT_TV_STD_INFO boot_info;
	memset(&boot_info, 0x0, sizeof(struct _BOOT_TV_STD_INFO));
	boot_info.dwMagicNumber = __swap_32(0xC0DE0BEE); /* set magic pattern in first word */

	boot_info.tv_sys.videoInfo.standard  = __swap_32(VO_STANDARD_NTSC_J);
	boot_info.tv_sys.videoInfo.enProg    =  1;
	boot_info.tv_sys.videoInfo.pedType   = __swap_32(1);	// ignored.
	boot_info.tv_sys.videoInfo.dataInt0  = __swap_32(4);	// related to deep color.
	boot_info.tv_sys.hdmiInfo.hdmiMode	 = __swap_32(VO_HDMI_OFF);

	memcpy((void *)(uintptr_t)VO_RESOLUTION, &boot_info, sizeof(struct _BOOT_TV_STD_INFO) );
	flush_cache(VO_RESOLUTION, sizeof(struct _BOOT_TV_STD_INFO));
	printf("Set HDMI TX OFF\n");
}