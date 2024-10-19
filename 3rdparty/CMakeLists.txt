# This directory manages third-party Voxen dependencies

# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER 3rdparty)

# External dependencies (system libraries)
# Imported targets created by `find_package` are not visible in
# parent directory so use an additional `-wrapper` pseudo target

find_package(glfw3 3.3.2 REQUIRED CONFIG)
add_library(glfw-wrapper INTERFACE)
target_link_libraries(glfw-wrapper INTERFACE glfw)
add_library(3rdparty::glfw ALIAS glfw-wrapper)

# Embedded dependencies (sources in tree)
include(catch.cmake)
include(cxxopts.cmake)
include(fmt.cmake)
include(glm.cmake)
include(platform-folders.cmake)
include(simpleini.cmake)
include(vulkan-headers.cmake)
# Depends on `vulkan-headers`, include after them
include(vma.cmake)
include(zlib.cmake)
