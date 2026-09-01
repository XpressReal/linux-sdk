# until fully tested, prefer `libwayland-egl` provided by `userland` instead of `wayland` when not using vc4graphics
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " \
    file://0001-wayland-add-wl_surface-wayland-protocol.patch \
    "

do_install:append() {
    if [ "${@bb.utils.contains("MACHINE_FEATURES", "panfrost", "1", "0", d)}" = "0" ]; then
        rm -f ${D}${libdir}/libwayland-egl*
        rm -f ${D}${libdir}/pkgconfig/wayland-egl.pc
    fi
}
