// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek RPC driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
//#define DEBUG

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/ion.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/compat.h>
#include <uapi/linux/ion.h>

#include <soc/realtek/uapi/ion_rtk.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/rtk_refclk.h>
#include <trace/events/rtk_rpc.h>

#include "rpc.h"
#include "rpc_mem_uapi.h"
#include "rpc_trace.h"

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

unsigned int retry_count_value = 5;
unsigned int retry_delay_value = 200;

static void rpc_mem_add(struct rpc_device *rpc_dev, struct rpc_mem_entry *entry);
static struct rpc_mem_entry *rpc_mem_remove(struct rpc_device *rpc_dev,
	    unsigned long phys_addr);

#define update_currProc(user, proc) \
do { \
user->currProc = proc; \
} while (0)

#define update_nextRpc(user, next) \
do { \
user->nextRpc = next; \
} while (0)

static inline int rpc_done(struct user_rpc *user)
{
	int ret = 0;
	uint32_t ringOut;

	ringOut = rpc_ringbuf_get_ringOut(&user->read_record);

	ret = (ringOut == user->nextRpc);

	return ret ;
}

static inline int ring_empty(struct user_rpc *user)
{
	int ret = 0;

	ret = rpc_ringbuf_empty(&user->read_record);

	return ret;
}

#define need_dispatch(user) \
	(user->currProc == NULL && !ring_empty(user) && rpc_done(user))

static ssize_t rpc_mem_read(struct rpc_client *client, char *buf, size_t count)
{
	struct user_rpc *user;
	ssize_t ret = 0;
	bool to_user = false;

	user = &client->user_intr;

	ret = rpc_ringbuf_read(&user->read_record, buf, count, to_user);

	spin_lock_bh(&user->lock);

	if (rpc_done(user)) {
		pr_debug("%s: Previous RPC is done, unregister myself\n",
			    client->name);
		update_currProc(user, NULL);
	}

	spin_unlock_bh(&user->lock);

	if (need_dispatch(user))
		tasklet_schedule(&(user->tasklet));

	return ret;
}

static ssize_t rpc_mem_write(struct rpc_client *client, char *buf, size_t count)
{
	struct user_rpc *user;
	ssize_t ret = 0;
	struct rpc_struct *rpc;
	bool big_endian;
	bool from_user = false;

	user = &client->user_intr;
	big_endian = client->big_endian;

	rpc = (struct rpc_struct *)(buf);
	pr_debug("%s: program:%u version:%u procedure:%u taskID:%u sysTID:%u sysPID:%u size:%u context:%x 90k:%u %s\n",
		    __func__, FW2SCPU(big_endian, rpc->programID),
		    FW2SCPU(big_endian, rpc->versionID),
		    FW2SCPU(big_endian, rpc->procedureID),
		    FW2SCPU(big_endian, rpc->taskID),
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
		    FW2SCPU(big_endian, rpc->sysTID),
#else
		    0,
#endif /* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
		    FW2SCPU(big_endian, rpc->sysPID),
		    FW2SCPU(big_endian, rpc->parameterSize),
		    FW2SCPU(big_endian, rpc->mycontext),
		    (u32)refclk_get_val_raw(), in_atomic() ? "atomic" : "");

	ret = rpc_ringbuf_write(&user->write_record, buf, count, from_user);

	rpc_send_interrupt(client);

	return ret;
}

static phys_addr_t __maybe_unused rtk_rpc_ion_pa(struct rpc_client *client,
	    struct rpc_mem_entry *rpc_entry)
{
	struct dma_buf *buf = rpc_entry->rpc_dmabuf;
	struct sg_table *table;
	struct dma_buf_attachment *attachment;
	dma_addr_t dma_addr;
	int err;

	attachment = dma_buf_attach(buf, client->dev);
	if (IS_ERR(attachment)) {
		err = PTR_ERR(attachment);
		goto error;
	}
	rpc_entry->attachment = attachment;

	table = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (!table) {
		err = -ENOMEM;
		goto detach;
	}

	dma_addr = sg_dma_address(table->sgl);

	rpc_entry->attachment = attachment;
	rpc_entry->table = table;

	return dma_addr;
detach:

	dma_buf_detach(buf, attachment);
error:

	return err;
}

