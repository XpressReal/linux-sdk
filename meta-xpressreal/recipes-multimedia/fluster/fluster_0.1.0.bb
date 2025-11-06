#
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:
#
# Copyright (C) 2007 Free Software Foundation, Inc.
#
# fluendo/fluster is licensed under the GNU Lesser General Public License v3.0
#
# https://github.com/fluendo/fluster/blob/master/LICENSE
#
# Permissions of this copyleft license are conditioned on making available
# complete source code of licensed works and modifications under the same license
# or the GNU GPLv3. Copyright and license notices must be preserved. Contributors
# provide an express grant of patent rights. However, a larger work using
# the licensed work through interfaces provided by the licensed work may be
# distributed under different terms and without source code for the larger work.

SUMMARY = "Fluster is a testing framework written in Python for decoder conformance."

LICENSE = "LGPL-3.0-or-later"
LICENSE_LOCATION ?= "${S}/LICENSE"
LIC_FILES_CHKSUM = "file://${LICENSE_LOCATION};md5=3000208d539ec061b899bce1d9ce9404"

RDEPENDS:${PN} += "bash python3-pip"

SRC_URI = "git://github.com/fluendo/fluster.git;protocol=https;branch=master"

SRCREV = "1bfcfe2c20f01b21d8942648609dbed09ba70839"

S = "${WORKDIR}/git"


inherit pkgconfig

do_install() {
	install -d ${D}${bindir}/fluster
	install -m 0755 ${S}/fluster.py ${D}${bindir}/
	cp -r ${S}/fluster ${D}${bindir}/
}


FILES_${PN} += "${bindir}/"
