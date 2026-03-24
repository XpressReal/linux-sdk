# Copyright (C) 2024 Realtek Semiconductor Corp.

DESCRIPTION = "Realtek WiFi kernel driver"
LICENSE = "CLOSED"

DEPENDS = "virtual/kernel"
inherit module kernel-module-split

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

SRC_URI:append = "\
	file://0001-rtl8733bu-fix-compile-error-in-kernel-6.6.patch \
	file://0002-rtl8822be-fix-compile-error-in-kernel-6.6.patch \
	file://0003-rtl8822ce-fix-compile-error-in-kernel-6.6.patch \
	"

SRC_URI:append:rtd16xx = "\
	file://0004-rtl8822es-fix-enum-conversion.patch \
	"

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

DEPENDS += "bc-native"

WIFI_MODULES = "rtl8852be rtl8852bs rtl8822es rtl8822cs rtl8821cu rtl8733bu rtl8822be rtl8822ce"
WIFI_MODULES:rtd16xx = "rtl8822es"
TARGET_PLATFORM = "CONFIG_PLATFORM_I386_PC=n CONFIG_PLATFORM_RTK1319=n CONFIG_PLATFORM_RTKSTB=y"

module_do_compile() {
	for module in ${WIFI_MODULES}; do
		make ${PARALLEL_MAKE} -C ${module} ${TARGET_PLATFORM} KSRC="${STAGING_KERNEL_DIR}" UPSTREAM_KSRC=y PLTFM_VER=0 CROSS="${CROSS_COMPILE}" all
	done
}

module_do_install(){
        MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/wifi
	install -d $MODULE_DIR
	for module in ${WIFI_MODULES}; do
		install -m 644 ${S}/${module}/*.ko $MODULE_DIR/
	done
}

# Ignore buildpaths check for the main package and the debug package
INSANE_SKIP:${PN} += "buildpaths"
INSANE_SKIP:${PN}-dbg += "buildpaths"
WARN_QA:remove = "buildpaths"
ERROR_QA:remove = "buildpaths"

MODULES_MODULE_SYMVERS_LOCATION = "${WIFI_MODULE}"
