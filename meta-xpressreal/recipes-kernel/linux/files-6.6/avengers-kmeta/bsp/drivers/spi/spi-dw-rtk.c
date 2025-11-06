// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Realtek SPI driver based on DW SPI Core on RTD139x,
 * RTD16xx, or RTD13xx platform
 *
 * Copyright (c) 2020 Realtek Corporation.
 */

#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>

#include "spi-dw.h"

#define DRIVER_NAME "spi-dw-rtk"
#define SPI_Wra_CTRL 0

#define SPI_WRAPPER_VER0 0x0
#define SPI_WRAPPER_VER1 0x1

struct rtk_spi {
	struct dw_spi dws;

	void __iomem *spi_wrapper;
	u32 spi_wrapper_ver;
	struct clk *clk;
	struct reset_control *rstc;
	struct device *dev;
	u32 clk_div;
};

struct rtk_spi_desc {
	u32 spi_wrapper_ver;
};

static const struct rtk_spi_desc rtk_desc = {
	.spi_wrapper_ver = SPI_WRAPPER_VER0
};

static const struct rtk_spi_desc rtd1625_desc = {
	.spi_wrapper_ver = SPI_WRAPPER_VER1
};

static inline struct rtk_spi *rtk_spi_to_hw(struct spi_device *spi)
{
	struct dw_spi *dws = spi_controller_get_devdata(spi->master);
	struct rtk_spi *hw = container_of(dws, struct rtk_spi, dws);

	return hw;
}

static inline void rtk_spi_wrap_ctrl(struct spi_device *spi,  int val)
{
	struct rtk_spi *hw = rtk_spi_to_hw(spi);

	writel(val, hw->spi_wrapper + SPI_Wra_CTRL);
}

static void rtk_spi_set_cs_v0(struct spi_device *spi, bool enable)
{
	rtk_spi_wrap_ctrl(spi, enable ? 0x17 : 0x13);

	dw_spi_set_cs(spi, enable);
}

static void rtk_spi_set_cs_v1(struct spi_device *spi, bool enable)
{
	rtk_spi_wrap_ctrl(spi, enable ? 0x30 : 0x20);

	dw_spi_set_cs(spi, enable);
}

static int rtk_spi_add_host(struct rtk_spi *hw)
{
	struct dw_spi *dws = &hw->dws;
	struct device *dev = hw->dev;
	int ret = 0;

	ret = dw_spi_add_host(dev, dws);
	if (ret) {
		dev_err(dev, "[SPI] Init failed, ret = %d\n", ret);
		goto exit;
	}
	dev_info(dev, "[SPI] num_cs %d, bus_num %d, max_freq %d, irq %d\n",
		 hw->dws.num_cs, hw->dws.host->bus_num, hw->dws.max_freq,
		 hw->dws.irq);
exit:
	return ret;
}

static int rtk_spi_probe_hw_v0(struct rtk_spi *hw)
{
	struct dw_spi *dws = &hw->dws;
	struct device *dev = hw->dev;
	struct clk *clk = clk_get(dev, NULL);
	struct reset_control *rstc =
		reset_control_get_exclusive(dev, NULL);
	int ret = 0;

	/* disable spi_X */
	reset_control_assert(rstc);	/* reset spi_X */

	/* Enable clk and release reset module */
	reset_control_deassert(rstc);	/* release reset */
	clk_prepare_enable(clk);	/* enable clk */

	hw->clk = clk;
	hw->rstc = rstc;
	dws->set_cs = rtk_spi_set_cs_v0;

	pm_runtime_enable(dev);

	/* call setup function */
	ret = rtk_spi_add_host(hw);
	if (ret)
		goto dw_err_exit;

	return 0;

dw_err_exit:
	pm_runtime_disable(dev);

	/* Disable clk and reset module */
	reset_control_assert(rstc);	/* reset */
	clk_disable_unprepare(clk);	/* disable clk */
	dev_set_drvdata(dev, NULL);

	/* release resource */
	reset_control_put(rstc);
	clk_put(clk);

	return ret;
}

