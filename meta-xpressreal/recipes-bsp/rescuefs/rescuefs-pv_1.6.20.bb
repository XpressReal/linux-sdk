SUMMARY = "Terminal-based tool for monitoring the progress of data through a pipeline"
HOMEPAGE = "http://www.ivarch.com/programs/pv.shtml"

LICENSE = "Artistic-2.0"
LIC_FILES_CHKSUM = "file://doc/COPYING;md5=9c50db2589ee3ef10a9b7b2e50ce1d02"

SRC_URI = "https://www.ivarch.com/programs/sources/pv-1.6.20.tar.bz2 \
           file://0001-pv-display-handle-error-of-tcgetpgrp-in-pv_in_foregr.patch \
"
SRC_URI[sha256sum] = "e831951eff0718fba9b1ef286128773b9d0e723e1fbfae88d5a3188814fdc603"

UPSTREAM_CHECK_URI = "http://www.ivarch.com/programs/pv.shtml"
UPSTREAM_CHECK_REGEX = "pv-(?P<pver>\d+(\.\d+)+).tar.bz2"

S = "${WORKDIR}/pv-1.6.20"

inherit autotools deploy nopackages

TARGET_LDFLAGS:append = " -static"

do_deploy() {
	${STRIP} ${B}/pv
	install -d ${DEPLOYDIR}/staging
	install -m 0755 ${B}/pv ${DEPLOYDIR}/staging/
}

addtask deploy after do_install
