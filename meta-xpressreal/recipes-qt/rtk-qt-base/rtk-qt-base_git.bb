#inherit qmake5_paths
# to add qmake in /usr/bin for compile
inherit qmake5

SUMMARY = "RTK QT application"
DESCRIPTION = "This is Realtek Qt application."
LICENSE = "CLOSED"

DEPENDS += "qtbase qtwayland gstreamer1.0 gstreamer1.0-plugins-base"
RDEPENDS:${PN} += "bash"

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
    ${OE_QMAKE_QMAKE} -o Makefile ${S}/rtk-qt-base.pro

    # build the project
    oe_runmake
}

# install files
do_install() {

    install -d ${D}${libdir}
    install -m 0644 -D ${WORKDIR}/build/lib-rtk-qt-base/librtkqtbase.so ${D}${libdir}/librtkqtbase.so
    install -d ${D}${includedir}
    install -m 0644 -D ${S}/lib-rtk-qt-base/rtkgst.h ${D}${includedir}

    install -d ${D}${bindir}/rtk-playback
    install -m 0755 -D ${WORKDIR}/build/rtk-playback/rtk-playback ${D}${bindir}/rtk-playback/
    install -m 0644 -D ${S}/rtk-playback/config.ini ${D}${bindir}/rtk-playback/

    install -d ${D}${bindir}/rtk-test
    install -m 0755 -D ${WORKDIR}/build/rtk-test/rtk-test ${D}${bindir}/rtk-test/
    install -m 0755 -D ${S}/rtk-test/rtk-test.sh ${D}${bindir}/rtk-test/
    install -m 0644 -D ${S}/rtk-test/rtk-test.ini ${D}${bindir}/rtk-test/
}

# install file to image
INSANE_SKIP:${PN} += "debug-files"
INSANE_SKIP:${PN} += "installed-vs-shipped"
FILES:${PN} += "${libdir}/*.so"
FILES:${PN} += "${bindir}"

PROVIDES = "rtk-qt-base"
RPROVIDES:${PN} = "rtk-qt-base"
#INSANE_SKIP:${PN} += "file-rdeps"
PACKAGES = "${PN}"
