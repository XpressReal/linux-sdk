// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for Realtek IR Receiver Controller
 *
 * Copyright (C) 2020 Simon Hsu <simon_hsu@realtek.com>
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/interrupt.h>

#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "rtk-ir.h"

#define RTK_IR_DEV "rtk-ir"

static const struct of_device_id rtk_ir_match[] = {
	{ .compatible = "realtek,rtk-ir" },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_ir_match);

static irqreturn_t rtk_ir_irq(int irq, void *dev_id)
{
	struct rtk_ir __maybe_unused *ir = (struct rtk_ir *)dev_id;

	rtk_ir_isr_hw(ir);
	rtk_ir_isr_hw1(ir);
	rtk_ir_isr_raw(ir);

	return IRQ_HANDLED;
}

static int rtk_ir_wakeup_key_handle(struct rtk_ir *ir)
{
	struct rc_dev *rc = ir->hw.rc;
	struct rc_map *map = &rc->rc_map;

	struct rtk_ir_hw_decoder *dec = ir->hw.dec;
	struct rtk_ir_scancode_req request = {0};

	unsigned int i, j, val;

	regmap_read(ir->iso, 0x65c, &val);
	dec->scancode(&request, val);
	for (i = 0; i < map->len; i++)
		if (request.scancode == (map->scan + i)->scancode)
			break;

	if (i < map->len)
		regmap_write(ir->iso, 0x65c, (map->scan + i)->keycode);

	if (i == map->len)
		for (j = 0; j < map->len; j++)
			if (KEY_POWER == (map->scan + j)->keycode) {
				request.scancode = (map->scan + j)->scancode;
				break;
			}
	return 0;
}

static int rtk_ir_key_event(struct notifier_block *notifier,
			    unsigned long pm_event,
			    void *unused)
{
	struct rtk_ir *ir = container_of(notifier, struct rtk_ir, pm_notifier);
	struct device *dev = ir->dev;
	struct rc_dev *rc = ir->hw.rc;
	struct rc_map *map = &rc->rc_map;
	struct pm_dev_param *node;
	bool sendKeyEvent = false;
	unsigned int wakeup_event, i = 0, j = 0, k = 0, val, nf_code;

	if (pm_event == PM_SUSPEND_PREPARE)
		return 0;

	node = rtk_pm_get_param(PM);
	wakeup_event = *((unsigned int *) node->data);

	/* error handling */
	if (wakeup_event >= MAX_EVENT) {
		dev_err(dev, "Bad wakeup event value %d !\n", wakeup_event);
		return NOTIFY_DONE;
	}

	/* after resume */
	switch (wakeup_event) {
	case ALARM_EVENT:
		sendKeyEvent = true;
		break;
	case TIMER_EVENT:
		sendKeyEvent = false;
		break;
	default:
		sendKeyEvent = true;
		break;
	}

	if (sendKeyEvent) {
		struct rtk_ir_hw_decoder *dec = ir->hw.dec;
		struct rtk_ir_scancode_req request;

		regmap_read(ir->iso, 0x65c, &val);
		regmap_read(ir->iso, 0x650, &nf_code);
		dev_info(dev, "Wakeup entire android src %d, 0x%x, %d\n", wakeup_event, val, nf_code);
		dec->scancode(&request, val);

		if (wakeup_event == IR_EVENT) {
			for (i = 0; i < map->len; i++)
				if (request.scancode == (map->scan + i)->scancode)
					break;
		} else if (wakeup_event == GPIO_EVENT && nf_code == 25856) {
			// KEY_PROG1: Netflix Button
			for (j = 0; j < map->len; j++)
				if (KEY_PROG1 == (map->scan + j)->keycode){
					request.scancode = (map->scan + j)->scancode;
					break;
				}
		} else if (wakeup_event == ALARM_EVENT) {
			// KEY_E: Global key for wifi connected
			for (k = 0; k < map->len; k++)
				if (KEY_E == (map->scan + k)->keycode){
					request.scancode = (map->scan + k)->scancode;
					break;
				}
		} else {
			for (j = 0; j < map->len; j++)
				if (KEY_POWER == (map->scan + j)->keycode){
					request.scancode = (map->scan + j)->scancode;
					break;
				}
		}

		if (wakeup_event == IR_EVENT) {
			pr_err("[IRDA_RESUME][%s] Scan code: 0x%x\n", __func__, request.scancode);
			rc_keydown(rc, map->rc_proto, request.scancode, 0);
		} else if (wakeup_event == GPIO_EVENT) {
			pr_err("[GPIO_RESUME][%s] Scan code: 0x%x\n", __func__, request.scancode);
			rc_keydown(rc, map->rc_proto, request.scancode, 0);
		} else if (wakeup_event == ALARM_EVENT) {
			pr_err("[ALARM_RESUME][%s] Check Wi-Fi connection, scan code: 0x%x\n", __func__, request.scancode);
			rc_keydown(rc, map->rc_proto, request.scancode, 0);
		} else if (wakeup_event == CEC_EVENT) {
			pr_err("[CEC_RESUME][%s] Scan code: 0x%x\n", __func__, request.scancode);
			rc_keydown(rc, map->rc_proto, request.scancode, 0);
		} else if (wakeup_event == HIFI_EVENT){
			pr_err("[HIFI_RESUME][%s] Scan code: 0x%x\n", __func__, request.scancode);
			rc_keydown(rc, map->rc_proto, request.scancode, 0);
		}
	}

	return NOTIFY_DONE;
}

