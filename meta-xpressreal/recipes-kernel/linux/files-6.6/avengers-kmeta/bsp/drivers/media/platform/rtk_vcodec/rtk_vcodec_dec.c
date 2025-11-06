	/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gcd.h>
#include <linux/genalloc.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/of.h>
#include <linux/ratelimit.h>
#include <linux/reset.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-vmalloc.h>

#include <linux/bitops.h>
#include "rtk_vcodec_drv.h"
#include "rtk_vcodec_dec.h"
#include "rtk_vcodec_h264.h"
#include "rtk_vcodec_vp8.h"

#define PARA_BUF_SIZE (10 * 1024)
#define PS_SAVE_SIZE ((320 + 8) * 1024 * 4)
#define SLICE_SAVE_SIZE (4096 * 2304 * 3 / 4) // this buffer for ASO/FMO
#define VP8_MB_SAVE_SIZE (17 * 4 * (4096 * 2304 / 256)) // MB information + split MVs)*4*MbNumbyte

#define RTK_REG_BIT_RD_PTR(x) (BIT_RD_PTR + 8 * (x))
#define RTK_REG_BIT_WR_PTR(x) (BIT_WR_PTR + 8 * (x))
#define RTK_REG_BIT_FRM_DIS_FLG(x) (BIT_FRM_DIS_FLG + 4 * (x))

#define	RTK_FRAME_CHROMA_INTERLEAVE (1 << 2)
#define RTK_FRAME_TILED2LINEAR (1 << 11)
#define	RTK_REORDER_ENABLE (1 << 1)
#define	RTK_NO_INT_ENABLE (1 << 10)
#define	RTK_MP4_CLASS_MPEG4 0

#define	PICWIDTH_OFFSET 16
#define PICWIDTH_MASK 0xffff
#define	PICHEIGHT_MASK 0xffff

#define RTK_COMMAND_SEQ_INIT 1
#define RTK_COMMAND_SEQ_END 2
#define	RTK_COMMAND_PIC_RUN 3
#define RTK_COMMAND_SET_FRAME_BUF 4

#define	RTK_USE_BIT_ENABLE (1 << 0)
#define	RTK_USE_IP_ENABLE  (1 << 1)
#define RTK_USE_DBK_ENABLE (3 << 2)
#define RTK_USE_OVL_ENABLE (1 << 4)
#define RTK_USE_BTP_ENABLE (1 << 5)
#define	RTK_USE_HOST_BIT_ENABLE (1 << 8)
#define	RTK_USE_HOST_IP_ENABLE  (1 << 9)
#define	RTK_USE_HOST_DBK_ENABLE (3 << 10)
#define RTK_USE_HOST_OVL_ENABLE (1 << 12)
#define RTK_USE_HOST_BTP_ENABLE (1 << 13)

#define CODA9_CACHE_PAGEMERGE_OFFSET 24
#define	CODA9_CACHE_LUMA_BUFFER_SIZE_OFFSET	16
#define	CODA9_CACHE_CB_BUFFER_SIZE_OFFSET 8
#define	CODA9_CACHE_CR_BUFFER_SIZE_OFFSET 0

#define CODA_ROT_MIR_ENABLE (1 << 4)

#define	CODA9_XY2RBC_TILED_MAP BIT(17)
#define CODA9_XY2RBC_CA_INC_HOR	BIT(16)

#define RTK_REG_PRODUCT_CODE 0x1044

#define RET_DEC_PIC_NOT_ENOUGH_FRAME_BUFFER (-1)
#define RET_DEC_PIC_NO_DATA (-2)
#define RET_DEC_PIC_NO_DECODE_AND_DISPLAY (-1)
#define RET_DEC_PIC_SKIP_FRAME (-2)
#define RET_DEC_PIC_DISPLAY_DELAY (-3)

/**
 * RTK gdi info
 */

#define XY2_INVERT	BIT(7)
#define XY2_ZERO	BIT(6)
#define XY2_TB_XOR	BIT(5)
#define XY2_XYSEL	BIT(4)
#define XY2_Y		(1 << 4)
#define XY2_X		(0 << 4)

