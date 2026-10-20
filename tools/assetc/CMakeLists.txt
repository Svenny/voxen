voxen_add_executable(assetc assetc.cpp)
target_link_libraries(assetc PRIVATE
	extras
	tk
	3rdparty::freetype
)
