/* SPDX-License-Identifier: GPL-2.0-only */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_vi

#if !defined(_RTK_VI_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_VI_TRACE_H

#include <linux/tracepoint.h>
#include "rtk_video_in.h"

TRACE_EVENT(vi_func_event,
	TP_PROTO(const char *event),
	TP_ARGS(event),
	TP_STRUCT__entry(
		__string(event, event)
	),
	TP_fast_assign(
		__assign_str(event, event);
	),
	TP_printk("[%s]", __get_str(event))
);


TRACE_EVENT(vi_buf_queue,
	TP_PROTO(unsigned vb_index),
	TP_ARGS(vb_index),
	TP_STRUCT__entry(
		__field(u32, vb_index)
	),
	TP_fast_assign(
		__entry->vb_index = vb_index;
	),
	TP_printk("buf%u --> queue", __entry->vb_index)
);


TRACE_EVENT(vi_buf_dqueue,
	TP_PROTO(unsigned ret, unsigned ms),
	TP_ARGS(ret, ms),
	TP_STRUCT__entry(
		__field(int, ret)
		__field(unsigned int, ms)
	),
	TP_fast_assign(
		__entry->ret = ret;
		__entry->ms = ms;
	),
	TP_printk("buf <-- dqueue, ret=%d %ums",
		__entry->ret, __entry->ms)
);

TRACE_EVENT(vi_mac_crtl,
	TP_PROTO(unsigned enable),
	TP_ARGS(enable),
	TP_STRUCT__entry(
		__field(u8, enable)
	),
	TP_fast_assign(
		__entry->enable = enable;
	),
	TP_printk("%s mac go",
		__entry->enable ? "Enabled" : "Disabled")
);


TRACE_EVENT(vi_frame_done,
	TP_PROTO(unsigned entry_index, unsigned done_st_a, unsigned done_st_b),
	TP_ARGS(entry_index, done_st_a, done_st_b),
	TP_STRUCT__entry(
		__field(u8, entry_index)
		__field(u32, done_st_a)
		__field(u32, done_st_b)
	),
	TP_fast_assign(
		__entry->entry_index = entry_index;
		__entry->done_st_a = done_st_a;
		__entry->done_st_b = done_st_b;
	),
	TP_printk("entry%u done, done_st_a=0x%08x done_st_b=0x%08x",
		__entry->entry_index, __entry->done_st_a, __entry->done_st_b)
);

TRACE_EVENT(vi_skip_frame,
	TP_PROTO(unsigned entry_index),
	TP_ARGS(entry_index),
	TP_STRUCT__entry(
		__field(u8, entry_index)
	),
	TP_fast_assign(
		__entry->entry_index = entry_index;
	),
	TP_printk("entry%u skip",
		__entry->entry_index)
);

TRACE_EVENT(vi_buffer_done,
	TP_PROTO(unsigned vb2_buf_id, unsigned entry_index,
		unsigned timestamp, unsigned sequence),
	TP_ARGS(vb2_buf_id, entry_index, timestamp, sequence),
	TP_STRUCT__entry(
		__field(u32, vb2_buf_id)
		__field(u8, entry_index)
		__field(u64, timestamp)
		__field(u32, sequence)
	),
	TP_fast_assign(
		__entry->vb2_buf_id = vb2_buf_id;
		__entry->entry_index = entry_index;
		__entry->timestamp = timestamp;
		__entry->sequence = sequence;
	),
	TP_printk("buf%u done by entry%u, ktime_ns=%llu sequence=%u",
		__entry->vb2_buf_id, __entry->entry_index,
		__entry->timestamp, __entry->sequence)
);

TRACE_EVENT(vi_dma_buf_cfg,
	TP_PROTO(u8 entry_index, u64 start_addr, u64 size),
	TP_ARGS(entry_index, start_addr, size),
	TP_STRUCT__entry(
		__field(u8, entry_index)
		__field(u64, start_addr)
		__field(u64, size)
	),
	TP_fast_assign(
		__entry->entry_index = entry_index;
		__entry->start_addr = start_addr;
		__entry->size = size;
	),
	TP_printk("entry%u valid, start_addr=0x%016llx size=%llu",
		__entry->entry_index,
		__entry->start_addr, __entry->size)
);


#endif /* _RTK_VI_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE rtk_video_in_trace
#include <trace/define_trace.h>
