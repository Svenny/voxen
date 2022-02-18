cmake_minimum_required(VERSION 3.16)

# include file with some cmake utils
include(CMakeUtils.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Strange, but CMAKE_CXX_COMPILER_ID don't work before `project`
project(VOXEN LANGUAGES C CXX VERSION 0.1.0)
set(VOXEN_VERSION_SUFFIX " pre-alpha")
set(VOXEN_VERSION_STRING "${VOXEN_VERSION_MAJOR}.${VOXEN_VERSION_MINOR}.${VOXEN_VERSION_PATCH}${VOXEN_VERSION_SUFFIX}")

# Enforce good coding standards to at least some extent
string(APPEND CMAKE_C_FLAGS " -Wall -Wextra -Wpedantic -Werror")
string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wpedantic -Werror")

# Clang's IPO uses ThinLTO which does not slow down build times as severely as usual -flto
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
endif()

# Always use LLD because why not
string(APPEND CMAKE_EXE_LINKER_FLAGS " -fuse-ld=lld")
string(APPEND CMAKE_SHARED_LINKER_FLAGS " -fuse-ld=lld")

option(VOXEN_FASTER_MATH_FLAGS "Speed up math at the of cost strict standard compliance" OFF)
if(VOXEN_FASTER_MATH_FLAGS)
	string(APPEND CMAKE_C_FLAGS " -fno-math-errno -fno-signed-zeros -fno-trapping-math -ffp-contract=fast-honor-pragmas")
	string(APPEND CMAKE_CXX_FLAGS " -fno-math-errno -fno-signed-zeros -fno-trapping-math -ffp-contract=fast-honor-pragmas")
endif()

if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0) OR
   (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0))
	# Remap /path/to/voxen/src/file.cpp -> /src/file.cpp in __FILE__ and co.
	string(APPEND CMAKE_C_FLAGS " -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=''")
	string(APPEND CMAKE_CXX_FLAGS " -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=''")
endif()

# Enable filesystem, if needed
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
	string(APPEND CMAKE_CXX_FLAGS " -lstdc++fs")
endif()

# Always link with libc++ because why not
string(APPEND CMAKE_CXX_FLAGS " -stdlib=libc++")

# This will teach the built binary to load .so's from its own directory
set(CMAKE_INSTALL_RPATH "\$ORIGIN")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

if(CMAKE_BUILD_TYPE STREQUAL "" AND NOT GENERATOR_IS_MULTI_CONFIG)
	message(WARNING "Build type not set, defaulting to Release")
	set(CMAKE_BUILD_TYPE Release CACHE STRING FORCE)
endif()

bool_option(USE_VULKAN_DEBUGGING "Enable Vulkan debugging features (validation layers, debug extensions)" OFF)
bool_option(USE_INTEGRATED_GPU "Target integrated GPU instead of discrete one" OFF)
bool_option(DEPLOY_BUILD "Build for creating installable version of Voxen" OFF)

# Build external dependencies
add_subdirectory(3rdparty)

# CMake target setup
add_executable(voxen "")

target_include_directories(voxen PUBLIC 3rdparty/glm 3rdparty/vulkan-headers)
target_compile_definitions(voxen PUBLIC VOXEN_DEBUG_BUILD=$<CONFIG:Debug>)

find_package(glfw3 3.3.2 REQUIRED CONFIG)
find_package(Threads REQUIRED)
target_link_libraries(voxen PUBLIC glfw fmt::fmt stdc++fs cxxopts SimpleIni Threads::Threads PlatformFolders)

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
