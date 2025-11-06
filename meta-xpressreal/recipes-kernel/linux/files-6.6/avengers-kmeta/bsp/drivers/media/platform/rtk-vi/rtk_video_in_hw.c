// SPDX-License-Identifier: GPL-2.0-only

#include "rtk_video_in.h"
#include "rtk_video_in_trace.h"

#define VI_MAC1_REG_OFFSET   (VI_DECODER_1 - VI_DECODER)
#define ABS(x) ((x) < 0 ? -(x) : (x))

static bool g_need_init_crt = true;

static unsigned int get_macreg_offset(u8 ch_index)
{
	unsigned int offset;

	switch (ch_index) {
	case CH_0:
		offset = 0;
		break;
	case CH_1:
		offset = VI_MAC1_REG_OFFSET;
		break;
	default:
		offset = 0;
		break;
	}

	return offset;
}

static void rtk_vi_mac_reset(struct rtk_vi *vi, unsigned char ch_index,
		unsigned char enable)
{
	unsigned int val;

	if (ch_index > CH_MAX)
		return;

	dev_info(vi->dev, "%s mac%u reset\n",
		enable ? "Enable":"Disable", ch_index);

	/* Active Low */
	switch (ch_index) {
	case CH_0:
		val = VI_RST_CTRL_mac_0(1) | VI_RST_CTRL_write_data(!enable);
		break;
	case CH_1:
		val = VI_RST_CTRL_mac_1(1) | VI_RST_CTRL_write_data(!enable);
		break;
	}

	regmap_write(vi->vi_reg, VI_RST_CTRL, val);


}

static void rtk_vi_mac_ctrl(struct rtk_vi *vi, unsigned char ch_index,
		unsigned char enable)
{
	unsigned int reg_val;

	if (ch_index > CH_MAX)
		return;

	dev_info(vi->dev, "%s mac%u go\n",
		enable ? "Enable":"Disable", ch_index);

	regmap_read(vi->vi_reg, VI_FC, &reg_val);

	switch (ch_index) {
	case CH_0:
		reg_val &= ~VI_FC_mac0_go_mask;
		reg_val |= VI_FC_mac0_go(enable);
		break;
	case CH_1:
		reg_val &= ~VI_FC_mac1_go_mask;
		reg_val |= VI_FC_mac1_go(enable);
		break;
	}

	regmap_write(vi->vi_reg, VI_FC, reg_val);

	trace_vi_mac_crtl(enable);
}

static void rtk_vi_cascade_mac_ctrl(struct rtk_vi *vi,
		u8 ch_index, u8 enable)
{
	u32 reg_val;
	u32 ch0_ready, ch1_ready;

	if (ch_index > CH_MAX)
		goto exit;

	if ((!enable) || (!vi->detect_done))
		goto cascade_go;

	regmap_read(vi->vi_reg, VI_ADR_ENTRY3_ST, &reg_val);
	ch0_ready = VI_ADR_ENTRY3_ST_get_valid(reg_val);

	regmap_read(vi->vi_reg, VI_ADR_1_ENTRY3_ST, &reg_val);
	ch1_ready = VI_ADR_1_ENTRY3_ST_get_valid(reg_val);

	if (!ch0_ready || !ch1_ready) {
		dev_info(vi->dev, "ENTRY is not ready, skip cascade ctrl\n");
		goto exit;
	}

cascade_go:
	if ((vi->cascade_mode == CASCADE_SLAVE) &&
		(!vi->detect_done) && enable) {
		vi->detect_done = true;
	} else {
		dev_info(vi->dev, "%s cascade go\n", enable ? "Enable":"Disable");
		regmap_write(vi->vi_reg, VI_FC, VI_FC_cascade_go(enable));
	}
exit:
	trace_vi_mac_crtl(enable);
}

static void rtk_vi_crc_ctrl(struct rtk_vi *vi, u8 ch_index, u8 enable)
{
	unsigned int val;

	if (ch_index > CH_MAX)
		return;

	dev_dbg(vi->dev, "%s mac%u crc\n",
		enable ? "Enable":"Disable", ch_index);

	switch (ch_index) {
	case CH_0:
		val = VI_CRC_CTRL_mac_0_en(1) | VI_CRC_CTRL_write_data(enable);
		break;
	case CH_1:
		val = VI_CRC_CTRL_mac_1_en(1) | VI_CRC_CTRL_write_data(enable);
		break;
	}

	regmap_write(vi->vi_reg, VI_CRC_CTRL, val);
}

static void rtk_vi_get_scaling_coeffs(struct rtk_vi *vi,
		u32 *coeff, int delta, int init_phase, int taps)
{
	u32 i;

	dev_dbg(vi->dev, "delta=0x%x\n", delta);
	dev_dbg(vi->dev, "ABS(delta - 0x10000)=0x%x\n", ABS(delta - 0x10000));

	if (taps > 8)
		return;

	if (ABS(delta - 0x10000) < 0x80 && init_phase == 0) {
		/* Almost 1X */
		for (i = 1 ; i <= 4; i++)
			coeff[(taps << 2) - i] = 0x1000;/* coeff Q12 */

	} else if (delta >= 0x10000) {
		int end = 0;

		end = taps * 4;

		if (delta <= 0x20000 || (taps == 2 && delta <= 0x40000)) {
			coeff[end-8] = 0x0249;
			coeff[end-7] = 0x0000;
			coeff[end-6] = 0x06DB;
			coeff[end-5] = 0x0492;
			coeff[end-4] = 0x0B6D;
			coeff[end-3] = 0x0924;
			coeff[end-2] = 0x1000;
			coeff[end-1] = 0x0DB6;
		} else if (delta <= 0x40000) {
			coeff[end-16] = 0x0001;
			coeff[end-15] = 0x0000;
			coeff[end-14] = 0x0035;
			coeff[end-13] = 0x000f;
			coeff[end-12] = 0x00f8;
			coeff[end-11] = 0x007f;
			coeff[end-10] = 0x02aa;
			coeff[end-9] = 0x01ad;
			coeff[end-8] = 0x03f3;
			coeff[end-7] = 0x02aa;
			coeff[end-6] = 0x06ef;
			coeff[end-5] = 0x056b;
			coeff[end-4] = 0x098c;
			coeff[end-3] = 0x085b;
			coeff[end-2] = 0x0aaa;
			coeff[end-1] = 0x0a5d;
		} else {
			int k;

			k = taps*4;/* taps*8/2 */
			for (i = 0; i < k; i++)
				coeff[i] = 0x1000/taps;
		}

	} else {
		/* upscaling */
	}

}

