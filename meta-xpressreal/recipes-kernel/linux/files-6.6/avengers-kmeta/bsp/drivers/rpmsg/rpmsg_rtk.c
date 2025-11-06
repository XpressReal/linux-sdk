// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek RPMSG driver
 *
 * Copyright (c) 2017-2022 Realtek Semiconductor Corp.
 */

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/idr.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/debugfs.h>
#include <linux/rpmsg.h>
#include <soc/realtek/rtk_ipc_shm.h>
#include <soc/realtek/rtk-rpmsg.h>
#include <linux/skbuff.h>
#include <linux/of_reserved_mem.h>
#include <linux/hwspinlock.h>
#include <linux/moduleparam.h>
#include <linux/remoteproc.h>
#include "rpmsg_internal.h"

#define IDR_MIN 0xf000000
#define IDR_MAX 0xfffffff
#define AUDIO_RPC_SET_NOTIFY (__cpu_to_be32(1U << 24)) /* ACPU write */
#define AUDIO_RPC_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 25))
#define VIDEO_RPC_SET_NOTIFY (__cpu_to_be32(1U << 0)) /* VCPU write */
#define VIDEO_RPC_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 1))
#define VE3_RPC_SET_NOTIFY (__cpu_to_be32(1U << 0)) /* VE3 write */
#define VE3_RPC_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 1))
#define RPC_AUDIO_SET_NOTIFY (__cpu_to_be32(1U << 8)) /* SCPU write */
#define RPC_AUDIO_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 9))
#define RPC_VIDEO_SET_NOTIFY (__cpu_to_be32(1U << 2)) /* SCPU write */
#define RPC_VIDEO_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 3))
#define RPC_VE3_SET_NOTIFY (__cpu_to_be32(1U << 2)) /* SCPU write */
#define RPC_VE3_FEEDBACK_NOTIFY (__cpu_to_be32(1U << 3))


#define RPC_SB2_INT 0xa80
#define RPC_SB2_INT_EN 0xa84
#define RPC_SB2_INT_ST 0xa88
#define RPC_INT_SA BIT(1)
#define RPC_INT_AS BIT(1)
#define RPC_INT_VS BIT(2)
#define RPC_INT_SV BIT(2)
#define RPC_INT_SVE3 BIT(0)
#define RPC_INT_VE3S BIT(4)
#define RPC_INT_HS BIT(4)
#define RPC_INT_SH BIT(4)
#define RPC_INT_H1S BIT(5)
#define RPC_INT_SH1 BIT(5)
#define RPC_INT_ACPU_EN BIT(1)
#define RPC_INT_VCPU_EN BIT(2)
#define RPC_INT_HIFI_EN BIT(4)
#define RPC_INT_HIFI1_EN BIT(5)
#define RPC_INT_WRITE_EN BIT(0)

#define RPC_INT_VE3_REG 0x7c
#define RPC_INT_VE3_EN BIT(4) /*need check*/

#define RPC_INT_WRITE_1 0x1
#define RPC_HAS_BIT(addr, bit) (readl(addr) & bit)
#define RPC_SET_BIT(addr, bit) (writel((readl(addr)|bit), addr))
#define RPC_RESET_BIT(addr, bit) (writel((readl(addr)&~bit), addr))

#define R_PROGRAM 98
#define AUDIO_SYSTEM 201
#define AUDIO_AGENT 202
#define VIDEO_AGENT 300
#define VENC_AGENT 400
#define HIFI_AGENT 500
#define KR4_AGENT 600
#define REPLYID 99

#define rpc_ringbuf_phys 0x40ff000
extern void __iomem *rpc_ringbuf_base;

#define ringbuf_phys_to_virt(x) (x - rpc_ringbuf_phys + rpc_ringbuf_base)

/*share memory with ACPU/VCPU*/
struct av_info {
	uint32_t ringBuf;
	uint32_t ringStart;
	uint32_t ringEnd;

	volatile uint32_t ringIn;
	volatile uint32_t ringOut;
};

struct hifi_info {
	uint32_t ringBuf;
	uint32_t ringStart;
	uint32_t ringEnd;

	volatile uint32_t ringIn;
	uint32_t reserved1[28];
	volatile uint32_t ringOut;
	uint32_t reserved2[31];
};

struct kr4_info {
	uint32_t ringBuf;
	uint32_t ringStart;
	uint32_t ringEnd;

	volatile uint32_t ringIn;
	uint32_t reserved1[4];
	volatile uint32_t ringOut;
	uint32_t reserved2[7];
};

struct rpc_shm_info {
	union {
		struct av_info *av;
		struct hifi_info *hifi;
		struct kr4_info *kr4;
	};

};

struct rtk_rpmsg_channel {
	struct rtk_rcpu *rcpu;
	struct rtk_rpmsg_device *rtk_rpdev;
	struct tasklet_struct tasklet;
	struct rpc_shm_info tx_info;
	struct rpc_shm_info rx_info;
	void __iomem *tx_fifo;
	void __iomem *rx_fifo;
	char name[32];
	uint32_t id;
	struct list_head list;
	struct list_head rtk_ept_lists;
	spinlock_t txlock;
	spinlock_t rxlock;
	spinlock_t list_lock;
	void (*handle_data)(unsigned long data);
	struct dentry *debugfs_node;
	struct idr ept_ids;
	struct mutex ept_ids_lock;
	int use_idr;
};

struct remote_cpu_info {
	char name[10];
	int to_rcpu_notify_bit;
	int to_rcpu_feedback_bit;
	int from_rcpu_notify_bit;
	int from_rcpu_feedback_bit;
	int to_rcpu_intr_bit;
	int from_rcpu_intr_bit;
	int intr_en;
	int big_endian;
	int id;
	irqreturn_t (*isr)(int irq, void *data);
	void (*send_interrupt)(struct rtk_rcpu *rcpu);
};


struct rtk_rcpu {
	struct device *dev;
	struct device_node *of_node;
	int irq;
	struct regmap *rcpu_intr_regmap;
	struct list_head channels;
	struct hwspinlock *hwlock;
	const struct remote_cpu_info *info;
	volatile uint32_t *rcpu_notify;
	volatile uint32_t *sync_flag;
	bool skip_handshake;
	int status;
	struct rtk_rpmsg_device *rtk_rpdev;
	struct rproc *rproc;
};

struct rtk_rpmsg_device {
	struct rtk_rcpu *rcpu;
	struct rpmsg_device rpdev;
};


struct rtk_rpmsg_endpoint {
	struct rpmsg_endpoint ept;
	struct rtk_rpmsg_channel *channel;
	struct list_head list;
};


#define to_rcpu_info(d)	container_of(d, struct rtk_rcpu, dev)
#define to_rtk_rpdevice(_rpdev)	container_of(_rpdev, struct rtk_rpmsg_device, rpdev)
#define to_rtk_ept(_ept) container_of(_ept, struct rtk_rpmsg_endpoint, ept)

void endian_swap_32_read(void *buf, size_t size)
{
	unsigned int *pData = (unsigned int *) buf;
	size_t i;

	if ((size%sizeof(int)) != 0) {
		pr_err("%s : Illegal size %zu\n", __func__, size);
	} else {
		for (i = 0; i < (size/sizeof(int)); i++)
			pData[i] = ntohl(pData[i]);
	}
}
EXPORT_SYMBOL_GPL(endian_swap_32_read);

void endian_swap_32_write(void *buf, size_t size)
{
	unsigned int *pData = (unsigned int *) buf;
	size_t i;

	if ((size%sizeof(int)) != 0) {
		pr_err("%s : Illegal size %zu\n", __func__, size);
	} else {
		for (i = 0; i < (size/sizeof(int)); i++)
			pData[i] = htonl(pData[i]);
	}
}
EXPORT_SYMBOL_GPL(endian_swap_32_write);


static void convert_rpc_struct(struct rpc_struct *rpc)
{
	rpc->programID = ntohl(rpc->programID);
	rpc->versionID = ntohl(rpc->versionID);
	rpc->procedureID = ntohl(rpc->procedureID);
	rpc->taskID = ntohl(rpc->taskID);
	rpc->sysTID = ntohl(rpc->sysTID);
	rpc->sysPID = ntohl(rpc->sysPID);
	rpc->parameterSize = ntohl(rpc->parameterSize);
	rpc->mycontext = ntohl(rpc->mycontext);

	pr_debug("rpc->programID:%d, rpc->versionID:%d, rpc->procedureID:%d, rpc->taskID:%d, rpc->sysTID:%d, rpc->sysPID:%d, rpc->parameterSize:%d, rpc->mycontext:0x%x\n",
		rpc->programID, rpc->versionID, rpc->procedureID, rpc->taskID, rpc->sysTID, rpc->sysPID, rpc->parameterSize, rpc->mycontext);
}

