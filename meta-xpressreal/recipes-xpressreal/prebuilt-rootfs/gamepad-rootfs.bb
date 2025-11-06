# Hanlding prebuilt rootfs, including Debian/Ubuntu
# Using GAMEPAD_NAME to control which rootfs will be used
#
# Use "bitbake gamepad-image" to generate a Debian or Ubuntu based image for gamepad

DESCRIPTIOM = "Handle Prebuilt Rootfs for Debian/Ubuntu OS"
LICENSE = "CLOSED"

GAMEPAD_NAME = "gamepad-debian-rootfs.tar.xz"
GAMEPAD_NAME:ubuntu = "gamepad-ubuntu-rootfs.tar.xz"

require linux-rootfs.inc

SRC_URI:append = "file://${GAMEPAD_NAME};unpack=0 \
	   file://configs/moonlight.service \
	   file://configs/alc5650.service \
	   file://configs/5650spk.sh \
	   file://configs/modules_gamepad \
	  "

do_install:append() {
    ln -sf ${WORKDIR}/${GAMEPAD_NAME} ${D}/${ROOTFS_NAME}
    install -d ${D}${bindir}
    install -m 644 ${WORKDIR}/configs/moonlight.service ${D}/etc/systemd/system
    install -m 644 ${WORKDIR}/configs/alc5650.service ${D}/etc/systemd/system
    install -m 755 ${WORKDIR}/configs/5650spk.sh ${D}${bindir}/5650spk.sh
    install -m 755 ${WORKDIR}/configs//modules_gamepad ${D}/etc/modules

    tar -cvJf ${D}/configs.tar.xz -C ${D}/etc .
}
