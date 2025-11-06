// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#include <linux/acpi.h>

#include "sysdbg_common.h"
#include "sysdbg_drv.h"
#include "sysdbg_reg.h"
#include "sysdbg_test.h"

static struct kobject sysdbg_kobj;
static struct kobject sysdbg_reg_kobj;
static int legacy_result = NA;
static int non_leg_result = NA;

static ssize_t all_tests_show(struct kobject *kobj, struct attribute *attr,
			      char *buf)
{
	int ret = 0;

	if (!strncmp(attr->name, "legacy_all_tests", 17)) {
		if (legacy_result == NA)
			ret = sysfs_emit(buf, "Not tested yet!\n");
		else
			ret = sysfs_emit(buf, "All tests were %s!\n",
					 (legacy_result == PASS) ? "passed" :
									 "failed");
	}

	if (!strncmp(attr->name, "non_leg_all_tests", 18)) {
		if (non_leg_result == NA)
			ret = sysfs_emit(buf, "Not tested yet!\n");
		else
			ret = sysfs_emit(buf, "All tests were %s!\n",
					 (non_leg_result == PASS) ? "passed" :
									  "failed");
	}

	return ret;
}

static ssize_t all_tests_store(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count)
{
	int ret, input;

	ret = kstrtoint(buf, 0, &input);

	if (ret < 0)
		return ret;

	if (input == 1) {
		if (!strncmp(attr->name, "legacy_all_tests", 17))
			legacy_result =
				(sysdbg_test_all_cases(LEGACY) == -EFAULT) ?
					      FAIL :
					      PASS;
		if (!strncmp(attr->name, "non_leg_all_tests", 18))
			non_leg_result =
				(sysdbg_test_all_cases(NON_LEG) == -EFAULT) ?
					      FAIL :
					      PASS;
	}

	return count;
}

static void all_tests_release(struct kobject *kobj)
{
	;
}

static const struct sysfs_ops all_tests_ops = {
	.show = all_tests_show,
	.store = all_tests_store,
};

static struct attribute legacy_all_tests_attr = {
	.name = "legacy_all_tests",
	.mode = 0660,
};

static struct attribute non_leg_all_tests_attr = {
	.name = "non_leg_all_tests",
	.mode = 0660,
};

static struct attribute *all_tests_attrs[] = {
	&legacy_all_tests_attr,
	&non_leg_all_tests_attr,
	NULL,
};
ATTRIBUTE_GROUPS(all_tests);

static struct kobj_type all_tests_ktype = {
	.sysfs_ops = &all_tests_ops,
	.release = all_tests_release,
	.default_groups = all_tests_groups,
};

struct reg_kattr {
	struct attribute kattr;
	int map_idx;
	int reg_ofst;
};

static ssize_t sysdbg_reg_show(struct kobject *kobj, struct attribute *attr,
			       char *buf)
{
	struct reg_kattr *kat;
	uint32_t low, high;

	kat = container_of(attr, struct reg_kattr, kattr);
	if ((kat->map_idx == 1) && (kat->reg_ofst == V3_TS_LW_OFST)) {
		// Caution: Read LSB word (TS_LW_OFST) first then MSB (TS_HW_OFST)
		low = sysdbg_reg_read(kat->map_idx, V3_TS_LW_OFST);
		high = sysdbg_reg_read(kat->map_idx, V3_TS_HW_OFST);
		return sysfs_emit(buf, "0x%08x%08x\n", high, low);
	} else
		return sysfs_emit(buf, "0x%08x\n",
				  sysdbg_reg_read(kat->map_idx, kat->reg_ofst));
}

static ssize_t sysdbg_reg_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	uint32_t value;
	struct reg_kattr *kat;

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	kat = container_of(attr, struct reg_kattr, kattr);
	sysdbg_reg_write(kat->map_idx, kat->reg_ofst, value);

	return count;
}

static void sysdbg_reg_release(struct kobject *kobj)
{
	;
}

static const struct sysfs_ops sysdbg_reg_ops = {
	.show = sysdbg_reg_show,
	.store = sysdbg_reg_store,
};

