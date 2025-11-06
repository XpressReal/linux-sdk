// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 Realtek Semiconductor Corporation
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/rtd1625-clk.h>
#include <dt-bindings/reset/rtd1625-reset.h>
#include "common.h"
#include "clk-pll.h"

#define RTD1625_REG_PLL_ACPU1               0x10c
#define RTD1625_REG_PLL_ACPU2               0x110
#define RTD1625_REG_PLL_SSC_DIG_ACPU0       0x5c0
#define RTD1625_REG_PLL_SSC_DIG_ACPU1       0x5c4
#define RTD1625_REG_PLL_SSC_DIG_ACPU2       0x5c8
#define RTD1625_REG_PLL_SSC_DIG_ACPU_DBG2   0x5dc

#define RTD1625_REG_PLL_VE1_1               0x114
#define RTD1625_REG_PLL_VE1_2               0x118
#define RTD1625_REG_PLL_SSC_DIG_VE1_0       0x584
#define RTD1625_REG_PLL_SSC_DIG_VE1_1       0x588
#define RTD1625_REG_PLL_SSC_DIG_VE1_2       0x58c
#define RTD1625_REG_PLL_SSC_DIG_VE1_DBG2    0x59c

#define RTD1625_REG_PLL_GPU1                0x1c0
#define RTD1625_REG_PLL_GPU2                0x1c4
#define RTD1625_REG_PLL_SSC_DIG_GPU0        0x5a0
#define RTD1625_REG_PLL_SSC_DIG_GPU1        0x5a4
#define RTD1625_REG_PLL_SSC_DIG_GPU2        0x5a8
#define RTD1625_REG_PLL_SSC_DIG_GPU_DBG2    0x5bc

#define RTD1625_REG_PLL_NPU1                0x1c8
#define RTD1625_REG_PLL_NPU2                0x1cc
#define RTD1625_REG_PLL_SSC_DIG_NPU0        0x800
#define RTD1625_REG_PLL_SSC_DIG_NPU1        0x804
#define RTD1625_REG_PLL_SSC_DIG_NPU2        0x808
#define RTD1625_REG_PLL_SSC_DIG_NPU_DBG2    0x81c

#define RTD1625_REG_PLL_VE2_1               0x1d0
#define RTD1625_REG_PLL_VE2_2               0x1d4
#define RTD1625_REG_PLL_SSC_DIG_VE2_0       0x5e0
#define RTD1625_REG_PLL_SSC_DIG_VE2_1       0x5e4
#define RTD1625_REG_PLL_SSC_DIG_VE2_2       0x5e8
#define RTD1625_REG_PLL_SSC_DIG_VE2_DBG2    0x5fc

#define RTD1625_REG_PLL_HIFI1               0x1d8
#define RTD1625_REG_PLL_HIFI2               0x1dc
#define RTD1625_REG_PLL_SSC_DIG_HIFI0       0x6e0
#define RTD1625_REG_PLL_SSC_DIG_HIFI1       0x6e4
#define RTD1625_REG_PLL_SSC_DIG_HIFI2       0x6e8
#define RTD1625_REG_PLL_SSC_DIG_HIFI_DBG2   0x6fc

