/* SPDX-License-Identifier: GPL-2.0-only */

#include <linux/workqueue.h>
#include <linux/skbuff.h>
#include <linux/mutex.h>
#include <linux/rpmsg.h>
#include <soc/realtek/rtk-rpmsg.h>

struct rtk_rpc_client {
	struct rpmsg_endpoint *ept;
	struct device *dev;
	char *name;
	int big_endian;
	struct rpmsg_device *rpdev;
	struct r_program_entry *r_program_head;
	int r_program_count;
	spinlock_t r_program_lock;
	struct task_struct *r_program_kthread;
	wait_queue_head_t r_program_waitQueue;
	int r_program_flag;
	struct work_struct work;
	spinlock_t queue_lock;
	struct sk_buff_head queue;
	int (*send)(struct rtk_rpc_client *client, uint32_t command, uint32_t param1,
		uint32_t param2, uint32_t *ret);
	uint32_t *retval;
	struct completion ack;

	struct mutex send_lock;
};

void trans_rpc_struct(struct rpc_struct *rpc, struct rpc_struct *rpc_raw);

void endian_swap_32_read(void *buf, size_t size);
void endian_swap_32_write(void *buf, size_t size);

#define REPLYID 99
#define KERNELID 98
#define RPC_TIMEOUT (5 * HZ)


