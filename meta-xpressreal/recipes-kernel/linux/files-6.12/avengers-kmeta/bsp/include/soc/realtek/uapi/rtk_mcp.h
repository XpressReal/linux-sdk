/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __UAPI_RTK_MCP_H
#define __UAPI_RTK_MCP_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MCP_IOC_MAGIC                          'm'
#define MCP_IOCTL_ENABLE_AUTO_PADDING          _IO(MCP_IOC_MAGIC, 2)
#define MCP_IOCTL_DISABLE_AUTO_PADDING         _IO(MCP_IOC_MAGIC, 3)
#define SMCP_IOCTL_ENCRYPT_IMAGE_WITH_IK       _IOWR(MCP_IOC_MAGIC, 4, struct smcp_desc_set)
#define SMCP_IOCTL_DECRYPT_IMAGE_WITH_IK       _IOWR(MCP_IOC_MAGIC, 5, struct smcp_desc_set)
#define MCP_IOCTL_DO_COMMAND                   _IOW(MCP_IOC_MAGIC, 6, struct mcp_desc_set)
#define MCP_IOCTL_MEM_IMPORT                   _IOWR(MCP_IOC_MAGIC, 8, struct mcp_mem_import)
#define MCP_IOCTL_MEM_FREE                     _IOWR(MCP_IOC_MAGIC, 9, struct mcp_mem_free)

#define MCP_IOCTL_DO_COMMAND_LEGACY            0x70000001

struct mcp_desc {
	__u32 ctrl;
	__u32 key[6];
	__u32 iv[4];
	__u32 src;
	__u32 dst;
	__u32 bytecount;
};

struct mcp_desc_set {
	union {
		struct mcp_desc *p_desc;
		__u64 pad0;
	};
	__u32 n_desc;
	__u32 pad1;
};

struct mcp_mem_import {
	__s32 fd;
	__u32 flags;
	__u32 mcp_va;
};

struct mcp_mem_free {
	__u32 mcp_va;
};

#define MCP_MODE_MASK     0x0000001f
#define MCP_MODE_DES      0x00000000
#define MCP_MODE_3DES     0x00000001
#define MCP_MODE_RC4      0x00000002
#define MCP_MODE_MD5      0x00000003
#define MCP_MODE_SHA_1    0x00000004
#define MCP_MODE_AES      0x00000005
#define MCP_MODE_AES_G    0x00000006
#define MCP_MODE_AES_H    0x00000007
#define MCP_MODE_CMAC     0x00000008
#define MCP_MODE_SHA256   0x0000000b
#define MCP_MODE_AES_192  0x0000001d
#define MCP_MODE_AES_256  0x00000015

#define MCP_ENC_MASK      0x00000020
#define MCP_DEC           0x00000000
#define MCP_ENC           0x00000020

#define MCP_BCM_MASK      0x000000c0
#define MCP_BCM_ECB       0x00000000
#define MCP_BCM_CBC       0x00000040
#define MCP_BCM_CTR       0x00000080
#define MCP_RC4           0x000000c0

#define MCP_IV_SEL_MASK   0x00000800
#define MCP_IV_SEL_DESC   0x00000000
#define MCP_IV_SEL_REG    0x00000800

#define MCP_KEY_SEL_MASK  0x00003000
#define MCP_KEY_SEL_DESC  0x00000000
#define MCP_KEY_SEL_OTP   0x00001000
#define MCP_KEY_SEL_CW    0x00002000

struct smcp_desc_set {
	__u32 data_in;
	__u32 data_out;
	__u32 length;
};

#endif
