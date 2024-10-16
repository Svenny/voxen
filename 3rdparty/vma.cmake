voxen_add_library(vma STATIC)
add_library(3rdparty::vma ALIAS vma)

target_compile_options(vma PRIVATE
	-Wno-cast-align
	-Wno-cast-function-type-strict
	-Wno-covered-switch-default
	-Wno-extra-semi-stmt
	-Wno-inconsistent-missing-destructor-override
	-Wno-missing-field-initializers
	-Wno-nullability-completeness
	-Wno-nullability-extension
	-Wno-old-style-cast
	-Wno-sign-conversion
	-Wno-suggest-destructor-override
	-Wno-switch-default
	-Wno-switch-enum
	-Wno-undef
	-Wno-unreachable-code-fallthrough
	-Wno-unused-function
	-Wno-unused-parameter
	-Wno-unused-template
	-Wno-unused-variable
	-Wno-weak-vtables
)

# Interface header of this library violates some of the above diagnostics.
# Mark it as system header to avoid failures in user code.
target_include_directories(vma SYSTEM INTERFACE vma)
target_link_libraries(vma PUBLIC 3rdparty::vulkan-headers)
target_sources(vma PRIVATE vma/vma/vk_mem_alloc.cpp)
