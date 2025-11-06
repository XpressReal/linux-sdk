# add a common override for all rtd1625 based board

MACHINEOVERRIDES =. "kent:"

require conf/machine/include/arm/armv8-2a/tune-cortexa78.inc
require conf/machine/include/avengers.inc
require conf/machine/include/overlayfs.inc

SPL_SUFFIX = "bin_pad"
SPL_BINARY = "spl/u-boot-spl.bin_pad"

#UBOOT_MACHINE = "rtd1625_emmc_defconfig"
UBOOT_CONFIG ??= "rtd1625_emmc"
UBOOT_CONFIG[rtd1625_emmc] = "rtd1625_phantom_defconfig,,u-boot.itb"

IMAGE_BOOT_FILES += " \
	u-boot.bin-rtd1625_emmc \
	"
IMAGE_BOOT_FILES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'SPL', 'rtd1625_emmc_bind.bin', '', d)}"

KERNEL_DEVICETREE = " \
	realtek/rtd1501s-phantom-8gb.dtb \
	realtek/rtd1501-rescue.dtb \
	"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'linux-firmware-rtd1625', '', d)}"

IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'libnpu', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'rtk-mod-npu', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'npu-header-dev', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'fr-demo', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'od-demo', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'pose-demo', '', d)}"
