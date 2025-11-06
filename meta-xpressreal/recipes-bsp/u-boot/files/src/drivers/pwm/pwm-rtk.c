// SPDX-License-Identifier: GPL-2.0 OR MIT
/* Realtek pulse-width-modulation controller driver
 *
 * Copyright (C) 2017 Realtek Semiconductor Corporation.
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <pwm.h>
#include <div64.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <command.h>


/* PWM V1 */
#define RTK_ADDR_PWM_V1_OCD	(0x0)
#define RTK_ADDR_PWM_V1_CD	(0x4)
#define RTK_ADDR_PWM_V1_CSD	(0x8)

#define RTK_PWM_V1_OCD_SHIFT	(8)
#define RTK_PWM_V1_CD_SHIFT	(8)
#define RTK_PWM_V1_CSD_SHIFT	(4)

#define RTK_PWM_V1_OCD_MASK	(0xff)
#define RTK_PWM_V1_CD_MASK	(0xff)
#define RTK_PWM_V1_CSD_MASK	(0xf)

/* PWM V2 */
#define RTK_ADDR_PWM0_V2	(0x4)
#define RTK_ADDR_PWM1_V2	(0x8)
#define RTK_ADDR_PWM2_V2	(0xc)
#define RTK_ADDR_PWM3_V2	(0x0)

#define RTK_PWM_V2_OCD_SHIFT	(8)
#define RTK_PWM_V2_CD_SHIFT	(0)
#define RTK_PWM_V2_CSD_SHIFT	(16)

#define RTK_PWM_V2_OCD_MASK	(0xff)
#define RTK_PWM_V2_CD_MASK	(0xff)
#define RTK_PWM_V2_CSD_MASK	(0x1f)

/* PWM V3 */
#define RTK_ADDR_PWM0_V3	(0x0)
#define RTK_ADDR_PWM1_V3	(0x4)
#define RTK_ADDR_PWM2_V3	(0x8)
#define RTK_ADDR_PWM3_V3	(0xc)
#define RTK_ADDR_PWM4_V3	(0x10)
#define RTK_ADDR_PWM5_V3	(0x14)
#define RTK_ADDR_PWM6_V3	(0x18)

#define RTK_PWM_V3_OCD_SHIFT	(8)
#define RTK_PWM_V3_CD_SHIFT	(0)
#define RTK_PWM_V3_CSD_SHIFT	(16)
#define RTK_PWM_V3_EN_SHIFT	(31)

#define RTK_PWM_V3_OCD_MASK	(0xff)
#define RTK_PWM_V3_CD_MASK	(0xff)
#define RTK_PWM_V3_CSD_MASK	(0x1f)

/* COMMON */
#define RTK_PWM_OCD_DEFAULT	(0xff)
#define RTK_PWM_CD_DEFAULT	(0x1)
#define RTK_PWM_CSD_DEFAULT	(0x1)
#define RTK_PWM_FLAGS_PULSE_LOW_ON_CHANGE         (0x1)

#define NUM_PWM			(4)
#define NUM_PWM_V3		(7)
#define RTK_27MHZ		27000000
#define RTK_1SEC_TO_NS		1000000000
#define STR_LEN			(16)

struct rtk_pwm_chip {
	void __iomem		*addr;
	int			base_freq;
	int			num_pwm;
	int			out_freq[NUM_PWM_V3]; /* Hz */
	int			duty_rate[NUM_PWM_V3];
	int			enable[NUM_PWM_V3];
	int			clksrc_div[NUM_PWM_V3];
	int			clkout_div[NUM_PWM_V3];
	int			clk_duty[NUM_PWM_V3];
	int			flags[NUM_PWM_V3];
	int			loc[NUM_PWM_V3];
	u32			isolation_map;
	struct rtk_pwm_data	*data;
};

struct rtk_pwm_data {
	u32 max_num_pwms;
	int ver;
	void (*pwm_update)(int hwpwm);
	void (*pwm_reg_set)(struct rtk_pwm_chip *pc, int hwpwm);
};

static struct udevice *rtk_pwm_dev;


