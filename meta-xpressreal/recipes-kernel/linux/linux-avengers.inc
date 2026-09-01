#common functions for Realtek Avengers SoC

DEPENDS += " lzop-native"
DEPENDS += " lz4-native"
KERNEL_DTC_FLAGS += "-@"

RESCUE_MODULES_LIST = ""
RESCUE_MODULES_LIST:append:stark = " \
		drivers/remoteproc/rtk_fw_remoteproc.ko \
		drivers/gpu/drm/display/drm_display_helper.ko \
		drivers/gpu/drm/realtek/rtk_drm.ko \
		drivers/gpu/drm/drm_dma_helper.ko \
		drivers/soc/realtek/common/rtk_krpc_agent.ko \
		drivers/soc/realtek/common/rtk_rpc_mem.ko \
		drivers/rpmsg/rpmsg_rtk.ko \
		drivers/net/usb/r8152/r8152.ko \
		"

include linux-avengers-config.inc
include linux-avengers-nas-sf.inc

do_patch:prepend() {
	cp -af ${WORKDIR}/avengers-kmeta/bsp/* ${S}
	(cd ${S}; git add -A; git commit -m "Realtek Soc Patches")
}

# Compile device tree overlays (*.dts under dts/realtek/overlays) to .dtbo.
# The overlays directory only exists on kernels whose kmeta ships overlays
# (6.6 today), so everything is guarded to keep 5.10/6.12 builds working.
do_compile:append() {
    OVERLAYS_DIR="${STAGING_KERNEL_DIR}/arch/arm64/boot/dts/realtek/overlays"
    OVERLAYS_OUT="${B}/arch/arm64/boot/dts/realtek/overlays"

    if [ -d "${OVERLAYS_DIR}" ]; then
        mkdir -p ${OVERLAYS_OUT}
        for DTS_FILE in ${OVERLAYS_DIR}/*.dts; do
            [ -e "${DTS_FILE}" ] || continue
            dtbo_name="$(basename ${DTS_FILE%.dts}).dtbo"
            # same cpp pass the kernel runs on dts, so overlays can #include
            # <dt-bindings/...>
            ${KERNEL_CC} -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
                -I ${STAGING_KERNEL_DIR}/include \
                -I ${STAGING_KERNEL_DIR}/scripts/dtc/include-prefixes \
                -o ${OVERLAYS_OUT}/${dtbo_name}.pre.dts ${DTS_FILE}
            ${B}/scripts/dtc/dtc -@ -I dts -O dtb \
                -i ${OVERLAYS_DIR} \
                -o ${OVERLAYS_OUT}/${dtbo_name} \
                ${OVERLAYS_OUT}/${dtbo_name}.pre.dts
            rm -f ${OVERLAYS_OUT}/${dtbo_name}.pre.dts
        done
    fi
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

	for dtbo in ${B}/arch/arm64/boot/dts/realtek/overlays/*.dtbo; do
		[ -e "${dtbo}" ] || break
		install -d ${DEPLOYDIR}/${BOOTFILES_DIR}/overlays
		install -m 0644 "${dtbo}" ${DEPLOYDIR}/${BOOTFILES_DIR}/overlays/
	done

	lzop -f9 ${WORKDIR}/deploy-linux-yocto/Image
	mv ${WORKDIR}/deploy-linux-yocto/Image.lzo ${DEPLOYDIR}/${BOOTFILES_DIR}/kernel.lzo

	lz4 -f -12 --favor-decSpeed ${WORKDIR}/deploy-linux-yocto/Image ${DEPLOYDIR}/${BOOTFILES_DIR}/kernel.lz4

	SELECTED_DIR="${DEPLOY_DIR_IMAGE}/selected-kmods"
	mkdir -p "${SELECTED_DIR}"

	# Kernel version
	KVER=$(ls "${D}/usr/lib/modules/" | head -n 1)

	# --- Copy selected kernel modules ---
	for mod in ${RESCUE_MODULES_LIST}; do
		SRC="${D}/usr/lib/modules/${KVER}/kernel/${mod}"
		if [ -f "$SRC" ]; then
			cp -a "$SRC" "${SELECTED_DIR}/"
		else
			bbwarn "Module $SRC not found!"
		fi
	done
}
