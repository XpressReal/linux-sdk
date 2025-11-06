// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Realtek FW Debug driver
 *
 * Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/sched/clock.h>
#include <linux/uaccess.h>

#include <soc/realtek/memory.h>
#include <soc/realtek/rtk-krpc-agent.h>
#include <soc/realtek/rtk_media_heap.h>

#include "rtk_fwdbg.h"

#define FWDBG_IOC_MAGIC 'k'
#define FWDBG_IOCTRGETDBGREG_A _IOWR(FWDBG_IOC_MAGIC, 0x10, struct dbg_flag)
#define FWDBG_IOCTRGETDBGREG_V _IOWR(FWDBG_IOC_MAGIC, 0x11, struct dbg_flag)
#define FWDBG_IOCTRGETDBGPRINT_V _IOWR(FWDBG_IOC_MAGIC, 0x12, struct dbg_flag)

#define FWDBG_DBGREG_GET 0
#define FWDBG_DBGREG_SET 1

#define MODULE_NAME "rtk_fwdbg"
#define MODULE_NUM 1
#define SYSFS_NAME_MAX 8

struct device *fw_dbg_dev;

struct dbg_flag {
	uint32_t op;
	uint32_t flagValue;
	uint32_t flagAddr;
};

struct fw_debug_flag {
	unsigned int acpu;
	unsigned int reserve_acpu[127];
	unsigned int vcpu;
	unsigned int reserve_vcpu[127];
};

struct fw_debug_flag_memory {
	struct fw_debug_flag *debug_flag;
	phys_addr_t debug_phys;
	size_t debug_size;
	void *vaddr;
};

struct fw_debug_print_memory {
	void *debug_hdr;
	phys_addr_t debug_phys;
	size_t debug_size;
	int32_t fd;
	void *vaddr;
	struct dma_buf *dmabuf;
	struct device *dev;
};

static struct fw_debug_flag_memory *mDebugFlagMemory;
static struct fw_debug_print_memory *mDebugPrintMemory;

static struct fwdbg_ctl_t *fwdbg_ctl;
static struct fwdbg_rpc_info *fw_rpc_info;
static struct mutex info_lock;
static u64 last_sysfs_access_nsec;
static int last_sysfs_access_target;
static int agent_idx[FWDBG_TARGET_MAX];
static struct kobject *fwdbg_top_kobj;
static int fwdbg_rtos_sup;

static int krpc_fwdbg_cb(struct rtk_krpc_ept_info *krpc_ept_info, char *buf)
{
	uint32_t *tmp;
	struct rpc_struct *rpc = (struct rpc_struct *)buf;

	if (rpc->programID == REPLYID) {
		tmp = (uint32_t *)(buf + sizeof(struct rpc_struct));
		*(krpc_ept_info->retval) = *(tmp + 1);

		complete(&krpc_ept_info->ack);
	}

	return 0;
}

static int fwdbg_krpc_init(int target)
{
	int ret, opt;
	struct device_node *np = fw_dbg_dev->of_node;

	opt = agent_idx[target];
	fw_rpc_info->fwdbg_ept_info = of_krpc_ept_info_get(np, opt);
	if (IS_ERR(fw_rpc_info->fwdbg_ept_info)) {
		ret = PTR_ERR(fw_rpc_info->fwdbg_ept_info);
		if (ret == -EPROBE_DEFER)
			dev_err(fw_dbg_dev,
				"krpc#%d ept info not ready, retry\n", opt);
		else
			dev_err(fw_dbg_dev,
				"Failed to get krpc#%d ept info: %d\n", opt,
				ret);

		return ret;
	}

	ret = krpc_info_init(fw_rpc_info->fwdbg_ept_info, "rtk-fwdbg",
			     krpc_fwdbg_cb);
	if (ret == 0)
		fw_rpc_info->is_krpc_init = 1;

	return ret;
}

