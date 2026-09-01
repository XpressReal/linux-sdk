#ifndef _LINUX_ION_RTK_CARVEOUT_HEAP_DEBUGFS_H
#define _LINUX_ION_RTK_CARVEOUT_HEAP_DEBUGFS_H

#include <linux/ion.h>

#ifdef CONFIG_DEBUG_FS
void debugfs_add_heap(struct ion_heap *heap);
#else
void debugfs_add_heap(struct ion_heap *heap)
{
}
#endif

extern int ion_carveout_heap_debug_show(struct ion_heap *heap,
					struct seq_file *s, void *prefix);

#endif /* _LINUX_ION_RTK_CARVEOUT_HEAP_DEBUGFS_H */
