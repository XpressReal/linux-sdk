# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(src) # needed for trace events

#rtk_vi-y := rtk_virtual_in.o
rtk_vi-y := rtk_video_in.o \
	rtk_video_in_hw.o \
	rtk_video_in_trace.o

obj-$(CONFIG_RTK_VI) += rtk_vi.o
