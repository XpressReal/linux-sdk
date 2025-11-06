ROOTFS_NAME = "rootfs.tar.xz"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI = "file://configs/modules \
	   file://configs/eth0 \
	   file://configs/end0 \
	   file://configs/noclear.conf \
	   file://configs/fstab \
	   file://configs/12-autologin.conf \
	   file://configs/52-drm.rules \
	   file://configs/start-weston.sh \
	   file://configs/wpa_supplicant-wlan0.conf \
	   file://configs/timeout.conf \
	   file://configs/70-persistent-net.rules \
	   file://README \
	   file://libgl1-mesa-dri_22.3.6-1.deb;unpack=0 \
	   file://libgl1-mesa-dri_24.0.9-0ubuntu0.2_arm64.deb;unpack=0 \
	  "

do_install() {
	install -d ${D}/etc/network/interfaces.d
	install -d ${D}/etc/systemd/system/getty@tty1.service.d
	install -d ${D}/lib/systemd/system
	install -d ${D}/etc/systemd/system.conf.d
	install -d ${D}/etc/default
	install -d ${D}/etc/profile.d
	install -d ${D}/opt/realtek
	install -d ${D}/etc/udev/rules.d
	install -d ${D}/usr/sbin
	install -d ${D}/etc/wpa_supplicant
	install -m 644 ${WORKDIR}/configs/modules ${D}/etc
	install -m 644 ${WORKDIR}/configs/fstab ${D}/etc
	install -m 644 ${WORKDIR}/configs/wpa_supplicant-wlan0.conf ${D}/etc/wpa_supplicant
	install -m 644 ${WORKDIR}/configs/70-persistent-net.rules ${D}/etc/udev/rules.d
	install -m 644 ${WORKDIR}/configs/timeout.conf ${D}/etc/systemd/system.conf.d

	if [ "${@bb.utils.contains("MACHINE_FEATURES", "split-home", "1", "0", d)}" = "0" ]; then
		sed -i -e 's:^LABEL=home:#LABEL=home:' ${D}/etc/fstab
	fi

	if [ "${@bb.utils.contains("MACHINE_FEATURES", "overlayfs-root", "1", "0", d)}" = "1" ]; then
		sed -i -e 's:^LABEL=rootfs:#LABEL=rootfs:' ${D}/etc/fstab
	fi

	install -m 644 ${WORKDIR}/README ${D}/opt/realtek
	install -m 644 ${WORKDIR}/libgl1-mesa-dri_22.3.6-1.deb ${D}/opt/realtek
	install -m 644 ${WORKDIR}/libgl1-mesa-dri_24.0.9-0ubuntu0.2_arm64.deb ${D}/opt/realtek
}

do_package_qa[noexec] = "1"

FILES:${PN} += "/${ROOTFS_NAME} /configs.tar.xz /opt /lib/systemd/system /usr/sbin"
