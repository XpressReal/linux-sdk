DESCRIPTION = "XZ-format compression library (static)"
LICENSE = "PD & GPL-2.0-or-later & LGPL-2.1-or-later"
LIC_FILES_CHKSUM = "file://COPYING;md5=d4378ea9d5d1fc9ab0ae10d7948827d9"

PV = "5.4.6"
SRC_URI = "https://github.com/tukaani-project/xz/releases/download/v${PV}/xz-${PV}.tar.gz"
SRC_URI[sha256sum] = "aeba3e03bf8140ddedf62a0a367158340520f6b384f75ca6045ccc6c0d43fd5c"

S = "${WORKDIR}/xz-${PV}"

inherit autotools gettext

EXTRA_OECONF = " \
    --enable-static \
    --disable-shared \
    --disable-xz \
    --disable-xzdec \
    --disable-lzmadec \
    --disable-scripts \
"

do_install() {
    install -d ${D}${libdir}
    install -m 0644 ${B}/src/liblzma/.libs/liblzma.a ${D}${libdir}

    install -d ${D}${includedir}
    install -d ${D}${includedir}/lzma
    install -m 0644 ${S}/src/liblzma/api/*.h ${D}${includedir}
    install -m 0644 ${S}/src/liblzma/api/lzma/*.h ${D}${includedir}/lzma
}

