/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __SOC_RTK_GPU_CACHE_H__
#define __SOC_RTK_GPU_CACHE_H__

#include <linux/miscdevice.h>
#include <linux/types.h>
#include <soc/realtek/uapi/rtk_gpu_cache.h>

struct rtk_gpu_cache_reg_frame_offset {
	u32 en_offset;
	u32 en_bit;
	u32 adr_begin_offset;
	u32 adr_end_offset;
	u32 payload_offset;
	u32 header_offset;
};

struct rtk_gpu_cache_reg_frame {
	struct rtk_gpu_cache_reg_frame_offset y;
	struct rtk_gpu_cache_reg_frame_offset c;
};

struct rtk_gpu_cache_reg_frame_info {
	u32 decomp_pitch_offset;
	u8 decomp_header_pitch_msb;
	u8 decomp_header_pitch_lsb;
	u8 decomp_payload_pitch_msb;
	u8 decomp_payload_pitch_lsb;
	u32 gpu_ip_pitch_offset;
	u8 gpu_ip_pitch_msb;
	u8 gpu_ip_pitch_lsb;
	u32 pic_height_offset;
	u8 pic_height_msb;
	u8 pic_height_lsb;
};

struct rtk_gpu_cache_reg_field {
	u32 offset;
	u8 msb;
	u8 lsb;
};

struct rtk_gpu_cache_param {
	int is_const_val;
	union {
		struct rtk_gpu_cache_reg_field reg;
		u32 const_val;
	};
};

#define DEFINE_GPU_CACHE_PARAM_REG_FIELD(_n, _reg, _msb, _lsb) \
	[GPU_CACHE_PARAM_ID_ ##_n] = { .reg = { _reg, _msb, _lsb } }
#define DEFINE_GPU_CACHE_PARAM_CONST(_n, _val) \
	[GPU_CACHE_PARAM_ID_ ##_n] = {.is_const_val = 1, .const_val = _val }

struct rtk_gpu_cache_desc {
	const struct rtk_gpu_cache_reg_frame *reg_frames;
	u32 num_frames;
	const struct rtk_gpu_cache_reg_frame_info *reg_frame_info;
	u32 num_frame_info;
	const struct rtk_gpu_cache_param *params;
	u32 num_params;
	u32 reg_decomp_offset;
	u32 addr_shift;
	u32 support_max_32gb_addr;
};

#define REG_DECOMP_STATUS          0x0
#define REG_DECOMP_BYPASS_EN       0x4
#define REG_DECOMP_CORE            0x8
#define REG_DECOMP_CORE_IRQ        0xc

extern const struct rtk_gpu_cache_desc rtd1319d_desc;
extern const struct rtk_gpu_cache_desc rtd1315e_desc;
extern const struct rtk_gpu_cache_desc rtd1625_desc;

/* driver */
struct rtk_gpu_cache_data;

struct gpu_cache_context {
};

struct gpu_cache_set_frame_args {
	u32 index;
	u32 type;
	u64 adr_begin;
	u64 adr_end;
	u64 header;
	u64 payload;
};

struct gpu_cache_set_frame_info_args {
	u32 decomp_payload_pitch;
	u32 decomp_header_pitch;
	u32 gpu_ip_pitch;
	u32 index;
	u32 pic_height;
};

struct gpu_cache_request_frame_args {
	u32 check_prev_adr;
	u64 y_adr_begin;
	u64 y_adr_end;
	u64 c_adr_begin;
	u64 c_adr_end;
};

struct device *rtk_gpu_cache_dev(struct rtk_gpu_cache_data *data);
struct rtk_gpu_cache_data *rtk_gpu_cache_mdev_to_data(struct miscdevice *mdev);

void rtk_gpu_cache_dev_get(struct rtk_gpu_cache_data *data);
void rtk_gpu_cache_dev_put(struct rtk_gpu_cache_data *data);
void rtk_gpu_cache_lock(struct rtk_gpu_cache_data *data);
void rtk_gpu_cache_unlock(struct rtk_gpu_cache_data *data);

int rtk_gpu_cache_set_frame(struct rtk_gpu_cache_data *data, struct gpu_cache_context *context,
			    struct gpu_cache_set_frame_args *x);
int rtk_gpu_cache_clear_frame(struct rtk_gpu_cache_data *data, struct gpu_cache_context *context,
			      u32 index, u32 type);
int rtk_gpu_cache_set_frame_info(struct rtk_gpu_cache_data *data, struct gpu_cache_context *context,
				 struct gpu_cache_set_frame_info_args *x);
int rtk_gpu_cache_set_param(struct rtk_gpu_cache_data *data, u32 id, u32 value);
int rtk_gpu_cache_get_param(struct rtk_gpu_cache_data *data, u32 id, u32 *value);
int rtk_gpu_cache_request_frame(struct rtk_gpu_cache_data *data, struct gpu_cache_context *context,
			        struct gpu_cache_request_frame_args *x);
int rtk_gpu_cache_release_frame(struct rtk_gpu_cache_data *data, struct gpu_cache_context *context,
				u32 i);
void rtk_gpu_cache_release_all_frames_by_context(struct rtk_gpu_cache_data *data,
						 struct gpu_cache_context *context);
int rtk_gpu_cache_flush(struct rtk_gpu_cache_data *data);

extern const struct file_operations rtk_gpu_cache_fops;

struct rtk_gpu_cache_data *rtk_gpu_cache_data_get(void);
static inline void rtk_gpu_cache_data_put(struct rtk_gpu_cache_data * data)
{}

#endif /* __SOC_RTK_GPU_CACHE_H__ */
