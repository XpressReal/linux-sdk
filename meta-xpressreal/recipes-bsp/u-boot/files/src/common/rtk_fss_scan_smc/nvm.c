#include <rtk_opp.h>
#include <fss_scan_smc_sysdeps.h>
#include <fss_scan_smc.h>
#include "internal.h"

int fsmc_opp_load(struct rtk_opp_data *data)
{
	int val;
	int i;
	int ret;

	for (i = 600; i <= 2000; i += 100) {
		if (fsmc_nvm_get_voltage(i, &val))
			continue;
		ret = rtk_opp_add_entry(data, i, val);
		if (ret)
			fsmc_printf("fss_scan_smc: failed to add opp <%d %d>\n", i, val);
	}

	fsmc_printf("fss_scan_smc: total opps: %d\n", data->num_entries);
	return data->num_entries == 0 ? FSS_SCAN_SMC_ERROR : 0;
}

void fsmc_nvm_set_verification_data(int val)
{
	fsmc_nvm_store_int("fss_scan_vd", val);
	fsmc_nvm_sync();
}

int fsmc_nvm_get_verification_data(int *val)
{
	return fsmc_nvm_load_int("fss_scan_vd", val);
}

void fsmc_nvm_set_voltage(int freq, int val)
{
	char buf[20];

	fsmc_snprintf(buf, sizeof(buf), "fss_scan_volt_%d", freq);
	fsmc_nvm_store_int(buf, val);
}

int fsmc_nvm_get_voltage(int freq, int *val)
{
	char buf[20];

	fsmc_snprintf(buf, sizeof(buf), "fss_scan_volt_%d", freq);
	return fsmc_nvm_load_int(buf, val);
}

void fsmc_nvm_set_failed_status(int val)
{
	fsmc_nvm_store_int("fss_scan_failed", val);
	fsmc_nvm_sync();
}

int fsmc_nvm_get_failed_status(int *val)
{
	return fsmc_nvm_load_int("fss_scan_failed", val);
}
