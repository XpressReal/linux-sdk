/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2019 by YH Hsieh <yh_hsieh@realtek.com>
 *
 * Time initialization.
 */
#include <common.h>
#include <command.h>
#include <linux/libfdt.h>
#include <mapmem.h>
#include <asm/sections.h>
#include <asm/global_data.h>
#include <usb.h>
#include <rtk/fw_info.h>
#include <mach/system.h>
#if defined(CONFIG_OF_BOARD_SETUP)
#include <asm/arch/ddr.h>
#endif
#include <env.h>
#include <env_internal.h>
#include <asm/arch/io.h>
#include <asm/arch/rbus/crt_sys_reg.h>
#include <asm/arch/platform_lib/board/gpio.h>
#include <asm/arch/cpu.h>
#include <net.h>
#include <fdt_support.h>
#include <image.h>
//#include <asm/arch/mcp.h>

DECLARE_GLOBAL_DATA_PTR;

const char *sysinfo = {
	"Board: Realtek QA Board\n"
};

#if defined(CONFIG_OF_BOARD)
#define CONFIG_SYS_FDT_LOAD_ADDR 0x03000000
void *board_fdt_blob_setup(int *err)
{
	void *fdt_blob = NULL;

	*err = 0;
	/* FDT is at end of image */
	fdt_blob = (ulong *)&_end;

        if (fdt_magic(fdt_blob) != FDT_MAGIC)
		fdt_blob = map_sysmem(CONFIG_SYS_FDT_LOAD_ADDR, 0);

	return fdt_blob;
}
#endif

#if defined(CONFIG_RTK_MMC_DRIVER)
int fdtdec_board_setup(const void *fdt_blob) {
	int ret = 0;
	const bool secure = rtk_is_secure_boot();

	if(FIT_IMAGE_ENABLE_VERIFY && secure)
		if(fdt_find_and_setprop((void *)fdt_blob, "/"FIT_SIG_NODENAME"/key-kernel", FIT_KEY_REQUIRED, "conf", 5, 0))
		ret = fdt_find_and_setprop((void *)fdt_blob, "/"FIT_SIG_NODENAME"/key-dev", FIT_KEY_REQUIRED, "conf", 5, 0);

	return ret;
}
#endif

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf(sysinfo);
	return 0;
}
#endif  /* CONFIG_DISPLAY_BOARDINFO */

void set_fw_share_mem(void)
{
	struct rtk_ipc_shm *p_ipc_shm =  (struct rtk_ipc_shm *) (MIPS_SHARED_MEMORY_ENTRY_ADDR + 0xC4);
	boot_av_info_t *boot_av = (boot_av_info_t *)MIPS_BOOT_AV_INFO_ADDR;

	/* clear share memory and boot av */
        memset(boot_av, 0, sizeof(boot_av_info_t));
	memset(p_ipc_shm, 0, sizeof(*p_ipc_shm));

	/* since MIPS_SHARED_MEMORY_ENTRY_ADDR is 4-byte alignment,
	 * we can use simple assignment to do this
	 */
        p_ipc_shm->pov_boot_av_info = __swap_32(MIPS_BOOT_AV_INFO_ADDR);
		p_ipc_shm->pov_boot_vd_std_ptr = __swap_32(VO_RESOLUTION);

	*(int *)(MIPS_SHARED_MEMORY_ENTRY_ADDR) = __swap_32(0x16803001);
	*(int *)(MIPS_SHARED_MEMORY_ENTRY_ADDR + 4) = __swap_32(MIPS_SHARED_MEMORY_ENTRY_ADDR);

	flush_dcache_all();
}

