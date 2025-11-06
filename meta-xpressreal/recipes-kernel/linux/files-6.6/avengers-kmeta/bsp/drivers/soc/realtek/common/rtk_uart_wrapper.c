// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2024 Realtek Semiconductor Corp.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/printk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/reset.h>

#define MIS_IO_UART_WRA_ISR		0x10
#define MIS_IO_UART_WRA_ISR_EN		0x18


/* common data */
struct ur_common_data {
	struct device *dev;
	void __iomem *base;
	int irq;
	struct clk *clk;
	struct reset_control *rst;
};

enum uart_wrapper_isr_bits {
	WRA_ISR_IP_INT_SHIFT			= 1,
	WRA_ISR_TIMEOUT_INT_SHIFT		= 2,
	WRA_ISR_UART_DMA_TX_THD_SHIFT		= 4,
	WRA_ISR_UART_DMA_RX_THD_SHIFT		= 5,
	WRA_ISR_UART_DMA_LENGTH_ERR_SHIFT	= 6,
	WRA_ISR_UART_DMA_TX_RINGBUFF_ERR_SHIFT	= 7,
	WRA_ISR_H5_RX_ERR_SHIFT			= 8,
};

enum uart_wrapper_isr_enable_bits {
	WRA_ISR_EN_IP_INT_EN_SHIFT			= 1,
	WRA_ISR_EN_TIMEOUT_INT_EN_SHIFT			= 2,
	WRA_ISR_EN_UART_DMA_TX_THD_EN_SHIFT		= 4,
	WRA_ISR_EN_UART_DMA_RX_THD_EN_SHIFT		= 5,
	WRA_ISR_EN_UART_DMA_LENGTH_ERR_EN_SHIFT		= 6,
	WRA_ISR_EN_UART_DMA_TX_RINGBUFF_ERR_EN_SHIFT	= 7,
	WRA_ISR_EN_H5_RX_ERR_EN_SHIFT			= 8,
};

static const char *uart_wrapper_isr_string[] = {
	[WRA_ISR_IP_INT_SHIFT] = "ip",
	[WRA_ISR_TIMEOUT_INT_SHIFT] = "timeout",
	[WRA_ISR_UART_DMA_TX_THD_SHIFT] = "DMA TX THD",
	[WRA_ISR_UART_DMA_RX_THD_SHIFT] = "DMA RX THD",
	[WRA_ISR_UART_DMA_LENGTH_ERR_SHIFT]= "DMA LENGHTH ERROR",
	[WRA_ISR_UART_DMA_TX_RINGBUFF_ERR_SHIFT] = "DMA TX RINGBUFFER ERROR",
	[WRA_ISR_H5_RX_ERR_SHIFT] = "H5 RX ERROR",
};

static irqreturn_t ur_wrap_int_handler(int irq, void *id)
{
	struct platform_device *pdev = id;
	struct ur_common_data *ur = platform_get_drvdata(pdev);
	u32 val;
	int i;

	val = readl(ur->base + MIS_IO_UART_WRA_ISR);
	dev_dbg(&pdev->dev, "isr: 0x%08x\n", val);
	writel(val, ur->base + MIS_IO_UART_WRA_ISR);

	while (val) {
		i = __ffs(val);
		val &= ~BIT(i);
		if (i >= ARRAY_SIZE(uart_wrapper_isr_string))
			BUG();
		dev_dbg(&pdev->dev, "interrupt triggered by %s(%d)\n",
			uart_wrapper_isr_string[i] ?: "UNKNOWN", i);
	}

	return IRQ_HANDLED;
}

static void ur_wrap_enable_interrupt(struct ur_common_data *ur, u32 int_msk)
{
	u32 val;

	val = readl(ur->base + MIS_IO_UART_WRA_ISR_EN);
	val |= int_msk;
	writel(val, ur->base + MIS_IO_UART_WRA_ISR_EN);
}

