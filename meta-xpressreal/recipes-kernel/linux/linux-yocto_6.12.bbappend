KMNVER = "${@d.getVar('PV').split('.')[0]}.${@d.getVar('PV').split('.')[1]}"

FILESEXTRAPATHS:prepend := "${THISDIR}/files-${KMNVER}:"

COMPATIBLE_MACHINE:realtekevb-rtd16xx-android = "rtd16xx"
COMPATIBLE_MACHINE:evb-rtd1635-mini = "prince"
COMPATIBLE_MACHINE:rose-rtd1635 = "prince"
COMPATIBLE_MACHINE:phantom-rtd1625-mini = "kent"
COMPATIBLE_MACHINE:phantom-vcodec-rtd1625 = "kent"
COMPATIBLE_MACHINE:phantom-rtd1625 = "kent"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b-mini = "stark"

KMACHINE:evb-rtd1635-mini = "evb-rtd1635"
KMACHINE:rose-rtd1635 = "evb-rtd1635"
KMACHINE:phantom-rtd1625-mini = "phantom-rtd1625"
KMACHINE:phantom-vcodec-rtd1625 = "phantom-rtd1625"
KMACHINE:phantom-rtd1625 = "phantom-rtd1625"
KMACHINE:bleedingedge-rtd1619b-mini = "bleedingedge-rtd1619b"
KMACHINE:realtekevb-rtd16xx-android = "realtekevb-rtd16xx"

SRCREV_machine = "081aa259b8f0252bfc7999b289b79bf129893498"
SRCREV_meta = "7a8d96185b9be165feb974fe6297b518f83b3b9c"
LINUX_VERSION = "6.12.58"

# kernel from android
KBRANCH:rtd16xx = "${@bb.utils.contains('KERNEL_FEATURES', 'rust', 'android16-6.12-lts', 'android16-6.12-2025-12', d)}"
SRCREV_machine:rtd16xx = "${@bb.utils.contains('KERNEL_FEATURES', 'rust', '64d481b3eec91a37206dd9373edb9c23bce4ea66', 'd4b665cdd41c28dd83ca6b7584ab426801743c5f', d)}"
LINUX_VERSION:rtd16xx = "${@bb.utils.contains('KERNEL_FEATURES', 'rust', '6.12.90', '6.12.58', d)}"
SRCREV_meta:rtd16xx = "${@bb.utils.contains('KERNEL_FEATURES', 'rust', 'b72dc1f8d370e15118f7593e33ff366cc5cd6eaf', '7a8d96185b9be165feb974fe6297b518f83b3b9c', d)}"
SRC_URI:rtd16xx = "git://android.googlesource.com/kernel/common;name=machine;branch=${KBRANCH};protocol=https; \
           git://git.yoctoproject.org/yocto-kernel-cache;type=kmeta;name=meta;branch=yocto-6.12;destsuffix=${KMETA};protocol=https"

SRC_URI:append = " file://avengers-kmeta;type=kmeta;name=avengers-kmeta;destsuffix=avengers-kmeta"

SRC_URI:append:prince = " file://prince.scc file://prince.cfg"
SRC_URI:append:kent = " file://kent.scc file://kent.cfg"
SRC_URI:append:stark = " file://stark.scc file://stark.cfg"
SRC_URI:append:rtd16xx = " ${@bb.utils.contains('KERNEL_FEATURES', 'rust', 'file://revert.scc', '', d)}"
SRC_URI:append:rtd16xx = " file://rtd16xx.scc file://rtd16xx.cfg"

KERNEL_FEATURES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'mali', '', 'features/nas/nas.scc', d)}"

V4L2_CFG = "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', 'v4l2_stateless.scc', 'v4l2_stateful.scc', d )}"

KERNEL_FEATURES:append = " \
			${@bb.utils.contains_any('MACHINE_FEATURES', 'v4l2 drm', 'features/dma-buf/dma-buf.scc', '', d)} \
			${@bb.utils.contains_any('MACHINE_FEATURES', 'v4l2 drm', 'features/remoteproc/remoteproc.scc', '', d)} \
			${@bb.utils.contains_any('MACHINE_FEATURES', 'v4l2 drm', 'features/rpmsg/rpmsg.scc', '', d)} \
			${@bb.utils.contains_any('MACHINE_FEATURES', 'v4l2 drm', 'features/drm/drm.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/sound/sound.scc', '', d)} \
			${@bb.utils.contains_any('MACHINE_FEATURES', 'v4l2 drm', 'features/media/media.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/v4l2.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/${V4L2_CFG}', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'dprx', 'features/dprx/dprx.scc', '', d)} \
			${@bb.utils.contains_any('MACHINE_FEATURES', 'vendor-wifi upstream-wifi', 'features/wifi/wifi.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'upstream-wifi', 'features/wifi/rtw.scc', '', d)} \
			${@bb.utils.contains_any('MACHINE_FEATURES', 'vendor-bt upstream-bt', 'features/bt/bt.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'upstream-bt', 'features/bt/rtl.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'android', 'features/android/android.scc', '', d)} \
			"

KERNEL_FEATURES:append = "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'features/drm-mesa/mesa.scc', 'features/mali/mali.scc', d)}"
KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'snd-soc-rtk-hifi snd-soc-rtk-afe', '', d)}"
KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains_any('MACHINE_FEATURES', 'v4l2 drm', 'rtk_avcpulog', '', d)}"
KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains('MACHINE_FEATURES', 'vendor-bt', 'rtk_rfkill', '', d)}"
KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains('MACHINE_FEATURES', 'upstream-bt', 'hci_uart', '', d)}"

require linux-avengers.inc
