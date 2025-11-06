// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek DHC SoC family power management driver
 * Copyright (c) 2020-2021 Realtek Semiconductor Corp.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/arm-smccc.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/psci.h>

#include <soc/realtek/rtk_pm.h>
#include <soc/realtek/uapi/rtk_timer.h>
#include <soc/realtek/uapi/rtk_gpio.h>
#include <soc/realtek/uapi/rtk_ir.h>
#include <soc/realtek/uapi/rtk_rtc.h>

static LIST_HEAD(rtk_pm_param_list);

size_t rtk_pm_get_gpio_size(struct pm_private *dev_pm, int type)
{
	void *pcpu_param = dev_pm->pcpu_param;
	int version = dev_pm->version;

	if (version == RTK_PCPU_VERSION_V2) {
		if (type == GET_GPIO_EN)
			return sizeof(((struct pm_pcpu_param_v2 *)pcpu_param)->wu_gpio_en);
		else
			return sizeof(((struct pm_pcpu_param_v2 *)pcpu_param)->wu_gpio_act);
	} else {
		if (type == GET_GPIO_EN)
			return sizeof(((struct pm_pcpu_param_v1 *)pcpu_param)->wu_gpio_en);
		else
			return sizeof(((struct pm_pcpu_param_v1 *)pcpu_param)->wu_gpio_act);
	}
}

char *rtk_pm_get_gpio(struct pm_private *dev_pm, int type)
{
	void *pcpu_param = dev_pm->pcpu_param;
	int version = dev_pm->version;

	if (version == RTK_PCPU_VERSION_V2) {
		if (type == GET_GPIO_EN)
			return ((struct pm_pcpu_param_v2 *)pcpu_param)->wu_gpio_en;
		else
			return ((struct pm_pcpu_param_v2 *)pcpu_param)->wu_gpio_act;
	} else {
		if (type == GET_GPIO_EN)
			return ((struct pm_pcpu_param_v1 *)pcpu_param)->wu_gpio_en;
		else
			return ((struct pm_pcpu_param_v1 *)pcpu_param)->wu_gpio_act;
	}
}

void rtk_pm_set_gpio(struct pm_private *dev_pm, int type, char *val)
{
	char *wu_gpio = rtk_pm_get_gpio(dev_pm, type);
	size_t size = rtk_pm_get_gpio_size(dev_pm, type);

	memcpy(wu_gpio, val, size);
}

int rtk_pm_get_timeout(struct pm_private *dev_pm)
{
	void *pcpu_param = dev_pm->pcpu_param;
	int version = dev_pm->version;

	if (version == RTK_PCPU_VERSION_V2)
		return ((struct pm_pcpu_param_v2 *)pcpu_param)->timerout_val;
	else
		return ((struct pm_pcpu_param_v1 *)pcpu_param)->timerout_val;
}

void rtk_pm_set_timeout(struct pm_private *dev_pm, unsigned int val)
{
	void *pcpu_param = dev_pm->pcpu_param;
	int version = dev_pm->version;

	if (version == RTK_PCPU_VERSION_V2)
		((struct pm_pcpu_param_v2 *)pcpu_param)->timerout_val = val;
	else
		((struct pm_pcpu_param_v1 *)pcpu_param)->timerout_val = val;
}

int rtk_pm_get_bt(struct pm_private *dev_pm)
{
	void *pcpu_param = dev_pm->pcpu_param;
	int version = dev_pm->version;

	if (version == RTK_PCPU_VERSION_V2)
		return ((struct pm_pcpu_param_v2 *)pcpu_param)->bt;
	else
		return ((struct pm_pcpu_param_v1 *)pcpu_param)->bt;
}

void rtk_pm_set_bt(struct pm_private *dev_pm, unsigned int val)
{
	void *pcpu_param = dev_pm->pcpu_param;
	int version = dev_pm->version;

	if (version == RTK_PCPU_VERSION_V2)
		((struct pm_pcpu_param_v2 *)pcpu_param)->bt = val;
	else
		((struct pm_pcpu_param_v1 *)pcpu_param)->bt = val;
}

int rtk_pm_get_wakeup_source(struct pm_private *dev_pm)
{
	void *pcpu_param = dev_pm->pcpu_param;
	int version = dev_pm->version;

	if (version == RTK_PCPU_VERSION_V2)
		return ((struct pm_pcpu_param_v2 *)pcpu_param)->wakeup_source;
	else
		return ((struct pm_pcpu_param_v1 *)pcpu_param)->wakeup_source;
}
EXPORT_SYMBOL(rtk_pm_get_wakeup_source);

