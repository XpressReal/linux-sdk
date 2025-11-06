// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek Remote Processor driver
 *
 * Copyright (c) 2017-2023 Realtek Semiconductor Corp.
 */

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/reset.h>
#include <linux/firmware.h>
#include <linux/dma-map-ops.h>
#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <soc/realtek/rtk_tee.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_ipc_shm.h>
#include <linux/mfd/syscon.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/regmap.h>
#include <linux/reboot.h>
#include "remoteproc_internal.h"

#define SMC_CMD_RTD1619B_LOAD 0x8400ff39
#define SMC_CMD_KR4_LOAD 0x82000008
#define SMC_CMD_VFW_LOAD 0x82000003
#define SMC_CMD_HIFI_LOAD 0x82000002
#define SMC_CMD_HIFI1_LOAD 0x8200000a
#define SMC_CMD_DLSR_LOAD  0x82000004
#define SMC_CMD_AVCERT_LOAD 0x82000009

#define SMC_CMD_KR4_CLK 0x82000506
#define SMC_CMD_VFW_CLK 0x82000504
#define SMC_CMD_HIFI_CLK 0x82000502
#define SMC_CMD_HIFI1_CLK 0x82000508
#define SMC_CMD_VCPU_START 0x8200050b

#define SYS_SOFT_RESET6		0x98000014
#define ACPU_STARTUP_FLAG	0x9801a360
#define ACPU_MAGIC1		0xacd1

#define VCPU_STARTUP_FLAG	0x7f0
#define VCPU_MAGIC1		0xacd1
#define SYS_SOFT_RESET1		0x98000000

#define ISO_SRAM_CTRL 0xfd8
#define SYS_SOFT_RESET6_S 0x98000014
#define VE3_A_ENTRY   0x04200000
#define HEVC_ENC_TOP_CR48 0x8c

#define ISO_POWER_CTRL		0xfd0
#define ISO_CTRL1		0x2f4
#define ISO_HIFI0_SRAM_PWR4	0x248
#define ISO_HIFI0_SRAM_PWR5	0x24C

#define WRITE_DATA BIT(0)
#define POLLING_TIME 1000

#define IPC_CMD_NONBLOCKING 0
#define IPC_CMD_BLOCKING 1
/*TO PCPU*/
#define IPC_CATE_PPC		0x2		/* Peripheral power control */
#define IPC_PPC_KR4_OFF		0x2000		/* power off acpu */
#define IPC_PPC_KR4_ON		0x2008		/* power on acpu */
#define IPC_PPC_VCPU_OFF	0x3000		/* power off vcpu */
#define IPC_PPC_VCPU_ON		0x3008		/* power on vcpu */
#define IPC_PPC_HIFI0_OFF	0x5000		/* power off hifi0 */
#define IPC_PPC_HIFI0_ON	0x5008		/* power on hifi0 */
#define IPC_PPC_HIFI1_OFF	0x6000		/* power off hifi1 */
#define IPC_PPC_HIFI1_ON	0x6008		/* power on hifi1 */

/*TO RPROC*/
#define IPC_CATE_PM 0x1
#define IPC_PM_SUSPEND 0x1
#define IPC_PM_RESUME 0x2

struct rtk_fw_rproc {
	struct rproc *rproc;
	struct device *dev;
	const struct rtk_fw_info *info;
	const struct rproc_pm_ops *pm_ops;
	bool need_ext_fw;
	struct regmap *intr_regmap;
	struct regmap *ipc_regmap;
	struct regmap *iso_regmap;
	struct regmap *crt_regmap;
	struct regmap *misc_regmap;
	struct regmap *ve3_regmap;
	struct clk *clk;
	struct reset_control *rstn;
	struct reset_control *rstn_bist;
	int verify_status;
	bool mcu;
};

struct rproc_pm_ops {
	int (*suspend)(struct rtk_fw_rproc *data);
	int (*resume)(struct rtk_fw_rproc *data);
};

struct rtk_fw_info {
	int smc_fid;
	int cert_type;
	const struct rproc_ops *ops;
	const struct pm_info *pm;
	const struct rproc_pm_ops *pm_ops;
	const struct log_info *log;
	int (*get_resource)(struct rtk_fw_rproc *data);
};

struct log_info {
	char *name;
	u32 ipc_offset;
};

enum {
	POLLING_TYPE = 0,
	INTR_TYPE,
};

enum {
	VERIFY_NONE = 0,
	VERIFY_SUCCESS = 1,
	VERIFY_FAILED = -1,
};

enum {
	TO_RPROC = 0,
	TO_PCPU = 1,
};

struct pm_info {
	u32 suspend_cmd;
	u32 resume_cmd;
	u32 poweron_cmd;
	u32 poweroff_cmd;
	u32 start_cmd;
	u32 suspend_type;
	u32 intr_offset;
	u32 intr_en_offset;
	u32 to_pcpu_intr_bit;
	u32 to_pcpu_ipc_regoff;
	u32 to_rproc_intr_bit;
	u32 to_rproc_ipc_regoff;
};

static int send_ipc_command(struct rtk_fw_rproc *rtk_rproc, u32 dst, u32 blocking,
			    u32 cate, u32 opcode, u32 arg)
{
	u32 parity;
	u32 cmd;
	u32 reg_offset, intr_offset, intr_bit;
	const struct pm_info *pm = rtk_rproc->info->pm;
	int ret = 0;
	int val, val1, val2;

	if (dst == TO_RPROC) {
		reg_offset = pm->to_rproc_ipc_regoff;
		intr_offset = pm->intr_offset;
		intr_bit = pm->to_rproc_intr_bit;
	} else if (dst == TO_PCPU) {
		reg_offset = pm->to_pcpu_ipc_regoff;
		intr_offset = pm->intr_offset;
		intr_bit = pm->to_pcpu_intr_bit;
	} else {
		dev_err(rtk_rproc->dev, "attemp to send ipc to invalid destination\n");
		return -EINVAL;
	}

	parity = cate ^ (opcode & GENMASK(7, 0)) ^ ((opcode >> 8) & GENMASK(7, 0)) ^ blocking;
	cmd = (blocking << 31) | (cate << 24) | (parity << 16) | opcode;

	regmap_write(rtk_rproc->ipc_regmap, reg_offset, cmd);
	regmap_write(rtk_rproc->ipc_regmap, reg_offset + 0x4, arg);
	regmap_write(rtk_rproc->intr_regmap, intr_offset, intr_bit | WRITE_DATA);

	if (blocking) {
		ret = regmap_read_poll_timeout(rtk_rproc->ipc_regmap, reg_offset + 0x4, val,
					       val == 0x1, POLLING_TIME, 1000 * POLLING_TIME);
		if (ret) {
			regmap_read(rtk_rproc->intr_regmap, intr_offset, &val);
			regmap_read(rtk_rproc->ipc_regmap, reg_offset, &val1);
			regmap_read(rtk_rproc->ipc_regmap, reg_offset + 0x4, &val2);
			dev_err(rtk_rproc->dev,
				"send %s ipc timeout(intr:0x%x cmd_reg:0x%x cmd_arg:0x%x)\n",
				dst ? "pcpu" : "rproc", val, val1, val2);
		}
		regmap_write(rtk_rproc->ipc_regmap, reg_offset, 0);
		regmap_write(rtk_rproc->ipc_regmap, reg_offset + 0x4, 0);
	}

	return ret;

}

