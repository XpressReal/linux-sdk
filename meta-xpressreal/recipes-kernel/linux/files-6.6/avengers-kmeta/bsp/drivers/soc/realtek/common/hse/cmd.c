// SPDX-License-Identifier: GPL-2.0-only
#include "hse.h"
#include "linux/printk.h"
#include "uapi/hse.h"

static inline u32 addr_msb(u64 addr)
{
	return (addr >> 32);
}

static int __append_copy(struct hse_command_queue *cq, u64 dst, u64 src, u32 size, u64 flags)
{
	u32 cmds[5] = {};
	u32 swap_opt = 0;
	u32 num_cmds = 4;
	bool ext = false;

	flags &= HSE_FLAGS_COPY_VALID_MASK;
	if (flags & HSE_FLAGS_COPY_SWAP_EN)
		swap_opt |= BIT(8) | ((flags & HSE_FLAGS_COPY_SWAP_OPT_MASK) << 9);

	cmds[0] = 0x1 | swap_opt;
	cmds[1] = size;
	cmds[2] = dst;
	cmds[3] = src;

	if (addr_msb(dst)) {
		ext = true;
		cmds[4] |= addr_msb(dst);
	}
	if (addr_msb(src)) {
		ext = true;
		cmds[4] |= addr_msb(src) << 4;
	}
	if (ext) {
		cmds[0] |= BIT(31);
		num_cmds = 5;
	}

	return hse_cq_add_data(cq, cmds, num_cmds);
}

static int __append_picture_copy(struct hse_command_queue *cq, u64 dst, u16 dst_pitch,
				 u64 src, u16 src_pitch, u16 width, u16 height, u64 flags)
{
	u32 cmds[6] = {};
	u32 swap_opt = 0;
	u32 num_cmds = 5;
	bool ext = false;

	flags &= HSE_FLAGS_COPY_VALID_MASK;
	if (flags & HSE_FLAGS_COPY_SWAP_EN)
		swap_opt |= BIT(8) | ((flags & HSE_FLAGS_COPY_SWAP_OPT_MASK) << 9);

	cmds[0] = 0x1 | swap_opt | BIT(14);
	cmds[1] = height | (width << 16);
	cmds[2] = dst;
	cmds[3] = src;
	cmds[4] = dst_pitch | (src_pitch << 16);

	if (addr_msb(dst)) {
		ext = true;
		cmds[5] |= addr_msb(dst);
	}
	if (addr_msb(src)) {
		ext = true;
		cmds[5] |= addr_msb(src) << 4;
	}
	if (ext) {
		cmds[0] |= BIT(31);
		num_cmds = 6;
	}
	return hse_cq_add_data(cq, cmds, num_cmds);
}

static int __append_constant_fill(struct hse_command_queue *cq, u64 dst, u32 val, u32 size)
{
	u32 cmds[5] = {};
	u32 num_cmds = 4;

	cmds[0] = 0x1 | BIT(19);
	cmds[1] = size;
	cmds[2] = dst;
	cmds[3] = val;
	if (addr_msb(dst)) {
		num_cmds = 5;
		cmds[0] |= BIT(31);
		cmds[4] |= addr_msb(dst);
	}
	return hse_cq_add_data(cq, cmds, num_cmds);
}

static int __append_xor(struct hse_command_queue *cq, u64 dst, u64 *src, u32 src_cnt, u32 size)
{
	u32 cmds[9] = {};
	u32 num_cmds = 3;
	int i;
	bool ext = false;
	u32 ext_cmd = 0;

	cmds[0] = 0x1 | ((src_cnt - 1) << 16);
	cmds[1] = size;
	cmds[2] = dst;

	if (addr_msb(dst)) {
		ext = true;
		ext_cmd |= addr_msb(dst);
	}

	for (i = 0; i < src_cnt; i++) {
		cmds[i + 3] = src[i];
		num_cmds += 1;

		if (addr_msb(src[i])) {
			ext = true;
			ext_cmd |= addr_msb(src[i]) << ((i + 1) * 4);
		}
	}

	if (ext) {
		cmds[0] |= BIT(31);
		cmds[num_cmds++] = ext_cmd;
	}

	return hse_cq_add_data(cq, cmds, num_cmds);
}

