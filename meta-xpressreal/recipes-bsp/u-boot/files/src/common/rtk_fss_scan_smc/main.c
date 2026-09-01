#include <common.h>
#include <malloc.h>
#include <linux/arm-smccc.h>
#include <linux/libfdt.h>
#include <asm/io.h>
#include <fss_scan_smc.h>
#include <fss_scan_smc_sysdeps.h>
#include "internal.h"

#define SIP_CFG_FSS_SCAN            0x82000402
#define SIP_CFG_SET_VOLTAGE_ID      0x82000403
#define FSS_SCAN_SMC_OUTPUT_SIZE    0x280

#ifndef FSS_SCAN_RESTORE_PLATFORM_CPU_VOLTAGE
#define FSS_SCAN_RESTORE_PLATFORM_CPU_VOLTAGE  1050000
#endif

#define NVM_MAX_ENTRIES 16

struct fs_nvm_entry {
	char name[32];
	int value;
	int valid;
};

static struct fs_nvm_entry fs_nvm_data[NVM_MAX_ENTRIES];

static int compute_checksum(unsigned const char *p, int len)
{
	int i, checksum = 0;

	for (i = 0; i < len; i++)
		checksum += *(p + i);
	return checksum;
}

static int compute_checksum_str(unsigned const char *p)
{
	int checksum = 0;

	while (*p)
		checksum += *p++;
	return checksum;
}

static int get_uuid_checksum(void)
{
#ifndef OTP_BASE
#define OTP_BASE 0x98017000
#endif
#define UUID_PART1 (OTP_BASE + 0x478)
#define UUID_PART2 (OTP_BASE + 0x47C)
#define UUID_PART3 (OTP_BASE + 0x480)

	int checksum = 0;
	unsigned int val;

	val = readl(UUID_PART1);
	checksum += compute_checksum((unsigned char *)&val, sizeof(val));
	val = readl(UUID_PART2);
	checksum += compute_checksum((unsigned char *)&val, sizeof(val));
	val = readl(UUID_PART3);
	checksum += compute_checksum((unsigned char *)&val, sizeof(val));
	return checksum;
}

static int calculate_checksum(void)
{
	int checksum = 0;
	int val;

	checksum += get_uuid_checksum();

	if (fsmc_nvm_get_voltage(1200, &val))
		val = -1;
	checksum += compute_checksum((unsigned char *)&val, sizeof(int));

	checksum += compute_checksum_str((const unsigned char *)fsmc_get_bootcode_version());

	return checksum;
}

static int parse_freq_from_name(const char *name)
{
	const char *prefix = "fss_scan_volt_";
	int prefix_len = 14; /* strlen("fss_scan_volt_") */
	int freq = 0;
	int i;

	if (strncmp(name, prefix, prefix_len) != 0)
		return -1;

	name += prefix_len;
	for (i = 0; name[i] >= '0' && name[i] <= '9'; i++) {
		freq = freq * 10 + (name[i] - '0');
	}

	return (name[i] == '\0' && i > 0) ? freq : -1;
}

static int fss_scan_smc_check_and_run(void)
{
	int checksum, previous_checksum = -1;
	int val;
	int ret;

#if FSS_SCAN_RESTORE_PLATFORM_CPU_VOLTAGE
	{
		void *fdt = fsmc_get_fdt_addr();

		if (fdt) {
			struct arm_smccc_res res;
			unsigned int fdt_size = fdt_totalsize(fdt);

			fsmc_printf("INFO: %s: SIP_CFG_SET_VOLTAGE: fdt=0x%08lx size=0x%x volt=%d\n",
				    __func__, (unsigned long)fdt, fdt_size,
				    FSS_SCAN_RESTORE_PLATFORM_CPU_VOLTAGE);
			arm_smccc_smc(SIP_CFG_SET_VOLTAGE_ID,
				      (unsigned long)fdt,
				      fdt_size,
				      FSS_SCAN_RESTORE_PLATFORM_CPU_VOLTAGE,
				      0, 0, 0, 0, &res);
			if (res.a0 != 0)
				fsmc_printf("WARN: %s: SIP_CFG_SET_VOLTAGE returned: %ld\n",
					    __func__, res.a0);
		}
	}
#endif

	if (!fsmc_nvm_get_verification_data(&previous_checksum)) {
		checksum = calculate_checksum();
		if (previous_checksum == checksum)
			return 0;

		fsmc_printf("INFO: %s: checksum not matched(%d,%d), run fss_scan_smc again\n",
			    __func__, previous_checksum, checksum);
	} else {
		if (!fsmc_nvm_get_failed_status(&val) && val == 1) {
			fsmc_printf("ERROR: %s: failed in previous scan!!\n", __func__);
			return FSS_SCAN_SMC_ERROR;
		}
	}

	ret = fss_scan_smc();
	if (ret) {
		fsmc_printf("ERROR: %s: failed in scan\n", __func__);
		fsmc_nvm_set_failed_status(1);
	} else {
		fsmc_nvm_set_failed_status(0);
		fsmc_nvm_set_verification_data(calculate_checksum());
	}

	return ret;
}

