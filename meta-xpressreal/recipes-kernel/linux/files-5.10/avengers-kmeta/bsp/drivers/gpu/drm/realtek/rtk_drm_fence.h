#ifndef __RTK_FENCE_H__
#define __RTK_FENCE_H__

#include <linux/dma-fence.h>
#include <linux/sync_file.h>
#include <drm/drm_flip_work.h>

#define PLOCK_BUFFER_SHIFT	512

#define PLOCK_BUFFER_SET_SIZE   (32) //bytes
#define PLOCK_BUFFER_SET        (2)  // 2 set of PLock buffer for sequence change
#define PLOCK_MAX_BUFFER_INDEX  (PLOCK_BUFFER_SET_SIZE*PLOCK_BUFFER_SET) //bytes  // seperate to 2 set of PLock buffer for sequence change
#define PLOCK_BUFFER_SIZE       (PLOCK_MAX_BUFFER_INDEX*2) //bytes

#define PLOCK_INIT              0xFF
#define PLOCK_QPEND             0
#define PLOCK_RECEIVED          1

struct rtk_dma_fence {
	struct dma_fence *fence;
	s32 __user *out_fence_ptr;
	struct sync_file *sync_file;
	int fd;
	unsigned int idx;
};

struct rtk_drm_fence {
	struct rtk_rpc_info *rpc_info;

	unsigned char *lock_vaddr;
	unsigned char *rcv_vaddr;
	dma_addr_t lock_paddr;
	dma_addr_t rcv_paddr;

	spinlock_t fence_lock;

	struct drm_flip_work fence_signal_work;
	struct list_head pending;

	unsigned int idx;
};

#endif