static void rtk_rpc_free_ion(struct dma_buf *dmabuf, struct dma_buf_attachment
				 *attach)
{
	pr_info("%s free ion buffer\n", __func__);
	BUG_ON(!dmabuf);
	BUG_ON(!attach);

	dma_buf_unmap_attachment(attach, attach->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}

enum RPC_REMOTE_CMD {
	RPC_REMOTE_CMD_ALLOC = 1,
	RPC_REMOTE_CMD_FREE = 2,
	RPC_REMOTE_CMD_ALLOC_SECURE_LEGACY = 3, /* legacy */
};

struct fw_alloc_parameter_legacy {
	uint32_t size;
} __attribute__((aligned(1)));

struct fw_free_parameter {
	uint32_t phys_addr;
} __attribute__((aligned(1)));

enum E_FW_ALLOC_FLAGS {
	eAlloc_Flag_SCPUACC                 = 1U << 31,
	eAlloc_Flag_ACPUACC                 = 1U << 30,
	eAlloc_Flag_HWIPACC                 = 1U << 29,
	eAlloc_Flag_VE_SPEC                 = 1U << 28,
	eAlloc_Flag_PROTECTED_AUDIO_POOL    = 1U << 27,
	eAlloc_Flag_PROTECTED_TP_POOL       = 1U << 26,
	eAlloc_Flag_PROTECTED_VO_POOL       = 1U << 25,
	eAlloc_Flag_PROTECTED_VIDEO_POOL    = 1U << 24,
	eAlloc_Flag_PROTECTED_AO_POOL       = 1U << 23,
	eAlloc_Flag_PROTECTED_METADATA_POOL = 1U << 22,
	eAlloc_Flag_VCPU_FWACC              = 1U << 21,
	eAlloc_Flag_CMA                     = 1U << 20,
	eAlloc_Flag_PROTECTED_FW_STACK      = 1U << 19,
	eAlloc_Flag_PROTECTED_EXT_BIT0      = 1U << 18,
	eAlloc_Flag_PROTECTED_EXT_BIT1      = 1U << 17,
	eAlloc_Flag_PROTECTED_EXT_BIT2      = 1U << 16,
	eAlloc_Flag_SKIP_ZERO               = 1U << 15,
};

struct fw_alloc_parameter {
	uint32_t size;
	uint32_t flags; /* enum E_FW_ALLOC_FLAGS */
} __attribute__((aligned(1)));

struct reply_fw_parameter {
	uint32_t taskID;
	uint32_t reply_value;
} __attribute__((aligned(1)));

static void endian_swap_32_read(void *buf, size_t size)
{
	if ((size%sizeof(int)) != 0) {
		pr_err("%s : Illegal size %zu\n", __func__, size);
		return;
	} else {
		unsigned int * pData = (unsigned int *) buf;
		size_t i;
		for (i=0; i<(size/sizeof(int));i++) {
			pData[i] = ntohl(pData[i]);
		}
	}
}

static void endian_swap_32_write(void *buf, size_t size)
{
	if ((size%sizeof(int)) != 0) {
		pr_err("%s : Illegal size %zu\n", __func__, size);
		return;
	} else {
		unsigned int * pData = (unsigned int *) buf;
		size_t i;
		for (i=0; i<(size/sizeof(int));i++) {
			pData[i] = htonl(pData[i]);
		}
	}
}

static unsigned int rpc_ion_alloc_handler_legacy(struct rpc_client *client,
	    bool bSecure, const struct fw_alloc_parameter_legacy *param)
{
#define FW_ALLOC_SPEC_MASK  0xC0000000
#define FW_ALLOC_VCPU_FWACC 0x40000000
#define FW_ALLOC_VCPU_EXTRA 0x80000000
	unsigned int reply_value = 0;

	struct device *dev = client->dev;
	//struct dma_heap *heap = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct rpc_mem_entry *rpc_entry;
	unsigned int fw_send_value = param->size;
	size_t alloc_val = 0;
	struct dma_buf *dmabuf = NULL;
	unsigned int ion_alloc_heap_mask;
	unsigned int ion_alloc_flags;

	do {

		if (bSecure) {
			ion_alloc_heap_mask = RTK_ION_HEAP_MEDIA_MASK | RTK_ION_HEAP_SECURE_MASK;
			ion_alloc_flags = RTK_FLAG_PROTECTED_V2_VO_POOL | RTK_FLAG_HWIPACC;
			if (RPC_AUDIO == client->id)
				ion_alloc_flags |= RTK_FLAG_ACPUACC;
		} else {
			ion_alloc_heap_mask = RTK_ION_HEAP_MEDIA_MASK;
			ion_alloc_flags = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_HWIPACC | RTK_FLAG_ACPUACC;
		}

		alloc_val = PAGE_ALIGN(fw_send_value & ~FW_ALLOC_SPEC_MASK);

		if (fw_send_value & FW_ALLOC_VCPU_FWACC) {
			ion_alloc_flags |= RTK_FLAG_VCPU_FWACC;
		}


		if (fw_send_value & FW_ALLOC_VCPU_EXTRA) {
			ion_alloc_flags |= RTK_FLAG_CMA;
		} else {
			/* reserved */
		}

		dmabuf = rheap_alloc("rtk_media_heap", alloc_val,
		                        ion_alloc_flags);

		if (IS_ERR_OR_NULL(dmabuf)) {
			if (ion_alloc_flags & RTK_FLAG_CMA) {
				ion_alloc_flags &= ~RTK_FLAG_CMA;
				dmabuf = rheap_alloc("rtk_media_heap",
						 alloc_val, ion_alloc_flags);
				if (IS_ERR_OR_NULL(dmabuf)) {
					dev_err(dev, "Failed to rheap_alloc\n");
					goto rheap_err;
				}
			} else {
				dev_err(dev, "Failed to rheap_alloc\n");
				goto rheap_err;
			}
		}

		rpc_entry = kmalloc(sizeof(struct rpc_mem_entry),
				GFP_KERNEL);

		rpc_entry->next = NULL;
		rpc_entry->rpc_dmabuf   = dmabuf;

		attach = dma_buf_attach(rpc_entry->rpc_dmabuf, dev);
		if (IS_ERR(attach)) {
			dev_err(dev, "Failed to attach dmabuf\n");
			goto attach_err;
		}

		table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
		if (IS_ERR(table)) {
			dev_err(dev, "Failed to map attachment \n");
			goto map_err;
		}
		rpc_entry->attachment = attach;
		rpc_entry->phys_addr = sg_phys(table->sgl);

		rpc_entry->size = alloc_val;
		rpc_mem_add(client->rpc_dev, rpc_entry);

		reply_value = rpc_entry->phys_addr;

		pr_debug("[%s] ion_alloc addr : 0x%x (heap:0x%x flags:0x%x)\n", __func__,
				reply_value, ion_alloc_heap_mask, ion_alloc_flags);

	} while (0);
	return reply_value;
map_err:
	dma_buf_detach(rpc_entry->rpc_dmabuf, attach);
attach_err:
	dma_buf_put(rpc_entry->rpc_dmabuf);
	kfree(rpc_entry);
rheap_err:
	reply_value = -1U;
	return reply_value;
}

static unsigned int rpc_ion_alloc_handler(struct rpc_client *client,
	    const struct fw_alloc_parameter *param)
{
	unsigned int reply_value = 0;
	unsigned int retry_count = 0;

	struct device *dev = client->dev;
	//struct dma_heap *heap = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct rpc_mem_entry *rpc_entry;
	struct dma_buf *dmabuf = NULL;

	size_t alloc_size = param->size;
	unsigned int alloc_flags = param->flags;
	unsigned int ion_alloc_heap_mask = RTK_ION_HEAP_MEDIA_MASK | RTK_ION_HEAP_SECURE_MASK;
	unsigned int ion_alloc_flags = 0;
retry:
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_SCPUACC                 ) ? RTK_FLAG_SCPUACC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_ACPUACC                 ) ? RTK_FLAG_ACPUACC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_HWIPACC                 ) ? RTK_FLAG_HWIPACC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_VE_SPEC                 ) ? RTK_FLAG_VE_SPEC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_AUDIO_POOL    ) ? RTK_FLAG_PROTECTED_V2_AUDIO_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_TP_POOL       ) ? RTK_FLAG_PROTECTED_V2_TP_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_VO_POOL       ) ? RTK_FLAG_PROTECTED_V2_VO_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_VIDEO_POOL    ) ? RTK_FLAG_PROTECTED_V2_VIDEO_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_AO_POOL       ) ? RTK_FLAG_PROTECTED_V2_AO_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_METADATA_POOL ) ? RTK_FLAG_PROTECTED_V2_METADATA_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_VCPU_FWACC              ) ? RTK_FLAG_VCPU_FWACC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_CMA                     ) ? RTK_FLAG_CMA : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_FW_STACK      ) ? RTK_FLAG_PROTECTED_V2_FW_STACK : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_EXT_BIT0      ) ? RTK_FLAG_PROTECTED_EXT_BIT0 : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_EXT_BIT1      ) ? RTK_FLAG_PROTECTED_EXT_BIT1 : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_EXT_BIT2      ) ? RTK_FLAG_PROTECTED_EXT_BIT2 : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_SKIP_ZERO               ) ? RTK_FLAG_SKIP_ZERO : 0;
	dev_info(client->dev, "%s alloc_size=0x%zx  ion_alloc_flags=0x%x \n", __func__,
			alloc_size, ion_alloc_flags);

	dmabuf = rheap_alloc("rtk_media_heap", alloc_size,
				ion_alloc_flags);

	if (IS_ERR_OR_NULL(dmabuf)) {
		if (ion_alloc_flags & RTK_FLAG_CMA) {
			ion_alloc_flags &= ~RTK_FLAG_CMA;
			dmabuf = rheap_alloc("rtk_media_heap",
					alloc_size, ion_alloc_flags);
		}
		if (IS_ERR_OR_NULL(dmabuf)) {
			if (retry_count < retry_count_value) {
				msleep(retry_delay_value);
				retry_count++;
				ion_alloc_flags = 0;
				goto retry;
			} else {
				pr_err("%s: ion alloc retry_delay = %d,"
					" count = %d, MAX_COUNT = %d\n",
					__func__, retry_delay_value,
					retry_count, retry_count_value);
				goto rheap_err;
			}
		}
	}

	rpc_entry = kmalloc(sizeof(struct rpc_mem_entry), GFP_KERNEL);
	rpc_entry->next = NULL;
	rpc_entry->rpc_dmabuf = dmabuf;
	attach = dma_buf_attach(rpc_entry->rpc_dmabuf, dev);
	if (IS_ERR(attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		goto attach_err;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		dev_err(dev, "Failed to map attachment \n");
		goto map_err;
	}
	rpc_entry->attachment = attach;
	rpc_entry->phys_addr = sg_phys(table->sgl);

	rpc_entry->size = alloc_size;
	rpc_mem_add(client->rpc_dev, rpc_entry);
	reply_value = rpc_entry->phys_addr;

	dev_dbg(client->dev, "[%s] ion_alloc addr : 0x%x (heap:0x%x flags:0x%x)\n",
		     __func__, reply_value, ion_alloc_heap_mask,
		     ion_alloc_flags);

	return reply_value;
map_err:
	dma_buf_detach(rpc_entry->rpc_dmabuf, attach);
attach_err:
	dma_buf_put(rpc_entry->rpc_dmabuf);
	kfree(rpc_entry);
rheap_err:
	reply_value = -1U;
	return reply_value;
}

