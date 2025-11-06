// SPDX-License-Identifier: GPL-2.0+
/*
 * Realtek SDIO host driver
 *
 * Copyright (c) 2017-2020 Realtek Semiconductor Corp.
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/sys_soc.h>
#include <linux/utsname.h>

#include "sdhci-pltfm.h"
#include "sdhci-rtk.h"

#define BOUNDARY_OK(addr, len) \
	((addr | (SZ_4K - 1)) == ((addr + len - 1) | (SZ_4K - 1)))

struct sdhci_rtk_soc_data {
	const struct sdhci_pltfm_data *pdata;
	u32 rtkquirks;
};

struct sdhci_rtk {
	const struct sdhci_rtk_soc_data *soc_data;
	struct gpio_desc *wifi_rst;
	struct regulator *dev_pwr;
	struct gpio_desc *sd_pwr_gpio;
	struct clk *clk_en_sdio;
	struct clk *clk_en_sdio_ip;
	struct reset_control *rstc_sdio;
	void __iomem *wrap_base;
	struct regmap *crt_base;
	struct regmap *iso_base;
	struct regmap *pinctrl_base;
	struct regmap *isosys_base;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_3v3_drv;
	struct pinctrl_state *pins_1v8_drv;
	struct pinctrl_state *pins_vsel_3v3;
	struct pinctrl_state *pins_vsel_1v8;
	u32 preset_pll;
	int location;
};

static const struct soc_device_attribute rtk_soc_thor[] = {
	{ .family = "Realtek Thor", },
	{ /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_hank[] = {
	{ .family = "Realtek Hank", },
	{ .family = "Realtek Hope", },
	{ .family = "Realtek Groot", },
	{ /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_stark[] = {
	{ .family = "Realtek Stark", },
	{ /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_parker[] = {
	{ .family = "Realtek Parker", },
	{ /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_kent[] = {
	{ .family = "Realtek Kent", },
	{ /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_axi_boundary[] = {
	{ .family = "Realtek Stark", },
	{ .family = "Realtek Parker", },
	{ /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_pll_v2[] = {
	{ .family = "Realtek Kent", },
	{ /* sentinel */ }
};

static const struct soc_device_attribute rtk_soc_power_cut[] = {
	{ .family = "Realtek Kent", },
	{ /* sentinel */ }
};

static void rtk_sdhci_sd_pad_power_ctrl(struct sdhci_host *host, int voltage)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct regmap *crt_base = rtk_host->crt_base;
	u32 reg;

	switch (voltage) {
	case MMC_SIGNAL_VOLTAGE_180:
		regmap_read(crt_base, SYS_PLL_SD1, &reg);
		reg &= ~REG_SEL3318_MASK;
		regmap_write(crt_base, SYS_PLL_SD1, reg);

		mdelay(1);

		regmap_read(crt_base, SYS_PLL_SD1, &reg);
		reg |= FIELD_PREP(REG_SEL3318_MASK, REG_SEL3318_0V);
		regmap_write(crt_base, SYS_PLL_SD1, reg);

		mdelay(1);

		regmap_read(crt_base, SYS_PLL_SD1, &reg);
		reg |= FIELD_PREP(REG_SEL3318_MASK, REG_SEL3318_1V8);
		regmap_write(crt_base, SYS_PLL_SD1, reg);

		mdelay(1);

		pinctrl_select_state(rtk_host->pinctrl, rtk_host->pins_vsel_1v8);

		break;
	case MMC_SIGNAL_VOLTAGE_330:
		regmap_read(crt_base, SYS_PLL_SD1, &reg);

		if (FIELD_GET(REG_SEL3318_MASK, reg) == REG_SEL3318_3V3)
			return;

		reg &= ~FIELD_PREP(REG_SEL3318_MASK, REG_SEL3318_3V3);
		regmap_write(crt_base, SYS_PLL_SD1, reg);

		mdelay(1);

		pinctrl_select_state(rtk_host->pinctrl, rtk_host->pins_vsel_3v3);

		mdelay(1);

		regmap_read(crt_base, SYS_PLL_SD1, &reg);
		reg &= ~REG_SEL3318_MASK;
		regmap_write(crt_base, SYS_PLL_SD1, reg);

		mdelay(1);

		regmap_read(crt_base, SYS_PLL_SD1, &reg);
		reg |= FIELD_PREP(REG_SEL3318_MASK, REG_SEL3318_3V3);
		regmap_write(crt_base, SYS_PLL_SD1, reg);

		break;
	}
}

/*
 * The sd_reset pin of Realtek sdio wifi connects to the specific gpio of SoC.
 * Toggling this gpio will reset the SDIO interface of Realtek wifi devices.
 */
static void rtk_sdhci_wifi_device_reset(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct device *dev = mmc_dev(host->mmc);

	if (gpiod_direction_output(rtk_host->wifi_rst, 0))
		dev_err(dev, "fail to set sd reset gpio low\n");

	mdelay(150);

	if (gpiod_direction_input(rtk_host->wifi_rst))
		dev_err(dev, "wifi reset fail\n");
}

static void rtk_sdhci_set_cmd_timeout_irq(struct sdhci_host *host, bool enable)
{
	if (host->mmc->card != NULL)
		return;

	if (enable)
		host->ier |= SDHCI_INT_TIMEOUT;
	else
		host->ier &= ~SDHCI_INT_TIMEOUT;

	sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
	sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
}

