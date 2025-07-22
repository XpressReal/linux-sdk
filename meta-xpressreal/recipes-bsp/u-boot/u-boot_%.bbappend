FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_DIR := "${THISDIR}/files/src"
PREBUILT_DIR := "${THISDIR}/files/prebuilt/${@d.getVar('UBOOT_CONFIG').split('_')[0]}"
HWSETTING_DIR := "${PREBUILT_DIR}/hw_setting/000*"

SUFFIX = "'' '_4gb'"
SUFFIX:realtekevb-rtd1619b = "''"
DDR_TYPE = "_lpddr4"
DDR_TYPE:backinblack-rtd1619b  = "_lpddr4"
DDR_TYPE:realtekevb-rtd1619b = "''"

# External hwsettings selected by GPIO
# - bit0: GPIO_74(0x4a), bit1: GPIO_72(0x48)
# - 0: backinblack 4GB: LPDDR4_32Gb_ddp_s2666
# - 1: bleedingedge 4GB: 2DDR4_16Gb_sdp_s2666
# - 2: backinblack 2GB: LPDDR4_16Gb_s2666
# - 3: bleedingedge 2GB: 2DDR4_8Gb_s2666
EXT_HWSETTINGS:realtekevb-rtd1619b:= " \
	scs_param/0000/SCS_Params_Area_RTD1619B_hwsetting_BOOT_LPDDR4_32Gb_ddp_s2666.bin \
	scs_param/0000/SCS_Params_Area_RTD1619B_hwsetting_BOOT_2DDR4_16Gb_sdp_s2666.bin \
	scs_param/0000/SCS_Params_Area_RTD1619B_hwsetting_BOOT_LPDDR4_16Gb_s2666.bin \
	scs_param/0000/SCS_Params_Area_RTD1619B_hwsetting_BOOT_2DDR4_8Gb_s2666.bin \
	"

DEPENDS += "u-boot-mkimage-native xxd-native lzop-native"

SRC_URI:append = " \
	file://patches/0001-boot-Only-define-checksum-algos-when-the-hashes-are-.patch \
	file://patches/0010-build-arm-Add-mach-realtek.patch \
	file://patches/0011-abortboot-detect-TAB-key-to-load-altbootcmd-for-rescue.patch \
	file://patches/0013-include-common.h-Add-debug-print-macro-and-block-dev.patch \
	file://patches/0024-armv8-start.S-Skip-lowlevel_init-on-EL2-and-EL1.patch \
	file://patches/0030-common-board_r.c-no-relocation.patch \
	file://patches/0037-drivers-mmc-Add-RTK_MMC_DRIVER.patch \
	file://patches/0038-drivers-usb-add-realtek-platform-usb-and-realtek-usb.patch \
	file://patches/0041-drivers-net-Add-RTL8168.patch \
	file://patches/0042-drivers-i2c-Add-rtk_i2c.patch \
	file://patches/0043-drivers-gpio-Add-rt_gpio.patch \
	file://patches/0044-drivers-Add-SPI_RTK_SFC.patch \
	file://patches/0047-lib-lzma-Skip-uncompressedSize-check-if-not-set.patch \
	file://patches/0060-common-Add-PMIC-fss-scan-v2-and-BIST-Shmoo-volt.patch \
	file://patches/903-arm-enable-ARM_SMCCC-without-ARM_PSCI_FW.patch \
	file://patches/904-tools-binman-replace-update-current-imagefile.patch \
	file://patches/910-fit-add-verify-on-image-load.patch \
	file://patches/911-Makefile-Signed-configurations-on-U-Boot-fitImage.patch \
	file://patches/912-spl-Makefile.spl-spl-with-padding.patch \
	file://patches/913-spl-Makefile.spl-usb-dwc3-without-gadget.patch \
	file://patches/R0001-net-Remove-eth_legacy.c.patch \
	file://patches/R0002-net-Make-DM_ETH-be-selected-by-NETDEVICE.patch \
	"

