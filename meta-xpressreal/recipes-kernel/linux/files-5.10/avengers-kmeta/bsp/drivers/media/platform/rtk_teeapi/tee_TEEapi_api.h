/* SPDX-License-Identifier: GPL-2.0-or-later*/
/*
 * Realtek TEE API driver
 *
 * Copyright (c) 2023 Realtek Semiconductor Corp.
 */
#ifndef _TEEAPI_H_INCLUDED_
#define _TEEAPI_H_INCLUDED_

int ta_TEEapi_init(struct tee_context **teeapi_ctx, unsigned int *teeapi_tee_session);
int ta_TEEapi_deinit(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session);
int ta_TEEapi_memcpy(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int dstPhysAddr, unsigned int srtPhysAddr, int size);
int ta_TEEapi_memcpy_a7(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int dstPAddr, unsigned char *buf, int size);
int ta_TEEapi_bitstreamprint(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int phy_addr, int size);
int ta_TEEapi_bitstreamout(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int srcPAddr, unsigned char *buf, int size);
int ta_TEEapi_OMX_CC_API(struct tee_context *teeapi_ctx, unsigned int teeapi_tee_session, unsigned int src_addr, unsigned char *dst_addr, unsigned int size, unsigned int codec_type, unsigned int mode);

#endif
