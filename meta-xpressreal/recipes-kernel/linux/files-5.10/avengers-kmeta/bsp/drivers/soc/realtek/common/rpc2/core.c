// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek RPC driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */
//#define DEBUG

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <asm/memory.h>

#include <soc/realtek/rtk_refclk.h>
#include <soc/realtek/rtk_ipc_shm.h>
#include "rpc.h"

static struct rpc_device *g_rpc_dev;
static DECLARE_RWSEM(rpc_dev_rwsem);

struct rpc_device *get_rpc_dev(void)
{
	if (!g_rpc_dev) {
		pr_err("%s rpc_device is not ready\n", __func__);
		return 0;
	}

	return g_rpc_dev;
}

uint32_t rpc_ringbuf_get_ring_data_size(struct rpc_record_mapping *record)
{
	volatile uint32_t *ringStart, *ringEnd, *ringIn, *ringOut;
	int32_t ringsize, datasize;

	ringStart = record->ringStart;
	ringEnd = record->ringEnd;
	ringIn = record->ringIn;
	ringOut = record->ringOut;

	ringsize = *ringEnd - *ringStart;
	datasize = (ringsize + *ringIn - *ringOut) % ringsize;

	pr_debug("%s %s ringsize:%d datasize:%d start:%x end:%x out:%x in:%x\n",
		    __func__, record->name, ringsize, datasize,
		    *ringStart, *ringEnd, *ringOut, *ringIn);

	return datasize;
}

bool rpc_ringbuf_empty(struct rpc_record_mapping *record)
{
	volatile uint32_t *ringStart, *ringEnd, *ringIn, *ringOut;

	ringStart = record->ringStart;
	ringEnd = record->ringEnd;
	ringIn = record->ringIn;
	ringOut = record->ringOut;

	pr_debug("%s %s ringStart: 0x%x  ringEnd: 0x%x ringOut:%x ringIn:%x\n",
		    __func__, record->name,
		    *ringStart, *ringEnd, *ringOut, *ringIn);

	return *ringIn == *ringOut;
}

uint32_t rpc_ringbuf_get_ringOut(struct rpc_record_mapping *record)
{
	volatile uint32_t *ringOut;

	ringOut = record->ringOut;

	pr_debug("%s %s ringOut:%x\n", __func__, record->name, *ringOut);

	return *ringOut;
}

uint32_t rpc_ringbuf_set_ringOut(struct rpc_record_mapping *record,
	    uint32_t out)
{
	volatile uint32_t *ringOut;

	down_write(&record->Sem);
	ringOut = record->ringOut;
	*ringOut = out;
	up_write(&record->Sem);

	pr_debug("%s %s ringOut:%x\n", __func__, record->name, *ringOut);

	return 0;
}

uint32_t rpc_ringbuf_update_ringOut_by_size(struct rpc_record_mapping *record,
	    uint32_t size)
{
	volatile uint32_t *ringStart, *ringEnd, *ringIn, *ringOut;

	ringStart = record->ringStart;
	ringEnd = record->ringEnd;
	ringIn = record->ringIn;
	ringOut = record->ringOut;

	down_write(&record->Sem);
	*ringOut = *ringOut + size;
	if (*ringOut >= *ringEnd)
		*ringOut = *ringStart + (*ringOut - *ringEnd);
	up_write(&record->Sem);

	return 0;
}

uint32_t rpc_ringbuf_get_next_ringOut_by_size(
	    struct rpc_record_mapping *record, uint32_t size)
{
	volatile uint32_t *ringStart, *ringEnd, *ringIn, *ringOut;
	uint32_t out;

	ringStart = record->ringStart;
	ringEnd = record->ringEnd;
	ringIn = record->ringIn;
	ringOut = record->ringOut;

	down_write(&record->Sem);
	out = *ringOut + size;
	if (out >= *ringEnd)
		out = *ringStart + (out - *ringEnd);
	up_write(&record->Sem);

	return out;
}

/*
 * get ring data in buf and return next data pointer
 */
