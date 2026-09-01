/*
 * ve1.c
 *
 * linux device driver for VE1.
 *
 * Copyright (C) 2006 - 2013  REALTEK INC.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 *
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <soc/realtek/memory.h>
#include <soc/realtek/rtk_media_heap.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/devfreq.h>
#include <linux/devfreq_cooling.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "ve1.h"
#include "compat_ve1.h"

enum rtk_ve1_chip_type g_chip_type = CHIP_TYPE_UNKNOWN;

//#define ENABLE_DEBUG_MSG
#ifdef ENABLE_DEBUG_MSG
#define DPRINTK(args...) pr_info(args);
#else
#define DPRINTK(args...)
#endif /* ENABLE_DEBUG_MSG */

#define DEV_NAME "[RTK_VE1]"
#if !IS_ENABLED(CONFIG_RTK_VE1_V4L2_BEHAVIOR)
#define DISABLE_ORIGIN_SUSPEND
#endif
#define USE_HRTIMEOUT_INSTEAD_OF_TIMEOUT

/* definitions to be changed as customer  configuration */
/* if you want to have clock gating scheme frame by frame */
/* #define VPU_SUPPORT_CLOCK_CONTROL */

/* if the platform driver knows the name of this driver */
/* VPU_PLATFORM_DEVICE_NAME */
#define VPU_SUPPORT_PLATFORM_DRIVER_REGISTER

/* if this driver knows the dedicated video memory address */
#ifdef CONFIG_RTK_PLATFORM_FPGA
#define VPU_SUPPORT_RESERVED_VIDEO_MEMORY
#endif

#define VPU_PLATFORM_DEVICE_NAME "vdec"
#define VPU_CLK_NAME "vcodec"
#define VPU_DEV_NAME "vpu"

/* if the platform driver knows this driver */
/* the definition of VPU_REG_BASE_ADDR and VPU_REG_SIZE are not meaningful */

#define VPU_REG_BASE_ADDR 0x98040000
#define VPU_REG_SIZE (0xC000)
#define MS_TO_NS(x) (x * 1E6L)

#define VE1_IRQ_NUM (85)

/* this definition is only for realtek FPGA board env */
/* so for SOC env of customers can be ignored */

#ifndef VM_RESERVED	/*for kernel up to 3.7.0 version*/
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define VE_SECURE_NORMAL 1
#define VE_SECURE_PROTECTION 2

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
#define VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE 0x0b800000 //(62*1024*1024)
#define VPU_DRAM_PHYSICAL_BASE 0x03200000 //0x86C00000
#include "vmm.h"
static video_mm_t s_vmem;
static vpudrv_buffer_t s_video_memory = {0};
#endif /*VPU_SUPPORT_RESERVED_VIDEO_MEMORY*/

static int vpu_hw_reset(u32 coreIdx);

/* end customer definition */
static vpudrv_buffer_t s_instance_pool = {0};
static vpudrv_buffer_t s_common_memory = {0};
static vpu_drv_context_t s_vpu_drv_context = {0};
static struct miscdevice s_vpu_dev;

static struct device *p_vpu_dev;
static int s_vpu_open_ref_count;

static int s_ve1_irq = VE1_IRQ_NUM;

static int ve_cti_en = 1;
/* FIX ME after ver.B IC */
static int ve_idle_en = 0;

/* Reset control for rtd13xxe/rtd13xxd/rtd16xxb */
static struct reset_control *rstc_ve1;
static struct reset_control *rstc_ve1_mmu;
static struct reset_control *rstc_ve1_mmu_func;
static struct reset_control *rstc_iso_bist;

static vpudrv_buffer_t s_vpu_register = {0};
static vpudrv_buffer_t s_bond_register = {0};
static vpudrv_buffer_t s_dc_register = {0};
static vpudrv_buffer_t s_dmc_register = {0};

static atomic_t s_interrupt_flag_ve1;
static wait_queue_head_t s_interrupt_wait_q_ve1;
static DEFINE_SPINLOCK(s_intr_lock_ve1);   /* serializes fifo+flag writes; atomic_read in wait condition is lock-free by design */
#define MAX_INTERRUPT_QUEUE 16
typedef struct kfifo kfifo_t;
static kfifo_t s_interrupt_pending_q_ve1;

/*
 * Locking model:
 *   s_vpu_lock (spinlock) is the authority for list integrity of s_vbp_head
 *   and s_inst_list_head (and for s_vpu_open_ref_count). Take it locally at
 *   every add/del/iterate. NEVER hold it across a sleeping call (e.g.
 *   dma_free_coherent): detach nodes under the lock, then free them after
 *   releasing it (see vpu_free_buffers()/vpu_suspend()).
 *
 *   s_vpu_sem serializes most higher-level flows -- vpu_open()/vpu_release(),
 *   vpu_suspend(), and the ioctl paths that acquire it explicitly (buffer
 *   ALLOCATE/FREE, common-memory/instance-pool setup, etc.). It is the sole
 *   lock for s_vpu_drv_context.open_count (bumped in vpu_open(), dropped in
 *   vpu_release(), reset in vpu_suspend() -- all process context) and for
 *   s_instance_pool. It does NOT by itself guarantee list integrity.
 *
 *   How s_vpu_sem is acquired differs by caller, on purpose:
 *     - vpu_open() uses down_interruptible(): it runs in the open() syscall,
 *       which may legitimately fail with -EINTR/-ERESTARTSYS, and nothing has
 *       been allocated yet, so bailing out on a signal leaks nothing.
 *     - vpu_release() and vpu_suspend() use down() (uninterruptible): the VFS
 *       does not retry a failed ->release(), and suspend must complete its
 *       teardown; bailing out on a signal would skip the cleanup below and
 *       leak permanently. These paths must NOT use down_interruptible().
 *
 *   s_vpu_drv_context.last_busy_jiffies is deliberately NOT lock-protected:
 *   it is a best-effort timestamp written from ve1_irq_handler() (IRQ context,
 *   which cannot take a sleeping semaphore) as well as from vpu_open(), and
 *   read by the devfreq idle check. Do not try to serialize it with
 *   s_vpu_sem.
 *
 *   Exception -- instance open/close does NOT hold s_vpu_sem: the
 *   VDI_IOCTL_OPEN_INSTANCE/CLOSE_INSTANCE ioctls, the exported
 *   rtk_ve1_ioctl_open_instance()/rtk_ve1_ioctl_close_instance(), and the
 *   compat ioctl all call rtk_ve1_open_inst()/rtk_ve1_close_inst() directly,
 *   serialized only by s_vpu_lock. Therefore s_inst_list_head and
 *   s_vpu_open_ref_count may be mutated by these paths even while another
 *   thread holds s_vpu_sem. Any sem-holding code that reads them (e.g.
 *   vpu_suspend()) must still take s_vpu_lock and snapshot/iterate under it --
 *   never assume the semaphore excludes a concurrent open/close.
 *
 *   Nesting order is always s_vpu_sem -> s_vpu_lock, never the reverse.
 */
static spinlock_t s_vpu_lock = __SPIN_LOCK_UNLOCKED(s_vpu_lock);
static DEFINE_SEMAPHORE(s_vpu_sem, 1);
static struct list_head s_vbp_head = LIST_HEAD_INIT(s_vbp_head);
static struct list_head s_inst_list_head = LIST_HEAD_INIT(s_inst_list_head);

static vpu_bit_firmware_info_t s_bit_firmware_info[MAX_NUM_VPU_CORE];

#define BIT_BASE 0x0000
#define BIT_INT_STS (BIT_BASE + 0x010)
#define BIT_INT_REASON (BIT_BASE + 0x174)
#define BIT_INT_CLEAR (BIT_BASE + 0x00C)
#define VE_CTRL_REG (BIT_BASE + 0x3000)
#define VE_CTI_GRP_REG (BIT_BASE + 0x3004)
#define VE_INT_STS_REG (BIT_BASE + 0x3020)
#define VE_MBIST_CTRL (BIT_BASE + 0x3C08)
#define VE_BISR_POWER_RESET (BIT_BASE + 0x3CB0)

#ifdef CONFIG_PM
/* implement to power management functions */
#define BIT_CODE_RUN (BIT_BASE + 0x000)
#define BIT_CODE_DOWN (BIT_BASE + 0x004)
#define BIT_INT_CLEAR (BIT_BASE + 0x00C)
#define BIT_INT_STS (BIT_BASE + 0x010)
#define BIT_CODE_RESET (BIT_BASE + 0x014)
#define BIT_INT_REASON (BIT_BASE + 0x174)
#define BIT_BUSY_FLAG (BIT_BASE + 0x160)
#define BIT_RUN_COMMAND (BIT_BASE + 0x164)
#define BIT_RUN_INDEX (BIT_BASE + 0x168)
#define BIT_RUN_COD_STD (BIT_BASE + 0x16C)

/* Product register */
#define VPU_PRODUCT_CODE_REGISTER   (BIT_BASE + 0x1044)

#ifndef DISABLE_ORIGIN_SUSPEND
static u32 s_vpu_reg_store[MAX_NUM_VPU_CORE][64];
#endif
#endif /* CONFIG_PM */

#define DECODER_ACTIVE_THRESHOLD_MS 2000

/*
 * common struct and definition
 */
#define ReadVpuRegister(addr, core) *(volatile unsigned int *)(s_vpu_register.virt_addr + (0x8000 * core) + addr)
#define WriteVpuRegister(addr, val, core) *(volatile unsigned int *)(s_vpu_register.virt_addr + (0x8000 * core) + addr) = (unsigned int)val
#define WriteVpu(addr, val) *(volatile unsigned int *)(addr) = (unsigned int)val;

#if IS_ENABLED(CONFIG_DMABUF_HEAPS_REALTEK)
static unsigned int to_heapflag(unsigned int mem_type)
{
	unsigned int flags;
	switch(mem_type) {
	case VE_SECURE_NORMAL:
		flags = (RTK_FLAG_SCPUACC | RTK_FLAG_HWIPACC |
			 RTK_FLAG_NONCACHED | RTK_FLAG_VE_SPEC);
		break;
	case VE_SECURE_PROTECTION:
		flags = (RTK_FLAG_HWIPACC | RTK_FLAG_PROTECTED_V2_VIDEO_POOL);
		break;
    default:
#if IS_ENABLED(CONFIG_RTK_VE1_V4L2_BEHAVIOR)
		flags = (RTK_FLAG_SCPUACC | RTK_FLAG_NONCACHED |
			RTK_FLAG_VO_POOL);
#else
		flags = (RTK_FLAG_SCPUACC | RTK_FLAG_HWIPACC |
			RTK_FLAG_NONCACHED);
#endif
    }
	return flags;
}
#endif

#define WRITE_DATA BIT(0)
#define POLLING_TIME 1000
#define INTR_OFFSET 0xa80
#define INTR_EN_OFFSET 0xa84
#define TO_PCPU_INTR_BIT BIT(3)
#define TO_PCPU_IPC_REGOFF 0x470
#define IPC_CMD_BLOCKING 1
#define IPC_CATE_PPC		0x2			/* Peripheral power control */
#define IPC_PPC_VETOP_ON	0xa208
static struct regmap *intr_regmap;
static struct regmap *ipc_regmap;

static void ve1_wrapper_setup_prince(unsigned int coreIdx)
{
	unsigned int ctrl_1;
	unsigned int ctrl_2;

	pr_info("%s ve1_wrapper_setup_prince coreIdx=0x%x\n", DEV_NAME, coreIdx);

	/* coreIdx == 0 */
	if ((coreIdx & (1 << 0)) != 0) {
		ctrl_1 = ReadVpuRegister(VE_CTRL_REG, 0);
		ctrl_2 = ReadVpuRegister(VE_CTI_GRP_REG, 0);
		ctrl_1 |= (ve_cti_en << 1 | ve_idle_en << 6);
		/* ve1_cti_cmd_depth for 1296 timing issue */
		ctrl_2 = (ctrl_2 & ~(0x3f << 24)) | (0x1a << 24);
		/*Set BISR POWER RESET bit12 to 1, make AXI available in stark*/
		WriteVpuRegister(VE_CTRL_REG, ctrl_1, 0);
		WriteVpuRegister(VE_CTI_GRP_REG, ctrl_2, 0);
	}
}

static void ve1_wrapper_setup_kent(unsigned int coreIdx)
{
	unsigned int ctrl_1;
	unsigned int ctrl_2;

	pr_info("%s ve1_wrapper_setup_kent coreIdx=0x%x\n", DEV_NAME, coreIdx);

	/* coreIdx == 0 */
	if ((coreIdx & (1 << 0)) != 0) {
		ctrl_1 = ReadVpuRegister(VE_CTRL_REG, 0);
		ctrl_2 = ReadVpuRegister(VE_CTI_GRP_REG, 0);
		ctrl_1 |= (ve_cti_en << 1 | ve_idle_en << 6);
		/* ve1_cti_cmd_depth for 1296 timing issue */
		ctrl_2 = (ctrl_2 & ~(0x3f << 24)) | (0x1a << 24);
		/*Set BISR POWER RESET bit12 to 1, make AXI available in stark*/
		WriteVpuRegister(VE_CTRL_REG, ctrl_1, 0);
		WriteVpuRegister(VE_CTI_GRP_REG, ctrl_2, 0);
	}
}

static void ve1_wrapper_setup_rtd13xxe(unsigned int coreIdx)
{
	unsigned int ctrl_1;
	unsigned int ctrl_2;
	unsigned int ctrl_4;
	int i;

	pr_info("%s ve1_wrapper_setup_rtd13xxe coreIdx=0x%x\n", DEV_NAME, coreIdx);

	/* coreIdx == 0 */
	if ((coreIdx & (1 << 0)) != 0) {
		ctrl_1 = ReadVpuRegister(VE_CTRL_REG, 0);
		ctrl_2 = ReadVpuRegister(VE_CTI_GRP_REG, 0);
		ctrl_1 |= (ve_cti_en << 1 | ve_idle_en << 6);
		/* ve1_cti_cmd_depth for 1296 timing issue */
		ctrl_2 = (ctrl_2 & ~(0x3f << 24)) | (0x1a << 24);
		/*Set BISR POWER RESET bit12 to 1, make AXI available in stark*/
		WriteVpuRegister(VE_CTRL_REG, ctrl_1, 0);
		WriteVpuRegister(VE_CTI_GRP_REG, ctrl_2, 0);

		for(i=0; i<2; i++) {  //workaround for TP1CK MEM TEST1 in stark
			ctrl_4 = ReadVpuRegister(VE_MBIST_CTRL, 0);
			ctrl_4 ^= (1<<2);  //toggle TEST1 signal of MEM
			WriteVpuRegister(VE_MBIST_CTRL, ctrl_4, 0);
		}
	}
}

