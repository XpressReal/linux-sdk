// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/kernel.h>
#include "rtk-gpu_cache_internal.h"

#define RTD1319D_GPU_CACHE_ENABLE            0x444
#define RTD1319D_GPU_CACHE_Y_ADR0_BEGIN      0x448
#define RTD1319D_GPU_CACHE_Y_ADR0_END        0x478
#define RTD1319D_GPU_CACHE_C_ADR0_BEGIN      0x4a8
#define RTD1319D_GPU_CACHE_C_ADR0_END        0x4d8
#define RTD1319D_GPU_CACHE_Y_HEADER0         0x508
#define RTD1319D_GPU_CACHE_C_HEADER0         0x538
#define RTD1319D_GPU_CACHE_Y_PAYLOAD0        0x568
#define RTD1319D_GPU_CACHE_C_PAYLOAD0        0x598
#define RTD1319D_GPU_DECOMP_PITCH0           0x5c8
#define RTD1319D_GPU_DECOMP_PITCH1           0x5cc
#define RTD1319D_REG_DECOMP_STATUS           0x5d0
#define RTD1319D_REG_DECOMP_BYPASS_EN        0x5d4
#define RTD1319D_REG_DECOMP_CORE             0x5d8
#define RTD1319D_REG_DECOMP_CORE_IRQ         0x5dc

#define RTD1319D_GPU_CACHE_Y_ADRn_BEGIN(n)  (RTD1319D_GPU_CACHE_Y_ADR0_BEGIN + (n) * 4)
#define RTD1319D_GPU_CACHE_Y_ADRn_END(n)    (RTD1319D_GPU_CACHE_Y_ADR0_END + (n) * 4)
#define RTD1319D_GPU_CACHE_C_ADRn_BEGIN(n)  (RTD1319D_GPU_CACHE_C_ADR0_BEGIN + (n) * 4)
#define RTD1319D_GPU_CACHE_C_ADRn_END(n)    (RTD1319D_GPU_CACHE_C_ADR0_END + (n) * 4)
#define RTD1319D_GPU_CACHE_Y_HEADER(n)      (RTD1319D_GPU_CACHE_Y_HEADER0 + (n) * 4)
#define RTD1319D_GPU_CACHE_C_HEADER(n)      (RTD1319D_GPU_CACHE_C_HEADER0 + (n) * 4)
#define RTD1319D_GPU_CACHE_Y_PAYLOAD(n)     (RTD1319D_GPU_CACHE_Y_PAYLOAD0 + (n) * 4)
#define RTD1319D_GPU_CACHE_C_PAYLOAD(n)     (RTD1319D_GPU_CACHE_C_PAYLOAD0 + (n) * 4)

#define DEFINE_RTD1319D_GPU_CACHE_ADDR(_i)                               \
{                                                                        \
	.y = {                                                           \
		.en_offset        = RTD1319D_GPU_CACHE_ENABLE,           \
		.en_bit           = 11 - (_i),                           \
		.adr_begin_offset = RTD1319D_GPU_CACHE_Y_ADRn_BEGIN(_i), \
		.adr_end_offset   = RTD1319D_GPU_CACHE_Y_ADRn_END(_i),   \
		.header_offset    = RTD1319D_GPU_CACHE_Y_HEADER(_i),     \
		.payload_offset   = RTD1319D_GPU_CACHE_Y_PAYLOAD(_i),    \
	},                                                               \
	.c = {                                                           \
		.en_offset        = RTD1319D_GPU_CACHE_ENABLE,           \
		.en_bit           = 23 - (_i),                           \
		.adr_begin_offset = RTD1319D_GPU_CACHE_C_ADRn_BEGIN(_i), \
		.adr_end_offset   = RTD1319D_GPU_CACHE_C_ADRn_END(_i),   \
		.header_offset    = RTD1319D_GPU_CACHE_C_HEADER(_i),     \
		.payload_offset   = RTD1319D_GPU_CACHE_C_PAYLOAD(_i),    \
	},                                                               \
}

static const struct rtk_gpu_cache_reg_frame rtd1319d_reg_frames[] = {
	DEFINE_RTD1319D_GPU_CACHE_ADDR(0),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(1),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(2),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(3),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(4),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(5),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(6),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(7),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(8),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(9),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(10),
	DEFINE_RTD1319D_GPU_CACHE_ADDR(11),
};

static const struct rtk_gpu_cache_reg_frame_info rtd1319d_reg_frame_info[] = {
	{
		.decomp_pitch_offset = RTD1319D_GPU_DECOMP_PITCH0,
		.decomp_header_pitch_msb = 31,
		.decomp_header_pitch_lsb = 16,
		.decomp_payload_pitch_msb = 15,
		.decomp_payload_pitch_lsb = 0,
		.gpu_ip_pitch_offset = RTD1319D_GPU_DECOMP_PITCH1,
		.gpu_ip_pitch_msb = 7,
		.gpu_ip_pitch_lsb = 0,
	}
};

static const struct rtk_gpu_cache_param rtd1319d_params[] = {
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(QLEVEL_QUEUE_SEL_Y, RTD1319D_REG_DECOMP_CORE, 1, 0),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(QLEVEL_QUEUE_SEL_C, RTD1319D_REG_DECOMP_CORE, 2, 2),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(LOSSY_EN, RTD1319D_REG_DECOMP_CORE, 3, 3),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(DECOMP_BPP, RTD1319D_REG_DECOMP_CORE, 22, 21),
	DEFINE_GPU_CACHE_PARAM_CONST(MAX_FRAMES, ARRAY_SIZE(rtd1319d_reg_frames)),
	DEFINE_GPU_CACHE_PARAM_CONST(SHARED_FRAME_INFO, 1),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(GPU_DECOMP_CBCR_SEL, RTD1319D_GPU_DECOMP_PITCH1, 9, 9),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(DDR_HEADER_SWAP, RTD1319D_GPU_DECOMP_PITCH1, 10, 10),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(DDR_PAYLOAD_SWAP, RTD1319D_GPU_DECOMP_PITCH1, 11, 11),
};

const struct rtk_gpu_cache_desc rtd1319d_desc = {
	.reg_frames = rtd1319d_reg_frames,
	.num_frames = ARRAY_SIZE(rtd1319d_reg_frames),
	.reg_frame_info = rtd1319d_reg_frame_info,
	.num_frame_info = ARRAY_SIZE(rtd1319d_reg_frame_info),
	.params = rtd1319d_params,
	.num_params = ARRAY_SIZE(rtd1319d_params),
	.reg_decomp_offset = RTD1319D_REG_DECOMP_STATUS,
};
