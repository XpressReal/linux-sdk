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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dev_printk.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <uapi/linux/ion.h>

#include <soc/realtek/rtk_refclk.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
//#include <ion_rtk_alloc.h>
#include <soc/realtek/uapi/ion_rtk.h>

#include "rpc.h"
#include "rpc_uapi.h"
#include "rpc_trace.h"

/*
 * dump ring buffer rate limiting:
 * not more than 1 ring buffer dumping every 3s
 */
DEFINE_RATELIMIT_STATE(ring_dump_state, 3 * HZ, 1);

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
	pr_debug("%s check %s read ring is%s empty\n", __func__,
		    (&user->read_record)->name ,ret?"":" NOT");

	return ret;
}

#define need_dispatch(user) \
	(user->currProc == NULL && !ring_empty(user) && rpc_done(user))

static uint32_t rpc_read_header(struct user_rpc *user, char *buf,
	    int datasize)
{
	uint32_t out, newOut;

	out = rpc_ringbuf_get_ringOut(&user->read_record);
	newOut = rpc_ringbuf_reading_data(&user->read_record, out,
		    buf, datasize);

	pr_debug("%s newOut=%x\n", __func__, newOut);
	return newOut;
}

static uint32_t rpc_read_parameter(struct user_rpc *user, char *buf,
	    int datasize)
{
	uint32_t out, newOut;

	out = rpc_ringbuf_get_next_ringOut_by_size(&user->read_record,
		    sizeof(struct rpc_struct));
	newOut = rpc_ringbuf_reading_data(&user->read_record, out,
		    buf, datasize);

	pr_debug("%s newOut=%x\n", __func__, newOut);
	return newOut;
}

uint32_t rpc_skip_parameter(struct user_rpc *user, int datasize)
{
	uint32_t out;

	out = rpc_ringbuf_get_next_ringOut_by_size(&user->read_record,
		    sizeof(struct rpc_struct) + datasize);

	pr_debug("%s Out=%x\n", __func__, out);
	return out;
}

void rpc_ignore(struct user_rpc *user, uint32_t data_size)
{
	uint32_t size;

	size = sizeof(struct rpc_struct) + data_size;
	rpc_ringbuf_update_ringOut_by_size(&user->read_record, size);
}

struct rpc_process *pick_one_proc(struct user_rpc *user)
{
	struct rpc_process *proc = NULL;

	spin_lock_bh(&user->lock);
	if (!list_empty(&user->tasks)) {
		proc = list_first_entry(&user->tasks, struct rpc_process, list);
		pr_debug("%s:%d Pick process:%d\n", user->name, __LINE__, proc->tgid);
	} else {
		pr_debug("%s:%d No available process\n", user->name, __LINE__);
	}
	spin_unlock_bh(&user->lock);
	return proc;
}

#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
struct rpc_process *pick_supported_proc(struct user_rpc *user,
		uint32_t programID)
{
	struct rpc_process *proc;
	struct rpc_handler *handler;

	spin_lock_bh(&user->lock);
	list_for_each_entry(proc, &user->tasks, list) {
		list_for_each_entry(handler, &proc->handlers, list) {
			if (handler->programID == programID) {
				spin_unlock_bh(&user->lock);
				pr_debug("%s:%d pid:%d supports programID:%u\n", __func__,
						__LINE__, proc->tgid, programID);
				return proc;
			}
		}
	}
	spin_unlock_bh(&user->lock);
	pr_debug("%s:%d can't find any process supports programID:%u\n", __func__,
			__LINE__, programID);
	return NULL;
}
#endif	/* CONFIG_REALTEK_RPC_PROGRAM_REGISTER */

struct rpc_process *lookup_by_taskID(struct user_rpc *user,
		uint32_t taskID)
{
	pid_t pid;
	struct rpc_process *proc = NULL;
	struct task_struct *task;

	/* sanity check */
	if (unlikely(taskID >= PID_MAX_DEFAULT)) {
		pr_err("%s:%d invalid taskID:%u >= pid_max:%d\n", user->name,
				__LINE__, taskID, PID_MAX_DEFAULT);
		return NULL;
	}

	task = pid_task(find_pid_ns(taskID, &init_pid_ns), PIDTYPE_PID);
	if (task == NULL) {
		pr_warn("%s:%d can't find_task_by_pid_ns! taskID:%u, thread may be dead\n",
				user->name, __LINE__, taskID);

		//BUG();

		/*
		 * taskID may be dead, use it directly, risky!!
		 * the same taskID may exist in more than one thread list
		 */
		pid = taskID;
	} else {
		pid = task->tgid;
		pr_debug("%s:%d find_task_by_pid_ns taskID:%u => pid:%d tgid:%d comm:%s\n",
				user->name, __LINE__, taskID, task->pid, task->tgid, task->comm);
	}

	spin_lock_bh(&user->lock);
	list_for_each_entry(proc, &user->tasks, list) {
		pr_debug("%s:%d proc->tgid:%d target-pid:%d\n",
				user->name, __LINE__, proc->tgid, pid);

		if (proc->minor_id != user->channel_id_read)
			continue;

		if (proc->tgid == pid) {
			spin_unlock_bh(&user->lock);
			pr_debug("%s:%d %s found process:%d (%px)\n",
				    __func__, __LINE__, user->name, pid, proc);
			return proc;
		} else {
			struct rpc_thread *thread;

			list_for_each_entry(thread, &proc->threads, list) {
				if (thread->pid == pid) {
					spin_unlock_bh(&user->lock);
					pr_debug("%s:%d %s found thread:%d in process:%d\n",
						    __func__, __LINE__,
						    user->name, pid, proc->tgid);
					return proc;
				}
			}
		}
	}
	spin_unlock_bh(&user->lock);
	pr_err("%s:%d taskID:%u never shows in thread list\n",
			user->name, __LINE__, taskID);
	return NULL;
}

