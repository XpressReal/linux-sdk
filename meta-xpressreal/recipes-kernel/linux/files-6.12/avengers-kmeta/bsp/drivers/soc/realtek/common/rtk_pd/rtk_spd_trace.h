#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_spd

#if !defined(_RTK_SPD_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_SPD_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(rtk_spd_pwr_control,
	TP_PROTO(struct device *dev, int offset, int mask, int val),
	TP_ARGS(dev, offset, mask, val),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(int, offset)
		__field(int, mask)
		__field(int, val)
	),
	TP_fast_assign(
		__assign_str(device);
		__entry->offset = offset;
		__entry->mask = mask;
		__entry->val = val;
	),
	TP_printk("dev_name=%s offset=%#x mask=%#x val=%#x",
		  __get_str(device), __entry->offset, __entry->mask, __entry->val)
);

TRACE_EVENT(rtk_spd_reset_control,
	TP_PROTO(struct device *dev, int deassert),
	TP_ARGS(dev, deassert),
	TP_STRUCT__entry(
		__string(device, dev_name(dev))
		__field(int, deassert)
	),
	TP_fast_assign(
		__assign_str(device);
		__entry->deassert = deassert;
	),
	TP_printk("dev_name=%s deassert=%d",
		  __get_str(device), __entry->deassert)
);

TRACE_EVENT(rtk_spd_clk_control,
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

#endif /* _RTK_SPD_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/soc/realtek/common/rtk_pd
#define TRACE_INCLUDE_FILE rtk_spd_trace
#include <trace/define_trace.h>
