#include <common.h>
#include <linux/arm-smccc.h>
#include <cpu_func.h>
#include <command.h>
#include <linux/compiler.h>
#include <linux/psci.h>
#include <malloc.h>
#include <usb.h>

#define PWR_KEY_IGPIO 26

enum _suspend_wakeup {
	eWAKEUP_ON_LAN,
	eWAKEUP_ON_IR,
	eWAKEUP_ON_GPIO,
	eWAKEUP_ON_ALARM,
	eWAKEUP_ON_TIMER,
	eWAKEUP_ON_MAX,
};

enum _wakeup_flags {
	fWAKEUP_ON_LAN          = 0x1U << eWAKEUP_ON_LAN,
	fWAKEUP_ON_IR           = 0x1U << eWAKEUP_ON_IR,
	fWAKEUP_ON_GPIO         = 0x1U << eWAKEUP_ON_GPIO,
	fWAKEUP_ON_ALARM        = 0x1U << eWAKEUP_ON_ALARM,
	fWAKEUP_ON_TIMER        = 0x1U << eWAKEUP_ON_TIMER,
};

#define SUSPEND_ISO_GPIO_SIZE 86
static struct suspend_param {
	unsigned int wakeup_source;
	unsigned int timerout_val;
	char wu_gpio_en[SUSPEND_ISO_GPIO_SIZE];
	char wu_gpio_act[SUSPEND_ISO_GPIO_SIZE];
	char misc[212];
}__attribute__((packed));


static void cmd_bl31_set_pm_param(unsigned int pcpu_data)
{
	struct arm_smccc_res res;
	arm_smccc_smc(0x8400ff04, pcpu_data, 0, 0, 0, 0, 0, 0, &res);
}

static void cmd_bl31_pcpu_power_off(void)
{
	struct arm_smccc_res res;
	arm_smccc_smc(PSCI_0_2_FN_SYSTEM_OFF, 0, 0, 0, 0, 0, 0, 0, &res);
}

static struct suspend_param *pcpu_data;

static int do_s5(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	/* Send to wake up source to pcpu fw by bl31 */
	pcpu_data = malloc(sizeof(struct suspend_param));
	memset(pcpu_data, 0, sizeof(struct suspend_param));
	pcpu_data->wakeup_source = __swap_32(fWAKEUP_ON_GPIO | fWAKEUP_ON_ALARM);
	//pcpu_data->wakeup_source |= __swap_32(fWAKEUP_ON_TIMER);
	//pcpu_data->timerout_val = __swap_32(5);
	pcpu_data->wu_gpio_en[PWR_KEY_IGPIO] = 1;
	pcpu_data->wu_gpio_act[PWR_KEY_IGPIO] = 0;
	flush_cache((unsigned int)(uintptr_t)pcpu_data, sizeof(struct suspend_param));

	cmd_bl31_set_pm_param((unsigned int)(uintptr_t)pcpu_data);
	cmd_bl31_pcpu_power_off();

	return 0;
}

U_BOOT_CMD(
	s5,	CONFIG_SYS_MAXARGS,		1,	do_s5,
	"enter s5",
	""
);