static void rtk_sdhci_pad_driving_configure(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct regmap *pinctrl_base = rtk_host->pinctrl_base;
	u32 reg;

	if (soc_device_match(rtk_soc_thor)) {
		regmap_read(pinctrl_base, ISO_PFUNC4, &reg);
		reg = (reg & 0xf) | 0xAF75EEB0;
		regmap_write(pinctrl_base, ISO_PFUNC4, reg);

		regmap_write(pinctrl_base, ISO_PFUNC5, 0x5EEBDD7B);

		regmap_read(pinctrl_base, ISO_PFUNC6, &reg);
		reg = (reg & 0xffffffc0) | 0x37;
		regmap_write(pinctrl_base, ISO_PFUNC6, reg);

		return;
	}

	if (host->timing > MMC_TIMING_SD_HS)
		pinctrl_select_state(rtk_host->pinctrl, rtk_host->pins_1v8_drv);
	else
		pinctrl_select_state(rtk_host->pinctrl, rtk_host->pins_3v3_drv);

}

static void rtk_sdhci_set_ssc_v2(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct regmap *crt_base = rtk_host->crt_base;
	void __iomem *wrap_base = rtk_host->wrap_base;
	u32 reg;

	regmap_read(crt_base, SYS_PLL_SDIO1, &reg);
	reg &= ~PHRT0;
	regmap_write(crt_base, SYS_PLL_SDIO1, reg);

	regmap_read(crt_base, SYS_PLL_SDIO2, &reg);
	reg &= ~(PLL_V2_SSC_OC_EN | PLL_V2_SSC_FLAG_INIT);
	regmap_write(crt_base, SYS_PLL_SDIO2, reg);

	reg = readl(wrap_base + SDHCI_RTK_SSC2);
	reg &= ~GRAN_SET;
	reg |= FIELD_PREP(GRAN_SET, GRAN_SET_208M_1PERCENT);
	writel(reg, wrap_base + SDHCI_RTK_SSC2);

	reg = readl(wrap_base + SDHCI_RTK_SSC1);
	reg &= ~(NCODE_SSC | FCODE_SSC | DOT_GRAN | EN_SSC);
	reg |= FIELD_PREP(NCODE_SSC, NCODE_SSC_INIT_VALUE) |
	       FIELD_PREP(FCODE_SSC, FCODE_SSC_208M_1PERCENT) |
	       FIELD_PREP(DOT_GRAN, DOT_GRAN_INIT_VALUE) | EN_SSC;
	writel(reg, wrap_base + SDHCI_RTK_SSC1);

	regmap_read(crt_base, SYS_PLL_SDIO2, &reg);
	reg |= PLL_V2_SSC_OC_EN | PLL_V2_SSC_FLAG_INIT;
	regmap_write(crt_base, SYS_PLL_SDIO2, reg);

	regmap_read(crt_base, SYS_PLL_SDIO2, &reg);
	reg &= ~PLL_V2_SSC_OC_EN;
	regmap_write(crt_base, SYS_PLL_SDIO2, reg);

	regmap_read(crt_base, SYS_PLL_SDIO1, &reg);
	reg |= PHRT0;
	regmap_write(crt_base, SYS_PLL_SDIO1, reg);
	udelay(100);
}

static void rtk_sdhci_pll_configure(struct sdhci_host *host, u32 pll, int execute_tuning)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct regmap *crt_base = rtk_host->crt_base;
	u32 val, reg;

	regmap_read(crt_base, SYS_PLL_SDIO4, &reg);
	reg &= ~SSC_RSTB;
	reg |= SSC_PLL_RSTB | SSC_PLL_POW;
	regmap_write(crt_base, SYS_PLL_SDIO4, reg);

	val = FIELD_PREP(REG_TUNE11, REG_TUNE11_1V9) |
	      FIELD_PREP(SSCPLL_CS1, SSCPLL_CS1_INIT_VALUE) |
	      FIELD_PREP(SSC_DEPTH, SSC_DEPTH_1_N) |
	      SSC_8X_EN |
	      FIELD_PREP(SSC_DIV_EXT_F, SSC_DIV_EXT_F_200M) |
	      EN_CPNEW;

	if (soc_device_match(rtk_soc_parker) || soc_device_match(rtk_soc_stark)) {
		val |= FIELD_PREP(SSCPLL_ICP, SSCPLL_ICP_10U) |
		       FIELD_PREP(SSCPLL_RS, SSCPLL_RS_13K);
	} else {
		val |= FIELD_PREP(SSCPLL_ICP, SSCPLL_ICP_20U) |
		       FIELD_PREP(SSCPLL_RS, SSCPLL_RS_10K);
	}

	/* The SSC shouldn't enable when execute tuning */
	if (!execute_tuning)
		regmap_write(crt_base, SYS_PLL_SDIO2, val);

	regmap_write(crt_base, SYS_PLL_SDIO3, pll);

	mdelay(2);

	regmap_update_bits(crt_base, SYS_PLL_SDIO4, SSC_RSTB, SSC_RSTB);

	udelay(200);
}