static void rtk_vi_hs_scaler(struct rtk_vi *vi,
		unsigned int hsi_phase,	unsigned int hsd_out,
		unsigned int hsd_delta)
{
	u32 offset;

	offset = get_macreg_offset(vi->ch_index);

	regmap_write(vi->vi_reg, VI_ISP_HSD + offset,
		VI_ISP_HSD_delta(hsd_delta));

	regmap_write(vi->vi_reg, VI_ISP_HSD_W + offset,
		VI_ISP_HSD_W_out(hsd_out));
}

static void rtk_vi_hs_coeff(struct rtk_vi *vi, int delta)
{
	u32 c[32] = {0};
	u32 cc[16] = {0};
	u32 offset;

	offset = get_macreg_offset(vi->ch_index);

	rtk_vi_get_scaling_coeffs(vi, c, delta, 0, 8);
	rtk_vi_get_scaling_coeffs(vi, cc, delta, 0, 4);

	/* for Y */
	regmap_write(vi->vi_reg, VI_ISP_HSYC_0 + offset,
		VI_ISP_HSYC_c1(c[1]) | VI_ISP_HSYC_c0(c[0]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_1 + offset,
		VI_ISP_HSYC_c1(c[3]) | VI_ISP_HSYC_c0(c[2]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_2 + offset,
		VI_ISP_HSYC_c1(c[5]) | VI_ISP_HSYC_c0(c[4]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_3 + offset,
		VI_ISP_HSYC_c1(c[7]) | VI_ISP_HSYC_c0(c[6]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_4 + offset,
		VI_ISP_HSYC_c1(c[9]) | VI_ISP_HSYC_c0(c[8]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_5 + offset,
		VI_ISP_HSYC_c1(c[11]) | VI_ISP_HSYC_c0(c[10]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_6 + offset,
		VI_ISP_HSYC_c1(c[13]) | VI_ISP_HSYC_c0(c[12]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_7 + offset,
		VI_ISP_HSYC_c1(c[15]) | VI_ISP_HSYC_c0(c[14]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_8 + offset,
		VI_ISP_HSYC_c1(c[17]) | VI_ISP_HSYC_c0(c[16]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_9 + offset,
		VI_ISP_HSYC_c1(c[19]) | VI_ISP_HSYC_c0(c[18]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_10 + offset,
		VI_ISP_HSYC_c1(c[21]) | VI_ISP_HSYC_c0(c[20]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_11 + offset,
		VI_ISP_HSYC_c1(c[23]) | VI_ISP_HSYC_c0(c[22]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_12 + offset,
		VI_ISP_HSYC_c1(c[25]) | VI_ISP_HSYC_c0(c[24]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_13 + offset,
		VI_ISP_HSYC_c1(c[27]) | VI_ISP_HSYC_c0(c[26]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_14 + offset,
		VI_ISP_HSYC_c1(c[29]) | VI_ISP_HSYC_c0(c[28]));
	regmap_write(vi->vi_reg, VI_ISP_HSYC_15 + offset,
		VI_ISP_HSYC_c1(c[31]) | VI_ISP_HSYC_c0(c[30]));

	/* for U,V */
	regmap_write(vi->vi_reg, VI_ISP_HSCC_0 + offset,
		VI_ISP_HSCC_c1(cc[1]) | VI_ISP_HSCC_c0(cc[0]));
	regmap_write(vi->vi_reg, VI_ISP_HSCC_1 + offset,
		VI_ISP_HSCC_c1(cc[3]) | VI_ISP_HSCC_c0(cc[2]));
	regmap_write(vi->vi_reg, VI_ISP_HSCC_2 + offset,
		VI_ISP_HSCC_c1(cc[5]) | VI_ISP_HSCC_c0(cc[4]));
	regmap_write(vi->vi_reg, VI_ISP_HSCC_3 + offset,
		VI_ISP_HSCC_c1(cc[7]) | VI_ISP_HSCC_c0(cc[6]));
	regmap_write(vi->vi_reg, VI_ISP_HSCC_4 + offset,
		VI_ISP_HSCC_c1(cc[9]) | VI_ISP_HSCC_c0(cc[8]));
	regmap_write(vi->vi_reg, VI_ISP_HSCC_5 + offset,
		VI_ISP_HSCC_c1(cc[11]) | VI_ISP_HSCC_c0(cc[10]));
	regmap_write(vi->vi_reg, VI_ISP_HSCC_6 + offset,
		VI_ISP_HSCC_c1(cc[13]) | VI_ISP_HSCC_c0(cc[12]));
	regmap_write(vi->vi_reg, VI_ISP_HSCC_7 + offset,
		VI_ISP_HSCC_c1(cc[15]) | VI_ISP_HSCC_c0(cc[14]));
}

static void rtk_vi_vs_scaler(struct rtk_vi *vi,
		u32 vsi_phase, u32 vsd_out,
		u32 vsd_delta)
{
	u32 offset;

	offset = get_macreg_offset(vi->ch_index);

	regmap_write(vi->vi_reg, VI_ISP_VSI + offset,
		VI_ISP_VSI_phase(vsi_phase));

	if (vi->is_interlace) {
		regmap_write(vi->vi_reg, VI_ISP_VSD_TF + offset,
			VI_ISP_VSD_TF_delta(vsd_delta));

		regmap_write(vi->vi_reg, VI_ISP_VSD_H_TF + offset,
			VI_ISP_VSD_H_TF_out(vsd_out/2));

		regmap_write(vi->vi_reg, VI_ISP_VSD_BF + offset,
			VI_ISP_VSD_BF_delta(vsd_delta));

		regmap_write(vi->vi_reg, VI_ISP_VSD_H_BF + offset,
			VI_ISP_VSD_H_BF_out(vsd_out/2));
	} else {
		regmap_write(vi->vi_reg, VI_ISP_VSD + offset,
			VI_ISP_VSD_delta(vsd_delta));
		regmap_write(vi->vi_reg, VI_ISP_VSD_H + offset,
			VI_ISP_VSD_H_out(vsd_out));
	}

}

static void rtk_vi_vs_coeff(struct rtk_vi *vi)
{
	u32 offset;
	u32 c0, c1, c2, c3, c4, c5, c6, c7;

	offset = get_macreg_offset(vi->ch_index);

	c0 = 0x3f753ff4;
	c1 = 0x3e3b3efa;
	c2 = 0x3ce93d81;
	c3 = 0x3d3f3cc8;
	c4 = 0x010e3e9f;
	c5 = 0x081e0444;
	c6 = 0x0fd90c26;
	c7 = 0x142e12b5;

	if (vi->ch_index == CH_1) {
		regmap_write(vi->vi_reg, VI_ISP_1_VSYC_0, c0);
		regmap_write(vi->vi_reg, VI_ISP_1_VSYC_1, c1);
		regmap_write(vi->vi_reg, VI_ISP_1_VSYC_2, c2);
		regmap_write(vi->vi_reg, VI_ISP_1_VSYC_3, c3);
		regmap_write(vi->vi_reg, VI_ISP_1_VSYC_4, c4);
		regmap_write(vi->vi_reg, VI_ISP_1_VSYC_5, c5);
		regmap_write(vi->vi_reg, VI_ISP_1_VSYC_6, c6);
		regmap_write(vi->vi_reg, VI_ISP_1_VSYC_7, c7);
	} else {
		regmap_write(vi->vi_reg, VI_ISP_VSYC_0 + offset, c0);
		regmap_write(vi->vi_reg, VI_ISP_VSYC_1 + offset, c1);
		regmap_write(vi->vi_reg, VI_ISP_VSYC_2 + offset, c2);
		regmap_write(vi->vi_reg, VI_ISP_VSYC_3 + offset, c3);
		regmap_write(vi->vi_reg, VI_ISP_VSYC_4 + offset, c4);
		regmap_write(vi->vi_reg, VI_ISP_VSYC_5 + offset, c5);
		regmap_write(vi->vi_reg, VI_ISP_VSYC_6 + offset, c6);
		regmap_write(vi->vi_reg, VI_ISP_VSYC_7 + offset, c7);
	}

	regmap_write(vi->vi_reg, VI_ISP_VSCC_0 + offset, c0);
	regmap_write(vi->vi_reg, VI_ISP_VSCC_1 + offset, c1);
	regmap_write(vi->vi_reg, VI_ISP_VSCC_2 + offset, c2);
	regmap_write(vi->vi_reg, VI_ISP_VSCC_3 + offset, c3);
	regmap_write(vi->vi_reg, VI_ISP_VSCC_4 + offset, c4);
	regmap_write(vi->vi_reg, VI_ISP_VSCC_5 + offset, c5);
	regmap_write(vi->vi_reg, VI_ISP_VSCC_6 + offset, c6);
	regmap_write(vi->vi_reg, VI_ISP_VSCC_7 + offset, c7);
}

static void rtk_vi_scale_down(struct rtk_vi *vi)
{
	u32 src_width, src_height;
	u32 dst_width, dst_height;
	u32 delta_num, delta_den, phase;

	src_width = vi->src_width;
	src_height = vi->src_height;
	dst_width = vi->dst_width;
	dst_height = vi->dst_height;

	/* set hs_scaler */
	phase = 0;
	delta_num = (src_width / dst_width) << 16;
	delta_den = ((src_width % dst_width)*0x10000) / dst_width;
	rtk_vi_hs_scaler(vi, phase,
		dst_width, (delta_num | delta_den));
	rtk_vi_hs_coeff(vi, (int)delta_num);

	/* set vs_scaler */
	phase = 0;
	delta_num = (src_height / dst_height) << 16;
	delta_den = ((src_height % dst_height)*0x10000) / dst_height;

	rtk_vi_vs_scaler(vi, phase,
		dst_height, (delta_num | delta_den));
	rtk_vi_vs_coeff(vi);
}

static void rtk_vi_decoder_cfg(struct rtk_vi *vi)
{
	u32 offset;
	u8 cfg_16bit, cfg_8bit;
	u8 code_system, input_fmt, fmt_seperate;
	u8 preamble_mode, ecc_en, mode;

	offset = get_macreg_offset(vi->ch_index);

	dev_info(vi->dev, "Config decoder src=%ux%u%s %s en_ecc=%s en_crc=%s\n",
		vi->src_width, vi->src_height, vi->is_interlace ? "I":"P",
		vi->bt_mode ? "BT.1120":"BT.656",
		vi->en_ecc ? "Y":"N", vi->en_crc ? "Y":"N");

	if (vi->separate_mode)
		dev_info(vi->dev, "separate_mode=%u\n", vi->separate_mode);

	cfg_8bit = 0;
	cfg_16bit = 0;
	code_system = vi->is_interlace;
	input_fmt = YC8BIT_CBY;
	preamble_mode = (u8)vi->bt_mode;

	switch (vi->separate_mode) {
	case SEP_CY_DESCEND:
		fmt_seperate = 1;
		input_fmt = YC16BIT_BA;
		cfg_16bit = CFG_DESCEND;
		break;
	case SEP_YC_DESCEND:
		fmt_seperate = 1;
		input_fmt = YC16BIT_AB;
		cfg_16bit = CFG_DESCEND;
		break;
	case SEP_CY_ASCEND:
		fmt_seperate = 1;
		input_fmt = YC16BIT_BA;
		cfg_16bit = CFG_ASCEND;
		break;
	case SEP_YC_ASCEND:
		fmt_seperate = 1;
		input_fmt = YC16BIT_AB;
		cfg_16bit = CFG_ASCEND;
		break;
	default:
		fmt_seperate = 0;
		break;
	}

	ecc_en = vi->en_ecc;
	mode = 1;

	regmap_write(vi->vi_reg, VI_DECODER + offset,
		VI_DECODER_pad_data_cfg_16bit(!cfg_16bit) |
		VI_DECODER_pad_data_cfg_8bit(!cfg_8bit) |
		VI_DECODER_sync_code_system(!code_system) |
		VI_DECODER_input_fmt(!input_fmt) |
		VI_DECODER_fmt_seperate(!fmt_seperate) |
		VI_DECODER_preamble_mode(!preamble_mode) |
		VI_DECODER_ecc_en(!ecc_en) |
		VI_DECODER_mode(!mode) |
		VI_DECODER_write_data(0));

	regmap_write(vi->vi_reg, VI_DECODER + offset,
		VI_DECODER_pad_data_cfg_16bit(cfg_16bit) |
		VI_DECODER_pad_data_cfg_8bit(cfg_8bit) |
		VI_DECODER_sync_code_system(code_system) |
		VI_DECODER_input_fmt(input_fmt) |
		VI_DECODER_fmt_seperate(fmt_seperate) |
		VI_DECODER_preamble_mode(preamble_mode) |
		VI_DECODER_ecc_en(ecc_en) |
		VI_DECODER_mode(mode) |
		VI_DECODER_write_data(1));

	if (vi->is_interlace) {
		/* P/I system share the same register of hsize */
		regmap_write(vi->vi_reg, VI_IMG + offset,
			VI_IMG_vsize(vi->src_height) |
			VI_IMG_hsize(vi->src_width));

		regmap_write(vi->vi_reg, VI_IMG_I + offset,
			VI_IMG_I_vsize_bottom(vi->src_height/2) |
			VI_IMG_I_vsize_top(vi->src_height/2));

	} else {
		regmap_write(vi->vi_reg, VI_IMG + offset,
			VI_IMG_vsize(vi->src_height) |
			VI_IMG_hsize(vi->src_width));
	}

	rtk_vi_crc_ctrl(vi, vi->ch_index, vi->en_crc);
}

static void rtk_vi_cascade_cfg(struct rtk_vi *vi)
{
	u8 din_sel;
	u8 data_rate;
	u8 ch2_id;
	u8 ch1_id;
	u8 enable;

	din_sel = 0;
	data_rate = 1;
	ch2_id = 1;
	ch1_id = 2;
	enable = 1;

	regmap_write(vi->vi_reg, VI_CASCADE,
		VI_CASCADE_din_sel(!din_sel) |
		VI_CASCADE_data_rate(!data_rate) |
		VI_CASCADE_ch2_id(!ch2_id) |
		VI_CASCADE_ch1_id(!ch1_id) |
		VI_CASCADE_en(!enable) |
		VI_CASCADE_write_data(0));

	regmap_write(vi->vi_reg, VI_CASCADE,
		VI_CASCADE_din_sel(din_sel) |
		VI_CASCADE_data_rate(data_rate) |
		VI_CASCADE_ch2_id(ch2_id) |
		VI_CASCADE_ch1_id(ch1_id) |
		VI_CASCADE_en(enable) |
		VI_CASCADE_write_data(1));

	rtk_vi_decoder_cfg(vi);
}

static void rtk_vi_isp_cfg(struct rtk_vi *vi)
{
	u32 offset;
	u8 scaling_down;
	u32 pitch;

	offset = get_macreg_offset(vi->ch_index);

	if ((vi->src_width > vi->dst_width) ||
		(vi->src_height > vi->dst_height))
		scaling_down = 1;
	else
		scaling_down = 0;

	dev_info(vi->dev, "Config ISP src %ux%u%s to dst %ux%u\n",
			vi->src_width, vi->src_height, vi->is_interlace ? "I":"P",
			vi->dst_width, vi->dst_height);

	pitch = roundup(vi->dst_width, 16);

	if (vi->is_interlace)
		pitch = pitch * 2;

	regmap_write(vi->vi_reg, VI_ISP_CFG + offset,
		VI_ISP_CFG_wb_f420(1) |
		VI_ISP_CFG_write_data(1));

	if (scaling_down)
		rtk_vi_scale_down(vi);

	regmap_write(vi->vi_reg, VI_ISP_SEL + offset,
		VI_ISP_SEL_hs_en(1) |
		VI_ISP_SEL_vs_en(1) |
		VI_ISP_SEL_write_data(scaling_down));

	regmap_write(vi->vi_reg, VI_ISP_SEQ_PITCH_Y + offset,
		VI_ISP_SEQ_PITCH_Y_p(pitch));
	regmap_write(vi->vi_reg, VI_ISP_SEQ_PITCH_C + offset,
		VI_ISP_SEQ_PITCH_C_p(pitch));

}

static void rtk_vi_packet_det_ctrl(struct rtk_vi *vi,
		u8 enable)
{
	u32 offset;

	offset = get_macreg_offset(vi->ch_index);

	if (enable && !vi->en_bt_pdi) {
		dev_info(vi->dev, "Skip packet detection\n");
		return;
	}

	dev_info(vi->dev, "%s packet det\n", enable ? "Enable":"Disable");

	regmap_write(vi->vi_reg, VI_BT_PDI + offset,
		VI_BT_PDI_en(1) |
		VI_BT_PDI_write_data(enable));

	if (enable)
		regmap_write(vi->vi_reg, VI_BT_PDI_FC + offset,
				VI_BT_PDI_FC_det_go(1) |
				VI_BT_PDI_FC_write_data(1));
}

static bool rtk_vi_get_det_result(struct rtk_vi *vi)
{
	u32 offset;
	u32 reg_val;
	u32 det_vsize;
	u32 det_hsize;
	bool det_done = false;

	offset = get_macreg_offset(vi->ch_index);

	if (vi->is_interlace) {
		regmap_read(vi->vi_reg, VI_BT_TF_IMG + offset, &reg_val);
		det_vsize = VI_BT_TF_IMG_get_vsize(reg_val);
		det_hsize = VI_BT_TF_IMG_get_hsize(reg_val);

		regmap_read(vi->vi_reg, VI_BT_BF_IMG + offset, &reg_val);
		if (det_vsize == VI_BT_BF_IMG_get_vsize(reg_val) &&
			det_hsize == VI_BT_BF_IMG_get_hsize(reg_val) &&
			vi->src_width == det_hsize &&
			vi->src_height == det_vsize*2)
			det_done = true;
	} else {
		regmap_read(vi->vi_reg, VI_BT_P_IMG + offset, &reg_val);
		det_vsize = VI_BT_P_IMG_get_vsize(reg_val);
		det_hsize = VI_BT_P_IMG_get_hsize(reg_val);

		if (vi->src_width == det_hsize &&
			vi->src_height == det_vsize)
			det_done = true;
	}

	if (det_done) {
		dev_info(vi->dev, "detect done, vsize=%u hsize=%u\n",
			det_vsize, det_hsize);
		/* Must disable mac_en before disable packet detection */
		vi->hw_ops->mac_ctrl(vi, vi->ch_index, DISABLE);
		vi->hw_ops->packet_det_ctrl(vi, DISABLE);
	} else {
		dev_info(vi->dev, "detect not match, det_vsize=%u det_hsize=%u\n",
			det_vsize, det_hsize);
		vi->hw_ops->packet_det_ctrl(vi, ENABLE);
	}

	return det_done;
}

static void rtk_vi_interrupt_ctrl(struct rtk_vi *vi,
		unsigned char ch_index, unsigned char enable)
{
	u32 val0 = 0;
	u32 val1 = 0;
	u8 ecc;
	u8 bt_pdi;

	if (ch_index > CH_MAX)
		return;

	dev_info(vi->dev, "%s ch%u interrupt\n",
		enable ? "Enable":"Disable", ch_index);

	if (enable) {
		ecc = vi->en_ecc;
		bt_pdi = vi->en_bt_pdi;
	} else {
		ecc = 1;
		bt_pdi = 1;
	}

	val1 = VI_SCPU_INTEN1_mac_0_entry1_ovr8(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovr7(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovr6(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovr5(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovr4(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovr3(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovr2(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovr1(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovr0(1) |
			VI_SCPU_INTEN1_mac_0_entry1_ovf(1) |
			VI_SCPU_INTEN1_mac_0_entry1_done(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr8(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr7(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr6(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr5(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr4(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr3(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr2(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr1(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovr0(1) |
			VI_SCPU_INTEN1_mac_0_entry0_ovf(1) |
			VI_SCPU_INTEN1_mac_0_entry0_done(1) |
			VI_SCPU_INTEN1_write_data(enable);

	switch (ch_index) {
	case CH_0:
		val0 = VI_SCPU_INTEN0_ecc_0(ecc) |
			VI_SCPU_INTEN0_vanc_fifo_0(1) |
			VI_SCPU_INTEN0_data_fifo_0(1) |
			VI_SCPU_INTEN0_bt_pdi(bt_pdi) |
			VI_SCPU_INTEN0_write_data(enable);

		regmap_write(vi->vi_reg, VI_SCPU_INTEN0, val0);
		regmap_write(vi->vi_reg, VI_SCPU_INTEN1, val1);
		regmap_write(vi->vi_reg, VI_SCPU_INTEN2, val1);

		if (!enable) {
			/* Clear interrupt flags*/
			regmap_write(vi->vi_reg, VI_INTST0, VI_INTST0_ch0_mask);
			regmap_write(vi->vi_reg, VI_INTST1, ~VI_INTST1_write_data_mask);
			regmap_write(vi->vi_reg, VI_INTST2, ~VI_INTST2_write_data_mask);
		}
		break;
	case CH_1:
		val0 = VI_SCPU_INTEN0_ecc_1(ecc) |
			VI_SCPU_INTEN0_vanc_fifo_1(1) |
			VI_SCPU_INTEN0_data_fifo_1(1) |
			VI_SCPU_INTEN0_bt_1_pdi(bt_pdi) |
			VI_SCPU_INTEN0_write_data(enable);

		regmap_write(vi->vi_reg, VI_SCPU_INTEN0, val0);
		regmap_write(vi->vi_reg, VI_SCPU_INTEN3, val1);
		regmap_write(vi->vi_reg, VI_SCPU_INTEN4, val1);

		if (!enable) {
			/* Clear interrupt flags*/
			regmap_write(vi->vi_reg, VI_INTST0, VI_INTST0_ch0_mask);
			regmap_write(vi->vi_reg, VI_INTST1, ~VI_INTST1_write_data_mask);
			regmap_write(vi->vi_reg, VI_INTST2, ~VI_INTST2_write_data_mask);
		}
		break;
	}

}

static void rtk_vi_dma_buf_cfg(struct rtk_vi *vi,
		u8 entry_index, u64 start_addr)
{
	u32 offset;
	u32 entry_offset;
	u32 pitch;
	u64 y_sa, y_ea, y_size;
	u64 c_sa, c_ea;
	u64 crc_sa, crc_ea;

	offset = get_macreg_offset(vi->ch_index);
	entry_offset = (VI_ADR_ENTRY1_TF_Y_SA - VI_ADR_ENTRY0_TF_Y_SA) * entry_index;
	offset += entry_offset;

	pitch = roundup(vi->dst_width, 16);
	y_sa = start_addr;
	y_size = pitch * vi->dst_height;
	y_ea = y_sa + y_size;

	c_sa = y_ea;
	c_ea = c_sa + y_size/2;

	crc_sa = c_ea;
	crc_ea = crc_sa + 16;

	dev_dbg(vi->dev, "Config entry%u dma start_addr=0x%08llx size=%llu\n",
		entry_index, start_addr, c_ea - y_sa);

	if (vi->is_interlace) {
		u64 crc_sa_bf, crc_ea_bf;

		/* Top field */
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_TF_Y_SA + offset,
			VI_ADR_ENTRY0_TF_Y_SA_a(y_sa/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_TF_Y_EA + offset,
			VI_ADR_ENTRY0_TF_Y_EA_a((y_ea - pitch)/16));

		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_TF_C_SA + offset,
			VI_ADR_ENTRY0_TF_C_SA_a(c_sa/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_TF_C_EA + offset,
			VI_ADR_ENTRY0_TF_C_EA_a((c_ea - pitch)/16));

		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_TF_VANC_SA + offset,
			VI_ADR_ENTRY0_TF_VANC_SA_a(crc_sa/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_TF_VANC_EA + offset,
			VI_ADR_ENTRY0_TF_VANC_EA_a(crc_ea/16));

		/* Bottom field */
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_BF_Y_SA + offset,
			VI_ADR_ENTRY0_BF_Y_SA_a((y_sa + pitch)/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_BF_Y_EA + offset,
			VI_ADR_ENTRY0_BF_Y_EA_a(y_ea/16));

		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_BF_C_SA + offset,
			VI_ADR_ENTRY0_BF_C_SA_a((c_sa + pitch)/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_BF_C_EA + offset,
			VI_ADR_ENTRY0_BF_C_EA_a(c_ea/16));

		crc_sa_bf = crc_ea;
		crc_ea_bf = crc_sa_bf + 16;
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_BF_VANC_SA + offset,
			VI_ADR_ENTRY0_BF_VANC_SA_a(crc_sa_bf/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_BF_VANC_EA + offset,
			VI_ADR_ENTRY0_BF_VANC_EA_a(crc_ea_bf/16));
	} else {
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_Y_SA + offset,
			VI_ADR_ENTRY0_TF_Y_SA_a(y_sa/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_Y_EA + offset,
			VI_ADR_ENTRY0_Y_EA_a(y_ea/16));

		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_C_SA + offset,
			VI_ADR_ENTRY0_C_SA_a(c_sa/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_C_EA + offset,
			VI_ADR_ENTRY0_C_EA_a(c_ea/16));

		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_VANC_SA + offset,
			VI_ADR_ENTRY0_VANC_SA_a(crc_sa/16));
		regmap_write(vi->vi_reg, VI_ADR_ENTRY0_VANC_EA + offset,
			VI_ADR_ENTRY0_VANC_EA_a(crc_ea/16));
	}

	/* Indicate dma buf is ready */
	regmap_write(vi->vi_reg, VI_ADR_ENTRY0_ST + offset,
			VI_ADR_ENTRY0_ST_write_en1(1) |
			VI_ADR_ENTRY0_ST_valid(1));

	trace_vi_dma_buf_cfg(entry_index, start_addr, c_ea - y_sa);
}

static int rtk_vi_state_isr(struct rtk_vi *vi, u8 ch_index,
			u32 *inst_a, u32 *inst_b)
{
	int ret = 0;
	bool det_done;
	u32 mask;
	u32 inst0 = 0;

	if (ch_index == CH_1)
		mask = VI_INTST0_ch1_mask;
	else
		mask = VI_INTST0_ch0_mask;

	regmap_read(vi->vi_reg, VI_INTST0, &inst0);

	inst0 &= mask;

	if (!inst0)
		goto read_inst_ab;

	if ((ch_index == CH_0 && VI_INTST0_get_bt_pdi(inst0)) ||
		(ch_index == CH_1 && VI_INTST0_get_bt_1_pdi(inst0))) {
		det_done = rtk_vi_get_det_result(vi);
		if (det_done) {
			vi->detect_done = true;
			wake_up_interruptible(&vi->detect_wait);
		}
	}

	/* MAC 0 error */
	if (inst0 & VI_INTST0_ch0_err_mask) {
		if (VI_INTST0_get_wb_adr(inst0))
			dev_err(vi->dev, "Error: mac0 buf entry invalid\n");

		if (VI_INTST0_get_ecc_0(inst0) &&
			(vi->cascade_mode == CASCADE_OFF)) {
			ret = -EIO;
			dev_err(vi->dev, "Error: mac0 ecc invalid\n");
		}

		if (VI_INTST0_get_vanc_fifo_0(inst0))
			dev_err(vi->dev, "Error: mac0 vanc fifo overflow\n");

		if (VI_INTST0_get_data_fifo_0(inst0))
			dev_dbg(vi->dev, "Warn: mac0 data fifo overflow\n");
	}
	/* MAC 1 error */
	if (inst0 & VI_INTST0_ch1_err_mask) {
		if (VI_INTST0_get_wb_adr_1(inst0))
			dev_err(vi->dev, "Error: mac1 buf entry invalid\n");

		if (VI_INTST0_get_ecc_1(inst0) &&
			(vi->cascade_mode == CASCADE_OFF)) {
			ret = -EIO;
			dev_err(vi->dev, "Error: mac1 ecc invalid\n");
		}

		if (VI_INTST0_get_vanc_fifo_1(inst0))
			dev_err(vi->dev, "Error: mac1 vanc fifo overflow\n");

		if (VI_INTST0_get_data_fifo_1(inst0))
			dev_dbg(vi->dev, "Warn: mac1 data fifo overflow\n");
	}

	regmap_write(vi->vi_reg, VI_INTST0, inst0);

read_inst_ab:
	if (ch_index == CH_1) {
		regmap_read(vi->vi_reg, VI_INTST3, inst_a);
		regmap_read(vi->vi_reg, VI_INTST4, inst_b);
	} else {
		regmap_read(vi->vi_reg, VI_INTST1, inst_a);
		regmap_read(vi->vi_reg, VI_INTST2, inst_b);
	}

	return ret;
}

static bool rtk_vi_is_frame_done(u8 entry_index, u32 inst_a, u32 inst_b)
{
	bool is_done = false;

	if (entry_index > ENTRY_3)
		return false;

	switch (entry_index) {
	case ENTRY_0:
		is_done = VI_INTST1_get_mac_0_entry0_done(inst_a) ? true : false;
		break;
	case ENTRY_1:
		is_done = VI_INTST1_get_mac_0_entry1_done(inst_a) ? true : false;
		break;
	case ENTRY_2:
		is_done = VI_INTST2_get_mac_0_entry2_done(inst_b) ? true : false;
		break;
	case ENTRY_3:
		is_done = VI_INTST2_get_mac_0_entry3_done(inst_b) ? true : false;
		break;
	}

	if (is_done)
		trace_vi_frame_done(entry_index, inst_a, inst_b);

	return is_done;
}

static void rtk_vi_clear_done_flag(struct rtk_vi *vi,
		u8 ch_index, u8 entry_index)
{
	u32 reg_addr;
	u32 val;

	if (entry_index > ENTRY_3)
		return;

	if (ch_index == CH_1) {
		switch (entry_index) {
		case ENTRY_0:
			reg_addr = VI_INTST3;
			val = VI_INTST3_mac_1_entry0_done_mask;
			break;
		case ENTRY_1:
			reg_addr = VI_INTST3;
			val = VI_INTST3_mac_1_entry1_done_mask;
			break;
		case ENTRY_2:
			reg_addr = VI_INTST4;
			val = VI_INTST4_mac_1_entry2_done_mask;
			break;
		case ENTRY_3:
			reg_addr = VI_INTST4;
			val = VI_INTST4_mac_1_entry3_done_mask;
			break;
		}
	} else {
		switch (entry_index) {
		case ENTRY_0:
			reg_addr = VI_INTST1;
			val = VI_INTST1_mac_0_entry0_done_mask;
			break;
		case ENTRY_1:
			reg_addr = VI_INTST1;
			val = VI_INTST1_mac_0_entry1_done_mask;
			break;
		case ENTRY_2:
			reg_addr = VI_INTST2;
			val = VI_INTST2_mac_0_entry2_done_mask;
			break;
		case ENTRY_3:
			reg_addr = VI_INTST2;
			val = VI_INTST2_mac_0_entry3_done_mask;
			break;
		}
	}

	regmap_write(vi->vi_reg, reg_addr, val);
}

static void rtk_vi_clear_mac_ovf_flags(struct rtk_vi *vi,
		u8 ch_index, u32 inst_a, u32 inst_b)
{
	switch (ch_index) {
	case CH_0:
		if (inst_a)
			regmap_write(vi->vi_reg, VI_INTST1, VI_INTST_mac_ovf_mask);

		if (inst_b)
			regmap_write(vi->vi_reg, VI_INTST2, VI_INTST_mac_ovf_mask);
		break;
	case CH_1:
		if (inst_a)
			regmap_write(vi->vi_reg, VI_INTST3, VI_INTST_mac_ovf_mask);

		if (inst_b)
			regmap_write(vi->vi_reg, VI_INTST4, VI_INTST_mac_ovf_mask);
		break;
	}
}

static void rtk_vi_clear_mac_ovr_flags(struct rtk_vi *vi,
		u8 ch_index, u32 inst_a, u32 inst_b)
{
	switch (ch_index) {
	case CH_0:
		if (inst_a)
			regmap_write(vi->vi_reg, VI_INTST1, VI_INTST_mac_ovr_mask);

		if (inst_b)
			regmap_write(vi->vi_reg, VI_INTST2, VI_INTST_mac_ovr_mask);
		break;
	case CH_1:
		if (inst_a)
			regmap_write(vi->vi_reg, VI_INTST3, VI_INTST_mac_ovr_mask);

		if (inst_b)
			regmap_write(vi->vi_reg, VI_INTST4, VI_INTST_mac_ovr_mask);
		break;
	}
}

static void rtk_vi_dump_registers(struct rtk_vi *vi,
		u32 start_offset, u32 end_offset)
{
	u32 offset;
	u32 reg_val = 0;

	dev_err(vi->dev, "Dump vi registers from offset 0x%x to 0x%x\n",
		start_offset, end_offset);

	for (offset = start_offset; offset <= end_offset; offset += 4) {
		usleep_range(100, 200);
		regmap_read(vi->vi_reg, offset, &reg_val);
		dev_info(vi->dev, "Read reg0x%08x = 0x%08x\n",
			0x9812b000 + offset, reg_val);
	}
}

static const struct rtk_vi_ops vi_hw_ops = {
	.mac_rst = rtk_vi_mac_reset,
	.mac_ctrl = rtk_vi_mac_ctrl,
	.scale_down = rtk_vi_scale_down,
	.decoder_cfg = rtk_vi_decoder_cfg,
	.isp_cfg = rtk_vi_isp_cfg,
	.packet_det_ctrl = rtk_vi_packet_det_ctrl,
	.get_det_result = rtk_vi_get_det_result,
	.interrupt_ctrl = rtk_vi_interrupt_ctrl,
	.dma_buf_cfg = rtk_vi_dma_buf_cfg,
	.state_isr = rtk_vi_state_isr,
	.is_frame_done = rtk_vi_is_frame_done,
	.clear_done_flag = rtk_vi_clear_done_flag,
	.clear_mac_ovf_flags = rtk_vi_clear_mac_ovf_flags,
	.clear_mac_ovr_flags = rtk_vi_clear_mac_ovr_flags,
	.dump_registers = rtk_vi_dump_registers,
};

/**
 * vi_cascade_ops - hardware operation for pr2k cascade mode
 *
 * @mac_ctrl: Use cascade_go instead of mac_go
 * @decoder_cfg: Normal decoder_cfg + cascade_cfg
 */
static const struct rtk_vi_ops vi_cascade_ops = {
	.mac_rst = rtk_vi_mac_reset,
	.mac_ctrl = rtk_vi_cascade_mac_ctrl,
	.scale_down = rtk_vi_scale_down,
	.decoder_cfg = rtk_vi_cascade_cfg,
	.isp_cfg = rtk_vi_isp_cfg,
	.packet_det_ctrl = rtk_vi_packet_det_ctrl,
	.get_det_result = rtk_vi_get_det_result,
	.interrupt_ctrl = rtk_vi_interrupt_ctrl,
	.dma_buf_cfg = rtk_vi_dma_buf_cfg,
	.state_isr = rtk_vi_state_isr,
	.is_frame_done = rtk_vi_is_frame_done,
	.clear_done_flag = rtk_vi_clear_done_flag,
	.clear_mac_ovf_flags = rtk_vi_clear_mac_ovf_flags,
	.clear_mac_ovr_flags = rtk_vi_clear_mac_ovr_flags,
	.dump_registers = rtk_vi_dump_registers,
};

int rtk_vi_hw_deinit(struct rtk_vi *vi)
{
	if (!vi->hw_init_done)
		return 0;

	if (!g_need_init_crt) {
		vi->hw_ops->mac_ctrl(vi, CH_0, DISABLE);
		vi->hw_ops->mac_ctrl(vi, CH_1, DISABLE);
		vi->hw_ops->interrupt_ctrl(vi, CH_0, DISABLE);
		vi->hw_ops->interrupt_ctrl(vi, CH_1, DISABLE);
		clk_disable_unprepare(vi->clk_vi);
		reset_control_assert(vi->reset_vi);

		dev_info(vi->dev, "Disable clock and assert reset\n");
		g_need_init_crt = true;
	}

	vi->hw_init_done = false;

	return 0;
}

int rtk_vi_hw_init(struct rtk_vi *vi)
{
	if (vi->hw_init_done)
		return 0;

	if (vi->cascade_mode == CASCADE_OFF) {
		vi->hw_ops = &vi_hw_ops;
		vi->en_bt_pdi = true;
	} else {
		vi->hw_ops = &vi_cascade_ops;

		if (vi->cascade_mode == CASCADE_MASTER)
			vi->en_bt_pdi = true;
		else
			vi->en_bt_pdi = false;
	}

	vi->en_ecc = true;
	vi->en_crc = true;

	dev_info(vi->dev, "en_ecc=%s en_bt_pdi=%s en_crc=%s\n",
			vi->en_ecc ? "Y":"N",
			vi->en_bt_pdi ? "Y":"N",
			vi->en_crc ? "Y":"N");

	vi->dst_width = vi->src_width;
	vi->dst_height = vi->src_height;

	if (g_need_init_crt) {
		reset_control_deassert(vi->reset_vi);
		clk_prepare_enable(vi->clk_vi);

		clk_disable(vi->clk_vi);
		reset_control_assert(vi->reset_vi);

		dev_info(vi->dev, "Deassert reset and enable clock\n");
		reset_control_deassert(vi->reset_vi);
		clk_enable(vi->clk_vi);

		g_need_init_crt = false;
	}

	vi->hw_init_done = true;

	return 0;
}
