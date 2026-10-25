include(VoxenAssets)

add_custom_target(voxen-assets ALL)

voxen_add_font_atlas(voxen-assets
	SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/core-assets/fonts/SourceCodePro-Regular.otf
	OUTPUT fonts/ascii.png
)

# The engine itself builds without assets, only the game needs them to run
add_dependencies(game voxen-assets)
