
PACKAGES =+ "${PN}-src-lib"
FILES:${PN}-src-lib = "${libdir}/rustlib/src/rust"

rust_do_install:append:class-native() {
    install -d ${D}${libdir}/rustlib/src/rust
    cp -r ${S}/library ${D}${libdir}/rustlib/src/rust
    find ${D}${libdir}/rustlib/src/rust/ -name "*.sh" -type f -delete
}

rust_do_install:class-nativesdk:append() {
    install -d ${D}${libdir}/rustlib/src/rust
    cp -r ${S}/library ${D}${libdir}/rustlib/src/rust
    find ${D}${libdir}/rustlib/src/rust/ -name "*.sh" -type f -delete
}

rust_do_install:class-target:append() {
    install -d ${D}${libdir}/rustlib/src/rust
    cp -r ${S}/library ${D}${libdir}/rustlib/src/rust
    find ${D}${libdir}/rustlib/src/rust -name "*.sh" -type f -delete
    install -m 0644 ${WORKDIR}/rust-targets/${RUST_HOST_SYS}.json ${D}${libdir}/rustlib/${RUST_HOST_SYS}/${RUST_HOST_SYS}.json
}
