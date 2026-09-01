SUMMARY = "WenQuanYi Micro Hei - A compact CJK font"
HOMEPAGE = "http://wenq.org/"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE_Apache2.txt;md5=400d2f704f0a4f27b035fb613c8ae0ec"

# Using a reliable source mirror for the tarball
SRC_URI = "https://sourceforge.net/projects/wqy/files/wqy-microhei/0.2.0-beta/wqy-microhei-0.2.0-beta.tar.gz"
SRC_URI[sha256sum] = "2802ac8023aa36a66ea6e7445854e3a078d377ffff42169341bd237871f7213e"

S = "${WORKDIR}/wqy-microhei"

inherit fontcache

do_install() {
    install -d ${D}${datadir}/fonts/truetype/
    install -m 0644 ${S}/wqy-microhei.ttc ${D}${datadir}/fonts/truetype/
}

# The fontcache class handles fc-cache automatically
PACKAGES = "${PN}"
FILES:${PN} = "${datadir}/fonts/truetype/wqy-microhei.ttc"