static void rtk_sdhci_pll_configure_v2(struct sdhci_host *host, u32 pll, int execute_tuning)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_rtk_soc_data *soc_data = rtk_host->soc_data;
	struct regmap *crt_base = rtk_host->crt_base;
	u32 rtkquirks = soc_data->rtkquirks;
	u32 reg, sscpll;

	regmap_read(crt_base, SYS_PLL_SDIO1, &reg);
	reg &= ~PHRT0;
	regmap_write(crt_base, SYS_PLL_SDIO1, reg);

	regmap_write(crt_base, SYS_PLL_SDIO3, pll);

	regmap_read(crt_base, SYS_PLL_SDIO2, &sscpll);
	sscpll &= ~(PLL_V2_PI_IBSELH | PLL_V2_SSC_PLL_ICP |
		    PLL_V2_SSC_PLL_RS | PLL_V2_SSC_DIV_EXT_F);

	switch (FIELD_GET(PLL_V2_SSC_DIV_N, pll)) {
	case PLL_V2_SSC_DIV_N_208M:
		sscpll |= FIELD_PREP(PLL_V2_PI_IBSELH, PLL_V2_PI_IBSELH_150_255M) |
			  FIELD_PREP(PLL_V2_SSC_PLL_ICP, PLL_V2_SSC_PLL_ICP_INIT_VALUE) |
			  FIELD_PREP(PLL_V2_SSC_PLL_RS, PLL_V2_SSC_PLL_RS_8K) |
			  FIELD_PREP(PLL_V2_SSC_DIV_EXT_F, PLL_V2_SSC_DIV_EXT_F_208M);
		break;
	case PLL_V2_SSC_DIV_N_200M:
		sscpll |= FIELD_PREP(PLL_V2_PI_IBSELH, PLL_V2_PI_IBSELH_150_255M) |
			  FIELD_PREP(PLL_V2_SSC_PLL_ICP, PLL_V2_SSC_PLL_ICP_INIT_VALUE) |
			  FIELD_PREP(PLL_V2_SSC_PLL_RS, PLL_V2_SSC_PLL_RS_8K) |
			  FIELD_PREP(PLL_V2_SSC_DIV_EXT_F, PLL_V2_SSC_DIV_EXT_F_200M);
		break;
	case PLL_V2_SSC_DIV_N_100M:
		sscpll |= FIELD_PREP(PLL_V2_PI_IBSELH, PLL_V2_PI_IBSELH_80_150M) |
			  FIELD_PREP(PLL_V2_SSC_PLL_ICP, PLL_V2_SSC_PLL_ICP_INIT_VALUE) |
			  FIELD_PREP(PLL_V2_SSC_PLL_RS, PLL_V2_SSC_PLL_RS_6K) |
			  FIELD_PREP(PLL_V2_SSC_DIV_EXT_F, PLL_V2_SSC_DIV_EXT_F_100M);
		break;
	}

	regmap_write(crt_base, SYS_PLL_SDIO2, sscpll);
	mdelay(2);

	regmap_read(crt_base, SYS_PLL_SDIO4, &reg);
	reg &= ~SSC_PLL_POW;
	reg |= SSC_RSTB | SSC_PLL_RSTB;
	regmap_write(crt_base, SYS_PLL_SDIO4, reg);

	regmap_read(crt_base, SYS_PLL_SDIO2, &reg);
	reg |= PLL_V2_SSC_OC_EN;
	regmap_write(crt_base, SYS_PLL_SDIO2, reg);

	regmap_read(crt_base, SYS_PLL_SDIO4, &reg);
	reg |= SSC_PLL_POW;
	regmap_write(crt_base, SYS_PLL_SDIO4, reg);
	udelay(200);

	regmap_read(crt_base, SYS_PLL_SDIO2, &reg);
	reg &= ~PLL_V2_SSC_OC_EN;
	regmap_write(crt_base, SYS_PLL_SDIO2, reg);

	regmap_read(crt_base, SYS_PLL_SDIO1, &reg);
	reg |= PHRT0;
	regmap_write(crt_base, SYS_PLL_SDIO1, reg);
	udelay(100);

	if (!execute_tuning && !(rtkquirks & RTKQUIRK_SSC_CLK_JITTER)) {
		if (FIELD_GET(PLL_V2_SSC_DIV_N, pll) == PLL_V2_SSC_DIV_N_208M) {
			dev_err(mmc_dev(host->mmc), "%s enable ssc function\n", __func__);
			rtk_sdhci_set_ssc_v2(host);
		}
	}

}

static void rtk_sdhci_sd_slot_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct device *dev = mmc_dev(host->mmc);

	if (soc_device_match(rtk_soc_stark)) {

		if (gpiod_direction_output(rtk_host->sd_pwr_gpio, 0))
			dev_err(dev, "fail to enable sd power");

		if (host->quirks2 & SDHCI_QUIRK2_NO_1_8_V)
			rtk_sdhci_sd_pad_power_ctrl(host, MMC_SIGNAL_VOLTAGE_330);
		else
			rtk_sdhci_sd_pad_power_ctrl(host, MMC_SIGNAL_VOLTAGE_180);
	}
}

static void sdhci_rtk_hw_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	void __iomem *wrap_base = rtk_host->wrap_base;
	struct regmap *crt_base = rtk_host->crt_base;
	struct regmap *iso_base = rtk_host->iso_base;
	struct regmap *isosys_base = rtk_host->isosys_base;

	u32 reg, val, pwr_cut;

	if (soc_device_match(rtk_soc_thor)) {
		regmap_update_bits(crt_base, SYS_PLL_SDIO1, BIAS_EN, BIAS_EN);
		regmap_update_bits(crt_base, SYS_PLL_SDIO2, SSCLDO_EN, SSCLDO_EN);
	} else if (soc_device_match(rtk_soc_hank)) {
		regmap_update_bits(iso_base, ISO_RESERVED, SDIO_SSCLDO_EN |
				   SDIO_BIAS_EN, SDIO_SSCLDO_EN | SDIO_BIAS_EN);
	} else {
		regmap_update_bits(iso_base, ISO_CTRL_1, SDIO_SSCLDO_EN |
				   SDIO_BIAS_EN, SDIO_SSCLDO_EN | SDIO_BIAS_EN);
	}
	udelay(200);

	if (soc_device_match(rtk_soc_power_cut)) {
		regmap_read(isosys_base, ISO_SYS_POWER_CTRL, &pwr_cut);
		if (pwr_cut & ISO_PHY)
			regmap_update_bits(isosys_base, ISO_SYS_POWER_CTRL, ISO_PHY, 0);
	}

	if (rtk_host->location == SDIO_LOC_0) {
		rtk_sdhci_sd_slot_init(host);
	} else if (soc_device_match(rtk_soc_stark)) {
		if (host->quirks2 & SDHCI_QUIRK2_NO_1_8_V)
			pinctrl_select_state(rtk_host->pinctrl, rtk_host->pins_vsel_3v3);
		else
			pinctrl_select_state(rtk_host->pinctrl, rtk_host->pins_vsel_1v8);
	}

	regmap_read(crt_base, SYS_PLL_SDIO3, &val);

	if (soc_device_match(rtk_soc_pll_v2)) {
		val &= ~PLL_V2_SSC_DIV_N;
		val |= FIELD_PREP(PLL_V2_SSC_DIV_N, PLL_V2_SSC_DIV_N_200M);
		rtk_sdhci_pll_configure_v2(host, val, 0);
	} else {
		val = FIELD_PREP(SSC_TBASE, SSC_TBASE_INIT_VALUE) |
		      FIELD_PREP(SSC_STEP_IN, SSC_STEP_IN_INIT_VALUE) |
		      FIELD_PREP(SSC_DIV_N, SSC_DIV_N_200M);
		rtk_sdhci_pll_configure(host, val, 0);
	}

	if (soc_device_match(rtk_soc_hank)) {
		reg = readl(wrap_base + SDIO_RTK_DUMMY_SYS1);
		reg |= BIT(31);
		writel(reg, wrap_base + SDIO_RTK_DUMMY_SYS1);
	}

	reg = readl(wrap_base + SDIO_RTK_ISREN);
	reg |= WRITE_DATA | INT4EN;
	writel(reg, wrap_base + SDIO_RTK_ISREN);

	reg = readl(wrap_base + SDIO_RTK_CTL);
	reg |= SUSPEND_N;
	writel(reg, wrap_base + SDIO_RTK_CTL);
}

