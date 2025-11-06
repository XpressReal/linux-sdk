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

static unsigned char g_rtk_nandinfo_line[128];
static unsigned int crc_table[256];
static int crc_table_computed = 0;

static void rtk_snand_set_nand_info(struct rtk_snand *snf, char *item)
{
        int ret = 0;
	unsigned long val;
        char *s = NULL;

	if ((s = strstr(item, "ds:")) != NULL) {
		ret = kstrtoul(s + 3, 10, &val);
		snf->nfi_cfg.chip_size = val;
	} else if ((s = strstr(item, "ps:")) != NULL) {
		ret = kstrtoul(s + 3, 10, &val);
		snf->nfi_cfg.page_size = val;
	} else if ((s = strstr(item, "bs:")) != NULL) {
		ret = kstrtoul(s + 3, 10, &val);
		snf->nfi_cfg.ppb = (val / snf->nfi_cfg.page_size);
	} else if ((s = strstr(item, "ecc:")) != NULL) {
		ret = kstrtoul(s + 4, 10, &val);
		snf->nfi_cfg.ecc = val;
	} else if ((s = strstr(item, "oob:")) != NULL) {
		ret = kstrtoul(s + 4, 10, &val);
		snf->nfi_cfg.oob_size = val;
	} else if ((s = strstr(item, "plane:")) != NULL) {
                ret = kstrtoul(s +6 , 10, &val);
                snf->nfi_cfg.plane = val;
	}
}

static int rtk_snand_get_nand_info_from_bootcode(struct rtk_snand *snf)
{
        const char * const delim = ",";
        char *sepstr = g_rtk_nandinfo_line;
        char *substr = NULL;

        if (strlen(g_rtk_nandinfo_line) == 0) {
                pr_err("No nand info got from lk!!!!\n");
                return -1;
        }

        pr_info("g_rtk_snandinfo_line:[%s]\n", sepstr);

        substr = strsep(&sepstr, delim);

        do {
                rtk_snand_set_nand_info(snf, substr);
                substr = strsep(&sepstr, delim);
        } while (substr);

        return 0;
}

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
	if (section)
		return -ERANGE;

	oobregion->offset = 6;
	oobregion->length = mtd->oobsize - oobregion->offset;

	return 0;
}

static int rtk_snand_ooblayout_free(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *oobregion)
{
	if (section)
		return -ERANGE;

	oobregion->offset = 0;
	oobregion->length = 6;

	return 0;
}

static const struct mtd_ooblayout_ops rtk_snand_ooblayout = {
	.ecc = rtk_snand_ooblayout_ecc,
	.free = rtk_snand_ooblayout_free,
};

static int rtk_snand_prepare_bouncebuf(struct rtk_snand *snf, size_t size)
{
        int ret = 0;

        if (snf->buf_len >= size)
                return 0;

        kfree(snf->buf);
        snf->buf = kmalloc(size, GFP_KERNEL);
        if (!snf->buf)
                return -ENOMEM;

        snf->buf_len = size;
        memset(snf->buf, 0xff, snf->buf_len);

        snf->oobtmp = kmalloc(snf->nfi_cfg.oob_size, GFP_KERNEL);
        if (!snf->oobtmp)
                return -ENOMEM;
        memset(snf->oobtmp, 0xff, snf->nfi_cfg.oob_size);

        snf->dataBuf = (unsigned char *)dma_alloc_coherent(snf->dev,
                                snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size, &snf->dataPhys,
                                GFP_DMA | GFP_KERNEL);
        if (!snf->dataBuf)
                ret = -ENOMEM;

        return ret;
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

	mtd_set_ooblayout(mtd, &rtk_snand_ooblayout);
	return 0;
}

static void rtk_snand_ecc_cleanup_ctx(struct nand_device *nand)
{
	struct rtk_ecc_config *ecc_cfg = nand_to_ecc_ctx(nand);

	kfree(ecc_cfg);
}

static int rtk_snand_ecc_prepare_io_req(struct nand_device *nand,
					struct nand_page_io_req *req)
{
	struct rtk_snand *snf = nand_to_rtk_snand(nand);
	struct rtk_ecc_config *ecc_cfg = nand_to_ecc_ctx(nand);
	int ret;

