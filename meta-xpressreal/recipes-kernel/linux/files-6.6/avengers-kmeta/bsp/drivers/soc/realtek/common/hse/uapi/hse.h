#ifndef __UAPI_HSE_H
#define __UAPI_HSE_H

#include <linux/ioctl.h>
#include <linux/types.h>

/**
 * device name under '/dev'
 */
#define HSE_MISC_NAME        "hse"


/**
 * Deprecated ioctl nr are listed below:
 *   0x04 - hw reset
 *   0x05 - force hw reset
 *   0x10 - copy cmd with dma_buf
 *   0x11 - xor cmd with dma_buf
 *   0x12 - constant fill cmd with dma_buf
 */

/**
 * ioctl version
 */
#define HSE_VERSION_MAJOR    3
#define HSE_VERSION_MINOR    4

/**
 * HSE_IOCTL_VERSION - get ioctl version.
 *
 * Returns a unsigned int with version. The major verion number is in [31:16],
 * and minor verion number is in [15:0].
 */
#define HSE_IOCTL_VERSION                _IOR('H', 0x00, __u32)

/**
 * HSE_IOCTL_CMD_PREP_RAW - prepare raw commands.
 *
 * Put commands in a hse_cmd struct into the internal command queue.
 */
#define HSE_IOCTL_CMD_PREP_RAW           _IOW('H', 0x01, struct hse_cmd)

/**
 * HSE_IOCTL_CMD_START - run commands in the internal command queue
 *
 * Run commands in the internal command queue, the commands in the internal command queue
 * would be cleared after this ioctl command.
 */
#define HSE_IOCTL_CMD_START              _IO('H', 0x02)
#define HSE_IOCTL_CMD_START_FLAGS        _IOW('H', 0x02, __u64)


#define HSE_IOCTL_SET_ENGINE             _IOW('H', 0x03, __u32)

/**
 * HSE_IOCTL_GET_FEATURES - get hse features
 *
 * Returns hse features listed in HSE_FEATURE_*
 */
#define HSE_IOCTL_GET_FEATURES           _IOR('H', 0x05, __u64)

/**
 * HSE_IOCTL_IMPORT_DMABUF - import a dma buf and return HSE VA
 */
#define HSE_IOCTL_IMPORT_DMABUF          _IOWR('H', 0x13, struct hse_import_dmabuf)

/**
 * HSE_IOCTL_RELEASE_MEM - release a imported HSE VA
 */
#define HSE_IOCTL_RELEASE_MEM            _IOW('H', 0x14, struct hse_release_mem)

/**
 * HSE_IOCTL_CMD_COPY - run a copy command.
 */
#define HSE_IOCTL_CMD_COPY               _IOW('H', 0x15, struct hse_cmd_copy)

/**
 * HSE_IOCTL_CMD_XOR - run a xor command.
 */
#define HSE_IOCTL_CMD_XOR                _IOW('H', 0x16, struct hse_cmd_xor)

/**
 * HSE_IOCTL_CMD_CONSTANT_FILL - run a constant fill command
 */
#define HSE_IOCTL_CMD_CONSTANT_FILL      _IOW('H', 0x17, struct hse_cmd_constant_fill)

/**
 * HSE_IOCTL_CMD_PICTURE_COPY - run a picture copy command.
 */
#define HSE_IOCTL_CMD_PICTURE_COPY       _IOW('H', 0x18, struct hse_cmd_picture_copy)

/**
 * HSE_IOCTL_CMD_YUY2I_TO_YUY2SP - run a YUY2 to NV16 command.
 */
#define HSE_IOCTL_CMD_YUY2_TO_NV16     _IOW('H', 0x19, struct hse_cmd_yuy2_to_nv16)

/**
 * HSE_IOCTL_CMD_ROTATE - run a rotate command
 */
#define HSE_IOCTL_CMD_ROTATE           _IOW('H', 0x1a, struct hse_cmd_rotate)

/**
 * HSE_IOCTL_CMD_FMT_CONVERT - run a yuv/rgb conversion command
 */
#define HSE_IOCTL_CMD_FMT_CONVERT _IOW('H', 0x1b, struct hse_cmd_fmt_convert)

/**
 * HSE_IOCTL_CMD_STRETCH - run a stretch command
 */
