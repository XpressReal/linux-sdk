// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#include <linux/delay.h>
#include <linux/io.h>

#include "sysdbg_common.h"
#include "sysdbg_drv.h"
#include "sysdbg_reg.h"
#include "sysdbg_sysfs.h"
#include "sysdbg_test.h"

static void sysdbg_test_delay_loop(int us)
{
	udelay(us);
}

static int32_t sysdbg_test_scratch_walkbits(uint8_t index, uint8_t offset,
					    bool walk_val)
{
	uint32_t pattern, write_val, read_val;
	bool res;

	for (pattern = 1; pattern != 0; pattern <<= 1) {
		write_val = walk_val ? pattern : ~pattern;
		sysdbg_scratch_write(index, offset, write_val);
		read_val = sysdbg_scratch_read(index, offset);
		res = (read_val == write_val) ? true : false;
		if (sysdbg_get_dbg())
			PRT("SCRATCH#%u@0x%08X,OFST:0x%02x walk %d %s at pattern:0x%08x\n",
			    index,
			    sysdbg_get_map_base(index) +
				    ((index == 0) ? LEGACY_SCRATCH_OFST : 0),
			    offset, walk_val, res ? "passed" : "failed",
			    write_val);
		if (res == false)
			return -EFAULT;
	}

	return 0;
}

static int32_t sysdbg_test_swpc_walkbits(uint8_t index, uint8_t offset,
					 bool walk_val)
{
	uint32_t pattern, write_val, read_val;
	bool res;
	int32_t ret = 0;

	// SWPC EVT
	for (pattern = 1; pattern != 0; pattern <<= 1) {
		write_val = walk_val ? pattern : ~pattern;
		sysdbg_swpc_evt_write(index, offset, write_val);
		read_val = sysdbg_swpc_evt_read(index, offset);
		res = (read_val == write_val) ? true : false;
		if (sysdbg_get_dbg())
			PRT("SWPC-EVT,OFST:0x%02x walk %d %s at pattern:0x%08x\n",
			    offset, walk_val, res ? "passed" : "failed",
			    write_val);
		if (res == false)
			ret |= -EFAULT;
	}
	// SWPC VAL HW
	for (pattern = 1; pattern != 0; pattern <<= 1) {
		write_val = walk_val ? pattern : ~pattern;
		sysdbg_swpc_val_hw_write(index, offset, write_val);
		read_val = sysdbg_swpc_val_hw_read(index, offset);
		res = (read_val == write_val) ? true : false;
		if (sysdbg_get_dbg())
			PRT("SWPC-VAL-H,OFST:0x%02x walk %d %s at pattern:0x%08x\n",
			    offset, walk_val, res ? "passed" : "failed",
			    write_val);
		if (res == false)
			ret |= -EFAULT;
	}
	// SWPC VAL LW
	for (pattern = 1; pattern != 0; pattern <<= 1) {
		write_val = walk_val ? pattern : ~pattern;
		sysdbg_swpc_val_lw_write(index, offset, write_val);
		read_val = sysdbg_swpc_val_lw_read(index, offset);
		res = (read_val == write_val) ? true : false;
		if (sysdbg_get_dbg())
			PRT("SWPC-VAL-L,OFST:0x%02x walk %d %s at pattern:0x%08x\n",
			    offset, walk_val, res ? "passed" : "failed",
			    write_val);
		if (res == false)
			ret |= -EFAULT;
	}

	return ret;
}