static void rtk_sdhci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);

	rtk_sdhci_set_cmd_timeout_irq(host, true);

	sdhci_request(mmc, mrq);
}

static void rtk_sdhci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	void __iomem *wrap_base = rtk_host->wrap_base;

	sdhci_set_ios(mmc, ios);

	rtk_sdhci_pad_driving_configure(host);

	/* enlarge wait time for data */
	if ((host->timing == MMC_TIMING_UHS_SDR104) &&
	    soc_device_match(rtk_soc_thor))
		writel(0x1d, wrap_base + SDIO_RTK_DUMMY_SYS1);
}

static void rtk_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	void __iomem *wrap_base = rtk_host->wrap_base;
	u32 reg;

	if (soc_device_match(rtk_soc_kent)) {
		sdhci_reset(host, mask);
		return;
	}

	reg = readl(wrap_base + SDIO_RTK_CTL);
	reg |= L4_GATED_DISABLE;
	writel(reg, wrap_base + SDIO_RTK_CTL);

	if (mask & SDHCI_RESET_DATA)
		sdhci_writel(host, SDHCI_INT_DATA_END, SDHCI_INT_STATUS);

	sdhci_reset(host, mask);

	reg = readl(wrap_base + SDIO_RTK_CTL);
	reg &= ~L4_GATED_DISABLE;
	writel(reg, wrap_base + SDIO_RTK_CTL);
}

static u32 rtk_sdhci_irq(struct sdhci_host *host, u32 intmask)
{
	u32 command;
	u16 clk;

	if (host->mmc->card != NULL)
		return intmask;

	if (host->cmd != NULL) {
		rtk_sdhci_set_cmd_timeout_irq(host, true);
		return intmask;
	}

	if (intmask & SDHCI_INT_TIMEOUT) {
		command = SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND));
		clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	} else {
		return intmask;
	}

	if (!clk && (command == MMC_GO_IDLE_STATE))
		rtk_sdhci_set_cmd_timeout_irq(host, false);

	if (command == MMC_GO_IDLE_STATE || command == SD_SEND_IF_COND ||
	    command == SD_IO_SEND_OP_COND || command == SD_IO_RW_DIRECT ||
	    command == MMC_APP_CMD || command == MMC_SEND_OP_COND) {
		intmask &= ~SDHCI_INT_TIMEOUT;
		sdhci_writel(host, SDHCI_INT_TIMEOUT, SDHCI_INT_STATUS);
		del_timer(&host->timer);
	}

	return intmask;
}

static int rtk_mmc_send_tuning(struct mmc_host *mmc, u32 opcode, int tx)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct mmc_command cmd = {0};
	u32 reg, mask;
	int err;

	if (tx) {
		mask = ~(SDHCI_INT_CRC | SDHCI_INT_END_BIT | SDHCI_INT_INDEX |
			 SDHCI_INT_DATA_TIMEOUT | SDHCI_INT_DATA_CRC |
			 SDHCI_INT_DATA_END_BIT | SDHCI_INT_BUS_POWER |
			 SDHCI_INT_AUTO_CMD_ERR | SDHCI_INT_ADMA_ERROR);

		reg = sdhci_readl(host, SDHCI_INT_ENABLE);
		reg &= mask;
		sdhci_writel(host, reg, SDHCI_INT_ENABLE);

		reg = sdhci_readl(host, SDHCI_SIGNAL_ENABLE);
		reg &= mask;
		sdhci_writel(host, reg, SDHCI_SIGNAL_ENABLE);

		cmd.opcode = opcode;
		if (mmc_card_sdio(mmc->card)) {
			cmd.arg = 0x2000;
			cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC;
		} else {
			cmd.arg = mmc->card->rca << 16;
			cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;
		}

		err = mmc_wait_for_cmd(mmc, &cmd, 0);
		if (err)
			return err;
	} else {
		return mmc_send_tuning(mmc, opcode, NULL);
	}

	return 0;
}

