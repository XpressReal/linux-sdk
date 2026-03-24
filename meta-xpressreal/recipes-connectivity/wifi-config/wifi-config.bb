SUMMARY = "Enable wpa_supplicant@wlan0 systemd service"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit systemd

SRC_URI += " \
	file://wpa_supplicant-wlan0.conf.in \
	file://wlan0.network \
	file://70-persistent-net.rules \
	file://99-wifi-autostart.rules \
	file://wpa_supplicant@.service \
	"

RDEPENDS:${PN} += "wpa-supplicant"

# Variables to override (can be overridden via local.conf or other layers)
WIFI_SSID_1 ?= "RTKStarkDemo"
WIFI_PSK_1 ?= "0123456789"
WIFI_PRIORITY_1 ?= "10"

WIFI_SSID_2 ?= "DHC_CPX"
WIFI_PSK_2 ?= "03-5780211"
WIFI_PRIORITY_2 ?= "100"

do_install() {
    install -D -p -m0644 ${WORKDIR}/wpa_supplicant@.service ${D}${sysconfdir}/systemd/system/wpa_supplicant@wlan0.service
    install -D -p -m0644 ${WORKDIR}/wlan0.network ${D}${sysconfdir}/systemd/network/wlan0.network

    install -D -p -m0644 ${WORKDIR}/70-persistent-net.rules ${D}${sysconfdir}/udev/rules.d/70-persistent-net.rules
    install -D -p -m0644 ${WORKDIR}/99-wifi-autostart.rules ${D}${sysconfdir}/udev/rules.d/99-wifi-autostart.rules

    install -d ${D}${sysconfdir}/wpa_supplicant
    sed -e 's|@SSID_1@|${WIFI_SSID_1}|' \
        -e 's|@PSK_1@|${WIFI_PSK_1}|' \
	-e 's|@SSID_2@|${WIFI_SSID_2}|' \
        -e 's|@PSK_2@|${WIFI_PSK_2}|' \
        ${WORKDIR}/wpa_supplicant-wlan0.conf.in > ${D}${sysconfdir}/wpa_supplicant/wpa_supplicant-wlan0.conf

}

FILES:${PN} += "${sysconfdir}/systemd/system"
