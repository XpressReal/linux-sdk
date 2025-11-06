FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " file://fstab-kvm"

do_install:append() {
	if [ "${@bb.utils.contains("MACHINE_FEATURES", "qt", "1", "0", d)}" = "1" ]; then
		# set UTF-8 to shell login environment to show correct fonts
		sed -i '$a export LC_ALL=en_US.UTF-8' ${D}/etc/profile
	fi

	if [ "${@bb.utils.contains("MACHINE_FEATURES", "kvm", "1", "0", d)}" = "1" ]; then
		install -d ${D}${sysconfdir}
		install -m 0644 ${WORKDIR}/fstab-kvm ${D}${sysconfdir}/fstab
	fi

}
