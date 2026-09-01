# Copyright (C) 2023, Realtek Semiconductor Corp.

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

STATEFUL_URI = ""
STATELESS_URI = ""

STATEFUL_URI += " \
                file://rtd1619b/vfw_stateful/video_firmware.bin.enc \
                file://rtd1619b/vfw_stateful/coda988_fw.bin \
                "

STATELESS_URI += " \
                file://rtd1619b/vfw_stateless/video_firmware.bin.enc \
                file://rtd1619b/vfw_stateless/coda988_codec_fw.bin \
                "

STATEFUL_URI += " \
                file://rtd1625/vfw_stateful/video_firmware.bin.enc \
                file://rtd1625/vfw_stateful/boda955_fw.bin \
                file://rtd1625/vfw_stateful/seurat.bin \
		"

STATELESS_URI += " \
                file://rtd1625/vfw_stateless/video_firmware.bin.enc \
                file://rtd1625/vfw_stateless/boda955_codec_fw.bin \
		"

STATEFUL_URI += " \
		file://rtd1635/vfw_stateful/video_firmware.bin.enc \
                file://rtd1635/vfw_stateful/boda955_fw.bin \
		"

STATELESS_URI += " \
                file://rtd1635/vfw_stateless/video_firmware.bin.enc \
                file://rtd1635/vfw_stateless/boda955_codec_fw.bin \
                "

SRC_URI:append = " \
		file://rtd1619b/AFW_Certificate_final.bin \
		file://rtd1619b/bluecore.audio.enc.A00-stark-noaudio-nonsecure-0112 \
		file://rtd1619b/bluecore.audio.enc.A00-stark-noaudio-nonsecure-nohmdi \
		file://rtd1619b/HIFI.bin-stark-nonsecure-allcache \
		file://rtd1619b/bluecore.audio.enc.A00-stark-audio-nonsecure \
		file://rtd1619b/bluecore.audio.enc.A00-stark-audio-nonsecure-hdmi_swap \
		file://rtd1619b/ve3_entry.img \
		file://rtd1619b/VE3FW.bin \
		file://rtd1619b/sof/sof-adsp-rtd1619b.ri \
		file://rtd1619b/sof/sof-adsp-rtd1619b.tplg \
		file://rtd1619b/sof/sof-adsp-rtd1619b-rt5650.tplg \
		"

SRC_URI:append = " \
		file://rtd1625/RISC_V.bin \
		file://rtd1625/AFW_Certificate.bin \
		file://rtd1625/HIFI.bin \
		file://rtd1625/sof/sof-adsp-rtd1501s.ri \
		file://rtd1625/sof/sof-adsp-rtd1501s.tplg \
		"

SRC_URI:append = " \
		file://rtd1635/AFW_Certificate.bin \
		file://rtd1635/HIFI.bin \
		file://rtd1635/ve3_entry.img \
		file://rtd1635/VE3FW.bin \
		file://rtd1635/DLSR_Param.bin \
		"

RESCUE_FIRMWARE_LIST = ""
RESCUE_FIRMWARE_LIST:append:stark = " \
		rtd1619b/AFW_Certificate_final.bin \
		rtd1619b/bluecore.audio.enc.A00-stark-noaudio-nonsecure-0112 \
		rtd1619b/bluecore.audio.enc.A00-stark-noaudio-nonsecure-nohmdi \
		"

include linux-firmware.inc

SRC_URI:append = "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', '${STATELESS_URI}', '${STATEFUL_URI}', d)}"

VFW_FOLDER = "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', 'vfw_stateless', 'vfw_stateful', d)}"

