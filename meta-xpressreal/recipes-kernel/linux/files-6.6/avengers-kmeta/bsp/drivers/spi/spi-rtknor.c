// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek SPI Nor Flash Controller Driver
 *
 * Copyright (c) 2024 Realtek Technologies Co., Ltd.
 */
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/spi-nor.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

#define SFC_OPCODE		(0x00)
#define SFC_CTL			(0x04)
#define SFC_SCK			(0x08)
#define SFC_CE			(0x0c)
#define SFC_WP			(0x10)
#define SFC_POS_LATCH		(0x14)
#define SFC_WAIT_WR		(0x18)
#define SFC_EN_WR		(0x1c)
#define SFC_FAST_RD		(0x20)
#define SFC_SCK_TAP		(0x24)
#define SFP_OPCODE2		(0x28)

#define MD_BASE_ADDE		0x9814BF00
#define MD_BASE_ADDE_STARK	0x9801B700
#define MD_FDMA_DDR_SADDR	(0x0c)
#define MD_FDMA_FL_SADDR	(0x10)
#define MD_FDMA_CTRL2		(0x14)
#define MD_FDMA_CTRL1		(0x18)
#define MD_FDMA_DDR_SADDR1	(0x20)

#define RTKSFC_DMA_MAX_LEN	0x100
#define RTKSFC_MAX_CHIP_NUM	1
#define RTKSFC_WAIT_TIMEOUT	1000000
#define RTKSFC_OP_READ          0x0
#define RTKSFC_OP_WRITE         0x1

#define NOR_BASE_PHYS           0x88100000

struct rtksfc_host {
	struct device *dev;
	struct mutex lock;
	struct clk* clk_nor;
	void __iomem *regbase;
	void __iomem *iobase;
	void __iomem *mdbase;
	void *buffer;
	dma_addr_t dma_buffer;
};

static const struct soc_device_attribute rtk_soc_stark[] = {
	{
		.family = "Realtek Stark",
	},
	{
                /* empty */
	}
};

static int rtk_spi_nor_read_status(struct rtksfc_host *host)
{
	int i = 100;

	while (i--) {
		writel(0x05, host->regbase + SFC_OPCODE);
		writel(0x10, host->regbase + SFC_CTL);

		if ((*(volatile unsigned char *)host->iobase) & 0x1)
			msleep(100);
		else
			return 0;
	}

	return -1;
}

static void rtk_spi_nor_read_mode(struct rtksfc_host *host)
{
	unsigned int tmp;

	writel(0x03, host->regbase + SFC_OPCODE);
	writel(0x18, host->regbase + SFC_CTL);
	tmp = *(volatile unsigned int *)(host->iobase);
	
	return;
}

static int rtk_set_reg(void __iomem *addr, unsigned int offset, int length, unsigned int value)
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
static void rtk_spi_nor_dualread_mode(struct rtksfc_host *host)
{
        unsigned int tmp;

        writel(0x23b, host->regbase + SFC_OPCODE);

	if (soc_device_match(rtk_soc_stark)) {
		writel(0x19, host->regbase + SFC_CTL);
	} else {
		writel(0x18, host->regbase + SFC_CTL);
		rtk_set_reg(host->regbase + SFC_CTL, 24, 8, 8);
	}
        tmp = *(volatile unsigned int *)(host->iobase);

        return;
}
#endif

static void rtk_spi_nor_write_mode(struct rtksfc_host *host)
{
	writel(0x2, host->regbase + SFC_OPCODE);
	writel(0x18, host->regbase + SFC_CTL);

	return;
}

static void rtk_spi_nor_enable_auto_write(struct rtksfc_host *host)
{
	writel(0x105, host->regbase + SFC_WAIT_WR);
	writel(0x106, host->regbase + SFC_EN_WR);
}

static void rtk_spi_nor_disable_auto_write(struct rtksfc_host *host)
{
	writel(0x005, host->regbase + SFC_WAIT_WR);
	writel(0x006, host->regbase + SFC_EN_WR);
}

static void rtk_spi_nor_driving(void)
{
	void __iomem *regbase;

	regbase = ioremap(0x9804f234, 0x8);

	rtk_set_reg(regbase, 12, 1, 1);
	rtk_set_reg(regbase, 9, 3, 3);
	rtk_set_reg(regbase, 6, 3, 2);

	rtk_set_reg(regbase, 25, 1, 1);
	rtk_set_reg(regbase, 22, 3, 3);
	rtk_set_reg(regbase, 19, 3, 2);

	rtk_set_reg(regbase + 0x4, 12, 1, 1);
	rtk_set_reg(regbase + 0x4, 9, 3, 3);
	rtk_set_reg(regbase + 0x4, 6, 3, 2);

	rtk_set_reg(regbase + 0x4, 25, 1, 1);
	rtk_set_reg(regbase + 0x4, 22, 3, 3);
	rtk_set_reg(regbase + 0x4, 19, 3, 2);
	iounmap(regbase);
}

