FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " \
    file://0002-if-nplane-exceed-1-convert-to-GL_TEXTURE_EXTERNAL_OES.patch \
    file://0003-support-sw-cursor-config.patch \
    file://0004-enable-rtk-dmabuf-v1-flow.patch \
    file://0005-add-alpha-channel-when-gbm-format-ARGB8888.patch \
    file://0006-weston-use-triple-buffer-for-scanout-plane.patch \
    file://0007-weston-seperate-UI-size-and-tv-system-with-tv-mode.patch \
    file://0008-add-transparent-fade-layer-to-weston-ini-for-setting.patch \
    file://0009-to-support-render-rectangle-on-waylandsink.patch \
    file://0010-to-support-remote-control-kms.patch \
    file://0011-weston-add-gbm-afbc-flag-to-enable-or-disable-afbc-f.patch \
    file://0012-support-video-overlay-plane-on-weston.patch \
    file://0013-to-support-subtitle-layer-feature.patch \
    file://0014-fix-video-layer-won-t-enter-hw-video-plane-when-subt.patch \
    file://0015-keep-v1-frame-to-avoid-tearing-issue.patch \
    file://0016-to-enable-or-disable-UI-hole-punch.patch \
    "