/* lock before call this function */
static inline int update_thread_list(struct user_rpc *user, pid_t pid)
{
	struct rpc_process *proc = NULL;
	struct rpc_process *proctmp;
	struct rpc_thread *thread;

	spin_lock_bh(&user->lock);
	list_for_each_entry(proctmp, &user->tasks, list) {
		/* find the entry in read device taik list with the same pid */
		if (proctmp->tgid == pid) {
			proc = proctmp;
			break;
		}
	}
	spin_unlock_bh(&user->lock);

	/* no found, should not happen */
	if (proc == NULL)
		return 0;

	/* current thread is main thread or the thread that open this device */
	if (current->pid == proc->tgid)
		return 0;

	spin_lock_bh(&user->lock);
	list_for_each_entry(thread, &proc->threads, list) {
		/* current is already in thread list */
		if (thread->pid == current->pid) {
			spin_unlock_bh(&user->lock);
			return 0;
		}
	}
	spin_unlock_bh(&user->lock);

	/* not found, so add new thread to list */
	thread = kmalloc(sizeof(struct rpc_thread), GFP_KERNEL);
	if (thread == NULL) {
		pr_err("%s: failed to allocate RPC_THREAD\n", __func__);
		return -ENOMEM;
	}
	pr_debug("%s:%d add thread:%s,%d,%d to thread list of process:%d\n",
			__func__, __LINE__, current->comm, current->pid, current->tgid,
			proc->tgid);
	thread->pid = current->pid;
	spin_lock_bh(&user->lock);
	list_add(&thread->list, &proc->threads);
	spin_unlock_bh(&user->lock);

	return 1;
}

static int check_dead_process(struct user_rpc *user, int pid)
{
	struct list_head *listptr;
	struct rpc_release_process_list *entry;
	struct rpc_release_process_list *release_proc_lists;

	spin_lock_bh(&user->release_proc_lock);
	release_proc_lists = &user->release_proc_lists;
	list_for_each(listptr, &release_proc_lists->list) {
		entry = list_entry(listptr, struct rpc_release_process_list, list);
		if (entry->pid == pid) {
			spin_unlock_bh(&user->release_proc_lock);
			return 0;
		}
	}
	spin_unlock_bh(&user->release_proc_lock);
	return -EINVAL;
}

void handle_dead_process_reply(struct rpc_client *client, struct user_rpc *user,
	    struct rpc_struct rpc)
{
	struct rpc_struct *rrpc;
	uint32_t *tmp;
	char replybuf[sizeof(struct rpc_struct) + 2*sizeof(uint32_t)];
	size_t size;
	bool big_endian = client->big_endian;
	bool from_user = false;

	/* fill Reply RPC */
	rrpc = (struct rpc_struct *)replybuf;
	rrpc->programID = REPLYID;
	rrpc->versionID = REPLYID;
	rrpc->procedureID = 0;
	rrpc->taskID = 0;
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
	rrpc->sysTID = 0;
#endif /* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
	rrpc->sysPID = 0;
	rrpc->parameterSize = 2*sizeof(uint32_t);
	rrpc->mycontext = rpc.mycontext;

	rpc_struct_convert_to_fw(rrpc, big_endian);

	/* fill the parameters... */
	tmp = (uint32_t *)(replybuf + sizeof(struct rpc_struct));
	*(tmp+0) = SCPU2FW(big_endian, rpc.taskID); /* FIXME: should be 64bit */
	*(tmp+1) = SCPU2FW(big_endian, 0xdead);

	size = sizeof(struct rpc_struct) + 2*sizeof(uint32_t);

	if (rpc_ringbuf_write(&user->write_record, (char *) &replybuf, size,
		    from_user) != size) {
		pr_err("[%s] handle_dead_process_reply error...\n", __func__);
		return;
	} else {
		rpc_send_interrupt(client);
	}
}

/*
 * This function may be called by tasklet and rpc_intr_read(),
 * rpc_poll_read()
 */
