// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek RPC driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */

#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <soc/realtek/rtk_refclk.h>

#include "rpc.h"

#ifdef CONFIG_DEBUG_FS
static int rpc_debug_version_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "rpc driver v2\n");

	return 0;
}

static int rpc_debug_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, rpc_debug_version_show, inode->i_private);
}

static const struct file_operations rpc_debug_version_ops = {
	.open =		rpc_debug_version_open,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

static int rpc_debug_remote_mem_show(struct seq_file *s, void *unused)
{
	struct rpc_device *rpc_dev = s->private;
	struct rpc_mem_entry *curr;
	int i = 0;

	spin_lock_bh(&rpc_dev->rpc_mem_lock);

	seq_printf(s, "Dump remote allocate memory: count=%d\n",
		    rpc_dev->rpc_mem_count);
	curr = rpc_dev->rpc_mem_head;

	while (curr != NULL) {
		seq_printf(s, "#%d entry@%px\n", i++, curr);
		seq_printf(s, "    phys_addr=0x%lx\n", curr->phys_addr);
		seq_printf(s, "    size=0x%lx\n", curr->size);
		seq_printf(s, "    dma_buf@%px\n", curr->rpc_dmabuf);
		seq_printf(s, "    next@%px\n", curr->next);

		curr = curr->next;
	}
	spin_unlock_bh(&rpc_dev->rpc_mem_lock);

	return 0;
}

static int rpc_debug_remote_mem_open(struct inode *inode, struct file *file)
{
	return single_open(file, rpc_debug_remote_mem_show, inode->i_private);
}

static const struct file_operations rpc_debug_remote_mem_ops = {
	.open =		rpc_debug_remote_mem_open,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

static int rpc_debug_status_show(struct seq_file *s, void *unused)
{
	struct rpc_client *client = s->private;
	struct rpc_process_group *tmp_entry;
	struct rpc_process_group *process_groups;
	struct list_head *listptr;

	seq_printf(s, "Client name: %s\n", client->name);
	seq_printf(s, "    id: %d\n", client->id);
	seq_printf(s, "    big_endian: %s\n", client->big_endian?"YES":"NO");
	seq_printf(s, "    ringbuf_paddr2vaddr_offset: 0x%llx\n",
		    client->ringbuf_paddr2vaddr_offset);
	seq_printf(s, "    is_support_poll: %s\n", client->is_support_poll?"YES":"NO");
	seq_printf(s, "    is_support_intr: %s\n", client->is_support_intr?"YES":"NO");

	seq_printf(s, "All user rpc process:\n");
	spin_lock_bh(&client->process_group_lock);

	process_groups = &client->process_groups;
	list_for_each(listptr, &process_groups->list) {
		struct rpc_process *tmp_proc;
		struct list_head *listptr2;
		int i = 0;

		tmp_entry = list_entry(listptr, struct rpc_process_group, list);
		seq_printf(s, "Process Group: %s (tgid=%d, type=%d)\n",
			    tmp_entry->comm,  tmp_entry->tgid, tmp_entry->type);

		list_for_each(listptr2, &tmp_entry->process_lists) {
			tmp_proc = list_entry(listptr2, struct rpc_process, at_group);

			seq_printf(s, "    #%d process: %s tgid=%d pid=%d\n",
				    i++, tmp_proc->name,  tmp_proc->tgid, tmp_proc->pid);
			seq_printf(s, "        minor_id: %d\n", tmp_proc->minor_id);
			seq_printf(s, "        bStayActive: %s\n",
				    tmp_proc->bStayActive?"true":"false");
			seq_printf(s, "        bExit: %s\n",
				    tmp_proc->bExit?"true":"false");
		}
	}
	spin_unlock_bh(&client->process_group_lock);

	return 0;
}

static int rpc_debug_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, rpc_debug_status_show, inode->i_private);
}

static const struct file_operations rpc_debug_status_ops = {
	.open =		rpc_debug_status_open,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

static void dump_ringbuf(struct seq_file *s, struct rpc_record_mapping *record)
{
	volatile uint32_t *ringStart, *ringEnd, *ringIn, *ringOut;
	bool big_endian = record->big_endian;
	uint64_t pa2va_offset = record->pa2va_offset;
	int i;

	down_write(&record->Sem);
	ringStart = record->ringStart;
	ringEnd = record->ringEnd;
	ringIn = record->ringIn;
	ringOut = record->ringOut;

	seq_printf(s, "RingStart: %x\n", *ringStart);
	seq_printf(s, "RingIn: %x\n", *ringIn);
	seq_printf(s, "RingOut: %x\n", *ringOut);
	seq_printf(s, "RingEnd: %x\n", *ringEnd);

	seq_printf(s, "RingBuffer:\n");
	for (i = 0; i < RPC_RING_SIZE; i += 16) {
		uint32_t *addr = (uint32_t *)(uintptr_t)(*ringStart + pa2va_offset + i);

		seq_printf(s, "%x: %08x %08x %08x %08x\n",
			*(record->ringStart) + i,
			FW2SCPU(big_endian, *(addr + 0)),
			FW2SCPU(big_endian, *(addr + 1)),
			FW2SCPU(big_endian, *(addr + 2)),
			FW2SCPU(big_endian, *(addr + 3)));
	}
	up_write(&record->Sem);
}

static int rpc_debug_kern_rpc_show(struct seq_file *s, void *unused)
{
	struct kern_rpc *kern = s->private;

	seq_printf(s, "Kernel RPC Name %s\n", kern->name);
	seq_printf(s, "    is_paused: %s\n", kern->is_paused?"YES":"NO");
	seq_printf(s, "    is_suspend: %s\n", kern->is_suspend?"YES":"NO");

	seq_printf(s, "Dump write record:\n");
	dump_ringbuf(s, &kern->write_record);

	seq_printf(s, "Dump read record:\n");
	dump_ringbuf(s, &kern->read_record);

	return 0;
}

static int rpc_debug_kern_rpc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rpc_debug_kern_rpc_show, inode->i_private);
}