uint32_t rpc_ringbuf_reading_data(struct rpc_record_mapping *record,
		uint32_t out, char *buf, int datasize)
{
	uint32_t *ringStart, *ringEnd;
	volatile uint32_t *ringIn;
	int size, tail;
	bool to_user = false;
	uint64_t pa2va_offset = record->pa2va_offset;
	uintptr_t addr = 0;

	ringStart = record->ringStart;
	ringEnd = record->ringEnd;
	ringIn = record->ringIn;

	pr_debug("%s %s ringStart: 0x%x  ringEnd: 0x%x out:%x ringIn:%x pa2va_offset:%llx\n",
		    __func__, record->name,
		    *ringStart, *ringEnd, out, *ringIn, pa2va_offset);

	size = rpc_ringbuf_get_ring_data_size(record);

	pr_debug("%s datasize:%d avail:%d\n", __func__, datasize, size);

	tail = *ringEnd - out;

	if (size < datasize) {
		pr_err("%s: Size not enough %d < %d\n", __func__, size, datasize);
		return 0;
	}

	if (tail >= datasize) {
		addr = (out + pa2va_offset);
		rpc_read_copy((int *)buf, (int *)addr, datasize, to_user);
		out += datasize;
	} else {
		addr = (out + pa2va_offset);
		rpc_read_copy((int *)buf, (int *)addr, tail, to_user);

		addr = (*ringStart + pa2va_offset);
		rpc_read_copy((int *)(buf + tail), (int *)addr, datasize - tail, to_user);

		out = *ringStart + datasize - tail;
	}

	return out;
}

ssize_t rpc_ringbuf_read(struct rpc_record_mapping *record,
	    char *buf, size_t count, bool to_user)
{
	int temp, size;
	size_t r;
	ssize_t ret = 0;
	uint32_t ptmp;
	int rpc_ring_size;
	volatile uint32_t *ringStart, *ringEnd, *ringIn, *ringOut;
	uint64_t pa2va_offset = record->pa2va_offset;
	uintptr_t addr = 0;

	ringStart = record->ringStart;
	ringEnd = record->ringEnd;
	ringIn = record->ringIn;
	ringOut = record->ringOut;
	rpc_ring_size = *ringEnd - *ringStart;

	down_write(&record->Sem);

	pr_debug("%s %s ringStart: 0x%x  ringEnd: 0x%x ringOut:%x ringIn:%x pa2va_offset:%llx\n",
		    __func__, record->name,
		    *ringStart, *ringEnd, *ringOut, *ringIn, pa2va_offset);

	if (*ringIn > *ringOut)
		size = *ringIn - *ringOut;
	else
		size = rpc_ring_size + *ringIn - *ringOut;

	pr_debug("%s:%d ==going read== count:%zu avail:%d\n",
			__func__, __LINE__, count, size);

	if (count > size)
		count = size;
	temp = *ringEnd - *ringOut;
	wmb();

	if (temp >= count) {
		addr = (*ringOut + pa2va_offset);
		r = rpc_read_copy((int *)buf, (int *)addr, count, to_user);
		if (r) {
			pr_err("%s:%d buf:%p count:%zx EFAULT\n",
				__func__, __LINE__, buf, count);
			ret = -EFAULT;
			goto out;
		}

		ret = count;
		ptmp = *ringOut + ((count+3) & 0xfffffffc);
		if (ptmp == *ringEnd)
			*ringOut = *ringStart;
		else
			*ringOut = ptmp;

		//pr_debug("RPC Read is in 1st kind...\n");
	} else {
		addr = (*ringOut + pa2va_offset);
		r = rpc_read_copy((int *)buf, (int *)addr, temp, to_user);
		if (r) {
			pr_err("%s:%d buf:%p count:%zx EFAULT\n",
					__func__, __LINE__, buf, count);
			ret = -EFAULT;
			goto out;
		}

		count -= temp;
		addr = (*ringStart + pa2va_offset);
		r = rpc_read_copy((int *)(buf + temp), (int *)addr, count, to_user);
		if (r) {
			pr_err("%s:%d buf:%p count:%zx EFAULT\n",
					__func__, __LINE__, buf, count);
			ret = -EFAULT;
			goto out;
		}
		ret = (temp + count);
		*ringOut = *ringStart+((count+3) & 0xfffffffc);

		//pr_debug("RPC Read is in 2nd kind...\n");
	}

	up_write(&record->Sem);

	return ret;
out:
	pr_err("[%s] read error occur\n", __func__);
	up_write(&record->Sem);

	return ret;
}

