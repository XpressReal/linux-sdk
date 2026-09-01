/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2016, Linaro Limited
 */
#ifndef TEE_PRIVATE_H
#define TEE_PRIVATE_H

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/tee_core.h>
#include <linux/types.h>

#define TEE_DEVICE_FLAG_REGISTERED	0x1
#define TEE_MAX_DEV_NAME_LEN		32

bool tee_device_get(struct tee_device *teedev);
int tee_shm_get_fd(struct tee_shm *shm);
int tee_shm_init(void);

struct tee_shm *tee_shm_alloc_priv_buf(struct tee_context *ctx, size_t size);
struct tee_shm *tee_shm_alloc_user_buf(struct tee_context *ctx, size_t size);
struct tee_shm *tee_shm_register_user_buf(struct tee_context *ctx, unsigned long addr, size_t length);

void *tee_get_drvdata(struct tee_device *teedev);
void tee_device_put(struct tee_device *teedev);
void tee_device_unregister(struct tee_device *teedev);
void teedev_close_context(struct tee_context *ctx);
void teedev_ctx_get(struct tee_context *ctx);
void teedev_ctx_put(struct tee_context *ctx);

extern struct bus_type rtk_tee_bus_type;
#endif /*TEE_PRIVATE_H*/
