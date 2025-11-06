# Copyright (C) 2024 Realtek Semiconductor Corp.
DESCRIPTION = "rtk gst plugins, rtkstt"
LICENSE = "CLOSED"

inherit pkgconfig

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

DEPENDS += "gstreamer1.0 gstreamer1.0-plugins-base libsndfile1 libnpu libtorch stt-demo tinyllama-gguf llama-cpp"
RDEPENDS_${PN} += "gstreamer1.0-plugins-base libnpu"
TARGET_CC_ARCH += "${LDFLAGS}"

do_install() {
        install -d ${D}${bindir}
        mkdir -p ${D}/${libdir}/gstreamer-1.0/
        install -m 0755 ${B}/*.so ${D}${libdir}/gstreamer-1.0/
        install -m 0755  ${B}/stt_unitest ${D}${bindir}
}

FILES:${PN} += "${libdir}/gstreamer-*/*.so"
INSANE_SKIP:${PN} += "installed-vs-shipped"
PACKAGE_DEBUG_SPLIT_STYLE = "debug-without-src"
