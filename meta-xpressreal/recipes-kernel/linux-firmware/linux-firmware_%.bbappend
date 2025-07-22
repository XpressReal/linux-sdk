# Copyright (C) 2023, Realtek Semiconductor Corp.

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

STATEFUL_URI = " \
                file://rtd1619b/vfw_stateful/video_firmware.bin.enc \
                "

STATELESS_URI = " \
                file://rtd1619b/vfw_stateless/video_firmware.bin.enc \
                file://rtd1619b/vfw_stateless/coda988_codec_fw.bin \
                "

SRC_URI:append = " \
		file://rtd1619b/AFW_Certificate_final.bin \
		file://rtd1619b/bluecore.audio.enc.A00-stark-noaudio-nonsecure-0112 \
		file://rtd1619b/HIFI.bin-stark-nonsecure-allcache \
		file://rtd1619b/bluecore.audio.enc.A00-stark-audio-nonsecure \
		file://rtd1619b/ve3_entry.img \
		file://rtd1619b/VE3FW.bin \
		file://aic8800/aic_powerlimit_8800d80.txt \
		file://aic8800/aic_userconfig_8800d80.txt \
		file://aic8800/fmacfw_8800d80_h_u02.bin \
		file://aic8800/fmacfw_8800d80_h_u02_ipc.bin \
		file://aic8800/fmacfw_8800d80_u02.bin \
		file://aic8800/fmacfw_8800d80_u02_ipc.bin \
		file://aic8800/fmacfwbt_8800d80_h_u02.bin \
		file://aic8800/fmacfwbt_8800d80_u02.bin \
		file://aic8800/fw_adid_8800d80_u02.bin \
		file://aic8800/fw_patch_8800d80_u02.bin \
		file://aic8800/fw_patch_8800d80_u02_ext0.bin \
		file://aic8800/fw_patch_table_8800d80_u02.bin \
		file://aic8800/lmacfw_rf_8800d80_u02.bin \
		"
SRC_URI:append = "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', '${STATELESS_URI}', '${STATEFUL_URI}', d)}"

VFW_FOLDER = "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', 'vfw_stateless', 'vfw_stateful', d)}"

# Install addition firmwares
do_install:append() {
	install -d ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b
	install -d ${D}${nonarch_base_libdir}/firmware/rtw89
	install -d ${D}${nonarch_base_libdir}/firmware/aic8800
	install -m 0644 ${WORKDIR}/rtd1619b/AFW_Certificate_final.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/bluecore.audio.enc.A00-stark-noaudio-nonsecure-0112 ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/HIFI.bin-stark-nonsecure-allcache ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/bluecore.audio.enc.A00-stark-audio-nonsecure ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/${VFW_FOLDER}/* ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/ve3_entry.img ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${WORKDIR}/rtd1619b/VE3FW.bin ${D}${nonarch_base_libdir}/firmware/realtek/rtd1619b/
	install -m 0644 ${B}/rtw89/* ${D}${nonarch_base_libdir}/firmware/rtw89/
	install -m 0644 ${WORKDIR}/aic8800/* ${D}${nonarch_base_libdir}/firmware/aic8800/
}

FILES:${PN}-rtd1619b = " ${nonarch_base_libdir}/firmware/realtek/rtd1619b/"
FILES:${PN}-rtl8852  = " ${nonarch_base_libdir}/firmware/rtw89/"
FILES:${PN}-aic8800 = " ${nonarch_base_libdir}/firmware/aic8800/"

PACKAGES =+ "${PN}-rtd1619b"
PACKAGES =+ "${PN}-rtl8852"
PACKAGES =+ "${PN}-aic8800"
