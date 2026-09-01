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

#include <linux/module.h>
#include <linux/kernel.h>
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
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>

#include <soc/realtek/rtk_refclk.h>
#include <trace/events/rtk_rpc.h>
#include "rpc.h"
#include "rpc_trace.h"

#define TIMEOUT (5*HZ)

int rpc_kern_pause(struct rpc_client *client)
{
	struct kern_rpc *kern;

	kern = &client->kern;
	kern->is_paused = 1;
	return 0;
}

int rpc_kern_suspend(struct rpc_client *client)
{
	struct kern_rpc *kern;

	kern = &client->kern;
	kern->is_suspend = 1;
	return 0;
}

int rpc_kern_resume(struct rpc_client* client)
{
	struct kern_rpc *kern;

	kern = &client->kern;
	kern->is_suspend = 0;
	return 0;
}

bool rpc_kern_ring_empty(struct rpc_client *client)
{
	struct kern_rpc *kern;

	dev_dbg(client->dev, "%s %s check ring if empty\n", __func__, client->name);
	kern = &client->kern;

	return rpc_ringbuf_empty(&kern->read_record);
}

ssize_t rpc_kern_read(struct rpc_client *client, char *buf, size_t count)
{
	struct kern_rpc *kern;
	ssize_t ret = 0;
	bool to_user = false;

	dev_dbg(client->dev, "%s read to buf@%p, count=%zx\n", __func__, buf, count);
	kern = &client->kern;
	ret = rpc_ringbuf_read(&kern->read_record, buf, count, to_user);

	return ret;
}

ssize_t rpc_kern_write(struct rpc_client *client, const char *buf, size_t count)
{
	struct kern_rpc *kern;
	ssize_t ret = 0;
	bool from_user = false;

	dev_dbg(client->dev, "%s write to buf@%px, count=%zx\n", __func__, buf, count);
	kern = &client->kern;
	ret = rpc_ringbuf_write(&kern->write_record, buf, count, from_user);

	rpc_send_interrupt(client);

	return ret;
}


static uint32_t handle_command(uint32_t command, uint32_t param1, uint32_t param2)
{
	int ret = 0;
/* TODO */
#if 0
	FUNC_PTR ptr;

	pr_info("Handle command %x, param1: %x, param2: %x...\n",
			command, param1, param2);
	down_write(&kern_rpc_sem);
	ptr = radix_tree_lookup(&kern_rpc_tree, command);
	if (ptr)
		ret = ptr(param1, param2);
	else
		pr_err("RPC: lookup kernel rpc %d error...\n", command);
	up_write(&kern_rpc_sem);
#endif
	return ret;
}

