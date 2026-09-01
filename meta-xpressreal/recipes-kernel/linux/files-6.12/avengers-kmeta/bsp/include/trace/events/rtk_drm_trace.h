/* SPDX-License-Identifier: GPL-2.0-only */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_drm

#if !defined(_RTK_DRM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_DRM_TRACE_H

#include <linux/tracepoint.h>
#include <drm/drm_vblank.h>

#define TRACE_LATE_PENDING_EVENT 0
#define TRACE_NO_PENDING_EVENT 1

#define TRACE_OSD1_PLANE 0
#define TRACE_SUB1_PLANE 1
#define TRACE_OSD3_PLANE 2
#define TRACE_OSD4_PLANE 3

extern const char *trace_func_names[];
extern const char *trace_plane_names[];

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
	TP_PROTO(unsigned int plane_idx,
		unsigned int context),
	TP_ARGS(plane_idx, context),
	TP_STRUCT__entry(
		__field(unsigned int, plane_idx)
		__field(unsigned int, context)
	),
	TP_fast_assign(
		__entry->plane_idx = plane_idx;
		__entry->context = context;
	),
	TP_printk("%s context %u update fail",
		trace_plane_names[__entry->plane_idx], __entry->context)
);

#endif /* _RTK_DRM_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH trace/events
#define TRACE_INCLUDE_FILE rtk_drm_trace
#include <trace/define_trace.h>
