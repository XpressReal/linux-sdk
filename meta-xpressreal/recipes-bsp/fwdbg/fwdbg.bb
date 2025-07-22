SUMMARY = "fwdbg a32 binary"
LICENSE = "CLOSED"
SRCREV = "${AUTOREV}"

SRC_URI = "\
        file://fwdbg \
        "

do_install() {
        install -d ${D}${bindir}
        install -m 0755 ${WORKDIR}/fwdbg ${D}${bindir}
}

do_package_qa[noexec] = "1"

FILES_${PN} += "${bindir}/"
