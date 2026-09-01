// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Realtek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/dma-buf.h>
#include <drm/drm_device.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_file.h>

#include "rtk_drm_drv.h"
#include "rtk_drm_gem.h"
#include "rtk_drm_rpc.h"

#define METADATA_HEADER 0x52544b6d

int rtk_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct rtk_gem_object *rtk_obj;
	struct drm_gem_object *gem_obj;
	struct drm_device *drm;
	int ret;
#ifdef DMABUF_HEAPS_RTK
	struct dma_buf *dmabuf;
#endif

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	gem_obj = vma->vm_private_data;
	rtk_obj = to_rtk_gem_obj(gem_obj);
	drm = gem_obj->dev;

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;
#ifdef DMABUF_HEAPS_RTK
	dmabuf = rtk_obj->dmabuf;
	ret = dmabuf->ops->mmap(dmabuf, vma);
	return ret;
#else
	ret = dma_mmap_attrs(drm->dev, vma, rtk_obj->vaddr, rtk_obj->paddr,
				gem_obj->size, DMA_ATTR_WRITE_COMBINE);
	if (ret)
		drm_gem_vm_close(vma);
	return ret;
#endif
}

struct rtk_gem_object *
__rtk_gem_object_create(struct drm_device *drm, size_t size)
{
	struct rtk_gem_object *rtk_obj;
	struct drm_gem_object *gem_obj;

	rtk_obj = kzalloc(sizeof(*rtk_obj), GFP_KERNEL);
	if (!rtk_obj)
		return ERR_PTR(-ENOMEM);
	gem_obj = &rtk_obj->base;

	if (drm_gem_object_init(drm, gem_obj, size) < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		kfree(rtk_obj);
		return ERR_PTR(-ENOMEM);
	}
	return rtk_obj;
}

const struct vm_operations_struct rtk_gem_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static int __maybe_unused rtk_gem_add_info(struct drm_device *drm,
					   struct rtk_gem_object *rtk_obj)
{
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_gem_object_info *info;
	struct pid *pid;
	const char *name;
	int ret, slot = -1;
	int i;

	mutex_lock(&priv->obj_lock);

	pid = get_task_pid(current, PIDTYPE_PID);
	name = kasprintf(GFP_KERNEL, "%s[%d]", current->comm, pid_nr(pid));
	if (priv->obj_info == NULL) {
		priv->obj_info = kcalloc(1, sizeof(*priv->obj_info),
					 GFP_KERNEL);
		priv->obj_info_num++;
		slot = 0;
		goto fill_slot;
	}

	for (i = 0; i < priv->obj_info_num; i++) {
		if (priv->obj_info[i].name == NULL) {
			if (slot == -1)
				slot = i;
			continue;
		}
		ret = strcmp(name, priv->obj_info[i].name);
		if (ret == 0) {
			kfree(name);
			name = priv->obj_info[i].name;
			slot = i;
			goto fill_slot;
		}
	}

	if (slot < 0) {
		slot = priv->obj_info_num;
		priv->obj_info_num++;
		priv->obj_info = krealloc(priv->obj_info, priv->obj_info_num *
					  sizeof(*priv->obj_info), GFP_KERNEL);
	}

fill_slot:
	info = &priv->obj_info[slot];
	info->name = name;
	info->paddr[info->num_allocated] = rtk_obj->paddr;
	info->num_allocated++;
	info->size_allocated += rtk_obj->base.size;

	put_pid(pid);
	mutex_unlock(&priv->obj_lock);

	return 0;
}

static int __maybe_unused rtk_gem_del_info(struct drm_device *drm,
					   struct rtk_gem_object *rtk_obj)
{
	struct rtk_drm_private *priv = drm->dev_private;
	struct rtk_gem_object_info *info;
	struct pid *pid;
	const char *name;
	int find, i, j;

	mutex_lock(&priv->obj_lock);

	pid = get_task_pid(current, PIDTYPE_PID);
	name = kasprintf(GFP_KERNEL, "%s[%d]", current->comm, pid_nr(pid));
	if (!name) {
		put_pid(pid);
		DRM_ERROR("%s - no memory\n", __func__);
		mutex_unlock(&priv->obj_lock);
		return -ENOMEM;
	}
	for (i = 0; i < priv->obj_info_num; i++) {
		info = &priv->obj_info[i];
		if (info->name == NULL)
			continue;

		find = 0;
		for (j = 0; j < info->num_allocated; j++) {
			if (find == 1)
				info->paddr[j-1] = info->paddr[j];
			if (info->paddr[j] == rtk_obj->paddr)
				find = 1;
		}
		if (find) {
			info->num_allocated--;
			info->size_allocated -= rtk_obj->base.size;
			if (!info->num_allocated) {
				kfree(info->name);
				info->name = NULL;
			}
			goto exit;
		}
	}

