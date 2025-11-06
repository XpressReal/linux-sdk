SUMMARY = "Parallel, indexed xz compressor (static binary)"
DESCRIPTION = "Pixz is a parallel, indexed xz compressor that can randomly access large xz files. This version is compiled as a static binary."
HOMEPAGE = "https://github.com/vasi/pixz"
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=5cf6d164086105f1512ccb81bfff1926"

PIXZ_VERSION = "1.0.7"

SRC_URI = "git://github.com/vasi/pixz.git;protocol=https;branch=master"

SRCREV = "573e4a8d2c9cc600ffeb109683d5a5904aa8fa50"

S = "${WORKDIR}/git"

DEPENDS = " \
    rescuefs-libarchive-static \
    rescuefs-liblzma-static \
    autoconf-native \
    automake-native \
    libtool-native \
    pkgconfig-native \
"

inherit autotools-brokensep pkgconfig

export ac_cv_prog_A2X = "no"
export ac_cv_prog_ASCIIDOC = "no"
export ac_cv_file_src_pixz_1 = "no"

do_configure:prepend() {
    cd ${S}
    
    export LIBARCHIVE_CFLAGS="-I${STAGING_INCDIR}"
    export LIBARCHIVE_LIBS="-L${STAGING_LIBDIR} -larchive -llzma"
    export LZMA_CFLAGS="-I${STAGING_INCDIR}"
    export LZMA_LIBS="-L${STAGING_LIBDIR} -llzma"
    
    autoreconf -fiv
}

EXTRA_OECONF = " \
    LIBARCHIVE_CFLAGS='-I${STAGING_INCDIR}' \
    LIBARCHIVE_LIBS='-L${STAGING_LIBDIR} -larchive -llzma' \
    LZMA_CFLAGS='-I${STAGING_INCDIR}' \
    LZMA_LIBS='-L${STAGING_LIBDIR} -llzma' \
"

LDFLAGS += "-static"

do_compile() {
    cd ${S}/src
    oe_runmake pixz
}

do_install() {
    ${STRIP} ${S}/src/pixz
    install -d ${D}${bindir}
    install -m 0755 ${S}/src/pixz ${D}${bindir}/
}

inherit deploy nopackages

do_deploy() {
	install -d ${DEPLOYDIR}/staging
	install -m 0755 ${S}/src/pixz ${DEPLOYDIR}/staging/pixz
}

FILES:${PN} = "${bindir}/pixz"

COMPATIBLE_HOST = "(aarch64|x86_64).*-linux"

addtask deploy after do_install