ssize_t rpc_ringbuf_write(struct rpc_record_mapping *record,
	     const char *buf, size_t count, bool from_user)
{
	int temp, size;
	size_t r;
	ssize_t ret = 0;
	uint32_t ptmp;
	volatile uint32_t *ringStart, *ringEnd, *ringIn, *ringOut;
	uint64_t pa2va_offset = record->pa2va_offset;
	uintptr_t addr = 0;

	ringStart = record->ringStart;
	ringEnd = record->ringEnd;
	ringIn = record->ringIn;
	ringOut = record->ringOut;

	pr_debug("%s %s ringStart: 0x%x  ringEnd: 0x%x ringOut:%x ringIn:%x pa2va_offset:%llx\n",
		    __func__, record->name,
		    *ringStart, *ringEnd, *ringOut, *ringIn, pa2va_offset);

	down_write(&record->Sem);

	if (*ringIn == *ringOut)
		size = 0;   // the ring is empty
	else if (*ringIn > *ringOut)
		size = *ringIn - *ringOut;
	else
		size = RPC_RING_SIZE + *ringIn - *ringOut;

	if (count > (RPC_RING_SIZE - size - 1))
		goto out;

	temp = *ringEnd - *ringIn;
	if (temp >= count) {
		addr = *ringIn + pa2va_offset;
		r = rpc_write_copy((int *) addr, (int *) buf, count, from_user);
		if (r) {
			ret = -EFAULT;
			goto out;
		}
		ret += count;
		ptmp = *ringIn + ((count+3) & 0xfffffffc);

		//asm("DSB");
		mb();

		if (ptmp == *ringEnd)
			*ringIn = *ringStart;
		else
			*ringIn = ptmp;

		pr_debug("RPC Write is in 1st kind...\n");
	} else {
		addr = *ringIn + pa2va_offset;
		r = rpc_write_copy((int *) addr, (int *) buf, temp, from_user);
		if (r) {
			ret = -EFAULT;
			goto out;
		}
		count -= temp;

		addr = *ringStart + pa2va_offset;
		r = rpc_write_copy((int *) addr, (int *) (buf + temp), count, from_user);
		if (r) {
			ret = -EFAULT;
			goto out;
		}
		ret += (temp + count);

		//asm("DSB");
		mb();

		*ringIn = *ringStart+((count+3) & 0xfffffffc);

		pr_debug("RPC Write is in 2nd kind...\n");
	}

	wmb();

	up_write(&record->Sem);

	return ret;
out:
	pr_err("[%s]RingBuf full! RPC FW intr write ringIn pointer is : %llx\n",
		    __func__, *ringIn + pa2va_offset);
	up_write(&record->Sem);
	return ret;
}

int dump_rpc_ringbuf(struct rpc_record_mapping *record)
{
	int i;
	uint64_t offset = record->pa2va_offset;
	bool big_endian = record->big_endian;
	uint32_t *addr;
	uintptr_t addr_tmp = 0;

	pr_err("%s:\n", record->name);
	pr_err("RingBuf@0x%x\n", *record->ringBuf);
	pr_err("RingStart:0x%x\n", *record->ringStart);
	pr_err("RingIn:0x%x\n", *record->ringIn);
	pr_err("RingOut:0x%x\n", *record->ringOut);
	pr_err("RingEnd:0x%x\n", *record->ringEnd);
	pr_err("RingBuffer:\n");
	for (i = 0; i < RPC_RING_SIZE; i += 16) {
		addr_tmp = ((*record->ringStart + offset) + i);
		addr = (uint32_t *)addr_tmp;

		pr_err("%x: %08x %08x %08x %08x\n",
			*(record->ringStart) + i,
			FW2SCPU(big_endian, *(addr + 0)),
			FW2SCPU(big_endian, *(addr + 1)),
			FW2SCPU(big_endian, *(addr + 2)),
			FW2SCPU(big_endian, *(addr + 3)));
	}

	return 0;
}

void rpc_send_interrupt(struct rpc_client *client)
{
	if (client->send_interrupt)
		client->send_interrupt(client);
	else
		pr_err("%s no send_interrupt for client %s\n",
			    __func__, client->name);
}

static char *rpc_class_devnode(struct device *dev, umode_t *mode)
{
	*mode = 0666;
	return NULL;
}

static ssize_t kernel_remote_allocate_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", 1);
}
static CLASS_ATTR_RO(kernel_remote_allocate);

