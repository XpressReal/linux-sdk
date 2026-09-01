// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 <cy.huang@realtek.com>
 */

#ifndef __RTK_PROTECT_H
#define __RTK_PROTECT_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/vmalloc.h>

enum e_notifier_protect_type {
	NOTIFIER_PROTECT_TYPE_NONE = 0,
	NOTIFIER_PROTECT_TYPE_1,
	NOTIFIER_PROTECT_TYPE_2,
	NOTIFIER_PROTECT_TYPE_3,
	NOTIFIER_PROTECT_TYPE_4,
	NOTIFIER_PROTECT_TYPE_5,
	NOTIFIER_PROTECT_TYPE_6,
	NOTIFIER_PROTECT_TYPE_7,
	NOTIFIER_PROTECT_TYPE_8,
	NOTIFIER_PROTECT_TYPE_9,
	NOTIFIER_PROTECT_TYPE_10,
	NOTIFIER_PROTECT_TYPE_11,
	NOTIFIER_PROTECT_TYPE_12,
	NOTIFIER_PROTECT_TYPE_13,
	NOTIFIER_PROTECT_TYPE_14,
	NOTIFIER_PROTECT_TYPE_15,
	NOTIFIER_PROTECT_TYPE_MAX,
};

enum e_notifier_protect_ext {
	NOTIFIER_PROTECTED_EXT_NONE = 0,
	NOTIFIER_PROTECTED_EXT_1,
	NOTIFIER_PROTECTED_EXT_2,
	NOTIFIER_PROTECTED_EXT_3,
	NOTIFIER_PROTECTED_EXT_4,
	NOTIFIER_PROTECTED_EXT_5,
	NOTIFIER_PROTECTED_EXT_6,
	NOTIFIER_PROTECTED_EXT_7,
	NOTIFIER_PROTECTED_EXT_MAX,
};

struct protect_region {
	enum e_notifier_protect_type type;
	unsigned long base;
	size_t size;
};

struct protect_ext_region {
	enum e_notifier_protect_ext ext;
	unsigned long base;
	size_t size;
	void *priv;
};


struct rtk_protect_create_info {
	struct protect_region mem;
	void *priv_virt;
};

struct rtk_protect_change_info {
	struct protect_region mem;
	void *priv_virt;
};

struct rtk_protect_destroy_info {
	void *priv_virt;
};

struct rtk_protect_info {
	struct list_head list;
	struct rtk_protect_create_info create_info;
};

struct rtk_protect_ext_set {
	struct protect_ext_region mem;
	void *priv_virt;
};

struct rtk_protect_ext_unset {
	void *priv_virt;
};

struct rtk_protect_ext_info {
	struct list_head list;
	struct rtk_protect_ext_set create_info;
};


int rtk_protect_create(struct rtk_protect_create_info *info);

int rtk_protect_change(struct rtk_protect_change_info *info);

int rtk_protect_destroy(struct rtk_protect_destroy_info *info);

int rtk_protect_ext_set(struct rtk_protect_ext_set * config);

int rtk_protect_ext_unset(struct rtk_protect_ext_unset * config);

bool rtk_protect_handler_ready(void);

#ifdef CONFIG_ARM64
struct page_change_data {
	pgprot_t set_mask;
	pgprot_t clear_mask;
};

static inline int _rtk_change_page_range(pte_t *ptep, unsigned long addr, void *data)
{
	struct page_change_data *cdata = data;
	pte_t pte = READ_ONCE(*ptep);

	pte = clear_pte_bit(pte, cdata->clear_mask);
	pte = set_pte_bit(pte, cdata->set_mask);

	set_pte(ptep, pte);
	return 0;
}