#define XY2(luma_sel, luma_bit, chroma_sel, chroma_bit) \
	(((XY2_##luma_sel) | (luma_bit)) << 8 | \
	 (XY2_##chroma_sel) | (chroma_bit))

static const u16 xy2ca_zero_map[16] = {
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
};

static const u16 xy2ca_tiled_map[16] = {
	XY2(Y,    0, Y,    0),
	XY2(Y,    1, Y,    1),
	XY2(Y,    2, Y,    2),
	XY2(Y,    3, X,    3),
	XY2(X,    3, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
	XY2(ZERO, 0, ZERO, 0),
};

/*
 * RA[15:0], CA[15:8] are hardwired to contain the 24-bit macroblock
 * start offset (macroblock size is 16x16 for luma, 16x8 for chroma).
 * Bits CA[4:0] are set using XY2CA above. BA[3:0] seems to be unused.
 */

#define RBC_CA		(0 << 4)
#define RBC_BA		(1 << 4)
#define RBC_RA		(2 << 4)
#define RBC_ZERO	(3 << 4)

#define RBC(luma_sel, luma_bit, chroma_sel, chroma_bit) \
	(((RBC_##luma_sel) | (luma_bit)) << 6 | \
	 (RBC_##chroma_sel) | (chroma_bit))

static const u16 rbc2axi_tiled_map[32] = {
	RBC(ZERO, 0, ZERO, 0),
	RBC(ZERO, 0, ZERO, 0),
	RBC(ZERO, 0, ZERO, 0),
	RBC(CA,   0, CA,   0),
	RBC(CA,   1, CA,   1),
	RBC(CA,   2, CA,   2),
	RBC(CA,   3, CA,   3),
	RBC(CA,   4, CA,   8),
	RBC(CA,   8, CA,   9),
	RBC(CA,   9, CA,  10),
	RBC(CA,  10, CA,  11),
	RBC(CA,  11, CA,  12),
	RBC(CA,  12, CA,  13),
	RBC(CA,  13, CA,  14),
	RBC(CA,  14, CA,  15),
	RBC(CA,  15, RA,   0),
	RBC(RA,   0, RA,   1),
	RBC(RA,   1, RA,   2),
	RBC(RA,   2, RA,   3),
	RBC(RA,   3, RA,   4),
	RBC(RA,   4, RA,   5),
	RBC(RA,   5, RA,   6),
	RBC(RA,   6, RA,   7),
	RBC(RA,   7, RA,   8),
	RBC(RA,   8, RA,   9),
	RBC(RA,   9, RA,  10),
	RBC(RA,  10, RA,  11),
	RBC(RA,  11, RA,  12),
	RBC(RA,  12, RA,  13),
	RBC(RA,  13, RA,  14),
	RBC(RA,  14, RA,  15),
	RBC(RA,  15, ZERO, 0),
};

static char rtk_frame_type(u32 flags)
{
	return (flags & V4L2_BUF_FLAG_KEYFRAME) ? 'I' :
	       (flags & V4L2_BUF_FLAG_PFRAME) ? 'P' :
	       (flags & V4L2_BUF_FLAG_BFRAME) ? 'B' : '?';
}

/**
 * RTK register operations
 */

void rtk_vcodec_write(struct rtk_vcodec_dev *dev, u32 data, u32 reg)
{
	// v4l2_dbg(3, coda_debug, &dev->v4l2_dev,
	// 	 "%s: data=0x%x, reg=0x%x\n", __func__, data, reg);
	writel(data, dev->regs_base + reg);
}

unsigned int rtk_vcodec_read(struct rtk_vcodec_dev *dev, u32 reg)
{
	u32 data;

	data = readl(dev->regs_base + reg);
	// v4l2_dbg(3, coda_debug, &dev->v4l2_dev,
	// 	 "%s: data=0x%x, reg=0x%x\n", __func__, data, reg);
	return data;
}

void rtk_vcodec_write_base(struct rtk_vcodec_ctx *ctx, struct rtk_q_data *q_data,
		     struct vb2_v4l2_buffer *buf, unsigned int reg_y)
{
	u32 base_y = vb2_dma_contig_plane_dma_addr(&buf->vb2_buf, 0);
	u32 base_cb, base_cr;

	switch (q_data->fourcc) {
	case V4L2_PIX_FMT_YUYV:
		/* Fallthrough: IN -H264-> CODA -NV12 MB-> VDOA -YUYV-> OUT */
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_YUV420:
	default:
		base_cb = base_y + q_data->bytesperline * q_data->height;
		base_cr = base_cb + q_data->bytesperline * q_data->height / 4;
		break;
	case V4L2_PIX_FMT_YVU420:
		/* Switch Cb and Cr for YVU420 format */
		base_cr = base_y + q_data->bytesperline * q_data->height;
		base_cb = base_cr + q_data->bytesperline * q_data->height / 4;
		break;
	case V4L2_PIX_FMT_YUV422P:
		base_cb = base_y + q_data->bytesperline * q_data->height;
		base_cr = base_cb + q_data->bytesperline * q_data->height / 2;
	}

	rtk_vcodec_write(ctx->dev, base_y, reg_y);
	rtk_vcodec_write(ctx->dev, base_cb, reg_y + 4);
	rtk_vcodec_write(ctx->dev, base_cr, reg_y + 8);
}

unsigned long rtk_dec_isbusy(struct rtk_vcodec_dev *dev)
{
	return rtk_vcodec_read(dev, BIT_BUSY_FLAG);
}

int rtk_wait_timeout(struct rtk_vcodec_dev *dev)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	while (rtk_dec_isbusy(dev)) {
		if (time_after(jiffies, timeout)) {
			v4l2_err(&dev->v4l2_dev, "rtk command timeout\n");
			return -ETIMEDOUT;
		}
	}
	return 0;
}

static void rtk_command_async(struct rtk_vcodec_ctx *ctx, int cmd)
{
	struct rtk_vcodec_dev *dev = ctx->dev;

	// TODO: ray refine free fb timing
	rtk_vcodec_write(dev, ctx->frm_dis_flg,
			RTK_REG_BIT_FRM_DIS_FLG(ctx->reg_idx));

	rtk_vcodec_write(dev, ctx->workbuf.paddr, BIT_WORK_BUF_ADDR);
	rtk_vcodec_write(dev, RTK_REG_BUSY_ENABLE, BIT_BUSY_FLAG);

	printk(KERN_INFO"[\x1b[33midx : %d, codec_mode : %d, codec_mode_aux : %d, ctx->frm_dis_flg : 0x%x\033[0m]\n",
		ctx->idx, ctx->params.codec_mode, ctx->params.codec_mode_aux, ctx->frm_dis_flg);
	rtk_vcodec_write(dev, ctx->idx, BIT_RUN_INDEX);
	rtk_vcodec_write(dev, ctx->params.codec_mode, BIT_RUN_COD_STD);
	rtk_vcodec_write(dev, ctx->params.codec_mode_aux, BIT_RUN_AUX_STD);

	rtk_vcodec_write(dev, cmd, BIT_RUN_COMMAND);
}

static int rtk_command_sync(struct rtk_vcodec_ctx *ctx, int cmd)
{
	struct rtk_vcodec_dev *dev = ctx->dev;
	int ret;

	lockdep_assert_held(&dev->rtk_mutex);

	rtk_command_async(ctx, cmd);
	ret = rtk_wait_timeout(dev);

	return ret;
}

void rtk_set_gdi_regs(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_vcodec_dev *dev = ctx->dev;
	const u16 *xy2ca_map;
	u32 xy2rbc_config;
	int i;

	printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	switch (ctx->tiled_map_type) {
	case RTK_LINEAR_FRAME_MAP:
	default:
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		xy2ca_map = xy2ca_zero_map;
		xy2rbc_config = 0;
		break;
	case RTK_TILED_FRAME_MB_RASTER_MAP:
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		xy2ca_map = xy2ca_tiled_map;
		xy2rbc_config = CODA9_XY2RBC_TILED_MAP |
				CODA9_XY2RBC_CA_INC_HOR |
				(16 - 1) << 12 | (8 - 1) << 4;
		break;
	}

	for (i = 0; i < 16; i++) {
		printk(KERN_INFO"[\x1b[32mxy2ca_map[%d] : 0x%x\033[0m]\n", i, xy2ca_map[i]);
		rtk_vcodec_write(dev, xy2ca_map[i],
				GDI_XY2_CAS_0 + 4 * i);
	}

	for (i = 0; i < 4; i++) {
		printk(KERN_INFO"[\x1b[32m xy2baMap[%d] : 0x%x\033[0m]\n", i, XY2(ZERO, 0, ZERO, 0));
		rtk_vcodec_write(dev, XY2(ZERO, 0, ZERO, 0),
				GDI_XY2_BA_0 + 4 * i);
	}

	for (i = 0; i < 16; i++) {
		printk(KERN_INFO"[\x1b[32m xy2raMap[%d] : 0x%x\033[0m]\n", i, XY2(ZERO, 0, ZERO, 0));
		rtk_vcodec_write(dev, XY2(ZERO, 0, ZERO, 0),
				GDI_XY2_RAS_0 + 4 * i);
	}

	printk(KERN_INFO"[\x1b[32mxy2rbc_config : 0x%x\033[0m]\n", xy2rbc_config);
	rtk_vcodec_write(dev, xy2rbc_config, GDI_XY2_RBC_CONFIG);

	if (xy2rbc_config) {
		for (i = 0; i < 32; i++) {
			printk(KERN_INFO"[\x1b[32m rbc2axi_tiled_map[%d] : 0x%x\033[0m]\n", i, rbc2axi_tiled_map[i]);
			rtk_vcodec_write(dev, rbc2axi_tiled_map[i],
					GDI_RBC2_AXI_0 + 4 * i);
		}
	}
}

/**
 * RTK bitstream operations
 */

static void rtk_kfifo_sync_from_device(struct rtk_vcodec_ctx *ctx)
{
	struct __kfifo *kfifo = &ctx->bitstream_fifo.kfifo;
	struct rtk_vcodec_dev *dev = ctx->dev;
	u32 rd_ptr;

	rd_ptr = rtk_vcodec_read(dev, RTK_REG_BIT_RD_PTR(ctx->reg_idx));

	kfifo->out = (kfifo->in & ~kfifo->mask) |
		      (rd_ptr - ctx->bitstream.paddr);
	if (kfifo->out > kfifo->in) {
		kfifo->out -= kfifo->mask + 1;
	}

	rtk_vcodec_dbg(4, ctx, "[rtk_kfifo_sync_from_device] ctx->reg_idx : %d\n", ctx->reg_idx);
	rtk_vcodec_dbg(4, ctx, "rd_ptr : 0x%x, kfifo->in : 0x%x, kfifo->out : 0x%x\n",
		rd_ptr, kfifo->in, kfifo->out);
}

static void rtk_kfifo_sync_to_device_full(struct rtk_vcodec_ctx *ctx)
{
	struct __kfifo *kfifo = &ctx->bitstream_fifo.kfifo;
	struct rtk_vcodec_dev *dev = ctx->dev;
	u32 rd_ptr, wr_ptr;

	rtk_vcodec_dbg(4, ctx, "[rtk_kfifo_sync_to_device_full] ctx->reg_idx : %d\n", ctx->reg_idx);
	rtk_vcodec_dbg(4, ctx, "kfifo->in : 0x%x, kfifo->out : 0x%x\n", kfifo->in, kfifo->out);

	rd_ptr = ctx->bitstream.paddr + (kfifo->out & kfifo->mask);
	rtk_vcodec_write(dev, rd_ptr, RTK_REG_BIT_RD_PTR(ctx->reg_idx));
	wr_ptr = ctx->bitstream.paddr + (kfifo->in & kfifo->mask);
	rtk_vcodec_write(dev, wr_ptr, RTK_REG_BIT_WR_PTR(ctx->reg_idx));
}

static void rtk_kfifo_sync_to_device_write(struct rtk_vcodec_ctx *ctx)
{
	struct __kfifo *kfifo = &ctx->bitstream_fifo.kfifo;
	struct rtk_vcodec_dev *dev = ctx->dev;
	u32 wr_ptr;

	rtk_vcodec_dbg(4, ctx, "[rtk_kfifo_sync_to_device_write] ctx->reg_idx : %d\n", ctx->reg_idx);
	rtk_vcodec_dbg(4, ctx, "kfifo->in : 0x%x, kfifo->out : 0x%x\n", kfifo->in, kfifo->out);

	wr_ptr = ctx->bitstream.paddr + (kfifo->in & kfifo->mask);
	rtk_vcodec_write(dev, wr_ptr, RTK_REG_BIT_WR_PTR(ctx->reg_idx));
}

static int rtk_bitstream_vp8_pad(struct rtk_vcodec_ctx *ctx,
						struct vb2_v4l2_buffer *src_buf, u32 payload)
{
	u8 *vaddr = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
	struct ivf_file_header *file_header;
	struct ivf_frame_header *frame_header;

	u32 file_header_size  = sizeof(*file_header);
	u32 frame_header_size = sizeof(*frame_header);
	u32 n;

	if (!ctx->initialized) {
		if (vaddr[0] == 'D' && vaddr[1] == 'K' && vaddr[2] == 'I' && vaddr[3] == 'F') {
			rtk_vcodec_dbg(1, ctx, "Found file header in first sequence\n");
			return 0;
		}

		file_header = kmalloc(file_header_size, GFP_KERNEL);
		if (!file_header)
			return -ENOMEM;

		rtk_vcodec_vp8_fill_file_header(ctx, file_header);
		n = kfifo_in(&ctx->bitstream_fifo, file_header, file_header_size);
		kfree(file_header);

		if (n < file_header_size) {
			rtk_vcodec_dbg(1, ctx, "Fill file header fail\n");
			return -ENOSPC;
		}
	}

	frame_header = kmalloc(frame_header_size, GFP_KERNEL);
	if (!frame_header)
		return -ENOMEM;

	rtk_vcodec_vp8_fill_frame_header(ctx, frame_header, payload);
	n = kfifo_in(&ctx->bitstream_fifo, frame_header, frame_header_size);
	kfree(frame_header);

	return (n < frame_header_size) ? -ENOSPC : 0;
}

static int rtk_bitstream_h264_pad(struct rtk_vcodec_ctx *ctx, u32 size)
{
	unsigned char *buf;
	u32 n;

	printk(KERN_ALERT"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	if (size < 6)
		size = 6;

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	rtk_vcodec_h264_fill_nal(size, buf);
	n = kfifo_in(&ctx->bitstream_fifo, buf, size);
	kfree(buf);

	return (n < size) ? -ENOSPC : 0;
}

static int rtk_bitstream_queue(struct rtk_vcodec_ctx *ctx, const u8 *buf, u32 size)
{
	u32 n = kfifo_in(&ctx->bitstream_fifo, buf, size);

	return (n < size) ? -ENOSPC : 0;
}

static u32 rtk_buffer_parse_headers(struct rtk_vcodec_ctx *ctx,
				     struct vb2_v4l2_buffer *src_buf,
				     u32 payload)
{
	u8 *vaddr = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
	u32 size = 0;

	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	switch (ctx->codec->src_fourcc) {
	case V4L2_PIX_FMT_MPEG2:
		printk(KERN_INFO"[MPEG2][\x1b[33m%s\033[0m]\n", __func__);
		// size = coda_mpeg2_parse_headers(ctx, vaddr, payload);
		break;
	case V4L2_PIX_FMT_MPEG4:
		printk(KERN_INFO"[MPEG4][\x1b[33m%s\033[0m]\n", __func__);
		// size = coda_mpeg4_parse_headers(ctx, vaddr, payload);
		break;
	default:
		printk(KERN_INFO"[OTHER][\x1b[33m%s\033[0m]\n", __func__);
		break;
	}

	return size;
}

void rtk_bitstream_end_flag(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_vcodec_dev *dev = ctx->dev;

	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	ctx->bit_stream_param |= RTK_BIT_STREAM_END_FLAG;

	/* If this context is currently running, update the hardware flag */
	if ((dev->devinfo->product == CODA_980) &&
	    rtk_dec_isbusy(dev) &&
	    (ctx->idx == rtk_vcodec_read(dev, BIT_RUN_INDEX))) {
		printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		rtk_vcodec_write(dev, ctx->bit_stream_param,
			   BIT_BIT_STREAM_PARAM);
	}
}

static bool rtk_bitstream_try_queue(struct rtk_vcodec_ctx *ctx,
				     struct vb2_v4l2_buffer *src_buf)
{
	unsigned long payload = vb2_get_plane_payload(&src_buf->vb2_buf, 0);
	u8 *vaddr = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
	int ret;
	int i;

	rtk_vcodec_dbg(4, ctx, "[rtk_bitstream_try_queue]\n");
	rtk_vcodec_dbg(4, ctx, "src_buf %d, payload %d, kfifo_len : %d\n",
		src_buf->vb2_buf.index, payload, rtk_get_bitstream_payload(ctx));

	if (rtk_get_bitstream_payload(ctx) + payload + 512 >=
		ctx->bitstream.size) {
		v4l2_err(&ctx->dev->v4l2_dev, "over bitstream buffer size\n");
		return false;
	}

	if (!vaddr) {
		v4l2_err(&ctx->dev->v4l2_dev, "trying to queue empty buffer\n");
		return true;
	}

	if (ctx->qsequence == 0 && payload < 512) {
		/*
		 * Add padding after the first buffer, if it is too small to be
		 * fetched by the CODA, by repeating the headers. Without
		 * repeated headers, or the first frame already queued, decoder
		 * sequence initialization fails with error code 0x2000 on i.MX6
		 * or error code 0x1 on i.MX51.
		 */
		u32 header_size = rtk_buffer_parse_headers(ctx, src_buf,
							    payload);

		if (header_size) {
			rtk_vcodec_dbg(1, ctx, "pad with %u-byte header\n",
				 header_size);
			for (i = payload; i < 512; i += header_size) {
				ret = rtk_bitstream_queue(ctx, vaddr,
							   header_size);
				if (ret < 0) {
					v4l2_err(&ctx->dev->v4l2_dev,
						 "bitstream buffer overflow\n");
					return false;
				}
				if (ctx->dev->devinfo->product == CODA_980) {
					printk(KERN_ALERT"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
					break;
				}
			}
		} else {
			rtk_vcodec_dbg(1, ctx,
				 "could not parse header, sequence initialization might fail\n");
		}

		/* Add padding before the first buffer, if it is too small */
		if (ctx->codec->src_fourcc == V4L2_PIX_FMT_H264)
			rtk_bitstream_h264_pad(ctx, 512 - payload);
	}

	if (ctx->codec->src_fourcc == V4L2_PIX_FMT_VP8) {
		rtk_bitstream_vp8_pad(ctx, src_buf, payload);
	}

	ret = rtk_bitstream_queue(ctx, vaddr, payload);
	if (ret < 0) {
		v4l2_err(&ctx->dev->v4l2_dev, "bitstream buffer overflow\n");
		return false;
	}

	src_buf->sequence = ctx->qsequence++;
	printk(KERN_INFO"[\x1b[33msrc_buf->sequence : %d\033[0m]\n", src_buf->sequence);

	/* Sync read pointer to device */
	if (ctx == v4l2_m2m_get_curr_priv(ctx->dev->m2m_dev)) {
		rtk_kfifo_sync_to_device_write(ctx);
	}

	/* Set the stream-end flag after the last buffer is queued */
	if (src_buf->flags & V4L2_BUF_FLAG_LAST) {
		rtk_bitstream_end_flag(ctx);
	}

	ctx->hold = false;

	rtk_vcodec_dbg(1, ctx, "rtk_bitstream_try_queue success\n");

	rtk_vcodec_dbg(4, ctx, "queue src_buf sequence %d \n",
		src_buf->sequence, (src_buf->flags & V4L2_BUF_FLAG_LAST) ?
				 " (last)" : "");

	return true;
}

void rtk_feed_bitstream(struct rtk_vcodec_ctx *ctx, struct list_head *buffer_list)
{
	struct vb2_v4l2_buffer *src_buf;
	struct rtk_meta_buffer *meta;
	u32 start;

	if (ctx->bit_stream_param & RTK_BIT_STREAM_END_FLAG) {
		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		rtk_vcodec_dbg(1, ctx, "bitstream end\n");
		return;
	}

	while (v4l2_m2m_num_src_bufs_ready(ctx->fh.m2m_ctx) > 0) {

		if (ctx->num_frame_buffers &&
		    ctx->num_metas >= ctx->num_frame_buffers) {
			meta = list_first_entry(&ctx->meta_buffer_list,
						struct rtk_meta_buffer, list);
			/*
			 * If we managed to fill in at least a full reorder
			 * window of buffers (num_frame_buffers is a
			 * conservative estimate for this) and the bitstream
			 * prefetcher has at least 2 256 bytes periods beyond
			 * the first buffer to fetch, we can safely stop queuing
			 * in order to limit the decoder drain latency.
			 */
			if (rtk_bitstream_can_fetch_past(ctx, meta->end)) {
				break;
			}
		}

		src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
		rtk_vcodec_dbg(4, ctx, "feed bitstream src buf %d\n", src_buf->vb2_buf.index);

		/* Drop frames that do not start/end with a SOI/EOI markers */
		// TODO: refine later
		// if (ctx->codec->src_fourcc == V4L2_PIX_FMT_JPEG &&
		//     !coda_jpeg_check_buffer(ctx, &src_buf->vb2_buf)) {
		// 	v4l2_err(&ctx->dev->v4l2_dev,
		// 		 "dropping invalid JPEG frame %d\n",
		// 		 ctx->qsequence);
		// 	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
		// 	if (buffer_list) {
		// 		struct v4l2_m2m_buffer *m2m_buf;

		// 		m2m_buf = container_of(src_buf,
		// 				       struct v4l2_m2m_buffer,
		// 				       vb);
		// 		list_add_tail(&m2m_buf->list, buffer_list);
		// 	} else {
		// 		v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_ERROR);
		// 	}
		// 	continue;
		// }

		/* Dump empty buffers */
		if (!vb2_get_plane_payload(&src_buf->vb2_buf, 0)) {
			src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
			rtk_vcodec_dbg(4, ctx, "no plane payload\n");
			continue;
		}

		/* Buffer start position */
		start = ctx->bitstream_fifo.kfifo.in;

		if (rtk_bitstream_try_queue(ctx, src_buf)) {
			/*
			 * Source buffer is queued in the bitstream ringbuffer;
			 * queue the timestamp and mark source buffer as done
			 */
			src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
			rtk_vcodec_dbg(4, ctx, "remove bitstream src buf %d\n", src_buf->vb2_buf.index);

			meta = kmalloc(sizeof(*meta), GFP_KERNEL);
			if (meta) {
				meta->sequence = src_buf->sequence;
				meta->timecode = src_buf->timecode;
				meta->timestamp = src_buf->vb2_buf.timestamp;
				meta->start = start;
				meta->end = ctx->bitstream_fifo.kfifo.in;
				meta->last = src_buf->flags & V4L2_BUF_FLAG_LAST;
				if (meta->last)
					rtk_vcodec_dbg(1, ctx, "meta %d is last\n", meta->sequence);

				spin_lock(&ctx->meta_buffer_lock);
				list_add_tail(&meta->list,
					      &ctx->meta_buffer_list);
				ctx->num_metas++;
				spin_unlock(&ctx->meta_buffer_lock);

				rtk_vcodec_dbg(4, ctx, "Add meta sequence  : %d%s\n",
					src_buf->sequence, (src_buf->flags & V4L2_BUF_FLAG_LAST) ? " (last)" : "");
				rtk_vcodec_dbg(4, ctx, "meta timestamp : %lld\n", meta->timestamp);
				rtk_vcodec_dbg(4, ctx, "meta start     : 0x%x, meta->end : 0x%x\n", meta->start, meta->end);
				rtk_vcodec_dbg(4, ctx, "num_metas      : %d\n", ctx->num_metas);
			}

			if (buffer_list) {
				struct v4l2_m2m_buffer *m2m_buf;

				m2m_buf = container_of(src_buf,
						       struct v4l2_m2m_buffer,
						       vb);
				list_add_tail(&m2m_buf->list, buffer_list);

				rtk_vcodec_dbg(4, ctx, "Add %c frame %d to m2m buf list\n",
					rtk_frame_type(src_buf->flags), src_buf->sequence);
			} else {
				v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);

				rtk_vcodec_dbg(4, ctx, "%c frame %d done\n",
					rtk_frame_type(src_buf->flags), src_buf->sequence);
			}
		} else {
			v4l2_err(&ctx->dev->v4l2_dev, "rtk_bitstream_try_queue fail\n");
			break;
		}
	}
}

/**
 * RTK vb2 queue operations
 */

static int rtk_queue_setup(struct vb2_queue *vq,
				unsigned int *nbuffers, unsigned int *nplanes,
				unsigned int sizes[], struct device *alloc_devs[])
{
	struct rtk_vcodec_ctx *ctx = vb2_get_drv_priv(vq);
	struct rtk_q_data *q_data;
	unsigned int size;

	q_data = rtk_get_q_data(ctx, vq->type);
	size = q_data->sizeimage;

	if (*nplanes) {
		return sizes[0] < size ? -EINVAL : 0;
	}

	*nplanes = 1;
	sizes[0] = size;

	rtk_vcodec_dbg(1, ctx, "%s get %d buffer(s) of size %d each.\n",
		v4l2_type_names[vq->type], *nbuffers, size);

	return 0;
}

static int rtk_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct rtk_q_data *q_data;

	q_data = rtk_get_q_data(ctx, vb->vb2_queue->type);
	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		if (vbuf->field == V4L2_FIELD_ANY)
			vbuf->field = V4L2_FIELD_NONE;
		if (vbuf->field != V4L2_FIELD_NONE) {
			printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			v4l2_warn(&ctx->dev->v4l2_dev,
				  "%s field isn't supported\n", __func__);
			return -EINVAL;
		}
	}

	rtk_vcodec_dbg(2, ctx, "%s buf prepare:\n", v4l2_type_names[vb->vb2_queue->type]);
	rtk_vcodec_dbg(2, ctx, "   vb2_plane_size : %d, q_data->sizeimage : %d\n",
		vb2_plane_size(vb, 0), q_data->sizeimage);

	if (vb2_plane_size(vb, 0) < q_data->sizeimage) {
		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		v4l2_warn(&ctx->dev->v4l2_dev,
			  "%s data will not fit into plane (%lu < %lu)\n",
			  __func__, vb2_plane_size(vb, 0),
			  (long)q_data->sizeimage);
		return -EINVAL;
	}

	return 0;
}

static void rtk_update_menu_ctrl(struct v4l2_ctrl *ctrl, int value)
{
	if (!ctrl)
		return;

	v4l2_ctrl_lock(ctrl);

	/*
	 * Extend the control range if the parsed stream contains a known but
	 * unsupported value or level.
	 */
	if (value > ctrl->maximum) {
		__v4l2_ctrl_modify_range(ctrl, ctrl->minimum, value,
			ctrl->menu_skip_mask & ~(1 << value),
			ctrl->default_value);
	} else if (value < ctrl->minimum) {
		__v4l2_ctrl_modify_range(ctrl, value, ctrl->maximum,
			ctrl->menu_skip_mask & ~(1 << value),
			ctrl->default_value);
	}

	__v4l2_ctrl_s_ctrl(ctrl, value);

	v4l2_ctrl_unlock(ctrl);
}

void rtk_update_profile_level_ctrls(struct rtk_vcodec_ctx *ctx, u8 profile_idc,
				     u8 level_idc)
{
	const char * const *profile_names;
	const char * const *level_names;
	struct v4l2_ctrl *profile_ctrl;
	struct v4l2_ctrl *level_ctrl;
	const char *codec_name;
	u32 profile_cid;
	u32 level_cid;
	int profile;
	int level;

	switch (ctx->codec->src_fourcc) {
	case V4L2_PIX_FMT_H264:
		codec_name = "H264";
		profile_cid = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
		level_cid = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
		profile_ctrl = ctx->h264_profile_ctrl;
		level_ctrl = ctx->h264_level_ctrl;
		profile = rtk_vcodec_h264_profile(profile_idc);
		level = rtk_vcodec_h264_level(level_idc);
		break;
	case V4L2_PIX_FMT_MPEG2:
		// TODO: refine later
		// codec_name = "MPEG-2";
		// profile_cid = V4L2_CID_MPEG_VIDEO_MPEG2_PROFILE;
		// level_cid = V4L2_CID_MPEG_VIDEO_MPEG2_LEVEL;
		// profile_ctrl = ctx->mpeg2_profile_ctrl;
		// level_ctrl = ctx->mpeg2_level_ctrl;
		// profile = coda_mpeg2_profile(profile_idc);
		// level = coda_mpeg2_level(level_idc);
		break;
	case V4L2_PIX_FMT_MPEG4:
		// TODO: refine later
		// codec_name = "MPEG-4";
		// profile_cid = V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE;
		// level_cid = V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL;
		// profile_ctrl = ctx->mpeg4_profile_ctrl;
		// level_ctrl = ctx->mpeg4_level_ctrl;
		// profile = coda_mpeg4_profile(profile_idc);
		// level = coda_mpeg4_level(level_idc);
		break;
	default:
		return;
	}

	profile_names = v4l2_ctrl_get_menu(profile_cid);
	level_names = v4l2_ctrl_get_menu(level_cid);

	if (profile < 0) {
		v4l2_warn(&ctx->dev->v4l2_dev, "Invalid %s profile: %u\n",
			  codec_name, profile_idc);
	} else {
		rtk_vcodec_dbg(1, ctx, "Parsed %s profile: %s\n", codec_name,
			 profile_names[profile]);
		rtk_update_menu_ctrl(profile_ctrl, profile);
	}

	if (level < 0) {
		v4l2_warn(&ctx->dev->v4l2_dev, "Invalid %s level: %u\n",
			  codec_name, level_idc);
	} else {
		rtk_vcodec_dbg(1, ctx, "Parsed %s level: %s\n", codec_name,
			 level_names[level]);
		rtk_update_menu_ctrl(level_ctrl, level);
	}
}

static void rtk_queue_source_change_event(struct rtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event source_change_event = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes = V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	v4l2_event_queue_fh(&ctx->fh, &source_change_event);
}

static void rtk_parabuf_write(struct rtk_vcodec_ctx *ctx, int index, u32 value)
{
	struct rtk_vcodec_dev *dev = ctx->dev;
	u32 *p = ctx->parabuf.vaddr;

	p[index ^ 1] = value;
}

static void rtk_vcodec_dispatch_framebuffers(struct rtk_vcodec_ctx *ctx,
					struct rtk_extra_buf *buf, size_t size, struct vb2_v4l2_buffer *dst_buf)
{
	char *name;

	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	name = kasprintf(GFP_KERNEL, "fb%d", dst_buf->vb2_buf.index);

	// TODO: because of DMA_ATTR_NO_KERNEL_MAPPING
	// buf->vaddr = vb2_plane_vaddr(&dst_buf->vb2_buf, 0);

	buf->size = size;

	buf->paddr = vb2_dma_contig_plane_dma_addr(&dst_buf->vb2_buf, 0);

	rtk_vcodec_dbg(2, ctx, "dispatch %s : paddr : 0x%x, buf->size : %d\n",
		name, buf->paddr, buf->size);

	kfree(name);
}

static int rtk_register_framebuffers(struct rtk_vcodec_ctx *ctx,
					struct rtk_q_data *q_data, struct vb2_v4l2_buffer *dst_buf)
{
	struct rtk_vcodec_dev *dev = ctx->dev;
	unsigned int ysize, ycbcr_size;
	unsigned int mv_size;
	int fb_index;

	if (ctx->codec->src_fourcc == V4L2_PIX_FMT_H264 ||
	    ctx->codec->dst_fourcc == V4L2_PIX_FMT_H264 ||
	    ctx->codec->src_fourcc == V4L2_PIX_FMT_MPEG4 ||
	    ctx->codec->dst_fourcc == V4L2_PIX_FMT_MPEG4) {

		ysize = round_up(q_data->rect.width, 32) * round_up(q_data->rect.height, 32);
	} else if (ctx->codec->src_fourcc == V4L2_PIX_FMT_VP8) {
		ysize = round_up(q_data->rect.width, 64) * round_up(q_data->rect.height, 64);
	} else {
		ysize = round_up(q_data->rect.width, 8) * q_data->rect.height;
	}

	fb_index = dst_buf->vb2_buf.index;

	rtk_vcodec_dbg(2, ctx, "registering fb%d\n", fb_index);

	/* Dispatch frame buffers */
	rtk_vcodec_dispatch_framebuffers(ctx, &ctx->rtk_fbs[fb_index].buf,
								q_data->sizeimage, dst_buf);

	/* Register frame buffers in the parameter buffer */
	u32 y, cb, cr, mvcol;

	/* Start addresses of Y, Cb, Cr planes */
	y = ctx->rtk_fbs[fb_index].buf.paddr;
	cb = y + ysize;
	cr = y + ysize + ysize/4;
	mvcol = ctx->mvcolbuf[fb_index].paddr;
	if (ctx->tiled_map_type == RTK_TILED_FRAME_MB_RASTER_MAP) {
		cb = round_up(cb, 4096);
		mvcol = cb + ysize/2;
		cr = 0;
		/* Packed 20-bit MSB of base addresses */
		/* YYYYYCCC, CCyyyyyc, cccc.... */
		y = (y & 0xfffff000) | cb >> 20;
		cb = (cb & 0x000ff000) << 12;
	}
	rtk_parabuf_write(ctx, fb_index * 3 + 0, y);
	rtk_parabuf_write(ctx, fb_index * 3 + 1, cb);
	rtk_parabuf_write(ctx, fb_index * 3 + 2, cr);

	/* mvcol buffer for h.264 and mpeg4 */
	if (ctx->codec->src_fourcc == V4L2_PIX_FMT_H264) {
		printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		printk(KERN_INFO"[\x1b[32m mvcol : 0x%x\033[0m]\n", mvcol);
		rtk_parabuf_write(ctx, 96 + fb_index, mvcol);
	}

	if (ctx->codec->src_fourcc == V4L2_PIX_FMT_MPEG4 && fb_index == 0) {
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		rtk_parabuf_write(ctx, 97, mvcol);
	}

	return 0;
}

static void rtk_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct rtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_queue *vq = vb->vb2_queue;
	struct rtk_q_data *q_data;

	q_data = rtk_get_q_data(ctx, vb->vb2_queue->type);

	/*
	 * In the decoder case, immediately try to copy the buffer into the
	 * bitstream ringbuffer and mark it as ready to be dequeued.
	 */
	if (ctx->bitstream.size && vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		/*
		 * For backwards compatibility, queuing an empty buffer marks
		 * the stream end
		 */
		if (vb2_get_plane_payload(vb, 0) == 0) {
			rtk_vcodec_dbg(1, ctx, "queuing an empty buffer\n");
			rtk_bitstream_end_flag(ctx);
		}

		if (q_data->fourcc == V4L2_PIX_FMT_H264) {
			printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			/*
			 * Unless already done, try to obtain profile_idc and
			 * level_idc from the SPS header. This allows to decide
			 * whether to enable reordering during sequence
			 * initialization.
			 */
			if (!ctx->params.h264_profile_idc) {
				printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
				rtk_vcodec_h264_parse_sps(ctx, vb);
				rtk_update_profile_level_ctrls(ctx,
						ctx->params.h264_profile_idc,
						ctx->params.h264_level_idc);

				rtk_vcodec_dbg(2, ctx, "Sequence Header parsing from bitstream:\n");
				rtk_vcodec_dbg(2, ctx, "H264 profile : %u, level : %u\n",
					ctx->params.h264_profile_idc, ctx->params.h264_level_idc);
			}
		}

		mutex_lock(&ctx->bitstream_mutex);
		v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
		if (vb2_is_streaming(vb->vb2_queue)) {
			/* This set buf->sequence = ctx->qsequence++ */
			rtk_feed_bitstream(ctx, NULL);
		}
		mutex_unlock(&ctx->bitstream_mutex);

		if (!ctx->initialized) {
			/*
			 * Run sequence initialization in case the queued
			 * buffer contained headers.
			 */

			if (vb2_is_streaming(vb->vb2_queue) &&
			    ctx->ops->seq_init_work) {

				rtk_vcodec_dbg(1, ctx, "Run sequence initialization in case the queued buffer contained headers.\n");

				queue_work(ctx->dev->workqueue,
					   &ctx->seq_init_work);
				flush_work(&ctx->seq_init_work);
			}

			if (ctx->initialized) {
				printk(KERN_INFO"[source_change_event]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
				rtk_queue_source_change_event(ctx);
			}
		}
	} else {
		if ((ctx->inst_type == RTK_VCODEC_CTX_ENCODER || !ctx->use_bit) &&
		    vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			vbuf->sequence = ctx->qsequence++;
			printk(KERN_INFO"[vbuf->sequence : %d][\x1b[33m%s\033[0m]\n", vbuf->sequence, __func__);
		}

		if (!ctx->streamon_cap) {
			rtk_register_framebuffers(ctx, q_data, vbuf);
		} else {
			// TODO: ray refine free fb timing

			mutex_lock(&ctx->buffer_mutex);
			mutex_lock(&ctx->dev->rtk_mutex);

			if (ctx->disp_use_flg & (1 << vbuf->vb2_buf.index)) {

				rtk_vcodec_dbg(2, ctx, "Set disp unuse frame buf %d\n",
					vbuf->vb2_buf.index);

				ctx->disp_use_flg &= ~(1 << vbuf->vb2_buf.index);
				ctx->frm_dis_flg &= ~(1 << vbuf->vb2_buf.index);

				rtk_vcodec_dbg(2, ctx, "Update disp_use_flg 0x%x, frm_dis_flg 0x%x\n",
					ctx->disp_use_flg, ctx->frm_dis_flg);
			}

			mutex_unlock(&ctx->dev->rtk_mutex);
			mutex_unlock(&ctx->buffer_mutex);
		}

		v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
	}
}

static int rtk_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct rtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct v4l2_device *v4l2_dev = &ctx->dev->v4l2_dev;
	struct rtk_q_data *q_data_src, *q_data_dst;
	struct v4l2_m2m_buffer *m2m_buf, *tmp;
	struct vb2_v4l2_buffer *buf;
	struct list_head list;
	int ret = 0;

	if (count < 1) {
		v4l2_err(&ctx->dev->v4l2_dev,
			 "no buffer in queue\n");
		return -EINVAL;
	}

	rtk_vcodec_dbg(1, ctx, "start streaming %s\n", v4l2_type_names[q->type]);

	INIT_LIST_HEAD(&list);

	q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		if (ctx->inst_type == RTK_VCODEC_CTX_DECODER && ctx->use_bit) {
			/* copy the buffers that were queued before streamon */
			mutex_lock(&ctx->bitstream_mutex);
			rtk_feed_bitstream(ctx, &list);
			mutex_unlock(&ctx->bitstream_mutex);

			// TODO: refine later
			// if (ctx->dev->devinfo->product != CODA_960 &&
			//     rtk_get_bitstream_payload(ctx) < 512) {
			// 	v4l2_err(v4l2_dev, "start payload < 512\n");
			// 	ret = -EINVAL;
			// 	goto err;
			// }

			if (!ctx->initialized) {
				/* Run sequence initialization */
				printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
				rtk_vcodec_dbg(1, ctx, "Run sequence initialization\n");
				if (ctx->ops->seq_init_work) {
					queue_work(ctx->dev->workqueue,
						   &ctx->seq_init_work);
					flush_work(&ctx->seq_init_work);
				}
			}
		}

		ctx->streamon_out = 1;
	} else {
		ctx->streamon_cap = 1;
	}

	rtk_vcodec_dbg(1, ctx, "start streaming out(%d), cap(%d)\n", ctx->streamon_out, ctx->streamon_cap);

	/* Don't start the coda unless both queues are on */
	if (!(ctx->streamon_out && ctx->streamon_cap))
		goto out;

	q_data_dst = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);

	rtk_vcodec_dbg(1, ctx, "src rect : %dx%d, dst : %dx%d\n",
		q_data_src->rect.width, q_data_src->rect.height,
		q_data_dst->width, q_data_dst->height);

	if ((q_data_src->rect.width != q_data_dst->width &&
	     round_up(q_data_src->rect.width, 16) != q_data_dst->width) ||
	    (q_data_src->rect.height != q_data_dst->height &&
	     round_up(q_data_src->rect.height, 16) != q_data_dst->height)) {

		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		v4l2_err(v4l2_dev, "can't convert %dx%d to %dx%d\n",
			 q_data_src->rect.width, q_data_src->rect.height,
			 q_data_dst->width, q_data_dst->height);
		ret = -EINVAL;
		goto err;
	}

	/* Allow BIT decoder device_run with no new buffers queued */
	if (ctx->inst_type == RTK_VCODEC_CTX_DECODER && ctx->use_bit)
		v4l2_m2m_set_src_buffered(ctx->fh.m2m_ctx, true);

	ctx->gopcounter = ctx->params.gop_size - 1;

	// TODO: refine later
	// if (q_data_dst->fourcc == V4L2_PIX_FMT_JPEG)
	// 	ctx->params.gop_size = 1;

	ctx->gopcounter = ctx->params.gop_size - 1;
	/* Only decoders have this control */
	if (ctx->mb_err_cnt_ctrl)
		v4l2_ctrl_s_ctrl(ctx->mb_err_cnt_ctrl, 0);

	ret = ctx->ops->start_streaming(ctx);
	if (ctx->inst_type == RTK_VCODEC_CTX_DECODER) {
		if (ret == -EAGAIN)
			goto out;
	}
	if (ret < 0)
		goto err;

out:
	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		list_for_each_entry_safe(m2m_buf, tmp, &list, list) {
			list_del(&m2m_buf->list);
			v4l2_m2m_buf_done(&m2m_buf->vb, VB2_BUF_STATE_DONE);
		}
	}

	return 0;

err:
	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		list_for_each_entry_safe(m2m_buf, tmp, &list, list) {
			list_del(&m2m_buf->list);
			v4l2_m2m_buf_done(&m2m_buf->vb, VB2_BUF_STATE_QUEUED);
		}
		while ((buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx)))
			v4l2_m2m_buf_done(buf, VB2_BUF_STATE_QUEUED);
	} else {
		while ((buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx)))
			v4l2_m2m_buf_done(buf, VB2_BUF_STATE_QUEUED);
	}
	return ret;
}

