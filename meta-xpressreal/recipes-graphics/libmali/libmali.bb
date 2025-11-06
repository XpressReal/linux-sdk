SUMMARY = "ARM libmali"
LICENSE = "CLOSED"

inherit bin_package pkgconfig

DISTRO_FEATURES:append = "usrmerge"
DEPENDS += "wayland libdrm"

PROVIDES = "virtual/libgles2 virtual/egl virtual/libgbm"

PREBUILT_DIR:hank = "malig57-r44p1-01eac0-wayland-drm-a64"
PREBUILT_DIR:stark = "malig57-r44p1-01eac0-wayland-drm-a64"
PREBUILT_DIR:kent = "malig310-r44p1-01eac0-wayland-drm-a64"

SRCREV = "${AUTOREV}"
SRC_URI = "file://${PREBUILT_DIR}.tar.bz2"

S = "${WORKDIR}/${PREBUILT_DIR}"

FILES:${PN} += "${libdir}/* ${base_libdir}/*"

FILES_SOLIBSDEV = ""

INSANE_SKIP:${PN} += "already-stripped"
INSANE_SKIP:${PN} += "ldflags"
INSANE_SKIP:${PN} += "dev-so"