static int trust_fw_load(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	int ret;
	unsigned int size;
	dma_addr_t dma;
	void *vaddr = NULL;
	struct arm_smccc_res res;
	int cert_type;

	dev_info(dev->parent, "Find FW Name: %s\n", rproc->firmware);
	dev_info(dev->parent, "size 0x%x\n", (unsigned int)fw->size);

	/* Prepare for dma alloc */
	size = PAGE_ALIGN(fw->size);

	/* alloc uncached memory */
	vaddr = dma_alloc_coherent(dev->parent, size, &dma, GFP_KERNEL);
	if (!vaddr) {
		ret = -ENOMEM;
		goto err;
	}
	memcpy(vaddr, fw->data, fw->size);

	if (rtk_rproc->info->cert_type)
		cert_type = rtk_rproc->info->cert_type;
	else
		cert_type = 0;

	arm_smccc_smc(rtk_rproc->info->smc_fid, dma, fw->size,
		      cert_type, 0, 0, 0, 0, &res);
	ret = (unsigned int)res.a0;
	if (ret) {
		dev_err(dev, "process fwtype: %d fail\n", rtk_rproc->info->smc_fid);
		rtk_rproc->verify_status = VERIFY_FAILED;
		goto err;
	}

	rtk_rproc->verify_status = VERIFY_SUCCESS;
err:

	dma_free_coherent(dev->parent, size, vaddr, dma);

	return ret;
}

static void update_boot_av_info(u32 mem, u32 mem_s)
{
	struct rtk_ipc_shm __iomem *ipc = (void __iomem *)IPC_SHM_VIRT;
	struct boot_av_info __iomem *boot_info = (void __iomem *)BOOT_AV_INFO;

	ipc->pov_boot_av_info = 0x4080600;
	boot_info->dwMagicNumber = BOOT_AV_INFO_MAGICNO_DLSR;
	boot_info->video_DLSR_binary_addr = mem;
	boot_info->video_DLSR_binary_addr_svp = mem_s;
}

static int dlsr_load(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	struct reserved_mem *rmem = NULL;
	struct device_node *node;
	struct arm_smccc_res res;
	dma_addr_t dma;
	void *vaddr = NULL;
	int ret = 0;
	unsigned int size;
	phys_addr_t mem;
	unsigned int mem_s;

	dev_info(dev->parent, "Find FW Name: %s\n", rproc->firmware);
	dev_info(dev->parent, "size 0x%x\n", (unsigned int)fw->size);

	/* Prepare for dma alloc */
	size = PAGE_ALIGN(fw->size);

	vaddr = dma_alloc_coherent(dev->parent, size, &dma, GFP_KERNEL);
	if (!vaddr) {
		ret = -ENOMEM;
		goto err;
	}
	memcpy(vaddr, fw->data, fw->size);

	node = of_parse_phandle(rtk_rproc->dev->of_node, "memory-region", 0);
	if (node)
		rmem = of_reserved_mem_lookup(node);
	of_node_put(node);

	if (!rmem) {
		dev_err(rtk_rproc->dev, "unable to resolve memory-region\n");
		return -EINVAL;
	}

	mem = rmem->base;

	arm_smccc_smc(rtk_rproc->info->smc_fid, dma, fw->size,
		      mem, 0, 0, 0, 0, &res);
	ret = (unsigned int)res.a0;
	if (ret) {
		dev_err(dev, "process fwtype: %d fail\n", rtk_rproc->info->smc_fid);
		rtk_rproc->verify_status = VERIFY_FAILED;
		goto err;
	}
	rtk_rproc->verify_status = VERIFY_SUCCESS;

	mem_s = (unsigned int)res.a1;

	update_boot_av_info(mem, mem_s);

err:

	return ret;

}

static int rtd1619b_acpu_start(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	struct arm_smccc_res res;
	int ret = 0;

	dev_info(&rproc->dev, "afw bring up\n");

	arm_smccc_smc(0x8400ffff, ACPU_STARTUP_FLAG,
		ACPU_MAGIC1, 0, 0, 0, 0, 0, &res);
	arm_smccc_smc(0x8400ffff, SYS_SOFT_RESET6,
		0x30, 0, 0, 0, 0, 0, &res);
	ret = clk_prepare_enable(rtk_rproc->clk);
	if (ret) {
		dev_err(rtk_rproc->dev, "failed to enable acpu clk, ret:%d\n", ret);
		goto err;
	}
err:
	return ret;
};

static int rtd1619b_acpu_stop(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	struct arm_smccc_res res;

	clk_disable_unprepare(rtk_rproc->clk);
	arm_smccc_smc(0x8400ffff, SYS_SOFT_RESET6,
		0x20, 0, 0, 0, 0, 0, &res);

	return 0;
};

static int rtd1619b_acpu_get_resource(struct rtk_fw_rproc *rtk_rproc)
{
	int ret = 0;

	rtk_rproc->clk = devm_clk_get(rtk_rproc->dev, "clk");
	if (IS_ERR(rtk_rproc->clk)) {
		dev_err(rtk_rproc->dev, "could not get clk\n");
		ret = PTR_ERR(rtk_rproc->clk);
		goto err;
	}

err:
	return ret;

}