static inline int _rtk_apply_to_pte_range(pmd_t *pmd, unsigned long addr,
				unsigned long end, void *data, pgtbl_mod_mask *mask)
{
	pte_t *pte;
	int err = 0;

	pte = pte_offset_kernel(pmd, addr);

	arch_enter_lazy_mmu_mode();

	do {
		if (!pte_none(ptep_get(pte))) {
			err = _rtk_change_page_range(pte, addr, data);
			if (err)
				break;
		} else
			BUG();
	} while (pte++, addr += PAGE_SIZE, addr != end);

	*mask |= PGTBL_PTE_MODIFIED;

	arch_leave_lazy_mmu_mode();

	return err;
}

static inline int _rtk_apply_to_pmd_range(pud_t *pud, unsigned long addr,
				unsigned long end, void *data, pgtbl_mod_mask *mask)
{
	pmd_t *pmd;
	unsigned long next;
	int err = 0;

	BUG_ON(pud_leaf(*pud));

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		if (pmd_none(*pmd))
			BUG();
		if (WARN_ON_ONCE(pmd_leaf(*pmd)))
			return -EINVAL;
		if (!pmd_none(*pmd) && WARN_ON_ONCE(pmd_bad(*pmd)))
			BUG();
		err = _rtk_apply_to_pte_range(pmd, addr, next, data, mask);
		if (err)
			break;
	} while (pmd++, addr = next, addr != end);

	return err;
}

static inline int _rtk_apply_to_pud_range(p4d_t *p4d, unsigned long addr,
				unsigned long end, void *data, pgtbl_mod_mask *mask)
{
	pud_t *pud;
	unsigned long next;
	int err = 0;

	pud = pud_offset(p4d, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_none(*pud))
			BUG();
		if (WARN_ON_ONCE(pud_leaf(*pud)))
			return -EINVAL;
		if (!pud_none(*pud) && WARN_ON_ONCE(pud_bad(*pud)))
			BUG();
		err = _rtk_apply_to_pmd_range(pud, addr, next, data, mask);
		if (err)
			break;
	} while (pud++, addr = next, addr != end);

	return err;
}

static inline int _rtk_apply_to_p4d_range(pgd_t *pgd, unsigned long addr,
				unsigned long end, void *data, pgtbl_mod_mask *mask)
{
	p4d_t *p4d;
	unsigned long next;
	int err = 0;

	p4d = p4d_offset(pgd, addr);

	do {
		next = p4d_addr_end(addr, end);
		if (p4d_none(*p4d))
			BUG();
		if (WARN_ON_ONCE(p4d_leaf(*p4d)))
			return -EINVAL;
		if (!p4d_none(*p4d) && WARN_ON_ONCE(p4d_bad(*p4d)))
			BUG();
		err = _rtk_apply_to_pud_range(p4d, addr, next, data, mask);
		if (err)
			break;
	} while (p4d++, addr = next, addr != end);

	return err;
}

static inline int _rtk_apply_to_page_range(unsigned long addr, unsigned long size, void *data)
{
	pgd_t *pgd;
	unsigned long start = addr, next;
	unsigned long end = addr + size;
	pgtbl_mod_mask mask = 0;
	int err = 0;
	unsigned long ttbr1 = read_sysreg(ttbr1_el1);

	if (WARN_ON(addr >= end))
		return -EINVAL;

	pgd = (pgd_t*)phys_to_virt(ttbr1 & 0xfffffffff000); /* clean ASID & lower ignore part */

	pgd = pgd_offset_pgd(pgd, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none(*pgd))
			BUG();
		if (WARN_ON_ONCE(pgd_leaf(*pgd)))
			return -EINVAL;
		if (!pgd_none(*pgd) && WARN_ON_ONCE(pgd_bad(*pgd)))
			BUG();
		err = _rtk_apply_to_p4d_range(pgd, addr, next, data, &mask);
		if (err)
			break;
	} while (pgd++, addr = next, addr != end);

	if (mask & ARCH_PAGE_TABLE_SYNC_MASK)
		arch_sync_kernel_mappings(start, start + size);

	return err;
}
#endif /* CONFIG_ARM64 */
#endif
