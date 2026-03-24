DESCRIPTIOM = "Linux Based Image for GamePad"
LICENSE = "CLOSED"

require recipes-xpressreal/images/linux-image.bb

PACKAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'rtk-mod-npu', '', d)}"
PACKAGE_INSTALL:append = " prebuilt-rootfs"

#IMAGE_NAME = "gnulinux"
