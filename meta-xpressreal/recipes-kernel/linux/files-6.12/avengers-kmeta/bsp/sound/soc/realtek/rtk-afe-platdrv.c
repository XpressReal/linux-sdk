// SPDX-License-Identifier: GPL-2.0
/*
 * rtk-afe-platform-driver.c  --  Realtek afe platform driver
 *
 * Copyright (c) 2024 RealTek Inc.
 * Author: Simon Hsu <simon_hsu@realtek.com>
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/dma-buf.h>
#include <linux/dma-map-ops.h>
#include <sound/hwdep.h>

#include <soc/realtek/rtk_refclk.h>

#include "rtk-afe-common.h"

MODULE_IMPORT_NS(DMA_BUF);

#define for_each_pcm_substream(pcm, str, subs) \
	for ((str) = 0; (str) < 2; (str)++) \
		for ((subs) = (pcm)->streams[str].substream; (subs); \
		     (subs) = (subs)->next)

static const struct rtk_afe_data stark_afe =
{
	.ao_num = 1,
	.ao_bitmap_mask = 0x1F,
	.i2s_out_ch = 2,
	.lb_secure = 0,
};

static const struct rtk_afe_data morbius_afe =
{
	.ao_num = 1,
	.ao_bitmap_mask = 0xFFF,
	.i2s_out_ch = 2,
	.lb_secure = 1,
};

static const struct rtk_afe_data kent_afe =
{
	.ao_num = 1,
	.ao_bitmap_mask = 0xFFF,
	.i2s_out_ch = 8,
	.lb_secure = 1,
};

static const struct rtk_afe_data kent_2ao_afe =
{
	.ao_num = 2,
	.ao_bitmap_mask = 0xFFF,
	.i2s_out_ch = 8,
	.lb_secure = 1,
};

static const struct snd_pcm_hardware rtk_afe_hardware = {
	.info = RTK_PCM_INFO,
	.formats = RTK_PCM_FORMATS,
	.period_bytes_min = RTK_PCM_MIN_PERIOD_SIZE,
	.period_bytes_max = RTK_PCM_MAX_PERIOD_SIZE,
	.periods_min = RTK_PCM_PERIODS_MIN,
	.periods_max = RTK_PCM_PERIODS_MAX,
	.buffer_bytes_max = RTK_PCM_MAX_BUFFER_SIZE,
	.fifo_size = RTK_PCM_FIFO_SIZE,
};

static const struct snd_pcm_hardware rtk_afe_capture_hardware = {
	.info = RTK_PCM_INFO,
	.formats = RTK_PCM_CAPTURE_FORMATS,
	.period_bytes_min  = RTK_PCM_MIN_PERIOD_SIZE,
	.period_bytes_max = 16 * 1024,
	.periods_min = RTK_PCM_PERIODS_MIN,
	.periods_max = RTK_PCM_PERIODS_MAX,
	.buffer_bytes_max = RTK_PCM_MAX_BUFFER_SIZE,
	.fifo_size = RTK_PCM_FIFO_SIZE,
};

static int rtk_snd_alloc(struct device *dev, size_t size,
			 void *phy, void **virt, unsigned long heap_flags)
{
	mutex_lock(&dev->mutex);
	rheap_setup_dma_pools(dev, "rtk_media_heap", heap_flags, __func__);
	*virt = dma_alloc_coherent(dev, size, phy, GFP_KERNEL);
	mutex_unlock(&dev->mutex);
	if (!*virt) {
		dev_err(dev, "[%s] alloc fail\n", __func__);
		return -ENOMEM;
	}
	return 0;
}

unsigned long ring_valid_data(unsigned long ring_base,
			      unsigned long ring_limit,
			      unsigned long ring_rp,
			      unsigned long ring_wp)
{
	if (ring_wp >= ring_rp)
		return (ring_wp - ring_rp);
	else
		return (ring_limit - ring_base) - (ring_rp - ring_wp);
}

unsigned long ring_add(unsigned long ring_base,
		       unsigned long ring_limit,
		       unsigned long ptr,
		       unsigned int bytes)
{
	ptr += bytes;
	if (ptr >= ring_limit)
		ptr = ring_base + (ptr - ring_limit);

	return ptr;
}

unsigned long valid_free_size(unsigned long base,
			      unsigned long limit,
			      unsigned long rp,
			      unsigned long wp)
{
	/* -1 to avoid confusing full or empty */
	return (limit - base) - ring_valid_data(base, limit, rp, wp) - 1;
}

