SUMMARY = "CEC Key Receiver Application"
SECTION = "utils"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = "cmake-native"

SRC_URI = " \
    file://CMakeLists.txt \
    file://cec_receiver.c \
	file://cecdefs.h \
"

S = "${WORKDIR}"

inherit cmake pkgconfig

EXTRA_OECMAKE:append = " \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_RPATH=${libdir} \
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=TRUE \
"

RPATHFIX = "${@bb.utils.contains('DISTRO_FEATURES', 'ld-is-gold', '-Wl,--enable-new-dtags', '', d)}"
CFLAGS:append = " ${RPATHFIX}"
CXXFLAGS:append = " ${RPATHFIX}"
LDFLAGS:append = " ${RPATHFIX}"

FILES:${PN} = "${bindir}/cec_receiver"