// Macro for declare register attribute
#define SYSDBG_REG_ATTR(_name, _map_idx, _reg_ofst)                            \
	struct reg_kattr sysdbg_##_name = { .kattr.name = #_name,              \
					    .kattr.mode = 0660,                \
					    .map_idx = _map_idx,               \
					    .reg_ofst = _reg_ofst }

// Declare all register attributes
static SYSDBG_REG_ATTR(ts32_rw_0, 0, LEGACY_TS_OFST);
static SYSDBG_REG_ATTR(ts64_ro_0, 1, V3_TS_LW_OFST);
static SYSDBG_REG_ATTR(v2_ts32_rw_0, 0, LEGACY_TS_OFST);
static SYSDBG_REG_ATTR(v2_ts32_rw_1, 1, LEGACY_TS_OFST);
static SYSDBG_REG_ATTR(v2_ts32_rw_2, 2, LEGACY_TS_OFST);
static SYSDBG_REG_ATTR(atom_inc_ro_0, 0, LEGACY_ATOM_INC_RO_OFST);
static SYSDBG_REG_ATTR(atom_inc_ro_1, 2, V3_ATOM_INC_RO_OFST);
static SYSDBG_REG_ATTR(atom_inc_ro_2, 2,
		       V3_ATOM_INC_RO_OFST + V3_ATOM_INC_NEXT_OFST);
static SYSDBG_REG_ATTR(atom_inc_ro_3, 2,
		       V3_ATOM_INC_RO_OFST + V3_ATOM_INC_NEXT_OFST * 2);
static SYSDBG_REG_ATTR(atom_inc_ro_4, 2,
		       V3_ATOM_INC_RO_OFST + V3_ATOM_INC_NEXT_OFST * 3);
static SYSDBG_REG_ATTR(atom_inc_rw_0, 0, LEGACY_ATOM_INC_RW_OFST);
static SYSDBG_REG_ATTR(atom_inc_rw_1, 2, V3_ATOM_INC_RW_OFST);
static SYSDBG_REG_ATTR(atom_inc_rw_2, 2,
		       V3_ATOM_INC_RW_OFST + V3_ATOM_INC_NEXT_OFST);
static SYSDBG_REG_ATTR(atom_inc_rw_3, 2,
		       V3_ATOM_INC_RW_OFST + V3_ATOM_INC_NEXT_OFST * 2);
static SYSDBG_REG_ATTR(atom_inc_rw_4, 2,
		       V3_ATOM_INC_RW_OFST + V3_ATOM_INC_NEXT_OFST * 3);