void rtk_snd_ringbuf_destroy(struct kref *kref)
{
	struct rtk_snd_ringbuf *buf =
			container_of(kref, struct rtk_snd_ringbuf, ref);

	rtk_snd_free_ringbuf(&buf);
}

int rtk_snd_reinit_ringheader(struct rtk_snd_ringbuf *ringbuf)
{
	struct rtk_afe_priv *afe;
	struct rpc_ringbuffer_header rpc_hdr = {0};
	int ret = 0, i, j;

	afe = ringbuf->afe;

	for (i = 0; i < ringbuf->num_hdrs; i++) {
		struct rtk_snd_ringbuf_hdr *hdr = &ringbuf->hdrs[i];

		/* uninit ringbuffer header */
		rpc_hdr.instance_id = hdr->rpc_hdr.instance_id;
		rpc_hdr.pin_id = hdr->rpc_hdr.pin_id;
		ret = rpc_init_ringbuffer_header(&afe->rpc_priv, &rpc_hdr);
		if (ret)
			goto err_send_rpc;


		for (j = 0; j < ringbuf->bufnum; j++) {
			*hdr->ptrs[j].p_write_ptr = *hdr->ptrs[j].p_begin_addr;
			*hdr->ptrs[j].p_read_ptr = *hdr->ptrs[j].p_begin_addr;
		}

		/* re-init ringbuffer header*/
		ret = rpc_init_ringbuffer_header(&afe->rpc_priv, &hdr->rpc_hdr);
		if (ret)
			goto err_send_rpc;
	}

err_send_rpc:
	return ret;
}

void rtk_snd_free_ringbuf(struct rtk_snd_ringbuf **ringbuf)
{
	struct rtk_afe_priv *afe;
	struct rpc_ringbuffer_header rpc_hdr = {0};
	struct snd_dma_buffer *dmab;
	dma_addr_t paddr;
	void *vaddr;
	size_t size;
	int i;

	if (*ringbuf == NULL) {
		pr_err("%s but ringbuf is NULL\n", __func__);
		return;
	}

	afe = (*ringbuf)->afe;

	if ((*ringbuf)->hdrs) {
		for (i = 0; i < (*ringbuf)->num_hdrs; i++) {
			struct rtk_snd_ringbuf_hdr *hdr = &(*ringbuf)->hdrs[i];

			dev_info(afe->dev, "id = 0x%x, pin = 0x%x, number = %d\n",
				 hdr->rpc_hdr.instance_id, hdr->rpc_hdr.pin_id,
				 (*ringbuf)->bufnum);

			/* uninit ringbuffer */
			rpc_hdr.instance_id = hdr->rpc_hdr.instance_id;
			rpc_hdr.pin_id = hdr->rpc_hdr.pin_id;
			if (rpc_hdr.instance_id && rpc_hdr.pin_id)
				rpc_init_ringbuffer_header(&afe->rpc_priv, &rpc_hdr);

			/* free descriptor page */
			if (hdr->vaddr)
				dma_free_coherent(afe->dev, SZ_4K, hdr->vaddr, hdr->paddr);
			if (hdr->ptrs)
				kfree(hdr->ptrs);
		}
		kfree((*ringbuf)->hdrs);
		(*ringbuf)->hdrs = NULL;
	}

	for (i = 0; i < (*ringbuf)->bufnum; i++) {
		dmab = &(*ringbuf)->dmab[i];
		vaddr = (void *)dmab->area;
		if (vaddr) {
			paddr = dmab->addr;
			size = dmab->bytes;
			dma_free_coherent(afe->dev, size, vaddr, paddr);
		} else {
			dev_err(afe->dev, "free dmabuf fail %d\n", i);
		}
	}

	if ((*ringbuf)->dmab) {
		kfree((*ringbuf)->dmab);
		(*ringbuf)->dmab = NULL;
	}
	kfree(*ringbuf);
	*ringbuf = NULL;
}

