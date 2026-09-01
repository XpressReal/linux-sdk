// SPDX-License-Identifier: GPL-2.0
#define CREATE_TRACE_POINTS
#include <trace/events/rtk_drm_trace.h>

const char *trace_func_names[] = {
	"late pending event",
	"no pending event",
};

const char *trace_plane_names[] = {
	"OSD1", "SUB1", "OSD3", "OSD4",
};
