# 1. Force static compilation back on globally for this recipe
DISABLE_STATIC = ""

# 2. Aggressively remove any hidden flags blocking normal static library generation
EXTRA_OECONF:remove = "--without-normal"
EXCONFIG_ARGS:remove = "--without-normal"

# 3. Explicitly append the upstream flag required to build the C static (.a) archives
EXTRA_OECONF += "--with-normal"

inherit deploy

do_deploy() {
	# Only deploy files for the target build; skip if building ncurses-native
	if [ "${PN}" = "ncurses" ]; then
		install -d ${DEPLOYDIR}/staging/terminfo/x
		install -m 0644 ${D}/etc/terminfo/x/xterm-color ${DEPLOYDIR}/staging/terminfo/x/xterm
	fi
}

addtask deploy after do_install