static int set_real_freq_by_target_freq(struct rtk_pwm_chip *pc, int hwpwm, int target_freq, bool fixed_ocd)
{
	int base_freq = pc->base_freq;
	int real_freq;
	int ocd, csd, div, opt_div;
	int min_ocd = 0;
	int max_ocd = 255;
	int min_csd = 0;
	int max_csd = 15;
	int i;

	div = base_freq / target_freq;

	/* find max bit */
	for (i = 0; i < 32; i++) {
		if ((div << i) & BIT(31))
			break;
	}
	csd = (32 - (i + 8)) - 1;
	if (csd > max_csd)
		csd = max_csd;
	else if (csd < min_csd)
		csd = min_csd;

	ocd = (div >> (csd + 1)) - 1;
	if (ocd > max_ocd)
		ocd = max_ocd;
	else if (ocd < min_ocd)
		ocd = min_ocd;

	if (fixed_ocd)
		ocd = pc->clkout_div[hwpwm];

	opt_div = BIT(csd + 1) * (ocd + 1);

	real_freq = base_freq / opt_div;

	pc->clkout_div[hwpwm] = ocd;
	pc->clksrc_div[hwpwm] = csd;
	pc->out_freq[hwpwm] = real_freq;

	return real_freq;
}

int set_real_period_by_target_period(struct rtk_pwm_chip *pc, int hwpwm, int target_period_ns)
{
	int base_ns = RTK_1SEC_TO_NS;
	int real_period_ns;
	int target_freq, real_freq;

	target_freq = base_ns / target_period_ns;
	real_freq = set_real_freq_by_target_freq(pc, hwpwm, target_freq, false);
	real_period_ns = base_ns / real_freq;

	return real_period_ns;
}

static int set_real_freq_by_target_div(struct rtk_pwm_chip *pc, int hwpwm, int clksrc_div, int clkout_div)
{
	int base_freq = pc->base_freq;
	int real_freq, div;

	div = BIT(clksrc_div + 1) * (clkout_div + 1);
	real_freq = base_freq / div;

	pc->clkout_div[hwpwm] = clkout_div;
	pc->clksrc_div[hwpwm] = clksrc_div;

	pc->out_freq[hwpwm] = real_freq;

	return real_freq;
}

static int set_clk_duty(struct rtk_pwm_chip *pc, int hwpwm, int duty_rate)
{
	int clkout_div = pc->clkout_div[hwpwm];
	int cd;

	if (duty_rate < 0) duty_rate = 0;
	if (duty_rate > 100) duty_rate = 100;

	pc->duty_rate[hwpwm] = duty_rate;
	cd = (duty_rate * (clkout_div + 1) / 100) - 1;

	if (cd < 0) cd = 0;
	if (cd > clkout_div) cd = clkout_div;
	pc->clk_duty[hwpwm] = cd;

	return 0;
}

#define ISO_DUMMY1	0x98007054
static void pwm_update_v1(int hwpwm)
{
	u32 value;

	value = readl(ISO_DUMMY1);
	writel(value | (1 << hwpwm), (void __iomem *)ISO_DUMMY1);
}

static void pwm_reg_set_v1(struct rtk_pwm_chip *pc, int hwpwm)
{
	u32 value;
	u32 old_val;
	int clkout_div = 0;
	int clk_duty = 0;
	int clksrc_div = 0;

	if (pc->enable[hwpwm] && pc->duty_rate[hwpwm]) {
		clkout_div = pc->clkout_div[hwpwm];
		clk_duty = pc->clk_duty[hwpwm];
		clksrc_div = pc->clksrc_div[hwpwm];
	}

	value = readl(pc->addr + RTK_ADDR_PWM_V1_OCD);
	old_val = value;
	value &= ~(RTK_PWM_V1_OCD_MASK << (hwpwm * RTK_PWM_V1_OCD_SHIFT));
	value |= clkout_div << (hwpwm * RTK_PWM_V1_OCD_SHIFT);
	if (value != old_val)
		writel(value, pc->addr + RTK_ADDR_PWM_V1_OCD);

	value = readl(pc->addr + RTK_ADDR_PWM_V1_CD);
	old_val = value;
	value &= ~(RTK_PWM_V1_CD_MASK << (hwpwm * RTK_PWM_V1_CD_SHIFT));
	value |= clk_duty << (hwpwm * RTK_PWM_V1_CD_SHIFT);
	if (value != old_val)
		writel(value, pc->addr + RTK_ADDR_PWM_V1_CD);

	value = readl(pc->addr + RTK_ADDR_PWM_V1_CSD);
	old_val = value;
	value &= ~(RTK_PWM_V1_CSD_MASK << (hwpwm * RTK_PWM_V1_CSD_SHIFT));
	value |= clksrc_div << (hwpwm * RTK_PWM_V1_CSD_SHIFT);
	if (value != old_val)
		writel(value, pc->addr + RTK_ADDR_PWM_V1_CSD);

	if (pc->data->pwm_update)
		pc->data->pwm_update(hwpwm);
}

