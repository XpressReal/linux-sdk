DESCRIPTION = "Janus WebRTC Gateway"
HOMEPAGE = "https://janus.conf.meetecho.com/"
LICENSE = "GPL-3.0-or-later"
LIC_FILES_CHKSUM = "file://COPYING;md5=c3707f19243459c077cf33ceb57e8c37"

SRC_URI = " \
	git://github.com/meetecho/janus-gateway;protocol=https;branch=master \
	file://janus_etc.tgz;unpack=0 \
	file://adapter.js \
	"

SRC_URI[sha256sum] = "ac5d3ee3b1d239a5d9cb0b3dabaee32964bdf8dbf68c014310dbc65a4ab0b77a"

SRCREV = "74cb226b8f940be43c096779b0d3681544d3905d"
S = "${WORKDIR}/git"

DEPENDS = "libwebsockets glib-2.0 jansson libnice openssl libconfig libsrtp libmicrohttpd"

inherit autotools pkgconfig

EXTRA_OECONF = "--disable-docs --disable-websockets --disable-data-channels --enable-plugin-streaming"

do_install:append() {
    install -d ${D}${sysconfdir}/janus
    tar -xvf ${WORKDIR}/janus_etc.tgz -C ${D}${sysconfdir}
    chown -R root:root ${D}${sysconfdir}/janus

    install -d ${D}${datadir}/janus/javascript
    install -m 0755  ${WORKDIR}/adapter.js ${D}${datadir}/janus/javascript/adapter.js
}

INSANE_SKIP:${PN} += "dev-so"
