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

set(VOXEN_IS_VS_GENERATOR OFF)
if(CMAKE_GENERATOR MATCHES "^Visual Studio.*")
	set(VOXEN_IS_VS_GENERATOR ON)
endif()

# RelWithDebInfo is Release plus debug info. CMake defaults give it less optimization
# (`-O2`, `/Ob1`), use Release flags instead. Debug info flags are added on top on
# Linux, on Windows they come from `MSVC_DEBUG_INFORMATION_FORMAT` (policy CMP0141).
# NOTE: these shadow the cache variables, edit them here rather than in the cache.
foreach(lang IN ITEMS C CXX)
	set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "${CMAKE_${lang}_FLAGS_RELEASE}")
	if(NOT WIN32)
		string(APPEND CMAKE_${lang}_FLAGS_RELWITHDEBINFO " -g")
	endif()
endforeach()

if(WIN32)
	# `/debug` makes the linker default to `/OPT:NOREF,NOICF` and `/INCREMENTAL`,
	# restore Release linker optimizations. lld-link can't link incrementally anyway.
	foreach(type IN ITEMS EXE SHARED MODULE)
		set(CMAKE_${type}_LINKER_FLAGS_RELWITHDEBINFO "/debug /INCREMENTAL:NO /OPT:REF /OPT:ICF")
	endforeach()
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

if(VOXEN_IMPORT_STD AND CMAKE_VERSION VERSION_LESS 4.4)
	# EXPERIMENT: CMake < 4.4 builds the `std` module in its own `__cmake_cxx23` target which
	# only gets global flags (4.4 builds it per consumer target, with consumer's flags). BMIs must be built with compatible flags (Clang refuses to load
	# `std.pcm` built without our `-fno-math-errno`), so make all such flags global.
	set(import_std_flags ${VOXEN_CODEGEN_FLAGS}
		-fno-math-errno -fno-signed-zeros -fno-trapping-math -ffp-contract=fast-honor-pragmas)
	if(WIN32)
		list(TRANSFORM import_std_flags PREPEND "/clang:" REGEX "^-f")
	endif()
	list(JOIN import_std_flags " " import_std_flags)
	string(APPEND CMAKE_CXX_FLAGS " ${import_std_flags}")
endif()

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
			# EXPERIMENT: clang-cl picks module interface mode from the `.cppm` extension
			# and warns that CMake's `-TP` (compile as C++) is unused. No way to target
			# module units only, so this has to be disabled for all C++ sources.
			$<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-command-line-argument>

			# CMake maps C++23 to `-std:c++latest`, which is C++26 in clang-cl.
			# Pin the standard explicitly, `/clang:` options win as they go last.
			# NOTE: VS generator evaluates this for all sources of a target with
			# any C++ sources, but at least it keeps C-only targets intact.
			$<$<COMPILE_LANGUAGE:CXX>:/clang:-std=c++23>
		)

		target_link_libraries(${target} PRIVATE
			# Futex ops like WaitOnAddress/WakeByAddress...
			synchronization
		)
	endif()

	if(VOXEN_IS_VS_GENERATOR)
		target_compile_options(${target} PRIVATE
			# Display names of files being compiled (makes VS output window informative)
			/showFilenames
			# Compile multiple source files in parallel (why it's not the default?!)
			/MP
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
		# EXPERIMENT: C++20 named modules. Scanning costs a dependency scan step
		# per source and is only needed by targets using (or exporting) modules.
		CXX_SCAN_FOR_MODULES ON

		# Target will be able to load .so's from its own directory
		INSTALL_RPATH "\$ORIGIN"
		BUILD_WITH_INSTALL_RPATH ON

		# Use -fvisibility=hidden by default
		C_VISIBILITY_PRESET hidden
		CXX_VISIBILITY_PRESET hidden

		# Use the release CRT in every configuration. The debug one (/MDd) adds
		# checked iterators and heap checks that cost a lot of Debug performance,
		# sanitizers on Linux cover that ground better.
		MSVC_RUNTIME_LIBRARY MultiThreadedDLL

		# Enable PIC for everything (executables, static and shared libs).
		# Shared libs must be PIC anyways. Static libs are almost always linked into
		# shared ones, consequently they must be PIC too. As the vast majority of
		# hot code paths is going to be in shared libs (libvoxen, probably plugins)
		# we should not lose any measurable perf from making executables PIE too.
		POSITION_INDEPENDENT_CODE ON

		# Use LTO for optimized builds (this is ThinLTO on Clang)
		INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
		INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON
		INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
		INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON

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

	if(type STREQUAL "SHARED" AND LINUX)
		# Static third-party libraries are linked into our shared libraries and must
		# not leak out of them: FreeType marks its API as default-visibility, and any
		# exported symbol could be interposed by a system copy of the same library
		# loaded into the process (e.g. through the Wayland/X11 stack).
		target_link_options(${name} PRIVATE -Wl,--exclude-libs,ALL)
	endif()
endfunction()

# Embed a natvis (VS debugger visualizers) file into PDBs of binaries containing `target`
# code: the target itself for executables and shared libraries, its consumers otherwise.
function(voxen_target_natvis target file)
	if(NOT WIN32)
		return()
	endif()

	get_target_property(type ${target} TYPE)
	cmake_path(ABSOLUTE_PATH file NORMALIZE)
	set(linkable OFF)
	if(type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "MODULE_LIBRARY")
		set(linkable ON)
	endif()

	if(VOXEN_IS_VS_GENERATOR)
		# MSBuild passes `Natvis` project items to the linker. CMake turns `.natvis` sources into
		# them, but `/NATVIS:` link options into a `<Link><Natvis>` setting ignored by ClangCL
		# toolset. Interface sources become items in consumer projects, use them instead.
		if(linkable)
			target_sources(${target} PRIVATE ${file})
		else()
			target_sources(${target} INTERFACE ${file})
		endif()
		return()
	endif()

	# Only configurations producing PDBs
	set(option "$<$<CONFIG:Debug,RelWithDebInfo>:/NATVIS:${file}>")
	if(linkable)
		target_link_options(${target} PRIVATE ${option})
	else()
		target_link_options(${target} INTERFACE ${option})
	endif()
endfunction()