static void print_rpc_struct(struct rtk_rpmsg_channel *channel, char *buf)
{
	struct rpc_struct *rpc = (struct rpc_struct *)buf;

	if (channel->rcpu->info->big_endian)
		dev_dbg(channel->rcpu->dev, "rpc->programID:%d, rpc->versionID:%d, rpc->procedureID:%d, rpc->taskID:%d, rpc->sysTID:%d, rpc->sysPID:%d, rpc->parameterSize:%d, rpc->mycontext:0x%x\n",
		ntohl(rpc->programID), ntohl(rpc->versionID), ntohl(rpc->procedureID), ntohl(rpc->taskID), ntohl(rpc->sysTID), ntohl(rpc->sysPID), ntohl(rpc->parameterSize), ntohl(rpc->mycontext));
	else
		dev_dbg(channel->rcpu->dev, "rpc->programID:%d, rpc->versionID:%d, rpc->procedureID:%d, rpc->taskID:%d, rpc->sysTID:%d, rpc->sysPID:%d, rpc->parameterSize:%d, rpc->mycontext:0x%x\n",
			rpc->programID, rpc->versionID, rpc->procedureID, rpc->taskID, rpc->sysTID, rpc->sysPID, rpc->parameterSize, rpc->mycontext);
}

static char *get_ring_data(struct rtk_rpmsg_channel *channel, int *retSize, struct rpc_struct *rpc)
{
	int size;
	int tail;
	int data_size;
	volatile uint32_t *ringIn, *ringOut, *ringStart, *ringEnd;
	int ringSize;
	int rpc_size = sizeof(struct rpc_struct);
	char *buf;
	char *tmp = (char *)rpc;
	uint32_t ring_tmp;
	struct rpc_shm_info *rx_info = &channel->rx_info;
	int out_offset;

	if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
		ringIn = &rx_info->hifi->ringIn;
		ringOut = &rx_info->hifi->ringOut;
		ringStart = &rx_info->hifi->ringStart;
		ringEnd = &rx_info->hifi->ringEnd;
	} else if (channel->id == KR4_ID) {
		ringIn = &rx_info->kr4->ringIn;
		ringOut = &rx_info->kr4->ringOut;
		ringStart = &rx_info->kr4->ringStart;
		ringEnd = &rx_info->kr4->ringEnd;
	} else {
		ringIn = &rx_info->av->ringIn;
		ringOut = &rx_info->av->ringOut;
		ringStart = &rx_info->av->ringStart;
		ringEnd = &rx_info->av->ringEnd;
	}

	ringSize = *ringEnd - *ringStart;
	size = (ringSize + *ringIn - *ringOut) % ringSize;
	out_offset = *ringOut - *ringStart;

	if (size < rpc_size) {
		dev_err(channel->rcpu->dev, "[%s] wrong rpc data size:0x%x\n", __func__, size);
		return ERR_PTR(-EINVAL);
	}
	tail = *ringEnd - *ringOut;

	if (tail >= rpc_size) {
		memcpy_fromio(tmp, (char *)(channel->rx_fifo + out_offset), rpc_size);
	} else {
		memcpy_fromio(tmp, (char *)(channel->rx_fifo + out_offset), tail);
		memcpy_fromio((tmp + tail), (char *)channel->rx_fifo, rpc_size - tail);
	}

	if (channel->rcpu->info->big_endian)
		convert_rpc_struct(rpc);

	data_size = rpc_size + rpc->parameterSize;
	*retSize = data_size;
	buf = kmalloc(data_size, GFP_ATOMIC);
	if (buf == NULL) {
		dev_err(channel->rcpu->dev, "[%s]cannot allocte buf\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
	if (size < data_size) {
		dev_err(channel->rcpu->dev, "[%s]rpc size not match. buf_size:0x%x  data_size:0x%x parameter_size:0x%x\n", __func__, size, data_size, rpc->parameterSize);
		kfree(buf);
		return ERR_PTR(-EINVAL);
	}

	if (tail >= data_size) {
		memcpy_fromio(buf, (char *)(channel->rx_fifo + out_offset), data_size);
		ring_tmp = *ringOut + ((data_size + 3) & 0xfffffffc);
		if (ring_tmp == *ringEnd)
			*ringOut = *ringStart;
		else
			*ringOut = ring_tmp;
	} else {
		memcpy_fromio(buf, (char *)(channel->rx_fifo + out_offset), tail);
		memcpy_fromio((buf + tail), (char *)channel->rx_fifo, data_size - tail);
		*ringOut = *ringStart + ((data_size - tail + 3) & 0xfffffffc);
	}

	return buf;
}

void handle_intr_data(unsigned long data)
{
	struct rtk_rpmsg_channel *channel = (struct rtk_rpmsg_channel *)data;
	struct rtk_rpmsg_endpoint *rtk_ept = NULL;
	struct device *dev = channel->rcpu->dev;
	unsigned long flags;
	int data_size = 0;
	struct rpc_struct rpc;
	char *buf;
	char *data_buf;
	uint32_t pid = 0;
	uint32_t addr = 0;
	struct list_head *list;
	struct task_struct *task;
	volatile uint32_t *ringIn, *ringOut, *ringStart, *ringEnd;

	if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
		ringIn = &channel->rx_info.hifi->ringIn;
		ringOut = &channel->rx_info.hifi->ringOut;
		ringStart = &channel->rx_info.hifi->ringStart;
		ringEnd = &channel->rx_info.hifi->ringEnd;
	} else if (channel->id == KR4_ID) {
		ringIn = &channel->rx_info.kr4->ringIn;
		ringOut = &channel->rx_info.kr4->ringOut;
		ringStart = &channel->rx_info.kr4->ringStart;
		ringEnd = &channel->rx_info.kr4->ringEnd;
	} else {
		ringIn = &channel->rx_info.av->ringIn;
		ringOut = &channel->rx_info.av->ringOut;
		ringStart = &channel->rx_info.av->ringStart;
		ringEnd = &channel->rx_info.av->ringEnd;
	}

	spin_lock_irqsave(&channel->rxlock, flags);
	if (*ringIn == *ringOut) {
		spin_unlock_irqrestore(&channel->rxlock, flags);
		return;
	}
	dev_dbg(dev, "[%s]before channel:%s  In:0x%x  Out:0x%x\n", __func__, channel->name, *ringIn, *ringOut);
	buf = get_ring_data(channel, &data_size, &rpc);
	if (IS_ERR(buf)) {
		dev_err(dev, "[%s]cannot get ring buffer data\n", __func__);
		spin_unlock_irqrestore(&channel->rxlock, flags);
		return;
	}
	dev_dbg(dev, "[%s]after channel:%s  In:0x%x  Out:0x%x\n", __func__, channel->name, *ringIn, *ringOut);
	spin_unlock_irqrestore(&channel->rxlock, flags);

	print_rpc_struct(channel, buf);

	data_buf = (buf + sizeof(struct rpc_struct));

	switch (rpc.programID) {
	case R_PROGRAM:
		break;
	case AUDIO_AGENT:
	case VIDEO_AGENT:
	case VENC_AGENT:
	case HIFI_AGENT:
	case KR4_AGENT:
		pid = rpc.sysPID;
		break;
	case REPLYID:
		if (channel->rcpu->info->big_endian)
			pid = ntohl(*((uint32_t *)data_buf));
		else
			pid = *((uint32_t *)data_buf);
		break;

	default:
		dev_err(dev, "[%s]unsupport programID:%d\n", __func__, rpc.programID);
		goto error;
	}

	if (rpc.programID == R_PROGRAM) {
		spin_lock_irqsave(&channel->list_lock, flags);
		list_for_each(list, &channel->rtk_ept_lists) {
			rtk_ept = list_entry(list, struct rtk_rpmsg_endpoint, list);
			if (rtk_ept->ept.priv != NULL && *(int *)rtk_ept->ept.priv == REMOTE_ALLOC) {
				dev_dbg(dev, "[%s]find rtk_ept(remote_alloc)\n", __func__);
				break;
			}

			rtk_ept = NULL;
		}
		spin_unlock_irqrestore(&channel->list_lock, flags);

	} else {
		task = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);

		if (task == NULL) {
			dev_err(dev, "[%s]cannot find task by pid :%d\n", __func__, pid);
			goto error;
		}
		addr = task->tgid;
		spin_lock_irqsave(&channel->list_lock, flags);
		list_for_each(list, &channel->rtk_ept_lists) {
			rtk_ept = list_entry(list, struct rtk_rpmsg_endpoint, list);
			if (rtk_ept->ept.addr == addr) {
				dev_dbg(dev, "[%s]find rtk_ept addr:0x%x\n", __func__, rtk_ept->ept.addr);
				break;
			}

			rtk_ept = NULL;
		}
		spin_unlock_irqrestore(&channel->list_lock, flags);
	}



	if (rtk_ept == NULL) {
		dev_err(dev, "[%s] cannnot find ept by addr 0x%x, programID=%d\n",
			__func__, addr, rpc.programID);
		goto error;
	}

	rtk_ept->ept.cb(rtk_ept->ept.rpdev, buf, data_size, rtk_ept->ept.priv, RPMSG_ADDR_ANY);

	kfree(buf);

	if (*ringIn != *ringOut)
		tasklet_schedule(&channel->tasklet);

	return;

