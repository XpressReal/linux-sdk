/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Realtek Semiconductor Corp.
 */

#ifndef SYSDBG_SYSFS_H
#define SYSDBG_SYSFS_H

#ifdef __cplusplus
extern "C" {
#endif

int sysdbg_sysfs_create(unsigned int hw_ver);
void sysdbg_sysfs_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSDBG_SYSFS_H */
