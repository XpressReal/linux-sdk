FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}-${PV}:"

PACKAGECONFIG:append:pn-gstreamer1.0-plugins-bad = " assrender kms opusparse"

SRC_URI:append = " \
    file://0005-waylandsink-provide-render-rectangle-setting.patch \
    file://0006-waylandsink-add-clear-and-slient-property.patch \
    file://0007-waylandsink-to-support-NV24-and-P010-format.patch \
    file://0008-waylandsink-add-stream-id-property-for-showing-fps.patch \
    file://0009-Increase-the-rank-of-vc1parse.patch \
    file://0012-Keeps-HDR-info-even-can-t-parse-SEI.patch \
    file://0015-Increase-the-rank-of-jpegparse.patch \
    file://0018-mpegts-decsriptors-parsed-fail-error-handling.patch \
    file://0019-NASPRJ-1158-fix-video-shuttering-issue.patch \
    file://0021-add-codec_data_size-6-for-vc1-WMV3-main-profile-case.patch \
    "
