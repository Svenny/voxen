voxen_add_3rdparty_library(harfbuzz)

# Single-TU build, `harfbuzz.cc` includes every source file of the core library.
# Optional backends are enabled by `HAVE_*` macros, only the FreeType font
# functions (`hb-ft.h`) are needed. Mutexes fall back to `std::mutex`.
target_sources(harfbuzz PRIVATE harfbuzz/src/harfbuzz.cc)
target_compile_definitions(harfbuzz PRIVATE HAVE_FREETYPE=1)
target_include_directories(harfbuzz SYSTEM PUBLIC harfbuzz/src)
target_link_libraries(harfbuzz PRIVATE 3rdparty::freetype)
