voxen_add_library(catch2 INTERFACE)
add_library(3rdparty::catch2 ALIAS catch2)

target_include_directories(catch2 INTERFACE catch)
