# Copyright (C) 2024 Realtek Semiconductor Corp.

DESCRIPTION = "rtk gst player example"
LICENSE = "CLOSED"

inherit pkgconfig

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

DEPENDS += "gstreamer1.0"
TARGET_CC_ARCH += "${LDFLAGS}"

do_install() {
        mkdir -p ${D}/${bindir}
        install -D -p -m0755 ${S}/gstplayer-example ${D}${bindir}/gstplayer-example
}

FILES:${PN} += "${bindir}/*"
