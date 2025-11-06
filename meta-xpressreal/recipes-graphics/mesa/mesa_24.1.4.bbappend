FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
		file://001-dri-add-realtek_dri-support.patch \
		file://drirc \
		"

SRC_URI:append:kent = "file://mali_csffw.bin"
PACKAGECONFIG:append = " kmsro panfrost"

do_install:append:stark() {
	install -d ${D}${sysconfdir}
	install -D -p -m 0644 ${WORKDIR}/drirc ${D}${sysconfdir}/drirc

}

do_install:append:kent() {
	install -d ${D}${sysconfdir}
	install -d ${D}${libdir}/firmware/arm/mali/arch10.12/
	install -D -p -m 0644 ${WORKDIR}/drirc ${D}${sysconfdir}/drirc
	install -D -p -m 0644 ${WORKDIR}/mali_csffw.bin ${D}${libdir}/firmware/arm/mali/arch10.12/mali_csffw.bin
}

ERROR_QA:remove = "patch-status"

PACKAGES =+ "mesa-avengers"
FILES:${PN}-avengers = "${sysconfdir}/drirc ${libdir}/firmware/arm/mali/arch10.12/*"
