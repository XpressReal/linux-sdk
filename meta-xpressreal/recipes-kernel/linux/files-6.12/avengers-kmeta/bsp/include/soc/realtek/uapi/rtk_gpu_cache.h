#ifndef __UAPI_GPU_CACHE_H
#define __UAPI_GPU_CACHE_H

#include <linux/types.h>

#define GPU_CACHE_IOCTL_VERSION_MAJOR  1
#define GPU_CACHE_IOCTL_VERSION_MINOR  2

/*
 * GPU_CACHE_IOCTL_GET_VERSION - get ioctl version
 */
#define GPU_CACHE_IOCTL_GET_VERSION       _IOR('g', 0, __u32)

/*
 * GPU_CACHE_IOCTL_IMPORT_DMABUF - import dmabuf
 */
#define GPU_CACHE_IOCTL_IMPORT_DMABUF     _IOWR('g', 1, struct gpu_cache_ioctl_import_dmabuf_data)

/*
 * GPU_CACHE_IOCTL_RELEASE_DMABUF - release dmabuf
 */
#define GPU_CACHE_IOCTL_RELEASE_DMABUF    _IOW('g', 2, __u64)

/*
 * GPU_CACHE_IOCTL_SET_FRAME - set addr in frame and enable
 *
 * The frame index should be requested by GPU_CACHE_IOCTL_REQUEST_FRAME.
 * The VAs should be imported via GPU_CACHE_IOCTL_IMPORT_DMABUF.
 */
#define GPU_CACHE_IOCTL_SET_FRAME         _IOW('g', 3, struct gpu_cache_ioctl_set_frame)

/*
 * GPU_CACHE_IOCTL_CLEAR_FRAME - disable, and clear addr in frame
 */
#define GPU_CACHE_IOCTL_CLEAR_FRAME       _IOW('g', 4, struct gpu_cache_ioctl_clear_frame)

/*
 * GPU_CACHE_IOCTL_SET_FRAME_INFO - set frame info
 *
 * See struct gpu_cache_ioctl_set_frame_info.
 */
#define GPU_CACHE_IOCTL_SET_FRAME_INFO    _IOW('g', 5, struct gpu_cache_ioctl_set_frame_info)

/*
 * GPU_CACHE_IOCTL_SET_PARAM - set a param
 */
#define GPU_CACHE_IOCTL_SET_PARAM         _IOW('g', 6, struct gpu_cache_ioctl_param_data)

/*
 * GPU_CACHE_IOCTL_GET_PARAM - get a param
 */
#define GPU_CACHE_IOCTL_GET_PARAM         _IOWR('g', 7, struct gpu_cache_ioctl_param_data)

/*
 * GPU_CACHE_IOCTL_FLUSH - flush gpu cache
 */
#define GPU_CACHE_IOCTL_FLUSH             _IO('g', 8)

/*
 * GPU_CACHE_IOCTL_REQUEST_FRAME - request a frame
 */
#define GPU_CACHE_IOCTL_REQUEST_FRAME_SIMPLE  _IOR('g', 9, __u32)
#define GPU_CACHE_IOCTL_REQUEST_FRAME         _IOWR('g', 9, struct gpu_cache_ioctl_request_frame)

/*
 * GPU_CACHE_IOCTL_RELEASE_FRAME - release a frame
 */
#define GPU_CACHE_IOCTL_RELEASE_FRAME     _IOW('g', 10, __u32)

/**
 * struct gpu_cache_ioctl_import_dmabuf_data - import a dma_buf and get its va
 * @fd: [in]  dmabuf fd to be imported
 * @va: [out] va of the dmabuf
 */
struct gpu_cache_ioctl_import_dmabuf_data {
	__s32 fd;
	__u64 va;
};

/**
 * struct gpu_cache_ioctl_set_frame - set a y/c frame
 * @index:            [in] frame index
 * @type:             [in] frame type, 0 for y frame and 1 for c frame
 * @adr_va:           [in] va of adr_begin and adr_end
 * @adr_begin_offset: [in] offset of adr_begin
 * @adr_end_offset:   [in] offset of adr_end
 * @header_va:        [in] va of header
 * @header_offset:    [in] offset of header
 * @payload_va:       [in] va of payload
 * @payload_offset:   [in] offset of payload
 */
struct gpu_cache_ioctl_set_frame {
	__u32 index;
	__u32 type;
	__u64 adr_va;
	__u32 adr_begin_offset;
	__u32 adr_end_offset;
	__u64 header_va;
	__u32 header_offset;
	__u64 payload_va;
	__u32 payload_offset;
};

/**
 * struct gpu_cache_ioctl_clear_frame - clear a y/c frame
 * @index:   [in] frame index
 * @type:    [in] frame type 0 for y frame and 1 for c frame
 */
struct gpu_cache_ioctl_clear_frame {
	__u32 index;
	__u32 type;
};

/**
 * struct gpu_cache_ioctl_set_frame_info - set frame info
 * @decomp_payload_pitch: [in] decomp_payload_pitch
 * @decomp_header_pitch:  [in] decomp_header_pitch
 * @gpu_ip_pitch:         [in] gpu_ip_pitch
 * @index:                [in] index (only available if shared_frame_info=0)
 * @pic_height:           [in] pic_height (only available if shared_frame_info=0)
 */
struct gpu_cache_ioctl_set_frame_info {
	__u32 decomp_payload_pitch;
	__u32 decomp_header_pitch;
	__u32 gpu_ip_pitch;
	__u32 index;
	__u32 pic_height;
};

/**
 * struct gpu_cache_ioctl_request_frame
 * @frame_index: [out] requested frame index
 * @hint_prev_addr: [in] enable hint for hw workaround
 * @[y_adr|c_adr]*: [in] hint data
 */
struct gpu_cache_ioctl_request_frame {
	__u32 frame_index;
	__u32 hint_prev_adr;
	__u64 y_adr_va;
	__u32 y_adr_begin_offset;
	__u32 y_adr_end_offset;
	__u64 c_adr_va;
	__u32 c_adr_begin_offset;
	__u32 c_adr_end_offset;
};

/**
 * param_id
 */
#define GPU_CACHE_PARAM_ID_QLEVEL_QUEUE_SEL_Y  0
#define GPU_CACHE_PARAM_ID_QLEVEL_QUEUE_SEL_C  1
#define GPU_CACHE_PARAM_ID_LOSSY_EN            2
#define GPU_CACHE_PARAM_ID_DECOMP_BPP          3
#define GPU_CACHE_PARAM_ID_MAX_FRAMES          4
#define GPU_CACHE_PARAM_ID_SHARED_FRAME_INFO   5
#define GPU_CACHE_PARAM_ID_GPU_DECOMP_CBCR_SEL 6
#define GPU_CACHE_PARAM_ID_DDR_HEADER_SWAP     7
#define GPU_CACHE_PARAM_ID_DDR_PAYLOAD_SWAP    8
#define GPU_CACHE_PARAM_ID_NEW_PACKING         9

/**
 * gpu_cache_ioctl_param_data - get/set a param
 * @param_id: [in] should be one of param_id
 * @value:    [in] value to be set (for GPU_CACHE_IOCTL_SET_PARAM)
 *            [out] the returned value (for GPU_CACHE_IOCTL_GET_PARAM)
 */
struct gpu_cache_ioctl_param_data {
	__u32 param_id;
	__u32 value;
};

#endif
