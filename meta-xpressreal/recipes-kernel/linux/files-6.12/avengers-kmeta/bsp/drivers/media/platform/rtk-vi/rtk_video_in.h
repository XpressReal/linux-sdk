/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef RTK_VIDEO_IN_H_
#define RTK_VIDEO_IN_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/videodev2.h>
#include <linux/dma-map-ops.h>
#include <linux/mm_types.h>
#include <linux/fdtable.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/sys_soc.h>

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>


#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include <soc/realtek/rtk_refclk.h>

#include "vi_reg.h"

#define SOC_ID_KENT    0
#define SOC_ID_PRINCE  1

/* DTS Extension Flags, for of_property ext-flags */
#define RTK_EXT_DFT_MIRROR      (1 << 0)

#define CH_0  0
#define CH_1  1
#define CH_2  2
#define CH_3  3

#define ENTRY_0  0
#define ENTRY_1  1
#define ENTRY_2  2
#define ENTRY_3  3

#define DISABLE 0
#define ENABLE  1

#define PROGRESSIVE_MODE  0
#define INTERLACE_MODE    1

#define PREAMBLE_3WORD_MODE  0 /* BT656, FF,00,00,XY */
#define PREAMBLE_6WORD_MODE  1 /* BT1120, FF,FF,00,00,00,00,XY,XY */

/* input fmt */
#define YC8BIT_CBY  0
#define YC8BIT_YCB  1
#define YC16BIT_BA  0
#define YC16BIT_AB  1

/*
 * of_property pad-cfg
 * pad_cfg of struct rtk_vi
 * Config of 8 bit mode pad sequence
 * 0: data seq - 7 6 5 ... 2 1 0
 * 1: data seq - 0 1 2 ... 5 6 7
 * 2: data seq - 3 2 1 0 7 6 5 4
 * 3: data seq - 4 5 6 7 0 1 2 3
 */
#define PAD_CFG_00  0
#define PAD_CFG_01  1
#define PAD_CFG_10  2
#define PAD_CFG_11  3

/* cfg_16bit pad data seq */
#define CFG_DESCEND 0
#define CFG_ASCEND  1

/* of_property bt-mode */
#define MODE_BT656   PREAMBLE_3WORD_MODE
#define MODE_BT1120  PREAMBLE_6WORD_MODE
/*
 * separate_mode of struct rtk_vi
 * Bit[0] fmt_seperate
 * Bit[1] input_fmt
 * Bit[2] pad_data_cfg_16bit
 * 0: Non-separate mode
 * 1: Y - PAD_B, C - PAD_A, data seq - 15 14 13 ... 2 1 0
 * 3: Y - PAD_A, C - PAD_B, data seq - 15 14 13 ... 2 1 0
 * 5: Y - PAD_B, C - PAD_A, data seq - 0 1 2 ... 13 14 15
 * 7: Y - PAD_A, C - PAD_B, data seq - 0 1 2 ... 13 14 15
 */
#define SEP_NONE        0
#define SEP_CY_DESCEND  1
#define SEP_YC_DESCEND  3
#define SEP_CY_ASCEND   5
#define SEP_YC_ASCEND   7

/* of_property cascade-mode */
#define CASCADE_OFF    0
#define CASCADE_MASTER 1
#define CASCADE_SLAVE  2
#define CASCADE_4CH    3

#define RTK_V4L2_CID_VI_MIRROR		(V4L2_CID_PRIVATE_BASE + 0)

struct rtk_vi;

