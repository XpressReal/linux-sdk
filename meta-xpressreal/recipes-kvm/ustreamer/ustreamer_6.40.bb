DESCRIPTION = "Lightweight and fast MJPEG video streamer based on V4L2"
HOMEPAGE = "https://github.com/pikvm/ustreamer"
LICENSE = "GPL-3.0-or-later"
LIC_FILES_CHKSUM = "file://LICENSE;md5=d32239bcb673463ab874e80d47fae504"

SRC_URI = "git://github.com/pikvm/ustreamer.git;branch=master;protocol=https"
SRCREV = "472673ea9067c377f3e6dfde75cd2c58282d0b59"

S = "${WORKDIR}/git"

DEPENDS += "libevent jpeg libbsd systemd libdrm libgpiod \
	python3-native python3-build-native"

inherit pkgconfig setuptools3

EXTRA_OEMAKE = "WITH_PYTHON=1 WITH_JANUS=1 WITH_SYSTEMD=1 WITH_V4P=1"

do_compile() {
    oe_runmake apps

    # Build python wheel inside python/ folder
    bbnote "${S}/python"
    (cd ${S}/python && python3 setup.py bdist_wheel)
}

do_install() {
    # Install binary as usual
    install -d ${D}${bindir}
    install -m 0755 ustreamer ${D}${bindir}/ustreamer

    install -d ${D}${PYTHON_SITEPACKAGES_DIR}
    cp -r ${S}/python/build/lib.linux-x86_64-cpython-312/* ${D}${PYTHON_SITEPACKAGES_DIR}/
}
