# Copyright (C) 2024 Realtek Semiconductor Corp.

DESCRIPTION = "Realtek NPU kernel driver"
LICENSE = "CLOSED"

DEPENDS = "virtual/kernel"
inherit module kernel-module-split

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

SOC_PLATFORM:stark = "realtek-1619b"
SOC_PLATFORM:kent = "realtek-kent"

module_do_compile() {
	make AQROOT="${S}" ARCH_TYPE=arm64 SOC_PLATFORM="${SOC_PLATFORM}" KERNEL_DIR="${STAGING_KERNEL_DIR}" CROSS_COMPILE="${CROSS_COMPILE}" \
		CC="${CC} -fdebug-prefix-map=${TMPDIR}=. -fmacro-prefix-map=${TMPDIR}=."
}

module_do_install(){
    MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/npu
	install -d $MODULE_DIR
	install -m 644 ${S}/${module}/galcore.ko $MODULE_DIR/
}
