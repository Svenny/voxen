include_guard(GLOBAL)

if(NOT CMAKE_GENERATOR MATCHES "^Ninja.*" AND NOT CMAKE_GENERATOR MATCHES "^Visual Studio.*")
	message(FATAL_ERROR "Only Ninja and Visual Studio generators are supported!")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 21.0)
	message(FATAL_ERROR "Only Clang 21 or newer is supported for C++ code!")
endif()

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_VERSION VERSION_LESS 21.0)
	message(FATAL_ERROR "Only Clang 21 or newer is supported for C code!")
endif()

option(VOXEN_WERROR "Treat compiler warnings as errors" ON)
set(VOXEN_ARCH_FLAGS "-march=x86-64-v3" CACHE STRING "Target CPU flags, space-separated")
set(VOXEN_SANITIZERS "" CACHE STRING
	"Sanitizers to enable, semicolon-separated (e.g. `address;undefined`). Linux only")

if(VOXEN_SANITIZERS AND NOT LINUX)
	message(FATAL_ERROR "VOXEN_SANITIZERS is only supported on Linux")
endif()

if(LINUX AND NOT CMAKE_LINKER_TYPE)
	# The fastest linker we have on Linux, non-LTO builds are just instant.
	# Not cached, so the default follows mold being (un)installed.
	find_program(VOXEN_MOLD_EXECUTABLE mold NO_CACHE)
	if(VOXEN_MOLD_EXECUTABLE)
		set(CMAKE_LINKER_TYPE MOLD)
	else()
		set(CMAKE_LINKER_TYPE LLD)
	endif()
endif()

if(CMAKE_LINKER_TYPE)
	message(STATUS "Using linker type: ${CMAKE_LINKER_TYPE}")
endif()

set(CLANG_OPTION_PREFIX "")
if(WIN32)
	# clang-cl needs this prefix for non-cl (native clang) options
	set(CLANG_OPTION_PREFIX "/clang:")
endif()

# Code generation and linker flags shared by Voxen targets and 3rdparty builds
separate_arguments(VOXEN_CODEGEN_FLAGS NATIVE_COMMAND "${VOXEN_ARCH_FLAGS}")
list(TRANSFORM VOXEN_CODEGEN_FLAGS PREPEND "${CLANG_OPTION_PREFIX}")
set(VOXEN_LINKER_FLAGS "")

if(LINUX)
	list(APPEND VOXEN_CODEGEN_FLAGS
		# We need more potentially-obscure-bugs-inducing optimizations...
		# Don't use PLT thunks for dynamically linked function calls.
		# This prevents lazy binding which we don't need anyway.
		-fno-plt
		# This allows interprocedural optimization for exported functions
		# when calling them from the same library (a frequent case for us).
		# Might be enabled in Clang by default but let's be sure.
		-fno-semantic-interposition
	)

	list(APPEND VOXEN_LINKER_FLAGS
		# Directly bind locally defined function symbols. Similar to
		# `-fno-semantic-interposition` but works at link time.
		# Possible side effect: inline functions can have different
		# addresses across binary boundaries (who cares?).
		# NOTE: needs `-Wl,`, clang driver takes bare `-B...` as a search path.
		-Wl,-Bsymbolic-functions
	)

	if(NOT VOXEN_SANITIZERS)
		# Treat unresolved symbols as errors when linking shared libraries.
		# Useful to catch missing dependencies a bit faster. Sanitizer runtimes
		# are linked only into executables, leaving shared libraries unresolved.
		list(APPEND VOXEN_LINKER_FLAGS -Wl,--no-undefined)
	endif()
endif()

