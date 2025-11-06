DESCRIPTION = "Multi-format archive library (static)"
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://COPYING;md5=d499814247adaee08d88080841cb5665"

PV = "3.7.4"
SRC_URI = "http://libarchive.org/downloads/libarchive-${PV}.tar.gz"
SRC_URI[sha256sum] = "7875d49596286055b52439ed42f044bd8ad426aa4cc5aabd96bfe7abb971d5e8"

S = "${WORKDIR}/libarchive-${PV}"

DEPENDS = "rescuefs-liblzma-static"

inherit autotools pkgconfig

EXTRA_OECONF = " \
    --enable-static \
    --disable-shared \
    --without-nettle \
    --without-openssl \
    --without-xml2 \
    --without-expat \
"

do_install() {
    install -d ${D}${libdir}
    install -m 0644 ${B}/.libs/libarchive.a ${D}${libdir}

    install -d ${D}${includedir}
    install -m 0644 ${S}/libarchive/archive*.h ${D}${includedir}
}

FILES:${PN}-staticdev = " \
    ${libdir}/libarchive.a \
    ${includedir}/archive*.h \
"