static void rpc_dispatch(unsigned long data)
{
	struct rpc_client *client = (struct rpc_client *)data;
	struct user_rpc *user = &client->user_intr;
	bool big_endian = client->big_endian;
	struct rpc_process *proc = NULL;
	struct rpc_process *curr;
	uint32_t out;
	int found = 0;
	uint32_t nextRpc = user->nextRpc;
	struct rpc_struct rpc;

	//dump_rpc_ringbuf(&user->read_record);
	dev_dbg(client->dev,
		    "%s %s: run dispatch. (currProc:%px next:%x)\n",
		    __func__, client->name, user->currProc, nextRpc);

	curr = (struct rpc_process *)user->currProc;
	if (curr != NULL || ring_empty(user) || !rpc_done(user)) {
		uint32_t ringOut = rpc_ringbuf_get_ringOut(&user->read_record);

		dev_dbg(client->dev,
			    "%s %s: unable to dispatch rpc. (currProc:%px (%d) Out:%x next:%x "
			    "size:%d %s)\n",
			    __func__, client->name, curr, curr? curr->tgid : 0,
			    ringOut, nextRpc,
			    rpc_ringbuf_get_ring_data_size(&user->read_record),
			    in_atomic() ? "atomic" : "");
		if (curr != NULL && check_dead_process(user, curr->tgid) == 0) {
			dev_dbg(client->dev,
				    "[rpc_dispatch]: process has been closed. ignore RPC.\n");
			if (!rpc_done(user)){
				rpc_ringbuf_set_ringOut(&user->read_record,
					    user->nextRpc);
			}
			spin_lock_bh(&user->lock);
			update_currProc(user, NULL);
			spin_unlock_bh(&user->lock);
			if (need_dispatch(user)) {
				tasklet_schedule(&(user->tasklet));
			}
		}
		return;
	}

	//peek_rpc_struct(client->name, dev);
	out = rpc_read_header(user, (char *) &rpc, sizeof(struct rpc_struct));
	if (out == 0)
		return;

	rpc_struct_convert_from_fw(&rpc, big_endian);
	show_rpc_struct(__func__, &rpc);

	switch (rpc.programID) {
	case R_PROGRAM:
		/* For remote allocate memory */
		dev_dbg(client->dev,
			    "%s: program:%u version:%u procedure:%u taskID:%u sysTID:%u sysPID:%u size:%u context:%x 90k:%u %s\n",
			    __func__, rpc.programID, rpc.versionID,
			    rpc.procedureID, rpc.taskID, rpc.sysTID, rpc.sysPID,
			    rpc.parameterSize, rpc.mycontext,
			    (u32)refclk_get_val_raw(), in_atomic() ? "atomic" : "");

		out = rpc_skip_parameter(user, rpc.parameterSize);

		spin_lock_bh(&user->lock);
		update_nextRpc(user, out);
		update_currProc(user, user->remote_alloc_proc);
		user->remote_alloc_flag = 1;
		wake_up_interruptible(&user->waitQueue);
		spin_unlock_bh(&user->lock);

		return;
	case VIDEO_AGENT:
	case AUDIO_AGENT:
	case VENC_AGENT:
	case HIFI_AGENT: {
		int taskID = 0;

		proc = NULL;
		/* use sysPID directly */
		if (rpc.sysPID > 0 && rpc.sysPID < PID_MAX_DEFAULT) {
			pr_info("lookup by sysPID:%d\n", rpc.sysPID);
			taskID = rpc.sysPID;
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
		/* lookup pid by sysTID */
		} else if (rpc.sysTID > 0 && rpc.sysTID < PID_MAX_DEFAULT) {
			pr_info("lookup by sysTID:%d\n", rpc.sysTID);
			taskID = rpc.sysTID;
		}
#endif /* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
		else {
			dev_err(client->dev,
				    "PID is out of range: sysPID:%d sysTID:%d\n",
				    rpc.sysPID, rpc.sysTID);
			return;
		}
		proc = lookup_by_taskID(user, taskID);
		if (unlikely(proc == NULL)) {
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
			proc = pick_supported_proc(user, rpc.programID);
#else
			proc = pick_one_proc(user);
#endif
		}
		if (proc == NULL || check_dead_process(user, taskID) == 0) {
			dev_err(client->dev,
				     "[%s] cannot find process by pid(%d)",
				     __func__, taskID);

			if (rpc.taskID != 0)
				handle_dead_process_reply(client, user, rpc);
			proc = NULL;
		}
		break;}
	case REPLYID:
		if (rpc.versionID == REPLYID &&
				rpc.parameterSize >= sizeof(uint32_t)) {
			uint32_t taskID;

			if (rpc_read_parameter(user, (char *) &taskID,
					sizeof(uint32_t)) == 0)
				return;

			taskID = FW2SCPU(big_endian, taskID);

			dev_dbg(client->dev, "%s: %s case REPLYID: taskID=%u\n",
				    client->name, __func__, taskID);
			proc = lookup_by_taskID(user, taskID);
			if (proc == NULL || check_dead_process(user, proc->tgid) == 0) {
				dev_err(client->dev,
					    "%s: REPLYID client:%s no process pid=%d\n",
					    __func__, client->name, taskID);
				proc = NULL;
			}
		}
		break;
	default:
		if (in_atomic() && __ratelimit(&ring_dump_state)) {
			dev_err(client->dev,
				    "%s:%d invalid programID:%u!!!\n",
				    __func__, __LINE__, rpc.programID);
			show_rpc_struct(__func__, &rpc);
			dump_rpc_ringbuf(&user->read_record);
		}
		return;
	}

	if (proc) {
		found = 1;
	} else if (__ratelimit(&ring_dump_state)) {
		dev_err(client->dev,
			    "%s:%d can't find process for handling %s programID:%u\n",
			    __func__, __LINE__, client->name, rpc.programID);
		show_rpc_struct(__func__, &rpc);
	}

	out = rpc_skip_parameter(user, rpc.parameterSize);

	spin_lock_bh(&user->lock);
	update_nextRpc(user, out);

	if (found) {
		update_currProc(user, proc);
		wake_up_interruptible(&proc->waitQueue);
		dev_dbg(client->dev,
			    "%s:%d ###Wakeup### proc:%px(%d) currProc=%px and "
			    "update nextRpc:%x for programID:%u\n",
			    __func__, __LINE__, proc, proc ? proc->tgid : 0,
			    user->currProc,
			    user->nextRpc, rpc.programID);
	} else {
		dev_dbg(client->dev,
			    "%s:%d ###no process to wakeup### nextRpc:%x for programID:%u\n",
			    __func__, __LINE__, user->nextRpc, rpc.programID);
		dev_dbg(client->dev, "%s: ignore RPC!!\n", __func__);

		rpc_ignore(user, rpc.parameterSize);
		update_currProc(user, NULL);
		if (need_dispatch(user)) {
			tasklet_schedule(&(user->tasklet));
		}
	}

	spin_unlock_bh(&user->lock);
}

