# Hanlding prebuilt rootfs, including Debian/Ubuntu
# PREBUILT_NAME is set in conf/machine/include/rootfs.inc
# Using PREBUILT_NAME to control which rootfs will be used
#
# Use "bitbake debian-image" to generate a Debian or Ubuntu image

DESCRIPTIOM = "Handle Prebuilt Rootfs for Debian/Ubuntu OS"
LICENSE = "CLOSED"

ROOTFS_NAME = "rootfs.tar.${@d.getVar('PREBUILT_NAME').split('.')[-1]}"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = "file://${PREBUILT_NAME};unpack=0 \
	   file://configs/modules \
	   file://configs/eth0 \
	   file://configs/end0 \
	   file://configs/noclear.conf \
	   file://configs/fstab \
	   file://configs/logo_banner.jpg \
	   file://configs/weston \
	   file://configs/weston.ini \
	   file://configs/weston.service \
	   file://configs/weston.sh \
	   file://configs/weston.socket \
	   file://configs/12-autologin.conf \
	   file://configs/52-drm.rules \
	   file://configs/start-weston.sh \
	   file://README \
	   file://libgl1-mesa-dri_22.3.6-1.deb;unpack=0 \
	   file://libgl1-mesa-dri_24.0.9-0ubuntu0.2_arm64.deb;unpack=0 \
	   file://libweston-10-0_10.0.1-1_arm64.deb;unpack=0 \
	   file://weston_10.0.1-1_arm64.deb;unpack=0 \
	  "

do_install() {
        install -d ${D}/etc/network/interfaces.d
	install -d ${D}/etc/systemd/system/getty@tty1.service.d
	install -d ${D}/lib/systemd/system
	install -d ${D}/etc/xdg/weston
	install -d ${D}/etc/default
	install -d ${D}/etc/profile.d
	install -d ${D}/opt/realtek
	install -d ${D}/etc/lightdm/lightdm.conf.d
	install -d ${D}/etc/udev/rules.d
	install -d ${D}/usr/sbin
	ln -sf ${WORKDIR}/${PREBUILT_NAME} ${D}/${ROOTFS_NAME}
	install -m 644 ${WORKDIR}/configs/modules ${D}/etc
	install -m 644 ${WORKDIR}/configs/fstab ${D}/etc
	if [ "${@bb.utils.contains("MACHINE_FEATURES", "split-home", "1", "0", d)}" = "0" ]; then
		sed -i -e 's:^LABEL=home:#LABEL=home:' ${D}/etc/fstab
	fi

	if [[ "${PREBUILT_NAME}" != *"rpios"* ]]; then
		install -m 644 ${WORKDIR}/configs/eth0 ${D}/etc/network/interfaces.d
		install -m 644 ${WORKDIR}/configs/end0 ${D}/etc/network/interfaces.d
		install -m 644 ${WORKDIR}/configs/weston.ini ${D}/etc/xdg/weston
		install -m 644 ${WORKDIR}/configs/logo_banner.jpg ${D}/etc/xdg/weston
		install -m 644 ${WORKDIR}/configs/weston ${D}/etc/default
		install -m 644 ${WORKDIR}/configs/weston.sh ${D}/etc/profile.d
		install -m 644 ${WORKDIR}/configs/noclear.conf ${D}/etc/systemd/system/getty@tty1.service.d
		install -m 644 ${WORKDIR}/configs/12-autologin.conf ${D}/etc/lightdm/lightdm.conf.d
        fi

	case ${PREBUILT_NAME} in
		"weston-"* )
		install -m 644 ${WORKDIR}/configs/weston.socket ${D}/lib/systemd/system
		install -m 644 ${WORKDIR}/configs/weston.service ${D}/lib/systemd/system
		install -m 644 ${WORKDIR}/configs/52-drm.rules ${D}/etc/udev/rules.d
		install -m 755 ${WORKDIR}/configs/start-weston.sh ${D}/usr/sbin
		;;
	esac

	install -m 644 ${WORKDIR}/README ${D}/opt/realtek
	install -m 644 ${WORKDIR}/libgl1-mesa-dri_22.3.6-1.deb ${D}/opt/realtek
	install -m 644 ${WORKDIR}/libgl1-mesa-dri_24.0.9-0ubuntu0.2_arm64.deb ${D}/opt/realtek
	install -m 644 ${WORKDIR}/libweston-10-0_10.0.1-1_arm64.deb ${D}/opt/realtek
	install -m 644 ${WORKDIR}/weston_10.0.1-1_arm64.deb ${D}/opt/realtek
	tar -cvJf ${D}/configs.tar.xz -C ${D}/etc .
}

do_package_qa[noexec] = "1"

FILES:${PN} += "/${ROOTFS_NAME} /configs.tar.xz /opt /lib/systemd/system /usr/sbin"
