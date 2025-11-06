// SPDX-License-Identifier: GPL-2.0
/*
 *  phy-rtk-usb3dp.c RTK usb3.0 dp phy driver
 *
 * copyright (c) 2024 realtek semiconductor corporation
 *
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/nvmem-consumer.h>
#include <linux/regmap.h>
#include <linux/sys_soc.h>
#include <linux/mfd/syscon.h>
#include <linux/suspend.h>
#include <linux/phy/phy.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/phy.h>
#include <linux/usb/role.h>
#include <linux/usb/typec_mux.h>
#include <linux/usb/typec_altmode.h>
#include <linux/usb/typec_dp.h>
#include "phy-rtk-usb3dp.h"
#include <linux/extcon-provider.h>
#include <linux/extcon.h>

#define PHY_IO_TIMEOUT_USEC		(50000)
#define PHY_IO_DELAY_US			(100)

static const unsigned int usb_type_c_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_DISP_DP,
	EXTCON_NONE,
};

static int rtk_phy_write(void __iomem *addr, unsigned int offset, int length, unsigned int val)
{
        unsigned int value, temp;
        int j;

        value = readl(addr);
        temp = 1;

        for (j = 0; j < length; j++)
                temp = temp * 2;

        value = (value & ~((temp - 1) << offset)) | (val << offset);
        writel(value, addr);

        return value;
}

static void rtk_u3dp_phy_initial_status(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	void __iomem *iso_base = type_c->iso_base;
	u32 value = 0;
	u32 power_cut_0, power_cut_1, power_cut_2;
	int i = 0, pc0_default = 0;
	int lanes = 2, polarity = 0;

	type_c->pre_lane = 2;
	type_c->ss = true;
#if 0
	writel(readl(base + REG_U3DP_0088) | BIT(12) | BIT(14), (base + REG_U3DP_0088));
	writel(readl(base + PCS_USB31_DP14_DPHY1) | 0x1, (base + PCS_USB31_DP14_DPHY1));
	writel(0x0, iso_base);
	mdelay(100);
	writel(readl(base + PCS_USB31_DP14_DPHY4) &~ 0x1, (base + PCS_USB31_DP14_DPHY4));
#endif

	value = readl(base + PCS_USB31_DP14_DPHY1) & ~(0x3);
        writel(value | USB_LAN0_1_SELECT, (base + PCS_USB31_DP14_DPHY1));

	writel(0x0, iso_base);
        mdelay(100);

        value = SPD_CTRL0(0) | SPD_CTRL1(0) | SPD_CTRL2(1) | SPD_CTRL3(1) | RST_DLY(3);
        writel(value , (base + PCS_USB31_DP14_DPHY2));

        value = DP_TX_EN_L3(1) | DP_TX_EN_L2(1) | DP_TX_EN_L1(0) | DP_TX_EN_L0(0);
        writel(value , (base + PCS_USB31_DP14_DPHY3));

	/*clean pwr_cut reg first */
	pc0_default = readl(base + POWER_CUT_EN0) & 0x7fff;
	for (i = 0; i < 5; i ++)
		writel(0, (base + POWER_CUT_EN0 + i * 4));

	/* power cut */
	power_cut_0 = DP_AVDD09_TRX_CK_L1 | USB_AVDD09_TRX_CK_L1 |
		DP_AVDD09_RX_OOBS_IN_L2 | USB_AVDD09_RX_OOBS_IN_L2;
	writel(power_cut_0 | pc0_default, (base + POWER_CUT_EN0));

	power_cut_1 = DP_AVDD09_TRX_RX_L2 | USB_AVDD09_TRX_RX_L2;
	writel(power_cut_1, (base + POWER_CUT_EN1));

	power_cut_2 = DP_AVDD18_CMU_RX_L2 | DP_AVDD18_RX_OOBS_IN_L2 |
		USB_AVDD18_CMU_RX_L2 | USB_AVDD18_RX_OOBS_IN_L2;
	writel(power_cut_2, (base + POWER_CUT_EN2));

	pr_debug("0x9800773c = 0x%x\n", readl(iso_base));
	pr_debug("0x9814fe00 = 0x%x\n", readl(base + PCS_USB31_DP14_DPHY1));
	pr_debug("0x9814fe04 = 0x%x\n", readl(base + PCS_USB31_DP14_DPHY2));
	pr_debug("0x9814fe08 = 0x%x\n", readl(base + PCS_USB31_DP14_DPHY3));
	pr_debug("0x9814fe0c = 0x%x\n", readl(base + PCS_USB31_DP14_DPHY4));
	type_c->state = USB_NON_FLIP;

	extcon_set_state(type_c->edev, EXTCON_DISP_DP, 1);

        extcon_set_property(type_c->edev, EXTCON_DISP_DP,
                            EXTCON_PROP_USB_SS,
                            (union extcon_property_value)(int)lanes);
        extcon_set_property(type_c->edev, EXTCON_DISP_DP,
                            EXTCON_PROP_USB_TYPEC_POLARITY,
                            (union extcon_property_value)(int)polarity);

        extcon_sync(type_c->edev, EXTCON_DISP_DP);
}

static int do_rtk_phy_init(struct rtk_phy *rtk_phy, int index)
{
	struct type_c_data *type_c;
	void __iomem *base;

	type_c = &rtk_phy->type_c;
	base = type_c->base;

	rtk_u3dp_phy_initial_status(type_c);

	rtk_phy_write(base + RX_L1_DPHY1, 22, 3, 0x1);
	rtk_phy_write(base + TOP_AIF_11C, 22, 2, 0x1);
	rtk_phy_write(base + TOP_AIF_0A0, 4, 3, 0x1);
	rtk_phy_write(base + TOP_AIF_0FC, 8, 3, 0x0);
	rtk_phy_write(base + CMU_USB_DPHY6, 16, 13, 0x1fe);
	rtk_phy_write(base + CMU_USB_DPHY5, 4, 12, 0x357); 

	return 0;
}

