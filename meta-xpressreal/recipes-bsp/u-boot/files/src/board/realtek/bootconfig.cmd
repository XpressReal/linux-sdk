# Boot script for U-Boot (legacy script image, built via
# u-boot_%.bbappend: mkimage -c none -A arm64 -T script -d bootconfig.cmd boot.scr).
#
# Yocto FIT boot with device tree overlay support.
#
# config.txt (FAT partition 1, U-Boot "env import -t" text format) knows:
#   overlays=<name> [<name> ...]   applied in order
# Only that variable is imported; overlays are selected by config.txt and
# applied by chaining "#<name>" onto the base FIT config:
#   bootm addr#base#overlay
# Every failure path falls back to the default "bootm yocto.itb" boot.
#
# It requires a list of environment variables to be defined before load:
# system dependent: mmcidx, bootmaddr
# optional system dependent: bootcfg

setenv config_addr_r  0x08000000

setenv overlays ""
# strip leading # so bootcfg is the bare FIT config name; '#' is re-added at bootm
if test -n "${bootcfg}"; then
	setexpr bootcfg sub "^#" "" "${bootcfg}"
fi

if fatload mmc ${mmcidx}:1 ${bootmaddr} yocto.itb; then

	# rescue boots the untouched FIT configuration (kernel+initrd+fdt)
	if test "${bootcfg}" = "rescue"; then
		bootm ${bootmaddr}#${bootcfg}
	fi

	if fatload mmc ${mmcidx}:1 ${config_addr_r} config.txt; then
		env import -t ${config_addr_r} ${filesize} overlays
	fi

	# default FIT configuration when bootcfg is not set
	if test -z "${bootcfg}"; then
		fdt addr ${bootmaddr}
		fdt get value bootcfg /configurations default
	fi

	if test -n "${bootcfg}"; then
		if test -n "${overlays}"; then
			for overlay in ${overlays}; do
				setenv bootcfg "${bootcfg}#${overlay}"
			done
		fi
		bootm ${bootmaddr}#${bootcfg}

		# overlay path failed: boot the default FIT config
		echo "Overlay boot failed, falling back to default FIT boot"
		bootm ${bootmaddr}
	else
		echo "ERROR: bootcfg not set and not default conf in yocto.itb"
	fi
else
	echo "ERROR: yocto.itb not found on mmc ${mmcidx}:1"
fi