static unsigned int rpc_ion_free_handler(struct rpc_client *client,
	    const struct fw_free_parameter *param)
{
	unsigned int reply_value = 0;
	unsigned int phys_addr = param->phys_addr;
	struct rpc_mem_entry *rpc_entry;

	rpc_entry = rpc_mem_remove(client->rpc_dev, phys_addr);
	if (rpc_entry) {
		rtk_rpc_free_ion(rpc_entry->rpc_dmabuf, rpc_entry->attachment);
		kfree(rpc_entry);
		dev_dbg(client->dev, "[%s] ion_free addr : 0x%x (reply_value : 0x%x)\n",
			    __func__, phys_addr, reply_value);
		reply_value = 0;
	} else {
		dev_err(client->dev, "[%s]cannot find rpc_entry to free:phys_addr:%x\n",
			    __func__, phys_addr);
		reply_value = -1U;
	}
	return reply_value;
}

void rpc_ion_handler(struct rpc_client *client)
{
	struct user_rpc *user;
	unsigned int reply_value = 0;
	struct rpc_struct rpc_header;
	struct rpc_struct* rpc = &rpc_header;
	enum RPC_REMOTE_CMD remote_cmd = 0;
	bool big_endian = client->big_endian;

	user = &client->user_intr;

	peek_rpc_struct(__func__, &user->read_record, 0);

	if (rpc_mem_read(client, (char *) rpc, sizeof(struct rpc_struct)) !=
		    sizeof(struct rpc_struct)) {
		pr_err("%s:%d\n", __func__, __LINE__);
		return;
	}
	if (big_endian)
		endian_swap_32_read(rpc, sizeof(*rpc));

	pr_debug("%s: program:%u version:%u procedure:%u taskID:%u sysTID:%u sysPID:%u size:%u context:%x 90k:%u %s\n",
		    __func__, (rpc->programID), (rpc->versionID),
		    (rpc->procedureID), (rpc->taskID),
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
		    (rpc->sysTID),
#else
		    0,
#endif /* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
		    (rpc->sysPID), (rpc->parameterSize), (rpc->mycontext),
		    (u32)refclk_get_val_raw(), in_atomic() ? "atomic" : "");

	if (rpc->parameterSize == 0) {
		pr_err("%s: parameterSize is zero!!\n", __func__);
		return;
	}

	remote_cmd = (enum RPC_REMOTE_CMD) rpc->procedureID;

	switch (remote_cmd) {
	case RPC_REMOTE_CMD_ALLOC_SECURE_LEGACY:
	case RPC_REMOTE_CMD_ALLOC:
		if (rpc->parameterSize == sizeof(struct fw_alloc_parameter_legacy)) {
			bool bSecure = (remote_cmd == RPC_REMOTE_CMD_ALLOC_SECURE_LEGACY) ? true : false;
			struct fw_alloc_parameter_legacy param;

			if (rpc_mem_read(client, (char *) &param, sizeof(param))
				    != sizeof(param)) {
				pr_err("%s : read struct fw_alloc_parameter_legacy failed!\n",
					    __func__);

				return;
			}
			if (big_endian)
				endian_swap_32_read(&param, sizeof(param));

			reply_value = rpc_ion_alloc_handler_legacy(client,
				    bSecure, &param);

		} else if (rpc->parameterSize == sizeof(struct fw_alloc_parameter)) {
			struct fw_alloc_parameter param;

			if (rpc_mem_read(client, (char *) &param, sizeof(param))
				    != sizeof(param)) {
				pr_err("%s : read struct fw_alloc_parameter failed!\n",
					    __func__);

				return;
			}
			if (big_endian)
				endian_swap_32_read(&param, sizeof(param));

			dev_dbg(client->dev, "%s alloc size=%d flags=0x%x\n",
				    __func__, param.size, param.flags);
			reply_value = rpc_ion_alloc_handler(client, &param);
		} else {
			pr_err("%s : parameterSize(%d) mismatch!\n",
				    __func__, rpc->parameterSize);

			return;
		}

		rpc->mycontext &= 0xfffffffc;
		break;
	case RPC_REMOTE_CMD_FREE:
		if (rpc->parameterSize == sizeof(struct fw_free_parameter)) {
			struct fw_free_parameter param;

			if (rpc_mem_read(client, (char *) &param, sizeof(param)) != sizeof(param)) {
				pr_err("%s : read struct fw_alloc_parameter failed!\n",
					    __func__);
				return;
			}
			if (big_endian)
				endian_swap_32_read(&param, sizeof(param));

			reply_value = rpc_ion_free_handler(client, &param);

		} else {
			pr_err("%s : parameterSize(%d) mismatch!\n", __func__,
				    rpc->parameterSize);
			return;
		}
		rpc->mycontext &= 0xfffffffc;
		break;
	default:
		reply_value = 0;
		pr_err("%s : Unknow cmd 0x%x parameterSize = %d\n", __func__,
			    remote_cmd, rpc->parameterSize);
		break;
	}

	if (true) {
		/*Reply RPC*/
		struct {
			struct rpc_struct header;
			struct reply_fw_parameter reply;
		} __attribute__((aligned(1))) reply_rpc;

		reply_rpc.header.programID = REPLYID;
		reply_rpc.header.versionID = REPLYID;
		reply_rpc.header.procedureID = 0;
		reply_rpc.header.taskID = 0;
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
		reply_rpc.header.sysTID = 0;
#endif /* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
		reply_rpc.header.sysPID = 0;
		reply_rpc.header.parameterSize = (unsigned int) sizeof(struct reply_fw_parameter);
		reply_rpc.header.mycontext = rpc->mycontext;
		reply_rpc.reply.taskID = rpc->taskID;
		reply_rpc.reply.reply_value = reply_value;

		 trace_rtk_rpc_peek_rpc_reply((struct rpc_struct_tp *)&reply_rpc.header,
				(u32)refclk_get_val_raw(), 0, rpc->taskID, false);
		if (big_endian)
			endian_swap_32_write(&reply_rpc, sizeof(reply_rpc));

		while (rpc_mem_write(client, (char *) &reply_rpc, sizeof(reply_rpc)) !=
			    (sizeof(reply_rpc))) {
			pr_err("[%s] rpc_mem_write error...\n", __func__);
			msleep(1);
		}
	}
	//tasklet_schedule(&(user->tasklet));
}

