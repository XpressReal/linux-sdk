FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += " \
	file://weston.ini \
	file://logo_banner.jpg \
	file://71-weston-drm.rules \
	file://profile \
	file://52-drm.rules \
	file://start-weston.sh \
	"

do_install:append() {
	install -d ${D}${base_prefix}/home/root
	install -D -p -m0644 ${WORKDIR}/weston.ini ${D}${sysconfdir}/xdg/weston/weston.ini
	install -D -p -m 0644 ${WORKDIR}/logo_banner.jpg ${D}${sysconfdir}/xdg/weston/logo_banner.jpg
	install -D -p -m0644 ${WORKDIR}/71-weston-drm.rules  ${D}${sysconfdir}/udev/rules.d/71-weston-drm.rules
	install -D -p -m0644 ${WORKDIR}/52-drm.rules  ${D}${sysconfdir}/udev/rules.d/52-drm.rules
	install -D -p -m0755 ${WORKDIR}/start-weston.sh  ${D}${bindir}/start-weston.sh
	install -D -p -m0644 ${WORKDIR}/profile ${D}${base_prefix}/home/root/.profile

	if ${@bb.utils.contains('MACHINE_FEATURES', 'rtk-ui', 'true', 'false',d )}; then
		sed -i 's/ui-hole-punch=true/ui-hole-punch=false/' ${D}${sysconfdir}/xdg/weston/weston.ini
	fi
}

FILES:${PN} += "${base_prefix}/home/root/.profile"
