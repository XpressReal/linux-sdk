// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Realtek DHC pin controller driver
 *
 * Copyright (c) 2019 Realtek Semiconductor Corp.
 * Copyright (c) 2017 Andreas Färber
 */

#include <linux/bitops.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "../core.h"
#include "../pinctrl-utils.h"
#include "pinctrl-rtd.h"

struct rtd_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pcdev;
	void __iomem *base;
	struct pinctrl_desc desc;
	const struct rtd_pinctrl_desc *info;
	u32 **save_regs;
};

/* custom pinconf parameters */
#define RTD_P_DRIVE (PIN_CONFIG_END + 1)
#define RTD_N_DRIVE (PIN_CONFIG_END + 2)
#define RTD_D_CYCLE (PIN_CONFIG_END + 3)
#define RTD_INPUT_VOLT (PIN_CONFIG_END + 4)
#define RTD_HVIL (PIN_CONFIG_END + 5)

static const struct pinconf_generic_params rtd_custom_bindings[] = {
	{"realtek,pdrive", RTD_P_DRIVE, 0},
	{"realtek,ndrive", RTD_N_DRIVE, 0},
	{"realtek,dcycle", RTD_D_CYCLE, 0},
	{"realtek,input_volt", RTD_INPUT_VOLT, 0},
	{"realtek,hvil", RTD_HVIL, 0},
};

static int rtd_pinctrl_get_groups_count(struct pinctrl_dev *pcdev)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);

	return data->info->num_groups;
}

static const char *rtd_pinctrl_get_group_name(struct pinctrl_dev *pcdev,
		unsigned int selector)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);

	return data->info->groups[selector].name;
}

static int rtd_pinctrl_get_group_pins(struct pinctrl_dev *pcdev,
		unsigned int selector, const unsigned int **pins, unsigned int *num_pins)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);

	*pins		= data->info->groups[selector].pins;
	*num_pins	= data->info->groups[selector].num_pins;

	return 0;
}

static void rtd_pinctrl_dbg_show(struct pinctrl_dev *pcdev,
					struct seq_file *s, unsigned offset)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);
	const struct rtd_pin_desc *mux = &data->info->muxes[offset];
	const struct rtd_pin_mux_desc *func;
	u32 val;
	u32 mask;
	u32 pin_val;
	int is_map;

	if ((mux->name == 0)) {
		seq_printf(s, "[not defined] ");
		return;
	}
	val = readl_relaxed(data->base + mux->mux_offset);
	mask = mux->mux_mask;
	pin_val = val & mask;

	is_map = 0;
	func = &mux->functions[0];
	seq_printf(s, "function: ");
	while (func->name) {
		if (func->mux_value == pin_val) {
			is_map = 1;
			seq_printf(s, "[%s] ", func->name);
		} else {
			seq_printf(s, "%s ", func->name);
		}
		func++;
	}
	if (!is_map)
		seq_printf(s, "[not defined] ");
}


static const struct pinctrl_ops rtd_pinctrl_ops = {
	.dt_node_to_map = pinconf_generic_dt_node_to_map_all,
	.dt_free_map = pinctrl_utils_free_map,
	.get_groups_count = rtd_pinctrl_get_groups_count,
	.get_group_name = rtd_pinctrl_get_group_name,
	.get_group_pins = rtd_pinctrl_get_group_pins,
	.pin_dbg_show = rtd_pinctrl_dbg_show,
};

static int rtd_pinctrl_get_functions_count(struct pinctrl_dev *pcdev)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);

	return data->info->num_functions;
}

static const char *rtd_pinctrl_get_function_name(struct pinctrl_dev *pcdev,
		unsigned int selector)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);

	return data->info->functions[selector].name;
}

static int rtd_pinctrl_get_function_groups(struct pinctrl_dev *pcdev,
		unsigned int selector, const char * const **groups,
		unsigned int * const num_groups)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);

	*groups		= data->info->functions[selector].groups;
	*num_groups	= data->info->functions[selector].num_groups;

	return 0;
}

static const struct rtd_pin_desc *rtd_pinctrl_find_mux(struct rtd_pinctrl *data, unsigned int pin)
{

	if (data->info->muxes[pin].name != 0)
		return &data->info->muxes[pin];

	return NULL;
}