#define HSE_IOCTL_CMD_STRETCH           _IOW('H', 0x1c, struct hse_cmd_stretch)

/**
 * struct hse_cmd - raw commands to be added
 * @size:       [in] legnth of raw commands
 * @cmds:       [in] command data
 */
struct hse_cmd {
	__u32 size;
	__u32 cmds[32];
};

/**
 * flags
 */
#define HSE_FLAGS_COPY_SWAP_OPT_MASK   0x0000001fLL
#define HSE_FLAGS_COPY_SWAP_EN         0x00000020LL
#define HSE_FLAGS_ROTATE_10BIT         0x00000001LL
#define HSE_FLAGS_PREP_CMD             0x100000000LL

#define HSE_FLAGS_COPY_VALID_MASK     \
	(HSE_FLAGS_COPY_SWAP_OPT_MASK | HSE_FLAGS_COPY_SWAP_EN | HSE_FLAGS_PREP_CMD)
#define HSE_FLAGS_ROTATE_VALID_MASK   (HSE_FLAGS_ROTATE_10BIT)

/**
 * features
 */
#define HSE_FEATURE_ROTATE_10BIT_SUPPORT   0x1LL

/**
 * Copy command swap options, valid with HSE_FLAGS_COPY_SWAP_EN set
 */
#define HSE_FLAGS_COPY_SWAP_OPT_ARGB    0
#define HSE_FLAGS_COPY_SWAP_OPT_ARBG    1
#define HSE_FLAGS_COPY_SWAP_OPT_AGRB    2
#define HSE_FLAGS_COPY_SWAP_OPT_AGBR    3
#define HSE_FLAGS_COPY_SWAP_OPT_ABRG    4
#define HSE_FLAGS_COPY_SWAP_OPT_ABGR    5
#define HSE_FLAGS_COPY_SWAP_OPT_RAGB    6
#define HSE_FLAGS_COPY_SWAP_OPT_RABG    7
#define HSE_FLAGS_COPY_SWAP_OPT_RGAB    8
#define HSE_FLAGS_COPY_SWAP_OPT_RGBA    9
#define HSE_FLAGS_COPY_SWAP_OPT_RBAG    10
#define HSE_FLAGS_COPY_SWAP_OPT_RBGA    11
#define HSE_FLAGS_COPY_SWAP_OPT_GARB    12
#define HSE_FLAGS_COPY_SWAP_OPT_GABR    13
#define HSE_FLAGS_COPY_SWAP_OPT_GRAB    14
#define HSE_FLAGS_COPY_SWAP_OPT_GRBA    15
#define HSE_FLAGS_COPY_SWAP_OPT_GBAR    16
#define HSE_FLAGS_COPY_SWAP_OPT_GBRA    17
#define HSE_FLAGS_COPY_SWAP_OPT_BARG    18
#define HSE_FLAGS_COPY_SWAP_OPT_BAGR    19
#define HSE_FLAGS_COPY_SWAP_OPT_BRAG    20
#define HSE_FLAGS_COPY_SWAP_OPT_BRGA    21
#define HSE_FLAGS_COPY_SWAP_OPT_BGAR    22
#define HSE_FLAGS_COPY_SWAP_OPT_BGRA    23

/**
 * struct hse_cmd_copy - data for a copy command
 * @size:       [in] size of copy
 * @dst_va:     [in] HSE VA of a imported destination buffer
 * @dst_offset: [in] destination buffer offset
 * @src_va:     [in] HSE VA of a imported source buffer
 * @src_offset: [in] source buffer offset
 * @flags:      [in] additional control flags, must in HSE_FLAGS_COPY_VALID_MASK
 */
struct hse_cmd_copy {
	__u32 size;
	__u32 dst_va;
	__u32 dst_offset;
	__u32 src_va;
	__u32 src_offset;
	__u64 flags;
};

/**
 * struct hse_cmd_picture_copy - data for a picture copy command
 * @width:      [in] width of a picture
 * @height:     [in] height of a picture
 * @dst_va:     [in] HSE VA of a imported destination buffer
 * @dst_offset: [in] destination buffer offset
 * @dst_pitch:  [in] pitch to dst, must greater than or equal to width
 * @src_va:     [in] HSE VA of a imported source buffer
 * @src_offset: [in] source buffer offset
 * @src_pitch:  [in] pitch to src, must greater than or equal to width
 * @flags:      [in] additional control flags, must in HSE_FLAGS_COPY_VALID_MASK
 *
 * dst_offset, dst_pitch, src_offset, src_pitch and width should be aligned to 4.
 */
