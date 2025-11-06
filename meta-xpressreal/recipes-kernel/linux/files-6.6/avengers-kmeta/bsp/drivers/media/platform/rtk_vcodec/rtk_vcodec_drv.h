/*
 * Realtek video decoder v4l2 driver
 *
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 *
 */

#ifndef _RTK_VCODEC_DRV_H_
#define _RTK_VCODEC_DRV_H_

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

#define RTK_MIN_FRAME_BUFFERS 1
#define RTK_MAX_FRAME_BUFFERS 32
#define RTK_LINEAR_FRAME_MAP 0
#define RTK_TILED_FRAME_MB_RASTER_MAP 1

enum {
	V4L2_M2M_SRC_Q_DATA = 0,
	V4L2_M2M_DST_Q_DATA = 1,
};


enum rtk_vcodec_ctx_type {
	RTK_VCODEC_CTX_ENCODER,
	RTK_VCODEC_CTX_DECODER,
};

enum rtk_vcodec_product {
	// TODO: support other coda ?
	// CODA_DX6 = 0xf001,
	// CODA_HX4 = 0xf00a,
	// CODA_7541 = 0xf012,
	CODA_960 = 0xf020,
	CODA_980 = 0xf021,
};

/**
* @brief enumeration type for interrupt bit for CODA series.
*/
enum int_bit {
	INT_BIT_INIT = 0,
	INT_BIT_SEQ_INIT = 1,
	INT_BIT_SEQ_END = 2,
	INT_BIT_PIC_RUN = 3,
	INT_BIT_FRAMEBUF_SET = 4,
	INT_BIT_ENC_HEADER = 5,
	INT_BIT_DEC_PARA_SET = 7,
	INT_BIT_DEC_BUF_FLUSH = 8,
	INT_BIT_USERDATA = 9,
	INT_BIT_DEC_FIELD = 10,
	INT_BIT_DEC_MB_ROWS = 13,
	INT_BIT_BIT_BUF_EMPTY = 14,
	INT_BIT_BIT_BUF_FULL = 15
};

/**
* @brief enumeration type for interrupt bit reasons for CODA series.
*/
enum int_bit_reason {
	INT_BIT_REASON_INIT = 0,
	INT_BIT_REASON_SEQ_INIT = 1,
	INT_BIT_REASON_SEQ_END = 2,
	INT_BIT_REASON_PIC_RUN = 3,
	INT_BIT_REASON_FRAMEBUF_SET = 4,
	INT_BIT_REASON_ENC_HEADER = 5,
	INT_BIT_REASON_DEC_PARA_SET = 7,
	INT_BIT_REASON_DEC_BUF_FLUSH = 8,
	INT_BIT_REASON_USERDATA = 9,
	INT_BIT_REASON_DEC_FIELD = 10,
	INT_BIT_REASON_DEC_MB_ROWS = 13,
	INT_BIT_REASON_BIT_BUF_EMPTY = 14,
	INT_BIT_REASON_BIT_BUF_FULL = 15
};

struct rtk_video_device;

struct rtk_devinfo {
	char			*firmware[3];
	enum rtk_vcodec_product	product;
	const struct rtk_codec	*codecs;
	unsigned int		num_codecs;
	const struct rtk_video_device **vdevs;
	unsigned int		num_vdevs;
	size_t			workbuf_size;
	size_t			tempbuf_size;
	size_t			sram_size;
};


struct rtk_extra_buf {
	void			*vaddr;
	dma_addr_t		paddr;
	u32			size;
	struct debugfs_blob_wrapper blob;
	struct dentry		*dentry;
};

struct rtk_vcodec_dev {
	struct v4l2_device	v4l2_dev;
	struct video_device	vfd[2];
	struct device		*dev;
	const struct rtk_devinfo *devinfo;
	int			firmware;
	// TODO: rtk support vdoa or not ?
	// struct vdoa_data	*vdoa;

	void __iomem		*regs_base;
	struct reset_control	*rstc;

	struct rtk_extra_buf	codebuf;
	struct rtk_extra_buf	tempbuf;
	struct rtk_extra_buf	workbuf;
	struct rtk_extra_buf	sram;

	struct mutex		dev_mutex;
	struct mutex		rtk_mutex;
	struct workqueue_struct	*workqueue;
	struct v4l2_m2m_dev	*m2m_dev;
	struct ida		ida;
	struct dentry		*debugfs_root;
	struct ratelimit_state	mb_err_rs;
};

struct rtk_codec {
	u32 mode;
	u32 src_fourcc;
	u32 dst_fourcc;
	u32 max_w;
	u32 max_h;
};

// TODO: rtk support jpeg or not ?
// struct coda_huff_tab;

