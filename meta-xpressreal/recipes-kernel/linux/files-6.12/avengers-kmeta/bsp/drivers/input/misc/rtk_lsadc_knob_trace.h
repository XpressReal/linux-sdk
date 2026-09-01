/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_lsadc_knob

#if !defined(_RTK_LSADC_KNOB_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_LSADC_KNOB_TRACE_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/tracepoint.h>
#include "rtk_lsadc_knob.h"

#define RTK_LSADC_KNOB_VOLTAGE_LIST \
	{RTK_LSADC_KNOB_VOLTAGE_ZERO, "0"}, \
	{RTK_LSADC_KNOB_VOLTAGE_LOW, "L"}, \
	{RTK_LSADC_KNOB_VOLTAGE_HIGH, "H"}

#define RTK_LSADC_KNOB_ROTATE_LIST \
	{RTK_LSADC_KNOB_ROTATE_NONE, "NONE"}, \
	{RTK_LSADC_KNOB_ROTATE_CLOCKWISE, "CLOCKWISE"}, \
	{RTK_LSADC_KNOB_ROTATE_ANTICLOCKWISE, "ANTICLOCKWISE"}

TRACE_EVENT(rtk_lsadc_knob_rotate,
	TP_PROTO(struct device *dev,
		 struct rtk_lsadc_knob_vstate *p,
		 struct rtk_lsadc_knob_vstate *c,
		 u32 ratio, u32 res),
	TP_ARGS(dev, p, c, ratio, res),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(u32, p_state)
		__field(u32, p_duration)
		__field(u32, c_state)
		__field(u32, c_duration)
		__field(u32, ratio)
		__field(u32, res)
	),
	TP_fast_assign(
		__assign_str(device);
		__entry->p_state = p->state;
		__entry->p_duration = p->duration;
		__entry->c_state = c->state;
		__entry->c_duration = c->duration;
		__entry->ratio = ratio;
		__entry->res = res;
	),
	TP_printk("%s: c=%6u/%s, p=%6u/%s, ratio=%4u, res=%s",
		  __get_str(device),
		  __entry->p_duration,
		  __print_symbolic(__entry->p_state, RTK_LSADC_KNOB_VOLTAGE_LIST),
		  __entry->c_duration,
		  __print_symbolic(__entry->c_state, RTK_LSADC_KNOB_VOLTAGE_LIST),
		  __entry->ratio,
		  __print_symbolic(__entry->res, RTK_LSADC_KNOB_ROTATE_LIST)
	)
);

#endif /* _RTK_LSADC_KNOB_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/input/misc
#define TRACE_INCLUDE_FILE rtk_lsadc_knob_trace
#include <trace/define_trace.h>
