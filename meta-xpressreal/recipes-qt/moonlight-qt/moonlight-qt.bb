SUMMARY = "Moonlight QT, a game streaming client"
DESCRIPTION = "Moonlight is an open source implementation of NVIDIA's GameStream protocol."
HOMEPAGE = "https://github.com/moonlight-stream/moonlight-qt"
LICENSE = "GPL-3.0-or-later"
LIC_FILES_CHKSUM = "file://LICENSE;md5=84dcc94da3adb52b53ae4fa38fe49e5d"

SRC_URI = "gitsm://github.com/moonlight-stream/moonlight-qt.git;protocol=https;branch=master"
SRCREV = "fad197fdce9895bd96f2c1a8f32f853e7d55fb8d"
SRC_URI += "\
        file://0001-Add-WiFi-Setting.patch \
        "

S = "${WORKDIR}/git"

DEPENDS += "qtbase qtdeclarative qtquickcontrols2 qtsvg qttools qtxmlpatterns openssl libsdl2 libsdl2-ttf libopus"
DEPENDS += "ffmpeg libdrm qtvirtualkeyboard"


EXTRA_QMAKEVARS_PRE += "CONFIG+=c++17"

inherit qmake5 pkgconfig

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/app/moonlight ${D}${bindir}/moonlight-qt
}
