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

#include "rtk_vcodec_drv.h"
#include "rtk_vcodec_dec.h"
#include "rtk_vcodec_fw.h"

#include <linux/of_address.h>

#define RTK_VCODEC_NAME "rtk-vcodec"

#define RTK_VCODEC_MAX_FORMATS 8

#define RTK_COD_STD_DECODE_H264 0
#define RTK_COD_STD_DECODE_HEVC 0
#define RTK_COD_STD_DECODE_VC1  1
#define RTK_COD_STD_DECODE_MP2  2
#define RTK_COD_STD_DECODE_MP4  3
#define RTK_COD_STD_DECODE_DV3  3
#define RTK_COD_STD_DECODE_RV   4
#define RTK_COD_STD_DECODE_AVS  5
#define RTK_COD_STD_DECODE_VPX  7

#define RTK_COD_STD_ENCODE_H264 8
#define RTK_COD_STD_ENCODE_MP4  11

// TODO: reference to ve1_vdi.c
#define RTK_SRAM_SIZE 0x1D000
#define	FRATE_RES_MASK 0xffff
#define	FRATE_DIV_OFFSET 16

#define TEMP_BUF_SIZE (204 * 1024)
#define WORK_BUF_SIZE (80 * 1024)

#define MAX_4K_WIDTH 4096
#define MAX_4K_HEIGHT 2304

#define MAX_2K_WIDTH 1920
#define MAX_2K_HEIGHT 1088

#define MIN_WIDTH 64
#define MIN_HEIGHT 64

#define CODA7_STREAM_BUF_PIC_FLUSH (1 << 3)
#define CODA9_FRAME_ENABLE_BWB (1 << 12)
#define RTK_REG_RESET_ENABLE (1 << 0)
#define RTK_REG_RUN_ENABLE (1 << 0)
#define RTK_REG_INT_CLEAR_ENABLE 0x1

#define VE_CTRL_REG (0x3000)
#define VE_CTI_GRP_REG (0x3004)
#define VE_MBIST_CTRL (0x3C08)
#define VE_BISR_POWER_RESET (0x3CB0)

#define	RTK_COMMAND_FIRMWARE_GET 0xf

#define	RTK_FIRMWARE_PRODUCT(x) (((x) >> 16) & 0xffff)
#define	RTK_FIRMWARE_MAJOR(x)	 (((x) >> 12) & 0x0f)
#define	RTK_FIRMWARE_MINOR(x)	 (((x) >> 8) & 0x0f)
#define	RTK_FIRMWARE_RELEASE(x) ((x) & 0xff)

// #define CODA_ROT_MIR_ENABLE	(1 << 4)
// #define CODA_ROT_0			(0x0 << 0)
// #define CODA_ROT_90			(0x1 << 0)
// #define CODA_ROT_180		(0x2 << 0)
// #define CODA_ROT_270		(0x3 << 0)
// #define CODA_MIR_NONE		(0x0 << 2)
#define CODA_MIR_VER		(0x1 << 2)
#define CODA_MIR_HOR		(0x2 << 2)
// #define CODA_MIR_VER_HOR	(0x3 << 2)

#define VE1_PROT_CTRL 0x3050
#define VE1_CTRL 0x3000

#define fh_to_ctx(__fh)	container_of(__fh, struct rtk_vcodec_ctx, fh)

int rtk_vcodec_debug;
module_param(rtk_vcodec_debug, int, 0644);
MODULE_PARM_DESC(rtk_vcodec_debug, "Debug level (0-2)");

static int disable_tiling;
module_param(disable_tiling, int, 0644);
MODULE_PARM_DESC(disable_tiling, "Disable tiled frame buffers");

#define RTK_CODEC(mode, src_fourcc, dst_fourcc, max_w, max_h) \
	{ mode, src_fourcc, dst_fourcc, max_w, max_h }

static const struct rtk_codec rtk_video_codecs[] = {
	RTK_CODEC(RTK_COD_STD_DECODE_H264, V4L2_PIX_FMT_H264,  V4L2_PIX_FMT_NV12, MAX_4K_WIDTH, MAX_4K_HEIGHT),
	RTK_CODEC(RTK_COD_STD_DECODE_HEVC, V4L2_PIX_FMT_HEVC,  V4L2_PIX_FMT_NV12, MAX_4K_WIDTH, MAX_4K_HEIGHT),
	RTK_CODEC(RTK_COD_STD_DECODE_MP2,  V4L2_PIX_FMT_MPEG2, V4L2_PIX_FMT_NV12, MAX_2K_WIDTH, MAX_2K_HEIGHT),
	RTK_CODEC(RTK_COD_STD_DECODE_MP4,  V4L2_PIX_FMT_MPEG4, V4L2_PIX_FMT_NV12, MAX_2K_WIDTH, MAX_2K_HEIGHT),
	RTK_CODEC(RTK_COD_STD_DECODE_VPX,  V4L2_PIX_FMT_VP8,   V4L2_PIX_FMT_NV12, MAX_2K_WIDTH, MAX_2K_HEIGHT),

	RTK_CODEC(RTK_COD_STD_ENCODE_H264, V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_H264,  MAX_4K_WIDTH, MAX_4K_HEIGHT),
	RTK_CODEC(RTK_COD_STD_ENCODE_MP4,  V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_MPEG4, MAX_4K_WIDTH, MAX_4K_HEIGHT),
};

struct rtk_video_device {
	const char *name;
	enum rtk_vcodec_ctx_type type;
	const struct rtk_context_ops *ops;
	const struct rtk_context_common_ops *common_ops;
	bool direct;
	u32 src_formats[RTK_VCODEC_MAX_FORMATS];
	u32 dst_formats[RTK_VCODEC_MAX_FORMATS];
};

static const struct rtk_video_device rtk_bit_decoder = {
	.name = "rtk-bit-decoder",
	.type = RTK_VCODEC_CTX_DECODER,
	// .ops = &rtk_ve1_decoder_ops,
	.common_ops = &rtk_common_ops,
	.src_formats = {
		V4L2_PIX_FMT_H264,
		V4L2_PIX_FMT_HEVC,
		V4L2_PIX_FMT_MPEG2,
		V4L2_PIX_FMT_MPEG4,
		V4L2_PIX_FMT_VP8,
	},
	.dst_formats = {
		V4L2_PIX_FMT_NV12,
	},
};

static const struct rtk_video_device rtk_bit_encoder = {
	.name = "rtk-bit-encoder",
	.type = RTK_VCODEC_CTX_ENCODER,
	// .ops = &rtk_ve1_encoder_ops,
	.common_ops = &rtk_common_ops,
	.src_formats = {
		V4L2_PIX_FMT_NV12,
	},
	.dst_formats = {
		V4L2_PIX_FMT_H264,
		V4L2_PIX_FMT_MPEG4,
	},
};

static const struct rtk_video_device *rtk_video_devices[] = {
	&rtk_bit_decoder,
	&rtk_bit_encoder,
};

enum rtk_platform {
	RTK_STARK,
	RTK_THOR,
};

static const struct rtk_devinfo rtk_devdata[] = {
	[RTK_STARK] = {
		.product      = CODA_980,
		.codecs       = rtk_video_codecs,
		.num_codecs   = ARRAY_SIZE(rtk_video_codecs),
		.vdevs        = rtk_video_devices,
		.num_vdevs    = ARRAY_SIZE(rtk_video_devices),
		.workbuf_size = WORK_BUF_SIZE,
		.tempbuf_size = TEMP_BUF_SIZE,
		.sram_size    = RTK_SRAM_SIZE,
	},
	[RTK_THOR] = {
		// TODO: other platform ?
	},
};

/**
 * RTK vcodec hw operations
 */

static inline int rtk_vcodec_is_initialized(struct rtk_vcodec_dev *dev)
{
	return rtk_vcodec_read(dev, BIT_CUR_PC) != 0;
}

int rtk_check_firmware(struct rtk_vcodec_dev *dev)
{
	u16 product, major, minor, release;
	u32 data;
	int ret;

	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	rtk_vcodec_write(dev, 0, RET_FW_VER_NUM);
	rtk_vcodec_write(dev, RTK_COMMAND_FIRMWARE_GET, BIT_RUN_COMMAND);

	if (rtk_wait_timeout(dev)) {
		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		v4l2_err(&dev->v4l2_dev, "firmware get command error\n");
		ret = -EIO;
		goto err_run_cmd;
	}

	/* Check we are compatible with the loaded firmware */
	data = rtk_vcodec_read(dev, RET_FW_VER_NUM);
	printk(KERN_INFO"[\x1b[33m RET_FW_VER_NUM : 0x%x\033[0m]\n", data);

	/* Check we are compatible with the loaded firmware */
	data = rtk_vcodec_read(dev, RET_FW_CODE_REV);
	printk(KERN_INFO"[\x1b[33m RET_FW_CODE_REV : 0x%x\033[0m]\n", data);


	return 0;

err_run_cmd:
	return ret;
}

static int rtk_vcodec_hw_reset(struct rtk_vcodec_dev *dev)
{
	unsigned long timeout;
	unsigned int idx, cmd;
	int ret;

	printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	lockdep_assert_held(&dev->rtk_mutex);

	if (!dev->rstc) {
		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		return -ENOENT;
	}

	idx = rtk_vcodec_read(dev, BIT_RUN_INDEX);
	printk(KERN_INFO"[idx : %d]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", idx, __func__, __LINE__);

	if (dev->devinfo->product == CODA_980) {
		printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		timeout = jiffies + msecs_to_jiffies(100);
		while (rtk_vcodec_read(dev, GDI_BWB_STATUS) != 0x0) {
			if (time_after(jiffies, timeout)) {
				printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
				return -ETIME;
			}
			printk(KERN_INFO"\t\t[\x1b[33m 000 cpu relax !!!\033[0m]\n");
			cpu_relax();
		}
	}

	printk(KERN_INFO"[\x1b[33m GDI_BWB_STATUS : 0x%x\033[0m]\n", rtk_vcodec_read(dev, GDI_BWB_STATUS));

	if (dev->devinfo->product == CODA_980) {
		printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		timeout = jiffies + msecs_to_jiffies(100);
		rtk_vcodec_write(dev, 0x11, GDI_BUS_CTRL);
		while (rtk_vcodec_read(dev, GDI_BUS_STATUS) != 0x77) {
			if (time_after(jiffies, timeout)) {
				printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
				return -ETIME;
			}
			printk(KERN_INFO"\t\t[\x1b[33m 111 cpu relax !!!\033[0m]\n");
			cpu_relax();
		}
	}

	printk(KERN_INFO"[\x1b[33m GDI_BUS_STATUS : 0x%x\033[0m]\n", rtk_vcodec_read(dev, GDI_BUS_STATUS));

	cmd = 0;
	// Software Reset Trigger
	cmd |= VPU_SW_RESET_VCE_CORE | VPU_SW_RESET_VCE_BUS;
	cmd |= VPU_SW_RESET_GDI_CORE | VPU_SW_RESET_GDI_BUS;

	printk(KERN_INFO"[\x1b[33m cmd : 0x%x\033[0m]\n", cmd);
	rtk_vcodec_write(dev, cmd, BIT_SW_RESET);

	timeout = jiffies + msecs_to_jiffies(100);
	while (rtk_vcodec_read(dev, BIT_SW_RESET_STATUS) != 0x0) {
		if (time_after(jiffies, timeout)) {
			printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			rtk_vcodec_write(dev, 0x0, BIT_SW_RESET);
			rtk_vcodec_write(dev, 0x0, GDI_BUS_CTRL);
			return -ETIME;
		}
		printk(KERN_INFO"\t\t[\x1b[33m cpu relax !!!\033[0m]\n");
		cpu_relax();
	}

	printk(KERN_INFO"[\x1b[33m BIT_SW_RESET_STATUS : 0x%x\033[0m]\n", rtk_vcodec_read(dev, BIT_SW_RESET_STATUS));

	rtk_vcodec_write(dev, 0x0, BIT_SW_RESET);
	rtk_vcodec_write(dev, 0x0, GDI_BUS_CTRL);

	printk(KERN_INFO"\t\t[\x1b[33m hw reset done !!!\033[0m]\n");
	printk(KERN_INFO"\t\t[\x1b[33m hw reset done !!!\033[0m]\n");
	printk(KERN_INFO"\t\t[\x1b[33m hw reset done !!!\033[0m]\n");

	return ret;
}

