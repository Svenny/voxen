include_guard(GLOBAL)

if(NOT CMAKE_GENERATOR STREQUAL "Ninja" AND NOT CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
	message(FATAL_ERROR "Only Ninja generator is supported!")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
	message(FATAL_ERROR "Only Clang 12 or newer is supported for C++ code!")
endif()

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_VERSION VERSION_LESS 12.0)
	message(FATAL_ERROR "Only Clang 12 or newer is supported for C code!")
endif()

# This function must be called on any Voxen C/C++ target (executable, static or shared library)
function(voxen_setup_target target is_executable)
	target_compile_options(${target} PRIVATE
		# Enforce good coding standards to at least some extent
		-Wall -Wextra -Wpedantic -Werror
		# Additional useful diagnostics not enabled by above flags
		-Wconversion -Wdeprecated -Wshadow -Wundef -Wweak-vtables
		# Remap /path/to/voxen/src/file.cpp -> src/file.cpp in __FILE__ and co.
		"-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/="

		# Speed up math at the cost of strict standard compliance
		-fno-math-errno # Stdlib math functions do not change errno
		-fno-signed-zeros # Assume +0.0 and -0.0 are the same
		-fno-trapping-math # Assume there are no floating point exceptions
		-ffp-contract=fast # Allow fusing multiply+add to FMA

		# Don't use PLT stubs for dynamically linked function calls.
		# This prevents lazy binding but we don't need it anyway.
		-fno-plt

		# These flags will only be applied when compiling C++ code
		$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++> # Always use libc++ as a runtime
	)

	target_link_options(${target} PRIVATE
		# Always use LLD for linking speed and LTO
		-fuse-ld=lld
		# Always use libc++ as a runtime
		-stdlib=libc++
	)

	if(is_executable)
		# Disable any possiblity to generate position-independent code for exectuables
		target_compile_options(${target} PRIVATE -fno-pie -fno-pic)
		target_link_options(${target} PRIVATE -no-pie -fno-pic)
	endif()

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

		# Use LTO for release builds (this is ThinLTO on Clang)
		INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
		INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON
		INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
		INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO OFF

		# Remove useless -D`target`_EXPORTS definition by CMake
		DEFINE_SYMBOL ""

		PCH_INSTANTIATE_TEMPLATES ON
		PCH_WARN_INVALID ON
	)

	if(GENERATOR_IS_MULTI_CONFIG)
		set_target_properties(${target} PROPERTIES
			ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/lib
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin
			RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin
		)
	else()
		set_target_properties(${target} PROPERTIES
			ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
			RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
		)
	endif()
endfunction()

# All executable targets should be created via this function
function(voxen_add_executable name)
	add_executable(${name} "")
	voxen_setup_target(${name} true)
endfunction()

# All library targets should be created via this function
function(voxen_add_library name type)
	add_library(${name} ${type} "")

	if(NOT type STREQUAL "INTERFACE")
		voxen_setup_target(${name} false)
	endif()
endfunction()
