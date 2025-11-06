/* SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause) */
/*
 * Realtek DHC SoC family power management driver
 *
 * Copyright (c) 2020 Realtek Semiconductor Corp.
 */

#ifndef _RTK_PM_H_
#define _RTK_PM_H_

#include <linux/suspend.h>
#include <soc/realtek/rtk_ipc_shm.h>
#include <soc/realtek/uapi/rtk_pm_pcpu.h>

#define DISABLE_IRQMUX       665
#define ENABLE_IRQMUX        786

#define SUSPEND_FLAG         0x325
#define RTK_RESUME_PARAM_V1  0x546
#define RTK_RESUME_PARAM_V2  0x675
#define RTK_RESUME_PARAM_V3  0x953

#define RTK_PCPU_VERSION_V1  0x56574
#define RTK_PCPU_VERSION_V2  0x78524

#define SIP_PWR_SYS_VER      0x82000700
#define SIP_PWR_SYS_LIST     0x82000701
#define SIP_PWR_SYS_SET      0x82000702
#define SIP_PWR_SYS_WKS_SET  0x82000703
#define SIP_PWR_SYS_WKS_INFO 0x82000704

#define GET_GPIO_EN          0x67526
#define GET_GPIO_ACT         0x32163

#define POWER_S3             0x3
#define POWER_S5             0x5

#define S3_NORMAL            0x187f
#define S3_DCO               0x183e
#define S3_APF               0x18ff
#define S3_VTC               0x18ff
#define S5_NORMAL            0x187f

#define MEM_VERIFIED_CNT    100

struct clk;

enum rtk_pm_driver_id {
	PM = 0,
	LAN,
	IRDA,
	GPIO,
	ALARM_TIMER,
	TIMER,
	CEC,
	USB,
};

enum resume_state_v1 {
	RESUME_V1_NONE = 0,
	RESUME_V1_UNKNOW,
	RESUME_V1_IR,
	RESUME_V1_GPIO,
	RESUME_V1_LAN,
	RESUME_V1_ALARM,
	RESUME_V1_TIMER,
	RESUME_V1_CEC,
	RESUME_V1_MAX_STATE,
};

enum resume_state_v2 {
	RESUME_V2_LAN = 0,
	RESUME_V2_IR,
	RESUME_V2_GPIO,
	RESUME_V2_ALARM,
	RESUME_V2_TIMER,
	RESUME_V2_CEC,
	RESUME_V2_USB,
	RESUME_V2_HIFI,
	RESUME_V2_VTC,
	RESUME_V2_PON,
	RESUME_V2_APF,
	RESUME_V2_PCTRL,
	RESUME_V2_TS,
	RESUME_V2_MAX_STATE,
};

struct pm_conf_data {
	uint32_t type;
	uint32_t sel;
	uint32_t wksrc_en;
};

struct rtk_pm_param {
	const char *const *reasons;
	unsigned int reasons_version;
	unsigned int version;
	void *pcpu_param;
};

struct pm_dev_param {
	struct list_head list;
	unsigned int dev_type;
	unsigned int pm_version;
	struct device *dev;
	void *data;
};

struct pm_private {
	struct pm_dev_param *device_param;
	void *pcpu_param;
	unsigned int version;
	unsigned int reasons_version;
	unsigned int suspend_context;
	unsigned int reboot_reasons;
	const char *const *reasons;
	unsigned int wakeup_reason;
	struct regmap *syscon_iso;
	dma_addr_t pcpu_param_pa;
	unsigned int pm_dbg;
	struct device *dev;
	struct clk *dco;
	unsigned int dco_mode;
};

struct mem_check {
	unsigned char *mem_addr;
	size_t mem_byte;
};

int rtk_pm_create_sysfs(void);
int rtk_pm_sysfs_sync(struct pm_private *dev_pm);
struct device *rtk_pm_get_dev(void);
struct pm_dev_param *rtk_pm_get_param(unsigned int id);
unsigned int rtk_pm_get_param_mask(void);
void rtk_pm_add_list(struct pm_dev_param *pm_node);
void rtk_pm_del_list(struct pm_dev_param *pm_node);
void rtk_pm_init_list(void);

int rtk_pm_get_bt(struct pm_private *dev_pm);
int rtk_pm_get_gpio_num(void);
size_t rtk_pm_get_gpio_size(struct pm_private *dev_pm, int type);
char *rtk_pm_get_gpio(struct pm_private *dev_pm, int type);
void rtk_pm_set_gpio(struct pm_private *dev_pm, int type, char *val);
int rtk_pm_get_timeout(struct pm_private *dev_pm);
int rtk_pm_get_wakeup_reason(void);
int rtk_pm_get_wakeup_source(struct pm_private *dev_pm);
void rtk_pm_set_bt(struct pm_private *dev_pm, unsigned int val);
void rtk_pm_set_pcpu_param_v2(struct device *dev);
void rtk_pm_set_pcpu_param_v1(struct device *dev);
void rtk_pm_set_timeout(struct pm_private *dev_pm, unsigned int val);
void rtk_pm_set_wakeup_source(struct pm_private *dev_pm, unsigned int val);
void rtk_pm_notfiy_pcpu_mode(struct device *dev, int mode);
int rtk_pm_add_param_node(struct pm_private *dev_pm,
			  enum rtk_pm_driver_id type);
int rtk_pm_remove_param_node(struct pm_private *dev_pm,
			     enum rtk_pm_driver_id type);

extern struct rtk_pm_param rtk_pm_param_v1;
extern struct rtk_pm_param rtk_pm_param_v2;
extern struct rtk_pm_param rtk_pm_param_v3;

/**
 * rtk_pm_wakeup_source_alarm_set - set wakeup_source alarm
 * @pm_dev: pm device
 * @enable: 1 to enable, 0 to disable
 */
static inline void rtk_pm_wakeup_source_alarm_set(struct pm_private *pm_dev, int enable)
{
	unsigned int wakeup_source = rtk_pm_get_wakeup_source(pm_dev);

	if (enable)
		wakeup_source |= BIT(ALARM_EVENT);
	else
		wakeup_source &= ~BIT(ALARM_EVENT);

	rtk_pm_set_wakeup_source(pm_dev, wakeup_source);
};

/**
 * rtk_pm_wakeup_source_hifi_enabled - check if wakeup_source hifi is enabled
 * @pm_dev: pm device
 *
 * return 1 if wakeup_source hifi is enabled
 */
static inline int rtk_pm_wakeup_source_hifi_enabled(struct pm_private *pm_dev)
{
	if (rtk_pm_get_wakeup_source(pm_dev) & BIT(HIFI_EVENT))
		return 1;
	return 0;
}

/**
 * rtk_pm_ignore_pd_pin - don't set pd pin in pm mode
 * @pm_dev: pm device
 */
static inline void rtk_pm_ignore_pd_pin(struct pm_private *pm_dev)
{
	unsigned int wakeup_source = rtk_pm_get_wakeup_source(pm_dev);

	wakeup_source |= PCPU_FLAGS_IGNORE_PD_PIN;
	rtk_pm_set_wakeup_source(pm_dev, wakeup_source);
}

#endif
