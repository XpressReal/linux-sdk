PACKAGECONFIG:append:pn-gstreamer1.0-plugins-bad = " v4l2codecs"

IMAGE_INSTALL:append = " \
		libdrm libdrm-tests \
		glmark2 \
		wayland weston \
		rtk-mod-v4l2dec v4l-utils v4l2test \
		rtk-mod-v4l2cap \
		gstreamer1.0 \
		gstreamer1.0-python \
		gstreamer1.0-plugins-base \
		gstreamer1.0-plugins-good \
		gstreamer1.0-plugins-bad \
		gstreamer1.0-meta-base \
		fluster \
		nnstreamer \
		json-c \
		rtk-media-framework \
		alsa-utils \
		"