int rtk_snd_prepare_ringbuf(struct rtk_afe_priv *afe,
			    struct snd_dma_buffer *dma_buffer,
			    size_t size, size_t bufnum,
			    struct rtk_snd_ringbuf **ringbuf,
			    struct rpc_ringbuffer_header *rpc_hdrs,
			    int num_hdrs,
			    unsigned long heap_flags)
{
	struct rtk_snd_ringbuf *buf;
	struct snd_dma_buffer *dmab;
	dma_addr_t paddr;
	void *vaddr;
	unsigned long flags;
	int i, j, ret = -1;

	if (*ringbuf) {
		pr_err("%s ringbuf not null, free it first\n", __func__);
		rtk_snd_free_ringbuf(ringbuf);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	buf->afe = afe;

	if (dma_buffer) {
		buf->dmab = dma_buffer;
		buf->bufnum = 1;
		goto prepare_ringbuf_header;
	}

	dmab = kcalloc(bufnum, sizeof(*dmab), GFP_KERNEL);
	if (!dmab)
		goto err_alloc_ringbuf;
	buf->dmab = dmab;

	for (i = 0; i < bufnum; i++) {
		ret = rtk_snd_alloc(afe->dev, size, (void *)&paddr, &vaddr,
				    heap_flags);
		if (ret)
			goto err_alloc_ringbuf;
		dmab[i].addr = paddr;
		dmab[i].area = vaddr;
		dmab[i].bytes = size;
		dmab[i].dev.dev = afe->dev;
		buf->bufnum ++;
	}

prepare_ringbuf_header:
	buf->hdrs = kcalloc(num_hdrs, sizeof(*buf->hdrs), GFP_KERNEL);
	if (!buf->hdrs)
		goto err_alloc_ringbuf;
	buf->num_hdrs = num_hdrs;

	if (afe->fw_mem_cfg)
		flags = RTK_ACPU_FLAGS;
	else
		flags = RTK_HIFI_FLAGS;

	ret = rtk_snd_alloc(afe->dev, SZ_4K, (void *)&paddr, &vaddr, flags);

	if (ret)
		goto err_alloc_ringbuf;
	buf->hdrs->paddr = paddr;
	buf->hdrs->vaddr = vaddr;

	for (i = 0; i < num_hdrs; i++) {
		struct rtk_snd_ringbuf_hdr *hdr = &buf->hdrs[i];
		int offset = 0;

		/* Allocate low-level descriptors that will sit on this page */
		hdr->ptrs = kcalloc(bufnum, sizeof(*hdr->ptrs), GFP_KERNEL);
		if (!hdr->ptrs)
			goto err_alloc_ringbuf;

		for (j = 0; j < bufnum; j++) {
			offset = init_ringbuf_header_ptrs(&hdr->ptrs[j], vaddr + j*offset, afe->fw_mem_cfg);

			if (!dma_buffer)
				*hdr->ptrs[j].p_begin_addr = buf->dmab[j].addr;
			else
				*hdr->ptrs[j].p_begin_addr = dma_buffer->addr;
			*hdr->ptrs[j].p_size = size;
			*hdr->ptrs[j].p_buffer_id = 0;
			*hdr->ptrs[j].p_write_ptr = *hdr->ptrs[j].p_begin_addr;
			*hdr->ptrs[j].p_read_ptr = *hdr->ptrs[j].p_begin_addr;
			*hdr->ptrs[j].p_num_read_ptr = 1;
		}

		/* Prepare and send high-level RPC header */
		memcpy(&hdr->rpc_hdr, &rpc_hdrs[i], sizeof(rpc_hdrs[i]));
		hdr->rpc_hdr.list_size = bufnum;
		for (j = 0; j < bufnum; j++)
			hdr->rpc_hdr.ringbuffer_header_list[j] = paddr + j*offset;

		if (hdr->rpc_hdr.instance_id != 0) {
			ret = rpc_init_ringbuffer_header(&afe->rpc_priv, &hdr->rpc_hdr);
			if (ret)
				goto err_alloc_ringbuf;
		}
		dev_info(afe->dev, "alloc id = 0x%x, pin = 0x%x, number = %d\n",
			 hdr->rpc_hdr.instance_id, hdr->rpc_hdr.pin_id,
			 buf->bufnum);
	}

	*ringbuf = buf;
	return 0;

err_alloc_ringbuf:
	pr_err("%s err alloc ringbuf\n", __func__);
	rtk_snd_free_ringbuf(&buf);

	return ret;
}

typedef int (*dai_register_cb)(struct rtk_afe_priv *);

static const dai_register_cb dai_register_cbs[] = {
	rtk_dai_i2s_register,
	rtk_dai_pdm_register,
	rtk_dai_tdm_register,
	rtk_dai_adc_register,
	rtk_dai_aio_register,
	rtk_dai_dprx_register,
	rtk_dai_memif_register,
};

static int rtk_snd_monitor_latency(struct snd_pcm_substream *substream,
				   struct rtk_afe_memif *memif)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_ringbuf *buf = memif->ringbuf;
	struct ringbuf_header_ptrs *hdr = buf->hdrs[0].ptrs;
	struct alsa_latency_info *fw_info, info;
	unsigned int retry = 0;
	unsigned int hwring_wp;
	unsigned int wp_buffer;
	unsigned int wp_frame = 0;
	u64 pcm_pts, cur_pts, diff_pts;
	u64 queuebuffer;
	int audio_latency;

	fw_info = hdr->p_latency;

	memcpy(&info, fw_info, sizeof(*fw_info));

	while(info.sum != (info.latency + info.ptsl)) {
		if (retry > 100) {
			if (info.ptsl < info.sum)
				info.latency = info.sum - info.ptsl;
			break;
		}
		memcpy(&info, fw_info, sizeof(*fw_info));
		retry++;
	}

	hwring_wp = *hdr->p_write_ptr;

	pcm_pts = ((u64)info.ptsh << 32) | ((u64)info.ptsl);
	cur_pts = (u64)refclk_get_val_raw();
	diff_pts = cur_pts - pcm_pts;

	if (info.decin_wp != hwring_wp) {
		wp_buffer = ring_valid_data(*hdr->p_begin_addr,
					*hdr->p_begin_addr + *hdr->p_size,
					*hdr->p_read_ptr,
					*hdr->p_write_ptr);
		wp_frame = bytes_to_frames(runtime, wp_buffer);
	}

	queuebuffer = wp_frame +
			ring_valid_data(0, runtime->boundary,
					memif->total_write,
					runtime->control->appl_ptr);
	queuebuffer = div_u64(queuebuffer * 1000000, runtime->rate);
	audio_latency = info.latency + queuebuffer - div64_ul(diff_pts * 1000, 90);
	audio_latency = audio_latency / 1000;

	if (audio_latency < 0)
		audio_latency = 0;

	return audio_latency;
}

