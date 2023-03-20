voxen_add_library(fmt SHARED)
add_library(3rdparty::fmt ALIAS fmt)

target_compile_definitions(fmt PRIVATE FMT_EXPORT)
target_include_directories(fmt PUBLIC fmt/include)
target_sources(fmt PRIVATE fmt/src/format.cc fmt/src/os.cc)
