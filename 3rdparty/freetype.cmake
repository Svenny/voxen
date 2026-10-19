voxen_add_3rdparty_library(freetype)

# Build options live in the vendored (patched) `include/freetype/config/ftoption.h`,
# the module list in `ftmodule.h` next to it. Keep this source list in sync with
# the latter: TrueType/OpenType (CFF) font drivers, the auto-hinter and the
# anti-aliasing and SDF renderers. No compressed fonts (no zlib), no SVG.
target_sources(freetype PRIVATE
	# Base: core + the public API pieces that are useful
	freetype/src/base/ftbase.c
	freetype/src/base/ftbbox.c
	freetype/src/base/ftbitmap.c
	freetype/src/base/ftglyph.c
	freetype/src/base/ftinit.c
	freetype/src/base/ftmm.c
	freetype/src/base/ftstroke.c
	freetype/src/base/ftsynth.c
	# Portable stdio-based system interface and debug stubs
	freetype/src/base/ftsystem.c
	freetype/src/base/ftdebug.c
	# Modules
	freetype/src/autofit/autofit.c
	freetype/src/cff/cff.c
	freetype/src/psaux/psaux.c
	freetype/src/pshinter/pshinter.c
	freetype/src/psnames/psnames.c
	freetype/src/sdf/sdf.c
	freetype/src/sfnt/sfnt.c
	freetype/src/smooth/smooth.c
	freetype/src/truetype/truetype.c
)
target_compile_definitions(freetype PRIVATE FT2_BUILD_LIBRARY)
target_include_directories(freetype SYSTEM PUBLIC freetype/include)
