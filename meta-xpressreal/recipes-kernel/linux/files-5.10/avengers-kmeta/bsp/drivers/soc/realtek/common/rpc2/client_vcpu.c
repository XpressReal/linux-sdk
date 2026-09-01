// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek RPC driver
 *
 * Copyright (c) 2017-2020 Realtek Semiconductor Corp.
 */
//#define DEBUG

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <soc/realtek/rtk_refclk.h>
#include <soc/realtek/rtk_ipc_shm.h>
#include "rpc.h"
#include "rpc_uapi.h"


struct vcpu_device {
	struct device *dev;
	int type;
	int rpc_irq;

	void __iomem *rpc_int_base;
	void __iomem *rpc_int_flag;
	void __iomem *video_rpc_flag;

	struct rpc_device* rpc_dev;
	struct rpc_client client;

	struct rpc_record *kern_write_record;
	struct rpc_record *kern_read_record;
	void __iomem *kern_write_ring;
	void __iomem *kern_read_ring;

	struct rpc_record *user_intr_write_record;
	struct rpc_record *user_intr_read_record;
	void __iomem *user_intr_write_ring;
	void __iomem *user_intr_read_ring;

	struct rpc_record *user_poll_write_record;
	struct rpc_record *user_poll_read_record;
	void __iomem *user_poll_write_ring;
	void __iomem *user_poll_read_ring;
};

/* callback */
static int vcpu_rpc_send_interrupt(struct rpc_client *client)
{
	struct vcpu_device *vcpu = container_of(client, struct vcpu_device, client);
	void __iomem *rpc_int_base = vcpu->rpc_int_base;
	void __iomem *rpc_int_flag = vcpu->rpc_int_flag;

	if (rpc_int_flag != NULL && RPC_HAS_BIT(rpc_int_flag, VIDEO_RPC_SET_NOTIFY))
		RPC_SET_BIT(rpc_int_flag, VIDEO_RPC_FEEDBACK_NOTIFY);

	dev_dbg(vcpu->dev, "send video interrupt\n");
	writel_relaxed((RPC_INT_SV | RPC_INT_WRITE_1), rpc_int_base + RPC_SB2_INT);

	return 0;
}

/* callback */
static void vcpu_rpc_set_flag(struct vcpu_device *vcpu, uint32_t flag)
{
	/* video RPC flag */
	writel(__cpu_to_be32(flag), vcpu->video_rpc_flag);
}

static uint32_t vcpu_rpc_get_flag(struct vcpu_device *vcpu)
{
	/* video RPC flag */
	return __be32_to_cpu(readl(vcpu->video_rpc_flag));
}

irqreturn_t vcpu_rpc_isr(int irq, void *dev_id)
{
	struct vcpu_device *vcpu = (struct vcpu_device *) dev_id;
	struct rpc_client *client = &vcpu->client;
	int itr;
	void __iomem *rpc_int_base = vcpu->rpc_int_base;
	void __iomem *rpc_int_flag = vcpu->rpc_int_flag;

	itr = readl_relaxed(rpc_int_base + RPC_SB2_INT_ST);

	dev_dbg(vcpu->dev, "%s itr=0x%x rpc_int_flag=0x%x\n",
		    __func__, itr, readl(rpc_int_flag));

	if (RPC_HAS_BIT(rpc_int_flag, RPC_VIDEO_FEEDBACK_NOTIFY)) {
		RPC_RESET_BIT(rpc_int_flag, RPC_VIDEO_FEEDBACK_NOTIFY);
	} else {
		if (itr & (RPC_INT_VS))
			writel_relaxed(RPC_INT_VS, rpc_int_base + RPC_SB2_INT_ST);

		return IRQ_HANDLED;
	}

	while (itr & RPC_INT_VS) {
		if (itr & RPC_INT_VS) {
			struct user_rpc *user;
			struct kern_rpc *kern;

			/* to clear interrupt, set bit[0] to 0 then we can clear A2S int */
			writel_relaxed(RPC_INT_VS, rpc_int_base + RPC_SB2_INT_ST);

			user = &client->user_intr;
			if (!rpc_ringbuf_empty(&user->read_record)) {
				tasklet_schedule(&(user->tasklet));
			}

			kern = &client->kern;
			if (!rpc_ringbuf_empty(&kern->read_record)) {
				wake_up_interruptible(&(kern->waitQueue));
			}
		}
		itr = readl_relaxed(rpc_int_base + RPC_SB2_INT_ST);
	}

	return IRQ_HANDLED;
}

