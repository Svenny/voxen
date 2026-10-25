include_guard(GLOBAL)

# Build-time code generation rules

find_package(Git QUIET)

# Generates version header (see `scripts/version.hpp.in`) for `target`,
# includable as `<${header}>`. The header is regenerated on every build
# but its timestamp only changes along with the contents.
function(voxen_add_version_header target header)
	set(include_dir ${CMAKE_BINARY_DIR}/configs)

	add_custom_target(${target}-version-file
		COMMAND ${CMAKE_COMMAND}
			-D OUTPUT_FILE=${include_dir}/${header}
			-D VERSION_MAJOR=${VOXEN_VERSION_MAJOR}
			-D VERSION_MINOR=${VOXEN_VERSION_MINOR}
			-D VERSION_PATCH=${VOXEN_VERSION_PATCH}
			-D VERSION_SUFFIX=${VOXEN_VERSION_SUFFIX}
			-D GIT_EXECUTABLE=${GIT_EXECUTABLE}
			-P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/scripts/gen_version_header.cmake
		BYPRODUCTS ${include_dir}/${header}
		COMMENT "Generating ${header} file"
		VERBATIM
	)
	add_dependencies(${target} ${target}-version-file)

	# Include directory with generated headers
	target_include_directories(${target} PUBLIC ${include_dir})
endfunction()