static void rtk_spi_nor_init(struct rtksfc_host *host)
{
	void __iomem *regbase;

	regbase = ioremap(0x98000018, 0x4);
	writel(readl(regbase) & ((~(0x1 << 0)) | (0x0 << 0)), regbase);
	iounmap(regbase);

	regbase = ioremap(0x9804e000, 0x200);
	rtk_set_reg(regbase + 0x120, 11, 1, 1);
	iounmap(regbase);

	host->iobase = ioremap(NOR_BASE_PHYS, 0x2000000);
	if (soc_device_match(rtk_soc_stark))
		host->mdbase = ioremap(MD_BASE_ADDE_STARK, 0x30);
	else
		host->mdbase = ioremap(MD_BASE_ADDE, 0x30);

	writel(0x00000003, host->regbase + SFC_SCK);
	writel(0x001a1307, host->regbase + SFC_CE);
	writel(0x00000000, host->regbase + SFC_POS_LATCH);
	writel(0x00000005, host->regbase + SFC_WAIT_WR);
	writel(0x00000006, host->regbase + SFC_EN_WR);

	rtk_spi_nor_driving();
	return;
}

static int rtkspi_command_read(struct rtksfc_host *host, const struct spi_mem_op *op)
{
	u8 opcode;
	size_t len = op->data.nbytes;
	unsigned int value;

	if (op->cmd.dtr)
		opcode = op->cmd.opcode >> 8;
	else
		opcode = op->cmd.opcode;

	writel(opcode, host->regbase + SFC_OPCODE);
	writel(0x00000010, host->regbase + SFC_CTL);
	value = *(volatile unsigned int *)host->iobase;
	//pr_info("SPINOR_OP_RDID:[0x%x]\n", value);
	memcpy_fromio(op->data.buf.in, (unsigned char *)host->iobase, len);

	return 0;
}
static int rtkspi_command_write(struct rtksfc_host *host, const struct spi_mem_op *op)
{
	u8 opcode;
	const u8 *buf = op->data.buf.out;
	unsigned int tmp;
	int ret = 0;

	if (op->cmd.dtr)
                opcode = op->cmd.opcode >> 8;
        else
                opcode = op->cmd.opcode;

	writel(opcode, host->regbase + SFC_OPCODE);

	switch (opcode) {
	case SPINOR_OP_WRSR:
		writel(0x10, host->regbase + SFC_CTL);
		*(volatile unsigned char *)(host->iobase) = buf[0];
		break;
        case SPINOR_OP_WREN:
		writel(0x0, host->regbase + SFC_CTL);
		tmp = *(volatile unsigned char *)(host->iobase);
		break;
        case SPINOR_OP_WRDI:
                writel(0x0, host->regbase + SFC_CTL);
		break;
        case SPINOR_OP_EN4B:
                writel(0x0, host->regbase + SFC_CTL);
                tmp = *(volatile unsigned int *)(host->iobase);
		/* controller setting */
		writel(0x1, host->regbase + SFP_OPCODE2);
		break;
        case SPINOR_OP_EX4B:
                writel(0x0, host->regbase + SFC_CTL);
		tmp = *(volatile unsigned int *)(host->iobase);
		/* controller setting */
                writel(0x0, host->regbase + SFP_OPCODE2);
                break;
	case SPINOR_OP_BE_4K:
	case SPINOR_OP_BE_4K_4B:
	case SPINOR_OP_SE:
	case SPINOR_OP_SE_4B:
                writel(0x08, host->regbase + SFC_CTL);
                tmp = *(volatile unsigned char *)(host->iobase + op->addr.val);
                ret = rtk_spi_nor_read_status(host);
                break;
	case SPINOR_OP_CHIP_ERASE:
		pr_info("Erase whole flash.\n");
		writel(0x0, host->regbase + SFC_CTL);
		tmp = *(volatile unsigned char *)host->iobase;
		ret = rtk_spi_nor_read_status(host);
		break;
        default:
		pr_warn("rtkspi_command_write, unknow command\n");
                break;
        }

	return ret;
}

static int rtk_spi_nor_byte_transfer(struct rtksfc_host *host, loff_t offset,
				size_t len, unsigned char *buf, u8 op_type)
{
	if (op_type == RTKSFC_OP_READ)
		memcpy_fromio(buf, (unsigned char *)(host->iobase + offset), len);
	else
		memcpy_toio((unsigned char *)(host->iobase + offset), buf, len);

	return len;
}

