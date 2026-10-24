voxen_add_library(llvm-demangle INTERFACE)
add_library(3rdparty::llvm-demangle ALIAS llvm-demangle)

# Header-only, everything is in an anonymous namespace: include it into one TU only
target_include_directories(llvm-demangle SYSTEM INTERFACE llvm-demangle/src)
