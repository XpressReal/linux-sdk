obj-$(CONFIG_SND_SOC_REALTEK) += common/
obj-$(CONFIG_SND_SOC_REALTEK_FL3236) += fl3236/
obj-$(CONFIG_SND_SOC_REALTEK) += aio_ctrl/

# machine driver
snd-soc-rtk-hifi-objs := \
       rtk-hifi-machine.o \

snd-soc-rtk-hifi-rt5650-objs := \
	rtk-hifi-rt5650-mc.o \

# platform driver
snd-soc-rtk-afe-objs := \
	rtk-hifi-rpc.o \
	rtk-dai-i2s.o \
	rtk-dai-pdm.o \
	rtk-dai-tdm.o \
	rtk-dai-adc.o \
	rtk-dai-aio.o \
	rtk-dai-dprx.o \
	rtk-dai-memif.o \
	rtk-afe-platdrv.o

obj-$(CONFIG_SND_SOC_RTK_HIFI_MACHINE) += snd-soc-rtk-hifi.o
obj-$(CONFIG_SND_SOC_RTK_HIFI_RT5650_MACHINE) += snd-soc-rtk-hifi-rt5650.o
obj-$(CONFIG_SND_SOC_RTK_PLATFORM) += snd-soc-rtk-afe.o
