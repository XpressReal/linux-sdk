// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realtek Dbus PMU driver for 1625
 *
 * Copyright (C) 2021-2024 Realtek Semiconductor Corporation
 * Copyright (C) 2021-2024 Ping-Hsiung Chiu <phelic@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt)	"[RTK_PMU] " fmt

#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "rtk_uncore_pmu.h"
#include "rtk_dbus_pmu.h"


#define DBUS_CTRL_OFFSET	0x0030
#define DBUS_SYSH_WR_LAT_CTRL	0x0c38
#define DBUS_SYS_WR_LAT_CTRL	0x0c3c


/* Target ID of 1625 */
/* SYSH domain */
#define VO1_REMAP_ID				0x1c	/* 7d'28 */
#define NPUPP_REMAP_ID				0x1e	/* 7d'30 */
#define SCPU_SW_REMAP_ID			0x7e	/* 7d'126 */
#define SCPU_NW_REMAP_ID			0x07	/* 7d'7 */
#define ACPU_REMAP_ID				0x08	/* 7d'8 */
#define VE1_REMAP_ID				0x0e	/* 7d'14 */
#define VE3_REMAP_ID				0x1f	/* 7d'31 */
#define PCIE1_REMAP_ID				0x21	/* 7d'33 */
#define AUCPU0_REMAP_ID				0x1d	/* 7d'29 */
#define PCIE0_REMAP_ID				0x10	/* 7d'16 */
#define MIPICSI_REMAP_ID			0x22	/* 7d'34 */
#define AUCPU1_REMAP_ID				0x26	/* 7d'38 */
#define VI_REMAP_ID				0x24	/* 7d'36 */
#define VO2_REMAP_ID				0x23	/* 7d'35 */
#define DIP_REMAP_ID				0x14	/* 7d'20 */
#define HSE_REE_REMAP_ID			0x15	/* 7d'21 */
#define HSE_TEE_REMAP_ID			0x7d	/* 7d'125 */

/* SYS domain */
#define AIO_REMAP_ID				0x01	/* 7d'1 */
#define HIF_REMAP_ID				0x02	/* 7d'2 */
#define JPEG_REMAP_ID				0x06	/* 7d'6 */
#define MIS_REMAP_ID				0x25	/* 7d'37 */
#define VTC_REMAP_ID				0x09	/* 7d'9 */
#define USB0_REMAP_ID				0x0b	/* 7d'11 */
#define SDIO_REMAP_ID				0x13	/* 7d'19 */
#define LSADC_REMAP_ID				0x20	/* 7d'32 */
#define MD_REMAP_ID				0x04	/* 7d'4 */
#define ETN_REMAP_ID				0x0c	/* 7d'12 */
#define EMMC_REMAP_ID				0x11	/* 7d'17 */
#define NF_REMAP_ID				0x12	/* 7d'18 */
#define SD_REMAP_ID				0x0d	/* 7d'13 */
#define CP_REMAP_ID				0x17	/* 7d'23 */
#define BGC_REMAP_ID				0x19	/* 7d'25 */
#define USB1_REMAP_ID				0x0a	/* 7d'10 */
#define TPB_REMAP_ID				0x1a	/* 7d'26 */
#define TP_REMAP_ID				0x1b	/* 7d'27 */