extern struct file_operations rpc_ctrl_fops;

static int rpc_default_open(struct inode *inode, struct file *filp)
{
	int minor = MINOR(inode->i_rdev);
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct list_head *listptr;
	struct list_head *clients;

	pr_debug("RPC open with minor number: %d\n", minor);

	rpc_dev = get_rpc_dev();
	if (!rpc_dev) {
		pr_err("%s rpc_device is not ready\n", __func__);
		return 0;
	}

	if (minor == 100) {
		pr_debug("RPC use rpc_ctrl_fops for minor number: %d\n", minor);

		inode->i_private = rpc_dev;
		filp->f_op = &rpc_ctrl_fops;
		return filp->f_op->open(inode, filp); /* dispatch to specific open */
	}

	down_read(&rpc_dev_rwsem);

	clients = &rpc_dev->clients;
	list_for_each(listptr, clients) {
		client = list_entry(listptr, struct rpc_client, list);
		if (client) {
			struct user_rpc *user_poll = NULL;
			struct user_rpc *user_intr = NULL;

			dev_dbg(rpc_dev->dev, "%s get client for %s\n",
				    __func__, client->name);

			if (client->is_support_poll)
				user_poll = &client->user_poll;
			if (client->is_support_intr)
				user_intr = &client->user_intr;

			if (user_poll &&
				    (user_poll->channel_id_write == minor ||
				    user_poll->channel_id_read == minor)) {
				inode->i_private = user_poll;
				replace_fops(filp, user_poll->f_ops);
				break;
			} else if (user_intr &&
				    (user_intr->channel_id_write == minor ||
				    user_intr->channel_id_read == minor)) {
				inode->i_private = user_intr;
				replace_fops(filp, user_intr->f_ops);
				break;
			}
		}
	}

	up_read(&rpc_dev_rwsem);

	dev_dbg(rpc_dev->dev, "%s client for %s user_intr=%px user_poll=%px\n",
		    __func__, client->name,
		    &client->user_intr, &client->user_poll);

	if (inode->i_private && filp->f_op->open)
		return filp->f_op->open(inode, filp);
	else
		pr_err("%s rpc_client for minor %d\n", __func__, minor);

	return 0;
}

struct file_operations rpc_default_fops = {
	.open = rpc_default_open,
};

int rpc_client_register(struct rpc_device *rpc_dev, struct rpc_client *client)
{
	client->rpc_dev = rpc_dev;

	INIT_LIST_HEAD(&client->process_groups.list);
	spin_lock_init(&client->process_group_lock);

	down_read(&rpc_dev_rwsem);
	list_add_tail(&client->list, &rpc_dev->clients);
	up_read(&rpc_dev_rwsem);

	add_rpc_debugfs_client(rpc_dev, client);

	return 0;
}

int rpc_client_unregister(struct rpc_device *rpc_dev, struct rpc_client *client)
{
	remove_rpc_debugfs_client(rpc_dev, client);

	down_read(&rpc_dev_rwsem);
	list_del(&client->list);
	up_read(&rpc_dev_rwsem);
	return 0;
}

struct rpc_client *rpc_client_get(int type)
{
	struct rpc_device *rpc_dev;
	struct rpc_client *client;
	struct list_head *listptr;
	struct list_head *clients;

	rpc_dev = get_rpc_dev();
	if (!rpc_dev) {
		pr_err("%s rpc_device is not ready\n", __func__);
		return NULL;
	}

	dev_dbg(rpc_dev->dev, "enter %s\n", __func__);

	down_read(&rpc_dev_rwsem);
	clients = &rpc_dev->clients;
	list_for_each(listptr, clients) {
		client = list_entry(listptr, struct rpc_client, list);
		if (client && client->id == type) {
			dev_dbg(rpc_dev->dev, "exit %s get client for type %d\n",
				    __func__, type);
			up_read(&rpc_dev_rwsem);
			return client;
		}
	}
	up_read(&rpc_dev_rwsem);

	dev_dbg(rpc_dev->dev, "exit %s no client for type %d\n", __func__, type);

	return NULL;
}

