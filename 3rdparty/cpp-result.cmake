voxen_add_library(cpp-result INTERFACE)
add_library(3rdparty::cpp-result ALIAS cpp-result)

target_include_directories(cpp-result INTERFACE cpp-result)
