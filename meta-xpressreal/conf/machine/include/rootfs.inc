# add a common rootfs override for prebuilt rootfs, such as Debian/Ubuntu
# Please use OVERRIDES to set ubuntu in your local.conf, default is debian
# OVERRIDES:append = ":ubuntu"
# Use OVERRIDES:append = ":rpios" to use RPIOS rootfs
#
# Debian rootfs image:
# -- debian-rootfs.tar.xz => console mode based rootfs
#    can be combined with MACHINE set to "bleedingedge-rtd1619b-mini"
#    can append "drm" in MACHINE_FEATURES to get virtual console support
# -- desktop-debian-rootfs.tar.xz => XFCE based Debian rootfs tar ball
# -- weston-debian-rootfs.tar.xz => Weston based Debian rootfs tar ball
#
# Ubuntu rootfs image:
# -- ubuntu-rootfs.tar.xz => console mode based rootfs
#    can be combined with MACHINE set to "bleedingedge-rtd1619b-mini"
#    can append "drm" in MACHINE_FEATURES to get virtual console support
# -- desktop-ubuntu-rootfs.tar.xz => Ubuntu Desktop rootfs image
# -- weston-ubuntu-rootfs.tar.xz => Weston based Ubuntu rootfs tar ball
#
# RPIOS rootfs image:
# -- rpios-rootfs.tar.xz => console mode based rootfs
#    can be combined with MACHINE set to "bleedingedge-rtd1619b-mini"
#    can append "drm" in MACHINE_FEATURES to get virtual console support
# -- desktop-rpios-rootfs.tar.xz => Ubuntu Desktop rootfs image
# !!!! No support for weston-rpios-rootfs.tar.xz !!!!!
#
# MACHINE_FEATURES:append = " xdesktop"
# select full desktop environment in Debian, Ubuntu or RPIOS
#
# MACHINE_FEATURES:
# split-home conflicts with overlayfs-root

#debian/ubuntu related settings
PREBUILT_NAME = "debian-rootfs.tar.xz"
PREBUILT_NAME:ubuntu = "ubuntu-rootfs.tar.xz"
PREBUILT_NAME:rpios = "rpios-rootfs.tar.xz"

python() {
    features = d.getVar("MACHINE_FEATURES").split()
    name = d.getVar("PREBUILT_NAME")

    if "mali" not in features and "panfrost" in features:
        features.remove("panfrost")
        if "xdesktop" in features:
            features.remove("xdesktop")
        d.setVar("MACHINE_FEATURES", " ".join(features))

    if "xdesktop" in features and "panfrost" in features:
        d.setVar("PREBUILT_NAME", "desktop-" + name)
    elif "mali" in features:
        d.setVar("PREBUILT_NAME", "weston-" + name)

    if "overlayfs-root" in features and "split-home" in features:
        raise bb.parse.SkipRecipe("Could not set both overlayfs-root and split-home in MACHINE_FEATURES!")
}