static SYSDBG_REG_ATTR(v2_atom_inc_ro_0, 0, LEGACY_ATOM_INC_RO_OFST);
static SYSDBG_REG_ATTR(v2_atom_inc_ro_1, 1, LEGACY_ATOM_INC_RO_OFST);
static SYSDBG_REG_ATTR(v2_atom_inc_ro_2, 2, LEGACY_ATOM_INC_RO_OFST);
static SYSDBG_REG_ATTR(v2_atom_inc_rw_0, 0, LEGACY_ATOM_INC_RW_OFST);
static SYSDBG_REG_ATTR(v2_atom_inc_rw_1, 1, LEGACY_ATOM_INC_RW_OFST);
static SYSDBG_REG_ATTR(v2_atom_inc_rw_2, 2, LEGACY_ATOM_INC_RW_OFST);
static SYSDBG_REG_ATTR(scratch_rw_00, 0, LEGACY_SCRATCH_OFST);
static SYSDBG_REG_ATTR(scratch_rw_01, 0, LEGACY_SCRATCH_OFST + 0x4);
static SYSDBG_REG_ATTR(scratch_rw_02, 0, LEGACY_SCRATCH_OFST + 0x8);
static SYSDBG_REG_ATTR(scratch_rw_03, 0, LEGACY_SCRATCH_OFST + 0xC);
static SYSDBG_REG_ATTR(scratch_rw_04, 3, 0x00);
static SYSDBG_REG_ATTR(scratch_rw_05, 3, 0x04);
static SYSDBG_REG_ATTR(scratch_rw_06, 3, 0x08);
static SYSDBG_REG_ATTR(scratch_rw_07, 3, 0x0C);
static SYSDBG_REG_ATTR(scratch_rw_08, 3, 0x10);
static SYSDBG_REG_ATTR(scratch_rw_09, 3, 0x14);
static SYSDBG_REG_ATTR(scratch_rw_10, 3, 0x18);
static SYSDBG_REG_ATTR(scratch_rw_11, 3, 0x1C);
static SYSDBG_REG_ATTR(scratch_rw_12, 3, 0x20);
static SYSDBG_REG_ATTR(scratch_rw_13, 3, 0x24);
static SYSDBG_REG_ATTR(scratch_rw_14, 3, 0x28);
static SYSDBG_REG_ATTR(scratch_rw_15, 3, 0x2C);
static SYSDBG_REG_ATTR(scratch_rw_16, 3, 0x30);
static SYSDBG_REG_ATTR(scratch_rw_17, 3, 0x34);
static SYSDBG_REG_ATTR(scratch_rw_18, 3, 0x38);
static SYSDBG_REG_ATTR(scratch_rw_19, 3, 0x3C);
static SYSDBG_REG_ATTR(v2_scratch_rw_00, 3, 0x00);
static SYSDBG_REG_ATTR(v2_scratch_rw_01, 3, 0x04);
static SYSDBG_REG_ATTR(v2_scratch_rw_02, 3, 0x08);
static SYSDBG_REG_ATTR(v2_scratch_rw_03, 3, 0x0C);
static SYSDBG_REG_ATTR(v2_scratch_rw_04, 3, 0x10);
static SYSDBG_REG_ATTR(v2_scratch_rw_05, 3, 0x14);
static SYSDBG_REG_ATTR(v2_scratch_rw_06, 3, 0x18);
static SYSDBG_REG_ATTR(v2_scratch_rw_07, 3, 0x1C);
static SYSDBG_REG_ATTR(v2_scratch_rw_08, 3, 0x20);
static SYSDBG_REG_ATTR(v2_scratch_rw_09, 3, 0x24);
static SYSDBG_REG_ATTR(v2_scratch_rw_10, 3, 0x28);
static SYSDBG_REG_ATTR(v2_scratch_rw_11, 3, 0x2C);
static SYSDBG_REG_ATTR(v2_scratch_rw_12, 3, 0x30);
static SYSDBG_REG_ATTR(v2_scratch_rw_13, 3, 0x34);
static SYSDBG_REG_ATTR(v2_scratch_rw_14, 3, 0x38);
static SYSDBG_REG_ATTR(v2_scratch_rw_15, 3, 0x3C);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_00, 4, V3_SWPC_VAL_LW_OFST);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_00, 4, V3_SWPC_VAL_HW_OFST);
static SYSDBG_REG_ATTR(swpc_evt_rw_00, 4, V3_SWPC_EVT_OFST);
static SYSDBG_REG_ATTR(swpc_ts_ro_00, 4, V3_SWPC_TS_OFST);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_01, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_01, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST);
static SYSDBG_REG_ATTR(swpc_evt_rw_01, 4, V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST);
static SYSDBG_REG_ATTR(swpc_ts_ro_01, 4, V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_02, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 2);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_02, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 2);
static SYSDBG_REG_ATTR(swpc_evt_rw_02, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 2);
static SYSDBG_REG_ATTR(swpc_ts_ro_02, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 2);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_03, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 3);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_03, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 3);
static SYSDBG_REG_ATTR(swpc_evt_rw_03, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 3);
static SYSDBG_REG_ATTR(swpc_ts_ro_03, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 3);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_04, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 4);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_04, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 4);
static SYSDBG_REG_ATTR(swpc_evt_rw_04, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 4);
static SYSDBG_REG_ATTR(swpc_ts_ro_04, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 4);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_05, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 5);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_05, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 5);
static SYSDBG_REG_ATTR(swpc_evt_rw_05, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 5);
static SYSDBG_REG_ATTR(swpc_ts_ro_05, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 5);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_06, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 6);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_06, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 6);
static SYSDBG_REG_ATTR(swpc_evt_rw_06, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 6);
static SYSDBG_REG_ATTR(swpc_ts_ro_06, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 6);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_07, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 7);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_07, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 7);
static SYSDBG_REG_ATTR(swpc_evt_rw_07, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 7);
static SYSDBG_REG_ATTR(swpc_ts_ro_07, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 7);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_08, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 8);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_08, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 8);
static SYSDBG_REG_ATTR(swpc_evt_rw_08, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 8);
static SYSDBG_REG_ATTR(swpc_ts_ro_08, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 8);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_09, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 9);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_09, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 9);
static SYSDBG_REG_ATTR(swpc_evt_rw_09, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 9);
static SYSDBG_REG_ATTR(swpc_ts_ro_09, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 9);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_10, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 10);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_10, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 10);
static SYSDBG_REG_ATTR(swpc_evt_rw_10, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 10);
static SYSDBG_REG_ATTR(swpc_ts_ro_10, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 10);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_11, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 11);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_11, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 11);
static SYSDBG_REG_ATTR(swpc_evt_rw_11, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 11);
static SYSDBG_REG_ATTR(swpc_ts_ro_11, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 11);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_12, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 12);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_12, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 12);
static SYSDBG_REG_ATTR(swpc_evt_rw_12, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 12);
static SYSDBG_REG_ATTR(swpc_ts_ro_12, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 12);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_13, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 13);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_13, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 13);
static SYSDBG_REG_ATTR(swpc_evt_rw_13, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 13);
static SYSDBG_REG_ATTR(swpc_ts_ro_13, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 13);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_14, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 14);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_14, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 14);
static SYSDBG_REG_ATTR(swpc_evt_rw_14, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 14);
static SYSDBG_REG_ATTR(swpc_ts_ro_14, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 14);
static SYSDBG_REG_ATTR(swpc_val_lw_rw_15, 4,
		       V3_SWPC_VAL_LW_OFST + V3_SWPC_NEXT_OFST * 15);