static int __append_yuy2_to_nv16(struct hse_command_queue *cq, u64 luma, u64 chroma, u16 dst_pitch,
				 u64 src, u32 src_pitch, u16 width, u16 height, u64 flags)
{
	u32 cmds[6] = {};

	cmds[0] = 0x2;
	cmds[1] = height | (width << 16);
	cmds[2] = dst_pitch | (src_pitch << 16);
	cmds[3] = luma;
	cmds[4] = chroma;
	cmds[5] = src;

	if (addr_msb(luma))
		cmds[0] |= addr_msb(luma) << 16;
	if (addr_msb(chroma))
		cmds[0] |= addr_msb(chroma) << 24;
	if (addr_msb(src))
		cmds[0] |= addr_msb(src) << 20;

	return hse_cq_add_data(cq, cmds, 6);
}

static int __append_stretch(struct hse_command_queue *cq,
			   u32 dst_width, u32 dst_height, u32 dst_pitch,
               u32 src_width, u32 src_height, u32 src_pitch,
			   u32 dst, u32 src, u32 v_ph, u32 h_ph, u16 colorSel)
{
	u32 cmds[8];

	u8 HEnable = (src_width == dst_width ) ? false : true;
	u8 VEnable = (src_height == dst_height ) ? false : true;
	u8 byVerticalTapNumber = SEINFO_VSCALING_4TAP;
	u8 byHorizontalTapNumber = SEINFO_HSCALING_8TAP;
	u8 chromaCtlBit = colorSel;

	cmds[0] = 0x4 | (VEnable << 22)
                  | (HEnable << 23)
                  | (byVerticalTapNumber << 26)
                  | (byHorizontalTapNumber << 27)
                  | (chromaCtlBit << 31);
	cmds[1] = dst_height | (dst_width << 16);
	cmds[2] = src_height | (src_width << 16);
	cmds[3] = dst_pitch  | (src_pitch << 16);
	cmds[4] = dst;
	cmds[5] = src;
	cmds[6] = v_ph;
	cmds[7] = h_ph;
	return hse_cq_add_data(cq, cmds, 8);
}

static const u32 swap_opt_rorn[][4] = {
	{  0, 18, 16,  9 },
	{  1, 12, 22, 11 },
	{  2, 19, 10, 15 },
	{  3,  6, 20, 17 },
	{  4, 13,  8, 21 },
	{  5,  7, 14, 23 },
	{  6, 20, 17,  3 },
	{  7, 14, 23,  5 },
	{  8, 21,  4, 13 },
	{  9,  0, 18, 16 },
	{ 10, 15,  2, 19 },
	{ 11,  1, 12, 22 },
	{ 12, 22, 11,  1 },
	{ 13,  8, 21,  4 },
	{ 14, 23,  5,  7 },
	{ 15,  2, 19, 10 },
	{ 16,  9,  0, 18 },
	{ 17,  3,  6, 20 },
	{ 18, 16,  9,  0 },
	{ 19, 10, 15,  2 },
	{ 20, 17,  3,  6 },
	{ 21,  4, 13,  8 },
	{ 22, 11,  1, 12 },
	{ 23,  5,  7, 14 },
};

#define COPY_SIZE_MAX    (518144)

static int hse_cq_prep_copy_f(struct hse_device *hse_dev, struct hse_command_queue *cq,
			      dma_addr_t dst, dma_addr_t src, u32 size, u64 flags)
{
	u32 c_size = size;
	u32 c_dst_addr = dst;
	u32 c_src_addr = src;
	int ret;
	int should_split = (c_src_addr % 16) || (c_dst_addr % 16) || (c_size % 16);

	if (flags)
		return -EINVAL;

	while (should_split && c_size > COPY_SIZE_MAX) {
		ret = __append_copy(cq, c_dst_addr, c_src_addr, COPY_SIZE_MAX, 0);
		if (ret)
			return ret;
		c_size -= COPY_SIZE_MAX;
		c_dst_addr += COPY_SIZE_MAX;
		c_src_addr += COPY_SIZE_MAX;
	}

	should_split = should_split && (c_size > 16);

	if (should_split) {
		u32 new_size;

		if (c_size <= 0x20)
			new_size = 0x10;
		else if (c_size <= 0x800)
			new_size = 0x20;
		else
			new_size = 0x800;

		ret = __append_copy(cq, c_dst_addr, c_src_addr, c_size & ~(new_size - 1), 0);
		if (ret)
			return ret;
		ret = __append_copy(cq, c_dst_addr + c_size - new_size,
				  c_src_addr + c_size - new_size, new_size, 0);
		if (ret)
			return ret;
		c_size = 0;
	}

	if (c_size != 0)
		ret = __append_copy(cq, c_dst_addr, c_src_addr, c_size, 0);
	return ret;
}

