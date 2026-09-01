/* SPDX-License-Identifier: GPL-2.0 */
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
#include <trace/events/rtk_rpc.h>
#include <linux/uaccess.h>

#ifndef _RTK_RPC_H
#define _RTK_RPC_H

#define CONFIG_REALTEK_RPC_PROGRAM_REGISTER
#define RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID

#define RPC_SB2_INT 0x0
#define RPC_SB2_INT_EN 0x4
#define RPC_SB2_INT_ST 0x8

#define RPC_INT_WRITE_1 1
#define RPC_INT_SA (1 << 1)
/*
 * for Hank SoC
 * RPC_INT_AS (1 << 1)
 * for other SoC
 * RPC_INT_AS (1 << 3)
 */
#define RPC_INT_AS (1 << 1)
#define RPC_INT_SA_HANK (1 << 3)
#define RPC_INT_VS (1 << 2)
#define RPC_INT_SV (1 << 2)
#define RPC_INT_SVE3 0x1
#define RPC_INT_VE3S_ST (1 << 4)
#define RPC_INT_HS (1 << 4)
#define RPC_INT_SH (1 << 4)

#define R_PROGRAM 98
#define AUDIO_SYSTEM 201
#define AUDIO_AGENT 202
#define VIDEO_AGENT 300
#define VENC_AGENT 400
#define HIFI_AGENT 500


#define KERNELID 98
#define REPLYID 99
#define RPC_AUDIO 0x0
#define RPC_VIDEO 0x1
#define RPC_VE3 0x2
#define RPC_HIFI 0x3
#define RPC_OK 0
#define RPC_FAIL -1

#ifndef RPC_ID
#define RPC_ID 0x5566
#endif

#ifndef RPC_MAJOR
#define RPC_MAJOR 0 /* dynamic major by default */
#endif

#define IPC_SHM_OFFSET (0x000000C4)

#define RPC_HAS_BIT(addr, bit) (readl(addr) & bit)
#define RPC_SET_BIT(addr,bit) (writel((readl(addr)|bit), addr))
#define RPC_RESET_BIT(addr,bit) (writel((readl(addr)&~bit), addr))

#define VO_DC_SET_NOTIFY (__cpu_to_be32(1U << 16)) /* ACPU write */
#define VO_DC_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 17))
#define AUDIO_RPC_SET_NOTIFY (__cpu_to_be32(1U << 24)) /* ACPU write */
#define AUDIO_RPC_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 25))
#define VIDEO_RPC_SET_NOTIFY (__cpu_to_be32(1U << 0)) /* VCPU write */
#define VIDEO_RPC_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 1))


#define DC_VO_SET_NOTIFY (__cpu_to_be32(1U << 0)) /* SCPU write */
#define DC_VO_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 1))
#define RPC_AUDIO_SET_NOTIFY (__cpu_to_be32(1U << 8)) /* SCPU write */
#define RPC_AUDIO_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 9))
#define RPC_VIDEO_SET_NOTIFY (__cpu_to_be32(1U << 2)) /* SCPU write */
#define RPC_VIDEO_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 3))

#define VE3_RPC_SET_NOTIFY (__cpu_to_be32(1U << 0)) /* VE3 write */
#define VE3_RPC_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 1))
#define RPC_VE3_SET_NOTIFY (__cpu_to_be32(1U << 2)) /* SCPU write */
#define RPC_VE3_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 3))

/*
 *struct rpc_ring_record
 *@uint32_t ringBuf: buffer for interrupt mode
 *@uint32_t ringStart: pointer to start of ring buffer
 *@uint32_t ringEnd: pointer to end of ring buffer
 *@volatile uint32_t ringIn: pointer to where next data will be inserted in
 *the ring buffer
 *@volatile uint32_t ringOut: pointer to where next data will
 *be extracted from the ring buffer
 */
struct rpc_record { /*size should be 64 bytes*/
	uint32_t ringBuf;
	uint32_t ringStart;
	uint32_t ringEnd;
	volatile uint32_t ringIn;
	volatile uint32_t ringOut;
	/* scpu internal use */
	uint32_t reserved1;
	uint32_t reserved2[10];
};