static void ur_wrap_disable_interrupt(struct ur_common_data *ur, u32 int_msk)
{
	u32 val;

	val = readl(ur->base + MIS_IO_UART_WRA_ISR_EN);
	val &= int_msk;
	writel(val, ur->base + MIS_IO_UART_WRA_ISR_EN);
}

static int ur_wrap_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ur_common_data *ur;
	struct device_node *np = dev->of_node;
	struct resource res;
	int ret;

	ur = devm_kzalloc(dev, sizeof(*ur), GFP_KERNEL);
	if (!ur)
		return -ENOMEM;
	ur->dev = dev;

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		return ret;

	ur->base = devm_ioremap(dev, res.start, resource_size(&res));
	if (!ur->base)
		return -ENOMEM;

	ur->irq = irq_of_parse_and_map(np, 0);
	if (!ur->irq) {
		dev_err(dev, "failed to parse irq: %d\n", ur->irq);
		return -ENXIO;
	}

	ret = devm_request_irq(dev, ur->irq, ur_wrap_int_handler, IRQF_SHARED,
			       dev_name(dev), pdev);
	if (ret) {
		dev_err(dev, "failed to request irq: %d\n", ret);
		return -ENXIO;
	}

	ur->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ur->clk)) {
		dev_err(dev, "fail to get clk\n");
		return PTR_ERR(ur->clk);
	}

	ur->rst = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(ur->rst)) {
		dev_err(dev, "fail to get rstc\n");
		return PTR_ERR(ur->rst);
	}

	platform_set_drvdata(pdev, ur);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	pm_runtime_get_sync(&pdev->dev);

	ret = devm_of_platform_populate(&pdev->dev);
	if (ret)
		pm_runtime_disable(&pdev->dev);

	return ret;
}

static int ur_wrap_remove(struct platform_device *pdev)
{
	pm_runtime_put(&pdev->dev);

        pm_runtime_disable(&pdev->dev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static int ur_wrap_runtime_suspend(struct device *dev)
{
	struct ur_common_data *ur = dev_get_drvdata(dev);

	dev_dbg(dev, "enter %s\n", __func__);
	ur_wrap_disable_interrupt(ur, BIT(WRA_ISR_EN_IP_INT_EN_SHIFT));
	reset_control_assert(ur->rst);
	clk_disable_unprepare(ur->clk);
	dev_dbg(dev, "exit %s\n", __func__);
	return 0;
}

static int ur_wrap_runtime_resume(struct device *dev)
{
	struct ur_common_data *ur = dev_get_drvdata(dev);

	dev_dbg(dev, "enter %s\n", __func__);
	clk_prepare_enable(ur->clk);
	reset_control_deassert(ur->rst);
	ur_wrap_enable_interrupt(ur, BIT(WRA_ISR_EN_IP_INT_EN_SHIFT));
	dev_dbg(dev, "exit %s\n", __func__);
	return 0;
}

static struct dev_pm_ops ur_wrap_pm_ops = {
	RUNTIME_PM_OPS(ur_wrap_runtime_suspend, ur_wrap_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
};


static const struct of_device_id ur_wrap_match[] = {
	{.compatible = "realtek,uart-wrapper"},
	{},
};
MODULE_DEVICE_TABLE(of, ur_wrap_match);

static struct platform_driver ur_wrap_driver = {
	.probe  = ur_wrap_probe,
	.remove = ur_wrap_remove,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "rtk-uart-wrapper",
		.pm             = &ur_wrap_pm_ops,
		.of_match_table = of_match_ptr(ur_wrap_match),
	},
};

static void __exit ur_wrap_exit(void)
{
	platform_driver_unregister(&ur_wrap_driver);
}
module_exit(ur_wrap_exit);

static int __init ur_wrap_init(void)
{
	return platform_driver_register(&ur_wrap_driver);
}
subsys_initcall_sync(ur_wrap_init);

MODULE_DESCRIPTION("Realtek UART-Wrapper driver");
MODULE_LICENSE("GPL");