static int rtk_phy_init(struct phy *phy)
{
	struct rtk_phy *rtk_phy = phy_get_drvdata(phy);
	int ret = 0;
	int i;
	unsigned long phy_init_time = jiffies;

	for (i = 0; i < rtk_phy->num_phy; i++)
		ret = do_rtk_phy_init(rtk_phy, i);

	dev_dbg(rtk_phy->dev, "Initialized RTK USB 3.0 DP PHY (take %dms)\n",
		jiffies_to_msecs(jiffies - phy_init_time));

	return ret;
}

static int rtk_phy_exit(struct phy *phy)
{
	return 0;
}

static const struct phy_ops ops = {
	.init		= rtk_phy_init,
	.exit		= rtk_phy_exit,
	.owner		= THIS_MODULE,
};

static int parse_phy_data(struct rtk_phy *rtk_phy)
{
	struct device *dev = rtk_phy->dev;
	struct device_node *np = dev->of_node;
	struct phy_parameter *phy_parameter;
	int ret = 0;
	int index;
	int na, ns, num_regs;

	na = of_n_addr_cells(np);
	ns = of_n_size_cells(np);
	num_regs = of_property_count_elems_of_size(np, "reg", (na + ns) * 4);

	rtk_phy->num_phy = num_regs -1;

	rtk_phy->phy_parameter = devm_kzalloc(dev, sizeof(struct phy_parameter) *
					      rtk_phy->num_phy, GFP_KERNEL);
	if (!rtk_phy->phy_parameter)
		return -ENOMEM;

	for (index = 0; index < rtk_phy->num_phy; index++) {
		phy_parameter = &((struct phy_parameter *)rtk_phy->phy_parameter)[index];

		phy_parameter->phy_reg.reg_mdio_ctl = of_iomap(dev->of_node, index);
	}

	return ret;
}

static void rtk_type_c_dp_setting(struct type_c_data *type_c)
{
	void __iomem *base = type_c->base;
	u32 value = 0, pc0_default = 0;
	u32 power_cut_0, power_cut_1, power_cut_2;
	int i = 0;

	type_c->state = DP_4;
	type_c->pre_lane = 4;

	/*clean pwr_cut reg first */
	pc0_default = readl(base + POWER_CUT_EN0) & 0x7fff;
	for (i = 0; i < 5; i ++)
		writel(0, (base + POWER_CUT_EN0 + i * 4));

	/* dp lan0,1,2,3 */
	value = readl(base + PCS_USB31_DP14_DPHY1) & ~(0x3);
	writel(value | DP_LAN0_1_2_3_SELECT, (base + PCS_USB31_DP14_DPHY1));

	writel(readl(base + PCS_USB31_DP14_DPHY4) | 0x1, (base + PCS_USB31_DP14_DPHY4));

        value = SPD_CTRL0(1) | SPD_CTRL1(1) | SPD_CTRL2(1) | SPD_CTRL3(1);
	writel(value , (base + PCS_USB31_DP14_DPHY2));

	value =	DP_TX_EN_L3(1) | DP_TX_EN_L2(1) | DP_TX_EN_L1(1) | DP_TX_EN_L0(1);
	writel(value , (base + PCS_USB31_DP14_DPHY3));

	/* power cut */
	power_cut_0 = DP_AVDD09_RX_OOBS_IN_L1 | USB_AVDD09_RX_OOBS_IN_L1 |
		USB_AVDD09_RX_OOBS_IN_L2 | USB_AVDD09_CMU_CK;
	writel(power_cut_0 | pc0_default, (base + POWER_CUT_EN0));

	power_cut_1 = DP_AVDD09_TRX_RX_L1 | USB_AVDD09_TRX_RX_L1 | USB_AVDD09_TRX_RX_L2;
	writel(power_cut_1, (base + POWER_CUT_EN1));

	power_cut_2 = DP_AVDD18_CMU_RX_L1 | DP_AVDD18_RX_OOBS_IN_L1 | USB_AVDD18_CMU_RX_L1 |
		USB_AVDD18_RX_OOBS_IN_L1 | USB_AVDD18_CMU_RX_L2 | USB_AVDD18_RX_OOBS_IN_L2;
	writel(power_cut_2, (base + POWER_CUT_EN2));

	value = (readl(base + REG_U3DP_0084) & ~REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L1_mask
                & ~REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L1_mask);
        writel(value, (base + REG_U3DP_0084));

        value = (readl(base + REG_U3DP_0090) & ~REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L1_mask
                & ~REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L1_mask);
        writel(value, (base + REG_U3DP_0090));
}