static int rtd1619b_vcpu_start(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	struct rtk_ipc_shm __iomem *ipc = (void __iomem *)IPC_SHM_VIRT;
	int timeout = 50;
	int ret = 0;

	ipc->video_rpc_flag = 0xffffffff;

	dev_info(rtk_rproc->dev, "vfw bring up\n");

	regmap_write(rtk_rproc->misc_regmap, VCPU_STARTUP_FLAG, VCPU_MAGIC1);

	reset_control_deassert(rtk_rproc->rstn_bist);

	reset_control_deassert(rtk_rproc->rstn);

	ret = clk_prepare_enable(rtk_rproc->clk);
	if (ret) {
		dev_err(rtk_rproc->dev, "failed to enable ve2 clk, ret:%d\n", ret);
		goto err;
	}

	while (ipc->video_rpc_flag && timeout) {
		timeout--;
		mdelay(10);
	}
	if (!timeout) {
		dev_info(&rproc->dev, "vfw boot timeout\n");
		ipc->video_rpc_flag = 0;
		return -EINVAL;
	}

err:
	return ret;
};

static int rtd1619b_vcpu_stop(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;

	clk_disable_unprepare(rtk_rproc->clk);
	reset_control_assert(rtk_rproc->rstn);
	reset_control_assert(rtk_rproc->rstn_bist);

	return 0;
}

static int rtd1619b_vcpu_get_resource(struct rtk_fw_rproc *rtk_rproc)
{
	int ret = 0;

	rtk_rproc->crt_regmap = syscon_regmap_lookup_by_phandle(rtk_rproc->dev->of_node, "crt-syscon");
	if (IS_ERR(rtk_rproc->crt_regmap)) {
		ret = -EINVAL;
		goto err;
	}

	rtk_rproc->misc_regmap = syscon_regmap_lookup_by_phandle(rtk_rproc->dev->of_node, "misc-syscon");
	if (IS_ERR(rtk_rproc->misc_regmap)) {
		ret = -EINVAL;
		goto err;
	}

	rtk_rproc->clk = devm_clk_get(rtk_rproc->dev, "clk");
	if (IS_ERR(rtk_rproc->clk)) {
		dev_err(rtk_rproc->dev, "could not get clk\n");
		ret = PTR_ERR(rtk_rproc->clk);
		goto err;
	}

	rtk_rproc->rstn = devm_reset_control_get(rtk_rproc->dev, "reset");
	if (IS_ERR(rtk_rproc->rstn)) {
		dev_err(rtk_rproc->dev, "could not get reset\n");
		ret = PTR_ERR(rtk_rproc->rstn);
		goto err;
	}

	rtk_rproc->rstn_bist = devm_reset_control_get(rtk_rproc->dev, "bist");
	if (IS_ERR(rtk_rproc->rstn_bist)) {
		dev_err(rtk_rproc->dev, "could not get bist\n");
		ret = PTR_ERR(rtk_rproc->rstn_bist);
		goto err;
	}

err:
	return ret;

}

static int rtd1619b_ve3_fw_load(struct rproc *rproc, const struct firmware *fw)
{
	struct device *dev = &rproc->dev;
	struct device_node *node;
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	unsigned int size;
	struct reserved_mem *rmem;
	phys_addr_t paddr;
	void __iomem *vaddr;
	const struct firmware *ve3_entry_fw;
	unsigned int ve3_entry_size;
	int ret;

	dev_info(dev->parent, "Find FW Name: %s\n", rproc->firmware);
	dev_info(dev->parent, "size 0x%x\n", (unsigned int)fw->size);

	ret = request_firmware(&ve3_entry_fw, "ve3_entry.img", dev);
	if (ret < 0) {
		dev_err(dev, "request_firmware failed: %d\n", ret);
		return ret;
	}

	ve3_entry_size = PAGE_ALIGN(ve3_entry_fw->size);

	size = PAGE_ALIGN(fw->size);

	node = of_parse_phandle(rtk_rproc->dev->of_node, "memory-region", 0);
	rmem = of_reserved_mem_lookup(node);
	if (!rmem) {
		dev_err(dev->parent, "Failed to find reserved memory region for VE3FW\n");
		return -ENOMEM;
	}

	paddr = rmem->base;

	vaddr = ioremap(paddr, rmem->size);
	if (!vaddr) {
		dev_err(dev->parent, "Failed to ioremap reserved memory\n");
		return -ENOMEM;
	}

	memcpy(vaddr, ve3_entry_fw->data, ve3_entry_size);
	memcpy(vaddr + 0x1000, fw->data, size);

	release_firmware(ve3_entry_fw);
	return 0;
}

static void rstn_ve3_set(unsigned int rstval)
{
	struct arm_smccc_res res;
	unsigned int val;

	val = ((rstval ? 1 : 0) << 10) | 0x800;
	arm_smccc_smc(0x8400ffff, SYS_SOFT_RESET6_S,
		val, 0, 0, 0, 0, 0, &res);

	mdelay(1);
}

static int rtd1619b_ve3_start(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	struct arm_smccc_res res;
	int ret = 0;

	ret = regmap_update_bits(rtk_rproc->iso_regmap, ISO_SRAM_CTRL, GENMASK(23, 21), 0x3 << 21);
	if (ret) {
		dev_err(rtk_rproc->dev, "Failed to update iso hifi0 bits\n");
	}

	rstn_ve3_set(1);

	ret = clk_prepare_enable(rtk_rproc->clk);
	if (ret) {
		dev_err(rtk_rproc->dev, "1: failed to enable ve3 clk en, ret:%d\n", ret);
		goto err;
	}

	regmap_write(rtk_rproc->ve3_regmap, HEVC_ENC_TOP_CR48, VE3_A_ENTRY >> 12);

	arm_smccc_smc(0x8400ffff, 0x9804A230, 0x22110040, 0, 0, 0, 0, 0, &res);

	clk_disable_unprepare(rtk_rproc->clk);
	ret = regmap_update_bits(rtk_rproc->iso_regmap, ISO_SRAM_CTRL, GENMASK(23, 21), 0x7 << 21);
	if (ret) {
		dev_err(rtk_rproc->dev, "Failed to update iso hifi0 bits\n");
	}

	ret = clk_prepare_enable(rtk_rproc->clk);
	if (ret) {
		dev_err(rtk_rproc->dev, "2: failed to enable ve3 clk en, ret:%d\n", ret);
		goto err;
	}

	ret = of_platform_populate(rproc->dev.parent->of_node, NULL, NULL, &rproc->dev);
	if (ret) {
		dev_err(&rproc->dev, "populate child device failed\n");
	}
err:
	return ret;
};

