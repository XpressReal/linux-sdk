PWD=$(shell pwd)

KDIR ?= $(PWD)/$(LINUX_KERNEL_PATH)
ROOTFSDIR ?=

ifeq ($(KDIR),)
$(error Must specify KDIR to build this module)
endif

rtkvdec-y := rtk_vcodec/rtk_vcodec_drv.o rtk_vcodec/rtk_vcodec_dec.o rtk_vcodec/rtk_vcodec_h264.o rtk_vcodec/rtk_vcodec_vp8.o
# rtkve1-y := ve1_v4l2.o ve1_decode.o ve1_mem.o ve1_mm.o ve1_vdi.o ve1_vdi_osal.o ve1_vpu.o ve1_product.o ve1_vpuapifunc.o ve1_vpuapi.o ve1_wrapper.o
# rtkve2-y := ve2.o ve2rpc.o

obj-m += rtkvdec.o #rtkve1.o rtkve2.o

# ifdef CONFIG_COMPAT
# 	rtkve1-y       += ve1_mem_compat.o
# endif

DECFLAGS = -DANDROID -DENABLE_SHOW_VIDEO_INFO

ifeq ($(CONFIG_ENABLE_CC), y)
	DECFLAGS += -DVPU_GET_CC
endif

ifeq ($(CONFIG_ENABLE_DV), y)
	rtkvdec-y += dvca/SimpVesSplit.o dvca/SimpVesSplitter.o
	rtkvdec-y += dvca/ringbuffer.o dvca/DVMetadataParser.o dvca/codec_metadata_context.o dvca/codec_metadata_wrapper.o dvca/DolbyParserWrapper.o
	rtkvdec-y += dvca/dovi_common.o
	DECFLAGS += -DDECODER_DOVI
endif

GIT_VERSION := $(shell git rev-parse --short HEAD)
DECFLAGS +=-DGIT_VERSION=\"$(GIT_VERSION)\"

.PHONY: all modules_install clean

all:
	$(MAKE) V=$(V) -C $(KDIR) M=$(PWD) EXTRA_CFLAGS="-I$(PWD) -I$(KDIR)/drivers/soc/realtek/rtd16xxb/rtk_ve/ve1 $(DECFLAGS)"

modules_install:
	$(MAKE) V=$(V) -C $(KDIR) M=$(PWD) modules_install

clean:
	$(MAKE) V=$(V) -C $(KDIR) M=$(PWD) clean
