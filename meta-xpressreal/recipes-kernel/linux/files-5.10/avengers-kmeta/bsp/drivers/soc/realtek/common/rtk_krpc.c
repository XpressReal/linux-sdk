// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Realtek DHC Kernel RPC driver
 *
 * Copyright (c) 2022 Realtek Semiconductor Corp.
 */


#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include <linux/delay.h>
#include <soc/realtek/uapi/ion_rtk.h>
#include <uapi/linux/ion.h>
#include <ion_rtk_alloc.h>
#include <soc/realtek/rtk-krpc.h>
#include <linux/notifier.h>
#include <linux/completion.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>



#define REMOTE_INIT BIT(0)
#define REMOTE_SUSPEND_READY BIT(1)
#define REMOTE_SUSPEND BIT(2)


#define FW_ALLOC_SPEC_MASK  0xC0000000
#define FW_ALLOC_VCPU_FWACC 0x40000000
#define FW_ALLOC_VCPU_EXTRA 0x80000000

extern struct raw_notifier_head rtk_rpmsg_chain_head;
unsigned int retry_count_value = 5;
unsigned int retry_delay_value = 200;


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

};

struct fw_alloc_parameter {
	uint32_t size;
	uint32_t flags; /* enum E_FW_ALLOC_FLAGS */
} __attribute__((aligned(1)));

struct fw_alloc_parameter_legacy {
	uint32_t size;
} __attribute__((aligned(1)));


enum rpc_remote_cmd {
	RPC_REMOTE_CMD_ALLOC = 1,
	RPC_REMOTE_CMD_FREE = 2,
	RPC_REMOTE_CMD_ALLOC_SECURE = 3,
};

#define FW_ALLOC_SPEC_MASK  0xC0000000
#define FW_ALLOC_VCPU_FWACC 0x40000000
#define FW_ALLOC_VCPU_EXTRA 0x80000000

struct r_program_entry {
	unsigned long phys_addr;
	struct dma_buf *rpc_dmabuf;
	struct sg_table *table;
	struct dma_buf_attachment *attachment;
	struct r_program_entry *next;
	size_t size;
};

static void r_program_add(struct rtk_rpc_client *client, struct r_program_entry *entry)
{
	spin_lock_bh(&client->r_program_lock);
	entry->next = client->r_program_head;
	client->r_program_head = entry;
	client->r_program_count++;
	spin_unlock_bh(&client->r_program_lock);
}

static struct r_program_entry *r_program_remove(struct rtk_rpc_client *client, unsigned long phys_addr)
{
	struct r_program_entry *prev = NULL;
	struct r_program_entry *curr = NULL;

	spin_lock_bh(&client->r_program_lock);
	curr = client->r_program_head;
	while (curr != NULL) {
		if (curr->phys_addr != phys_addr) {
			prev = curr;
			curr = curr->next;
			continue;
		}

		if (prev == NULL)
			client->r_program_head = curr->next;
		else
			prev->next = curr->next;

		client->r_program_count--;
		spin_unlock_bh(&client->r_program_lock);

		return curr;
	}
	spin_unlock_bh(&client->r_program_lock);
	return NULL;
}


