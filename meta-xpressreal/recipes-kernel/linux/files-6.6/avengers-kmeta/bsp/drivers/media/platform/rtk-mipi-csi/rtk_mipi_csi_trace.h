/* SPDX-License-Identifier: GPL-2.0-only */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_mipicsi

#if !defined(_RTK_MIPICSI_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_MIPICSI_TRACE_H

#include <linux/tracepoint.h>
#include "rtk_mipi_csi.h"

TRACE_EVENT(mipicsi_func_event,
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


TRACE_EVENT(mipicsi_buf_queue,
	TP_PROTO(u32 vb_index),
	TP_ARGS(vb_index),
	TP_STRUCT__entry(
		__field(u32, vb_index)
	),
	TP_fast_assign(
		__entry->vb_index = vb_index;
	),
	TP_printk("buf%u --> queue", __entry->vb_index)
);

TRACE_EVENT(mipicsi_buf_dqueue,
	TP_PROTO(u32 sequence, u64 ktime_us),
	TP_ARGS(sequence, ktime_us),
	TP_STRUCT__entry(
		__field(u32, sequence)
		__field(u64, ktime_us)
	),
	TP_fast_assign(
		__entry->sequence = sequence;
		__entry->ktime_us = ktime_us;
	),
	TP_printk("buf <-- dqueue, sequence=%u ktime_us=%llu",
		__entry->sequence, __entry->ktime_us)
);

TRACE_EVENT(mipicsi_buf_dqueue_err,
	TP_PROTO(int ret),
	TP_ARGS(ret),
	TP_STRUCT__entry(
		__field(int, ret)
	),
	TP_fast_assign(
		__entry->ret = ret;
	),
	TP_printk("buf <-- dqueue, ret=%d", __entry->ret)
);

TRACE_EVENT(mipicsi_buf_prepare,
	TP_PROTO(u32 vb_index, u32 size),
	TP_ARGS(vb_index, size),
	TP_STRUCT__entry(
		__field(u32, vb_index)
		__field(u32, size)
	),
	TP_fast_assign(
		__entry->vb_index = vb_index;
		__entry->size = size;
	),
	TP_printk("buf_prepare, vb->index=%u size=%u",
		__entry->vb_index, __entry->size)
);

TRACE_EVENT(mipicsi_buf_finish,
	TP_PROTO(u32 vb_index),
	TP_ARGS(vb_index),
	TP_STRUCT__entry(
		__field(u32, vb_index)
	),
	TP_fast_assign(
		__entry->vb_index = vb_index;
	),
	TP_printk("buf_finish, vb->index=%u", __entry->vb_index)
);

TRACE_EVENT(mipicsi_app_crtl,
	TP_PROTO(u8 ch_index, u8 scaling_down,
		u8 is_compenc, u8 enable),
	TP_ARGS(ch_index, scaling_down, is_compenc, enable),
	TP_STRUCT__entry(
		__field(u8, ch_index)
		__field(u8, scaling_down)
		__field(u8, is_compenc)
		__field(u8, enable)
	),
	TP_fast_assign(
		__entry->ch_index = ch_index;
		__entry->scaling_down = scaling_down;
		__entry->is_compenc = is_compenc;
		__entry->enable = enable;
	),
	TP_printk("%s Ch%u, scaling_down=%s is_compenc=%s",
		__entry->enable ? "Enabled" : "Disabled", __entry->ch_index,
		__entry->scaling_down ? "Y" : "N", __entry->is_compenc ? "Y" : "N")
);


TRACE_EVENT(mipicsi_app_size_cfg,
	TP_PROTO(u8 ch_index, struct rtk_mipicsi *mipicsi),
	TP_ARGS(ch_index, mipicsi),
	TP_STRUCT__entry(
		__field(u8, ch_index)
		__field(u32, src_width)
		__field(u32, src_height)
		__field(u32, dst_width)
		__field(u32, dst_height)
		__field(u8, mode)
		__field(u32, line_pitch)
		__field(u32, header_pitch)
		__field(u32, video_size)
	),
	TP_fast_assign(
		__entry->ch_index = ch_index;
		__entry->src_width = mipicsi->src_width;
		__entry->src_height = mipicsi->src_height;
		__entry->dst_width = mipicsi->dst_width;
		__entry->dst_height = mipicsi->dst_height;
		__entry->mode = mipicsi->mode;
		__entry->line_pitch = mipicsi->line_pitch;
		__entry->header_pitch = mipicsi->header_pitch;
		__entry->video_size = mipicsi->video_size;
	),
	TP_printk("Ch%u [%ux%u]->[%ux%u] %s line_pitch=%u header_pitch=%u video_size=%u",
		__entry->ch_index, __entry->src_width, __entry->src_height,
		__entry->dst_width, __entry->dst_height, __entry->mode ? "COMPENC":"LINE",
		__entry->line_pitch, __entry->header_pitch, __entry->video_size)
);


TRACE_EVENT(mipicsi_dma_buf_cfg,
	TP_PROTO(u8 ch_index, u8 entry_index, u64 start_addr, u64 size),
	TP_ARGS(ch_index, entry_index, start_addr, size),
	TP_STRUCT__entry(
		__field(u8, ch_index)
		__field(u8, entry_index)
		__field(u64, start_addr)
		__field(u64, size)
	),
	TP_fast_assign(
		__entry->ch_index = ch_index;
		__entry->entry_index = entry_index;
		__entry->start_addr = start_addr;
		__entry->size = size;
	),
	TP_printk("Ch%u entry%u valid, start_addr=0x%08llx size=%llu",
		__entry->ch_index, __entry->entry_index,
		__entry->start_addr, __entry->size)
);


TRACE_EVENT(mipicsi_frame_done,
	TP_PROTO(u32 done_st, u8 ch_index, u8 entry_index),
	TP_ARGS(done_st, ch_index, entry_index),
	TP_STRUCT__entry(
		__field(u32, done_st)
		__field(u8, ch_index)
		__field(u8, entry_index)
	),
	TP_fast_assign(
		__entry->done_st = done_st;
		__entry->ch_index = ch_index;
		__entry->entry_index = entry_index;
	),
	TP_printk("Ch%u entry%u done, done_st=0x%08x",
		__entry->ch_index, __entry->entry_index, __entry->done_st)
);

TRACE_EVENT(mipicsi_skip_frame,
	TP_PROTO(u8 ch_index, u8 entry_index),
	TP_ARGS(ch_index, entry_index),
	TP_STRUCT__entry(
		__field(u8, ch_index)
		__field(u8, entry_index)
	),
	TP_fast_assign(
		__entry->ch_index = ch_index;
		__entry->entry_index = entry_index;
	),
	TP_printk("Ch%u entry%u skip",
		__entry->ch_index, __entry->entry_index)
);

TRACE_EVENT(mipicsi_buffer_done,
	TP_PROTO(u32 vb2_buf_id, u8 ch_index, u8 entry_index,
		u64 timestamp, u32 sequence),
	TP_ARGS(vb2_buf_id, ch_index, entry_index, timestamp, sequence),
	TP_STRUCT__entry(
		__field(u32, vb2_buf_id)
		__field(u8, ch_index)
		__field(u8, entry_index)
		__field(u64, timestamp)
		__field(u32, sequence)
	),
	TP_fast_assign(
		__entry->vb2_buf_id = vb2_buf_id;
		__entry->ch_index = ch_index;
		__entry->entry_index = entry_index;
		__entry->timestamp = timestamp;
		__entry->sequence = sequence;
	),
	TP_printk("buf%u done by Ch%u entry%u, ktime_us=%llu sequence=%u",
		__entry->vb2_buf_id, __entry->ch_index, __entry->entry_index,
		__entry->timestamp, __entry->sequence)
);

TRACE_EVENT(mipicsi_dump_frame_state,
	TP_PROTO(u8 ch_index, u32 frame_width, u32 frame_height,
		u32 errcnt, u32 frame_cnt, u32 line_cnt, u32 pixel_cnt),
	TP_ARGS(ch_index, frame_width, frame_height,
		errcnt, frame_cnt, line_cnt, pixel_cnt),
	TP_STRUCT__entry(
		__field(u8, ch_index)
		__field(u32, frame_width)
		__field(u32, frame_height)
		__field(u32, errcnt)
		__field(u32, frame_cnt)
		__field(u32, line_cnt)
		__field(u32, pixel_cnt)
	),
	TP_fast_assign(
		__entry->ch_index = ch_index;
		__entry->frame_width = frame_width;
		__entry->frame_height = frame_height;
		__entry->errcnt = errcnt;
		__entry->frame_cnt = frame_cnt;
		__entry->line_cnt = line_cnt;
		__entry->pixel_cnt = pixel_cnt;
	),
	TP_printk("Ch%u: width=%u height=%u errcnt=%u frame_cnt=%u line_cnt=%u pixel_cnt=%u",
		__entry->ch_index, __entry->frame_width, __entry->frame_height,
		__entry->errcnt, __entry->frame_cnt, __entry->line_cnt, __entry->pixel_cnt)
);

#endif /* _RTK_MIPICSI_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE rtk_mipi_csi_trace
#include <trace/define_trace.h>
