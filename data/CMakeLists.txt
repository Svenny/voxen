add_custom_target(voxen-assets)
# XXX: it's not really a codegen target...
set_target_properties(voxen-assets PROPERTIES FOLDER codegen)

# Now there is no asset build/pack logic, just copy this one font atlas
set(FONT_IN ${CMAKE_SOURCE_DIR}/data/core-assets/fonts/SourceCodePro-Regular-ascii.png)
set(FONT_OUT "data/assets/fonts/ascii.png")

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/$<CONFIG>/${FONT_OUT}
	MAIN_DEPENDENCY ${FONT_IN}
	COMMENT "Copying debug font atlas file"
	COMMAND ${CMAKE_COMMAND} -E copy "${FONT_IN}" "${CMAKE_BINARY_DIR}/$<CONFIG>/${FONT_OUT}"
)

target_sources(voxen-assets PRIVATE ${CMAKE_BINARY_DIR}/$<CONFIG>/${FONT_OUT})

add_dependencies(voxen voxen-assets)
