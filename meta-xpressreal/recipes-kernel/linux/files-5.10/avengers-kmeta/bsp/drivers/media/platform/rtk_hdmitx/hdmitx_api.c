/*
 * hdmitx_api.c - RTK hdmitx driver
 *
 * Copyright (C) 2017 Realtek Semiconductor Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#include <linux/syscalls.h> /* needed for the _IOW etc stuff used later */
#include <linux/mpage.h>
#include <linux/dcache.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <sound/asound.h>
#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/fdtable.h>
#include <linux/delay.h>
#include <linux/regmap.h>

#include <linux/reset.h>
#include <linux/clk.h> /* clk_get */
#include <linux/clk-provider.h>
#include <linux/sys_soc.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/memory.h>

#include <soc/realtek/kernel-rpc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include "hdmitx_rpc.h"
#include "dc_rpc.h"
#include "hdmitx.h"
#include "hdmitx_dev.h"
#include "hdmitx_reg.h"
#include "hdmi_new_reg.h"
#include "rtk_edid.h"
#include "crt_reg.h"
#include "hdmitx_scdc.h"
#include "hdmitx_trace.h"


#include <uapi/linux/ion.h>
#include <soc/realtek/uapi/ion_rtk.h>

#include <ion_rtk_alloc.h>
#define ion_alloc ext_rtk_ion_alloc

extern struct ion_device *rtk_phoenix_ion_device;

static const struct soc_device_attribute rtk_soc_hercules[] = {
	{ .family = "Realtek Hercules", },
	{ /* empty */ }
};

static const struct soc_device_attribute rtk_soc_parker[] = {
	{ .family = "Realtek Parker", },
	{ /* empty */ }
};

static const struct soc_device_attribute rtk_soc_morbius[] = {
        { .family = "Realtek RTD1325", },
        { /* empty */ }
};

enum HDMI_AVMUTE {
	HDMI_CLRAVM = 0,
	HDMI_SETAVM
};

enum HDMI_CLK_STATE {
	HDMI_CLK_OFF = 0,
	HDMI_CLK_ON,
	HDMI_CLK_PLL_ON
};

#if (IS_ENABLED(CONFIG_ARCH_RTD129x) || IS_ENABLED(CONFIG_ARCH_RTD119X))
#define CONVERT_FOR_AVCPU(x)		((unsigned int)(x) | 0xA0000000)
#else
#define CONVERT_FOR_AVCPU(x)		(x)
#endif

unsigned int tmds_en = 0;
unsigned int hdmi_clk_always_on = 0;
unsigned int dp_or_dsi_exist = 0;
static unsigned int hdmi_is_off;
static DEFINE_MUTEX(hdmi_is_off_mutex);

#if 1//__LINUX_MEDIA_NAS__
static int hpdDetect = 0;
#endif

static void hdmitx_print_sink_info(asoc_hdmi_t *p_this);

static int is_hdmi_clock_on(hdmitx_device_t *tx_dev)
{
	int ret_value;
	unsigned int pll_mask;
	unsigned char pll_hdmi;
	struct clk *clk_hdmitx;
	struct reset_control *reset_hdmitx;

	if (tx_dev->use_new_mac)
		pll_mask = 0xCF;
	else
		pll_mask = 0xBF;

	clk_hdmitx = tx_dev->clk_hdmi;
	reset_hdmitx = tx_dev->reset_hdmi;

	if (hdmi_is_off) {
		ret_value = HDMI_CLK_OFF;
	} else if (__clk_is_enabled(clk_hdmitx) &&
		(!reset_control_status(reset_hdmitx))) {

		pll_hdmi = hdmipll_read32(tx_dev->dev, SYS_PLL_HDMI);
		if ((pll_hdmi&pll_mask) == pll_mask)
			ret_value = HDMI_CLK_PLL_ON;
		else
			ret_value = HDMI_CLK_ON;
	} else {
		ret_value = HDMI_CLK_OFF;
	}

	return ret_value;
}

void hdmitx_enable_rxsense_int(hdmitx_device_t *tx_dev)
{
	unsigned int reg_val;

	regmap_read(tx_dev->top_base, RXST, &reg_val);
	reg_val |= RXST_rxsenseint(1);
	regmap_write(tx_dev->top_base, RXST, reg_val);
}

int hdmitx_get_rxsense(hdmitx_device_t *tx_dev)
{
	unsigned int reg_val;

	regmap_read(tx_dev->top_base, RXST, &reg_val);

	if (RXST_get_Rxstatus(reg_val))
		return HDMI_RXSENSE_ON;
	else
		return HDMI_RXSENSE_OFF;
}

int hdmitx_check_rx_sense(hdmitx_device_t *tx_dev)
{
	unsigned int val;
	static int rxsense = HDMI_RXSENSE_ON;

	mutex_lock(&hdmi_is_off_mutex);
	if (is_hdmi_clock_on(tx_dev) != HDMI_CLK_OFF) {
		val = hdmitx_read32(tx_dev->dev, HDMI_PHY_STATUS);
		rxsense = HDMI_PHY_STATUS_get_Rxstatus(val);
	} else {
		/* Return previous status */
		dev_dbg(tx_dev->dev, "Skip check_rx_sense, HDMI clock not enable");
	}
	mutex_unlock(&hdmi_is_off_mutex);

	dev_dbg(tx_dev->dev, "RX status=%u", rxsense);

	if (rxsense)
		return HDMI_RXSENSE_ON;
	else
		return HDMI_RXSENSE_OFF;
}

int hdmitx_send_AVmute(struct device *dev, int flag)
{
	int ret_val;
	hdmitx_device_t *tx_dev = dev_get_drvdata(dev);
	
	ret_val = 0;

	if ((flag == HDMI_SETAVM) || (flag == HDMI_CLRAVM))
		set_mute_gpio_pulse(dev);

	/*  Skip AV mute if HDMI clock not enable */
	mutex_lock(&hdmi_is_off_mutex);
	if (is_hdmi_clock_on(tx_dev) != HDMI_CLK_PLL_ON) {
		dev_info(dev, "Skip av mute flag=%u, HDMI clock not enable", flag);
		goto unlock;
	}

	switch (flag) {
	case HDMI_SETAVM:
		if (tx_dev->use_new_mac) {
			hdmitx_mask32(dev, HDMI_NEW_PKT_GCP0,
				~(HDMI_NEW_PKT_GCP0_gcp_en_mask |
				HDMI_NEW_PKT_GCP0_set_avmute_mask |
				HDMI_NEW_PKT_GCP0_clr_avmute_mask),
				HDMI_NEW_PKT_GCP0_gcp_en(1) |
				HDMI_NEW_PKT_GCP0_set_avmute(1) |
				HDMI_NEW_PKT_GCP0_clr_avmute(0));
		} else {
			if (IS_ENABLED(CONFIG_RTK_HDCP_1x)) {
				ta_hdcp14_init();
				ta_hdcp_lib_set_encryption(0);
			}

			hdmitx_write32(dev, HDMI_GCPCR,
				HDMI_GCPCR_enablegcp(1) |
				HDMI_GCPCR_gcp_clearavmute(1) |
				HDMI_GCPCR_gcp_setavmute(1) |
				HDMI_GCPCR_write_data(0));
			hdmitx_write32(dev, HDMI_GCPCR,
				HDMI_GCPCR_enablegcp(1) |
				HDMI_GCPCR_gcp_clearavmute(0) |
				HDMI_GCPCR_gcp_setavmute(1) |
				HDMI_GCPCR_write_data(1));
		}
		dev_dbg(dev, "set av mute");
		break;
	case HDMI_CLRAVM:
		if (tx_dev->use_new_mac) {
			hdmitx_mask32(dev, HDMI_NEW_PKT_GCP0,
				~(HDMI_NEW_PKT_GCP0_gcp_en_mask |
				HDMI_NEW_PKT_GCP0_set_avmute_mask |
				HDMI_NEW_PKT_GCP0_clr_avmute_mask),
				HDMI_NEW_PKT_GCP0_gcp_en(1) |
				HDMI_NEW_PKT_GCP0_set_avmute(0) |
				HDMI_NEW_PKT_GCP0_clr_avmute(1));
		} else {
			hdmitx_write32(dev, HDMI_GCPCR,
				HDMI_GCPCR_enablegcp(1) |
				HDMI_GCPCR_gcp_clearavmute(1) |
				HDMI_GCPCR_gcp_setavmute(1) |
				HDMI_GCPCR_write_data(0));
			hdmitx_write32(dev, HDMI_GCPCR,
				HDMI_GCPCR_enablegcp(1) |
				HDMI_GCPCR_gcp_clearavmute(1) |
				HDMI_GCPCR_gcp_setavmute(0) |
				HDMI_GCPCR_write_data(1));
		}
		dev_dbg(dev, "clear av mute");
		break;
	default:
		dev_err(dev, "unknown av mute parameter");
		ret_val = -EINVAL;
	}

unlock:
	mutex_unlock(&hdmi_is_off_mutex);

	return ret_val;
}

