voxen_add_library(backward INTERFACE)
add_library(3rdparty::backward ALIAS backward)

# Interface header of this library violates -Wundef, -Wsign* and -Wweak-vtables
# Mark it as system header to avoid failures in user code
target_include_directories(backward SYSTEM INTERFACE backward)