error:
	kfree(buf);
	return;
}

void handle_kern_data(unsigned long data)
{

	struct rtk_rpmsg_channel *channel = (struct rtk_rpmsg_channel *)data;
	struct rtk_rpmsg_endpoint *rtk_ept;
	struct device *dev = channel->rcpu->dev;
	struct rpc_struct rpc;
	char *buf;
	char *data_buf;
	unsigned long flags;
	int data_size = 0;
	uint32_t pid = 0;
	uint32_t addr;
	struct list_head *list;
	volatile uint32_t *ringIn, *ringOut, *ringStart, *ringEnd;

	if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
		ringIn = &channel->rx_info.hifi->ringIn;
		ringOut = &channel->rx_info.hifi->ringOut;
		ringStart = &channel->rx_info.hifi->ringStart;
		ringEnd = &channel->rx_info.hifi->ringEnd;
	} else if (channel->id == KR4_ID) {
		ringIn = &channel->rx_info.kr4->ringIn;
		ringOut = &channel->rx_info.kr4->ringOut;
		ringStart = &channel->rx_info.kr4->ringStart;
		ringEnd = &channel->rx_info.kr4->ringEnd;
	} else {
		ringIn = &channel->rx_info.av->ringIn;
		ringOut = &channel->rx_info.av->ringOut;
		ringStart = &channel->rx_info.av->ringStart;
		ringEnd = &channel->rx_info.av->ringEnd;
	}

	spin_lock_irqsave(&channel->rxlock, flags);
	if (*ringIn == *ringOut) {
		spin_unlock_irqrestore(&channel->rxlock, flags);
		return;
	}
	dev_dbg(dev, "[%s]before channel:%s  In:0x%x  Out:0x%x\n", __func__, channel->name, *ringIn, *ringOut);
	buf = get_ring_data(channel, &data_size, &rpc);
	if (IS_ERR(buf)) {
		dev_err(dev, "[%s]cannot get ring buffer data\n", __func__);
		spin_unlock_irqrestore(&channel->rxlock, flags);
		return;
	}
	dev_dbg(dev, "[%s]after channel:%s  In:0x%x  Out:0x%x\n", __func__, channel->name, *ringIn, *ringOut);
	spin_unlock_irqrestore(&channel->rxlock, flags);

	print_rpc_struct(channel, buf);

	data_buf = (buf + sizeof(struct rpc_struct));
	switch (rpc.programID) {
	case R_PROGRAM:
		pid = 0;
		break;
	case AUDIO_AGENT:
	case VIDEO_AGENT:
	case VENC_AGENT:
	case HIFI_AGENT:
	case KR4_AGENT:
		pid = rpc.sysPID;
		break;
	case REPLYID:
		if (channel->rcpu->info->big_endian)
			pid = ntohl(*((uint32_t *)data_buf));
		else
			pid = *((uint32_t *)data_buf);

		break;

	default:
		dev_err(dev, "[%s]unsupport programID:%d\n", __func__, rpc.programID);
		goto error;
	}

	addr = pid;

	spin_lock_irqsave(&channel->list_lock, flags);
	list_for_each(list, &channel->rtk_ept_lists) {
		rtk_ept = list_entry(list, struct rtk_rpmsg_endpoint, list);
		if (rtk_ept->ept.addr == addr) {
			dev_dbg(dev, "[%s]find rtk_ept addr:0x%x\n", __func__, rtk_ept->ept.addr);
			break;
		}

		rtk_ept = NULL;
	}
	spin_unlock_irqrestore(&channel->list_lock, flags);

	if (rtk_ept == NULL) {
		dev_err(dev, "[%s] cannnot find ept by addr 0x%x\n", __func__, addr);
		goto error;
	}

	rtk_ept->ept.cb(rtk_ept->ept.rpdev, buf, data_size, rtk_ept->ept.priv, RPMSG_ADDR_ANY);
	kfree(buf);

	if (*ringIn != *ringOut)
		tasklet_schedule(&channel->tasklet);

	return;

error:
	kfree(buf);
	return;
}


static int check_notify_flag(struct rtk_rcpu *rcpu)
{
	unsigned long flags;

	if (rcpu->rcpu_notify == 0)
		return 0;

	if (RPC_HAS_BIT(rcpu->rcpu_notify, rcpu->info->to_rcpu_feedback_bit)) {
		if (rcpu->hwlock)
			hwspin_lock_timeout_irqsave(rcpu->hwlock, UINT_MAX, &flags);
		RPC_RESET_BIT(rcpu->rcpu_notify, rcpu->info->to_rcpu_feedback_bit);
		if (rcpu->hwlock)
			hwspin_unlock_irqrestore(rcpu->hwlock, &flags);
		return 0;
	}

	return -EINVAL;
}

static irqreturn_t rtk_rcpu_isr(int irq, void *data)
{
	struct rtk_rcpu *rcpu = data;
	struct rtk_rpmsg_channel *channel;
	uint32_t intr_st;

	regmap_read(rcpu->rcpu_intr_regmap, RPC_SB2_INT_ST, &intr_st);

	if (check_notify_flag(rcpu) && !(intr_st & rcpu->info->from_rcpu_intr_bit)) {
		regmap_write(rcpu->rcpu_intr_regmap, RPC_SB2_INT_ST, rcpu->info->from_rcpu_intr_bit);
		return IRQ_HANDLED;
	}

	regmap_write(rcpu->rcpu_intr_regmap, RPC_SB2_INT_ST, rcpu->info->from_rcpu_intr_bit);

	list_for_each_entry(channel, &rcpu->channels, list) {
		channel->handle_data((unsigned long)channel);
	}

	return IRQ_HANDLED;
}

static irqreturn_t rtk_rcpu_ve3_isr(int irq, void *data)
{
	struct rtk_rcpu *rcpu = data;
	struct rtk_rpmsg_channel *channel;
	uint32_t intr_st;

	regmap_read(rcpu->rcpu_intr_regmap, 0x88, &intr_st);

	if (check_notify_flag(rcpu) && !(intr_st & rcpu->info->from_rcpu_intr_bit)) {
		regmap_write(rcpu->rcpu_intr_regmap, 0x88, intr_st & (~rcpu->info->from_rcpu_intr_bit));
		regmap_write(rcpu->rcpu_intr_regmap, 0xe0, 0x0);
		return IRQ_HANDLED;
	}

	regmap_write(rcpu->rcpu_intr_regmap, 0x88, intr_st & (~rcpu->info->from_rcpu_intr_bit));
	regmap_write(rcpu->rcpu_intr_regmap, 0xe0, 0x0);

	list_for_each_entry(channel, &rcpu->channels, list) {
		channel->handle_data((unsigned long)channel);
	}

	return IRQ_HANDLED;
}

static void rtk_rcpu_release(struct device *dev)
{
	//struct rtk_rcpu *rcpu = to_rcpu_info(dev);

	//kfree(rcpu);
}

static void rpmsg_send_interrupt(struct rtk_rcpu *rcpu)
{
	unsigned long flags;

	if (rcpu->rcpu_notify && RPC_HAS_BIT(rcpu->rcpu_notify, rcpu->info->from_rcpu_notify_bit)) {
		if (rcpu->hwlock)
			hwspin_lock_timeout_irqsave(rcpu->hwlock, UINT_MAX, &flags);
		RPC_SET_BIT(rcpu->rcpu_notify, rcpu->info->from_rcpu_feedback_bit);
		if (rcpu->hwlock)
			hwspin_unlock_irqrestore(rcpu->hwlock, &flags);
	}

	regmap_write(rcpu->rcpu_intr_regmap, RPC_SB2_INT, rcpu->info->to_rcpu_intr_bit | RPC_INT_WRITE_1);
}

