# Copyright (C) 2023, Realtek Semiconductor Corp.
SUMMARY = "TEE Client and CA/TA"
LICENSE = "CLOSED"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = ""

SOC_MODEL = ""
SOC_MODEL:stark = "rtd1619b"
SOC_MODEL:kent = "rtd1625"

SRC_URI:append:stark = " \
	file://${SOC_MODEL}/tee-supplicant \
	file://${SOC_MODEL}/pkcs11-tool \
	file://${SOC_MODEL}/libteec.so \
	file://${SOC_MODEL}/libckteec.so \
	file://${SOC_MODEL}/77280208-346f-42e7-b1653bdd457c418d.ta \
	file://${SOC_MODEL}/c8280208-346f-42e7-b1653bdd457c418d.ta \
	file://${SOC_MODEL}/df280208-346f-42e7-b1653bdd457c418d.ta \
	file://${SOC_MODEL}/fa280208-346f-42e7-b1653bdd457c418d.ta \
	file://${SOC_MODEL}/fd02c9da-306c-48c7-a49c-bbd827ae86ee.ta \
"

SRC_URI:append:kent = " \
	file://${SOC_MODEL}/tee-supplicant \
	file://${SOC_MODEL}/pkcs11-tool \
	file://${SOC_MODEL}/libp11.so \
	file://${SOC_MODEL}/libteec.so \
	file://${SOC_MODEL}/libteec.so.1 \
	file://${SOC_MODEL}/libteec.so.1.0 \
	file://${SOC_MODEL}/libteec.so.1.0.0 \
	file://${SOC_MODEL}/libckteec.so \
	file://${SOC_MODEL}/libckteec.so.0 \
	file://${SOC_MODEL}/4e280208-346f-42e7-b1653bdd457c418d.ta \
	file://${SOC_MODEL}/8aaaf200-2450-11e4-abe20002a5d5c51b.ta \
	file://${SOC_MODEL}/c8280208-346f-42e7-b1653bdd457c418d.ta \
	file://${SOC_MODEL}/fa280208-346f-42e7-b1653bdd457c418d.ta \
	file://${SOC_MODEL}/fd02c9da-306c-48c7-a49c-bbd827ae86ee.ta \
"

S = "${WORKDIR}"

INSANE_SKIP:${PN} += "already-stripped"
FILES_SOLIBSDEV = ""

PACKAGES = "${PN} ${PN}-dbg ${PN}-dev"

do_install() {
	install -d ${D}${libdir} ${D}${libdir}/teetz ${D}${bindir}
	srcdir="${WORKDIR}/${SOC_MODEL}"

	# library
	for so in libteec.so libteec.so.1 libteec.so.1.0 libteec.so.1.0.0 libckteec.so libckteec.so.0 libp11.so; do
		if [ -f "${srcdir}/$so" ]; then
			install -m 0644 "${srcdir}/$so" ${D}${libdir}/
		fi
	done

	# execute file
	for bin in tee-supplicant pkcs11-tool; do
		if [ -f "${srcdir}/$bin" ]; then
			install -m 0755 "${srcdir}/$bin" ${D}${bindir}/
	fi
	done

	for ta in "${srcdir}"/*.ta; do
		if [ -f "$ta" ]; then
			install -m 0644 "$ta" ${D}${libdir}/teetz/
		fi
	done
}

FILES:${PN} += " \
	${libdir}/libteec.so* \
	${libdir}/libckteec.so* \
	${libdir}/teetz \
	${libdir}/teetz/*.ta \
	${bindir}/tee-supplicant \
	${bindir}/pkcs11-tool \
"
FILES:${PN}:append:kent = " ${libdir}/libp11.so"

RDEPENDS:${PN} += "libcrypto libssl"
