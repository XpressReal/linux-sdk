# Remove the dependency on mesa-vulkan-drivers since we use mali GPU
RRECOMMENDS:${PN}:remove = "mesa-vulkan-drivers"