static void pwm_reg_set_v2(struct rtk_pwm_chip *pc, int hwpwm)
{
	void __iomem *reg;
	u32 value;
	u32 old_val;
	int clkout_div = 0;
	int clk_duty = 0;
	int clksrc_div = 0;

	if (pc->enable[hwpwm] && pc->duty_rate[hwpwm]) {
		clkout_div = pc->clkout_div[hwpwm];
		clk_duty = pc->clk_duty[hwpwm];
		clksrc_div = pc->clksrc_div[hwpwm];
	}

	switch (hwpwm) {
	case 0:
		reg = pc->addr + RTK_ADDR_PWM0_V2;
		break;
	case 1:
		reg = pc->addr + RTK_ADDR_PWM1_V2;
		break;
	case 2:
		reg = pc->addr + RTK_ADDR_PWM2_V2;
		break;
	case 3:
		reg = pc->addr + RTK_ADDR_PWM3_V2;
		break;
	default:
		dev_err(rtk_pwm_dev, "invalid index: PWM %d\n", hwpwm);
		return;
	}

	old_val = readl(reg);
	value = ((clksrc_div & RTK_PWM_V2_CSD_MASK) << RTK_PWM_V2_CSD_SHIFT) |
		((clkout_div & RTK_PWM_V2_OCD_MASK) << RTK_PWM_V2_OCD_SHIFT) |
		((clk_duty & RTK_PWM_V2_CD_MASK) << RTK_PWM_V2_CD_SHIFT);
	if (value != old_val)
		writel(value, reg);
}

static void pwm_reg_set_v3(struct rtk_pwm_chip *pc, int hwpwm)
{
	void __iomem *reg;
	u32 value;
	u32 old_val;
	int clkout_div = 0;
	int clk_duty = 0;
	int clksrc_div = 0;

	clkout_div = pc->clkout_div[hwpwm];
	clk_duty = pc->clk_duty[hwpwm];
	clksrc_div = pc->clksrc_div[hwpwm];

	switch (hwpwm) {
	case 0:
		reg = pc->addr + RTK_ADDR_PWM0_V3;
		break;
	case 1:
		reg = pc->addr + RTK_ADDR_PWM1_V3;
		break;
	case 2:
		reg = pc->addr + RTK_ADDR_PWM2_V3;
		break;
	case 3:
		reg = pc->addr + RTK_ADDR_PWM3_V3;
		break;
	case 4:
		reg = pc->addr + RTK_ADDR_PWM4_V3;
		break;
	case 5:
		reg = pc->addr + RTK_ADDR_PWM5_V3;
		break;
	case 6:
		reg = pc->addr + RTK_ADDR_PWM6_V3;
		break;
	default:
		dev_err(rtk_pwm_dev, "invalid index: PWM %d\n", hwpwm);
		return;
	}

	old_val = readl(reg);
	value = ((clksrc_div & RTK_PWM_V3_CSD_MASK) << RTK_PWM_V3_CSD_SHIFT) |
		((clkout_div & RTK_PWM_V3_OCD_MASK) << RTK_PWM_V3_OCD_SHIFT) |
		((clk_duty & RTK_PWM_V3_CD_MASK) << RTK_PWM_V3_CD_SHIFT) |
		(!!pc->enable[hwpwm] << RTK_PWM_V3_EN_SHIFT);
	if (value != old_val)
		writel(value, reg);

}
static void pwm_set_register(struct rtk_pwm_chip *pc, int hwpwm)
{
	if (pc == NULL) {
		printf("parameter error! pc == NULL\n");
		return;
	} else if (pc->isolation_map & BIT(hwpwm)) {
		printf("PWM %d can't be set because it is isloated\n", hwpwm);
		return;
	}

	pc->data->pwm_reg_set(pc, hwpwm);
}

int pwm_get_freq(int pwm_number, unsigned int* freq)
{
	struct rtk_pwm_chip *pc;

	if (!rtk_pwm_dev) {
		printf("No PWM device is found!\n");
		return -ENODEV;
	}
	pc = dev_get_priv(rtk_pwm_dev);

	if (pwm_number < 0 || pwm_number >= pc->num_pwm) {
		dev_err(rtk_pwm_dev, "invalid pwm_number=%d (0~%d)\n", pwm_number, pc->num_pwm - 1);
		return -EINVAL;
	}
	if (pc->enable[pwm_number]) {
		*freq = pc->out_freq[pwm_number];
	} else {
		*freq = 0;
	}

	return 0;
}

