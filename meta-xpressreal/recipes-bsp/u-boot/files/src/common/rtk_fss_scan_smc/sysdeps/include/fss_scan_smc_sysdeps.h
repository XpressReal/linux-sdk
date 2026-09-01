#ifndef __SYSDEPS_FSS_SCAN_SMC_SYSDEPS_H
#define __SYSDEPS_FSS_SCAN_SMC_SYSDEPS_H

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)    (sizeof(arr) / sizeof((arr)[0]))
#endif

#include <stddef.h>

#ifdef FSS_SCAN_SMC_DEBUG
int fsmc_debug(const char* format, ...);
#else
#define fsmc_debug(...) do { } while(0)
#endif
int fsmc_printf(const char* format, ...);
int fsmc_snprintf(char *s, size_t n, const char *format, ...);
void fsmc_udelay(int usec);
#define fsmc_mdelay(_msec)  fsmc_udelay((_msec) * 1000)

void fsmc_nvm_store_int(const char *name, int value);
int fsmc_nvm_load_int(const char *name, int *value);
void fsmc_nvm_sync(void);

void *fsmc_get_fdt_addr(void);
const char *fsmc_get_bootcode_version(void);

#endif
