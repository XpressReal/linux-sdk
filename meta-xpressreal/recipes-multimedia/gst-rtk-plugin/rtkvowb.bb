# Copyright (C) 2024 Realtek Semiconductor Corp.
DESCRIPTION = "rtk gst plugins, rtkvowb"
LICENSE = "CLOSED"

inherit pkgconfig

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

DEPENDS += "gstreamer1.0 gstreamer1.0-plugins-base rtkdmaallocator libdrm"
RDEPENDS:${PN} += "rtkdmaallocator"

TARGET_CC_ARCH += "${LDFLAGS}"

do_install() {
        mkdir -p ${D}/${libdir}/gstreamer-1.0/
        cp ${S}/*.so ${D}/${libdir}/gstreamer-1.0/
}

FILES:${PN} += "${libdir}/gstreamer-*/*.so"
INSANE_SKIP:${PN} += "installed-vs-shipped"
PACKAGE_DEBUG_SPLIT_STYLE = "debug-without-src"
