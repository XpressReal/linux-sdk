/*
 * memory.h
 *
 * Copyright (c) 2017 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#include <linux/slab.h>
#include <dt-bindings/soc/realtek,mem-flag.h>

/* higher bits for buffer flags */
#define RTK_FLAG_NONCACHED          (1U << 31)


#define RTK_FLAG_POOL_CONDITION            (\
	RTK_FLAG_ACPUACC | \
	RTK_FLAG_SCPUACC | \
	RTK_FLAG_HWIPACC | \
	RTK_FLAG_VE_SPEC | \
	RTK_FLAG_PROTECTED_MASK | \
	RTK_FLAG_VCPU_FWACC | \
	RTK_FLAG_HIFIACC | \
	RTK_FLAG_CMA | \
	RTK_FLAG_SKIP_ZERO)


#define RPC_RINGBUF_PHYS (0x040ff000)
#define RPC_RINGBUF_SIZE (0x00004000)

#define DUMP_FLAG(nm)			\
{					\
	.name = __stringify(nm) "[" 	\
			#nm "] " ,	\
	.num = ilog2(nm)		\
}

struct rtk_heap_table {
	char *name;
	int num;
};

static struct rtk_heap_table rtk_heap_flags[] = {
	DUMP_FLAG(RTK_FLAG_SCPUACC),
	DUMP_FLAG(RTK_FLAG_ACPUACC),
	DUMP_FLAG(RTK_FLAG_HWIPACC),
	DUMP_FLAG(RTK_FLAG_VE_SPEC),
	DUMP_FLAG(RTK_FLAG_PROTECTED_BIT0),
	DUMP_FLAG(RTK_FLAG_PROTECTED_BIT1),
	DUMP_FLAG(RTK_FLAG_VCPU_FWACC),
	DUMP_FLAG(RTK_FLAG_CMA),
	DUMP_FLAG(RTK_FLAG_PROTECTED_DYNAMIC),
	DUMP_FLAG(RTK_FLAG_PROTECTED_BIT2),
	DUMP_FLAG(RTK_FLAG_PROTECTED_BIT3),
	DUMP_FLAG(RTK_FLAG_PROTECTED_EXT_BIT0),
	DUMP_FLAG(RTK_FLAG_PROTECTED_EXT_BIT1),
	DUMP_FLAG(RTK_FLAG_PROTECTED_EXT_BIT2),
	DUMP_FLAG(RTK_FLAG_NONCACHED),
	DUMP_FLAG(RTK_FLAG_HIFIACC),
	DUMP_FLAG(RTK_FLAG_SKIP_ZERO),
};

#define MAX_WORD 60

static __always_inline char *ka_dispflag(unsigned long flags, gfp_t gfp) {
	int i, j;
	char *name, *__name, *__tmp;
	unsigned long total_bits = BITS_PER_BYTE * sizeof(flags);

	if (flags == 0) {
		pr_err("%s , input parameters fault\n", __func__);
		return NULL;
	}
	/* have to take care max bytes of 60 */
	name = kmalloc(ARRAY_SIZE(rtk_heap_flags) * MAX_WORD, gfp);
	if (name == NULL) {
		pr_err("%s , kmalloc error\n", __func__);
		return NULL;
	}

	strcpy(name, "");
	__tmp = name;

	i = __ffs(flags);
	while (i < total_bits) {
		for (j = 0; j < ARRAY_SIZE(rtk_heap_flags); j++) {

			if (i != rtk_heap_flags[j].num)
				continue;
			__name = rtk_heap_flags[j].name;
			snprintf(__tmp, MAX_WORD, "%s", __name);
			__tmp += strlen(__name);
			break;
		}
		i = find_next_bit(&flags, total_bits , i+1);
	}

	if (*name)
		pr_info(" %s = %s \n", __func__, name);

	return name;
}


#endif
