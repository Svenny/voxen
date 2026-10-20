include_guard(GLOBAL)

# GLSL to SPIR-V shader compilation rules

find_program(GLSL_COMPILER glslc)
if(NOT GLSL_COMPILER)
	message(FATAL_ERROR "GLSL compiler not found!")
endif()

# Creates custom target `target` compiling GLSL shaders to SPIR-V:
#     voxen_add_shaders(<target> SOURCE_DIR <dir> [INCLUDE_DIRS <dir>...] SHADERS <shader>...)
#
# `SHADERS` are paths relative to `SOURCE_DIR`, the output is
# `<build>/<config>/data/assets/shaders/<shader>.spv`.
function(voxen_add_shaders target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "SOURCE_DIR" "INCLUDE_DIRS;SHADERS")

	set(compiler_options -O --target-env=vulkan1.3 -Werror)
	foreach(dir IN LISTS arg_INCLUDE_DIRS)
		list(APPEND compiler_options -I${dir})
	endforeach()

	add_custom_target(${target})
	set_target_properties(${target} PROPERTIES FOLDER codegen)

	foreach(shader IN LISTS arg_SHADERS)
		# Input GLSL file
		set(shader_source ${arg_SOURCE_DIR}/${shader})
		# Output SPIR-V file
		set(shader_bin data/assets/shaders/${shader}.spv)
		# Output .d file to account for changed `#include`s
		set(shader_depfile shader_depfiles/${shader}.d)

		add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_bin}
				${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_depfile}
			MAIN_DEPENDENCY ${shader_source}
			DEPFILE ${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_depfile}
			COMMENT "Building SPIR-V for shader ${shader}"
			COMMAND ${GLSL_COMPILER} ${compiler_options}
				# Enable debug information generation for debuggable builds
				$<$<CONFIG:Debug>:-g>
				${shader_source} -o ${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_bin}
				-MD -MF ${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_depfile} -MT $<CONFIG>/${shader_bin}
		)

		target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_bin})
	endforeach()
endfunction()
