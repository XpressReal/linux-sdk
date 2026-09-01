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
#include <linux/hwspinlock.h>

#include <soc/realtek/rtk_refclk.h>
#include <soc/realtek/rtk_ipc_shm.h>
#include "rpc.h"
#include "rpc_uapi.h"

struct acpu_device {
	struct device *dev;

	int type;
	int rpc_irq;

	void __iomem *rpc_int_base;
	void __iomem *rpc_int_flag;
	void __iomem *audio_rpc_flag;

	struct rpc_device* rpc_dev;
	struct rpc_client client;

	spinlock_t ASLock;
	struct hwspinlock *hwlock;

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
static int acpu_rpc_send_interrupt(struct rpc_client *client)
{
	struct acpu_device *acpu = container_of(client, struct acpu_device, client);
	void __iomem *rpc_int_base = acpu->rpc_int_base;
	void __iomem *rpc_int_flag = acpu->rpc_int_flag;

	dev_dbg(acpu->dev, "check rpc_int_flag=%x (AUDIO_RPC_SET_NOTIFY=%x)\n",
		    RPC_HAS_BIT(rpc_int_flag, AUDIO_RPC_SET_NOTIFY), AUDIO_RPC_SET_NOTIFY);
	if (rpc_int_flag != NULL && RPC_HAS_BIT(rpc_int_flag, AUDIO_RPC_SET_NOTIFY)) {
		if (acpu->hwlock)
			hwspin_lock_timeout_raw(acpu->hwlock, UINT_MAX);
		dev_dbg(acpu->dev, "set AUDIO_RPC_FEEDBACK_NOTIFY\n");
		RPC_SET_BIT(rpc_int_flag, AUDIO_RPC_FEEDBACK_NOTIFY);
		if (acpu->hwlock)
			hwspin_unlock_raw(acpu->hwlock);
	}
	dev_dbg(acpu->dev, "send audio interrupt\n");
	writel_relaxed((RPC_INT_SA | RPC_INT_WRITE_1), rpc_int_base + RPC_SB2_INT);
	dev_dbg(acpu->dev, "%s RPC_INT=0x%x rpc_int_flag=0x%x\n",
		    __func__, readl(rpc_int_base + RPC_SB2_INT), readl(rpc_int_flag));

	return 0;
}

/* callback */
static void acpu_rpc_set_flag(struct acpu_device *acpu, uint32_t flag)
{
	/* audio RPC flag */
	writel(__cpu_to_be32(flag), acpu->audio_rpc_flag);
}

static uint32_t acpu_rpc_get_flag(struct acpu_device *acpu)
{
	/* audio RPC flag */
	return __be32_to_cpu(readl(acpu->audio_rpc_flag));
}

irqreturn_t acpu_rpc_isr(int irq, void *dev_id)
{
	struct acpu_device *acpu = (struct acpu_device *) dev_id;
	struct rpc_client *rpc_client = &acpu->client;
	int itr;
	void __iomem *rpc_int_base = acpu->rpc_int_base;
	void __iomem *rpc_int_flag = acpu->rpc_int_flag;

	itr = readl_relaxed(rpc_int_base + RPC_SB2_INT_ST);

	if (RPC_HAS_BIT(rpc_int_flag, RPC_AUDIO_FEEDBACK_NOTIFY)) {
		if (acpu->hwlock)
			hwspin_lock_timeout_raw(acpu->hwlock, UINT_MAX);
		RPC_RESET_BIT(rpc_int_flag, RPC_AUDIO_FEEDBACK_NOTIFY);
		if (acpu->hwlock)
			hwspin_unlock_raw(acpu->hwlock);

		dev_dbg(acpu->dev, "%s itr=0x%x rpc_int_flag=0x%x (RPC_AUDIO_FEEDBACK_NOTIFY=%x)\n",
			    __func__, itr, readl(rpc_int_flag), RPC_AUDIO_FEEDBACK_NOTIFY);
	} else {
		if (itr & (1 << 1))
			writel_relaxed(1 << 1, rpc_int_base + RPC_SB2_INT_ST);

		//dev_dbg(acpu->dev, "%s IRQ_HANDLED itr=0x%x \n",
		//	    __func__, readl_relaxed(rpc_int_base + RPC_SB2_INT_ST));
		return IRQ_HANDLED;
	}

	while (itr & 1 << 1) {
		if (itr & 1 << 1) {
			struct user_rpc *user;
			struct kern_rpc *kern;

			/* to clear interrupt, set bit[0] to 0 then we can clear A2S int */
			writel_relaxed(1 << 1, rpc_int_base + RPC_SB2_INT_ST);

			dev_dbg(acpu->dev, "%s check RPC_SB2_INT_ST (itr=0x%x)\n",
				    __func__, readl(rpc_int_base + RPC_SB2_INT_ST));

			/* for user rpc */
			user = &rpc_client->user_intr;
			if (!rpc_ringbuf_empty(&user->read_record)) {
				dev_dbg(acpu->dev, "%s tasklet_schedule\n", __func__);
				tasklet_schedule(&(user->tasklet));
			}

			/* for kernel rpc */
			kern = &rpc_client->kern;
			if (!rpc_ringbuf_empty(&kern->read_record)) {
				dev_dbg(acpu->dev, "%s wake_up_interruptible\n", __func__);
				wake_up_interruptible(&(kern->waitQueue));
			}
		}

		itr = readl_relaxed(rpc_int_base + RPC_SB2_INT_ST);
	}

	return IRQ_HANDLED;
}

static int setup_record(struct acpu_device *acpu, struct rpc_record *record,
	    struct rpc_record_mapping *record_mapping, uint32_t ringbuf_paddr)
{
	dev_dbg(acpu->dev, "rpc_record addr: %px\n", record);
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