static void hdmitx_dump_VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM(struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *arg, char *str)
{
	pr_devel("%s", __func__);
	pr_info("interfaceType       =0x%x", arg->interfaceType);

	pr_info("videoInfo.standard  =0x%x", arg->videoInfo.standard);
	pr_info("videoInfo.enProg    =0x%x", arg->videoInfo.enProg);
	pr_info("videoInfo.enDIF     =0x%x", arg->videoInfo.enDIF);
	pr_info("videoInfo.enCompRGB =0x%x", arg->videoInfo.enCompRGB);
	pr_info("videoInfo.pedType   =0x%x", arg->videoInfo.pedType);
	pr_info("videoInfo.dataInt0  =0x%x", arg->videoInfo.dataInt0);
	pr_info("videoInfo.dataInt1  =0x%x", arg->videoInfo.dataInt1);

	pr_info("hdmiInfo.hdmiMode   =0x%x", arg->hdmiInfo.hdmiMode);
	pr_info("hdmiInfo.audioSampleFreq    =0x%x", arg->hdmiInfo.audioSampleFreq);
	pr_info("hdmiInfo.audioChannelCount  =0x%x", arg->hdmiInfo.audioChannelCount);
	pr_info("hdmiInfo.dataByte1  =0x%x", arg->hdmiInfo.dataByte1);
	pr_info("hdmiInfo.dataByte2  =0x%x", arg->hdmiInfo.dataByte2);
	pr_info("hdmiInfo.dataByte3  =0x%x", arg->hdmiInfo.dataByte3);
	pr_info("hdmiInfo.dataByte4  =0x%x", arg->hdmiInfo.dataByte4);
	pr_info("hdmiInfo.dataByte5  =0x%x", arg->hdmiInfo.dataByte5);
	pr_info("hdmiInfo.dataInt0   =0x%x", arg->hdmiInfo.dataInt0);
	pr_info("hdmiInfo.hdmi2px_feature   =0x%x", arg->hdmiInfo.hdmi2px_feature);
	pr_info("hdmiInfo.hdmi_off_mode     =0x%x", arg->hdmiInfo.hdmi_off_mode);
	pr_info("hdmiInfo.hdr_ctrl_mode     =0x%x", arg->hdmiInfo.hdr_ctrl_mode);
	pr_info("hdmiInfo.reserved4         =0x%x", arg->hdmiInfo.reserved4);

}

__attribute__((unused)) static void rtk_hdmi_free(struct dma_buf_attachment *attach,
			struct sg_table *table)
{
	struct dma_buf *dmabuf;

	dmabuf = attach->dmabuf;
	BUG_ON(!dmabuf);
	BUG_ON(!dmabuf->ops);
	BUG_ON(!dmabuf->ops->end_cpu_access);

	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);
	dma_buf_put(dmabuf);
}

static int rtk_hdmi_alloc(struct device *dev, size_t size,
			struct dma_buf_attachment **attach,
			struct sg_table **table,
			void **ion_phy, void **ion_virt)
{

	int ret;
	unsigned int alloc_flags;
	struct dma_buf *dmabuf;

	alloc_flags = RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC |
			RTK_FLAG_ACPUACC;

	dmabuf = rheap_alloc(NULL, size, alloc_flags);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(dev, "Failed to dma_buf_get\n");
		ret = PTR_ERR(dmabuf);
		goto rheap_err;
	}

	*attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(*attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		ret = PTR_ERR(*attach);
		goto attach_err;
	}

	*table = dma_buf_map_attachment(*attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(*table)) {
		dev_err(dev, "Failed to map attachment \n");
		ret = PTR_ERR(*table);
		goto map_err;
	}

	ret = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (ret)
		goto access_err;

	*ion_virt = dma_buf_vmap(dmabuf);
	if (!*ion_virt) {
		dev_err(dev, "dma_buf_vmap failed\n");
		ret = -ENOMEM;
		goto kmap_err;
	}

	/*or using sg_dma_address(table->sgl)) ?*/
	*ion_phy = (void *)sg_phys((*table)->sgl);

	dev_dbg(dev, "%s *ion_phy = 0x%p , *ion_virt = 0x%p \n", __func__,
		(void *)*ion_phy, (void *)*ion_virt);

	return 0;
kmap_err:
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
access_err:
	dma_buf_unmap_attachment(*attach, *table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(dmabuf, *attach);
attach_err:
	dma_buf_put(dmabuf);
rheap_err:

	return ret;

}

int init_hdmitx_rpc_info(struct device *dev,
               struct hdmitx_rpc_info *rpc_info)
{
	int ret;
	struct dma_buf_attachment *attach;
	struct sg_table *table;

	ret = rtk_hdmi_alloc(dev, 4096, &attach, &table,
			     (void **)&rpc_info->paddr,
			     (void **)&rpc_info->vaddr);

	if (ret)
		return ret;

	rpc_info->attach = attach;
	rpc_info->dmabuf = attach->dmabuf;