static void rtk_copy_bitcode(struct rtk_vcodec_dev *dev, const unsigned short * const buf,
			       size_t size)
{
	u32 *src = (u32 *)buf;

	printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	/* Check if the firmware has a 16-byte Freescale header, skip it */
	if (buf[0] == 'M' && buf[1] == 'X')
		src += 4;
	/*
	 * Check whether the firmware is in native order or pre-reordered for
	 * memory access. The first instruction opcode always is 0xe40e.
	 */
	if (__le16_to_cpup((__le16 *)src) == 0xe40e) {
		u32 *dst = dev->codebuf.vaddr;
		printk(KERN_INFO"[\x1b[32mdst : %px\033[0m]\n", dst);
		int i;
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		/* Firmware in native order, reorder while copying */
		// TODO: refine later
		// if (dev->devinfo->product == CODA_DX6) {
		// 	for (i = 0; i < (size - 16) / 4; i++)
		// 		dst[i] = (src[i] << 16) | (src[i] >> 16);
		// } else {
		for (i = 0; i < (size - 16) / 4; i += 2) {
			dst[i] = (src[i + 1] << 16) | (src[i + 1] >> 16);
			dst[i + 1] = (src[i] << 16) | (src[i] >> 16);
		}
		// }
	} else {
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		/* Copy the already reordered firmware image */
		memcpy(dev->codebuf.vaddr, src, size);
	}
}

static int rtk_vcodec_hw_init(struct rtk_vcodec_dev *dev)
{
	u32 data;
	u16 *p;
	int i, ret;
	unsigned int bit_int_val;
	// TODO rtk16xxb settings

	printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	reset_control_reset(dev->rstc);

	// TODO: reference to ve1_wrapper_setup()
	unsigned int ctrl_1;
	unsigned int ctrl_2;
	unsigned int ctrl_3;
	unsigned int ctrl_4;
	static int ve_cti_en = 1;
	/* FIX ME after ver.B IC */
	static int ve_idle_en = 0;

	ctrl_1 = rtk_vcodec_read(dev, VE_CTRL_REG);
	ctrl_2 = rtk_vcodec_read(dev, VE_CTI_GRP_REG);
	ctrl_3 = rtk_vcodec_read(dev, VE_BISR_POWER_RESET);

	printk(KERN_INFO"00000 [\x1b[32mctrl_1 : 0x%x, ctrl_2 : 0x%x, ctrl_3 : 0x%x\033[0m]\n", ctrl_1, ctrl_2, ctrl_3);

	ctrl_1 |= (ve_cti_en << 1 | ve_idle_en << 6);
	/* ve1_cti_cmd_depth for 1296 timing issue */
	ctrl_2 = (ctrl_2 & ~(0x3f << 24)) | (0x1a << 24);
	ctrl_3 |= (1<<12);
	/*Set BISR POWER RESET bit12 to 1, make AXI available in stark*/

	printk(KERN_INFO"11111 [\x1b[32mctrl_1 : 0x%x, ctrl_2 : 0x%x, ctrl_3 : 0x%x\033[0m]\n", ctrl_1, ctrl_2, ctrl_3);
	rtk_vcodec_write(dev, ctrl_1, VE_CTRL_REG);
	rtk_vcodec_write(dev, ctrl_2, VE_CTI_GRP_REG);
	rtk_vcodec_write(dev, ctrl_3, VE_BISR_POWER_RESET);

	for(i = 0 ; i < 2 ; i++) {  //workaround for TP1CK MEM TEST1 in stark
		ctrl_4 = rtk_vcodec_read(dev, VE_MBIST_CTRL);
		printk(KERN_INFO"22222 [\x1b[32mctrl_4 : 0x%x\033[0m]\n", ctrl_4);
		ctrl_4 ^= (1<<2);  //toggle TEST1 signal of MEM
		printk(KERN_INFO"33333 [\x1b[32mctrl_4 : 0x%x\033[0m]\n", ctrl_4);
		rtk_vcodec_write(dev, ctrl_4, VE_MBIST_CTRL);
	}

	// TODO: refine later
	// rtk_vcodec_write(dev, 0, BIT_RUN_INDEX);
	// rtk_check_firmware(dev);

	// printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	// TODO: refine later
	rtk_vcodec_hw_reset(dev);
	// printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	if ((rtk_vcodec_read(dev, VE1_CTRL) & 0x2) != 0) {
		rtk_vcodec_write(dev, 0x2, VE1_PROT_CTRL);
	}

	rtk_copy_bitcode(dev, bit_code, sizeof(bit_code));

	rtk_vcodec_write(dev, 0, BIT_INT_ENABLE);
	rtk_vcodec_write(dev, 0, BIT_CODE_RUN);

	for (i = 0; i < 2048 ; ++i) {
		data = bit_code[i];
		rtk_vcodec_write(dev, (i << 16) | data, BIT_CODE_DOWN);
	}

	/* Clear registers */
	for (i = 0; i < 64; i++)
		rtk_vcodec_write(dev, 0, BIT_CODE_BUF_ADDR + i * 4);

	rtk_vcodec_write(dev, dev->codebuf.paddr, BIT_CODE_BUF_ADDR);
	rtk_vcodec_write(dev, dev->tempbuf.paddr, BIT_TEMP_BUF_ADDR);

	rtk_vcodec_write(dev, CODA7_STREAM_BUF_PIC_FLUSH, BIT_BIT_STREAM_CTRL);
	rtk_vcodec_write(dev, 0x4, BIT_FRAME_MEM_CTRL);
	rtk_vcodec_write(dev, 0, BIT_BIT_STREAM_PARAM);
	rtk_vcodec_write(dev, 0, BIT_AXI_SRAM_USE);
	rtk_vcodec_write(dev, 0, BIT_INT_ENABLE);
	rtk_vcodec_write(dev, 0, BIT_ROLLBACK_STATUS);

	bit_int_val = (1 << INT_BIT_BIT_BUF_FULL);
	bit_int_val |= (1 << INT_BIT_BIT_BUF_EMPTY);
	bit_int_val |= (1 << INT_BIT_DEC_MB_ROWS);
	bit_int_val |= (1 << INT_BIT_SEQ_INIT);
	bit_int_val |= (1 << INT_BIT_INIT);
	bit_int_val |= (1 << INT_BIT_DEC_FIELD);
	bit_int_val |= (1 << INT_BIT_PIC_RUN);

	printk(KERN_INFO"[\x1b[32m bit_int_val : 0x%x\033[0m]\n", bit_int_val);
	rtk_vcodec_write(dev, bit_int_val, BIT_INT_ENABLE);

	rtk_vcodec_write(dev, RTK_REG_INT_CLEAR_ENABLE, BIT_INT_CLEAR);
	rtk_vcodec_write(dev, RTK_REG_BUSY_ENABLE, BIT_BUSY_FLAG);
	rtk_vcodec_write(dev, RTK_REG_RESET_ENABLE, BIT_CODE_RESET);
	udelay(10);
	rtk_vcodec_write(dev, RTK_REG_RUN_ENABLE, BIT_CODE_RUN);

	ret = rtk_wait_timeout(dev);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "rtk_vcodec_hw_init timeout\n");
		// rtk_vcodec_hw_reset(dev);
		return ret;
	}

	v4l2_dbg(0, rtk_vcodec_debug, &dev->v4l2_dev,
			"rtk_vcodec_hw_init success\n");

	return 0;
}

/**
 * RTK vcodec common operations
 */

const char *rtk_vcodec_product_name(int product)
{
	static char buf[9];

	switch (product) {
	case CODA_980:
		return "CODA980";
	default:
		snprintf(buf, sizeof(buf), "(0x%04x)", product);
		return buf;
	}
}

int rtk_vcodec_alloc_extra_buf(struct rtk_vcodec_dev *dev, struct rtk_extra_buf *buf,
		       size_t size, const char *name, struct dentry *parent)
{
	buf->vaddr = dma_alloc_coherent(dev->dev, size, &buf->paddr,
					GFP_KERNEL);
	if (!buf->vaddr) {
		v4l2_err(&dev->v4l2_dev,
			 "Failed to allocate %s buffer of size %zu\n",
			 name, size);
		return -ENOMEM;
	}

	buf->size = size;

	if (name && parent) {

		v4l2_dbg(1, rtk_vcodec_debug, &dev->v4l2_dev,
			"alloc extra buf (%s) vaddr : %px, paddr : %px, size : %d\n",
			name, buf->vaddr, buf->paddr, size);

		buf->blob.data = buf->vaddr;
		buf->blob.size = size;
		buf->dentry = debugfs_create_blob(name, 0444, parent,
						  &buf->blob);
	}

	return 0;
}

void rtk_vcodec_free_extra_buf(struct rtk_vcodec_dev *dev,
		       struct rtk_extra_buf *buf, const char *name)
{
	if (buf->vaddr) {

		v4l2_dbg(1, rtk_vcodec_debug, &dev->v4l2_dev,
			"free extra buf (%s) vaddr : %px, paddr : %px, size : %d\n",
			name, buf->vaddr, buf->paddr, buf->size);

		dma_free_coherent(dev->dev, buf->size, buf->vaddr, buf->paddr);
		buf->vaddr = NULL;
		buf->size = 0;
		debugfs_remove(buf->dentry);
		buf->dentry = NULL;
	}
}

static const struct rtk_video_device *to_rtk_video_device(struct video_device
							    *vdev)
{
	struct rtk_vcodec_dev *dev = video_get_drvdata(vdev);
	unsigned int i = vdev - dev->vfd;

	if (i >= dev->devinfo->num_vdevs)
		return NULL;

	return dev->devinfo->vdevs[i];
}

