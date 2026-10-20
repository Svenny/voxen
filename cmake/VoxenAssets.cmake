include_guard(GLOBAL)

# Asset compilation rules. Outputs go to `<build>/<config>/data/assets/`.
# Assets are compiled with host tools from `tools/`, and they will rebuild
# when tools change.

# Adds font atlas compilation (see `tools/assetc`) to custom target `target`:
#     voxen_add_font_atlas(<target> SOURCE <font file> OUTPUT <path relative to data/assets>)
function(voxen_add_font_atlas target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "SOURCE;OUTPUT" "")

	set(output ${CMAKE_BINARY_DIR}/$<CONFIG>/data/assets/${arg_OUTPUT})

	add_custom_command(OUTPUT ${output}
		COMMAND assetc --type font-atlas --input ${arg_SOURCE} --output ${output}
		DEPENDS assetc ${arg_SOURCE}
		COMMENT "Compiling font atlas ${arg_OUTPUT}"
		VERBATIM
	)

	target_sources(${target} PRIVATE ${output})
endfunction()
