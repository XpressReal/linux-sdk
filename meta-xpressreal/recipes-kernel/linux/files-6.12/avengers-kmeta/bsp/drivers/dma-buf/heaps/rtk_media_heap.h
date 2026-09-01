// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF RTK heap
 *
 * Copyright (c) 2022 Realtek Semiconductor Corp
 * Author: <cy.huang@realtek.com> .
 */

 #ifndef _RTK_MEDIA_HEAP_H
 #define _RTK_MEDIA_HEAP_H

bool is_rtk_static_protect(int flags);
void rtk_check_flag(unsigned long *flags);
bool is_rtk_dynamic_protect(int flags);
bool __maybe_unused is_rtk_gen_heap(int flags);
void rtk_pool_free(struct rtk_heap *rtk_heap, struct page *pages, size_t size,
		   const char *name);
void rtk_normal_free(struct rtk_heap *rtk_heap, struct page *pages, size_t size,
		     const char *name);
void rtk_static_secure_free(struct rtk_heap *rtk_heap, struct page *pages,
			    size_t size, const char *name);
void rtk_dynamic_secure_free(struct rtk_heap *rtk_heap, struct page *pages,
			     size_t len, const char *name);
struct page *rtk_dynamic_secure_allocate(struct rtk_heap *rtk_heap, size_t len,
					 unsigned long flags, char *caller);
struct page *rtk_normal_allocate(struct rtk_heap *rtk_heap, size_t size,
				 unsigned long flags, char *caller);
struct page *rtk_static_secure_allocate(struct rtk_heap *rtk_heap, size_t size,
					unsigned long flags, char *caller);
struct page *rtk_pool_allocate(struct rtk_heap *rtk_heap, size_t size,
			       unsigned long flags, char *caller);
#endif /* _RTK_MEDIA_HEAP_H */