static const struct file_operations rpc_debug_kern_rpc_ops = {
	.open =		rpc_debug_kern_rpc_open,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

static int rpc_debug_user_rpc_show(struct seq_file *s, void *unused)
{
	struct user_rpc *user = s->private;
	struct rpc_process *proc;
	struct rpc_release_process_list *release_proc_lists;
	struct rpc_release_process_list *tmp_entry;
	struct list_head *listptr;
	int i;

	seq_printf(s, "User RPC Name %s\n", user->name);
	seq_printf(s, "    is_paused: %s\n", user->is_paused?"YES":"NO");
	seq_printf(s, "    is_suspend: %s\n", user->is_suspend?"YES":"NO");
	seq_printf(s, "    channel_id_write: %d\n", user->channel_id_write);
	seq_printf(s, "    channel_id_read: %d\n", user->channel_id_read);

	seq_printf(s, "currProc: %d\n", user->currProc?
		    ((struct rpc_process *)user->currProc)->tgid : 0);
	seq_printf(s, "nextRpc: %x\n", user->nextRpc);

	spin_lock_bh(&user->lock);
	seq_printf(s, "Dump RPC process:\n");
	i = 0;
	list_for_each_entry(proc, &user->tasks, list) {
		struct rpc_thread *thread;
		struct rpc_handler *handler;

		seq_printf(s, "    #%d process %s @%px tgid: %d pid: %d\n",
			    i++, proc->name, proc, proc->tgid, proc->pid);
		seq_printf(s, "        minor_id: %d\n", proc->minor_id);
		seq_printf(s, "        bStayActive: %s\n",
			    proc->bStayActive?"true":"false");
		seq_printf(s, "        bExit: %s\n",
			    proc->bExit?"true":"false");

		list_for_each_entry(thread, &proc->threads, list) {
			seq_printf(s, "        thread@%px pid: %d\n",
				     thread, thread->pid);
		}

		list_for_each_entry(handler, &proc->handlers, list) {
			seq_printf(s, "        handler@%px programID: %d\n",
				     handler, handler->programID);
		}
	}
	spin_unlock_bh(&user->lock);

	spin_lock_bh(&user->release_proc_lock);
	seq_printf(s, "Dump Release process:\n");
	release_proc_lists = &user->release_proc_lists;
	list_for_each(listptr, &release_proc_lists->list) {
		tmp_entry = list_entry(listptr, struct rpc_release_process_list,
			    list);
		seq_printf(s, "    process pid: %d cnt=%d\n", tmp_entry->pid,
			    tmp_entry->cnt);
	}
	spin_unlock_bh(&user->release_proc_lock);

	seq_printf(s, "Dump write record:\n");
	dump_ringbuf(s, &user->write_record);

	seq_printf(s, "Dump read record:\n");
	dump_ringbuf(s, &user->read_record);

	return 0;
}

static int rpc_debug_user_rpc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rpc_debug_user_rpc_show, inode->i_private);
}

static const struct file_operations rpc_debug_user_rpc_ops = {
	.open =		rpc_debug_user_rpc_open,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

void add_rpc_debugfs(struct rpc_device *rpc_dev)
{
	dev_dbg(rpc_dev->dev, "%s Enter\n", __func__);
	rpc_dev->debug_root = debugfs_create_dir("rpc", NULL);

	debugfs_create_file("remote_mem", 0444,
		    rpc_dev->debug_root,
		    rpc_dev,
		    &rpc_debug_remote_mem_ops);

	debugfs_create_file("version", 0444,
		    rpc_dev->debug_root,
		    rpc_dev,
		    &rpc_debug_version_ops);


	dev_dbg(rpc_dev->dev, "%s Exit\n", __func__);
}

void add_rpc_debugfs_client(struct rpc_device *rpc_dev,
	    struct rpc_client *client)
{
	if (!rpc_dev->debug_root) {
		dev_err(rpc_dev->dev, "%s debug_root is NULL\n", __func__);
		return;
	}

	dev_dbg(client->dev, "%s %s Enter\n", __func__, client->name);
	client->debug_node = debugfs_create_dir(client->name,
		    rpc_dev->debug_root);
	if (client->debug_node) {
		if (!debugfs_create_file("status", 0444,
			    client->debug_node,
			    client,
			    &rpc_debug_status_ops))
			goto file_error;

		if (!debugfs_create_file("kern_rpc", 0444,
			    client->debug_node,
			    &client->kern,
			    &rpc_debug_kern_rpc_ops))
			goto file_error;

		if (client->is_support_poll &&
			    !debugfs_create_file("poll_rpc", 0444,
			      client->debug_node,
			      &client->user_poll,
			      &rpc_debug_user_rpc_ops))
			goto file_error;

		if (client->is_support_intr &&
			    !debugfs_create_file("intr_rpc", 0444,
			      client->debug_node,
			      &client->user_intr,
			      &rpc_debug_user_rpc_ops))
			goto file_error;
	}

	dev_dbg(client->dev, "%s %s Exit\n", __func__, client->name);
	return;

file_error:
	debugfs_remove_recursive(client->debug_node);
}

void remove_rpc_debugfs(struct rpc_device *rpc_dev)
{
	debugfs_remove_recursive(rpc_dev->debug_root);
}

void remove_rpc_debugfs_client(struct rpc_device *rpc_dev,
	    struct rpc_client *client)
{
	debugfs_remove_recursive(client->debug_node);
}

#endif /* CONFIG_DEBUG_FS */
