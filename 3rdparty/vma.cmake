voxen_add_library(vma STATIC)
add_library(3rdparty::vma ALIAS vma)

# Not our code, don't bother with its warnings (same as for Catch2)
target_compile_options(vma PRIVATE -w)

# Interface header of this library triggers many of our warnings.
# Mark it as system header to avoid failures in user code.
target_include_directories(vma SYSTEM INTERFACE vma)
target_link_libraries(vma PUBLIC 3rdparty::vulkan-headers)
target_sources(vma PRIVATE vma/vma/vk_mem_alloc.cpp)
