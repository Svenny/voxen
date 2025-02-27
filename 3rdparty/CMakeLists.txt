# This directory manages third-party Voxen dependencies

# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER 3rdparty)

include(${CMAKE_SOURCE_DIR}/tools/cmake/3rdparty_helpers.cmake)

execute_process(COMMAND ${Python3_EXECUTABLE}
	${CMAKE_SOURCE_DIR}/tools/build/build-3rdparty.py
	${CMAKE_SOURCE_DIR}/3rdparty
	${CMAKE_BINARY_DIR}/3rdparty-build
	${CMAKE_BINARY_DIR}/3rdparty-package

	COMMAND_ERROR_IS_FATAL ANY
)

add_custom_target(3rdparty-pull-binaries)

function(voxen_take_package package target alias)
	find_package(${package}
		REQUIRED CONFIG GLOBAL
		PATHS ${CMAKE_BINARY_DIR}/3rdparty-package
		NO_DEFAULT_PATH
	)
	add_library(${alias} ALIAS ${target})

	# 3rdparty libs are built in release, `IMPORTED_LOCATION` is empty.
	# TODO: this is not robust at all.
	get_target_property(lib_path ${target} IMPORTED_LOCATION_RELEASE)

	if(LINUX)
		# Strip version suffix
		string(REGEX REPLACE "\\.so.*$" ".so" unversioned_lib_path ${lib_path})

		# Locate the semi-versioned symlinked file (libfoo.so.xx)
		execute_process(
			COMMAND readlink "${unversioned_lib_path}"
			OUTPUT_VARIABLE lib_name
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
	elseif(WIN32)
		get_filename_component(lib_name ${lib_path} NAME)
	else()
		message(FATAL_ERROR "Unknown build platform")
	endif()

	add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/$<CONFIG>/bin/${lib_name}"
		MAIN_DEPENDENCY ${lib_path}
		COMMENT "Pulling 3rdparty binary ${lib_name}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${lib_path} "${CMAKE_BINARY_DIR}/$<CONFIG>/bin/${lib_name}"
	)
	target_sources(3rdparty-pull-binaries PRIVATE "${CMAKE_BINARY_DIR}/$<CONFIG>/bin/${lib_name}")
endfunction()

voxen_take_package(fmt fmt::fmt 3rdparty::fmt)
voxen_take_package(freetype freetype 3rdparty::freetype)
voxen_take_package(glfw3 glfw 3rdparty::glfw)
voxen_take_package(jsoncpp jsoncpp_lib 3rdparty::jsoncpp)
voxen_take_package(zlib zlib 3rdparty::zlib)

# TODO: solve this shit more systematically
if(MSVC)
	target_compile_options(freetype INTERFACE "/external:I${CMAKE_BINARY_DIR}/3rdparty-package/include/freetype2")
	target_compile_options(freetype INTERFACE "/I${CMAKE_BINARY_DIR}/3rdparty-package/include/freetype2")
endif()

# Embedded dependencies (sources in tree)
include(backward.cmake)
include(catch.cmake)
include(cpp-result.cmake)
include(cxxopts.cmake)
include(glm.cmake)
include(json5cpp.cmake)
include(pcg.cmake)
include(platform-folders.cmake)
include(simpleini.cmake)
include(vulkan-headers.cmake)
# Depends on `vulkan-headers`, include after them
include(vma.cmake)

# Fixup VS generated project IntelliSense in projects
# that set SYSTEM include directories. Should be
# a temporary thing until CMake+VS+ClangCL stabilizes.
voxen_fixup_3rdparty_intellisense(backward)
voxen_fixup_3rdparty_intellisense(cxxopts)
voxen_fixup_3rdparty_intellisense(glm)
voxen_fixup_3rdparty_intellisense(json5cpp)
voxen_fixup_3rdparty_intellisense(pcg)
voxen_fixup_3rdparty_intellisense(simpleini)
voxen_fixup_3rdparty_intellisense(vma)