static const char * const clk_gpu_parents[] = { "pll_gpu", "clk_sys" };
static CLK_REGMAP_MUX(clk_gpu, clk_gpu_parents, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, 0x28, 12, 0x1);
static const char * const clk_ve_parents[] = { "pll_vo", "clk_sysh", "pll_ve1", "pll_ve2" };
static CLK_REGMAP_MUX(clk_ve1, clk_ve_parents, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, 0x4c, 0, 0x3);
static CLK_REGMAP_MUX(clk_ve2, clk_ve_parents, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, 0x4c, 3, 0x3);
static CLK_REGMAP_MUX(clk_ve4, clk_ve_parents, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, 0x4c, 6, 0x3);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_misc, CLK_IS_CRITICAL, 0x050, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_pcie0, 0, 0x050, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_gspi, 0, 0x050, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_iso_misc, CLK_IGNORE_UNUSED, 0x050, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sds, 0, 0x050, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_hdmi, CLK_IGNORE_UNUSED, 0x050, 14, 1);
static CLK_REGMAP_GATE(clk_en_gpu, "clk_gpu", CLK_SET_RATE_PARENT, 0x050, 18, 1);
static CLK_REGMAP_GATE(clk_en_ve1, "clk_ve1", CLK_SET_RATE_PARENT, 0x050, 20, 1);
static CLK_REGMAP_GATE(clk_en_ve2, "clk_ve2", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x050, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lsadc, 0, 0x050, 28, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_se, 0, 0x050, 30, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_dcu, 0, 0x054, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_cp, 0, 0x054, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_md, 0, 0x054, 4, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tp, CLK_IS_CRITICAL, 0x054, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_rcic, 0, 0x054, 8, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_nf, 0, 0x054, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_emmc, 0, 0x054, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sd, 0, 0x054, 14, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sdio_ip, 0, 0x054, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mipi_csi, 0, 0x054, 18, 1);
static CLK_REGMAP_GATE(clk_en_emmc_ip, "pll_emmc", CLK_SET_RATE_PARENT, 0x054, 20, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sdio, 0, 0x054, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sd_ip, 0, 0x054, 24, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tpb, 0, 0x054, 28, 1);
static CLK_REGMAP_GATE(clk_en_misc_sc1, "clk_en_misc", CLK_SET_RATE_PARENT, 0x054, 30, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_3, "clk_en_misc", CLK_SET_RATE_PARENT, 0x058, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_jpeg, 0, 0x058, 4, 1);
static CLK_REGMAP_GATE(clk_en_acpu, "pll_acpu", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x058, 6, 1);
static CLK_REGMAP_GATE(clk_en_misc_sc0, "clk_en_misc", CLK_SET_RATE_PARENT, 0x058, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_hdmirx, 0, 0x058, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_hse, CLK_IS_CRITICAL, 0x058, 28, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_fan, 0, 0x05c, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sata_wrap_sys, 0, 0x05c, 8, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sata_wrap_sysh, 0, 0x05c, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sata_mac_sysh, 0, 0x05c, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_r2rdsc, 0, 0x05c, 14, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tpc, 0, 0x05c, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_pcie1, 0, 0x05c, 18, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_4, "clk_en_misc", CLK_SET_RATE_PARENT, 0x05c, 20, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_5, "clk_en_misc", CLK_SET_RATE_PARENT, 0x05c, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tsio, 0, 0x05c, 24, 1);
static CLK_REGMAP_GATE(clk_en_ve4, "clk_ve4", CLK_SET_RATE_PARENT, 0x05c, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_edp, 0, 0x05c, 28, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tsio_trx, 0, 0x05c, 30, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_pcie2, 0, 0x08c, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_iso_gspi, 0, 0x08c, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_earc, 0, 0x08c, 4, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lite, 0, 0x08c, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mipi_dsi, CLK_IGNORE_UNUSED, 0x08c, 8, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_npupp, 0, 0x08c, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_npu, 0, 0x08c, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu0, CLK_IGNORE_UNUSED, 0x08c, 14, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu1, CLK_IGNORE_UNUSED, 0x08c, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_nsram, 0, 0x08c, 18, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_hdmitop, CLK_IGNORE_UNUSED, 0x08c, 20, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu_sram, CLK_IGNORE_UNUSED, 0x08c, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu_iso_npu, CLK_IGNORE_UNUSED, 0x08c, 24, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_keyladder, 0, 0x08c, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_ifcp_klm, 0, 0x08c, 28, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_ifcp, 0, 0x08c, 30, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdl_genpw, 0, 0x0b0, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdl_chip, 0, 0x0b0, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdl_ip, 0, 0x0b0, 4, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdlm2m, 0, 0x0b0, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdl_xtal, 0, 0x0b0, 8, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_test_mux, 0, 0x0b0, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_dla, 0, 0x0b0, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tpcw, CLK_IGNORE_UNUSED, 0x0b0, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_gpu_ts_src, CLK_IGNORE_UNUSED, 0x0b0, 18, 1);
static CLK_REGMAP_GATE_NO_PARENT_RO(clk_en_gpu2d, CLK_IGNORE_UNUSED, 0x0b0, 20, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_vi, 0, 0x0b0, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lvds1, 0, 0x0b0, 24, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lvds2, 0, 0x0b0, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu, CLK_IGNORE_UNUSED, 0x0b0, 28, 1);
static CLK_REGMAP_GATE(clk_en_ur1, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 0, 1);
static CLK_REGMAP_GATE(clk_en_ur2, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 2, 1);
static CLK_REGMAP_GATE(clk_en_ur3, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 4, 1);
static CLK_REGMAP_GATE(clk_en_ur4, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 6, 1);
static CLK_REGMAP_GATE(clk_en_ur5, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 8, 1);
static CLK_REGMAP_GATE(clk_en_ur6, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 10, 1);
static CLK_REGMAP_GATE(clk_en_ur7, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 12, 1);
static CLK_REGMAP_GATE(clk_en_ur8, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 14, 1);
static CLK_REGMAP_GATE(clk_en_ur9, "clk_en_ur_top", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x884, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_ur_top, CLK_IS_CRITICAL, 0x884, 18, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_7, "clk_en_misc", CLK_SET_RATE_PARENT, 0x884, 28, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_6, "clk_en_misc", CLK_SET_RATE_PARENT, 0x884, 30, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_spi0, 0, 0x894, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_spi1, 0, 0x894, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_spi2, 0, 0x894, 4, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lsadc0, 0, 0x894, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lsadc1, 0, 0x894, 18, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_isomis_dma, 0, 0x894, 20, 1);
static CLK_REGMAP_GATE_NO_PARENT_RO(clk_en_audio_adc, CLK_IGNORE_UNUSED, 0x894, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_dptx, 0, 0x894, 24, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_npu_mipi_csi, 0, 0x894, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_edptx, 0, 0x894, 28, 1);

static const char * const clk_det_outputs[] = {
	[0x00] = "clk_sys_ungate",
	[0x01] = "pll_bus",
	[0x02] = "clk_sysh_ungate",
	[0x03] = "pll_dcsb",
	[0x04] = "ck2x_psaud1a_mux",
	[0x05] = "ck2x_psaud1a",
	[0x06] = "ck2x_psaud2a_mux",
	[0x07] = "ck2x_psaud2a",
	[0x08] = "clk_acpu_ungate",
	[0x09] = "pll_acpu",
	[0x0a] = "clk_aucpu_ungate_main",
	[0x0b] = "pll_hifi",
	[0x0c] = "clk_aucpu_ungate_scpu",
	[0x0d] = "pll_gpu",
	[0x0e] = "clk_gpu",
	[0x0f] = "pll_ddsa",
	[0x10] = "clk_hfosc_rng",
	[0x11] = "pll_ddsa_mux",
	[0x12] = "clk_npu",
	[0x13] = "pll_npu",
	[0x14] = "clk_npu_axi",
	[0x15] = "pll_vodma",
	[0x16] = "clk_ve1",
	[0x17] = "pll_ve1",
	[0x18] = "clk_ve2",
	[0x19] = "pll_ve2",
	[0x1a] = "clk_ve4",
	[0x1b] = "clk_trng",
	[0x1c] = "clk_trng_main",
	[0x1d] = "clk_scpu_trc",
	[0x1e] = "clk_gpu2d",
	[0x1f] = "clk_aucpu_ungate_iso",
	[0x20] = "clk_mipi_dsi_ungate",
	[0x21] = "clk_npu_mipi_csi",
	[0x22] = "clk_dptx_pxl_gated",
};

static struct clk_regmap_clkdet clk_det = {
	.clkr.hw.init = CLK_HW_INIT_NO_PARENT("ref_clk", &clk_regmap_clkdet_ops, CLK_GET_RATE_NOCACHE),
	.type = CLK_DET_TYPE_CRT,
	.ofs  = 0x424,
	.output_sel = clk_det_outputs,
	.n_output_sel = ARRAY_SIZE(clk_det_outputs),
	.mask_output_sel = GENMASK(5, 0),
	.reg_output_sel = 0x428,
};

#define FREQ_NF_MASK       (0x7FFFF)
#define FREQ_NF(_r, _nf)   { .rate = _r, .val = (_nf), }

static const struct freq_table acpu_tbl[] = {
	FREQ_NF(513000000, 0x00011000),
	FREQ_TABLE_END
};

static const struct freq_table ve_tbl[] = {
	FREQ_NF(553500000, 0x00012800),
	FREQ_NF(661500000, 0x00016800),
	FREQ_NF(688500000, 0x00017800),
	FREQ_TABLE_END
};