# This function must be called on any Voxen C/C++ target (executable, static or shared library)
function(voxen_setup_target target)
	set(CLANG_OPTION_WALL "-Wall")

	if(WIN32)
		# clang-cl treats `-Wall` like MSVC `/Wall`, i.e. `-Weverything`. `/clang:-Wall` won't do
		# either, clang-cl moves all `/clang:` options to the end, overriding `-Wno-...` options.
		# `-W4` is clang-cl's spelling of `-Wall -Wextra`, and it keeps its place.
		set(CLANG_OPTION_WALL "-W4")
	endif()

	if(VOXEN_WERROR)
		target_compile_options(${target} PRIVATE -Werror)
	endif()

	if(VOXEN_SANITIZERS)
		list(JOIN VOXEN_SANITIZERS "," sanitizers)
		target_compile_options(${target} PRIVATE -fsanitize=${sanitizers} -fno-omit-frame-pointer)
		target_link_options(${target} PRIVATE -fsanitize=${sanitizers})
	endif()

	target_compile_options(${target} PRIVATE ${VOXEN_CODEGEN_FLAGS})
	target_link_options(${target} PRIVATE ${VOXEN_LINKER_FLAGS})

	# Common stuff for all platforms
	target_compile_options(${target} PRIVATE
		# Enforce good coding standards to at least some extent
		${CLANG_OPTION_WALL} -Wextra -Wpedantic
		# Additional useful diagnostics not enabled by above flags
		-Wconversion -Wdeprecated -Wshadow -Wundef -Wweak-vtables
		-Wshadow-uncaptured-local -Wunused-member-function -Wconditional-uninitialized
		# Does not recognize some member functions are needed to satisfy concepts
		-Wno-unneeded-member-function

		# Prefix maps ensure more reproducible and environment-independent builds
		# Remap /path/to/voxen/src/file.cpp -> src/file.cpp in __FILE__ and co.
		"${CLANG_OPTION_PREFIX}-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/="
		# Same for debug info. To access sources during debugging
		# set source prefix map to `/ => ${CMAKE_SOURCE_DIR}/`.
		"${CLANG_OPTION_PREFIX}-fdebug-prefix-map=${CMAKE_SOURCE_DIR}/=/"

		# Speed up math at the cost of strict standard compliance
		${CLANG_OPTION_PREFIX}-fno-math-errno # Stdlib math functions do not change errno
		${CLANG_OPTION_PREFIX}-fno-signed-zeros # Assume +0.0 and -0.0 are the same
		${CLANG_OPTION_PREFIX}-fno-trapping-math # Assume there are no floating point exceptions
		# Allow fusing multiply+add to FMA. Unlike plain `fast`, this one lets code opt out
		# with `#pragma clang fp contract(off)` where bit-exact results are needed.
		${CLANG_OPTION_PREFIX}-ffp-contract=fast-honor-pragmas
	)

	if(WIN32)
		target_compile_definitions(${target} PRIVATE
			# Stop windows crt from bitching about innocent C functions
			-D_CRT_NONSTDC_NO_WARNINGS
			-D_CRT_SECURE_NO_WARNINGS
			# `min`/`max` macros from `windows.h` would break `std::min` and `m::min`
			-DNOMINMAX
		)

		target_compile_options(${target} PRIVATE
			# Windows-only warning on `extern template class VOXEN_API ...`. The proper
			# fix would be a separate dllimport-only macro for these declarations.
			-Wno-dllexport-explicit-instantiation-decl

			# CMake maps C++23 to `-std:c++latest`, which is C++26 in clang-cl.
			# Pin the standard explicitly, `/clang:` options win as they go last.
			# NOTE: VS generator evaluates this for all sources of a target with
			# any C++ sources, but at least it keeps C-only targets intact.
			$<$<COMPILE_LANGUAGE:CXX>:/clang:-std=c++23>

			# Display names of files being compiled (makes VS output window informative)
			/showFilenames
			# Compile multiple source files in parallel (why it's not the default?!)
			/MP

			# CMake+VS+ClangCL+IntelliSense interaction is broken beyond hope.
			# Include dirs marked as SYSTEM compile fine but are not seen by IntelliSense.
			# CMake adds these with `-imsvc` switch but VS expects `/external:I` seemingly.
			# Fixes VS2019... but VS2022 is not satisfied even with that.
			#
			# I have no hope that it will ever get fixed, so use all three possible switches
			# and pray that clang-cl won't start issuing warnings one day because of `/I`.
			"/external:I${CMAKE_BINARY_DIR}/3rdparty-package/include"
			"/I${CMAKE_BINARY_DIR}/3rdparty-package/include"
		)

		target_link_libraries(${target} PRIVATE
			# Futex ops like WaitOnAddress/WakeByAddress...
			synchronization
		)

		# Built-in clang-tidy does not properly receive clang flags
		# so we have to disable all that useless junk here as well
		set_target_properties(${target} PROPERTIES VS_GLOBAL_ClangTidyChecks "\
-clang-diagnostic-c++98-compat,-clang-diagnostic-c++98-compat-pedantic,\
-clang-diagnostic-pre-c++17-compat,-clang-diagnostic-pre-c++20-compat-pedantic,\
-clang-diagnostic-pre-c++14-compat,-clang-diagnostic-c++20-compat,-clang-diagnostic-pre-c++20-compat,\
-clang-diagnostic-pragma-once-outside-header,-clang-diagnostic-unsafe-buffer-usage,\
-clang-diagnostic-shadow-field-in-constructor,-clang-diagnostic-extra-semi-stmt,\
-clang-diagnostic-switch-default,-clang-diagnostic-global-constructors,-clang-diagnostic-exit-time-destructors,\
-clang-diagnostic-c++98-compat-local-type-template-args"
		)
	endif()

	# We usually name targets in lowercase, convert to uppercase for nicer macros
	string(TOUPPER ${target} TARGET_UPPERCASE)

	set_target_properties(${target} PROPERTIES
		# Require C17 and C++23
		C_STANDARD 17 C_STANDARD_REQUIRED TRUE
		CXX_STANDARD 23 CXX_STANDARD_REQUIRED TRUE
		# We don't use C++ modules, don't spend time scanning sources for them
		CXX_SCAN_FOR_MODULES OFF

		# Target will be able to load .so's from its own directory
		INSTALL_RPATH "\$ORIGIN"
		BUILD_WITH_INSTALL_RPATH ON

		# Use -fvisibility=hidden by default
		C_VISIBILITY_PRESET hidden
		CXX_VISIBILITY_PRESET hidden

		# 3rdparty is always built in Release (/MD) and we must not mix it with /MDd,
		# so set /MD for our code as well. This loses some hypothetical debug
		# capabilities, but then, do we really need them anywhere?
		MSVC_RUNTIME_LIBRARY MultiThreadedDLL

		# Enable PIC for everything (executables, static and shared libs).
		# Shared libs must be PIC anyways. Static libs are almost always linked into
		# shared ones, consequently they must be PIC too. As the vast majority of
		# hot code paths is going to be in shared libs (libvoxen, probably plugins)
		# we should not lose any measurable perf from making executables PIE too.
		POSITION_INDEPENDENT_CODE ON

		# Use LTO for release builds (this is ThinLTO on Clang)
		INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
		INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON
		INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
		INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF

		# Define `TARGET_EXPORTS` instead of `target_EXPORTS` when compiling.
		# Should be only needed on windows to switch between dllexport/dllimport.
		DEFINE_SYMBOL "${TARGET_UPPERCASE}_EXPORTS"

		PCH_INSTANTIATE_TEMPLATES ON
		PCH_WARN_INVALID ON
	)

	# Use predefined binary output directories
	# XXX: shouldn't we do this at install stage?
	set_target_properties(${target} PROPERTIES
		ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/lib
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin
	)
endfunction()

# All executable targets should be created via this function
function(voxen_add_executable name sources)
	add_executable(${name} ${sources})
	voxen_setup_target(${name})
endfunction()

# All library targets should be created via this function
function(voxen_add_library name type)
	add_library(${name} ${type} "")

	if(NOT type STREQUAL "INTERFACE")
		voxen_setup_target(${name})
	endif()
endfunction()