static void ve1_wrapper_setup_rtd13xxd(unsigned int coreIdx)
{
	unsigned int ctrl_1;
	unsigned int ctrl_2;
	unsigned int ctrl_4;
	int i;

	pr_info("%s ve1_wrapper_setup_rtd13xxd coreIdx=0x%x\n", DEV_NAME, coreIdx);

	/* coreIdx == 0 */
	if ((coreIdx & (1 << 0)) != 0) {
		ctrl_1 = ReadVpuRegister(VE_CTRL_REG, 0);
		ctrl_2 = ReadVpuRegister(VE_CTI_GRP_REG, 0);
		ctrl_1 |= (ve_cti_en << 1 | ve_idle_en << 6);
		/* ve1_cti_cmd_depth for 1296 timing issue */
		ctrl_2 = (ctrl_2 & ~(0x3f << 24)) | (0x1a << 24);
		/*Set BISR POWER RESET bit12 to 1, make AXI available in stark*/
		WriteVpuRegister(VE_CTRL_REG, ctrl_1, 0);
		WriteVpuRegister(VE_CTI_GRP_REG, ctrl_2, 0);

		for(i=0; i<2; i++) {  //workaround for TP1CK MEM TEST1 in stark
			ctrl_4 = ReadVpuRegister(VE_MBIST_CTRL, 0);
			ctrl_4 ^= (1<<2);  //toggle TEST1 signal of MEM
			WriteVpuRegister(VE_MBIST_CTRL, ctrl_4, 0);
		}
	}
}

static void ve1_wrapper_setup_rtd16xxb(unsigned int coreIdx)
{
	unsigned int ctrl_1;
	unsigned int ctrl_2;
	unsigned int ctrl_3;
	unsigned int ctrl_4;
	int i;

	pr_info("%s ve1_wrapper_setup_rtd16xxb coreIdx=0x%x\n", DEV_NAME, coreIdx);

	/* coreIdx == 0 */
	if ((coreIdx & (1 << 0)) != 0) {
		ctrl_1 = ReadVpuRegister(VE_CTRL_REG, 0);
		ctrl_2 = ReadVpuRegister(VE_CTI_GRP_REG, 0);
		ctrl_3 = ReadVpuRegister(VE_BISR_POWER_RESET, 0);
		ctrl_1 |= (ve_cti_en << 1 | ve_idle_en << 6);
		/* ve1_cti_cmd_depth for 1296 timing issue */
		ctrl_2 = (ctrl_2 & ~(0x3f << 24)) | (0x1a << 24);
		ctrl_3 |= (1<<12);
		/*Set BISR POWER RESET bit12 to 1, make AXI available in stark*/
		WriteVpuRegister(VE_CTRL_REG, ctrl_1, 0);
		WriteVpuRegister(VE_CTI_GRP_REG, ctrl_2, 0);
		WriteVpuRegister(VE_BISR_POWER_RESET, ctrl_3, 0);

		for(i=0; i<2; i++) {  //workaround for TP1CK MEM TEST1 in stark
			ctrl_4 = ReadVpuRegister(VE_MBIST_CTRL, 0);
			ctrl_4 ^= (1<<2);  //toggle TEST1 signal of MEM
			WriteVpuRegister(VE_MBIST_CTRL, ctrl_4, 0);
		}
	}
}

static void ve1_wrapper_setup(unsigned int coreIdx)
{
	switch (g_chip_type) {
		case CHIP_TYPE_PRINCE:
			ve1_wrapper_setup_prince(coreIdx);
			break;
		case CHIP_TYPE_KENT:
			ve1_wrapper_setup_kent(coreIdx);
			break;
		case CHIP_TYPE_RTD13XXE:
			ve1_wrapper_setup_rtd13xxe(coreIdx);
			break;
		case CHIP_TYPE_RTD13XXD:
			ve1_wrapper_setup_rtd13xxd(coreIdx);
			break;
		case CHIP_TYPE_RTD16XXB:
			ve1_wrapper_setup_rtd16xxb(coreIdx);
			break;
		default:
			break;
	}
}

static int vpu_hw_reset(u32 coreIdx)
{
	if ((g_chip_type == CHIP_TYPE_RTD13XXE || g_chip_type == CHIP_TYPE_RTD13XXD
		|| g_chip_type == CHIP_TYPE_RTD16XXB) && rstc_ve1) {
		reset_control_reset(rstc_ve1);
	}
	ve1_wrapper_setup((1 << coreIdx));
	return 0;
}

static void vpu_setup_mmu_prince(void)
{
	pr_info("%s vpu_setup_mmu_prince\n", DEV_NAME);
}

static void vpu_setup_mmu_kent(void)
{
	pr_info("%s vpu_setup_mmu_kent\n", DEV_NAME);
}

static void vpu_setup_mmu_rtd13xxe(void)
{
	pr_info("%s vpu_setup_mmu_rtd13xxe\n", DEV_NAME);
	reset_control_deassert(rstc_ve1_mmu);
	reset_control_deassert(rstc_ve1_mmu_func);
}

static void vpu_setup_mmu_rtd13xxd(void)
{
	pr_info("%s vpu_setup_mmu_rtd13xxd\n", DEV_NAME);
	reset_control_deassert(rstc_ve1_mmu);
	reset_control_deassert(rstc_ve1_mmu_func);
}

static void vpu_setup_mmu_rtd16xxb(void)
{
	pr_info("%s vpu_setup_mmu_rtd16xxb\n", DEV_NAME);
}

static void vpu_setup_mmu(void)
{
	switch (g_chip_type) {
		case CHIP_TYPE_PRINCE:
			vpu_setup_mmu_prince();
			break;
		case CHIP_TYPE_KENT:
			vpu_setup_mmu_kent();
			break;
		case CHIP_TYPE_RTD13XXE:
			vpu_setup_mmu_rtd13xxe();
			break;
		case CHIP_TYPE_RTD13XXD:
			vpu_setup_mmu_rtd13xxd();
			break;
		case CHIP_TYPE_RTD16XXB:
			vpu_setup_mmu_rtd16xxb();
			break;
		default:
			break;
	}
}

int rtk_ve1_alloc_dma_buffer(vpudrv_buffer_t *vb)
{
	if (!vb)
		return -1;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	vb->phys_addr = (unsigned long)vmem_alloc(&s_vmem, vb->size, 0);
	if ((unsigned long)vb->phys_addr  == (unsigned long)-1) {
		pr_err("%s Physical memory allocation error size=%d\n", DEV_NAME, vb->size);
		return -1;
	}

	vb->base = (unsigned long)(s_video_memory.base + (vb->phys_addr - s_video_memory.phys_addr));
#else
	mutex_lock(&p_vpu_dev->mutex);
#if IS_ENABLED(CONFIG_DMABUF_HEAPS_REALTEK)
	rheap_setup_dma_pools(s_vpu_dev.this_device, "rtk_media_heap",
				to_heapflag(vb->mem_type), __func__);
#endif

	vb->base = (unsigned long)dma_alloc_coherent(s_vpu_dev.this_device, PAGE_ALIGN(vb->size), (dma_addr_t *) (&vb->phys_addr), GFP_DMA | GFP_KERNEL);
	mutex_unlock(&p_vpu_dev->mutex);
	if ((void *)(vb->base) == NULL) {
		pr_err("%s Physical memory allocation error size=%d\n", DEV_NAME, vb->size);
		return -1;
	}
	vb->virt_addr = vb->base;
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */

	DPRINTK("%s [%d]rtk_ve1_alloc_dma_buffer.base:0x%lx.phys_addr:0x%lx.size:%d\n",DEV_NAME,__LINE__,vb->base,vb->phys_addr,vb->size);
	return 0;
}

__maybe_unused
static int vpu_alloc_dma_buffer2(vpudrv_buffer_t *vb)
{
	if (!vb)
		return -1;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	vb->phys_addr = (unsigned long)vmem_alloc(&s_vmem, vb->size, 0);
	if ((unsigned long)vb->phys_addr  == (unsigned long)-1) {
		pr_err("%s Physical memory allocation error size=%d\n", DEV_NAME, vb->size);
		return -1;
	}

	vb->base = (unsigned long)(s_video_memory.base + (vb->phys_addr - s_video_memory.phys_addr));
#else
	mutex_lock(&p_vpu_dev->mutex);
#if IS_ENABLED(CONFIG_DMABUF_HEAPS_REALTEK)
	rheap_setup_dma_pools(s_vpu_dev.this_device, "rtk_media_heap",
				to_heapflag(vb->mem_type), __func__);
#endif

	vb->base = (unsigned long)dma_alloc_coherent(s_vpu_dev.this_device, PAGE_ALIGN(vb->size), (dma_addr_t *) (&vb->phys_addr), GFP_DMA | GFP_KERNEL);
	mutex_unlock(&p_vpu_dev->mutex);
	if ((void *)(vb->base) == NULL) {
		pr_err("%s Physical memory allocation error size=%d\n", DEV_NAME, vb->size);
		return -1;
	}
	vb->virt_addr = vb->base;
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */

	DPRINTK("%s [%d]vpu_alloc_dma_buffer2.base:0x%lx.phys_addr:0x%lx.size:%d\n",DEV_NAME,__LINE__,vb->base,vb->phys_addr,vb->size);
	return 0;
}

/* Release the DMA backing using vb->base as-is. Use this when the caller
 * already holds the full (untruncated) kernel base — e.g. teardown paths that
 * own the vpudrv_buffer_t, or the instance/common pool. Does NOT touch s_vbp_head. */
void rtk_ve1_free_dma_buffer_base(vpudrv_buffer_t *vb)
{
	if (!vb || !vb->base)
		return;
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	vmem_free(&s_vmem, vb->phys_addr, 0);
#else
	dma_free_coherent(s_vpu_dev.this_device, PAGE_ALIGN(vb->size),
		(void *)vb->base, vb->phys_addr);
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */
}

void rtk_ve1_free_dma_buffer(vpudrv_buffer_t *vb)
{
	vpudrv_buffer_pool_t *vbp;
	unsigned long base_in_vbp = 0;
	if (!vb)
		return;

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (vb->base)
		vmem_free(&s_vmem, vb->phys_addr, 0);
#else
	if (vb->base) {
		/* vb->base may be a 32-bit-truncated value passed from a 32-bit
		 * userspace via ioctl; recover the full 64-bit kernel base by
		 * matching phys_addr against s_vbp_head. */
		spin_lock(&s_vpu_lock);
		list_for_each_entry(vbp, &s_vbp_head, list) {
			if (vbp->vb.phys_addr == vb->phys_addr) {
				base_in_vbp = vbp->vb.base;
				break;
			}
		}
		spin_unlock(&s_vpu_lock);
		if (base_in_vbp == 0) {
			pr_err("%d.%s.no match vbp->vb.phys_addr:%lx.size:%d",
				__LINE__, __func__,
				vb->phys_addr, vb->size);
			base_in_vbp = vb->base;
		}
		DPRINTK("%d.%s.base_in_vbp:0x%px.vb->size:%d(maybe not 4096 align)\n",
			__LINE__, __func__,
			(void *)(base_in_vbp), vb->size);
		dma_free_coherent(s_vpu_dev.this_device, PAGE_ALIGN(vb->size), (void *)base_in_vbp, vb->phys_addr);
	}
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */
}

/* size=40 for Android M */
#define PTHREAD_MUTEX_T_HANDLE_SIZE 40

static int rtk_ve1_free_instances(struct file *filp)
{
	vpudrv_instanace_list_t *vil, *n;
	vpudrv_instance_pool_t *vip;
	void *vip_base;
	int instance_pool_size_per_core;
	void *vdi_mutexes_base;
	const int PTHREAD_MUTEX_T_DESTROY_VALUE[10] = {0xdead10cc};
	LIST_HEAD(to_free);

	DPRINTK("%s rtk_ve1_free_instances\n", DEV_NAME);

	/* s_instance_pool.size  assigned to the size of all core once call VDI_IOCTL_GET_INSTANCE_POOL by user. */
	instance_pool_size_per_core = (s_instance_pool.size/MAX_NUM_VPU_CORE);

	/* phase 1: detach this filp's nodes and drop the ref count under the
	 * spinlock (no sleeping).  s_inst_list_head and s_vpu_open_ref_count are
	 * both s_vpu_lock-protected.  The instance-pool teardown below touches
	 * s_instance_pool (s_vpu_sem-protected, held by the caller) and ends in
	 * kfree(), so it runs in phase 2 outside the spinlock -- on PREEMPT_RT
	 * the spinlock is a sleeping mutex and kfree() may sleep. */
	spin_lock(&s_vpu_lock);
	list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
		if (vil->filp == filp) {
			s_vpu_open_ref_count--;
			list_move(&vil->list, &to_free);
		}
	}
	spin_unlock(&s_vpu_lock);

	/* phase 2: tear down the instance pool slot and free outside the lock */
	list_for_each_entry_safe(vil, n, &to_free, list) {
		vip_base = (void *)(s_instance_pool.base + (instance_pool_size_per_core*vil->core_idx));
		DPRINTK("%s rtk_ve1_free_instances detect instance crash instIdx=%d, coreIdx=%d, vip_base=%p, instance_pool_size_per_core=%d\n", DEV_NAME, (int)vil->inst_idx, (int)vil->core_idx, vip_base, (int)instance_pool_size_per_core);
		vip = (vpudrv_instance_pool_t *)vip_base;

		if (vip) {
			/* only first 4 byte is key point(inUse of CodecInst in vpuapi) to free the corresponding instance. */
			memset(&vip->codecInstPool[vil->inst_idx], 0x00, 4);

			if (vil->inst_idx == (vip->pendingInstIdxPlus1-1) && vip->pendingInst != 0) {
				pr_warn("%s vil->inst_idx:%d, vil->core_idx:%d is pending, clear in here\n", DEV_NAME, (int)vil->inst_idx, (int)vil->core_idx);
				vip->pendingInst = 0;
				vip->pendingInstIdxPlus1 = 0;
			}

			vdi_mutexes_base = (vip_base + (instance_pool_size_per_core - PTHREAD_MUTEX_T_HANDLE_SIZE*4));
			DPRINTK("%s rtk_ve1_free_instances : force to destroy vdi_mutexes_base=%p in userspace \n", DEV_NAME, vdi_mutexes_base);
			if (vdi_mutexes_base) {
				int i;
				for (i = 0; i < 4; i++) {
					memcpy(vdi_mutexes_base, &PTHREAD_MUTEX_T_DESTROY_VALUE, PTHREAD_MUTEX_T_HANDLE_SIZE);
					vdi_mutexes_base += PTHREAD_MUTEX_T_HANDLE_SIZE;
				}
			}
		}
		list_del(&vil->list);
		kfree(vil);
	}
	return 1;
}