static int rpc_kernel_thread(void *p)
{
	struct rpc_client *client = (struct rpc_client *)p;
	struct kern_rpc *kern;
	char readbuf[sizeof(struct rpc_struct) + 3*sizeof(uint32_t)];
	struct rpc_struct *rpc;
	size_t rpc_struct_size = sizeof(struct rpc_struct);
	uint32_t *tmp;
	bool big_endian;

	kern = &client->kern;
	big_endian = client->big_endian;

	while (1) {
		dev_dbg(client->dev, "#@# wait %s %s\n", current->comm, client->name);
		if (wait_event_interruptible(kern->waitQueue,
			    !rpc_kern_ring_empty(client) ||
			        kthread_should_stop())) {
			pr_notice("%s got signal or should stop...\n", current->comm);
			continue;
		}
		dev_dbg(client->dev, " #@# wakeup %s \n", current->comm);

		if (kthread_should_stop()) {
			pr_notice("%s exit...\n", current->comm);
			break;
		}

		/* read the reply data... */
		if (rpc_kern_read(client, readbuf, rpc_struct_size) !=
				rpc_struct_size) {
			pr_err("ERROR in read %s kernel RPC...\n", client->name);
			continue;
		}

		rpc = (struct rpc_struct *)readbuf;
		rpc_struct_convert_from_fw(rpc, big_endian);
		tmp = (uint32_t *)(readbuf + rpc_struct_size);
		if (rpc->taskID) {
			/* handle the request... */
			char replybuf[sizeof(struct rpc_struct) + 2*sizeof(uint32_t)];
			uint32_t ret;
			struct rpc_struct *rrpc = (struct rpc_struct *)replybuf;

			/* read the payload... */
			if (rpc_kern_read(client, readbuf + rpc_struct_size,
					3*sizeof(uint32_t)) != 3*sizeof(uint32_t)) {
				pr_err("ERROR in read payload...\n");
				continue;
			}
			ret = handle_command(FW2SCPU(big_endian,*tmp),
				    FW2SCPU(big_endian,*(tmp+1)),
				    FW2SCPU(big_endian,*(tmp+2)));

			/* (1) fill the rpc_struct ... */
			rrpc->programID = REPLYID;
			rrpc->versionID = REPLYID;
			rrpc->procedureID = 0;
			rrpc->taskID = 0;
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
			rrpc->sysTID = 0;
#endif /* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
			rrpc->sysPID = 0;
			rrpc->parameterSize = 2*sizeof(uint32_t);
			rrpc->mycontext = rpc->mycontext;

			rpc_struct_convert_to_fw(rrpc, big_endian);

			/* (2) fill the parameters... */
			tmp = (uint32_t *)(replybuf + rpc_struct_size);
			*(tmp+0) = SCPU2FW(big_endian, rpc->taskID); /* FIXME: should be 64bit */
			*(tmp+1) = SCPU2FW(big_endian, ret);

			if (rpc_kern_write(client, replybuf, sizeof(replybuf)) !=
					sizeof(replybuf)) {
				pr_err("ERROR in send kernel RPC...\n");
				return RPC_FAIL;
			}
		} else {
			/* read the payload... */
			if (rpc_kern_read(client, readbuf + rpc_struct_size,
					2*sizeof(uint32_t)) != 2*sizeof(uint32_t)) {
				pr_err("ERROR in read payload...\n");
				continue;
			}

			/* parse the reply data... */
#if 0
			/* FIXME: mycontext should be 64bit */
			*((uint32_t *)ntohl(rpc->mycontext)) = ntohl(*(tmp+1));
#else
			*(kern->rpc_retval) = FW2SCPU(big_endian, *(tmp+1));
#endif
			//pr_info("tmp %x opt %d\n", ntohl(*tmp), opt);
			kern->complete_condition = 1;
#if 0
			/* FIXME: wait_queue_head_t * should be 64bit */
			//wake_up((wait_queue_head_t *)ntohl(*tmp)); /* ack the sync... */
#else
			wake_up(&(kern->rpc_wq)); // ack the sync...
#endif
		}
	}

	return 0;
}

static int dump_client_rpc(struct rpc_client *client)
{
	struct kern_rpc *kern;
	struct user_rpc *user;
	int ret;

	kern = &client->kern;
	dev_err(client->dev, "Dump ring buf for %s kernel rpc\n", client->name);
	ret = dump_rpc_ringbuf(&kern->write_record);
	ret = dump_rpc_ringbuf(&kern->read_record);

	user = &client->user_intr;
	dev_err(client->dev, "Dump ring buf for %s user rpc\n", client->name);
	ret = dump_rpc_ringbuf(&user->write_record);
	ret = dump_rpc_ringbuf(&user->read_record);
	

	return ret;
}