int remote_alloc_thread(void * p)
{
	struct rpc_client *client = (struct rpc_client *)p;
	struct user_rpc *user = &client->user_intr;

	while (1) {
		if (wait_event_interruptible(user->waitQueue,
			    user->remote_alloc_flag || kthread_should_stop())) {
			pr_notice("%s got signal or should stop...\n", current->comm);
			continue;
		}

		if (kthread_should_stop()) {
			pr_notice("%s exit...\n", current->comm);
			break;
		}
		spin_lock_bh(&user->lock);
		user->remote_alloc_flag = 0;
		spin_unlock_bh(&user->lock);
		rpc_ion_handler(client);
	}
	return 0;
}

static void rpc_mem_add(struct rpc_device *rpc_dev,
	    struct rpc_mem_entry *entry)
{
	spin_lock_bh(&rpc_dev->rpc_mem_lock);
	entry->next = rpc_dev->rpc_mem_head;
	rpc_dev->rpc_mem_head = entry;
	rpc_dev->rpc_mem_count++;
	spin_unlock_bh(&rpc_dev->rpc_mem_lock);
}

static struct rpc_mem_entry *rpc_mem_remove(struct rpc_device *rpc_dev,
	    unsigned long phys_addr)
{
	struct rpc_mem_entry *prev = NULL;
	struct rpc_mem_entry *curr = NULL;

	spin_lock_bh(&rpc_dev->rpc_mem_lock);
	curr = rpc_dev->rpc_mem_head;
	while (curr != NULL) {
		if (curr->phys_addr != phys_addr) {
			prev = curr;
			curr = curr->next;
			continue;
		}

		if (prev == NULL) {
			rpc_dev->rpc_mem_head = curr->next;
		} else {
			prev->next = curr->next;
		}
		rpc_dev->rpc_mem_count--;
		spin_unlock_bh(&rpc_dev->rpc_mem_lock);

		return curr;
	}
	spin_unlock_bh(&rpc_dev->rpc_mem_lock);

	return NULL;
}