static int rpc_sysfs_class_init(struct rpc_device *rpc_dev)
{
	int ret;
	struct device *dev;
	int rpc_major;
	struct class *rpc_class;

	dev = rpc_dev->dev;
	rpc_major = RPC_MAJOR;

	ret = register_chrdev(rpc_major, "realtek-rpc", &rpc_default_fops);
	if (ret < 0) {
		dev_dbg(dev, "can not get major %d\n", rpc_major);
		goto exit;
	}

	if (rpc_major == 0)
		rpc_major = ret; /* dynamic */
	rpc_dev->rpc_major = rpc_major;

	dev_dbg(dev, "rpc major number: %d\n", rpc_dev->rpc_major);

	rpc_class = class_create(THIS_MODULE, "rpc");
	if (IS_ERR(rpc_class)) {
		ret = PTR_ERR(rpc_class);
		goto exit;
	}
	rpc_dev->rpc_class = rpc_class;
	rpc_class->devnode = rpc_class_devnode;

	ret = class_create_file(rpc_class,
		    &class_attr_kernel_remote_allocate);
	if (ret) {
		dev_err(dev, "create class file failed\n");
		ret = -EINVAL;
		goto exit;
	}

	device_create(rpc_class, NULL, MKDEV(rpc_major, 100), rpc_dev, "rpc100");

	rpc_mem_init(rpc_dev);
exit:
	return ret;
}

static int __maybe_unused rtk_rpc_get_reserved_memory(struct device *dev,
	    struct device_node *np, int index,
	    phys_addr_t *paddr, void __iomem **vaddr)
{
	struct device_node *node;
	struct resource res;
	int ret;

	*paddr = 0;
	*vaddr = 0;

	/* Get reserved memory region from Device-tree */
	node = of_parse_phandle(np, "memory-region", index);
	if (!np) {
		dev_err(dev, "No %s specified\n", "memory-region");
		return -ENODEV;
	}

	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		dev_err(dev, "No memory address assigned to the region\n");
		return -ENOMEM;
	}

	*paddr = res.start;
	if (*paddr) {
		struct page **pages;
		struct page **tmp;
		int i = 0;
		int npages = 0;

		npages = resource_size(&res) >> PAGE_SHIFT;
		pages = vmalloc(sizeof(struct page *) * npages);
		if (!pages) {
			dev_err(dev, "%s, fail to allocate memory\n", __func__);
			ret = -ENOMEM;
			return ret;
		}
		for (i = 0, tmp = pages; i < npages; i++)
			*(tmp++) = phys_to_page(res.start + (PAGE_SIZE * i));
		*vaddr = vmap(pages, npages,
			    VM_MAP, pgprot_writecombine(PAGE_KERNEL));
		vfree(pages);
		dev_info(dev, "Allocated reserved memory, vaddr: 0x%llx, paddr: 0x%x\n",
			    (u64)(uintptr_t)*vaddr, (unsigned int)*paddr);
	}

	return 0;
}

static int rtk_rpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node;
	struct rpc_device* rpc_dev;
	phys_addr_t paddr;
	void __iomem *vaddr;
	int ret;

	dev_info(dev, "Enter %s\n", __func__);

	rpc_dev = devm_kzalloc(dev, sizeof(struct rpc_device), GFP_KERNEL);
	if (!rpc_dev) {
		ret = -ENOMEM;
		goto err1;
	}

	rpc_dev->dev = dev;
	platform_set_drvdata(pdev, rpc_dev);

	node = pdev->dev.of_node;
	if (WARN_ON(!node)) {
		dev_err(dev, "%s can not found device node\n", __func__);
		ret = -ENODEV;
		goto err1;
	}

	ret = rtk_rpc_get_reserved_memory(dev, node, 0, &paddr, &vaddr);
	if (ret) {
		dev_err(dev, "%s can get reserved memory for rpc common buffer\n",
			     __func__);
		goto err1;
	}

	rpc_dev->rpc_common_paddr = paddr;
	rpc_dev->rpc_common_vaddr = (void __iomem *)vaddr;
	rpc_dev->ipc_shm_vaddr = rpc_dev->rpc_common_vaddr + IPC_SHM_OFFSET;

	ret = rtk_rpc_get_reserved_memory(dev, node, 1, &paddr, &vaddr);
	if (ret) {
		dev_err(dev, "%s can get reserved memory for rpc ringbuf buffer\n",
			     __func__);
		goto err1;
	}
	rpc_dev->rpc_ringbuf_paddr = paddr;
	rpc_dev->rpc_ringbuf_vaddr = (void __iomem *)vaddr;
	rpc_dev->ringbuf_paddr2vaddr_offset = (uint64_t)(uintptr_t)rpc_dev->rpc_ringbuf_vaddr -
		    rpc_dev->rpc_ringbuf_paddr;

	dev_dbg(dev, "%s ringbuf_paddr2vaddr_offset = %llx\n",
		    __func__, rpc_dev->ringbuf_paddr2vaddr_offset);

	INIT_LIST_HEAD(&rpc_dev->clients);

	ret = rpc_sysfs_class_init(rpc_dev);

	add_rpc_debugfs(rpc_dev);

	g_rpc_dev = rpc_dev;

	dev_info(dev, "%s populate subnode device\n", __func__);
	ret = of_platform_populate(node, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "%s failed to add subnode device\n", __func__);
		goto err1;
	}

