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


struct hifi_device {
	struct device *dev;
	int type;
	int rpc_irq;

	void __iomem *rpc_int_base;
	void __iomem *hifi_rpc_flag;

	struct rpc_device* rpc_dev;
	struct rpc_client client;

	struct rpc_record_hifi *kern_write_record;
	struct rpc_record_hifi *kern_read_record;
	void __iomem *kern_write_ring;
	void __iomem *kern_read_ring;

	struct rpc_record_hifi *user_intr_write_record;
	struct rpc_record_hifi *user_intr_read_record;
	void __iomem *user_intr_write_ring;
	void __iomem *user_intr_read_ring;
};

/* callback */
static int hifi_rpc_send_interrupt(struct rpc_client *client)
{
	struct hifi_device *hifi = container_of(client, struct hifi_device, client);
	void __iomem *rpc_int_base = hifi->rpc_int_base;

	dev_dbg(hifi->dev, "send hifi interrupt\n");
	writel_relaxed((RPC_INT_SH | RPC_INT_WRITE_1), rpc_int_base + RPC_SB2_INT);

	return 0;
}

/* callback */
static void hifi_rpc_set_flag(struct hifi_device *hifi, uint32_t flag)
{
	/* hifi RPC flag */
	writel(flag, hifi->hifi_rpc_flag);
}

static uint32_t hifi_rpc_get_flag(struct hifi_device *hifi)
{
	/* hifi RPC flag */
	return __be32_to_cpu(readl(hifi->hifi_rpc_flag));
}

irqreturn_t hifi_rpc_isr(int irq, void *dev_id)
{
	struct hifi_device *hifi = (struct hifi_device *) dev_id;
	struct rpc_client *client = &hifi->client;
	int itr;
	void __iomem *rpc_int_base = hifi->rpc_int_base;

	itr = readl_relaxed(rpc_int_base + RPC_SB2_INT_ST);

	dev_dbg(hifi->dev, "%s itr=0x%x\n", __func__, itr);

	while (itr & RPC_INT_HS) {
		struct user_rpc *user;
		struct kern_rpc *kern;

		writel_relaxed(RPC_INT_HS, rpc_int_base + RPC_SB2_INT_ST);

		user = &client->user_intr;
		if (!rpc_ringbuf_empty(&user->read_record)) {
			tasklet_schedule(&(user->tasklet));
		}

		kern = &client->kern;
		if (!rpc_ringbuf_empty(&kern->read_record)) {
			wake_up_interruptible(&(kern->waitQueue));
		}
		itr = readl_relaxed(rpc_int_base + RPC_SB2_INT_ST);
	}

	return IRQ_HANDLED;
}

static int setup_record(struct hifi_device *hifi, struct rpc_record_hifi *record,
	    struct rpc_record_mapping *record_mapping, uint32_t ringbuf_paddr)
{
	dev_dbg(hifi->dev, "rpc_record addr: %p\n", record);
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

	dev_dbg(hifi->dev, "The hifi rpc_record:\n");
	dev_dbg(hifi->dev, "RPC ringStart: %x\n", record->ringStart);
	dev_dbg(hifi->dev, "RPC ringEnd:   %x\n", record->ringEnd);
	dev_dbg(hifi->dev, "RPC ringIn:    %x\n", record->ringIn);
	dev_dbg(hifi->dev, "RPC ringOut:   %x\n", record->ringOut);

	return 0;
}

static int hifi_rpc_user_intr_init(struct hifi_device *hifi)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record_hifi *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &hifi->client;
	rpc_dev = hifi->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->hifi_user_intr_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record_hifi *)&rpc_comm_buffer->hifi_user_intr_write_record;

	hifi->user_intr_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	hifi->user_intr_write_record = rpc_record;
	setup_record(hifi, rpc_record, &client->user_intr.write_record, ringbuf_paddr);
	snprintf((&client->user_intr.write_record)->name, 32, "%s", "hifi-intr-write");
	(&client->user_intr.write_record)->pa2va_offset = offset;
	(&client->user_intr.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->hifi_user_intr_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record_hifi *)&rpc_comm_buffer->hifi_user_intr_read_record;

	hifi->user_intr_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	hifi->user_intr_read_record = rpc_record;
	setup_record(hifi, rpc_record, &client->user_intr.read_record, ringbuf_paddr);
	snprintf((&client->user_intr.read_record)->name, 32, "%s", "hifi-intr-read");
	(&client->user_intr.read_record)->pa2va_offset = offset;
	(&client->user_intr.read_record)->big_endian = client->big_endian;

	rpc_client_user_intr_init(rpc_dev, client);

	return result;
}

