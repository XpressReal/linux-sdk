// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
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
#include <linux/tracepoint.h>

#define CREATE_TRACE_POINTS
#include <trace/events/rtk_rpc.h>

EXPORT_TRACEPOINT_SYMBOL_GPL(rtk_rpc_peek_rpc_request);
EXPORT_TRACEPOINT_SYMBOL_GPL(rtk_rpc_peek_rpc_reply);