int hse_cq_prep_copy(struct hse_device *hse_dev, struct hse_command_queue *cq,
		     dma_addr_t dst, dma_addr_t src, u32 size, u64 flags)
{
	if (hse_should_workaround_copy(hse_dev))
		return hse_cq_prep_copy_f(hse_dev, cq, dst, src, size, flags);

	return __append_copy(cq, dst, src, size, flags);
}

static bool picture_copy_args_valid(u32 dst, u16 dst_pitch, u32 src, u16 src_pitch, u16 width, u16 height)
{
	return IS_ALIGNED(dst, 4) && IS_ALIGNED(dst_pitch, 4) &&
		IS_ALIGNED(src, 4) && IS_ALIGNED(src_pitch, 4) &&
		IS_ALIGNED(width, 4) && dst_pitch >= width && src_pitch >= width;
}

int hse_cq_prep_picture_copy(struct hse_device *hse_dev, struct hse_command_queue *cq,
			     dma_addr_t dst, u16 dst_pitch, dma_addr_t src, u16 src_pitch,
			     u16 width, u16 height, u64 flags)
{
	if (!picture_copy_args_valid(dst, dst_pitch, src, src_pitch, width, height))
		return -EINVAL;

	return __append_picture_copy(cq, dst, dst_pitch, src, src_pitch, width, height, flags);
}

int hse_cq_prep_constant_fill(struct hse_device *hse_dev, struct hse_command_queue *cq,
			      dma_addr_t dst, u32 val, u32 size, u64 flags)
{
	if (hse_should_workaround_copy(hse_dev))
		return -EINVAL;

	return __append_constant_fill(cq, dst, val, size);
}

int hse_cq_prep_xor(struct hse_device *hse_dev, struct hse_command_queue *cq,
		    dma_addr_t dst, dma_addr_t *src, u32 src_cnt, u32 size, u64 flags)
{
	u64 s[5];
	int i;

	if (hse_should_workaround_copy(hse_dev) || src_cnt <= 1 || src_cnt > 5)
		return -EINVAL;

	for (i = 0; i < src_cnt; i++)
		s[i] = src[i];

	return __append_xor(cq, dst, s, src_cnt, size);
}

#define YUY2_TO_NV16_SRC_BYTES_PER_PIXEL 2

static bool yuy2_to_nv16_args_valid(u32 luma, u32 chroma, u16 dst_pitch, u32 src, u16 src_pitch, u16 width,
				u16 height)
{
	return IS_ALIGNED(luma, 2) && IS_ALIGNED(chroma, 2) &&  IS_ALIGNED(dst_pitch, 2) &&
		(dst_pitch >= width) &&	IS_ALIGNED(src, 4) && IS_ALIGNED(src_pitch, 4) &&
		(src_pitch >= width * YUY2_TO_NV16_SRC_BYTES_PER_PIXEL) && IS_ALIGNED(width, 2);
}

int hse_cq_prep_yuy2_to_nv16(struct hse_device *hse_dev, struct hse_command_queue *cq,
			     dma_addr_t luma, dma_addr_t chroma, u16 dst_pitch,
			     dma_addr_t src, u16 src_pitch, u16 width, u16 height, u64 flags)
{

	if (!yuy2_to_nv16_args_valid(luma, chroma, dst_pitch, src,
					    src_pitch, width, height))
		return -EINVAL;
	return __append_yuy2_to_nv16(cq, luma, chroma, dst_pitch, src, src_pitch,
				    width, height, flags);
}

static int __append_rotate(struct hse_command_queue *cq,
			   u64 dst, u32 dst_pitch, u64 src, u32 src_pitch,
			   u32 width, u32 height, u32 mode, u32 color_format, u32 format_10bit)
{
	u32 cmds[5] = {};

	cmds[0] = 0x5 | (format_10bit << 8) | (mode << 29) | (color_format << 31);
	cmds[1] = height | (width << 16);
	cmds[2] = dst_pitch | (src_pitch << 16);
	cmds[3] = dst;
	cmds[4] = src;
	if (addr_msb(dst))
		cmds[0] |= addr_msb(dst) << 12;
	if (addr_msb(src))
		cmds[0] |= addr_msb(src) << 16;
	return hse_cq_add_data(cq, cmds, 5);
}

