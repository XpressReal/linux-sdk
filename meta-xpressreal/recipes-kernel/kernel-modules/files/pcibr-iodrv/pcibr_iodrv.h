/* SPDX-License-Identifier: GPL-2.0-or-later
* Driver for Realtek PCI-Express Multi-IO bridge
*
* Copyright(c) 2025 Realtek Semiconductor Corp. All rights reserved.
*
* Author:
*   Network Interface Controllers crew <nicfae@realtek.com>
*/
#ifndef _PCIBR_IODRV_H
#define _PCIBR_IODRV_H

#ifndef TRUE
	#define TRUE    1
	#define FALSE   0
#endif

#define RTL_IOC_MAGIC                   'R'

#define IOC_PCI_CONFIG                  _IOWR(RTL_IOC_MAGIC, 0, PCI_CONFIG_RW)
#define IOC_GET_VERSION                 _IOR(RTL_IOC_MAGIC, 1, char[16])
#define IOC_PCI_MMIO                    _IOWR(RTL_IOC_MAGIC, 2, PCI_CONFIG_RW)

typedef struct _PCI_CONFIG_RW_
{
	union {
		unsigned char   byte;
		unsigned short  word;
		unsigned int    dword;
	};
	unsigned int        addr;
	unsigned int        size;
	unsigned char       bar;
	unsigned char       is_read;
	unsigned char       rvsd0;
	unsigned char       rvsd1;
	unsigned int        sig;
} PCI_CONFIG_RW, *PPCI_CONFIG_RW;

#endif // end of #ifndef _PCIBR_IODRV_H
