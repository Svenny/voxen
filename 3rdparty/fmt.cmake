voxen_add_3rdparty_library(fmt)

# `fmt/os.h` is not used, so `src/os.cc` is not built
target_sources(fmt PRIVATE fmt/src/format.cc)
target_include_directories(fmt SYSTEM PUBLIC fmt/include)