static int rtk_spi_nor_dma_transfer(struct rtksfc_host *host, loff_t offset,
					size_t len, u8 op_type)
{
	unsigned int val;
	uint64_t dma_buffer;
	uint32_t low_32_bits;
	uint8_t high_3_bits;

	writel(0x0a, host->mdbase + MD_FDMA_CTRL1);
	/* setup MD DDR addr and flash addr */
	if (soc_device_match(rtk_soc_stark)) {
		writel((unsigned long)(host->dma_buffer),
			host->mdbase + MD_FDMA_DDR_SADDR);
	} else {
		dma_buffer = (uint64_t)(host->dma_buffer);
		low_32_bits = dma_buffer & 0xFFFFFFFF;
		high_3_bits = (dma_buffer >> 32) & 0x7;

		writel((unsigned long)low_32_bits,
			host->mdbase + MD_FDMA_DDR_SADDR);
		writel((unsigned long)high_3_bits,
			host->mdbase + MD_FDMA_DDR_SADDR1);
	}

	writel((unsigned long)((volatile u8*)(NOR_BASE_PHYS + offset)),
				host->mdbase + MD_FDMA_FL_SADDR);
	if (op_type == RTKSFC_OP_READ)
		val = (0xC000000 | len);
	else
		val = (0x6000000 | len);

	writel(val, host->mdbase + MD_FDMA_CTRL2);
	/* go */
	writel(0x03, host->mdbase + MD_FDMA_CTRL1);
	udelay(1);

	while (readl(host->mdbase + MD_FDMA_CTRL1) & 0x1)
		udelay(100);

	return rtk_spi_nor_read_status(host);
}

static ssize_t rtk_spi_nor_read(struct rtksfc_host *host, const struct spi_mem_op *op)
{
	loff_t from, n_from;
	size_t len;
	size_t n_len = 0, r_len = 0;
	unsigned int offset;
	u_char *read_buf = op->data.buf.in;
	int ret;
	int i;
	from = op->addr.val;
	len = op->data.nbytes;

	offset = from & 0x3;
	/* Byte stage */
	if (offset != 0) {
		r_len = (offset > len) ? len : offset;
		rtk_spi_nor_read_mode(host);
		ret = rtk_spi_nor_byte_transfer(host, from, r_len, (u8 *)read_buf,
							RTKSFC_OP_READ);
	}
	n_from = from + r_len;
	n_len = len - r_len;

	/* DMA stage */
	while (n_len > 0) {
		r_len = (n_len >= RTKSFC_DMA_MAX_LEN) ? RTKSFC_DMA_MAX_LEN : n_len;

	//	if (opcode == 0x3b)
	//		rtk_spi_nor_dualread_mode(host);
	//	else
			rtk_spi_nor_read_mode(host);
		ret = rtk_spi_nor_dma_transfer(host, n_from, r_len,
							RTKSFC_OP_READ);
		if (ret) {
			pr_err("DMA read timeout\n");
			return ret;
		}

		for (i = 0; i < r_len; i++)
			*(u8 *)(read_buf + offset + i) = *(u8 *)(host->buffer + i);

		n_len -= r_len;
		offset += r_len;
		n_from += r_len;
	}
	return len;
}

static ssize_t rtk_spi_nor_write(struct rtksfc_host *host, const struct spi_mem_op *op)
{
	loff_t to = op->addr.val;
	size_t len = op->data.nbytes;
	const u_char *write_buf = op->data.buf.out;
	int r_len = (int)len, w_len = 0;
	int offset;
	int ret;
	int i;

	rtk_spi_nor_enable_auto_write(host);
	offset = to & 0x3;

	/* byte stage */
	if (offset != 0) {
		w_len = (offset > len) ? len : offset;
		rtk_spi_nor_write_mode(host);
		ret = rtk_spi_nor_byte_transfer(host, to, w_len, (u8 *)write_buf,
							RTKSFC_OP_WRITE);
	}

	to = to + w_len;
	r_len = (int)len - w_len;

	/* DMA stage */
	offset = 0;
	while (r_len > 0) {
		w_len = (r_len >= RTKSFC_DMA_MAX_LEN) ? RTKSFC_DMA_MAX_LEN : r_len;
		for(i = 0; i < w_len; i++)
			*(u8 *)(host->buffer + i) = *(u8 *)(write_buf + i);
		rtk_spi_nor_write_mode(host);
		ret = rtk_spi_nor_dma_transfer(host, to + offset, w_len,
							RTKSFC_OP_WRITE);
		r_len = r_len - w_len;
		offset = offset + w_len;
	}

	rtk_spi_nor_read_mode(host);
	rtk_spi_nor_disable_auto_write(host);

	return len;
}

