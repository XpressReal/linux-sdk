# Copyright (C) 2025, Realtek Semiconductor Corp.

SUMMARY = "A face recognition application developed with reference to NXP's open-source NNStreamer examples."
DESCRIPTION = "NXP NNStreamer examples with face processing tasks. \
The facenetuint8.so and network_binary_facenet.nb are derived from FaceNet model (source: https://github.com/NXP/nxp-vision-model-zoo). \
The ultrafaceuint8.so and network_binary_ultraface.nb are derived from UltraFace model (source: https://github.com/Linzaer/Ultra-Light-Fast-Generic-Face-Detector-1MB/blob/master/models/onnx/version-RFB-320.onnx)."
HOMEPAGE = "https://github.com/nxp-imx/nxp-nnstreamer-examples"
LICENSE = "BSD-3-Clause & MIT"
LICENSE_LOCATION ?= "${S}/LICENSE"
LIC_FILES_CHKSUM = "file://${LICENSE_LOCATION};md5=df2d5c27ffc38b06ea00cd3edc2b4572"
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

RDEPENDS:${PN} += "bash python3-pip libnpu"

SRC_URI = "git://github.com/nxp-imx/nxp-nnstreamer-examples.git;branch=main;protocol=https"
SRCREV = "b8dafc13bb93b06826f9fc6b91056270cecc0fbd"
SRC_URI[sha256sum] = "bb1731a72bbe4136f37f905fcd50643ce7dc3528aed1de0fb1743883f0645006"

S = "${WORKDIR}/git"

inherit pkgconfig

SOC_NAME:hank= "stark"
SOC_NAME:stark = "stark"
SOC_NAME:kent = "kent"

SRC_URI:append = " \
	file://facerecognition/0001-Modify-face-detection-flow.patch \
	file://facerecognition/0002-Adopt-USB-camera-as-input-source.patch \
	file://facerecognition/0003-Modify-face-recognition-flow.patch \
	file://facerecognition/0004-Support-H264-USB-camera-and-add-more-input-options.patch \
	file://facerecognition/0005-Add-more-input-options-and-modification.patch \
	file://facerecognition/0006-Support-MJPEG-hw-decode.patch \
	file://facerecognition/0007-Add-pixel-format-into-input-option.patch \
	file://facerecognition/0008-Refine-pipeline.patch \
	file://facerecognition/ultrafaceuint8.so \
	file://facerecognition/facenetuint8.so \
	file://facerecognition/${SOC_NAME}/network_binary_facenet.nb \
	file://facerecognition/${SOC_NAME}/network_binary_ultraface.nb \
"

do_install() {
	install -d ${D}/root/ai-demo
	install -d ${D}/root/ai-demo/face-processing
	install -d ${D}/root/ai-demo/face-processing/sample
	cp -r ${S}/tasks/face-processing/common/ ${D}/root/ai-demo/face-processing/
	cp -r ${S}/tasks/face-processing/face-detection/ ${D}/root/ai-demo/face-processing/
	cp -r ${S}/tasks/face-processing/face-recogniton/ ${D}/root/ai-demo/face-processing/
	install -m 0755 ${WORKDIR}/facerecognition/ultrafaceuint8.so ${D}/root/ai-demo/face-processing/sample/
	install -m 0755 ${WORKDIR}/facerecognition/facenetuint8.so ${D}/root/ai-demo/face-processing/sample/
	install -m 0755 ${WORKDIR}/facerecognition/${SOC_NAME}/network_binary_facenet.nb ${D}/root/ai-demo/face-processing/sample/
	install -m 0755 ${WORKDIR}/facerecognition/${SOC_NAME}/network_binary_ultraface.nb ${D}/root/ai-demo/face-processing/sample/
}

FILES:${PN} += "/root/"