static int rtk_sdhci_change_phase(struct sdhci_host *host, u8 sample_point, int tx)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct regmap *crt_base = rtk_host->crt_base;
	u32 reg;
	u16 clk = 0;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	/* To disable reset signal */
	regmap_read(crt_base, SYS_PLL_SDIO1, &reg);
	reg &= ~PHRT0;
	regmap_write(crt_base, SYS_PLL_SDIO1, reg);

	regmap_read(crt_base, SYS_PLL_SDIO1, &reg);

	if (tx) {
		reg &= ~PHSEL0_MASK;
		reg |= (sample_point << PHSEL0_SHIFT);
	} else {
		reg &= ~PHSEL1_MASK;
		reg |= (sample_point << PHSEL1_SHIFT);
	}

	regmap_write(crt_base, SYS_PLL_SDIO1, reg);
	/* re-enable reset signal */
	regmap_read(crt_base, SYS_PLL_SDIO1, &reg);
	reg |= PHRT0;
	regmap_write(crt_base, SYS_PLL_SDIO1, reg);
	udelay(100);

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	return 0;
}

static inline u32 test_phase_bit(u32 phase_map, unsigned int bit)
{
	bit %= MAX_PHASE;
	return phase_map & (1 << bit);
}

static int sd_get_phase_len(u32 phase_map, unsigned int start_bit)
{
	int i;

	for (i = 0; i < MAX_PHASE; i++) {
		if (test_phase_bit(phase_map, start_bit + i) == 0)
			return i;
	}
	return MAX_PHASE;
}

static u8 rtk_sdhci_search_final_phase(struct sdhci_host *host, u32 phase_map, int tx)
{
	int start = 0, len = 0;
	int start_final = 0, len_final = 0;
	u8 final_phase = 0xFF;

	if (phase_map == 0) {
		pr_err("phase error: [map:%08x]\n", phase_map);
		return final_phase;
	}

	while (start < MAX_PHASE) {
		len = sd_get_phase_len(phase_map, start);
		if (len_final < len) {
			start_final = start;
			len_final = len;
		}
		start += len ? len : 1;
	}

	if (len_final > MINIMUM_CONTINUE_LENGTH)
		final_phase = (start_final + len_final / 2) % MAX_PHASE;
	else
		final_phase = 0xFF;

	dev_err(mmc_dev(host->mmc), "%s phase: [map:%08x] [maxlen:%d] [final:%d]\n",
		tx ? "tx" : "rx", phase_map, len_final, final_phase);

	return final_phase;
}

