voxen_add_library(fmt STATIC)
add_library(3rdparty::fmt ALIAS fmt)

target_include_directories(fmt PUBLIC fmt/include)
target_sources(fmt PRIVATE fmt/src/format.cc fmt/src/os.cc)