static void rpmsg_send_ve3_interrupt(struct rtk_rcpu *rcpu)
{
	if (RPC_HAS_BIT(rcpu->rcpu_notify, rcpu->info->from_rcpu_notify_bit))
		RPC_SET_BIT(rcpu->rcpu_notify, rcpu->info->from_rcpu_feedback_bit);

	regmap_write(rcpu->rcpu_intr_regmap, 0x78, RPC_INT_SVE3);

}

static int __rtk_rpmsg_send(struct rtk_rpmsg_channel *channel, const void *data,
			   int len, bool block)
{
	struct rpc_shm_info *tx_info = &channel->tx_info;
	int size = 0;
	int ret = 0;
	int count = 0;
	int tmp;
	int remain_len;
	int ring_buffer_size;
	int ringIn_offset;
	unsigned long flags;
	uint32_t ring_tmp;
	volatile uint32_t *ringIn, *ringOut, *ringStart, *ringEnd;
	uint32_t in, out;

	if (channel->rcpu->status == IS_DISCONNECTED) {
		dev_err(channel->rcpu->dev, "cannot send rpc, remote cpu init failed\n");
		return -EINVAL;
	}

	if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
		ringIn = &tx_info->hifi->ringIn;
		ringOut = &tx_info->hifi->ringOut;
		ringStart = &tx_info->hifi->ringStart;
		ringEnd = &tx_info->hifi->ringEnd;
	} else if (channel->id == KR4_ID) {
		ringIn = &tx_info->kr4->ringIn;
		ringOut = &tx_info->kr4->ringOut;
		ringStart = &tx_info->kr4->ringStart;
		ringEnd = &tx_info->kr4->ringEnd;
	} else {
		ringIn = &tx_info->av->ringIn;
		ringOut = &tx_info->av->ringOut;
		ringStart = &tx_info->av->ringStart;
		ringEnd = &tx_info->av->ringEnd;
	}

	spin_lock_irqsave(&channel->txlock, flags);

	dev_dbg(channel->rcpu->dev, "[%s]before write channel name:%s ringIn:0x%x ringOut:0x%x len:0x%x\n", __func__, channel->name, *ringIn, *ringOut, len);

	ring_buffer_size = *ringEnd - *ringStart;
	in = *ringIn;
	out = *ringOut;
	if (in == out)
		size = ring_buffer_size;
	else if (in > out)
		size = ring_buffer_size - (in - out);
	else
		size = out - in;


	if (len > size - 1) {
		dev_err(channel->rcpu->dev, "rpc ring buffer is full(len:0x%x  size:0x%x)\n", len, size);
		ret = -EAGAIN;
		goto out;
	}

	ringIn_offset = *ringIn - *ringStart;
	tmp = *ringEnd - *ringIn;
	if (tmp >= len) {
		memcpy_toio((void *)(channel->tx_fifo + ringIn_offset), data, len);
		count += len;
		ring_tmp = *ringIn + ((len + 3) & 0xfffffffc);

		if (ring_tmp == *ringEnd)
			*ringIn = *ringStart;
		else
			*ringIn = ring_tmp;
	} else {
		memcpy_toio((void *)(channel->tx_fifo + ringIn_offset), data, tmp);
		count += tmp;
		remain_len = len - tmp;
		memcpy_toio((void *)(channel->tx_fifo), (void *)(data + tmp), remain_len);
		count += remain_len;
		*ringIn = *ringStart + ((remain_len + 3) & 0xfffffffc);
	}
	ret = count;
	channel->rcpu->info->send_interrupt(channel->rcpu);

	dev_dbg(channel->rcpu->dev, "[%s]after write channel name:%s ringIn:0x%x ringOut:0x%x len:0x%x\n", __func__, channel->name, *ringIn, *ringOut, len);

out:
	spin_unlock_irqrestore(&channel->txlock, flags);
	return ret;

}


void rcpu_set_flag(struct rtk_rcpu *rcpu, uint32_t flag)
{
	writel(__cpu_to_be32(flag), rcpu->sync_flag);
}

uint32_t rcpu_get_flag(struct rtk_rcpu *rcpu)
{
	return readl(rcpu->sync_flag);
}

/*static void populate_rpdev_device(struct rtk_rcpu *rcpu)
{
	struct rtk_rpmsg_channel *channel;
	int ret = 0;

	list_for_each_entry(channel, &rcpu->channels, list) {
		struct rpmsg_device *rpdev = &channel->rtk_rpdev->rpdev;
		ret = of_platform_populate(rpdev->dev.of_node, NULL, NULL, &rpdev->dev);
		if (ret) {
			dev_err(rcpu->dev, "populate child device failed (channel:%s)\n", channel->name);
		}
	}
}*/

static int rtk_kick_rcpu(struct rtk_rcpu *rcpu)
{
	int timeout = 3000;
	uint32_t intr_en_reg;

	if (rcpu->info->id == VE3_ID) {
		regmap_read(rcpu->rcpu_intr_regmap, RPC_INT_VE3_REG, &intr_en_reg);
		regmap_write(rcpu->rcpu_intr_regmap, RPC_INT_VE3_REG, rcpu->info->intr_en | intr_en_reg);
	} else {
		regmap_write(rcpu->rcpu_intr_regmap, RPC_SB2_INT_EN, rcpu->info->intr_en | RPC_INT_WRITE_EN);
	}

	if (rcpu->rcpu_notify)
		RPC_SET_BIT(rcpu->rcpu_notify, rcpu->info->to_rcpu_notify_bit);

	rcpu_set_flag(rcpu, 0xffffffff);
	rcpu->info->send_interrupt(rcpu);

	if (rcpu->skip_handshake) {
		dev_dbg(rcpu->dev, "skip the handshake check\n");
		rcpu->status = IS_CONNECTED;
		return 0;
	}

	while ((rcpu_get_flag(rcpu) == 0xffffffff) && ((timeout--) > 0))
		mdelay(1);


	if (timeout <= 0) {
		rcpu->status = IS_DISCONNECTED;
	} else {
		rcpu->status = IS_CONNECTED;
		//populate_rpdev_device(rcpu);
	}

	dev_err(rcpu->dev, "%s %s (SYNC_FLAG = 0x%08x)\n",
		rcpu->dev->of_node->name,
		(timeout > 0) ? "OK" : "timeout", rcpu_get_flag(rcpu));

	return 0;
}


static int rtk_rpmsg_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct rtk_rpmsg_endpoint *rtk_ept = to_rtk_ept(ept);
	struct rtk_rpmsg_channel *channel = rtk_ept->channel;

	return __rtk_rpmsg_send(channel, data, len, false);
}

static int rtk_rpmsg_sendto(struct rpmsg_endpoint *ept, void *data, int len, u32 dst)
{
	struct rtk_rpmsg_endpoint *rtk_ept = to_rtk_ept(ept);
	struct rtk_rpmsg_channel *channel = rtk_ept->channel;

	return __rtk_rpmsg_send(channel, data, len, false);
}


static int rtk_rpmsg_trysend(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct rtk_rpmsg_endpoint *rtk_ept = to_rtk_ept(ept);
	struct rtk_rpmsg_channel *channel = rtk_ept->channel;

	return __rtk_rpmsg_send(channel, data, len, true);
}

static int rtk_rpmsg_trysendto(struct rpmsg_endpoint *ept, void *data, int len, u32 dst)
{
	struct rtk_rpmsg_endpoint *rtk_ept = to_rtk_ept(ept);
	struct rtk_rpmsg_channel *channel = rtk_ept->channel;

	return __rtk_rpmsg_send(channel, data, len, true);
}

#if 0
static int rtk_rpmsg_set_signals(struct rpmsg_endpoint *ept, u32 set, u32 clear)
{
	struct rtk_rpmsg_endpoint *rtk_ept = to_rtk_ept(ept);
	struct rtk_rpmsg_channel *channel = rtk_ept->channel;

	if (set & REMOTE_INIT)
		return rtk_kick_rcpu(channel->rcpu);

	return -EINVAL;
}
#endif
static void __ept_release(struct kref *kref)
{
	struct rpmsg_endpoint *ept = container_of(kref, struct rpmsg_endpoint,
						  refcount);
	struct rtk_rpmsg_endpoint *rtk_ept =  container_of(ept, struct rtk_rpmsg_endpoint, ept);
	struct rtk_rpmsg_channel *channel = rtk_ept->channel;
	unsigned long flags;

	spin_lock_irqsave(&channel->list_lock, flags);
	list_del(&rtk_ept->list);
	spin_unlock_irqrestore(&channel->list_lock, flags);

	kfree(rtk_ept);
}

