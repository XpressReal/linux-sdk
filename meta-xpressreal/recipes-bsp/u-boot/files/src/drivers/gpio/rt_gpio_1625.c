#include "rt_gpio_1625.h"
#include <common.h>
#include <linux/delay.h>
#include <asm/arch/platform_lib/board/gpio.h>
#include <asm/arch/io.h>
#include <asm/arch/rbus/misc_reg.h>
#include <asm/arch/rbus/iso_reg.h>

#define GPIO_BASE 0x98031100
#define GPIO_DIR_OFFSET 0x0
#define GPIO_DATO_OFFSET 0x2
#define GPIO_DATI_OFFSET 0x4

static void set_pconf_pullsel(unsigned int GPIO_NUM, unsigned char pull_sel)
{
	unsigned int regAddr,val,mask;

	if(pin_regmap[GPIO_NUM].pcof_regoff==PCOF_UNSUPPORT)
	{
		printf("[set_pconf_pullsel] GPIO#%d pin not support pull select\n",GPIO_NUM);
		return;
	}

	regAddr = pin_regmap[GPIO_NUM].pmux_base+pin_regmap[GPIO_NUM].pcof_regoff;
	mask = 0x3<<pin_regmap[GPIO_NUM].pcof_regbit;// 2bit mask

	val = rtd_inl((volatile unsigned int *)(uintptr_t)regAddr);
	switch(pull_sel)
	{
		case PULL_DISABLE:
			val = (val & ~mask);
			break;
		case PULL_DOWN:
			val = (val & ~mask)|(0x1<<pin_regmap[GPIO_NUM].pcof_regbit);
			break;
		case PULL_UP:
			val = (val & ~mask)|(0x3<<pin_regmap[GPIO_NUM].pcof_regbit);
			break;
	}

	rtd_outl((volatile unsigned int *)(uintptr_t)regAddr,val);
}

static void set_gpio_pinmux(int GPIO_NUM)
{
	unsigned int regAddr,val,mask;

	if((GPIO_NUM < 0) || (GPIO_NUM >= GPIO_TOTAL))
	{
		printf("[%s] No GPIO#%d pin\n",__FUNCTION__,GPIO_NUM);
		return;
	}
	else if(pin_regmap[GPIO_NUM].pmux_regoff==PMUX_UNSUPPORT)
		return;

	// Get pinmux address and mask
	regAddr = pin_regmap[GPIO_NUM].pmux_base+pin_regmap[GPIO_NUM].pmux_regoff;
	mask =	pin_regmap[GPIO_NUM].pmux_regbitmsk << pin_regmap[GPIO_NUM].pmux_regbit;

	// Get original pinmux setting
	val = rtd_inl((volatile unsigned int *)(uintptr_t)regAddr);
	val = val & (~mask);//All GPIO pinmux value is 0

	rtd_outl((volatile unsigned int *)(uintptr_t)regAddr,val);
}

/*======================================================================
 * Func : getISOGPIO
 *
 * Desc : Set ISO GPIO as input and get input value
 *
 * Parm : ISOGPIO_NUM: ISO GPIO number
 *
 * Retn : 0 (Low) / 1 (High)/ -1(Fail)
 *======================================================================*/
int getISOGPIO(int ISOGPIO_NUM)
{
	unsigned int regAddr = GPIO_BASE + ISOGPIO_NUM * 0x4;
	int regValue;

	if (ISOGPIO_NUM < 0 || ISOGPIO_NUM >= GPIO_TOTAL) {
		printf("%s: GPIO number(%d) out of ranges\n", __func__, ISOGPIO_NUM);
		return -1;
	}

	set_gpio_pinmux(ISOGPIO_NUM);
	udelay(1000);

	regValue = 0x2 << GPIO_DIR_OFFSET;
	rtd_outl((volatile unsigned int *)(uintptr_t)regAddr, regValue);

	regValue = rtd_inl((volatile unsigned int *)(uintptr_t)regAddr) & (1 << GPIO_DATI_OFFSET);
	if (regValue)
		return 1;
	else
		return 0;
}


/*======================================================================
 * Func : setISOGPIO
 *
 * Desc : Set ISO GPIO as output and assign ouput level
 *
 * Parm : ISOGPIO_NUM: ISO GPIO number
 *            value: 0 (Output Low)/ 1 (Output High)
 *
 * Retn : 0 (OK) / -1 (FAIL)
 *======================================================================*/
int setISOGPIO(int ISOGPIO_NUM, int value)
{
	unsigned int regAddr = GPIO_BASE + ISOGPIO_NUM * 0x4;
	int regValue;

	if (ISOGPIO_NUM < 0 || ISOGPIO_NUM >= GPIO_TOTAL) {
		printf("%s: GPIO number(%d) out of ranges\n", __func__, ISOGPIO_NUM);
		return -1;
	}

	set_gpio_pinmux(ISOGPIO_NUM);
	udelay(1000);

	if (value)
		regValue = 0x3 << GPIO_DATO_OFFSET;
	else
		regValue = 0x2 << GPIO_DATO_OFFSET;

	rtd_outl((volatile unsigned int *)(uintptr_t)regAddr, regValue);

	regValue = 0x3 << GPIO_DIR_OFFSET;
	rtd_outl((volatile unsigned int *)(uintptr_t)regAddr, regValue);

	return 0;
}

/*======================================================================
 * Func : setISOGPIO_pullsel
 *
 * Desc : Set ISO GPIO pull disable/down/up
 *
 * Parm : ISOGPIO_NUM: ISO GPIO number(0~56)
 *            pull_sel: 0 (Pull Disable)/ 1 (Pull Down)/ 2 (Pull Up)
 *
 * Retn : 0 (OK) / -1 (FAIL)
 *======================================================================*/
int setISOGPIO_pullsel(int ISOGPIO_NUM, unsigned char pull_sel)
{
	if((ISOGPIO_NUM<0)||(ISOGPIO_NUM>=GPIO_TOTAL))
	{
		printf("[%s] No ISOGPIO#%d pin\n",__FUNCTION__,ISOGPIO_NUM);
		return -1;
	}
	else if(pull_sel>2)
	{
		printf("[%s] Invalid pull_sel value, ISOGPIO#%d\n",__FUNCTION__,ISOGPIO_NUM);
		return -1;
	}

	set_pconf_pullsel(ISOGPIO_NUM, pull_sel);
	return 0;
}


int getGPIO(int GPIO_NUM){
	return -1;
}
int setGPIO(int GPIO_NUM, int value){
	return -1;
}
int setGPIO_pullsel(unsigned int GPIO_NUM, unsigned char pull_sel){
	return -1;
}