struct rpc_record_hifi { /*size should be 512 bytes*/
	uint32_t ringBuf;
	uint32_t ringStart;
	uint32_t ringEnd;
	volatile uint32_t ringIn;
	uint32_t reserved1[28];
	volatile uint32_t ringOut;
	uint32_t reserved2[31];
	/* scpu internal use */
	uint32_t reserved3[32];
};

struct rpc_record_mapping {
	uint32_t *ringBuf;
	uint32_t *ringStart;
	uint32_t *ringEnd;
	volatile uint32_t *ringIn;
	volatile uint32_t *ringOut;
	struct rw_semaphore Sem;
	char name[32];

	uint64_t pa2va_offset;
	bool big_endian;
};

/*
 * struct rpc_thread
 * maintain a list of threads in the same process
 */
struct rpc_thread {
	pid_t pid; /* user process tid */
	struct list_head list;
};

/*
 * struct rpc_process
 * @struct list_head threads: pids that share the same file descriptor
 *
 * maintain a list of processes that use the same RPC device
 */
struct rpc_process {
	struct list_head list; /* myself list */
	pid_t tgid; /* user process pid */
	pid_t pid; /* user process tid and only for record to debug*/
	char name[TASK_COMM_LEN];

	wait_queue_head_t waitQueue;
	struct rpc_client *client;
	struct user_rpc *user;
	int minor_id;

	struct list_head threads; /* store threads list for user process tid */
#ifdef CONFIG_REALTEK_RPC_PROGRAM_REGISTER
	struct list_head handlers;
#endif	/* CONFIG_REALTEK_RPC_PROGRAM_REGISTER */

	struct list_head at_group;

	bool bStayActive; // If true, then FW will not be notified when process is destroyed.
	bool bExit;
};

struct rpc_process_group {
	struct list_head list; /* myself list */
	int tgid;
	int type;
	char comm[TASK_COMM_LEN];

	struct list_head process_lists;

	/* for send destroy rpc */
	phys_addr_t paddr;
	void *vaddr;
	struct dma_buf *rpc_dmabuf;
	struct sg_table *table;
	struct dma_buf_attachment *attachment;
};

/*
 * struct rpc_handler
 * maintain a list of handlers
 */
struct rpc_handler {
	uint32_t programID;
	struct list_head list; /* myself list */
};

struct rpc_release_process_list {
	int pid;
	int cnt;
	struct list_head list;
};

struct rpc_mem_entry {
	unsigned long phys_addr;
	unsigned long size;
	struct dma_buf *rpc_dmabuf;
	struct sg_table *table;
	struct dma_buf_attachment *attachment;
	struct rpc_mem_entry *next;
};

struct kern_rpc {
	struct rpc_record_mapping write_record;
	struct rpc_record_mapping read_record;

	char name[32];

	wait_queue_head_t waitQueue;
	struct task_struct *rpc_kthread;

	wait_queue_head_t rpc_wq;
	uint32_t *rpc_retval;
	int complete_condition;
	struct mutex rpc_kern_lock;

	int is_paused;
	int is_suspend;
};

struct user_rpc {
	struct rpc_record_mapping write_record;
	struct rpc_record_mapping read_record;

	char name[32];
	/* for ring buf read/write */
	//wait_queue_head_t waitQueue;

	/* for rpc process */
	volatile void *currProc;
	struct list_head tasks; /* store rpc_process */
	volatile uint32_t nextRpc;

	struct tasklet_struct tasklet;
	spinlock_t lock;

	/* for remote allocate */
	int remote_alloc_flag;
	wait_queue_head_t waitQueue;
	struct task_struct *remote_alloc_kthread;
	struct rpc_process *remote_alloc_proc;

	/* for process release */
	struct rpc_release_process_list release_proc_lists;
	spinlock_t release_proc_lock;

	/* user file operations */
	struct file_operations *f_ops;
	int channel_id_write;
	int channel_id_read;
	struct device *dev_channel_write;
	struct device *dev_channel_read;

