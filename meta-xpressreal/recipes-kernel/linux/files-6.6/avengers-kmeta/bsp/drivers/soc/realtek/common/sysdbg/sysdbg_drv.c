// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#include "sysdbg_common.h"
#include "sysdbg_drv.h"
#include "sysdbg_reg.h"
#include "sysdbg_sysfs.h"
#include "sysdbg_test.h"

unsigned int hw_ver;
module_param(hw_ver, uint, 0444);
MODULE_PARM_DESC(hw_ver, "SYSDBG hardware version;");

unsigned int dbg;
module_param(dbg, uint, 0444);
MODULE_PARM_DESC(dbg, "debug mode - 0 off; others on;");

/* Register default mapping */
static struct addr_map v1_dflt_addr_map[V1_TOT_NR_REG_SETS] = {
	// Legacy SYSDBG (with 1 scratch register)
	{ .base = V1_LEGACY_MAP_BASE, .size = V1_LEGACY_MAP_SIZE },
};

static struct addr_map v2_dflt_addr_map[V2_TOT_NR_REG_SETS] = {
	// Legacy SYSDBG x 3 + 16 scratch register
	{ .base = V2_LEGACY0_MAP_BASE, .size = V2_LEGACY0_MAP_SIZE },
	{ .base = V2_LEGACY1_MAP_BASE, .size = V2_LEGACY1_MAP_SIZE },
	{ .base = V2_LEGACY2_MAP_BASE, .size = V2_LEGACY2_MAP_SIZE },
	{ .base = V2_SCRATCH_MAP_BASE, .size = V2_SCRATCH_MAP_SIZE },
};

static struct addr_map v3_dflt_addr_map[V3_TOT_NR_REG_SETS] = {
	// Legacy SYSDBG (with 4 scratch register)
	{ .base = V3_LEGACY_MAP_BASE, .size = V3_LEGACY_MAP_SIZE },
	// RTK SYSDBG Timestamp
	{ .base = V3_TS_MAP_BASE, .size = V3_TS_MAP_SIZE },
	// RTK SYSDBG Atomic INC
	{ .base = V3_ATOM_INC_MAP_BASE, .size = V3_ATOM_INC_MAP_SIZE },
	// RTK SYSDBG Scratch
	{ .base = V3_SCRATCH_MAP_BASE, .size = V3_SCRATCH_MAP_SIZE },
	// RTK SWPC
	{ .base = V3_SWPC_MAP_BASE, .size = V3_SWPC_MAP_SIZE },
};

/* Global variables */
static void __iomem *maps[V3_TOT_NR_REG_SETS];