	dev_dbg(acpu->dev, "The acpu rpc_record:\n");
	dev_dbg(acpu->dev, "RPC ringStart: %x\n", record->ringStart);
	dev_dbg(acpu->dev, "RPC ringEnd:   %x\n", record->ringEnd);
	dev_dbg(acpu->dev, "RPC ringIn:    %x\n", record->ringIn);
	dev_dbg(acpu->dev, "RPC ringOut:   %x\n", record->ringOut);

	return 0;
}

static int acpu_rpc_user_poll_init(struct acpu_device *acpu)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &acpu->client;
	rpc_dev = acpu->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->acpu_user_poll_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->acpu_user_poll_write_record;

	acpu->user_poll_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	acpu->user_poll_write_record = rpc_record;
	setup_record(acpu, rpc_record, &client->user_poll.write_record, ringbuf_paddr);
	snprintf((&client->user_poll.write_record)->name, 32, "%s", "acpu-poll-write");
	(&client->user_poll.write_record)->pa2va_offset = offset;
	(&client->user_poll.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->acpu_user_poll_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->acpu_user_poll_read_record;

	acpu->user_poll_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	acpu->user_poll_read_record = rpc_record;
	setup_record(acpu, rpc_record, &client->user_poll.read_record, ringbuf_paddr);
	snprintf((&client->user_poll.read_record)->name, 32, "%s", "acpu-poll-read");
	(&client->user_poll.read_record)->pa2va_offset = offset;
	(&client->user_poll.read_record)->big_endian = client->big_endian;

	rpc_client_user_poll_init(rpc_dev, client);

	return result;
}

static int acpu_rpc_user_intr_init(struct acpu_device *acpu)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &acpu->client;
	rpc_dev = acpu->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->acpu_user_intr_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->acpu_user_intr_write_record;

