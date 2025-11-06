/* SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause) */
/*
 * Copyright (c) 2017-2024 Realtek Semiconductor Corp.
 */

#ifndef __SOC_RTK_IPC_SHM_H
#define __SOC_RTK_IPC_SHM_H

extern void __iomem *rpc_common_base;
extern void __iomem *rpc_ringbuf_base;

#define IPC_SHM_VIRT (rpc_common_base + 0x000000C4)
#define BOOT_AV_INFO (rpc_common_base + 0x00000600)


struct rtk_ipc_shm {
	volatile uint32_t sys_assign_serial;
	volatile uint32_t pov_boot_vd_std_ptr;
	volatile uint32_t pov_boot_av_info;
	volatile uint32_t audio_rpc_flag;
	volatile uint32_t suspend_mask;
	volatile uint32_t suspend_flag;
	volatile uint32_t vo_vsync_flag;
	volatile uint32_t audio_fw_entry_pt;
	volatile uint32_t power_saving_ptr;
	volatile unsigned char printk_buffer[24];
	volatile uint32_t ir_extended_tbl_pt;
	volatile uint32_t vo_int_sync;
	volatile uint32_t bt_wakeup_flag;
	volatile uint32_t ir_scancode_mask;
	volatile uint32_t ir_wakeup_scancode;
	volatile uint32_t suspend_wakeup_flag;
	volatile uint32_t acpu_resume_state;
	volatile uint32_t gpio_wakeup_enable;
	volatile uint32_t gpio_wakeup_activity;
	volatile uint32_t gpio_output_change_enable;
	volatile uint32_t gpio_output_change_activity;
	volatile uint32_t audio_reciprocal_timer_sec;
	volatile uint32_t u_boot_version_magic;
	volatile uint32_t u_boot_version_info;
	volatile uint32_t suspend_watchdog;
	volatile uint32_t xen_domu_boot_st;
	volatile uint32_t gpio_wakeup_enable2;
	volatile uint32_t gpio_wakeup_activity2;
	volatile uint32_t gpio_output_change_enable2;
	volatile uint32_t gpio_output_change_activity2;
	volatile uint32_t gpio_wakeup_enable3;
	volatile uint32_t gpio_wakeup_activity3;
	volatile uint32_t video_rpc_flag;
	volatile uint32_t video_int_sync;
	volatile unsigned char video_printk_buffer[24];
	volatile uint32_t video_suspend_mask;
	volatile uint32_t video_suspend_flag;
	volatile uint32_t ve3_rpc_flag;
	volatile uint32_t ve3_int_sync;
	volatile uint32_t reserved1[30];
	volatile uint32_t hifi_rpc_flag;
	volatile uint32_t reserved2[31];
	volatile uint32_t hifi_acpu_hifiWrite[32];
	volatile uint32_t hifi_acpu_hifiRead[32];
	volatile unsigned char hifi_printk_buffer[128];
	volatile uint32_t hifi1_rpc_flag;
	volatile unsigned char hifi1_printk_buffer[24];
	volatile uint32_t reserved3[25];
	volatile uint32_t kr4_rpc_flag;
	volatile unsigned char kr4_printk_buffer[24];
	volatile uint32_t reserved4[1];
};

struct avcpu_syslog_struct{
	volatile uint32_t log_buf_addr;
	volatile uint32_t log_buf_len;
	volatile uint32_t logged_chars;
	volatile uint32_t log_start;
	volatile uint32_t con_start;
	volatile uint32_t log_end;
};

#define BOOT_AV_INFO_MAGICNO_DLSR 0x24525451

