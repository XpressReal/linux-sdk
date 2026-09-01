/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _UAPI_RTK_RPC_H
#define _UAPI_RTK_RPC_H

/* Use 'k' as magic number */
#define RPC_IOC_MAGIC 'k'

/*
 * S means "Set": through a ptr,
 * T means "Tell": directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */
#define RPC_IOCTTIMEOUT _IO(RPC_IOC_MAGIC,  0)
#define RPC_IOCQTIMEOUT _IO(RPC_IOC_MAGIC,  1)
#define RPC_IOCTRESET _IO(RPC_IOC_MAGIC,  2)
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
#define RPC_IOCTHANDLER _IO(RPC_IOC_MAGIC, 3)
#endif

struct S_RPC_IOC_PROCESS_CONFIG_0 {
    int bStayActive;
    int reserved[16-1];
};
#define RPC_IOC_PROCESS_CONFIG_0 _IOW(RPC_IOC_MAGIC, 4, struct S_RPC_IOC_PROCESS_CONFIG_0)
#define RPC_IOCTEXITLOOP _IO(RPC_IOC_MAGIC, 5)
#define RPC_IOCTGETGPID _IOR(RPC_IOC_MAGIC, 6, int)
#define RPC_IOCTGETGTGID _IOR(RPC_IOC_MAGIC, 7, int)

/*
 * struct RPC_DBG_FLAG
 * @uint32_t op: 0:get, 1:set
 */
struct RPC_DBG_FLAG {
	uint32_t op;
	uint32_t flagValue;
	uint32_t flagAddr;
};
#define RPC_DBGREG_GET 0
#define RPC_DBGREG_SET 1
#define RPC_IOCTRGETDBGREG_A _IOWR(RPC_IOC_MAGIC, 0x10, struct RPC_DBG_FLAG)
#define RPC_IOCTRGETDBGREG_V _IOWR(RPC_IOC_MAGIC, 0x11, struct RPC_DBG_FLAG)
#define RPC_IOCTRGETDBGPRINT_V _IOWR(RPC_IOC_MAGIC, 0x12, struct RPC_DBG_FLAG)

#endif /* _UAPI_RTK_RPC_H */
