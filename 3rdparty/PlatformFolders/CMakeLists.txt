add_library(PlatformFolders STATIC sago/platform_folders.cpp)

target_include_directories(PlatformFolders INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})