KMNVER = "${@d.getVar('PV').split('.')[0]}.${@d.getVar('PV').split('.')[1]}"

FILESEXTRAPATHS:prepend := "${THISDIR}/files-${KMNVER}:"


COMPATIBLE_MACHINE:realtekevb-rtd1619b = "stark"
COMPATIBLE_MACHINE:backinblack-rtd1619b = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b-chromium = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b-mini = "stark"
COMPATIBLE_MACHINE:pymparticles-rtd1319 = "hank"
COMPATIBLE_MACHINE:pymparticles-rtd1319-mini = "hank"

KMACHINE:pymparticles-rtd1319-mini = "pymparticles-rtd1319"
KMACHINE:realtekevb-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:backinblack-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:bleedingedge-rtd1619b-chromium = "bleedingedge-rtd1619b"
KMACHINE:bleedingedge-rtd1619b-mini = "bleedingedge-rtd1619b"

SRC_URI:append = " file://avengers-kmeta;type=kmeta;name=avengers-kmeta;destsuffix=avengers-kmeta"

SRC_URI:append:stark = " file://stark.scc file://stark.cfg"
SRC_URI:append:hank = " file://hank.scc file://hank.cfg"

KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', 'chromium', '', 'features/upstream/upstream.scc', d)}"
KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', 'mali', '', 'features/nas/nas.scc', d)}"

V4L2_CFG = "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', 'v4l2_stateless.scc', 'v4l2_stateful.scc', d )}"

KERNEL_FEATURES:append: = " \
			${@bb.utils.contains('MACHINE_FEATURES', 'overlayfs-root', 'features/init/init.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/dma-buf/dma-buf.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/drm/drm.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/rpmsg/rpmsg.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/media/media.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/${V4L2_CFG}', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/sound/sound.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'hifi', 'features/sound/hifi.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'features/drm/panfrost.scc', 'features/mali/mali.scc', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'chromium', 'features/chromium/chromium.scc', '', d)} \
			"

KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', '2KUI', 'features/linux/linux.scc', '', d)}"

KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', 'mipi', 'features/drm/mipi.scc', '', d)}"

KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', 'tee', 'features/tee/tee.scc', '', d)}"

KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'snd-soc-hifi-realtek snd-soc-realtek rtk_avcpulog', '', d)}"
require linux-avengers.inc