void rtk_pm_set_wakeup_source(struct pm_private *dev_pm, unsigned int val)
{
	void *pcpu_param = dev_pm->pcpu_param;
	int version = dev_pm->version;

	if (version == RTK_PCPU_VERSION_V2)
		((struct pm_pcpu_param_v2 *)pcpu_param)->wakeup_source = val;
	else
		((struct pm_pcpu_param_v1 *)pcpu_param)->wakeup_source = val;
}
EXPORT_SYMBOL(rtk_pm_set_wakeup_source);

struct device *rtk_pm_get_dev(void)
{
	struct pm_dev_param *pm_param = rtk_pm_get_param(PM);

	if (pm_param && pm_param->dev)
		return pm_param->dev;
	else
		return NULL;
}
EXPORT_SYMBOL(rtk_pm_get_dev);

void rtk_pm_add_list(struct pm_dev_param *pm_node)
{
	list_add(&pm_node->list, &rtk_pm_param_list);
}
EXPORT_SYMBOL(rtk_pm_add_list);

void rtk_pm_del_list(struct pm_dev_param *pm_node)
{
	list_del(&pm_node->list);
}
EXPORT_SYMBOL(rtk_pm_del_list);

struct pm_dev_param *rtk_pm_get_param(unsigned int id)
{
	struct pm_dev_param *node;

	list_for_each_entry(node, &rtk_pm_param_list, list) {
		if (node->dev_type == id)
			return node;
	}

	return NULL;
}
EXPORT_SYMBOL(rtk_pm_get_param);

unsigned int rtk_pm_get_param_mask(void)
{
	/* CONFIG_R8169SOC, CONFIG_IR_RTK, CONFIG_RTK_HDMI_CEC */
	unsigned long param_mask = ~(BIT(LAN_EVENT) | BIT(IR_EVENT) | BIT(CEC_EVENT));
	struct pm_dev_param *node;

	list_for_each_entry(node, &rtk_pm_param_list, list) {
		if (node->dev_type)
			param_mask |= BIT(node->dev_type - 1);
	}

	return (unsigned int)param_mask;
}
EXPORT_SYMBOL(rtk_pm_get_param_mask);

void rtk_pm_set_pcpu_param_v1(struct device *dev)
{
	struct pm_private *dev_pm = dev_get_drvdata(dev);
	struct pm_pcpu_param_v1 *pcpu_param = (struct pm_pcpu_param_v1 *) dev_pm->pcpu_param;
	struct arm_smccc_res res;
	struct pm_dev_param *node;
	struct ipc_shm_irda_v1 *irda = &pcpu_param->irda_info;
	struct ipc_shm_cec_v1 *cec = &pcpu_param->cec_info;
	unsigned int wakeup_source = rtk_pm_get_wakeup_source(dev_pm);

	wakeup_source &= rtk_pm_get_param_mask();
	rtk_pm_set_wakeup_source(dev_pm, htonl(wakeup_source));
	rtk_pm_set_timeout(dev_pm, htonl(rtk_pm_get_timeout(dev_pm)));

	/* The bt data endianness is swapped in the PCPU FW. */
	rtk_pm_set_bt(dev_pm, rtk_pm_get_bt(dev_pm));

	list_for_each_entry(node, &rtk_pm_param_list, list) {
		switch (node->dev_type) {
		case LAN:
			break;
		case IRDA:
			memcpy(irda, node->data, sizeof(*irda));
			break;
		case GPIO:
			break;
		case ALARM_TIMER:
			break;
		case TIMER:
			break;
		case CEC:
			memcpy(cec, node->data, sizeof(*cec));
			break;
		case USB:
			break;
		default:
			break;
		}
	}

	arm_smccc_smc(0x8400ff04, dev_pm->pcpu_param_pa, 0, 0, 0, 0, 0, 0, &res);

	rtk_pm_set_wakeup_source(dev_pm, htonl(rtk_pm_get_wakeup_source(dev_pm)));
	rtk_pm_set_timeout(dev_pm, htonl(rtk_pm_get_timeout(dev_pm)));

	/* The bt data endianness is swapped in the PCPU FW. */
	rtk_pm_set_bt(dev_pm, rtk_pm_get_bt(dev_pm));
}
EXPORT_SYMBOL(rtk_pm_set_pcpu_param_v1);

