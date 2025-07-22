# Copyright (C) 2024 Realtek Semiconductor Corp.

DESCRIPTION = "rtk gst plugins, shmallocator"
LICENSE = "CLOSED"

inherit pkgconfig

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

DEPENDS += "gstreamer1.0 gstreamer1.0-plugins-base"
TARGET_CC_ARCH += "${LDFLAGS}"

do_install() {
        mkdir -p ${D}/${libdir}
		mkdir -p ${D}/${includedir}
		cp ${S}/*.so ${D}/${libdir}
		cp ${S}/*.h ${D}/${includedir}
}

FILES:${PN} += "${libdir}/*.so"
INSANE_SKIP:${PN} += "installed-vs-shipped"
PACKAGE_DEBUG_SPLIT_STYLE = "debug-without-src"

PROVIDES = "rtkshmallocator"
RPROVIDES:${PN} = "rtkshmallocator"
INSANE_SKIP_${PN} += "file-rdeps"
PACKAGES = "${PN}"