	ret = rtk_snand_setup_pagefmt(snf, nand->memorg.pagesize,
				      nand->memorg.oobsize);
	if (ret)
		return ret;

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
static void rtk_snand_read_tag(struct rtk_snand *snf)
{
	unsigned char *oobbuf = snf->dataBuf + snf->nfi_cfg.page_size;
	void __iomem *map_base = snf->regs;
	unsigned char buf[256];
	unsigned int val = 0x0;
	int eccstep_size = (snf->nfi_cfg.ecc == 0x1) ? 32 : 16;
	int ecc_size = (snf->nfi_cfg.ecc == 0x1) ? 20 : 10;
	int eccstep =  4;
	int i;

	writel(0x0, map_base + REG_READ_BY_PP);
	writel(0x30 | 0x02, map_base + REG_SRAM_CTL);

	memset(oobbuf, 0xFF, snf->nfi_cfg.oob_size);
	memset(buf, 0xFF, snf->nfi_cfg.oob_size);

	for (i = 0; i < (snf->nfi_cfg.oob_size / 4); i++) {
		val = readl(map_base + (i*4));

		buf[(i * 4)] = val & 0xff;
		buf[(i * 4) + 1] = (val >> 8) & 0xff;
		buf[(i * 4) + 2] = (val >> 16) & 0xff;
		buf[(i * 4) + 3] = (val >> 24) & 0xff;
	}

	for (i = 0; i < eccstep; i++)
		memcpy(oobbuf + (i * (ecc_size + 6)), buf + (i * eccstep_size),
			(ecc_size + 6));

	writel(0x0, map_base + REG_SRAM_CTL);
	writel(0x80, map_base + REG_READ_BY_PP);

	return;
}

static int rtk_snand_read_bbt(struct rtk_snand *snf, int page, u8 *p, u32 size)
{
	void __iomem *base = snf->regs;
	int ret = 0;
	dma_addr_t buf_dma = snf->dataPhys;

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
	writel(0x82, base + REG_DATA_TL1);
	writel(snf->nfi_cfg.page_size / 0x200, base + REG_PAGE_LEN);

	// set data dma addr
	writel((uintptr_t)(buf_dma >> 3), base + REG_DMA_CTL1);
	writel((snf->nfi_cfg.page_size >> 9) & 0x0000FFFF, base + REG_DMA_CTL2);
	// dma xfer setting
        writel(0x3, base + REG_DMA_CTL3);

	// set spare dma addr
	writel(0x0, base + REG_SPR_DDR_CTL);

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
	if (ret)
		pr_err("busy waiting bbt read SPI_CTRL0 done...\n");

	ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
	if (ret)
                pr_err("busy waiting bbt dma xfer done...\n");

	rtk_snand_read_tag(snf);

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
	unsigned char *new_buf = NULL;
	int rc = 0;

	new_buf =  snf->dataBuf;
        memset(new_buf, 0xFF, snf->nfi_cfg.page_size);

	rc = rtk_snand_read_bbt(snf, 4 * snf->nfi_cfg.ppb, new_buf, snf->nfi_cfg.page_size);
	if (new_buf[snf->nfi_cfg.page_size + 4] != SBT_TAG) {
		pr_err("Check SBT2.\n");
		memset(new_buf, 0xFF, snf->nfi_cfg.page_size);

		rc = rtk_snand_read_bbt(snf, 5 * snf->nfi_cfg.ppb, new_buf, snf->nfi_cfg.page_size);
		if (new_buf[snf->nfi_cfg.page_size + 4] != SBT_TAG) {
			pr_err("NO SBT1 and NO SBT2.\n");
			kfree(snf->sbt);
			snf->sbt = NULL;
			return rc;
		} else {
			pr_err("SBT2 exist, load it.\n");
			memcpy(snf->sbt , new_buf + CRCLEN, sizeof(struct sb_table) * SBTCNT);
		}
	} else {
		pr_err("SBT1 exist, load it.\n");
		memcpy(snf->sbt , new_buf + CRCLEN, sizeof(struct sb_table) * SBTCNT);
	}

        rtk_snand_dump_SBT(snf);
        return rc;
}

static void rtk_snand_dump_BBT(struct rtk_snand *snf)
{
        int i;
        int BBs = 0;

	pr_err("[%s] Nand BBT Content\n", __FUNCTION__);

	if (snf->bbt) {
		for (i = 0; i < snf->RBA; i++){
			if (i == 0 && snf->bbt[i].BB_die == BB_DIE_INIT && snf->bbt[i].bad_block == BB_INIT ){
				pr_err("Congratulation!! No BBs in this Nand.\n");
				break;
			}
			if (snf->bbt[i].bad_block != BB_INIT ){
				pr_err("[%d] (%d, %u, %d, %u)\n", i,
				snf->bbt[i].BB_die, snf->bbt[i].bad_block, snf->bbt[i].RB_die, snf->bbt[i].remap_block);
				BBs++;
			}
		}
	}
	return;
}

static int rtk_snand_scan_bbt(struct rtk_snand *snf)
{
        unsigned char *new_buf = NULL;
        int rc = 0;

	new_buf =  snf->dataBuf;
	memset(new_buf, 0xFF, snf->nfi_cfg.page_size);

	rc = rtk_snand_read_bbt(snf, snf->nfi_cfg.ppb, new_buf, snf->nfi_cfg.page_size);
	if (new_buf[snf->nfi_cfg.page_size + 4] != BBT_TAG) {
		pr_err("Check BBT2.\n");
		memset(new_buf, 0xFF, snf->nfi_cfg.page_size);

		rc = rtk_snand_read_bbt(snf, 2 * snf->nfi_cfg.ppb, new_buf, snf->nfi_cfg.page_size);
		if (new_buf[snf->nfi_cfg.page_size + 4] != BBT_TAG)
                        pr_err("NO BBT1 and NO BBT2.\n");
		else
			pr_err("BBT2 exist, load it.\n");
				memcpy(snf->bbt , new_buf + CRCLEN, sizeof(struct bb_table) * snf->RBA);
	} else {
		pr_err("BBT1 exist, load it.\n");
		memcpy(snf->bbt , new_buf + CRCLEN, sizeof(struct bb_table) * snf->RBA);
        }

	rtk_snand_dump_BBT(snf);
	return rc;
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
		return -1;
	}