int __pwm_set_freq(int pwm_number, int freq, bool fixed_ocd)
{
	struct rtk_pwm_chip *pc;

	if (!rtk_pwm_dev) {
		printf("No PWM device is found!\n");
		return -ENODEV;
	}
	pc = dev_get_priv(rtk_pwm_dev);

	if (pwm_number < 0 || pwm_number >= pc->num_pwm) {
		dev_err(rtk_pwm_dev, "invalid pwm_number=%d (0~%d)\n", pwm_number, pc->num_pwm - 1);
		return -EINVAL;
	}

	if (freq < 1 || freq >= RTK_27MHZ) {
		dev_err(rtk_pwm_dev, "invalid freq=%d (1~27MHz)\n", freq);
		return -EINVAL;
	}

	if( pc->out_freq[pwm_number] == freq ) {
		dev_err(rtk_pwm_dev, "pwm_set_freq : freq value is not changed, skip! \n");
	}
	else {
		set_real_freq_by_target_freq(pc, pwm_number, freq, fixed_ocd);
		/* update CD if OCD is not fixed */
		if (!fixed_ocd)
			set_clk_duty(pc, pwm_number, pc->duty_rate[pwm_number]);
		pwm_set_register(pc, pwm_number);
	}

	return 0;
}

int pwm_set_freq(int pwm_number, unsigned int freq)
{
	return __pwm_set_freq(pwm_number, freq, true);
}

int pwm_get_duty_rate(int pwm_number, unsigned int* duty_rate)
{
	struct rtk_pwm_chip *pc;

	if (!rtk_pwm_dev) {
		printf("No PWM device is found!\n");
		return -ENODEV;
	}
	pc = dev_get_priv(rtk_pwm_dev);

	if (pwm_number < 0 || pwm_number >= pc->num_pwm) {
		dev_err(rtk_pwm_dev, "invalid pwm_number=%d (0~%d)\n", pwm_number, pc->num_pwm - 1);
		return -EINVAL;
	}

	if (pc->enable[pwm_number]) {
		*duty_rate = pc->duty_rate[pwm_number];
	} else {
		*duty_rate = 0;
	}

	return 0;
}

int pwm_set_duty_rate(int pwm_number, unsigned int duty_rate)
{
	struct rtk_pwm_chip *pc;

	if (!rtk_pwm_dev) {
		printf("No PWM device is found!\n");
		return -ENODEV;
	}
	pc = dev_get_priv(rtk_pwm_dev);

	if (pwm_number < 0 || pwm_number >= pc->num_pwm) {
		dev_err(rtk_pwm_dev, "invalid pwm_number=%d (0~%d)\n", pwm_number, pc->num_pwm - 1);
		return -EINVAL;
	}

	if (duty_rate > 100) {
		dev_err(rtk_pwm_dev, "invalid duty_rate=%d (0~100)\n", duty_rate);
		return -EINVAL;
	}

	if (pc->duty_rate[pwm_number] != duty_rate) {
		set_clk_duty(pc, pwm_number, duty_rate);
		pwm_set_register(pc, pwm_number);
	}

	return 0;
}

int pwm_enable(int pwm_number, unsigned int value)
{
	struct rtk_pwm_chip *pc;

	if (!rtk_pwm_dev) {
		printf("No PWM device is found!\n");
		return -ENODEV;
	}
	pc = dev_get_priv(rtk_pwm_dev);

	if (pwm_number < 0 || pwm_number >= pc->num_pwm) {
		dev_err(rtk_pwm_dev, "invalid pwm_number=%d (0~%d)\n", pwm_number, pc->num_pwm - 1);
		return -EINVAL;
	}

	if (value > 3) {
		dev_err(rtk_pwm_dev, "invalid value=%d (0~3)\n", value);
		return -EINVAL;
	}

	pc->enable[pwm_number] = value;
	pwm_set_register(pc, pwm_number);
	return 0;
}

int rtk_pwm_show(void)
{
	int i;
	struct rtk_pwm_chip *pc;
	void __iomem *reg;
	

	if (!rtk_pwm_dev) {
		printf("No PWM device is found!\n");
		return -ENODEV;
	}
	pc = dev_get_priv(rtk_pwm_dev);

	printf("PWM Registers:\n");
	for (i = 0; i < pc->num_pwm; i++) {
		reg = pc->addr + 4 * i;
		printf("\t reg 0x%p = 0x%08x\n", reg, readl(reg));
	}

	for (i = 0; i < pc->num_pwm; i++) {
		printf("\nPWM %d:", i);
		printf("\t enable     %2d, \t out_freq %5d, \t duty_rate %3d\n", pc->enable[i], pc->out_freq[i], pc->duty_rate[i]);
		printf("\t clksrc_div %2d, \t clkout_div %3d, \t clk_duty  %3d\n", pc->clksrc_div[i], pc->clkout_div[i], pc->clk_duty[i]);
	}

	return 0;
}

