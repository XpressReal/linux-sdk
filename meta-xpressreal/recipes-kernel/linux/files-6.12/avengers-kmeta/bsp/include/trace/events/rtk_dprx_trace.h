/* SPDX-License-Identifier: GPL-2.0-only */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM rtk_dprx

#if !defined(_RTK_DPRX_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _RTK_DPRX_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(dprx_func_event,
	TP_PROTO(const char *event),
	TP_ARGS(event),
	TP_STRUCT__entry(
		__string(event, event)
	),
	TP_fast_assign(
		__assign_str(event);
	),
	TP_printk("[%s]", __get_str(event))
);

TRACE_EVENT(dprx_video_size_cfg,
	TP_PROTO(u32 src_width, u32 src_height, u32 dst_width, u32 dst_height,
		bool compenc_mode, u32 line_pitch, u32 header_pitch, u32 video_size),
	TP_ARGS(src_width, src_height, dst_width, dst_height,
		compenc_mode, line_pitch, header_pitch, video_size),
	TP_STRUCT__entry(
		__field(u32, src_width)
		__field(u32, src_height)
		__field(u32, dst_width)
		__field(u32, dst_height)
		__field(bool, compenc_mode)
		__field(u32, line_pitch)
		__field(u32, header_pitch)
		__field(u32, video_size)
	),
	TP_fast_assign(
		__entry->src_width = src_width;
		__entry->src_height = src_height;
		__entry->dst_width = dst_width;
		__entry->dst_height = dst_height;
		__entry->compenc_mode = compenc_mode;
		__entry->line_pitch = line_pitch;
		__entry->header_pitch = header_pitch;
		__entry->video_size = video_size;
	),
	TP_printk("[%ux%u]->[%ux%u] %.7s line_pitch=%u header_pitch=%u video_size=%u",
		__entry->src_width, __entry->src_height,
		__entry->dst_width, __entry->dst_height, __entry->compenc_mode ? "COMPENC":"LINE",
		__entry->line_pitch, __entry->header_pitch, __entry->video_size)
);

TRACE_EVENT(dprx_dma_buf_cfg,
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
	TP_printk("Set entry%u valid, start_addr=0x%08llx size=%llu",
		__entry->entry_index,
		__entry->start_addr, __entry->size)
);

TRACE_EVENT(dprx_wrapper_config,
	TP_PROTO(u8 src_fmt, u8 scaling_down,
		u8 is_compenc, u8 enable),
	TP_ARGS(src_fmt, scaling_down, is_compenc, enable),
	TP_STRUCT__entry(
		__field(u8, src_fmt)
		__field(u8, scaling_down)
		__field(u8, is_compenc)
		__field(u8, enable)
	),
	TP_fast_assign(
		__entry->src_fmt = src_fmt;
		__entry->scaling_down = scaling_down;
		__entry->is_compenc = is_compenc;
		__entry->enable = enable;
	),
	TP_printk("%s src_fmt=%u, scaling_down=%s is_compenc=%s",
		__entry->enable ? "Enabled" : "Disabled", __entry->src_fmt,
		__entry->scaling_down ? "Y" : "N", __entry->is_compenc ? "Y" : "N")
);