	if((readl(base + REG_SPI_STATUS) & 0x4) != 0) {
		pr_err("busy waiting bbt erase SPI_status done...\n");     	    
		return -1;
	}

	return 0;	
}

static int rtk_snand_write_bbt(struct rtk_snand *snf, int page, u8 *p)
{
	void __iomem *base = snf->regs;
        u32 dma_len = snf->buf_len;
        int ret = 0;
        dma_addr_t buf_dma = snf->dataPhys;
	dma_addr_t oobPhy = snf->dataPhys + snf->nfi_cfg.page_size;

	dma_len = snf->nfi_cfg.page_size;

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

        // set data dma addr
        writel((uintptr_t)(buf_dma >> 3), base + REG_DMA_CTL1);
        writel((snf->nfi_cfg.page_size >> 9) & 0x0000FFFF, base + REG_DMA_CTL2);
        // dma xfer setting
        writel(0x1, base + REG_DMA_CTL3);

        // set spare dma addr
	writel((0x1 << 30) , base + REG_SPR_DDR_CTL);
	writel((uintptr_t)(oobPhy) / 8, base + REG_SPR_DDR_CTL2);

	// set PP (read is through PP SRAM)
        writel(0x0, base + REG_READ_BY_PP);
        writel(0x0, base + REG_PP_CTL1);
        writel(0x0, base + REG_PP_CTL0);

        writel(0xc0, base + REG_SPI_SA);
        writel(0x0, base + REG_SPI_CMD1);

        writel(0xb | (0x1 << 4), base + REG_SPI_CTRL0);

        // wait auto trigger done, pp busy done and dma xfer done
        ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
        if (ret)
                pr_err("still busy waiting bbt SPI_CTRL0 done...\n");

        ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
        if (ret)
                pr_err("still busy waiting bbt dma xfer done...\n");

	ret = wait_done(base + REG_SPI_STATUS, 0x8, 0);
        if (ret)
                pr_err("still busy waiting bbt SPI STATUS...\n");
	
	return 0;
}

static unsigned int snand_Reflect(unsigned int ref, char ch)
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

int rtk_update_bbt_to_flash(struct rtk_snand *snf)
{
	int rc1 = 0;
	int rc2 = 0;
	unsigned char *buf = NULL;
	u32 crc;

	buf = snf->dataBuf;
	memset(buf, 0xFF, snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size);

	rtk_snand_bbt_erase(snf, 64);
        rtk_snand_write_bbt(snf, 64, buf);

	crc = rtk_snand_table_crc_calculate(snf, BBTABLE, (unsigned char *)snf->bbt, NULL);
        memcpy(buf, &crc, CRCLEN);	
	memcpy(buf + 4, snf->bbt, sizeof(struct bb_table) *snf->RBA);

	*(buf + snf->nfi_cfg.page_size + 4) = BBT_TAG;

	rc2 = rtk_snand_bbt_erase(snf, 64);
	if (rc2 == 0)
		rc2 = rtk_snand_write_bbt(snf, 64, buf);

	rc1 = rtk_snand_bbt_erase(snf, 128);
        if (rc1 == 0)
                rc1 = rtk_snand_write_bbt(snf, 128, buf);

	if (rc1 != 0 && rc2 != 0)
		return -1;
	else
		return 0;
}

static int rtk_update_BBT(struct rtk_snand *snf, unsigned int blk)
{
        unsigned int i, flag = 1;

        for (i = 0; i < snf->RBA; i++){
                if (snf->bbt[i].bad_block == BB_INIT && snf->bbt[i].remap_block != RB_INIT) {
                        flag = 0;
                        snf->bbt[i].BB_die = 0;
                        snf->bbt[i].bad_block = blk;
                        snf->bbt[i].RB_die = 0;
                        break;
                }
        }

        if (flag) {
                pr_err("[%s] RBA do not have free remap block\n", __FUNCTION__);
                return -1;
        }
        if (rtk_update_bbt_to_flash(snf)){
                pr_err("[%s] rtk_update_bbt_to_spi_flash() fails.\n", __func__);
                return -1;
        }

        pr_err("[%s] rtk_update_bbt() successfully.\n", __FUNCTION__);
        return 0;
}

static unsigned int rtk_snand_find_real_blk(struct rtk_snand *snf, unsigned int blk)
{
        unsigned int i = 0;

        for (i = 0; i < snf->RBA; i++){
		if (snf->bbt[i].bad_block == BB_INIT) {
			pr_err("find available reserved block, remap_block = %d\n",
				snf->bbt[i].remap_block);
			return snf->bbt[i].remap_block;
		}
        }

	pr_err("RTK %s(%d) No available reserved block.\n",
		__func__, __LINE__);

        return -1;
}