static void rtk_rpmsg_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct rtk_rpmsg_endpoint *rtk_ept =  container_of(ept, struct rtk_rpmsg_endpoint, ept);
	struct rtk_rcpu *rcpu = rtk_ept->channel->rcpu;

	dev_dbg(rcpu->dev, "[%s] channel:%s tgid:%d pid:%d\n", __func__, rtk_ept->channel->name, current->tgid, current->pid);

	kref_put(&ept->refcount, __ept_release);
}

static const struct rpmsg_endpoint_ops rtk_rpc_endpoint_ops = {
	.destroy_ept = rtk_rpmsg_destroy_ept,
	.send = rtk_rpmsg_send,
	.sendto = rtk_rpmsg_sendto,
	.trysend = rtk_rpmsg_trysend,
	.trysendto = rtk_rpmsg_trysendto,
	//.set_signals = rtk_rpmsg_set_signals,
};

static struct rtk_rpmsg_channel *rtk_find_channel(struct rtk_rcpu *rcpu, const char *name)
{
	struct rtk_rpmsg_channel *channel;
	struct rtk_rpmsg_channel *ret = NULL;

	list_for_each_entry(channel, &rcpu->channels, list) {
		if (!strcmp(channel->name, name)) {
			ret = channel;
			break;
		}
	}

	return ret;
}

static struct rpmsg_endpoint *rtk_rpc_create_ept(struct rpmsg_device *rpdev,
						  rpmsg_rx_cb_t cb, void *priv,
						  struct rpmsg_channel_info chinfo)
{
	struct rtk_rpmsg_endpoint *rtk_ept;
	struct rtk_rpmsg_channel *channel;
	struct rtk_rpmsg_device *rtk_rpdev = to_rtk_rpdevice(rpdev);
	struct rtk_rcpu *rcpu = rtk_rpdev->rcpu;
	struct rpmsg_endpoint *ept;
	const char *name = chinfo.name;
	int id = 0;
	unsigned long flags;

	dev_dbg(rcpu->dev, "comm:%s tgid:%d pid:%d\n", current->comm, current->tgid, current->pid);

	channel = rtk_find_channel(rcpu, name);
	if (channel == NULL) {
		dev_err(rcpu->dev, "[%s] cannot find specific channel:%s\n", __func__, name);
		return NULL;
	}

	rtk_ept = kzalloc(sizeof(*rtk_ept), GFP_KERNEL);
	if (!rtk_ept)
		return ERR_PTR(-ENOMEM);

	ept = &rtk_ept->ept;
	kref_init(&ept->refcount);

	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &rtk_rpc_endpoint_ops;
	if (channel->use_idr) {
		mutex_lock(&channel->ept_ids_lock);
		id = idr_alloc(&channel->ept_ids, rtk_ept, IDR_MIN, IDR_MAX, GFP_KERNEL);
		if (id < 0) {
			dev_err(rcpu->dev, "idr_alloc failed: %d\n", id);
			mutex_unlock(&channel->ept_ids_lock);
			goto free_ept;
		}
		mutex_unlock(&channel->ept_ids_lock);
	} else {
		id = current->tgid;
	}
	ept->addr = id;

	spin_lock_irqsave(&channel->list_lock, flags);
	list_add_tail(&rtk_ept->list, &channel->rtk_ept_lists);
	spin_unlock_irqrestore(&channel->list_lock, flags);
	rtk_ept->channel = channel;

	return ept;

free_ept:
	kref_put(&ept->refcount, __ept_release);
	return NULL;
}

static int rtk_rpc_announce_create(struct rpmsg_device *rpdev)
{
	struct rtk_rpmsg_device *rtk_rpdev = to_rtk_rpdevice(rpdev);

	if (strstr(rpdev->id.name, "intr"))
		rtk_kick_rcpu(rtk_rpdev->rcpu);

	return 0;
}

static const struct rpmsg_device_ops rtk_rpmsg_device_ops = {
	.create_ept = rtk_rpc_create_ept,
	.announce_create = rtk_rpc_announce_create,
};

static int rtk_create_chrdev(struct rtk_rcpu *rcpu)
{
	struct rtk_rpmsg_device *rtk_rpdev;

	rtk_rpdev = kzalloc(sizeof(*rtk_rpdev), GFP_KERNEL);
	if (!rtk_rpdev)
		return -ENOMEM;

	rtk_rpdev->rcpu = rcpu;
	rtk_rpdev->rpdev.ops = &rtk_rpmsg_device_ops;
	rtk_rpdev->rpdev.dev.parent = rcpu->dev;
	rtk_rpdev->rpdev.dev.release = rtk_rcpu_release;
	rtk_rpdev->rpdev.src = rcpu->info->id;
	rtk_rpdev->rpdev.little_endian = !rcpu->info->big_endian;

	return rpmsg_ctrldev_register_device(&rtk_rpdev->rpdev);
}

static void rtk_rpc_release_device(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);
	struct rtk_rpmsg_device *rtk_rpdev = to_rtk_rpdevice(rpdev);

	kfree(rtk_rpdev);
}

static int rtk_rpc_create_deivce(struct rtk_rpmsg_channel *channel, struct device_node *node)
{
	struct rtk_rpmsg_device *rtk_rpdev;
	struct rpmsg_device *rpdev;
	struct rtk_rcpu *rcpu = channel->rcpu;
	int ret = 0;

	rtk_rpdev = kzalloc(sizeof(*rtk_rpdev), GFP_KERNEL);
	if (!rtk_rpdev)
		return -ENOMEM;

	rtk_rpdev->rcpu = rcpu;
	rtk_rpdev->rpdev.ops = &rtk_rpmsg_device_ops;
	channel->rtk_rpdev = rtk_rpdev;

	rpdev = &rtk_rpdev->rpdev;
	strscpy(rpdev->id.name, channel->name, RPMSG_NAME_SIZE);
	rpdev->src = RPMSG_ADDR_ANY;
	rpdev->dst = RPMSG_ADDR_ANY;

	rpdev->dev.of_node = node;
	rpdev->dev.parent = rcpu->dev;
	rpdev->dev.release = rtk_rpc_release_device;
	rpdev->little_endian = !rcpu->info->big_endian;

	ret = rpmsg_register_device(rpdev);
	if (ret) {
		dev_err(rcpu->dev, "rpmsg device register failed (channel:%s)\n", channel->name);
		kfree(rtk_rpdev);
		return ret;
	}

	ret = of_platform_populate(rpdev->dev.of_node, NULL, NULL, &rpdev->dev);
	if (ret) {
		dev_err(rcpu->dev, "populate child device failed (channel:%s)\n", channel->name);
	}

	return ret;

}

