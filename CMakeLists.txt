cmake_minimum_required(VERSION 3.21)

# Can't use `if(GENERATOR_IS_MULTI_CONFIG)` without this line
get_cmake_property(GENERATOR_IS_MULTI_CONFIG GENERATOR_IS_MULTI_CONFIG)
if(GENERATOR_IS_MULTI_CONFIG)
	if(CMAKE_CONFIGURATION_TYPES STREQUAL "")
		message(WARNING "Build types not set, defaulting to Release")
		set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING FORCE)
	endif()
else()
	if(CMAKE_BUILD_TYPE STREQUAL "")
		message(WARNING "Build type not set, defaulting to Release")
		set(CMAKE_BUILD_TYPE Release CACHE STRING FORCE)
	endif()
endif()

# Enable hierarchical project view in VS sln
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# Set game executable startup by default in VS sln
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT game)

# Strange, but CMAKE_CXX_COMPILER_ID don't work before `project`
project(VOXEN LANGUAGES C CXX VERSION 0.1.0)
set(VOXEN_VERSION_SUFFIX "pre-alpha")

# Python is used in buildsystem parts where CMake is too ugly
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

include(tools/cmake/bool_option.cmake)
include(tools/cmake/toolchain_setup.cmake)

# Build third-party dependencies
add_subdirectory(3rdparty)

# CMake target setup
voxen_add_library(extras INTERFACE)
voxen_add_library(voxen SHARED)
voxen_add_executable(game "")

target_compile_definitions(voxen PUBLIC VOXEN_DEBUG_BUILD=$<CONFIG:Debug>)

find_package(Threads REQUIRED)
target_link_libraries(voxen PUBLIC
	extras
	3rdparty::cxxopts
	3rdparty::fmt
	3rdparty::glm
	3rdparty::simpleini
	Threads::Threads
)
target_link_libraries(voxen PRIVATE
	3rdparty::backward
	3rdparty::glfw
	3rdparty::pcg
	3rdparty::platform-folders
	3rdparty::vma
	3rdparty::vulkan-headers
	3rdparty::zlib
)
if (LINUX)
	target_link_libraries(voxen PRIVATE dw)
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