static void rtk_type_c_usb_dp_setting(struct type_c_data *type_c, int cc)
{
	void __iomem *base = type_c->base;
	u32 value = 0, pc0_default;
	u32 power_cut_0, power_cut_1, power_cut_2;
	int i = 0;

	type_c->state = USB_DP;
	type_c->pre_lane = 2;

	/*clean pwr_cut reg first */
	pc0_default = readl(base + POWER_CUT_EN0) & 0x7fff;
        for (i = 0; i < 5; i ++)
                writel(0, (base + POWER_CUT_EN0 + i * 4));

	if (cc == enable_cc1) {
		// usb01,dp23
		value = readl(base + PCS_USB31_DP14_DPHY1) & ~(0x3);
		writel(value | USB_LAN0_1_SELECT, (base + PCS_USB31_DP14_DPHY1));

		value = SPD_CTRL0(0) | SPD_CTRL1(0) | SPD_CTRL2(1) | SPD_CTRL3(1);
		writel(value , (base + PCS_USB31_DP14_DPHY2));

		value = DP_TX_EN_L3(1) | DP_TX_EN_L2(1) | DP_TX_EN_L1(0) | DP_TX_EN_L0(0);
		writel(value , (base + PCS_USB31_DP14_DPHY3));

		/* power cut */
		power_cut_0 = DP_AVDD09_TRX_CK_L1 | USB_AVDD09_TRX_CK_L1 |
				DP_AVDD09_RX_OOBS_IN_L2 | USB_AVDD09_RX_OOBS_IN_L2;
		writel(power_cut_0 | pc0_default, (base + POWER_CUT_EN0));

		power_cut_1 = DP_AVDD09_TRX_RX_L2 | USB_AVDD09_TRX_RX_L2;
		writel(power_cut_1, (base + POWER_CUT_EN1));

		power_cut_2 = DP_AVDD18_CMU_RX_L2 | DP_AVDD18_RX_OOBS_IN_L2 |
				USB_AVDD18_CMU_RX_L2 | USB_AVDD18_RX_OOBS_IN_L2;
		writel(power_cut_2, (base + POWER_CUT_EN2));
	} else if (cc == enable_cc2) {
		// usb23,dp01
		value = readl(base + PCS_USB31_DP14_DPHY1) & ~(0x3);
                writel(value | USB_LAN2_3_SELECT, (base + PCS_USB31_DP14_DPHY1));

                value = SPD_CTRL0(1) | SPD_CTRL1(1) | SPD_CTRL2(0) | SPD_CTRL3(0);
                writel(value , (base + PCS_USB31_DP14_DPHY2));

                value = DP_TX_EN_L3(0) | DP_TX_EN_L2(0) | DP_TX_EN_L1(1) | DP_TX_EN_L0(1);
                writel(value , (base + PCS_USB31_DP14_DPHY3));

		/* power cut */
		power_cut_0 = DP_AVDD09_RX_OOBS_IN_L1 | USB_AVDD09_RX_OOBS_IN_L1 | USB_AVDD09_TRX_CK_L2;
		writel(power_cut_0 | pc0_default, (base + POWER_CUT_EN0));

		power_cut_1 = DP_AVDD09_TRX_RX_L1 | USB_AVDD09_TRX_RX_L1;
		writel(power_cut_1, (base + POWER_CUT_EN1));

		power_cut_2 = DP_AVDD18_CMU_RX_L1 | DP_AVDD18_RX_OOBS_IN_L1 |
				USB_AVDD18_CMU_RX_L1 | USB_AVDD18_RX_OOBS_IN_L1;
		writel(power_cut_2, (base + POWER_CUT_EN2));
	} else {
		return;
	}

}

static void rtk_type_c_usb_setting(struct type_c_data *type_c, int cc)
{
	void __iomem *base = type_c->base;
	u32 value = 0, pc0_default;
	u32 power_cut_0, power_cut_1, power_cut_2, power_cut_3, power_cut_4;
	int i = 0;

	/*clean pwr_cut reg first */
	pc0_default = readl(base + POWER_CUT_EN0) & 0x7fff;
        for (i = 0; i < 5; i ++)
                writel(0, (base + POWER_CUT_EN0 + i * 4));

	type_c->pre_lane = 0;
        if (cc == enable_cc1) {
		/* usb lan0,1 */
		writel(USB_LAN0_1_SELECT, (base + PCS_USB31_DP14_DPHY1));
		value = SPD_CTRL0(0) | SPD_CTRL1(0) | SPD_CTRL2(0) | SPD_CTRL3(0);
		writel(value , (base + PCS_USB31_DP14_DPHY2));
		value = DP_TX_EN_L3(0) | DP_TX_EN_L2(0) | DP_TX_EN_L1(0) | DP_TX_EN_L0(0);
		writel(value , (base + PCS_USB31_DP14_DPHY3));
		writel(readl(base + PCS_USB31_DP14_DPHY4) &~ 0x1, (base + PCS_USB31_DP14_DPHY4));
		type_c->state = USB_NON_FLIP;

		/* pwr cut */
		power_cut_0 = DP_AVDD09_TRX_CK_L1 | DP_AVDD09_RX_OOBS_IN_L2 | DP_AVDD09_DPCMU_CK_CUT;
		writel(power_cut_0 | pc0_default, (base + POWER_CUT_EN0));

		power_cut_1 = DP_AVDD09_TRX_L2 | DP_AVDD09_TRX_RX_L2 |
                                DP_AVDD09_TX_CK_L3 | DP_AVDD09_TRX_CK_L2;
		power_cut_2 = DP_AVDD18_CMU_RX_L2 | DP_AVDD18_RX_OOBS_IN_L2 |
				DP_AVDD09_TX_L3 | DP_AVDD18_DPCMU_CUT;

		if (!type_c->ss) {
			power_cut_1 |= USB_AVDD09_TX_CK_L3 | USB_AVDD09_TX_CK_L0 | USB_AVDD09_TRX_L1;
			power_cut_2 |= USB_AVDD18_TRX_L1;
		}

		writel(power_cut_1, (base + POWER_CUT_EN1));
		writel(power_cut_2, (base + POWER_CUT_EN2));

		power_cut_3 = POWERCUT_EN_USB_L2 | DP_AVDD18_TX_L3 | DP_AVDD18_TRX_L2;
		writel(power_cut_3, (base + POWER_CUT_EN3));

		power_cut_4 = POWERCUT_EN_USB_L3;
		writel(power_cut_4, (base + POWER_CUT_EN4));
	} else if (cc == enable_cc2) {
		/* usb lan2,3 */
		writel(USB_LAN2_3_SELECT, (base + PCS_USB31_DP14_DPHY1));
		value = SPD_CTRL0(0) | SPD_CTRL1(0) | SPD_CTRL2(0) | SPD_CTRL3(0);
		writel(value , (base + PCS_USB31_DP14_DPHY2));
		value = DP_TX_EN_L3(0) | DP_TX_EN_L2(0) | DP_TX_EN_L1(0) | DP_TX_EN_L0(0);
		writel(value , (base + PCS_USB31_DP14_DPHY3));
		writel(readl(base + PCS_USB31_DP14_DPHY4) &~ 0x1, (base + PCS_USB31_DP14_DPHY4));
		type_c->state = USB_FLIP;

		/* pwr cut */
		power_cut_0 = DP_AVDD09_TRX_CK_L1 | DP_AVDD09_RX_OOBS_IN_L1 | DP_AVDD09_DPCMU_CK_CUT;
		writel(power_cut_0 | pc0_default, (base + POWER_CUT_EN0));

		power_cut_1 = DP_AVDD09_TX_CK_L0 | DP_AVDD09_TRX_L1 |
				DP_AVDD09_TRX_CK_L2 | DP_AVDD09_TRX_RX_L1;
		power_cut_2 = DP_AVDD09_TX_L0 | DP_AVDD18_TRX_L1 | DP_AVDD18_CMU_RX_L1 |
				DP_AVDD18_RX_OOBS_IN_L1 | DP_AVDD18_DPCMU_CUT;

		if (!type_c->ss) {
			power_cut_1 |= USB_AVDD09_TX_CK_L3 | USB_AVDD09_TX_CK_L0 | USB_AVDD09_TRX_L2;
			power_cut_2 |= USB_AVDD18_TRX_L2;
		}

		writel(power_cut_1, (base + POWER_CUT_EN1));
		writel(power_cut_2, (base + POWER_CUT_EN2));

		power_cut_3 = POWERCUT_EN_USB_L0 | DP_AVDD18_TX_L0 | POWERCUT_EN_USB_L1;
		writel(power_cut_3, (base + POWER_CUT_EN3));
	} else {
                return;
        }
}

