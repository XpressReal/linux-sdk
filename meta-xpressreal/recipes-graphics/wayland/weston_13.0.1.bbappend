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
    file://0016-to-enable-or-disable-UI-hole-punch.patch \
    file://0017-fix-entering-wrong-view.patch \
    file://0018-add-default-max-mode-to-choose-max-tv-mode.patch \
    file://0019-remove-max-bpc-setting.patch \
    file://0020-SW-9314-add-NV16-and-RGB888-to-support-mjpeg-render.patch \
    file://0021-weston-to-support-display-control-for-kms_ipc.patch \
    file://0022-weston-to-support-display-and-vo-control.patch \
    file://0024-weston-add-P010-format-support.patch \
    file://0025-NASPRJ-1171-Add-NV24-to-support-mjpeg-render.patch \
    file://0028-release-framebuffer-when-drm-output-was-destroyed.patch \
    file://0030-weston-fix-transparency-of-gui-fail-after-hotplug-hdmi.patch \
    file://0031-weston-modeset-fail-when-hotplug.patch \
    file://0032-weston-handling-consecutive-hotplug-events.patch \
    file://0033-weston-fix-weston-crash-when-receiving-consecutive-h.patch \
    file://0034-weston-avoid-background-transparent-sw-cursor-trailing.patch \
    file://0035-NASPRJ-1360-support-mali-afrc-mode.patch \
    file://0036-OPC-26-add-rtk-demo-app.patch \
    file://0037-adjust-ui-and-tv-mode-select-method.patch \
    file://0038-weston-add-drm-fence-flow.patch \
    file://0039-reintegrated-weston-KMS-control-architecture.patch \
    file://0040-xwayland-add-ac-flag-to-disable-access-control.patch \
    file://0041-add-rtk-bg-layer-for-application.patch \
    file://0042-fix-race-condition-for-fence_refcnt.patch \
    file://0043-adjust-tv-mode-setting-flow.patch \
    "