static int32_t sysdbg_test_ts_enbl_dsbl(unsigned int hw_ver, bool legacy)
{
	int32_t ret;
	uint32_t ts1, ts2, ts3, atom_inc;
	uint8_t i, map_idx, nr_sets;
	bool cnts_result[V3_TOT_NR_ATOM];

	switch (hw_ver) {
	case 2:
		map_idx = 0;
		nr_sets = 3;
		break;
	case 1:
	case 3:
		if (legacy == LEGACY) {
			map_idx = 0;
			nr_sets = 1;
		} else {
			// Non-legacy 64-bit TS has no control register
			map_idx = 2;
			// Check if non-legacy ATOM INC could be cleared by legacy control
			atom_inc = sysdbg_atom_inc_ro_read(map_idx, 0);
			atom_inc = sysdbg_atom_inc_ro_read(map_idx, 0);
			sysdbg_ts_on_off(0, DSBL);
			sysdbg_ts_on_off(0, ENBL);
			atom_inc = sysdbg_atom_inc_ro_read(map_idx, 0);
			if (atom_inc == 1) {
				PRT("AtomCNT#%u was reset by legacy TS ENBL/DSBL\n",
				    map_idx);
				ret = -EFAULT;
			} else
				ret = 0;
			goto exit;
		}
		break;
	default:
		ret = -EFAULT;
		goto exit;
	}
	// Test TS disable & enable bit
	for (i = map_idx; i < nr_sets; i++) {
		cnts_result[i] = true;
		// Check the default value of SYSDBG control register
		PRT("SYSDBG#%u@0x%08X,CTRL Default:0x%08x\n", i,
		    sysdbg_get_map_base(i),
		    sysdbg_reg_read(i, LEGACY_CTRL_OFST));
		// Read atomic counter twice
		atom_inc = sysdbg_atom_inc_ro_read(i, 0);
		atom_inc = sysdbg_atom_inc_ro_read(i, 0);
		// Check if atomic counter was incremented
		if ((atom_inc == 0) || (atom_inc == 0xDEADBEEF)) {
			cnts_result[i] = false;
			PRT("AtomCNT#%u can't increment\n", i);
		}
		// Disable TS bit
		sysdbg_ts_on_off(i, DSBL);
		// TS should stop counting when disabled, read it twice
		ts1 = sysdbg_ts32_read(i);
		// Add some delay
		sysdbg_test_delay_loop(512);
		ts2 = sysdbg_ts32_read(i);
		// Check if TS was stopped
		if (ts1 != ts2) {
			cnts_result[i] = false;
			PRT("TS#%u didn't stop when disabled\n", i);
		}
		if (sysdbg_get_dbg())
			PRT("SYSDBG#%u@0x%08X was disabled,TS#1:0x%08x,TS#2:0x%08x,ATOM_INC:0x%08x\n",
			    i, sysdbg_get_map_base(i), ts1, ts2, atom_inc);
		// Enable TS bit
		sysdbg_ts_on_off(i, ENBL);
		// Read TS again
		ts3 = sysdbg_ts32_read(i);
		// TS would be reset to 0 and start counting during re-enabling
		if (ts3 > ts1) {
			cnts_result[i] = false;
			PRT("TS#%u seems not to be reset to 0\n", i);
		}
		// Read atomic counter again
		atom_inc = sysdbg_atom_inc_ro_read(i, 0);
		// Check if atomic counter was reset to 0
		if (atom_inc != 1) {
			cnts_result[i] = false;
			PRT("AtomCNT#%u seems not to be reset to 0\n", i);
		}
	}
	// Examine & return final test result
	ret = 0;
	for (i = map_idx; i < nr_sets; i++) {
		if (cnts_result[i] == false) {
			ret = -EFAULT;
			break;
		}
	}

exit:
	return ret;
}

