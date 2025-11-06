/*
 * rtk_snand.h - nand driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#ifndef __SNAND_REG_H
#define __SNAND_REG_H


#define SPINAND_RESET		0xff
#define SPINAND_READID		0x9f
#define SPINAND_GET_FEATURE	0x0f
#define SPINAND_SET_FEATURE	0x1f
#define SPINAND_ERASE		0xd8
#define SPINAND_PAGE_READ       0x13
#define SPINAND_PROGRAM_EXECUTE	0x10
#define SPINAND_WRITE_ENABLE	0x6

#define REG_ND_PA0              (0x0)
#define REG_ND_PA1              (0x4)
#define REG_ND_PA2              (0x8)
#define REG_ND_CA0              (0xc)
#define REG_ND_PA3              (0x2c)
#define REG_BLANK_CHK		(0x34)
#define REG_ND_ECC              (0x38)
#define REG_ND_CA1              (0x3c)
#define REG_SPI_CTRL0		(0x50)
#define REG_SPI_CTRL1		(0x54)
#define REG_SPI_CMD1		(0x58)
#define REG_SPI_CMD2		(0x5c)
#define REG_SPI_CMD3		(0x60)
#define REG_SPI_CMD4		(0x64)
#define REG_SPI_CMD5		(0x68)
#define REG_SPI_CMD6		(0x6c)
#define REG_SPI_SA		(0x7c)
#define REG_SPI_SDATA		(0x80)
#define REG_SPI_MASK		(0x84)
#define REG_SPI_HIT		(0x88)
#define REG_SPI_STATUS		(0x8c)
#define REG_SPI_T1		(0x90)
#define REG_SPI_T2		(0x94)
#define REG_SPI_T3		(0x98)
#define REG_SPI_WP		(0x9c)
#define REG_SPI_ID0		(0xa0)
#define REG_SPI_ID1		(0xa4)
#define REG_SPI_ID2		(0xa8)
#define REG_SPI_LOW_PWR		(0xac)
#define REG_RX_DLY		(0x280)

#define REG_DATA_TL0            (0x100)
#define REG_DATA_TL1            (0x104)
#define REG_PP_CTL0             (0x110)
#define REG_ECC_SEL             (0x128)
#define REG_PP_CTL1             (0x12c)
#define REG_RND_DATA_STR_COL_H	(0x210)
#define	REG_RND_SPR_STR_COL_H	(0x214)
#define REG_RND_SPR_STR_COL_L	(0x218)
#define REG_RND_EN              (0x21c)
#define REG_READ_BY_PP          (0x228)
#define REG_PAGE_LEN            (0x270)
#define REG_SRAM_CTL		(0x300)
#define REG_DMA_CTL1            (0x304)
#define REG_DMA_CTL2            (0x308)
#define REG_DMA_CTL3            (0x30c)
#define REG_SPR_DDR_CTL         (0x348)
#define REG_SPR_DDR_CTL2        (0x35c)

/* Reserve Block Area usage */
#define BB_INIT			0xFFFE
#define RB_INIT			0xFFFD
#define BBT_TAG			0xBB
#define SBT_TAG			0xAA
#define TAG_FACTORY_PARAM	(0x82)
#define BB_DIE_INIT		0xEEEE
#define RB_DIE_INIT		BB_DIE_INIT
#define BAD_RESERVED		0x4444
#define SB_INIT			0xFFAA
#define CRCLEN			4
#define TAGOFFSET		4
#define BLOCKINFO		2
#define	BBT1			1
#define	BBT2			2
#define SBT1			4
#define SBT2			5
#define SBTCNT			16
#define BOOTBLKSTART		6
#define NOTBOOTAREA	        0
#define BOOTAREA	        1
#define RTK_TIMEOUT		500000

#define NF_ND_PA0_page_addr0(value)             (0x000000FF&((value)<<0))
#define NF_ND_PA1_page_addr1(value)             (0x000000FF&((value)<<0))
#define NF_ND_PA2_addr_mode(value)              (0x000000E0&((value)<<5))
#define NF_ND_PA2_page_addr2(value)             (0x0000001F&((value)<<0))
#define REG_ND_PA0              (0x0)
#define REG_ND_PA1              (0x4)
#define REG_ND_PA2              (0x8)

enum access_mode {
	ECC,
	RAW
};

enum access_type {
	NFWRITE,
	NFREAD,
	NFERASE
};

enum table_type {
	BBTABLE,
	SBTABLE
};

struct sb_table {
        u16 chipnum;
        u16 block;
        u16 shift;
};

struct bb_table {
        u16 BB_die;
        u16 bad_block;
        u16 RB_die;
        u16 remap_block;
};

struct rtk_snand_conf {
	size_t chip_size;
	size_t page_size;
	size_t oob_size;
	u16 sector_size;
	u8 nsectors;
	u8 spare_size;
	u8 plane;
	unsigned char ecc;
	u16 ppb;
};

struct rtk_snand {
	struct spi_controller *ctlr;
	struct device *dev;
	struct clk* clk_nand;
	void __iomem *regs;
	void __iomem *pll_regs;
	unsigned char flashname[16];
	unsigned char ecc;
	unsigned char cmdbuf[128];
	const u8* txbuf;
	u8 *buf;
	u8 *oobtmp;
	u8 *dataBuf;
	dma_addr_t dataPhys;
	size_t buf_len;
	u64 chipsize;
	struct nand_flash_dev *nf_ids;
	struct rtk_snand_conf nfi_cfg;
	struct bb_table *bbt;
	struct sb_table *sbt;
	struct rtk_ecc_config *ecc_cfg;
	struct nand_ecc_engine ecc_eng;
	struct nand_ecc ecc1;
	u64 offs;
	u32 bbtcrc;
	u32 sbtcrc;
	u32 RBA;
	u32 RBASTART;
	u32 SHIFTBLK;   /* count in shift table */
	u32 bootarea;
	u32 bootblk;
	u32 bootareashift;
};

#endif
