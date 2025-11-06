#
# Copyright (C) 2022 RealTek Inc.
#
ccflags-y += -I$(srctree)/drivers/soc/realtek/common/
ccflags-y += -I$(srctree)/drivers/soc/realtek/common/rpc

# platform driver
obj-$(CONFIG_SND_ACPU_AO) += snd-soc-realtek.o
snd-soc-realtek-y += snd-realtek.o snd-realtek_rpc.o

obj-$(CONFIG_SND_HIFI_AO) += snd-soc-hifi-realtek.o
snd-soc-hifi-realtek-y += snd-hifi-realtek.o snd-hifi-realtek_rpc.o

obj-$(CONFIG_SND_HIFI_RTK_COMPRESS) += snd-soc-hifi-compr-rtk.o
snd-soc-hifi-compr-rtk-y += snd-hifi-rtk-compress.o snd-hifi-rtk-compr-rpc.o
