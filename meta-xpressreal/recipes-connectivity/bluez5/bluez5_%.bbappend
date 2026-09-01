FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " file://bluetooth.service.rtk-hciattach.conf"

SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    if ${@bb.utils.contains('MACHINE_FEATURES', 'vendor-bt', 'true', 'false', d)}; then
        install -d ${D}${systemd_unitdir}/system/bluetooth.service.d
        install -m 0644 ${WORKDIR}/bluetooth.service.rtk-hciattach.conf \
            ${D}${systemd_unitdir}/system/bluetooth.service.d/rtk-hciattach.conf
    fi
}

FILES:${PN} += "${systemd_unitdir}/system/bluetooth.service.d/rtk-hciattach.conf"
