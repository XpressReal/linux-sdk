# SPDX-License-Identifier: GPL-2.0-only

ENABLE_NPP_ISR = YES
ENABLE_NPP_FPGA_TEST = NO
ENABLE_NPP_MEASURE_TIME = NO

ifeq ($(ENABLE_NPP_ISR), YES)
  ccflags-y += -DENABLE_NPP_ISR
endif

ifeq ($(ENABLE_NPP_FPGA_TEST), YES)
  ccflags-y += -DENABLE_NPP_FPGA_TEST
endif

ifeq ($(ENABLE_NPP_MEASURE_TIME), YES)
  ccflags-y += -DENABLE_NPP_MEASURE_TIME
endif

rtk_npp-y := drv_npp.o npp_capture.o npp_m2m.o npp_ctrl.o npp_register.o
obj-$(CONFIG_RTK_NPP) += rtk_npp.o
