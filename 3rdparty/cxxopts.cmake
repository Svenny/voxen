voxen_add_library(cxxopts INTERFACE)
add_library(3rdparty::cxxopts ALIAS cxxopts)

# Interface header of this library violates -Wdeprecated and -Wweak-vtables
# Mark it as system header to avoid failures in user code
target_include_directories(cxxopts SYSTEM INTERFACE cxxopts)
