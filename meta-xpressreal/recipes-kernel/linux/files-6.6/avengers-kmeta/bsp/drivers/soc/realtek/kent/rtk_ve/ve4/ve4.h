/**
  ve4.h

  linux device driver for VPU.

 Copyright (C) 2006 - 2013  REALTEK INC.

  This library is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the Free
  Software Foundation; either version 2.1 of the License, or (at your option)
  any later version.

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St, Fifth Floor, Boston, MA
  02110-1301  USA

*/

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#include <linux/fs.h>
#include <linux/types.h>

#include "ve4config.h"

#define USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY

#define VDI_IOCTL_MAGIC 'V'
#define VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY _IO(VDI_IOCTL_MAGIC, 0)
#define VDI_IOCTL_FREE_PHYSICALMEMORY _IO(VDI_IOCTL_MAGIC, 1)
#define VDI_IOCTL_WAIT_INTERRUPT _IO(VDI_IOCTL_MAGIC, 2)
#define VDI_IOCTL_SET_CLOCK_GATE _IO(VDI_IOCTL_MAGIC, 3)
#define VDI_IOCTL_RESET _IO(VDI_IOCTL_MAGIC, 4)
#define VDI_IOCTL_GET_INSTANCE_POOL _IO(VDI_IOCTL_MAGIC, 5)
#define VDI_IOCTL_GET_COMMON_MEMORY _IO(VDI_IOCTL_MAGIC, 6)
#define VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO _IO(VDI_IOCTL_MAGIC, 8)
#define VDI_IOCTL_OPEN_INSTANCE _IO(VDI_IOCTL_MAGIC, 9)
#define VDI_IOCTL_CLOSE_INSTANCE _IO(VDI_IOCTL_MAGIC, 10)
#define VDI_IOCTL_GET_INSTANCE_NUM _IO(VDI_IOCTL_MAGIC, 11)
#define VDI_IOCTL_GET_REGISTER_INFO _IO(VDI_IOCTL_MAGIC, 12)
#define VDI_IOCTL_GET_REGISTER2_INFO _IO(VDI_IOCTL_MAGIC, 13)

/* RTK ioctl */
#define VDI_IOCTL_SET_RTK_CLK_GATING _IO(VDI_IOCTL_MAGIC, 16)
#define VDI_IOCTL_GET_TOTAL_INSTANCE_NUM _IO(VDI_IOCTL_MAGIC, 17)

#define VE_PLL_SYSH	0x000
#define VE_PLL_VE1	0x001
#define VE_PLL_VE2	0x010

typedef struct vpudrv_buffer_t {
	unsigned int size;
	unsigned long long phys_addr;
	unsigned long long base;
	unsigned long virt_addr; /* virtual user space address */
	unsigned int mem_type; /* RTK, for protect memory */
} vpudrv_buffer_t;

typedef struct vpu_bit_firmware_info_t {
	unsigned int size; /* size of this structure*/
	unsigned int core_idx;
	unsigned long reg_base_offset;
	unsigned short bit_code[512];
} vpu_bit_firmware_info_t;

typedef struct vpu_clock_info_t{
	unsigned int core_idx;
	unsigned int enable;
	unsigned int value;
} vpu_clock_info_t;

typedef struct vpudrv_inst_info_t {
	unsigned int core_idx;
	unsigned int inst_idx;
	int inst_open_count; /* for output only*/
} vpudrv_inst_info_t;

typedef struct vpudrv_intr_info_t {
	unsigned int timeout;
	int intr_reason;
	int intr_inst_index;
} vpudrv_intr_info_t;

typedef struct vpu_drv_context_t {
	struct fasync_struct *async_queue;
	unsigned long interrupt_reason_ve4;
	/* !<< device reference count. Not instance count */
	u32 open_count;
} vpu_drv_context_t;

/* To track the allocated memory buffer */
typedef struct vpudrv_buffer_pool_t {
	struct list_head list;
	struct vpudrv_buffer_t vb;
	struct file *filp;
} vpudrv_buffer_pool_t;

/* To track the instance index and buffer in instance pool */
typedef struct vpudrv_instanace_list_t {
	struct list_head list;
	unsigned long inst_idx;
	unsigned long core_idx;
	struct file *filp;
} vpudrv_instanace_list_t;

typedef struct vpudrv_instance_pool_t {
	unsigned char codecInstPool[MAX_NUM_INSTANCE][MAX_INST_HANDLE_SIZE];
	vpudrv_buffer_t vpu_common_buffer;
	int vpu_instance_num;
	int instance_pool_inited;
	void* pendingInst;
	int pendingInstIdxPlus1;
} vpudrv_instance_pool_t;

#define VDI_NUM_LOCK_HANDLES 6

extern vpudrv_buffer_t *kent_ve4_get_instance_pool(void);
extern vpudrv_buffer_t *kent_ve4_get_common_memory(void);
extern vpudrv_buffer_t *kent_ve4_get_vpu_register(void);
extern vpudrv_buffer_t *kent_ve4_get_vpu_register2(void);
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
extern vpudrv_buffer_t vpu_get_video_memory(void);
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */

extern void kent_ve4_sem_up(void);
extern void kent_ve4_open_ref_count_inc(void);
extern void kent_ve4_open_ref_count_dec(void);
extern int kent_ve4_down_interruptible(void);
extern void kent_ve4_clock_getting(vpu_clock_info_t *clockInfo);
extern void kent_ve4_add_vbp_list(vpudrv_buffer_pool_t *vbp, struct file *filp);
extern int kent_ve4_open_inst(vpudrv_inst_info_t *inst_info, struct file *filp);
extern void kent_ve4_close_inst(vpudrv_inst_info_t *inst_info);
extern void kent_ve4_get_inst_num(vpudrv_inst_info_t *inst_info);
extern void kent_ve4_get_total_inst_num(vpudrv_inst_info_t *inst_info);
extern void kent_ve4_free_mem(vpudrv_buffer_t *vb);
extern int kent_ve4_wait_init(vpu_drv_context_t *dev, vpudrv_intr_info_t *info);
extern int kent_ve4_alloc_from_vm(void);
extern int kent_ve4_alloc_from_dmabuffer2(void);
extern int kent_ve4_alloc_dma_buffer(vpudrv_buffer_t *vb);
extern void kent_ve4_free_dma_buffer(vpudrv_buffer_t *vb);
#endif
