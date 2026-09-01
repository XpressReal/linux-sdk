#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/spinand.h>
#include <linux/arm-smccc.h>
#include "spi_rtk_snand.h"
#include <linux/debugfs.h>

static unsigned int crc_table[256];
static int crc_table_computed = 0;

static int rtk_snand_scan_sbt(struct rtk_snand *snf);
static int rtk_snand_scan_bbt(struct rtk_snand *snf);
static int rtk_snand_table_crc_calculate(struct rtk_snand *snf, int type, void *t, int *cnt);
static int rtk_snand_get_physical_page(struct rtk_snand *snf, int page);
static int rtk_snand_get_mapping_page(struct rtk_snand *snf, int page);

static struct rtk_snand *nand_to_rtk_snand(struct nand_device *nand)
{
	struct nand_ecc_engine *eng = nand->ecc.engine;

	return container_of(eng, struct rtk_snand, ecc_eng);
}

static int wait_done(void __iomem *regs, u64 mask, unsigned int value)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(regs, val, (val & mask) == value, 10,
					RTK_TIMEOUT);
	if (ret)
		return -EIO;

	return 0;
}

static int rtk_snand_ooblayout_ecc(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *oobregion)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct rtk_snand *snf = nand_to_rtk_snand(nand);
	int eccstep = snf->nfi_cfg.page_size / SZ_512;
	int ecc_size = (snf->nfi_cfg.ecc == 0x1) ? 20 : 10;

	if (section < 0 || section >= eccstep)
		return -ERANGE;

	oobregion->offset = ((6 + ecc_size) * section) + 6;
	oobregion->length = ecc_size;

	return 0;
}

static int rtk_snand_ooblayout_free(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *oobregion)
{
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct rtk_snand *snf = nand_to_rtk_snand(nand);
	int eccstep = snf->nfi_cfg.page_size / SZ_512;
	int ecc_size = (snf->nfi_cfg.ecc == 0x1) ? 20 : 10;

	if (section < 0 || section >= eccstep)
		return -ERANGE;

	oobregion->offset = (6 + ecc_size) * section;
	oobregion->length = 6;

	return 0;
}

static const struct mtd_ooblayout_ops rtk_snand_ooblayout = {
	.ecc = rtk_snand_ooblayout_ecc,
	.free = rtk_snand_ooblayout_free,
};

static int rtk_snand_prepare_bouncebuf(struct rtk_snand *snf, size_t size)
{
	unsigned int total_size = snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size;
	u32 ps = snf->nfi_cfg.page_size;
	u32 os = snf->nfi_cfg.oob_size;

	if (snf->buf_len >= size)
		return 0;

	if (snf->buf)
		kfree(snf->buf);

	snf->buf = kmalloc(total_size, GFP_KERNEL);
	if (!snf->buf)
		return -ENOMEM;

	snf->buf_len = total_size;
	memset(snf->buf, 0xff, snf->buf_len);

	if (!snf->dataBuf) {
		snf->dataBuf = (unsigned char *)dma_alloc_coherent(snf->dev,
				total_size,
				&snf->dataPhys, GFP_KERNEL);
		if (!snf->dataBuf) {
			kfree(snf->buf);
			snf->buf = NULL;
			return -ENOMEM;
		}
	}

	if (!snf->w_dbuf) {
		snf->w_dbuf = dma_alloc_coherent(snf->dev, ps,
						 &snf->dbuf_dma, GFP_KERNEL);
		if (!snf->w_dbuf) {
			dma_free_coherent(snf->dev, total_size, snf->dataBuf, snf->dataPhys);
			snf->dataBuf = NULL;
			kfree(snf->buf);
			snf->buf = NULL;
			return -ENOMEM;
		}
	}

	if (!snf->w_obuf) {
		snf->w_obuf = dma_alloc_coherent(snf->dev, os,
						 &snf->obuf_dma, GFP_KERNEL);
		if (!snf->w_obuf) {
			dma_free_coherent(snf->dev, ps, snf->w_dbuf, snf->dbuf_dma);
			snf->w_dbuf = NULL;
			dma_free_coherent(snf->dev, total_size, snf->dataBuf, snf->dataPhys);
			snf->dataBuf = NULL;
			kfree(snf->buf);
			snf->buf = NULL;
			return -ENOMEM;
		}
	}

	return 0;
}

static int rtk_snand_setup_pagefmt(struct rtk_snand *snf, u32 page_size,
                                   u32 oob_size)
{
	u32 spare_size;
	u8 nsectors;

	nsectors = page_size / 512;
	if (nsectors > 8) {
		dev_err(snf->dev, "too many sectors required.\n");
		goto err;
	}

	spare_size = oob_size / nsectors;

	snf->nfi_cfg.sector_size = 512;
	snf->nfi_cfg.page_size = page_size;
	snf->nfi_cfg.oob_size = oob_size;
	snf->nfi_cfg.nsectors = nsectors;
	snf->nfi_cfg.spare_size = spare_size;

	dev_dbg(snf->dev, "page format: (%u + %u) * %u\n",
		snf->nfi_cfg.sector_size, spare_size, nsectors);
	return rtk_snand_prepare_bouncebuf(snf, page_size + oob_size);
err:
	dev_err(snf->dev, "page size %u + %u is not supported\n", page_size,
		oob_size);
	return -EOPNOTSUPP;
}

static int rtk_snand_ecc_init_ctx(struct nand_device *nand)
{
	struct mtd_info *mtd = nanddev_to_mtd(nand);
	struct rtk_snand *snf = nand_to_rtk_snand(nand);
	struct nand_ecc_props *conf = &nand->ecc.ctx.conf;
	struct nand_ecc_props *user = &nand->ecc.user_conf;
	int step_size, strength, hw_max_strength = 6;
	int total_corr_bits, sectors;
	int ret;
	u32 b;

	ret = rtk_snand_setup_pagefmt(snf, nand->memorg.pagesize,
				      nand->memorg.oobsize);
	if (ret)
		return ret;

	snf->nfi_cfg.page_size = nand->memorg.pagesize;
	snf->nfi_cfg.ppb = nand->memorg.pages_per_eraseblock;
	snf->nfi_cfg.oob_size = nand->memorg.oobsize;
	snf->nfi_cfg.plane = nand->memorg.planes_per_lun -1;
	snf->nfi_cfg.chip_size = (u64)nand->memorg.pagesize *
		nand->memorg.pages_per_eraseblock * nand->memorg.eraseblocks_per_lun;
	snf->nfi_cfg.ecc = ((u64)snf->nfi_cfg.oob_size * 16 >= snf->nfi_cfg.page_size) ? 1 : 0;

	pr_info("=== NAND Config ===\n");
	pr_info("page_size:     %zu\n", snf->nfi_cfg.page_size);
	pr_info("ppb:           %u\n", snf->nfi_cfg.ppb);
	pr_info("oob_size:      %zu\n", snf->nfi_cfg.oob_size);
	pr_info("plane:         %u\n", snf->nfi_cfg.plane);
	pr_info("chip_size:     %llu (0x%llx)\n",
		(unsigned long long)snf->nfi_cfg.chip_size,
		(unsigned long long)snf->nfi_cfg.chip_size);
	pr_info("ecc:           %d\n", snf->nfi_cfg.ecc);

	if (snf->nfi_cfg.ecc)
		hw_max_strength = 12;

	if (user->step_size && user->strength) {
		step_size = user->step_size;
		strength = user->strength;
	} else {
		strength = hw_max_strength;
		step_size = 512;
	}

	if (step_size && strength) {
		total_corr_bits = (mtd->writesize / step_size) * strength;
		sectors = mtd->writesize / 512;
		strength = total_corr_bits / sectors;
		step_size = 512;

		dev_info(snf->dev, "ECC: auto config strength = %d\n", strength);
	}

	if (strength > 12) {
		dev_warn(snf->dev, "Required ECC (%d bits) exceeds HW max, capping at 12\n", strength);
		strength = 12;
	} else if (strength > 6) {
		strength = 12;
	} else {
		strength = 6;
	}

	conf->step_size = step_size;
	conf->strength = strength;
	snf->ecc_threshold = strength - 2;

	ret = rtk_snand_scan_sbt(snf);
	if (ret) {
		dev_warn(snf->dev, "SBT scan fail, ret = %d\n", ret);
		return ret;
	}

	b = (unsigned int)(snf->nfi_cfg.chip_size) / (snf->nfi_cfg.page_size * snf->nfi_cfg.ppb);
	snf->RBA = ((b * 5) / 100) - BLOCKINFO - snf->SHIFTBLK;

	snf->bbt = kmalloc(sizeof(struct bb_table) * snf->RBA, GFP_KERNEL);
	if (!snf->bbt) {
		dev_err(snf->dev, "RTK %s(%d) alloc bbt fail.\n",
			__func__, __LINE__);
		return -ENOMEM;
	}
	memset(snf->bbt, 0xff, sizeof(struct bb_table) * snf->RBA);

	ret = rtk_snand_scan_bbt(snf);
	if (ret) {
		dev_err(snf->dev, "BBT scan fail, ret = %d\n", ret);
		return ret;
	}

	mtd_set_ooblayout(mtd, &rtk_snand_ooblayout);

	return 0;
}