static int fss_scan_smc_disabled(void)
{
	int ret;

	ret = fsmc_fdt_constraint();
	if (ret) {
		fsmc_printf("INFO: %s: fdt constraint\n", __func__);
		return 1;
	}

	return 0;
}

void fss_scan_smc_main(void)
{
	int ret;

	if (fss_scan_smc_disabled())
		return;

	ret = fss_scan_smc_check_and_run();
	if (!ret)
		fss_scan_smc_update_fdt();
}

int fss_scan_smc(void)
{
	struct arm_smccc_res res;
	void *dtb_addr;
	unsigned int dtb_size;
	void *output_buf;
	int ret = 0;

	dtb_addr = fsmc_get_fdt_addr();
	if (!dtb_addr) {
		fsmc_printf("ERROR: %s: failed to get fdt address\n", __func__);
		return FSS_SCAN_SMC_ERROR;
	}

	dtb_size = fdt_totalsize(dtb_addr);
	fsmc_printf("INFO: %s: fdt size = 0x%08x\n", __func__, dtb_size);

	output_buf = malloc(FSS_SCAN_SMC_OUTPUT_SIZE);
	if (!output_buf) {
		fsmc_printf("ERROR: %s: failed to allocate output buffer\n", __func__);
		return FSS_SCAN_SMC_ERROR;
	}

	fsmc_printf("INFO: %s: calling SMC with:\n", __func__);
	fsmc_printf("INFO:   fss_scan_id   = 0x%08x\n", SIP_CFG_FSS_SCAN);
	fsmc_printf("INFO:   dtb_addr      = 0x%08lx\n", (unsigned long)dtb_addr);
	fsmc_printf("INFO:   dtb_size      = 0x%08x\n", dtb_size);
	fsmc_printf("INFO:   output_addr   = 0x%08lx\n", (unsigned long)output_buf);
	fsmc_printf("INFO:   output_size   = 0x%08x\n", FSS_SCAN_SMC_OUTPUT_SIZE);

	arm_smccc_smc(SIP_CFG_FSS_SCAN,
		       (unsigned long)dtb_addr,
		       dtb_size,
		       (unsigned long)output_buf,
		       FSS_SCAN_SMC_OUTPUT_SIZE,
		       0, 0, 0, &res);

	fsmc_printf("INFO: %s: SMC result: a0=0x%08lx, a1=0x%08lx, a2=0x%08lx, a3=0x%08lx\n",
		    __func__, res.a0, res.a1, res.a2, res.a3);

	if (res.a0 != 0) {
		fsmc_printf("ERROR: %s: SMC returned error: %ld\n", __func__, res.a0);
		ret = FSS_SCAN_SMC_ERROR;
	} else {
		int i;

		memcpy(fs_nvm_data, output_buf, sizeof(fs_nvm_data));

		for (i = 0; i < NVM_MAX_ENTRIES; i++) {
			if (fs_nvm_data[i].valid) {
				int freq;

				fsmc_printf("INFO: %s: [%d] %s = %d (valid=%d)\n",
					    __func__, i, fs_nvm_data[i].name,
					    fs_nvm_data[i].value, fs_nvm_data[i].valid);

				freq = parse_freq_from_name(fs_nvm_data[i].name);
				if (freq > 0)
					fsmc_nvm_set_voltage(freq, fs_nvm_data[i].value);
			}
		}
		fsmc_nvm_sync();
	}

	free(output_buf);
	return ret;
}

int fss_scan_smc_update_fdt(void)
{
	struct rtk_opp_data data;
	struct rtk_opp_param *param = &data.param;
	void *fdt = fsmc_get_fdt_addr();

	if (!fdt)
		return FSS_SCAN_SMC_ERROR;

	rtk_opp_init_data(&data);

	if (rtk_opp_get_fdt_param(fdt, "fss", param)) {
		fsmc_printf("ERROR: fss_scan_smc: failed to get volt_param\n");
		return FSS_SCAN_SMC_ERROR;
	}

	if (fsmc_opp_load(&data)) {
		fsmc_printf("ERROR: fss_scan_smc: failed to load opp from storage\n");
		return FSS_SCAN_SMC_ERROR;
	}

	if (rtk_opp_update_fdt_table(fdt, "fss", &data)) {
		fsmc_printf("ERROR: fss_scan_smc: failed to update opp table\n");
		return FSS_SCAN_SMC_ERROR;
	}

	if (rtk_opp_mark_fdt_updated(fdt, "fss")) {
		fsmc_printf("ERROR: fss_scan_smc: failed to enable fss opp\n");
		return FSS_SCAN_SMC_ERROR;
	}

	return 0;
}

int fsmc_fdt_constraint(void)
{
	void *fdt = fsmc_get_fdt_addr();
	int offset;
	const void *p;

	if (!fdt)
		return 1;

	offset = fdt_path_offset(fdt, "/");
	if (offset < 0)
		return 1;

	p = fdt_getprop(fdt, offset, "model", NULL);
	if (p && strstr(p, "Rescue"))
		return 1;

	return 0;
}