static const struct rtk_codec *rtk_vcodec_find_codec(struct rtk_vcodec_dev *dev,
						int src_fourcc, int dst_fourcc)
{
	const struct rtk_codec *codecs = dev->devinfo->codecs;
	int num_codecs = dev->devinfo->num_codecs;
	int k;

	if (src_fourcc == dst_fourcc)
		return NULL;

	for (k = 0; k < num_codecs; k++) {
		if (codecs[k].src_fourcc == src_fourcc &&
		    codecs[k].dst_fourcc == dst_fourcc)
			break;
	}

	if (k == num_codecs)
		return NULL;

	return &codecs[k];
}

static void rtk_vcodec_set_default_params(struct rtk_vcodec_ctx *ctx)
{
	unsigned int max_w, max_h;
	int i;

	ctx->codec = rtk_vcodec_find_codec(ctx->dev, ctx->cvd->src_formats[0],
				     ctx->cvd->dst_formats[0]);

	max_w = min(ctx->codec->max_w, MAX_4K_WIDTH);
	max_h = min(ctx->codec->max_h, MAX_4K_HEIGHT);

	ctx->params.codec_mode = ctx->codec->mode;

	if (ctx->cvd->src_formats[0] == V4L2_PIX_FMT_JPEG)
		ctx->colorspace = V4L2_COLORSPACE_JPEG;
	else
		ctx->colorspace = V4L2_COLORSPACE_REC709;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->params.framerate = 30;
	/* Default formats for output and input queues */
	ctx->q_data[V4L2_M2M_SRC_Q_DATA].fourcc = ctx->cvd->src_formats[0];
	ctx->q_data[V4L2_M2M_SRC_Q_DATA].width = max_w;
	ctx->q_data[V4L2_M2M_SRC_Q_DATA].height = max_h;
	ctx->q_data[V4L2_M2M_SRC_Q_DATA].bytesperline = 0;
	ctx->q_data[V4L2_M2M_SRC_Q_DATA].sizeimage = 3 << 20;
	ctx->q_data[V4L2_M2M_SRC_Q_DATA].rect.width = max_w;
	ctx->q_data[V4L2_M2M_SRC_Q_DATA].rect.height = max_h;

	ctx->q_data[V4L2_M2M_DST_Q_DATA].fourcc = ctx->cvd->dst_formats[0];
	ctx->q_data[V4L2_M2M_DST_Q_DATA].width = max_w;
	ctx->q_data[V4L2_M2M_DST_Q_DATA].height = max_h;
	ctx->q_data[V4L2_M2M_DST_Q_DATA].bytesperline = max_w;
	ctx->q_data[V4L2_M2M_DST_Q_DATA].sizeimage = max_w * max_h * 3 / 2;;
	ctx->q_data[V4L2_M2M_DST_Q_DATA].rect.width = max_w;
	ctx->q_data[V4L2_M2M_DST_Q_DATA].rect.height = max_h;

	/*
	 * Since the RBC2AXI logic only supports a single chroma plane,
	 * macroblock tiling only works for to NV12 pixel format.
	 */
	ctx->tiled_map_type = RTK_LINEAR_FRAME_MAP;

	rtk_vcodec_dbg(2, ctx, "set default params :\n");
	rtk_vcodec_dbg(2, ctx, "codec_mode : %d\n", ctx->params.codec_mode);
	for (i = 0 ; i < 2 ; i++) {
		rtk_vcodec_dbg(2, ctx, "q_data[%d].fourcc : 0x%x\n", i, ctx->q_data[i].fourcc);
		rtk_vcodec_dbg(2, ctx, "q_data[%d].width : %d, q_data[%d].height : %d\n", i, ctx->q_data[i].width, i, ctx->q_data[i].height);
		rtk_vcodec_dbg(2, ctx, "q_data[%d].bytesperline : %d, q_data[%d].sizeimage : %d\n",
			i, ctx->q_data[i].bytesperline, i, ctx->q_data[i].sizeimage);
		rtk_vcodec_dbg(2, ctx, "q_data[%d].rect.width : %d, q_data[%d].rect.height : %d\n",
			i, ctx->q_data[i].rect.width, i, ctx->q_data[i].rect.height);
	}
}

/*
 * RTK vcodec v4l2 ioctl operations
 */

static int rtk_vcodec_querycap(struct file *file, void *priv,
			 struct v4l2_capability *cap)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	strscpy(cap->driver, RTK_VCODEC_NAME, sizeof(cap->driver));
	strscpy(cap->card, rtk_vcodec_product_name(ctx->dev->devinfo->product),
		sizeof(cap->card));
	strscpy(cap->bus_info, "platform:" RTK_VCODEC_NAME, sizeof(cap->bus_info));

	cap->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int rtk_vcodec_enum_fmt(struct file *file, void *priv,
			 struct v4l2_fmtdesc *f)
{
	struct video_device *vdev = video_devdata(file);
	const struct rtk_video_device *cvd = to_rtk_video_device(vdev);
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	const u32 *formats;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		formats = cvd->src_formats;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		formats = cvd->dst_formats;
	} else {
		return -EINVAL;
	}

	if (f->index >= RTK_VCODEC_MAX_FORMATS || formats[f->index] == 0) {
		return -EINVAL;
	}

	f->pixelformat = formats[f->index];

	rtk_vcodec_dbg(2, ctx, "(%s) enum format: \n", v4l2_type_names[f->type]);
	rtk_vcodec_dbg(2, ctx, "     formats[%d] : 0x%x\n", f->index, formats[f->index]);

	return 0;
}

static int rtk_vcodec_g_fmt(struct file *file, void *priv,
		      struct v4l2_format *f)
{
	struct rtk_q_data *q_data;
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	q_data = rtk_get_q_data(ctx, f->type);
	if (!q_data) {
		return -EINVAL;
	}

	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.pixelformat  = q_data->fourcc;
	f->fmt.pix.width        = q_data->width;
	f->fmt.pix.height       = q_data->height;
	f->fmt.pix.bytesperline = q_data->bytesperline;

	f->fmt.pix.sizeimage	= q_data->sizeimage;
	f->fmt.pix.colorspace	= ctx->colorspace;
	f->fmt.pix.xfer_func	= ctx->xfer_func;
	f->fmt.pix.ycbcr_enc	= ctx->ycbcr_enc;
	f->fmt.pix.quantization	= ctx->quantization;

	rtk_vcodec_dbg(2, ctx, "(%s) get format: \n", v4l2_type_names[f->type]);
	rtk_vcodec_dbg(2, ctx, "     pixelformat  : 0x%x, field : %d\n", f->fmt.pix.pixelformat, f->fmt.pix.field);
	rtk_vcodec_dbg(2, ctx, "     widthxheight : (%dx%d)\n", f->fmt.pix.width, f->fmt.pix.height);
	rtk_vcodec_dbg(2, ctx, "     bytesperline : %d\n", f->fmt.pix.bytesperline);
	rtk_vcodec_dbg(2, ctx, "     sizeimage    : %d\n", f->fmt.pix.sizeimage);

	return 0;
}

static int rtk_try_pixelformat(struct rtk_vcodec_ctx *ctx, struct v4l2_format *f)
{
	struct rtk_q_data *q_data;
	const u32 *formats;
	int i;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		formats = ctx->cvd->src_formats;
	else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		formats = ctx->cvd->dst_formats;
	else
		return -EINVAL;

	for (i = 0; i < RTK_VCODEC_MAX_FORMATS; i++) {
		if (formats[i] == f->fmt.pix.pixelformat) {
			f->fmt.pix.pixelformat = formats[i];
			return 0;
		}
	}

	/* Fall back to currently set pixelformat */
	q_data = rtk_get_q_data(ctx, f->type);
	f->fmt.pix.pixelformat = q_data->fourcc;

	return 0;
}

static int rtk_try_fmt(struct rtk_vcodec_ctx *ctx, const struct rtk_codec *codec,
			struct v4l2_format *f)
{
	unsigned int width = 0, height = 0;
	unsigned int bytesperline = 0, sizeimage = 0;

	struct rtk_q_data *q_data;
	struct rtk_q_data *q_data_src;

	// printk(KERN_ALERT"[f->fmt.pix.width  : %d]\n", f->fmt.pix.width);
	// printk(KERN_ALERT"[f->fmt.pix.height : %d]\n", f->fmt.pix.height);

	q_data     = rtk_get_q_data(ctx, f->type);
	q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);


	// printk(KERN_ALERT"[q_data->ori_width  : %d]\n", q_data->ori_width);
	// printk(KERN_ALERT"[q_data->ori_height : %d]\n", q_data->ori_height);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_H264) {
			// printk(KERN_ALERT"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			q_data->ori_width  = clamp(f->fmt.pix.width, MIN_WIDTH, MAX_4K_WIDTH);
			q_data->ori_height = clamp(f->fmt.pix.height, MIN_HEIGHT, MAX_4K_HEIGHT);
			width  = round_up(q_data->ori_width, 32);
			height = round_up(q_data->ori_height, 32);
		} else if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_VP8) {
			// printk(KERN_ALERT"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			q_data->ori_width  = clamp(f->fmt.pix.width, MIN_WIDTH, MAX_2K_WIDTH);
			q_data->ori_height = clamp(f->fmt.pix.height, MIN_HEIGHT, MAX_2K_HEIGHT);
			width  = round_up(q_data->ori_width, 64);
			height = round_up(q_data->ori_height, 64);
		}
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		if (q_data_src->fourcc == V4L2_PIX_FMT_H264) {
			// printk(KERN_ALERT"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			q_data->ori_width  = clamp(f->fmt.pix.width, MIN_WIDTH, MAX_4K_WIDTH);
			q_data->ori_height = clamp(f->fmt.pix.height, MIN_HEIGHT, MAX_4K_HEIGHT);
			width  = round_up(q_data->ori_width, 32);
			height = round_up(q_data->ori_height, 32);
		} else if (q_data_src->fourcc == V4L2_PIX_FMT_VP8) {
			// printk(KERN_ALERT"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			q_data->ori_width  = clamp(f->fmt.pix.width, MIN_WIDTH, MAX_2K_WIDTH);
			q_data->ori_height = clamp(f->fmt.pix.height, MIN_HEIGHT, MAX_2K_HEIGHT);
			width  = round_up(q_data->ori_width, 64);
			height = round_up(q_data->ori_height, 64);
		}
	}

	// printk(KERN_ALERT"[width  : %d]\n", width);
	// printk(KERN_ALERT"[height : %d]\n", height);

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		bytesperline = width;
		sizeimage = bytesperline * height * 3 / 2;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		bytesperline = 0;
		sizeimage = 3 << 20;
	}

	f->fmt.pix.width        = width;
	f->fmt.pix.height       = height;
	f->fmt.pix.field        = V4L2_FIELD_NONE;
	f->fmt.pix.bytesperline = bytesperline;
	f->fmt.pix.sizeimage    = sizeimage;

	rtk_vcodec_dbg(2, ctx, "(%s) try format: \n", v4l2_type_names[f->type]);
	rtk_vcodec_dbg(2, ctx, "     pixelformat  : 0x%x, field : %d\n", f->fmt.pix.pixelformat, f->fmt.pix.field);
	rtk_vcodec_dbg(2, ctx, "     widthxheight : (%dx%d)\n", f->fmt.pix.width, f->fmt.pix.height);
	rtk_vcodec_dbg(2, ctx, "     bytesperline : %d\n", f->fmt.pix.bytesperline);
	rtk_vcodec_dbg(2, ctx, "     sizeimage    : %d\n", f->fmt.pix.sizeimage);

	return 0;
}

