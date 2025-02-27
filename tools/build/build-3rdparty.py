#!/usr/bin/env python3
"""
Build all external dependencies and store them in a consumable "package".

TODO: while build jobs should be mostly parallel internally (save for linking)
CMake configure jobs are single-threaded and can be parallelized manually,
watching out for inter-library dependencies.
When more dependencies will be added, this might become relevant on Windows
with its excruciatingly slow file IO and process startup.
"""

import os
import pathlib
import subprocess
import sys

if len(sys.argv) != 4:
	print('Usage: python3 build-3rdparty.py <3rdparty-dir> <temp-dir> <package-dir>')
	sys.exit(1)

path_3rdparty = pathlib.Path(sys.argv[1]).resolve()
path_temp = pathlib.Path(sys.argv[2]).resolve()
path_package = pathlib.Path(sys.argv[3]).resolve()

print('3rdparty directory:', path_3rdparty)
print('Temp directory:', path_temp)
print('Package directory:', path_package)

cmake_generator_args = []
standard_clang_flags = ''
standard_linker_flags = ''

if sys.platform == 'linux':
	cmake_generator_args = [
		'-GNinja', '-DCMAKE_BUILD_TYPE=Release',
		'-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++'
	]
	# TODO: unify with `toolchain_setup.cmake`
	standard_clang_flags = '-march=x86-64-v3 -fno-plt -fno-semantic-interposition -flto=thin'
	standard_linker_flags = '-fuse-ld=mold -Bsymbolic-functions --no-undefined -flto=thin'
elif sys.platform == 'win32':
	cmake_generator_args = [ '-GVisual Studio 17 2022', '-Ax64', '-TClangCL', '-DCMAKE_CONFIGURATION_TYPES=Release' ]
	# TODO: unify with `toolchain_setup.cmake`
	standard_clang_flags = '/clang:-march=x86-64-v3 /clang:-flto=thin /EHsc /showFilenames /MP'
else:
	print('Unknown build platform!')
	sys.exit(1)

cmake_standard_args = [
	*cmake_generator_args,
	f'-DCMAKE_INSTALL_PREFIX={path_package}', f'-DCMAKE_PREFIX_PATH={path_package}',
	#f'-DCMAKE_C_COMPILER={c_compiler}', f'-DCMAKE_CXX_COMPILER={cxx_compiler}',
	f'-DCMAKE_C_FLAGS={standard_clang_flags}', f'-DCMAKE_CXX_FLAGS={standard_clang_flags}',
	f'-DCMAKE_EXE_LINKER_FLAGS={standard_linker_flags}', f'-DCMAKE_SHARED_LINKER_FLAGS={standard_linker_flags}',
	f'-DCMAKE_MODULE_LINKER_FLAGS={standard_linker_flags}'
]

def build_standard(lib_name: str, specific_args: list, override_build_path: str = None):
	path_source = str(path_3rdparty / lib_name)
	path_build = str(path_temp / lib_name) if override_build_path is None else str(override_build_path)

	print()
	print('=====')
	print('Now building:', lib_name)
	print('Source path:', path_source)
	print('Build path:', path_build)
	print()

	subprocess.run([ 'cmake', *cmake_standard_args, '-S', path_source, '-B', path_build, *specific_args ], check=True)
	subprocess.run([ 'cmake', '--build', path_build, '--config', 'Release', '--target', 'install' ], check=True)

def build_temporary_freetype():
	path_build = str(path_temp / 'freetype-temp')
	path_install = str(path_temp / 'freetype-temp-install')

	build_standard('freetype', [
		f'-DCMAKE_INSTALL_PREFIX={path_install}',
		'-DFT_DISABLE_BZIP2=ON', '-DFT_DISABLE_BROTLI=ON', '-DFT_DISABLE_HARFBUZZ=ON',
		'-DFT_REQUIRE_ZLIB=ON', '-DFT_REQUIRE_PNG=ON', '-DBUILD_SHARED_LIBS=ON'
	], path_build)

# Many other libs depend on this little boy, build it first
build_standard('zlib', [ '-DZLIB_BUILD_EXAMPLES=OFF', '-DBUILD_SHARED_LIBS=ON' ])

# LibPNG depends on zlib, don't order before it
build_standard('libpng', [ '-DPNG_TESTS=OFF', '-DPNG_TOOLS=OFF', '-DPNG_STATIC=OFF', '-DSKIP_INSTALL_EXECUTABLES=ON' ])

# FreeType depends on zlib and libpng, don't order before them.
#
# Also it has HarfBuzz integration but this forms a cyclic dependency.
# Start by building a temporary copy of FreeType wihout this integration.
#
# Use temporary directories. Later we will build the proper variant,
# and we don't want to put that in the same location, as then this
# script would reconfigure and rebuild libs on every run.
build_temporary_freetype()

# HarfBuzz has integration with FreeType that we need, so build after it.
# Make sure to use temporary freetype location, not the package directory.
build_standard('harfbuzz', [
	f'-DCMAKE_PREFIX_PATH={str(path_temp / 'freetype-temp-install')}',
	'-DHB_HAVE_FREETYPE=ON', '-DHB_BUILD_SUBSET=OFF', '-DBUILD_SHARED_LIBS=ON'
])

# Now that we have HarfBuzz build FreeType again with integration (for better auto-hinting)
build_standard('freetype', [
	'-DFT_DISABLE_BZIP2=ON', '-DFT_DISABLE_BROTLI=ON',
	'-DFT_REQUIRE_ZLIB=ON', '-DFT_REQUIRE_PNG=ON', '-DFT_REQUIRE_HARFBUZZ=ON',
	'-DBUILD_SHARED_LIBS=ON',
])

# Independent libraries

build_standard('fmt', [ '-DBUILD_SHARED_LIBS=ON', '-DFMT_DOC=OFF', '-DFMT_TEST=OFF' ])
build_standard('jsoncpp', [ '-DBUILD_SHARED_LIBS=ON',
	'-DJSONCPP_WITH_TESTS=OFF', '-DBUILD_STATIC_LIBS=OFF', '-DBUILD_OBJECT_LIBS=OFF'
])
build_standard('glfw', [ '-DBUILD_SHARED_LIBS=ON',
	'-DGLFW_BUILD_DOCS=OFF', '-DGLFW_BUILD_TESTS=OFF', '-DGLFW_BUILD_DOCS=OFF', '-DGLFW_BUILD_EXAMPLES=OFF'
])
