SUMMARY = "ARM libmali"
LICENSE = "CLOSED"

inherit bin_package pkgconfig

DEPENDS += "wayland libdrm"

PROVIDES = "virtual/libgles2 virtual/egl virtual/libgbm"

SRCREV = "${AUTOREV}"
SRC_URI = "file://malig57-r44p1-01eac0-wayland-drm-a64.tar.bz2"

PREBUILT_DIR = "malig57-r44p1-01eac0-wayland-drm-a64"
S = "${WORKDIR}/${PREBUILT_DIR}"

FILES:${PN} += "${libdir}/*"

FILES_SOLIBSDEV = ""

INSANE_SKIP:${PN} += "already-stripped"
INSANE_SKIP:${PN} += "ldflags"
INSANE_SKIP:${PN} += "dev-so"