static char *prepare_rpc_data(struct rtk_krpc_ept_info *krpc_ept_info,
			      uint32_t command, uint32_t param1,
			      uint32_t param2, int *len)
{
	struct rpc_struct *rpc;
	uint32_t *tmp;
	char *buf;

	*len = sizeof(struct rpc_struct) + 3 * sizeof(uint32_t);
	buf = kmalloc(sizeof(struct rpc_struct) + 3 * sizeof(uint32_t),
		      GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	rpc = (struct rpc_struct *)buf;
	rpc->programID = KERNELID;
	rpc->versionID = KERNELID;
	rpc->procedureID = 0;
	rpc->taskID = krpc_ept_info->id;
	rpc->sysTID = krpc_ept_info->id;
	rpc->sysPID = krpc_ept_info->id;
	rpc->parameterSize = 3 * sizeof(uint32_t);
	rpc->mycontext = 0;
	tmp = (uint32_t *)(buf + sizeof(struct rpc_struct));
	*tmp = command;
	*(tmp + 1) = param1;
	*(tmp + 2) = param2;

	return buf;
}

static int fwdbg_send_rpc(struct fwdbg_rpc_info *fw_rpc_info, uint32_t command,
			  uint32_t param1, uint32_t param2, uint32_t *retval)
{
	int ret, len;
	char *buf;
	struct rtk_krpc_ept_info *krpc_ept_info;

	krpc_ept_info = fw_rpc_info->fwdbg_ept_info;

	dev_dbg(fw_dbg_dev, "vaddr:%08lx, paddr:%08x\n",
		(uintptr_t)fw_rpc_info->vaddr,
		(unsigned int)fw_rpc_info->paddr);

	mutex_lock(&krpc_ept_info->send_mutex);

	buf = prepare_rpc_data(fw_rpc_info->fwdbg_ept_info, command, param1,
			       param2, &len);
	if (!IS_ERR(buf)) {
		krpc_ept_info->retval = retval;
		ret = rtk_send_rpc(krpc_ept_info, buf, len);
		if (!wait_for_completion_timeout(&krpc_ept_info->ack,
						 RPC_TIMEOUT)) {
			dev_err(fw_rpc_info->dev, "kernel rpc timeout: %s...\n",
				krpc_ept_info->name);
			rtk_krpc_dump_ringbuf_info(krpc_ept_info);
			ret = -EINVAL;
		}
		kfree(buf);
	} else
		ret = -ENOMEM;

	mutex_unlock(&krpc_ept_info->send_mutex);

	return ret;
}

static void set_fw_dbg(int opt)
{
	bool is_big_endian;
	struct rpc_data_t *rpc;
	uint32_t command, result, rpc_ret = 0;

	rpc = fw_rpc_info->vaddr;
	memset(rpc, 0, sizeof(struct rpc_data_t));

	switch (opt) {
	case FWDBG_ACPU:
		is_big_endian = true;
		command = ENUM_KRPC_AFW_DEBUGLEVEL;
		break;
	case FWDBG_HIFI:
	case FWDBG_HIFI1:
	case FWDBG_KR4:
		is_big_endian = false;
		if (fw_rpc_info->to_rtos == true)
			command = ENUM_KRPC_RTOS_SET_LOG_LVL;
		else
			command = ENUM_KRPC_AFW_DEBUGLEVEL;
		break;
	case FWDBG_VCPU:
		is_big_endian = true;
		command = fw_rpc_info->cmd + VRPC_DBG_FUNC_BASE;
		break;
	default:
		/* Should not go to here! */
		return;
	}
	dev_dbg(fw_dbg_dev, "target:%d, cmd:%08x\n", opt, command);

	/* Send RPC and check the result */
	rpc->data =
		is_big_endian ? htonl(fw_rpc_info->data) : fw_rpc_info->data;
	fwdbg_send_rpc(fw_rpc_info, command, fw_rpc_info->paddr,
		       fw_rpc_info->paddr + offsetof(struct rpc_data_t, result),
		       &rpc_ret);
	result = is_big_endian ? ntohl(rpc->result) : rpc->result;
	if ((rpc_ret != S_OK) || (result != S_OK))
		dev_err(fw_dbg_dev, "RPC failed, rpc_ret=0x%x, result=0x%x\n",
			rpc_ret, result);
}

static void get_fw_dbg(int opt)
{
	bool is_big_endian;
	struct rpc_data_t *rpc;
	uint32_t command, result, lvl, rpc_ret = 0;

	rpc = fw_rpc_info->vaddr;
	memset(rpc, 0, sizeof(struct rpc_data_t));

	switch (opt) {
	case FWDBG_ACPU:
		is_big_endian = true;
		command = ENUM_KRPC_GET_AFW_DEBUGLEVEL;
		break;
	case FWDBG_HIFI:
	case FWDBG_HIFI1:
	case FWDBG_KR4:
		is_big_endian = false;
		if (fw_rpc_info->to_rtos == true)
			command = ENUM_KRPC_RTOS_GET_LOG_LVL;
		else
			command = ENUM_KRPC_GET_AFW_DEBUGLEVEL;
		break;
	default:
		/* Should not go to here! */
		fw_rpc_info->data = 0xDEADBEEF;
		return;
	}

	/* Send RPC and check the result */
	fwdbg_send_rpc(fw_rpc_info, command, fw_rpc_info->paddr,
		       fw_rpc_info->paddr + offsetof(struct rpc_data_t, result),
		       &rpc_ret);
	result = is_big_endian ? ntohl(rpc->result) : rpc->result;
	lvl = is_big_endian ? ntohl(rpc->data) : rpc->data;
	if ((rpc_ret != S_OK) || (result != S_OK)) {
		dev_err(fw_dbg_dev, "RPC failed, rpc_ret=0x%x, result=0x%x\n",
			rpc_ret, result);
		fw_rpc_info->data = 0xDEADBEEF;
	} else
		fw_rpc_info->data = lvl;
}

static int vcmd_sanity_check(void)
{
	int ret = 0;
	uint32_t high, low;
	uint32_t data = fw_rpc_info->data;

	switch (fw_rpc_info->cmd) {
	case ENUM_KRPC_VFW_DBG:
	case ENUM_KRPC_VFW_HDR10:
	case ENUM_KRPC_VFW_HEVC_COMP:
	case ENUM_KRPC_VFW_HEVC_LOSSY:
	case ENUM_KRPC_VFW_VP9_COMP:
	case ENUM_KRPC_VFW_LOG_CTRL:
		if (data > 1)
			ret = -EINVAL;
		break;
	case ENUM_KRPC_VFW_LOG_LVL:
		if (data > 9)
			ret = -EINVAL;
		break;
	case ENUM_KRPC_VFW_HEVC_COMP_OPT:
		if (data > 2)
			ret = -EINVAL;
		break;
	case ENUM_KRPC_VFW_DEC_DBG:
	case ENUM_KRPC_VFW_FL_DBG:
		if (data > 99)
			ret = -EINVAL;
		break;
	case ENUM_KRPC_VFW_KBL_DBG:
		high = data >> 16;
		low = data & 0xFFFF;
		if ((high > 99) || (low > 99))
			ret = -EINVAL;
		break;
	case ENUM_KRPC_VFW_VPMASK:
	case ENUM_KRPC_VFW_ENBL_PRT:
	case ENUM_KRPC_VFW_DSBL_PRT:
	case ENUM_KRPC_VFW_HELP:
	case ENUM_KRPC_VFW_SHOW_VER:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int fw_log_level_ratelimit(struct kobj_attribute *attr, int *target)
{
	u64 nsec, tmp;
	bool result;

	if (strcmp(attr->attr.name, "acpu-dbg") == 0)
		*target = FWDBG_ACPU;
	else if (strcmp(attr->attr.name, "vcpu-dbg") == 0)
		*target = FWDBG_VCPU;
	else if (strcmp(attr->attr.name, "hifi-dbg") == 0)
		*target = FWDBG_HIFI;
	else if (strcmp(attr->attr.name, "hifi1-dbg") == 0)
		*target = FWDBG_HIFI1;
	else if (strcmp(attr->attr.name, "kr4-dbg") == 0)
		*target = FWDBG_KR4;
	else
		*target = FWDBG_TARGET_MAX;

	if (last_sysfs_access_nsec == 0) {
		last_sysfs_access_nsec = local_clock();
		last_sysfs_access_target = *target;
		return 0;
	}

	nsec = local_clock();
	tmp = (nsec > last_sysfs_access_nsec) ? nsec - last_sysfs_access_nsec :
						      last_sysfs_access_nsec - nsec;

	/* Should not access the same node again within 2 seconds */
	if ((last_sysfs_access_target == *target) && (tmp < 2000000000LL))
		result = true;
	else
		result = false;

	last_sysfs_access_nsec = nsec;
	last_sysfs_access_target = *target;

	return result ? -EBUSY : 0;
}

static ssize_t fw_log_level_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	int ret, target;
	unsigned int flag_mask;
	uint32_t fw_lvl, os_lvl;

	/* Rate limiting to prevent from DoS attacks */
	if (fw_log_level_ratelimit(attr, &target)) {
		ret = -EBUSY;
		goto out;
	}

	/* Check if global structure is in use */
	if (fw_rpc_info != NULL) {
		ret = -EBUSY;
		goto out;
	}

	/* Allocate RPC buffer */
	if (mutex_trylock(&info_lock)) {
		fw_rpc_info = kzalloc(sizeof(*fw_rpc_info), GFP_KERNEL);
		if (!fw_rpc_info) {
			ret = -ENOMEM;
			mutex_unlock(&info_lock);
			goto out;
		}
		mutex_unlock(&info_lock);
	} else {
		ret = -EBUSY;
		goto out;
	}

	flag_mask = RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC | RTK_FLAG_VCPU_FWACC;
	rheap_setup_dma_pools(fw_dbg_dev, NULL, flag_mask, __func__);
	fw_rpc_info->vaddr =
		dma_alloc_coherent(fw_dbg_dev, RPC_BUFFER_SIZE,
				   &fw_rpc_info->paddr, GFP_DMA | GFP_KERNEL);
	if (!fw_rpc_info->vaddr) {
		ret = -ENOMEM;
		goto dma_fail_out;
	}

	fw_rpc_info->dev = fw_dbg_dev;

	ret = fwdbg_krpc_init(target);
	if (ret < 0) {
		ret = -EREMOTEIO;
		goto rpc_fail_out;
	}
	/* Get FW debug setting */
	get_fw_dbg(target);
	fw_lvl = fw_rpc_info->data;

	/* Get OS debug setting */
	if (fwdbg_rtos_sup) {
		fw_rpc_info->to_rtos = true;
		get_fw_dbg(target);
		os_lvl = fw_rpc_info->data;
	}

rpc_fail_out:
	if (fw_rpc_info->is_krpc_init == 1)
		krpc_info_deinit(fw_rpc_info->fwdbg_ept_info);

	dma_free_coherent(fw_dbg_dev, RPC_BUFFER_SIZE, fw_rpc_info->vaddr,
			  fw_rpc_info->paddr);
dma_fail_out:
	kfree(fw_rpc_info);
	fw_rpc_info = NULL;
out:
	if (ret)
		return ret;

	if (fwdbg_rtos_sup == 0)
		return sprintf(buf, "FW:0x%08x\n", fw_lvl);
	else
		return sprintf(buf, "FW:0x%08x, OS:%d\n", fw_lvl, os_lvl);
}

static ssize_t fw_log_level_store(struct kobject *kobj,
				  struct kobj_attribute *attr, const char *buf,
				  size_t count)
{
	int ret, target;
	unsigned int flag_mask, timeout, intvl;
	char str[3];

	/* Rate limiting to prevent from DoS attacks */
	if (fw_log_level_ratelimit(attr, &target)) {
		ret = -EBUSY;
		goto out;
	}

	/* Check if global structure is in use */
	if (fw_rpc_info != NULL) {
		ret = -EBUSY;
		goto out;
	}

	/* Allocate structure */
	if (mutex_trylock(&info_lock)) {
		fw_rpc_info = kzalloc(sizeof(*fw_rpc_info), GFP_KERNEL);
		if (!fw_rpc_info) {
			ret = -ENOMEM;
			mutex_unlock(&info_lock);
			goto out;
		}
		mutex_unlock(&info_lock);
	} else {
		ret = -EBUSY;
		goto out;
	}

	/* Process input */
	fw_rpc_info->to_rtos = false;
	if (target == FWDBG_VCPU) {
		if (sscanf(buf, "%d %2d %2d", &fw_rpc_info->cmd, &timeout, &intvl) ==
		    3) {
			if (fw_rpc_info->cmd == ENUM_KRPC_VFW_KBL_DBG) {
				fw_rpc_info->data = (intvl << 16) | timeout;
				goto rpc_begin;
			}
		}
		if (sscanf(buf, "%d %2d", &fw_rpc_info->cmd,
			   &fw_rpc_info->data) == 2) {
			if ((fw_rpc_info->cmd == ENUM_KRPC_VFW_DEC_DBG) ||
			    (fw_rpc_info->cmd == ENUM_KRPC_VFW_FL_DBG))
				goto rpc_begin;
		}
		if (sscanf(buf, "%d %x", &fw_rpc_info->cmd,
			   &fw_rpc_info->data) != 2) {
			/* CMDs without argument */
			if (kstrtou32(buf, 0, &fw_rpc_info->cmd)) {
				ret = -EINVAL;
				goto dma_fail_out;
			}
			switch (fw_rpc_info->cmd) {
			case ENUM_KRPC_VFW_ENBL_PRT:
			case ENUM_KRPC_VFW_DSBL_PRT:
			case ENUM_KRPC_VFW_HELP:
			case ENUM_KRPC_VFW_SHOW_VER:
				/* Unused data */
				fw_rpc_info->data = 0;
				break;
			default:
				ret = -EINVAL;
				goto dma_fail_out;
			}
		}
	} else { /* Audio firmware */
		if (kstrtou32(buf, 0, &fw_rpc_info->data)) {
			if (fwdbg_rtos_sup == 0) {
				ret = -EINVAL;
				goto dma_fail_out;
			}
			/* RTOS debug level switching */
			if (sscanf(buf, "%2s %d", str, &fw_rpc_info->data) !=
			    2) {
				ret = -EINVAL;
				goto dma_fail_out;
			}
			if (strncmp(str, "os", 3) != 0) {
				ret = -EINVAL;
				goto dma_fail_out;
			} else {
				/* Check if data is valid */
				if (fw_rpc_info->data > 4) {
					ret = -EINVAL;
					goto dma_fail_out;
				}
				fw_rpc_info->to_rtos = true;
			}
		}
	}

rpc_begin:
	if (target == FWDBG_VCPU) {
		if (vcmd_sanity_check()) {
			ret = -EINVAL;
			goto dma_fail_out;
		}
	}
	/* Allocate RPC buffer */
	flag_mask = RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC | RTK_FLAG_VCPU_FWACC;
	rheap_setup_dma_pools(fw_dbg_dev, NULL, flag_mask, __func__);
	fw_rpc_info->vaddr =
		dma_alloc_coherent(fw_dbg_dev, RPC_BUFFER_SIZE,
				   &fw_rpc_info->paddr, GFP_DMA | GFP_KERNEL);
	if (!fw_rpc_info->vaddr) {
		ret = -ENOMEM;
		goto dma_fail_out;
	}

	fw_rpc_info->dev = fw_dbg_dev;

	ret = fwdbg_krpc_init(target);
	if (ret < 0) {
		ret = -EREMOTEIO;
		goto rpc_fail_out;
	}

	/* Send FW debug setting */
	set_fw_dbg(target);

rpc_fail_out:
	if (fw_rpc_info->is_krpc_init == 1)
		krpc_info_deinit(fw_rpc_info->fwdbg_ept_info);

	dma_free_coherent(fw_dbg_dev, RPC_BUFFER_SIZE, fw_rpc_info->vaddr,
			  fw_rpc_info->paddr);
dma_fail_out:
	kfree(fw_rpc_info);
	fw_rpc_info = NULL;
out:
	if (ret)
		return ret;
	else
		return count;
}

static struct kobj_attribute adbg_level_attr =
	__ATTR(acpu-dbg, 0660, fw_log_level_show, fw_log_level_store);
static struct kobj_attribute vdbg_level_attr =
	__ATTR(vcpu-dbg, 0660, NULL, fw_log_level_store);
static struct kobj_attribute hdbg_level_attr =
	__ATTR(hifi-dbg, 0660, fw_log_level_show, fw_log_level_store);
static struct kobj_attribute hdbg1_level_attr =
	__ATTR(hifi1-dbg, 0660, fw_log_level_show, fw_log_level_store);
static struct kobj_attribute kdbg_level_attr =
	__ATTR(kr4-dbg, 0660, fw_log_level_show, fw_log_level_store);

static struct attribute *fwdbg_attrs[] = {
	&adbg_level_attr.attr,	&vdbg_level_attr.attr, &hdbg_level_attr.attr,
	&hdbg1_level_attr.attr, &kdbg_level_attr.attr, NULL,
};

static struct attribute_group fwdbg_attr_group = {
	.attrs = fwdbg_attrs,
};

static int fw_debug_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct fw_debug_print_memory *dbm = dmabuf->priv;
	struct device *dev = dbm->dev;
	size_t size = vma->vm_end - vma->vm_start;

	return dma_mmap_coherent(dev, vma, dbm->vaddr, dbm->debug_phys, size);
}
static void fw_debug_release(struct dma_buf *dmabuf)
{
	struct fw_debug_print_memory *dbm = dmabuf->priv;
	struct device *dev = dbm->dev;

	dma_free_coherent(dev, dbm->debug_size, dbm->vaddr, dbm->debug_phys);
}

static const struct dma_buf_ops fw_debug_dma_buf_ops = {
	.mmap = fw_debug_mmap,
	.release = fw_debug_release,
};

static struct fw_debug_flag_memory *get_debug_flag_memory(struct device *dev)
{
	struct fw_debug_flag_memory *tmp;
	unsigned int flag_mask = 0;
	dma_addr_t daddr;
	void *vaddr;

	if (mDebugFlagMemory)
		return mDebugFlagMemory;

	tmp = kzalloc(sizeof(struct fw_debug_flag_memory), GFP_KERNEL);
	if (tmp == NULL)
		goto alloc_err;

	flag_mask |= RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC | RTK_FLAG_VCPU_FWACC;
	mutex_lock(&dev->mutex);
	rheap_setup_dma_pools(dev, NULL, flag_mask, __func__);
	vaddr = dma_alloc_coherent(dev, sizeof(struct fw_debug_flag), &daddr,
				   GFP_KERNEL);
	mutex_unlock(&dev->mutex);
	if (!vaddr)
		goto rheap_err;

	tmp->debug_flag = vaddr;
	tmp->debug_phys = daddr;
	mDebugFlagMemory = tmp;

	return mDebugFlagMemory;

rheap_err:
	kfree(tmp);
alloc_err:
	return NULL;
}

static struct fw_debug_print_memory *get_debug_print_memory(struct device *dev)
{
	struct fw_debug_print_memory *tmp;
	unsigned int flag_mask = 0;
	dma_addr_t daddr;
	void *cookie;
	size_t size = sizeof(struct fw_debug_flag);
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	if (mDebugPrintMemory)
		return mDebugPrintMemory;

	tmp = kzalloc(sizeof(struct fw_debug_print_memory), GFP_KERNEL);

	if (tmp == NULL)
		goto alloc_err;

	flag_mask |= RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC | RTK_FLAG_VCPU_FWACC;

	mutex_lock(&dev->mutex);

	rheap_setup_dma_pools(dev, NULL, flag_mask, __func__);
	cookie = dma_alloc_attrs(dev, size, &daddr, GFP_KERNEL,
				 DMA_ATTR_NO_KERNEL_MAPPING);
	mutex_unlock(&dev->mutex);

	if (!cookie)
		goto rheap_err;

	exp_info.ops = &fw_debug_dma_buf_ops;
	exp_info.size = size;
	exp_info.flags = O_RDWR;
	exp_info.priv = tmp;
	tmp->dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(tmp->dmabuf))
		goto dmabuf_err;
	tmp->fd = dma_buf_fd(tmp->dmabuf, O_CLOEXEC);
	tmp->debug_phys = daddr;
	tmp->dev = dev;
	mDebugPrintMemory = tmp;

	return mDebugPrintMemory;

dmabuf_err:
	dma_free_attrs(dev, size, cookie, daddr, DMA_ATTR_NO_KERNEL_MAPPING);
rheap_err:
	kfree(tmp);
alloc_err:
	return NULL;
}

