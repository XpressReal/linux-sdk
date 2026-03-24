SUMMARY = "PiKVM Daemon (KVMD)"
DESCRIPTION = "kvmd is the PiKVM system daemon controlling ustreamer, nginx, etc."
HOMEPAGE = "https://github.com/pikvm/kvmd"
LICENSE = "GPL-3.0-or-later"
LIC_FILES_CHKSUM = "file://LICENSE;md5=d32239bcb673463ab874e80d47fae504"

SRC_URI = "git://github.com/pikvm/kvmd.git;branch=master;protocol=https"
SRCREV = "42efb73c983bae91d7ad07a7eef548ff596a567f"
SRC_URI += "\
	file://0001-tools.py-fix-too-few-arguments-for-Generator.patch \
	file://0002-otg-remove-inquiry_string_cdrom-since-kernel-6.6.patch \
	file://0003-fix-missing-qsize-implemention-in-python-3.12.patch \
	file://0004-modify-index.html-to-switch-between-legacy-and-webrtc.patch \
	file://0005-modify-javascript-to-select-legacy-or-webrtc.patch \
	file://0007-nginx-config-add-an-option-to-enable-disable-https.patch \
	file://kvmd.service \
	file://kvmd-otg.service \
	file://kvmd-nginx.service \
	file://kvmd-pst.service \
	file://kvmd-janus.service \
	file://kvmd-janus-static.service \
	file://kvmd-ustreamer.service \
	file://kvmd-media.service  \
	file://kvmd-otgnet.service  \
	file://kvmd-webterm.service \
	file://kvmd-webrtc.service \
	file://kvmd-extend.service \
	file://platform \
	file://kvmd-tmpfiles.conf \
	file://kvmd-sudoer \
	file://90-gpio.rules \
	file://99-kvmd.rules \
	file://htpasswd \
	file://99-kvmd.conf \
	file://nginx.ctx-server.conf \
	file://streamer.py \
	file://httpd_kvm_extend.py \
	file://override.yaml \
	file://nginx.ctx-server.conf \
	file://gadget-conf.sh \
	file://gen-qr-codes.sh \
	file://background.png \
	file://otg.yaml \
	file://main.yaml \
	file://gen-ssl.sh \
	file://bg.png \
	file://desktop-background.png \
	file://logo0.svg \
	file://logo.svg \
	file://tdesign_user-circle-filled.svg \
	file://blank-stream-rtkBg-01.jpg \
	file://blank-stream-rtkBg-02.jpg \
	file://blank-stream-rtkBg-03.jpg \
	file://blank-stream-rtkBg-04.jpg \
	file://blank-stream-rtkBg-05.jpg \
	file://blank-stream-rtkBg-06.jpg \
	file://gpio-conf.sh \
	file://gpio.yaml \
	"

SRC_URI += "${@bb.utils.contains('PACKAGECONFIG', 'demo', ' \
	        file://0010-web-index.html-modify-for-Realtek-Demo-UI.patch \
	        file://0011-web-css-patch-for-Realtek-Demo-style.patch \
                file://0012-web-javascript-modify-for-Realtek-Demo-purpose.patch \
                file://0013-web-modify-for-Realtek-KVM-UI.patch \
                file://0014-Support-USB-drive-passthrough.patch \
                file://0015-kvmd-Add-WoL-feature.patch ', '', d)}"

S = "${WORKDIR}/git"

DEPENDS = "python3 openssl nginx ustreamer"
inherit setuptools3 systemd useradd

USERADD_PACKAGES = "${PN}"

PACKAGECONFIG ??= "demo"
PACKAGECONFIG[develop] = ",,,"
PACKAGECONFIG[demo] = ",,,"
PACKAGECONFIG[otg_disabled] = ",,,"

# Define the user group
GROUPADD_PARAM:${PN} = "--system kvmd; --system kvmd-nginx; --system kvmd-pst; --system gpio; --system video; --system kvmd-localhid"

# Define the user
USERADD_PARAM:${PN} = "-r -s /bin/bash -d / -M -g kvmd -G kvmd-pst,gpio,video,audio kvmd; \
                       -r -s /usr/sbin/nologin -d / -M -g kvmd-nginx -G kvmd kvmd-nginx; \
                       -r -s /usr/sbin/nologin -d / -M -g kvmd-pst -G kvmd kvmd-pst; \
                       -r -s /usr/sbin/nologin -d / -M -g kvmd-pst -G kvmd,input kvmd-localhid; \
		      "