static int setup_record(struct vcpu_device *vcpu, struct rpc_record *record,
	    struct rpc_record_mapping *record_mapping, uint32_t ringbuf_paddr)
{
	dev_dbg(vcpu->dev, "rpc_record addr: %p\n", record);
	/* Initialize pointers... */
	record->ringBuf = ringbuf_paddr;
	record->ringStart = ringbuf_paddr;
	record->ringEnd = ringbuf_paddr + RPC_RING_SIZE;
	record->ringIn = ringbuf_paddr;
	record->ringOut = ringbuf_paddr;

	record_mapping->ringBuf = &record->ringBuf;
	record_mapping->ringStart = &record->ringStart;
	record_mapping->ringEnd = &record->ringEnd;
	record_mapping->ringIn = &record->ringIn;
	record_mapping->ringOut = &record->ringOut;

	dev_dbg(vcpu->dev, "The vcpu rpc_record:\n");
	dev_dbg(vcpu->dev, "RPC ringStart: %x\n", record->ringStart);
	dev_dbg(vcpu->dev, "RPC ringEnd:   %x\n", record->ringEnd);
	dev_dbg(vcpu->dev, "RPC ringIn:    %x\n", record->ringIn);
	dev_dbg(vcpu->dev, "RPC ringOut:   %x\n", record->ringOut);

	return 0;
}

static int vcpu_rpc_user_poll_init(struct vcpu_device *vcpu)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &vcpu->client;
	rpc_dev = vcpu->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->vcpu_user_poll_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->vcpu_user_poll_write_record;

	vcpu->user_poll_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	vcpu->user_poll_write_record = rpc_record;
	setup_record(vcpu, rpc_record, &client->user_poll.write_record, ringbuf_paddr);
	snprintf((&client->user_poll.write_record)->name, 32, "%s", "vcpu-poll-write");
	(&client->user_poll.write_record)->pa2va_offset = offset;
	(&client->user_poll.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->vcpu_user_poll_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->vcpu_user_poll_read_record;

	vcpu->user_poll_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	vcpu->user_poll_read_record = rpc_record;
	setup_record(vcpu, rpc_record, &client->user_poll.read_record, ringbuf_paddr);
	snprintf((&client->user_poll.read_record)->name, 32, "%s", "vcpu-poll-read");
	(&client->user_poll.read_record)->pa2va_offset = offset;
	(&client->user_poll.read_record)->big_endian = client->big_endian;

	rpc_client_user_poll_init(rpc_dev, client);

	return result;
}

static int vcpu_rpc_user_intr_init(struct vcpu_device *vcpu)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &vcpu->client;
	rpc_dev = vcpu->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->vcpu_user_intr_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->vcpu_user_intr_write_record;

	vcpu->user_intr_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	vcpu->user_intr_write_record = rpc_record;
	setup_record(vcpu, rpc_record, &client->user_intr.write_record, ringbuf_paddr);
	snprintf((&client->user_intr.write_record)->name, 32, "%s", "vcpu-kern-write");
	(&client->user_intr.write_record)->pa2va_offset = offset;
	(&client->user_intr.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->vcpu_user_intr_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->vcpu_user_intr_read_record;

	vcpu->user_intr_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	vcpu->user_intr_read_record = rpc_record;
	setup_record(vcpu, rpc_record, &client->user_intr.read_record, ringbuf_paddr);
	snprintf((&client->user_intr.read_record)->name, 32, "%s", "vcpu-intr-read");
	(&client->user_intr.read_record)->pa2va_offset = offset;
	(&client->user_intr.read_record)->big_endian = client->big_endian;

	rpc_client_user_intr_init(rpc_dev, client);

	return result;
}