static int rtk_snand_backup_block(struct rtk_snand *snf, unsigned int blk, u8 *p)
{
	unsigned int backup_block = rtk_snand_find_real_blk(snf, blk);
        unsigned int backup_start_page = backup_block * snf->nfi_cfg.ppb;
        unsigned int i;
        char *cmp_buf = NULL;
        int rc = 0;

        cmp_buf = kmalloc(snf->nfi_cfg.page_size, GFP_KERNEL);

	if (rtk_snand_bbt_erase(snf, backup_start_page))
		pr_err("[%s]error: erase block %u failure\n", __FUNCTION__, backup_start_page);

        for(i = 0; i < snf->nfi_cfg.ppb; i++) {
		memcpy(cmp_buf, p, snf->nfi_cfg.page_size);
		rc = rtk_snand_write_bbt(snf, backup_start_page + i, p);
		if (rc != 0)
			pr_err("[%s] rtk_backup fail.\n", __FUNCTION__);
	}
	return rc;
}

static int rtk_snand_badblock_handle(struct rtk_snand *snf, unsigned int page, uint32_t backup, u8 *p)
{
	unsigned int block = page / snf->nfi_cfg.ppb;

        if (rtk_update_BBT(snf, block) == -1) {
                pr_err("[%s] rtk_update_BBT fail.\n", __FUNCTION__);
                return -1;
        }

        if (backup) {
                /* Backup all block */
                if (rtk_snand_backup_block(snf, block, p) == -1) {
                        pr_err("[%s] rtk_backup_all_block fail.\n", __FUNCTION__);
                        return -1;
                }
        }
        return 1;
}