static void usb01_en(struct type_c_data *type_c, int en)
{
	void __iomem *base = type_c->base;
	u32 value = 0;

	value = (readl(base + REG_U3DP_004C) &~(REG_U3DP_004C_usb_OOBS_REGSIG_L1_mask)
					&~ (REG_U3DP_004C_dp_OOBS_REGSIG_L1_mask)
					&~ (REG_U3DP_004C_usb_OOBS_L1_mask)
					&~(REG_U3DP_004C_dp_OOBS_L1_mask));
	writel((value | REG_U3DP_004C_usb_OOBS_REGSIG_L1(en) 
		| REG_U3DP_004C_dp_OOBS_REGSIG_L1(en) | REG_U3DP_004C_usb_OOBS_L1(en)
		| REG_U3DP_004C_dp_OOBS_L1(en)), (base + REG_U3DP_004C));

	value = (readl(base + REG_U3DP_0084) & ~REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L1_mask
                        & ~REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L1_mask);
                writel(value | REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L1(en)
			| REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L1(en), (base + REG_U3DP_0084));

	value = (readl(base + REG_U3DP_0090) & ~REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L1_mask
                & ~REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L1_mask);
        writel(value | REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L1(en)
		| REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L1(en) ,(base + REG_U3DP_0090));
}

static void usb23_en(struct type_c_data *type_c, int en)
{
        void __iomem *base = type_c->base;
        u32 value = 0;

        value = (readl(base + REG_U3DP_004C) &~(REG_U3DP_004C_usb_OOBS_REGSIG_L2_mask)
                                        &~ (REG_U3DP_004C_dp_OOBS_REGSIG_L2_mask)
                                        &~ (REG_U3DP_004C_usb_OOBS_L2_mask)
                                        &~(REG_U3DP_004C_dp_OOBS_L2_mask));
        writel((value | REG_U3DP_004C_usb_OOBS_REGSIG_L2(en)
                | REG_U3DP_004C_dp_OOBS_REGSIG_L2(en) | REG_U3DP_004C_usb_OOBS_L2(en)
                | REG_U3DP_004C_dp_OOBS_L2(en)), (base + REG_U3DP_004C));

        value = (readl(base + REG_U3DP_0084) & ~REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L2_mask
                        & ~REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L2_mask);
                writel(value | REG_U3DP_0084_usb_REG_RX_ENKOFFSET_L2(en)
                        | REG_U3DP_0084_dp_REG_RX_ENKOFFSET_L2(en), (base + REG_U3DP_0084));

        value = (readl(base + REG_U3DP_0090) & ~REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L2_mask
                & ~REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L2_mask);
        writel(value | REG_U3DP_0090_usb_REG_RX_PI_POW_SEL_L2(en)
                | REG_U3DP_0090_dp_REG_RX_PI_POW_SEL_L2(en) ,(base + REG_U3DP_0090));
}

static void rtk_usb01_chgto_dp(struct type_c_data *type_c, int cc, int lanes)
{
	void __iomem *base = type_c->base;

	if (lanes == 4) {
	//	usb01_en(type_c, 0);
		writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x10000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx0
		writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x20000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx1
		writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x40000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx2
                writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x80000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx3
		rtk_type_c_dp_setting(type_c);
		mdelay(10);

	} else {
		writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x40000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx2
		writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x80000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx3
		rtk_type_c_usb_dp_setting(type_c, cc);
		mdelay(10);
	}
}

static void rtk_usb23_chgto_dp(struct type_c_data *type_c, int cc, int lanes)
{
        void __iomem *base = type_c->base;

        if (lanes == 4) {
                //usb23_en(type_c, 0);
                writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x10000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx0
                writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x20000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx1
                writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x40000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx2
                writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x80000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx3
		rtk_type_c_dp_setting(type_c);
                mdelay(10);
        } else {
		writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x10000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx0
		writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x20000,
			(base + PCS_USB31_DP14_DPHY3)); //dis tx1
                rtk_type_c_usb_dp_setting(type_c, cc);
                mdelay(10);
        }
}

