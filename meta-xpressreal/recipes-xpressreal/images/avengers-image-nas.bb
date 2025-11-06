DESCRIPTION = "Avengers image with NAS features"

#need to add extra two layers, one is meta-openembedded/meta-networking, the other is meta-openembedded/meta-python

inherit core-image

LICENSE = "MIT"

IMAGE_INSTALL += "samba ecryptfs-utils fio iperf3 parted e2fsprogs e2fsprogs-resize2fs usbutils pciutils nginx"

include benchmark.inc