static int rtk_vcodec_try_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	const struct rtk_q_data *q_data_src;
	const struct rtk_codec *codec;
	struct vb2_queue *src_vq;
	int ret;
	unsigned int ysize;

	ret = rtk_try_pixelformat(ctx, f);
	if (ret < 0) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to rtk_try_pixelformat\n");
		return ret;
	}

	f->fmt.pix.colorspace = ctx->colorspace;
	f->fmt.pix.xfer_func = ctx->xfer_func;
	f->fmt.pix.ycbcr_enc = ctx->ycbcr_enc;
	f->fmt.pix.quantization = ctx->quantization;

	q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	codec = rtk_vcodec_find_codec(ctx->dev, q_data_src->fourcc,
				f->fmt.pix.pixelformat);

	if (!codec) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to rtk_vcodec_find_codec\n");
		return -EINVAL;
	}

	return rtk_try_fmt(ctx, codec, f);
}

static int rtk_s_fmt(struct rtk_vcodec_ctx *ctx, struct v4l2_format *f,
		      struct v4l2_rect *r)
{
	struct rtk_q_data *q_data;
	struct vb2_queue *vq;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = rtk_get_q_data(ctx, f->type);
	if (!q_data)
		return -EINVAL;

	if (vb2_is_busy(vq)) {
		v4l2_err(&ctx->dev->v4l2_dev, "%s: %s queue busy: %d\n",
			 __func__, v4l2_type_names[f->type], vq->num_buffers);
		return -EBUSY;
	}

	q_data->fourcc       = f->fmt.pix.pixelformat;
	q_data->width        = f->fmt.pix.width;
	q_data->height       = f->fmt.pix.height;
	q_data->bytesperline = f->fmt.pix.bytesperline;
	q_data->sizeimage    = f->fmt.pix.sizeimage;

	if (r) {
		q_data->rect = *r;
	} else {
		q_data->rect.left = 0;
		q_data->rect.top = 0;
		q_data->rect.width = f->fmt.pix.width;
		q_data->rect.height = f->fmt.pix.height;
	}

	switch (f->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_YUYV:
		ctx->tiled_map_type = RTK_TILED_FRAME_MB_RASTER_MAP;
		break;
	case V4L2_PIX_FMT_NV12:
		// TODO: coda upstream
		// TODO: ray modify
		// if (!disable_tiling && ctx->use_bit &&
		//     ctx->dev->devinfo->product == CODA_980) {
		// 	ctx->tiled_map_type = RTK_TILED_FRAME_MB_RASTER_MAP;
		// 	break;
		// }
		fallthrough;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV422P:
		ctx->tiled_map_type = RTK_LINEAR_FRAME_MAP;
		break;
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_MPEG2:
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_VP8:
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		ctx->ops = &rtk_ve1_decoder_ops;
		if (ctx->ops->seq_init_work)
			INIT_WORK(&ctx->seq_init_work, ctx->ops->seq_init_work);
		break;
	default:
		break;
	}

	// TODO: Not sure rtk support vdoa or not
	// if (ctx->tiled_map_type == RTK_TILED_FRAME_MB_RASTER_MAP &&
	//     !coda_try_fmt_vdoa(ctx, f, &ctx->use_vdoa) &&
	//     ctx->use_vdoa)
	// 	vdoa_context_configure(ctx->vdoa,
	// 			       round_up(f->fmt.pix.width, 16),
	// 			       f->fmt.pix.height,
	// 			       f->fmt.pix.pixelformat);
	// else
	ctx->use_vdoa = false;

	rtk_vcodec_dbg(2, ctx, "(%s) set format: \n", v4l2_type_names[f->type]);
	rtk_vcodec_dbg(2, ctx, "     pixelformat  : 0x%x, field : %d\n", f->fmt.pix.pixelformat, f->fmt.pix.field);
	rtk_vcodec_dbg(2, ctx, "     widthxheight : (%dx%d)\n", f->fmt.pix.width, f->fmt.pix.height);
	rtk_vcodec_dbg(2, ctx, "     bytesperline : %d\n", f->fmt.pix.bytesperline);
	rtk_vcodec_dbg(2, ctx, "     sizeimage    : %d\n", f->fmt.pix.sizeimage);

	rtk_vcodec_dbg(2, ctx, "(%s) set qdata: \n", v4l2_type_names[f->type]);
	rtk_vcodec_dbg(2, ctx, "     fourcc       : %4.4s\n", (char *)&q_data->fourcc);
	rtk_vcodec_dbg(2, ctx, "     widthxheight : (%dx%d)\n", q_data->width, q_data->height);
	rtk_vcodec_dbg(2, ctx, "     bytesperline : %d\n", q_data->bytesperline);
	rtk_vcodec_dbg(2, ctx, "     sizeimage    : %d\n", q_data->sizeimage);
	rtk_vcodec_dbg(2, ctx, "     rect leftxtop     : (%dx%d)\n", q_data->rect.left, q_data->rect.top);
	rtk_vcodec_dbg(2, ctx, "     rect widthxheight : (%dx%d)\n", q_data->rect.width, q_data->rect.height);
	rtk_vcodec_dbg(2, ctx, "     ori  widthxheight : (%dx%d)\n", q_data->ori_width, q_data->ori_height);

	rtk_vcodec_dbg(1, ctx, "Setting %s format, wxh: %dx%d, fmt: %4.4s sizeimage : %d %c\n",
		 v4l2_type_names[f->type], q_data->width, q_data->height,
		 (char *)&q_data->fourcc,
		 q_data->sizeimage,
		 (ctx->tiled_map_type == RTK_LINEAR_FRAME_MAP) ? 'L' : 'T');

	return 0;
}

static int rtk_vcodec_s_fmt_vid_cap(struct file *file, void *priv,
			      struct v4l2_format *f)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct rtk_q_data *q_data_src;
	const struct rtk_codec *codec;
	struct v4l2_rect r;
	int ret;

	ret = rtk_vcodec_try_fmt_vid_cap(file, priv, f);
	if (ret) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to rtk_vcodec_try_fmt_vid_cap\n");
		return ret;
	}

	q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
	r.left = 0;
	r.top = 0;
	r.width = q_data_src->width;
	r.height = q_data_src->height;

	ret = rtk_s_fmt(ctx, f, &r);
	if (ret) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to rtk_s_fmt\n");
		return ret;
	}

	if (ctx->inst_type != RTK_VCODEC_CTX_ENCODER)
		return 0;

	/* Setting the coded format determines the selected codec */
	codec = rtk_vcodec_find_codec(ctx->dev, q_data_src->fourcc,
				f->fmt.pix.pixelformat);
	if (!codec) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to determine codec\n");
		return -EINVAL;
	}
	ctx->codec = codec;

	ctx->colorspace = f->fmt.pix.colorspace;
	ctx->xfer_func = f->fmt.pix.xfer_func;
	ctx->ycbcr_enc = f->fmt.pix.ycbcr_enc;
	ctx->quantization = f->fmt.pix.quantization;

	return 0;
}

static void rtk_set_default_colorspace(struct v4l2_pix_format *fmt)
{
	enum v4l2_colorspace colorspace;

	if (fmt->pixelformat == V4L2_PIX_FMT_JPEG)
		colorspace = V4L2_COLORSPACE_JPEG;
	else if (fmt->width <= 720 && fmt->height <= 576)
		colorspace = V4L2_COLORSPACE_SMPTE170M;
	else
		colorspace = V4L2_COLORSPACE_REC709;

	fmt->colorspace = colorspace;
	fmt->xfer_func = V4L2_XFER_FUNC_DEFAULT;
	fmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->quantization = V4L2_QUANTIZATION_DEFAULT;
}

static int rtk_vcodec_try_fmt_vid_out(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct rtk_vcodec_dev *dev = ctx->dev;
	const struct rtk_q_data *q_data_dst;
	const struct rtk_codec *codec;
	int ret;

	ret = rtk_try_pixelformat(ctx, f);
	if (ret < 0) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to rtk_try_pixelformat\n");
		return ret;
	}

	if (f->fmt.pix.colorspace == V4L2_COLORSPACE_DEFAULT) {
		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		rtk_set_default_colorspace(&f->fmt.pix);
	}

	q_data_dst = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	codec = rtk_vcodec_find_codec(dev, f->fmt.pix.pixelformat, q_data_dst->fourcc);

	return rtk_try_fmt(ctx, codec, f);
}

static int rtk_vcodec_s_fmt_vid_out(struct file *file, void *priv,
			      struct v4l2_format *f)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	const struct rtk_codec *codec;
	struct v4l2_format f_cap;
	struct vb2_queue *dst_vq;
	int ret;

	ret = rtk_vcodec_try_fmt_vid_out(file, priv, f);
	if (ret) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to rtk_vcodec_try_fmt_vid_out\n");
		return ret;
	}

	ret = rtk_s_fmt(ctx, f, NULL);
	if (ret) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to rtk_s_fmt\n");
		return ret;
	}

	ctx->colorspace = f->fmt.pix.colorspace;
	ctx->xfer_func = f->fmt.pix.xfer_func;
	ctx->ycbcr_enc = f->fmt.pix.ycbcr_enc;
	ctx->quantization = f->fmt.pix.quantization;

	// return 0;
 // TODO: refine later
	if (ctx->inst_type != RTK_VCODEC_CTX_DECODER)
		return 0;

	codec = rtk_vcodec_find_codec(ctx->dev, f->fmt.pix.pixelformat,
				V4L2_PIX_FMT_NV12);
	if (!codec) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to determine codec\n");
		return -EINVAL;
	}
	ctx->codec = codec;

	dst_vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	if (!dst_vq)
		return -EINVAL;

	if (vb2_is_busy(dst_vq))
		return 0;

	memset(&f_cap, 0, sizeof(f_cap));
	f_cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rtk_vcodec_g_fmt(file, priv, &f_cap);
	f_cap.fmt.pix.width = f->fmt.pix.width;
	f_cap.fmt.pix.height = f->fmt.pix.height;

	return rtk_vcodec_s_fmt_vid_cap(file, priv, &f_cap);
}

static int rtk_vcodec_reqbufs(struct file *file, void *priv,
			struct v4l2_requestbuffers *rb)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;

	ret = v4l2_m2m_reqbufs(file, ctx->fh.m2m_ctx, rb);
	if (ret)
		return ret;

	rtk_vcodec_dbg(2, ctx, "(%s) req %d bufs\n", v4l2_type_names[rb->type], rb->count);

	/*
	 * Allow to allocate instance specific per-context buffers, such as
	 * bitstream ringbuffer, slice buffer, work buffer, etc. if needed.
	 */

	if (ctx->ops) {
		return ctx->ops->reqbufs(ctx, rb);
	}

	return 0;
}

