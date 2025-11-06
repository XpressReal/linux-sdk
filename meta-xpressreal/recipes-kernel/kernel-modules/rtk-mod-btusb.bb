# Copyright (C) 2025 Realtek Semiconductor Corp.

DESCRIPTION = "RTK Bluetooth USB driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

DEPENDS = "virtual/kernel"
inherit module kernel-module-split

SRC_URI = " \
	file://${BPN}.tar.xz \
	"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

do_configure() {
	:
}

do_compile() {
	make ${PARALLEL_MAKE} -C ${STAGING_KERNEL_DIR} M=`pwd`/usb/bluetooth_usb_driver  modules
}

do_clean() {
	rm -f usb/bluetooth_usb_driver/{*.o,*.ko}
}

do_install(){
        MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/usb/bluetooth
	install -d $MODULE_DIR
	install -m 644 ${S}/usb/bluetooth_usb_driver/rtk_btusb.ko $MODULE_DIR/
}

KERNEL_MODULE_AUTOLOAD += "rtk_btusb"