static void rtk_ir_get_wakeinfo_v1(struct rtk_ir_hw *hw, struct ipc_shm_irda_v1 *data,
				   unsigned int keycode, unsigned int *custcode)
{
	struct rc_dev *rc = hw->rc;
	struct rc_map *map = &rc->rc_map;
	struct irda_wakeup_key_v1 *tbl;
	struct rtk_ir_wakeinfo info;
	int i, j, idx;
	int check_custcode = 0;

	for (j = 0; j < MAX_KEY_TBL; j++)
		if (custcode[j] != 0)
			check_custcode = 1;

	for (i = 0; i < map->len; i++) {
		if (keycode != (map->scan + i)->keycode && keycode != 0xffffffff)
			continue;

		if (keycode == 0xffffffff)
			hw->dec->wakeinfo(&info, keycode);
		else
			hw->dec->wakeinfo(&info, (map->scan + i)->scancode);

		if (check_custcode) {
			for (j = 0; j < MAX_KEY_TBL; j++)
				if (info.addr == custcode[j])
					break;
			if (j == MAX_KEY_TBL)
				continue;
		}

		for (j = 0; j < MAX_KEY_TBL; j++) {
			tbl = data->key_tbl + j;
			idx = htonl(tbl->wakeup_keynum);
			if (idx != 0 && info.addr != htonl(tbl->cus_code))
				continue;

			if (idx == 0) {
				tbl->cus_code = htonl(info.addr);
				tbl->cus_mask = htonl(info.addr_msk);
			}
			tbl->scancode_mask = htonl(info.scancode_msk);
			tbl->wakeup_scancode[idx++] = htonl(info.scancode);
			tbl->wakeup_keynum = htonl(idx);

			pr_info("irda wake-tbl : [%d, %d] = 0x%x, 0x%x\n",
				j, idx-1, info.scancode, info.addr);

			if (keycode == 0xffffffff)
				return;
			break;
		}
	}
	return;
}

