/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_xdi

#if !defined(_RTK_XDI_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_XDI_TRACE_H

#include <linux/tracepoint.h>
#include "rtk_xdi_internal.h"

TRACE_EVENT(xdi_cmd_start,
	TP_PROTO(struct rtk_xdi_context *ctx, u32 id),
	TP_ARGS(ctx, id),
	TP_STRUCT__entry(
		__field(struct rtk_xdi_context *, ctx)
		__field(u32, id)
	),
	TP_fast_assign(
		__entry->ctx = ctx;
		__entry->id = id;
	),
	TP_printk("ctx=%p id=%u", __entry->ctx, __entry->id)
);

TRACE_EVENT(xdi_cmd_end,
	TP_PROTO(struct rtk_xdi_context *ctx, u32 id, int result),
	TP_ARGS(ctx, id, result),
	TP_STRUCT__entry(
		__field(struct rtk_xdi_context *, ctx)
		__field(u32, id)
		__field(int, result)
	),
	TP_fast_assign(
		__entry->ctx = ctx;
		__entry->id = id;
		__entry->result = result;
	),
	TP_printk("ctx=%p id=%u result=%d", __entry->ctx, __entry->id, __entry->result)
);

TRACE_EVENT(xdi_cmd_queue,
	TP_PROTO(struct rtk_xdi_context *ctx, u32 id),
	TP_ARGS(ctx, id),
	TP_STRUCT__entry(
		__field(struct rtk_xdi_context *, ctx)
		__field(u32, id)
	),
	TP_fast_assign(
		__entry->ctx = ctx;
		__entry->id = id;
	),
	TP_printk("ctx=%p id=%u", __entry->ctx, __entry->id)
);

#endif /* _RTK_XDI_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/media/platform/rtk-xdi
#define TRACE_INCLUDE_FILE rtk_xdi_trace
#include <trace/define_trace.h>
