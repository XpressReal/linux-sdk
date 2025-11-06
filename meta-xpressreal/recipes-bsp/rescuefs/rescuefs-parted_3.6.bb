DESCRIPTION = "Generate parted from parted recipe for rescue rootfs"

LICENSE = "GPL-3.0-or-later"
LIC_FILES_CHKSUM = "file://COPYING;md5=2f31b266d3440dd7ee50f92cf67d8e6c"

SRC_URI = "${GNU_MIRROR}/parted/parted-${PV}.tar.xz \
	   file://002-fix-doc-mandir.patch \
	   "

SRC_URI[sha256sum] = "3b43dbe33cca0f9a18601ebab56b7852b128ec1a3df3a9b30ccde5e73359e612"

S = "${WORKDIR}/parted-${PV}"

DEPENDS = "ncurses util-linux virtual/libiconv rescuefs-e2fsprogs"

inherit autotools pkgconfig gettext texinfo

EXTRA_OEMAKE = "LDFLAGS=-all-static"
EXTRA_OECONF = "--disable-device-mapper --without-readline --disable-shared LDFLAGS=-static"
EXTRA_OECONF:remove = "--disable-static"

do_install() {
	install -d ${D}${base_sbindir}
	${STRIP} ${B}/parted/parted
}

inherit deploy nopackages

do_deploy() {
	install -d ${DEPLOYDIR}/staging
	install -m 0755 ${B}/parted/parted ${DEPLOYDIR}/staging/parted
}

addtask deploy after do_install