	int timeout;

	int is_paused;
	int is_suspend;
};

struct rpc_client {
	struct list_head list;
	struct device *dev;
	char name[32];
	int id;
	bool big_endian;
	uint64_t ringbuf_paddr2vaddr_offset;

	/* rpc_device */
	struct rpc_device *rpc_dev;

	/* kernel rpc */
	struct kern_rpc kern;

	/* user rpc */
	struct user_rpc user_poll;
	struct user_rpc user_intr;
	bool is_support_poll;
	bool is_support_intr;

	/* for all user rpc process */
	struct rpc_process_group process_groups;
	spinlock_t process_group_lock;

	/* callback */
	int (*send_interrupt)(struct rpc_client *client);

	int (*suspend)(struct rpc_client* client);
	int (*resume)(struct rpc_client* client);
	void (*shutdown)(struct rpc_client* client);

	/* for debugfs */
	struct dentry *debug_node;
};

struct rpc_device {
	struct device *dev;
	int rpc_major;
	struct class *rpc_class;

	struct list_head clients;

	phys_addr_t rpc_common_paddr;
	void __iomem *rpc_common_vaddr;
	void __iomem *ipc_shm_vaddr;
	phys_addr_t rpc_ringbuf_paddr;
	void __iomem *rpc_ringbuf_vaddr;
	uint64_t ringbuf_paddr2vaddr_offset;

	void __iomem *mDebugFlagMemory;
	void __iomem *mDebugPrintMemory;

	spinlock_t rpc_mem_lock;
	struct rpc_mem_entry *rpc_mem_head;
	int rpc_mem_count;

	/* for debugfs */
	struct dentry *debug_root;
};

/* Inline function */

#ifdef MY_COPY
//FIXME: accepts SCPU addr
static inline int my_memcpy_fromio(int *des, int *src, int size)
{
#if defined(CONFIG_CPU_V7)
	_memcpy_fromio(des, src, size);
	return 0;
#else
	__memcpy_fromio(des, src, size);
	return 0;
#endif
}

static inline int my_memcpy_toio(int *des, int *src, int size)
{
#if defined(CONFIG_CPU_V7)
	_memcpy_tomio(des, src, size);
	return 0;
#else
	__memcpy_toio(des, src, size);
	return 0;
#endif
}

static int inline my_copy_to_user(int *des, int *src, int size)
{
	char buf[256];
	int count = size;
	void *pSrc = (void *)src;
	int ret = 0;
	int i = 0;

	if (size > 256) {
		BUG();
	}

	pr_debug("%s des:%px, src:%px, size:%d pid:%d tid:%d comm:%s\n",
		    __func__,
		    des, src, size, current->tgid, current->pid, current->comm);

	while (size >= 4) {
		*(int *)&buf[i] = __raw_readl(pSrc);
		i += 4;
		pSrc += 4;
		size -= 4;
	}

	while (size > 0) {
		buf[i] = __raw_readb(pSrc);
		i++;
		pSrc++;
		size--;
	}

	ret = copy_to_user((int *)des, (int *)buf, count);

	return ret;
}

static int inline my_copy_from_user(volatile void __iomem *des, const void *src, int size)
{
	char buf[256];
	int ret = 0;
	int i = 0;
	volatile char *cdes;

	if (size > 256) {
		BUG();
	}

	pr_debug("%s des:%px, src:%px, size:%d pid:%d tid:%d comm:%s\n",
		    __func__,
		    des, src, size, current->tgid, current->pid, current->comm);

	ret = copy_from_user((unsigned int *) buf, (unsigned int __user *) src, size);

	if (ret != 0)
		pr_err("copy_from_user error: %d bytes\n", ret);

	cdes = (char *)des;
	for (i = 0 ; i < size ; i++)
		cdes[i] = buf[i];

	return 0;
}