#if defined(CONFIG_PWM_RTK_CMD)
static int do_pwm(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	uint id = 0, value = 0;
	uint loc = 0;

	id = 0xdeadbeef;
	value = 0xdeadbeef;

	if (argc < 2)
		return CMD_RET_USAGE;

	switch (*argv[1]) {
	case 'i': /* init */
		rtk_pwm_init();
		return 0;

	case 's': /* show */
		rtk_pwm_show();
		return 0;

	case 'e': /* enable */
		if (argc >= 5) {
			id = (uint) simple_strtoul(argv[2], NULL, 10);
			loc = (uint) simple_strtoul(argv[3], NULL, 10);
			value = (uint) simple_strtoul(argv[4], NULL, 10);
			_rtk_pwm_pinmux(id, loc, value);
			if (pwm_enable(id, value) == 0)
				return 0;
		}
		break;

	case 'd': /* duty */
		if (argc >= 4) {
			id = (uint) simple_strtoul(argv[2], NULL, 10);
			value = (uint) simple_strtoul(argv[3], NULL, 10);
			if (pwm_set_duty_rate(id, value) == 0)
				return 0;
		} else if (argc == 3) {
			id = (uint) simple_strtoul(argv[2], NULL, 10);
			if (pwm_get_duty_rate(id, &value) == 0) {
				printf("PWM %d duty rate %d%%\n", id, value);
				return 0;
			}
		}
		break;
	case 'f': /* freq */
		if (argc >= 4) {
			id = (uint) simple_strtoul(argv[2], NULL, 10);
			value = (uint) simple_strtoul(argv[3], NULL, 10);
			if (pwm_set_freq(id, value) == 0)
				return 0;
		} else if (argc == 3) {
			id = (uint) simple_strtoul(argv[2], NULL, 10);
			if (pwm_get_freq(id, &value) == 0) {
				printf("PWM %d output frequency %d HZ\n", id, value);
				return 0;
			}
		}
	}

	printf("\nid 0x%x, value 0x%x\n\n", id, value);

	return CMD_RET_USAGE;
}

U_BOOT_CMD(pwm, 5, 0, do_pwm,
	"Control PWM 0,1,2,3",
	"\t init                      --- set pin mux and PWM initial values\n"
	"\t show                      --- show current setting of PWM 0 ~ 3\n"
	"\t enable <ID> <loc> <1|0>   --- turn on/off PWM <ID> <location>\n"
	"\t duty <ID> [<duty_rate>]   --- get/set duty rate of PWM <ID>\n"
	"\t freq <ID> [<target freq>] --- get/set target frequency of PWM <ID>\n"
	"Parameters:\n"
	"\t ID : <0-3>\n"
	"\t loc : <0-1>\n"
	"\t duty_rate : <0-100>\n"
	"\t freq : <1-52735>\n");
#endif /* CONFIG_PWM_RTK_CMD */

#define PIN_00	0x00
#define PIN_01	0x01
#define PIN_02	0x02
#define PIN_03	0x03
#define PIN_10	0x10
#define PIN_11	0x11
#define PIN_12	0x12
#define PIN_20	0x20
#define PIN_21	0x21
#define PIN_30	0x30
#define PIN_31	0x31
#define PIN_32	0x32
#define PIN_33	0x33
#define PIN_40	0x40
#define PIN_41	0x41
#define PIN_42	0x42
#define PIN_43	0x43
#define PIN_50	0x50
#define PIN_51	0x51
#define PIN_52	0x52
#define PIN_53	0x53
#define PIN_60	0x60
#define PIN_61	0x61
#define PIN_62	0x62
#define PIN_63	0x63

static inline void update_reg(unsigned int addr, unsigned int mask, unsigned int val)
{
	u32 rval;

	rval = readl((uintptr_t)addr);
	rval &= ~mask;
	rval |= (mask & val);
	writel(rval, (uintptr_t)addr);
}

#if defined(CONFIG_TARGET_RTD1319)
#define ISO_MUXPAD2	(0x9804E008)

