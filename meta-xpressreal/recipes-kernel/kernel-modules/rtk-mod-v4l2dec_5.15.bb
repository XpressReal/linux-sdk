SUMMARY = "Realtek v4l2 video codec kernel driver"
LICENSE = "CLOSED"
DEPENDS = "virtual/kernel"
inherit module
COMPATIBLE_MACHINE:realtekevb-rtd1619b = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b-mini = "stark"
COMPATIBLE_MACHINE:phantom-rtd1625 = "kent"

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.

EXTRA_OEMAKE:stark += "KDIR=${STAGING_KERNEL_DIR}"
EXTRA_OEMAKE:kent += "KDIR=${STAGING_KERNEL_DIR}"

MODULES_INSTALL_TARGET += "-C ${STAGING_KERNEL_DIR} M=${S}"

KERNEL_MODULE_AUTOLOAD += "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', ' coda9 rtkve ', ' rtkvdec rtkve1 rtkve2 rtkve_enc ', d)}"