static struct fw_debug_flag *get_debug_flag(struct device *dev)
{
	struct fw_debug_flag_memory *debug_memory = get_debug_flag_memory(dev);

	return (debug_memory) ? debug_memory->debug_flag : NULL;
}

static phys_addr_t get_debug_flag_phyAddr(struct device *dev)
{
	struct fw_debug_flag_memory *debug_memory = get_debug_flag_memory(dev);

	return (debug_memory) ? debug_memory->debug_phys : -1UL;
}

static struct fw_debug_print_memory *get_debug_print(struct device *dev)
{
	struct fw_debug_print_memory *debug_print = get_debug_print_memory(dev);

	return (debug_print) ? debug_print : NULL;
}

static phys_addr_t get_debug_print_phyAddr(struct device *dev)
{
	struct fw_debug_print_memory *debug_print = get_debug_print_memory(dev);

	return (debug_print) ? debug_print->debug_phys : -1UL;
}

long rtk_fwdbg_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct dbg_flag dFlag;
	unsigned int *puDebugFlag = NULL;
	struct fw_debug_flag *debug_flag;
	phys_addr_t debug_flag_phyAddr;
	struct fw_debug_print_memory *debug_print;
	phys_addr_t debug_print_phyAddr;
	struct device *dev = filp->private_data;

	switch (cmd) {
	case FWDBG_IOCTRGETDBGREG_A:
	case FWDBG_IOCTRGETDBGREG_V:
		if (copy_from_user(&dFlag, (void __user *)arg, sizeof(dFlag)))
			return -EFAULT;
		debug_flag = get_debug_flag(dev);
		debug_flag_phyAddr = get_debug_flag_phyAddr(dev);

		if (debug_flag == NULL || -1 == debug_flag_phyAddr)
			return -EFAULT;

		if (cmd == FWDBG_IOCTRGETDBGREG_V) {
			puDebugFlag = &debug_flag->vcpu;
			debug_flag_phyAddr =
				debug_flag_phyAddr +
				offsetof(struct fw_debug_flag, vcpu);
		} else {
			puDebugFlag = &debug_flag->acpu;
			debug_flag_phyAddr =
				debug_flag_phyAddr +
				offsetof(struct fw_debug_flag, acpu);
		}

		if (dFlag.op == FWDBG_DBGREG_SET) {
			*puDebugFlag = dFlag.flagValue;
		} else {
			dFlag.flagValue = (unsigned int)*puDebugFlag;
			dFlag.flagAddr = (uint32_t)debug_flag_phyAddr & -1U;
			if (copy_to_user((void __user *)arg, &dFlag,
					 sizeof(dFlag)))
				return -EFAULT;
		}

		pr_debug("FWDBG cmd=%s op=%s phyAddr=0x%08llx flag=0x%08x",
			 (cmd == FWDBG_IOCTRGETDBGREG_V) ?
				       "FWDBG_IOCTRGETDBGREG_V" :
				       "FWDBG_IOCTRGETDBGREG_A",
			 (dFlag.op == FWDBG_DBGREG_SET) ? "SET" : "GET",
			 debug_flag_phyAddr, *puDebugFlag);
		break;
	case FWDBG_IOCTRGETDBGPRINT_V:
		if (copy_from_user(&dFlag, (void __user *)arg, sizeof(dFlag)))
			return -EFAULT;

		debug_print = get_debug_print(dev);
		debug_print_phyAddr = get_debug_print_phyAddr(dev);

		if (debug_print == NULL || -1 == debug_print_phyAddr)
			return -EFAULT;

		dFlag.flagAddr = (uint32_t)debug_print_phyAddr & -1U;
		dFlag.flagValue = debug_print->fd;

		if (copy_to_user((void __user *)arg, &dFlag, sizeof(dFlag)))
			return -EFAULT;
		break;
	default:
		pr_warn("[FWDBG]: error ioctl command...\n");
		break;
	}

	return 0;
}