static int rtk_sdhci_tuning(struct sdhci_host *host, u32 opcode, int tx)
{
	struct mmc_host *mmc = host->mmc;
	int err, i, sample_point;
	u32 raw_phase_map[TUNING_CNT] = {0}, phase_map;
	u8 final_phase = 0;

	for (sample_point = 0; sample_point < MAX_PHASE; sample_point++) {
		for (i = 0; i < TUNING_CNT; i++) {
			rtk_sdhci_change_phase(host, (u8) sample_point, tx);
			err = rtk_mmc_send_tuning(mmc, opcode, tx);
			if (err == 0)
				raw_phase_map[i] |= (1 << sample_point);
		}
	}

	phase_map = 0xFFFFFFFF;
	for (i = 0; i < TUNING_CNT; i++)
		phase_map &= raw_phase_map[i];

	if (phase_map) {
		final_phase = rtk_sdhci_search_final_phase(host, phase_map, tx);
		if (final_phase == 0xFF) {
			pr_err("%s final phase = 0x%08x invalid\n", __func__, final_phase);
			return -EINVAL;
		}
		err = rtk_sdhci_change_phase(host, final_phase, tx);
		if (err < 0)
			return err;
	} else {
		pr_err("%s  fail !phase_map\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void rtk_sdhci_down_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct regmap *crt_base = rtk_host->crt_base;
	u32 reg, ssc_div_n;

	regmap_read(crt_base, SYS_PLL_SDIO3, &reg);

	if (soc_device_match(rtk_soc_pll_v2)) {
		switch (FIELD_GET(PLL_V2_SSC_DIV_N, reg)) {
		case PLL_V2_SSC_DIV_N_208M:
			reg &= ~PLL_V2_SSC_DIV_N;
			reg |= FIELD_PREP(PLL_V2_SSC_DIV_N, PLL_V2_SSC_DIV_N_200M);
			rtk_sdhci_pll_configure_v2(host, reg, 1);
			dev_err(mmc_dev(host->mmc), "%s down clock to 200M\n", __func__);
			break;
		case PLL_V2_SSC_DIV_N_200M:
			reg &= ~PLL_V2_SSC_DIV_N;
			reg |= FIELD_PREP(PLL_V2_SSC_DIV_N, PLL_V2_SSC_DIV_N_100M);
			rtk_sdhci_pll_configure_v2(host, reg, 1);
			dev_err(mmc_dev(host->mmc), "%s down clock to 100M\n", __func__);
			break;
		case PLL_V2_SSC_DIV_N_100M:
			reg &= ~PLL_V2_SSC_DIV_N;
			reg |= FIELD_PREP(PLL_V2_SSC_DIV_N, PLL_V2_SSC_DIV_N_50M);
			rtk_sdhci_pll_configure_v2(host, reg, 1);
			break;
		}
	} else {
		ssc_div_n = (reg & 0x03FF0000) >> 16;
		/* When PLL set to 96, may interference wifi 2.4Ghz */
		if (ssc_div_n == 158)
			ssc_div_n = ssc_div_n - 7;

		reg = ((reg & (~0x3FF0000)) | ((ssc_div_n - 7) << 16));
		rtk_sdhci_pll_configure(host, reg, 1);
	}
}

static int rtk_sdhci_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_rtk_soc_data *soc_data = rtk_host->soc_data;
	struct regmap *crt_base = rtk_host->crt_base;
	void __iomem *wrap_base = rtk_host->wrap_base;
	u32 rtkquirks = soc_data->rtkquirks;
	u32 reg, tx_opcode;
	int ret = 0;

	pr_err("%s : Execute Clock Phase Tuning\n", __func__);

	/* To disable the SSC during the phase tuning process. */
	if (soc_device_match(rtk_soc_pll_v2)) {
		reg = readl(wrap_base + SDHCI_RTK_SSC1);
		reg &= ~EN_SSC;
		writel(reg, wrap_base + SDHCI_RTK_SSC1);
	} else {
		regmap_read(crt_base, SYS_PLL_SDIO2, &reg);
		reg &= ~SSC_DEPTH;
		regmap_write(crt_base, SYS_PLL_SDIO2, reg);
	}

	if (mmc_card_sdio(host->mmc->card))
		tx_opcode = SD_IO_RW_DIRECT;
	else
		tx_opcode = MMC_SEND_STATUS;

	ret = rtk_sdhci_tuning(host, tx_opcode, 1);
	if (ret)
		pr_err("tx tuning fail\n");

	do {
		ret = rtk_sdhci_tuning(host, MMC_SEND_TUNING_BLOCK, 0);

		if (ret) {
			regmap_read(crt_base, SYS_PLL_SDIO3, &reg);
			if ((FIELD_GET(SSC_DIV_N, reg) < SSC_DIV_N_50M) ||
			    (FIELD_GET(PLL_V2_SSC_DIV_N, reg) == PLL_V2_SSC_DIV_N_50M)) {
				pr_err("%s: Tuning RX fail\n", __func__);
				return ret;
			}
			rtk_sdhci_down_clock(host);
		}
	} while (ret);

	if (!(rtkquirks & RTKQUIRK_SSC_CLK_JITTER)) {
		if (soc_device_match(rtk_soc_pll_v2)) {
			regmap_read(crt_base, SYS_PLL_SDIO3, &reg);
			if (FIELD_GET(PLL_V2_SSC_DIV_N, reg) == PLL_V2_SSC_DIV_N_208M) {
				reg = readl(wrap_base + SDHCI_RTK_SSC1);
				reg |= EN_SSC;
				writel(reg, wrap_base + SDHCI_RTK_SSC1);
			}
		} else {
			regmap_read(crt_base, SYS_PLL_SDIO2, &reg);
			reg |= FIELD_PREP(SSC_DEPTH, SSC_DEPTH_1_N);
			regmap_write(crt_base, SYS_PLL_SDIO2, reg);
		}
	}

	regmap_read(crt_base, SYS_PLL_SDIO3, &reg);

	if (soc_device_match(rtk_soc_pll_v2))
		dev_info(mmc_dev(host->mmc), "after tuning, current pll = %lx\n",
			 FIELD_GET(PLL_V2_SSC_DIV_N, reg));
	else
		dev_info(mmc_dev(host->mmc), "after tuning, current pll = %lx\n",
			 FIELD_GET(SSC_DIV_N, reg));

	return 0;
}

static void rtk_sdhci_set_uhs_signaling(struct sdhci_host *host, unsigned timing)
{
	u16 ctrl_2 = 0;

	sdhci_set_uhs_signaling(host, timing);

	if (timing > MMC_TIMING_SD_HS) {
		ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		sdhci_writew(host, ctrl_2 | SDHCI_CTRL_VDD_180, SDHCI_HOST_CONTROL2);
	}
}

static void rtk_sdhci_adma_write_desc(struct sdhci_host *host, void **desc, dma_addr_t addr,
				      int len, unsigned int cmd)
{
	int tmplen, offset;

	if (likely(!soc_device_match(rtk_soc_axi_boundary) || !len || BOUNDARY_OK(addr, len))) {
		sdhci_adma_write_desc(host, desc, addr, len, cmd);
		return;
	}

	pr_debug("%s: descriptor splitting, addr %pad, len %d\n", mmc_hostname(host->mmc), &addr, len);

	offset = addr & (SZ_4K - 1);
	tmplen = SZ_4K - offset;
	sdhci_adma_write_desc(host, desc, addr, tmplen, cmd);

	addr += tmplen;
	len -= tmplen;
	sdhci_adma_write_desc(host, desc, addr, len, cmd);
}

static void rtk_sdhci_request_done(struct sdhci_host *host, struct mmc_request *mrq)
{
	rtk_sdhci_set_cmd_timeout_irq(host, true);

	mmc_request_done(host->mmc, mrq);
}

/* Update card information to determine SD/SDIO tx tuning function */
static void rtk_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
	mmc->card = card;
}

static void rtk_replace_mmc_host_ops(struct sdhci_host *host)
{
	host->mmc_host_ops.request	= rtk_sdhci_request;
	host->mmc_host_ops.set_ios	= rtk_sdhci_set_ios;
	host->mmc_host_ops.init_card	= rtk_init_card;
}

static const struct sdhci_ops rtk_sdhci_ops = {
	.set_clock = sdhci_set_clock,
	.irq = rtk_sdhci_irq,
	.set_bus_width = sdhci_set_bus_width,
	.reset = rtk_sdhci_reset,
	.platform_execute_tuning = rtk_sdhci_execute_tuning,
	.set_uhs_signaling = rtk_sdhci_set_uhs_signaling,
	.adma_write_desc = rtk_sdhci_adma_write_desc,
	.request_done = rtk_sdhci_request_done,
};

static const struct sdhci_pltfm_data sdhci_rtk_sdio_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_CARD_DETECTION |
		  SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC,
	.quirks2 = SDHCI_QUIRK2_BROKEN_DDR50 |
		   SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.ops = &rtk_sdhci_ops,
};

static struct sdhci_rtk_soc_data rtd1319_soc_data = {
	.pdata = &sdhci_rtk_sdio_pdata,
};

static struct sdhci_rtk_soc_data rtd1619b_soc_data = {
	.pdata = &sdhci_rtk_sdio_pdata,
};

