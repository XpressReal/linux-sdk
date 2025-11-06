SUMMARY = "Realtek Avengers SoC Firmware Related Service"
DESCRIPTION = "Use systemd service to implement firmware related control"
LICENSE = "CLOSED"

SRC_URI = " file://firmware.service \
            file://display.service \
          "

S = "${WORKDIR}"

RDEPENDS:${PN} = "systemd fwdbg rtk-media-framework"

do_install () {
    install -d ${D}${systemd_unitdir}/system/
    install -d ${D}${sysconfdir}/systemd/system/graphical.target.wants/

    install -m 0644 ${WORKDIR}/firmware.service ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/display.service ${D}${systemd_unitdir}/system

    # Enable the firmware.service
    ln -sf ${systemd_unitdir}/system/firmware.service \
            ${D}${sysconfdir}/systemd/system/graphical.target.wants/firmware.service

    ln -sf ${systemd_unitdir}/system/display.service \
            ${D}${sysconfdir}/systemd/system/graphical.target.wants/display.service
}


FILES:${PN} = "${systemd_unitdir}/system/*.service ${sysconfdir}"

# As this package is tied to systemd, only build it when we're also building systemd.
python () {
    if not bb.utils.contains ('DISTRO_FEATURES', 'systemd', True, False, d):
        raise bb.parse.SkipPackage("'systemd' not in DISTRO_FEATURES")
}