struct boot_av_info{
	unsigned int dwMagicNumber;           //identificatin String "$RTK" or "STD3"
	unsigned int dwVideoStreamAddress;    //The Location of Video ES Stream
	unsigned int dwVideoStreamLength;     //Video ES Stream Length
	unsigned int dwAudioStreamAddress;    //The Location of Audio ES Stream
	unsigned int dwAudioStreamLength;     //Audio ES Stream Length
	unsigned char bVideoDone;
	unsigned char bAudioDone;
	unsigned char bHDMImode;              //monitor device mode (DVI/HDMI)
	char dwAudioStreamVolume;             //Audio ES Stream Volume -60 ~ 40
	char dwAudioStreamRepeat;             //0 : no repeat ; 1 :repeat
	unsigned char charbPowerOnImage;      //Alternative of power on image or video
	unsigned char charbRotate;            //enable v &h flip
	unsigned int dwVideoStreamType;       //Video Stream Type
	unsigned int audio_buffer_addr;       //Audio decode buffer
	unsigned int audio_buffer_size;
	unsigned int video_buffer_addr;       //Video decode buffer
	unsigned int video_buffer_size;
	unsigned int last_image_addr;         //Buffer used to keep last image
	unsigned int last_image_size;
	unsigned char logo_enable;
	unsigned int logo_addr;
	unsigned int src_width;              //logo_src_width
	unsigned int src_height;             //logo_src_height
	unsigned int dst_width;              //logo_dst_width
	unsigned int dst_height;             //logo_dst_height
	unsigned int logo_addr_2;
	unsigned int vo_secure_addr;         //Address for secure vo
	unsigned int is_OSD_mixer1;
	unsigned int bGraphicBufferDone;
	unsigned int is_from_fastboot_resume;
	unsigned int zoomMagic;              //0x2452544F
	unsigned int zoomFunction;           //bit0: zoomratio
	unsigned int zoomXYWHRatio_x;
	unsigned int zoomXYWHRatio_y;
	unsigned int zoomXYWHRatio_w;
	unsigned int zoomXYWHRatio_h;
	unsigned int zoomXYWHRatio_base;
	unsigned int video_DLSR_binary_addr;//0x24525451
	unsigned int video_DLSR_binary_addr_svp;//0x24525451
	unsigned int one_step_cvbs_setting;//0x24525452
	unsigned int is_boot_logo_splash ; //0x24525453
	unsigned int splash_boot_logo_num ;
	unsigned int splash_boot_logo_fps ;
	unsigned int hdmi_swap_config;//0x24525454
	unsigned int user_setting_param_start_addr;//0x24525455
	unsigned int output_interface_format_addr;//0x24525456
	unsigned int vo_permanent_buf_addr; //0x24525457
	unsigned int set_step_flag; //0x24525458
	unsigned int splash_pic_status; //0x24525459, AUT-280, splash_pic_status: define in BOOT_PIC_SPLASH_STATUS
	unsigned int cur_splash_pic_addr;
	unsigned int splash_pic_start_addr;
	unsigned int src_splash_pic_width;
	unsigned int src_splash_pic_height;
	unsigned int rotate_reserved;//0x2452545A
} ;


#if defined(CONFIG_RTK_XEN_SUPPORT) && defined(CONFIG_RTK_RPC)
#define SETMASK(bits, pos)		(((-1U) >> (32-bits))  << (pos))
#define CLRMASK(bits, pos)		(~(SETMASK(bits, pos)))
#define SET_VAL(val,bits,pos)		((val << pos) & SETMASK(bits, pos))
#define GET_VAL(reg,bits,pos)		((reg & SETMASK(bits, pos)) >> pos)

#define XEN_DOMU_BOOT_ST_MAGIC_KEY			(0xEA)
#define XEN_DOMU_BOOT_ST_MAGIC_KEY_MASK			(XEN_DOMU_BOOT_ST_MAGIC_KEY << 24)

#define XEN_DOMU_BOOT_ST_VERSION_SET(reg)		SET_VAL(reg, 4,20)
#define XEN_DOMU_BOOT_ST_VERSION_GET(reg)		GET_VAL(reg, 4,20)
#define XEN_DOMU_BOOT_ST_VERSION			(1)

#define XEN_DOMU_BOOT_ST_AUTHOR_SET(reg)		SET_VAL(reg, 4,16)
#define XEN_DOMU_BOOT_ST_AUTHOR_GET(reg)		GET_VAL(reg, 4,16)
#define XEN_DOMU_BOOT_ST_AUTHOR_ACPU			(1)
#define XEN_DOMU_BOOT_ST_AUTHOR_SCPU			(2)

#define XEN_DOMU_BOOT_ST_STATE_SET(reg)			SET_VAL(reg, 8, 8)
#define XEN_DOMU_BOOT_ST_STATE_GET(reg)			GET_VAL(reg, 8, 8)
#define XEN_DOMU_BOOT_ST_STATE_SCPU_BOOT		(1)
#define XEN_DOMU_BOOT_ST_STATE_SCPU_RESTART		(2)
#define XEN_DOMU_BOOT_ST_STATE_SCPU_POWOFF		(3)
#define XEN_DOMU_BOOT_ST_STATE_ACPU_LOCK		(4)
#define XEN_DOMU_BOOT_ST_STATE_ACPU_UNLOCK		(5)
#define XEN_DOMU_BOOT_ST_STATE_SCPU_WAIT_DONE		(6)
#define XEN_DOMU_BOOT_ST_STATE_ACPU_ENTER_IDLE		(7)
#define XEN_DOMU_BOOT_ST_STATE_SCPU_SUSPEND		(8)
#define XEN_DOMU_BOOT_ST_STATE_SCPU_RESUME		(9)
#define XEN_DOMU_BOOT_ST_STATE_ACPU_ACK			(10)


#define XEN_DOMU_BOOT_ST_EXT_SET(reg)			SET_VAL(reg, 8, 0)
#define XEN_DOMU_BOOT_ST_EXT_GET(reg)			GET_VAL(reg, 8, 0)
#endif /* defined(CONFIG_RTK_XEN_SUPPORT) && defined(CONFIG_RTK_RPC) */

#endif /* End of __SOC_RTK_IPC_SHM_H */