static int rtk_snand_check_badblock_table(struct rtk_snand *snf, unsigned int blk)
{
        int r_blk = blk;
        int i;

        for (i = 0; i < snf->RBA; i++) {
                if (r_blk == snf->bbt[i].bad_block)
                        r_blk = snf->bbt[i].remap_block;
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
                } else
                        break;
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

int rtk_snand_get_real_page(struct rtk_snand *snf, int page, int mode)
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

static void rtk_snand_read_fdm(struct rtk_snand *snf, u8 *buf)
{
	void __iomem *base = snf->regs;
	unsigned char *oobbuf = buf;
	unsigned char tbuf[256];
	unsigned int val = 0x0;
	int eccstep_size = (snf->nfi_cfg.ecc == 0x1) ? 32 : 16;
        int ecc_size = (snf->nfi_cfg.ecc == 0x1) ? 20 : 10;
        int i;

	// set PP (read is through PP SRAM)
        writel(0x0, base + REG_READ_BY_PP);
        writel(0x32, base + REG_SRAM_CTL);

	memset(buf, 0xFF, snf->nfi_cfg.oob_size);
	memset(tbuf, 0xFF, snf->nfi_cfg.oob_size);

	for (i = 0; i < snf->nfi_cfg.oob_size / 4; i++) {
		val = readl(base + (i*4));

		tbuf[(i*4)] = val & 0xff;
		tbuf[(i*4)+1] = (val >> 8) & 0xff;
		tbuf[(i*4)+2] = (val >> 16) & 0xff;
		tbuf[(i*4)+3] = (val >> 24) & 0xff;
	}

	for (i = 0; i < snf->nfi_cfg.nsectors; i++)
		memcpy(oobbuf + (i *(ecc_size + 6)), tbuf + (i * eccstep_size), (ecc_size + 6));

	writel(0x0, base + REG_SRAM_CTL);
	writel(0x80, base + REG_READ_BY_PP);
}

int rtk_snand_get_mapping_page(struct rtk_snand *snf, int page)
{
        return rtk_snand_get_real_page(snf, page, BBTABLE);
}

int rtk_snand_get_physical_page(struct rtk_snand *snf, int page)
{
        return rtk_snand_get_real_page(snf, page, SBTABLE);
}

static int rtk_snand_read_page_cache(struct rtk_snand *snf,
				     const struct spi_mem_op *op)
{
	void __iomem *base = snf->regs;
	u8 *buf = snf->buf;
	u8 *oobbuf = buf + snf->nfi_cfg.page_size; 
	u32 op_addr = snf->offs;
	// where to start copying data from bounce buffer
	u32 rd_offset = 0;
	u32 dma_len = 0;
	u32 last_bit;
	u32 mask;
	u8 *p = op->data.buf.in;
	int ret = 0;
	int phy_page = rtk_snand_get_physical_page(snf, op_addr);
        int real_page = rtk_snand_get_mapping_page(snf, phy_page);
	unsigned int blank_check = 0;
	dma_addr_t buf_dma = snf->dataPhys;
        //dma_addr_t oobPhy = buf_dma + snf->nfi_cfg.page_size;

	dma_len = snf->nfi_cfg.page_size;
	last_bit = fls(snf->nfi_cfg.page_size + snf->nfi_cfg.oob_size);
	mask = (1 << last_bit) - 1;
	rd_offset = op_addr & mask;
	op_addr = real_page;

	buf_dma = dma_map_single(snf->dev, (u8 *)p, dma_len, DMA_FROM_DEVICE);
        ret = dma_mapping_error(snf->dev, buf_dma);
        if (ret) {
                dev_err(snf->dev, "DMA mapping failed.\n");
                return -1;
        }

        dma_sync_single_for_device(snf->dev, buf_dma, dma_len, DMA_FROM_DEVICE);

	writel(0x1, base + REG_SPI_MASK);
	writel(0x0, base + REG_SPI_HIT);

	// enable random mode
	writel(0x1, base + REG_RND_EN);
        writel(0, base + REG_RND_DATA_STR_COL_H);
        writel(snf->nfi_cfg.page_size >> 8, base + REG_RND_SPR_STR_COL_H);
        writel(snf->nfi_cfg.page_size & 0xff, base + REG_RND_SPR_STR_COL_L);

	// enable blank check
	writel(0x1, base + REG_BLANK_CHK);

	// set PA and CA
	writel(op_addr & 0xff, base + REG_ND_PA0);
	writel((op_addr >> 8) & 0xff, base + REG_ND_PA1);
	writel((op_addr >> 16) & 0x1f, base + REG_ND_PA2);
	writel(((op_addr >> 21)& 0x7) << 5, base + REG_ND_PA3);

	if (snf->nfi_cfg.plane) {
		if (snf->nfi_cfg.page_size == 2048) {
			writel(0x0, base + REG_ND_CA0);
			writel(0x10, base + REG_ND_CA1);
			writel(0x10 | 0x8, base + REG_RND_SPR_STR_COL_H);
			writel(0x10, base + REG_RND_DATA_STR_COL_H);
		} else {
			writel(0x0, base + REG_ND_CA0);
			writel(0x20, base + REG_ND_CA1);
			writel(0x20 | 0x10, base + REG_RND_SPR_STR_COL_H);
			writel(0x20, base + REG_RND_DATA_STR_COL_H);
		}
	} else {
		writel(0x0, base + REG_ND_CA0);
		writel(0x0, base + REG_ND_CA1);
	}

	// set transfer size
	writel(0x82, base + REG_DATA_TL1);
	writel(snf->nfi_cfg.page_size / 0x200, base + REG_PAGE_LEN);

	// set data dma addr
	writel((uintptr_t)(buf_dma >> 3), base + REG_DMA_CTL1);
	writel((snf->nfi_cfg.page_size >> 9) & 0x0000FFFF, base + REG_DMA_CTL2);
	// dma xfer setting
        writel(0x3, base + REG_DMA_CTL3);

	// set spare dma addr
	//writel((0x1 << 30) | (((uintptr_t)(oobPhy) / 8) & 0x1fffffff), base + REG_SPR_DDR_CTL);
	writel(0x0, base + REG_SPR_DDR_CTL);

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
	if (ret)
		pr_err("still busy waiting SPI_CTRL0 done...\n");

	ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
	if (ret)
                pr_err("still busy waiting dma xfer done...\n");

	rtk_snand_read_fdm(snf, oobbuf);

	blank_check = readl(base + REG_BLANK_CHK);
	if (blank_check & 0x2) {
		ret = 0;
	} else if (readl(base + REG_ND_ECC) & 0x10) {
		if (blank_check & 0x8) {
                        dev_err(snf->dev, "RTK %s(%d) un-correct ecc error ... page:[%d]\n",
                                                        __func__, __LINE__, op_addr);
                        ret = -1;
                } else {
                        ret = 2;
                }
	}
#if 0
	if (readl(base + REG_ND_ECC) & 0x10) {
		dev_err(snf->dev, "RTK %s unusable page:[%d]\n", __func__, op_addr);
		ret = -2;
	} else {
                if (blank_check & 0x8) {
                        dev_err(snf->dev, "RTK %s(%d) un-correct ecc error ... page:[%d]\n",
                                                        __func__, __LINE__, op_addr);
                        ret = -1;
		} else {
			ret = 0;
		}
	}

#endif
	writel(0x1, base + REG_BLANK_CHK);
	dma_sync_single_for_cpu(snf->dev, buf_dma, dma_len, DMA_FROM_DEVICE);
	dma_unmap_single(snf->dev, buf_dma, dma_len, DMA_FROM_DEVICE);

	if (op->data.nbytes == snf->nfi_cfg.oob_size)
                memcpy(op->data.buf.in, oobbuf, snf->nfi_cfg.oob_size);

	return ret;
}

static int rtk_snand_read_oob(struct rtk_snand *snf, const struct spi_mem_op *op)
{
	int ret = 0;

	ret = rtk_snand_read_page_cache(snf, op);

	if (ret < 0) {
		pr_err("rtk_snf_read oob fail\n");
                rtk_snand_badblock_handle(snf, op->addr.val, 1, op->data.buf.in);
        }

	return ret;
}

static int rtk_snand_write_page_cache(struct rtk_snand *snf,
				      const struct spi_mem_op *op)
{
	void __iomem *base = snf->regs;

	snf->txbuf = op->data.buf.out;
        // enable random mode
        writel(0x1, base + REG_RND_EN);
	writel(0, base + REG_RND_DATA_STR_COL_H);
        writel(snf->nfi_cfg.page_size >> 8, base + REG_RND_SPR_STR_COL_H);
        writel(snf->nfi_cfg.page_size & 0xff, base + REG_RND_SPR_STR_COL_L);

        // set transfer size
        writel(0x2, base + REG_DATA_TL1);
        writel(snf->nfi_cfg.page_size / 0x200, base + REG_PAGE_LEN);

	// set PP (read is through PP SRAM)
        writel(0x0, base + REG_READ_BY_PP);
        writel(0x0, base + REG_PP_CTL1);
        writel(0x0, base + REG_PP_CTL0);

        writel(0xc0, base + REG_SPI_SA);
        writel(0x0, base + REG_SPI_CMD1);

	return 0;
}

static int rtk_snand_progarm_load(struct rtk_snand *snf,
				      const struct spi_mem_op *op)
{
	void __iomem *base = snf->regs;
        u32 op_addr = op->addr.val;
	int phy_page = rtk_snand_get_physical_page(snf, op_addr);
        int real_page = rtk_snand_get_mapping_page(snf, phy_page);
        u32 dma_len = snf->buf_len;
        int ret = 0;
	dma_addr_t buf_dma = snf->dataPhys;
	dma_addr_t oobPhy = snf->dataPhys + snf->nfi_cfg.page_size;

	dma_len = snf->nfi_cfg.page_size;
	op_addr = real_page;

	buf_dma = dma_map_single(snf->dev, (u8 *)snf->txbuf, dma_len, DMA_TO_DEVICE);
        ret = dma_mapping_error(snf->dev, buf_dma);
        if (ret) {
                dev_err(snf->dev, "DMA mapping failed.\n");
                return -1;
        }
   	dma_sync_single_for_device(snf->dev, buf_dma, dma_len, DMA_TO_DEVICE);

	writel(0x1, base + REG_SPI_MASK);
        writel(0x0, base + REG_SPI_HIT);

        // set PA and CA
        writel(op_addr & 0xff, base + REG_ND_PA0);
        writel((op_addr >> 8) & 0xff, base + REG_ND_PA1);
        writel((op_addr >> 16) & 0x1f, base + REG_ND_PA2);

	if (snf->nfi_cfg.plane) {
		if (snf->nfi_cfg.page_size == 2048) {
			writel(0x0, base + REG_ND_CA0);
			writel(0x10, base + REG_ND_CA1);
			writel(0x10 | 0x8, base + REG_RND_SPR_STR_COL_H);
			writel(0x10, base + REG_RND_DATA_STR_COL_H);
		} else {
			writel(0x0, base + REG_ND_CA0);
			writel(0x20, base + REG_ND_CA1);
			writel(0x20 | 0x10, base + REG_RND_SPR_STR_COL_H);
			writel(0x20, base + REG_RND_DATA_STR_COL_H);
		}
	} else {
		writel(0x0, base + REG_ND_CA0);
		writel(0x0, base + REG_ND_CA1);
        }

        // set data dma addr
        writel(((uintptr_t)(buf_dma >> 3)), base + REG_DMA_CTL1);
        writel((snf->nfi_cfg.page_size >> 9) & 0x0000FFFF, base + REG_DMA_CTL2);
        // dma xfer setting
        writel(0x1, base + REG_DMA_CTL3);

	// set spare dma addr
	writel((0x1 << 30) , base + REG_SPR_DDR_CTL);
	writel(((uintptr_t)(oobPhy) / 8), base + REG_SPR_DDR_CTL2);

        writel(0xc0, base + REG_SPI_SA);
        writel(0x0, base + REG_SPI_CMD1);

        writel(0xb | (0x1 << 4), base + REG_SPI_CTRL0);

        // wait auto trigger done, pp busy done and dma xfer done
        ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
        if (ret)
                pr_err("still busy waiting SPI_CTRL0 done...\n");

        ret = wait_done(base + REG_DMA_CTL3, 0x1, 0);
        if (ret)
		pr_err("still busy waiting dma xfer done...\n");

	ret = wait_done(base + REG_SPI_STATUS, 0x8, 0);
        if (ret)
		pr_err("still busy waiting SPI STATUS...\n");

        dma_sync_single_for_cpu(snf->dev, buf_dma, dma_len, DMA_TO_DEVICE);
        dma_unmap_single(snf->dev, buf_dma, dma_len, DMA_TO_DEVICE);

	dma_sync_single_for_cpu(snf->dev, oobPhy, snf->nfi_cfg.oob_size, DMA_TO_DEVICE);
	dma_unmap_single(snf->dev, oobPhy, snf->nfi_cfg.oob_size, DMA_TO_DEVICE);

	return ret;
}

static int rtk_snand_write_oob(struct rtk_snand *snf, const struct spi_mem_op *op)
{
	int ret = 0;        

	ret = rtk_snand_write_page_cache(snf, op);
	if (ret < 0) {
		pr_err("rtk_snand_write oob fail\n");
		rtk_snand_badblock_handle(snf, op->addr.val, 0, (u8 *)op->data.buf.out);
        }
	return ret;
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
		return -1;
	}

	if((readl(base + REG_SPI_STATUS) & 0x4) != 0) {
		pr_err("Erase fail\n");     	    
		return -1;
	}

	return 0;	
}

