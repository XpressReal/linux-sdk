#include <common.h>
#include <env.h>
#include <version.h>
#include <vsprintf.h>
#include "include/fss_scan_smc_sysdeps.h"

#ifdef FSS_SCAN_SMC_DEBUG
int fsmc_debug(const char* format, ...)
{
	va_list arglist;
	int ret;

	printf("(fss_scan_smc) DEBUG: ");

	va_start(arglist, format);
	ret = vprintf(format, arglist);
	va_end(arglist);

	return ret;
}
#endif

int fsmc_printf(const char* format, ...)
{
	va_list arglist;
	int ret;

	printf("(fss_scan_smc) ");

	va_start(arglist, format);
	ret = vprintf(format, arglist);
	va_end(arglist);

	return ret;
}

int fsmc_snprintf(char *s, size_t n, const char *format, ...)
{
	va_list arglist;
	int ret;

	va_start(arglist, format);
	ret = vsnprintf(s, n, format, arglist);
	va_end(arglist);

	return ret;
}

void fsmc_udelay(int usec)
{
	udelay(usec);
}

void fsmc_nvm_store_int(const char *name, int value)
{
	char buf[12];

	snprintf(buf, sizeof(buf), "%d", value);
	env_set(name, buf);
}

int fsmc_nvm_load_int(const char *name, int *value)
{
	const char *val = env_get(name);

	if (!val)
		return -1;
	*value = (int)simple_strtol(val, NULL, 10);
	return 0;
}

void fsmc_nvm_sync(void)
{
	env_save();
}

static void *fdt_loadaddr = NULL;
void fsmc_set_fdt_addr(void* addr)
{
	fdt_loadaddr = addr;
}

void *fsmc_get_fdt_addr(void)
{
	const char *val = env_get("fdt_loadaddr");
	unsigned long addr;

	if (fdt_loadaddr)
		return fdt_loadaddr;

	if (!val)
		return NULL;
	addr = simple_strtoul(val, NULL, 16);
	return (void *)addr;
}

const char *fsmc_get_bootcode_version(void)
{
	return U_BOOT_VERSION;
}
