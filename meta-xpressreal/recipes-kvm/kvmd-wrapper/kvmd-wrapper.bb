SUMMARY = "kvmd SUID wrapper for setting rooted files"
LICENSE = "CLOSED"

SRC_URI = "\
	file://kvmd-wrapper.c \
	file://kvmd-set-lun-file.py \
	file://kvmd-usbip.py \
	"

S = "${WORKDIR}"

inherit pkgconfig

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -O2 kvmd-wrapper.c -o kvmd-wrapper
}

do_install() {
    install -D -p -m 4755 ${WORKDIR}/kvmd-wrapper ${D}${exec_prefix}/local/bin/kvmd-wrapper
    install -D -p -m 0755 ${WORKDIR}/kvmd-set-lun-file.py ${D}${exec_prefix}/local/bin/kvmd-set-lun-file.py
    install -D -p -m 0755 ${WORKDIR}/kvmd-usbip.py ${D}${exec_prefix}/local/bin/kvmd-usbip.py
}

FILES:${PN} += "/usr/local/bin"

