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
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

#include <soc/realtek/rtk_refclk.h>
#include <soc/realtek/rtk_ipc_shm.h>
#include "rpc.h"
#include "rpc_uapi.h"

struct ve3_device {
	struct device *dev;
	int type;
	int rpc_irq;

	struct regmap *rpc_int_base;
	void __iomem *rpc_int_flag;
	void __iomem *ve3_rpc_flag;
	int clk_enable;

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
};

/* callback */
static int ve3_rpc_send_interrupt(struct rpc_client *client)
{
	struct ve3_device *ve3 = container_of(client, struct ve3_device, client);
	void __iomem *rpc_int_base = ve3->rpc_int_base;
	void __iomem *rpc_int_flag = ve3->rpc_int_flag;

	if (rpc_int_flag != NULL && RPC_HAS_BIT(rpc_int_flag, VE3_RPC_SET_NOTIFY))
		RPC_SET_BIT(rpc_int_flag, VE3_RPC_FEEDBACK_NOTIFY);

	dev_dbg(ve3->dev, "send ve3 interrupt\n");
	regmap_write(rpc_int_base, 0x78, RPC_INT_SVE3);

	return 0;
}

/* callback */
static void ve3_rpc_set_flag(struct ve3_device *ve3, uint32_t flag)
{
	/* ve3 RPC flag */
	writel(__cpu_to_be32(flag), ve3->ve3_rpc_flag);
}

static uint32_t ve3_rpc_get_flag(struct ve3_device *ve3)
{
	/* ve3 RPC flag */
	return __be32_to_cpu(readl(ve3->ve3_rpc_flag));
}

irqreturn_t ve3_rpc_isr(int irq, void *dev_id)
{
	struct ve3_device *ve3 = (struct ve3_device *) dev_id;
	struct rpc_client *client = &ve3->client;
	int itr;
	struct regmap *rpc_int_base = ve3->rpc_int_base;
	void __iomem *rpc_int_flag = ve3->rpc_int_flag;

	regmap_read(rpc_int_base, 0x88, &itr);

	dev_dbg(ve3->dev, "%s itr=0x%x rpc_int_flag=0x%x\n",
		    __func__, itr, readl(rpc_int_flag));

	if (RPC_HAS_BIT(rpc_int_flag, RPC_VE3_FEEDBACK_NOTIFY)) {
		RPC_RESET_BIT(rpc_int_flag, RPC_VE3_FEEDBACK_NOTIFY);
	} else {
		if (itr & RPC_INT_VE3S_ST) {
			regmap_write(rpc_int_base, 0x88, itr & (~RPC_INT_VE3S_ST));
			regmap_write(rpc_int_base, 0xe0, 0x0);
		}
		return IRQ_HANDLED;
	}

	while (itr & RPC_INT_VE3S_ST) {
		struct user_rpc *user;
		struct kern_rpc *kern;

		/* to clear interrupt, set bit[0] to 0 then we can clear A2S int */
		regmap_write(rpc_int_base, 0x88, itr & (~RPC_INT_VE3S_ST));
		regmap_write(rpc_int_base, 0xe0, 0x0);

		user = &client->user_intr;
		if (!rpc_ringbuf_empty(&user->read_record)) {
			tasklet_schedule(&(user->tasklet));
		}

		kern = &client->kern;
		if (!rpc_ringbuf_empty(&kern->read_record)) {
			wake_up_interruptible(&(kern->waitQueue));
		}

		regmap_read(rpc_int_base, 0x88, &itr);
	}

	return IRQ_HANDLED;
}

static int setup_record(struct ve3_device *ve3, struct rpc_record *record,
	    struct rpc_record_mapping *record_mapping, uint32_t ringbuf_paddr)
{
	dev_dbg(ve3->dev, "rpc_record addr: %p\n", record);
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