static int32_t sysdbg_test_ts_rw_wrap(unsigned int hw_ver, bool legacy)
{
	int32_t ret;
	uint32_t ts1, ts2;
	uint8_t i, start, end;
	bool cnts_result[V3_TOT_NR_ATOM];

	switch (hw_ver) {
	case 2:
		start = 0;
		end = 3;
		break;
	case 1:
	case 3:
		if (legacy == LEGACY) {
			start = 0;
			end = 1;
		} else {
			// Non-legacy 64-bit TS is read-only
			return 0;
		}
		break;
	default:
		ret = -EFAULT;
		goto exit;
	}
	// Test TS write/read and overflow wrap
	for (i = start; i < end; i++) {
		cnts_result[i] = true;
		// Write TS to 0
		sysdbg_ts32_write(i, 0x00000000);
		// Read TS
		ts1 = sysdbg_ts32_read(i);
		// Add some delay
		sysdbg_test_delay_loop(512);
		// Read TS again
		ts2 = sysdbg_ts32_read(i);
		if (sysdbg_get_dbg())
			PRT("SYSDBG#%u@0x%08X TS was set to 0x0,Val#1:0x%08x,Val#2:0x%08x\n",
			    i, sysdbg_get_map_base(i), ts1, ts2);
		// Check if TS value lies within a reasonable range
		if ((ts1 >= 0x00010000) || (ts2 >= 0x00010000)) {
			cnts_result[i] = false;
			PRT("TS#%u read/write abnormal\n", i);
		}
		// Write TS to a value near 32-bit integer limit
		sysdbg_ts32_write(i, 0xFFFFFF00);
		// Read TS
		ts1 = sysdbg_ts32_read(i);
		// Add some delay
		sysdbg_test_delay_loop(4096);
		// Read TS again
		ts2 = sysdbg_ts32_read(i);
		if (sysdbg_get_dbg())
			PRT("SYSDBG#%u@0x%08X TS was set to 0xFFFFFF00,Val#1:0x%08x,Val#2:0x%08x\n",
			    i, sysdbg_get_map_base(i), ts1, ts2);
		// Check if TS is overflow wrapped
		if (ts2 >= ts1) {
			cnts_result[i] = false;
			PRT("TS#%u seems not wrap,Val#1:0x%08x,Val#2:0x%08x\n",
			    i, ts1, ts2);
		}
	}
	// Examine & return final test result
	ret = 0;
	for (i = start; i < end; i++) {
		if (cnts_result[i] == false) {
			ret = -EFAULT;
			break;
		}
	}

exit:
	return ret;
}

static int32_t sysdbg_test_ts_writable(unsigned int hw_ver, bool legacy)
{
	uint8_t i;
	unsigned long flags;
	uint32_t ts32_1, ts32_2;
	uint64_t ts64_1, ts64_2;
	int32_t ret = 0;

	switch (hw_ver) {
	case 2:
		// 3 legacy sets in version 2, ignore scratch 1
		for (i = 0; i < (V2_TOT_NR_REG_SETS - 1); i++) {
			local_irq_save(flags);
			ts32_1 = sysdbg_ts32_read(i);
			sysdbg_ts32_write(i, 0x00000000);
			ts32_2 = sysdbg_ts32_read(i);
			local_irq_restore(flags);
			ret = (ts32_2 < ts32_1) ? 0 : -EFAULT;
			if (ret == -EFAULT)
				break;
		}
		break;
	case 1:
	case 3:
		if (legacy == LEGACY) {
			local_irq_save(flags);
			ts32_1 = sysdbg_ts32_read(0);
			sysdbg_ts32_write(0, 0x00000000);
			ts32_2 = sysdbg_ts32_read(0);
			local_irq_restore(flags);
			ret = (ts32_2 < ts32_1) ? 0 : -EFAULT;
		} else {
			// 64-bit TS is read-only
			local_irq_save(flags);
			ts64_1 = sysdbg_ts64_read(1);
			sysdbg_ts64_write(1, 0x0);
			ts64_2 = sysdbg_ts64_read(1);
			local_irq_restore(flags);
			// Check if 64-bit TS was cleared
			if (ts64_2 <= ts64_1) {
				PRT("64-bit TS seems be changed after setting to 0! (TS64#1=0x%llx, TS64#2=0x%llx)\n",
				    ts64_1, ts64_2);
				ret = -EFAULT;
			}
			// Check if the increment of 64-bit TS is reasonable
			if ((ts64_2 - ts64_1) > 0x1000000UL) {
				PRT("64-bit TS increment seems unreasonable! (DIFF=0x%llx)\n",
				    ts64_2 - ts64_1);
				ret = -EFAULT;
			}
		}
		break;
	}

	return ret;
}