static int vpu_free_buffers(struct file *filp)
{
	vpudrv_buffer_pool_t *pool, *n;
	LIST_HEAD(to_free);

	DPRINTK("%s vpu_free_buffers\n", DEV_NAME);

	/* phase 1: detach this filp's nodes under the spinlock (no sleeping) */
	spin_lock(&s_vpu_lock);
	list_for_each_entry_safe(pool, n, &s_vbp_head, list) {
		if (pool->filp == filp)
			list_move(&pool->list, &to_free);
	}
	spin_unlock(&s_vpu_lock);

	/* phase 2: free outside the lock; base is the full kernel base */
	list_for_each_entry_safe(pool, n, &to_free, list) {
		if (pool->vb.base)
			rtk_ve1_free_dma_buffer_base(&pool->vb);
		list_del(&pool->list);
		kfree(pool);
	}

	return 0;
}

static irqreturn_t ve1_irq_handler(int irq, void *dev_id)
{
	vpu_drv_context_t *dev = (vpu_drv_context_t *)dev_id;

	/* this can be removed. it also work in VPU_WaitInterrupt of API function */
	int core = 0;
	unsigned long interrupt_reason_ve1 = 0;
	unsigned int vpu_int_sts_ve1 = 0;

	/* it means that we didn't get an information the current core from API layer. No core activated.*/
	if (s_bit_firmware_info[core].size == 0) {
		pr_err("[VPUDRV] :  s_bit_firmware_info[core].size is zero\n");
		return IRQ_HANDLED;
	}

	vpu_int_sts_ve1 = ReadVpuRegister(BIT_INT_STS, core);
	if (vpu_int_sts_ve1) {
		dev->last_busy_jiffies = jiffies;

		interrupt_reason_ve1 = ReadVpuRegister(BIT_INT_REASON, core);
#if !IS_ENABLED(CONFIG_RTK_VE1_V4L2_BEHAVIOR)
		WriteVpuRegister(BIT_INT_REASON, 0, core);
#endif
		WriteVpuRegister(BIT_INT_CLEAR, 0x1, core);
		if (interrupt_reason_ve1 == 0) {
			pr_err("%s %d.interrupt_reason_ve1:%lu\n",DEV_NAME,__LINE__,interrupt_reason_ve1);
		}
	}

	//DPRINTK("%s VE1 intr_reason: 0x%08lx\n", DEV_NAME, dev->interrupt_reason_ve1);

	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN); /* notify the interrupt to user space */

	if (vpu_int_sts_ve1) {
		if (core == 0) {
			/* hardirq: local IRQ already off, plain spin_lock is correct */
			spin_lock(&s_intr_lock_ve1);
			if (!kfifo_is_full(&s_interrupt_pending_q_ve1)) {
				kfifo_in(&s_interrupt_pending_q_ve1,
					 &interrupt_reason_ve1, sizeof(unsigned long));
			}
			else {
				pr_err("%s %d.kfifo_is_full kfifo_count=%d.reason:0x%lx\n",
					DEV_NAME, __LINE__,
					kfifo_len(&s_interrupt_pending_q_ve1),
					interrupt_reason_ve1);
			}
			atomic_set(&s_interrupt_flag_ve1, 1);
			spin_unlock(&s_intr_lock_ve1);
			wake_up_interruptible(&s_interrupt_wait_q_ve1);
		}
		//DPRINTK("%s [-]%s\n", DEV_NAME, __func__);
	}

	return IRQ_HANDLED;
}

int rtk_ve1_down_interruptible(void)
{
	return down_interruptible(&s_vpu_sem);
}

void rtk_ve1_sem_up(void)
{
	up(&s_vpu_sem);
}

static int vpu_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	DPRINTK("%s [+] %s\n", DEV_NAME, __func__);

	ret = rtk_ve1_down_interruptible();
	if (ret != 0)
		return ret;

	s_vpu_drv_context.open_count++;
	if (s_vpu_drv_context.last_busy_jiffies == 0) {
		s_vpu_drv_context.last_busy_jiffies = jiffies;
	}
	filp->private_data = (void *)(&s_vpu_drv_context);
	rtk_ve1_sem_up();

	DPRINTK("%s [-] %s\n", DEV_NAME, __func__);

	return 0;
}

void rtk_ve1_clock_getting(vpu_clock_info_t *clockInfo)
{
	if (clockInfo->enable) {
		pm_runtime_get_sync(p_vpu_dev);
		s_vpu_drv_context.is_decoding_active = 1;
	} else {
		pm_runtime_mark_last_busy(p_vpu_dev);
		pm_runtime_put_autosuspend(p_vpu_dev);
		s_vpu_drv_context.is_decoding_active = 0;
	}
}

void rtk_ve1_add_vbp_list(vpudrv_buffer_pool_t *vbp, struct file *filp)
{
	vbp->filp = filp;
	spin_lock(&s_vpu_lock);
	list_add(&vbp->list, &s_vbp_head);
	spin_unlock(&s_vpu_lock);
}

int rtk_ve1_open_inst(vpudrv_inst_info_t *inst_info, struct file *filp)
{
	vpudrv_instanace_list_t *vil, *n;

	vil = kzalloc(sizeof(*vil), GFP_KERNEL);
	if (!vil)
		return -ENOMEM;

	vil->inst_idx = inst_info->inst_idx;
	vil->core_idx = inst_info->core_idx;
	vil->filp = filp;

	spin_lock(&s_vpu_lock);
	list_add(&vil->list, &s_inst_list_head);
	s_vpu_open_ref_count++;

	inst_info->inst_open_count = 0;
	list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
		if (vil->core_idx == inst_info->core_idx)
			inst_info->inst_open_count++;
	}
	spin_unlock(&s_vpu_lock);

	return 0;
}

void rtk_ve1_close_inst(vpudrv_inst_info_t *inst_info)
{
	vpudrv_instanace_list_t *vil, *n;
	bool found = false;

	spin_lock(&s_vpu_lock);
	list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
		if (vil->inst_idx == inst_info->inst_idx && vil->core_idx == inst_info->core_idx) {
			list_del(&vil->list);
			kfree(vil);
			found = true;
			break;
		}
	}
	/* only drop the ref count if an instance was actually removed, so a
	 * stray close of a non-existent inst (e.g. double-close) cannot
	 * underflow s_vpu_open_ref_count and skew the suspend teardown check */
	if (found)
		s_vpu_open_ref_count--;

	inst_info->inst_open_count = 0;
	list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
		if (vil->core_idx == inst_info->core_idx)
			inst_info->inst_open_count++;
	}
	spin_unlock(&s_vpu_lock);
}

void rtk_ve1_get_inst_num(vpudrv_inst_info_t *inst_info)
{
	vpudrv_instanace_list_t *vil, *n;

	inst_info->inst_open_count = 0;

	spin_lock(&s_vpu_lock);
	list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
		if (vil->core_idx == inst_info->core_idx)
			inst_info->inst_open_count++;
	}
	spin_unlock(&s_vpu_lock);
}

void rtk_ve1_get_total_inst_num(vpudrv_inst_info_t *inst_info)
{
	vpudrv_instanace_list_t *vil, *n;

	inst_info->inst_open_count = 0;

	spin_lock(&s_vpu_lock);
	list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
		inst_info->inst_open_count++;
	}
	spin_unlock(&s_vpu_lock);
}

void rtk_ve1_free_mem(vpudrv_buffer_t *vb)
{
	vpudrv_buffer_pool_t *vbp, *n;

	spin_lock(&s_vpu_lock);
	list_for_each_entry_safe(vbp, n, &s_vbp_head, list) {
		if (vbp->vb.phys_addr == vb->phys_addr) {
			list_del(&vbp->list);
			kfree(vbp);
			break;
		}
	}
	spin_unlock(&s_vpu_lock);
}

int rtk_ve1_wait_init(vpu_drv_context_t *dev, vpudrv_intr_info_t *info)
{
	int ret = 0;
	unsigned long intr_reason_in_q;
	int interrupt_flag_in_q;
	unsigned long flags;

	if (info->core_idx == 0) {
#ifdef USE_HRTIMEOUT_INSTEAD_OF_TIMEOUT
		ktime_t ktime;
		unsigned long delay_in_ms = 1L;
		ktime = ktime_set(0, MS_TO_NS(delay_in_ms));
		//ktime = ktime_set(0, MS_TO_NS((u64)info.timeout));
#endif /* USE_HRTIMEOUT_INSTEAD_OF_TIMEOUT */

		if (dev->interrupt_reason_ve1 != 0) {
			pr_err("%s %d.strange.dev->r:0x%lx.\n",
				DEV_NAME,__LINE__,dev->interrupt_reason_ve1);
		}

		/* drain anything already queued before we sleep */
		intr_reason_in_q = 0;
		spin_lock_irqsave(&s_intr_lock_ve1, flags);
		interrupt_flag_in_q = kfifo_out(&s_interrupt_pending_q_ve1,
						&intr_reason_in_q, sizeof(unsigned long));
		if (kfifo_is_empty(&s_interrupt_pending_q_ve1))
			atomic_set(&s_interrupt_flag_ve1, 0);   /* flag mirrors fifo */
		spin_unlock_irqrestore(&s_intr_lock_ve1, flags);
		//pr_info("%s %d.interrupt_flag_in_q:%d.intr_reason_in_q:0x%lx.kfifo_count=%d\n",DEV_NAME,__LINE__,interrupt_flag_in_q,intr_reason_in_q,kfifo_len(&s_interrupt_pending_q_ve1));
		if (interrupt_flag_in_q > 0)
		{
			//pr_info("%s %d.interrupt_flag_in_q:%d.intr_reason_in_q:0x%lx.kfifo_count=%d\n",DEV_NAME,__LINE__,interrupt_flag_in_q,intr_reason_in_q,kfifo_len(&s_interrupt_pending_q_ve1));
			dev->interrupt_reason_ve1 = intr_reason_in_q;
			goto INTERRUPT_REMAIN_IN_QUEUE;
		}
		dev->interrupt_reason_ve1 = 0;

		smp_rmb();
#ifdef USE_HRTIMEOUT_INSTEAD_OF_TIMEOUT
		ret = wait_event_interruptible_hrtimeout(s_interrupt_wait_q_ve1,
					atomic_read(&s_interrupt_flag_ve1) != 0, ktime);
#else /* USE_HRTIMEOUT_INSTEAD_OF_TIMEOUT */
		ret = wait_event_interruptible_timeout(s_interrupt_wait_q_ve1,
					atomic_read(&s_interrupt_flag_ve1) != 0,
					msecs_to_jiffies(info->timeout));
#endif /* USE_HRTIMEOUT_INSTEAD_OF_TIMEOUT */

		if (signal_pending(current)) {
			//pr_err("%s %d.signal_pending\n",DEV_NAME,__LINE__);
			return -ERESTARTSYS;
		}

		/*
		 * One consume path for BOTH the woken and the timed-out case,
		 * done under the ISR lock so flag and fifo can never disagree.
		 * An interrupt that lands right at the timeout boundary is
		 * picked up here instead of being left behind as stale state
		 * for the next wait.
		 */
		intr_reason_in_q = 0;
		spin_lock_irqsave(&s_intr_lock_ve1, flags);
		interrupt_flag_in_q = kfifo_out(&s_interrupt_pending_q_ve1,
						&intr_reason_in_q, sizeof(unsigned long));
		if (kfifo_is_empty(&s_interrupt_pending_q_ve1))
			atomic_set(&s_interrupt_flag_ve1, 0);
		spin_unlock_irqrestore(&s_intr_lock_ve1, flags);
		//pr_info("%s %d.interrupt_flag_in_q:%d.intr_reason_in_q:0x%lx.kfifo_count=%d\n",DEV_NAME,__LINE__,interrupt_flag_in_q,intr_reason_in_q,kfifo_len(&s_interrupt_pending_q_ve1));
		if (interrupt_flag_in_q > 0) {
			dev->interrupt_reason_ve1 = intr_reason_in_q;
		}
		else {
			/* genuinely nothing pending -> real timeout, exit clean */
			dev->interrupt_reason_ve1 = 0;
			return -ETIME;
		}

INTERRUPT_REMAIN_IN_QUEUE:
		info->intr_reason = dev->interrupt_reason_ve1;
		dev->interrupt_reason_ve1 = 0;   /* flag already maintained under lock */
		ret = 0;
	}

	return ret;
}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
vpudrv_buffer_t vpu_get_video_memory(void)
{
	return s_video_memory;
}
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */

vpudrv_buffer_t *rtk_ve1_get_instance_pool(void)
{
	return &s_instance_pool;
}

vpudrv_buffer_t *rtk_ve1_get_common_memory(void)
{
	return &s_common_memory;
}

vpudrv_buffer_t *rtk_ve1_get_vpu_register(void)
{
	return &s_vpu_register;
}

vpudrv_buffer_t *rtk_ve1_get_bond_register(void)
{
	return &s_bond_register;
}

vpudrv_buffer_t *rtk_ve1_get_dc_register(void)
{
	return &s_dc_register;
}

vpudrv_buffer_t *rtk_ve1_get_dmc_register(void)
{
	return &s_dmc_register;
}

int rtk_ve1_alloc_from_vm(void)
{
	int ret = 0;

	s_instance_pool.size = PAGE_ALIGN(s_instance_pool.size);
	s_instance_pool.base = (unsigned long)vmalloc(s_instance_pool.size);
	s_instance_pool.phys_addr = s_instance_pool.base;
	s_instance_pool.virt_addr = s_instance_pool.base;

	if (s_instance_pool.base == 0) {
		return -ENOMEM;
	}

	memset((void *)s_instance_pool.base, 0x0, s_instance_pool.size);

	return ret;
}