static struct sdhci_rtk_soc_data rtd1319d_soc_data = {
	.pdata = &sdhci_rtk_sdio_pdata,
};

static struct sdhci_rtk_soc_data rtk_soc_data = {
	.pdata = &sdhci_rtk_sdio_pdata,
};

static const struct of_device_id sdhci_rtk_dt_match[] = {
	{.compatible = "realtek,rtd1319-sdio", .data = &rtd1319_soc_data},
	{.compatible = "realtek,rtd1619b-sdio", .data = &rtd1619b_soc_data},
	{.compatible = "realtek,rtd1319d-sdio", .data = &rtd1319d_soc_data},
	{.compatible = "realtek,rtd1625-sdio", .data = &rtk_soc_data},
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_rtk_dt_match);

static int sdhci_rtk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sdhci_rtk_soc_data *soc_data;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *sd_node;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_rtk *rtk_host;
	int ret = 0;

	pr_info("%s: build at : %s\n", __func__, utsname()->version);

	match = of_match_device(sdhci_rtk_dt_match, &pdev->dev);
	if (!match)
		return -EINVAL;
	soc_data = match->data;

	host = sdhci_pltfm_init(pdev, soc_data->pdata, sizeof(*rtk_host));
	if (IS_ERR(host))
		return PTR_ERR(host);
	pltfm_host = sdhci_priv(host);

	rtk_host = sdhci_pltfm_priv(pltfm_host);
	rtk_host->soc_data = soc_data;
	rtk_host->preset_pll = 0;
	rtk_host->location = SDIO_LOC_1;

	rtk_host->wrap_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(rtk_host->wrap_base)) {
		ret = PTR_ERR(rtk_host->wrap_base);
		dev_err(&pdev->dev, "couldn't get wrapper control register base address\n");
		goto err;
	}

	rtk_host->crt_base = syscon_regmap_lookup_by_phandle(node, "realtek,crt");
	if (IS_ERR(rtk_host->crt_base)) {
		ret = PTR_ERR(rtk_host->crt_base);
		dev_err(&pdev->dev, "couldn't get crt register base address\n");
		goto err;
	}

	rtk_host->iso_base = syscon_regmap_lookup_by_phandle(node, "realtek,iso");
	if (IS_ERR(rtk_host->iso_base)) {
		ret = PTR_ERR(rtk_host->iso_base);
		dev_err(&pdev->dev, "couldn't get iso register base address\n");
		goto err;
	}

	rtk_host->pinctrl_base = syscon_regmap_lookup_by_phandle(node, "realtek,pinctrl");
	if (IS_ERR(rtk_host->pinctrl_base)) {
		ret = PTR_ERR(rtk_host->pinctrl_base);
		dev_err(&pdev->dev, "couldn't get pin control register base address\n");
		goto err;
	}

	if (of_device_is_compatible(node, "realtek,rtd1625-sdio")) {
		rtk_host->isosys_base = syscon_regmap_lookup_by_phandle(node, "realtek,isosys");
		if (IS_ERR(rtk_host->isosys_base)) {
			ret = PTR_ERR(rtk_host->isosys_base);
			dev_info(&pdev->dev, "couldn't get iso sys register base address\n");
			goto err;
		}
	}

	sd_node = of_get_compatible_child(node, "sd-slot");
	if (sd_node) {
		rtk_host->location = SDIO_LOC_0;

		rtk_host->sd_pwr_gpio = devm_gpiod_get(&pdev->dev, "sd-power", GPIOD_OUT_LOW);
		if (IS_ERR(rtk_host->sd_pwr_gpio))
			dev_err(&pdev->dev, "%s can't request power gpio\n", __func__);

		of_node_put(sd_node);
	}

	rtk_host->dev_pwr = devm_regulator_get_optional(&pdev->dev, "device-power");
	if (IS_ERR(rtk_host->dev_pwr)) {
		if (PTR_ERR(rtk_host->dev_pwr) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
	} else {
		ret = regulator_enable(rtk_host->dev_pwr);
		if (ret)
			dev_err(&pdev->dev, "failed to enable SDIO device power\n");
	}

	if (device_property_read_bool(&pdev->dev, "wifi-rst-shared"))
		regmap_update_bits(rtk_host->iso_base, ISO_USB_TYPEC_CTRL_CC1, PLR_EN, 0);

	rtk_host->wifi_rst = devm_gpiod_get(&pdev->dev, "wifi-rst", GPIOD_OUT_LOW);
	if (IS_ERR(rtk_host->wifi_rst))
		dev_err(&pdev->dev, "%s rtk wifi reset gpio invalid\n", __func__);
	else
		rtk_sdhci_wifi_device_reset(host);

	rtk_host->rstc_sdio = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(rtk_host->rstc_sdio)) {
		pr_warn("Failed to get sdio reset control(%ld)\n", PTR_ERR(rtk_host->rstc_sdio));
		rtk_host->rstc_sdio = NULL;
	}

	rtk_host->clk_en_sdio = devm_clk_get(&pdev->dev, "sdio");
	if (IS_ERR(rtk_host->clk_en_sdio)) {
		pr_warn("Failed to get sdio clk(%ld)\n", PTR_ERR(rtk_host->clk_en_sdio));
		rtk_host->clk_en_sdio = NULL;
	}

	rtk_host->clk_en_sdio_ip = devm_clk_get(&pdev->dev, "sdio_ip");
	if (IS_ERR(rtk_host->clk_en_sdio_ip)) {
		pr_warn("Failed to get sdio ip clk(%ld)\n", PTR_ERR(rtk_host->clk_en_sdio_ip));
		rtk_host->clk_en_sdio_ip = NULL;
	}

	rtk_host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(rtk_host->pinctrl)) {
		ret = PTR_ERR(rtk_host->pinctrl);
		pr_err("fail to get pinctrl\n");
		goto err_sdio_clk;
	}

	rtk_host->pins_default = pinctrl_lookup_state(rtk_host->pinctrl, PINCTRL_STATE_DEFAULT);
	if (IS_ERR(rtk_host->pins_default)) {
		ret = PTR_ERR(rtk_host->pins_default);
		pr_warn("fail to get default state\n");
		goto err_sdio_clk;
	}

	rtk_host->pins_3v3_drv = pinctrl_lookup_state(rtk_host->pinctrl, "sdio-3v3-drv");
	if (IS_ERR(rtk_host->pins_3v3_drv)) {
		ret = PTR_ERR(rtk_host->pins_3v3_drv);
		pr_warn("fail to get pad driving for 3.3V state\n");
		goto err_sdio_clk;
	}

	rtk_host->pins_1v8_drv = pinctrl_lookup_state(rtk_host->pinctrl, "sdio-1v8-drv");
	if (IS_ERR(rtk_host->pins_1v8_drv)) {
		ret = PTR_ERR(rtk_host->pins_1v8_drv);
		pr_warn("fail to get pad driving for 1.8V state\n");
		goto err_sdio_clk;
	}

	if (soc_device_match(rtk_soc_stark)) {
		rtk_host->pins_vsel_3v3 = pinctrl_lookup_state(rtk_host->pinctrl, "sdio-vsel-3v3");
		if (IS_ERR(rtk_host->pins_vsel_3v3)) {
			ret = PTR_ERR(rtk_host->pins_vsel_3v3);
			pr_warn("fail to get pad power state for 3.3V\n");
			goto err_sdio_clk;
		}

		rtk_host->pins_vsel_1v8 = pinctrl_lookup_state(rtk_host->pinctrl, "sdio-vsel-1v8");
		if (IS_ERR(rtk_host->pins_vsel_1v8)) {
			ret = PTR_ERR(rtk_host->pins_vsel_1v8);
			pr_warn("fail to get pad power state for 1.8V\n");
			goto err_sdio_clk;
		}

		if (device_property_read_bool(&pdev->dev, "no-1-8-v"))
			host->quirks2 |= SDHCI_QUIRK2_NO_1_8_V;
	}

	reset_control_deassert(rtk_host->rstc_sdio);
	clk_prepare_enable(rtk_host->clk_en_sdio);
	clk_prepare_enable(rtk_host->clk_en_sdio_ip);

	rtk_replace_mmc_host_ops(host);

	sdhci_rtk_hw_init(host);

	ret = mmc_of_parse(host->mmc);
	if (ret)
		goto err_sdio_clk;

	ret = sdhci_add_host(host);
	if (ret)
		goto err_sdio_clk;

	return 0;