static int rtk_snand_io_op(struct rtk_snand *snf, const struct spi_mem_op *op)
{
	void __iomem *base = snf->regs;
	int ret = 0;
	u32 id1 = 0, status, mask;
	u16 cmd = op->cmd.opcode;
	unsigned char id[3];
	const u8 *tx_buf = NULL;
	u8 *rx_buf = NULL;

	if (op->data.dir == SPI_MEM_DATA_IN)
		rx_buf = op->data.buf.in;
	else
		tx_buf = op->data.buf.out;

	switch(cmd) {
	case SPINAND_READID:
		writel((readl(base + REG_SPI_CTRL1) & ~(0x3 << 6)) | (0x3 << 6), base + REG_SPI_CTRL1);
		writel(0x5 | (0x1 << 4), base + REG_SPI_CTRL0);
		ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);

		id[0] = readl(base + REG_SPI_ID0) & 0xff;
                id[1] = readl(base + REG_SPI_ID1) & 0xff;
                id[2] = readl(base + REG_SPI_ID2) & 0xff;
                //id = readl(base + REG_SPI_ID0)& 0xff | ((readl(base + REG_SPI_ID1) & 0xff) << 8) | ((readl(base + REG_SPI_ID2) & 0xff) << 16);
                id1 = id[0] | (id[1] << 8) | (id[2] << 16);
	//	pr_err("id = 0x%x\n", id1);

		memcpy_fromio(op->data.buf.in, (unsigned char *)base + REG_SPI_ID0, 1);
		memcpy_fromio(op->data.buf.in + 1, (unsigned char *)base + REG_SPI_ID1, 1);
		memcpy_fromio(op->data.buf.in + 2, (unsigned char *)base + REG_SPI_ID2, 1);
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
		break;
	case SPINAND_SET_FEATURE:
		writel(0x0, base + REG_SPI_MASK);
		writel(0x0, base + REG_SPI_HIT);
		writel(op->addr.val, base + REG_SPI_SA);
		writel(tx_buf[0], base + REG_SPI_SDATA);
		writel(0x7 | (0x1 << 4), base + REG_SPI_CTRL0);
		ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
		break;
	case SPINAND_GET_FEATURE:
		writel(0x1, base + REG_SPI_MASK);
                writel(0x0, base + REG_SPI_HIT);
                writel(op->addr.val, base + REG_SPI_SA);
		writel(cmd, base + REG_SPI_CMD1);
		writel(0x8 | (0x1 << 4), base + REG_SPI_CTRL0);
		ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
		status = readl(base + REG_SPI_STATUS);
		mask = readl(base + REG_SPI_MASK);
		if ((status & mask) == readl(base + REG_SPI_HIT))
			rx_buf[0] = status & mask;
                break;
	case SPINAND_RESET:
		writel(cmd, base + REG_SPI_CMD1);
		writel(0x11, base + REG_SPI_CTRL0);
		ret = wait_done(base + REG_SPI_CTRL0, 0x10, 0);
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
	struct rtk_snand *snf = spi_controller_get_devdata(mem->spi->master);

	pr_debug("OP %02x,  ADDR %08llX@%d:%u DATA %d:%u", op->cmd.opcode,
		op->addr.val, op->addr.buswidth, op->addr.nbytes,
		op->data.buswidth, op->data.nbytes);

	if (rtk_snand_is_page_ops(op)) {
		if (op->data.dir == SPI_MEM_DATA_IN)
			return rtk_snand_read_oob(snf, op);
		else
			return rtk_snand_write_oob(snf, op);
	} else {
		return rtk_snand_io_op(snf, op);
	}
	return 0;
}

