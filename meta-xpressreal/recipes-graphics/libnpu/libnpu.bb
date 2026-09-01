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
FILES:${PN} += "${sysconfdir}/npu/configs/*"
FILES:${PN}:remove:stark = "${sysconfdir}/npu/configs/*"
FILES:${PN} += "${sysconfdir}/profile.d/npu_env.sh"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

PACKAGES =+ "${PN}-header"
FILES:${PN}-header = "${includedir}/npu_header_CL/include/CL/cl_viv_vx_ext.h"
FILES:${PN}-dev += "${includedir}/npu_header"

FILES_SOLIBSDEV = ""

INSANE_SKIP:${PN} += "already-stripped"
INSANE_SKIP:${PN} += "ldflags"
INSANE_SKIP:${PN} += "dev-so"

SOC_NAME:stark = "1619b"
SOC_NAME:kent = "kent"
SOC_NAME:prince = "prince"

LIBNPU_PATH_VIPLITE = "viplite/library/64bit"
LIBNPU_PATH_OVXLIB   = "ovxlib/library/acuity-root-dir/lib/arm64"
LIBNPU_CONFIG_SRC     = "viplite/configs"
LIBNPU_CONFIG_PATH    = "npu/configs"
LIBNPU_CONFIG_PATH:stark = ""

VIV_GPU_CONFIG = ""
VIV_GPU_CONFIG:kent = "VIP9000ULDI_PLUS_PID0X10000046.config"
VIV_GPU_CONFIG:prince = "VIP9000NANOSI_PLUS_PID0X10000060.config"

TENSOR_FILTER_PATH_VIPLITE = "viplite-nnstreamer/prebuilt"
TENSOR_FILTER_PATH_OVXLIB = "ovxlib/NNStreamer/prebuilt"

LIBNPU_INC_VIPLITE  = "viplite/header"
LIBNPU_INC_OVXLIB    = "ovxlib/library/acuity-root-dir/include"

OVXLIB_INC  = "ovxlib/library/acuity-root-dir/ovxlib-package-dev/arm64/include"

LIBNPU_PATH:stark = "${S}/${LIBNPU_PATH_OVXLIB}/${SOC_NAME}"
LIBNPU_PATH = "${S}/${LIBNPU_PATH_VIPLITE}/${SOC_NAME}/"
LIBNPU_INC_PATH:stark = "${S}/${LIBNPU_INC_OVXLIB}"
LIBNPU_INC_PATH = "${S}/${LIBNPU_INC_VIPLITE}/${SOC_NAME}/include"
OVXLIB_INC_PATH:stark = "${S}/${OVXLIB_INC}"
TENSOR_FILTER_PREBUILT_PATH = "${S}/${TENSOR_FILTER_PATH_VIPLITE}"
TENSOR_FILTER_PREBUILT_PATH:stark = "${S}/${TENSOR_FILTER_PATH_OVXLIB}"

do_install() {
    install -d ${D}${libdir}
    install -d ${D}${sysconfdir}/profile.d
    install -d ${D}${includedir}/npu_header
    install -d ${D}${includedir}/npu_header_CL/include/CL
    install -d ${D}${libdir}/nnstreamer/filters
    install -m 0755 ${LIBNPU_PATH}/* ${D}${libdir}
    install -m 0755 ${TENSOR_FILTER_PREBUILT_PATH}/* ${D}${libdir}/nnstreamer/filters
    install -m 0644 ${LIBNPU_INC_PATH}/CL/cl_viv_vx_ext.h ${D}${includedir}/npu_header_CL/include/CL
    cp -r ${LIBNPU_INC_PATH}/* ${D}${includedir}/npu_header

    echo "export VIVANTE_SDK_DIR=${includedir}/npu_header_CL/" \
        >> ${D}${sysconfdir}/profile.d/npu_env.sh
    chmod 0644 ${D}${sysconfdir}/profile.d/npu_env.sh

    if [ -n "${LIBNPU_CONFIG_PATH}" ]; then
        install -d ${D}${sysconfdir}/${LIBNPU_CONFIG_PATH}
        install -m 0644 ${S}/${LIBNPU_CONFIG_SRC}/${VIV_GPU_CONFIG} ${D}${sysconfdir}/${LIBNPU_CONFIG_PATH}/
        echo "export VIV_GPU_FILE=${sysconfdir}/${LIBNPU_CONFIG_PATH}/${VIV_GPU_CONFIG}" \
            > ${D}${sysconfdir}/profile.d/npu_env.sh
    fi

    if [ -n "${OVXLIB_INC_PATH}" ]; then
        install -d ${D}${includedir}/npu_header/ovxlib
        cp -r ${OVXLIB_INC_PATH}/* ${D}${includedir}/npu_header/ovxlib
    fi
}