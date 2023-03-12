# This directory manages third-party Voxen dependencies

# External dependencies (system libraries)
# Imported targets created by `find_package` are not visible in
# parent directory so use an additional `-wrapper` pseudo target

find_package(glfw3 3.3.2 REQUIRED CONFIG)
add_library(glfw-wrapper INTERFACE)
target_link_libraries(glfw-wrapper INTERFACE glfw)
add_library(3rdparty::glfw ALIAS glfw-wrapper)

find_package(ZLIB 1.2.11 REQUIRED)
add_library(zlib-wrapper INTERFACE)
target_link_libraries(zlib-wrapper INTERFACE ZLIB::ZLIB)
add_library(3rdparty::zlib ALIAS zlib-wrapper)

# Embedded dependencies (sources in tree)
include(catch.cmake)
include(cxxopts.cmake)
include(fmt.cmake)
include(glm.cmake)
include(platform-folders.cmake)
include(simpleini.cmake)
include(vulkan-headers.cmake)