int rtk_fwdbg_open(struct inode *inode, struct file *filp)
{
	filp->private_data = fw_dbg_dev;

	return 0;
}

static const struct file_operations rtk_fwdbg_fops = {
	.owner = THIS_MODULE,
	.open = rtk_fwdbg_open,
	.unlocked_ioctl = rtk_fwdbg_ioctl,
	.compat_ioctl = rtk_fwdbg_ioctl,
};

static char *fwdbg_devnode(const struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0660;
	return NULL;
}

static bool rtk_fwdbg_krpc_agent_dtb_parse(struct platform_device *pdev)
{
	int num, i;
	struct device_node *np, *agent_np;
	bool detected = false;

	np = pdev->dev.of_node;
	/* Array initializing */
	for (i = 0; i < FWDBG_TARGET_MAX; i++)
		agent_idx[i] = 255;

	num = of_count_phandle_with_args(np, "realtek,krpc-agent", NULL);
	dev_dbg(&pdev->dev, "Found %d KRPC agent(s)\n", num);
	for (i = 0; i < num; i++) {
		agent_np = of_parse_phandle(np, "realtek,krpc-agent", i);
		dev_dbg(&pdev->dev, "KRPC agent#%d:%s\n", i + 1,
			agent_np->name);
		if (!strncmp("acpu-", agent_np->name, sizeof("acpu-") - 1)) {
			agent_idx[FWDBG_ACPU] = i;
			detected = true;
		} else if (!strncmp("vcpu-", agent_np->name,
				    sizeof("vcpu-") - 1)) {
			agent_idx[FWDBG_VCPU] = i;
			detected = true;
		} else if (!strncmp("hifi-", agent_np->name,
				    sizeof("hifi-") - 1)) {
			agent_idx[FWDBG_HIFI] = i;
			detected = true;
		} else if (!strncmp("hifi1-", agent_np->name,
				    sizeof("hifi1-") - 1)) {
			agent_idx[FWDBG_HIFI1] = i;
			detected = true;
		} else if (!strncmp("kr4-", agent_np->name,
				    sizeof("kr4-") - 1)) {
			agent_idx[FWDBG_KR4] = i;
			detected = true;
		} else
			dev_info(&pdev->dev, "Unknown KRPC agent#%d:%s\n",
				 i + 1, agent_np->name);
	}

	/* Support RTOS debug according to DTS property */
	of_property_read_u32((&pdev->dev)->of_node, "rtos-dbg",
			     &fwdbg_rtos_sup);

	return detected;
}

