/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#ifndef SYSDBG_REG_H
#define SYSDBG_REG_H

#ifdef __cplusplus
extern "C" {
#endif

/* SYSDBG Address Mapping */
#define V1_LEGACY_MAP_BASE 0x980076E0
#define V1_LEGACY_MAP_SIZE 0x10
#define V2_LEGACY0_MAP_BASE 0x9801B100
#define V2_LEGACY0_MAP_SIZE 0x10
#define V2_LEGACY1_MAP_BASE 0x9801B110
#define V2_LEGACY1_MAP_SIZE 0x10
#define V2_LEGACY2_MAP_BASE 0x9801B120
#define V2_LEGACY2_MAP_SIZE 0x10
#define V2_SCRATCH_MAP_BASE 0x9801B140
#define V2_SCRATCH_MAP_SIZE 0x3C
#define V3_LEGACY_MAP_BASE 0x98089400
#define V3_LEGACY_MAP_SIZE 0x1C
#define V3_TS_MAP_BASE 0x98089420
#define V3_TS_MAP_SIZE 0x14
#define V3_ATOM_INC_MAP_BASE 0x98089440
#define V3_ATOM_INC_MAP_SIZE 0x1C
#define V3_SCRATCH_MAP_BASE 0x98089460
#define V3_SCRATCH_MAP_SIZE 0x3C
#define V3_SWPC_MAP_BASE 0x98089500
#define V3_SWPC_MAP_SIZE 0xF0

/* SYSDBG Legacy Register Offset */
#define LEGACY_CTRL_OFST 0x0
#define LEGACY_TS_OFST 0x4
#define LEGACY_ATOM_INC_RO_OFST 0x8
#define LEGACY_ATOM_INC_RW_OFST 0xC
#define LEGACY_SCRATCH_OFST 0x10

/* SYSDBG Register Offset (RTK Internal Use) */
#define V3_TS_LW_OFST 0x0
#define V3_TS_HW_OFST 0x4
#define V3_TS_CMP_VAL_LW_OFST 0x8
#define V3_TS_CMP_VAL_HW_OFST 0xC
#define V3_TS_CMP_CTRL_OFST 0x10
#define V3_TS_CMP_STAT_OFST 0x14
#define V3_ATOM_INC_RO_OFST 0x0
#define V3_ATOM_INC_RW_OFST 0x4
#define V3_ATOM_INC_NEXT_OFST 0x8
#define V3_SWPC_VAL_LW_OFST 0x0
#define V3_SWPC_VAL_HW_OFST 0x4
#define V3_SWPC_EVT_OFST 0x8
#define V3_SWPC_TS_OFST 0xC
#define V3_SWPC_NEXT_OFST 0x10

/* SYSDBG Legacy CTRL REG Mask */
#define LEGACY_CTRL_MASK 0x000FFFFF
#define LEGACY_TS_EN_MASK 0x00080000
#define LEGACY_CLK_MUX_MASK 0x00070000
#define LEGACY_CLK_DIV_MASK 0x0000FFFF

/* SYSDBG CMP CTRL REG Mask */
#define V3_TS_CMP_EN_MASK 0x1
#define V3_TS_WR_EN0_MASK 0x2
#define V3_TS_SCPU_INT_EN_MASK 0x10
#define V3_TS_WR_EN1_MASK 0x20
#define V3_TS_PCPU_INT_EN_MASK 0x100
#define V3_TS_WR_EN2_MASK 0x200

/* Summary */
#define V1_TOT_NR_REG_SETS 1 // Legacy
#define V1_TOT_NR_SCRATCH 1
#define V2_TOT_NR_REG_SETS 4 // Legacy x 3 + Scratch
#define V2_TOT_NR_SCRATCH 16
#define V3_TOT_NR_REG_SETS 5 // Legacy + TS + ATOM INC + Scratch + SWPC
#define V3_TOT_NR_ATOM 4
#define V3_TOT_NR_SCRATCH 16
#define V3_TOT_NR_SWPC 16
#define V3_LEGACY_TOT_NR_SCRATCH 4

#ifdef __cplusplus
}
#endif

#endif /* SYSDBG_REG_H */
