SUMMARY = "Secure RTP library (libsrtp) version 2"
DESCRIPTION = "SRTP reference implementation providing encryption, message authentication, and replay protection"
HOMEPAGE = "https://github.com/cisco/libsrtp"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=2909fcf6f09ffff8430463d91c08c4e1"

SRC_URI = "git://github.com/cisco/libsrtp.git;protocol=https;branch=2_2_x_throttle"
SRCREV = "94ac00d5ac6409e3f6409e4a5edfcdbdaa7fdabe"

S = "${WORKDIR}/git"

EXTRA_OEMAKE = "prefix=${prefix} DESTDIR=${D} CC='${CC}'"

DEPENDS = "zlib openssl"

do_configure() {
    ./configure --prefix=${prefix} --host=${HOST_SYS} --enable-openssl
}

do_compile() {
    oe_runmake shared_library
}

do_install() {
    oe_runmake install

    # Move headers to standard location
    install -d ${D}${includedir}/srtp2
    cp -r ${S}/include/* ${D}${includedir}/srtp2/

    # Manually install pkg-config file
    install -d ${D}${libdir}/pkgconfig
    cat > ${D}${libdir}/pkgconfig/libsrtp2.pc <<EOF
prefix=${prefix}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include/srtp2

Name: libsrtp2
Description: Secure RTP reference implementation (v2.2.0)
Version: 2.2.0
Libs: -L\${libdir} -lsrtp2
Cflags: -I\${includedir}
EOF
}

# Provide a pkg-config file if needed by downstream (e.g., Janus)
FILES:${PN} += "${libdir}/*.so* ${includedir}/srtp2"