static int32_t sysdbg_test_atom_inc_rw_wrap(unsigned int hw_ver, bool legacy)
{
	int32_t ret;
	uint32_t atom_inc1, atom_inc2;
	uint8_t i, nr_sets, map_idx;
	bool cnts_result[V3_TOT_NR_ATOM];

	switch (hw_ver) {
	case 2:
		nr_sets = 3;
		break;
	case 1:
	case 3:
		if (legacy == LEGACY)
			nr_sets = 1;
		else
			nr_sets = 4;
		break;
	default:
		ret = -EFAULT;
		goto exit;
	}
	// Test ATOM INC write/read and overflow wrap
	for (i = 0; i < nr_sets; i++) {
		switch (hw_ver) {
		case 1:
		case 2:
			map_idx = i;
			break;
		case 3:
			if (legacy == LEGACY)
				map_idx = 0;
			else
				map_idx = 2;
			break;
		}
		cnts_result[i] = true;
		// Read atomic counter
		atom_inc1 = sysdbg_atom_inc_ro_read(map_idx,
						    i * V3_ATOM_INC_NEXT_OFST);
		// Write atomic counter (read-only)
		sysdbg_atom_inc_ro_write(map_idx, i * V3_ATOM_INC_NEXT_OFST,
					 atom_inc1 - 1);
		// Read atomic counter again
		atom_inc2 = sysdbg_atom_inc_ro_read(map_idx,
						    i * V3_ATOM_INC_NEXT_OFST);
		if (sysdbg_get_dbg())
			PRT("SYSDBG#%u@0x%08X ATOM_INC(RO) was set to Val(RO)#1:0x%08x - 1,Val#2(RO):0x%08x\n",
			    i, sysdbg_get_map_base(i), atom_inc1, atom_inc2);
		// Check if the write to read-only counter is discarded
		if (atom_inc1 == atom_inc2) {
			cnts_result[i] = false;
			PRT("ATOM_INC_RO#%u seems not to be read-only\n", i);
		}
		// Read atomic counter (RO)
		atom_inc1 = sysdbg_atom_inc_ro_read(map_idx,
						    i * V3_ATOM_INC_NEXT_OFST);
		// Write atomic counter
		sysdbg_atom_inc_rw_write(map_idx, i * V3_ATOM_INC_NEXT_OFST,
					 atom_inc1 - 1);
		// Read atomic counter again
		atom_inc2 = sysdbg_atom_inc_ro_read(map_idx,
						    i * V3_ATOM_INC_NEXT_OFST);
		if (sysdbg_get_dbg())
			PRT("SYSDBG#%u@0x%08X ATOM_INC(RW) was set to Val(RO)#1:0x%08x - 1,Val(RO)#2:0x%08x\n",
			    i, sysdbg_get_map_base(i), atom_inc1, atom_inc2);
		// Check if the write operation is permitted
		if (atom_inc1 != atom_inc2) {
			cnts_result[i] = false;
			PRT("ATOM_INC_RW#%u seems not work\n", i);
		}
		// Write atomic counter (RW)
		sysdbg_atom_inc_rw_write(map_idx, i * V3_ATOM_INC_NEXT_OFST,
					 0xFFFFFFFF);
		// Read atomic counter (RO)
		atom_inc1 = sysdbg_atom_inc_ro_read(map_idx,
						    i * V3_ATOM_INC_NEXT_OFST);
		// Read atomic counter again
		atom_inc2 = sysdbg_atom_inc_ro_read(map_idx,
						    i * V3_ATOM_INC_NEXT_OFST);
		if (sysdbg_get_dbg())
			PRT("SYSDBG#%u@0x%08X ATOM_INC(RW) was set to 0xFFFFFFFF,Val(RO)#1:0x%08x,Val(RO)#2:0x%08x\n",
			    i, sysdbg_get_map_base(i), atom_inc1, atom_inc2);
		// Check if atomic counter is overflow wrapped
		if ((atom_inc1 != 0x0) || (atom_inc2 != 0x1)) {
			cnts_result[i] = false;
			PRT("ATOM_INC#%u seems not wrap\n", i);
		}
	}
	// Examine & return final test result
	ret = 0;
	for (i = 0; i < nr_sets; i++) {
		if (cnts_result[i] == false) {
			ret = -EFAULT;
			break;
		}
	}

exit:
	return ret;
}

