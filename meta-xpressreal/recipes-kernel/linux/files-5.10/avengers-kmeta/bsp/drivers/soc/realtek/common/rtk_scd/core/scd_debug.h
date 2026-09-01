// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Copyright (c) 2019 Realtek Semiconductor Corp.
 */

#ifndef __SCD_DEBUG_H__
#define __SCD_DEBUG_H__

#include <linux/slab.h> // for kmalloc and free

//-- scd debug messages
//#define CONFIG_SMARTCARD_DBG
//#define CONFIG_SCD_INT_DBG
#define CONFIG_SCD_TX_DBG      // oo>> dump TX INT log may causes RX overflow if Rx more than FIFO depth
//#define CONFIG_SCD_RX_DBG
//#define CONFIG_SCD_INFO
#define CONFIG_SCD_WARNING

#ifdef  CONFIG_SMARTCARD_DBG
#define SC_TRACE(fmt, args...)		printk(KERN_EMERG "[SCD] TRACE,(%s,%d)" fmt, __FILE__, __LINE__, ## args)
#else
#define SC_TRACE(args...)			do{}while(0)
#endif


#ifdef  CONFIG_SCD_INT_DBG
#define SC_INT_DBG(fmt, args...)	printk(KERN_EMERG "[SCD] INT(%d), " fmt, __LINE__, ## args)
#else
#define SC_INT_DBG(args...)			do{}while(0)
#endif

#ifdef CONFIG_SCD_INFO
#define SC_INFO(fmt, args...)		printk(KERN_EMERG "[SCD] Info,(%d) " fmt, __LINE__, ## args)
#else
#define SC_INFO(fmt, args...)		do{}while(0)
#endif

#ifdef CONFIG_SCD_WARNING
#define SC_WARNING(fmt, args...)	printk(KERN_EMERG "[SCD] Warning,(%s,%d)" fmt, __FILE__, __LINE__, ## args)
#else
#define SC_WARNING(fmt, args...)    do{}while(0)
#endif

#ifdef CONFIG_SCD_TX_DBG
#define SC_TX_INT_DBG(fd, fmt, args...)  \
	do  { \
		unsigned char *temp = (unsigned char *)kmalloc(TX_INT_LOG_MAX_LEN, GFP_KERNEL | __GFP_ZERO); \
		if(temp) { \
			sprintf(temp, "[SCD] INT(%d), " fmt, __LINE__, ## args); \
			kfifo_in(&(fd), temp, strlen(temp)); \
			kfree(temp); \
		} \
	} while(0);

#define SC_TX_INT_DUMP(fd, tx_len, bDump) \
	do { \
		int log_len = kfifo_len(&(fd)); \
		if( (bDump) && log_len)  { \
			unsigned char *temp = (unsigned char *)kmalloc(TX_INT_LOG_MAX_LEN, GFP_KERNEL | __GFP_ZERO); \
			if(temp) { \
				log_len = kfifo_out(&(fd), temp, sizeof(temp)); \
				printk(KERN_EMERG "\n[SCD][INT](%d) begin dump INT LOG: tx(%d), log(%d):\n%s[SCD][INT] end dump INT LOG\n", __LINE__, (tx_len), log_len, temp); \
				kfree(temp); \
			} \
		} \
	} while(0);
#else

#define SC_TX_INT_DBG(fd, fmt, args...)    do{}while(0)
#define SC_TX_INT_DUMP(fd, tx_len, bDump)  do{}while(0)

#endif // CONFIG_SCD_TX_DBG

#endif  //__SCD_DEBUG_H__
