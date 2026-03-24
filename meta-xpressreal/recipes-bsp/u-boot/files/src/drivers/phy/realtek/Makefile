# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_PHY_RTK_RTD_USB2PHY)	+= phy-rtk-usb2.o
ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_PHY_RTK_RTD_USB3PHY)	+= phy-rtk-usb3.o
ifdef CONFIG_TARGET_RTD1619B
obj-$(CONFIG_PHY_RTD_PCIE)	+= phy-rtk-pcie-rtd16xxb.o
else
obj-$(CONFIG_PHY_RTD_PCIE)	+= phy-rtk-pcie.o
endif
endif