static SYSDBG_REG_ATTR(swpc_val_hw_rw_15, 4,
		       V3_SWPC_VAL_HW_OFST + V3_SWPC_NEXT_OFST * 15);
static SYSDBG_REG_ATTR(swpc_evt_rw_15, 4,
		       V3_SWPC_EVT_OFST + V3_SWPC_NEXT_OFST * 15);
static SYSDBG_REG_ATTR(swpc_ts_ro_15, 4,
		       V3_SWPC_TS_OFST + V3_SWPC_NEXT_OFST * 15);

static struct attribute *sysdbg_v1_reg_attrs[] = {
	&sysdbg_ts32_rw_0.kattr,
	&sysdbg_atom_inc_ro_0.kattr,
	&sysdbg_atom_inc_rw_0.kattr,
	&sysdbg_scratch_rw_00.kattr,
	NULL,
};
ATTRIBUTE_GROUPS(sysdbg_v1_reg);

static struct kobj_type sysdbg_v1_reg_ktype = {
	.sysfs_ops = &sysdbg_reg_ops,
	.release = sysdbg_reg_release,
	.default_groups = sysdbg_v1_reg_groups,
};

static struct attribute *sysdbg_v2_reg_attrs[] = {
	&sysdbg_v2_ts32_rw_0.kattr,	&sysdbg_v2_ts32_rw_1.kattr,
	&sysdbg_v2_ts32_rw_2.kattr,	&sysdbg_v2_atom_inc_ro_0.kattr,
	&sysdbg_v2_atom_inc_rw_0.kattr, &sysdbg_v2_atom_inc_ro_1.kattr,
	&sysdbg_v2_atom_inc_rw_1.kattr, &sysdbg_v2_atom_inc_ro_2.kattr,
	&sysdbg_v2_atom_inc_rw_2.kattr, &sysdbg_v2_scratch_rw_00.kattr,
	&sysdbg_v2_scratch_rw_01.kattr, &sysdbg_v2_scratch_rw_02.kattr,
	&sysdbg_v2_scratch_rw_03.kattr, &sysdbg_v2_scratch_rw_04.kattr,
	&sysdbg_v2_scratch_rw_05.kattr, &sysdbg_v2_scratch_rw_06.kattr,
	&sysdbg_v2_scratch_rw_07.kattr, &sysdbg_v2_scratch_rw_08.kattr,
	&sysdbg_v2_scratch_rw_09.kattr, &sysdbg_v2_scratch_rw_10.kattr,
	&sysdbg_v2_scratch_rw_11.kattr, &sysdbg_v2_scratch_rw_12.kattr,
	&sysdbg_v2_scratch_rw_13.kattr, &sysdbg_v2_scratch_rw_14.kattr,
	&sysdbg_v2_scratch_rw_15.kattr, NULL,
};
ATTRIBUTE_GROUPS(sysdbg_v2_reg);

