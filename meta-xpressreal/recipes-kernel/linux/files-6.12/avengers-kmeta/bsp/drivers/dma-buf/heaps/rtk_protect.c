// SPDX-License-Identifier: GPL-2.0
/*
 * RTK HEAP PROTECT Interface
 *
 * Copyright (c) 2022 <cy.huang@realtek.com>
 */
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

#include <soc/realtek/rtk_tee.h>
#include "rtk_protect.h"

extern bool protect_stub_mode;
static struct tee_mem_device *memdev;

int rtk_protect_create(struct rtk_protect_create_info *info)
{
	struct tee_mem_protected_slot *slot;
	int ret;

	if (protect_stub_mode) {
		pr_debug("rtk_protect: stub mode active, skip TEE create\n");
		info->priv_virt = (void *)0xDEAD0000;  /* fake slot pointer */
		return 0;
	}

	slot = tee_mem_protected_create(memdev, info->mem.base, info->mem.size, info->mem.type);
	if (IS_ERR_OR_NULL(slot)) {
		ret = slot == NULL ? -EINVAL: PTR_ERR(slot);
		pr_err("failed to create protected region: %d\n", ret);
		return ret;
	}

	pr_debug("%s: %pK (0x%08lx ~ 0x%08lx)\n", __func__,
		slot, info->mem.base, (info->mem.base + info->mem.size));
	info->priv_virt = slot;
	return 0;
}

int rtk_protect_change(struct rtk_protect_change_info *info)
{
	struct tee_mem_protected_slot *slot = info->priv_virt;
	int ret;

	if (protect_stub_mode) {
		pr_debug("rtk_protect: stub mode active, skip TEE change\n");
		return 0;
	}

	ret = tee_mem_protected_change(memdev, slot, info->mem.base, info->mem.size, info->mem.type);
	if (ret) {
		pr_err("failed to change protected region: %pK (0x%08lx ~ 0x%08lx): %d\n",
			slot, info->mem.base, info->mem.base + info->mem.size, ret);
		return ret;
	}

	pr_debug("%s: %pK (0x%08lx ~ 0x%08lx)\n", __func__,
		slot, info->mem.base, (info->mem.base + info->mem.size));
	return 0;
}

int rtk_protect_destroy(struct rtk_protect_destroy_info *info)
{
	struct tee_mem_protected_slot *slot = info->priv_virt;
	int ret;

	if (protect_stub_mode) {
		pr_debug("rtk_protect: stub mode active, skip TEE destroy\n");
		info->priv_virt = NULL;
		return 0;
	}
	ret = tee_mem_protected_destroy(memdev, slot);
	if (ret) {
		pr_err("failed to destroy protected region: %pK: %d\n", slot, ret);
		return ret;
	}

	pr_debug("%s: %pk\n", __func__, slot);
	info->priv_virt = NULL;
	return 0;
}

int rtk_protect_ext_set(struct rtk_protect_ext_set *info)
{
	struct tee_mem_protected_ext_slot *slot;
	int ret;

	if (protect_stub_mode) {
		pr_debug("rtk_protect: stub mode active, skip ext_set\n");
		info->priv_virt = (void *)0xDEAD0001;  /* fake marker */
		return 0;
	}

	slot = tee_mem_protected_ext_create(memdev, info->mem.base, info->mem.size,
		info->mem.ext, info->mem.priv);
	if (IS_ERR_OR_NULL(slot)) {
		ret = slot == NULL ? -EINVAL: PTR_ERR(slot);
		pr_err("failed to set protected region ext: %d\n", ret);
		return ret;
	}

	pr_debug("%s: %pK (0x%08lx ~ 0x%08lx)\n", __func__,
		slot, info->mem.base, info->mem.base + info->mem.size);
	info->priv_virt = slot;
	return 0;
}

int rtk_protect_ext_unset(struct rtk_protect_ext_unset *info)
{
	struct tee_mem_protected_ext_slot *slot = info->priv_virt;
	int ret;

	if (protect_stub_mode) {
		pr_debug("rtk_protect: stub mode active, skip ext_unset\n");
		info->priv_virt = NULL;
		return 0;
	}

	ret = tee_mem_protected_ext_destroy(memdev, slot);
	if (ret) {
		pr_err("failed to unset protected region ext: %pK: %d\n", slot, ret);
		return ret;
	}

	pr_debug("%s: %pK\n", __func__, slot);
	return 0;
}

bool rtk_protect_handler_ready(void)
{
	if (protect_stub_mode) {
		pr_debug("rtk_protect: stub mode active, skip handler check\n");
		return true;
	}

	memdev = tee_mem_dev_get_simple();

	return memdev != NULL;
}
