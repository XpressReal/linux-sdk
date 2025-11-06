/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __SOC_RTK_GPU_CACHE_CONTROL_H__
#define __SOC_RTK_GPU_CACHE_CONTROL_H__

#include <soc/realtek/uapi/rtk_gpu_cache.h>

struct rtk_gpu_cache_control;

#if IS_ENABLED(CONFIG_RTK_GPU_CACHE_CONTROL)
struct rtk_gpu_cache_control *rtk_gpu_cache_control_create(void);
void rtk_gpu_cache_control_destroy(struct rtk_gpu_cache_control *control);
int rtk_gpu_cache_control_set_frame(struct rtk_gpu_cache_control *control,
				    u32 index, u32 type, dma_addr_t adr_begin, dma_addr_t adr_end,
				    dma_addr_t header, dma_addr_t payload);
int rtk_gpu_cache_control_clear_frame(struct rtk_gpu_cache_control *control, u32 index, u32 type);
int rtk_gpu_cache_control_set_frame_info(struct rtk_gpu_cache_control *control, u32 index,
					 u32 decomp_payload_pitch, u32 decomp_header_pitch,
					 u32 gpu_ip_pitch, u32 pic_height);
int rtk_gpu_cache_control_set_param(struct rtk_gpu_cache_control *control, u32 id, u32 value);
int rtk_gpu_cache_control_get_param(struct rtk_gpu_cache_control *control, u32 id, u32 *value);
int rtk_gpu_cache_control_request_frame(struct rtk_gpu_cache_control *control);
int rtk_gpu_cache_control_request_frame2(struct rtk_gpu_cache_control *control,
					 dma_addr_t prev_y_adr_begin, dma_addr_t prev_y_adr_end,
					 dma_addr_t prev_c_adr_begin, dma_addr_t prev_c_adr_end);
int rtk_gpu_cache_control_release_frame(struct rtk_gpu_cache_control *control, u32 i);
int rtk_gpu_cache_control_flush(struct rtk_gpu_cache_control *control);
#endif

#endif /* __SOC_RTK_GPU_CACHE_CONTROL_H__ */