static int rtk_vcodec_qbuf(struct file *file, void *priv,
		     struct v4l2_buffer *buf)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	if (ctx->inst_type == RTK_VCODEC_CTX_DECODER &&
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		buf->flags &= ~V4L2_BUF_FLAG_LAST;
	}

	rtk_vcodec_dbg(2, ctx, "(%s) queue buf %d\n", v4l2_type_names[buf->type], buf->index);

	return v4l2_m2m_qbuf(file, ctx->fh.m2m_ctx, buf);
}

static int rtk_vcodec_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;
	// static int fake_buf_last = 0;

	ret = v4l2_m2m_dqbuf(file, ctx->fh.m2m_ctx, buf);

	if (ctx->inst_type == RTK_VCODEC_CTX_DECODER &&
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		buf->flags &= ~V4L2_BUF_FLAG_LAST;
	}

	rtk_vcodec_dbg(2, ctx, "(%s) dequeue buf %d%s\n",
		v4l2_type_names[buf->type], buf->index,
		(buf->flags & V4L2_BUF_FLAG_LAST) ?
				 " (last)" : "");

	return ret;
}

static int rtk_vcodec_g_selection(struct file *file, void *fh,
			    struct v4l2_selection *s)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct rtk_q_data *q_data;
	struct v4l2_rect r, *rsel;

	rtk_vcodec_dbg(2, ctx, "(%s) get selection, target : 0x%x\n", s->type, s->target);

	q_data = rtk_get_q_data(ctx, s->type);
	if (!q_data) {
		return -EINVAL;
	}

	r.left = 0;
	r.top = 0;
	r.width = q_data->width;
	r.height = q_data->height;
	rsel = &q_data->rect;

	rtk_vcodec_dbg(2, ctx, "rect x-y(%dx%d), rect w-h(%dx%d)\n",
		r.left, r.top, r.width, r.height);

	rtk_vcodec_dbg(2, ctx, "rsel x-y(%dx%d), rsel w-h(%dx%d)\n",
		rsel->left, rsel->top, rsel->width, rsel->height);

	switch (s->target) {
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		rsel = &r;
		fallthrough;
	case V4L2_SEL_TGT_CROP:
		if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT ||
		    ctx->inst_type == RTK_VCODEC_CTX_DECODER) {
			return -EINVAL;
		}
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		rsel = &r;
		fallthrough;
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
		    ctx->inst_type == RTK_VCODEC_CTX_ENCODER) {
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	s->r = *rsel;

	return 0;
}

static void rtk_vcodec_wake_up_capture_queue(struct rtk_vcodec_ctx *ctx)
{
	struct vb2_queue *dst_vq;

	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

	rtk_vcodec_dbg(1, ctx, "waking up capture queue\n");

	dst_vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	dst_vq->last_buffer_dequeued = true;
	wake_up(&dst_vq->done_wq);
}

static int rtk_vcodec_try_decoder_cmd(struct file *file, void *fh,
				struct v4l2_decoder_cmd *dc)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(fh);

	if (ctx->inst_type != RTK_VCODEC_CTX_DECODER)
		return -ENOTTY;

	return v4l2_m2m_ioctl_try_decoder_cmd(file, fh, dc);
}

static bool rtk_vcodec_mark_last_meta(struct rtk_vcodec_ctx *ctx)
{
	struct rtk_meta_buffer *meta;

	spin_lock(&ctx->meta_buffer_lock);
	if (list_empty(&ctx->meta_buffer_list)) {
		spin_unlock(&ctx->meta_buffer_lock);
		rtk_vcodec_dbg(1, ctx, "meta buffer list empty\n");
		return false;
	}

	meta = list_last_entry(&ctx->meta_buffer_list, struct rtk_meta_buffer,
			       list);
	meta->last = true;

	rtk_vcodec_dbg(1, ctx, "marking last meta %d\n", meta->sequence);

	spin_unlock(&ctx->meta_buffer_lock);
	return true;
}

static bool rtk_vcodec_mark_last_dst_buf(struct rtk_vcodec_ctx *ctx)
{
	struct vb2_v4l2_buffer *buf;
	struct vb2_buffer *dst_vb;
	struct vb2_queue *dst_vq;
	unsigned long flags;

	rtk_vcodec_dbg(1, ctx, "marking last capture buffer\n");

	dst_vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	spin_lock_irqsave(&dst_vq->done_lock, flags);
	if (list_empty(&dst_vq->done_list)) {
		spin_unlock_irqrestore(&dst_vq->done_lock, flags);
		rtk_vcodec_dbg(1, ctx, "capture buffer done list empty\n");
		return false;
	}

	dst_vb = list_last_entry(&dst_vq->done_list, struct vb2_buffer,
				 done_entry);
	buf = to_vb2_v4l2_buffer(dst_vb);
	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	buf->flags |= V4L2_BUF_FLAG_LAST;

	spin_unlock_irqrestore(&dst_vq->done_lock, flags);
	return true;
}

static int rtk_vcodec_decoder_cmd(struct file *file, void *fh,
			    struct v4l2_decoder_cmd *dc)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct rtk_vcodec_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *buf;
	struct vb2_queue *dst_vq;
	bool stream_end;
	bool wakeup;
	int ret;

	rtk_vcodec_dbg(2, ctx, "decoder cmd 0x%x: \n", dc->cmd);

	ret = rtk_vcodec_try_decoder_cmd(file, fh, dc);
	if (ret < 0)
		return ret;

	switch (dc->cmd) {
	case V4L2_DEC_CMD_START:
		// TODO: refine later
		// mutex_lock(&dev->coda_mutex);
		// mutex_lock(&ctx->bitstream_mutex);
		// coda_bitstream_flush(ctx);
		// dst_vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx,
		// 			 V4L2_BUF_TYPE_VIDEO_CAPTURE);
		// vb2_clear_last_buffer_dequeued(dst_vq);
		// ctx->bit_stream_param &= ~RTK_BIT_STREAM_END_FLAG;
		// coda_fill_bitstream(ctx, NULL);
		// mutex_unlock(&ctx->bitstream_mutex);
		// mutex_unlock(&dev->coda_mutex);
		break;
	case V4L2_DEC_CMD_STOP:
		stream_end = false;
		wakeup = false;

		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);

		mutex_lock(&ctx->wakeup_mutex);

		buf = v4l2_m2m_last_src_buf(ctx->fh.m2m_ctx);
		if (buf) {
			printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			rtk_vcodec_dbg(1, ctx, "marking last pending buffer %d\n", buf->vb2_buf.index);

			/* Mark last buffer */
			buf->flags |= V4L2_BUF_FLAG_LAST;

			if (v4l2_m2m_num_src_bufs_ready(ctx->fh.m2m_ctx) == 0) {
				printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
				rtk_vcodec_dbg(1, ctx, "all remaining buffers queued\n");
				stream_end = true;
			}
		} else {
			if (ctx->use_bit) {
				printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
				if (rtk_vcodec_mark_last_meta(ctx)) {
					printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
					stream_end = true;
				} else {
					printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
					wakeup = true;
				}
			} else {
				printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
				if (!rtk_vcodec_mark_last_dst_buf(ctx)) {
					printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
					wakeup = true;
				}
			}
		}

		if (stream_end) {
			rtk_vcodec_dbg(1, ctx, "all remaining buffers queued\n");
			printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			/* Set the stream-end flag on this context */
			rtk_bitstream_end_flag(ctx);
			ctx->hold = false;
			v4l2_m2m_try_schedule(ctx->fh.m2m_ctx);
		}

		if (wakeup) {
			printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
			/* If there is no buffer in flight, wake up */
			rtk_vcodec_wake_up_capture_queue(ctx);
		}

		mutex_unlock(&ctx->wakeup_mutex);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rtk_vcodec_enum_framesizes(struct file *file, void *fh,
				struct v4l2_frmsizeenum *fsize)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct rtk_q_data *q_data_src;
	const struct rtk_codec *codec;

	if (fsize->index) {
		return -EINVAL;
	}

	if (fsize->pixel_format != V4L2_PIX_FMT_NV12) {
		codec = rtk_vcodec_find_codec(ctx->dev, fsize->pixel_format,
					V4L2_PIX_FMT_NV12);
	} else {
		q_data_src = rtk_get_q_data(ctx, V4L2_BUF_TYPE_VIDEO_OUTPUT);
		codec = rtk_vcodec_find_codec(ctx->dev, q_data_src->fourcc,
					V4L2_PIX_FMT_NV12);
	}
	if (!codec) {
		v4l2_err(&ctx->dev->v4l2_dev, "failed to determine codec\n");
		return -ENOTTY;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_CONTINUOUS;
	fsize->stepwise.min_width = MIN_WIDTH;
	fsize->stepwise.max_width = codec->max_w;
	fsize->stepwise.step_width = 1;
	fsize->stepwise.min_height = MIN_HEIGHT;
	fsize->stepwise.max_height = codec->max_h;
	fsize->stepwise.step_height = 1;

	rtk_vcodec_dbg(2, ctx, "enum framesizes: \n");
	rtk_vcodec_dbg(2, ctx, "min_width : %d, max_width : %d\n",
		fsize->stepwise.min_width, fsize->stepwise.max_width);
	rtk_vcodec_dbg(2, ctx, "min_height : %d, max_height : %d\n",
		fsize->stepwise.min_height, fsize->stepwise.max_height);
	rtk_vcodec_dbg(2, ctx, "step_width : %d, step_height : %d\n",
		fsize->stepwise.step_width, fsize->stepwise.step_height);

	return 0;
}

static int rtk_vcodec_subscribe_event(struct v4l2_fh *fh,
				const struct v4l2_event_subscription *sub)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(fh);

	rtk_vcodec_dbg(2, ctx, "subscribe event (0x%x)\n", sub->type);

	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		if (ctx->inst_type == RTK_VCODEC_CTX_DECODER)
			return v4l2_event_subscribe(fh, sub, 0, NULL);
		else
			return -EINVAL;
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

// TODO: for debug
int rtk_vcodec_expbuf(struct file *file, void *priv, struct v4l2_exportbuffer *eb)
{
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;

	rtk_vcodec_dbg(2, ctx, "(%s) export buf %d\n", v4l2_type_names[eb->type], eb->index);

	ret = v4l2_m2m_ioctl_expbuf(file, priv, eb);

	return ret;
}

