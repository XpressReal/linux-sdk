FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_DIR := "${THISDIR}/files/src"

require u-boot-avengers.inc

# uboot-config.bbclass:147 eager-expands ${B} into KCONFIG_CONFIG_ROOTDIR at anon-func
# time, BEFORE externalsrc (devtool) repoints ${B} to ${WORKDIR}/${BPN}-${PV}. That freezes
# a stale ".../build/<config>" path, so the devtool do_configure snapshot later fails with
# "cp: cannot stat '.../build/<config>/.config'". Re-assert it lazily (keep ${B} unexpanded)
# so it tracks the live ${B} at task time. This MUST be an anonymous python func: a plain '='
# or :forcevariable is applied at parse/expandKeys and then clobbered by the class's later
# anon setVar. Our append anon is registered after uboot-config's, so it wins.
python () {
    machine = (d.getVar('UBOOT_MACHINE') or '').strip()
    # Only single-config builds set/use KCONFIG_CONFIG_ROOTDIR (multi-config disables menuconfig).
    if machine and len(machine.split()) == 1:
        d.setVar('KCONFIG_CONFIG_ROOTDIR', '${B}/' + machine)
}

DEPENDS += "u-boot-mkimage-native xxd-native lzop-native"

SRC_URI:append = " \
	file://patches/0000-tiny-printf-Correct-return-values.patch \
	file://patches/CVE-2026-33243.patch \
	file://patches/0001-boot-Only-define-checksum-algos-when-the-hashes-are-.patch \
	file://patches/0002-env_spi-support-overriding-spi-dev-from-board-code.patch \
	file://patches/0002-env-mmc-Make-redundant-env-in-both-eMMC-boot-partiti.patch \
	file://patches/0003-usb-dwc3-gadget-fix-crash-in-dwc3_gadget_giveback.patch \
	file://patches/0004-usb-dwc3-invalidate-dcache-on-buffer-used-in-interru.patch \
	file://patches/0005-01-android-Fix-ramdisk-loading-for-bootimage-v3.patch \
	file://patches/0005-02-boot-android-Provide-vendor_bootimg_addr-in-boot_get.patch \
	file://patches/0005-03-bootstd-Add-a-bootmeth-for-Android.patch \
	file://patches/0005-04-boot-android-fix-booting-without-a-ramdisk.patch \
	file://patches/0005-05-bootstd-android-Add-U-Boot-version-to-cmdline.patch \
	file://patches/0005-06-boot-android-Fix-ramdisk-loading-for-v2-header.patch \
	file://patches/0005-07-image-android-use-ulong-for-kernel-address.patch \
	file://patches/0005-08-image-android-do-not-boot-XIP-when-kernel-is-compres.patch \
	file://patches/0005-09-image-android-handle-ramdisk-default-address.patch \
	file://patches/0005-10-bootstd-android-add-support-of-bootimage-v2.patch \
	file://patches/0005-11-bootstd-android-add-non-A-B-image-support.patch \
	file://patches/0005-12-bootstd-android-don-t-read-whole-partition-sizes.patch \
	file://patches/0005-13-boot-android-fix-extra-command-line-support.patch \
	file://patches/0005-14-boot-android-free-newbootargs-when-done.patch \
	file://patches/0005-15-boot-android-rework-bootargs-concatenation.patch \
	file://patches/0005-16-boot-android-Check-kcmdline-s-for-NULL-in-android_im.patch \
	file://patches/0005-17-bootstd-android-Add-missing-NULL-in-the-avb-partitio.patch \
	file://patches/0005-18-bootstd-android-Allow-boot-with-AVB-failures-when-un.patch \
	file://patches/0005-19-boot-android-handle-boot-images-with-missing-DTB.patch \
	file://patches/0005-20-bootstd-android-avoid-possible-null-pointer-derefere.patch \
	file://patches/0005-21-image-android-fix-ramdisk-default-address.patch \
	file://patches/0005-22-boot-image-android-Workaround-kernel-ramdisk-invalid.patch \
	file://patches/0005-23-boot-android-Prevent-use-of-unintialised-variable.patch \
	file://patches/0005-24-android-boot-fix-wrong-end-of-header-in-v3-v4-parsin.patch \
	file://patches/0005-25-bootstd-android-add-the-bootargs-env-to-the-commandl.patch \
	file://patches/0005-26-abootimg-Add-init_boot-image-support.patch \
	file://patches/0005-27-android-boot-Add-set_abootimg_addr-and-set_avendor_b.patch \
	file://patches/0005-28-bootstd-Add-bootflow_iter_check_mmc-helper.patch \
	file://patches/0005-29-common-avb_verify-don-t-call-mmc_switch_part-for-SD.patch \
	file://patches/0005-30-common-avb_verify-rework-error-debug-prints.patch \
	file://patches/0005-31-common-avb_verify-add-str_avb_io_error-str_avb_slot_.patch \
	file://patches/0005-32-cmd-bcb-support-various-block-device-interfaces-for-.patch \
	file://patches/0005-33-cmd-bcb-extend-BCB-C-API-to-allow-read-write-the-fie.patch \
	file://patches/0005-34-cmd-bcb-Fix-segfault-on-invalid-block-device.patch \
	file://patches/0005-35-cmd-avb-rework-prints.patch \
	file://patches/0005-36-cmd-avb-rework-do_avb_verify_part.patch \
	file://patches/0005-37-boot-android-Always-use-8-byte-aligned-DT-with-libfd.patch \
	file://patches/0005-38-boot-android-import-addBootConfigParameters-from-AOS.patch \
	file://patches/0005-39-boot-android-Add-bootconfig-support.patch \
	file://patches/0006-01-env-Introduce-support-for-MTD.patch \
	file://patches/0006-01-env-mtd-fix-usability-with-NAND-flashes.patch \
	file://patches/0006-01-env-mtd-add-the-missing-put_mtd_device.patch \
	file://patches/0006-02-mtd-spinand-add-support-for-ESMT-F50x1G41LB.patch \
	file://patches/0006-03-mtd-spinand-Add-support-for-XTX-SPINAND.patch \
	file://patches/0006-04-01-cmd-mtd-OTP-access-support.patch \
	file://patches/0006-04-02-cmd-mtd-Enable-speed-benchmarking.patch \
	file://patches/0006-04-03-cmd-mtd-add-markbad-command-support.patch \
	file://patches/0006-04-04-cmd-mtd-add-nand_write_test-command-support.patch \
	file://patches/0006-04-05-cmd-mtd-add-nand_read_test-command-support.patch \
	file://patches/0006-04-06-cmd-mtd-fix-speed-measurement-in-the-speed-benchmark.patch \
	file://patches/0006-04-07-cmd-mtd-add-benchmark-option-to-the-help.patch \
	file://patches/0006-05-01-disk-support-MTD-partitions.patch \
	file://patches/0006-05-02-drivers-introduce-mtdblock-abstraction.patch \
	file://patches/0006-05-03-spinand-bind-mtdblock.patch \
	file://patches/0006-05-04-ubi-allow-to-read-from-volume-with-offset.patch \
	file://patches/0006-05-05-ubi-allow-to-write-to-volume-with-offset.patch \
	file://patches/0006-05-06-drivers-introduce-UBI-block-abstraction.patch \
	file://patches/0006-05-07-disk-don-t-try-search-for-partition-type-if-already-.patch \
	file://patches/0006-05-08-disk-support-UBI-partitions.patch \
	file://patches/0006-05-09-spinand-bind-UBI-block.patch \
	file://patches/0006-05-10-mtd-mtdpart-fix-partitions-searching.patch \
	file://patches/0006-09-spi-Introduce-setup_for_spinand.patch \
	file://patches/0006-10-spinand-call-SPI-setup_for_spinand-if-supported.patch \
	file://patches/0006-11-mtd-spinand-winbond-add-Winbond-W25N04KV-flash-suppo.patch \
	file://patches/0006-12-mtd-spinand-gigadevice-sync-supported-chips-with-lin.patch \
	file://patches/0006-13-spi-spi-mem-Extend-SPI-MEM-ops-to-match-Linux-6.16.patch \
	file://patches/0006-14-mtd-spinand-Use-the-spi-mem-dirmap-API.patch \
	file://patches/0006-15-mtd-nand-Add-a-NAND-page-I-O-request-type.patch \
	file://patches/0006-16-mtd-spinand-core-add-missing-MODULE_DEVICE_TABLE.patch \
	file://patches/0006-17-mtd-spinand-Extend-spinand_wait-to-match-Linux-kerne.patch \
	file://patches/0006-18-mtd-spinand-Make-use-of-spinand_to_-mtd-nand-helpers.patch \
	file://patches/0006-19-mtd-spinand-Align-logic-for-enabling-ECC-to-match-Li.patch \
	file://patches/0006-20-mtd-spinand-Refactor-spinand_init-functions.patch \
	file://patches/0006-21-mtd-spinand-Refactor-ECC-OOB-functions.patch \
	file://patches/0006-22-mtd-spinand-Sync-core-code-and-device-support-with-L.patch \
	file://patches/0006-23-mtd-spinand-set-bitflip_threshold-to-75-of-ECC-stren.patch \
	file://patches/0006-24-mtd-spinand-Add-support-for-setting-plane-select-bit.patch \
	file://patches/0006-25-mtd-spinand-Remove-write_enable_op-in-markbad.patch \
	file://patches/0006-26-mtd-spinand-Introduce-a-way-to-avoid-raw-access.patch \
	file://patches/0006-27-mtd-spinand-add-support-of-continuous-reading-mode.patch \
	file://patches/0006-28-mtd-spinand-Add-read-retry-support.patch \
	file://patches/0006-29-mtd-spinand-add-OTP-support.patch \
	file://patches/0006-30-mtd-spinand-Enhance-the-logic-when-picking-a-variant.patch \
	file://patches/0006-31-mtd-spinand-propagate-spinand_wait-errors-from-spina.patch \
	file://patches/0006-32-mtd-spinand-Add-a-configure_chip-hook.patch \
	file://patches/0006-33-mtd-spinand-Sync-core-code-and-device-support-with-L.patch \
	file://patches/0006-34-mtd-spinand-fix-direct-mapping-creation-sizes.patch \
	file://patches/0006-35-mtd-spinand-try-a-regular-dirmap-if-creating-a-dirma.patch \
	file://patches/0006-36-mtd-spinand-repeat-reading-in-regular-mode-if-contin.patch \
	file://patches/0006-37-mtd-spinand-add-support-for-FudanMicro-FM25S01A.patch \
	file://patches/0007-00-mtd-nand-Rename-a-core-structure.patch \
	file://patches/0007-01-mtd-nand-Introduce-the-ECC-engine-framework.patch \
	file://patches/0007-02-mtd-nand-Use-the-new-generic-ECC-object.patch \
	file://patches/0007-03-mtd-nand-ecc-Add-an-I-O-request-tweaking-mechanism.patch \
	file://patches/0007-04-mtd-spinand-Instantiate-a-SPI-NAND-on-die-ECC-engine.patch \
	file://patches/0007-05-mtd-nand-Let-on-die-ECC-engines-be-retrieved-from-th.patch \
	file://patches/0007-06-mtd-spinand-Fill-a-default-ECC-provider-algorithm.patch \
	file://patches/0007-07-mtd-nand-Add-helpers-to-manage-ECC-engines-and-confi.patch \
	file://patches/0007-08-mtd-spinand-Use-the-external-ECC-engine-logic.patch \
	file://patches/0007-09-mtd-spinand-Allow-the-case-where-there-is-no-ECC-eng.patch \
	file://patches/0007-10-mtd-spinand-Fix-OOB-read.patch \
	file://patches/0007-11-mtd-spinand-Fix-MTD_OPS_AUTO_OOB-requests.patch \
	file://patches/0007-12-mtd-nand-ecc-Add-infrastructure-to-support-hardware-.patch \
	file://patches/0007-13-mtd-spinand-Create-direct-mapping-descriptors-for-EC.patch \
	file://patches/0008-spl-Add-generic-spl_load-function.patch \
	file://patches/0009-spl-semihosting-Don-t-close-fd-before-spl_load_simpl.patch \
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
	file://patches/0045-FEATURE-usb-dwc3-dwc3-generic-add-kent-usb-support.patch \
	file://patches/0046-usb-gadget-mass_storage-add-super-speed-support.patch \
	file://patches/0047-lib-lzma-Skip-uncompressedSize-check-if-not-set.patch \
	file://patches/0048-drivers-pwm-Add-pwm-rtk.patch \
	file://patches/0060-common-Add-PMIC-fss-scan-v2-and-BIST-Shmoo-volt.patch \
	file://patches/0062-usb-add-delay-when-port-reset-and-get-trb.patch \
	file://patches/0063-usb-dwc3-gadget-fix-max-packet-size-for-superspeed.patch \
	file://patches/0066-common-Add-command-for-s5.patch \
	file://patches/0067-drivers-clk-Add-clk-rtk.patch \
	file://patches/0068-drivers-i2c-Add-i2c-rtk.patch \
	file://patches/0069-drivers-pci-Add-realtek-pcie-support.patch \
	file://patches/0070-common-add-rtk-fss-scan-smc.patch \
	file://patches/0071-bootm-RTK_FSS_SCAN_SMC-add-fssscan-subcmd-after-fdt.patch \
	file://patches/0072-bootm-android-support-adtbo_idx-for-dtbo.patch \
	file://patches/100-mtd-spi-support-ENV_OFFSET_REDUND.patch \
	file://patches/101-spl-add-SPI-NAND-support-via-MTD-in-SPL.patch \
	file://patches/102-spl-add-BOOT_DEVICE_SPI_NAND-for-SPL_SPI_NAND_LOAD.patch \
	file://patches/103-mtd-nand-ecc-support-ecc-engine-framework-on-u-boot.patch \
	file://patches/903-arm-enable-ARM_SMCCC-without-ARM_PSCI_FW.patch \
	file://patches/904-tools-binman-replace-update-current-imagefile.patch \
	file://patches/905-fit-spl-support-FIT_CIPHER.patch \
	file://patches/906-aes-use-mcp-for-aes-cbc.patch \
	file://patches/907-common-hash.c-ensure-sha256-ctx-align-with-ARMV8_CE.patch \
	file://patches/908-spl-fit_full-Record-loadables-into-the-FDT.patch \
	file://patches/910-fit-add-verify-on-image-load.patch \
	file://patches/911-Makefile-Signed-configurations-on-U-Boot-fitImage.patch \
	file://patches/912-spl-Makefile.spl-spl-with-padding.patch \
	file://patches/913-spl-Makefile.spl-usb-dwc3-without-gadget.patch \
	file://patches/915-fit-set-min-decomp-size-to-8M-for-bootlogo.patch \
	file://patches/R0003-support-load-bootargs-from-fat.patch \
	"

SRC_URI:append:rtd16xx = " \
	file://patches/0070-fastboot-avb-add-flashing-unlock-and-wire-lock-state.patch \
	"

ERROR_QA:remove = "patch-status"

do_src_copy() {
	# for scripts/setlocalversion to generate uboot.release
	(cd ${SRC_DIR}/../../; echo -n "-g$(git log --format=%h -1 -- .)" > ${S}/.scmversion)
	(cd ${SRC_DIR}/../../; git --no-optional-locks status -uno --porcelain . | read dummy && echo -n "-dirty" >> ${S}/.scmversion || true)
	cp -afL ${SRC_DIR}/* ${S}
	(cd ${S}; git add -A; git commit -m "Realtek Soc Patches")
}

do_compile:append() {
	mkimage -c none -A arm64 -T script -d ${S}/board/realtek/bootconfig.cmd ${B}/boot.scr
}

do_deploy:append() {
	cp ${S}/board/realtek/config.txt ${B}/boot.scr ${DEPLOY_DIR_IMAGE}/
}

do_compile[depends] = "bootfiles:do_deploy"

addtask src_copy before do_patch after do_unpack
