RDEPENDS:${PN}-tools += "perl"
PACKAGECONFIG:append = " eglfs \
						 gles2 \
						 kms \
						 gbm \
						 "
PACKAGECONFIG:append = " fontconfig"
PACKAGECONFIG:append = " examples"

# replace original setting in qtbase_git.bb
PACKAGECONFIG[eglfs] = "-egl -eglfs,-no-eglfs,drm"
PACKAGECONFIG_GL = ""
