voxen_add_library(glm INTERFACE)
add_library(3rdparty::glm ALIAS glm)

target_include_directories(glm INTERFACE glm)
