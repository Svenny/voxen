voxen_add_library(pcg INTERFACE)
add_library(3rdparty::pcg ALIAS pcg)

target_include_directories(pcg SYSTEM INTERFACE pcg/include)