static int rtd_pinctrl_set_one_mux(struct pinctrl_dev *pcdev,
	unsigned int pin, const char *func_name)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);
	const struct rtd_pin_desc *mux;
	u32 val;
	int i;

	mux = rtd_pinctrl_find_mux(data, pin);
	if (!mux)
		return 0;

	if (!mux->functions) {
		dev_err(pcdev->dev, "No functions available for pin %s\n", mux->name);
		return -ENOTSUPP;
	}

	for (i = 0; mux->functions[i].name; i++) {
		if (strcmp(mux->functions[i].name, func_name) != 0)
			continue;
		val = readl_relaxed(data->base + mux->mux_offset);
		val &= ~mux->mux_mask;
		val |= mux->functions[i].mux_value & mux->mux_mask;
		writel_relaxed(val, data->base + mux->mux_offset);
		return 0;
	}

	dev_err(pcdev->dev, "No function %s available for pin %s\n", func_name, mux->name);
	return -EINVAL;
}

static int rtd_pinctrl_set_mux(struct pinctrl_dev *pcdev,
		unsigned int function, unsigned int group)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);
	const unsigned int *pins;
	unsigned int num_pins;
	const char *func_name;
	const char *group_name;
	int i, ret;

	func_name = data->info->functions[function].name;
	group_name = data->info->groups[group].name;

	ret = rtd_pinctrl_get_group_pins(pcdev, group, &pins, &num_pins);
	if (ret) {
		dev_err(pcdev->dev, "Getting pins for group %s failed\n", group_name);
		return ret;
	}

	for (i = 0; i < num_pins; i++) {
		ret = rtd_pinctrl_set_one_mux(pcdev, pins[i], func_name);
		if (ret)
			return ret;
	}

	return 0;
}

static int rtd_pinctrl_gpio_request_enable(struct pinctrl_dev *pcdev,
	struct pinctrl_gpio_range *range, unsigned int offset)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);
	struct gpio_chip *gc = range->gc;

	if (strstr(gc->label, "isom") != NULL)
		return rtd_pinctrl_set_one_mux(pcdev, offset, data->info->gpio_func_name);

	return rtd_pinctrl_set_one_mux(pcdev, offset, "gpio");
}

static const struct pinmux_ops rtd_pinmux_ops = {
	.get_functions_count = rtd_pinctrl_get_functions_count,
	.get_function_name = rtd_pinctrl_get_function_name,
	.get_function_groups = rtd_pinctrl_get_function_groups,
	.set_mux = rtd_pinctrl_set_mux,
	.gpio_request_enable = rtd_pinctrl_gpio_request_enable,
};


static const struct pinctrl_pin_desc *rtd_pinctrl_get_pin_by_number(struct rtd_pinctrl *data, int number)
{
	int i;

	for (i = 0; i < data->info->num_pins; i++) {
		if (data->info->pins[i].number == number)
			return &data->info->pins[i];
	}

	return NULL;
}

static const struct rtd_pin_config_desc *rtd_pinctrl_find_config(struct rtd_pinctrl *data, unsigned int pin)
{

	if (data->info->configs[pin].name != 0)
		return &data->info->configs[pin];

	return NULL;
}

static const struct rtd_pin_config_desc *rtd_pinctrl_find_config_by_name(struct rtd_pinctrl *data, char *name)
{
	int i;

	for (i = 0; i < data->info->num_configs; i++) {
		if (data->info->configs[i].name && !strcmp(data->info->configs[i].name, name))
			return &data->info->configs[i];
	}

	return NULL;
}


static const struct rtd_pin_sconfig_desc *rtd_pinctrl_find_sconfig(struct rtd_pinctrl *data, unsigned int pin)
{

	int i;
	const struct pinctrl_pin_desc *pin_desc;
	const char *pin_name;

	pin_desc = rtd_pinctrl_get_pin_by_number(data, pin);
	if (!pin_desc)
		return NULL;

	pin_name = pin_desc->name;

	for (i = 0; i < data->info->num_sconfigs; i++) {
		if (strcmp(data->info->sconfigs[i].name, pin_name) == 0)
			return &data->info->sconfigs[i];
	}

	return NULL;
}


static int rtd_pconf_parse_conf(struct rtd_pinctrl *data,
	unsigned int pinnr, enum pin_config_param param,
	enum pin_config_param arg)
{
	u8 set_val = 0;
	u16 strength;
	u32 val;
	u32 mask;
	u32 pulsel_off, pulen_off, smt_off, curr_off, pow_off, reg_off,
		p_off, n_off, input_volt_off, sr_off, hvil_off;
	const struct rtd_pin_config_desc *config_desc;
	const struct rtd_pin_sconfig_desc *sconfig_desc;