static void rtk_dp0123_chgto(struct type_c_data *type_c, int cc, int lanes)
{
        void __iomem *base = type_c->base;

	if (lanes == 0) {
		if (cc == enable_cc1) {
			// usb 0,1
			writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x10000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx0
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x20000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx1
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x40000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx2
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x80000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx3
			usb01_en(type_c, 0);
			mdelay(10);
			usb01_en(type_c, 1);
			rtk_type_c_usb_setting(type_c, cc);
		} else {
			// usb 2,3
			writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x10000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx0
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x20000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx1
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x40000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx2
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x80000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx3
			usb23_en(type_c, 0);
			mdelay(10);
			usb23_en(type_c, 1);
			rtk_type_c_usb_setting(type_c, cc);
		}
	} else if (lanes == 2) {
		if (cc == enable_cc1) {
		//usb01,dp23
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x10000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx0
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x20000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx1
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x40000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx2
                        writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x80000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx3

                        usb01_en(type_c, 0);
                        rtk_type_c_usb_dp_setting(type_c, cc);
			mdelay(3);
                        usb01_en(type_c, 1);
		} else {
		//usb23,dp01
			writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x10000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx0
			writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x20000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx1
			writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x40000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx2
	                writel(readl(base + PCS_USB31_DP14_DPHY3) &~ 0x80000,
				(base + PCS_USB31_DP14_DPHY3)); //dis tx3

			usb23_en(type_c, 0);
			rtk_type_c_usb_dp_setting(type_c, cc);
			mdelay(3);
			usb23_en(type_c, 1);
		}
	}
}

static int rtk_usb_type_c_plug_config(struct type_c_data *type_c, int cc, int lanes)
{
	void __iomem *iso_base = type_c->iso_base;
	void __iomem *base = type_c->base;
	int ret = 0;

	pr_info("cc = %d, pre_lane = %d, pre_state = %d, chg_to_lane = %d\n",
		cc, type_c->pre_lane, type_c->state, lanes);

	if (cc == disable_cc)
		return ret;

	if (lanes == type_c->pre_lane) {
combo:
		if (lanes == 4) {
			writel(0x71C70000 , (base + POWER_CUT_EN3));
			writel(0x38 , (base + POWER_CUT_EN4));
			mdelay(1000);
		//	writel(0x7E, iso_base);
		//	mdelay(100);
			rtk_type_c_dp_setting(type_c);
		} else if (lanes == 2) {
			rtk_type_c_usb_dp_setting(type_c, cc);
		} else {
			rtk_type_c_usb_setting(type_c, cc);
		}
	} else {
		switch(type_c->state)
		{
		case DP_4:
			writel(0x0, iso_base);
			mdelay(1000);
			rtk_dp0123_chgto(type_c, cc, lanes);
			break;
		case USB_NON_FLIP:
			writel(0x71C70000 , (base + POWER_CUT_EN3));
			writel(0x38 , (base + POWER_CUT_EN4));
			mdelay(1000);
		//	writel(0x7E, iso_base);
		//	mdelay(100);
			rtk_usb01_chgto_dp(type_c, cc, lanes);
			break;
		case USB_FLIP:
			writel(0x71C70000 , (base + POWER_CUT_EN3));
			writel(0x38 , (base + POWER_CUT_EN4));
			mdelay(1000);
		//	writel(0x7E, iso_base);
		//	mdelay(100);
			rtk_usb23_chgto_dp(type_c, cc, lanes);
			break;
		case USB_DP:
                        goto combo;
		default:
			break;
		}
	}

        return ret;
}

static int rtk_dp_get_port_lanes(struct type_c_data *type_c)
{
        union extcon_property_value property;
        int dptx;
        u8 lanes;

        dptx = extcon_get_state(type_c->edev, EXTCON_DISP_DP);
        if (dptx > 0) {
                extcon_get_property(type_c->edev, EXTCON_DISP_DP,
                                    EXTCON_PROP_USB_SS, &property);
                if (property.intval)
                        lanes = 2;
                else
                        lanes = 4;
        } else {
                lanes = 0;
        }

        return lanes;
}

static int __rtk_usb_type_c_host_check(struct type_c_data *type_c)
{
        int state;

        if (!type_c->edev)
                return -ENODEV;

        state = extcon_get_state(type_c->edev, EXTCON_USB_HOST);
        if (state < 0)
                state = 0;

        pr_debug("%s EXTCON_USB_HOST state=%d\n", __func__, state);
	return state;
}

static ssize_t state_change_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;

	return sprintf(buf, "0x%x\n", type_c->state);
}

static ssize_t state_change_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct rtk_phy *rtk_phy = dev_get_drvdata(dev);
	struct type_c_data *type_c = &rtk_phy->type_c;
	int cc_stat = 0;

	if (sysfs_streq(buf, "u1")) {
		dev_info(dev, "%s curret status: USB, cc1 host connect\n", __func__);
		rtk_usb_type_c_plug_config(type_c, 1, 0);
		type_c->state = USB_NON_FLIP;
	} else if (sysfs_streq(buf, "u2")) {
		dev_info(dev, "%s curret status: USB, cc2 host connect\n", __func__);
		rtk_usb_type_c_plug_config(type_c, 2, 0);
		type_c->state = USB_FLIP;
	} else if (sysfs_streq(buf, "d")) {
		if (type_c->state == USB_NON_FLIP)
			cc_stat = 2;
		else if (type_c->state == USB_FLIP)
			cc_stat = 1;
		dev_info(dev, "%s curret status: DP\n", __func__);
		rtk_usb_type_c_plug_config(type_c, type_c->state, 4);
		type_c->state = DP_4;
	} else if (sysfs_streq(buf, "c1")) {
		dev_info(dev, "%s curret status: USB_DP\n", __func__);
		rtk_usb_type_c_plug_config(type_c, 1, 2);
		type_c->state = USB_DP;
	} else if (sysfs_streq(buf, "c2")) {
		dev_info(dev, "%s curret status: DP_USB\n", __func__);
		rtk_usb_type_c_plug_config(type_c, 2, 2);
		type_c->state = USB_DP;
	} else {
		dev_info(dev, "%s ERROR INPUT\n", __func__);
		type_c->state = -1;
	}

	return count;
}
DEVICE_ATTR(state_change, S_IRUGO | S_IWUSR, state_change_show, state_change_store);

