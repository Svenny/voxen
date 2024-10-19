# Stripped down rework of zlib/CMakeLists.txt
voxen_add_library(zlib SHARED)
add_library(3rdparty::zlib ALIAS zlib)

# Add SYSTEM property, zlib headers trigger some compiler warnings
target_include_directories(zlib SYSTEM PUBLIC zlib)
target_link_libraries(zlib PRIVATE dummy_suppress_3rdparty_warnings)

# Make it export symbols
set_target_properties(zlib PROPERTIES
	C_VISIBILITY_PRESET default # For linux
	DEFINE_SYMBOL ZLIB_DLL # For windows
)

target_compile_options(zlib PRIVATE
	-Wno-cast-align
	-Wno-cast-qual
	-Wno-comma
	-Wno-covered-switch-default
	-Wno-format-nonliteral
	-Wno-missing-prototypes
	-Wno-missing-variable-declarations
)

if(LINUX)
	# Without it it will just use read/write/etc. without declarations
	target_compile_definitions(zlib PRIVATE -DHAVE_UNISTD_H)
endif()

target_sources(zlib PRIVATE
	zlib/zconf.h
	# We should generate it from zlib.h.cmakein... but hey this works too
	zlib/zlib.h

	zlib/crc32.h
	zlib/deflate.h
	zlib/gzguts.h
	zlib/inffast.h
	zlib/inffixed.h
	zlib/inflate.h
	zlib/inftrees.h
	zlib/trees.h
	zlib/zutil.h

	zlib/adler32.c
	zlib/compress.c
	zlib/crc32.c
	zlib/deflate.c
	zlib/gzclose.c
	zlib/gzlib.c
	zlib/gzread.c
	zlib/gzwrite.c
	zlib/inflate.c
	zlib/infback.c
	zlib/inftrees.c
	zlib/inffast.c
	zlib/trees.c
	zlib/uncompr.c
	zlib/zutil.c
)