struct rtk_params {
	u8			rot_mode;
	u8			h264_intra_qp;
	u8			h264_inter_qp;
	u8			h264_min_qp;
	u8			h264_max_qp;
	u8			h264_disable_deblocking_filter_idc;
	s8			h264_slice_alpha_c0_offset_div2;
	s8			h264_slice_beta_offset_div2;
	bool			h264_constrained_intra_pred_flag;
	s8			h264_chroma_qp_index_offset;
	u8			h264_profile_idc;
	u8			h264_level_idc;
	u8			mpeg2_profile_idc;
	u8			mpeg2_level_idc;
	u8			mpeg4_intra_qp;
	u8			mpeg4_inter_qp;
	u8			gop_size;
	int			intra_refresh;
	// TODO: rtk support jpeg or not ?
	// enum v4l2_jpeg_chroma_subsampling jpeg_chroma_subsampling;
	// u8			jpeg_quality;
	// u8			jpeg_restart_interval;
	// u8			*jpeg_qmat_tab[3];
	// int			jpeg_qmat_index[3];
	// int			jpeg_huff_dc_index[3];
	// int			jpeg_huff_ac_index[3];
	// u32			*jpeg_huff_data;
	// struct coda_huff_tab	*jpeg_huff_tab;
	int			codec_mode;
	int			codec_mode_aux;
	enum v4l2_mpeg_video_multi_slice_mode slice_mode;
	u32			framerate;
	u16			bitrate;
	u16			vbv_delay;
	u32			vbv_size;
	u32			slice_max_bits;
	u32			slice_max_mb;
	bool			force_ipicture;
	bool			gop_size_changed;
	bool			bitrate_changed;
	bool			framerate_changed;
	bool			h264_intra_qp_changed;
	bool			intra_refresh_changed;
	bool			slice_mode_changed;
	bool			frame_rc_enable;
	bool			mb_rc_enable;
};

struct rtk_meta_buffer {
	struct list_head	list;
	u32			sequence;
	struct v4l2_timecode	timecode;
	u64			timestamp;
	unsigned int		start;
	unsigned int		end;
	bool			last;
};

struct rtk_q_data {
	unsigned int		width;
	unsigned int		height;
	unsigned int		bytesperline;
	unsigned int		sizeimage;
	unsigned int		fourcc;
	struct v4l2_rect	rect;
	unsigned int		ori_width;
	unsigned int		ori_height;
};

struct rtk_axi_sram_info {
	u32		axi_sram_use;
	phys_addr_t	buf_bit_use;
	phys_addr_t	buf_ip_ac_dc_use;
	phys_addr_t	buf_dbk_y_use;
	phys_addr_t	buf_dbk_c_use;
	phys_addr_t	buf_ovl_use;
	phys_addr_t	buf_btp_use;
	int		remaining;
	phys_addr_t	next_paddr;
};

struct rtk_vcodec_ctx;

struct rtk_context_ops {
	// int (*queue_init)(void *priv, struct vb2_queue *src_vq,
	// 		  struct vb2_queue *dst_vq);
	int (*reqbufs)(struct rtk_vcodec_ctx *ctx, struct v4l2_requestbuffers *rb);
	int (*start_streaming)(struct rtk_vcodec_ctx *ctx);
	int (*prepare_run)(struct rtk_vcodec_ctx *ctx);
	void (*finish_run)(struct rtk_vcodec_ctx *ctx);
	void (*run_timeout)(struct rtk_vcodec_ctx *ctx);
	void (*seq_init_work)(struct work_struct *work);
	// void (*seq_end_work)(struct work_struct *work);
	// void (*release)(struct rtk_vcodec_ctx *ctx);
};

struct rtk_context_common_ops {
	int (*queue_init)(void *priv, struct vb2_queue *src_vq,
			  struct vb2_queue *dst_vq);
	void (*seq_end_work)(struct work_struct *work);
	void (*release)(struct rtk_vcodec_ctx *ctx);
};

struct rtk_frame_buffers {
	struct rtk_extra_buf		buf;
	struct rtk_meta_buffer	meta;
	u32				type;
	u32				error;
};

