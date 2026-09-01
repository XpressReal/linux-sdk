/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Header of Realtek register bus PMU
 *
 * Copyright (C) 2020-2023 Realtek Semiconductor Corporation
 * Copyright (C) 2020-2023 Ping-Hsiung Chiu <phelic@realtek.com>
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

#ifndef __RTK_RBUS_PMU__
#define __RTK_RBUS_PMU__

#include <linux/perf_event.h>

#include "rtk_uncore_pmu.h"


#define	RTK_RBUS_PMU_PDEV_NAME	"rtk-rbus-pmu"
#define RTK_RBUS_PMU_CPUHP_NAME	RTK_PMU_CPUHP_NAME("rtk-rbus")

/*
 * Macros with complex values fail on kernel checking script.
 * This macro is a workaround, wrapping the value as a macro function and
 * expand.
 */
#define _ESCAPE_COMPLEX(...)	__VA_ARGS__

#define RBUS_EVENT_ATTR_GROUP_V1(_n, _s, _t)			\
_ESCAPE_COMPLEX(						\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_acc_lat, \
				RBUS_EV_CONFIG(_t, _s, ACC_LAT)),	\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_max_lat, \
				RBUS_EV_CONFIG(_t, _s, MAX_LAT)),	\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_req_num, \
				RBUS_EV_CONFIG(_t, _s, REQ_NUM))	\
)

#define RBUS_EVENT_REF_GROUP_V1(_n)			\
_ESCAPE_COMPLEX(					\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__ACC_LAT),	\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__MAX_LAT),	\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__REQ_NUM)	\
)

#define RBUS_EVENT_GROUP_V1(_n, _t, _s)					\
static struct perf_pmu_events_attr attr_list_##_n[] = {			\
	RBUS_EVENT_ATTR_GROUP_V1(_n, _s, _t)			\
}

#define __RBUS_EVENT_ATTR_GROUP_V(_n, _s, _t, _v)				\
_ESCAPE_COMPLEX(							\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_req_num, \
				RBUS_EV_CONFIG(_t, _s, V##_v##_REQ)),	\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_rtg_total, \
				RBUS_EV_CONFIG(_t, _s, V##_v##_RTG_TOTAL)),	\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_rta_max, \
				RBUS_EV_CONFIG(_t, _s, V##_v##_RTA_MAX)),	\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_rta_total, \
				RBUS_EV_CONFIG(_t, _s, V##_v##_RTA_TOTAL))	\
)
#define RBUS_EVENT_ATTR_GROUP_V2(_n, _s, _t)	\
	__RBUS_EVENT_ATTR_GROUP_V(_n, _s, _t, 2)
#define RBUS_EVENT_ATTR_GROUP_V3(_n, _s, _t)	\
	__RBUS_EVENT_ATTR_GROUP_V(_n, _s, _t, 3)

#define __RBUS_EVENT_REF_GROUP_V(_n, _v)			\
_ESCAPE_COMPLEX(					\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_REQ),	\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_RTG_TOTAL),	\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_RTA_MAX),	\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_RTA_TOTAL)	\
)
#define RBUS_EVENT_REF_GROUP_V2(_n)		__RBUS_EVENT_REF_GROUP_V(_n, 2)
#define RBUS_EVENT_REF_GROUP_V3(_n)		__RBUS_EVENT_REF_GROUP_V(_n, 3)

#define __RBUS_PRE_ARB_EVENT_REF_GROUP_V(_n, _v)			\
_ESCAPE_COMPLEX(					\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_REQ),	\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_RTG),	\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_RTA)	\
)
#define __RBUS_POST_ARB_EVENT_REF_GROUP_V(_n, _v)			\
_ESCAPE_COMPLEX(					\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_PSEL),	\
	RTK_PMU_EVENT_ITEM_REF(attr_list_##_n, RBUS_PMC__V##_v##_PTA)	\
)
#define RBUS_PRE_ARB_EVENT_REF_GROUP_V4(_n)	__RBUS_PRE_ARB_EVENT_REF_GROUP_V(_n, 4)
#define RBUS_POST_ARB_EVENT_REF_GROUP_V4(_n)	__RBUS_POST_ARB_EVENT_REF_GROUP_V(_n, 4)

