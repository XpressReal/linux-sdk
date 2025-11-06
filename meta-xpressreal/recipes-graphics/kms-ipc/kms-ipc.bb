# Copyright (C) 2024 Realtek Semiconductor Corp.

SUMMARY = "KMS IPC"
LICENSE = "CLOSED"

inherit pkgconfig
DEPENDS += "libdrm"

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

EXTRA_OEMAKE = " \
	'INCLUDES_DIR=${STAGING_INCDIR}'"

TARGET_CC_ARCH += "${LDFLAGS}"
do_install () {
	install -D -p -m0755 ${S}/kms_ipc ${D}${bindir}/kms_ipc
}

FILES:${PN} += "${bindir}/*"
