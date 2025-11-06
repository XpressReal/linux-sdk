/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/videodev2.h>

#include "rtk_vcodec_drv.h"
#include "rtk_vcodec_vp8.h"

static const u32 ivf_header_signature = v4l2_fourcc('D', 'K', 'I', 'F');
static const u32 ivf_header_fourcc = v4l2_fourcc('V', 'P', '8', '0');

void rtk_vcodec_vp8_fill_file_header(struct rtk_vcodec_ctx *ctx,
						struct ivf_file_header *header)
{
	struct rtk_q_data *q_data_src;
	q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);

	rtk_vcodec_dbg(1, ctx, "VP8 fill file header, wxh(%dx%d) \n",
		q_data_src->ori_width, q_data_src->ori_height);

	header->signature     = ivf_header_signature;
	header->version       = 0x0;
	header->header_length = 0x20;
	header->fourcc        = ivf_header_fourcc;
	header->width         = q_data_src->ori_width;
	header->height        = q_data_src->ori_height;
	header->denominator   = 30000;
	header->numerator     = 1000;
	header->frame_cnt     = 0;
	header->unused        = 0;
}


void rtk_vcodec_vp8_fill_frame_header(struct rtk_vcodec_ctx *ctx,
				struct ivf_frame_header *header, u32 payload)
{
	rtk_vcodec_dbg(1, ctx, "VP8 fill frame header, payload(%d) \n", payload);

	header->size = payload;
	header->timestamp = 0;
}