	config_desc = rtd_pinctrl_find_config(data, pinnr);
	if (!config_desc)
		return -ENOTSUPP;

	switch ((u32)param) {
	case PIN_CONFIG_INPUT_SCHMITT:
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		if (config_desc->smt_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support input schmitt\n", config_desc->name);
			return -ENOTSUPP;
		}
		smt_off = config_desc->base_bit + config_desc->smt_offset;
		set_val = arg;
		val = readl(data->base + config_desc->reg_offset);
		if (set_val)
			val |= BIT(smt_off);
		else
			val &= ~BIT(smt_off);
		writel(val, data->base + config_desc->reg_offset);
		break;
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		if (config_desc->pud_en_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support input bias\n", config_desc->name);
			return -ENOTSUPP;
		}
		pulen_off = config_desc->base_bit + config_desc->pud_en_offset;
		set_val = arg;
		val = readl(data->base + config_desc->reg_offset);
		if (set_val)
			val |= BIT(pulen_off);
		else
			val &= ~BIT(pulen_off);
		writel(val, data->base + config_desc->reg_offset);
		break;
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		if (config_desc->pud_en_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support input bias\n", config_desc->name);
			return -ENOTSUPP;
		}
		pulen_off = config_desc->base_bit + config_desc->pud_en_offset;
		val = readl(data->base + config_desc->reg_offset);
		val &= ~BIT(pulen_off);
		writel(val, data->base + config_desc->reg_offset);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		if (config_desc->pud_en_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support input bias\n", config_desc->name);
			return -ENOTSUPP;
		}
		pulen_off = config_desc->base_bit + config_desc->pud_en_offset;
		pulsel_off = config_desc->base_bit + config_desc->pud_sel_offset;
		val = readl(data->base + config_desc->reg_offset);
		val |= BIT(pulen_off) | BIT(pulsel_off);
		writel(val, data->base + config_desc->reg_offset);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		if (config_desc->pud_en_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support input bias\n", config_desc->name);
			return -ENOTSUPP;
		}
		pulen_off = config_desc->base_bit + config_desc->pud_en_offset;
		pulsel_off = config_desc->base_bit + config_desc->pud_sel_offset;
		val = readl(data->base + config_desc->reg_offset);
		val |= BIT(pulen_off);
		val &= ~BIT(pulsel_off);
		writel(val, data->base + config_desc->reg_offset);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		curr_off = config_desc->base_bit + config_desc->curr_offset;
		strength = arg;
		val = readl(data->base + config_desc->reg_offset);
		switch (config_desc->curr_type) {
		case PADDRI_4_8:
			if (strength == 4)
				val &= ~BIT(curr_off);
			else if (strength == 8)
				val |= BIT(curr_off);
			else
				return -EINVAL;
			break;
		case PADDRI_2_4:
			if (strength == 2)
				val &= ~BIT(curr_off);
			else if (strength == 4)
				val |= BIT(curr_off);
			else
				return -EINVAL;
			break;
		case NA:
			pr_err("[rtd_pinctrl][%s] not support drive strength\n", config_desc->name);
			return -ENOTSUPP;
		default:
			return -EINVAL;
		}
		writel(val, data->base + config_desc->reg_offset);
		break;

	case PIN_CONFIG_POWER_SOURCE:
		if (config_desc->power_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support power source\n", config_desc->name);
			return -ENOTSUPP;
		}
		pow_off = config_desc->base_bit + config_desc->power_offset;
		if (pow_off / 32) {
			reg_off = config_desc->reg_offset + 0x4;
			pow_off %= 32;
		} else {
			reg_off = config_desc->reg_offset;
		}
		set_val = arg;
		val = readl(data->base + reg_off);
		if (set_val)
			val |= BIT(pow_off);
		else
			val &= ~BIT(pow_off);
		writel(val, data->base + reg_off);
		break;
	case PIN_CONFIG_SLEW_RATE:
		if (config_desc->slew_rate_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support slew rate\n", config_desc->name);
			return -ENOTSUPP;
		}
		sr_off = config_desc->base_bit + config_desc->slew_rate_offset;
		set_val = arg;
		val = readl(data->base + config_desc->reg_offset);
		val = (val & ~(0x3 << sr_off)) | (set_val << sr_off);
		writel(val, data->base + config_desc->reg_offset);
		break;

		break;
	case RTD_HVIL:
		if (config_desc->hvil_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support hvil\n", config_desc->name);
			return -ENOTSUPP;
		}
		hvil_off = config_desc->base_bit + config_desc->hvil_offset;
		set_val = arg;
		val = readl(data->base + config_desc->reg_offset);
		if (set_val)
			val |= BIT(hvil_off);
		else
			val &= ~BIT(hvil_off);
		writel(val, data->base + config_desc->reg_offset);
		break;

		break;
	case RTD_INPUT_VOLT:
		if (config_desc->input_volt_offset == NA) {
			pr_err("[rtd_pinctrl][%s] not support input voltage level\n", config_desc->name);
			return -ENOTSUPP;
		}
		input_volt_off = config_desc->base_bit + config_desc->input_volt_offset;
		set_val = arg;
		val = readl(data->base + config_desc->reg_offset);
		if (set_val)
			val |= BIT(input_volt_off);
		else
			val &= ~BIT(input_volt_off);
		writel(val, data->base + config_desc->reg_offset);
		break;
	case RTD_P_DRIVE:
		sconfig_desc = rtd_pinctrl_find_sconfig(data, pinnr);
		if (!sconfig_desc)
			return -ENOTSUPP;
		set_val = arg;
		if (sconfig_desc->pdrive_offset / 31) {
			p_off = sconfig_desc->pdrive_offset % 32;
			reg_off = sconfig_desc->reg_offset + 0x4;
		} else {
			p_off = sconfig_desc->pdrive_offset;
			reg_off = sconfig_desc->reg_offset;
		}
		val = readl(data->base + reg_off);
		mask = GENMASK(p_off + sconfig_desc->pdrive_maskbits - 1, p_off);
		val = (val & ~mask) | (set_val << p_off);
		writel(val, data->base + reg_off);
		break;
	case RTD_N_DRIVE:
		sconfig_desc = rtd_pinctrl_find_sconfig(data, pinnr);
		if (!sconfig_desc)
			return -ENOTSUPP;
		set_val = arg;
		if (sconfig_desc->ndrive_offset / 31) {
			n_off = sconfig_desc->ndrive_offset % 32;
			reg_off = sconfig_desc->reg_offset + 0x4;
		} else {
			n_off = sconfig_desc->ndrive_offset;
			reg_off = sconfig_desc->reg_offset;
		}
		val = readl(data->base + reg_off);
		mask = GENMASK(n_off + sconfig_desc->ndrive_maskbits - 1, n_off);
		val = (val & ~mask) | (set_val << n_off);
		writel(val, data->base + reg_off);
		break;
	case RTD_D_CYCLE:
		sconfig_desc = rtd_pinctrl_find_sconfig(data, pinnr);
		if (!sconfig_desc || (sconfig_desc->dcycle_offset == NA)) {
			pr_err("[rtd_pinctrl][%s] not support power source\n", sconfig_desc->name);
			return -ENOTSUPP;
		}
		set_val = arg;
		val = readl(data->base + sconfig_desc->reg_offset);
		mask = GENMASK(sconfig_desc->dcycle_offset +
			sconfig_desc->dcycle_maskbits - 1, sconfig_desc->dcycle_offset);
		val = (val & ~mask) | (set_val << sconfig_desc->dcycle_offset);
		writel(val, data->base + sconfig_desc->reg_offset);
		break;
	default:
		return -ENOTSUPP;
	}