static void rtk_fwdbg_sysfs_create(struct platform_device *pdev)
{
	int i, ret;

	/* Create /sys/kernel/fwdbg/ top level directory */
	fwdbg_top_kobj = kobject_create_and_add("fwdbg", kernel_kobj);
	if (!fwdbg_top_kobj) {
		dev_err(&pdev->dev, "Failed to create SYSFS top\n");
		return;
	}

	/* Create all files of group */
	ret = sysfs_create_group(fwdbg_top_kobj, &fwdbg_attr_group);
	if (ret)
		dev_err(&pdev->dev, "Failed to create SYSFS group\n");

	/* Remove inactive files */
	for (i = 0; i < FWDBG_TARGET_MAX; i++) {
		if (agent_idx[i] == 255) {
			switch (i) {
			case FWDBG_ACPU:
				sysfs_remove_link(fwdbg_top_kobj, "acpu-dbg");
				break;
			case FWDBG_VCPU:
				sysfs_remove_link(fwdbg_top_kobj, "vcpu-dbg");
				break;
			case FWDBG_HIFI:
				sysfs_remove_link(fwdbg_top_kobj, "hifi-dbg");
				break;
			case FWDBG_HIFI1:
				sysfs_remove_link(fwdbg_top_kobj, "hifi1-dbg");
				break;
			case FWDBG_KR4:
				sysfs_remove_link(fwdbg_top_kobj, "kr4-dbg");
				break;
			}
		}
	}
}

