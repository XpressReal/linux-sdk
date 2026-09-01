/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_RTK_XDI_H_
#define _UAPI_RTK_XDI_H_

#include <linux/types.h>

#define XDI_IOCTL_VERSION_MAJOR  1
#define XDI_IOCTL_VERSION_MINOR  0

#define XDI_IOCTL_MAGIC 'x'
#define XDI_IOCTL_GET_VERSION    _IOR(XDI_IOCTL_MAGIC, 0, __u32)
#define XDI_IOCTL_IMPORT_DMABUF  _IOWR(XDI_IOCTL_MAGIC, 1, struct xdi_dmabuf)
#define XDI_IOCTL_RELEASE_DMABUF _IOW(XDI_IOCTL_MAGIC, 2, struct xdi_dmabuf)
#define XDI_IOCTL_START_CMD      _IOW(XDI_IOCTL_MAGIC, 3, struct xdi_cmd)
#define XDI_IOCTL_SET_STILL_REGION _IOW(XDI_IOCTL_MAGIC, 4, struct xdi_still_region)
#define XDI_IOCTL_CLEAR_STILL_REGION _IO(XDI_IOCTL_MAGIC, 5)

struct xdi_still_region {
	__u32 handles[3];
};

struct xdi_dmabuf {
	__s32 fd;
	__u32 handle;
	__u32 flags;
};

struct xdi_flags {
	__u32 __reserved_core_rvd0:1;
	__u32 topfield:1;
	__u32 f422:1;
	__u32 ppc10b:1;
	__u32 nv21:1;
	__u32 st:1;
	__u32 __reserved_core_rvd1:26;

	/* core_di */
	__u32 weave_teeth_en:1;
	__u32 noise_level_en:1;
	__u32 cu:1;
	__u32 light_comb_en:1;
	__u32 weight_filter_en:1;
	__u32 __reserved_core_di_rvd0:1;
	__u32 mf_select:1;
	__u32 comb_chk_en:1;
	__u32 chk_use_max_mf:1;
	__u32 wb_chk_result:1;
	__u32 wb_max_diff:1;
	__u32 use_wb_diff:1;
	__u32 source:2;
	__u32 __reserved_core_di_rvd1:2;
	__u32 bob_chk0_en:1;
	__u32 bob_chk1_en:1;
	__u32 bob_chk2_en:1;
	__u32 chk4_en:1;
	__u32 smooth_en:1;
	__u32 still_en:1;
	__u32 still_reset:1;
	__u32 vote_en:1;
	__u32 dbg_blend_ratio:1;
	__u32 dbg_combing:1;
	__u32 __reserved_core_di_rvd2:1;
	__u32 dbg_bob:1;
	__u32 __reserved_core_di_rvd3:1;
	__u32 hcs_420_sel_pn:1;
	__u32 mode:2;

	/* wb */
	__u32 wb_f420:1;
	__u32 wb_tpc_num:8;
	__u32 wb_ppc10b:1;
	__u32 wb_p010:1;
	__u32 wb_tr:1;
	__u32 __reserved_wb_rvd0:20;
};

struct xdi_param {
	__u32 id;
	__u32 value;
};

#define XDI_MAX_PARAMS 24

struct xdi_cmd {
	__u32 y_handles[4];
	__u32 y_offsets[4];
	__u16 y_pitches[4];
	__u32 c_handles[4];
	__u32 c_offsets[4];
	__u16 c_pitches[4];
	__u16 w;
	__u16 h;
	struct xdi_flags flags;
	__u32 num_params;
	struct xdi_param params[XDI_MAX_PARAMS];
};

static inline void xdi_cmd_default(struct xdi_cmd *c)
{
	c->flags.mode = 2;
	c->flags.bob_chk0_en = 1;
	c->flags.bob_chk1_en = 1;
	c->flags.bob_chk2_en = 1;
	c->flags.use_wb_diff = 1;
	c->flags.wb_max_diff = 1;
	c->flags.chk_use_max_mf = 1;
	c->flags.comb_chk_en = 1;
	c->flags.mf_select = 1;
}


#define XDI_PARAM_ID_CORE_DI_DYNAMIC_0         1
#define XDI_PARAM_ID_CORE_DI_DYNAMIC_1         2
#define XDI_PARAM_ID_CORE_DI_SMOOTH            3
#define XDI_PARAM_ID_CORE_DI_STILL             4
#define XDI_PARAM_ID_CORE_DI_VOTE              5
#define XDI_PARAM_ID_CORE_DI_TEETH             6
#define XDI_PARAM_ID_CORE_DI_WGTFILT_THD       7
#define XDI_PARAM_ID_CORE_DI_WGTFILT_WGT       8
#define XDI_PARAM_ID_CORE_DI_WGTFILT_WIN0      9
#define XDI_PARAM_ID_CORE_DI_WGTFILT_WIN1      10
#define XDI_PARAM_ID_CORE_DI_WGTFILT_WIN2      11
#define XDI_PARAM_ID_CORE_DI_NOISE             12
#define XDI_PARAM_ID_CORE_DI_NOISE_WIN_H       13
#define XDI_PARAM_ID_CORE_DI_NOISE_WIN_W       14
#define XDI_PARAM_ID_CORE_DI_NOISE_LEVEL_SUM   15
#define XDI_PARAM_ID_CORE_DI_NOISE_LEVEL       16
#define XDI_PARAM_ID_CORE_DI_C_TYPE            17
#define XDI_PARAM_ID_CORE_DI_C_WIN0_H          18
#define XDI_PARAM_ID_CORE_DI_C_WIN0_W          19
#define XDI_PARAM_ID_CORE_DI_C_WIN1_H          20
#define XDI_PARAM_ID_CORE_DI_C_WIN1_W          21
#define XDI_PARAM_ID_CORE_DI_C_WIN2_H          22
#define XDI_PARAM_ID_CORE_DI_C_WIN2_W          23

#endif /* _UAPI_RTK_XDI_H_ */
