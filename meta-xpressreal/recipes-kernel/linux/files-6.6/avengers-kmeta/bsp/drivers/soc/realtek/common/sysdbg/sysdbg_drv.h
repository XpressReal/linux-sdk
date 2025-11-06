/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#ifndef SYSDBG_DRV_H
#define SYSDBG_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

struct addr_map {
	uint32_t base;
	uint32_t size;
};

unsigned int sysdbg_get_hw_ver(void);
unsigned int sysdbg_get_dbg(void);
uint32_t sysdbg_get_map_base(uint8_t index);
void sysdbg_ts_on_off(uint8_t index, bool enable);
void sysdbg_set_clk_div(uint8_t index, uint16_t div_n);
uint32_t sysdbg_ts32_read(uint8_t index);
void sysdbg_ts32_write(uint8_t index, uint32_t value);
uint64_t sysdbg_ts64_read(uint8_t index);
void sysdbg_ts64_write(uint8_t index, uint64_t value);
uint32_t sysdbg_atom_inc_ro_read(uint8_t index, uint8_t offset);
void sysdbg_atom_inc_ro_write(uint8_t index, uint8_t offset, uint32_t value);
uint32_t sysdbg_atom_inc_rw_read(uint8_t index, uint8_t offset);
void sysdbg_atom_inc_rw_write(uint8_t index, uint8_t offset, uint32_t value);
uint32_t sysdbg_scratch_read(uint8_t index, uint8_t offset);
void sysdbg_scratch_write(uint8_t index, uint8_t offset, uint32_t value);
uint32_t sysdbg_ts64_cmp_ctrl_read(uint8_t index);
void sysdbg_ts64_cmp_ctrl_write(uint8_t index, bool wr_en0, bool cmp_en,
				bool wr_en1, bool scpu_en, bool wr_en2,
				bool pcpu_en);
uint64_t sysdbg_ts64_cmp_value_read(uint8_t index);
void sysdbg_ts64_cmp_value_write(uint8_t index, uint64_t value);
uint32_t sysdbg_swpc_evt_read(uint8_t index, uint8_t offset);
void sysdbg_swpc_evt_write(uint8_t index, uint8_t offset, uint32_t value);
uint32_t sysdbg_swpc_val_hw_read(uint8_t index, uint8_t offset);
void sysdbg_swpc_val_hw_write(uint8_t index, uint8_t offset, uint32_t value);
uint32_t sysdbg_swpc_val_lw_read(uint8_t index, uint8_t offset);
void sysdbg_swpc_val_lw_write(uint8_t index, uint8_t offset, uint32_t value);
uint32_t sysdbg_swpc_ts32_read(uint8_t index, uint8_t offset);
uint32_t sysdbg_reg_read(int map_idx, int offset);
void sysdbg_reg_write(int map_idx, int offset, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* SYSDBG_DRV_H */
