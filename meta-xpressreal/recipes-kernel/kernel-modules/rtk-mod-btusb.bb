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

DRV_SRC_PATH = "linux/usb/bluetooth_usb_driver"
DRV_SRC_PATH:realtekevb-rtd16xx-android = "android/linux/drivers/bluetooth"
DRV_SRC_PATH:realtekevb-rtd16xx-android-z0e = "android/linux/drivers/bluetooth"

do_compile() {
	make ${PARALLEL_MAKE} -C ${STAGING_KERNEL_DIR} M=`pwd`/${DRV_SRC_PATH}  modules
}

do_install(){
        MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/usb/bluetooth
	install -d $MODULE_DIR
	install -m 644 ${S}/${DRV_SRC_PATH}/rtk_btusb.ko $MODULE_DIR/
}

# Ignore buildpaths check for the main package and the debug package
INSANE_SKIP:${PN} += "buildpaths"
INSANE_SKIP:${PN}-dbg += "buildpaths"

KERNEL_MODULE_AUTOLOAD += "rtk_btusb"
