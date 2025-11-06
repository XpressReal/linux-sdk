SUMMARY = "Python X Library (pure Python implementation of X11 protocol)"
HOMEPAGE = "https://github.com/python-xlib/python-xlib"
LICENSE = "LGPL-2.1"
LIC_FILES_CHKSUM = "file://LICENSE;md5=8975de00e0aab10867abf36434958a28"

SRC_URI = "https://github.com/python-xlib/python-xlib/releases/download/0.33/python-xlib-0.33.tar.gz"

SRC_URI[sha256sum] = "55af7906a2c75ce6cb280a584776080602444f75815a7aff4d287bb2d7018b32"

S = "${WORKDIR}/python-xlib-0.33"

DEPENDS += "python3-setuptools-scm-native"

inherit setuptools3
