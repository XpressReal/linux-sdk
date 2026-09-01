#
# Makefile for the pcibr io device drivers.
#

ifneq ($(KERNELRELEASE),)
	obj-m := pcibr_iodrv.o
	#EXTRA_CFLAGS += -DDEBUG
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD :=$(shell pwd)

.PHONY:all
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

.PHONY:clean
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif
