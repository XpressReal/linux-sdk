// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/kernel.h>
#include "rtk-gpu_cache_internal.h"

#define RTD1625_GPU_CACHE_ENABLE0        0x300
#define RTD1625_GPU_CACHE_ENABLE1        0x304
#define RTD1625_GPU_CACHE_Y_ADR0_BEGIN   0x308
#define RTD1625_GPU_CACHE_Y_ADR0_END     0x368
#define RTD1625_GPU_CACHE_C_ADR0_BEGIN   0x3c8
#define RTD1625_GPU_CACHE_C_ADR0_END     0x428
#define RTD1625_GPU_CACHE_Y_HEADER0      0x488
#define RTD1625_GPU_CACHE_C_HEADER0      0x4e8
#define RTD1625_GPU_CACHE_Y_PAYLOAD0     0x548
#define RTD1625_GPU_CACHE_C_PAYLOAD0     0x5a8
#define RTD1625_GPU_DECOMP_PITCH0        0x608
#define RTD1625_GPU_CACHE_FRAME_INFO0    0x668
#define RTD1625_GPU_CACHE_CTRL           0x6c8
#define RTD1625_REG_DECOMP_STATUS        0x6cc
#define RTD1625_REG_DECOMP_BYPASS_EN     0x6d0
#define RTD1625_REG_DECOMP_CORE          0x6d4
#define RTD1625_REG_DECOMP_CORE_IRQ      0x6d8

#define RTD1625_GPU_CACHE_Y_ADRn_BEGIN(n)  (RTD1625_GPU_CACHE_Y_ADR0_BEGIN + (n) * 4)
#define RTD1625_GPU_CACHE_Y_ADRn_END(n)    (RTD1625_GPU_CACHE_Y_ADR0_END + (n) * 4)
#define RTD1625_GPU_CACHE_C_ADRn_BEGIN(n)  (RTD1625_GPU_CACHE_C_ADR0_BEGIN + (n) * 4)
#define RTD1625_GPU_CACHE_C_ADRn_END(n)    (RTD1625_GPU_CACHE_C_ADR0_END + (n) * 4)
#define RTD1625_GPU_CACHE_Y_HEADER(n)      (RTD1625_GPU_CACHE_Y_HEADER0 + (n) * 4)
#define RTD1625_GPU_CACHE_C_HEADER(n)      (RTD1625_GPU_CACHE_C_HEADER0 + (n) * 4)
#define RTD1625_GPU_CACHE_Y_PAYLOAD(n)     (RTD1625_GPU_CACHE_Y_PAYLOAD0 + (n) * 4)
#define RTD1625_GPU_CACHE_C_PAYLOAD(n)     (RTD1625_GPU_CACHE_C_PAYLOAD0 + (n) * 4)
#define RTD1625_GPU_DECOMP_PITCH(n)        (RTD1625_GPU_DECOMP_PITCH0 + (n) * 4)
#define RTD1625_GPU_CACHE_FRAME_INFO(n)    (RTD1625_GPU_CACHE_FRAME_INFO0 + (n) * 4)


#define DEFINE_RTD1625_GPU_CACHE_ADDR(_i)                               \
{                                                                       \
	.y = {                                                          \
		.en_offset        = RTD1625_GPU_CACHE_ENABLE0,          \
		.en_bit           = (_i),                               \
		.adr_begin_offset = RTD1625_GPU_CACHE_Y_ADRn_BEGIN(_i), \
		.adr_end_offset   = RTD1625_GPU_CACHE_Y_ADRn_END(_i),   \
		.header_offset    = RTD1625_GPU_CACHE_Y_HEADER(_i),     \
		.payload_offset   = RTD1625_GPU_CACHE_Y_PAYLOAD(_i),    \
	},                                                              \
	.c = {                                                          \
		.en_offset        = RTD1625_GPU_CACHE_ENABLE1,          \
		.en_bit           = (_i),                               \
		.adr_begin_offset = RTD1625_GPU_CACHE_C_ADRn_BEGIN(_i), \
		.adr_end_offset   = RTD1625_GPU_CACHE_C_ADRn_END(_i),   \
		.header_offset    = RTD1625_GPU_CACHE_C_HEADER(_i),     \
		.payload_offset   = RTD1625_GPU_CACHE_C_PAYLOAD(_i),    \
	},                                                              \
}

static const struct rtk_gpu_cache_reg_frame rtd1625_reg_frames[] = {
	DEFINE_RTD1625_GPU_CACHE_ADDR(0),
	DEFINE_RTD1625_GPU_CACHE_ADDR(1),
	DEFINE_RTD1625_GPU_CACHE_ADDR(2),
	DEFINE_RTD1625_GPU_CACHE_ADDR(3),
	DEFINE_RTD1625_GPU_CACHE_ADDR(4),
	DEFINE_RTD1625_GPU_CACHE_ADDR(5),
	DEFINE_RTD1625_GPU_CACHE_ADDR(6),
	DEFINE_RTD1625_GPU_CACHE_ADDR(7),
	DEFINE_RTD1625_GPU_CACHE_ADDR(8),
	DEFINE_RTD1625_GPU_CACHE_ADDR(9),
	DEFINE_RTD1625_GPU_CACHE_ADDR(10),
	DEFINE_RTD1625_GPU_CACHE_ADDR(11),
	DEFINE_RTD1625_GPU_CACHE_ADDR(12),
	DEFINE_RTD1625_GPU_CACHE_ADDR(13),
	DEFINE_RTD1625_GPU_CACHE_ADDR(14),
	DEFINE_RTD1625_GPU_CACHE_ADDR(15),
	DEFINE_RTD1625_GPU_CACHE_ADDR(16),
	DEFINE_RTD1625_GPU_CACHE_ADDR(17),
	DEFINE_RTD1625_GPU_CACHE_ADDR(18),
	DEFINE_RTD1625_GPU_CACHE_ADDR(19),
	DEFINE_RTD1625_GPU_CACHE_ADDR(20),
	DEFINE_RTD1625_GPU_CACHE_ADDR(21),
	DEFINE_RTD1625_GPU_CACHE_ADDR(22),
	DEFINE_RTD1625_GPU_CACHE_ADDR(23),
};

#define DEFINE_RTD1625_GPU_FRAME_INFO(_i)                        \
{                                                                \
	.decomp_pitch_offset = RTD1625_GPU_DECOMP_PITCH(_i),     \
	.decomp_header_pitch_msb = 31,                           \
	.decomp_header_pitch_lsb = 16,                           \
	.decomp_payload_pitch_msb = 15,                          \
	.decomp_payload_pitch_lsb = 0,                           \
	.gpu_ip_pitch_offset = RTD1625_GPU_CACHE_FRAME_INFO(_i), \
	.gpu_ip_pitch_msb = 31,                                  \
	.gpu_ip_pitch_lsb = 16,                                  \
	.pic_height_offset = RTD1625_GPU_CACHE_FRAME_INFO(_i),   \
	.pic_height_msb = 15,                                    \
	.pic_height_lsb = 0,                                     \
}

static const struct rtk_gpu_cache_reg_frame_info rtd1625_reg_frame_info[] = {
	DEFINE_RTD1625_GPU_FRAME_INFO(0),
	DEFINE_RTD1625_GPU_FRAME_INFO(1),
	DEFINE_RTD1625_GPU_FRAME_INFO(2),
	DEFINE_RTD1625_GPU_FRAME_INFO(3),
	DEFINE_RTD1625_GPU_FRAME_INFO(4),
	DEFINE_RTD1625_GPU_FRAME_INFO(5),
	DEFINE_RTD1625_GPU_FRAME_INFO(6),
	DEFINE_RTD1625_GPU_FRAME_INFO(7),
	DEFINE_RTD1625_GPU_FRAME_INFO(8),
	DEFINE_RTD1625_GPU_FRAME_INFO(9),
	DEFINE_RTD1625_GPU_FRAME_INFO(10),
	DEFINE_RTD1625_GPU_FRAME_INFO(11),
	DEFINE_RTD1625_GPU_FRAME_INFO(12),
	DEFINE_RTD1625_GPU_FRAME_INFO(13),
	DEFINE_RTD1625_GPU_FRAME_INFO(14),
	DEFINE_RTD1625_GPU_FRAME_INFO(15),
	DEFINE_RTD1625_GPU_FRAME_INFO(16),
	DEFINE_RTD1625_GPU_FRAME_INFO(17),
	DEFINE_RTD1625_GPU_FRAME_INFO(18),
	DEFINE_RTD1625_GPU_FRAME_INFO(19),
	DEFINE_RTD1625_GPU_FRAME_INFO(20),
	DEFINE_RTD1625_GPU_FRAME_INFO(21),
	DEFINE_RTD1625_GPU_FRAME_INFO(22),
	DEFINE_RTD1625_GPU_FRAME_INFO(23),
};

static const struct rtk_gpu_cache_param rtd1625_params[] = {
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(QLEVEL_QUEUE_SEL_Y, RTD1625_REG_DECOMP_CORE, 1, 0),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(QLEVEL_QUEUE_SEL_C, RTD1625_REG_DECOMP_CORE, 2, 2),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(LOSSY_EN, RTD1625_REG_DECOMP_CORE, 3, 3),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(DECOMP_BPP, RTD1625_REG_DECOMP_CORE, 22, 21),
	DEFINE_GPU_CACHE_PARAM_CONST(MAX_FRAMES, ARRAY_SIZE(rtd1625_reg_frames)),
	DEFINE_GPU_CACHE_PARAM_CONST(SHARED_FRAME_INFO, 0),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(NEW_PACKING, RTD1625_GPU_CACHE_CTRL, 18, 18),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(GPU_DECOMP_CBCR_SEL, RTD1625_GPU_CACHE_CTRL, 1, 1),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(DDR_HEADER_SWAP, RTD1625_GPU_CACHE_CTRL, 2, 2),
	DEFINE_GPU_CACHE_PARAM_REG_FIELD(DDR_PAYLOAD_SWAP, RTD1625_GPU_CACHE_CTRL, 3, 3),
};

const struct rtk_gpu_cache_desc rtd1625_desc = {
	.reg_frames = rtd1625_reg_frames,
	.num_frames = ARRAY_SIZE(rtd1625_reg_frames),
	.reg_frame_info = rtd1625_reg_frame_info,
	.num_frame_info = ARRAY_SIZE(rtd1625_reg_frame_info),
	.params = rtd1625_params,
	.num_params = ARRAY_SIZE(rtd1625_params),
	.reg_decomp_offset = RTD1625_REG_DECOMP_STATUS,
	.addr_shift = 3,
	.support_max_32gb_addr = 1,
};
