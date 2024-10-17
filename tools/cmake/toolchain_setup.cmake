include_guard(GLOBAL)

if(NOT CMAKE_GENERATOR MATCHES "^Ninja.*" AND NOT CMAKE_GENERATOR MATCHES "^Visual Studio.*")
	message(FATAL_ERROR "Only Ninja and Visual Studio generators are supported!")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18.0)
	message(FATAL_ERROR "Only Clang 18 or newer is supported for C++ code!")
endif()

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_VERSION VERSION_LESS 18.0)
	message(FATAL_ERROR "Only Clang 18 or newer is supported for C code!")
endif()

# This function must be called on any Voxen C/C++ target (executable, static or shared library)
function(voxen_setup_target target is_executable)
	set(CLANG_OPTION_PREFIX "")

	if(WIN32)
		# clang-cl needs this prefix for non-cl (native clang) options
		set(CLANG_OPTION_PREFIX "/clang:")
	endif()

	# Common stuff for all platforms
	target_compile_options(${target} PRIVATE
		# Enforce good coding standards to at least some extent
		-Wall -Wextra -Wpedantic -Werror
		# Additional useful diagnostics not enabled by above flags
		-Wconversion -Wdeprecated -Wshadow -Wundef -Wweak-vtables

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
		${CLANG_OPTION_PREFIX}-ffp-contract=fast # Allow fusing multiply+add to FMA
	)

	if(LINUX)
		target_compile_options(${target} PRIVATE
			# We need more potentially-obscure-bugs-inducing optimizations...
			# Don't use PLT thunks for dynamically linked function calls.
			# This prevents lazy binding but we don't need it anyway.
			-fno-plt
			# This allows interprocedural optimization for exported functions
			# when calling them from the same library (a frequent case for us).
			# Might be enabled in Clang by default but let's be sure.
			-fno-semantic-interposition
		)

		target_link_options(${target} PRIVATE
			# Always use LLD for linking speed and LTO
			-fuse-ld=lld

			# Directly bind locally defined function symbols. Similar to
			# `-fno-semantic-interposition` but works at link time.
			# Possible side effect: inline functions can have different
			# addresses across binary boundaries (but who cares?).
			-Bsymbolic-functions

			# Treat unresolved symbols as errors when linking shared libraries.
			# Useful to catch missing dependencies a bit faster.
			--no-undefined
		)
	elseif(WIN32)
		# Stop windows crt from bitching about innocent C functions
		target_compile_definitions(${target} PRIVATE
			-D_CRT_NONSTDC_NO_WARNINGS
			-D_CRT_SECURE_NO_WARNINGS
		)

		# Disable nonsensical warnings triggering only on clang-cl for some reason
		target_compile_options(${target} PRIVATE
			# WTF? We're not compiling in C++98 mode...
			-Wno-c++20-compat -Wno-c++20-extensions
			-Wno-c++98-compat -Wno-c++98-compat-pedantic

			# We're doing A LOT of incompatible function pointer casts in vulkan code
			-Wno-cast-function-type-strict
			# It thinks I shouldn't use automatic template deduction...
			-Wno-ctad-maybe-unsupported
			# Some M$-specific shit I don't care about
			-Wno-dllexport-explicit-instantiation-decl
			# Can't parse doxygen multi-variable "\param x,y,z" syntax; otherwise would be a nice thing
			-Wno-documentation
			# Some global variables have destructors. So what?
			-Wno-exit-time-destructors
			# Triggers on empty `;` statements that occur after some macros
			-Wno-extra-semi-stmt
			# Some global variables have constructors. So what?
			-Wno-global-constructors
			# Triggers on plain old C cast
			-Wno-old-style-cast
			# Cries about us using `__identifier` e.g. in `defer` implementation
			-Wno-reserved-identifier
			# We have lots of places like `MyClass(value) : value(value) {}`
			-Wno-shadow-field-in-constructor
			# Requires `default` label in all switches, even fully covered ones
			-Wno-switch-default
			# Triggers even when we have terminating `default` unlike `-Wswitch`
			-Wno-switch-enum
			# Does not recognize some member functions are needed to satisfy concepts
			-Wno-unneeded-member-function
			# Triggers on ~any unchecked array access, total nonsense
			-Wno-unsafe-buffer-usage
		)

		target_link_libraries(${target} PRIVATE
			# Futex ops like WaitOnAddress/WakeByAddress...
			synchronization
		)
	endif()

	# We usually name targets in lowercase, convert to uppercase for nicer macros
	string(TOUPPER ${target} TARGET_UPPERCASE)

	set_target_properties(${target} PROPERTIES
		# Require C17 and C++20
		C_STANDARD 17 C_STANDARD_REQUIRED TRUE
		CXX_STANDARD 20 CXX_STANDARD_REQUIRED TRUE

		# Target will be able to load .so's from its own directory
		INSTALL_RPATH "\$ORIGIN"
		BUILD_WITH_INSTALL_RPATH ON

		# Use -fvisibility=hidden by default
		C_VISIBILITY_PRESET hidden
		CXX_VISIBILITY_PRESET hidden

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
	voxen_setup_target(${name} true)
endfunction()

# All library targets should be created via this function
function(voxen_add_library name type)
	add_library(${name} ${type} "")

	if(NOT type STREQUAL "INTERFACE")
		voxen_setup_target(${name} false)
	endif()
endfunction()
