FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_DIR := "${THISDIR}/files/src"

require u-boot-avengers.inc

DEPENDS += "u-boot-mkimage-native xxd-native lzop-native"

SRC_URI:append = " \
	file://patches/0001-boot-Only-define-checksum-algos-when-the-hashes-are-.patch \
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
	file://patches/903-arm-enable-ARM_SMCCC-without-ARM_PSCI_FW.patch \
	file://patches/904-tools-binman-replace-update-current-imagefile.patch \
	file://patches/905-fit-spl-support-FIT_CIPHER.patch \
	file://patches/906-aes-use-mcp-for-aes-cbc.patch \
	file://patches/907-common-hash.c-Use-stack-space-for-hash-context.patch \
	file://patches/910-fit-add-verify-on-image-load.patch \
	file://patches/911-Makefile-Signed-configurations-on-U-Boot-fitImage.patch \
	file://patches/912-spl-Makefile.spl-spl-with-padding.patch \
	file://patches/913-spl-Makefile.spl-usb-dwc3-without-gadget.patch \
	file://patches/915-fit-set-min-decomp-size-to-8M-for-bootlogo.patch \
	file://patches/R0003-support-load-bootargs-from-fat.patch \
	"

ERROR_QA:remove = "patch-status"

do_src_copy() {
	cp -afL ${SRC_DIR}/* ${S}
	(cd ${S}; git add -A; git commit -m "Realtek Soc Patches")
}

do_compile[depends] = "bootfiles:do_deploy"

addtask src_copy before do_patch after do_unpack