static int rpc_mem_find_fd(struct rpc_device *rpc_dev, unsigned long phys_addr,
	    unsigned long *offset, unsigned long *size)
{
	int ret_fd = -1;
	spin_lock_bh(&rpc_dev->rpc_mem_lock);
	do {
		struct rpc_mem_entry *curr = rpc_dev->rpc_mem_head;
		while (curr != NULL) {
			if (phys_addr >= curr->phys_addr &&
			    phys_addr < (curr->phys_addr + curr->size)) {

				ret_fd =
				    dma_buf_fd(curr->rpc_dmabuf, O_CLOEXEC);

				if (ret_fd >= 0) {
					struct dma_buf *dmabuf =
					    dma_buf_get(ret_fd);
					if (IS_ERR(dmabuf))
						pr_err
						    ("%s : dma=%p (phy=0x%08lx) fd=%d\n",
						     __func__, dmabuf,
						     phys_addr, ret_fd);
					else
						curr->rpc_dmabuf = dmabuf;
				}

				if (offset)
					*offset = phys_addr - curr->phys_addr;

				if (size)
					*size = curr->size;

				break;
			} else {
				curr = curr->next;
			}
		}
	} while (0);
	spin_unlock_bh(&rpc_dev->rpc_mem_lock);
	return ret_fd;
}

