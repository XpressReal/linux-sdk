# sourced from upstream ( meta-openembedded/meta-oe/recipes-multimedia/v4l2apps/v4l-utils_1.26.1.bb )

FILESEXTRAPATHS:prepend:= "${THISDIR}/files:"

#DEPENDS = "jpeg \
#          ${@bb.utils.contains('DISTRO_FEATURES', 'x11', 'virtual/libx11', '', d)} \
#           ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'systemd', '', d)} \
#           ${@bb.utils.contains('DISTRO_FEATURES', 'alsa', 'alsa-lib', '', d)} \
#           ${@bb.utils.contains_any('PACKAGECONFIG', 'qv4l2 qvidcap', 'qtbase qtbase-native', '', d)}"

# v4l2 explicitly sets _FILE_OFFSET_BITS=32 to get access to
# both 32 and 64 bit file APIs.  But it does not handle the time side?
# Needs further investigation
GLIBC_64BIT_TIME_FLAGS = ""

inherit meson gettext pkgconfig

PACKAGECONFIG ??= "media-ctl"
PACKAGECONFIG[media-ctl] = ""

#PACKAGECONFIG ??= ""
#PACKAGECONFIG[qv4l2] = ",-Dqv4l2=disabled"
#PACKAGECONFIG[qvidcap] = ",-Dqvidcap=disabled"
#PACKAGECONFIG[v4l2-tracer] = ",-Dv4l2-tracer=disabled,json-c"

#---------------------------------------
# Uncomment to build stable branch 1.26
#SRC_URI = "\
#    git://git.linuxtv.org/v4l-utils.git;protocol=https;branch=stable-1.26 \
#    file://0001-keytable-meson-Restrict-the-installation-of-50-rc_ke.patch \
#"
#SRCREV = "4aee01a027923cab1e40969f56f8ba58d3e6c0d1"
#--------------------------------------
# build latest 
SRC_URI = "\
    git://git.linuxtv.org/v4l-utils.git;protocol=https;branch=master \
"
# Include the below patch if tying to build version < 1.27 
#file://0001-keytable-meson-Restrict-the-installation-of-50-rc_ke.patch 

SRCREV = "d700deb143685b8217aa8a6eeeba3b090d4287fc"
#--------------------------------------

PV .= "+git"
S = "${WORKDIR}/git"
EXTRA_OEMESON = "-Dudevdir=${base_libdir}/udev -Dv4l2-compliance-32=false -Dv4l2-ctl-32=false"

# Disable the erroneous installation of gconv-modules that would break glib
# like it is done in Debian and ArchLinux.
EXTRA_OEMESON += "-Dgconv=disabled"

RPROVIDES:${PN}-dbg += "libv4l-dbg"

#PACKAGES =+ "media-ctl ir-keytable rc-keymaps libv4l libv4l-dev qv4l2 qvidcap"
#RDEPENDS:qv4l2 += "\
#    ${@bb.utils.contains('PACKAGECONFIG', 'qv4l2', 'qtbase', '', d)}"
#RDEPENDS:qvidcap += "\
#    ${@bb.utils.contains('PACKAGECONFIG', 'qvidcap', 'qtbase', '', d)}"
#FILES:libv4l += "${libdir}/libv4l2tracer.so \"