err_sdio_clk:
	clk_disable_unprepare(rtk_host->clk_en_sdio_ip);
	clk_disable_unprepare(rtk_host->clk_en_sdio);
	sdhci_pltfm_free(pdev);
err:
	dev_err(&pdev->dev, "%s failed %d\n", __func__, ret);
	return ret;
}

static int sdhci_rtk_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	int dead = (readl(host->ioaddr + SDHCI_INT_STATUS) == 0xFFFFFFFF);

	sdhci_remove_host(host, dead);
	sdhci_pltfm_free(pdev);

	return 0;
}

static void sdhci_rtk_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdhci_host *host = dev_get_drvdata(dev);

	pr_err("[SDIO] %s\n", __func__);

	sdhci_suspend_host(host);
}

static int __maybe_unused sdhci_rtk_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct regmap *crt_base = rtk_host->crt_base;

	pr_err("[SDIO] %s start\n", __func__);

	regmap_read(crt_base, SYS_PLL_SDIO1, &rtk_host->preset_pll);

	sdhci_suspend_host(host);
	reset_control_assert(rtk_host->rstc_sdio);
	clk_disable_unprepare(rtk_host->clk_en_sdio);
	clk_disable_unprepare(rtk_host->clk_en_sdio_ip);

	pr_err("[SDIO] %s OK\n", __func__);
	return 0;
}

static int __maybe_unused sdhci_rtk_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_rtk *rtk_host = sdhci_pltfm_priv(pltfm_host);
	struct regmap *crt_base = rtk_host->crt_base;
	u8 reg;

	host->clock = 0;

	pr_err("[SDIO] %s start\n", __func__);

	reset_control_deassert(rtk_host->rstc_sdio);
	clk_prepare_enable(rtk_host->clk_en_sdio);
	clk_prepare_enable(rtk_host->clk_en_sdio_ip);

	sdhci_rtk_hw_init(host);

	if ((host->timing == MMC_TIMING_UHS_SDR50) || (host->timing == MMC_TIMING_UHS_SDR104))
		regmap_write(crt_base, SYS_PLL_SDIO1, rtk_host->preset_pll);

	reg = sdhci_readb(host, SDHCI_POWER_CONTROL);
	reg |= SDHCI_POWER_ON | SDHCI_POWER_330;
	sdhci_writeb(host, reg, SDHCI_POWER_CONTROL);

	sdhci_resume_host(host);

	pr_err("[SDIO] %s OK\n", __func__);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sdhci_rtk_pmops, sdhci_rtk_suspend, sdhci_rtk_resume);

static struct platform_driver sdhci_rtk_driver = {
	.driver		= {
		.name	= "sdhci-rtk",
		.of_match_table = sdhci_rtk_dt_match,
		.pm	= &sdhci_rtk_pmops,
	},
	.probe		= sdhci_rtk_probe,
	.remove		= sdhci_rtk_remove,
	.shutdown	= sdhci_rtk_shutdown,
};

module_platform_driver(sdhci_rtk_driver);

MODULE_DESCRIPTION("SDIO driver for Realtek DHC SoCs");
MODULE_LICENSE("GPL");