static void rtk_snand_ecc_cleanup_ctx(struct nand_device *nand)
{
}

static int rtk_snand_ecc_prepare_io_req(struct nand_device *nand,
					struct nand_page_io_req *req)
{
	struct rtk_snand *snf = nand_to_rtk_snand(nand);
	struct rtk_ecc_config *ecc_cfg = nand_to_ecc_ctx(nand);

	snf->ecc_cfg = ecc_cfg;

	return 0;
}

static int rtk_snand_ecc_finish_io_req(struct nand_device *nand,
				       struct nand_page_io_req *req)
{
	return 0;
}

static struct nand_ecc_engine_ops rtk_snf_ecc_engine_ops = {
	.init_ctx = rtk_snand_ecc_init_ctx,
	.cleanup_ctx = rtk_snand_ecc_cleanup_ctx,
	.prepare_io_req = rtk_snand_ecc_prepare_io_req,
	.finish_io_req = rtk_snand_ecc_finish_io_req,
};

static int rtk_snand_read_fdm(struct rtk_snand *snf, u8 *buf);
static int rtk_snand_read_bbt_page(struct rtk_snand *snf, int page, u8 *p, u32 size)
{
	void __iomem *base = snf->regs;
	int ret = 0;
	dma_addr_t buf_dma = snf->dataPhys;

	writel(0x41, base + REG_SPI_MASK);
	writel(0x0, base + REG_SPI_HIT);

	// enable random mode
	writel(0x1, base + REG_RND_EN);
        writel(0, base + REG_RND_DATA_STR_COL_H);
        writel(snf->nfi_cfg.page_size >> 8, base + REG_RND_SPR_STR_COL_H);
        writel(snf->nfi_cfg.page_size & 0xff, base + REG_RND_SPR_STR_COL_L);

	// enable blank check
	writel(0x1, base + REG_BLANK_CHK);
	if (snf->nfi_cfg.ecc)
		writel(0xc, base + 0x1c);
	else
		writel(0x6, base + 0x1c);

	// set PA and CA
	writel(page & 0xff, base + REG_ND_PA0);
	writel((page >> 8) & 0xff, base + REG_ND_PA1);
	writel((page >> 16) & 0x1f, base + REG_ND_PA2);

	writel(0x0, base + REG_ND_CA0);
        writel(0x0, base + REG_ND_CA1);

	// set spare dma addr
	writel(0x0, base + REG_SPR_DDR_CTL);

	// set transfer size
	writel(0x82, base + REG_DATA_TL1);
	writel(snf->nfi_cfg.page_size / 0x200, base + REG_PAGE_LEN);

	// set data dma addr
	writel((uintptr_t)(buf_dma >> 3), base + REG_DMA_CTL1);
	writel((snf->nfi_cfg.page_size >> 9) & 0x0000FFFF, base + REG_DMA_CTL2);

	// dma xfer setting
        writel(0x3, base + REG_DMA_CTL3);

	// set PP (read is through PP SRAM)
	writel(0x80, base + REG_READ_BY_PP);
        writel(0x0, base + REG_PP_CTL1);
        writel(0x0, base + REG_PP_CTL0);

	writel(0xc0, base + REG_SPI_SA);
	writel(0x0, base + REG_SPI_CMD1);

	writel(0x9 | (0x1 << 4), base + REG_SPI_CTRL0);
	writel(0x83, base + REG_SPI_CTRL1);

	// wait auto trigger done, pp busy done and dma xfer done
        ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
	if (ret) {
		pr_err("busy waiting bbt read SPI_CTRL0 done...\n");
		return -ETIMEDOUT;
	}

	ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
	if (ret) {
		pr_err("busy waiting bbt dma xfer done...\n");
		return -ETIMEDOUT;
	}

	if (p) {
		if (p != snf->dataBuf)
			memcpy(p, snf->dataBuf, snf->nfi_cfg.page_size);
		ret = rtk_snand_read_fdm(snf, p + snf->nfi_cfg.page_size);
		if (ret)
			return ret;
	}

	return 0;
}

static void rtk_snand_dump_SBT(struct rtk_snand *snf)
{
	int i = 0;

	for (i = 0; i < SBTCNT; i++) {
		if (snf->sbt[i].block == SB_INIT)
			break;
		pr_info("[%d](%d, %d)\n", i, snf->sbt[i].block, snf->sbt[i].shift);
        }

	snf->SHIFTBLK = i;

	return;
}

