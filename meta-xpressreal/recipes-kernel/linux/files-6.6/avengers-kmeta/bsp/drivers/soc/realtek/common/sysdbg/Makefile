#SPDX - License - Identifier : GPL - 2.0
MODULE_NAME := sysdbg
obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := sysdbg_drv.o sysdbg_test.o sysdbg_sysfs.o
all:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODULE_NAME) -objs

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODULE_NAME) -objs modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(MODULE_NAME) -objs clean
