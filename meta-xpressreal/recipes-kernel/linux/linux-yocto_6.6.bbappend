KMNVER = "${@d.getVar('PV').split('.')[0]}.${@d.getVar('PV').split('.')[1]}"

FILESEXTRAPATHS:prepend := "${THISDIR}/files-${KMNVER}:"


COMPATIBLE_MACHINE:phantom-rtd1625 = "kent"
COMPATIBLE_MACHINE:phantom-rtd1625-mini = "kent"
COMPATIBLE_MACHINE:realtekevb-rtd1619b = "stark"
COMPATIBLE_MACHINE:backinblack-rtd1619b = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b = "stark"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b-mini = "stark"
COMPATIBLE_MACHINE:badassium-rtd1315c = "stark"
COMPATIBLE_MACHINE:xpressreal-rtd1619b = "stark"
COMPATIBLE_MACHINE:pymparticles-rtd1319 = "hank"
COMPATIBLE_MACHINE:pymparticles-rtd1319-mini = "hank"

KMACHINE:pymparticles-rtd1319-mini = "pymparticles-rtd1319"
KMACHINE:realtekevb-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:backinblack-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:bleedingedge-rtd1619b-mini = "bleedingedge-rtd1619b"
KMACHINE:badassium-rtd1315c = "bleedingedge-rtd1619b"
KMACHINE:xpressreal-rtd1619b = "bleedingedge-rtd1619b"
KMACHINE:phantom-rtd1625-mini = "phantom-rtd1625"

SRC_URI:append = " file://avengers-kmeta;type=kmeta;name=avengers-kmeta;destsuffix=avengers-kmeta"

SRC_URI:append:stark = " file://stark.scc file://stark.cfg"
SRC_URI:append:hank = " file://hank.scc file://hank.cfg"
SRC_URI:append:kent = " file://kent.scc file://kent.cfg"

KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', 'mali', '', 'features/nas/nas.scc', d)}"

V4L2_CFG = "${@bb.utils.contains('DISTRO_FEATURES', 'stateless_v4l2', 'v4l2_stateless.scc', 'v4l2_stateful.scc', d )}"

KERNEL_FEATURES:append: = " \
			${@bb.utils.contains('MACHINE_FEATURES', 'overlayfs-root', 'features/init/init.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/dma-buf/dma-buf.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/drm/drm.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/rpmsg/rpmsg.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/media/media.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/v4l2.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/${V4L2_CFG}', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/sound/sound.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'hifi', 'features/sound/hifi.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'bt', 'features/bt/bt.scc', '', d)} \
			"

KERNEL_FEATURES:append:stark = "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'features/drm/panfrost.scc', 'features/mali/mali.scc', d)}"
KERNEL_FEATURES:append:hank = "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'features/drm/panfrost.scc', 'features/mali/mali.scc', d)}"
KERNEL_FEATURES:append:kent = "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', 'features/drm-panthor/panthor.scc', 'features/mali/mali.scc', d)}"


KERNEL_FEATURES:append:stark = " \
				${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/drm/stark.scc', '', d)} \
				${@bb.utils.contains('MACHINE_FEATURES', 'hifi', ' features/sound/stark.scc', '', d)} \
				"

# kent audio flow must be in hifi so always enable hifi flow
KERNEL_FEATURES:append:kent = " \
				${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'features/drm/kent.scc', '', d)} \
				${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/sound/hifi.scc', '', d)} \
				${@bb.utils.contains('MACHINE_FEATURES', 'hifi', ' features/sound/kent.scc', '', d)} \
				"

KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', 'mipi', 'features/drm/mipi.scc', '', d)}"

KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', 'tee', 'features/tee/tee.scc', '', d)}"

KERNEL_FEATURES:append = "${@(' features/gamepad/gamepad-mipi.scc' if (bb.utils.contains('MACHINE_FEATURES', 'gamepad', True, False, d) and bb.utils.contains('MACHINE_FEATURES', 'mipi', True, False, d)) else (' features/gamepad/gamepad-hdmi.scc' if bb.utils.contains('MACHINE_FEATURES', 'gamepad', True, False, d) else ''))}"

KERNEL_FEATURES:append: = " ${@bb.utils.contains('MACHINE_FEATURES', 'kvm', 'features/kvm/kvm.scc', '', d)}"

KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains('MACHINE_FEATURES', 'drm', 'snd-soc-rtk-hifi snd-soc-rtk-afe rtk_avcpulog', '', d)}"

KERNEL_MODULE_AUTOLOAD += " ${@bb.utils.contains('MACHINE_FEATURES', 'bt', 'rtk_rfkill', '', d)}"
require linux-avengers.inc
