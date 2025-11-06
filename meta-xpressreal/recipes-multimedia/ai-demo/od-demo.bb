# Copyright (C) 2025, Realtek Semiconductor Corp.

SUMMARY = "Object Detection Demo"
DESCRIPTION = "A demo application for object detection using a pre-built binaries. \
The ssdmobilenetv2cocouint8.so and network_binary_ssdmobilenet.nb are derived from ssd_mobilenet_v2_coco model \
(source : https://github.com/nnsuite/testcases/tree/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco)"
SECTION = "multimedia"
RDEPENDS:${PN} += "libnpu"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${WORKDIR}/objectdetect/ssdmobilenet/LICENSE.txt;md5=6f798069926aa738ee3bbbcac6c62a2f"

SOC_NAME:hank= "stark"
SOC_NAME:stark = "stark"
SOC_NAME:kent = "kent"

SRC_URI = " \
	file://objectdetect/ssdmobilenet/ssdmobilenetv2cocouint8.so \
	file://objectdetect/ssdmobilenet/${SOC_NAME}/network_binary_ssdmobilenet.nb \
	file://objectdetect/ssdmobilenet/coco_labels_list.txt \
	file://objectdetect/ssdmobilenet/box_priors.txt \
	file://objectdetect/ssdmobilenet/LICENSE.txt \
"

do_install() {
	install -d ${D}/root/ai-demo
	install -d ${D}/root/ai-demo/object-detection
	install -d ${D}/root/ai-demo/object-detection/sample
	install -d ${D}/root/ai-demo/object-detection/sample/ssdmobilenet
	install -m 0755 ${WORKDIR}/objectdetect/ssdmobilenet/ssdmobilenetv2cocouint8.so ${D}/root/ai-demo/object-detection/sample/ssdmobilenet
	install -m 0755 ${WORKDIR}/objectdetect/ssdmobilenet/${SOC_NAME}/network_binary_ssdmobilenet.nb ${D}/root/ai-demo/object-detection/sample/ssdmobilenet
	install -m 0755 ${WORKDIR}/objectdetect/ssdmobilenet/coco_labels_list.txt ${D}/root/ai-demo/object-detection/sample/ssdmobilenet
	install -m 0755 ${WORKDIR}/objectdetect/ssdmobilenet/box_priors.txt ${D}/root/ai-demo/object-detection/sample/ssdmobilenet
}

FILES:${PN} += "/root/"
