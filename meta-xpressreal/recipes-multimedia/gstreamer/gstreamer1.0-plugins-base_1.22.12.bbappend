FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}-${PV}:"

PACKAGECONFIG:append:pn-gstreamer1.0-plugins-base = " opus"

SRC_URI:append = " \
    file://0001-UPSTREAM-videodecoder-set-decode-only-flag-by-decode.patch \
    file://0002-Support-AV1-for-riff.patch \
    file://0003-UPSTREAM-appsink-add-propose_allocation-support.patch \
    file://0004-NASPRJ-1157-fix-stream-have-wrong-pts-issue.patch \
    file://0005-Add-robust-PTS-fallback-for-frames-with-missing-time.patch \
    "
