DESCRIPTION = "rtk kms ipc ui tool (wayland/gtk)"
LICENSE = "CLOSED"

inherit pkgconfig

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

TARGET_CC_ARCH += "${LDFLAGS}"
DEPENDS += "gtk+3 libdrm rtk-media-framework"

do_install() {
      install -d ${D}${bindir}
      install -m0755 ${S}/kms_ipc_ui ${D}${bindir}/kms_ipc_ui
}

FILES:${PN} += "${bindir}/kms_ipc_ui"