static int __rtk_usb_type_c_update(struct type_c_data *type_c)
{
        enum usb_role usb_role = USB_ROLE_NONE;
	enum typec_orientation orientation;
        int polarity = 0;
        int cc = 0;
        int host_state = 0;
        int lanes = 0;
        int mux = 0;

        lanes = rtk_dp_get_port_lanes(type_c);
        if (lanes == 4) {
                extcon_get_property(type_c->edev, EXTCON_DISP_DP,
                                    EXTCON_PROP_USB_TYPEC_POLARITY,
                                    (union extcon_property_value *)&polarity);
                extcon_get_property(type_c->edev, EXTCON_DISP_DP,
                                    EXTCON_PROP_USB_SS,
				    (union extcon_property_value *)&mux);
        } else {
                usb_role = USB_ROLE_HOST;
                extcon_get_property(type_c->edev, EXTCON_USB_HOST,
                        EXTCON_PROP_USB_TYPEC_POLARITY,
                        (union extcon_property_value *)&polarity);
                extcon_get_property(type_c->edev, EXTCON_USB_HOST,
                        EXTCON_PROP_USB_SS,
                        (union extcon_property_value *)&mux);
        }

	host_state = __rtk_usb_type_c_host_check(type_c);

	if (polarity == 0) {
                cc = enable_cc1;
		orientation = TYPEC_ORIENTATION_NORMAL;
        } else if (polarity == 1) {
                cc = enable_cc2;
		orientation = TYPEC_ORIENTATION_REVERSE;
        } else {
		pr_err("error polarity value\n");
	}

	if (!host_state)
		cc = TYPEC_ORIENTATION_NONE;

        pr_info("%s polarity=%d cc=%d mux=%d lanes=%d, host_state=%d\n",
                    __func__, polarity, cc, mux, lanes, host_state);

        rtk_usb_type_c_plug_config(type_c, cc, lanes);

        return NOTIFY_DONE;
}

static int __rtk_usb_type_c_dp_notifier(struct notifier_block *nb,
                             unsigned long event, void *ptr)
{
	struct type_c_data *type_c = container_of(nb, struct type_c_data, edev_nb);

	__rtk_usb_type_c_update(type_c);

        return NOTIFY_DONE;
}

static int rtk_udphy_orien_sw_set(struct typec_switch_dev *sw,
				 enum typec_orientation orien)
{
	struct rtk_phy *rtk_phy = typec_switch_get_drvdata(sw);
	int cc = 0;

	if (orien == TYPEC_ORIENTATION_NORMAL)
		cc = enable_cc1;
        else if (orien == TYPEC_ORIENTATION_REVERSE)
                cc = enable_cc2;
        else if (orien == TYPEC_ORIENTATION_NONE)
                cc = disable_cc;

	rtk_phy->flip = cc;
	pr_info("rtk_udphy_orien_sw_set, orien = %d, cc = %d\n", orien, cc);

	return 0;
}

static void rtk_udphy_orien_switch_unregister(void *data)
{
	struct rtk_phy *rtk_phy = data;

	typec_switch_unregister(rtk_phy->sw);
}

static int rtk_typec_switch_register(struct rtk_phy *rtk_phy)
{
	struct typec_switch_desc sw_desc = { };
	struct device *dev = rtk_phy->dev;

	sw_desc.drvdata = rtk_phy;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = rtk_udphy_orien_sw_set;

	rtk_phy->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(rtk_phy->sw)) {
		dev_err(rtk_phy->dev, "Error register typec orientation switch: %ld\n",
			PTR_ERR(rtk_phy->sw));
		return PTR_ERR(rtk_phy->sw);
	}

	pr_info("rtk_u3dp_phy_setup_typec_switch\n");
	return devm_add_action_or_reset(dev,
					rtk_udphy_orien_switch_unregister, rtk_phy);
}

static int rtk_udphy_typec_mux_set(struct typec_mux_dev *mux,
                                 struct typec_mux_state *state)
{
        struct rtk_phy *rtk_phy = typec_mux_get_drvdata(mux);
	struct type_c_data *type_c = &rtk_phy->type_c;
	int cc = 0;
	u8 lanes;
	bool polarity = false;
	bool dp = false;

	switch (state->mode) {
	case TYPEC_STATE_USB:
		rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_USB;
		rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_USB;
                rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_USB;
                rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_USB;
		lanes = 0;
		break;

	case TYPEC_DP_STATE_C:
	case TYPEC_DP_STATE_E:
		rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_DP;
		rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_DP;
		rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_DP;
		rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_DP;
		lanes = 4;
		break;

	case TYPEC_DP_STATE_D:
	default:
		if (rtk_phy->flip == enable_cc2) {
			rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_DP;
			rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_DP;
			rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_USB;
			rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_USB;
		} else {
			rtk_phy->lane_mux_sel[0] = PHY_LANE_MUX_USB;
			rtk_phy->lane_mux_sel[1] = PHY_LANE_MUX_USB;
			rtk_phy->lane_mux_sel[2] = PHY_LANE_MUX_DP;
			rtk_phy->lane_mux_sel[3] = PHY_LANE_MUX_DP;
		}
		lanes = 2;
		break;
	}

