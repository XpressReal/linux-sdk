#include <linux/dma-buf.h>
#include <soc/realtek/rtk_media_heap.h>
#include <soc/realtek/memory.h>
#include "common.h"

int rtk_aio_ctrl_alloc_buf(struct device *dev, struct rtk_aio_ctrl_buf *buf)
{
	size_t size = PAGE_SIZE;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	int ret;

	dmabuf = rheap_alloc("rtk_audio_heap", size,
		RTK_FLAG_NONCACHED | RTK_FLAG_SCPUACC | RTK_FLAG_ACPUACC);
	if (IS_ERR_OR_NULL(dmabuf))
		return -ENOMEM;
	dma_buf_set_name(dmabuf, __func__);


	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		dev_err(dev, "failed to attach dma-buf: %d\n", ret);
		goto put_dma_buf;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		dev_err(dev, "failed to map attachment: %d\n", ret);
		goto detach_dma_buf;
	}

	if (sgt->nents != 1) {
		ret = -EINVAL;
		dev_err(dev, "scatter list not supportted\n");
		goto unmap_attachment;
	}

	dma_addr = sg_dma_address(sgt->sgl);
	if (!dma_addr) {
		ret = -ENOMEM;
		dev_err(dev, "invalid dma address\n");
		goto unmap_attachment;
	}

	buf->buf = dmabuf;
	buf->attach = attach;
	buf->sgt = sgt;
	buf->dma = dma_addr;
	buf->size = size;

	return 0;

unmap_attachment:
	dma_buf_unmap_attachment(attach, sgt, DMA_TO_DEVICE);
detach_dma_buf:
	dma_buf_detach(dmabuf, attach);
put_dma_buf:
	dma_buf_put(dmabuf);

	return ret;
}

void rtk_aio_ctrl_free_buf(struct rtk_aio_ctrl_buf *buf)
{
	dma_buf_unmap_attachment(buf->attach, buf->sgt, DMA_TO_DEVICE);
	dma_buf_detach(buf->buf, buf->attach);
	dma_buf_put(buf->buf);
}

int rtk_aio_ctrl_copy_to_buf(struct rtk_aio_ctrl_buf *buf, void *data, size_t size)
{
	void *virt;

	if (size > buf->size)
		return -EINVAL;

	dma_buf_begin_cpu_access(buf->buf, DMA_BIDIRECTIONAL);
	virt = dma_buf_vmap(buf->buf);
	if (!virt)
		return -ENOMEM;
	memcpy(virt, data, size);
	dma_buf_vunmap(buf->buf, virt);
	dma_buf_end_cpu_access(buf->buf, DMA_BIDIRECTIONAL);
	return 0;
}