struct hse_cmd_picture_copy {
	__u16 width;
	__u16 height;
	__u32 dst_va;
	__u32 dst_offset;
	__u16 dst_pitch;
	__u32 src_va;
	__u32 src_offset;
	__u16 src_pitch;
	__u64 flags;
};

#define HSE_XOR_NUM   5

/**
 * struct hse_cmd_xor - data for a xor command
 * @size:       [in] size of xor
 * @dst_va:     [in] HSE VA of a imported destination buffer
 * @dst_offset: [in] destination buffer offset
 * @src_va:     [in] HSE VAs of of imported source buffers
 * @src_offset: [in] source buffer offsets
 * @src_num:    [in] number of source buffers
 * @flags:      [in] additional control flags
 */
struct hse_cmd_xor {
	__u32 size;
	__u32 dst_va;
	__u32 dst_offset;
	__u32 src_va[HSE_XOR_NUM];
	__u32 src_offset[HSE_XOR_NUM];
	__u32 src_num;
	__u64 flags;
};

/**
 * struct hse_cmd_constant_fill - data for a constant_fill command
 * @size:       [in] size of constant fill
 * @dst_va:     [in] HSE VA of a imported destination buffer
 * @dst_offset: [in] destination buffer offset
 * @val:        [in] value to be filled, using unsigned int of this value.
 * @flags:      [in] additional control flags
 */
struct hse_cmd_constant_fill {
	__u32 size;
	__u32 dst_va;
	__u32 dst_offset;
	__u32 val;
	__u64 flags;
};

/**
 * struct hse_cmd_yuy2_to_nv16 - data for a yuy2_to_nv16 command
 * @width:      [in] width of a picture (unit: pixels, 2 bytes per pixel)
 * @height:     [in] height of a picture
 * @dst_pitch:  [in] pitch to luma and chroma, must greater than or equal to width (unit: bytes)
 * @luma_va:    [in] HSE VA of a imported destination buffer for luma
 * @luma_offset:[in] destination buffer offset for  luma
 * @chroma_va:  [in] HSE VA of a imported destination buffer for chroma
 * @chroma_offset:
 *              [in] destination buffer offset for chroma
 * @src_pitch:  [in] pitch to src, must greater than or equal to (width * 2) (unit: bytes)
 * @src_va:     [in] HSE VA of a imported source buffer
 * @src_offset: [in] source buffer offset
 * @flags:      [in] additional control flags
 */
struct hse_cmd_yuy2_to_nv16 {
	__u16 width;
	__u16 height;
	__u16 dst_pitch;
	__u32 luma_va;
	__u32 luma_offset;
	__u32 chroma_va;
	__u32 chroma_offset;
	__u16 src_pitch;
	__u32 src_va;
	__u32 src_offset;
	__u64 flags;
};

enum HSE_COLOR_FMT {
	/* YUV */
	HSE_FMT_YUV420SP,
	HSE_FMT_YUV422SP,
	/* RGB with 32-bit/pixel */
	HSE_FMT_ARGB = 2,
	HSE_FMT_BGRA = 3,
	/* RGB with 24-bit/pixel */
	HSE_FMT_RGB888 = 26,
};

/**
 * struct hse_cmd_fmt_convert - data for a yuv/rgb conversion command
 * @width:      [in] width of a picture (unit: pixels)
 * @height:     [in] height of a picture
 * @dst_fmt:     [in] enum HSE_COLOR_FMT; destinate color format
 * @dst_luma_va:    [in] HSE VA of a imported destination buffer for luma
 * @dst_luma_offset:[in] destination buffer offset for  luma
 * @dst_chroma_va:    [in] HSE VA of a imported destination buffer for chroma (0 for RGB type)
 * @dst_chroma_offset:[in] destination buffer offset for chroma (0 for RGB type)
 * @dst_pitch:  [in] pitch to luma and chroma, must greater than or equal to width (unit: bytes, 16-byte alignment)
 * @alpha_out : [in] Alpha value when YUV to ARGB
 * @src_fmt:     [in] enum HSE_COLOR_FMT; source color format
 * @src_luma_va:    [in] HSE VA of a imported src buffer for luma
 * @src_luma_offset:[in] src buffer offset for luma
 * @src_chroma_va:    [in] HSE VA of a imported src buffer for chroma (0 for RGB type)
 * @src_chroma_offset:[in] src buffer offset for chroma (0 for RGB type)
 * @src_pitch:  [in] pitch to src, must greater than or equal to width (unit: bytes, 16-byte alignment)
 */
