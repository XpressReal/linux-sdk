DESCRIPTION = "Realtek Linux Bluetooth"
SECTION = "bluetooth"
DEPENDS = ""
LICENSE = "CLOSED"

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

SRC_URI:append = "\
	   file://0001-rtk_hciattach_Makefile.patch \
	   file://rtk-hciattach.service \
	  "
#S = "${WORKDIR}/git/uart/rtk_hciattach"
SRC_DIR = "${WORKDIR}/${BPN}-${PV}"
S = "${SRC_DIR}/uart/rtk_hciattach"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"

EXTRA_OEMAKE = "'CC=${CC}' 'CFLAGS=${CFLAGS}' 'LDFLAGS=${LDFLAGS}'"

INSANE_SKIP:${PN} += "ldflags"

# when this question was originally asked the format was
INSANE_SKIP_${PN} += "ldflags"

inherit systemd

do_install() {
	install -d ${D}${bindir}
	install -d ${D}${systemd_unitdir}/system
	install -d ${D}/${nonarch_base_libdir}
	install -d ${D}/${nonarch_base_libdir}/firmware/rtlbt
	install -d ${D}/${nonarch_base_libdir}/firmware/rtl_bt
	install -m 755 ${S}/rtk_hciattach ${D}/${bindir}/rtk_hciattach
	install -m 0644 ${SRC_DIR}/dhc-rtkbt-firmware/lib/firmware/*_fw ${D}/${nonarch_base_libdir}/firmware/
	install -m 0644 ${SRC_DIR}/dhc-rtkbt-firmware/lib/firmware/*_config ${D}/${nonarch_base_libdir}/firmware/
	install -m 0644 ${SRC_DIR}/dhc-rtkbt-firmware/lib/firmware/rtlbt/* ${D}/${nonarch_base_libdir}/firmware/rtlbt/
	install -m 0644 ${SRC_DIR}/dhc-rtkbt-firmware/lib/firmware/rtl_bt/* ${D}/${nonarch_base_libdir}/firmware/rtl_bt
	install -m 0644 ${WORKDIR}/rtk-hciattach.service ${D}${systemd_unitdir}/system/rtk-hciattach.service
}

SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE_${PN} = "rtk-hciattach.service"

FILES:${PN} = "${bindir}/rtk_hciattach \
	       ${systemd_unitdir}/system/rtk-hciattach.service \
               ${nonarch_base_libdir}/firmware/* \
               ${nonarch_base_libdir}/firmware/rtlbt/* \
               ${nonarch_base_libdir}/firmware/rtl_bt/* \
              "
