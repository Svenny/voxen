# Catch2 v3 is built as a regular (non-amalgamated) static library,
# this compiles faster than including the amalgamated header everywhere

# Generate user config header the same way Catch2's own CMake does
set(CATCH_CONFIG_DEFAULT_REPORTER "console")
set(CATCH_CONFIG_CONSOLE_WIDTH "80")
# Capture stdout/stderr output into test reports
set(CATCH_CONFIG_EXPERIMENTAL_REDIRECT ON)
configure_file(catch2/catch2/catch_user_config.hpp.in catch2-generated/catch2/catch_user_config.hpp)

file(GLOB_RECURSE CATCH2_SOURCES CONFIGURE_DEPENDS catch2/catch2/*.cpp)
list(FILTER CATCH2_SOURCES EXCLUDE REGEX "/catch_main\\.cpp$")

voxen_add_library(catch2 STATIC)
add_library(3rdparty::catch2 ALIAS catch2)

target_sources(catch2 PRIVATE ${CATCH2_SOURCES})
# Not our code, don't bother with its warnings. This includes platform-specific
# code paths, so pinpoint suppression would be fragile. `-w` works in clang-cl too.
target_compile_options(catch2 PRIVATE -w)
target_include_directories(catch2 SYSTEM PUBLIC catch2 ${CMAKE_CURRENT_BINARY_DIR}/catch2-generated)

# Default `main()` function implementation
voxen_add_library(catch2-main STATIC)
add_library(3rdparty::catch2-main ALIAS catch2-main)

target_sources(catch2-main PRIVATE catch2/catch2/internal/catch_main.cpp)
target_compile_options(catch2-main PRIVATE -w)
target_link_libraries(catch2-main PUBLIC catch2)
