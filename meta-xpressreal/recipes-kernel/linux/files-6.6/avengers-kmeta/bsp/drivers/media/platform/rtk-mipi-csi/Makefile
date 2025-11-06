# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(src) # needed for trace events

rtk_mipicsi-y := rtk_mipi_csi.o \
		rtk_mipi_csi_hw.o \
		rtk_mipi_csi_trace.o \
		rtk_mipi_csi_dbg.o

obj-$(CONFIG_RTK_MIPI_CSI) += rtk_mipicsi.o
