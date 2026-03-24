# Copyright (C) 2024 Realtek Semiconductor Corp.

DESCRIPTION = "Kernel Loadable Module for Mali GPU kernel driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

DEPENDS = "virtual/kernel"
inherit module kernel-module-split

SRC_URI = " \
	file://${BPN}.tar.xz \
	"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

SRC_URI:append = "\
	file://0001-build-mali-r54-csf-and-jm-driver.patch \
	"

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

PACKAGES =+ "kernel-module-mali_kbase kernel-module-dmabuf-exporter"
RPROVIDES:${PN} += "kernel-module-mali_kbase kernel-module-dmabuf-exporter"

MODULES_DMA_BUF_LOCATION = "driver/product/kernel/drivers/base/arm/dma_buf_test_exporter"
MODULES_MALI_BASE_LOCATION = "driver/product/kernel/drivers/gpu/arm/midgard"


EXTRA_OEMAKE += "KDIR=${STAGING_KERNEL_DIR}"

module_do_install:kent(){
	MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/gpu/arm
	install -d $MODULE_DIR
	install -m 644 ${S}/CSF/${MODULES_MALI_BASE_LOCATION}/mali_kbase.ko $MODULE_DIR/mali_kbase.ko
	install -m 644 ${S}/CSF/${MODULES_DMA_BUF_LOCATION}/dma-buf-test-exporter.ko $MODULE_DIR/dmabuf-exporter.ko
}

module_do_install:stark(){
	MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/gpu/arm
	install -d $MODULE_DIR
	install -m 644 ${S}/JM/${MODULES_MALI_BASE_LOCATION}/mali_kbase.ko $MODULE_DIR/mali_kbase.ko
	install -m 644 ${S}/JM/${MODULES_DMA_BUF_LOCATION}/dma-buf-test-exporter.ko $MODULE_DIR/dmabuf-exporter.ko
}

module_do_install:hank(){
	MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/gpu/arm
	install -d $MODULE_DIR
	install -m 644 ${S}/JM/${MODULES_MALI_BASE_LOCATION}/mali_kbase.ko $MODULE_DIR/mali_kbase.ko
	install -m 644 ${S}/JM/${MODULES_DMA_BUF_LOCATION}/dma-buf-test-exporter.ko $MODULE_DIR/dmabuf-exporter.ko
}

module_do_install:rtd16xx(){
	MODULE_DIR=${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/gpu/arm
	install -d $MODULE_DIR
	install -m 644 ${S}/JM/${MODULES_MALI_BASE_LOCATION}/mali_kbase.ko $MODULE_DIR/mali_kbase_jm.ko
	install -m 644 ${S}/JM/${MODULES_DMA_BUF_LOCATION}/dma-buf-test-exporter.ko $MODULE_DIR/dmabuf-exporter_jm.ko
	install -m 644 ${S}/CSF/${MODULES_MALI_BASE_LOCATION}/mali_kbase.ko $MODULE_DIR/mali_kbase_csf.ko
	install -m 644 ${S}/CSF/${MODULES_DMA_BUF_LOCATION}/dma-buf-test-exporter.ko $MODULE_DIR/dmabuf-exporter_csf.ko
}

KERNEL_MODULE_AUTOLOAD += "mali_kbase dmabuf-exporter"