static int rtd1619b_ve3_stop(struct rproc *rproc)
{
	return 0;
};

static int rtd1619b_ve3_get_resource(struct rtk_fw_rproc *rtk_rproc)
{
	int ret = 0;

	rtk_rproc->iso_regmap = syscon_regmap_lookup_by_phandle(rtk_rproc->dev->of_node, "iso-syscon");
	if (IS_ERR(rtk_rproc->iso_regmap)) {
		ret = -EINVAL;
		goto err;
	}

	rtk_rproc->ve3_regmap = syscon_regmap_lookup_by_phandle(rtk_rproc->dev->of_node, "ve3-syscon");
	if (IS_ERR(rtk_rproc->ve3_regmap)) {
		ret = -EINVAL;
		goto err;
	}

	rtk_rproc->clk = devm_clk_get(rtk_rproc->dev, "clk");
	if (IS_ERR(rtk_rproc->clk)) {
		dev_err(rtk_rproc->dev, "could not get clk\n");
		ret = PTR_ERR(rtk_rproc->clk);
		goto err;
	}

err:
	return ret;

}

static int rtd1619b_hifi_poweron(struct rtk_fw_rproc *rtk_rproc)
{
	int timeout = 500 * 1000;
	int usleep = 1000;
	int ret = 0;
	int val;
	bool change;

	/*To turn on aucpu power. Set bit 0 and bit1 of ISO_hifi0_sram_pwr4 register to 0*/
	ret = regmap_update_bits_check(rtk_rproc->iso_regmap, ISO_HIFI0_SRAM_PWR4, GENMASK(1, 0), 0x0, &change);
	if (ret) {
		dev_err(rtk_rproc->dev, "Failed to update bits\n");
	}
	if (!change)
		goto out;

	/* To wait for the 'done' bit (bit 2) in ISO_HIFI0_SRAM_PWR5 to be set */
	ret = regmap_read_poll_timeout(rtk_rproc->iso_regmap, ISO_HIFI0_SRAM_PWR5, val, val & BIT(2), usleep, timeout);
	if (ret) {
		dev_err(rtk_rproc->dev, "Timeout waiting for 'done' bit in ISO_HIFI0_SRAM_PWR5\n");
	}

	regmap_write(rtk_rproc->ve3_regmap, ISO_HIFI0_SRAM_PWR5, BIT(2));

	ret = regmap_update_bits(rtk_rproc->iso_regmap, ISO_POWER_CTRL, BIT(13), 0x0);
	if (ret) {
		dev_err(rtk_rproc->dev, "Failed to update iso hifi0 bits\n");
	}

	ret = regmap_update_bits(rtk_rproc->iso_regmap, ISO_CTRL1, GENMASK(21, 20), 0x2 << 20);
	if (ret) {
		dev_err(rtk_rproc->dev, "Failed to update iso hifi0 bits\n");
	}

out:
	return ret;
}

static int rtd1619b_hifi_start(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	struct arm_smccc_res res;
	int ret;

	dev_info(&rproc->dev, "hifi bring up\n");

	rtd1619b_hifi_poweron(rtk_rproc);
	ret = clk_prepare_enable(rtk_rproc->clk);
	if (ret) {
		dev_err(rtk_rproc->dev, "failed to enable hifi clk, ret:%d\n", ret);
		goto err;
	}

	arm_smccc_smc(0x8400ff36, 0, 0, 0, 0, 0, 0, 0, &res);
	ret = (unsigned int)res.a0;
	dev_info(rtk_rproc->dev, "[%s] smc ret: 0x%x\n", __func__, ret);

err:
	return ret;
};

static int rtd1619b_hifi_stop(struct rproc *rproc)
{
	return 0;
};

static int rtd1619b_hifi_get_resource(struct rtk_fw_rproc *rtk_rproc)
{
	int ret = 0;

	rtk_rproc->iso_regmap = syscon_regmap_lookup_by_phandle(rtk_rproc->dev->of_node, "iso-syscon");
	if (IS_ERR(rtk_rproc->iso_regmap)) {
		ret = -EINVAL;
		goto err;
	}

	rtk_rproc->clk = devm_clk_get(rtk_rproc->dev, "clk");
	if (IS_ERR(rtk_rproc->clk)) {
		dev_err(rtk_rproc->dev, "could not get clk\n");
		ret = PTR_ERR(rtk_rproc->clk);
		goto err;
	}
err:
	return ret;

};

static int load_extra_fw(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	const struct firmware *firmware_p;
	int ret;

	ret = request_firmware(&firmware_p, rproc->firmware, &rproc->dev);
	if (ret < 0) {
		dev_err(rtk_rproc->dev, "request_firmware failed: %d\n", ret);
		release_firmware(firmware_p);
		return ret;
	}
	ret = rproc->ops->load(rproc, firmware_p);
	if (ret < 0)
		dev_err(rtk_rproc->dev, "load firmware failed: %d\n", ret);

	release_firmware(firmware_p);

	return ret;
}

static int rtd1625_kr4_start(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	const struct pm_info *pm = rtk_rproc->info->pm;
	int ret = 0;

	dev_info(&rproc->dev, "kr4 bring up\n");

	ret = send_ipc_command(rtk_rproc, TO_PCPU, IPC_CMD_BLOCKING, IPC_CATE_PPC,
			       pm->poweron_cmd, 0);

	if (ret)
		dev_err(rtk_rproc->dev, "poweron failed\n");

	return ret;
}

static int rtd1625_kr4_attach(struct rproc *rproc)
{
	return 0;
}

static int rtd1625_kr4_stop(struct rproc *rproc)
{
	return 0;
}

static int rtd1625_vcpu_start(struct rproc *rproc)
{
	struct rtk_ipc_shm __iomem *ipc = (void __iomem *)IPC_SHM_VIRT;
	struct arm_smccc_res res;
	int timeout = 50;
	int ret;
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	const struct pm_info *pm = rtk_rproc->info->pm;

	dev_info(&rproc->dev, "vcpu bring up\n");

	ipc->video_rpc_flag = 0xffffffff;

	ret = send_ipc_command(rtk_rproc, TO_PCPU, IPC_CMD_BLOCKING, IPC_CATE_PPC,
			       pm->poweron_cmd, 0);

	if (ret) {
		dev_err(rtk_rproc->dev, "poweron failed\n");
		return ret;
	}

	arm_smccc_smc(pm->start_cmd, 0, 0, 0, 0, 0, 0, 0, &res);
	ret = (unsigned int)res.a0;
	if (ret) {
		dev_info(&rproc->dev, "smc failed, ret: 0x%x\n", ret);
		return -EINVAL;
	}

	while (ipc->video_rpc_flag && timeout--)
		mdelay(10);

	if (!timeout) {
		dev_info(&rproc->dev, "vfw boot timeout\n");
		ipc->video_rpc_flag = 0;
		return -EINVAL;
	}

	return 0;
}

