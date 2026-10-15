# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER tools/build)

voxen_add_executable(gen-opts tool_gen_opts.cpp)
target_link_libraries(gen-opts PRIVATE
	3rdparty::cxxopts
	3rdparty::json5cpp
)
