# Catch2 v3 is built as a regular (non-amalgamated) static library,
# this compiles faster than including the amalgamated header everywhere

# Generate user config header the same way Catch2's own CMake does
set(CATCH_CONFIG_DEFAULT_REPORTER "console")
set(CATCH_CONFIG_CONSOLE_WIDTH "80")
# Capture stdout/stderr output into test reports
set(CATCH_CONFIG_EXPERIMENTAL_REDIRECT ON)
configure_file(catch2/src/catch2/catch_user_config.hpp.in catch2-generated/catch2/catch_user_config.hpp)

file(GLOB_RECURSE CATCH2_SOURCES CONFIGURE_DEPENDS catch2/src/catch2/*.cpp)
list(FILTER CATCH2_SOURCES EXCLUDE REGEX "/catch_main\\.cpp$")

voxen_add_3rdparty_library(catch2)
target_sources(catch2 PRIVATE ${CATCH2_SOURCES})
target_include_directories(catch2 SYSTEM PUBLIC catch2/src ${CMAKE_CURRENT_BINARY_DIR}/catch2-generated)

# Default `main()` function implementation
voxen_add_3rdparty_library(catch2-main)
target_sources(catch2-main PRIVATE catch2/src/catch2/internal/catch_main.cpp)
target_link_libraries(catch2-main PUBLIC catch2)