static int rtd1625_vcpu_attach(struct rproc *rproc)
{
	return 0;
}

static int rtd1625_vcpu_stop(struct rproc *rproc)
{
	return 0;
}

static int rtd1625_hifi_start(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	const struct pm_info *pm = rtk_rproc->info->pm;
	int ret = 0;

	dev_info(&rproc->dev, "hifi bring up\n");

	ret = send_ipc_command(rtk_rproc, TO_PCPU, IPC_CMD_BLOCKING, IPC_CATE_PPC,
			       pm->poweron_cmd, 0);

	if (ret)
		dev_err(rtk_rproc->dev, "poweron failed\n");

	return ret;
};

static int rtd1625_hifi_attach(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	int ret = 0;

	if (rtk_rproc->need_ext_fw)
		ret = load_extra_fw(rproc);

	return ret;
};

static int rtd1625_hifi_stop(struct rproc *rproc)
{
	return 0;
}

static int rtd1625_hifi1_start(struct rproc *rproc)
{
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;
	const struct pm_info *pm = rtk_rproc->info->pm;
	int ret = 0;

	dev_info(&rproc->dev, "hifi1 bring up\n");

	ret = send_ipc_command(rtk_rproc, TO_PCPU, IPC_CMD_BLOCKING, IPC_CATE_PPC,
			       pm->poweron_cmd, 0);

	if (ret)
		dev_err(rtk_rproc->dev, "poweron failed\n");

	return ret;
};

static int rtd1625_hifi1_stop(struct rproc *rproc)
{
	return 0;
}

static int rtd1619b_rproc_suspend(struct rtk_fw_rproc *rtk_rproc)
{
	struct rproc *rproc = rtk_rproc->rproc;

	return rproc->ops->stop(rproc);
};

static int rtd1619b_rproc_resume(struct rtk_fw_rproc *rtk_rproc)
{
	struct rproc *rproc = rtk_rproc->rproc;

	return rproc->ops->start(rproc);
}

static int rtd1625_rproc_suspend_with_mcu(struct rtk_fw_rproc *rtk_rproc)
{
	const struct pm_info *pm;
	unsigned int val;
	int ret = 0;

	if (!rtk_rproc->info->pm)
		goto out;

	dev_info(rtk_rproc->dev, "mcu suspend flow\n");

	pm = rtk_rproc->info->pm;

	ret = send_ipc_command(rtk_rproc, TO_RPROC, IPC_CMD_NONBLOCKING, IPC_CATE_PM,
			       pm->suspend_cmd, 0);
	if (ret) {
		dev_err(rtk_rproc->dev, "suspend failed\n");
		goto out;
	}

	regmap_read(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset, &val);
	if (val & rtk_rproc->info->pm->to_rproc_intr_bit)
		regmap_write(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset,
			     rtk_rproc->info->pm->to_rproc_intr_bit);

out:
	return ret;
};

static int rtd1625_rproc_resume_with_mcu(struct rtk_fw_rproc *rtk_rproc)
{
	const struct pm_info *pm = rtk_rproc->info->pm;
	unsigned int val;
	int ret = 0;

	if (!(rtk_rproc->ipc_regmap || rtk_rproc->intr_regmap))
		goto out;

	dev_info(rtk_rproc->dev, "mcu resume flow\n");

	regmap_read(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset, &val);
	if (!(val & rtk_rproc->info->pm->to_rproc_intr_bit))
		regmap_write(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset,
			     rtk_rproc->info->pm->to_rproc_intr_bit | WRITE_DATA);
	if (!(val & rtk_rproc->info->pm->to_pcpu_intr_bit))
		regmap_write(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset,
			     rtk_rproc->info->pm->to_pcpu_intr_bit | WRITE_DATA);


	ret = send_ipc_command(rtk_rproc, TO_RPROC, IPC_CMD_BLOCKING, IPC_CATE_PM,
			       pm->resume_cmd, 0);

	if (ret) {
		dev_err(rtk_rproc->dev, "resume failed\n");
		goto out;
	}

out:
	return ret;
}

static int rtd1625_rproc_suspend(struct rtk_fw_rproc *rtk_rproc)
{
	const struct pm_info *pm;
	unsigned int val;
	int ret = 0;

	if (!rtk_rproc->info->pm)
		goto out;

	pm = rtk_rproc->info->pm;

	ret = send_ipc_command(rtk_rproc, TO_RPROC, IPC_CMD_BLOCKING, IPC_CATE_PM,
			       pm->suspend_cmd, 0);
	if (ret) {
		dev_err(rtk_rproc->dev, "suspend failed\n");
		goto out;
	}

	ret = send_ipc_command(rtk_rproc, TO_PCPU, IPC_CMD_BLOCKING, IPC_CATE_PPC,
			       pm->poweroff_cmd, 0);

	if (ret) {
		dev_err(rtk_rproc->dev, "poweroff failed\n");
		goto out;
	}

	regmap_read(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset, &val);
	if (val & rtk_rproc->info->pm->to_rproc_intr_bit)
		regmap_write(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset,
			     rtk_rproc->info->pm->to_rproc_intr_bit);

out:
	return ret;
};

static int rtd1625_rproc_resume(struct rtk_fw_rproc *rtk_rproc)
{
	const struct pm_info *pm = rtk_rproc->info->pm;
	unsigned int val;
	struct arm_smccc_res res;
	int ret = 0;

	if (!(rtk_rproc->ipc_regmap || rtk_rproc->intr_regmap))
		goto out;

	regmap_read(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset, &val);
	if (!(val & rtk_rproc->info->pm->to_rproc_intr_bit))
		regmap_write(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset,
			     rtk_rproc->info->pm->to_rproc_intr_bit | WRITE_DATA);
	if (!(val & rtk_rproc->info->pm->to_pcpu_intr_bit))
		regmap_write(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset,
			     rtk_rproc->info->pm->to_pcpu_intr_bit | WRITE_DATA);


	ret = send_ipc_command(rtk_rproc, TO_PCPU, IPC_CMD_BLOCKING, IPC_CATE_PPC,
			       pm->poweron_cmd, 0);

	if (ret) {
		dev_err(rtk_rproc->dev, "poweron failed\n");
		goto out;
	}

	if (pm->start_cmd) {
		arm_smccc_smc(pm->start_cmd, 0, 0, 0, 0, 0, 0, 0, &res);
		ret = (unsigned int)res.a0;
		if (ret) {
			dev_info(rtk_rproc->dev, "smc failed, ret: 0x%x\n", ret);
			ret = -EINVAL;
			goto out;
		}
	}

out:
	return ret;
}

