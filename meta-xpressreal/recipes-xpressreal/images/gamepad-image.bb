DESCRIPTION = "Add moonlight-qt for Game Console Applications"

inherit core-image

LICENSE = "MIT"

PACKAGECONFIG:append:pn-gstreamer1.0-plugins-bad = " v4l2codecs"

IMAGE_INSTALL += "moonlight-qt wifi-config \
		  libdrm wayland weston \
		  rtk-mod-v4l2dec \
		  alsa-utils \
		  ttf-dejavu-sans fontconfig wqy-microhei\
		  "

ROOTFS_POSTPROCESS_COMMAND += "disable_weston_service;"

disable_weston_service () {
	rm -f ${IMAGE_ROOTFS}/etc/systemd/system/weston.service
	rm -f ${IMAGE_ROOTFS}/etc/systemd/system/weston.socket

	ln -sf /dev/null ${IMAGE_ROOTFS}/etc/systemd/system/weston.service
	ln -sf /dev/null ${IMAGE_ROOTFS}/etc/systemd/system/weston.socket
}
