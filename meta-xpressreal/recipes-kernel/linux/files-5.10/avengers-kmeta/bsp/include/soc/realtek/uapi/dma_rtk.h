#ifndef __LINUX_DMA_RTK_H_
#define __LINUX_DMA_RTK_H_
/* dma_rtk.h
 *
 * Copyright (c) 2019 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#include <uapi/linux/ion.h>

struct rtk_dma_ioc_get_memory_info_s {
	int handle;
	unsigned int heapMask; /* request: select the heap to be queried */
	unsigned int flags; /* request: set the conditions to query, 0 is to query all the conditions */
	unsigned int usedSize; /* response */
	unsigned int freeSize; /* response */
};

struct rtk_dma_ioc_sync_range {
	int handle;
	u32 phyAddr;
	unsigned int len;
};

struct rtk_dma_ioc_phy_info {
	int handle;
	unsigned long long addr;
	unsigned long long len;
};


#define RTK_DMA_IOC_MAGIC 'D'
#define RTK_DMA_TILER_ALLOC (0x0)
#define RTK_DMA_GET_LAST_ALLOC_ADDR (0x1)
#define RTK_DMA_IOC_INVALIDATE (0x10)
#define RTK_DMA_IOC_FLUSH (0x11)
#define RTK_DMA_IOC_GET_MEMORY_INFO _IOWR(RTK_DMA_IOC_MAGIC, 0x12, struct rtk_dma_ioc_get_memory_info_s)
#define RTK_DMA_IOC_INVALIDATE_RANGE (0x13)
#define RTK_DMA_IOC_FLUSH_RANGE (0x14)
#define RTK_DMA_IOC_GET_PHYINFO (0x15)

extern unsigned int retry_count_value;
extern unsigned int retry_delay_value;
#endif /* __LINUX_DMA_RTK_H_ */
