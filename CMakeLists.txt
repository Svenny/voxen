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

# XXX: ideally `project` should go as early as possible
project(VOXEN LANGUAGES C CXX VERSION 0.1.0)
set(VOXEN_VERSION_SUFFIX "pre-alpha")

# Python is used in buildsystem parts where CMake is too ugly.
# Baseline is the version shipped with Ubuntu 26.04.
find_package(Python3 3.14 REQUIRED COMPONENTS Interpreter)

include(tools/cmake/toolchain_setup.cmake)

# Build third-party dependencies
add_subdirectory(3rdparty)

# CMake target setup
voxen_add_library(extras INTERFACE)
voxen_add_library(m INTERFACE)
voxen_add_library(tk STATIC)
voxen_add_library(voxen SHARED)
voxen_add_executable(game "")

add_dependencies(voxen 3rdparty-pull-binaries)
target_compile_definitions(voxen PUBLIC VOXEN_DEBUG_BUILD=$<CONFIG:Debug>)

find_package(Threads REQUIRED)
target_link_libraries(voxen PUBLIC
	extras
	m
	3rdparty::fmt
	3rdparty::glm
	Threads::Threads
)
target_link_libraries(voxen PRIVATE
	tk
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
if(VOXEN_USE_PCH)
	target_precompile_headers(voxen PRIVATE <voxen/pch.hpp>)
endif()

option(VOXEN_BUILD_DATA "Build/pack data (assets) files" ON)
if(VOXEN_BUILD_DATA)
	add_subdirectory(data)
endif()

option(VOXEN_BUILD_TESTS "Build tests for some small parts of codebase" OFF)
if(VOXEN_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
