# Copyright (C) 2024 Realtek Semiconductor Corp.

SUMMARY = "libnpu"
LICENSE = "CLOSED"

inherit bin_package pkgconfig

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

DEPENDS = "nnstreamer"

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"

FILES:${PN} += "${libdir}/*"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

FILES_SOLIBSDEV = ""


INSANE_SKIP:${PN} += "already-stripped"
INSANE_SKIP:${PN} += "ldflags"
INSANE_SKIP:${PN} += "dev-so"

LIBNPU_PATH = "library/acuity-root-dir/lib/arm64/1619b"
TENSOR_FILTER_PATH = "NNStreamer/prebuilt"

do_install() {
    install -d ${D}${libdir}
    install -d ${D}${libdir}/nnstreamer/filters
    install -m 0755 ${S}/${LIBNPU_PATH}/* ${D}${libdir}
    install -m 0755 ${S}/${TENSOR_FILTER_PATH}/* ${D}${libdir}/nnstreamer/filters
}