	cc = rtk_phy->flip;
	pr_info("%s, state = %lu\n", __func__, state->mode);
	rtk_usb_type_c_plug_config(type_c, cc, lanes);

	polarity = (rtk_phy->flip == enable_cc2) ? 1 : 0;

	if (lanes != 0)
		dp = 1;

	extcon_set_state(type_c->edev, EXTCON_DISP_DP, dp);

	extcon_set_property(type_c->edev, EXTCON_DISP_DP,
                            EXTCON_PROP_USB_SS,
                            (union extcon_property_value)(int)lanes);
	extcon_set_property(type_c->edev, EXTCON_DISP_DP,
                            EXTCON_PROP_USB_TYPEC_POLARITY,
                            (union extcon_property_value)(int)polarity);

	extcon_sync(type_c->edev, EXTCON_DISP_DP);
        return 0;
}

static void rtk_udphy_typec_mux_unregister(void *data)
{
	struct rtk_phy *rtk_phy = data;

	typec_mux_unregister(rtk_phy->mux);
}

static int rtk_typec_mux_register(struct rtk_phy *rtk_phy)
{
	struct typec_mux_desc mux_desc = {};
        struct device *dev = rtk_phy->dev;

        mux_desc.drvdata = rtk_phy;
        mux_desc.fwnode = dev->fwnode;
        mux_desc.set = rtk_udphy_typec_mux_set;

        rtk_phy->mux = typec_mux_register(dev, &mux_desc);
        if (IS_ERR(rtk_phy->mux)) {
                dev_err(rtk_phy->dev, "Error register typec orientation switch: %ld\n",
                        PTR_ERR(rtk_phy->mux));
                return PTR_ERR(rtk_phy->mux);
        }

	pr_info("rtk_u3dp_phy_setup_mux_switch\n");
        return devm_add_action_or_reset(dev,
			rtk_udphy_typec_mux_unregister, rtk_phy);
}

static int u3dp_phy_setup_type_c_device(struct rtk_phy *rtk_phy)
{
	struct device_node *node = NULL;
	struct type_c_data *type_c;
	struct device *dev = rtk_phy->dev;
	int ret;

	type_c = &rtk_phy->type_c;

#ifdef CONFIG_CHROME_PLATFORMS
	node = of_parse_phandle(dev_of_node(rtk_phy->dev), "realtek,type-c", 0);
#endif

        if (node) {
		type_c->edev = extcon_find_edev_by_node(node);
		if (IS_ERR(type_c->edev)) {
			pr_err("Get extcon type_c fail (ret=%ld)\n",
                            PTR_ERR(type_c->edev));
			ret = (int)PTR_ERR(type_c->edev);
			type_c->edev = NULL;
			return ret;
		} else {
			pr_info("success to register edev\n");
			type_c->edev_nb.notifier_call = __rtk_usb_type_c_dp_notifier;
			ret = extcon_register_notifier(type_c->edev, EXTCON_USB, &type_c->edev_nb);
			if (ret < 0) {
				pr_err("couldn't register cable notifier\n");
				return ret;
			}
		}
	} else {
		ret = rtk_typec_switch_register(rtk_phy);
		if (ret) {
			pr_err("rtk_typec_switch_register failed\n");
			return ret;
		}
		ret = rtk_typec_mux_register(rtk_phy);
		if (ret) {
			pr_err("rtk_typec_mux_register failed\n");
			return ret;
		}

		type_c->edev = devm_extcon_dev_allocate(dev, usb_type_c_cable);
		if (IS_ERR(type_c->edev)) {
			dev_err(dev, "failed to allocate extcon device\n");
			return -ENOMEM;
		}

		ret = devm_extcon_dev_register(dev, type_c->edev);
		if (ret < 0) {
			dev_err(dev, "failed to register extcon device\n");
			return ret;
		}

		extcon_set_property_capability(type_c->edev, EXTCON_DISP_DP,
					EXTCON_PROP_USB_SS);
		extcon_set_property_capability(type_c->edev, EXTCON_DISP_DP,
					EXTCON_PROP_USB_TYPEC_POLARITY);
        }
        return 0;
}

static void rtk_usb_typec_sub_probe_work(struct work_struct *work)
{
	struct rtk_phy *rtk_phy = container_of(work, struct rtk_phy, delayed_work.work);
        struct device *dev = rtk_phy->dev;
        int ret = 0;
        unsigned long probe_time = jiffies;

        dev_info(dev, "%s Start ...\n", __func__);

        ret = u3dp_phy_setup_type_c_device(rtk_phy);
        if (ret == -EPROBE_DEFER)
                queue_delayed_work(rtk_phy->wq_typec_phy, &rtk_phy->delayed_work,
                        msecs_to_jiffies(1000));

        dev_info(dev, "%s ... End (take %d ms)\n", __func__,
                    jiffies_to_msecs(jiffies - probe_time));

}

