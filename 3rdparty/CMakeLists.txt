# This directory manages embedded third-party Voxen dependencies

include(cxxopts.cmake)
include(fmt.cmake)
include(platform-folders.cmake)
include(simpleini.cmake)

## Catch2 dependency
add_subdirectory(catch)
