#ifndef __RTK_FENCE_H__
#define __RTK_FENCE_H__

#include <linux/dma-fence.h>
#include "rtk_drm_rpc.h"
#include <linux/sync_file.h>
#include <drm/drm_flip_work.h>

#define PLOCK_BUFFER_SET_SIZE   (32) //bytes
#define PLOCK_BUFFER_SET        (2)  // 2 set of PLock buffer for sequence change
#define PLOCK_MAX_BUFFER_INDEX  (PLOCK_BUFFER_SET_SIZE*PLOCK_BUFFER_SET) //bytes  // seperate to 2 set of PLock buffer for sequence change
#define PLOCK_BUFFER_SIZE       (PLOCK_MAX_BUFFER_INDEX*2) //bytes

#define PLOCK_INIT              0xFF
#define PLOCK_QPEND             0
#define PLOCK_RECEIVED          1

#define CONTEXT_SIZE (4096)

struct rtk_dma_fence {
	struct dma_fence *fence;
	s32 __user *out_fence_ptr;
	struct sync_file *sync_file;
	int fd;
	unsigned int idx;
};

struct rtk_drm_fence {
	dma_addr_t pLock_paddr;
	dma_addr_t pReceived_paddr;

	spinlock_t fence_lock;
	spinlock_t idx_lock;

	struct drm_flip_work fence_signal_work;
	struct list_head pending;
	struct list_head drop_list;

	unsigned int idx;
};

#endif
