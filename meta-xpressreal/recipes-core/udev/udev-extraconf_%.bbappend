FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
		file://mount_sh.patch \
		"
ERROR_QA:remove = "patch-status"

MOUNT_BASE = "/run/media"

# keeps only automount.rules
do_install:append() {
    rm -f ${D}${sysconfdir}/udev/rules.d/autonet.rules
    rm -f ${D}${sysconfdir}/udev/rules.d/localextra.rules
}
