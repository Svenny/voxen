# This directory manages embedded third-party Voxen dependencies

include(fmt.cmake)
include(simpleini.cmake)

## PlatformFolders dependency
add_subdirectory(PlatformFolders)

## Catch2 dependency
add_subdirectory(catch)

## cxxopts dependency
add_subdirectory(cxxopts)