static const struct v4l2_ioctl_ops rtk_vcodec_ioctl_ops = {
	.vidioc_querycap	= rtk_vcodec_querycap,

	.vidioc_enum_fmt_vid_cap = rtk_vcodec_enum_fmt,
	.vidioc_g_fmt_vid_cap	= rtk_vcodec_g_fmt,
	.vidioc_try_fmt_vid_cap	= rtk_vcodec_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap	= rtk_vcodec_s_fmt_vid_cap,

	.vidioc_enum_fmt_vid_out = rtk_vcodec_enum_fmt,
	.vidioc_g_fmt_vid_out	= rtk_vcodec_g_fmt,
	.vidioc_try_fmt_vid_out	= rtk_vcodec_try_fmt_vid_out,
	.vidioc_s_fmt_vid_out	= rtk_vcodec_s_fmt_vid_out,

	.vidioc_reqbufs		= rtk_vcodec_reqbufs,
	.vidioc_querybuf	= v4l2_m2m_ioctl_querybuf,

	.vidioc_qbuf		= rtk_vcodec_qbuf,
	.vidioc_expbuf		= rtk_vcodec_expbuf,
	.vidioc_dqbuf		= rtk_vcodec_dqbuf,
	.vidioc_create_bufs	= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf	= v4l2_m2m_ioctl_prepare_buf,

	.vidioc_streamon	= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,

	.vidioc_g_selection	= rtk_vcodec_g_selection,

	.vidioc_try_decoder_cmd	= rtk_vcodec_try_decoder_cmd,
	.vidioc_decoder_cmd	= rtk_vcodec_decoder_cmd,

	.vidioc_enum_framesizes	= rtk_vcodec_enum_framesizes,

	.vidioc_subscribe_event = rtk_vcodec_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

/*
 * RTK vcodec mem-to-mem operations.
 */

void rtk_vcodec_m2m_buf_done(struct rtk_vcodec_ctx *ctx, struct vb2_v4l2_buffer *buf,
		       enum vb2_buffer_state state)
{
	const struct v4l2_event eos_event = {
		.type = V4L2_EVENT_EOS
	};

	if (buf->flags & V4L2_BUF_FLAG_LAST) {
		v4l2_event_queue_fh(&ctx->fh, &eos_event);
	}

	v4l2_m2m_buf_done(buf, state);
}

static void rtk_vcodec_device_run(void *m2m_priv)
{
	struct rtk_vcodec_ctx *ctx = m2m_priv;
	struct rtk_vcodec_dev *dev = ctx->dev;

	queue_work(dev->workqueue, &ctx->pic_run_work);
}

static void rtk_vcodec_pic_run_work(struct work_struct *work)
{
	struct rtk_vcodec_ctx *ctx = container_of(work, struct rtk_vcodec_ctx, pic_run_work);
	struct rtk_vcodec_dev *dev = ctx->dev;
	int ret;

	mutex_lock(&ctx->buffer_mutex);
	mutex_lock(&dev->rtk_mutex);

	ret = ctx->ops->prepare_run(ctx);
	if (ret < 0 && ctx->inst_type == RTK_VCODEC_CTX_DECODER) {
		printk(KERN_INFO"[fn_name]:[\x1b[31m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		mutex_unlock(&dev->rtk_mutex);
		mutex_unlock(&ctx->buffer_mutex);
		/* job_finish scheduled by prepare_decode */
		return;
	}

	if (!wait_for_completion_timeout(&ctx->completion,
					 msecs_to_jiffies(1000))) {
		if (ctx->use_bit) {
			dev_err(dev->dev, "RTK PIC_RUN timeout\n");

			ctx->hold = true;

			rtk_vcodec_hw_reset(dev);
		}

		if (ctx->ops->run_timeout)
			ctx->ops->run_timeout(ctx);
	} else {
		ctx->ops->finish_run(ctx);
	}

	if ((ctx->aborting || (!ctx->streamon_cap && !ctx->streamon_out)) &&
	    ctx->common_ops->seq_end_work) {
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		queue_work(dev->workqueue, &ctx->seq_end_work);
	}

	mutex_unlock(&dev->rtk_mutex);
	mutex_unlock(&ctx->buffer_mutex);

	v4l2_m2m_job_finish(ctx->dev->m2m_dev, ctx->fh.m2m_ctx);
}

static int rtk_vcodec_job_ready(void *m2m_priv)
{
	struct rtk_vcodec_ctx *ctx = m2m_priv;
	int src_bufs = v4l2_m2m_num_src_bufs_ready(ctx->fh.m2m_ctx);

	/*
	 * For both 'P' and 'key' frame cases 1 picture
	 * and 1 frame are needed. In the decoder case,
	 * the compressed frame can be in the bitstream.
	 */
	if (!src_bufs && ctx->inst_type != RTK_VCODEC_CTX_DECODER) {
		rtk_vcodec_dbg(1, ctx, "not ready: not enough vid-out buffers.\n");
		return 0;
	}

	if (!v4l2_m2m_num_dst_bufs_ready(ctx->fh.m2m_ctx)) {
		rtk_vcodec_dbg(1, ctx, "not ready: not enough vid-cap buffers.\n");
		return 0;
	}

	if (ctx->inst_type == RTK_VCODEC_CTX_DECODER && ctx->use_bit) {
		bool stream_end = ctx->bit_stream_param &
				  RTK_BIT_STREAM_END_FLAG;
		int num_metas = ctx->num_metas;
		struct rtk_meta_buffer *meta;
		unsigned int count;

		count = hweight32(ctx->frm_dis_flg);

		rtk_vcodec_dbg(2, ctx,
			 "stream_end : %d, ctx->hold : %d, count : %d\n",
			 stream_end, ctx->hold, count);

		// TODO: ray refine free fb timing
		if (/*ctx->use_vdoa && */count >= (ctx->num_frame_buffers /*- 1*/)) {

			rtk_vcodec_dbg(1, ctx,
				 "not ready: all frame buffers in use: %d/%d (0x%x)",
				 count, ctx->num_frame_buffers,
				 ctx->frm_dis_flg);
			return 0;
		}

		if (ctx->hold && !src_bufs) {
			rtk_vcodec_dbg(1, ctx,
				 "not ready: on hold for more buffers.\n");
			return 0;
		}

		if (!stream_end && (num_metas + src_bufs) < 2) {

			rtk_vcodec_dbg(1, ctx,
				 "not ready: need 2 buffers available (queue:%d + bitstream:%d)\n",
				 num_metas, src_bufs);
			return 0;
		}

		meta = list_first_entry(&ctx->meta_buffer_list,
					struct rtk_meta_buffer, list);
		if (!rtk_bitstream_can_fetch_past(ctx, meta->end) &&
		    !stream_end) {

			rtk_vcodec_dbg(1, ctx,
				 "not ready: not enough bitstream data to read past %u (%u)\n",
				 meta->end, ctx->bitstream_fifo.kfifo.in);
			return 0;
		}
	}

	if (ctx->aborting) {
		rtk_vcodec_dbg(1, ctx, "not ready: aborting\n");
		return 0;
	}

	rtk_vcodec_dbg(2, ctx, "job ready\n");

	return 1;
}

static void rtk_vcodec_job_abort(void *priv)
{
	struct rtk_vcodec_ctx *ctx = priv;

	ctx->aborting = 1;

	rtk_vcodec_dbg(1, ctx, "job abort\n");
}

static const struct v4l2_m2m_ops rtk_vcodec_m2m_ops = {
	.device_run	= rtk_vcodec_device_run,
	.job_ready	= rtk_vcodec_job_ready,
	.job_abort	= rtk_vcodec_job_abort,
};

/**
 * RTK vcodec ctrls operations
 */

static int rtk_vcodec_s_ctrl(struct v4l2_ctrl *ctrl)
{
	const char * const *val_names = v4l2_ctrl_get_menu(ctrl->id);
	struct rtk_vcodec_ctx *ctx = container_of(ctrl->handler, struct rtk_vcodec_ctx, ctrls);

	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	printk(KERN_INFO"[\x1b[33mctrl->id : %d\033[0m]\n", ctrl->id);
	// if (val_names)
	// 	rtk_vcodec_dbg(2, ctx, "s_ctrl: id = 0x%x, name = \"%s\", val = %d (\"%s\")\n",
	// 		 ctrl->id, ctrl->name, ctrl->val, val_names[ctrl->val]);
	// else
	// 	rtk_vcodec_dbg(2, ctx, "s_ctrl: id = 0x%x, name = \"%s\", val = %d\n",
	// 		 ctrl->id, ctrl->name, ctrl->val);

	// switch (ctrl->id) {
	// case V4L2_CID_HFLIP:
	// 	if (ctrl->val)
	// 		ctx->params.rot_mode |= CODA_MIR_HOR;
	// 	else
	// 		ctx->params.rot_mode &= ~CODA_MIR_HOR;
	// 	break;
	// case V4L2_CID_VFLIP:
	// 	if (ctrl->val)
	// 		ctx->params.rot_mode |= CODA_MIR_VER;
	// 	else
	// 		ctx->params.rot_mode &= ~CODA_MIR_VER;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_BITRATE:
	// 	ctx->params.bitrate = ctrl->val / 1000;
	// 	ctx->params.bitrate_changed = true;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
	// 	ctx->params.gop_size = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
	// 	ctx->params.h264_intra_qp = ctrl->val;
	// 	ctx->params.h264_intra_qp_changed = true;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
	// 	ctx->params.h264_inter_qp = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
	// 	ctx->params.h264_min_qp = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
	// 	ctx->params.h264_max_qp = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA:
	// 	ctx->params.h264_slice_alpha_c0_offset_div2 = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA:
	// 	ctx->params.h264_slice_beta_offset_div2 = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
	// 	ctx->params.h264_disable_deblocking_filter_idc = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_CONSTRAINED_INTRA_PREDICTION:
	// 	ctx->params.h264_constrained_intra_pred_flag = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
	// 	ctx->params.frame_rc_enable = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
	// 	ctx->params.mb_rc_enable = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_CHROMA_QP_INDEX_OFFSET:
	// 	ctx->params.h264_chroma_qp_index_offset = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	// 	/* TODO: switch between baseline and constrained baseline */
	// 	if (ctx->inst_type == RTK_VCODEC_CTX_ENCODER)
	// 		ctx->params.h264_profile_idc = 66;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	// 	/* nothing to do, this is set by the encoder */
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP:
	// 	ctx->params.mpeg4_intra_qp = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP:
	// 	ctx->params.mpeg4_inter_qp = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_MPEG2_PROFILE:
	// case V4L2_CID_MPEG_VIDEO_MPEG2_LEVEL:
	// case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
	// case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
	// 	/* nothing to do, these are fixed */
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
	// 	ctx->params.slice_mode = ctrl->val;
	// 	ctx->params.slice_mode_changed = true;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
	// 	ctx->params.slice_max_mb = ctrl->val;
	// 	ctx->params.slice_mode_changed = true;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	// 	ctx->params.slice_max_bits = ctrl->val * 8;
	// 	ctx->params.slice_mode_changed = true;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB:
	// 	ctx->params.intra_refresh = ctrl->val;
	// 	ctx->params.intra_refresh_changed = true;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
	// 	ctx->params.force_ipicture = true;
	// 	break;
	// case V4L2_CID_JPEG_COMPRESSION_QUALITY:
	// 	// TODO: Not sure rtk support jpeg or not
	// 	// coda_set_jpeg_compression_quality(ctx, ctrl->val);
	// 	break;
	// case V4L2_CID_JPEG_RESTART_INTERVAL:
	// 	// TODO: Not sure rtk support jpeg or not
	// 	// ctx->params.jpeg_restart_interval = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_VBV_DELAY:
	// 	ctx->params.vbv_delay = ctrl->val;
	// 	break;
	// case V4L2_CID_MPEG_VIDEO_VBV_SIZE:
	// 	ctx->params.vbv_size = min(ctrl->val * 8192, 0x7fffffff);
	// 	break;
	// default:
	// 	rtk_vcodec_dbg(1, ctx, "Invalid control, id=%d, val=%d\n",
	// 		 ctrl->id, ctrl->val);
	// 	return -EINVAL;
	// }

	return 0;
}

static int rtk_vcodec_g_v_ctrl(struct v4l2_ctrl *ctrl)
{
	const char * const *val_names = v4l2_ctrl_get_menu(ctrl->id);
	struct rtk_vcodec_ctx *ctx =
			container_of(ctrl->handler, struct rtk_vcodec_ctx, ctrls);
	int ret = 0;

	if (val_names)
		rtk_vcodec_dbg(2, ctx, "s_ctrl: id = 0x%x, name = \"%s\", val = %d (\"%s\")\n",
			 ctrl->id, ctrl->name, ctrl->val, val_names[ctrl->val]);
	else
		rtk_vcodec_dbg(2, ctx, "s_ctrl: id = 0x%x, name = \"%s\", val = %d\n",
			 ctrl->id, ctrl->name, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->initialized) {
			rtk_vcodec_dbg(2, ctx, "min_frame_buffers : %d\n", ctx->min_frame_buffers);
			ctrl->val = ctx->min_frame_buffers;
		} else {
			rtk_vcodec_dbg(2, ctx, "Seq not initialized yet\n");
			ctrl->val = 1;
		}
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static const struct v4l2_ctrl_ops rtk_vcodec_ctrl_ops = {
	.s_ctrl = rtk_vcodec_s_ctrl,
	.g_volatile_ctrl = rtk_vcodec_g_v_ctrl,
};

static void rtk_vcodec_decode_ctrls(struct rtk_vcodec_ctx *ctx)
{
	u8 max;

	ctx->h264_profile_ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrls,
		&rtk_vcodec_ctrl_ops, V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		~((1 << V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
		  (1 << V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE) |
		  (1 << V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) |
		  (1 << V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)),
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
	if (ctx->h264_profile_ctrl)
		ctx->h264_profile_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (ctx->dev->devinfo->product == CODA_980)
		max = V4L2_MPEG_VIDEO_H264_LEVEL_4_1;

	ctx->h264_level_ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrls,
		&rtk_vcodec_ctrl_ops, V4L2_CID_MPEG_VIDEO_H264_LEVEL, max, 0, max);
	if (ctx->h264_level_ctrl)
		ctx->h264_level_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ctx->mpeg2_profile_ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrls,
		&rtk_vcodec_ctrl_ops, V4L2_CID_MPEG_VIDEO_MPEG2_PROFILE,
		V4L2_MPEG_VIDEO_MPEG2_PROFILE_HIGH, 0,
		V4L2_MPEG_VIDEO_MPEG2_PROFILE_HIGH);
	if (ctx->mpeg2_profile_ctrl)
		ctx->mpeg2_profile_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ctx->mpeg2_level_ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrls,
		&rtk_vcodec_ctrl_ops, V4L2_CID_MPEG_VIDEO_MPEG2_LEVEL,
		V4L2_MPEG_VIDEO_MPEG2_LEVEL_HIGH, 0,
		V4L2_MPEG_VIDEO_MPEG2_LEVEL_HIGH);
	if (ctx->mpeg2_level_ctrl)
		ctx->mpeg2_level_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ctx->mpeg4_profile_ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrls,
		&rtk_vcodec_ctrl_ops, V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
		V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY, 0,
		V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY);
	if (ctx->mpeg4_profile_ctrl)
		ctx->mpeg4_profile_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	ctx->mpeg4_level_ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrls,
		&rtk_vcodec_ctrl_ops, V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
		V4L2_MPEG_VIDEO_MPEG4_LEVEL_5, 0,
		V4L2_MPEG_VIDEO_MPEG4_LEVEL_5);
	if (ctx->mpeg4_level_ctrl)
		ctx->mpeg4_level_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;
}