static int32_t sysdbg_create_map(unsigned int hw_ver, bool legacy)
{
	uint8_t i;
	int32_t ret = 0;

	switch (hw_ver) {
	case 1:
		if (legacy) {
			maps[0] = ioremap(v1_dflt_addr_map[0].base,
					  v1_dflt_addr_map[0].size);
			if (!(maps[0])) {
				PRT("Map failed on legacy set#0\n");
				ret = -EINVAL;
			}
		}
		break;
	case 2:
		if (legacy) {
			for (i = 0; i < V2_TOT_NR_REG_SETS; i++) {
				maps[i] = ioremap(v2_dflt_addr_map[i].base,
						  v2_dflt_addr_map[i].size);
				if (!(maps[i])) {
					PRT("Map failed on non-legacy set#%hu\n",
					    i);
					ret = -EINVAL;
					break;
				}
			}
		}
		break;
	case 3:
		if (legacy) {
			maps[0] = ioremap(v3_dflt_addr_map[0].base,
					  v3_dflt_addr_map[0].size);
			if (!(maps[0])) {
				PRT("Map failed on legacy set#0\n");
				ret = -EINVAL;
			}
		} else {
			for (i = 1; i < V3_TOT_NR_REG_SETS; i++) {
				maps[i] = ioremap(v3_dflt_addr_map[i].base,
						  v3_dflt_addr_map[i].size);
				if (!(maps[i])) {
					PRT("Map failed on non-legacy set#%hu\n",
					    i);
					ret = -EINVAL;
					break;
				}
			}
		}
		break;
	default:
		PRT("Unsupported hardware version\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void sysdbg_destroy_map(unsigned int hw_ver, bool legacy)
{
	uint8_t i;

	switch (hw_ver) {
	case 1:
		if (legacy)
			iounmap(maps[0]);
		break;
	case 2:
		if (legacy) {
			for (i = 0; i < V2_TOT_NR_REG_SETS; i++)
				iounmap(maps[i]);
		}
		break;
	case 3:
		if (legacy) {
			iounmap(maps[0]);
		} else {
			for (i = 1; i < V3_TOT_NR_REG_SETS; i++)
				iounmap(maps[i]);
		}
	default:
		break;
	}
}

unsigned int sysdbg_get_hw_ver(void)
{
	return hw_ver;
}

unsigned int sysdbg_get_dbg(void)
{
	return dbg;
}

uint32_t sysdbg_get_map_base(uint8_t index)
{
	switch (hw_ver) {
	case 1:
		if (index < V1_TOT_NR_REG_SETS)
			return v1_dflt_addr_map[index].base;
		else
			return 0xDEADBEEF;
	case 2:
		if (index < V2_TOT_NR_REG_SETS)
			return v2_dflt_addr_map[index].base;
		else
			return 0xDEADBEEF;
	case 3:
		if (index < V3_TOT_NR_REG_SETS)
			return v3_dflt_addr_map[index].base;
		else
			return 0xDEADBEEF;
	default:
		return 0xDEADBEEF;
	}
}

void sysdbg_ts_on_off(uint8_t index, bool enable)
{
	uint32_t val;

	if ((hw_ver == 3) && (index != 0)) {
		PRT("Not support TS on/off at non-legacy set#%d\n", index);
		return;
	}

	val = readl((maps[index] + LEGACY_CTRL_OFST)) & LEGACY_CTRL_MASK;
	if (enable == ENBL)
		writel((val | LEGACY_TS_EN_MASK),
		       (maps[index] + LEGACY_CTRL_OFST));
	else
		writel((val & (~LEGACY_TS_EN_MASK)),
		       (maps[index] + LEGACY_CTRL_OFST));
}

void sysdbg_set_clk_div(uint8_t index, uint16_t div_n)
{
	uint32_t val;

	if ((hw_ver == 3) && (index != 0)) {
		PRT("Not support CLK DIV at non-legacy set#%d\n", index);
		return;
	}

	val = readl((maps[index] + LEGACY_CTRL_OFST)) & LEGACY_CTRL_MASK &
	      (~LEGACY_CLK_DIV_MASK);
	if (div_n)
		writel((val | (div_n & LEGACY_CLK_DIV_MASK)),
		       (maps[index] + LEGACY_CTRL_OFST));
}

uint32_t sysdbg_ts32_read(uint8_t index)
{
	if ((hw_ver == 3) && (index != 0))
		return 0xDEADBEEF;

	return readl((maps[index] + LEGACY_TS_OFST));
}

void sysdbg_ts32_write(uint8_t index, uint32_t value)
{
	if ((hw_ver == 3) && (index != 0))
		PRT("Not support TS write at set#%d\n", index);
	else
		writel(value, (maps[index] + LEGACY_TS_OFST));
}

uint64_t sysdbg_ts64_read(uint8_t index)
{
	uint64_t ts;
	uint32_t low, high;

	if ((hw_ver == 3) && (index == 1)) {
		// Caution: Read LSB word (TS_LW_OFST) first then MSB (TS_HW_OFST)
		low = readl((maps[index] + V3_TS_LW_OFST));
		high = readl((maps[index] + V3_TS_HW_OFST));
		//	if (dbg)
		//		PRT("64bit-TS:HW=0x%08x,LW=0x%08x\n", high, low);
		ts = ((uint64_t)high << 32) | low;
	} else
		ts = 0xDEADBEEFBAADF00D;

	return ts;
}

void sysdbg_ts64_write(uint8_t index, uint64_t value)
{
	uint32_t low, high;

	if ((hw_ver == 3) && (index == 1)) {
		// Caution: 64-bit TS should be read-only, just for testing if it really is
		low = value & 0xFFFFFFFF;
		high = value >> 32;
		if (dbg)
			PRT("Write 64bit-TS:HW=0x%08x,LW=0x%08x\n", high, low);
		writel(low, (maps[index] + V3_TS_LW_OFST));
		writel(high, (maps[index] + V3_TS_HW_OFST));
	} else
		PRT("Not support 64-bit TS write at set#%d\n", index);
}

uint32_t sysdbg_atom_inc_ro_read(uint8_t index, uint8_t offset)
{
	switch (hw_ver) {
	case 1:
	case 2:
		return readl((maps[index] + LEGACY_ATOM_INC_RO_OFST));
	case 3:
		if (index == 0)
			return readl((maps[index] + LEGACY_ATOM_INC_RO_OFST));
		else if (index == 2)
			return readl((maps[index] + V3_ATOM_INC_RO_OFST + offset));
	default:
		return 0xDEADBEEF;
	}
}

void sysdbg_atom_inc_ro_write(uint8_t index, uint8_t offset, uint32_t value)
{
	switch (hw_ver) {
	case 1:
	case 2:
		writel(value, (maps[index] + LEGACY_ATOM_INC_RO_OFST));
		break;
	case 3:
		if (index == 0)
			writel(value, (maps[index] + LEGACY_ATOM_INC_RO_OFST));
		else if (index == 2)
			writel(value, (maps[index] + V3_ATOM_INC_RO_OFST + offset));
		break;
	default:
		PRT("Not support ATOM INC RO write at set#%d\n", index);
		break;
	}
}

uint32_t sysdbg_atom_inc_rw_read(uint8_t index, uint8_t offset)
{
	switch (hw_ver) {
	case 1:
	case 2:
		return readl((maps[index] + LEGACY_ATOM_INC_RW_OFST));
	case 3:
		if (index == 0)
			return readl((maps[index] + LEGACY_ATOM_INC_RW_OFST));
		else if (index == 2)
			return readl((maps[index] + V3_ATOM_INC_RW_OFST + offset));
	default:
		return 0xDEADBEEF;
	}
}

void sysdbg_atom_inc_rw_write(uint8_t index, uint8_t offset, uint32_t value)
{
	switch (hw_ver) {
	case 1:
	case 2:
		writel(value, (maps[index] + LEGACY_ATOM_INC_RW_OFST));
		break;
	case 3:
		if (index == 0)
			writel(value, (maps[index] + LEGACY_ATOM_INC_RW_OFST));
		else if (index == 2)
			writel(value, (maps[index] + V3_ATOM_INC_RW_OFST + offset));
		break;
	default:
		PRT("Not support ATOM INC RW write at set#%d\n", index);
		break;
	}
}

uint32_t sysdbg_scratch_read(uint8_t index, uint8_t offset)
{
	switch (hw_ver) {
	case 1:
		return readl((maps[index] + LEGACY_SCRATCH_OFST));
	case 2:
		return readl((maps[index] + offset));
	case 3:
		if (index == 0)
			return readl(
				(maps[index] + LEGACY_SCRATCH_OFST + offset));
		else if (index == 3)
			return readl((maps[index] + offset));
	default:
		return 0xDEADBEEF;
	}
}

void sysdbg_scratch_write(uint8_t index, uint8_t offset, uint32_t value)
{
	switch (hw_ver) {
	case 1:
		writel(value, (maps[index] + LEGACY_SCRATCH_OFST));
		break;
	case 2:
		writel(value, (maps[index] + offset));
		break;
	case 3:
		if (index == 0)
			writel(value,
			       (maps[index] + LEGACY_SCRATCH_OFST + offset));
		else if (index == 3)
			writel(value, (maps[index] + offset));
		break;
	default:
		PRT("Not support scratch write at set#%d\n", index);
		break;
	}
}

uint32_t sysdbg_ts64_cmp_ctrl_read(uint8_t index)
{
	if ((hw_ver == 3) && (index == 1))
		return readl((maps[index] + V3_TS_CMP_CTRL_OFST));

	PRT("Not support TS CMP at set#%d\n", index);
	return 0xDEADBEEF;
}

void sysdbg_ts64_cmp_ctrl_write(uint8_t index, bool wr_en0, bool cmp_en,
				bool wr_en1, bool scpu_en, bool wr_en2,
				bool pcpu_en)
{
	uint32_t val;

	if ((hw_ver == 3) && (index == 1)) {
		val = readl((maps[index] + V3_TS_CMP_CTRL_OFST));

		if (wr_en0 == ENBL)
			val |= V3_TS_WR_EN0_MASK;
		else
			val &= (~V3_TS_WR_EN0_MASK);

		if (cmp_en == ENBL)
			val |= V3_TS_CMP_EN_MASK;
		else
			val &= (~V3_TS_CMP_EN_MASK);

		if (wr_en1 == ENBL)
			val |= V3_TS_WR_EN1_MASK;
		else
			val &= (~V3_TS_WR_EN1_MASK);

		if (scpu_en == ENBL)
			val |= V3_TS_SCPU_INT_EN_MASK;
		else
			val &= (~V3_TS_SCPU_INT_EN_MASK);

		if (wr_en2 == ENBL)
			val |= V3_TS_WR_EN2_MASK;
		else
			val &= (~V3_TS_WR_EN2_MASK);

		if (pcpu_en == ENBL)
			val |= V3_TS_PCPU_INT_EN_MASK;
		else
			val &= (~V3_TS_PCPU_INT_EN_MASK);

		writel(val, (maps[index] + V3_TS_CMP_CTRL_OFST));
		//if (dbg)
		//	PRT("Write TS CMP CTRL = 0x%08x\n", val);
	} else
		PRT("Not support TS CMP at set#%d\n", index);
}

uint64_t sysdbg_ts64_cmp_value_read(uint8_t index)
{
	uint64_t cmp;

	if ((hw_ver == 3) && (index == 1)) {
		cmp = (uint64_t)readl((maps[index] + V3_TS_CMP_VAL_HW_OFST))
		      << 32;
		cmp |= (uint64_t)readl((maps[index] + V3_TS_CMP_VAL_LW_OFST));
	} else {
		PRT("Not support TS CMP Read at set#%d\n", index);
		return 0xDEADBEEFBAADF00D;
	}

	return cmp;
}

void sysdbg_ts64_cmp_value_write(uint8_t index, uint64_t value)
{
	uint32_t high, low;

	if ((hw_ver == 3) && (index == 1)) {
		low = value & 0xFFFFFFFF;
		high = value >> 32;
		//if (dbg)
		//	PRT("Write 64bit-CMP:HW=0x%08x,LW=0x%08x\n", high, low);
		writel(high, (maps[index] + V3_TS_CMP_VAL_HW_OFST));
		writel(low, (maps[index] + V3_TS_CMP_VAL_LW_OFST));
	} else
		PRT("Not support TS CMP Write at set#%d\n", index);
}

uint32_t sysdbg_swpc_evt_read(uint8_t index, uint8_t offset)
{
	if ((hw_ver == 3) && (index == 4))
		return readl((maps[index] + offset + V3_SWPC_EVT_OFST));
	else
		return 0xDEADBEEF;
}

void sysdbg_swpc_evt_write(uint8_t index, uint8_t offset, uint32_t value)
{
	if ((hw_ver == 3) && (index == 4))
		writel(value, (maps[index] + offset + V3_SWPC_EVT_OFST));
	else
		PRT("Not support SWPC EVT Write at set#%d\n", index);
}

uint32_t sysdbg_swpc_val_hw_read(uint8_t index, uint8_t offset)
{
	if ((hw_ver == 3) && (index == 4))
		return readl((maps[index] + offset + V3_SWPC_VAL_HW_OFST));
	else
		return 0xDEADBEEF;
}

void sysdbg_swpc_val_hw_write(uint8_t index, uint8_t offset, uint32_t value)
{
	if ((hw_ver == 3) && (index == 4))
		writel(value, (maps[index] + offset + V3_SWPC_VAL_HW_OFST));
	else
		PRT("Not support SWPC VAL-H Write at set#%d\n", index);
}

uint32_t sysdbg_swpc_val_lw_read(uint8_t index, uint8_t offset)
{
	if ((hw_ver == 3) && (index == 4))
		return readl((maps[index] + offset + V3_SWPC_VAL_LW_OFST));
	else
		return 0xDEADBEEF;
}

void sysdbg_swpc_val_lw_write(uint8_t index, uint8_t offset, uint32_t value)
{
	if ((hw_ver == 3) && (index == 4))
		writel(value, (maps[index] + offset + V3_SWPC_VAL_LW_OFST));
	else
		PRT("Not support SWPC VAL-L Write at set#%d\n", index);
}

uint32_t sysdbg_swpc_ts32_read(uint8_t index, uint8_t offset)
{
	if ((hw_ver == 3) && (index == 4))
		return readl((maps[index] + offset + V3_SWPC_TS_OFST));
	else
		return 0xDEADBEEF;
}

uint32_t sysdbg_reg_read(int map_idx, int offset)
{
	return readl((maps[map_idx] + offset));
}

void sysdbg_reg_write(int map_idx, int offset, uint32_t value)
{
	writel(value, (maps[map_idx] + offset));
}

static int __init rtk_sysdbg_init(void)
{
	if (hw_ver == 0) {
		PRT("Please specify SYSDBG hardware version first!\n");
		goto exit;
	}

	// Create address map
	if (sysdbg_create_map(hw_ver, LEGACY))
		goto exit;
	if (sysdbg_create_map(hw_ver, NON_LEG))
		goto exit;

	// Create SYSFS nodes
	if (sysdbg_sysfs_create(hw_ver))
		goto exit;

	PRT("RTK SYSDBG module is loaded!\n");

exit:
	return 0;
}
module_init(rtk_sysdbg_init);

static void __exit rtk_sysdbg_exit(void)
{
	if (hw_ver == 0)
		return;

	// Destroy SYSFS nodes
	sysdbg_sysfs_destroy();

	// Destroy address map
	sysdbg_destroy_map(hw_ver, NON_LEG);
	sysdbg_destroy_map(hw_ver, LEGACY);

	PRT("RTK SYSDBG module was exited!\n");
}
module_exit(rtk_sysdbg_exit);

MODULE_AUTHOR("Shawn Huang <shawn.huang724@realtek.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek SYSDBG driver");
