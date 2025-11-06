# Copyright (C) 2025, Realtek Semiconductor Corp.
SUMMARY = "TinyLlama GGUF model file"
DESCRIPTION = "The TinyLlama 1.1B Chat model in GGUF format"
HOMEPAGE = "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${WORKDIR}/tinyllama/LICENSE.txt;md5=4d8b4b1669dfba9098273a77e7d6aac6"
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = " \
			git://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF;branch=main;protocol=https \
			file://tinyllama/LICENSE.txt \
			"
SRCREV = "52e7645ba7c309695bec7ac98f4f005b139cf465"
SRC_URI[sha256sum] = "bb1731a72bbe4136f37f905fcd50643ce7dc3528aed1de0fb1743883f0645006"

B = "${WORKDIR}"
S = "${WORKDIR}/git"

do_install() {
	install -d ${D}/root/ai-demo
	install -d ${D}/root/ai-demo/tinyllama_gguf
	install -m 0644 ${S}/tinyllama-1.1b-chat-v1.0.Q4_0.gguf ${D}/root/ai-demo/tinyllama_gguf
    #install -m 0644 ${S}/tinyllama-1.1b-chat-v1.0.Q4_K_S.gguf ${D}/root/ai-demo/tinyllama_gguf
	#install -m 0644 ${S}/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf ${D}/root/ai-demo/tinyllama_gguf	
}

FILES:${PN} += "/root/ai-demo"