	DRM_ERROR("%s - can't find match context to remove\n", __func__);
exit:
	put_pid(pid);
	kfree(name);
	mutex_unlock(&priv->obj_lock);
	return 0;
}

struct sg_table *rtk_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct rtk_gem_object *rtk_obj = to_rtk_gem_obj(obj);
	struct sg_table *sgt;
	struct drm_device *drm = obj->dev;
	int ret;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		goto err_get_sgtable;

	ret = dma_get_sgtable_attrs(drm->dev, sgt, rtk_obj->vaddr,
				    rtk_obj->paddr, obj->size,
				    DMA_ATTR_WRITE_COMBINE);
	if (ret) {
		DRM_ERROR("failed to allocate sgt, %d\n", ret);
		kfree(sgt);
		return ERR_PTR(ret);
	}
	return sgt;

err_get_sgtable:
	DRM_ERROR("get sg table fail\n");
	return NULL;
}

struct drm_gem_object *rtk_gem_prime_import_sg_table(struct drm_device *dev,
			struct dma_buf_attachment *attach, struct sg_table *sg)
{
	struct rtk_gem_object *rtk_obj;
	struct video_object *vobj;

	if (sg->nents != 1)
		return ERR_PTR(-EINVAL);

	rtk_obj = __rtk_gem_object_create(dev, attach->dmabuf->size);
	if (IS_ERR(rtk_obj))
		return ERR_CAST(rtk_obj);

	vobj = (struct video_object *)dma_buf_vmap(attach->dmabuf);
	if (vobj && vobj->header.type == METADATA_HEADER) {
		rtk_obj->vaddr = vobj;
		rtk_obj->dmabuf_type = DMABUF_TYPE_METADATA;
	} else {
		rtk_obj->paddr = sg_dma_address(sg->sgl);
		rtk_obj->dmabuf_type = DMABUF_TYPE_NORMAL;
	}

	if (vobj)
		dma_buf_vunmap(attach->dmabuf, vobj);

	rtk_obj->sgt = sg;

	return &rtk_obj->base;
}

int rtk_gem_prime_mmap(struct drm_gem_object *gem_obj,
		      struct vm_area_struct *vma)
{
	struct rtk_gem_object *rtk_obj = to_rtk_gem_obj(gem_obj);
//	struct drm_device *drm = gem_obj->dev;
	int ret;
#ifdef DMABUF_HEAPS_RTK
	struct dma_buf *dmabuf;
#endif

	ret = drm_gem_mmap_obj(gem_obj, gem_obj->size, vma);
	if (ret < 0)
		return ret;

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;
#ifdef DMABUF_HEAPS_RTK
	dmabuf = rtk_obj->dmabuf;
	ret = dmabuf->ops->mmap(dmabuf, vma);
#else
	ret = dma_mmap_attrs(drm->dev, vma, rtk_obj->vaddr, rtk_obj->paddr,
				gem_obj->size, DMA_ATTR_WRITE_COMBINE);
#endif
	if (ret)
		drm_gem_vm_close(vma);
	return ret;
}

void rtk_gem_free_object(struct drm_gem_object *gem_obj)
{
//	struct drm_device *drm = gem_obj->dev;
	struct rtk_gem_object *rtk_obj = to_rtk_gem_obj(gem_obj);
#ifdef DMABUF_HEAPS_RTK
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
#endif

//	rtk_gem_del_info(drm, rtk_obj);

	if (gem_obj->import_attach) {
		drm_prime_gem_destroy(gem_obj, rtk_obj->sgt);
	} else {
#ifdef DMABUF_HEAPS_RTK
		dmabuf = rtk_obj->dmabuf;
		attach = rtk_obj->attach;
		table = rtk_obj->sgt;
		if (rtk_obj->vaddr) {
			dma_buf_vunmap(dmabuf, rtk_obj->vaddr);
		}
		dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
		dma_buf_unmap_attachment(attach, table,
					 DMA_BIDIRECTIONAL);
		dma_buf_detach(dmabuf, attach);
		dma_buf_put(dmabuf);
#else
		dma_free_attrs(drm->dev, gem_obj->size, rtk_obj->vaddr, rtk_obj->paddr,
				DMA_ATTR_WRITE_COMBINE);
#endif
	}
	drm_gem_object_release(gem_obj);

	kfree(rtk_obj);
}

void *rtk_gem_prime_vmap(struct drm_gem_object *gem_obj)
{
	struct rtk_gem_object *rtk_obj = to_rtk_gem_obj(gem_obj);

	return rtk_obj->vaddr;
}

void rtk_gem_prime_vunmap(struct drm_gem_object *gem_obj, void *vaddr)
{
    /* Nothing to do */
}