static const struct rproc_pm_ops rtd1619b_pm_ops = {
	.suspend = rtd1619b_rproc_suspend,
	.resume = rtd1619b_rproc_resume,
};

static const struct rproc_pm_ops rtd1625_mcu_pm_ops = {
	.suspend = rtd1625_rproc_suspend_with_mcu,
	.resume = rtd1625_rproc_resume_with_mcu,
};

static const struct rproc_pm_ops rtd1625_pm_ops = {
	.suspend = rtd1625_rproc_suspend,
	.resume = rtd1625_rproc_resume,
};

static const struct log_info acpu_log_info = {
	.name = "alog",
	.ipc_offset = offsetof(struct rtk_ipc_shm, printk_buffer),
};

static const struct log_info vcpu_log_info = {
	.name = "vlog",
	.ipc_offset = offsetof(struct rtk_ipc_shm, video_printk_buffer),
};

static const struct log_info hifi_log_info = {
	.name = "hlog",
	.ipc_offset = offsetof(struct rtk_ipc_shm, hifi_printk_buffer),
};

static const struct log_info hifi1_log_info = {
	.name = "hlog1",
	.ipc_offset = offsetof(struct rtk_ipc_shm, hifi1_printk_buffer),
};

static const struct log_info kr4_log_info = {
	.name = "klog",
	.ipc_offset = offsetof(struct rtk_ipc_shm, kr4_printk_buffer),
};

static const struct rproc_ops rtd_avcert_ops = {
	.load = trust_fw_load,
};

static const struct rproc_ops rtd_dlsr_ops = {
	.load = dlsr_load,
};

static const struct rproc_ops rtd1619b_acpu_ops = {
	.load = trust_fw_load,
	.start = rtd1619b_acpu_start,
	.stop = rtd1619b_acpu_stop,
};

static const struct rproc_ops rtd1619b_vcpu_ops = {
	.load = trust_fw_load,
	.start = rtd1619b_vcpu_start,
	.stop = rtd1619b_vcpu_stop,
};

static const struct rproc_ops rtd1619b_hifi_ops = {
	.load = trust_fw_load,
	.start = rtd1619b_hifi_start,
	.stop = rtd1619b_hifi_stop,
};

static const struct rproc_ops rtd1619b_ve3_ops = {
	.load = rtd1619b_ve3_fw_load,
	.start = rtd1619b_ve3_start,
	.stop = rtd1619b_ve3_stop,
};

static const struct rproc_ops rtd1625_kr4_ops = {
	.start = rtd1625_kr4_start,
	.load = trust_fw_load,
	.attach = rtd1625_kr4_attach,
	.stop = rtd1625_kr4_stop,
};

static const struct rproc_ops rtd1625_vcpu_ops = {
	.start = rtd1625_vcpu_start,
	.load = trust_fw_load,
	.attach = rtd1625_vcpu_attach,
	.stop = rtd1625_vcpu_stop,
};

static const struct rproc_ops rtd1625_hifi_ops = {
	.start = rtd1625_hifi_start,
	.load = trust_fw_load,
	.attach = rtd1625_hifi_attach,
	.stop = rtd1625_hifi_stop,
};

static const struct rproc_ops rtd1625_hifi1_ops = {
	.start = rtd1625_hifi1_start,
	.load = trust_fw_load,
	.stop = rtd1625_hifi1_stop,
};

static const struct pm_info kr4_pm = {
	.suspend_cmd = IPC_PM_SUSPEND,
	.resume_cmd = IPC_PM_RESUME,
	.poweron_cmd = IPC_PPC_KR4_ON,
	.poweroff_cmd = IPC_PPC_KR4_OFF,
	.suspend_type = INTR_TYPE,
	.intr_offset = 0xa80,
	.intr_en_offset = 0xa84,
	.to_pcpu_intr_bit = BIT(3),
	.to_pcpu_ipc_regoff = 0x470,
	.to_rproc_intr_bit = BIT(1),
	.to_rproc_ipc_regoff = 0x420,
};

static const struct pm_info vcpu_pm = {
	.suspend_cmd = IPC_PM_SUSPEND,
	.poweron_cmd = IPC_PPC_VCPU_ON,
	.poweroff_cmd = IPC_PPC_VCPU_OFF,
	.start_cmd = SMC_CMD_VCPU_START,
	.suspend_type = POLLING_TYPE,
	.intr_offset = 0xa80,
	.intr_en_offset = 0xa84,
	.to_pcpu_intr_bit = BIT(3),
	.to_pcpu_ipc_regoff = 0x470,
	.to_rproc_intr_bit = BIT(2),
	.to_rproc_ipc_regoff = 0x440,
};

static const struct pm_info hifi_pm = {
	.suspend_cmd = IPC_PM_SUSPEND,
	.poweron_cmd = IPC_PPC_HIFI0_ON,
	.poweroff_cmd = IPC_PPC_HIFI0_OFF,
	.suspend_type = INTR_TYPE,
	.intr_offset = 0xa80,
	.intr_en_offset = 0xa84,
	.to_pcpu_intr_bit = BIT(3),
	.to_pcpu_ipc_regoff = 0x470,
	.to_rproc_intr_bit = BIT(4),
	.to_rproc_ipc_regoff = 0x4b0,
};

static const struct pm_info hifi1_pm = {
	.suspend_cmd = IPC_PM_SUSPEND,
	.poweron_cmd = IPC_PPC_HIFI1_ON,
	.poweroff_cmd = IPC_PPC_HIFI1_OFF,
	.suspend_type = INTR_TYPE,
	.intr_offset = 0xa80,
	.intr_en_offset = 0xa84,
	.to_pcpu_intr_bit = BIT(3),
	.to_pcpu_ipc_regoff = 0x470,
	.to_rproc_intr_bit = BIT(5),
	.to_rproc_ipc_regoff = 0x4f0,
};

