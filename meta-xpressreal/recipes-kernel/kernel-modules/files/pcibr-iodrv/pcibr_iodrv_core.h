/* SPDX-License-Identifier: GPL-2.0-or-later
* Driver for Realtek PCI-Express Multi-IO bridge
*
* Copyright(c) 2025 Realtek Semiconductor Corp. All rights reserved.
*
* Author:
*   Network Interface Controllers crew <nicfae@realtek.com>
*/
#ifndef _PCIBR_IODRV_CORE_H
#define _PCIBR_IODRV_CORE_H

#define DRIVER_VERSION          "1.00.01"
#define MODULENAME              "pcibr_iodrv"
#define MAX_DEV_NUM             10
#define MAX_IO_SIZE             0x100

#define BYTE    __u8
#define WORD    __u16
#define DWORD   __u32
#define DWORD64 __u64
#define BOOLEAN int
typedef void  * PVOID;
typedef BYTE  * PBYTE;
typedef WORD  * PWORD;
typedef DWORD * PDWORD;

#define PRINT_LEVEL    KERN_NOTICE

#ifdef DEBUG
#define InfPrint(fmt,args...)     printk(PRINT_LEVEL "[PCIBR] INF:" fmt "\n",## args)
#define DbgPrint(fmt,args...)     printk(PRINT_LEVEL "[PCIBR] DBG:" fmt "\n",## args)
#define ErrPrint(fmt,args...)     printk(PRINT_LEVEL "[PCIBR] Err:" fmt "\n",## args)
#define FunPrint(fmt,args...)     printk(PRINT_LEVEL "[PCIBR]" "%s %i: " fmt "\n",__FUNCTION__,__LINE__,## args)
#else
#define InfPrint(fmt,args...)     printk(PRINT_LEVEL "[PCIBR] INF:" fmt "\n",## args)
#define DbgPrint(fmt,args...)
#define ErrPrint(fmt,args...)     printk(PRINT_LEVEL "[PCIBR] Err:" fmt "\n",## args)
#define FunPrint(fmt,args...)
#endif

/*******************************************************************************
*******************************************************************************/
static inline int rtk_pci_mem_rw(void __iomem *remap_addr, uint32_t reg, int len, uint8_t *value, bool is_read)
{
    int ret = 0;

    if (is_read)
    {
        if (len == 1)
            *value = ioread8(remap_addr + reg);
        else if (len == 2)
            *(uint16_t *)value = ioread16(remap_addr + reg);
        else if (len == 4)
            *(uint32_t *)value = ioread32(remap_addr + reg);
        else
            ret = -1;
    }
    else
    {
        if (len == 1)
            iowrite8(*value, remap_addr + reg);
        else if (len == 2)
            iowrite16(*(uint16_t *)value, remap_addr + reg);
        else if (len == 4)
            iowrite32(*(uint32_t *)value, remap_addr + reg);
        else
            ret = -1;
    }

    return ret;
}

typedef struct _DEV_INFO_
{
	dev_t                   devno;
	bool                    bUsed;
} DEV_INFO, *PDEV_INFO;

typedef struct _PCIBR_IODRV_DEV_
{
	struct cdev                 cdev;
	struct pci_dev              *pdev;
	atomic_t                    count;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	struct semaphore            dev_sem;
#else
	struct mutex                dev_mutex;
#endif
	unsigned int                index;
	void __iomem                *remap_addr[6];
} PCIBR_IODRV_DEV, *PPCIBR_IODRV_DEV;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
#define __devinit
#define __devexit
#define __devexit_p(func)   func
#endif

#endif // end of #ifndef _PCIBR_IODRV_CORE_H
