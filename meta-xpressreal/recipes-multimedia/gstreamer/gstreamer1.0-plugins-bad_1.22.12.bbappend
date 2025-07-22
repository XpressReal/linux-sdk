FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}-${PV}:"

PACKAGECONFIG:append:pn-gstreamer1.0-plugins-bad = " assrender kms"
SRC_URI:append = " \
    file://0005-to-support-render-rectangle-on-waylandsink.patch \
    file://0006-add-property-to-clear-subtitle-immediately.patch \
    file://0007-add-property-to-silent-subtitle.patch \
    file://0008-fix-initial-state-to-clear-subtitle.patch \
    file://0009-Increase-the-rank-of-vc1parse.patch \
    file://0010-waylandsink-release-buffer-when-flush.patch \
    "
