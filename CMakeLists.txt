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

# XXX: ideally `project` should go as early as possible
project(VOXEN LANGUAGES C CXX VERSION 0.1.0)
set(VOXEN_VERSION_SUFFIX "pre-alpha")

# Python is used in buildsystem parts where CMake is too ugly.
# Baseline is the version shipped with Ubuntu 26.04.
find_package(Python3 3.14 REQUIRED COMPONENTS Interpreter)

# Voxen CMake modules, `include(Voxen...)`
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

include(VoxenToolchain)

# Third-party dependencies, vendored and built in-graph
add_subdirectory(3rdparty)

# CMake target setup
voxen_add_library(extras INTERFACE)
voxen_add_library(m INTERFACE)
voxen_add_library(tk STATIC)
voxen_add_library(voxen SHARED)
voxen_add_executable(game "")

find_package(Threads REQUIRED)
target_link_libraries(voxen PUBLIC
	extras
	m
	3rdparty::fmt
	Threads::Threads
)
target_link_libraries(voxen PRIVATE
	tk
	3rdparty::sdl
	3rdparty::vma
	3rdparty::vulkan-headers
)
if (LINUX)
	# `std::stacktrace` implementation in libstdc++
	target_link_libraries(voxen PRIVATE stdc++exp)
endif()
target_link_libraries(tk PRIVATE 3rdparty::llvm-demangle 3rdparty::zlib)
target_link_libraries(game PRIVATE voxen)

include(include/CMakeLists.txt)
include(src/CMakeLists.txt)

option(VOXEN_USE_PCH "Use precompiled headers" ON)
if(VOXEN_USE_PCH)
	target_precompile_headers(voxen PRIVATE <voxen/pch.hpp>)
endif()

add_subdirectory(tools)

option(VOXEN_BUILD_DATA "Build/pack data (assets) files" ON)
if(VOXEN_BUILD_DATA)
	add_subdirectory(data)
endif()

option(VOXEN_BUILD_TESTS "Build the test suite" OFF)
if(VOXEN_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

option(VOXEN_BUILD_SANDBOX "Build research demos from src/sandbox/" OFF)
if(VOXEN_BUILD_SANDBOX)
	add_subdirectory(src/sandbox)
endif()
