#include <drm/drm_drv.h>
#include "rtk_drm_ringbuffer.h"

void rtk_drm_ringbuffer_reset(struct rtk_drm_ringbuffer *rb)
{
	struct rpmsg_device *rpdev = rb->rpdev;

	memset(rb->virt, 0, rb->size);

	rb->shm_ringheader = rb->virt + rb->header_offset;
	rb->shm_ringheader->beginAddr  = cpu_to_rpmsg32(rpdev, (u32)rb->addr);
	rb->shm_ringheader->size       = cpu_to_rpmsg32(rpdev, rb->header_offset);
	rb->shm_ringheader->bufferID   = cpu_to_rpmsg32(rpdev, 1);
	rb->shm_ringheader->writePtr   = rb->shm_ringheader->beginAddr;
	rb->shm_ringheader->readPtr[0] = rb->shm_ringheader->beginAddr;
}

int rtk_drm_ringbuffer_alloc(struct drm_device *drm, struct rpmsg_device *rpdev,
			     struct rtk_drm_ringbuffer *rb, u32 buffer_size)
{
	rb->drm = drm;
	rb->rpdev = rpdev;
	rb->size = buffer_size + RTK_DRM_RINGBUFFER_HEADER_SIZE;
	rb->header_offset = buffer_size;

	rb->virt = dma_alloc_coherent(drm->dev, rb->size, &rb->addr,
				      GFP_KERNEL | __GFP_NOWARN);
	if (!rb->virt)
		return -ENOMEM;

	rtk_drm_ringbuffer_reset(rb);
	return  0;
}

void rtk_drm_ringbuffer_free(struct rtk_drm_ringbuffer *rb)
{
	dma_free_coherent(rb->drm->dev, rb->size, rb->virt, rb->addr);
}

int rtk_drm_ringbuffer_write(struct rtk_drm_ringbuffer *rb, void *cmd, u32 size)
{
	u32 buf_r, buf_w, buf_b, buf_s, buf_l;
	u32 buf_space;
	void *ptr_b, *ptr_w;

	buf_r = rpmsg32_to_cpu(rb->rpdev, rb->shm_ringheader->readPtr[0]);
	buf_w = rpmsg32_to_cpu(rb->rpdev, rb->shm_ringheader->writePtr);
	buf_b = rpmsg32_to_cpu(rb->rpdev, rb->shm_ringheader->beginAddr);
	buf_s = rpmsg32_to_cpu(rb->rpdev, rb->shm_ringheader->size);
	buf_l = buf_b + buf_s;
	buf_space = buf_r + (buf_r > buf_w ? 0 : buf_s) - buf_w;

	if (buf_space < size)
		return -ENOBUFS;

	ptr_b = rb->virt;
	ptr_w = ptr_b + (buf_w - buf_b);

	if (buf_w + size <= buf_l) {
		memcpy_toio(ptr_w, cmd, size);
	} else {
		memcpy_toio(ptr_w, cmd, buf_l - buf_w);
		memcpy_toio(ptr_b, cmd + buf_l - buf_w, size - (buf_l - buf_w));
	}
	buf_w += size;
	if (buf_w >= buf_l)
		buf_w -= buf_s;

	rb->shm_ringheader->writePtr = cpu_to_rpmsg32(rb->rpdev, buf_w);
	return 0;
}

int rtk_drm_ringbuffer_read(struct rtk_drm_ringbuffer *rb, void *data, u32 data_size,
			    bool update_read_ptr)
{
	void *ptr_b, *ptr_r;
	u32 buf_r, buf_w, buf_b, buf_s, buf_l;
	u32 buf_data_size;

	buf_r = rpmsg32_to_cpu(rb->rpdev, rb->shm_ringheader->readPtr[0]);
	buf_w = rpmsg32_to_cpu(rb->rpdev, rb->shm_ringheader->writePtr);
	buf_b = rpmsg32_to_cpu(rb->rpdev, rb->shm_ringheader->beginAddr);
	buf_s = rpmsg32_to_cpu(rb->rpdev, rb->shm_ringheader->size);
	buf_l = buf_b + buf_s;

	buf_data_size = buf_w + (buf_w < buf_r ? buf_s :  0) - buf_r;

	if (buf_data_size < data_size)
		return -ENODATA;

	ptr_b = rb->virt;
	ptr_r = ptr_b + (buf_r - buf_b);

	if (data) {
		if (buf_r + data_size <= buf_l) {
			memcpy_fromio(data, ptr_r, data_size);
		} else {
			memcpy_fromio(data, ptr_r,  buf_l - buf_r);
			memcpy_fromio(data + buf_l - buf_r, ptr_b, data_size - (buf_l - buf_r));
		}
	}

	if (update_read_ptr) {
		buf_r += data_size;
		if (buf_r >= buf_l)
			buf_r -= buf_s;

		rb->shm_ringheader->readPtr[0] = cpu_to_rpmsg32(rb->rpdev, buf_r);
	}
	return 0;
}