static void rtk_stop_streaming(struct vb2_queue *q)
{
	struct rtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct rtk_vcodec_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *buf;
	bool stop;

	stop = ctx->streamon_out && ctx->streamon_cap;

	rtk_vcodec_dbg(1, ctx, "stop streaming %s\n", v4l2_type_names[q->type]);

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		ctx->streamon_out = 0;
		rtk_bitstream_end_flag(ctx);

		ctx->qsequence = 0;

		while ((buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx))) {
			rtk_vcodec_dbg(1, ctx, "remove %d and set error\n",
				buf->vb2_buf.index);
			v4l2_m2m_buf_done(buf, VB2_BUF_STATE_ERROR);
		}
	} else {
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		ctx->streamon_cap = 0;

		ctx->osequence = 0;
		ctx->sequence_offset = 0;

		while ((buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx))) {
			rtk_vcodec_dbg(1, ctx, "remove %d and set error\n",
				buf->vb2_buf.index);
			v4l2_m2m_buf_done(buf, VB2_BUF_STATE_ERROR);
		}
	}

	if (stop) {
		struct rtk_meta_buffer *meta;
		if (ctx->common_ops->seq_end_work) {
			queue_work(dev->workqueue, &ctx->seq_end_work);
			flush_work(&ctx->seq_end_work);
		}
		spin_lock(&ctx->meta_buffer_lock);
		while (!list_empty(&ctx->meta_buffer_list)) {
			meta = list_first_entry(&ctx->meta_buffer_list,
						struct rtk_meta_buffer, list);
			list_del(&meta->list);
			kfree(meta);
		}
		ctx->num_metas = 0;
		spin_unlock(&ctx->meta_buffer_lock);
		kfifo_init(&ctx->bitstream_fifo,
			ctx->bitstream.vaddr, ctx->bitstream.size);
		ctx->runcounter = 0;
		ctx->aborting = 0;
		ctx->hold = false;
	}

	if (!ctx->streamon_out && !ctx->streamon_cap) {
		ctx->bit_stream_param &= ~RTK_BIT_STREAM_END_FLAG;
	}
}