	return 0;
}


static int rtd_pin_config_get(struct pinctrl_dev *pcdev, unsigned int pinnr,
		unsigned long *config)
{
	return -ENOTSUPP;
}

static int rtd_pin_config_set(struct pinctrl_dev *pcdev, unsigned int pinnr,
		unsigned long *configs, unsigned int num_configs)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);
	int i;
	int ret = 0;


	for (i = 0; i < num_configs; i++) {
		ret = rtd_pconf_parse_conf(data, pinnr,
			pinconf_to_config_param(configs[i]),
			pinconf_to_config_argument(configs[i]));
		if (ret < 0)
			return ret;
	}

	return 0;
}


static int rtd_pin_config_group_set(struct pinctrl_dev *pcdev, unsigned int group,
				unsigned long *configs, unsigned int num_configs)
{
	struct rtd_pinctrl *data = pinctrl_dev_get_drvdata(pcdev);
	const unsigned int *pins;
	unsigned int num_pins;
	const char *group_name;
	int i, ret;

	group_name = data->info->groups[group].name;

	ret = rtd_pinctrl_get_group_pins(pcdev, group, &pins, &num_pins);
	if (ret) {
		dev_err(pcdev->dev, "Getting pins for group %s failed\n", group_name);
		return ret;
	}

	for (i = 0; i < num_pins; i++) {
		ret = rtd_pin_config_set(pcdev, pins[i], configs, num_configs);
		if (ret)
			return ret;
	}

	return 0;
}


