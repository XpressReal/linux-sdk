#common functions for Realtek Avengers SoC
PREBUILT_DIR := "${THISDIR}/files/prebuilt/${@d.getVar('UBOOT_CONFIG').split('_')[0]}"
HWSETTING_DIR := "${PREBUILT_DIR}/hw_setting/000*"

SUFFIX = "'' '_4gb'"
SUFFIX:realtekevb-rtd1619b = "''"
SUFFIX:kent = "''"
DDR_TYPE = "_ddr4"
DDR_TYPE:backinblack-rtd1619b  = "_lpddr4"
DDR_TYPE:badassium-rtd1315c  = "_lpddr4"
DDR_TYPE:xpressreal-rtd1619b  = "_lpddr4"
DDR_TYPE:realtekevb-rtd1619b = "''"
DDR_TYPE:kent = "_lpddr4x"

DEV_KEY := "dev"
# Prefer prod.key to dev.key
DTE_FW_KEYS := "prod ${DEV_KEY}"
PROD_UBOOT_KEY := "uboot"
PROD_KERNEL_KEY := "kernel"
SECURE_BOOT_KEYS := "${PROD_KERNEL_KEY} ${PROD_UBOOT_KEY} ${DTE_FW_KEYS}"

HASH_ALG = "sha256"
HASH_ALG:kent = "sha512"
IV_KEY := "2dc2df39420321d0cef1fe2374029d95"
CIPHER_ALG = "aes-128-cbc"
CIPHER_ALG:kent = "aes-256-cbc"

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

do_compile:prepend() {
	if [ -d ${PREBUILT_DIR}/keys ]; then
		rm -rf ${B}/keys
		cp -a ${PREBUILT_DIR}/keys ${B}
		cd ${B}

		if [ -d keys/cipher ]; then
			mkimage -F -K keys/cipher/dummy.dtb -k keys/cipher ${DEPLOY_DIR_IMAGE}/yocto.itb
			dtc -I dtb -O dts -o ${S}/arch/arm/dts/cipher.dts keys/cipher/dummy.dtb
			sed -i "/dts-v1/a /plugin/;" ${S}/arch/arm/dts/cipher.dts
			if grep -q cipher ${S}/arch/arm/dts/cipher.dts; then
				sed -i "s/cipher/__overlay__/" ${S}/arch/arm/dts/cipher.dts
				sed -i '/__overlay__/i fragment@0 {\n target-path = "/cipher";' ${S}/arch/arm/dts/cipher.dts
				echo "};" >> ${S}/arch/arm/dts/cipher.dts
				sed -i '/load.*0x03[0,F][F,6]0000/a cipher {algo="aes256"; key-name-hint = "ubootkey";};' ${S}/board/realtek/rtd16*_*.its
			else
				rm -f keys/cipher/dummy.dtb
			fi
		fi

		for keyname in ${SECURE_BOOT_KEYS}; do
			[ -e keys/${keyname}.key ] && \
			openssl req -batch -new -x509 -key keys/${keyname}.key -out keys/${keyname}.crt
		done
		mkimage -F -K keys/dummy.dtb -k keys -r ${DEPLOY_DIR_IMAGE}/yocto.itb
		dtc -I dtb -O dts -o ${S}/arch/arm/dts/sign.dtsi keys/dummy.dtb
		sed -i "/dts-v1/d" ${S}/arch/arm/dts/sign.dtsi
		sed -i "s/conf/invalid/" ${S}/arch/arm/dts/sign.dtsi
		sed -i "/signature/a bootph-all;" ${S}/arch/arm/dts/sign.dtsi
		sed -i "/key-${DEV_KEY}/a bootph-all;" ${S}/arch/arm/dts/sign.dtsi
		sed -i "/key-${PROD_UBOOT_KEY}/a bootph-all;" ${S}/arch/arm/dts/sign.dtsi

		if [ -f keys/cipher/dummy.dtb ]; then
			cp --dereference keys/cipher/*.bin keys/
		fi
	fi
}

do_deploy:append() {
	cd ${DEPLOYDIR}
	for config in ${UBOOT_MACHINE}; do
		i=$(expr $i + 1);

		install -d ${B}/${config}/prebuilt
		cp -afL ${PREBUILT_DIR}/* ${B}/${config}/prebuilt/
		install -m 644 ${B}/${config}/${SPL_BINARY} ${B}/${config}/u-boot.img ${B}/${config}/prebuilt/

		for keyname in ${DTE_FW_KEYS}; do
		if [ -e ${B}/${config}/prebuilt/keys/cipher/${keyname}key.bin ] && \
			[ -e ${B}/keys/cipher/dummy.dtb ]; then
			openssl enc -e -${CIPHER_ALG} -iv ${IV_KEY} -K $(xxd -ps -c 32 ${B}/${config}/prebuilt/keys/cipher/${keyname}key.bin) -nopad -in ${B}/${config}/${SPL_BINARY} -out ${B}/${config}/prebuilt/$(basename ${SPL_BINARY})
			break;
		fi
		done
		for keyname in ${DTE_FW_KEYS}; do
		if [ -e ${B}/${config}/prebuilt/keys/${keyname}.key ]; then
			openssl dgst -${HASH_ALG} -binary ${B}/${config}/prebuilt/$(basename ${SPL_BINARY}) > ${B}/${config}/prebuilt/u-boot-spl.sha
			openssl rsautl -inkey ${B}/${config}/prebuilt/keys/${keyname}.key -sign -in ${B}/${config}/prebuilt/u-boot-spl.sha -out ${B}/${config}/prebuilt/u-boot-spl.sig
			${OBJCOPY} -I binary -O binary --reverse-bytes=256 ${B}/${config}/prebuilt/u-boot-spl.sig ${B}/${config}/prebuilt/u-boot-spl.sig
			break;
		fi
		done

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
					cp -af ${B}/${config}/*bind${suffix}.bin .
				done
			fi
		done
		unset j
	done
	unset i
	(mkdir -p hwsetting; cp -af ${HWSETTING_DIR}/* hwsetting/)
}

include u-boot-avengers-config.inc
