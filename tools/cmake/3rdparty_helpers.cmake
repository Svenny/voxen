include_guard(GLOBAL)

# Link 3rdparty module against this dummy to suppress
# warnings and avoid `-Werror`-related build failures.
# We could simply disable `-Werror` but then build log will still be quite dirty.

add_library(dummy_suppress_3rdparty_warnings INTERFACE)
target_compile_options(dummy_suppress_3rdparty_warnings INTERFACE
	-Wno-extra-semi
	-Wno-implicit-fallthrough
	-Wno-implicit-int-conversion
	-Wno-nonportable-system-include-path
	-Wno-sign-conversion
	-Wno-shorten-64-to-32
	-Wno-switch-default
	-Wno-zero-as-null-pointer-constant
)

# Call this to tweak some properties of externally-created targets to better align with Voxen
function(voxen_fixup_3rdparty_target target)
	# TODO: partially duplicates `toolchain_setup.cmake` - factor out to common function?
	set_target_properties(${target} PROPERTIES
		# Strip unnecessary dependence on paths in built libraries
		INSTALL_RPATH "\$ORIGIN"
		BUILD_WITH_INSTALL_RPATH ON

		# Use predefined binary output directories
		# XXX: shouldn't we do this at install stage?
		ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/lib
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin
	)
endfunction()
