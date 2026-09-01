DESCRIPTION = "Avengers image with NAS features"

#need to add extra two layers, one is meta-openembedded/meta-networking, the other is meta-openembedded/meta-python

inherit core-image

LICENSE = "MIT"

IMAGE_INSTALL += "samba ecryptfs-utils fio iperf3 parted e2fsprogs e2fsprogs-resize2fs usbutils pciutils nginx"

include benchmark.inc

EXTRA_PACKAGES = " \
		rtk-mod-v4l2dec v4l-utils \
		gstreamer1.0 \
		gstreamer1.0-python \
		gstreamer1.0-plugins-base \
		gstreamer1.0-plugins-good \
		gstreamer1.0-plugins-bad \
		gstreamer1.0-meta-base \
		alsa-utils \
                "

PACKAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', ' ${EXTRA_PACKAGES}', '', d)}"