TRACE_EVENT(dprx_buf_queue,
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

TRACE_EVENT(dprx_buf_dqueue,
	TP_PROTO(int ret, u32 ms),
	TP_ARGS(ret, ms),
	TP_STRUCT__entry(
		__field(int, ret)
		__field(u32, ms)
	),
	TP_fast_assign(
		__entry->ret = ret;
		__entry->ms = ms;
	),
	TP_printk("buf <-- dqueue, ret=%d %ums",
		__entry->ret, __entry->ms)
);

/*
 * Link Training Tracepoints - CR Phase
 * Low-latency tracing (~1-2µs) for debugging without affecting LT timing
 */

/**
 * dprx_lt_state - Track state machine transitions
 * @old_state: Previous state
 * @new_state: New state
 * @event: Event that triggered transition
 */
TRACE_EVENT(dprx_lt_state,
	TP_PROTO(u8 old_state, u8 new_state, u8 event),
	TP_ARGS(old_state, new_state, event),
	TP_STRUCT__entry(
		__field(u8, old_state)
		__field(u8, new_state)
		__field(u8, event)
	),
	TP_fast_assign(
		__entry->old_state = old_state;
		__entry->new_state = new_state;
		__entry->event = event;
	),
	TP_printk("LT: %u -> %u (event=%u)",
		__entry->old_state, __entry->new_state, __entry->event)
);

/**
 * dprx_lt_dpcd_irq - Track DPCD IRQ events
 * @irq_status: Raw IRQ status from PB7_DD_AUX_DPCD_IRQ
 * @in_isr: Whether called from hardirq context
 */
TRACE_EVENT(dprx_lt_dpcd_irq,
	TP_PROTO(u8 irq_status, bool in_isr),
	TP_ARGS(irq_status, in_isr),
	TP_STRUCT__entry(
		__field(u8, irq_status)
		__field(bool, in_isr)
	),
	TP_fast_assign(
		__entry->irq_status = irq_status;
		__entry->in_isr = in_isr;
	),
	TP_printk("LT IRQ: 0x%02x %s",
		__entry->irq_status,
		__entry->in_isr ? "[ISR]" : "[thread]")
);

/**
 * dprx_lt_tp_set - Track Training Pattern changes
 * @tp_raw: Raw value of DPCD 0x102
 * @tp_select: Training pattern (0-4)
 * @scramble_off: Scrambling disabled flag
 */
TRACE_EVENT(dprx_lt_tp_set,
	TP_PROTO(u8 tp_raw, u8 tp_select, bool scramble_off),
	TP_ARGS(tp_raw, tp_select, scramble_off),
	TP_STRUCT__entry(
		__field(u8, tp_raw)
		__field(u8, tp_select)
		__field(bool, scramble_off)
	),
	TP_fast_assign(
		__entry->tp_raw = tp_raw;
		__entry->tp_select = tp_select;
		__entry->scramble_off = scramble_off;
	),
	TP_printk("LT: TP%u (raw=0x%02x, scramble=%s)",
		__entry->tp_select, __entry->tp_raw,
		__entry->scramble_off ? "OFF" : "ON")
);

/**
 * dprx_lt_cr_status - Track CR status for each lane
 * @lane_count: Number of lanes
 * @lane01_status: DPCD 0x202 value (Lane 0-1 status)
 * @lane23_status: DPCD 0x203 value (Lane 2-3 status)
 * @all_cr_done: Whether all lanes have CR_DONE
 */
TRACE_EVENT(dprx_lt_cr_status,
	TP_PROTO(u8 lane_count, u8 lane01_status, u8 lane23_status, bool all_cr_done),
	TP_ARGS(lane_count, lane01_status, lane23_status, all_cr_done),
	TP_STRUCT__entry(
		__field(u8, lane_count)
		__field(u8, lane01_status)
		__field(u8, lane23_status)
		__field(bool, all_cr_done)
	),
	TP_fast_assign(
		__entry->lane_count = lane_count;
		__entry->lane01_status = lane01_status;
		__entry->lane23_status = lane23_status;
		__entry->all_cr_done = all_cr_done;
	),
	TP_printk("LT CR: lanes=%u 0x202=0x%02x 0x203=0x%02x %s",
		__entry->lane_count, __entry->lane01_status, __entry->lane23_status,
		__entry->all_cr_done ? "CR_DONE" : "CR_NOT_DONE")
);

/**
 * dprx_lt_adjust_req - Track VS/PE adjust request
 * @adj_lane01: DPCD 0x206 value (Adjust Request Lane 0-1)
 * @adj_lane23: DPCD 0x207 value (Adjust Request Lane 2-3)
 * @lane_count: Number of active lanes
 */
TRACE_EVENT(dprx_lt_adjust_req,
	TP_PROTO(u8 adj_lane01, u8 adj_lane23, u8 lane_count),
	TP_ARGS(adj_lane01, adj_lane23, lane_count),
	TP_STRUCT__entry(
		__field(u8, adj_lane01)
		__field(u8, adj_lane23)
		__field(u8, lane_count)
	),
	TP_fast_assign(
		__entry->adj_lane01 = adj_lane01;
		__entry->adj_lane23 = adj_lane23;
		__entry->lane_count = lane_count;
	),
	TP_printk("LT ADJ: 0x206=0x%02x 0x207=0x%02x lanes=%u",
		__entry->adj_lane01, __entry->adj_lane23, __entry->lane_count)
);

/**
 * dprx_lt_fast_cr - Track fast CR update in hardirq
 * @lane_count: Number of lanes
 * @cr_done_mask: Bitmask of CR_DONE lanes
 * @elapsed_ns: Time elapsed in nanoseconds (0 if not measured)
 */
TRACE_EVENT(dprx_lt_fast_cr,
	TP_PROTO(u8 lane_count, u8 cr_done_mask, u32 elapsed_ns),
	TP_ARGS(lane_count, cr_done_mask, elapsed_ns),
	TP_STRUCT__entry(
		__field(u8, lane_count)
		__field(u8, cr_done_mask)
		__field(u32, elapsed_ns)
	),
	TP_fast_assign(
		__entry->lane_count = lane_count;
		__entry->cr_done_mask = cr_done_mask;
		__entry->elapsed_ns = elapsed_ns;
	),
	TP_printk("LT FAST CR: lanes=%u cr_mask=0x%02x %uns",
		__entry->lane_count, __entry->cr_done_mask, __entry->elapsed_ns)
);

/**
 * dprx_lt_cr_defer - Track CR_DONE deferral for drive setting optimization
 * @lane_count: Number of lanes
 * @cdr_locked_mask: Bitmask of CDR locked lanes (from PHY)
 * @cr_reported_mask: Bitmask of lanes reporting CR_DONE (may be less due to deferral)
 * @is_first_lock: true if this is the first CDR lock (deferral active)
 *
 * Per DP spec, Sink may delay setting CR_DONE until drive settings are
 * optimized. This trace helps debug the deferral mechanism.
 */
TRACE_EVENT(dprx_lt_cr_defer,
	TP_PROTO(u8 lane_count, u8 cdr_locked_mask, u8 cr_reported_mask, bool is_first_lock),
	TP_ARGS(lane_count, cdr_locked_mask, cr_reported_mask, is_first_lock),
	TP_STRUCT__entry(
		__field(u8, lane_count)
		__field(u8, cdr_locked_mask)
		__field(u8, cr_reported_mask)
		__field(bool, is_first_lock)
	),
	TP_fast_assign(
		__entry->lane_count = lane_count;
		__entry->cdr_locked_mask = cdr_locked_mask;
		__entry->cr_reported_mask = cr_reported_mask;
		__entry->is_first_lock = is_first_lock;
	),
	TP_printk("LT CR DEFER: lanes=%u cdr=0x%02x reported=0x%02x %s",
		__entry->lane_count, __entry->cdr_locked_mask, __entry->cr_reported_mask,
		__entry->is_first_lock ? "DEFERRED" : "NORMAL")
);

/**
 * dprx_lt_cdr_tp1_detect - Track TP1 detection status for CDR lock check
 * @lane_mask: Input lane mask (which lanes to check)
 * @tp1_detect: Raw TP1 detection value from register 0x98166050
 * @locked_mask: Output CDR locked lane mask
 *
 * Traces the hardware TP1 detection status read from DPRX14_MAC_IP_EQ_CRC_3.
 * Used to debug CDR lock timing issues during Clock Recovery phase.
 */
TRACE_EVENT(dprx_lt_cdr_tp1_detect,
	TP_PROTO(u8 lane_mask, u8 tp1_detect, u8 locked_mask),
	TP_ARGS(lane_mask, tp1_detect, locked_mask),
	TP_STRUCT__entry(
		__field(u8, lane_mask)
		__field(u8, tp1_detect)
		__field(u8, locked_mask)
	),
	TP_fast_assign(
		__entry->lane_mask = lane_mask;
		__entry->tp1_detect = tp1_detect;
		__entry->locked_mask = locked_mask;
	),
	TP_printk("LT CDR: lane_mask=0x%02x tp1_detect=0x%02x locked=0x%02x",
		__entry->lane_mask, __entry->tp1_detect, __entry->locked_mask)
);

/**
 * dprx_lt_link_config - Track Link Config (0x100-0x101)
 * @link_rate: Link rate code (0x06=RBR, 0x0A=HBR, 0x14=HBR2, 0x1E=HBR3)
 * @lane_count: Lane count (1, 2, or 4)
 * @enhanced_frame: Enhanced framing enabled
 */
TRACE_EVENT(dprx_lt_link_config,
	TP_PROTO(u8 link_rate, u8 lane_count, bool enhanced_frame),
	TP_ARGS(link_rate, lane_count, enhanced_frame),
	TP_STRUCT__entry(
		__field(u8, link_rate)
		__field(u8, lane_count)
		__field(bool, enhanced_frame)
	),
	TP_fast_assign(
		__entry->link_rate = link_rate;
		__entry->lane_count = lane_count;
		__entry->enhanced_frame = enhanced_frame;
	),
	TP_printk("LT CFG: rate=0x%02x lanes=%u enhanced=%d",
		__entry->link_rate, __entry->lane_count, __entry->enhanced_frame)
);

/**
 * dprx_lt_cr_retry - Track CR retry status
 * @retry_count: Current retry count
 * @same_vs_count: Same VS request count
 * @max_retry: Maximum retry limit
 */
TRACE_EVENT(dprx_lt_cr_retry,
	TP_PROTO(u8 retry_count, u8 same_vs_count, u8 max_retry),
	TP_ARGS(retry_count, same_vs_count, max_retry),
	TP_STRUCT__entry(
		__field(u8, retry_count)
		__field(u8, same_vs_count)
		__field(u8, max_retry)
	),
	TP_fast_assign(
		__entry->retry_count = retry_count;
		__entry->same_vs_count = same_vs_count;
		__entry->max_retry = max_retry;
	),
	TP_printk("LT CR: retry=%u/%u same_vs=%u",
		__entry->retry_count, __entry->max_retry, __entry->same_vs_count)
);

/*
 * Link Training Tracepoints - EQ Phase
 */

/**
 * dprx_lt_eq_status - Track EQ status for each lane
 * @lane_count: Number of lanes
 * @eq_done_mask: Bitmask of EQ_DONE lanes
 * @symbol_lock_mask: Bitmask of SYMBOL_LOCKED lanes
 * @lane_aligned: true if INTERLANE_ALIGN_DONE
 */
TRACE_EVENT(dprx_lt_eq_status,
	TP_PROTO(u8 lane_count, u8 eq_done_mask, u8 symbol_lock_mask, bool lane_aligned),
	TP_ARGS(lane_count, eq_done_mask, symbol_lock_mask, lane_aligned),
	TP_STRUCT__entry(
		__field(u8, lane_count)
		__field(u8, eq_done_mask)
		__field(u8, symbol_lock_mask)
		__field(bool, lane_aligned)
	),
	TP_fast_assign(
		__entry->lane_count = lane_count;
		__entry->eq_done_mask = eq_done_mask;
		__entry->symbol_lock_mask = symbol_lock_mask;
		__entry->lane_aligned = lane_aligned;
	),
	TP_printk("LT EQ: lanes=%u eq=0x%02x sym=0x%02x %s",
		__entry->lane_count, __entry->eq_done_mask, __entry->symbol_lock_mask,
		__entry->lane_aligned ? "ALIGNED" : "NOT_ALIGNED")
);

/**
 * dprx_lt_eq_retry - Track EQ retry status
 * @retry_count: Current retry count
 * @max_retry: Maximum retry limit
 * @eq_done: EQ_DONE achieved
 * @sym_locked: SYMBOL_LOCKED achieved
 */
TRACE_EVENT(dprx_lt_eq_retry,
	TP_PROTO(u8 retry_count, u8 max_retry, bool eq_done, bool sym_locked),
	TP_ARGS(retry_count, max_retry, eq_done, sym_locked),
	TP_STRUCT__entry(
		__field(u8, retry_count)
		__field(u8, max_retry)
		__field(bool, eq_done)
		__field(bool, sym_locked)
	),
	TP_fast_assign(
		__entry->retry_count = retry_count;
		__entry->max_retry = max_retry;
		__entry->eq_done = eq_done;
		__entry->sym_locked = sym_locked;
	),
	TP_printk("LT EQ: retry=%u/%u eq=%s sym=%s",
		__entry->retry_count, __entry->max_retry,
		__entry->eq_done ? "DONE" : "NOT_DONE",
		__entry->sym_locked ? "LOCKED" : "NOT_LOCKED")
);

/*
 * Link Training Diagnostic Tracepoints
 * For debugging 20ms delay issue
 */

/**
 * dprx_lt_thread_enter - Track threaded handler entry
 * @irq_pending: Pending IRQ status bits
 *
 * Used to measure scheduling latency between hardirq and threaded handler
 */
TRACE_EVENT(dprx_lt_thread_enter,
	TP_PROTO(u8 irq_pending),
	TP_ARGS(irq_pending),
	TP_STRUCT__entry(
		__field(u8, irq_pending)
	),
	TP_fast_assign(
		__entry->irq_pending = irq_pending;
	),
	TP_printk("LT THREAD: enter pending=0x%02x",
		__entry->irq_pending)
);

/**
 * dprx_lt_phy_cfg - Track PHY configure timing
 * @is_start: true = start, false = end
 * @link_rate: Link rate code
 * @lane_count: Lane count
 * @elapsed_us: Elapsed time in microseconds (only valid when is_start=false)
 * @ret: Return value (only valid when is_start=false)
 */
TRACE_EVENT(dprx_lt_phy_cfg,
	TP_PROTO(bool is_start, u8 link_rate, u8 lane_count, u32 elapsed_us, int ret),
	TP_ARGS(is_start, link_rate, lane_count, elapsed_us, ret),
	TP_STRUCT__entry(
		__field(bool, is_start)
		__field(u8, link_rate)
		__field(u8, lane_count)
		__field(u32, elapsed_us)
		__field(int, ret)
	),
	TP_fast_assign(
		__entry->is_start = is_start;
		__entry->link_rate = link_rate;
		__entry->lane_count = lane_count;
		__entry->elapsed_us = elapsed_us;
		__entry->ret = ret;
	),
	TP_printk("LT PHY CFG: %s rate=0x%02x lanes=%u %uus%s",
		__entry->is_start ? "START" : "END",
		__entry->link_rate, __entry->lane_count,
		__entry->elapsed_us,
		__entry->is_start ? "" :
			(__entry->ret ? " FAILED" : " OK"))
);

/**
 * dprx_phy_ops - Track PHY operation timing
 * @op_name: Operation name ("init", "configure", "calibrate")
 * @elapsed_us: Elapsed time in microseconds
 * @ret: Return value from PHY operation
 */
TRACE_EVENT(dprx_phy_ops,
	TP_PROTO(const char *op_name, u32 elapsed_us, int ret),
	TP_ARGS(op_name, elapsed_us, ret),
	TP_STRUCT__entry(
		__string(op_name, op_name)
		__field(u32, elapsed_us)
		__field(int, ret)
	),
	TP_fast_assign(
		__assign_str(op_name);
		__entry->elapsed_us = elapsed_us;
		__entry->ret = ret;
	),
	TP_printk("PHY %s: %uus ret=%d",
		__get_str(op_name), __entry->elapsed_us, __entry->ret)
);

/**
 * dprx_lt_phy_lane_remap - Track physical-to-logical DP lane remapping
 * @typec_flipped: Type-C cable orientation (false=normal/0x4E, true=flipped/0xB1)
 * @phy_mask: Physical lane mask before remapping (from EQ_CRC register)
 * @dp_mask: Logical DP lane mask after remapping
 */
TRACE_EVENT(dprx_lt_phy_lane_remap,
	TP_PROTO(bool typec_flipped, u8 phy_mask, u8 dp_mask),
	TP_ARGS(typec_flipped, phy_mask, dp_mask),
	TP_STRUCT__entry(
		__field(bool, typec_flipped)
		__field(u8, phy_mask)
		__field(u8, dp_mask)
	),
	TP_fast_assign(
		__entry->typec_flipped = typec_flipped;
		__entry->phy_mask = phy_mask;
		__entry->dp_mask = dp_mask;
	),
	TP_printk("PHY LANE REMAP: %s phy=0x%x -> dp=0x%x",
		__entry->typec_flipped ? "flipped(0xB1)" : "normal(0x4E)",
		__entry->phy_mask, __entry->dp_mask)
);

#endif /* _RTK_DPRX_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH trace/events
#define TRACE_INCLUDE_FILE rtk_dprx_trace
#include <trace/define_trace.h>