struct hse_cmd_fmt_convert {
	__u16 width;
	__u16 height;
	__u32 dst_fmt;
	__u32 dst_luma_va;
	__u32 dst_luma_offset;
	__u32 dst_chroma_va;
	__u32 dst_chroma_offset;
	__u16 dst_pitch;
	__u16 alpha_out;
	__u32 src_fmt;
	__u32 src_luma_va;
	__u32 src_luma_offset;
	__u32 src_chroma_va;
	__u32 src_chroma_offset;
	__u16 src_pitch;
};

#define HSE_ROTATE_MODE_90_DEG    0
#define HSE_ROTATE_MODE_180_DEG   1
#define HSE_ROTATE_MODE_270_DEG   2

#define HSE_ROTATE_COLOR_FORMAT_Y      0
#define HSE_ROTATE_COLOR_FORMAT_CbCr   1

#define HSE_DELTA_PHASE_BASE   (16384)

#define COLOR_SEL_Y         (0)
#define COLOR_SEL_CBCR      (1)
/* use Stretch to do SP -> P convert */
#define COLOR_SEL_SP_P_CB   (2)
#define COLOR_SEL_SP_P_CR   (3)

/**
 * struct hse_cmd_rotate - data for a rotate command
 * @width:      [in] width of a picture (unit in pixel)
 * @height:     [in] height of a picture
 * @dst_pitch:  [in] pitch to dst (unit: bytes)
 * @dst_va:     [in] HSE VA of a imported destination buffer for dst
 * @dst_offset: [in] destination buffer offset for dst
 * @src_pitch:  [in] pitch to src (unit: bytes)
 * @src_va:     [in] HSE VA of a imported source buffer
 * @src_offset: [in] source buffer offset
 * @mode:       [in] degree to rotate (should be one of HSE_ROTATE_MODE_*)
 * @color_format: [in] color format (should be one of HSE_ROTATE_COLOR_FORMAT_*)
 * @flags:      [in] additional control flags, must in HSE_FLAGS_ROTATE_VALID_MASK
 */
struct hse_cmd_rotate {
	__u16 width;
	__u16 height;
	__u32 dst_pitch;
	__u32 dst_va;
	__u32 dst_offset;
	__u32 src_pitch;
	__u32 src_va;
	__u32 src_offset;
	__u32 mode;
	__u32 color_format;
	__u64 flags;
};

/**
 * struct hse_import_dmabuf
 * @fd:         [in] a dma_buf fd to import
 * @hse_va:     [out] HSE VA
 */
struct hse_import_dmabuf {
	__s32 fd;
	__u32 hse_va;
};

/**
 * struct hse_release_mem
 * @hse_va:     [in] HSE VA
 */
struct hse_release_mem {
	__u32 hse_va;
};

/**
 * struct hse_cmd_stretch - data for a stretch command
 * @dst_width:    [in] destination width of a picture (unit in pixel)
 * @dst_height:   [in] destination height of a picture
 * @dst_pitch:    [in] pitch to dst (unit: bytes)
 * @dst_va:       [in] hse va of a imported destination buffer for dst
 * @dst_offset:   [in] destination buffer offset for dst
 * @stc_width:    [in] source width of a picture (unit in pixel)
 * @src_height:   [in] source height of a picture
 * @src_pitch:    [in] pitch to src (unit: bytes)
 * @src_va:       [in] hse va of a imported source buffer
 * @src_offset:   [in] source buffer offset
 * @colorSel:     [in] Luma or Chroma data select
 */
struct hse_cmd_stretch {
	__u16 dst_width;
	__u16 dst_height;
	__u32 dst_pitch;
	__u32 dst_va;
	__u32 dst_offset;
	__u16 src_width;
	__u16 src_height;
	__u32 src_pitch;
	__u32 src_va;
	__u32 src_offset;
    __u16 colorSel;
};

#endif /* __UAPI_HSE_H */
