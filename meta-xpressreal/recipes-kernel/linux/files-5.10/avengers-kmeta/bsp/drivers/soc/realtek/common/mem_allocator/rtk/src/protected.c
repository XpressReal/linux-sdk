#include <soc/realtek/rtk_tee.h>
#include "protected.h"
#include "linux/slab.h"

unsigned long get_protected_base(const struct RTK_PROTECTED_INFO *info)
{
	return info->mem.base;
}

unsigned long get_protected_limit(const struct RTK_PROTECTED_INFO *info)
{
	return info->mem.base + info->mem.size;
}

size_t get_protected_size(const struct RTK_PROTECTED_INFO * info)
{
	return info->mem.size;
}

void *get_protected_priv(const struct RTK_PROTECTED_INFO *info)
{
	return info->priv_virt;
}

void set_protected_range(struct RTK_PROTECTED_INFO *info, unsigned long base,
			 unsigned long limit)
{
	info->mem.base = base;
	info->mem.size = limit - base;
}

void set_protected_priv(struct RTK_PROTECTED_INFO *info, void *priv_virt)
{
	info->priv_virt = priv_virt;
}

void init_protected_info(struct RTK_PROTECTED_INFO *info)
{
	memset((void *)info, 0, sizeof(struct RTK_PROTECTED_INFO));
	INIT_LIST_HEAD(&info->list);
}

enum E_ION_NOTIFIER_PROTECTED_TYPE get_protected_type(const struct
						      RTK_PROTECTED_INFO *info)
{
	return info->mem.type;
}

void set_protected_type(struct RTK_PROTECTED_INFO *info,
			enum E_ION_NOTIFIER_PROTECTED_TYPE type)
{
	info->mem.type = type;
}

void init_protected_ext_info(struct RTK_PROTECTED_EXT_INFO *info)
{
	memset((void *)info, 0, sizeof(struct RTK_PROTECTED_EXT_INFO));
}

void set_protected_ext_priv(struct RTK_PROTECTED_EXT_INFO *info, void *priv_virt)
{
	info->priv_virt = priv_virt;
}

void *get_protected_ext_priv(struct RTK_PROTECTED_EXT_INFO *info)
{
	return info->priv_virt;
}

void set_protected_ext_info(struct RTK_PROTECTED_EXT_INFO *info, enum E_ION_NOTIFIER_PROTECTED_EXT ext,
		unsigned long base, unsigned long size, void * parent_priv)
{
	info->mem.ext   = ext;
	info->mem.base  = base;
	info->mem.size  = size;
	info->mem.parent_priv = parent_priv;
}

struct RTK_PROTECTED_EXT_INFO * create_protected_ext_info(enum E_ION_NOTIFIER_PROTECTED_EXT ext,
		unsigned long base, unsigned long size, struct RTK_PROTECTED_INFO * parent)
{
	struct RTK_PROTECTED_EXT_INFO * ret = NULL;
	do {
		struct ion_rtk_protected_ext_set config;
		struct RTK_PROTECTED_EXT_INFO * ext_info = NULL;
		ext_info = (struct RTK_PROTECTED_EXT_INFO *) kzalloc(sizeof(struct RTK_PROTECTED_EXT_INFO), GFP_KERNEL);
		if (!ext_info)
			break;
		init_protected_ext_info(ext_info);
		set_protected_ext_info(ext_info, ext, base, size, get_protected_priv(parent));
		memcpy(&config.mem, &ext_info->mem, sizeof(ext_info->mem));
		if (ion_rtk_protected_ext_set_notify(&config) != NOTIFY_OK) {
			kfree(ext_info);
			break;
		}
		set_protected_ext_priv(ext_info, config.priv_virt);
		ret = ext_info;
	} while (0);
	return ret;
}

int destroy_protected_ext_info(struct RTK_PROTECTED_EXT_INFO * info)
{
	int ret = -1;
	do {
		struct ion_rtk_protected_ext_unset config;

		if (!info)
			break;

		config.priv_virt = get_protected_ext_priv(info);
		if (ion_rtk_protected_ext_unset_notify(&config) != NOTIFY_OK) {
			kfree(info);
			break;
		}

		kfree(info);
		ret = 0;
	} while(0);
	return ret;
}

static struct tee_mem_device *memdev;

int ion_rtk_protected_create_notify(struct ion_rtk_protected_create_info *info)
{
	struct tee_mem_protected_slot *slot;
	int ret;

	slot = tee_mem_protected_create(memdev, info->mem.base, info->mem.size, info->mem.type);
	if (IS_ERR_OR_NULL(slot)) {
		ret = slot == NULL ? -EINVAL: PTR_ERR(slot);
		pr_err("failed to create protected region: %d\n", ret);
		return ret;
	}

	pr_debug("%s: %pK (0x%08lx ~ 0x%08lx\n", __func__,
		slot, info->mem.base, (info->mem.base + info->mem.size));
	info->priv_virt = slot;
	return 0;
}

int ion_rtk_protected_change_notify(struct ion_rtk_protected_change_info *info)
{
	struct tee_mem_protected_slot *slot = info->priv_virt;
	int ret;

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

int ion_rtk_protected_destroy_notify(struct ion_rtk_protected_destroy_info *info)
{
	struct tee_mem_protected_slot *slot = info->priv_virt;
	int ret;

	ret = tee_mem_protected_destroy(memdev, slot);
	if (ret) {
		pr_err("failed to destroy protected region: %pK: %d\n", slot, ret);
		return ret;
	}

	pr_debug("%s: %pk\n", __func__, slot);
	info->priv_virt = NULL;
	return 0;
}

int ion_rtk_protected_ext_set_notify(struct ion_rtk_protected_ext_set *info)
{
	struct tee_mem_protected_ext_slot *slot;
	int ret;

	slot = tee_mem_protected_ext_create(memdev, info->mem.base, info->mem.size,
		info->mem.ext, info->mem.parent_priv);
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

int ion_rtk_protected_ext_unset_notify(struct ion_rtk_protected_ext_unset *info)
{
	struct tee_mem_protected_ext_slot *slot = info->priv_virt;
	int ret;

	ret = tee_mem_protected_ext_destroy(memdev, slot);
	if (ret) {
		pr_err("failed to unset protected region ext: %pK: %d\n", slot, ret);
		return ret;
	}

	pr_debug("%s: %pK\n", __func__, slot);
	return 0;
}

int ion_rtk_protected_handler_num(void)
{
	memdev = tee_mem_dev_get_simple();

	return memdev ? 1 : 0;
}