#if defined(CONFIG_TARGET_RTD1619B)
static void set_hifi_pll_and_ssc_control(void)
{
	/* Skip if AUCPU PLL is set already */
	if (rtd_inl(SYS_PLL_HIFI2) == 0x3)
		return;

	/* Set AUCPU PLL & SSC control:
	 * Set 0x9800_01DC 0x5, PLL OEB=1, RSTB=0, POW=1
	 * Set 0x9800_01DC 0x7, PLL OEB=1, RSTB=1, POW=1
	 * Set 0x9800_06E0 0xC, CKSSC_INV=1, SSC_DIG_RSTB=1, OC_EN=0, SSC_EN=0
	 * Set 0x9800_01D8 (Electrical specification, no need setting in simulation)
	 * Set 0x9800_06E4 0x1C800, 810Mz, SSC_NCODE_T=39, SSC_FCODE=0
	 * Set 0x9800_06E0 0xD, CKSSC_INV=1, SSC_DIG_RSTB=1, OC_EN=1, SSC_EN=0
	 * Set 0x9800_01DC 0x3, PLL OEB=0, RSTB=1, POW=1
	 * Need wait 200us for PLL oscillating
	 */
	rtd_outl(SYS_PLL_HIFI2, 0x00000005);
	rtd_outl(SYS_PLL_HIFI2, 0x00000007);
	rtd_outl(SYS_PLL_SSC_DIG_HIFI0, 0x0000000c);
	/* Set RS value of PLL to increase performance, please refer to Note 2 of
	 * https://wiki.realtek.com/pages/viewpage.action?pageId=136516076
	 */
	rtd_outl(SYS_PLL_HIFI1, 0x02060000);
	/* Set frequency to 486 Mhz */
	rtd_outl(SYS_PLL_SSC_DIG_HIFI1, 0x00010800);
	rtd_outl(SYS_PLL_SSC_DIG_HIFI0, 0x0000000d);
	udelay(200);
	rtd_outl(SYS_PLL_HIFI2, 0x00000003);
}

void disable_hifi_pll(void)
{
	rtd_outl(SYS_PLL_HIFI2, 0x00000004);
}

static int _do_npu_bisr(void)
{
	rtd_outl(SYS_SOFT_RESET1, _BIT19 | _BIT18); // rstn_npu_bist=2'b11
	/* clk_en_aucpu_iso_npu[25:24]=2'b11
	 * - enabled for bisr
	 * clk_en_npu[13:12]=2'b11
	 * - enabled for polling bisr_done(0x98086800[4]) status bit
	 */
	rtd_outl(SYS_CLOCK_ENABLE7, _BIT25 | _BIT24 | _BIT13 | _BIT12);

	/* 50 ms timeout for checking SRAM bist is done */
	#define TIMEOUT_MS 50
	int i;
	for (i = 0; i < TIMEOUT_MS; i++) {
		if((rtd_inl(0x98086800) & _BIT4) == _BIT4)
			break;
		udelay(1000);
	}
	/* Disable clk_en_npu when polling for bisr status bit is done */
	rtd_outl(SYS_CLOCK_ENABLE7, _BIT13);
	return (i == TIMEOUT_MS)? -1: i;
}

void do_npu_bisr(void)
{
	/* clk_en_npu[13:12]=2'b11
	 * - enabled for polling bisr_done(0x98086800[4]) status bit
	 */
	rtd_outl(SYS_CLOCK_ENABLE7, _BIT13 | _BIT12);

	/* if bisr has already done */
	if((rtd_inl(0x98086800) & _BIT4) == _BIT4) {
		printf("[NPU] bisr has been done, bisr done register 0x%x\n", rtd_inl(0x98086800));
		return;
	}
	else {
		printf("[NPU] bisr has not been done, bisr done register 0x%x\n", rtd_inl(0x98086800));
	}

	/* Disable clk_en_npu when polling for bisr status bit is done */
	rtd_outl(SYS_CLOCK_ENABLE7, _BIT13);

	/* NPU sram BISR needs HiFi PLL to be configured
	 * and only supports frequency lower than 500 Mhz
	 * find more details in SW-4886
	 */
	int ret;

	/* if HiFi PLL is set already */
	if (rtd_inl(SYS_PLL_HIFI2) == 0x3) {
		if (rtd_inl(SYS_PLL_SSC_DIG_HIFI1) > 0x10800) {
			printf("[NPU] error: hifi pll freq is higher than 500 Mhz.\n");
		}
		else {
			ret = _do_npu_bisr();
			if (ret < 0)
				printf("[NPU] error: do bisr failed: timeout.\n");
			else
				printf("[NPU] do bisr takes %d ms.\n", ret);
		}
	}
	/* if HiFi PLL is set by us, disabled it when bisr done */
	else {
		set_hifi_pll_and_ssc_control();
		if (rtd_inl(SYS_PLL_SSC_DIG_HIFI1) > 0x10800) {
			printf("[NPU] error: set hifi pll but freq is higher than 500 Mhz.\n");
			disable_hifi_pll();
		}
		else {
			ret = _do_npu_bisr();
			if (ret < 0)
				printf("[NPU] error: do bisr failed: timeout.\n");
			else
				printf("[NPU] set hifi pll and do bisr takes %d ms.\n", ret);

			disable_hifi_pll();
		}
	}
}

