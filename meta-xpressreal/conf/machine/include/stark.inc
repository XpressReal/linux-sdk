# add a common override for all rtd1619b based board

MACHINEOVERRIDES =. "stark:"

require conf/machine/include/avengers.inc
require conf/machine/include/overlayfs.inc

SPL_SUFFIX = "bin_pad"
SPL_BINARY = "spl/u-boot-spl.bin_pad"

#UBOOT_MACHINE = "rtd1619b_emmc_defconfig"
UBOOT_CONFIG ??= "rtd1619b_emmc rtd1619b_spi"
UBOOT_CONFIG[rtd1619b_emmc] = "rtd1619b_bleedingedge_defconfig,,u-boot.itb"
UBOOT_CONFIG[rtd1619b_spi] = "rtd1619b_spi_defconfig,,u-boot.itb"

IMAGE_BOOT_FILES += " \
	u-boot.bin-rtd1619b_emmc \
	u-boot.bin-rtd1619b_spi \
	"
IMAGE_BOOT_FILES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'SPL', 'rtd1619b_emmc_bind.bin rtd1619b_emmc_bind_4gb.bin', '', d)}"

KERNEL_DEVICETREE = " \
	realtek/rtd1619b-bleedingedge-2gb.dtb \
	realtek/rtd1619b-backinblack-2gb.dtb \
	realtek/rtd1619b-rescue.dtb \
	"
KERNEL_DEVICETREE:append = " realtek/rtd1619b-bleedingedge-2gb-spi.dtb"
IMAGE_INSTALL:append = " linux-firmware-aic8800"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'mali', 'linux-firmware-rtd1619b', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'libnpu', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'npu', 'rtk-mod-npu', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'tee', 'linux-tee', '', d)}"
