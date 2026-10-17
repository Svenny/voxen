#!/usr/bin/env python3
"""
Build all external dependencies and store them in a consumable "package".

TODO: while build jobs should be mostly parallel internally (save for linking)
CMake configure jobs are single-threaded and can be parallelized manually,
watching out for inter-library dependencies.
When more dependencies will be added, this might become relevant on Windows
with its excruciatingly slow file IO and process startup.
"""

import argparse
import pathlib
import subprocess
import sys

parser = argparse.ArgumentParser(description='Build all external dependencies into a package directory.',
	epilog='Any extra `-D...` arguments are passed to CMake configure step of every dependency.')
parser.add_argument('path_3rdparty', type=pathlib.Path, help='3rdparty sources directory')
parser.add_argument('path_temp', type=pathlib.Path, help='temporary build directory')
parser.add_argument('path_package', type=pathlib.Path, help='package (installation) directory')
parser.add_argument('--cmake', default='cmake', help='CMake executable to use')
parser.add_argument('--compile-flags', default='', help='C/C++ compiler flags shared with the main build')
parser.add_argument('--linker-flags', default='', help='linker flags shared with the main build')
args, extra_cmake_args = parser.parse_known_args()

for arg in extra_cmake_args:
	if not arg.startswith('-D'):
		parser.error(f'unrecognized argument: {arg}')

path_3rdparty = args.path_3rdparty.resolve()
path_temp = args.path_temp.resolve()
path_package = args.path_package.resolve()

print('3rdparty directory:', path_3rdparty)
print('Temp directory:', path_temp)
print('Package directory:', path_package)

cmake_generator_args = []
# Dependencies are always built with LTO. Their warnings are not our business,
# and some (e.g. jsoncpp) even enable `-Weverything` on clang-cl, flooding the log.
standard_clang_flags = args.compile_flags + ' -w'
standard_linker_flags = args.linker_flags

if sys.platform == 'linux':
	cmake_generator_args = [ '-GNinja', '-DCMAKE_BUILD_TYPE=Release' ]
	standard_clang_flags += ' -flto=thin'
	standard_linker_flags += ' -flto=thin'
elif sys.platform == 'win32':
	cmake_generator_args = [ '-GVisual Studio 18 2026', '-Ax64', '-TClangCL', '-DCMAKE_CONFIGURATION_TYPES=Release' ]
	# No `/showFilenames`, clang-cl ignores it with LTO and warns about every file
	standard_clang_flags += ' /clang:-flto=thin /EHsc /MP'
else:
	print('Unknown build platform!')
	sys.exit(1)

cmake_standard_args = [
	*cmake_generator_args,
	f'-DCMAKE_INSTALL_PREFIX={path_package}', f'-DCMAKE_PREFIX_PATH={path_package}',
	f'-DCMAKE_C_FLAGS={standard_clang_flags}', f'-DCMAKE_CXX_FLAGS={standard_clang_flags}',
	f'-DCMAKE_EXE_LINKER_FLAGS={standard_linker_flags}', f'-DCMAKE_SHARED_LINKER_FLAGS={standard_linker_flags}',
	f'-DCMAKE_MODULE_LINKER_FLAGS={standard_linker_flags}',
	*extra_cmake_args,
]

def build_standard(lib_name: str, specific_args: list):
	path_source = str(path_3rdparty / lib_name)
	path_build = str(path_temp / lib_name)

	print()
	print('=====')
	print('Now building:', lib_name)
	print('Source path:', path_source)
	print('Build path:', path_build)
	print()

	subprocess.run([ args.cmake, *cmake_standard_args, '-S', path_source, '-B', path_build, *specific_args ], check=True)
	subprocess.run([ args.cmake, '--build', path_build, '--config', 'Release', '--target', 'install' ], check=True)

# Many other libs depend on this little boy, build it first
# Don't disable static library - zlib's CMake config would fail to find it unless
# `find_package` explicitly requests `shared` component, which we don't do.
zlib_args = [ '-DZLIB_BUILD_TESTING=OFF', '-DZLIB_BUILD_SHARED=ON' ]
if sys.platform == 'win32':
	# DLL version resource includes `zlib.h`, and with ClangCL toolset the resource
	# compiler chokes on Clang's `stddef.h` included from there. `Z_SOLO` makes
	# `zconf.h` skip all standard headers, the resource only needs version macros.
	zlib_args += [ '-DCMAKE_RC_FLAGS=/DZ_SOLO' ]
build_standard('zlib', zlib_args)

# FreeType depends on zlib, don't order before it.
#
# It also has HarfBuzz integration (for auto-hinting), but this forms a cyclic
# dependency. We only use FreeType for SDF rendering, which doesn't need hinting.
build_standard('freetype', [
	'-DFT_DISABLE_BZIP2=ON', '-DFT_DISABLE_BROTLI=ON', '-DFT_DISABLE_HARFBUZZ=ON',
	'-DFT_REQUIRE_ZLIB=ON', '-DFT_DISABLE_PNG=ON', '-DBUILD_SHARED_LIBS=ON',
])

# HarfBuzz has integration with FreeType that we need, so build after it
build_standard('harfbuzz', [
	'-DHB_HAVE_FREETYPE=ON', '-DHB_BUILD_SUBSET=OFF', '-DBUILD_SHARED_LIBS=ON',
	'-DHB_BUILD_RASTER=OFF', '-DHB_BUILD_VECTOR=OFF', '-DHB_BUILD_GPU=OFF',
	# HarfBuzz turns some warnings into errors with pragmas, and clang-cl
	# does trigger them (`-Wunused-function`). Not our job to fix these.
	f'-DCMAKE_CXX_FLAGS={standard_clang_flags} -DHB_NO_PRAGMA_GCC_DIAGNOSTIC_ERROR',
	# HarfBuzz uses this variable before including `GNUInstallDirs`,
	# its exported include directory is broken unless set explicitly
	'-DCMAKE_INSTALL_INCLUDEDIR=include',
])

# Independent libraries

fmt_args = [ '-DBUILD_SHARED_LIBS=ON', '-DFMT_DOC=OFF', '-DFMT_TEST=OFF' ]
if sys.platform == 'win32':
	# As a master project with VS generator, fmt writes `run-msbuild.bat` into its source tree.
	# Pretend it's a subproject to avoid this, but then installation must be requested explicitly.
	fmt_args += [ '-DFMT_MASTER_PROJECT=OFF', '-DFMT_INSTALL=ON' ]
build_standard('fmt', fmt_args)
build_standard('jsoncpp', [ '-DBUILD_SHARED_LIBS=ON',
	'-DJSONCPP_WITH_TESTS=OFF', '-DBUILD_STATIC_LIBS=OFF', '-DBUILD_OBJECT_LIBS=OFF'
])
build_standard('glfw', [ '-DBUILD_SHARED_LIBS=ON',
	'-DGLFW_BUILD_DOCS=OFF', '-DGLFW_BUILD_TESTS=OFF', '-DGLFW_BUILD_DOCS=OFF', '-DGLFW_BUILD_EXAMPLES=OFF'
])
