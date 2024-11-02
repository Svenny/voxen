# This directory manages third-party Voxen dependencies

# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER 3rdparty)

include(${CMAKE_SOURCE_DIR}/tools/cmake/3rdparty_helpers.cmake)

# Embedded dependencies (sources in tree)
include(catch.cmake)
include(cxxopts.cmake)
include(fmt.cmake)
include(glfw.cmake)
include(glm.cmake)
include(jsoncpp.cmake)
include(json5cpp.cmake)
include(platform-folders.cmake)
include(simpleini.cmake)
include(vulkan-headers.cmake)
# Depends on `vulkan-headers`, include after them
include(vma.cmake)
include(zlib.cmake)
