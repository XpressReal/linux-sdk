SUMMARY = "llama.cpp - lightweight LLM inference for GGUF models"
DESCRIPTION = "CPU-only inference engine for GGUF LLM models like TinyLlama, built from llama.cpp"
HOMEPAGE = "https://github.com/ggml-org/llama.cpp"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=1539dadbedb60aa18519febfeab70632"

SRC_URI = "git://github.com/ggml-org/llama.cpp;branch=master;protocol=https \
          "

SRCREV = "cbc68be51d88b1d5531643b926a4b359c3cff131"

B = "${WORKDIR}/build"
S = "${WORKDIR}/git"

inherit cmake systemd

DEPENDS += "curl tinyllama-gguf"
RDEPENDS_${PN} += "curl"


EXTRA_OECMAKE += "\
    -DCMAKE_C_FLAGS='-O3 -mcpu=cortex-a55' \
    -DCMAKE_CXX_FLAGS='-O3 -mcpu=cortex-a55' \
    -DGGML_CPU=cortex-a55 \
    -DGGML_FORCE_FP16=OFF \
    -DLLAMA_OPENMP=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DLLAMA_STATIC=ON \
"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/bin/llama-run ${D}${bindir}/llama-run
    install -m 0755 ${B}/bin/llama-cli ${D}${bindir}/llama-cli
}

FILES:${PN} += "${bindir}/llama-run ${bindir}/llama-cli"
INSANE_SKIP:${PN} += "buildpaths already-stripped"