static int rtk_vcodec_ctrls_setup(struct rtk_vcodec_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(&ctx->ctrls, 9);

	v4l2_ctrl_new_std(&ctx->ctrls, &rtk_vcodec_ctrl_ops,
		V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&ctx->ctrls, &rtk_vcodec_ctrl_ops,
		V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (ctx->inst_type == RTK_VCODEC_CTX_ENCODER) {
		// TODO: refine later
		// v4l2_ctrl_new_std(&ctx->ctrls, &rtk_vcodec_ctrl_ops,
		// 		  V4L2_CID_MIN_BUFFERS_FOR_OUTPUT,
		// 		  1, 1, 1, 1);
		// if (ctx->cvd->dst_formats[0] == V4L2_PIX_FMT_JPEG)
		// 	rtk_vcodec_jpeg_encode_ctrls(ctx);
		// else
		// 	rtk_vcodec_encode_ctrls(ctx);
	} else {
		ctrl = v4l2_ctrl_new_std(&ctx->ctrls, &rtk_vcodec_ctrl_ops,
				  V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				  RTK_MIN_FRAME_BUFFERS, RTK_MAX_FRAME_BUFFERS, 1, 1);
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

		if (ctx->cvd->src_formats[0] == V4L2_PIX_FMT_H264)
			rtk_vcodec_decode_ctrls(ctx);

		// TODO: refine later
		// ctx->mb_err_cnt_ctrl = v4l2_ctrl_new_custom(&ctx->ctrls,
		// 				&coda_mb_err_cnt_ctrl_config,
		// 				NULL);
		// if (ctx->mb_err_cnt_ctrl)
		// 	ctx->mb_err_cnt_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	}

	if (ctx->ctrls.error) {
		v4l2_err(&ctx->dev->v4l2_dev,
			"control initialization error (%d)",
			ctx->ctrls.error);
		return -EINVAL;
	}

	return v4l2_ctrl_handler_setup(&ctx->ctrls);
}

/*
 * RTK vcodec file operations
 */

static int rtk_vcodec_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct rtk_vcodec_dev *dev = video_get_drvdata(vdev);
	struct rtk_vcodec_ctx *ctx;
	unsigned int max = ~0;
	char *name;
	int ret;
	int idx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	idx = ida_alloc_max(&dev->ida, max, GFP_KERNEL);
	if (idx < 0) {
		ret = idx;
		goto err_rtk_max;
	}

	name = kasprintf(GFP_KERNEL, "context%d", idx);
	if (!name) {
		ret = -ENOMEM;
		goto err_rtk_name_init;
	}

	ctx->debugfs_entry = debugfs_create_dir(name, dev->debugfs_root);
	kfree(name);

	ctx->cvd = to_rtk_video_device(vdev);
	ctx->inst_type = ctx->cvd->type;
	ctx->common_ops = ctx->cvd->common_ops;
	ctx->use_bit = !ctx->cvd->direct;
	init_completion(&ctx->completion);
	INIT_WORK(&ctx->pic_run_work, rtk_vcodec_pic_run_work);
	// if (ctx->ops->seq_init_work)
	// 	INIT_WORK(&ctx->seq_init_work, ctx->ops->seq_init_work);
	if (ctx->common_ops->seq_end_work)
		INIT_WORK(&ctx->seq_end_work, ctx->common_ops->seq_end_work);
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	ctx->dev = dev;
	ctx->idx = idx;

	rtk_vcodec_dbg(1, ctx, "\x1b[33mopen instance (%p) (%s)\033[0m\n", ctx, ctx->cvd->name);

	switch (dev->devinfo->product) {
	case CODA_980:
		/*
		 * Enabling the BWB when decoding can hang the firmware with
		 * certain streams. The issue was tracked as ENGR00293425 by
		 * Freescale. As a workaround, disable BWB for all decoders.
		 * The enable_bwb module parameter allows to override this.
		 */
		// TODO: refine later
		// if (enable_bwb || ctx->inst_type == RTK_VCODEC_CTX_ENCODER)
		// 	ctx->frame_mem_ctrl = CODA9_FRAME_ENABLE_BWB;
		// fallthrough;
	// TODO: refine later
	// case CODA_HX4:
	// case CODA_7541:
		ctx->reg_idx = 0;
		break;
	default:
		ctx->reg_idx = idx;
	}

	ctx->use_vdoa = false;

	// TODO rtk16xxb settings
	ret = pm_runtime_get_sync(dev->dev);
	/* Power up and upload firmware if necessary */
	// TODO: refine later
	if (ret < 0) {
		v4l2_err(&dev->v4l2_dev, "failed to power up: %d\n", ret);
		goto err_pm_get;
	}

	rtk_vcodec_set_default_params(ctx);

	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev, ctx,
					    ctx->common_ops->queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);

		v4l2_err(&dev->v4l2_dev, "%s return error (%d)\n",
			 __func__, ret);
		goto err_ctx_init;
	}

	ret = rtk_vcodec_ctrls_setup(ctx);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "failed to setup coda controls\n");
		goto err_ctrls_setup;
	}

	ctx->fh.ctrl_handler = &ctx->ctrls;

	mutex_init(&ctx->bitstream_mutex);
	mutex_init(&ctx->buffer_mutex);
	mutex_init(&ctx->wakeup_mutex);
	INIT_LIST_HEAD(&ctx->meta_buffer_list);
	spin_lock_init(&ctx->meta_buffer_lock);

	return 0;

err_ctrls_setup:
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
err_ctx_init:
	pm_runtime_put_sync(dev->dev);
err_pm_get:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
err_rtk_name_init:
	ida_free(&dev->ida, ctx->idx);
err_rtk_max:
	kfree(ctx);
	return ret;
}

static int rtk_vcodec_release(struct file *file)
{
	struct rtk_vcodec_dev *dev = video_drvdata(file);
	struct rtk_vcodec_ctx *ctx = fh_to_ctx(file->private_data);

	rtk_vcodec_dbg(1, ctx, "\x1b[31mrelease instance (%p) (%s)\033[0m\n", ctx, ctx->cvd->name);

	if (ctx->inst_type == RTK_VCODEC_CTX_DECODER && ctx->use_bit) {
		printk(KERN_INFO"[fn_name]:[\x1b[32m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		// TODO: refine later
		rtk_bitstream_end_flag(ctx);
	}

	/* If this instance is running, call .job_abort and wait for it to end */
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	// if (ctx->vdoa)
	// 	vdoa_context_destroy(ctx->vdoa);

	/* In case the instance was not running, we still need to call SEQ_END */
	if (ctx->common_ops->seq_end_work) {
		queue_work(dev->workqueue, &ctx->seq_end_work);
		flush_work(&ctx->seq_end_work);
	}

	v4l2_ctrl_handler_free(&ctx->ctrls);

	// TODO rtk16xxb settings
	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);
	// TODO: refine later
	// pm_runtime_put_sync(dev->dev);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	ida_free(&dev->ida, ctx->idx);
	if (ctx->common_ops->release) {
		ctx->common_ops->release(ctx);
	}
	debugfs_remove_recursive(ctx->debugfs_entry);
	kfree(ctx);

	return 0;
}

static const struct v4l2_file_operations rtk_vcodec_fops = {
	.owner		= THIS_MODULE,
	.open		= rtk_vcodec_open,
	.release	= rtk_vcodec_release,
	.poll		= v4l2_m2m_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= v4l2_m2m_fop_mmap,
};