struct rtk_vi_soc {
	u8 id;
	u32 (*get_macreg_offset)(u8 ch_index);
	/* register offset */
	u32 fc;
	u32 cascade;
	u32 scpu_inten0;
	u32 scpu_inten1;
	u32 scpu_inten2;
	u32 scpu_inten3;
	u32 scpu_inten4;
	u32 scpu_inten5;
	u32 scpu_inten6;
	u32 scpu_inten7;
	u32 scpu_inten8;
	u32 intst0;
	u32 intst1;
	u32 intst2;
	u32 intst3;
	u32 intst4;
	u32 intst5;
	u32 intst6;
	u32 intst7;
	u32 intst8;
	u32 rst_ctrl;
	u32 crc_ctrl;
	/* register offset -mac */
	u32 decoder;
	u32 img;
	u32 img_i;
	u32 rawdata;
	u32 vanc;
	u32 vanc_p;
	u32 vanc_i;
	u32 vanc_seq_pitch;
	u32 isp_cfg;
	u32 isp_sel;
	u32 isp_seq_pitch_y;
	u32 isp_seq_pitch_c;
	u32 isp_vsi;
	u32 isp_vsd;
	u32 isp_vsd_h;
	u32 isp_vsd_tf;
	u32 isp_vsd_h_tf;
	u32 isp_vsd_bf;
	u32 isp_vsd_h_bf;
	u32 isp_vsyc_0;
	u32 isp_vsyc_1;
	u32 isp_vsyc_2;
	u32 isp_vsyc_3;
	u32 isp_vsyc_4;
	u32 isp_vsyc_5;
	u32 isp_vsyc_6;
	u32 isp_vsyc_7;
	u32 isp_vscc_0;
	u32 isp_vscc_1;
	u32 isp_vscc_2;
	u32 isp_vscc_3;
	u32 isp_vscc_4;
	u32 isp_vscc_5;
	u32 isp_vscc_6;
	u32 isp_vscc_7;
	u32 isp_hsd;
	u32 isp_hsd_w;
	u32 isp_hsyc_0;
	u32 isp_hsyc_1;
	u32 isp_hsyc_2;
	u32 isp_hsyc_3;
	u32 isp_hsyc_4;
	u32 isp_hsyc_5;
	u32 isp_hsyc_6;
	u32 isp_hsyc_7;
	u32 isp_hsyc_8;
	u32 isp_hsyc_9;
	u32 isp_hsyc_10;
	u32 isp_hsyc_11;
	u32 isp_hsyc_12;
	u32 isp_hsyc_13;
	u32 isp_hsyc_14;
	u32 isp_hsyc_15;
	u32 isp_hscc_0;
	u32 isp_hscc_1;
	u32 isp_hscc_2;
	u32 isp_hscc_3;
	u32 isp_hscc_4;
	u32 isp_hscc_5;
	u32 isp_hscc_6;
	u32 isp_hscc_7;
	u32 bt_pdi_fc;
	u32 bt_pdi;
	u32 bt_vanc_i;
	u32 bt_tf_img;
	u32 bt_bf_img;
	u32 bt_vanc_p;
	u32 bt_p_img;
	u32 adr_entry0_y_sa;
	u32 adr_entry0_y_ea;
	u32 adr_entry0_c_sa;
	u32 adr_entry0_c_ea;
	u32 adr_entry0_vanc_sa;
	u32 adr_entry0_vanc_ea;
	u32 adr_entry0_tf_y_sa;
	u32 adr_entry0_tf_y_ea;
	u32 adr_entry0_tf_c_sa;
	u32 adr_entry0_tf_c_ea;
	u32 adr_entry0_tf_vanc_sa;
	u32 adr_entry0_tf_vanc_ea;
	u32 adr_entry0_bf_y_sa;
	u32 adr_entry0_bf_y_ea;
	u32 adr_entry0_bf_c_sa;
	u32 adr_entry0_bf_c_ea;
	u32 adr_entry0_bf_vanc_sa;
	u32 adr_entry0_bf_vanc_ea;
	u32 adr_entry0_st;
	u32 adr_entry3_st;
};

struct rtk_vi_ops {
	void (*mac_rst)(struct rtk_vi *vi, u8 ch_index,	u8 enable);
	void (*mac_ctrl)(struct rtk_vi *vi, u8 ch_index, u8 enable);
	void (*scale_down)(struct rtk_vi *vi);
	void (*decoder_cfg)(struct rtk_vi *vi);
	void (*isp_cfg)(struct rtk_vi *vi);
	void (*packet_det_ctrl)(struct rtk_vi *vi, u8 enable);
	bool (*get_det_result)(struct rtk_vi *vi);
	void (*interrupt_ctrl)(struct rtk_vi *vi, u8 ch_index, u8 enable);
	void (*dma_buf_cfg)(struct rtk_vi *vi, u8 entry_index, u64 start_addr);
	int  (*state_isr)(struct rtk_vi *vi, u8 ch_index, u32 *inst_a, u32 *inst_b);
	bool (*is_frame_done)(u8 entry_index, u32 inst_a, u32 inst_b);
	void (*clear_done_flag)(struct rtk_vi *vi, u8 ch_index, u8 entry_index);
	void (*clear_mac_ovf_flags)(struct rtk_vi *vi,
		u8 ch_index, u32 inst_a, u32 inst_b);
	void (*clear_mac_ovr_flags)(struct rtk_vi *vi,
		u8 ch_index, u32 inst_a, u32 inst_b);
	void (*dump_registers)(struct rtk_vi *vi, u32 start_offset, u32 end_offset);
};

#define to_rtk_vi(x) container_of(x, struct rtk_vi, x)

struct rtk_vi {
	struct reset_control *reset_vi;
	struct clk *clk_vi;
	struct regmap *vi_reg;
	int irq;
	u8 max_ch;
	u8 ch_index;

	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	struct vb2_queue queue;
	struct v4l2_pix_format pix_fmt;
	struct v4l2_bt_timings detected_timings;
	unsigned int v4l2_input_status;
	struct mutex video_lock;

	struct list_head buffers;
	struct mutex buffer_lock; /* buffer list lock */
	u32 sequence;
	u32 drop_cnt;
	u32 ovf_cnt;

	struct rtk_vi_soc soc;
	const struct rtk_vi_ops *hw_ops;
	struct rtk_vi_buffer *cur_buf[4];

	bool hw_init_done;
	bool is_interlace;
	bool en_ecc;
	bool en_bt_pdi;
	bool en_crc;
	bool en_mirror;
	u32 src_width;
	u32 src_height;
	u32 dst_width;
	u32 dst_height;
	u32 bt_mode;
	u32 pad_cfg;
	u32 separate_mode;
	u32 cascade_mode;
	u32 ext_flags;

	wait_queue_head_t detect_wait;
	bool detect_done;
};

struct rtk_vi_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head link;
	dma_addr_t phy_addr;
	u8 ch_index;
	u8 entry_index;
};

#define to_vi_buffer(buf)	container_of(buf, struct rtk_vi_buffer, vb)

struct rtk_vi_fmt {
	unsigned int fourcc;
};

extern int rtk_vi_hw_init(struct rtk_vi *vi);
extern int rtk_vi_hw_deinit(struct rtk_vi *vi);

#endif /* RTK_VIDEO_IN_H_ */
