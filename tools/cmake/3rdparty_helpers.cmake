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

# Currently something either VS or CMake VS+ClangCL project generation
# is buggy. SYSTEM include dirs compile fine but are not seen by IntelliSense.
# CMake adds these with `-imsvc` switch but VS expects `/external:I` seemingly.
# Call this function on a 3rdparty target as a workaround until it stabilizes.
function(voxen_fixup_3rdparty_intellisense target)
	if(MSVC)
		get_target_property(SYS_DIRS ${target} INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
		foreach(DIR IN LISTS SYS_DIRS)
			target_compile_options(${target} INTERFACE "/external:I${DIR}")
		endforeach()
	endif()
endfunction()
