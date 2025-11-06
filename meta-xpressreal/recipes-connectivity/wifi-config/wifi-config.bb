SUMMARY = "Enable wpa_supplicant@wlan0 systemd service"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit systemd

SRC_URI += " \
	file://wpa_supplicant-wlan0.conf \
	file://wlan0.network \
	file://70-persistent-net.rules \
	file://99-wifi-autostart.rules \
	file://wpa_supplicant@.service \
	"

RDEPENDS:${PN} += "wpa-supplicant"

do_install() {
    install -D -p -m0644 ${WORKDIR}/wpa_supplicant@.service ${D}${sysconfdir}/systemd/system/wpa_supplicant@wlan0.service

    install -d ${D}${sysconfdir}/wpa_supplicant
    install -D -p -m0644 ${WORKDIR}/wpa_supplicant-wlan0.conf ${D}${sysconfdir}/wpa_supplicant/wpa_supplicant-wlan0.conf
    
    install -D -p -m0644 ${WORKDIR}/wlan0.network ${D}${sysconfdir}/systemd/network/wlan0.network

    install -D -p -m0644 ${WORKDIR}/70-persistent-net.rules ${D}${sysconfdir}/udev/rules.d/70-persistent-net.rules
    install -D -p -m0644 ${WORKDIR}/99-wifi-autostart.rules ${D}${sysconfdir}/udev/rules.d/99-wifi-autostart.rules
}

FILES:${PN} += "${sysconfdir}/systemd/system"
