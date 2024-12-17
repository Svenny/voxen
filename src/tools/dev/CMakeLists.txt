# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER tools/dev)

voxen_add_executable(uidgen tool_uidgen.cpp)
target_link_libraries(uidgen PRIVATE voxen)

option(VOXEN_BUILD_FONTPACK_TOOL "Build FreeType client tool for generating and packing SDF fonts" OFF)
if(VOXEN_BUILD_FONTPACK_TOOL)
	find_package(Freetype)
	voxen_add_executable(fontpack tool_fontpack.cpp)
	target_link_libraries(fontpack PRIVATE
		extras
		voxen
		Freetype::Freetype
	)
endif()