void rtk_pm_notfiy_pcpu_mode(struct device *dev, int mode)
{
	struct pm_private *dev_pm = dev_get_drvdata(dev);
	unsigned int wakeup_source = rtk_pm_get_wakeup_source(dev_pm);
	struct pm_conf_data *config;
	dma_addr_t pcpu_param_pa;
	struct arm_smccc_res res;
	void *pcpu_param_va;

	wakeup_source &= rtk_pm_get_param_mask();
	rtk_pm_set_wakeup_source(dev_pm, wakeup_source);

	pcpu_param_va = dma_alloc_coherent(dev, sizeof(struct pm_conf_data), &pcpu_param_pa, GFP_KERNEL);
	config = (struct pm_conf_data *) pcpu_param_va;

	if (mode == POWER_S3) {
		config->type = POWER_S3;
		config->sel = 0;
		config->wksrc_en = wakeup_source;
	} else if (mode == POWER_S5) {
		config->type = POWER_S5;
		config->sel = 0;
		config->wksrc_en = wakeup_source;
	}

	arm_smccc_smc(SIP_PWR_SYS_SET, pcpu_param_pa, sizeof(struct pm_conf_data), 0, 0, 0, 0, 0, &res);

	dma_free_coherent(dev, sizeof(struct pm_conf_data), pcpu_param_va, pcpu_param_pa);
}

static int rtk_pm_check_wakeup_disabled(struct pm_private *dev_pm, enum rtk_pm_driver_id id)
{
	return !(id && (rtk_pm_get_wakeup_source(dev_pm) & BIT(id - 1)));
}

