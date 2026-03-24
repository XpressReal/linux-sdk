FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
        file://0014-enable-stateful-av1-format.patch \
        "

PACKAGECONFIG:append = " proprietary-codecs"
PACKAGECONFIG[use-v4l2] = "use_v4l2_codec=true enable_hevc_parser_and_hw_decoder=true enable_platform_hevc=true,use_v4l2_codec=false"

DEPENDS:append = " ${@bb.utils.contains('MACHINE_FEATURES', 'panfrost', '', 'dummy-dri', d)}"
