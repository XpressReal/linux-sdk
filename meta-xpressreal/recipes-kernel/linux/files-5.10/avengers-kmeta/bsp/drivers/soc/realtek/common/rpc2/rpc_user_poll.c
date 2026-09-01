/*
 * Realtek RPC driver
 *
 * Copyright (c) 2017 Realtek Semiconductor Corp.
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
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/fdtable.h>
#include <linux/kmemleak.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/syscalls.h>
#include <linux/ion.h>
#include <uapi/linux/ion.h>

#include <soc/realtek/uapi/ion_rtk.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_refclk.h>

#include "rpc.h"
#include "rpc_uapi.h"

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

int rpc_poll_pause(struct user_rpc *user)
{
	user->is_paused = 1;
	return 0;
}

int rpc_poll_suspend(struct user_rpc *user)
{
	user->is_suspend = 1;
	return 0;
}

int rpc_poll_resume(struct user_rpc *user)
{
	user->is_suspend = 0;
	return 0;
}

int rpc_poll_open(struct inode *inode, struct file *filp)
{
	struct user_rpc *user = (struct user_rpc *)inode->i_private;
	int minor = MINOR(inode->i_rdev);

	pr_debug("RPC poll open with minor number: %d\n", minor);

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
		proc->bStayActive = false;

		init_waitqueue_head(&proc->waitQueue);
		INIT_LIST_HEAD(&proc->threads);
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
		INIT_LIST_HEAD(&proc->handlers);
#endif
		spin_lock_bh(&proc->user->lock);
		list_add(&proc->list, &proc->user->tasks);
		spin_unlock_bh(&proc->user->lock);
		pr_debug("%s: Current process pid:%d tgid:%d => %d(%p) for %s\n",
			     __func__, current->pid, current->tgid, proc->tgid,
			      &proc->waitQueue, proc->user->name);

		filp->private_data = proc;
	}

	//MOD_INC_USE_COUNT; /* Before we maybe sleep */

	return 0;
}

int rpc_poll_release(struct inode *inode, struct file *filp)
{
	struct rpc_process *proc;
	struct rpc_client *client;
	struct user_rpc *user;
	struct rpc_thread *th, *thtmp;
	int minor = MINOR(inode->i_rdev);
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
	struct rpc_handler *handler, *hdltmp;
#endif

	proc = filp->private_data;
	client = proc->client;
	user = proc->user;

	WARN_ON(proc->minor_id != minor);

	if (user->currProc == proc) {
		pr_debug("%s: clear %s current process\n", __func__,
			    proc->user->name);
		update_currProc(user, NULL);
		if (!rpc_done(user)) {
			pr_debug("%s: previous rpc hasn't finished, force clear!!\n",
				    __func__);
			rpc_ringbuf_set_ringOut(&user->read_record, user->nextRpc);
		}
	}

	spin_lock_bh(&user->lock);

#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
	//unregister myself from handler list
	list_for_each_entry_safe(handler, hdltmp, &proc->handlers, list) {
		list_del(&handler->list);
		kfree(handler);
	}
#endif

	list_for_each_entry_safe(th, thtmp, &proc->threads, list) {
		list_del(&th->list);
		kfree(th);
	}

	/* remove myself from task list */
	list_del(&proc->list);
	kfree(proc);

	spin_unlock_bh(&user->lock);

	pr_debug("RPC poll close with minor number: %d\n", minor);

//	MOD_DEC_USE_COUNT;

	return 0;
}