struct rtk_snd_dmabuf_attachment {
	struct sg_table sgt;
};

static int rtk_snd_dmabuf_attach(struct dma_buf *dmabuf,
				 struct dma_buf_attachment *attach)
{
	struct rtk_snd_dmabuf_attachment *a;
	struct rtk_snd_ringbuf *buf = dmabuf->priv;
	struct snd_dma_buffer *dmab = buf->dmab;
	struct device *dev = dmab->dev.dev;
	dma_addr_t daddr = dmab->addr;
	void *vaddr = dmab->area;
	size_t size = dmab->bytes;
	int ret;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	ret = dma_get_sgtable(dev, &a->sgt, vaddr, daddr, size);
	if (ret < 0) {
		dev_err(dev, "failed to get scatterlist from DMA API\n");
		kfree(a);
		return -EINVAL;
	}

	attach->priv = a;

	return 0;
}

static void rtk_snd_dmabuf_detatch(struct dma_buf *dmabuf,
				   struct dma_buf_attachment *attach)
{
	struct rtk_snd_dmabuf_attachment *a = attach->priv;

	sg_free_table(&a->sgt);
	kfree(a);
}

static struct sg_table *rtk_snd_map_dmabuf(struct dma_buf_attachment *attach,
					   enum dma_data_direction dir)
{
	struct rtk_snd_dmabuf_attachment *a = attach->priv;
	struct sg_table *table;
	int ret;

