SUMMERY = "STT demo"
DESCRIPTION = "STT demo for Yocto platform"
LICENSE = "CLOSED"

inherit pkgconfig
DEPENDS += "libnpu"
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SOC_NAME:stark = "1619b"
SOC_NAME:kent = "kent"

SRC_URI = " \
    file://stt/libquartznet.so \
    file://stt/${SOC_NAME}/network_binary.nb \
"

S = "${WORKDIR}/stt-demo"

do_install() {
	install -d ${D}/root/ai-demo
	install -d ${D}/root/ai-demo/stt-processing
	install -d ${D}/root/ai-demo/stt-processing/sample

	install -m 0755 ${WORKDIR}/stt/libquartznet.so ${D}/root/ai-demo/stt-processing/sample
	install -m 0755 ${WORKDIR}/stt/${SOC_NAME}/network_binary.nb ${D}/root/ai-demo/stt-processing/sample 
}

FILES:${PN} += " /root"
INSANE_SKIP:${PN} += "dev-so"