static inline int my_copy_user(int *des, int *src, int size)
{
	char *csrc, *cdes;
	int i;

	pr_debug("%s des:%px, src:%px, size:%d pid:%d tid:%d comm:%s\n",
		    __func__,
		    des, src, size, current->tgid, current->pid, current->comm);

	might_fault();

	if ((unsigned long)des < 0xc0000000 &&
	    access_ok(des, size) == 0)
		BUG();

	if ((unsigned long)src < 0xc0000000 &&
	    access_ok(src, size) == 0)
		BUG();

	if (((unsigned long)src & 0x3) || ((unsigned long)des & 0x3))
		pr_warn("my_copy_user: unaligned happen...\n");

	while (size >= 4) {
		*des++ = *src++;
		size -= 4;
	}

	csrc = (char *)src;
	cdes = (char *)des;

	for (i = 0 ; i < size ; i++)
		cdes[i] = csrc[i];

	return 0;
}
#endif /* MY_COPY */

static int __maybe_unused rpc_read_copy(int *des, int *src, int size, bool to_user)
{
	if (to_user) {
#ifdef MY_COPY
		return my_copy_to_user((int *)des, (int *)src, size);
#else
		return copy_to_user((int *)des, (int *)src, size);
#endif /* MY_COPY */
	} else {
#ifdef MY_COPY
		//return my_memcpy_fromio((int *)des, (int *)src, size);
		return my_copy_user((int *)des, (int *)src, size);
#else
		memcpy((int *)des, (int *)src, size);
		return 0;
#endif /* MY_COPY */
	}
}

static int __maybe_unused rpc_write_copy(int *des, int *src, int size, bool from_user)
{
	if (from_user) {
#ifdef MY_COPY
		return my_copy_from_user((int *)des, (int *)src, size);
#else
		return copy_from_user((int *)des, (int *)src, size);
#endif /* MY_COPY */
	} else {
#ifdef MY_COPY
		//return my_memcpy_toio((int *)des, (int *)src, size);
		return my_copy_user((int *)des, (int *)src, size);
#else
		memcpy((int *)des, (int *)src, size);
		return 0;
#endif
	}
}

/* convert data form fw to scpu */
#define FW2SCPU(big_endian, value) \
	(big_endian?ntohl(value):(value))
/* convert data form scpu to fw */
#define SCPU2FW(big_endian, value) \
	(big_endian?htonl(value):(value))

/* convert rpc_struct form fw to scpu */
static inline void rpc_struct_convert_from_fw(struct rpc_struct *rpc, bool big_endian)
{
	if (!big_endian)
		return;

	rpc->programID = ntohl(rpc->programID);
	rpc->versionID = ntohl(rpc->versionID);
	rpc->procedureID = ntohl(rpc->procedureID);
	rpc->taskID = ntohl(rpc->taskID);
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
	rpc->sysTID = ntohl(rpc->sysTID);
#endif	/* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
	rpc->sysPID = ntohl(rpc->sysPID);
	rpc->parameterSize = ntohl(rpc->parameterSize);
	rpc->mycontext = ntohl(rpc->mycontext);
}

/* convert rpc_struct form scpu to fw */
static inline void rpc_struct_convert_to_fw(struct rpc_struct *rpc, bool big_endian)
{
	if (!big_endian)
		return;

	rpc->programID = htonl(rpc->programID);
	rpc->versionID = htonl(rpc->versionID);
	rpc->procedureID = htonl(rpc->procedureID);
	rpc->taskID = htonl(rpc->taskID);
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
	rpc->sysTID = htonl(rpc->sysTID);
#endif	/* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
	rpc->sysPID = htonl(rpc->sysPID);
	rpc->parameterSize = htonl(rpc->parameterSize);
	rpc->mycontext = htonl(rpc->mycontext);
}

