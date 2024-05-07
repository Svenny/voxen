voxen_add_library(vma STATIC)
add_library(3rdparty::vma ALIAS vma)

target_compile_options(vma PRIVATE
	-Wno-missing-field-initializers
	-Wno-nullability-completeness
	-Wno-nullability-extension
	-Wno-sign-conversion
	-Wno-undef
	-Wno-unused-function
	-Wno-unused-parameter
	-Wno-unused-variable
	-Wno-weak-vtables
)

# Interface header of this library violates some of the above diagnostics.
# Mark it as system header to avoid failures in user code.
target_include_directories(vma SYSTEM INTERFACE vma)
target_link_libraries(vma PUBLIC 3rdparty::vulkan-headers)
target_sources(vma PRIVATE vma/vma/vk_mem_alloc.cpp)