ERROR_QA:remove = "patch-status"

do_src_copy() {
	cp -af ${SRC_DIR}/* ${S}
	(cd ${S}; git add -A; git commit -m "Realtek Soc Patches")
}

do_compile:prepend() {
	if [ -d ${PREBUILT_DIR}/keys ]; then
		rm -rf ${B}/keys
		cp -a ${PREBUILT_DIR}/keys ${B}
		cd ${B}
		openssl req -batch -new -x509 -key keys/dev.key -out keys/dev.crt
		mkimage -F -K keys/dummy.dtb -k keys -r ${DEPLOY_DIR_IMAGE}/yocto.itb
		dtc -I dtb -O dts -o ${S}/arch/arm/dts/sign.dtsi keys/dummy.dtb
		sed -i "/dts-v1/d" ${S}/arch/arm/dts/sign.dtsi
		sed -i "s/signature/signature: signature/" ${S}/arch/arm/dts/sign.dtsi
		sed -i "s/key-dev/key_dev: key-dev/" ${S}/arch/arm/dts/sign.dtsi
		sed -i "s/conf/invalid/" ${S}/arch/arm/dts/sign.dtsi
	fi
}

do_deploy:append() {
	cd ${DEPLOYDIR}
	for config in ${UBOOT_MACHINE}; do
		i=$(expr $i + 1);

		install -d ${B}/${config}/prebuilt
		cp -af ${PREBUILT_DIR}/* ${B}/${config}/prebuilt/
		install -m 644 ${B}/${config}/${SPL_BINARY} ${B}/${config}/u-boot.img ${B}/${config}/prebuilt/

		if [ -e ${B}/${config}/prebuilt/keys/dev.key ]; then
			openssl dgst -sha256 -binary ${B}/${config}/${SPL_BINARY} > ${B}/${config}/prebuilt/u-boot-spl.sha
			openssl rsautl -inkey ${B}/${config}/prebuilt/keys/dev.key -sign -in ${B}/${config}/prebuilt/u-boot-spl.sha -out ${B}/${config}/prebuilt/u-boot-spl.sig
			${OBJCOPY} -I binary -O binary --reverse-bytes=256 ${B}/${config}/prebuilt/u-boot-spl.sig ${B}/${config}/prebuilt/u-boot-spl.sig
		fi

		for type in ${UBOOT_CONFIG}; do
			j=$(expr $j + 1);
			if [ $j -eq $i ]; then
				for suffix in ${SUFFIX}; do
					[ -e ${B}/${config}/prebuilt/${type}${DDR_TYPE}${suffix}.dts ] || continue
					k=0
					for exthw in ${EXT_HWSETTINGS}; do
						dd if=${B}/${config}/prebuilt/${exthw} of=${B}/${config}/prebuilt/ext_hwsetting_$k.bin bs=1 skip=8 count=8192
						k=$(expr $k + 1);
					done
					unset k
					${BUILD_CPP} \
						-nostdinc -undef -D__DTS__ -x assembler-with-cpp \
						-o ${B}/${config}/prebuilt/${type}${suffix}.pp \
						${B}/${config}/prebuilt/${type}${DDR_TYPE}${suffix}.dts
					dtc -I dts -O dtb -o ${B}/${config}/prebuilt/${type}${suffix}.dtb ${B}/${config}/prebuilt/${type}${suffix}.pp
					(cd ${B}/${config} && ${S}/tools/binman/binman build --update-fdt -I ./prebuilt --dt ./prebuilt/${type}${suffix}.dtb -O ./)
					cp -af ${B}/${config}/bind${suffix}.bin ${type}_bind${suffix}.bin
				done
			fi
		done
		unset j
	done
	unset i
	(mkdir -p hwsetting; cp -af ${HWSETTING_DIR}/* hwsetting/)
}

do_compile[depends] = "bootfiles:do_deploy"

addtask src_copy before do_patch after do_unpack