int hse_cq_prep_rotate(struct hse_device *hse_dev, struct hse_command_queue *cq,
		       dma_addr_t dst, u32 dst_pitch, dma_addr_t src, u32 src_pitch,
		       u32 width, u32 height, u32 mode, u32 color_format, u32 format_10bit)
{
	if (!hse_support_rotate_10bit(hse_dev) && format_10bit == 1)
		return -EINVAL;

	return __append_rotate(cq, dst, dst_pitch, src, src_pitch, width, height, mode,
			       color_format, format_10bit);
}

int hse_cq_prep_yuv2rgb_coeff(struct hse_device *hse_dev,
			      struct hse_command_queue *cq)
{
	u32 cmds[7] = { 0 };

	cmds[0] = 0x6 | (1 << 8);
	cmds[1] = 0x0400 | (0x0400 << 16);
	cmds[2] = 0x0400 | (0x0000 << 16);
	cmds[3] = 0x1F40 | (0x076C << 16);
	cmds[4] = 0x064D | (0x1E21 << 16);
	cmds[5] = 0x0000 | (0x0336 << 16);
	cmds[6] = 0x0054 | (0x0312 << 16);

	return hse_cq_add_data(cq, cmds, 7);
}

int hse_cq_prep_rgb2yuv_coeff(struct hse_device *hse_dev,
			      struct hse_command_queue *cq)
{
	u32 cmds[7] = { 0 };

	cmds[0] = 0x6 | (1 << 8);
	cmds[1] = 0x00da | (0x1f8b << 16);
	cmds[2] = 0x0200 | (0x02dc << 16);
	cmds[3] = 0x1e75 | (0x1e2f << 16);
	cmds[4] = 0x004a | (0x0200 << 16);
	cmds[5] = 0x1fd1;
	cmds[6] = 128 | (128 << 16);

	return hse_cq_add_data(cq, cmds, 7);
}

