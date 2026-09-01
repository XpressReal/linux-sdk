// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022-2025 Realtek Semiconductor Corp.
 * Author: Cheng-Yu Lee <cylee12@realtek.com>
 */

#define pr_fmt(fmt) "gpu_cache: " fmt

#include <linux/device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include "rtk-gpu_cache_internal.h"

struct rtk_gpu_cache_control {
	struct gpu_cache_context context;
	struct rtk_gpu_cache_data *data;
};

struct rtk_gpu_cache_control *rtk_gpu_cache_control_create(void)
{
	struct rtk_gpu_cache_control *control;

	control = kzalloc(sizeof(*control), GFP_KERNEL);
	if (!control)
		return NULL;

	control->data = rtk_gpu_cache_data_get();
	if (IS_ERR(control->data)) {
		kfree(control);
		return NULL;
	}

	rtk_gpu_cache_dev_get(control->data);
	return control;
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_create);

void rtk_gpu_cache_control_destroy(struct rtk_gpu_cache_control *control)
{
	rtk_gpu_cache_dev_put(control->data);
	rtk_gpu_cache_data_put(control->data);
	kfree(control);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_destroy);

int rtk_gpu_cache_control_request_frame(struct rtk_gpu_cache_control *control)
{
	struct gpu_cache_request_frame_args x = {};

	return rtk_gpu_cache_request_frame(control->data, &control->context, &x);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_request_frame);

int rtk_gpu_cache_control_request_frame2(struct rtk_gpu_cache_control *control,
					 dma_addr_t prev_y_adr_begin, dma_addr_t prev_y_adr_end,
					 dma_addr_t prev_c_adr_begin, dma_addr_t prev_c_adr_end)
{
	struct gpu_cache_request_frame_args x = {
		.check_prev_adr = 1,
		.y_adr_begin = prev_y_adr_begin,
		.y_adr_end = prev_y_adr_end,
		.c_adr_begin = prev_c_adr_begin,
		.c_adr_end = prev_c_adr_end,
	};

	return rtk_gpu_cache_request_frame(control->data, &control->context, &x);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_request_frame2);

int rtk_gpu_cache_control_release_frame(struct rtk_gpu_cache_control *control, u32 i)
{
	 return rtk_gpu_cache_release_frame(control->data, &control->context, i);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_release_frame);

int rtk_gpu_cache_control_set_frame(struct rtk_gpu_cache_control *control,
				    u32 index, u32 type, dma_addr_t adr_begin, dma_addr_t adr_end,
				    dma_addr_t header, dma_addr_t payload)
{
	struct gpu_cache_set_frame_args x = {
		.index = index,
		.type = type,
		.adr_begin = adr_begin,
		.adr_end = adr_end,
		.header = header,
		.payload = payload,
	};

	return rtk_gpu_cache_set_frame(control->data, &control->context, &x);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_set_frame);

int rtk_gpu_cache_control_clear_frame(struct rtk_gpu_cache_control *control, u32 index, u32 type)
{
	return rtk_gpu_cache_clear_frame(control->data, &control->context, index, type);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_clear_frame);

int rtk_gpu_cache_control_set_frame_info(struct rtk_gpu_cache_control *control, u32 index,
					 u32 decomp_payload_pitch, u32 decomp_header_pitch,
					 u32 gpu_ip_pitch, u32 pic_height)
{
	struct gpu_cache_set_frame_info_args x = {
		.decomp_payload_pitch = decomp_payload_pitch,
		.decomp_header_pitch = decomp_header_pitch,
		.gpu_ip_pitch = gpu_ip_pitch,
		.index = index,
		.pic_height = pic_height,
	};

	return rtk_gpu_cache_set_frame_info(control->data, &control->context, &x);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_set_frame_info);

int rtk_gpu_cache_control_set_param(struct rtk_gpu_cache_control *control, u32 id, u32 value)
{
	return rtk_gpu_cache_set_param(control->data, id, value);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_set_param);

int rtk_gpu_cache_control_get_param(struct rtk_gpu_cache_control *control, u32 id, u32 *value)
{

	return rtk_gpu_cache_get_param(control->data, id, value);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_get_param);

int rtk_gpu_cache_control_flush(struct rtk_gpu_cache_control *control)
{
	return rtk_gpu_cache_flush(control->data);
}
EXPORT_SYMBOL_GPL(rtk_gpu_cache_control_flush);