#define AUDIO_ION_FLAG \
		(RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC)

static int add_rpc_process_groups(struct rpc_client *client,
	     struct rpc_process *proc)
{
	struct rpc_process_group *entry, *tmp_entry;
	struct rpc_process_group *process_groups;
	struct dma_buf *dmabuf = NULL;
	struct list_head *listptr;
	//unsigned int ion_alloc_flags;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	int ret = 0;

	spin_lock_bh(&client->process_group_lock);
	process_groups = &client->process_groups;
	list_for_each(listptr, &process_groups->list) {
		tmp_entry = list_entry(listptr, struct rpc_process_group, list);
		if (tmp_entry->tgid == proc->tgid){

			pr_debug("[%s] get tgid=%d rpc_process_group add proc\n",
				    __func__, tmp_entry->tgid);
			list_add(&proc->at_group, &tmp_entry->process_lists);
			spin_unlock_bh(&client->process_group_lock);
			return 0;
		}
	}
	spin_unlock_bh(&client->process_group_lock);

	entry = kmalloc(sizeof(struct rpc_process_group), GFP_KERNEL);
	if (!entry) {
		pr_err("[%s] kmalloc failed\n", __func__);
		ret = -ENOMEM;
		goto malloc_err;
	}

	dmabuf = rheap_alloc("rtk_audio_heap", 4096, AUDIO_ION_FLAG);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(client->dev, "Failed to rheap_alloc\n");
		ret = -ENOMEM;
		goto rheap_err;
	}

	strcpy(entry->comm, current->comm);

	entry->tgid = current->tgid;
	entry->type = client->id;
	entry->rpc_dmabuf = dmabuf;
	attach = dma_buf_attach(entry->rpc_dmabuf, client->dev);
	if (IS_ERR(attach)) {
		dev_err(client->dev, "Failed to attach dmabuf\n");
		ret = PTR_ERR(attach);
		goto attach_err;
	}
	entry->attachment = attach;
	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		dev_err(client->dev, "Failed to map attachment \n");
		ret = PTR_ERR(table);
		goto map_err;
	}
	entry->paddr = sg_phys(table->sgl);
	ret = dma_buf_begin_cpu_access(entry->rpc_dmabuf,
				 DMA_BIDIRECTIONAL);
	if (ret)
		goto access_err;

	entry->vaddr = dma_buf_vmap(entry->rpc_dmabuf);
	if (!entry->vaddr) {
		dev_err(client->dev, "dma_buf_vmap failed\n");
		ret = -ENOMEM;
		goto kmap_err;
	}

	INIT_LIST_HEAD(&entry->list);
	INIT_LIST_HEAD(&entry->process_lists);

	spin_lock_bh(&client->process_group_lock);
	list_add(&entry->list, &process_groups->list);
	list_add(&proc->at_group, &entry->process_lists);
	spin_unlock_bh(&client->process_group_lock);

	return 0;

kmap_err:
	dma_buf_end_cpu_access(entry->rpc_dmabuf, DMA_BIDIRECTIONAL);
access_err:
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(entry->rpc_dmabuf, attach);
attach_err:
	dma_buf_put(entry->rpc_dmabuf);
rheap_err:
	kfree(entry);
malloc_err:

	return ret;
}

static struct rpc_process_group *find_rpc_process_group(struct rpc_client *client,
		    int tgid)
{
	struct list_head *listptr;
	struct rpc_process_group *entry;
	struct rpc_process_group *process_groups;

	spin_lock_bh(&client->process_group_lock);
	process_groups = &client->process_groups;
	list_for_each(listptr, &process_groups->list) {
		entry = list_entry(listptr, struct rpc_process_group, list);
		if (entry->tgid == tgid) {
			spin_unlock_bh(&client->process_group_lock);
			return entry;
		}
	}
	spin_unlock_bh(&client->process_group_lock);
	return NULL;
}

static void remove_rpc_process_group(struct rpc_client *client,
	    struct rpc_process *proc)
{
	struct rpc_process_group *entry;

	entry = find_rpc_process_group(client, proc->tgid);

	spin_lock_bh(&client->process_group_lock);
	list_del(&proc->at_group);
	if (list_empty(&entry->process_lists))
		list_del(&entry->list);
	else
		entry = NULL;
	spin_unlock_bh(&client->process_group_lock);

