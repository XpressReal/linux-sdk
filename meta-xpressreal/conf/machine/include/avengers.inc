# meta-avengers default settings

require rootfs.inc

PREFERRED_PROVIDER_virtual/kernel ?= "linux-yocto"
PREFERRED_VERSION_linux-yocto ?= "6.6%"

PREFERRED_VERSION_nginx = "1.24.0"

KERNEL_IMAGETYPE = "Image"

IMAGE_FSTYPES ?= "tar.bz2 wic wic.xz"

# Do not update fstab file when using wic images
WIC_CREATE_EXTRA_ARGS ?= "--no-fstab-update"

MACHINE_FEATURES:append = " SPL"

#misc
SERIAL_CONSOLES ?= "460800;ttyS0"
SERIAL_CONSOLES_CHECK ??= "${SERIAL_CONSOLES}"

INIT_MANAGER = "systemd"

KERNEL_EXTRA_FEATURES:append = "${@bb.utils.contains('MACHINE_FEATURES', 'overlayfs-root', ' features/overlayfs/overlayfs.scc', '', d)}"
EXTRA_IMAGE_FEATURES += "${@bb.utils.contains('MACHINE_FEATURES', 'overlayfs-root', ' overlayfs-root', '', d)}"
WKS_FILE ?= "${@bb.utils.contains('MACHINE_FEATURES', 'overlayfs-root', 'avengers-overlayfs.wks', 'avengers.wks', d)}"

BOOTFILES_DIR ?= "bootfiles"
IMAGE_BOOT_FILES = " \
	yocto.itb \
	"

do_image_wic[depends] += " \
	bootfiles:do_deploy \
	u-boot:do_deploy \
	"
IMAGE_INSTALL:append = " libubootenv-bin kernel-modules fwdbg dropbear sysstat"
IMAGE_INSTALL:append = " rtk-mod-wifi wpa-supplicant"
IMAGE_INSTALL:append = " alsa-utils"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'mesa-avengers', 'libmali vulkan-headers vulkan-loader vulkan-tools vulkan-samples vkcube', d)}"
IMAGE_INSTALL:append = " ${@oe.utils.conditional('VIRTUAL-RUNTIME_init_manager', 'systemd', 'systemd-firmware', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'qt', 'qtbase qtbase-examples qtdeclarative qtdeclarative-examples qtwayland', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'qt', 'udev-extraconf', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'rtk-ui', 'rtk-qt-base', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'rtk-ui-plus', 'rtk-qt-plus', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'stt-capable', ' stt-demo rtkstt libtorch llama-cpp tinyllama-gguf', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'bt', ' rtkbt rtk-mod-btusb bluez5', '', d)}"

# stt and tinyllama setting
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'stt-capable', ' stt-demo rtkstt libtorch llama-cpp tinyllama-gguf', '', d)}"
#IMAGE_INSTALL:append = " llama-cpp tinyllama-gguf"

# font settings
FONT_CHINESE = "ttf-droid-sans ttf-droid-sans-fallback ttf-droid-sans-mono ttf-droid-serif freetype"
IMAGE_LINGUAS:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'rtk-ui', 'en-us zh-cn zh-tw', '', d)}"
IMAGE_INSTALL:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'rtk-ui', "rtk-qt-base ${FONT_CHINESE}", '', d)}"

DISTRO_FEATURES:remove = " ${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', '', 'x11', d)}"
DISTRO_FEATURES:append = " ${@oe.utils.conditional('VIRTUAL-RUNTIME_init_manager', 'systemd', 'pam', '', d)}"

PREFERRED_VERSION_weston ?= "13.0.1"
PREFERRED_VERSION_mesa ?= "24.1.4"
PREFERRED_VERSION_wayland-protocols ?= "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', '1.35', '', d)}"

PREFERRED_PROVIDER_virtual/libgles1 ?= "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'mesa', 'libmali', d)}"
PREFERRED_PROVIDER_virtual/libgles2 ?= "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'mesa', 'libmali', d)}"
PREFERRED_PROVIDER_virtual/libgles3 ?= "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'mesa', 'libmali', d)}"
PREFERRED_PROVIDER_virtual/egl ?= "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'mesa', 'libmali', d)}"
PREFERRED_PROVIDER_virtual/libgbm ?= "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'mesa', 'libmali', d)}"

PREFERRED_VERSION_vulkan-headers ?= "1.3.231.1"
PREFERRED_VERSION_vulkan-loader ?= "1.3.231.1"
PREFERRED_VERSION_vulkan-tools ?= "1.3.231.1"

GSTREAMER_VERSION ?= "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', '1.24.5', '1.22.12', d )}"
PREFERRED_VERSION_gstreamer1.0 ?= "${GSTREAMER_VERSION}"
PREFERRED_VERSION_gstreamer1.0-python ?= "${GSTREAMER_VERSION}"
PREFERRED_VERSION_gstreamer1.0-libav ?= "${GSTREAMER_VERSION}"
PREFERRED_VERSION_gstreamer1.0-plugins-bad ?= "${GSTREAMER_VERSION}"
PREFERRED_VERSION_gstreamer1.0-plugins-base ?= "${GSTREAMER_VERSION}"
PREFERRED_VERSION_gstreamer1.0-plugins-good ?= "${GSTREAMER_VERSION}"
PREFERRED_VERSION_gstreamer1.0-plugins-ugly ?= "${GSTREAMER_VERSION}"

IMAGE_INSTALL:append = " rtkshmallocator rtkdmaallocator rtksubtitlepool rtkdvdspu rtksrtrender rtkassrender rtkhookqueue rtkvowb rtkmixer rtknvrsink rtkhse rtksubtitlebin"

XSERVER ?= " \
	xserver-xorg \
	xf86-video-modesetting \
	mesa-driver-swrast \
	xserver-xorg-utils \
	xserver-xorg-extension-glx \
	xf86-input-evdev \
	xf86-input-mouse \
        "

MACHINE_ESSENTIAL_EXTRA_RRECOMMENDS += "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', '', 'kernel-module-mali_kbase kernel-module-dmabuf-exporter', d)}"

