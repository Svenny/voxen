voxen_add_executable(assetc "")
target_sources(assetc PRIVATE
	assetc.cpp
	font_atlas.cpp
	font_atlas.hpp
)
target_link_libraries(assetc PRIVATE
	extras
	tk
	3rdparty::freetype
)