static int rtk_usb3phy_probe(struct platform_device *pdev)
{
	struct rtk_phy *rtk_phy;
	struct device *dev = &pdev->dev;
	struct phy *generic_phy;
	struct phy_provider *phy_provider;
	struct resource *res;
	const struct phy_cfg *phy_cfg;
	struct type_c_data *type_c;
	int ret;

	phy_cfg = of_device_get_match_data(dev);
	if (!phy_cfg) {
		dev_err(dev, "phy config are not assigned!\n");
		return -EINVAL;
	}

	rtk_phy = devm_kzalloc(dev, sizeof(*rtk_phy), GFP_KERNEL);
	if (!rtk_phy)
		return -ENOMEM;

	rtk_phy->dev			= &pdev->dev;
	rtk_phy->phy.dev		= rtk_phy->dev;
	rtk_phy->phy.label		= "rtk-usb3phy";

	rtk_phy->phy_cfg = devm_kzalloc(dev, sizeof(*phy_cfg), GFP_KERNEL);

	memcpy(rtk_phy->phy_cfg, phy_cfg, sizeof(*phy_cfg));

	ret = parse_phy_data(rtk_phy);
	if (ret)
		goto err;

	type_c = &rtk_phy->type_c;
	rtk_phy->wq_typec_phy = create_singlethread_workqueue("rtk_typec_u3phy");

	INIT_DELAYED_WORK(&rtk_phy->delayed_work, rtk_usb_typec_sub_probe_work);
        rtk_usb_typec_sub_probe_work(&rtk_phy->delayed_work.work);

	type_c->iso_base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(type_c->iso_base))
		return PTR_ERR(type_c->iso_base);

	type_c->base = devm_platform_get_and_ioremap_resource(pdev, 1, &res);
	if (IS_ERR(type_c->base))
		return PTR_ERR(type_c->base);

	type_c->pre_lane = -1;
	platform_set_drvdata(pdev, rtk_phy);

	generic_phy = devm_phy_create(rtk_phy->dev, NULL, &ops);
	if (IS_ERR(generic_phy))
		return PTR_ERR(generic_phy);

	phy_set_drvdata(generic_phy, rtk_phy);

	phy_provider = devm_of_phy_provider_register(rtk_phy->dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	ret = usb_add_phy_dev(&rtk_phy->phy);
	if (ret)
		goto err;

	ret = device_create_file(&pdev->dev, &dev_attr_state_change);
	if (ret < 0)
		dev_err(&pdev->dev, "device_create_state_change fail (ret=%d)\n", ret);

	type_c->ss = false;
	writel(POWERCUT_1_USB, (type_c->base + POWER_CUT_EN1));
	writel(POWERCUT_2_USB, (type_c->base + POWER_CUT_EN2));
err:
	return ret;
}

static int rtk_usb3phy_remove(struct platform_device *pdev)
{
	struct rtk_phy *rtk_phy = platform_get_drvdata(pdev);
	int index;

	for (index = 0; index < rtk_phy->num_phy; index++) {
		struct phy_parameter *phy_parameter;

		phy_parameter = &((struct phy_parameter *)rtk_phy->phy_parameter)[index];

		iounmap(phy_parameter->phy_reg.reg_mdio_ctl);
	}

	device_remove_file(&pdev->dev, &dev_attr_state_change);

	usb_remove_phy(&rtk_phy->phy);

	return 0;
}

static const struct phy_cfg rtd1625_phy_cfg = {
	.param_size = MAX_USB_PHY_DATA_SIZE,
//	.param = {  [1] = {0x01, 0xac8c},
//		    [6] = {0x06, 0x0017},
//		    [9] = {0x09, 0x724c},
//		   [10] = {0x0a, 0xb610},
//		   [11] = {0x0b, 0xb90d},
//		   [13] = {0x0d, 0xef2a},
//		   [15] = {0x0f, 0x9050},
//		   [16] = {0x10, 0x000c},
//		   [32] = {0x20, 0x70ff},
//		   [34] = {0x22, 0x0013},
//		   [35] = {0x23, 0xdb66},
//		   [38] = {0x26, 0x8609},
//		   [41] = {0x29, 0xff13},
//		   [42] = {0x2a, 0x3070}, },
	.use_default_parameter = false,
	.check_rx_front_end_offset = false,
};

#ifdef CONFIG_PM
static int rtk_usb3phy_suspend(struct device *dev)
{
	struct rtk_phy *rtk_phy  = dev_get_drvdata(dev);
	struct type_c_data *type_c;
	int i;

	dev_info(dev, "%s enter u3dp suspend\n", __func__);
	type_c = &rtk_phy->type_c;

	writel(0x1fe, type_c->iso_base);

	for (i = 0; i < 5; i ++)
                writel(0xfffffff, (type_c->base + POWER_CUT_EN0 + i * 4));

	return 0;
}

static int rtk_usb3phy_resume(struct device *dev)
{
	struct rtk_phy *rtk_phy  = dev_get_drvdata(dev);
	struct type_c_data *type_c;
	int i;

	type_c = &rtk_phy->type_c;
	dev_info(dev, "%s enter u3dp resume\n", __func__);

	for (i = 0; i < 5; i ++)
                writel(0x0, (type_c->base + POWER_CUT_EN0 + i * 4));
	rtk_u3dp_phy_initial_status(type_c);

	return 0;
}
#else
static int rtk_usb3phy_suspend(struct device *dev)
{
        dev_err(dev,"User should enable CONFIG_PM kernel config\n");

        return 0;
}
static int rtk_usb3phy_resume(struct device *dev)
{
        dev_err(dev, "User should enable CONFIG_PM kernel config\n");

        return 0;
}
#endif /*CONFIG_PM*/

static const struct dev_pm_ops rtk_usb3phy_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rtk_usb3phy_suspend, rtk_usb3phy_resume)
};

static const struct of_device_id usbphy_rtk_dt_match[] = {
	{ .compatible = "realtek,rtd1625-usb3dp-phy", .data = &rtd1625_phy_cfg },
	{},
};
MODULE_DEVICE_TABLE(of, usbphy_rtk_dt_match);

static struct platform_driver rtk_usb3dp_phy_driver = {
	.probe		= rtk_usb3phy_probe,
	.remove		= rtk_usb3phy_remove,
	.driver		= {
		.name	= "rtk-usb3-dp-phy",
		.pm	= &rtk_usb3phy_pm_ops,
		.of_match_table = usbphy_rtk_dt_match,
	},
};

module_platform_driver(rtk_usb3dp_phy_driver);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: rtk-usb3-dp-phy");
MODULE_AUTHOR("Jyan Chou <jyanchou@realtek.com>");
MODULE_DESCRIPTION("Realtek usb 3.0 dp phy driver");
