DESCRIPTION = "Closed source binary files to help boot Realtek Avenger SoC based devices."
LICENSE = "CLOSED"

inherit deploy nopackages

DEPENDS += "u-boot-mkimage-native dtc-native"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

IMAGE_SRC_FILE = "image.its"
IMAGE_SRC_FILE:stark = "image-rtd1619b.its"

SRC_URI:append = " file://${IMAGE_SRC_FILE}"

do_deploy() {

	install -d ${DEPLOYDIR}/${BOOTFILES_DIR}

	cp ${DEPLOY_DIR_IMAGE}/${BOOTFILES_DIR}/* ${WORKDIR}/

	mkimage -f ${WORKDIR}/${IMAGE_SRC_FILE} ${DEPLOYDIR}/yocto.itb
}

do_deploy[depends] = "virtual/kernel:do_deploy rescuefs:do_deploy"

addtask deploy after do_install
