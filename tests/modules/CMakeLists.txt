# EXPERIMENT: dummy libraries covering C++ modules cases not present in the engine

# Module exported from a shared library, importing modules of other libraries
voxen_add_library(modshared SHARED)
target_sources(modshared PRIVATE modshared.cpp include/modshared/api.hpp)
target_sources(modshared PUBLIC FILE_SET CXX_MODULES FILES modshared.cppm)
target_include_directories(modshared PUBLIC include)
target_link_libraries(modshared PUBLIC m PRIVATE tk)

# Module of a static library depending on (and re-exporting) the above one
voxen_add_library(modstatic STATIC)
target_sources(modstatic PUBLIC FILE_SET CXX_MODULES FILES modstatic.cppm)
target_link_libraries(modstatic PUBLIC modshared)

voxen_add_executable(test-modules "")
target_sources(test-modules PRIVATE modules.test.cpp)
target_link_libraries(test-modules PRIVATE
	test-common
	modstatic
	tk
	3rdparty::vulkan-headers
)

add_test(NAME modules COMMAND test-modules "[modules]")