	table = &a->sgt;

	ret = dma_map_sgtable(attach->dev, table, dir, 0);
	if (ret)
		table = ERR_PTR(ret);
	return table;
}

static void rtk_snd_unmap_dmabuf(struct dma_buf_attachment *attach,
				 struct sg_table *table,
				 enum dma_data_direction dir)
{
	dma_unmap_sgtable(attach->dev, table, dir, 0);
}

static int rtk_snd_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct rtk_snd_ringbuf *buf = dmabuf->priv;
	struct snd_dma_buffer *dmab = buf->dmab;
	struct device *dev = dmab->dev.dev;
	dma_addr_t paddr = dmab->addr;
	void *vaddr = dmab->area;
	size_t size = vma->vm_end - vma->vm_start;

	if (vaddr)
		return dma_mmap_coherent(dev, vma, vaddr, paddr, size);

	return 0;
}

static void rtk_snd_release(struct dma_buf *dmabuf)
{
	struct rtk_snd_ringbuf *buf = dmabuf->priv;

	kref_put(&buf->ref, rtk_snd_ringbuf_destroy);
}

static const struct dma_buf_ops rtk_snd_dmabuf_ops = {
	.attach = rtk_snd_dmabuf_attach,
	.detach = rtk_snd_dmabuf_detatch,
	.map_dma_buf = rtk_snd_map_dmabuf,
	.unmap_dma_buf = rtk_snd_unmap_dmabuf,
	.mmap = rtk_snd_mmap,
	.release = rtk_snd_release,
};

static int rtk_hwdep_ioctl(struct snd_hwdep *hwdep, struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	struct rtk_afe_memif *memif = hwdep->private_data;
	struct snd_pcm_substream *substream;
	struct rtk_snd_ringbuf *buf;
	struct rtk_pcm_mmap_fd __user *mmap_fd;
	struct dma_buf *dmabuf;
	int ret = 0, fd, latency;

	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	mutex_lock(&memif->lock);

	substream = memif->substream;
	buf = memif->ringbuf;
	switch (cmd) {
	case SNDRV_PCM_IOCTL_MMAP_DATA_FD:
		if (!substream || !substream->runtime) {
			ret = -ENODEV;
			goto err_exit;
		}
		if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
			ret = -EINVAL;
			goto err_exit;
		}
		mmap_fd = (struct rtk_pcm_mmap_fd __user *)arg;

		exp_info.ops = &rtk_snd_dmabuf_ops;
		exp_info.size = buf->dmab->bytes;
		exp_info.flags = O_RDWR;
		exp_info.priv = buf;
		kref_get(&buf->ref);

		dmabuf = dma_buf_export(&exp_info);
		if (IS_ERR(dmabuf))
			goto err_export_buf;
		fd = dma_buf_fd(dmabuf, O_CLOEXEC);
		if (fd < 0)
			goto err_get_fd;
		if (put_user(fd, &mmap_fd->fd))
			goto err_get_fd;
		break;

err_get_fd:
		dma_buf_put(dmabuf);
err_export_buf:
		pr_err("%s: err mmap data fd\n", __func__);
		kref_put(&buf->ref, rtk_snd_ringbuf_destroy);
		ret = -1;
		break;

	case SNDRV_PCM_IOCTL_GET_LATENCY:
		if (!substream || !substream->runtime) {
			ret = -ENODEV;
			goto err_exit;
		}
		if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
			ret = -EINVAL;
			goto err_exit;
		}

		latency = rtk_snd_monitor_latency(substream, memif);
		ret = put_user(latency, (int __user *)arg);
		break;

	default:
		ret = -EINVAL;
		break;
	};

err_exit:
	mutex_unlock(&memif->lock);
	return ret;
}