static int32_t sysdbg_test_scratch(unsigned int hw_ver, bool legacy)
{
	int32_t ret;
	uint8_t i, map_idx, nr_sets;
	bool scratch_result[V3_TOT_NR_SCRATCH];

	switch (hw_ver) {
	case 1:
		map_idx = 0;
		nr_sets = V1_TOT_NR_SCRATCH;
		break;
	case 2:
		map_idx = 3;
		nr_sets = V2_TOT_NR_SCRATCH;
		break;
	case 3:
		if (legacy == LEGACY) {
			map_idx = 0;
			nr_sets = V3_LEGACY_TOT_NR_SCRATCH;
		} else {
			map_idx = 3;
			nr_sets = V3_TOT_NR_SCRATCH;
		}
		break;
	default:
		ret = -EFAULT;
		goto exit;
	}
	// Test scratch register sets
	for (i = 0; i < nr_sets; i++) {
		scratch_result[i] = true;
		ret = sysdbg_test_scratch_walkbits(map_idx, i * 0x4, WALK0);
		if (ret == -EFAULT) {
			scratch_result[i] = false;
			continue;
		}
		ret = sysdbg_test_scratch_walkbits(map_idx, i * 0x4, WALK1);
		if (ret == -EFAULT)
			scratch_result[i] = false;
	}
	// Examine & return final test result
	ret = 0;
	for (i = 0; i < nr_sets; i++) {
		if (scratch_result[i] == false) {
			ret = -EFAULT;
			break;
		}
	}

exit:
	return ret;
}

static int32_t sysdbg_test_clk_div(unsigned int hw_ver, bool legacy)
{
	int32_t ret;
	uint32_t ts1, ts2, diff1, diff2, diff3;
	uint8_t i, start, end;
	unsigned long flags;
	bool cnts_result[V3_TOT_NR_ATOM];

	switch (hw_ver) {
	case 2:
		start = 0;
		end = 3;
		break;
	case 1:
	case 3:
		if (legacy == LEGACY) {
			start = 0;
			end = 1;
		} else {
			PRT("There is no clock divider for non-legacy TS\n");
			ret = 0;
			goto exit;
		}
		break;
	default:
		ret = -EFAULT;
		goto exit;
	}
	// Test clock divider with TS diff
	for (i = start; i < end; i++) {
		cnts_result[i] = true;
		local_irq_save(flags);
		// Get TS diff with div = 1
		ts1 = sysdbg_ts32_read(i);
		sysdbg_test_delay_loop(512);
		ts2 = sysdbg_ts32_read(i);
		diff1 = ts2 - ts1;
		sysdbg_set_clk_div(i, 0x3);
		// Get TS diff with div = 3
		ts1 = sysdbg_ts32_read(i);
		sysdbg_test_delay_loop(512);
		ts2 = sysdbg_ts32_read(i);
		diff2 = ts2 - ts1;
		sysdbg_set_clk_div(i, 0x7);
		// Get TS diff with div = 7
		ts1 = sysdbg_ts32_read(i);
		sysdbg_test_delay_loop(512);
		ts2 = sysdbg_ts32_read(i);
		local_irq_restore(flags);
		diff3 = ts2 - ts1;
		if (sysdbg_get_dbg())
			PRT("SYSDBG#%u@0x%08X Diff#1:0x%08x(DIV=1),Diff#2:0x%08x(DIV=3),Diff#3:0x%08x(DIV=7)\n",
			    i, sysdbg_get_map_base(i), diff1, diff2, diff3);
		if (((diff1 >> 2) > diff2) || ((diff1 >> 3) > diff3)) {
			cnts_result[i] = false;
			PRT("DIV#%u seems not work\n", i);
		}
	}
	ret = 0;
	for (i = start; i < end; i++) {
		// Set div to hardware default
		sysdbg_set_clk_div(i, 0x1);
		// Examine & return final test result
		if (cnts_result[i] == false) {
			ret = -EFAULT;
			break;
		}
	}

exit:
	return ret;
}