#define __RBUS_EVENT_GROUP_V(_n, _t, _s, _v)				\
static struct perf_pmu_events_attr attr_list_##_n[] = {			\
	RBUS_EVENT_ATTR_GROUP_V##_v(_n, _s, _t)				\
}
#define RBUS_EVENT_GROUP_V2(_n, _t, _s)		\
	__RBUS_EVENT_GROUP_V(_n, _t, _s, 2)
#define RBUS_EVENT_GROUP_V3(_n, _t, _s)		\
	__RBUS_EVENT_GROUP_V(_n, _t, _s, 3)

#define RBUS_PRE_ARB_EVENT_ATTR_GROUP_V(_n, _t, _s, _v)			\
_ESCAPE_COMPLEX(							\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_req,	\
				RBUS_EV_CONFIG_V2(_t, _s, V##_v##_REQ)),	\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_rtg,	\
				RBUS_EV_CONFIG_V2(_t, _s, V##_v##_RTG)),	\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_rta,	\
				RBUS_EV_CONFIG_V2(_t, _s, V##_v##_RTA))		\
)
#define RBUS_POST_ARB_EVENT_ATTR_GROUP_V(_n, _t, _s, _v)			\
_ESCAPE_COMPLEX(						\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_psel,	\
				RBUS_EV_CONFIG_V2(_t, _s, V##_v##_PSEL)),	\
	RTK_PMU_EVENT_ATTR_ITEM(_n##_pta,	\
				RBUS_EV_CONFIG_V2(_t, _s, V##_v##_PTA)),	\
)

#define __RBUS_PRE_ARB_EVENT_GROUP_V(_n, _t, _s, _v)				\
static struct perf_pmu_events_attr attr_list_##_n[] = {				\
	RBUS_PRE_ARB_EVENT_ATTR_GROUP_V(_n, _t, _s, _v)				\
}

#define __RBUS_POST_ARB_EVENT_GROUP_V(_n, _t, _s, _v)				\
static struct perf_pmu_events_attr attr_list_##_n[] = {				\
	RBUS_POST_ARB_EVENT_ATTR_GROUP_V(_n, _t, _s, _v)			\
}

#define RBUS_PRE_ARB_EVENT_GROUP_V4(_n, _t, _s)					\
	__RBUS_PRE_ARB_EVENT_GROUP_V(_n, _t, _s, 4)
#define RBUS_POST_ARB_EVENT_GROUP_V4(_n, _t, _s)				\
	__RBUS_POST_ARB_EVENT_GROUP_V(_n, _t, _s, 4)

#define RBUS_DRV_EVENT_ATTR(_n, _c)		\
	RTK_PMU_EVENT_ATTR(_n, RBUS_EV_CONFIG(_c, RBUS_DRV, NONE))

#define RBUS_ARB_DRV_EVENT_ATTR(_n, _c)		\
	RTK_PMU_EVENT_ATTR(_n, RBUS_EV_CONFIG(_c, RBUS_ARB_DRV, NONE))

#define RBUS_DRV_EVENT_REF(_n)		\
	RTK_PMU_EVENT_REF(_n)


/* PMC set of Rbus */
enum {
	PMC_SET__RBUS		= 0,
	PMC_SET__RBUS_DRV,
};

/* PMC ARB set of Rbus */
enum {
	PMC_SET__RBUS_PRE_ARB		= 0,
	PMC_SET__RBUS_POST_ARB,
	PMC_SET__RBUS_ARB_DRV
};

/* Members of a Rbus PMC group */
enum rbus_ev_enum_v1 {
	RBUS_PMC__ACC_LAT = 0,          /* accumulate latency */
	RBUS_PMC__MAX_LAT,              /* max latency */
	RBUS_PMC__REQ_NUM,              /* # of requests */
	RBUS_PMC__V1_USAGE_NUM
};

/* Members of a Rbus PMC group */
enum rbus_ev_enum_v2 {
	RBUS_PMC__V2_REQ,		/* # of requests */
	RBUS_PMC__V2_RTG_TOTAL,		/* accumulate grant latency */
	RBUS_PMC__V2_RTA_MAX,		/* max latency */
	RBUS_PMC__V2_RTA_TOTAL,		/* accumulate latency */
	RBUS_PMC__V2_USAGE_NUM
};

/* Members of a Rbus PMC group */
enum rbus_ev_enum_v3 {
	RBUS_PMC__V3_REQ,		/* # of requests */
	RBUS_PMC__V3_RTG_TOTAL,		/* accumulate grant latency */
	RBUS_PMC__V3_RTA_TOTAL,		/* accumulate latency */
	RBUS_PMC__V3_RTA_MAX,		/* max latency */
	RBUS_PMC__V3_USAGE_NUM
};

/* Members of a Rbus Pre Arb PMC group */
enum rbus_ev_enum_v4_pre_arb {
	RBUS_PMC__V4_REQ,		/* # of requests */
	RBUS_PMC__V4_RTG,		/* accumulate grant latency */
	RBUS_PMC__V4_RTA,		/* accumulate latency */
	RBUS_PMC__V4_PRE_ARB_USAGE_NUM
};

/* Members of a Rbus post arb PMC group */
enum rbus_ev_enum_v4_post_arb {
	RBUS_PMC__V4_PSEL,		/* # of psels */
	RBUS_PMC__V4_PTA,		/* accumulate psel latency */
	RBUS_PMC__V4_POST_ARB_USAGE_NUM
};


enum {
	RTK_RBUS_EV_ORDER_V1 = 0,
	RTK_RBUS_EV_ORDER_V2 = 1,
	RTK_RBUS_EV_ORDER_V3 = 2,
	RTK_RBUS_EV_ORDER_V4 = 3,
};

/* Driver event does not have usage field */
#define RBUS_PMC__NONE			0

/* Driver events */
enum {
	RBUS_REFRESH	= RTK_DRV_EV_NUM,
	RBUS_EV_NUM
};

#define RBUS_OVERFLOW_STATUS_0		0x0074
#define RBUS_OVERFLOW_ENABLE_0		0x0078
#define RBUS_OVERFLOW_STATUS_1		0x0090
#define RBUS_OVERFLOW_ENABLE_1		0x0094
#define RBUS_OVERFLOW_STATUS_WIDTH	28
#define RBUS_OVERFLOW_PMCG_WIDTH	4
#define RBUS_OVERFLOW_OFFSET		4

#define RBUS_PRE_ARB_OVERFLOW_STATUS_0		0x0010
#define RBUS_PRE_ARB_OVERFLOW_ENABLE_0		0x0014
#define RBUS_PRE_ARB_OVERFLOW_STATUS_1		0x0018
#define RBUS_PRE_ARB_OVERFLOW_ENABLE_1		0x001C
#define RBUS_POST_ARB_OVERFLOW_STATUS_0		0x0020
#define RBUS_POST_ARB_OVERFLOW_ENABLE_0		0x0024
#define RBUS_ARB_OVERFLOW_STATUS_WIDTH		24
#define RBUS_ARB_OVERFLOW_PMCG_WIDTH		3

/* Latch counter value to shadow register */
#define RBUS_ARB_UPDATE_CTRL			0x0008

/* Offset of post arb start/end address*/
#define RBUS_POST_ARB_START_ADDR_CONFIG_OFFSET		0x40
#define RBUS_POST_ARB_END_ADDR_CONFIG_OFFSET		0x80

static inline unsigned long
__get_ov_status_reg(struct rtk_pmc_set *ps, unsigned int client_idx)
{
	return (unsigned long)ps->base + (client_idx <
		(RBUS_OVERFLOW_STATUS_WIDTH / RBUS_OVERFLOW_PMCG_WIDTH) ?
		(RBUS_OVERFLOW_STATUS_0) : (RBUS_OVERFLOW_STATUS_1));
}

static inline unsigned long
__get_ov_en_reg(struct rtk_pmc_set *ps, unsigned int client_idx)
{
	return (unsigned long)ps->base + (client_idx <
		(RBUS_OVERFLOW_STATUS_WIDTH / RBUS_OVERFLOW_PMCG_WIDTH) ?
		(RBUS_OVERFLOW_ENABLE_0) : (RBUS_OVERFLOW_ENABLE_1));
}

static inline unsigned long
__get_ov_en_reg_v2(struct rtk_pmc_set *ps, unsigned int client_idx)
{
	if (ps->type == PMC_SET__RBUS_PRE_ARB) {
		return (unsigned long)ps->base + (client_idx <
			(RBUS_ARB_OVERFLOW_STATUS_WIDTH / RBUS_ARB_OVERFLOW_PMCG_WIDTH) ?
			(RBUS_PRE_ARB_OVERFLOW_ENABLE_0) : (RBUS_PRE_ARB_OVERFLOW_ENABLE_1));
	}

	return (unsigned long)ps->base + RBUS_POST_ARB_OVERFLOW_ENABLE_0;
}

static inline int
__has_multiple_ov(struct rtk_pmu *pmu)
{
	return rtk_pmu_last_client(pmu->pmcss[PMC_SET__RBUS]->meta) >=
		RBUS_OVERFLOW_STATUS_WIDTH / RBUS_OVERFLOW_PMCG_WIDTH;
}

union rtk_rbus_event_desc {
	int val;
	struct {
		unsigned target:5;	/* target id */
		unsigned set:3;		/* pmc set */
		unsigned usage:4;	/* pmc usage */
	};
};

union rtk_rbus_arb_event_desc {
	int val;
	struct {
		unsigned target:20;	/* target id */
		unsigned set:3;		/* pmc set */
		unsigned usage:4;	/* pmc usage */
	};
};

#define SET_OFFSET			5
#define USAGE_OFFSET			8
#define TARGET_MASK			0x001f
#define HWC_MASK			0x00ff

#define ARB_CPU_ID_OFFSET		8
#define ARB_SET_OFFSET			20
#define ARB_USAGE_OFFSET		23
#define ARB_RBUS_TARGET_MASK		GENMASK(5, 0)
#define ARB_TARGET_MASK			GENMASK(19, 0)
#define ARB_HWC_MASK			GENMASK(22, 0)
#define RBUS_POST_ARB_CTRL_OFFSET	16
#define RBUS_PRE_ARB_CPU_SEL_OFFSET	16
#define RBUS_ARB_USAGE_OFFSET		0x40
#define RBUS_CUSTOM_RANGE_ID		0xff

#define RBUS_EV_CONFIG(ev, set, usage)	\
	((ev) | (PMC_SET__##set << SET_OFFSET) | \
	 (RBUS_PMC__##usage << USAGE_OFFSET))

#define RBUS_EV_CONFIG_V2(ev, set, usage)	\
	((ev) | (PMC_SET__##set << ARB_SET_OFFSET) | \
	 (RBUS_PMC__##usage << ARB_USAGE_OFFSET))

static inline union rtk_rbus_event_desc
__get_event_desc(u64 config)
{
	union rtk_rbus_event_desc desc = {.val = (int)config};

	return desc;
}

static inline union rtk_rbus_arb_event_desc
__get_event_desc_v2(u64 config)
{
	union rtk_rbus_arb_event_desc desc = {.val = (int)config};

	return desc;
}

static inline union rtk_rbus_event_desc
get_event_desc(struct perf_event *event)
{
	return __get_event_desc(event->attr.config);
}

static inline union rtk_rbus_arb_event_desc
get_event_desc_v2(struct perf_event *event)
{
	return __get_event_desc_v2(event->attr.config);
}

static inline int
rtk_rbus_pmc_target(u64 config)
{
	return (int)(config & TARGET_MASK);
}

static inline int
rtk_rbus_pmc_target_v2(u64 config)
{
	return (int)(config & ARB_TARGET_MASK);
}

int rbus_pmu_ps_init_v2(const struct rtk_pmc_set_meta *meta,
			struct rtk_pmc_set *ps);
int rbus_pmu_ps_drv_init(const struct rtk_pmc_set_meta *meta,
			 struct rtk_pmc_set *ps);
int rtk_rbus_pmu_init(struct rtk_pmu *pmu, struct device_node *dt,
		      const char *name,
		      const struct attribute_group **attr_groups,
		      const struct rtk_pmc_set_meta *meta, int nr_ps);
int rtk_rbus_pmu_init_v2(struct rtk_pmu *pmu, struct device_node *dt,
		      const char *name,
		      const struct attribute_group **attr_groups,
		      const struct rtk_pmc_set_meta *meta, int nr_ps);

extern struct attribute_group rtk_rbus_format_attr_group;

/* Platform specific init functions */
int rtk_16xxb_rbus_init(struct rtk_pmu *pmu, struct device_node *dt);
int rtk_13xxd_rbus_init(struct rtk_pmu *pmu, struct device_node *dt);
int rtk_1625_rbus_init(struct rtk_pmu *pmu, struct device_node *dt);


#endif /* End of __RTK_RBUS_PMU__ */