static void rtk_fwdbg_sysfs_destroy(void)
{
	sysfs_remove_group(fwdbg_top_kobj, &fwdbg_attr_group);
	kobject_del(fwdbg_top_kobj);
	kobject_put(fwdbg_top_kobj);
}

static int rtk_fwdbg_probe(struct platform_device *pdev)
{
	struct class *fwdbg_class = NULL;
	int ret = 0;

	fwdbg_ctl = kzalloc(sizeof(struct fwdbg_ctl_t), GFP_KERNEL);
	if (!fwdbg_ctl)
		return -ENOMEM;
	dev_set_drvdata(&pdev->dev, fwdbg_ctl);

	ret = alloc_chrdev_region(&fwdbg_ctl->devno, 0, MODULE_NUM,
				  MODULE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "alloc_chrdev_region failed");
		goto delete_ctl;
	}

	cdev_init(&fwdbg_ctl->cdev, &rtk_fwdbg_fops);
	ret = cdev_add(&fwdbg_ctl->cdev, fwdbg_ctl->devno, MODULE_NUM);
	if (ret) {
		dev_err(&pdev->dev, "cdev_add with ret %d\n", ret);
		goto unregister_devno;
	}

	fwdbg_class = class_create(MODULE_NAME);
	if (IS_ERR(fwdbg_class)) {
		ret = PTR_ERR(fwdbg_class);
		goto unregister_cdev;
	}

	fwdbg_class->devnode = fwdbg_devnode;
	fwdbg_ctl->cdev.owner = THIS_MODULE;
	fwdbg_ctl->class = fwdbg_class;

	fwdbg_ctl->dev = device_create(fwdbg_class, NULL, fwdbg_ctl->devno,
				       NULL, MODULE_NAME);
	if (IS_ERR(fwdbg_ctl->dev)) {
		ret = PTR_ERR(fwdbg_ctl->dev);
		goto delete_class;
	}

	mDebugFlagMemory = NULL;
	mDebugPrintMemory = NULL;

	fw_dbg_dev = &pdev->dev;

	fw_dbg_dev->coherent_dma_mask = DMA_BIT_MASK(32);
	fw_dbg_dev->dma_mask = (u64 *)&fw_dbg_dev->coherent_dma_mask;
	set_dma_ops(fw_dbg_dev, &rheap_dma_ops);

	/* Parse KRPC agents in device tree & create SYSFS interface */
	if (rtk_fwdbg_krpc_agent_dtb_parse(pdev) == true) {
		mutex_init(&info_lock);
		rtk_fwdbg_sysfs_create(pdev);
	}

	dev_info(&pdev->dev, "probe\n");

	return 0;

