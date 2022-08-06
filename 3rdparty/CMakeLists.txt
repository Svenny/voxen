# This directory manages embedded third-party Voxen dependencies

include(fmt.cmake)
include(platform-folders.cmake)
include(simpleini.cmake)

## Catch2 dependency
add_subdirectory(catch)

## cxxopts dependency
add_subdirectory(cxxopts)