static int rpmsg_debug_show(struct seq_file *s, void *unused)
{
	struct rtk_rpmsg_channel *channel = (struct rtk_rpmsg_channel *)s->private;
	int ringSize;
	int i;
	uint32_t *addr;
	volatile uint32_t *ringIn, *ringOut, *ringStart, *ringEnd, *ringBuf;

	if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
		ringBuf = &channel->tx_info.hifi->ringBuf;
		ringIn = &channel->tx_info.hifi->ringIn;
		ringOut = &channel->tx_info.hifi->ringOut;
		ringStart = &channel->tx_info.hifi->ringStart;
		ringEnd = &channel->tx_info.hifi->ringEnd;
	} else if (channel->id == KR4_ID) {
		ringBuf = &channel->tx_info.kr4->ringBuf;
		ringIn = &channel->tx_info.kr4->ringIn;
		ringOut = &channel->tx_info.kr4->ringOut;
		ringStart = &channel->tx_info.kr4->ringStart;
		ringEnd = &channel->tx_info.kr4->ringEnd;
	} else {
		ringBuf = &channel->tx_info.av->ringBuf;
		ringIn = &channel->tx_info.av->ringIn;
		ringOut = &channel->tx_info.av->ringOut;
		ringStart = &channel->tx_info.av->ringStart;
		ringEnd = &channel->tx_info.av->ringEnd;
	}

	seq_printf(s, "name: %s\n", channel->name);
	seq_puts(s, "TX ringBuffer\n");
	seq_printf(s, "RingBuf: %x\n", *ringBuf);
	seq_printf(s, "RingStart: %x\n", *ringStart);
	seq_printf(s, "RingIn: %x\n", *ringIn);
	seq_printf(s, "RingOut: %x\n", *ringOut);
	seq_printf(s, "RingEnd: %x\n", *ringEnd);

	seq_puts(s, "\nRingBuffer:\n");
	ringSize = *ringEnd - *ringStart;

	for (i = 0; i < ringSize; i += 16) {
		addr = (uint32_t *)(channel->tx_fifo + i);

		if (channel->rcpu->info->big_endian)
			seq_printf(s, "%x: %08x %08x %08x %08x\n", *ringStart + i, ntohl(*(addr + 0)),
					ntohl(*(addr + 1)), ntohl(*(addr + 2)), ntohl(*(addr + 3)));
		else
			seq_printf(s, "%x: %08x %08x %08x %08x\n", *ringStart + i, *(addr + 0),
					*(addr + 1), *(addr + 2), *(addr + 3));
	}

	if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
		ringBuf = &channel->rx_info.hifi->ringBuf;
		ringIn = &channel->rx_info.hifi->ringIn;
		ringOut = &channel->rx_info.hifi->ringOut;
		ringStart = &channel->rx_info.hifi->ringStart;
		ringEnd = &channel->rx_info.hifi->ringEnd;
	} else if (channel->id == KR4_ID) {
		ringBuf = &channel->rx_info.kr4->ringBuf;
		ringIn = &channel->rx_info.kr4->ringIn;
		ringOut = &channel->rx_info.kr4->ringOut;
		ringStart = &channel->rx_info.kr4->ringStart;
		ringEnd = &channel->rx_info.kr4->ringEnd;
	} else {
		ringBuf = &channel->rx_info.av->ringBuf;
		ringIn = &channel->rx_info.av->ringIn;
		ringOut = &channel->rx_info.av->ringOut;
		ringStart = &channel->rx_info.av->ringStart;
		ringEnd = &channel->rx_info.av->ringEnd;
	}

	seq_printf(s, "name: %s\n", channel->name);
	seq_puts(s, "RX ringBuffer\n");
	seq_printf(s, "RingBuf: %x\n", *ringBuf);
	seq_printf(s, "RingStart: %x\n", *ringStart);
	seq_printf(s, "RingIn: %x\n", *ringIn);
	seq_printf(s, "RingOut: %x\n", *ringOut);
	seq_printf(s, "RingEnd: %x\n", *ringEnd);

	seq_puts(s, "\nRingBuffer:\n");
	ringSize = *ringEnd - *ringStart;

	for (i = 0; i < ringSize; i += 16) {
		addr = (uint32_t *)(channel->rx_fifo + i);

		if (channel->rcpu->info->big_endian)
			seq_printf(s, "%x: %08x %08x %08x %08x\n", *ringStart + i, ntohl(*(addr + 0)),
					ntohl(*(addr + 1)), ntohl(*(addr + 2)), ntohl(*(addr + 3)));
		else
			seq_printf(s, "%x: %08x %08x %08x %08x\n", *ringStart + i, *(addr + 0),
					*(addr + 1), *(addr + 2), *(addr + 3));
	}


	return 0;
}

static int rpmsg_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, rpmsg_debug_show, inode->i_private);
}

static const struct file_operations rpmsg_debug_ops = {
	.open = rpmsg_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct rtk_rpmsg_channel *rtk_rpc_create_channel(struct device_node *node,
							struct rtk_rcpu *rcpu,
							struct dentry *rpmsg_dir)
{
	struct rtk_rpmsg_channel *channel;
	struct device *dev = rcpu->dev;
	const char *name;
	const u32 *prop;
	int size;
	int ret = 0;
	u32 tx_info, rx_info, tx_fifo, rx_fifo, tx_fifo_size, rx_fifo_size;

	channel = kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel)
		return ERR_PTR(-ENOMEM);

	prop = of_get_property(node, "tx-info", &size);
	if (prop) {
		tx_info = of_read_number(prop, 1);
	} else {
		dev_err(dev, "[%s]cannot get channel info tx_info\n", __func__);
		goto free_channel;
	}
	prop = of_get_property(node, "rx-info", &size);
	if (prop) {
		rx_info = of_read_number(prop, 1);
	} else {
		dev_err(dev, "[%s]cannot get channel info rx_info\n", __func__);
		goto free_channel;
	}
	prop = of_get_property(node, "tx-fifo", &size);
	if (prop) {
		tx_fifo = of_read_number(prop, 1);
	} else {
		dev_err(dev, "[%s]cannot get channel info tx_fifo\n", __func__);
		goto free_channel;
	}
	prop = of_get_property(node, "rx-fifo", &size);
	if (prop) {
		rx_fifo = of_read_number(prop, 1);
	} else {
		dev_err(dev, "[%s]cannot get channel info rx_fifo\n", __func__);
		goto free_channel;
	}
	prop = of_get_property(node, "tx-fifo-size", &size);
	if (prop) {
		tx_fifo_size = of_read_number(prop, 1);
	} else {
		dev_err(dev, "[%s]cannot get channel info tx_fifo_size\n", __func__);
		goto free_channel;
	}
	prop = of_get_property(node, "rx-fifo-size", &size);
	if (prop) {
		rx_fifo_size = of_read_number(prop, 1);
	} else {
		dev_err(dev, "[%s]cannot get channel info rx_fifo_size\n", __func__);
		goto free_channel;
	}
	ret = of_property_read_string(node, "name", &name);
	if (ret < 0) {
		dev_err(dev, "[%s]cannot get channel info name\n", __func__);
		goto free_channel;
	}

	INIT_LIST_HEAD(&channel->rtk_ept_lists);
	spin_lock_init(&channel->txlock);
	spin_lock_init(&channel->rxlock);
	spin_lock_init(&channel->list_lock);

	channel->id = rcpu->info->id;
	strscpy(channel->name, name, RPMSG_NAME_SIZE);
	channel->tx_fifo = ringbuf_phys_to_virt(tx_fifo);
	channel->rx_fifo = ringbuf_phys_to_virt(rx_fifo);

	if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
		channel->tx_info.hifi = ringbuf_phys_to_virt(tx_info);
		channel->tx_info.hifi->ringBuf = tx_fifo;
		channel->tx_info.hifi->ringStart = tx_fifo;
		channel->tx_info.hifi->ringIn = tx_fifo;
		channel->tx_info.hifi->ringOut = tx_fifo;
		channel->tx_info.hifi->ringEnd = tx_fifo + tx_fifo_size;
		channel->rx_info.hifi = ringbuf_phys_to_virt(rx_info);
		channel->rx_info.hifi->ringBuf = rx_fifo;
		channel->rx_info.hifi->ringStart = rx_fifo;
		channel->rx_info.hifi->ringIn = rx_fifo;
		channel->rx_info.hifi->ringOut = rx_fifo;
		channel->rx_info.hifi->ringEnd = rx_fifo + rx_fifo_size;
	} else if (channel->id == KR4_ID) {
		channel->tx_info.kr4 = ringbuf_phys_to_virt(tx_info);
		channel->tx_info.kr4->ringBuf = tx_fifo;
		channel->tx_info.kr4->ringStart = tx_fifo;
		channel->tx_info.kr4->ringIn = tx_fifo;
		channel->tx_info.kr4->ringOut = tx_fifo;
		channel->tx_info.kr4->ringEnd = tx_fifo + tx_fifo_size;
		channel->rx_info.kr4 = ringbuf_phys_to_virt(rx_info);
		channel->rx_info.kr4->ringBuf = rx_fifo;
		channel->rx_info.kr4->ringStart = rx_fifo;
		channel->rx_info.kr4->ringIn = rx_fifo;
		channel->rx_info.kr4->ringOut = rx_fifo;
		channel->rx_info.kr4->ringEnd = rx_fifo + rx_fifo_size;
	} else {
		channel->tx_info.av = ringbuf_phys_to_virt(tx_info);
		channel->tx_info.av->ringBuf = tx_fifo;
		channel->tx_info.av->ringStart = tx_fifo;
		channel->tx_info.av->ringIn = tx_fifo;
		channel->tx_info.av->ringOut = tx_fifo;
		channel->tx_info.av->ringEnd = tx_fifo + tx_fifo_size;
		channel->rx_info.av = ringbuf_phys_to_virt(rx_info);
		channel->rx_info.av->ringBuf = rx_fifo;
		channel->rx_info.av->ringStart = rx_fifo;
		channel->rx_info.av->ringIn = rx_fifo;
		channel->rx_info.av->ringOut = rx_fifo;
		channel->rx_info.av->ringEnd = rx_fifo + rx_fifo_size;
	}

	if (strstr(channel->name, "kernel")) {
		channel->handle_data = &handle_kern_data;
		channel->use_idr = 1;
		idr_init(&channel->ept_ids);
		mutex_init(&channel->ept_ids_lock);
	} else {
		channel->handle_data = &handle_intr_data;
		channel->use_idr = 0;
	}

	channel->rcpu = rcpu;
	channel->debugfs_node = debugfs_create_file(channel->name, 0444, rpmsg_dir,
						    channel, &rpmsg_debug_ops);
	tasklet_init(&channel->tasklet, channel->handle_data, (unsigned long)channel);
	list_add(&channel->list, &rcpu->channels);
	rtk_rpc_create_deivce(channel, node);

	return channel;
