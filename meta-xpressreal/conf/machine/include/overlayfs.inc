# meta-avengers overlayfs default settings

OVERLAYFS_ETC_MOUNT_POINT ?= "/overlay"
OVERLAYFS_ETC_FSTYPE ?= "ext4"
# Prefer SDMMC to eMMC
OVERLAYFS_ETC_DEVICE ?= "/dev/mmcblk1p3 /dev/mmcblk0p3"
OVERLAYFS_ETC_USE_ORIG_INIT_NAME ?= "0"
OVERLAYFS_ETC_MOUNT_OPTIONS ?= "defaults"
OVERLAYFS_ETC_INIT_TEMPLATE ?= "${COREBASE}/meta/files/overlayfs-root-preinit.sh.in"
OVERLAYFS_ETC_EXPOSE_LOWER ?= "0"
OVERLAYFS_ETC_CREATE_MOUNT_DIRS ?= "0"
