# SPDX-License-Identifier: GPL-2.0-only

rtk_dprx-y := rtk_dprx_v4l2.o \
	rtk_dprx_rbus.o \
	rtk_dprx_phy.o \
	rtk_dprx_aux.o \
	rtk_dprx_wrapper.o \
	rtk_dprx_mac.o \
	rtk_dprx_edid.o \
	rtk_dprx_audio.o \
	rtk_dprx_trace.o

# Link Training Module
rtk_dprx-y += rtk_dprx_link_training.o
rtk_dprx-y += rtk_dprx_lt_phy.o
rtk_dprx-y += rtk_dprx_lt_sysfs.o
rtk_dprx-y += rtk_dprx_lt_v4l2.o
rtk_dprx-y += rtk_dprx_lt_extcon.o

obj-$(CONFIG_RTK_DP_RX) += rtk_dprx.o