static int vcpu_rpc_kern_init(struct vcpu_device *vcpu)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &vcpu->client;
	rpc_dev = vcpu->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->vcpu_kern_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->vcpu_kern_write_record;

	vcpu->kern_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	vcpu->kern_write_record = rpc_record;
	setup_record(vcpu, rpc_record, &client->kern.write_record, ringbuf_paddr);
	snprintf((&client->kern.write_record)->name, 32, "%s", "vcpu-kern-write");
	(&client->kern.write_record)->pa2va_offset = offset;
	(&client->kern.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->vcpu_kern_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->vcpu_kern_read_record;

	vcpu->kern_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	vcpu->kern_read_record = rpc_record;
	setup_record(vcpu, rpc_record, &client->kern.read_record, ringbuf_paddr);
	snprintf((&client->kern.read_record)->name, 32, "%s", "vcpu-kern-read");
	(&client->kern.read_record)->pa2va_offset = offset;
	(&client->kern.read_record)->big_endian = client->big_endian;

	rpc_client_kern_init(client);

	return result;
}

/* callback */
static int vcpu_rpc_interrupt_init(struct vcpu_device *vcpu)
{
	//struct vcpu_device *vcpu = container_of(client, struct rpc_client, client);
	int max_count = 5000;
	int wait_time = 0;
	int ret = -1;

	RPC_SET_BIT(vcpu->rpc_int_flag, RPC_VIDEO_SET_NOTIFY);

	writel_relaxed(RPC_INT_SV | RPC_INT_WRITE_1,
		    vcpu->rpc_int_base + RPC_SB2_INT_EN);

	ret = request_irq(vcpu->rpc_irq,
			  vcpu_rpc_isr, IRQF_SHARED | IRQF_NO_SUSPEND,
			  "vcpu_rpc",
			  (void *) vcpu);
	if (ret) {
		dev_err(vcpu->dev, "register vcpu irq handler failed\n");
		goto exit;
	}

	/* Enable the interrupt from system to audio & video */
	vcpu_rpc_set_flag(vcpu, 0xffffffff);
	vcpu_rpc_send_interrupt(&vcpu->client);

	dev_warn(vcpu->dev, "wait vcpu ready");

	while ((vcpu_rpc_get_flag(vcpu) == 0xffffffff) && ((max_count--) > 0)) {
		mdelay(1);
		if ((++wait_time) == 10)
			wait_time = 0;
	}

	while ((--wait_time) > 0)
		dev_warn(vcpu->dev, ".");

	dev_warn(vcpu->dev, "%s (RPC_VIDEO FLAG = 0x%08x)\n",
		(max_count > 0) ? "OK" : "timeout", vcpu_rpc_get_flag(vcpu));

exit:
	return ret;
}

#ifdef CONFIG_PM
/*
 * Disable the interrupt from system to audio & video
 */
static int vcpu_suspend(struct rpc_client *client)
{
	struct vcpu_device *vcpu = container_of(client, struct vcpu_device, client);
	struct device *dev = vcpu->dev;

	dev_info(dev, "enter %s\n", __func__);

	dev_info(dev, "exit %s\n", __func__);

	return 0;
}

/*
 * Enable the interrupt from system to audio & video
 */
static int vcpu_resume(struct rpc_client *client)
{
	struct vcpu_device *vcpu = container_of(client, struct vcpu_device, client);
	struct device *dev = vcpu->dev;

	dev_info(dev, "enter %s\n", __func__);

	dev_info(dev, "exit %s\n", __func__);

	return 0;
}

static void vcpu_shutdown(struct rpc_client *client)
{
	struct vcpu_device *vcpu = container_of(client, struct vcpu_device, client);
	struct device *dev = vcpu->dev;
	void __iomem *rpc_int_flag = vcpu->rpc_int_flag;
	int max_count = 500;

	dev_info(dev, "enter %s\n", __func__);

	vcpu_rpc_set_flag(vcpu, 0xdaedffff); /* STOP VIDEO HAS_CHECK */
	while ((vcpu_rpc_get_flag(vcpu) != 0x0) && (max_count > 0)) {
		mdelay(1);
		max_count--;
	}

	/* disable interrupt */
	RPC_RESET_BIT(rpc_int_flag, RPC_VIDEO_SET_NOTIFY);

	wmb();

	vcpu_rpc_set_flag(vcpu, 0xdeadffff); /* WAIT AUDIO RPC SUSPEND READY */
	while ((vcpu_rpc_get_flag(vcpu) != 0x0) && (max_count > 0)) {
		mdelay(1);
		max_count--;
	}

	vcpu_rpc_set_flag(vcpu, 0xdaedffff); /* STOP VIDEO HAS_CHECK */
	while ((vcpu_rpc_get_flag(vcpu) != 0x0) && (max_count > 0)) {
		mdelay(1);
		max_count--;
	}

	dev_info(dev, "wait %d ms\n", (500 - max_count));
	dev_info(dev, "Exit %s\n", __func__);
}
#endif /* CONFIG_PM */