#ifdef CONFIG_COMPAT
struct compat_rpc_mem_fd_data {
	compat_ulong_t phyAddr;
	compat_ulong_t ret_offset;
	compat_ulong_t ret_size;
	compat_int_t ret_fd;
};

#define COMPAT_RPC_MEM_IOC_EXPORT		_IOWR(RPC_MEM_IOC_MAGIC, 0, struct compat_rpc_mem_fd_data)

static int compat_get_rpc_mem_fd_data(
	struct compat_rpc_mem_fd_data __user *data32,
	struct rpc_mem_fd_data __user *data)
{
	compat_ulong_t p;
	compat_ulong_t o;
	compat_ulong_t s;
	compat_int_t f;
	int err;

	err = get_user(p, &data32->phyAddr);
	err |= put_user(p, &data->phyAddr);
	err |= get_user(o, &data32->ret_offset);
	err |= put_user(o, &data->ret_offset);
	err |= get_user(s, &data32->ret_size);
	err |= put_user(s, &data->ret_size);
	err |= get_user(f, &data32->ret_fd);
	err |= put_user(f, &data->ret_fd);

	return err;
}

static int compat_put_rpc_mem_fd_data(
	struct compat_rpc_mem_fd_data __user *data32,
	struct rpc_mem_fd_data __user *data)
{
	compat_ulong_t p;
	compat_ulong_t o;
	compat_ulong_t s;
	compat_int_t f;
	int err;

