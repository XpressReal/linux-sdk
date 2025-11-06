/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2019 by YH_HSIEH <yh_hsieh@realtek.com>
 *
 * Time initialization.
 */
#ifndef __RTK_CPU_H__
#define __RTK_CPU_H__

#include <linux/bitops.h>

#if defined(CONFIG_TARGET_RTD1319)
#define REG_CHIP_REV			0x9801A3F4
#else
#define REG_CHIP_REV			0x9801a204
#endif

#define RTD1xxx_CHIP_REVISION_A00 0x00000000
#define RTD1xxx_CHIP_REVISION_B00 0x00010000
#define RTD1xxx_CHIP_REVISION_B01 0x00020000
#define RTD1xxx_CHIP_REVISION_B02 0x00030000

#define AARCH_REGISTER 0x9800707C

#ifndef __ASSEMBLY__

#include <asm/io.h>
#include <linux/arm-smccc.h>

static inline u32 get_rtd1xxx_cpu_revision(void) {
	u32 val = __raw_readl(REG_CHIP_REV);
	return val;
}

static inline const u32 get_rtd1xxx_gpio_select(void) {
	const u32 val = (__raw_readl(AARCH_REGISTER) >> 16) & GENMASK(2, 0);
	return val;
}

static inline const bool rtk_is_secure_boot(void) {
	struct arm_smccc_res res = {0};
	bool ret = 0;
#if defined(CONFIG_TARGET_RTD1625)
	char mem __aligned(64) = 0;
	flush_cache((uintptr_t)&mem, 1);
	arm_smccc_smc(0x82000401, (uintptr_t)&mem, 0, 0, 0, 0, 0, 0, &res);
	invalidate_dcache_range((uintptr_t)&mem, (uintptr_t)&mem+1);
	ret = (!res.a0) && (mem != 0);
#else
	arm_smccc_smc(0x8400ff2e, 0, 0, 0, 0, 0, 0, 0, &res);
	ret = (res.a0 != 0);
#endif
	return ret;
}

#if defined(CONFIG_TARGET_RTD1625)
static inline u32 rtk_get_bootmode(void)
{
	return (__raw_readl(0x98007678) >> 29) & GENMASK(1, 0);
}
#endif // CONFIG_TARGET_RTD1625

#endif
#endif