static int rtk_snand_scan_sbt(struct rtk_snand *snf)
{
	struct device *dev = snf->dev;
	unsigned char *new_buf = snf->dataBuf;
	u32 total_size = snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size;
	static const u32 sbt_blocks[] = { 4, 5 };  /* SBT1 and SBT2 block positions */
	int ret, i;
	int crc_check;

	/* Allocate SBT table */
	snf->sbt = kzalloc(sizeof(struct sb_table) * SBTCNT, GFP_KERNEL);
	if (!snf->sbt) {
		dev_err(dev, "RTK %s(%d) alloc sbt fail.\n", __func__, __LINE__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(sbt_blocks); i++) {
		memset(new_buf, 0xFF, total_size);
		ret = rtk_snand_read_bbt_page(snf, sbt_blocks[i] * snf->nfi_cfg.ppb,
				 new_buf, total_size);

		dev_dbg(dev, "ret=%d, tag=0x%02x\n", ret, new_buf[snf->nfi_cfg.page_size + 4]);

		if (ret == 0 && new_buf[snf->nfi_cfg.page_size + 4] == SBT_TAG) {
			/* CRC check */
			crc_check = rtk_snand_table_crc_calculate(snf, SBTABLE, new_buf + 4, NULL);
			if (crc_check == *(u32 *)new_buf) {
				memcpy(snf->sbt, new_buf + CRCLEN,
					sizeof(struct sb_table) * SBTCNT);
				dev_info(dev, "SBT%d found at block %d, CRC check OK.\n",
					i + 1, sbt_blocks[i] * snf->nfi_cfg.ppb);
				rtk_snand_dump_SBT(snf);
				return 0;
			}
			dev_err(dev, "SBT%d CRC fail, calc=0x%08x, stored=0x%08x\n",
				i + 1, crc_check, *(u32 *)new_buf);
		}
	}

	dev_info(dev, "RTK %s(%d) NO SBT1 & NO SBT2.\n", __func__, __LINE__);
	kfree(snf->sbt);
	snf->sbt = NULL;

	return -ENODEV;
}

static void rtk_snand_dump_BBT(struct rtk_snand *snf)
{
	struct device *dev = snf->dev;
	int i, BBs = 0;

	dev_info(dev, "snand BBT Content\n");

	if (snf->bbt) {
		for (i = 0; i < snf->RBA; i++){
			if (i == 0 && snf->bbt[i].BB_die == BB_DIE_INIT
				&& snf->bbt[i].bad_block == BB_INIT ){
				dev_info(dev, "Congratulation!! No BBs in this snand.\n");
				break;
			}
			if (snf->bbt[i].bad_block != BB_INIT ){
				pr_err("[%d] (%d, %u, %d, %u)\n", i,
				snf->bbt[i].BB_die, snf->bbt[i].bad_block,
				snf->bbt[i].RB_die, snf->bbt[i].remap_block);
				BBs++;
			}
		}
	}
	return;
}

static int rtk_snand_scan_bbt(struct rtk_snand *snf)
{
	struct device *dev = snf->dev;
	unsigned char *new_buf = snf->dataBuf;
	u32 total_size = snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size;
	static const u32 bbt_blocks[] = { 1, 2 };  /* BBT1 and BBT2 positions */
	int ret, i;
	int crc_check;

	for (i = 0; i < ARRAY_SIZE(bbt_blocks); i++) {
		memset(new_buf, 0xFF, total_size);
		ret = rtk_snand_read_bbt_page(snf, bbt_blocks[i] * snf->nfi_cfg.ppb,
			new_buf, total_size);

		dev_dbg(dev, "ret=%d, tag=0x%02x\n", ret, new_buf[snf->nfi_cfg.page_size + 4]);

		if (ret == 0 && new_buf[snf->nfi_cfg.page_size + 4] == BBT_TAG) {
			/* CRC check */
			crc_check = rtk_snand_table_crc_calculate(snf, BBTABLE, new_buf + 4, NULL);
			if (crc_check == *(u32 *)new_buf) {
				memcpy(snf->bbt, new_buf + CRCLEN,
					sizeof(struct bb_table) * snf->RBA);
				dev_info(dev, "BBT%d found at block %d, CRC check OK.\n",
					i + 1, bbt_blocks[i] * snf->nfi_cfg.ppb);
				rtk_snand_dump_BBT(snf);
				return 0;
			}
			dev_err(dev, "BBT%d CRC fail, calc=0x%08x, stored=0x%08x\n",
				i + 1, crc_check, *(u32 *)new_buf);
		}
	}

	dev_info(dev, "RTK %s(%d) NO BBT1 & NO BBT2.\n", __func__, __LINE__);

	snf->RBA = 0;
	return -ENODEV;
}

static int rtk_snand_bbt_erase(struct rtk_snand *snf, u32 page)
{
	void __iomem *base = snf->regs;
	int ret = 0;

	writel(0x1, base + REG_SPI_MASK);
        writel(0x0, base + REG_SPI_HIT);
	writel(0xc0, base + REG_SPI_SA);

	writel(page & 0xff, base + REG_ND_PA0);
        writel(page >> 8 & 0xff, base + REG_ND_PA1);
        writel(0x1f & (page >> 16), base + REG_ND_PA2);

	writel(0x0, base + REG_ND_CA0);
	writel(0x0, base + REG_ND_CA1);

	writel(0xc0, base + REG_SPI_SA);
	writel(0x0, base + REG_SPI_CMD1);
	writel((0xd | 0x1 << 4), base + REG_SPI_CTRL0);

	if ((ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0))) {
		pr_err("busy waiting bbt erase SPI_CTRL0 done...\n");
		return -ETIMEDOUT;
	}

	if ((readl(base + REG_SPI_STATUS) & 0x4) != 0) {
		pr_err("busy waiting bbt erase SPI_status done...\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int rtk_snand_write_bbt_page(struct rtk_snand *snf, int page, u8 *p)
{
	void __iomem *base = snf->regs;
        int ret = 0;
        dma_addr_t buf_dma = snf->dataPhys;
	dma_addr_t oobPhy = snf->dataPhys + snf->nfi_cfg.page_size;

	if (p && p != snf->dataBuf)
		memcpy(snf->dataBuf, p, snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size);

        writel(0x1, base + REG_SPI_MASK);
        writel(0x0, base + REG_SPI_HIT);

        // enable random mode
        writel(0x1, base + REG_RND_EN);
	writel(0, base + REG_RND_DATA_STR_COL_H);
        writel(snf->nfi_cfg.page_size >> 8, base + REG_RND_SPR_STR_COL_H);
        writel(snf->nfi_cfg.page_size & 0xff, base + REG_RND_SPR_STR_COL_L);

        // set PA and CA
	writel(page & 0xff, base + REG_ND_PA0);
        writel((page >> 8) & 0xff, base + REG_ND_PA1);
        writel((page >> 16) & 0x1f, base + REG_ND_PA2);

        writel(0x0, base + REG_ND_CA0);
        writel(0x0, base + REG_ND_CA1);

        // set transfer size
        writel(0x2, base + REG_DATA_TL1);
        writel(snf->nfi_cfg.page_size / 0x200, base + REG_PAGE_LEN);

	// set spare dma addr
	writel((0x1 << 30) , base + REG_SPR_DDR_CTL);
	writel((u32)(oobPhy >> 3), base + REG_SPR_DDR_CTL2);

        // set data dma addr
        writel((uintptr_t)(buf_dma >> 3), base + REG_DMA_CTL1);
        writel((snf->nfi_cfg.page_size >> 9) & 0x0000FFFF, base + REG_DMA_CTL2);
	// dma xfer setting
	writel(0x1, base + REG_DMA_CTL3);

	// set PP (read is through PP SRAM)
        writel(0x0, base + REG_READ_BY_PP);
        writel(0x0, base + REG_PP_CTL1);
        writel(0x0, base + REG_PP_CTL0);

        writel(0xc0, base + REG_SPI_SA);
        writel(0x0, base + REG_SPI_CMD1);

        writel(0xb | (0x1 << 4), base + REG_SPI_CTRL0);

        // wait auto trigger done, pp busy done and dma xfer done
        ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
        if (ret) {
                pr_err("still busy waiting bbt SPI_CTRL0 done...\n");
                return ret;
        }

        ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
        if (ret) {
                pr_err("still busy waiting bbt dma xfer done...\n");
                return ret;
        }

	ret = wait_done(base + REG_SPI_STATUS, 0x8, 0);
        if (ret)
                pr_err("still busy waiting bbt SPI STATUS...\n");

	return ret;
}

static unsigned int snand_Reflect(unsigned int ref, unsigned int ch)
{
	unsigned int value = 0;
	int i;
	/* Swap bit 0 for bit 7 */
	/* bit 1 for bit 6, etc. */
	for (i = 1; i < (ch + 1); i++) {
		if (ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}

	return value;
}

static void snand_make_crc_table(void)
{
	unsigned int polynomial = 0x04C11DB7;
	int i, j;

	for (i = 0; i <= 0xFF; i++) {
		crc_table[i] = snand_Reflect(i, 8) << 24;
		for (j = 0; j < 8; j++)
			crc_table[i] = (crc_table[i] << 1) ^ (crc_table[i] &
					(1 << 31) ? polynomial : 0);
		crc_table[i] = snand_Reflect(crc_table[i],  32);
	}

	crc_table_computed = 1;
}

static unsigned int snand_crc32(unsigned char *p, int len, unsigned int *crc)
{
	int cnt = len;
	unsigned int value;

	if (!crc_table_computed)
		snand_make_crc_table();

	value = 0xFFFFFFFF;
	while (cnt--) {
		value = (value >> 8) ^ crc_table[(value & 0xFF) ^ *p++];
	}

	*crc = value ^ 0xFFFFFFFF;

	return 0;
}

static int rtk_snand_table_crc_calculate(struct rtk_snand *snf, int type, void *t, int *cnt)
{
	struct bb_table *bbt;
	struct sb_table *sbt;
	unsigned int hash_value = 0;
	int i;

	if (type == BBTABLE) {
		bbt = (struct bb_table *)t;
		if (cnt)
			*cnt = 0;

		for (i = 0; i < snf->RBA; i++) {
			if ((bbt[i].BB_die != BB_DIE_INIT) &&
					(bbt[i].bad_block != BB_INIT)) {
				if (cnt)
					*cnt = *cnt + 1;
			}
		}

		snand_crc32((unsigned char *)bbt, sizeof(struct bb_table) * snf->RBA,
				&hash_value);
	} else if (type == SBTABLE) {
		sbt = (struct sb_table *)t;
		snand_crc32((unsigned char *)sbt, sizeof(struct sb_table) * 16, &hash_value);
	}
	return hash_value;
}

static inline int is_page_empty(const u8 *buf, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (buf[i] != 0xFF)
			return 0;
	}

	return 1;
}

static void rtk_snand_update_BBT(struct rtk_snand *snf, int o_blk,
                                 int s_blk, int m_blk)
{
	unsigned int erase_size = snf->nfi_cfg.page_size * snf->nfi_cfg.ppb;
	unsigned int total_blks = (unsigned int)(snf->nfi_cfg.chip_size / erase_size);
	unsigned int bbt_base = total_blks - BLOCKINFO;
	unsigned int idx_m, idx_s;

	if ((unsigned int)m_blk >= bbt_base) {
		dev_err(snf->dev, "BBM: m_blk %d out of range\n", m_blk);
		return;
	}

	idx_m = (bbt_base - 1) - m_blk;
	if (idx_m >= (unsigned int)snf->RBA) {
		dev_err(snf->dev, "BBM: bbt index %u out of range (RBA=%d)\n", idx_m, snf->RBA);
		return;
	}

	if (s_blk == 0) {
		snf->bbt[idx_m].bad_block = BAD_RESERVED;
		snf->bbt[idx_m].BB_die = 0;
	} else {
		if (o_blk != s_blk) {
			if ((unsigned int)s_blk >= bbt_base) {
				dev_err(snf->dev, "BBM: s_blk %d out of range\n", s_blk);
				return;
			}

			idx_s = (bbt_base - 1) - s_blk;
			if (idx_s >= (unsigned int)snf->RBA) {
				dev_err(snf->dev, "BBM: bbt index %u out of range (RBA=%d)\n", idx_s, snf->RBA);
				return;
			}
			snf->bbt[idx_s].bad_block = BAD_RESERVED;
			snf->bbt[idx_s].BB_die = 0;
		}

		snf->bbt[idx_m].bad_block = o_blk;
		snf->bbt[idx_m].BB_die = 0;
	}
}

static int rtk_snand_backup_block(struct rtk_snand *snf, int src_blk, int map_blk,
				  int offset, u8 *p)
{
	unsigned int dest_start_page = map_blk * snf->nfi_cfg.ppb;
	unsigned int src_start_page = src_blk * snf->nfi_cfg.ppb;
	unsigned int i;
	u8 *tmp_buf = snf->dataBuf;
	int ret = 0;

	ret = rtk_snand_bbt_erase(snf, dest_start_page);
	if (ret < 0) {
		dev_err(snf->dev, "erase block %d failed\n", map_blk);
		return ret;
	}

	for (i = 0; i < snf->nfi_cfg.ppb; i++) {
		if (i == offset) {
			ret = rtk_snand_write_bbt_page(snf, dest_start_page + i, p);
		} else {
			ret = rtk_snand_read_bbt_page(snf, src_start_page + i, tmp_buf, snf->nfi_cfg.page_size);
			if (ret < 0) {
				dev_warn(snf->dev, "BBM: skip read fail at src_page %d\n", src_start_page + i);
				ret = 0;
				continue;
			}

			/* check if page is empty (0xFF) - skip write*/
			if (is_page_empty(tmp_buf, snf->nfi_cfg.page_size))
				continue;

			ret = rtk_snand_write_bbt_page(snf, dest_start_page + i, tmp_buf);
			if (ret < 0) {
				dev_err(snf->dev, "BBM: backup write fail at page %d\n", dest_start_page + i);
				break;
			}
		}
	}
	return ret;
}

static int rtk_snand_write_bbt_to_flash(struct rtk_snand *snf, unsigned int page)
{
	struct device *dev = snf->dev;
	u8 *tmp_buf = snf->dataBuf;
	u8 *oob_ptr;
	u32 crc;
	int ret;

	ret = rtk_snand_bbt_erase(snf, page);
	if (ret < 0) {
		dev_err(dev, "BBM: Erase BBT block at page %u failed\n", page);
		return ret;
	}

	memset(tmp_buf, 0xff, snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size);

	if (CRCLEN) {
		crc = rtk_snand_table_crc_calculate(snf, BBTABLE, (u8 *)snf->bbt, NULL);
		memcpy(tmp_buf, &crc, CRCLEN);
	}

	memcpy(tmp_buf + CRCLEN, snf->bbt, sizeof(struct bb_table) * snf->RBA);

	oob_ptr = tmp_buf + snf->nfi_cfg.page_size;
	*(oob_ptr + TAGOFFSET) = BBT_TAG;

	ret =  rtk_snand_write_bbt_page(snf, page, tmp_buf);
	if (ret < 0) {
		dev_err(dev, "BBM: Write BBT failed at page %u\n", page);
		return ret;
	}

	return 0;
}

static int rtk_snand_write_bbt(struct rtk_snand *snf, u8 *buf)
{
	int ret1, ret2;
	unsigned int ppb = snf->nfi_cfg.ppb;

	ret1 = rtk_snand_write_bbt_to_flash(snf, BBT1 * ppb);
	ret2 = rtk_snand_write_bbt_to_flash(snf, BBT2 * ppb);

	return ((ret1 < 0) && (ret2 < 0)) ? -1 : 0;
}

static int rtk_snand_find_available_reserved_block(struct rtk_snand *snf)
{
	int i;

	for (i = 0; i < snf->RBA; i++) {
		if (snf->bbt[i].bad_block == BB_INIT)
			return snf->bbt[i].remap_block;
	}

	dev_err(snf->dev, "RTK %s(%d) No available reserved block.\n",
			__func__, __LINE__);
	return -ENOSPC;
}

static int rtk_snand_bb_handle(struct rtk_snand *snf, int orig_blk, int real_page,
			int backup, u8 *p, int mode)
{
	struct device *dev = snf->dev;
	unsigned int ppb = snf->nfi_cfg.ppb;
	unsigned int src_blk = real_page / ppb;
	unsigned int page_offset = real_page % ppb;
	int dest_blk = -1;
	int ret = 0;
	int retry_limit = 3;

	while (retry_limit--) {
		dest_blk = rtk_snand_find_available_reserved_block(snf);
		if (dest_blk < 0) {
			dev_err(snf->dev, "RTK SNAND: No more reserved blocks available!\n");
			return -ENOSPC;
		}

		if (backup && dest_blk > 0) {
			ret = rtk_snand_backup_block(snf, src_blk, dest_blk,
				page_offset, p);
			if (ret < 0) {
				dev_err(dev, "RTK SNAND: Reserved block %d is also bad, retrying...\n", dest_blk);
				rtk_snand_update_BBT(snf, 0, 0, dest_blk);
				continue;
			}
		}

		rtk_snand_update_BBT(snf, orig_blk, src_blk, dest_blk);

		ret = rtk_snand_write_bbt(snf, p);
		if (ret < 0) {
			dev_err(snf->dev, "BBM: Fatal error, could not save any BBT to flash!\n");
			return -EIO;
		}

		dev_info(snf->dev, "BBM: Remapped blk %d -> %d (Mode: %d)\n", src_blk, dest_blk, mode);
		return 0;
	}

	return -EIO;
}

static int rtk_snand_check_badblock_table(struct rtk_snand *snf, unsigned int blk)
{
	int r_blk = blk;
	int i;

	for (i = 0; i < snf->RBA; i++) {
		if (blk == snf->bbt[i].bad_block) {
			r_blk = snf->bbt[i].remap_block;
			blk = r_blk;
		}
	}
	return r_blk;
}

static int rtk_snand_check_shift_table(struct rtk_snand *snf, unsigned int blk)
{
	int r_blk = blk;
	int i;

	if (!snf->sbt)
		return r_blk;

	for (i = 0; i < SBTCNT; i++) {
		if (snf->sbt[i].chipnum != SB_INIT) {
			if ((r_blk >= snf->sbt[i].block))
				r_blk = blk + snf->sbt[i].shift;
		} else {
                        break;
		}
	}
	return r_blk;
}

static unsigned int rtk_snand_page_to_block(struct rtk_snand *snf, int page)
{
	return page / snf->nfi_cfg.ppb;
}

static unsigned int rtk_snand_page_offset_in_block(struct rtk_snand *snf, int page)
{
	return page % snf->nfi_cfg.ppb;
}

static int rtk_snand_get_real_page(struct rtk_snand *snf, int page, int mode)
{
	int offset, block; /* bootblk; */

	offset = rtk_snand_page_offset_in_block(snf, page);
	block = rtk_snand_page_to_block(snf, page);

	if (mode == SBTABLE) {
		if (snf->sbt)
			block = rtk_snand_check_shift_table(snf, block);
	} else {
		if (snf->bbt)
			block = rtk_snand_check_badblock_table(snf, block);
	}

	return (block * snf->nfi_cfg.ppb) + offset;
}

static int rtk_snand_read_fdm(struct rtk_snand *snf, u8 *buf)
{
	void __iomem *base = snf->regs;
	u32 val;
	u8 *tbuf;
	int i;
	int eccstep_size = (snf->nfi_cfg.ecc == 0x1) ? 32 : 16;
	int oobuse_size = (snf->nfi_cfg.ecc == 0x1) ? (6 + 20) : (6 + 10);
	int eccstep = snf->nfi_cfg.page_size / SZ_512;

	tbuf = kmalloc(snf->nfi_cfg.oob_size, GFP_KERNEL);
	if (!tbuf) {
		dev_err(snf->dev, "Failed to allocate temp OOB buffer\n");
		return -ENOMEM;
	}

	writel(0x0, base + REG_READ_BY_PP);
	writel(0x32, base + REG_SRAM_CTL);

	memset(tbuf, 0xFF, snf->nfi_cfg.oob_size);
	memset(buf, 0xFF, snf->nfi_cfg.oob_size);

	for (i = 0; i < (snf->nfi_cfg.oob_size / 4); i++) {
		val = readl(base + (i * 4));

		tbuf[(i * 4)] = val & 0xff;
		tbuf[(i * 4) + 1] = (val >> 8) & 0xff;
		tbuf[(i * 4) + 2] = (val >> 16) & 0xff;
		tbuf[(i * 4) + 3] = (val >> 24) & 0xff;
	}

	for (i = 0; i < eccstep; i++)
		memcpy(buf + (i * oobuse_size), tbuf + (i * eccstep_size), oobuse_size);

	writel(0x0, base + REG_SRAM_CTL);
	writel(0x80, base + REG_READ_BY_PP);

	kfree(tbuf);
	return 0;
}

static int rtk_snand_get_mapping_page(struct rtk_snand *snf, int page)
{
        return rtk_snand_get_real_page(snf, page, BBTABLE);
}

static int rtk_snand_get_physical_page(struct rtk_snand *snf, int page)
{
        return rtk_snand_get_real_page(snf, page, SBTABLE);
}

static int rtk_snand_read_page_cache(struct rtk_snand *snf,
				     const struct spi_mem_op *op)
{
	void __iomem *base = snf->regs;
	u8 *oobbuf = snf->buf + snf->nfi_cfg.page_size;
	u32 op_addr = snf->offs;
	u32 page_size = snf->nfi_cfg.page_size;
	u32 oob_size = snf->nfi_cfg.oob_size;
	int ret = 0;
	int phy_page = rtk_snand_get_physical_page(snf, op_addr);
	int real_page = rtk_snand_get_mapping_page(snf, phy_page);
	unsigned int blank_check = 0;
	unsigned int eccNum = 0;

	memset(snf->dataBuf, 0xFF, page_size + oob_size);
	op_addr = real_page;

	pr_debug("############# rtk_snand_read_page_cache, op_addr = %d, len = %d\n",
			op_addr, op->data.nbytes);

	writel(0x41, base + REG_SPI_MASK);
	writel(0x0, base + REG_SPI_HIT);

	// enable random mode
	writel(0x1, base + REG_RND_EN);
        writel(0, base + REG_RND_DATA_STR_COL_H);
        writel(page_size >> 8, base + REG_RND_SPR_STR_COL_H);
        writel(page_size & 0xff, base + REG_RND_SPR_STR_COL_L);

	// enable blank check
	writel(0x1, base + REG_BLANK_CHK);
	writel(snf->ecc_threshold, base + REG_BLANK_ZERO_NUM);

	// set PA and CA
	writel(op_addr & 0xff, base + REG_ND_PA0);
	writel((op_addr >> 8) & 0xff, base + REG_ND_PA1);
	writel((op_addr >> 16) & 0x1f, base + REG_ND_PA2);
	writel(((op_addr >> 21)& 0x7) << 5, base + REG_ND_PA3);

	if (snf->nfi_cfg.plane) {
		u32 block_addr = op_addr / snf->nfi_cfg.ppb;
		u32 plane_addr = block_addr % 2;
		u32 column_addr = 0;
		int final_shift = ilog2(page_size) + 1;

		if (snf->nfi_cfg.page_size == SZ_2K) {
			writel(0x0, base + REG_ND_CA0);
			writel(0x10, base + REG_ND_CA1);
			writel(0x10 | 0x8, base + REG_RND_SPR_STR_COL_H);
			writel(0x10, base + REG_RND_DATA_STR_COL_H);
		} else {
			column_addr = plane_addr << final_shift;
			writel(column_addr & 0xff, base + REG_ND_CA0);
			writel((column_addr >> 8) & 0xff, base + REG_ND_CA1);
			writel((readl(base + REG_RND_DATA_STR_COL_H) | ((column_addr >> 8) & 0xff)),
				base + REG_RND_DATA_STR_COL_H);
			writel((readl(base + REG_RND_SPR_STR_COL_H) | ((column_addr >> 8) & 0xff)),
				base + REG_RND_SPR_STR_COL_H);

			pr_debug("RTK-READ-DEBUG: page_addr=0x%x, block=%u, plane=%u, col_addr=0x%x\n",
				op_addr, block_addr, plane_addr, column_addr);
		}
	} else {
		writel(0x0, base + REG_ND_CA0);
		writel(0x0, base + REG_ND_CA1);
	}

	// set transfer size
	writel(0x200 & 0xff, base + REG_DATA_TL0);
	writel(0x82, base + REG_DATA_TL1);
	writel(snf->nfi_cfg.page_size / 0x200, base + REG_PAGE_LEN);

	// set spare dma addr
	writel(0x0, base + REG_SPR_DDR_CTL);

	// set data dma addr
	writel((u32)(snf->dataPhys >> 3), base + REG_DMA_CTL1);
	writel((page_size >> 9), base + REG_DMA_CTL2);

	// dma xfer setting
	writel(0x3, base + REG_DMA_CTL3);

	// set PP (read is through PP SRAM)
	writel(0x80, base + REG_READ_BY_PP);
	writel(0x0, base + REG_PP_CTL1);
	writel(0x0, base + REG_PP_CTL0);

	writel(0xc0, base + REG_SPI_SA);
	writel(0x0, base + REG_SPI_CMD1);

	writel(0x9 | (0x1 << 4), base + REG_SPI_CTRL0);
	writel(0x83, base + REG_SPI_CTRL1);

	// wait auto trigger done, pp busy done and dma xfer done
	ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
	if (ret) {
		pr_err("still busy waiting SPI_CTRL0 done...\n");
		goto read_page_exit;
	}

	ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
	if (ret) {
		pr_err("still busy waiting dma xfer done...\n");
		goto read_page_exit;
	}

	ret = rtk_snand_read_fdm(snf, oobbuf);
	if (ret)
		goto read_page_exit;

	blank_check = readl(base + REG_BLANK_CHK);
	eccNum = readl(base + REG_MAX_ECC_NUM) & 0xff;

	if (blank_check & 0x2) {
		ret = 0;
		dev_dbg(snf->dev, "RTK %s: all one page\n", __func__);
		memset(op->data.buf.in, 0xFF, op->data.nbytes);
		goto read_page_exit;
	} else if (readl(base + REG_ND_ECC) & 0x10) {
		dev_err(snf->dev, "RTK %s(%d) un-correct ecc error ... page:[%d]\n",
				__func__, __LINE__, op_addr);
		ret = -EBADMSG;
		goto read_page_exit;
	} else {
		if (eccNum > snf->ecc_threshold) {
			dev_warn(snf->dev, "RTK %s ecc over threshold, eccNum = %d\n",
				__func__, eccNum);
		}
		ret = eccNum;
	}

	if (op->data.nbytes >= page_size) {
		u32 copy_oob = op->data.nbytes - page_size;

		memcpy(op->data.buf.in, snf->dataBuf, page_size);

		if (copy_oob > oob_size)
			copy_oob = oob_size;

		if (copy_oob > 0)
			memcpy(op->data.buf.in + page_size, oobbuf, copy_oob);
	} else if (op->data.nbytes == oob_size) {
		memcpy(op->data.buf.in, oobbuf, oob_size);
	}

read_page_exit:
	writel(0x1, base + REG_BLANK_CHK);

	return ret;
}

static int rtk_snand_read_oob(struct rtk_snand *snf, const struct spi_mem_op *op)
{
	int ret = 0;
	int phy_page, real_page, o_blk;
	u8 *p_data = (u8 *)op->data.buf.in;

	ret = rtk_snand_read_page_cache(snf, op);

	if (ret < 0 || ret > snf->ecc_threshold) {
		dev_err(snf->dev, "RTK SNAND: read oob fail at offset 0x%llx\n", snf->offs);

		phy_page = rtk_snand_get_physical_page(snf, snf->offs);
		real_page = rtk_snand_get_mapping_page(snf, phy_page);
		o_blk = phy_page / snf->nfi_cfg.ppb;

		if (!rtk_snand_bb_handle(snf, o_blk, real_page, 1, p_data, SNF_READ)) {
			ret = 0;
		}
	}

	return ret;
}

static int rtk_snand_write_page_cache(struct rtk_snand *snf,
				      const struct spi_mem_op *op)
{
	u32 ps = snf->nfi_cfg.page_size;
	u32 os = snf->nfi_cfg.oob_size;
	u32 nbytes = op->data.nbytes;
	u32 data_len = (nbytes > ps) ? ps : nbytes;
	int ecc_steps;
	int spare_per_sector;
	int i;

	if (!ps || !os || !op->data.buf.out || !snf->w_dbuf || !snf->w_obuf)
		return -EINVAL;

	ecc_steps = ps / 512;
	if (!ecc_steps)
		return -EINVAL;

	spare_per_sector = os / ecc_steps;

	pr_debug("data_write: nbytes = %d\n", op->data.nbytes);

	memset(snf->w_dbuf, 0xFF, ps);
	memcpy(snf->w_dbuf, op->data.buf.out, data_len);
	memset(snf->w_obuf, 0xFF, os);

	if (nbytes > ps) {
		const u8 *oob_src = (const u8 *)op->data.buf.out + ps;
		u32 oob_src_len = nbytes - ps;

		if (oob_src_len < (u32)(ecc_steps * 6))
			return -EINVAL;

		for (i = 0; i < ecc_steps; i++)
			memcpy(snf->w_obuf + (i * spare_per_sector),
				oob_src + (i * 6), 6);
	}

	return 0;
}

static int rtk_snand_progarm_load(struct rtk_snand *snf,
				  const struct spi_mem_op *op)
{
	void __iomem *base = snf->regs;
	u32 op_addr = op->addr.val;
	int phy_page = rtk_snand_get_physical_page(snf, op_addr);
	int real_page = rtk_snand_get_mapping_page(snf, phy_page);
	int o_blk = phy_page / snf->nfi_cfg.ppb;
	int ret = 0;

	op_addr = real_page;
	pr_debug("############# rtk_snand_progarm_load, op_addr = %d\n", op_addr);

	writel(0x1, base + REG_SPI_MASK);
        writel(0x0, base + REG_SPI_HIT);

	// enable random mode
	writel(0x1, base + REG_RND_EN);
	writel(0, base + REG_RND_DATA_STR_COL_H);
	writel((snf->nfi_cfg.page_size >> 8) & 0xff, base + REG_RND_SPR_STR_COL_H);
	writel(snf->nfi_cfg.page_size & 0xff, base + REG_RND_SPR_STR_COL_L);

        // set PA and CA
        writel(op_addr & 0xff, base + REG_ND_PA0);
        writel((op_addr >> 8) & 0xff, base + REG_ND_PA1);
        writel((op_addr >> 16) & 0x1f, base + REG_ND_PA2);
	writel(((op_addr >> 21) & 0x7) << 5, base + REG_ND_PA3);

	if (snf->nfi_cfg.plane) {
		u32 block_addr = op_addr / snf->nfi_cfg.ppb;
		u32 plane_addr = block_addr % 2;
		u32 column_addr = 0;
		int final_shift = ilog2(snf->nfi_cfg.page_size) + 1;

		if (snf->nfi_cfg.page_size == 2048) {
			writel(0x0, base + REG_ND_CA0);
			writel(0x10, base + REG_ND_CA1);
			writel(0x10 | 0x8, base + REG_RND_SPR_STR_COL_H);
			writel(0x10, base + REG_RND_DATA_STR_COL_H);
		} else {
			column_addr = plane_addr << final_shift;
			writel(column_addr & 0xff, base + REG_ND_CA0);
			writel((column_addr >> 8) & 0xff, base + REG_ND_CA1);
			writel((readl(base + REG_RND_DATA_STR_COL_H) | ((column_addr >> 8) & 0xff)),
				base + REG_RND_DATA_STR_COL_H);
			writel((readl(base + REG_RND_SPR_STR_COL_H) | ((column_addr >> 8) & 0xff)),
				base + REG_RND_SPR_STR_COL_H);

			pr_debug("RTK-WRITE-DEBUG: page_addr=0x%x, block=%u, plane=%u, col_addr=0x%x\n",
				op_addr, block_addr, plane_addr, column_addr);
		}
	} else {
		writel(0x0, base + REG_ND_CA0);
		writel(0x0, base + REG_ND_CA1);
        }

	// set transfer size
	writel(0x200 & 0xff, base + REG_DATA_TL0);
	writel((0x200 >> 8) & 0x3f, base + REG_DATA_TL1);
	writel(snf->nfi_cfg.page_size / 0x200, base + REG_PAGE_LEN);

	// set spare dma addr
	writel((0x1 << 30), base + REG_SPR_DDR_CTL);
	writel((u32)(snf->obuf_dma >> 3), base + REG_SPR_DDR_CTL2);

        // set data dma addr
	writel((u32)(snf->dbuf_dma >> 3), base + REG_DMA_CTL1);
	writel(snf->nfi_cfg.page_size / 0x200, base + REG_DMA_CTL2);

	// set PP (read is through PP SRAM)
	writel(0x0, base + REG_READ_BY_PP);
	writel(0x0, base + REG_PP_CTL1);
	writel(0x0, base + REG_PP_CTL0);

        // dma xfer setting
        writel(0x1, base + REG_DMA_CTL3);
	wmb();

        writel(0xc0, base + REG_SPI_SA);
        writel(0x0, base + REG_SPI_CMD1);

	writel((0xb << 0) | (0x1 << 4), base + REG_SPI_CTRL0);

        // wait auto trigger done, pp busy done and dma xfer done
        ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
	if (ret) {
                pr_err("still busy waiting SPI_CTRL0 done...\n");
		goto handle_bad_block;
	}

	ret = wait_done(base + REG_PP_CTL0, (0x1 << 2), 0);
	if (ret) {
		pr_err("still busy waiting pp ctl0 done...\n");
		goto handle_bad_block;
	}

	ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
	if (ret) {
		pr_err("still busy waiting dma xfer done...\n");
		goto handle_bad_block;
	}

	ret = wait_done(base + REG_SPI_STATUS, 0x8, 0);
	if (ret) {
		pr_err("still busy waiting SPI STATUS...\n");
		goto handle_bad_block;
	}

	pr_debug("REG_DMA_ADR (0x304) = 0x%08x\n", readl(base + 0x304));
	pr_debug("REG_DMA_CONF(0x30c) = 0x%08x\n", readl(base + 0x30c));

	return 0;

handle_bad_block: {
	u32 ps = snf->nfi_cfg.page_size;
	u32 os = snf->nfi_cfg.oob_size;
	u8 *p_data = kmalloc(ps + os, GFP_KERNEL);

	if (p_data) {
		memcpy(p_data, snf->w_dbuf, ps);
		memcpy(p_data + ps, snf->w_obuf, os);
		if (!rtk_snand_bb_handle(snf, o_blk, real_page, 1, p_data, SNF_WRITE))
			ret = 0;
		kfree(p_data);
	}
	return ret;
}
}

static int rtk_snand_write_oob(struct rtk_snand *snf, const struct spi_mem_op *op)
{
	return rtk_snand_write_page_cache(snf, op);
}

static bool rtk_snand_is_page_ops(const struct spi_mem_op *op)
{
	if (op->addr.nbytes != 2)
	    return false;

	if (op->addr.buswidth != 1 && op->addr.buswidth != 2 &&
	    op->addr.buswidth != 4)
		return false;

	// match read from page instructions
	if (op->data.dir == SPI_MEM_DATA_IN) {
		// check dummy cycle first
		if (op->dummy.nbytes * BITS_PER_BYTE / op->dummy.buswidth > 0xf)
			return false;
		// quad io / quad out
		if ((op->addr.buswidth == 4 || op->addr.buswidth == 1) &&
		    op->data.buswidth == 4)
			return true;

		// dual io / dual out
		if ((op->addr.buswidth == 2 || op->addr.buswidth == 1) &&
		    op->data.buswidth == 2)
			return true;

		// standard spi
		if (op->addr.buswidth == 1 && op->data.buswidth == 1)
			return true;
	} else if (op->data.dir == SPI_MEM_DATA_OUT) {
		// check dummy cycle first
		if (op->dummy.nbytes)
			return false;
		// program load quad out
		if (op->addr.buswidth == 1 && op->data.buswidth == 4)
			return true;
		// standard spi
		if (op->addr.buswidth == 1 && op->data.buswidth == 1)
			return true;
	}
	return false;
}

static bool rtk_snand_supports_op(struct spi_mem *mem,
				  const struct spi_mem_op *op)
{
	if (!spi_mem_default_supports_op(mem, op))
		return false;
	if (op->cmd.nbytes != 1 || op->cmd.buswidth != 1)
		return false;
	if (rtk_snand_is_page_ops(op))
		return true;
	return ((op->addr.nbytes == 0 || op->addr.buswidth == 1) &&
		(op->dummy.nbytes == 0 || op->dummy.buswidth == 1) &&
		(op->data.nbytes == 0 || op->data.buswidth == 1));
}

static int rtk_snf_erase_block(struct rtk_snand *snf, const struct spi_mem_op *op)
{
	void __iomem *base = snf->regs;
	u32 op_addr = op->addr.val;
	int phy_page = rtk_snand_get_physical_page(snf, op_addr);
        int real_page = rtk_snand_get_mapping_page(snf, phy_page);
	int o_blk = phy_page / snf->nfi_cfg.ppb;
	int ret = 0;

	writel(0x1, base + REG_SPI_MASK);
	writel(0x0, base + REG_SPI_HIT);
	writel(0xc0, base + REG_SPI_SA);

	writel(real_page & 0xff, base + REG_ND_PA0);
	writel((real_page >> 8) & 0xff, base + REG_ND_PA1);
	writel(0x1f & ((real_page) >> 16), base + REG_ND_PA2);

	writel(0x0, base + REG_ND_CA0);
	writel(0x0, base + REG_ND_CA1);

	writel(0xc0, base + REG_SPI_SA);
	writel(0x0, base + REG_SPI_CMD1);
	writel((0xd | 0x1 << 4), base + REG_SPI_CTRL0);

	if ((ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0))) {
		pr_err("wait Erase spi trigger fail\n");
		ret = -EIO;
		goto handle_bad_block;
	}

	if((readl(base + REG_SPI_STATUS) & 0x4) != 0) {
		pr_err("Erase fail\n");
		ret = -EIO;
		goto handle_bad_block;
	}

	return 0;

handle_bad_block:
	if (!rtk_snand_bb_handle(snf, o_blk, real_page, 0, NULL, SNF_ERASE)) {
		ret = 0;
	}
	return ret;
}

static int rtk_snand_io_op(struct rtk_snand *snf, const struct spi_mem_op *op)
{
	void __iomem *base = snf->regs;
	int ret = 0;
	u16 cmd = op->cmd.opcode;
	const u8 *tx_buf = NULL;
	u8 *in_buf = op->data.buf.in;

	if (op->data.dir != SPI_MEM_DATA_IN)
		tx_buf = op->data.buf.out;

	switch(cmd) {
	case SPINAND_READID:
		writel((readl(base + REG_SPI_CTRL1) & ~(0x3 << 6)) | (0x3 << 6), base + REG_SPI_CTRL1);
		writel(0x5 | (0x1 << 4), base + REG_SPI_CTRL0);
		ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
		if (ret) {
			pr_err("SPINAND_READID timeout...\n");
			return -ETIMEDOUT;
		}

		if (in_buf) {
			memcpy_fromio(in_buf, (unsigned char *)base + REG_SPI_ID0, 1);
			memcpy_fromio(in_buf + 1, (unsigned char *)base + REG_SPI_ID1, 1);
			memcpy_fromio(in_buf + 2, (unsigned char *)base + REG_SPI_ID2, 1);
			pr_debug("id = 0x%02x%02x%02x\n", in_buf[0], in_buf[1], in_buf[2]);
		}
		break;
	case SPINAND_ERASE:
		ret = rtk_snf_erase_block(snf, op);
		break;
	case SPINAND_PROGRAM_EXECUTE:
		ret = rtk_snand_progarm_load(snf, op);
		break;
	case SPINAND_WRITE_ENABLE:
                writel(cmd, base + REG_SPI_CMD1);
                writel((0x2 | 0x1 << 4), base + REG_SPI_CTRL0);
                ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
		if (ret) {
			pr_err("SPINAND_WRITE_ENABLE timeout...\n");
			return -ETIMEDOUT;
                }
		break;
	case SPINAND_SET_FEATURE:
		writel(0x0, base + REG_SPI_MASK);
		writel(0x0, base + REG_SPI_HIT);
		writel(op->addr.val, base + REG_SPI_SA);
		writel(tx_buf[0], base + REG_SPI_SDATA);
		writel(0x7 | (0x1 << 4), base + REG_SPI_CTRL0);
		ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
		if (ret) {
			pr_err("SPINAND_SET_FEATURE timeout...\n");
			return -ETIMEDOUT;
                }
		break;
	case SPINAND_GET_FEATURE:
		writel(0x1, base + REG_SPI_MASK);
		writel(0x0, base + REG_SPI_HIT);
		writel(op->addr.val, base + REG_SPI_SA);
		writel(0x8 | (0x1 << 4), base + REG_SPI_CTRL0);
		ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
		if (ret) {
			pr_err("SPINAND_GET_FEATURE timeout...\n");
			return -ETIMEDOUT;
		}
                break;
	case SPINAND_RESET:
		writel(cmd, base + REG_SPI_CMD1);
		writel(0x11, base + REG_SPI_CTRL0);
		ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
		if (ret) {
			pr_err("SPINAND_RESET timeout...\n");
			return -ETIMEDOUT;
                }
		mdelay(2);
		break;
	case SPINAND_PAGE_READ:
		snf->offs = op->addr.val;
		break;
	default:
		break;
	}
	return ret;
}

static int rtk_snand_exec_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	struct rtk_snand *snf = spi_controller_get_devdata(mem->spi->controller);

	pr_debug("OP %02x,  ADDR %08llX@%d:%u DATA %d:%u", op->cmd.opcode,
		op->addr.val, op->addr.buswidth, op->addr.nbytes,
		op->data.buswidth, op->data.nbytes);

	if (rtk_snand_is_page_ops(op)) {
		if (op->data.dir == SPI_MEM_DATA_IN)
			return rtk_snand_read_oob(snf, op);
		else
			return rtk_snand_write_oob(snf, op);
	}
	return rtk_snand_io_op(snf, op);
}

static const struct spi_controller_mem_ops rtk_snand_mem_ops = {
	//.adjust_op_size = rtk_snand_adjust_op_size,
	.supports_op = rtk_snand_supports_op,
	.exec_op = rtk_snand_exec_op,
};

static const struct spi_controller_mem_caps rtk_snand_mem_caps = {
	.ecc = true,
};

static const struct of_device_id rtk_snand_ids[] = {
	{ .compatible = "realtek,rtd1625-snf" },
	{ .compatible = "realtek,rtd1635-snf" },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_snand_ids);

static unsigned int rtks_set_reg(void __iomem *addr, unsigned int offset, int length, unsigned int value)
{
        unsigned int value1;

        value1 = readl(addr);
        value1 = (value1 & ~((BIT(length) - 1) << offset)) | (value << offset);
        writel(value1, addr);

        return value1;
}

static void rtk_snand_pll_setup(struct rtk_snand *snf)
{
        void __iomem *pll_base = snf->pll_regs;
	void __iomem *regbase;
	unsigned int ssc_div_n;
	unsigned int ssc_div_ext_f;
	u32 sscpll_icp = 1;
	u32 pi_ibselh;
	u32 sscpll_rs;
	unsigned int tmp_val = 0;

        regbase = ioremap(0x980006b0, 0x20);

	//100MHz
	writel(readl(pll_base + 0x0) &~(1 << 1), pll_base);
	udelay(10);
	rtks_set_reg(regbase, 8, 1, 0);

	ssc_div_n = 12;
	tmp_val = (readl(pll_base + 0x8) & 0xffff) | (ssc_div_n << 16);
	writel(tmp_val, pll_base + 0x8);

	ssc_div_ext_f = 6674;
	tmp_val = (readl(pll_base + 0x4) & ~(0x1fff << 13));
	writel(tmp_val | (ssc_div_ext_f << 13), pll_base + 0x4);
	udelay(3);

	writel(6068, regbase + 0xc);
	writel((readl(regbase + 0xc) & ~(0xff << 13)) | (12 << 13), regbase + 0xc);
	writel((readl(regbase + 0x10) & ~(0x1fffff << 0)) | 3048, regbase + 0x10);

	rtks_set_reg(regbase, 0, 1, 1);
	if (ssc_div_n > 30 && ssc_div_n <= 46) {  //222.75~324 MHz
                pi_ibselh = 3;
                sscpll_icp = 2;
                sscpll_rs = 3;
        } else if (ssc_div_n > 19 && ssc_div_n <= 30) {   //148.5~222.75 MHz
                pi_ibselh = 2;
                sscpll_icp = 1;
                sscpll_rs = 3;
        } else if (ssc_div_n > 9 && ssc_div_n <= 19) {     ///81~148.5 MHz
                pi_ibselh = 1;
                sscpll_icp = 1;
                sscpll_rs = 2;
        } else if (ssc_div_n > 4 && ssc_div_n <= 9) {      ///81~148.5 MHz
                pi_ibselh = 0;
                sscpll_icp = 0;
                sscpll_rs = 2;
        } else {
                pr_err("!!! out of range !!!\n");
                pi_ibselh = 2;
                sscpll_icp = 1;
                sscpll_rs = 3;
        }

        writel((readl(pll_base + 0x4) & ~(0xf << 5)) | (sscpll_icp << 5), pll_base + 0x4);
        writel((readl(pll_base + 0x4) & ~(0x7 << 10)) | (sscpll_rs << 10), pll_base + 0x4);
        writel((readl(pll_base + 0x4) & ~(0x3 << 27)) | (pi_ibselh << 27), pll_base + 0x4);
	udelay(40);

	rtks_set_reg(regbase, 8, 1, 1);
	rtks_set_reg(regbase, 0, 1, 0);

	writel(readl(pll_base) | (1 << 1), pll_base); //reset pll, phrt0=1
        udelay(20);

	iounmap(regbase);
	return;
}

static int rtk_snand_lk_exact_emulation(struct rtk_snand *snf, unsigned int page_addr, bool oob)
{
	void __iomem *base = snf->regs;;
	void *dbuf_cpu, *obuf_cpu;
	dma_addr_t dbuf_dma, obuf_dma;
	int ret;

	pr_debug("Starting Exact LK-Logic Write (Page: 0x%x) #####\n", page_addr);

	dbuf_cpu = dma_alloc_coherent(snf->dev, snf->nfi_cfg.page_size, &dbuf_dma, GFP_KERNEL);
	obuf_cpu = dma_alloc_coherent(snf->dev, snf->nfi_cfg.oob_size, &obuf_dma, GFP_KERNEL);

	if (!dbuf_cpu || !obuf_cpu) {
		pr_err("[RTK-EMU] Memory allocation failed!\n");
		return -ENOMEM;
	}

	memset(dbuf_cpu, 0x66, snf->nfi_cfg.page_size);
	memset(obuf_cpu, 0xAA, snf->nfi_cfg.oob_size);

	writel(0x1, base + REG_SPI_MASK);
	writel(0x0, base + REG_SPI_HIT);

	 // enable random mode
        writel(0x1, base + REG_RND_EN);
        writel(0, base + REG_RND_DATA_STR_COL_H);
        writel((snf->nfi_cfg.page_size >> 8) & 0xff, base + REG_RND_SPR_STR_COL_H);
        writel(snf->nfi_cfg.page_size & 0xff, base + REG_RND_SPR_STR_COL_L);

	// set PA and CA
	writel(page_addr & 0xff, base + REG_ND_PA0);
        writel((page_addr >> 8) & 0xff, base + REG_ND_PA1);
        writel((page_addr >> 16) & 0x1f, base + REG_ND_PA2);
        writel(((page_addr >> 21) & 0x7) << 5, base + REG_ND_PA3);

	if (snf->nfi_cfg.plane) {
		u32 block_addr = page_addr / snf->nfi_cfg.ppb;
		u32 plane_addr = block_addr % 2;
		u32 column_addr = 0;
		int last_set_bit_pos = ilog2(snf->nfi_cfg.page_size);
		int final_shift = last_set_bit_pos + 1;

                if (snf->nfi_cfg.page_size == 2048) {
                        writel(0x0, base + REG_ND_CA0);
                        writel(0x10, base + REG_ND_CA1);
                        writel(0x10 | 0x8, base + REG_RND_SPR_STR_COL_H);
                        writel(0x10, base + REG_RND_DATA_STR_COL_H);
                } else {
			column_addr = plane_addr << final_shift;
			writel(column_addr & 0xff, base + REG_ND_CA0);
			writel((column_addr >> 8) & 0xff, base + REG_ND_CA1);
			writel((readl(base + REG_RND_DATA_STR_COL_H) | ((column_addr >> 8) & 0xff)),
				base + REG_RND_DATA_STR_COL_H);
			writel((readl(base + REG_RND_SPR_STR_COL_H) | ((column_addr >> 8) & 0xff)),
				base + REG_RND_SPR_STR_COL_H);

			pr_info("RTK-DEBUG: page_addr=0x%x, block=%u, plane=%u, col_addr=0x%x\n",
				page_addr, block_addr, plane_addr, column_addr);
		}
        } else {
                writel(0x0, base + REG_ND_CA0);
                writel(0x0, base + REG_ND_CA1);
        }

        // set transfer size
	writel(0x200 & 0xff, base + REG_DATA_TL0);
	writel((0x200 >> 8) & 0x3f, base + REG_DATA_TL1);
	writel(snf->nfi_cfg.page_size / 0x200, base + REG_PAGE_LEN);

	// set data dma addr
	writel(((u32)dbuf_dma >> 3), base + REG_DMA_CTL1); // REG_DMA_ADR
	writel(snf->nfi_cfg.page_size / 0x200, base + REG_DMA_CTL2);

	if (oob) {
		// set spare dma addr
		writel((0x1 << 30) , base + REG_SPR_DDR_CTL);
		writel(((u32)obuf_dma >> 3), base + REG_SPR_DDR_CTL2);
	}

	// set PP (read is through PP SRAM)
        writel(0x0, base + REG_READ_BY_PP);
        writel(0x0, base + REG_PP_CTL1);
        writel(0x0, base + REG_PP_CTL0);

	// dma xfer setting
        writel(0x1, base + REG_DMA_CTL3);
        wmb();

	writel(0xc0, base + REG_SPI_SA);
        writel(0x0, base + REG_SPI_CMD1);

	writel((0xb << 0) | (0x1 << 4), base + REG_SPI_CTRL0);

	// wait auto trigger done, pp busy done and dma xfer done
        ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
        if (ret) {
                pr_err("still busy waiting SPI_CTRL0 done...\n");
                return ret;
        }

        ret = wait_done(base + REG_PP_CTL0, (0x1 << 2), 0);
        if (ret) {
                pr_err("still busy waiting pp ctl0 done...\n");
                return ret;
        }

        ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
        if (ret) {
                pr_err("still busy waiting dma xfer done...\n");
                return ret;
        }

        ret = wait_done(base + REG_SPI_STATUS, 0x8, 0);
        if (ret) {
                pr_err("still busy waiting SPI STATUS...\n");
                return ret;
        }

	pr_info("REG_DMA_ADR (0x304) = 0x%08x\n", readl(base + 0x304));
	pr_info("REG_DMA_CONF(0x30c) = 0x%08x\n", readl(base + 0x30c));

	dma_free_coherent(snf->dev, snf->nfi_cfg.page_size, dbuf_cpu, dbuf_dma);
	dma_free_coherent(snf->dev, snf->nfi_cfg.oob_size, obuf_cpu, obuf_dma);

	return 0;
}

static ssize_t rtk_snand_debug_write(struct file *file, const char __user *user_buf,
                                   size_t count, loff_t *ppos)
{
	struct rtk_snand *snand = file->private_data;
	u32 target_page;
	char buf[16];
	int ret;

	if (count == 0)
		return 0;
	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';

	ret = kstrtou32(buf, 0, &target_page);
	if (ret) {
		pr_err("Invalid input: %s\n", buf);
		return ret;
	}

	pr_debug("Triggering LK-Exact Emulation... #####\n");
	rtk_snand_lk_exact_emulation(snand, target_page, 1);

	return count;
}

static const struct file_operations rtk_snand_debug_fops = {
    .write = rtk_snand_debug_write,
    .open  = simple_open,
};

static int rtk_snand_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device *child;
	const struct of_device_id *dev_id;
	struct spi_controller *ctlr;
	struct resource *res;
	struct rtk_snand *snf;
	int ret;
	void __iomem *regbase1;
	struct dentry *root;
	bool nand_found = false;

	dev_id = of_match_node(rtk_snand_ids, np);
	if (!dev_id)
		return -EINVAL;

	regbase1 = ioremap(0x9804f054, 0x4);
	writel(0x0, regbase1);

	ctlr = devm_spi_alloc_master(&pdev->dev, sizeof(*snf));
	if (!ctlr)
		return -ENOMEM;

	snf = spi_controller_get_devdata(ctlr);

	snf->ctlr = ctlr;
	snf->dev = dev;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dev, "failed to set DMA mask\n");
		return ret;
	}

	snf->clk_nand = devm_clk_get(&pdev->dev, "nand");
	if (IS_ERR(snf->clk_nand)) {
		dev_err(dev, "%s: clk_get() returns %ld\n", __func__,
			PTR_ERR(snf->clk_nand));
		return -EINVAL;
	}

	clk_prepare_enable(snf->clk_nand);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	snf->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(snf->regs)) {
		ret = PTR_ERR(snf->regs);
		goto err_disable_clk;
	}
	pr_debug("NAND Physical Base: %pa\n", &res->start);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	snf->pll_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(snf->pll_regs)) {
		ret = PTR_ERR(snf->pll_regs);
		dev_err(dev, "no pll reg base\n");
		goto err_disable_clk;
	}

	rtk_snand_pll_setup(snf);
	platform_set_drvdata(pdev, ctlr);

	ret = rtk_snand_setup_pagefmt(snf, SZ_2K, SZ_64);
	if (ret) {
		dev_err(snf->dev, "failed to set initial page format\n");
		return -EINVAL;
	}

	// setup ECC engine
	snf->ecc_eng.dev = &pdev->dev;
	snf->ecc_eng.integration = NAND_ECC_ENGINE_INTEGRATION_PIPELINED;
	snf->ecc_eng.ops = &rtk_snf_ecc_engine_ops;
	snf->ecc_eng.priv = snf;

	ret = nand_ecc_register_on_host_hw_engine(&snf->ecc_eng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register ecc engine.\n");
		return ret;
	}

	ctlr->num_chipselect = 1;
	ctlr->mem_ops = &rtk_snand_mem_ops;
	ctlr->mem_caps = &rtk_snand_mem_caps;
	ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
	ctlr->mode_bits = SPI_RX_DUAL | SPI_RX_QUAD | SPI_TX_DUAL | SPI_TX_QUAD;
	ctlr->dev.of_node = pdev->dev.of_node;

	ret = spi_register_controller(ctlr);
	if (ret) {
		dev_err(&pdev->dev, "spi_register_controller failed.\n");
		goto err_disable_clk;
	}

	list_for_each_entry(child, &ctlr->dev.kobj.entry, kobj.entry) {
		if (child->driver && strstr(child->driver->name, "spi-nand")) {
			nand_found = true;
			break;
		}
	}

	if (!nand_found) {
		dev_warn(dev, "NAND chip NOT found. Releasing pins for eMMC...\n");
		ret = -ENODEV;
		goto err_unregister;
	}

	iounmap(regbase1);
	root = debugfs_create_dir("rtk_nand", NULL);
	if (!root)
		pr_warn("rtk_snand: failed to create debugfs directory\n");
	else
		debugfs_create_file("force_write", 0200, root, snf, &rtk_snand_debug_fops);

	return 0;