// TODO: Does encoder need its own qops ?
static const struct vb2_ops rtk_qops = {
	.queue_setup     = rtk_queue_setup,
	.buf_prepare     = rtk_buf_prepare,
	.buf_queue       = rtk_buf_queue,
	.start_streaming = rtk_start_streaming,
	.stop_streaming  = rtk_stop_streaming,
	.wait_prepare    = vb2_ops_wait_prepare,
	.wait_finish     = vb2_ops_wait_finish,
};

/**
 * RTK Decoder common operations
 */

static phys_addr_t rtk_sram_alloc(struct rtk_axi_sram_info *sram, size_t size)
{
	phys_addr_t ret;

	if (size > sram->remaining)
		return 0;

	sram->remaining -= size;
	ret = sram->next_paddr;
	sram->next_paddr += size;

	return ret;
}

static void rtk_config_axi_sram(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_axi_sram_info *sram_info = &ctx->sram_info;
	struct rtk_vcodec_dev *dev = ctx->dev;
	int w64, w128, w144, offset;
	unsigned int mb_width;
	int dbk_use_bits;
	int bit_use_bits;
	int ip_use_bits;

	memset(sram_info, 0, sizeof(*sram_info));

	sram_info->next_paddr = NULL;
	sram_info->remaining = dev->sram.size;

	rtk_vcodec_dbg(3, ctx, "sram base : %px, sram size : %d\n",
			sram_info->next_paddr, sram_info->remaining);

	dbk_use_bits = RTK_USE_HOST_DBK_ENABLE | RTK_USE_DBK_ENABLE;
	bit_use_bits = RTK_USE_HOST_BIT_ENABLE | RTK_USE_BIT_ENABLE;
	ip_use_bits  = RTK_USE_HOST_IP_ENABLE  | RTK_USE_IP_ENABLE;

	if (ctx->inst_type == RTK_VCODEC_CTX_ENCODER) {
		struct rtk_q_data *q_data_src;

		q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
		mb_width = DIV_ROUND_UP(q_data_src->rect.width, 16);
		w128 = mb_width * 128;
		w64 = mb_width * 64;

		/* Only H.264BP and H.263P3 are considered */
		sram_info->buf_dbk_y_use = rtk_sram_alloc(sram_info, w64);
		sram_info->buf_dbk_c_use = rtk_sram_alloc(sram_info, w64);
		if (!sram_info->buf_dbk_y_use || !sram_info->buf_dbk_c_use)
			goto out;
		sram_info->axi_sram_use |= dbk_use_bits;

		sram_info->buf_bit_use = rtk_sram_alloc(sram_info, w128);
		if (!sram_info->buf_bit_use)
			goto out;
		sram_info->axi_sram_use |= bit_use_bits;

		sram_info->buf_ip_ac_dc_use = rtk_sram_alloc(sram_info, w128);
		if (!sram_info->buf_ip_ac_dc_use)
			goto out;
		sram_info->axi_sram_use |= ip_use_bits;

		/* OVL and BTP disabled for encoder */
	} else if (ctx->inst_type == RTK_VCODEC_CTX_DECODER) {
		struct rtk_q_data *q_data_dst, *q_data_src;

		q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);

		q_data_dst = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
		mb_width = DIV_ROUND_UP(q_data_dst->width, 16);
		w144 = mb_width * 144;
		w128 = mb_width * 128;
		w64  = mb_width * 64;

		if (q_data_src->fourcc == V4L2_PIX_FMT_H264)
			sram_info->buf_bit_use = rtk_sram_alloc(sram_info, w144);
		else if (q_data_src->fourcc == V4L2_PIX_FMT_VP8)
			sram_info->buf_bit_use = rtk_sram_alloc(sram_info, 0);

		sram_info->axi_sram_use |= bit_use_bits;

		sram_info->buf_ip_ac_dc_use = rtk_sram_alloc(sram_info, w64);

		sram_info->axi_sram_use |= ip_use_bits;

		if (q_data_src->fourcc == V4L2_PIX_FMT_H264)
			offset = (ctx->params.h264_profile_idc == 66) ? w64 : w128;
		else if (q_data_src->fourcc == V4L2_PIX_FMT_VP8)
			offset = w128;

		sram_info->buf_dbk_c_use = rtk_sram_alloc(sram_info, offset);
		sram_info->buf_dbk_y_use = rtk_sram_alloc(sram_info, offset);

		sram_info->axi_sram_use |= dbk_use_bits;
	}

out:

	rtk_vcodec_dbg(3, ctx, "axi_sram_use     : 0x%x\n",	sram_info->axi_sram_use);
	rtk_vcodec_dbg(3, ctx, "buf_bit_use      : 0x%x\n",	sram_info->buf_bit_use);
	rtk_vcodec_dbg(3, ctx, "buf_ip_ac_dc_use : 0x%x\n",	sram_info->buf_ip_ac_dc_use);
	rtk_vcodec_dbg(3, ctx, "buf_dbk_y_use    : 0x%x\n",	sram_info->buf_dbk_y_use);
	rtk_vcodec_dbg(3, ctx, "buf_dbk_c_use    : 0x%x\n",	sram_info->buf_dbk_c_use);
}

static inline int rtk_vcodec_alloc_ctx_buf(struct rtk_vcodec_ctx *ctx,
					 struct rtk_extra_buf *buf, size_t size,
					 const char *name)
{
	return rtk_vcodec_alloc_extra_buf(ctx->dev, buf, size, name, ctx->debugfs_entry);
}