/* SYSH domain events */
DBUS_SYSH_EVENT_GROUP(vo1,	VO1_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(npupp,	NPUPP_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(scpu_sw,	SCPU_SW_REMAP_ID,	DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(scpu_nw,	SCPU_NW_REMAP_ID,	DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(acpu,	ACPU_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(ve1,	VE1_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(ve3,	VE3_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(pcie1,	PCIE1_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(aucpu0,	AUCPU0_REMAP_ID,	DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(pcie0,	PCIE0_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(mipicsi,	MIPICSI_REMAP_ID,	DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(aucpu1,	AUCPU1_REMAP_ID,	DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(vi,	VI_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(vo2,	VO2_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(dip,	DIP_REMAP_ID,		DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(hse_ree,	HSE_REE_REMAP_ID,	DBUS_CH_AUTO);
DBUS_SYSH_EVENT_GROUP(hse_tee,	HSE_TEE_REMAP_ID,	DBUS_CH_AUTO);


/* SYS domain events */
DBUS_SYS_EVENT_GROUP_V2(aio,	AIO_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(hif,	HIF_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(jpeg,	JPEG_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(mis,	MIS_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(vtc,	VTC_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(usb0,	USB0_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(sdio,	SDIO_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(lsadc,	LSADC_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(md,	MD_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(etn,	ETN_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(emmc,	EMMC_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(nf,	NF_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(sd,	SD_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(cp,	CP_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(bgc,	BGC_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(usb1,	USB1_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(tpb,	TPB_REMAP_ID);
DBUS_SYS_EVENT_GROUP_V2(tp,	TP_REMAP_ID);

/* Channel total events */
DBUS_TOTAL_EVENT_ATTR_GROUP(dbus_ch0,	0);
DBUS_TOTAL_EVENT_ATTR_GROUP(dbus_ch1,	1);

/* Driver statistics events */
DBUS_DRV_EVENT_ATTR(dbus_refresh,	DBUS_REFRESH);
DBUS_DRV_EVENT_ATTR(dbus_overflow,	RTK_DRV_OVERFLOW);


static struct attribute *rtk_1625_event_attrs[] = {
	DBUS_DRV_EVENT_REF(dbus_refresh),
	DBUS_DRV_EVENT_REF(dbus_overflow),

	DBUS_TOTAL_EVENT_REF_GROUP(dbus_ch0),
	DBUS_TOTAL_EVENT_REF_GROUP(dbus_ch1),

	DBUS_SYSH_EVENT_REF_GROUP(vo1),
	DBUS_SYSH_EVENT_REF_GROUP(npupp),
	DBUS_SYSH_EVENT_REF_GROUP(scpu_sw),
	DBUS_SYSH_EVENT_REF_GROUP(scpu_nw),
	DBUS_SYSH_EVENT_REF_GROUP(acpu),
	DBUS_SYSH_EVENT_REF_GROUP(ve1),
	DBUS_SYSH_EVENT_REF_GROUP(ve3),
	DBUS_SYSH_EVENT_REF_GROUP(pcie1),
	DBUS_SYSH_EVENT_REF_GROUP(aucpu0),
	DBUS_SYSH_EVENT_REF_GROUP(pcie0),
	DBUS_SYSH_EVENT_REF_GROUP(mipicsi),
	DBUS_SYSH_EVENT_REF_GROUP(aucpu1),
	DBUS_SYSH_EVENT_REF_GROUP(vi),
	DBUS_SYSH_EVENT_REF_GROUP(vo2),
	DBUS_SYSH_EVENT_REF_GROUP(dip),
	DBUS_SYSH_EVENT_REF_GROUP(hse_ree),
	DBUS_SYSH_EVENT_REF_GROUP(hse_tee),

	DBUS_SYS_EVENT_REF_GROUP_V2(aio),
	DBUS_SYS_EVENT_REF_GROUP_V2(hif),
	DBUS_SYS_EVENT_REF_GROUP_V2(jpeg),
	DBUS_SYS_EVENT_REF_GROUP_V2(mis),
	DBUS_SYS_EVENT_REF_GROUP_V2(vtc),
	DBUS_SYS_EVENT_REF_GROUP_V2(usb0),
	DBUS_SYS_EVENT_REF_GROUP_V2(sdio),
	DBUS_SYS_EVENT_REF_GROUP_V2(lsadc),
	DBUS_SYS_EVENT_REF_GROUP_V2(md),
	DBUS_SYS_EVENT_REF_GROUP_V2(etn),
	DBUS_SYS_EVENT_REF_GROUP_V2(emmc),
	DBUS_SYS_EVENT_REF_GROUP_V2(nf),
	DBUS_SYS_EVENT_REF_GROUP_V2(sd),
	DBUS_SYS_EVENT_REF_GROUP_V2(cp),
	DBUS_SYS_EVENT_REF_GROUP_V2(bgc),
	DBUS_SYS_EVENT_REF_GROUP_V2(usb1),
	DBUS_SYS_EVENT_REF_GROUP_V2(tpb),
	DBUS_SYS_EVENT_REF_GROUP_V2(tp),
	NULL
};

static struct attribute_group rtk_1625_event_attr_group = {
	.name = "events",
	.attrs = rtk_1625_event_attrs,
};

static const struct attribute_group *rtk_1625_dbus_attr_groups[] = {
	/* must be Null-terminated */
	[RTK_PMU_ATTR_GROUP__COMMON] = &rtk_pmu_common_attr_group,
	[RTK_PMU_ATTR_GROUP__FORMAT] = &rtk_dbus_format_attr_group,
	[RTK_PMU_ATTR_GROUP__EVENT] = &rtk_1625_event_attr_group,
	[RTK_PMU_ATTR_GROUP__NUM] = NULL
};

/* channel control bit */
enum {
	ACPU_CH_SEL	= 0,
	VO_CH_SEL	= 1,
	DIP_CH_SEL	= 2,
	VE1_CH_SEL	= 3,
	VE3_CH_SEL	= 4,
	PCIE_CH_SEL	= 5,
	HSE_CH_SEL	= 6,
	AUCPU_CH_SEL	= 7,
	C9_CH_SEL	= 8,
	SCPU_CH_SEL	= 9,
	GPU_CH_SEL	= 10,
	C10_CH_SEL	= 11,
	C0_CH_SEL	= 12,
	C1_CH_SEL	= 13,
	C2_CH_SEL	= 14,
};

static u8
__get_client_ch(struct rtk_pmc_set *ps, unsigned int target)
{
	unsigned long ch_reg;
	u8 ch = 0;

	if (ps->has_ext)
		/*
		 * using static channel information instead read from control
		 * register.
		 */
		ch_reg = (unsigned long)ps->ext_info;
	else
		ch_reg = rtk_readl((unsigned long)ps->base + DBUS_CH_SEL_CTRL0);

	switch (target) {
	case VO1_REMAP_ID:
		ch = test_bit(VO_CH_SEL, &ch_reg);
		break;
	case VO2_REMAP_ID:
		ch = test_bit(C10_CH_SEL, &ch_reg);
		break;
	case SCPU_NW_REMAP_ID:
	case SCPU_SW_REMAP_ID:
		ch = test_bit(SCPU_CH_SEL, &ch_reg);
		break;
	case VE1_REMAP_ID:
		ch = test_bit(VE1_CH_SEL, &ch_reg);
		break;
	case VE3_REMAP_ID:
		ch = test_bit(VE3_CH_SEL, &ch_reg);
		break;
	case PCIE0_REMAP_ID:
		ch = test_bit(C0_CH_SEL, &ch_reg);
		break;
	case PCIE1_REMAP_ID:
		ch = test_bit(PCIE_CH_SEL, &ch_reg);
		break;
	case DIP_REMAP_ID:
		ch = test_bit(DIP_CH_SEL, &ch_reg);
		break;
	case HSE_REE_REMAP_ID:
	case HSE_TEE_REMAP_ID:
		ch = test_bit(HSE_CH_SEL, &ch_reg);
		break;
	case ACPU_REMAP_ID:
		ch = test_bit(ACPU_CH_SEL, &ch_reg);
		break;
	case AUCPU0_REMAP_ID:
		ch = test_bit(AUCPU_CH_SEL, &ch_reg);
		break;
	case AUCPU1_REMAP_ID:
		ch = test_bit(C2_CH_SEL, &ch_reg);
		break;
	case NPUPP_REMAP_ID:
		ch = test_bit(GPU_CH_SEL, &ch_reg);
		break;
	case MIPICSI_REMAP_ID:
		ch = test_bit(C1_CH_SEL, &ch_reg);
		break;
	case VI_REMAP_ID:
		ch = test_bit(C9_CH_SEL, &ch_reg);
		break;
	default:
		break;
	}

	WARN(ch >= DBUS_CH_AUTO, "target:%#x locates at channel %d\n",
	     target, ch);

	return ch;
}

static union rtk_pmc_desc
rtk_1625_arrange_pmc(struct rtk_pmu *pmu, u64 config)
{
	union rtk_pmc_desc pmc;
	union rtk_dbus_event_desc desc = __get_event_desc(config);
	struct rtk_pmc_set *ps = pmu->pmcss[desc.set];
	int idx = ps->arrange_pmc(ps, rtk_dbus_pmc_config(config),
				  rtk_dbus_pmc_target(config));

	if (idx < 0) {
		pmc.val = -EAGAIN;
	} else {
		pmc.set = desc.set;
		pmc.idx = idx;
		pmc.usage = desc.usage;

		if (desc.set == PMC_SET__DBUS_SYSH && desc.ch >= DBUS_CH_AUTO)
			pmc.ch = __get_client_ch(ps, desc.target);
		else
			pmc.ch = desc.ch;
	}

	dmsg("%s- event desc %#x:%#x:%#x\n", pmu->name, desc.set, desc.target,
	     desc.usage);
	dmsg("%s- event pmc %#x:%#x:%#x\n", pmu->name, pmc.set, pmc.idx,
	     pmc.usage);
	return pmc;
}

static const unsigned int rtk_1625_dbus_sysh_clients[] = {
	VO1_REMAP_ID, NPUPP_REMAP_ID, SCPU_SW_REMAP_ID, SCPU_NW_REMAP_ID,
	ACPU_REMAP_ID, VE1_REMAP_ID, VE3_REMAP_ID, PCIE1_REMAP_ID,
	AUCPU0_REMAP_ID, PCIE0_REMAP_ID, MIPICSI_REMAP_ID,
	AUCPU1_REMAP_ID, VI_REMAP_ID, VO2_REMAP_ID, DIP_REMAP_ID,
	HSE_REE_REMAP_ID, HSE_TEE_REMAP_ID,
};

/* address offset of Dbus sysh domain PMCG */
static const unsigned long rtk_1625_dbus_sysh_pmcgs[] = {
	0x0060, 0x0070, 0x0080, 0x0510, 0x0520, 0x530, 0x540, 0x550
};

/* address offset of configs of Dbus sysh domain PMCG */
static const unsigned long rtk_1625_dbus_sysh_configs[] = {
	0x0050, 0x0500, 0x504
};

static const unsigned int rtk_1625_dbus_sys_clients[] = {
	AIO_REMAP_ID, HIF_REMAP_ID, JPEG_REMAP_ID, MIS_REMAP_ID,
	VTC_REMAP_ID, USB0_REMAP_ID, SDIO_REMAP_ID, LSADC_REMAP_ID,
	MD_REMAP_ID, ETN_REMAP_ID, EMMC_REMAP_ID, NF_REMAP_ID,
	SD_REMAP_ID, CP_REMAP_ID, BGC_REMAP_ID, USB1_REMAP_ID,
	TPB_REMAP_ID, TP_REMAP_ID,
};

/* address offset of Dbus sys domain(bridge) PMCG */
static const unsigned long rtk_1625_dbus_sys_pmcgs[] = {
	0x0090, 0x00a0, 0x00b0
};

/* address offset of configs of Dbus sys domain(bridge) PMCG */
static const unsigned long rtk_1625_dbus_sys_configs[] = {
	0x0054
};

static const unsigned int rtk_1625_dbus_ch_clients[] = {
	DBUS_CH_0, DBUS_CH_1
};

/* address offset of Dbus output channel statistics PMCG */
static const unsigned long rtk_1625_dbus_ch_pmcgs[] = {
	0x0034, 0x0040
};

static const unsigned int rtk_1625_dbus_drv_clients[] = {
	RTK_DRV_OVERFLOW, DBUS_REFRESH
};

static const struct rtk_pmc_set_meta rtk_1625_ps_meta[] = {
	{
		.name = "1625 Dbus SYS",
		.compatible = "dbus-sys",
		.hw_ver = RTK_DBUS_BIT_ORDER_V2,
		.type = PMC_SET__DBUS_SYS,
		.init = rtk_dbus_ps_init,
		.group_size = DBUS_PMC__USAGE_NUM,
		.config_size = 3,
		.config_width = 8,
		.val_mask = (const unsigned int []){
			RTK_PMU_VAL_MASK(32),
			RTK_PMU_VAL_MASK(32),
			RTK_PMU_VAL_MASK(32),
			RTK_PMU_VAL_MASK(32),
		},
		.ov_th = (const unsigned int []){
			RTK_PMU_OVERFLOW_TH(32),
			RTK_PMU_OVERFLOW_TH(32),
			RTK_PMU_OVERFLOW_TH(32),
			RTK_PMU_OVERFLOW_TH(32),
		},

		.nr_clients = ARRAY_SIZE(rtk_1625_dbus_sys_clients),
		.nr_pmcgs = ARRAY_SIZE(rtk_1625_dbus_sys_pmcgs),
		.nr_configs = ARRAY_SIZE(rtk_1625_dbus_sys_configs),
		.clients = rtk_1625_dbus_sys_clients,
		.pmcgs = rtk_1625_dbus_sys_pmcgs,
		.configs = rtk_1625_dbus_sys_configs,
	},
	{
		.name = "1625 Dbus SYSH",
		.compatible = "dbus-sysh",
		.hw_ver = RTK_DBUS_BIT_ORDER_V2,
		.type = PMC_SET__DBUS_SYSH,
		.init = rtk_dbus_ps_init,
		.group_size = DBUS_PMC__USAGE_NUM,
		.config_size = 3,
		.config_width = 8,
		.val_mask = (const unsigned int []){
			RTK_PMU_VAL_MASK(32),
			RTK_PMU_VAL_MASK(32),
			RTK_PMU_VAL_MASK(32),
			RTK_PMU_VAL_MASK(32),
		},
		.ov_th = (const unsigned int []){
			RTK_PMU_OVERFLOW_TH(32),
			RTK_PMU_OVERFLOW_TH(32),
			RTK_PMU_OVERFLOW_TH(32),
			RTK_PMU_OVERFLOW_TH(32),
		},

		.nr_clients = ARRAY_SIZE(rtk_1625_dbus_sysh_clients),
		.nr_pmcgs = ARRAY_SIZE(rtk_1625_dbus_sysh_pmcgs),
		.nr_configs = ARRAY_SIZE(rtk_1625_dbus_sysh_configs),
		.clients = rtk_1625_dbus_sysh_clients,
		.pmcgs = rtk_1625_dbus_sysh_pmcgs,
		.configs = rtk_1625_dbus_sysh_configs,
	},
	{
		.name = "1625 Dbus CH",
		.compatible = "dbus-ch",
		.hw_ver = RTK_DBUS_BIT_ORDER_V2,
		.type = PMC_SET__DBUS_CH,
		.init = rtk_dbus_ch_init,
		.group_size = 3,
		.val_mask = (const unsigned int []){
			RTK_PMU_VAL_MASK(32),
			RTK_PMU_VAL_MASK(32),
			RTK_PMU_VAL_MASK(32),
		},
		.ov_th = (const unsigned int []){
			RTK_PMU_OVERFLOW_TH(32),
			RTK_PMU_OVERFLOW_TH(32),
			RTK_PMU_OVERFLOW_TH(32),
		},

		.nr_clients = ARRAY_SIZE(rtk_1625_dbus_ch_clients),
		.nr_pmcgs = ARRAY_SIZE(rtk_1625_dbus_ch_pmcgs),
		.nr_configs = 0,
		.clients = rtk_1625_dbus_ch_clients,
		.pmcgs = rtk_1625_dbus_ch_pmcgs,
		.configs = NULL,
	},
	{
		.name = "1625 Dbus DRV",
		.type = PMC_SET__DBUS_DRV,
		.init = rtk_dbus_drv_init,
		.group_size = 1,

		.nr_clients = ARRAY_SIZE(rtk_1625_dbus_drv_clients),
		.nr_pmcgs = DBUS_EV_NUM,
		.nr_configs = 0,
		.clients = rtk_1625_dbus_drv_clients,
		.pmcgs = NULL,
		.configs = NULL,
	},
	{},
};

int rtk_1625_dbus_init(struct rtk_pmu *pmu, struct device_node *dt)
{
	int ret;
	unsigned long ctrl;
	u32 val;

	ret = rtk_dbus_pmu_init(pmu, dt,
				"rtk_1625_dbus_pmu",
				DBUS_CTRL_OFFSET,
				rtk_1625_dbus_attr_groups,
				rtk_1625_ps_meta,
				RTK_PMU_META_NR(rtk_1625_ps_meta));

	if (!ret) {
		pmu->arrange_pmc = rtk_1625_arrange_pmc;

		/* make write latency counts the entire write operation */
		ctrl = (unsigned long)pmu->base + DBUS_SYSH_WR_LAT_CTRL;
		val = BIT(0) | BIT(4) | BIT(8) | BIT(12) |
			BIT(16) | BIT(20) | BIT(24) | BIT(28);
		rtk_writel(ctrl, val);

		ctrl = (unsigned long)pmu->base + DBUS_SYS_WR_LAT_CTRL;
		val = BIT(0) | BIT(4) | BIT(8);
		rtk_writel(ctrl, val);
	}

	return ret;
}
EXPORT_SYMBOL(rtk_1625_dbus_init);
MODULE_LICENSE("GPL");