static int vcpu_rpc_probe(struct platform_device *pdev)
{
	struct device *dev, *parent_dev;
	struct device_node *node;
	struct vcpu_device *vcpu;
	struct rpc_device *rpc_dev = NULL;
	struct rpc_client *client;
	int ret = 0;

	dev_info(&pdev->dev, "enter %s\n", __func__);

	dev = &pdev->dev;
	vcpu = devm_kzalloc(dev, sizeof(struct vcpu_device), GFP_KERNEL);
	if (!vcpu) {
		ret = -ENOMEM;
		return ret;
	}

	vcpu->dev = dev;
	platform_set_drvdata(pdev, vcpu);

	node = pdev->dev.of_node;
	if (WARN_ON(!node))
		dev_err(dev, "can not found device node\n");

	vcpu->rpc_int_base = of_iomap(node, 0);
	if (WARN_ON(!vcpu->rpc_int_base)) {
		dev_warn(dev, "can not map registers for %s\n", node->name);
		goto exit;
	}

	vcpu->rpc_irq = irq_of_parse_and_map(node, 0);
	if (WARN_ON(!vcpu->rpc_irq))
		dev_warn(dev, "can not parse VCPU irq\n");

	dev_info(dev, "rpc_int_base: %p\n", vcpu->rpc_int_base);
	dev_info(dev, "vcpu irq: %d\n", vcpu->rpc_irq);

	parent_dev = dev->parent;
	if (parent_dev && (rpc_dev = dev_get_drvdata(parent_dev))) {
		struct rtk_ipc_shm __iomem *ipc;

		vcpu->rpc_dev = rpc_dev;
		ipc = (void __iomem *) rpc_dev->ipc_shm_vaddr;
		if (ipc) {
			vcpu->rpc_int_flag = (void __iomem *)&ipc->video_int_sync;
			vcpu->video_rpc_flag = (void __iomem *)&ipc->video_rpc_flag;
		}
	}

	client = &vcpu->client;
	client->ringbuf_paddr2vaddr_offset = rpc_dev->ringbuf_paddr2vaddr_offset;
	client->dev = dev;
	client->id = RPC_VIDEO;
	client->big_endian = true;
	snprintf(client->name, 32, "%s", "vcpu");

	client->send_interrupt = &vcpu_rpc_send_interrupt;

	client->suspend = &vcpu_suspend;
	client->resume = &vcpu_resume;
	client->shutdown = &vcpu_shutdown;

	vcpu_rpc_user_poll_init(vcpu);
	vcpu_rpc_user_intr_init(vcpu);
	vcpu_rpc_kern_init(vcpu);

	rpc_client_register(rpc_dev, client);

	vcpu_rpc_interrupt_init(vcpu);

	dev_info(dev, "exit %s (ret=%d)\n", __func__, ret);
exit:
	return ret;
}

static int __maybe_unused vcpu_rpc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id vcpu_rpc_ids[] = {
	{.compatible = "realtek,vcpu_rpc" },
	{/* Sentinel */ },
};

static struct platform_driver vcpu_rpc_driver = {
	.probe = vcpu_rpc_probe,
	.remove = vcpu_rpc_remove,
	.driver = {
		.name = "realtek-vcpu-rpc",
		.of_match_table = vcpu_rpc_ids,
	},
};

int vcpu_rpc_init(void)
{
	return platform_driver_register(&vcpu_rpc_driver);
}
//device_initcall(vcpu_rpc_init);

MODULE_LICENSE("GPL");
