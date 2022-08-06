voxen_add_library(vulkan-headers INTERFACE)
add_library(3rdparty::vulkan-headers ALIAS vulkan-headers)

target_include_directories(vulkan-headers INTERFACE vulkan-headers)
