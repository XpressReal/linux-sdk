FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# Bump to bc3af51 which includes:
# - strncpy fix (merged upstream, 0001 patch no longer needed)
# - prepare in pcm_generic_transfer (fixes silent playback on RTK hifi_pcm)
# - pcm_drain() API addition
SRCREV = "bc3af517534346742a2e753f753a0ad21f51513b"
LIC_FILES_CHKSUM = "file://NOTICE;md5=d2918795d9185efcbf430b9ad5cda46d"

SRC_URI:remove = "file://0001-fixed-compilation-error-caused-by-strncpy.patch"
SRC_URI:append = " \
    file://0002-tinyplay-use-pcm_drain-instead-of-pcm_wait.patch \
    file://0003-tinywavinfo-fix-sign-compare-warning.patch \
"
