DESCRIPTION = "Avengers image with KVM Support"

#need to add extra two layers, one is meta-openembedded/meta-networking, the other is meta-openembedded/meta-python

#inherit core-image
#
require recipes-graphics/images/core-image-weston.bb

LICENSE = "MIT"

WKS_FILE := "pikvm.wks"

IMAGE_INSTALL:append = " ustreamer kvmd tesseract sudo iproute2 iptables janus libpython3 wpa-supplicant systemd-boot systemd-conf wifi-config"

IMAGE_INSTALL:append = " \
		libdrm libdrm-tests \
		rtk-mod-v4l2dec v4l-utils v4l2test \
		rtk-mod-v4l2cap \
		gstreamer1.0 \
		gstreamer1.0-python \
		gstreamer1.0-plugins-base \
		gstreamer1.0-plugins-good \
		gstreamer1.0-plugins-bad \
		gstreamer1.0-meta-base \
		"

