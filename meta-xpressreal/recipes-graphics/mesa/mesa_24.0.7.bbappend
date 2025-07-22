FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
		file://001-dri-add-realtek_dri-support.patch \
		file://drirc \
		"

PACKAGECONFIG:append = " kmsro panfrost"

do_install:append() {
	install -d ${D}${sysconfdir}
	install -D -p -m 0644 ${WORKDIR}/drirc ${D}${sysconfdir}/drirc
}

ERROR_QA:remove = "patch-status"

PACKAGES =+ "mesa-avengers"
FILES:${PN}-avengers = "${sysconfdir}/drirc"