	if (!rpc_info->vaddr) {
		dev_err(dev, "%s failed to allocate rpc buffer\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&rpc_info->lock);

	dev_info(dev, "%s done\n", __func__);

	return 0;
}


static int send_rpc(hdmitx_device_t *tx_dev, int opt, uint32_t command, uint32_t param1, uint32_t param2, uint32_t *retval)
{
	int ret = 0;

	ret = send_rpc_command(opt, command, param1, param2, retval);

	return ret;
}

static int get_vo_rpc_opt(hdmitx_device_t *tx_dev)
{
	int opt;

	if (IS_ENABLED(CONFIG_RTK_VO_ON_HIFI))
		opt = RPC_HIFI;
	else
		opt = RPC_AUDIO;

	return opt;
}

static int get_ao_rpc_opt(hdmitx_device_t *tx_dev)
{
	int opt;

	if (IS_ENABLED(CONFIG_REALTEK_RPC_V2)) {
		if (is_rpc_available(RPC_HIFI))
			opt = RPC_HIFI;
		else
			opt = RPC_AUDIO;

	} else {
		if (IS_ENABLED(CONFIG_REALTEK_RPC_HIFI))
			opt = RPC_HIFI;
		else
			opt = RPC_AUDIO;
	}

	return opt;
}

int rpc_toagent_set_display_format(struct device *dev,
	struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT *output_fmt)
{
	struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	unsigned int tmds_config;
	hdmitx_device_t *tx_dev = dev_get_drvdata(dev);
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;
	int opt;

	opt = get_vo_rpc_opt(tx_dev);

	mutex_lock(&rpc_info->lock);
	rpc = rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	if (opt == RPC_HIFI) {
		memcpy(rpc, output_fmt, sizeof(*rpc));
	} else {
		rpc->cmd_version = htonl(output_fmt->cmd_version);
		rpc->display_mode = htonl(output_fmt->display_mode);
		rpc->vic = htonl(output_fmt->vic);
		rpc->clock = htonl(output_fmt->clock);
		rpc->is_fractional_fps = htonl(output_fmt->is_fractional_fps);
		rpc->colorspace = htonl(output_fmt->colorspace);
		rpc->color_depth = htonl(output_fmt->color_depth);
		rpc->hdr_mode = htonl(output_fmt->hdr_mode);
		memcpy(&rpc->avi_infoframe, &output_fmt->avi_infoframe,
			sizeof(struct hdmi_infoframe_packet));
		rpc->src_3d_fmt = htonl(output_fmt->src_3d_fmt);
		rpc->dst_3d_fmt = htonl(output_fmt->dst_3d_fmt);
		rpc->en_dithering = htonl(output_fmt->en_dithering);
		rpc->frl_rate = htonl(output_fmt->frl_rate);
	}

	tmds_config = set_scdc_tmds_configuration(dev, output_fmt);

	if (opt == RPC_HIFI)
		rpc->tmds_config = tmds_config;
	else
		rpc->tmds_config = htonl(tmds_config);

	if (send_rpc(tx_dev, opt,
		ENUM_VIDEO_KERNEL_RPC_SET_DISPLAY_OUTPUT_FORMAT,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {
		ret = -EIO;
		goto exit;
	}

	/* Resend enable scramble of scdc for the TV with defect */
	if (tmds_config)
		set_scdc_tmds_configuration(dev, output_fmt);

	if (rpc_ret != FW_RETURN_SUCCESS) {
		dev_err(dev, "invoke SET_DISPLAY_OUTPUT_FORMAT rpc_ret=0x%x\n", rpc_ret);
		ret = -ENOEXEC;
		goto exit;
	}

	ret = 0;

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_toagent_get_display_format(struct device *dev,
	struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT *output_fmt)
{
	struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT *i_rpc;
	struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT *o_rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	hdmitx_device_t *tx_dev = dev_get_drvdata(dev);
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;
	int opt;

	opt = get_vo_rpc_opt(tx_dev);

	mutex_lock(&rpc_info->lock);
	i_rpc = rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT), RPC_ALIGN_SZ);
	o_rpc = (struct VIDEO_RPC_VOUT_DISPLAY_OUTPUT_FORMAT *)((unsigned long)i_rpc + offset);
	memset_io(i_rpc, 0, offset + sizeof(*o_rpc));

	if (send_rpc(tx_dev, opt,
		ENUM_VIDEO_KERNEL_RPC_GET_DISPLAY_OUTPUT_FORMAT,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {
		ret = -EIO;
		goto exit;
	}

	if (rpc_ret != FW_RETURN_SUCCESS) {
		dev_err(dev, "invoke GET_DISPLAY_OUTPUT_FORMAT rpc_ret=0x%x\n", rpc_ret);
		ret = -ENOEXEC;
		goto exit;
	}

	if (opt == RPC_HIFI) {
		memcpy(output_fmt, o_rpc, sizeof(*output_fmt));
	} else {
		output_fmt->cmd_version = htonl(o_rpc->cmd_version);
		output_fmt->display_mode = htonl(o_rpc->display_mode);
		output_fmt->vic = htonl(o_rpc->vic);
		output_fmt->clock = htonl(o_rpc->clock);
		output_fmt->is_fractional_fps = htonl(o_rpc->is_fractional_fps);
		output_fmt->colorspace = htonl(o_rpc->colorspace);
		output_fmt->color_depth = htonl(o_rpc->color_depth);
		output_fmt->tmds_config = htonl(o_rpc->tmds_config);
		output_fmt->hdr_mode = htonl(o_rpc->hdr_mode);
		memcpy(&output_fmt->avi_infoframe, &o_rpc->avi_infoframe,
				sizeof(struct hdmi_infoframe_packet));
		output_fmt->src_3d_fmt = htonl(o_rpc->src_3d_fmt);
		output_fmt->dst_3d_fmt = htonl(o_rpc->dst_3d_fmt);
		output_fmt->en_dithering = htonl(o_rpc->en_dithering);
		output_fmt->frl_rate = htonl(o_rpc->frl_rate);
	}

	ret = 0;

exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int rpc_toagent_send_edid_raw_data(struct device *dev,
		u8 *edid_data, u32 edid_size)
{
	struct VIDEO_RPC_VOUT_HDMI_EDID_RAW_DATA *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	unsigned long edid_offset;
	hdmitx_device_t *tx_dev = dev_get_drvdata(dev);
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;

	mutex_lock(&rpc_info->lock);
	rpc = (struct VIDEO_RPC_VOUT_HDMI_EDID_RAW_DATA *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_HDMI_EDID_RAW_DATA), RPC_ALIGN_SZ);
	edid_offset = offset * 2;
	memset_io(rpc, 0, sizeof(*rpc));

	memcpy(rpc_info->vaddr + edid_offset, edid_data, edid_size);

	if (get_vo_rpc_opt(tx_dev) == RPC_AUDIO) {
		rpc->paddr = htonl(rpc_info->paddr + edid_offset);
		rpc->size = htonl(edid_size);

		ret = send_rpc(tx_dev, RPC_AUDIO, ENUM_KERNEL_RPC_HDMI_EDID_RAW_DATA,
				CONVERT_FOR_AVCPU(rpc_info->paddr),
				CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
				&rpc_ret);
		if (ret) {
			ret = -EIO;
			dev_err(dev, "Send RPC_AUDIO RPC_HDMI_EDID_RAW_DATA failed, ret=%d\n", ret);
			goto exit;
		}
	}

	if (get_ao_rpc_opt(tx_dev) == RPC_HIFI) {
		rpc->paddr = rpc_info->paddr + edid_offset;
		rpc->size = edid_size;

		ret = send_rpc(tx_dev, RPC_HIFI, ENUM_KERNEL_RPC_HDMI_EDID_RAW_DATA,
				CONVERT_FOR_AVCPU(rpc_info->paddr),
				CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
				&rpc_ret);
		if (ret) {
			ret = -EIO;
			dev_err(dev, "Send RPC_HIFI RPC_HDMI_EDID_RAW_DATA failed, ret=%d\n", ret);
			goto exit;
		}
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int RPC_TOAGENT_HDMI_Config_TV_System(hdmitx_device_t *tx_dev,
	struct edid_information *hdmitx_edid_info,
	struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *arg)
{
	struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	unsigned int hdmi_mode;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;
	int opt;

	opt = get_vo_rpc_opt(tx_dev);

	hdmi_mode = arg->hdmiInfo.hdmiMode;

	mutex_lock(&hdmi_is_off_mutex);
	if (hdmi_clk_always_on)
		hdmi_is_off = 1;
	else if (hdmi_mode == VO_HDMI_OFF)
		hdmi_is_off = 1;
	mutex_unlock(&hdmi_is_off_mutex);

	mutex_lock(&rpc_info->lock);
	rpc = (struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	if (opt == RPC_HIFI) {
		memcpy(rpc, arg, sizeof(*rpc));
	} else {
		rpc->interfaceType = htonl(arg->interfaceType);

		rpc->videoInfo.standard = htonl(arg->videoInfo.standard);
		rpc->videoInfo.enProg = arg->videoInfo.enProg;
		rpc->videoInfo.enDIF = arg->videoInfo.enDIF;
		rpc->videoInfo.enCompRGB = arg->videoInfo.enCompRGB;
		rpc->videoInfo.pedType  = htonl(arg->videoInfo.pedType);
		rpc->videoInfo.dataInt0 = htonl(arg->videoInfo.dataInt0);
		rpc->videoInfo.dataInt1 = htonl(arg->videoInfo.dataInt1);

		rpc->hdmiInfo.hdmiMode  = htonl(arg->hdmiInfo.hdmiMode);
		rpc->hdmiInfo.audioSampleFreq = htonl(arg->hdmiInfo.audioSampleFreq);
		rpc->hdmiInfo.audioChannelCount = arg->hdmiInfo.audioChannelCount;
		rpc->hdmiInfo.dataByte1 = arg->hdmiInfo.dataByte1;
		rpc->hdmiInfo.dataByte2 = arg->hdmiInfo.dataByte2;
		rpc->hdmiInfo.dataByte3 = arg->hdmiInfo.dataByte3;
		rpc->hdmiInfo.dataByte4 = arg->hdmiInfo.dataByte4;
		rpc->hdmiInfo.dataByte5 = arg->hdmiInfo.dataByte5;
		rpc->hdmiInfo.dataInt0  = htonl(arg->hdmiInfo.dataInt0);
		/* Assign hdmi2px_feature after send SCDC */
		rpc->hdmiInfo.hdmi_off_mode = htonl(arg->hdmiInfo.hdmi_off_mode);
		rpc->hdmiInfo.hdr_ctrl_mode = htonl(arg->hdmiInfo.hdr_ctrl_mode);
		rpc->hdmiInfo.reserved4 = htonl(arg->hdmiInfo.reserved4);
	}

	if (arg->hdmiInfo.hdmiMode == VO_HDMI_ON) {
		unsigned int frl_flag;
		unsigned char scramble;

		frl_flag = arg->hdmiInfo.hdmi2px_feature & HDMI2PX_FRLRATE_MASK;
		if (frl_flag  == 0) {
			scramble = hdmitx_send_scdc_TmdsConfig(hdmitx_edid_info,
				arg->videoInfo.standard,
				arg->hdmiInfo.dataInt0,
				arg->hdmiInfo.dataByte1);

			if (scramble != 0)
				arg->hdmiInfo.hdmi2px_feature |= HDMI2PX_SCRAMBLE;
		}
	}

	if (opt == RPC_HIFI) {
		rpc->hdmiInfo.hdmi2px_feature = arg->hdmiInfo.hdmi2px_feature;
	} else {
		rpc->hdmiInfo.hdmi2px_feature = htonl(arg->hdmiInfo.hdmi2px_feature);
	}

	if (send_rpc(tx_dev, opt, ENUM_VIDEO_KERNEL_RPC_CONFIG_TV_SYSTEM,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);

	mutex_lock(&hdmi_is_off_mutex);
	if (hdmi_clk_always_on)
		hdmi_is_off = 0;
	else if (hdmi_mode != VO_HDMI_OFF)
		hdmi_is_off = 0;
	mutex_unlock(&hdmi_is_off_mutex);

	return ret;
}

int RPC_TOAGENT_HDMI_Config_AVI_Info(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_CONFIG_HDMI_INFO_FRAME *arg)
{
	struct VIDEO_RPC_VOUT_CONFIG_HDMI_INFO_FRAME *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;
	int opt;

	opt = get_vo_rpc_opt(tx_dev);

	mutex_lock(&rpc_info->lock);
	rpc = (struct VIDEO_RPC_VOUT_CONFIG_HDMI_INFO_FRAME *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_CONFIG_HDMI_INFO_FRAME), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	if (opt == RPC_HIFI) {
		memcpy(rpc, arg, sizeof(*rpc));
	} else {
		rpc->hdmiMode  = htonl(arg->hdmiMode);
		rpc->audioSampleFreq = htonl(arg->audioSampleFreq);
		rpc->audioChannelCount = arg->audioChannelCount;
		rpc->dataByte1 = arg->dataByte1;
		rpc->dataByte2 = arg->dataByte2;
		rpc->dataByte3 = arg->dataByte3;
		rpc->dataByte4 = arg->dataByte4;
		rpc->dataByte5 = arg->dataByte5;
		rpc->dataInt0  = htonl(arg->dataInt0);
		rpc->hdmi2px_feature = htonl(arg->hdmi2px_feature);
		rpc->hdmi_off_mode = htonl(arg->hdmi_off_mode);
		rpc->hdr_ctrl_mode = htonl(arg->hdr_ctrl_mode);
		rpc->reserved4 = htonl(arg->reserved4);
	}

	if (send_rpc(tx_dev, opt, ENUM_VIDEO_KERNEL_RPC_CONFIG_HDMI_INFO_FRAME,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int RPC_TOAGENT_HDMI_Set(hdmitx_device_t *tx_dev,
		int opt, struct AUDIO_HDMI_SET *arg)
{
	struct AUDIO_HDMI_SET *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;

	mutex_lock(&rpc_info->lock);
	rpc = (struct AUDIO_HDMI_SET *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct AUDIO_HDMI_SET), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	if (opt == RPC_HIFI)
		rpc->HDMI_Frequency = arg->HDMI_Frequency;
	else
		rpc->HDMI_Frequency = htonl(arg->HDMI_Frequency);

	if (send_rpc(tx_dev, opt, ENUM_KERNEL_RPC_HDMI_SET,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}


int RPC_TOAGENT_HDMI_Mute(hdmitx_device_t *tx_dev,
		int opt, struct AUDIO_HDMI_MUTE_INFO *arg)
{
	struct AUDIO_HDMI_MUTE_INFO *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;

	mutex_lock(&rpc_info->lock);
	rpc = (struct AUDIO_HDMI_MUTE_INFO *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct AUDIO_HDMI_MUTE_INFO), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	if (opt == RPC_HIFI) {
		rpc->instanceID = arg->instanceID;
		rpc->hdmi_mute = arg->hdmi_mute;
	} else {
		rpc->instanceID = htonl(arg->instanceID);
		rpc->hdmi_mute = arg->hdmi_mute;
	}

	if (send_rpc(tx_dev, opt, ENUM_KERNEL_RPC_HDMI_MUTE,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int RPC_TOAGENT_HDMI_OUT_VSDB(hdmitx_device_t *tx_dev,
		int opt, struct AUDIO_HDMI_OUT_VSDB_DATA *arg)
{
	struct AUDIO_HDMI_OUT_VSDB_DATA *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;

	mutex_lock(&rpc_info->lock);
	rpc = (struct AUDIO_HDMI_OUT_VSDB_DATA *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct AUDIO_HDMI_OUT_VSDB_DATA), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	if (opt == RPC_HIFI)
		rpc->HDMI_VSDB_delay = arg->HDMI_VSDB_delay;
	else
		rpc->HDMI_VSDB_delay = htonl(arg->HDMI_VSDB_delay);

	if (send_rpc(tx_dev, opt, ENUM_KERNEL_RPC_HDMI_OUT_VSDB,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int RPC_ToAgent_HDMI_OUT_EDID_0(hdmitx_device_t *tx_dev,
		int opt, struct AUDIO_HDMI_OUT_EDID_DATA2 *arg)
{
	struct AUDIO_HDMI_OUT_EDID_DATA2 *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;

	mutex_lock(&rpc_info->lock);
	rpc = (struct AUDIO_HDMI_OUT_EDID_DATA2 *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct AUDIO_HDMI_OUT_EDID_DATA2), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	if (opt == RPC_HIFI) {
		rpc->Version = arg->Version;
		rpc->HDMI_output_enable = arg->HDMI_output_enable;
		rpc->EDID_DATA_addr = arg->EDID_DATA_addr;
	} else {
		rpc->Version = htonl(arg->Version);
		rpc->HDMI_output_enable = htonl(arg->HDMI_output_enable);
		rpc->EDID_DATA_addr = htonl(arg->EDID_DATA_addr);
	}

	if (send_rpc(tx_dev, opt, ENUM_KERNEL_RPC_HDMI_OUT_EDID2,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return 0;
}

int RPC_ToAgent_QueryDisplayWin_0(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT *arg)
{
	struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN *i_rpc;
	struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT *o_rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;
	int opt;

	opt = get_vo_rpc_opt(tx_dev);

	mutex_lock(&rpc_info->lock);
	i_rpc = (struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_IN), RPC_ALIGN_SZ);
	o_rpc = (struct VIDEO_RPC_VOUT_QUERY_DISP_WIN_OUT *)((unsigned long)i_rpc + offset);

	memset_io(i_rpc, 0, offset + sizeof(*o_rpc));

	if (send_rpc(tx_dev, opt, ENUM_VIDEO_KERNEL_RPC_QUERY_DISPLAY_WIN,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	if (rpc_ret != S_OK) {
		ret = -ENOEXEC;
		goto exit;
	}

	if (opt == RPC_HIFI) {
		arg->standard = o_rpc->standard;
	} else {
		arg->standard = htonl(o_rpc->standard);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int RPC_ToAgent_Vout_EDIDdata(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_EDID_DATA *arg)
{
	struct VIDEO_RPC_VOUT_EDID_DATA *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;
	int opt;

	opt = get_vo_rpc_opt(tx_dev);

	mutex_lock(&rpc_info->lock);
	rpc = (struct VIDEO_RPC_VOUT_EDID_DATA *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_EDID_DATA), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	memcpy(rpc, arg, sizeof(struct VIDEO_RPC_VOUT_EDID_DATA));

	if (send_rpc(tx_dev, opt, ENUM_VIDEO_KERNEL_RPC_VOUT_EDID_DATA,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int RPC_ToAgent_QueryConfigTvSystem(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *arg)
{
	struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *i_rpc;
	struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *o_rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;
	int opt;

	opt = get_vo_rpc_opt(tx_dev);

	mutex_lock(&rpc_info->lock);
	i_rpc = (struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM), RPC_ALIGN_SZ);
	o_rpc = (struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM *)((unsigned long)i_rpc + offset);
	memset_io(i_rpc, 0, offset + sizeof(*o_rpc));

	if (send_rpc(tx_dev, opt, ENUM_VIDEO_KERNEL_RPC_QUERY_CONFIG_TV_SYSTEM,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	if (opt == RPC_HIFI) {
		memcpy(arg, o_rpc, sizeof(*arg));
	} else {
		arg->interfaceType = htonl(o_rpc->interfaceType);
		arg->videoInfo.standard = htonl(o_rpc->videoInfo.standard);
		arg->videoInfo.enProg = o_rpc->videoInfo.enProg;
		arg->videoInfo.enDIF = o_rpc->videoInfo.enDIF;
		arg->videoInfo.enCompRGB = o_rpc->videoInfo.enCompRGB;
		arg->videoInfo.pedType = htonl(o_rpc->videoInfo.pedType);
		arg->videoInfo.dataInt0 = htonl(o_rpc->videoInfo.dataInt0);
		arg->videoInfo.dataInt1 = htonl(o_rpc->videoInfo.dataInt1);
		arg->hdmiInfo.hdmiMode = htonl(o_rpc->hdmiInfo.hdmiMode);
		arg->hdmiInfo.audioSampleFreq = htonl(o_rpc->hdmiInfo.audioSampleFreq);
		arg->hdmiInfo.audioChannelCount = o_rpc->hdmiInfo.audioChannelCount;
		arg->hdmiInfo.dataByte1 = o_rpc->hdmiInfo.dataByte1;
		arg->hdmiInfo.dataByte2 = o_rpc->hdmiInfo.dataByte2;
		arg->hdmiInfo.dataByte3 = o_rpc->hdmiInfo.dataByte3;
		arg->hdmiInfo.dataByte4 = o_rpc->hdmiInfo.dataByte4;
		arg->hdmiInfo.dataByte5 = o_rpc->hdmiInfo.dataByte5;
		arg->hdmiInfo.dataInt0 = htonl(o_rpc->hdmiInfo.dataInt0);
		arg->hdmiInfo.hdmi2px_feature = htonl(o_rpc->hdmiInfo.hdmi2px_feature);
		arg->hdmiInfo.hdmi_off_mode = htonl(o_rpc->hdmiInfo.hdmi_off_mode);
		arg->hdmiInfo.hdr_ctrl_mode = htonl(o_rpc->hdmiInfo.hdr_ctrl_mode);
		arg->hdmiInfo.reserved4 = htonl(o_rpc->hdmiInfo.reserved4);
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int RPC_ToAgent_SET_HDMI_VRR(hdmitx_device_t *tx_dev,
		struct VIDEO_RPC_VOUT_HDMI_VRR *arg)
{
	struct VIDEO_RPC_VOUT_HDMI_VRR *rpc;
	int ret;
	u32 rpc_ret;
	unsigned long offset;
	struct hdmitx_rpc_info *rpc_info = &tx_dev->rpc_info;
	int opt;

	opt = get_vo_rpc_opt(tx_dev);

	mutex_lock(&rpc_info->lock);
	rpc = (struct VIDEO_RPC_VOUT_HDMI_VRR *)rpc_info->vaddr;
	offset = ALIGN(sizeof(struct VIDEO_RPC_VOUT_HDMI_VRR), RPC_ALIGN_SZ);
	memset_io(rpc, 0, sizeof(*rpc));

	if (opt == RPC_HIFI) {
		memcpy(rpc, arg, sizeof(*rpc));
	} else {
		int i;

		rpc->vrr_function = htonl(arg->vrr_function);
		rpc->vrr_act = htonl(arg->vrr_act);
		for (i = 0; i < 15; i++)
			rpc->reserved[i] = htonl(arg->reserved[i]);
	}

	if (send_rpc(tx_dev, opt, ENUM_VIDEO_KERNEL_RPC_SET_HDMI_VRR,
		CONVERT_FOR_AVCPU(rpc_info->paddr),
		CONVERT_FOR_AVCPU(rpc_info->paddr + offset),
		&rpc_ret)) {

		ret = -EIO;
		goto exit;
	}

	ret = 0;
exit:
	mutex_unlock(&rpc_info->lock);
	return ret;
}

int hdmitx_reset_sink_capability(asoc_hdmi_t *p_this)
{
	struct edid *raw_edid;

	p_this->en_default_edid = false;
	p_this->sink_cap_available = false;
	memset(&p_this->sink_cap, 0x0, sizeof(p_this->sink_cap));
	memset(&p_this->sink_cap.cec_phy_addr, 0xff, sizeof(p_this->sink_cap.cec_phy_addr));

	raw_edid = p_this->hdmitx_edid_info.raw_edid;
	if (!p_this->fake && p_this->hdmitx_edid_info.raw_edid != NULL)
		kfree(p_this->hdmitx_edid_info.raw_edid);

	memset(&p_this->hdmitx_edid_info, 0x0, sizeof(struct edid_information));

	if (p_this->fake)
		p_this->hdmitx_edid_info.raw_edid = raw_edid;

	memset(&p_this->support_list, 0x0, sizeof(struct hdmi_support_list));

	return 0;
}


int hdmitx_check_same_edid(hdmitx_device_t *tx_dev)
{
	int ret_val;
	struct edid *base_edid;
	struct edid *new_base_edid;
	asoc_hdmi_t *p_this = tx_dev->hdmi_data; 

	ret_val = 0;

	if (p_this->sink_cap_available != true)
		goto out;

	new_base_edid = rtk_get_base_edid(tx_dev->dev);
	if (new_base_edid == NULL)
		goto out;

	base_edid = p_this->hdmitx_edid_info.raw_edid;

	if ((new_base_edid->mfg_id[0] == base_edid->mfg_id[0]) &&
		(new_base_edid->mfg_id[1] == base_edid->mfg_id[1]) &&
		(new_base_edid->checksum == base_edid->checksum)) {

		pr_info("Same EDID mfg_id=%02x %02x checksum=%02x",
			new_base_edid->mfg_id[0], new_base_edid->mfg_id[1],
			new_base_edid->checksum);

		ret_val = 1;
	}

	kfree(new_base_edid);
out:
	return ret_val;
}

int hdmitx_get_sink_capability(hdmitx_device_t *tx_dev)
{
	asoc_hdmi_t *p_this = tx_dev->hdmi_data;
	struct edid *edid;
	u32 edid_size;
	struct VIDEO_RPC_VOUT_EDID_DATA *vout_edid;
	int ret;

	if (p_this->sink_cap_available == true)
		return 0;

	ret = rtk_get_edid(tx_dev->dev);
	if (ret)
		return ret;

	edid = p_this->hdmitx_edid_info.raw_edid;
	vout_edid = &p_this->sink_cap.vout_edid_data;
	rtk_add_edid_modes(edid, &p_this->sink_cap);
	rtk_edid_to_eld(&p_this->hdmitx_edid_info, &p_this->sink_cap);

	rtk_read_hf_extension_edid(tx_dev->dev);

	if (vout_edid->et || vout_edid->dolby_len || vout_edid->hdr10_plus)
		p_this->hdmitx_edid_info.scdc_capable |= SCDC_PRESENT;

	if (p_this->hdmitx_edid_info.max_frl_rate)
		p_this->sink_cap.hdmi_mode = HDMI_MODE_FRL;
	else if (rtk_detect_hdmi_monitor(&p_this->hdmitx_edid_info))
		p_this->sink_cap.hdmi_mode = HDMI_MODE_HDMI;
	else
		p_this->sink_cap.hdmi_mode = HDMI_MODE_DVI;

	p_this->sink_cap_available = true;

	hdmitx_print_sink_info(p_this);

	/* VIC filter for specific sink device */
	rtk_filter_quirks_vic(edid, &p_this->sink_cap);

	p_this->support_list.tx_support_size = gen_hdmi_format_support(&p_this->support_list.tx_support[0],
		&p_this->sink_cap, &p_this->hdmitx_edid_info);

	/* Send raw edid to fw */
	edid_size = edid->extensions;
	if (p_this->hdmitx_edid_info.override_extdb_count > edid_size)
		edid_size = p_this->hdmitx_edid_info.override_extdb_count;

	edid_size = (edid_size + 1) * EDID_LENGTH;
	ret = rpc_toagent_send_edid_raw_data(tx_dev->dev, (u8 *) edid, edid_size);
	if (ret)
		return ret;

	return 0;
}

static void hdmitx_print_sink_capability(asoc_hdmi_t *p_this)
{
	pr_info("\n=================== %s sink_capabilities ========================",
		p_this->fake ? "fake EDID " : "");
	pr_info("hdmi_mode=%u ", p_this->sink_cap.hdmi_mode);
	pr_info("est_modes=%u ", p_this->sink_cap.est_modes);
	pr_info("\n");
	//VSBD
	pr_info("cec_phy_addr=0x%02x 0x%02x ", p_this->sink_cap.cec_phy_addr[0], p_this->sink_cap.cec_phy_addr[1]);
	pr_info("support_AI=%d ", p_this->sink_cap.support_AI);
	pr_info("DC_Y444=%d ", p_this->sink_cap.DC_Y444);
	pr_info("color_space=%u ", p_this->sink_cap.color_space);
	pr_info("dvi_dual=%u ", p_this->sink_cap.dvi_dual);
	pr_info("max_tmds_clock=%d ", p_this->sink_cap.max_tmds_clock);
	pr_info("latency_present=%d %d ", p_this->sink_cap.latency_present[0], p_this->sink_cap.latency_present[1]);
	pr_info("video_latency=%u %u ", p_this->sink_cap.video_latency[0], p_this->sink_cap.video_latency[1]);
	pr_info("audio_latency=%u %u ", p_this->sink_cap.audio_latency[0], p_this->sink_cap.audio_latency[1]);
	pr_info("\n");

	pr_info("structure_all=0x%x\n", p_this->sink_cap.structure_all);

	//Video
	pr_info("display_info.width_mm=%u ", p_this->sink_cap.display_info.width_mm);
	pr_info("display_info.height_mm=%u ", p_this->sink_cap.display_info.height_mm);
	pr_info("display_info.bpc=%u ", p_this->sink_cap.display_info.bpc);
	pr_info("display_info.color_formats=%u ", p_this->sink_cap.display_info.color_formats);
	pr_info("display_info.cea_rev=%u ", p_this->sink_cap.display_info.cea_rev);
	pr_info("vic=0x%llx ", p_this->sink_cap.vic);
	pr_info("extended_vic=%u ", p_this->sink_cap.extended_vic);
	pr_info("vic2=0x%llx ", p_this->sink_cap.vic2);
	pr_info("vic2_420=0x%llx ", p_this->sink_cap.vic2_420);
	pr_info("HDR metadata: et(0x%02x) sm(0x%02x) max_luminace(0x%02x) max_frame_avg(0x%02x) min_luminace(0x%02x) ",
			p_this->sink_cap.vout_edid_data.et, p_this->sink_cap.vout_edid_data.sm,
			p_this->sink_cap.vout_edid_data.max_luminace, p_this->sink_cap.vout_edid_data.max_frame_avg,
			p_this->sink_cap.vout_edid_data.min_luminace);
	pr_info("Color characteristics: red_green_lo(0x%02x) black_white_lo(0x%02x) ",
		p_this->sink_cap.vout_edid_data.red_green_lo, p_this->sink_cap.vout_edid_data.black_white_lo);
	pr_info("Color characteristics: red_x(0x%02x) red_y(0x%02x) ",
		p_this->sink_cap.vout_edid_data.red_x, p_this->sink_cap.vout_edid_data.red_y);
	pr_info("Color characteristics: green_x(0x%02x) green_y(0x%02x) ",
		p_this->sink_cap.vout_edid_data.green_x, p_this->sink_cap.vout_edid_data.green_y);
	pr_info("Color characteristics: blue_x(0x%02x) blue_y(0x%02x) ",
		p_this->sink_cap.vout_edid_data.blue_x, p_this->sink_cap.vout_edid_data.blue_y);
	pr_info("Color characteristics: white_x(0x%02x) white_y(0x%02x) ",
		p_this->sink_cap.vout_edid_data.white_x, p_this->sink_cap.vout_edid_data.white_y);

	pr_info("=================================================================\n");
}

static void hdmitx_print_sink_info(asoc_hdmi_t *p_this)
{
	pr_info("Dump %s EDID\n", p_this->fake ? "fake " : "");

	print_color_formats(p_this->sink_cap.display_info.color_formats);

	if (p_this->sink_cap.hdmi_mode == HDMI_MODE_HDMI)
		pr_info("DEVICE MODE: HDMI Mode ");
	else if (p_this->sink_cap.hdmi_mode == HDMI_MODE_DVI)
		pr_info("DEVICE MODE: DVI Mode ");
	else if (p_this->sink_cap.hdmi_mode == HDMI_MODE_FRL)
		pr_info("DEVICE MODE: FRL Mode ");

	print_cea_modes(p_this->sink_cap.vic,
		p_this->sink_cap.vic2, p_this->sink_cap.vic2_420);

	pr_info("CEC Address: 0x%x%02x ", p_this->sink_cap.cec_phy_addr[0],
					p_this->sink_cap.cec_phy_addr[1]);

	print_deep_color(p_this->sink_cap.display_info.bpc);

	if (p_this->sink_cap.DC_Y444)
		pr_info("Support Deep Color in YCbCr444 ");

	if (p_this->sink_cap.max_tmds_clock)
		pr_info("Max TMDS clock: %d", p_this->sink_cap.max_tmds_clock);

	pr_info("Video Latency: %d %d ", p_this->sink_cap.video_latency[0],
					p_this->sink_cap.video_latency[1]);

	pr_info("Audio Latency: %d %d ", p_this->sink_cap.audio_latency[0],
					p_this->sink_cap.audio_latency[1]);

	print_color_space(p_this->sink_cap.color_space);

	hdmi_print_raw_edid((unsigned char *)p_this->hdmitx_edid_info.raw_edid,
		p_this->hdmitx_edid_info.override_extdb_count);
}

int get_full_edid(hdmitx_device_t *tx_dev, struct full_edid *f_edid)
{
	asoc_hdmi_t *data = tx_dev->hdmi_data;
	int ret = 0;
	unsigned int edid_size;

	if (!hdmitx_switch_get_state(tx_dev->dev)) {
		pr_err("[%s]hpd unplug", __func__);
		return -EREMOTEIO;
	}

	if (!(data->sink_cap_available)) {
		pr_err("[%s]sink cap is not available", __func__);
		return -ENOMSG;
	}

	edid_size = (unsigned int) *((u8 *)data->hdmitx_edid_info.raw_edid + 0x7e);
	if (data->hdmitx_edid_info.override_extdb_count > edid_size)
		edid_size = data->hdmitx_edid_info.override_extdb_count;

	edid_size = (edid_size + 1) * EDID_LENGTH;

	if (f_edid->buf_size < edid_size) {
		pr_err("[%s] buf_size(%u) < edid_size(%u)", __func__,
					f_edid->buf_size, edid_size);
		return -EINVAL;
	}

	ret = copy_to_user(f_edid->edid_ptr,
			(u8 *)data->hdmitx_edid_info.raw_edid, edid_size);

	if (ret != 0)
		ret = -EFAULT;

	return ret;
}

int set_fake_edid(hdmitx_device_t *tx_dev, struct fake_edid *fake)
{
	asoc_hdmi_t *tx_data = tx_dev->hdmi_data;
	struct edid *raw_edid;

	tx_data->fake = false;
	hdmitx_reset_sink_capability(tx_data);

	if (fake->size > 0) { /* set fake */
		if (fake->size % EDID_LENGTH != 0) {
			dev_err(tx_dev->dev, "%s: invalid EDID size.", __func__);
			return -EINVAL;
		}

		raw_edid = kmalloc(fake->size, GFP_KERNEL);
		if (raw_edid == NULL) {
			dev_err(tx_dev->dev,
				"%s: Could not allocate fake EDID block.", __func__);
			return -ENOMEM;
		}

		if (copy_from_user(raw_edid, fake->data_ptr, fake->size)) {
			dev_err(tx_dev->dev,
				"%s: failed to copy EDID data from user space", __func__);
			kfree(raw_edid);
			return -EFAULT;
		}

		tx_data->hdmitx_edid_info.raw_edid = raw_edid;
		tx_data->fake = true;
	}

	return hdmitx_get_sink_capability(tx_dev);
}

int get_edid_block(hdmitx_device_t *tx_dev, struct block_edid *b_edid)
{
	int ret;
	u8 *buffer;

	ret =  -EFAULT;

	buffer = kmalloc(b_edid->block_size * EDID_LENGTH, GFP_KERNEL);
	if (!buffer) {
		dev_err(tx_dev->dev, "%s:kmalloc failed", __func__);
		ret =  -EBUSY;
		goto exit;
	}

	ret = rtk_get_edid_block(tx_dev->dev, b_edid->block_index,
			b_edid->block_size, buffer);
	if (ret != 0) {
		ret = -EIO;
		goto free;
	}

	ret = copy_to_user(b_edid->edid_ptr, buffer,
			b_edid->block_size * EDID_LENGTH);
	if (ret != 0)
		ret = -EFAULT;

free:
	kfree(buffer);
exit:
	return ret;
}

int ops_config_tv_system(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM tv_system;
	unsigned char scramble;
	asoc_hdmi_t *data = tx_dev->hdmi_data;
	

	pr_devel("%s", __func__);

	if (copy_from_user(&tv_system, arg, sizeof(tv_system))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	pr_info("TV system: hdmiMode(%u) standard(%u) dataByte1(0x%02x) dataInt0(0x%02x)",
				tv_system.hdmiInfo.hdmiMode,
				tv_system.videoInfo.standard,
				tv_system.hdmiInfo.dataByte1,
				tv_system.hdmiInfo.dataInt0);

	if (tv_system.hdmiInfo.hdmiMode == VO_HDMI_ON) {
		/* [Bit0] HDMI2.x */
		if ((data->hdmitx_edid_info.hdmi_id == HDMI_2P0_IDENTIFIER) ||
			(data->hdmitx_edid_info.hdmi_id == HDMI_2P1_IDENTIFIER))
			tv_system.hdmiInfo.hdmi2px_feature |= HDMI2PX_2P0;

		scramble = hdmitx_send_scdc_TmdsConfig(&data->hdmitx_edid_info,
			tv_system.videoInfo.standard, tv_system.hdmiInfo.dataInt0,
			 tv_system.hdmiInfo.dataByte1);

		if (scramble != 0)
			tv_system.hdmiInfo.hdmi2px_feature |= HDMI2PX_SCRAMBLE;
	} else if (tv_system.hdmiInfo.hdmiMode == VO_HDMI_OFF) {
		if (hdmi_clk_always_on)
			tv_system.hdmiInfo.hdmi_off_mode = VO_HDMI_OFF_CLOCK_ON;
	}

	if (__RTK_HDMI_GENERIC_DEBUG__)
		hdmitx_dump_VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM(&tv_system, NULL);

	return RPC_TOAGENT_HDMI_Config_TV_System(tx_dev,
					 &data->hdmitx_edid_info, &tv_system);
}

int ops_config_avi_info(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct VIDEO_RPC_VOUT_CONFIG_HDMI_INFO_FRAME info_frame;

	pr_devel("%s", __func__);

	if (copy_from_user(&info_frame, arg, sizeof(info_frame))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	return RPC_TOAGENT_HDMI_Config_AVI_Info(tx_dev, &info_frame);
}

int ops_set_frequency(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct AUDIO_HDMI_SET set;
	unsigned char frl_rate;
	unsigned int audio_frl_flag;
	int opt;

	pr_devel("%s", __func__);

	if (copy_from_user(&set, arg, sizeof(set))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	trace_hdmitx_set_frequency(set.HDMI_Frequency);

	if (tx_dev->frl)
		frl_rate = tx_dev->frl->current_frl_rate; 
	else
		frl_rate = 0;

	switch (frl_rate) {
	case 1:
		audio_frl_flag = AUDIO_FRL_RATE1;
		break;
	case 2:
		audio_frl_flag = AUDIO_FRL_RATE2;
		break;
	case 3:
		audio_frl_flag = AUDIO_FRL_RATE3;
		break;
	default:
		audio_frl_flag = 0;
		break;
	}

	pr_devel("Set audio frl=%u freq=%u",
		frl_rate, set.HDMI_Frequency);

	set.HDMI_Frequency = audio_frl_flag | set.HDMI_Frequency;

	opt = get_ao_rpc_opt(tx_dev);

	return RPC_TOAGENT_HDMI_Set(tx_dev, opt, &set);
}

int ops_set_audio_mute(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct AUDIO_HDMI_MUTE_INFO mute_info;
	int opt;

	pr_devel("%s", __func__);

	if (copy_from_user(&mute_info, arg, sizeof(mute_info))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	opt = get_ao_rpc_opt(tx_dev);

	return RPC_TOAGENT_HDMI_Mute(tx_dev, opt, &mute_info);
}

int ops_send_audio_vsdb_data(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct AUDIO_HDMI_OUT_VSDB_DATA vsdb_data;
	int opt;

	pr_devel("%s", __func__);

	if (copy_from_user(&vsdb_data, arg, sizeof(vsdb_data))) {
		pr_err("%s:failed to copy from user ! ", __func__);
		return -EFAULT;
	}

	opt = get_ao_rpc_opt(tx_dev);

	return RPC_TOAGENT_HDMI_OUT_VSDB(tx_dev, opt, &vsdb_data);
}

int ops_send_audio_edid2(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct AUDIO_HDMI_OUT_EDID_DATA2 edid2;
	int opt;

	pr_devel("%s", __func__);

	if (copy_from_user(&edid2, arg, sizeof(edid2))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	opt = get_ao_rpc_opt(tx_dev);

	return RPC_ToAgent_HDMI_OUT_EDID_0(tx_dev, opt, &edid2);
}


int ops_get_sink_cap(void __user *arg, hdmitx_device_t *tx_dev)
{
	asoc_hdmi_t *data = tx_dev->hdmi_data;
	int ret = 0;

	pr_devel("%s", __func__);
	
	if (!hdmitx_switch_get_state(tx_dev->dev)) {
		pr_err("[%s]hpd unplug", __func__);
		return -EREMOTEIO;		
	}

	if (!(data->sink_cap_available)) {
		pr_err("[%s]sink cap is not available", __func__);
		return -ENOMSG;
	}

	if (copy_to_user(arg, &data->sink_cap, sizeof(data->sink_cap))) {
		pr_err("%s:failed to copy to user !", __func__);
		return -EFAULT;
	}

	if (__RTK_HDMI_GENERIC_DEBUG__)
		hdmitx_print_sink_capability(data);

	return ret;
}

int ops_get_raw_edid(void __user *arg, hdmitx_device_t *tx_dev)
{
	asoc_hdmi_t *data = tx_dev->hdmi_data;
	int ret = 0;

	pr_devel("%s", __func__);

	if (!hdmitx_switch_get_state(tx_dev->dev)) {
		pr_err("[%s]hpd unplug", __func__);
		return -EREMOTEIO;		
	}

	if (!(data->sink_cap_available)) {
		pr_err("[%s]sink cap is not available", __func__);
		return -ENOMSG;
	}

	if (copy_to_user(arg, (const void *)data->hdmitx_edid_info.raw_edid, sizeof(struct raw_edid))) {
		pr_err("%s:failed to copy to user !", __func__);
		return -EFAULT;
	}

	return ret;
}

int ops_get_link_status(void __user *arg, hdmitx_device_t *tx_dev)
{
	static unsigned long prev_jif = 0;
	static unsigned int c = 0;
	static int prev_status = -1;
	int ret = 0;
	int real_hpd;
	int status = 0;

	pr_devel("%s", __func__);

	if (copy_from_user(&real_hpd, arg, sizeof(real_hpd))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	status = show_hpd_status(tx_dev->dev, real_hpd ? true:false);

	if (copy_to_user(arg, &status, sizeof(status))) {
		pr_err("%s:failed to copy to user !", __func__);
		return -EFAULT;
	}

	if (status != prev_status ||
		(jiffies_to_msecs(jiffies - prev_jif) > T_DEBOUNCE_MS)) {
		trace_hdmitx_get_link_status(status, c);
		prev_status = status;
		prev_jif = jiffies;
		c = 0;
	} else {
		c++;
	}

	return ret;
}

int ops_get_video_config(void __user *arg, asoc_hdmi_t *data)
{
	int ret = 0;

	pr_devel("%s", __func__);

	if (copy_to_user(arg, &data->sink_cap.vic,
		sizeof(data->sink_cap.vic))) {

		pr_err("%s:failed to copy to user !", __func__);
		return -EFAULT;
	}

	trace_hdmitx_get_video_config(data->sink_cap.vic);

	return ret;
}

int ops_send_AVmute(void __user *arg, hdmitx_device_t *tx_dev)
{
	int flag;

	pr_devel("%s", __func__);

	if (copy_from_user(&flag, arg, sizeof(flag))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	trace_hdmitx_send_AVmute(flag);

	return hdmitx_send_AVmute(tx_dev->dev, flag);
}

int ops_check_rx_sense(void __user *arg, hdmitx_device_t *tx_dev)
{
	static unsigned long prev_jif = 0;
	static unsigned int c = 0;
	static int prev_rxsense = -1;
	int rx_sense;
	int ret = 0;

	pr_devel("%s", __func__);

	if (tx_dev->rxsense_mode > RXSENSE_PASSIVE_MODE)
		rx_sense = hdmitx_get_rxsense(tx_dev);
	else
		rx_sense = hdmitx_check_rx_sense(tx_dev);

	if (copy_to_user(arg, &rx_sense, sizeof(rx_sense))) {
		pr_err("%s:failed to copy to user !", __func__);
		return -EFAULT;
	}

	if ((rx_sense != prev_rxsense) ||
		(jiffies_to_msecs(jiffies - prev_jif) > T_DEBOUNCE_MS)) {
		trace_hdmitx_check_rx_sense(rx_sense, c);
		prev_rxsense = rx_sense;
		prev_jif = jiffies;
		c = 0;
	} else {
		c++;
	}

	return ret;
}

int ops_get_extension_blk_count(void __user *arg, hdmitx_device_t *tx_dev)
{
	asoc_hdmi_t *data = tx_dev->hdmi_data;
	int ret = 0;
	int count = 0;

	pr_devel("%s", __func__);

	if (!hdmitx_switch_get_state(tx_dev->dev)) {
		pr_err("[%s]hpd unplug", __func__);
		return -EREMOTEIO;		
	}

	if (!(data->sink_cap_available)) {
		pr_err("[%s]sink cap is not available", __func__);
		return -ENOMSG;
	}

	count = (int) *((u8 *)data->hdmitx_edid_info.raw_edid + 0x7e);
	if (data->hdmitx_edid_info.override_extdb_count > count)
		count = data->hdmitx_edid_info.override_extdb_count;

	if (copy_to_user(arg, &count, sizeof(int))) {
		pr_err("%s:failed to copy to user !", __func__);
		return -EFAULT;
	}

	trace_hdmitx_get_extension_blk_count(count);

	return ret;
}


int ops_get_full_edid(void __user *arg, hdmitx_device_t *tx_dev)
{
	int ret;
	struct full_edid f_edid;

	pr_devel("%s", __func__);

	if (copy_from_user(&f_edid, arg, sizeof(struct full_edid))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	ret = get_full_edid(tx_dev, &f_edid);

	return ret;
}


int ops_send_vout_edid_data(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct VIDEO_RPC_VOUT_EDID_DATA vout_edid_data;

	pr_devel("%s", __func__);

	if (copy_from_user(&vout_edid_data, arg, sizeof(vout_edid_data))) {
		pr_err("%s:failed to copy from user !", __func__);
		return -EFAULT;
	}
	return RPC_ToAgent_Vout_EDIDdata(tx_dev, &vout_edid_data);
}

int ops_get_edid_support_list(void __user *arg, hdmitx_device_t *tx_dev)
{
	asoc_hdmi_t *data = tx_dev->hdmi_data;
	int i, ret = 0;
	struct hdmi_support_list *support_list = &data->support_list;

	pr_devel("%s", __func__);

	if (!hdmitx_switch_get_state(tx_dev->dev)) {
        	pr_err("[%s]hpd unplug", __func__);
                ret = -EREMOTEIO;
		goto exit;
        }
	
	if (!(data->sink_cap_available)) {
		pr_err("[%s]sink cap is not available", __func__);
		ret = -ENOMSG;
		goto exit;
	}

	if (copy_to_user(arg, support_list, sizeof(struct hdmi_support_list))) {
		pr_err("%s:failed to copy to user ! ", __func__);
		ret = -EFAULT;
		goto exit;
	}

	if (!tracing_is_on())
		goto exit;

	for (i = 0; i < support_list->tx_support_size; i++)
		trace_hdmitx_get_edid_support_list(
			&support_list->tx_support[i], i);

exit:
	return ret;
}

int ops_set_output_format(void __user *arg, hdmitx_device_t *tx_dev)
{
	int ret = 0;
	struct hdmi_format_setting format_setting;

	pr_devel("%s", __func__);

	if (copy_from_user(&format_setting, arg, sizeof(format_setting))) {
			pr_err("%s:failed to copy from user !", __func__);
			return -EFAULT;
	}

	trace_hdmitx_set_output_format(&format_setting);
	if (tx_dev->use_new_mac)
		ret = set_hdmitx_format_new(tx_dev->dev, &format_setting);
	else
		ret = set_hdmitx_format(tx_dev->dev, &format_setting);

	if (!IS_ENABLED(CONFIG_RTK_HDCP_1x) ||
		(format_setting.mode == DISPLAY_MODE_OFF))
		goto skip_soc_check;

	if (soc_device_match(rtk_soc_morbius) &&
		tx_dev->use_new_mac) {
		ta_hdcp14_init();
		ta_hdcp_set_rekey_win(4);
	} else if (soc_device_match(rtk_soc_hercules) &&
		(format_setting.vic == VIC_720X480P60)) {
		ta_hdcp14_init();
		ta_hdcp_fix480p();
	} else if (soc_device_match(rtk_soc_parker)) {
		switch (format_setting.vic) {
		case VIC_720X480P60:
		case VIC_720X480I60:
		case VIC_720X576P50:
		case VIC_720X576I50:
			ta_hdcp14_init();
			ta_hdcp_set_keepout_win(0x1fa, 0x688);
			break;
		default:
			break;
		};
	}

skip_soc_check:

	return ret;
}

int ops_get_output_format(void __user *arg, hdmitx_device_t *tx_dev)
{
	int ret = 0;
	struct hdmi_format_setting format_setting;

	pr_devel("%s", __func__);

	if (tx_dev->use_new_mac)
		ret = get_hdmitx_format_new(tx_dev->dev, &format_setting);
	else
		ret = get_hdmitx_format(tx_dev->dev, &format_setting);

	if (ret != 0)
		goto exit;

	if (copy_to_user(arg, &format_setting, sizeof(struct hdmi_format_setting))) {
		pr_err("%s:failed to copy to user ! ", __func__);
		ret =  -EFAULT;
	}

	trace_hdmitx_get_output_format(&format_setting);
exit:
	return ret;
}

int ops_set_interface_type(void __user *arg)
{
	int ret = 0;
	int type;

	pr_devel("%s", __func__);

	if (copy_from_user(&type, arg, sizeof(type))) {
		pr_err("%s:failed to copy from user !", __func__);
		ret = -EFAULT;
	}

	trace_hdmitx_set_interface_type(type);
	set_vo_interface_type(type);

	return ret;
}

int ops_get_config_tv_system(void __user *arg, hdmitx_device_t *tx_dev)
{
	int ret;
	struct VIDEO_RPC_VOUT_CONFIG_TV_SYSTEM tv_system;

	ret = RPC_ToAgent_QueryConfigTvSystem(tx_dev, &tv_system);

	if (ret != 0) {
		ret =  -EFAULT;
		goto exit;
	}

	if (copy_to_user(arg, &tv_system, sizeof(tv_system))) {
		pr_err("%s:failed to copy to user ! ", __func__);
		ret =  -EFAULT;
	}

	ret = 0;

exit:
	return ret;
}

int ops_wait_hotplug(void __user *arg, hdmitx_device_t *tx_dev)
{
	int ret= 0;
	int status = 0;
	dev_dbg(tx_dev->dev, "%s", __func__);

	status = show_hpd_status(tx_dev->dev, false);

	wait_event_interruptible(tx_dev->hpd_wait, ((status!=show_hpd_status(tx_dev->dev, false)) || (!hpdDetect)));

	status = show_hpd_status(tx_dev->dev, false);

	if (copy_to_user(arg,&status,sizeof(int))) {
		dev_err(tx_dev->dev, "%s:failed to copy to user ! ", __func__);
		return -EFAULT;
	}

	return ret;
}

int ops_set_hotplug_detection(void __user *arg, hdmitx_device_t *tx_dev)
{
	int ret= 0;
	int fEn = 0;
	dev_dbg(tx_dev->dev, "%s", __func__);

	if (copy_from_user(&fEn, arg, sizeof(fEn))) {
		dev_err(tx_dev->dev, "%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	hpdDetect = fEn;
	if(fEn == 0)
		wake_up_interruptible(&tx_dev->hpd_wait);

	return ret;
}

int ops_get_edid_block(void __user *arg, hdmitx_device_t *tx_dev)
{
	int ret;
	struct block_edid b_edid;

	ret =  -EFAULT;
	if (copy_from_user(&b_edid, arg, sizeof(b_edid))) {
		dev_err(tx_dev->dev, "%s:failed to copy from user !", __func__);
		ret =  -EFAULT;
		goto exit;
	}

	ret = get_edid_block(tx_dev, &b_edid);

exit:
	return ret;
}

int ops_set_vrr(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct VIDEO_RPC_VOUT_HDMI_VRR vrr_config;
	int ret = -EFAULT;

	if (copy_from_user(&vrr_config, arg, sizeof(vrr_config))) {
		HDMI_ERROR("%s:failed to copy from user !", __func__);
		goto exit;
	}

	dev_info(tx_dev->dev, "Set vrr_function=%u vrr_act=%u",
		vrr_config.vrr_function, vrr_config.vrr_act);

	ret = RPC_ToAgent_SET_HDMI_VRR(tx_dev, &vrr_config);

exit:
	return ret;
}

int ops_ctrl_5v(void __user *arg, hdmitx_device_t *tx_dev)
{
	int ctrl5v_cmd;
	int ret = -ENODEV;

	if (copy_from_user(&ctrl5v_cmd, arg, sizeof(ctrl5v_cmd))) {
		HDMI_ERROR("%s:failed to copy from user !", __func__);
		goto exit;
	}

	ctrl5v_cmd = ctrl_hdmi_5v(tx_dev->dev, ctrl5v_cmd);
	if (ctrl5v_cmd < 0)
		goto exit;

	ret = copy_to_user(arg, &ctrl5v_cmd, sizeof(ctrl5v_cmd));
	if (ret != 0)
		ret = -EFAULT;

exit:
	return ret;
}

int ops_set_fake_edid(void __user *arg, hdmitx_device_t *tx_dev)
{
	struct fake_edid fake;
	int ret;

	pr_devel("%s", __func__);

	if (copy_from_user(&fake, arg, sizeof(struct fake_edid))) {
		dev_err(tx_dev->dev, "%s:failed to copy from user !", __func__);
		return -EFAULT;
	}

	ret = set_fake_edid(tx_dev, &fake);

	return ret;
}

int ops_skip_edid_check(void __user *arg, hdmitx_device_t *tx_dev)
{
	int skip_check;
	int ret = -EINVAL;

	if (copy_from_user(&skip_check, arg, sizeof(skip_check))) {
		HDMI_ERROR("%s:failed to copy from user !", __func__);
		goto exit;
	}

	dev_info(tx_dev->dev, "Set allow invalid edid, skip_check=%d", skip_check);

	if ((skip_check == 0) || (skip_check == 1)) {
		rtk_set_allow_invalid_edid(skip_check);
		ret = 0;
	}
exit:
	return ret;
}