static const struct spi_controller_mem_ops rtk_snand_mem_ops = {
	//.adjust_op_size = rtk_snand_adjust_op_size,
	.supports_op = rtk_snand_supports_op,
	.exec_op = rtk_snand_exec_op,
};

static const struct of_device_id rtk_snand_ids[] = {
	{ .compatible = "realtek,rtd1625-snf" },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_snand_ids);

unsigned int rtks_set_reg(void __iomem *addr, unsigned int offset, int length, unsigned int value)
{
        unsigned int value1, temp;
        int j;

        value1 = readl(addr);

        temp = 1;

        for (j = 0; j < length; j++)
                temp = temp * 2;

        value1 = (value1 & ~((temp - 1) << offset)) | (value << offset);

        writel(value1, addr);

        return value1;
}

#if 0
static void rtk_snand_pinmux_setting(void)
{
	void __iomem *regbase;

	regbase = ioremap(0x9804f200, 0x20);

	rtks_set_reg(regbase, 16, 4, 3);
	rtks_set_reg(regbase, 20, 4, 3);
	rtks_set_reg(regbase, 24, 4, 3);
	rtks_set_reg(regbase, 28, 4, 3);
	rtks_set_reg(regbase + 4, 0, 4, 3);
	rtks_set_reg(regbase + 4, 4, 4, 3);

	iounmap(regbase);
}
#endif

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
	
	writel(readl(pll_base + 0x0) &~(1 << 1), pll_base);
	rtks_set_reg(regbase, 1, 1, 0);
	rtks_set_reg(regbase, 8, 1, 0);
	udelay(20);

	ssc_div_n = 0x9;
	tmp_val = (readl(pll_base + 0x8) & 0xffff) | (ssc_div_n << 16);
	writel(tmp_val, pll_base + 0x8);

	ssc_div_ext_f = 0x84b;
	tmp_val = readl(pll_base + 0x4) & ~(0x1fff << 13) & ~(0x3 << 27) & ~(0x7 << 10) & ~(0xf << 5);
	writel(tmp_val | (ssc_div_ext_f << 13) |  (0x2 << 10), pll_base + 0x4);
	udelay(20);

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

