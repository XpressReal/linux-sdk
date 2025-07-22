FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}-${PV}:"

SRC_URI:append = " \
    file://0001-UPSTREAM-videodecoder-set-decode-only-flag-by-decode.patch \
    file://0002-Support-AV1-for-riff.patch \
    "