#endif

/**
 * @brief board_init
 *
 * @return 0
 */
int board_init(void)
{
	set_fw_share_mem();
#if defined(CONFIG_TARGET_RTD1619B)
	do_npu_bisr();
#endif

	return 0;
}

extern int rtl8168_initialize(struct bd_info *bis);
int board_eth_init(struct bd_info *bis)
{
	rtl8168_initialize(gd->bd);
	return 0;
}

#ifdef CONFIG_MISC_INIT_R
#if defined(CONFIG_TARGET_RTD1619B)
#define RESET_MAGIC		0xaabbcc00
#define RESET_MAGIC_MASK	0xffffff00
#define RESET_ACTION_POWER_BUTTON 0xea020000

typedef enum {
	RESET_ACTION_NO_ACTION = 0,
	RESET_ACTION_FASTBOOT,
	RESET_ACTION_RECOVERY,
	RESET_ACTION_GOLDEN,
	RESET_ACTION_RESCUE,
	RESET_ACTION_FASTBOOTD,
	RESET_ACTION_QUIESCENT,
	RESET_ACTION_WARMBOOT_ABNORMAL,
	RESET_ACTION_COLDBOOT_ABNORMAL,
	RESET_ACTION_KERNEL_PANIC,
	RESET_ACTION_ABNORMAL = 0xff,
}RESET_ACTION;

static const char *get_reboot_reason(void)
{
	unsigned int boot_reason = rtd_inl(PLATFORM_REBOOT_ACTION_ADDR);

	if (boot_reason == RESET_ACTION_POWER_BUTTON)
		return "cold,power-button";

	if ((boot_reason & RESET_MAGIC_MASK) != RESET_MAGIC)
		return "cold";

	switch (boot_reason & ~RESET_MAGIC_MASK) {
		case RESET_ACTION_KERNEL_PANIC:
			return "panic";
		default:
			return "soft";
	}
}
#endif // CONFIG_TARGET_RTD1619B

int misc_init_r(void)
{
	const char *board_name = "";
#if defined(CONFIG_SPI_RTK_SFC) && !defined(CONFIG_RTK_MMC_DRIVER)
	board_name = "#spinor";
#else
	const u32 select = get_rtd1xxx_gpio_select();

	if (!strlen(CONFIG_BOARD_FIT_CONFIG_NAME))
		board_name = (select & 0x1) ? "#rtd1619b-bleedingedge-emmc" : "#rtd1619b-backinblack";
	else
		board_name = "#" CONFIG_BOARD_FIT_CONFIG_NAME;
#endif
	env_set("bootcfg", board_name);
#if CONFIG_IS_ENABLED(USB_STORAGE)
#if !defined(CONFIG_TARGET_RTD1625)
#if CONFIG_USB_BOOT_GPIO_NUM
	setISOGPIO_pullsel(CONFIG_USB_BOOT_GPIO_NUM, PULL_UP);
	if (!getISOGPIO(CONFIG_USB_BOOT_GPIO_NUM))
#endif
	env_set("boot_usb", "1");
#endif // !CONFIG_TARGET_RTD1625
#endif /* USB_STORAGE */

	if(gd->fdt_blob) {
		int noffset1 = fdt_path_offset(gd->fdt_blob, "/"FIT_SIG_NODENAME"/key-prod");
		int noffset2 = fdt_path_offset(gd->fdt_blob, "/"FIT_SIG_NODENAME"/key-dev");
		const void *modulus = (noffset1 > 0) ? fdt_getprop(gd->fdt_blob, noffset1, "rsa,modulus", NULL) : NULL;

		if(!modulus) modulus = (noffset2 > 0) ? fdt_getprop(gd->fdt_blob, noffset2, "rsa,modulus", NULL) : NULL;
		if (modulus) {
			char addr[11];
			//MCP_SHA256_hash_hwpadding(modulus, RSA_SIGNATURE_LENGTH, hash, NULL, 1);
			sprintf(addr, "0x%08x", modulus);
			env_set("rsa_modulus", addr);
		}

#if defined(CONFIG_TARGET_RTD1619B) || defined(CONFIG_TARGET_RTD1625)
		/* no loadables: recovery U-Boot */
		/* override bootcmd in recovery U-Boot */
		if (fdt_subnode_offset(gd->fdt_blob, 0, "fit-images") < 0) {
#ifdef CONFIG_SPI_RTK_SFC
			env_set("bootcfg", "#rescue");
#else
			env_set("bootcmd", "run altbootcmd || ums 1 mmc ${mmcidx}");
#endif
		}
#endif
	}

#if defined(CONFIG_TARGET_RTD1619B)
	printf("reboot_reason: %s\n", get_reboot_reason());
#endif // CONFIG_TARGET_RTD1619B

	return 0;
}
#endif

