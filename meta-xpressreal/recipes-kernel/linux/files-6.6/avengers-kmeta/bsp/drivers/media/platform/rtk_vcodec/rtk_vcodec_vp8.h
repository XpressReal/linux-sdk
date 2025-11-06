/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#ifndef __RTK_VCODEC_VP8_H__
#define __RTK_VCODEC_VP8_H__

struct ivf_file_header {
	u32 signature;
	u16 version;
	u16 header_length;
	u32 fourcc;
	u16 width;
	u16 height;
	u32 denominator;
	u32 numerator;
	u32 frame_cnt;
	u32 unused;
} __attribute__((packed));

struct ivf_frame_header {
	u32 size;
	u64 timestamp;
} __attribute__((packed));

void rtk_vcodec_vp8_fill_file_header(struct rtk_vcodec_ctx *ctx,
						struct ivf_file_header *header);

void rtk_vcodec_vp8_fill_frame_header(struct rtk_vcodec_ctx *ctx,
				struct ivf_frame_header *header, u32 payload);

#endif /* __RTK_VCODEC_VP8_H__ */
