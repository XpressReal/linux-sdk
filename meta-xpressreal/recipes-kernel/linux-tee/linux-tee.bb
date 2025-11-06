# Copyright (C) 2023, Realtek Semiconductor Corp.
SUMMARY = "TEE Client and CA/TA"
LICENSE = "CLOSED"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append:stark = " \
		file://rtd1619b/libteec.so \
		file://rtd1619b/tee-supplicant \
		file://rtd1619b/77280208-346f-42e7-b1653bdd457c418d.ta \
		file://rtd1619b/c8280208-346f-42e7-b1653bdd457c418d.ta \
		file://rtd1619b/df280208-346f-42e7-b1653bdd457c418d.ta \
		file://rtd1619b/fa280208-346f-42e7-b1653bdd457c418d.ta \
		file://rtd1619b/pkcs11-tool \
		file://rtd1619b/libckteec.so \
		file://rtd1619b/fd02c9da-306c-48c7-a49c-bbd827ae86ee.ta \
		"

INSANE_SKIP:${PN} += "already-stripped"
#INSANE_SKIP_${PN} += " ldflags"
#INHIBIT_PACKAGE_STRIP = "1"
#INHIBIT_SYSROOT_STRIP = "1"
#SOLIBS = ".so"
FILES_SOLIBSDEV = ""


# Install addition firmwares
do_install:append:stark() {
	install -d ${D}${libdir}
	install -d ${D}${libdir}/teetz
	install -d ${D}${bindir}
	install -m 0755 ${WORKDIR}/rtd1619b/libteec.so ${D}${libdir}/
	install -m 0755 ${WORKDIR}/rtd1619b/tee-supplicant ${D}${bindir}/
	install -m 0644 ${WORKDIR}/rtd1619b/77280208-346f-42e7-b1653bdd457c418d.ta ${D}${libdir}/teetz/
	install -m 0644 ${WORKDIR}/rtd1619b/c8280208-346f-42e7-b1653bdd457c418d.ta ${D}${libdir}/teetz/
	install -m 0644 ${WORKDIR}/rtd1619b/df280208-346f-42e7-b1653bdd457c418d.ta ${D}${libdir}/teetz/
	install -m 0644 ${WORKDIR}/rtd1619b/fa280208-346f-42e7-b1653bdd457c418d.ta ${D}${libdir}/teetz/
	install -m 0755 ${WORKDIR}/rtd1619b/pkcs11-tool ${D}${bindir}/
	install -m 0755 ${WORKDIR}/rtd1619b/libckteec.so ${D}${libdir}/
	install -m 0644 ${WORKDIR}/rtd1619b/fd02c9da-306c-48c7-a49c-bbd827ae86ee.ta ${D}${libdir}/teetz/
}

FILES:${PN} = "${libdir}/*"
FILES:${PN} =+ "${bindir}/*"

RDEPENDS:${PN} += "libcrypto libssl"