static int hifi_rpc_kern_init(struct hifi_device *hifi)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record_hifi *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &hifi->client;
	rpc_dev = hifi->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->hifi_kern_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record_hifi *)&rpc_comm_buffer->hifi_kern_write_record;

	hifi->kern_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	hifi->kern_write_record = rpc_record;
	setup_record(hifi, rpc_record, &client->kern.write_record, ringbuf_paddr);
	snprintf((&client->kern.write_record)->name, 32, "%s", "hifi-kern-write");
	(&client->kern.write_record)->pa2va_offset = offset;
	(&client->kern.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->hifi_kern_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record_hifi *)&rpc_comm_buffer->hifi_kern_read_record;

	hifi->kern_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	hifi->kern_read_record = rpc_record;
	setup_record(hifi, rpc_record, &client->kern.read_record, ringbuf_paddr);
	snprintf((&client->kern.read_record)->name, 32, "%s", "hifi-kern-read");
	(&client->kern.read_record)->pa2va_offset = offset;
	(&client->kern.read_record)->big_endian = client->big_endian;

	rpc_client_kern_init(client);

	return result;
}

/* callback */
static int hifi_rpc_interrupt_init(struct hifi_device *hifi)
{
	void __iomem *rpc_int_base = hifi->rpc_int_base;
	int max_count = 5000;
	int wait_time = 0;
	int ret = -1;

	writel_relaxed(readl_relaxed(rpc_int_base + RPC_SB2_INT_EN) | RPC_INT_SH | RPC_INT_WRITE_1,
		    rpc_int_base + RPC_SB2_INT_EN);

	ret = request_irq(hifi->rpc_irq,
			hifi_rpc_isr, IRQF_SHARED | IRQF_NO_SUSPEND,
			"hifi_rpc",
			(void *)hifi);
	if (ret) {
		dev_err(hifi->dev, "register hifi irq handler failed\n");
		goto exit;
	}

	hifi_rpc_set_flag(hifi, 0xffffffff);
	hifi_rpc_send_interrupt(&hifi->client);

	dev_warn(hifi->dev, "wait hifi ready");

	while ((hifi_rpc_get_flag(hifi) == 0xffffffff) && ((max_count--) > 0)) {
		mdelay(1);
		if ((++wait_time) == 10)
			wait_time = 0;
	}

	while ((--wait_time) > 0)
		dev_warn(hifi->dev, ".");

	dev_warn(hifi->dev, "%s (RPC_HIFI FLAG = 0x%08x)\n",
		(max_count > 0) ? "OK" : "timeout", hifi_rpc_get_flag(hifi));

exit:
	return ret;
}

static int hifi_rpc_probe(struct platform_device *pdev)
{
	struct device *dev, *parent_dev;
	struct device_node *node;
	struct hifi_device *hifi;
	struct rpc_device *rpc_dev = NULL;
	struct rpc_client *client;
	int ret = 0;

	dev_info(&pdev->dev, "enter %s\n", __func__);

	dev = &pdev->dev;
	hifi = devm_kzalloc(dev, sizeof(struct hifi_device), GFP_KERNEL);
	if (!hifi) {
		ret = -ENOMEM;
		return ret;
	}

	hifi->dev = dev;
	platform_set_drvdata(pdev, hifi);

	node = pdev->dev.of_node;
	if (WARN_ON(!node))
		dev_err(dev, "can not found device node\n");

	hifi->rpc_int_base = of_iomap(node, 0);
	if (WARN_ON(!hifi->rpc_int_base)) {
		dev_warn(dev, "can not map registers for %s\n", node->name);
		goto exit;
	}

	hifi->rpc_irq = irq_of_parse_and_map(node, 0);
	if (WARN_ON(!hifi->rpc_irq))
		dev_warn(dev, "%s can not parse irq\n", __func__);

	dev_info(dev, "rpc_int_base: %p\n", hifi->rpc_int_base);
	dev_info(dev, "hifi irq: %d\n", hifi->rpc_irq);

	parent_dev = dev->parent;
	if (parent_dev && (rpc_dev = dev_get_drvdata(parent_dev))) {
		struct rtk_ipc_shm __iomem *ipc;

		hifi->rpc_dev = rpc_dev;
		ipc = (void __iomem *) rpc_dev->ipc_shm_vaddr;
		if (ipc)
			hifi->hifi_rpc_flag = (void __iomem *)&ipc->hifi_rpc_flag;
	}

	client = &hifi->client;
	client->ringbuf_paddr2vaddr_offset = rpc_dev->ringbuf_paddr2vaddr_offset;
	client->dev = dev;
	client->id = RPC_HIFI;
	client->big_endian = false;
	snprintf(client->name, 32, "%s", "hifi");

	client->send_interrupt = &hifi_rpc_send_interrupt;

	hifi_rpc_user_intr_init(hifi);
	hifi_rpc_kern_init(hifi);

	rpc_client_register(rpc_dev, client);

	hifi_rpc_interrupt_init(hifi);

	dev_info(dev, "exit %s (ret=%d)\n", __func__, ret);
exit:
	return ret;
}

static int __maybe_unused hifi_rpc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id hifi_rpc_ids[] = {
	{.compatible = "realtek,hifi_rpc" },
	{/* Sentinel */ },
};

static struct platform_driver hifi_rpc_driver = {
	.probe = hifi_rpc_probe,
	.remove = hifi_rpc_remove,
	.driver = {
		.name = "realtek-hifi-rpc",
		.of_match_table = hifi_rpc_ids,
	},
};

int hifi_rpc_init(void)
{
	return platform_driver_register(&hifi_rpc_driver);
}
//device_initcall(hifi_rpc_init);

MODULE_LICENSE("GPL");
