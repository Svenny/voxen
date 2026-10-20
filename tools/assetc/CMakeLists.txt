voxen_add_executable(assetc "")
target_sources(assetc PRIVATE
	font_atlas.cpp
	font_atlas.hpp
	main.cpp
)
target_link_libraries(assetc PRIVATE
	extras
	tk
	3rdparty::freetype
)