delete_class:
	class_destroy(fwdbg_class);

unregister_cdev:
	cdev_del(&fwdbg_ctl->cdev);

unregister_devno:
	unregister_chrdev_region(fwdbg_ctl->devno, MODULE_NUM);

delete_ctl:
	kfree(fwdbg_ctl);
	fwdbg_ctl = NULL;
	dev_set_drvdata(&pdev->dev, NULL);

	return ret;
}

static int rtk_fwdbg_remove(struct platform_device *pdev)
{
	if (!fwdbg_ctl)
		return -ENODEV;

	if (fwdbg_top_kobj) {
		rtk_fwdbg_sysfs_destroy();
		fwdbg_top_kobj = NULL;
	}
	device_destroy(fwdbg_ctl->class, fwdbg_ctl->devno);
	class_destroy(fwdbg_ctl->class);
	cdev_del(&fwdbg_ctl->cdev);
	unregister_chrdev_region(fwdbg_ctl->devno, MODULE_NUM);
	kfree(fwdbg_ctl);
	fwdbg_ctl = NULL;

	dev_info(&pdev->dev, "remove\n");
	return 0;
}

static const struct of_device_id rtk_fwdbg_of_match[] = {
	{ .compatible = "realtek, rtk-fwdbg" },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_fwdbg_of_match);

static struct platform_driver rtk_fwdbg_driver = {
	.probe = rtk_fwdbg_probe,
	.remove = rtk_fwdbg_remove,
	.driver = {
		.name = "rtk-fwdbg",
		.owner = THIS_MODULE,
		.of_match_table = rtk_fwdbg_of_match,
	},
};

static int __init rtk_fwdbg_init(void)
{
	return platform_driver_register(&rtk_fwdbg_driver);
}
module_init(rtk_fwdbg_init);

static void __exit rtk_fwdbg_exit(void)
{
	platform_driver_unregister(&rtk_fwdbg_driver);
}
module_exit(rtk_fwdbg_exit);

MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
