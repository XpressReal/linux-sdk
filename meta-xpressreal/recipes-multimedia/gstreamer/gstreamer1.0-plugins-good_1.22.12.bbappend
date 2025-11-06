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
    file://0009-Limit-the-sink-caps-of-v4l2dec-to-realtek-spec.patch \
    file://0010-UPSTREAM-v4l2bufferpool-queue-back-the-buffer-flagge.patch \
    file://0011-UPSTREAM-Sync-upstream-memory-leakage-related-patche.patch \
    file://0012-Fix-memory-leakage-while-encoding.patch \
    file://0013-UPSTREAM-Sync-upstream-codes.patch \
    file://0014-Release-encoder-buffer-pooling-during-flushing.patch \
    file://0015-V4l2transform-add-property-to-configure-crop-compose.patch \
    file://0016-Fix-segmentfault-from-gst_v4l2_buffer_pool_dqbuf.patch \
    file://0017-Fix-got-stuck-issue-after-quick-re-seek.patch \
    file://0018-Do-not-trigger-format-change-while-only-HDR-metadata.patch \
    file://0019-Fix-WVC1-can-t-decode-issue.patch \
    file://0020-Interlaced-mode-drop-old-frame-do-not-use-g_warning.patch \
    file://0021-Support-P010.patch \
    file://0022-Fix-Bad-first_access-parameter-for-AC3.patch \
    file://0023-Fix-EOS-flush-running-during-resolution-change-event.patch \
    file://0024-Do-not-check-the-duration-of-each-video-tracks.patch \
    file://0025-fix-memory-leak-in-qtdemux_parse_trak.patch \
    file://0026-Fix-IS_MUTABLE-structure-issue.patch \
    "
