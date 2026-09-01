SUMMARY = "Realtek Avengers SoC Firmware Log Collector"
DESCRIPTION = "Collect Realtek firmware logs from /dev/alog, /dev/vlog, and /dev/hlog under a configurable storage directory."
LICENSE = "CLOSED"

SRC_URI = " \
    file://rtk-fwlog.default \
    file://rtk-fwlog-path-setup \
    file://rtk-fwlog-path.service \
    file://rtk-fwlog-prepare \
    file://rtk-fwlog-prepare.service \
    file://rtk-fwlog@.service \
    file://rtk-fwlog.target \
    file://rtk-fwlog-rotate.service \
    file://rtk-fwlog-rotate.timer \
    file://rtk-fwlog-logrotate.conf \
"

S = "${WORKDIR}"

inherit features_check systemd

REQUIRED_DISTRO_FEATURES = "systemd"

SYSTEMD_SERVICE:${PN} = "rtk-fwlog.target"
SYSTEMD_AUTO_ENABLE = "disable"

RDEPENDS:${PN} += "logrotate"

do_install() {
    install -D -m 0644 ${WORKDIR}/rtk-fwlog.default \
        ${D}${sysconfdir}/default/rtk-fwlog
    install -D -m 0755 ${WORKDIR}/rtk-fwlog-path-setup \
        ${D}${libexecdir}/rtk-fwlog-path-setup
    install -D -m 0755 ${WORKDIR}/rtk-fwlog-prepare \
        ${D}${libexecdir}/rtk-fwlog-prepare

    install -D -m 0644 ${WORKDIR}/rtk-fwlog-path.service \
        ${D}${systemd_system_unitdir}/rtk-fwlog-path.service
    install -D -m 0644 ${WORKDIR}/rtk-fwlog-prepare.service \
        ${D}${systemd_system_unitdir}/rtk-fwlog-prepare.service
    install -D -m 0644 ${WORKDIR}/rtk-fwlog@.service \
        ${D}${systemd_system_unitdir}/rtk-fwlog@.service
    install -D -m 0644 ${WORKDIR}/rtk-fwlog.target \
        ${D}${systemd_system_unitdir}/rtk-fwlog.target
    install -D -m 0644 ${WORKDIR}/rtk-fwlog-rotate.service \
        ${D}${systemd_system_unitdir}/rtk-fwlog-rotate.service
    install -D -m 0644 ${WORKDIR}/rtk-fwlog-rotate.timer \
        ${D}${systemd_system_unitdir}/rtk-fwlog-rotate.timer

    sed -i \
        -e 's|@LIBEXECDIR@|${libexecdir}|g' \
        -e 's|@SYSCONFDIR@|${sysconfdir}|g' \
        ${D}${systemd_system_unitdir}/rtk-fwlog-path.service
    sed -i \
        -e 's|@LIBEXECDIR@|${libexecdir}|g' \
        ${D}${systemd_system_unitdir}/rtk-fwlog-prepare.service
    sed -i \
        -e 's|@SBINDIR@|${sbindir}|g' \
        -e 's|@LOCALSTATEDIR@|${localstatedir}|g' \
        -e 's|@SYSCONFDIR@|${sysconfdir}|g' \
        ${D}${systemd_system_unitdir}/rtk-fwlog-rotate.service

    install -D -m 0644 ${WORKDIR}/rtk-fwlog-logrotate.conf \
        ${D}${sysconfdir}/rtk-fwlog-logrotate.conf
}

FILES:${PN} += " \
    ${sysconfdir}/default/rtk-fwlog \
    ${libexecdir}/rtk-fwlog-path-setup \
    ${libexecdir}/rtk-fwlog-prepare \
    ${systemd_system_unitdir}/rtk-fwlog-path.service \
    ${systemd_system_unitdir}/rtk-fwlog-prepare.service \
    ${systemd_system_unitdir}/rtk-fwlog@.service \
    ${systemd_system_unitdir}/rtk-fwlog.target \
    ${systemd_system_unitdir}/rtk-fwlog-rotate.service \
    ${systemd_system_unitdir}/rtk-fwlog-rotate.timer \
    ${sysconfdir}/rtk-fwlog-logrotate.conf \
"

CONFFILES:${PN} += "${sysconfdir}/default/rtk-fwlog"