err_unregister:
	iounmap(regbase1);
	spi_unregister_controller(ctlr);
err_disable_clk:
	clk_disable_unprepare(snf->clk_nand);
	if (snf->dataBuf) {
		dma_free_coherent(snf->dev, snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size,
				snf->dataBuf, snf->dataPhys);
		snf->dataBuf = NULL;
	}

	if (snf->w_dbuf) {
		dma_free_coherent(snf->dev, snf->nfi_cfg.page_size,
				  snf->w_dbuf, snf->dbuf_dma);
		snf->w_dbuf = NULL;
	}

	if (snf->w_obuf) {
		dma_free_coherent(snf->dev, snf->nfi_cfg.oob_size,
				  snf->w_obuf, snf->obuf_dma);
		snf->w_obuf = NULL;
	}

	kfree(snf->buf);
	kfree(snf->sbt);
	kfree(snf->bbt);

	return ret;
}

static void rtk_snand_remove(struct platform_device *pdev)
{
	struct spi_controller *ctlr = platform_get_drvdata(pdev);
	struct rtk_snand *snf = spi_controller_get_devdata(ctlr);

	spi_unregister_controller(ctlr);
	clk_disable_unprepare(snf->clk_nand);

	if (snf->dataBuf) {
		dma_free_coherent(snf->dev, snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size,
				  snf->dataBuf, snf->dataPhys);
		snf->dataBuf = NULL;
	}

	if (snf->w_dbuf) {
		dma_free_coherent(snf->dev, snf->nfi_cfg.page_size,
				  snf->w_dbuf, snf->dbuf_dma);
		snf->w_dbuf = NULL;
	}

        if (snf->w_obuf) {
		dma_free_coherent(snf->dev, snf->nfi_cfg.oob_size,
				  snf->w_obuf, snf->obuf_dma);
		snf->w_obuf = NULL;
	}

	kfree(snf->buf);
	kfree(snf->sbt);
	kfree(snf->bbt);
}

static struct platform_driver rtk_snand_driver = {
	.probe = rtk_snand_probe,
	.remove = rtk_snand_remove,
	.driver = {
		.name = "rtk-snand",
		.of_match_table = rtk_snand_ids,
	},
};

module_platform_driver(rtk_snand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jyan Chou <jyanchou@realtek.com>");
MODULE_DESCRIPTION("RealTek SPI-NAND Flash Controller Driver");
