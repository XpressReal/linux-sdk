# Copyright (C) 2024 Realtek Semiconductor Corp.

SUMMARY = "npu header"
LICENSE = "CLOSED"

SRC_URI = "\
         file://npu_env.sh \
         file://cl_viv_vx_ext.h \
        "

do_install() {
    install -d ${D}${sysconfdir}/profile.d
    install -d ${D}${includedir}/npu/include/CL
    install -m 0644 ${WORKDIR}/npu_env.sh ${D}${sysconfdir}/profile.d/
    install -m 0644 ${WORKDIR}/cl_viv_vx_ext.h ${D}${includedir}/npu/include/CL
}

FILES:${PN} += "${sysconfdir}/profile.d/npu_env.sh"
FILES:${PN}-dev += "${includedir}/npu/include/CL/cl_viv_vx_ext.h"
