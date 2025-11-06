/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef RTK_MIPI_CSI_H_
#define RTK_MIPI_CSI_H_

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
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/videodev2.h>
#include <linux/debugfs.h>
#include <linux/sys_soc.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ioctl.h>

#include <media/videobuf2-dma-contig.h>

#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/rtk_refclk.h>

#include "mipi_csi_rx_top_reg.h"
#include "mipi_csi_rx_app_reg.h"
#include "mipi_top_phy_reg.h"


#define DATA_MODE_LINE      0
#define DATA_MODE_COMPENC   1

#define LANE_0  0
#define LANE_1  1
#define LANE_2  2
#define LANE_3  3

#define CH_0  0
#define CH_1  1
#define CH_2  2
#define CH_3  3
#define CH_4  4
#define CH_5  5
#define CH_MAX  CH_5

#define ENTRY_0  0
#define ENTRY_1  1
#define ENTRY_2  2
#define ENTRY_3  3

#define DISABLE 0
#define ENABLE  1

#define MIPI_TOP_0  0
#define MIPI_TOP_1  1

#define SKEW_DIS   0
#define SKEW_AUTO  1

#define MIRROR_DIS   0
#define MIRROR_EN    1

#define OFFSET_RANGE0     0
#define OFFSET_RANGE1     1
#define OFFSET_RANGE2     2
#define MAX_OFFSET_RANGE  3

/* TOP lane flags for MIPI0_DPHY_PWDB and MIPI0_DPHY_LANE_EN */
#define TOP_LANE_0   (BIT(0))
#define TOP_LANE_1   (BIT(1))
#define TOP_LANE_2   (BIT(2))
#define TOP_LANE_3   (BIT(3))
#define TOP_LANE_CK  (BIT(4))
#define TOP_LANE_NUM_1   (TOP_LANE_CK | TOP_LANE_0)
#define TOP_LANE_NUM_2   (TOP_LANE_CK | TOP_LANE_0 | TOP_LANE_1)
#define TOP_LANE_NUM_3   (TOP_LANE_CK | TOP_LANE_0 | TOP_LANE_1 | TOP_LANE_2)
#define TOP_LANE_NUM_4   (TOP_LANE_CK | TOP_LANE_0 | TOP_LANE_1 | TOP_LANE_2 | TOP_LANE_3)


#define RTK_V4L2_CID_MIPI_COMPENC		(V4L2_CID_PRIVATE_BASE + 0)

struct rtk_mipicsi_phy {
	u32 hsamp0;
	u32 hsamp1;
	u32 hsamp2;
	u32 hsamp3;
	u32 offset_range_ckk;
	u32 offset_range_lane3;
	u32 offset_range_lane2;
	u32 offset_range_lane1;
	u32 offset_range_lane0;
	u32 hsampck;
	u32 lpamp;
	u8 ctune;
	u8 rtune;
	u8 ileq_adj;
	u8 ivga_adj;
	u32 z0;
};

/**
 * struct rtk_mipi_meta_data - 48 bytes frame info
 */
#define get_meta_misc0_crc(data)        ((0x00FF0000&(data))>>16)
#define get_meta_misc0_height_mis(data) ((0x00008000&(data))>>15)
#define get_meta_misc0_width_mis(data)  ((0x00004000&(data))>>14)
#define get_meta_misc0_fmt(data)        ((0x00003E00&(data))>>9)
#define get_meta_misc0_mode(data)       ((0x00000100&(data))>>8)
#define get_meta_misc0_frame_cnt(data)  ((0x000000FF&(data))>>0)
struct rtk_mipi_meta_data {
	unsigned int f_start_ts_l;
	unsigned int f_start_ts_m;
	unsigned int f_end_ts_l;
	unsigned int f_end_ts_m;
	unsigned int sa_y_header;
	unsigned int sa_y_body;
	unsigned int sa_c_header;
	unsigned int sa_c_body;
	unsigned int misc0;
	unsigned int misc1;
	unsigned int misc2;
	unsigned int misc3;
};

#define MAX_DMA_BUFS 48
/**
 * struct rtk_mipicsi_dma_bufs
 *
 * @size: allocated size
 * @vaddr:  Virtual address
 * @paddr: Physical address
 */
