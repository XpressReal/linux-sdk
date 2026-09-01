#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_gpc

#if !defined(_RTK_GPC_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_GPC_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(rtk_gpc_sram_power,
	TP_PROTO(struct device *dev, int on, int ret),
	TP_ARGS(dev, on, ret),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(int, on)
		__field(int, ret)
	),
	TP_fast_assign(
		__assign_str(device);
		__entry->on = on;
		__entry->ret = ret;
	),
	TP_printk("dev_name=%s on=%d ret=%d",
		  __get_str(device), __entry->on, __entry->ret)
);

TRACE_EVENT(rtk_gpc_iso_control,
	TP_PROTO(struct device *dev, int offset, int mask, int isolate),
	TP_ARGS(dev, offset, mask, isolate),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(int, offset)
		__field(int, mask)
		__field(int, isolate)
	),
	TP_fast_assign(
		__assign_str(device);
		__entry->offset = offset;
		__entry->mask = mask;
		__entry->isolate = isolate;
	),
	TP_printk("dev_name=%s offset=%#x mask=%#x isolate=%d",
		  __get_str(device), __entry->offset, __entry->mask, __entry->isolate)
);

TRACE_EVENT(rtk_gpc_reset_control,
	TP_PROTO(struct device *dev, int id, int deassert),
	TP_ARGS(dev, id, deassert),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(int, id)
		__field(int, deassert)
	),
	TP_fast_assign(
		__assign_str(device);
		__entry->id = id;
		__entry->deassert = deassert;
	),
	TP_printk("dev_name=%s id=%d deassert=%d",
		  __get_str(device), __entry->id, __entry->deassert)
);

TRACE_EVENT(rtk_gpc_clk_control,
	TP_PROTO(struct device *dev, int enable),
	TP_ARGS(dev, enable),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(int, enable)
	),
	TP_fast_assign(
		__assign_str(device);
		__entry->enable = enable;
	),
	TP_printk("dev_name=%s enable=%d",
		  __get_str(device), __entry->enable)
);

#endif /* _RTK_GPC_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/soc/realtek/common/rtk_pd
#define TRACE_INCLUDE_FILE rtk_gpc_trace
#include <trace/define_trace.h>
