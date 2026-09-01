# Copyright (C) 2025 Realtek Semiconductor Corp.

DESCRIPTION = "Realtek PCI BR I/O bridge driver"
LICENSE = "CLOSED"

DEPENDS = "virtual/kernel"
inherit module kernel-module-split

SRC_URI = " \
	file://pcibr-iodrv/Makefile \
	file://pcibr-iodrv/pcibr_iodrv.c \
	file://pcibr-iodrv/pcibr_iodrv.h \
	file://pcibr-iodrv/pcibr_iodrv_core.h \
	"

S = "${WORKDIR}/${BPN}"

module_do_compile() {
	make KERNELDIR=${STAGING_KERNEL_DIR}
}

module_do_install(){
	MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/misc
	install -d $MODULE_DIR
	install -m 644 ${S}/pcibr_iodrv.ko $MODULE_DIR/
}

INSANE_SKIP:${PN} += "buildpaths"
INSANE_SKIP:${PN}-dbg += "buildpaths"