	dev_dbg(ve3->dev, "The ve3 rpc_record:\n");
	dev_dbg(ve3->dev, "RPC ringStart: %x\n", record->ringStart);
	dev_dbg(ve3->dev, "RPC ringEnd:   %x\n", record->ringEnd);
	dev_dbg(ve3->dev, "RPC ringIn:    %x\n", record->ringIn);
	dev_dbg(ve3->dev, "RPC ringOut:   %x\n", record->ringOut);

	return 0;
}

static int ve3_rpc_user_intr_init(struct ve3_device *ve3)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &ve3->client;
	rpc_dev = ve3->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)(uintptr_t)&rpc_comm_buffer->ve3_user_intr_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->ve3_user_intr_write_record;

	ve3->user_intr_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	ve3->user_intr_write_record = rpc_record;
	setup_record(ve3, rpc_record, &client->user_intr.write_record, ringbuf_paddr);
	snprintf((&client->user_intr.write_record)->name, 32, "%s", "ve3-intr-write");
	(&client->user_intr.write_record)->pa2va_offset = offset;
	(&client->user_intr.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->ve3_user_intr_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->ve3_user_intr_read_record;

	ve3->user_intr_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	ve3->user_intr_read_record = rpc_record;
	setup_record(ve3, rpc_record, &client->user_intr.read_record, ringbuf_paddr);
	snprintf((&client->user_intr.read_record)->name, 32, "%s", "ve3-intr-read");
	(&client->user_intr.read_record)->pa2va_offset = offset;
	(&client->user_intr.read_record)->big_endian = client->big_endian;

	rpc_client_user_intr_init(rpc_dev, client);

	return result;
}

static int ve3_rpc_kern_init(struct ve3_device *ve3)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &ve3->client;
	rpc_dev = ve3->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->ve3_kern_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->ve3_kern_write_record;

	ve3->kern_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	ve3->kern_write_record = rpc_record;
	setup_record(ve3, rpc_record, &client->kern.write_record, ringbuf_paddr);
	snprintf((&client->kern.write_record)->name, 32, "%s", "ve3-kern-write");
	(&client->kern.write_record)->pa2va_offset = offset;
	(&client->kern.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->ve3_kern_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->ve3_kern_read_record;

	ve3->kern_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	ve3->kern_read_record = rpc_record;
	setup_record(ve3, rpc_record, &client->kern.read_record, ringbuf_paddr);
	snprintf((&client->kern.read_record)->name, 32, "%s", "ve3-kern-read");
	(&client->kern.read_record)->pa2va_offset = offset;
	(&client->kern.read_record)->big_endian = client->big_endian;

	rpc_client_kern_init(client);

	return result;
}

/* callback */
static int ve3_rpc_interrupt_init(struct ve3_device *ve3)
{
	//struct ve3_device *ve3 = container_of(client, struct rpc_client, client);
	int max_count = 5000;
	int wait_time = 0;
	int ret = -1;

	if (!ve3->clk_enable) {
		dev_err(ve3->dev, "%s ve3 not enable\n", __func__);
		return -ENODEV;
	}

	ret = request_irq(ve3->rpc_irq,
		    ve3_rpc_isr, IRQF_SHARED | IRQF_NO_SUSPEND,
				"ve3_rpc",
				(void *)ve3);
	if (ret) {
		dev_err(ve3->dev, "register ve3 irq handler failed\n");
		goto exit;
	}

	ve3_rpc_set_flag(ve3, 0xffffffff);
	ve3_rpc_send_interrupt(&ve3->client);

	dev_warn(ve3->dev, "wait ve3 ready");

	while ((ve3_rpc_get_flag(ve3) == 0xffffffff) && ((max_count--) > 0)) {
		mdelay(1);
		if ((++wait_time) == 10)
			wait_time = 0;
	}

	while ((--wait_time) > 0)
		dev_warn(ve3->dev, ".");

	dev_warn(ve3->dev, "%s (RPC_VE3 FLAG = 0x%08x)\n",
		(max_count > 0) ? "OK" : "timeout", ve3_rpc_get_flag(ve3));

exit:
	return ret;
}

static int ve3_rpc_probe(struct platform_device *pdev)
{
	struct device *dev, *parent_dev;
	struct device_node *node;
	struct ve3_device *ve3;
	struct rpc_device *rpc_dev = NULL;
	struct rpc_client *client;
	struct device_node *syscon_np;
	static struct clk *ve3_clk;
	int ret = 0;

	dev_info(&pdev->dev, "enter %s\n", __func__);

	dev = &pdev->dev;
	ve3 = devm_kzalloc(dev, sizeof(struct ve3_device), GFP_KERNEL);
	if (!ve3) {
		ret = -ENOMEM;
		return ret;
	}

	ve3->dev = dev;
	platform_set_drvdata(pdev, ve3);

	node = pdev->dev.of_node;
	if (WARN_ON(!node))
		dev_err(dev, "can not found device node\n");

	ve3_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(ve3_clk)) {
		dev_err(dev, "ve3 clock source missing or invalid\n");
		return PTR_ERR(ve3_clk);
	}
	if (__clk_is_enabled(ve3_clk))
		ve3->clk_enable = 1;
	else
		ve3->clk_enable = 0;

	devm_clk_put(&pdev->dev, ve3_clk);

	syscon_np = of_parse_phandle(node, "syscon", 0);
	if (IS_ERR_OR_NULL(syscon_np))
		return -ENODEV;

	ve3->rpc_int_base = syscon_node_to_regmap(syscon_np);
	if (IS_ERR_OR_NULL(ve3->rpc_int_base)) {
		of_node_put(syscon_np);
		return -EINVAL;
	}

	ve3->rpc_irq = irq_of_parse_and_map(node, 0);
	if (WARN_ON(!ve3->rpc_irq))
		dev_warn(dev, "%s can not parse irq\n", __func__);

	dev_info(dev, "rpc_int_base: %p\n", ve3->rpc_int_base);
	dev_info(dev, "ve3 irq: %d\n", ve3->rpc_irq);

	parent_dev = dev->parent;
	if (parent_dev && (rpc_dev = dev_get_drvdata(parent_dev))) {
		struct rtk_ipc_shm __iomem *ipc;

		ve3->rpc_dev = rpc_dev;
		ipc = (void __iomem *)rpc_dev->ipc_shm_vaddr;
		if (ipc) {
			ve3->rpc_int_flag = (void __iomem *)&ipc->ve3_int_sync;
			ve3->ve3_rpc_flag = (void __iomem *)&ipc->ve3_rpc_flag;
		}
	}

	client = &ve3->client;
	client->ringbuf_paddr2vaddr_offset = rpc_dev->ringbuf_paddr2vaddr_offset;
	client->dev = dev;
	client->id = RPC_VE3;
	client->big_endian = true;
	snprintf(client->name, 32, "%s", "ve3");

	client->send_interrupt = &ve3_rpc_send_interrupt;

	ve3_rpc_user_intr_init(ve3);
	ve3_rpc_kern_init(ve3);

	rpc_client_register(rpc_dev, client);

	ve3_rpc_interrupt_init(ve3);

	dev_info(dev, "exit %s (ret=%d)\n", __func__, ret);

	return ret;
}

static int __maybe_unused ve3_rpc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id ve3_rpc_ids[] = {
	{.compatible = "realtek,ve3_rpc" },
	{/* Sentinel */ },
};

static struct platform_driver ve3_rpc_driver = {
	.probe = ve3_rpc_probe,
	.remove = ve3_rpc_remove,
	.driver = {
		.name = "realtek-ve3-rpc",
		.of_match_table = ve3_rpc_ids,
	},
};

int ve3_rpc_init(void)
{
	return platform_driver_register(&ve3_rpc_driver);
}
//device_initcall(ve3_rpc_init);

MODULE_LICENSE("GPL");