int rtk_ve1_alloc_from_dmabuffer2(void)
{
	int ret = 0;

	if (vpu_alloc_dma_buffer2(&s_instance_pool) != 0) {
		return -ENOMEM;
	}

	memset((void *)s_instance_pool.base, 0x0, s_instance_pool.size);

	return ret;
}
static long vpu_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	int ret = 0;
	vpu_drv_context_t *dev = (vpu_drv_context_t *)filp->private_data;

	switch (cmd) {
	case VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY:
	{
		vpudrv_buffer_pool_t *vbp;

		DPRINTK("%s [+]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n", DEV_NAME);

		ret = rtk_ve1_down_interruptible();
		if (ret != 0)
			return ret;

		vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
		if (!vbp) {
			rtk_ve1_sem_up();
			return -ENOMEM;
		}

		ret = copy_from_user(&(vbp->vb), (vpudrv_buffer_t *)arg,
							 sizeof(vpudrv_buffer_t));
		if (ret) {
			kfree(vbp);
			rtk_ve1_sem_up();
			return -EFAULT;
		}

		ret = rtk_ve1_alloc_dma_buffer(&(vbp->vb));
		if (ret != 0) {
			kfree(vbp);
			rtk_ve1_sem_up();
			return -ENOMEM;
		}

		ret = copy_to_user((void __user *)arg, &(vbp->vb),
						   sizeof(vpudrv_buffer_t));
		if (ret) {
			/* the buffer is not on s_vbp_head yet; free its DMA
			 * backing by the full base we still hold, then the node */
			rtk_ve1_free_dma_buffer_base(&(vbp->vb));
			kfree(vbp);
			rtk_ve1_sem_up();
			return -EFAULT;
		}

		rtk_ve1_add_vbp_list(vbp, filp);
		rtk_ve1_sem_up();

		DPRINTK("%s [-]VDI_IOCTL_ALLOCATE_PHYSICAL_MEMORY\n", DEV_NAME);
	}
	break;
	case VDI_IOCTL_FREE_PHYSICALMEMORY:
	{
		vpudrv_buffer_t vb;

		DPRINTK("%s [+]VDI_IOCTL_FREE_PHYSICALMEMORY\n", DEV_NAME);

		ret = rtk_ve1_down_interruptible();
		if (ret != 0)
			return ret;

		ret = copy_from_user(&vb, (vpudrv_buffer_t *)arg,
				     sizeof(vpudrv_buffer_t));
		if (ret) {
			rtk_ve1_sem_up();
			return -EFAULT;
		}

		if (vb.base)
			rtk_ve1_free_dma_buffer(&vb);

		rtk_ve1_free_mem(&vb);
		rtk_ve1_sem_up();

		DPRINTK("%s [-]VDI_IOCTL_FREE_PHYSICALMEMORY\n", DEV_NAME);
	}
	break;
	case VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO:
	{
#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
		DPRINTK("%s [+]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n", DEV_NAME);

		if (s_video_memory.base == 0)
			return -EFAULT;

		ret = copy_to_user((void __user *)arg, &s_video_memory, sizeof(vpudrv_buffer_t));
		if (ret)
			return -EFAULT;

		DPRINTK("%s [-]VDI_IOCTL_GET_RESERVED_VIDEO_MEMORY_INFO\n", DEV_NAME);
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */
	}
	break;
	case VDI_IOCTL_WAIT_INTERRUPT:
	{
		vpudrv_intr_info_t info;

		DPRINTK("[VPUDRV][+]VDI_IOCTL_WAIT_INTERRUPT\n");

		ret = copy_from_user(&info, (vpudrv_intr_info_t *)arg,
				     sizeof(vpudrv_intr_info_t));
		if (ret)
			return -EFAULT;

		ret = rtk_ve1_wait_init(dev, &info);
		if (ret != 0)
			return -EFAULT;

		ret = copy_to_user((void __user *)arg, &info,
				   sizeof(vpudrv_intr_info_t));
		if (ret)
			return -EFAULT;

		DPRINTK("[VPUDRV][-]VDI_IOCTL_WAIT_INTERRUPT, info.intr_reason:0x%x\n", info.intr_reason);
	}
	break;

	case VDI_IOCTL_SET_CLOCK_GATE:
	{
		u32 clkgate;

		//DPRINTK("[VPUDRV][+]VDI_IOCTL_SET_CLOCK_GATE\n");
		if (get_user(clkgate, (u32 __user *) arg))
			return -EFAULT;

#ifdef VPU_SUPPORT_CLOCK_CONTROL
		return -EFAULT;
#endif /* VPU_SUPPORT_CLOCK_CONTROL */
		//DPRINTK("[VPUDRV][-]VDI_IOCTL_SET_CLOCK_GATE\n");
	}
	break;
	case VDI_IOCTL_GET_INSTANCE_POOL:
	{
		DPRINTK("%s [+]VDI_IOCTL_GET_INSTANCE_POOL\n", DEV_NAME);

		ret = rtk_ve1_down_interruptible();
		if (ret != 0)
			return ret;

		if (s_instance_pool.base != 0) {
			ret = copy_to_user((void __user *)arg, &s_instance_pool,
							   sizeof(vpudrv_buffer_t));
			if (ret) {
				rtk_ve1_sem_up();
				return -EFAULT;
			}
		} else {
			ret = copy_from_user(&s_instance_pool,
					     (vpudrv_buffer_t *)arg,
					     sizeof(vpudrv_buffer_t));
			if (ret) {
				rtk_ve1_sem_up();
				return -EFAULT;
			}
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
			ret = rtk_ve1_alloc_from_vm();
			if (ret) {
				rtk_ve1_sem_up();
				return -EFAULT;
			}
#else
			ret = rtk_ve1_alloc_from_dmabuffer2();
			if (ret) {
				rtk_ve1_sem_up();
				return -EFAULT;
			}
#endif /* USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY */
			memset((void *)s_instance_pool.base, 0x0, s_instance_pool.size); /*clearing memory*/
			ret = copy_to_user((void __user *)arg, &s_instance_pool, sizeof(vpudrv_buffer_t));
			if (ret) {
				rtk_ve1_sem_up();
				return -EFAULT;
			}
		}

		rtk_ve1_sem_up();

		DPRINTK("%s [-]VDI_IOCTL_GET_INSTANCE_POOL\n", DEV_NAME);
	}
	break;
	case VDI_IOCTL_GET_COMMON_MEMORY:
	{
		DPRINTK("%s [+]VDI_IOCTL_GET_COMMON_MEMORY\n", DEV_NAME);

		/* s_common_memory is shared state; serialize like GET_INSTANCE_POOL */
		ret = rtk_ve1_down_interruptible();
		if (ret != 0)
			return ret;

		if (s_common_memory.base != 0) {
			ret = copy_to_user((void __user *)arg, &s_common_memory,
					   sizeof(vpudrv_buffer_t));
			if (ret != 0) {
				rtk_ve1_sem_up();
				return -EFAULT;
			}
		} else {
			ret = copy_from_user(&s_common_memory,
					     (vpudrv_buffer_t *)arg,
					     sizeof(vpudrv_buffer_t));
			if (ret) {
				rtk_ve1_sem_up();
				return -EFAULT;
			}

			ret = rtk_ve1_alloc_dma_buffer(&s_common_memory);
			if (ret != 0) {
				rtk_ve1_sem_up();
				return -ENOMEM;
			}

			ret = copy_to_user((void __user *)arg, &s_common_memory,
					   sizeof(vpudrv_buffer_t));
			if (ret) {
				rtk_ve1_sem_up();
				return -EFAULT;
			}
		}

		rtk_ve1_sem_up();

		DPRINTK("%s [-]VDI_IOCTL_GET_COMMON_MEMORY\n", DEV_NAME);
	}
	break;
	case VDI_IOCTL_OPEN_INSTANCE:
	{
		vpudrv_inst_info_t inst_info;

		ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
				     sizeof(vpudrv_inst_info_t));
		if (ret)
			return -EFAULT;

		ret = rtk_ve1_open_inst(&inst_info, filp);
		if (ret)
			return -ENOMEM;

		ret = copy_to_user((void __user *)arg, &inst_info,
				   sizeof(vpudrv_inst_info_t));
		if (ret) {
			/* roll back the instance opened above: user space never
			 * sees the success, so leaving it on s_inst_list_head
			 * (with s_vpu_open_ref_count bumped) would desync caller
			 * state until release.  close_inst undoes both. */
			rtk_ve1_close_inst(&inst_info);
			return -EFAULT;
		}

		DPRINTK("%s VDI_IOCTL_OPEN_INSTANCE core_idx=%d, inst_idx=%d, s_vpu_open_ref_count=%d, inst_open_count=%d\n", DEV_NAME, (int)inst_info.core_idx, (int)inst_info.inst_idx, s_vpu_open_ref_count, inst_info.inst_open_count);
	}
	break;
	case VDI_IOCTL_CLOSE_INSTANCE:
	{
		vpudrv_inst_info_t inst_info;

		DPRINTK("%s [+]VDI_IOCTL_CLOSE_INSTANCE\n", DEV_NAME);

		ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
				     sizeof(vpudrv_inst_info_t));
		if (ret)
			return -EFAULT;

		rtk_ve1_close_inst(&inst_info);

		ret = copy_to_user((void __user *)arg, &inst_info,
						   sizeof(vpudrv_inst_info_t));
		if (ret)
			return -EFAULT;

		DPRINTK("%s VDI_IOCTL_CLOSE_INSTANCE core_idx=%d, inst_idx=%d, s_vpu_open_ref_count=%d, inst_open_count=%d\n", DEV_NAME, (int)inst_info.core_idx, (int)inst_info.inst_idx, s_vpu_open_ref_count, inst_info.inst_open_count);
	}
	break;
	case VDI_IOCTL_GET_INSTANCE_NUM:
	{
		vpudrv_inst_info_t inst_info;

		DPRINTK("%s [+]VDI_IOCTL_GET_INSTANCE_NUM\n", DEV_NAME);

		ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
				     sizeof(vpudrv_inst_info_t));
		if (ret)
			return -EFAULT;

		rtk_ve1_get_inst_num(&inst_info);


		ret = copy_to_user((void __user *)arg, &inst_info,
				   sizeof(vpudrv_inst_info_t));
		if (ret)
			return -EFAULT;

		DPRINTK("%s VDI_IOCTL_GET_INSTANCE_NUM core_idx=%d, inst_idx=%d, open_count=%d\n", DEV_NAME, (int)inst_info.core_idx, (int)inst_info.inst_idx, inst_info.inst_open_count);
	}
	break;
	case VDI_IOCTL_RESET:
	{
		u32 coreIdx;

		if (get_user(coreIdx, (u32 __user *) arg))
			return -EFAULT;

		vpu_hw_reset(coreIdx);
	}
	break;
	case VDI_IOCTL_GET_REGISTER_INFO:
	{
		DPRINTK("%s [+]VDI_IOCTL_GET_REGISTER_INFO\n", DEV_NAME);

		ret = copy_to_user((void __user *)arg, &s_vpu_register,
				   sizeof(vpudrv_buffer_t));
		if (ret)
			return -EFAULT;

		DPRINTK("%s [-]VDI_IOCTL_GET_REGISTER_INFO s_vpu_register.phys_addr=0x%lx, s_vpu_register.virt_addr=0x%lx, s_vpu_register.size=%d\n", DEV_NAME, s_vpu_register.phys_addr, s_vpu_register.virt_addr, s_vpu_register.size);
	}
	break;
	/* RTK ioctl */
	case VDI_IOCTL_SET_RTK_CLK_GATING:
	{
		vpu_clock_info_t clockInfo;

		DPRINTK("%s [+]VDI_IOCTL_SET_RTK_CLK_GATING\n", DEV_NAME);

		ret = copy_from_user(&clockInfo, (vpu_clock_info_t *)arg,
				     sizeof(vpu_clock_info_t));
		if (ret)
			return -EFAULT;

		rtk_ve1_clock_getting(&clockInfo);

		DPRINTK("%s [-]VDI_IOCTL_SET_RTK_CLK_GATING clockInfo.core_idx:%d, clockInfo.enable:%d\n", DEV_NAME, clockInfo.core_idx, clockInfo.enable);
	}
	break;
	case VDI_IOCTL_SET_RTK_CLK_PLL:
	{
		return -ENOIOCTLCMD;
	}
	break;
	case VDI_IOCTL_GET_RTK_CLK_PLL:
	{
		return -ENOIOCTLCMD;
	}
	break;
	case VDI_IOCTL_SET_RTK_CLK_SELECT:
	{
		return -ENOIOCTLCMD;
	}
	break;
	case VDI_IOCTL_GET_RTK_CLK_SELECT:
	{
		return -ENOIOCTLCMD;
	}
	break;
	case VDI_IOCTL_GET_RTK_SUPPORT_TYPE:
	{
		if (g_chip_type == CHIP_TYPE_RTD16XXB) {
			ret = copy_to_user((void __user *)arg, &s_bond_register, sizeof(vpudrv_buffer_t));
			if (ret != 0)
				ret = -EFAULT;
		} else {
			ret = -ENOIOCTLCMD;
		}
	}
	break;
	case VDI_IOCTL_GET_RTK_DCSYS_INFO:
	{
		if (g_chip_type == CHIP_TYPE_RTD16XXB) {
			vpudrv_buffer_t vb;
			ret = copy_from_user(&vb, (vpudrv_buffer_t *)arg, sizeof(vpudrv_buffer_t));
			if (vb.mem_type == 0)
				ret = copy_to_user((void __user *)arg, &s_dc_register, sizeof(vpudrv_buffer_t));
			else
				ret = copy_to_user((void __user *)arg, &s_dmc_register, sizeof(vpudrv_buffer_t));
			if (ret != 0)
				ret = -EFAULT;
		} else {
			ret = -ENOIOCTLCMD;
		}
	}
	break;
	case VDI_IOCTL_GET_RTK_ASIC_REVISION:
		return -ENOIOCTLCMD;
	case VDI_IOCTL_GET_TOTAL_INSTANCE_NUM:
	{
		vpudrv_inst_info_t inst_info;

		DPRINTK("%s [+]VDI_IOCTL_GET_TOTAL_INSTANCE_NUM\n", DEV_NAME);

		ret = copy_from_user(&inst_info, (vpudrv_inst_info_t *)arg,
				     sizeof(vpudrv_inst_info_t));
		if (ret)
			return -EFAULT;
		rtk_ve1_get_total_inst_num(&inst_info);

		ret = copy_to_user((void __user *)arg, &inst_info,
						   sizeof(vpudrv_inst_info_t));
		if (ret)
			return -EFAULT;

		DPRINTK("%s VDI_IOCTL_GET_TOTAL_INSTANCE_NUM core_idx=%d, inst_idx=%d, open_count=%d\n", DEV_NAME, (int)inst_info.core_idx, (int)inst_info.inst_idx, inst_info.inst_open_count);
	}
	break;
	default:
	{
		pr_err("%s No such IOCTL, cmd is %d\n", DEV_NAME, cmd);
	}
	break;
	}

	return ret;
}