/* We don't need parameter f_pos here... */
ssize_t rpc_poll_read(struct file *filp, char *buf, size_t count,
	loff_t *f_pos)
{
	struct rpc_process *proc = filp->private_data;
	struct rpc_client *client = proc->client;
	struct user_rpc* user = proc->user;
	bool to_user = true;
	ssize_t ret = 0;

	pr_debug("%s thread:%s pid:%d tgid:%d device:%s\n",
		    __func__, current->comm, current->pid, current->tgid,
		    client->name);

	WARN_ON(proc->minor_id != user->channel_id_read);

	if (user->is_paused) {
		pr_err("RPCpoll: someone access rpc poll during the pause...\n");
		pr_err("%s:%d buf:%p count:%zx EAGAIN\n", __func__, __LINE__, buf, count);
		msleep(1000);
		return -EAGAIN;
	}

	while (user->is_suspend) {
		pr_warn("RPCpoll: someone access rpc poll during the suspend!!!...\n");
		msleep(1000);
	}

	if (need_dispatch(user))
		tasklet_schedule(&(user->tasklet));

	pr_debug("%s: dev:%s(%p) currProc:%p\n", __func__,
		    user->name, client->dev, user->currProc);
	if ((user->currProc != proc) || (ring_empty(user))) {
		if (unlikely(!(filp->f_flags & O_NONBLOCK))) {
			//pr_warn("%s:%d:%s Warning: pid:%d use blocking mode with poll buffer!\n", __func__, __LINE__, user->name, current->pid);
		}
		goto out; //return anyway
	}

	ret = rpc_ringbuf_read(&user->read_record, buf, count, to_user);

	spin_lock_bh(&user->lock);
	if (rpc_done(user)) {
		pr_debug("%s: Previous RPC is done, unregister myself\n", __func__);
		update_currProc(user, NULL);
	}
	spin_unlock_bh(&user->lock);

	//process next rpc command if any
	if (need_dispatch(user))
		tasklet_schedule(&(user->tasklet));

	pr_debug("%s:%d buf:%p count:%zx actual:%zx\n", __func__, __LINE__,
		    buf, count, ret);

out:
	return ret;
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

/* We don't need parameter f_pos here... */
ssize_t rpc_poll_write(struct file *filp, const char *buf, size_t count,
	loff_t *f_pos)
{
	struct rpc_process *proc = filp->private_data;
	struct rpc_client *client = proc->client;
	struct user_rpc *user = &client->user_intr;
	bool from_user = true;
	ssize_t ret = 0;

	if (user->is_paused) {
		pr_warn("RPCpoll: someone access rpc poll during the pause...\n");
		msleep(1000);
		return -EAGAIN;
	}

	while (user->is_suspend) {
		pr_warn("RPCpoll: someone access rpc poll during the suspend!!!...\n");
		msleep(1000);
	}

#if 1
	/* Threads that share the same file descriptor should have the same tgid
	 * However, with uClibc pthread library, pthread_create() creates threads with pid == tgid
	 * So the tgid is not real tgid, we have to maintain the thread list that we can lookup later
	 */
	if (current->pid != proc->tgid)
		update_thread_list(user, proc->tgid);
#endif
	ret = rpc_ringbuf_write(&user->write_record, buf, count, from_user);

	//peek_rpc_struct(user->name, dev, f_pos);

	return ret;
}

long rpc_poll_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct rpc_process *proc = filp->private_data;
	struct rpc_client *client = proc->client;
	struct user_rpc *user = &client->user_intr;
	int ret = 0;
	int found;
	struct rpc_handler *handler;

	while (user->is_suspend) {
		pr_warn("RPCpoll: someone access rpc poll during the suspend!!!...\n");
		msleep(1000);
	}

	switch (cmd) {
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
	case RPC_IOCTHANDLER:
		pr_debug("%s:%d : Register handler for programID:%lu\n", __func__, __LINE__, arg);
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
			pr_err("%s: failed to allocate RPC_HANDLER\n", __func__);
			return -ENOMEM;
		}

		handler->programID = arg;
		spin_lock_bh(&user->lock);
		list_add(&handler->list, &proc->handlers);
		spin_unlock_bh(&user->lock);
		pr_debug("%s:%d %s: Add handler pid:%d for programID:%lu\n", __func__, __LINE__, proc->user->name, proc->tgid, arg);
		break;
#endif
	case RPC_IOC_PROCESS_CONFIG_0:
	{
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
		break;
	}
	default:
		pr_warn("%s:%d unsupported ioctl cmd:%x arg:%lx\n", __func__, __LINE__, cmd, arg);
		return -ENOTTY;
	}

	return ret;
}

struct file_operations rpc_poll_fops = {
	//llseek:scull_llseek,
	.unlocked_ioctl = rpc_poll_ioctl,
	.compat_ioctl = rpc_poll_ioctl,
	.read = rpc_poll_read,
	.write = rpc_poll_write,
	.open = rpc_poll_open,
	.release = rpc_poll_release,
};

int rpc_client_user_poll_init(struct rpc_device *rpc_dev,
	    struct rpc_client *client)
{
	struct user_rpc *user = &client->user_poll;
	struct rpc_record_mapping *record;
	struct device *dev;
	struct device_node *node;
	int id;
	int result = 0;

	client->is_support_poll = true;

	dev = client->dev;
	node = dev->of_node;
	if (WARN_ON(!node)) {
		dev_err(dev, "can not found device node\n");
		return -ENODATA;
	}

	result = of_property_read_u32(node, "poll_channel_id_write",
			    &user->channel_id_write);
	if (result) {
		user->channel_id_write = -1;
	}

	result = of_property_read_u32(node, "poll_channel_id_read",
			    &user->channel_id_read);
	if (result) {
		user->channel_id_read = -1;
	}

	record = &user->read_record;
	user->nextRpc = *record->ringOut;
	user->currProc = NULL;
	snprintf(user->name, 32, "%s-user-poll", client->name);

	/* Initialize wait queue... */
	//init_waitqueue_head(&(user->waitQueue));

	/* Initialize sempahores... */
	init_rwsem(&user->write_record.Sem);
	init_rwsem(&user->read_record.Sem);

	INIT_LIST_HEAD(&user->tasks);
	spin_lock_init(&user->lock);

	user->is_paused = 0;
	user->is_suspend = 0;

	INIT_LIST_HEAD(&user->release_proc_lists.list);
	spin_lock_init(&user->release_proc_lock);

	user->f_ops = &rpc_poll_fops;

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

	return result;
}


MODULE_LICENSE("GPL v2");
