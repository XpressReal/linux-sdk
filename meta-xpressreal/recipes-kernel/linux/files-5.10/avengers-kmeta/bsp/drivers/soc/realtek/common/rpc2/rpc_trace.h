/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Realtek RPC driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef _RTK_RPC_TRACE_H
#define _RTK_RPC_TRACE_H

#include <trace/events/rtk_rpc.h>

static inline void peek_rpc_struct(const char *func,
	    struct rpc_record_mapping *record, int num)
{
	struct rpc_struct rpc;
	uint32_t pid;
	uint32_t arg;
	uint32_t out;
	bool big_endian = record->big_endian;

	if (rpc_ringbuf_get_ring_data_size(record) < sizeof(struct rpc_struct))
		return;

	out = rpc_ringbuf_get_ringOut(record);
	out = rpc_ringbuf_reading_data(record,
		    out, (char *)&rpc, sizeof(struct rpc_struct));

	if (out == 0)
		return;
	rpc_struct_convert_from_fw(&rpc, big_endian);
	show_rpc_struct(func, &rpc);

	if (rpc.programID == AUDIO_AGENT && rpc.versionID == 0) {
		//Parse more information here
	} else if (rpc.programID == VIDEO_AGENT && rpc.versionID == 0) {
		//Parse more information here
	} else if (rpc.programID == R_PROGRAM && rpc.versionID == 0) {
		out = rpc_ringbuf_reading_data(record,
			    out, (char *)&arg, sizeof(uint32_t));
		if (out == 0)
			return;

		arg = FW2SCPU(big_endian, arg);
		if (rpc.procedureID == 1)
			pr_debug("%s: alloc %u bytes\n", func, arg);
		else
			pr_debug("%s: free addr %x\n", func, arg);
	} else if (rpc.programID == REPLYID && rpc.versionID == REPLYID) {
		out = rpc_ringbuf_reading_data(record,
			    out, (char *)&pid, sizeof(uint32_t));
		if (out == 0)
			return;

		pid = FW2SCPU(big_endian, pid);
		pr_debug("%s: reply to taskid:%u\n", func, pid);
	}

	if (rpc.programID == REPLYID && rpc.versionID == REPLYID)
		trace_rtk_rpc_peek_rpc_reply((struct rpc_struct_tp *)&rpc,
				(u32)refclk_get_val_raw(), num, pid, false);
	else
		trace_rtk_rpc_peek_rpc_request((struct rpc_struct_tp *)&rpc,
				(u32)refclk_get_val_raw(), num, rpc.taskID, false);
}

#endif /* _RTK_RPC_TRACE_H */
