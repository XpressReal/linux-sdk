KMNVER = "${@d.getVar('PV').split('.')[0]}.${@d.getVar('PV').split('.')[1]}"

FILESEXTRAPATHS:prepend := "${THISDIR}/files-${KMNVER}:"

COMPATIBLE_MACHINE:phantom-rtd1625-mini = "kent"
COMPATIBLE_MACHINE:phantom-vcodec-rtd1625 = "kent"
COMPATIBLE_MACHINE:bleedingedge-rtd1619b-mini = "stark"

KMACHINE:phantom-rtd1625-mini = "phantom-rtd1625"
KMACHINE:phantom-vcodec-rtd1625 = "phantom-rtd1625"
KMACHINE:bleedingedge-rtd1619b-mini = "bleedingedge-rtd1619b"

SRC_URI:append = " file://avengers-kmeta;type=kmeta;name=avengers-kmeta;destsuffix=avengers-kmeta"

SRC_URI:append:stark = " file://stark.scc file://stark.cfg"
SRC_URI:append:kent = " file://kent.scc file://kent.cfg"

KERNEL_FEATURES:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'mali', '', 'features/nas/nas.scc', d)}"

KERNEL_FEATURES:append = " \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/drm/drm.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/media.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/rpmsg/rpmsg.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/v4l2.scc', '', d)} \
			${@bb.utils.contains('MACHINE_FEATURES', 'v4l2', 'features/media/v4l2_stateful.scc', '', d)} \
			"

KERNEL_FEATURES:append = "${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', '', 'features/mali/mali.scc', d)}"

require linux-avengers.inc