void rtk_pm_set_pcpu_param_v2(struct device *dev)
{
	struct pm_private *dev_pm = dev_get_drvdata(dev);
	struct pm_pcpu_param_v2 *pcpu_param;
	struct tc_wakeup_param *timer_param;
	struct irda_wakeup_key_v2 *key_tbl;
	struct gpio_wakeup_param *gpio;
	struct ipc_shm_irda_v2 *irda;
	struct rtc_wakeup_param *rtc;
	struct ipc_shm_cec_v2 *cec;
	struct pm_dev_param *node;
	struct arm_smccc_res res;
	dma_addr_t pcpu_param_pa;
	void *pcpu_param_va;
	int key_tbl_size;
	int i, idx, pos;

	list_for_each_entry(node, &rtk_pm_param_list, list) {

		if (rtk_pm_check_wakeup_disabled(dev_pm, node->dev_type))
			continue;

		switch (node->dev_type) {
		case IRDA:
			pcpu_param_va = dma_alloc_coherent(dev, sizeof(struct ipc_shm_irda_v2), &pcpu_param_pa, GFP_KERNEL);
			irda = (struct ipc_shm_irda_v2 *) pcpu_param_va;
			irda->header.version = IRDA_WK_VER;
			irda->header.type = IR_EVENT;
			irda->header.len = sizeof(struct ipc_shm_irda_v2);
			irda->ckey_nr = ((struct ipc_shm_irda_v2 *)node->data)->ckey_nr;

			key_tbl = ((struct ipc_shm_irda_v2 *)node->data)->key_tbl;
			key_tbl_size = sizeof(((struct ipc_shm_irda_v2 *)node->data)->key_tbl);

			memcpy(irda->key_tbl, key_tbl, key_tbl_size);

			arm_smccc_smc(SIP_PWR_SYS_WKS_SET, pcpu_param_pa, sizeof(struct ipc_shm_irda_v2), 0, 0, 0, 0, 0, &res);

			dma_free_coherent(dev, sizeof(struct ipc_shm_irda_v2), pcpu_param_va, pcpu_param_pa);
			break;
		case GPIO:
			pcpu_param = (struct pm_pcpu_param_v2 *) dev_pm->pcpu_param;
			pcpu_param_va = dma_alloc_coherent(dev, sizeof(struct gpio_wakeup_param), &pcpu_param_pa, GFP_KERNEL);
			gpio = (struct gpio_wakeup_param *) pcpu_param_va;
			gpio->hdr.version = GPIO_WK_VER;
			gpio->hdr.type = GPIO_EVENT;
			gpio->hdr.len = sizeof(struct gpio_wakeup_param);

			for (i = 0; i < 192; i++) {
				if (pcpu_param->wu_gpio_en[i]) {
					idx = i / 32;
					pos = i % 32;
					gpio->bitmap[idx] |= (1U << pos);
				}
			}

			arm_smccc_smc(SIP_PWR_SYS_WKS_SET, pcpu_param_pa, sizeof(struct gpio_wakeup_param), 0, 0, 0, 0, 0, &res);

			dma_free_coherent(dev, sizeof(struct gpio_wakeup_param), pcpu_param_va, pcpu_param_pa);
			break;
		case ALARM_TIMER:
			pcpu_param_va = dma_alloc_coherent(dev, sizeof(struct rtc_wakeup_param), &pcpu_param_pa, GFP_KERNEL);
			rtc = (struct rtc_wakeup_param *) pcpu_param_va;
			rtc->hdr.version = RTC_WK_VER;
			rtc->hdr.type = ALARM_EVENT;
			rtc->hdr.len = sizeof(struct rtc_wakeup_param);

			arm_smccc_smc(SIP_PWR_SYS_WKS_SET, pcpu_param_pa, sizeof(struct rtc_wakeup_param), 0, 0, 0, 0, 0, &res);

			dma_free_coherent(dev, sizeof(struct rtc_wakeup_param), pcpu_param_va, pcpu_param_pa);
			break;
		case TIMER:
			pcpu_param = (struct pm_pcpu_param_v2 *) dev_pm->pcpu_param;
			pcpu_param_va = dma_alloc_coherent(dev, sizeof(struct tc_wakeup_param), &pcpu_param_pa, GFP_KERNEL);
			timer_param = (struct tc_wakeup_param *) pcpu_param_va;
			timer_param->hdr.version = TC_WK_VER;
			timer_param->hdr.type = TIMER_EVENT;
			timer_param->hdr.len = sizeof(struct tc_wakeup_param);
			timer_param->timeout = pcpu_param->timerout_val;

			arm_smccc_smc(SIP_PWR_SYS_WKS_SET, pcpu_param_pa, sizeof(struct tc_wakeup_param), 0, 0, 0, 0, 0, &res);

			dma_free_coherent(dev, sizeof(struct tc_wakeup_param), pcpu_param_va, pcpu_param_pa);
			break;
		case CEC:
			pcpu_param_va = dma_alloc_coherent(dev, sizeof(struct ipc_shm_cec_v2), &pcpu_param_pa, GFP_KERNEL);
			cec = (struct ipc_shm_cec_v2 *) pcpu_param_va;

			memcpy(cec, node->data, sizeof(struct ipc_shm_cec_v2));

			cec->hdr.version = CEC_WK_VER;
			cec->hdr.type = CEC_EVENT;
			cec->hdr.len = sizeof(struct ipc_shm_cec_v2);

			arm_smccc_smc(SIP_PWR_SYS_WKS_SET, pcpu_param_pa, sizeof(struct ipc_shm_cec_v2), 0, 0, 0, 0, 0, &res);

			dma_free_coherent(dev, sizeof(struct ipc_shm_cec_v2), pcpu_param_va, pcpu_param_pa);
			break;
		default:
			break;
		}
	}
}
EXPORT_SYMBOL(rtk_pm_set_pcpu_param_v2);

int rtk_pm_get_wakeup_reason(void)
{
	struct pm_dev_param *pm_param = rtk_pm_get_param(PM);
	struct pm_private *dev_pm = dev_get_drvdata(pm_param->dev);

	return dev_pm->wakeup_reason;
}
EXPORT_SYMBOL(rtk_pm_get_wakeup_reason);

int rtk_pm_add_param_node(struct pm_private *dev_pm, enum rtk_pm_driver_id type)
{
	struct pm_dev_param *node = rtk_pm_get_param(type);

	if (!node) {
		node = devm_kzalloc(dev_pm->dev, sizeof(*node), GFP_KERNEL);
		if (!node)
			return -ENOMEM;

		node->dev = dev_pm->dev;
		node->dev_type = type;
		node->pm_version = dev_pm->version;
		if (type == PM)
			dev_pm->device_param = node;
		rtk_pm_add_list(node);
	}

	return 0;
}

int rtk_pm_remove_param_node(struct pm_private *dev_pm, enum rtk_pm_driver_id type)
{
	struct pm_dev_param *node = rtk_pm_get_param(type);

	if (node) {
		node = rtk_pm_get_param(type);
		if (node) {
			rtk_pm_del_list(node);
			devm_kfree(dev_pm->dev, node);
		}
	}

	return 0;
}

MODULE_LICENSE("GPL v2");
