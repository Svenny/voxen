include_guard(GLOBAL)

# Slang to SPIR-V shader compilation rules

# Prebuilt `slangc` is vendored for supported hosts, see `toolchains/vendor.toml`.
# Set `VOXEN_SLANGC` to use another one (e.g. from Vulkan SDK) for local experiments.
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
	if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
		set(VOXEN_VENDORED_SLANG_PLATFORM linux-x86_64)
	elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
		set(VOXEN_VENDORED_SLANG_PLATFORM windows-x86_64)
	endif()
endif()

find_program(VOXEN_SLANGC slangc
	HINTS ${CMAKE_SOURCE_DIR}/toolchains/slang/${VOXEN_VENDORED_SLANG_PLATFORM}/bin
	DOC "Slang shader compiler"
	REQUIRED
)

# Without `git lfs pull` the vendored binaries are text pointer files
file(READ ${VOXEN_SLANGC} slangc_head LIMIT 64)
if(slangc_head MATCHES "^version https://git-lfs")
	message(FATAL_ERROR "${VOXEN_SLANGC} is a Git LFS pointer, run `git lfs pull`")
endif()

execute_process(COMMAND ${VOXEN_SLANGC} -v
	OUTPUT_VARIABLE slangc_version ERROR_VARIABLE slangc_version
	RESULT_VARIABLE slangc_result OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT slangc_result EQUAL 0)
	message(FATAL_ERROR "Can't run ${VOXEN_SLANGC}: ${slangc_result}")
endif()
message(STATUS "Slang compiler: ${VOXEN_SLANGC} (${slangc_version})")

# Creates custom target `target` compiling Slang shaders to SPIR-V:
#     voxen_add_shaders(<target> SOURCE_DIR <dir> SHADERS <shader>...)
#
# Each `<shader>` is `<pass>.<stage>` with `<stage>` being `vert`, `frag` or `comp`.
# It names the entry point `vertexMain`, `fragmentMain` or `computeMain` respectively
# of `<SOURCE_DIR>/<pass>.slang`, one pass file can have several entry points.
# The output for each is `<build>/<config>/data/assets/shaders/<shader>.spv`, where the game
# expects it, with the entry point named `main`. `SOURCE_DIR` is also the root for `import`s.
function(voxen_add_shaders target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "SOURCE_DIR" "SHADERS")

	set(compiler_options
		-target spirv
		# SPIR-V 1.6 is the maximum for Vulkan 1.3
		-profile spirv_1_6
		-std 2026
		-I ${arg_SOURCE_DIR}
		# Match `m` math library: row-major storage, `mul(matrix, column_vector)`
		-matrix-layout-row-major
		# The engine requires `scalarBlockLayout` feature
		-fvk-use-scalar-layout
		-O2
		-warnings-as-errors all
	)

	add_custom_target(${target})
	set_target_properties(${target} PROPERTIES FOLDER codegen)

	foreach(shader IN LISTS arg_SHADERS)
		if(NOT shader MATCHES "^(.+)\\.(vert|frag|comp)$")
			message(FATAL_ERROR "Shader '${shader}' is not `<pass>.<vert|frag|comp>`")
		endif()
		set(pass ${CMAKE_MATCH_1})
		set(entry_vert vertexMain)
		set(entry_frag fragmentMain)
		set(entry_comp computeMain)
		set(entry ${entry_${CMAKE_MATCH_2}})

		# Input Slang file
		set(shader_source ${arg_SOURCE_DIR}/${pass}.slang)
		# Output SPIR-V file
		set(shader_bin ${CMAKE_BINARY_DIR}/$<CONFIG>/data/assets/shaders/${shader}.spv)
		# Output .d file to account for changed `import`s and `#include`s
		set(shader_depfile ${CMAKE_BINARY_DIR}/$<CONFIG>/shader_depfiles/${shader}.d)

		add_custom_command(OUTPUT ${shader_bin} ${shader_depfile}
			MAIN_DEPENDENCY ${shader_source}
			DEPENDS ${VOXEN_SLANGC}
			DEPFILE ${shader_depfile}
			COMMENT "Building SPIR-V for shader ${shader}"
			# Validate the output with SPIRV-Tools bundled into slangc
			COMMAND ${CMAKE_COMMAND} -E env SLANG_RUN_SPIRV_VALIDATION=1
				${VOXEN_SLANGC} ${shader_source} -entry ${entry} ${compiler_options}
				# Enable debug information generation for debuggable builds
				$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-g>
				-o ${shader_bin} -depfile ${shader_depfile}
			VERBATIM
			# Drops `-g` genex evaluating to empty instead of passing ""
			COMMAND_EXPAND_LISTS
		)

		target_sources(${target} PRIVATE ${shader_bin})
	endforeach()
endfunction()

# Transitional GLSL rule for shaders not yet converted to Slang
function(voxen_add_glsl_shaders target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "SOURCE_DIR" "INCLUDE_DIRS;SHADERS")

	find_program(GLSL_COMPILER glslc REQUIRED)

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
				$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-g>
				${shader_source} -o ${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_bin}
				-MD -MF ${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_depfile} -MT $<CONFIG>/${shader_bin}
		)

		target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/$<CONFIG>/${shader_bin})
	endforeach()
endfunction()