static int rtk_snand_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const struct of_device_id *dev_id;
	struct spi_controller *ctlr;
	struct resource *res;
	struct rtk_snand *snf;
	u32 b;
	int ret;

	dev_id = of_match_node(rtk_snand_ids, np);
	if (!dev_id)
		return -EINVAL;

	ctlr = devm_spi_alloc_master(&pdev->dev, sizeof(*snf));
	if (!ctlr)
		return -ENOMEM;

	snf = spi_controller_get_devdata(ctlr);

	snf->ctlr = ctlr;
	snf->dev = dev;

	snf->clk_nand = devm_clk_get(&pdev->dev, "nand");
	if (IS_ERR(snf->clk_nand)) {
		dev_err(dev, "%s: clk_get() returns %ld\n", __func__,
			PTR_ERR(snf->clk_nand));
		return -1;
	}

	clk_prepare_enable(snf->clk_nand);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	snf->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(snf->regs)) {
		ret = PTR_ERR(snf->regs);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	snf->pll_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(snf->pll_regs)) {
		ret = PTR_ERR(snf->pll_regs);
		dev_err(dev, "no pll reg base\n");
	}

	rtk_snand_pll_setup(snf);
	//rtk_snand_pinmux_setting();

	platform_set_drvdata(pdev, ctlr);
	rtk_snand_get_nand_info_from_bootcode(snf);

	ret = rtk_snand_setup_pagefmt(snf, snf->nfi_cfg.page_size, snf->nfi_cfg.oob_size);
	if (ret) {
		dev_err(snf->dev, "failed to set initial page format\n");
		return -1;
	}

	snf->sbt = kmalloc(sizeof(struct sb_table) * SBTCNT, GFP_KERNEL);
	if (!snf->sbt) {
		pr_err("RTK %s(%d) alloc sbt fail.\n", __func__, __LINE__);
		return -ENOMEM;
	}
	memset(snf->sbt, 0xff, sizeof(struct sb_table) * SBTCNT);
	rtk_snand_scan_sbt(snf);

	b = (unsigned int)(snf->nfi_cfg.chip_size) / (snf->nfi_cfg.page_size * snf->nfi_cfg.ppb);
	snf->RBA = ((b * 5) / 100) - BLOCKINFO - snf->SHIFTBLK;

	snf->bbt = kmalloc(sizeof(struct bb_table) * snf->RBA, GFP_KERNEL);
	if (!snf->bbt) {
		dev_err(dev, "RTK %s(%d) alloc bbt fail.\n",
			__func__, __LINE__);
		return -ENOMEM;
	}
	memset(snf->bbt, 0xff, sizeof(struct bb_table) * snf->RBA);
	rtk_snand_scan_bbt(snf);

	// setup ECC engine
	snf->ecc_eng.dev = &pdev->dev;
	snf->ecc_eng.integration = NAND_ECC_ENGINE_INTEGRATION_PIPELINED;
	snf->ecc_eng.ops = &rtk_snf_ecc_engine_ops;
	snf->ecc_eng.priv = snf;

	ret = nand_ecc_register_on_host_hw_engine(&snf->ecc_eng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register ecc engine.\n");
	}

	ctlr->num_chipselect = 1;
	ctlr->mem_ops = &rtk_snand_mem_ops;
	ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
	ctlr->mode_bits = SPI_RX_DUAL | SPI_RX_QUAD | SPI_TX_DUAL | SPI_TX_QUAD;
	ctlr->dev.of_node = pdev->dev.of_node;

	ret = spi_register_controller(ctlr);
	if (ret) {
		dev_err(&pdev->dev, "spi_register_controller failed.\n");
	} else {
		dev_err(dev, "success to init spi-nand chip\n");
	}

	return 0;

}

static int rtk_snand_remove(struct platform_device *pdev)
{
	struct spi_controller *ctlr = platform_get_drvdata(pdev);
	struct rtk_snand *snf = spi_controller_get_devdata(ctlr);

	spi_unregister_controller(ctlr);
	clk_disable_unprepare(snf->clk_nand);
	kfree(snf->buf);
	return 0;
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

static int __init rtk_snand_info_setup(char *line)
{
        strlcpy(g_rtk_nandinfo_line, line, sizeof(g_rtk_nandinfo_line));
        return 1;
}
__setup("snandinfo=", rtk_snand_info_setup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jyan Chou <jyanchou@realtek.com>");
MODULE_DESCRIPTION("RealTek SPI-NAND Flash Controller Driver");
