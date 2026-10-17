# Make all targets created here appear in a folder in VS
set(CMAKE_FOLDER tools/research)

voxen_add_executable(demo-coroutines demo_coroutines.cpp)
target_link_libraries(demo-coroutines PRIVATE voxen tk)
