SUMMARY = "Google Generative AI High level API client library and tools"
HOMEPAGE = "https://github.com/google/generative-ai-python"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=3b83ef96387f14655fc854ddc3c6bd57"

SRC_URI = "git://github.com/google/generative-ai-python.git;protocol=https;branch=main;tag=v0.8.6"

S = "${WORKDIR}/git"

inherit setuptools3

RDEPENDS:${PN} += "\
    python3-google-ai-generativelanguage \
    python3-google-api-core \
    python3-google-api-python-client \
    python3-google-auth \
    python3-protobuf \
    python3-pydantic \
    python3-tqdm \
    python3-typing-extensions \
"
