/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#ifndef __RTK_VCODEC_H264_H__
#define __RTK_VCODEC_H264_H__

int rtk_vcodec_h264_fill_nal(int size, char *p);
int rtk_vcodec_h264_padding(int size, char *p);
int rtk_vcodec_h264_profile(int profile_idc);
int rtk_vcodec_h264_level(int level_idc);
int rtk_vcodec_h264_parse_sps(struct rtk_vcodec_ctx *ctx, struct vb2_buffer *vb);
// int coda_h264_sps_fixup(struct rtk_vcodec_ctx *ctx, int width, int height, char *buf,
// 			int *size, int max_size);

#endif /* __RTK_VCODEC_H264_H__ */
