// SPDX-License-Identifier: GPL-2.0-only
/*
 * dev_tp.h - TP device of Realtek TP demux driver
 *
 * Copyright (C) 2023 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

struct rtk_tpd_ctx {
	struct rtktpfei *tpfei;
};

static int rtk_tp_open(struct inode *inode, struct file *filp)
{
	struct rtktpfei *tpfei = container_of(filp->private_data,
						      struct rtktpfei,
						      mdev);
	struct rtk_tpd_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	ctx->tpfei = tpfei;
	filp->private_data = ctx;

	pm_runtime_get_sync(tpfei->dev);

	return 0;
}

static int rtk_tp_release(struct inode *inode, struct file *filp)
{
	struct rtk_tpd_ctx *ctx = filp->private_data;
	struct rtktpfei *tpfei = ctx->tpfei;

	pm_runtime_put_sync(tpfei->dev);
	kfree(ctx);
	return 0;
}

static const struct vm_operations_struct rtk_tp_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys,
#endif
};

static int rtk_tp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct rtk_tpd_ctx *ctx = filp->private_data;
	struct rtktpfei *tpfei = ctx->tpfei;
	phys_addr_t addr;
	resource_size_t size;


	if (vma->vm_end < vma->vm_start)
		return -EINVAL;

	if (vma->vm_pgoff >= tpfei->num_tpm)
		return -E2BIG;
	addr = tpfei->tpm[vma->vm_pgoff].addr;
	size = tpfei->tpm[vma->vm_pgoff].size;

	if (addr & ~PAGE_MASK)
		return -EINVAL;

	if ((vma->vm_end - vma->vm_start) > size)
		return -EINVAL;

	vma->vm_ops = &rtk_tp_vm_ops;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return remap_pfn_range(vma,
			       vma->vm_start,
			       addr >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
}

#define RTK_TP_IOCTL_GET_MOUDLE_NUM       _IOR('C', 0x01, unsigned int)
#define RTK_TP_IOCTL_USE_DEFAULT_PINS     _IO('C', 0x05)
#define RTK_TP_IOCTL_USE_SERIAL_PINS      _IO('C', 0x06)

static long rtk_tp_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct rtk_tpd_ctx *ctx = filp->private_data;
	struct rtktpfei *tpfei = ctx->tpfei;
	unsigned int num_tpm;
	int ret = 0;

	switch (cmd) {
	case RTK_TP_IOCTL_GET_MOUDLE_NUM:
		num_tpm = tpfei->num_tpm;
		ret = copy_to_user((unsigned int __user *)arg,
				&num_tpm, sizeof(unsigned int));
		break;

#if 0
	case RTK_TP_IOCTL_USE_DEFAULT_PINS:
		if (!tpdev->pins_default)
			return -EINVAL;
		return pinctrl_select_state(tpdev->pinctrl, tpdev->pins_default);

	case RTK_TP_IOCTL_USE_SERIAL_PINS:
		if (!tpdev->pins_serial)
			return -EINVAL;
		return pinctrl_select_state(tpdev->pinctrl, tpdev->pins_serial);
#endif

	default:
		return -ENOIOCTLCMD;
	}
	return ret;
}

static const struct file_operations rtk_tp_fops = {
	.owner          = THIS_MODULE,
	.open           = rtk_tp_open,
	.release        = rtk_tp_release,
	.mmap           = rtk_tp_mmap,
	.unlocked_ioctl = rtk_tp_ioctl,
	.compat_ioctl   = compat_ptr_ioctl,
};

static int rtk_tp_dev_init(struct rtktpfei *tpfei)
{
	struct device *dev = tpfei->dev;
	int ret;

#if 0
	tpfei->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(tpdev->pinctrl))
		dev_dbg(dev, "no pinctrl\n");
	else {
		tpfei->pins_default = pinctrl_lookup_state(tpfei->pinctrl, PINCTRL_STATE_DEFAULT);
		if (IS_ERR(tpfei->pins_default))
			dev_warn(dev, "could not get default state\n");
		tpfei->pins_serial = pinctrl_lookup_state(tpfei->pinctrl, "serial");
		if (IS_ERR(tpfei->pins_serial))
			dev_warn(dev, "could not get state serial\n");
	}
#endif

	tpfei->mdev.minor  = MISC_DYNAMIC_MINOR;
	tpfei->mdev.name   = "tp";
	tpfei->mdev.fops   = &rtk_tp_fops;
	tpfei->mdev.parent = dev;
	ret = misc_register(&tpfei->mdev);
	if (ret) {
		dev_err(dev, "failed to register misc device: %d\n", ret);
		return ret;
	}

	return 0;
}

