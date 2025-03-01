# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER tools/dev)

voxen_add_executable(uidgen tool_uidgen.cpp)
target_link_libraries(uidgen PRIVATE voxen)

voxen_add_executable(fontpack tool_fontpack.cpp)
target_link_libraries(fontpack PRIVATE
	extras
	voxen
	3rdparty::freetype
)
