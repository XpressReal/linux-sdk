FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}-${PV}:"

PACKAGECONFIG:append:pn-gstreamer1.0-plugins-bad = " assrender kms opusparse"

SRC_URI:append = " \
    file://0005-to-support-render-rectangle-on-waylandsink.patch \
    file://0006-add-property-to-clear-subtitle-immediately.patch \
    file://0007-add-property-to-silent-subtitle.patch \
    file://0008-fix-initial-state-to-clear-subtitle.patch \
    file://0009-Increase-the-rank-of-vc1parse.patch \
    file://0010-waylandsink-release-buffer-when-flush.patch \
    file://0011-gstwaylandsink-to-handle-trick-play-behavior.patch \
    file://0012-Keeps-HDR-info-even-can-t-parse-SEI.patch \
    file://0013-gstwaylandsink-keep-2-frame-for-interlace-video.patch \
    file://0014-gstwaylandsink-call-gst_video_frame_unmap-to-avoid-m.patch \
    file://0015-Increase-the-rank-of-jpegparse.patch \
    file://0016-waylandsink-to-support-P010-format.patch \
    file://0017-waylandsink-to-support-NV24-format.patch \
    file://0018-mpegts-decsriptors-parsed-fail-error-handling.patch \
    file://0019-NASPRJ-1158-fix-video-shuttering-issue.patch \
    file://0020-waylandsink-add-lock-to-avoid-null-last-buffer.patch \
    file://0021-add-codec_data_size-6-for-vc1-WMV3-main-profile-case.patch \
    "