# Install addition firmwares
do_install:append() {
	install -d ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b
	install -d ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/sof
	install -d ${D}${nonarch_base_libdir}/firmware/rtw89
	install -d ${D}${nonarch_base_libdir}/firmware/rtl_bt
	install -m 0644 ${WORKDIR}/rtd1619b/AFW_Certificate_final.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/bluecore.audio.enc.A00-stark-noaudio-nonsecure-0112 ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/HIFI.bin-stark-nonsecure-allcache ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/bluecore.audio.enc.A00-stark-audio-nonsecure ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/bluecore.audio.enc.A00-stark-audio-nonsecure-hdmi_swap ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/	
	install -m 0644 ${WORKDIR}/rtd1619b/bluecore.audio.enc.A00-stark-noaudio-nonsecure-nohmdi ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/${VFW_FOLDER}/* ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/ve3_entry.img ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/VE3FW.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/sof/* ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/sof/
	install -m 0644 ${B}/rtw89/* ${D}${nonarch_base_libdir}/firmware/rtw89/
	install -m 0644 ${B}/rtl_bt/* ${D}${nonarch_base_libdir}/firmware/rtl_bt/

	SELECTED_DIR="${DEPLOY_DIR_IMAGE}/selected-firmware"
	mkdir -p "${SELECTED_DIR}"

	for fw in ${RESCUE_FIRMWARE_LIST}; do
		SRC_FW="${WORKDIR}/${fw}"
		if [ -f "$SRC_FW" ]; then
			install -m 0644 -D "$SRC_FW" "${SELECTED_DIR}/${fw}"
		else
			bbwarn "Firmware $SRC_FW not found!"
		fi
	done

	install -d ${D}${nonarch_base_libdir}/firmware/realtek/rtd1625
	install -d ${D}${nonarch_base_libdir}/firmware/realtek/rtd1625/sof
	install -m 0644 ${WORKDIR}/rtd1625/RISC_V.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1625/
	install -m 0644 ${WORKDIR}/rtd1625/AFW_Certificate.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1625/
	install -m 0644 ${WORKDIR}/rtd1625/HIFI.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1625/
	install -m 0644 ${WORKDIR}/rtd1625/${VFW_FOLDER}/* ${D}${nonarch_base_libdir}/firmware/realtek/rtd1625/
	install -m 0644 ${WORKDIR}/rtd1625/sof/* ${D}${nonarch_base_libdir}/firmware/realtek/rtd1625/sof/

	install -d ${D}${nonarch_base_libdir}/firmware/realtek/rtd1635
	install -m 0644 ${WORKDIR}/rtd1635/AFW_Certificate.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1635/
	install -m 0644 ${WORKDIR}/rtd1635/HIFI.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1635/
	install -m 0644 ${WORKDIR}/rtd1635/ve3_entry.img ${D}${nonarch_base_libdir}/firmware/realtek/rtd1635/
	install -m 0644 ${WORKDIR}/rtd1635/VE3FW.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1635/
	install -m 0644 ${WORKDIR}/rtd1635/${VFW_FOLDER}/* ${D}${nonarch_base_libdir}/firmware/realtek/rtd1635/
	install -m 0644 ${WORKDIR}/rtd1635/DLSR_Param.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1635/
}

FILES:${PN}-rtd1619b = " ${nonarch_base_libdir}/firmware/realtek/rtd1619b/"
FILES:${PN}-rtd1625 = " ${nonarch_base_libdir}/firmware/realtek/rtd1625/"
FILES:${PN}-rtd1635 = " ${nonarch_base_libdir}/firmware/realtek/rtd1635/"
FILES:${PN}-rtl8852  = " ${nonarch_base_libdir}/firmware/rtw89/"
FILES:${PN}-rtlbt  = " ${nonarch_base_libdir}/firmware/rtl_bt/"

PACKAGES =+ "${PN}-rtd1619b"
PACKAGES =+ "${PN}-rtd1625"
PACKAGES =+ "${PN}-rtd1635"
PACKAGES =+ "${PN}-rtl8852"
PACKAGES =+ "${PN}-rtlbt"
PACKAGES =+ "${PN}-rtw"

ALLOW_EMPTY:${PN}-rtw = "1"
RDEPENDS:${PN}-rtw = " \
	${PN}-rtl8821 \
	"
