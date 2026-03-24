SUMMARY = "Realtek v4l2 vo capture kernel driver"
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

SRC_URI:append:rtd16xx = "\
	file://0001-v4l2cap-Fix-attribute-warning-on-write-overflow.patch \
	"

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.

EXTRA_OEMAKE:stark += "KDIR=${STAGING_KERNEL_DIR}"
EXTRA_OEMAKE:kent += "KDIR=${STAGING_KERNEL_DIR}"
EXTRA_OEMAKE:rtd16xx += "KDIR=${STAGING_KERNEL_DIR}"

MODULES_INSTALL_TARGET += "-C ${STAGING_KERNEL_DIR} M=${S}"

KERNEL_MODULE_AUTOLOAD += "rtkcapture"

INSANE_SKIP:${PN}:rtd16xx += "installed-vs-shipped"
