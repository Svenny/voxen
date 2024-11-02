voxen_add_library(json5cpp INTERFACE)
add_library(3rdparty::json5cpp ALIAS json5cpp)

# This code triggers some compiler warnings.
# Mark as system headers to avoid failures in user code.
target_include_directories(json5cpp SYSTEM INTERFACE json5cpp)
# This library is just a thin layer over jsoncpp
target_link_libraries(json5cpp INTERFACE 3rdparty::jsoncpp)
