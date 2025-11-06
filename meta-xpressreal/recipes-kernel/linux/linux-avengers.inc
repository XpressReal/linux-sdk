#common functions for Realtek Avengers SoC

DEPENDS += " lzop-native"

include linux-avengers-config.inc
include linux-avengers-nas-sf.inc

do_patch:prepend() {
	cp -af ${WORKDIR}/avengers-kmeta/bsp/* ${S}
	(cd ${S}; git add -A; git commit -m "Realtek Soc Patches")
}

do_deploy:append() {

	install -d ${DEPLOYDIR}/${BOOTFILES_DIR}

	for DTS_FILE in ${KERNEL_DEVICETREE}
	do
	    i=$(expr $i + 1);
	    if echo "${DTS_FILE}" | grep -q "rescue"; then
	        cp ${WORKDIR}/deploy-linux-yocto/$(basename ${DTS_FILE}) ${DEPLOYDIR}/${BOOTFILES_DIR}/rescue.dtb
	    elif echo "${DTS_FILE}" | grep -q "spi"; then
	        cp ${WORKDIR}/deploy-linux-yocto/$(basename ${DTS_FILE}) ${DEPLOYDIR}/${BOOTFILES_DIR}/spi.dtb
	    else
	        cp ${WORKDIR}/deploy-linux-yocto/$(basename ${DTS_FILE}) ${DEPLOYDIR}/${BOOTFILES_DIR}/
	    fi
	    if [ $i -eq 1 ]; then
	        cp ${WORKDIR}/deploy-linux-yocto/$(basename ${DTS_FILE}) ${DEPLOYDIR}/${BOOTFILES_DIR}/boot.dtb
	    fi
	done
	unset i

	lzop -f9 ${WORKDIR}/deploy-linux-yocto/Image
	mv ${WORKDIR}/deploy-linux-yocto/Image.lzo ${DEPLOYDIR}/${BOOTFILES_DIR}/kernel.lzo
}