static void rtk_vcodec_free_mvcol_buffers(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_vcodec_dev *dev = ctx->dev;
	int i;

	for (i = 0; i < ctx->num_frame_buffers; i++)
		rtk_vcodec_free_extra_buf(dev, &ctx->mvcolbuf[i], "mvcolbuf");
}

static int rtk_vcodec_alloc_mvcol_buffers(struct rtk_vcodec_ctx *ctx,
				      struct rtk_q_data *q_data)
{
	unsigned int ysize, ycbcr_size, mv_size;
	int i, ret;

	ysize = round_up(q_data->rect.width, 32) * round_up(q_data->rect.height, 32);

	for (i = 0; i < ctx->num_frame_buffers; i++) {
		if ((q_data->fourcc == V4L2_PIX_FMT_H264 ||
			(q_data->fourcc == V4L2_PIX_FMT_MPEG4 && i == 0))) {
			ycbcr_size = (ysize * 3) / 2;
			mv_size = (ycbcr_size + 4) / 5;
			mv_size = ((mv_size + 7) / 8) * 8;

			ret = rtk_vcodec_alloc_ctx_buf(ctx, &ctx->mvcolbuf[i],
				     mv_size, "mvcolbuf");
			if (ret < 0) {
				rtk_vcodec_free_mvcol_buffers(ctx);
				return ret;
			}
		}
	}

	return 0;
}

static void rtk_vcodec_free_ctx_buffers(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_vcodec_dev *dev = ctx->dev;

	rtk_vcodec_free_extra_buf(dev, &ctx->slicebuf, "slicebuf");
	rtk_vcodec_free_extra_buf(dev, &ctx->workbuf, "workbuf");
	rtk_vcodec_free_extra_buf(dev, &ctx->parabuf, "parabuf");
}

static int rtk_vcodec_alloc_ctx_buffers(struct rtk_vcodec_ctx *ctx,
				      struct rtk_q_data *q_data)
{
	struct rtk_vcodec_dev *dev = ctx->dev;
	size_t size;
	unsigned int ysize, ycbcr_size, mvcol_size;
	int ret;

	if (!ctx->parabuf.vaddr) {
		size = PARA_BUF_SIZE;
		ret = rtk_vcodec_alloc_ctx_buf(ctx, &ctx->parabuf,
					     size, "parabuf");
		if (ret < 0)
			return ret;
	}

	if (!ctx->slicebuf.vaddr && q_data->fourcc == V4L2_PIX_FMT_H264) {
		size = SLICE_SAVE_SIZE;
		ret = rtk_vcodec_alloc_ctx_buf(ctx, &ctx->slicebuf, size,
					     "slicebuf");
		if (ret < 0)
			goto err;
	}

	if (!ctx->slicebuf.vaddr && q_data->fourcc == V4L2_PIX_FMT_VP8) {
		size = VP8_MB_SAVE_SIZE;
		ret = rtk_vcodec_alloc_ctx_buf(ctx, &ctx->slicebuf, size,
					     "slicebuf");
		if (ret < 0)
			goto err;
	}

	if (!ctx->workbuf.vaddr) {
		size = dev->devinfo->workbuf_size;
		if (dev->devinfo->product == CODA_980 &&
		    q_data->fourcc == V4L2_PIX_FMT_H264)
			size += PS_SAVE_SIZE;
		ret = rtk_vcodec_alloc_ctx_buf(ctx, &ctx->workbuf, size,
					     "workbuf");
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	rtk_vcodec_free_ctx_buffers(ctx);
	return ret;
}

static int rtk_alloc_bitstream_buffer(struct rtk_vcodec_ctx *ctx,
				       struct rtk_q_data *q_data)
{
	if (ctx->bitstream.vaddr) {
		return 0;
	}

	ctx->bitstream.size = roundup_pow_of_two(q_data->sizeimage * 2);
	ctx->bitstream.vaddr = dma_alloc_wc(ctx->dev->dev, ctx->bitstream.size,
					    &ctx->bitstream.paddr, GFP_KERNEL);

	v4l2_dbg(1, rtk_vcodec_debug, &ctx->dev->v4l2_dev,
			"alloc bitstream buf vaddr : %px, paddr : %px, size : %d\n",
			ctx->bitstream.vaddr, ctx->bitstream.paddr, ctx->bitstream.size);

	if (!ctx->bitstream.vaddr) {
		v4l2_err(&ctx->dev->v4l2_dev,
			 "failed to allocate bitstream ringbuffer");
		return -ENOMEM;
	}
	kfifo_init(&ctx->bitstream_fifo,
		   ctx->bitstream.vaddr, ctx->bitstream.size);

	return 0;
}

static void rtk_free_bitstream_buffer(struct rtk_vcodec_ctx *ctx)
{
	if (ctx->bitstream.vaddr == NULL)
		return;

	v4l2_dbg(1, rtk_vcodec_debug, &ctx->dev->v4l2_dev,
			"free bitstream buf vaddr : %px, paddr : %px, size : %d\n",
			ctx->bitstream.vaddr, ctx->bitstream.paddr, ctx->bitstream.size);

	dma_free_wc(ctx->dev->dev, ctx->bitstream.size, ctx->bitstream.vaddr,
		    ctx->bitstream.paddr);
	ctx->bitstream.vaddr = NULL;
	kfifo_init(&ctx->bitstream_fifo, NULL, 0);
}

static int rtk_queue_init(struct rtk_vcodec_ctx *ctx, struct vb2_queue *vq)
{
	vq->drv_priv = ctx;
	vq->ops = &rtk_qops;
	vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	vq->lock = &ctx->dev->dev_mutex;
	/* One way to indicate end-of-stream for coda is to set the
	 * bytesused == 0. However by default videobuf2 handles bytesused
	 * equal to 0 as a special case and changes its value to the size
	 * of the buffer. Set the allow_zero_bytesused flag, so
	 * that videobuf2 will keep the value of bytesused intact.
	 */
	vq->allow_zero_bytesused = 1;
	/*
	 * We might be fine with no buffers on some of the queues, but that
	 * would need to be reflected in job_ready(). Currently we expect all
	 * queues to have at least one buffer queued.
	 */
	vq->min_buffers_needed = 1;
	vq->dev = ctx->dev->dev;

	return vb2_queue_init(vq);
}

int rtk_decoder_queue_init(void *priv, struct vb2_queue *src_vq,
			    struct vb2_queue *dst_vq)
{
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	src_vq->io_modes = VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	src_vq->mem_ops = &vb2_vmalloc_memops;

	ret = rtk_queue_init(priv, src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	dst_vq->dma_attrs = DMA_ATTR_NO_KERNEL_MAPPING;
	dst_vq->mem_ops = &vb2_dma_contig_memops;

	return rtk_queue_init(priv, dst_vq);
}

static int rtk_decoder_reqbufs(struct rtk_vcodec_ctx *ctx,
				struct v4l2_requestbuffers *rb)
{
	struct rtk_q_data *q_data_src;
	int ret;

	if (rb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		if (rb->count) {
			q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
			ret = rtk_vcodec_alloc_ctx_buffers(ctx, q_data_src);
			if (ret < 0)
				return ret;
			ret = rtk_alloc_bitstream_buffer(ctx, q_data_src);
			if (ret < 0) {
				rtk_free_bitstream_buffer(ctx);
				return ret;
			}
		} else {
			rtk_free_bitstream_buffer(ctx);
			rtk_vcodec_free_ctx_buffers(ctx);
		}
	}

	if (rb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		if (rb->count) {
			ctx->num_frame_buffers = rb->count;
			q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
			/* Allocate mvcol buffers */
			ret = rtk_vcodec_alloc_mvcol_buffers(ctx, q_data_src);
			if (ret < 0)
				return ret;
		} else {
			rtk_vcodec_free_mvcol_buffers(ctx);
		}
	}

	return 0;
}

/**
 * RTK VE2 Decoder operations
 */

const struct rtk_context_ops rtk_ve2_decoder_ops = {
	// TODO: need to figure out how to implement ve2 ops
	// .queue_init = rtk_decoder_queue_init,
	.reqbufs = rtk_decoder_reqbufs,
	// .start_streaming = rtk_ve2_start_decoding,
	// .prepare_run = rtk_ve2_prepare_decode,
	// .finish_run = rtk_ve2_finish_decode,
	// .run_timeout = rtk_ve2_decode_timeout,
	// .seq_init_work = rtk_ve2_seq_init_work,
	// .seq_end_work = rtk_ve2_seq_end_work,
	// .release = rtk_ve2_release,
};


/**
 * RTK VE1 Decoder operations
 */

static bool rtk_reorder_enable(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_vcodec_dev *dev = ctx->dev;
	int profile;

	return true;

	// if (dev->devinfo->product != CODA_HX4 &&
	//     dev->devinfo->product != CODA_7541 &&
	//     dev->devinfo->product != CODA_960) {
	// 	printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	// 	return false;
	// }
	//

	if (ctx->codec->src_fourcc == V4L2_PIX_FMT_JPEG) {
		printk(KERN_INFO"[V4L2_PIX_FMT_JPEG]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		return false;
	}

	if (ctx->codec->src_fourcc != V4L2_PIX_FMT_H264) {
		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		return true;
	}

	printk(KERN_INFO"[\x1b[32mctx->params.h264_profile_idc : 0x%x\033[0m]\n",
		ctx->params.h264_profile_idc);

	profile = rtk_vcodec_h264_profile(ctx->params.h264_profile_idc);
	if (profile < 0)
		v4l2_warn(&dev->v4l2_dev, "Unknown H264 Profile: %u\n",
			  ctx->params.h264_profile_idc);

	rtk_vcodec_dbg(2, ctx, "H264 profile : %u\n", profile);

	/* Baseline profile does not support reordering */
	return profile > V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
}

// TODO: need to figure out what it is
static void rtk_decoder_drop_used_metas(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_meta_buffer *meta, *tmp;
	/*
	 * All metas that end at or before the RD pointer (fifo out),
	 * are now consumed by the VPU and should be released.
	 */
	spin_lock(&ctx->meta_buffer_lock);
	list_for_each_entry_safe(meta, tmp, &ctx->meta_buffer_list, list) {
		if (ctx->bitstream_fifo.kfifo.out >= meta->end) {

			rtk_vcodec_dbg(2, ctx, "releasing meta: seq=%d start=%d end=%d\n",
				 meta->sequence, meta->start, meta->end);

			list_del(&meta->list);
			ctx->num_metas--;
			ctx->first_frame_sequence++;
			kfree(meta);
		}
	}
	spin_unlock(&ctx->meta_buffer_lock);
}

static void rtk_set_frame_cache(struct rtk_vcodec_ctx *ctx, u32 fourcc)
{
	u32 cache_size, cache_config;

	printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	// TODO: refine later
	// if (ctx->tiled_map_type == RTK_LINEAR_FRAME_MAP) {
	// 	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	// 	/* Luma 2x0 page, 2x6 cache, chroma 2x0 page, 2x4 cache size */
	// 	cache_size = 0x20262024;
	// 	cache_config = 2 << CODA9_CACHE_PAGEMERGE_OFFSET;
	// } else {
	// 	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	// 	/* Luma 0x2 page, 4x4 cache, chroma 0x2 page, 4x3 cache size */
	// 	cache_size = 0x02440243;
	// 	cache_config = 1 << CODA9_CACHE_PAGEMERGE_OFFSET;
	// }

	printk(KERN_INFO"[\x1b[32mcache_size : 0x%x\033[0m]\n", cache_size);

	// TODO: rtk16xxb settings no frame cache size setting in decoder
	// rtk_vcodec_write(ctx->dev, cache_size, CMD_SET_FRAME_CACHE_SIZE);

	// TODO: refine later
	// if (fourcc == V4L2_PIX_FMT_NV12 || fourcc == V4L2_PIX_FMT_YUYV) {
	// 	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	// 	cache_config |= 32 << CODA9_CACHE_LUMA_BUFFER_SIZE_OFFSET |
	// 			16 << CODA9_CACHE_CR_BUFFER_SIZE_OFFSET |
	// 			0 << CODA9_CACHE_CB_BUFFER_SIZE_OFFSET;
	// } else {
	// 	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	// 	cache_config |= 32 << CODA9_CACHE_LUMA_BUFFER_SIZE_OFFSET |
	// 			8 << CODA9_CACHE_CR_BUFFER_SIZE_OFFSET |
	// 			8 << CODA9_CACHE_CB_BUFFER_SIZE_OFFSET;
	// }
	// TODO: hot code here for temp, reference to rtk16xxb settings
	cache_config = 0x7e0;

	printk(KERN_INFO"[\x1b[32mcache_config : 0x%x\033[0m]\n", cache_config);

	rtk_vcodec_write(ctx->dev, cache_config, CMD_SET_FRAME_CACHE_CONFIG);
}

static int rtk_vcodec_seq_init(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_q_data *q_data_src, *q_data_dst;
	u32 bitstream_buf, bitstream_size;
	struct rtk_vcodec_dev *dev = ctx->dev;
	int width, height;
	u8 profile, level;
	u32 src_fourcc, dst_fourcc;
	u32 val;
	int ret;

	lockdep_assert_held(&dev->rtk_mutex);

	printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	rtk_vcodec_dbg(1, ctx, "Video Data Order Adapter: %s\n",
		 ctx->use_vdoa ? "Enabled" : "Disabled");

	/* Start decoding */
	q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	q_data_dst = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	bitstream_buf = ctx->bitstream.paddr;
	bitstream_size = ctx->bitstream.size;
	src_fourcc = q_data_src->fourcc;
	dst_fourcc = q_data_dst->fourcc;

	/* Update coda bitstream read and write pointers from kfifo */
	rtk_kfifo_sync_to_device_full(ctx);

	// TODO: coda upstream
	// ctx->frame_mem_ctrl &= ~(RTK_FRAME_CHROMA_INTERLEAVE | (0x3 << 9) |
	// 			 RTK_FRAME_TILED2LINEAR);
	// if (dst_fourcc == V4L2_PIX_FMT_NV12 || dst_fourcc == V4L2_PIX_FMT_YUYV)
	// 	ctx->frame_mem_ctrl |= RTK_FRAME_CHROMA_INTERLEAVE;
	// if (ctx->tiled_map_type == RTK_TILED_FRAME_MB_RASTER_MAP)
	// 	ctx->frame_mem_ctrl |= (0x3 << 9) |
	// 		((ctx->use_vdoa) ? 0 : RTK_FRAME_TILED2LINEAR);

	// TODO: rtk16xxb settings
	ctx->frame_mem_ctrl = (1 << 2) | (1 << 15);
	printk(KERN_INFO"[\x1b[32mctx->frame_mem_ctrl : 0x%x\033[0m]\n", ctx->frame_mem_ctrl);
	rtk_vcodec_write(dev, ctx->frame_mem_ctrl, BIT_FRAME_MEM_CTRL);

	// TODO: ray refine free fb timing
	ctx->disp_use_flg = 0;
	ctx->display_idx = -1;
	ctx->frm_dis_flg = 0;
	rtk_vcodec_write(dev, 0, RTK_REG_BIT_FRM_DIS_FLG(ctx->reg_idx));

	rtk_vcodec_write(dev, bitstream_buf, CMD_DEC_SEQ_BB_START);
	rtk_vcodec_write(dev, bitstream_size / 1024, CMD_DEC_SEQ_BB_SIZE);

	val = 0;
	if (rtk_reorder_enable(ctx)) {
		val |= RTK_REORDER_ENABLE;
	}

	printk(KERN_INFO"[\x1b[32mCMD_DEC_SEQ_OPTION val :0x%x\033[0m]\n", val);
	rtk_vcodec_write(dev, val, CMD_DEC_SEQ_OPTION);

	ctx->params.codec_mode = ctx->codec->mode;
	if (dev->devinfo->product == CODA_980 &&
	    src_fourcc == V4L2_PIX_FMT_MPEG4) {
		ctx->params.codec_mode_aux = RTK_AUX_STD_MPEG4;
	} else if (src_fourcc == V4L2_PIX_FMT_VP8) {
		ctx->params.codec_mode_aux = RTK_AUX_STD_VP8;
	} else {
		ctx->params.codec_mode_aux = RTK_AUX_STD_H264;
	}

	if (src_fourcc == V4L2_PIX_FMT_MPEG4) {
		rtk_vcodec_write(dev, RTK_MP4_CLASS_MPEG4,
			   CMD_DEC_SEQ_MP4_ASP_CLASS);
	}

	if (src_fourcc == V4L2_PIX_FMT_H264) {
		rtk_vcodec_write(dev, 0x1, CMD_DEC_SEQ_X264_MV_EN);
		rtk_vcodec_write(dev, 0x400, CMD_DEC_SEQ_SPP_CHUNK_SIZE);
	}

	ctx->bit_stream_param = RTK_BIT_STREAM_PICEND_MODE;

	rtk_vcodec_write(dev, 0x0, BIT_BIT_STREAM_CTRL);
	rtk_vcodec_write(dev, ctx->bit_stream_param, BIT_BIT_STREAM_PARAM);

	ret = rtk_command_sync(ctx, RTK_COMMAND_SEQ_INIT);
	ctx->bit_stream_param = 0;
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "RTK_COMMAND_SEQ_INIT timeout\n");
		return ret;
	}
	ctx->sequence_offset = ~0U;
	ctx->initialized = 1;
	ctx->first_frame_sequence = 0;