static int rtk_vcodec_register_device(struct rtk_vcodec_dev *dev, int i)
{
	struct video_device *vfd = &dev->vfd[i];
	const char *name;
	int ret;

	if (i >= dev->devinfo->num_vdevs)
		return -EINVAL;

	name = dev->devinfo->vdevs[i]->name;
	strscpy(vfd->name, dev->devinfo->vdevs[i]->name, sizeof(vfd->name));
	vfd->fops	= &rtk_vcodec_fops;
	vfd->ioctl_ops	= &rtk_vcodec_ioctl_ops;
	vfd->release	= video_device_release_empty;
	vfd->lock	= &dev->dev_mutex;
	vfd->v4l2_dev	= &dev->v4l2_dev;
	vfd->vfl_dir	= VFL_DIR_M2M;
	vfd->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	video_set_drvdata(vfd, dev);

	/* Not applicable, use the selection API instead */
	// TODO: refine later
	// v4l2_disable_ioctl(vfd, VIDIOC_CROPCAP);
	// v4l2_disable_ioctl(vfd, VIDIOC_G_CROP);
	// v4l2_disable_ioctl(vfd, VIDIOC_S_CROP);

	ret = video_register_device(vfd, VFL_TYPE_VIDEO, 0);
	if (!ret) {
		v4l2_info(&dev->v4l2_dev, "%s registered as %s\n",
			  name, video_device_node_name(vfd));
	}
	return ret;
}

static void rtk_vcodec_fw_callback(struct rtk_vcodec_dev *dev)
{
	int i, ret;

	/* allocate auxiliary per-device code buffer for the BIT processor */
	ret = rtk_vcodec_alloc_extra_buf(dev, &dev->codebuf, sizeof(bit_code), "codebuf",
				 dev->debugfs_root);
	if (ret < 0)
		goto put_pm;

	dev->m2m_dev = v4l2_m2m_init(&rtk_vcodec_m2m_ops);
	if (IS_ERR(dev->m2m_dev)) {
		v4l2_err(&dev->v4l2_dev, "Failed to init mem2mem device\n");
		goto put_pm;
	}

	for (i = 0; i < dev->devinfo->num_vdevs; i++) {
		ret = rtk_vcodec_register_device(dev, i);
		if (ret) {
			v4l2_err(&dev->v4l2_dev,
				 "Failed to register %s video device: %d\n",
				 dev->devinfo->vdevs[i]->name, ret);
			goto rel_vfd;
		}
	}

	// TODO: rtk16xxb settings
	pm_runtime_set_suspended(dev->dev);
	pm_runtime_use_autosuspend(dev->dev);
	pm_runtime_set_autosuspend_delay(dev->dev, 15000);
	pm_runtime_enable(dev->dev);
	pm_runtime_mark_last_busy(dev->dev);

	return;

rel_vfd:
	while (--i >= 0)
		video_unregister_device(&dev->vfd[i]);
	v4l2_m2m_release(dev->m2m_dev);
put_pm:
	pm_runtime_put_sync(dev->dev);
}

static int rtk_vcodec_firmware_request(struct rtk_vcodec_dev *dev)
{
	int ret = 0;
	rtk_vcodec_fw_callback(dev);

	return ret;
}

irqreturn_t rtk_vcodec_irq_handler(int irq, void *data)
{
	struct rtk_vcodec_dev *dev = data;
	struct rtk_vcodec_ctx *ctx;
	unsigned long irq_reason = 0;
	unsigned int irq_status = 0;

	/* read status register to attend the IRQ */
	irq_status = rtk_vcodec_read(dev, BIT_INT_STS);
	if (irq_status) {
		irq_reason = rtk_vcodec_read(dev, BIT_INT_REASON);
		if (irq_reason & (1 << INT_BIT_REASON_PIC_RUN)) {
			printk(KERN_INFO"\t\t[\x1b[32m decode success !!!!\033[0m]\n");
		} else if (irq_reason & (1 << INT_BIT_REASON_BIT_BUF_EMPTY)) {
			printk(KERN_INFO"\t\t[\x1b[31m buffer empty !!!!\033[0m]\n");
		} else if (irq_reason & (1 << INT_BIT_REASON_DEC_FIELD)) {
			printk(KERN_INFO"\t\t[\x1b[33m decode field !!!!\033[0m]\n");
		} else if (irq_reason & (1 << INT_BIT_REASON_SEQ_INIT)) {
			printk(KERN_INFO"\t\t[\x1b[33m seq int success !!!!\033[0m]\n");
		} else if (irq_reason & (1 << INT_BIT_REASON_SEQ_END)) {
			printk(KERN_INFO"\t\t[\x1b[33m seq end success !!!!\033[0m]\n");
		}

		rtk_vcodec_write(dev, 0, BIT_INT_REASON);
		rtk_vcodec_write(dev, RTK_REG_INT_CLEAR_ENABLE, BIT_INT_CLEAR);
	} else {
		// TODO: need to do hw reset ?
		printk(KERN_INFO"\t\t[\x1b[31m decode error !!!! (0x%x)\033[0m]\n", irq_reason);
	}

	ctx = v4l2_m2m_get_curr_priv(dev->m2m_dev);
	if (ctx == NULL) {
		v4l2_err(&dev->v4l2_dev,
			 "Instance released before the end of transaction\n");
		return IRQ_HANDLED;
	}

	if (ctx->aborting) {
		rtk_vcodec_dbg(1, ctx, "task has been aborted\n");
	}

	if (rtk_dec_isbusy(ctx->dev)) {
		rtk_vcodec_dbg(1, ctx, "coda is still busy!!!!\n");
		return IRQ_NONE;
	}

	rtk_vcodec_dbg(2, ctx, "\x1b[32mrtk_vcodec_irq_handler (%p) (%s)\033[0m\n", ctx, ctx->cvd->name);

	complete(&ctx->completion);

	return IRQ_HANDLED;
}

static const struct of_device_id rtk_dt_ids[] = {
	// TODO: \drivers\soc\realtek\rtd16xxb\rtk_ve\ve1
	{ .compatible = "realtek,rtk13xx-ve1", .data = &rtk_devdata[RTK_STARK] },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rtk_dt_ids);

static int rtk_vcodec_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct rtk_vcodec_dev *dev;
	int ret, irq;
	printk(KERN_ALERT"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->devinfo = of_device_get_match_data(&pdev->dev);

	dev->dev = &pdev->dev;

	/* Get memory for physical registers */
	dev->regs_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dev->regs_base))
		return PTR_ERR(dev->regs_base);

	/* IRQ */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		return irq;
	}

	ret = devm_request_irq(&pdev->dev, irq, rtk_vcodec_irq_handler, 0,
			       RTK_VCODEC_NAME "-video", dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request irq: %d\n", ret);
		return ret;
	}

	dev->rstc = devm_reset_control_get_exclusive(&pdev->dev, "reset"); 
	if (IS_ERR(dev->rstc)) {
		ret = PTR_ERR(dev->rstc);
		dev_err(&pdev->dev, "failed get reset control: %d\n", ret);
		return ret;
	}

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		return ret;

	// TODO: Not sure rtk need ratelimit or not
	// ratelimit_default_init(&dev->mb_err_rs);
	mutex_init(&dev->dev_mutex);
	mutex_init(&dev->rtk_mutex);
	ida_init(&dev->ida);

	dev->debugfs_root = debugfs_create_dir("rtk-vcodec-dbg", NULL);

	/* allocate auxiliary per-device buffers for the BIT processor */
	if (dev->devinfo->tempbuf_size) {
		ret = rtk_vcodec_alloc_extra_buf(dev, &dev->tempbuf,
					 dev->devinfo->tempbuf_size, "tempbuf",
					 dev->debugfs_root);
		if (ret < 0)
			goto err_v4l2_register;
	}

	dev->sram.size = dev->devinfo->sram_size;

	dev->workqueue = alloc_workqueue("rtk", WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!dev->workqueue) {
		dev_err(&pdev->dev, "unable to alloc workqueue\n");
		ret = -ENOMEM;
		goto err_v4l2_register;
	}

	platform_set_drvdata(pdev, dev);

	/*
	 * Start activated so we can directly call rtk_vcodec_hw_init in
	 * coda_fw_callback regardless of whether CONFIG_PM is
	 * enabled or whether the device is associated with a PM domain.
	 */

	// TODO: refine later
	// pm_runtime_get_noresume(&pdev->dev);
	// pm_runtime_set_active(&pdev->dev);
	// pm_runtime_enable(&pdev->dev);

	ret = rtk_vcodec_firmware_request(dev);
	if (ret)
		goto err_alloc_workqueue;
	return 0;

err_alloc_workqueue:
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);
	destroy_workqueue(dev->workqueue);
err_v4l2_register:
	v4l2_device_unregister(&dev->v4l2_dev);
	return ret;
}

static int rtk_vcodec_remove(struct platform_device *pdev)
{
	struct rtk_vcodec_dev *dev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(dev->vfd); i++) {
		if (video_get_drvdata(&dev->vfd[i]))
			video_unregister_device(&dev->vfd[i]);
	}
	if (dev->m2m_dev)
		v4l2_m2m_release(dev->m2m_dev);
	pm_runtime_disable(&pdev->dev);
	v4l2_device_unregister(&dev->v4l2_dev);
	destroy_workqueue(dev->workqueue);

	rtk_vcodec_free_extra_buf(dev, &dev->codebuf, "codebuf");
	rtk_vcodec_free_extra_buf(dev, &dev->tempbuf, "tempbuf");
	debugfs_remove_recursive(dev->debugfs_root);
	ida_destroy(&dev->ida);
	return 0;
}

// #ifdef CONFIG_PM

static int rtk_vcodec_runtime_resume(struct device *dev)
{
	struct rtk_vcodec_dev *rdev = dev_get_drvdata(dev);
	int ret = 0, i;

	printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
	printk(KERN_INFO"[\x1b[33m pm_domain : 0x%x, codebuf.vaddr : 0x%x\033[0m]\n", dev->pm_domain, rdev->codebuf.vaddr);

	// TODO: coda upstream
	if (dev->pm_domain && rdev->codebuf.vaddr) {
		printk(KERN_INFO"[fn_name]:[\x1b[33m%s\033[0m], [line]: \x1b[33m%d\033[0m\n", __func__, __LINE__);
		ret = rtk_vcodec_hw_init(rdev);
		if (ret)
			v4l2_err(&rdev->v4l2_dev, "HW initialization failed\n");
	}

	return ret;
}
// #endif

static const struct dev_pm_ops rtk_vcodec_pm_ops = {
	SET_RUNTIME_PM_OPS(NULL, rtk_vcodec_runtime_resume, NULL)
};

static struct platform_driver rtk_vcodec_driver = {
	.probe	= rtk_vcodec_probe,
	.remove	= rtk_vcodec_remove,
	.driver	= {
		.name	= RTK_VCODEC_NAME,
		.of_match_table = rtk_dt_ids,
		.pm	= &rtk_vcodec_pm_ops,
	},
};

module_platform_driver(rtk_vcodec_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ray Tang <ray.tang@realtek.com>");
MODULE_DESCRIPTION("Realtek video codec V4L2 driver");
