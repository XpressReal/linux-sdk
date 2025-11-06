#inherit qmake5_paths
# to add qmake in /usr/bin for compile
inherit qmake5

SUMMARY = "RTK QT application"
DESCRIPTION = "This is Realtek Qt application."
LICENSE = "CLOSED"

DEPENDS += "qtbase qtwayland gstreamer1.0 gstreamer1.0-plugins-base rtk-qt-base"
RDEPENDS:${PN} += "rtk-qt-base"

inherit pkgconfig

SRC_URI = "file://${BPN}.tar.xz"
SRCREV = "${AUTOREV}"
include ${BPN}.inc

S = "${WORKDIR}/${BPN}-${PV}"
SDK_DIR = "${THISDIR}/../../rtk-dl"
FILESEXTRAPATHS:append := ":${SDK_DIR}"
#B = "${WORKDIR}/build"

EXTRA_QMAKEVARS_CONFIGURE += "${PACKAGECONFIG_CONFARGS}"

do_compile() {
    export CFLAGS="${CFLAGS} $(pkg-config --cflags gstreamer-1.0 gstreamer-video-1.0)"
    export LDFLAGS="${LDFLAGS} $(pkg-config --libs gstreamer-1.0 gstreamer-video-1.0)"

    # use qmake to generate Makefile
    ${OE_QMAKE_QMAKE} -o Makefile ${S}/rtk-qt-plus.pro

    # build the project
    oe_runmake
}

# install files
do_install() {

#    install -d ${D}${libdir}
#    install -m 0644 -D ${WORKDIR}/build/lib-rtk-qt-base/librtkqtbase.so ${D}${libdir}/librtkqtbase.so
#    install -d ${D}${includedir}
#    install -m 0644 -D ${S}/lib-rtk-qt-base/rtkgst.h ${D}${includedir}

    install -d ${D}${bindir}/rtk-multi-playback
    install -m 0755 -D ${WORKDIR}/build/rtk-multi-playback/rtk-multi-playback ${D}${bindir}/rtk-multi-playback/
    install -m 0644 -D ${S}/rtk-multi-playback/config.ini ${D}${bindir}/rtk-multi-playback/
}

# install file to image
INSANE_SKIP:${PN} += "debug-files"
INSANE_SKIP:${PN} += "installed-vs-shipped"
#FILES:${PN} += "${libdir}/*.so"
FILES:${PN} += "${bindir}"

PROVIDES = "rtk-qt-plus"
RPROVIDES:${PN} = "rtk-qt-plus"
#INSANE_SKIP:${PN} += "file-rdeps"
PACKAGES = "${PN}"