	/* Update kfifo out pointer from coda bitstream read pointer */
	rtk_kfifo_sync_from_device(ctx);

	/*
	 * After updating the read pointer, we need to check if
	 * any metas are consumed and should be released.
	 */
	rtk_decoder_drop_used_metas(ctx);

	if (rtk_vcodec_read(dev, RET_DEC_SEQ_SUCCESS) == 0) {
		v4l2_err(&dev->v4l2_dev,
			"RTK_COMMAND_SEQ_INIT failed, error code = 0x%x\n",
			rtk_vcodec_read(dev, RET_DEC_SEQ_SEQ_ERR_REASON));
		return -EAGAIN;
	}

	val = rtk_vcodec_read(dev, RET_DEC_SEQ_SRC_SIZE);
	width = (val >> PICWIDTH_OFFSET) & PICWIDTH_MASK;
	height = val & PICHEIGHT_MASK;

	rtk_vcodec_dbg(2, ctx, "[RET_DEC_SEQ_SRC_SIZE] width(%d), height(%d)\n", width, height);

	if (width > q_data_dst->bytesperline || height > q_data_dst->height) {
		v4l2_err(&dev->v4l2_dev, "stream is %dx%d, not %dx%d\n",
			 width, height, q_data_dst->bytesperline,
			 q_data_dst->height);
		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		return -EINVAL;
	}

	width = round_up(width, 16);
	height = round_up(height, 16);

	rtk_vcodec_dbg(1, ctx, "start decoding: %dx%d\n", width, height);

	ctx->min_frame_buffers = rtk_vcodec_read(dev, RET_DEC_SEQ_FRAME_NEED);
	ctx->num_frame_buffers = 0;

	rtk_vcodec_dbg(2, ctx, "min fb(%d), num fb(%d)\n",
		ctx->min_frame_buffers, ctx->num_frame_buffers);

	/*
	 * If the VDOA is used, the decoder needs one additional frame,
	 * because the frames are freed when the next frame is decoded.
	 * Otherwise there are visible errors in the decoded frames (green
	 * regions in displayed frames) and a broken order of frames (earlier
	 * frames are sporadically displayed after later frames).
	 */
	if (ctx->use_vdoa)
		ctx->num_frame_buffers += 1;

	// TODO: ray add frame buffer flow, when reqbufs (min_frame_buffers + 2) ?
	if ((ctx->min_frame_buffers + 2) > RTK_MAX_FRAME_BUFFERS) {
		v4l2_err(&dev->v4l2_dev,
			 "not enough framebuffers to decode (%d < %d)\n",
			 RTK_MAX_FRAME_BUFFERS, ctx->min_frame_buffers + 2);
		return -EINVAL;
	}

	/* sequence crop information */
	if (src_fourcc == V4L2_PIX_FMT_H264) {
		u32 left_right;
		u32 top_bottom;

		left_right = rtk_vcodec_read(dev, RET_DEC_SEQ_CROP_LEFT_RIGHT);
		top_bottom = rtk_vcodec_read(dev, RET_DEC_SEQ_CROP_TOP_BOTTOM);

		q_data_dst->rect.left = (left_right >> 10) & 0x3ff;
		q_data_dst->rect.top = (top_bottom >> 10) & 0x3ff;
		q_data_dst->rect.width = width - q_data_dst->rect.left -
					 (left_right & 0x3ff);
		q_data_dst->rect.height = height - q_data_dst->rect.top -
					  (top_bottom & 0x3ff);

		rtk_vcodec_dbg(2, ctx, "H264 seq cropping info (%dx%d)-(%dx%d)\n",
			q_data_dst->rect.left, q_data_dst->rect.top,
			q_data_dst->rect.width, q_data_dst->rect.height);
	}

	if (src_fourcc == V4L2_PIX_FMT_VP8) {
		u32 scale_info;

		scale_info = rtk_vcodec_read(dev, RET_DEC_SEQ_VP8_SCALE_INFO);

		q_data_dst->rect.left = 0;
		q_data_dst->rect.top = 0;
		q_data_dst->rect.width = (scale_info >> 14) & 0x3FFF;
		q_data_dst->rect.height = (scale_info >> 0) & 0x3FFF;

		rtk_vcodec_dbg(2, ctx, "VP8 seq cropping info (%dx%d)-(%dx%d)\n",
			q_data_dst->rect.left, q_data_dst->rect.top,
			q_data_dst->rect.width, q_data_dst->rect.height);
	}

	val = rtk_vcodec_read(dev, RET_DEC_SEQ_HEADER_REPORT);
	profile = val & 0xff;
	level = (val >> 8) & 0x7f;

	if (profile || level)
		rtk_update_profile_level_ctrls(ctx, profile, level);

	rtk_vcodec_dbg(2, ctx, "Sequence Header Report from RET_DEC_SEQ_HEADER_REPORT:\n");
	rtk_vcodec_dbg(2, ctx, "H264 profile : %u, level : %u\n", profile, level);

