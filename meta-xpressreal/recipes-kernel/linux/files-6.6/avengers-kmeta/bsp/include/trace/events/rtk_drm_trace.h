/* SPDX-License-Identifier: GPL-2.0-only */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_drm

#if !defined(_RTK_DRM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_DRM_TRACE_H

#include <linux/tracepoint.h>
#include <linux/version.h>
#include <drm/drm_vblank.h>

#define TRACE_LATE_PENDING_EVENT 0
#define TRACE_NO_PENDING_EVENT 1
#define TRACE_LATE_WRITE_CMD 2

/* __assign_str() dropped the src argument in kernel 6.10 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
#define __rtk_assign_str(field, src)	__assign_str(field)
#else
#define __rtk_assign_str(field, src)	__assign_str(field, src)
#endif

extern const char *trace_func_names[];

TRACE_EVENT(rtk_drm_func_event,
	TP_PROTO(unsigned int index),
	TP_ARGS(index),
	TP_STRUCT__entry(
		__field(unsigned int, index)
	),
	TP_fast_assign(
		__entry->index = index;
	),
	TP_printk("[%s]", trace_func_names[__entry->index])
);

TRACE_EVENT(rtk_drm_context_update_fail,
	TP_PROTO(const char *plane_name, unsigned int context),
	TP_ARGS(plane_name, context),
	TP_STRUCT__entry(
		__string(plane_name, plane_name)
		__field(unsigned int, context)
	),
	TP_fast_assign(
		__rtk_assign_str(plane_name, plane_name);
		__entry->context = context;
	),
	TP_printk("%s context %u update fail",
		__get_str(plane_name), __entry->context)
);

TRACE_EVENT(rtk_drm_fence_update,
	TP_PROTO(const char *plane_name, unsigned int ctx,
		 int total, int signaled, const char *sig_buf,
		 int pending, const char *pend_buf),
	TP_ARGS(plane_name, ctx, total, signaled, sig_buf, pending, pend_buf),
	TP_STRUCT__entry(
		__string(plane_name, plane_name)
		__field(unsigned int, ctx)
		__field(int, total)
		__field(int, signaled)
		__string(sig_buf, sig_buf)
		__field(int, pending)
		__string(pend_buf, pend_buf)
	),
	TP_fast_assign(
		__rtk_assign_str(plane_name, plane_name);
		__entry->ctx = ctx;
		__entry->total = total;
		__entry->signaled = signaled;
		__rtk_assign_str(sig_buf, sig_buf);
		__entry->pending = pending;
		__rtk_assign_str(pend_buf, pend_buf);
	),
	TP_printk("%s ctx=%u total=%d signaled=%d[%s] pending=%d[%s]",
		__get_str(plane_name), __entry->ctx,
		__entry->total, __entry->signaled, __get_str(sig_buf),
		__entry->pending, __get_str(pend_buf))
);

#endif /* _RTK_DRM_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH trace/events
#define TRACE_INCLUDE_FILE rtk_drm_trace
#include <trace/define_trace.h>