#ifdef CONFIG_COMPAT
static int rtk_hwdep_compat_ioctl(struct snd_hwdep *hwdep, struct file *file,
				  unsigned int cmd, unsigned long arg)
{
	return rtk_hwdep_ioctl(hwdep, file, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define rtk_hwdep_compat_ioctl NULL
#endif

static int rtk_snd_create_hwdep(struct snd_pcm *pcm, struct rtk_afe_memif *memif)
{
	static const struct snd_hwdep_ops ops = {
		.ioctl		= rtk_hwdep_ioctl,
		.ioctl_compat	= rtk_hwdep_compat_ioctl,
	};
	struct snd_hwdep *hwdep;
	int ret;

	ret = snd_hwdep_new(pcm->card, pcm->name, pcm->device, &hwdep);
	if (ret)
		return ret;

	strncpy(hwdep->name, pcm->name, sizeof(hwdep->name) - 1);
	hwdep->iface = pcm->device;
	hwdep->ops = ops;
	hwdep->private_data = memif;
	hwdep->exclusive = true;

	return 0;
}

static int rtk_afe_component_probe(struct snd_soc_component *component)
{
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(component);
	struct rtk_afe_dai *dai;

	list_for_each_entry(dai, &afe->sub_dais, list) {
		if (dai->controls)
			snd_soc_add_component_controls(component,
						       dai->controls,
						       dai->num_controls);

		if (dai->dapm_widgets)
			snd_soc_dapm_new_controls(&component->dapm,
						  dai->dapm_widgets,
						  dai->num_dapm_widgets);
	}
	/* add routes after all widgets are added */
	list_for_each_entry(dai, &afe->sub_dais, list) {
		if (dai->dapm_routes)
			snd_soc_dapm_add_routes(&component->dapm,
						dai->dapm_routes,
						dai->num_dapm_routes);
	}

	snd_soc_dapm_new_widgets(component->dapm.card);

	return 0;
}

static int rtk_afe_pcm_new(struct snd_soc_component *component,
			   struct snd_soc_pcm_runtime *rtd)
{
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	struct rtk_afe_memif *memif = afe->dai_priv[cpu_dai->id];
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_substream *substream;
	size_t size;

	size = afe->rtk_afe_capture_hardware->buffer_bytes_max;
	substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (substream)
		snd_pcm_set_managed_buffer(substream, SNDRV_DMA_TYPE_DEV_WC_SG,
				afe->dev, size, size);
	rtk_snd_create_hwdep(pcm, memif);

	return 0;
}

static snd_pcm_uframes_t rtk_afe_pcm_pointer(struct snd_soc_component *component,
					     struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(component);
	struct rtk_afe_memif *memif = &afe->memif[snd_soc_rtd_to_cpu(rtd, 0)->id];
	struct ringbuf_header_ptrs *header = memif->ringbuf->hdrs[0].ptrs;
	struct snd_pcm_runtime *runtime = substream->runtime;
	snd_pcm_uframes_t read_addsize = 0;
	unsigned int hw_ringrp;
	snd_pcm_uframes_t ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		hw_ringrp = *header->p_read_ptr;
		memif->hw_ptr = bytes_to_frames(runtime,
					(size_t)(hw_ringrp - *header->p_begin_addr));
		if (memif->hw_ptr != memif->prehw_ptr) {
			read_addsize = ring_valid_data(0, runtime->buffer_size,
						       memif->prehw_ptr, memif->hw_ptr);

			memif->total_read = ring_add(0, runtime->boundary,
						     memif->total_read, read_addsize);
		}
		memif->prehw_ptr = memif->hw_ptr;
		ret = memif->total_read % runtime->buffer_size;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		ret = memif->total_write % runtime->buffer_size;
	}
	return ret;
}

static int rtk_afe_pcm_ack(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct rtk_afe_priv *afe = snd_soc_component_get_drvdata(component);
	struct rtk_afe_memif *memif = &afe->memif[snd_soc_rtd_to_cpu(rtd, 0)->id];
	struct ringbuf_header_ptrs *header = memif->ringbuf->hdrs[0].ptrs;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int period_count = 0;
	unsigned int hw_ring_free_size;
	unsigned int hw_ring_free_frame;
	unsigned int hw_ringrp;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return 0;

	hw_ringrp = *header->p_read_ptr;

	/* update wp (the pointer application send data to alsa) */
	period_count = ring_valid_data(0, runtime->boundary,
				       memif->total_write,
				       runtime->control->appl_ptr) / runtime->period_size;

	/* Check the buffer available size between alsa and FW */
	hw_ring_free_size =
		valid_free_size(*header->p_begin_addr,
				*header->p_begin_addr + *header->p_size,
				hw_ringrp,
				*header->p_write_ptr);

	hw_ring_free_frame = bytes_to_frames(runtime, hw_ring_free_size);

	if ((runtime->period_size * period_count) > hw_ring_free_frame)
		period_count = hw_ring_free_frame / runtime->period_size;

	if (hw_ring_free_size <= frames_to_bytes(runtime, runtime->period_size))
		period_count = 0;

	if (period_count) {
		*header->p_write_ptr =
			ring_add(*header->p_begin_addr,
				 *header->p_begin_addr + *header->p_size,
				 *header->p_write_ptr,
				 frames_to_bytes(runtime,
				 runtime->period_size * period_count));
		memif->total_write =
			ring_add(0, runtime->boundary,
				 memif->total_write,
				 runtime->period_size * period_count);
	}
	return 0;
}

static int rtk_afe_pcm_mmap(struct snd_soc_component *component, struct snd_pcm_substream *substream,
                            struct vm_area_struct *area)
{
	struct snd_dma_buffer *dmab = substream->runtime->dma_buffer_p;
	struct device *dev = dmab->dev.dev;
	dma_addr_t daddr = dmab->addr;
	void *vaddr = dmab->area;
	size_t size = area->vm_end - area->vm_start;

	if (vaddr)
		return dma_mmap_coherent(dev, area, vaddr, daddr, size);

	return 0;
}

static const struct snd_soc_component_driver rtk_afe_component = {
	.name = AFE_PCM_NAME,
	.probe = rtk_afe_component_probe,
	.pcm_construct = rtk_afe_pcm_new,
	.pointer = rtk_afe_pcm_pointer,
	.ack = rtk_afe_pcm_ack,
	.mmap = rtk_afe_pcm_mmap,
};

static int rtk_afe_combine_sub_dai(struct rtk_afe_priv *afe)
{
	struct rtk_afe_dai *dai;
	size_t num_dai_drivers = 0, dai_idx = 0;

	/* calcualte total dai driver size */
	list_for_each_entry(dai, &afe->sub_dais, list) {
		num_dai_drivers += dai->num_dai_drivers;
	}

	dev_info(afe->dev, "%s(), num of dai %zd\n", __func__, num_dai_drivers);

	/* combine sub_dais */
	afe->num_dai_drivers = num_dai_drivers;
	afe->dai_drivers = devm_kcalloc(afe->dev,
					num_dai_drivers,
					sizeof(struct snd_soc_dai_driver),
					GFP_KERNEL);
	if (!afe->dai_drivers)
		return -ENOMEM;

	list_for_each_entry(dai, &afe->sub_dais, list) {
		/* dai driver */
		memcpy(&afe->dai_drivers[dai_idx],
		       dai->dai_drivers,
		       dai->num_dai_drivers *
		       sizeof(struct snd_soc_dai_driver));
		dai_idx += dai->num_dai_drivers;
	}

	return 0;
}

/* todo probe function review */
static int rtk_afe_pcm_probe(struct platform_device *pdev)
{
	struct rtk_afe_priv *afe;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct rtk_krpc_ept_info *hifi_ept;
	dma_addr_t paddr;
	void *vaddr;
	int cache_cfg, i, ret;
	afe = devm_kzalloc(dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	afe->memif = devm_kcalloc(dev, RTK_DAI_MEMIF_NUM, sizeof(*afe->memif),
				  GFP_KERNEL);
	if (!afe->memif)
		return -ENOMEM;

	platform_set_drvdata(pdev, afe);
	afe->data = of_device_get_match_data(dev);
	afe->dev = dev;

	/* prepare kernel rpc */
	hifi_ept = of_krpc_ept_info_get(np, 0);
	if (IS_ERR(hifi_ept))
		return dev_err_probe(dev, PTR_ERR(hifi_ept),
				     "fail to get HIFI krpc ept: 0x%lx\n",
				     PTR_ERR(hifi_ept));

	set_dma_ops(dev, &rheap_dma_ops);

	rheap_setup_dma_pools(dev, "rtk_media_heap", RTK_ACPU_FLAGS, __func__);
	vaddr = dma_alloc_coherent(dev, SZ_4K, &paddr, GFP_KERNEL);
	if (!vaddr)
		return -ENOMEM;

	afe->rpc_priv.ept = hifi_ept;
	afe->rpc_priv.paddr = paddr;
	afe->rpc_priv.vaddr = vaddr;

	rpc_ept_init(&afe->rpc_priv);

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < ARRAY_SIZE(dai_register_cbs); i++) {
		ret = dai_register_cbs[i](afe);
		if (ret) {
			dev_warn(afe->dev, "dai register i %d fail, ret %d\n",
				 i, ret);
			goto err_pm_disable;
		}
	}

	/* init dai_driver and component_driver */
	ret = rtk_afe_combine_sub_dai(afe);
	if (ret) {
		dev_warn(afe->dev, "rtk_afe_combine_sub_dai fail, ret %d\n",
			 ret);
		goto err_pm_disable;
	}

	afe->rtk_afe_hardware = &rtk_afe_hardware;
	afe->rtk_afe_capture_hardware = &rtk_afe_capture_hardware;

	rpc_get_chache_config(&afe->rpc_priv,&cache_cfg);

	if (cache_cfg == ENUM_MEMORY_CFG_ALL_CACHE)
		afe->fw_mem_cfg = 1;
	else if (cache_cfg == ENUM_MEMORY_CFG_WITH_UNCACHE)
		afe->fw_mem_cfg = 0;
	else {
		afe->fw_mem_cfg = 0;
		dev_err(dev,"UNKNOW FW memory config %d\n",cache_cfg);
		//goto err_pm_disable;
	}

	ret = devm_snd_soc_register_component(dev, &rtk_afe_component,
					      afe->dai_drivers,
					      afe->num_dai_drivers);
	if (ret) {
		dev_warn(dev, "err register dai component\n");
		goto err_pm_disable;
	}

	return 0;
err_pm_disable:
//	pm_runtime_disable(&pdev->dev);

	return ret;
}

static void rtk_afe_pcm_remove(struct platform_device *pdev)
{
	struct rtk_afe_priv *afe = platform_get_drvdata(pdev);

	dma_free_coherent(&pdev->dev, SZ_4K,
			  afe->rpc_priv.vaddr, afe->rpc_priv.paddr);
}

static const struct of_device_id rtk_afe_pcm_dt_match[] = {
	{ .compatible = "realtek,rtd1920s-hifi-afe", .data = &kent_afe },
	{ .compatible = "realtek,kent-2ao-afe", .data = &kent_2ao_afe },
	{ .compatible = "realtek,rtd1325-hifi-afe", .data = &morbius_afe },
	{ .compatible = "realtek,audio-out", .data = &morbius_afe},
	{ .compatible = "realtek,rtd1619b-hifi-afe", .data = &stark_afe },
	{},
};
MODULE_DEVICE_TABLE(of, rtk_afe_pcm_dt_match);

static struct platform_driver rtk_afe_pcm_driver = {
	.driver = {
		   .name = "rtk-hifi-afe",
		   .of_match_table = rtk_afe_pcm_dt_match,
	},
	.probe = rtk_afe_pcm_probe,
	.remove = rtk_afe_pcm_remove,
};

module_platform_driver(rtk_afe_pcm_driver);

MODULE_DESCRIPTION("Realtek ALSA SoC AFE platform driver");
MODULE_AUTHOR("Simon Hsu <simon_hsu@realtek.com>");
MODULE_LICENSE("GPL v2");
