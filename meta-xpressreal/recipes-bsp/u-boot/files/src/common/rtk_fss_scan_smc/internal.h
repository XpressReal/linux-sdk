#ifndef __FSS_SCAN_SMC_INTERNAL_H
#define __FSS_SCAN_SMC_INTERNAL_H

#include <rtk_opp.h>

int fsmc_opp_load(struct rtk_opp_data *data);
void fsmc_nvm_set_verification_data(int val);
int fsmc_nvm_get_verification_data(int *val);
void fsmc_nvm_set_voltage(int freq, int val);
int fsmc_nvm_get_voltage(int freq, int *val);
void fsmc_nvm_set_failed_status(int val);
int fsmc_nvm_get_failed_status(int *val);

int fsmc_fdt_constraint(void);

#endif
