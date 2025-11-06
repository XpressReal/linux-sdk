SUMMARY = "A Python module to customize the process title"
DESCRIPTION = "The setproctitle module allows a process to change its \
title (as displayed by system tools such as ps, top or MacOS Activity \
Monitor)."
HOMEPAGE = "https://github.com/dvarrazzo/py-setproctitle"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://COPYRIGHT;md5=86d2d41b5f4f023f43466f8cb7adebaa"

SRC_URI[sha256sum] = "c9f32b96c700bb384f33f7cf07954bb609d35dd82752cef57fb2ee0968409169"

inherit pypi setuptools3