static void rtk_ir_get_wakeinfo_v2(struct rtk_ir_hw *hw, struct ipc_shm_irda_v2 *data,
				   unsigned int keycode, unsigned int *custcode)
{
	struct rc_dev *rc = hw->rc;
	struct rc_map *map = &rc->rc_map;
	struct irda_wakeup_key_v2 *tbl;
	struct rtk_ir_wakeinfo info;
	int i, j, idx;
	int check_custcode = 0;

	for (j = 0; j < MAX_KEY_TBL; j++)
		if (custcode[j] != 0)
			check_custcode = 1;

	for (i = 0; i < map->len; i++) {
		if (keycode != (map->scan + i)->keycode && keycode != 0xffffffff)
			continue;

		if (keycode == 0xffffffff)
			hw->dec->wakeinfo(&info, keycode);
		else
			hw->dec->wakeinfo(&info, (map->scan + i)->scancode);

		if (check_custcode) {
			for (j = 0; j < MAX_KEY_TBL; j++)
				if (info.addr == custcode[j])
					break;
			if (j == MAX_KEY_TBL)
				continue;
		}

		for (j = 0; j < MAX_KEY_TBL; j++) {
			tbl = data->key_tbl + j;
			idx = tbl->pkey_nr;
			if (idx != 0 && info.addr != tbl->ckey)
				continue;

			if (idx == 0) {
				tbl->ckey = info.addr;
				tbl->ckey_start = __ffs(info.addr_msk);
				tbl->ckey_end = __fls(info.addr_msk);
			}
			tbl->pkey_start = __ffs(info.scancode_msk);
			tbl->pkey_end = __fls(info.scancode_msk);
			tbl->pkeys[idx++] = info.scancode;
			tbl->pkey_nr = idx;
			tbl->valid = 1;

			data->ckey_nr++;
			pr_info("irda wake-tbl : [%d, %d] = 0x%x, 0x%x\n", j,
				idx - 1, info.scancode, info.addr);

			if (keycode == 0xffffffff)
				return;
			break;
		}
	}
	return;
}

static void rtk_ir_set_wakeup(struct rtk_ir *ir)
{
	struct device *dev = ir->dev;
	struct device_node *np = dev->of_node;
	struct ipc_shm_irda_v1 *pcpu_data_v1 = &ir->pcpu_data_v1;
	struct ipc_shm_irda_v2 *pcpu_data_v2 = &ir->pcpu_data_v2;
	unsigned int keycode[MAX_WAKEUP_CODE];
	unsigned int custcode[MAX_KEY_TBL] = {0};
	int i, num;

	struct pm_dev_param *pm_node = rtk_pm_get_param(PM);
	struct pm_private *dev_pm = dev_get_drvdata(pm_node->dev);

	num = of_property_count_u32_elems(np, "wakeup-custcode");
	if (num > 0) {
		if (num > MAX_KEY_TBL)
			num = MAX_KEY_TBL;
		of_property_read_u32_array(np, "wakeup-custcode", custcode, num);
	}

	num = of_property_count_u32_elems(np, "wakeup-key");
	if (num < 0) {
		num = 1;
		keycode[0] = KEY_POWER;
	} else {
		if (num > MAX_WAKEUP_CODE)
			num = MAX_WAKEUP_CODE;
		of_property_read_u32_array(np, "wakeup-key", keycode, num);
	}

	if (ir->hw.rc) {
		if (dev_pm->version == RTK_PCPU_VERSION_V2) {
			for (i = 0; i < num; i++)
				rtk_ir_get_wakeinfo_v2(&ir->hw, pcpu_data_v2, keycode[i], custcode);
			ir->pm_param.data = pcpu_data_v2;
		} else {
			for (i = 0; i < num; i++)
				rtk_ir_get_wakeinfo_v1(&ir->hw, pcpu_data_v1, keycode[i], custcode);
			pcpu_data_v1->dev_count++;
			ir->pm_param.data = pcpu_data_v1;
		}
	}

	if (ir->hw1.rc) {
		if (dev_pm->version == RTK_PCPU_VERSION_V2) {
			for (i = 0; i < num; i++)
				rtk_ir_get_wakeinfo_v2(&ir->hw1, pcpu_data_v2, keycode[i], custcode);
			ir->pm_param.data = pcpu_data_v2;
		} else {
			for (i = 0; i < num; i++)
				rtk_ir_get_wakeinfo_v1(&ir->hw1, pcpu_data_v1, keycode[i], custcode);
			pcpu_data_v1->dev_count++;
			ir->pm_param.data = pcpu_data_v1;
		}
	}

	ir->pm_param.dev = ir->dev;
	ir->pm_param.dev_type = IRDA;

	rtk_pm_add_list(&ir->pm_param);
}