static int32_t sysdbg_test_swpc(unsigned int hw_ver, bool legacy)
{
	int32_t ret;
	uint8_t i, map_idx, nr_sets;
	bool swpc_result[V3_TOT_NR_SWPC];
	uint32_t ts1, ts2;

	if (legacy == LEGACY) {
		PRT("There is no SWPC for legacy TS\n");
		ret = 0;
		goto exit;
	} else {
		map_idx = 4;
		nr_sets = V3_TOT_NR_SWPC;
	}
	// Test scratch register sets
	for (i = 0; i < nr_sets; i++) {
		swpc_result[i] = true;
		ret = sysdbg_test_swpc_walkbits(map_idx, i * V3_SWPC_NEXT_OFST,
						WALK0);
		if (ret == -EFAULT) {
			swpc_result[i] = false;
			continue;
		}
		ret = sysdbg_test_swpc_walkbits(map_idx, i * V3_SWPC_NEXT_OFST,
						WALK1);
		if (ret == -EFAULT)
			swpc_result[i] = false;
	}
	// Examine & return final test result
	ret = 0;
	for (i = 0; i < nr_sets; i++) {
		if (swpc_result[i] == false) {
			ret = -EFAULT;
			PRT("SWPC walking bit test failed at set#%d\n", i);
			goto exit;
		}
	}
	// Examine SWPC TS
	for (i = 0; i < nr_sets - 1; i++) {
		ts1 = sysdbg_swpc_ts32_read(map_idx, i * V3_SWPC_NEXT_OFST);
		ts2 = sysdbg_swpc_ts32_read(map_idx,
					    (i + 1) * V3_SWPC_NEXT_OFST);
		if (ts1 < ts2) {
			if (sysdbg_get_dbg()) {
				PRT("SWPC TS#%d:0x%08x\n", i, ts1);
				if (i == nr_sets - 2)
					PRT("SWPC TS#%d:0x%08x\n", i + 1, ts2);
			}
		} else {
			ret = -EFAULT;
			PRT("SWPC TS seems not work as expected at set#%d\n",
			    i + 1);
			break;
		}
	}

exit:
	return ret;
}

int32_t sysdbg_test_all_cases(bool legacy)
{
	int32_t ret, ret2 = 0;
	unsigned int hw_ver;

	// Only V3 has non-legacy set
	hw_ver = sysdbg_get_hw_ver();
	if ((hw_ver < 3) && (legacy == NON_LEG))
		return 0;

	// TS enable/disable
	ret = sysdbg_test_ts_enbl_dsbl(hw_ver, legacy);
	PRT("%s TS ENBL/DSBL Test: %s\n", (legacy) ? "Legacy" : "Non-legacy",
	    (ret == -EFAULT) ? "Failed" : "Passed");
	ret2 = (ret | ret2) ? -EFAULT : 0;
	// TS read/write/overflow wrap
	ret = sysdbg_test_ts_rw_wrap(hw_ver, legacy);
	PRT("%s TS RW/Wrap Test: %s\n", (legacy) ? "Legacy" : "Non-legacy",
	    (ret == -EFAULT) ? "Failed" : "Passed");
	ret2 = (ret | ret2) ? -EFAULT : 0;
	// TS writable
	ret = sysdbg_test_ts_writable(hw_ver, legacy);
	PRT("%s TS Writable Test: %s\n", (legacy) ? "Legacy" : "Non-legacy",
	    (ret == -EFAULT) ? "Failed" : "Passed");
	ret2 = (ret | ret2) ? -EFAULT : 0;
	// ATOM_INC read/write/overflow wrap
	ret = sysdbg_test_atom_inc_rw_wrap(hw_ver, legacy);
	PRT("%s ATOM_INC RW/Wrap Test: %s\n",
	    (legacy) ? "Legacy" : "Non-legacy",
	    (ret == -EFAULT) ? "Failed" : "Passed");
	ret2 = (ret | ret2) ? -EFAULT : 0;
	// Scratch test
	ret = sysdbg_test_scratch(hw_ver, legacy);
	PRT("%s Scratch Test: %s\n", (legacy) ? "Legacy" : "Non-legacy",
	    (ret == -EFAULT) ? "Failed" : "Passed");
	ret2 = (ret | ret2) ? -EFAULT : 0;
	// CLK DIV test
	ret = sysdbg_test_clk_div(hw_ver, legacy);
	PRT("%s CLK DIV Test: %s\n", (legacy) ? "Legacy" : "Non-legacy",
	    (ret == -EFAULT) ? "Failed" : "Passed");
	ret2 = (ret | ret2) ? -EFAULT : 0;
	// SWPC test
	ret = sysdbg_test_swpc(hw_ver, legacy);
	PRT("%s SWPC Test: %s\n", (legacy) ? "Legacy" : "Non-legacy",
	    (ret == -EFAULT) ? "Failed" : "Passed");
	ret2 = (ret | ret2) ? -EFAULT : 0;

	return ret2;
}
