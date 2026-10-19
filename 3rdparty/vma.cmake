voxen_add_3rdparty_library(vma)

# VMA is a single header, this TU instantiates its implementation. Vulkan functions
# are fetched at runtime through `VmaVulkanFunctions`, nothing links `vulkan-1`.
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vma-generated/vk_mem_alloc.cpp CONTENT [[
#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>
]])
target_sources(vma PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/vma-generated/vk_mem_alloc.cpp)
target_include_directories(vma SYSTEM PUBLIC vma/include)
target_link_libraries(vma PUBLIC 3rdparty::vulkan-headers)
