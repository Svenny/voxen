voxen_add_library(fmt SHARED)
add_library(3rdparty::fmt ALIAS fmt)

target_compile_definitions(fmt PRIVATE FMT_LIB_EXPORT)
# Interface header of this library violates -Wdeprecated
# Mark it as system header to avoid failures in user code
target_include_directories(fmt SYSTEM PUBLIC fmt/include)
target_link_libraries(fmt PRIVATE dummy_suppress_3rdparty_warnings)
target_sources(fmt PRIVATE fmt/src/format.cc fmt/src/os.cc)
