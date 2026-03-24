/*
 *
 * Copyright (C) 2025 Realtek Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <log.h>
#include <dm.h>
#include <common.h>
#include <dt-bindings/clock/rtd1625-clk.h>
#include "clk_rtk.h"

/* TODO: MUX */
// static const char * const clk_gpu_parents[] = { "pll_gpu", "clk_sys" };
// static CLK_REGMAP_MUX(clk_gpu, clk_gpu_parents, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, 0x28, 12, 0x1);
// static const char * const clk_ve_parents[] = { "pll_vo", "clk_sysh", "pll_ve1", "pll_ve2" };
// static CLK_REGMAP_MUX(clk_ve1, clk_ve_parents, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, 0x4c, 0, 0x3);
// static CLK_REGMAP_MUX(clk_ve2, clk_ve_parents, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, 0x4c, 3, 0x3);
// static CLK_REGMAP_MUX(clk_ve4, clk_ve_parents, CLK_SET_RATE_PARENT | CLK_SET_RATE_NO_REPARENT, 0x4c, 6, 0x3);

static CLK_REGMAP_GATE_NO_PARENT(clk_en_misc, CLK_IS_CRITICAL, 0x050, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_pcie0, CLK_IGNORE_UNUSED, 0x050, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_gspi, CLK_IGNORE_UNUSED, 0x050, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_iso_misc, CLK_IGNORE_UNUSED, 0x050, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sds, CLK_IGNORE_UNUSED, 0x050, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_hdmi, CLK_IGNORE_UNUSED, 0x050, 14, 1);
static CLK_REGMAP_GATE(clk_en_gpu, "clk_gpu", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x050, 18, 1);
static CLK_REGMAP_GATE(clk_en_ve1, "clk_ve1", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x050, 20, 1);
static CLK_REGMAP_GATE(clk_en_ve2, "clk_ve2", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x050, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_se, CLK_IGNORE_UNUSED, 0x050, 30, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_md, CLK_IGNORE_UNUSED, 0x054, 4, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tp, CLK_IS_CRITICAL, 0x054, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_rcic, CLK_IGNORE_UNUSED, 0x054, 8, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_nf, CLK_IGNORE_UNUSED, 0x054, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_emmc, CLK_IGNORE_UNUSED, 0x054, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sd, CLK_IGNORE_UNUSED, 0x054, 14, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sdio_ip, CLK_IGNORE_UNUSED, 0x054, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mipi_csi, CLK_IGNORE_UNUSED, 0x054, 18, 1);
static CLK_REGMAP_GATE(clk_en_emmc_ip, "pll_emmc", CLK_SET_RATE_PARENT, 0x054, 20, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sdio, CLK_IGNORE_UNUSED, 0x054, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sd_ip, CLK_IGNORE_UNUSED, 0x054, 24, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tpb, CLK_IGNORE_UNUSED, 0x054, 28, 1);
static CLK_REGMAP_GATE(clk_en_misc_sc1, "clk_en_misc", CLK_IGNORE_UNUSED, 0x054, 30, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_3, "clk_en_misc", CLK_IGNORE_UNUSED, 0x058, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_jpeg, CLK_IGNORE_UNUSED, 0x058, 4, 1);
static CLK_REGMAP_GATE(clk_en_acpu, "pll_acpu", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x058, 6, 1);
static CLK_REGMAP_GATE(clk_en_misc_sc0, "clk_en_misc", CLK_IGNORE_UNUSED, 0x058, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_hdmirx, CLK_IGNORE_UNUSED, 0x058, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_hse, CLK_IS_CRITICAL, 0x058, 28, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_fan, CLK_IGNORE_UNUSED, 0x05c, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sata_wrap_sys, CLK_IGNORE_UNUSED, 0x05c, 8, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sata_wrap_sysh, CLK_IGNORE_UNUSED, 0x05c, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_sata_mac_sysh, CLK_IGNORE_UNUSED, 0x05c, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_r2rdsc, CLK_IGNORE_UNUSED, 0x05c, 14, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_pcie1, CLK_IGNORE_UNUSED, 0x05c, 18, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_4, "clk_en_misc", CLK_IGNORE_UNUSED, 0x05c, 20, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_5, "clk_en_misc", CLK_IGNORE_UNUSED, 0x05c, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tsio, CLK_IGNORE_UNUSED, 0x05c, 24, 1);
static CLK_REGMAP_GATE(clk_en_ve4, "clk_ve4", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED, 0x05c, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_edp, CLK_IGNORE_UNUSED, 0x05c, 28, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tsio_trx, CLK_IGNORE_UNUSED, 0x05c, 30, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_pcie2, CLK_IGNORE_UNUSED, 0x08c, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_earc, CLK_IGNORE_UNUSED, 0x08c, 4, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lite, CLK_IGNORE_UNUSED, 0x08c, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mipi_dsi, CLK_IGNORE_UNUSED, 0x08c, 8, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_npupp, CLK_IGNORE_UNUSED, 0x08c, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_npu, CLK_IGNORE_UNUSED, 0x08c, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu0, CLK_IGNORE_UNUSED, 0x08c, 14, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu1, CLK_IGNORE_UNUSED, 0x08c, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_nsram, CLK_IGNORE_UNUSED, 0x08c, 18, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_hdmitop, CLK_IGNORE_UNUSED, 0x08c, 20, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu_iso_npu, CLK_IGNORE_UNUSED, 0x08c, 24, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_keyladder, CLK_IGNORE_UNUSED, 0x08c, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_ifcp_klm, CLK_IGNORE_UNUSED, 0x08c, 28, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_ifcp, CLK_IGNORE_UNUSED, 0x08c, 30, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdl_genpw, CLK_IGNORE_UNUSED, 0x0b0, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdl_chip, CLK_IGNORE_UNUSED, 0x0b0, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdl_ip, CLK_IGNORE_UNUSED, 0x0b0, 4, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdlm2m, CLK_IGNORE_UNUSED, 0x0b0, 6, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_mdl_xtal, CLK_IGNORE_UNUSED, 0x0b0, 8, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_test_mux, CLK_IGNORE_UNUSED, 0x0b0, 10, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_dla, CLK_IGNORE_UNUSED, 0x0b0, 12, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_tpcw, CLK_IGNORE_UNUSED, 0x0b0, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_gpu_ts_src, CLK_IGNORE_UNUSED, 0x0b0, 18, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_vi, CLK_IGNORE_UNUSED, 0x0b0, 22, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lvds1, CLK_IGNORE_UNUSED, 0x0b0, 24, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lvds2, CLK_IGNORE_UNUSED, 0x0b0, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_aucpu, CLK_IGNORE_UNUSED, 0x0b0, 28, 1);
static CLK_REGMAP_GATE(clk_en_ur1, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 0, 1);
static CLK_REGMAP_GATE(clk_en_ur2, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 2, 1);
static CLK_REGMAP_GATE(clk_en_ur3, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 4, 1);
static CLK_REGMAP_GATE(clk_en_ur4, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 6, 1);
static CLK_REGMAP_GATE(clk_en_ur5, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 8, 1);
static CLK_REGMAP_GATE(clk_en_ur6, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 10, 1);
static CLK_REGMAP_GATE(clk_en_ur7, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 12, 1);
static CLK_REGMAP_GATE(clk_en_ur8, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 14, 1);
static CLK_REGMAP_GATE(clk_en_ur9, "clk_en_ur_top", CLK_IGNORE_UNUSED, 0x884, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_ur_top, CLK_IS_CRITICAL, 0x884, 18, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_7, "clk_en_misc", CLK_IGNORE_UNUSED, 0x884, 28, 1);
static CLK_REGMAP_GATE(clk_en_misc_i2c_6, "clk_en_misc", CLK_IGNORE_UNUSED, 0x884, 30, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_spi0, CLK_IGNORE_UNUSED, 0x894, 0, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_spi1, CLK_IGNORE_UNUSED, 0x894, 2, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_spi2, CLK_IGNORE_UNUSED, 0x894, 4, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lsadc0, CLK_IGNORE_UNUSED, 0x894, 16, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_lsadc1, CLK_IGNORE_UNUSED, 0x894, 18, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_isomis_dma, CLK_IGNORE_UNUSED, 0x894, 20, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_dptx, CLK_IGNORE_UNUSED, 0x894, 24, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_npu_mipi_csi, CLK_IGNORE_UNUSED, 0x894, 26, 1);
static CLK_REGMAP_GATE_NO_PARENT(clk_en_edptx, CLK_IGNORE_UNUSED, 0x894, 28, 1);


