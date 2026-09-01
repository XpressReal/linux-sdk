// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Realtek RPC driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */
//#define DEBUG
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <soc/realtek/rtk_refclk.h>

#include "rpc.h"

#define MAX_SUPPORT_CLIENTS 5

extern int acpu_rpc_init(void);
extern int vcpu_rpc_init(void);
extern int ve3_rpc_init(void);
extern int hifi_rpc_init(void);

struct rtk_client_init_call {
	char name[32];
	bool is_added;
	int (*init)(void);
};

struct rtk_client_init_call rtk_client_init[MAX_SUPPORT_CLIENTS] = {
	{.name = "acpu", .is_added = true, .init = &acpu_rpc_init},
	{.name = "vcpu", .is_added = true, .init = &vcpu_rpc_init},
	{.name = "ve3", .is_added = true, .init = &ve3_rpc_init},
	{.name = "hifi", .is_added = true, .init = &hifi_rpc_init},
};

int rtk_rpc_client_init(void)
{
	int i;

	for (i = 0; i < MAX_SUPPORT_CLIENTS; i++) {
		int (*init)(void);

		pr_info("%s index=%d is_added=%s name=%s\n", __func__, i,
			rtk_client_init[i].is_added?"YES":"NO",
			rtk_client_init[i].name);

		init = rtk_client_init[i].init;
		if (init)
			init();
	}

	return 0;
}