u32 get_board_rev(void)
{
	uint revision = 0;

	//revision = (uint)simple_strtoul(CONFIG_VERSION, NULL, 16);

	return revision;
}

#if defined(CONFIG_TARGET_RTD1625)
enum env_location env_get_location(enum env_operation op, int prio)
{
	u32 bootmode = rtk_get_bootmode();

	if (prio)
		return ENVL_UNKNOWN;

	switch (bootmode) {
	case 0x3:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_MMC))
			return ENVL_MMC;
		else
			return ENVL_NOWHERE;

	case 0x2:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_SPI_FLASH))
			return ENVL_SPI_FLASH;
		else
			return ENVL_NOWHERE;

	default:
		return ENVL_NOWHERE;
	}
}
#endif

#if defined(CONFIG_OF_BOARD_SETUP)
#if defined(CONFIG_TARGET_RTD1619B)
int bsv_opp_main(void *blob);
#endif
int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret = 0;

#if defined(CONFIG_HDMI_DISABLED)
	if (rtk_fdt_hdmi_is_off(blob))
		set_hdmi_off();
#endif

	if (eth_get_dev_index() >= 0)
		fdt_status_okay_by_alias(blob, "ethernet");
#if defined(CONFIG_TARGET_RTD1619B)
	/* set core power to 812500 */
	ret = run_command_list("pmic apw8886", -1, 0);
	if (ret)
		ret = run_command_list("pmic probe apw8886 0 0x12", -1, 0);
	if (!ret)
		run_command_list("pmic apw8886 dc2_volt set 812500", -1, 0);

	/* Setup fss scan */
	//ret = run_command_list("bsv", -1, 0);
	ret = bsv_opp_main(blob);
	//if (ret)
#endif
	//ret = run_command_list("fss_scan main", -1, 0);
	if (ret)
		printf("failed to apply dynamic opp\n");

	return ret;
}
#endif

#if defined(CONFIG_TARGET_RTD1619B)
#define BOOT_AV_INFO_MAGICNO    0x2452544D
#define BOOT_LOGO_ADDR          0x2F700000
#define BOOT_LOGO_SIZE          0x00900000
#define VO_SECURE_ADDR          BOOT_LOGO_ADDR + BOOT_LOGO_SIZE - 0x100000
#define VO_SECURE_SIZE          0x00005a00
static void bootlogo_image_process(ulong bootlogo_image, size_t bootlogo_size)
{
	if (((u32)bootlogo_image == BOOT_LOGO_ADDR) && (env_get_yesno("bootlogo") == 1)) {
		boot_av_info_t *boot_av = (boot_av_info_t *)MIPS_BOOT_AV_INFO_ADDR;
		boot_av->dwMagicNumber = __swap_32(BOOT_AV_INFO_MAGICNO);
		memset((void *)(uintptr_t)VO_SECURE_ADDR, 0x0, VO_SECURE_SIZE);
		boot_av->vo_secure_addr = __swap_32(VO_SECURE_ADDR);
		boot_av->logo_enable = 1;
		boot_av->logo_addr = __swap_32((u32)bootlogo_image);
		boot_av->logo_addr_2 = __swap_32((u32)bootlogo_image);
		boot_av->src_width = __swap_32((u32)env_get_ulong("src_width", 10, 1920));
		boot_av->src_height = __swap_32((u32)env_get_ulong("src_height", 10, 1080));
		boot_av->dst_width = __swap_32((u32)env_get_ulong("dst_width", 10, 1920));
		boot_av->dst_height = __swap_32((u32)env_get_ulong("dst_height", 10, 1080));
	}
}
U_BOOT_FIT_LOADABLE_HANDLER(IH_TYPE_FIRMWARE, bootlogo_image_process);
#endif
