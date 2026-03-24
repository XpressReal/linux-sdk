KMNVER = "${@d.getVar('PV').split('.')[0]}.${@d.getVar('PV').split('.')[1]}"

FILESEXTRAPATHS:prepend := "${THISDIR}/files-${KMNVER}:"


COMPATIBLE_MACHINE:realtekevb-rtd16xx-android = "rtd16xx"
COMPATIBLE_MACHINE:phantom-rtd1625 = "kent"
COMPATIBLE_MACHINE:phantom-rtd1625-mini = "kent"
COMPATIBLE_MACHINE:realtekevb-rtd1619b = "stark"
COMPATIBLE_MACHINE:backinblack-rtd1619b = "stark"
COMPATIBLE_MACHINE:hulkbuster-rtd1619b = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b-mini = "stark"
COMPATIBLE_MACHINE:badassium-rtd1315c = "stark"
COMPATIBLE_MACHINE:xpressreal-rtd1619b = "stark"
COMPATIBLE_MACHINE:pymparticles-rtd1319 = "hank"
COMPATIBLE_MACHINE:pymparticles-rtd1319-mini = "hank"

KMACHINE:pymparticles-rtd1319-mini = "pymparticles-rtd1319"
KMACHINE:realtekevb-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:backinblack-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:hulkbuster-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:bleedingedge-rtd1619b-mini = "bleedingedge-rtd1619b"
KMACHINE:badassium-rtd1315c = "bleedingedge-rtd1619b"
KMACHINE:xpressreal-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:phantom-rtd1625-mini = "phantom-rtd1625"
KMACHINE:realtekevb-rtd16xx-android = "realtekevb-rtd16xx"

# kernel from android
KBRANCH:rtd16xx = "android15-6.6-desktop"
SRCREV_machine:rtd16xx = "f2ce5ec8fb4f5ed596a874173a078253f820411e"
SRC_URI:rtd16xx = "git://android.googlesource.com/kernel/common;name=machine;branch=${KBRANCH};protocol=https; \
           git://git.yoctoproject.org/yocto-kernel-cache;type=kmeta;name=meta;branch=yocto-6.6;destsuffix=${KMETA};protocol=https"
#LINUX_VERSION:rtd16xx = "6.6.77"
#SRCREV_meta:rtd16xx = "145708c10581d59d9bc1d280111d046647b30ef8"

SRC_URI:append = " file://avengers-kmeta;type=kmeta;name=avengers-kmeta;destsuffix=avengers-kmeta"

SRC_URI:append:stark = " file://stark.scc file://stark.cfg"
SRC_URI:append:hank = " file://hank.scc file://hank.cfg"
SRC_URI:append:kent = " file://kent.scc file://kent.cfg"
SRC_URI:append:rtd16xx = " file://rtd16xx.scc file://rtd16xx.cfg"

KERNEL_FEATURES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'android', '', 'features/android/ack.scc', d)}"
KERNEL_FEATURES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'mali', '', 'features/nas/nas.scc', d)}"

V4L2_CFG = "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', 'v4l2_stateless.scc', 'v4l2_stateful.scc', d )}"

KERNEL_FEATURES:append = " \
			${@bb.utils.contains('MACHINE_FEATURES', 'overlayfs-root', 'features/init/init.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/dma-buf/dma-buf.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/drm/drm.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/rpmsg/rpmsg.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/media/media.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/v4l2.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/${V4L2_CFG}', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/sound/sound.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'bt', 'features/bt/bt.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'android', 'features/android/android.scc', '', d)} \
			"

KERNEL_FEATURES:append = "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'features/drm-mesa/mesa.scc', 'features/mali/mali.scc', d)}"


KERNEL_FEATURES:append:stark = " ${@bb.utils.contains('MACHINE_FEATURES', 'nohifi', ' features/sound/nohifi.scc', '', d)}"
KERNEL_FEATURES:append:stark = " ${@bb.utils.contains('MACHINE_FEATURES', '2KUI', 'features/linux/linux.scc', '', d)}"

KERNEL_FEATURES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'mipi', 'features/drm/mipi.scc', '', d)}"

KERNEL_FEATURES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'tee', 'features/tee/tee.scc', '', d)}"

KERNEL_FEATURES:append = "${@(' features/gamepad/gamepad-mipi.scc' if (bb.utils.contains('MACHINE_FEATURES', 'gamepad', True, False, d) and bb.utils.contains('MACHINE_FEATURES', 'mipi', True, False, d)) else (' features/gamepad/gamepad-hdmi.scc' if bb.utils.contains('MACHINE_FEATURES', 'gamepad', True, False, d) else ''))}"

KERNEL_FEATURES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'kvm', 'features/kvm/kvm.scc', '', d)}"

# chromium with mali ddk need use render node but mesa have no need
KERNEL_FEATURES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'chromium-browser', 'features/chromium-browser/chromium-browser.scc', '', d)}"
KERNEL_FEATURES:remove = " ${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'features/chromium-browser/chromium-browser.scc', '', d)}"

KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'snd-soc-rtk-hifi snd-soc-rtk-afe rtk_avcpulog', '', d)}"
KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains('MACHINE_FEATURES', 'bt', 'rtk_rfkill', '', d)}"
KERNEL_MODULE_AUTOLOAD:append:rtd16xx = " ${@bb.utils.contains('MACHINE_FEATURES', 'bt', 'hci_uart', '', d)}"
KERNEL_MODULE_AUTOLOAD:remove:rtd16xx = " ${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'snd-soc-rtk-hifi snd-soc-rtk-afe', '', d)}"

KERNEL_MODULE_AUTOLOAD:remove:stark = "${@bb.utils.contains('MACHINE_FEATURES', 'nohifi', 'snd-soc-rtk-hifi snd-soc-rtk-afe', '', d)}"
KERNEL_MODULE_AUTOLOAD:append:stark = " ${@bb.utils.contains('MACHINE_FEATURES', 'nohifi', 'snd-soc-hifi-realtek snd-soc-realtek snd-realtek-notify', '', d)}"

require linux-avengers.inc