err1:
	dev_info(dev, "Exit %s (ret=%d)\n", __func__, ret);
	return ret;
}

static int __maybe_unused rtk_rpc_remove(struct platform_device *pdev)
{

	//remove_rpc_debugfs(rpc_dev);
	return 0;
}

#ifdef CONFIG_PM
/*
 * Disable the interrupt from system to audio & video
 */
static int rtk_rpc_pm_suspend(struct device *dev)
{
	struct rpc_device *rpc_dev = dev_get_drvdata(dev);
	struct list_head *listptr;
	struct list_head *clients;
	struct rpc_client *entry;

	dev_info(dev, "enter %s\n", __func__);

	clients = &rpc_dev->clients;
	list_for_each(listptr, clients) {
		entry = list_entry(listptr, struct rpc_client, list);
		if (entry && entry->suspend) {
			entry->suspend(entry);
		}
	}

	dev_info(dev, "exit %s\n", __func__);

	return 0;
}

/*
 * Enable the interrupt from system to audio & video
 */
static int rtk_rpc_pm_resume(struct device *dev)
{
	struct rpc_device *rpc_dev = dev_get_drvdata(dev);
	struct list_head *listptr;
	struct list_head *clients;
	struct rpc_client *entry;

	dev_info(dev, "enter %s\n", __func__);

	clients = &rpc_dev->clients;
	list_for_each(listptr, clients) {
		entry = list_entry(listptr, struct rpc_client, list);
		if (entry && entry->resume) {
			entry->resume(entry);
		}
	}

	dev_info(dev, "exit %s\n", __func__);

	return 0;
}
static void rtk_rpc_pm_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rpc_device *rpc_dev = dev_get_drvdata(dev);
	struct list_head *listptr;
	struct list_head *clients;
	struct rpc_client *entry;

	dev_info(dev, "enter %s\n", __func__);

	clients = &rpc_dev->clients;
	list_for_each(listptr, clients) {
		entry = list_entry(listptr, struct rpc_client, list);
		if (entry && entry->shutdown) {
			entry->shutdown(entry);
		}
	}

	dev_info(dev, "exit %s\n", __func__);
}

static const struct dev_pm_ops rtk_rpc_pm_ops = {
	.suspend_late = rtk_rpc_pm_suspend,
	.resume_early = rtk_rpc_pm_resume,
	.poweroff = rtk_rpc_pm_suspend,
#ifdef CONFIG_HIBERNATION
	.freeze = rtk_rpc_pm_suspend,
	.thaw = rtk_rpc_pm_resume,
	.restore = rtk_rpc_pm_resume,
#endif
};
#endif /* CONFIG_PM */

static struct of_device_id rtk_rpc_ids[] = {
	{.compatible = "realtek,rpc" },
	{/* Sentinel */ },
};

static struct platform_driver rtk_rpc_driver = {
	.probe = rtk_rpc_probe,
	.remove = rtk_rpc_remove,
#ifdef CONFIG_PM
	.shutdown = rtk_rpc_pm_shutdown,
#endif /* CONFIG_PM */
	.driver = {
		.name = "realtek-rpc",
		.bus = &platform_bus_type,
		.of_match_table = rtk_rpc_ids,
	},
};

static int rtk_rpc_init(void)
{
	rtk_rpc_client_init();
	return platform_driver_register(&rtk_rpc_driver);
}
device_initcall(rtk_rpc_init);

MODULE_LICENSE("GPL");