static int rtk_nor_exec_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	struct rtksfc_host *host = spi_controller_get_devdata(mem->spi->master);

	if ((op->data.nbytes == 0) ||
	    ((op->addr.nbytes != 3) && (op->addr.nbytes != 4))) {
		if (op->data.dir == SPI_MEM_DATA_IN)
			return rtkspi_command_read(host, op);
		else
			return rtkspi_command_write(host, op);
	}

	if (op->data.dir == SPI_MEM_DATA_OUT)
		return rtk_spi_nor_write(host, op);
	if (op->data.dir == SPI_MEM_DATA_IN)
		return rtk_spi_nor_read(host, op);

	pr_warn("exec_op, unknow command:0x%x\n", op->cmd.opcode);
	return -EINVAL;
}

static bool rtk_nor_supports_op(struct spi_mem *mem,
				const struct spi_mem_op *op)
{
	if (op->cmd.buswidth > 1)
		return false;

	if (op->addr.nbytes != 0) {
		if (op->addr.buswidth > 1)
			return false;
		if (op->addr.nbytes < 3 || op->addr.nbytes > 4)
			return false;
	}

	if (op->dummy.nbytes != 0) {
		if (op->dummy.buswidth > 1 || op->dummy.nbytes > 7)
			return false;
	}

	if (op->data.nbytes != 0 && op->data.buswidth > 4)
		return false;

	return spi_mem_default_supports_op(mem, op);
}

static const char *rtk_nor_get_name(struct spi_mem *mem)
{
	struct rtksfc_host *host = spi_controller_get_devdata(mem->spi->master);
	struct device *dev = host->dev;

	return devm_kasprintf(dev, GFP_KERNEL, "RtkSFC");
}

static const struct spi_controller_mem_ops rtk_nor_mem_ops = {
	.supports_op = rtk_nor_supports_op,
	.exec_op = rtk_nor_exec_op,
	.get_name = rtk_nor_get_name,
};

static int rtk_spi_nor_probe(struct platform_device *pdev)
{
	struct spi_controller *ctrl;
	struct resource *res;
	struct rtksfc_host *host;
	int ret, mask;

	pr_info("start rtk_spi_nor_probe\n");
	ctrl = spi_alloc_master(&pdev->dev, sizeof(*host));
	if (!ctrl)
		return -ENOMEM;

	ctrl->mode_bits = SPI_RX_DUAL | SPI_TX_DUAL;
	ctrl->bus_num = -1;
	ctrl->mem_ops = &rtk_nor_mem_ops;
	ctrl->num_chipselect = 1;
	ctrl->dev.of_node = pdev->dev.of_node;
	platform_set_drvdata(pdev, ctrl);
	host = spi_controller_get_devdata(ctrl);
	host->dev = &pdev->dev;
	/* Map the registers */

	host->clk_nor = devm_clk_get(&pdev->dev, "nor");
        if (IS_ERR(host->clk_nor)) {
                pr_err("%s: clk_get() returns %ld\n", __func__,
                        PTR_ERR(host->clk_nor));
        }

        clk_prepare_enable(host->clk_nor);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->regbase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(host->regbase))
		return PTR_ERR(host->regbase);

	if (soc_device_match(rtk_soc_stark))
		mask = 32;
	else
		mask = 35;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(mask));
	if (ret) {
		pr_err("Unable to set dma mask: %d\n", mask);
		return ret;
	}

	host->buffer = dmam_alloc_coherent(&pdev->dev, RTKSFC_DMA_MAX_LEN,
			&host->dma_buffer, GFP_KERNEL);
	if (!host->buffer)
		return -ENOMEM;

	mutex_init(&host->lock);
	rtk_spi_nor_init(host);
	ret = spi_register_controller(ctrl);
	if (ret < 0)
		mutex_destroy(&host->lock);

	rtk_spi_nor_read_mode(host);
	pr_info("rtk_spi_nor_probe done\n");
	return ret;
}

static int rtk_spi_nor_remove(struct platform_device *pdev)
{
	struct spi_controller *ctrl = platform_get_drvdata(pdev);
	struct rtksfc_host *host = spi_controller_get_devdata(ctrl);

	mutex_destroy(&host->lock);
	return 0;
}

static const struct of_device_id rtk_spi_nor_dt_ids[] = {
	{ .compatible = "realtek,rtd1625-sfc"},
	{ .compatible = "realtek,rtd16xxb-sfc"},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, rtk_spi_nor_dt_ids);

static struct platform_driver rtk_spi_nor_driver = {
	.driver = {
		.name	= "rtk-sfc",
		.of_match_table = rtk_spi_nor_dt_ids,
	},
	.probe	= rtk_spi_nor_probe,
	.remove	= rtk_spi_nor_remove,
};
module_platform_driver(rtk_spi_nor_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<jyanchou@realtek.com>");
MODULE_DESCRIPTION("Realtek SPI Nor Flash Controller Driver");
