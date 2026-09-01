/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _RTK_FWDBG_H_
#define _RTK_FWDBG_H_

#define S_OK 0x10000000
#define RPC_BUFFER_SIZE PAGE_SIZE
#define VRPC_DBG_FUNC_BASE 18000

enum ENUM_AUDIO_KERNEL_RPC_CMD {
	ENUM_KRPC_AFW_DEBUGLEVEL = 80,
	ENUM_KRPC_GET_AFW_DEBUGLEVEL = 82,
};

enum ENUM_VIDEO_KERNEL_RPC_DBG_CMD {
	ENUM_KRPC_VFW_DBG = 0,
	ENUM_KRPC_VFW_VPMASK = 1,
	ENUM_KRPC_VFW_ENBL_PRT = 2,
	ENUM_KRPC_VFW_DSBL_PRT = 3,
	ENUM_KRPC_VFW_LOG_LVL = 4,
	ENUM_KRPC_VFW_HELP = 5,
	ENUM_KRPC_VFW_SHOW_VER = 6,
	ENUM_KRPC_VFW_HDR10 = 7,
	ENUM_KRPC_VFW_HEVC_COMP_OPT = 8,
	ENUM_KRPC_VFW_HEVC_COMP = 9,
	ENUM_KRPC_VFW_HEVC_LOSSY = 10,
	ENUM_KRPC_VFW_VP9_COMP = 11,
	ENUM_KRPC_VFW_LOG_CTRL = 12,
	ENUM_KRPC_VFW_DEC_DBG = 13,
	ENUM_KRPC_VFW_FL_DBG = 14,
	ENUM_KRPC_VFW_KBL_DBG = 15,
};

enum ENUM_RTOS_KERNEL_RPC_CMD {
	ENUM_KRPC_RTOS_SET_LOG_LVL = 0x20000001,
	ENUM_KRPC_RTOS_GET_LOG_LVL = 0x20000002,
};

enum remote_id {
	FWDBG_ACPU = 0,
	FWDBG_VCPU,
	FWDBG_HIFI,
	FWDBG_HIFI1,
	FWDBG_KR4,
	FWDBG_TARGET_MAX
};

struct fwdbg_ctl_t {
	struct class *class;
	struct device *dev;
	dev_t devno;
	struct cdev cdev;
};

struct fwdbg_rpc_info {
	struct device *dev;
	unsigned int ret;
	void *vaddr;
	dma_addr_t paddr;
	struct rtk_krpc_ept_info *fwdbg_ept_info;
	uint32_t cmd;
	uint32_t data;
	bool is_krpc_init;
	bool to_rtos;
};

struct rpc_data_t {
	uint32_t data;
	uint32_t reserved[31]; /* for 128-byte aligned */
	uint32_t result;
};
#endif /* _RTK_FWDBG_H_ */
