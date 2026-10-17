include_guard(GLOBAL)

# Currently something either VS or CMake VS+ClangCL project generation
# is buggy. SYSTEM include dirs compile fine but are not seen by IntelliSense.
# CMake adds these with `-imsvc` switch but VS expects `/external:I` seemingly.
# Call this function on a 3rdparty target as a workaround until it stabilizes.
function(voxen_fixup_3rdparty_intellisense target)
	if(MSVC)
		get_target_property(SYS_DIRS ${target} INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
		foreach(DIR IN LISTS SYS_DIRS)
			target_compile_options(${target} INTERFACE "/external:I${DIR}")
			target_compile_options(${target} INTERFACE "/I${DIR}")
		endforeach()
	endif()
endfunction()
