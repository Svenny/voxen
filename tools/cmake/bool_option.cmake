include_guard(GLOBAL)

# Define an user-configurable option which sets the corresponding variable to
# `true` or `false`, suitable for directly pasting it into CMake-configured headers
function(bool_option VARNAME text default_value)
	option(${VARNAME} ${text} ${default_value})
	if(${VARNAME})
		set(${VARNAME} true PARENT_SCOPE)
	else()
		set(${VARNAME} false PARENT_SCOPE)
	endif()
endfunction()
