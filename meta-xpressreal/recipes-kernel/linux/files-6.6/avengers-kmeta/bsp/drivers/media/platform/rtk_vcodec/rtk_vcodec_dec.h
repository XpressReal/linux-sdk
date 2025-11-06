/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#ifndef _RTK_VCODEC_DEC_H_
#define _RTK_VCODEC_DEC_H_

#include <linux/debugfs.h>
#include <linux/idr.h>
#include <linux/irqreturn.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/videodev2.h>
#include <linux/ratelimit.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-v4l2.h>

#include "rtk_vcodec_regs.h"

#define	RTK_REG_BUSY_ENABLE 1

#define RTK_BIT_STREAM_END_FLAG    (1 << 2)
#define RTK_BIT_STREAM_PICEND_MODE (1 << 4)

enum rtk_aux_std {
	RTK_AUX_STD_H264  = 0,
	RTK_AUX_STD_MPEG4 = 0,
	RTK_AUX_STD_HEVC  = 1,
	RTK_AUX_STD_DVIX3 = 1,
	RTK_AUX_STD_VP8   = 2,
};

void rtk_vcodec_write(struct rtk_vcodec_dev *dev, u32 data, u32 reg);
unsigned int rtk_vcodec_read(struct rtk_vcodec_dev *dev, u32 reg);
void rtk_vcodec_write_base(struct rtk_vcodec_ctx *ctx, struct rtk_q_data *q_data,
		     struct vb2_v4l2_buffer *buf, unsigned int reg_y);

void rtk_bitstream_end_flag(struct rtk_vcodec_ctx *ctx);
int rtk_wait_timeout(struct rtk_vcodec_dev *dev);
unsigned long rtk_dec_isbusy(struct rtk_vcodec_dev *dev);

int rtk_decoder_queue_init(void *priv, struct vb2_queue *src_vq,
			    struct vb2_queue *dst_vq);

#endif /* _RTK_VCODEC_DEC_H_ */
