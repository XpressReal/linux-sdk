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
	file://kvmd-etc.tgz;unpack=0 \
	file://rtkweb.tgz;unpack=0 \
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
	file://platform \
	file://kvmd-tmpfiles.conf \
	file://kvmd-sudoer \
	file://90-gpio.rules \
	file://99-kvmd.rules \
	file://web.css \
	file://htpasswd \
	file://99-kvmd.conf \
	file://nginx.conf.mako \
	file://nginx.ctx-server.conf \
	file://streamer.py \
	file://override.yaml \
	file://nginx.ctx-server.conf \
	"

S = "${WORKDIR}/git"

DEPENDS = "python3 openssl nginx ustreamer"
inherit setuptools3 systemd useradd

USERADD_PACKAGES = "${PN}"

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

    install -d ${D}${datadir}/kvmd/web
    if [ "${@bb.utils.contains("MACHINE_FEATURES", "demo", "1", "0", d)}" = "1" ]; then
        tar -xvf ${WORKDIR}/rtkweb.tgz -C ${D}${datadir}/kvmd
    else
        cp -r ${S}/web/* ${D}${datadir}/kvmd/web
    fi
    chown -R kvmd:kvmd ${D}${datadir}/kvmd/web

    # Install configuration
    install -d ${D}${sysconfdir}/kvmd
    tar -xvf ${WORKDIR}/kvmd-etc.tgz -C ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/override.yaml ${D}${sysconfdir}/kvmd/override.yaml
    install -m 0644 ${WORKDIR}/nginx.conf.mako ${D}${sysconfdir}/kvmd/nginx/nginx.conf.mako

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

    #install system related files
    install -d ${D}${exec_prefix}/bin
    install -m 0755 ${S}/testenv/fakes/vcgencmd ${D}${exec_prefix}/bin/

    install -m 0644 ${WORKDIR}/platform ${D}${datadir}/kvmd/

    install -d ${D}${datadir}/kvmd/keymaps
    install -m 0644 ${S}/contrib/keymaps/* ${D}${datadir}/kvmd/keymaps/

    install -D -p -m0644 ${WORKDIR}/web.css ${D}${sysconfdir}/kvmd
    touch ${D}${sysconfdir}/kvmd/totp.secret
    install -D -p -m0600 ${WORKDIR}/htpasswd ${D}${sysconfdir}/kvmd

    chown -R kvmd:kvmd ${D}${sysconfdir}/kvmd

    #install -d ${D}run/kvmd
    install -D -p -m0644 ${WORKDIR}/kvmd-tmpfiles.conf ${D}${sysconfdir}/tmpfiles.d/kvmd.conf

    install -D -p -m0644 ${WORKDIR}/kvmd-sudoer ${D}${sysconfdir}/sudoers.d/kvmd

    install -D -p -m0644 ${WORKDIR}/90-gpio.rules ${D}${sysconfdir}/udev/rules.d/90-gpio.rules
    install -D -p -m0644 ${WORKDIR}/99-kvmd.rules ${D}${sysconfdir}/udev/rules.d/99-kvmd.rules

    install -D -p -m0644 ${WORKDIR}/99-kvmd.conf ${D}${sysconfdir}/sysctl.d/99-kvmd.conf

    install -D -p -m0644 ${WORKDIR}/streamer.py ${D}${PYTHON_SITEPACKAGES_DIR}/kvmd/apps/kvmd/api

}

SYSTEMD_SERVICE:${PN} = "kvmd.service kvmd-otg.service kvmd-nginx.service kvmd-janus-static.service"
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

FILES:${PN} += "/usr/lib/systemd/system"