static const struct freq_table bus_tbl[] = {
	FREQ_NF(513000000, 0x00011000),
	FREQ_NF(540000000, 0x00012000),
	FREQ_NF(553500000, 0x00012800),
	FREQ_TABLE_END
};

static const struct freq_table ddsa_tbl[] = {
	FREQ_NF(432000000, 0x0000e000),
	FREQ_TABLE_END
};

static const struct freq_table gpu_tbl[] = {
	FREQ_NF(405000000, 0x0000d000),
	FREQ_NF(540000000, 0x00012000),
	FREQ_NF(661500000, 0x00016800),
	FREQ_NF(729000000, 0x00019000),
	FREQ_NF(810000000, 0x0001c000),
	FREQ_NF(850500000, 0x0001d800),
	FREQ_TABLE_END
};

static const struct freq_table hifi_tbl[] = {
	FREQ_NF(756000000, 0x0001a000),
	FREQ_NF(810000000, 0x0001c000),
	FREQ_TABLE_END
};

static const struct freq_table npu_tbl[] = {
	FREQ_NF(661500000, 0x00016800),
	FREQ_NF(729000000, 0x00019000),
	FREQ_NF(810000000, 0x0001c000),
	FREQ_TABLE_END
};

static struct reg_sequence pll_acpu_seq_power_on[] = {
	{RTD1625_REG_PLL_ACPU2,         0x00000005},
	{RTD1625_REG_PLL_ACPU2,         0x00000007},
	{RTD1625_REG_PLL_ACPU1,         0x00054000},
	{RTD1625_REG_PLL_SSC_DIG_ACPU2, 0x001e1f8e},
	{RTD1625_REG_PLL_SSC_DIG_ACPU0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_ACPU1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_ACPU0, 0x00000005, 200},
	{RTD1625_REG_PLL_ACPU2,         0x00000003},
};

static struct reg_sequence pll_acpu_seq_power_off[] = {
	{RTD1625_REG_PLL_ACPU2,         0x00000004},
};

static struct reg_sequence pll_acpu_seq_set_freq[] = {
	{RTD1625_REG_PLL_SSC_DIG_ACPU0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_ACPU1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_ACPU0, 0x00000005},
};