	return 0;
}

static int rtk_start_decoding(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_q_data *q_data_src, *q_data_dst;
	struct rtk_vcodec_dev *dev = ctx->dev;
	u32 src_fourcc, dst_fourcc;
	int ret;

	q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	q_data_dst = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	src_fourcc = q_data_src->fourcc;
	dst_fourcc = q_data_dst->fourcc;

	rtk_vcodec_dbg(2, ctx, "start decoding: 0x%x\n", dst_fourcc);

	if (!ctx->initialized) {
		ret = rtk_vcodec_seq_init(ctx);
		if (ret < 0) {
			printk(KERN_ALERT"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			return ret;
		}
	} else {
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

		// TODO: coda upstream
		// ctx->frame_mem_ctrl &= ~(RTK_FRAME_CHROMA_INTERLEAVE | (0x3 << 9) |
		// 			 RTK_FRAME_TILED2LINEAR);
		// if (dst_fourcc == V4L2_PIX_FMT_NV12 || dst_fourcc == V4L2_PIX_FMT_YUYV)
		// 	ctx->frame_mem_ctrl |= RTK_FRAME_CHROMA_INTERLEAVE;
		// if (ctx->tiled_map_type == RTK_TILED_FRAME_MB_RASTER_MAP)
		// 	ctx->frame_mem_ctrl |= (0x3 << 9) |
		// 		((ctx->use_vdoa) ? 0 : RTK_FRAME_TILED2LINEAR);

		// ctx->frame_mem_ctrl |= (1 << 15);
	}

	rtk_vcodec_write(dev, ctx->parabuf.paddr, BIT_PARA_BUF_ADDR);
	rtk_vcodec_write(dev, dev->tempbuf.paddr, BIT_TEMP_BUF_ADDR);

	/* Tell the decoder how many frame buffers we allocated. */
	printk(KERN_ALERT"[ctx->num_frame_buffers : %d]\n", ctx->num_frame_buffers);
	rtk_vcodec_write(dev, ctx->num_frame_buffers, CMD_SET_FRAME_BUF_NUM);

	printk(KERN_ALERT"[q_data_dst->rect.width : %d]\n", q_data_dst->rect.width);

	if (src_fourcc == V4L2_PIX_FMT_H264) {
		rtk_vcodec_write(dev, ctx->slicebuf.paddr,
				CMD_SET_FRAME_SLICE_BB_START);
		rtk_vcodec_write(dev, ctx->slicebuf.size / 1024,
				CMD_SET_FRAME_SLICE_BB_SIZE);

		rtk_vcodec_write(dev, round_up(q_data_dst->rect.width, 32),
				CMD_SET_FRAME_BUF_STRIDE);
	}

	if (src_fourcc == V4L2_PIX_FMT_VP8) {
		rtk_vcodec_write(dev, ctx->slicebuf.paddr,
				CMD_SET_FRAME_MB_BUF_BASE);

		rtk_vcodec_write(dev, round_up(q_data_dst->rect.width, 64),
				CMD_SET_FRAME_BUF_STRIDE);
	}

	/* Set secondary AXI SRAM */
	rtk_config_axi_sram(ctx);

	rtk_vcodec_write(dev, ctx->sram_info.buf_bit_use,
			CMD_SET_FRAME_AXI_BIT_ADDR);

	rtk_vcodec_write(dev, ctx->sram_info.buf_ip_ac_dc_use,
			CMD_SET_FRAME_AXI_IPACDC_ADDR);

	rtk_vcodec_write(dev, ctx->sram_info.buf_dbk_y_use,
			CMD_SET_FRAME_AXI_DBKY_ADDR);

	rtk_vcodec_write(dev, ctx->sram_info.buf_dbk_c_use,
			CMD_SET_FRAME_AXI_DBKC_ADDR);

	/* VC 1use OVL and BTP */
	// rtk_vcodec_write(dev, ctx->sram_info.buf_ovl_use,
	// 		CMD_SET_FRAME_AXI_OVL_ADDR);

	// rtk_vcodec_write(dev, ctx->sram_info.buf_btp_use,
	// 		CMD_SET_FRAME_AXI_BTP_ADDR);

	rtk_vcodec_write(dev, -1, CMD_SET_FRAME_DELAY);

	rtk_set_frame_cache(ctx, dst_fourcc);

	rtk_vcodec_write(dev, 0, CMD_SET_FRAME_MAX_DEC_SIZE);

	if (rtk_command_sync(ctx, RTK_COMMAND_SET_FRAME_BUF)) {
		v4l2_err(&ctx->dev->v4l2_dev,
			 "RTK_COMMAND_SET_FRAME_BUF timeout\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int rtk_ve1_start_decoding(struct rtk_vcodec_ctx *ctx)
{
	int ret = 0;
	struct rtk_vcodec_dev *dev = ctx->dev;

	mutex_lock(&dev->rtk_mutex);
	ret = rtk_start_decoding(ctx);
	mutex_unlock(&dev->rtk_mutex);

	return ret;
}

static int rtk_ve1_prepare_decode(struct rtk_vcodec_ctx *ctx)
{
	struct vb2_v4l2_buffer *dst_buf;
	struct rtk_vcodec_dev *dev = ctx->dev;
	struct rtk_q_data *q_data_dst, *q_data_src;
	struct rtk_meta_buffer *meta;
	u32 rot_mode = 0;
	u32 reg_addr, reg_stride;
	u32 src_fourcc;
	int ret = 0;

	rtk_vcodec_dbg(2, ctx, "prepare decode (0x%x)\n", rtk_vcodec_read(dev, RTK_REG_PRODUCT_CODE));

	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	q_data_dst = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);

	/* Try to copy source buffer contents into the bitstream ringbuffer */
	mutex_lock(&ctx->bitstream_mutex);
	rtk_feed_bitstream(ctx, NULL);
	mutex_unlock(&ctx->bitstream_mutex);

	if (rtk_get_bitstream_payload(ctx) < 512 &&
	    (!(ctx->bit_stream_param & RTK_BIT_STREAM_END_FLAG))) {
		rtk_vcodec_dbg(1, ctx, "bitstream payload: %d, skipping\n",
			 rtk_get_bitstream_payload(ctx));
		v4l2_m2m_job_finish(ctx->dev->m2m_dev, ctx->fh.m2m_ctx);
		return -EAGAIN;
	}

	/* Run rtk_start_decoding (again) if not yet initialized */
	if (!ctx->initialized) {
		rtk_vcodec_dbg(1, ctx, "Run rtk_start_decoding (again) if not yet initialized\n");

		int ret = rtk_start_decoding(ctx);

		if (ret < 0) {
			v4l2_err(&dev->v4l2_dev, "failed to start decoding\n");
			v4l2_m2m_job_finish(ctx->dev->m2m_dev, ctx->fh.m2m_ctx);
			return -EAGAIN;
		} else {
			ctx->initialized = 1;
		}
	}

	// TODO: need to compare with rtk16xxb
	// TODO: ray modify
	// if (dev->devinfo->product == CODA_960)
	// 	rtk_set_gdi_regs(ctx);

	if (ctx->use_vdoa &&
	    ctx->display_idx >= 0 &&
	    ctx->display_idx < ctx->num_frame_buffers) {
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		// TODO: refine later
		// vdoa_device_run(ctx->vdoa,
		// 		vb2_dma_contig_plane_dma_addr(&dst_buf->vb2_buf, 0),
		// 		ctx->rtk_fbs[ctx->display_idx].buf.paddr);
	} else {
		if (dev->devinfo->product == CODA_960) {
			/*
			 * It was previously assumed that the CODA960 has an
			 * internal list of 64 buffer entries that contains
			 * both the registered internal frame buffers as well
			 * as the rotator buffer output, and that the ROT_INDEX
			 * register must be set to a value between the last
			 * internal frame buffers' index and 64.
			 * At least on firmware version 3.1.1 it turns out that
			 * setting ROT_INDEX to any value >= 32 causes CODA
			 * hangups that it can not recover from with the SRC VPU
			 * reset.
			 * It does appear to work however, to just set it to a
			 * fixed value in the [ctx->num_rtk_fbs, 31]
			 * range, for example CODA_MAX_FRAMEBUFFERS.
			 */

			printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			// TODO: rtk16xxb no this register if rotate needed
			// rtk_vcodec_write(dev, RTK_MAX_FRAME_BUFFERS,
			// 	   CMD_DEC_PIC_ROT_INDEX);

			// TODO: rtk16xxb no this register if rotate needed
			// reg_addr = CMD_DEC_PIC_ROT_ADDR_Y;
			// reg_stride = CMD_DEC_PIC_ROT_STRIDE;
		} else {
			// TODO: refine later
			// reg_addr = CODA_CMD_DEC_PIC_ROT_ADDR_Y;
			// reg_stride = CODA_CMD_DEC_PIC_ROT_STRIDE;
		}

		// TODO: rtk16xxb no this register if rotate needed
		// rtk_vcodec_write_base(ctx, q_data_dst, dst_buf, reg_addr);
		// rtk_vcodec_write(dev, q_data_dst->bytesperline, reg_stride);
		// rot_mode = CODA_ROT_MIR_ENABLE | ctx->params.rot_mode;
	}

	rtk_vcodec_write(dev, 0, RET_DEC_PIC_CROP_LEFT_RIGHT);
	rtk_vcodec_write(dev, 0, RET_DEC_PIC_CROP_TOP_BOTTOM);
	rtk_vcodec_write(dev, rot_mode, CMD_DEC_PIC_ROT_MODE);
	rtk_vcodec_write(dev, 0, CMD_DEC_PIC_USER_DATA_BASE_ADDR);
	rtk_vcodec_write(dev, 0, CMD_DEC_PIC_USER_DATA_BUF_SIZE);
	rtk_vcodec_write(dev, 0, CMD_DEC_PIC_OPTION);
	rtk_vcodec_write(dev, 0, CMD_DEC_PIC_NUM_ROWS);

	rtk_vcodec_write(dev, ctx->sram_info.axi_sram_use, BIT_AXI_SRAM_USE);

	// TODO: refine later
	// spin_lock(&ctx->meta_buffer_lock);
	// meta = list_first_entry_or_null(&ctx->meta_buffer_list,
					// struct rtk_meta_buffer, list);
	// printk(KERN_INFO"[\x1b[32mmeta->sequence : %d\033[0m]\n", meta->sequence);

	// if (meta && ctx->codec->src_fourcc == V4L2_PIX_FMT_JPEG) {

	// 	/* If this is the last buffer in the bitstream, add padding */
	// 	if (meta->end == ctx->bitstream_fifo.kfifo.in) {
	// 		static unsigned char buf[512];
	// 		unsigned int pad;

	// 		/* Pad to multiple of 256 and then add 256 more */
	// 		pad = ((0 - meta->end) & 0xff) + 256;

	// 		memset(buf, 0xff, sizeof(buf));

	// 		kfifo_in(&ctx->bitstream_fifo, buf, pad);
	// 	}
	// }
	// spin_unlock(&ctx->meta_buffer_lock);

	rtk_kfifo_sync_to_device_full(ctx);

	// TODO: ray modify
	ctx->bit_stream_param |= RTK_BIT_STREAM_PICEND_MODE;
	printk(KERN_INFO"[ctx->bit_stream_param : %d]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", ctx->bit_stream_param, __func__, __LINE__);
	rtk_vcodec_write(dev, ctx->bit_stream_param, BIT_BIT_STREAM_PARAM);

	// TODO: rtk16xxb settings
	ctx->frame_mem_ctrl = (1 << 2) | (1 << 15);
	rtk_vcodec_write(dev, ctx->frame_mem_ctrl, BIT_FRAME_MEM_CTRL);

	rtk_vcodec_write(dev, 0x0, BIT_BIT_STREAM_CTRL);

	rtk_vcodec_write(dev, -1, CMD_SET_FRAME_DELAY);

	/* Clear decode success flag */
	rtk_vcodec_write(dev, 0, RET_DEC_PIC_SUCCESS);

	/* Clear error return value */
	rtk_vcodec_write(dev, 0, RET_DEC_PIC_ERR_MB);

	rtk_command_async(ctx, RTK_COMMAND_PIC_RUN);

	rtk_vcodec_dbg(2, ctx, "start picture run !\n");

	return ret;
}

static void rtk_ve1_finish_decode(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_vcodec_dev *dev = ctx->dev;
	struct rtk_q_data *q_data_src;
	struct rtk_q_data *q_data_dst;
	struct vb2_v4l2_buffer *dst_buf;
	struct rtk_meta_buffer *meta;
	int width, height;
	int decoded_idx;
	int display_idx;
	struct rtk_frame_buffers *decoded_frame = NULL;
	u32 src_fourcc;
	int success;
	u32 err_mb;
	int err_vdoa = 0;
	u32 val;

	rtk_vcodec_dbg(2, ctx, "finish decode.\n");

	if (ctx->aborting) {
		rtk_vcodec_dbg(2, ctx, "finish decode aborting.\n");
		return;
	}

	// /* Update kfifo out pointer from coda bitstream read pointer */
	rtk_kfifo_sync_from_device(ctx);

	/*
	 * in stream-end mode, the read pointer can overshoot the write pointer
	 * by up to 512 bytes
	 */
	if (ctx->bit_stream_param & RTK_BIT_STREAM_END_FLAG) {
		if (rtk_get_bitstream_payload(ctx) >= ctx->bitstream.size - 512) {
			printk(KERN_ALERT"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			kfifo_init(&ctx->bitstream_fifo,
				ctx->bitstream.vaddr, ctx->bitstream.size);
		}
	}

	q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	src_fourcc = q_data_src->fourcc;

	val = rtk_vcodec_read(dev, RET_DEC_PIC_SUCCESS);

	rtk_vcodec_dbg(2, ctx, "RET_DEC_PIC_SUCCESS = 0x%x\n", val);

	success = val & 0x1;
	if (!success) {
		rtk_vcodec_err(ctx, "decode failed\n");
	}

	if (src_fourcc == V4L2_PIX_FMT_H264) {
		if (val & (1 << 2))
			v4l2_err(&dev->v4l2_dev,
				 "insufficient slice buffer space (%d bytes)\n",
				 ctx->slicebuf.size);
	}

	val = rtk_vcodec_read(dev, RET_DEC_PIC_SIZE);
	width = (val >> 16) & 0xffff;
	height = val & 0xffff;

	printk(KERN_INFO"[\x1b[32mwidth : %d, height : %d\033[0m]\n", width, height);

	q_data_dst = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);

	/* frame crop information */
	if (src_fourcc == V4L2_PIX_FMT_H264) {
		u32 left_right;
		u32 top_bottom;

		left_right = rtk_vcodec_read(dev, RET_DEC_PIC_CROP_LEFT_RIGHT);
		top_bottom = rtk_vcodec_read(dev, RET_DEC_PIC_CROP_TOP_BOTTOM);

		if (left_right == 0xffffffff && top_bottom == 0xffffffff) {
			/* Keep current crop information */
		} else {
			struct v4l2_rect *rect = &q_data_dst->rect;

			rect->left = left_right >> 16 & 0xffff;
			rect->top = top_bottom >> 16 & 0xffff;
			rect->width = width - rect->left -
				      (left_right & 0xffff);
			rect->height = height - rect->top -
				       (top_bottom & 0xffff);

			rtk_vcodec_dbg(2, ctx, "H264 pic cropping info (%dx%d)-(%dx%d)\n",
				q_data_dst->rect.left, q_data_dst->rect.top,
				q_data_dst->rect.width, q_data_dst->rect.height);
		}
	}

	if (src_fourcc == V4L2_PIX_FMT_VP8) {
		u32 scale_info;

		scale_info = rtk_vcodec_read(dev, RET_DEC_PIC_VP8_SCALE_INFO);

		q_data_dst->rect.left = 0;
		q_data_dst->rect.top = 0;
		q_data_dst->rect.width = (scale_info >> 14) & 0x3FFF;
		q_data_dst->rect.height = (scale_info >> 0) & 0x3FFF;

		rtk_vcodec_dbg(2, ctx, "VP8 pic cropping info (%dx%d)-(%dx%d)\n",
			q_data_dst->rect.left, q_data_dst->rect.top,
			q_data_dst->rect.width, q_data_dst->rect.height);
	}

	err_mb = rtk_vcodec_read(dev, RET_DEC_PIC_ERR_MB);
	if (err_mb > 0) {
		// if (__ratelimit(&dev->mb_err_rs))
			rtk_vcodec_dbg(1, ctx, "errors in %d macroblocks\n", err_mb);
		// v4l2_ctrl_s_ctrl(ctx->mb_err_cnt_ctrl,
		// 		 v4l2_ctrl_g_ctrl(ctx->mb_err_cnt_ctrl) + err_mb);
	}

	ctx->frm_dis_flg = rtk_vcodec_read(dev,
				     RTK_REG_BIT_FRM_DIS_FLG(ctx->reg_idx));

	/* The previous display frame was copied out and can be overwritten */
	if (ctx->display_idx >= 0 &&
	    ctx->display_idx < ctx->num_frame_buffers) {

		// TODO: ray refine free fb timing
		ctx->disp_use_flg |= (1 << ctx->display_idx);
		rtk_vcodec_dbg(2, ctx, "Disp use frame bufs 0x%x\n", ctx->disp_use_flg);

		// ctx->frm_dis_flg &= ~(1 << ctx->display_idx);
		// printk(KERN_ALERT"set free idx %d\n", ctx->display_idx);
		// rtk_vcodec_write(dev, ctx->frm_dis_flg,
		// 		RTK_REG_BIT_FRM_DIS_FLG(ctx->reg_idx));
	}


	/*
	 * The index of the last decoded frame, not necessarily in
	 * display order, and the index of the next display frame.
	 * The latter could have been decoded in a previous run.
	 */
	decoded_idx = rtk_vcodec_read(dev, RET_DEC_PIC_CUR_IDX);
	display_idx = rtk_vcodec_read(dev, RET_DEC_PIC_FRAME_IDX);

	rtk_vcodec_dbg(2, ctx, "decode(%d) display(%d) ctx->frm_dis_flg(0x%x)\n",
		decoded_idx, display_idx, ctx->frm_dis_flg);

	if (decoded_idx == RET_DEC_PIC_NOT_ENOUGH_FRAME_BUFFER) {
		/* no frame was decoded, but we might have a display frame */
		if (display_idx >= 0 && display_idx < ctx->num_frame_buffers) {
			ctx->sequence_offset++;
		} else if (ctx->display_idx < 0) {
			if (ctx->display_idx == RET_DEC_PIC_DISPLAY_DELAY) {
				/* can't hold when display delay happened */
				rtk_vcodec_dbg(0, ctx, "display delay happened because of picture ordering\n");
			} else {
				ctx->hold = true;
				v4l2_err(&dev->v4l2_dev,
					"hold when ctx->display_idx = %d\n", ctx->display_idx);
			}
		}
	} else if (decoded_idx == RET_DEC_PIC_NO_DATA) {
		if (ctx->display_idx >= 0 &&
		    ctx->display_idx < ctx->num_frame_buffers) {
			ctx->sequence_offset++;
		}
		/* no frame was decoded, we still return remaining buffers */
	} else if (decoded_idx < 0 || decoded_idx >= ctx->num_frame_buffers) {
		v4l2_err(&dev->v4l2_dev,
			 "decoded frame index out of range: %d\n", decoded_idx);
	} else {
		int sequence;
		bool interlace_frame = false;

		rtk_vcodec_dbg(2, ctx, "Has decoded frame buf %d\n", decoded_idx);

		decoded_frame = &ctx->rtk_fbs[decoded_idx];

		val = rtk_vcodec_read(dev, RET_DEC_PIC_FRAME_NUM);
		rtk_vcodec_dbg(2, ctx, "Already decoded %d frames\n", val);

		if (ctx->sequence_offset == -1) {
			ctx->sequence_offset = val;
			printk(KERN_INFO"22222 [\x1b[32mctx->sequence_offset : %d\033[0m]\n", ctx->sequence_offset);
		}

		sequence = val + ctx->first_frame_sequence
			       - ctx->sequence_offset;

		rtk_vcodec_dbg(2, ctx, "ctx->first_frame_sequence : %d\n", ctx->first_frame_sequence);
		rtk_vcodec_dbg(2, ctx, "sequence : %d, val : %d\n", sequence, val);

		// check is interlace frame or not
		if (rtk_vcodec_read(dev, RET_DEC_PIC_TYPE) & 0x40000)
			interlace_frame = true;

remove_meta:
		spin_lock(&ctx->meta_buffer_lock);
		if (!list_empty(&ctx->meta_buffer_list)) {
			meta = list_first_entry(&ctx->meta_buffer_list,
					      struct rtk_meta_buffer, list);
			list_del(&meta->list);
			ctx->num_metas--;

			rtk_vcodec_dbg(4, ctx, "remove meta sequence : %d%s\n",
						meta->sequence, (meta->last) ? " (last)" : "");
			rtk_vcodec_dbg(4, ctx, "meta start : 0x%x, meta->end : 0x%x\n", meta->start, meta->end);
			rtk_vcodec_dbg(4, ctx, "remaining num_metas : %d\n", ctx->num_metas);

			spin_unlock(&ctx->meta_buffer_lock);
			/*
			 * Clamp counters to 16 bits for comparison, as the HW
			 * counter rolls over at this point for h.264. This
			 * may be different for other formats, but using 16 bits
			 * should be enough to detect most errors and saves us
			 * from doing different things based on the format.
			 */
			if ((sequence & 0xffff) != (meta->sequence & 0xffff)) {
				v4l2_err(&dev->v4l2_dev,
					 "sequence number mismatch (%d(%d) != %d)\n",
					 sequence, ctx->sequence_offset,
					 meta->sequence);
			}
			decoded_frame->meta = *meta;
			kfree(meta);

			if (interlace_frame) {
				rtk_vcodec_dbg(0, ctx, "remove another meta containing field\n");
				interlace_frame = false;
				goto remove_meta;
			}
		} else {
			printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			spin_unlock(&ctx->meta_buffer_lock);
			v4l2_err(&dev->v4l2_dev, "empty timestamp list!\n");
			memset(&decoded_frame->meta, 0,
			       sizeof(struct rtk_meta_buffer));
			decoded_frame->meta.sequence = sequence;
			decoded_frame->meta.last = false;
			ctx->sequence_offset++;
		}

		val = rtk_vcodec_read(dev, RET_DEC_PIC_TYPE) & 0x7;
		decoded_frame->type = (val == 0) ? V4L2_BUF_FLAG_KEYFRAME :
				      (val == 1) ? V4L2_BUF_FLAG_PFRAME :
						   V4L2_BUF_FLAG_BFRAME;

		decoded_frame->error = err_mb;
	}

	if (display_idx == RET_DEC_PIC_NO_DECODE_AND_DISPLAY) {
		/*
		 * no more frames to be decoded, but there could still
		 * be rotator output to dequeue
		 */
		rtk_vcodec_dbg(2, ctx, "no more frames to be decode and display\n");

		ctx->hold = true;
	} else if (display_idx == RET_DEC_PIC_DISPLAY_DELAY) {
		/* possibly prescan failure */
	} else if (display_idx < 0 || display_idx >= ctx->num_frame_buffers) {
		v4l2_err(&dev->v4l2_dev,
			 "presentation frame index out of range: %d\n",
			 display_idx);
	}

	/* If a frame was copied out, return it */
	if (ctx->display_idx >= 0 &&
	    ctx->display_idx < ctx->num_frame_buffers) {
		struct rtk_frame_buffers *ready_frame;

		rtk_vcodec_dbg(2, ctx, "Has ready frame buf %d\n", ctx->display_idx);

		ready_frame = &ctx->rtk_fbs[ctx->display_idx];

		dst_buf = v4l2_m2m_dst_buf_remove_by_idx(ctx->fh.m2m_ctx, ctx->display_idx);

		dst_buf->sequence = ctx->osequence++;

		dst_buf->field = V4L2_FIELD_NONE;
		dst_buf->flags &= ~(V4L2_BUF_FLAG_KEYFRAME |
					     V4L2_BUF_FLAG_PFRAME |
					     V4L2_BUF_FLAG_BFRAME);
		dst_buf->flags |= ready_frame->type;
		meta = &ready_frame->meta;
		if (meta->last && !rtk_reorder_enable(ctx)) {
			/*
			 * If this was the last decoded frame, and reordering
			 * is disabled, this will be the last display frame.
			 */
			rtk_vcodec_dbg(1, ctx, "last meta, marking %d as last frame\n",
				dst_buf->vb2_buf.index);
			dst_buf->flags |= V4L2_BUF_FLAG_LAST;
		} else if (ctx->bit_stream_param & RTK_BIT_STREAM_END_FLAG &&
			   display_idx == -1) {
			/*
			 * If there is no designated presentation frame anymore,
			 * this frame has to be the last one.
			 */
			rtk_vcodec_dbg(1, ctx,
				 "no more frames to return, marking %d as last frame\n",
				 	dst_buf->vb2_buf.index);
			dst_buf->flags |= V4L2_BUF_FLAG_LAST;
		}
		dst_buf->timecode = meta->timecode;
		dst_buf->vb2_buf.timestamp = meta->timestamp;

		vb2_set_plane_payload(&dst_buf->vb2_buf, 0,
				      q_data_dst->sizeimage);

		if (ready_frame->error || err_vdoa) {
			rtk_vcodec_dbg(2, ctx, "Set ready frame %d error\n", dst_buf->vb2_buf.index);
			rtk_vcodec_m2m_buf_done(ctx, dst_buf, VB2_BUF_STATE_ERROR);
		} else {
			rtk_vcodec_dbg(2, ctx, "Set ready frame %d done\n", dst_buf->vb2_buf.index);
			rtk_vcodec_m2m_buf_done(ctx, dst_buf, VB2_BUF_STATE_DONE);
		}

		if (decoded_frame) {
			rtk_vcodec_dbg(1, ctx, "job finished: decoded %c frame %u, returned %c frame %u (%u/%u)%s\n",
				 rtk_frame_type(decoded_frame->type),
				 decoded_frame->meta.sequence,
				 rtk_frame_type(dst_buf->flags),
				 ready_frame->meta.sequence,
				 dst_buf->sequence, ctx->qsequence,
				 (dst_buf->flags & V4L2_BUF_FLAG_LAST) ?
				 " (last)" : "");
		} else {
			rtk_vcodec_dbg(1, ctx, "job finished: no frame decoded (%d), returned %c frame %u (%u/%u)%s\n",
				 decoded_idx,
				 rtk_frame_type(dst_buf->flags),
				 ready_frame->meta.sequence,
				 dst_buf->sequence, ctx->qsequence,
				 (dst_buf->flags & V4L2_BUF_FLAG_LAST) ?
				 " (last)" : "");
		}
	} else {
		if (decoded_frame) {
			rtk_vcodec_dbg(1, ctx, "job finished: decoded %c frame %u, no frame returned (%d)\n",
				 rtk_frame_type(decoded_frame->type),
				 decoded_frame->meta.sequence,
				 ctx->display_idx);
		} else {
			rtk_vcodec_dbg(1, ctx, "job finished: no frame decoded (%d) or returned (%d)\n",
				 decoded_idx, ctx->display_idx);
		}
	}

	// /* The rotator will copy the current display frame next time */
	ctx->display_idx = display_idx;

	/*
	 * The current decode run might have brought the bitstream fill level
	 * below the size where we can start the next decode run. As userspace
	 * might have filled the output queue completely and might thus be
	 * blocked, we can't rely on the next qbuf to trigger the bitstream
	 * refill. Check if we have data to refill the bitstream now.
	 */
	mutex_lock(&ctx->bitstream_mutex);
	rtk_feed_bitstream(ctx, NULL);
	mutex_unlock(&ctx->bitstream_mutex);
}

static void rtk_ve1_decode_timeout(struct rtk_vcodec_ctx *ctx)
{
	printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	// TODO: refine later

	// struct vb2_v4l2_buffer *dst_buf;

	/*
	 * For now this only handles the case where we would deadlock with
	 * userspace, i.e. userspace issued DEC_CMD_STOP and waits for EOS,
	 * but after a failed decode run we would hold the context and wait for
	 * userspace to queue more buffers.
	 */
	// if (!(ctx->bit_stream_param & RTK_BIT_STREAM_END_FLAG))
	// 	return;

	// dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	// dst_buf->sequence = ctx->qsequence - 1;

	// coda_m2m_buf_done(ctx, dst_buf, VB2_BUF_STATE_ERROR);
}

static void rtk_ve1_seq_init_work(struct work_struct *work)
{
	struct rtk_vcodec_ctx *ctx = container_of(work,
					    struct rtk_vcodec_ctx, seq_init_work);
	struct rtk_vcodec_dev *dev = ctx->dev;

	mutex_lock(&ctx->buffer_mutex);
	mutex_lock(&dev->rtk_mutex);

	if (!ctx->initialized) {
		rtk_vcodec_seq_init(ctx);
	}

	mutex_unlock(&dev->rtk_mutex);
	mutex_unlock(&ctx->buffer_mutex);
}

static void rtk_ve1_seq_end_work(struct work_struct *work)
{
	struct rtk_vcodec_ctx *ctx = container_of(work, struct rtk_vcodec_ctx, seq_end_work);
	struct rtk_vcodec_dev *dev = ctx->dev;

	mutex_lock(&ctx->buffer_mutex);
	mutex_lock(&dev->rtk_mutex);

	if (ctx->initialized == 0)
		goto out;

	rtk_vcodec_dbg(1, ctx, "%s: sent command 'SEQ_END' to coda\n", __func__);
	if (rtk_command_sync(ctx, RTK_COMMAND_SEQ_END)) {
		v4l2_err(&dev->v4l2_dev,
			 "RTK_COMMAND_SEQ_END timeout\n");
	}

	/*
	 * FIXME: Sometimes h.264 encoding fails with 8-byte sequences missing
	 * from the output stream after the h.264 decoder has run. Resetting the
	 * hardware after the decoder has finished seems to help.
	 */
	// TODO: refine later
	// if (dev->devinfo->product == CODA_960)
	// 	coda_hw_reset(ctx);

	kfifo_init(&ctx->bitstream_fifo,
		ctx->bitstream.vaddr, ctx->bitstream.size);

	ctx->initialized = 0;

out:
	mutex_unlock(&dev->rtk_mutex);
	mutex_unlock(&ctx->buffer_mutex);
}

static void rtk_ve1_release(struct rtk_vcodec_ctx *ctx)
{
	rtk_vcodec_dbg(1, ctx, "rtk_ve1_release\n");
	mutex_lock(&ctx->buffer_mutex);
	rtk_vcodec_free_ctx_buffers(ctx);
	rtk_vcodec_free_mvcol_buffers(ctx);
	rtk_free_bitstream_buffer(ctx);
	mutex_unlock(&ctx->buffer_mutex);
}

const struct rtk_context_ops rtk_ve1_decoder_ops = {
	// .queue_init = rtk_decoder_queue_init,
	.reqbufs = rtk_decoder_reqbufs,
	.start_streaming = rtk_ve1_start_decoding,
	.prepare_run = rtk_ve1_prepare_decode,
	.finish_run = rtk_ve1_finish_decode,
	.run_timeout = rtk_ve1_decode_timeout,
	.seq_init_work = rtk_ve1_seq_init_work,
	// .seq_end_work = rtk_ve1_seq_end_work,
	// .release = rtk_ve1_release,
};

const struct rtk_context_common_ops rtk_common_ops = {
	.queue_init = rtk_decoder_queue_init,
	.seq_end_work = rtk_ve1_seq_end_work,
	.release = rtk_ve1_release,
};
