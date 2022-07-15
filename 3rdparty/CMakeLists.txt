# This cmake file are managing embedded 3party Voxen dependencies

## fmt dependency
include(fmt.cmake)

## PlatformFolders dependency
add_subdirectory(PlatformFolders)

## Catch2 dependency
add_subdirectory(catch)

## cxxopts dependency
add_subdirectory(cxxopts)

## SimpleInit dependency
add_subdirectory(simpleini-4.17)