int send_rpc_command(int opt, uint32_t command, uint32_t param1,
		uint32_t param2, uint32_t *retvalue)
{
	struct rpc_client *client;
	struct kern_rpc  *kern;
	char sendbuf[sizeof(struct rpc_struct) + 3*sizeof(uint32_t)];
	struct rpc_struct *rpc = (struct rpc_struct *)sendbuf;
	uint32_t *tmp;
	bool big_endian;

	client = rpc_client_get(opt);
	if (!client) {
		pr_err("RPCkern: no rpc client for opt=%d \n", opt);
		return -ENODEV;
	}

	kern = &client->kern;
	big_endian = client->big_endian;

	if (kern->is_paused) {
		pr_warn("RPCkern: someone access rpc kern during the pause...\n");
		return RPC_FAIL;
	}

	mutex_lock(&(kern->rpc_kern_lock));

	while (kern->is_suspend) {
		pr_warn("RPCkern: someone access rpc poll during the suspend!!!...\n");
		msleep(1000);
	}

	if (kern->rpc_kthread == 0) {
		pr_warn("RPCkern: %s is disabled...\n", kern->rpc_kthread->comm);
		mutex_unlock(&(kern->rpc_kern_lock));
		return RPC_FAIL;
	}

	pr_debug(" #@# sendbuf: %zx cmd %x param1 %x param2 %x\n",
			sizeof(sendbuf), command, param1, param2);

	rpc->programID = KERNELID;
	rpc->versionID = KERNELID;
	rpc->procedureID = 0;
	//rpc->taskID = 0;
	rpc->taskID = current->pid;// 0;

#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
	rpc->sysTID = 0;
#endif /* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
	rpc->sysPID = 0;
	rpc->parameterSize = 3*sizeof(uint32_t);
	rpc->mycontext = 0;

	rpc_struct_convert_to_fw(rpc, big_endian);

	kern->rpc_retval = retvalue;

	/* fill the parameters... */
	tmp = (uint32_t *)(sendbuf + sizeof(struct rpc_struct));
	*tmp = SCPU2FW(big_endian, command);
	*(tmp+1) = SCPU2FW(big_endian, param1);
	*(tmp+2) = SCPU2FW(big_endian, param2);

	trace_rtk_rpc_peek_rpc_request((struct rpc_struct_tp *)rpc,
		    (u32)refclk_get_val_raw(), 0, rpc->taskID, big_endian);

	kern->complete_condition = 0;
	if (rpc_kern_write(client, sendbuf, sizeof(sendbuf)) != sizeof(sendbuf)) {
		pr_err("ERROR in send kernel RPC...\n");
		mutex_unlock(&(kern->rpc_kern_lock));
		return RPC_FAIL;
	}
	/* wait the result... */
	//if (!sleep_on_timeout(&rpc_wq[opt], TIMEOUT)) {
	if (!wait_event_timeout(kern->rpc_wq, kern->complete_condition, TIMEOUT)) {
		pr_err("kernel rpc timeout -> disable %s...\n", kern->rpc_kthread->comm);
		WARN(1, " #@# sendbuf: size%zx cmd:%x param1:%x param2:%x\n",
				sizeof(sendbuf), command, param1, param2);
		dump_client_rpc(client);

		kthread_stop(kern->rpc_kthread);
		kern->rpc_kthread = 0;
		mutex_unlock(&(kern->rpc_kern_lock));
		return RPC_FAIL;
	} else {
		pr_debug(" #@# ret: %d \n", *(retvalue));

		mutex_unlock(&(kern->rpc_kern_lock));
		return RPC_OK;
	}
}
EXPORT_SYMBOL(send_rpc_command);

int send_rpc_command_with_pid(int opt, int pid, uint32_t command, uint32_t param1,
			      uint32_t param2, uint32_t *retvalue)
{
	/*TODO*/
	return -EINVAL;
}
EXPORT_SYMBOL(send_rpc_command_with_pid);

int register_kernel_rpc(char *name, int opt, void *cb, void *data)
{
	/*TODO*/
	return -EINVAL;
}
EXPORT_SYMBOL(register_kernel_rpc);

void unregister_kernel_rpc(int opt, int id)
{
	/*TODO*/
	return;
}
EXPORT_SYMBOL(unregister_kernel_rpc);


bool is_rpc_available(int opt)
{
	struct rpc_client *client;

	client = rpc_client_get(opt);
	if (!client) {
		pr_err("RPCkern: no rpc client for opt=%d \n", opt);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(is_rpc_available);


int rpc_client_kern_init(struct rpc_client* client)
{
	int result = 0;
	struct kern_rpc *kern;

	kern = &client->kern;

	snprintf(kern->name, 32, "%s-kern", client->name);
	/* Initialize wait queue... */
	init_waitqueue_head(&(kern->waitQueue));

	/* Initialize sempahores... */
	init_rwsem(&kern->write_record.Sem);
	init_rwsem(&kern->read_record.Sem);

	kern->rpc_kthread = kthread_run(rpc_kernel_thread, (void *)client,
		    "rpc-kern-%s", client->name);

	init_waitqueue_head(&(kern->rpc_wq));
	mutex_init(&kern->rpc_kern_lock);

	kern->is_paused = 0;
	kern->is_suspend = 0;

	return result;
}

MODULE_LICENSE("GPL v2");
