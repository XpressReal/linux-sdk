/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */

#ifndef __UAPI_RTK_PM_H
#define __UAPI_RTK_PM_H

struct pm_pcpu_param_hdr {
	uint32_t type; /* Flag present the wakeup source */
	uint32_t version; /* Param/info version */
	uint32_t len; /* Param/info length, including the wksrc_param_hdr */
};

#endif /* __UAPI_RTK_PM_H */