static inline void show_rpc_struct(const char *func, struct rpc_struct *rpc)
{
#ifdef RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID
	pr_debug("%s: program:%u version:%u procedure:%u taskID:%u sysTID:%u sysPID:%u size:%u context:%x 90k:%u %s\n",
		func, rpc->programID, rpc->versionID, rpc->procedureID, rpc->taskID, rpc->sysTID, rpc->sysPID,
		rpc->parameterSize, rpc->mycontext, (u32)refclk_get_val_raw(), in_atomic() ? "atomic" : "");
#else
	pr_debug("%s: program:%u version:%u procedure:%u taskID:%u sysPID:%u size:%u context:%x 90k:u %s\n",
			func, rpc->programID, rpc->versionID, rpc->procedureID, rpc->taskID, rpc->sysPID, rpc->parameterSize,
			rpc->mycontext, (u32)refclk_get_val_raw(), in_atomic() ? "atomic" : "");
#endif	/* RPC_SUPPORT_MULTI_CALLER_SEND_TID_PID */
}

struct rpc_device *get_rpc_dev(void);

void rpc_send_interrupt(struct rpc_client *client);

int remote_alloc_thread(void * p);
int rpc_mem_init(struct rpc_device *rpc_dev);

uint32_t rpc_ringbuf_get_ring_data_size(struct rpc_record_mapping *record);
bool rpc_ringbuf_empty(struct rpc_record_mapping *record);
uint32_t rpc_ringbuf_get_ringOut(struct rpc_record_mapping *record);
uint32_t rpc_ringbuf_set_ringOut(struct rpc_record_mapping *record,
	    uint32_t out);
uint32_t rpc_ringbuf_update_ringOut_by_size(struct rpc_record_mapping *record,
	    uint32_t size);
uint32_t rpc_ringbuf_get_next_ringOut_by_size(
	    struct rpc_record_mapping *record, uint32_t size);
uint32_t rpc_ringbuf_reading_data(struct rpc_record_mapping *record,
		uint32_t out, char *buf, int datasize);
ssize_t rpc_ringbuf_read(struct rpc_record_mapping *record,
	    char *buf, size_t count, bool to_user);
ssize_t rpc_ringbuf_write(struct rpc_record_mapping *record,
	     const char *buf, size_t count, bool from_user);
int dump_rpc_ringbuf(struct rpc_record_mapping *record);

int rpc_client_kern_init(struct rpc_client* client);
int rpc_client_user_intr_init(struct rpc_device* rpc_dev,
	     struct rpc_client* client);
int rpc_client_user_poll_init(struct rpc_device *rpc_dev,
	     struct rpc_client* client);

int rpc_notify_fw_destroy_process(struct rpc_client *client,
	    int pid, phys_addr_t paddr, void *vaddr);

/* For debugfs */
#ifdef CONFIG_DEBUG_FS
void add_rpc_debugfs(struct rpc_device *rpc_dev);
void add_rpc_debugfs_client(struct rpc_device *rpc_dev,
	    struct rpc_client *client);
void remove_rpc_debugfs(struct rpc_device *rpc_dev);
void remove_rpc_debugfs_client(struct rpc_device *rpc_dev,
	    struct rpc_client *client);
#else
static inline void add_rpc_debugfs(struct rpc_device *rpc_dev)
{
}

static inline void add_rpc_debugfs_client(struct rpc_device *rpc_dev,
	    struct rpc_client *client)
{
}

static inline void remove_rpc_debugfs(struct rpc_device *rpc_dev)
{
}

static inline void remove_rpc_debugfs_client(struct rpc_device *rpc_dev,
	    struct rpc_client *client)
{
}
#endif

/* For client */
int rtk_rpc_client_init(void);
struct rpc_client *rpc_client_get(int type);
int rpc_client_register(struct rpc_device *rpc_dev, struct rpc_client *client);
int rpc_client_unregister(struct rpc_device *rpc_dev, struct rpc_client *client);

/* Define for rpc_comm_buffer */

#define RPC_RING_SIZE 512	/* size of ring buffer */
#define RPC_RECORD_SIZE 64	/* size of ring buffer record */
#define RPC_RECORD_HIFI_SIZE 384	/* size of ring buffer record */