	if (entry != NULL) {
		struct dma_buf *dmabuf = entry->rpc_dmabuf;
		struct dma_buf_attachment *attach = entry->attachment;

		BUG_ON(!dmabuf);
		BUG_ON(!attach);

		dma_buf_vunmap(dmabuf, entry->vaddr);
		dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		dma_buf_unmap_attachment(attach, attach->sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(dmabuf, attach);

		dma_buf_put(entry->rpc_dmabuf);
		kfree(entry);
	}
}

static long rpc_intr_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct rpc_process *proc = filp->private_data;
	struct rpc_client *client = proc->client;
	struct user_rpc *user = &client->user_intr;
	int ret = 0;
	pid_t g_pid;
	pid_t g_tgid;

	while (user->is_suspend) {
		pr_warn("RPCintr: someone access rpc poll during the suspend!!!...\n");
		msleep(1000);
	}

	switch (cmd) {
	case RPC_IOCTTIMEOUT:
		user->timeout = arg;
		break;
	case RPC_IOCQTIMEOUT:
		return user->timeout;
	case RPC_IOCTEXITLOOP: {
		proc->bExit = true;
		wake_up_interruptible(&proc->waitQueue);
		return 0;
	}
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
	case RPC_IOCTHANDLER: {
		int found;
		struct rpc_process *proc = filp->private_data;
		struct user_rpc *user = proc->user;
		struct rpc_handler *handler;

		pr_debug("%s:%d : Register handler for programID:%lu\n",
				__func__, __LINE__, arg);
		found = 0;
		list_for_each_entry(handler, &proc->handlers, list) {
			if (handler->programID == arg) {
				found = 1;
				break;
			}
		}

		if (found)
			break;

		/* not found, add to handler list */
		handler = kmalloc(sizeof(struct rpc_handler), GFP_KERNEL);
		if (handler == NULL) {
			pr_err("%s: failed to allocate RPC_HANDLER", __func__);
			return -ENOMEM;
		}
		handler->programID = arg;
		spin_lock_bh(&user->lock);
		list_add(&handler->list, &proc->handlers);
		spin_unlock_bh(&user->lock);
		pr_debug("%s:%d %s: Add handler pid:%d for programID:%lu\n",
				__func__, __LINE__, proc->user->name, proc->tgid, arg);
		break;
	}
#endif /* CONFIG_REALTEK_RPC_PROGRAM_REGISTER */
	case RPC_IOC_PROCESS_CONFIG_0: {
		struct S_RPC_IOC_PROCESS_CONFIG_0 config;

		if (copy_from_user(&config, (void __user *)arg, sizeof(struct S_RPC_IOC_PROCESS_CONFIG_0))) {
			pr_err("ERROR! %s cmd:RPC_IOC_PROCESS_CONFIG_0 copy_from_user failed\n", __func__);
			return -ENOMEM;
		}
		if (proc == NULL) {
			pr_err("ERROR! %s cmd:RPC_IOC_PROCESS_CONFIG_0 proc:%p\n", __func__, proc);
			return -ENOMEM;
		}
		proc->bStayActive = (config.bStayActive > 0) ? true : false;
		break;}
	case RPC_IOCTGETGPID:
		g_tgid = task_tgid_nr(current);
		pr_debug("[%s][RPC_IOCTGETGPID]get current global g_tgid:%d \n", __func__, g_tgid);
		if (copy_to_user((int __user *)arg, &g_tgid, sizeof(g_tgid))) {
			pr_err("[RPC_IOCTGETGPID] copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	case RPC_IOCTGETGTGID:
		g_pid = task_pid_nr(current);
		pr_debug("[%s][RPC_IOCTGETGTGID]get current global g_pid:%d \n", __func__, g_pid);
		if (copy_to_user((int __user *)arg, &g_pid, sizeof(g_tgid))) {
			pr_err("[RPC_IOCTGETGTGID] copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	default: /* redundant, as cmd was checked against MAXNR */
		pr_err("%s:%d unsupported ioctl cmd:%x arg:%lx", __func__,
				__LINE__, cmd, arg);
		return -ENOTTY;
	}

	return ret;
}

/*
 * We don't need parameter f_pos here...
 * note:rpc_intr_read support both blocking & nonblocking modes
 */
static ssize_t rpc_intr_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	struct rpc_process *proc = filp->private_data;
	struct rpc_client *client = proc->client;
	struct user_rpc* user = proc->user;
	bool to_user = true;
	ssize_t ret = 0;
	long k;

	pr_debug("%s thread:%s pid:%d tgid:%d device:%s\n",
		    __func__, current->comm, current->pid, current->tgid,
		    client->name);
	pr_debug("%s: %s buf:%px count:%zx\n", __func__, user->name, buf, count);

	WARN_ON(proc->minor_id != user->channel_id_read);

	if (user->is_paused) {
		pr_err("RPCintr: someone access rpc intr during the pause...\n");
		pr_err("%s:%d buf:%p count:%zx EAGAIN\n", user->name, __LINE__, buf,
				count);
		msleep(1000);
		return -EAGAIN;
	}

	if (need_dispatch(user))
		tasklet_schedule(&(user->tasklet));

	pr_debug("%s: name:%s proc:%px currProc:%px \n",
		__func__, user->name, proc, user->currProc);

	if ((user->currProc != proc) || (ring_empty(user))) {
		if (filp->f_flags & O_NONBLOCK)
			goto done;

wait_again:
		pr_debug("%s: wait event tpid=%d (name:%s currProc:%px pid=%d)\n",
			    __func__, current->tgid, user->name, user->currProc,
			    user->currProc?((struct rpc_process *)user->currProc)->tgid:0);
		k = wait_event_interruptible_timeout(proc->waitQueue,
			    (((user->currProc == proc) && (!ring_empty(user))) ||
			    proc->bExit),
			    user->timeout);
		if (k == 0)
			goto done; /* timeout */

		pr_debug("%s: %s get event tgid=%d pid=%d (name:%s currProc:%px pid=%d)\n",
			    __func__, current->comm, current->tgid, current->pid,
			    user->name, user->currProc,
			    user->currProc?((struct rpc_process *)user->currProc)->tgid:0);

		//if (current->flags & PF_FREEZE) {
		//refrigerator(PF_FREEZE);
		//if (!signal_pending(current))
			//goto wait_again;
		//}

		if (try_to_freeze()) {
			if (!signal_pending(current))
				goto wait_again;
		}

		if (signal_pending(current)) {
			pr_debug("%s %s (tgid=%d pid=%d) RPC deblock because of receiving a signal...\n",
				    __func__, current->comm,
				    current->tgid, current->pid);
			goto done;
		}

		if (proc->bExit) {
			pr_info("%s %s (tgid=%d pid=%d) user request to exit\n",
				    __func__, current->comm,
				    current->tgid, current->pid);

			goto done;
		}
	}

	peek_rpc_struct(__func__, &user->read_record, user->channel_id_read);

	ret = rpc_ringbuf_read(&user->read_record, buf, count, to_user);
	/*
	 * NOTE: we do not need spin lock here because we are protected by
	 * semaphore already
	 */
	spin_lock_bh(&user->lock);
	if (rpc_done(user) && user->currProc == proc) {
		pr_debug("%s %s: Previous RPC (proc=%px) is done, unregister myself\n",
			__func__, user->name, proc);
		update_currProc(user, NULL);
	}
	spin_unlock_bh(&user->lock);

	/* process next rpc command if any */
	if (need_dispatch(user))
		tasklet_schedule(&(user->tasklet));

	pr_debug("%s %s: pid:%d tgid:%d count:%zx actual:%zx "
		    "nextRpc:%x currProc:%px(%d)\n",
		    __func__, user->name, current->pid, current->tgid, count, ret,
		    user->nextRpc, user->currProc,
		    user->currProc ? ((struct rpc_process *)user->currProc)->tgid : 0);
done:
	pr_debug("%s %s: pid:%d reads %zx bytes\n",
		    __func__, user->name, current->pid, ret);
	return ret;
}

/*
 * We don't need parameter f_pos here...
 * note: rpc_intr_write only support nonblocking mode
 */
static ssize_t rpc_intr_write(struct file *filp, const char *buf, size_t count,
		loff_t *f_pos)
{
	struct rpc_process *proc = filp->private_data;
	struct rpc_client *client = proc->client;
	struct user_rpc *user = &client->user_intr;
	bool from_user = true;
	ssize_t ret = 0;

	pr_debug("%s thread:%s pid:%d tgid:%d device:%s\n",
		    __func__, current->comm, current->pid, current->tgid,
		    client->name);
	pr_debug("%s: %s buf:%px count:%zx\n", __func__, user->name, buf, count);

	if (user->is_paused) {
		pr_err("RPCintr: someone access rpc intr during the pause...\n");
		pr_err("%s:%d buf:%p count:%zx EAGAIN\n", __func__, __LINE__, buf,
				count);
		msleep(1000);
		return -EAGAIN;
	}

	WARN_ON(proc->minor_id != user->channel_id_write);

#if 1
	/*
	 * Threads that share the same file descriptor should have the same tgid
	 * However, with uClibc pthread library, pthread_create() creates threads
	 * with pid == tgid So the tgid is not real tgid, we have to maintain the
	 * thread list that we can lookup later
	 */
	if (current->pid != proc->tgid)
		update_thread_list(user, proc->tgid);
#endif

	ret = rpc_ringbuf_write(&user->write_record, buf, count, from_user);

	peek_rpc_struct(__func__, &user->write_record, user->channel_id_write);

	/* notify all the processes in the wait queue */
	//wake_up_interruptible(&dev->waitQueue);

	rpc_send_interrupt(client);

	pr_debug("%s:%d thread:%s pid:%d tgid:%d device:%s\n",
			__func__, __LINE__, current->comm, current->pid,
			current->tgid, user->name);
	pr_debug("%s:%d buf:%px count:%zx actual:%zx\n",
			__func__, __LINE__, buf, count, ret);

	pr_debug("%s pid:%d write done (ret=%zx)\n",
			user->name, current->pid, ret);
	return ret;
}

static int rpc_intr_open(struct inode *inode, struct file *filp)
{
	struct user_rpc *user = (struct user_rpc *)inode->i_private;
	int minor = MINOR(inode->i_rdev);
	int ret = 0;

	if (user) {
		struct rpc_client *client= container_of(
			    user, struct rpc_client, user_intr);
		struct rpc_process *proc = kmalloc(sizeof(struct rpc_process),
			    GFP_KERNEL | __GFP_ZERO);

		if (proc == NULL) {
			pr_err("%s: failed to allocate RPC_PROCESS", __func__);
			return -ENOMEM;
		}

		if (minor == user->channel_id_write)
			pr_debug("%s: %s use user intr write channel (id=%d)",
				    __func__, client->name, minor);
		if (minor == user->channel_id_read)
			pr_debug("%s: %s use user intr read channel (id=%d)",
				    __func__, client->name, minor);

		/* current->tgid = process id, current->pid = thread id */
		proc->client = client;
		proc->user = user;
		proc->minor_id = minor;
		proc->tgid = current->tgid;
		proc->pid = current->pid; /* only for record and debug */
		strcpy(proc->name, current->comm);
		proc->bStayActive = false;
		init_waitqueue_head(&proc->waitQueue);
		INIT_LIST_HEAD(&proc->list);
		INIT_LIST_HEAD(&proc->at_group);
		INIT_LIST_HEAD(&proc->threads);
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
		INIT_LIST_HEAD(&proc->handlers);
#endif

		spin_lock_bh(&user->lock);
		list_add(&proc->list, &user->tasks);
		spin_unlock_bh(&user->lock);
		pr_debug("%s: Current process %s pid:%d tgid:%d => proc=%px for %s\n",
			    __func__, current->comm, current->pid, current->tgid,
			    proc, proc->user->name);

		filp->private_data = proc;
		ret = add_rpc_process_groups(client, proc);
		if (ret) {
			pr_err("%s: add_rpc_process_groups failed err:%d\n",
				    __func__, ret);
			return ret;
		}
	}

	return 0; /* success */
}

static int rpc_intr_release(struct inode *inode, struct file *filp)
{
	struct rpc_process *proc;
	struct rpc_client *client;
	struct user_rpc *user;
	int minor = MINOR(inode->i_rdev);
	struct rpc_thread *th, *thtmp;
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
	struct rpc_handler *handler, *hdltmp;
#endif /* CONFIG_REALTEK_RPC_PROGRAM_REGISTER */

	proc = filp->private_data;
	client = proc->client;
	user = proc->user;

	pr_debug("%s: Current process pid:%d tgid:%d => proc=%px (%d) for %s\n",
		    __func__, current->pid, current->tgid, proc, proc->tgid,
		    proc->user->name);

	WARN_ON(proc->minor_id != minor);

	spin_lock_bh(&user->lock);

#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
	/* unregister myself from handler list */
	list_for_each_entry_safe(handler, hdltmp, &proc->handlers, list) {
		list_del(&handler->list);
		kfree(handler);
	}
#endif /* CONFIG_REALTEK_RPC_PROGRAM_REGISTER */

	list_for_each_entry_safe(th, thtmp, &proc->threads, list) {
		list_del(&th->list);
		kfree(th);
	}

	/* remove myself from task list*/
	list_del(&proc->list);
	kfree(proc);

	spin_unlock_bh(&user->lock);

	pr_debug("RPC intr close with minor number: %d\n", minor);

	return 0;
}

static void add_release_process_list(struct user_rpc *user, int pid)
{
	struct rpc_release_process_list *release_proc_lists;
	struct rpc_release_process_list *proc_entry;
	struct rpc_release_process_list *tmp_entry;
	struct list_head *listptr;

	spin_lock_bh(&user->release_proc_lock);
	release_proc_lists = &user->release_proc_lists;
	list_for_each(listptr, &release_proc_lists->list) {
		tmp_entry = list_entry(listptr, struct rpc_release_process_list, list);
		if (tmp_entry->pid == pid) {
			pr_debug("%s pid=%d is added to release_proc_lists\n",
				    __func__, pid);
			spin_unlock_bh(&user->release_proc_lock);
			return;
		}
	}
	spin_unlock_bh(&user->release_proc_lock);

	proc_entry = kmalloc(sizeof(struct rpc_release_process_list), GFP_KERNEL);
	proc_entry->pid = pid;
	proc_entry->cnt = 2;

	spin_lock_bh(&user->release_proc_lock);
	list_add(&proc_entry->list, &release_proc_lists->list);
	spin_unlock_bh(&user->release_proc_lock);
}

static void remove_release_process_list(struct user_rpc *user, int pid)
{
	struct list_head *listptr;
	struct rpc_release_process_list *entry;
	struct rpc_release_process_list *release_proc_lists;

	spin_lock_bh(&user->release_proc_lock);
	release_proc_lists = &user->release_proc_lists;
	list_for_each(listptr, &release_proc_lists->list) {
		entry = list_entry(listptr, struct rpc_release_process_list, list);
		if (entry->pid == pid) {
			entry->cnt--;
			if (entry->cnt == 0) {
				list_del(listptr);
				kfree(entry);
			}
			spin_unlock_bh(&user->release_proc_lock);
			return;
		}
	}
	spin_unlock_bh(&user->release_proc_lock);
}

static int rpc_intr_flush(struct file *filp, fl_owner_t id)
{
	struct rpc_process_group *process_group_entry = NULL;
	//int minor = MINOR(filp->f_inode->i_rdev);
	struct rpc_process *proc;
	struct rpc_client *client;
	struct user_rpc *user;
	struct task_struct *task;

	if (file_count(filp) > 1)
		return 0;

	proc = filp->private_data;
	client = proc->client;
	user = proc->user;

	pr_debug("%s: Current process pid:%d tgid:%d => proc=%px (%d) for %s\n",
		    __func__, current->pid, current->tgid, proc, proc->tgid,
		    proc->user->name);

	add_release_process_list(user, proc->tgid);

	if (user->currProc == proc) {
		pr_debug("%s: proc=%px clear %s current process (pid=%d)\n", __func__,
				proc, proc->client->name, proc->tgid);
		update_currProc(user, NULL);
		 /* intr read device (ugly code)*/
		if (!rpc_done(user)) {
			pr_debug("%s: previous rpc hasn't finished, force clear!! ringOut=0x%x\n",
				    __func__, user->nextRpc);
			rpc_ringbuf_set_ringOut(&user->read_record, user->nextRpc);
		}
	}

	process_group_entry = find_rpc_process_group(client, proc->tgid);

	if (user->is_paused) {
		pr_err("rpc is paused, no self destroy: %d\n", proc->tgid);
	} else if (proc->bStayActive) {
		pr_err("bStayActive is true, no self destroy: %d\n", proc->tgid);
	} else {
		if (!(IS_ENABLED(CONFIG_RTK_VO_ON_HIFI) && client->id == RPC_AUDIO)) {
			task = pid_task(find_pid_ns(proc->tgid, &init_pid_ns), PIDTYPE_PID);
			if (task != NULL && (task->flags & PF_SIGNALED))
				task = NULL;

			if (process_group_entry != NULL && task == NULL) {
				pr_err("self destroy in flush: tgid=%d client->name:%s\n",
					    proc->tgid, client->name);

				rpc_notify_fw_destroy_process(client,
					    proc->tgid, process_group_entry->paddr,
					    process_group_entry->vaddr);
			}
		}
	}

	remove_rpc_process_group(client, proc);
	remove_release_process_list(user, proc->tgid);

	return 0;
}

struct file_operations rpc_intr_fops = {
	//.llseek = scull_llseek,
	.unlocked_ioctl = rpc_intr_ioctl,
	.compat_ioctl = rpc_intr_ioctl,
	.read = rpc_intr_read,
	.write = rpc_intr_write,
	.open = rpc_intr_open,
	.release = rpc_intr_release,
	.flush = rpc_intr_flush,
};

int rpc_client_user_intr_init(struct rpc_device *rpc_dev,
	     struct rpc_client *client)
{
	struct user_rpc *user = &client->user_intr;
	struct rpc_record_mapping *record;
	struct device *dev;
	struct device_node *node;
	int id;
	int result = 0;

	client->is_support_intr = true;

	dev = client->dev;
	node = dev->of_node;
	if (WARN_ON(!node)) {
		dev_err(dev, "can not found device node\n");
		return -ENODATA;
	}

	result = of_property_read_u32(node, "intr_channel_id_write",
			    &user->channel_id_write);
	if (result) {
		user->channel_id_write = -1;
	}

	result = of_property_read_u32(node, "intr_channel_id_read",
			    &user->channel_id_read);
	if (result) {
		user->channel_id_read = -1;
	}

	record = &user->read_record;
	user->nextRpc = *record->ringOut;
	user->currProc = NULL;

	snprintf(user->name, 32, "%s-user-intr", client->name);
	/* Initialize wait queue... for record */
	//init_waitqueue_head(&(user->waitQueue));

	/* Initialize sempahores... */
	init_rwsem(&user->write_record.Sem);
	init_rwsem(&user->read_record.Sem);

	INIT_LIST_HEAD(&user->tasks);
	tasklet_init(&user->tasklet, rpc_dispatch, (unsigned long) client);
	spin_lock_init(&user->lock);

	/* for remote alloc thread */
	user->remote_alloc_flag = 0;
	init_waitqueue_head(&user->waitQueue);
	user->remote_alloc_kthread = kthread_run(remote_alloc_thread,
		    (void *)client, "%s_remote_alloc_thread", client->name);
	user->remote_alloc_proc = kmalloc(sizeof(struct rpc_process), GFP_KERNEL | __GFP_ZERO);
	user->remote_alloc_proc->tgid = 0;

	//is_init = 1;
	user->is_paused = 0;
	user->is_suspend = 0;

	INIT_LIST_HEAD(&user->release_proc_lists.list);
	spin_lock_init(&user->release_proc_lock);

	user->f_ops = &rpc_intr_fops;

	id = user->channel_id_write;
	if (id >= 0) {
		dev = device_create(rpc_dev->rpc_class,
			    NULL, MKDEV(rpc_dev->rpc_major, id), user,
			    "rpc%d", id);
		dev->coherent_dma_mask = DMA_BIT_MASK(32);
		dev->dma_mask = (u64 *)&dev->coherent_dma_mask;
		user->dev_channel_write = dev;
	}

	id = user->channel_id_read;
	if (id >= 0) {
		dev = device_create(rpc_dev->rpc_class,
			    NULL, MKDEV(rpc_dev->rpc_major, id), user,
			    "rpc%d", id);
		dev->coherent_dma_mask = DMA_BIT_MASK(32);
		dev->dma_mask = (u64 *)&dev->coherent_dma_mask;
		user->dev_channel_read = dev;
	}

	user->timeout = HZ;

	//dev_info(rpc_dev, "\033[31mrpc is not paused & suspended\033[m\n");

	return result;
}

