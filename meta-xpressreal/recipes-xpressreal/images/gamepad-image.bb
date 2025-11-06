DESCRIPTIOM = "Linux Based Image for GamePad"
LICENSE = "CLOSED"

require recipes-xpressreal/images/linux-image.bb

PACKAGE_INSTALL:append = " gamepad-rootfs"

#IMAGE_NAME = "gamepad"
