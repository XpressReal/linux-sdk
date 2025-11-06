# add a common override for all rtd1319 based board

MACHINEOVERRIDES =. "hank:"

require conf/machine/include/arm/armv8-2a/tune-cortexa55.inc
require conf/machine/include/avengers.inc

SPL_SUFFIX = "bin_pad"
SPL_BINARY = "spl/u-boot-spl.bin_pad"

#UBOOT_MACHINE = "rtd1319_spi_defconfig"
UBOOT_CONFIG ??= "rtd1319_spi"
UBOOT_CONFIG[rtd1319_spi] = "rtd1319_spi_defconfig,,u-boot.img"

IMAGE_BOOT_FILES += " \
	u-boot.bin-rtd1319_spi \
	"

KERNEL_DEVICETREE:append = " realtek/rtd1319-pymparticles-2gb-spi.dtb"