static const struct rtk_gate *cc_gates[] = {
	[RTD1625_CRT_CLK_EN_MISC] = &clk_en_misc,
	[RTD1625_CRT_CLK_EN_PCIE0] = &clk_en_pcie0,
	[RTD1625_CRT_CLK_EN_GSPI] = &clk_en_gspi,
	[RTD1625_CRT_CLK_EN_ISO_MISC] = &clk_en_iso_misc,
	[RTD1625_CRT_CLK_EN_SDS] = &clk_en_sds,
	[RTD1625_CRT_CLK_EN_HDMI] = &clk_en_hdmi,
	[RTD1625_CRT_CLK_EN_GPU] = &clk_en_gpu,
	[RTD1625_CRT_CLK_EN_VE1] = &clk_en_ve1,
	[RTD1625_CRT_CLK_EN_VE2] = &clk_en_ve2,
	[RTD1625_CRT_CLK_EN_MD] = &clk_en_md,
	[RTD1625_CRT_CLK_EN_TP] = &clk_en_tp,
	[RTD1625_CRT_CLK_EN_RCIC] = &clk_en_rcic,
	[RTD1625_CRT_CLK_EN_NF] = &clk_en_nf,
	[RTD1625_CRT_CLK_EN_EMMC] = &clk_en_emmc,
	[RTD1625_CRT_CLK_EN_SD] = &clk_en_sd,
	[RTD1625_CRT_CLK_EN_SDIO_IP] = &clk_en_sdio_ip,
	[RTD1625_CRT_CLK_EN_MIPI_CSI] = &clk_en_mipi_csi,
	[RTD1625_CRT_CLK_EN_EMMC_IP] = &clk_en_emmc_ip,
	[RTD1625_CRT_CLK_EN_SDIO] = &clk_en_sdio,
	[RTD1625_CRT_CLK_EN_SD_IP] = &clk_en_sd_ip,
	[RTD1625_CRT_CLK_EN_TPB] = &clk_en_tpb,
	[RTD1625_CRT_CLK_EN_MISC_SC1] = &clk_en_misc_sc1,
	[RTD1625_CRT_CLK_EN_MISC_I2C_3] = &clk_en_misc_i2c_3,
	[RTD1625_CRT_CLK_EN_ACPU] = &clk_en_acpu,
	[RTD1625_CRT_CLK_EN_JPEG] = &clk_en_jpeg,
	[RTD1625_CRT_CLK_EN_MISC_SC0] = &clk_en_misc_sc0,
	[RTD1625_CRT_CLK_EN_HDMIRX] = &clk_en_hdmirx,
	[RTD1625_CRT_CLK_EN_HSE] = &clk_en_hse,
	[RTD1625_CRT_CLK_EN_FAN] = &clk_en_fan,
	[RTD1625_CRT_CLK_EN_SATA_WRAP_SYS] = &clk_en_sata_wrap_sys,
	[RTD1625_CRT_CLK_EN_SATA_WRAP_SYSH] = &clk_en_sata_wrap_sysh,
	[RTD1625_CRT_CLK_EN_SATA_MAC_SYSH] = &clk_en_sata_mac_sysh,
	[RTD1625_CRT_CLK_EN_R2RDSC] = &clk_en_r2rdsc,
	[RTD1625_CRT_CLK_EN_PCIE1] = &clk_en_pcie1,
	[RTD1625_CRT_CLK_EN_MISC_I2C_4] = &clk_en_misc_i2c_4,
	[RTD1625_CRT_CLK_EN_MISC_I2C_5] = &clk_en_misc_i2c_5,
	[RTD1625_CRT_CLK_EN_TSIO] = &clk_en_tsio,
	[RTD1625_CRT_CLK_EN_VE4] = &clk_en_ve4,
	[RTD1625_CRT_CLK_EN_EDP] = &clk_en_edp,
	[RTD1625_CRT_CLK_EN_TSIO_TRX] = &clk_en_tsio_trx,
	[RTD1625_CRT_CLK_EN_PCIE2] = &clk_en_pcie2,
	[RTD1625_CRT_CLK_EN_EARC] = &clk_en_earc,
	[RTD1625_CRT_CLK_EN_LITE] = &clk_en_lite,
	[RTD1625_CRT_CLK_EN_MIPI_DSI] = &clk_en_mipi_dsi,
	[RTD1625_CRT_CLK_EN_NPUPP] = &clk_en_npupp,
	[RTD1625_CRT_CLK_EN_NPU] = &clk_en_npu,
	[RTD1625_CRT_CLK_EN_AUCPU0] = &clk_en_aucpu0,
	[RTD1625_CRT_CLK_EN_AUCPU1] = &clk_en_aucpu1,
	[RTD1625_CRT_CLK_EN_NSRAM] = &clk_en_nsram,
	[RTD1625_CRT_CLK_EN_HDMITOP] = &clk_en_hdmitop,
	[RTD1625_CRT_CLK_EN_AUCPU_ISO_NPU] = &clk_en_aucpu_iso_npu,
	[RTD1625_CRT_CLK_EN_KEYLADDER] = &clk_en_keyladder,
	[RTD1625_CRT_CLK_EN_IFCP_KLM] = &clk_en_ifcp_klm,
	[RTD1625_CRT_CLK_EN_IFCP] = &clk_en_ifcp,
	[RTD1625_CRT_CLK_EN_MDL_GENPW] = &clk_en_mdl_genpw,
	[RTD1625_CRT_CLK_EN_MDL_CHIP] = &clk_en_mdl_chip,
	[RTD1625_CRT_CLK_EN_MDL_IP] = &clk_en_mdl_ip,
	[RTD1625_CRT_CLK_EN_MDLM2M] = &clk_en_mdlm2m,
	[RTD1625_CRT_CLK_EN_MDL_XTAL] = &clk_en_mdl_xtal,
	[RTD1625_CRT_CLK_EN_TEST_MUX] = &clk_en_test_mux,
	[RTD1625_CRT_CLK_EN_DLA] = &clk_en_dla,
	[RTD1625_CRT_CLK_EN_TPCW] = &clk_en_tpcw,
	[RTD1625_CRT_CLK_EN_GPU_TS_SRC] = &clk_en_gpu_ts_src,
	[RTD1625_CRT_CLK_EN_VI] = &clk_en_vi,
	[RTD1625_CRT_CLK_EN_LVDS1] = &clk_en_lvds1,
	[RTD1625_CRT_CLK_EN_LVDS2] = &clk_en_lvds2,
	[RTD1625_CRT_CLK_EN_AUCPU] = &clk_en_aucpu,
	[RTD1625_CRT_CLK_EN_UR1] = &clk_en_ur1,
	[RTD1625_CRT_CLK_EN_UR2] = &clk_en_ur2,
	[RTD1625_CRT_CLK_EN_UR3] = &clk_en_ur3,
	[RTD1625_CRT_CLK_EN_UR4] = &clk_en_ur4,
	[RTD1625_CRT_CLK_EN_UR5] = &clk_en_ur5,
	[RTD1625_CRT_CLK_EN_UR6] = &clk_en_ur6,
	[RTD1625_CRT_CLK_EN_UR7] = &clk_en_ur7,
	[RTD1625_CRT_CLK_EN_UR8] = &clk_en_ur8,
	[RTD1625_CRT_CLK_EN_UR9] = &clk_en_ur9,
	[RTD1625_CRT_CLK_EN_UR_TOP] = &clk_en_ur_top,
	[RTD1625_CRT_CLK_EN_MISC_I2C_7] = &clk_en_misc_i2c_7,
	[RTD1625_CRT_CLK_EN_MISC_I2C_6] = &clk_en_misc_i2c_6,
	[RTD1625_CRT_CLK_EN_SPI0] = &clk_en_spi0,
	[RTD1625_CRT_CLK_EN_SPI1] = &clk_en_spi1,
	[RTD1625_CRT_CLK_EN_SPI2] = &clk_en_spi2,
	[RTD1625_CRT_CLK_EN_LSADC0] = &clk_en_lsadc0,
	[RTD1625_CRT_CLK_EN_LSADC1] = &clk_en_lsadc1,
	[RTD1625_CRT_CLK_EN_ISOMIS_DMA] = &clk_en_isomis_dma,
	[RTD1625_CRT_CLK_EN_DPTX] = &clk_en_dptx,
	[RTD1625_CRT_CLK_EN_NPU_MIPI_CSI] = &clk_en_npu_mipi_csi,
	[RTD1625_CRT_CLK_EN_EDPTX] = &clk_en_edptx,
	// [RTD1625_CRT_CLK_GPU] = &clk_gpu,
	// [RTD1625_CRT_CLK_VE1] = &clk_ve1,
	// [RTD1625_CRT_CLK_VE2] = &clk_ve2,
	// [RTD1625_CRT_CLK_VE4] = &clk_ve4,
	// [RTD1625_CRT_PLL_VE1] = &pll_ve1,
	// [RTD1625_CRT_PLL_DDSA] = &pll_ddsa,
	// [RTD1625_CRT_PLL_BUS] = &pll_bus,
	// [RTD1625_CRT_CLK_SYS] = &clk_sys.hw,
	// [RTD1625_CRT_PLL_DCSB] = &pll_dcsb,
	// [RTD1625_CRT_CLK_SYSH] = &clk_sysh.hw,
	// [RTD1625_CRT_PLL_GPU] = &pll_gpu,
	// [RTD1625_CRT_PLL_NPU] = &pll_npu,
	// [RTD1625_CRT_PLL_VE2] = &pll_ve2,
	// [RTD1625_CRT_PLL_HIFI] = &pll_hifi,
	// [RTD1625_CRT_PLL_EMMC_REF] = &pll_emmc_ref.hw,
	// [RTD1625_CRT_PLL_EMMC] = &pll_emmc,
	// [RTD1625_CRT_PLL_EMMC_VP0] = &pll_emmc.phase0_hw,
	// [RTD1625_CRT_PLL_EMMC_VP1] = &pll_emmc.phase1_hw,
	// [RTD1625_CRT_PLL_ACPU] = &pll_acpu,
	// [RTD1625_CRT_CLK_DET] = &clk_det,
	// [RTD1625_CRT_CLK_NPU] = &clk_npu.hw,
	// [RTD1625_CRT_CLK_NPU_MIPI_CSI] = &clk_npu_mipi_csi.hw,
	[RTD1625_CRT_CLK_MAX] = NULL,
};


static const struct udevice_id clk_rtk_ctrl_ids[] = {
	{.compatible = "realtek,rtd1625-crt-clk"},
	{},
};

static int clk_rtk_ctrl_probe(struct udevice *dev)
{
	struct clk_rtk_ctrl_priv *priv = dev_get_priv(dev);

	priv->reg_base = dev_read_addr_ptr(dev);
	if (!priv->reg_base)
		return -ENOENT;

	priv->gates = (struct rtk_gate **) cc_gates;
	priv->gate_count = RTD1625_CRT_CLK_MAX;

	return 0;
}


U_BOOT_DRIVER(clk_rtk_ctrl_cc) = {
	.name = "rtk_ctrl_cc",
	.id = UCLASS_CLK,
	.of_match = clk_rtk_ctrl_ids,
	.ops = &clk_rtk_ctrl_ops,
	.probe = clk_rtk_ctrl_probe,
	.priv_auto = sizeof(struct clk_rtk_ctrl_priv),
};