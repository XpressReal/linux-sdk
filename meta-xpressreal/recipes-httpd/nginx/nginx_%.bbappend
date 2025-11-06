FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRCREV = "f5e30888a256136d9c550bf1ada77d6ea78a48af"

SRC_URI += " \
	     git://github.com/arut/nginx-dav-ext-module.git;protocol=https;branch=master;destsuffix=dav-ext;name=dav-ext \
	     file://default_server \
	     file://webdav.passwd \
           "

EXTRA_OECONF += "--add-module=../dav-ext --with-http_auth_request_module"

PACKAGECONFIG:append = " webdav gunzip xslt"

do_install:append() {
	install -d ${D}${sysconfdir}/nginx/sites-available
	install -d ${D}${localstatedir}/www/webdav
	install -d ${D}${localstatedir}/www/tmp
	chown www-data:www-data -R ${D}${localstatedir}/www/webdav
	chmod 775 ${D}${localstatedir}/www/webdav
	chown www-data:www-data -R ${D}${localstatedir}/www/tmp
	chmod 775 ${D}${localstatedir}/www/tmp
	install -m 0644 ${WORKDIR}/default_server ${D}${sysconfdir}/nginx/sites-available/
	install -m 0644 ${WORKDIR}/webdav.passwd ${D}${sysconfdir}/nginx/

	#don't install nginx.service when kvm feature is set
	#kvm has its own kvmd-nginx
	if [ "${@bb.utils.contains("MACHINE_FEATURES", "kvm", "1", "0", d)}" = "0" ]; then
		install -d ${D}${sysconfdir}/systemd/system/multi-user.target.wants
		ln -sf /lib/systemd/system/nginx.service \
			${D}${sysconfdir}/systemd/system/multi-user.target.wants/nginx.service
	fi
}

SYSTEMD_SERVICE:${PN} = ""
