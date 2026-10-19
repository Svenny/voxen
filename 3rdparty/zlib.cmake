voxen_add_3rdparty_library(zlib)

# Deflate, inflate and crc32 only, no gzip file API
target_sources(zlib PRIVATE
	zlib/adler32.c
	zlib/crc32.c
	zlib/deflate.c
	zlib/inffast.c
	zlib/inflate.c
	zlib/inftrees.c
	zlib/trees.c
	zlib/zutil.c
)
target_include_directories(zlib SYSTEM PUBLIC zlib)
