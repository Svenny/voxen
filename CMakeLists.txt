cmake_minimum_required(VERSION 3.21)

# Can't use `if(GENERATOR_IS_MULTI_CONFIG)` without this line
get_cmake_property(GENERATOR_IS_MULTI_CONFIG GENERATOR_IS_MULTI_CONFIG)

# Strange, but CMAKE_CXX_COMPILER_ID don't work before `project`
project(VOXEN LANGUAGES C CXX VERSION 0.1.0)
set(VOXEN_VERSION_SUFFIX " pre-alpha")
set(VOXEN_VERSION_STRING "${VOXEN_VERSION_MAJOR}.${VOXEN_VERSION_MINOR}.${VOXEN_VERSION_PATCH}${VOXEN_VERSION_SUFFIX}")

include(tools/cmake/bool_option.cmake)
include(tools/cmake/toolchain_setup.cmake)

if(CMAKE_BUILD_TYPE STREQUAL "" AND NOT GENERATOR_IS_MULTI_CONFIG)
	message(WARNING "Build type not set, defaulting to Release")
	set(CMAKE_BUILD_TYPE Release CACHE STRING FORCE)
endif()

bool_option(USE_VULKAN_DEBUGGING "Enable Vulkan debugging features (validation layers, debug extensions)" OFF)
bool_option(USE_INTEGRATED_GPU "Target integrated GPU instead of discrete one" OFF)
bool_option(DEPLOY_BUILD "Build for creating installable version of Voxen" OFF)

# Build third-party dependencies
add_subdirectory(3rdparty)

# CMake target setup
voxen_add_library(extras STATIC)
voxen_add_library(voxen SHARED)
voxen_add_executable(game)

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
	3rdparty::glfw
	3rdparty::platform-folders
	3rdparty::vulkan-headers
	3rdparty::zlib
)
target_link_libraries(game PRIVATE voxen)

include(include/CMakeLists.txt)
include(src/CMakeLists.txt)

option(VOXEN_USE_PCH "Use precompiled headers" ON)
if(VOXEN_USE_PCH)
	target_precompile_headers(voxen PRIVATE <voxen/pch.hpp>)
endif()

option(VOXEN_BUILD_TESTS "Build tests for some small parts of codebase" OFF)
if(VOXEN_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