static int fmt_convert_args_valid(u16 dst_pitch, u16 src_pitch,
			    u32 dst_luma_addr, u32 dst_chroma_addr,
			    u32 src_luma_addr, u32 src_chroma_addr)
{
	int ret = 0;

	if (!IS_ALIGNED(dst_pitch, 16) || !IS_ALIGNED(src_pitch, 16)) {
		pr_err("src/dst pitch should be 16 alignmnet!");
		ret = -EINVAL;
		goto exit;
	}

	if (!IS_ALIGNED(dst_luma_addr, 16) ||
		!IS_ALIGNED(dst_chroma_addr, 16) ||
		!IS_ALIGNED(src_luma_addr, 16) ||
		!IS_ALIGNED(src_chroma_addr, 16)) {
		pr_err("address should be 16 alignmnet!");
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

int hse_cq_prep_fmt_convert(struct hse_device *hse_dev,
			    struct hse_command_queue *cq, u8 dst_fmt,
			    u16 dst_pitch, u16 dst_rgb_order, u32 dst_luma_addr,
			    u32 dst_chroma_addr, u16 alpha_out, u8 yuv_down_h,
			    u8 yuv_down_v, u8 src_fmt, u16 src_pitch,
			    u16 src_rgb_order, u32 src_luma_addr,
			    u32 src_chroma_addr, u16 width, u16 height)
{
	u32 cmds[8] = { 0 };

	if (fmt_convert_args_valid(dst_pitch, src_pitch, dst_luma_addr,
		dst_chroma_addr, src_luma_addr, src_chroma_addr))
		return -EINVAL;

	cmds[0] = 0x7 | (src_fmt << 8) | (src_rgb_order << 11) |
		  (dst_fmt << 16) | (dst_rgb_order << 19) | (yuv_down_h << 24) |
		  (yuv_down_v << 25);
	cmds[1] = height | (width << 16);
	cmds[2] = dst_pitch | (alpha_out << 16);
	cmds[3] = dst_luma_addr;
	cmds[4] = dst_chroma_addr;
	cmds[5] = src_pitch;
	cmds[6] = src_luma_addr;
	cmds[7] = src_chroma_addr;

	return hse_cq_add_data(cq, cmds, 8);
}

int hse_cq_prep_stretch(struct hse_device *hse_dev, struct hse_command_queue *cq,
		       dma_addr_t dst, u32 dst_pitch, dma_addr_t src, u32 src_pitch,
		       u16 dst_width, u16 dst_height, u16 src_width, u16 src_height, u16 colorSel)
{
    u8 byVerticalTapNumber = SEINFO_VSCALING_4TAP;
    u8 byHorizontalTapNumber = SEINFO_HSCALING_8TAP;
    u32 dwHorizontalScalingRatio = (src_width << 14) / dst_width;
    u32 dwVerticalScalingRatio = (src_height << 14) / dst_height;

    u32 hDeltaPhase = 0;
    u32 hMSB = 0;
    u32 hLSB = 0;
    u32 hInitialPhase = 0;

    u32 vDeltaPhase = 0;
    u32 vMSB = 0;
    u32 vLSB = 0;
    u32 vInitialPhase = 0;

    u32 v_ph = 0, h_ph = 0;

    /* calculate Vertical delta phase */
    {
        u64 deltaPhase = src_height * (HSE_DELTA_PHASE_BASE) / dst_height;
        vDeltaPhase = (int32_t) deltaPhase;
        u32 deltaPhaseMeta = vDeltaPhase;
        if(deltaPhaseMeta != deltaPhase) {
            vDeltaPhase = vDeltaPhase + 1;
        }

        vMSB = vDeltaPhase >> 14;
        vLSB = vDeltaPhase & 0x3FFF;
        v_ph = (vLSB & 0x3FFF) | ((vMSB & 0xF) << 14) | (vInitialPhase << 18);
    }

    /* calculate Horizontal delta phase */
    {
        u64 deltaPhase = src_width * (HSE_DELTA_PHASE_BASE) / dst_width;
        hDeltaPhase = (int32_t) deltaPhase;
        u32 deltaPhaseMeta = hDeltaPhase;
        if(deltaPhaseMeta != deltaPhase) {
            hDeltaPhase = hDeltaPhase + 1;
        }

        hMSB = hDeltaPhase >> 14;
        hLSB = hDeltaPhase & 0x3FFF;
        h_ph = (hLSB & 0x3FFF) | ((hMSB & 0xF) << 14) | (hInitialPhase << 18);
    }

    /* calculate coeffs */
    u16 tmp_coef[96] = {};
    u16 *tmp_coef_4t16p = &(tmp_coef[0]);
    u16 *tmp_coef_8t16p = &(tmp_coef[32]);

    /* then generate coef, vertical remains the same */
    SetVideoScalingCoeffs((u16 *)tmp_coef_4t16p,
            dwVerticalScalingRatio,
            0,
            1 << (byVerticalTapNumber+1),
            0) ;

    if(colorSel != COLOR_SEL_SP_P_CB && colorSel != COLOR_SEL_SP_P_CR) {
        SetVideoScalingCoeffs((u16 *)tmp_coef_8t16p,
                dwHorizontalScalingRatio,
                0,
                1 << (byHorizontalTapNumber+1),
                0) ;
    } else {
        int32_t cbMode = 0;
        if(colorSel == COLOR_SEL_SP_P_CB) {
            cbMode = 1;
        }

        SetCoeffsCbCrMode(
                (u16 *)tmp_coef_8t16p,
                cbMode,
                1 << (byHorizontalTapNumber+1),
                0);
    }

    u16 *coef_pt = &(tmp_coef[0]);
    u32 coef_pt_idx = 0;
    u32 coeffs[32] = {0};

    /* convert coeff to cmds and write to command queue */
    for(u8 i = 0; i < 8; i++) {
        /* word 0 */
        coeffs[0] = 0x3 | (i<<19);
        /* word 1 - 6 */
        for(u8 j = 1; j < 7; j++) {
            coeffs[j] = (coef_pt[coef_pt_idx] & 0x3FFF)
                    | ((coef_pt[coef_pt_idx+1] & 0x3FFF) << 16);
            coef_pt_idx = coef_pt_idx + 2;
        }

        /* write command */
        pr_warn("[%s] coeffs %.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x",
                __FUNCTION__,
                coeffs[0],
                coeffs[1],
                coeffs[2],
                coeffs[3],
                coeffs[4],
                coeffs[5],
                coeffs[6],
                coeffs[7]);

	    hse_cq_add_data(cq, coeffs, 8);
    }

    return __append_stretch(cq, dst_width, dst_height, dst_pitch,
                                src_width, src_height, src_pitch,
                                dst, src, v_ph, h_ph, colorSel);
}