struct rtk_mipicsi_dma_bufs {
	struct device *dev;
	struct list_head link;
	unsigned int size;
	void *vaddr;
	dma_addr_t paddr;
	struct dma_buf *dmabuf;
	int fd;
};

struct rtk_mipicsi;

struct rtk_mipicsi_ops {
	u8 (*is_frame_done)(u32 done_st, u8 ch_index, u8 entry_index);
	void (*scale_down)(struct rtk_mipicsi *mipicsi, u8 ch_index);
	void (*app_ctrl)(struct rtk_mipicsi *mipicsi,
		u8 ch_index, u8 enable);
	u32 (*calculate_video_size)(u32 dst_width, u32 dst_height, u8 mode);
	void (*app_size_cfg)(struct rtk_mipicsi *mipicsi, u8 ch_index);
	void (*dma_buf_cfg)(struct rtk_mipicsi *mipicsi,
		uint64_t start_addr, u8 ch_index, u8 entry_index);
	void (*clear_done_flag)(struct rtk_mipicsi *mipicsi,
		u8 ch_index, u8 entry_index);
	void (*dump_frame_state)(struct rtk_mipicsi *mipicsi,
		u8 ch_index);
	bool (*phy_check)(struct rtk_mipicsi *mipicsi, u8 top_index,
			u32 timeout_ms);
	bool (*eq_tuning)(struct rtk_mipicsi *mipicsi,
			u8 top_index, u32 check_ms, u8 max_sample);
	void (*aphy_set_manual_skw)(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 enable);
	u8 (*aphy_get_manual_skw)(struct rtk_mipicsi *mipicsi,
		u8 top_index);
	void (*aphy_set_skw_sclk)(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_index, u8 sclk);
	u8 (*aphy_get_skw_sclk)(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_index);
	void (*aphy_set_skw_sdata)(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_index, u8 sdata);
	u8 (*aphy_get_skw_sdata)(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 lane_index);
	void (*aphy_set_ctune)(struct rtk_mipicsi *mipicsi,
			u8 top_index, u8 rtune);
	u8 (*aphy_get_ctune)(struct rtk_mipicsi *mipicsi,
			u8 top_index);
	void (*aphy_set_rtune)(struct rtk_mipicsi *mipicsi,
			u8 top_index, u8 rtune);
	u8 (*aphy_get_rtune)(struct rtk_mipicsi *mipicsi,
			u8 top_index);
	void (*aphy_set_d2s)(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 d2s_isel);
	u8 (*aphy_get_d2s)(struct rtk_mipicsi *mipicsi,
		u8 top_index);
	void (*aphy_set_ibn_dc)(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 ibn_dc);
	u8 (*aphy_get_ibn_dc)(struct rtk_mipicsi *mipicsi,
		u8 top_index);
	void (*aphy_set_ibn_gm)(struct rtk_mipicsi *mipicsi,
		u8 top_index, u8 ibn_gm);
	u8 (*aphy_get_ibn_gm)(struct rtk_mipicsi *mipicsi,
		u8 top_index);
	void (*dump_aphy_cfg)(struct rtk_mipicsi *mipicsi,
			u8 top_index);
	void (*dump_meta_data)(struct rtk_mipicsi *mipicsi,
		struct rtk_mipi_meta_data *meta_data);
	void (*dump_entry_state)(struct rtk_mipicsi *mipicsi,
			u8 ch_index, u8 entry_index);
	void (*interrupt_ctrl)(struct rtk_mipicsi *mipicsi,
			u8 ch_index, u8 enable);
	int (*get_intr_state)(struct rtk_mipicsi *mipicsi,
			u32 *p_done_st, u8 ch_max);
	void (*meta_swap)(struct rtk_mipicsi *mipicsi, u8 enable);
	void (*crc_ctrl)(struct rtk_mipicsi *mipicsi, u8 enable);
	void (*urgent_ctrl)(struct rtk_mipicsi *mipicsi, u8 enable);
	/* Internal color Bar signal for debug test */
	void (*color_bar_test)(struct rtk_mipicsi *mipicsi, u8 enable);
};

struct rtk_mipicsi_buffer {
	struct vb2_v4l2_buffer vb;
	struct list_head link;
	dma_addr_t phy_addr;
	u8 ch_index;
	u8 entry_index;
};

