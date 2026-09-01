/* SPDX-License-Identifier: GPL-2.0-or-later
* Driver for Realtek PCI-Express Multi-IO bridge
*
* Copyright(c) 2025 Realtek Semiconductor Corp. All rights reserved.
*
* Author:
*   Network Interface Controllers crew <nicfae@realtek.com>
*/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/fs.h>       // struct file_operations
#include <linux/mm.h>       // mmap
#include <linux/slab.h>     // kmalloc
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,4,0)
#include <linux/pci-aspm.h>
#endif
#include <asm/io.h>
#include <asm/uaccess.h>        // copy_to_user
#include <linux/uaccess.h>      // copy_to_user
#include <asm/byteorder.h>

#include "pcibr_iodrv.h"
#include "pcibr_iodrv_core.h"

static DEV_INFO     dev_info[MAX_DEV_NUM] = {};
static atomic_t     dev_num;
static spinlock_t   module_lock;
static int          major = 0;
static struct class *sys_class;
static dev_t        devno;

module_param(major, int, S_IRUGO|S_IWUSR);

static struct pci_device_id pcibr_iodrv_id_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x9151), },
	{0,},
};

MODULE_DEVICE_TABLE(pci, pcibr_iodrv_id_table);

static long dev_ioctl(struct file *pFile, unsigned int cmd, unsigned long arg)
{
	PCIBR_IODRV_DEV *pcibr_dev;
	int result = 0;

	if (_IOC_TYPE(cmd) != RTL_IOC_MAGIC)
	{
		DbgPrint("Invalid command!!!");
		return -ENOTTY;
	}

	pcibr_dev = (PCIBR_IODRV_DEV *)pFile->private_data;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	if (down_interruptible(&pcibr_dev->dev_sem))
	{
		ErrPrint("lock fail");
		return -ERESTARTSYS;
	}
#else
	if (mutex_lock_interruptible(&pcibr_dev->dev_mutex))
	{
		ErrPrint("lock fail");
		return -ERESTARTSYS;
	}
#endif

	switch (cmd)
	{
		case IOC_PCI_CONFIG:
		{
			PCI_CONFIG_RW pci_config;

			if (copy_from_user(&pci_config, (void __user *)arg, sizeof(PCI_CONFIG_RW)))
			{
				ErrPrint("copy_from_user fail");
				result = -EFAULT;
				break;
			}

			if (pci_config.bar != 0 || pci_config.sig != 0x5245414c)
			{
				result = -EFAULT;
				break;
			}

			if (pci_config.is_read == TRUE)
			{
				if (pci_config.size == 1)
				{
					result = pci_read_config_byte(pcibr_dev->pdev, pci_config.addr, &pci_config.byte);
				}
				else if (pci_config.size == 2)
				{
					result = pci_read_config_word(pcibr_dev->pdev, pci_config.addr, &pci_config.word);
				}
				else if (pci_config.size == 4)
				{
					result = pci_read_config_dword(pcibr_dev->pdev, pci_config.addr, &pci_config.dword);
				}
				else
				{
					result = -EINVAL;
				}

				if (result)
				{
					ErrPrint("Read PCI fail:%d",result);
					break;
				}

				if (copy_to_user((void __user *)arg , &pci_config, sizeof(PCI_CONFIG_RW)))
				{
					ErrPrint("copy_from_user fail");
					result = -EFAULT;
					break;
				}
			}
			else // write
			{
				if (pci_config.size==1)
				{
					result = pci_write_config_byte(pcibr_dev->pdev, pci_config.addr, pci_config.byte);
				}
				else if (pci_config.size==2)
				{
					result = pci_write_config_word(pcibr_dev->pdev, pci_config.addr, pci_config.word);
				}
				else if (pci_config.size==4)
				{
					result = pci_write_config_dword(pcibr_dev->pdev, pci_config.addr, pci_config.dword);
				}
				else
				{
					result = -EINVAL;
				}

				if (result)
				{
					ErrPrint("Write PCI fail:%d",result);
					break;
				}
			}

			DbgPrint("IOC_CONFIG is_read=%u, size=%d, addr=0x%02x , data=0x%08x",
				pci_config.is_read, pci_config.size,
				pci_config.addr, pci_config.dword);

			break;
		}
		case IOC_GET_VERSION:
		{
			if (copy_to_user((char __user *)arg, DRIVER_VERSION, strlen(DRIVER_VERSION) + 1))
				result = -EFAULT;
			break;
		}
		case IOC_PCI_MMIO:
		{
			PCI_CONFIG_RW pci_config;

			if (copy_from_user(&pci_config, (void __user *)arg, sizeof(PCI_CONFIG_RW)))
			{
				ErrPrint("copy_from_user fail");
				result = -EFAULT;
				break;
			}

			if (pci_config.bar > 5 ||
			    pci_config.sig != 0x5245414c ||
			    !pcibr_dev->remap_addr[pci_config.bar])
			{
				result = -EFAULT;
				break;
			}

			result = rtk_pci_mem_rw(pcibr_dev->remap_addr[pci_config.bar],
						pci_config.addr,
						pci_config.size,
						&pci_config.byte,
						pci_config.is_read);
			if (result != 0) {
				result = -EFAULT;
				break;
			}

			if (pci_config.is_read) {
				if (copy_to_user((void __user *)arg , &pci_config, sizeof(PCI_CONFIG_RW)))
				{
					ErrPrint("copy_from_user fail");
					result = -EFAULT;
					break;
				}
			}

			DbgPrint("MMIO bar = %d, is_read=%u, size=%d, addr=0x%08x, data=0x%08x",
				pci_config.bar,
				pci_config.is_read,
				pci_config.size,
				pci_config.addr,
				pci_config.dword);
			break;
		}

		default:
			ErrPrint("Command not support!!!");
			result = -ENOTTY;
			break;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	up(&pcibr_dev->dev_sem);
#else
	mutex_unlock(&pcibr_dev->dev_mutex);
#endif
	return result;
}

static int dev_open(struct inode *inode, struct file *pFile)
{
	PCIBR_IODRV_DEV *pcibr_dev;

	pcibr_dev = container_of(inode->i_cdev, PCIBR_IODRV_DEV, cdev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	atomic_inc(&pcibr_dev->count);
	if (atomic_read(&pcibr_dev->count) > 1)
	{
		atomic_dec(&pcibr_dev->count);
		DbgPrint("Busy");
		return -EMFILE;
	}
#else
	if (atomic_inc_return(&pcibr_dev->count) > 1)
	{
		atomic_dec(&pcibr_dev->count);
		DbgPrint("Busy");
		return -EMFILE;
	}
#endif

	pFile->private_data = pcibr_dev;

	return 0;
}

static int dev_close(struct inode *inode, struct file *pFile)
{
	PCIBR_IODRV_DEV *pcibr_dev;

	pcibr_dev = container_of(inode->i_cdev, PCIBR_IODRV_DEV, cdev);

	pFile->private_data = NULL;

	atomic_dec(&pcibr_dev->count);

	return 0;
}

static struct file_operations pcibr_iodrv_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = dev_ioctl,
	.open           = dev_open,
	.release        = dev_close,
};

static int __devinit pcibr_iodrv_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	PCIBR_IODRV_DEV   *pcibr_dev;
	int     result, i;
	char node_name[20] = {};
	uint32_t mem_size;

	DbgPrint("Enter\r\n");

	if (PCI_FUNC(pdev->devfn) != 0) {
		return -ENODEV;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S |
				PCIE_LINK_STATE_L1 |
				PCIE_LINK_STATE_CLKPM);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	atomic_inc(&dev_num);
	if (atomic_read(&dev_num) > MAX_DEV_NUM)
#else
	if (atomic_inc_return(&dev_num) > MAX_DEV_NUM)
#endif
	{
		atomic_dec(&dev_num);
		DbgPrint("Too Many Device");
		return -EFAULT;
	}

	pcibr_dev = kmalloc(sizeof(PCIBR_IODRV_DEV), GFP_KERNEL);
	if (!pcibr_dev)
	{
		ErrPrint("Allocate dev fail");
		return -ENOMEM;
	}
	memset(pcibr_dev, 0, sizeof(PCIBR_IODRV_DEV));

	spin_lock(&module_lock);
	for (i = 0; i < MAX_DEV_NUM; i++)
	{
		if (dev_info[i].bUsed == FALSE)
		{
			dev_info[i].bUsed = TRUE;
			pcibr_dev->index = i;
			snprintf(node_name, sizeof(node_name), "%s%d", MODULENAME, pcibr_dev->index);
			break;
		}
	}
	spin_unlock(&module_lock);

	pcibr_dev->pdev = pdev;
	atomic_set(&pcibr_dev->count, 0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	init_MUTEX(&pcibr_dev->dev_sem);
#else
	mutex_init(&pcibr_dev->dev_mutex);
#endif

	InfPrint("device = 0x%4x pcibr_dev=%p, major=%u, minor=%u",
				id->device,
				pcibr_dev,
				major,
				MINOR(dev_info[pcibr_dev->index].devno));

	cdev_init(&pcibr_dev->cdev, &pcibr_iodrv_fops);
	pcibr_dev->cdev.owner = THIS_MODULE;
	result = cdev_add(&pcibr_dev->cdev, dev_info[pcibr_dev->index].devno, 1);
	if (result)
	{
		DbgPrint("cdev_add fault");
		spin_lock(&module_lock);
		dev_info[pcibr_dev->index].bUsed = FALSE;
		spin_unlock(&module_lock);
		kfree(pcibr_dev);
		return result;
	}

	pci_set_drvdata(pdev, pcibr_dev);

	result = pci_enable_device(pcibr_dev->pdev);
	if (result < 0)
	{
		ErrPrint("pci_enable_device fail");
		return result;
	}

	result = pci_request_regions(pcibr_dev->pdev, MODULENAME);
	if (result < 0)
	{
		ErrPrint("pci_request_regions fail");
		goto error_2;
	}

	pci_set_master(pcibr_dev->pdev);

	switch(id->device)
	{
	case 0x9151:
		InfPrint("Multi-IO found!\n");
		mem_size = pci_resource_len(pcibr_dev->pdev, 0);
		pcibr_dev->remap_addr[0] = pci_iomap(pcibr_dev->pdev, 0, mem_size);
		if (!pcibr_dev->remap_addr[0]) {
			result = -ENOMEM;
			goto error_1;
		}
		mem_size = pci_resource_len(pcibr_dev->pdev, 2);
		pcibr_dev->remap_addr[2] = pci_iomap(pcibr_dev->pdev, 2, mem_size);
		if (!pcibr_dev->remap_addr[2]) {
			result = -ENOMEM;
			goto error_1;
		}
		break;
	default:
		goto error_1;
		break;
	}

	device_create(sys_class,
			&pdev->dev,
			dev_info[pcibr_dev->index].devno,
			NULL,
			node_name);

	InfPrint("ID=0x%04x %s device node created.\n", id->device, node_name);

	DbgPrint("Exit\r\n");
	return 0;

error_1:
	pci_release_regions(pcibr_dev->pdev);
error_2:
	pci_disable_device(pcibr_dev->pdev);
	cdev_del(&pcibr_dev->cdev);
	kfree(pcibr_dev);

	ErrPrint("Exit\r\n");
	return result;
}

static void __devexit pcibr_iodrv_remove(struct pci_dev *pdev)
{
	PCIBR_IODRV_DEV *pcibr_dev;

	pcibr_dev = pci_get_drvdata(pdev);
	DbgPrint("pcibr_dev=%p", pcibr_dev);

	device_destroy(sys_class, dev_info[pcibr_dev->index].devno);
	InfPrint("%s device node destroy.\n", MODULENAME);

	pci_release_regions(pdev);
	pci_disable_device(pdev);

	cdev_del(&pcibr_dev->cdev);
	spin_lock(&module_lock);
	dev_info[pcibr_dev->index].bUsed = FALSE;
	spin_unlock(&module_lock);
	kfree(pcibr_dev);
	pci_set_drvdata(pdev, NULL);
	atomic_dec(&dev_num);
}

static struct pci_driver pcibr_io_driver = {
	.name       = MODULENAME,
	.id_table   = pcibr_iodrv_id_table,
	.probe      = pcibr_iodrv_probe,
	.remove     = __devexit_p(pcibr_iodrv_remove),
};

static int __init pcibr_iodrv_init(void)
{
	int result, i;

	memset(dev_info, 0, sizeof(dev_info));
	atomic_set(&dev_num, 0);
	spin_lock_init(&module_lock);

	if (!major) {
		result = alloc_chrdev_region(&devno, 0, MAX_DEV_NUM, MODULENAME);
		major = MAJOR(devno);
	} else {
		devno = MKDEV(major, 0);
		result = register_chrdev_region(devno, MAX_DEV_NUM, MODULENAME);
	}

	if (result < 0) {
		DbgPrint("Can't get major %d", major);
		return result;
}
	DbgPrint("Major : %d",major);

	for (i = 0; i < MAX_DEV_NUM; i++) {
		dev_info[i].devno = MKDEV(major, i);
	}

	/* Create device class. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
	sys_class = class_create(MODULENAME);
#else
	sys_class = class_create(THIS_MODULE, MODULENAME);
#endif
	if(IS_ERR(sys_class)) {
		DbgPrint("Failed to create a class of device.\n");
		return -EFAULT;
	}
	DbgPrint("%s class created.\n", MODULENAME);

	return pci_register_driver(&pcibr_io_driver);
}

static void __exit pcibr_iodrv_exit(void)
{
	pci_unregister_driver(&pcibr_io_driver);
	unregister_chrdev_region(MKDEV(major, 0), MAX_DEV_NUM);
	class_destroy(sys_class);
	DbgPrint("%s class destroy.\n", MODULENAME);
}

module_init(pcibr_iodrv_init);
module_exit(pcibr_iodrv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Network Interface Controllers crew <nicfae@realtek.com>");
MODULE_VERSION(DRIVER_VERSION);
MODULE_DESCRIPTION("RealTek PCI-Express Multi-IO bridge Driver");
