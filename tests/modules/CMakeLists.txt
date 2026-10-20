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

if(VOXEN_IMPORT_STD)
	voxen_add_library(modstd STATIC)
	target_sources(modstd PUBLIC FILE_SET CXX_MODULES FILES modstd.cppm)
	target_link_libraries(modstd PRIVATE tk)

	voxen_add_executable(test-import-std "")
	target_sources(test-import-std PRIVATE import_std.test.cpp)
	target_link_libraries(test-import-std PRIVATE test-common modstd tk)
	if(MSVC)
		# CMake supports `import std;` with Clang only for libc++ and libstdc++, not MS STL.
		# clang-cl does build MS STL `std.ixx` though, so do what CMake does for MSVC by hand.
		cmake_path(SET std_dir NORMALIZE "$ENV{VCToolsInstallDir}/modules")
		if(NOT EXISTS "${std_dir}/std.ixx")
			message(FATAL_ERROR "`std.ixx` not found in `${std_dir}`, run from VS developer environment")
		endif()
		voxen_add_library(voxen-std STATIC)
		target_sources(voxen-std PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${std_dir}" FILES "${std_dir}/std.ixx")
		# Not our code, and it includes STL headers in the module purview on purpose
		target_compile_options(voxen-std PRIVATE -w)
		target_link_libraries(modstd PUBLIC voxen-std)
	else()
		set_target_properties(modstd test-import-std PROPERTIES CXX_MODULE_STD ON)
		# CMake 4.4 builds the `std` module in a synthesized target per consumer, with its flags.
		# libstdc++ `std.cc` trips our warnings: `-Wreserved-module-identifier` (for `export
		# module std;` itself) and `-Wundef` (`#if __cpp_lib_...` of not implemented features).
		target_compile_options(modstd PRIVATE -Wno-reserved-module-identifier -Wno-undef)
		target_compile_options(test-import-std PRIVATE -Wno-reserved-module-identifier -Wno-undef)
	endif()

	add_test(NAME modules-import-std COMMAND test-import-std "[modules::import_std]")
endif()
