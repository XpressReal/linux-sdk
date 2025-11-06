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

SOC_NAME:stark = "1619b"
SOC_NAME:kent = "kent"

LIBNPU_PATH = "library/acuity-root-dir/lib/arm64/"
TENSOR_FILTER_PATH = "NNStreamer/prebuilt"
LIBNPU_INC  = "library/acuity-root-dir/include"
OVXLIB_INC  = "library/acuity-root-dir/ovxlib-package-dev/arm64/include"

do_install() {
    install -d ${D}${libdir}
    install -d ${D}${libdir}/nnstreamer/filters
    install -d ${D}${includedir}/npu_header
    install -d ${D}${includedir}/npu_header/ovxlib
    install -m 0755 ${S}/${LIBNPU_PATH}/${SOC_NAME}/* ${D}${libdir}
    install -m 0755 ${S}/${TENSOR_FILTER_PATH}/* ${D}${libdir}/nnstreamer/filters
    cp -r ${S}/${LIBNPU_INC}/* ${D}${includedir}/npu_header
    cp -r ${S}/${OVXLIB_INC}/* ${D}${includedir}/npu_header/ovxlib
}
