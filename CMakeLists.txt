cmake_minimum_required(VERSION 4.2)

# Can't use `if(GENERATOR_IS_MULTI_CONFIG)` without this line
get_cmake_property(GENERATOR_IS_MULTI_CONFIG GENERATOR_IS_MULTI_CONFIG)
if(GENERATOR_IS_MULTI_CONFIG)
	if(CMAKE_CONFIGURATION_TYPES STREQUAL "")
		message(WARNING "Build types not set, defaulting to Release")
		set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Available build types" FORCE)
	endif()
else()
	if(CMAKE_BUILD_TYPE STREQUAL "")
		message(WARNING "Build type not set, defaulting to Release")
		set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
	endif()
endif()

# Enable hierarchical project view in VS sln
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# Set game executable startup by default in VS sln
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT game)

# EXPERIMENT: `import std;` support, must be enabled before `project()`.
# The activation UUID changes with CMake versions, see `Help/dev/experimental.rst`.
option(VOXEN_IMPORT_STD "EXPERIMENT: build the `import std;` test" OFF)
if(VOXEN_IMPORT_STD)
	if(CMAKE_VERSION VERSION_LESS 4.3)
		set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
	elseif(CMAKE_VERSION VERSION_LESS 4.4)
		set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
	elseif(CMAKE_VERSION VERSION_LESS 4.5)
		set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
	else()
		message(FATAL_ERROR "Look up `import std` activation UUID for CMake ${CMAKE_VERSION}")
	endif()
endif()

# XXX: ideally `project` should go as early as possible
project(VOXEN LANGUAGES C CXX VERSION 0.1.0)
set(VOXEN_VERSION_SUFFIX "pre-alpha")

# Python is used in buildsystem parts where CMake is too ugly.
# Baseline is the version shipped with Ubuntu 26.04.
find_package(Python3 3.14 REQUIRED COMPONENTS Interpreter)

include(tools/cmake/toolchain_setup.cmake)

# Third-party dependencies, vendored and built in-graph
add_subdirectory(3rdparty)

# CMake target setup
voxen_add_library(extras INTERFACE)
# EXPERIMENT: `m` has a module interface unit, someone has to compile it
voxen_add_library(m STATIC)
voxen_add_library(tk STATIC)
voxen_add_library(voxen SHARED)
voxen_add_executable(game "")

find_package(Threads REQUIRED)
target_link_libraries(voxen PUBLIC
	extras
	m
	3rdparty::fmt
	Threads::Threads
	# EXPERIMENT: public headers import `tk.cli` so consumers need its BMI. It would be
	# nice to link `tk` code into libvoxen only, but CMake does not look for BMIs of
	# `$<COMPILE_ONLY:tk>`: module dependencies must be real link dependencies.
	tk
)
target_link_libraries(voxen PRIVATE
	3rdparty::glfw
	3rdparty::vma
	3rdparty::vulkan-headers
	3rdparty::zlib
)
if (LINUX)
	# `std::stacktrace` implementation in libstdc++
	target_link_libraries(voxen PRIVATE stdc++exp)
endif()
target_link_libraries(game PRIVATE voxen)

include(include/CMakeLists.txt)
include(src/CMakeLists.txt)

option(VOXEN_USE_PCH "Use precompiled headers" ON)
if(VOXEN_USE_PCH AND MSVC)
	# EXPERIMENT: clang-scan-deps silently reports no imports for sources compiled with
	# MSVC-style PCH (`/Yu`, what CMake uses with clang-cl), so their BMIs are never passed
	# and the compilation fails with "module not found". Clang-style PCH on Linux is fine.
	message(WARNING "PCH is incompatible with C++ modules scanning on Windows, disabling it")
	set(VOXEN_USE_PCH OFF)
endif()
if(VOXEN_USE_PCH)
	target_precompile_headers(voxen PRIVATE <voxen/pch.hpp>)
endif()

option(VOXEN_BUILD_DATA "Build/pack data (assets) files" ON)
if(VOXEN_BUILD_DATA)
	add_subdirectory(data)
endif()

option(VOXEN_BUILD_TESTS "Build the test suite" OFF)
if(VOXEN_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
