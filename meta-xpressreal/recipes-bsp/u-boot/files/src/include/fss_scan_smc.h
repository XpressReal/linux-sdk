#ifndef __FSS_SCAN_SMC_H
#define __FSS_SCAN_SMC_H

#define FSS_SCAN_SMC_ERROR    -1

int fss_scan_smc(void);
int fss_scan_smc_update_fdt(void);
void fss_scan_smc_main(void);

void fsmc_set_fdt_addr(void* addr);

#endif /* __FSS_SCAN_SMC_H */