static const struct rtk_fw_info rtd1619b_avcert_info = {
	.smc_fid = SMC_CMD_RTD1619B_LOAD,
	.cert_type = 0x15,
	.ops = &rtd_avcert_ops,
};

static const struct rtk_fw_info rtd1619b_acpu_info = {
	.smc_fid = SMC_CMD_RTD1619B_LOAD,
	.cert_type = 0xff02,
	.ops = &rtd1619b_acpu_ops,
	.pm_ops = &rtd1619b_pm_ops,
	.log = &acpu_log_info,
	.get_resource = &rtd1619b_acpu_get_resource,
};

static const struct rtk_fw_info rtd1619b_vcpu_info = {
	.smc_fid = SMC_CMD_RTD1619B_LOAD,
	.cert_type = 0xff04,
	.ops = &rtd1619b_vcpu_ops,
	.pm_ops = &rtd1619b_pm_ops,
	.log = &vcpu_log_info,
	.get_resource = &rtd1619b_vcpu_get_resource,
};

static const struct rtk_fw_info rtd1619b_hifi_info = {
	.smc_fid = SMC_CMD_RTD1619B_LOAD,
	.cert_type = 0xff0a,
	.ops = &rtd1619b_hifi_ops,
	.log = &hifi_log_info,
	.get_resource = &rtd1619b_hifi_get_resource,
};

static const struct rtk_fw_info rtd1619b_ve3_info = {
	.ops = &rtd1619b_ve3_ops,
	.get_resource = &rtd1619b_ve3_get_resource,
};
static const struct rtk_fw_info rtd_avcert_info = {
	.smc_fid = SMC_CMD_AVCERT_LOAD,
	.ops = &rtd_avcert_ops,
};

static const struct rtk_fw_info rtd_dlsr_info = {
	.smc_fid = SMC_CMD_DLSR_LOAD,
	.ops = &rtd_dlsr_ops,
};

static const struct rtk_fw_info rtd1625_kr4_info = {
	.smc_fid = SMC_CMD_KR4_LOAD,
	.ops = &rtd1625_kr4_ops,
	.pm = &kr4_pm,
	.pm_ops = &rtd1625_pm_ops,
	.log = &kr4_log_info,
};

static const struct rtk_fw_info rtd1625_vcpu_info = {
	.smc_fid = SMC_CMD_VFW_LOAD,
	.ops = &rtd1625_vcpu_ops,
	.pm = &vcpu_pm,
	.pm_ops = &rtd1625_pm_ops,
	.log = &vcpu_log_info,
};

static const struct rtk_fw_info rtd1625_hifi_info = {
	.smc_fid = SMC_CMD_HIFI_LOAD,
	.ops = &rtd1625_hifi_ops,
	.pm = &hifi_pm,
	.pm_ops = &rtd1625_pm_ops,
	.log = &hifi_log_info,
};

static const struct rtk_fw_info rtd1625_hifi1_info = {
	.smc_fid = SMC_CMD_HIFI1_LOAD,
	.ops = &rtd1625_hifi1_ops,
	.pm = &hifi1_pm,
	.pm_ops = &rtd1625_pm_ops,
	.log = &hifi1_log_info,
};

static ssize_t show_verify_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rproc *rproc = dev_get_drvdata(dev);
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;

	return sprintf(buf, "%d\n", rtk_rproc->verify_status);
}

static DEVICE_ATTR(verify_state, S_IRUGO, show_verify_state, NULL);

static int rtd_rproc_log_init(struct rtk_fw_rproc *rtk_rproc)
{
	const struct log_info *log = rtk_rproc->info->log;
	struct avcpu_syslog_struct __iomem *avlog_p = (void __iomem *)IPC_SHM_VIRT +
						      log->ipc_offset;
	struct reserved_mem *rmem;
	struct device_node *node;
	char *path;
	u32 loglevel;
	int ret;

	node = of_parse_phandle(rtk_rproc->dev->of_node, "log-memory-region", 0);
	if (!node)
		return -ENOENT;

	rmem = of_reserved_mem_lookup(node);
	if (!rmem) {
		dev_err(rtk_rproc->dev, "Failed to find reserved memory for log buffer\n");
		of_node_put(node);
		return -ENOMEM;
	}
	avlog_p->log_buf_addr = rmem->base;
	avlog_p->log_buf_len = rmem->size;
	of_node_put(node);


	path = kasprintf(GFP_KERNEL, "%s%s", "/rtk_avcpu/", log->name);
	node = of_find_node_by_path(path);
	kfree(path);
	if (node) {
		ret = of_property_read_u32(node, "lvl", &loglevel);
		if (ret) {
			dev_err(rtk_rproc->dev, "Property 'lvl' not found; default log level set to 0\n");
			avlog_p->con_start = 0;
		} else {
			avlog_p->con_start = loglevel;
		}
		of_node_put(node);
	} else {
		dev_err(rtk_rproc->dev, "Failed to find rtk_avcpu node, set loglevel = 0\n");
		avlog_p->con_start = 0;
	}

	return 0;
}

static int rtd1625_rproc_pm_init(struct rtk_fw_rproc *rtk_rproc)
{
	int ret = 0;
	int val;

	rtk_rproc->intr_regmap = syscon_regmap_lookup_by_phandle(rtk_rproc->dev->of_node, "intr-syscon");
	if (IS_ERR_OR_NULL(rtk_rproc->intr_regmap)) {
		dev_err(rtk_rproc->dev, "cannot get intr regmap\n");
		ret = -EINVAL;
		goto err;
	}

	rtk_rproc->ipc_regmap = syscon_regmap_lookup_by_phandle(rtk_rproc->dev->of_node, "ipc-syscon");
	if (IS_ERR_OR_NULL(rtk_rproc->ipc_regmap)) {
		dev_err(rtk_rproc->dev, "cannot get intr regmap\n");
		ret = -EINVAL;
		goto err;
	}
	regmap_read(rtk_rproc->ipc_regmap, rtk_rproc->info->pm->intr_en_offset, &val);
	if (!(val & rtk_rproc->info->pm->to_rproc_intr_bit))
		regmap_write(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset,
		    rtk_rproc->info->pm->to_rproc_intr_bit | WRITE_DATA);
	if (!(val & rtk_rproc->info->pm->to_pcpu_intr_bit))
		regmap_write(rtk_rproc->intr_regmap, rtk_rproc->info->pm->intr_en_offset,
		    rtk_rproc->info->pm->to_pcpu_intr_bit | WRITE_DATA);

err:
	return ret;
}

