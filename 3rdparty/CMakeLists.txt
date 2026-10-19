# Third-party dependencies. Sources are vendored into `3rdparty/<name>` by
# `tools/maintenance/vendor.py` from `vendor.toml`, each `<name>.cmake` builds
# them in-graph as static libraries or interface targets for header-only ones.
# Only the parts we actually use are built, these files do not try to mirror
# the upstream build systems.

# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER 3rdparty)

# Compiled dependencies get the full engine toolchain setup (flags, configurations,
# sanitizers, LTO...) but none of our warnings, it's not our code to fix. `-w` works
# in clang-cl too, and also silences warnings libraries escalate to errors with
# `#pragma GCC diagnostic error`.
function(voxen_add_3rdparty_library name)
	voxen_add_library(${name} STATIC)
	add_library(3rdparty::${name} ALIAS ${name})
	target_compile_options(${name} PRIVATE -w)
endfunction()

include(catch2.cmake)
include(fmt.cmake)
include(freetype.cmake)
include(glfw.cmake)
# Depends on `freetype`, include after it
include(harfbuzz.cmake)
include(vulkan-headers.cmake)
# Depends on `vulkan-headers`, include after them
include(vma.cmake)
include(zlib.cmake)
