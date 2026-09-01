# meta-xpressreal settings for console only applications

DISTRO_FEATURES:remove = "x11"
DISTRO_FEATURES:remove = "opengl"
DISTRO_FEATURES:remove = "wayland"
DISTRO_FEATURES:remove = "vulkan"

IMAGE_INSTALL:remove = "alsa-utils"
# supported by libmali
IMAGE_INSTALL:remove = "vulkan-headers"
IMAGE_INSTALL:remove = "vulkan-loader"
IMAGE_INSTALL:remove = "vulkan-tools"
IMAGE_INSTALL:remove = "vulkan-samples"
IMAGE_INSTALL:remove = "vkcube"
IMAGE_INSTALL:remove = "rtkgpuconverter"