void _rtk_pwm_pinmux(int id, int loc, int en)
{
	u32 value = 0;
	u32 pin = ((id & 0x3) << 4) | (loc & 0x1);

	switch (pin) {
	case PIN_00: /* gpio_12 */
		update_reg(ISO_MUXPAD2, 0x00000003, en ? 0x00000002 : 0);
		break;
	case PIN_01: /* gpio_20 */
		update_reg(ISO_MUXPAD2, 0x001C0000, en ? 0x00080000 : 0);
		break;
	case PIN_10: /* gpio_13 */
		update_reg(ISO_MUXPAD2, 0x0000000C, en ? 0x00000008 : 0);
		break;
	case PIN_11: /* gpio_21 */
		update_reg(ISO_MUXPAD2, 0x00600000, en ? 0x00400000 : 0);
		break;
	case PIN_20: /* gpio_14 */
		update_reg(ISO_MUXPAD2, 0x00000070, en ? 0x00000020 : 0);
		break;
	case PIN_21: /* gpio_22 */
		update_reg(ISO_MUXPAD2, 0x01800000, en ? 0x01000000 : 0);
		break;
	case PIN_30: /* gpio_15 */
		update_reg(ISO_MUXPAD2, 0x00000380, en ? 0x00000100 : 0);
		break;
	case PIN_31: /* gpio_23 */
		update_reg(ISO_MUXPAD2, 0x06000000, en ? 0x04000000 : 0);
		break;
	}
}
#elif defined(CONFIG_TARGET_RTD1619B)
#define ISO_MUXPAD2	(0x9804E008)
#define ISO_MUXPAD3	(0x9804E00C)
#define ISO_MUXPAD5	(0x9804E014)

void _rtk_pwm_pinmux(int id, int loc, int en)
{
	u32 pin = ((id & 0x3) << 4) | (loc & 0x1);

	switch (pin) {
	case PIN_00: /* gpio_26 */
		update_reg(ISO_MUXPAD3, 0x0000000c, en ? 0x0000000c : 0);
		break;

	case PIN_10: /* gpio_27 */
		update_reg(ISO_MUXPAD3, 0x00000030, en ? 0x00000030 : 0);
		break;

	case PIN_20: /* gpio_28 */
		update_reg(ISO_MUXPAD3, 0x000000c0, en ? 0x000000c0 : 0);
		break;

	case PIN_30: /* gpio_47 */
		update_reg(ISO_MUXPAD5, 0x00000003, en ? 0x00000003 : 0);
		break;

	case PIN_01: /* gpio_20 */
		update_reg(ISO_MUXPAD2, 0x00e00000, en ? 0x00400000 : 0);
		break;

	case PIN_11: /* gpio_21 */
		update_reg(ISO_MUXPAD2, 0x07000000, en ? 0x02000000 : 0);
		break;

	case PIN_21: /* gpio_22 */
		update_reg(ISO_MUXPAD2, 0x18000000, en ? 0x10000000 : 0);
		break;

	case PIN_31: /* gpio_23 */
		update_reg(ISO_MUXPAD2, 0x60000000, en ? 0x40000000 : 0);
		break;
	}
}
#elif defined(CONFIG_TARGET_RTD1625)
#define ISO_MUXPAD1	(0x9804E004)
#define ISO_MUXPAD2	(0x9804E008)
#define ISO_MUXPAD4	(0x9804E010)
#define ISO_MUXPAD6	(0x9804E018)
#define ISO_PFUNC42	(0x9804E188)
#define VE4_MUXPAD0	(0x9814E000)
#define VE4_MUXPAD1	(0x9814E004)
#define VE4_MUXPAD4	(0x9814E010)
#define VE4_MUXPAD5	(0x9814E014)