static int rtk_spi_probe_hw_v1(struct rtk_spi *hw)
{
	struct dw_spi *dws = &hw->dws;
	struct device *dev = hw->dev;
	struct clk *clk_gspi = clk_get(dev, "gspi");
	struct clk *clk_spi = clk_get(dev, "spi");
	struct clk *clk_dma = clk_get_optional(dev, "isomis_dma");
	struct reset_control *rstc_gspi =
		reset_control_get_exclusive(dev, "gspi");
	struct reset_control *rstc_misc =
		reset_control_get_optional_exclusive(dev, "misc");
	struct reset_control *rstc_spi =
		reset_control_get_exclusive(dev, "spi");
	struct reset_control *rstc_dma =
		reset_control_get_optional_exclusive(dev, "isomis_dma");
	int ret = 0;

	/* disable spi_X */
	reset_control_assert(rstc_spi);	/* reset spi_X */

	/* Enable clk and release reset module */
	clk_prepare_enable(clk_gspi);
	reset_control_deassert(rstc_gspi);

	reset_control_deassert(rstc_misc);
	reset_control_deassert(rstc_spi);
	reset_control_deassert(rstc_dma);

	clk_prepare_enable(clk_spi);
	clk_prepare_enable(clk_dma);

	hw->clk = clk_spi;
	hw->rstc = rstc_spi;
	dws->set_cs = rtk_spi_set_cs_v1;

	pm_runtime_enable(dev);

	writel(0xef, hw->spi_wrapper + SPI_Wra_CTRL);

	/* call setup function */
	ret = rtk_spi_add_host(hw);
	if (ret)
		goto dw_err_exit;

	/* release resource */
	reset_control_put(rstc_gspi);
	reset_control_put(rstc_misc);
	reset_control_put(rstc_dma);
	clk_put(clk_gspi);
	clk_put(clk_dma);

	return 0;

dw_err_exit:
	pm_runtime_disable(dev);

	/* Disable clk and reset module */
	reset_control_assert(rstc_spi);	/* reset */
	clk_disable_unprepare(clk_spi);	/* disable clk */
	dev_set_drvdata(dev, NULL);

	/* release resource */
	reset_control_put(rstc_gspi);
	reset_control_put(rstc_spi);
	reset_control_put(rstc_misc);
	reset_control_put(rstc_dma);
	clk_put(clk_gspi);
	clk_put(clk_spi);
	clk_put(clk_dma);

	return ret;
}

static int rtk_spi_probe(struct platform_device *pdev)
{
	struct rtk_spi *hw;
	struct dw_spi *dws;
	const struct rtk_spi_desc *spi_desc;
	u32 val;
	int ret = -ENODEV;

	if (WARN_ON(!(pdev->dev.of_node))) {
		dev_err(&pdev->dev, "[SPI] Error: No dts node\n");
		goto exit;
	}

	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	if (!hw) {
		ret = -ENOMEM;
		goto exit;
	}

	dev_set_drvdata(&pdev->dev, hw);
	hw->dev = &pdev->dev;
	dws = &hw->dws;

	/* Find and map resources */
	spi_desc = of_device_get_match_data(&pdev->dev);
	if (!spi_desc) {
		dev_err(&pdev->dev, "[SPI] fail to retrieve spi_desc\n");
		goto exit;
	}
	hw->spi_wrapper_ver = spi_desc->spi_wrapper_ver;
	dev_info(&pdev->dev, "spi_wrapper_ver=%d\n", hw->spi_wrapper_ver);

	dws->regs = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(dws->regs)) {
		dev_err(&pdev->dev,
			"[SPI] DW SPI region map failed, addr 0x%p\n",
			dws->regs);
		goto exit;
	}
	hw->spi_wrapper = of_iomap(pdev->dev.of_node, 1);

	if (IS_ERR(dws->regs)) {
		dev_err(&pdev->dev,
			"[SPI] SPI wrapper region map failed, addr 0x%p\n",
			hw->spi_wrapper);
		goto exit;
	}

	dws->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (dws->irq < 0) {
		dev_err(&pdev->dev, "[SPI] no irq resource?\n");
		ret = -ENXIO;
		goto exit;
	}

	if (!of_property_read_u32(pdev->dev.of_node, "num-chipselect", &val))
		dws->num_cs = val;
	if (!of_property_read_u32(pdev->dev.of_node, "bus-num", &val))
		dws->bus_num = val;
	if (!of_property_read_u32(pdev->dev.of_node, "clock-frequency", &val))
		dws->max_freq = val;

	switch (hw->spi_wrapper_ver) {
	case SPI_WRAPPER_VER0:
		ret = rtk_spi_probe_hw_v0(hw);
		break;
	case SPI_WRAPPER_VER1:
		ret = rtk_spi_probe_hw_v1(hw);
		break;
	default:
		dev_err(&pdev->dev, "[SPI] unknown version %d\n",
			hw->spi_wrapper_ver);
		ret = -EINVAL;
		goto exit;
	}
	if (ret)
		dev_err(&pdev->dev, "[SPI] failed to init hw v%d\n",
			hw->spi_wrapper_ver);

