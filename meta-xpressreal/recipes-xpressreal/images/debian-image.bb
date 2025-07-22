DESCRIPTIOM = "Debian Image"
LICENSE = "CLOSED"

inherit image

IMAGE_FEATURES = ""
PACKAGE_INSTALL = "prebuilt-rootfs kernel-modules linux-firmware-rtd1619b linux-firmware-rtl8822 linux-firmware-aic8800 rtk-mod-npu"
PACKAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'mali', 'rtk-mod-v4l2dec fwdbg', '', d)}"

#use the layout with separate home partition
WKS_FILE = "${@bb.utils.contains('MACHINE_FEATURES', 'split-home', 'avengers-home.wks', 'avengers.wks', d)}"

fakeroot do_prebuilt() {
	tar --exclude=usr/lib/firmware --exclude=usr/lib/modules -xf ${IMAGE_ROOTFS}/rootfs.tar.* -C ${IMAGE_ROOTFS}
	rm -f ${IMAGE_ROOTFS}/rootfs.tar.*
	tar -xf ${IMAGE_ROOTFS}/configs.tar.xz -C ${IMAGE_ROOTFS}/etc
	rm -f ${IMAGE_ROOTFS}/configs.tar.xz
}

do_prebuilt[depends] += "virtual/fakeroot-native:do_populate_sysroot"

addtask prebuilt after do_rootfs before do_image
