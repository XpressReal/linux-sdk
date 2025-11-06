DESCRIPTIOM = "Standard Linux OS Image"
LICENSE = "CLOSED"

inherit image

IMAGE_FEATURES = ""
IMAGE_FEATURES += "${@bb.utils.contains('MACHINE_FEATURES', 'overlayfs-root', ' overlayfs-root', '', d)}"
PACKAGE_INSTALL = "kernel-modules linux-firmware-rtl8822 rtk-mod-wifi"
PACKAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'rtk-mod-v4l2dec rtk-mod-v4l2cap', '', d)}"
PACKAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'mesa-avengers', '', d)}"
PACKAGE_INSTALL:append = " ${@([t for t in d.getVar('IMAGE_INSTALL', True).split() if t.startswith('linux-firmware')] or [''])[0]}"

#use the layout with separate home partition
WKS_FILE := "${@bb.utils.contains('MACHINE_FEATURES', 'split-home', 'avengers-home.wks', '${WKS_FILE}', d)}"

#change password for root user
PASSWORD_ROOT ?= "test0000"
#change password for nas user
PASSWORD_NAS ?= "test0000"


fakeroot do_prebuilt() {
	tar --exclude=usr/lib/firmware --exclude=usr/lib/modules -xf ${IMAGE_ROOTFS}/rootfs.tar.xz -C ${IMAGE_ROOTFS}
	rm -f ${IMAGE_ROOTFS}/rootfs.tar.*
	tar -xf ${IMAGE_ROOTFS}/configs.tar.xz -C ${IMAGE_ROOTFS}/etc
	rm -f ${IMAGE_ROOTFS}/configs.tar.xz

	echo "${MACHINE}" > ${IMAGE_ROOTFS}/etc/hostname

	for pair in \
		"root:${PASSWORD_ROOT}" \
		"nas:${PASSWORD_NAS}"
	do
		u="${pair%:*}"
		p="${pair##*:}"

		hash_p=$(openssl passwd -6 "${p}")
		escaped_hash=$(echo "${hash_p}" | sed 's/\$/\\$/g')

		sed -i "/^${u}:/s|:[^:]*:|:${escaped_hash}:|" ${IMAGE_ROOTFS}/etc/shadow
	done
}

do_prebuilt[depends] += "virtual/fakeroot-native:do_populate_sysroot"

addtask prebuilt after do_rootfs before do_image