static const struct pinconf_ops rtd_pinconf_ops = {
	.is_generic = true,
	.pin_config_get = rtd_pin_config_get,
	.pin_config_set = rtd_pin_config_set,
	.pin_config_group_set = rtd_pin_config_group_set,
};


static void detect_vol_config_set(struct rtd_pinctrl *data)
{
	int i, j;
	const struct rtd_pin_vol_detect_desc *vol_pins = data->info->vol_pins;
	unsigned int val;
	unsigned int set_val;
	const struct rtd_pin_config_desc *config_desc;
	u32 pow_off, reg_off;

	for (i = 0; i < data->info->num_vol_pins; i++) {
		val = readl(data->base + vol_pins[i].reg_offset);
		writel(val | BIT(vol_pins[i].en_offset), data->base + vol_pins[i].reg_offset);
		val = readl(data->base + vol_pins[i].reg_offset);
		if (val & BIT(vol_pins[i].status_offset))
			set_val = 1;
		else
			set_val = 0;
		j = 0;
		while (vol_pins[i].pins[j]) {
			config_desc = rtd_pinctrl_find_config_by_name(data, vol_pins[i].pins[j]);
			if (!config_desc) {
				dev_err(data->pcdev->dev, "cannot set config for pins:%s\n", vol_pins[i].pins[j]);
				continue;
			}
			pow_off = config_desc->base_bit + config_desc->power_offset;
			if (pow_off / 32) {
				reg_off = config_desc->reg_offset + 0x4;
				pow_off %= 32;
			} else {
				reg_off = config_desc->reg_offset;
			}
			val = readl(data->base + reg_off);
			if (set_val)
				val |= BIT(pow_off);
			else
				val &= ~BIT(pow_off);
			writel(val, data->base + reg_off);
			j++;
		}
	}
}

static int save_reg_init(struct rtd_pinctrl *data)
{
	const struct rtd_pin_range *pin_range = data->info->pin_range;
	struct device *dev = data->pcdev->dev;
	const struct rtd_reg_range *range;
	size_t num_entries;
	int i;

	data->save_regs = devm_kcalloc(dev, pin_range->num_ranges, sizeof(u32 *), GFP_KERNEL);

	if (!data->save_regs)
		return -ENOMEM;

	for (i = 0; i < pin_range->num_ranges; i++) {
		range = &pin_range->ranges[i];
		num_entries = range->len;

		data->save_regs[i] = devm_kzalloc(dev, num_entries * sizeof(u32), GFP_KERNEL);
		if (!data->save_regs[i])
			return -ENOMEM;
	}

	return 0;
}

#ifdef CONFIG_PINCTRL_RTD_SELFTEST
static void rtd_pinctrl_selftest(struct rtd_pinctrl *data)
{
	int i, j, k, l;
	int f;

	for (i = 0; i < data->info->num_muxes; i++) {
		if (data->info->muxes[i].name == 0)
			continue;
		//Check for pin
		if (strcmp(data->info->pins[i].name, data->info->muxes[i].name) != 0)
			dev_warn(data->pcdev->dev, "Mux %s lacking matching pin\n",
				 data->info->muxes[i].name);

		//Check for group
		if (strcmp(data->info->groups[i].name, data->info->muxes[i].name) != 0)
			dev_warn(data->pcdev->dev, "Mux %s lacking matching group\n",
				 data->info->muxes[i].name);

		for (j = 0; data->info->muxes[i].functions[j].name; j++) {
			//Check for function
			for (k = 0; k < data->info->num_functions; k++) {
				if (strcmp(data->info->functions[k].name,
					data->info->muxes[i].functions[j].name) == 0)
					break;
			}
			if (k == data->info->num_functions) {
				dev_warn(data->pcdev->dev, "Mux %s lacking function %s\n",
					 data->info->muxes[i].name,
					 data->info->muxes[i].functions[j].name);
				continue;
			}
			for (l = 0; l < data->info->functions[k].num_groups; l++) {
				if (strcmp(data->info->muxes[i].name,
					data->info->functions[k].groups[l]) == 0)
					break;
			}
			if (l == data->info->functions[k].num_groups) {
				dev_warn(data->pcdev->dev, "function %s lacking mux %s\n",
				data->info->functions[k].name, data->info->muxes[i].name);
				continue;
			}

			//Check for duplicate mux value - assumption: ascending order
			if (j > 0 && data->info->muxes[i].functions[j].mux_value
					<= data->info->muxes[i].functions[j - 1].mux_value) {
				dev_warn(data->pcdev->dev, "Mux %s function %s has unexpected value\n",
					 data->info->muxes[i].name,
					 data->info->muxes[i].functions[j].name);
				continue;
			}

		}
	}

	for (i = 0; i < data->info->num_functions; i++) {
		for (j = 0; j < data->info->functions[i].num_groups; j++) {
			for (k = 0; k < data->info->num_muxes; k++) {
				if (data->info->muxes[k].name == 0)
					continue;
				if (strcmp(data->info->functions[i].groups[j], data->info->muxes[k].name) == 0)
					break;
			}
			if (k == data->info->num_muxes) {
				dev_warn(data->pcdev->dev, "group mux %s could not find\n",
					 data->info->functions[i].groups[j]);
				break;
			}
			f = 0;
			for (l = 0; data->info->muxes[k].functions[l].name; l++) {
				if (strcmp(data->info->muxes[k].functions[l].name,  data->info->functions[i].name) == 0) {
					f = 1;
					break;
				}
			}
			if (f != 1)
				dev_warn(data->pcdev->dev, "function %s lacking in mux %s\n",
					 data->info->functions[i].name, data->info->muxes[k].name);
		}
	}
}
#endif