static int rtk_ir_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct rtk_ir *ir;
	int ret;
	unsigned int wake_event;

	ir = devm_kzalloc(dev, sizeof(*ir), GFP_KERNEL);
	if (!ir)
		return -ENOMEM;

	ir->dev = dev;
	ir->clk = devm_clk_get(dev, "irda");
	if (IS_ERR(ir->clk)) {
		dev_err(dev, "failed to get ir clock.\n");
		return PTR_ERR(ir->clk);
	}
	ir->rsts = devm_reset_control_array_get_optional_shared(dev);
	if (IS_ERR(ir->rsts)) {
		dev_err(dev, "failed to get ir reset.\n");
		return PTR_ERR(ir->rsts);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ir->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(ir->base))
		return PTR_ERR(ir->base);

	ir->iso = syscon_regmap_lookup_by_phandle(dev->of_node, "syscon");
	if (IS_ERR(ir->iso)) {
		dev_err(dev, "failed to remap iso regs\n");
		return PTR_ERR(ir->iso);
	}

	platform_set_drvdata(pdev, ir);

	ir->irq = platform_get_irq(pdev, 0);
	if (ir->irq < 0)
		return -ENODEV;

	ret = devm_request_irq(dev, ir->irq, rtk_ir_irq, 0, RTK_IR_DEV, ir);
	if (ret) {
		dev_err(dev, "failed request irq\n");
		return -EINVAL;
	}

	if (clk_prepare_enable(ir->clk)) {
		dev_err(dev, "try to enable ir_clk failed\n");
		return -EINVAL;
	}
	reset_control_deassert(ir->rsts);

#if IS_ENABLED(CONFIG_IR_RTK_HW)
	if (rtk_ir_hw_probe(ir)) {
		dev_err(dev, "ir hw probe fail");
		goto err_probe_fail;
	}
#endif
#if IS_ENABLED(CONFIG_IR_RTK_HW1)
	if (rtk_ir_hw1_probe(ir)) {
		dev_err(dev, "ir hw1 probe fail");
		goto err_probe_fail;
	}
#endif
#if IS_ENABLED(CONFIG_IR_RTK_RAW)
	if (rtk_ir_raw_probe(ir)) {
		dev_err(dev, "ir raw probe fail");
		goto err_probe_fail;
	}
#endif

	if (of_property_read_u32(dev->of_node, "wake-event", &wake_event))
		wake_event = 1;

	if (wake_event) {
		dev_info(dev, "add pm notifier for send wake event\n");
		ir->pm_notifier.notifier_call = rtk_ir_key_event;
		ret = register_pm_notifier(&ir->pm_notifier);
		if (ret) {
			dev_err(dev, "error registering pm notifier(%d)\n", ret);
			goto err_probe_fail;
		}
	}

	rtk_ir_set_wakeup(ir);
	rtk_ir_wakeup_key_handle(ir);

	return 0;

err_probe_fail:
	return -1;
}

static int rtk_ir_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int rtk_ir_suspend(struct device *dev)
{
	struct rtk_ir __maybe_unused *ir = dev_get_drvdata(dev);

	rtk_ir_hw_suspend(ir);
	rtk_ir_hw1_suspend(ir);

	return 0;
}

static int rtk_ir_resume(struct device *dev)
{
	struct rtk_ir __maybe_unused *ir = dev_get_drvdata(dev);

	rtk_ir_hw_resume(ir);
	rtk_ir_hw1_resume(ir);

	return 0;
}

static void rtk_ir_shutdown(struct platform_device *pdev)
{
	rtk_ir_suspend(&pdev->dev);
}

static const struct dev_pm_ops rtk_irda_pm_ops = {
	.suspend = rtk_ir_suspend,
	.resume = rtk_ir_resume,
};

#else

static const struct dev_pm_ops rtk_irda_pm_ops = {};

#endif

static struct platform_driver rtk_ir_driver = {
	.probe          = rtk_ir_probe,
	.remove         = rtk_ir_remove,
	.driver = {
		.name = RTK_IR_DEV,
		.of_match_table = rtk_ir_match,
#ifdef CONFIG_PM
		.pm = &rtk_irda_pm_ops,
#endif /* CONFIG_PM */
	},
#ifdef CONFIG_PM
	.shutdown = rtk_ir_shutdown,
#endif /* CONFIG_PM */
};

module_platform_driver(rtk_ir_driver);

MODULE_DESCRIPTION("Realtek IR Receiver Controller Driver");
MODULE_AUTHOR("Simon Hsu <simon_hsu@realtek.com>");
MODULE_LICENSE("GPL");
