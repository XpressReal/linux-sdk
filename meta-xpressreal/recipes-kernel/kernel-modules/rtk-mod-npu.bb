# Copyright (C) 2024 Realtek Semiconductor Corp.

DESCRIPTION = "Realtek NPU kernel driver"
LICENSE = "CLOSED"

DEPENDS = "virtual/kernel"
inherit module kernel-module-split

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

SOC_PLATFORM:stark = "realtek-1619b"
SOC_PLATFORM:kent = "kent"
SOC_PLATFORM:prince = "prince"

S:stark = "${WORKDIR}/${BPN}-${PV}/ovxlib"
S = "${WORKDIR}/${BPN}-${PV}/viplite"

SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

ENABLE_40BITVA:kent = "0"
ENABLE_40BITVA:prince = "1"

module_do_compile() {
	make -f makefile.linux VIPLITE_ROOT="${S}" ARCH_TYPE=arm64 CPU_TYPE=0 CPU_ARCH=0 \
		VENDOR_NAME="realtek" SOC_CONFIG="${SOC_PLATFORM}" \
		USE_LINUX_PLATFORM_DEVICE=1 AUTO_CORRECT_CONFLICTS=1 ENABLE_MMU=1 ENABLE_40BITVA="${ENABLE_40BITVA}" \
		KERNEL_DIR="${STAGING_KERNEL_DIR}" CROSS_COMPILE="${CROSS_COMPILE}" \
		CC="${CC} -fdebug-prefix-map=${TMPDIR}=. -fmacro-prefix-map=${TMPDIR}=. -Wno-error=missing-prototypes"
}

module_do_compile:stark() {
	make AQROOT="${S}" ARCH_TYPE=arm64 SOC_PLATFORM="${SOC_PLATFORM}" KERNEL_DIR="${STAGING_KERNEL_DIR}" CROSS_COMPILE="${CROSS_COMPILE}" \
		CC="${CC} -fdebug-prefix-map=${TMPDIR}=. -fmacro-prefix-map=${TMPDIR}=."
}

module_do_install(){
    MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/npu
	install -d $MODULE_DIR
	install -m 644 ${S}/vip_drv/src/vipcore.ko $MODULE_DIR/
}

module_do_install:stark(){
    MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/npu
	install -d $MODULE_DIR
	install -m 644 ${S}/galcore.ko $MODULE_DIR/
}