int rtk_ve1_ioctl_get_instance_pool(vpudrv_buffer_t *vdb)
{
	int ret = 0;
	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (vdb == NULL)
	{
		pr_err("%s [%d]%s.vdb == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	ret = rtk_ve1_down_interruptible();
	if (ret != 0)
		return ret;

	if (s_instance_pool.base != 0) {
		DPRINTK("%s [%d]%s.s_instance_pool(base:0x%lx,virt:0x%lx,phys:0x%lx,size:%d).\n",DEV_NAME,__LINE__,__func__,s_instance_pool.base,s_instance_pool.virt_addr,s_instance_pool.phys_addr,s_instance_pool.size);
		*vdb = s_instance_pool;
	} else {
		s_instance_pool = *vdb;

	#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
		ret = rtk_ve1_alloc_from_vm();
		if (ret) {
			rtk_ve1_sem_up();
			return -EFAULT;
		}
	#else
		ret = rtk_ve1_alloc_from_dmabuffer2();
		if (ret) {
			rtk_ve1_sem_up();
			return -EFAULT;
		}
	#endif /* USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY */
		DPRINTK("%s [%d]%s.s_instance_pool(base:0x%lx,virt:0x%lx,phys:0x%lx,size:%d).\n",DEV_NAME,__LINE__,__func__,s_instance_pool.base,s_instance_pool.virt_addr,s_instance_pool.phys_addr,s_instance_pool.size);
		memset((void *)s_instance_pool.base, 0x0, s_instance_pool.size); /*clearing memory*/
		*vdb = s_instance_pool;
	}

	rtk_ve1_sem_up();

	DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_get_instance_pool);

int rtk_ve1_ioctl_get_register_info(vpudrv_buffer_t *vdb)
{
	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (vdb == NULL)
	{
		pr_err("%s [%d]%s.vdb == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}
	*vdb = s_vpu_register;
	DPRINTK("%s [%d]%s.s_vpu_register(virt:0x%lx,phys:0x%lx,size:%d)\n",DEV_NAME,__LINE__,__func__,s_vpu_register.virt_addr,s_vpu_register.phys_addr,s_vpu_register.size);

	DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	return 0;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_get_register_info);

int rtk_ve1_ioctl_set_rtk_clk_gating(vpu_clock_info_t* clockInfo)
{
	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	if (clockInfo == NULL)
	{
		pr_err("%s [%d]%s.clockInfo == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	rtk_ve1_clock_getting(clockInfo);

	DPRINTK("%s [-] [%d]%s.core_idx:%d.enable:%d\n",DEV_NAME,__LINE__,__func__,clockInfo->core_idx,clockInfo->enable);
	return 0;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_set_rtk_clk_gating);

int rtk_ve1_ioctl_get_common_memory(vpudrv_buffer_t *vdb)
{
	int ret = 0;
	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (vdb == NULL)
	{
		pr_err("%s [%d]%s.vdb == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	/* s_common_memory is shared state; serialize like the GET_INSTANCE_POOL paths */
	ret = rtk_ve1_down_interruptible();
	if (ret != 0)
		return ret;

	if (s_common_memory.base != 0)
	{
		DPRINTK("%s [%d]%s.s_common_memory(base:0x%lx,virt:0x%lx,phys:0x%lx,size:%d)\n",DEV_NAME,__LINE__,__func__,s_common_memory.base,s_common_memory.virt_addr,s_common_memory.phys_addr,s_common_memory.size);
		*vdb = s_common_memory;
	} else {
		s_common_memory = *vdb;
		DPRINTK("%s [%d]%s.s_common_memory(base:0x%lx,virt:0x%lx,phys:0x%lx,size:%d)\n",DEV_NAME,__LINE__,__func__,s_common_memory.base,s_common_memory.virt_addr,s_common_memory.phys_addr,s_common_memory.size);
		if (rtk_ve1_alloc_dma_buffer(&s_common_memory) != -1)
		{
			DPRINTK("%s [%d]%s.s_common_memory(base:0x%lx,virt:0x%lx,phys:0x%lx,size:%d)\n",DEV_NAME,__LINE__,__func__,s_common_memory.base,s_common_memory.virt_addr,s_common_memory.phys_addr,s_common_memory.size);
			memset((void *)s_common_memory.virt_addr, 0x0, s_common_memory.size); /*clearing memory*/
			*vdb = s_common_memory;
			rtk_ve1_sem_up();
			return ret;
		}

		ret = -EFAULT;
	}
	rtk_ve1_sem_up();
	DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_get_common_memory);

ssize_t rtk_ve1_write_bit_firmware(vpu_bit_firmware_info_t *buf, size_t len)
{
	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (!buf) {
		pr_err("%s [%d]%s.buf == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	if (len == sizeof(vpu_bit_firmware_info_t) || len == sizeof(compat_vpu_bit_firmware_info_t))	{
		vpu_bit_firmware_info_t *bit_firmware_info;

		bit_firmware_info = kmalloc(sizeof(vpu_bit_firmware_info_t), GFP_KERNEL);
		if (!bit_firmware_info) {
			pr_err("%s [%d]%s.bit_firmware_info allocation error\n",DEV_NAME,__LINE__,__func__);
			return -EFAULT;
		}

		if (len == sizeof(vpu_bit_firmware_info_t)) {
			*bit_firmware_info = *buf;
		}

		if (bit_firmware_info->size == sizeof(vpu_bit_firmware_info_t)) {
			DPRINTK("%s [%d]%s.set bit_firmware_info coreIdx=0x%x, reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
					DEV_NAME,__LINE__,__func__,bit_firmware_info->core_idx,(int)bit_firmware_info->reg_base_offset,bit_firmware_info->size,bit_firmware_info->bit_code[0]);

			if (bit_firmware_info->core_idx > MAX_NUM_VPU_CORE) {
				pr_err("%s [%d]%s.coreIdx[%d] is exceeded than MAX_NUM_VPU_CORE[%d]\n",DEV_NAME,__LINE__,__func__,bit_firmware_info->core_idx,MAX_NUM_VPU_CORE);
				kfree(bit_firmware_info);
				return -ENODEV;
			}

			memcpy((void *)&s_bit_firmware_info[bit_firmware_info->core_idx], bit_firmware_info, sizeof(vpu_bit_firmware_info_t));
			kfree(bit_firmware_info);

			DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
			return len;
		}

		kfree(bit_firmware_info);
	}

	DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	return -1;
}
EXPORT_SYMBOL(rtk_ve1_write_bit_firmware);

int rtk_ve1_ioctl_allocate_physical_memory(void *filp, vpudrv_buffer_t *vdb)
{
	int ret = 0;
	vpudrv_buffer_pool_t *vbp;

	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (filp == NULL || vdb == NULL)
	{
		pr_err("%s [%d]%s.filp == NULL || vdb == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	ret = rtk_ve1_down_interruptible();
	if (ret != 0)
		return ret;

	vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
	if (!vbp) {
		rtk_ve1_sem_up();
		pr_err("%s [%d]%s.kzalloc() fail\n",DEV_NAME,__LINE__,__func__);
		return -ENOMEM;
	}

	vbp->vb = *vdb;

	ret = rtk_ve1_alloc_dma_buffer(&(vbp->vb));
	if (ret != 0) {
		kfree(vbp);
		rtk_ve1_sem_up();
		pr_err("%s [%d]%s.vpu_alloc_dma_buffer() fail\n",DEV_NAME,__LINE__,__func__);
		return -ENOMEM;
	}
	DPRINTK("%s [%d]vbp->vb(base:0x%lx,virt:0x%lx,phys:0x%lx,size:%d)\n",DEV_NAME,__LINE__,vbp->vb.base,vbp->vb.virt_addr,vbp->vb.phys_addr,vbp->vb.size);

	*vdb = vbp->vb;
	DPRINTK("%s [%d]%s.vdb(base:0x%lx,virt:0x%lx,phys:0x%lx,size:%d).filp:0x%px\n",DEV_NAME,__LINE__,__func__,vdb->base,vdb->virt_addr,vdb->phys_addr,vdb->size,filp);

	rtk_ve1_add_vbp_list(vbp, filp);
	rtk_ve1_sem_up();

	DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_allocate_physical_memory);

int rtk_ve1_ioctl_free_physical_memory(vpudrv_buffer_t *vdb)
{
	int ret = 0;

	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (vdb == NULL)
	{
		pr_err("%s [%d]%s.vdb == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	ret = rtk_ve1_down_interruptible();
	if (ret != 0)
		return ret;

	if (vdb->base)
	{
		rtk_ve1_free_dma_buffer(vdb);
		DPRINTK("%s [%d]%s.af vpu_free_dma_buffer\n",DEV_NAME,__LINE__,__func__);
	}

	rtk_ve1_free_mem(vdb);
	rtk_ve1_sem_up();

	DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_free_physical_memory);

int rtk_ve1_ioctl_allocate_physical_memory_no_mmap(void *filp, vpudrv_buffer_t *vdb)
{
	int ret = 0;
	vpudrv_buffer_pool_t *vbp;

	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (filp == NULL || vdb == NULL)
	{
		pr_err("%s [%d]%s.filp == NULL || vdb == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	ret = rtk_ve1_down_interruptible();
	if (ret != 0)
		return ret;

	vbp = kzalloc(sizeof(*vbp), GFP_KERNEL);
	if (!vbp) {
		rtk_ve1_sem_up();
		pr_err("%s [%d]%s.kzalloc() fail\n",DEV_NAME,__LINE__,__func__);
		return -ENOMEM;
	}

	vbp->vb = *vdb;

	ret = rtk_ve1_alloc_dma_buffer(&(vbp->vb));
	if (ret != 0) {
		kfree(vbp);
		rtk_ve1_sem_up();
		pr_err("%s [%d]%s.vpu_alloc_dma_buffer() fail\n",DEV_NAME,__LINE__,__func__);
		return -ENOMEM;
	}

	*vdb = vbp->vb;
	DPRINTK("%s [%d]%s.vdb(base:0x%lx,virt:0x%lx,phys:0x%lx,size:%d).filp:0x%px\n",DEV_NAME,__LINE__,__func__,vdb->base,vdb->virt_addr,vdb->phys_addr,vdb->size,filp);

	rtk_ve1_add_vbp_list(vbp, filp);
	rtk_ve1_sem_up();

	DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_allocate_physical_memory_no_mmap);

int rtk_ve1_ioctl_free_physical_memory_no_mmap(vpudrv_buffer_t *vdb)
{
	int ret = 0;

	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (vdb == NULL)
	{
		pr_err("%s [%d]%s.vdb == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	ret = rtk_ve1_down_interruptible();
	if (ret != 0)
		return ret;

	if (vdb->base)
	{
		rtk_ve1_free_dma_buffer(vdb);
		DPRINTK("%s [%d]%s.af vpu_free_dma_buffer\n",DEV_NAME,__LINE__,__func__);
	}

	rtk_ve1_free_mem(vdb);
	rtk_ve1_sem_up();

	DPRINTK("%s [-] [%d]%s\n",DEV_NAME,__LINE__,__func__);
	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_free_physical_memory_no_mmap);

int rtk_ve1_ioctl_open_instance(void *filp, vpudrv_inst_info_t *inst_info)
{
	int ret = 0;

	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (filp == NULL || inst_info == NULL)
	{
		pr_err("%s [%d]%s.filp == NULL || inst_info == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	ret = rtk_ve1_open_inst(inst_info, filp);
	if (ret)
		return -ENOMEM;

	DPRINTK("%s [-] [%d]%s.core_idx=%d.inst_idx=%d.s_vpu_open_ref_count=%d.inst_open_count=%d\n",DEV_NAME,__LINE__,__func__,inst_info->core_idx,inst_info->inst_idx,s_vpu_open_ref_count,inst_info->inst_open_count);
	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_open_instance);

int rtk_ve1_ioctl_close_instance(vpudrv_inst_info_t *inst_info)
{
	int ret = 0;

	DPRINTK("%s [+] [%d]%s\n",DEV_NAME,__LINE__,__func__);

	if (inst_info == NULL)
	{
		pr_err("%s [%d]%s.inst_info == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	rtk_ve1_close_inst(inst_info);

	DPRINTK("%s [-] [%d]%s.core_idx=%d.inst_idx=%d.s_vpu_open_ref_count=%d.inst_open_count=%d\n",DEV_NAME,__LINE__,__func__,inst_info->core_idx,inst_info->inst_idx,s_vpu_open_ref_count,inst_info->inst_open_count);
	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_close_instance);

int rtk_ve1_ioctl_wait_interrupt(vpudrv_intr_info_t *intr_info)
{
	int ret = 0;
	struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)(&s_vpu_drv_context);

	if (intr_info == NULL)
	{
		pr_err("%s [%d]%s.inst_info == NULL\n",DEV_NAME,__LINE__,__func__);
		return -EFAULT;
	}

	ret = rtk_ve1_wait_init(dev, intr_info);
	if (ret != 0)
		return -EFAULT;

	return ret;
}
EXPORT_SYMBOL(rtk_ve1_ioctl_wait_interrupt);

static ssize_t vpu_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	return -1;
}

#ifdef CONFIG_COMPAT
static int get_from_compat_vpu_bit_firmware_info(const char __user *buf,
	vpu_bit_firmware_info_t *info)
{
	compat_vpu_bit_firmware_info_t *compat_info;

	compat_info = kzalloc(sizeof(*compat_info), GFP_KERNEL);
	if (!compat_info)
		return -ENOMEM;

	if (copy_from_user(compat_info, buf, sizeof(*compat_info))) {
		kfree(compat_info);
		return -EFAULT;
	}

	info->size = sizeof(*info);
	info->core_idx = compat_info->core_idx;
	info->reg_base_offset = compat_info->reg_base_offset;
	memcpy(info->bit_code, compat_info->bit_code, sizeof(compat_info->bit_code));

	kfree(compat_info);
	return 0;
}
#endif



static ssize_t vpu_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{

	/* DPRINTK("[VPUDRV] vpu_write len=%d\n", (int)len); */
	if (!buf) {
		pr_err("%s vpu_write buf = NULL error \n", DEV_NAME);
		return -EFAULT;
	}

	if (len == sizeof(vpu_bit_firmware_info_t) || len == sizeof(compat_vpu_bit_firmware_info_t))	{
		vpu_bit_firmware_info_t *bit_firmware_info;

		bit_firmware_info = kmalloc(sizeof(vpu_bit_firmware_info_t), GFP_KERNEL);
		if (!bit_firmware_info) {
			pr_err("%s vpu_write  bit_firmware_info allocation error\n", DEV_NAME);
			return -EFAULT;
		}

		if (len == sizeof(vpu_bit_firmware_info_t)) {
			if (copy_from_user(bit_firmware_info, buf, len)) {
				pr_err("%s vpu_write copy_from_user error for bit_firmware_info\n", DEV_NAME);
				kfree(bit_firmware_info);
				return -EFAULT;
			}
		}
#ifdef CONFIG_COMPAT
		 else {
			int err = get_from_compat_vpu_bit_firmware_info(buf, bit_firmware_info);

			if (err) {
				pr_err("%s get_from_compat_vpu_bit_firmware_info return %d\n", DEV_NAME, err);
				kfree(bit_firmware_info);
				return -EFAULT;
			}
		}
#endif /* CONFIG_COMPAT */

		if (bit_firmware_info->size == sizeof(vpu_bit_firmware_info_t)) {
			DPRINTK("%s vpu_write set bit_firmware_info coreIdx=0x%x, reg_base_offset=0x%x size=0x%x, bit_code[0]=0x%x\n",
					DEV_NAME, bit_firmware_info->core_idx, (int)bit_firmware_info->reg_base_offset, bit_firmware_info->size, bit_firmware_info->bit_code[0]);

			if (bit_firmware_info->core_idx >= MAX_NUM_VPU_CORE) {
				pr_err("%s vpu_write coreIdx[%u] is exceeded than MAX_NUM_VPU_CORE[%d]\n", DEV_NAME, bit_firmware_info->core_idx, MAX_NUM_VPU_CORE);
				kfree(bit_firmware_info);
				return -ENODEV;
			}

			memcpy((void *)&s_bit_firmware_info[bit_firmware_info->core_idx], bit_firmware_info, sizeof(vpu_bit_firmware_info_t));
			kfree(bit_firmware_info);

			return len;
		}

		kfree(bit_firmware_info);
	}

	return -EINVAL;
}

static int vpu_release(struct inode *inode, struct file *filp)
{
	DPRINTK("%s vpu_release\n", DEV_NAME);

	/*
	 * Use down() (uninterruptible), NOT down_interruptible(): the VFS does
	 * not retry a failed ->release(), so bailing out on a signal would skip
	 * the buffer/instance teardown and the open_count decrement below and
	 * leak those resources permanently. The teardown must always run.
	 */
	down(&s_vpu_sem);

	/* found and free the not handled buffer by user applications */
	vpu_free_buffers(filp);
	/* found and free the not closed instance by user applications */
	rtk_ve1_free_instances(filp);
	s_vpu_drv_context.open_count--;
	if (s_vpu_drv_context.open_count == 0) {
		if (s_instance_pool.base) {
			DPRINTK("%s free instance pool\n", DEV_NAME);
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
			vfree((const void *)s_instance_pool.base);
#else
			rtk_ve1_free_dma_buffer_base(&s_instance_pool);
#endif /* USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY */
			s_instance_pool.base = 0;
		}
	}
	up(&s_vpu_sem);

	return 0;
}

static int vpu_fasync(int fd, struct file *filp, int mode)
{
	struct vpu_drv_context_t *dev = (struct vpu_drv_context_t *)filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int vpu_map_to_register(struct file *fp, struct vm_area_struct *vm)
{
	unsigned long pfn;

	vm_flags_set(vm, VM_IO | VM_RESERVED);
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	pfn = vm->vm_pgoff;

	return remap_pfn_range(vm, vm->vm_start, pfn, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_physical_memory(struct file *fp, struct vm_area_struct *vm)
{
	vm_flags_set(vm, VM_IO | VM_RESERVED);
	vm->vm_page_prot = pgprot_writecombine(vm->vm_page_prot);

	return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
}

static int vpu_map_to_instance_pool_memory(struct file *fp, struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
	int ret;
	long length = vm->vm_end - vm->vm_start;
	unsigned long start = vm->vm_start;
	char *vmalloc_area_ptr = (char *)s_instance_pool.base;
	unsigned long pfn;

	vm_flags_set(vm, VM_RESERVED);

	/* loop over all pages, map it page individually */
	while (length > 0) {
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		ret = remap_pfn_range(vm, start, pfn, PAGE_SIZE, PAGE_SHARED);
		if (ret < 0) {
			return ret;
		}
		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}

	return 0;
#else
	vm_flags_set(vm, VM_RESERVED);
	vm->vm_page_prot = pgprot_noncached(vm->vm_page_prot);
	return remap_pfn_range(vm, vm->vm_start, vm->vm_pgoff, vm->vm_end-vm->vm_start, vm->vm_page_prot) ? -EAGAIN : 0;
#endif /* USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY */
}

/*!
 * @brief memory map interface for vpu file operation
 * @return  0 on success or negative error code on error
 */
static int vpu_mmap(struct file *fp, struct vm_area_struct *vm)
{
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
	if (vm->vm_pgoff == 0)
		return vpu_map_to_instance_pool_memory(fp, vm);

	if (vm->vm_pgoff == (s_vpu_register.phys_addr>>PAGE_SHIFT))
		return vpu_map_to_register(fp, vm);

	if (g_chip_type == CHIP_TYPE_RTD16XXB) {
		if ((vm->vm_pgoff == (s_bond_register.phys_addr>>PAGE_SHIFT))
			|| (vm->vm_pgoff == (s_dc_register.phys_addr>>PAGE_SHIFT))
			|| (vm->vm_pgoff == (s_dmc_register.phys_addr>>PAGE_SHIFT))) {
			return vpu_map_to_register(fp, vm);
		}
	}

	return vpu_map_to_physical_memory(fp, vm);
#else
	if (vm->vm_pgoff) {
		if (vm->vm_pgoff == (s_instance_pool.phys_addr>>PAGE_SHIFT))
			return vpu_map_to_instance_pool_memory(fp, vm);

		if (g_chip_type == CHIP_TYPE_RTD16XXB) {
			if ((vm->vm_pgoff == (s_bond_register.phys_addr>>PAGE_SHIFT))
			|| (vm->vm_pgoff == (s_dc_register.phys_addr>>PAGE_SHIFT))
			|| (vm->vm_pgoff == (s_dmc_register.phys_addr>>PAGE_SHIFT)))
				return vpu_map_to_register(fp, vm);
		}

		return vpu_map_to_physical_memory(fp, vm);
	} else {
		return vpu_map_to_register(fp, vm);
	}
#endif /* USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY */
}

static struct file_operations vpu_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.read = vpu_read,
	.write = vpu_write,
	/*.ioctl = vpu_ioctl, // for kernel 2.6.9*/
	.unlocked_ioctl = vpu_ioctl,
	.compat_ioctl = compat_vpu_ioctl,
	.release = vpu_release,
	.fasync = vpu_fasync,
	.mmap = vpu_mmap,
};

static int vpu_pcpu_ipc_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct resource res;
	int ret = 0;
	int val;

	if (g_chip_type < CHIP_TYPE_KENT) {
		return ret;
	}

	ret = of_address_to_resource(node, 0, &res);
	if (ret) {
		pr_err("%s %d.%s.failed to get resource\n", DEV_NAME, __LINE__, __func__);
		return ret;
	}
	//pr_info("%s %d.%s.dev:0x%px.node:0x%px.res.start:0x%lx\n", DEV_NAME, __LINE__, __func__, dev, node, res.start);
	intr_regmap = syscon_regmap_lookup_by_phandle(node, "intr-syscon");
	if (IS_ERR_OR_NULL(intr_regmap)) {
		dev_err(dev, "%s %d.%s.cannot get intr regmap\n", DEV_NAME, __LINE__, __func__);
		return -EINVAL;
	}
	//pr_info("%s %d.%s.intr_regmap:0x%px\n", DEV_NAME, __LINE__, __func__, intr_regmap);
	ipc_regmap = syscon_regmap_lookup_by_phandle(node, "ipc-syscon");
	if (IS_ERR_OR_NULL(ipc_regmap)) {
		dev_err(dev, "%s %d.%s.cannot get intr regmap\n", DEV_NAME, __LINE__, __func__);
		return -EINVAL;
	}
	//pr_info("%s %d.%s.ipc_regmap:0x%px\n", DEV_NAME, __LINE__, __func__, ipc_regmap);
	regmap_read(intr_regmap, INTR_EN_OFFSET, &val);
	//pr_info("%s %d.%s.TO_PCPU_INTR_BIT:0x%x.val:0x%x\n", DEV_NAME, __LINE__, __func__,
	//	TO_PCPU_INTR_BIT, val);
	if (!(val & TO_PCPU_INTR_BIT)) {
		//pr_info("%s %d.%s.regmap_write INTR_EN_OFFSET:0x%x.val:0x%x\n", DEV_NAME, __LINE__, __func__,
		//	INTR_EN_OFFSET, (TO_PCPU_INTR_BIT | WRITE_DATA));
		regmap_write(intr_regmap, INTR_EN_OFFSET,
		    TO_PCPU_INTR_BIT | WRITE_DATA);
	}

	return ret;
}

static int vpu_pcpu_ipc_vetop_sram_on(struct device *dev)
{
	int ret = 0;
	int val;
	int val1;
	int val2;
	u32 parity;
	u32 cmd;

	if (g_chip_type < CHIP_TYPE_KENT) {
		return ret;
	}

	parity = IPC_CATE_PPC ^ (IPC_PPC_VETOP_ON & GENMASK(7, 0)) ^ ((IPC_PPC_VETOP_ON >> 8) & GENMASK(7, 0)) ^ IPC_CMD_BLOCKING;
	cmd = (IPC_CMD_BLOCKING << 31) | (IPC_CATE_PPC << 24) | (parity << 16) | IPC_PPC_VETOP_ON;
	//pr_info("%s %d.%s.cmd:0x%x.opcode:0x%x.parity:0x%x\n", DEV_NAME, __LINE__, __func__, cmd, IPC_PPC_VETOP_ON, parity);
	regmap_write(ipc_regmap, TO_PCPU_IPC_REGOFF, cmd);
	regmap_write(ipc_regmap, TO_PCPU_IPC_REGOFF + 0x4, 0);
	regmap_write(intr_regmap, INTR_OFFSET, TO_PCPU_INTR_BIT | WRITE_DATA);
	ret = regmap_read_poll_timeout(intr_regmap, INTR_OFFSET, val,
			!(val & TO_PCPU_INTR_BIT), POLLING_TIME, 1000 * POLLING_TIME);
	if (ret) {
		dev_err(dev, "%d.%s.send pcpu interrupt timeout\n", __LINE__, __func__);
		return ret;
	}
	// pcpu interrupt blocking
	ret = regmap_read_poll_timeout(ipc_regmap, TO_PCPU_IPC_REGOFF + 0x4, val,
			val == 0x1, POLLING_TIME, 1000 * POLLING_TIME);
	if (ret) {
		regmap_read(intr_regmap, INTR_OFFSET, &val);
		regmap_read(ipc_regmap, TO_PCPU_IPC_REGOFF, &val1);
		regmap_read(ipc_regmap, TO_PCPU_IPC_REGOFF + 0x4, &val2);
		dev_err(dev,
			"%d.%s.send pcpu ipc timeout(intr:0x%x cmd_reg:0x%x cmd_arg:0x%x)\n",
			__LINE__, __func__, val, val1, val2);
	}
	regmap_write(ipc_regmap, TO_PCPU_IPC_REGOFF, 0);
	regmap_write(ipc_regmap, TO_PCPU_IPC_REGOFF + 0x4, 0);

	return ret;
}

static struct clk *rtk_ve1_clk;
static struct devfreq_dev_profile rtk_ve1_defreq_profile;
static struct devfreq *rtk_ve1_devfreq = NULL;
struct thermal_cooling_device *rtk_ve1_tcd = NULL;

static int vpu_target(struct device *dev, unsigned long *freq, u32 flags)
{
	unsigned long cuf_freq = clk_get_rate(rtk_ve1_clk);
	struct dev_pm_opp *opp;

	opp = devfreq_recommended_opp(dev, freq, 0);
	if (IS_ERR(opp)) {
		dev_err(dev, "failed to find recommended opp\n");
		return PTR_ERR(opp);
	}
	dev_pm_opp_put(opp);

	if (cuf_freq == *freq)
		return 0;
	return clk_set_rate(rtk_ve1_clk, *freq);
}

static int vpu_get_cur_freq(struct device *dev, unsigned long *freq)
{
	*freq = clk_get_rate(rtk_ve1_clk);
	return 0;
}

static inline bool vpu_is_busy(void)
{
	int core;
	unsigned int busy = 0;

	for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
		if (s_bit_firmware_info[core].size == 0)
			continue;

		busy |= ReadVpuRegister(BIT_BUSY_FLAG, core);
	}

	return !!(busy);
}

static u32 vpu_lookup_power_mW(unsigned long freq, int busy)
{
	int i;

	if (!s_vpu_drv_context.tbl || s_vpu_drv_context.tbl_cnt == 0)
		return 0;

	for (i = 0; i < s_vpu_drv_context.tbl_cnt; i++)
		if (freq <= s_vpu_drv_context.tbl[i].freq)
			return busy ? s_vpu_drv_context.tbl[i].mW_decode : s_vpu_drv_context.tbl[i].mW_idle;

	return busy ? s_vpu_drv_context.tbl[s_vpu_drv_context.tbl_cnt - 1].mW_decode
			: s_vpu_drv_context.tbl[s_vpu_drv_context.tbl_cnt - 1].mW_idle;
}

static int vpu_get_real_power(struct devfreq *df, u32 *power,
				unsigned long freq,
				unsigned long voltage)
{
	unsigned long idle_time_ms;
	int busy = 0;
	int ret = 0;

	if (!s_vpu_drv_context.tbl_cnt) {
		ret = -EINVAL;
		*power = 0;
		goto exit;
	}

	if (s_vpu_drv_context.last_busy_jiffies)
		idle_time_ms = jiffies_to_msecs(jiffies - s_vpu_drv_context.last_busy_jiffies);

	if (s_vpu_drv_context.last_busy_jiffies &&
		idle_time_ms < DECODER_ACTIVE_THRESHOLD_MS) {
		busy = 1;
	} else if (s_vpu_drv_context.is_decoding_active) {
		if (vpu_is_busy()){
			busy = 1;
			pr_info("VPU is busy but can't decode any frames\n");
		}
	}

	*power = vpu_lookup_power_mW(freq, busy);
exit:
	return ret;
}

static struct devfreq_cooling_power vpu_power_ops = {
	.get_real_power = &vpu_get_real_power,
};

static int vpu_parse_opp_power_tables(struct device *dev)
{
	unsigned long freq = 0;
	int count = 0;
	int ret = 0;
	int i = 0;

	count = dev_pm_opp_get_opp_count(dev);
	if (count <= 0) {
		ret = -ENOENT;
		dev_err(dev, "failed to get OPP count: %d\n", ret);
		goto exit;
	}

	s_vpu_drv_context.tbl =
			devm_kcalloc(dev, count, sizeof(*s_vpu_drv_context.tbl), GFP_KERNEL);
	if (!s_vpu_drv_context.tbl) {
		ret = -ENOMEM;
		dev_err(dev, "failed to malloc OPP table: %d\n", ret);
		goto exit;
	}

	for (;;) {
		struct dev_pm_opp *opp;
		struct device_node *opp_table_np = NULL;
		struct device_node *child_np = NULL;
		u32 mW_idle = 0, mW_decode = 0;
		int r;

		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR(opp))
			break;

		dev_pm_opp_put(opp);

		opp_table_np = of_parse_phandle(dev->of_node, "operating-points-v2", 0);
		if (opp_table_np) {
			for_each_child_of_node(opp_table_np, child_np) {
				u64 opp_rate_hz = 0;

				if (of_property_read_u64_index(child_np, "opp-hz", 0,
								&opp_rate_hz) == 0) {
					if (opp_rate_hz == (u64)freq) {
						r = of_property_read_u32(child_np,
							"opp-milliwatt-idle", &mW_idle);
						if (r)
							mW_idle = 0;

						r = of_property_read_u32(child_np,
							"opp-milliwatt-decode", &mW_decode);
						if (r)
							mW_decode = 0;

						of_node_put(child_np);
						break;
					}
				}
			}
			of_node_put(opp_table_np);
		}

		s_vpu_drv_context.tbl[i].freq = freq;
		s_vpu_drv_context.tbl[i].mW_idle = mW_idle;
		s_vpu_drv_context.tbl[i].mW_decode = mW_decode;
		i++;
		freq += 1;
	}

	s_vpu_drv_context.tbl_cnt = i;

exit:
	return ret;
}

static int vpu_init_devfreq(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct devfreq_dev_profile *profile = &rtk_ve1_defreq_profile;
	int ret = 0;

	if (!of_find_property(np, "operating-points-v2", NULL)) {
		dev_info(dev, "no operating-points-v2\n");
		goto exit;
	}

	rtk_ve1_clk = devm_clk_get(dev, NULL);
	if (IS_ERR(rtk_ve1_clk)) {
		dev_info(dev, "no clk for devfreq\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = devm_pm_opp_of_add_table(dev);
	if (ret < 0) {
		dev_info(dev, "failed to get OPP table: %d\n", ret);
		goto exit;
	}

	profile->get_cur_freq = vpu_get_cur_freq;
	profile->target       = vpu_target;
	profile->initial_freq = clk_get_rate(rtk_ve1_clk);
	rtk_ve1_devfreq = devm_devfreq_add_device(dev, profile, "performance", NULL);
	if (IS_ERR(rtk_ve1_devfreq)) {
		ret = PTR_ERR(rtk_ve1_devfreq);
		goto exit;
	}

	ret = dev_pm_opp_of_register_em(dev, NULL);
	if (ret) {
		dev_err(dev, "failed to register Energy Model: %d\n", ret);
		goto exit;
	}

	rtk_ve1_tcd = of_devfreq_cooling_register_power(np, rtk_ve1_devfreq, &vpu_power_ops);
	if (IS_ERR(rtk_ve1_tcd)) {
		ret = PTR_ERR(rtk_ve1_tcd);
		dev_err(dev, "failed to register cooling device: %d\n", ret);
		goto exit;
	}

	ret = vpu_parse_opp_power_tables(dev);
	if (ret)
		goto err_unregister_cooling;

	return ret;
err_unregister_cooling:
	devfreq_cooling_unregister(rtk_ve1_tcd);
exit:
	return ret;
}

static const struct of_device_id rtk_ve1_dt_match[] = {
	{ .compatible = "realtek,prince-ve1", .data = (void *)CHIP_TYPE_PRINCE },
	{ .compatible = "realtek,kent-ve1", .data = (void *)CHIP_TYPE_KENT },
	{ .compatible = "realtek,rtd13xxd-ve1", .data = (void *)CHIP_TYPE_RTD13XXD },
	{ .compatible = "realtek,rtd13xxe-ve1", .data = (void *)CHIP_TYPE_RTD13XXE },
	{ .compatible = "realtek,rtk16xxb-ve1", .data = (void *)CHIP_TYPE_RTD16XXB },
	{}
};
MODULE_DEVICE_TABLE(of, rtk_ve1_dt_match);

static int rtd16xxb_init_extra_registers(struct device_node *node)
{
	struct resource res;
	void __iomem *iobase;
	u32 bonding_value;
	unsigned long virt_addr;
	int ret = 0;

	/* DC register (index 1) */
	ret = of_address_to_resource(node, 1, &res);
	if (ret) {
		pr_err("%s %d.%s.failed to get dc resource\n", DEV_NAME, __LINE__, __func__);
		return ret;
	}
	iobase = of_iomap(node, 1);
	if (!iobase) {
		pr_err("%s %d.%s.failed to iomap dc register\n", DEV_NAME, __LINE__, __func__);
		return -ENOMEM;
	}
	s_dc_register.phys_addr = res.start;
	s_dc_register.virt_addr = (unsigned long)iobase;
	s_dc_register.size = res.end - res.start + 1;

	/* Bond register (index 2) - also read bonding value */
	ret = of_address_to_resource(node, 2, &res);
	if (ret) {
		pr_err("%s %d.%s.failed to get bond resource\n", DEV_NAME, __LINE__, __func__);
		goto cleanup_dc;
	}
	iobase = of_iomap(node, 2);
	if (!iobase) {
		pr_err("%s %d.%s.failed to iomap bond register\n", DEV_NAME, __LINE__, __func__);
		ret = -ENOMEM;
		goto cleanup_dc;
	}
	s_bond_register.phys_addr = res.start;
	s_bond_register.virt_addr = (unsigned long)iobase;
	s_bond_register.size = res.end - res.start + 1;

	virt_addr = (unsigned long)iobase;
	bonding_value = __raw_readl((void __iomem *)virt_addr);
	pr_info("%s %d.%s.bonding_value:0x%x\n", DEV_NAME, __LINE__, __func__, bonding_value);

	/* DMC register (index 3) */
	ret = of_address_to_resource(node, 3, &res);
	if (ret) {
		pr_err("%s %d.%s.failed to get dmc resource\n", DEV_NAME, __LINE__, __func__);
		goto cleanup_bond;
	}
	iobase = of_iomap(node, 3);
	if (!iobase) {
		pr_err("%s %d.%s.failed to iomap dmc register\n", DEV_NAME, __LINE__, __func__);
		ret = -ENOMEM;
		goto cleanup_bond;
	}
	s_dmc_register.phys_addr = res.start;
	s_dmc_register.virt_addr = (unsigned long)iobase;
	s_dmc_register.size = res.end - res.start + 1;

	return 0;

cleanup_bond:
	iounmap((void *)s_bond_register.virt_addr);
	s_bond_register.virt_addr = 0;
cleanup_dc:
	iounmap((void *)s_dc_register.virt_addr);
	s_dc_register.virt_addr = 0;
	return ret;
}

static void ve1_reset_control_get(struct device *dev)
{
	if (dev == NULL) {
		return;
	}

	if (g_chip_type == CHIP_TYPE_RTD13XXE || g_chip_type == CHIP_TYPE_RTD13XXD
		|| g_chip_type == CHIP_TYPE_RTD16XXB) {
		rstc_ve1 = devm_reset_control_get_exclusive(dev, "reset");
		if (IS_ERR(rstc_ve1)) {
			dev_warn(dev, "failed to get reset control ve1: %ld\n", PTR_ERR(rstc_ve1));
			rstc_ve1 = NULL;
		}
	}
	if (g_chip_type == CHIP_TYPE_RTD13XXE || g_chip_type == CHIP_TYPE_RTD13XXD) {
		rstc_ve1_mmu = devm_reset_control_get_exclusive(dev, "mmu");
		if (IS_ERR(rstc_ve1_mmu)) {
			dev_warn(dev, "failed to get reset control mmu: %ld\n", PTR_ERR(rstc_ve1_mmu));
			rstc_ve1_mmu = NULL;
		}

		rstc_ve1_mmu_func = devm_reset_control_get_exclusive(dev, "mmu_func");
		if (IS_ERR(rstc_ve1_mmu_func)) {
			dev_warn(dev, "failed to get reset control mmu_func: %ld\n", PTR_ERR(rstc_ve1_mmu_func));
			rstc_ve1_mmu_func = NULL;
		}

		rstc_iso_bist = devm_reset_control_get_exclusive(dev, "iso_bist");
		if (IS_ERR(rstc_iso_bist)) {
			dev_warn(dev, "failed to get reset control iso_bist: %ld\n", PTR_ERR(rstc_iso_bist));
			rstc_iso_bist = NULL;
		}
	}
}

static int vpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err = 0;
	struct resource res;
	void __iomem *iobase;
	int irq;
	struct device_node *node = pdev->dev.of_node;
	const struct of_device_id *match;

	pr_info("%s vpu_probe\n", DEV_NAME);

	/* Detect chip type from device tree compatible */
	match = of_match_node(rtk_ve1_dt_match, node);
	if (match && match->data) {
		g_chip_type = (enum rtk_ve1_chip_type)match->data;
		pr_info("%s %d.%s.chip type detected: %d\n", DEV_NAME, __LINE__, __func__, g_chip_type);
	} else {
		pr_err("%s %d.%s.chip type not matched, return -EINVAL\n", DEV_NAME, __LINE__, __func__);
		return -EINVAL;
	}

	err = of_address_to_resource(node, 0, &res);
	if (err) {
		pr_err("%s %d.%s.failed to get resource\n", DEV_NAME, __LINE__, __func__);
		return err;
	}
	iobase = of_iomap(node, 0);
	if (!iobase) {
		pr_err("%s %d.%s.failed to iomap\n", DEV_NAME, __LINE__, __func__);
		return -ENOMEM;
	}

	s_vpu_register.phys_addr = res.start;
	s_vpu_register.virt_addr = (unsigned long)iobase;
	s_vpu_register.size = res.end - res.start + 1;

	pr_info("%s vpu base address get from DTB physical base addr=0x%lx, virtual base=0x%lx, size=0x%x\n", DEV_NAME, s_vpu_register.phys_addr, s_vpu_register.virt_addr, s_vpu_register.size);

	if (g_chip_type == CHIP_TYPE_RTD16XXB) {
		err = rtd16xxb_init_extra_registers(node);
		if (err) {
			iounmap((void *)s_vpu_register.virt_addr);
			s_vpu_register.virt_addr = 0;
			return err;
		}
	}

	/* IRQ not acquired yet: keep guard reliable for early cleanup paths */
	s_ve1_irq = 0;

	s_vpu_dev.minor = MISC_DYNAMIC_MINOR;
	s_vpu_dev.name = VPU_DEV_NAME;
	s_vpu_dev.fops = &vpu_fops;
	s_vpu_dev.parent = NULL;
	err = misc_register(&s_vpu_dev);
	if (err) {
		pr_err("%s %d.%s.failed to register misc device\n", DEV_NAME, __LINE__, __func__);
		goto ERROR_PROVE_DEVICE;
	}

	s_vpu_dev.this_device->coherent_dma_mask = DMA_BIT_MASK(32);
	s_vpu_dev.this_device->dma_mask = (u64 *)&s_vpu_dev.this_device
						->coherent_dma_mask;
#if IS_ENABLED(CONFIG_DMABUF_HEAPS_REALTEK)
	set_dma_ops(s_vpu_dev.this_device, &rheap_dma_ops);
#endif

	p_vpu_dev = &pdev->dev;

	init_waitqueue_head(&s_interrupt_wait_q_ve1);
	err = kfifo_alloc(&s_interrupt_pending_q_ve1, MAX_INTERRUPT_QUEUE*sizeof(unsigned long), GFP_KERNEL);
	if (err) {
		pr_err("%s %d.%s.kfifo_alloc failed 0x%x\n", DEV_NAME, __LINE__, __func__, err);
		goto ERROR_PROVE_DEVICE;
	}
	s_common_memory.base = 0;
	s_instance_pool.base = 0;

	irq = irq_of_parse_and_map(node, 0);
	if (irq <= 0) {
		pr_err("%s %d.%s.failed to parse IRQ\n", DEV_NAME, __LINE__, __func__);
		err = -EINVAL;
		s_ve1_irq = 0;   /* IRQ not acquired: cleanup must not free_irq */
		goto ERROR_PROVE_DEVICE;
	}

	s_ve1_irq = irq;
	pr_info("%s s_ve1_irq:%d want to register ve1_irq_handler\n", DEV_NAME, s_ve1_irq);
	err = request_irq(s_ve1_irq, ve1_irq_handler, 0, "VE1_CODEC_IRQ", (void *)(&s_vpu_drv_context));
	if (err != 0) {
		if (err == -EINVAL)
			pr_err("%s %d.%s.Bad s_ve1_irq number or handler\n", DEV_NAME, __LINE__, __func__);
		else if (err == -EBUSY)
			pr_err("%s %d.%s.s_ve1_irq <%d> busy\n", DEV_NAME, __LINE__, __func__, s_ve1_irq);
		s_ve1_irq = 0;   /* request_irq failed: no handler registered to free later */
		goto ERROR_PROVE_DEVICE;
	}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	s_video_memory.size = VPU_INIT_VIDEO_MEMORY_SIZE_IN_BYTE;
	s_video_memory.phys_addr = VPU_DRAM_PHYSICAL_BASE;
	s_video_memory.base = (unsigned long)ioremap_nocache(s_video_memory.phys_addr, PAGE_ALIGN(s_video_memory.size));
	if (!s_video_memory.base) {
		pr_err("%s %d.%s.fail to remap video memory physical phys_addr=0x%x, base=0x%x, size=%d\n", DEV_NAME, __LINE__, __func__,
			(int)s_video_memory.phys_addr, (int)s_video_memory.base, (int)s_video_memory.size);
		err = -ENOMEM;
		goto ERROR_PROVE_DEVICE;
	}

	if (vmem_init(&s_vmem, s_video_memory.phys_addr, s_video_memory.size) < 0) {
		pr_err("%s %d.%s.fail to init vmem system\n", DEV_NAME, __LINE__, __func__);
		iounmap((void *)s_video_memory.base);
		s_video_memory.base = 0;
		err = -ENOMEM;
		goto ERROR_PROVE_DEVICE;
	}
	pr_info("%s success to probe vpu device with reserved video memory phys_addr=0x%x, base = 0x%x\n", DEV_NAME, (int) s_video_memory.phys_addr, (int)s_video_memory.base);
#else
	pr_info("%s success to probe vpu device with non reserved video memory\n", DEV_NAME);
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */

	err = vpu_pcpu_ipc_init(dev);
	if (err < 0) {
		goto ERROR_PROVE_DEVICE;
	}

	err = vpu_init_devfreq(dev);
	if (err) {
		dev_warn(dev, "failed to initialize devfreq: %d\n", err);
		err = 0;
	}

	if (rtk_ve1_devfreq)
		devfreq_suspend_device(rtk_ve1_devfreq);

	ve1_reset_control_get(dev);

	pm_runtime_set_suspended(dev);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, 15000);
	pm_runtime_enable(dev);
	pm_runtime_mark_last_busy(&pdev->dev);

	return 0;


ERROR_PROVE_DEVICE:

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (s_video_memory.base) {
		iounmap((void *)s_video_memory.base);
		s_video_memory.base = 0;
		vmem_exit(&s_vmem);
	}
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */

	if (s_ve1_irq) {
		free_irq(s_ve1_irq, &s_vpu_drv_context);
		s_ve1_irq = 0;
	}

	/* safe even on the misc_register-fail path: data is NULL, kfree(NULL) is a no-op */
	kfifo_free(&s_interrupt_pending_q_ve1);

	if (s_dmc_register.virt_addr) {
		iounmap((void *)s_dmc_register.virt_addr);
		s_dmc_register.virt_addr = 0;
	}
	if (s_bond_register.virt_addr) {
		iounmap((void *)s_bond_register.virt_addr);
		s_bond_register.virt_addr = 0;
	}
	if (s_dc_register.virt_addr) {
		iounmap((void *)s_dc_register.virt_addr);
		s_dc_register.virt_addr = 0;
	}
	if (s_vpu_register.virt_addr) {
		iounmap((void *)s_vpu_register.virt_addr);
		s_vpu_register.virt_addr = 0;
	}

	/*
	 * Only deregister if misc_register() actually succeeded: this_device is
	 * set on success and left NULL on failure. Reaching this label from the
	 * misc_register() failure goto must NOT call misc_deregister() -- the
	 * device was never registered and its list node is already poisoned.
	 */
	if (s_vpu_dev.this_device)
		misc_deregister(&s_vpu_dev);

	return err;
}

static int vpu_remove(struct platform_device *pdev)
{
	DPRINTK("%s vpu_remove\n", DEV_NAME);

	if (s_dmc_register.virt_addr) {
		iounmap((void *)s_dmc_register.virt_addr);
		s_dmc_register.virt_addr = 0;
	}
	if (s_bond_register.virt_addr) {
		iounmap((void *)s_bond_register.virt_addr);
		s_bond_register.virt_addr = 0;
	}
	if (s_dc_register.virt_addr) {
		iounmap((void *)s_dc_register.virt_addr);
		s_dc_register.virt_addr = 0;
	}
	if (s_vpu_register.virt_addr) {
		iounmap((void *)s_vpu_register.virt_addr);
		s_vpu_register.virt_addr = 0;
	}

	pm_runtime_disable(&pdev->dev);
	kfifo_free(&s_interrupt_pending_q_ve1);

	if (rtk_ve1_tcd)
		devfreq_cooling_unregister(rtk_ve1_tcd);

#ifdef VPU_SUPPORT_PLATFORM_DRIVER_REGISTER
	if (s_instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
		vfree((const void *)s_instance_pool.base);
#else
		rtk_ve1_free_dma_buffer_base(&s_instance_pool);
#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */
		s_instance_pool.base = 0;
	}

	if (s_common_memory.base) {
		rtk_ve1_free_dma_buffer_base(&s_common_memory);
		s_common_memory.base = 0;
	}

#ifdef VPU_SUPPORT_RESERVED_VIDEO_MEMORY
	if (s_video_memory.base) {
		iounmap((void *)s_video_memory.base);
		s_video_memory.base = 0;
		vmem_exit(&s_vmem);
	}
#endif /* VPU_SUPPORT_RESERVED_VIDEO_MEMORY */

	misc_deregister(&s_vpu_dev);

	if (s_ve1_irq)
		free_irq(s_ve1_irq, &s_vpu_drv_context);

#endif /* VPU_SUPPORT_PLATFORM_DRIVER_REGISTER */

	return 0;
}

static void vpu_shutdown(struct platform_device *pdev)
{
	pr_info("%s Enter %s\n", DEV_NAME, __func__);

	pm_runtime_force_suspend(&pdev->dev);
}

#ifdef CONFIG_PM
/* DO NOT CHANGE */

static int vpu_suspend(struct device *pdev)
{
	int ref_count;

	pr_info("%s %d.%s.enter\n",
		DEV_NAME, __LINE__, __func__);

	pm_runtime_get_sync(pdev);

	/* RTK wrapper */
	down(&s_vpu_sem);   /* serialize teardown with open/release and the sem-taking ioctls (open_count, instance_pool) */

	/* Instance open/close does NOT take s_vpu_sem (see locking model above):
	 * s_vpu_open_ref_count and s_inst_list_head are guarded by s_vpu_lock
	 * alone, so snapshot/iterate them under the spinlock like vpu_resume()
	 * does instead of reading them bare. */
	spin_lock(&s_vpu_lock);
	ref_count = s_vpu_open_ref_count;
	spin_unlock(&s_vpu_lock);
	if (ref_count > 0) {
#ifdef DISABLE_ORIGIN_SUSPEND
		vpudrv_instanace_list_t *vil, *n;
		vpudrv_instance_pool_t *vip;
		void *vip_base;
		int instance_pool_size_per_core;
		vpudrv_buffer_pool_t *pool, *nn;
		unsigned long flags;
		LIST_HEAD(vbp_to_free);

		/* s_instance_pool.size  assigned to the size of all core once call VDI_IOCTL_GET_INSTANCE_POOL by user. */
		instance_pool_size_per_core = (s_instance_pool.size/MAX_NUM_VPU_CORE);

		wake_up_interruptible_all(&s_interrupt_wait_q_ve1);
		spin_lock_irqsave(&s_intr_lock_ve1, flags);
		kfifo_reset(&s_interrupt_pending_q_ve1);
		atomic_set(&s_interrupt_flag_ve1, 0);
		spin_unlock_irqrestore(&s_intr_lock_ve1, flags);
		s_vpu_drv_context.open_count = 0;

		spin_lock(&s_vpu_lock);
		s_vpu_open_ref_count = 0;
		list_for_each_entry_safe(vil, n, &s_inst_list_head, list) {
			vip_base = (void *)(s_instance_pool.base + (instance_pool_size_per_core*vil->core_idx));
			vip = (vpudrv_instance_pool_t *)vip_base;
			if (vip) {
				/* only first 4 byte is key point(inUse of CodecInst in vpuapi) to free the corresponding instance. */
				memset(&vip->codecInstPool[vil->inst_idx], 0x00, 4);
			}
			list_del(&vil->list);
			kfree(vil);
		}
		spin_unlock(&s_vpu_lock);

		/* phase 1: detach the whole buffer list under the spinlock */
		spin_lock(&s_vpu_lock);
		list_splice_init(&s_vbp_head, &vbp_to_free);
		spin_unlock(&s_vpu_lock);

		/* phase 2: free outside the lock; base is the full kernel base */
		list_for_each_entry_safe(pool, nn, &vbp_to_free, list) {
			if (pool->vb.base)
				rtk_ve1_free_dma_buffer_base(&pool->vb);
			list_del(&pool->list);
			kfree(pool);
		}

		if (s_instance_pool.base) {
#ifdef USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY
			vfree((const void *)s_instance_pool.base);
#else
			rtk_ve1_free_dma_buffer_base(&s_instance_pool);
#endif /* USE_VMALLOC_FOR_INSTANCE_POOL_MEMORY */
			s_instance_pool.base = 0;
		}
#else /* else of DISABLE_ORIGIN_SUSPEND */
		int i;
		int core;
		unsigned long timeout = jiffies + HZ; /* vpu wait timeout to 1sec */
		int product_code = 0;

		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {
			if (s_bit_firmware_info[core].size == 0)
				continue;
			product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER, core);

			while (ReadVpuRegister(BIT_BUSY_FLAG, core)) {
				if (time_after(jiffies, timeout))
					goto DONE_SUSPEND;
			}

			for (i = 0; i < 64; i++)
				s_vpu_reg_store[core][i] = ReadVpuRegister(BIT_BASE+(0x100+(i * 4)), core);
		}
#endif /* end of DISABLE_ORIGIN_SUSPEND */
	}
	up(&s_vpu_sem);

	pm_runtime_force_suspend(pdev);

	pr_info("%s %d.%s.leave\n",
		DEV_NAME, __LINE__, __func__);

	return 0;

#ifndef DISABLE_ORIGIN_SUSPEND
DONE_SUSPEND:
	up(&s_vpu_sem);
#endif

	pm_runtime_put_sync(pdev);

	pr_info("%s %d.%s.leave\n",
		DEV_NAME, __LINE__, __func__);

	return -EAGAIN;
}

static int vpu_resume(struct device *pdev)
{
	int ret;

	pr_info("%s %d.%s.enter\n",
		DEV_NAME, __LINE__, __func__);

	ret = vpu_pcpu_ipc_vetop_sram_on(pdev);
	if (ret < 0) {
		pr_err("%s %d.%s.vpu_pcpu_ipc_vetop_sram_on() fail\n", DEV_NAME, __LINE__, __func__);
		return ret;
	}

	pm_runtime_force_resume(pdev);

	//RTK wrapper
	ve1_wrapper_setup((1 << 1) | 1);

#ifdef DISABLE_ORIGIN_SUSPEND
#else /* else of DISABLE_ORIGIN_SUSPEND */
	int i;
	int core;
	int product_code = 0;
	unsigned long timeout = jiffies + HZ; /* vpu wait timeout to 1sec */
	u32 val;
	int ref_count;

	spin_lock(&s_vpu_lock);
	ref_count = s_vpu_open_ref_count;
	spin_unlock(&s_vpu_lock);

	if (ref_count > 0) {
		for (core = 0; core < MAX_NUM_VPU_CORE; core++) {

			if (s_bit_firmware_info[core].size == 0) {
				continue;
			}

			product_code = ReadVpuRegister(VPU_PRODUCT_CODE_REGISTER, core);

			WriteVpuRegister(BIT_CODE_RUN, 0, core);

			/*---- LOAD BOOT CODE*/
			for (i = 0; i < 512; i++) {
				val = s_bit_firmware_info[core].bit_code[i];
				WriteVpuRegister(BIT_CODE_DOWN, ((i << 16) | val), core);
			}

			for (i = 0 ; i < 64 ; i++)
				WriteVpuRegister(BIT_BASE+(0x100+(i * 4)), s_vpu_reg_store[core][i], core);

			WriteVpuRegister(BIT_BUSY_FLAG, 1, core);
			WriteVpuRegister(BIT_CODE_RESET, 1, core);
			WriteVpuRegister(BIT_CODE_RUN, 1, core);

			while (ReadVpuRegister(BIT_BUSY_FLAG, core)) {
				if (time_after(jiffies, timeout))
					goto DONE_WAKEUP;
			}
		}
	}
#endif /* end of DISABLE_ORIGIN_SUSPEND */

#ifndef DISABLE_ORIGIN_SUSPEND
DONE_WAKEUP:
#endif /* DISABLE_ORIGIN_SUSPEND */

	pm_runtime_put_sync(pdev);

	pr_info("%s %d.%s.leave\n",
		DEV_NAME, __LINE__, __func__);

	return 0;
}
#else
static int vpu_suspend(struct device *pdev);
static int vpu_resume(struct device *pdev);
#endif /* CONFIG_PM */

static int rtk_ve1_runtime_suspend(struct device *dev)
{
	pr_info("%s %d.%s\n",
		DEV_NAME, __LINE__, __func__);

	if (rtk_ve1_devfreq)
		devfreq_suspend_device(rtk_ve1_devfreq);
	return 0;
}

static int rtk_ve1_runtime_resume(struct device *dev)
{
	pr_info("%s %d.%s.enter\n",
		DEV_NAME, __LINE__, __func__);

	vpu_setup_mmu();

	if ((g_chip_type == CHIP_TYPE_RTD13XXE || g_chip_type == CHIP_TYPE_RTD13XXD) && rstc_iso_bist) {
		reset_control_deassert(rstc_iso_bist);
	}

	ve1_wrapper_setup((1 << 1) | 1);

	if (rtk_ve1_devfreq)
		devfreq_resume_device(rtk_ve1_devfreq);

	pr_info("%s %d.%s.leave\n",
		DEV_NAME, __LINE__, __func__);
	return 0;
}

static const struct dev_pm_ops rtk_ve1_pmops = {
	.runtime_suspend = rtk_ve1_runtime_suspend,
	.runtime_resume = rtk_ve1_runtime_resume,
	.suspend = vpu_suspend,
	.resume = vpu_resume,
};

static struct platform_driver rtk_ve1_driver = {
	.driver = {
		.name = "rtk-ve1",
		.owner = THIS_MODULE,
		.of_match_table = rtk_ve1_dt_match,
		.pm = &rtk_ve1_pmops,
	},
	.probe = vpu_probe,
	.remove = vpu_remove,
	.shutdown = vpu_shutdown,
};
module_platform_driver(rtk_ve1_driver);

MODULE_AUTHOR("A customer using RTK VPU, Inc.");
MODULE_DESCRIPTION("VPU linux driver");
MODULE_LICENSE("GPL");