do_install:append() {
    install -d ${D}${datadir}/kvmd/extras
    cp -r ${S}/extras/* ${D}${datadir}/kvmd/extras/
    install -m 0644 ${WORKDIR}/nginx.ctx-server.conf ${D}${datadir}/kvmd/extras/janus/nginx.ctx-server.conf

    #Copy the original web files
    install -d ${D}${datadir}/kvmd/web
    cp -r ${S}/web/* ${D}${datadir}/kvmd/web

    #Copy files for Realtek Demo UI
    install -D -p -m0644 ${WORKDIR}/desktop-background.png ${D}${datadir}/kvmd/web/kvm/desktop-background.png
    install -D -p -m0644 ${WORKDIR}/bg.png ${D}${datadir}/kvmd/web/share/png/bg.png
    install -D -p -m0644 ${WORKDIR}/blank-stream-rtkBg-01.jpg ${D}${datadir}/kvmd/web/share/png/blank-stream-rtkBg-01.jpg
    install -D -p -m0644 ${WORKDIR}/blank-stream-rtkBg-02.jpg ${D}${datadir}/kvmd/web/share/png/blank-stream-rtkBg-02.jpg
    install -D -p -m0644 ${WORKDIR}/blank-stream-rtkBg-03.jpg ${D}${datadir}/kvmd/web/share/png/blank-stream-rtkBg-03.jpg
    install -D -p -m0644 ${WORKDIR}/blank-stream-rtkBg-04.jpg ${D}${datadir}/kvmd/web/share/png/blank-stream-rtkBg-04.jpg
    install -D -p -m0644 ${WORKDIR}/blank-stream-rtkBg-05.jpg ${D}${datadir}/kvmd/web/share/png/blank-stream-rtkBg-05.jpg
    install -D -p -m0644 ${WORKDIR}/blank-stream-rtkBg-06.jpg ${D}${datadir}/kvmd/web/share/png/blank-stream-rtkBg-06.jpg
    install -D -p -m0644 ${WORKDIR}/logo0.svg ${D}${datadir}/kvmd/web/share/svg/logo0.svg
    install -D -p -m0644 ${WORKDIR}/tdesign_user-circle-filled.svg ${D}${datadir}/kvmd/web/share/svg/tdesign_user-circle-filled.svg
    if [ "${@bb.utils.contains("PACKAGECONFIG", "demo", "1", "0", d)}" = "1" ]; then
        install -m 0644 ${WORKDIR}/logo.svg ${D}${datadir}/kvmd/web/share/svg/logo.svg
    fi

    #Remove pikvm icons
    rm -f ${D}${datadir}/kvmd/web/favicon.ico
    rm -f ${D}${datadir}/kvmd/web/share/favicon-16x16.png
    rm -f ${D}${datadir}/kvmd/web/share/favicon-32x32.png
    rm -f ${D}${datadir}/kvmd/web/share/svg/favicon.svg

    chown -R kvmd:kvmd ${D}${datadir}/kvmd/web

    # Install configuration
    install -d ${D}${sysconfdir}/kvmd/nginx
    install -d ${D}${sysconfdir}/kvmd/override.d
    cp -r ${S}/configs/nginx/* ${D}${sysconfdir}/kvmd/nginx
    # copy the configuration files of kvmd
    cp ${S}/configs/kvmd/*.* ${D}${sysconfdir}/kvmd/
    cp ${S}/configs/kvmd/*passwd ${D}${sysconfdir}/kvmd/
    install -m 0644 ${WORKDIR}/main.yaml ${D}${sysconfdir}/kvmd/main.yaml
    install -m 0644 ${WORKDIR}/override.yaml ${D}${sysconfdir}/kvmd/override.yaml
    if [ "${@bb.utils.contains('PACKAGECONFIG', 'otg_disabled', "1", "0", d)}" = "1" ]; then
	install -m 0644 ${WORKDIR}/otg.yaml ${D}${sysconfdir}/kvmd/override.d/otg.yaml
    fi
    install -m 0644 ${WORKDIR}/gpio.yaml ${D}${sysconfdir}/kvmd/override.d/gpio.yaml

    install -m 0644 ${WORKDIR}/background.png ${D}${sysconfdir}/kvmd/background.png

    #Install script to handle board specific settings
    install -D -p -m0755 ${WORKDIR}/gadget-conf.sh ${D}${exec_prefix}/local/bin/gadget-conf.sh
    install -D -p -m0755 ${WORKDIR}/gen-qr-codes.sh ${D}${exec_prefix}/local/bin/gen-qr-codes.sh
    install -D -p -m0755 ${WORKDIR}/gen-ssl.sh ${D}${exec_prefix}/local/bin/gen-ssl.sh
    install -D -p -m0755 ${WORKDIR}/gpio-conf.sh ${D}${exec_prefix}/local/bin/gpio-conf.sh

    # Add service file
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/kvmd.service ${D}${systemd_system_unitdir}/kvmd.service
    install -m 0644 ${WORKDIR}/kvmd-otg.service ${D}${systemd_system_unitdir}/kvmd-otg.service
    install -m 0644 ${WORKDIR}/kvmd-nginx.service ${D}${systemd_system_unitdir}/kvmd-nginx.service
    install -m 0644 ${WORKDIR}/kvmd-pst.service ${D}${systemd_system_unitdir}/kvmd-pst.service
    install -m 0644 ${WORKDIR}/kvmd-janus.service ${D}${systemd_system_unitdir}/kvmd-janus.service
    install -m 0644 ${WORKDIR}/kvmd-janus-static.service ${D}${systemd_system_unitdir}/kvmd-janus-static.service
    install -m 0644 ${WORKDIR}/kvmd-media.service ${D}${systemd_system_unitdir}/kvmd-media.service
    install -m 0644 ${WORKDIR}/kvmd-ustreamer.service ${D}${systemd_system_unitdir}/kvmd-ustreamer.service
    install -m 0644 ${WORKDIR}/kvmd-webrtc.service ${D}${systemd_system_unitdir}/kvmd-webrtc.service
    #install -m 0644 ${WORKDIR}/kvmd-otgnet.service ${D}${systemd_system_unitdir}/kvmd-otgnet.service
    #install -m 0644 ${WORKDIR}/kvmd-webterm.service ${D}${systemd_system_unitdir}/kvmd-webterm.service
    install -m 0644 ${WORKDIR}/kvmd-extend.service ${D}${systemd_system_unitdir}/kvmd-extend.service

    #install system related files
    install -d ${D}${exec_prefix}/bin
    install -m 0755 ${S}/testenv/fakes/vcgencmd ${D}${exec_prefix}/bin/

    install -m 0644 ${WORKDIR}/platform ${D}${datadir}/kvmd/

    install -d ${D}${datadir}/kvmd/keymaps
    install -m 0644 ${S}/contrib/keymaps/* ${D}${datadir}/kvmd/keymaps/

    #overwrite the default configuration files
    install -D -p -m0600 ${WORKDIR}/htpasswd ${D}${sysconfdir}/kvmd

    chown -R kvmd:kvmd ${D}${sysconfdir}/kvmd

    #install -d ${D}run/kvmd
    install -D -p -m0644 ${WORKDIR}/kvmd-tmpfiles.conf ${D}${sysconfdir}/tmpfiles.d/kvmd.conf

    install -D -p -m0644 ${WORKDIR}/kvmd-sudoer ${D}${sysconfdir}/sudoers.d/kvmd

    install -D -p -m0644 ${WORKDIR}/90-gpio.rules ${D}${sysconfdir}/udev/rules.d/90-gpio.rules
    install -D -p -m0644 ${WORKDIR}/99-kvmd.rules ${D}${sysconfdir}/udev/rules.d/99-kvmd.rules

    install -D -p -m0644 ${WORKDIR}/99-kvmd.conf ${D}${sysconfdir}/sysctl.d/99-kvmd.conf

    install -D -p -m0644 ${WORKDIR}/streamer.py ${D}${PYTHON_SITEPACKAGES_DIR}/kvmd/apps/kvmd/api
    install -D -p -m0644 ${WORKDIR}/httpd_kvm_extend.py ${D}${PYTHON_SITEPACKAGES_DIR}/kvmd/apps/kvmd/api

}

SYSTEMD_SERVICE:${PN} = "kvmd.service kvmd-otg.service kvmd-nginx.service kvmd-janus-static.service kvmd-extend.service"
#SYSTEMD_AUTO_ENABLE = "enable"

RDEPENDS:${PN} += "\
    ustreamer \
    nginx \
    python3-aiofiles \
    python3-aiohttp \
    python3-asyncio \
    python3-async-lru \
    python3-bcrypt \
    python3-core \
    python3-dbus \
    python3-dbus-next \
    python3-evdev \
    python3-gpiod \
    python3-json \
    python3-jinja2 \
    python3-logging \
    python3-mako \
    python3-netifaces \
    python3-passlib \
    python3-pillow \
    python3-psutil \
    python3-pygments \
    python3-pyotp \
    python3-pyusb \
    python3-pyudev \
    python3-pyserial \
    python3-pyserial-asyncio \
    python3-pyyaml \
    python3-requests \
    python3-setproctitle \
    python3-systemd \
    python3-xlib \
    python3-zstandard \
    "

FILES:${PN} += "/usr/lib/systemd/system /usr/local/bin"