free_channel:
	kfree(channel);

	return ERR_PTR(-EPERM);
}

static int rcpu_rpmsg_init(struct rtk_rcpu *rcpu)
{
	struct device_node *child_node;
	struct rtk_ipc_shm __iomem *ipc = (void __iomem *)IPC_SHM_VIRT;
	struct device_node *syscon_np;
	struct dentry *rpmsg_dir;
	int lock_id;
	int ret = 0;
	int irq;

	rcpu->status = IS_UNINITIALIZED;

	if (rcpu->info->id == AUDIO_ID) {
		rcpu->rcpu_notify = &ipc->vo_int_sync;
		rcpu->sync_flag = &ipc->audio_rpc_flag;
	} else if (rcpu->info->id == VIDEO_ID) {
		rcpu->rcpu_notify = &ipc->video_int_sync;
		rcpu->sync_flag = &ipc->video_rpc_flag;
	} else if (rcpu->info->id == VE3_ID) {
		rcpu->rcpu_notify = &ipc->ve3_int_sync;
		rcpu->sync_flag = &ipc->ve3_rpc_flag;
	} else if (rcpu->info->id == HIFI_ID) {
		rcpu->rcpu_notify = 0;
		rcpu->sync_flag = &ipc->hifi_rpc_flag;
	}  else if (rcpu->info->id == HIFI1_ID) {
		rcpu->rcpu_notify = 0;
		rcpu->sync_flag = &ipc->hifi1_rpc_flag;
	}  else if (rcpu->info->id == KR4_ID) {
		rcpu->rcpu_notify = 0;
		rcpu->sync_flag = &ipc->kr4_rpc_flag;
	}

	syscon_np = of_parse_phandle(rcpu->dev->of_node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np)) {
		ret = -ENODEV;
		goto err;
	}
	rcpu->rcpu_intr_regmap = syscon_node_to_regmap(syscon_np);
	if (IS_ERR_OR_NULL(rcpu->rcpu_intr_regmap)) {
		of_node_put(syscon_np);
		ret = -EINVAL;
		goto err;
	}
	lock_id = of_hwspin_lock_get_id(rcpu->dev->of_node, 0);
	if (lock_id > 0 || (IS_ENABLED(CONFIG_HWSPINLOCK) && lock_id == 0)) {
		struct hwspinlock *lock = devm_hwspin_lock_request_specific(rcpu->dev, lock_id);
		if (lock) {
			dev_info(rcpu->dev, "use hwlock%d\n", lock_id);
			rcpu->hwlock = lock;
		}
	} else {
		if (lock_id != -ENOENT)
			dev_err(rcpu->dev, "failed to get hwlock: %pe\n", ERR_PTR(lock_id));
	}

	INIT_LIST_HEAD(&rcpu->channels);

	irq = irq_of_parse_and_map(rcpu->dev->of_node, 0);
	if (irq < 0) {
		dev_err(rcpu->dev, "[%s]required rpc interrupt missing\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	ret = devm_request_irq(rcpu->dev, irq,
		rcpu->info->isr, IRQF_SHARED|IRQF_NO_SUSPEND,
		rcpu->info->name, rcpu);
	if (ret) {
		dev_err(rcpu->dev, "[%s]failed to request rpc irq\n", __func__);
		goto err;
	}

	rcpu->irq = irq;

	if (of_find_property(rcpu->dev->of_node, "skip-handshake", NULL))
		rcpu->skip_handshake = true;
	else
		rcpu->skip_handshake = false;

	rpmsg_dir = debugfs_lookup("rpmsg", NULL);
	if (!rpmsg_dir)
		rpmsg_dir = debugfs_create_dir("rpmsg", NULL);

	for_each_available_child_of_node(rcpu->dev->of_node, child_node)
		rtk_rpc_create_channel(child_node, rcpu, rpmsg_dir);

	ret = rtk_create_chrdev(rcpu);
	if (ret) {
		dev_err(rcpu->dev, "create char device failed (%s)\n", rcpu->info->name);
		goto err;
	}
	dev_info(rcpu->dev, "probed\n");

	return 0;

 err:
	kfree(rcpu);

	return ret;
}


int check_rcpu_status(struct device *dev)
{
	struct rtk_rcpu *rcpu = dev_get_drvdata(dev);

	return rcpu->status;
}
EXPORT_SYMBOL_GPL(check_rcpu_status);

void rtk_dump_all_ringbuf_info(struct device *dev)
{
	struct rtk_rcpu *rcpu = dev_get_drvdata(dev->parent);
	struct rtk_rpmsg_channel *channel;
	int i = 0;
	uint32_t *addr;
	volatile uint32_t *ringIn, *ringOut, *ringStart, *ringEnd;

	list_for_each_entry(channel, &rcpu->channels, list) {
		if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
			ringIn = &channel->tx_info.hifi->ringIn;
			ringOut = &channel->tx_info.hifi->ringOut;
			ringStart = &channel->tx_info.hifi->ringStart;
			ringEnd = &channel->tx_info.hifi->ringEnd;
		} else if (channel->id == KR4_ID) {
			ringIn = &channel->tx_info.kr4->ringIn;
			ringOut = &channel->tx_info.kr4->ringOut;
			ringStart = &channel->tx_info.kr4->ringStart;
			ringEnd = &channel->tx_info.kr4->ringEnd;
		} else {
			ringIn = &channel->tx_info.av->ringIn;
			ringOut = &channel->tx_info.av->ringOut;
			ringStart = &channel->tx_info.av->ringStart;
			ringEnd = &channel->tx_info.av->ringEnd;
		}
		pr_err("============================================\n");
		pr_err("name: %s\n", channel->name);
		pr_err("TX ring buffer:\n");
		pr_err("ringStart: 0x%x\n", *ringStart);
		pr_err("ringEnd: 0x%x\n", *ringEnd);
		pr_err("ringIn: 0x%x\n", *ringIn);
		pr_err("ringOut: 0x%x\n", *ringOut);
		pr_err("ring data:\n");
		for (i = 0; i < *ringEnd - *ringStart ; i += 16) {
			addr = (uint32_t *)(channel->tx_fifo + i);
			if (rcpu->info->big_endian)
				pr_err("%x: %08x %08x %08x %08x\n", *ringStart + i, ntohl(*(addr + 0)),
					ntohl(*(addr + 1)), ntohl(*(addr + 2)), ntohl(*(addr + 3)));
			else
				pr_err("%x: %08x %08x %08x %08x\n", *ringStart + i, *(addr + 0),
					*(addr + 1), *(addr + 2), *(addr + 3));
		}

		if (channel->id == HIFI_ID || channel->id == HIFI1_ID) {
			ringIn = &channel->rx_info.hifi->ringIn;
			ringOut = &channel->rx_info.hifi->ringOut;
			ringStart = &channel->rx_info.hifi->ringStart;
			ringEnd = &channel->rx_info.hifi->ringEnd;
		} else if (channel->id == KR4_ID) {
			ringIn = &channel->rx_info.kr4->ringIn;
			ringOut = &channel->rx_info.kr4->ringOut;
			ringStart = &channel->rx_info.kr4->ringStart;
			ringEnd = &channel->rx_info.kr4->ringEnd;
		} else {
			ringIn = &channel->rx_info.av->ringIn;
			ringOut = &channel->rx_info.av->ringOut;
			ringStart = &channel->rx_info.av->ringStart;
			ringEnd = &channel->rx_info.av->ringEnd;
		}
		pr_err("RX ring buffer:\n");
		pr_err("ringStart: 0x%x\n", *ringStart);
		pr_err("ringEnd: 0x%x\n", *ringEnd);
		pr_err("ringIn: 0x%x\n", *ringIn);
		pr_err("ringOut: 0x%x\n", *ringOut);
		pr_err("ring data:\n");
		for (i = 0; i < *ringEnd - *ringStart ; i += 16) {
			addr = (uint32_t *)(channel->rx_fifo + i);
			if (rcpu->info->big_endian)
				pr_err("%x: %08x %08x %08x %08x\n", *ringStart + i, ntohl(*(addr + 0)),
					ntohl(*(addr + 1)), ntohl(*(addr + 2)), ntohl(*(addr + 3)));
			else
				pr_err("%x: %08x %08x %08x %08x\n", *ringStart + i, *(addr + 0),
					*(addr + 1), *(addr + 2), *(addr + 3));
		}
		pr_err("============================================\n");
	}

}
EXPORT_SYMBOL(rtk_dump_all_ringbuf_info);