static struct kobj_type sysdbg_v2_reg_ktype = {
	.sysfs_ops = &sysdbg_reg_ops,
	.release = sysdbg_reg_release,
	.default_groups = sysdbg_v2_reg_groups,
};

static struct attribute *sysdbg_v3_reg_attrs[] = {
	&sysdbg_ts32_rw_0.kattr,
	&sysdbg_ts64_ro_0.kattr,
	&sysdbg_atom_inc_ro_0.kattr,
	&sysdbg_atom_inc_ro_1.kattr,
	&sysdbg_atom_inc_ro_2.kattr,
	&sysdbg_atom_inc_ro_3.kattr,
	&sysdbg_atom_inc_ro_4.kattr,
	&sysdbg_atom_inc_rw_0.kattr,
	&sysdbg_atom_inc_rw_1.kattr,
	&sysdbg_atom_inc_rw_2.kattr,
	&sysdbg_atom_inc_rw_3.kattr,
	&sysdbg_atom_inc_rw_4.kattr,
	&sysdbg_scratch_rw_00.kattr,
	&sysdbg_scratch_rw_01.kattr,
	&sysdbg_scratch_rw_02.kattr,
	&sysdbg_scratch_rw_03.kattr,
	&sysdbg_scratch_rw_04.kattr,
	&sysdbg_scratch_rw_05.kattr,
	&sysdbg_scratch_rw_06.kattr,
	&sysdbg_scratch_rw_07.kattr,
	&sysdbg_scratch_rw_08.kattr,
	&sysdbg_scratch_rw_09.kattr,
	&sysdbg_scratch_rw_10.kattr,
	&sysdbg_scratch_rw_11.kattr,
	&sysdbg_scratch_rw_12.kattr,
	&sysdbg_scratch_rw_13.kattr,
	&sysdbg_scratch_rw_14.kattr,
	&sysdbg_scratch_rw_15.kattr,
	&sysdbg_scratch_rw_16.kattr,
	&sysdbg_scratch_rw_17.kattr,
	&sysdbg_scratch_rw_18.kattr,
	&sysdbg_scratch_rw_19.kattr,
	&sysdbg_swpc_val_lw_rw_00.kattr,
	&sysdbg_swpc_val_hw_rw_00.kattr,
	&sysdbg_swpc_evt_rw_00.kattr,
	&sysdbg_swpc_ts_ro_00.kattr,
	&sysdbg_swpc_val_lw_rw_01.kattr,
	&sysdbg_swpc_val_hw_rw_01.kattr,
	&sysdbg_swpc_evt_rw_01.kattr,
	&sysdbg_swpc_ts_ro_01.kattr,
	&sysdbg_swpc_val_lw_rw_02.kattr,
	&sysdbg_swpc_val_hw_rw_02.kattr,
	&sysdbg_swpc_evt_rw_02.kattr,
	&sysdbg_swpc_ts_ro_02.kattr,
	&sysdbg_swpc_val_lw_rw_03.kattr,
	&sysdbg_swpc_val_hw_rw_03.kattr,
	&sysdbg_swpc_evt_rw_03.kattr,
	&sysdbg_swpc_ts_ro_03.kattr,
	&sysdbg_swpc_val_lw_rw_04.kattr,
	&sysdbg_swpc_val_hw_rw_04.kattr,
	&sysdbg_swpc_evt_rw_04.kattr,
	&sysdbg_swpc_ts_ro_04.kattr,
	&sysdbg_swpc_val_lw_rw_05.kattr,
	&sysdbg_swpc_val_hw_rw_05.kattr,
	&sysdbg_swpc_evt_rw_05.kattr,
	&sysdbg_swpc_ts_ro_05.kattr,
	&sysdbg_swpc_val_lw_rw_06.kattr,
	&sysdbg_swpc_val_hw_rw_06.kattr,
	&sysdbg_swpc_evt_rw_06.kattr,
	&sysdbg_swpc_ts_ro_06.kattr,
	&sysdbg_swpc_val_lw_rw_07.kattr,
	&sysdbg_swpc_val_hw_rw_07.kattr,
	&sysdbg_swpc_evt_rw_07.kattr,
	&sysdbg_swpc_ts_ro_07.kattr,
	&sysdbg_swpc_val_lw_rw_08.kattr,
	&sysdbg_swpc_val_hw_rw_08.kattr,
	&sysdbg_swpc_evt_rw_08.kattr,
	&sysdbg_swpc_ts_ro_08.kattr,
	&sysdbg_swpc_val_lw_rw_09.kattr,
	&sysdbg_swpc_val_hw_rw_09.kattr,
	&sysdbg_swpc_evt_rw_09.kattr,
	&sysdbg_swpc_ts_ro_09.kattr,
	&sysdbg_swpc_val_lw_rw_10.kattr,
	&sysdbg_swpc_val_hw_rw_10.kattr,
	&sysdbg_swpc_evt_rw_10.kattr,
	&sysdbg_swpc_ts_ro_10.kattr,
	&sysdbg_swpc_val_lw_rw_11.kattr,
	&sysdbg_swpc_val_hw_rw_11.kattr,
	&sysdbg_swpc_evt_rw_11.kattr,
	&sysdbg_swpc_ts_ro_11.kattr,
	&sysdbg_swpc_val_lw_rw_12.kattr,
	&sysdbg_swpc_val_hw_rw_12.kattr,
	&sysdbg_swpc_evt_rw_12.kattr,
	&sysdbg_swpc_ts_ro_12.kattr,
	&sysdbg_swpc_val_lw_rw_13.kattr,
	&sysdbg_swpc_val_hw_rw_13.kattr,
	&sysdbg_swpc_evt_rw_13.kattr,
	&sysdbg_swpc_ts_ro_13.kattr,
	&sysdbg_swpc_val_lw_rw_14.kattr,
	&sysdbg_swpc_val_hw_rw_14.kattr,
	&sysdbg_swpc_evt_rw_14.kattr,
	&sysdbg_swpc_ts_ro_14.kattr,
	&sysdbg_swpc_val_lw_rw_15.kattr,
	&sysdbg_swpc_val_hw_rw_15.kattr,
	&sysdbg_swpc_evt_rw_15.kattr,
	&sysdbg_swpc_ts_ro_15.kattr,
	NULL,
};
ATTRIBUTE_GROUPS(sysdbg_v3_reg);