void __rtk_pwm_pinmux(int id, int loc, int od, int en)
{
	u32 pin = ((id & 0x7) << 4) | (loc & 0x3);

	switch (pin) {
	case PIN_00: /* gpio_136 */
		update_reg(VE4_MUXPAD5, 0x0000f000, en ? 0x0000d000 : 0);
		update_reg(ISO_PFUNC42, 0x00000001, od ? 0x00000001 : 0);
		break;

	case PIN_10: /* gpio_7 */
		update_reg(VE4_MUXPAD0, 0x01e00000, en ? 0x01a00000 : 0);
		update_reg(ISO_PFUNC42, 0x00000002, od ? 0x00000002 : 0);
		break;

	case PIN_20: /* gpio_16 */
		update_reg(VE4_MUXPAD1, 0x000000f0, en ? 0x000000d0 : 0);
		update_reg(ISO_PFUNC42, 0x00000004, od ? 0x00000004 : 0);
		break;

	case PIN_30: /* gpio_19 */
		update_reg(VE4_MUXPAD1, 0x000f0000, en ? 0x000d0000 : 0);
		update_reg(ISO_PFUNC42, 0x00000008, od ? 0x00000008 : 0);
		break;

	case PIN_01: /* gpio_6 */
		update_reg(VE4_MUXPAD0, 0x001f0000, en ? 0x000d0000 : 0);
		update_reg(ISO_PFUNC42, 0x00000010, od ? 0x00000010 : 0);
		break;

	case PIN_11: /* gpio_93 */
		update_reg(VE4_MUXPAD4, 0x0f000000, en ? 0x0d000000 : 0);
		update_reg(ISO_PFUNC42, 0x00000020, od ? 0x00000020 : 0);
		break;

	case PIN_21: /* gpio_95 */
		update_reg(ISO_MUXPAD1, 0x0f000000, en ? 0x0d000000 : 0);
		update_reg(ISO_PFUNC42, 0x00000040, od ? 0x00000040 : 0);
		break;

	case PIN_31: /* gpio_97 */
		update_reg(ISO_MUXPAD2, 0x0000000f, en ? 0x0000000d : 0);
		update_reg(ISO_PFUNC42, 0x00000080, od ? 0x00000080 : 0);
		break;

	case PIN_02: /* gpio_162 */
		update_reg(ISO_MUXPAD6, 0xf0000000, en ? 0xd0000000 : 0);
		update_reg(ISO_PFUNC42, 0x00000100, od ? 0x00000100 : 0);
		break;

	case PIN_03: /* gpio_12 */
		update_reg(VE4_MUXPAD0, 0x1e000000, en ? 0x1a000000 : 0);
		update_reg(ISO_PFUNC42, 0x00000200, od ? 0x00000200 : 0);
		break;

	case PIN_40: /* gpio_52 */
		update_reg(ISO_MUXPAD1, 0x000f0000, en ? 0x000d0000 : 0);
		update_reg(ISO_PFUNC42, 0x00000400, od ? 0x00000400 : 0);
		break;

	case PIN_50: /* gpio_130 */
		update_reg(ISO_MUXPAD4, 0x0001e000, en ? 0x0001a000 : 0);
		update_reg(ISO_PFUNC42, 0x00000800, od ? 0x00000800 : 0);
		break;

	case PIN_60: /* gpio_132 */
		update_reg(VE4_MUXPAD4, 0xf0000000, en ? 0xd0000000 : 0);
		update_reg(ISO_PFUNC42, 0x00001000, od ? 0x00001000 : 0);
		break;

	}
}

void _rtk_pwm_pinmux(int id, int loc, int en)
{
	__rtk_pwm_pinmux(id, loc, 0, en);
}
#endif /* CONFIG_TARGET_RTD1319 | CONFIG_TARGET_RTD1619B */

static int rtk_pwm_probe(struct udevice *dev)
{
	struct rtk_pwm_chip *pc = dev_get_priv(dev);
	char pwm_name[STR_LEN];
	ofnode node;
	ofnode tmp_node;
	int i;

	rtk_pwm_dev = dev;
	pc->data = (struct rtk_pwm_data *)dev_get_driver_data(dev);
	pc->base_freq = RTK_27MHZ;
	pc->addr = dev_read_addr_ptr(dev);
	if (!pc->addr)
		return -EINVAL;
	pc->num_pwm = dev_read_u32_default(dev, "num-pwm", pc->data->max_num_pwms);
	printf("Load RTK PWM driver... total %d PWMs\n", pc->num_pwm);

	for (i = 0; i < pc->num_pwm; i++) {
		snprintf(pwm_name, STR_LEN, "pwm-%d", i);
		node = dev_read_subnode(dev, pwm_name);
		if (!ofnode_valid(node)) {
			dev_err(dev, "could not find [%s] sub-node\n", pwm_name);
			return -EINVAL;
		}

		pc->clkout_div[i] = ofnode_read_u32_default(node, "clkout_div", RTK_PWM_OCD_DEFAULT);

		pc->clksrc_div[i] = ofnode_read_u32_default(node, "clksrc_div", RTK_PWM_CD_DEFAULT);

		set_real_freq_by_target_div(pc, i, pc->clksrc_div[i], pc->clkout_div[i]);

		pc->enable[i] = ofnode_read_u32_default(node, "enable", 0);

		set_clk_duty(pc, i, ofnode_read_u32_default(node, "duty_rate", 50));

		pc->loc[i] = ofnode_read_u32_default(node, "loc", 0);

		tmp_node = ofnode_find_subnode(node, "pulse-low-on-change");
		if (ofnode_valid(tmp_node))
			pc->flags[i] |= RTK_PWM_FLAGS_PULSE_LOW_ON_CHANGE;

		dev_info(dev, "hwpwm=(%d) enable=(%d) duty_rate=(%d) clksrc_div=(%d) clkout_div=(%d)\n",
			 i, pc->enable[i], pc->duty_rate[i], pc->clksrc_div[i], pc->clkout_div[i]);
		dev_info(dev, "defualt output frequency = %dHz\n", pc->out_freq[i]);
	}

	for (i = 0; i < pc->num_pwm; i++) {
		switch (pc->enable[i]) {
		case 0: /* disable */
		case 1: /* enable */
			printf("%s PWM%d at location %d\n", pc->enable[i] ? "enable" : "disable", i, pc->loc[i]);
			_rtk_pwm_pinmux(i, pc->loc[i], 1);
			break;
		case 3: /* isolate */
			/* skip the PWM settings */
			printf("skip PWM%d settings\n", i);
			pc->isolation_map |= BIT(i);
			break;
		default:
			dev_err(dev, "unsupported pwm mode %d\n", pc->enable[i]);
		}

		pwm_set_register(pc, i);
	}

	return 0;
}