static int rtk_register_rproc(struct device *dev, struct device_node *node)
{
	struct rtk_fw_rproc *rtk_rproc;
	struct rproc *rproc;
	int ret = 0;
	const struct rtk_fw_info *info;
	const char *fw_name;

	info = kzalloc(sizeof(struct rtk_fw_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info = device_get_match_data(dev);

	ret = rproc_of_parse_firmware(dev, 0, &fw_name);
	if (ret) {
		dev_err(dev, "failed to parse firmware-name property, ret = %d\n",
			ret);
		goto out;
	}

	rproc = rproc_alloc(dev, node->name, info->ops, fw_name, sizeof(*rtk_rproc));
	if (!rproc) {
		ret = -ENOMEM;
		goto out;
	}

	rproc->auto_boot = false;
	rtk_rproc = rproc->priv;
	rtk_rproc->rproc = rproc;
	rtk_rproc->info = info;
	rtk_rproc->dev = dev;
	rtk_rproc->verify_status = VERIFY_NONE;

	rtk_rproc->intr_regmap = NULL;
	rtk_rproc->ipc_regmap = NULL;

	if (rtk_rproc->info->pm_ops)
		rtk_rproc->pm_ops = rtk_rproc->info->pm_ops;

	if (of_find_property(dev->of_node, "need-ext-fw", NULL))
		rtk_rproc->need_ext_fw = true;
	else
		rtk_rproc->need_ext_fw = false;

	if (of_find_property(dev->of_node, "is-booted", NULL))
		rproc->state = RPROC_DETACHED;

	dev_set_drvdata(dev, rproc);

	ret = device_create_file(rtk_rproc->dev, &dev_attr_verify_state);
	if (ret) {
		dev_err(rtk_rproc->dev, "failed: create sysfs entry\n");
		return ret;
	}

	if (of_find_property(dev->of_node, "only-load-fw", NULL)) {
		if (rtk_rproc->need_ext_fw) {
			ret = load_extra_fw(rproc);
			goto out;
		} else {
			ret = -EINVAL;
			goto put_rproc;
		}
	}

	if (of_find_property(dev->of_node, "mcu-suspend", NULL))
		rtk_rproc->pm_ops = &rtd1625_mcu_pm_ops;

	if (rtk_rproc->info->get_resource) {
		ret = rtk_rproc->info->get_resource(rtk_rproc);
		if (ret)
			goto put_rproc;
	}

	if (rtk_rproc->info->log)
		rtd_rproc_log_init(rtk_rproc);

	if (rtk_rproc->info->pm) {
		ret = rtd1625_rproc_pm_init(rtk_rproc);
		if (ret)
			goto put_rproc;
	}
	ret = rproc_add(rproc);
	if (ret) {
		dev_err(&rproc->dev, "rproc_add failed\n");
		goto put_rproc;
	}

	ret = rproc_boot(rproc);
	if (ret)
		rtk_rproc->verify_status = VERIFY_FAILED;

	return 0;

put_rproc:
	rproc_free(rproc);
out:
	return ret;
}

static int rtk_rproc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node;
	int ret = 0;

	node = pdev->dev.of_node;
	if (WARN_ON(!node)) {
		dev_err(dev, "%s can not found device node\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	ret = rtk_register_rproc(&pdev->dev, node);
	if (ret) {
		dev_err(&pdev->dev, "register rproc failed\n");
		goto err;
	}

err:
	return ret;
}

static int rtk_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);

	rproc_del(rproc);
	rproc_free(rproc);

	return 0;
}

static const struct of_device_id rtk_rproc_of_match[] = {
	{ .compatible = "realtek,rtd-rproc-avcert", .data = &rtd_avcert_info },
	{ .compatible = "realtek,rtd-rproc-dlsr", .data = &rtd_dlsr_info },
	{ .compatible = "realtek,rtd1625-rproc-kr4", .data = &rtd1625_kr4_info },
	{ .compatible = "realtek,rtd1625-rproc-vcpu", .data = &rtd1625_vcpu_info },
	{ .compatible = "realtek,rtd1625-rproc-hifi", .data = &rtd1625_hifi_info },
	{ .compatible = "realtek,rtd1625-rproc-hifi1", .data = &rtd1625_hifi1_info },
	{ .compatible = "realtek,rtd1619b-rproc-avcert", .data = &rtd1619b_avcert_info },
	{ .compatible = "realtek,rtd1619b-rproc-acpu", .data = &rtd1619b_acpu_info },
	{ .compatible = "realtek,rtd1619b-rproc-vcpu", .data = &rtd1619b_vcpu_info },
	{ .compatible = "realtek,rtd1619b-rproc-hifi", .data = &rtd1619b_hifi_info },
	{ .compatible = "realtek,rtd1619b-rproc-ve3", .data = &rtd1619b_ve3_info },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_rproc_of_match);

static int rtk_fw_suspend(struct device *dev)
{
	struct rproc *rproc = dev_get_drvdata(dev);
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;

	if (rtk_rproc->pm_ops)
		return rtk_rproc->pm_ops->suspend(rtk_rproc);

	return 0;
}

static int rtk_fw_resume(struct device *dev)
{
	struct rproc *rproc = dev_get_drvdata(dev);
	struct rtk_fw_rproc *rtk_rproc = rproc->priv;

	if (rtk_rproc->pm_ops)
		return rtk_rproc->pm_ops->resume(rtk_rproc);

	return 0;
}

static const struct dev_pm_ops rtk_fw_rproc_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(rtk_fw_suspend, rtk_fw_resume)
};

static struct platform_driver rtk_rproc_driver = {
	.probe = rtk_rproc_probe,
	.remove = rtk_rproc_remove,
	.driver = {
		.name = "rtk-rproc",
		.of_match_table = rtk_rproc_of_match,
		.pm = &rtk_fw_rproc_pm_ops,
	},
};
module_platform_driver(rtk_rproc_driver);

MODULE_AUTHOR("YH_HSIEH <yh_hsieh@realtek.com>");
MODULE_DESCRIPTION("Realtek Rproc Driver");
MODULE_LICENSE("GPL v2");