static struct kobj_type sysdbg_v3_reg_ktype = {
	.sysfs_ops = &sysdbg_reg_ops,
	.release = sysdbg_reg_release,
	.default_groups = sysdbg_v3_reg_groups,
};

int sysdbg_sysfs_create(unsigned int hw_ver)
{
	int retval;

	/* Creating a kobject of /sys/kernel/sysdbg */
	retval = kobject_init_and_add(&sysdbg_kobj, &all_tests_ktype,
				      kernel_kobj, "%s", "sysdbg");
	if (retval) {
		kobject_put(&sysdbg_kobj);
		retval = -ENOMEM;
		goto exit;
	}

	switch (hw_ver) {
	case 1:
		retval = kobject_init_and_add(&sysdbg_reg_kobj,
					      &sysdbg_v1_reg_ktype,
					      &sysdbg_kobj, "%s", "regs");
		break;
	case 2:
		retval = kobject_init_and_add(&sysdbg_reg_kobj,
					      &sysdbg_v2_reg_ktype,
					      &sysdbg_kobj, "%s", "regs");
		break;
	case 3:
		retval = kobject_init_and_add(&sysdbg_reg_kobj,
					      &sysdbg_v3_reg_ktype,
					      &sysdbg_kobj, "%s", "regs");
		break;
	default:
		retval = -ENODEV;
	}

	if (retval) {
		kobject_put(&sysdbg_reg_kobj);
		retval = -ENOMEM;
	}

exit:
	return retval;
}

void sysdbg_sysfs_destroy(void)
{
	kobject_del(&sysdbg_reg_kobj);
	kobject_put(&sysdbg_reg_kobj);
	kobject_del(&sysdbg_kobj);
	kobject_put(&sysdbg_kobj);
}
