FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_DIR := "${THISDIR}/files/src"

require u-boot-avengers.inc

DEPENDS += "u-boot-mkimage-native xxd-native lzop-native"

SRC_URI:append = " \
	file://patches/0001-boot-Only-define-checksum-algos-when-the-hashes-are-.patch \
	file://patches/0002-env-mmc-Make-redundant-env-in-both-eMMC-boot-partiti.patch \
	file://patches/0003-usb-dwc3-gadget-fix-crash-in-dwc3_gadget_giveback.patch \
	file://patches/0004-usb-dwc3-invalidate-dcache-on-buffer-used-in-interru.patch \
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
	file://patches/0066-common-Add-command-for-s5.patch \
	file://patches/903-arm-enable-ARM_SMCCC-without-ARM_PSCI_FW.patch \
	file://patches/904-tools-binman-replace-update-current-imagefile.patch \
	file://patches/905-fit-spl-support-FIT_CIPHER.patch \
	file://patches/906-aes-use-mcp-for-aes-cbc.patch \
	file://patches/910-fit-add-verify-on-image-load.patch \
	file://patches/911-Makefile-Signed-configurations-on-U-Boot-fitImage.patch \
	file://patches/912-spl-Makefile.spl-spl-with-padding.patch \
	file://patches/913-spl-Makefile.spl-usb-dwc3-without-gadget.patch \
	file://patches/915-fit-set-min-decomp-size-to-8M-for-bootlogo.patch \
	file://patches/R0001-net-Remove-eth_legacy.c.patch \
	file://patches/R0002-net-Make-DM_ETH-be-selected-by-NETDEVICE.patch \
	file://patches/R0003-support-load-bootargs-from-fat.patch \
	"

ERROR_QA:remove = "patch-status"

do_src_copy() {
	cp -afL ${SRC_DIR}/* ${S}
	(cd ${S}; git add -A; git commit -m "Realtek Soc Patches")
}

do_compile[depends] = "bootfiles:do_deploy"

addtask src_copy before do_patch after do_unpack