	acpu->user_intr_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	acpu->user_intr_write_record = rpc_record;
	setup_record(acpu, rpc_record, &client->user_intr.write_record, ringbuf_paddr);
	snprintf((&client->user_intr.write_record)->name, 32, "%s", "acpu-intr-write");
	(&client->user_intr.write_record)->pa2va_offset = offset;
	(&client->user_intr.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->acpu_user_intr_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->acpu_user_intr_read_record;

	acpu->user_intr_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	acpu->user_intr_read_record = rpc_record;
	setup_record(acpu, rpc_record, &client->user_intr.read_record, ringbuf_paddr);
	snprintf((&client->user_intr.read_record)->name, 32, "%s", "acpu-intr-read");
	(&client->user_intr.read_record)->pa2va_offset = offset;
	(&client->user_intr.read_record)->big_endian = client->big_endian;

	rpc_client_user_intr_init(rpc_dev, client);

	return result;
}

static int acpu_rpc_kern_init(struct acpu_device *acpu)
{
	int result = 0;
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct rpc_comm_buffer *rpc_comm_buffer;
	struct rpc_record *rpc_record;
	uint64_t ringbuf_vaddr, offset;
	uint32_t ringbuf_paddr;

	client = &acpu->client;
	rpc_dev = acpu->rpc_dev;
	rpc_comm_buffer = (struct rpc_comm_buffer *)rpc_dev->rpc_ringbuf_vaddr;
	offset = rpc_dev->ringbuf_paddr2vaddr_offset;

	/* init rpc_record for write record */
	ringbuf_vaddr =(uint64_t)(uintptr_t)&rpc_comm_buffer->acpu_kern_write_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->acpu_kern_write_record;

	acpu->kern_write_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	acpu->kern_write_record = rpc_record;
	setup_record(acpu, rpc_record, &client->kern.write_record, ringbuf_paddr);
	snprintf((&client->kern.write_record)->name, 32, "%s", "acpu-kern-write");
	(&client->kern.write_record)->pa2va_offset = offset;
	(&client->kern.write_record)->big_endian = client->big_endian;

	/* init rpc_record for read record */
	ringbuf_vaddr = (uint64_t)(uintptr_t)&rpc_comm_buffer->acpu_kern_read_ring;
	ringbuf_paddr = ringbuf_vaddr - offset;
	rpc_record = (struct rpc_record *)&rpc_comm_buffer->acpu_kern_read_record;

	acpu->kern_read_ring = (void __iomem *)(uintptr_t)ringbuf_vaddr;
	acpu->kern_read_record = rpc_record;
	setup_record(acpu, rpc_record, &client->kern.read_record, ringbuf_paddr);
	snprintf((&client->kern.read_record)->name, 32, "%s", "acpu-kern-read");
	(&client->kern.read_record)->pa2va_offset = offset;
	(&client->kern.read_record)->big_endian = client->big_endian;

	rpc_client_kern_init(client);

	return result;
}

/* callback */
static int acpu_rpc_interrupt_init(struct acpu_device *acpu)
{
	//struct acpu_device *acpu = container_of(client, struct rpc_client, client);
	int ret = -1;

	if (acpu->hwlock)
		hwspin_lock_timeout_raw(acpu->hwlock, UINT_MAX);
	RPC_SET_BIT(acpu->rpc_int_flag, RPC_AUDIO_SET_NOTIFY);
	if (acpu->hwlock)
		hwspin_unlock_raw(acpu->hwlock);

	writel_relaxed(RPC_INT_SA | RPC_INT_WRITE_1,
		    acpu->rpc_int_base + RPC_SB2_INT_EN);

	dev_dbg(acpu->dev, "%s rpc_int=0x%x rpc_int_flag=0x%x\n",
		    __func__, readl(acpu->rpc_int_base + RPC_SB2_INT_EN),
		    readl(acpu->rpc_int_flag));

	ret = request_irq(acpu->rpc_irq,
			  acpu_rpc_isr, IRQF_SHARED | IRQF_NO_SUSPEND,
			  "acpu_rpc",
			  (void *) acpu);
	if (ret) {
		dev_err(acpu->dev, "register acpu irq handler failed\n");
		goto exit;
	}

	/* Enable the interrupt from system to audio & video */
	acpu_rpc_set_flag(acpu, 0xffffffff);
	acpu_rpc_send_interrupt(&acpu->client);

exit:
	return ret;

}

#ifdef CONFIG_PM
/*
 * Disable the interrupt from system to audio & video
 */
static int acpu_suspend(struct rpc_client* client)
{
	struct acpu_device *acpu = container_of(client, struct acpu_device, client);
	struct device *dev = acpu->dev;
	void __iomem *rpc_int_flag = acpu->rpc_int_flag;
	int max_count = 500;

	dev_info(dev, "enter %s\n", __func__);

	acpu_rpc_set_flag(acpu, 0xdaedffff); /* STOP AUDIO HAS_CHECK */
	while ((acpu_rpc_get_flag(acpu) != 0x0) && (max_count > 0)) {
		mdelay(1);
		max_count--;
	}

	RPC_RESET_BIT(rpc_int_flag, RPC_AUDIO_SET_NOTIFY); /* Disable Interrupt */

	wmb();

	acpu_rpc_set_flag(acpu, 0xdeadffff); /* WAIT AUDIO RPC SUSPEND READY */
	while ((acpu_rpc_get_flag(acpu) != 0x0) && (max_count > 0)) {
		mdelay(1);
		max_count--;
	}

	dev_info(dev, "wait %d ms\n", (500 - max_count));

	dev_info(dev, "exit %s\n", __func__);

	return 0;
}

/*
 * Enable the interrupt from system to audio & video
 */
static int acpu_resume(struct rpc_client *client)
{
	struct acpu_device *acpu = container_of(client, struct acpu_device, client);
	struct device *dev = acpu->dev;
	void __iomem *rpc_int_flag = acpu->rpc_int_flag;

	dev_info(dev, "enter %s\n", __func__);

	RPC_SET_BIT(rpc_int_flag, RPC_AUDIO_SET_NOTIFY);
	acpu_rpc_set_flag(acpu, 0xffffffff);

	dev_info(dev, "exit %s\n", __func__);

	return 0;
}

static void acpu_shutdown(struct rpc_client *client)
{
	struct acpu_device *acpu = container_of(client, struct acpu_device, client);
	struct device *dev = acpu->dev;
	void __iomem *rpc_int_flag = acpu->rpc_int_flag;
	int max_count = 500;

	dev_info(dev, "enter %s\n", __func__);

	acpu_rpc_set_flag(acpu, 0xdaedffff); /* STOP AUDIO HAS_CHECK */
	while ((acpu_rpc_get_flag(acpu) != 0x0) && (max_count > 0)) {
		mdelay(1);
		max_count--;
	}

	/* disable Interrupt */
	RPC_RESET_BIT(rpc_int_flag, RPC_AUDIO_SET_NOTIFY);

	wmb();

	acpu_rpc_set_flag(acpu, 0xdeadffff); /* WAIT AUDIO RPC SUSPEND READY */
	while ((acpu_rpc_get_flag(acpu) != 0x0) && (max_count > 0)) {
		mdelay(1);
		max_count--;
	}

	dev_info(dev, "wait %d ms\n", (500 - max_count));
	dev_info(dev, "Exit %s\n", __func__);
}
#endif /* CONFIG_PM */


static int acpu_rpc_probe(struct platform_device *pdev)
{
	struct device *dev, *parent_dev;
	struct device_node *node;
	struct acpu_device* acpu;
	struct rpc_device *rpc_dev = NULL;
	struct rpc_client *client;
	int lock_id;
	int ret = 0;

	dev_info(&pdev->dev, "enter %s\n", __func__);

	dev = &pdev->dev;
	acpu = devm_kzalloc(dev, sizeof(struct acpu_device), GFP_KERNEL);
	if (!acpu) {
		ret = -ENOMEM;
		return ret;
	}

	acpu->dev = dev;
	platform_set_drvdata(pdev, acpu);

	node = pdev->dev.of_node;
	if (WARN_ON(!node))
		dev_err(dev, "can not found device node\n");

	acpu->rpc_int_base = of_iomap(node, 0);
	if (WARN_ON(!acpu->rpc_int_base)) {
		dev_warn(dev, "can not map registers for %s\n", node->name);
		goto exit;
	}

	acpu->rpc_irq = irq_of_parse_and_map(node, 0);
	if (WARN_ON(!acpu->rpc_irq))
		dev_warn(dev, "can not parse ACPU irq\n");

	dev_info(dev, "rpc_int_base: %px\n", acpu->rpc_int_base);
	dev_info(dev, "acpu irq: %d\n", acpu->rpc_irq);

	spin_lock_init(&acpu->ASLock);

	parent_dev = dev->parent;
	if (parent_dev && (rpc_dev = dev_get_drvdata(parent_dev))) {
		struct rtk_ipc_shm __iomem *ipc;

		acpu->rpc_dev = rpc_dev;
		ipc = (void __iomem *) rpc_dev->ipc_shm_vaddr;
		if (ipc) {
			acpu->rpc_int_flag = (void __iomem *)&ipc->vo_int_sync;
			acpu->audio_rpc_flag = (void __iomem *)&ipc->audio_rpc_flag;
		}
		dev_dbg(dev, "rpc_int_flag: %px value=0x%x\n",
			    acpu->rpc_int_flag, readl(acpu->rpc_int_flag));
		dev_dbg(dev, "audio_rpc_flag: %px value=0x%x\n",
			    acpu->audio_rpc_flag, readl(acpu->audio_rpc_flag));
	}

	client = &acpu->client;
	client->ringbuf_paddr2vaddr_offset = rpc_dev->ringbuf_paddr2vaddr_offset;
	client->dev = dev;
	client->id = RPC_AUDIO;
	client->big_endian = true;
	snprintf(client->name, 32, "%s", "acpu");

	client->send_interrupt = &acpu_rpc_send_interrupt;

	client->suspend = &acpu_suspend;
	client->resume = &acpu_resume;
	client->shutdown = &acpu_shutdown;

	acpu_rpc_user_poll_init(acpu);
	acpu_rpc_user_intr_init(acpu);
	acpu_rpc_kern_init(acpu);

	rpc_client_register(rpc_dev, client);

	lock_id = of_hwspin_lock_get_id(dev->of_node, 0);
	if (lock_id > 0 || (IS_ENABLED(CONFIG_HWSPINLOCK) && lock_id == 0)) {
		struct hwspinlock *lock = devm_hwspin_lock_request_specific(dev, lock_id);

		if (lock) {
			dev_info(dev, "use hwlock%d\n", lock_id);
			acpu->hwlock = lock;
		}
	} else {
		if (lock_id != -ENOENT)
			dev_err(dev, "failed to get hwlock: %pe\n", ERR_PTR(lock_id));
	}

	acpu_rpc_interrupt_init(acpu);

	dev_info(dev, "exit %s (ret=%d)\n", __func__, ret);
exit:
	return ret;
}

static int __maybe_unused acpu_rpc_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id acpu_rpc_ids[] = {
	{.compatible = "realtek,acpu_rpc" },
	{/* Sentinel */ },
};

static struct platform_driver acpu_rpc_driver = {
	.probe = acpu_rpc_probe,
	.remove = acpu_rpc_remove,
	.driver = {
		.name = "realtek-acpu-rpc",
		.of_match_table = acpu_rpc_ids,
	},
};

int acpu_rpc_init(void)
{
	return platform_driver_register(&acpu_rpc_driver);
}
//device_initcall(acpu_rpc_init);

MODULE_LICENSE("GPL");