int rtk_pwm_init(void)
{
	if (!rtk_pwm_dev) {
		printf("No PWM device is found!\n");
		return -ENODEV;
	}

	return rtk_pwm_probe(rtk_pwm_dev);
}

static int rtk_pwm_set_config(struct udevice *dev, uint channel,
			      uint period_ns, uint duty_ns)
{
	struct rtk_pwm_chip *pc = dev_get_priv(dev);
	int real_period_ns, duty_rate;
	int clkout_div = pc->clkout_div[channel];
	int cd;
	s64 tmp;

	real_period_ns = set_real_period_by_target_period(pc, channel, period_ns);

	tmp = (s64)duty_ns * 100;
	duty_rate = (int)do_div(tmp, period_ns);

	/* improve accuracy */
	pc->duty_rate[channel] = duty_rate;
	cd = (int)DIV_ROUND_CLOSEST_ULL((unsigned long long)duty_ns * (clkout_div + 1),  period_ns) - 1;
	if (cd < 0)
		cd = 0;
	else if (cd > clkout_div)
		cd = clkout_div;
	pc->clk_duty[channel] = cd;

	pwm_set_register(pc, channel);

	return 0;
};

static int rtk_pwm_set_enable(struct udevice *dev, uint channel, bool enable)
{
	pwm_enable(channel, enable);

	return 0;
};

static const struct pwm_ops rtk_pwm_ops = {
	.set_config = rtk_pwm_set_config,
	.set_enable = rtk_pwm_set_enable,
};

static const struct rtk_pwm_data rtk_pwm_v1_data = {
	.max_num_pwms = 4,
	.ver = 1,
	.pwm_update = NULL,
	.pwm_reg_set = pwm_reg_set_v1,
};

static const struct rtk_pwm_data rtk_pwm_v1_ext_data = {
	.max_num_pwms = 4,
	.ver = 1,
	.pwm_update = pwm_update_v1,
	.pwm_reg_set = pwm_reg_set_v1,
};

static const struct rtk_pwm_data rtk_pwm_v2_data = {
	.max_num_pwms = 4,
	.ver = 2,
	.pwm_update = NULL,
	.pwm_reg_set = pwm_reg_set_v2,
};

static const struct rtk_pwm_data rtk_pwm_v3_data = {
	.max_num_pwms = 7,
	.ver = 3,
	.pwm_update = NULL,
	.pwm_reg_set = pwm_reg_set_v3,
};

static const struct udevice_id rtk_pwm_ids[] = {
	{ .compatible = "realtek,rtk-pwm-v1", .data = (ulong)&rtk_pwm_v1_data },
	{ .compatible = "realtek,rtk-16xxb-pwm-v1", .data = (ulong)&rtk_pwm_v1_ext_data },
	{ .compatible = "realtek,rtk-pwm-v2", .data = (ulong)&rtk_pwm_v2_data },
	{ .compatible = "realtek,rtk-pwm-v3", .data = (ulong)&rtk_pwm_v3_data },
	{ /* sentinel */ }
};

static int rtk_pwm_bind(struct udevice *dev)
{
	int ret = 0;

#if CONFIG_IS_ENABLED(DM)
	dev_or_flags(dev, DM_FLAG_PROBE_AFTER_BIND);
	ret = dm_scan_fdt_dev(dev);
#endif /* CONFIG_IS_ENABLED(DM) */
	return ret;
}

U_BOOT_DRIVER(rtk_pwm) = {
	.name = "rtk-pwm",
	.id = UCLASS_PWM,
	.of_match = rtk_pwm_ids,
	.ops = &rtk_pwm_ops,
	.bind = rtk_pwm_bind,
	.probe = rtk_pwm_probe,
	.priv_auto = sizeof(struct rtk_pwm_chip),
};