/**
 * struct rtk_mipicsi_conf - device config for of_device_id data
 *
 * @en_group_dev: Non-standard feature. Multi channel in one video device.
 *
 */
struct rtk_mipicsi_conf {
	bool en_group_dev;
};

/**
 * struct rtk_mipicsi_group_data - for en_group_dev feature
 *
 * @dma_bufs: All allocated DMA buffers
 * @num_dma_bufs:  number of allocated buffer
 * @dma_list: list for inactive DMA buff
 * @done_dma_buf: DMA buffer pointer for finished frame on each channel
 * @done_ts: refclk timestamp
 * @done_flags: [Bit0] Ch0 done [Bit1] Ch1 done...
 * @ready: done_flags mask, all channels done if ready=done_flags
 * @do_stop: 1 - stop streaming
 * @done_wait: for wait all channels frame ready
 * @dq_dma_buf: DMA buffer pointer for user space is used
 *
 */
#define NUM_MAX_VB  12
struct rtk_mipicsi_group_data {
	struct rtk_mipicsi_dma_bufs dma_bufs[MAX_DMA_BUFS];
	u32 num_dma_bufs;
	struct list_head dma_list;
	struct rtk_mipicsi_dma_bufs *done_dma_buf[6];
	u64 done_ts[6];
	u8 done_flags;
	u8 ready;
	bool do_stop;
	wait_queue_head_t done_wait;
	struct rtk_mipicsi_dma_bufs *dq_dma_buf[NUM_MAX_VB][6];
};

#define MAX_SCLK   0x3F
#define MAX_SDATA  0x3F

#define MAX_CTUNE   13
#define MAX_RTUNE   12
#define MAX_D2S     16
#define MAX_IBM_DC  4
#define MAX_IBM_GM  4
struct rtk_mipicsi_debug {
	struct dentry *debugfs_dir;
	bool en_colorbar;
	u64 phy_check_ms;
	u8 tuning_sample;
};

struct rtk_mipicsi_crt {
	struct reset_control *reset_mipi;
	struct clk *clk_mipi;
	struct clk *clk_npu_mipi;
	struct clk *clk_npu_pll;
	bool crt_obtained;
	bool phy_inited;
};

struct rtk_mipicsi {
	struct regmap *topreg;
	struct regmap *phyreg;
	struct regmap *appreg;
	int irq;
	union {
		u32 ch_index;
		u32 ch_max;
	};

	struct device *dev;
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	struct vb2_queue queue;
	struct v4l2_pix_format pix_fmt;
	struct v4l2_bt_timings active_timings;
	struct v4l2_bt_timings detected_timings;
	unsigned int v4l2_input_status;
	struct mutex video_lock;

	struct rtk_mipicsi_group_data *g_data;

	struct list_head buffers;
	struct mutex buffer_lock; /* buffer list lock */
	spinlock_t slock;

	const struct rtk_mipicsi_conf *conf;
	const struct rtk_mipicsi_ops *hw_ops;
	union {
		struct rtk_mipicsi_buffer *cur_buf[4];
		struct rtk_mipicsi_dma_bufs *cur_dma_buf[6][4];
	};
	struct rtk_mipicsi_debug debug;
	u32 sequence;
	u8 mode;
	u32 skew_mode;
	u32 mirror_mode;
	u32 src_width;
	u32 src_height;
	u32 dst_width;
	u32 dst_height;
	u32 line_pitch;
	u32 header_pitch;
	u32 video_size;
	bool hw_init_done;
};

#define to_rtk_mipicsi(x) container_of(x, struct rtk_mipicsi, x)

#define to_mipicsi_buffer(buf)	container_of(buf, struct rtk_mipicsi_buffer, vb)

extern int rtk_mipi_csi_save_crt(struct rtk_mipicsi *mipicsi,
		struct rtk_mipicsi_crt *crt);
extern int rtk_mipi_csi_hw_init(struct rtk_mipicsi *mipicsi);
extern int rtk_mipi_csi_hw_deinit(struct rtk_mipicsi *mipicsi);
extern void rtk_mipicsi_setup_dbgfs(struct rtk_mipicsi *mipicsi);

#endif /* RTK_MIPI_CSI_H_ */
