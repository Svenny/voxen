include(${CMAKE_SOURCE_DIR}/tools/cmake/3rdparty_helpers.cmake)

voxen_add_library(platform-folders STATIC)
add_library(3rdparty::platform-folders ALIAS platform-folders)

target_include_directories(platform-folders INTERFACE platform-folders)
target_link_libraries(platform-folders PRIVATE dummy_suppress_3rdparty_warnings)
target_sources(platform-folders PRIVATE platform-folders/sago/platform_folders.cpp)