	err = get_user(p, &data->phyAddr);
	err |= put_user(p, &data32->phyAddr);
	err |= get_user(o, &data->ret_offset);
	err |= put_user(o, &data32->ret_offset);
	err |= get_user(s, &data->ret_size);
	err |= put_user(s, &data32->ret_size);
	err |= get_user(f, &data->ret_fd);
	err |= put_user(f, &data32->ret_fd);

	return err;
}

static long compat_rpc_mem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_RPC_MEM_IOC_EXPORT:
	{
		struct compat_rpc_mem_fd_data __user *data32;
		struct rpc_mem_fd_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_rpc_mem_fd_data(data32, data);
		if (err)
			return err;

		ret = filp->f_op->unlocked_ioctl(filp, RPC_MEM_IOC_EXPORT, (unsigned long)data);
		err = compat_put_rpc_mem_fd_data(data32, data);
		return ret ? ret : err;
	}

	default:
	{
		printk(KERN_ERR "[COMPAT_RPC_MEM] No such IOCTL, cmd is %d\n", cmd);
		return -ENOIOCTLCMD;
	}
	}
}
#endif

static long rpc_mem_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	struct rpc_device *rpc_dev = NULL;
	long ret;

	rpc_dev = get_rpc_dev();
	if (!rpc_dev) {
		pr_err("%s: copy_from_user ERROR!\n", __func__);
	}

	ret = -ENOTTY;
	switch (cmd) {
	case RPC_MEM_IOC_EXPORT:
		{
			struct rpc_mem_fd_data data;
			if (copy_from_user
			    (&data, (void __user *)arg, sizeof(data))) {
				pr_err("%s: copy_from_user ERROR!\n", __func__);
				break;
			}

			data.ret_fd = rpc_mem_find_fd(rpc_dev, data.phyAddr,
				    &data.ret_offset, &data.ret_size);

			if (data.ret_fd < 0) {
				pr_err("%s : ret_fd = %d\n", __func__,
				       data.ret_fd);
				break;
			}

			if (copy_to_user
			    ((void __user *)arg, &data, sizeof(data))) {
				__close_fd(current->files, data.ret_fd);
				pr_err
				    ("%s : copy_to_user failed! (phyAddr=0x%08lx)\n",
				     __func__, data.phyAddr);
				break;
			}
			ret = 0;
			break;
		}
	default:
		pr_err("%s: Unknown ioctl (cmd=0x%08x)\n", __func__, cmd);
		ret = -ENOTTY;
		break;
	}
	return ret;
}

static const struct file_operations rpc_mem_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = rpc_mem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_rpc_mem_ioctl,
#endif
};

int rpc_mem_init(struct rpc_device *rpc_dev)
{
	struct miscdevice *dev;
	int ret;

	spin_lock_init(&rpc_dev->rpc_mem_lock);
	rpc_dev->rpc_mem_head = NULL;
	rpc_dev->rpc_mem_count = 0;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->minor = MISC_DYNAMIC_MINOR;
	dev->name = "rpc_mem";
	dev->fops = &rpc_mem_fops;
	dev->parent = NULL;

	ret = misc_register(dev);
	if (ret) {
		pr_err("rpc_mem: failed to register misc device.\n");
		kfree(dev);
		return ret;
	}

	return 0;
}