struct rtk_vcodec_ctx {
	struct rtk_vcodec_dev			*dev;
	struct mutex			buffer_mutex;
	struct work_struct		pic_run_work;
	struct work_struct		seq_init_work;
	struct work_struct		seq_end_work;
	struct completion		completion;
	const struct rtk_video_device	*cvd;
	const struct rtk_context_ops	*ops;
	const struct rtk_context_common_ops	*common_ops;
	int				aborting;
	int				initialized;
	int				streamon_out;
	int				streamon_cap;
	u32				qsequence;
	u32				osequence;
	u32				sequence_offset;
	struct rtk_q_data		q_data[2];
	enum rtk_vcodec_ctx_type		inst_type;
	const struct rtk_codec		*codec;
	enum v4l2_colorspace		colorspace;
	enum v4l2_xfer_func		xfer_func;
	enum v4l2_ycbcr_encoding	ycbcr_enc;
	enum v4l2_quantization		quantization;
	struct rtk_params		params;
	struct v4l2_ctrl_handler	ctrls;
	struct v4l2_ctrl		*h264_profile_ctrl;
	struct v4l2_ctrl		*h264_level_ctrl;
	struct v4l2_ctrl		*mpeg2_profile_ctrl;
	struct v4l2_ctrl		*mpeg2_level_ctrl;
	struct v4l2_ctrl		*mpeg4_profile_ctrl;
	struct v4l2_ctrl		*mpeg4_level_ctrl;
	struct v4l2_ctrl		*mb_err_cnt_ctrl;
	struct v4l2_fh			fh;
	int				gopcounter;
	int				runcounter;
	int				jpeg_ecs_offset;
	char				vpu_header[3][64];
	int				vpu_header_size[3];
	struct kfifo			bitstream_fifo;
	struct mutex			bitstream_mutex;
	struct rtk_extra_buf		bitstream;
	bool				hold;
	struct rtk_extra_buf		parabuf;
	struct rtk_extra_buf		mvcolbuf[RTK_MAX_FRAME_BUFFERS];
	struct rtk_extra_buf		slicebuf;
	struct rtk_frame_buffers	rtk_fbs[RTK_MAX_FRAME_BUFFERS];
	struct list_head		meta_buffer_list;
	spinlock_t			meta_buffer_lock;
	int				num_metas;
	unsigned int			first_frame_sequence;
	struct rtk_extra_buf		workbuf;
	int				min_frame_buffers;
	int				num_frame_buffers;
	int				idx;
	int				reg_idx;
	struct rtk_axi_sram_info		sram_info;
	int				tiled_map_type;
	u32				bit_stream_param;
	u32				frm_dis_flg;
	// TODO: ray refine free fb timing
	u32				disp_use_flg;
	u32				frame_mem_ctrl;
	u32				para_change;
	int				display_idx;
	struct dentry			*debugfs_entry;
	bool				use_bit;
	bool				use_vdoa;
	// TODO: rtk support vdoa or not ?
	// struct vdoa_ctx			*vdoa;
	/*
	 * wakeup mutex used to serialize encoder stop command and finish_run,
	 * ensures that finish_run always either flags the last returned buffer
	 * or wakes up the capture queue to signal EOS afterwards.
	 */
	struct mutex			wakeup_mutex;
};

extern int rtk_vcodec_debug;

#define rtk_vcodec_dbg(level, ctx, fmt, arg...)				\
	do {								\
		if (rtk_vcodec_debug >= (level))				\
			v4l2_dbg((level), rtk_vcodec_debug, &(ctx)->dev->v4l2_dev, \
			 "%u: " fmt, (ctx)->idx, ##arg);		\
	} while (0)

#define rtk_vcodec_err(ctx, fmt, arg...)				\
	do {								\
		v4l2_err(&(ctx)->dev->v4l2_dev, \
		 "%u: " fmt, (ctx)->idx, ##arg);		\
	} while (0)

int rtk_vcodec_alloc_extra_buf(struct rtk_vcodec_dev *dev, struct rtk_extra_buf *buf,
				size_t size, const char *name, struct dentry *parent);

void rtk_vcodec_free_extra_buf(struct rtk_vcodec_dev *dev, struct rtk_extra_buf *buf,
				const char *name);

static inline struct rtk_q_data *rtk_get_q_data(struct rtk_vcodec_ctx *ctx,
					     enum v4l2_buf_type type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return &(ctx->q_data[V4L2_M2M_SRC_Q_DATA]);
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return &(ctx->q_data[V4L2_M2M_DST_Q_DATA]);
	default:
		return NULL;
	}
}

static inline unsigned rtk_get_bitstream_payload(struct rtk_vcodec_ctx *ctx)
{
	return kfifo_len(&ctx->bitstream_fifo);
}

/*
 * The bitstream prefetcher needs to read at least 2 256 byte periods past
 * the desired bitstream position for all data to reach the decoder.
 */
static inline bool rtk_bitstream_can_fetch_past(struct rtk_vcodec_ctx *ctx,
						 unsigned int pos)
{
	return (int)(ctx->bitstream_fifo.kfifo.in - ALIGN(pos, 256)) > 512;
}

bool rtk_bitstream_can_fetch_past(struct rtk_vcodec_ctx *ctx, unsigned int pos);

void rtk_vcodec_m2m_buf_done(struct rtk_vcodec_ctx *ctx, struct vb2_v4l2_buffer *buf,
		       enum vb2_buffer_state state);

extern const struct rtk_context_ops rtk_ve1_decoder_ops;
extern const struct rtk_context_ops rtk_ve2_decoder_ops;

extern const struct rtk_context_common_ops rtk_common_ops;

#endif /* _RTK_VCODEC_DRV_H_ */
