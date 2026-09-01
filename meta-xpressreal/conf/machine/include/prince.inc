# add a common override for all rtd1635 based board

MACHINEOVERRIDES =. "prince:"

require conf/machine/include/arm/armv8-2a/tune-cortexa55.inc
require conf/machine/include/avengers.inc
require conf/machine/include/overlayfs.inc

#UBOOT_MACHINE = "rtd1635_emmc_defconfig"
UBOOT_CONFIG ??= "rtd1635_emmc"
UBOOT_CONFIG[rtd1635_emmc] = "rtd1635_evb_defconfig,,u-boot.itb"

IMAGE_BOOT_FILES += " \
	u-boot.bin-rtd1635_emmc \
	"
IMAGE_BOOT_FILES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'SPL', 'rtd1635_emmc_bind.bin', '', d)}"

KERNEL_DEVICETREE = " \
	realtek/rtd1761b-rose-8gb-hdmi.dtb \
	realtek/rtd1761-rescue.dtb \
	"

IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'linux-firmware-rtd1635', '', d)}"

IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'libnpu', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'rtk-mod-npu', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'libnpu-header', '', d)}"