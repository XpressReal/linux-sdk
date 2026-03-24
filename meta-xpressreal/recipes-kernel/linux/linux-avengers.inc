#common functions for Realtek Avengers SoC

DEPENDS += " lzop-native"
DEPENDS += " lz4-native"

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
	    fi
	    if [ $i -eq 1 ]; then
	        cp ${WORKDIR}/deploy-linux-yocto/$(basename ${DTS_FILE}) ${DEPLOYDIR}/${BOOTFILES_DIR}/boot.dtb
	    fi
	    cp ${WORKDIR}/deploy-linux-yocto/$(basename ${DTS_FILE}) ${DEPLOYDIR}/${BOOTFILES_DIR}/
	done
	unset i

	lzop -f9 ${WORKDIR}/deploy-linux-yocto/Image
	mv ${WORKDIR}/deploy-linux-yocto/Image.lzo ${DEPLOYDIR}/${BOOTFILES_DIR}/kernel.lzo

	lz4 -f -12 --favor-decSpeed ${WORKDIR}/deploy-linux-yocto/Image ${DEPLOYDIR}/${BOOTFILES_DIR}/kernel.lz4

	SELECTED_DIR="${DEPLOY_DIR_IMAGE}/selected-kmods"
	mkdir -p "${SELECTED_DIR}"

	# Kernel version
	KVER=$(ls "${D}/usr/lib/modules/" | head -n 1)

	# --- Copy selected kernel modules ---
	MODULES_LIST="
		drivers/remoteproc/rtk_fw_remoteproc.ko
		drivers/gpu/drm/display/drm_display_helper.ko
		drivers/gpu/drm/realtek/rtk_drm.ko
		drivers/gpu/drm/drm_dma_helper.ko
		drivers/soc/realtek/common/rtk_krpc_agent.ko
		drivers/soc/realtek/common/rtk_rpc_mem.ko
		drivers/rpmsg/rpmsg_rtk.ko
		"

	for mod in $MODULES_LIST; do
		SRC="${D}/usr/lib/modules/${KVER}/kernel/${mod}"
		if [ -f "$SRC" ]; then
			cp -a "$SRC" "${SELECTED_DIR}/"
		else
			bbwarn "Module $SRC not found!"
		fi
	done
}
