SUMMARY = "display dialog boxes from shell scripts"
DESCRIPTION = "Dialog lets you to present a variety of questions \
or display messages using dialog boxes from a shell \
script (or any scripting language)."
HOMEPAGE = "http://invisible-island.net/dialog/"
SECTION = "console/utils"
LICENSE = "LGPL-2.1-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=a6f89e2100d9b6cdffcea4f398e37343"

# Dependencies for static compilation
DEPENDS = "ncurses"

SRC_URI = "https://invisible-mirror.net/archives/${BPN}/dialog-${PV}.tgz"
SRC_URI[sha256sum] = "339d311c6abb240213426b99ad63565cbcb3e8641ef1989c033e945b754d34ef"

S = "${WORKDIR}/dialog-${PV}"

inherit autotools-brokensep pkgconfig

# Explicitly ensure no dynamic X11 features attempt to link
PACKAGECONFIG = ""

# Pass the static flag securely to the target toolchain linker
EXTRA_OEMAKE += "LDFLAGS='${LDFLAGS} -static'"

EXTRA_OECONF += "--with-ncurses \
                 --disable-rpath-hack \
                 --enable-static --disable-shared"

do_configure() {
    # Update config.guess and config.sub for cross-compilation
    gnu-configize --force

    # Fix ncurses config scripts for the sysroot
    sed -i 's,${cf_ncuconfig_root}6-config,${cf_ncuconfig_root}-config,g' configure
    sed -i 's,cf_have_ncuconfig=unknown,cf_have_ncuconfig=yes,g' configure

    # Execute the shipped configure script directly (Do NOT use autotools_do_configure)
    oe_runconf
}

# Clear runtime dependencies that are now baked into the binary
RDEPENDS:${PN} = ""

inherit deploy nopackages

do_deploy() {
	${STRIP} ${B}/dialog
	install -d ${DEPLOYDIR}/staging
	install -m 0755 ${B}/dialog ${DEPLOYDIR}/staging/dialog
}

addtask deploy after do_install
