FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
        file://0014-enable-stateful-av1-format.patch \
        file://0015-NASPRJ-1344-resolution-change-issue-on-youtube.patch \
        file://0016-Downgrade-wayland-version-requirement-to-1.22.patch \
        file://0017-NASPRJ-1406-add-stateful-h265-format.patch \
        file://0018-using-AFBC-on-ozone-wayland.patch \
        "

PACKAGECONFIG:append = " proprietary-codecs"
PACKAGECONFIG[use-v4l2] = "use_v4l2_codec=true enable_hevc_parser_and_hw_decoder=true enable_platform_hevc=true,use_v4l2_codec=false"

DEPENDS:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', '', 'dummy-dri', d)}"

GN_ARGS:append = " use_system_libwayland=true"

do_configure:prepend() {
    if [ ! -f "${STAGING_LIBDIR_NATIVE}/pkgconfig/wayland-egl.pc" ]; then
        install -d ${STAGING_LIBDIR_NATIVE}/pkgconfig
        install -m 0644 ${RECIPE_SYSROOT}/usr/lib/pkgconfig/wayland-egl.pc \
                        ${STAGING_LIBDIR_NATIVE}/pkgconfig/wayland-egl.pc
    fi
}