int rtd_pinctrl_probe(struct platform_device *pdev, const struct rtd_pinctrl_desc *desc)
{
	struct rtd_pinctrl *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	data->dev = &pdev->dev;
	data->info = desc;
	data->desc.name = dev_name(&pdev->dev);;
	data->desc.pins = data->info->pins;
	data->desc.npins = data->info->num_pins;
	data->desc.pctlops = &rtd_pinctrl_ops;
	data->desc.pmxops = &rtd_pinmux_ops;
	data->desc.confops = &rtd_pinconf_ops;
	data->desc.custom_params = rtd_custom_bindings;
	data->desc.num_custom_params = ARRAY_SIZE(rtd_custom_bindings);
	data->desc.owner = THIS_MODULE;

	data->pcdev = pinctrl_register(&data->desc, &pdev->dev, data);
	if (!data->pcdev)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);

	if (data->info->vol_pins)
		detect_vol_config_set(data);

	if (data->info->need_restore) {
		ret = save_reg_init(data);
		if (ret) {
			dev_err(&pdev->dev, "Failed to allocate backup buffer\n");
			return ret;
		}
	}
#ifdef CONFIG_PINCTRL_RTD_SELFTEST
	rtd_pinctrl_selftest(data);
#endif
	dev_info(&pdev->dev, "probed\n");

	return 0;
}
EXPORT_SYMBOL(rtd_pinctrl_probe);

#ifdef CONFIG_PM
static int realtek_pinctrl_suspend(struct device *dev)
{
	struct rtd_pinctrl *data = dev_get_drvdata(dev);
	const struct rtd_pin_range *pin_range = data->info->pin_range;
	const struct rtd_reg_range *range;
	u32 *save_reg;
	int i;
	int j;

	if (!data->info->need_restore)
		return 0;

	for (i = 0; i < pin_range->num_ranges; i++) {
		range = &pin_range->ranges[i];
		save_reg = data->save_regs[i];

		for (j = 0; j < range->len; j = j + 0x4) {
			save_reg[j] = readl(data->base + range->offset + j);
		}
	}

	return 0;
}

static int realtek_pinctrl_resume(struct device *dev)
{
	struct rtd_pinctrl *data = dev_get_drvdata(dev);
	const struct rtd_pin_range *pin_range = data->info->pin_range;
	const struct rtd_reg_range *range;
	u32 *save_reg;
	int i;
	int j;

	if (!data->info->need_restore)
		return 0;

	for (i = 0; i < pin_range->num_ranges; i++) {
		range = &pin_range->ranges[i];
		save_reg = data->save_regs[i];

		for (j = 0; j < range->len; j = j + 0x4) {
			writel(save_reg[j], data->base + range->offset + j);
		}
	}

	return 0;
}
#endif

DEFINE_NOIRQ_DEV_PM_OPS(realtek_pinctrl_pm_ops, realtek_pinctrl_suspend, realtek_pinctrl_resume);
EXPORT_SYMBOL(realtek_pinctrl_pm_ops);

MODULE_DESCRIPTION("Realtek DHC SoC pinctrl driver");
MODULE_LICENSE("GPL v2");
