# Copyright (C) 2025, Realtek Semiconductor Corp.

SUMMARY = "Pose estimation Demo"
DESCRIPTION = "A demo application for pose estimation  using a pre-built binaries. \
The poseunit8.so and network_binary_posenet.nb are derived from posenet model \
(source : https://github.com/nnsuite/testcases/tree/master/DeepLearningModels/tensorflow-lite/pose_estimation)"
SECTION = "multimedia"
RDEPENDS:${PN} += "libnpu"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${WORKDIR}/poseestimation/posenet/LICENSE.txt;md5=3b83ef96387f14655fc854ddc3c6bd57"

SOC_NAME:hank= "stark"
SOC_NAME:stark = "stark"
SOC_NAME:kent = "kent"

SRC_URI = " \
	file://poseestimation/posenet/poseuint8.so \
	file://poseestimation/posenet/point_labels.txt \
	file://poseestimation/posenet/${SOC_NAME}/network_binary_posenet.nb \
	file://poseestimation/posenet/LICENSE.txt \
"

do_install() {
	install -d ${D}/root/ai-demo
	install -d ${D}/root/ai-demo/pose-estimation
	install -d ${D}/root/ai-demo/pose-estimation/sample
	install -d ${D}/root/ai-demo/pose-estimation/sample/posenet
	install -m 0755 ${WORKDIR}/poseestimation/posenet/poseuint8.so ${D}/root/ai-demo/pose-estimation/sample/posenet
	install -m 0755 ${WORKDIR}/poseestimation/posenet/point_labels.txt ${D}/root/ai-demo/pose-estimation/sample/posenet
	install -m 0755 ${WORKDIR}/poseestimation/posenet/${SOC_NAME}/network_binary_posenet.nb ${D}/root/ai-demo/pose-estimation/sample/posenet
}

FILES:${PN} += "/root/"
