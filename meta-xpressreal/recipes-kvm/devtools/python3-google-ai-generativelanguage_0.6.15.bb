SUMMARY = "Google AI Generative Language API client library"
HOMEPAGE = "https://github.com/googleapis/google-api-python-client"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=3b83ef96387f14655fc854ddc3c6bd57"

SRC_URI = "file://google-ai-generativelanguage/google_ai_generativelanguage-${PV}.tar.gz"

SRC_URI[sha256sum] = "8f6d9dc4c12b065fe2d0289026171acea5183ebf2d0b11cefe12f3821e159ec3"

S = "${WORKDIR}/google_ai_generativelanguage-${PV}"

inherit setuptools3

RDEPENDS:${PN} += "\
    python3-google-api-core \
    python3-google-auth \
    python3-proto-plus \
    python3-protobuf \
"
