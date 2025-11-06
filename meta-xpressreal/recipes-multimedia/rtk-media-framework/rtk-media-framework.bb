# Copyright (C) 2024 Realtek Semiconductor Corp.

DESCRIPTION = "rtk media framework"
LICENSE = "CLOSED"

inherit pkgconfig

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

DEPENDS += "gstreamer1.0 gstreamer1.0-plugins-base json-c libdrm"
TARGET_CC_ARCH += "${LDFLAGS}"

do_install() {
	mkdir -p ${D}/${bindir}
	mkdir -p ${D}/${libdir}
	mkdir -p ${D}/${includedir}
	install -D -p -m0755 ${S}/RTK_media_info/rtk_media_info_bin ${D}${bindir}/rtk_media_info_bin
	cp ${S}/RTK_media_info/*.so ${D}/${libdir}
	cp ${S}/RTK_media_info/rtk_media_info.h ${D}/${includedir}

	install -D -p -m0755 ${S}/RTK_media_cap/rtk_media_cap_bin ${D}${bindir}/rtk_media_cap_bin
	cp ${S}/RTK_media_cap/*.so ${D}/${libdir}
	cp ${S}/RTK_media_cap/rtk_media_cap.h ${D}/${includedir}

	install -D -p -m0755 ${S}/RTK_display_ctrl/kms_ipc ${D}${bindir}/kms_ipc
	cp ${S}/RTK_display_ctrl/*.so ${D}/${libdir}
	cp ${S}/RTK_display_ctrl/rtk_display_ctrl.h ${D}/${includedir}
}

do_install:append:stark() {
	mkdir -p ${D}/${sysconfdir}
	install -D -p -m0755 ${S}/soc_cap_conf/rtk_video_cap_stark.json ${D}${sysconfdir}/rtk_video_cap.json
	install -D -p -m0755 ${S}/soc_cap_conf/rtk_subtitle_cap_stark.json ${D}${sysconfdir}/rtk_subtitle_cap.json
}

FILES:${PN} += "${bindir}/* ${libdir}/*.so ${sysconfdir}/*"
INSANE_SKIP:${PN} += "installed-vs-shipped"
PACKAGE_DEBUG_SPLIT_STYLE = "debug-without-src"

PROVIDES = "rtk-media-framework"
RPROVIDES:${PN} = "rtk-media-framework"
INSANE_SKIP_${PN} += "file-rdeps"
PACKAGES = "${PN}"