static struct rtk_gem_object *rtk_gem_object_create(struct drm_device *drm,
					size_t size, unsigned int flags)
{
	struct rtk_gem_object *rtk_obj;
	struct device *dev = drm->dev;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	dma_addr_t dma_addr;
	int ret = 0;

	size = round_up(size, PAGE_SIZE);

	rtk_obj = __rtk_gem_object_create(drm, size);
	if (IS_ERR(rtk_obj))
		return ERR_CAST(rtk_obj);

#ifdef DMABUF_HEAPS_RTK
	if (!is_media_heap(flags)) {
		if (is_audio_heap(flags))
			dmabuf = rheap_alloc("rtk_audio_heap",
						 size, get_rtk_flags(flags));
		else
			dmabuf = rheap_alloc(NULL,
						 size, get_rtk_flags(flags));
	} else {
		dmabuf = rheap_alloc("rtk_media_heap", size,
						 get_rtk_flags(flags));
		if (IS_ERR_OR_NULL(dmabuf)) {
			if (is_audio_heap(flags))
				 dmabuf = rheap_alloc("rtk_audio_heap",
						 size, get_rtk_flags(flags));
		}
	}

	if (IS_ERR_OR_NULL(dmabuf)) {
		DRM_ERROR("failed to allocate dma heap buffer\n");
		goto err_dma_alloc;
	}

	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		dev_err(dev, "Failed to attach dmabuf\n");
		goto attach_err;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		dev_err(dev, "Failed to map attachment \n");
		goto map_err;
	}

	if (table->nents != 1) {
	    ret = -EINVAL;
	    goto access_err;
	}

	dma_addr = sg_dma_address(table->sgl);
	if (!dma_addr) {
	    ret = -ENOMEM;
	    goto access_err;
	}

	ret = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (ret)
		goto access_err;

	rtk_obj->vaddr = dma_buf_vmap(dmabuf);
	if (!rtk_obj->vaddr) {
		dev_err(dev, "dma_buf_vmap failed\n");
		ret = -ENOMEM;
		goto kmap_err;
	}

	rtk_obj->attach = attach;
	rtk_obj->paddr = dma_addr;
	rtk_obj->dmabuf = dmabuf;
	rtk_obj->sgt= table;
#else
	rtk_obj->vaddr = dma_alloc_attrs(drm->dev, size,
					&rtk_obj->paddr,
					GFP_KERNEL | __GFP_NOWARN,
					DMA_ATTR_WRITE_COMBINE);
#endif
	if (!rtk_obj->vaddr) {
		DRM_ERROR("failed to allocate dma buffer\n");
		goto err_dma_alloc;
	}
	return rtk_obj;

kmap_err:
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
access_err:
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
map_err:
	dma_buf_detach(dmabuf, attach);
attach_err:
	dma_buf_put(dmabuf);
err_dma_alloc:
	drm_gem_object_release(&rtk_obj->base);
	return ERR_PTR(-ENOMEM);
}

int rtk_gem_dumb_create(struct drm_file *file_priv,
			struct drm_device *drm,
			struct drm_mode_create_dumb *args)
{
	struct rtk_gem_object *rtk_obj;
	int ret;

	DRM_DEBUG_KMS("[ %d x %d, bpp=%d, flags=0x%x]\n", args->width,
			args->height, args->bpp, args->flags);

	args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	args->size = args->pitch * args->height;

	rtk_obj = rtk_gem_object_create(drm, args->size, args->flags);
	if (IS_ERR(rtk_obj))
		return PTR_ERR(rtk_obj);

	ret = drm_gem_handle_create(file_priv, &rtk_obj->base, &args->handle);
	if (ret)
		goto err_create_handle;

//	rtk_gem_add_info(drm, rtk_obj);

	drm_gem_object_put(&rtk_obj->base);

	return 0;

err_create_handle:
	rtk_gem_free_object(&rtk_obj->base);
	return ret;
}

int rtk_gem_dumb_map_offset(struct drm_file *file_priv,
			    struct drm_device *dev, uint32_t handle,
			    uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret;

	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return -EINVAL;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
	DRM_DEBUG_KMS("offset = 0x%llx\n", *offset);

out:
	drm_gem_object_put(obj);

	return 0;
}

#if defined(CONFIG_DEBUG_FS)
int rtk_gem_info_debugfs(struct seq_file *m, void *unused)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *drm = node->minor->dev;
	struct rtk_drm_private *priv = drm->dev_private;
	int i;

	mutex_lock(&priv->obj_lock);
	for (i = 0; i < priv->obj_info_num; i++) {
		if (!priv->obj_info[i].num_allocated)
			continue;
		seq_printf(m, "%30s: %6dkb, objs-%d\n",
			   priv->obj_info[i].name,
			   priv->obj_info[i].size_allocated / 1024,
			   priv->obj_info[i].num_allocated);
	}
	mutex_unlock(&priv->obj_lock);

	return 0;
}
#endif
