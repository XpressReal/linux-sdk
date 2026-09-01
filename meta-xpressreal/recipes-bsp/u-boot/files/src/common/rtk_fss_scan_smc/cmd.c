#include <common.h>
#include <command.h>
#include <fss_scan_smc.h>

static int do_fss_scan_smc(struct cmd_tbl *cmdtp, int flag, int argc,
			    char *const argv[])
{
	fss_scan_smc_main();
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(fss_scan_smc, 1, 0, do_fss_scan_smc,
	   "run FSS scan via SMC and update OPP table", "");
