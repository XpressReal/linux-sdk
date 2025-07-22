DESCRIPTION = "Generate resize2fs from e2fsprogs for rescue rootfs"

require recipes-devtools/e2fsprogs/e2fsprogs.inc

BBCLASSEXTEND=""

SRCREV = "f4c9cc4bedacde8408edda3520a32d3842290112"
UPSTREAM_CHECK_GITTAGREGEX = "v(?P<pver>\d+\.\d+(\.\d+)*)$"

LDFLAGS:append = " -static"
EXTRA_OECONF:remove = "--disable-static"

do_install () {
	${STRIP} ${B}/resize/resize2fs ${B}/e2fsck/e2fsck
	install -d ${D}/${base_libdir}
	install -m 0644 ${B}/${baselib}/libuuid.a ${D}/${base_libdir}/
}

inherit deploy nopackages

do_deploy() {
	install -d ${DEPLOYDIR}/staging
	install -m 0755 ${B}/resize/resize2fs ${DEPLOYDIR}/staging/
	install -m 0755 ${B}/e2fsck/e2fsck ${DEPLOYDIR}/staging/
}

addtask deploy after do_install