struct rproc *check_rproc_state(struct device *dev)
{
	struct rproc *rproc;
	phandle phandle;
	struct device_node *node;

	if (of_property_read_u32(dev->of_node, "rproc", &phandle)) {
		dev_dbg(dev, "could not get %s rproc phandle, skipping\n", dev->of_node->name);
		return 0;
	}

	node = of_find_node_by_phandle(phandle);
	if (node) {
		if (!of_device_is_available(node)) {
			dev_err(dev, "rproc is disabled, do not probe\n");
			return ERR_PTR(-ENODEV);
		}
	} else {
		return ERR_PTR(-ENODEV);
	}

	rproc = rproc_get_by_phandle(phandle);
	if (!rproc) {
		dev_err(dev, "could not get %s rproc handle\n", dev->of_node->name);
		return ERR_PTR(-EPROBE_DEFER);
	}

	if (rproc->state != RPROC_RUNNING) {
		dev_dbg(dev, "%s rproc is not ready\n", dev->of_node->name);
		return ERR_PTR(-EPROBE_DEFER);
	}

	return rproc;
}

static int rtk_rcpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rtk_rcpu *rcpu;
	struct rproc *rproc;
	int ret = 0;

	rproc = check_rproc_state(dev);
	if (IS_ERR(rproc))
		return PTR_ERR(rproc);

	rcpu = kzalloc(sizeof(*rcpu), GFP_KERNEL);
	if (!rcpu)
		return -ENOMEM;

	rcpu->dev = dev;
	rcpu->info = device_get_match_data(dev);
	rcpu->rproc = rproc;
	dev_set_drvdata(dev, rcpu);

	ret = rcpu_rpmsg_init(rcpu);
	if (ret) {
		dev_err(dev, "rcpu initialize failed");
		return ret;
	}

	return ret;
}


static int rtk_rcpu_remove(struct platform_device *pdev)
{
	struct rtk_rcpu *rcpu = platform_get_drvdata(pdev);

	regmap_write(rcpu->rcpu_intr_regmap, RPC_SB2_INT_EN, rcpu->info->intr_en);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rtk_rcpu_suspend(struct device *dev)
{
	struct rtk_rcpu *rcpu = dev_get_drvdata(dev);

	if (!rcpu->rproc)
		regmap_write(rcpu->rcpu_intr_regmap, RPC_SB2_INT_EN, rcpu->info->intr_en);

	return 0;
}

static int rtk_rcpu_resume(struct device *dev)
{

	struct rtk_rcpu *rcpu = dev_get_drvdata(dev);

	if (!rcpu->rproc)
		regmap_write(rcpu->rcpu_intr_regmap, RPC_SB2_INT_EN, rcpu->info->intr_en | RPC_INT_WRITE_EN);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rtk_rcpu_pm_ops, rtk_rcpu_suspend,
			 rtk_rcpu_resume);
#else
static const struct dev_pm_ops rtk_rcpu_pm_ops = {};
#endif

static const struct remote_cpu_info acpu_info = {
	.name = "acpu-rpc",
	.id = AUDIO_ID,
	.big_endian = 1,
	.from_rcpu_notify_bit = AUDIO_RPC_SET_NOTIFY,
	.from_rcpu_feedback_bit = AUDIO_RPC_FEEDBACK_NOTIFY,
	.to_rcpu_notify_bit = RPC_AUDIO_SET_NOTIFY,
	.to_rcpu_feedback_bit = RPC_AUDIO_FEEDBACK_NOTIFY,
	.to_rcpu_intr_bit = RPC_INT_SA,
	.from_rcpu_intr_bit = RPC_INT_AS,
	.intr_en = RPC_INT_ACPU_EN,
	.isr = &rtk_rcpu_isr,
	.send_interrupt = &rpmsg_send_interrupt,
};

static const struct remote_cpu_info vcpu_info = {
	.name = "vcpu-rpc",
	.id = VIDEO_ID,
	.big_endian = 1,
	.from_rcpu_notify_bit = VIDEO_RPC_SET_NOTIFY,
	.from_rcpu_feedback_bit = VIDEO_RPC_FEEDBACK_NOTIFY,
	.to_rcpu_notify_bit = RPC_VIDEO_SET_NOTIFY,
	.to_rcpu_feedback_bit = RPC_VIDEO_FEEDBACK_NOTIFY,
	.to_rcpu_intr_bit = RPC_INT_SV,
	.from_rcpu_intr_bit = RPC_INT_VS,
	.intr_en = RPC_INT_VCPU_EN,
	.isr = &rtk_rcpu_isr,
	.send_interrupt = &rpmsg_send_interrupt,
};

static const struct remote_cpu_info ve3_info = {
	.name = "ve3-rpc",
	.id = VE3_ID,
	.big_endian = 1,
	.from_rcpu_notify_bit = VE3_RPC_SET_NOTIFY,
	.from_rcpu_feedback_bit = VE3_RPC_FEEDBACK_NOTIFY,
	.to_rcpu_notify_bit = RPC_VE3_SET_NOTIFY,
	.to_rcpu_feedback_bit = RPC_VE3_FEEDBACK_NOTIFY,
	.to_rcpu_intr_bit = RPC_INT_SVE3,
	.from_rcpu_intr_bit = RPC_INT_VE3S,
	.intr_en = RPC_INT_VE3_EN,
	.isr = &rtk_rcpu_ve3_isr,
	.send_interrupt = &rpmsg_send_ve3_interrupt,
};

static const struct remote_cpu_info hifi_info = {
	.name = "hifi-rpc",
	.id = HIFI_ID,
	.big_endian = 0,
	.to_rcpu_intr_bit = RPC_INT_SH,
	.from_rcpu_intr_bit = RPC_INT_HS,
	.intr_en = RPC_INT_HIFI_EN,
	.isr = &rtk_rcpu_isr,
	.send_interrupt = &rpmsg_send_interrupt,
};

static const struct remote_cpu_info hifi1_info = {
	.name = "hifi1-rpc",
	.id = HIFI1_ID,
	.big_endian = 0,
	.to_rcpu_intr_bit = RPC_INT_SH1,
	.from_rcpu_intr_bit = RPC_INT_H1S,
	.intr_en = RPC_INT_HIFI1_EN,
	.isr = &rtk_rcpu_isr,
	.send_interrupt = &rpmsg_send_interrupt,
};

static const struct remote_cpu_info kr4_info = {
	.name = "kr4-rpc",
	.id = KR4_ID,
	.big_endian = 0,
	.to_rcpu_intr_bit = RPC_INT_SA,
	.from_rcpu_intr_bit = RPC_INT_AS,
	.intr_en = RPC_INT_ACPU_EN,
	.isr = &rtk_rcpu_isr,
	.send_interrupt = &rpmsg_send_interrupt,
};


static const struct of_device_id rtk_rcpu_of_match[] = {
	{ .compatible = "realtek,acpu-rpmsg", .data = &acpu_info },
	{ .compatible = "realtek,vcpu-rpmsg", .data = &vcpu_info },
	{ .compatible = "realtek,ve3-rpmsg", .data = &ve3_info },
	{ .compatible = "realtek,hifi-rpmsg", .data = &hifi_info },
	{ .compatible = "realtek,hifi1-rpmsg", .data = &hifi1_info },
	{ .compatible = "realtek,kr4-rpmsg", .data = &kr4_info },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_rcpu_of_match);

static struct platform_driver rtk_rcpu_driver = {
	.probe = rtk_rcpu_probe,
	.remove = rtk_rcpu_remove,
	.driver = {
		.name = "rtk-rpmsg",
		.of_match_table = rtk_rcpu_of_match,
		.pm     = &rtk_rcpu_pm_ops,
	},
};

static int __init rtk_rcpu_init(void)
{
	return platform_driver_register(&rtk_rcpu_driver);
}
module_init(rtk_rcpu_init);
static void __exit rtk_rcpu_exit(void)
{
	platform_driver_unregister(&rtk_rcpu_driver);
}
module_exit(rtk_rcpu_exit);

MODULE_AUTHOR("TYChang <tychang@realtek.com>");
MODULE_DESCRIPTION("Realtek RPMSG Driver");
MODULE_LICENSE("GPL v2");

