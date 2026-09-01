/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2020 Realtek Semiconductor Corp.
 */

#ifndef __UAPI_RTK_CEC_H
#define __UAPI_RTK_CEC_H

//#include "rtk_pm.h"
#include <soc/realtek/rtk_pm.h>

#define CEC_WK_VER 0x01000000
#define MAX_OSD_LEN 32

struct ipc_shm_cec_v1 {
	unsigned int standby_config;
	unsigned char standby_logical_addr;
	unsigned short standby_physical_addr;
	unsigned char standby_cec_version;
	unsigned int standby_vendor_id;
	unsigned short standby_rx_mask;
	unsigned char standby_cec_wakeup_off;
	unsigned char standby_osd_name[MAX_OSD_LEN];
};
struct ipc_shm_cec_v2 {
	struct pm_pcpu_param_hdr hdr;
	uint8_t cec_version;
	uint8_t cec_wakeup_off;
	uint8_t logical_addr;
	uint8_t rsvd;
	uint16_t physical_addr;
	uint16_t rx_mask;
	uint32_t config; /* wakeup flags */
	uint32_t vendor_id; /* vendor id */
	uint8_t osd_name[MAX_OSD_LEN];
} __packed;

#endif /* __UAPI_RTK_CEC_H */