exit:
	return ret;
}

static int rtk_spi_remove(struct platform_device *dev)
{
	struct rtk_spi *hw = platform_get_drvdata(dev);
	struct dw_spi *dws = &hw->dws;

	dw_spi_remove_host(dws);

	pm_runtime_disable(&dev->dev);

	/* Disable clk and reset module */
	reset_control_assert(hw->rstc);
	clk_disable_unprepare(hw->clk);
	reset_control_put(hw->rstc);
	clk_put(hw->clk);

	dev_set_drvdata(&dev->dev, NULL);
	return 0;
}


#ifdef CONFIG_PM_SLEEP

static int rtk_spi_suspend(struct device *dev)
{
	struct rtk_spi *hw = dev_get_drvdata(dev);
	struct dw_spi *dws = &hw->dws;
	int ret;

	dev_info(dev, "[SPI] Enter %s\n", __func__);

	/* save SPI baud rate */
	hw->clk_div = dw_readl(dws, DW_SPI_BAUDR);

	ret = dw_spi_suspend_host(dws);

	dev_info(dev, "[SPI] Exit %s\n", __func__);

	return ret;
}

static int rtk_spi_resume(struct device *dev)
{
	struct rtk_spi *hw = dev_get_drvdata(dev);
	struct dw_spi *dws = &hw->dws;
	int ret;

	dev_info(dev, "[SPI] Enter %s\n", __func__);

	ret = dw_spi_resume_host(dws);

	/* restore SPI baud rate */
	dw_spi_enable_chip(dws, 0);
	dw_spi_set_clk(dws, hw->clk_div);
	dw_spi_enable_chip(dws, 1);

	dev_info(dev, "[SPI] Exit %s\n", __func__);

	return ret;
}

static SIMPLE_DEV_PM_OPS(rtk_spi_pm_ops, rtk_spi_suspend, rtk_spi_resume);

#define RTK_SPI_PM_OPS	(&rtk_spi_pm_ops)

#else /* CONFIG_PM_SLEEP */
#define RTK_SPI_PM_OPS	NULL
#endif /* CONFIG_PM_SLEEP */


static const struct of_device_id rtk_spi_match[] = {
	{ .compatible = "realtek,rtk-dw-apb-ssi", .data = &rtk_desc},
	{ .compatible = "realtek,rtd1625-dw-apb-ssi", .data = &rtd1625_desc},
	{},
};
MODULE_DEVICE_TABLE(of, rtk_spi_match);

static struct platform_driver rtk_spi_driver = {
	.probe = rtk_spi_probe,
	.remove = rtk_spi_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = RTK_SPI_PM_OPS,
		.of_match_table = of_match_ptr(rtk_spi_match),
	},
};
module_platform_driver(rtk_spi_driver);

MODULE_AUTHOR("Eric Wang <ericwang@realtek.com>");
MODULE_DESCRIPTION("SPI driver based on DW SPI Core on RTK platform");
MODULE_LICENSE("GPL v2");
