cmake_minimum_required(VERSION 3.16)

# Inbox external dependencies will be installed here
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/3rdparty")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Enforce good coding standards to at least some extent
string(APPEND CMAKE_C_FLAGS " -Wall -Wextra -Wpedantic -Werror")
string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wpedantic -Werror")
# RTTI is only useful for dynamic_cast, which means a design flaw in 99,9% cases
string(APPEND CMAKE_CXX_FLAGS " -fno-rtti")
# Remap /path/to/voxen/src/file.cpp -> /src/file.cpp in __FILE__ and co.
string(APPEND CMAKE_C_FLAGS " -ffile-prefix-map=${CMAKE_SOURCE_DIR}/=''")
string(APPEND CMAKE_CXX_FLAGS " -ffile-prefix-map=${CMAKE_SOURCE_DIR}/=''")

# This will teach the built binary to load .so's from its own directory
set(CMAKE_INSTALL_RPATH "\$ORIGIN")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

if(CMAKE_BUILD_TYPE STREQUAL "" AND NOT GENERATOR_IS_MULTI_CONFIG)
	message(WARNING "Build type not set, defaulting to Release")
	set(CMAKE_BUILD_TYPE Release CACHE STRING FORCE)
endif()

project(VOXEN LANGUAGES C CXX VERSION 0.1.0)
set(VOXEN_VERSION_SUFFIX " pre-alpha")

# Build external dependencies
if (UNIX)
    set (3RDPARTY_COMMON_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
    message (STATUS "Building external dependencies:")
    execute_process (COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/build.sh
        --build-dir ${CMAKE_BINARY_DIR} --cmake-cmd ${CMAKE_COMMAND} -j 16
        -- ${3RDPARTY_COMMON_ARGS}
        RESULT_VARIABLE 3RDPARTY_BUILD_RESULT)
    if (3RDPARTY_BUILD_RESULT)
        message (FATAL_ERROR "Building external dependencies failed!")
    endif ()
else ()
    message (FATAL_ERROR "Dependency management is not implemented for non-Unix platforms")
endif ()

# CMake target setup
add_executable(voxen "")

target_compile_definitions(voxen PUBLIC
	-DVOXEN_VERSION_MAJOR=${VOXEN_VERSION_MAJOR}
	-DVOXEN_VERSION_MINOR=${VOXEN_VERSION_MINOR}
	-DVOXEN_VERSION_PATCH=${VOXEN_VERSION_PATCH}
	-DVOXEN_VERSION_SUFFIX="${VOXEN_VERSION_SUFFIX}"
	-DVOXEN_VERSION_STRING="${VOXEN_VERSION_MAJOR}.${VOXEN_VERSION_MINOR}.${VOXEN_VERSION_PATCH}${VOXEN_VERSION_SUFFIX}"
)
target_include_directories(voxen PUBLIC include 3rdparty/glm)

find_package(glfw3 3.3.2 REQUIRED CONFIG)
find_package(fmt 6.2.0 REQUIRED CONFIG)
find_package(Vulkan REQUIRED)
target_link_libraries(voxen PUBLIC glfw fmt::fmt Vulkan::Vulkan)

include(include/CMakeLists.txt)
include(src/CMakeLists.txt)

# Enable precompiled headers only in Release builds
target_precompile_headers(voxen PRIVATE $<$<CONFIG:Release>:<voxen/pch.hpp$<ANGLE-R>>)
