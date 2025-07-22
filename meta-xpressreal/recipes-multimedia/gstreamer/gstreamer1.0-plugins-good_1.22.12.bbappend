FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}-${PV}:"

SRC_URI:append = " \
    file://0001-Support-stateful-v4l2-av1-decode.patch \
    file://0002-UPSTREAM-append-non-colorimetry-structure-to-probed-.patch \
    file://0003-UPSTREAM-passing-HDR10-information.patch \
    file://0004-UPSTREAM-handle-unsupported-hlg-colorimetry-graceful.patch \
    file://0005-UPSTREAM-v4l2videodec-release-decode-only-frame-in-i.patch \
    file://0006-Force-vp9parse-to-output-frame-instead-of-super-fram.patch \
    file://0007-Fixed-deadlock-while-gst_element_seek-with-GST_SEEK_.patch \
    file://0008-Support-video-x-wmv-with-WVC1-and-WMV3-formats.patch \
    file://0009-Limit-the-sink-caps-of-v4l2dec-to-realtek-spec.patch  \
    "