static struct clk_pll2 pll_acpu = {
	.clkr.hw.init = CLK_HW_INIT("pll_acpu", "osc27m", &clk_pll2_ops,
				    CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	.seq_power_on = pll_acpu_seq_power_on,
	.num_seq_power_on = ARRAY_SIZE(pll_acpu_seq_power_on),
	.seq_power_off = pll_acpu_seq_power_off,
	.num_seq_power_off = ARRAY_SIZE(pll_acpu_seq_power_off),
	.seq_set_freq = pll_acpu_seq_set_freq,
	.num_seq_set_freq = ARRAY_SIZE(pll_acpu_seq_set_freq),
	.freq_reg = RTD1625_REG_PLL_SSC_DIG_ACPU1,
	.freq_tbl  = acpu_tbl,
	.freq_mask = 0x7FFFF,
	.freq_ready_reg = RTD1625_REG_PLL_SSC_DIG_ACPU_DBG2,
	.freq_ready_mask = BIT(20),
	.freq_ready_val = BIT(20),
	.power_reg = RTD1625_REG_PLL_ACPU2,
	.power_mask = 0x7,
	.power_val_on = 0x3,
};

static struct reg_sequence pll_ve1_seq_power_on[] = {
	{RTD1625_REG_PLL_VE1_2,         0x00000005},
	{RTD1625_REG_PLL_VE1_2,         0x00000007},
	{RTD1625_REG_PLL_VE1_1,         0x00054000},
	{RTD1625_REG_PLL_SSC_DIG_VE1_0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_VE1_1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_VE1_0, 0x00000005, 200},
	{RTD1625_REG_PLL_VE1_2,         0x00000003},
};

static struct reg_sequence pll_ve1_seq_power_off[] = {
	{RTD1625_REG_PLL_VE1_2,         0x00000004},
};

static struct reg_sequence pll_ve1_seq_set_freq[] = {
	{RTD1625_REG_PLL_SSC_DIG_VE1_0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_VE1_1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_VE1_0, 0x00000005},
};

static struct clk_pll2 pll_ve1 = {
	.clkr.hw.init = CLK_HW_INIT("pll_ve1", "osc27m", &clk_pll2_ops,
				    CLK_GET_RATE_NOCACHE),
	.seq_power_on = pll_ve1_seq_power_on,
	.num_seq_power_on = ARRAY_SIZE(pll_ve1_seq_power_on),
	.seq_power_off = pll_ve1_seq_power_off,
	.num_seq_power_off = ARRAY_SIZE(pll_ve1_seq_power_off),
	.seq_set_freq = pll_ve1_seq_set_freq,
	.num_seq_set_freq = ARRAY_SIZE(pll_ve1_seq_set_freq),
	.freq_reg = RTD1625_REG_PLL_SSC_DIG_VE1_1,
	.freq_tbl  = ve_tbl,
	.freq_mask = 0x7ffff,
	.freq_ready_reg = RTD1625_REG_PLL_SSC_DIG_VE1_DBG2,
	.freq_ready_mask = BIT(20),
	.freq_ready_val = BIT(20),
	.power_reg = RTD1625_REG_PLL_VE1_2,
	.power_mask = 0x7,
	.power_val_on = 0x3,
};

static struct clk_pll pll_ddsa = {
	.pll_ofs   = 0x120,
	.ssc_ofs   = 0x560,
	.pll_type  = CLK_PLL_TYPE_NF_SSC,
	.freq_tbl  = ddsa_tbl,
	.freq_mask = FREQ_NF_MASK,
	.clkr.hw.init = CLK_HW_INIT("pll_ddsa", "osc27m", &clk_pll_ro_ops, CLK_GET_RATE_NOCACHE),
};

static struct clk_pll pll_bus = {
	.pll_ofs   = CLK_OFS_INVALID,
	.ssc_ofs   = 0x520,
	.pll_type  = CLK_PLL_TYPE_NF_SSC,
	.freq_tbl  = bus_tbl,
	.freq_mask = FREQ_NF_MASK,
	.clkr.hw.init = CLK_HW_INIT("pll_bus", "osc27m", &clk_pll_ro_ops, CLK_GET_RATE_NOCACHE),
};

static CLK_FIXED_FACTOR(clk_sys, "clk_sys", "pll_bus", 2, 1, 0);

static struct clk_pll pll_dcsb = {
	.pll_ofs   = CLK_OFS_INVALID,
	.ssc_ofs   = 0x540,
	.pll_type  = CLK_PLL_TYPE_NF_SSC,
	.freq_tbl  = bus_tbl,
	.freq_mask = FREQ_NF_MASK,
	.clkr.hw.init = CLK_HW_INIT("pll_dcsb", "osc27m", &clk_pll_ro_ops, CLK_GET_RATE_NOCACHE),
};

static CLK_FIXED_FACTOR(clk_sysh, "clk_sysh", "pll_dcsb", 1, 1, 0);

static struct reg_sequence pll_gpu_seq_power_on[] = {
	{RTD1625_REG_PLL_GPU2,         0x00000005},
	{RTD1625_REG_PLL_GPU2,         0x00000007},
	{RTD1625_REG_PLL_GPU1,         0x00054000},
	{RTD1625_REG_PLL_SSC_DIG_GPU0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_GPU1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_GPU0, 0x00000005, 200},
	{RTD1625_REG_PLL_GPU2,         0x00000003},
};

static struct reg_sequence pll_gpu_seq_power_off[] = {
	{RTD1625_REG_PLL_GPU2,         0x00000004},
};

static struct reg_sequence pll_gpu_seq_set_freq[] = {
	{RTD1625_REG_PLL_SSC_DIG_GPU0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_GPU1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_GPU0, 0x00000005},
};

static struct clk_pll2 pll_gpu = {
	.clkr.hw.init = CLK_HW_INIT("pll_gpu", "osc27m", &clk_pll2_ops,
				    CLK_GET_RATE_NOCACHE),
	.seq_power_on = pll_gpu_seq_power_on,
	.num_seq_power_on = ARRAY_SIZE(pll_gpu_seq_power_on),
	.seq_power_off = pll_gpu_seq_power_off,
	.num_seq_power_off = ARRAY_SIZE(pll_gpu_seq_power_off),
	.seq_set_freq = pll_gpu_seq_set_freq,
	.num_seq_set_freq = ARRAY_SIZE(pll_gpu_seq_set_freq),
	.freq_reg = RTD1625_REG_PLL_SSC_DIG_GPU1,
	.freq_tbl  = gpu_tbl,
	.freq_mask = 0x7ffff,
	.freq_ready_reg = RTD1625_REG_PLL_SSC_DIG_GPU_DBG2,
	.freq_ready_mask = BIT(20),
	.freq_ready_val = BIT(20),
	.power_reg = RTD1625_REG_PLL_GPU2,
	.power_mask = 0x7,
	.power_val_on = 0x3,
};

static struct reg_sequence pll_npu_seq_power_on[] = {
	{RTD1625_REG_PLL_NPU2,         0x00000005},
	{RTD1625_REG_PLL_NPU2,         0x00000007},
	{RTD1625_REG_PLL_NPU1,         0x00054000},
	{RTD1625_REG_PLL_SSC_DIG_NPU0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_NPU1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_NPU0, 0x00000005, 200},
	{RTD1625_REG_PLL_NPU2,         0x00000003},
};

static struct reg_sequence pll_npu_seq_power_off[] = {
	{RTD1625_REG_PLL_NPU2,         0x00000004},
	{RTD1625_REG_PLL_NPU1,         0x00054010},
};

static struct reg_sequence pll_npu_seq_set_freq[] = {
	{RTD1625_REG_PLL_SSC_DIG_NPU0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_NPU1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_NPU0, 0x00000005},
};

static struct clk_pll2 pll_npu = {
	.clkr.hw.init = CLK_HW_INIT("pll_npu", "osc27m", &clk_pll2_ops,
				    CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	.seq_power_on = pll_npu_seq_power_on,
	.num_seq_power_on = ARRAY_SIZE(pll_npu_seq_power_on),
	.seq_power_off = pll_npu_seq_power_off,
	.num_seq_power_off = ARRAY_SIZE(pll_npu_seq_power_off),
	.seq_set_freq = pll_npu_seq_set_freq,
	.num_seq_set_freq = ARRAY_SIZE(pll_npu_seq_set_freq),
	.freq_reg = RTD1625_REG_PLL_SSC_DIG_NPU1,
	.freq_tbl  = npu_tbl,
	.freq_mask = 0x7ffff,
	.freq_ready_reg = RTD1625_REG_PLL_SSC_DIG_NPU_DBG2,
	.freq_ready_mask = BIT(20),
	.freq_ready_val = BIT(20),
	.power_reg = RTD1625_REG_PLL_NPU2,
	.power_mask = 0x7,
	.power_val_on = 0x3,
};

static CLK_FIXED_FACTOR(clk_npu, "clk_npu", "pll_npu", 1, 1, CLK_SET_RATE_PARENT);
static CLK_FIXED_FACTOR(clk_npu_mipi_csi, "clk_npu_mipi_csi", "pll_npu", 1, 1, CLK_SET_RATE_PARENT);

static struct reg_sequence pll_ve2_seq_power_on[] = {
	{RTD1625_REG_PLL_VE2_2,         0x00000005},
	{RTD1625_REG_PLL_VE2_2,         0x00000007},
	{RTD1625_REG_PLL_VE2_1,         0x00054000},
	{RTD1625_REG_PLL_SSC_DIG_VE2_0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_VE2_1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_VE2_0, 0x00000005, 200},
	{RTD1625_REG_PLL_VE2_2,         0x00000003},
};

static struct reg_sequence pll_ve2_seq_power_off[] = {
	{RTD1625_REG_PLL_VE2_2,         0x00000004},
};

static struct reg_sequence pll_ve2_seq_set_freq[] = {
	{RTD1625_REG_PLL_SSC_DIG_VE2_0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_VE2_1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_VE2_0, 0x00000005},
};

static struct clk_pll2 pll_ve2 = {
	.clkr.hw.init = CLK_HW_INIT("pll_ve2", "osc27m", &clk_pll2_ops,
				    CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	.seq_power_on = pll_ve2_seq_power_on,
	.num_seq_power_on = ARRAY_SIZE(pll_ve2_seq_power_on),
	.seq_power_off = pll_ve2_seq_power_off,
	.num_seq_power_off = ARRAY_SIZE(pll_ve2_seq_power_off),
	.seq_set_freq = pll_ve2_seq_set_freq,
	.num_seq_set_freq = ARRAY_SIZE(pll_ve2_seq_set_freq),
	.freq_reg = RTD1625_REG_PLL_SSC_DIG_VE2_1,
	.freq_tbl  = ve_tbl,
	.freq_mask = 0x7ffff,
	.freq_ready_reg = RTD1625_REG_PLL_SSC_DIG_VE2_DBG2,
	.freq_ready_mask = BIT(20),
	.freq_ready_val = BIT(20),
	.power_reg = RTD1625_REG_PLL_VE2_2,
	.power_mask = 0x7,
	.power_val_on = 0x3,
};

static struct reg_sequence pll_hifi_seq_power_on[] = {
	{RTD1625_REG_PLL_HIFI2,         0x00000005},
	{RTD1625_REG_PLL_HIFI2,         0x00000007},
	{RTD1625_REG_PLL_HIFI1,         0x00054000},
	{RTD1625_REG_PLL_SSC_DIG_HIFI0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_HIFI1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_HIFI0, 0x00000005, 200},
	{RTD1625_REG_PLL_HIFI2,         0x00000003},
};

static struct reg_sequence pll_hifi_seq_power_off[] = {
	{RTD1625_REG_PLL_HIFI2,         0x00000004},
};

static struct reg_sequence pll_hifi_seq_set_freq[] = {
	{RTD1625_REG_PLL_SSC_DIG_HIFI0, 0x00000004},
	{RTD1625_REG_PLL_SSC_DIG_HIFI1, 0x00000000},
	{RTD1625_REG_PLL_SSC_DIG_HIFI0, 0x00000005},
};

static struct clk_pll2 pll_hifi = {
	.clkr.hw.init = CLK_HW_INIT("pll_hifi", "osc27m", &clk_pll2_ops,
				    CLK_GET_RATE_NOCACHE | CLK_IGNORE_UNUSED),
	.seq_power_on = pll_hifi_seq_power_on,
	.num_seq_power_on = ARRAY_SIZE(pll_hifi_seq_power_on),
	.seq_power_off = pll_hifi_seq_power_off,
	.num_seq_power_off = ARRAY_SIZE(pll_hifi_seq_power_off),
	.seq_set_freq = pll_hifi_seq_set_freq,
	.num_seq_set_freq = ARRAY_SIZE(pll_hifi_seq_set_freq),
	.freq_reg = RTD1625_REG_PLL_SSC_DIG_HIFI1,
	.freq_tbl  = hifi_tbl,
	.freq_mask = 0x7ffff,
	.freq_ready_reg = RTD1625_REG_PLL_SSC_DIG_HIFI_DBG2,
	.freq_ready_mask = BIT(20),
	.freq_ready_val = BIT(20),
	.power_reg = RTD1625_REG_PLL_HIFI2,
	.power_mask = 0x7,
	.power_val_on = 0x3,
};

static CLK_FIXED_FACTOR(pll_emmc_ref, "pll_emmc_ref", "osc27m", 1, 1, 0);

static struct clk_pll_mmc pll_emmc = {
	.pll_ofs = 0x1f0,
	.ssc_dig_ofs = 0x6b0,
	.clkr.hw.init = CLK_HW_INIT("pll_emmc", "pll_emmc_ref", &clk_pll_mmc_v2_ops, 0),
	.phase0_hw.init = CLK_HW_INIT("pll_emmc_vp0", "pll_emmc", &clk_pll_mmc_phase_ops, 0),
	.phase1_hw.init = CLK_HW_INIT("pll_emmc_vp1", "pll_emmc", &clk_pll_mmc_phase_ops, 0),
};

static struct clk_regmap *rtd1625_cc_regmap_clks[] = {
	&clk_en_misc.clkr,
	&clk_en_pcie0.clkr,
	&clk_en_gspi.clkr,
	&clk_en_iso_misc.clkr,
	&clk_en_sds.clkr,
	&clk_en_hdmi.clkr,
	&clk_en_gpu.clkr,
	&clk_en_ve1.clkr,
	&clk_en_ve2.clkr,
	&clk_en_lsadc.clkr,
	&clk_en_se.clkr,
	&clk_en_dcu.clkr,
	&clk_en_cp.clkr,
	&clk_en_md.clkr,
	&clk_en_tp.clkr,
	&clk_en_rcic.clkr,
	&clk_en_nf.clkr,
	&clk_en_emmc.clkr,
	&clk_en_sd.clkr,
	&clk_en_sdio_ip.clkr,
	&clk_en_mipi_csi.clkr,
	&clk_en_emmc_ip.clkr,
	&clk_en_sdio.clkr,
	&clk_en_sd_ip.clkr,
	&clk_en_tpb.clkr,
	&clk_en_misc_sc1.clkr,
	&clk_en_misc_i2c_3.clkr,
	&clk_en_jpeg.clkr,
	&clk_en_acpu.clkr,
	&clk_en_misc_sc0.clkr,
	&clk_en_hdmirx.clkr,
	&clk_en_hse.clkr,
	&clk_en_fan.clkr,
	&clk_en_sata_wrap_sys.clkr,
	&clk_en_sata_wrap_sysh.clkr,
	&clk_en_sata_mac_sysh.clkr,
	&clk_en_r2rdsc.clkr,
	&clk_en_tpc.clkr,
	&clk_en_pcie1.clkr,
	&clk_en_misc_i2c_4.clkr,
	&clk_en_misc_i2c_5.clkr,
	&clk_en_tsio.clkr,
	&clk_en_ve4.clkr,
	&clk_en_edp.clkr,
	&clk_en_tsio_trx.clkr,
	&clk_en_pcie2.clkr,
	&clk_en_iso_gspi.clkr,
	&clk_en_earc.clkr,
	&clk_en_lite.clkr,
	&clk_en_mipi_dsi.clkr,
	&clk_en_npupp.clkr,
	&clk_en_npu.clkr,
	&clk_en_aucpu0.clkr,
	&clk_en_aucpu1.clkr,
	&clk_en_nsram.clkr,
	&clk_en_hdmitop.clkr,
	&clk_en_aucpu_sram.clkr,
	&clk_en_aucpu_iso_npu.clkr,
	&clk_en_keyladder.clkr,
	&clk_en_ifcp_klm.clkr,
	&clk_en_ifcp.clkr,
	&clk_en_mdl_genpw.clkr,
	&clk_en_mdl_chip.clkr,
	&clk_en_mdl_ip.clkr,
	&clk_en_mdlm2m.clkr,
	&clk_en_mdl_xtal.clkr,
	&clk_en_test_mux.clkr,
	&clk_en_dla.clkr,
	&clk_en_tpcw.clkr,
	&clk_en_gpu_ts_src.clkr,
	&clk_en_gpu2d.clkr,
	&clk_en_vi.clkr,
	&clk_en_lvds1.clkr,
	&clk_en_lvds2.clkr,
	&clk_en_aucpu.clkr,
	&clk_en_ur1.clkr,
	&clk_en_ur2.clkr,
	&clk_en_ur3.clkr,
	&clk_en_ur4.clkr,
	&clk_en_ur5.clkr,
	&clk_en_ur6.clkr,
	&clk_en_ur7.clkr,
	&clk_en_ur8.clkr,
	&clk_en_ur9.clkr,
	&clk_en_ur_top.clkr,
	&clk_en_misc_i2c_7.clkr,
	&clk_en_misc_i2c_6.clkr,
	&clk_en_spi0.clkr,
	&clk_en_spi1.clkr,
	&clk_en_spi2.clkr,
	&clk_en_lsadc0.clkr,
	&clk_en_lsadc1.clkr,
	&clk_en_isomis_dma.clkr,
	&clk_en_audio_adc.clkr,
	&clk_en_dptx.clkr,
	&clk_en_npu_mipi_csi.clkr,
	&clk_en_edptx.clkr,
	&clk_gpu.clkr,
	&clk_ve1.clkr,
	&clk_ve2.clkr,
	&clk_ve4.clkr,
	&pll_ve1.clkr,
	&pll_ddsa.clkr,
	&pll_bus.clkr,
	&pll_dcsb.clkr,
	&pll_gpu.clkr,
	&pll_npu.clkr,
	&pll_ve2.clkr,
	&pll_hifi.clkr,
	&pll_emmc.clkr,
	&pll_acpu.clkr,
	&clk_det.clkr,
};

static struct rtk_reset_bank rtd1625_cc_reset_banks[] = {
	{ .ofs = 0x000, .write_en = 1, },
	{ .ofs = 0x004, .write_en = 1, },
	{ .ofs = 0x008, .write_en = 1, },
	{ .ofs = 0x00c, .write_en = 1, },
	{ .ofs = 0x068, .write_en = 1, },
	{ .ofs = 0x090, .write_en = 1, },
	{ .ofs = 0x0b8, .write_en = 1, },
	{ .ofs = 0x454, },
	{ .ofs = 0x458, },
	{ .ofs = 0x464, },
	{ .ofs = 0x880, .write_en = 1, },
	{ .ofs = 0x890, .write_en = 1, },
};

static struct clk_hw_onecell_data rtd1625_cc_hw_data = {
	.num = RTD1625_CRT_CLK_MAX,
	.hws = {
		[RTD1625_CRT_CLK_EN_MISC] = &__clk_regmap_gate_hw(&clk_en_misc),
		[RTD1625_CRT_CLK_EN_PCIE0] = &__clk_regmap_gate_hw(&clk_en_pcie0),
		[RTD1625_CRT_CLK_EN_GSPI] = &__clk_regmap_gate_hw(&clk_en_gspi),
		[RTD1625_CRT_CLK_EN_ISO_MISC] = &__clk_regmap_gate_hw(&clk_en_iso_misc),
		[RTD1625_CRT_CLK_EN_SDS] = &__clk_regmap_gate_hw(&clk_en_sds),
		[RTD1625_CRT_CLK_EN_HDMI] = &__clk_regmap_gate_hw(&clk_en_hdmi),
		[RTD1625_CRT_CLK_EN_GPU] = &__clk_regmap_gate_hw(&clk_en_gpu),
		[RTD1625_CRT_CLK_EN_VE1] = &__clk_regmap_gate_hw(&clk_en_ve1),
		[RTD1625_CRT_CLK_EN_VE2] = &__clk_regmap_gate_hw(&clk_en_ve2),
		[RTD1625_CRT_CLK_EN_LSADC] = &__clk_regmap_gate_hw(&clk_en_lsadc),
		[RTD1625_CRT_CLK_EN_CP] = &__clk_regmap_gate_hw(&clk_en_cp),
		[RTD1625_CRT_CLK_EN_MD] = &__clk_regmap_gate_hw(&clk_en_md),
		[RTD1625_CRT_CLK_EN_TP] = &__clk_regmap_gate_hw(&clk_en_tp),
		[RTD1625_CRT_CLK_EN_RCIC] = &__clk_regmap_gate_hw(&clk_en_rcic),
		[RTD1625_CRT_CLK_EN_NF] = &__clk_regmap_gate_hw(&clk_en_nf),
		[RTD1625_CRT_CLK_EN_EMMC] = &__clk_regmap_gate_hw(&clk_en_emmc),
		[RTD1625_CRT_CLK_EN_SD] = &__clk_regmap_gate_hw(&clk_en_sd),
		[RTD1625_CRT_CLK_EN_SDIO_IP] = &__clk_regmap_gate_hw(&clk_en_sdio_ip),
		[RTD1625_CRT_CLK_EN_MIPI_CSI] = &__clk_regmap_gate_hw(&clk_en_mipi_csi),
		[RTD1625_CRT_CLK_EN_EMMC_IP] = &__clk_regmap_gate_hw(&clk_en_emmc_ip),
		[RTD1625_CRT_CLK_EN_SDIO] = &__clk_regmap_gate_hw(&clk_en_sdio),
		[RTD1625_CRT_CLK_EN_SD_IP] = &__clk_regmap_gate_hw(&clk_en_sd_ip),
		[RTD1625_CRT_CLK_EN_TPB] = &__clk_regmap_gate_hw(&clk_en_tpb),
		[RTD1625_CRT_CLK_EN_MISC_SC1] = &__clk_regmap_gate_hw(&clk_en_misc_sc1),
		[RTD1625_CRT_CLK_EN_MISC_I2C_3] = &__clk_regmap_gate_hw(&clk_en_misc_i2c_3),
		[RTD1625_CRT_CLK_EN_ACPU] = &__clk_regmap_gate_hw(&clk_en_acpu),
		[RTD1625_CRT_CLK_EN_JPEG] = &__clk_regmap_gate_hw(&clk_en_jpeg),
		[RTD1625_CRT_CLK_EN_MISC_SC0] = &__clk_regmap_gate_hw(&clk_en_misc_sc0),
		[RTD1625_CRT_CLK_EN_HDMIRX] = &__clk_regmap_gate_hw(&clk_en_hdmirx),
		[RTD1625_CRT_CLK_EN_HSE] = &__clk_regmap_gate_hw(&clk_en_hse),
		[RTD1625_CRT_CLK_EN_FAN] = &__clk_regmap_gate_hw(&clk_en_fan),
		[RTD1625_CRT_CLK_EN_SATA_WRAP_SYS] = &__clk_regmap_gate_hw(&clk_en_sata_wrap_sys),
		[RTD1625_CRT_CLK_EN_SATA_WRAP_SYSH] = &__clk_regmap_gate_hw(&clk_en_sata_wrap_sysh),
		[RTD1625_CRT_CLK_EN_SATA_MAC_SYSH] = &__clk_regmap_gate_hw(&clk_en_sata_mac_sysh),
		[RTD1625_CRT_CLK_EN_R2RDSC] = &__clk_regmap_gate_hw(&clk_en_r2rdsc),
		[RTD1625_CRT_CLK_EN_TPC] = &__clk_regmap_gate_hw(&clk_en_tpc),
		[RTD1625_CRT_CLK_EN_PCIE1] = &__clk_regmap_gate_hw(&clk_en_pcie1),
		[RTD1625_CRT_CLK_EN_MISC_I2C_4] = &__clk_regmap_gate_hw(&clk_en_misc_i2c_4),
		[RTD1625_CRT_CLK_EN_MISC_I2C_5] = &__clk_regmap_gate_hw(&clk_en_misc_i2c_5),
		[RTD1625_CRT_CLK_EN_TSIO] = &__clk_regmap_gate_hw(&clk_en_tsio),
		[RTD1625_CRT_CLK_EN_VE4] = &__clk_regmap_gate_hw(&clk_en_ve4),
		[RTD1625_CRT_CLK_EN_EDP] = &__clk_regmap_gate_hw(&clk_en_edp),
		[RTD1625_CRT_CLK_EN_TSIO_TRX] = &__clk_regmap_gate_hw(&clk_en_tsio_trx),
		[RTD1625_CRT_CLK_EN_PCIE2] = &__clk_regmap_gate_hw(&clk_en_pcie2),
		[RTD1625_CRT_CLK_EN_ISO_GSPI] = &__clk_regmap_gate_hw(&clk_en_iso_gspi),
		[RTD1625_CRT_CLK_EN_EARC] = &__clk_regmap_gate_hw(&clk_en_earc),
		[RTD1625_CRT_CLK_EN_LITE] = &__clk_regmap_gate_hw(&clk_en_lite),
		[RTD1625_CRT_CLK_EN_MIPI_DSI] = &__clk_regmap_gate_hw(&clk_en_mipi_dsi),
		[RTD1625_CRT_CLK_EN_NPUPP] = &__clk_regmap_gate_hw(&clk_en_npupp),
		[RTD1625_CRT_CLK_EN_NPU] = &__clk_regmap_gate_hw(&clk_en_npu),
		[RTD1625_CRT_CLK_EN_AUCPU0] = &__clk_regmap_gate_hw(&clk_en_aucpu0),
		[RTD1625_CRT_CLK_EN_AUCPU1] = &__clk_regmap_gate_hw(&clk_en_aucpu1),
		[RTD1625_CRT_CLK_EN_NSRAM] = &__clk_regmap_gate_hw(&clk_en_nsram),
		[RTD1625_CRT_CLK_EN_HDMITOP] = &__clk_regmap_gate_hw(&clk_en_hdmitop),
		[RTD1625_CRT_CLK_EN_AUCPU_SRAM] = &__clk_regmap_gate_hw(&clk_en_aucpu_sram),
		[RTD1625_CRT_CLK_EN_AUCPU_ISO_NPU] = &__clk_regmap_gate_hw(&clk_en_aucpu_iso_npu),
		[RTD1625_CRT_CLK_EN_KEYLADDER] = &__clk_regmap_gate_hw(&clk_en_keyladder),
		[RTD1625_CRT_CLK_EN_IFCP_KLM] = &__clk_regmap_gate_hw(&clk_en_ifcp_klm),
		[RTD1625_CRT_CLK_EN_IFCP] = &__clk_regmap_gate_hw(&clk_en_ifcp),
		[RTD1625_CRT_CLK_EN_MDL_GENPW] = &__clk_regmap_gate_hw(&clk_en_mdl_genpw),
		[RTD1625_CRT_CLK_EN_MDL_CHIP] = &__clk_regmap_gate_hw(&clk_en_mdl_chip),
		[RTD1625_CRT_CLK_EN_MDL_IP] = &__clk_regmap_gate_hw(&clk_en_mdl_ip),
		[RTD1625_CRT_CLK_EN_MDLM2M] = &__clk_regmap_gate_hw(&clk_en_mdlm2m),
		[RTD1625_CRT_CLK_EN_MDL_XTAL] = &__clk_regmap_gate_hw(&clk_en_mdl_xtal),
		[RTD1625_CRT_CLK_EN_TEST_MUX] = &__clk_regmap_gate_hw(&clk_en_test_mux),
		[RTD1625_CRT_CLK_EN_DLA] = &__clk_regmap_gate_hw(&clk_en_dla),
		[RTD1625_CRT_CLK_EN_TPCW] = &__clk_regmap_gate_hw(&clk_en_tpcw),
		[RTD1625_CRT_CLK_EN_GPU_TS_SRC] = &__clk_regmap_gate_hw(&clk_en_gpu_ts_src),
		[RTD1625_CRT_CLK_EN_GPU2D] = &__clk_regmap_gate_hw(&clk_en_gpu2d),
		[RTD1625_CRT_CLK_EN_VI] = &__clk_regmap_gate_hw(&clk_en_vi),
		[RTD1625_CRT_CLK_EN_LVDS1] = &__clk_regmap_gate_hw(&clk_en_lvds1),
		[RTD1625_CRT_CLK_EN_LVDS2] = &__clk_regmap_gate_hw(&clk_en_lvds2),
		[RTD1625_CRT_CLK_EN_AUCPU] = &__clk_regmap_gate_hw(&clk_en_aucpu),
		[RTD1625_CRT_CLK_EN_UR1] = &__clk_regmap_gate_hw(&clk_en_ur1),
		[RTD1625_CRT_CLK_EN_UR2] = &__clk_regmap_gate_hw(&clk_en_ur2),
		[RTD1625_CRT_CLK_EN_UR3] = &__clk_regmap_gate_hw(&clk_en_ur3),
		[RTD1625_CRT_CLK_EN_UR4] = &__clk_regmap_gate_hw(&clk_en_ur4),
		[RTD1625_CRT_CLK_EN_UR5] = &__clk_regmap_gate_hw(&clk_en_ur5),
		[RTD1625_CRT_CLK_EN_UR6] = &__clk_regmap_gate_hw(&clk_en_ur6),
		[RTD1625_CRT_CLK_EN_UR7] = &__clk_regmap_gate_hw(&clk_en_ur7),
		[RTD1625_CRT_CLK_EN_UR8] = &__clk_regmap_gate_hw(&clk_en_ur8),
		[RTD1625_CRT_CLK_EN_UR9] = &__clk_regmap_gate_hw(&clk_en_ur9),
		[RTD1625_CRT_CLK_EN_UR_TOP] = &__clk_regmap_gate_hw(&clk_en_ur_top),
		[RTD1625_CRT_CLK_EN_MISC_I2C_7] = &__clk_regmap_gate_hw(&clk_en_misc_i2c_7),
		[RTD1625_CRT_CLK_EN_MISC_I2C_6] = &__clk_regmap_gate_hw(&clk_en_misc_i2c_6),
		[RTD1625_CRT_CLK_EN_SPI0] = &__clk_regmap_gate_hw(&clk_en_spi0),
		[RTD1625_CRT_CLK_EN_SPI1] = &__clk_regmap_gate_hw(&clk_en_spi1),
		[RTD1625_CRT_CLK_EN_SPI2] = &__clk_regmap_gate_hw(&clk_en_spi2),
		[RTD1625_CRT_CLK_EN_LSADC0] = &__clk_regmap_gate_hw(&clk_en_lsadc0),
		[RTD1625_CRT_CLK_EN_LSADC1] = &__clk_regmap_gate_hw(&clk_en_lsadc1),
		[RTD1625_CRT_CLK_EN_ISOMIS_DMA] = &__clk_regmap_gate_hw(&clk_en_isomis_dma),
		[RTD1625_CRT_CLK_EN_AUDIO_ADC] = &__clk_regmap_gate_hw(&clk_en_audio_adc),
		[RTD1625_CRT_CLK_EN_DPTX] = &__clk_regmap_gate_hw(&clk_en_dptx),
		[RTD1625_CRT_CLK_EN_NPU_MIPI_CSI] = &__clk_regmap_gate_hw(&clk_en_npu_mipi_csi),
		[RTD1625_CRT_CLK_EN_EDPTX] = &__clk_regmap_gate_hw(&clk_en_edptx),
		[RTD1625_CRT_CLK_GPU] = &__clk_regmap_mux_hw(&clk_gpu),
		[RTD1625_CRT_CLK_VE1] = &__clk_regmap_mux_hw(&clk_ve1),
		[RTD1625_CRT_CLK_VE2] = &__clk_regmap_mux_hw(&clk_ve2),
		[RTD1625_CRT_CLK_VE4] = &__clk_regmap_mux_hw(&clk_ve4),
		[RTD1625_CRT_PLL_VE1] = &__clk_pll_hw(&pll_ve1),
		[RTD1625_CRT_PLL_DDSA] = &__clk_pll_hw(&pll_ddsa),
		[RTD1625_CRT_PLL_BUS] = &__clk_pll_hw(&pll_bus),
		[RTD1625_CRT_CLK_SYS] = &clk_sys.hw,
		[RTD1625_CRT_PLL_DCSB] = &__clk_pll_hw(&pll_dcsb),
		[RTD1625_CRT_CLK_SYSH] = &clk_sysh.hw,
		[RTD1625_CRT_PLL_GPU] = &__clk_pll_hw(&pll_gpu),
		[RTD1625_CRT_PLL_NPU] = &__clk_pll_hw(&pll_npu),
		[RTD1625_CRT_PLL_VE2] = &__clk_pll_hw(&pll_ve2),
		[RTD1625_CRT_PLL_HIFI] = &__clk_pll_hw(&pll_hifi),
		[RTD1625_CRT_PLL_EMMC_REF] = &pll_emmc_ref.hw,
		[RTD1625_CRT_PLL_EMMC] = &__clk_pll_mmc_hw(&pll_emmc),
		[RTD1625_CRT_PLL_EMMC_VP0] = &pll_emmc.phase0_hw,
		[RTD1625_CRT_PLL_EMMC_VP1] = &pll_emmc.phase1_hw,
		[RTD1625_CRT_PLL_ACPU] = &__clk_pll2_hw(&pll_acpu),
		[RTD1625_CRT_CLK_DET] = &__clk_regmap_clkdet_hw(&clk_det),
		[RTD1625_CRT_CLK_NPU] = &clk_npu.hw,
		[RTD1625_CRT_CLK_NPU_MIPI_CSI] = &clk_npu_mipi_csi.hw,

		[RTD1625_CRT_CLK_MAX] = NULL,
	},
};

static const struct rtk_clk_desc rtd1625_cc_desc = {
	.clk_data = &rtd1625_cc_hw_data,
	.clks = rtd1625_cc_regmap_clks,
	.num_clks = ARRAY_SIZE(rtd1625_cc_regmap_clks),
	.reset_banks = rtd1625_cc_reset_banks,
	.num_reset_banks = ARRAY_SIZE(rtd1625_cc_reset_banks),
};

static int rtd1625_cc_probe(struct platform_device *pdev)
{
	const struct rtk_clk_desc *desc;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	return rtk_clk_probe(pdev, desc);
}

static const struct of_device_id rtd1625_cc_match[] = {
	{ .compatible = "realtek,rtd1625-crt-clk", .data = &rtd1625_cc_desc,},
	{ /* sentinel */ }
};

static struct platform_driver rtd1625_cc_driver = {
	.probe = rtd1625_cc_probe,
	.driver = {
		.name = "rtk-rtd1625-crt-clk",
		.of_match_table = rtd1625_cc_match,
	},
};

static int __init rtd1625_cc_init(void)
{
	return platform_driver_register(&rtd1625_cc_driver);
}
subsys_initcall(rtd1625_cc_init);

MODULE_DESCRIPTION("Reatek RTD1625 CRT Controller Driver");
MODULE_AUTHOR("Cheng-Yu Lee <cylee12@realtek.com>");
MODULE_LICENSE("GPL v2");