static void rtk_rpc_free_ion(struct r_program_entry *rpc_entry)
{
	struct dma_buf *dmabuf = rpc_entry->rpc_dmabuf;
	struct dma_buf_attachment *attach = rpc_entry->attachment;

	pr_info("%s free ion buffer\n", __func__);
	BUG_ON(!dmabuf);
	BUG_ON(!attach);

	dma_buf_unmap_attachment(attach, attach->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}

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


void remote_alloc_reply(struct rtk_rpc_client *client, struct rpc_struct *rpc, unsigned long reply)
{
	char *buf = kmalloc(sizeof(struct rpc_struct) + (2 * sizeof(uint32_t)), GFP_KERNEL);
	struct rpc_struct *rrpc;
	uint32_t *tmp;
	int ret;

	rrpc = (struct rpc_struct *)buf;
	rrpc->programID = REPLYID;
	rrpc->versionID = REPLYID;
	rrpc->procedureID = 0;
	rrpc->taskID = 0;
	rrpc->sysTID = 0;
	rrpc->sysPID = 0;
	rrpc->parameterSize = 2 * sizeof(uint32_t);
	rrpc->mycontext = rpc->mycontext;

	tmp = (uint32_t *)(buf + sizeof(struct rpc_struct));
	*tmp = rpc->taskID;
	*(tmp + 1) = reply;

	if (client->big_endian)
		endian_swap_32_write((void *)buf, sizeof(struct rpc_struct) + (2 * sizeof(uint32_t)));


	ret = rpmsg_send(client->rpdev->ept, (void *)buf, sizeof(struct rpc_struct) + (2 * sizeof(uint32_t)));
	if (ret != sizeof(struct rpc_struct) + (2 * sizeof(uint32_t)))
		dev_err(client->dev, "send_rpc length error:%x   %x\n", ret, sizeof(struct rpc_struct) + (2 * sizeof(uint32_t)));

	kfree(buf);

}

unsigned int rpc_ion_alloc_handler_legacy(struct rtk_rpc_client *client, bool secure, const struct fw_alloc_parameter_legacy *param)
{
	unsigned int reply_value = 0;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct r_program_entry *rpc_entry;
	unsigned int fw_send_value = param->size;
	size_t alloc_val = 0;
	struct dma_buf *dmabuf = NULL;
	unsigned int ion_alloc_flags;
	struct device *dev = client->dev;

	if (secure) {
		ion_alloc_flags = RTK_FLAG_PROTECTED_V2_VO_POOL | RTK_FLAG_HWIPACC;
		if (!strcmp(client->name, "acpu"))
			ion_alloc_flags |= RTK_FLAG_ACPUACC;
	} else {
		ion_alloc_flags = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_HWIPACC | RTK_FLAG_ACPUACC;
	}

	alloc_val = PAGE_ALIGN(fw_send_value & ~FW_ALLOC_SPEC_MASK);

	if (fw_send_value & FW_ALLOC_VCPU_FWACC)
		ion_alloc_flags |= RTK_FLAG_VCPU_FWACC;

	if (fw_send_value & FW_ALLOC_VCPU_EXTRA)
		ion_alloc_flags |= RTK_FLAG_CMA;

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

	rpc_entry = kmalloc(sizeof(struct r_program_entry),
			GFP_KERNEL);

	rpc_entry->next = NULL;
	rpc_entry->rpc_dmabuf	= dmabuf;

	attach = dma_buf_attach(rpc_entry->rpc_dmabuf, dev);
	if (IS_ERR(attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		goto attach_err;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		dev_err(dev, "Failed to map attachment\n");
		goto map_err;
	}
	rpc_entry->attachment = attach;
	rpc_entry->phys_addr = sg_phys(table->sgl);

	rpc_entry->size = alloc_val;
	r_program_add(client, rpc_entry);

	reply_value = rpc_entry->phys_addr;

	dev_dbg(dev, "[%s] ion_alloc addr : 0x%x (flags:0x%x)\n", __func__,
			reply_value, ion_alloc_flags);

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

unsigned int rpc_ion_alloc_handler(struct rtk_rpc_client *client, const struct fw_alloc_parameter *param)
{
	unsigned int reply_value = 0;
	unsigned int retry_count = 0;
	size_t alloc_size = param->size;
	unsigned int alloc_flags = param->flags;
	unsigned int ion_alloc_flags = 0;
	struct r_program_entry *rpc_entry;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = client->dev;

retry:
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_SCPUACC) ? RTK_FLAG_SCPUACC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_ACPUACC) ? RTK_FLAG_ACPUACC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_HWIPACC) ? RTK_FLAG_HWIPACC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_VE_SPEC) ? RTK_FLAG_VE_SPEC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_AUDIO_POOL) ? RTK_FLAG_PROTECTED_V2_AUDIO_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_TP_POOL) ? RTK_FLAG_PROTECTED_V2_TP_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_VO_POOL) ? RTK_FLAG_PROTECTED_V2_VO_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_VIDEO_POOL) ? RTK_FLAG_PROTECTED_V2_VIDEO_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_AO_POOL) ? RTK_FLAG_PROTECTED_V2_AO_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_METADATA_POOL) ? RTK_FLAG_PROTECTED_V2_METADATA_POOL : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_VCPU_FWACC) ? RTK_FLAG_VCPU_FWACC : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_CMA) ? RTK_FLAG_CMA : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_FW_STACK) ? RTK_FLAG_PROTECTED_V2_FW_STACK : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_EXT_BIT0) ? RTK_FLAG_PROTECTED_EXT_BIT0 : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_EXT_BIT1) ? RTK_FLAG_PROTECTED_EXT_BIT1 : 0;
	ion_alloc_flags |= (alloc_flags & eAlloc_Flag_PROTECTED_EXT_BIT2) ? RTK_FLAG_PROTECTED_EXT_BIT2 : 0;

	dev_info(dev, "%s alloc_size=0x%x  ion_alloc_flags=0x%x\n", __func__,
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
				dev_err(dev, "%s: ion alloc retry_delay = %d, count = %d, MAX_COUNT = %d\n",
					__func__, retry_delay_value,
					retry_count, retry_count_value);
				goto rheap_err;
			}
		}
	}

	rpc_entry = kmalloc(sizeof(struct r_program_entry), GFP_KERNEL);
	rpc_entry->next = NULL;
	rpc_entry->rpc_dmabuf = dmabuf;
	attach = dma_buf_attach(rpc_entry->rpc_dmabuf, dev);
	if (IS_ERR(attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		goto attach_err;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		dev_err(dev, "Failed to map attachment\n");
		goto map_err;
	}
	rpc_entry->attachment = attach;
	rpc_entry->phys_addr = sg_phys(table->sgl);

	rpc_entry->size = alloc_size;
	r_program_add(client, rpc_entry);
	reply_value = rpc_entry->phys_addr;

	dev_dbg(dev, "[%s] ion_alloc addr : 0x%x (flags:0x%x)\n", __func__,
			reply_value, ion_alloc_flags);

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


static void remote_allocate_handler(struct rtk_rpc_client *client, char *buf)
{
	char *tmp;
	enum rpc_remote_cmd remote_cmd;
	unsigned long reply_value = 0;
	struct r_program_entry *rpc_entry;
	phys_addr_t phys_addr;
	struct rpc_struct *rpc = (struct rpc_struct *)buf;

	remote_cmd = (enum rpc_remote_cmd) rpc->procedureID;
	tmp = (char *)(buf + sizeof(struct rpc_struct));

	switch (remote_cmd) {
	case RPC_REMOTE_CMD_ALLOC:
	case RPC_REMOTE_CMD_ALLOC_SECURE:

		if (rpc->parameterSize == sizeof(struct fw_alloc_parameter)) {
			struct fw_alloc_parameter param;

			memcpy((char *)&param, tmp, rpc->parameterSize);

			reply_value = rpc_ion_alloc_handler(client, &param);
		} else if (rpc->parameterSize == sizeof(struct fw_alloc_parameter_legacy)) {
			struct fw_alloc_parameter_legacy param;

			bool secure = (remote_cmd == RPC_REMOTE_CMD_ALLOC_SECURE) ? true : false;

			memcpy((char *)&param, tmp, rpc->parameterSize);

			reply_value = rpc_ion_alloc_handler_legacy(client, secure, &param);
		} else {
			dev_err(client->dev, "%s :RPC_REMOTE_CMD_ALLOC parameterSize(%d) mismatch!\n", __func__, rpc->parameterSize);
			break;
		}

		rpc->mycontext &= 0xfffffffc;

		break;
	case RPC_REMOTE_CMD_FREE:
		phys_addr = *(u32 *)tmp;
		rpc_entry = r_program_remove(client, phys_addr);

		if (rpc_entry) {
			rtk_rpc_free_ion(rpc_entry);
			kfree(rpc_entry);
			dev_dbg(client->dev, "[%s] ion_free addr : 0x%x (reply_value : 0x%lx)\n", __func__, phys_addr, reply_value);
			reply_value = 0;
		} else {
			dev_err(client->dev, "[%s]cannot find rpc_entry to free:phys_addr:%x\n", __func__, phys_addr);
			reply_value = -1U;
		}
		break;
	default:
		dev_err(client->dev, "[%s][%s]command not find %d\n", __func__, client->name, remote_cmd);
		break;
	}

	rpc->mycontext = rpc->mycontext & 0xfffffffc;

	remote_alloc_reply(client, rpc, reply_value);
}

static void reply_handler(struct rtk_rpc_client *client, char *buf)
{
	uint32_t *tmp;

	tmp = (uint32_t *)(buf + sizeof(struct rpc_struct));
	*client->retval = *(tmp + 1);

	complete(&client->ack);
}

static void  kern_rpc_work(struct work_struct *work)
{
	struct rtk_rpc_client *client = container_of(work, struct rtk_rpc_client, work);
	struct sk_buff *skb;
	struct rpc_struct *rpc;

	spin_lock(&client->queue_lock);
	if (skb_queue_empty(&client->queue))
		return;
	skb = skb_dequeue(&client->queue);
	spin_unlock(&client->queue_lock);

	rpc = (struct rpc_struct *)skb->data;

	dev_dbg(client->dev, "[%s]rpc->programID:%d, rpc->versionID:%d, rpc->procedureID:%d, rpc->taskID:%d, rpc->sysTID:%d, rpc->sysPID:%d, rpc->parameterSize:%d, rpc->mycontext:0x%x\n",
		__func__, rpc->programID, rpc->versionID, rpc->procedureID, rpc->taskID, rpc->sysTID, rpc->sysPID, rpc->parameterSize, rpc->mycontext);

	if (rpc->programID == 98)
		remote_allocate_handler(client, skb->data);
	else if (rpc->programID == 99)
		reply_handler(client, skb->data);

	kfree_skb(skb);
	if (!skb_queue_empty(&client->queue))
		schedule_work(&client->work);

}


static int rtk_rpc_callback(struct rpmsg_device *rpdev,
				void *data,
				int count,
				void *priv,
				u32 addr)
{
	struct rtk_rpc_client *client = dev_get_drvdata(&rpdev->dev);
	char *buf = (char *)data;
	struct sk_buff *skb;
	struct rpc_struct *rpc;

	rpc = (struct rpc_struct *)buf;

	if (client->big_endian)
		endian_swap_32_read(buf, sizeof(struct rpc_struct) + ntohl(rpc->parameterSize));

	dev_dbg(client->dev, "[%s]:rpc->programID:%d, rpc->versionID:%d, rpc->procedureID:%d, rpc->taskID:%d, rpc->sysTID:%d, rpc->sysPID:%d, rpc->parameterSize:%d, rpc->mycontext:0x%x\n",
				__func__, rpc->programID, rpc->versionID, rpc->procedureID, rpc->taskID, rpc->sysTID, rpc->sysPID, rpc->parameterSize, rpc->mycontext);

	skb = alloc_skb(count, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	skb_put_data(skb, buf, count);

	spin_lock(&client->queue_lock);
	skb_queue_tail(&client->queue, skb);
	spin_unlock(&client->queue_lock);

	schedule_work(&client->work);

	return 0;
}


static int rtk_rpc_probe(struct rpmsg_device *rpdev)
{
	struct rtk_rpc_client *client;

	client = devm_kzalloc(&rpdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->dev = &rpdev->dev;
	client->ept = rpdev->ept;
	client->name = (char *)of_device_get_match_data(&rpdev->dev);

	client->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	client->dev->dma_mask = (u64 *)&client->dev->coherent_dma_mask;
	client->big_endian = rcpu_endian_check(client->dev->parent);

	client->rpdev = rpdev;
	spin_lock_init(&client->r_program_lock);
	mutex_init(&client->send_lock);
	spin_lock_init(&client->queue_lock);
	skb_queue_head_init(&client->queue);
	init_completion(&client->ack);
	dev_set_drvdata(&rpdev->dev, client);
	INIT_WORK(&client->work, kern_rpc_work);

	raw_notifier_call_chain(&rtk_rpmsg_chain_head, REMOTE_INIT, (void *)client->ept);

	//rpmsg_set_signals(client->ept, REMOTE_INIT, 0);

	dev_info(&rpdev->dev, "probe\n");

	return of_platform_populate(rpdev->dev.of_node, NULL, NULL, &rpdev->dev);
}

static void rtk_rpc_remove(struct rpmsg_device *rpdev)
{
	of_platform_depopulate(&rpdev->dev);
}



static const struct of_device_id rtk_rpc_of_match[] = {
	{ .compatible = "realtek,rpc-kernel-acpu", .data = "acpu" },
	{ .compatible = "realtek,rpc-kernel-vcpu", .data = "vcpu" },
	{ .compatible = "realtek,rpc-kernel-ve3", .data = "ve3" },
	{ .compatible = "realtek,rpc-kernel-hifi", .data = "hifi" },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_rpc_of_match);

static struct rpmsg_driver rtk_rpc_driver = {
	.probe = rtk_rpc_probe,
	.remove = rtk_rpc_remove,
	.callback = rtk_rpc_callback,
	.drv  = {
		.name  = "rtk_rpc",
		.of_match_table = rtk_rpc_of_match,
	},
};


static int __init rtk_rpc_init(void)
{
	return register_rpmsg_driver(&rtk_rpc_driver);
}
module_init(rtk_rpc_init);

static void __exit rtk_rpc_exit(void)
{
	unregister_rpmsg_driver(&rtk_rpc_driver);
}
module_exit(rtk_rpc_exit);