struct rpc_comm_buffer { /* total size 16k */
	/* a/v cpu user space ring from 0x040FF000 */
	uint8_t acpu_user_poll_write_ring[RPC_RING_SIZE]; /* unused */
	uint8_t acpu_user_intr_write_ring[RPC_RING_SIZE];
	uint8_t acpu_user_poll_read_ring[RPC_RING_SIZE]; /* unused */
	uint8_t acpu_user_intr_read_ring[RPC_RING_SIZE];
	uint8_t vcpu_user_poll_write_ring[RPC_RING_SIZE]; /* unused */
	uint8_t vcpu_user_intr_write_ring[RPC_RING_SIZE];
	uint8_t vcpu_user_poll_read_ring[RPC_RING_SIZE]; /* unused */
	uint8_t vcpu_user_intr_read_ring[RPC_RING_SIZE];

	/* a/v cpu user space record from 0x04100000 */
	uint8_t acpu_user_poll_write_record[RPC_RECORD_SIZE]; /* unused */
	uint8_t acpu_user_poll_read_record[RPC_RECORD_SIZE]; /* unused */
	uint8_t vcpu_user_poll_write_record[RPC_RECORD_SIZE]; /* unused */
	uint8_t vcpu_user_poll_read_record[RPC_RECORD_SIZE]; /* unused */
	uint8_t acpu_user_intr_write_record[RPC_RECORD_SIZE];
	uint8_t acpu_user_intr_read_record[RPC_RECORD_SIZE];
	uint8_t vcpu_user_intr_write_record[RPC_RECORD_SIZE];
	uint8_t vcpu_user_intr_read_record[RPC_RECORD_SIZE];

	/* a/v cpu kernel space ring from 0x04100200 */
	uint8_t acpu_kern_write_ring[RPC_RING_SIZE];
	const uint8_t acpu_kern_read_ring[RPC_RING_SIZE];
	uint8_t vcpu_kern_write_ring[RPC_RING_SIZE];
	uint8_t vcpu_kern_read_ring[RPC_RING_SIZE];

	/* a/v cpu kernel space record from 0x04100a00 */
	uint8_t acpu_kern_write_record[RPC_RECORD_SIZE];
	uint8_t acpu_kern_read_record[RPC_RECORD_SIZE];
	uint8_t vcpu_kern_write_record[RPC_RECORD_SIZE];
	uint8_t vcpu_kern_read_record[RPC_RECORD_SIZE];
	uint8_t reserved1[1280]; /* unused */

	/* ve3 user and kernel space ring from 0x04101000 */
	uint8_t ve3_user_intr_write_ring[RPC_RING_SIZE];
	uint8_t ve3_user_intr_read_ring[RPC_RING_SIZE];
	uint8_t ve3_kern_write_ring[RPC_RING_SIZE];
	uint8_t ve3_kern_read_ring[RPC_RING_SIZE];

	/* ve3 user and kernel space record from 0x04101800 */
	uint8_t ve3_user_intr_write_record[RPC_RECORD_SIZE];
	uint8_t ve3_user_intr_read_record[RPC_RECORD_SIZE];
	uint8_t ve3_kern_write_record[RPC_RECORD_SIZE];
	uint8_t ve3_kern_read_record[RPC_RECORD_SIZE];
	uint8_t reserved2[256]; /* unused */

	/* hifi user and kernel space ring from 0x04101a00 */
	uint8_t hifi_user_intr_write_ring[RPC_RING_SIZE];
	uint8_t hifi_user_intr_read_ring[RPC_RING_SIZE];
	uint8_t reserved3[512]; /* unused */
	uint8_t hifi_kern_write_ring[RPC_RING_SIZE]; /* 0x04102000 */
	uint8_t hifi_kern_read_ring[RPC_RING_SIZE];

	/* hifi user and kernel space record from 0x04102400 */
	uint8_t hifi_user_intr_write_record[RPC_RECORD_HIFI_SIZE];
	uint8_t hifi_user_intr_read_record[RPC_RECORD_HIFI_SIZE];
	uint8_t hifi_kern_write_record[RPC_RECORD_HIFI_SIZE];
	uint8_t hifi_kern_read_record[RPC_RECORD_HIFI_SIZE];
	uint8_t reserved4[1536]; /* unused */
};

#endif /* _RTK_RPC_H */
