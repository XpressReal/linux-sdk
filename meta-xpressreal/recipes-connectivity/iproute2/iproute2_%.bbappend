do_install:append() {
    install -d  ${D}${bindir}
    (cd ${D}${bindir} && ln -sf ${sbindir}/ip . )
}

FILES:${PN} += "${bindir}/ip"
