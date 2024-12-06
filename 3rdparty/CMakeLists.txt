# This directory manages third-party Voxen dependencies

# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER 3rdparty)

include(${CMAKE_SOURCE_DIR}/tools/cmake/3rdparty_helpers.cmake)

# Embedded dependencies (sources in tree)
include(backward.cmake)
include(catch.cmake)
include(cxxopts.cmake)
include(fmt.cmake)
include(glfw.cmake)
include(glm.cmake)
include(platform-folders.cmake)
include(simpleini.cmake)
include(vulkan-headers.cmake)
# Depends on `vulkan-headers`, include after them
include(vma.cmake)
include(zlib.cmake)

# Fixup VS generated project IntelliSense in projects
# that set SYSTEM include directories. Should be
# a temporary thing until CMake+VS+ClangCL stabilizes.
voxen_fixup_3rdparty_intellisense(backward)
voxen_fixup_3rdparty_intellisense(cxxopts)
voxen_fixup_3rdparty_intellisense(fmt)
voxen_fixup_3rdparty_intellisense(glfw)
voxen_fixup_3rdparty_intellisense(glm)
voxen_fixup_3rdparty_intellisense(simpleini)
voxen_fixup_3rdparty_intellisense(vma)
voxen_fixup_3rdparty_intellisense(zlib)
